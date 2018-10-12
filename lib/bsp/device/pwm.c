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
#include <plic.h>
#include <semphr.h>
#include <stdio.h>
#include <sysctl.h>
#include <timer.h>

#define COMMON_ENTRY                                                                            \
    pwm_data *data = (pwm_data *)userdata;                                                      \
    volatile kendryte_timer_t *pwm = (volatile kendryte_timer_t *)data->base_addr;              \
    (void)pwm;                                                                                  \
    (void)data;

typedef struct
{
    sysctl_clock_t clock;
    uintptr_t base_addr;
    uint32_t pin_count;
    struct
    {
        uint32_t periods;
    };
} pwm_data;

static void pwm_install(void *userdata)
{
    COMMON_ENTRY;
}

static int pwm_open(void *userdata)
{
    return 1;
}

static void pwm_close(void *userdata)
{
}

static double pwm_set_frequency(double frequency, void *userdata)
{
    COMMON_ENTRY;
    uint32_t clk_freq = sysctl_clock_get_freq(data->clock);

    /* frequency = clk_freq / periods */
    int32_t periods = (int32_t)(clk_freq / frequency);
    configASSERT(periods > 0 && periods <= INT32_MAX);
    frequency = clk_freq / (double)periods;
    data->periods = periods;
    return frequency;
}

static double pwm_set_active_duty_cycle_percentage(uint32_t pin, double duty_cycle_percentage, void *userdata)
{
    COMMON_ENTRY;
    configASSERT(pin < data->pin_count);
    configASSERT(duty_cycle_percentage >= 0 && duty_cycle_percentage <= 1);

    uint32_t percent = (uint32_t)(duty_cycle_percentage * data->periods);
    pwm->channel[pin].load_count = data->periods - percent;
    pwm->load_count2[pin] = percent;
    return percent / 100.0;
}

static void pwm_set_enable(uint32_t pin, bool enable, void *userdata)
{
    COMMON_ENTRY;
    configASSERT(pin < data->pin_count);
    if (enable)
        pwm->channel[pin].control = TIMER_CR_INTERRUPT_MASK | TIMER_CR_PWM_ENABLE | TIMER_CR_USER_MODE | TIMER_CR_ENABLE;
    else
        pwm->channel[pin].control = TIMER_CR_INTERRUPT_MASK;
}

static pwm_data dev0_data = {SYSCTL_CLOCK_TIMER0, TIMER0_BASE_ADDR, 4, {0}};
static pwm_data dev1_data = {SYSCTL_CLOCK_TIMER1, TIMER1_BASE_ADDR, 4, {0}};
static pwm_data dev2_data = {SYSCTL_CLOCK_TIMER2, TIMER2_BASE_ADDR, 4, {0}};

const pwm_driver_t g_pwm_driver_pwm0 = {{&dev0_data, pwm_install, pwm_open, pwm_close}, 4, pwm_set_frequency, pwm_set_active_duty_cycle_percentage, pwm_set_enable};
const pwm_driver_t g_pwm_driver_pwm1 = {{&dev1_data, pwm_install, pwm_open, pwm_close}, 4, pwm_set_frequency, pwm_set_active_duty_cycle_percentage, pwm_set_enable};
const pwm_driver_t g_pwm_driver_pwm2 = {{&dev2_data, pwm_install, pwm_open, pwm_close}, 4, pwm_set_frequency, pwm_set_active_duty_cycle_percentage, pwm_set_enable};
