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
#include <string.h>
#include <stdlib.h>
#include <FreeRTOS.h>
#include <aes.h>
#include <driver.h>
#include <sysctl.h>
#include <hal.h>

#define COMMON_ENTRY                                               \
    aes_dev_data *data = (aes_dev_data *)userdata;                 \
    volatile aes_t *aes = (volatile aes_t *)data->base_addr;       \
    (void)aes;

typedef struct
{
    sysctl_clock_t clock;
    uintptr_t base_addr;
    sysctl_dma_select_t dma_req_base;
    SemaphoreHandle_t free_mutex;

    struct
    {
        uint32_t aes_hardware_tag[4];
    };
} aes_dev_data;

static void aes_install(void *userdata)
{
    COMMON_ENTRY;
    sysctl_clock_enable(data->clock);
    data->free_mutex = xSemaphoreCreateMutex();
}

static int aes_open(void *userdata)
{
    return 1;
}

static void aes_close(void *userdata)
{
}

static void entry_exclusive(aes_dev_data *data)
{
    configASSERT(xSemaphoreTake(data->free_mutex, portMAX_DELAY) == pdTRUE);
}

static void exit_exclusive(aes_dev_data *data)
{
    xSemaphoreGive(data->free_mutex);
}

static void os_aes_write_aad(uint32_t aad_data, void *userdata)
{
    COMMON_ENTRY;
    aes->aes_aad_data = aad_data;
}

static void os_aes_write_text(uint32_t text_data, void *userdata)
{
    COMMON_ENTRY;
    aes->aes_text_data = text_data;
}

static void os_gcm_write_tag(uint32_t *tag, void *userdata)
{
    COMMON_ENTRY;
    aes->gcm_in_tag[0] = tag[3];
    aes->gcm_in_tag[1] = tag[2];
    aes->gcm_in_tag[2] = tag[1];
    aes->gcm_in_tag[3] = tag[0];
}

static uint32_t os_aes_get_data_in_flag(void *userdata)
{
    COMMON_ENTRY;
    return aes->data_in_flag;
}

static uint32_t os_aes_get_data_out_flag(void *userdata)
{
    COMMON_ENTRY;
    return aes->data_out_flag;
}

static uint32_t os_gcm_get_tag_in_flag(void *userdata)
{
    COMMON_ENTRY;
    return aes->tag_in_flag;
}

static uint32_t os_aes_read_out_data(void *userdata)
{
    COMMON_ENTRY;
    return aes->aes_out_data;
}

static uint32_t os_gcm_get_tag_chk(void *userdata)
{
    COMMON_ENTRY;
    return aes->tag_chk;
}

static void os_gcm_clear_chk_tag(void *userdata)
{
    COMMON_ENTRY;
    aes->tag_clear = 0;
}

static int os_gcm_check_tag(uint32_t *gcm_tag, void *userdata)
{
    while (!os_gcm_get_tag_in_flag(userdata))
        ;
    os_gcm_write_tag(gcm_tag, userdata);
    while (!os_gcm_get_tag_chk(userdata))
        ;
    if (os_gcm_get_tag_chk(userdata) == 0x2)
    {
        os_gcm_clear_chk_tag(userdata);
        return 1;
    }
    else
    {
        os_gcm_clear_chk_tag(userdata);
        return 0;
    }
}

static void os_gcm_get_tag(uint8_t *gcm_tag, void *userdata)
{
    COMMON_ENTRY;
    uint32_t uint32_tag;
    uint8_t i = 0;

    uint32_tag = aes->gcm_out_tag[3];
    gcm_tag[i++] = (uint8_t)((uint32_tag >> 24) & 0xff);
    gcm_tag[i++] = (uint8_t)((uint32_tag >> 16) & 0xff);
    gcm_tag[i++] = (uint8_t)((uint32_tag >> 8) & 0xff);
    gcm_tag[i++] = (uint8_t)((uint32_tag)&0xff);

    uint32_tag = aes->gcm_out_tag[2];
    gcm_tag[i++] = (uint8_t)((uint32_tag >> 24) & 0xff);
    gcm_tag[i++] = (uint8_t)((uint32_tag >> 16) & 0xff);
    gcm_tag[i++] = (uint8_t)((uint32_tag >> 8) & 0xff);
    gcm_tag[i++] = (uint8_t)((uint32_tag)&0xff);

    uint32_tag = aes->gcm_out_tag[1];
    gcm_tag[i++] = (uint8_t)((uint32_tag >> 24) & 0xff);
    gcm_tag[i++] = (uint8_t)((uint32_tag >> 16) & 0xff);
    gcm_tag[i++] = (uint8_t)((uint32_tag >> 8) & 0xff);
    gcm_tag[i++] = (uint8_t)((uint32_tag)&0xff);

    uint32_tag = aes->gcm_out_tag[0];
    gcm_tag[i++] = (uint8_t)((uint32_tag >> 24) & 0xff);
    gcm_tag[i++] = (uint8_t)((uint32_tag >> 16) & 0xff);
    gcm_tag[i++] = (uint8_t)((uint32_tag >> 8) & 0xff);
    gcm_tag[i++] = (uint8_t)((uint32_tag)&0xff);

    os_gcm_check_tag((uint32_t *)gcm_tag, userdata);
}

