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
#include <stdlib.h>

extern "C"
{
    void *_aligned_malloc(size_t size, size_t alignment);
    void _aligned_free(void *ptr);
}

void *_aligned_malloc(size_t size, size_t alignment)
{
    auto offset = alignment - 1 + sizeof(void *);
    auto p_head = malloc(size + offset);
    if (p_head)
    {
        auto p_ret = (uintptr_t(p_head) + offset) & ~(alignment - 1);
        auto p_link = reinterpret_cast<void **>(p_ret - sizeof(void *));
        *p_link = p_head;
        return reinterpret_cast<void *>(p_ret);
    }

    return nullptr;
}

void _aligned_free(void *ptr)
{
    if (ptr)
    {
        auto puc = uintptr_t(ptr);
        auto p_link = reinterpret_cast<void **>(puc - sizeof(void *));
        free(*p_link);
    }
}
