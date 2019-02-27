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
#include <atomic.h>
#include <semphr.h>
#include <hal.h>
#include <kernel/driver_impl.hpp>
#include <stdlib.h>
#include <string.h>
#include <printf.h>
#include <sys/unistd.h>
#include "network/dm9051.h"
#include "task.h"

using namespace sys;

/* Private typedef -----------------------------------------------------------------------------------------*/
enum DM9051_PHY_mode
{
    DM9051_10MHD = 0,
    DM9051_100MHD = 1,
    DM9051_10MFD = 4,
    DM9051_100MFD = 5,
    DM9051_10M = 6,
    DM9051_AUTO = 8,
    DM9051_1M_HPNA = 0x10
};

enum DM9051_TYPE
{
    TYPE_DM9051E,
    TYPE_DM9051A,
    TYPE_DM9051B,
    TYPE_DM9051
};

/* Private constants ---------------------------------------------------------------------------------------*/
#define DM9051_PHY (0x40) /* PHY address 0x01                                             */

/* Exported typedef ----------------------------------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------------------------------------*/
#define DM9051_ID       (0x90510A46) /* DM9051A ID                                                   */
#define DM9051_PKT_MAX  (1536) /* Received packet max size                                     */
#define DM9051_PKT_RDY  (0x01) /* Packet ready to receive                                      */

#define DM9051_NCR      (0x00)
#define DM9051_NSR      (0x01)
#define DM9051_TCR      (0x02)
#define DM9051_TSR1     (0x03)
#define DM9051_TSR2     (0x04)
#define DM9051_RCR      (0x05)
#define DM9051_RSR      (0x06)
#define DM9051_ROCR     (0x07)
#define DM9051_BPTR     (0x08)
#define DM9051_FCTR     (0x09)
#define DM9051_FCR      (0x0A)
#define DM9051_EPCR     (0x0B)
#define DM9051_EPAR     (0x0C)
#define DM9051_EPDRL    (0x0D)
#define DM9051_EPDRH    (0x0E)
#define DM9051_WCR      (0x0F)

#define DM9051_PAR      (0x10)
#define DM9051_MAR      (0x16)

#define DM9051_GPCR     (0x1e)
#define DM9051_GPR      (0x1f)
#define DM9051_TRPAL    (0x22)
#define DM9051_TRPAH    (0x23)
#define DM9051_RWPAL    (0x24)
#define DM9051_RWPAH    (0x25)

#define DM9051_VIDL     (0x28)
#define DM9051_VIDH     (0x29)
#define DM9051_PIDL     (0x2A)
#define DM9051_PIDH     (0x2B)

#define DM9051_CHIPR    (0x2C)
#define DM9051_TCR2     (0x2D)
#define DM9051_OTCR     (0x2E)
#define DM9051_SMCR     (0x2F)

#define DM9051_ETCR     (0x30) /* early transmit control/status register                             */
#define DM9051_CSCR     (0x31) /* check sum control register                                         */
#define DM9051_RCSSR    (0x32) /* receive check sum status register                                  */

#define DM9051_PBCR     (0x38)
#define DM9051_INTR     (0x39)
#define DM9051_MPCR     (0x55)
#define DM9051_MRCMDX   (0x70)
#define DM9051_MRCMDX1  (0x71)
#define DM9051_MRCMD    (0x72)
#define DM9051_MRRL     (0x74)
#define DM9051_MRRH     (0x75)
#define DM9051_MWCMDX   (0x76)
#define DM9051_MWCMD    (0x78)
#define DM9051_MWRL     (0x7A)
#define DM9051_MWRH     (0x7B)
#define DM9051_TXPLL    (0x7C)
#define DM9051_TXPLH    (0x7D)
#define DM9051_ISR      (0x7E)
#define DM9051_IMR      (0x7F)

#define CHIPR_DM9051A   (0x19)
#define CHIPR_DM9051B   (0x1B)

#define DM9051_REG_RESET    (0x01)
#define DM9051_IMR_OFF  (0x80)
#define DM9051_TCR2_SET (0x90) /* set one packet */
#define DM9051_RCR_SET  (0x31)
#define DM9051_BPTR_SET (0x37)
#define DM9051_FCTR_SET (0x38)
#define DM9051_FCR_SET  (0x28)
#define DM9051_TCR_SET  (0x01)

