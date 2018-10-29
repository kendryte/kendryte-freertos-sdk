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
#include <hal.h>
#include <kernel/driver_impl.hpp>
#include <stdlib.h>
#include <string.h>
#include <sysctl.h>

using namespace sys;

#define COMMON_ENTRY \
    semaphore_lock locker(free_mutex_);

class k_aes_driver : public aes_driver, public static_object, public free_object_access
{
public:
    k_aes_driver(uintptr_t base_addr, sysctl_clock_t clock, sysctl_reset_t reset, sysctl_dma_select_t dma_req)
        : aes_(*reinterpret_cast<volatile aes_t *>(base_addr)), clock_(clock), reset_(reset), dma_req_(dma_req)
    {
    }

    virtual void install() override
    {
        free_mutex_ = xSemaphoreCreateMutex();
        sysctl_clock_disable(clock_);
    }

    virtual void on_first_open() override
    {
        sysctl_reset(reset_);
        sysctl_clock_enable(clock_);
    }

    virtual void on_last_close() override
    {
        sysctl_clock_disable(clock_);
    }

    virtual void aes_ecb128_hard_decrypt(gsl::span<const uint8_t> input_key, gsl::span<const uint8_t> input_data, gsl::span<uint8_t> output_data) override
    {
        COMMON_ENTRY;

        size_t input_len = input_data.size();
        size_t padding_len = ((input_len + 15) / 16) * 16;
        os_aes_init(input_key.data(), AES_128, NULL, 0L, NULL, AES_ECB, AES_HARD_DECRYPTION, 0L, input_len);
        if (padding_len <= AES_TRANSMISSION_THRESHOLD)
        {
            os_aes_process(input_data.data(), output_data.data(), padding_len, AES_ECB);
        }
        else
        {
            handle_t aes_read = dma_open_free();
            dma_set_request_source(aes_read, dma_req_);

            SemaphoreHandle_t event_read = xSemaphoreCreateBinary();
            aes_.dma_sel = 1;
            dma_transmit_async(aes_read, &aes_.aes_out_data, output_data.data(), 0, 1, sizeof(uint32_t), padding_len >> 2, 4, event_read);
            aes_input_bytes(input_data.data(), input_len, AES_ECB);
            configASSERT(xSemaphoreTake(event_read, portMAX_DELAY) == pdTRUE);
            dma_close(aes_read);
            vSemaphoreDelete(event_read);
        }
    }

    virtual void aes_ecb128_hard_encrypt(gsl::span<const uint8_t> input_key, gsl::span<const uint8_t> input_data, gsl::span<uint8_t> output_data) override
    {
        COMMON_ENTRY;

        size_t input_len = input_data.size();
        size_t padding_len = ((input_len + 15) / 16) * 16;
        os_aes_init(input_key.data(), AES_128, NULL, 0L, NULL, AES_ECB, AES_HARD_ENCRYPTION, 0L, input_len);
        if (padding_len <= AES_TRANSMISSION_THRESHOLD)
        {
            os_aes_process(input_data.data(), output_data.data(), input_len, AES_ECB);
        }
        else
        {
            handle_t aes_read = dma_open_free();
            dma_set_request_source(aes_read, dma_req_);

            SemaphoreHandle_t event_read = xSemaphoreCreateBinary();
            aes_.dma_sel = 1;
            dma_transmit_async(aes_read, &aes_.aes_out_data, output_data.data(), 0, 1, sizeof(uint32_t), padding_len >> 2, 4, event_read);
            aes_input_bytes(input_data.data(), input_len, AES_ECB);
            configASSERT(xSemaphoreTake(event_read, portMAX_DELAY) == pdTRUE);
            dma_close(aes_read);
            vSemaphoreDelete(event_read);
        }
    }

