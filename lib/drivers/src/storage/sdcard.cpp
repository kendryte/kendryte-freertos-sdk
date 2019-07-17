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
#include "storage/sdcard.h"
#include <hal.h>
#include <kernel/driver_impl.hpp>
#include <stdlib.h>
#include <string.h>

using namespace sys;

/*
 * @brief  Start Data tokens:
 *         Tokens (necessary because at nop/idle (and CS active) only 0xff is
 *         on the data/command line)
 */
#define SD_START_DATA_SINGLE_BLOCK_READ 0xFE /*!< Data token start byte, Start Single Block Read */
#define SD_START_DATA_MULTIPLE_BLOCK_READ 0xFE /*!< Data token start byte, Start Multiple Block Read */
#define SD_START_DATA_SINGLE_BLOCK_WRITE 0xFE /*!< Data token start byte, Start Single Block Write */
#define SD_START_DATA_MULTIPLE_BLOCK_WRITE 0xFC /*!< Data token start byte, Start Multiple Block Write */

/*
 * @brief  Commands: CMDxx = CMD-number | 0x40
 */
#define SD_CMD0 0 /*!< CMD0 = 0x40 */
#define SD_CMD8 8 /*!< CMD8 = 0x48 */
#define SD_CMD9 9 /*!< CMD9 = 0x49 */
#define SD_CMD10 10 /*!< CMD10 = 0x4A */
#define SD_CMD12 12 /*!< CMD12 = 0x4C */
#define SD_CMD16 16 /*!< CMD16 = 0x50 */
#define SD_CMD17 17 /*!< CMD17 = 0x51 */
#define SD_CMD18 18 /*!< CMD18 = 0x52 */
#define SD_ACMD23 23 /*!< CMD23 = 0x57 */
#define SD_CMD24 24 /*!< CMD24 = 0x58 */
#define SD_CMD25 25 /*!< CMD25 = 0x59 */
#define SD_ACMD41 41 /*!< ACMD41 = 0x41 */
#define SD_CMD55 55 /*!< CMD55 = 0x55 */
#define SD_CMD58 58 /*!< CMD58 = 0x58 */
#define SD_CMD59 59 /*!< CMD59 = 0x59 */

#define SD_SPI_LOW_CLOCK_RATE 200000U
#define SD_SPI_HIGH_CLOCK_RATE 20000000U
#define SPI_SLAVE_SELECT 3

/** 
  * @brief  Card Specific Data: CSD Register   
  */
typedef struct
{
    uint8_t CSDStruct; /*!< CSD structure */
    uint8_t SysSpecVersion; /*!< System specification version */
    uint8_t Reserved1; /*!< Reserved */
    uint8_t TAAC; /*!< Data read access-time 1 */
    uint8_t NSAC; /*!< Data read access-time 2 in CLK cycles */
    uint8_t MaxBusClkFrec; /*!< Max. bus clock frequency */
    uint16_t CardComdClasses; /*!< Card command classes */
    uint8_t RdBlockLen; /*!< Max. read data block length */
    uint8_t PartBlockRead; /*!< Partial blocks for read allowed */
    uint8_t WrBlockMisalign; /*!< Write block misalignment */
    uint8_t RdBlockMisalign; /*!< Read block misalignment */
    uint8_t DSRImpl; /*!< DSR implemented */
    uint8_t Reserved2; /*!< Reserved */
    uint32_t DeviceSize; /*!< Device Size */
    uint8_t MaxRdCurrentVDDMin; /*!< Max. read current @ VDD min */
    uint8_t MaxRdCurrentVDDMax; /*!< Max. read current @ VDD max */
    uint8_t MaxWrCurrentVDDMin; /*!< Max. write current @ VDD min */
    uint8_t MaxWrCurrentVDDMax; /*!< Max. write current @ VDD max */
    uint8_t DeviceSizeMul; /*!< Device size multiplier */
    uint8_t EraseGrSize; /*!< Erase group size */
    uint8_t EraseGrMul; /*!< Erase group size multiplier */
    uint8_t WrProtectGrSize; /*!< Write protect group size */
    uint8_t WrProtectGrEnable; /*!< Write protect group enable */
    uint8_t ManDeflECC; /*!< Manufacturer default ECC */
    uint8_t WrSpeedFact; /*!< Write speed factor */
    uint8_t MaxWrBlockLen; /*!< Max. write data block length */
    uint8_t WriteBlockPaPartial; /*!< Partial blocks for write allowed */
    uint8_t Reserved3; /*!< Reserded */
    uint8_t ContentProtectAppli; /*!< Content protection application */
    uint8_t FileFormatGrouop; /*!< File format group */
    uint8_t CopyFlag; /*!< Copy flag (OTP) */
    uint8_t PermWrProtect; /*!< Permanent write protection */
    uint8_t TempWrProtect; /*!< Temporary write protection */
    uint8_t FileFormat; /*!< File Format */
    uint8_t ECC; /*!< ECC code */
    uint8_t CSD_CRC; /*!< CSD CRC */
    uint8_t Reserved4; /*!< always 1*/
} SD_CSD;

