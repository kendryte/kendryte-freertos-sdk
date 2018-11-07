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

static StaticTask_t s_idle_task;
static StackType_t s_idle_task_stack[configMINIMAL_STACK_SIZE];

void start_scheduler(int core_id);

static void main_thunk(void *p)
{
    main_thunk_param_t *param = (main_thunk_param_t *)p;
    param->ret = param->user_main(0, 0);
}

void enable_core(int core_id)
{
    core_sync_awaken(core_id);
}

int __attribute__((weak)) configure_fpioa()
{
    return 0;
}

int os_entry(int core_id, int number_of_cores, int (*user_main)(int, char **))
{
    clear_csr(mie, MIP_MTIP);
    clint_ipi_enable();
    set_csr(mstatus, MSTATUS_MIE);

    if (core_id == 0)
    {
        install_hal();
        install_drivers();
        configure_fpioa();

        TaskHandle_t mainTask;
        main_thunk_param_t param = {};
        param.user_main = user_main;

        if (xTaskCreate(main_thunk, "Core 0 Main", configMAIN_TASK_STACK_SIZE, &param, configMAIN_TASK_PRIORITY, &mainTask) != pdPASS)
        {
            return -1;
        }

        enable_core(1);
        start_scheduler(core_id);
        return param.ret;
    }
    else
    {
        while (!core_sync_is_awake(core_id))
        {
            asm volatile("wfi");
        }

        start_scheduler(core_id);
        return 0;
    }
}

void start_scheduler(int core_id)
{
    vTaskStartScheduler();
}

void vApplicationIdleHook(void)
{
}

void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize)
{
    /* Pass out a pointer to the StaticTask_t structure in which the Idle task's
    state will be stored. */
    *ppxIdleTaskTCBBuffer = &s_idle_task;

    /* Pass out the array that will be used as the Idle task's stack. */
    *ppxIdleTaskStackBuffer = s_idle_task_stack;

    /* Pass out the size of the array pointed to by *ppxIdleTaskStackBuffer.
    Note that, as the array is necessarily of type StackType_t,
    configMINIMAL_STACK_SIZE is specified in words, not bytes. */
    *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    configASSERT(!"Stackoverflow !");
}
