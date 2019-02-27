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
#include "FreeRTOS.h"
#include "devices.h"
#include "kernel/driver_impl.hpp"
#include "network.h"
#include <lwip/sockets.h>
#include <lwip/errno.h>
#include <string.h>

using namespace sys;

static void check_lwip_error(int result)
{
    if (result < 0)
    {
        throw errno_exception(strerror(errno), errno);
    }
}

static void to_lwip_sockaddr(sockaddr_in &addr, const socket_address_t &socket_addr)
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

class k_network_socket : public network_socket, public heap_object, public exclusive_object_access
{
public:
    k_network_socket(address_family_t address_family, socket_type_t type, protocol_type_t protocol)
    {
        int domain;
        switch (address_family)
        {
        case AF_UNSPECIFIED:
        case AF_INTERNETWORK:
            domain = AF_INET;
            break;
        default:
            throw std::invalid_argument("Invalid address family.");
        }

        int s_type;
        switch (type)
        {
        case SOCKET_STREAM:
            s_type = SOCK_STREAM;
            break;
        case SOCKET_DATAGRAM:
            s_type = SOCK_DGRAM;
            break;
        default:
            throw std::invalid_argument("Invalid socket type.");
        }

        int s_protocol;
        switch (protocol)
        {
        case PROTCL_IP:
            s_protocol = IPPROTO_IP;
            break;
        default:
            throw std::invalid_argument("Invalid protocol type.");
        }

        auto sock = lwip_socket(domain, s_type, s_protocol);
        check_lwip_error(sock);
        sock_ = sock;
    }

    explicit k_network_socket(int sock)
        : sock_(sock)
    {
    }

    ~k_network_socket()
    {
        lwip_close(sock_);
    }

    virtual void install() override
    {
    }

    virtual object_accessor<network_socket> accept(socket_address_t *remote_address) override
    {
        object_ptr<k_network_socket> socket(std::in_place, new k_network_socket());

        sockaddr_in remote;
        socklen_t remote_len = sizeof(remote);

        auto sock = lwip_accept(sock_, reinterpret_cast<sockaddr *>(&remote), &remote_len);
        check_lwip_error(sock);
        socket->sock_ = sock;
        if (remote_address)
            to_sys_sockaddr(*remote_address, remote);
        return make_accessor(socket);
    }

