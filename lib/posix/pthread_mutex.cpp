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
#include "utils.h"
#include <FreeRTOS.h>
#include <atomic>
#include <cstring>
#include <errno.h>
#include <memory>
#include <platform.h>
#include <pthread.h>
#include <semphr.h>
#include <task.h>

#include <stdio.h>

struct k_pthread_mutex
{
    pthread_mutexattr_t attr;
    StaticSemaphore_t semphr;
    TaskHandle_t owner;

    k_pthread_mutex(pthread_mutexattr_t attr) noexcept
        : attr(attr)
    {
        if (attr.type == PTHREAD_MUTEX_RECURSIVE)
            xSemaphoreCreateRecursiveMutexStatic(&semphr);
        else
            xSemaphoreCreateMutexStatic(&semphr);
    }

    void give() noexcept
    {
        if (attr.type == PTHREAD_MUTEX_RECURSIVE)
            xSemaphoreGiveRecursive(&semphr);
        else
            xSemaphoreGive(&semphr);
    }

    void update_owner() noexcept
    {
        owner = xSemaphoreGetMutexHolder(&semphr);
    }
};

static void pthread_mutex_init_if_static(pthread_mutex_t *mutex)
{
    if (*mutex == PTHREAD_MUTEX_INITIALIZER)
    {
        configASSERT(pthread_mutex_init(mutex, nullptr) == 0);
    }
}

int pthread_mutex_lock(pthread_mutex_t *mutex)
{
    return pthread_mutex_timedlock(mutex, NULL);
}

int pthread_mutex_timedlock(pthread_mutex_t *mutex, const struct timespec *abstime)
{
    if (*mutex == PTHREAD_MUTEX_INITIALIZER)
        return 0;

    int iStatus = 0;
    k_pthread_mutex *k_mutex = reinterpret_cast<k_pthread_mutex *>(*mutex);
    TickType_t xDelay = portMAX_DELAY;
    BaseType_t xFreeRTOSMutexTakeStatus = pdFALSE;

    /* Convert abstime to a delay in TickType_t if provided. */
    if (abstime != NULL)
        xDelay = timespec_to_ticks(*abstime);

    /* Check if trying to lock a currently owned mutex. */
    if ((iStatus == 0) && (k_mutex->attr.type == PTHREAD_MUTEX_ERRORCHECK) && /* Only PTHREAD_MUTEX_ERRORCHECK type detects deadlock. */
        (k_mutex->owner == xTaskGetCurrentTaskHandle())) /* Check if locking a currently owned mutex. */
    {
        iStatus = EDEADLK;
    }

    if (iStatus == 0)
    {
        /* Call the correct FreeRTOS mutex take function based on mutex type. */
        if (k_mutex->attr.type == PTHREAD_MUTEX_RECURSIVE)
        {
            xFreeRTOSMutexTakeStatus = xSemaphoreTakeRecursive(&k_mutex->semphr, xDelay);
        }
        else
        {
            xFreeRTOSMutexTakeStatus = xSemaphoreTake(&k_mutex->semphr, xDelay);
        }

        /* If the mutex was successfully taken, set its owner. */
        if (xFreeRTOSMutexTakeStatus == pdPASS)
        {
            k_mutex->owner = xTaskGetCurrentTaskHandle();
        }
        /* Otherwise, the mutex take timed out. */
        else
        {
            iStatus = ETIMEDOUT;
        }
    }

    return iStatus;
}

int pthread_mutex_unlock(pthread_mutex_t *mutex)
{
    if (*mutex == PTHREAD_MUTEX_INITIALIZER)
        return 0;

    int iStatus = 0;
    k_pthread_mutex *k_mutex = reinterpret_cast<k_pthread_mutex *>(*mutex);

    /* Check if trying to unlock an unowned mutex. */
    if (((k_mutex->attr.type == PTHREAD_MUTEX_ERRORCHECK) || (k_mutex->attr.type == PTHREAD_MUTEX_RECURSIVE)) && (k_mutex->owner != xTaskGetCurrentTaskHandle()))
    {
        iStatus = EPERM;
    }

    if (iStatus == 0)
    {
        /* Call the correct FreeRTOS mutex unlock function based on mutex type. */
        k_mutex->give();

        /* Update the owner of the mutex. A recursive mutex may still have an
         * owner, so it should be updated with xSemaphoreGetMutexHolder. */
        k_mutex->update_owner();
    }

    return iStatus;
}

int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr)
{
    int iStatus = 0;
    k_pthread_mutex *k_mutex = nullptr;

    pthread_mutexattr_t k_attr;
    memset(&k_attr, 0, sizeof(k_attr));
    if (attr)
        k_attr = *attr;

    k_mutex = new (std::nothrow) k_pthread_mutex(k_attr);

    if (!k_mutex)
    {
        iStatus = ENOMEM;
    }

    if (iStatus == 0)
    {
        /* Set the output. */
        *mutex = reinterpret_cast<uintptr_t>(k_mutex);
    }

    return iStatus;
}

int pthread_mutex_destroy(pthread_mutex_t *mutex)
{
    printf("pthread_mutex_destroy\n");
    k_pthread_mutex *k_mutex = reinterpret_cast<k_pthread_mutex *>(*mutex);

    /* Free resources in use by the mutex. */
    if (k_mutex->owner == NULL)
    {
        delete k_mutex;
    }

    return 0;
}

int pthread_equal(pthread_t t1, pthread_t t2)
{
    int iStatus = 0;

    /* Compare the thread IDs. */
    if (t1 && t2)
    {
        iStatus = (t1 == t2);
    }

    return iStatus;
}