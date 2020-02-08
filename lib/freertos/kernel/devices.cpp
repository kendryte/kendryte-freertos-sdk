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
#include "devices.h"
#include "FreeRTOS.h"
#include "device_priv.h"
#include "filesystem.h"
#include "hal.h"
#include "kernel/driver.hpp"
#include <atomic.h>
#include <plic.h>
#include <semphr.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysctl.h>
#include <uarths.h>
#include <sys/lock.h>

using namespace sys;

#define MAX_HANDLES 256
#define HANDLE_OFFSET 256
#define MAX_CUSTOM_DRIVERS 32

#define DEFINE_INSTALL_DRIVER(type)          \
    static void install_##type##_drivers()   \
    {                                        \
        auto head = g_##type##_drivers;      \
        while (head->name)                   \
        {                                    \
            auto &driver = head->driver_ptr; \
            driver->install();               \
            head++;                          \
        }                                    \
    }

#define CATCH_ALL           \
    catch (errno_exception & e) \
    {                       \
        errno = e.code();    \
        return -1;          \
    }

typedef struct
{
    object_accessor<object_access> object;
} _file;

static _file *handles_[MAX_HANDLES];
static driver_registry_t g_custom_drivers[MAX_CUSTOM_DRIVERS];
static const char dummy_driver_name[] = "";
static _lock_t dma_lock;

uintptr_t fft_file_;
uintptr_t aes_file_;
uintptr_t sha256_file_;
uintptr_t kpu_file_;

extern UBaseType_t uxCPUClockRate;

DEFINE_INSTALL_DRIVER(hal);
DEFINE_INSTALL_DRIVER(dma);
DEFINE_INSTALL_DRIVER(system);

object_accessor<driver> find_free_driver(driver_registry_t *registry, const char *name)
{
    auto head = registry;
    while (head->name)
    {
        if (strcmp(name, head->name) == 0)
        {
            auto &driver = head->driver_ptr;
            try
            {
                return make_accessor(driver);
            }
            catch (...)
            {
                return {};
            }
        }

        head++;
    }

    return {};
}

object_accessor<driver> find_free_dynamic_driver(const char *name)
{
    size_t i = 0;
    driver_registry_t *head = g_custom_drivers;
    for (i = 0; i < MAX_CUSTOM_DRIVERS; i++, head++)
    {
        if (head->name && strcmp(name, head->name) == 0)
        {
            auto &driver = head->driver_ptr;
            try
            {
                return make_accessor(driver);
            }
            catch (...)
            {
                return {};
            }
        }
    }

    return {};
}

void install_drivers()
{
    install_system_drivers();

    fft_file_ = io_open("/dev/fft0");
    aes_file_ = io_open("/dev/aes0");
    sha256_file_ = io_open("/dev/sha256");
    kpu_file_ = io_open("/dev/kpu0");
}

static _file *io_alloc_file(object_accessor<object_access> object)
{
    if (object)
    {
        _file *file = new (std::nothrow) _file;
        if (!file)
            return nullptr;
        file->object = std::move(object);
        return file;
    }

    return nullptr;
}

static _file *io_alloc_file(object_ptr<object_access> object)
{
    if (object)
        return io_alloc_file(make_accessor(object));

    return nullptr;
}

static _file *io_open_reg(driver_registry_t *registry, const char *name, _file **file)
{
    auto driver = find_free_driver(registry, name);
    if (driver)
    {
        _file *ret = io_alloc_file(std::move(driver));
        *file = ret;
        return ret;
    }

    return nullptr;
}

static _file *io_open_dynamic(const char *name, _file **file)
{
    auto driver = find_free_dynamic_driver(name);
    if (driver)
    {
        _file *ret = io_alloc_file(std::move(driver));
        *file = ret;
        return ret;
    }

    return nullptr;
}

/* Generic IO Implementation Helper Macros */

#define DEFINE_READ_PROXY(t)                                  \
    if (auto f = rfile->object.as<t>())                       \
    {                                                         \
        return (int)f->read({ buffer, std::ptrdiff_t(len) }); \
    }

#define DEFINE_WRITE_PROXY(t)                                  \
    if (auto f = rfile->object.as<t>())                        \
    {                                                          \
        return (int)f->write({ buffer, std::ptrdiff_t(len) }); \
    }

