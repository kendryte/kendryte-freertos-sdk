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
#include <aes.h>
#include <driver.h>
#include <hal.h>
#include <io.h>
#include <plic.h>
#include <semphr.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysctl.h>

#define AES_USE_DMA 1
#define AES_BASE_ADDR (0x50450000)
#define COMMON_ENTRY                                                      \
    aes_dev_data* data = (aes_dev_data*)userdata;                         \
    volatile aes_t* aes = (volatile aes_t*)data->base_addr; \
    (void)aes;

typedef struct
{
    uintptr_t base_addr;
    SemaphoreHandle_t free_mutex;

    struct
    {
        uint32_t aes_hardware_tag[4];
    };
} aes_dev_data;

static void aes_install(void* userdata)
{
    COMMON_ENTRY;
    sysctl_clock_enable(SYSCTL_CLOCK_AES);
    sysctl_reset(SYSCTL_RESET_AES);
    data->free_mutex = xSemaphoreCreateMutex();
}

static int aes_open(void* userdata)
{
    return 1;
}

static void aes_close(void* userdata)
{
}

static void entry_exclusive(aes_dev_data* data)
{
    configASSERT(xSemaphoreTake(data->free_mutex, portMAX_DELAY) == pdTRUE);
}

static void exit_exclusive(aes_dev_data* data)
{
    xSemaphoreGive(data->free_mutex);
}

static int os_aes_write_aad(uint32_t aad_data, void* userdata)
{
    COMMON_ENTRY;
    aes->aes_aad_data = aad_data;
    return 0;
}

static int os_aes_write_text(uint32_t text_data, void* userdata)
{
    COMMON_ENTRY;
    aes->aes_text_data = text_data;
    return 0;
}

static int os_aes_write_tag(uint32_t* tag, void* userdata)
{
    COMMON_ENTRY;
    aes->gcm_in_tag[0] = tag[3];
    aes->gcm_in_tag[1] = tag[2];
    aes->gcm_in_tag[2] = tag[1];
    aes->gcm_in_tag[3] = tag[0];
    return 0;
}

static int os_get_data_in_flag(void* userdata)
{
    COMMON_ENTRY;
    /* data can in flag 1: data ready 0: data not ready */
    return aes->data_in_flag;
}

static int os_get_data_out_flag(void* userdata)
{
    COMMON_ENTRY;
    /* data can output flag 1: data ready 0: data not ready */
    return aes->data_out_flag;
}

static int os_get_tag_in_flag(void* userdata)
{
    COMMON_ENTRY;
    /* data can output flag 1: data ready 0: data not ready */
    return aes->tag_in_flag;
}

static uint32_t os_read_out_data(void* userdata)
{
    COMMON_ENTRY;
    return aes->aes_out_data;
}

static int os_aes_check_tag(void* userdata)
{
    COMMON_ENTRY;
    return aes->tag_chk;
}

static int os_aes_get_tag(uint8_t* l_tag, void* userdata)
{
    COMMON_ENTRY;
    uint32_t u32tag;
    uint8_t i = 0;

    u32tag = aes->gcm_out_tag[3];
    l_tag[i++] = (uint8_t)((u32tag >> 24) & 0xff);
    l_tag[i++] = (uint8_t)((u32tag >> 16) & 0xff);
    l_tag[i++] = (uint8_t)((u32tag >> 8) & 0xff);
    l_tag[i++] = (uint8_t)((u32tag)&0xff);

    u32tag = aes->gcm_out_tag[2];
    l_tag[i++] = (uint8_t)((u32tag >> 24) & 0xff);
    l_tag[i++] = (uint8_t)((u32tag >> 16) & 0xff);
    l_tag[i++] = (uint8_t)((u32tag >> 8) & 0xff);
    l_tag[i++] = (uint8_t)((u32tag)&0xff);

    u32tag = aes->gcm_out_tag[1];
    l_tag[i++] = (uint8_t)((u32tag >> 24) & 0xff);
    l_tag[i++] = (uint8_t)((u32tag >> 16) & 0xff);
    l_tag[i++] = (uint8_t)((u32tag >> 8) & 0xff);
    l_tag[i++] = (uint8_t)((u32tag)&0xff);

    u32tag = aes->gcm_out_tag[0];
    l_tag[i++] = (uint8_t)((u32tag >> 24) & 0xff);
    l_tag[i++] = (uint8_t)((u32tag >> 16) & 0xff);
    l_tag[i++] = (uint8_t)((u32tag >> 8) & 0xff);
    l_tag[i++] = (uint8_t)((u32tag)&0xff);
    return 1;
}

