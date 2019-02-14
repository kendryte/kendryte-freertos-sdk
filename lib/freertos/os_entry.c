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
#include "core_sync.h"
#include "kernel/device_priv.h"
#include "task.h"
#include <clint.h>
#include <encoding.h>
#include <fpioa.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct
{
    int (*user_main)(int, char **);
    int ret;
} main_thunk_param_t;

extern void __libc_init_array(void);
extern void __libc_fini_array(void);

static StaticTask_t s_idle_task[portNUM_PROCESSORS];
static StackType_t s_idle_task_stack[portNUM_PROCESSORS][configMINIMAL_STACK_SIZE];
static StaticTask_t s_timer_task[portNUM_PROCESSORS];
static StackType_t s_timer_task_stack[portNUM_PROCESSORS][configMINIMAL_STACK_SIZE];

void start_scheduler(int core_id);

int __attribute__((weak)) configure_fpioa()
{
    return 0;
}

static void main_thunk(void *p)
{
    /* Register finalization function */
    atexit(__libc_fini_array);
    /* Init libc array for C++ */
    __libc_init_array();

    install_hal();
    install_drivers();
    configure_fpioa();

    main_thunk_param_t *param = (main_thunk_param_t *)p;
    param->ret = param->user_main(0, 0);
}

static void os_entry_core1()
{
    clear_csr(mie, MIP_MTIP);
    clint_ipi_enable();
    set_csr(mstatus, MSTATUS_MIE);

    vTaskStartScheduler();
}

int os_entry(int (*user_main)(int, char **))
{
    clear_csr(mie, MIP_MTIP);
    clint_ipi_enable();
    set_csr(mstatus, MSTATUS_MIE);

    TaskHandle_t mainTask;
    main_thunk_param_t param = {};
    param.user_main = user_main;

    if (xTaskCreate(main_thunk, "Core 0 Main", configMAIN_TASK_STACK_SIZE, &param, configMAIN_TASK_PRIORITY, &mainTask) != pdPASS)
    {
        return -1;
    }

    core_sync_awaken((uintptr_t)os_entry_core1);
    vTaskStartScheduler();
    return param.ret;
}

void vApplicationIdleHook(void)
{
}

void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize)
{
    UBaseType_t uxPsrId = uxPortGetProcessorId();
    /* Pass out a pointer to the StaticTask_t structure in which the Idle task's
    state will be stored. */
    *ppxIdleTaskTCBBuffer = &s_idle_task[uxPsrId];

    /* Pass out the array that will be used as the Idle task's stack. */
    *ppxIdleTaskStackBuffer = s_idle_task_stack[uxPsrId];

    /* Pass out the size of the array pointed to by *ppxIdleTaskStackBuffer.
    Note that, as the array is necessarily of type StackType_t,
    configMINIMAL_STACK_SIZE is specified in words, not bytes. */
    *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}

void vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimerTaskTCBBuffer, StackType_t **ppxTimerTaskStackBuffer, uint32_t *pulTimerTaskStackSize)
{
    UBaseType_t uxPsrId = uxPortGetProcessorId();
    /* Pass out a pointer to the StaticTask_t structure in which the Idle task's
    state will be stored. */
    *ppxTimerTaskTCBBuffer = &s_timer_task[uxPsrId];

    /* Pass out the array that will be used as the Idle task's stack. */
    *ppxTimerTaskStackBuffer = s_timer_task_stack[uxPsrId];

    /* Pass out the size of the array pointed to by *ppxIdleTaskStackBuffer.
    Note that, as the array is necessarily of type StackType_t,
    configMINIMAL_STACK_SIZE is specified in words, not bytes. */
    *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    configASSERT(!"Stackoverflow !");
}
