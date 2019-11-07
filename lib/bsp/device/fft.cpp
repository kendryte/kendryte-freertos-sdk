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
#include <fft.h>
#include <hal.h>
#include <kernel/driver_impl.hpp>
#include <sysctl.h>
#include <utility.h>

using namespace sys;

#define COMMON_ENTRY \
    semaphore_lock locker(free_mutex_);

class k_fft_driver : public fft_driver, public static_object, public free_object_access
{
public:
    k_fft_driver(uintptr_t base_addr, sysctl_clock_t clock)
        : fft_(*reinterpret_cast<volatile fft_t *>(base_addr)), clock_(clock)
    {
    }

    virtual void install() override
    {
        free_mutex_ = xSemaphoreCreateMutex();
        sysctl_clock_disable(clock_);
    }

    virtual void on_first_open() override
    {
        sysctl_clock_enable(clock_);

        fft_.intr_clear.fft_done_clear = 1;
        fft_.intr_mask.fft_done_mask = 0;
    }

    virtual void on_last_close() override
    {
        sysctl_clock_disable(clock_);
    }

    virtual void complex_uint16(uint16_t shift, fft_direction_t direction, const uint64_t *input, size_t point_num, uint64_t *output) override
    {
        fft_point_t point = FFT_512;
        switch (point_num)
        {
        case 512:
            point = FFT_512;
            break;
        case 256:
            point = FFT_256;
            break;
        case 128:
            point = FFT_128;
            break;
        case 64:
            point = FFT_64;
            break;
        default:
            configASSERT(!"Invalid fft point");
            break;
        }

        fft_fft_ctrl_t ctl;
        ctl.data = fft_.fft_ctrl.data;
        ctl.dma_send = 1;
        ctl.fft_input_mode = 0;
        ctl.fft_data_mode = 0;
        ctl.fft_point = point;
        ctl.fft_mode = direction;
        ctl.fft_shift = shift;
        ctl.fft_enable = 1;
        fft_.fft_ctrl.data = ctl.data;

        uintptr_t dma_write = dma_open_free();
        uintptr_t dma_read = dma_open_free();
        dma_set_request_source(dma_write, SYSCTL_DMA_SELECT_FFT_TX_REQ);
        dma_set_request_source(dma_read, SYSCTL_DMA_SELECT_FFT_RX_REQ);
        SemaphoreHandle_t event_read = xSemaphoreCreateBinary(), event_write = xSemaphoreCreateBinary();
        dma_transmit_async(dma_read, &fft_.fft_output_fifo, output, 0, 1, sizeof(uint64_t), point_num >> 1, 4, event_read);
        dma_transmit_async(dma_write, input, &fft_.fft_input_fifo, 1, 0, sizeof(uint64_t), point_num >> 1, 4, event_write);
        configASSERT(xSemaphoreTake(event_read, portMAX_DELAY) == pdTRUE && xSemaphoreTake(event_write, portMAX_DELAY) == pdTRUE);

        dma_close(dma_write);
        dma_close(dma_read);
        vSemaphoreDelete(event_read);
        vSemaphoreDelete(event_write);
    }

private:
    volatile fft_t &fft_;
    sysctl_clock_t clock_;
    SemaphoreHandle_t free_mutex_;
};

static k_fft_driver dev0_driver(FFT_BASE_ADDR, SYSCTL_CLOCK_FFT);

driver &g_fft_driver_fft0 = dev0_driver;
