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
#include <task.h>
#include <fpioa.h>
#include <hal.h>
#include <kernel/driver_impl.hpp>
#include <atomic.h>
#include <math.h>
#include <semphr.h>
#include <spi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysctl.h>
#include <utility.h>
#include <printf.h>

using namespace sys;

#define SPI_TRANSMISSION_THRESHOLD  0x800UL
#define SPI_DMA_BLOCK_TIME          1000UL

/* SPI Controller */

#define TMOD_MASK (3 << tmod_off_)
#define TMOD_VALUE(value) (value << tmod_off_)
#define COMMON_ENTRY \
    semaphore_lock locker(free_mutex_);

typedef struct _spi_slave_instance
{
    uint8_t int_pin;
    uint8_t ready_pin;

    object_ptr<gpio_driver> s_gpio_driver;
    object_accessor<gpio_driver> s_gpio;

    size_t data_bit_length;
    volatile spi_slave_status_e status;
    volatile spi_slave_command_t command;
    volatile uint8_t *config_ptr;
    uint32_t config_len;

    uintptr_t dma;
    SemaphoreHandle_t dma_event;
    SemaphoreHandle_t cs_event;
    SemaphoreHandle_t slave_event;
    spi_slave_receive_callback_t callback;
} spi_slave_instance_t;

class k_spi_device_driver;

class k_spi_driver : public spi_driver, public static_object, public free_object_access
{
public:
    k_spi_driver(uintptr_t base_addr, sysctl_clock_t clock, sysctl_dma_select_t dma_req, uint8_t mod_off, uint8_t dfs_off, uint8_t tmod_off, uint8_t frf_off)
        : spi_(*reinterpret_cast<volatile spi_t *>(base_addr)), clock_(clock), dma_req_(dma_req), mod_off_(mod_off), dfs_off_(dfs_off), tmod_off_(tmod_off), frf_off_(frf_off)
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
    }

    virtual void on_last_close() override
    {
        sysctl_clock_disable(clock_);
    }

    virtual object_ptr<spi_device_driver> get_device(spi_mode_t mode, spi_frame_format_t frame_format, uint32_t chip_select_mask, uint32_t data_bit_length) override;

    double set_clock_rate(k_spi_device_driver &device, double clock_rate);
    void set_endian(k_spi_device_driver &device, uint32_t endian);
    int read(k_spi_device_driver &device, gsl::span<uint8_t> buffer);
    int write(k_spi_device_driver &device, gsl::span<const uint8_t> buffer);
    int transfer_full_duplex(k_spi_device_driver &device, gsl::span<const uint8_t> write_buffer, gsl::span<uint8_t> read_buffer);
    int transfer_sequential(k_spi_device_driver &device, gsl::span<const uint8_t> write_buffer, gsl::span<uint8_t> read_buffer);
    int read_write(k_spi_device_driver &device, gsl::span<const uint8_t> write_buffer, gsl::span<uint8_t> read_buffer);
    void fill(k_spi_device_driver &device, uint32_t instruction, uint32_t address, uint32_t value, size_t count);
    void slave_config(handle_t gpio_handle, uint8_t int_pin, uint8_t ready_pin, size_t data_bit_length, uint8_t *data, uint32_t len, spi_slave_receive_callback_t callback)
    {
        slave_instance_.s_gpio_driver = system_handle_to_object(gpio_handle).get_object().as<gpio_driver>();
        slave_instance_.s_gpio = make_accessor(slave_instance_.s_gpio_driver);

        slave_instance_.status = IDLE;
        slave_instance_.config_ptr = data;
        slave_instance_.config_len = len;
        slave_instance_.data_bit_length = data_bit_length;
        slave_instance_.ready_pin = ready_pin;
        slave_instance_.int_pin = int_pin;
        slave_instance_.dma = dma_open_free();
        slave_instance_.dma_event = xSemaphoreCreateBinary();
        slave_instance_.cs_event = xSemaphoreCreateBinary();
        slave_instance_.slave_event = xSemaphoreCreateBinary();
        slave_instance_.callback = callback;
        uint8_t slv_oe = 10;
        sysctl_reset(SYSCTL_RESET_SPI2);
        sysctl_clock_enable(SYSCTL_CLOCK_SPI2);
        sysctl_clock_set_threshold(SYSCTL_THRESHOLD_SPI2, 9);

        uint32_t data_width = data_bit_length / 8;

        spi_.ssienr = 0x00;
        spi_.ctrlr0 = (0x0 << mod_off_) | (0x1 << slv_oe) | ((data_bit_length - 1) << dfs_off_);
        spi_.dmatdlr = 0x04;
        spi_.dmardlr = 0x03;
        spi_.dmacr = 0x00;
        spi_.txftlr = 0x00;
        spi_.rxftlr = 0x08 / data_width - 1;
        spi_.imr = 0x10;
        spi_.ssienr = 0x01;

        slave_instance_.s_gpio->set_drive_mode(ready_pin, GPIO_DM_OUTPUT);
        slave_instance_.s_gpio->set_pin_value(ready_pin, GPIO_PV_HIGH);

        slave_instance_.s_gpio->set_drive_mode(int_pin, GPIO_DM_INPUT_PULL_UP);
        slave_instance_.s_gpio->set_pin_edge(int_pin, GPIO_PE_RISING);
        slave_instance_.s_gpio->set_on_changed(int_pin, (gpio_on_changed_t)spi_slave_cs_irq, this);

        pic_set_irq_priority(IRQN_SPI_SLAVE_INTERRUPT, 4);
        pic_set_irq_enable(IRQN_SPI_SLAVE_INTERRUPT, 1);
        pic_set_irq_handler(IRQN_SPI_SLAVE_INTERRUPT, spi_slave_irq, this);
        TaskHandle_t h1, h2;
        auto ret = xTaskCreate(spi_slave_cs_thread, "spi_slave_cs", 4096*8, this, 2, &h1);
        configASSERT(ret == pdTRUE);
        ret = xTaskCreate(spi_slave_irq_thread, "spi_slave_irq", 4096*8, this, 3, &h2);
        configASSERT(ret == pdTRUE);
    }

