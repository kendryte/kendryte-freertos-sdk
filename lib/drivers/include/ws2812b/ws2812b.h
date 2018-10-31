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
#ifndef _DRIVERS_WS2812B_H
#define _DRIVERS_WS2812B_H

#include <stdint.h>
#include <osdefs.h>

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief       Install a SPI ws2812b driver
 *
 * @param[in]   spi_handle          The SPI controller handle
 * @param[in]   total_number        The total number of the ws2812b LED.
 *
 * @return      result
 *     - 0      Fail
 *     - other  The driver handle
 */
handle_t spi_ws2812b_driver_install(handle_t spi_handle, uint32_t total_number);

/**
 * @brief       Clear rgb buffer.
 *
 * @param[in]   ws2812b_handle      The ws2812b handle
 */
void ws2812b_clear_rgb_buffer(handle_t ws2812b_handle);

/**
 * @brief       Set rgb buffer.
 *
 * @param[in]   ws2812b_handle      The ws2812b handle
 * @param[in]   ws2812b_number      A number of ws2812b LED.
 * @param[in]   rgb_data            The ws2812b rgb data.
 */
void ws2812b_set_rgb_buffer(handle_t ws2812b_handle, uint32_t ws2812b_number, uint32_t rgb_data);

/**
 * @brief       Light up the ws2812b by SPI.
 *
 * @param[in]   ws2812b_handle      The ws2812b handle
 */
void ws2812b_set_rgb(handle_t ws2812b_handle);

#ifdef __cplusplus
}
#endif

#endif /* _DRIVERS_WS2812B_H */
