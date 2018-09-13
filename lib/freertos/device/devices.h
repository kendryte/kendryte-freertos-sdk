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
#ifndef _FREERTOS_DEVICES_H
#define _FREERTOS_DEVICES_H

#include <driver.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief       Install all drivers
 */
void install_drivers();

/**
 * @brief       Open a device
 *
 * @param[in]   name        The device path
 *
 * @return      result
       - 0      Fail
       - other  The device handle
 */
uintptr_t io_open(const char* name);

/**
 * @brief       Close a device
 *
 * @param[in]   file        The device handle
 *
 * @return      result
 *     - 0      Success
 *     - other  Fail
 */
int io_close(uintptr_t file);

/**
 * @brief       Read from a device
 *
 * @param[in[   file        The device handle
 * @param[out]  buffer      The destination buffer
 * @param[in]   len     Maximum bytes to read
 *
 * @return      Actual bytes read
 */
int io_read(uintptr_t file, char* buffer, size_t len);

/**
 * @brief       Write to a device
 *
 * @param[in]   file        The device handle
 * @param[in]   buffer      The source buffer
 * @param[in]   len         Bytes to write
 *
 * @return      result
 *     - len    Success
 *     - other  Fail
 */
int io_write(uintptr_t file, const char* buffer, size_t len);

/**
 * @brief       Send control info to a device
 *
 * @param[in]   file                The device handle
 * @param[in]   control_code        The control code
 * @param[in]   write_buffer        The source buffer
 * @param[in]   write_len           Bytes to write
 * @param[in]   read_buffer         The destination buffer
 * @param[in]   read_len            Maximum bytes to read
 *
 * @return      Actual bytes read
 */
int io_control(uintptr_t file, size_t control_code, const char* write_buffer, size_t write_len, char* read_buffer, size_t read_len);

/**
 * @brief       Configure a UART device
 *
 * @param[in]   file            The UART handle
 * @param[in]   baud_rate       The baud rate
 * @param[in]   data_width      The databits width
 * @param[in]   stopbit         The stopbit selection
 * @param[in]   parity          The parity selection
 */
void uart_config(uintptr_t file, size_t baud_rate, size_t data_width, uart_stopbit stopbit, uart_parity parity);

/**
 * @brief       Get the pin count of a GPIO controller
 *
 * @param[in]   file        The GPIO controller handle
 *
 * @return      The pin count
 */
size_t gpio_get_pin_count(uintptr_t file);

/**
 * @brief       Set the drive mode of a GPIO pin
 *
 * @param[in]   file        The GPIO controller handle
 * @param[in]   pin         The GPIO pin
 * @param[in]   stopbit     The drive mode selection
 */
void gpio_set_drive_mode(uintptr_t file, size_t pin, gpio_drive_mode mode);

/**
 * @brief       Set the edge trigger mode of a GPIO pin
 *
 * @param[in]   file        The GPIO controller handle
 * @param[in]   pin         The GPIO pin
 * @param[in]   stopbit     The edge trigger mode selection
 */
void gpio_set_pin_edge(uintptr_t file, size_t pin, gpio_pin_edge edge);

/**
 * @brief       Set the changed handler of a GPIO pin
 *
 * @param[in]   file            The GPIO controller handle
 * @param[in]   pin             The GPIO pin
 * @param[in]   callback        The changed handler
 * @param[in]   userdata        The userdata of the handler
 */
void gpio_set_onchanged(uintptr_t file, size_t pin, gpio_onchanged callback, void* userdata);

/**
 * @brief       Get the value of a GPIO pin
 *
 * @param[in]   file        The GPIO controller handle
 * @param[in]   pin         The GPIO pin
 *
 * @return      The value of the pin
 */
gpio_pin_value gpio_get_pin_value(uintptr_t file, size_t pin);

/**
 * @brief       Set the value of a GPIO pin
 *
 * @param[in]   file        The GPIO controller handle
 * @param[in]   pin         The GPIO pin
 * @param[in]   value       The value to be set
 */
void gpio_set_pin_value(uintptr_t file, size_t pin, gpio_pin_value value);

/**
 * @brief       Register and open a I2C device
 *
 * @param[in]   file                The I2C controller handle
 * @param[in]   name                Specify the path to access the device
 * @param[in]   slave_address       The address of slave
 * @param[in]   address_width       The bits width of address
 * @param[in]   bus_speed_mode      The bus speed mode selection
 *
 * @return      The I2C device handle
 */
uintptr_t i2c_get_device(uintptr_t file, const char* name, size_t slave_address, size_t address_width, i2c_bus_speed_mode bus_speed_mode);