static int os_aes_clear_chk_tag(void* userdata)
{
    COMMON_ENTRY;
    aes->tag_clear = 0;
    return 0;
}

static int os_check_tag(uint32_t* aes_gcm_tag, void* userdata)
{
    while (!os_get_tag_in_flag(userdata))
        ;
    os_aes_write_tag(aes_gcm_tag, userdata);
    while (!os_aes_check_tag(userdata))
        ;
    if (os_aes_check_tag(userdata) == 2)
    {
        os_aes_clear_chk_tag(userdata);
        return 1;
    }
    else
    {
        os_aes_clear_chk_tag(userdata);
        return 0;
    }
}

static int os_aes_init(uint8_t* key_addr, uint8_t key_length, uint8_t* aes_iv,
    uint8_t iv_length, uint8_t* aes_aad, aes_cipher_mod cipher_mod, aes_encrypt_sel encrypt_sel,
    uint32_t add_size, uint32_t data_size, void* userdata)
{
    COMMON_ENTRY;

    int i, remainder, num, cnt;
    uint32_t u32data;
    uint8_t u8data[4] = {0};

    if ((cipher_mod == AES_CIPHER_ECB) || (cipher_mod == AES_CIPHER_CBC))
        data_size = ((data_size + 15) / 16) * 16;
    aes->aes_endian |= 1;

    /* write key Low byte alignment */
    num = key_length / 4;
    for (i = 0; i < num; i++)
    {
        if (i < 4)
            aes->aes_key[i] = *((uint32_t*)(&key_addr[key_length - (4 * i) - 4]));
        else
            aes->aes_key_ext[i - 4] = *((uint32_t*)(&key_addr[key_length - (4 * i) - 4]));
    }
    remainder = key_length % 4;
    if (remainder)
    {
        switch (remainder)
        {
        case 1:
            u8data[0] = key_addr[0];
            break;
        case 2:
            u8data[0] = key_addr[0];
            u8data[1] = key_addr[1];
            break;
        case 3:
            u8data[0] = key_addr[0];
            u8data[1] = key_addr[1];
            u8data[2] = key_addr[2];
            break;
        default:
            break;
        }
        if (num < 4)
            aes->aes_key[num] = *((uint32_t*)(&u8data[0]));
        else
            aes->aes_key_ext[num - 4] = *((uint32_t*)(&u8data[0]));
    }

    /* write iv Low byte alignment */
    num = iv_length / 4;
    for (i = 0; i < num; i++)
        aes->aes_iv[i] = *((uint32_t*)(&aes_iv[iv_length - (4 * i) - 4]));
    remainder = iv_length % 4;
    if (remainder)
    {
        switch (remainder)
        {
        case 1:
            u8data[0] = aes_iv[0];
            break;
        case 2:
            u8data[0] = aes_iv[0];
            u8data[1] = aes_iv[1];
            break;
        case 3:
            u8data[0] = aes_iv[0];
            u8data[1] = aes_iv[1];
            u8data[2] = aes_iv[2];
            break;
        default:
            break;
        }
        aes->aes_iv[num] = *((uint32_t*)(&u8data[0]));
    }
    aes->mode_ctl.kmode = key_length / 8 - 2; /* 00:AES_128 01:AES_192 10:AES_256 11:RESERVED */
    aes->mode_ctl.cipher_mode = cipher_mod;

    /*
     * [1:0],set the first bit and second bit 00:ecb; 01:cbc;
     * 10,11：AES_CIPHER_GCM
     */
    aes->encrypt_sel = encrypt_sel;
    aes->gb_aad_end_adr = add_size - 1;
    aes->gb_pc_end_adr = data_size - 1;
    aes->gb_aes_en |= 1;

    /* write aad */
    if (cipher_mod == AES_CIPHER_GCM)
    {
        num = add_size / 4;
        for (i = 0; i < num; i++)
        {
            u32data = *((uint32_t*)(aes_aad + i * 4));
            while (!os_get_data_in_flag(userdata))
                ;
            os_aes_write_aad(u32data, userdata);
        }
        cnt = 4 * num;
        remainder = add_size % 4;
        if (remainder)
        {
            switch (remainder)
            {
            case 1:
                u8data[0] = aes_aad[cnt];
                break;
            case 2:
                u8data[0] = aes_aad[cnt];
                u8data[1] = aes_aad[cnt + 1];
                break;
            case 3:
                u8data[0] = aes_aad[cnt];
                u8data[1] = aes_aad[cnt + 1];
                u8data[2] = aes_aad[cnt + 2];
                break;
            default:
                return 0;
            }
            u32data = *((uint32_t*)(&u8data[0]));
            while (!os_get_data_in_flag(userdata))
                ;
            os_aes_write_aad(u32data, userdata);
        }
    }

    return 1;
}

