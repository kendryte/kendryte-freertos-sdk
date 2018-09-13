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
#include <hard_fft.h>
#include <io.h>
#include <plic.h>
#include <semphr.h>
#include <stdio.h>
#include <sysctl.h>

#define FFT_BASE_ADDR (0x42000000)
#define FFT_RESULT_ADDR (0x42100000 + 0x2000)

#define COMMON_ENTRY                              \
    fft_dev_data* data = (fft_dev_data*)userdata; \
    volatile fft_t* fft = (volatile fft_t*)data->base_addr;

typedef struct
{
    uintptr_t base_addr;
    SemaphoreHandle_t free_mutex;
} fft_dev_data;

static void fft_install(void* userdata)
{
    COMMON_ENTRY;
    sysctl_clock_enable(SYSCTL_CLOCK_FFT);
    sysctl_reset(SYSCTL_RESET_FFT);
    data->free_mutex = xSemaphoreCreateMutex();

    fft->intr_clear.fft_done_clear = 1;
    fft->intr_mask.fft_done_mask = 0;
}

static int fft_open(void* userdata)
{
    return 1;
}

static void fft_close(void* userdata)
{
}

static void entry_exclusive(fft_dev_data* data)
{
    configASSERT(xSemaphoreTake(data->free_mutex, portMAX_DELAY) == pdTRUE);
}

static void exit_exclusive(fft_dev_data* data)
{
    xSemaphoreGive(data->free_mutex);
}

static void fft_complex_uint16(fft_point point, fft_direction direction, uint32_t shifts_mask, const uint16_t* input, uint16_t* output, void* userdata)
{
    COMMON_ENTRY;
    configASSERT(((uintptr_t)input) % 8 == 0 && ((uintptr_t)output) % 8 == 0);
    configASSERT(shifts_mask <= 0x1ff);

    entry_exclusive(data);

    fft_fft_ctrl_t ctl = fft->fft_ctrl;
    ctl.dma_send = 1;
    ctl.fft_input_mode = 0;
    ctl.fft_data_mode = 0;
    ctl.fft_point = point;
    ctl.fft_mode = direction;
    ctl.fft_shift = shifts_mask;
    ctl.fft_enable = 1;
    fft->fft_ctrl = ctl;

    size_t blocks = 0;
    switch (point)
    {
    case FFT_512:
        blocks = 256;
        break;
    case FFT_256:
        blocks = 128;
        break;
    case FFT_128:
        blocks = 64;
        break;
    case FFT_64:
        blocks = 32;
        break;
    default:
        configASSERT(!"Invalid fft point");
        break;
    }

    uintptr_t dma_write = dma_open_free();
    uintptr_t dma_read = dma_open_free();

    dma_set_select_request(dma_write, SYSCTL_DMA_SELECT_FFT_TX_REQ);
    dma_set_select_request(dma_read, SYSCTL_DMA_SELECT_FFT_RX_REQ);

    SemaphoreHandle_t event_read = xSemaphoreCreateBinary(), event_write = xSemaphoreCreateBinary();

    dma_transmit_async(dma_read, &fft->fft_output_fifo, output, 0, 1, sizeof(uint64_t), blocks, 4, event_read);
    dma_transmit_async(dma_write, input, &fft->fft_input_fifo, 1, 0, sizeof(uint64_t), blocks, 4, event_write);

    configASSERT(xSemaphoreTake(event_read, portMAX_DELAY) == pdTRUE && xSemaphoreTake(event_write, portMAX_DELAY) == pdTRUE);

    dma_close(dma_write);
    dma_close(dma_read);
    vSemaphoreDelete(event_read);
    vSemaphoreDelete(event_write);

    exit_exclusive(data);
}

static fft_dev_data dev0_data = {FFT_BASE_ADDR, 0};

const fft_driver_t g_fft_driver_fft0 = {{&dev0_data, fft_install, fft_open, fft_close}, fft_complex_uint16};
