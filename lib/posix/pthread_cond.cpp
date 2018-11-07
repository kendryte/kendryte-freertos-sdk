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
#include <climits>
#include <cstring>
#include <errno.h>
#include <kernel/driver_impl.hpp>
#include <platform.h>
#include <pthread.h>
#include <semphr.h>
#include <task.h>

using namespace sys;

static const pthread_condattr_t s_default_cond_attributes = {
    .is_initialized = true,
    .clock = portMAX_DELAY
};

struct k_pthread_cond
{
    StaticSemaphore_t mutex;
    StaticSemaphore_t wait_semphr;
    uint32_t waiting_threads;

    k_pthread_cond() noexcept
        : waiting_threads(0)
    {
        xSemaphoreCreateMutexStatic(&mutex);
        xSemaphoreCreateCountingStatic(UINT_MAX, 0U, &wait_semphr);
    }

    ~k_pthread_cond()
    {
        vSemaphoreDelete(&mutex);
        vSemaphoreDelete(&wait_semphr);
    }

    semaphore_lock lock() noexcept
    {
        return { &mutex };
    }

    void give() noexcept
    {
        xSemaphoreGive(&wait_semphr);
        waiting_threads--;
    }
};

static void pthread_cond_init_if_static(pthread_cond_t *cond)
{
    if (*cond == PTHREAD_COND_INITIALIZER)
    {
        configASSERT(pthread_cond_init(cond, nullptr) == 0);
    }
}

int pthread_condattr_init(pthread_condattr_t *__attr)
{
    *__attr = s_default_cond_attributes;
    return 0;
}

int pthread_condattr_destroy(pthread_condattr_t *__attr)
{
    __attr->is_initialized = false;
    return 0;
}

int pthread_condattr_getclock(const pthread_condattr_t *__attr, clockid_t *__clock_id)
{
    *__clock_id = __attr->clock;
    return 0;
}

int pthread_condattr_setclock(pthread_condattr_t *__attr, clockid_t __clock_id)
{
    __attr->clock = __clock_id;
    return 0;
}

int pthread_condattr_getpshared(const pthread_condattr_t *__attr, int *__pshared)
{
    *__pshared = 1;
    return 0;
}

int pthread_condattr_setpshared(pthread_condattr_t *__attr, int __pshared)
{
    return 0;
}

int pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr)
{
    int iStatus = 0;
    k_pthread_cond *k_cond = nullptr;

    /* Silence warnings about unused parameters. */
    (void)attr;

    k_cond = new (std::nothrow) k_pthread_cond();

    if (!k_cond)
    {
        iStatus = ENOMEM;
    }

    if (iStatus == 0)
    {
        /* Set the output. */
        *cond = reinterpret_cast<uintptr_t>(k_cond);
    }

    return iStatus;
}

int pthread_cond_destroy(pthread_cond_t *cond)
{
    k_pthread_cond *k_cond = reinterpret_cast<k_pthread_cond *>(*cond);

    delete k_cond;
    return 0;
}

int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex)
{
    return pthread_cond_timedwait(cond, mutex, NULL);
}

int pthread_cond_signal(pthread_cond_t *cond)
{
    pthread_cond_init_if_static(cond);
    k_pthread_cond *k_cond = reinterpret_cast<k_pthread_cond *>(*cond);

    /* Check that at least one thread is waiting for a signal. */
    if (k_cond->waiting_threads)
    {
        /* Lock xCondMutex to protect access to iWaitingThreads.
         * This call will never fail because it blocks forever. */
        auto lock = k_cond->lock();
        xSemaphoreTake(&k_cond->mutex, portMAX_DELAY);

        /* Check again that at least one thread is waiting for a signal after
         * taking xCondMutex. If so, unblock it. */
        if (k_cond->waiting_threads)
            k_cond->give();
    }

    return 0;
}

int pthread_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex, const struct timespec *abstime)
{
    int iStatus = 0;
    pthread_cond_init_if_static(cond);
    k_pthread_cond *k_cond = reinterpret_cast<k_pthread_cond *>(*cond);
    TickType_t xDelay = portMAX_DELAY;

    /* Convert abstime to a delay in TickType_t if provided. */
    if (abstime != NULL)
    {
        xDelay = timespec_to_ticks(*abstime);
    }

    /* Increase the counter of threads blocking on condition variable, then
     * unlock mutex. */
    if (iStatus == 0)
    {
        {
            auto lock = k_cond->lock();
            k_cond->waiting_threads++;
        }

        iStatus = pthread_mutex_unlock(mutex);
    }

    /* Wait on the condition variable. */
    if (iStatus == 0)
    {
        if (xSemaphoreTake(&k_cond->wait_semphr, xDelay) == pdPASS)
        {
            /* When successful, relock mutex. */
            iStatus = pthread_mutex_lock(mutex);
        }
        else
        {
            /* Timeout. Relock mutex and decrement number of waiting threads. */
            iStatus = ETIMEDOUT;
            (void)pthread_mutex_lock(mutex);

            {
                auto lock = k_cond->lock();
                k_cond->waiting_threads--;
            }
        }
    }

    return iStatus;
}

int pthread_cond_broadcast(pthread_cond_t *cond)
{
    int i = 0;
    pthread_cond_init_if_static(cond);
    k_pthread_cond *k_cond = reinterpret_cast<k_pthread_cond *>(*cond);

    /* Lock xCondMutex to protect access to iWaitingThreads.
     * This call will never fail because it blocks forever. */
    auto locker = k_cond->lock();

    /* Unblock all threads waiting on this condition variable. */
    for (i = 0; i < k_cond->waiting_threads; i++)
    {
        xSemaphoreGive(&k_cond->wait_semphr);
    }

    /* All threads were unblocked, set waiting threads to 0. */
    k_cond->waiting_threads = 0;

    return 0;
}
