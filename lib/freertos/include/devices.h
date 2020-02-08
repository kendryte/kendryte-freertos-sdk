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

#include <stddef.h>
#include <stdint.h>
#include "osdefs.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief       Open a device
 *
 * @param[in]   name        The device path
 *
 * @return      result
 *     - 0      Fail
 *     - other  The device handle
 */
handle_t io_open(const char *name);

/**
 * @brief       Close a device
 *
 * @param[in]   file        The device handle
 *
 * @return      result
 *     - 0      Success
 *     - other  Fail
 */
int io_close(handle_t file);

/**
 * @brief       Read from a device
 *
 * @param[in[   file        The device handle
 * @param[out]  buffer      The destination buffer
 * @param[in]   len         Maximum bytes to read
 *
 * @return      Actual bytes read
 */
int io_read(handle_t file, uint8_t *buffer, size_t len);

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
int io_write(handle_t file, const uint8_t *buffer, size_t len);

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
int io_control(handle_t file, uint32_t control_code, const uint8_t *write_buffer, size_t write_len, uint8_t *read_buffer, size_t read_len);

/**
 * @brief       Configure a UART device
 *
 * @param[in]   file            The UART handle
 * @param[in]   baud_rate       The baud rate
 * @param[in]   databits        The databits width (5-8)
 * @param[in]   stopbits        The stopbits selection
 * @param[in]   parity          The parity selection
 */
void uart_config(handle_t file, uint32_t baud_rate, uint32_t databits, uart_stopbits_t stopbits, uart_parity_t parity);

/**
 * @brief       Set uart read blocking time.
 *
 * @param[in]   file            The UART handle
 * @param[in]   millisecond     Blocking time
 *
 */
void uart_set_read_timeout(handle_t file, size_t millisecond);

/**
 * @brief       Get the pin count of a GPIO controller
 *
 * @param[in]   file        The GPIO controller handle
 *
 * @return      The pin count
 */
uint32_t gpio_get_pin_count(handle_t file);

/**
 * @brief       Set the drive mode of a GPIO pin
 *
 * @param[in]   file        The GPIO controller handle
 * @param[in]   pin         The GPIO pin
 * @param[in]   mode        The drive mode selection
 */
void gpio_set_drive_mode(handle_t file, uint32_t pin, gpio_drive_mode_t mode);

/**
 * @brief       Set the edge trigger mode of a GPIO pin
 *
 * @param[in]   file        The GPIO controller handle
 * @param[in]   pin         The GPIO pin
 * @param[in]   edge        The edge trigger mode selection
 */
void gpio_set_pin_edge(handle_t file, uint32_t pin, gpio_pin_edge_t edge);

/**
 * @brief       Set the changed handler of a GPIO pin
 *
 * @param[in]   file            The GPIO controller handle
 * @param[in]   pin             The GPIO pin
 * @param[in]   callback        The changed handler
 * @param[in]   userdata        The userdata of the handler
 */
void gpio_set_on_changed(handle_t file, uint32_t pin, gpio_on_changed_t callback, void *userdata);

/**
 * @brief       Get the value of a GPIO pin
 *
 * @param[in]   file        The GPIO controller handle
 * @param[in]   pin         The GPIO pin
 *
 * @return      The value of the pin
 */
gpio_pin_value_t gpio_get_pin_value(handle_t file, uint32_t pin);

/**
 * @brief       Set the value of a GPIO pin
 *
 * @param[in]   file        The GPIO controller handle
 * @param[in]   pin         The GPIO pin
 * @param[in]   value       The value to be set
 */
void gpio_set_pin_value(handle_t file, uint32_t pin, gpio_pin_value_t value);

/**
 * @brief       Register and open a I2C device
 *
 * @param[in]   file                The I2C controller handle
 * @param[in]   name                Specify the path to access the device
 * @param[in]   slave_address       The address of slave
 * @param[in]   address_width       The bits width of address
 *
 * @return      The I2C device handle
 */