    virtual void aes_ecb192_hard_decrypt(gsl::span<const uint8_t> input_key, gsl::span<const uint8_t> input_data, gsl::span<uint8_t> output_data) override
    {
        COMMON_ENTRY;

        size_t input_len = input_data.size();
        size_t padding_len = ((input_len + 15) / 16) * 16;
        os_aes_init(input_key.data(), AES_192, NULL, 0L, NULL, AES_ECB, AES_HARD_DECRYPTION, 0L, input_len);
        if (padding_len <= AES_TRANSMISSION_THRESHOLD)
        {
            os_aes_process(input_data.data(), output_data.data(), padding_len, AES_ECB);
        }
        else
        {
            handle_t aes_read = dma_open_free();
            dma_set_request_source(aes_read, dma_req_);

            SemaphoreHandle_t event_read = xSemaphoreCreateBinary();
            aes_.dma_sel = 1;
            dma_transmit_async(aes_read, &aes_.aes_out_data, output_data.data(), 0, 1, sizeof(uint32_t), padding_len >> 2, 4, event_read);
            aes_input_bytes(input_data.data(), input_len, AES_ECB);
            configASSERT(xSemaphoreTake(event_read, portMAX_DELAY) == pdTRUE);
            dma_close(aes_read);
            vSemaphoreDelete(event_read);
        }
    }

    virtual void aes_ecb192_hard_encrypt(gsl::span<const uint8_t> input_key, gsl::span<const uint8_t> input_data, gsl::span<uint8_t> output_data) override
    {
        COMMON_ENTRY;

        size_t input_len = input_data.size();
        size_t padding_len = ((input_len + 15) / 16) * 16;
        os_aes_init(input_key.data(), AES_192, NULL, 0L, NULL, AES_ECB, AES_HARD_ENCRYPTION, 0L, input_len);
        if (padding_len <= AES_TRANSMISSION_THRESHOLD)
        {
            os_aes_process(input_data.data(), output_data.data(), input_len, AES_ECB);
        }
        else
        {
            handle_t aes_read = dma_open_free();
            dma_set_request_source(aes_read, dma_req_);

            SemaphoreHandle_t event_read = xSemaphoreCreateBinary();
            aes_.dma_sel = 1;
            dma_transmit_async(aes_read, &aes_.aes_out_data, output_data.data(), 0, 1, sizeof(uint32_t), padding_len >> 2, 4, event_read);
            aes_input_bytes(input_data.data(), input_len, AES_ECB);
            configASSERT(xSemaphoreTake(event_read, portMAX_DELAY) == pdTRUE);
            dma_close(aes_read);
            vSemaphoreDelete(event_read);
        }
    }

    virtual void aes_ecb256_hard_decrypt(gsl::span<const uint8_t> input_key, gsl::span<const uint8_t> input_data, gsl::span<uint8_t> output_data) override
    {
        COMMON_ENTRY;

        size_t input_len = input_data.size();
        size_t padding_len = ((input_len + 15) / 16) * 16;
        os_aes_init(input_key.data(), AES_256, NULL, 0L, NULL, AES_ECB, AES_HARD_DECRYPTION, 0L, input_len);
        if (padding_len <= AES_TRANSMISSION_THRESHOLD)
        {
            os_aes_process(input_data.data(), output_data.data(), padding_len, AES_ECB);
        }
        else
        {
            handle_t aes_read = dma_open_free();
            dma_set_request_source(aes_read, dma_req_);

            SemaphoreHandle_t event_read = xSemaphoreCreateBinary();
            aes_.dma_sel = 1;
            dma_transmit_async(aes_read, &aes_.aes_out_data, output_data.data(), 0, 1, sizeof(uint32_t), padding_len >> 2, 4, event_read);
            aes_input_bytes(input_data.data(), input_len, AES_ECB);
            configASSERT(xSemaphoreTake(event_read, portMAX_DELAY) == pdTRUE);
            dma_close(aes_read);
            vSemaphoreDelete(event_read);
        }
    }

    virtual void aes_ecb256_hard_encrypt(gsl::span<const uint8_t> input_key, gsl::span<const uint8_t> input_data, gsl::span<uint8_t> output_data) override
    {
        COMMON_ENTRY;

        size_t input_len = input_data.size();
        size_t padding_len = ((input_len + 15) / 16) * 16;
        os_aes_init(input_key.data(), AES_256, NULL, 0L, NULL, AES_ECB, AES_HARD_ENCRYPTION, 0L, input_len);
        if (padding_len <= AES_TRANSMISSION_THRESHOLD)
        {
            os_aes_process(input_data.data(), output_data.data(), input_len, AES_ECB);
        }
        else
        {
            handle_t aes_read = dma_open_free();
            dma_set_request_source(aes_read, dma_req_);

            SemaphoreHandle_t event_read = xSemaphoreCreateBinary();
            aes_.dma_sel = 1;
            dma_transmit_async(aes_read, &aes_.aes_out_data, output_data.data(), 0, 1, sizeof(uint32_t), padding_len >> 2, 4, event_read);
            aes_input_bytes(input_data.data(), input_len, AES_ECB);
            configASSERT(xSemaphoreTake(event_read, portMAX_DELAY) == pdTRUE);
            dma_close(aes_read);
            vSemaphoreDelete(event_read);
        }
    }

