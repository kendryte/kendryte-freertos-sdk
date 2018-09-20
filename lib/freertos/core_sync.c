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

volatile UBaseType_t g_core_sync_events[portNUM_PROCESSORS] = { 0 };
static volatile TaskHandle_t s_pending_to_add_tasks[portNUM_PROCESSORS] = { 0 };
static volatile UBaseType_t s_core_awake[portNUM_PROCESSORS] = { 1, 0 };

void handle_irq_m_soft(uintptr_t cause, uintptr_t epc)
{
    uint64_t core_id = uxPortGetProcessorId();
    switch (g_core_sync_events[core_id])
    {
    case CORE_SYNC_ADD_TCB:
    {
        TaskHandle_t newTask = atomic_read(&s_pending_to_add_tasks[core_id]);
        if (newTask)
        {
            vAddNewTaskToCurrentReadyList(newTask);
            atomic_set(&s_pending_to_add_tasks[core_id], NULL);
        }
    }
    break;
    case CORE_SYNC_CONTEXT_SWITCH:
        configASSERT(!"Shouldn't process here");
        break;
    case CORE_SYNC_WAKE_UP:
        atomic_set(&s_core_awake[core_id], 1);
        break;
    default:
        break;
    }

    core_sync_complete(core_id);
}

void handle_irq_m_timer(uintptr_t cause, uintptr_t epc)
{
    prvSetNextTimerInterrupt();

    /* Increment the RTOS tick. */
    if (xTaskIncrementTick() != pdFALSE)
    {
        core_sync_request_context_switch(uxPortGetProcessorId());
    }
}

void core_sync_request_context_switch(uint64_t core_id)
{
    while (1)
    {
        core_sync_event_t old = atomic_cas(&g_core_sync_events[core_id], CORE_SYNC_NONE, CORE_SYNC_CONTEXT_SWITCH);
        if (old == CORE_SYNC_NONE)
            break;
        else if (old == CORE_SYNC_CONTEXT_SWITCH)
            return;
    }

    clint_ipi_send(core_id);
}

void core_sync_complete(uint64_t core_id)
{
    clint_ipi_clear(core_id);
    atomic_set(&g_core_sync_events[core_id], CORE_SYNC_NONE);
}

int core_sync_is_awake(UBaseType_t uxPsrId)
{
    return !!atomic_read(&s_core_awake[uxPsrId]);
}

void core_sync_awaken(UBaseType_t uxPsrId)
{
    while (atomic_cas(&g_core_sync_events[uxPsrId], CORE_SYNC_NONE, CORE_SYNC_WAKE_UP) != CORE_SYNC_NONE)
        ;
    clint_ipi_send(uxPsrId);
}

void vPortAddNewTaskToReadyListAsync(UBaseType_t uxPsrId, void* pxNewTaskHandle)
{
    // Wait for last adding tcb complete
    while (atomic_read(&s_pending_to_add_tasks[uxPsrId]));
    atomic_set(&s_pending_to_add_tasks[uxPsrId], pxNewTaskHandle);

    while (atomic_cas(&g_core_sync_events[uxPsrId], CORE_SYNC_NONE, CORE_SYNC_ADD_TCB) != CORE_SYNC_NONE)
        ;
    clint_ipi_send(uxPsrId);
}
