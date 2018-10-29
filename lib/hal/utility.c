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
#include "clint.h"
#include "sysctl.h"
#include "utility.h"
#include <printf.h>

uint32_t get_bit_mask(volatile uint32_t* bits, uint32_t mask)
{
    return (*bits) & mask;
}

void set_bit_mask(volatile uint32_t* bits, uint32_t mask, uint32_t value)
{
    uint32_t org = (*bits) & ~mask;
    *bits = org | (value & mask);
}

uint32_t get_bit_idx(volatile uint32_t* bits, uint32_t idx)
{
    return ((*bits) & (1 << idx)) >> idx;
}

void set_bit_idx(volatile uint32_t* bits, uint32_t idx, uint32_t value)
{
    uint32_t org = (*bits) & ~(1 << idx);
    *bits = org | (value << idx);
}

void busy_wait(uint64_t millionseconds)
{
    uint64_t clint_time = clint->mtime;
    uint64_t nop_all = millionseconds * (sysctl_clock_get_freq(SYSCTL_CLOCK_CPU) / CLINT_CLOCK_DIV / 1000000UL) + 1;

    while (clint->mtime - clint_time < nop_all)
        ;
}