private:
    void setup_device(k_spi_device_driver &device);

    static void spi_slave_irq_thread(void *userdata)
    {
        auto &driver = *reinterpret_cast<k_spi_driver *>(userdata);
        while (1)
        {
            if (xSemaphoreTake(driver.slave_instance_.slave_event, portMAX_DELAY) == pdTRUE)
            {
                atomic_cas(&driver.slave_instance_.status, IDLE, COMMAND);
            }
        }
    }

    static void spi_slave_cs_thread(void *userdata)
    {
        auto &driver = *reinterpret_cast<k_spi_driver *>(userdata);
        while (1)
        {
            if (xSemaphoreTake(driver.slave_instance_.cs_event, portMAX_DELAY) == pdTRUE)
            {
                spi_slave_status_e status = atomic_read(&driver.slave_instance_.status);
                if (status == IDLE)
                    spi_slave_idle_mode(userdata);
                else if (status == COMMAND)
                    spi_slave_command_mode(userdata);
                else if (status == TRANSFER)
                    spi_slave_transfer_mode(userdata);
            }
        }
    }

    static void spi_slave_cs_irq(uint32_t pin, void *userdata)
    {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        auto &driver = *reinterpret_cast<k_spi_driver *>(userdata);
        xSemaphoreGiveFromISR(driver.slave_instance_.cs_event, &xHigherPriorityTaskWoken);

        if (xHigherPriorityTaskWoken)
            portYIELD_FROM_ISR();
    }

    static void spi_slave_irq(void *ctx)
    {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        auto &driver = *reinterpret_cast<k_spi_driver *>(ctx);
        auto &spi_handle = driver.spi();
        spi_handle.imr = 0x00;
        *reinterpret_cast<volatile uint32_t *>(spi_handle.icr);
        xSemaphoreGiveFromISR(driver.slave_instance_.slave_event, &xHigherPriorityTaskWoken);

        if (xHigherPriorityTaskWoken)
            portYIELD_FROM_ISR();
    }

    static void spi_slave_idle_mode(void *userdata)
    {
        uint8_t slv_oe = 10;
        auto &driver = *reinterpret_cast<k_spi_driver *>(userdata);
        auto &spi_handle = driver.spi();
        uint32_t data_width = driver.slave_instance_.data_bit_length / 8;
        driver.slave_instance_.status = IDLE;
        spi_handle.ssienr = 0x00;
        spi_handle.ctrlr0 = (0x0 << driver.mod_off_) | (0x1 << slv_oe) | ((driver.slave_instance_.data_bit_length - 1) << driver.dfs_off_);
        spi_handle.rxftlr = 0x08 / data_width - 1;

        spi_handle.dmacr = 0x00;
        spi_handle.imr = 0x10;
        spi_handle.ssienr = 0x01;

        driver.slave_instance_.s_gpio->set_pin_value(driver.slave_instance_.ready_pin, GPIO_PV_HIGH);
    }

    static void spi_slave_command_mode(void *userdata)
    {
        auto &driver = *reinterpret_cast<k_spi_driver *>(userdata);
        auto &spi_handle = driver.spi();
        uint8_t cmd_data[8], sum = 0;
        uint8_t slv_oe = 10;

        uint32_t data_width = (driver.slave_instance_.data_bit_length + 7) / 8;
        vTaskEnterCritical();
        switch (data_width)
        {
        case 4:
            for (uint32_t i = 0; i < 8 / 4; i++)
                ((uint32_t *)cmd_data)[i] = spi_handle.dr[0];
            break;
        case 2:
            for (uint32_t i = 0; i < 8 / 2; i++)
                ((uint16_t *)cmd_data)[i] = spi_handle.dr[0];
            break;
        default:
            for (uint32_t i = 0; i < 8; i++)
                cmd_data[i] = spi_handle.dr[0];
            break;
        }
        vTaskExitCritical();
        for (uint32_t i = 0; i < 7; i++)
        {
            sum += cmd_data[i];
        }
        if (cmd_data[7] != sum)
        {
            spi_slave_idle_mode(userdata);
            return;
        }
        driver.slave_instance_.command.cmd = cmd_data[0];
        driver.slave_instance_.command.addr = cmd_data[1] | (cmd_data[2] << 8) | (cmd_data[3] << 16) | (cmd_data[4] << 24);
        driver.slave_instance_.command.len = cmd_data[5] | (cmd_data[6] << 8);
        if (driver.slave_instance_.command.len == 0)
            driver.slave_instance_.command.len = 65536;
        if ((driver.slave_instance_.command.cmd < WRITE_DATA_BLOCK) && (driver.slave_instance_.command.len > 8))
        {
            spi_slave_idle_mode(userdata);
            return;
        }
        driver.slave_instance_.status = TRANSFER;
        spi_handle.ssienr = 0x00;
        if (driver.slave_instance_.command.cmd == WRITE_CONFIG)
        {
            spi_handle.ctrlr0 = (0x0 << driver.mod_off_) | (0x1 << slv_oe) | ((driver.slave_instance_.data_bit_length - 1) << driver.dfs_off_);
            spi_handle.rxftlr = driver.slave_instance_.command.len / data_width - 1;
            spi_handle.imr = 0x00;
            spi_handle.ssienr = 0x01;
        }
        else if (driver.slave_instance_.command.cmd == READ_CONFIG)
        {
            spi_handle.ctrlr0 = (0x0 << driver.mod_off_) | (0x0 << slv_oe) | ((driver.slave_instance_.data_bit_length - 1) << driver.dfs_off_);
            set_bit_mask(&spi_handle.ctrlr0, 3 << driver.tmod_off_, 1 << driver.tmod_off_ );
            spi_handle.txftlr = 0x00;
            spi_handle.imr = 0x00;
            spi_handle.ssienr = 0x01;

            vTaskEnterCritical();
            switch (data_width)
            {
            case 4:
                for (uint32_t i = 0; i < driver.slave_instance_.command.len / 4; i++)
                {
                    spi_handle.dr[0] = ((uint32_t *)&driver.slave_instance_.config_ptr[driver.slave_instance_.command.addr])[i];
                }
                break;
            case 2:
                for (uint32_t i = 0; i < driver.slave_instance_.command.len / 2; i++)
                {
                    spi_handle.dr[0] = ((uint16_t *)&driver.slave_instance_.config_ptr[driver.slave_instance_.command.addr])[i];
                }
                break;
            default:
                for (uint32_t i = 0; i < driver.slave_instance_.command.len; i++)
                {
                    spi_handle.dr[0] = ((uint8_t *)&driver.slave_instance_.config_ptr[driver.slave_instance_.command.addr])[i];
                }
                break;
            }
            vTaskExitCritical();
        }
        else if (driver.slave_instance_.command.cmd == WRITE_DATA_BYTE)
        {
            spi_handle.ctrlr0 = (0x0 << driver.mod_off_) | (0x1 << slv_oe) | ((driver.slave_instance_.data_bit_length - 1) << driver.dfs_off_);
            spi_handle.rxftlr = driver.slave_instance_.command.len / data_width - 1;
            spi_handle.imr = 0x00;
            spi_handle.ssienr = 0x01;
        }
        else if (driver.slave_instance_.command.cmd == READ_DATA_BYTE)
        {
            spi_handle.ctrlr0 = (0x0 << driver.mod_off_) | (0x0 << slv_oe) | ((driver.slave_instance_.data_bit_length - 1) << driver.dfs_off_);
            set_bit_mask(&spi_handle.ctrlr0, 3 << driver.tmod_off_, 1 << driver.tmod_off_);
            spi_handle.txftlr = 0x00;
            spi_handle.imr = 0x00;
            spi_handle.ssienr = 0x01;

            vTaskEnterCritical();
            switch (data_width)
            {
            case 4:
                for (uint32_t i = 0; i < driver.slave_instance_.command.len / 4; i++)
                {
                    spi_handle.dr[0] = (reinterpret_cast<uint32_t *>(driver.slave_instance_.command.addr))[i];
                }
                break;
            case 2:
                for (uint32_t i = 0; i < driver.slave_instance_.command.len / 2; i++)
                {
                    spi_handle.dr[0] =(reinterpret_cast<uint16_t *>(driver.slave_instance_.command.addr))[i];
                }
                break;
            default:
                for (uint32_t i = 0; i < driver.slave_instance_.command.len; i++)
                {
                    spi_handle.dr[0] = (reinterpret_cast<uint8_t *>(driver.slave_instance_.command.addr))[i];
                }
                break;
            }
            vTaskExitCritical();
        } 
        
        else if (driver.slave_instance_.command.cmd == WRITE_DATA_BLOCK)
        {
            spi_handle.ctrlr0 = (0x0 << driver.mod_off_) | (0x1 << slv_oe) | ((32 - 1) << driver.dfs_off_);

            spi_handle.dmacr = 0x01;
            spi_handle.imr = 0x00;
            spi_handle.ssienr = 0x01;

            dma_set_request_source(driver.slave_instance_.dma, driver.dma_req_);
            spi_handle.dmacr = 0x1;
            dma_transmit_async(driver.slave_instance_.dma, &spi_handle.dr[0], reinterpret_cast<volatile void *>(driver.slave_instance_.command.addr & 0xFFFFFFF0), 0, 1, 4, driver.slave_instance_.command.len * 4, 4, driver.slave_instance_.dma_event);
        }
        else if (driver.slave_instance_.command.cmd == READ_DATA_BLOCK) 
        {
            spi_handle.ctrlr0 = (0x0 << driver.mod_off_) | (0x0 << slv_oe) | ((32 - 1) << driver.dfs_off_);
            set_bit_mask(&spi_handle.ctrlr0, 3 << driver.tmod_off_, 1 << driver.tmod_off_);
            spi_handle.dmacr = 0x02;
            spi_handle.imr = 0x00;
            spi_handle.ssienr = 0x01;

            dma_set_request_source(driver.slave_instance_.dma, driver.dma_req_ + 1);
            dma_transmit_async(driver.slave_instance_.dma, reinterpret_cast<volatile void *>(driver.slave_instance_.command.addr & 0xFFFFFFF0), &spi_handle.dr[0], 1, 0, 4, driver.slave_instance_.command.len * 4, 4, driver.slave_instance_.dma_event);
        }
        else
        {
            spi_slave_idle_mode(userdata);
            return;
        }
        driver.slave_instance_.s_gpio->set_pin_value(driver.slave_instance_.ready_pin, GPIO_PV_LOW);
    }

    static void spi_slave_transfer_mode(void *userdata)
    {
        
        auto &driver = *reinterpret_cast<k_spi_driver *>(userdata);
        auto &spi_handle = driver.spi();
        uint32_t command_len = 0;
        uint32_t data_width = (driver.slave_instance_.data_bit_length + 7) / 8;

        switch (data_width)
        {
        case 4:
            command_len = driver.slave_instance_.command.len / 4;
            break;
        case 2:
            command_len = driver.slave_instance_.command.len / 2;
            break;
        default:
            command_len = driver.slave_instance_.command.len;
            break;
        }
        driver.slave_instance_.command.err = 0;
        if (driver.slave_instance_.command.cmd == WRITE_CONFIG || driver.slave_instance_.command.cmd == WRITE_DATA_BYTE)
        {
            if (spi_handle.rxflr < command_len - 1)
                driver.slave_instance_.command.err = 1;
        }
        else if (driver.slave_instance_.command.cmd == READ_CONFIG || driver.slave_instance_.command.cmd == READ_DATA_BYTE)
        {
            if (spi_handle.txflr != 0)
                driver.slave_instance_.command.err = 2;
        } else if (driver.slave_instance_.command.cmd == WRITE_DATA_BLOCK || driver.slave_instance_.command.cmd == READ_DATA_BLOCK)
        {
            if(xSemaphoreTake(driver.slave_instance_.dma_event, portMAX_DELAY) == pdFALSE)
                driver.slave_instance_.command.err = 3;
        }
        else
        {
            spi_slave_idle_mode(userdata);
            return;
        }

        if (driver.slave_instance_.command.err == 0)
        {
            vTaskEnterCritical();
            if (driver.slave_instance_.command.cmd == WRITE_CONFIG)
            {
                switch (data_width)
                {
                case 4:
                    for (uint32_t i = 0; i < command_len; i++)
                    {
                        ((uint32_t *)&driver.slave_instance_.config_ptr[driver.slave_instance_.command.addr])[i] = spi_handle.dr[0];
                    }
                    break;
                case 2:
                    for (uint32_t i = 0; i < command_len; i++)
                    {
                        ((uint16_t *)&driver.slave_instance_.config_ptr[driver.slave_instance_.command.addr])[i] = spi_handle.dr[0];
                    }
                    break;
                default:
                    for (uint32_t i = 0; i < command_len; i++)
                    {
                        ((uint8_t *)&driver.slave_instance_.config_ptr[driver.slave_instance_.command.addr])[i] = spi_handle.dr[0];
                    }
                    break;
                }
            }
            else if (driver.slave_instance_.command.cmd == WRITE_DATA_BYTE)
            {
                switch(data_width)
                {
                case 4:
                    for (uint32_t i = 0; i < command_len; i++)
                    {
                        (reinterpret_cast<uint32_t *>(driver.slave_instance_.command.addr))[i] = spi_handle.dr[0];
                    }
                    break;
                case 2:
                    for (uint32_t i = 0; i < command_len; i++)
                    {
                        (reinterpret_cast<uint16_t *>(driver.slave_instance_.command.addr))[i] = spi_handle.dr[0];
                    }
                    break;
                default:
                    for (uint32_t i = 0; i < command_len; i++)
                    {
                        (reinterpret_cast<uint8_t *>(driver.slave_instance_.command.addr))[i] = spi_handle.dr[0];
                    }
                    break;
                }
            }
            vTaskExitCritical();
        }
        
        if(driver.slave_instance_.callback != NULL)
        {
            driver.slave_instance_.callback((void *)&driver.slave_instance_.command);
        }
        spi_slave_idle_mode(userdata);
    }

    volatile spi_t &spi()
    {
        return spi_;
    }

    static void write_inst_addr(volatile uint32_t *dr, const uint8_t **buffer, size_t width)
    {
        configASSERT(width <= 4);
        if (width)
        {
            uint32_t cmd = 0;
            uint8_t *pcmd = (uint8_t *)&cmd;
            size_t i;
            for (i = 0; i < width; i++)
            {
                pcmd[i] = **buffer;
                ++(*buffer);
            }

            *dr = cmd;
        }
    }