    virtual void bind(const socket_address_t &address) override
    {
        sockaddr_in addr;
        to_lwip_sockaddr(addr, address);
        check_lwip_error(lwip_bind(sock_, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)));
    }

    virtual void connect(const socket_address_t &address) override
    {
        sockaddr_in addr;
        to_lwip_sockaddr(addr, address);
        check_lwip_error(lwip_connect(sock_, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)));
    }

    virtual void listen(uint32_t backlog) override
    {
        check_lwip_error(lwip_listen(sock_, backlog));
    }

    virtual void shutdown(socket_shutdown_t how) override
    {
        int s_how;
        switch (how)
        {
        case SOCKSHTDN_SEND:
            s_how = SHUT_WR;
            break;
        case SOCKSHTDN_RECEIVE:
            s_how = SHUT_RD;
            break;
        case SOCKSHTDN_BOTH:
            s_how = SHUT_RDWR;
            break;
        default:
            throw std::invalid_argument("Invalid how.");
        }

        check_lwip_error(lwip_shutdown(sock_, s_how));
    }

    virtual size_t send(gsl::span<const uint8_t> buffer, socket_message_flag_t flags) override
    {
        uint8_t send_flags = 0;
        if (flags & MESSAGE_PEEK)
            send_flags |= MSG_PEEK;
        if (flags & MESSAGE_WAITALL)
            send_flags |= MSG_WAITALL;
        if (flags & MESSAGE_OOB)
            send_flags |= MSG_OOB;
        if (flags & MESSAGE_DONTWAIT)
            send_flags |= MSG_DONTWAIT;
        if (flags & MESSAGE_MORE)
            send_flags |= MSG_MORE;

        auto ret = lwip_send(sock_, buffer.data(), buffer.size_bytes(), send_flags);
        check_lwip_error(ret);
        configASSERT(ret == buffer.size_bytes());
        return ret;
    }

    virtual size_t receive(gsl::span<uint8_t> buffer, socket_message_flag_t flags) override
    {
        uint8_t recv_flags = 0;
        if (flags & MESSAGE_PEEK)
            recv_flags |= MSG_PEEK;
        if (flags & MESSAGE_WAITALL)
            recv_flags |= MSG_WAITALL;
        if (flags & MESSAGE_OOB)
            recv_flags |= MSG_OOB;
        if (flags & MESSAGE_DONTWAIT)
            recv_flags |= MSG_DONTWAIT;
        if (flags & MESSAGE_MORE)
            recv_flags |= MSG_MORE;
        auto ret = lwip_recv(sock_, buffer.data(), buffer.size_bytes(), recv_flags);
        check_lwip_error(ret);
        return ret;
    }

    virtual size_t send_to(gsl::span<const uint8_t> buffer, socket_message_flag_t flags, const socket_address_t &to) override
    {
        uint8_t send_flags = 0;
        if (flags & MESSAGE_PEEK)
            send_flags |= MSG_PEEK;
        if (flags & MESSAGE_WAITALL)
            send_flags |= MSG_WAITALL;
        if (flags & MESSAGE_OOB)
            send_flags |= MSG_OOB;
        if (flags & MESSAGE_DONTWAIT)
            send_flags |= MSG_DONTWAIT;
        if (flags & MESSAGE_MORE)
            send_flags |= MSG_MORE;

        sockaddr_in remote;
        socklen_t remote_len = sizeof(remote);
        to_lwip_sockaddr(remote, to);

        auto ret = lwip_sendto(sock_, buffer.data(), buffer.size_bytes(), send_flags, reinterpret_cast<const sockaddr *>(&remote), remote_len);
        check_lwip_error(ret);
        configASSERT(ret == buffer.size_bytes());
        return ret;
    }

    virtual size_t receive_from(gsl::span<uint8_t> buffer, socket_message_flag_t flags, socket_address_t *from) override
    {
        uint8_t recv_flags = 0;
        if (flags & MESSAGE_PEEK)
            recv_flags |= MSG_PEEK;
        if (flags & MESSAGE_WAITALL)
            recv_flags |= MSG_WAITALL;
        if (flags & MESSAGE_OOB)
            recv_flags |= MSG_OOB;
        if (flags & MESSAGE_DONTWAIT)
            recv_flags |= MSG_DONTWAIT;
        if (flags & MESSAGE_MORE)
            recv_flags |= MSG_MORE;

        sockaddr_in remote;
        socklen_t remote_len = sizeof(remote);

        auto ret = lwip_recvfrom(sock_, buffer.data(), buffer.size_bytes(), recv_flags, reinterpret_cast<sockaddr *>(&remote), &remote_len);
        check_lwip_error(ret);
        if (from)
            to_sys_sockaddr(*from, remote);
        return ret;
    }

    virtual size_t read(gsl::span<uint8_t> buffer) override
    {
        auto ret = lwip_read(sock_, buffer.data(), buffer.size_bytes());
        check_lwip_error(ret);
        return ret;
    }

    virtual size_t write(gsl::span<const uint8_t> buffer) override
    {
        auto ret = lwip_write(sock_, buffer.data(), buffer.size_bytes());
        check_lwip_error(ret);
        configASSERT(ret == buffer.size_bytes());
        return ret;
    }

    virtual int fcntl(int cmd, int val) override
    {
        auto ret = lwip_fcntl(sock_, cmd, val);
        check_lwip_error(ret);
        return ret;
    }

    virtual void select(fd_set *readset, fd_set *writeset, fd_set *exceptset, struct timeval *timeout) override
    {
        check_lwip_error(lwip_select(sock_ + 1, readset, writeset, exceptset, timeout));
    }

    virtual int control(uint32_t control_code, gsl::span<const uint8_t> write_buffer, gsl::span<uint8_t> read_buffer) override
    {
        int val = *reinterpret_cast<const int *>(write_buffer.data());
        check_lwip_error(lwip_ioctl(sock_, (unsigned int)control_code, &val));
        return 0;
    }

private:
    k_network_socket()
        : sock_(0)
    {
    }

private:
    int sock_;
};

#define SOCKET_ENTRY                                    \
    auto &obj = system_handle_to_object(socket_handle); \
    configASSERT(obj.is<k_network_socket>());           \
    auto f = obj.as<k_network_socket>();

#define CATCH_ALL \
    catch (errno_exception &e)      \
    {                               \
        errno = e.code();           \
        return -1;                  \
    }

