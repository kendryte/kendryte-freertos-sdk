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
#ifndef _FREERTOS_DRIVER_IMPL_H
#define _FREERTOS_DRIVER_IMPL_H

#include "driver.hpp"
#include <atomic>

namespace sys
{
class static_object : public virtual object
{
public:
    virtual void add_ref() override;
    virtual bool release() override;
};

class heap_object : public virtual object
{
public:
    heap_object() noexcept;
    virtual ~heap_object() = default;

    virtual void add_ref() override;
    virtual bool release() override;

private:
    std::atomic<size_t> ref_count_;
};

class free_object_access : public virtual object_access
{
public:
    free_object_access() noexcept;

    virtual void open() override;
    virtual void close() override;

protected:
    virtual void on_first_open();
    virtual void on_last_close();

private:
    std::atomic<size_t> used_count_;
};

class exclusive_object_access : public virtual object_access
{
public:
    exclusive_object_access() noexcept;

    virtual void open() override;
    virtual void close() override;

protected:
    virtual void on_first_open();
    virtual void on_last_close();

private:
    std::atomic_flag used_;
};

class semaphore_lock
{
public:
    semaphore_lock(SemaphoreHandle_t semaphore) noexcept;
    semaphore_lock(semaphore_lock &) = delete;
    semaphore_lock &operator=(semaphore_lock &) = delete;
    ~semaphore_lock();

private:
    SemaphoreHandle_t semaphore_;
};
}

#endif /* _FREERTOS_DRIVER_H */