private:
    volatile spi_t &spi_;
    sysctl_clock_t clock_;
    sysctl_dma_select_t dma_req_;
    uint8_t mod_off_;
    uint8_t dfs_off_;
    uint8_t tmod_off_;
    uint8_t frf_off_;

    SemaphoreHandle_t free_mutex_;
    spi_slave_instance_t slave_instance_;
};

/* SPI Device */

class k_spi_device_driver : public spi_device_driver, public heap_object, public exclusive_object_access
{
public:
    k_spi_device_driver(object_accessor<k_spi_driver> spi, spi_mode_t mode, spi_frame_format_t frame_format, uint32_t chip_select_mask, uint32_t data_bit_length)
        : spi_(std::move(spi)), mode_(mode), frame_format_(frame_format), chip_select_mask_(chip_select_mask), data_bit_length_(data_bit_length)
    {
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

        buffer_width_ = get_buffer_width(data_bit_length);
    }

    virtual void install() override
    {
    }

    virtual void config_non_standard(uint32_t instruction_length, uint32_t address_length, uint32_t wait_cycles, spi_inst_addr_trans_mode_t trans_mode) override
    {
        instruction_length_ = instruction_length;
        address_length_ = address_length;
        inst_width_ = get_inst_addr_width(instruction_length);
        addr_width_ = get_inst_addr_width(address_length);
        wait_cycles_ = wait_cycles;
        trans_mode_ = trans_mode;
    }

