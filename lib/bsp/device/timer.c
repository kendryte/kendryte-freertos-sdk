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
#include <limits.h>
#include <plic.h>
#include <semphr.h>
#include <stdio.h>
#include <sysctl.h>
#include <timer.h>
#include <utility.h>

#define COMMON_ENTRY                                                                 \
    timer_data *data = (timer_data *)userdata;                                       \
    volatile kendryte_timer_t *timer = (volatile kendryte_timer_t *)data->base_addr; \
    (void)timer;

typedef struct
{
    uintptr_t base_addr;
    sysctl_clock_t clock;
    plic_irq_t irq;
    size_t channel;
    timer_on_tick_t on_tick;
    void *ontick_data;
} timer_data;

static void timer_isr(void *userdata);

static void timer_install(void *userdata)
{
    COMMON_ENTRY;

    if (data->channel == 0)
    {
        sysctl_clock_enable(data->clock);

        readl(&timer->eoi);
        size_t i;
        for (i = 0; i < 4; i++)
            timer->channel[i].control = TIMER_CR_INTERRUPT_MASK;

        pic_set_irq_handler(data->irq, timer_isr, userdata);
        pic_set_irq_handler(data->irq + 1, timer_isr, userdata);
        pic_set_irq_priority(data->irq, 1);
        pic_set_irq_priority(data->irq + 1, 1);
        pic_set_irq_enable(data->irq, 1);
        pic_set_irq_enable(data->irq + 1, 1);
    }
}

static int timer_open(void *userdata)
{
    COMMON_ENTRY;
    return 1;
}

static void timer_close(void *userdata)
{
}

static size_t timer_set_interval(size_t nanoseconds, void *userdata)
{
    COMMON_ENTRY;
    uint32_t clk_freq = sysctl_clock_get_freq(data->clock);
    double min_step = 1e9 / clk_freq;
    size_t value = (size_t)(nanoseconds / min_step);
    configASSERT(value > 0 && value < UINT32_MAX);
    timer->channel[data->channel].load_count = (uint32_t)value;
    return (size_t)(min_step * value);
}

static void timer_set_on_tick(timer_on_tick_t on_tick, void *ontick_data, void *userdata)
{
    COMMON_ENTRY;
    data->ontick_data = ontick_data;
    data->on_tick = on_tick;
}

static void timer_set_enable(bool enable, void *userdata)
{
    COMMON_ENTRY;
    if (enable)
        timer->channel[data->channel].control = TIMER_CR_USER_MODE | TIMER_CR_ENABLE;
    else
        timer->channel[data->channel].control = TIMER_CR_INTERRUPT_MASK;
}

static void timer_isr(void *userdata)
{
    COMMON_ENTRY;
    uint32_t channel = timer->intr_stat;
    size_t i = 0;
    for (i = 0; i < 4; i++)
    {
        if (channel & 1)
        {
            timer_data* td = data + i;
            if (td->on_tick)
                td->on_tick(td->ontick_data);
        }

        channel >>= 1;
    }

    readl(&timer->eoi);
}

/* clang-format off */
#define DEFINE_TIMER_DATA(i) \
{ TIMER##i##_BASE_ADDR, SYSCTL_CLOCK_TIMER##i, IRQN_TIMER##i##A_INTERRUPT, 0, NULL, NULL },  \
{ TIMER##i##_BASE_ADDR, SYSCTL_CLOCK_TIMER##i, IRQN_TIMER##i##A_INTERRUPT, 1, NULL, NULL },  \
{ TIMER##i##_BASE_ADDR, SYSCTL_CLOCK_TIMER##i, IRQN_TIMER##i##A_INTERRUPT, 2, NULL, NULL },  \
{ TIMER##i##_BASE_ADDR, SYSCTL_CLOCK_TIMER##i, IRQN_TIMER##i##A_INTERRUPT, 3, NULL, NULL }
/* clang format on */

#define INIT_TIMER_DRIVER(i) { { &dev_data[i], timer_install, timer_open, timer_close }, timer_set_interval, timer_set_on_tick, timer_set_enable }

static timer_data dev_data[12] =
{
    DEFINE_TIMER_DATA(0),
    DEFINE_TIMER_DATA(1),
    DEFINE_TIMER_DATA(2)
};

const timer_driver_t g_timer_driver_timer0 = INIT_TIMER_DRIVER(0);
const timer_driver_t g_timer_driver_timer1 = INIT_TIMER_DRIVER(1);
const timer_driver_t g_timer_driver_timer2 = INIT_TIMER_DRIVER(2);
const timer_driver_t g_timer_driver_timer3 = INIT_TIMER_DRIVER(3);
const timer_driver_t g_timer_driver_timer4 = INIT_TIMER_DRIVER(4);
const timer_driver_t g_timer_driver_timer5 = INIT_TIMER_DRIVER(5);
const timer_driver_t g_timer_driver_timer6 = INIT_TIMER_DRIVER(6);
const timer_driver_t g_timer_driver_timer7 = INIT_TIMER_DRIVER(7);
const timer_driver_t g_timer_driver_timer8 = INIT_TIMER_DRIVER(8);
const timer_driver_t g_timer_driver_timer9 = INIT_TIMER_DRIVER(9);
const timer_driver_t g_timer_driver_timer10 = INIT_TIMER_DRIVER(10);
const timer_driver_t g_timer_driver_timer11 = INIT_TIMER_DRIVER(11);