/**
 * @brief       Write to then read from a I2C device
 *
 * @param[in]   file                The I2C device handle
 * @param[in]   write_buffer        The source buffer
 * @param[in]   write_len           Bytes to write
 * @param[in]   read_buffer         The destination buffer
 * @param[in]   read_len            Maximum bytes to read
 *
 * @return      Actual bytes read
 */
int i2c_dev_transfer_sequential(uintptr_t file, const char* write_buffer, size_t write_len, char* read_buffer, size_t read_len);

/**
 * @brief       Configure a I2C controller with slave mode
 *
 * @param[in]   file                The I2C controller handle
 * @param[in]   slave_address       The address of slave
 * @param[in]   address_width       The bits width of address
 * @param[in]   bus_speed_mode      The bus speed mode selection
 * @param[in]   handler             The slave handler
 */
void i2c_config_as_slave(uintptr_t file, size_t slave_address, size_t address_width, i2c_bus_speed_mode bus_speed_mode, i2c_slave_handler* handler);

/**
 * @brief       Configure a I2S controller with render mode
 *
 * @param[in]   file                The I2S controller handle
 * @param[in]   format              The audio format
 * @param[in]   delay_ms            The buffer length in milliseconds
 * @param[in]   align_mode          The I2S align mode selection
 * @param[in]   channels_mask       The channels selection mask
 */
void i2s_config_as_render(uintptr_t file, const audio_format_t* format, size_t delay_ms, i2s_align_mode align_mode, size_t channels_mask);

/**
 * @brief       Configure a I2S controller with render mode
 *
 * @param[in]   file                The I2S controller handle
 * @param[in]   format              The audio format
 * @param[in]   delay_ms            The buffer length in milliseconds
 * @param[in]   align_mode          The I2S align mode selection
 * @param[in]   channels_mask       The channels selection mask
 */
void i2s_config_as_capture(uintptr_t file, const audio_format_t* format, size_t delay_ms, i2s_align_mode align_mode, size_t channels_mask);

/**
 * @brief       Get the audio buffer of a I2S controller
 *
 * @param[in]   file        The I2S controller handle
 * @param[out]  buffer      The address of audio buffer
 * @param[out]  frames      The available frames count in buffer
 */
void i2s_get_buffer(uintptr_t file, char** buffer, size_t* frames);

/**
 * @brief       Release the audio buffer of a I2S controller
 *
 * @param[in]   file        The I2S controller handle
 * @param[out]  frames      The frames have been confirmed read or written
 */
void i2s_release_buffer(uintptr_t file, size_t frames);

/**
 * @brief       Start rendering or recording of a I2S controller
 *
 * @param[in]   file        The I2S controller handle
 */
void i2s_start(uintptr_t file);

/**
 * @brief       Stop rendering or recording of a I2S controller
 *
 * @param[in]   file        The I2S controller handle
 */
void i2s_stop(uintptr_t file);

/**
 * @brief       Register and open a SPI device
 *
 * @param[in]   file                    The SPI controller handle
 * @param[in]   name                    Specify the path to access the device
 * @param[in]   mode                    The SPI mode selection
 * @param[in]   frame_format            The SPI frame format selection
 * @param[in]   chip_select_line        The CS mask
 * @param[in]   data_bit_length         The length of data bits
 *
 * @return      The SPI device handle
 */
uintptr_t spi_get_device(uintptr_t file, const char* name, spi_mode mode, spi_frame_format frame_format, size_t chip_select_line, size_t data_bit_length);

/**
 * @brief       Configure a SPI device with non-standard mode
 *
 * @param[in]   file                            The SPI device handle
 * @param[in]   instruction_length              The length of instruction
 * @param[in]   address_length                  The length of address
 * @param[in]   wait_cycles                     The wait cycles
 * @param[in]   spi_addr_inst_trans_mode        The transmition mode of address and instruction
 */
void spi_dev_config(uintptr_t file, size_t instruction_length, size_t address_length, size_t wait_cycles, spi_addr_inst_trans_mode trans_mode);

/**
 * @brief       Set the speed of a SPI device
 *
 * @param[in]   file        The SPI device handle
 * @param[in]   speed       The desired speed in Hz
 *
 * @return      The actual speed after set
 */
double spi_dev_set_speed(uintptr_t file, double speed);

/**
 * @brief       Transfer data between a SPI device using full duplex
 *
 * @param[in]   file                The SPI device handle
 * @param[in]   write_buffer        The source buffer
 * @param[in]   write_len           Bytes to write
 * @param[in]   read_buffer         The destination buffer
 * @param[in]   read_len            Maximum bytes to read
 *
 * @return      Actual bytes read
 */