    virtual double set_clock_rate(double clock_rate) override
    {
        return spi_->set_clock_rate(*this, clock_rate);
    }

    virtual void set_endian(uint32_t endian) override
    {
        spi_->set_endian(*this, endian);
    }
	
    virtual int read(gsl::span<uint8_t> buffer) override
    {
        return spi_->read(*this, buffer);
    }

    virtual int write(gsl::span<const uint8_t> buffer) override
    {
        return spi_->write(*this, buffer);
    }

    virtual int transfer_full_duplex(gsl::span<const uint8_t> write_buffer, gsl::span<uint8_t> read_buffer) override
    {
        return spi_->transfer_full_duplex(*this, write_buffer, read_buffer);
    }

    virtual int transfer_sequential(gsl::span<const uint8_t> write_buffer, gsl::span<uint8_t> read_buffer) override
    {
        return spi_->transfer_sequential(*this, write_buffer, read_buffer);
    }

    virtual void fill(uint32_t instruction, uint32_t address, uint32_t value, size_t count) override
    {
        spi_->fill(*this, instruction, address, value, count);
    }

private:
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

private:
    friend class k_spi_driver;

    object_accessor<k_spi_driver> spi_;
    spi_mode_t mode_;
    spi_frame_format_t frame_format_;
    uint32_t chip_select_mask_;
    uint32_t data_bit_length_;
    uint32_t instruction_length_ = 0;
    uint32_t address_length_ = 0;
    uint32_t inst_width_ = 0;
    uint32_t addr_width_ = 0;
    uint32_t wait_cycles_ = 0;
    spi_inst_addr_trans_mode_t trans_mode_;
    uint32_t baud_rate_ = 0x2;
    uint32_t buffer_width_ = 0;
    uint32_t endian_ = 0;
};

