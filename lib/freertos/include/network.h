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

handle_t network_socket_open(address_family_t address_family, socket_type_t type, protocol_type_t protocol);

handle_t network_socket_close(handle_t socket_handle);

#ifdef __cplusplus
}
#endif

#endif /* _FREERTOS_NETWORK_H */