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
#include <fpioa.h>
#include <hal.h>
#include <i2s.h>
#include <kernel/driver_impl.hpp>
#include <math.h>
#include <semphr.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysctl.h>
#include <utility.h>

using namespace sys;

#define I2S_DMA_BLOCK_TIME          1000UL

#define BUFFER_COUNT 2
#define COMMON_ENTRY                                         \
    i2s_data *data = (i2s_data *)userdata;                   \
    volatile i2s_t *i2s = (volatile i2s_t *)data->base_addr; \
    (void)i2s;

typedef enum
{
    I2S_RECEIVE,
    I2S_SEND
} i2s_transmit;

class k_i2s_driver : public i2s_driver, public static_object, public exclusive_object_access
{
public:
    k_i2s_driver(uintptr_t base_addr, sysctl_clock_t clock, sysctl_threshold_t threshold, sysctl_dma_select_t dma_req)
        : i2s_(*reinterpret_cast<volatile i2s_t *>(base_addr)), clock_(clock), threshold_(threshold), dma_req_(dma_req)
    {
    }

    virtual void install() override
    {
        sysctl_clock_disable(clock_);
    }

    virtual void on_first_open() override
    {
        sysctl_clock_enable(clock_);

        ier_t u_ier;

        u_ier.reg_data = readl(&i2s_.ier);
        u_ier.ier.ien = 1;
        writel(u_ier.reg_data, &i2s_.ier);
    }

    virtual void on_last_close() override
    {
        sysctl_clock_disable(clock_);
    }

    virtual void config_as_render(const audio_format_t &format, size_t delay_ms, i2s_align_mode_t align_mode, uint32_t channels_mask) override
    {
        session_.transmit = I2S_SEND;

        uint32_t am = 0;

        switch (align_mode)
        {
        case I2S_AM_STANDARD:
            am = 0x1;
            break;
        case I2S_AM_RIGHT:
            am = 0x2;
            break;
        case I2S_AM_LEFT:
            am = 0x4;
            break;
        default:
            configASSERT(!"I2S align mode not supported.");
            break;
        }

        uint32_t threshold;
        i2s_word_select_cycles_t wsc;
        i2s_word_length_t wlen;
        size_t block_align;
        uint32_t dma_divide16;

        extract_params(format, threshold, wsc, wlen, block_align, dma_divide16);

        sysctl_clock_set_threshold(threshold_, threshold);

        i2s_transmit_set_enable(I2S_RECEIVE, 0);
        i2s_transmit_set_enable(I2S_SEND, 0);

        ccr_t u_ccr;
        cer_t u_cer;

        u_cer.reg_data = readl(&i2s_.cer);
        u_cer.cer.clken = 0;
        writel(u_cer.reg_data, &i2s_.cer);

        u_ccr.reg_data = readl(&i2s_.ccr);
        u_ccr.ccr.clk_word_size = wsc;
        u_ccr.ccr.clk_gate = NO_CLOCK_GATING;
        u_ccr.ccr.align_mode = am;
        u_ccr.ccr.dma_tx_en = 1;
        u_ccr.ccr.sign_expand_en = 1;
        u_ccr.ccr.dma_divide_16 = dma_divide16;
        u_ccr.ccr.dma_rx_en = 0;
        writel(u_ccr.reg_data, &i2s_.ccr);

        u_cer.reg_data = readl(&i2s_.cer);
        u_cer.cer.clken = 1;
        writel(u_cer.reg_data, &i2s_.cer);

        writel(1, &i2s_.txffr);
        writel(1, &i2s_.rxffr);

        size_t channel = 0;
        size_t enabled_channel = 0;
        for (channel = 0; channel < 4; channel++)
        {
            auto &i2sc = i2s_.channel[channel];

            if ((channels_mask & 3) == 3)
            {
                i2sc_transmit_set_enable(I2S_SEND, 1, i2sc);
                i2sc_transmit_set_enable(I2S_RECEIVE, 0, i2sc);
                i2sc_set_mask_interrupt(i2sc, 0, 0, 1, 1);

                rcr_tcr_t u_tcr;
                u_tcr.reg_data = readl(&i2sc.tcr);
                u_tcr.rcr_tcr.wlen = wlen;
                writel(u_tcr.reg_data, &i2sc.tcr);

                i2s_set_threshold(i2sc, I2S_SEND, TRIGGER_LEVEL_4);
                enabled_channel++;
            }
            else
            {
                i2sc_transmit_set_enable(I2S_SEND, 0, i2sc);
                i2sc_transmit_set_enable(I2S_RECEIVE, 0, i2sc);
            }

            channels_mask >>= 2;
        }

        configASSERT(enabled_channel * 2 == format.channels);

        session_.channels = format.channels;
        session_.block_align = block_align;
        session_.buffer_frames = format.sample_rate * delay_ms / 1000;
        configASSERT(session_.buffer_frames >= 100);
        free(session_.buffer);
        session_.buffer_size = session_.block_align * session_.buffer_frames;
        session_.buffer = (uint8_t *)malloc(session_.buffer_size * BUFFER_COUNT);
        memset(session_.buffer, 0, session_.buffer_size * BUFFER_COUNT);
        session_.buffer_ptr = 0;
        session_.next_free_buffer = 0;
        session_.stop_signal = 0;
        session_.transmit_dma = NULL_HANDLE;
        session_.dma_in_use_buffer = 0;
        session_.use_low_16bits = false;
    }

