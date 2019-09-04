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
#include <string.h>
#include <sysctl.h>
#include <uart.h>

using namespace sys;

#define UART_BRATE_CONST 16
#define RINGBUFF_LEN 64
#define UART_BUFFER_COUNT 2

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
    k_uart_driver(uintptr_t base_addr, sysctl_clock_t clock, plic_irq_t irq, sysctl_dma_select_t dma_req)
        : uart_(*reinterpret_cast<volatile uart_t *>(base_addr)), clock_(clock), irq_(irq), dma_req_(dma_req)
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

    virtual void config_use_dma(size_t buffer_size, int use_dma) override
    {
        if(use_dma)
        {
            use_dma_ = 1;

            session_.buffer_ptr = 0;
            session_.next_free_buffer = 0;
            session_.dma_in_use_buffer = 0;
            free(session_.buffer);
            session_.buffer_size = buffer_size * sizeof(uint32_t);
            session_.buffer = (uint8_t *)malloc(session_.buffer_size * UART_BUFFER_COUNT);
            memset(session_.buffer, 0, session_.buffer_size);
            configASSERT(!session_.transmit_dma);

            session_.stop_signal = 0;
            session_.transmit_dma = dma_open_free();
            dma_set_request_source(session_.transmit_dma, dma_req_);
            session_.stage_completion_event = xSemaphoreCreateCounting(100, 0);
            session_.completion_event = xSemaphoreCreateBinary();

            const volatile void *srcs[1] = {
                &uart_.RBR
            };

            volatile void *dests[UART_BUFFER_COUNT] = {
                session_.buffer,
                session_.buffer + session_.buffer_size
            };

            dma_loop_async(session_.transmit_dma, srcs, 1, dests, UART_BUFFER_COUNT, 0, 1, sizeof(uint32_t), session_.buffer_size >> 2, 1, uart_stage_completion_isr, this, session_.completion_event, &session_.stop_signal);
        }
        else
        {
            use_dma_ = 0;
            dma_stop(session_.transmit_dma);
            configASSERT(xSemaphoreTake(session_.completion_event, portMAX_DELAY) == pdTRUE);
            dma_close(session_.transmit_dma);
            session_.transmit_dma = 0;
            vSemaphoreDelete(session_.stage_completion_event);
            vSemaphoreDelete(session_.completion_event);
            free(session_.buffer);
        }
    }

    virtual int read(gsl::span<uint8_t> buffer) override
    {
        if(use_dma_)
        {
            int next_free_buffer = session_.next_free_buffer;
            while (next_free_buffer == session_.dma_in_use_buffer)
            {
                xSemaphoreTake(session_.stage_completion_event, portMAX_DELAY);
                next_free_buffer = session_.next_free_buffer;
            }
            configASSERT(session_.buffer_ptr + buffer.size() * sizeof(uint32_t) <= session_.buffer_size);
            uint32_t *v_recv_buf = (uint32_t *)(session_.buffer + session_.buffer_size * next_free_buffer + session_.buffer_ptr);
            for(uint32_t i = 0; i < buffer.size(); i++)
            {
                buffer.data()[i] = (uint8_t)(v_recv_buf[i] & 0xff);
            }

            session_.buffer_ptr += buffer.size() * sizeof(uint32_t);
            if (session_.buffer_ptr >= session_.buffer_size)
            {
                session_.buffer_ptr = 0;
                int next_free_buffer = session_.next_free_buffer + 1;
                if (next_free_buffer == UART_BUFFER_COUNT)
                    next_free_buffer = 0;
                session_.next_free_buffer = next_free_buffer;
            }
            return buffer.size();
        }
        else
            return read_ringbuff(buffer.data(), buffer.size());
    }

    virtual int write(gsl::span<const uint8_t> buffer) override
    {
        if(use_dma_)
        {
            uint32_t *v_send_buf = (uint32_t *)malloc(buffer.size() * sizeof(uint32_t));
            configASSERT(v_send_buf!=NULL);
            for(uint32_t i = 0; i < buffer.size(); i++)
                v_send_buf[i] = buffer.data()[i];
            uintptr_t dma_write = dma_open_free();
            dma_set_request_source(dma_write, dma_req_ + 1);

            SemaphoreHandle_t event_write = xSemaphoreCreateBinary();
            dma_transmit_async(dma_write, v_send_buf, &uart_.THR, 1, 0, sizeof(uint32_t), buffer.size(), 1, event_write);

            configASSERT(xSemaphoreTake(event_write, portMAX_DELAY) == pdTRUE);
            dma_close(dma_write);
            vSemaphoreDelete(event_write);
            free((void *)v_send_buf);
            return buffer.size();
        }
        else
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
    }

    virtual void set_read_timeout(size_t millisecond) override
    {
        read_timeout_ = millisecond / portTICK_PERIOD_MS;
    }

private:
    static void uart_stage_completion_isr(void *userdata)
    {
        auto &driver = *reinterpret_cast<k_uart_driver *>(userdata);

        int dma_in_use_buffer = driver.session_.dma_in_use_buffer;

        dma_in_use_buffer++;
        if (dma_in_use_buffer == UART_BUFFER_COUNT)
            dma_in_use_buffer = 0;
        driver.session_.dma_in_use_buffer = dma_in_use_buffer;

        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xSemaphoreGiveFromISR(driver.session_.stage_completion_event, &xHigherPriorityTaskWoken);
        if (xHigherPriorityTaskWoken)
            portYIELD_FROM_ISR();
    }

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
    sysctl_dma_select_t dma_req_;
    SemaphoreHandle_t receive_event_;
    uint8_t use_dma_;
    ringbuffer_t *recv_buf_;
    size_t read_timeout_ = portMAX_DELAY;

    struct
    {
        uint8_t *buffer;
        size_t buffer_size;
        size_t buffer_ptr;
        volatile int next_free_buffer;
        volatile int dma_in_use_buffer;
        int stop_signal;
        handle_t transmit_dma;
        SemaphoreHandle_t stage_completion_event;
        SemaphoreHandle_t completion_event;
    } session_;

};

static k_uart_driver dev0_driver(UART1_BASE_ADDR, SYSCTL_CLOCK_UART1, IRQN_UART1_INTERRUPT, SYSCTL_DMA_SELECT_UART1_RX_REQ);
static k_uart_driver dev1_driver(UART2_BASE_ADDR, SYSCTL_CLOCK_UART2, IRQN_UART2_INTERRUPT, SYSCTL_DMA_SELECT_UART2_RX_REQ);
static k_uart_driver dev2_driver(UART3_BASE_ADDR, SYSCTL_CLOCK_UART3, IRQN_UART3_INTERRUPT, SYSCTL_DMA_SELECT_UART3_RX_REQ);

driver &g_uart_driver_uart0 = dev0_driver;
driver &g_uart_driver_uart1 = dev1_driver;
driver &g_uart_driver_uart2 = dev2_driver;
