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
#ifndef _PIN_CFG_H
#define _PIN_CFG_H

#include <platform.h>
#include <stdbool.h>
#include <stdint.h>

#define PIN_CFG_VERSION 1

typedef struct _fpioa_cfg_item
{
    int number;
    fpioa_function_t function;
} fpioa_cfg_item_t;

typedef struct _fpioa_cfg
{
    uint32_t version;
    uint32_t functions_count;
    fpioa_cfg_item_t functions[];
} fpioa_cfg_t;

typedef struct _power_bank_item
{
    sysctl_power_bank_t power_bank;
    sysctl_io_power_mode_t io_power_mode;
} power_bank_item_t;

typedef struct _power_bank_cfg
{
    uint32_t version;
    uint32_t power_banks_count;
    power_bank_item_t power_banks[];
} power_bank_cfg_t;

typedef struct _pin_cfg
{
    uint32_t version;
    bool set_spi0_dvp_data;
} pin_cfg_t;

extern const fpioa_cfg_t g_fpioa_cfg;
extern const power_bank_cfg_t g_power_bank_cfg;
extern const pin_cfg_t g_pin_cfg;

#endif