    virtual void aes_cbc128_hard_decrypt(cbc_context_t &context, gsl::span<const uint8_t> input_data, gsl::span<uint8_t> output_data) override
    {
        COMMON_ENTRY;

        size_t input_len = input_data.size();
        size_t padding_len = ((input_len + 15) / 16) * 16;
        os_aes_init(context.input_key, AES_128, context.iv, IV_LEN_128, NULL, AES_CBC, AES_HARD_DECRYPTION, 0L, input_len);
        if (padding_len <= AES_TRANSMISSION_THRESHOLD)
        {
            os_aes_process(input_data.data(), output_data.data(), padding_len, AES_CBC);
        }
        else
        {
            handle_t aes_read = dma_open_free();
            dma_set_request_source(aes_read, dma_req_);

            SemaphoreHandle_t event_read = xSemaphoreCreateBinary();
            aes_.dma_sel = 1;
            dma_transmit_async(aes_read, &aes_.aes_out_data, output_data.data(), 0, 1, sizeof(uint32_t), padding_len >> 2, 4, event_read);
            aes_input_bytes(input_data.data(), input_len, AES_CBC);
            configASSERT(xSemaphoreTake(event_read, portMAX_DELAY) == pdTRUE);
            dma_close(aes_read);
            vSemaphoreDelete(event_read);
        }
    }

    virtual void aes_cbc128_hard_encrypt(cbc_context_t &context, gsl::span<const uint8_t> input_data, gsl::span<uint8_t> output_data) override
    {
        COMMON_ENTRY;

        size_t input_len = input_data.size();
        size_t padding_len = ((input_len + 15) / 16) * 16;
        os_aes_init(context.input_key, AES_128, context.iv, IV_LEN_128, NULL, AES_CBC, AES_HARD_ENCRYPTION, 0L, input_len);
        if (padding_len <= AES_TRANSMISSION_THRESHOLD)
        {
            os_aes_process(input_data.data(), output_data.data(), input_len, AES_CBC);
        }
        else
        {
            handle_t aes_read = dma_open_free();

            dma_set_request_source(aes_read, dma_req_);

            SemaphoreHandle_t event_read = xSemaphoreCreateBinary();
            aes_.dma_sel = 1;
            dma_transmit_async(aes_read, &aes_.aes_out_data, output_data.data(), 0, 1, sizeof(uint32_t), padding_len >> 2, 4, event_read);
            aes_input_bytes(input_data.data(), input_len, AES_CBC);
            configASSERT(xSemaphoreTake(event_read, portMAX_DELAY) == pdTRUE);

            dma_close(aes_read);
            vSemaphoreDelete(event_read);
        }
    }

    virtual void aes_cbc192_hard_decrypt(cbc_context_t &context, gsl::span<const uint8_t> input_data, gsl::span<uint8_t> output_data) override
    {
        COMMON_ENTRY;

        size_t input_len = input_data.size();
        size_t padding_len = ((input_len + 15) / 16) * 16;
        os_aes_init(context.input_key, AES_192, context.iv, IV_LEN_128, NULL, AES_CBC, AES_HARD_DECRYPTION, 0L, input_len);
        if (padding_len <= AES_TRANSMISSION_THRESHOLD)
        {
            os_aes_process(input_data.data(), output_data.data(), padding_len, AES_CBC);
        }
        else
        {
            handle_t aes_read = dma_open_free();
            dma_set_request_source(aes_read, dma_req_);

            SemaphoreHandle_t event_read = xSemaphoreCreateBinary();
            aes_.dma_sel = 1;
            dma_transmit_async(aes_read, &aes_.aes_out_data, output_data.data(), 0, 1, sizeof(uint32_t), padding_len >> 2, 4, event_read);
            aes_input_bytes(input_data.data(), input_len, AES_CBC);
            configASSERT(xSemaphoreTake(event_read, portMAX_DELAY) == pdTRUE);
            dma_close(aes_read);
            vSemaphoreDelete(event_read);
        }
    }

