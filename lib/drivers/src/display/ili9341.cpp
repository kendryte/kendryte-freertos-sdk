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
#include "display/ili9341.h"
#include <hal.h>
#include <kernel/driver_impl.hpp>
#include <stdlib.h>
#include <string.h>
#include <sys/unistd.h>

using namespace sys;

#define SPI_SLAVE_SELECT    3
#define SPI_CLOCK_RATE      3200000U
#define WAIT_CYCLE          0U

enum _instruction_length
{
    INSTRUCTION_LEN_0 = 0,
    INSTRUCTION_LEN_8 = 8,
    INSTRUCTION_LEN_16 = 16,
    INSTRUCTION_LEN_32 = 32,
};

enum _address_length
{
    ADDRESS_LEN_0 = 0,
    ADDRESS_LEN_8 = 8,
    ADDRESS_LEN_16 = 16,
    ADDRESS_LEN_32 = 32,
};

enum _frame_length
{
    FRAME_LEN_0 = 0,
    FRAME_LEN_8 = 8,
    FRAME_LEN_16 = 16,
    FRAME_LEN_32 = 32,
};

typedef struct
{
    uint8_t dir;
    uint16_t width;
    uint16_t height;
} lcd_ctl_t;

static lcd_ctl_t lcd_ctl;

namespace
{
static constexpr uint16_t pixel_width = LCD_Y_MAX;
static constexpr uint16_t pixel_height = LCD_X_MAX;
}

class ili9341_primary_surface : public surface, public heap_object, public free_object_access
{
public:
    ili9341_primary_surface()
    {
    }

    virtual void on_first_open() override
    {
    }

    virtual void on_last_close() override
    {
    }

    virtual size_u_t get_pixel_size() noexcept override
    {
        return { pixel_width, pixel_height };
    }

    virtual color_format_t get_format() noexcept override
    {
        return color_format_t::COLOR_FORMAT_B5G6R5_UNORM;
    }

    virtual surface_data_t lock(const rect_u_t &rect) override
    {
        throw std::runtime_error("Not supported.");
    }

    virtual void unlock(surface_data_t &data) override
    {
        throw std::runtime_error("Not supported.");
    }

    virtual surface_location_t get_location() noexcept override
    {
        return surface_location_t::DEVICE_MEMORY;
    }

private:

};

class ili9341_driver : public display_driver, public heap_object, public free_object_access
{
public:
    ili9341_driver(handle_t spi_handle, handle_t dcx_gpio_handle, uint32_t dcx_gpio_pin)
        : spi_driver_(system_handle_to_object(spi_handle).get_object().as<spi_driver>())
        , dcx_gpio_driver_(system_handle_to_object(dcx_gpio_handle).get_object().as<gpio_driver>())
        , dcx_gpio_pin_(dcx_gpio_pin)
    {
    }

    virtual void install() override
    {
    }

    virtual void on_first_open() override
    {
        auto spi = make_accessor(spi_driver_);
        spi8_dev_ = make_accessor(spi->get_device(SPI_MODE_0, SPI_FF_OCTAL, 1 << SPI_SLAVE_SELECT, 8));
        spi16_dev_ = make_accessor(spi->get_device(SPI_MODE_0, SPI_FF_OCTAL, 1 << SPI_SLAVE_SELECT, 16));
        spi32_dev_ = make_accessor(spi->get_device(SPI_MODE_0, SPI_FF_OCTAL, 1 << SPI_SLAVE_SELECT, 32));

        dcx_gpio_ = make_accessor(dcx_gpio_driver_);
        dcx_gpio_->set_drive_mode(dcx_gpio_pin_, GPIO_DM_OUTPUT);
        dcx_gpio_->set_pin_value(dcx_gpio_pin_, GPIO_PV_HIGH);

        spi8_dev_->config_non_standard(INSTRUCTION_LEN_8, ADDRESS_LEN_0, WAIT_CYCLE, SPI_AITM_AS_FRAME_FORMAT);
        spi16_dev_->config_non_standard(INSTRUCTION_LEN_16, ADDRESS_LEN_0, WAIT_CYCLE, SPI_AITM_AS_FRAME_FORMAT);
        spi32_dev_->config_non_standard(INSTRUCTION_LEN_0, ADDRESS_LEN_32, WAIT_CYCLE, SPI_AITM_AS_FRAME_FORMAT);

        spi8_dev_->set_clock_rate(SPI_CLOCK_RATE);
        spi16_dev_->set_clock_rate(SPI_CLOCK_RATE);
        spi32_dev_->set_clock_rate(SPI_CLOCK_RATE);
        initialize();
    }

    virtual void on_last_close() override
    {
        spi8_dev_.reset();
        spi16_dev_.reset();
        spi32_dev_.reset();
        dcx_gpio_.reset();
    }

    virtual object_ptr<surface> get_primary_surface() override
    {
        return make_object<ili9341_primary_surface>();
    }

    virtual void clear(object_ptr<surface> surface, const rect_u_t &rect, const color_value_t &color) override
    {
        lcd_set_area(rect.left, rect.top, rect.right-1, rect.bottom-1);
        uint32_t data = (uint32_t)rgb565::from(color).value << 16 | (uint32_t)rgb565::from(color).value;

        tft_fill_data(&data, rect.get_size().width * rect.get_size().height / 2);
    }