handle_t i2c_get_device(handle_t file, uint32_t slave_address, uint32_t address_width);

/**
 * @brief       Set the clock rate of a I2C device
 *
 * @param[in]   file            The I2C device handle
 * @param[in]   clock_rate      The desired clock rate in Hz
 *
 * @return      The actual clock rate after set
 */
double i2c_dev_set_clock_rate(handle_t file, double clock_rate);

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
int i2c_dev_transfer_sequential(handle_t file, const uint8_t *write_buffer, size_t write_len, uint8_t *read_buffer, size_t read_len);

/**
 * @brief       Configure a I2C controller with slave mode
 *
 * @param[in]   file                The I2C controller handle
 * @param[in]   slave_address       The address of slave
 * @param[in]   address_width       The bits width of address
 * @param[in]   handler             The slave handler
 */
void i2c_config_as_slave(handle_t file, uint32_t slave_address, uint32_t address_width, i2c_slave_handler_t *handler);

/**
 * @brief       Set the clock rate of a slave I2C controller
 *
 * @param[in]   file            The I2C controller handle
 * @param[in]   clock_rate      The desired clock rate in Hz
 *
 * @return      The actual clock rate after set
 */
double i2c_slave_set_clock_rate(handle_t file, double clock_rate);

/**
 * @brief       Configure a I2S controller with render mode
 *
 * @param[in]   file                The I2S controller handle
 * @param[in]   format              The audio format
 * @param[in]   delay_ms            The buffer length in milliseconds
 * @param[in]   align_mode          The I2S align mode selection
 * @param[in]   channels_mask       The channels selection mask
 */
void i2s_config_as_render(handle_t file, const audio_format_t *format, size_t delay_ms, i2s_align_mode_t align_mode, size_t channels_mask);

/**
 * @brief       Configure a I2S controller with capture mode
 *
 * @param[in]   file                The I2S controller handle
 * @param[in]   format              The audio format
 * @param[in]   delay_ms            The buffer length in milliseconds
 * @param[in]   align_mode          The I2S align mode selection
 * @param[in]   channels_mask       The channels selection mask
 */
void i2s_config_as_capture(handle_t file, const audio_format_t *format, size_t delay_ms, i2s_align_mode_t align_mode, size_t channels_mask);

/**
 * @brief       Get the audio buffer of a I2S controller
 *
 * @param[in]   file        The I2S controller handle
 * @param[out]  buffer      The address of audio buffer
 * @param[out]  frames      The available frames count in buffer
 */
void i2s_get_buffer(handle_t file, uint8_t **buffer, size_t *frames);

/**
 * @brief       Release the audio buffer of a I2S controller
 *
 * @param[in]   file        The I2S controller handle
 * @param[out]  frames      The frames have been confirmed read or written
 */
void i2s_release_buffer(handle_t file, size_t frames);

/**
 * @brief       Start rendering or recording of a I2S controller
 *
 * @param[in]   file        The I2S controller handle
 */
void i2s_start(handle_t file);

/**
 * @brief       Stop rendering or recording of a I2S controller
 *
 * @param[in]   file        The I2S controller handle
 */
void i2s_stop(handle_t file);

/**
 * @brief       Set spi slave configuration
 *
 * @param[in]   file                The SPI controller handle
 * @param[in]   gpio_handle         The GPIO handle
 * @param[in]   int_pin             SPI master starts sending data interrupt.
 * @param[in]   ready_pin           SPI slave ready.
 * @param[in]   data_bit_length     Spi data bit length,suport 8/16/32 bit.
 * @param[in]   data                SPI slave device data buffer.
 * @param[in]   len                 The length of SPI slave device data buffer.
 * @param[in]   callback            Callback of spi slave.
 *
 * @return      Void
 */
void spi_slave_config(handle_t file, handle_t gpio_handle, uint8_t int_pin, uint8_t ready_pin, size_t data_bit_length, uint8_t *data, uint32_t len, spi_slave_receive_callback_t callback);