/** 
  * @brief  Card Identification Data: CID Register   
  */
typedef struct
{
    uint8_t ManufacturerID; /*!< ManufacturerID */
    uint16_t OEM_AppliID; /*!< OEM/Application ID */
    uint32_t ProdName1; /*!< Product Name part1 */
    uint8_t ProdName2; /*!< Product Name part2*/
    uint8_t ProdRev; /*!< Product Revision */
    uint32_t ProdSN; /*!< Product Serial Number */
    uint8_t Reserved1; /*!< Reserved1 */
    uint16_t ManufactDate; /*!< Manufacturing Date */
    uint8_t CID_CRC; /*!< CID CRC */
    uint8_t Reserved2; /*!< always 1 */
} SD_CID;

/** 
  * @brief SD Card information 
  */
typedef struct
{
    SD_CSD SD_csd;
    SD_CID SD_cid;
    uint64_t CardCapacity; /*!< Card Capacity */
    uint32_t CardBlockSize; /*!< Card Block Size */
} SD_CardInfo;

class k_spi_sdcard_driver : public block_storage_driver, public heap_object, public free_object_access
{
public:
    k_spi_sdcard_driver(handle_t spi_handle, handle_t cs_gpio_handle, uint32_t cs_gpio_pin)
        : spi_driver_(system_handle_to_object(spi_handle).get_object().as<spi_driver>())
        , cs_gpio_driver_(system_handle_to_object(cs_gpio_handle).get_object().as<gpio_driver>())
        , cs_gpio_pin_(cs_gpio_pin)
    {
    }

    virtual void install() override
    {
    }

    virtual void on_first_open() override
    {
        auto spi = make_accessor(spi_driver_);
        spi8_dev_ = make_accessor(spi->get_device(SPI_MODE_0, SPI_FF_STANDARD, 1, 8));

        cs_gpio_ = make_accessor(cs_gpio_driver_);
        cs_gpio_->set_drive_mode(cs_gpio_pin_, GPIO_DM_OUTPUT);
        cs_gpio_->set_pin_value(cs_gpio_pin_, GPIO_PV_HIGH);

        spi8_dev_->set_clock_rate(SD_SPI_LOW_CLOCK_RATE);
        configASSERT(sd_init() == 0);
    }

    virtual void on_last_close() override
    {
        spi8_dev_.reset();
        cs_gpio_.reset();
    }

    virtual uint32_t get_rw_block_size() override
    {
        return card_info_.CardBlockSize;
    }

    virtual uint32_t get_blocks_count() override
    {
        return card_info_.CardCapacity;
    }

    virtual void read_blocks(uint32_t start_block, uint32_t blocks_count, gsl::span<uint8_t> buffer) override
    {
        spi8_dev_->set_clock_rate(SD_SPI_HIGH_CLOCK_RATE);
        sd_read_sector_dma(buffer.data(), start_block, blocks_count);
    }

    virtual void write_blocks(uint32_t start_block, uint32_t blocks_count, gsl::span<const uint8_t> buffer) override
    {
        spi8_dev_->set_clock_rate(SD_SPI_HIGH_CLOCK_RATE);
        sd_write_sector_dma(buffer.data(), start_block, blocks_count);
    }

private:
    void set_tf_cs_low()
    {
        cs_gpio_->set_pin_value(cs_gpio_pin_, GPIO_PV_LOW);
    }

