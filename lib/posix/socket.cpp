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
#include "utils.h"
#include "sys/socket.h"
#include <network.h>
#include <kernel/driver_impl.hpp>

using namespace sys;

#define SOCKET_ENTRY                             \
    auto &obj = system_handle_to_object(socket);   \
    configASSERT(obj.is<network_socket>());      \
    auto f = obj.as<network_socket>();

#define CATCH_ALL \
    catch (...) { return -1; }

#define CHECK_ARG(x) \
    if (!x) throw std::invalid_argument(#x " is invalid.");

static void to_sys_sockaddr(socket_address_t &addr, const struct sockaddr &socket_addr)
{
    if (socket_addr.sa_family != AF_INET)
        throw std::runtime_error("Invalid socket address.");
    addr.data[0] = socket_addr.sa_data[2];
    addr.data[1] = socket_addr.sa_data[3];
    addr.data[2] = socket_addr.sa_data[4];
    addr.data[3] = socket_addr.sa_data[5];
    *reinterpret_cast<uint16_t *>(addr.data + 4) = ntohs(*reinterpret_cast<const uint16_t *>(socket_addr.sa_data));
}

int socket(int domain, int type, int protocol)
{
    try
    {
        address_family_t address_family;
        switch (domain)
        {
        case AF_INET:
            address_family = AF_INTERNETWORK;
            break;
        default:
            throw std::invalid_argument("Invalid domain.");
        }

        socket_type_t s_type;
        switch (type)
        {
        case SOCK_STREAM:
            s_type = SOCKET_STREAM;
            break;
        case SOCK_DGRAM:
            s_type = SOCKET_DATAGRAM;
            break;
        default:
            throw std::invalid_argument("Invalid type.");
        }

        protocol_type_t s_protocol;
        switch (protocol)
        {
        case IPPROTO_TCP:
            s_protocol = PROTCL_TCP;
            break;
        default:
            throw std::invalid_argument("Invalid protocol.");
        }

        return network_socket_open(address_family, s_type, s_protocol);
    }
    catch (...)
    {
        return NULL_HANDLE;
    }
}

int bind(int socket, const struct sockaddr *address, socklen_t address_len)
{
    try
    {
        SOCKET_ENTRY;
        CHECK_ARG(address);

        socket_address_t remote_addr;
        to_sys_sockaddr(remote_addr, *address);
        f->bind(remote_addr);
        return 0;
    }
    CATCH_ALL;
}