/**
 * @brief       Register and open a SPI device
 *
 * @param[in]   file                    The SPI controller handle
 * @param[in]   name                    Specify the path to access the device
 * @param[in]   mode                    The SPI mode selection
 * @param[in]   frame_format            The SPI frame format selection
 * @param[in]   chip_select_mask        The CS mask
 * @param[in]   data_bit_length         The length of data bits
 *
 * @return      The SPI device handle
 */
handle_t spi_get_device(handle_t file, spi_mode_t mode, spi_frame_format_t frame_format, uint32_t chip_select_mask, uint32_t data_bit_length);

/**
 * @brief       Configure a SPI device with non-standard mode
 *
 * @param[in]   file                    The SPI device handle
 * @param[in]   instruction_length      The length of instruction
 * @param[in]   address_length          The length of address
 * @param[in]   wait_cycles             The wait cycles
 * @param[in]   trans_mode              The transmition mode of instruction and address
 */
void spi_dev_config_non_standard(handle_t file, uint32_t instruction_length, uint32_t address_length, uint32_t wait_cycles, spi_inst_addr_trans_mode_t trans_mode);

/**
 * @brief       Set the clock rate of a SPI device
 *
 * @param[in]   file            The SPI device handle
 * @param[in]   clock_rate      The desired clock rate in Hz
 *
 * @return      The actual clock rate after set
 */
double spi_dev_set_clock_rate(handle_t file, double clock_rate);

void spi_dev_set_endian(handle_t file, uint32_t endian);

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
int spi_dev_transfer_full_duplex(handle_t file, const uint8_t *write_buffer, size_t write_len, uint8_t *read_buffer, size_t read_len);

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
int spi_dev_transfer_sequential(handle_t file, const uint8_t *write_buffer, size_t write_len, uint8_t *read_buffer, size_t read_len);

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
void spi_dev_fill(handle_t file, uint32_t instruction, uint32_t address, uint32_t value, size_t count);

/**
 * @brief       Configure a DVP device
 *
 * @param[in]   file            The DVP device handle
 * @param[in]   width           The frame width
 * @param[in]   height          The frame height
 * @param[in]   auto_enable     Process frames automatically
 */
void dvp_config(handle_t file, uint32_t width, uint32_t height, bool auto_enable);

/**
 * @brief       Enable to process of current frame
 *
 * @param[in]   file    The DVP device handle
 */
void dvp_enable_frame(handle_t file);

/**
 * @brief       Get the count of outputs of a DVP device
 *
 * @param[in]   file        The DVP device handle
 *
 * @return      The count of outputs
 */
uint32_t dvp_get_output_num(handle_t file);

/**
 * @brief       Set or unset a signal to a DVP device
 *
 * @param[in]   file        The DVP device handle
 * @param[in]   type        The signal type
 * @param[in]   value       1 is set, 0 is unset
 */
void dvp_set_signal(handle_t file, dvp_signal_type_t type, bool value);

/**
 * @brief       Enable or disable a output of a DVP device
 *
 * @param[in]   file        The DVP device handle
 * @param[in]   index       The output index
 * @param[in]   enable      1 is enable, 0 is disable
 */
void dvp_set_output_enable(handle_t file, uint32_t index, bool enable);

/**
 * @brief       Set output attributes of a DVP device
 *
 * @param[in]   file                The DVP device handle
 * @param[in]   index               The output index
 * @param[in]   format              The output format
 * @param[out]  output_buffer       The output buffer
 */
void dvp_set_output_attributes(handle_t file, uint32_t index, video_format_t format, void *output_buffer);

/**
 * @brief       Enable or disable a frame event of a DVP device
 *
 * @param[in]   file        The DVP device handle
 * @param[in]   event       The frame event
 * @param[in]   enable      1 is enable, 0 is disable
 */
