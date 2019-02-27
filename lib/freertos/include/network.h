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
#ifndef _FREERTOS_NETWORK_H
#define _FREERTOS_NETWORK_H

#include "osdefs.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief       Initialize network
 *
 * @return      result
 *     - 0      Success
 *     - other  Fail
 */
int network_init();

/**
 * @brief       Add network interface
 *
 * @param[in]   adapter_handle      The network adapter handle
 * @param[in]   ip_address          The network ip addr
 * @param[in]   net_mask            The network mask
 * @param[in]   gateway             The network gateway
 *
 * @return      result
 *     - 0      Fail
 *     - other  The network driver handle
 */
handle_t network_interface_add(handle_t adapter_handle, const ip_address_t *ip_address, const ip_address_t *net_mask, const ip_address_t *gateway);

/**
 * @brief       Enable network interface
 *
 * @param[in]   netif_handle        The network driver handle
 * @param[in]   enable              interface up or down
 *
 * @return      result
 *     - 0      Success
 *     - other  Fail
 */
int network_interface_set_enable(handle_t netif_handle, bool enable);

/**
 * @brief       Set a network interface as the default network interface
 *
 * @param[in]   netif_handle        The network driver handle
 *
 * @return      result
 *     - 0      Success
 *     - other  Fail
 */
int network_interface_set_as_default(handle_t netif_handle);

/**
 * @brief       Change the IP address of a network interface
 *
 * @param[in]   netif_handle        The network driver handle
 * @param[in]   ip_address          The network ip addr
 * @param[in]   net_mask            The network mask
 * @param[in]   gateway             The network gateway
 *
 * @return      result
 *     - 0      Success
 *     - other  Fail
 */
int network_set_addr(handle_t netif_handle, const ip_address_t *ip_address, const ip_address_t *net_mask, const ip_address_t *gateway);

/**
 * @brief       Get the IP address of a network interface
 *
 * @param[in]   netif_handle        The network driver handle
 * @param[out]  ip_address          The network ip addr
 * @param[out]  net_mask            The network mask
 * @param[out]  gateway             The network gateway
 *
 * @return      result
 *     - 0      Success
 *     - other  Fail
 */
int network_get_addr(handle_t netif_handle, ip_address_t *ip_address, ip_address_t *net_mask, ip_address_t *gateway);

/**
 * @brief       Get IP address through DHCP
 *
 * @param[in]   netif_handle        The network driver handle
 *
 * @return                      result
 *     - DHCP_ADDRESS_ASSIGNED  Success
 *     - other                  Fail
 */
dhcp_state_t network_interface_dhcp_pooling(handle_t netif_handle);

/**
 * @brief       Open network socket and returns a socket handle
 *
 * @param[in]   address_family      The protocol family which will be used for communication
 *                                  Currently defined family are:
 *                                  AF_INTERNETWORK     IPv4 Internet protocols
 * @param[in]   type                The socket has the indicated type, which specifies the communication semantics.
 *                                  Currently defined type are:
 *                                  SOCKET_STREAM       Connect based byte streams
 *                                  SOCKET_DATAGRAM     Supports datagrams
 * @param[in]   protocol            The protocol specifies a particular protocol to be used with the socket
 *                                  Currently defined protocol are:
 *                                  PROTCL_IP           For IP
 *
 * @return      result
 *     - 0      Success
 *     - other  Fail
 */
handle_t network_socket_open(address_family_t address_family, socket_type_t type, protocol_type_t protocol);

/**
 * @brief       Close a socket handle
 *
 * @param[in]   socket_handle       The network driver handle
 *
 * @return      result
 *     - 0      Success
 *     - other  Fail
 */
handle_t network_socket_close(handle_t socket_handle);

/**
 * @brief       Bind a address to a socket
 *
 * @param[in]   socket_handle       The network driver handle
 * @param[in]   local_address       Bind a local address
 *
 * @return      result
 *     - 0      Success
 *     - other  Fail
 */
int network_socket_bind(handle_t socket_handle, const socket_address_t *local_address);

/**
 * @brief       Initiate a connection on a socket
 *
 * @param[in]   socket_handle       The network driver handle
 * @param[in]   remote_address      Send to remote address
 *
 * @return      result
 *     - 0      Success
 *     - other  Fail
 */
int network_socket_connect(handle_t socket_handle, const socket_address_t *remote_address);

