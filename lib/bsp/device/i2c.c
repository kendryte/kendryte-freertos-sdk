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
#include <i2c.h>
#include <plic.h>
#include <semphr.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysctl.h>

/* I2C Controller */

#define COMMON_ENTRY                                                      \
    i2c_data *data = (i2c_data *)userdata;                                \
    volatile i2c_t *i2c = (volatile i2c_t *)data->base_addr;              \
    (void)i2c;

typedef struct
{
    sysctl_clock_t clock;
    sysctl_threshold_t threshold;
    sysctl_dma_select_t dma_req_base;
    uintptr_t base_addr;
    struct
    {
        SemaphoreHandle_t free_mutex;
        i2c_slave_handler_t slave_handler;
    };
} i2c_data;

static void i2c_install(void *userdata)
{
    COMMON_ENTRY;

    /* GPIO clock under APB0 clock, so enable APB0 clock firstly */
    sysctl_clock_enable(data->clock);
    sysctl_clock_set_threshold(data->threshold, 3);
    data->free_mutex = xSemaphoreCreateMutex();
}

static int i2c_open(void *userdata)
{
    return 1;
}

static void i2c_close(void *userdata)
{
}

static double i2c_get_hlcnt(double clock_rate, i2c_data *data, uint32_t *hcnt, uint32_t *lcnt)
{
    uint32_t v_i2c_freq = sysctl_clock_get_freq(data->clock);
    uint16_t v_period_clk_cnt = v_i2c_freq / clock_rate / 2;

    if (v_period_clk_cnt == 0)
        v_period_clk_cnt = 1;

    *hcnt = v_period_clk_cnt;
    *lcnt = v_period_clk_cnt;
    return v_i2c_freq / v_period_clk_cnt * 2;
}

/* I2C Device */

#define COMMON_DEV_ENTRY                               \
    i2c_dev_data *dev_data = (i2c_dev_data *)userdata; \
    i2c_data *data = (i2c_data *)dev_data->i2c_data;

typedef struct
{
    i2c_data *i2c_data;
    size_t slave_address;
    size_t address_width;
    uint32_t hcnt;
    uint32_t lcnt;
} i2c_dev_data;

static void i2c_dev_install(void *userdata);
static int i2c_dev_open(void *userdata);
static void i2c_dev_close(void *userdata);
static double i2c_dev_set_clock_rate(double clock_rate, void *userdata);
static int i2c_dev_read(uint8_t *buffer, size_t len, void *userdata);
static int i2c_dev_write(const uint8_t *buffer, size_t len, void *userdata);
static int i2c_dev_transfer_sequential(const uint8_t *write_buffer, size_t write_len, uint8_t *read_buffer, size_t read_len, void *userdata);

static i2c_device_driver_t * i2c_get_device(uint32_t slave_address, uint32_t address_width, void *userdata)
{
    i2c_device_driver_t *driver = (i2c_device_driver_t *)malloc(sizeof(i2c_device_driver_t));
    memset(driver, 0, sizeof(i2c_device_driver_t));

    i2c_dev_data *dev_data = (i2c_dev_data *)malloc(sizeof(i2c_dev_data));
    dev_data->slave_address = slave_address;
    dev_data->address_width = address_width;
    dev_data->hcnt = 37;
    dev_data->lcnt = 40;
    dev_data->i2c_data = userdata;

    driver->base.userdata = dev_data;
    driver->base.install = i2c_dev_install;
    driver->base.open = i2c_dev_open;
    driver->base.close = i2c_dev_close;
    driver->set_clock_rate = i2c_dev_set_clock_rate;
    driver->read = i2c_dev_read;
    driver->write = i2c_dev_write;
    driver->transfer_sequential = i2c_dev_transfer_sequential;
    return driver;
}

static void i2c_config_as_master(uint32_t slave_address, uint32_t address_width, uint32_t hcnt, uint32_t lcnt, void *userdata)
{
    configASSERT(address_width == 7 || address_width == 10);
    COMMON_ENTRY;

    i2c->enable = 0;
    i2c->con = I2C_CON_MASTER_MODE | I2C_CON_SLAVE_DISABLE | I2C_CON_RESTART_EN | (address_width == 10 ? I2C_CON_10BITADDR_SLAVE : 0) | I2C_CON_SPEED(1);
    i2c->ss_scl_hcnt = I2C_SS_SCL_HCNT_COUNT(hcnt);
    i2c->ss_scl_lcnt = I2C_SS_SCL_LCNT_COUNT(lcnt);
    i2c->tar = I2C_TAR_ADDRESS(slave_address);
    i2c->intr_mask = 0;

    i2c->dma_cr = 0x3;
    i2c->dma_rdlr = 0;
    i2c->dma_tdlr = 4;

    i2c->enable = I2C_ENABLE_ENABLE;
}