    void set_tf_cs_high()
    {
        cs_gpio_->set_pin_value(cs_gpio_pin_, GPIO_PV_HIGH);
    }

    void sd_write_data(const uint8_t *data_buff, size_t length)
    {
        spi8_dev_->write({ data_buff, std::ptrdiff_t(length) });
    }

    void sd_read_data(uint8_t *data_buff, size_t length)
    {
        spi8_dev_->read({ data_buff, std::ptrdiff_t(length) });
    }

    void sd_write_data_dma(const uint8_t *data_buff)
    {
        spi8_dev_->write({ data_buff, 512L });
    }

    void sd_read_data_dma(uint8_t *data_buff)
    {
        spi8_dev_->read({ data_buff, 512L });
    }

    /*
     * @brief  Send 5 bytes command to the SD card.
     * @param  Cmd: The user expected command to send to SD card.
     * @param  Arg: The command argument.
     * @param  Crc: The CRC.
     * @retval None
     */
    void sd_send_cmd(uint8_t cmd, uint32_t arg, uint8_t crc)
    {
        uint8_t frame[6];
        /*!< Construct byte 1 */
        frame[0] = (cmd | 0x40);
        /*!< Construct byte 2 */
        frame[1] = (uint8_t)(arg >> 24);
        /*!< Construct byte 3 */
        frame[2] = (uint8_t)(arg >> 16);
        /*!< Construct byte 4 */
        frame[3] = (uint8_t)(arg >> 8);
        /*!< Construct byte 5 */
        frame[4] = (uint8_t)(arg);
        /*!< Construct CRC: byte 6 */
        frame[5] = (crc);
        /*!< SD chip select low */
        set_tf_cs_low();
        /*!< Send the Cmd bytes */
        sd_write_data(frame, 6);
    }

    /*
     * @brief  Send 5 bytes command to the SD card.
     * @param  Cmd: The user expected command to send to SD card.
     * @param  Arg: The command argument.
     * @param  Crc: The CRC.
     * @retval None
     */
    void sd_end_cmd()
    {
        uint8_t frame[1] = { 0xFF };
        /*!< SD chip select high */
        set_tf_cs_high();
        /*!< Send the Cmd bytes */
        sd_write_data(frame, 1);
    }

    /*
     * @brief  Returns the SD response.
     * @param  None
     * @retval The SD Response:
     *         - 0xFF: Sequence failed
     *         - 0: Sequence succeed
     */
    uint8_t sd_get_response()
    {
        uint8_t result;
        uint16_t timeout = 0x0FFF;
        /*!< Check if response is got or a timeout is happen */
        while (timeout--)
        {
            sd_read_data(&result, 1);
            /*!< Right response got */
            if (result != 0xFF)
                return result;
        }
        /*!< After time out */
        return 0xFF;
    }

    /*
     * @brief  Get SD card data response.
     * @param  None
     * @retval The SD status: Read data response xxx0<status>1
     *         - status 010: Data accecpted
     *         - status 101: Data rejected due to a crc error
     *         - status 110: Data rejected due to a Write error.
     *         - status 111: Data rejected due to other error.
     */
    uint8_t sd_get_dataresponse()
    {
        uint8_t response;
        /*!< Read resonse */
        sd_read_data(&response, 1);
        /*!< Mask unused bits */
        response &= 0x1F;
        if (response != 0x05)
            return 0xFF;
        /*!< Wait null data */
        sd_read_data(&response, 1);
        while (response == 0)
            sd_read_data(&response, 1);
        /*!< Return response */
        return 0;
    }

