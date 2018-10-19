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
#include <hal.h>
#include <math.h>
#include <semphr.h>
#include <spi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysctl.h>
#include <utility.h>

/* SPI Controller */

#define COMMON_ENTRY                                                      \
    spi_data *data = (spi_data *)userdata;                                \
    volatile spi_t * spi = (volatile spi_t *)data->base_addr;             \
    (void)spi;

#define TMOD_MASK (3 << data->tmod_off)
#define TMOD_VALUE(value) (value << data->tmod_off)
#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))

typedef struct
{
    sysctl_clock_t clock;
    uintptr_t base_addr;
    uint8_t mod_off;
    uint8_t dfs_off;
    uint8_t tmod_off;
    uint8_t frf_off;
    sysctl_dma_select_t dma_req_base;

    struct
    {
        spi_frame_format_t frame_format;
        size_t chip_select_mask;
        size_t buffer_width;
        size_t inst_width;
        size_t addr_width;
        SemaphoreHandle_t free_mutex;
    };
} spi_data;

static void spi_install(void *userdata)
{
    COMMON_ENTRY;

    /* GPIO clock under APB0 clock, so enable APB0 clock firstly */
    sysctl_clock_enable(data->clock);
    data->free_mutex = xSemaphoreCreateMutex();
}

static int spi_open(void *userdata)
{
    return 1;
}

static void spi_close(void *userdata)
{
}

/* SPI Device */

#define COMMON_DEV_ENTRY                               \
    spi_dev_data *dev_data = (spi_dev_data *)userdata; \
    spi_data *data = (spi_data *)dev_data->spi_data;

typedef struct
{
    spi_data *spi_data;
    spi_mode_t mode;
    spi_frame_format_t frame_format;
    size_t chip_select_mask;
    size_t data_bit_length;
    size_t instruction_length;
    size_t address_length;
    size_t wait_cycles;
    spi_inst_addr_trans_mode_t trans_mode;
    uint32_t baud_rate;
} spi_dev_data;

static void spi_dev_install(void *userdata);
static int spi_dev_open(void *userdata);
static void spi_dev_close(void *userdata);
static int spi_dev_read(uint8_t *buffer, size_t len, void *userdata);
static int spi_dev_write(const uint8_t *buffer, size_t len, void *userdata);
static void spi_dev_config_non_standard(uint32_t instruction_length, uint32_t address_length, uint32_t wait_cycles, spi_inst_addr_trans_mode_t trans_mode, void *userdata);
static double spi_dev_set_clock_rate(double clock_rate, void *userdata);
static int spi_dev_transfer_sequential(const uint8_t *write_buffer, size_t write_len, uint8_t *read_buffer, size_t read_len, void *userdata);
static int spi_dev_transfer_full_duplex(const uint8_t *write_buffer, size_t write_len, uint8_t *read_buffer, size_t read_len, void *userdata);
static void spi_dev_fill(uint32_t instruction, uint32_t address, uint32_t value, size_t count, void *userdata);

static spi_device_driver_t * spi_get_device(spi_mode_t mode, spi_frame_format_t frame_format, uint32_t chip_select_mask, uint32_t data_bit_length, void *userdata)
{
    spi_device_driver_t *driver = (spi_device_driver_t *)malloc(sizeof(spi_device_driver_t));
    memset(driver, 0, sizeof(spi_device_driver_t));

    spi_dev_data* dev_data = (spi_dev_data*)malloc(sizeof(spi_dev_data));
    dev_data->spi_data = userdata;
    dev_data->mode = mode;
    dev_data->frame_format = frame_format;
    dev_data->chip_select_mask = chip_select_mask;
    dev_data->data_bit_length = data_bit_length;
    dev_data->baud_rate = 0x2;

    driver->base.userdata = dev_data;
    driver->base.install = spi_dev_install;
    driver->base.open = spi_dev_open;
    driver->base.close = spi_dev_close;
    driver->read = spi_dev_read;
    driver->write = spi_dev_write;
    driver->config = spi_dev_config_non_standard;
    driver->set_clock_rate = spi_dev_set_clock_rate;
    driver->transfer_sequential = spi_dev_transfer_sequential;
    driver->transfer_full_duplex = spi_dev_transfer_full_duplex;
    driver->fill = spi_dev_fill;
    return driver;
}

static int get_buffer_width(size_t data_bit_length)
{
    if (data_bit_length <= 8)
        return 1;
    else if (data_bit_length <= 16)
        return 2;
    return 4;
}

static int get_inst_addr_width(size_t length)
{
    if (length == 0)
        return 0;
    else if (length <= 8)
        return 1;
    else if (length <= 16)
        return 2;
    else if (length <= 24)
        return 3;
    return 4;
}

