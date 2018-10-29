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

/**
 * @file
 * @brief       Universal Asynchronous I2S_RECEIVER/I2S_TRANSMITTER (UART)
 *
 *              The UART peripheral supports the following features:
 *
 *              - 8-N-1 and 8-N-2 formats: 8 data bits, no parity bit, 1 start
 *                bit, 1 or 2 stop bits
 *
 *              - 8-entry transmit and receive FIFO buffers with programmable
 *                watermark interrupts
 *
 *              - 16× Rx oversampling with 2/3 majority voting per bit
 *
 *              The UART peripheral does not support hardware flow control or
 *              other modem control signals, or synchronous serial data
 *              tranfesrs.
 *
 *
 */

#ifndef _DRIVER_APBUART_H
#define _DRIVER_APBUART_H

#include <stdint.h>
#include <platform.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _uart
{
    union
    {
        uint32_t RBR;
        uint32_t DLL;
        uint32_t THR;
    };

    union
    {
        uint32_t DLH;
        uint32_t IER;
    };

    union
    {
        uint32_t FCR;
        uint32_t IIR;
    };

    uint32_t LCR;
    uint32_t MCR;
    uint32_t LSR;
    uint32_t MSR;
    uint32_t SCR;
    uint32_t LPDLL;
    uint32_t LPDLH;
    uint32_t reserve[18];
    uint32_t FAR;
    uint32_t TFR;
    uint32_t RFW;
    uint32_t USR;
    uint32_t TFL;
    uint32_t RFL;
    uint32_t SRR;
    uint32_t SRTS;
    uint32_t SBCR;
    uint32_t SDMAM;
    uint32_t SFE;
    uint32_t SRT;
    uint32_t STET;
    uint32_t HTX;
    uint32_t DMASA;
    uint32_t TCR;
    uint32_t DE_EN;
    uint32_t RE_EN;
    uint32_t DET;
    uint32_t TAT;
    uint32_t DLF;
    uint32_t RAR;
    uint32_t TAR;
    uint32_t LCR_EXT;
    uint32_t R[5];
    uint32_t CPR;
    uint32_t UCV;
    uint32_t CTR;
} uart_t;

#ifdef __cplusplus
}
#endif

#endif /* _DRIVER_APBUART_H */
