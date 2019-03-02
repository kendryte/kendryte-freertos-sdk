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
#include <hal.h>
#include <kernel/driver_impl.hpp>
#include <kpu.h>
#include <sysctl.h>
#include <math.h>

using namespace sys;

#define COMMON_ENTRY \
    semaphore_lock locker(free_mutex_);

class k_model_context : public heap_object, public free_object_access
{
public:
    k_model_context(uint8_t *buffer)
    {
        uintptr_t base_addr = (uintptr_t)buffer;
        kpu_model_header_t *header = (kpu_model_header_t *)buffer;
        model_buffer_ = buffer;
        layer_headers_ = (kpu_model_layer_header_t *)(base_addr + sizeof(kpu_model_header_t));
        layers_length_ = header->layers_length;
        body_start_ = (uint8_t *)(base_addr + sizeof(kpu_model_header_t) + 8 * header->layers_length);
        storage_ = std::make_unique<uint8_t[]>(header->main_mem_usage);
        main_buffer_ = { storage_.get(), ptrdiff_t(header->main_mem_usage) };
    }

    virtual void on_first_open() override
    {
    }

    virtual void on_last_close() override
    {
    }

    void get(kpu_model_context_t *ctx)
    {
        ctx->body_start = body_start_;
        ctx->model_buffer = model_buffer_;
        ctx->main_buffer = main_buffer_.data();
        ctx->layer_headers = layer_headers_;
        ctx->layers_length = layers_length_;
    }
private:
    uint8_t *model_buffer_;
    kpu_model_layer_header_t *layer_headers_;
    uint8_t *body_start_;
    uint32_t layers_length_;
    gsl::span<uint8_t> main_buffer_;
    std::unique_ptr<uint8_t[]> storage_;
};

class k_kpu_driver : public kpu_driver, public static_object, public free_object_access
{
public:
    k_kpu_driver(uintptr_t base_addr, sysctl_clock_t clock, sysctl_dma_select_t dma_req)
        : kpu_(*reinterpret_cast<volatile kpu_config_t *>(base_addr)), clock_(clock), dma_req_(dma_req)
    {
        completion_event_ = xSemaphoreCreateBinary();
    }

    virtual void install() override
    {
        free_mutex_ = xSemaphoreCreateMutex();
        sysctl_clock_disable(clock_);
    }

    virtual void on_first_open() override
    {
        sysctl_clock_enable(clock_);
    }

    virtual void on_last_close() override
    {
        sysctl_clock_disable(clock_);
    }

    virtual handle_t model_load_from_buffer(uint8_t *buffer) override
    {
        return system_alloc_handle(make_accessor(make_object<k_model_context>(buffer)));
    }

    virtual int run(handle_t context, const uint8_t *src, uint8_t **output, size_t *output_size) override
    {
        COMMON_ENTRY;

        auto model_context = system_handle_to_object(context).as<k_model_context>();
        model_context->get(&ctx_);
        dma_ch_ = dma_open_free();
        ctx_.current_layer = 0;
        ctx_.current_body = ctx_.body_start;
        
        kpu_model_header_t *header = (kpu_model_header_t *)ctx_.model_buffer;
        kpu_.interrupt_clear.reg = 7;

        kpu_.fifo_threshold.data.fifo_full_threshold = 10;
        kpu_.fifo_threshold.data.fifo_empty_threshold = 1;
        kpu_.fifo_threshold.data.reserved = 0;

        kpu_.eight_bit_mode.data.eight_bit_mode = header->flags & 1;
        kpu_.eight_bit_mode.data.reserved = 0;

        kpu_.interrupt_mask.data.calc_done_int = 1;
        kpu_.interrupt_mask.data.layer_cfg_almost_empty_int = 0;
        kpu_.interrupt_mask.data.layer_cfg_almost_full_int = 1;
        kpu_.interrupt_mask.data.reserved = 0;

        pic_set_irq_priority(IRQN_AI_INTERRUPT, 1);
        pic_set_irq_handler(IRQN_AI_INTERRUPT, kpu_isr_handle, this);
        pic_set_irq_enable(IRQN_AI_INTERRUPT, 1);

        kpu_model_layer_header_t *first_layer_header = ctx_.layer_headers;
        if (first_layer_header->type != KL_K210_CONV)
            return -1;
        kpu_model_conv_layer_argument_t *first_layer = (kpu_model_conv_layer_argument_t *)ctx_.body_start;
        kpu_layer_argument_t *layer_arg = (kpu_layer_argument_t *)(ctx_.model_buffer + first_layer->layer_offset);

        if ((layer_arg->image_size.data.i_row_wid + 1) % 64 != 0)
        {
            kpu_input_with_padding(layer_arg, src);
            ai_step_not_isr();
        }
        else
        {
            kpu_input_dma(layer_arg, src);
        }
        while (!done_flag_)
        {
            if(xSemaphoreTake(completion_event_, portMAX_DELAY) == pdTRUE)
            {
                if (ctx_.current_layer != ctx_.layers_length)
                {
                    while(ai_step() == 1)
                        ;
                }
                else
                {
                    kpu_done();
                }
            }
        }
        *output = output_address_;
        *output_size = output_size_;
        return 0;
    }

private:
    static void kpu_isr_handle(void *userdata)
    {
        auto &driver = *reinterpret_cast<k_kpu_driver *>(userdata);

        driver.kpu_.interrupt_clear.data.calc_done_int = 1;
        driver.kpu_.interrupt_clear.data.layer_cfg_almost_empty_int = 1;
        driver.kpu_.interrupt_clear.data.layer_cfg_almost_full_int = 1;
        driver.kpu_.interrupt_clear.data.reserved = 0;

        driver.kpu_.interrupt_mask.data.calc_done_int = 1;
        driver.kpu_.interrupt_mask.data.layer_cfg_almost_empty_int = 1;
        driver.kpu_.interrupt_mask.data.layer_cfg_almost_full_int = 1;
        driver.kpu_.interrupt_mask.data.reserved = 0;

        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xSemaphoreGiveFromISR(driver.completion_event_, &xHigherPriorityTaskWoken);
        if (xHigherPriorityTaskWoken)
        {
            portYIELD_FROM_ISR();
        }
    }