static void os_aes_init(const uint8_t *input_key,
    size_t input_key_len,
    uint8_t *iv,
    size_t iv_len,
    uint8_t *gcm_aad,
    aes_cipher_mode_t cipher_mode,
    aes_encrypt_sel_t encrypt_sel,
    size_t gcm_aad_len,
    size_t input_data_len,
    void *userdata)
{
    COMMON_ENTRY;
    size_t remainder, uint32_num, uint8_num, i;
    uint32_t uint32_data;
    uint8_t uint8_data[4] = {0};
    size_t padding_len = input_data_len;
    if ((cipher_mode == AES_ECB) || (cipher_mode == AES_CBC))
        padding_len = ((input_data_len + 15) / 16) * 16;
    aes->aes_endian |= 1;
    uint32_num = input_key_len / 4;
    for (i = 0; i < uint32_num; i++)
    {
        if (i < 4)
            aes->aes_key[i] = *((uint32_t *)(&input_key[input_key_len - (4 * i) - 4]));
        else
            aes->aes_key_ext[i - 4] = *((uint32_t *)(&input_key[input_key_len - (4 * i) - 4]));
    }

    uint32_num = iv_len / 4;
    for (i = 0; i < uint32_num; i++)
        aes->aes_iv[i] = *((uint32_t *)(&iv[iv_len - (4 * i) - 4]));

    aes->mode_ctl.kmode = input_key_len / 8 - 2; /* b'00:AES_128 b'01:AES_192 b'10:AES_256 b'11:RESERVED */
    aes->mode_ctl.cipher_mode = cipher_mode;
    aes->encrypt_sel = encrypt_sel;
    aes->gb_aad_num = gcm_aad_len - 1;
    aes->gb_pc_num = padding_len - 1;
    aes->gb_aes_en |= 1;

    if (cipher_mode == AES_GCM)
    {
        uint32_num = gcm_aad_len / 4;
        for (i = 0; i < uint32_num; i++)
        {
            uint32_data = *((uint32_t *)(&gcm_aad[i * 4]));
            while (!os_aes_get_data_in_flag(userdata))
                ;
            os_aes_write_aad(uint32_data, userdata);
        }
        uint8_num = 4 * uint32_num;
        remainder = gcm_aad_len % 4;
        if (remainder)
        {
            switch (remainder)
            {
            case 1:
                uint8_data[0] = gcm_aad[uint8_num];
                break;
            case 2:
                uint8_data[0] = gcm_aad[uint8_num];
                uint8_data[1] = gcm_aad[uint8_num + 1];
                break;
            case 3:
                uint8_data[0] = gcm_aad[uint8_num];
                uint8_data[1] = gcm_aad[uint8_num + 1];
                uint8_data[2] = gcm_aad[uint8_num + 2];
                break;
            default:
                break;
            }

            uint32_data = *((uint32_t *)(&uint8_data[0]));
            while (!os_aes_get_data_in_flag(userdata))
                ;
            os_aes_write_aad(uint32_data, userdata);
        }
    }
}

static void aes_input_bytes(const uint8_t *input_data, size_t input_data_len, aes_cipher_mode_t cipher_mode, void *userdata)
{
    size_t padding_len, uint32_num, uint8_num, remainder, i;
    uint32_t uint32_data;
    uint8_t uint8_data[4] = {0};

    padding_len = ((input_data_len + 15) / 16) * 16;
    uint32_num = input_data_len / 4;
    for (i = 0; i < uint32_num; i++)
    {
        uint32_data = *((uint32_t *)(&input_data[i * 4]));
        while (!os_aes_get_data_in_flag(userdata))
            ;
        os_aes_write_text(uint32_data, userdata);
    }
    uint8_num = 4 * uint32_num;
    remainder = input_data_len % 4;
    if (remainder)
    {
        switch (remainder)
        {
        case 1:
            uint8_data[0] = input_data[uint8_num];
            break;
        case 2:
            uint8_data[0] = input_data[uint8_num];
            uint8_data[1] = input_data[uint8_num + 1];
            break;
        case 3:
            uint8_data[0] = input_data[uint8_num];
            uint8_data[1] = input_data[uint8_num + 1];
            uint8_data[2] = input_data[uint8_num + 2];
            break;
        default:
            break;
        }

        uint32_data = *((uint32_t *)(&uint8_data[0]));
        while (!os_aes_get_data_in_flag(userdata))
            ;
        os_aes_write_text(uint32_data, userdata);
    }
    if ((cipher_mode == AES_ECB) || (cipher_mode == AES_CBC))
    {
        uint32_num = (padding_len - input_data_len) / 4;
        for (i = 0; i < uint32_num; i++)
        {
            while (!os_aes_get_data_in_flag(userdata))
                ;
            os_aes_write_text(0, userdata);
        }
        uint32_num = padding_len / 4;
    }
}

