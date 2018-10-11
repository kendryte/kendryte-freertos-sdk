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
#ifndef _BSP_PLATFORM_H
#define _BSP_PLATFORM_H

/* clang-format off */
/* Register base address */

/* Under Coreplex */
#define CLINT_BASE_ADDR     (0x02000000)
#define PLIC_BASE_ADDR      (0x0C000000)

/* Under TileLink */
#define UARTHS_BASE_ADDR    (0x38000000)
#define GPIOHS_BASE_ADDR    (0x38001000)

/* Under AXI 64 bit */
#define RAM_BASE_ADDR       (0x80000000)
#define RAM_SIZE            (6 * 1024 * 1024)

#define IO_BASE_ADDR        (0x40000000)
#define IO_SIZE             (6 * 1024 * 1024)

#define AI_RAM_BASE_ADDR    (0x80600000)
#define AI_RAM_SIZE         (2 * 1024 * 1024)

#define AI_IO_BASE_ADDR     (0x40600000)
#define AI_IO_SIZE          (2 * 1024 * 1024)

#define AI_BASE_ADDR        (0x40800000)
#define AI_SIZE             (12 * 1024 * 1024)

#define FFT_BASE_ADDR       (0x42000000)
#define FFT_SIZE            (4 * 1024 * 1024)

#define ROM_BASE_ADDR       (0x88000000)
#define ROM_SIZE            (128 * 1024)

/* Under AHB 32 bit */
#define DMAC_BASE_ADDR      (0x50000000)

/* Under APB1 32 bit */
#define GPIO_BASE_ADDR      (0x50200000)
#define UART1_BASE_ADDR     (0x50210000)
#define UART2_BASE_ADDR     (0x50220000)
#define UART3_BASE_ADDR     (0x50230000)
#define SPI_SLAVE_BASE_ADDR (0x50240000)
#define I2S0_BASE_ADDR      (0x50250000)
#define I2S1_BASE_ADDR      (0x50260000)
#define I2S2_BASE_ADDR      (0x50270000)
#define I2C0_BASE_ADDR      (0x50280000)
#define I2C1_BASE_ADDR      (0x50290000)
#define I2C2_BASE_ADDR      (0x502A0000)
#define FPIOA_BASE_ADDR     (0x502B0000)
#define SHA256_BASE_ADDR    (0x502C0000)
#define TIMER0_BASE_ADDR    (0x502D0000)
#define TIMER1_BASE_ADDR    (0x502E0000)
#define TIMER2_BASE_ADDR    (0x502F0000)

/* Under APB2 32 bit */
#define WDT0_BASE_ADDR      (0x50400000)
#define WDT1_BASE_ADDR      (0x50410000)
#define OTP_BASE_ADDR       (0x50420000)
#define DVP_BASE_ADDR       (0x50430000)
#define SYSCTL_BASE_ADDR    (0x50440000)
#define AES_BASE_ADDR       (0x50450000)
#define RTC_BASE_ADDR       (0x50460000)


/* Under APB3 32 bit */
#define SPI0_BASE_ADDR      (0x52000000)
#define SPI1_BASE_ADDR      (0x53000000)
#define SPI3_BASE_ADDR      (0x54000000)

/* clang-format on */

