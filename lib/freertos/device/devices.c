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
#include "FreeRTOS.h"
#include <atomic.h>
#include "device_priv.h"
#include <devices.h>
#include <driver.h>
#include <hal.h>
#include <plic.h>
#include <semphr.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysctl.h>
#include <uarths.h>

#define MAX_HANDLES 256
#define HANDLE_OFFSET 256
#define MAX_CUSTOM_DRIVERS 32

#define DEFINE_INSTALL_DRIVER(type)                     \
    static void install_##type##_drivers()              \
    {                                                   \
        driver_registry_t *head = g_##type##_drivers;   \
        while (head->name)                              \
        {                                               \
            const driver_base_t *driver = head->driver; \
            driver->install(driver->userdata);          \
            head++;                                     \
        }                                               \
    }

typedef struct
{
    driver_registry_t *driver_reg;
} _file;

static _file *handles_[MAX_HANDLES] = {0};
static driver_registry_t g_custom_drivers[MAX_CUSTOM_DRIVERS] = {0};

uintptr_t fft_file_;
uintptr_t aes_file_;
uintptr_t sha256_file_;

extern UBaseType_t uxCPUClockRate;

DEFINE_INSTALL_DRIVER(hal);
DEFINE_INSTALL_DRIVER(dma);
DEFINE_INSTALL_DRIVER(system);

driver_registry_t * find_free_driver(driver_registry_t *registry, const char *name)
{
    driver_registry_t *head = registry;
    while (head->name)
    {
        if (strcmp(name, head->name) == 0)
        {
            driver_base_t *driver = (driver_base_t*)head->driver;
            if (driver->open(driver->userdata))
                return head;
            else
                return NULL;
        }

        head++;
    }

    return NULL;
}

static driver_registry_t * install_custom_driver_core(const char *name, driver_type type, const void *driver)
{
    size_t i = 0;
    driver_registry_t *head = g_custom_drivers;
    for (i = 0; i < MAX_CUSTOM_DRIVERS; i++, head++)
    {
        if (!head->name)
        {
            head->name = strdup(name);
            head->type = type;
            head->driver = driver;
            return head;
        }
    }

    configASSERT(!"Max custom drivers exceeded.");
    return NULL;
}

void install_drivers()
{
    install_system_drivers();

    fft_file_ = io_open("/dev/fft0");
    aes_file_ = io_open("/dev/aes0");
    sha256_file_ = io_open("/dev/sha256");
}

static _file * io_alloc_file(driver_registry_t *driver_reg)
{
    if (driver_reg)
    {
        _file *file = (_file *)malloc(sizeof(_file));
        if (!file)
            return NULL;
        file->driver_reg = driver_reg;
        return file;
    }

    return NULL;
}

static _file *io_open_reg(driver_registry_t *registry, const char *name, _file **file)
{
    driver_registry_t *driver_reg = find_free_driver(registry, name);
    _file *ret = io_alloc_file(driver_reg);
    *file = ret;
    return ret;
}

/* Generic IO Implementation Helper Macros */

