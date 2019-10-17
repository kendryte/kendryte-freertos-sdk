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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <devices.h>
#include "w25qxx.h"
#include "printf.h"
#include "iomem.h"

uintptr_t spi_adapter;
uintptr_t spi_stand;
static SemaphoreHandle_t event;


static enum w25qxx_status_t w25qxx_receive_data(uint8_t* cmd_buff, uint8_t cmd_len, uint8_t* rx_buff, uint32_t rx_len)
{
    //xSemaphoreTake(event, portMAX_DELAY);

    spi_dev_transfer_sequential(spi_stand, (uint8_t *)cmd_buff, cmd_len, (uint8_t *)rx_buff, rx_len);
    //xSemaphoreGive(event);

    return W25QXX_OK;
}

static enum w25qxx_status_t w25qxx_receive_data_enhanced(uint32_t* cmd_buff, uint8_t cmd_len, uint8_t* rx_buff, uint32_t rx_len)
{
    //xSemaphoreTake(event, portMAX_DELAY);
    memcpy(rx_buff, cmd_buff, cmd_len);
    io_read(spi_adapter, (uint8_t *)rx_buff, rx_len);
    //xSemaphoreGive(event);

    return W25QXX_OK;
}

static enum w25qxx_status_t w25qxx_send_data(uintptr_t file, uint8_t* cmd_buff, uint8_t cmd_len, uint8_t* tx_buff, uint32_t tx_len)
{
    configASSERT(cmd_len);
    //xSemaphoreTake(event, portMAX_DELAY);
    uint8_t* tmp_buf = iomem_malloc(cmd_len + tx_len);
    memcpy(tmp_buf, cmd_buff, cmd_len);
    if (tx_len)
        memcpy(tmp_buf + cmd_len, tx_buff, tx_len);
    io_write(file, (uint8_t *)tmp_buf, cmd_len + tx_len);
    iomem_free(tmp_buf);
    //xSemaphoreGive(event);
    return W25QXX_OK;
}

static enum w25qxx_status_t w25qxx_write_enable(void)
{
    uint8_t cmd[1] = {WRITE_ENABLE};

    w25qxx_send_data(spi_stand, cmd, 1, 0, 0);
    return W25QXX_OK;
}

static enum w25qxx_status_t w25qxx_read_status_reg1(uint8_t* reg_data)
{
    uint8_t cmd[1] = {READ_REG1};
    uint8_t data[1];

    w25qxx_receive_data(cmd, 1, data, 1);
    *reg_data = data[0];
    return W25QXX_OK;
}
static enum w25qxx_status_t w25qxx_read_status_reg2(uint8_t* reg_data)
{
    uint8_t cmd[1] = {READ_REG2};
    uint8_t data[1];

    w25qxx_receive_data(cmd, 1, data, 1);
    *reg_data = data[0];
    return W25QXX_OK;
}
static enum w25qxx_status_t w25qxx_write_status_reg(uint8_t reg1_data, uint8_t reg2_data)
{
    uint8_t cmd[3] = {WRITE_REG1, reg1_data, reg2_data};

    w25qxx_write_enable();
    w25qxx_send_data(spi_stand, cmd, 3, 0, 0);
    return W25QXX_OK;
}

static enum w25qxx_status_t w25qxx_enable_quad_mode(void)
{
    uint8_t reg_data;

    w25qxx_read_status_reg2(&reg_data);
    if (!(reg_data & REG2_QUAL_MASK))
    {
        reg_data |= REG2_QUAL_MASK;
        w25qxx_write_status_reg(0x00, reg_data);
    }
    return W25QXX_OK;
}

static enum w25qxx_status_t w25qxx_is_busy(void)
{
    uint8_t status;

    w25qxx_read_status_reg1(&status);
    if (status & REG1_BUSY_MASK)
        return W25QXX_BUSY;
    return W25QXX_OK;
}

enum w25qxx_status_t w25qxx_sector_erase(uint32_t addr)
{
    uint8_t cmd[4] = {SECTOR_ERASE};

