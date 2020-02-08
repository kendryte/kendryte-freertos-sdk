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
#include <dvp.h>
#include <fpioa.h>
#include <hal.h>
#include <kernel/driver_impl.hpp>
#include <plic.h>
#include <semphr.h>
#include <stdio.h>
#include <sysctl.h>
#include <utility.h>
#include <iomem.h>

using namespace sys;

class k_dvp_driver : public dvp_driver, public static_object, public exclusive_object_access
{
public:
    k_dvp_driver(uintptr_t base_addr, sysctl_clock_t clock)
        : dvp_(*reinterpret_cast<volatile dvp_t *>(base_addr)), clock_(clock)
    {
    }

    virtual void install() override
    {
        sysctl_clock_disable(clock_);

        pic_set_irq_handler(IRQN_DVP_INTERRUPT, dvp_frame_event_isr, this);
        pic_set_irq_priority(IRQN_DVP_INTERRUPT, 1);
    }

    virtual void on_first_open() override
    {
        sysctl_clock_enable(clock_);
    }

    virtual void on_last_close() override
    {
        sysctl_clock_disable(clock_);
    }

    virtual uint32_t get_output_num() override
    {
        return 2;
    }

    virtual void config(uint32_t width, uint32_t height, bool auto_enable) override
    {
        configASSERT(width % 8 == 0 && width && height);

        uint32_t dvp_cfg = dvp_.dvp_cfg;

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

        dvp_.dvp_cfg = dvp_cfg;
        dvp_.cmos_cfg |= DVP_CMOS_CLK_DIV(xclk_devide_) | DVP_CMOS_CLK_ENABLE;
        width_ = width;
        height_ = height;
    }

    virtual void enable_frame() override
    {
        dvp_.sts = DVP_STS_DVP_EN | DVP_STS_DVP_EN_WE;
    }

    virtual void set_signal(dvp_signal_type_t type, bool value) override
    {
        switch (type)
        {
        case DVP_SIG_POWER_DOWN:
            if (value)
                dvp_.cmos_cfg |= DVP_CMOS_POWER_DOWN;
            else
                dvp_.cmos_cfg &= ~DVP_CMOS_POWER_DOWN;
            break;
        case DVP_SIG_RESET:
            if (value)
                dvp_.cmos_cfg |= DVP_CMOS_RESET;
            else
                dvp_.cmos_cfg &= ~DVP_CMOS_RESET;
            break;
        default:
            configASSERT(!"Invalid signal type.");
            break;
        }
    }

    virtual void set_output_enable(uint32_t index, bool enable) override
    {
        configASSERT(index < 2);

        if (index == 0)
        {
            if (enable)
                dvp_.dvp_cfg |= DVP_CFG_AI_OUTPUT_ENABLE;
            else
                dvp_.dvp_cfg &= ~DVP_CFG_AI_OUTPUT_ENABLE;
        }
        else
        {
            if (enable)
                dvp_.dvp_cfg |= DVP_CFG_DISPLAY_OUTPUT_ENABLE;
            else
                dvp_.dvp_cfg &= ~DVP_CFG_DISPLAY_OUTPUT_ENABLE;
        }
    }

    virtual void set_output_attributes(uint32_t index, video_format_t format, void *output_buffer) override
    {
        configASSERT(index < 2);
#if FIX_CACHE
        configASSERT(!is_memory_cache((uintptr_t)output_buffer));
#endif
        if (index == 0)
        {
            configASSERT(format == VIDEO_FMT_RGB24_PLANAR);
            uintptr_t buffer_addr = (uintptr_t)output_buffer;
            size_t planar_size = width_ * height_;
            dvp_.r_addr = buffer_addr;
            dvp_.g_addr = buffer_addr + planar_size;
            dvp_.b_addr = buffer_addr + planar_size * 2;
        }
        else
        {
            configASSERT(format == VIDEO_FMT_RGB565);
            dvp_.rgb_addr = (uintptr_t)output_buffer;
        }
    }

    virtual void set_frame_event_enable(dvp_frame_event_t event, bool enable) override
    {
        switch (event)
        {
        case VIDEO_FE_BEGIN:
            if (enable)
            {
                dvp_.sts |= DVP_STS_FRAME_START | DVP_STS_FRAME_START_WE;
                dvp_.dvp_cfg |= DVP_CFG_START_INT_ENABLE;
            }
            else
            {
                dvp_.dvp_cfg &= ~DVP_CFG_START_INT_ENABLE;
            }
            break;
        case VIDEO_FE_END:
            if (enable)
            {
                dvp_.sts |= DVP_STS_FRAME_FINISH | DVP_STS_FRAME_FINISH_WE;
                dvp_.dvp_cfg |= DVP_CFG_FINISH_INT_ENABLE;
            }
            else
            {
                dvp_.dvp_cfg &= ~DVP_CFG_FINISH_INT_ENABLE;
            }
            break;
        default:
            configASSERT(!"Invalid event.");
            break;
        }

        pic_set_irq_enable(IRQN_DVP_INTERRUPT, 1);
    }

    virtual void set_on_frame_event(dvp_on_frame_event_t callback, void *userdata) override
    {
        frame_event_callback_data_ = userdata;
        frame_event_callback_ = callback;
    }

    virtual double xclk_set_clock_rate(double clock_rate) override
    {
        uint32_t apb1_pclk = sysctl_clock_get_freq(SYSCTL_CLOCK_APB1);
        int16_t xclk_divide = apb1_pclk / clock_rate / 2 - 1;
        configASSERT((xclk_divide >= 0) && (xclk_divide < (1 << 8)));
        xclk_devide_ = xclk_divide;
        return apb1_pclk / (xclk_divide + 1);
    }

private:
    static void dvp_frame_event_isr(void *userdata)
    {
        auto &driver = *reinterpret_cast<k_dvp_driver *>(userdata);
        if (driver.dvp_.sts & DVP_STS_FRAME_START)
        {
            dvp_on_frame_event_t callback;
            if ((callback = driver.frame_event_callback_))
                callback(VIDEO_FE_BEGIN, driver.frame_event_callback_data_);
            driver.dvp_.sts |= DVP_STS_FRAME_START | DVP_STS_FRAME_START_WE;
        }
        if (driver.dvp_.sts & DVP_STS_FRAME_FINISH)
        {
            dvp_on_frame_event_t callback;
            if ((callback = driver.frame_event_callback_))
                callback(VIDEO_FE_END, driver.frame_event_callback_data_);
            driver.dvp_.sts |= DVP_STS_FRAME_FINISH | DVP_STS_FRAME_FINISH_WE;
        }
    }

private:
    volatile dvp_t &dvp_;
    sysctl_clock_t clock_;

    dvp_on_frame_event_t frame_event_callback_;
    void *frame_event_callback_data_;
    size_t width_;
    size_t height_;
    uint32_t xclk_devide_;
};

static k_dvp_driver dev0_driver(DVP_BASE_ADDR, SYSCTL_CLOCK_DVP);

driver &g_dvp_driver_dvp0 = dev0_driver;
