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

using namespace sys;

static void check_lwip_error(int result)
{
    if (result < 0)
        throw std::runtime_error(strerror(errno));
}

static void to_lwip_sockaddr(sockaddr_in &addr, const socket_address_t &socket_addr)
{
    if (socket_addr.family != AF_INTERNETWORK)
        throw std::runtime_error("Invalid socket address.");

    addr.sin_len = sizeof(addr);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(*reinterpret_cast<const uint16_t *>(socket_addr.data + 4));
    addr.sin_addr.s_addr = LWIP_MAKEU32(socket_addr.data[0], socket_addr.data[1], socket_addr.data[2], socket_addr.data[3]);
}

static void to_sys_sockaddr(socket_address_t &addr, const sockaddr_in &socket_addr)
{
    if (socket_addr.sin_family != AF_INET)
        throw std::runtime_error("Invalid socket address.");
    addr.data[0] = (socket_addr.sin_addr.s_addr >> 24) & 0xFF;
    addr.data[1] = (socket_addr.sin_addr.s_addr >> 16) & 0xFF;
    addr.data[2] = (socket_addr.sin_addr.s_addr >> 8) & 0xFF;
    addr.data[3] = socket_addr.sin_addr.s_addr & 0xFF;
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
        case PROTCL_TCP:
            s_protocol = IPPROTO_TCP;
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

    virtual size_t send(gsl::span<const uint8_t> buffer) override
    {
        auto ret = lwip_write(sock_, buffer.data(), buffer.size_bytes());
        check_lwip_error(ret);
        configASSERT(ret == buffer.size_bytes());
        return ret;
    }

    virtual size_t receive(gsl::span<uint8_t> buffer) override
    {
        auto ret = lwip_read(sock_, buffer.data(), buffer.size_bytes());
        check_lwip_error(ret);
        return ret;
    }

private:
    k_network_socket()
        : sock_(0)
    {
    }

private:
    int sock_;
};

#define SOCKET_ENTRY                                      \
    auto &obj = system_handle_to_object(socket_handle);   \
    configASSERT(obj.is<k_network_socket>());             \
    auto f = obj.as<k_network_socket>();

#define CATCH_ALL \
    catch (...) { return -1; }

#define CHECK_ARG(x) \
    if (!x) throw std::invalid_argument(#x " is invalid.");

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

int network_socket_connect(handle_t socket_handle, const socket_address_t* remote_address)
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

handle_t network_socket_accept(handle_t socket_handle, socket_address_t* remote_address)
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

int network_socket_bind(handle_t socket_handle, const socket_address_t* remote_address)
{
    try
    {
        SOCKET_ENTRY;
        CHECK_ARG(remote_address);

        f->bind(*remote_address);
        return 0;
    }
    CATCH_ALL;
}