/**
 * @brief       PLIC External Interrupt Numbers
 *
 * @note        PLIC interrupt sources
 *
 * | Source | Name                     | Description                        |
 * |--------|--------------------------|------------------------------------|
 * | 0      | IRQN_NO_INTERRUPT        | The non-existent interrupt         |
 * | 1      | IRQN_SPI0_INTERRUPT      | SPI0 interrupt                     |
 * | 2      | IRQN_SPI1_INTERRUPT      | SPI1 interrupt                     |
 * | 3      | IRQN_SPI_SLAVE_INTERRUPT | SPI_SLAVE interrupt                |
 * | 4      | IRQN_SPI3_INTERRUPT      | SPI3 interrupt                     |
 * | 5      | IRQN_I2S0_INTERRUPT      | I2S0 interrupt                     |
 * | 6      | IRQN_I2S1_INTERRUPT      | I2S1 interrupt                     |
 * | 7      | IRQN_I2S2_INTERRUPT      | I2S2 interrupt                     |
 * | 8      | IRQN_I2C0_INTERRUPT      | I2C0 interrupt                     |
 * | 9      | IRQN_I2C1_INTERRUPT      | I2C1 interrupt                     |
 * | 10     | IRQN_I2C2_INTERRUPT      | I2C2 interrupt                     |
 * | 11     | IRQN_UART1_INTERRUPT     | UART1 interrupt                    |
 * | 12     | IRQN_UART2_INTERRUPT     | UART2 interrupt                    |
 * | 13     | IRQN_UART3_INTERRUPT     | UART3 interrupt                    |
 * | 14     | IRQN_TIMER0A_INTERRUPT   | TIMER0 channel 0 or 1 interrupt    |
 * | 15     | IRQN_TIMER0B_INTERRUPT   | TIMER0 channel 2 or 3 interrupt    |
 * | 16     | IRQN_TIMER1A_INTERRUPT   | TIMER1 channel 0 or 1 interrupt    |
 * | 17     | IRQN_TIMER1B_INTERRUPT   | TIMER1 channel 2 or 3 interrupt    |
 * | 18     | IRQN_TIMER2A_INTERRUPT   | TIMER2 channel 0 or 1 interrupt    |
 * | 19     | IRQN_TIMER2B_INTERRUPT   | TIMER2 channel 2 or 3 interrupt    |
 * | 20     | IRQN_RTC_INTERRUPT       | RTC tick and alarm interrupt       |
 * | 21     | IRQN_WDT0_INTERRUPT      | Watching dog timer0 interrupt      |
 * | 22     | IRQN_WDT1_INTERRUPT      | Watching dog timer1 interrupt      |
 * | 23     | IRQN_APB_GPIO_INTERRUPT  | APB GPIO interrupt                 |
 * | 24     | IRQN_DVP_INTERRUPT       | Digital video port interrupt       |
 * | 25     | IRQN_AI_INTERRUPT        | AI accelerator interrupt           |
 * | 26     | IRQN_FFT_INTERRUPT       | FFT accelerator interrupt          |
 * | 27     | IRQN_DMA0_INTERRUPT      | DMA channel0 interrupt             |
 * | 28     | IRQN_DMA1_INTERRUPT      | DMA channel1 interrupt             |
 * | 29     | IRQN_DMA2_INTERRUPT      | DMA channel2 interrupt             |
 * | 30     | IRQN_DMA3_INTERRUPT      | DMA channel3 interrupt             |
 * | 31     | IRQN_DMA4_INTERRUPT      | DMA channel4 interrupt             |
 * | 32     | IRQN_DMA5_INTERRUPT      | DMA channel5 interrupt             |
 * | 33     | IRQN_UARTHS_INTERRUPT    | Hi-speed UART0 interrupt           |
 * | 34     | IRQN_GPIOHS0_INTERRUPT   | Hi-speed GPIO0 interrupt           |
 * | 35     | IRQN_GPIOHS1_INTERRUPT   | Hi-speed GPIO1 interrupt           |
 * | 36     | IRQN_GPIOHS2_INTERRUPT   | Hi-speed GPIO2 interrupt           |
 * | 37     | IRQN_GPIOHS3_INTERRUPT   | Hi-speed GPIO3 interrupt           |
 * | 38     | IRQN_GPIOHS4_INTERRUPT   | Hi-speed GPIO4 interrupt           |
 * | 39     | IRQN_GPIOHS5_INTERRUPT   | Hi-speed GPIO5 interrupt           |
 * | 40     | IRQN_GPIOHS6_INTERRUPT   | Hi-speed GPIO6 interrupt           |
 * | 41     | IRQN_GPIOHS7_INTERRUPT   | Hi-speed GPIO7 interrupt           |
 * | 42     | IRQN_GPIOHS8_INTERRUPT   | Hi-speed GPIO8 interrupt           |
 * | 43     | IRQN_GPIOHS9_INTERRUPT   | Hi-speed GPIO9 interrupt           |
 * | 44     | IRQN_GPIOHS10_INTERRUPT  | Hi-speed GPIO10 interrupt          |
 * | 45     | IRQN_GPIOHS11_INTERRUPT  | Hi-speed GPIO11 interrupt          |
 * | 46     | IRQN_GPIOHS12_INTERRUPT  | Hi-speed GPIO12 interrupt          |
 * | 47     | IRQN_GPIOHS13_INTERRUPT  | Hi-speed GPIO13 interrupt          |
 * | 48     | IRQN_GPIOHS14_INTERRUPT  | Hi-speed GPIO14 interrupt          |
 * | 49     | IRQN_GPIOHS15_INTERRUPT  | Hi-speed GPIO15 interrupt          |
 * | 50     | IRQN_GPIOHS16_INTERRUPT  | Hi-speed GPIO16 interrupt          |
 * | 51     | IRQN_GPIOHS17_INTERRUPT  | Hi-speed GPIO17 interrupt          |
 * | 52     | IRQN_GPIOHS18_INTERRUPT  | Hi-speed GPIO18 interrupt          |
 * | 53     | IRQN_GPIOHS19_INTERRUPT  | Hi-speed GPIO19 interrupt          |
 * | 54     | IRQN_GPIOHS20_INTERRUPT  | Hi-speed GPIO20 interrupt          |
 * | 55     | IRQN_GPIOHS21_INTERRUPT  | Hi-speed GPIO21 interrupt          |
 * | 56     | IRQN_GPIOHS22_INTERRUPT  | Hi-speed GPIO22 interrupt          |
 * | 57     | IRQN_GPIOHS23_INTERRUPT  | Hi-speed GPIO23 interrupt          |
 * | 58     | IRQN_GPIOHS24_INTERRUPT  | Hi-speed GPIO24 interrupt          |
 * | 59     | IRQN_GPIOHS25_INTERRUPT  | Hi-speed GPIO25 interrupt          |
 * | 60     | IRQN_GPIOHS26_INTERRUPT  | Hi-speed GPIO26 interrupt          |
 * | 61     | IRQN_GPIOHS27_INTERRUPT  | Hi-speed GPIO27 interrupt          |
 * | 62     | IRQN_GPIOHS28_INTERRUPT  | Hi-speed GPIO28 interrupt          |
 * | 63     | IRQN_GPIOHS29_INTERRUPT  | Hi-speed GPIO29 interrupt          |
 * | 64     | IRQN_GPIOHS30_INTERRUPT  | Hi-speed GPIO30 interrupt          |
 * | 65     | IRQN_GPIOHS31_INTERRUPT  | Hi-speed GPIO31 interrupt          |
 *
 */
