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
 * @brief      Field Programmable GPIO Array (FPIOA)
 *
 *             The FPIOA peripheral supports the following features:
 *
 *             - 48 IO with 256 functions
 *
 *             - Schmitt trigger
 *
 *             - Invert input and output
 *
 *             - Pull up and pull down
 *
 *             - Driving selector
 *
 *             - Static input and output
 *
 */

#ifndef _DRIVER_FPIOA_H
#define _DRIVER_FPIOA_H

#include <stdint.h>
#include <platform.h>

#ifdef __cplusplus
extern "C" {
#endif

/* clang-format off */
/* Pad number settings */
#define FPIOA_NUM_IO    (48)
/* clang-format on */

/**
 * @brief      FPIOA pull settings
 *
 * @note       FPIOA pull settings description
 *
 * | PU  | PD  | Description                       |
 * |-----|-----|-----------------------------------|
 * | 0   | 0   | No Pull                           |
 * | 0   | 1   | Pull Down                         |
 * | 1   | 0   | Pull Up                           |
 * | 1   | 1   | Undefined                         |
 *
 */

/* clang-format off */
typedef enum _fpioa_pull
{
    FPIOA_PULL_NONE,      /*!< No Pull */
    FPIOA_PULL_DOWN,      /*!< Pull Down */
    FPIOA_PULL_UP,        /*!< Pull Up */
    FPIOA_PULL_MAX        /*!< Count of pull settings */
} fpioa_pull_t;
/* clang-format on */

/**
 * @brief      FPIOA driving settings
 *
 * @note       FPIOA driving settings description
 *             There are 16 kinds of driving settings
 *
 * @note       Low Level Output Current
 *
 * |DS[3:0] |Min(mA)|Typ(mA)|Max(mA)|
 * |--------|-------|-------|-------|
 * |0000    |3.2    |5.4    |8.3    |
 * |0001    |4.7    |8.0    |12.3   |
 * |0010    |6.3    |10.7   |16.4   |
 * |0011    |7.8    |13.2   |20.2   |
 * |0100    |9.4    |15.9   |24.2   |
 * |0101    |10.9   |18.4   |28.1   |
 * |0110    |12.4   |20.9   |31.8   |
 * |0111    |13.9   |23.4   |35.5   |
 *
 * @note       High Level Output Current
 *
 * |DS[3:0] |Min(mA)|Typ(mA)|Max(mA)|
 * |--------|-------|-------|-------|
 * |0000    |5.0    |7.6    |11.2   |
 * |0001    |7.5    |11.4   |16.8   |
 * |0010    |10.0   |15.2   |22.3   |
 * |0011    |12.4   |18.9   |27.8   |
 * |0100    |14.9   |22.6   |33.3   |
 * |0101    |17.4   |26.3   |38.7   |
 * |0110    |19.8   |30.0   |44.1   |
 * |0111    |22.3   |33.7   |49.5   |
 *
 */

/* clang-format off */
typedef enum _fpioa_driving
{
    FPIOA_DRIVING_0,      /*!< 0000 */
    FPIOA_DRIVING_1,      /*!< 0001 */
    FPIOA_DRIVING_2,      /*!< 0010 */
    FPIOA_DRIVING_3,      /*!< 0011 */
    FPIOA_DRIVING_4,      /*!< 0100 */
    FPIOA_DRIVING_5,      /*!< 0101 */
    FPIOA_DRIVING_6,      /*!< 0110 */
    FPIOA_DRIVING_7,      /*!< 0111 */
    FPIOA_DRIVING_8,      /*!< 1000 */
    FPIOA_DRIVING_9,      /*!< 1001 */
    FPIOA_DRIVING_10,     /*!< 1010 */
    FPIOA_DRIVING_11,     /*!< 1011 */
    FPIOA_DRIVING_12,     /*!< 1100 */
    FPIOA_DRIVING_13,     /*!< 1101 */
    FPIOA_DRIVING_14,     /*!< 1110 */
    FPIOA_DRIVING_15,     /*!< 1111 */
    FPIOA_DRIVING_MAX     /*!< Count of driving settings */
} fpioa_driving_t;
/* clang-format on */

/**
 * @brief      FPIOA IO
 *
 *             FPIOA IO is the specific pin of the chip package. Every IO
 *             has a 32bit width register that can independently implement
 *             schmitt trigger, invert input, invert output, strong pull
 *             up, driving selector, static input and static output. And more,
 *             it can implement any pin of any peripheral devices.
 *
 * @note       FPIOA IO's register bits Layout
 *
 * | Bits      | Name     |Description                                        |
 * |-----------|----------|---------------------------------------------------|
 * | 31        | PAD_DI   | Read current IO's data input.                     |
 * | 30:24     | NA       | Reserved bits.                                    |
 * | 23        | ST       | Schmitt trigger.                                  |
 * | 22        | DI_INV   | Invert Data input.                                |
 * | 21        | IE_INV   | Invert the input enable signal.                   |
 * | 20        | IE_EN    | Input enable. It can disable or enable IO input.  |
 * | 19        | SL       | Slew rate control enable.                         |
 * | 18        | SPU      | Strong pull up.                                   |
 * | 17        | PD       | Pull select: 0 for pull down, 1 for pull up.      |
 * | 16        | PU       | Pull enable.                                      |
 * | 15        | DO_INV   | Invert the result of data output select (DO_SEL). |
 * | 14        | DO_SEL   | Data output select: 0 for DO, 1 for OE.           |
 * | 13        | OE_INV   | Invert the output enable signal.                  |
 * | 12        | OE_EN    | Output enable.It can disable or enable IO output. |
 * | 11:8      | DS       | Driving selector.                                 |
 * | 7:0       | CH_SEL   | Channel select from 256 input.                    |
 *
 */
typedef struct _fpioa_io_config
{
    uint32_t ch_sel : 8;
    /*!< Channel select from 256 input. */
    uint32_t ds : 4;
    /*!< Driving selector. */
    uint32_t oe_en : 1;
    /*!< Static output enable, will AND with OE_INV. */
    uint32_t oe_inv : 1;
    /*!< Invert output enable. */
    uint32_t do_sel : 1;
    /*!< Data output select: 0 for DO, 1 for OE. */
    uint32_t do_inv : 1;
    /*!< Invert the result of data output select (DO_SEL). */
    uint32_t pu : 1;
    /*!< Pull up enable. 0 for nothing, 1 for pull up. */
    uint32_t pd : 1;
    /*!< Pull down enable. 0 for nothing, 1 for pull down. */
    uint32_t resv0 : 1;
    /*!< Reserved bits. */
    uint32_t sl : 1;
    /*!< Slew rate control enable. */
    uint32_t ie_en : 1;
    /*!< Static input enable, will AND with IE_INV. */
    uint32_t ie_inv : 1;
    /*!< Invert input enable. */
    uint32_t di_inv : 1;
    /*!< Invert Data input. */
    uint32_t st : 1;
    /*!< Schmitt trigger. */
    uint32_t resv1 : 7;
    /*!< Reserved bits. */
    uint32_t pad_di : 1;
    /*!< Read current IO's data input. */
} __attribute__((packed, aligned(4))) fpioa_io_config_t;

/**
 * @brief      FPIOA tie setting
 *
 *             FPIOA Object have 48 IO pin object and 256 bit input tie bits.
 *             All SPI arbitration signal will tie high by default.
 *
 * @note       FPIOA function tie bits RAM Layout
 *
 * | Address   | Name             |Description                       |
 * |-----------|------------------|----------------------------------|
 * | 0x000     | TIE_EN[31:0]     | Input tie enable bits [31:0]     |
 * | 0x004     | TIE_EN[63:32]    | Input tie enable bits [63:32]    |
 * | 0x008     | TIE_EN[95:64]    | Input tie enable bits [95:64]    |
 * | 0x00C     | TIE_EN[127:96]   | Input tie enable bits [127:96]   |
 * | 0x010     | TIE_EN[159:128]  | Input tie enable bits [159:128]  |
 * | 0x014     | TIE_EN[191:160]  | Input tie enable bits [191:160]  |
 * | 0x018     | TIE_EN[223:192]  | Input tie enable bits [223:192]  |
 * | 0x01C     | TIE_EN[255:224]  | Input tie enable bits [255:224]  |
 * | 0x020     | TIE_VAL[31:0]    | Input tie value bits [31:0]      |
 * | 0x024     | TIE_VAL[63:32]   | Input tie value bits [63:32]     |
 * | 0x028     | TIE_VAL[95:64]   | Input tie value bits [95:64]     |
 * | 0x02C     | TIE_VAL[127:96]  | Input tie value bits [127:96]    |
 * | 0x030     | TIE_VAL[159:128] | Input tie value bits [159:128]   |
 * | 0x034     | TIE_VAL[191:160] | Input tie value bits [191:160]   |
 * | 0x038     | TIE_VAL[223:192] | Input tie value bits [223:192]   |
 * | 0x03C     | TIE_VAL[255:224] | Input tie value bits [255:224]   |
 *
 * @note       Function which input tie high by default
 *
 * | Name          |Description                            |
 * |---------------|---------------------------------------|
 * | SPI0_ARB      | Arbitration function of SPI master 0  |
 * | SPI1_ARB      | Arbitration function of SPI master 1  |
 *
 *             Tie high means the SPI Arbitration input is 1
 *
 */
typedef struct _fpioa_tie
{
    uint32_t en[FUNC_MAX / 32];
    /*!< FPIOA GPIO multiplexer tie enable array */
    uint32_t val[FUNC_MAX / 32];
    /*!< FPIOA GPIO multiplexer tie value array */
} __attribute__((packed, aligned(4))) fpioa_tie_t;

/**
 * @brief      FPIOA Object
 *
 *             FPIOA Object have 48 IO pin object and 256 bit input tie bits.
 *             All SPI arbitration signal will tie high by default.
 *
 * @note       FPIOA IO Pin RAM Layout
 *
 * | Address   | Name     |Description                     |
 * |-----------|----------|--------------------------------|
 * | 0x000     | PAD0     | FPIOA GPIO multiplexer io 0    |
 * | 0x004     | PAD1     | FPIOA GPIO multiplexer io 1    |
 * | 0x008     | PAD2     | FPIOA GPIO multiplexer io 2    |
 * | 0x00C     | PAD3     | FPIOA GPIO multiplexer io 3    |
 * | 0x010     | PAD4     | FPIOA GPIO multiplexer io 4    |
 * | 0x014     | PAD5     | FPIOA GPIO multiplexer io 5    |
 * | 0x018     | PAD6     | FPIOA GPIO multiplexer io 6    |
 * | 0x01C     | PAD7     | FPIOA GPIO multiplexer io 7    |
 * | 0x020     | PAD8     | FPIOA GPIO multiplexer io 8    |
 * | 0x024     | PAD9     | FPIOA GPIO multiplexer io 9    |
 * | 0x028     | PAD10    | FPIOA GPIO multiplexer io 10   |
 * | 0x02C     | PAD11    | FPIOA GPIO multiplexer io 11   |
 * | 0x030     | PAD12    | FPIOA GPIO multiplexer io 12   |
 * | 0x034     | PAD13    | FPIOA GPIO multiplexer io 13   |
 * | 0x038     | PAD14    | FPIOA GPIO multiplexer io 14   |
 * | 0x03C     | PAD15    | FPIOA GPIO multiplexer io 15   |
 * | 0x040     | PAD16    | FPIOA GPIO multiplexer io 16   |
 * | 0x044     | PAD17    | FPIOA GPIO multiplexer io 17   |
 * | 0x048     | PAD18    | FPIOA GPIO multiplexer io 18   |
 * | 0x04C     | PAD19    | FPIOA GPIO multiplexer io 19   |
 * | 0x050     | PAD20    | FPIOA GPIO multiplexer io 20   |
 * | 0x054     | PAD21    | FPIOA GPIO multiplexer io 21   |
 * | 0x058     | PAD22    | FPIOA GPIO multiplexer io 22   |
 * | 0x05C     | PAD23    | FPIOA GPIO multiplexer io 23   |
 * | 0x060     | PAD24    | FPIOA GPIO multiplexer io 24   |
 * | 0x064     | PAD25    | FPIOA GPIO multiplexer io 25   |
 * | 0x068     | PAD26    | FPIOA GPIO multiplexer io 26   |
 * | 0x06C     | PAD27    | FPIOA GPIO multiplexer io 27   |
 * | 0x070     | PAD28    | FPIOA GPIO multiplexer io 28   |
 * | 0x074     | PAD29    | FPIOA GPIO multiplexer io 29   |
 * | 0x078     | PAD30    | FPIOA GPIO multiplexer io 30   |
 * | 0x07C     | PAD31    | FPIOA GPIO multiplexer io 31   |
 * | 0x080     | PAD32    | FPIOA GPIO multiplexer io 32   |
 * | 0x084     | PAD33    | FPIOA GPIO multiplexer io 33   |
 * | 0x088     | PAD34    | FPIOA GPIO multiplexer io 34   |
 * | 0x08C     | PAD35    | FPIOA GPIO multiplexer io 35   |
 * | 0x090     | PAD36    | FPIOA GPIO multiplexer io 36   |
 * | 0x094     | PAD37    | FPIOA GPIO multiplexer io 37   |
 * | 0x098     | PAD38    | FPIOA GPIO multiplexer io 38   |
 * | 0x09C     | PAD39    | FPIOA GPIO multiplexer io 39   |
 * | 0x0A0     | PAD40    | FPIOA GPIO multiplexer io 40   |
 * | 0x0A4     | PAD41    | FPIOA GPIO multiplexer io 41   |
 * | 0x0A8     | PAD42    | FPIOA GPIO multiplexer io 42   |
 * | 0x0AC     | PAD43    | FPIOA GPIO multiplexer io 43   |
 * | 0x0B0     | PAD44    | FPIOA GPIO multiplexer io 44   |
 * | 0x0B4     | PAD45    | FPIOA GPIO multiplexer io 45   |
 * | 0x0B8     | PAD46    | FPIOA GPIO multiplexer io 46   |
 * | 0x0BC     | PAD47    | FPIOA GPIO multiplexer io 47   |
 *
 */
typedef struct _fpioa
{
    fpioa_io_config_t io[FPIOA_NUM_IO];
    /*!< FPIOA GPIO multiplexer io array */
    fpioa_tie_t tie;
    /*!< FPIOA GPIO multiplexer tie */
} __attribute__((packed, aligned(4))) fpioa_t;

/**
 * @brief       FPIOA object instanse
 */
extern volatile fpioa_t *const fpioa;

/**
 * @brief       Initialize FPIOA user custom default settings
 *
 * @note        This function will set all FPIOA pad registers to user-defined
 *              values from kconfig
 *
 * @return      result
 *     - 0      Success
 *     - Other  Fail
 */
int fpioa_init(void);

/**
 * @brief       Get IO configuration
 *
 * @param[in]   number      The IO number
 * @param       cfg         Pointer to struct of IO configuration for specified IO
 *
 * @return      result
 *     - 0      Success
 *     - Other  Fail
 */
int fpioa_get_io(int number, fpioa_io_config_t *cfg);

/**
 * @brief       Set IO configuration
 *
 * @param[in]   number      The IO number
 * @param[in]   cfg         Pointer to struct of IO configuration for specified IO
 *
 * @return      result
 *     - 0      Success
 *     - Other  Fail
 */
int fpioa_set_io(int number, fpioa_io_config_t *cfg);

/**
 * @brief       Set IO configuration with function number
 *
 * @note        The default IO configuration which bind to function number will
 *              set automatically
 *
 * @param[in]   number      The IO number
 * @param[in]   function    The function enum number
 *
 * @return      result
 *     - 0      Success
 *     - Other  Fail
 */
int fpioa_set_function_raw(int number, fpioa_function_t function);

/**
 * @brief       Set only IO configuration with function number
 *
 * @note        The default IO configuration which bind to function number will
 *              set automatically
 *
 * @param[in]   number      The IO number
 * @param[in]   function    The function enum number
 *
 * @return      result
 *     - 0      Success
 *     - Other  Fail
 */
int fpioa_set_function(int number, fpioa_function_t function);

/**
 * @brief       Set tie enable to function
 *
 * @param[in]   function    The function enum number
 * @param[in]   enable      Tie enable to set, 1 is enable, 0 is disable
 *
 * @return      result
 *     - 0      Success
 *     - Other  Fail
 */
int fpioa_set_tie_enable(fpioa_function_t function, int enable);

/**
 * @brief       Set tie value to function
 *
 * @param[in]   function    The function enum number
 * @param[in]   value       Tie value to set, 1 is HIGH, 0 is LOW
 *
 * @return      result
 *     - 0      Success
 *     - Other  Fail
 */
int fpioa_set_tie_value(fpioa_function_t function, int value);

/**
 * @brief      Set IO pull function
 *
 * @param[in]   number  The IO number
 * @param[in]   pull    The pull enum number
 *
 * @return      result
 *     - 0      Success
 *     - Other  Fail
 */
int fpioa_set_io_pull(int number, fpioa_pull_t pull);

/**
 * @brief       Get IO pull function
 *
 * @param[in]   number  The IO number
 *
 * @return      result
 *     - -1     Fail
 *     - Other  The pull enum number
 */
int fpioa_get_io_pull(int number);

/**
 * @brief       Set IO driving
 *
 * @param[in]   number   The IO number
 * @param[in]   driving  The driving enum number
 *
 * @return      result
 *     - 0      Success
 *     - Other  Fail
 */
int fpioa_set_io_driving(int number, fpioa_driving_t driving);

/**
 * @brief       Get IO driving
 *
 * @param[in]   number  The IO number
 *
 * @return      result
 *     - -1     Fail
 *     - Other  The driving enum number
 */
int fpioa_get_io_driving(int number);

/**
 * @brief       Get IO by function
 *
 * @param[in]   function  The function enum number
 *
 * @return      result
 *     - -1     Fail
 *     - Other  The IO number
 */
int fpioa_get_io_by_function(fpioa_function_t function);

#ifdef __cplusplus
}
#endif

#endif /* _DRIVER_FPIOA_H */

