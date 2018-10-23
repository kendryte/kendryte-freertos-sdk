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
#include <encoding.h>
#include <stdint.h>
#include <stdio.h>
#include "sysctl.h"
#include "uarths.h"

volatile uarths_t *const uarths = (volatile uarths_t *)UARTHS_BASE_ADDR;

uint8_t uarths_read_byte()
{
    while (1)
    {
        uarths_rxdata_t recv = uarths->rxdata;
        if (!recv.empty)
            return recv.data;
    }
}

void uarths_write_byte(uint8_t c)
{
    while (uarths->txdata.full)
        continue;
    uarths->txdata.data = c;
}

void uarths_puts(const char *s)
{
    while (*s)
        uarths_write_byte(*s++);
}

size_t uarths_read(uint8_t* buffer, size_t len)
{
    size_t read = 0;

    uarths_rxdata_t recv = uarths->rxdata;
    while (len && !recv.empty)
    {
        *buffer++ = recv.data;
        read++;
        len--;
        recv = uarths->rxdata;
    }

    if (len && !read)
    {
        *buffer++ = uarths_read_byte();
        read++;
        len--;
    }

    return read;
}

void uarths_init()
{
    uint32_t freq = sysctl_clock_get_freq(SYSCTL_CLOCK_CPU);
    uint16_t div = freq / 115200 - 1;

    /* Set UART registers */
    uarths->div.div = div;
    uarths->txctrl.txen = 1;
    uarths->rxctrl.rxen = 1;
    uarths->txctrl.txcnt = 0;
    uarths->rxctrl.rxcnt = 0;
    uarths->ip.txwm = 1;
    uarths->ip.rxwm = 1;
    uarths->ie.txwm = 0;
    uarths->ie.rxwm = 1;
}
