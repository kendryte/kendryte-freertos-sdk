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
#ifndef _DRIVER_TIMER_H
#define _DRIVER_TIMER_H

#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif

/* clang-format off */
typedef struct _timer_channel
{
    /* TIMER_N Load Count Register              (0x00+(N-1)*0x14) */
    volatile uint32_t load_count;
    /* TIMER_N Current Value Register           (0x04+(N-1)*0x14) */
    volatile uint32_t current_value;
    /* TIMER_N Control Register                 (0x08+(N-1)*0x14) */
    volatile uint32_t control;
    /* TIMER_N Interrupt Clear Register         (0x0c+(N-1)*0x14) */
    volatile uint32_t eoi;
    /* TIMER_N Interrupt Status Register        (0x10+(N-1)*0x14) */
    volatile uint32_t intr_stat;
} __attribute__((packed, aligned(4))) timer_channel_t;

typedef struct _kendryte_timer
{
    /* TIMER_N Register                         (0x00-0x4c) */
    volatile timer_channel_t channel[4];
    /* reserverd                                (0x50-0x9c) */
    volatile uint32_t resv1[20];
    /* TIMER Interrupt Status Register          (0xa0) */
    volatile uint32_t intr_stat;
    /* TIMER Interrupt Clear Register           (0xa4) */
    volatile uint32_t eoi;
    /* TIMER Raw Interrupt Status Register      (0xa8) */
    volatile uint32_t raw_intr_stat;
    /* TIMER Component Version Register         (0xac) */
    volatile uint32_t comp_version;
    /* TIMER_N Load Count2 Register             (0xb0-0xbc) */
    volatile uint32_t load_count2[4];
} __attribute__((packed, aligned(4))) kendryte_timer_t;

/* TIMER Control Register */
#define TIMER_CR_ENABLE             0x00000001
#define TIMER_CR_MODE_MASK          0x00000002
#define TIMER_CR_FREE_MODE          0x00000000
#define TIMER_CR_USER_MODE          0x00000002
#define TIMER_CR_INTERRUPT_MASK     0x00000004
#define TIMER_CR_PWM_ENABLE         0x00000008
/* clang-format on */

#ifdef __cplusplus
}
#endif

#endif /* _DRIVER_TIMER_H */
