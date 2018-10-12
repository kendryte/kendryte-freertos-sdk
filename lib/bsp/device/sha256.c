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
#include <stdlib.h>
#include <string.h>
#include <FreeRTOS.h>
#include <dmac.h>
#include <driver.h>
#include <hal.h>
#include <sha256.h>
#include <sysctl.h>

#define ROTL(x, n) (((x) << (n)) | ((x) >> (32 - (n))))
#define ROTR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
#define BYTESWAP(x) ((ROTR((x), 8) & 0xff00ff00L) | (ROTL((x), 8) & 0x00ff00ffL))
#define BYTESWAP64(x) byteswap64(x)
#define COMMON_ENTRY                                                               \
    sha256_dev_data *data = (sha256_dev_data*)userdata;                            \
    volatile sha256_t *sha256 = (volatile sha256_t *)data->base_addr; \
    (void)sha256;

typedef struct
{
    sysctl_clock_t clock;
    uintptr_t base_addr;
    SemaphoreHandle_t free_mutex;
} sha256_dev_data;

static const uint8_t padding[64] =
{
    0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static inline uint64_t byteswap64(uint64_t x)
{
    uint32_t a = (uint32_t)(x >> 32);
    uint32_t b = (uint32_t)x;
    return ((uint64_t)BYTESWAP(b) << 32) | (uint64_t)BYTESWAP(a);
}

static void sha256_install(void *userdata)
{
    COMMON_ENTRY;
    sysctl_clock_enable(data->clock);
    data->free_mutex = xSemaphoreCreateMutex();
}

static int sha256_open(void *userdata)
{
    return 1;
}

static void sha256_close(void *userdata)
{
}

static void entry_exclusive(sha256_dev_data *data)
{
    configASSERT(xSemaphoreTake(data->free_mutex, portMAX_DELAY) == pdTRUE);
}

static void exit_exclusive(sha256_dev_data *data)
{
    xSemaphoreGive(data->free_mutex);
}

static void sha256_update_buf(sha256_context_t *context, const void *input, size_t input_len)
{
    const uint8_t *data = input;
    size_t buffer_bytes_left;
    size_t bytes_to_copy;
    uint32_t i;

    while (input_len)
    {
        buffer_bytes_left = SHA256_BLOCK_LEN - context->buffer_len;
        bytes_to_copy = buffer_bytes_left;
        if (bytes_to_copy > input_len)
            bytes_to_copy = input_len;
        memcpy(&context->buffer.bytes[context->buffer_len], data, bytes_to_copy);
        context->total_len += bytes_to_copy * 8L;
        context->buffer_len += bytes_to_copy;
        data += bytes_to_copy;
        input_len -= bytes_to_copy;
        if (context->buffer_len == SHA256_BLOCK_LEN)
        {
            for (i = 0; i < 16; i++)
            {
                context->dma_buf[context->dma_buf_len++] = context->buffer.words[i];
            }
            context->buffer_len = 0L;
        }
    }
}

static void sha256_final_buf(sha256_context_t *context)
{
    size_t bytes_to_pad;
    size_t length_pad;

    bytes_to_pad = 120L - context->buffer_len;
    if (bytes_to_pad > 64L)
        bytes_to_pad -= 64L;
    length_pad = BYTESWAP64(context->total_len);
    sha256_update_buf(context, padding, bytes_to_pad);
    sha256_update_buf(context, &length_pad, 8L);
}

static void sha256_hard_calculate(const uint8_t *input, size_t input_len, uint8_t *output, void *userdata)
{
    COMMON_ENTRY;
    entry_exclusive(data);
    uint32_t i = 0;
    sha256_context_t context;
    sysctl_clock_enable(data->clock);
    sha256->sha_function_reg_0.sha_endian = SHA256_BIG_ENDIAN;
    sha256->sha_function_reg_0.sha_en = ENABLE_SHA;
    sha256->sha_num_reg.sha_data_cnt = (input_len + SHA256_BLOCK_LEN + 8) / SHA256_BLOCK_LEN;
    context.dma_buf = (uint32_t*)malloc((input_len + SHA256_BLOCK_LEN + 8) / SHA256_BLOCK_LEN * 16 * sizeof(uint32_t));
    context.buffer_len = 0L;
    context.dma_buf_len = 0L;
    context.total_len = 0L;
    for (i = 0; i < (sizeof(context.dma_buf) / 4); i++)
        context.dma_buf[i] = 0;
    sha256_update_buf(&context, input, input_len);
    sha256_final_buf(&context);

    uintptr_t dma_write = dma_open_free();

    dma_set_request_source(dma_write, SYSCTL_DMA_SELECT_SHA_RX_REQ);

    SemaphoreHandle_t event_write = xSemaphoreCreateBinary();

    dma_transmit_async(dma_write, context.dma_buf, &sha256->sha_data_in1, 1, 0, sizeof(uint32_t), context.dma_buf_len, 16, event_write);
    sha256->sha_function_reg_1.dma_en = 0x1;
    configASSERT(xSemaphoreTake(event_write, portMAX_DELAY) == pdTRUE);

    while (!(sha256->sha_function_reg_0.sha_en))
        ;
    for (i = 0; i < SHA256_HASH_WORDS; i++)
        *((uint32_t*)&output[i * 4]) = sha256->sha_result[SHA256_HASH_WORDS - i - 1];
    free(context.dma_buf);
    dma_close(dma_write);
    vSemaphoreDelete(event_write);

    exit_exclusive(data);
}

static sha256_dev_data dev0_data = {SYSCTL_CLOCK_SHA, SHA256_BASE_ADDR, 0};

const sha256_driver_t g_sha_driver_sha256 = {{&dev0_data, sha256_install, sha256_open, sha256_close}, sha256_hard_calculate};
