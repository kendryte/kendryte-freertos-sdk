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

int network_init();

handle_t network_interface_add(handle_t adapter_handle, const ip_address_t *ip_address, const ip_address_t *net_mask, const ip_address_t *gateway);

int network_interface_set_enable(handle_t netif_handle, bool enable);

int network_interface_set_as_default(handle_t netif_handle);

int network_set_addr(handle_t netif_handle, const ip_address_t *ip_address, const ip_address_t *net_mask, const ip_address_t *gateway);

int network_get_addr(handle_t netif_handle, ip_address_t *ip_address, ip_address_t *net_mask, ip_address_t *gateway);

dhcp_state_t network_interface_dhcp_pooling(handle_t netif_handle);

handle_t network_socket_open(address_family_t address_family, socket_type_t type, protocol_type_t protocol);

handle_t network_socket_close(handle_t socket_handle);

int network_socket_bind(handle_t socket_handle, const socket_address_t *remote_address);

int network_socket_connect(handle_t socket_handle, const socket_address_t *remote_address);

int network_socket_listen(handle_t socket_handle, uint32_t backlog);

handle_t network_socket_accept(handle_t socket_handle, socket_address_t *remote_address);

int network_socket_shutdown(handle_t socket_handle, socket_shutdown_t how);

int network_socket_send(handle_t socket_handle, const uint8_t *data, size_t len, uint8_t flags);

int network_socket_receive(handle_t socket_handle, uint8_t *data, size_t len, uint8_t flags);

int network_socket_send_to(handle_t socket_handle, const uint8_t *data, size_t len, uint8_t flags, const socket_address_t *to);

int network_socket_receive_from(handle_t socket_handle, uint8_t *data, size_t len, uint8_t flags, socket_address_t *from);

int network_socket_gethostbyname(const char *name, hostent_t *hostent);

int network_socket_addr_parse(const char *ip_addr, int port, uint8_t *socket_addr);

int network_socket_addr_to_string(uint8_t *socket_addr, char *ip_addr, int *port);

#ifdef __cplusplus
}
#endif

#endif /* _FREERTOS_NETWORK_H */