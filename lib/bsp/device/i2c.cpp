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
#include <hal.h>
#include <i2c.h>
#include <kernel/driver_impl.hpp>
#include <plic.h>
#include <semphr.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysctl.h>
#include <utility.h>

using namespace sys;

/* I2C Controller */

#define COMMON_ENTRY \
    semaphore_lock locker(free_mutex_);

class k_i2c_device_driver;

class k_i2c_driver : public i2c_driver, public static_object, public free_object_access
{
public:
    k_i2c_driver(uintptr_t base_addr, sysctl_clock_t clock, sysctl_threshold_t threshold, sysctl_dma_select_t dma_req)
        : i2c_(*reinterpret_cast<volatile i2c_t *>(base_addr)), clock_(clock), threshold_(threshold), dma_req_(dma_req)
    {
    }

    virtual void install() override
    {
        free_mutex_ = xSemaphoreCreateMutex();
        sysctl_clock_disable(clock_);
        sysctl_clock_set_threshold(threshold_, 3);
    }

    virtual void on_first_open() override
    {
        sysctl_clock_enable(clock_);
    }

    virtual void on_last_close() override
    {
        sysctl_clock_disable(clock_);
    }

    virtual object_ptr<i2c_device_driver> get_device(uint32_t slave_address, uint32_t address_width) override;

    virtual void config_as_slave(uint32_t slave_address, uint32_t address_width, const i2c_slave_handler_t &handler) override
    {
        configASSERT(address_width == 7 || address_width == 10);
        slave_handler_ = handler;

        i2c_.enable = 0;
        i2c_.con = (address_width == 10 ? I2C_CON_10BITADDR_SLAVE : 0) | I2C_CON_SPEED(1) | I2C_CON_STOP_DET_IFADDRESSED;
        i2c_.ss_scl_hcnt = I2C_SS_SCL_HCNT_COUNT(37);
        i2c_.ss_scl_lcnt = I2C_SS_SCL_LCNT_COUNT(40);
        i2c_.sar = I2C_SAR_ADDRESS(slave_address);
        i2c_.rx_tl = I2C_RX_TL_VALUE(0);
        i2c_.tx_tl = I2C_TX_TL_VALUE(0);
        i2c_.intr_mask = I2C_INTR_MASK_RX_FULL | I2C_INTR_MASK_START_DET | I2C_INTR_MASK_STOP_DET | I2C_INTR_MASK_RD_REQ;

        int i2c_idx = clock_ - SYSCTL_CLOCK_I2C0;
        pic_set_irq_priority(IRQN_I2C0_INTERRUPT + i2c_idx, 1);
        pic_set_irq_handler(IRQN_I2C0_INTERRUPT + i2c_idx, on_i2c_irq, this);
        pic_set_irq_enable(IRQN_I2C0_INTERRUPT + i2c_idx, 1);

        i2c_.enable = I2C_ENABLE_ENABLE;
    }

    virtual double slave_set_clock_rate(double clock_rate) override
    {
        uint32_t hcnt;
        uint32_t lcnt;
        clock_rate = i2c_get_hlcnt(clock_rate, hcnt, lcnt);
        i2c_.ss_scl_hcnt = I2C_SS_SCL_HCNT_COUNT(hcnt);
        i2c_.ss_scl_lcnt = I2C_SS_SCL_LCNT_COUNT(lcnt);
        return clock_rate;
    }

    double set_clock_rate(k_i2c_device_driver &device, double clock_rate);

    int read(k_i2c_device_driver &device, gsl::span<uint8_t> buffer)
    {
        COMMON_ENTRY;
        setup_device(device);

        uint8_t fifo_len, index;
        size_t len = buffer.size();
        size_t rx_len = len;
        size_t read = 0;
        auto it = buffer.begin();

        fifo_len = rx_len < 7 ? rx_len : 7;
        for (index = 0; index < fifo_len; index++)
            i2c_.data_cmd = I2C_DATA_CMD_CMD;
        len -= fifo_len;
        while (len || rx_len)
        {
            fifo_len = i2c_.rxflr;
            fifo_len = rx_len < fifo_len ? rx_len : fifo_len;
            for (index = 0; index < fifo_len; index++)
                *it++ = i2c_.data_cmd;
            rx_len -= fifo_len;
            read += fifo_len;
            fifo_len = 8 - i2c_.txflr;
            fifo_len = len < fifo_len ? len : fifo_len;
            for (index = 0; index < fifo_len; index++)
                i2c_.data_cmd = I2C_DATA_CMD_CMD;
            if (i2c_.tx_abrt_source != 0)
                return read;
            len -= fifo_len;
        }

        return read;
    }