    int ai_step()
    {
        uint32_t cnt_layer_id = ctx_.current_layer++;
        uint8_t *layer_body = ctx_.current_body;
        kpu_model_layer_header_t *cnt_layer_header = ctx_.layer_headers + cnt_layer_id;
        ctx_.current_body += cnt_layer_header->body_size;

        switch (cnt_layer_header->type)
        {
            case KL_GLOBAL_AVERAGE_POOL2D:
                kpu_global_average_pool2d((kpu_model_gap2d_layer_argument_t *)layer_body);
                break;
            case KL_QUANTIZE:
                kpu_quantize((kpu_model_quantize_layer_argument_t *)layer_body);
                break;
            case KL_DEQUANTIZE:
                kpu_dequantize((kpu_model_dequantize_layer_argument_t *)layer_body);
                break;
            case KL_L2_NORMALIZATION:
                kpu_l2_normalization((kpu_model_l2_norm_layer_argument_t *)layer_body);
                break;
            case KL_K210_CONV:
                kpu_conv((kpu_model_conv_layer_argument_t *)layer_body);
                return 0;
            case KL_K210_ADD_PADDING:
                kpu_add_padding((kpu_model_add_padding_layer_argument_t *)layer_body);
                break;
            case KL_K210_REMOVE_PADDING:
                kpu_remove_padding((kpu_model_remove_padding_layer_argument_t *)layer_body);
                break;
            default:
                configASSERT("Layer is not supported.");
        }

        if (cnt_layer_id != (ctx_.layers_length - 1))
        {
            return 1;
        }
        else
        {
            kpu_done();
            return 0;
        }
    }

    void kpu_input_with_padding(kpu_layer_argument_t *layer, const uint8_t *src)
    {
        size_t width = layer->image_size.data.i_row_wid + 1;
        size_t height = layer->image_size.data.i_col_high + 1;
        size_t channels = layer->image_channel_num.data.i_ch_num + 1;
        uint8_t *dest = (uint8_t *)(uintptr_t)(AI_IO_BASE_ADDR + layer->image_addr.data.image_src_addr * 64);
        size_t oc, y, x;

        uint32_t row_padding;
        uint32_t row_group;
        uint32_t row_length;

        if (width <= 16)
        {
            row_padding = 16;
            row_group = 4;
            row_length = 1;
        }
        else if (width <= 32)
        {
            row_padding = 32;
            row_group = 2;
            row_length = 1;
        }
        else
        {
            row_padding = 64;
            row_group = 1;
            row_length = (width + 63) / 64;
        }

        for (oc = 0; oc < channels; oc++)
        {
            uint8_t *channel_origin = dest + oc / row_group * row_length * height * 64 + oc % row_group * row_padding;
            for (y = 0; y < height; y++)
            {
            uint8_t *y_origin = channel_origin + y * row_length * 64;
            for (x = 0; x < width; x++)
            y_origin[x] = *src++;
            }
        }
    }

    void ai_step_not_isr()
    {
        portENTER_CRITICAL();
        ai_step();
        vPortExitCritical();
    }

