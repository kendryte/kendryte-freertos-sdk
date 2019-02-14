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
#include "display_context.h"
#include <gsl/span>
#include <hal.h>
#include <kernel/driver_impl.hpp>
#include <stdlib.h>
#include <string.h>

using namespace sys;

size_t get_pixel_bytes(color_format_t format)
{
    switch (format)
    {
    case color_format_t::COLOR_FORMAT_B5G6R5_UNORM:
        return 2;
    case color_format_t::COLOR_FORMAT_R32G32B32A32_FLOAT:
        return 16;
    default:
        throw std::invalid_argument("Invalid format.");
    }
}

static void copy_bits(const uint8_t *src, size_t src_stride, uint8_t *dest, size_t dest_stride, size_t line_size, size_t height)
{
    for (size_t y = 0; y < height; y++)
    {
        auto begin = src + y * src_stride;
        std::copy(begin, begin + line_size, dest + y * dest_stride);
    }
}

static void fill_bits(const surface_data_t &surface_data, color_format_t format, const color_value_t &color)
{
    if (format == color_format_t::COLOR_FORMAT_B5G6R5_UNORM)
    {
        gsl::span<uint16_t> span = { reinterpret_cast<uint16_t *>(surface_data.data.data()), surface_data.data.size() / 2 };

        auto src = span.data();
        auto value = rgb565::from(color).value;
        for (size_t y = 0; y < surface_data.rect.bottom - surface_data.rect.top; y++)
        {
            for (size_t x = 0; x < surface_data.rect.right - surface_data.rect.left; x++)
                src[x] = value;

            src += surface_data.stride / 2;
        }
    }
    else
    {
        throw std::runtime_error("Not implemented.");
    }
}

class software_surface : public surface, public heap_object, public free_object_access
{
public:
    software_surface(color_format_t format, const size_u_t &size)
        : format_(format), size_(size), stride_(size.width * get_pixel_bytes(format))
    {
        auto bytes = stride_ * size.height;
        storage_ = std::make_unique<uint8_t[]>(bytes);
        data_ = { storage_.get(), ptrdiff_t(bytes) };
    }

    software_surface(color_format_t format, const size_u_t &size, const surface_data_t &surface_data, bool copy)
        : format_(format), size_(size)
    {
        if (!copy)
        {
            stride_ = surface_data.stride;
            data_ = surface_data.data;

            auto bytes = stride_ * size.height;
            configASSERT(bytes == data_.size_bytes());
        }
        else
        {
            stride_ = size.width * get_pixel_bytes(format);
            auto bytes = stride_ * size.height;
            storage_ = std::make_unique<uint8_t[]>(bytes);
            data_ = { storage_.get(), ptrdiff_t(bytes) };
            copy_bits(surface_data.data.data(), surface_data.stride, data_.data(), stride_, stride_, size.height);
        }
    }

    virtual void on_first_open() override
    {
    }

    virtual void on_last_close() override
    {
    }

    virtual size_u_t get_pixel_size() noexcept override
    {
        return size_;
    }

    virtual color_format_t get_format() noexcept override
    {
        return format_;
    }

    virtual surface_location_t get_location() noexcept override
    {
        return surface_location_t::SYSTEM_MEMORY;
        ;
    }

    virtual surface_data_t lock(const rect_u_t &rect) override
    {
        auto begin = rect.top * stride_ + get_pixel_bytes(format_) * rect.left;
        auto end = (int32_t(rect.bottom - 1)) * stride_ + get_pixel_bytes(format_) * rect.right;

        if (begin > data_.size_bytes() || end > data_.size_bytes())
            throw std::out_of_range("Lock rect is out of range.");

        return { { data_.data() + begin, data_.data() + end }, stride_, rect };
    }

    virtual void unlock(surface_data_t &data) override
    {
    }

private:
    color_format_t format_;
    size_u_t size_;
    surface_location_t location_;
    size_t stride_;

    gsl::span<uint8_t> data_;
    std::unique_ptr<uint8_t[]> storage_;
};

class k_display_context : public display_driver, public heap_object, public free_object_access
{
public:
    k_display_context(object_accessor<display_driver> &&device)
        : device_(std::move(device))
    {
        primary_surface_ = device_->get_primary_surface();
        offscreen_surface_ = get_software_surface(primary_surface_->get_format(), primary_surface_->get_pixel_size());
    }

    virtual object_ptr<surface> get_primary_surface() override
    {
        return primary_surface_;
    }

    virtual object_ptr<surface> get_software_surface(color_format_t format, const size_u_t &size)
    {
        return make_object<software_surface>(format, size);
    }

    virtual object_ptr<surface> get_software_surface(color_format_t format, const size_u_t &size, const surface_data_t &data, bool copy)
    {
        return make_object<software_surface>(format, size, data, copy);
    }

    virtual void install() override
    {
    }
    virtual void on_first_open() override
    {
    }

    virtual void on_last_close() override
    {
    }

    virtual void clear(object_ptr<surface> surface, const rect_u_t &rect, const color_value_t &color) override
    {
        if (surface->get_location() == surface_location_t::DEVICE_MEMORY)
        {
            device_->clear(surface, rect, color);
            auto src_locker = offscreen_surface_->lock(rect);
            fill_bits(src_locker, offscreen_surface_->get_format(), color);
            offscreen_surface_->unlock(src_locker);
        }
        else
        {
            auto src_locker = surface->lock(rect);
            fill_bits(src_locker, surface->get_format(), color);
            surface->unlock(src_locker);
        }
    }

