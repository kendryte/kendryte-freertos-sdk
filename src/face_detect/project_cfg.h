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
#ifndef _PROJECT_CFG_H_
#define _PROJECT_CFG_H_
#include <pin_cfg.h>

#define SPI_CHANNEL 0
#define SPI_SLAVE_SELECT 3
#define __SPI_SYSCTL(x, y) SYSCTL_##x##_SPI##y
#define _SPI_SYSCTL(x, y) __SPI_SYSCTL(x, y)
#define SPI_SYSCTL(x) _SPI_SYSCTL(x, SPI_CHANNEL)
#define __SPI_SS(x, y) FUNC_SPI##x##_SS##y
#define _SPI_SS(x, y) __SPI_SS(x, y)
#define SPI_SS _SPI_SS(SPI_CHANNEL, SPI_SLAVE_SELECT)
#define __SPI(x, y) FUNC_SPI##x##_##y
#define _SPI(x, y) __SPI(x, y)
#define SPI(x) _SPI(SPI_CHANNEL, x)

#define DCX_IO          (8)
#define DCX_GPIONUM     (2)
#define TF_CS_GPIONUM   7

const fpioa_cfg_t g_fpioa_cfg =
{
    .version = PIN_CFG_VERSION,
    .functions_count = 11 + 4,
    .functions =
    {
        {DCX_IO, FUNC_GPIOHS0 + DCX_GPIONUM},
        {6, SPI_SS},
        {7, SPI(SCLK)},
        {11, FUNC_CMOS_RST},
        {13, FUNC_CMOS_PWDN},
        {10, FUNC_SCCB_SCLK},
        {9, FUNC_SCCB_SDA},
        {14, FUNC_CMOS_XCLK},
        {12, FUNC_CMOS_VSYNC},
        {17, FUNC_CMOS_HREF},
        {15, FUNC_CMOS_PCLK},
        {32, FUNC_GPIOHS0 + TF_CS_GPIONUM},
        {29, FUNC_SPI1_SCLK},
        {30, FUNC_SPI1_D0},
        {31, FUNC_SPI1_D1}
    }
};
const pin_cfg_t g_pin_cfg =
{
    .version = PIN_CFG_VERSION,
    .set_spi0_dvp_data = 1
};

#endif