    virtual void config_as_capture(const audio_format_t &format, size_t delay_ms, i2s_align_mode_t align_mode, uint32_t channels_mask) override
    {
        session_.transmit = I2S_RECEIVE;

        uint32_t am = 0;

        switch (align_mode)
        {
        case I2S_AM_STANDARD:
            am = 0x1;
            break;
        case I2S_AM_RIGHT:
            am = 0x2;
            break;
        case I2S_AM_LEFT:
            am = 0x4;
            break;
        default:
            configASSERT(!"I2S align mode not supported.");
            break;
        }

        uint32_t threshold;
        i2s_word_select_cycles_t wsc;
        i2s_word_length_t wlen;
        size_t block_align;
        uint32_t dma_divide16;

        extract_params(format, threshold, wsc, wlen, block_align, dma_divide16);

        sysctl_clock_set_threshold(threshold_, threshold);

        i2s_transmit_set_enable(I2S_RECEIVE, 0);
        i2s_transmit_set_enable(I2S_SEND, 0);

        ccr_t u_ccr;
        cer_t u_cer;

        u_cer.reg_data = readl(&i2s_.cer);
        u_cer.cer.clken = 0;
        writel(u_cer.reg_data, &i2s_.cer);

        u_ccr.reg_data = readl(&i2s_.ccr);
        u_ccr.ccr.clk_word_size = wsc;
        u_ccr.ccr.clk_gate = NO_CLOCK_GATING;
        u_ccr.ccr.align_mode = am;
        u_ccr.ccr.dma_tx_en = 0;
        u_ccr.ccr.sign_expand_en = 1;
        u_ccr.ccr.dma_divide_16 = 0;
        u_ccr.ccr.dma_rx_en = 1;
        writel(u_ccr.reg_data, &i2s_.ccr);

        u_cer.reg_data = readl(&i2s_.cer);
        u_cer.cer.clken = 1;
        writel(u_cer.reg_data, &i2s_.cer);

        writel(1, &i2s_.txffr);
        writel(1, &i2s_.rxffr);

        size_t channel = 0;
        size_t enabled_channel = 0;
        for (channel = 0; channel < 4; channel++)
        {
            auto &i2sc = i2s_.channel[channel];

            if ((channels_mask & 3) == 3)
            {
                i2sc_transmit_set_enable(I2S_SEND, 0, i2sc);
                i2sc_transmit_set_enable(I2S_RECEIVE, 1, i2sc);
                i2sc_set_mask_interrupt(i2sc, 1, 1, 0, 0);

                /* set word length */
                rcr_tcr_t u_tcr;
                u_tcr.reg_data = readl(&i2sc.rcr);
                u_tcr.rcr_tcr.wlen = wlen;
                writel(u_tcr.reg_data, &i2sc.rcr);

                i2s_set_threshold(i2sc, I2S_RECEIVE, TRIGGER_LEVEL_4);
                enabled_channel++;
            }
            else
            {
                i2sc_transmit_set_enable(I2S_SEND, 0, i2sc);
                i2sc_transmit_set_enable(I2S_RECEIVE, 0, i2sc);
            }

            channels_mask >>= 2;
        }

        configASSERT(enabled_channel * 2 == format.channels);

        session_.channels = format.channels;
        session_.block_align = block_align;
        session_.buffer_frames = format.sample_rate * delay_ms / 1000;
        configASSERT(session_.buffer_frames >= 100);
        free(session_.buffer);
        session_.buffer_size = session_.block_align * session_.buffer_frames;
        session_.buffer = (uint8_t *)malloc(session_.buffer_size * BUFFER_COUNT);
        memset(session_.buffer, 0, session_.buffer_size * BUFFER_COUNT);
        session_.buffer_ptr = 0;
        session_.next_free_buffer = 0;
        session_.stop_signal = 0;
        session_.transmit_dma = NULL_HANDLE;
        session_.dma_in_use_buffer = 0;
        session_.use_low_16bits = format.bits_per_sample == 16;
        free(session_.buffer_16to32);
        if (session_.use_low_16bits)
            session_.buffer_16to32 = (uint8_t *)malloc(session_.buffer_size * 2 * BUFFER_COUNT);
    }

