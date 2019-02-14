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
#include "dump.h"
#include "interrupt.h"
#include <encoding.h>
#include <stdlib.h>

void __attribute__((weak)) handle_misaligned_fetch(uintptr_t *regs, uintptr_t cause)
{
    dump_core("misaligned fetch", regs, cause);
    sys_exit(1337);
}

void __attribute__((weak)) handle_fault_fetch(uintptr_t *regs, uintptr_t cause)
{
    dump_core("fault fetch", regs, cause);
    sys_exit(1337);
}

void __attribute__((weak)) handle_illegal_instruction(uintptr_t *regs, uintptr_t cause)
{
    dump_core("illegal instruction", regs, cause);
    sys_exit(1337);
}

void __attribute__((weak)) handle_breakpoint(uintptr_t *regs, uintptr_t cause)
{
    dump_core("breakpoint", regs, cause);
    sys_exit(1337);
}

void __attribute__((weak)) handle_misaligned_load(uintptr_t *regs, uintptr_t cause)
{
    dump_core("misaligned load", regs, cause);
    sys_exit(1337);
}

void __attribute__((weak)) handle_fault_load(uintptr_t *regs, uintptr_t cause)
{
    dump_core("fault load", regs, cause);
    sys_exit(1337);
}

void __attribute__((weak)) handle_misaligned_store(uintptr_t *regs, uintptr_t cause)
{
    dump_core("misaligned store", regs, cause);
    sys_exit(1337);
}

void __attribute__((weak)) handle_fault_store(uintptr_t *regs, uintptr_t cause)
{
    dump_core("fault store", regs, cause);
    sys_exit(1337);
}

void handle_except(uintptr_t *regs, uintptr_t cause)
{
    static void (*const cause_table[])(uintptr_t * regs, uintptr_t cause) = {
        [CAUSE_MISALIGNED_FETCH] = handle_misaligned_fetch,
        [CAUSE_FAULT_FETCH] = handle_fault_fetch,
        [CAUSE_ILLEGAL_INSTRUCTION] = handle_illegal_instruction,
        [CAUSE_BREAKPOINT] = handle_breakpoint,
        [CAUSE_MISALIGNED_LOAD] = handle_misaligned_load,
        [CAUSE_FAULT_LOAD] = handle_fault_load,
        [CAUSE_MISALIGNED_STORE] = handle_misaligned_store,
        [CAUSE_FAULT_STORE] = handle_fault_store
    };

    cause_table[cause & CAUSE_HYPERVISOR_IRQ_REASON_MASK](regs, cause);
}
