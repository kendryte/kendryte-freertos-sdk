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
#include <dvp.h>
#include <fpioa.h>
#include <hal.h>
#include <plic.h>
#include <semphr.h>
#include <stdio.h>
#include <sysctl.h>
#include <utility.h>

#define COMMON_ENTRY                                                      \
    dvp_data *data = (dvp_data *)userdata;                                \
    volatile dvp_t *dvp = (volatile dvp_t *)data->base_addr;              \
    (void)dvp;

typedef struct
{
    sysctl_clock_t clock;
    uintptr_t base_addr;

    struct
    {
        dvp_on_frame_event_t frame_event_callback;
        void *frame_event_callback_data;
        size_t width;
        size_t height;
        uint32_t xclk_devide;
    };
} dvp_data;

static void dvp_frame_event_isr(void *userdata);

static void dvp_install(void *userdata)
{
    COMMON_ENTRY;

    sysctl_clock_enable(data->clock);
    pic_set_irq_handler(IRQN_DVP_INTERRUPT, dvp_frame_event_isr, userdata);
    pic_set_irq_priority(IRQN_DVP_INTERRUPT, 1);
}

static int dvp_open(void *userdata)
{
    return 1;
}

static void dvp_close(void *userdata)
{
}

static void dvp_config(uint32_t width, uint32_t height, bool auto_enable, void *userdata)
{
    COMMON_ENTRY;
    configASSERT(width % 8 == 0 && width && height);

    uint32_t dvp_cfg = dvp->dvp_cfg;

    if (width / 8 % 4 == 0)
    {
        dvp_cfg |= DVP_CFG_BURST_SIZE_4BEATS;
        set_bit_mask(&dvp_cfg, DVP_AXI_GM_MLEN_MASK | DVP_CFG_HREF_BURST_NUM_MASK, DVP_AXI_GM_MLEN_4BYTE | DVP_CFG_HREF_BURST_NUM(width / 8 / 4));
    }
    else
    {
        dvp_cfg &= (~DVP_CFG_BURST_SIZE_4BEATS);
        set_bit_mask(&dvp_cfg, DVP_AXI_GM_MLEN_MASK, DVP_AXI_GM_MLEN_1BYTE | DVP_CFG_HREF_BURST_NUM(width / 8));
    }

    set_bit_mask(&dvp_cfg, DVP_CFG_LINE_NUM_MASK, DVP_CFG_LINE_NUM(height));

    if (auto_enable)
        dvp_cfg |= DVP_CFG_AUTO_ENABLE;
    else
        dvp_cfg &= ~DVP_CFG_AUTO_ENABLE;

    dvp->dvp_cfg = dvp_cfg;
    dvp->cmos_cfg |= DVP_CMOS_CLK_DIV(data->xclk_devide) | DVP_CMOS_CLK_ENABLE;
    data->width = width;
    data->height = height;
}

static void dvp_enable_frame(void *userdata)
{
    COMMON_ENTRY;
    dvp->sts = DVP_STS_DVP_EN | DVP_STS_DVP_EN_WE;
}

static void dvp_set_signal(dvp_signal_type_t type, bool value, void *userdata)
{
    COMMON_ENTRY;
    switch (type)
    {
    case DVP_SIG_POWER_DOWN:
        if (value)
            dvp->cmos_cfg |= DVP_CMOS_POWER_DOWN;
        else
            dvp->cmos_cfg &= ~DVP_CMOS_POWER_DOWN;
        break;
    case DVP_SIG_RESET:
        if (value)
            dvp->cmos_cfg |= DVP_CMOS_RESET;
        else
            dvp->cmos_cfg &= ~DVP_CMOS_RESET;
        break;
    default:
        configASSERT(!"Invalid signal type.");
        break;
    }
}

static void dvp_set_output_enable(uint32_t index, bool enable, void *userdata)
{
    COMMON_ENTRY;
    configASSERT(index < 2);

    if (index == 0)
    {
        if (enable)
            dvp->dvp_cfg |= DVP_CFG_AI_OUTPUT_ENABLE;
        else
            dvp->dvp_cfg &= ~DVP_CFG_AI_OUTPUT_ENABLE;
    }
    else
    {
        if (enable)
            dvp->dvp_cfg |= DVP_CFG_DISPLAY_OUTPUT_ENABLE;
        else
            dvp->dvp_cfg &= ~DVP_CFG_DISPLAY_OUTPUT_ENABLE;
    }
}