    virtual void aes_cbc192_hard_encrypt(cbc_context_t &context, gsl::span<const uint8_t> input_data, gsl::span<uint8_t> output_data) override
    {
        COMMON_ENTRY;

        size_t input_len = input_data.size();
        size_t padding_len = ((input_len + 15) / 16) * 16;
        os_aes_init(context.input_key, AES_192, context.iv, IV_LEN_128, NULL, AES_CBC, AES_HARD_ENCRYPTION, 0L, input_len);
        if (padding_len <= AES_TRANSMISSION_THRESHOLD)
        {
            os_aes_process(input_data.data(), output_data.data(), input_len, AES_CBC);
        }
        else
        {
            handle_t aes_read = dma_open_free();
            dma_set_request_source(aes_read, dma_req_);

            SemaphoreHandle_t event_read = xSemaphoreCreateBinary();
            aes_.dma_sel = 1;
            dma_transmit_async(aes_read, &aes_.aes_out_data, output_data.data(), 0, 1, sizeof(uint32_t), padding_len >> 2, 4, event_read);
            aes_input_bytes(input_data.data(), input_len, AES_CBC);
            configASSERT(xSemaphoreTake(event_read, portMAX_DELAY) == pdTRUE);
            dma_close(aes_read);
            vSemaphoreDelete(event_read);
        }
    }

    virtual void aes_cbc256_hard_decrypt(cbc_context_t &context, gsl::span<const uint8_t> input_data, gsl::span<uint8_t> output_data) override
    {
        COMMON_ENTRY;

        size_t input_len = input_data.size();
        size_t padding_len = ((input_len + 15) / 16) * 16;
        os_aes_init(context.input_key, AES_256, context.iv, IV_LEN_128, NULL, AES_CBC, AES_HARD_DECRYPTION, 0L, input_len);
        if (padding_len <= AES_TRANSMISSION_THRESHOLD)
        {
            os_aes_process(input_data.data(), output_data.data(), padding_len, AES_CBC);
        }
        else
        {
            handle_t aes_read = dma_open_free();
            dma_set_request_source(aes_read, dma_req_);

            SemaphoreHandle_t event_read = xSemaphoreCreateBinary();
            aes_.dma_sel = 1;
            dma_transmit_async(aes_read, &aes_.aes_out_data, output_data.data(), 0, 1, sizeof(uint32_t), padding_len >> 2, 4, event_read);
            aes_input_bytes(input_data.data(), input_len, AES_CBC);
            configASSERT(xSemaphoreTake(event_read, portMAX_DELAY) == pdTRUE);
            dma_close(aes_read);
            vSemaphoreDelete(event_read);
        }
    }

    virtual void aes_cbc256_hard_encrypt(cbc_context_t &context, gsl::span<const uint8_t> input_data, gsl::span<uint8_t> output_data) override
    {
        COMMON_ENTRY;

        size_t input_len = input_data.size();
        size_t padding_len = ((input_len + 15) / 16) * 16;
        os_aes_init(context.input_key, AES_256, context.iv, IV_LEN_128, NULL, AES_CBC, AES_HARD_ENCRYPTION, 0L, input_len);
        if (padding_len <= AES_TRANSMISSION_THRESHOLD)
        {
            os_aes_process(input_data.data(), output_data.data(), input_len, AES_CBC);
        }
        else
        {
            handle_t aes_read = dma_open_free();
            dma_set_request_source(aes_read, dma_req_);

            SemaphoreHandle_t event_read = xSemaphoreCreateBinary();
            aes_.dma_sel = 1;
            dma_transmit_async(aes_read, &aes_.aes_out_data, output_data.data(), 0, 1, sizeof(uint32_t), padding_len >> 2, 4, event_read);
            aes_input_bytes(input_data.data(), input_len, AES_CBC);
            configASSERT(xSemaphoreTake(event_read, portMAX_DELAY) == pdTRUE);
            dma_close(aes_read);
            vSemaphoreDelete(event_read);
        }
    }