    void kpu_input_dma(kpu_layer_argument_t *layer, const uint8_t *src)
    {
        uint64_t input_size = layer->kernel_calc_type_cfg.data.channel_switch_addr * 64 * (layer->image_channel_num.data.i_ch_num + 1);

        dma_set_request_source(dma_ch_, dma_req_);
        dma_transmit_async(dma_ch_, src, (void *)(uintptr_t)((uint8_t *)AI_IO_BASE_ADDR + layer->image_addr.data.image_src_addr * 64), 1, 1, sizeof(uint64_t), input_size / 8, 16, completion_event_);
    }

    int kpu_done()
    {
        kpu_.interrupt_clear.data.calc_done_int = 1;
        kpu_.interrupt_clear.data.layer_cfg_almost_empty_int = 1;
        kpu_.interrupt_clear.data.layer_cfg_almost_full_int = 1;
        kpu_.interrupt_clear.data.reserved = 0;

        kpu_.interrupt_mask.data.calc_done_int = 1;
        kpu_.interrupt_mask.data.layer_cfg_almost_empty_int = 1;
        kpu_.interrupt_mask.data.layer_cfg_almost_full_int = 1;
        kpu_.interrupt_mask.data.reserved = 0;

        kpu_model_header_t *header = (kpu_model_header_t *)ctx_.model_buffer;
        output_address_ = ctx_.main_buffer + header->output_address;
        output_size_ = header->output_size;
        done_flag_ = 1;
        return 0;
    }

    void kpu_conv(kpu_model_conv_layer_argument_t *arg)
    {
        kpu_layer_argument_t *layer = (kpu_layer_argument_t *)(ctx_.model_buffer + arg->layer_offset);
        layer->kernel_load_cfg.data.para_start_addr = (uintptr_t)(ctx_.model_buffer + arg->weights_offset);
        layer->kernel_pool_type_cfg.data.bwsx_base_addr = (uintptr_t)(ctx_.model_buffer + arg->bn_offset);
        layer->kernel_calc_type_cfg.data.active_addr = (uintptr_t)(ctx_.model_buffer + arg->act_offset);

        if (arg->flags & KLF_MAIN_MEM_OUT)
        {
            uint8_t *dest = ctx_.main_buffer + arg->main_mem_out_address;

            layer->dma_parameter.data.send_data_out = 1;

            dma_set_request_source(dma_ch_, dma_req_);
            dma_transmit_async(dma_ch_, (void *)(&kpu_.fifo_data_out), dest, 0, 1, sizeof(uint64_t), (layer->dma_parameter.data.dma_total_byte + 8) / 8, 8, completion_event_);
            kpu_send_layer(layer);
        }
        else
        {
            kpu_send_layer(layer);
            kpu_.interrupt_mask.data.calc_done_int = 1;
            kpu_.interrupt_mask.data.layer_cfg_almost_empty_int = 0;
            kpu_.interrupt_mask.data.layer_cfg_almost_full_int = 1;
            kpu_.interrupt_mask.data.reserved = 0;
            
        }
    }

    void kpu_send_layer(const kpu_layer_argument_t *layer)
    {
        kpu_.layer_argument_fifo = layer->interrupt_enabe.reg;
        kpu_.layer_argument_fifo = layer->image_addr.reg;
        kpu_.layer_argument_fifo = layer->image_channel_num.reg;
        kpu_.layer_argument_fifo = layer->image_size.reg;
        kpu_.layer_argument_fifo = layer->kernel_pool_type_cfg.reg;
        kpu_.layer_argument_fifo = layer->kernel_load_cfg.reg;
        kpu_.layer_argument_fifo = layer->kernel_offset.reg;
        kpu_.layer_argument_fifo = layer->kernel_calc_type_cfg.reg;
        kpu_.layer_argument_fifo = layer->write_back_cfg.reg;
        kpu_.layer_argument_fifo = layer->conv_value.reg;
        kpu_.layer_argument_fifo = layer->conv_value2.reg;
        kpu_.layer_argument_fifo = layer->dma_parameter.reg;
    }

    void kpu_global_average_pool2d(kpu_model_gap2d_layer_argument_t *arg)
    {
        const float *src = (const float *)(ctx_.main_buffer + arg->main_mem_in_address);
        float *dest = (float *)(ctx_.main_buffer + arg->main_mem_out_address);
        size_t oc, channels = arg->channels, kernel_size = arg->kernel_size;
        
        for (oc = 0; oc < channels; oc++)
        {
            float sum = 0.f;
            size_t i;
            for (i = 0; i < kernel_size; i++)
                sum += *src++;

            dest[oc] = sum / kernel_size;
        }
    }