object_ptr<spi_device_driver> k_spi_driver::get_device(spi_mode_t mode, spi_frame_format_t frame_format, uint32_t chip_select_mask, uint32_t data_bit_length)
{
    auto driver = make_object<k_spi_device_driver>(make_accessor<k_spi_driver>(this), mode, frame_format, chip_select_mask, data_bit_length);
    driver->install();
    return driver;
}

double k_spi_driver::set_clock_rate(k_spi_device_driver &device, double clock_rate)
{
    double clk = (double)sysctl_clock_get_freq(clock_);
    uint32_t div = std::min(65534U, std::max((uint32_t)ceil(clk / clock_rate), 2U));
    if (div & 1)
        div++;
    device.baud_rate_ = div;
    return clk / div;
}

void k_spi_driver::set_endian(k_spi_device_driver &device, uint32_t endian)
{
    device.endian_ = endian;
}

int k_spi_driver::read(k_spi_device_driver &device, gsl::span<uint8_t> buffer)
{
    COMMON_ENTRY;

    setup_device(device);

    uint32_t i = 0;
    size_t rx_buffer_len = buffer.size();
    size_t rx_frames = rx_buffer_len / device.buffer_width_;
    auto buffer_read = buffer.data();
    set_bit_mask(&spi_.ctrlr0, TMOD_MASK, TMOD_VALUE(2));
    spi_.ctrlr1 = rx_frames - 1;
    spi_.ssienr = 0x01;
    if (device.frame_format_ == SPI_FF_STANDARD)
    {
        spi_.dr[0] = 0xFFFFFFFF;
    }

    if (rx_frames < SPI_TRANSMISSION_THRESHOLD)
    {
        vTaskEnterCritical();
        size_t index, fifo_len;
        while (rx_frames)
        {
            const uint8_t *buffer_it = buffer.data();
            write_inst_addr(spi_.dr, &buffer_it, device.inst_width_);
            write_inst_addr(spi_.dr, &buffer_it, device.addr_width_);
            spi_.ser = device.chip_select_mask_;

            fifo_len = spi_.rxflr;
            fifo_len = fifo_len < rx_frames ? fifo_len : rx_frames;
            switch (device.buffer_width_)
            {
            case 4:
                for (index = 0; index < fifo_len; index++)
                    ((uint32_t *)buffer_read)[i++] = spi_.dr[0];
                break;
            case 2:
                for (index = 0; index < fifo_len; index++)
                    ((uint16_t *)buffer_read)[i++] = (uint16_t)spi_.dr[0];
                break;
            default:
                for (index = 0; index < fifo_len; index++)
                    buffer_read[i++] = (uint8_t)spi_.dr[0];
                break;
            }
            rx_frames -= fifo_len;
        }
        vTaskExitCritical();
    }
    else
    {
        uintptr_t dma_read = dma_open_free();
        dma_set_request_source(dma_read, dma_req_);
        spi_.dmacr = 0x1;
        SemaphoreHandle_t event_read = xSemaphoreCreateBinary();

        dma_transmit_async(dma_read, &spi_.dr[0], buffer_read, 0, 1, device.buffer_width_, rx_frames, 1, event_read);
        const uint8_t *buffer_it = buffer.data();
        write_inst_addr(spi_.dr, &buffer_it, device.inst_width_);
        write_inst_addr(spi_.dr, &buffer_it, device.addr_width_);
        spi_.ser = device.chip_select_mask_;

        configASSERT(pdTRUE == xSemaphoreTake(event_read, SPI_DMA_BLOCK_TIME));

        dma_close(dma_read);
        vSemaphoreDelete(event_read);
    }

    spi_.ser = 0x00;
    spi_.ssienr = 0x00;
    spi_.dmacr = 0x00;

    return buffer.size();
}

