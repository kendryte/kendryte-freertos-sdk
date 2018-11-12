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
#include <FreeRTOSConfig.h>
#include <clint.h>
#include <fpioa.h>
#include <hal.h>
#include <stdlib.h>
#include <string.h>
#include <sysctl.h>
#include <uarths.h>
#include "pin_cfg_priv.h"
#include <stdio.h>

#define PLL1_OUTPUT_FREQ 160000000UL
#define PLL2_OUTPUT_FREQ 45158400UL

extern uint8_t __bss_start[];
extern uint8_t __bss_end[];
extern uint8_t _tls_data[];

extern int main(int argc, char* argv[]);
extern void __libc_init_array(void);
extern void __libc_fini_array(void);
extern int os_entry(int core_id, int number_of_cores, int (*user_main)(int, char**));

static void setup_clocks()
{
    sysctl_pll_set_freq(SYSCTL_PLL1, PLL1_OUTPUT_FREQ);
    sysctl_pll_set_freq(SYSCTL_PLL2, PLL2_OUTPUT_FREQ);
}

static void init_bss(void)
{
    memset(__bss_start, 0, __bss_end - __bss_start);
}

void _init_bsp(int core_id, int number_of_cores)
{
    if (core_id == 0)
    {
        /* Initialize bss data to 0 */
        init_bss();
        /* Register finalization function */
        atexit(__libc_fini_array);
        /* Init libc array for C++ */
        __libc_init_array();
        /* Init FPIOA */
        fpioa_init();
        bsp_pin_setup();
        /* Setup clocks */
        setup_clocks();
        /* Init UART */
        uarths_init();
    }

    exit(os_entry(core_id, number_of_cores, main));
}