    /*
     * @brief  Read the CSD card register
     *         Reading the contents of the CSD register in SPI mode is a simple
     *         read-block transaction.
     * @param  SD_csd: pointer on an SCD register structure
     * @retval The SD Response:
     *         - 0xFF: Sequence failed
     *         - 0: Sequence succeed
     */
    uint8_t sd_get_csdregister(SD_CSD *SD_csd)
    {
        uint8_t csd_tab[18];
        /*!< Send CMD9 (CSD register) or CMD10(CSD register) */
        sd_send_cmd(SD_CMD9, 0, 0);
        /*!< Wait for response in the R1 format (0x00 is no errors) */
        if (sd_get_response() != 0x00)
        {
            sd_end_cmd();
            return 0xFF;
        }
        if (sd_get_response() != SD_START_DATA_SINGLE_BLOCK_READ)
        {
            sd_end_cmd();
            return 0xFF;
        }
        /*!< Store CSD register value on csd_tab */
        /*!< Get CRC bytes (not really needed by us, but required by SD) */
        sd_read_data(csd_tab, 18);
        sd_end_cmd();
        /*!< Byte 0 */
        SD_csd->CSDStruct = (csd_tab[0] & 0xC0) >> 6;
        SD_csd->SysSpecVersion = (csd_tab[0] & 0x3C) >> 2;
        SD_csd->Reserved1 = csd_tab[0] & 0x03;
        /*!< Byte 1 */
        SD_csd->TAAC = csd_tab[1];
        /*!< Byte 2 */
        SD_csd->NSAC = csd_tab[2];
        /*!< Byte 3 */
        SD_csd->MaxBusClkFrec = csd_tab[3];
        /*!< Byte 4 */
        SD_csd->CardComdClasses = csd_tab[4] << 4;
        /*!< Byte 5 */
        SD_csd->CardComdClasses |= (csd_tab[5] & 0xF0) >> 4;
        SD_csd->RdBlockLen = csd_tab[5] & 0x0F;
        /*!< Byte 6 */
        SD_csd->PartBlockRead = (csd_tab[6] & 0x80) >> 7;
        SD_csd->WrBlockMisalign = (csd_tab[6] & 0x40) >> 6;
        SD_csd->RdBlockMisalign = (csd_tab[6] & 0x20) >> 5;
        SD_csd->DSRImpl = (csd_tab[6] & 0x10) >> 4;
        SD_csd->Reserved2 = 0; /*!< Reserved */
        SD_csd->DeviceSize = (csd_tab[6] & 0x03) << 10;
        /*!< Byte 7 */
        SD_csd->DeviceSize = (csd_tab[7] & 0x3F) << 16;
        /*!< Byte 8 */
        SD_csd->DeviceSize |= csd_tab[8] << 8;
        /*!< Byte 9 */
        SD_csd->DeviceSize |= csd_tab[9];
        /*!< Byte 10 */
        SD_csd->EraseGrSize = (csd_tab[10] & 0x40) >> 6;
        SD_csd->EraseGrMul = (csd_tab[10] & 0x3F) << 1;
        /*!< Byte 11 */
        SD_csd->EraseGrMul |= (csd_tab[11] & 0x80) >> 7;
        SD_csd->WrProtectGrSize = (csd_tab[11] & 0x7F);
        /*!< Byte 12 */
        SD_csd->WrProtectGrEnable = (csd_tab[12] & 0x80) >> 7;
        SD_csd->ManDeflECC = (csd_tab[12] & 0x60) >> 5;
        SD_csd->WrSpeedFact = (csd_tab[12] & 0x1C) >> 2;
        SD_csd->MaxWrBlockLen = (csd_tab[12] & 0x03) << 2;
        /*!< Byte 13 */
        SD_csd->MaxWrBlockLen |= (csd_tab[13] & 0xC0) >> 6;
        SD_csd->WriteBlockPaPartial = (csd_tab[13] & 0x20) >> 5;
        SD_csd->Reserved3 = 0;
        SD_csd->ContentProtectAppli = (csd_tab[13] & 0x01);
        /*!< Byte 14 */
        SD_csd->FileFormatGrouop = (csd_tab[14] & 0x80) >> 7;
        SD_csd->CopyFlag = (csd_tab[14] & 0x40) >> 6;
        SD_csd->PermWrProtect = (csd_tab[14] & 0x20) >> 5;
        SD_csd->TempWrProtect = (csd_tab[14] & 0x10) >> 4;
        SD_csd->FileFormat = (csd_tab[14] & 0x0C) >> 2;
        SD_csd->ECC = (csd_tab[14] & 0x03);
        /*!< Byte 15 */
        SD_csd->CSD_CRC = (csd_tab[15] & 0xFE) >> 1;
        SD_csd->Reserved4 = 1;
        /*!< Return the reponse */
        return 0;
    }

