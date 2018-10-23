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
#include <driver.h>

#ifdef __cplusplus
extern "C"
{
#endif

int spi_sdcard_driver_install(const char *name, const char *spi_name, const char *cs_gpio_name, uint32_t cs_gpio_pin);

#ifdef __cplusplus
}
#endif

#endif /* _DRIVERS_SDCARD_H */
