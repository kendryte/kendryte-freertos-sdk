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
#ifndef _DRIVERS_SDCARD_H
#define _DRIVERS_SDCARD_H

#include <stdint.h>
#include <osdefs.h>

#ifdef __cplusplus
extern "C"
{
#endif
    
/**
 * @brief       Install a SPI SDCard driver
 *
 * @param[in]   spi_handle          The SPI controller handle
 * @param[in]   cs_gpio_handle      The GPIO controller handle for CS
 * @param[in]   cs_gpio_pin         The GPIO pin for CS
 *
 * @return      result
 *     - 0      Fail
 *     - other  The driver handle
 */
handle_t spi_sdcard_driver_install(handle_t spi_handle, handle_t cs_gpio_handle, uint32_t cs_gpio_pin);

#ifdef __cplusplus
}
#endif

#endif /* _DRIVERS_SDCARD_H */
