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
#ifndef _FREERTOS_DRIVER_H
#define _FREERTOS_DRIVER_H

#include "object.hpp"
#include <gsl/span>
#include <memory>

#define MAKE_ENUM_CLASS_BITMASK_TYPE(enumName)                                                           \
    static_assert(std::is_enum<enumName>::value, "enumName is not a enum.");                             \
    constexpr enumName operator|(enumName a, enumName b) noexcept                                        \
    {                                                                                                    \
        typedef std::underlying_type_t<enumName> underlying_type;                                        \
        return static_cast<enumName>(static_cast<underlying_type>(a) | static_cast<underlying_type>(b)); \
    }                                                                                                    \
    constexpr enumName operator&(enumName a, enumName b) noexcept                                        \
    {                                                                                                    \
        typedef std::underlying_type_t<enumName> underlying_type;                                        \
        return static_cast<enumName>(static_cast<underlying_type>(a) & static_cast<underlying_type>(b)); \
    }                                                                                                    \
    constexpr enumName operator^(enumName a, enumName b) noexcept                                        \
    {                                                                                                    \
        typedef std::underlying_type_t<enumName> underlying_type;                                        \
        return static_cast<enumName>(static_cast<underlying_type>(a) ^ static_cast<underlying_type>(b)); \
    }                                                                                                    \
    constexpr enumName operator~(enumName a) noexcept                                                    \
    {                                                                                                    \
        typedef std::underlying_type_t<enumName> underlying_type;                                        \
        return static_cast<enumName>(~static_cast<underlying_type>(a));                                  \
    }                                                                                                    \
    constexpr enumName &operator|=(enumName &a, enumName b) noexcept                                     \
    {                                                                                                    \
        return a = (a | b);                                                                              \
    }                                                                                                    \
    constexpr enumName &operator&=(enumName &a, enumName b) noexcept                                     \
    {                                                                                                    \
        return a = (a & b);                                                                              \
    }                                                                                                    \
    constexpr enumName &operator^=(enumName &a, enumName b) noexcept                                     \
    {                                                                                                    \
        return a = (a ^ b);                                                                              \
    }

namespace sys
{
MAKE_ENUM_CLASS_BITMASK_TYPE(file_access_t);
MAKE_ENUM_CLASS_BITMASK_TYPE(file_mode_t);
MAKE_ENUM_CLASS_BITMASK_TYPE(socket_message_flag_t);

class errno_exception : public std::runtime_error
{
public:
    explicit errno_exception(const char *msg, int code) noexcept
        : runtime_error(msg), code_(code)
    {
    }

    int code() const noexcept
    {
        return code_;
    }

private:
    int code_;
};

class object_access : public virtual object
{
public:
    virtual void open() = 0;
    virtual void close() = 0;
};

template <class T>
struct object_accessor
{
public:
    object_accessor() = default;

    object_accessor(object_accessor &) = delete;
    object_accessor &operator=(object_accessor &other) = delete;

    template <class U>
    object_accessor(object_accessor<U> &&other)
        : obj_(std::move(other.obj_))
    {
    }

    explicit object_accessor(object_ptr<T> &&obj) noexcept
        : obj_(std::move(obj))
    {
    }

    ~object_accessor()
    {
        if (obj_)
            std::move(obj_)->close();
    }

    operator bool() const noexcept
    {
        return obj_;
    }

    object_accessor &operator=(object_accessor &&other) noexcept
    {
        if (obj_)
            obj_->close();
        obj_ = std::move(other.obj_);
        return *this;
    }

    T *operator->() const noexcept
    {
        return obj_.get();
    }

    template <class U>
    bool is() const
    {
        return obj_.template is<U>();
    }

    template <class U>
    object_accessor<U> move_as()
    {
        auto obj = obj_.template as<U>();
        if (obj_ && !obj)
            throw std::bad_cast();
        obj_.reset();
        return object_accessor<U>(std::move(obj));
    }