#define NCR_EXT_PHY     (1 << 7)
#define NCR_WAKEEN      (1 << 6)
#define NCR_FCOL        (1 << 4)
#define NCR_FDX         (1 << 3)
#define NCR_LBK         (3 << 1)
#define NCR_RST         (1 << 0)
#define NCR_DEFAULT     (0x0) /* Disable Wakeup */

#define NSR_SPEED       (1 << 7)
#define NSR_LINKST      (1 << 6)
#define NSR_WAKEST      (1 << 5)
#define NSR_TX2END      (1 << 3)
#define NSR_TX1END      (1 << 2)
#define NSR_RXOV        (1 << 1)
#define NSR_CLR_STATUS (NSR_WAKEST | NSR_TX2END | NSR_TX1END)

#define TCR_TJDIS       (1 << 6)
#define TCR_EXCECM      (1 << 5)
#define TCR_PAD_DIS2    (1 << 4)
#define TCR_CRC_DIS2    (1 << 3)
#define TCR_PAD_DIS1    (1 << 2)
#define TCR_CRC_DIS1    (1 << 1)
#define TCR_TXREQ       (1 << 0) /* Start TX */
#define TCR_DEFAULT     (0x0)

#define TSR_TJTO        (1 << 7)
#define TSR_LC          (1 << 6)
#define TSR_NC          (1 << 5)
#define TSR_LCOL        (1 << 4)
#define TSR_COL         (1 << 3)
#define TSR_EC          (1 << 2)

#define RCR_WTDIS       (1 << 6)
#define RCR_DIS_LONG    (1 << 5)
#define RCR_DIS_CRC     (1 << 4)
#define RCR_ALL         (1 << 3)
#define RCR_RUNT        (1 << 2)
#define RCR_PRMSC       (1 << 1)
#define RCR_RXEN        (1 << 0)
#define RCR_DEFAULT (RCR_DIS_LONG | RCR_DIS_CRC)

#define RSR_RF          (1 << 7)
#define RSR_MF          (1 << 6)
#define RSR_LCS         (1 << 5)
#define RSR_RWTO        (1 << 4)
#define RSR_PLE         (1 << 3)
#define RSR_AE          (1 << 2)
#define RSR_CE          (1 << 1)
#define RSR_FOE         (1 << 0)

#define BPTR_DEFAULT    (0x3f)
#define FCTR_DEAFULT    (0x38)
#define FCR_DEFAULT     (0xFF)
#define SMCR_DEFAULT    (0x0)
#define PBCR_MAXDRIVE   (0x44)

//#define FCTR_HWOT(ot) ((ot & 0xF ) << 4 )
//#define FCTR_LWOT(ot) (ot & 0xF )

#define IMR_PAR         (1 << 7)
#define IMR_LNKCHGI     (1 << 5)
#define IMR_UDRUN       (1 << 4)
#define IMR_ROOM        (1 << 3)
#define IMR_ROM         (1 << 2)
#define IMR_PTM         (1 << 1)
#define IMR_PRM         (1 << 0)
#define IMR_FULL (IMR_PAR | IMR_LNKCHGI | IMR_UDRUN | IMR_ROOM | IMR_ROM | IMR_PTM | IMR_PRM)
#define IMR_OFF (IMR_PAR)
#define IMR_DEFAULT (IMR_PAR | IMR_PRM | IMR_PTM)

#define ISR_ROOS        (1 << 3)
#define ISR_ROS         (1 << 2)
#define ISR_PTS         (1 << 1)
#define ISR_PRS         (1 << 0)
#define ISR_CLR_STATUS  (0x80 | 0x3F)

#define EPCR_REEP       (1 << 5)
#define EPCR_WEP        (1 << 4)
#define EPCR_EPOS       (1 << 3)
#define EPCR_ERPRR      (1 << 2)
#define EPCR_ERPRW      (1 << 1)
#define EPCR_ERRE       (1 << 0)

#define GPCR_GEP_CNTL   (1 << 0)

#define SPI_WR_BURST    (0xF8)
#define SPI_RD_BURST    (0x72)

#define SPI_READ        (0x03)
#define SPI_WRITE       (0x04)
#define SPI_WRITE_BUFFER    (0x05) /* Send a series of bytes from the Master to the Slave */
#define SPI_READ_BUFFER     (0x06) /* Send a series of bytes from the Slave  to the Master */