    cmd[1] = (uint8_t)(addr >> 16);
    cmd[2] = (uint8_t)(addr >> 8);
    cmd[3] = (uint8_t)(addr);
    w25qxx_write_enable();
    w25qxx_send_data(spi_stand, cmd, 4, 0, 0);
    return W25QXX_OK;
}

enum w25qxx_status_t w25qxx_read_id(uint8_t *manuf_id, uint8_t *device_id)
{
    uint8_t cmd[4] = {READ_ID, 0x00, 0x00, 0x00};
    uint8_t data[2] = {0};

    w25qxx_receive_data(cmd, 4, data, 2);
    *manuf_id = data[0];
    *device_id = data[1];
    return W25QXX_OK;
}

static enum w25qxx_status_t w25qxx_read_data_less_64kb(uint32_t addr, uint8_t* data_buf, uint32_t length)
{
    uint32_t cmd[2];

    switch (WORK_TRANS_MODE)
    {
        case SPI_FF_DUAL:
            *(((uint8_t*)cmd) + 0) = FAST_READ_DUAL_OUTPUT;
            *(((uint8_t*)cmd) + 1) = (uint8_t)(addr >> 0);
            *(((uint8_t*)cmd) + 2) = (uint8_t)(addr >> 8);
            *(((uint8_t*)cmd) + 3) = (uint8_t)(addr >> 16);
            w25qxx_receive_data_enhanced(cmd, 4, data_buf, length);
            break;
        case SPI_FF_QUAD:
            *(((uint8_t*)cmd) + 0) = FAST_READ_QUAL_OUTPUT;
            *(((uint8_t*)cmd) + 1) = (uint8_t)(addr >> 0);
            *(((uint8_t*)cmd) + 2) = (uint8_t)(addr >> 8);
            *(((uint8_t*)cmd) + 3) = (uint8_t)(addr >> 16);
            w25qxx_receive_data_enhanced(cmd, 4, data_buf, length);
            break;
        case SPI_FF_STANDARD:
        default:
            *(((uint8_t*)cmd) + 0) = READ_DATA;
            *(((uint8_t*)cmd) + 1) = (uint8_t)(addr >> 16);
            *(((uint8_t*)cmd) + 2) = (uint8_t)(addr >> 8);
            *(((uint8_t*)cmd) + 3) = (uint8_t)(addr >> 0);
            w25qxx_receive_data((uint8_t*)cmd, 4, data_buf, length);
            break;
    }
    return W25QXX_OK;
}

enum w25qxx_status_t w25qxx_read_data(uint32_t addr, uint8_t* data_buf, uint32_t length)
{
    uint32_t len;

    while (length)
    {
        len = length >= 0x010000 ? 0x010000 : length;
        w25qxx_read_data_less_64kb(addr, data_buf, len);
        addr += len;
        data_buf += len;
        length -= len;
    }
    return W25QXX_OK;
}

static enum w25qxx_status_t w25qxx_page_program(uint32_t addr, uint8_t* data_buf, uint32_t length)
{
    uint32_t cmd[2];
    w25qxx_write_enable();
    if (WORK_TRANS_MODE == SPI_FF_QUAD)
    {
        *(((uint8_t*)cmd) + 0) = QUAD_PAGE_PROGRAM;
        *(((uint8_t*)cmd) + 1) = (uint8_t)(addr >> 0);
        *(((uint8_t*)cmd) + 2) = (uint8_t)(addr >> 8);
        *(((uint8_t*)cmd) + 3) = (uint8_t)(addr >> 16);
        w25qxx_send_data(spi_adapter, (uint8_t*)cmd, 4, data_buf, length);
    }
    else
    {
        *(((uint8_t*)cmd) + 0) = PAGE_PROGRAM;
        *(((uint8_t*)cmd) + 1) = (uint8_t)(addr >> 16);
        *(((uint8_t*)cmd) + 2) = (uint8_t)(addr >> 8);
        *(((uint8_t*)cmd) + 3) = (uint8_t)(addr >> 0);
        w25qxx_send_data(spi_stand, (uint8_t*)cmd, 4, data_buf, length);
    }
    while (w25qxx_is_busy() == W25QXX_BUSY)
        ;
    return W25QXX_OK;
}

