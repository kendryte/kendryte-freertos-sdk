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
#include <dvp.h>
#include <fpioa.h>
#include <hal.h>
#include <kernel/driver_impl.hpp>
#include <semphr.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysctl.h>
#include <utility.h>

using namespace sys;

/* SCCB Controller */

#define COMMON_ENTRY \
    semaphore_lock locker(free_mutex_);

class k_sccb_device_driver;

class k_sccb_driver : public sccb_driver, public static_object, public free_object_access
{
public:
    k_sccb_driver(uintptr_t base_addr, sysctl_clock_t clock)
        : sccb_(*reinterpret_cast<volatile dvp_t *>(base_addr)), clock_(clock)
    {
    }

    virtual void install() override
    {
        free_mutex_ = xSemaphoreCreateMutex();
        sysctl_clock_disable(clock_);
    }

    virtual void on_first_open() override
    {
        sysctl_clock_enable(clock_);
        set_bit_mask(&sccb_.sccb_cfg, DVP_SCCB_SCL_LCNT_MASK | DVP_SCCB_SCL_HCNT_MASK, DVP_SCCB_SCL_LCNT(500) | DVP_SCCB_SCL_HCNT(500));
    }

    virtual void on_last_close() override
    {
        sysctl_clock_disable(clock_);
    }

    virtual object_ptr<sccb_device_driver> get_device(uint32_t slave_address, uint32_t reg_address_width) override;

    uint8_t read_byte(k_sccb_device_driver &device, uint16_t reg_address);
    void write_byte(k_sccb_device_driver &device, uint16_t reg_address, uint8_t value);

private:
    void setup_device(k_sccb_device_driver &device);

    void dvp_sccb_start_transfer()
    {
        while (sccb_.sts & DVP_STS_SCCB_EN)
            ;
        sccb_.sts = DVP_STS_SCCB_EN | DVP_STS_SCCB_EN_WE;
        while (sccb_.sts & DVP_STS_SCCB_EN)
            ;
    }

private:
    volatile dvp_t &sccb_;
    sysctl_clock_t clock_;

    SemaphoreHandle_t free_mutex_;
};

/* SCCB Device */

class k_sccb_device_driver : public sccb_device_driver, public heap_object, public exclusive_object_access
{
public:
    k_sccb_device_driver(object_accessor<k_sccb_driver> sccb, uint32_t slave_address, uint32_t reg_address_width)
        : sccb_(std::move(sccb)), slave_address_(slave_address), reg_address_width_(reg_address_width)
    {
        configASSERT(reg_address_width == 8 || reg_address_width == 16);
    }

    virtual void install() override
    {
    }

    virtual uint8_t read_byte(uint16_t reg_address) override
    {
        return sccb_->read_byte(*this, reg_address);
    }

    virtual void write_byte(uint16_t reg_address, uint8_t value) override
    {
        sccb_->write_byte(*this, reg_address, value);
    }

private:
    friend class k_sccb_driver;

    object_accessor<k_sccb_driver> sccb_;
    uint32_t slave_address_;
    uint32_t reg_address_width_;
};

void k_sccb_driver::setup_device(k_sccb_device_driver &device)
{
}

object_ptr<sccb_device_driver> k_sccb_driver::get_device(uint32_t slave_address, uint32_t reg_address_width)
{
    auto driver = make_object<k_sccb_device_driver>(make_accessor<k_sccb_driver>(this), slave_address, reg_address_width);
    driver->install();
    return driver;
}

uint8_t k_sccb_driver::read_byte(k_sccb_device_driver &device, uint16_t reg_address)
{
    COMMON_ENTRY;
    setup_device(device);

    if (device.reg_address_width_ == 8)
    {
        set_bit_mask(&sccb_.sccb_cfg, DVP_SCCB_BYTE_NUM_MASK, DVP_SCCB_BYTE_NUM_2);
        sccb_.sccb_ctl = DVP_SCCB_WRITE_DATA_ENABLE | DVP_SCCB_DEVICE_ADDRESS(device.slave_address_) | DVP_SCCB_REG_ADDRESS(reg_address);
    }
    else
    {
        set_bit_mask(&sccb_.sccb_cfg, DVP_SCCB_BYTE_NUM_MASK, DVP_SCCB_BYTE_NUM_3);
        sccb_.sccb_ctl = DVP_SCCB_WRITE_DATA_ENABLE | DVP_SCCB_DEVICE_ADDRESS(device.slave_address_) | DVP_SCCB_REG_ADDRESS(reg_address >> 8) | DVP_SCCB_WDATA_BYTE0(reg_address & 0xFF);
    }

    dvp_sccb_start_transfer();
    sccb_.sccb_ctl = DVP_SCCB_DEVICE_ADDRESS(device.slave_address_);
    dvp_sccb_start_transfer();

    uint8_t ret = DVP_SCCB_RDATA_BYTE(sccb_.sccb_cfg);
    return ret;
}

void k_sccb_driver::write_byte(k_sccb_device_driver &device, uint16_t reg_address, uint8_t value)
{
    COMMON_ENTRY;
    setup_device(device);

    if (device.reg_address_width_ == 8)
    {
        set_bit_mask(&sccb_.sccb_cfg, DVP_SCCB_BYTE_NUM_MASK, DVP_SCCB_BYTE_NUM_3);
        sccb_.sccb_ctl = DVP_SCCB_WRITE_DATA_ENABLE | DVP_SCCB_DEVICE_ADDRESS(device.slave_address_) | DVP_SCCB_REG_ADDRESS(reg_address) | DVP_SCCB_WDATA_BYTE0(value);
    }
    else
    {
        set_bit_mask(&sccb_.sccb_cfg, DVP_SCCB_BYTE_NUM_MASK, DVP_SCCB_BYTE_NUM_4);
        sccb_.sccb_ctl = DVP_SCCB_WRITE_DATA_ENABLE | DVP_SCCB_DEVICE_ADDRESS(device.slave_address_) | DVP_SCCB_REG_ADDRESS(reg_address >> 8) | DVP_SCCB_WDATA_BYTE0(reg_address & 0xFF) | DVP_SCCB_WDATA_BYTE1(value);
    }

    dvp_sccb_start_transfer();
}

static k_sccb_driver dev0_driver(DVP_BASE_ADDR, SYSCTL_CLOCK_DVP);

driver &g_sccb_driver_sccb0 = dev0_driver;