    /*
     * @brief  Read the CID card register.
     *         Reading the contents of the CID register in SPI mode is a simple
     *         read-block transaction.
     * @param  SD_cid: pointer on an CID register structure
     * @retval The SD Response:
     *         - 0xFF: Sequence failed
     *         - 0: Sequence succeed
     */
    uint8_t sd_get_cidregister(SD_CID *SD_cid)
    {
        uint8_t cid_tab[18];
        /*!< Send CMD10 (CID register) */
        sd_send_cmd(SD_CMD10, 0, 0);
        /*!< Wait for response in the R1 format (0x00 is no errors) */
        if (sd_get_response() != 0x00)
        {
            sd_end_cmd();
            return 0xFF;
        }
        if (sd_get_response() != SD_START_DATA_SINGLE_BLOCK_READ)
        {
            sd_end_cmd();
            return 0xFF;
        }
        /*!< Store CID register value on cid_tab */
        /*!< Get CRC bytes (not really needed by us, but required by SD) */
        sd_read_data(cid_tab, 18);
        sd_end_cmd();
        /*!< Byte 0 */
        SD_cid->ManufacturerID = cid_tab[0];
        /*!< Byte 1 */
        SD_cid->OEM_AppliID = cid_tab[1] << 8;
        /*!< Byte 2 */
        SD_cid->OEM_AppliID |= cid_tab[2];
        /*!< Byte 3 */
        SD_cid->ProdName1 = cid_tab[3] << 24;
        /*!< Byte 4 */
        SD_cid->ProdName1 |= cid_tab[4] << 16;
        /*!< Byte 5 */
        SD_cid->ProdName1 |= cid_tab[5] << 8;
        /*!< Byte 6 */
        SD_cid->ProdName1 |= cid_tab[6];
        /*!< Byte 7 */
        SD_cid->ProdName2 = cid_tab[7];
        /*!< Byte 8 */
        SD_cid->ProdRev = cid_tab[8];
        /*!< Byte 9 */
        SD_cid->ProdSN = cid_tab[9] << 24;
        /*!< Byte 10 */
        SD_cid->ProdSN |= cid_tab[10] << 16;
        /*!< Byte 11 */
        SD_cid->ProdSN |= cid_tab[11] << 8;
        /*!< Byte 12 */
        SD_cid->ProdSN |= cid_tab[12];
        /*!< Byte 13 */
        SD_cid->Reserved1 |= (cid_tab[13] & 0xF0) >> 4;
        SD_cid->ManufactDate = (cid_tab[13] & 0x0F) << 8;
        /*!< Byte 14 */
        SD_cid->ManufactDate |= cid_tab[14];
        /*!< Byte 15 */
        SD_cid->CID_CRC = (cid_tab[15] & 0xFE) >> 1;
        SD_cid->Reserved2 = 1;
        /*!< Return the reponse */
        return 0;
    }

    /*
     * @brief  Returns information about specific card.
     * @param  cardinfo: pointer to a SD_CardInfo structure that contains all SD
     *         card information.
     * @retval The SD Response:
     *         - 0xFF: Sequence failed
     *         - 0: Sequence succeed
     */
    uint8_t sd_get_cardinfo(SD_CardInfo *cardinfo)
    {
        if (sd_get_csdregister(&(cardinfo->SD_csd)))
            return 0xFF;
        if (sd_get_cidregister(&(cardinfo->SD_cid)))
            return 0xFF;
        cardinfo->CardCapacity = (cardinfo->SD_csd.DeviceSize + 1) * 1024;
        cardinfo->CardBlockSize = 1 << (cardinfo->SD_csd.RdBlockLen);
        cardinfo->CardCapacity *= cardinfo->CardBlockSize;
        /*!< Returns the reponse */
        return 0;
    }

