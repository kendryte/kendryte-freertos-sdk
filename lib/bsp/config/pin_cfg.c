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
#include <pin_cfg.h>
#include <fpioa.h>
#include <FreeRTOS.h>
#include <sysctl.h>

const fpioa_cfg_t __attribute__((weak)) g_fpioa_cfg =
{
    .version = PIN_CFG_VERSION,
    .functions_count = 0
};

const power_bank_cfg_t __attribute__((weak)) g_power_bank_cfg =
{
    .version = PIN_CFG_VERSION,
    .power_banks_count = 0
};

const pin_cfg_t __attribute__((weak)) g_pin_cfg =
{
    .version = PIN_CFG_VERSION,
    .set_spi0_dvp_data = 0
};

static void fpioa_setup()
{
    configASSERT(g_fpioa_cfg.version == PIN_CFG_VERSION);

    uint32_t i;
    for (i = 0; i < g_fpioa_cfg.functions_count; i++)
    {
        fpioa_cfg_item_t item = g_fpioa_cfg.functions[i];
        fpioa_set_function(item.number, item.function);
    }
}

static void power_bank_setup()
{
    configASSERT(g_power_bank_cfg.version == PIN_CFG_VERSION);

    uint32_t i;
    for (i = 0; i < g_power_bank_cfg.power_banks_count; i++)
    {
        power_bank_item_t item = g_power_bank_cfg.power_banks[i];
        sysctl_set_power_mode(item.power_bank, item.io_power_mode);
    }
}

static void pin_setup()
{
    configASSERT(g_pin_cfg.version == PIN_CFG_VERSION);

    sysctl_set_spi0_dvp_data(g_pin_cfg.set_spi0_dvp_data);
}

void bsp_pin_setup()
{
    fpioa_setup();
    power_bank_setup();
    pin_setup();
}