static int i2c_read(uint8_t *buffer, size_t len, void *userdata)
{
    COMMON_ENTRY;

    uint8_t fifo_len, index;
    uint8_t rx_len = len;
    size_t read = 0;

    fifo_len = len < 7 ? len : 7;
    for (index = 0; index < fifo_len; index++)
        i2c->data_cmd = I2C_DATA_CMD_CMD;
    len -= fifo_len;
    while (len || rx_len)
    {
        fifo_len = i2c->rxflr;
        fifo_len = rx_len < fifo_len ? rx_len : fifo_len;
        for (index = 0; index < fifo_len; index++)
            *buffer++ = i2c->data_cmd;
        rx_len -= fifo_len;
        read += fifo_len;
        fifo_len = 8 - i2c->txflr;
        fifo_len = len < fifo_len ? len : fifo_len;
        for (index = 0; index < fifo_len; index++)
            i2c->data_cmd = I2C_DATA_CMD_CMD;
        if (i2c->tx_abrt_source != 0)
            return read;
        len -= fifo_len;
    }

    return read;
}

static int i2c_write(const uint8_t *buffer, size_t len, void *userdata)
{
    COMMON_ENTRY;

    uintptr_t dma_write = dma_open_free();

    dma_set_request_source(dma_write, data->dma_req_base + 1);
    dma_transmit(dma_write, buffer, &i2c->data_cmd, 1, 0, 1, len, 4);
    dma_close(dma_write);

    while (i2c->status & I2C_STATUS_ACTIVITY)
    {
        if (i2c->tx_abrt_source != 0)
            configASSERT(!"source abort");
    }
    return len;
}

static int i2c_transfer_sequential(const uint8_t *write_buffer, size_t write_len, uint8_t *read_buffer, size_t read_len, void *userdata)
{
    COMMON_ENTRY;

    uint32_t *write_cmd = malloc(sizeof(uint32_t) * (write_len + read_len));
    size_t i;
    for (i = 0; i < write_len; i++)
        write_cmd[i] = write_buffer[i];
    for (i = 0; i < read_len; i++)
        write_cmd[i + write_len] = I2C_DATA_CMD_CMD;

    uintptr_t dma_write = dma_open_free();
    uintptr_t dma_read = dma_open_free();
    SemaphoreHandle_t event_read = xSemaphoreCreateBinary(), event_write = xSemaphoreCreateBinary();

    dma_set_request_source(dma_write, data->dma_req_base + 1);
    dma_set_request_source(dma_read, data->dma_req_base);

    dma_transmit_async(dma_read, &i2c->data_cmd, read_buffer, 0, 1, 1 /*sizeof(uint32_t)*/, read_len, 1 /*4*/, event_read);
    dma_transmit_async(dma_write, write_cmd, &i2c->data_cmd, 1, 0, sizeof(uint32_t), write_len + read_len, 4, event_write);

    configASSERT(xSemaphoreTake(event_read, portMAX_DELAY) == pdTRUE && xSemaphoreTake(event_write, portMAX_DELAY) == pdTRUE);

    dma_close(dma_write);
    dma_close(dma_read);
    vSemaphoreDelete(event_read);
    vSemaphoreDelete(event_write);
    free(write_cmd);
    return read_len;
}

static void entry_exclusive(i2c_dev_data *dev_data)
{
    i2c_data* data = (i2c_data*)dev_data->i2c_data;
    configASSERT(xSemaphoreTake(data->free_mutex, portMAX_DELAY) == pdTRUE);
    i2c_config_as_master(dev_data->slave_address, dev_data->address_width, dev_data->hcnt, dev_data->lcnt, data);
}

static void exit_exclusive(i2c_dev_data *dev_data)
{
    i2c_data* data = (i2c_data*)dev_data->i2c_data;
    xSemaphoreGive(data->free_mutex);
}

static void i2c_dev_install(void *userdata)
{
}

static int i2c_dev_open(void *userdata)
{
    return 1;
}

static void i2c_dev_close(void *userdata)
{
}

static double i2c_dev_set_clock_rate(double clock_rate, void *userdata)
{
    COMMON_DEV_ENTRY;
    return i2c_get_hlcnt(clock_rate, data, &dev_data->hcnt, &dev_data->lcnt);
}

static int i2c_dev_read(uint8_t *buffer, size_t len, void *userdata)
{
    COMMON_DEV_ENTRY;
    entry_exclusive(dev_data);
    int ret = i2c_read(buffer, len, data);
    exit_exclusive(dev_data);
    return ret;
}

static int i2c_dev_write(const uint8_t *buffer, size_t len, void *userdata)
{
    COMMON_DEV_ENTRY;
    entry_exclusive(dev_data);
    int ret = i2c_write(buffer, len, data);
    exit_exclusive(dev_data);
    return ret;
}

