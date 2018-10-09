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
#include <sysctl.h>

#define AES_BASE_ADDR (0x50450000)
#define COMMON_ENTRY                                               \
    aes_dev_data *data = (aes_dev_data *)userdata;                 \
    volatile aes_t *aes = (volatile aes_t *)data->base_addr;       \
    (void)aes;

typedef struct
{
    sysctl_clock_t clock;
    uintptr_t base_addr;
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

static void os_aes_write_tag(uint32_t *tag, void *userdata)
{
    COMMON_ENTRY;
    aes->gcm_in_tag[0] = tag[3];
    aes->gcm_in_tag[1] = tag[2];
    aes->gcm_in_tag[2] = tag[1];
    aes->gcm_in_tag[3] = tag[0];
}

static uint32_t os_get_data_in_flag(void *userdata)
{
    COMMON_ENTRY;
    return aes->data_in_flag;
}

static uint32_t os_get_data_out_flag(void *userdata)
{
    COMMON_ENTRY;
    return aes->data_out_flag;
}

static uint32_t os_get_tag_in_flag(void *userdata)
{
    COMMON_ENTRY;
    return aes->tag_in_flag;
}

static uint32_t os_read_out_data(void *userdata)
{
    COMMON_ENTRY;
    return aes->aes_out_data;
}

static uint32_t os_aes_check_tag(void *userdata)
{
    COMMON_ENTRY;
    return aes->tag_chk;
}

static void os_aes_get_tag(uint8_t *output_tag, void *userdata)
{
    COMMON_ENTRY;
    uint32_t uint32_tag;
    uint8_t i = 0;

    uint32_tag = aes->gcm_out_tag[3];
    output_tag[i++] = (uint8_t)((uint32_tag >> 24) & 0xff);
    output_tag[i++] = (uint8_t)((uint32_tag >> 16) & 0xff);
    output_tag[i++] = (uint8_t)((uint32_tag >> 8) & 0xff);
    output_tag[i++] = (uint8_t)((uint32_tag)&0xff);

    uint32_tag = aes->gcm_out_tag[2];
    output_tag[i++] = (uint8_t)((uint32_tag >> 24) & 0xff);
    output_tag[i++] = (uint8_t)((uint32_tag >> 16) & 0xff);
    output_tag[i++] = (uint8_t)((uint32_tag >> 8) & 0xff);
    output_tag[i++] = (uint8_t)((uint32_tag)&0xff);

    uint32_tag = aes->gcm_out_tag[1];
    output_tag[i++] = (uint8_t)((uint32_tag >> 24) & 0xff);
    output_tag[i++] = (uint8_t)((uint32_tag >> 16) & 0xff);
    output_tag[i++] = (uint8_t)((uint32_tag >> 8) & 0xff);
    output_tag[i++] = (uint8_t)((uint32_tag)&0xff);

    uint32_tag = aes->gcm_out_tag[0];
    output_tag[i++] = (uint8_t)((uint32_tag >> 24) & 0xff);
    output_tag[i++] = (uint8_t)((uint32_tag >> 16) & 0xff);
    output_tag[i++] = (uint8_t)((uint32_tag >> 8) & 0xff);
    output_tag[i++] = (uint8_t)((uint32_tag)&0xff);
}

static void os_aes_clear_chk_tag(void *userdata)
{
    COMMON_ENTRY;
    aes->tag_clear = 0;
}

static int os_check_tag(uint32_t *aes_gcm_tag, void *userdata)
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

static void os_aes_init(uint8_t *input_key, size_t input_key_len, uint8_t *iv, size_t iv_len,
    uint8_t *gcm_add, aes_cipher_mode_t cipher_mode, aes_encrypt_sel_t encrypt_sel, size_t gcm_add_len,
    size_t input_data_len, void *userdata)
{
    COMMON_ENTRY;
    size_t padding_len, i, remainder, uint32_num, uint8_num;
    uint32_t uint32_data;
    uint8_t uint8_data[4] = {0};

    padding_len = input_data_len;
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
    remainder = input_key_len % 4;
    if (remainder)
    {
        switch (remainder)
        {
            case 1:
                uint8_data[0] = input_key[0];
                break;
            case 2:
                uint8_data[0] = input_key[0];
                uint8_data[1] = input_key[1];
                break;
            case 3:
                uint8_data[0] = input_key[0];
                uint8_data[1] = input_key[1];
                uint8_data[2] = input_key[2];
                break;
            default:
                break;
        }
        if (uint32_num < 4)
            aes->aes_key[uint32_num] = *((uint32_t *)(&uint8_data[0]));
        else
            aes->aes_key_ext[uint32_num - 4] = *((uint32_t *)(&uint8_data[0]));
    }
    uint32_num = iv_len / 4;
    for (i = 0; i < uint32_num; i++)
        aes->aes_iv[i] = *((uint32_t *)(&iv[iv_len - (4 * i) - 4]));
    remainder = iv_len % 4;
    if (remainder)
    {
        switch (remainder)
        {
        case 1:
            uint8_data[0] = iv[0];
            break;
        case 2:
            uint8_data[0] = iv[0];
            uint8_data[1] = iv[1];
            break;
        case 3:
            uint8_data[0] = iv[0];
            uint8_data[1] = iv[1];
            uint8_data[2] = iv[2];
            break;
        default:
            break;
        }
        aes->aes_iv[uint32_num] = *((uint32_t *)(&uint8_data[0]));
    }
    aes->mode_ctl.kmode = input_key_len / 8 - 2; /* 00:AES_128 01:AES_192 10:AES_256 11:RESERVED */
    aes->mode_ctl.cipher_mode = cipher_mode;
    aes->encrypt_sel = encrypt_sel;
    aes->gb_aad_end_adr = gcm_add_len - 1;
    aes->gb_pc_end_adr = padding_len - 1;
    aes->gb_aes_en |= 1;

    if (cipher_mode == AES_GCM)
    {
        uint32_num = gcm_add_len / 4;
        for (i = 0; i < uint32_num; i++)
        {
            uint32_data = *((uint32_t *)(gcm_add + i * 4));
            while (!os_get_data_in_flag(userdata))
                ;
            os_aes_write_aad(uint32_data, userdata);
        }
        uint8_num = 4 * uint32_num;
        remainder = gcm_add_len % 4;
        if (remainder)
        {
            switch (remainder)
            {
                case 1:
                    uint8_data[0] = gcm_add[uint8_num];
                    break;
                case 2:
                    uint8_data[0] = gcm_add[uint8_num];
                    uint8_data[1] = gcm_add[uint8_num + 1];
                    break;
                case 3:
                    uint8_data[0] = gcm_add[uint8_num];
                    uint8_data[1] = gcm_add[uint8_num + 1];
                    uint8_data[2] = gcm_add[uint8_num + 2];
                    break;
                default:
                    break;
            }
            uint32_data = *((uint32_t *)(&uint8_data[0]));
            while (!os_get_data_in_flag(userdata))
                ;
            os_aes_write_aad(uint32_data, userdata);
        }
    }
}

static void aes_process_less_80_bytes(uint8_t *input_data,
    uint8_t *output_data,
    uint32_t input_data_len,
    aes_cipher_mode_t cipher_mode,
    void *userdata)
{
    size_t padding_len, uint32_num, i, remainder, uint8_num;
    uint32_t uint32_data;
    uint8_t uint8_data[4] = {0};
    padding_len = ((input_data_len + 15) / 16) * 16;
    uint32_num = input_data_len / 4;
    for (i = 0; i < uint32_num; i++)
    {
        uint32_data = *((uint32_t *)(&input_data[i * 4]));
        while (!os_get_data_in_flag(userdata))
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
        while (!os_get_data_in_flag(userdata))
            ;
        os_aes_write_text(uint32_data, userdata);
    }
    if ((cipher_mode == AES_ECB) || (cipher_mode == AES_CBC))
    {
        uint32_num = (padding_len - input_data_len) / 4;
        for (i = 0; i < uint32_num; i++)
        {
            while (!os_get_data_in_flag(userdata))
                ;
            os_aes_write_text(0, userdata);
        }
        uint32_num = padding_len / 4;
    }
    for (i = 0; i < uint32_num; i++)
    {
        while (!os_get_data_out_flag(userdata))
            ;
        *((uint32_t *)(&output_data[i * 4])) = os_read_out_data(userdata);
    }

    if ((cipher_mode == AES_GCM) && (remainder))
    {
        while (!os_get_data_out_flag(userdata))
            ;

        *((uint32_t *)(&uint8_data[0])) = os_read_out_data(userdata);
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

static int os_aes_process(uint8_t *input_data,
    uint8_t *output_data,
    size_t input_data_len,
    aes_cipher_mode_t cipher_mode,
    void *userdata)
{
    size_t temp_len, i = 0;

    if (input_data_len >= 80)
    {
        for (i = 0; i < (input_data_len / 80); i++)
            aes_process_less_80_bytes(&input_data[i * 80], &output_data[i * 80], 80, cipher_mode, userdata);
    }
    temp_len = input_data_len % 80;
    if (temp_len)
        aes_process_less_80_bytes(&input_data[i * 80], &output_data[i * 80], temp_len, cipher_mode, userdata);
    return 1;
}

static void aes_hard_decrypt(const aes_param_t *param, void *userdata)
{
    COMMON_ENTRY;
    configASSERT(param->input_key_len == AES_128 || param->input_key_len == AES_192 || param->input_key_len == AES_256);
    int padding_len = param->input_data_len;
    entry_exclusive(data);
    os_aes_init(param->input_key, param->input_key_len, param->iv, param->iv_len, param->gcm_add,
        param->cipher_mode, AES_HARD_DECRYPTION, param->gcm_add_len, param->input_data_len, userdata);
    if (param->cipher_mode == AES_CBC || param->cipher_mode == AES_ECB)
    {
        padding_len = ((padding_len + 15) / 16) * 16;
    }
    os_aes_process(param->input_data, param->output_data, padding_len, param->cipher_mode, userdata);
    if (param->cipher_mode == AES_GCM)
    {
        os_aes_get_tag(param->gcm_tag, userdata);
        os_check_tag(data->aes_hardware_tag, userdata); /* haredware need this */
    }
    exit_exclusive(data);
}

static void aes_hard_encrypt(const aes_param_t *param, void *userdata)
{
    COMMON_ENTRY;
    configASSERT(param->input_key_len == AES_128 || param->input_key_len == AES_192 || param->input_key_len == AES_256);
    entry_exclusive(data);
    os_aes_init(param->input_key, param->input_key_len, param->iv, param->iv_len, param->gcm_add,
        param->cipher_mode, AES_HARD_ENCRYPTION, param->gcm_add_len, param->input_data_len, userdata);
    os_aes_process(param->input_data, param->output_data, param->input_data_len, param->cipher_mode, userdata);
    if (param->cipher_mode == AES_GCM)
    {
        os_aes_get_tag(param->gcm_tag, userdata);
        os_check_tag(data->aes_hardware_tag, userdata); /* haredware need this */
    }
    exit_exclusive(data);
}

static aes_dev_data dev0_data = {SYSCTL_CLOCK_AES, AES_BASE_ADDR, 0, {{0}}};

const aes_driver_t g_aes_driver_aes0 = {{&dev0_data, aes_install, aes_open, aes_close}, aes_hard_decrypt, aes_hard_encrypt};