static void aes_process_less_80_bytes(const uint8_t *input_data,
    uint8_t *output_data,
    size_t input_data_len,
    aes_cipher_mode_t cipher_mode,
    void *userdata)
{
    size_t padding_len, uint32_num, uint8_num, remainder, i;
    uint32_t uint32_data;
    uint8_t uint8_data[4] = {0};

    padding_len = ((input_data_len + 15) / 16) * 16;
    uint32_num = input_data_len / 4;
    for (i = 0; i < uint32_num; i++)
    {
        uint32_data = *((uint32_t *)(&input_data[i * 4]));
        while (!os_aes_get_data_in_flag(userdata))
            ;
        os_aes_write_text(uint32_data, userdata);
    }
    uint8_num = 4 * uint32_num;
    remainder = input_data_len % 4;
    if (remainder)
    {
        switch (remainder)
        {
        case 1:
            uint8_data[0] = input_data[uint8_num];
            break;
        case 2:
            uint8_data[0] = input_data[uint8_num];
            uint8_data[1] = input_data[uint8_num + 1];
            break;
        case 3:
            uint8_data[0] = input_data[uint8_num];
            uint8_data[1] = input_data[uint8_num + 1];
            uint8_data[2] = input_data[uint8_num + 2];
            break;
        default:
            break;
        }

        uint32_data = *((uint32_t *)(&uint8_data[0]));
        while (!os_aes_get_data_in_flag(userdata))
            ;
        os_aes_write_text(uint32_data, userdata);
    }
    if ((cipher_mode == AES_ECB) || (cipher_mode == AES_CBC))
    {
        uint32_num = (padding_len - input_data_len) / 4;
        for (i = 0; i < uint32_num; i++)
        {
            while (!os_aes_get_data_in_flag(userdata))
                ;
            os_aes_write_text(0, userdata);
        }
        uint32_num = padding_len / 4;
    }
    for (i = 0; i < uint32_num; i++)
    {
        while (!os_aes_get_data_out_flag(userdata))
            ;
        *((uint32_t *)(&output_data[i * 4])) = os_aes_read_out_data(userdata);
    }
    if ((cipher_mode == AES_GCM) && (remainder))
    {
        while (!os_aes_get_data_out_flag(userdata))
            ;
        *((uint32_t *)(&uint8_data[0])) = os_aes_read_out_data(userdata);
        switch (remainder)
        {
        case 1:
            output_data[uint32_num * 4] = uint8_data[0];
            break;
        case 2:
            output_data[uint32_num * 4] = uint8_data[0];
            output_data[(i * 4) + 1] = uint8_data[1];
            break;
        case 3:
            output_data[uint32_num * 4] = uint8_data[0];
            output_data[(i * 4) + 1] = uint8_data[1];
            output_data[(i * 4) + 2] = uint8_data[2];
            break;
        default:
            break;
        }
    }
}

static void os_aes_process(const uint8_t *input_data,
    uint8_t *output_data,
    size_t input_data_len,
    aes_cipher_mode_t cipher_mode,
    void *userdata)
{
    size_t temp_len = 0;
    uint32_t i = 0;

    if (input_data_len >= 80)
    {
        for (i = 0; i < (input_data_len / 80); i++)
            aes_process_less_80_bytes(&input_data[i * 80], &output_data[i * 80], 80, cipher_mode, userdata);
    }
    temp_len = input_data_len % 80;
    if (temp_len)
        aes_process_less_80_bytes(&input_data[i * 80], &output_data[i * 80], temp_len, cipher_mode, userdata);
}

static void aes_ecb128_hard_decrypt(const uint8_t *input_key, const uint8_t *input_data, size_t input_len, uint8_t *output_data, void *userdata)
{
    COMMON_ENTRY;
    entry_exclusive(data);
    size_t padding_len = ((input_len + 15) / 16) * 16;
    os_aes_init(input_key, AES_128, NULL, 0L, NULL, AES_ECB, AES_HARD_DECRYPTION, 0L, input_len, userdata);
    if(padding_len <= AES_TRANSMISSION_THRESHOLD)
    {
        os_aes_process(input_data, output_data, padding_len, AES_ECB, userdata);
    }
    else
    {
        handle_t aes_read = dma_open_free();
        dma_set_request_source(aes_read, data->dma_req_base);

        SemaphoreHandle_t event_read = xSemaphoreCreateBinary();
        aes->dma_sel = 1;
        dma_transmit_async(aes_read, &aes->aes_out_data, output_data, 0, 1, sizeof(uint32_t), padding_len >> 2, 4, event_read);
        aes_input_bytes(input_data, input_len, AES_ECB, userdata);
        configASSERT(xSemaphoreTake(event_read, portMAX_DELAY) == pdTRUE);
        dma_close(aes_read);
        vSemaphoreDelete(event_read);
    }
    exit_exclusive(data);
}