    /*
     * @brief  Initializes the SD/SD communication.
     * @param  None
     * @retval The SD Response:
     *         - 0xFF: Sequence failed
     *         - 0: Sequence succeed
     */
    uint8_t sd_init()
    {
        uint8_t frame[10], index, result;

        /*!< SD chip select high */
        set_tf_cs_high();
        /*!< Send dummy byte 0xFF, 10 times with CS high */
        /*!< Rise CS and MOSI for 80 clocks cycles */
        /*!< Send dummy byte 0xFF */
        for (index = 0; index < 10; index++)
            frame[index] = 0xFF;
        sd_write_data(frame, 10);

        /*------------Put SD in SPI mode--------------*/
        /*!< SD initialized and set to SPI mode properly */
        sd_send_cmd(SD_CMD0, 0, 0x95);
        result = sd_get_response();
        sd_end_cmd();

        if (result != 0x01)
            return 0xFF;
        sd_send_cmd(SD_CMD8, 0x01AA, 0x87);
        /*!< 0x01 or 0x05 */
        result = sd_get_response();
        sd_read_data(frame, 4);
        sd_end_cmd();
        if (result != 0x01)
            return 0xFF;
        index = 0xFF;

        while (index--)
        {
            sd_send_cmd(SD_CMD55, 0, 0);
            result = sd_get_response();
            sd_end_cmd();
            if (result != 0x01)
                return 0xFF;
            sd_send_cmd(SD_ACMD41, 0x40000000, 0);
            result = sd_get_response();
            sd_end_cmd();
            if (result == 0x00)
                break;
        }
        if (index == 0)
            return 0xFF;

        index = 100;
        while (index--)
        {
            sd_send_cmd(SD_CMD58, 0, 1);
            result = sd_get_response();
            sd_read_data(frame, 4);
            sd_end_cmd();
            if (result == 0)
            {
                break;
            }
        }
        if (index == 0)
        {
            return 0xFF;
        }

        if ((frame[0] & 0x40) == 0)
            return 0xFF;

        spi8_dev_->set_clock_rate(SD_SPI_HIGH_CLOCK_RATE);
        return sd_get_cardinfo(&card_info_);
    }

    /*
     * @brief  Reads a block of data from the SD.
     * @param  data_buff: pointer to the buffer that receives the data read from the
     *                  SD.
     * @param  sector: SD's internal address to read from.
     * @retval The SD Response:
     *         - 0xFF: Sequence failed
     *         - 0: Sequence succeed
     */
    uint8_t sd_read_sector(uint8_t *data_buff, uint32_t sector, uint32_t count)
    {
        uint8_t frame[2], flag;
        /*!< Send CMD17 (SD_CMD17) to read one block */
        if (count == 1)
        {
            flag = 0;
            sd_send_cmd(SD_CMD17, sector, 0);
        }
        else
        {
            flag = 1;
            sd_send_cmd(SD_CMD18, sector, 0);
        }
        /*!< Check if the SD acknowledged the read block command: R1 response (0x00: no errors) */
        if (sd_get_response() != 0x00)
        {
            sd_end_cmd();
            return 0xFF;
        }
        while (count)
        {
            if (sd_get_response() != SD_START_DATA_SINGLE_BLOCK_READ)
                break;
            /*!< Read the SD block data : read NumByteToRead data */
            sd_read_data(data_buff, 512);
            /*!< Get CRC bytes (not really needed by us, but required by SD) */
            sd_read_data(frame, 2);
            data_buff += 512;
            count--;
        }
        sd_end_cmd();
        if (flag)
        {
            sd_send_cmd(SD_CMD12, 0, 0);
            sd_get_response();
            sd_end_cmd();
            sd_end_cmd();
        }
        /*!< Returns the reponse */
        return count > 0 ? 0xFF : 0;
    }

    /*
     * @brief  Writes a block on the SD
     * @param  data_buff: pointer to the buffer containing the data to be written on
     *                  the SD.
     * @param  sector: address to write on.
     * @retval The SD Response:
     *         - 0xFF: Sequence failed
     *         - 0: Sequence succeed
     */
    uint8_t sd_write_sector(uint8_t *data_buff, uint32_t sector, uint32_t count)
    {
        uint8_t frame[2] = { 0xFF };

        if (count == 1)
        {
            frame[1] = SD_START_DATA_SINGLE_BLOCK_WRITE;
            sd_send_cmd(SD_CMD24, sector, 0);
        }
        else
        {
            frame[1] = SD_START_DATA_MULTIPLE_BLOCK_WRITE;
            sd_send_cmd(SD_ACMD23, count, 0);
            sd_get_response();
            sd_end_cmd();
            sd_send_cmd(SD_CMD25, sector, 0);
        }
        /*!< Check if the SD acknowledged the write block command: R1 response (0x00: no errors) */
        if (sd_get_response() != 0x00)
        {
            sd_end_cmd();
            return 0xFF;
        }
        while (count--)
        {
            /*!< Send the data token to signify the start of the data */
            sd_write_data(frame, 2);
            /*!< Write the block data to SD : write count data by block */
            sd_write_data(data_buff, 512);
            /*!< Put CRC bytes (not really needed by us, but required by SD) */
            sd_write_data(frame, 2);
            data_buff += 512;
            /*!< Read data response */
            if (sd_get_dataresponse() != 0x00)
            {
                sd_end_cmd();
                return 0xFF;
            }
        }
        sd_end_cmd();
        sd_end_cmd();
        /*!< Returns the reponse */
        return 0;
    }