    virtual void aes_gcm128_hard_decrypt(gcm_context_t &context, gsl::span<const uint8_t> input_data, gsl::span<uint8_t> output_data, gsl::span<uint8_t> gcm_tag) override
    {
        COMMON_ENTRY;

        size_t input_len = input_data.size();
        os_aes_init(context.input_key, AES_128, context.iv, IV_LEN_96, context.gcm_aad,
            AES_GCM, AES_HARD_DECRYPTION, context.gcm_aad_len, input_len);
        if (input_len <= AES_TRANSMISSION_THRESHOLD)
        {
            os_aes_process(input_data.data(), output_data.data(), input_len, AES_GCM);
        }
        else
        {
            handle_t aes_read = dma_open_free();
            dma_set_request_source(aes_read, dma_req_);

            SemaphoreHandle_t event_read = xSemaphoreCreateBinary();
            aes_.dma_sel = 1;
            dma_transmit_async(aes_read, &aes_.aes_out_data, output_data.data(), 0, 1, sizeof(uint32_t), (input_len + 3) >> 2, 4, event_read);
            aes_input_bytes(input_data.data(), input_len, AES_GCM);
            configASSERT(xSemaphoreTake(event_read, portMAX_DELAY) == pdTRUE);
            dma_close(aes_read);
            vSemaphoreDelete(event_read);
        }

        os_gcm_get_tag(gcm_tag.data());
    }

    virtual void aes_gcm128_hard_encrypt(gcm_context_t &context, gsl::span<const uint8_t> input_data, gsl::span<uint8_t> output_data, gsl::span<uint8_t> gcm_tag) override
    {
        COMMON_ENTRY;

        size_t input_len = input_data.size();
        os_aes_init(context.input_key, AES_128, context.iv, IV_LEN_96, context.gcm_aad,
            AES_GCM, AES_HARD_ENCRYPTION, context.gcm_aad_len, input_len);
        if (input_len <= AES_TRANSMISSION_THRESHOLD)
        {
            os_aes_process(input_data.data(), output_data.data(), input_len, AES_GCM);
        }
        else
        {
            handle_t aes_read = dma_open_free();
            dma_set_request_source(aes_read, dma_req_);

            SemaphoreHandle_t event_read = xSemaphoreCreateBinary();
            aes_.dma_sel = 1;
            dma_transmit_async(aes_read, &aes_.aes_out_data, output_data.data(), 0, 1, sizeof(uint32_t), (input_len + 3) >> 2, 4, event_read);
            aes_input_bytes(input_data.data(), input_len, AES_GCM);
            configASSERT(xSemaphoreTake(event_read, portMAX_DELAY) == pdTRUE);
            dma_close(aes_read);
            vSemaphoreDelete(event_read);
        }

        os_gcm_get_tag(gcm_tag.data());
    }

    virtual void aes_gcm192_hard_decrypt(gcm_context_t &context, gsl::span<const uint8_t> input_data, gsl::span<uint8_t> output_data, gsl::span<uint8_t> gcm_tag) override
    {
        COMMON_ENTRY;

        size_t input_len = input_data.size();
        os_aes_init(context.input_key, AES_192, context.iv, IV_LEN_96, context.gcm_aad,
            AES_GCM, AES_HARD_DECRYPTION, context.gcm_aad_len, input_len);
        if (input_len <= AES_TRANSMISSION_THRESHOLD)
        {
            os_aes_process(input_data.data(), output_data.data(), input_len, AES_GCM);
        }
        else
        {
            handle_t aes_read = dma_open_free();
            dma_set_request_source(aes_read, dma_req_);

            SemaphoreHandle_t event_read = xSemaphoreCreateBinary();
            aes_.dma_sel = 1;
            dma_transmit_async(aes_read, &aes_.aes_out_data, output_data.data(), 0, 1, sizeof(uint32_t), (input_len + 3) >> 2, 4, event_read);
            aes_input_bytes(input_data.data(), input_len, AES_GCM);
            configASSERT(xSemaphoreTake(event_read, portMAX_DELAY) == pdTRUE);
            dma_close(aes_read);
            vSemaphoreDelete(event_read);
        }

        os_gcm_get_tag(gcm_tag.data());
    }

    virtual void aes_gcm192_hard_encrypt(gcm_context_t &context, gsl::span<const uint8_t> input_data, gsl::span<uint8_t> output_data, gsl::span<uint8_t> gcm_tag) override
    {
        COMMON_ENTRY;

        size_t input_len = input_data.size();
        os_aes_init(context.input_key, AES_192, context.iv, IV_LEN_96, context.gcm_aad,
            AES_GCM, AES_HARD_ENCRYPTION, context.gcm_aad_len, input_len);
        if (input_len <= AES_TRANSMISSION_THRESHOLD)
        {
            os_aes_process(input_data.data(), output_data.data(), input_len, AES_GCM);
        }
        else
        {
            handle_t aes_read = dma_open_free();
            dma_set_request_source(aes_read, dma_req_);

            SemaphoreHandle_t event_read = xSemaphoreCreateBinary();
            aes_.dma_sel = 1;
            dma_transmit_async(aes_read, &aes_.aes_out_data, output_data.data(), 0, 1, sizeof(uint32_t), (input_len + 3) >> 2, 4, event_read);
            aes_input_bytes(input_data.data(), input_len, AES_GCM);
            configASSERT(xSemaphoreTake(event_read, portMAX_DELAY) == pdTRUE);
            dma_close(aes_read);
            vSemaphoreDelete(event_read);
        }

        os_gcm_get_tag(gcm_tag.data());
    }