static void dvp_set_output_attributes(uint32_t index, video_format_t format, void *output_buffer, void *userdata)
{
    COMMON_ENTRY;
    configASSERT(index < 2);

    if (index == 0)
    {
        configASSERT(format == VIDEO_FMT_RGB24_PLANAR);
        uintptr_t buffer_addr = (uintptr_t)output_buffer;
        size_t planar_size = data->width * data->height;
        dvp->r_addr = buffer_addr;
        dvp->g_addr = buffer_addr + planar_size;
        dvp->b_addr = buffer_addr + planar_size * 2;
    }
    else
    {
        configASSERT(format == VIDEO_FMT_RGB565);
        dvp->rgb_addr = (uintptr_t)output_buffer;
    }
}

static void dvp_frame_event_isr(void *userdata)
{
    COMMON_ENTRY;

    if (dvp->sts & DVP_STS_FRAME_START)
    {
        dvp_on_frame_event_t callback;
        if ((callback = data->frame_event_callback))
            callback(VIDEO_FE_BEGIN, data->frame_event_callback_data);
        dvp->sts |= DVP_STS_FRAME_START | DVP_STS_FRAME_START_WE;
    }
    if (dvp->sts & DVP_STS_FRAME_FINISH)
    {
        dvp_on_frame_event_t callback;
        if ((callback = data->frame_event_callback))
            callback(VIDEO_FE_END, data->frame_event_callback_data);
        dvp->sts |= DVP_STS_FRAME_FINISH | DVP_STS_FRAME_FINISH_WE;
    }
}

static void dvp_set_frame_event_enable(dvp_frame_event_t event, bool enable, void *userdata)
{
    COMMON_ENTRY;
    switch (event)
    {
    case VIDEO_FE_BEGIN:
        if (enable)
        {
            dvp->sts |= DVP_STS_FRAME_START | DVP_STS_FRAME_START_WE;
            dvp->dvp_cfg |= DVP_CFG_START_INT_ENABLE;
        }
        else
        {
            dvp->dvp_cfg &= ~DVP_CFG_START_INT_ENABLE;
        }
        break;
    case VIDEO_FE_END:
        if (enable)
        {
            dvp->sts |= DVP_STS_FRAME_FINISH | DVP_STS_FRAME_FINISH_WE;
            dvp->dvp_cfg |= DVP_CFG_FINISH_INT_ENABLE;
        }
        else
        {
            dvp->dvp_cfg &= ~DVP_CFG_FINISH_INT_ENABLE;
        }
        break;
    default:
        configASSERT(!"Invalid event.");
        break;
    }

    pic_set_irq_enable(IRQN_DVP_INTERRUPT, 1);
}

static void dvp_set_on_frame_event(dvp_on_frame_event_t callback, void *callback_data, void *userdata)
{
    COMMON_ENTRY;

    data->frame_event_callback_data = callback_data;
    data->frame_event_callback = callback;
}

static double dvp_xclk_set_clock_rate(double clock_rate, void *userdata)
{
    COMMON_ENTRY;

    uint32_t apb1_pclk = sysctl_clock_get_freq(SYSCTL_CLOCK_APB1);
    int16_t xclk_divide = apb1_pclk / clock_rate / 2 -1;
    configASSERT((xclk_divide >= 0) && ( xclk_divide <(1 << 8)));
    data->xclk_devide = xclk_divide;
    return apb1_pclk / (xclk_divide + 1);
}

static dvp_data dev0_data = {SYSCTL_CLOCK_DVP, DVP_BASE_ADDR, {0}};

const dvp_driver_t g_dvp_driver_dvp0 = {{&dev0_data, dvp_install, dvp_open, dvp_close}, 2, dvp_config, dvp_enable_frame, dvp_set_signal, dvp_set_output_enable, dvp_set_output_attributes, dvp_set_frame_event_enable, dvp_set_on_frame_event, dvp_xclk_set_clock_rate};