int k_spi_driver::write(k_spi_device_driver &device, gsl::span<const uint8_t> buffer)
{
    COMMON_ENTRY;

    setup_device(device);

    uint32_t i = 0;
    size_t tx_buffer_len = buffer.size() - (device.inst_width_ + device.addr_width_);
    size_t tx_frames = tx_buffer_len / device.buffer_width_;
    auto buffer_write = buffer.data();
    set_bit_mask(&spi_.ctrlr0, TMOD_MASK, TMOD_VALUE(1));

    if (tx_frames < SPI_TRANSMISSION_THRESHOLD)
    {
        vTaskEnterCritical();
        size_t index, fifo_len;
        spi_.ssienr = 0x01;
        write_inst_addr(spi_.dr, &buffer_write, device.inst_width_);
        write_inst_addr(spi_.dr, &buffer_write, device.addr_width_);
        spi_.ser = device.chip_select_mask_;
        while (tx_buffer_len)
        {
            fifo_len = 32 - spi_.txflr;
            fifo_len = fifo_len < tx_buffer_len ? fifo_len : tx_buffer_len;
            switch (device.buffer_width_)
            {
            case 4:
                fifo_len = fifo_len / 4 * 4;
                for (index = 0; index < fifo_len / 4; index++)
                    spi_.dr[0] = ((uint32_t *)buffer_write)[i++];
                break;
            case 2:
                fifo_len = fifo_len / 2 * 2;
                for (index = 0; index < fifo_len / 2; index++)
                    spi_.dr[0] = ((uint16_t *)buffer_write)[i++];
                break;
            default:
                for (index = 0; index < fifo_len; index++)
                    spi_.dr[0] = buffer_write[i++];
                break;
            }
            tx_buffer_len -= fifo_len;
        }
        vTaskExitCritical();
    }
    else
    {
        uintptr_t dma_write = dma_open_free();
        dma_set_request_source(dma_write, dma_req_ + 1);
        spi_.dmacr = 0x2;
        spi_.ssienr = 0x01;
        write_inst_addr(spi_.dr, &buffer_write, device.inst_width_);
        write_inst_addr(spi_.dr, &buffer_write, device.addr_width_);
        SemaphoreHandle_t event_write = xSemaphoreCreateBinary();

        dma_transmit_async(dma_write, buffer_write, &spi_.dr[0], 1, 0, device.buffer_width_, tx_frames, 4, event_write);
        spi_.ser = device.chip_select_mask_;
        configASSERT(pdTRUE == xSemaphoreTake(event_write, SPI_DMA_BLOCK_TIME));

        dma_close(dma_write);
        vSemaphoreDelete(event_write);
    }
    while ((spi_.sr & 0x05) != 0x04)
        ;
    spi_.ser = 0x00;
    spi_.ssienr = 0x00;
    spi_.dmacr = 0x00;

    return buffer.size();
}

