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
#include <atomic>
#include <climits>
#include <cstring>
#include <errno.h>
#include <kernel/driver_impl.hpp>
#include <platform.h>
#include <pthread.h>
#include <semphr.h>
#include <task.h>
#include <unordered_map>

// Workaround for keeping pthread functions
void *g_pthread_keep[] = {
    (void *)pthread_cond_init,
    (void *)pthread_mutex_init,
    (void *)pthread_self
};

static const pthread_attr_t s_default_thread_attributes = {
    .stacksize = 4096*8,
    .schedparam = { .sched_priority = tskIDLE_PRIORITY },
    .detachstate = PTHREAD_CREATE_JOINABLE
};

struct k_pthread_key
{
    void (*destructor)(void *);
};

struct k_pthread_tls
{
    std::unordered_map<pthread_key_t, uintptr_t> storage;
};

struct k_pthread
{
    pthread_attr_t attr;
    StaticSemaphore_t join_mutex;
    StaticSemaphore_t join_barrier;
    void *(*startroutine)(void *);
    void *arg;
    TaskHandle_t handle;
    void *ret;

    k_pthread(pthread_attr_t attr, void *(*startroutine)(void *), void *arg) noexcept
        : attr(attr), startroutine(startroutine), arg(arg)
    {
        if (attr.detachstate == PTHREAD_CREATE_JOINABLE)
        {
            xSemaphoreCreateMutexStatic(&join_mutex);
            xSemaphoreCreateBinaryStatic(&join_barrier);
        }
    }

    BaseType_t create() noexcept
    {
        auto ret = xTaskCreate(thread_thunk, "posix", (uint16_t)(attr.stacksize / sizeof(StackType_t)), this, attr.schedparam.sched_priority, &handle);
        if (ret == pdPASS)
        {
            /* Store the pointer to the thread object in the task tag. */
            vTaskSetApplicationTaskTag(handle, (TaskHookFunction_t)this);
        }

        return ret;
    }

    void cancel() noexcept
    {
        vTaskSuspendAll();
        on_exit();
        xTaskResumeAll();
    }

private:
    static void thread_thunk(void *arg)
    {
        k_pthread *k_thread = reinterpret_cast<k_pthread *>(arg);
        k_thread->ret = k_thread->startroutine(k_thread->arg);

        k_thread->on_exit();
    }

    void on_exit()
    {
        /* If this thread is joinable, wait for a call to pthread_join. */
        if (attr.detachstate == PTHREAD_CREATE_JOINABLE)
        {
            xSemaphoreGive(&join_barrier);
            /* Suspend until the call to pthread_join. The caller of pthread_join
             * will perform cleanup. */
            vTaskSuspend(NULL);
        }
        else
        {
            /* For a detached thread, perform cleanup of thread object. */
            delete this;
            delete reinterpret_cast<k_pthread_tls *>(pvTaskGetThreadLocalStoragePointer(NULL, PTHREAD_TLS_INDEX));
            vTaskDelete(NULL);
        }
    }
};

int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*startroutine)(void *), void *arg)
{
    int iStatus = 0;
    k_pthread *k_thrd = NULL;

    /* Allocate memory for new thread object. */
    k_thrd = new (std::nothrow) k_pthread(attr ? *attr : s_default_thread_attributes, startroutine, arg);

    if (!k_thrd)
    {
        /* No memory. */
        iStatus = EAGAIN;
    }

    if (iStatus == 0)
    {
        /* Suspend all tasks to create a critical section. This ensures that
         * the new thread doesn't exit before a tag is assigned. */
        vTaskSuspendAll();

        /* Create the FreeRTOS task that will run the pthread. */
        if (k_thrd->create() != pdPASS)
        {
            /* Task creation failed, no memory. */
            delete k_thrd;
            iStatus = EAGAIN;
        }
        else
        {
            /* Set the thread object for the user. */
            *thread = reinterpret_cast<uintptr_t>(k_thrd);
        }

        /* End the critical section. */
        xTaskResumeAll();
    }

    return iStatus;
}