/**
 * @brief       Listen for connections on a socket
 *
 * @param[in]   socket_handle       The network driver handle
 * @param[in]   backlog             The backlog argument defines the maximum length to which
 *                                  the queue of pending connections for sockfd may grow.
 *
 * @return      result
 *     - 0      Success
 *     - other  Fail
 */
int network_socket_listen(handle_t socket_handle, uint32_t backlog);

/**
 * @brief       Accept a connection on a socket
 *
 * @param[in]   socket_handle       The network driver handle
 * @param[out]  remote_address      It is filled in with the address of the peer socket
 *
 * @return      result
 *     - 0      Success
 *     - other  Fail
 */
handle_t network_socket_accept(handle_t socket_handle, socket_address_t *remote_address);

/**
 * @brief       Shut down part of a full-duplex connection
 *
 * @param[in]   socket_handle       The network driver handle
 * @param[in]   how                 SOCKSHTDN_SEND      :further transmissions will be disallowed.
 *                                  SOCKSHTDN_RECEIVE   :further receptions will be disallowed.
 *                                  SOCKSHTDN_BOTH      :further transmissions and receptions will be disallowed.
 *
 * @return      result
 *     - 0      Success
 *     - other  Fail
 */
int network_socket_shutdown(handle_t socket_handle, socket_shutdown_t how);

/**
 * @brief       Send a message on a socket
 *
 * @param[in]   socket_handle       The network driver handle
 * @param[in]   data                The address of message data
 * @param[in]   len                 The message data length
 * @param[in]   flags               The  flags  argument is the bitwise OR of zero or more of the following flags
 *
 * @return      result
 *     - 0      Success
 *     - other  Fail
 */
int network_socket_send(handle_t socket_handle, const uint8_t *data, size_t len, socket_message_flag_t flags);

/**
 * @brief       Receive a message from a socket
 *
 * @param[in]   socket_handle       The network driver handle
 * @param[out]  data                The address of message data
 * @param[in]   len                 The message data length
 * @param[in]   flags               The  flags  argument is the bitwise OR of zero or more of the following flags
 *
 * @return      result
 *     - 0      Success
 *     - other  Fail
 */
int network_socket_receive(handle_t socket_handle, uint8_t *data, size_t len, socket_message_flag_t flags);

/**
 * @brief       Send a message on a socket
 *
 * @param[in]   socket_handle       The network driver handle
 * @param[in]   data                The address of message data
 * @param[in]   len                 The message data length
 * @param[in]   flags               The  flags argument is the bitwise OR of zero or more of the following flags
 * @param[in]   to                  The address of remote socket
 *
 * @return      result
 *     - 0      Success
 *     - other  Fail
 */
int network_socket_send_to(handle_t socket_handle, const uint8_t *data, size_t len, socket_message_flag_t flags, const socket_address_t *to);

/**
 * @brief       Receive a message from a socket
 *
 * @param[in]   socket_handle       The network driver handle
 * @param[out]  data                The address of message data
 * @param[in]   len                 The message data length
 * @param[in]   flags               The  flags  argument is the bitwise OR of zero or more of the following flags
 *
 * @return      result
 *     - 0      Success
 *     - other  Fail
 */
int network_socket_receive_from(handle_t socket_handle, uint8_t *data, size_t len, socket_message_flag_t flags, socket_address_t *from);
int network_socket_fcntl(handle_t socket_handle, int cmd, int val);
int network_socket_select(handle_t socket_handle, fd_set *readset, fd_set *writeset, fd_set *exceptset, struct timeval *timeout);

/**
 * @brief       Get host address information by name
 *
 * @param[in]   name                host name
 * @param[out]  hostent             host entry
 *
 * @return      result
 *     - 0      Success
 *     - other  Fail
 */
int network_socket_gethostbyname(const char *name, hostent_t *hostent);

/**
 * @brief       Socket addr parse
 *
 * @param[in]   ip_addr             ip address
 * @param[in]   port                port number
 * @param[out]  socket_addr         socket address
 *
 * @return      result
 *     - 0      Success
 *     - other  Fail
 */
int network_socket_addr_parse(const char *ip_addr, int port, uint8_t *socket_addr);

/**
 * @brief       Socket addr to string
 *
 * @param[in]   socket_addr         socket address
 * @param[out]  ip_addr             ip address
 * @param[out]  port                port number
 *
 * @return      result
 *     - 0      Success
 *     - other  Fail
 */
int network_socket_addr_to_string(uint8_t *socket_addr, char *ip_addr, int *port);

#ifdef __cplusplus
}
#endif

#endif /* _FREERTOS_NETWORK_H */