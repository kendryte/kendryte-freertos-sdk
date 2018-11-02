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
#ifndef _DRIVER_AES_H
#define _DRIVER_AES_H

#include <stdint.h>
#include <platform.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AES_TRANSMISSION_THRESHOLD  (896UL)
typedef enum _aes_iv_len
{
    IV_LEN_96 = 12,
    IV_LEN_128 = 16,
} aes_iv_len_t;

typedef struct _aes_mode_ctl
{
    /* [2:0]:000:ecb; 001:cbc,010:gcm */
    uint32_t cipher_mode : 3;
    /* [4:3]:00:aes-128; 01:aes-192; 10:aes-256;11:reserved*/
    uint32_t kmode : 2;
    /* [6:5]:input key order 1：little endian; 0: big endian */
    uint32_t key_order : 2;
    /* [8:7]:input data order 1：little endian; 0: big endian */
    uint32_t input_order : 2;
    /* [10:9]:output data order 1：little endian; 0: big endian */
    uint32_t output_order : 2;
    uint32_t reserved : 21;
} __attribute__((packed, aligned(4))) aes_mode_ctl_t;

/**
 * @brief       AES
 */
typedef struct _aes
{
    /* (0x00) customer key.1st~4th byte key */
    uint32_t aes_key[4];
    /* (0x10) 0: encryption; 1: decryption */
    uint32_t encrypt_sel;
    /* (0x14) aes mode reg */
    aes_mode_ctl_t mode_ctl;
    /* (0x18) Initialisation Vector. GCM support 96bit. CBC support 128bit */
    uint32_t aes_iv[4];
    /* (0x28) input data endian;1:little endian; 0:big endian */
    uint32_t aes_endian;
    /* (0x2c) calculate status. 1:finish; 0:not finish */
    uint32_t aes_finish;
    /* (0x30) aes out data to dma 0:cpu 1:dma */
    uint32_t dma_sel;
    /* (0x34) gcm Additional authenticated data number */
    uint32_t gb_aad_num;
    uint32_t reserved;
    /* (0x3c) aes plantext/ciphter text input data number */
    uint32_t gb_pc_num;
    /* (0x40) aes plantext/ciphter text input data */
    uint32_t aes_text_data;
    /* (0x44) Additional authenticated data */
    uint32_t aes_aad_data;
    /**
     * (0x48) [1:0],b'00:check not finish; b'01:check fail; b'10:check success;
     * b'11:reversed
     */
    uint32_t tag_chk;
    /* (0x4c) data can input flag. 1: data can input; 0 : data cannot input */
    uint32_t data_in_flag;
    /* (0x50) gcm input tag for compare with the calculate tag */
    uint32_t gcm_in_tag[4];
    /* (0x60) aes plantext/ciphter text output data */
    uint32_t aes_out_data;
    /* (0x64) aes module enable */
    uint32_t gb_aes_en;
    /* (0x68) data can output flag 1: data ready 0: data not ready */
    uint32_t data_out_flag;
    /* (0x6c) allow tag input when use gcm */
    uint32_t tag_in_flag;
    /* (0x70) clear tag_chk */
    uint32_t tag_clear;
    uint32_t gcm_out_tag[4];
    /* (0x84) customer key for aes-192 aes-256.5th~8th byte key */
    uint32_t aes_key_ext[4];
} __attribute__((packed, aligned(4))) aes_t;

#ifdef __cplusplus
}
#endif

#endif /* _DRIVER_AES_H */
