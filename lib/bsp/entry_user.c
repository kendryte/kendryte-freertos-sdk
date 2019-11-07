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
#include "interrupt.h"
#include "pin_cfg_priv.h"
#include <FreeRTOSConfig.h>
#include <clint.h>
#include <fpioa.h>
#include <hal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysctl.h>
#include <uarths.h>

#define PLL1_OUTPUT_FREQ 400000000UL
#define PLL2_OUTPUT_FREQ 45158400UL

extern uint8_t _tls_data[];

extern int main(int argc, char *argv[]);
extern int os_entry(int (*user_main)(int, char **));

static void setup_clocks()
{
    sysctl_pll_set_freq(SYSCTL_PLL1, PLL1_OUTPUT_FREQ);
    sysctl_pll_set_freq(SYSCTL_PLL2, PLL2_OUTPUT_FREQ);
}

void _init_bsp()
{
    /* Init FPIOA */
    fpioa_init();
    bsp_pin_setup();
    /* Setup clocks */
    setup_clocks();
    /* Init UART */
    uarths_init();

    exit(os_entry(main));
}
