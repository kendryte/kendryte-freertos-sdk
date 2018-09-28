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

#include <FreeRTOS.h>
#include <semphr.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef uintptr_t handle_t;

typedef struct tag_driver_base
{
    void *userdata;
    void (*install)(void *userdata);
    int (*open)(void* userdata);
    void (*close)(void* userdata);
} driver_base_t;

typedef enum
{
    DRIVER_UART,
    DRIVER_GPIO,
    DRIVER_I2C,
    DRIVER_I2C_DEVICE,
    DRIVER_I2S,
    DRIVER_SPI,
    DRIVER_SPI_DEVICE,
    DRIVER_DVP,
    DRIVER_SCCB,
    DRIVER_SCCB_DEVICE,
    DRIVER_FFT,
    DRIVER_AES,
    DRIVER_SHA256,
    DRIVER_TIMER,
    DRIVER_PWM,
    DRIVER_WDT,
    DRIVER_RTC,
    DRIVER_PIC,
    DRIVER_DMAC,
    DRIVER_DMA,
    DRIVER_CUSTOM
} driver_type;

typedef struct tag_driver_registry
{
    const char *name;
    const void *driver;
    driver_type type;
} driver_registry_t;

typedef enum _uart_stopbits
{
    UART_STOP_1,
    UART_STOP_1_5,
    UART_STOP_2
} uart_stopbits_t;

typedef enum _uart_parity
{
    UART_PARITY_NONE,
    UART_PARITY_ODD,
    UART_PARITY_EVEN
} uart_parity_t;

typedef struct tag_uart_driver
{
    driver_base_t base;
    void (*config)(uint32_t baud_rate, uint32_t databits, uart_stopbits_t stopbits, uart_parity_t parity, void *userdata);
    int (*read)(char *buffer, size_t len, void *userdata);
    int (*write)(const char *buffer, size_t len, void *userdata);
} uart_driver_t;

typedef enum _gpio_drive_mode
{
    GPIO_DM_INPUT,
    GPIO_DM_INPUT_PULL_DOWN,
    GPIO_DM_INPUT_PULL_UP,
    GPIO_DM_OUTPUT
} gpio_drive_mode_t;

typedef enum _gpio_pin_edge
{
    GPIO_PE_NONE,
    GPIO_PE_FALLING,
    GPIO_PE_RISING,
    GPIO_PE_BOTH
} gpio_pin_edge_t;

typedef enum _gpio_pin_value
{
    GPIO_PV_LOW,
    GPIO_PV_HIGH
} gpio_pin_value_t;

typedef void (*gpio_on_changed_t)(uint32_t pin, void *userdata);

typedef struct tag_gpio_driver
{
    driver_base_t base;
    uint32_t pin_count;
    void (*set_drive_mode)(void *userdata, uint32_t pin, gpio_drive_mode_t mode);
    void (*set_pin_edge)(void *userdata, uint32_t pin, gpio_pin_edge_t edge);
    void (*set_on_changed)(void *userdata, uint32_t pin, gpio_on_changed_t callback, void *callback_data);
    void (*set_pin_value)(void *userdata, uint32_t pin, gpio_pin_value_t value);
    gpio_pin_value_t (*get_pin_value)(void *userdata, uint32_t pin);
} gpio_driver_t;

typedef enum
{
    I2C_BS_STANDARD
} i2c_bus_speed_mode_t;

typedef struct tag_i2c_device_driver
{
    driver_base_t base;
    int (*read)(char *buffer, size_t len, void *userdata);
    int (*write)(const char *buffer, size_t len, void *userdata);
    int (*transfer_sequential)(const char *write_buffer, size_t write_len, char *read_buffer, size_t read_len, void *userdata);
} i2c_device_driver_t;

typedef enum _i2c_event
{
    I2C_EV_START,
    I2C_EV_RESTART,
    I2C_EV_STOP
} i2c_event_t;

typedef struct _i2c_slave_handler
{
    void (*on_receive)(uint32_t data);
    uint32_t (*on_transmit)();
    void (*on_event)(i2c_event_t event);
} i2c_slave_handler_t;

typedef struct tag_i2c_driver
{
    driver_base_t base;
    i2c_device_driver_t * (*get_device)(uint32_t slave_address, uint32_t address_width, i2c_bus_speed_mode_t bus_speed_mode, void *userdata);
    void (*config_as_slave)(uint32_t slave_address, uint32_t address_width, i2c_bus_speed_mode_t bus_speed_mode, i2c_slave_handler_t *handler, void *userdata);
} i2c_driver_t;

typedef enum _audio_format_type
{
    AUDIO_FMT_PCM
} audio_format_type_t;

typedef struct _audio_format
{
    audio_format_type_t type;
    uint32_t bits_per_sample;
    uint32_t sample_rate;
    uint32_t channels;
} audio_format_t;

