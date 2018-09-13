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

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct tag_driver_base
{
    void* userdata;
    void (*install)(void* userdata);
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
    DRIVER_RTC,
    DRIVER_PIC,
    DRIVER_DMAC,
    DRIVER_DMA,
    DRIVER_CUSTOM
} driver_type;

typedef struct tag_driver_registry
{
    const char* name;
    const void* driver;
    driver_type type;
} driver_registry_t;

typedef enum
{
    UART_STOP_1,
    UART_STOP_1_5,
    UART_STOP_2
} uart_stopbit;

typedef enum
{
    UART_PARITY_NONE,
    UART_PARITY_ODD,
    UART_PARITY_EVEN
} uart_parity;

typedef struct tag_uart_driver
{
    driver_base_t base;
    void (*config)(size_t baud_rate, size_t data_width, uart_stopbit stopbit, uart_parity parity, void* userdata);
    int (*read)(char* buffer, size_t len, void* userdata);
    int (*write)(const char* buffer, size_t len, void* userdata);
} uart_driver_t;

typedef enum
{
    GPIO_DM_INPUT,
    GPIO_DM_INPUT_PULL_DOWN,
    GPIO_DM_INPUT_PULL_UP,
    GPIO_DM_OUTPUT,
    GPIO_DM_OUTPUT_OPEN_DRAIN,
    GPIO_DM_OUTPUT_OPEN_DRAIN_PULL_UP,
    GPIO_DM_OUTPUT_OPEN_SOURCE,
    GPIO_DM_OUTPUT_OPEN_SOURCE_PULL_DOWN
} gpio_drive_mode;

typedef enum
{
    GPIO_PE_NONE,
    GPIO_PE_FALLING,
    GPIO_PE_RISING,
    GPIO_PE_BOTH
} gpio_pin_edge;

typedef enum
{
    GPIO_PV_LOW,
    GPIO_PV_HIGH
} gpio_pin_value;

typedef void (*gpio_onchanged)(size_t pin, void* userdata);

typedef struct tag_gpio_driver
{
    driver_base_t base;
    size_t pin_count;
    void (*set_drive_mode)(void* userdata, size_t pin, gpio_drive_mode mode);
    void (*set_pin_edge)(void* userdata, size_t pin, gpio_pin_edge edge);
    void (*set_onchanged)(void* userdata, size_t pin, gpio_onchanged callback, void* callback_data);
    void (*set_pin_value)(void* userdata, size_t pin, gpio_pin_value value);
    gpio_pin_value (*get_pin_value)(void* userdata, size_t pin);
} gpio_driver_t;

typedef enum
{
    I2C_BS_STANDARD,
    I2C_BS_FAST,
    I2C_BS_HIGH_SPEED
} i2c_bus_speed_mode;

typedef struct tag_i2c_device_driver
{
    driver_base_t base;
    int (*read)(char* buffer, size_t len, void* userdata);
    int (*write)(const char* buffer, size_t len, void* userdata);
    int (*transfer_sequential)(const char* write_buffer, size_t write_len, char* read_buffer, size_t read_len, void* userdata);
} i2c_device_driver_t;

typedef enum
{
    I2C_EV_START,
    I2C_EV_RESTART,
    I2C_EV_STOP
} i2c_event;

typedef struct
{
    void (*on_receive)(uint32_t data);
    uint32_t (*on_transmit)();
    void (*on_event)(i2c_event event);
} i2c_slave_handler;

typedef struct tag_i2c_driver
{
    driver_base_t base;
    i2c_device_driver_t* (*get_device)(size_t slave_address, size_t address_width, i2c_bus_speed_mode bus_speed_mode, void* userdata);
    void (*config_as_slave)(size_t slave_address, size_t address_width, i2c_bus_speed_mode bus_speed_mode, i2c_slave_handler* handler, void* userdata);
} i2c_driver_t;

typedef enum
{
    I2S_AM_STANDARD,
    I2S_AM_RIGHT,
    I2S_AM_LEFT
} i2s_align_mode;

typedef enum
{
    AUDIO_FMT_PCM
} audio_format_type;

typedef struct tag_audio_format
{
    audio_format_type type;
    size_t bits_per_sample;
    size_t sample_rate;
    size_t channels;
} audio_format_t;

typedef struct tag_i2s_driver
{
    driver_base_t base;
    void (*config_as_render)(const audio_format_t* format, size_t delay_ms, i2s_align_mode align_mode, size_t channels_mask, void* userdata);
    void (*config_as_capture)(const audio_format_t* format, size_t delay_ms, i2s_align_mode align_mode, size_t channels_mask, void* userdata);
    void (*get_buffer)(char** buffer, size_t* frames, void* userdata);
    void (*release_buffer)(size_t frames, void* userdata);
    void (*start)(void* userdata);
    void (*stop)(void* userdata);
} i2s_driver_t;