static void spi_config_as_master(spi_mode_t mode, spi_frame_format_t frame_format, uint32_t chip_select_mask, uint32_t data_bit_length, uint32_t baud_rate, void *userdata)
{
    COMMON_ENTRY;
    configASSERT(data_bit_length >= 4 && data_bit_length <= 32);
    configASSERT(chip_select_mask);

    switch (frame_format)
    {
    case SPI_FF_DUAL:
        configASSERT(data_bit_length % 2 == 0);
        break;
    case SPI_FF_QUAD:
        configASSERT(data_bit_length % 4 == 0);
        break;
    case SPI_FF_OCTAL:
        configASSERT(data_bit_length % 8 == 0);
        break;
    default:
        break;
    }

    spi->baudr = baud_rate;
    spi->imr = 0x00;
    spi->dmacr = 0x00;
    spi->dmatdlr = 0x10;
    spi->dmardlr = 0x0;
    spi->ser = 0x00;
    spi->ssienr = 0x00;
    spi->ctrlr0 = (mode << data->mod_off) | (frame_format << data->frf_off) | ((data_bit_length - 1) << data->dfs_off);
    spi->spi_ctrlr0 = 0;

    data->chip_select_mask = chip_select_mask;
    data->frame_format = frame_format;
    data->buffer_width = get_buffer_width(data_bit_length);
    data->inst_width = 0;
    data->addr_width = 0;
}

static void spi_config(uint32_t instruction_length, uint32_t address_length, uint32_t wait_cycles, spi_inst_addr_trans_mode_t trans_mode, void *userdata)
{
    COMMON_ENTRY;
    configASSERT(data->frame_format != SPI_FF_STANDARD);
    configASSERT(wait_cycles < (1 << 5));

    uint32_t inst_l = 0;
    switch (instruction_length)
    {
    case 0:
        inst_l = 0;
        break;
    case 4:
        inst_l = 1;
        break;
    case 8:
        inst_l = 2;
        break;
    case 16:
        inst_l = 3;
        break;
    default:
        configASSERT("Invalid instruction length");
        break;
    }

    uint32_t trans = 0;
    switch (trans_mode)
    {
    case SPI_AITM_STANDARD:
        trans = 0;
        break;
    case SPI_AITM_ADDR_STANDARD:
        trans = 1;
        break;
    case SPI_AITM_AS_FRAME_FORMAT:
        trans = 2;
        break;
    default:
        configASSERT("Invalid trans mode");
        break;
    }

    configASSERT(address_length % 4 == 0 && address_length <= 60);
    uint32_t addr_l = address_length / 4;

    spi->spi_ctrlr0 = (wait_cycles << 11) | (inst_l << 8) | (addr_l << 2) | trans;
    data->inst_width = get_inst_addr_width(instruction_length);
    data->addr_width = get_inst_addr_width(address_length);
}

static void write_inst_addr(volatile uint32_t* dr, const uint8_t** buffer, size_t width)
{
    configASSERT(width <= 4);
    if (width)
    {
        uint32_t cmd = 0;
        uint8_t *pcmd = (uint8_t*)&cmd;
        size_t i;
        for (i = 0; i < width; i++)
        {
            pcmd[i] = **buffer;
            ++(*buffer);
        }

        *dr = cmd;
    }
}

static int spi_read(uint8_t *buffer, size_t len, void *userdata)
{
    COMMON_ENTRY;

    size_t frames = len / data->buffer_width;
    uintptr_t dma_read = dma_open_free();
    dma_set_request_source(dma_read, data->dma_req_base);

    uint8_t *ori_buffer = buffer;

    set_bit_mask(&spi->ctrlr0, TMOD_MASK, TMOD_VALUE(2));
    spi->ctrlr1 = frames - 1;
    spi->dmacr = 0x1;
    spi->ssienr = 0x01;

    SemaphoreHandle_t event_read = xSemaphoreCreateBinary();

    dma_transmit_async(dma_read, &spi->dr[0], ori_buffer, 0, 1, data->buffer_width, frames, 1, event_read);

    write_inst_addr(spi->dr, (const uint8_t**)&buffer, data->inst_width);
    write_inst_addr(spi->dr, (const uint8_t**)&buffer, data->addr_width);
    spi->ser = data->chip_select_mask;

    configASSERT(xSemaphoreTake(event_read, portMAX_DELAY) == pdTRUE);
    dma_close(dma_read);
    vSemaphoreDelete(event_read);

    spi->ser = 0x00;
    spi->ssienr = 0x00;
    spi->dmacr = 0x00;
    return len;
}