    void kpu_quantize(kpu_model_quantize_layer_argument_t *arg)
    {
        size_t width = arg->width;
        size_t height = arg->height;
        size_t channels = arg->channels;
        const float *src = (const float *)(ctx_.main_buffer + arg->main_mem_in_address);;
        const kpu_model_quant_param_t q = arg->quant_param;

        if (arg->flags & KLF_MAIN_MEM_OUT)
        {
            uint8_t *dest = (uint8_t *)(ctx_.main_buffer + arg->mem_out_address);
            size_t i, count = width * height * channels;
            for (i = 0; i < count; i++)
            {
                int value = (*src++ - q.bias) / q.scale;
                if (value < 0) value = 0;
                if (value > 0xFF) value = 0xFF;
                *dest++ = (uint8_t)value;
            }
        }
        else
        {
            uint8_t *dest = (uint8_t *)AI_IO_BASE_ADDR + arg->mem_out_address * 64;
            size_t oc, y, x;

            uint32_t row_padding;
            uint32_t row_group;
            uint32_t row_length;

            if (width <= 16)
            {
                row_padding = 16;
                row_group = 4;
                row_length = 1;
            }
            else if (width <= 32)
            {
                row_padding = 32;
                row_group = 2;
                row_length = 1;
            }
            else
            {
                row_padding = 64;
                row_group = 1;
                row_length = (width + 63) / 64;
            }

            for (oc = 0; oc < channels; oc++)
            {
                uint8_t *channel_origin = dest + oc / row_group * row_length * height * 64 + oc % row_group * row_padding;
                for (y = 0; y < height; y++)
                {
                    uint8_t *y_origin = channel_origin + y * row_length * 64;
                    for (x = 0; x < width; x++)
                    {
                        int value = (*src++ - q.bias) / q.scale;
                        if (value < 0) value = 0;
                        if (value > 0xFF) value = 0xFF;
                        y_origin[x] = (uint8_t)value;
                    }
                }
            }
        }
    }

    void kpu_dequantize(kpu_model_dequantize_layer_argument_t *arg)
    {
        const uint8_t *src = (const uint8_t *)(ctx_.main_buffer + arg->main_mem_in_address);
        float *dest = (float *)(ctx_.main_buffer + arg->main_mem_out_address);
        size_t oc, count = arg->count;
        const kpu_model_quant_param_t q = arg->quant_param;
        
        for (oc = 0; oc < count; oc++)
            dest[oc] = *src++ * q.scale + q.bias;
    }

    void kpu_l2_normalization(kpu_model_l2_norm_layer_argument_t *arg)
    {
        const float *src = (const float *)(ctx_.main_buffer + arg->main_mem_in_address);
        float *dest = (float *)(ctx_.main_buffer + arg->main_mem_out_address);
        size_t oc, channels = arg->channels;

        float sum = 0.f;
        const float epsilon = 1e-10f;
        for (oc = 0; oc < channels; oc++)
            sum += src[oc] * src[oc];
        if (sum < epsilon)
            sum = epsilon;
        sum = 1.f / sqrtf(sum);
        for (oc = 0; oc < channels; oc++)
            dest[oc] = src[oc] * sum;
    }

    void kpu_add_padding(kpu_model_add_padding_layer_argument_t *arg)
    {
        const uint8_t *src = (const uint8_t *)(ctx_.main_buffer + arg->main_mem_in_address);
        uint8_t *dest = (uint8_t *)AI_IO_BASE_ADDR + arg->kpu_mem_out_address * 64L;

        uint32_t row_padding = 16;
        uint32_t row_group = 4;
        uint32_t row_length = 1;
        uint32_t height = 4;
        uint32_t oc, x, y, channels = arg->channels;

        for (oc = 0; oc < channels; oc++)
        {
            uint8_t *channel_origin = dest + oc / row_group * row_length * height * 64 + oc % row_group * row_padding;
            for (y = 0; y < 1; y++)
            {
                uint8_t *y_origin = channel_origin + y * row_length * 64;
                for (x = 0; x < 1; x++)
                    y_origin[x] = *src++;
            }
        }
    }

    void kpu_remove_padding(kpu_model_remove_padding_layer_argument_t *arg)
    {
        const uint8_t *src = (const uint8_t *)(ctx_.main_buffer + arg->main_mem_in_address);
        uint8_t *dest = (uint8_t *)(ctx_.main_buffer + arg->main_mem_out_address);
        uint32_t oc, channels = arg->channels;

        for (oc = 0; oc < channels; oc++)
            *dest++ = src[oc * 16];
    }

private:
    volatile kpu_config_t &kpu_;
    sysctl_clock_t clock_;
    sysctl_dma_select_t dma_req_;
    SemaphoreHandle_t free_mutex_;
    uintptr_t dma_ch_;
    SemaphoreHandle_t completion_event_;
    uint8_t done_flag_ = 0;
    uint8_t *output_address_;
    size_t output_size_;
    kpu_model_context_t ctx_;
};

static k_kpu_driver dev0_driver(AI_BASE_ADDR, SYSCTL_CLOCK_AI, SYSCTL_DMA_SELECT_AI_RX_REQ);

driver &g_kpu_driver_kpu0 = dev0_driver;