typedef enum
{
    SPI_Mode_0,
    SPI_Mode_1,
    SPI_Mode_2,
    SPI_Mode_3,
} spi_mode;

typedef enum
{
    SPI_FF_STANDARD,
    SPI_FF_DUAL,
    SPI_FF_QUAD,
    SPI_FF_OCTAL
} spi_frame_format;

typedef enum
{
    SPI_AITM_STANDARD,
    SPI_AITM_ADDR_STANDARD,
    SPI_AITM_AS_FRAME_FORMAT
} spi_addr_inst_trans_mode;

typedef struct tag_spi_device_driver
{
    driver_base_t base;
    void (*config)(size_t instruction_length, size_t address_length, size_t wait_cycles, spi_addr_inst_trans_mode trans_mode, void* userdata);
    double (*set_speed)(double speed, void* userdata);
    int (*read)(char* buffer, size_t len, void* userdata);
    int (*write)(const char* buffer, size_t len, void* userdata);
    int (*transfer_full_duplex)(const char* write_buffer, size_t write_len, char* read_buffer, size_t read_len, void* userdata);
    int (*transfer_sequential)(const char* write_buffer, size_t write_len, char* read_buffer, size_t read_len, void* userdata);
    void (*fill)(size_t instruction, size_t address, uint32_t value, size_t count, void* userdata);
} spi_device_driver_t;

typedef struct tag_spi_driver
{
    driver_base_t base;
    spi_device_driver_t* (*get_device)(spi_mode mode, spi_frame_format frame_format, size_t chip_select_line, size_t data_bit_length, void* userdata);
} spi_driver_t;

typedef enum
{
    VIDEO_FMT_RGB565,
    VIDEO_FMT_RGB24Planar
} video_format;

typedef struct tag_sccb_device_driver
{
    driver_base_t base;
    uint8_t (*read_byte)(uint16_t reg_address, void* userdata);
    void (*write_byte)(uint16_t reg_address, uint8_t value, void* userdata);
} sccb_device_driver_t;

typedef struct tag_sccb_driver
{
    driver_base_t base;
    sccb_device_driver_t* (*get_device)(size_t slave_address, size_t address_width, void* userdata);
} sccb_driver_t;

typedef enum
{
    VIDEO_FE_BEGIN,
    VIDEO_FE_END
} video_frame_event;

typedef enum
{
    DVP_SIG_POWER_DOWN,
    DVP_SIG_RESET
} dvp_signal_type;

typedef void (*dvp_on_frame_event)(video_frame_event event, void* userdata);

typedef struct tag_dvp_driver
{
    driver_base_t base;
    size_t output_num;
    void (*config)(size_t width, size_t height, int auto_enable, void* userdata);
    void (*enable_frame)(void* userdata);
    void (*set_signal)(dvp_signal_type type, int value, void* userdata);
    void (*set_output_enable)(size_t index, int enable, void* userdata);
    void (*set_output_attributes)(size_t index, video_format format, void* output_buffer, void* userdata);
    void (*set_frame_event_enable)(video_frame_event event, int enable, void* userdata);
    void (*set_on_frame_event)(dvp_on_frame_event callback, void* callback_data, void* userdata);
} dvp_driver_t;

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

typedef void (*timer_ontick)(void* userdata);

typedef struct tag_timer_driver
{
    driver_base_t base;
    size_t (*set_interval)(size_t nanoseconds, void* userdata);
    void (*set_ontick)(timer_ontick ontick, void* ontick_data, void* userdata);
    void (*set_enable)(int enable, void* userdata);
} timer_driver_t;

typedef struct tag_pwm_driver
{
    driver_base_t base;
    size_t pin_count;
    double (*set_frequency)(double frequency, void* userdata);
    double (*set_active_duty_cycle_percentage)(size_t pin, double duty_cycle_percentage, void* userdata);
    void (*set_enable)(size_t pin, int enable, void* userdata);
} pwm_driver_t;

typedef struct tag_datetime
{
    uint32_t year;
    uint32_t month;
    uint32_t day;
    uint32_t hour;
    uint32_t minute;
    uint32_t second;
} datetime_t;

typedef struct tag_rtc_driver
{
    driver_base_t base;
    void (*get_datetime)(datetime_t* datetime, void* userdata);
    void (*set_datetime)(const datetime_t* datetime, void* userdata);
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