static int spi_write(const uint8_t *buffer, size_t len, void *userdata)
{
    COMMON_ENTRY;

    uintptr_t dma_write = dma_open_free();
    dma_set_request_source(dma_write, data->dma_req_base + 1);

    set_bit_mask(&spi->ctrlr0, TMOD_MASK, TMOD_VALUE(1));
    spi->dmacr = 0x2;
    spi->ssienr = 0x01;

    write_inst_addr(spi->dr, &buffer, data->inst_width);
    write_inst_addr(spi->dr, &buffer, data->addr_width);

    size_t frames = (len - (data->inst_width + data->addr_width)) / data->buffer_width;
    SemaphoreHandle_t event_write = xSemaphoreCreateBinary();
    dma_transmit_async(dma_write, buffer, &spi->dr[0], 1, 0, data->buffer_width, frames, 4, event_write);

    spi->ser = data->chip_select_mask;
    configASSERT(xSemaphoreTake(event_write, portMAX_DELAY) == pdTRUE);
    dma_close(dma_write);
    vSemaphoreDelete(event_write);

    while ((spi->sr & 0x05) != 0x04)
        ;
    spi->ser = 0x00;
    spi->ssienr = 0x00;
    spi->dmacr = 0x00;
    return len;
}

void spi_fill(uint32_t instruction, uint32_t address, uint32_t value, size_t count, void *userdata)
{
    COMMON_ENTRY;

    uintptr_t dma_write = dma_open_free();
    dma_set_request_source(dma_write, data->dma_req_base + 1);

    set_bit_mask(&spi->ctrlr0, TMOD_MASK, TMOD_VALUE(1));
    spi->dmacr = 0x2;
    spi->ssienr = 0x01;

    const uint8_t *buffer = (const uint8_t*)&instruction;
    write_inst_addr(spi->dr, &buffer, data->inst_width);
    buffer = (const uint8_t*)&address;
    write_inst_addr(spi->dr, &buffer, data->addr_width);

    SemaphoreHandle_t event_write = xSemaphoreCreateBinary();
    dma_transmit_async(dma_write, &value, &spi->dr[0], 0, 0, sizeof(uint32_t), count, 4, event_write);

    spi->ser = data->chip_select_mask;
    configASSERT(xSemaphoreTake(event_write, portMAX_DELAY) == pdTRUE);
    dma_close(dma_write);
    vSemaphoreDelete(event_write);

    while ((spi->sr & 0x05) != 0x04)
        ;
    spi->ser = 0x00;
    spi->ssienr = 0x00;
    spi->dmacr = 0x00;
}

static int spi_read_write(const uint8_t *write_buffer, size_t write_len, uint8_t *read_buffer, size_t read_len, void *userdata)
{
    COMMON_ENTRY;
    configASSERT(data->frame_format == SPI_FF_STANDARD);

    uintptr_t dma_write = dma_open_free();
    uintptr_t dma_read = dma_open_free();

    dma_set_request_source(dma_write, data->dma_req_base + 1);
    dma_set_request_source(dma_read, data->dma_req_base);

    size_t tx_frames = write_len / data->buffer_width;
    size_t rx_frames = read_len / data->buffer_width;

    spi->ctrlr1 = rx_frames - 1;
    spi->dmacr = 0x3;
    spi->ssienr = 0x01;
    spi->ser = data->chip_select_mask;
    SemaphoreHandle_t event_read = xSemaphoreCreateBinary(), event_write = xSemaphoreCreateBinary();

    dma_transmit_async(dma_read, &spi->dr[0], read_buffer, 0, 1, data->buffer_width, rx_frames, 1, event_read);
    dma_transmit_async(dma_write, write_buffer, &spi->dr[0], 1, 0, data->buffer_width, tx_frames, 4, event_write);

    configASSERT(xSemaphoreTake(event_read, portMAX_DELAY) == pdTRUE && xSemaphoreTake(event_write, portMAX_DELAY) == pdTRUE);

    dma_close(dma_write);
    dma_close(dma_read);
    vSemaphoreDelete(event_read);
    vSemaphoreDelete(event_write);

    spi->ser = 0x00;
    spi->ssienr = 0x00;
    spi->dmacr = 0x00;

    return read_len;
}

static int spi_transfer_full_duplex(const uint8_t *write_buffer, size_t write_len, uint8_t *read_buffer, size_t read_len, void *userdata)
{
    COMMON_ENTRY;
    configASSERT(data->frame_format == SPI_FF_STANDARD);
    set_bit_mask(&spi->ctrlr0, TMOD_MASK, TMOD_VALUE(0));
    return spi_read_write(write_buffer, write_len, read_buffer, read_len, userdata);
}

static int spi_transfer_sequential(const uint8_t *write_buffer, size_t write_len, uint8_t *read_buffer, size_t read_len, void *userdata)
{
    COMMON_ENTRY;
    configASSERT(data->frame_format == SPI_FF_STANDARD);
    set_bit_mask(&spi->ctrlr0, TMOD_MASK, TMOD_VALUE(3));
    return spi_read_write(write_buffer, write_len, read_buffer, read_len, userdata);
}

