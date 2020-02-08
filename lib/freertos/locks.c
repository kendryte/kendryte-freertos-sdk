#include "FreeRTOS.h"
#include "portmacro.h"
#include "semphr.h"
#include "task.h"
#include <stdlib.h>
#include <sys/lock.h>

typedef long _lock_t;
static void lock_init_generic(_lock_t *lock, uint8_t mutex_type)
{
    portENTER_CRITICAL();
    if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED)
    {
        /* nothing to do until the scheduler is running */
        portEXIT_CRITICAL();
        return;
    }

    if (!*lock)
    {
        xSemaphoreHandle new_sem = xQueueCreateMutex(mutex_type);
        if (!new_sem)
            abort();
        *lock = (_lock_t)new_sem;
    }

    portEXIT_CRITICAL();
}

void _lock_init(_lock_t *lock)
{
    *lock = 0;
    lock_init_generic(lock, queueQUEUE_TYPE_MUTEX);
}

void _lock_init_recursive(_lock_t *lock)
{
    *lock = 0;
    lock_init_generic(lock, queueQUEUE_TYPE_RECURSIVE_MUTEX);
}

void _lock_close(_lock_t *lock)
{
    portENTER_CRITICAL();
    if (*lock)
    {
        xSemaphoreHandle h = (xSemaphoreHandle)(*lock);
        configASSERT(xSemaphoreGetMutexHolder(h) == NULL);
        vSemaphoreDelete(h);
        *lock = 0;
    }

    portEXIT_CRITICAL();
}

void _lock_close_recursive(_lock_t *lock) __attribute__((alias("_lock_close")));

static int lock_acquire_generic(_lock_t *lock, uint32_t delay, uint8_t mutex_type)
{
    xSemaphoreHandle h = (xSemaphoreHandle)(*lock);
    if (!h)
    {
        if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED)
            return 0; /* locking is a no-op before scheduler is up, so this "succeeds" */

        lock_init_generic(lock, mutex_type);
        h = (xSemaphoreHandle)(*lock);
        configASSERT(h != NULL);
    }

    BaseType_t success;
    if (uxPortIsInISR())
    {
        /* In ISR Context */
        if (mutex_type == queueQUEUE_TYPE_RECURSIVE_MUTEX)
        {
            vPortDebugBreak();
            abort(); /* recursive mutexes make no sense in ISR context */
        }

        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        success = xSemaphoreTakeFromISR(h, &xHigherPriorityTaskWoken);
        if (!success && delay > 0)
        {
            vPortDebugBreak();
            abort(); /* Tried to block on mutex from ISR, couldn't... rewrite your program to avoid libc interactions in ISRs! */
        }

        if (xHigherPriorityTaskWoken)
            portYIELD_FROM_ISR();
    }
    else
    {
        if (mutex_type == queueQUEUE_TYPE_RECURSIVE_MUTEX)
            success = xSemaphoreTakeRecursive(h, delay);
        else
            success = xSemaphoreTake(h, delay);
    }

    return (success == pdTRUE) ? 0 : -1;
}

void _lock_acquire(_lock_t *lock)
{
    lock_acquire_generic(lock, 200, queueQUEUE_TYPE_MUTEX);
}

void _lock_acquire_recursive(_lock_t *lock)
{
    lock_acquire_generic(lock, 200, queueQUEUE_TYPE_RECURSIVE_MUTEX);
}

int _lock_try_acquire(_lock_t *lock)
{
    return lock_acquire_generic(lock, 0, queueQUEUE_TYPE_MUTEX);
}

int _lock_try_acquire_recursive(_lock_t *lock)
{
    return lock_acquire_generic(lock, 0, queueQUEUE_TYPE_RECURSIVE_MUTEX);
}

static void lock_release_generic(_lock_t *lock, uint8_t mutex_type)
{
    xSemaphoreHandle h = (xSemaphoreHandle)(*lock);
    if (h == NULL)
    {
        return;
    }

    if (uxPortIsInISR())
    {
        if (mutex_type == queueQUEUE_TYPE_RECURSIVE_MUTEX)
        {
            vPortDebugBreak();
            abort(); /* recursive mutexes make no sense in ISR context */
        }

        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xSemaphoreGiveFromISR(h, &xHigherPriorityTaskWoken);
        if (xHigherPriorityTaskWoken)
            portYIELD_FROM_ISR();
    }
    else
    {
        if (mutex_type == queueQUEUE_TYPE_RECURSIVE_MUTEX)
        {
            xSemaphoreGiveRecursive(h);
        }
        else
        {
            xSemaphoreGive(h);
        }
    }
}

void _lock_release(_lock_t *lock)
{
    lock_release_generic(lock, queueQUEUE_TYPE_MUTEX);
}

void _lock_release_recursive(_lock_t *lock)
{
    lock_release_generic(lock, queueQUEUE_TYPE_RECURSIVE_MUTEX);
}