    virtual void aes_gcm256_hard_decrypt(gcm_context_t &context, gsl::span<const uint8_t> input_data, gsl::span<uint8_t> output_data, gsl::span<uint8_t> gcm_tag) override
    {
        COMMON_ENTRY;

        size_t input_len = input_data.size();
        os_aes_init(context.input_key, AES_256, context.iv, IV_LEN_96, context.gcm_aad,
            AES_GCM, AES_HARD_DECRYPTION, context.gcm_aad_len, input_len);
        if (input_len <= AES_TRANSMISSION_THRESHOLD)
        {
            os_aes_process(input_data.data(), output_data.data(), input_len, AES_GCM);
        }
        else
        {
            handle_t aes_read = dma_open_free();
            dma_set_request_source(aes_read, dma_req_);

            SemaphoreHandle_t event_read = xSemaphoreCreateBinary();
            aes_.dma_sel = 1;
            dma_transmit_async(aes_read, &aes_.aes_out_data, output_data.data(), 0, 1, sizeof(uint32_t), (input_len + 3) >> 2, 4, event_read);
            aes_input_bytes(input_data.data(), input_len, AES_GCM);
            configASSERT(xSemaphoreTake(event_read, portMAX_DELAY) == pdTRUE);
            dma_close(aes_read);
            vSemaphoreDelete(event_read);
        }

        os_gcm_get_tag(gcm_tag.data());
    }

    virtual void aes_gcm256_hard_encrypt(gcm_context_t &context, gsl::span<const uint8_t> input_data, gsl::span<uint8_t> output_data, gsl::span<uint8_t> gcm_tag) override
    {
        COMMON_ENTRY;

        size_t input_len = input_data.size();
        os_aes_init(context.input_key, AES_256, context.iv, IV_LEN_96, context.gcm_aad,
            AES_GCM, AES_HARD_ENCRYPTION, context.gcm_aad_len, input_len);
        if (input_len <= AES_TRANSMISSION_THRESHOLD)
        {
            os_aes_process(input_data.data(), output_data.data(), input_len, AES_GCM);
        }
        else
        {
            handle_t aes_read = dma_open_free();
            dma_set_request_source(aes_read, dma_req_);

            SemaphoreHandle_t event_read = xSemaphoreCreateBinary();
            aes_.dma_sel = 1;
            dma_transmit_async(aes_read, &aes_.aes_out_data, output_data.data(), 0, 1, sizeof(uint32_t), (input_len + 3) >> 2, 4, event_read);
            aes_input_bytes(input_data.data(), input_len, AES_GCM);
            configASSERT(xSemaphoreTake(event_read, portMAX_DELAY) == pdTRUE);
            dma_close(aes_read);
            vSemaphoreDelete(event_read);
        }

        os_gcm_get_tag(gcm_tag.data());
    }

private:
    void os_aes_init(const uint8_t *input_key,
        size_t input_key_len,
        uint8_t *iv,
        size_t iv_len,
        uint8_t *gcm_aad,
        aes_cipher_mode_t cipher_mode,
        aes_encrypt_sel_t encrypt_sel,
        size_t gcm_aad_len,
        size_t input_data_len)
    {
        sysctl_reset(reset_);
        size_t remainder, uint32_num, uint8_num, i;
        uint32_t uint32_data;
        uint8_t uint8_data[4] = { 0 };
        size_t padding_len = input_data_len;
        if ((cipher_mode == AES_ECB) || (cipher_mode == AES_CBC))
            padding_len = ((input_data_len + 15) / 16) * 16;
        aes_.aes_endian |= 1;
        uint32_num = input_key_len / 4;
        for (i = 0; i < uint32_num; i++)
        {
            if (i < 4)
                aes_.aes_key[i] = *((uint32_t *)(&input_key[input_key_len - (4 * i) - 4]));
            else
                aes_.aes_key_ext[i - 4] = *((uint32_t *)(&input_key[input_key_len - (4 * i) - 4]));
        }

        uint32_num = iv_len / 4;
        for (i = 0; i < uint32_num; i++)
            aes_.aes_iv[i] = *((uint32_t *)(&iv[iv_len - (4 * i) - 4]));

        aes_.mode_ctl.kmode = input_key_len / 8 - 2; /* b'00:AES_128 b'01:AES_192 b'10:AES_256 b'11:RESERVED */
        aes_.mode_ctl.cipher_mode = cipher_mode;
        aes_.encrypt_sel = encrypt_sel;
        aes_.gb_aad_num = gcm_aad_len - 1;
        aes_.gb_pc_num = padding_len - 1;
        aes_.gb_aes_en |= 1;

        if (cipher_mode == AES_GCM)
        {
            uint32_num = gcm_aad_len / 4;
            for (i = 0; i < uint32_num; i++)
            {
                uint32_data = *((uint32_t *)(&gcm_aad[i * 4]));
                while (!os_aes_get_data_in_flag())
                    ;
                os_aes_write_aad(uint32_data);
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
                while (!os_aes_get_data_in_flag())
                    ;
                os_aes_write_aad(uint32_data);
            }
        }
    }

