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
#include <plic.h>
#include <semphr.h>
#include <stdio.h>
#include <sysctl.h>
#include <timer.h>
#include <utility.h>

using namespace sys;

static void *irq_context[3][4];

class k_timer_driver : public timer_driver, public static_object, public exclusive_object_access
{
public:
    k_timer_driver(uintptr_t base_addr, sysctl_clock_t clock, plic_irq_t irq, uint32_t num, uint32_t channel)
        : timer_(*reinterpret_cast<volatile kendryte_timer_t *>(base_addr)), clock_(clock), irq_(irq), num_(num), channel_(channel)
    {
    }

    virtual void install() override
    {
        irq_context[num_][channel_] = this;
        if (channel_ == 0)
        {
            sysctl_clock_enable(clock_);

            readl(&timer_.eoi);
            size_t i;
            for (i = 0; i < 4; i++)
                timer_.channel[i].control = TIMER_CR_INTERRUPT_MASK;

            pic_set_irq_handler(irq_, timer_isr, irq_context[num_]);
            pic_set_irq_handler(irq_ + 1, timer_isr, irq_context[num_]);
            pic_set_irq_priority(irq_, 1);
            pic_set_irq_priority(irq_ + 1, 1);
            pic_set_irq_enable(irq_, 1);
            pic_set_irq_enable(irq_ + 1, 1);
            sysctl_clock_disable(clock_);
        }
    }

    virtual void on_first_open() override
    {
        sysctl_clock_enable(clock_);
    }

    virtual void on_last_close() override
    {
        sysctl_clock_disable(clock_);
    }

    virtual size_t set_interval(size_t nanoseconds) override
    {
        uint32_t clk_freq = sysctl_clock_get_freq(clock_);
        double min_step = 1e9 / clk_freq;
        size_t value = (size_t)(nanoseconds / min_step);
        configASSERT(value > 0 && value < UINT32_MAX);
        timer_.channel[channel_].load_count = (uint32_t)value;
        return (size_t)(min_step * value);
    }

    virtual void set_on_tick(timer_on_tick_t on_tick, void *userdata) override
    {
        ontick_data_ = userdata;
        on_tick_ = on_tick;
    }

    virtual void set_enable(bool enable) override
    {
        if (enable)
            timer_.channel[channel_].control = TIMER_CR_USER_MODE | TIMER_CR_ENABLE;
        else
            timer_.channel[channel_].control = TIMER_CR_INTERRUPT_MASK;
    }

private:
    static void timer_isr(void *userdata)
    {
        k_timer_driver **context = reinterpret_cast<k_timer_driver **>(userdata);
        auto &driver = *context[0];
        auto &timer = driver.timer_;

        uint32_t channel = timer.intr_stat;
        size_t i = 0;
        for (i = 0; i < 4; i++)
        {
            if (channel & 1)
            {
                auto &driver_ch = *context[i];
                if (driver_ch.on_tick_)
                {
                    driver_ch.on_tick_(driver_ch.ontick_data_);
                }
            }

            channel >>= 1;
        }

        readl(&timer.eoi);
    }

private:
    volatile kendryte_timer_t &timer_;
    sysctl_clock_t clock_;
    plic_irq_t irq_;
    size_t num_;
    size_t channel_;

    timer_on_tick_t on_tick_;
    void *ontick_data_;
};

/* clang-format off */
#define DEFINE_TIMER_DATA(i) \
{ TIMER##i##_BASE_ADDR, SYSCTL_CLOCK_TIMER##i, IRQN_TIMER##i##A_INTERRUPT, i, 0 },  \
{ TIMER##i##_BASE_ADDR, SYSCTL_CLOCK_TIMER##i, IRQN_TIMER##i##A_INTERRUPT, i, 1 },  \
{ TIMER##i##_BASE_ADDR, SYSCTL_CLOCK_TIMER##i, IRQN_TIMER##i##A_INTERRUPT, i, 2 },  \
{ TIMER##i##_BASE_ADDR, SYSCTL_CLOCK_TIMER##i, IRQN_TIMER##i##A_INTERRUPT, i, 3 }
/* clang format on */

#define INIT_TIMER_DRIVER(i) { { &dev_driver[i], timer_install, timer_open, timer_close }, timer_set_interval, timer_set_on_tick, timer_set_enable }

static k_timer_driver dev_driver[12] =
{
    DEFINE_TIMER_DATA(0),
    DEFINE_TIMER_DATA(1),
    DEFINE_TIMER_DATA(2)
};

driver &g_timer_driver_timer0 = dev_driver[0];
driver &g_timer_driver_timer1 = dev_driver[1];
driver &g_timer_driver_timer2 = dev_driver[2];
driver &g_timer_driver_timer3 = dev_driver[3];
driver &g_timer_driver_timer4 = dev_driver[4];
driver &g_timer_driver_timer5 = dev_driver[5];
driver &g_timer_driver_timer6 = dev_driver[6];
driver &g_timer_driver_timer7 = dev_driver[7];
driver &g_timer_driver_timer8 = dev_driver[8];
driver &g_timer_driver_timer9 = dev_driver[9];
driver &g_timer_driver_timer10 = dev_driver[10];
driver &g_timer_driver_timer11 = dev_driver[11];