typedef enum _i2s_align_mode
{
    I2S_AM_STANDARD,
    I2S_AM_RIGHT,
    I2S_AM_LEFT
} i2s_align_mode_t;

typedef struct tag_i2s_driver
{
    driver_base_t base;
    void (*config_as_render)(const audio_format_t *format, size_t delay_ms, i2s_align_mode_t align_mode, size_t channels_mask, void *userdata);
    void (*config_as_capture)(const audio_format_t *format, size_t delay_ms, i2s_align_mode_t align_mode, size_t channels_mask, void *userdata);
    void (*get_buffer)(char **buffer, size_t *frames, void *userdata);
    void (*release_buffer)(size_t frames, void *userdata);
    void (*start)(void *userdata);
    void (*stop)(void *userdata);
} i2s_driver_t;

typedef enum _spi_mode
{
    SPI_MODE_0,
    SPI_MODE_1,
    SPI_MODE_2,
    SPI_MODE_3,
} spi_mode_t;

typedef enum _spi_frame_format
{
    SPI_FF_STANDARD,
    SPI_FF_DUAL,
    SPI_FF_QUAD,
    SPI_FF_OCTAL
} spi_frame_format_t;

typedef enum _spi_inst_addr_trans_mode
{
    SPI_AITM_STANDARD,
    SPI_AITM_ADDR_STANDARD,
    SPI_AITM_AS_FRAME_FORMAT
} spi_inst_addr_trans_mode_t;

typedef struct tag_spi_device_driver
{
    driver_base_t base;
    void (*config)(uint32_t instruction_length, uint32_t address_length, uint32_t wait_cycles, spi_inst_addr_trans_mode_t trans_mode, void *userdata);
    double (*set_clock_rate)(double clock_rate, void *userdata);
    int (*read)(char *buffer, size_t len, void *userdata);
    int (*write)(const char *buffer, size_t len, void *userdata);
    int (*transfer_full_duplex)(const char *write_buffer, size_t write_len, char *read_buffer, size_t read_len, void *userdata);
    int (*transfer_sequential)(const char *write_buffer, size_t write_len, char *read_buffer, size_t read_len, void *userdata);
    void (*fill)(uint32_t instruction, uint32_t address, uint32_t value, size_t count, void *userdata);
} spi_device_driver_t;

typedef struct tag_spi_driver
{
    driver_base_t base;
    spi_device_driver_t * (*get_device)(spi_mode_t mode, spi_frame_format_t frame_format, uint32_t chip_select_mask, uint32_t data_bit_length, void *userdata);
} spi_driver_t;

typedef enum _video_format
{
    VIDEO_FMT_RGB565,
    VIDEO_FMT_RGB24_PLANAR
} video_format_t;

typedef enum _video_frame_event
{
    VIDEO_FE_BEGIN,
    VIDEO_FE_END
} dvp_frame_event_t;

typedef enum _dvp_signal_type
{
    DVP_SIG_POWER_DOWN,
    DVP_SIG_RESET
} dvp_signal_type_t;

typedef void (*dvp_on_frame_event_t)(dvp_frame_event_t event, void* userdata);

typedef struct tag_dvp_driver
{
    driver_base_t base;
    uint32_t output_num;
    void (*config)(uint32_t width, uint32_t height, int auto_enable, void* userdata);
    void (*enable_frame)(void* userdata);
    void (*set_signal)(dvp_signal_type_t type, int value, void* userdata);
    void (*set_output_enable)(uint32_t index, int enable, void* userdata);
    void (*set_output_attributes)(uint32_t index, video_format_t format, void* output_buffer, void* userdata);
    void (*set_frame_event_enable)(dvp_frame_event_t event, int enable, void* userdata);
    void (*set_on_frame_event)(dvp_on_frame_event_t callback, void* callback_data, void* userdata);
} dvp_driver_t;

typedef struct tag_sccb_device_driver
{
    driver_base_t base;
    uint8_t(*read_byte)(uint16_t reg_address, void* userdata);
    void(*write_byte)(uint16_t reg_address, uint8_t value, void* userdata);
} sccb_device_driver_t;

typedef struct tag_sccb_driver
{
    driver_base_t base;
    sccb_device_driver_t* (*get_device)(uint32_t slave_address, uint32_t address_width, void* userdata);
} sccb_driver_t;

typedef struct
{
    int16_t I1;
    int16_t R1;
    int16_t I2;
    int16_t R2;
} fft_data;

typedef enum
{
    FFT_512,
    FFT_256,
    FFT_128,
    FFT_64
} fft_point;

typedef enum
{
    FFT_DIR_BACKWARD,
    FFT_DIR_FORWARD
} fft_direction;

typedef struct tag_fft_driver
{
    driver_base_t base;
    void (*complex_uint16)(fft_point point, fft_direction direction, uint32_t shifts_mask, const uint16_t* input, uint16_t* output, void* userdata);
} fft_driver_t;