class dm9051_driver : public network_adapter_driver, public heap_object, public exclusive_object_access
{
public:
    dm9051_driver(handle_t spi_handle, uint32_t spi_cs_mask, handle_t int_gpio_handle, uint32_t int_gpio_pin, const mac_address_t &mac_address)
        : spi_driver_(system_handle_to_object(spi_handle).get_object().as<spi_driver>())
        , spi_cs_mask_(spi_cs_mask)
        , mac_address_(mac_address)
        , int_gpio_driver_(system_handle_to_object(int_gpio_handle).get_object().as<gpio_driver>())
        , int_gpio_pin_(int_gpio_pin)
    {
    }

    virtual void install() override
    {
    }

    virtual void on_first_open() override
    {
        auto spi = make_accessor(spi_driver_);
        spi_dev_ = make_accessor(spi->get_device(SPI_MODE_0, SPI_FF_STANDARD, spi_cs_mask_, 8));
        spi_dev_->set_clock_rate(20000000);

        int_gpio_ = make_accessor(int_gpio_driver_);
        int_gpio_->set_drive_mode(int_gpio_pin_, GPIO_DM_INPUT);
        int_gpio_->set_pin_edge(int_gpio_pin_, GPIO_PE_FALLING);
        int_gpio_->set_on_changed(int_gpio_pin_, (gpio_on_changed_t)isr_handle, this);

        uint32_t value = 0;
        /* Read DM9051 PID / VID, Check MCU SPI Setting correct */
        value |= (uint32_t)read(DM9051_VIDL);
        value |= (uint32_t)read(DM9051_VIDH) << 8;
        value |= (uint32_t)read(DM9051_PIDL) << 16;
        value |= (uint32_t)read(DM9051_PIDH) << 24;

        configASSERT(value == 0x90510a46);
    }

    virtual void on_last_close() override
    {
        spi_dev_.reset();
    }

    virtual void set_handler(network_adapter_handler *handler) override
    {
        handler_ = handler;
    }

    virtual mac_address_t get_mac_address() override
    {
        return mac_address_;
    }

    virtual bool is_packet_available() override
    {
        uint8_t rxbyte;
        /* Check packet ready or not */
        rxbyte = read(DM9051_MRCMDX);
        rxbyte = read(DM9051_MRCMDX);

        if ((rxbyte != 1) && (rxbyte != 0))
        {
            /* Reset RX FIFO pointer */
            write(DM9051_RCR, RCR_DEFAULT); //RX disable
            write(DM9051_MPCR, 0x01); //Reset RX FIFO pointer
            usleep(2e3);
            write(DM9051_RCR, (RCR_DEFAULT | RCR_RXEN)); //RX Enable

            return false;
        }
        return (rxbyte & 0b1) == 0b1;
    }

    virtual void reset(SemaphoreHandle_t interrupt_event) override
    {
        interrupt_event_ = interrupt_event;
        write(DM9051_NCR, DM9051_REG_RESET);
        while (read(DM9051_NCR) & DM9051_REG_RESET)
            ;

        write(DM9051_GPCR, GPCR_GEP_CNTL);
        write(DM9051_GPR, 0x00); //Power on PHY
        usleep(1e5);

        set_phy_mode(DM9051_AUTO);
        set_mac_address(mac_address_);

        /* set multicast address */
        for (size_t i = 0; i < 8; i++)
        { /* Clear Multicast set */
            /* Set Broadcast */
            write(DM9051_MAR + i, (7 == i) ? 0x80 : 0x00);
        }

        /************************************************
        *** Activate DM9051 and Setup DM9051 Registers **
        *************************************************/
        /* Clear DM9051 Set and Disable Wakeup function */
        write(DM9051_NCR, NCR_DEFAULT);
        /* Clear TCR Register set */
        write(DM9051_TCR, TCR_DEFAULT);
        /* Discard long Packet and CRC error Packet */
        write(DM9051_RCR, RCR_DEFAULT);
        /*  Set 1.15 ms Jam Pattern Timer */
        write(DM9051_BPTR, BPTR_DEFAULT);

        /* Open / Close Flow Control */
        //DM9051_Write_Reg(DM9051_FCTR, FCTR_DEAFULT);
        write(DM9051_FCTR, 0x3A);
        write(DM9051_FCR, FCR_DEFAULT);

        /* Set Memory Conttrol Register£¬TX = 3K£¬RX = 13K */
        write(DM9051_SMCR, SMCR_DEFAULT);
        /* Set Send one or two command Packet*/
        write(DM9051_TCR2, DM9051_TCR2_SET);

        //DM9051_Write_Reg(DM9051_TCR2, 0x80);
        write(DM9051_INTR, 0x1);

        /* Clear status */
        write(DM9051_NSR, NSR_CLR_STATUS);
        write(DM9051_ISR, ISR_CLR_STATUS);

        write(DM9051_IMR, IMR_PAR | IMR_PRM);
        write(DM9051_RCR, (RCR_DEFAULT | RCR_RXEN)); /* Enable RX */
    }

