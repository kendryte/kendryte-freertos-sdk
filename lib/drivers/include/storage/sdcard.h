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
#ifndef _DRIVERS_SDCARD_H
#define _DRIVERS_SDCARD_H

#include <stdint.h>
#include <osdefs.h>

#ifdef __cplusplus
extern "C"
{
#endif

/*
 * @brief  Start Data tokens:
 *         Tokens (necessary because at nop/idle (and CS active) only 0xff is
 *         on the data/command line)
 */
#define SD_START_DATA_SINGLE_BLOCK_READ    0xFE  /*!< Data token start byte, Start Single Block Read */
#define SD_START_DATA_MULTIPLE_BLOCK_READ  0xFE  /*!< Data token start byte, Start Multiple Block Read */
#define SD_START_DATA_SINGLE_BLOCK_WRITE   0xFE  /*!< Data token start byte, Start Single Block Write */
#define SD_START_DATA_MULTIPLE_BLOCK_WRITE 0xFC  /*!< Data token start byte, Start Multiple Block Write */

/*
 * @brief  Commands: CMDxx = CMD-number | 0x40
 */
#define SD_CMD0          0   /*!< CMD0 = 0x40 */
#define SD_CMD8          8   /*!< CMD8 = 0x48 */
#define SD_CMD9          9   /*!< CMD9 = 0x49 */
#define SD_CMD10         10  /*!< CMD10 = 0x4A */
#define SD_CMD12         12  /*!< CMD12 = 0x4C */
#define SD_CMD16         16  /*!< CMD16 = 0x50 */
#define SD_CMD17         17  /*!< CMD17 = 0x51 */
#define SD_CMD18         18  /*!< CMD18 = 0x52 */
#define SD_ACMD23        23  /*!< CMD23 = 0x57 */
#define SD_CMD24         24  /*!< CMD24 = 0x58 */
#define SD_CMD25         25  /*!< CMD25 = 0x59 */
#define SD_ACMD41        41  /*!< ACMD41 = 0x41 */
#define SD_CMD55         55  /*!< CMD55 = 0x55 */
#define SD_CMD58         58  /*!< CMD58 = 0x58 */
#define SD_CMD59         59  /*!< CMD59 = 0x59 */

#define SD_SPI_LOW_CLOCK_RATE       200000U
#define SD_SPI_HIGH_CLOCK_RATE      40000000U
#define SPI_SLAVE_SELECT 3

/** 
  * @brief  Card Specific Data: CSD Register   
  */ 
typedef struct {
    uint8_t  CSDStruct;            /*!< CSD structure */
    uint8_t  SysSpecVersion;       /*!< System specification version */
    uint8_t  Reserved1;            /*!< Reserved */
    uint8_t  TAAC;                 /*!< Data read access-time 1 */
    uint8_t  NSAC;                 /*!< Data read access-time 2 in CLK cycles */
    uint8_t  MaxBusClkFrec;        /*!< Max. bus clock frequency */
    uint16_t CardComdClasses;      /*!< Card command classes */
    uint8_t  RdBlockLen;           /*!< Max. read data block length */
    uint8_t  PartBlockRead;        /*!< Partial blocks for read allowed */
    uint8_t  WrBlockMisalign;      /*!< Write block misalignment */
    uint8_t  RdBlockMisalign;      /*!< Read block misalignment */
    uint8_t  DSRImpl;              /*!< DSR implemented */
    uint8_t  Reserved2;            /*!< Reserved */
    uint32_t DeviceSize;           /*!< Device Size */
    uint8_t  MaxRdCurrentVDDMin;   /*!< Max. read current @ VDD min */
    uint8_t  MaxRdCurrentVDDMax;   /*!< Max. read current @ VDD max */
    uint8_t  MaxWrCurrentVDDMin;   /*!< Max. write current @ VDD min */
    uint8_t  MaxWrCurrentVDDMax;   /*!< Max. write current @ VDD max */
    uint8_t  DeviceSizeMul;        /*!< Device size multiplier */
    uint8_t  EraseGrSize;          /*!< Erase group size */
    uint8_t  EraseGrMul;           /*!< Erase group size multiplier */
    uint8_t  WrProtectGrSize;      /*!< Write protect group size */
    uint8_t  WrProtectGrEnable;    /*!< Write protect group enable */
    uint8_t  ManDeflECC;           /*!< Manufacturer default ECC */
    uint8_t  WrSpeedFact;          /*!< Write speed factor */
    uint8_t  MaxWrBlockLen;        /*!< Max. write data block length */
    uint8_t  WriteBlockPaPartial;  /*!< Partial blocks for write allowed */
    uint8_t  Reserved3;            /*!< Reserded */
    uint8_t  ContentProtectAppli;  /*!< Content protection application */
    uint8_t  FileFormatGrouop;     /*!< File format group */
    uint8_t  CopyFlag;             /*!< Copy flag (OTP) */
    uint8_t  PermWrProtect;        /*!< Permanent write protection */
    uint8_t  TempWrProtect;        /*!< Temporary write protection */
    uint8_t  FileFormat;           /*!< File Format */
    uint8_t  ECC;                  /*!< ECC code */
    uint8_t  CSD_CRC;              /*!< CSD CRC */
    uint8_t  Reserved4;            /*!< always 1*/
} SD_CSD;

/** 
  * @brief  Card Identification Data: CID Register   
  */
typedef struct {
    uint8_t  ManufacturerID;       /*!< ManufacturerID */
    uint16_t OEM_AppliID;          /*!< OEM/Application ID */
    uint32_t ProdName1;            /*!< Product Name part1 */
    uint8_t  ProdName2;            /*!< Product Name part2*/
    uint8_t  ProdRev;              /*!< Product Revision */
    uint32_t ProdSN;               /*!< Product Serial Number */
    uint8_t  Reserved1;            /*!< Reserved1 */
    uint16_t ManufactDate;         /*!< Manufacturing Date */
    uint8_t  CID_CRC;              /*!< CID CRC */
    uint8_t  Reserved2;            /*!< always 1 */
} SD_CID;

/** 
  * @brief SD Card information 
  */
typedef struct {
    SD_CSD SD_csd;
    SD_CID SD_cid;
    uint64_t CardCapacity;  /*!< Card Capacity */
    uint32_t CardBlockSize; /*!< Card Block Size */
} SD_CardInfo;

handle_t spi_sdcard_driver_install(handle_t spi_handle, handle_t cs_gpio_handle, uint32_t cs_gpio_pin);

#ifdef __cplusplus
}
#endif

#endif /* _DRIVERS_SDCARD_H */