    void aes_input_bytes(const uint8_t *input_data, size_t input_data_len, aes_cipher_mode_t cipher_mode)
    {
        size_t padding_len, uint32_num, uint8_num, remainder, i;
        uint32_t uint32_data;
        uint8_t uint8_data[4] = { 0 };

        padding_len = ((input_data_len + 15) / 16) * 16;
        uint32_num = input_data_len / 4;
        for (i = 0; i < uint32_num; i++)
        {
            uint32_data = *((uint32_t *)(&input_data[i * 4]));
            while (!os_aes_get_data_in_flag())
                ;
            os_aes_write_text(uint32_data);
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
            while (!os_aes_get_data_in_flag())
                ;
            os_aes_write_text(uint32_data);
        }
        if ((cipher_mode == AES_ECB) || (cipher_mode == AES_CBC))
        {
            uint32_num = (padding_len - input_data_len) / 4;
            for (i = 0; i < uint32_num; i++)
            {
                while (!os_aes_get_data_in_flag())
                    ;
                os_aes_write_text(0);
            }
            uint32_num = padding_len / 4;
        }
    }

    void os_aes_process(const uint8_t *input_data,
        uint8_t *output_data,
        size_t input_data_len,
        aes_cipher_mode_t cipher_mode)
    {
        size_t temp_len = 0;
        uint32_t i = 0;

        if (input_data_len >= 80)
        {
            for (i = 0; i < (input_data_len / 80); i++)
                aes_process_less_80_bytes(&input_data[i * 80], &output_data[i * 80], 80, cipher_mode);
        }
        temp_len = input_data_len % 80;
        if (temp_len)
            aes_process_less_80_bytes(&input_data[i * 80], &output_data[i * 80], temp_len, cipher_mode);
    }