static void aes_ecb128_hard_encrypt(const uint8_t *input_key, const uint8_t *input_data, size_t input_len, uint8_t *output_data, void *userdata)
{
    COMMON_ENTRY;
    entry_exclusive(data);
    size_t padding_len = ((input_len + 15) / 16) * 16;
    os_aes_init(input_key, AES_128, NULL, 0L, NULL, AES_ECB, AES_HARD_ENCRYPTION, 0L, input_len, userdata);
    if(padding_len <= AES_TRANSMISSION_THRESHOLD)
    {
        os_aes_process(input_data, output_data, input_len, AES_ECB, userdata);
    }
    else
    {
        handle_t aes_read = dma_open_free();
        dma_set_request_source(aes_read, data->dma_req_base);

        SemaphoreHandle_t event_read = xSemaphoreCreateBinary();
        aes->dma_sel = 1;
        dma_transmit_async(aes_read, &aes->aes_out_data, output_data, 0, 1, sizeof(uint32_t), padding_len >> 2, 4, event_read);
        aes_input_bytes(input_data, input_len, AES_ECB, userdata);
        configASSERT(xSemaphoreTake(event_read, portMAX_DELAY) == pdTRUE);
        dma_close(aes_read);
        vSemaphoreDelete(event_read);
    }
    exit_exclusive(data);
}

static void aes_ecb192_hard_decrypt(const uint8_t *input_key, const uint8_t *input_data, size_t input_len, uint8_t *output_data, void *userdata)
{
    COMMON_ENTRY;
    entry_exclusive(data);
    size_t padding_len = ((input_len + 15) / 16) * 16;
    os_aes_init(input_key, AES_192, NULL, 0L, NULL, AES_ECB, AES_HARD_DECRYPTION, 0L, input_len, userdata);
    if(padding_len <= AES_TRANSMISSION_THRESHOLD)
    {
        os_aes_process(input_data, output_data, padding_len, AES_ECB, userdata);
    }
    else
    {
        handle_t aes_read = dma_open_free();
        dma_set_request_source(aes_read, data->dma_req_base);

        SemaphoreHandle_t event_read = xSemaphoreCreateBinary();
        aes->dma_sel = 1;
        dma_transmit_async(aes_read, &aes->aes_out_data, output_data, 0, 1, sizeof(uint32_t), padding_len >> 2, 4, event_read);
        aes_input_bytes(input_data, input_len, AES_ECB, userdata);
        configASSERT(xSemaphoreTake(event_read, portMAX_DELAY) == pdTRUE);
        dma_close(aes_read);
        vSemaphoreDelete(event_read);
    }
    exit_exclusive(data);
}

static void aes_ecb192_hard_encrypt(const uint8_t *input_key, const uint8_t *input_data, size_t input_len, uint8_t *output_data, void *userdata)
{
    COMMON_ENTRY;
    entry_exclusive(data);
    size_t padding_len = ((input_len + 15) / 16) * 16;
    os_aes_init(input_key, AES_192, NULL, 0L, NULL, AES_ECB, AES_HARD_ENCRYPTION, 0L, input_len, userdata);
    if(padding_len <= AES_TRANSMISSION_THRESHOLD)
    {
        os_aes_process(input_data, output_data, input_len, AES_ECB, userdata);
    }
    else
    {
        handle_t aes_read = dma_open_free();
        dma_set_request_source(aes_read, data->dma_req_base);

        SemaphoreHandle_t event_read = xSemaphoreCreateBinary();
        aes->dma_sel = 1;
        dma_transmit_async(aes_read, &aes->aes_out_data, output_data, 0, 1, sizeof(uint32_t), padding_len >> 2, 4, event_read);
        aes_input_bytes(input_data, input_len, AES_ECB, userdata);
        configASSERT(xSemaphoreTake(event_read, portMAX_DELAY) == pdTRUE);
        dma_close(aes_read);
        vSemaphoreDelete(event_read);
    }
    exit_exclusive(data);
}

static void aes_ecb256_hard_decrypt(const uint8_t *input_key, const uint8_t *input_data, size_t input_len, uint8_t *output_data, void *userdata)
{
    COMMON_ENTRY;
    entry_exclusive(data);
    size_t padding_len = ((input_len + 15) / 16) * 16;
    os_aes_init(input_key, AES_256, NULL, 0L, NULL, AES_ECB, AES_HARD_DECRYPTION, 0L, input_len, userdata);
    if(padding_len <= AES_TRANSMISSION_THRESHOLD)
    {
        os_aes_process(input_data, output_data, padding_len, AES_ECB, userdata);
    }
    else
    {
        handle_t aes_read = dma_open_free();
        dma_set_request_source(aes_read, data->dma_req_base);

        SemaphoreHandle_t event_read = xSemaphoreCreateBinary();
        aes->dma_sel = 1;
        dma_transmit_async(aes_read, &aes->aes_out_data, output_data, 0, 1, sizeof(uint32_t), padding_len >> 2, 4, event_read);
        aes_input_bytes(input_data, input_len, AES_ECB, userdata);
        configASSERT(xSemaphoreTake(event_read, portMAX_DELAY) == pdTRUE);
        dma_close(aes_read);
        vSemaphoreDelete(event_read);
    }
    exit_exclusive(data);
}

