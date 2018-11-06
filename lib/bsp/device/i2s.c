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
#include <driver.h>
#include <fpioa.h>
#include <hal.h>
#include <i2s.h>
#include <math.h>
#include <semphr.h>
#include <stdio.h>
#include <stdlib.h>
#include <sysctl.h>
#include <utility.h>

#define BUFFER_COUNT 2
#define COMMON_ENTRY                                                      \
    i2s_data *data = (i2s_data *)userdata;                                \
    volatile i2s_t *i2s = (volatile i2s_t *)data->base_addr;              \
    (void)i2s;

typedef enum
{
    I2S_RECEIVE,
    I2S_SEND
} i2s_transmit;

typedef struct
{
    sysctl_clock_t clock;
    uintptr_t base_addr;
    sysctl_dma_select_t dma_req_base;
    sysctl_threshold_t clock_threshold;
    struct
    {
        i2s_transmit transmit;
        uint8_t *buffer;
        size_t buffer_frames;
        size_t buffer_size;
        size_t block_align;
        size_t channels;
        size_t buffer_ptr;
        volatile int next_free_buffer;
        volatile int dma_in_use_buffer;
        int stop_signal;
        uintptr_t transmit_dma;
        SemaphoreHandle_t stage_completion_event;
        SemaphoreHandle_t completion_event;
    };
} i2s_data;

static void i2s_transmit_set_enable(i2s_transmit transmit, int enable, volatile i2s_t *i2s);
static void i2sc_transmit_set_enable(i2s_transmit transmit, int enable, volatile i2s_channel_t *i2sc);

static void i2s_install(void *userdata)
{
    COMMON_ENTRY;

    /* GPIO clock under APB0 clock, so enable APB0 clock firstly */
    sysctl_clock_enable(data->clock);

    ier_t u_ier;

    u_ier.reg_data = readl(&i2s->ier);
    u_ier.ier.ien = 1;
    writel(u_ier.reg_data, &i2s->ier);

    data->buffer = NULL;
}

static int i2s_open(void *userdata)
{
    return 1;
}

static void i2s_close(void *userdata)
{
}

static void i2s_set_threshold(volatile i2s_channel_t *i2sc, i2s_transmit transmit, i2s_fifo_threshold_t threshold)
{
    if (transmit == I2S_RECEIVE)
    {
        rfcr_t u_rfcr;

        u_rfcr.reg_data = readl(&i2sc->rfcr);
        u_rfcr.rfcr.rxchdt = threshold;
        writel(u_rfcr.reg_data, &i2sc->rfcr);
    }
    else
    {
        tfcr_t u_tfcr;

        u_tfcr.reg_data = readl(&i2sc->tfcr);
        u_tfcr.tfcr.txchet = threshold;
        writel(u_tfcr.reg_data, &i2sc->tfcr);
    }
}

static void i2sc_set_mask_interrupt(volatile i2s_channel_t *i2sc,
    uint32_t rx_available_int, uint32_t rx_overrun_int,
    uint32_t tx_empty_int, uint32_t tx_overrun_int)
{
    imr_t u_imr;

    u_imr.reg_data = readl(&i2sc->imr);

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
    writel(u_imr.reg_data, &i2sc->imr);
}

static void extract_params(const audio_format_t *format, uint32_t *threshold, i2s_word_select_cycles_t *wsc, i2s_word_length_t *wlen, size_t *block_align, uint32_t *dma_divide16)
{
    uint32_t pll2_clock = 0;
    pll2_clock = sysctl_pll_get_freq(SYSCTL_PLL2);
    configASSERT((format->sample_rate > pll2_clock / (1 << 23)) && (format->sample_rate < pll2_clock / (1 << 7)));
    switch (format->bits_per_sample)
    {
        case 16:
            *wsc = SCLK_CYCLES_32;
            *wlen = RESOLUTION_16_BIT;
            *block_align = format->channels * 2;
            *dma_divide16 = 1;
            break;
        case 24:
            *wsc = SCLK_CYCLES_32;
            *wlen = RESOLUTION_24_BIT;
            *block_align = format->channels * 4;
            *dma_divide16 = 0;
            break;
        case 32:
            *wsc = SCLK_CYCLES_32;
            *wlen = RESOLUTION_32_BIT;
            *block_align = format->channels * 4;
            *dma_divide16 = 0;
            break;
        default:
            configASSERT(!"Invlid bits per sample");
            break;
    }
    *threshold = round(pll2_clock / (format->sample_rate * 128.0) - 1);
}

