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
#include "misc/ws2812b/ws2812b.h"
#include <kernel/driver_impl.hpp>
#include <stdlib.h>
#include <string.h>

using namespace sys;

#define WS2812B_SPI_CLOCK_RATE 2500000

typedef union _ws2812b_rgb {
    struct
    {
        uint32_t blue : 8;
        uint32_t red : 8;
        uint32_t green : 8;
        uint32_t reserved : 8;
    };
    uint32_t rgb;
} ws2812b_rgb;

typedef struct _ws2812b_info
{
    size_t total_number;
    ws2812b_rgb *rgb_buffer;
} ws2812b_info;

class k_spi_ws2812b_driver : public driver, public heap_object, public free_object_access
{
public:
    k_spi_ws2812b_driver(handle_t spi_handle, uint32_t total_number)
        : spi_driver_(system_handle_to_object(spi_handle).get_object().as<spi_driver>())
    {
        this->ws2812b_info_.total_number = total_number;
        this->ws2812b_info_.rgb_buffer = (ws2812b_rgb *)malloc(total_number * sizeof(ws2812b_rgb));
        configASSERT(ws2812b_info_.rgb_buffer != NULL);
        memset(this->ws2812b_info_.rgb_buffer, 0x0, total_number * sizeof(ws2812b_rgb));
    }

    virtual void install() override
    {
    }

    virtual void on_first_open() override
    {
        auto spi = make_accessor(spi_driver_);
        spi32_dev_ = make_accessor(spi->get_device(SPI_MODE_0, SPI_FF_STANDARD, 1, 32));
        spi32_clock_rate_ = (uint32_t)spi32_dev_->set_clock_rate(WS2812B_SPI_CLOCK_RATE);
    }

    virtual void on_last_close() override
    {
        spi32_dev_.reset();
        configASSERT(ws2812b_info_.rgb_buffer != NULL);
        free(ws2812b_info_.rgb_buffer);
    }

    void clear_rgb_buffer()
    {
        configASSERT(ws2812b_info_.rgb_buffer != NULL);
        memset(ws2812b_info_.rgb_buffer, 0x0, ws2812b_info_.total_number * sizeof(ws2812b_rgb));
    }

    void set_rgb_buffer(uint32_t number, uint32_t rgb_data)
    {
        configASSERT(number < ws2812b_info_.total_number);
        configASSERT(ws2812b_info_.rgb_buffer != NULL);

        (ws2812b_info_.rgb_buffer + number)->rgb = rgb_data;
    }

    void set_rgb()
    {
        uint32_t longbit;
        uint32_t shortbit;
        uint32_t resbit;
        uint32_t i = 0;
        size_t ws_cnt = ws2812b_info_.total_number;
        uint32_t *ws_data = (uint32_t *)ws2812b_info_.rgb_buffer;
        uint32_t clk_time = 1e9 / spi32_clock_rate_; /* nanosecond per clk */
        configASSERT(clk_time <= (850 + 150) / 2);

        longbit = (850 - 150 + clk_time - 1) / clk_time;
        shortbit = (400 - 150 + clk_time - 1) / clk_time;
        resbit = (400000 / clk_time);
        uint32_t spi_send_cnt = (((ws_cnt * 24 * (longbit + shortbit) + resbit + 7) / 8) + 3) / 4;
        uint32_t reset_cnt = ((resbit + 7) / 8 + 3) / 4;
        uint32_t *tmp_spi_data = (uint32_t *)malloc((spi_send_cnt + reset_cnt) * 4);
        configASSERT(tmp_spi_data != NULL);
        const uint8_t *ws2812b_spi_send = (const uint8_t *)tmp_spi_data;

        memset(tmp_spi_data, 0, (spi_send_cnt + reset_cnt) * 4);
        uint32_t *spi_data = tmp_spi_data;
        spi_data += reset_cnt;
        int pos = 31;
        uint32_t long_cnt = longbit;
        uint32_t short_cnt = shortbit;
        for (i = 0; i < ws_cnt; i++)
        {
            for (uint32_t mask = 0x800000; mask > 0; mask >>= 1)
            {
                long_cnt = longbit;
                short_cnt = shortbit;

                if (ws_data[i] & mask)
                {
                    while (long_cnt--)
                    {
                        *(spi_data) |= (1 << (pos--));
                        if (pos < 0)
                        {
                            spi_data++;
                            pos = 31;
                        }
                    }
                    while (short_cnt--)
                    {
                        *(spi_data) &= ~(1 << (pos--));
                        if (pos < 0)
                        {
                            spi_data++;
                            pos = 31;
                        }
                    }
                }
                else
                {
                    while (short_cnt--)
                    {
                        *(spi_data) |= (1 << (pos--));
                        if (pos < 0)
                        {
                            spi_data++;
                            pos = 31;
                        }
                    }
                    while (long_cnt--)
                    {
                        *(spi_data) &= ~(1 << (pos--));
                        if (pos < 0)
                        {
                            spi_data++;
                            pos = 31;
                        }
                    }
                }
            }
        }
        spi32_dev_->write({ ws2812b_spi_send, std::ptrdiff_t((spi_send_cnt + reset_cnt) * 4) });
        free(tmp_spi_data);
    }

private:
    object_ptr<spi_driver> spi_driver_;
    object_accessor<spi_device_driver> spi32_dev_;
    uint32_t spi32_clock_rate_;
    ws2812b_info ws2812b_info_;
};

handle_t spi_ws2812b_driver_install(handle_t spi_handle, uint32_t total_number)
{
    try
    {
        auto driver = make_object<k_spi_ws2812b_driver>(spi_handle, total_number);
        driver->install();
        return system_alloc_handle(make_accessor(driver));
    }
    catch (...)
    {
        return NULL_HANDLE;
    }
}

void ws2812b_clear_rgb_buffer(handle_t ws2812b_handle)
{
    auto driver = system_handle_to_object(ws2812b_handle).as<k_spi_ws2812b_driver>();
    driver->clear_rgb_buffer();
}

void ws2812b_set_rgb_buffer(handle_t ws2812b_handle, uint32_t ws2812b_number, uint32_t rgb_data)
{
    auto driver = system_handle_to_object(ws2812b_handle).as<k_spi_ws2812b_driver>();
    driver->set_rgb_buffer(ws2812b_number, rgb_data);
}

void ws2812b_set_rgb(handle_t ws2812b_handle)
{
    auto driver = system_handle_to_object(ws2812b_handle).as<k_spi_ws2812b_driver>();
    driver->set_rgb();
}