#define DEFINE_READ_PROXY(tl, tu)                                                  \
    if (rfile->driver_reg->type == DRIVER_##tu)                                    \
    {                                                                              \
        const tl##_driver_t *tl = (const tl##_driver_t*)rfile->driver_reg->driver; \
        return tl->read(buffer, len, tl->base.userdata);                           \
    }

#define DEFINE_WRITE_PROXY(tl, tu)                                                 \
    if (rfile->driver_reg->type == DRIVER_##tu)                                    \
    {                                                                              \
        const tl##_driver_t *tl = (const tl##_driver_t*)rfile->driver_reg->driver; \
        return tl->write(buffer, len, tl->base.userdata);                          \
    }

#define DEFINE_CONTROL_PROXY(tl, tu)                                                                            \
    if (rfile->driver_reg->type == DRIVER_##tu)                                                                 \
    {                                                                                                           \
        const tl##_driver_t *tl = (const tl##_driver_t*)rfile->driver_reg->driver;                              \
        return tl->io_control(control_code, write_buffer, write_len, read_buffer, read_len, tl->base.userdata); \
    }

static void dma_add_free();

int io_read(handle_t file, uint8_t *buffer, size_t len)
{
    _file *rfile = (_file *)handles_[file - HANDLE_OFFSET];
    /* clang-format off */
    DEFINE_READ_PROXY(uart, UART)
    else DEFINE_READ_PROXY(i2c_device, I2C_DEVICE)
    else DEFINE_READ_PROXY(spi_device, SPI_DEVICE)
    else
    {
        return -1;
    }
    /* clang-format on */
}

static void io_free(_file *file)
{
    if (file)
    {
        if (file->driver_reg->type == DRIVER_DMA)
            dma_add_free();

        driver_base_t *driver = (driver_base_t *)file->driver_reg->driver;
        driver->close(driver->userdata);

        free(file);
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

    if (file)
        return io_alloc_handle(file);
    configASSERT(file);
    return 0;
}

int io_close(handle_t file)
{
    if (file)
    {
        _file *rfile = (_file *)handles_[file - HANDLE_OFFSET];
        io_free(rfile);
        atomic_set(handles_ + file - HANDLE_OFFSET, 0);
    }

    return 0;
}

int io_write(handle_t file, const uint8_t *buffer, size_t len)
{
    _file *rfile = (_file *)handles_[file - HANDLE_OFFSET];
    /* clang-format off */
    DEFINE_WRITE_PROXY(uart, UART)
    else DEFINE_WRITE_PROXY(i2c_device, I2C_DEVICE)
    else DEFINE_WRITE_PROXY(spi_device, SPI_DEVICE)
    else
    {
        return -1;
    }
    /* clang-format on */
}

int io_control(handle_t file, uint32_t control_code, const uint8_t *write_buffer, size_t write_len, uint8_t *read_buffer, size_t read_len)
{
    _file *rfile = (_file *)handles_[file - HANDLE_OFFSET];
    DEFINE_CONTROL_PROXY(custom, CUSTOM)

    return -1;
}

/* Device IO Implementation Helper Macros */

#define COMMON_ENTRY(tl, tu)                                \
    _file *rfile = (_file *)handles_[file - HANDLE_OFFSET]; \
    configASSERT(rfile->driver_reg->type == DRIVER_##tu);   \
    const tl##_driver_t *tl = (const tl##_driver_t *)rfile->driver_reg->driver;

#define COMMON_ENTRY_FILE(file, tl, tu)                     \
    _file *rfile = (_file *)handles_[file - HANDLE_OFFSET]; \
    configASSERT(rfile->driver_reg->type == DRIVER_##tu);   \
    const tl##_driver_t *tl = (const tl##_driver_t *)rfile->driver_reg->driver;

/* UART */

void uart_config(handle_t file, uint32_t baud_rate, uint32_t databits, uart_stopbits_t stopbits, uart_parity_t parity)
{
    COMMON_ENTRY(uart, UART);
    uart->config(baud_rate, databits, stopbits, parity, uart->base.userdata);
}

/* GPIO */

uint32_t gpio_get_pin_count(handle_t file)
{
    COMMON_ENTRY(gpio, GPIO);
    return gpio->pin_count;
}

void gpio_set_drive_mode(handle_t file, uint32_t pin, gpio_drive_mode_t mode)
{
    COMMON_ENTRY(gpio, GPIO);
    gpio->set_drive_mode(pin, mode, gpio->base.userdata);
}

void gpio_set_pin_edge(handle_t file, uint32_t pin, gpio_pin_edge_t edge)
{
    COMMON_ENTRY(gpio, GPIO);
    gpio->set_pin_edge(pin, edge, gpio->base.userdata);
}

void gpio_set_on_changed(handle_t file, uint32_t pin, gpio_on_changed_t callback, void *userdata)
{
    COMMON_ENTRY(gpio, GPIO);
    gpio->set_on_changed(pin, callback, userdata, gpio->base.userdata);
}

gpio_pin_value_t gpio_get_pin_value(handle_t file, uint32_t pin)
{
    COMMON_ENTRY(gpio, GPIO);
    return gpio->get_pin_value(pin, gpio->base.userdata);
}

void gpio_set_pin_value(handle_t file, uint32_t pin, gpio_pin_value_t value)
{
    COMMON_ENTRY(gpio, GPIO);
    gpio->set_pin_value(pin, value, gpio->base.userdata);
}

/* I2C */

handle_t i2c_get_device(handle_t file, const char *name, uint32_t slave_address, uint32_t address_width)
{
    COMMON_ENTRY(i2c, I2C);
    i2c_device_driver_t *driver = i2c->get_device(slave_address, address_width, i2c->base.userdata);
    driver_registry_t *reg = install_custom_driver_core(name, DRIVER_I2C_DEVICE, driver);
    return io_alloc_handle(io_alloc_file(reg));
}

double i2c_dev_set_clock_rate(handle_t file, double clock_rate)
{
    COMMON_ENTRY(i2c_device, I2C_DEVICE);
    return i2c_device->set_clock_rate(clock_rate, i2c_device->base.userdata);
}

int i2c_dev_transfer_sequential(handle_t file, const uint8_t *write_buffer, size_t write_len, uint8_t *read_buffer, size_t read_len)
{
    COMMON_ENTRY(i2c_device, I2C_DEVICE);
    return i2c_device->transfer_sequential(write_buffer, write_len, read_buffer, read_len, i2c_device->base.userdata);
}

void i2c_config_as_slave(handle_t file, uint32_t slave_address, uint32_t address_width, i2c_slave_handler_t *handler)
{
    COMMON_ENTRY(i2c, I2C);
    i2c->config_as_slave(slave_address, address_width, handler, i2c->base.userdata);
}

double i2c_slave_set_clock_rate(handle_t file, double clock_rate)
{
    COMMON_ENTRY(i2c, I2C);
    return i2c->slave_set_clock_rate(clock_rate, i2c->base.userdata);
}

/* I2S */

void i2s_config_as_render(handle_t file, const audio_format_t *format, size_t delay_ms, i2s_align_mode_t align_mode, size_t channels_mask)
{
    COMMON_ENTRY(i2s, I2S);
    i2s->config_as_render(format, delay_ms, align_mode, channels_mask, i2s->base.userdata);
}

void i2s_config_as_capture(handle_t file, const audio_format_t *format, size_t delay_ms, i2s_align_mode_t align_mode, size_t channels_mask)
{
    COMMON_ENTRY(i2s, I2S);
    i2s->config_as_capture(format, delay_ms, align_mode, channels_mask, i2s->base.userdata);
}

void i2s_get_buffer(handle_t file, uint8_t* *buffer, size_t *frames)
{
    COMMON_ENTRY(i2s, I2S);
    i2s->get_buffer(buffer, frames, i2s->base.userdata);
}

void i2s_release_buffer(handle_t file, size_t frames)
{
    COMMON_ENTRY(i2s, I2S);
    i2s->release_buffer(frames, i2s->base.userdata);
}

void i2s_start(handle_t file)
{
    COMMON_ENTRY(i2s, I2S);
    i2s->start(i2s->base.userdata);
}

void i2s_stop(handle_t file)
{
    COMMON_ENTRY(i2s, I2S);
    i2s->stop(i2s->base.userdata);
}

/* SPI */

handle_t spi_get_device(handle_t file, const char *name, spi_mode_t mode, spi_frame_format_t frame_format, uint32_t chip_select_mask, uint32_t data_bit_length)
{
    COMMON_ENTRY(spi, SPI);
    spi_device_driver_t *driver = spi->get_device(mode, frame_format, chip_select_mask, data_bit_length, spi->base.userdata);
    driver_registry_t *reg = install_custom_driver_core(name, DRIVER_SPI_DEVICE, driver);
    return io_alloc_handle(io_alloc_file(reg));
}

void spi_dev_config_non_standard(handle_t file, uint32_t instruction_length, uint32_t address_length, uint32_t wait_cycles, spi_inst_addr_trans_mode_t trans_mode)
{
    COMMON_ENTRY(spi_device, SPI_DEVICE);
    spi_device->config(instruction_length, address_length, wait_cycles, trans_mode, spi_device->base.userdata);
}

double spi_dev_set_clock_rate(handle_t file, double clock_rate)
{
    COMMON_ENTRY(spi_device, SPI_DEVICE);
    return spi_device->set_clock_rate(clock_rate, spi_device->base.userdata);
}

int spi_dev_transfer_full_duplex(handle_t file, const uint8_t *write_buffer, size_t write_len, uint8_t *read_buffer, size_t read_len)
{
    COMMON_ENTRY(spi_device, SPI_DEVICE);
    return spi_device->transfer_full_duplex(write_buffer, write_len, read_buffer, read_len, spi_device->base.userdata);
}

int spi_dev_transfer_sequential(handle_t file, const uint8_t *write_buffer, size_t write_len, uint8_t *read_buffer, size_t read_len)
{
    COMMON_ENTRY(spi_device, SPI_DEVICE);
    return spi_device->transfer_sequential(write_buffer, write_len, read_buffer, read_len, spi_device->base.userdata);
}

void spi_dev_fill(handle_t file, uint32_t instruction, uint32_t address, uint32_t value, size_t count)
{
    COMMON_ENTRY(spi_device, SPI_DEVICE);
    return spi_device->fill(instruction, address, value, count, spi_device->base.userdata);
}

/* DVP */

void dvp_config(handle_t file, uint32_t width, uint32_t height, bool auto_enable)
{
    COMMON_ENTRY(dvp, DVP);
    dvp->config(width, height, auto_enable, dvp->base.userdata);
}

void dvp_enable_frame(handle_t file)
{
    COMMON_ENTRY(dvp, DVP);
    dvp->enable_frame(dvp->base.userdata);
}

uint32_t dvp_get_output_num(handle_t file)
{
    COMMON_ENTRY(dvp, DVP);
    return dvp->output_num;
}

void dvp_set_signal(handle_t file, dvp_signal_type_t type, bool value)
{
    COMMON_ENTRY(dvp, DVP);
    dvp->set_signal(type, value, dvp->base.userdata);
}

void dvp_set_output_enable(handle_t file, uint32_t index, bool enable)
{
    COMMON_ENTRY(dvp, DVP);
    dvp->set_output_enable(index, enable, dvp->base.userdata);
}

void dvp_set_output_attributes(handle_t file, uint32_t index, video_format_t format, void *output_buffer)
{
    COMMON_ENTRY(dvp, DVP);
    dvp->set_output_attributes(index, format, output_buffer, dvp->base.userdata);
}

void dvp_set_frame_event_enable(handle_t file, dvp_frame_event_t event, bool enable)
{
    COMMON_ENTRY(dvp, DVP);
    dvp->set_frame_event_enable(event, enable, dvp->base.userdata);
}

void dvp_set_on_frame_event(handle_t file, dvp_on_frame_event_t handler, void *userdata)
{
    COMMON_ENTRY(dvp, DVP);
    dvp->set_on_frame_event(handler, userdata, dvp->base.userdata);
}

double dvp_xclk_set_clock_rate(handle_t file, double clock_rate)
{
    COMMON_ENTRY(dvp, DVP);
    return dvp->xclk_set_clock_rate(clock_rate, dvp->base.userdata);
}

/* SSCB */

handle_t sccb_get_device(handle_t file, const char *name, uint32_t slave_address, uint32_t reg_address_width)
{
    COMMON_ENTRY(sccb, SCCB);
    sccb_device_driver_t *driver = sccb->get_device(slave_address, reg_address_width, sccb->base.userdata);
    driver_registry_t *reg = install_custom_driver_core(name, DRIVER_SCCB_DEVICE, driver);
    return io_alloc_handle(io_alloc_file(reg));
}

uint8_t sccb_dev_read_byte(handle_t file, uint16_t reg_address)
{
    COMMON_ENTRY(sccb_device, SCCB_DEVICE);
    return sccb_device->read_byte(reg_address, sccb_device->base.userdata);
}

void sccb_dev_write_byte(handle_t file, uint16_t reg_address, uint8_t value)
{
    COMMON_ENTRY(sccb_device, SCCB_DEVICE);
    sccb_device->write_byte(reg_address, value, sccb_device->base.userdata);
}

/* FFT */

void fft_complex_uint16(uint16_t shift, fft_direction_t direction, const uint64_t *input, size_t point_num, uint64_t *output)
{
    COMMON_ENTRY_FILE(fft_file_, fft, FFT);
    fft->complex_uint16(shift, direction, input, point_num, output, fft->base.userdata);
}

/* AES */

void aes_ecb128_hard_decrypt(const uint8_t *input_key, const uint8_t *input_data, size_t input_len, uint8_t *output_data)
{
    COMMON_ENTRY_FILE(aes_file_, aes, AES);
    aes->aes_ecb128_hard_decrypt(input_key, input_data, input_len, output_data, aes->base.userdata);
}

void aes_ecb128_hard_encrypt(const uint8_t *input_key, const uint8_t *input_data, size_t input_len, uint8_t *output_data)
{
    COMMON_ENTRY_FILE(aes_file_, aes, AES);
    aes->aes_ecb128_hard_encrypt(input_key, input_data, input_len, output_data, aes->base.userdata);
}

void aes_ecb192_hard_decrypt(const uint8_t *input_key, const uint8_t *input_data, size_t input_len, uint8_t *output_data)
{
    COMMON_ENTRY_FILE(aes_file_, aes, AES);
    aes->aes_ecb192_hard_decrypt(input_key, input_data, input_len, output_data, aes->base.userdata);
}

void aes_ecb192_hard_encrypt(const uint8_t *input_key, const uint8_t *input_data, size_t input_len, uint8_t *output_data)
{
    COMMON_ENTRY_FILE(aes_file_, aes, AES);
    aes->aes_ecb192_hard_encrypt(input_key, input_data, input_len, output_data, aes->base.userdata);
}

void aes_ecb256_hard_decrypt(const uint8_t *input_key, const uint8_t *input_data, size_t input_len, uint8_t *output_data)
{
    COMMON_ENTRY_FILE(aes_file_, aes, AES);
    aes->aes_ecb256_hard_decrypt(input_key, input_data, input_len, output_data, aes->base.userdata);
}

void aes_ecb256_hard_encrypt(const uint8_t *input_key, const uint8_t *input_data, size_t input_len, uint8_t *output_data)
{
    COMMON_ENTRY_FILE(aes_file_, aes, AES);
    aes->aes_ecb256_hard_encrypt(input_key, input_data, input_len, output_data, aes->base.userdata);
}

void aes_cbc128_hard_decrypt(cbc_context_t *context, const uint8_t *input_data, size_t input_len, uint8_t *output_data)
{
    COMMON_ENTRY_FILE(aes_file_, aes, AES);
    aes->aes_cbc128_hard_decrypt(context, input_data, input_len, output_data, aes->base.userdata);
}

void aes_cbc128_hard_encrypt(cbc_context_t *context, const uint8_t *input_data, size_t input_len, uint8_t *output_data)
{
    COMMON_ENTRY_FILE(aes_file_, aes, AES);
    aes->aes_cbc128_hard_encrypt(context, input_data, input_len, output_data, aes->base.userdata);
}

void aes_cbc192_hard_decrypt(cbc_context_t *context, const uint8_t *input_data, size_t input_len, uint8_t *output_data)
{
    COMMON_ENTRY_FILE(aes_file_, aes, AES);
    aes->aes_cbc192_hard_decrypt(context, input_data, input_len, output_data, aes->base.userdata);
}

void aes_cbc192_hard_encrypt(cbc_context_t *context, const uint8_t *input_data, size_t input_len, uint8_t *output_data)
{
    COMMON_ENTRY_FILE(aes_file_, aes, AES);
    aes->aes_cbc192_hard_encrypt(context, input_data, input_len, output_data, aes->base.userdata);
}

void aes_cbc256_hard_decrypt(cbc_context_t *context, const uint8_t *input_data, size_t input_len, uint8_t *output_data)
{
    COMMON_ENTRY_FILE(aes_file_, aes, AES);
    aes->aes_cbc256_hard_decrypt(context, input_data, input_len, output_data, aes->base.userdata);
}

void aes_cbc256_hard_encrypt(cbc_context_t *context, const uint8_t *input_data, size_t input_len, uint8_t *output_data)
{
    COMMON_ENTRY_FILE(aes_file_, aes, AES);
    aes->aes_cbc256_hard_encrypt(context, input_data, input_len, output_data, aes->base.userdata);
}

void aes_gcm128_hard_decrypt(gcm_context_t *context, const uint8_t *input_data, size_t input_len, uint8_t *output_data, uint8_t *gcm_tag)
{
    COMMON_ENTRY_FILE(aes_file_, aes, AES);
    aes->aes_gcm128_hard_decrypt(context, input_data, input_len, output_data, gcm_tag, aes->base.userdata);
}

void aes_gcm128_hard_encrypt(gcm_context_t *context, const uint8_t *input_data, size_t input_len, uint8_t *output_data, uint8_t *gcm_tag)
{
    COMMON_ENTRY_FILE(aes_file_, aes, AES);
    aes->aes_gcm128_hard_encrypt(context, input_data, input_len, output_data, gcm_tag, aes->base.userdata);
}

void aes_gcm192_hard_decrypt(gcm_context_t *context, const uint8_t *input_data, size_t input_len, uint8_t *output_data, uint8_t *gcm_tag)
{
    COMMON_ENTRY_FILE(aes_file_, aes, AES);
    aes->aes_gcm192_hard_decrypt(context, input_data, input_len, output_data, gcm_tag, aes->base.userdata);
}

void aes_gcm192_hard_encrypt(gcm_context_t *context, const uint8_t *input_data, size_t input_len, uint8_t *output_data, uint8_t *gcm_tag)
{
    COMMON_ENTRY_FILE(aes_file_, aes, AES);
    aes->aes_gcm192_hard_encrypt(context, input_data, input_len, output_data, gcm_tag, aes->base.userdata);
}

void aes_gcm256_hard_decrypt(gcm_context_t *context, const uint8_t *input_data, size_t input_len, uint8_t *output_data, uint8_t *gcm_tag)
{
    COMMON_ENTRY_FILE(aes_file_, aes, AES);
    aes->aes_gcm256_hard_decrypt(context, input_data, input_len, output_data, gcm_tag, aes->base.userdata);
}

void aes_gcm256_hard_encrypt(gcm_context_t *context, const uint8_t *input_data, size_t input_len, uint8_t *output_data, uint8_t *gcm_tag)
{
    COMMON_ENTRY_FILE(aes_file_, aes, AES);
    aes->aes_gcm256_hard_encrypt(context, input_data, input_len, output_data, gcm_tag, aes->base.userdata);
}

/* SHA */

void sha256_hard_calculate(const uint8_t *input, size_t input_len, uint8_t *output)
{
    COMMON_ENTRY_FILE(sha256_file_, sha256, SHA256);
    sha256->sha256_hard_calculate(input, input_len, output, sha256->base.userdata);
}

/* TIMER */

size_t timer_set_interval(handle_t file, size_t nanoseconds)
{
    COMMON_ENTRY(timer, TIMER);
    return timer->set_interval(nanoseconds, timer->base.userdata);
}

void timer_set_on_tick(handle_t file, timer_on_tick_t on_tick, void *ontick_data)
{
    COMMON_ENTRY(timer, TIMER);
    timer->set_on_tick(on_tick, ontick_data, timer->base.userdata);
}

void timer_set_enable(handle_t file, bool enable)
{
    COMMON_ENTRY(timer, TIMER);
    timer->set_enable(enable, timer->base.userdata);
}

/* PWM */

uint32_t pwm_get_pin_count(handle_t file)
{
    COMMON_ENTRY(pwm, PWM);
    return pwm->pin_count;
}

double pwm_set_frequency(handle_t file, double frequency)
{
    COMMON_ENTRY(pwm, PWM);
    return pwm->set_frequency(frequency, pwm->base.userdata);
}

double pwm_set_active_duty_cycle_percentage(handle_t file, uint32_t pin, double duty_cycle_percentage)
{
    COMMON_ENTRY(pwm, PWM);
    return pwm->set_active_duty_cycle_percentage(pin, duty_cycle_percentage, pwm->base.userdata);
}

void pwm_set_enable(handle_t file, uint32_t pin, bool enable)
{
    COMMON_ENTRY(pwm, PWM);
    pwm->set_enable(pin, enable, pwm->base.userdata);
}

/* WDT */
void wdt_set_response_mode(handle_t file, wdt_response_mode_t mode)
{
    COMMON_ENTRY(wdt, WDT);
    wdt->set_response_mode(mode, wdt->base.userdata);
}

size_t wdt_set_timeout(handle_t file, size_t nanoseconds)
{
    COMMON_ENTRY(wdt, WDT);
    return wdt->set_timeout(nanoseconds, wdt->base.userdata);
}

void wdt_set_on_timeout(handle_t file, wdt_on_timeout_t handler, void *userdata)
{
    COMMON_ENTRY(wdt, WDT);
    wdt->set_on_timeout(handler, userdata, wdt->base.userdata);
}

void wdt_restart_counter(handle_t file)
{
    COMMON_ENTRY(wdt, WDT);
    wdt->restart_counter(wdt->base.userdata);
}

void wdt_set_enable(handle_t file, bool enable)
{
    COMMON_ENTRY(wdt, WDT);
    wdt->set_enable(enable, wdt->base.userdata);
}

/* RTC */

void rtc_get_datetime(handle_t file, struct tm *datetime)
{
    COMMON_ENTRY(rtc, RTC);
    rtc->get_datetime(datetime, rtc->base.userdata);
}

void rtc_set_datetime(handle_t file, const struct tm *datetime)
{
    COMMON_ENTRY(rtc, RTC);
    rtc->set_datetime(datetime, rtc->base.userdata);
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
    COMMON_ENTRY_FILE(pic_file_, pic, PIC);
    pic->set_irq_enable(irq, enable, pic->base.userdata);
}

void pic_set_irq_priority(uint32_t irq, uint32_t priority)
{
    COMMON_ENTRY_FILE(pic_file_, pic, PIC);
    pic->set_irq_priority(irq, priority, pic->base.userdata);
}

void pic_set_irq_handler(uint32_t irq, pic_irq_handler_t handler, void *userdata)
{
    atomic_set(pic_context_.callback_userdata + irq, userdata);
    pic_context_.pic_callbacks[irq] = handler;
}

void kernel_iface_pic_on_irq(uint32_t irq)
{
    pic_irq_handler_t handler = pic_context_.pic_callbacks[irq];
    if (handler)
        handler(pic_context_.callback_userdata[irq]);
}

/* DMA */

handle_t dma_open_free()
{
    configASSERT(xSemaphoreTake(dma_free_, portMAX_DELAY) == pdTRUE);

    driver_registry_t *head = g_dma_drivers, *driver_reg = NULL;
    while (head->name)
    {
        driver_base_t *driver = (driver_base_t*)head->driver;
        if (driver->open(driver->userdata))
        {
            driver_reg = head;
            break;
        }

        head++;
    }

    configASSERT(driver_reg);
    uintptr_t handle = io_alloc_handle(io_alloc_file(driver_reg));
    return handle;
}

void dma_close(handle_t file)
{
    io_close(file);
}

static void dma_add_free()
{
    xSemaphoreGive(dma_free_);
}

void dma_set_request_source(handle_t file, uint32_t request)
{
    COMMON_ENTRY(dma, DMA);
    dma->set_select_request(request, dma->base.userdata);
}

void dma_transmit_async(handle_t file, const volatile void *src, volatile void *dest, bool src_inc, bool dest_inc, size_t element_size, size_t count, size_t burst_size, SemaphoreHandle_t completion_event)
{
    COMMON_ENTRY(dma, DMA);
    dma->transmit_async(src, dest, src_inc, dest_inc, element_size, count, burst_size, completion_event, dma->base.userdata);
}

void dma_transmit(handle_t file, const volatile void *src, volatile void *dest, bool src_inc, bool dest_inc, size_t element_size, size_t count, size_t burst_size)
{
    SemaphoreHandle_t event = xSemaphoreCreateBinary();
    dma_transmit_async(file, src, dest, src_inc, dest_inc, element_size, count, burst_size, event);
    //	printf("event: %p\n", event);
    configASSERT(xSemaphoreTake(event, portMAX_DELAY) == pdTRUE);
    vSemaphoreDelete(event);
}

void dma_loop_async(handle_t file, const volatile void **srcs, size_t src_num, volatile void **dests, size_t dest_num, bool src_inc, bool dest_inc, size_t element_size, size_t count, size_t burst_size, dma_stage_completion_handler_t stage_completion_handler, void *stage_completion_handler_data, SemaphoreHandle_t completion_event, int *stop_signal)
{
    COMMON_ENTRY(dma, DMA);
    dma->loop_async(srcs, src_num, dests, dest_num, src_inc, dest_inc, element_size, count, burst_size, stage_completion_handler, stage_completion_handler_data, completion_event, stop_signal, dma->base.userdata);
}

/* Custom Driver */

void system_install_custom_driver(const char *name, const custom_driver_t *driver)
{
    install_custom_driver_core(name, DRIVER_CUSTOM, driver);
}

/* System */

uint32_t system_set_cpu_frequency(uint32_t frequency)
{
    uint32_t result = sysctl_pll_set_freq(SYSCTL_PLL0, (sysctl->clk_sel0.aclk_divider_sel + 1) * 2 * frequency);
    uxCPUClockRate = result;
    uarths_init();
    return result;
}