int spi_dev_transfer_full_duplex(uintptr_t file, const char* write_buffer, size_t write_len, char* read_buffer, size_t read_len);

/**
 * @brief       Write to then read from a SPI device
 *
 * @param[in]   file                The SPI device handle
 * @param[in]   write_buffer        The source buffer
 * @param[in]   write_len           Bytes to write
 * @param[in]   read_buffer         The destination buffer
 * @param[in]   read_len            Maximum bytes to read
 *
 * @return      Actual bytes read
 */
int spi_dev_transfer_sequential(uintptr_t file, const char* write_buffer, size_t write_len, char* read_buffer, size_t read_len);

/**
 * @brief       Fill a sequence of idential frame to a SPI device
 *
 * @param[in]   file            The SPI device handle
 * @param[in]   instruction     The instruction
 * @param[in]   file            The SPI device handle
 * @param[in]   address         The address
 * @param[in]   value           The value
 * @param[in]   count           THe count of frames
 */
void spi_dev_fill(uintptr_t file, size_t instruction, size_t address, uint32_t value, size_t count);

/**
 * @brief       Configure a DVP device
 *
 * @param[in]   file            The DVP device handle
 * @param[in]   width           The frame width
 * @param[in]   height          The frame height
 * @param[in]   auto_enable     Process frames automatically
 */
void dvp_config(uintptr_t file, size_t width, size_t height, int auto_enable);

/**
 * @brief       Enable to process of current frame
 *
 * @param[in]   file    The DVP device handle
 */
void dvp_enable_frame(uintptr_t file);

/**
 * @brief       Get the count of outputs of a DVP device
 *
 * @param[in]   file        The DVP device handle
 *
 * @return      The count of outputs
 */
size_t dvp_get_output_num(uintptr_t file);

/**
 * @brief       Set or unset a signal to a DVP device
 *
 * @param[in]   file        The DVP device handle
 * @param[in]   type        The signal type
 * @param[in]   value       1 is set, 0 is unset
 */
void dvp_set_signal(uintptr_t file, dvp_signal_type type, int value);

/**
 * @brief       Enable or disable a output of a DVP device
 *
 * @param[in]   file        The DVP device handle
 * @param[in]   index       The output index
 * @param[in]   enable      1 is enable, 0 is disable
 */
void dvp_set_output_enable(uintptr_t file, size_t index, int enable);

/**
 * @brief       Set output attributes of a DVP device
 *
 * @param[in]   file                The DVP device handle
 * @param[in]   index               The output index
 * @param[in]   format              The output format
 * @param[out]  output_buffer       The output buffer
 */
void dvp_set_output_attributes(uintptr_t file, size_t index, video_format format, void* output_buffer);

/**
 * @brief       Enable or disable a frame event of a DVP device
 *
 * @param[in]   file        The DVP device handle
 * @param[in]   event       The frame event
 * @param[in]   enable      1 is enable, 0 is disable
 */
void dvp_set_frame_event_enable(uintptr_t file, video_frame_event event, int enable);

/**
 * @brief       Set the frame event handler of a DVP device
 *
 * @param[in]   file                The DVP device handle
 * @param[in]   callback            The event handler
 * @param[in]   callback_data       The userdata of the event handler
 */
void dvp_set_on_frame_event(uintptr_t file, dvp_on_frame_event callback, void* callback_data);

/**
 * @brief       Register and open a SCCB device
 *
 * @param[in]   file                The SCCB controller handle
 * @param[in]   name                Specify the path to access the device
 * @param[in]   slave_address       The address of slave
 * @param[in]   address_width       The bits width of address
 *
 * @return      The SCCB device handle
 */
uintptr_t sccb_get_device(uintptr_t file, const char* name, size_t slave_address, size_t address_width);

/**
 * @brief       Read a byte from a SCCB device
 *
 * @param[in]   file            The SCCB controller handle
 * @param[in]   reg_address     The register address
 *
 * @return      The byte read
 */
uint8_t sccb_dev_read_byte(uintptr_t file, uint16_t reg_address);

/**
 * @brief       Write a byte to a SCCB device
 *
 * @param[in]   file            The SCCB controller handle
 * @param[in]   reg_address     The register address
 * @param[in]   value           The data byte
 */
void sccb_dev_write_byte(uintptr_t file, uint16_t reg_address, uint8_t value);

/**
 * @brief       Do 16bit quantized complex FFT
 *
 * @param[in]   point           The FFT points count
 * @param[in]   direction       The direction
 * @param[in]   shifts_mask     The shifts selection in 9 stage
 * @param[in]   input           The input data
 * @param[out]  output          The output data
 */