static void aes_ecb256_hard_encrypt(const uint8_t *input_key, const uint8_t *input_data, size_t input_len, uint8_t *output_data, void *userdata)
{
    COMMON_ENTRY;
    entry_exclusive(data);
    size_t padding_len = ((input_len + 15) / 16) * 16;
    os_aes_init(input_key, AES_256, NULL, 0L, NULL, AES_ECB, AES_HARD_ENCRYPTION, 0L, input_len, userdata);
    if(padding_len <= AES_TRANSMISSION_THRESHOLD)
    {
        os_aes_process(input_data, output_data, input_len, AES_ECB, userdata);
    }
    else
    {
        handle_t aes_read = dma_open_free();
        dma_set_request_source(aes_read, data->dma_req_base);

        SemaphoreHandle_t event_read = xSemaphoreCreateBinary();
        aes->dma_sel = 1;
        dma_transmit_async(aes_read, &aes->aes_out_data, output_data, 0, 1, sizeof(uint32_t), padding_len >> 2, 4, event_read);
        aes_input_bytes(input_data, input_len, AES_ECB, userdata);
        configASSERT(xSemaphoreTake(event_read, portMAX_DELAY) == pdTRUE);
        dma_close(aes_read);
        vSemaphoreDelete(event_read);
    }
    exit_exclusive(data);
}

static void aes_cbc128_hard_decrypt(cbc_context_t *context, const uint8_t *input_data, size_t input_len, uint8_t *output_data, void *userdata)
{
    COMMON_ENTRY;
    entry_exclusive(data);
    size_t padding_len = ((input_len + 15) / 16) * 16;
    os_aes_init(context->input_key, AES_128, context->iv, IV_LEN_128, NULL, AES_CBC, AES_HARD_DECRYPTION, 0L, input_len, userdata);
    if(padding_len <= AES_TRANSMISSION_THRESHOLD)
    {
        os_aes_process(input_data, output_data, padding_len, AES_CBC, userdata);
    }
    else
    {
        handle_t aes_read = dma_open_free();
        dma_set_request_source(aes_read, data->dma_req_base);

        SemaphoreHandle_t event_read = xSemaphoreCreateBinary();
        aes->dma_sel = 1;
        dma_transmit_async(aes_read, &aes->aes_out_data, output_data, 0, 1, sizeof(uint32_t), padding_len >> 2, 4, event_read);
        aes_input_bytes(input_data, input_len, AES_CBC, userdata);
        configASSERT(xSemaphoreTake(event_read, portMAX_DELAY) == pdTRUE);
        dma_close(aes_read);
        vSemaphoreDelete(event_read);
    }
    exit_exclusive(data);
}

static void aes_cbc128_hard_encrypt(cbc_context_t *context, const uint8_t *input_data, size_t input_len, uint8_t *output_data, void *userdata)
{
    COMMON_ENTRY;
    entry_exclusive(data);
    size_t padding_len = ((input_len + 15) / 16) * 16;
    os_aes_init(context->input_key, AES_128, context->iv, IV_LEN_128, NULL, AES_CBC, AES_HARD_ENCRYPTION, 0L, input_len, userdata);
    if(padding_len <= AES_TRANSMISSION_THRESHOLD)
    {
        os_aes_process(input_data, output_data, input_len, AES_CBC, userdata);
    }
    else
    {
        handle_t aes_read = dma_open_free();

        dma_set_request_source(aes_read, data->dma_req_base);

        SemaphoreHandle_t event_read = xSemaphoreCreateBinary();
        aes->dma_sel = 1;
        dma_transmit_async(aes_read, &aes->aes_out_data, output_data, 0, 1, sizeof(uint32_t), padding_len >> 2, 4, event_read);
        aes_input_bytes(input_data, input_len, AES_CBC, userdata);
        configASSERT(xSemaphoreTake(event_read, portMAX_DELAY) == pdTRUE);

        dma_close(aes_read);
        vSemaphoreDelete(event_read);
    }
    exit_exclusive(data);
}