#define CHECK_ARG(x) \
    if (!x)          \
        throw std::invalid_argument(#x " is invalid.");

handle_t network_socket_open(address_family_t address_family, socket_type_t type, protocol_type_t protocol)
{
    try
    {
        auto socket = make_object<k_network_socket>(address_family, type, protocol);
        return system_alloc_handle(make_accessor<object_access>(socket));
    }
    catch (...)
    {
        return NULL_HANDLE;
    }
}

handle_t network_socket_close(handle_t socket_handle)
{
    return io_close(socket_handle);
}

int network_socket_connect(handle_t socket_handle, const socket_address_t *remote_address)
{
    try
    {
        SOCKET_ENTRY;
        CHECK_ARG(remote_address);

        f->connect(*remote_address);
        return 0;
    }
    CATCH_ALL;
}

int network_socket_listen(handle_t socket_handle, uint32_t backlog)
{
    try
    {
        SOCKET_ENTRY;

        f->listen(backlog);
        return 0;
    }
    CATCH_ALL;
}

handle_t network_socket_accept(handle_t socket_handle, socket_address_t *remote_address)
{
    try
    {
        SOCKET_ENTRY;
        CHECK_ARG(remote_address);

        return system_alloc_handle(f->accept(remote_address));
    }
    catch (...)
    {
        return NULL_HANDLE;
    }
}

int network_socket_shutdown(handle_t socket_handle, socket_shutdown_t how)
{
    try
    {
        SOCKET_ENTRY;

        f->shutdown(how);
        return 0;
    }
    CATCH_ALL;
}

int network_socket_bind(handle_t socket_handle, const socket_address_t *local_address)
{
    try
    {
        SOCKET_ENTRY;
        CHECK_ARG(local_address);

        f->bind(*local_address);
        return 0;
    }
    CATCH_ALL;
}

int network_socket_send(handle_t socket_handle, const uint8_t *data, size_t len, socket_message_flag_t flags)
{
    try
    {
        SOCKET_ENTRY;

        f->send({ data, std::ptrdiff_t(len) }, flags);
        return 0;
    }
    CATCH_ALL;
}

int network_socket_receive(handle_t socket_handle, uint8_t *data, size_t len, socket_message_flag_t flags)
{
    try
    {
        SOCKET_ENTRY;

        return f->receive({ data, std::ptrdiff_t(len) }, flags);
    }
    CATCH_ALL;
}

int network_socket_send_to(handle_t socket_handle, const uint8_t *data, size_t len, socket_message_flag_t flags, const socket_address_t *to)
{
    try
    {
        SOCKET_ENTRY;

        f->send_to({ data, std::ptrdiff_t(len) }, flags, *to);
        return 0;
    }
    CATCH_ALL;
}

int network_socket_receive_from(handle_t socket_handle, uint8_t *data, size_t len, socket_message_flag_t flags, socket_address_t *from)
{
    try
    {
        SOCKET_ENTRY;

        return f->receive_from({ data, std::ptrdiff_t(len) }, flags, from);
    }
    CATCH_ALL;
}

int network_socket_fcntl(handle_t socket_handle, int cmd, int val)
{
    try
    {
        SOCKET_ENTRY;

        return f->fcntl(cmd, val);
    }
    CATCH_ALL;
}

int network_socket_select(int socket_handle, fd_set *readset, fd_set *writeset, fd_set *exceptset, struct timeval *timeout)
{
    try
    {
        SOCKET_ENTRY;

        f->select(readset, writeset, exceptset, timeout);
        return 0;
    }
    CATCH_ALL;
}

int network_socket_addr_parse(const char *ip_addr, int port, uint8_t *socket_addr)
{
    try
    {
        const char *sep = ".";
        char *p;
        int data;
        char ip_addr_p[16];
        strcpy(ip_addr_p, ip_addr);
        uint8_t *socket_addr_p = socket_addr;
        p = strtok(ip_addr_p, sep);
        while (p)
        {
            data = atoi(p);
            if (data > 255)
                throw std::invalid_argument(" ipaddr is invalid.");
            *socket_addr_p++ = (uint8_t)data;
            p = strtok(NULL, sep);
        }
        if (socket_addr_p - socket_addr != 4)
            throw std::invalid_argument(" ipaddr size is invalid.");
        *socket_addr_p++ = port & 0xff;
        *socket_addr_p = (port >> 8) & 0xff;
        return 0;
    }
    CATCH_ALL;
}

int network_socket_addr_to_string(uint8_t *socket_addr, char *ip_addr, int *port)
{
    try
    {
        char *p = ip_addr;

        int i = 0;
        do
        {
            char tmp[8] = { 0 };
            itoa(socket_addr[i++], tmp, 10);
            strcpy(p, tmp);
            p += strlen(tmp);
        } while ((i < 4) && (*p++ = '.'));

        *port = (int)(socket_addr[4] | (socket_addr[5] << 8));
        return 0;
    }
    CATCH_ALL;
}
