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
#include <stdlib.h>
#include <string.h>
#include <kernel/driver.hpp>

#define COMMON_ENTRY                                                      \
    spi_sdcard_dev_data_t *data = (spi_sdcard_dev_data_t *)userdata;

typedef struct _spi_sdcard_dev_data
{
    handle_t spi_dev;

} spi_sdcard_dev_data_t;

static void spi_sdcard_install(void *userdata);
static int spi_sdcard_open(void *userdata);
static void spi_sdcard_close(void *userdata);
static uint32_t spi_sdcard_get_rw_block_size(void *userdata);
static uint32_t spi_sdcard_get_blocks_count(void *userdata);
static int spi_sdcard_read_blocks(uint32_t start_block, uint32_t blocks_count, uint8_t *buffer, void *userdata);
static int spi_sdcard_write_blocks(uint32_t start_block, uint32_t blocks_count, const uint8_t *buffer, void *userdata);

int spi_sdcard_driver_install(const char *name, const char *spi_name, const char *cs_gpio_name, uint32_t cs_gpio_pin)
{
    block_storage_driver_t *driver = NULL;
    spi_sdcard_dev_data_t *dev_data = NULL;

    driver = (block_storage_driver_t *)malloc(sizeof(block_storage_driver_t));
    if (!driver) goto on_error;
    memset(driver, 0, sizeof(block_storage_driver_t));

    dev_data = (spi_sdcard_dev_data_t *)malloc(sizeof(spi_sdcard_dev_data_t));
    if (!dev_data) goto on_error;
    memset(dev_data, 0, sizeof(spi_sdcard_dev_data_t));

    handle_t spi = io_open(spi_name);
    if (!spi) goto on_error;
    dev_data->spi_dev = spi_get_device(spi, NULL, SPI_MODE_0, SPI_FF_STANDARD, 1, 8);
    if (!dev_data->spi_dev) goto on_error;

    driver->base.userdata = dev_data;
    driver->base.install = spi_sdcard_install;
    driver->base.open = spi_sdcard_open;
    driver->base.close = spi_sdcard_close;
    driver->get_rw_block_size = spi_sdcard_get_rw_block_size;
    driver->get_blocks_count = spi_sdcard_get_blocks_count;
    driver->read_blocks = spi_sdcard_read_blocks;
    driver->write_blocks = spi_sdcard_write_blocks;
    
    if (system_install_driver(name, DRIVER_BLOCK_STORAGE, driver) != 0) goto on_error;
    return 0;
on_error:
    free(dev_data);
    free(driver);
    return -1;
}

static void spi_sdcard_install(void *userdata)
{
    COMMON_ENTRY;


}