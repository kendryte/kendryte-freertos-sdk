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
#include <gpiohs.h>
#include <hal.h>
#include <kernel/driver_impl.hpp>
#include <plic.h>
#include <semphr.h>
#include <stdio.h>
#include <sysctl.h>
#include <utility.h>

using namespace sys;

class k_gpiohs_driver : public gpio_driver, public static_object, public free_object_access
{
public:
    k_gpiohs_driver(uintptr_t base_addr)
        : gpiohs_(*reinterpret_cast<volatile gpiohs_t *>(base_addr))
    {
    }

    virtual void install() override
    {
        gpiohs_.rise_ie.u32[0] = 0;
        gpiohs_.rise_ip.u32[0] = 0xFFFFFFFF;
        gpiohs_.fall_ie.u32[0] = 0;
        gpiohs_.fall_ip.u32[0] = 0xFFFFFFFF;

        uint32_t i;
        for (i = 0; i < 32; i++)
        {
            pin_context_[i] = {};
            pin_context_[i].driver = this;
            pin_context_[i].pin = i;
            pic_set_irq_handler(IRQN_GPIOHS0_INTERRUPT + i, gpiohs_pin_on_change_isr, pin_context_ + i);
            pic_set_irq_priority(IRQN_GPIOHS0_INTERRUPT + i, 1);
        }
    }

    virtual void on_first_open() override
    {
    }

    virtual void on_last_close() override
    {
    }

    virtual uint32_t get_pin_count() override
    {
        return 32;
    }

    virtual void set_drive_mode(uint32_t pin, gpio_drive_mode_t mode) override
    {
        int io_number = fpioa_get_io_by_function(static_cast<fpioa_function_t>(FUNC_GPIOHS0 + pin));
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
            configASSERT(!"GPIO drive mode is not supported.") break;
        }

        fpioa_set_io_pull(io_number, pull);
        volatile uint32_t *reg = dir ? gpiohs_.output_en.u32 : gpiohs_.input_en.u32;
        volatile uint32_t *reg_d = !dir ? gpiohs_.output_en.u32 : gpiohs_.input_en.u32;
        set_bit_idx(reg_d, pin, 0);
        set_bit_idx(reg, pin, 1);
    }

    virtual void set_pin_edge(uint32_t pin, gpio_pin_edge_t edge) override
    {
        uint32_t rise = 0, fall = 0, irq = 0;
        switch (edge)
        {
        case GPIO_PE_NONE:
            rise = fall = irq = 0;
            break;
        case GPIO_PE_FALLING:
            rise = 0;
            fall = irq = 1;
            break;
        case GPIO_PE_RISING:
            fall = 0;
            rise = irq = 1;
            break;
        case GPIO_PE_BOTH:
            rise = fall = irq = 1;
            break;
        default:
            configASSERT(!"Invalid gpio edge");
            break;
        }

        pin_context_[pin].edge = edge;
        set_bit_idx(gpiohs_.rise_ie.u32, pin, rise);
        set_bit_idx(gpiohs_.fall_ie.u32, pin, fall);
        pic_set_irq_enable(IRQN_GPIOHS0_INTERRUPT + pin, irq);
    }

    virtual void set_on_changed(uint32_t pin, gpio_on_changed_t callback, void *userdata) override
    {
        pin_context_[pin].userdata = userdata;
        pin_context_[pin].callback = callback;
    }

    virtual gpio_pin_value_t get_pin_value(uint32_t pin) override
    {
        return static_cast<gpio_pin_value_t>(get_bit_idx(gpiohs_.input_val.u32, pin));
    }

    virtual void set_pin_value(uint32_t pin, gpio_pin_value_t value) override
    {
        set_bit_idx(gpiohs_.output_val.u32, pin, value);
    }

private:
    static void gpiohs_pin_on_change_isr(void *userdata)
    {
        auto &pin_context = *reinterpret_cast<gpiohs_pin_context *>(userdata);
        auto &driver = pin_context.driver;
        auto &gpiohs = driver->gpiohs_;

        uint32_t pin = pin_context.pin;
        uint32_t rise = 0, fall = 0;
        switch (pin_context.edge)
        {
        case GPIO_PE_NONE:
            rise = fall = 0;
            break;
        case GPIO_PE_FALLING:
            rise = 0;
            fall = 1;
            break;
        case GPIO_PE_RISING:
            fall = 0;
            rise = 1;
            break;
        case GPIO_PE_BOTH:
            rise = fall = 1;
            break;
        default:
            configASSERT(!"Invalid gpio edge");
            break;
        }

        if (rise)
        {
            set_bit_idx(gpiohs.rise_ie.u32, pin, 0);
            set_bit_idx(gpiohs.rise_ip.u32, pin, 1);
            set_bit_idx(gpiohs.rise_ie.u32, pin, 1);
        }

        if (fall)
        {
            set_bit_idx(gpiohs.fall_ie.u32, pin, 0);
            set_bit_idx(gpiohs.fall_ip.u32, pin, 1);
            set_bit_idx(gpiohs.fall_ie.u32, pin, 1);
        }

        if (pin_context.callback)
            pin_context.callback(pin_context.pin, pin_context.userdata);
    }

private:
    volatile gpiohs_t &gpiohs_;

    struct gpiohs_pin_context
    {
        k_gpiohs_driver *driver;
        uint32_t pin;
        gpio_pin_edge_t edge;
        gpio_on_changed_t callback;
        void *userdata;
    } pin_context_[32];
};

static k_gpiohs_driver dev0_driver(GPIOHS_BASE_ADDR);

driver &g_gpiohs_driver_gpio0 = dev0_driver;