static void entry_exclusive(spi_dev_data* dev_data)
{
    spi_data *data = (spi_data *)dev_data->spi_data;
    configASSERT(xSemaphoreTake(data->free_mutex, portMAX_DELAY) == pdTRUE);
    spi_config_as_master(dev_data->mode, dev_data->frame_format, dev_data->chip_select_mask, dev_data->data_bit_length, dev_data->baud_rate, data);
    if (dev_data->frame_format != SPI_FF_STANDARD)
        spi_config(dev_data->instruction_length, dev_data->address_length, dev_data->wait_cycles, dev_data->trans_mode, data);
}

static void exit_exclusive(spi_dev_data* dev_data)
{
    spi_data *data = (spi_data *)dev_data->spi_data;
    xSemaphoreGive(data->free_mutex);
}

static void spi_dev_install(void *userdata)
{
}

static int spi_dev_open(void *userdata)
{
    return 1;
}

static void spi_dev_close(void *userdata)
{
}

static int spi_dev_read(uint8_t *buffer, size_t len, void *userdata)
{
    COMMON_DEV_ENTRY;
    entry_exclusive(dev_data);
    int ret = spi_read(buffer, len, data);
    exit_exclusive(dev_data);
    return ret;
}

static int spi_dev_write(const uint8_t *buffer, size_t len, void *userdata)
{
    COMMON_DEV_ENTRY;
    entry_exclusive(dev_data);
    int ret = spi_write(buffer, len, data);
    exit_exclusive(dev_data);
    return ret;
}

static void spi_dev_config_non_standard(uint32_t instruction_length, uint32_t address_length, uint32_t wait_cycles, spi_inst_addr_trans_mode_t trans_mode, void *userdata)
{
    spi_dev_data* dev_data = (spi_dev_data*)userdata;
    dev_data->instruction_length = instruction_length;
    dev_data->address_length = address_length;
    dev_data->wait_cycles = wait_cycles;
    dev_data->trans_mode = trans_mode;
}

static double spi_dev_set_clock_rate(double clock_rate, void *userdata)
{
    COMMON_DEV_ENTRY;
    double clk = (double)sysctl_clock_get_freq(data->clock);
    uint32_t div = min(65534, max((uint32_t)ceil(clk / clock_rate), 2));
    if (div & 1)
        div++;
    dev_data->baud_rate = div;
    return clk / div;
}

static int spi_dev_transfer_sequential(const uint8_t *write_buffer, size_t write_len, uint8_t *read_buffer, size_t read_len, void *userdata)
{
    COMMON_DEV_ENTRY;
    entry_exclusive(dev_data);
    int ret = spi_transfer_sequential(write_buffer, write_len, read_buffer, read_len, data);
    exit_exclusive(dev_data);
    return ret;
}

static int spi_dev_transfer_full_duplex(const uint8_t *write_buffer, size_t write_len, uint8_t *read_buffer, size_t read_len, void *userdata)
{
    COMMON_DEV_ENTRY;
    entry_exclusive(dev_data);
    int ret = spi_transfer_full_duplex(write_buffer, write_len, read_buffer, read_len, data);
    exit_exclusive(dev_data);
    return ret;
}

static void spi_dev_fill(uint32_t instruction, uint32_t address, uint32_t value, size_t count, void *userdata)
{
    COMMON_DEV_ENTRY;
    entry_exclusive(dev_data);
    spi_fill(instruction, address, value, count, data);
    exit_exclusive(dev_data);
}

static spi_data dev0_data = {SYSCTL_CLOCK_SPI0, SPI0_BASE_ADDR, 6, 16, 8, 21, SYSCTL_DMA_SELECT_SSI0_RX_REQ, {0}};
static spi_data dev1_data = {SYSCTL_CLOCK_SPI1, SPI1_BASE_ADDR, 6, 16, 8, 21, SYSCTL_DMA_SELECT_SSI1_RX_REQ, {0}};
static spi_data dev3_data = {SYSCTL_CLOCK_SPI3, SPI3_BASE_ADDR, 8, 0, 10, 22, SYSCTL_DMA_SELECT_SSI3_RX_REQ, {0}};

const spi_driver_t g_spi_driver_spi0 = {{&dev0_data, spi_install, spi_open, spi_close}, spi_get_device};
const spi_driver_t g_spi_driver_spi1 = {{&dev1_data, spi_install, spi_open, spi_close}, spi_get_device};
const spi_driver_t g_spi_driver_spi3 = {{&dev3_data, spi_install, spi_open, spi_close}, spi_get_device};