/* clang-format off */
typedef enum _plic_irq
{
    IRQN_NO_INTERRUPT        = 0, /*!< The non-existent interrupt */
    IRQN_SPI0_INTERRUPT      = 1, /*!< SPI0 interrupt */
    IRQN_SPI1_INTERRUPT      = 2, /*!< SPI1 interrupt */
    IRQN_SPI_SLAVE_INTERRUPT = 3, /*!< SPI_SLAVE interrupt */
    IRQN_SPI3_INTERRUPT      = 4, /*!< SPI3 interrupt */
    IRQN_I2S0_INTERRUPT      = 5, /*!< I2S0 interrupt */
    IRQN_I2S1_INTERRUPT      = 6, /*!< I2S1 interrupt */
    IRQN_I2S2_INTERRUPT      = 7, /*!< I2S2 interrupt */
    IRQN_I2C0_INTERRUPT      = 8, /*!< I2C0 interrupt */
    IRQN_I2C1_INTERRUPT      = 9, /*!< I2C1 interrupt */
    IRQN_I2C2_INTERRUPT      = 10, /*!< I2C2 interrupt */
    IRQN_UART1_INTERRUPT     = 11, /*!< UART1 interrupt */
    IRQN_UART2_INTERRUPT     = 12, /*!< UART2 interrupt */
    IRQN_UART3_INTERRUPT     = 13, /*!< UART3 interrupt */
    IRQN_TIMER0A_INTERRUPT   = 14, /*!< TIMER0 channel 0 or 1 interrupt */
    IRQN_TIMER0B_INTERRUPT   = 15, /*!< TIMER0 channel 2 or 3 interrupt */
    IRQN_TIMER1A_INTERRUPT   = 16, /*!< TIMER1 channel 0 or 1 interrupt */
    IRQN_TIMER1B_INTERRUPT   = 17, /*!< TIMER1 channel 2 or 3 interrupt */
    IRQN_TIMER2A_INTERRUPT   = 18, /*!< TIMER2 channel 0 or 1 interrupt */
    IRQN_TIMER2B_INTERRUPT   = 19, /*!< TIMER2 channel 2 or 3 interrupt */
    IRQN_RTC_INTERRUPT       = 20, /*!< RTC tick and alarm interrupt */
    IRQN_WDT0_INTERRUPT      = 21, /*!< Watching dog timer0 interrupt */
    IRQN_WDT1_INTERRUPT      = 22, /*!< Watching dog timer1 interrupt */
    IRQN_APB_GPIO_INTERRUPT  = 23, /*!< APB GPIO interrupt */
    IRQN_DVP_INTERRUPT       = 24, /*!< Digital video port interrupt */
    IRQN_AI_INTERRUPT        = 25, /*!< AI accelerator interrupt */
    IRQN_FFT_INTERRUPT       = 26, /*!< FFT accelerator interrupt */
    IRQN_DMA0_INTERRUPT      = 27, /*!< DMA channel0 interrupt */
    IRQN_DMA1_INTERRUPT      = 28, /*!< DMA channel1 interrupt */
    IRQN_DMA2_INTERRUPT      = 29, /*!< DMA channel2 interrupt */
    IRQN_DMA3_INTERRUPT      = 30, /*!< DMA channel3 interrupt */
    IRQN_DMA4_INTERRUPT      = 31, /*!< DMA channel4 interrupt */
    IRQN_DMA5_INTERRUPT      = 32, /*!< DMA channel5 interrupt */
    IRQN_UARTHS_INTERRUPT    = 33, /*!< Hi-speed UART0 interrupt */
    IRQN_GPIOHS0_INTERRUPT   = 34, /*!< Hi-speed GPIO0 interrupt */
    IRQN_GPIOHS1_INTERRUPT   = 35, /*!< Hi-speed GPIO1 interrupt */
    IRQN_GPIOHS2_INTERRUPT   = 36, /*!< Hi-speed GPIO2 interrupt */
    IRQN_GPIOHS3_INTERRUPT   = 37, /*!< Hi-speed GPIO3 interrupt */
    IRQN_GPIOHS4_INTERRUPT   = 38, /*!< Hi-speed GPIO4 interrupt */
    IRQN_GPIOHS5_INTERRUPT   = 39, /*!< Hi-speed GPIO5 interrupt */
    IRQN_GPIOHS6_INTERRUPT   = 40, /*!< Hi-speed GPIO6 interrupt */
    IRQN_GPIOHS7_INTERRUPT   = 41, /*!< Hi-speed GPIO7 interrupt */
    IRQN_GPIOHS8_INTERRUPT   = 42, /*!< Hi-speed GPIO8 interrupt */
    IRQN_GPIOHS9_INTERRUPT   = 43, /*!< Hi-speed GPIO9 interrupt */
    IRQN_GPIOHS10_INTERRUPT  = 44, /*!< Hi-speed GPIO10 interrupt */
    IRQN_GPIOHS11_INTERRUPT  = 45, /*!< Hi-speed GPIO11 interrupt */
    IRQN_GPIOHS12_INTERRUPT  = 46, /*!< Hi-speed GPIO12 interrupt */
    IRQN_GPIOHS13_INTERRUPT  = 47, /*!< Hi-speed GPIO13 interrupt */
    IRQN_GPIOHS14_INTERRUPT  = 48, /*!< Hi-speed GPIO14 interrupt */
    IRQN_GPIOHS15_INTERRUPT  = 49, /*!< Hi-speed GPIO15 interrupt */
    IRQN_GPIOHS16_INTERRUPT  = 50, /*!< Hi-speed GPIO16 interrupt */
    IRQN_GPIOHS17_INTERRUPT  = 51, /*!< Hi-speed GPIO17 interrupt */
    IRQN_GPIOHS18_INTERRUPT  = 52, /*!< Hi-speed GPIO18 interrupt */
    IRQN_GPIOHS19_INTERRUPT  = 53, /*!< Hi-speed GPIO19 interrupt */
    IRQN_GPIOHS20_INTERRUPT  = 54, /*!< Hi-speed GPIO20 interrupt */
    IRQN_GPIOHS21_INTERRUPT  = 55, /*!< Hi-speed GPIO21 interrupt */
    IRQN_GPIOHS22_INTERRUPT  = 56, /*!< Hi-speed GPIO22 interrupt */
    IRQN_GPIOHS23_INTERRUPT  = 57, /*!< Hi-speed GPIO23 interrupt */
    IRQN_GPIOHS24_INTERRUPT  = 58, /*!< Hi-speed GPIO24 interrupt */
    IRQN_GPIOHS25_INTERRUPT  = 59, /*!< Hi-speed GPIO25 interrupt */
    IRQN_GPIOHS26_INTERRUPT  = 60, /*!< Hi-speed GPIO26 interrupt */
    IRQN_GPIOHS27_INTERRUPT  = 61, /*!< Hi-speed GPIO27 interrupt */
    IRQN_GPIOHS28_INTERRUPT  = 62, /*!< Hi-speed GPIO28 interrupt */
    IRQN_GPIOHS29_INTERRUPT  = 63, /*!< Hi-speed GPIO29 interrupt */
    IRQN_GPIOHS30_INTERRUPT  = 64, /*!< Hi-speed GPIO30 interrupt */
    IRQN_GPIOHS31_INTERRUPT  = 65, /*!< Hi-speed GPIO31 interrupt */
    IRQN_MAX
} plic_irq_t;
/* clang-format on */