    virtual void copy_subresource(object_ptr<surface> src, object_ptr<surface> dest, const rect_u_t &src_rect, const point_u_t &dest_position) override
    {
        if(src->get_location() == surface_location_t::DEVICE_MEMORY)
        {
        }
        if (dest->get_location() == surface_location_t::DEVICE_MEMORY)
        {
            auto locker = src->lock(src_rect);
            lcd_draw_picture(dest_position.x, dest_position.y, src_rect.get_size().width, src_rect.get_size().height, (uint16_t *)locker.data.data());
            src->unlock(locker);
        }
    }

private:
    void set_dcx_control()
    {
        dcx_gpio_->set_pin_value(dcx_gpio_pin_, GPIO_PV_LOW);
    }

    void set_dcx_data()
    {
        dcx_gpio_->set_pin_value(dcx_gpio_pin_, GPIO_PV_HIGH);
    }

    void tft_write_command(const uint8_t data_buff)
    {
        set_dcx_control();
        spi8_dev_->write( {&data_buff, 1L});
    }

    void tft_write_byte(const uint8_t *data_buff, size_t length)
    {
        set_dcx_data();
        spi8_dev_->write( {data_buff, std::ptrdiff_t(length)} );
    }

    void tft_write_half(const uint16_t *data_buff, size_t length)
    {
        set_dcx_data();
        spi16_dev_->write({ (const uint8_t *)data_buff, std::ptrdiff_t(length) * 2 });
    }

    void tft_write_word(const uint32_t *data_buff, size_t length)
    {
        set_dcx_data();
        spi32_dev_->write({ (const uint8_t *)data_buff, std::ptrdiff_t(length) * 4 });
    }

    void tft_fill_data(uint32_t *data_buf, uint32_t length)
    {
        set_dcx_data();
        spi32_dev_->fill(0, *data_buf, *data_buf, std::ptrdiff_t(length) - 1);
    }

    void initialize()
    {
        uint8_t data;
        tft_write_command(SOFTWARE_RESET);
        usleep(100 * 1000);
        tft_write_command(SLEEP_OFF);
        usleep(100 * 1000);
        tft_write_command(PIXEL_FORMAT_SET);
        data = 0x55;
        tft_write_byte(&data, 1);
        lcd_set_direction(DIR_YX_RLUD);
        tft_write_command(DISPALY_ON);
    }

    void lcd_set_direction(enum lcd_dir_t dir)
    {
        uint8_t dir_data = dir;
        dir_data |= 0x08;
        lcd_ctl.dir = dir_data;
        if (lcd_ctl.dir & DIR_XY_MASK)
        {
            lcd_ctl.width = LCD_Y_MAX - 1;
            lcd_ctl.height = LCD_X_MAX - 1;
        }
        else
        {
            lcd_ctl.width = LCD_X_MAX - 1;
            lcd_ctl.height = LCD_Y_MAX - 1;
        }

        tft_write_command(MEMORY_ACCESS_CTL);
        tft_write_byte((uint8_t *)&lcd_ctl.dir, 1);
    }

    void lcd_set_area(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
    {
        uint8_t data[4];

        data[0] = (uint8_t)(x1 >> 8);
        data[1] = (uint8_t)(x1);
        data[2] = (uint8_t)(x2 >> 8);
        data[3] = (uint8_t)(x2);
        tft_write_command(HORIZONTAL_ADDRESS_SET);
        tft_write_byte(data, 4);
        data[0] = (uint8_t)(y1 >> 8);
        data[1] = (uint8_t)(y1);
        data[2] = (uint8_t)(y2 >> 8);
        data[3] = (uint8_t)(y2);
        tft_write_command(VERTICAL_ADDRESS_SET);
        tft_write_byte(data, 4);
        tft_write_command(MEMORY_WRITE);
    }

    void lcd_draw_picture(uint16_t x1, uint16_t y1, uint16_t width, uint16_t height, uint16_t *ptr)
    {
        lcd_set_area(x1, y1, x1 + width - 1, y1 + height - 1);
        tft_write_half(ptr, width * height);
    }

private:
    object_ptr<spi_driver> spi_driver_;
    object_ptr<gpio_driver> dcx_gpio_driver_;
    uint32_t dcx_gpio_pin_;

    object_accessor<gpio_driver> dcx_gpio_;
    object_accessor<spi_device_driver> spi8_dev_;
    object_accessor<spi_device_driver> spi16_dev_;
    object_accessor<spi_device_driver> spi32_dev_;
};

handle_t ili9341_driver_install(handle_t spi_handle, handle_t dcx_gpio_handle, uint32_t dcx_gpio_pin)
{
    try
    {
        auto driver = make_object<ili9341_driver>(spi_handle, dcx_gpio_handle, dcx_gpio_pin);
        driver->install();
        return system_alloc_handle(make_accessor(driver));
    }
    catch (...)
    {
        return NULL_HANDLE;
    }
}