static void aes_cbc192_hard_decrypt(cbc_context_t *context, const uint8_t *input_data, size_t input_len, uint8_t *output_data, void *userdata)
{
    COMMON_ENTRY;
    entry_exclusive(data);
    size_t padding_len = ((input_len + 15) / 16) * 16;
    os_aes_init(context->input_key, AES_192, context->iv, IV_LEN_128, NULL, AES_CBC, AES_HARD_DECRYPTION, 0L, input_len, userdata);
    if(padding_len <= AES_TRANSMISSION_THRESHOLD)
    {
        os_aes_process(input_data, output_data, padding_len, AES_CBC, userdata);
    }
    else
    {
        handle_t aes_read = dma_open_free();
        dma_set_request_source(aes_read, data->dma_req_base);

        SemaphoreHandle_t event_read = xSemaphoreCreateBinary();
        aes->dma_sel = 1;
        dma_transmit_async(aes_read, &aes->aes_out_data, output_data, 0, 1, sizeof(uint32_t), padding_len >> 2, 4, event_read);
        aes_input_bytes(input_data, input_len, AES_CBC, userdata);
        configASSERT(xSemaphoreTake(event_read, portMAX_DELAY) == pdTRUE);
        dma_close(aes_read);
        vSemaphoreDelete(event_read);
    }
    exit_exclusive(data);
}

static void aes_cbc192_hard_encrypt(cbc_context_t *context, const uint8_t *input_data, size_t input_len, uint8_t *output_data, void *userdata)
{
    COMMON_ENTRY;
    entry_exclusive(data);
    size_t padding_len = ((input_len + 15) / 16) * 16;
    os_aes_init(context->input_key, AES_192, context->iv, IV_LEN_128, NULL, AES_CBC, AES_HARD_ENCRYPTION, 0L, input_len, userdata);
    if(padding_len <= AES_TRANSMISSION_THRESHOLD)
    {
        os_aes_process(input_data, output_data, input_len, AES_CBC, userdata);
    }
    else
    {
        handle_t aes_read = dma_open_free();
        dma_set_request_source(aes_read, data->dma_req_base);

        SemaphoreHandle_t event_read = xSemaphoreCreateBinary();
        aes->dma_sel = 1;
        dma_transmit_async(aes_read, &aes->aes_out_data, output_data, 0, 1, sizeof(uint32_t), padding_len >> 2, 4, event_read);
        aes_input_bytes(input_data, input_len, AES_CBC, userdata);
        configASSERT(xSemaphoreTake(event_read, portMAX_DELAY) == pdTRUE);
        dma_close(aes_read);
        vSemaphoreDelete(event_read);
    }
    exit_exclusive(data);
}

static void aes_cbc256_hard_decrypt(cbc_context_t *context, const uint8_t *input_data, size_t input_len, uint8_t *output_data, void *userdata)
{
    COMMON_ENTRY;
    entry_exclusive(data);
    size_t padding_len = ((input_len + 15) / 16) * 16;
    os_aes_init(context->input_key, AES_256, context->iv, IV_LEN_128, NULL, AES_CBC, AES_HARD_DECRYPTION, 0L, input_len, userdata);
    if(padding_len <= AES_TRANSMISSION_THRESHOLD)
    {
        os_aes_process(input_data, output_data, padding_len, AES_CBC, userdata);
    }
    else
    {
        handle_t aes_read = dma_open_free();
        dma_set_request_source(aes_read, data->dma_req_base);

        SemaphoreHandle_t event_read = xSemaphoreCreateBinary();
        aes->dma_sel = 1;
        dma_transmit_async(aes_read, &aes->aes_out_data, output_data, 0, 1, sizeof(uint32_t), padding_len >> 2, 4, event_read);
        aes_input_bytes(input_data, input_len, AES_CBC, userdata);
        configASSERT(xSemaphoreTake(event_read, portMAX_DELAY) == pdTRUE);
        dma_close(aes_read);
        vSemaphoreDelete(event_read);
    }
    exit_exclusive(data);
}

static void aes_cbc256_hard_encrypt(cbc_context_t *context, const uint8_t *input_data, size_t input_len, uint8_t *output_data, void *userdata)
{
    COMMON_ENTRY;
    entry_exclusive(data);
    size_t padding_len = ((input_len + 15) / 16) * 16;
    os_aes_init(context->input_key, AES_256, context->iv, IV_LEN_128, NULL, AES_CBC, AES_HARD_ENCRYPTION, 0L, input_len, userdata);
    if(padding_len <= AES_TRANSMISSION_THRESHOLD)
    {
        os_aes_process(input_data, output_data, input_len, AES_CBC, userdata);
    }
    else
    {
        handle_t aes_read = dma_open_free();
        dma_set_request_source(aes_read, data->dma_req_base);

        SemaphoreHandle_t event_read = xSemaphoreCreateBinary();
        aes->dma_sel = 1;
        dma_transmit_async(aes_read, &aes->aes_out_data, output_data, 0, 1, sizeof(uint32_t), padding_len >> 2, 4, event_read);
        aes_input_bytes(input_data, input_len, AES_CBC, userdata);
        configASSERT(xSemaphoreTake(event_read, portMAX_DELAY) == pdTRUE);
        dma_close(aes_read);
        vSemaphoreDelete(event_read);
    }
    exit_exclusive(data);
}