static int i2c_dev_transfer_sequential(const uint8_t *write_buffer, size_t write_len, uint8_t *read_buffer, size_t read_len, void *userdata)
{
    COMMON_DEV_ENTRY;
    entry_exclusive(dev_data);
    int ret = i2c_transfer_sequential(write_buffer, write_len, read_buffer, read_len, data);
    exit_exclusive(dev_data);
    return ret;
}

static void on_i2c_irq(void *userdata)
{
    COMMON_ENTRY;

    uint32_t status = i2c->intr_stat;
    uint32_t dummy;

    if (status & I2C_INTR_STAT_START_DET)
    {
        data->slave_handler.on_event(I2C_EV_START);
        dummy = i2c->clr_start_det;
    }
    if (status & I2C_INTR_STAT_STOP_DET)
    {
        data->slave_handler.on_event(I2C_EV_STOP);
        dummy = i2c->clr_stop_det;
    }
    if (status & I2C_INTR_STAT_RX_FULL)
    {
        data->slave_handler.on_receive(i2c->data_cmd);
    }
    if (status & I2C_INTR_STAT_RD_REQ)
    {
        i2c->data_cmd = data->slave_handler.on_transmit();
        dummy = i2c->clr_rd_req;
    }

    (void)dummy;
}

static void i2c_config_as_slave(uint32_t slave_address, uint32_t address_width, i2c_slave_handler_t* handler, void *userdata)
{
    configASSERT(address_width == 7 || address_width == 10);
    COMMON_ENTRY;

    data->slave_handler.on_event = handler->on_event;
    data->slave_handler.on_receive = handler->on_receive;
    data->slave_handler.on_transmit = handler->on_transmit;

    i2c->enable = 0;
    i2c->con = (address_width == 10 ? I2C_CON_10BITADDR_SLAVE : 0) | I2C_CON_SPEED(1) | I2C_CON_STOP_DET_IFADDRESSED;
    i2c->ss_scl_hcnt = I2C_SS_SCL_HCNT_COUNT(37);
    i2c->ss_scl_lcnt = I2C_SS_SCL_LCNT_COUNT(40);
    i2c->sar = I2C_SAR_ADDRESS(slave_address);
    i2c->rx_tl = I2C_RX_TL_VALUE(0);
    i2c->tx_tl = I2C_TX_TL_VALUE(0);
    i2c->intr_mask = I2C_INTR_MASK_RX_FULL | I2C_INTR_MASK_START_DET | I2C_INTR_MASK_STOP_DET | I2C_INTR_MASK_RD_REQ;

    int i2c_idx = data->clock - SYSCTL_CLOCK_I2C0;
    pic_set_irq_priority(IRQN_I2C0_INTERRUPT + i2c_idx, 1);
    pic_set_irq_handler(IRQN_I2C0_INTERRUPT + i2c_idx, on_i2c_irq, userdata);
    pic_set_irq_enable(IRQN_I2C0_INTERRUPT + i2c_idx, 1);

    i2c->enable = I2C_ENABLE_ENABLE;
}

static double i2c_slave_set_clock_rate(double clock_rate, void *userdata)
{
    COMMON_ENTRY;

    uint32_t hcnt;
    uint32_t lcnt;
    clock_rate = i2c_get_hlcnt(clock_rate, data, &hcnt, &lcnt);
    i2c->ss_scl_hcnt = I2C_SS_SCL_HCNT_COUNT(hcnt);
    i2c->ss_scl_lcnt = I2C_SS_SCL_LCNT_COUNT(lcnt);
    return clock_rate;
}

static i2c_data dev0_data = {SYSCTL_CLOCK_I2C0, SYSCTL_THRESHOLD_I2C0, SYSCTL_DMA_SELECT_I2C0_RX_REQ, I2C0_BASE_ADDR, {0}};
static i2c_data dev1_data = {SYSCTL_CLOCK_I2C1, SYSCTL_THRESHOLD_I2C1, SYSCTL_DMA_SELECT_I2C1_RX_REQ, I2C1_BASE_ADDR, {0}};
static i2c_data dev2_data = {SYSCTL_CLOCK_I2C2, SYSCTL_THRESHOLD_I2C2, SYSCTL_DMA_SELECT_I2C2_RX_REQ, I2C2_BASE_ADDR, {0}};

const i2c_driver_t g_i2c_driver_i2c0 = {{&dev0_data, i2c_install, i2c_open, i2c_close}, i2c_get_device, i2c_config_as_slave, i2c_slave_set_clock_rate};
const i2c_driver_t g_i2c_driver_i2c1 = {{&dev1_data, i2c_install, i2c_open, i2c_close}, i2c_get_device, i2c_config_as_slave, i2c_slave_set_clock_rate};
const i2c_driver_t g_i2c_driver_i2c2 = {{&dev2_data, i2c_install, i2c_open, i2c_close}, i2c_get_device, i2c_config_as_slave, i2c_slave_set_clock_rate};