    int write(k_i2c_device_driver &device, gsl::span<const uint8_t> buffer)
    {
        COMMON_ENTRY;
        setup_device(device);

        uintptr_t dma_write = dma_open_free();

        dma_set_request_source(dma_write, dma_req_ + 1);
        dma_transmit(dma_write, buffer.data(), &i2c_.data_cmd, 1, 0, 1, buffer.size(), 4);
        dma_close(dma_write);

        while (i2c_.status & I2C_STATUS_ACTIVITY)
        {
            if (i2c_.tx_abrt_source != 0)
                configASSERT(!"source abort");
        }
        return buffer.size();
    }

    int transfer_sequential(k_i2c_device_driver &device, gsl::span<const uint8_t> write_buffer, gsl::span<uint8_t> read_buffer)
    {
        COMMON_ENTRY;
        setup_device(device);

        auto write_cmd = std::make_unique<uint32_t[]>(write_buffer.size() + read_buffer.size());
        size_t i;
        for (i = 0; i < write_buffer.size(); i++)
            write_cmd[i] = write_buffer[i];
        for (i = 0; i < read_buffer.size(); i++)
            write_cmd[i + write_buffer.size()] = I2C_DATA_CMD_CMD;

        uintptr_t dma_write = dma_open_free();
        uintptr_t dma_read = dma_open_free();
        SemaphoreHandle_t event_read = xSemaphoreCreateBinary(), event_write = xSemaphoreCreateBinary();

        dma_set_request_source(dma_write, dma_req_ + 1);
        dma_set_request_source(dma_read, dma_req_);

        dma_transmit_async(dma_read, &i2c_.data_cmd, read_buffer.data(), 0, 1, 1, read_buffer.size(), 1, event_read);
        dma_transmit_async(dma_write, write_cmd.get(), &i2c_.data_cmd, 1, 0, sizeof(uint32_t), write_buffer.size() + read_buffer.size(), 4, event_write);

        configASSERT(xSemaphoreTake(event_read, portMAX_DELAY) == pdTRUE && xSemaphoreTake(event_write, portMAX_DELAY) == pdTRUE);

        dma_close(dma_write);
        dma_close(dma_read);
        vSemaphoreDelete(event_read);
        vSemaphoreDelete(event_write);
        return read_buffer.size();
    }

private:
    void setup_device(k_i2c_device_driver &device);

    double i2c_get_hlcnt(double clock_rate, uint32_t &hcnt, uint32_t &lcnt)
    {
        uint32_t v_i2c_freq = sysctl_clock_get_freq(clock_);
        uint16_t v_period_clk_cnt = v_i2c_freq / clock_rate / 2;

        if (v_period_clk_cnt == 0)
            v_period_clk_cnt = 1;

        hcnt = v_period_clk_cnt;
        lcnt = v_period_clk_cnt;
        return v_i2c_freq / v_period_clk_cnt * 2;
    }

    static void on_i2c_irq(void *userdata)
    {
        auto &driver = *reinterpret_cast<k_i2c_driver *>(userdata);
        auto &i2c_ = driver.i2c_;

        uint32_t status = i2c_.intr_stat;

        if (status & I2C_INTR_STAT_START_DET)
        {
            driver.slave_handler_.on_event(I2C_EV_START);
            readl(&i2c_.clr_start_det);
        }
        if (status & I2C_INTR_STAT_STOP_DET)
        {
            driver.slave_handler_.on_event(I2C_EV_STOP);
            readl(&i2c_.clr_stop_det);
        }
        if (status & I2C_INTR_STAT_RX_FULL)
        {
            driver.slave_handler_.on_receive(i2c_.data_cmd);
        }
        if (status & I2C_INTR_STAT_RD_REQ)
        {
            i2c_.data_cmd = driver.slave_handler_.on_transmit();
            readl(&i2c_.clr_rd_req);
        }
    }

private:
    volatile i2c_t &i2c_;
    sysctl_clock_t clock_;
    sysctl_threshold_t threshold_;
    sysctl_dma_select_t dma_req_;

