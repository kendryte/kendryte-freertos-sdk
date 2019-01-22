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
#include "dump.h"
#include <stdint.h>
#include <stdlib.h>
#include <syslog.h>
#include <task.h>

static const char *TAG = "INTERRUPT";

void __attribute__((weak)) handle_irq_dummy(uintptr_t *regs, uintptr_t cause)
{
    LOGE(TAG, "unhandled interrupt: Cause 0x%016lx, EPC 0x%016lx\n", cause, regs[REG_EPC]);
    exit(1337);
}

uintptr_t *handle_irq(uintptr_t *regs, uintptr_t cause)
{
    // NMI
    if (cause & CAUSE_MACHINE_IRQ_MASK)
    {
#if defined(__GNUC__)
#pragma GCC diagnostic ignored "-Woverride-init"
#endif
        /* clang-format off */
	    static void(* const irq_table[])(
            uintptr_t *regs, uintptr_t cause) = {
	    	[0 ... 14]    = handle_irq_dummy,
	    	[IRQ_M_SOFT]  = handle_irq_m_soft,
	    	[IRQ_M_TIMER] = handle_irq_m_timer,
	    	[IRQ_M_EXT]   = handle_irq_m_ext,
	    };
        /* clang-format on */
#if defined(__GNUC__)
#pragma GCC diagnostic warning "-Woverride-init"
#endif
        irq_table[cause & CAUSE_HYPERVISOR_IRQ_REASON_MASK](regs, cause);
    }
    else if (cause > CAUSE_USER_ECALL)
    {
        handle_syscall(regs, cause);
    }
    else
    {
        handle_except(regs, cause);
    }

    return regs;
}
