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
#ifndef _FREERTOS_OBJECT_H
#define _FREERTOS_OBJECT_H

#include "osdefs.h"
#include <typeinfo>
#include <utility>

namespace sys
{
class access_denied_exception : public std::exception
{
public:
    using exception::exception;
};

class object
{
public:
    virtual void add_ref() = 0;
    virtual bool release() = 0;
};

template <class T>
class object_ptr
{
public:
    constexpr object_ptr(nullptr_t = nullptr) noexcept
        : obj_(nullptr)
    {
    }

    constexpr object_ptr(std::in_place_t in, T *obj) noexcept
        : obj_(obj)
    {
    }

    object_ptr(T *obj) noexcept
        : obj_(obj)
    {
        add_ref();
    }

    object_ptr(const object_ptr &other) noexcept
        : obj_(other.obj_)
    {
        add_ref();
    }

    object_ptr(const object_ptr &&other) noexcept
        : obj_(other.obj_)
    {
        other.obj_ = nullptr;
    }

    template <class... Args>
    object_ptr(std::in_place_t, Args &&... args)
        : obj_(new T(std::forward<Args>(args)...))
    {
    }

    template <class U, typename = std::enable_if_t<std::is_convertible_v<U *, T *>>>
    object_ptr(const object_ptr<U> &other) noexcept
        : obj_(other.obj_)
    {
        add_ref();
    }

    template <class U, typename = std::enable_if_t<std::is_convertible_v<U *, T *>>>
    object_ptr(object_ptr<U> &&other) noexcept
        : obj_(other.obj_)
    {
        other.obj_ = nullptr;
    }

    template <class U, typename = std::enable_if_t<std::is_convertible_v<U, T>>>
    object_ptr &operator=(const object_ptr<U> &other) noexcept
    {
        reset(other.obj_);
        return *this;
    }

    template <class U, typename = std::enable_if_t<std::is_convertible_v<U, T>>>
    object_ptr &operator=(object_ptr<U> &&other) noexcept
    {
        reset(other.obj_);
        other.obj_ = nullptr;
        return *this;
    }

    object_ptr &operator=(const object_ptr &other) noexcept
    {
        reset(other.obj_);
        return *this;
    }

    object_ptr &operator=(object_ptr &&other) noexcept
    {
        if (obj_ != other.obj_)
        {
            release();
            obj_ = other.obj_;
            other.obj_ = nullptr;
        }

        return *this;
    }

    operator bool() const noexcept
    {
        return obj_;
    }

    ~object_ptr()
    {
        reset();
    }

    void reset(T *obj = nullptr) noexcept
    {
        if (obj != obj_)
        {
            release();
            obj_ = obj;
            add_ref();
        }
    }

    T *get() const noexcept { return obj_; }

    T *operator->() const noexcept { return get(); }
    T &operator*() const noexcept { return *obj_; }

    template <class U>
    object_ptr<U> as() const noexcept
    {
        auto ptr = dynamic_cast<U *>(obj_);
        return object_ptr<U>(ptr);
    }

    template <class U>
    bool is() const noexcept
    {
        auto ptr = dynamic_cast<U *>(obj_);
        return ptr;
    }

private:
    void add_ref() noexcept
    {
        if (obj_)
            obj_->add_ref();
    }

    void release() noexcept
    {
        if (obj_)
            obj_->release();
        obj_ = nullptr;
    }

private:
    template <class U>
    friend class object_ptr;

    T *obj_;
};

template <typename T, typename... Args>
object_ptr<T> make_object(Args &&... args)
{
    return object_ptr<T>(std::in_place, std::forward<Args>(args)...);
}
}

#endif /* _FREERTOS_OBJECT_H */