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
#include "sys/ioctl.h"
#include "utils.h"
#include <kernel/driver_impl.hpp>
#include <network.h>
#include <string.h>
#include <sys/socket.h>

using namespace sys;

#define CATCH_ALL \
    catch (errno_exception &e)      \
    {                               \
        errno = e.code();           \
        return -1;                  \
    }

int ioctl(int handle, unsigned int cmd, void *argp)
{
    try
    {
        auto &obj = system_handle_to_object(handle); \
        configASSERT(obj.is<custom_driver>());      \
        auto f = obj.as<custom_driver>();

        const int val = *(int *)argp;
        f->control((uint32_t)cmd, { (const uint8_t *)&val, 4L }, { NULL, 0L });

        return 0;
    }
    CATCH_ALL;
}

