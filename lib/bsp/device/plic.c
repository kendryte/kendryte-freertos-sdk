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
#include <plic.h>
#include <semphr.h>
#include <stdio.h>
#include <sysctl.h>

#define plic ((volatile struct plic_t*)PLIC_BASE_ADDR)

static void plic_install(void* userdata)
{
    int i = 0;
    size_t core_id;

    for (core_id = 0; core_id < PLIC_NUM_HARTS; core_id++)
    {
        /* Disable all interrupts for the current core. */
        for (i = 0; i < ((PLIC_NUM_SOURCES + 32u) / 32u); i++)
            plic->target_enables.target[core_id].enable[i] = 0;
    }

    /* Set priorities to zero. */
    for (i = 0; i < PLIC_NUM_SOURCES; i++)
        plic->source_priorities.priority[i] = 0;

    /* Set the threshold to zero. */
    for (core_id = 0; core_id < PLIC_NUM_HARTS; core_id++)
    {
        plic->targets.target[core_id].priority_threshold = 0;
    }

    /* Enable machine external interrupts. */
    set_csr(mie, MIP_MEIP);
}

static int plic_open(void* userdata)
{
    return 1;
}

static void plic_close(void* userdata)
{
}

static void plic_set_irq_enable(size_t irq, int enable, void* userdata)
{
    configASSERT(irq <= PLIC_NUM_SOURCES);

    /* Get current enable bit array by IRQ number */
    uint32_t current = plic->target_enables.target[0].enable[irq / 32];
    /* Set enable bit in enable bit array */
    if (enable)
        current |= (uint32_t)1 << (irq % 32);
    else
        current &= ~((uint32_t)1 << (irq % 32));
    /* Write back the enable bit array */
    plic->target_enables.target[0].enable[irq / 32] = current;
}

static void plic_set_irq_priority(size_t irq, size_t priority, void* userdata)
{
    configASSERT(irq <= PLIC_NUM_SOURCES);
    /* Set interrupt priority by IRQ number */
    plic->source_priorities.priority[irq] = priority;
}

static void plic_complete_irq(uint32_t source)
{
    unsigned long core_id = uxPortGetProcessorId();
    /* Perform IRQ complete */
    plic->targets.target[core_id].claim_complete = source;
}

/*Entry Point for PLIC Interrupt Handler*/
void handle_irq_m_ext(uintptr_t cause, uintptr_t epc)
{
    /**
     * After the highest-priority pending interrupt is claimed by a target
     * and the corresponding IP bit is cleared, other lower-priority
     * pending interrupts might then become visible to the target, and so
     * the PLIC EIP bit might not be cleared after a claim. The interrupt
     * handler can check the local meip/heip/seip/ueip bits before exiting
     * the handler, to allow more efficient service of other interrupts
     * without first restoring the interrupted context and taking another
     * interrupt trap.
     */
    if (read_csr(mip) & MIP_MEIP)
    {
        /* Get current core id */
        uint64_t core_id = read_csr(mhartid);
        uint64_t ie_flag = read_csr(mie);
        uint32_t int_num = plic->targets.target[core_id].claim_complete;
        uint32_t int_threshold = plic->targets.target[core_id].priority_threshold;

        plic->targets.target[core_id].priority_threshold = plic->source_priorities.priority[int_num];
        clear_csr(mie, MIP_MTIP | MIP_MSIP);
        set_csr(mstatus, MSTATUS_MIE);
        kernel_iface_pic_on_irq(int_num);
        plic_complete_irq(int_num);
        clear_csr(mstatus, MSTATUS_MIE);
        set_csr(mstatus, MSTATUS_MPIE | MSTATUS_MPP);
        write_csr(mie, ie_flag);
        plic->targets.target[core_id].priority_threshold = int_threshold;
    }
}

const pic_driver_t g_pic_driver_plic0 = {{NULL, plic_install, plic_open, plic_close}, plic_set_irq_enable, plic_set_irq_priority};