static enum w25qxx_status_t w25qxx_sector_program(uint32_t addr, uint8_t* data_buf)
{
    uint8_t index;

    for (index = 0; index < w25qxx_FLASH_PAGE_NUM_PER_SECTOR; index++)
    {
        w25qxx_page_program(addr, data_buf, w25qxx_FLASH_PAGE_SIZE);
        addr += w25qxx_FLASH_PAGE_SIZE;
        data_buf += w25qxx_FLASH_PAGE_SIZE;
    }
    return W25QXX_OK;
}

enum w25qxx_status_t w25qxx_write_data(uint32_t addr, uint8_t* data_buf, uint32_t length)
{
    uint32_t sector_addr, sector_offset, sector_remain, write_len, index;
    uint8_t *swap_buf = (uint8_t *)iomem_malloc(w25qxx_FLASH_SECTOR_SIZE);
    uint8_t *pread, *pwrite;

    while (length)
    {
        sector_addr = addr & (~(w25qxx_FLASH_SECTOR_SIZE - 1));
        sector_offset = addr & (w25qxx_FLASH_SECTOR_SIZE - 1);
        sector_remain = w25qxx_FLASH_SECTOR_SIZE - sector_offset;
        write_len = length < sector_remain ? length : sector_remain;
        w25qxx_read_data(sector_addr, swap_buf, w25qxx_FLASH_SECTOR_SIZE);
        pread = swap_buf + sector_offset;
        pwrite = data_buf;
        for (index = 0; index < write_len; index++)
        {
            if ((*pwrite) != ((*pwrite) & (*pread)))
            {
                w25qxx_sector_erase(sector_addr);
                while (w25qxx_is_busy() == W25QXX_BUSY)
                    ;
                break;
            }
            pwrite++;
            pread++;
        }
        if (write_len == w25qxx_FLASH_SECTOR_SIZE)
            w25qxx_sector_program(sector_addr, data_buf);
        else
        {
            pread = swap_buf + sector_offset;
            pwrite = data_buf;
            for (index = 0; index < write_len; index++)
                *pread++ = *pwrite++;
            w25qxx_sector_program(sector_addr, swap_buf);
        }
        length -= write_len;
        addr += write_len;
        data_buf += write_len;
    }
    iomem_free(swap_buf);
    return W25QXX_OK;
}

enum w25qxx_status_t w25qxx_init(uintptr_t spi_in)
{
    uint8_t manuf_id, device_id;
    event = xSemaphoreCreateMutex();
    spi_stand = spi_get_device(spi_in, SPI_MODE_0, SPI_FF_STANDARD, CHIP_SELECT, FRAME_LENGTH);
    spi_dev_set_clock_rate(spi_stand, 25000000);
    w25qxx_read_id(&manuf_id, &device_id);
    if ((manuf_id != 0xEF && manuf_id != 0xC8) || (device_id != 0x17 && device_id != 0x16))
    {
        printf("manuf_id:0x%02x, device_id:0x%02x\n", manuf_id, device_id);
    }
    printf("manuf_id:0x%02x, device_id:0x%02x\n", manuf_id, device_id);
    switch (WORK_TRANS_MODE)
    {
        case SPI_FF_DUAL:
            spi_adapter = spi_get_device(spi_in, SPI_MODE_0, SPI_FF_DUAL, CHIP_SELECT, FRAME_LENGTH);
            spi_dev_config_non_standard(spi_adapter, INSTRUCTION_LENGTH, ADDRESS_LENGTH, WAIT_CYCLE, SPI_AITM_STANDARD);
            break;
        case SPI_FF_QUAD:
            spi_adapter = spi_get_device(spi_in, SPI_MODE_0, SPI_FF_QUAD, CHIP_SELECT, FRAME_LENGTH);
            spi_dev_config_non_standard(spi_adapter, INSTRUCTION_LENGTH, ADDRESS_LENGTH, WAIT_CYCLE, SPI_AITM_STANDARD);
            w25qxx_enable_quad_mode();
            break;
        case SPI_FF_STANDARD:
        default:
            spi_adapter = spi_stand;
            break;
    }
    return W25QXX_OK;
}

