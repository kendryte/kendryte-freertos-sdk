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
#ifndef _FREERTOS_OSDEFS_H
#define _FREERTOS_OSDEFS_H

#include <platform.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>
#include "FreeRTOS.h"
#include "semphr.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define NULL_HANDLE 0

#define MAX_PATH 256

typedef uintptr_t handle_t;

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

typedef void(*gpio_on_changed_t)(uint32_t pin, void *userdata);

typedef enum _i2c_event
{
    I2C_EV_START,
    I2C_EV_RESTART,
    I2C_EV_STOP
} i2c_event_t;

typedef struct _i2c_slave_handler
{
    void(*on_receive)(uint32_t data);
    uint32_t(*on_transmit)();
    void(*on_event)(i2c_event_t event);
} i2c_slave_handler_t;

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

typedef enum {
    WRITE_CONFIG,
    READ_CONFIG,
    WRITE_DATA_BYTE,
    READ_DATA_BYTE,
    WRITE_DATA_BLOCK,
    READ_DATA_BLOCK,
} spi_slave_command_e;

typedef struct
{
    uint8_t cmd;
    uint8_t err;
    uint32_t addr;
    uint32_t len;
} spi_slave_command_t;

typedef int (*spi_slave_receive_callback_t)(void *ctx);

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

typedef void(*dvp_on_frame_event_t)(dvp_frame_event_t event, void *userdata);

typedef struct tag_fft_data
{
    int16_t I1;
    int16_t R1;
    int16_t I2;
    int16_t R2;
} fft_data_t;

typedef enum tag_fft_direction
{
    FFT_DIR_BACKWARD,
    FFT_DIR_FORWARD,
    FFT_DIR_MAX,
} fft_direction_t;

typedef enum
{
    AES_ECB = 0,
    AES_CBC = 1,
    AES_GCM = 2,
    AES_CIPHER_MAX
} aes_cipher_mode_t;

typedef enum
{
    AES_128 = 16,
    AES_192 = 24,
    AES_256 = 32,
} aes_kmode;

typedef enum
{
    AES_HARD_ENCRYPTION = 0,
    AES_HARD_DECRYPTION = 1,
} aes_encrypt_sel_t;

typedef struct _gcm_context
{
    /* The buffer holding the encryption or decryption key. */
    uint8_t *input_key;
    /* The initialization vector. must be 96 bit */
    uint8_t *iv;
    /* The buffer holding the Additional authenticated data. or NULL */
    uint8_t *gcm_aad;
    /* The length of the Additional authenticated data. or 0L */
    size_t gcm_aad_len;
} gcm_context_t;

typedef struct _cbc_context
{
    /* The buffer holding the encryption or decryption key. */
    uint8_t *input_key;
    /* The initialization vector. must be 128 bit */
    uint8_t *iv;
} cbc_context_t;

typedef void(*timer_on_tick_t)(void* userdata);

typedef enum _wdt_response_mode
{
    WDT_RESP_RESET,
    WDT_RESP_INTERRUPT
} wdt_response_mode_t;

typedef int(*wdt_on_timeout_t)(void *userdata);

typedef void(*pic_irq_handler_t)(void *userdata);

typedef void(*dma_stage_completion_handler_t)(void *userdata);

typedef enum _file_access
{
    FILE_ACCESS_READ = 1,
    FILE_ACCESS_WRITE = 2,
    FILE_ACCESS_READ_WRITE = 3
} file_access_t;

typedef enum _file_mode
{
    /* Opens the file. The function fails if the file is not existing. (Default) */
    FILE_MODE_OPEN_EXISTING,
    /* Creates a new file. The function fails with FR_EXIST if the file is existing. */
    FILE_MODE_CREATE_NEW,
    /* Creates a new file. If the file is existing, it will be truncated and overwritten. */
    FILE_MODE_CREATE_ALWAYS,
    /* Opens the file if it is existing. If not, a new file will be created. */
    FILE_MODE_OPEN_ALWAYS,
    /* Same as FILE_MODE_OPEN_ALWAYS except the read/write pointer is set end of the file. */
    FILE_MODE_APPEND,
    FILE_MODE_TRUNCATE
} file_mode_t;

typedef struct _find_file_data
{
    char filename[MAX_PATH];
} find_find_data_t;

typedef enum _address_family
{
    AF_UNSPECIFIED,
    AF_INTERNETWORK
} address_family_t;

typedef enum _socket_type
{
    SOCKET_STREAM,
    SOCKET_DATAGRAM
} socket_type_t;

typedef enum _socket_message_flag
{
    MESSAGE_NORMAL = 0x00,
    MESSAGE_PEEK = 0x01,
    MESSAGE_WAITALL = 0x02,
    MESSAGE_OOB = 0x04,
    MESSAGE_DONTWAIT = 0x08,
    MESSAGE_MORE = 0x10
} socket_message_flag_t;

typedef enum _protocol_type
{
    PROTCL_IP
} protocol_type_t;

typedef struct _socket_address
{
    uint8_t size;
    address_family_t family;
    uint8_t data[14];
} socket_address_t;

typedef enum _socket_shutdown
{
    SOCKSHTDN_RECEIVE,
    SOCKSHTDN_SEND,
    SOCKSHTDN_BOTH
} socket_shutdown_t;

typedef struct _ip_address
{
    address_family_t family;
    uint8_t data[16];
} ip_address_t;

typedef struct _mac_address
{
    uint8_t data[6];
} mac_address_t;

typedef struct _hostent
{
    /* Official name of the host. */
    char *h_name;
    /* A pointer to an array of pointers to alternative host names, terminated by a null pointer. */
    char **h_aliases;
    /* Address type. */ 
    uint32_t h_addrtype;
    /* The length, in bytes, of the address. */
    uint32_t h_length;
    /* A pointer to an array of pointers to network addresses (in
    network byte order) for the host, terminated by a null pointer. */
    uint8_t **h_addr_list; 
} hostent_t;

typedef enum _dhcp_state
{
    DHCP_START = 0,
    DHCP_WAIT_ADDRESS,
    DHCP_ADDRESS_ASSIGNED,
    DHCP_TIMEOUT,
    DHCP_FAIL
} dhcp_state_t;

#define SYS_IOCPARM_MASK    0x7fU           /* parameters must be < 128 bytes */
#define SYS_IOC_VOID        0x20000000UL    /* no parameters */
#define SYS_IOC_OUT         0x40000000UL    /* copy out parameters */
#define SYS_IOC_IN          0x80000000UL    /* copy in parameters */
#define SYS_IOC_INOUT       (SYS_IOC_IN | SYS_IOC_OUT) /* 0x20000000 distinguishes new & old ioctl's */
#define SYS_IO(x, y)        (SYS_IOC_VOID | ((x) << 8) | (y))

#define SYS_IOR(x, y, t)    (SYS_IOC_OUT | (((uint32_t)sizeof(t) & SYS_IOCPARM_MASK) << 16) | ((x) << 8) | (y))

#define SYS_IOW(x, y, t)    (SYS_IOC_IN | (((uint32_t)sizeof(t) & SYS_IOCPARM_MASK) << 16) | ((x) << 8) | (y))

#define SYS_FIONBIO         SYS_IOW('f', 126, uint32_t) /* set/clear non-blocking i/o */

#ifdef __cplusplus
}
#endif

#endif /* _FREERTOS_OSDEFS_H */