    template <class U>
    U *as()
    {
        auto obj = obj_.template as<U>();
        if (obj)
            return obj.get();
        return nullptr;
    }

    object_ptr<T> get_object() const noexcept
    {
        return obj_;
    }

    void reset() noexcept
    {
        if (obj_)
            std::move(obj_)->close();
    }

private:
    template <class U>
    friend class object_accessor;

    object_ptr<T> obj_;
};

template <typename T>
object_accessor<T> make_accessor(object_ptr<T> obj)
{
    obj->open();
    return object_accessor<T>(std::move(obj));
}

class driver : public virtual object, public virtual object_access
{
public:
    virtual void install() = 0;
};

typedef struct tag_driver_registry
{
    const char *name;
    object_ptr<driver> driver_ptr;
} driver_registry_t;

class uart_driver : public driver
{
public:
    virtual void config(uint32_t baud_rate, uint32_t databits, uart_stopbits_t stopbits, uart_parity_t parity) = 0;
    virtual int read(gsl::span<uint8_t> buffer) = 0;
    virtual int write(gsl::span<const uint8_t> buffer) = 0;
    virtual void set_read_timeout(size_t millisecond) = 0;
};

class gpio_driver : public driver
{
public:
    virtual uint32_t get_pin_count() = 0;
    virtual void set_drive_mode(uint32_t pin, gpio_drive_mode_t mode) = 0;
    virtual void set_pin_edge(uint32_t pin, gpio_pin_edge_t edge) = 0;
    virtual void set_on_changed(uint32_t pin, gpio_on_changed_t callback, void *userdata) = 0;
    virtual gpio_pin_value_t get_pin_value(uint32_t pin) = 0;
    virtual void set_pin_value(uint32_t pin, gpio_pin_value_t value) = 0;
};

class i2c_device_driver : public driver
{
public:
    virtual double set_clock_rate(double clock_rate) = 0;
    virtual int read(gsl::span<uint8_t> buffer) = 0;
    virtual int write(gsl::span<const uint8_t> buffer) = 0;
    virtual int transfer_sequential(gsl::span<const uint8_t> write_buffer, gsl::span<uint8_t> read_buffer) = 0;
};

class i2c_driver : public driver
{
public:
    virtual object_ptr<i2c_device_driver> get_device(uint32_t slave_address, uint32_t address_width) = 0;
    virtual void config_as_slave(uint32_t slave_address, uint32_t address_width, const i2c_slave_handler_t &handler) = 0;
    virtual double slave_set_clock_rate(double clock_rate) = 0;
};

class i2s_driver : public driver
{
public:
    virtual void config_as_render(const audio_format_t &format, size_t delay_ms, i2s_align_mode_t align_mode, uint32_t channels_mask) = 0;
    virtual void config_as_capture(const audio_format_t &format, size_t delay_ms, i2s_align_mode_t align_mode, uint32_t channels_mask) = 0;
    virtual void get_buffer(gsl::span<uint8_t> &buffer, size_t &frames) = 0;
    virtual void release_buffer(uint32_t frames) = 0;
    virtual void start() = 0;
    virtual void stop() = 0;
};

class spi_device_driver : public driver
{
public:
    virtual void config_non_standard(uint32_t instruction_length, uint32_t address_length, uint32_t wait_cycles, spi_inst_addr_trans_mode_t trans_mode) = 0;
    virtual double set_clock_rate(double clock_rate) = 0;
    virtual void set_endian(uint32_t endian) = 0;
    virtual int read(gsl::span<uint8_t> buffer) = 0;
    virtual int write(gsl::span<const uint8_t> buffer) = 0;
    virtual int transfer_full_duplex(gsl::span<const uint8_t> write_buffer, gsl::span<uint8_t> read_buffer) = 0;
    virtual int transfer_sequential(gsl::span<const uint8_t> write_buffer, gsl::span<uint8_t> read_buffer) = 0;
    virtual void fill(uint32_t instruction, uint32_t address, uint32_t value, size_t count) = 0;
};

class spi_driver : public driver
{
public:
    virtual object_ptr<spi_device_driver> get_device(spi_mode_t mode, spi_frame_format_t frame_format, uint32_t chip_select_mask, uint32_t data_bit_length) = 0;
    virtual void slave_config(handle_t gpio_handle, uint8_t int_pin, uint8_t ready_pin, size_t data_bit_length, uint8_t *data, uint32_t len, spi_slave_receive_callback_t callback) = 0;
};

class dvp_driver : public driver
{
public:
    virtual uint32_t get_output_num() = 0;
    virtual void config(uint32_t width, uint32_t height, bool auto_enable) = 0;
    virtual void enable_frame() = 0;
    virtual void set_signal(dvp_signal_type_t type, bool value) = 0;
    virtual void set_output_enable(uint32_t index, bool enable) = 0;
    virtual void set_output_attributes(uint32_t index, video_format_t format, void *output_buffer) = 0;
    virtual void set_frame_event_enable(dvp_frame_event_t event, bool enable) = 0;
    virtual void set_on_frame_event(dvp_on_frame_event_t callback, void *userdata) = 0;
    virtual double xclk_set_clock_rate(double clock_rate) = 0;
};

class sccb_device_driver : public driver
{
public:
    virtual uint8_t read_byte(uint16_t reg_address) = 0;
    virtual void write_byte(uint16_t reg_address, uint8_t value) = 0;
};

class sccb_driver : public driver
{
public:
    virtual object_ptr<sccb_device_driver> get_device(uint32_t slave_address, uint32_t reg_address_width) = 0;
};

class fft_driver : public driver
{
public:
    virtual void complex_uint16(uint16_t shift, fft_direction_t direction, const uint64_t *input, size_t point_num, uint64_t *output) = 0;
};

class aes_driver : public driver
{
public:
    virtual void aes_ecb128_hard_decrypt(gsl::span<const uint8_t> input_key, gsl::span<const uint8_t> input_data, gsl::span<uint8_t> output_data) = 0;
    virtual void aes_ecb128_hard_encrypt(gsl::span<const uint8_t> input_key, gsl::span<const uint8_t> input_data, gsl::span<uint8_t> output_data) = 0;
    virtual void aes_ecb192_hard_decrypt(gsl::span<const uint8_t> input_key, gsl::span<const uint8_t> input_data, gsl::span<uint8_t> output_data) = 0;
    virtual void aes_ecb192_hard_encrypt(gsl::span<const uint8_t> input_key, gsl::span<const uint8_t> input_data, gsl::span<uint8_t> output_data) = 0;
    virtual void aes_ecb256_hard_decrypt(gsl::span<const uint8_t> input_key, gsl::span<const uint8_t> input_data, gsl::span<uint8_t> output_data) = 0;
    virtual void aes_ecb256_hard_encrypt(gsl::span<const uint8_t> input_key, gsl::span<const uint8_t> input_data, gsl::span<uint8_t> output_data) = 0;
    virtual void aes_cbc128_hard_decrypt(cbc_context_t &context, gsl::span<const uint8_t> input_data, gsl::span<uint8_t> output_data) = 0;
    virtual void aes_cbc128_hard_encrypt(cbc_context_t &context, gsl::span<const uint8_t> input_data, gsl::span<uint8_t> output_data) = 0;
    virtual void aes_cbc192_hard_decrypt(cbc_context_t &context, gsl::span<const uint8_t> input_data, gsl::span<uint8_t> output_data) = 0;
    virtual void aes_cbc192_hard_encrypt(cbc_context_t &context, gsl::span<const uint8_t> input_data, gsl::span<uint8_t> output_data) = 0;
    virtual void aes_cbc256_hard_decrypt(cbc_context_t &context, gsl::span<const uint8_t> input_data, gsl::span<uint8_t> output_data) = 0;
    virtual void aes_cbc256_hard_encrypt(cbc_context_t &context, gsl::span<const uint8_t> input_data, gsl::span<uint8_t> output_data) = 0;
    virtual void aes_gcm128_hard_decrypt(gcm_context_t &context, gsl::span<const uint8_t> input_data, gsl::span<uint8_t> output_data, gsl::span<uint8_t> gcm_tag) = 0;
    virtual void aes_gcm128_hard_encrypt(gcm_context_t &context, gsl::span<const uint8_t> input_data, gsl::span<uint8_t> output_data, gsl::span<uint8_t> gcm_tag) = 0;
    virtual void aes_gcm192_hard_decrypt(gcm_context_t &context, gsl::span<const uint8_t> input_data, gsl::span<uint8_t> output_data, gsl::span<uint8_t> gcm_tag) = 0;
    virtual void aes_gcm192_hard_encrypt(gcm_context_t &context, gsl::span<const uint8_t> input_data, gsl::span<uint8_t> output_data, gsl::span<uint8_t> gcm_tag) = 0;
    virtual void aes_gcm256_hard_decrypt(gcm_context_t &context, gsl::span<const uint8_t> input_data, gsl::span<uint8_t> output_data, gsl::span<uint8_t> gcm_tag) = 0;
    virtual void aes_gcm256_hard_encrypt(gcm_context_t &context, gsl::span<const uint8_t> input_data, gsl::span<uint8_t> output_data, gsl::span<uint8_t> gcm_tag) = 0;
};

class sha256_driver : public driver
{
public:
    virtual void sha256_hard_calculate(gsl::span<const uint8_t> input_data, gsl::span<uint8_t> output_data) = 0;
};

class timer_driver : public driver
{
public:
    virtual size_t set_interval(size_t nanoseconds) = 0;
    virtual void set_on_tick(timer_on_tick_t on_tick, void *userdata) = 0;
    virtual void set_enable(bool enable) = 0;
};

class pwm_driver : public driver
{
public:
    virtual uint32_t get_pin_count() = 0;
    virtual double set_frequency(double frequency) = 0;
    virtual double set_active_duty_cycle_percentage(uint32_t pin, double duty_cycle_percentage) = 0;
    virtual void set_enable(uint32_t pin, bool enable) = 0;
};

class wdt_driver : public driver
{
public:
    virtual void set_response_mode(wdt_response_mode_t mode) = 0;
    virtual size_t set_timeout(size_t nanoseconds) = 0;
    virtual void set_on_timeout(wdt_on_timeout_t handler, void *userdata) = 0;
    virtual void restart_counter() = 0;
    virtual void set_enable(bool enable) = 0;
};

class rtc_driver : public driver
{
public:
    virtual void get_datetime(struct tm &datetime) = 0;
    virtual void set_datetime(const struct tm &datetime) = 0;
};

class kpu_driver : public driver
{
public:
    virtual handle_t model_load_from_buffer(uint8_t *buffer) = 0;
    virtual int run(handle_t context, const uint8_t *src) = 0;
    virtual int get_output(handle_t context, uint32_t index, uint8_t **data, size_t *size) = 0;
};

class custom_driver : public driver
{
public:
    virtual int control(uint32_t control_code, gsl::span<const uint8_t> write_buffer, gsl::span<uint8_t> read_buffer) = 0;
};

/* ===== internal drivers ======*/

void kernel_iface_pic_on_irq(uint32_t irq);

class pic_driver : public driver
{
public:
    virtual void set_irq_enable(uint32_t irq, bool enable) = 0;
    virtual void set_irq_priority(uint32_t irq, uint32_t priority) = 0;
};

class dma_driver : public driver
{
public:
    virtual void set_select_request(uint32_t request) = 0;
    virtual void config(uint32_t priority) = 0;
    virtual void transmit_async(const volatile void *src, volatile void *dest, bool src_inc, bool dest_inc, size_t element_size, size_t count, size_t burst_size, SemaphoreHandle_t completion_event) = 0;
    virtual void loop_async(const volatile void **srcs, size_t src_num, volatile void **dests, size_t dest_num, bool src_inc, bool dest_inc, size_t element_size, size_t count, size_t burst_size, dma_stage_completion_handler_t stage_completion_handler, void *stage_completion_handler_data, SemaphoreHandle_t completion_event, int *stop_signal) = 0;
    virtual void stop() = 0;
};

class dmac_driver : public driver
{
public:
};

class block_storage_driver : public driver
{
public:
    virtual uint32_t get_rw_block_size() = 0;
    virtual uint32_t get_blocks_count() = 0;
    virtual void read_blocks(uint32_t start_block, uint32_t blocks_count, gsl::span<uint8_t> buffer) = 0;
    virtual void write_blocks(uint32_t start_block, uint32_t blocks_count, gsl::span<const uint8_t> buffer) = 0;
};

class filesystem_file : public virtual object_access
{
public:
    virtual size_t read(gsl::span<uint8_t> buffer) = 0;
    virtual size_t write(gsl::span<const uint8_t> buffer) = 0;
    virtual fpos_t get_position() = 0;
    virtual void set_position(fpos_t position) = 0;
    virtual uint64_t get_size() = 0;
    virtual void flush() = 0;
};

class network_adapter_handler
{
public:
    virtual void notify_input() = 0;
};

class network_adapter_driver : public driver
{
public:
    virtual void set_handler(network_adapter_handler *handler) = 0;
    virtual mac_address_t get_mac_address() = 0;
    virtual void disable_rx(void) = 0;
    virtual void enable_rx(void) = 0;
    virtual bool interface_check() = 0;
    virtual bool is_packet_available() = 0;
    virtual void reset(SemaphoreHandle_t interrupt_event) = 0;
    virtual void begin_send(size_t length) = 0;
    virtual void send(gsl::span<const uint8_t> buffer) = 0;
    virtual void end_send() = 0;
    virtual size_t begin_receive() = 0;
    virtual void receive(gsl::span<uint8_t> buffer) = 0;
    virtual void end_receive() = 0;
};

class network_socket : public virtual custom_driver, public virtual object_access
{
public:
    virtual object_accessor<network_socket> accept(socket_address_t *remote_address) = 0;
    virtual void bind(const socket_address_t &address) = 0;
    virtual void connect(const socket_address_t &address) = 0;
    virtual void listen(uint32_t backlog) = 0;
    virtual void shutdown(socket_shutdown_t how) = 0;
    virtual size_t send(gsl::span<const uint8_t> buffer, socket_message_flag_t flags) = 0;
    virtual size_t receive(gsl::span<uint8_t> buffer, socket_message_flag_t flags) = 0;
    virtual size_t send_to(gsl::span<const uint8_t> buffer, socket_message_flag_t flags, const socket_address_t &to) = 0;
    virtual size_t receive_from(gsl::span<uint8_t> buffer, socket_message_flag_t flags, socket_address_t *from) = 0;
    virtual size_t read(gsl::span<uint8_t> buffer) = 0;
    virtual size_t write(gsl::span<const uint8_t> buffer) = 0;
    virtual int fcntl(int cmd, int val) = 0;
    virtual void select(fd_set *readset, fd_set *writeset, fd_set *exceptset, struct timeval *timeout) = 0;
};

extern driver_registry_t g_hal_drivers[];
extern driver_registry_t g_dma_drivers[];
extern driver_registry_t g_system_drivers[];

/**
 * @brief       Install a driver
 * @param[in]   name        Specify the path to access it later
 * @param[in]   type        The type of driver
 *
 * @return      result
 *     - NULL   Fail
 *     - other  The driver registry
 */
driver_registry_t *system_install_driver(const char *name, object_ptr<driver> driver);

object_accessor<driver> system_open_driver(const char *name);

handle_t system_alloc_handle(object_accessor<object_access> object);

object_accessor<object_access> &system_handle_to_object(handle_t file);
}

#endif /* _FREERTOS_DRIVER_H */