static void aes_gcm128_hard_decrypt(gcm_context_t *context, const uint8_t *input_data, size_t input_len, uint8_t *output_data, uint8_t *gcm_tag, void *userdata)
{
    COMMON_ENTRY;
    entry_exclusive(data);
    os_aes_init(context->input_key, AES_128, context->iv, IV_LEN_96, context->gcm_aad,
            AES_GCM, AES_HARD_DECRYPTION, context->gcm_aad_len, input_len, userdata);
    if(input_len <= AES_TRANSMISSION_THRESHOLD)
    {
        os_aes_process(input_data, output_data, input_len, AES_GCM, userdata);
    }
    else
    {
        configASSERT(input_len % 4 == 0);

        handle_t aes_read = dma_open_free();
        dma_set_request_source(aes_read, data->dma_req_base);

        SemaphoreHandle_t event_read = xSemaphoreCreateBinary();
        aes->dma_sel = 1;
        dma_transmit_async(aes_read, &aes->aes_out_data, output_data, 0, 1, sizeof(uint32_t), input_len >> 2, 4, event_read);
        aes_input_bytes(input_data, input_len, AES_GCM, userdata);
        configASSERT(xSemaphoreTake(event_read, portMAX_DELAY) == pdTRUE);
        dma_close(aes_read);
        vSemaphoreDelete(event_read);
    }
    os_gcm_get_tag(gcm_tag, userdata);
    exit_exclusive(data);
}

static void aes_gcm128_hard_encrypt(gcm_context_t *context, const uint8_t *input_data, size_t input_len, uint8_t *output_data, uint8_t *gcm_tag, void *userdata)
{
    COMMON_ENTRY;
    entry_exclusive(data);
    os_aes_init(context->input_key, AES_128, context->iv, IV_LEN_96, context->gcm_aad,
            AES_GCM, AES_HARD_ENCRYPTION, context->gcm_aad_len, input_len, userdata);
    if(input_len <= AES_TRANSMISSION_THRESHOLD)
    {
        os_aes_process(input_data, output_data, input_len, AES_GCM, userdata);
    }
    else
    {
        configASSERT(input_len % 4 == 0);

        handle_t aes_read = dma_open_free();
        dma_set_request_source(aes_read, data->dma_req_base);

        SemaphoreHandle_t event_read = xSemaphoreCreateBinary();
        aes->dma_sel = 1;
        dma_transmit_async(aes_read, &aes->aes_out_data, output_data, 0, 1, sizeof(uint32_t), input_len >> 2, 4, event_read);
        aes_input_bytes(input_data, input_len, AES_GCM, userdata);
        configASSERT(xSemaphoreTake(event_read, portMAX_DELAY) == pdTRUE);
        dma_close(aes_read);
        vSemaphoreDelete(event_read);
    }
    os_gcm_get_tag(gcm_tag, userdata);
    exit_exclusive(data);
}

static void aes_gcm192_hard_decrypt(gcm_context_t *context, const uint8_t *input_data, size_t input_len, uint8_t *output_data, uint8_t *gcm_tag, void *userdata)
{
    COMMON_ENTRY;
    entry_exclusive(data);
    os_aes_init(context->input_key, AES_192, context->iv, IV_LEN_96, context->gcm_aad,
            AES_GCM, AES_HARD_DECRYPTION, context->gcm_aad_len, input_len, userdata);
    if(input_len <= AES_TRANSMISSION_THRESHOLD)
    {
        os_aes_process(input_data, output_data, input_len, AES_GCM, userdata);
    }
    else
    {
        configASSERT(input_len % 4 == 0);

        handle_t aes_read = dma_open_free();
        dma_set_request_source(aes_read, data->dma_req_base);

        SemaphoreHandle_t event_read = xSemaphoreCreateBinary();
        aes->dma_sel = 1;
        dma_transmit_async(aes_read, &aes->aes_out_data, output_data, 0, 1, sizeof(uint32_t), input_len >> 2, 4, event_read);
        aes_input_bytes(input_data, input_len, AES_GCM, userdata);
        configASSERT(xSemaphoreTake(event_read, portMAX_DELAY) == pdTRUE);
        dma_close(aes_read);
        vSemaphoreDelete(event_read);
    }
    os_gcm_get_tag(gcm_tag, userdata);
    exit_exclusive(data);
}