    uint8_t sd_read_sector_dma(uint8_t *data_buff, uint32_t sector, uint32_t count)
    {
        uint8_t frame[2], flag;

        /*!< Send CMD17 (SD_CMD17) to read one block */
        if (count == 1)
        {
            flag = 0;
            sd_send_cmd(SD_CMD17, sector, 0);
        }
        else
        {
            flag = 1;
            sd_send_cmd(SD_CMD18, sector, 0);
        }
        /*!< Check if the SD acknowledged the read block command: R1 response (0x00: no errors) */
        if (sd_get_response() != 0x00)
        {
            sd_end_cmd();
            return 0xFF;
        }
        while (count)
        {
            if (sd_get_response() != SD_START_DATA_SINGLE_BLOCK_READ)
                break;
            /*!< Read the SD block data : read NumByteToRead data */
            sd_read_data_dma(data_buff);
            /*!< Get CRC bytes (not really needed by us, but required by SD) */
            sd_read_data(frame, 2);
            data_buff += 512;
            count--;
        }
        sd_end_cmd();
        if (flag)
        {
            sd_send_cmd(SD_CMD12, 0, 0);
            sd_get_response();
            sd_end_cmd();
            sd_end_cmd();
        }
        /*!< Returns the reponse */
        return count > 0 ? 0xFF : 0;
    }

    uint8_t sd_write_sector_dma(const uint8_t *data_buff, uint32_t sector, uint32_t count)
    {
        uint8_t frame[2] = { 0xFF };

        if (count == 1)
        {
            frame[1] = SD_START_DATA_SINGLE_BLOCK_WRITE;
            sd_send_cmd(SD_CMD24, sector, 0);
        }
        else
        {
            frame[1] = SD_START_DATA_MULTIPLE_BLOCK_WRITE;
            sd_send_cmd(SD_ACMD23, count, 0);
            sd_get_response();
            sd_end_cmd();
            sd_send_cmd(SD_CMD25, sector, 0);
        }
        /*!< Check if the SD acknowledged the write block command: R1 response (0x00: no errors) */
        if (sd_get_response() != 0x00)
        {
            sd_end_cmd();
            return 0xFF;
        }
        while (count--)
        {
            /*!< Send the data token to signify the start of the data */
            sd_write_data(frame, 2);
            /*!< Write the block data to SD : write count data by block */
            sd_write_data_dma(data_buff);
            /*!< Put CRC bytes (not really needed by us, but required by SD) */
            sd_write_data(frame, 2);
            data_buff += 512;
            /*!< Read data response */
            if (sd_get_dataresponse() != 0x00)
            {
                sd_end_cmd();
                return 0xFF;
            }
        }
        sd_end_cmd();
        sd_end_cmd();
        /*!< Returns the reponse */
        return 0;
    }

private:
    object_ptr<spi_driver> spi_driver_;
    object_ptr<gpio_driver> cs_gpio_driver_;
    uint32_t cs_gpio_pin_;

    object_accessor<gpio_driver> cs_gpio_;
    object_accessor<spi_device_driver> spi8_dev_;
    SD_CardInfo card_info_;
};

handle_t spi_sdcard_driver_install(handle_t spi_handle, handle_t cs_gpio_handle, uint32_t cs_gpio_pin)
{
    try
    {
        auto driver = make_object<k_spi_sdcard_driver>(spi_handle, cs_gpio_handle, cs_gpio_pin);
        driver->install();
        return system_alloc_handle(make_accessor(driver));
    }
    catch (...)
    {
        return NULL_HANDLE;
    }
}