int k_spi_driver::transfer_full_duplex(k_spi_device_driver &device, gsl::span<const uint8_t> write_buffer, gsl::span<uint8_t> read_buffer)
{
    COMMON_ENTRY;
    setup_device(device);
    set_bit_mask(&spi_.ctrlr0, TMOD_MASK, TMOD_VALUE(0));
    return read_write(device, write_buffer, read_buffer);
}

int k_spi_driver::transfer_sequential(k_spi_device_driver &device, gsl::span<const uint8_t> write_buffer, gsl::span<uint8_t> read_buffer)
{
    COMMON_ENTRY;
    setup_device(device);
    set_bit_mask(&spi_.ctrlr0, TMOD_MASK, TMOD_VALUE(3));
    return read_write(device, write_buffer, read_buffer);
}

int k_spi_driver::read_write(k_spi_device_driver &device, gsl::span<const uint8_t> write_buffer, gsl::span<uint8_t> read_buffer)
{
    configASSERT(device.frame_format_ == SPI_FF_STANDARD);
    size_t tx_buffer_len = write_buffer.size();
    size_t rx_buffer_len = read_buffer.size();
    size_t tx_frames = tx_buffer_len / device.buffer_width_;
    size_t rx_frames = rx_buffer_len / device.buffer_width_;
    auto buffer_read = read_buffer.data();
    auto buffer_write = write_buffer.data();
    uint32_t i = 0;

    if (rx_frames < SPI_TRANSMISSION_THRESHOLD)
    {
        vTaskEnterCritical();
        size_t index, fifo_len;
        spi_.ctrlr1 = rx_frames - 1;
        spi_.ssienr = 0x01;
        while (tx_buffer_len)
        {
            fifo_len = 32 - spi_.txflr;
            fifo_len = fifo_len < tx_buffer_len ? fifo_len : tx_buffer_len;
            switch (device.buffer_width_)
            {
            case 4:
                fifo_len = fifo_len / 4 * 4;
                for (index = 0; index < fifo_len / 4; index++)
                    spi_.dr[0] = ((uint32_t *)buffer_write)[i++];
                break;
            case 2:
                fifo_len = fifo_len / 2 * 2;
                for (index = 0; index < fifo_len / 2; index++)
                    spi_.dr[0] = ((uint16_t *)buffer_write)[i++];
                break;
            default:
                for (index = 0; index < fifo_len; index++)
                    spi_.dr[0] = buffer_write[i++];
                break;
            }
            spi_.ser = device.chip_select_mask_;
            tx_buffer_len -= fifo_len;
        }
        i = 0;
        while (rx_buffer_len)
        {
            fifo_len = spi_.rxflr;
            fifo_len = fifo_len < rx_buffer_len ? fifo_len : rx_buffer_len;
            switch (device.buffer_width_)
            {
            case 4:
                fifo_len = fifo_len / 4 * 4;
                for (index = 0; index < fifo_len / 4; index++)
                    ((uint32_t *)buffer_read)[i++] = spi_.dr[0];
                break;
            case 2:
                fifo_len = fifo_len / 2 * 2;
                for (index = 0; index < fifo_len / 2; index++)
                    ((uint16_t *)buffer_read)[i++] = (uint16_t)spi_.dr[0];
                break;
            default:
                for (index = 0; index < fifo_len; index++)
                    buffer_read[i++] = (uint8_t)spi_.dr[0];
                break;
            }
            spi_.ser = device.chip_select_mask_;
            rx_buffer_len -= fifo_len;
        }
        vTaskExitCritical();
    }
    else
    {
        uintptr_t dma_write = dma_open_free();
        uintptr_t dma_read = dma_open_free();

        dma_set_request_source(dma_write, dma_req_ + 1);
        dma_set_request_source(dma_read, dma_req_);
        spi_.ctrlr1 = rx_frames - 1;
        spi_.dmacr = 0x3;
        spi_.ssienr = 0x01;
        spi_.ser = device.chip_select_mask_;
        SemaphoreHandle_t event_read = xSemaphoreCreateBinary(), event_write = xSemaphoreCreateBinary();
        dma_transmit_async(dma_read, &spi_.dr[0], buffer_read, 0, 1, device.buffer_width_, rx_frames, 1, event_read);
        dma_transmit_async(dma_write, buffer_write, &spi_.dr[0], 1, 0, device.buffer_width_, tx_frames, 4, event_write);

        configASSERT(xSemaphoreTake(event_read, SPI_DMA_BLOCK_TIME) == pdTRUE && xSemaphoreTake(event_write, SPI_DMA_BLOCK_TIME) == pdTRUE);

        dma_close(dma_write);
        dma_close(dma_read);
        vSemaphoreDelete(event_read);
        vSemaphoreDelete(event_write);
    }
    spi_.ser = 0x00;
    spi_.ssienr = 0x00;
    spi_.dmacr = 0x00;

    return read_buffer.size();
}

