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
#ifndef _SHA256_H
#define _SHA256_H
#include <stdint.h>
#include <stddef.h>
#include <platform.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ENABLE_SHA          (0x1)
#define SHA256_BIG_ENDIAN   (0x1)

#define SHA256_HASH_LEN     32L
#define SHA256_HASH_WORDS   8L
#define SHA256_BLOCK_LEN    64L

typedef struct _sha_num_reg
{
    /* The total amount of data calculated by SHA256 is set by this register, and the smallest unit is 512bit. */
    uint32_t sha_data_cnt : 16;
    /* currently calculated block number. 512bit=1block*/
    uint32_t sha_data_num : 16;
} __attribute__((packed, aligned(4))) sha_num_reg_t;

typedef struct _sha_function_reg_0
{
    /* write:SHA256 enable register. read:Calculation completed flag  */
    uint32_t sha_en : 1;
    uint32_t reserved00 : 7;
    /* SHA256 calculation overflow flag */
    uint32_t sha_overflow : 1;
    uint32_t reserved01 : 7;
    /* Endian setting; b'0:little endian b'1:big endian */
    uint32_t sha_endian : 1;
    uint32_t reserved02 : 15;
} __attribute__((packed, aligned(4))) sha_function_reg_0_t;

typedef struct _sha_function_reg_1
{
    /* Sha and DMA handshake signals enable.b'1:enable;b'0:disable */
    uint32_t dma_en : 1;
    uint32_t reserved10 : 7;
    /* b'1:sha256 fifo is full; b'0:not full */
    uint32_t fifo_in_full : 1;
    uint32_t reserved11 : 23;
} __attribute__((packed, aligned(4))) sha_function_reg_1_t;

typedef struct _sha256
{
    /* Calculated sha256 return value. */
    uint32_t sha_result[8];
    /* SHA256 input data from this register. */
    uint32_t sha_data_in1;
    uint32_t reselved0;
    sha_num_reg_t sha_num_reg;
    sha_function_reg_0_t sha_function_reg_0;
    uint32_t reserved1;
    sha_function_reg_1_t sha_function_reg_1;
} __attribute__((packed, aligned(4))) sha256_t;

typedef struct _sha256_context
{
    size_t total_len;
    size_t dma_buf_len;
    uint32_t *dma_buf;
    size_t buffer_len;
    union
    {
        uint32_t words[16];
        uint8_t bytes[64];
    } buffer;
} sha256_context_t;

#ifdef __cplusplus
}
#endif

#endif