void dvp_set_frame_event_enable(handle_t file, dvp_frame_event_t event, bool enable);

/**
 * @brief       Set the frame event handler of a DVP device
 *
 * @param[in]   file            The DVP device handle
 * @param[in]   handler         The event handler
 * @param[in]   userdata        The userdata of the event handler
 */
void dvp_set_on_frame_event(handle_t file, dvp_on_frame_event_t handler, void *userdata);

/**
 * @brief       Set the rate of the DVP XCLK
 *
 * @param[in]   file            The DVP device handle
 * @param[in]   clock_rate      The desired clock rate in Hz
 *
 * @return      The actual clock rate after set
 */
double dvp_xclk_set_clock_rate(handle_t file, double clock_rate);

/**
 * @brief       Register and open a SCCB device
 *
 * @param[in]   file                    The SCCB controller handle
 * @param[in]   name                    Specify the path to access the device
 * @param[in]   slave_address           The address of slave
 * @param[in]   reg_address_width       The bits width of register address
 *
 * @return      The SCCB device handle
 */
handle_t sccb_get_device(handle_t file, uint32_t slave_address, uint32_t reg_address_width);

/**
 * @brief       Read a byte from a SCCB device
 *
 * @param[in]   file            The SCCB controller handle
 * @param[in]   reg_address     The register address
 *
 * @return      The byte read
 */
uint8_t sccb_dev_read_byte(handle_t file, uint16_t reg_address);

/**
 * @brief       Write a byte to a SCCB device
 *
 * @param[in]   file            The SCCB controller handle
 * @param[in]   reg_address     The register address
 * @param[in]   value           The data byte
 */
void sccb_dev_write_byte(handle_t file, uint16_t reg_address, uint8_t value);

/**
 * @brief       Do 16bit quantized complex FFT
 *
 * @param[in]   shift           The shifts selection in 9 stage
 * @param[in]   direction       The direction
 * @param[in]   input           The input data
 * @param[in]   point           The FFT points count
 * @param[out]  output          The output data
 */
void fft_complex_uint16(uint16_t shift, fft_direction_t direction, const uint64_t *input, size_t point, uint64_t *output);

/**
 * @brief       AES-ECB-128 decryption
 *
 * @param[in]   input_key       The decryption key. must be 16bytes.
 * @param[in]   input_data      The buffer holding the input data.
 * @param[in]   input_len       The length of a data unit in bytes.
 *                              This can be any length between 16 bytes and 2^31 bytes inclusive
 *                              (between 1 and 2^27 block cipher blocks).
 * @param[out]  output_data     The buffer holding the output data.
 */
void aes_ecb128_hard_decrypt(const uint8_t *input_key, const uint8_t *input_data, size_t input_len, uint8_t *output_data);

/**
 * @brief       AES-ECB-128 encryption
 *
 * @param[in]   input_key       The encryption key. must be 16bytes.
 * @param[in]   input_data      The buffer holding the input data.
 * @param[in]   input_len       The length of a data unit in bytes.
 *                              This can be any length between 16 bytes and 2^31 bytes inclusive
 *                              (between 1 and 2^27 block cipher blocks).
 * @param[out]  output_data     The buffer holding the output data.
 */
void aes_ecb128_hard_encrypt(const uint8_t *input_key, const uint8_t *input_data, size_t input_len, uint8_t *output_data);

/**
 * @brief       AES-ECB-192 decryption
 *
 * @param[in]   input_key       The decryption key. must be 24bytes.
 * @param[in]   input_data      The buffer holding the input data.
 * @param[in]   input_len       The length of a data unit in bytes.
 *                              This can be any length between 16 bytes and 2^31 bytes inclusive
 *                              (between 1 and 2^27 block cipher blocks).
 * @param[out]  output_data     The buffer holding the output data.
 */
void aes_ecb192_hard_decrypt(const uint8_t *input_key, const uint8_t *input_data, size_t input_len, uint8_t *output_data);