#define DEFINE_CONTROL_PROXY(t)                                                                                                       \
    if (auto t = rfile->object.as<t##_driver>())                                                                                      \
    {                                                                                                                                 \
        return (int)t->control(control_code, { write_buffer, std::ptrdiff_t(write_len) }, { read_buffer, std::ptrdiff_t(read_len) }); \
    }

static void dma_add_free();

int io_read(handle_t file, uint8_t *buffer, size_t len)
{
    try
    {
        configASSERT(file >= HANDLE_OFFSET);
        _file *rfile = (_file *)handles_[file - HANDLE_OFFSET];
        /* clang-format off */
        DEFINE_READ_PROXY(uart_driver)
        else DEFINE_READ_PROXY(i2c_device_driver)
        else DEFINE_READ_PROXY(spi_device_driver)
        else DEFINE_READ_PROXY(filesystem_file)
        else DEFINE_READ_PROXY(network_socket)
        else
        {
            return -1;
        }
        /* clang-format on */
    }
    CATCH_ALL;
}

static void io_free(_file *file)
{
    if (file)
    {
        if (file->object.is<dma_driver>())
            dma_add_free();

        delete file;
    }
}

static handle_t io_alloc_handle(_file *file)
{
    if (file)
    {
        size_t i, j;
        for (i = 0; i < 2; i++)
        {
            for (j = 0; j < MAX_HANDLES; j++)
            {
                if (atomic_cas(handles_ + j, 0, file) == 0)
                    return j + HANDLE_OFFSET;
            }
        }

        io_free(file);
        return 0;
    }

    return 0;
}

handle_t io_open(const char *name)
{
    _file *file = 0;
    if (io_open_reg(g_system_drivers, name, &file))
    {
    }
    else if (io_open_reg(g_hal_drivers, name, &file))
    {
    }
    else if (io_open_dynamic(name, &file))
    {
    }

    if (file)
        return io_alloc_handle(file);
    configASSERT(file);
    return 0;
}

int io_close(handle_t file)
{
    if (file)
    {
        configASSERT(file >= HANDLE_OFFSET);
        _file *rfile = (_file *)handles_[file - HANDLE_OFFSET];
        io_free(rfile);
        atomic_set(handles_ + file - HANDLE_OFFSET, 0);
    }

    return 0;
}

int io_write(handle_t file, const uint8_t *buffer, size_t len)
{
    try
    {
        configASSERT(file >= HANDLE_OFFSET);
        _file *rfile = (_file *)handles_[file - HANDLE_OFFSET];
        /* clang-format off */
        DEFINE_WRITE_PROXY(uart_driver)
        else DEFINE_WRITE_PROXY(i2c_device_driver)
        else DEFINE_WRITE_PROXY(spi_device_driver)
        else DEFINE_WRITE_PROXY(filesystem_file)
        else DEFINE_WRITE_PROXY(network_socket)
        else
        {
            return -1;
        }
        /* clang-format on */
    }
    CATCH_ALL;
}

int io_control(handle_t file, uint32_t control_code, const uint8_t *write_buffer, size_t write_len, uint8_t *read_buffer, size_t read_len)
{
    try
    {
        configASSERT(file >= HANDLE_OFFSET);
        _file *rfile = (_file *)handles_[file - HANDLE_OFFSET];
        DEFINE_CONTROL_PROXY(custom)
        else
        {
            return -1;
        }
    }
    CATCH_ALL;
    
}

/* Device IO Implementation Helper Macros */

#define COMMON_ENTRY(t)                                     \
    configASSERT(file >= HANDLE_OFFSET);                    \
    _file *rfile = (_file *)handles_[file - HANDLE_OFFSET]; \
    configASSERT(rfile && rfile->object.is<t##_driver>());  \
    auto t = rfile->object.as<t##_driver>();

#define COMMON_ENTRY_FILE(file, t)                          \
    configASSERT(file >= HANDLE_OFFSET);                    \
    _file *rfile = (_file *)handles_[file - HANDLE_OFFSET]; \
    configASSERT(rfile && rfile->object.is<t##_driver>());  \
    auto t = rfile->object.as<t##_driver>();

/* UART */

void uart_config(handle_t file, uint32_t baud_rate, uint32_t databits, uart_stopbits_t stopbits, uart_parity_t parity)
{
    COMMON_ENTRY(uart);
    uart->config(baud_rate, databits, stopbits, parity);
}

void uart_set_read_timeout(handle_t file, size_t millisecond)
{
    COMMON_ENTRY(uart);
    uart->set_read_timeout(millisecond);
}

/* GPIO */

uint32_t gpio_get_pin_count(handle_t file)
{
    COMMON_ENTRY(gpio);
    return gpio->get_pin_count();
}

void gpio_set_drive_mode(handle_t file, uint32_t pin, gpio_drive_mode_t mode)
{
    COMMON_ENTRY(gpio);
    gpio->set_drive_mode(pin, mode);
}

void gpio_set_pin_edge(handle_t file, uint32_t pin, gpio_pin_edge_t edge)
{
    COMMON_ENTRY(gpio);
    gpio->set_pin_edge(pin, edge);
}

void gpio_set_on_changed(handle_t file, uint32_t pin, gpio_on_changed_t callback, void *userdata)
{
    COMMON_ENTRY(gpio);
    gpio->set_on_changed(pin, callback, userdata);
}

gpio_pin_value_t gpio_get_pin_value(handle_t file, uint32_t pin)
{
    COMMON_ENTRY(gpio);
    return gpio->get_pin_value(pin);
}

void gpio_set_pin_value(handle_t file, uint32_t pin, gpio_pin_value_t value)
{
    COMMON_ENTRY(gpio);
    gpio->set_pin_value(pin, value);
}

/* I2C */

handle_t i2c_get_device(handle_t file, uint32_t slave_address, uint32_t address_width)
{
    COMMON_ENTRY(i2c);
    auto driver = i2c->get_device(slave_address, address_width);
    return io_alloc_handle(io_alloc_file(driver));
}

double i2c_dev_set_clock_rate(handle_t file, double clock_rate)
{
    COMMON_ENTRY(i2c_device);
    return i2c_device->set_clock_rate(clock_rate);
}

int i2c_dev_transfer_sequential(handle_t file, const uint8_t *write_buffer, size_t write_len, uint8_t *read_buffer, size_t read_len)
{
    COMMON_ENTRY(i2c_device);
    return i2c_device->transfer_sequential({ write_buffer, std::ptrdiff_t(write_len) }, { read_buffer, std::ptrdiff_t(read_len) });
}

void i2c_config_as_slave(handle_t file, uint32_t slave_address, uint32_t address_width, i2c_slave_handler_t *handler)
{
    COMMON_ENTRY(i2c);
    i2c->config_as_slave(slave_address, address_width, *handler);
}

double i2c_slave_set_clock_rate(handle_t file, double clock_rate)
{
    COMMON_ENTRY(i2c);
    return i2c->slave_set_clock_rate(clock_rate);
}

/* I2S */

void i2s_config_as_render(handle_t file, const audio_format_t *format, size_t delay_ms, i2s_align_mode_t align_mode, size_t channels_mask)
{
    COMMON_ENTRY(i2s);
    i2s->config_as_render(*format, delay_ms, align_mode, channels_mask);
}

void i2s_config_as_capture(handle_t file, const audio_format_t *format, size_t delay_ms, i2s_align_mode_t align_mode, size_t channels_mask)
{
    COMMON_ENTRY(i2s);
    i2s->config_as_capture(*format, delay_ms, align_mode, channels_mask);
}

void i2s_get_buffer(handle_t file, uint8_t **buffer, size_t *frames)
{
    COMMON_ENTRY(i2s);
    gsl::span<uint8_t> span;
    i2s->get_buffer(span, *frames);
    *buffer = span.data();
}

void i2s_release_buffer(handle_t file, size_t frames)
{
    COMMON_ENTRY(i2s);
    i2s->release_buffer(frames);
}

void i2s_start(handle_t file)
{
    COMMON_ENTRY(i2s);
    i2s->start();
}

void i2s_stop(handle_t file)
{
    COMMON_ENTRY(i2s);
    i2s->stop();
}

/* SPI */
void spi_slave_config(handle_t file, handle_t gpio_handle, uint8_t int_pin, uint8_t ready_pin, size_t data_bit_length, uint8_t *data, uint32_t len, spi_slave_receive_callback_t callback)
{
    COMMON_ENTRY(spi);
    spi->slave_config(gpio_handle, int_pin, ready_pin, data_bit_length, data, len, callback);
}

handle_t spi_get_device(handle_t file, spi_mode_t mode, spi_frame_format_t frame_format, uint32_t chip_select_mask, uint32_t data_bit_length)
{
    COMMON_ENTRY(spi);
    auto driver = spi->get_device(mode, frame_format, chip_select_mask, data_bit_length);
    return io_alloc_handle(io_alloc_file(driver));
}

void spi_dev_config_non_standard(handle_t file, uint32_t instruction_length, uint32_t address_length, uint32_t wait_cycles, spi_inst_addr_trans_mode_t trans_mode)
{
    COMMON_ENTRY(spi_device);
    spi_device->config_non_standard(instruction_length, address_length, wait_cycles, trans_mode);
}

double spi_dev_set_clock_rate(handle_t file, double clock_rate)
{
    COMMON_ENTRY(spi_device);
    return spi_device->set_clock_rate(clock_rate);
}

void spi_dev_set_endian(handle_t file, uint32_t endian)
{
    COMMON_ENTRY(spi_device);
    return spi_device->set_endian(endian);
}

int spi_dev_transfer_full_duplex(handle_t file, const uint8_t *write_buffer, size_t write_len, uint8_t *read_buffer, size_t read_len)
{
    COMMON_ENTRY(spi_device);
    return spi_device->transfer_full_duplex({ write_buffer, std::ptrdiff_t(write_len) }, { read_buffer, std::ptrdiff_t(read_len) });
}

int spi_dev_transfer_sequential(handle_t file, const uint8_t *write_buffer, size_t write_len, uint8_t *read_buffer, size_t read_len)
{
    COMMON_ENTRY(spi_device);
    return spi_device->transfer_sequential({ write_buffer, std::ptrdiff_t(write_len) }, { read_buffer, std::ptrdiff_t(read_len) });
}

void spi_dev_fill(handle_t file, uint32_t instruction, uint32_t address, uint32_t value, size_t count)
{
    COMMON_ENTRY(spi_device);
    return spi_device->fill(instruction, address, value, count);
}

/* DVP */

void dvp_config(handle_t file, uint32_t width, uint32_t height, bool auto_enable)
{
    COMMON_ENTRY(dvp);
    dvp->config(width, height, auto_enable);
}

void dvp_enable_frame(handle_t file)
{
    COMMON_ENTRY(dvp);
    dvp->enable_frame();
}

uint32_t dvp_get_output_num(handle_t file)
{
    COMMON_ENTRY(dvp);
    return dvp->get_output_num();
}

void dvp_set_signal(handle_t file, dvp_signal_type_t type, bool value)
{
    COMMON_ENTRY(dvp);
    dvp->set_signal(type, value);
}

void dvp_set_output_enable(handle_t file, uint32_t index, bool enable)
{
    COMMON_ENTRY(dvp);
    dvp->set_output_enable(index, enable);
}

void dvp_set_output_attributes(handle_t file, uint32_t index, video_format_t format, void *output_buffer)
{
    COMMON_ENTRY(dvp);
    dvp->set_output_attributes(index, format, output_buffer);
}

void dvp_set_frame_event_enable(handle_t file, dvp_frame_event_t event, bool enable)
{
    COMMON_ENTRY(dvp);
    dvp->set_frame_event_enable(event, enable);
}

void dvp_set_on_frame_event(handle_t file, dvp_on_frame_event_t handler, void *userdata)
{
    COMMON_ENTRY(dvp);
    dvp->set_on_frame_event(handler, userdata);
}

double dvp_xclk_set_clock_rate(handle_t file, double clock_rate)
{
    COMMON_ENTRY(dvp);
    return dvp->xclk_set_clock_rate(clock_rate);
}

/* SSCB */

handle_t sccb_get_device(handle_t file, uint32_t slave_address, uint32_t reg_address_width)
{
    COMMON_ENTRY(sccb);
    auto driver = sccb->get_device(slave_address, reg_address_width);
    return io_alloc_handle(io_alloc_file(driver));
}

uint8_t sccb_dev_read_byte(handle_t file, uint16_t reg_address)
{
    COMMON_ENTRY(sccb_device);
    return sccb_device->read_byte(reg_address);
}

void sccb_dev_write_byte(handle_t file, uint16_t reg_address, uint8_t value)
{
    COMMON_ENTRY(sccb_device);
    sccb_device->write_byte(reg_address, value);
}

/* FFT */

void fft_complex_uint16(uint16_t shift, fft_direction_t direction, const uint64_t *input, size_t point_num, uint64_t *output)
{
    COMMON_ENTRY_FILE(fft_file_, fft);
    fft->complex_uint16(shift, direction, input, point_num, output);
}

/* AES */

void aes_ecb128_hard_decrypt(const uint8_t *input_key, const uint8_t *input_data, size_t input_len, uint8_t *output_data)
{
    COMMON_ENTRY_FILE(aes_file_, aes);
    aes->aes_ecb128_hard_decrypt({ input_key, 16 }, { input_data, std::ptrdiff_t(input_len) }, { output_data, std::ptrdiff_t(input_len) });
}

void aes_ecb128_hard_encrypt(const uint8_t *input_key, const uint8_t *input_data, size_t input_len, uint8_t *output_data)
{
    COMMON_ENTRY_FILE(aes_file_, aes);
    aes->aes_ecb128_hard_encrypt({ input_key, 16 }, { input_data, std::ptrdiff_t(input_len) }, { output_data, std::ptrdiff_t(input_len) });
}

void aes_ecb192_hard_decrypt(const uint8_t *input_key, const uint8_t *input_data, size_t input_len, uint8_t *output_data)
{
    COMMON_ENTRY_FILE(aes_file_, aes);
    aes->aes_ecb192_hard_decrypt({ input_key, 24 }, { input_data, std::ptrdiff_t(input_len) }, { output_data, std::ptrdiff_t(input_len) });
}

void aes_ecb192_hard_encrypt(const uint8_t *input_key, const uint8_t *input_data, size_t input_len, uint8_t *output_data)
{
    COMMON_ENTRY_FILE(aes_file_, aes);
    aes->aes_ecb192_hard_encrypt({ input_key, 24 }, { input_data, std::ptrdiff_t(input_len) }, { output_data, std::ptrdiff_t(input_len) });
}

void aes_ecb256_hard_decrypt(const uint8_t *input_key, const uint8_t *input_data, size_t input_len, uint8_t *output_data)
{
    COMMON_ENTRY_FILE(aes_file_, aes);
    aes->aes_ecb256_hard_decrypt({ input_key, 32 }, { input_data, std::ptrdiff_t(input_len) }, { output_data, std::ptrdiff_t(input_len) });
}

void aes_ecb256_hard_encrypt(const uint8_t *input_key, const uint8_t *input_data, size_t input_len, uint8_t *output_data)
{
    COMMON_ENTRY_FILE(aes_file_, aes);
    aes->aes_ecb256_hard_encrypt({ input_key, 32 }, { input_data, std::ptrdiff_t(input_len) }, { output_data, std::ptrdiff_t(input_len) });
}

void aes_cbc128_hard_decrypt(cbc_context_t *context, const uint8_t *input_data, size_t input_len, uint8_t *output_data)
{
    COMMON_ENTRY_FILE(aes_file_, aes);
    aes->aes_cbc128_hard_decrypt(*context, { input_data, std::ptrdiff_t(input_len) }, { output_data, std::ptrdiff_t(input_len) });
}

void aes_cbc128_hard_encrypt(cbc_context_t *context, const uint8_t *input_data, size_t input_len, uint8_t *output_data)
{
    COMMON_ENTRY_FILE(aes_file_, aes);
    aes->aes_cbc128_hard_encrypt(*context, { input_data, std::ptrdiff_t(input_len) }, { output_data, std::ptrdiff_t(input_len) });
}

void aes_cbc192_hard_decrypt(cbc_context_t *context, const uint8_t *input_data, size_t input_len, uint8_t *output_data)
{
    COMMON_ENTRY_FILE(aes_file_, aes);
    aes->aes_cbc192_hard_decrypt(*context, { input_data, std::ptrdiff_t(input_len) }, { output_data, std::ptrdiff_t(input_len) });
}

void aes_cbc192_hard_encrypt(cbc_context_t *context, const uint8_t *input_data, size_t input_len, uint8_t *output_data)
{
    COMMON_ENTRY_FILE(aes_file_, aes);
    aes->aes_cbc192_hard_encrypt(*context, { input_data, std::ptrdiff_t(input_len) }, { output_data, std::ptrdiff_t(input_len) });
}

void aes_cbc256_hard_decrypt(cbc_context_t *context, const uint8_t *input_data, size_t input_len, uint8_t *output_data)
{
    COMMON_ENTRY_FILE(aes_file_, aes);
    aes->aes_cbc256_hard_decrypt(*context, { input_data, std::ptrdiff_t(input_len) }, { output_data, std::ptrdiff_t(input_len) });
}

void aes_cbc256_hard_encrypt(cbc_context_t *context, const uint8_t *input_data, size_t input_len, uint8_t *output_data)
{
    COMMON_ENTRY_FILE(aes_file_, aes);
    aes->aes_cbc256_hard_encrypt(*context, { input_data, std::ptrdiff_t(input_len) }, { output_data, std::ptrdiff_t(input_len) });
}

void aes_gcm128_hard_decrypt(gcm_context_t *context, const uint8_t *input_data, size_t input_len, uint8_t *output_data, uint8_t *gcm_tag)
{
    COMMON_ENTRY_FILE(aes_file_, aes);
    aes->aes_gcm128_hard_decrypt(*context, { input_data, std::ptrdiff_t(input_len) }, { output_data, std::ptrdiff_t(input_len) }, { gcm_tag, 16 });
}

void aes_gcm128_hard_encrypt(gcm_context_t *context, const uint8_t *input_data, size_t input_len, uint8_t *output_data, uint8_t *gcm_tag)
{
    COMMON_ENTRY_FILE(aes_file_, aes);
    aes->aes_gcm128_hard_encrypt(*context, { input_data, std::ptrdiff_t(input_len) }, { output_data, std::ptrdiff_t(input_len) }, { gcm_tag, 16 });
}

void aes_gcm192_hard_decrypt(gcm_context_t *context, const uint8_t *input_data, size_t input_len, uint8_t *output_data, uint8_t *gcm_tag)
{
    COMMON_ENTRY_FILE(aes_file_, aes);
    aes->aes_gcm192_hard_decrypt(*context, { input_data, std::ptrdiff_t(input_len) }, { output_data, std::ptrdiff_t(input_len) }, { gcm_tag, 16 });
}

void aes_gcm192_hard_encrypt(gcm_context_t *context, const uint8_t *input_data, size_t input_len, uint8_t *output_data, uint8_t *gcm_tag)
{
    COMMON_ENTRY_FILE(aes_file_, aes);
    aes->aes_gcm192_hard_encrypt(*context, { input_data, std::ptrdiff_t(input_len) }, { output_data, std::ptrdiff_t(input_len) }, { gcm_tag, 16 });
}

void aes_gcm256_hard_decrypt(gcm_context_t *context, const uint8_t *input_data, size_t input_len, uint8_t *output_data, uint8_t *gcm_tag)
{
    COMMON_ENTRY_FILE(aes_file_, aes);
    aes->aes_gcm256_hard_decrypt(*context, { input_data, std::ptrdiff_t(input_len) }, { output_data, std::ptrdiff_t(input_len) }, { gcm_tag, 16 });
}

void aes_gcm256_hard_encrypt(gcm_context_t *context, const uint8_t *input_data, size_t input_len, uint8_t *output_data, uint8_t *gcm_tag)
{
    COMMON_ENTRY_FILE(aes_file_, aes);
    aes->aes_gcm256_hard_encrypt(*context, { input_data, std::ptrdiff_t(input_len) }, { output_data, std::ptrdiff_t(input_len) }, { gcm_tag, 16 });
}

/* SHA */

void sha256_hard_calculate(const uint8_t *input, size_t input_len, uint8_t *output)
{
    COMMON_ENTRY_FILE(sha256_file_, sha256);
    sha256->sha256_hard_calculate({ input, std::ptrdiff_t(input_len) }, { output, 32 });
}

/* TIMER */

size_t timer_set_interval(handle_t file, size_t nanoseconds)
{
    COMMON_ENTRY(timer);
    return timer->set_interval(nanoseconds);
}

void timer_set_on_tick(handle_t file, timer_on_tick_t on_tick, void *ontick_data)
{
    COMMON_ENTRY(timer);
    timer->set_on_tick(on_tick, ontick_data);
}

void timer_set_enable(handle_t file, bool enable)
{
    COMMON_ENTRY(timer);
    timer->set_enable(enable);
}

/* PWM */

uint32_t pwm_get_pin_count(handle_t file)
{
    COMMON_ENTRY(pwm);
    return pwm->get_pin_count();
}

double pwm_set_frequency(handle_t file, double frequency)
{
    COMMON_ENTRY(pwm);
    return pwm->set_frequency(frequency);
}

double pwm_set_active_duty_cycle_percentage(handle_t file, uint32_t pin, double duty_cycle_percentage)
{
    COMMON_ENTRY(pwm);
    return pwm->set_active_duty_cycle_percentage(pin, duty_cycle_percentage);
}

void pwm_set_enable(handle_t file, uint32_t pin, bool enable)
{
    COMMON_ENTRY(pwm);
    pwm->set_enable(pin, enable);
}

/* WDT */
void wdt_set_response_mode(handle_t file, wdt_response_mode_t mode)
{
    COMMON_ENTRY(wdt);
    wdt->set_response_mode(mode);
}

size_t wdt_set_timeout(handle_t file, size_t nanoseconds)
{
    COMMON_ENTRY(wdt);
    return wdt->set_timeout(nanoseconds);
}

void wdt_set_on_timeout(handle_t file, wdt_on_timeout_t handler, void *userdata)
{
    COMMON_ENTRY(wdt);
    wdt->set_on_timeout(handler, userdata);
}

void wdt_restart_counter(handle_t file)
{
    COMMON_ENTRY(wdt);
    wdt->restart_counter();
}

void wdt_set_enable(handle_t file, bool enable)
{
    COMMON_ENTRY(wdt);
    wdt->set_enable(enable);
}

/* RTC */

void rtc_get_datetime(handle_t file, struct tm *datetime)
{
    COMMON_ENTRY(rtc);
    rtc->get_datetime(*datetime);
}

void rtc_set_datetime(handle_t file, const struct tm *datetime)
{
    COMMON_ENTRY(rtc);
    rtc->set_datetime(*datetime);
}

/* KPU */
handle_t kpu_model_load_from_buffer(uint8_t *buffer)
{
    COMMON_ENTRY_FILE(kpu_file_, kpu);
    return kpu->model_load_from_buffer(buffer);
}

int kpu_run(handle_t context, const uint8_t *src)
{
    COMMON_ENTRY_FILE(kpu_file_, kpu);
    return kpu->run(context, src);
}

int kpu_get_output(handle_t context, uint32_t index, uint8_t **data, size_t *size)
{
    COMMON_ENTRY_FILE(kpu_file_, kpu);
    return kpu->get_output(context, index, data, size);
}

/* HAL */

static uintptr_t pic_file_;

typedef struct
{
    pic_irq_handler_t pic_callbacks[IRQN_MAX];
    void *callback_userdata[IRQN_MAX];
} pic_context_t;

static pic_context_t pic_context_;
static SemaphoreHandle_t dma_free_;

static void init_dma_system()
{
    size_t count = 0;
    driver_registry_t *head = g_dma_drivers;
    while (head->name)
    {
        count++;
        head++;
    }

    dma_free_ = xSemaphoreCreateCounting(count, count);
}

void install_hal()
{
    uxCPUClockRate = sysctl_clock_get_freq(SYSCTL_CLOCK_CPU);
    install_hal_drivers();
    pic_file_ = io_open("/dev/pic0");
    configASSERT(pic_file_);

    install_dma_drivers();
    init_dma_system();
}

/* PIC */

void pic_set_irq_enable(uint32_t irq, bool enable)
{
    COMMON_ENTRY_FILE(pic_file_, pic);
    pic->set_irq_enable(irq, enable);
}

void pic_set_irq_priority(uint32_t irq, uint32_t priority)
{
    COMMON_ENTRY_FILE(pic_file_, pic);
    pic->set_irq_priority(irq, priority);
}

void pic_set_irq_handler(uint32_t irq, pic_irq_handler_t handler, void *userdata)
{
    atomic_set(pic_context_.callback_userdata + irq, userdata);
    pic_context_.pic_callbacks[irq] = handler;
}

void sys::kernel_iface_pic_on_irq(uint32_t irq)
{
    pic_irq_handler_t handler = pic_context_.pic_callbacks[irq];
    if (handler)
        handler(pic_context_.callback_userdata[irq]);
}

/* DMA */

handle_t dma_open_free()
{
    _lock_acquire_recursive(&dma_lock);
    configASSERT(xSemaphoreTake(dma_free_, portMAX_DELAY) == pdTRUE);

    driver_registry_t *head = g_dma_drivers;
    object_accessor<driver> dma;
    while (head->name)
    {
        auto &driver = head->driver_ptr;
        try
        {
            dma = make_accessor(driver);
            break;
        }
        catch (...)
        {
            head++;
        }
    }

    configASSERT(dma);
    uintptr_t handle = io_alloc_handle(io_alloc_file(std::move(dma)));
    _lock_release_recursive(&dma_lock);

    return handle;
}

void dma_close(handle_t file)
{
    _lock_acquire_recursive(&dma_lock);
    io_close(file);
    _lock_release_recursive(&dma_lock);
}

static void dma_add_free()
{
    xSemaphoreGive(dma_free_);
}

void dma_set_request_source(handle_t file, uint32_t request)
{
    COMMON_ENTRY(dma);
    dma->set_select_request(request);
}

void dma_transmit_async(handle_t file, const volatile void *src, volatile void *dest, bool src_inc, bool dest_inc, size_t element_size, size_t count, size_t burst_size, SemaphoreHandle_t completion_event)
{
    COMMON_ENTRY(dma);
    dma->transmit_async(src, dest, src_inc, dest_inc, element_size, count, burst_size, completion_event);
}

void dma_transmit(handle_t file, const volatile void *src, volatile void *dest, bool src_inc, bool dest_inc, size_t element_size, size_t count, size_t burst_size)
{
    SemaphoreHandle_t event = xSemaphoreCreateBinary();
    dma_transmit_async(file, src, dest, src_inc, dest_inc, element_size, count, burst_size, event);
    configASSERT(xSemaphoreTake(event, portMAX_DELAY) == pdTRUE);
    vSemaphoreDelete(event);
}

void dma_loop_async(handle_t file, const volatile void **srcs, size_t src_num, volatile void **dests, size_t dest_num, bool src_inc, bool dest_inc, size_t element_size, size_t count, size_t burst_size, dma_stage_completion_handler_t stage_completion_handler, void *stage_completion_handler_data, SemaphoreHandle_t completion_event, int *stop_signal)
{
    COMMON_ENTRY(dma);
    dma->loop_async(srcs, src_num, dests, dest_num, src_inc, dest_inc, element_size, count, burst_size, stage_completion_handler, stage_completion_handler_data, completion_event, stop_signal);
}

void dma_stop(handle_t file)
{
    COMMON_ENTRY(dma);
    dma->stop();
}
/* System */

driver_registry_t *sys::system_install_driver(const char *name, object_ptr<driver> driver)
{
    size_t i = 0;
    driver_registry_t *head = g_custom_drivers;
    for (i = 0; i < MAX_CUSTOM_DRIVERS; i++, head++)
    {
        if (!head->name)
        {
            head->name = name ? strdup(name) : dummy_driver_name;
            head->driver_ptr = driver;

            driver->install();
            return head;
        }
    }

    configASSERT(!"Max custom drivers exceeded.");
    return nullptr;
}

object_accessor<driver> sys::system_open_driver(const char *name)
{
    auto driver = find_free_driver(g_system_drivers, name);
    if (!driver)
        driver = find_free_driver(g_hal_drivers, name);
    if (!driver)
        driver = find_free_dynamic_driver(name);
    if (!driver)
        throw std::runtime_error("driver is not found.");
    return driver;
}

handle_t sys::system_alloc_handle(object_accessor<object_access> object)
{
    return io_alloc_handle(io_alloc_file(std::move(object)));
}

object_accessor<object_access> &sys::system_handle_to_object(handle_t file)
{
    if (file < HANDLE_OFFSET)
        throw std::invalid_argument("Invalid handle.");

    _file *rfile = (_file *)handles_[file - HANDLE_OFFSET];
    configASSERT(rfile);
    return rfile->object;
}

uint32_t system_set_cpu_frequency(uint32_t frequency)
{
    uint32_t divider = (sysctl->clk_sel0.aclk_divider_sel + 1) * 2;
    uint32_t result = sysctl_pll_set_freq(SYSCTL_PLL0, divider * frequency) / divider;
    uxCPUClockRate = result;
    uarths_init();
    return result;
}