static void aes_gcm192_hard_encrypt(gcm_context_t *context, const uint8_t *input_data, size_t input_len, uint8_t *output_data, uint8_t *gcm_tag, void *userdata)
{
    COMMON_ENTRY;
    entry_exclusive(data);
    os_aes_init(context->input_key, AES_192, context->iv, IV_LEN_96, context->gcm_aad,
            AES_GCM, AES_HARD_ENCRYPTION, context->gcm_aad_len, input_len, userdata);
    if(input_len <= AES_TRANSMISSION_THRESHOLD)
    {
        os_aes_process(input_data, output_data, input_len, AES_GCM, userdata);
    }
    else
    {
        configASSERT(input_len % 4 == 0);

        handle_t aes_read = dma_open_free();
        dma_set_request_source(aes_read, data->dma_req_base);

        SemaphoreHandle_t event_read = xSemaphoreCreateBinary();
        aes->dma_sel = 1;
        dma_transmit_async(aes_read, &aes->aes_out_data, output_data, 0, 1, sizeof(uint32_t), input_len >> 2, 4, event_read);
        aes_input_bytes(input_data, input_len, AES_GCM, userdata);
        configASSERT(xSemaphoreTake(event_read, portMAX_DELAY) == pdTRUE);
        dma_close(aes_read);
        vSemaphoreDelete(event_read);
    }
    os_gcm_get_tag(gcm_tag, userdata);
    exit_exclusive(data);
}

static void aes_gcm256_hard_decrypt(gcm_context_t *context, const uint8_t *input_data, size_t input_len, uint8_t *output_data, uint8_t *gcm_tag, void *userdata)
{
    COMMON_ENTRY;
    entry_exclusive(data);
    os_aes_init(context->input_key, AES_256, context->iv, IV_LEN_96, context->gcm_aad,
            AES_GCM, AES_HARD_DECRYPTION, context->gcm_aad_len, input_len, userdata);
    if(input_len <= AES_TRANSMISSION_THRESHOLD)
    {
        os_aes_process(input_data, output_data, input_len, AES_GCM, userdata);
    }
    else
    {
        configASSERT(input_len % 4 == 0);

        handle_t aes_read = dma_open_free();
        dma_set_request_source(aes_read, data->dma_req_base);

        SemaphoreHandle_t event_read = xSemaphoreCreateBinary();
        aes->dma_sel = 1;
        dma_transmit_async(aes_read, &aes->aes_out_data, output_data, 0, 1, sizeof(uint32_t), input_len >> 2, 4, event_read);
        aes_input_bytes(input_data, input_len, AES_GCM, userdata);
        configASSERT(xSemaphoreTake(event_read, portMAX_DELAY) == pdTRUE);
        dma_close(aes_read);
        vSemaphoreDelete(event_read);
    }
    os_gcm_get_tag(gcm_tag, userdata);
    exit_exclusive(data);
}

static void aes_gcm256_hard_encrypt(gcm_context_t *context, const uint8_t *input_data, size_t input_len, uint8_t *output_data, uint8_t *gcm_tag, void *userdata)
{
    COMMON_ENTRY;
    entry_exclusive(data);
    os_aes_init(context->input_key, AES_256, context->iv, IV_LEN_96, context->gcm_aad,
            AES_GCM, AES_HARD_ENCRYPTION, context->gcm_aad_len, input_len, userdata);
    if(input_len <= AES_TRANSMISSION_THRESHOLD)
    {
        os_aes_process(input_data, output_data, input_len, AES_GCM, userdata);
    }
    else
    {
        configASSERT(input_len % 4 == 0);

        handle_t aes_read = dma_open_free();
        dma_set_request_source(aes_read, data->dma_req_base);

        SemaphoreHandle_t event_read = xSemaphoreCreateBinary();
        aes->dma_sel = 1;
        dma_transmit_async(aes_read, &aes->aes_out_data, output_data, 0, 1, sizeof(uint32_t), input_len >> 2, 4, event_read);
        aes_input_bytes(input_data, input_len, AES_GCM, userdata);
        configASSERT(xSemaphoreTake(event_read, portMAX_DELAY) == pdTRUE);
        dma_close(aes_read);
        vSemaphoreDelete(event_read);
    }
    os_gcm_get_tag(gcm_tag, userdata);
    exit_exclusive(data);
}

static aes_dev_data dev0_data = {SYSCTL_CLOCK_AES, AES_BASE_ADDR, SYSCTL_DMA_SELECT_AES_REQ, 0, {{0}}};

const aes_driver_t g_aes_driver_aes0 =
{
    {&dev0_data, aes_install, aes_open, aes_close},
    aes_ecb128_hard_decrypt,
    aes_ecb128_hard_encrypt,
    aes_ecb192_hard_decrypt,
    aes_ecb192_hard_encrypt,
    aes_ecb256_hard_decrypt,
    aes_ecb256_hard_encrypt,
    aes_cbc128_hard_decrypt,
    aes_cbc128_hard_encrypt,
    aes_cbc192_hard_decrypt,
    aes_cbc192_hard_encrypt,
    aes_cbc256_hard_decrypt,
    aes_cbc256_hard_encrypt,
    aes_gcm128_hard_decrypt,
    aes_gcm128_hard_encrypt,
    aes_gcm192_hard_decrypt,
    aes_gcm192_hard_encrypt,
    aes_gcm256_hard_decrypt,
    aes_gcm256_hard_encrypt
};