typedef enum _sysctl_dma_select_t
{
    SYSCTL_DMA_SELECT_SSI0_RX_REQ,
    SYSCTL_DMA_SELECT_SSI0_TX_REQ,
    SYSCTL_DMA_SELECT_SSI1_RX_REQ,
    SYSCTL_DMA_SELECT_SSI1_TX_REQ,
    SYSCTL_DMA_SELECT_SSI2_RX_REQ,
    SYSCTL_DMA_SELECT_SSI2_TX_REQ,
    SYSCTL_DMA_SELECT_SSI3_RX_REQ,
    SYSCTL_DMA_SELECT_SSI3_TX_REQ,
    SYSCTL_DMA_SELECT_I2C0_RX_REQ,
    SYSCTL_DMA_SELECT_I2C0_TX_REQ,
    SYSCTL_DMA_SELECT_I2C1_RX_REQ,
    SYSCTL_DMA_SELECT_I2C1_TX_REQ,
    SYSCTL_DMA_SELECT_I2C2_RX_REQ,
    SYSCTL_DMA_SELECT_I2C2_TX_REQ,
    SYSCTL_DMA_SELECT_UART1_RX_REQ,
    SYSCTL_DMA_SELECT_UART1_TX_REQ,
    SYSCTL_DMA_SELECT_UART2_RX_REQ,
    SYSCTL_DMA_SELECT_UART2_TX_REQ,
    SYSCTL_DMA_SELECT_UART3_RX_REQ,
    SYSCTL_DMA_SELECT_UART3_TX_REQ,
    SYSCTL_DMA_SELECT_AES_REQ,
    SYSCTL_DMA_SELECT_SHA_RX_REQ,
    SYSCTL_DMA_SELECT_AI_RX_REQ,
    SYSCTL_DMA_SELECT_FFT_RX_REQ,
    SYSCTL_DMA_SELECT_FFT_TX_REQ,
    SYSCTL_DMA_SELECT_I2S0_TX_REQ,
    SYSCTL_DMA_SELECT_I2S0_RX_REQ,
    SYSCTL_DMA_SELECT_I2S1_TX_REQ,
    SYSCTL_DMA_SELECT_I2S1_RX_REQ,
    SYSCTL_DMA_SELECT_I2S2_TX_REQ,
    SYSCTL_DMA_SELECT_I2S2_RX_REQ,
    SYSCTL_DMA_SELECT_I2S0_BF_DIR_REQ,
    SYSCTL_DMA_SELECT_I2S0_BF_VOICE_REQ,
    SYSCTL_DMA_SELECT_MAX
} sysctl_dma_select_t;

#endif /* _BSP_PLATFORM_H */