static int aes_process_less_80_bytes(uint8_t* aes_in_data,
    uint8_t* aes_out_data,
    uint32_t data_size,
    aes_cipher_mod cipher_mod,
    void* userdata)
{
    int padding_size;
    int num, i, remainder, cnt;
    uint32_t u32data;
    uint8_t u8data[4] = {0};

    /* Fill 128 bits	(16byte) */
    padding_size = ((data_size + 15) / 16) * 16;

    /* write text */
    num = data_size / 4;
    for (i = 0; i < num; i++)
    {
        u32data = *((uint32_t*)(&aes_in_data[i * 4]));
        while (!os_get_data_in_flag(userdata))
            ;
        os_aes_write_text(u32data, userdata);
    }
    cnt = 4 * num;
    remainder = data_size % 4;
    if (remainder)
    {
        switch (remainder)
        {
        case 1:
            u8data[0] = aes_in_data[cnt];
            break;
        case 2:
            u8data[0] = aes_in_data[cnt];
            u8data[1] = aes_in_data[cnt + 1];
            break;
        case 3:
            u8data[0] = aes_in_data[cnt];
            u8data[1] = aes_in_data[cnt + 1];
            u8data[2] = aes_in_data[cnt + 2];
            break;
        default:
            return 0;
        }
        u32data = *((uint32_t*)(&u8data[0]));
        while (!os_get_data_in_flag(userdata))
            ;
        os_aes_write_text(u32data, userdata);
    }

    if ((cipher_mod == AES_CIPHER_ECB) || (cipher_mod == AES_CIPHER_CBC))
    {
        /* use 0 to Fill 128 bits */
        num = (padding_size - data_size) / 4;
        for (i = 0; i < num; i++)
        {
            while (!os_get_data_in_flag(userdata))
                ;
            os_aes_write_text(0, userdata);
        }

        /* get data */
        num = padding_size / 4;
    }

    /* get data */
    for (i = 0; i < num; i++)
    {
        while (!os_get_data_out_flag(userdata))
            ;
        *((uint32_t*)(&aes_out_data[i * 4])) = os_read_out_data(userdata);
    }

    if ((cipher_mod == AES_CIPHER_GCM) && (remainder))
    {
        while (!os_get_data_out_flag(userdata))
            ;

        *((uint32_t*)(&u8data[0])) = os_read_out_data(userdata);
        switch (remainder)
        {
        case 1:
            aes_out_data[num * 4] = u8data[0];
            break;
        case 2:
            aes_out_data[num * 4] = u8data[0];
            aes_out_data[(i * 4) + 1] = u8data[1];
            break;
        case 3:
            aes_out_data[num * 4] = u8data[0];
            aes_out_data[(i * 4) + 1] = u8data[1];
            aes_out_data[(i * 4) + 2] = u8data[2];
            break;
        default:
            return 0;
        }
    }

    return 1;
}

static int os_aes_process(uint8_t* aes_in_data,
    uint8_t* aes_out_data,
    uint32_t data_size,
    aes_cipher_mod cipher_mod,
    void* userdata)
{

    uint32_t i, temp_size;

    i = 0;
    if (data_size >= 80)
    {
        for (i = 0; i < (data_size / 80); i++)
            aes_process_less_80_bytes(&aes_in_data[i * 80], &aes_out_data[i * 80], 80, cipher_mod, userdata);
    }
    temp_size = data_size % 80;
    if (temp_size)
        aes_process_less_80_bytes(&aes_in_data[i * 80], &aes_out_data[i * 80], temp_size, cipher_mod, userdata);
    return 1;
}