    void aes_process_less_80_bytes(const uint8_t *input_data,
        uint8_t *output_data,
        size_t input_data_len,
        aes_cipher_mode_t cipher_mode)
    {
        size_t padding_len, uint32_num, uint8_num, remainder, i;
        uint32_t uint32_data;
        uint8_t uint8_data[4] = { 0 };

        padding_len = ((input_data_len + 15) / 16) * 16;
        uint32_num = input_data_len / 4;
        for (i = 0; i < uint32_num; i++)
        {
            uint32_data = *((uint32_t *)(&input_data[i * 4]));
            while (!os_aes_get_data_in_flag())
                ;
            os_aes_write_text(uint32_data);
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
            while (!os_aes_get_data_in_flag())
                ;
            os_aes_write_text(uint32_data);
        }
        if ((cipher_mode == AES_ECB) || (cipher_mode == AES_CBC))
        {
            uint32_num = (padding_len - input_data_len) / 4;
            for (i = 0; i < uint32_num; i++)
            {
                while (!os_aes_get_data_in_flag())
                    ;
                os_aes_write_text(0);
            }
            uint32_num = padding_len / 4;
        }
        for (i = 0; i < uint32_num; i++)
        {
            while (!os_aes_get_data_out_flag())
                ;
            *((uint32_t *)(&output_data[i * 4])) = os_aes_read_out_data();
        }
        if ((cipher_mode == AES_GCM) && (remainder))
        {
            while (!os_aes_get_data_out_flag())
                ;
            *((uint32_t *)(&uint8_data[0])) = os_aes_read_out_data();
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

    uint32_t os_aes_get_data_in_flag()
    {
        return aes_.data_in_flag;
    }

    void os_aes_write_text(uint32_t text_data)
    {
        aes_.aes_text_data = text_data;
    }

    void os_gcm_write_tag(uint32_t *tag)
    {
        aes_.gcm_in_tag[0] = tag[3];
        aes_.gcm_in_tag[1] = tag[2];
        aes_.gcm_in_tag[2] = tag[1];
        aes_.gcm_in_tag[3] = tag[0];
    }

    uint32_t os_aes_get_data_out_flag()
    {
        return aes_.data_out_flag;
    }

    uint32_t os_gcm_get_tag_in_flag()
    {
        return aes_.tag_in_flag;
    }

    uint32_t os_aes_read_out_data()
    {
        return aes_.aes_out_data;
    }

    uint32_t os_gcm_get_tag_chk()
    {
        return aes_.tag_chk;
    }

    void os_gcm_clear_chk_tag()
    {
        aes_.tag_clear = 0;
    }

    int os_gcm_check_tag(uint32_t *gcm_tag)
    {
        while (!os_gcm_get_tag_in_flag())
            ;
        os_gcm_write_tag(gcm_tag);
        while (!os_gcm_get_tag_chk())
            ;
        if (os_gcm_get_tag_chk() == 0x2)
        {
            os_gcm_clear_chk_tag();
            return 1;
        }
        else
        {
            os_gcm_clear_chk_tag();
            return 0;
        }
    }

    void os_gcm_get_tag(uint8_t *gcm_tag)
    {
        uint32_t uint32_tag;
        uint8_t i = 0;

        uint32_tag = aes_.gcm_out_tag[3];
        gcm_tag[i++] = (uint8_t)((uint32_tag >> 24) & 0xff);
        gcm_tag[i++] = (uint8_t)((uint32_tag >> 16) & 0xff);
        gcm_tag[i++] = (uint8_t)((uint32_tag >> 8) & 0xff);
        gcm_tag[i++] = (uint8_t)((uint32_tag)&0xff);

        uint32_tag = aes_.gcm_out_tag[2];
        gcm_tag[i++] = (uint8_t)((uint32_tag >> 24) & 0xff);
        gcm_tag[i++] = (uint8_t)((uint32_tag >> 16) & 0xff);
        gcm_tag[i++] = (uint8_t)((uint32_tag >> 8) & 0xff);
        gcm_tag[i++] = (uint8_t)((uint32_tag)&0xff);

        uint32_tag = aes_.gcm_out_tag[1];
        gcm_tag[i++] = (uint8_t)((uint32_tag >> 24) & 0xff);
        gcm_tag[i++] = (uint8_t)((uint32_tag >> 16) & 0xff);
        gcm_tag[i++] = (uint8_t)((uint32_tag >> 8) & 0xff);
        gcm_tag[i++] = (uint8_t)((uint32_tag)&0xff);

        uint32_tag = aes_.gcm_out_tag[0];
        gcm_tag[i++] = (uint8_t)((uint32_tag >> 24) & 0xff);
        gcm_tag[i++] = (uint8_t)((uint32_tag >> 16) & 0xff);
        gcm_tag[i++] = (uint8_t)((uint32_tag >> 8) & 0xff);
        gcm_tag[i++] = (uint8_t)((uint32_tag)&0xff);

        os_gcm_check_tag((uint32_t *)gcm_tag);
    }

    void os_aes_write_aad(uint32_t aad_data)
    {
        aes_.aes_aad_data = aad_data;
    }

private:
    volatile aes_t &aes_;
    sysctl_clock_t clock_;
    sysctl_reset_t reset_;
    sysctl_dma_select_t dma_req_;
    SemaphoreHandle_t free_mutex_;
};

static k_aes_driver dev0_driver(AES_BASE_ADDR, SYSCTL_CLOCK_AES, SYSCTL_RESET_AES, SYSCTL_DMA_SELECT_AES_REQ);

driver& g_aes_driver_aes0 = dev0_driver;