typedef enum
{
    AES_CIPHER_ECB = 0,
    AES_CIPHER_CBC = 1,
    AES_CIPHER_GCM = 2
} aes_cipher_mod;

typedef enum
{
    AES_128 = 16,
    AES_192 = 24,
    AES_256 = 32,
} aes_kmode;

typedef enum
{
    AES_MODE_ENCRYPTION = 0,
    AES_MODE_DECRYPTION = 1,
} aes_encrypt_sel;

typedef struct tag_aes_parameter
{
    uint8_t* aes_in_data;
    uint8_t* key_addr;
    uint8_t key_length;
    uint8_t* gcm_iv;
    uint8_t iv_length;
    uint8_t* aes_aad;
    uint32_t add_size;
    aes_cipher_mod cipher_mod;
    uint32_t data_size;
    uint8_t* aes_out_data;
    uint8_t* tag;
} aes_parameter;

typedef struct tag_aes_driver
{
    driver_base_t base;
    void (*decrypt)(aes_parameter* aes_param, void* userdata);
    void (*encrypt)(aes_parameter* aes_param, void* userdata);
} aes_driver_t;

typedef struct tag_sha256_driver
{
    driver_base_t base;
    void (*sha_str)(const char* str, size_t length, uint8_t* hash, void* userdata);
} sha256_driver_t;

typedef void (*timer_on_tick_t)(void* userdata);

typedef struct tag_timer_driver
{
    driver_base_t base;
    size_t (*set_interval)(size_t nanoseconds, void* userdata);
    void (*set_on_tick)(timer_on_tick_t on_tick, void* ontick_data, void* userdata);
    void (*set_enable)(int enable, void* userdata);
} timer_driver_t;

typedef struct tag_pwm_driver
{
    driver_base_t base;
    uint32_t pin_count;
    double (*set_frequency)(double frequency, void* userdata);
    double (*set_active_duty_cycle_percentage)(uint32_t pin, double duty_cycle_percentage, void* userdata);
    void (*set_enable)(uint32_t pin, int enable, void* userdata);
} pwm_driver_t;

typedef enum _wdt_response_mode
{
    WDT_RESP_RESET,
    WDT_RESP_INTERRUPT
} wdt_response_mode_t;

typedef int (*wdt_on_timeout_t)(void *userdata);

typedef struct tag_wdt_driver
{
    driver_base_t base;
    void (*set_response_mode)(wdt_response_mode_t mode, void *userdata);
    size_t (*set_timeout)(size_t nanoseconds, void *userdata);
    void (*set_on_timeout)(wdt_on_timeout_t handler, void *handler_userdata, void *userdata);
    void (*restart_counter)(void *userdata);
    void (*set_enable)(int enable, void *userdata);
} wdt_driver_t;

typedef struct tag_rtc_driver
{
    driver_base_t base;
    void (*get_datetime)(struct tm *datetime, void *userdata);
    void (*set_datetime)(const struct tm *datetime, void *userdata);
} rtc_driver_t;

typedef struct tag_custom_driver
{
    driver_base_t base;
    int (*io_control)(size_t control_code, const char* write_buffer, size_t write_len, char* read_buffer, size_t read_len, void* userdata);
} custom_driver_t;

/* ===== internal drivers ======*/

typedef void (*pic_irq_handler)(void* userdata);
void kernel_iface_pic_on_irq(size_t irq);

typedef struct tag_pic_driver
{
    driver_base_t base;
    void (*set_irq_enable)(size_t irq, int enable, void* userdata);
    void (*set_irq_priority)(size_t irq, size_t priority, void* userdata);
} pic_driver_t;

typedef struct tag_dmac_driver
{
    driver_base_t base;
} dmac_driver_t;

typedef void (*dma_stage_completion_handler)(void* userdata);

typedef struct tag_dma_driver
{
    driver_base_t base;
    void (*set_select_request)(uint32_t request, void* userdata);
    void (*config)(uint32_t priority, void* userdata);
    void (*transmit_async)(const volatile void* src, volatile void* dest, int src_inc, int dest_inc, size_t element_size, size_t count, size_t burst_size, SemaphoreHandle_t completion_event, void* userdata);
    void (*loop_async)(const volatile void** srcs, size_t src_num, volatile void** dests, size_t dest_num, int src_inc, int dest_inc, size_t element_size, size_t count, size_t burst_size, dma_stage_completion_handler stage_completion_handler, void* stage_completion_handler_data, SemaphoreHandle_t completion_event, int* stop_signal, void* userdata);
} dma_driver_t;

extern driver_registry_t g_hal_drivers[];
extern driver_registry_t g_dma_drivers[];
extern driver_registry_t g_system_drivers[];

#ifdef __cplusplus
}
#endif

#endif /* _FREERTOS_DRIVER_H */