    virtual void begin_send(size_t length) override
    {
        configASSERT(length <= std::numeric_limits<uint16_t>::max());
        while (read(DM9051_TCR) & DM9051_TCR_SET)
        {
            usleep(5000);
        }
        write(DM9051_TXPLL, length & 0xff);
        write(DM9051_TXPLH, (length >> 8) & 0xff);
    }

    virtual void send(gsl::span<const uint8_t> buffer) override
    {
        write_memory(buffer);
    }

    virtual void end_send() override
    {
        /* Issue TX polling command */
        write(DM9051_TCR, TCR_TXREQ);
    }

    virtual size_t begin_receive() override
    {
        uint16_t len = 0;
        uint16_t status;

        read(DM9051_MRCMDX); // dummy read

        uint8_t header[4];
        read_memory({ header });
        status = header[0] | (header[1] << 8);
        len = header[2] | (header[3] << 8);
        if (len > DM9051_PKT_MAX)
        { // read-error
            len = 0; // read-error (keep no change to rx fifo)
        }

        if ((status & 0xbf00) || (len < 0x40) || (len > DM9051_PKT_MAX))
        {
            if (status & 0x8000)
            {
                printf("rx length error \r\n");
            }
            if (len > DM9051_PKT_MAX)
            {
                printf("rx length too big \r\n");
            }
        }

        return len;
    }

    virtual void receive(gsl::span<uint8_t> buffer) override
    {
        read_memory(buffer);
    }

    virtual void end_receive() override
    {
    }

    virtual void disable_rx() override
    {
        uint8_t rxchk;

        /* Disable DM9051a interrupt */
        write(DM9051_IMR, IMR_PAR);

        /* Must rx packet available */
        rxchk = read(DM9051_ISR);
        if (!(rxchk & ISR_PRS))
        {
            /* restore receive interrupt */
            //.DM9051_device.imr_all |= IMR_PRM;
            //.DM9051_Write_Reg(DM9051_IMR, DM9051_device.imr_all);
            //.return false;
        }
        /* clear the rx interrupt-event */
        write(DM9051_ISR, rxchk);
    }

    virtual void enable_rx() override
    {
        /* restore receive interrupt */
        write(DM9051_IMR, IMR_PAR | IMR_PRM);
    }

    virtual bool interface_check() override
    {
        uint8_t link_status = 0;
        link_status = read(DM9051_NSR);
        link_status = read(DM9051_NSR);
        if ((link_status)&0x40)
        {
            return true;
        }
        else
        {
            return false;
        }
    }

private:
    
    static void isr_handle(uint32_t pin, void *userdata)
    {
        auto &driver = *reinterpret_cast<dm9051_driver *>(userdata);
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xSemaphoreGiveFromISR(driver.interrupt_event_, &xHigherPriorityTaskWoken);
        if (xHigherPriorityTaskWoken)
        {
            portYIELD_FROM_ISR();
        }
    }

    uint8_t read(uint8_t addr)
    {
        const uint8_t to_write[1] = { addr };
        uint8_t to_read[1] = { 0 };
        spi_dev_->transfer_sequential({ to_write }, { to_read });
        return to_read[0];
    }

    void write(uint8_t addr, uint8_t data)
    {
        const uint8_t to_write[] = { static_cast<uint8_t>(addr | 0x80), data };
        spi_dev_->write({ to_write });
    }

