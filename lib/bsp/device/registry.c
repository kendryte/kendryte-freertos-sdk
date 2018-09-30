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
#include <driver.h>

/* System Drivers */

extern const uart_driver_t g_uart_driver_uart0;
extern const uart_driver_t g_uart_driver_uart1;
extern const uart_driver_t g_uart_driver_uart2;

extern const gpio_driver_t g_gpio_driver_gpio0;
extern const gpio_driver_t g_gpiohs_driver_gpio0;

extern const i2c_driver_t g_i2c_driver_i2c0;
extern const i2c_driver_t g_i2c_driver_i2c1;
extern const i2c_driver_t g_i2c_driver_i2c2;

extern const i2s_driver_t g_i2s_driver_i2s0;
extern const i2s_driver_t g_i2s_driver_i2s1;
extern const i2s_driver_t g_i2s_driver_i2s2;

extern const spi_driver_t g_spi_driver_spi0;
extern const spi_driver_t g_spi_driver_spi1;
extern const spi_driver_t g_spi_driver_spi3;

extern const sccb_driver_t g_sccb_driver_sccb0;

extern const dvp_driver_t g_dvp_driver_dvp0;

extern const fft_driver_t g_fft_driver_fft0;

extern const aes_driver_t g_aes_driver_aes0;

extern const sha256_driver_t g_sha_driver_sha256;

extern const timer_driver_t g_timer_driver_timer0;
extern const timer_driver_t g_timer_driver_timer1;
extern const timer_driver_t g_timer_driver_timer2;
extern const timer_driver_t g_timer_driver_timer3;
extern const timer_driver_t g_timer_driver_timer4;
extern const timer_driver_t g_timer_driver_timer5;
extern const timer_driver_t g_timer_driver_timer6;
extern const timer_driver_t g_timer_driver_timer7;
extern const timer_driver_t g_timer_driver_timer8;
extern const timer_driver_t g_timer_driver_timer9;
extern const timer_driver_t g_timer_driver_timer10;
extern const timer_driver_t g_timer_driver_timer11;

extern const pwm_driver_t g_pwm_driver_pwm0;
extern const pwm_driver_t g_pwm_driver_pwm1;
extern const pwm_driver_t g_pwm_driver_pwm2;

extern const wdt_driver_t g_wdt_driver_wdt0;
extern const wdt_driver_t g_wdt_driver_wdt1;

extern const rtc_driver_t g_rtc_driver_rtc0;

driver_registry_t g_system_drivers[] =
{
    {"/dev/uart1", &g_uart_driver_uart0, DRIVER_UART},
    {"/dev/uart2", &g_uart_driver_uart1, DRIVER_UART},
    {"/dev/uart3", &g_uart_driver_uart2, DRIVER_UART},

    {"/dev/gpio0", &g_gpiohs_driver_gpio0, DRIVER_GPIO},
    {"/dev/gpio1", &g_gpio_driver_gpio0, DRIVER_GPIO},

    {"/dev/i2c0", &g_i2c_driver_i2c0, DRIVER_I2C},
    {"/dev/i2c1", &g_i2c_driver_i2c1, DRIVER_I2C},
    {"/dev/i2c2", &g_i2c_driver_i2c2, DRIVER_I2C},

    {"/dev/i2s0", &g_i2s_driver_i2s0, DRIVER_I2S},
    {"/dev/i2s1", &g_i2s_driver_i2s1, DRIVER_I2S},
    {"/dev/i2s2", &g_i2s_driver_i2s2, DRIVER_I2S},

    {"/dev/spi0", &g_spi_driver_spi0, DRIVER_SPI},
    {"/dev/spi1", &g_spi_driver_spi1, DRIVER_SPI},
    {"/dev/spi3", &g_spi_driver_spi3, DRIVER_SPI},

    {"/dev/sccb0", &g_sccb_driver_sccb0, DRIVER_SCCB},

    {"/dev/dvp0", &g_dvp_driver_dvp0, DRIVER_DVP},

    {"/dev/fft0", &g_fft_driver_fft0, DRIVER_FFT},

    {"/dev/aes0", &g_aes_driver_aes0, DRIVER_AES},

    {"/dev/sha256", &g_sha_driver_sha256, DRIVER_SHA256},

    {"/dev/timer0", &g_timer_driver_timer0, DRIVER_TIMER},
    {"/dev/timer1", &g_timer_driver_timer1, DRIVER_TIMER},
    {"/dev/timer2", &g_timer_driver_timer2, DRIVER_TIMER},
    {"/dev/timer3", &g_timer_driver_timer3, DRIVER_TIMER},
    {"/dev/timer4", &g_timer_driver_timer4, DRIVER_TIMER},
    {"/dev/timer5", &g_timer_driver_timer5, DRIVER_TIMER},
    {"/dev/timer6", &g_timer_driver_timer6, DRIVER_TIMER},
    {"/dev/timer7", &g_timer_driver_timer7, DRIVER_TIMER},
    {"/dev/timer8", &g_timer_driver_timer8, DRIVER_TIMER},
    {"/dev/timer9", &g_timer_driver_timer9, DRIVER_TIMER},
    {"/dev/timer10", &g_timer_driver_timer10, DRIVER_TIMER},
    {"/dev/timer11", &g_timer_driver_timer11, DRIVER_TIMER},

    {"/dev/pwm0", &g_pwm_driver_pwm0, DRIVER_PWM},
    {"/dev/pwm1", &g_pwm_driver_pwm1, DRIVER_PWM},
    {"/dev/pwm2", &g_pwm_driver_pwm2, DRIVER_PWM},

    {"/dev/wdt0", &g_wdt_driver_wdt0, DRIVER_WDT},
    {"/dev/wdt1", &g_wdt_driver_wdt1, DRIVER_WDT},

    {"/dev/rtc0", &g_rtc_driver_rtc0, DRIVER_RTC},
    {0}
};

/* HAL Drivers */

extern const pic_driver_t g_pic_driver_plic0;
extern const dmac_driver_t g_dmac_driver_dmac0;

driver_registry_t g_hal_drivers[] =
{
    {"/dev/pic0", &g_pic_driver_plic0, DRIVER_PIC},
    {"/dev/dmac0", &g_dmac_driver_dmac0, DRIVER_DMAC},
    {0}
};

/* DMA Drivers */

extern const dma_driver_t g_dma_driver_dma0;
extern const dma_driver_t g_dma_driver_dma1;
extern const dma_driver_t g_dma_driver_dma2;
extern const dma_driver_t g_dma_driver_dma3;
extern const dma_driver_t g_dma_driver_dma4;
extern const dma_driver_t g_dma_driver_dma5;
driver_registry_t g_dma_drivers[] =
{
    {"/dev/dmac0/0", &g_dma_driver_dma0, DRIVER_DMA},
    {"/dev/dmac0/1", &g_dma_driver_dma1, DRIVER_DMA},
    {"/dev/dmac0/2", &g_dma_driver_dma2, DRIVER_DMA},
    {"/dev/dmac0/3", &g_dma_driver_dma3, DRIVER_DMA},
    {"/dev/dmac0/4", &g_dma_driver_dma4, DRIVER_DMA},
    {"/dev/dmac0/5", &g_dma_driver_dma5, DRIVER_DMA},
    {0}
};
