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
#include <fft.h>
#include <sysctl.h>

#define COMMON_ENTRY                                    \
    fft_dev_data *data = (fft_dev_data *)userdata;      \
    volatile fft_t *fft = (volatile fft_t *)data->base_addr;

typedef struct
{
    sysctl_clock_t clock;
    uintptr_t base_addr;
    SemaphoreHandle_t free_mutex;
} fft_dev_data;

static void fft_install(void *userdata)
{
    COMMON_ENTRY;
    sysctl_clock_enable(data->clock);
    data->free_mutex = xSemaphoreCreateMutex();
    fft->intr_clear.fft_done_clear = 1;
    fft->intr_mask.fft_done_mask = 0;
}

static int fft_open(void *userdata)
{
    return 1;
}

static void fft_close(void *userdata)
{
}

static void entry_exclusive(fft_dev_data *data)
{
    configASSERT(xSemaphoreTake(data->free_mutex, portMAX_DELAY) == pdTRUE);
}

static void exit_exclusive(fft_dev_data *data)
{
    xSemaphoreGive(data->free_mutex);
}

static void fft_complex_uint16(uint16_t shift, fft_direction_t direction, const uint64_t *input, size_t point_num, uint64_t *output, void *userdata)
{
    COMMON_ENTRY;
    entry_exclusive(data);
    fft_point_t point = FFT_512;
    switch(point_num)
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
    fft_fft_ctrl_t ctl = fft->fft_ctrl;
    ctl.dma_send = 1;
    ctl.fft_input_mode = 0;
    ctl.fft_data_mode = 0;
    ctl.fft_point = point;
    ctl.fft_mode = direction;
    ctl.fft_shift = shift;
    ctl.fft_enable = 1;
    fft->fft_ctrl = ctl;

    uintptr_t dma_write = dma_open_free();
    uintptr_t dma_read = dma_open_free();
    dma_set_request_source(dma_write, SYSCTL_DMA_SELECT_FFT_TX_REQ);
    dma_set_request_source(dma_read, SYSCTL_DMA_SELECT_FFT_RX_REQ);
    SemaphoreHandle_t event_read = xSemaphoreCreateBinary(), event_write = xSemaphoreCreateBinary();
    dma_transmit_async(dma_read, &fft->fft_output_fifo, output, 0, 1, sizeof(uint64_t), point_num>>1, 4, event_read);
    dma_transmit_async(dma_write, input, &fft->fft_input_fifo, 1, 0, sizeof(uint64_t), point_num>>1, 4, event_write);
    configASSERT(xSemaphoreTake(event_read, portMAX_DELAY) == pdTRUE && xSemaphoreTake(event_write, portMAX_DELAY) == pdTRUE);

    dma_close(dma_write);
    dma_close(dma_read);
    vSemaphoreDelete(event_read);
    vSemaphoreDelete(event_write);

    exit_exclusive(data);
}

static fft_dev_data dev0_data = {SYSCTL_CLOCK_FFT, FFT_BASE_ADDR, 0};

const fft_driver_t g_fft_driver_fft0 = {{&dev0_data, fft_install, fft_open, fft_close}, fft_complex_uint16};
