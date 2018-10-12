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
#include <task.h>
#include <sleep.h>
#include <sysctl.h>

int nanosleep(const struct timespec* req, struct timespec* rem)
{
    uint64_t clock_sleep_ms = (uint64_t)req->tv_sec * 1000;
    uint64_t nsec_ms = req->tv_nsec / 1000000;
    uint64_t nsec_trailing = req->tv_nsec % 1000000;

    clock_sleep_ms += nsec_ms;

    if (clock_sleep_ms)
        vTaskDelay(pdMS_TO_TICKS(clock_sleep_ms));

    uint64_t microsecs = nsec_trailing / 1000;
    if (microsecs)
    {
        uint32_t cycles_per_microsec = sysctl_clock_get_freq(SYSCTL_CLOCK_CPU) / 3000000;
        while (microsecs--)
        {
            int i = cycles_per_microsec;
            while (i--)
                asm volatile("nop");
        }
    }

    return 0;
}

int usleep(useconds_t usec)
{
    /* clang-format off */
    struct timespec req =
    {
        .tv_sec = 0,
        .tv_nsec = usec * 1000
    };
    /* clang-format on */

    return nanosleep(&req, (struct timespec*)0x0);
}

unsigned int sleep(unsigned int seconds)
{
    /* clang-format off */
    struct timespec req =
    {
        .tv_sec = seconds,
        .tv_nsec = 0
    };
    /* clang-format on */

    return (unsigned int)nanosleep(&req, (struct timespec*)0x0);
}
