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
#include "syscalls.h"
#include <kernel/driver_impl.hpp>
#include <devices.h>
#include <filesystem.h>
#include <string.h>
#include <sys/file.h>
#include <sys/unistd.h>

using namespace sys;

int sys_open(const char *name, int flags, int mode)
{
    if (strstr(name, "/dev/") != NULL)
    {
        return io_open(name);
    }
    else if (strstr(name, "/fs/") != NULL)
    {
        file_access_t file_access = FILE_ACCESS_READ;
        if (flags == O_WRONLY)
            file_access = FILE_ACCESS_WRITE;
        else if (flags == O_RDWR)
            file_access = FILE_ACCESS_READ_WRITE;

        file_mode_t file_mode = FILE_MODE_OPEN_EXISTING;
        if (mode & O_CREAT)
            file_mode |= FILE_MODE_CREATE_ALWAYS;
        if (mode & O_APPEND)
            file_mode |= FILE_MODE_APPEND;
        if (mode & O_TRUNC)
            file_mode |= FILE_MODE_CREATE_NEW;
        return filesystem_file_open(name, file_access, file_mode);
    }
    else
    {
        return -EINVAL;
    }
}

off_t sys_lseek(int fildes, off_t offset, int whence)
{
    try
    {
        auto &obj = system_handle_to_object(fildes);
        if (auto f = obj.as<filesystem_file>())
        {
            if (whence == SEEK_SET)
            {
                f->set_position(offset);
            }
            else if (whence == SEEK_CUR)
            {
                auto pos = f->get_position();
                f->set_position(pos + offset);
            }
            else if (whence == SEEK_END)
            {
                auto pos = f->get_size();
                f->set_position(pos - offset);
            }

            return f->get_position();
        }

        return -1;
    }
    catch (...)
    {
        return -1;
    }
}
