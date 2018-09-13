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
/* Enable kernel-mode log API */
#include <stdlib.h>

#include <FreeRTOSConfig.h>
#include <sysctl.h>
#include "clint.h"
#include "entry.h"
#include "fpioa.h"
#include "uarths.h"

#define PLL0_OUTPUT_FREQ configCPU_CLOCK_HZ * 2
#define PLL1_OUTPUT_FREQ 160000000UL
#define PLL2_OUTPUT_FREQ 45158400UL

extern int main(int argc, char* argv[]);
extern void __libc_init_array(void);
extern void __libc_fini_array(void);
extern int os_entry(int core_id, int number_of_cores, int (*user_main)(int, char**));

void setup_clocks()
{
    /* Only set once */
    if (!sysctl->pll0.pll_out_en0)
    {
        sysctl_pll_enable(SYSCTL_PLL0);
        sysctl_pll_set_freq(SYSCTL_PLL0, SYSCTL_SOURCE_IN0, PLL0_OUTPUT_FREQ);
        while (sysctl_pll_is_lock(SYSCTL_PLL0) == 0)
            sysctl_pll_clear_slip(SYSCTL_PLL0);
        sysctl_clock_enable(SYSCTL_CLOCK_PLL0);
        sysctl->clk_sel0.aclk_divider_sel = 0;
        sysctl_clock_set_clock_select(SYSCTL_CLOCK_SELECT_ACLK, SYSCTL_SOURCE_PLL0);

        sysctl_pll_enable(SYSCTL_PLL1);
        sysctl_pll_set_freq(SYSCTL_PLL1, SYSCTL_SOURCE_IN0, PLL1_OUTPUT_FREQ);
        while (sysctl_pll_is_lock(SYSCTL_PLL1) == 0)
            sysctl_pll_clear_slip(SYSCTL_PLL1);
        sysctl_clock_enable(SYSCTL_CLOCK_PLL1);

        sysctl_pll_enable(SYSCTL_PLL2);
        sysctl_pll_set_freq(SYSCTL_PLL2, SYSCTL_SOURCE_IN0, PLL2_OUTPUT_FREQ);
        while (sysctl_pll_is_lock(SYSCTL_PLL2) == 0)
            sysctl_pll_clear_slip(SYSCTL_PLL2);
        sysctl_clock_enable(SYSCTL_CLOCK_PLL2);
    }
}

void _init_bsp(int core_id, int number_of_cores)
{
    /* Initialize thread local data */
    init_tls();

    if (core_id == 0)
    {
        /* Copy lma data to memory */
        init_lma();
        /* Initialize bss data to 0 */
        init_bss();
        /* Register finalization function */
        atexit(__libc_fini_array);
        /* Init libc array for C++ */
        __libc_init_array();
        /* Init FPIOA */
        fpioa_init();
        /* Setup clocks */
        setup_clocks();
        /* Init UART */
        uart_init();
    }

    exit(os_entry(core_id, number_of_cores, main));
}