    virtual void copy_subresource(object_ptr<surface> src, object_ptr<surface> dest, const rect_u_t &src_rect, const point_u_t &dest_position) override
    {
        if (src->get_format() != dest->get_format())
            throw std::invalid_argument("Src and dest must have same format.");

        if (src->get_location() == surface_location_t::SYSTEM_MEMORY && dest->get_location() == surface_location_t::SYSTEM_MEMORY)
        {
            auto src_locker = src->lock(src_rect);
            auto dest_locker = dest->lock({ dest_position, src_rect.get_size() });
            auto line_size = src_rect.get_size().width * get_pixel_bytes(src->get_format());
            copy_bits(src_locker.data.data(), src_locker.stride, dest_locker.data.data(), dest_locker.stride, line_size, src_rect.get_size().height);
            dest->unlock(dest_locker);
            src->unlock(src_locker);
        }
        else if (dest->get_location() == surface_location_t::SYSTEM_MEMORY)
        {
            auto src_locker = offscreen_surface_->lock(src_rect);
            auto dest_locker = dest->lock({ dest_position, src_rect.get_size() });
            auto line_size = src_rect.get_size().width * get_pixel_bytes(offscreen_surface_->get_format());
            copy_bits(src_locker.data.data(), src_locker.stride, dest_locker.data.data(), dest_locker.stride, line_size, src_rect.get_size().height);
            dest->unlock(dest_locker);
            offscreen_surface_->unlock(src_locker);
        }
        else if (src->get_location() == surface_location_t::SYSTEM_MEMORY)
        {
            device_->copy_subresource(src, dest, src_rect, dest_position);
            auto src_locker = src->lock(src_rect);
            auto dest_locker = offscreen_surface_->lock({ dest_position, src_rect.get_size() });
            auto line_size = src_rect.get_size().width * get_pixel_bytes(src->get_format());
            copy_bits(src_locker.data.data(), src_locker.stride, dest_locker.data.data(), dest_locker.stride, line_size, src_rect.get_size().height);
            offscreen_surface_->unlock(dest_locker);
            src->unlock(src_locker);
        }
        else
        {
            throw std::runtime_error("Not supported.");
        }
    }

private:
    object_accessor<display_driver> device_;
    object_ptr<surface> primary_surface_;
    object_ptr<surface> offscreen_surface_;
};

#define DISPLAY_ENTRY                                    \
    auto &obj = system_handle_to_object(display_handle); \
    configASSERT(obj.is<k_display_context>());           \
    auto f = obj.as<k_display_context>();

#define CATCH_ALL \
    catch (...) { return -1; }

handle_t create_display_context(handle_t lcd_handle)
{
    try
    {
        auto display_context = make_object<k_display_context>(system_handle_to_object(lcd_handle).move_as<display_driver>());

        return system_alloc_handle(make_accessor(display_context));
    }
    catch (...)
    {
        return NULL_HANDLE;
    }
}

int clear_screen(handle_t display_handle, const point_u_t *position, uint32_t width, uint32_t height, const color_value_t *color)
{
    try
    {
        DISPLAY_ENTRY;
        auto surf = f->get_primary_surface();
        rect_u_t rect = { *position, { width, height } };
        f->clear(surf, rect, *color);

        return 0;
    }
    CATCH_ALL;
}

int display_screen(handle_t display_handle, const point_u_t *position, uint32_t width, uint32_t height, const uint8_t *picture)
{
    try
    {
        DISPLAY_ENTRY;

        rect_u_t src_rect = { { 0, 0 }, { width, height } };

        auto src_surface = f->get_software_surface(color_format_t::COLOR_FORMAT_B5G6R5_UNORM, { width, height });
        auto dst_surface = f->get_primary_surface();

        auto src_locker = src_surface->lock(src_rect);
        auto line_size = src_rect.get_size().width * get_pixel_bytes(src_surface->get_format());
        copy_bits(picture, src_locker.stride, src_locker.data.data(), src_locker.stride, line_size, src_rect.get_size().height);
        src_surface->unlock(src_locker);

        f->copy_subresource(src_surface, dst_surface, src_rect, *position);
        return 0;
    }
    CATCH_ALL;
}

int capture_picture(handle_t display_handle, const point_u_t *position, uint32_t width, uint32_t height, uint8_t *picture)
{
    try
    {
        DISPLAY_ENTRY;

        rect_u_t src_rect = { *position, { width, height } };
        auto src_surface = f->get_primary_surface();
        auto dst_surface = f->get_software_surface(color_format_t::COLOR_FORMAT_B5G6R5_UNORM, { width, height });
        f->copy_subresource(src_surface, dst_surface, src_rect, {0, 0});

		rect_u_t dst_rect = { {0, 0}, { width, height } };
        auto dst_locker = dst_surface->lock(dst_rect);
		auto line_size = dst_rect.get_size().width * get_pixel_bytes(dst_surface->get_format());
        copy_bits(dst_locker.data.data(), dst_locker.stride, picture, dst_locker.stride, line_size, dst_rect.get_size().height);
		dst_surface->unlock(dst_locker);

        return 0;
    }
    CATCH_ALL;
}