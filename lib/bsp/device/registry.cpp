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
#include <kernel/driver.hpp>

using namespace sys;

/* System Drivers */

extern driver &g_uart_driver_uart0;
extern driver &g_uart_driver_uart1;
extern driver &g_uart_driver_uart2;

extern driver &g_gpio_driver_gpio0;
extern driver &g_gpiohs_driver_gpio0;

extern driver &g_i2c_driver_i2c0;
extern driver &g_i2c_driver_i2c1;
extern driver &g_i2c_driver_i2c2;

extern driver &g_i2s_driver_i2s0;
extern driver &g_i2s_driver_i2s1;
extern driver &g_i2s_driver_i2s2;

extern driver &g_spi_driver_spi0;
extern driver &g_spi_driver_spi1;
extern driver &g_spi_driver_spi_slave;
extern driver &g_spi_driver_spi3;

extern driver &g_sccb_driver_sccb0;

extern driver &g_dvp_driver_dvp0;

extern driver &g_fft_driver_fft0;

extern driver &g_aes_driver_aes0;

extern driver &g_sha_driver_sha256;

extern driver &g_timer_driver_timer0;
extern driver &g_timer_driver_timer1;
extern driver &g_timer_driver_timer2;
extern driver &g_timer_driver_timer3;
extern driver &g_timer_driver_timer4;
extern driver &g_timer_driver_timer5;
extern driver &g_timer_driver_timer6;
extern driver &g_timer_driver_timer7;
extern driver &g_timer_driver_timer8;
extern driver &g_timer_driver_timer9;
extern driver &g_timer_driver_timer10;
extern driver &g_timer_driver_timer11;

extern driver &g_pwm_driver_pwm0;
extern driver &g_pwm_driver_pwm1;
extern driver &g_pwm_driver_pwm2;

extern driver &g_wdt_driver_wdt0;
extern driver &g_wdt_driver_wdt1;

extern driver &g_rtc_driver_rtc0;

extern driver &g_kpu_driver_kpu0;

driver_registry_t sys::g_system_drivers[] = {
    { "/dev/uart1", { std::in_place, &g_uart_driver_uart0 } },
    { "/dev/uart2", { std::in_place, &g_uart_driver_uart1 } },
    { "/dev/uart3", { std::in_place, &g_uart_driver_uart2 } },

    { "/dev/gpio0", { std::in_place, &g_gpiohs_driver_gpio0 } },
    { "/dev/gpio1", { std::in_place, &g_gpio_driver_gpio0 } },

    { "/dev/i2c0", { std::in_place, &g_i2c_driver_i2c0 } },
    { "/dev/i2c1", { std::in_place, &g_i2c_driver_i2c1 } },
    { "/dev/i2c2", { std::in_place, &g_i2c_driver_i2c2 } },

    { "/dev/i2s0", { std::in_place, &g_i2s_driver_i2s0 } },
    { "/dev/i2s1", { std::in_place, &g_i2s_driver_i2s1 } },
    { "/dev/i2s2", { std::in_place, &g_i2s_driver_i2s2 } },

    { "/dev/spi0", { std::in_place, &g_spi_driver_spi0 } },
    { "/dev/spi1", { std::in_place, &g_spi_driver_spi1 } },
    { "/dev/spi_slave", { std::in_place, &g_spi_driver_spi_slave } },
    { "/dev/spi3", { std::in_place, &g_spi_driver_spi3 } },

    { "/dev/sccb0", { std::in_place, &g_sccb_driver_sccb0 } },

    { "/dev/dvp0", { std::in_place, &g_dvp_driver_dvp0 } },

    { "/dev/fft0", { std::in_place, &g_fft_driver_fft0 } },

    { "/dev/aes0", { std::in_place, &g_aes_driver_aes0 } },

    { "/dev/sha256", { std::in_place, &g_sha_driver_sha256 } },

    { "/dev/timer0", { std::in_place, &g_timer_driver_timer0 } },
    { "/dev/timer1", { std::in_place, &g_timer_driver_timer1 } },
    { "/dev/timer2", { std::in_place, &g_timer_driver_timer2 } },
    { "/dev/timer3", { std::in_place, &g_timer_driver_timer3 } },
    { "/dev/timer4", { std::in_place, &g_timer_driver_timer4 } },
    { "/dev/timer5", { std::in_place, &g_timer_driver_timer5 } },
    { "/dev/timer6", { std::in_place, &g_timer_driver_timer6 } },
    { "/dev/timer7", { std::in_place, &g_timer_driver_timer7 } },
    { "/dev/timer8", { std::in_place, &g_timer_driver_timer8 } },
    { "/dev/timer9", { std::in_place, &g_timer_driver_timer9 } },
    { "/dev/timer10", { std::in_place, &g_timer_driver_timer10 } },
    { "/dev/timer11", { std::in_place, &g_timer_driver_timer11 } },

    { "/dev/pwm0", { std::in_place, &g_pwm_driver_pwm0 } },
    { "/dev/pwm1", { std::in_place, &g_pwm_driver_pwm1 } },
    { "/dev/pwm2", { std::in_place, &g_pwm_driver_pwm2 } },

    { "/dev/wdt0", { std::in_place, &g_wdt_driver_wdt0 } },
    { "/dev/wdt1", { std::in_place, &g_wdt_driver_wdt1 } },

    { "/dev/rtc0", { std::in_place, &g_rtc_driver_rtc0 } },
    { "/dev/kpu0", { std::in_place, &g_kpu_driver_kpu0 } },
    {}
};

/* HAL Drivers */

extern driver &g_pic_driver_plic0;
extern driver &g_dmac_driver_dmac0;

driver_registry_t sys::g_hal_drivers[] = {
    { "/dev/pic0", { std::in_place, &g_pic_driver_plic0 } },
    { "/dev/dmac0", { std::in_place, &g_dmac_driver_dmac0 } },
    {}
};

/* DMA Drivers */

extern driver &g_dma_driver_dma0;
extern driver &g_dma_driver_dma1;
extern driver &g_dma_driver_dma2;
extern driver &g_dma_driver_dma3;
extern driver &g_dma_driver_dma4;
extern driver &g_dma_driver_dma5;
driver_registry_t sys::g_dma_drivers[] = {
    { "/dev/dmac0/0", { std::in_place, &g_dma_driver_dma0 } },
    { "/dev/dmac0/1", { std::in_place, &g_dma_driver_dma1 } },
    { "/dev/dmac0/2", { std::in_place, &g_dma_driver_dma2 } },
    { "/dev/dmac0/3", { std::in_place, &g_dma_driver_dma3 } },
    { "/dev/dmac0/4", { std::in_place, &g_dma_driver_dma4 } },
    { "/dev/dmac0/5", { std::in_place, &g_dma_driver_dma5 } },
    {}
};
