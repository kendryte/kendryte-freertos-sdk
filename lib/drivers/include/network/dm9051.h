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
#ifndef _DRIVERS_DM9051_H
#define _DRIVERS_DM9051_H

#include <osdefs.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief       Install DM9051 driver
 *
 * @param[in]   spi_handle          The SPI controller handle
 * @param[in]   spi_cs_mask         The SPI chip select
 * @param[in]   int_gpio_handle     The GPIO controller handle for dm9051 interrupt
 * @param[in]   int_gpio_pin        The GPIO pin for dm9051 interrupt
 * @param[in]   mac_address         The MAC address for network
 *
 * @return      result
 *     - 0      Fail
 *     - other  The network adapter handle
 */
handle_t dm9051_driver_install(handle_t spi_handle, uint32_t spi_cs_mask, handle_t int_gpio_handle, uint32_t int_gpio_pin, const mac_address_t *mac_address);

#ifdef __cplusplus
}
#endif

#endif /* _DRIVERS_DM9051_H */