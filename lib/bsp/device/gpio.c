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
#include <driver.h>
#include <fpioa.h>
#include <gpio.h>
#include <semphr.h>
#include <stdio.h>
#include <sysctl.h>
#include <utility.h>

#define COMMON_ENTRY                                                         \
    gpio_data *data = (gpio_data *)userdata;                                 \
    volatile gpio_t *gpio = (volatile gpio_t *)data->base_addr;              \
    configASSERT(pin < data->pin_count);                                     \
    (void)data;                                                              \
    (void)gpio;

typedef struct
{
    uint32_t pin_count;
    uintptr_t base_addr;
} gpio_data;

static void gpio_install(void *userdata)
{
    /* GPIO clock under APB0 clock, so enable APB0 clock firstly */
    sysctl_clock_enable(SYSCTL_CLOCK_APB0);
    sysctl_clock_enable(SYSCTL_CLOCK_APB1);
    sysctl_clock_enable(SYSCTL_CLOCK_GPIO);
}

static int gpio_open(void *userdata)
{
    return 1;
}

static void gpio_close(void *userdata)
{
}

static void gpio_set_drive_mode(uint32_t pin, gpio_drive_mode_t mode, void *userdata)
{
    COMMON_ENTRY;
    int io_number = fpioa_get_io_by_function(FUNC_GPIO0 + pin);
    configASSERT(io_number > 0);

    fpioa_pull_t pull = 0;
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
    set_bit_idx(gpio->direction.u32, pin, dir);
}

static void gpio_set_pin_edge(uint32_t pin, gpio_pin_edge_t edge, void *userdata)
{
    COMMON_ENTRY;
    configASSERT(!"Not supported.");
}

static void gpio_set_on_changed(uint32_t pin, gpio_on_changed_t callback, void *callback_data, void *userdata)
{
    COMMON_ENTRY;
    configASSERT(!"Not supported.");
}

static gpio_pin_value_t gpio_get_pin_value(uint32_t pin, void *userdata)
{
    COMMON_ENTRY;

    uint32_t dir = get_bit_idx(gpio->direction.u32, pin);
    volatile uint32_t* reg = dir ? gpio->data_output.u32 : gpio->data_input.u32;
    return get_bit_idx(reg, pin);
}

static void gpio_set_pin_value(uint32_t pin, gpio_pin_value_t value, void *userdata)
{
    COMMON_ENTRY;

    uint32_t dir = get_bit_idx(gpio->direction.u32, pin);
    volatile uint32_t* reg = dir ? gpio->data_output.u32 : gpio->data_input.u32;
    configASSERT(dir == 1);
    set_bit_idx(reg, pin, value);
}

static gpio_data dev0_data = {8, GPIO_BASE_ADDR};

const gpio_driver_t g_gpio_driver_gpio0 = {{&dev0_data, gpio_install, gpio_open, gpio_close}, 8, gpio_set_drive_mode, gpio_set_pin_edge, gpio_set_on_changed, gpio_set_pin_value, gpio_get_pin_value};
