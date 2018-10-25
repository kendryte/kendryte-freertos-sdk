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
#include "storage/sdcard.h"
#include <devices.h>
#include <hal.h>
#include <kernel/driver_impl.hpp>
#include <stdlib.h>
#include <string.h>

using namespace sys;

class k_spi_sdcard_driver : public wdt_driver, public heap_object, public free_object_access
{
public:
    k_spi_sdcard_driver(const char *spi_name, const char *cs_gpio_name, uint32_t cs_gpio_pin)
        : spi_name_(spi_name), cs_gpio_name_(cs_gpio_name), cs_gpio_pin_(cs_gpio_pin)
    {
    }

    virtual void install() override
    {
    }

    virtual void on_first_open() override
    {
        auto spi = system_open_driver(spi_name_);
    }

    virtual void on_last_close() override
    {
    }

    virtual void set_response_mode(wdt_response_mode_t mode) override
    {
    }

    virtual size_t set_timeout(size_t nanoseconds) override
    {
        return size_t();
    }

    virtual void set_on_timeout(wdt_on_timeout_t handler, void *userdata) override
    {
    }

    virtual void restart_counter() override
    {
    }

    virtual void set_enable(bool enable) override
    {
    }

private:
    const char *spi_name_;
    const char *cs_gpio_name_;
    uint32_t cs_gpio_pin_;
};

int spi_sdcard_driver_install(const char *name, const char *spi_name, const char *cs_gpio_name, uint32_t cs_gpio_pin)
{
    try
    {
        auto driver = make_object<k_spi_sdcard_driver>(spi_name, cs_gpio_name, cs_gpio_pin);
        system_install_driver(name, driver.as<sys::driver>());
        return 0;
    }
    catch (...)
    {
        return -1;
    }
}