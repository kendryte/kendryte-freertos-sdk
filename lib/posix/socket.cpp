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
#include "sys/socket.h"
#include "utils.h"
#include <kernel/driver_impl.hpp>
#include <network.h>
#include <string.h>
#include <sys/select.h>

using namespace sys;

#define SOCKET_ENTRY                             \
    auto &obj = system_handle_to_object(socket); \
    configASSERT(obj.is<network_socket>());      \
    auto f = obj.as<network_socket>();

#define CATCH_ALL               \
    catch (errno_exception & e) \
    {                           \
        errno = e.code();       \
        return -1;              \
    }

#define CHECK_ARG(x) \
    if (!x)          \
        throw std::invalid_argument(#x " is invalid.");

static void to_posix_sockaddr(sockaddr_in &addr, const socket_address_t &socket_addr)
{
    if (socket_addr.family != AF_INTERNETWORK)
        throw std::runtime_error("Invalid socket address.");

    addr.sin_len = sizeof(addr);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(*reinterpret_cast<const uint16_t *>(socket_addr.data + 4));
    addr.sin_addr.s_addr = LWIP_MAKEU32(socket_addr.data[3], socket_addr.data[2], socket_addr.data[1], socket_addr.data[0]);
}

static void to_sys_sockaddr(socket_address_t &addr, const sockaddr_in &socket_addr)
{
    if (socket_addr.sin_family != AF_INET)
        throw std::runtime_error("Invalid socket address.");

    addr.family = AF_INTERNETWORK;
    addr.data[3] = (socket_addr.sin_addr.s_addr >> 24) & 0xFF;
    addr.data[2] = (socket_addr.sin_addr.s_addr >> 16) & 0xFF;
    addr.data[1] = (socket_addr.sin_addr.s_addr >> 8) & 0xFF;
    addr.data[0] = socket_addr.sin_addr.s_addr & 0xFF;
    *reinterpret_cast<uint16_t *>(addr.data + 4) = ntohs(socket_addr.sin_port);
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
        case IPPROTO_IP:
            s_protocol = PROTCL_IP;
            break;
        case IPPROTO_TCP:
            s_protocol = PROTCL_IP;
            break;
        case IPPROTO_UDP:
            s_protocol = PROTCL_IP;
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

        socket_address_t local_addr;
        to_sys_sockaddr(local_addr, *(reinterpret_cast<const sockaddr_in *>(address)));
        f->bind(local_addr);
        return 0;
    }
    CATCH_ALL;
}

int accept(int socket, struct sockaddr *address, socklen_t *address_len)
{
    try
    {
        SOCKET_ENTRY;
        CHECK_ARG(address);

        socket_address_t remote_addr;
        auto ret = f->accept(&remote_addr);

        sockaddr_in *addr = reinterpret_cast<sockaddr_in *>(address);
        to_posix_sockaddr(*addr, remote_addr);

        return system_alloc_handle(std::move(ret));
    }
    CATCH_ALL;
}

int shutdown(int socket, int how)
{
    try
    {
        SOCKET_ENTRY;
        f->shutdown((socket_shutdown_t)how);
        return 0;
    }
    CATCH_ALL;
}

int connect(int socket, const struct sockaddr *address, socklen_t address_len)
{
    try
    {
        SOCKET_ENTRY;
        CHECK_ARG(address);

        socket_address_t remote_addr;
        to_sys_sockaddr(remote_addr, *reinterpret_cast<const sockaddr_in *>(address));
        f->connect(remote_addr);
        return 0;
    }
    CATCH_ALL;
}

int listen(int socket, int backlog)
{
    try
    {
        SOCKET_ENTRY;

        f->listen(backlog);
        return 0;
    }
    CATCH_ALL;
}