/**
 * @brief       AES-ECB-192 encryption
 *
 * @param[in]   input_key       The encryption key. must be 24bytes.
 * @param[in]   input_data      The buffer holding the input data.
 * @param[in]   input_len       The length of a data unit in bytes.
 *                              This can be any length between 16 bytes and 2^31 bytes inclusive
 *                              (between 1 and 2^27 block cipher blocks).
 * @param[out]  output_data     The buffer holding the output data.
 */
void aes_ecb192_hard_encrypt(const uint8_t *input_key, const uint8_t *input_data, size_t input_len, uint8_t *output_data);

/**
 * @brief       AES-ECB-256 decryption
 *
 * @param[in]   input_key       The decryption key. must be 32bytes.
 * @param[in]   input_data      The buffer holding the input data.
 * @param[in]   input_len       The length of a data unit in bytes.
 *                              This can be any length between 16 bytes and 2^31 bytes inclusive
 *                              (between 1 and 2^27 block cipher blocks).
 * @param[out]  output_data     The buffer holding the output data.
 */
void aes_ecb256_hard_decrypt(const uint8_t *input_key, const uint8_t *input_data, size_t input_len, uint8_t *output_data);

/**
 * @brief       AES-ECB-256 encryption
 *
 * @param[in]   input_key       The encryption key. must be 32bytes.
 * @param[in]   input_data      The buffer holding the input data.
 * @param[in]   input_len       The length of a data unit in bytes.
 *                              This can be any length between 16 bytes and 2^31 bytes inclusive
 *                              (between 1 and 2^27 block cipher blocks).
 * @param[out]  output_data     The buffer holding the output data.
 */
void aes_ecb256_hard_encrypt(const uint8_t *input_key, const uint8_t *input_data, size_t input_len, uint8_t *output_data);

/**
 * @brief       AES-CBC-128 decryption
 *
 * @param[in]   context         The cbc context to use for encryption or decryption.
 * @param[in]   input_key       The decryption key. must be 16bytes.
 * @param[in]   input_data      The buffer holding the input data.
 * @param[in]   input_len       The length of a data unit in bytes.
 *                              This can be any length between 16 bytes and 2^31 bytes inclusive
 *                              (between 1 and 2^27 block cipher blocks).
 * @param[out]  output_data     The buffer holding the output data.
 */
void aes_cbc128_hard_decrypt(cbc_context_t *context, const uint8_t *input_data, size_t input_len, uint8_t *output_data);

/**
 * @brief       AES-CBC-128 encryption
 *
 * @param[in]   context         The cbc context to use for encryption or decryption.
 * @param[in]   input_key       The encryption key. must be 16bytes.
 * @param[in]   input_data      The buffer holding the input data.
 * @param[in]   input_len       The length of a data unit in bytes.
 *                              This can be any length between 16 bytes and 2^31 bytes inclusive
 *                              (between 1 and 2^27 block cipher blocks).
 * @param[out]  output_data     The buffer holding the output data.
 */
void aes_cbc128_hard_encrypt(cbc_context_t *context, const uint8_t *input_data, size_t input_len, uint8_t *output_data);

/**
 * @brief       AES-CBC-192 decryption
 *
 * @param[in]   context         The cbc context to use for encryption or decryption.
 * @param[in]   input_key       The decryption key. must be 24bytes.
 * @param[in]   input_data      The buffer holding the input data.
 * @param[in]   input_len       The length of a data unit in bytes.
 *                              This can be any length between 16 bytes and 2^31 bytes inclusive
 *                              (between 1 and 2^27 block cipher blocks).
 * @param[out]  output_data     The buffer holding the output data.
 */
void aes_cbc192_hard_decrypt(cbc_context_t *context, const uint8_t *input_data, size_t input_len, uint8_t *output_data);