    void write_phy(uint8_t addr, uint16_t data)
    {
        /* Fill the phyxcer register into REG_0C */
        write(DM9051_EPAR, DM9051_PHY | addr);

        /* Fill the written data into REG_0D & REG_0E */
        write(DM9051_EPDRL, (data & 0xff));
        write(DM9051_EPDRH, ((data >> 8) & 0xff));
        /* Issue phyxcer write command */
        write(DM9051_EPCR, 0xa);

        /* Wait write complete */
        //_DM9051_Delay_ms(500);
        while (read(DM9051_EPCR) & 0x1)
        {
            usleep(1000);
        }; //Wait complete

        /* Clear phyxcer write command */
        write(DM9051_EPCR, 0x0);
    }

    uint16_t read_phy(uint8_t addr)
    {
        /* Fill the phyxcer register into REG_0C */
        write(DM9051_EPAR, DM9051_PHY | addr);
        /* Issue phyxcer read command */
        write(DM9051_EPCR, 0xc);

        /* Wait read complete */
        //_DM9051_Delay_ms(100);
        while (read(DM9051_EPCR) & 0x1)
        {
            usleep(1000);
        }; //Wait complete

        /* Clear phyxcer read command */
        write(DM9051_EPCR, 0x0);
        return (read(DM9051_EPDRH) << 8) | read(DM9051_EPDRL);
    }

    void read_memory(gsl::span<uint8_t> buffer)
    {
        const uint8_t to_write[1] = { SPI_RD_BURST };

        spi_dev_->transfer_sequential({ to_write }, buffer);
    }

    void write_memory(gsl::span<const uint8_t> buffer)
    {
        auto new_buffer = std::make_unique<uint8_t[]>(buffer.size_bytes() + 1);
        std::copy(buffer.begin(), buffer.end(), new_buffer.get() + 1);
        new_buffer[0] = SPI_WR_BURST;
        spi_dev_->write({ new_buffer.get(), buffer.size_bytes() + 1 });
    }

    void set_mac_address(const mac_address_t &mac_addr)
    {
        write(DM9051_PAR + 0, mac_addr.data[0]);
        write(DM9051_PAR + 1, mac_addr.data[1]);
        write(DM9051_PAR + 2, mac_addr.data[2]);
        write(DM9051_PAR + 3, mac_addr.data[3]);
        write(DM9051_PAR + 4, mac_addr.data[4]);
        write(DM9051_PAR + 5, mac_addr.data[5]);

        configASSERT(read(DM9051_PAR) == mac_addr.data[0]);
    }

    void set_phy_mode(uint32_t media_mode)
    {
        uint16_t phy_reg4 = 0x01e1, phy_reg0 = 0x1000;

        if (!(media_mode & DM9051_AUTO))
        {
            switch (media_mode)
            {
            case DM9051_10MHD:
                phy_reg4 = 0x21;
                phy_reg0 = 0x0000;
                break;
            case DM9051_10MFD:
                phy_reg4 = 0x41;
                phy_reg0 = 0x1100;
                break;
            case DM9051_100MHD:
                phy_reg4 = 0x81;
                phy_reg0 = 0x2000;
                break;
            case DM9051_100MFD:
                phy_reg4 = 0x101;
                phy_reg0 = 0x3100;
                break;
            case DM9051_10M:
                phy_reg4 = 0x61;
                phy_reg0 = 0x1200;
                break;
            }

            /* Set PHY media mode */
            write_phy(4, phy_reg4);
            /* Write rphy_reg0 to Tmp */
            write_phy(0, phy_reg0);
            usleep(10000);
        }
    }

private:
    object_ptr<spi_driver> spi_driver_;
    uint32_t spi_cs_mask_;
    mac_address_t mac_address_;
    network_adapter_handler *handler_;

    object_ptr<gpio_driver> int_gpio_driver_;
    uint32_t int_gpio_pin_;

    object_accessor<gpio_driver> int_gpio_;
    object_accessor<spi_device_driver> spi_dev_;

    SemaphoreHandle_t interrupt_event_;
};

handle_t dm9051_driver_install(handle_t spi_handle, uint32_t spi_cs_mask, handle_t int_gpio_handle, uint32_t int_gpio_pin, const mac_address_t *mac_address)
{
    try
    {
        configASSERT(mac_address);
        auto driver = make_object<dm9051_driver>(spi_handle, spi_cs_mask, int_gpio_handle, int_gpio_pin, * mac_address);
        driver->install();
        return system_alloc_handle(make_accessor(driver));
    }
    catch (...)
    {
        return NULL_HANDLE;
    }
}
