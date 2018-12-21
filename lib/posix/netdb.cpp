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
#include "sys/netdb.h"
#include "utils.h"
#include <kernel/driver_impl.hpp>
#include <network.h>
#include <string.h>
#include <sys/socket.h>

using namespace sys;

static struct hostent posix_hostent;
struct hostent *gethostbyname(const char *name)
{
    try
    {
        hostent_t sys_hostent;
        network_socket_gethostbyname(name, &sys_hostent);
        posix_hostent.h_name = sys_hostent.h_name;
        posix_hostent.h_aliases = sys_hostent.h_aliases;
        posix_hostent.h_length = sys_hostent.h_length;
        posix_hostent.h_addr_list = reinterpret_cast<char **>(sys_hostent.h_addr_list);
        switch (sys_hostent.h_addrtype)
        {
        case AF_INTERNETWORK:
            posix_hostent.h_addrtype = AF_INET;
            break;
        default:
            throw std::invalid_argument("Invalid address type.");
        }
        return &posix_hostent;
    }
    catch (...)
    {
        return NULL;
    }
    
}
