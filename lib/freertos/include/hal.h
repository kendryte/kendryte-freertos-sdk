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
#ifndef _FREERTOS_HAL_H
#define _FREERTOS_HAL_H

#include <stddef.h>
#include <stdint.h>
#include "osdefs.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief       Set frequency of CPU
 * @param[in]   frequency       The desired frequency in Hz
 *
 * @return      The actual frequency of CPU after set
 */
uint32_t system_set_cpu_frequency(uint32_t frequency);

/**
 * @brief       Enable or disable IRQ
 * @param[in]   irq         IRQ number
 * @param[in]   enable      1 is enable, 0 is disable
 */
void pic_set_irq_enable(uint32_t irq, bool enable);

/**
 * @brief       Set handler of IRQ
 * @param[in]   irq             IRQ number
 * @param[in]   handler         The handler function
 * @param[in]   userdata        The userdata of the handler function
 */
void pic_set_irq_handler(uint32_t irq, pic_irq_handler_t handler, void *userdata);

/**
 * @brief       Set priority of IRQ
 * @param[in]   irq             IRQ number
 * @param[in]   priority        The priority of IRQ
 */
void pic_set_irq_priority(uint32_t irq, uint32_t priority);

/**
 * @brief       Wait for a free DMA and open it
 *
 * @return      The DMA handle
 */
handle_t dma_open_free();

/**
 * @brief       Close DMA
 * @param[in]   file        The DMA handle
 */
void dma_close(handle_t file);

/**
 * @brief       Set the request source of DMA
 * @param[in]   file        The DMA handle
 * @param[in]   request     The request source number
 */
void dma_set_request_source(handle_t file, uint32_t request);

/**
 * @brief       DMA asynchronously
 * @param[in]   file                    The DMA handle
 * @param[in]   src                     The address of source
 * @param[out]  dest                    The address of destination
 * @param[in]   src_inc                 Enable increment of source address
 * @param[in]   dest_inc                Enable increment of destination address
 * @param[in]   element_size            Element size in bytes
 * @param[in]   count                   Element count to transmit
 * @param[in]   burst_size              Element count to transmit per request
 * @param[in]   completion_event        Event to signal when this transmition is completed
 */
void dma_transmit_async(handle_t file, const volatile void *src, volatile void *dest, bool src_inc, bool dest_inc, size_t element_size, size_t count, size_t burst_size, SemaphoreHandle_t completion_event);

/**
 * @brief       DMA synchrnonously
 * @param[in]   file                The DMA handle
 * @param[in]   src                 The address of source
 * @param[out]  dest                The address of destination
 * @param[in]   src_inc             Enable increment of source address
 * @param[in]   dest_inc            Enable increment of destination address
 * @param[in]   element_size        Element size in bytes
 * @param[in]   count               Element count to transmit
 * @param[in]   burst_size          Element count to transmit per request
 */
void dma_transmit(handle_t file, const volatile void *src, volatile void *dest, bool src_inc, bool dest_inc, size_t element_size, size_t count, size_t burst_size);

/**
 * @brief       DMA loop asynchronously
 * @param[in]   file                                The DMA handle
 * @param[in]   srcs                                The addresses of source
 * @param[in]   src_num                             The source addresses count
 * @param[out]  dests                               The addresses of destination
 * @param[in]   dest_num                            The destination addresses count
 * @param[in]   src_inc                             Enable increment of source address
 * @param[in]   dest_inc                            Enable increment of destination address
 * @param[in]   element_size                        Element size in bytes
 * @param[in]   count                               Element count to transmit in one loop
 * @param[in]   burst_size                          Element count to transmit per request
 * @param[in]   stage_completion_handler            The handler function when on loop is completed
 * @param[in]   stage_completion_handler_data       The userdata of the handler function
 * @param[in]   completion_event                    Event to signal when this transmition is completed
 * @param[in]   stop_signal                         The address of signal indicating whether to stop the transmition, set to 1 to stop
 */
void dma_loop_async(handle_t file, const volatile void **srcs, size_t src_num, volatile void **dests, size_t dest_num, bool src_inc, bool dest_inc, size_t element_size, size_t count, size_t burst_size, dma_stage_completion_handler_t stage_completion_handler, void *stage_completion_handler_data, SemaphoreHandle_t completion_event, int *stop_signal);

void dma_stop(handle_t file);
#ifdef __cplusplus
}
#endif

#endif /* _FREERTOS_HAL_H */