static void i2s_config_as_render(const audio_format_t *format, size_t delay_ms, i2s_align_mode_t align_mode, size_t channels_mask, void *userdata)
{
    COMMON_ENTRY;

    data->transmit = I2S_SEND;

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

    extract_params(format, &threshold, &wsc, &wlen, &block_align, &dma_divide16);

    sysctl_clock_set_threshold(data->clock_threshold, threshold);

    i2s_transmit_set_enable(I2S_RECEIVE, 0, userdata);
    i2s_transmit_set_enable(I2S_SEND, 0, userdata);

    ccr_t u_ccr;
    cer_t u_cer;

    u_cer.reg_data = readl(&i2s->cer);
    u_cer.cer.clken = 0;
    writel(u_cer.reg_data, &i2s->cer);

    u_ccr.reg_data = readl(&i2s->ccr);
    u_ccr.ccr.clk_word_size = wsc;
    u_ccr.ccr.clk_gate = NO_CLOCK_GATING;
    u_ccr.ccr.align_mode = am;
    u_ccr.ccr.dma_tx_en = 1;
    u_ccr.ccr.sign_expand_en = 1;
    u_ccr.ccr.dma_divide_16 = dma_divide16;
    u_ccr.ccr.dma_rx_en = 0;
    writel(u_ccr.reg_data, &i2s->ccr);

    u_cer.reg_data = readl(&i2s->cer);
    u_cer.cer.clken = 1;
    writel(u_cer.reg_data, &i2s->cer);

    writel(1, &i2s->txffr);
    writel(1, &i2s->rxffr);

    size_t channel = 0;
    size_t enabled_channel = 0;
    for (channel = 0; channel < 4; channel++)
    {
        volatile i2s_channel_t *i2sc = i2s->channel + channel;

        if ((channels_mask & 3) == 3)
        {
            i2sc_transmit_set_enable(I2S_SEND, 1, i2sc);
            i2sc_transmit_set_enable(I2S_RECEIVE, 0, i2sc);
            i2sc_set_mask_interrupt(i2sc, 0, 0, 1, 1);

            rcr_tcr_t u_tcr;
            u_tcr.reg_data = readl(&i2sc->tcr);
            u_tcr.rcr_tcr.wlen = wlen;
            writel(u_tcr.reg_data, &i2sc->tcr);

            i2s_set_threshold(i2sc, I2S_SEND, TRIGGER_LEVEL_8);
            enabled_channel++;
        }
        else
        {
            i2sc_transmit_set_enable(I2S_SEND, 0, i2sc);
            i2sc_transmit_set_enable(I2S_RECEIVE, 0, i2sc);
        }

        channels_mask >>= 2;
    }

    configASSERT(enabled_channel * 2 == format->channels);

    data->channels = format->channels;
    data->block_align = block_align;
    data->buffer_frames = format->sample_rate * delay_ms / 1000;
    configASSERT(data->buffer_frames >= 100);
    free(data->buffer);
    data->buffer_size = data->block_align * data->buffer_frames;
    data->buffer = (uint8_t*)malloc(data->buffer_size * BUFFER_COUNT);
    data->buffer_ptr = 0;
    data->next_free_buffer = 0;
    data->stop_signal = 0;
    data->transmit_dma = 0;
    data->dma_in_use_buffer = 0;
}