void k_spi_driver::fill(k_spi_device_driver &device, uint32_t instruction, uint32_t address, uint32_t value, size_t count)
{
    COMMON_ENTRY;
    setup_device(device);

    uintptr_t dma_write = dma_open_free();
    dma_set_request_source(dma_write, dma_req_ + 1);

    set_bit_mask(&spi_.ctrlr0, TMOD_MASK, TMOD_VALUE(1));
    spi_.dmacr = 0x2;
    spi_.ssienr = 0x01;

    const uint8_t *buffer = (const uint8_t *)&instruction;
    write_inst_addr(spi_.dr, &buffer, device.inst_width_);
    buffer = (const uint8_t *)&address;
    write_inst_addr(spi_.dr, &buffer, device.addr_width_);

    SemaphoreHandle_t event_write = xSemaphoreCreateBinary();
    dma_transmit_async(dma_write, &value, &spi_.dr[0], 0, 0, sizeof(uint32_t), count, 4, event_write);

    spi_.ser = device.chip_select_mask_;
    configASSERT(xSemaphoreTake(event_write, SPI_DMA_BLOCK_TIME) == pdTRUE);
    dma_close(dma_write);
    vSemaphoreDelete(event_write);

    while ((spi_.sr & 0x05) != 0x04)
        ;
    spi_.ser = 0x00;
    spi_.ssienr = 0x00;
    spi_.dmacr = 0x00;
}

void k_spi_driver::setup_device(k_spi_device_driver &device)
{
    spi_.baudr = device.baud_rate_;
    spi_.imr = 0x00;
    spi_.dmacr = 0x00;
    spi_.dmatdlr = 0x10;
    spi_.dmardlr = 0x0;
    spi_.ser = 0x00;
    spi_.ssienr = 0x00;
    spi_.ctrlr0 = (device.mode_ << mod_off_) | (device.frame_format_ << frf_off_) | ((device.data_bit_length_ - 1) << dfs_off_);
    spi_.spi_ctrlr0 = 0;

    if (device.frame_format_ != SPI_FF_STANDARD)
    {
        configASSERT(device.wait_cycles_ < (1 << 5));

        uint32_t inst_l = 0;
        switch (device.instruction_length_)
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
        switch (device.trans_mode_)
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

        configASSERT(device.address_length_ % 4 == 0 && device.address_length_ <= 60);
        uint32_t addr_l = device.address_length_ / 4;

        spi_.spi_ctrlr0 = (device.wait_cycles_ << 11) | (inst_l << 8) | (addr_l << 2) | trans;
        spi_.endian = device.endian_;
    }
}

static k_spi_driver dev0_driver(SPI0_BASE_ADDR, SYSCTL_CLOCK_SPI0, SYSCTL_DMA_SELECT_SSI0_RX_REQ, 6, 16, 8, 21);
static k_spi_driver dev1_driver(SPI1_BASE_ADDR, SYSCTL_CLOCK_SPI1, SYSCTL_DMA_SELECT_SSI1_RX_REQ, 6, 16, 8, 21);
static k_spi_driver dev_slave_driver(SPI_SLAVE_BASE_ADDR, SYSCTL_CLOCK_SPI2, SYSCTL_DMA_SELECT_SSI2_RX_REQ, 6, 16, 8, 21);
static k_spi_driver dev3_driver(SPI3_BASE_ADDR, SYSCTL_CLOCK_SPI3, SYSCTL_DMA_SELECT_SSI3_RX_REQ, 8, 0, 10, 22);

driver &g_spi_driver_spi0 = dev0_driver;
driver &g_spi_driver_spi1 = dev1_driver;
driver &g_spi_driver_spi_slave = dev_slave_driver;
driver &g_spi_driver_spi3 = dev3_driver;
