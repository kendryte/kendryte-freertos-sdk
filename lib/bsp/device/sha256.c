/* Copyright 2018 Canaan Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <FreeRTOS.h>
#include <dmac.h>
#include <driver.h>
#include <hal.h>
#include <io.h>
#include <plic.h>
#include <semphr.h>
#include <sha256.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysctl.h>

#define ROTL(x, n) (((x) << (n)) | ((x) >> (32 - (n))))
#define ROTR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
#define BYTESWAP(x) ((ROTR((x), 8) & 0xff00ff00L) | (ROTL((x), 8) & 0x00ff00ffL))
#define BYTESWAP64(x) byteswap64(x)
#define COMMON_ENTRY                                                               \
    sha256_dev_data* data = (sha256_dev_data*)userdata;                            \
    volatile struct sha256_t* sha256 = (volatile struct sha256_t*)data->base_addr; \
    (void)sha256;

typedef struct
{
    uintptr_t base_addr;
    SemaphoreHandle_t free_mutex;
} sha256_dev_data;

typedef struct
{
    uint64_t total_length;
    uint32_t hash[SHA256_HASH_WORDS];
    uint32_t dma_buf_length;
    uint32_t* dma_buf;
    uint32_t buffer_length;
    union
    {
        uint32_t words[16];
        uint8_t bytes[64];
    } buffer;
} sha256_context;

static const uint8_t padding[64] =
{
    0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00
};

static inline uint64_t byteswap64(uint64_t x)
{
    uint32_t a = x >> 32;
    uint32_t b = (uint32_t)x;

    return ((uint64_t)BYTESWAP(b) << 32) | (uint64_t)BYTESWAP(a);
}

static void sha256_install(void* userdata)
{
    COMMON_ENTRY;
    sysctl_clock_enable(SYSCTL_CLOCK_SHA);
    sysctl_reset(SYSCTL_RESET_SHA);
    data->free_mutex = xSemaphoreCreateMutex();
}

static int sha256_open(void* userdata)
{
    return 1;
}

static void sha256_close(void* userdata)
{
}

static void entry_exclusive(sha256_dev_data* data)
{
    configASSERT(xSemaphoreTake(data->free_mutex, portMAX_DELAY) == pdTRUE);
}

static void exit_exclusive(sha256_dev_data* data)
{
    xSemaphoreGive(data->free_mutex);
}

static void sha256_update_buf(sha256_context* sc, const void* vdata, uint32_t len)
{
    const uint8_t* data = vdata;
    uint32_t buffer_bytes_left;
    uint32_t bytes_to_copy;
    uint32_t i;

    while (len)
    {
        buffer_bytes_left = 64L - sc->buffer_length;

        bytes_to_copy = buffer_bytes_left;
        if (bytes_to_copy > len)
            bytes_to_copy = len;

        memcpy(&sc->buffer.bytes[sc->buffer_length], data, bytes_to_copy);

        sc->total_length += bytes_to_copy * 8L;

        sc->buffer_length += bytes_to_copy;
        data += bytes_to_copy;
        len -= bytes_to_copy;

        if (sc->buffer_length == 64L)
        {
            for (i = 0; i < 16; i++)
            {
                sc->dma_buf[sc->dma_buf_length++] = sc->buffer.words[i];
            }
            sc->buffer_length = 0L;
        }
    }
}

void sha256_final_buf(sha256_context* sc)
{
    uint32_t bytes_to_pad;
    uint64_t length_pad;

    bytes_to_pad = 120L - sc->buffer_length;

    if (bytes_to_pad > 64L)
        bytes_to_pad -= 64L;
    length_pad = BYTESWAP64(sc->total_length);
    sha256_update_buf(sc, padding, bytes_to_pad);
    sha256_update_buf(sc, &length_pad, 8L);
}

static void sha256_str(const char* str, size_t length, uint8_t* hash, void* userdata)
{
    COMMON_ENTRY;
    entry_exclusive(data);
    int i = 0;
    sha256_context sha;
    sysctl_clock_enable(SYSCTL_CLOCK_SHA);
    sysctl_reset(SYSCTL_RESET_SHA);

    sha256->reserved0 = 0;
    sha256->sha_status |= 1 << 16; /*!< 0 for little endian, 1 for big endian */
    sha256->sha_status |= 1; /*!< enable sha256 */
    sha256->sha_data_num = (length + 64 + 8) / 64;
    sha.dma_buf = (uint32_t*)malloc((length + 64 + 8) / 64 * 16 * sizeof(uint32_t));
    sha.buffer_length = 0L;
    sha.dma_buf_length = 0L;
    sha.total_length = 0LL;
    for (i = 0; i < (sizeof(sha.dma_buf) / 4); i++)
        sha.dma_buf[i] = 0;
    sha256_update_buf(&sha, str, length);
    sha256_final_buf(&sha);

    uintptr_t dma_write = dma_open_free();

    dma_set_select_request(dma_write, SYSCTL_DMA_SELECT_SHA_RX_REQ);

    SemaphoreHandle_t event_write = xSemaphoreCreateBinary();

    dma_transmit_async(dma_write, sha.dma_buf, &sha256->sha_data_in1, 1, 0, sizeof(uint32_t), sha.dma_buf_length, 16, event_write);
    sha256->sha_input_ctrl |= 1; /*!< dma enable */
    configASSERT(xSemaphoreTake(event_write, portMAX_DELAY) == pdTRUE);

    while (!(sha256->sha_status & 0x01))
        ;
    for (i = 0; i < SHA256_HASH_WORDS; i++)
        *((uint32_t*)&hash[i * 4]) = sha256->sha_result[SHA256_HASH_WORDS - i - 1];
    free(sha.dma_buf);
    dma_close(dma_write);
    vSemaphoreDelete(event_write);

    exit_exclusive(data);
}

static sha256_dev_data dev0_data = {SHA256_BASE_ADDR, 0};

const sha256_driver_t g_sha_driver_sha256 = {{&dev0_data, sha256_install, sha256_open, sha256_close}, sha256_str};