/**
 * @brief       AES-CBC-192 encryption
 *
 * @param[in]   context         The cbc context to use for encryption or decryption.
 * @param[in]   input_key       The encryption key. must be 24bytes.
 * @param[in]   input_data      The buffer holding the input data.
 * @param[in]   input_len       The length of a data unit in bytes.
 *                              This can be any length between 16 bytes and 2^31 bytes inclusive
 *                              (between 1 and 2^27 block cipher blocks).
 * @param[out]  output_data     The buffer holding the output data.
 */
void aes_cbc192_hard_encrypt(cbc_context_t *context, const uint8_t *input_data, size_t input_len, uint8_t *output_data);

/**
 * @brief       AES-CBC-256 decryption
 *
 * @param[in]   context         The cbc context to use for encryption or decryption.
 * @param[in]   input_key       The decryption key. must be 32bytes.
 * @param[in]   input_data      The buffer holding the input data.
 * @param[in]   input_len       The length of a data unit in bytes.
 *                              This can be any length between 16 bytes and 2^31 bytes inclusive
 *                              (between 1 and 2^27 block cipher blocks).
 * @param[out]  output_data     The buffer holding the output data.
 */
void aes_cbc256_hard_decrypt(cbc_context_t *context, const uint8_t *input_data, size_t input_len, uint8_t *output_data);

/**
 * @brief       AES-CBC-256 encryption
 *
 * @param[in]   context         The cbc context to use for encryption or decryption.
 * @param[in]   input_key       The encryption key. must be 32bytes.
 * @param[in]   input_data      The buffer holding the input data.
 * @param[in]   input_len       The length of a data unit in bytes.
 *                              This can be any length between 16 bytes and 2^31 bytes inclusive
 *                              (between 1 and 2^27 block cipher blocks).
 * @param[out]  output_data     The buffer holding the output data.
 */
void aes_cbc256_hard_encrypt(cbc_context_t *context, const uint8_t *input_data, size_t input_len, uint8_t *output_data);

/**
 * @brief       AES-GCM-128 decryption
 *
 * @param[in]   context         The gcm context to use for encryption or decryption.
 * @param[in]   input_key       The decryption key. must be 16bytes.
 * @param[in]   input_data      The buffer holding the input data.
 * @param[in]   input_len       The length of a data unit in bytes.
 *                              This can be any length between 16 bytes and 2^31 bytes inclusive
 *                              (between 1 and 2^27 block cipher blocks).
 * @param[out]  output_data     The buffer holding the output data.
 * @param[out]  gcm_tag         The buffer for holding the tag.The length of the tag must be 4 bytes.
 */
void aes_gcm128_hard_decrypt(gcm_context_t *context, const uint8_t *input_data, size_t input_len, uint8_t *output_data, uint8_t *gcm_tag);

/**
 * @brief       AES-GCM-128 encryption
 *
 * @param[in]   context         The gcm context to use for encryption or decryption.
 * @param[in]   input_key       The encryption key. must be 16bytes.
 * @param[in]   input_data      The buffer holding the input data.
 * @param[in]   input_len       The length of a data unit in bytes.
 *                              This can be any length between 16 bytes and 2^31 bytes inclusive
 *                              (between 1 and 2^27 block cipher blocks).
 * @param[out]  output_data     The buffer holding the output data.
 * @param[out]  gcm_tag         The buffer for holding the tag.The length of the tag must be 4 bytes.
 */
void aes_gcm128_hard_encrypt(gcm_context_t *context, const uint8_t *input_data, size_t input_len, uint8_t *output_data, uint8_t *gcm_tag);

/**
 * @brief       AES-GCM-192 decryption
 *
 * @param[in]   context         The gcm context to use for encryption or decryption.
 * @param[in]   input_key       The decryption key. must be 24bytes.
 * @param[in]   input_data      The buffer holding the input data.
 * @param[in]   input_len       The length of a data unit in bytes.
 *                              This can be any length between 16 bytes and 2^31 bytes inclusive
 *                              (between 1 and 2^27 block cipher blocks).
 * @param[out]  output_data     The buffer holding the output data.
 * @param[out]  gcm_tag         The buffer for holding the tag.The length of the tag must be 4 bytes.
 */
