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
#include <FreeRTOS.h>
#include <fpioa.h>
#include <gpio.h>
#include <kernel/driver_impl.hpp>
#include <semphr.h>
#include <stdio.h>
#include <sysctl.h>
#include <utility.h>

using namespace sys;

class k_gpio_driver : public gpio_driver, public static_object, public free_object_access
{
public:
    k_gpio_driver(uintptr_t base_addr)
        : gpio_(*reinterpret_cast<volatile gpio_t *>(base_addr))
    {
    }

    virtual void install() override
    {
        sysctl_clock_disable(SYSCTL_CLOCK_GPIO);
    }

    virtual void on_first_open() override
    {
        sysctl_clock_enable(SYSCTL_CLOCK_GPIO);
    }

    virtual void on_last_close() override
    {
        sysctl_clock_disable(SYSCTL_CLOCK_GPIO);
    }

    virtual uint32_t get_pin_count() override
    {
        return 8;
    }

    virtual void set_drive_mode(uint32_t pin, gpio_drive_mode_t mode) override
    {
        int io_number = fpioa_get_io_by_function(static_cast<fpioa_function_t>(FUNC_GPIO0 + pin));
        configASSERT(io_number > 0);

        fpioa_pull_t pull = FPIOA_PULL_NONE;
        uint32_t dir = 0;

        switch (mode)
        {
        case GPIO_DM_INPUT:
            pull = FPIOA_PULL_NONE;
            dir = 0;
            break;
        case GPIO_DM_INPUT_PULL_DOWN:
            pull = FPIOA_PULL_DOWN;
            dir = 0;
            break;
        case GPIO_DM_INPUT_PULL_UP:
            pull = FPIOA_PULL_UP;
            dir = 0;
            break;
        case GPIO_DM_OUTPUT:
            pull = FPIOA_PULL_DOWN;
            dir = 1;
            break;
        default:
            configASSERT(!"GPIO drive mode is not supported.");
            break;
        }

        fpioa_set_io_pull(io_number, pull);
        set_bit_idx(gpio_.direction.u32, pin, dir);
    }

    virtual void set_pin_edge(uint32_t pin, gpio_pin_edge_t edge) override
    {
        configASSERT(!"Not supported.");
    }

    virtual void set_on_changed(uint32_t pin, gpio_on_changed_t callback, void *userdata) override
    {
        configASSERT(!"Not supported.");
    }

    virtual gpio_pin_value_t get_pin_value(uint32_t pin) override
    {
        uint32_t dir = get_bit_idx(gpio_.direction.u32, pin);
        volatile uint32_t *reg = dir ? gpio_.data_output.u32 : gpio_.data_input.u32;
        return static_cast<gpio_pin_value_t>(get_bit_idx(reg, pin));
    }

    virtual void set_pin_value(uint32_t pin, gpio_pin_value_t value) override
    {
        uint32_t dir = get_bit_idx(gpio_.direction.u32, pin);
        volatile uint32_t *reg = dir ? gpio_.data_output.u32 : gpio_.data_input.u32;
        configASSERT(dir == 1);
        set_bit_idx(reg, pin, value);
    }

private:
    volatile gpio_t &gpio_;
};

static k_gpio_driver dev0_driver(GPIO_BASE_ADDR);

driver &g_gpio_driver_gpio0 = dev0_driver;
