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
#include <math.h>
#include <plic.h>
#include <semphr.h>
#include <stdio.h>
#include <sysctl.h>
#include <wdt.h>
#include <utility.h>

#define COMMON_ENTRY                                               \
    wdt_data *data = (wdt_data *)userdata;                         \
    volatile wdt_t *wdt = (volatile wdt_t *)data->base_addr;       \
    (void)wdt;

typedef struct
{
    uintptr_t base_addr;
    sysctl_clock_t clock;
    sysctl_threshold_t threshold;
    plic_irq_t irq;
    sysctl_reset_t reset;
    struct
    {
        wdt_on_timeout_t on_timeout;
        void *on_timeout_data;
    };
} wdt_data;

static void wdt_isr(void *userdata);

static void wdt_install(void *userdata)
{
    COMMON_ENTRY;
    sysctl_reset(data->reset);
    sysctl_clock_set_threshold(data->threshold, 0);
    sysctl_clock_enable(data->clock);

    pic_set_irq_priority(data->irq, 1);
    pic_set_irq_enable(data->irq, 1);
    pic_set_irq_handler(data->irq, wdt_isr, userdata);
}

static int wdt_open(void *userdata)
{
    COMMON_ENTRY;
    return 1;
}

static void wdt_close(void *userdata)
{
}

static void wdt_set_response_mode(wdt_response_mode_t mode, void *userdata)
{
    COMMON_ENTRY;

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

    wdt->cr &= (~WDT_CR_RMOD_MASK);
    wdt->cr |= rmode;
}

static size_t wdt_set_timeout(size_t nanoseconds, void *userdata)
{
    COMMON_ENTRY;
    uint32_t clk_freq = sysctl_clock_get_freq(data->clock);
    double min_step = 1e9 / clk_freq;
    double set_step = nanoseconds / min_step;
    uint32_t value  = (uint32_t)log2((uint64_t)set_step) - 16;
    configASSERT(value <= 0xF);

    wdt->torr = WDT_TORR_TOP((uint8_t)value);
    return (uint64_t)min_step << (16 + value);
}

static void wdt_set_on_timeout(wdt_on_timeout_t on_timeout, void *on_timeout_data, void *userdata)
{
    COMMON_ENTRY;
    data->on_timeout_data = on_timeout_data;
    data->on_timeout = on_timeout;

    pic_set_irq_enable(data->irq, on_timeout ? 1 : 0);
}

static void wdt_restart_counter(void *userdata)
{
    COMMON_ENTRY;
    wdt->crr = WDT_CRR_MASK;
}

static void wdt_set_enable(bool enable, void *userdata)
{
    COMMON_ENTRY;
    if (enable)
    {
        wdt->crr = WDT_CRR_MASK;
        wdt->cr |= WDT_CR_ENABLE;
    }
    else
    {
        wdt->crr = WDT_CRR_MASK;
        wdt->cr &= (~WDT_CR_ENABLE);
    }
}

static void wdt_isr(void *userdata)
{
    COMMON_ENTRY;
    if (data->on_timeout)
    {
        if (data->on_timeout(data->on_timeout_data))
        {
            readl(&wdt->eoi);
        }
    }
}

static wdt_data dev0_data = {WDT0_BASE_ADDR, SYSCTL_CLOCK_WDT0, SYSCTL_THRESHOLD_WDT0, IRQN_WDT0_INTERRUPT, SYSCTL_RESET_WDT0, {0}};
static wdt_data dev1_data = {WDT1_BASE_ADDR, SYSCTL_CLOCK_WDT1, SYSCTL_THRESHOLD_WDT1, IRQN_WDT1_INTERRUPT, SYSCTL_RESET_WDT1, {0}};

const wdt_driver_t g_wdt_driver_wdt0 = {{&dev0_data, wdt_install, wdt_open, wdt_close}, wdt_set_response_mode, wdt_set_timeout, wdt_set_on_timeout, wdt_restart_counter, wdt_set_enable};
const wdt_driver_t g_wdt_driver_wdt1 = {{&dev1_data, wdt_install, wdt_open, wdt_close}, wdt_set_response_mode, wdt_set_timeout, wdt_set_on_timeout, wdt_restart_counter, wdt_set_enable};