    virtual void get_buffer(gsl::span<uint8_t> &buffer, size_t &frames) override
    {
        int next_free_buffer = session_.next_free_buffer;
        while (next_free_buffer == session_.dma_in_use_buffer)
        {
            xSemaphoreTake(session_.stage_completion_event, portMAX_DELAY);
            next_free_buffer = session_.next_free_buffer;
        }

        frames = (session_.buffer_size - session_.buffer_ptr) / session_.block_align;
        buffer = { session_.buffer + session_.buffer_size * next_free_buffer + session_.buffer_ptr, std::ptrdiff_t(frames * session_.block_align) };
    }

    virtual void release_buffer(uint32_t frames) override
    {
        session_.buffer_ptr += frames * session_.block_align;
        if (session_.buffer_ptr >= session_.buffer_size)
        {
            session_.buffer_ptr = 0;
            int next_free_buffer = session_.next_free_buffer + 1;
            if (next_free_buffer == BUFFER_COUNT)
                next_free_buffer = 0;
            session_.next_free_buffer = next_free_buffer;
        }
    }

    virtual void start() override
    {
        if (session_.transmit == I2S_SEND)
        {
            configASSERT(!session_.transmit_dma);

            session_.stop_signal = 0;
            session_.transmit_dma = dma_open_free();
            dma_set_request_source(session_.transmit_dma, dma_req_ - 1);
            session_.dma_in_use_buffer = 0;
            session_.stage_completion_event = xSemaphoreCreateCounting(100, 0);
            session_.completion_event = xSemaphoreCreateBinary();

            const volatile void *srcs[BUFFER_COUNT] = {
                session_.buffer,
                session_.buffer + session_.buffer_size
            };
            volatile void *dests[1] = {
                &i2s_.txdma
            };

            dma_loop_async(session_.transmit_dma, srcs, BUFFER_COUNT, dests, 1, 1, 0, sizeof(uint32_t), session_.buffer_size >> 2, 1, i2s_stage_completion_isr, this, session_.completion_event, &session_.stop_signal);
        }
        else
        {
            configASSERT(!session_.transmit_dma);

            session_.stop_signal = 0;
            session_.transmit_dma = dma_open_free();
            dma_set_request_source(session_.transmit_dma, dma_req_);
            session_.dma_in_use_buffer = 0;
            session_.stage_completion_event = xSemaphoreCreateCounting(100, 0);
            session_.completion_event = xSemaphoreCreateBinary();

            const volatile void *srcs[1] = {
                &i2s_.rxdma
            };

            if (session_.buffer_16to32)
            {
                volatile void *dests[BUFFER_COUNT] = {
                    session_.buffer_16to32,
                    session_.buffer_16to32 + session_.buffer_size * 2
                };

                dma_loop_async(session_.transmit_dma, srcs, 1, dests, BUFFER_COUNT, 0, 1, sizeof(uint32_t), session_.buffer_size * 2 >> 2, 4, i2s_stage_completion_isr, this, session_.completion_event, &session_.stop_signal);
            }
            else
            {
                volatile void *dests[BUFFER_COUNT] = {
                    session_.buffer,
                    session_.buffer + session_.buffer_size
                };

                dma_loop_async(session_.transmit_dma, srcs, 1, dests, BUFFER_COUNT, 0, 1, sizeof(uint32_t), session_.buffer_size >> 2, 4, i2s_stage_completion_isr, this, session_.completion_event, &session_.stop_signal);
            }
        }

        i2s_transmit_set_enable(session_.transmit, 1);
    }

