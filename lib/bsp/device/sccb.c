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
#include <dvp.h>
#include <fpioa.h>
#include <hal.h>
#include <semphr.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysctl.h>
#include <utility.h>

/* SCCB Controller */

#define COMMON_ENTRY                                                       \
    sccb_data *data = (sccb_data *)userdata;                               \
    volatile dvp_t *sccb = (volatile dvp_t *)data->base_addr;              \
    (void)sccb;

typedef struct
{
    sysctl_clock_t clock;
    uintptr_t base_addr;
    SemaphoreHandle_t free_mutex;
} sccb_data;

static void sccb_install(void *userdata)
{
    COMMON_ENTRY;

    sysctl_clock_enable(data->clock);
    set_bit_mask(&sccb->sccb_cfg, DVP_SCCB_SCL_LCNT_MASK | DVP_SCCB_SCL_HCNT_MASK, DVP_SCCB_SCL_LCNT(500) | DVP_SCCB_SCL_HCNT(500));

    data->free_mutex = xSemaphoreCreateMutex();
}

static int sccb_open(void *userdata)
{
    return 1;
}

static void sccb_close(void *userdata)
{
}

/* SCCB Device */

#define COMMON_DEV_ENTRY                                 \
    sccb_dev_data *dev_data = (sccb_dev_data *)userdata; \
    sccb_data *data = (sccb_data *)dev_data->sccb_data;  \
    volatile dvp_t *sccb = (volatile dvp_t *)data->base_addr;

typedef struct
{
    sccb_data *sccb_data;
    uint32_t slave_address;
    uint32_t reg_address_width;
} sccb_dev_data;

static void sccb_dev_install(void *userdata);
static int sccb_dev_open(void *userdata);
static void sccb_dev_close(void *userdata);
static uint8_t sccb_dev_read_byte(uint16_t reg_address, void *userdata);
static void sccb_dev_write_byte(uint16_t reg_address, uint8_t value, void *userdata);

static sccb_device_driver_t * sccb_get_device(uint32_t slave_address, uint32_t reg_address_width, void *userdata)
{
    configASSERT(reg_address_width == 8 || reg_address_width == 16);

    sccb_device_driver_t *driver = (sccb_device_driver_t *)malloc(sizeof(sccb_device_driver_t));
    memset(driver, 0, sizeof(sccb_device_driver_t));

    sccb_dev_data* dev_data = (sccb_dev_data*)malloc(sizeof(sccb_dev_data));
    dev_data->slave_address = slave_address;
    dev_data->reg_address_width = reg_address_width;
    dev_data->sccb_data = userdata;

    driver->base.userdata = dev_data;
    driver->base.install = sccb_dev_install;
    driver->base.open = sccb_dev_open;
    driver->base.close = sccb_dev_close;
    driver->read_byte = sccb_dev_read_byte;
    driver->write_byte = sccb_dev_write_byte;
    return driver;
}

static void entry_exclusive(sccb_dev_data *dev_data)
{
    sccb_data *data = (sccb_data *)dev_data->sccb_data;
    configASSERT(xSemaphoreTake(data->free_mutex, portMAX_DELAY) == pdTRUE);
}

static void exit_exclusive(sccb_dev_data *dev_data)
{
    sccb_data *data = (sccb_data *)dev_data->sccb_data;
    xSemaphoreGive(data->free_mutex);
}

static void sccb_dev_install(void *userdata)
{
}

static int sccb_dev_open(void *userdata)
{
    return 1;
}

static void sccb_dev_close(void *userdata)
{
}

static void dvp_sccb_start_transfer(volatile dvp_t *dvp)
{
    while (dvp->sts & DVP_STS_SCCB_EN)
        ;
    dvp->sts = DVP_STS_SCCB_EN | DVP_STS_SCCB_EN_WE;
    while (dvp->sts & DVP_STS_SCCB_EN)
        ;
}

static uint8_t sccb_dev_read_byte(uint16_t reg_address, void *userdata)
{
    COMMON_DEV_ENTRY;
    entry_exclusive(dev_data);

    if (dev_data->reg_address_width == 8)
    {
        set_bit_mask(&sccb->sccb_cfg, DVP_SCCB_BYTE_NUM_MASK, DVP_SCCB_BYTE_NUM_2);
        sccb->sccb_ctl = DVP_SCCB_WRITE_DATA_ENABLE | DVP_SCCB_DEVICE_ADDRESS(dev_data->slave_address) | DVP_SCCB_REG_ADDRESS(reg_address);
    }
    else
    {
        set_bit_mask(&sccb->sccb_cfg, DVP_SCCB_BYTE_NUM_MASK, DVP_SCCB_BYTE_NUM_3);
        sccb->sccb_ctl = DVP_SCCB_WRITE_DATA_ENABLE | DVP_SCCB_DEVICE_ADDRESS(dev_data->slave_address) | DVP_SCCB_REG_ADDRESS(reg_address >> 8) | DVP_SCCB_WDATA_BYTE0(reg_address & 0xFF);
    }

    dvp_sccb_start_transfer(sccb);
    sccb->sccb_ctl = DVP_SCCB_DEVICE_ADDRESS(dev_data->slave_address);
    dvp_sccb_start_transfer(sccb);

    uint8_t ret = DVP_SCCB_RDATA_BYTE(sccb->sccb_cfg);

    exit_exclusive(dev_data);
    return ret;
}

static void sccb_dev_write_byte(uint16_t reg_address, uint8_t value, void *userdata)
{
    COMMON_DEV_ENTRY;
    entry_exclusive(dev_data);

    if (dev_data->reg_address_width == 8)
    {
        set_bit_mask(&sccb->sccb_cfg, DVP_SCCB_BYTE_NUM_MASK, DVP_SCCB_BYTE_NUM_3);
        sccb->sccb_ctl = DVP_SCCB_WRITE_DATA_ENABLE | DVP_SCCB_DEVICE_ADDRESS(dev_data->slave_address) | DVP_SCCB_REG_ADDRESS(reg_address) | DVP_SCCB_WDATA_BYTE0(value);
    }
    else
    {
        set_bit_mask(&sccb->sccb_cfg, DVP_SCCB_BYTE_NUM_MASK, DVP_SCCB_BYTE_NUM_4);
        sccb->sccb_ctl = DVP_SCCB_WRITE_DATA_ENABLE | DVP_SCCB_DEVICE_ADDRESS(dev_data->slave_address) | DVP_SCCB_REG_ADDRESS(reg_address >> 8) | DVP_SCCB_WDATA_BYTE0(reg_address & 0xFF) | DVP_SCCB_WDATA_BYTE1(value);
    }

    dvp_sccb_start_transfer(sccb);

    exit_exclusive(dev_data);
}

static sccb_data dev0_data = {SYSCTL_CLOCK_DVP, DVP_BASE_ADDR, NULL};

const sccb_driver_t g_sccb_driver_sccb0 = {{&dev0_data, sccb_install, sccb_open, sccb_close}, sccb_get_device};