void aes_gcm192_hard_decrypt(gcm_context_t *context, const uint8_t *input_data, size_t input_len, uint8_t *output_data, uint8_t *gcm_tag);

/**
 * @brief       AES-GCM-192 encryption
 *
 * @param[in]   context         The gcm context to use for encryption or decryption.
 * @param[in]   input_key       The encryption key. must be 24bytes.
 * @param[in]   input_data      The buffer holding the input data.
 * @param[in]   input_len       The length of a data unit in bytes.
 *                              This can be any length between 16 bytes and 2^31 bytes inclusive
 *                              (between 1 and 2^27 block cipher blocks).
 * @param[out]  output_data     The buffer holding the output data.
 * @param[out]  gcm_tag         The buffer for holding the tag.The length of the tag must be 4 bytes.
 */
void aes_gcm192_hard_encrypt(gcm_context_t *context, const uint8_t *input_data, size_t input_len, uint8_t *output_data, uint8_t *gcm_tag);

/**
 * @brief       AES-GCM-256 decryption
 *
 * @param[in]   context         The gcm context to use for encryption or decryption.
 * @param[in]   input_key       The decryption key. must be 32bytes.
 * @param[in]   input_data      The buffer holding the input data.
 * @param[in]   input_len       The length of a data unit in bytes.
 *                              This can be any length between 16 bytes and 2^31 bytes inclusive
 *                              (between 1 and 2^27 block cipher blocks).
 * @param[out]  output_data     The buffer holding the output data.
 * @param[out]  gcm_tag         The buffer for holding the tag.The length of the tag must be 4 bytes.
 */
void aes_gcm256_hard_decrypt(gcm_context_t *context, const uint8_t *input_data, size_t input_len, uint8_t *output_data, uint8_t *gcm_tag);

/**
 * @brief       AES-GCM-256 encryption
 *
 * @param[in]   context         The gcm context to use for encryption or decryption.
 * @param[in]   input_key       The encryption key. must be 32bytes.
 * @param[in]   input_data      The buffer holding the input data.
 * @param[in]   input_len       The length of a data unit in bytes.
 *                              This can be any length between 16 bytes and 2^31 bytes inclusive
 *                              (between 1 and 2^27 block cipher blocks).
 * @param[out]  output_data     The buffer holding the output data.
 * @param[out]  gcm_tag         The buffer for holding the tag.The length of the tag must be 4 bytes.
 */
void aes_gcm256_hard_encrypt(gcm_context_t *context, const uint8_t *input_data, size_t input_len, uint8_t *output_data, uint8_t *gcm_tag);

/**
 * @brief       Do sha256
 *
 * @param[in]   input         The sha256 data
 * @param[in]   input_len      The data length
 * @param[out]  output        The sha256 result
 */
void sha256_hard_calculate(const uint8_t *input, size_t input_len, uint8_t *output);

/**
 * @brief       Set the interval of a TIMER device
 *
 * @param[in]   file            The TIMER controller handle
 * @param[in]   nanoseconds     The desired interval in nanoseconds
 *
 * @return      The actual interval in nanoseconds
 */
size_t timer_set_interval(handle_t file, size_t nanoseconds);

/**
 * @brief       Set the tick handler of a TIMER device
 *
 * @param[in]   file            The TIMER controller handle
 * @param[in]   on_tick         The tick handler
 * @param[in]   userdata        The userdata of the handler
 */
void timer_set_on_tick(handle_t file, timer_on_tick_t on_tick, void *userdata);

/**
 * @brief       Enable or disable a TIMER device
 *
 * @param[in]   file        The TIMER controller handle
 * @param[in]   enable      1 is enable, 0 is disable
 */
void timer_set_enable(handle_t file, bool enable);

