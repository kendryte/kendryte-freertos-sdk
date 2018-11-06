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
#include <atomic>
#include <climits>
#include <cstring>
#include <errno.h>
#include <kernel/driver_impl.hpp>
#include <platform.h>
#include <pthread.h>
#include <semphr.h>
#include <task.h>

static const pthread_attr_t s_default_thread_attributes = {
    .stacksize = 4096,
    .schedparam = { .sched_priority = tskIDLE_PRIORITY },
    .detachstate = PTHREAD_CREATE_JOINABLE
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
        vTaskSuspendAll();

        /* Release xJoinBarrier and delete it. */
        (void)xSemaphoreGive(&k_thrd->join_barrier);
        vSemaphoreDelete(&k_thrd->join_barrier);

        /* Release xJoinMutex and delete it. */
        (void)xSemaphoreGive(&k_thrd->join_mutex);
        vSemaphoreDelete(&k_thrd->join_mutex);

        /* Delete the FreeRTOS task that ran the thread. */
        vTaskDelete(k_thrd->handle);

        /* Set the return value. */
        if (retval != NULL)
        {
            *retval = k_thrd->ret;
        }

        /* Free the thread object. */
        delete k_thrd;

        /* End the critical section. */
        xTaskResumeAll();
    }

    return iStatus;
}

pthread_t pthread_self(void)
{
    /* Return a reference to this pthread object, which is stored in the
     * FreeRTOS task tag. */
    return (uintptr_t)xTaskGetApplicationTaskTag(NULL);
}

//int pthread_cancel(pthread_t pthread)
//{
//    k_pthread *k_thrd = reinterpret_cast<k_pthread *>(pthread);
//
//    k_thrd->cancel();
//
//    return 0;
//}