void fft_complex_uint16(fft_point point, fft_direction direction, uint32_t shifts_mask, const uint16_t* input, uint16_t* output);

/**
* @brief       Do aes decrypt
*
* @param[in]   aes_in_data      The aes input decrypt data
* @param[in]   key_addr         The aes key address
* @param[in]   key_length       The aes key length.16:AES_128 24:AES_192 32:AES_256
* @param[in]   gcm_iv           The gcm iv address
* @param[in]   iv_length        The gcm iv length
* @param[in]   aes_aad          The gcm add address
* @param[in]   add_size         The gcm add length
* @param[in]   cipher_mod       The cipher mode. 00:AES_CIPHER_ECB 01:AES_CIPHER_CBC 10:AES_CIPHER_GCM
* @param[in]   data_size        The input data size
* @param[out]  aes_out_data     The output data
* @param[out]  tag              The gcm output tag
*/
void aes_decrypt(aes_parameter* aes_param);

/**
 * @brief       Do aes encrypt
 *
 * @param[in]   aes_in_data         The aes input decrypt data
 * @param[in]   key_addr            The aes key address
 * @param[in]   key_length          The aes key length.16:AES_128 24:AES_192 32:AES_256
 * @param[in]   gcm_iv              The gcm iv address
 * @param[in]   iv_length           The gcm iv length
 * @param[in]   aes_aad             The gcm add address
 * @param[in]   add_size            The gcm add length
 * @param[in]   cipher_mod          The cipher mode. 00:AES_CIPHER_ECB 01:AES_CIPHER_CBC 10:AES_CIPHER_GCM
 * @param[in]   data_size           The input data size
 * @param[out]  aes_out_data        The output data
 * @param[out]  tag                 The output tag
 */
void aes_encrypt(aes_parameter* aes_param);

/**
 * @brief       Do sha256
 *
 * @param[in]   str         The sha256 string
 * @param[in]   length      The string length
 * @param[out]  hash        The sha256 result
 */
void sha256_str(const char* str, size_t length, uint8_t* hash);

/**
 * @brief       Set the interval of a TIMER device
 *
 * @param[in]   file            The TIMER controller handle
 * @param[in]   nanoseconds     The desired interval in nanoseconds
 *
 * @return      The actual interval
 */
size_t timer_set_interval(uintptr_t file, size_t nanoseconds);

/**
 * @brief       Set the tick handler of a TIMER device
 *
 * @param[in]   file            The TIMER controller handle
 * @param[in]   ontick          The tick handler
 * @param[in]   ontick_data     The userdata of the handler
 */
void timer_set_ontick(uintptr_t file, timer_ontick ontick, void* ontick_data);

/**
 * @brief       Enable or disable a TIMER device
 *
 * @param[in]   file        The TIMER controller handle
 * @param[in]   enable      1 is enable, 0 is disable
 */
void timer_set_enable(uintptr_t file, int enable);

/**
 * @brief       Get the pin count of a PWM controller
 *
 * @param[in]   file        The PWM controller handle
 *
 * @return      The pin count
 */
size_t pwm_get_pin_count(uintptr_t file);

/**
 * @brief       Set the frequency of a PWM controller
 *
 * @param[in]   file            The PWM controller handle
 * @param[in]   frequency       The desired frequency in Hz
 *
 * @return      The actual frequency after set
 */
double pwm_set_frequency(uintptr_t file, double frequency);

/**
 * @brief       Set the active duty cycle percentage of a PWM pin
 *
 * @param[in]   file                        The PWM controller handle
 * @param[in]   pin                         The PWM pin
 * @param[in]   duty_cycle_percentage       The desired active duty cycle percentage
 *
 * @return      The actual active duty cycle percentage after set
 */
double pwm_set_active_duty_cycle_percentage(uintptr_t file, size_t pin, double duty_cycle_percentage);

/**
 * @brief       Enable or disable a PWM pin
 *
 * @param[in]   file        The PWM controller handle
 * @param[in]   pin         The PWM pin
 * @param[in]   enable      1 is enable, 0 is disable
 */
void pwm_set_enable(uintptr_t file, size_t pin, int enable);

/**
 * @brief       Get the datetime of a RTC device
 *
 * @param[in]   file            The RTC device
 * @param[out]  datetime        The datatime
 */
void rtc_get_datetime(uintptr_t file, datetime_t* datetime);

/**
 * @brief       Set the datetime of a RTC device
 *
 * @param[in]   file            The RTC device
 * @param[out]  datetime        The datatime to be set
 */
void rtc_set_datetime(uintptr_t file, const datetime_t* datetime);

#ifdef __cplusplus
}
#endif

#endif /* _FREERTOS_DEVICES_H */