    virtual void stop() override
    {
        dma_stop(session_.transmit_dma);
        configASSERT(pdTRUE == xSemaphoreTake(session_.completion_event, I2S_DMA_BLOCK_TIME));
        dma_close(session_.transmit_dma);
        i2s_transmit_set_enable(session_.transmit, 0);
    }

private:
    void i2s_set_threshold(volatile i2s_channel_t &i2sc, i2s_transmit transmit, i2s_fifo_threshold_t threshold)
    {
        if (transmit == I2S_RECEIVE)
        {
            rfcr_t u_rfcr;

            u_rfcr.reg_data = readl(&i2sc.rfcr);
            u_rfcr.rfcr.rxchdt = threshold;
            writel(u_rfcr.reg_data, &i2sc.rfcr);
        }
        else
        {
            tfcr_t u_tfcr;

            u_tfcr.reg_data = readl(&i2sc.tfcr);
            u_tfcr.tfcr.txchet = threshold;
            writel(u_tfcr.reg_data, &i2sc.tfcr);
        }
    }

    void i2sc_set_mask_interrupt(volatile i2s_channel_t &i2sc,
        uint32_t rx_available_int, uint32_t rx_overrun_int,
        uint32_t tx_empty_int, uint32_t tx_overrun_int)
    {
        imr_t u_imr;

        u_imr.reg_data = readl(&i2sc.imr);

        if (rx_available_int == 1)
            u_imr.imr.rxdam = 1;
        else
            u_imr.imr.rxdam = 0;
        if (rx_overrun_int == 1)
            u_imr.imr.rxfom = 1;
        else
            u_imr.imr.rxfom = 0;

        if (tx_empty_int == 1)
            u_imr.imr.txfem = 1;
        else
            u_imr.imr.txfem = 0;
        if (tx_overrun_int == 1)
            u_imr.imr.txfom = 1;
        else
            u_imr.imr.txfom = 0;
        writel(u_imr.reg_data, &i2sc.imr);
    }

    static void extract_params(const audio_format_t &format, uint32_t &threshold, i2s_word_select_cycles_t &wsc,
        i2s_word_length_t &wlen, size_t &block_align, uint32_t &dma_divide16)
    {
        uint32_t pll2_clock = 0;
        pll2_clock = sysctl_pll_get_freq(SYSCTL_PLL2);
        configASSERT((format.sample_rate > pll2_clock / (1 << 23)) && (format.sample_rate < pll2_clock / (1 << 7)));
        switch (format.bits_per_sample)
        {
        case 16:
            wsc = SCLK_CYCLES_32;
            wlen = RESOLUTION_16_BIT;
            block_align = format.channels * 2;
            dma_divide16 = 1;
            break;
        case 24:
            wsc = SCLK_CYCLES_32;
            wlen = RESOLUTION_24_BIT;
            block_align = format.channels * 4;
            dma_divide16 = 0;
            break;
        case 32:
            wsc = SCLK_CYCLES_32;
            wlen = RESOLUTION_32_BIT;
            block_align = format.channels * 4;
            dma_divide16 = 0;
            break;
        default:
            configASSERT(!"Invlid bits per sample");
            break;
        }

        threshold = round(pll2_clock / (format.sample_rate * 128.0) - 1);
    }

