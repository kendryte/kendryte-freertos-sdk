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
#include <kernel/driver_impl.hpp>
#include <limits.h>
#include <math.h>
#include <plic.h>
#include <semphr.h>
#include <stdio.h>
#include <sysctl.h>
#include <wdt.h>
#include <utility.h>

using namespace sys;

class k_wdt_driver : public wdt_driver, public static_object, public free_object_access
{
public:
    k_wdt_driver(uintptr_t base_addr, sysctl_clock_t clock, sysctl_threshold_t threshold, plic_irq_t irq, sysctl_reset_t reset)
        : wdt_(*reinterpret_cast<volatile wdt_t *>(base_addr)), clock_(clock), threshold_(threshold), irq_(irq), reset_(reset)
    {
    }

    virtual void install() override
    {
        sysctl_clock_disable(clock_);
    }

    virtual void on_first_open() override
    {
        sysctl_reset(reset_);
        sysctl_clock_set_threshold(threshold_, 0);
        sysctl_clock_enable(clock_);

        pic_set_irq_priority(irq_, 1);
        pic_set_irq_enable(irq_, 1);
        pic_set_irq_handler(irq_, wdt_isr, this);
    }

    virtual void on_last_close() override
    {
        sysctl_clock_disable(clock_);
    }

    virtual void set_response_mode(wdt_response_mode_t mode) override
    {
        uint8_t rmode = 0;
        switch (mode)
        {
        case WDT_RESP_RESET:
            rmode = WDT_CR_RMOD_RESET;
            break;
        case WDT_RESP_INTERRUPT:
            rmode = WDT_CR_RMOD_INTERRUPT;
            break;
        default:
            configASSERT(!"Invalid wdt response mode.");
            break;
        }

        wdt_.cr &= (~WDT_CR_RMOD_MASK);
        wdt_.cr |= rmode;
    }

    virtual size_t set_timeout(size_t nanoseconds) override
    {
        uint32_t clk_freq = sysctl_clock_get_freq(clock_);
        double min_step = 1e9 / clk_freq;
        double set_step = nanoseconds / min_step;
        uint32_t value = (uint32_t)log2(set_step) - 16;
        configASSERT(value <= 0xF);

        wdt_.torr = WDT_TORR_TOP((uint8_t)value);
        return (uint64_t)min_step << (16 + value);
    }

    virtual void set_on_timeout(wdt_on_timeout_t handler, void *userdata) override
    {
        on_timeout_data_ = userdata;
        on_timeout_ = handler;

        pic_set_irq_enable(irq_, handler ? 1 : 0);
    }

    virtual void restart_counter() override
    {
        wdt_.crr = WDT_CRR_MASK;
    }

    virtual void set_enable(bool enable) override
    {
        if (enable)
        {
            wdt_.crr = WDT_CRR_MASK;
            wdt_.cr |= WDT_CR_ENABLE;
        }
        else
        {
            wdt_.crr = WDT_CRR_MASK;
            wdt_.cr &= (~WDT_CR_ENABLE);
        }
    }

private:
    static void wdt_isr(void *userdata)
    {
        auto &driver = *reinterpret_cast<k_wdt_driver *>(userdata);

        if (driver.on_timeout_)
        {
            if (driver.on_timeout_(driver.on_timeout_data_))
            {
                readl(&driver.wdt_.eoi);
            }
        }
    }

private:
    volatile wdt_t &wdt_;
    sysctl_clock_t clock_;
    sysctl_threshold_t threshold_;
    plic_irq_t irq_;
    sysctl_reset_t reset_;

    wdt_on_timeout_t on_timeout_;
    void *on_timeout_data_;
};

static k_wdt_driver dev0_driver(WDT0_BASE_ADDR, SYSCTL_CLOCK_WDT0, SYSCTL_THRESHOLD_WDT0, IRQN_WDT0_INTERRUPT, SYSCTL_RESET_WDT0);
static k_wdt_driver dev1_driver(WDT1_BASE_ADDR, SYSCTL_CLOCK_WDT1, SYSCTL_THRESHOLD_WDT1, IRQN_WDT1_INTERRUPT, SYSCTL_RESET_WDT1);

driver &g_wdt_driver_wdt0 = dev0_driver;
driver &g_wdt_driver_wdt1 = dev1_driver;
