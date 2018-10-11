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
#include <driver.h>
#include <hal.h>
#include <plic.h>
#include <semphr.h>
#include <stdio.h>
#include <stdlib.h>
#include <sysctl.h>
#include <uart.h>

#define UART_BRATE_CONST 16
#define RINGBUFF_LEN 64
#define COMMON_ENTRY                                            \
    uart_data *data = (uart_data *)userdata;                    \
    volatile uart_t *uart = (volatile uart_t *)data->base_addr; \
    (void)uart;

typedef struct
{
    size_t head;
    size_t tail;
    size_t length;
    uint8_t ring_buffer[RINGBUFF_LEN];
} ringbuffer_t;

typedef struct
{
    sysctl_clock_t clock;
    uintptr_t base_addr;
    size_t channel;
    ringbuffer_t *recv_buf;
} uart_data;

static int write_ringbuff(uint8_t rdata, void *userdata)
{
    COMMON_ENTRY;
    ringbuffer_t *ring_buff = data->recv_buf;

    if (ring_buff->length >= RINGBUFF_LEN)
        return -1;

    ring_buff->ring_buffer[ring_buff->tail] = rdata;
    ring_buff->tail = (ring_buff->tail + 1) % RINGBUFF_LEN;
    ring_buff->length++;
    return 0;
}

static int read_ringbuff(uint8_t *rData, size_t len, void *userdata)
{
    COMMON_ENTRY;
    ringbuffer_t* ring_buff = data->recv_buf;
    size_t cnt = 0;
    while ((len--) && ring_buff->length)
    {
        *(rData++) = ring_buff->ring_buffer[ring_buff->head];
        ring_buff->head = (ring_buff->head + 1) % RINGBUFF_LEN;
        ring_buff->length--;
        cnt++;
    }

    return cnt;
}

static void on_irq_apbuart_recv(void *userdata)
{
    COMMON_ENTRY;
    while (uart->LSR & 1)
        write_ringbuff(((uint8_t)(uart->RBR & 0xff)), userdata);
}

static void uart_install(void *userdata)
{
    COMMON_ENTRY;

    sysctl_clock_enable(data->clock);
}

static int uart_open(void *userdata)
{
    COMMON_ENTRY;
    ringbuffer_t* ring_buff = malloc(sizeof(ringbuffer_t));
    ring_buff->head = 0;
    ring_buff->tail = 0;
    ring_buff->length = 0;
    data->recv_buf = ring_buff;
    pic_set_irq_handler(IRQN_UART1_INTERRUPT + data->channel, on_irq_apbuart_recv, userdata);
    pic_set_irq_priority(IRQN_UART1_INTERRUPT + data->channel, 1);
    pic_set_irq_enable(IRQN_UART1_INTERRUPT + data->channel, 1);
    return 1;
}

static void uart_close(void *userdata)
{
    COMMON_ENTRY;
    free(data->recv_buf);
}

static void uart_config(uint32_t baud_rate, uint32_t databits, uart_stopbits_t stopbits, uart_parity_t parity, void *userdata)
{
    COMMON_ENTRY;

    configASSERT(databits >= 5 && databits <= 8);
    if (databits == 5)
    {
        configASSERT(stopbits != UART_STOP_2);
    }
    else
    {
        configASSERT(stopbits != UART_STOP_1_5);
    }

    uint32_t stopbit_val = stopbits == UART_STOP_1 ? 0 : 1;
    uint32_t parity_val = 0;
    switch (parity)
    {
    case UART_PARITY_NONE:
        parity_val = 0;
        break;
    case UART_PARITY_ODD:
        parity_val = 1;
        break;
    case UART_PARITY_EVEN:
        parity_val = 3;
        break;
    default:
        configASSERT(!"Invalid parity");
        break;
    }

    uint32_t freq = sysctl_clock_get_freq(data->clock);
    uint32_t u16Divider = (freq + UART_BRATE_CONST * baud_rate / 2) / (UART_BRATE_CONST * baud_rate);

    /* Set UART registers */
    uart->TCR &= ~(1u);
    uart->TCR &= ~(1u << 3);
    uart->TCR &= ~(1u << 4);
    uart->TCR |= (1u << 2);
    uart->TCR &= ~(1u << 1);
    uart->DE_EN &= ~(1u);

    uart->LCR |= 1u << 7;
    uart->DLL = u16Divider & 0xFF;
    uart->DLH = u16Divider >> 8;
    uart->LCR = 0;
    uart->LCR = (databits - 5) | (stopbit_val << 2) | (parity_val << 3);
    uart->LCR &= ~(1u << 7);
    uart->MCR &= ~3;
    uart->IER = 1;
}

static int uart_putc(volatile uart_t *uart, char c)
{
    while (!(uart->LSR & (1u << 6)))
        ;
    uart->THR = c;
    return 0;
}

static int uart_read(uint8_t *buffer, size_t len, void *userdata)
{
    return read_ringbuff(buffer, len, userdata);
}

static int uart_write(const uint8_t *buffer, size_t len, void *userdata)
{
    COMMON_ENTRY;

    int write = 0;
    while (write < len)
    {
        uart_putc(uart, *buffer++);
        write++;
    }

    return write;
}

static uart_data dev0_data = {SYSCTL_CLOCK_UART1, UART1_BASE_ADDR, 0, NULL};
static uart_data dev1_data = {SYSCTL_CLOCK_UART2, UART2_BASE_ADDR, 1, NULL};
static uart_data dev2_data = {SYSCTL_CLOCK_UART3, UART3_BASE_ADDR, 2, NULL};

const uart_driver_t g_uart_driver_uart0 = {{&dev0_data, uart_install, uart_open, uart_close}, uart_config, uart_read, uart_write};
const uart_driver_t g_uart_driver_uart1 = {{&dev1_data, uart_install, uart_open, uart_close}, uart_config, uart_read, uart_write};
const uart_driver_t g_uart_driver_uart2 = {{&dev2_data, uart_install, uart_open, uart_close}, uart_config, uart_read, uart_write};