/**
 * @brief       Get the pin count of a PWM controller
 *
 * @param[in]   file        The PWM controller handle
 *
 * @return      The pin count
 */
uint32_t pwm_get_pin_count(handle_t file);

/**
 * @brief       Set the frequency of a PWM controller
 *
 * @param[in]   file            The PWM controller handle
 * @param[in]   frequency       The desired frequency in Hz
 *
 * @return      The actual frequency after set
 */
double pwm_set_frequency(handle_t file, double frequency);

/**
 * @brief       Set the active duty cycle percentage of a PWM pin
 *
 * @param[in]   file                        The PWM controller handle
 * @param[in]   pin                         The PWM pin
 * @param[in]   duty_cycle_percentage       The desired active duty cycle percentage
 *
 * @return      The actual active duty cycle percentage after set
 */
double pwm_set_active_duty_cycle_percentage(handle_t file, uint32_t pin, double duty_cycle_percentage);

/**
 * @brief       Enable or disable a PWM pin
 *
 * @param[in]   file        The PWM controller handle
 * @param[in]   pin         The PWM pin
 * @param[in]   enable      1 is enable, 0 is disable
 */
void pwm_set_enable(handle_t file, uint32_t pin, bool enable);

/**
 * @brief       Set the response mode of a WDT device
 *
 * @param[in]   file        The WDT device handle
 * @param[in]   mode        The response mode
 */
void wdt_set_response_mode(handle_t file, wdt_response_mode_t mode);

/**
 * @brief       Set the timeout of a WDT device
 *
 * @param[in]   file            The WDT device handle
 * @param[in]   nanoseconds     The desired timeout in nanoseconds
 *
 * @return      The actual timeout in nanoseconds
 */
size_t wdt_set_timeout(handle_t file, size_t nanoseconds);

/**
 * @brief       Set the timeout handler of a WDT device
 *
 * @param[in]   file        The WDT device handle
 * @param[in]   handler     The timeout handler
 * @param[in]   userdata    The userdata of the handler
 */
void wdt_set_on_timeout(handle_t file, wdt_on_timeout_t handler, void *userdata);

/**
 * @brief       Restart the counter a WDT device
 *
 * @param[in]   file        The WDT device handle
 */
void wdt_restart_counter(handle_t file);

/**
 * @brief       Enable or disable a WDT device
 *
 * @param[in]   file        The WDT device handle
 * @param[in]   enable      1 is enable, 0 is disable
 */
void wdt_set_enable(handle_t file, bool enable);

/**
 * @brief       Get the datetime of a RTC device
 *
 * @param[in]   file            The RTC device
 * @param[out]  datetime        The datatime
 */
void rtc_get_datetime(handle_t file, struct tm *datetime);

/**
 * @brief       Set the datetime of a RTC device
 *
 * @param[in]   file            The RTC device
 * @param[out]  datetime        The datatime to be set
 */
void rtc_set_datetime(handle_t file, const struct tm *datetime);

/**
 * @brief       Load model from buffer
 *
 * @param[in]   buffer      model data
 *
 * @return      result
 *     - 0      Fail
 *     - other  The kpu context handle
 */
handle_t kpu_model_load_from_buffer(uint8_t *buffer);

/**
 * @brief       KPU run.
 *
 * @param[in]   context         The kpu context handle
 * @param[in]   src             The src data
 *
 * @return      result
 *     - 0      Success
 *     - other  Fail
 */
int kpu_run(handle_t context, const uint8_t *src);

/**
 * @brief       Get output data.
 *
 * @param[in]   context         The kpu context handle
 * @param[in]   index           The output index.
 * @param[out]  data            The address of the kpu output data address.
 * @param[out]  size            The address of output data size
 *
 * @return      result
 *     - 0      Success
 *     - other  Fail
 */
int kpu_get_output(handle_t context, uint32_t index, uint8_t **data, size_t *size);

#ifdef __cplusplus
}
#endif

#endif /* _FREERTOS_DEVICES_H */