static void aes_decrypt(aes_parameter* aes_param, void* userdata)
{
    COMMON_ENTRY;
    configASSERT(aes_param->key_length == AES_128 || aes_param->key_length == AES_192 || aes_param->key_length == AES_256);

    entry_exclusive(data);
    os_aes_init(aes_param->key_addr, aes_param->key_length, aes_param->gcm_iv, aes_param->iv_length, aes_param->aes_aad,
        aes_param->cipher_mod, AES_MODE_DECRYPTION, aes_param->add_size, aes_param->data_size, userdata);

    int padding_size = aes_param->data_size;
    if (aes_param->cipher_mod == AES_CIPHER_CBC || aes_param->cipher_mod == AES_CIPHER_ECB)
    {
        padding_size = ((padding_size + 15) / 16) * 16;
    }

#if (AES_USE_DMA == 1)
    uint8_t* padding_buffer = NULL;
    padding_buffer = (uint8_t*)malloc(padding_size * sizeof(uint8_t));
    memset(padding_buffer, 0, padding_size);
    memcpy(padding_buffer, aes_param->aes_in_data, aes_param->data_size);

    uintptr_t aes_write = dma_open_free();
    uintptr_t aes_read = dma_open_free();

    dma_set_request_source(aes_read, SYSCTL_DMA_SELECT_AES_REQ);

    SemaphoreHandle_t event_read = xSemaphoreCreateBinary(), event_write = xSemaphoreCreateBinary();

    dma_transmit_async(aes_read, &aes->aes_out_data, aes_param->aes_out_data, 0, 1, sizeof(uint32_t), padding_size >> 2, 4, event_read);
    dma_transmit_async(aes_write, aes_param->aes_in_data, &aes->aes_text_data, 1, 0, sizeof(uint32_t), padding_size >> 2, 4, event_write);
    aes->dma_sel = 1;

    configASSERT(xSemaphoreTake(event_read, portMAX_DELAY) == pdTRUE && xSemaphoreTake(event_write, portMAX_DELAY) == pdTRUE);

    dma_close(aes_write);
    dma_close(aes_read);
    vSemaphoreDelete(event_read);
    vSemaphoreDelete(event_write);
#else
    os_aes_process(aes_param->aes_in_data, aes_param->aes_out_data, padding_size, aes_param->cipher_mod, userdata);
#endif
    if (aes_param->cipher_mod == AES_CIPHER_GCM)
    {
        os_aes_get_tag(aes_param->tag, userdata);
        os_check_tag(data->aes_hardware_tag, userdata); /* haredware need this */
    }
#if (AES_USE_DMA == 1)
    free(padding_buffer);
#endif
    exit_exclusive(data);
}

static void aes_encrypt(aes_parameter* aes_param, void* userdata)
{
    COMMON_ENTRY;
    configASSERT(aes_param->key_length == AES_128 || aes_param->key_length == AES_192 || aes_param->key_length == AES_256);

    entry_exclusive(data);
    os_aes_init(aes_param->key_addr, aes_param->key_length, aes_param->gcm_iv, aes_param->iv_length, aes_param->aes_aad,
        aes_param->cipher_mod, AES_MODE_ENCRYPTION, aes_param->add_size, aes_param->data_size, userdata);

    int padding_size = aes_param->data_size;
#if (AES_USE_DMA == 1)
    uint8_t* padding_buffer = NULL;
    if (aes_param->cipher_mod == AES_CIPHER_CBC || aes_param->cipher_mod == AES_CIPHER_ECB)
    {
        padding_size = ((padding_size + 15) / 16) * 16;
    }
    padding_buffer = (uint8_t*)malloc(padding_size * sizeof(uint8_t));
    memset(padding_buffer, 0, padding_size);
    memcpy(padding_buffer, aes_param->aes_in_data, aes_param->data_size);

    uintptr_t aes_read = dma_open_free();
    uintptr_t aes_write = dma_open_free();

    dma_set_request_source(aes_read, SYSCTL_DMA_SELECT_AES_REQ);

    SemaphoreHandle_t event_read = xSemaphoreCreateBinary(), event_write = xSemaphoreCreateBinary();
    aes->dma_sel = 1;
    dma_transmit_async(aes_read, &aes->aes_out_data, aes_param->aes_out_data, 0, 1, sizeof(uint32_t), padding_size >> 2, 4, event_read);
    dma_transmit_async(aes_write, padding_buffer, &aes->aes_text_data, 1, 0, sizeof(uint32_t), padding_size >> 2, 4, event_write);

    configASSERT(xSemaphoreTake(event_read, portMAX_DELAY) == pdTRUE && xSemaphoreTake(event_write, portMAX_DELAY) == pdTRUE);

    dma_close(aes_write);
    dma_close(aes_read);
    vSemaphoreDelete(event_read);
    vSemaphoreDelete(event_write);
#else
    os_aes_process(aes_param->aes_in_data, aes_param->aes_out_data, aes_param->data_size, aes_param->cipher_mod, userdata);
#endif
    if (aes_param->cipher_mod == AES_CIPHER_GCM)
    {
        os_aes_get_tag(aes_param->tag, userdata);
        os_check_tag(data->aes_hardware_tag, userdata); /* haredware need this */
    }
#if (AES_USE_DMA == 1)
    free(padding_buffer);
#endif
    exit_exclusive(data);
}

static aes_dev_data dev0_data = {AES_BASE_ADDR, 0, {{0}}};

const aes_driver_t g_aes_driver_aes0 = {{&dev0_data, aes_install, aes_open, aes_close}, aes_decrypt, aes_encrypt};
