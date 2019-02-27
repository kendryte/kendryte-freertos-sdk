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
#include "FreeRTOS.h"
#include "task.h"
#include <atomic.h>
#include <clint.h>
#include <core_sync.h>
#include <encoding.h>
#include <plic.h>

extern volatile uintptr_t g_wake_address;

static volatile core_sync_event_t s_core_sync_events[portNUM_PROCESSORS];
static corelock_t s_core_sync_locks[portNUM_PROCESSORS] = { CORELOCK_INIT, CORELOCK_INIT };
static volatile TaskHandle_t s_pending_to_add_tasks[portNUM_PROCESSORS];

void handle_irq_m_soft(uintptr_t *regs, uintptr_t cause)
{
    uint64_t core_id = uxPortGetProcessorId();
    clint_ipi_clear(core_id);
    switch (s_core_sync_events[core_id])
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
    case CORE_SYNC_SWITCH_CONTEXT:
        vTaskSwitchContext();
        break;
    default:
        break;
    }

    core_sync_complete(core_id);
}

void core_sync_request(uint64_t core_id, int event)
{
    vTaskEnterCritical();

    corelock_lock(&s_core_sync_locks[core_id]);
    while (s_core_sync_events[core_id] != CORE_SYNC_NONE)
        ;
    s_core_sync_events[core_id] = event;
    clint_ipi_send(core_id);
    corelock_unlock(&s_core_sync_locks[core_id]);
    vTaskExitCritical();
}

void core_sync_complete(uint64_t core_id)
{
    atomic_set(&s_core_sync_events[core_id], CORE_SYNC_NONE);
}

void core_sync_awaken(uintptr_t address)
{
    g_wake_address = address;
}

void vPortAddNewTaskToReadyListAsync(UBaseType_t core_id, void *pxNewTaskHandle)
{
    corelock_lock(&s_core_sync_locks[core_id]);
    while (s_core_sync_events[core_id] != CORE_SYNC_NONE)
        ;
    s_pending_to_add_tasks[core_id] = pxNewTaskHandle;
    s_core_sync_events[core_id] = CORE_SYNC_ADD_TCB;
    clint_ipi_send(core_id);
    corelock_unlock(&s_core_sync_locks[core_id]);
}