    SemaphoreHandle_t free_mutex_;
    i2c_slave_handler_t slave_handler_;
};

/* I2C Device */

class k_i2c_device_driver : public i2c_device_driver, public heap_object, public exclusive_object_access
{
public:
    k_i2c_device_driver(object_accessor<k_i2c_driver> i2c, uint32_t slave_address, uint32_t address_width)
        : i2c_(std::move(i2c)), slave_address_(slave_address), address_width_(address_width)
    {
        configASSERT(address_width == 7 || address_width == 10);
    }

    virtual void install() override
    {
    }

    virtual double set_clock_rate(double clock_rate) override
    {
        return i2c_->set_clock_rate(*this, clock_rate);
    }

    virtual int read(gsl::span<uint8_t> buffer) override
    {
        return i2c_->read(*this, buffer);
    }

    virtual int write(gsl::span<const uint8_t> buffer) override
    {
        return i2c_->write(*this, buffer);
    }

    virtual int transfer_sequential(gsl::span<const uint8_t> write_buffer, gsl::span<uint8_t> read_buffer) override
    {
        return i2c_->transfer_sequential(*this, write_buffer, read_buffer);
    }

private:
    friend class k_i2c_driver;

    object_accessor<k_i2c_driver> i2c_;
    uint32_t slave_address_;
    uint32_t address_width_;

    uint32_t hcnt_ = 37;
    uint32_t lcnt_ = 40;
};

object_ptr<i2c_device_driver> k_i2c_driver::get_device(uint32_t slave_address, uint32_t address_width)
{
    auto driver = make_object<k_i2c_device_driver>(make_accessor<k_i2c_driver>(this), slave_address, address_width);
    driver->install();
    return driver;
}

void k_i2c_driver::setup_device(k_i2c_device_driver &device)
{
    i2c_.enable = 0;
    i2c_.con = I2C_CON_MASTER_MODE | I2C_CON_SLAVE_DISABLE | I2C_CON_RESTART_EN | (device.address_width_ == 10 ? I2C_CON_10BITADDR_SLAVE : 0) | I2C_CON_SPEED(1);
    i2c_.ss_scl_hcnt = I2C_SS_SCL_HCNT_COUNT(device.hcnt_);
    i2c_.ss_scl_lcnt = I2C_SS_SCL_LCNT_COUNT(device.lcnt_);
    i2c_.tar = I2C_TAR_ADDRESS(device.slave_address_);
    i2c_.intr_mask = 0;

    i2c_.dma_cr = 0x3;
    i2c_.dma_rdlr = 0;
    i2c_.dma_tdlr = 4;

    i2c_.enable = I2C_ENABLE_ENABLE;
}

double k_i2c_driver::set_clock_rate(k_i2c_device_driver &device, double clock_rate)
{
    return i2c_get_hlcnt(clock_rate, device.hcnt_, device.lcnt_);
}

static k_i2c_driver dev0_driver(I2C0_BASE_ADDR, SYSCTL_CLOCK_I2C0, SYSCTL_THRESHOLD_I2C0, SYSCTL_DMA_SELECT_I2C0_RX_REQ);
static k_i2c_driver dev1_driver(I2C1_BASE_ADDR, SYSCTL_CLOCK_I2C1, SYSCTL_THRESHOLD_I2C1, SYSCTL_DMA_SELECT_I2C1_RX_REQ);
static k_i2c_driver dev2_driver(I2C2_BASE_ADDR, SYSCTL_CLOCK_I2C2, SYSCTL_THRESHOLD_I2C2, SYSCTL_DMA_SELECT_I2C2_RX_REQ);

driver &g_i2c_driver_i2c0 = dev0_driver;
driver &g_i2c_driver_i2c1 = dev1_driver;
driver &g_i2c_driver_i2c2 = dev2_driver;
