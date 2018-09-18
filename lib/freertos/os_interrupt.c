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
#include <atomic.h>
#include <clint.h>
#include <core_sync.h>
#include <encoding.h>
#include <plic.h>
#include "portable/portmacro.h"
#include "task.h"

volatile BaseType_t xHigherPriorityTaskWoken = 0;
extern TaskHandle_t volatile xPendingAddReadyTCBs[portNUM_PROCESSORS];

volatile UBaseType_t xCoreSyncEvents[portNUM_PROCESSORS] = { 0 };
volatile UBaseType_t xWakeUp[portNUM_PROCESSORS] = { 1, 0 };

volatile int xx[99999];

uintptr_t handle_irq_m_soft(uintptr_t cause, uintptr_t epc, uintptr_t regs[32])
{
    xx[0] = 0;
    uint64_t hart_id = xPortGetProcessorId();
    switch (xCoreSyncEvents[hart_id])
    {
    case CORE_SYNC_ADD_TCB:
    {
        TaskHandle_t pxTCB = xPendingAddReadyTCBs[hart_id];
        if (pxTCB)
        {
            vAddNewTaskToCurrentReadyList(pxTCB);
            xPendingAddReadyTCBs[hart_id] = NULL;
        }
    }
    break;
    case CORE_SYNC_CONTEXT_SWITCH:
        configASSERT(!"Shouldn't process here");
        break;
    case CORE_SYNC_WAKE_UP:
        xWakeUp[hart_id] = 1;
        break;
    default:
        break;
    }

    core_sync_complete(hart_id);
    return epc;
}

void core_sync_complete(uint64_t hart_id)
{
    clint_ipi_clear(hart_id);
    xCoreSyncEvents[hart_id] = CORE_SYNC_NONE;
}

uintptr_t handle_irq_m_timer(uintptr_t cause, uintptr_t epc, uintptr_t regs[32])
{
    prvSetNextTimerInterrupt();

    /* Increment the RTOS tick. */
    if (xTaskIncrementTick() != pdFALSE)
    {
        core_sync_request_context_switch(xPortGetProcessorId());
    }
    return epc;
}

void core_sync_request_context_switch(uint64_t hart_id)
{
    while (1)
    {
        UBaseType_t old = atomic_cas(&xCoreSyncEvents[hart_id], CORE_SYNC_NONE, CORE_SYNC_CONTEXT_SWITCH);
        if (old == CORE_SYNC_NONE)
            break;
        else if (old == CORE_SYNC_CONTEXT_SWITCH)
            return;
    }

    clint_ipi_send(hart_id);
}

/*-----------------------------------------------------------*/

/* Sets the next timer interrupt
 * Reads previous timer compare register, and adds tickrate */
void prvSetNextTimerInterrupt(void)
{
    UBaseType_t xPsrId = xPortGetProcessorId();
    clint->mtimecmp[xPsrId] += (configTICK_CLOCK_HZ / configTICK_RATE_HZ);
}

void vPortNotifyProcessorAddNewTaskToReadyList(UBaseType_t xPsrId)
{
    while (atomic_cas(&xCoreSyncEvents[xPsrId], CORE_SYNC_NONE, CORE_SYNC_ADD_TCB) != CORE_SYNC_NONE)
        ;
    clint_ipi_send(xPsrId);
}

void vPortWakeUpProcessor(UBaseType_t xPsrId)
{
    while (atomic_cas(&xCoreSyncEvents[xPsrId], CORE_SYNC_NONE, CORE_SYNC_WAKE_UP) != CORE_SYNC_NONE)
        ;
    clint_ipi_send(xPsrId);
}

UBaseType_t xIsProcessorWakeUp(UBaseType_t xPsrId)
{
    return xWakeUp[xPsrId];
}
