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
#include <plic.h>
#include <semphr.h>
#include <stdio.h>
#include <sysctl.h>
#include <timer.h>

using namespace sys;

class k_pwm_driver : public pwm_driver, public static_object, public free_object_access
{
public:
    k_pwm_driver(uintptr_t base_addr, sysctl_clock_t clock)
        : pwm_(*reinterpret_cast<volatile kendryte_timer_t *>(base_addr)), clock_(clock)
    {
    }

    virtual void install() override
    {
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

    virtual uint32_t get_pin_count() override
    {
        return 4;
    }

    virtual double set_frequency(double frequency) override
    {
        uint32_t clk_freq = sysctl_clock_get_freq(clock_);

        /* frequency = clk_freq / periods */
        int32_t periods = (int32_t)(clk_freq / frequency);
        configASSERT(periods > 0 && periods <= INT32_MAX);
        frequency = clk_freq / (double)periods;
        periods_ = periods;
        return frequency;
    }

    virtual double set_active_duty_cycle_percentage(uint32_t pin, double duty_cycle_percentage) override
    {
        configASSERT(pin < get_pin_count());
        configASSERT(duty_cycle_percentage >= 0 && duty_cycle_percentage <= 1);

        uint32_t percent = (uint32_t)(duty_cycle_percentage * periods_);
        pwm_.channel[pin].load_count = periods_ - percent;
        pwm_.load_count2[pin] = percent;
        return percent / 100.0;
    }

    virtual void set_enable(uint32_t pin, bool enable) override
    {
        configASSERT(pin < get_pin_count());
        if (enable)
            pwm_.channel[pin].control = TIMER_CR_INTERRUPT_MASK | TIMER_CR_PWM_ENABLE | TIMER_CR_USER_MODE | TIMER_CR_ENABLE;
        else
            pwm_.channel[pin].control = TIMER_CR_INTERRUPT_MASK;
    }

private:
    volatile kendryte_timer_t &pwm_;
    sysctl_clock_t clock_;

    uint32_t periods_;
};

static k_pwm_driver dev0_driver(TIMER0_BASE_ADDR, SYSCTL_CLOCK_TIMER0);
static k_pwm_driver dev1_driver(TIMER1_BASE_ADDR, SYSCTL_CLOCK_TIMER1);
static k_pwm_driver dev2_driver(TIMER2_BASE_ADDR, SYSCTL_CLOCK_TIMER2);

driver &g_pwm_driver_pwm0 = dev0_driver;
driver &g_pwm_driver_pwm1 = dev1_driver;
driver &g_pwm_driver_pwm2 = dev2_driver;
