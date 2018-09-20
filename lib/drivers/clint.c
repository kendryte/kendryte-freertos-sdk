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
#include <stddef.h>
#include <stdint.h>
#include "encoding.h"
#include "clint.h"
#include "sysctl.h"

volatile struct clint_t* const clint = (volatile struct clint_t*)CLINT_BASE_ADDR;

int clint_ipi_init(void)
{
    /* Clear the Machine-Software bit in MIE */
    clear_csr(mie, MIP_MSIP);
    return 0;
}

int clint_ipi_enable(void)
{
    /* Enable interrupts in general */
    set_csr(mstatus, MSTATUS_MIE);
    /* Set the Machine-Software bit in MIE */
    set_csr(mie, MIP_MSIP);
    return 0;
}

int clint_ipi_disable(void)
{
    /* Clear the Machine-Software bit in MIE */
    clear_csr(mie, MIP_MSIP);
    return 0;
}

int clint_ipi_send(size_t core_id)
{
    if (core_id >= CLINT_NUM_HARTS)
        return -1;
    clint->msip[core_id].msip = 1;
    return 0;
}

int clint_ipi_clear(size_t core_id)
{
    if (core_id >= CLINT_NUM_HARTS)
        return -1;
    if (clint->msip[core_id].msip)
    {
        clint->msip[core_id].msip = 0;
        return 1;
    }
    return 0;
}
