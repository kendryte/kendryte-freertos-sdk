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
#include <hal.h>
#include <kernel/driver_impl.hpp>
#include <plic.h>
#include <semphr.h>
#include <stdio.h>
#include <stdlib.h>
#include <sysctl.h>
#include <uart.h>

using namespace sys;

#define UART_BRATE_CONST 16
#define RINGBUFF_LEN 64

typedef struct
{
    size_t head;
    size_t tail;
    size_t length;
    uint8_t ring_buffer[RINGBUFF_LEN];
} ringbuffer_t;

class k_uart_driver : public uart_driver, public static_object, public free_object_access
{
public:
    k_uart_driver(uintptr_t base_addr, sysctl_clock_t clock, plic_irq_t irq)
        : uart_(*reinterpret_cast<volatile uart_t *>(base_addr)), clock_(clock), irq_(irq)
    {
    }

    virtual void install() override
    {
        receive_event_ = xSemaphoreCreateBinary();
        sysctl_clock_disable(clock_);
    }

    virtual void on_first_open() override
    {
        sysctl_clock_enable(clock_);

        ringbuffer_t *ring_buff = new ringbuffer_t();
        ring_buff->head = 0;
        ring_buff->tail = 0;
        ring_buff->length = 0;
        recv_buf_ = ring_buff;
        pic_set_irq_handler(irq_, on_irq_apbuart_recv, this);
        pic_set_irq_priority(irq_, 1);
        pic_set_irq_enable(irq_, 1);
    }

    virtual void on_last_close() override
    {
        sysctl_clock_disable(clock_);
        delete recv_buf_;
    }

    virtual void config(uint32_t baud_rate, uint32_t databits, uart_stopbits_t stopbits, uart_parity_t parity) override
    {
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

        uint32_t freq = sysctl_clock_get_freq(clock_);
        uint32_t u16Divider = (freq + UART_BRATE_CONST * baud_rate / 2) / (UART_BRATE_CONST * baud_rate);

        /* Set UART registers */
        uart_.TCR &= ~(1u);
        uart_.TCR &= ~(1u << 3);
        uart_.TCR &= ~(1u << 4);
        uart_.TCR |= (1u << 2);
        uart_.TCR &= ~(1u << 1);
        uart_.DE_EN &= ~(1u);

        uart_.LCR |= 1u << 7;
        uart_.DLL = u16Divider & 0xFF;
        uart_.DLH = u16Divider >> 8;
        uart_.LCR = 0;
        uart_.LCR = (databits - 5) | (stopbit_val << 2) | (parity_val << 3);
        uart_.LCR &= ~(1u << 7);
        uart_.MCR &= ~3;
        uart_.IER = 1;
    }

    virtual int read(gsl::span<uint8_t> buffer) override
    {
        return read_ringbuff(buffer.data(), buffer.size());
    }

    virtual int write(gsl::span<const uint8_t> buffer) override
    {
        auto it = buffer.begin();
        int write = 0;
        while (write < buffer.size())
        {
            uart_putc(*it++);
            write++;
        }

        return write;
    }

    virtual void set_read_timeout(size_t millisecond) override
    {
        read_timeout_ = millisecond / portTICK_PERIOD_MS;
    }

private:
    int uart_putc(char c)
    {
        while (!(uart_.LSR & (1u << 6)))
            ;
        uart_.THR = c;
        return 0;
    }

    int write_ringbuff(uint8_t rdata)
    {
        ringbuffer_t *ring_buff = recv_buf_;

        if (ring_buff->length >= RINGBUFF_LEN)
            return -1;

        ring_buff->ring_buffer[ring_buff->tail] = rdata;
        ring_buff->tail = (ring_buff->tail + 1) % RINGBUFF_LEN;
        ring_buff->length++;
        return 0;
    }

    int read_ringbuff(uint8_t *rData, size_t len)
    {
        ringbuffer_t *ring_buff = recv_buf_;
        size_t cnt = 0;
        while (len)
        {
            if(ring_buff->length)
            {
                *(rData++) = ring_buff->ring_buffer[ring_buff->head];
                ring_buff->head = (ring_buff->head + 1) % RINGBUFF_LEN;
                ring_buff->length--;
                cnt++;
                len--;
            }
            else
            {
                if(xSemaphoreTake(receive_event_, read_timeout_) == pdTRUE)
                {
                    continue;
                }
                else
                {
                    return -1;
                }
            }
        }

        return cnt;
    }

    static void on_irq_apbuart_recv(void *userdata)
    {
        auto &driver = *reinterpret_cast<k_uart_driver *>(userdata);
        auto &uart = driver.uart_;

        while (uart.LSR & 1)
        {
            driver.write_ringbuff(((uint8_t)(uart.RBR & 0xff)));
        }
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xSemaphoreGiveFromISR(driver.receive_event_, &xHigherPriorityTaskWoken);
        if (xHigherPriorityTaskWoken)
        {
            portYIELD_FROM_ISR();
        }
    }

private:
    volatile uart_t &uart_;
    sysctl_clock_t clock_;
    plic_irq_t irq_;
    SemaphoreHandle_t receive_event_;

    ringbuffer_t *recv_buf_;
    size_t read_timeout_ = portMAX_DELAY;
};

static k_uart_driver dev0_driver(UART1_BASE_ADDR, SYSCTL_CLOCK_UART1, IRQN_UART1_INTERRUPT);
static k_uart_driver dev1_driver(UART2_BASE_ADDR, SYSCTL_CLOCK_UART2, IRQN_UART2_INTERRUPT);
static k_uart_driver dev2_driver(UART3_BASE_ADDR, SYSCTL_CLOCK_UART3, IRQN_UART3_INTERRUPT);

driver &g_uart_driver_uart0 = dev0_driver;
driver &g_uart_driver_uart1 = dev1_driver;
driver &g_uart_driver_uart2 = dev2_driver;