int recv(int socket, void *mem, size_t len, int flags)
{
    try
    {
        socket_message_flag_t recv_flags = MESSAGE_NORMAL;
        if (flags & MSG_PEEK)
            recv_flags |= MESSAGE_PEEK;
        if (flags & MSG_WAITALL)
            recv_flags |= MESSAGE_WAITALL;
        if (flags & MSG_OOB)
            recv_flags |= MESSAGE_OOB;
        if (flags & MSG_DONTWAIT)
            recv_flags |= MESSAGE_DONTWAIT;
        if (flags & MSG_MORE)
            recv_flags |= MESSAGE_MORE;

        SOCKET_ENTRY;

        return f->receive({ (uint8_t *)mem, std::ptrdiff_t(len) }, recv_flags);
    }
    CATCH_ALL;
}

int send(int socket, const void *data, size_t size, int flags)
{
    try
    {
        socket_message_flag_t send_flags = MESSAGE_NORMAL;
        if (flags & MSG_PEEK)
            send_flags |= MESSAGE_PEEK;
        if (flags & MSG_WAITALL)
            send_flags |= MESSAGE_WAITALL;
        if (flags & MSG_OOB)
            send_flags |= MESSAGE_OOB;
        if (flags & MSG_DONTWAIT)
            send_flags |= MESSAGE_DONTWAIT;
        if (flags & MSG_MORE)
            send_flags |= MESSAGE_MORE;

        SOCKET_ENTRY;

        f->send({ (const uint8_t *)data, std::ptrdiff_t(size) }, send_flags);
        return 0;
    }
    CATCH_ALL;
}

int recvfrom(int socket, void *mem, size_t len, int flags, struct sockaddr *from, socklen_t *fromlen)
{
    try
    {
        socket_message_flag_t recv_flags = MESSAGE_NORMAL;
        if (flags & MSG_PEEK)
            recv_flags |= MESSAGE_PEEK;
        if (flags & MSG_WAITALL)
            recv_flags |= MESSAGE_WAITALL;
        if (flags & MSG_OOB)
            recv_flags |= MESSAGE_OOB;
        if (flags & MSG_DONTWAIT)
            recv_flags |= MESSAGE_DONTWAIT;
        if (flags & MSG_MORE)
            recv_flags |= MESSAGE_MORE;

        SOCKET_ENTRY;

        socket_address_t remote_addr;
        auto ret = f->receive_from({ (uint8_t *)mem, std::ptrdiff_t(len) }, recv_flags, &remote_addr);

        sockaddr_in *addr = reinterpret_cast<sockaddr_in *>(from);
        to_posix_sockaddr(*addr, remote_addr);
        return ret;
    }
    CATCH_ALL;
}

int sendto(int socket, const void *data, size_t size, int flags, const struct sockaddr *to, socklen_t tolen)
{
    try
    {
        socket_message_flag_t send_flags = MESSAGE_NORMAL;
        if (flags & MSG_PEEK)
            send_flags |= MESSAGE_PEEK;
        if (flags & MSG_WAITALL)
            send_flags |= MESSAGE_WAITALL;
        if (flags & MSG_OOB)
            send_flags |= MESSAGE_OOB;
        if (flags & MSG_DONTWAIT)
            send_flags |= MESSAGE_DONTWAIT;
        if (flags & MSG_MORE)
            send_flags |= MESSAGE_MORE;

        SOCKET_ENTRY;

        socket_address_t remote_addr;
        to_sys_sockaddr(remote_addr, *reinterpret_cast<const sockaddr_in *>(to));

        f->send_to({ (const uint8_t *)data, std::ptrdiff_t(size) }, send_flags, remote_addr);
        return 0;
    }
    CATCH_ALL;
}

int select(int maxfdp1, fd_set *readset, fd_set *writeset, fd_set *exceptset, struct timeval *timeout)
{
    try
    {
        int socket = maxfdp1 - 1;
        SOCKET_ENTRY;

        f->select(readset, writeset, exceptset, timeout);
        return 0;
    }
    CATCH_ALL;
}