static void i2s_config_as_capture(const audio_format_t *format, size_t delay_ms, i2s_align_mode_t align_mode, size_t channels_mask, void *userdata)
{
    COMMON_ENTRY;

    data->transmit = I2S_RECEIVE;

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

    configASSERT(format->bits_per_sample != 16);
    extract_params(format, &threshold, &wsc, &wlen, &block_align, &dma_divide16);

    sysctl_clock_set_threshold(data->clock_threshold, threshold);

    i2s_transmit_set_enable(I2S_RECEIVE, 0, userdata);
    i2s_transmit_set_enable(I2S_SEND, 0, userdata);

    ccr_t u_ccr;
    cer_t u_cer;

    u_cer.reg_data = readl(&i2s->cer);
    u_cer.cer.clken = 0;
    writel(u_cer.reg_data, &i2s->cer);

    u_ccr.reg_data = readl(&i2s->ccr);
    u_ccr.ccr.clk_word_size = wsc;
    u_ccr.ccr.clk_gate = NO_CLOCK_GATING;
    u_ccr.ccr.align_mode = am;
    u_ccr.ccr.dma_tx_en = 0;
    u_ccr.ccr.sign_expand_en = 1;
    u_ccr.ccr.dma_divide_16 = 0;
    u_ccr.ccr.dma_rx_en = 1;
    writel(u_ccr.reg_data, &i2s->ccr);

    u_cer.reg_data = readl(&i2s->cer);
    u_cer.cer.clken = 1;
    writel(u_cer.reg_data, &i2s->cer);

    writel(1, &i2s->txffr);
    writel(1, &i2s->rxffr);

    size_t channel = 0;
    size_t enabled_channel = 0;
    for (channel = 0; channel < 4; channel++)
    {
        volatile i2s_channel_t *i2sc = i2s->channel + channel;

        if ((channels_mask & 3) == 3)
        {
            i2sc_transmit_set_enable(I2S_SEND, 0, i2sc);
            i2sc_transmit_set_enable(I2S_RECEIVE, 1, i2sc);
            i2sc_set_mask_interrupt(i2sc, 1, 1, 0, 0);

            /* set word length */
            rcr_tcr_t u_tcr;
            u_tcr.reg_data = readl(&i2sc->rcr);
            u_tcr.rcr_tcr.wlen = wlen;
            writel(u_tcr.reg_data, &i2sc->rcr);

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

    configASSERT(enabled_channel * 2 == format->channels);

    data->channels = format->channels;
    data->block_align = block_align;
    data->buffer_frames = format->sample_rate * delay_ms / 1000;
    configASSERT(data->buffer_frames >= 100);
    free(data->buffer);
    data->buffer_size = data->block_align * data->buffer_frames;
    data->buffer = (uint8_t*)malloc(data->buffer_size * BUFFER_COUNT);
    data->buffer_ptr = 0;
    data->next_free_buffer = 0;
    data->stop_signal = 0;
    data->transmit_dma = 0;
    data->dma_in_use_buffer = 0;
}

static void i2s_get_buffer(uint8_t **buffer, size_t *frames, void *userdata)
{
    COMMON_ENTRY;

    while (data->next_free_buffer == data->dma_in_use_buffer)
    {
        xSemaphoreTake(data->stage_completion_event, portMAX_DELAY);
    }

    *buffer = data->buffer + data->buffer_size * data->next_free_buffer + data->buffer_ptr;
    *frames = (data->buffer_size - data->buffer_ptr) / data->block_align;
}

static void i2s_release_buffer(size_t frames, void *userdata)
{
    COMMON_ENTRY;

    data->buffer_ptr += frames * data->block_align;
    if (data->buffer_ptr >= data->buffer_size)
    {
        data->buffer_ptr = 0;
        if (++data->next_free_buffer >= BUFFER_COUNT)
            data->next_free_buffer = 0;
    }
}

static void i2s_transmit_set_enable(i2s_transmit transmit, int enable, volatile i2s_t *i2s)
{
    irer_t u_irer;
    iter_t u_iter;

    if (transmit == I2S_RECEIVE)
    {
        u_irer.reg_data = readl(&i2s->irer);
        u_irer.irer.rxen = enable;
        writel(u_irer.reg_data, &i2s->irer);
    }
    else
    {
        u_iter.reg_data = readl(&i2s->iter);
        u_iter.iter.txen = enable;
        writel(u_iter.reg_data, &i2s->iter);
    }
}

static void i2sc_transmit_set_enable(i2s_transmit transmit, int enable, volatile i2s_channel_t *i2sc)
{
    rer_t u_rer;
    ter_t u_ter;

    if (transmit == I2S_SEND)
    {
        u_ter.reg_data = readl(&i2sc->ter);
        u_ter.ter.txchenx = enable;
        writel(u_ter.reg_data, &i2sc->ter);
    }
    else
    {
        u_rer.reg_data = readl(&i2sc->rer);
        u_rer.rer.rxchenx = enable;
        writel(u_rer.reg_data, &i2sc->rer);
    }
}

static void i2s_stage_completion_isr(void *userdata)
{
    COMMON_ENTRY;

    if (++data->dma_in_use_buffer >= BUFFER_COUNT)
        data->dma_in_use_buffer = 0;

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(data->stage_completion_event, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken)
        portYIELD();
}

static void i2s_start(void *userdata)
{
    COMMON_ENTRY;

    if (data->transmit == I2S_SEND)
    {
        configASSERT(!data->transmit_dma);

        data->stop_signal = 0;
        data->transmit_dma = dma_open_free();
        dma_set_request_source(data->transmit_dma, data->dma_req_base - 1);
        data->dma_in_use_buffer = 0;
        data->stage_completion_event = xSemaphoreCreateCounting(100, 0);
        data->completion_event = xSemaphoreCreateBinary();

        const volatile void *srcs[BUFFER_COUNT] = {
            data->buffer,
            data->buffer + data->buffer_size};
        volatile void *dests[1] = {
            &i2s->txdma};

        dma_loop_async(data->transmit_dma, srcs, BUFFER_COUNT, dests, 1, 1, 0, sizeof(uint32_t), data->buffer_size >> 2, 1, i2s_stage_completion_isr, userdata, data->completion_event, &data->stop_signal);
    }
    else
    {
        configASSERT(!data->transmit_dma);

        data->stop_signal = 0;
        data->transmit_dma = dma_open_free();
        dma_set_request_source(data->transmit_dma, data->dma_req_base);
        data->dma_in_use_buffer = 0;
        data->stage_completion_event = xSemaphoreCreateCounting(100, 0);
        data->completion_event = xSemaphoreCreateBinary();

        const volatile void *srcs[1] = {
            &i2s->rxdma};
        volatile void *dests[BUFFER_COUNT] = {
            data->buffer,
            data->buffer + data->buffer_size};

        dma_loop_async(data->transmit_dma, srcs, 1, dests, BUFFER_COUNT, 0, 1, sizeof(uint32_t), data->buffer_size >> 2, 4, i2s_stage_completion_isr, userdata, data->completion_event, &data->stop_signal);
    }
    i2s_transmit_set_enable(data->transmit, 1, i2s);
}

static void i2s_stop(void *userdata)
{
    COMMON_ENTRY;
    i2s_transmit_set_enable(data->transmit, 0, i2s);
}

static i2s_data dev0_data = {SYSCTL_CLOCK_I2S0, I2S0_BASE_ADDR, SYSCTL_DMA_SELECT_I2S0_RX_REQ, SYSCTL_THRESHOLD_I2S0, {0}};
static i2s_data dev1_data = {SYSCTL_CLOCK_I2S1, I2S1_BASE_ADDR, SYSCTL_DMA_SELECT_I2S1_RX_REQ, SYSCTL_THRESHOLD_I2S1, {0}};
static i2s_data dev2_data = {SYSCTL_CLOCK_I2S2, I2S2_BASE_ADDR, SYSCTL_DMA_SELECT_I2S2_RX_REQ, SYSCTL_THRESHOLD_I2S2, {0}};

const i2s_driver_t g_i2s_driver_i2s0 = {{&dev0_data, i2s_install, i2s_open, i2s_close}, i2s_config_as_render, i2s_config_as_capture, i2s_get_buffer, i2s_release_buffer, i2s_start, i2s_stop};
const i2s_driver_t g_i2s_driver_i2s1 = {{&dev1_data, i2s_install, i2s_open, i2s_close}, i2s_config_as_render, i2s_config_as_capture, i2s_get_buffer, i2s_release_buffer, i2s_start, i2s_stop};
const i2s_driver_t g_i2s_driver_i2s2 = {{&dev2_data, i2s_install, i2s_open, i2s_close}, i2s_config_as_render, i2s_config_as_capture, i2s_get_buffer, i2s_release_buffer, i2s_start, i2s_stop};
