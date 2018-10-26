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
#include "kernel/driver_impl.hpp"

using namespace sys;

void static_object::add_ref()
{
}

bool static_object::release()
{
    return false;
}

heap_object::heap_object() noexcept
    : ref_count_(1)
{
}

void heap_object::add_ref()
{
    ref_count_.fetch_add(1, std::memory_order_relaxed);
}

bool heap_object::release()
{
    if (ref_count_.fetch_sub(1, std::memory_order_relaxed) == 1)
    {
        delete this;
        return true;
    }

    return false;
}

free_object_access::free_object_access() noexcept
    : used_count_(0)
{
}

void free_object_access::open()
{
    if (used_count_.fetch_add(1, std::memory_order_relaxed) == 0)
        on_first_open();
}

void free_object_access::close()
{
    if (used_count_.fetch_sub(1, std::memory_order_relaxed) == 1)
        on_last_close();
}

void free_object_access::on_first_open()
{
}

void free_object_access::on_last_close()
{
}

exclusive_object_access::exclusive_object_access() noexcept
    : used_(ATOMIC_FLAG_INIT)
{
}

void exclusive_object_access::open()
{
    if (used_.test_and_set(std::memory_order_acquire))
        throw access_denied_exception();
    else
        on_first_open();
}

void exclusive_object_access::on_first_open()
{
}

void exclusive_object_access::on_last_close()
{
}

void exclusive_object_access::close()
{
    on_last_close();
    used_.clear(std::memory_order_release);
}

semaphore_lock::semaphore_lock(SemaphoreHandle_t semaphore) noexcept
    : semaphore_(semaphore)
{
    configASSERT(xSemaphoreTake(semaphore, portMAX_DELAY) == pdTRUE);
}

semaphore_lock::~semaphore_lock()
{
    xSemaphoreGive(semaphore_);
}