    void i2s_transmit_set_enable(i2s_transmit transmit, int enable)
    {
        irer_t u_irer;
        iter_t u_iter;

        if (transmit == I2S_RECEIVE)
        {
            u_irer.reg_data = readl(&i2s_.irer);
            u_irer.irer.rxen = enable;
            writel(u_irer.reg_data, &i2s_.irer);
        }
        else
        {
            u_iter.reg_data = readl(&i2s_.iter);
            u_iter.iter.txen = enable;
            writel(u_iter.reg_data, &i2s_.iter);
        }
    }

    void i2sc_transmit_set_enable(i2s_transmit transmit, int enable, volatile i2s_channel_t &i2sc)
    {
        rer_t u_rer;
        ter_t u_ter;

        if (transmit == I2S_SEND)
        {
            u_ter.reg_data = readl(&i2sc.ter);
            u_ter.ter.txchenx = enable;
            writel(u_ter.reg_data, &i2sc.ter);
        }
        else
        {
            u_rer.reg_data = readl(&i2sc.rer);
            u_rer.rer.rxchenx = enable;
            writel(u_rer.reg_data, &i2sc.rer);
        }
    }

    static void i2s_stage_completion_isr(void *userdata)
    {
        auto &driver = *reinterpret_cast<k_i2s_driver *>(userdata);

        int dma_in_use_buffer = driver.session_.dma_in_use_buffer;
        if (driver.session_.buffer_16to32)
        {
            const uint32_t *src = (uint32_t *)(driver.session_.buffer_16to32 + dma_in_use_buffer * driver.session_.buffer_size * 2);
            uint16_t *dest = (uint16_t *)(driver.session_.buffer + dma_in_use_buffer * driver.session_.buffer_size);
            size_t count = driver.session_.buffer_size / sizeof(uint16_t);
            size_t i;
            for (i = 0; i < count; i++)
                *dest++ = (uint16_t)*src++;
        }

        dma_in_use_buffer++;
        if (dma_in_use_buffer == BUFFER_COUNT)
            dma_in_use_buffer = 0;
        driver.session_.dma_in_use_buffer = dma_in_use_buffer;

        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xSemaphoreGiveFromISR(driver.session_.stage_completion_event, &xHigherPriorityTaskWoken);
        if (xHigherPriorityTaskWoken)
            portYIELD_FROM_ISR();
    }

private:
    volatile i2s_t &i2s_;
    sysctl_clock_t clock_;
    sysctl_threshold_t threshold_;
    sysctl_dma_select_t dma_req_;

    struct
    {
        i2s_transmit transmit;
        uint8_t *buffer;
        uint8_t *buffer_16to32;
        size_t buffer_frames;
        size_t buffer_size;
        size_t block_align;
        size_t channels;
        bool use_low_16bits;
        size_t buffer_ptr;
        volatile int next_free_buffer;
        volatile int dma_in_use_buffer;
        int stop_signal;
        handle_t transmit_dma;
        SemaphoreHandle_t stage_completion_event;
        SemaphoreHandle_t completion_event;
    } session_;
};

static k_i2s_driver dev0_driver(I2S0_BASE_ADDR, SYSCTL_CLOCK_I2S0, SYSCTL_THRESHOLD_I2S0, SYSCTL_DMA_SELECT_I2S0_RX_REQ);
static k_i2s_driver dev1_driver(I2S1_BASE_ADDR, SYSCTL_CLOCK_I2S1, SYSCTL_THRESHOLD_I2S1, SYSCTL_DMA_SELECT_I2S1_RX_REQ);
static k_i2s_driver dev2_driver(I2S2_BASE_ADDR, SYSCTL_CLOCK_I2S2, SYSCTL_THRESHOLD_I2S2, SYSCTL_DMA_SELECT_I2S2_RX_REQ);

driver &g_i2s_driver_i2s0 = dev0_driver;
driver &g_i2s_driver_i2s1 = dev1_driver;
driver &g_i2s_driver_i2s2 = dev2_driver;