int pthread_join(pthread_t pthread, void **retval)
{
    int iStatus = 0;
    k_pthread *k_thrd = reinterpret_cast<k_pthread *>(pthread);

    /* Make sure pthread is joinable. Otherwise, this function would block
     * forever waiting for an unjoinable thread. */
    if (k_thrd->attr.detachstate != PTHREAD_CREATE_JOINABLE)
    {
        iStatus = EDEADLK;
    }

    /* Only one thread may attempt to join another. Lock the join mutex
     * to prevent other threads from calling pthread_join on the same thread. */
    if (iStatus == 0)
    {
        if (xSemaphoreTake(&k_thrd->join_mutex, 0) != pdPASS)
        {
            /* Another thread has already joined the requested thread, which would
             * cause this thread to wait forever. */
            iStatus = EDEADLK;
        }
    }

    /* Attempting to join the calling thread would cause a deadlock. */
    if (iStatus == 0)
    {
        if (pthread_equal(pthread_self(), pthread) != 0)
        {
            iStatus = EDEADLK;
        }
    }

    if (iStatus == 0)
    {
        /* Wait for the joining thread to finish. Because this call waits forever,
         * it should never fail. */
        (void)xSemaphoreTake(&k_thrd->join_barrier, portMAX_DELAY);

        /* Create a critical section to clean up the joined thread. */
        //vTaskSuspendAll();

        /* Release xJoinBarrier and delete it. */
        (void)xSemaphoreGive(&k_thrd->join_barrier);
        vSemaphoreDelete(&k_thrd->join_barrier);

        /* Release xJoinMutex and delete it. */
        (void)xSemaphoreGive(&k_thrd->join_mutex);
        vSemaphoreDelete(&k_thrd->join_mutex);

        /* Set the return value. */
        if (retval != NULL)
        {
            *retval = k_thrd->ret;
        }

        /* Free the thread object. */
        delete reinterpret_cast<k_pthread_tls *>(pvTaskGetThreadLocalStoragePointer(k_thrd->handle, PTHREAD_TLS_INDEX));
        /* Delete the FreeRTOS task that ran the thread. */
        vTaskDelete(k_thrd->handle);
        delete k_thrd;

        /* End the critical section. */
        //xTaskResumeAll();
    }

    return iStatus;
}

pthread_t pthread_self(void)
{
    /* Return a reference to this pthread object, which is stored in the
     * FreeRTOS task tag. */
    return (uintptr_t)xTaskGetApplicationTaskTag(NULL);
}

int pthread_cancel(pthread_t pthread)
{
    k_pthread *k_thrd = reinterpret_cast<k_pthread *>(pthread);

    k_thrd->cancel();

    return 0;
}

int pthread_key_create(pthread_key_t *__key, void (*__destructor)(void *))
{
    auto k_key = new (std::nothrow) k_pthread_key;
    if (k_key)
    {
        k_key->destructor = __destructor;

        *__key = reinterpret_cast<uintptr_t>(k_key);
        return 0;
    }

    return ENOMEM;
}

int pthread_key_delete(pthread_key_t key)
{
    auto k_key = reinterpret_cast<k_pthread_key *>(key);
    delete k_key;
    return 0;
}

void *pthread_getspecific(pthread_key_t key)
{
    auto tls = reinterpret_cast<k_pthread_tls *>(pvTaskGetThreadLocalStoragePointer(NULL, PTHREAD_TLS_INDEX));

    if (tls)
    {
        auto it = tls->storage.find(key);
        if (it != tls->storage.end())
            return reinterpret_cast<void *>(it->second);
    }

    return nullptr;
}

int pthread_setspecific(pthread_key_t key, const void *value)
{
    auto tls = reinterpret_cast<k_pthread_tls *>(pvTaskGetThreadLocalStoragePointer(NULL, PTHREAD_TLS_INDEX));
    if (!tls)
    {
        tls = new (std::nothrow) k_pthread_tls;
        if (!tls)
            return ENOMEM;
        vTaskSetThreadLocalStoragePointer(NULL, PTHREAD_TLS_INDEX, tls);
    }

    try
    {
        tls->storage[key] = uintptr_t(value);
        return 0;
    }
    catch (...)
    {
        return ENOMEM;
    }
}

int pthread_once(pthread_once_t *once_control, void (*init_routine)(void))
{
    while (true)
    {
        if (atomic_read(&once_control->init_executed) == 1)
            return 0;

        if (atomic_cas(&once_control->init_executed, 0, 2) == 0)
            break;
    }

    init_routine();
    atomic_set(&once_control->init_executed, 1);
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
