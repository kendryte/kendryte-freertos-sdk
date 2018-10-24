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
#include <memory>

namespace sys
{
class object
{
public:
    virtual void add_ref() = 0;
    virtual bool release() = 0;
};

template<class T>
class object_ptr
{
public:
    constexpr object_ptr(nullptr_t = nullptr) noexcept
        : obj_(nullptr)
    {
    }

    object_ptr(std::in_place_t, T* obj) noexcept
        : obj_(obj)
    {
    }
};
}

#endif /* _FREERTOS_OBJECT_H */