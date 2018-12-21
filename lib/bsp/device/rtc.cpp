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
#include <fpioa.h>
#include <hal.h>
#include <kernel/driver_impl.hpp>
#include <limits.h>
#include <math.h>
#include <plic.h>
#include <rtc.h>
#include <semphr.h>
#include <stdio.h>
#include <sysctl.h>
#include <utility.h>

using namespace sys;

class k_rtc_driver : public rtc_driver, public static_object, public free_object_access
{
public:
    k_rtc_driver(uintptr_t base_addr, sysctl_clock_t clock)
        : rtc_(*reinterpret_cast<volatile rtc_t *>(base_addr)), clock_(clock)
    {
    }

    virtual void install() override
    {
        sysctl_clock_disable(clock_);
    }

    virtual void on_first_open() override
    {
        sysctl_clock_enable(clock_);

        /* Unprotect RTC */
        rtc_protect_set(0);
        /* Set RTC clock frequency */
        rtc_timer_set_clock_frequency(sysctl_clock_get_freq(SYSCTL_CLOCK_IN0));
        rtc_timer_set_clock_count_value(1);

        /* Set RTC mode to timer running mode */
        rtc_timer_set_mode(RTC_TIMER_RUNNING);
    }

    virtual void on_last_close() override
    {
        sysctl_clock_disable(clock_);
    }

    virtual void get_datetime(tm &datetime) override
    {
        if (rtc_timer_get_mode() != RTC_TIMER_RUNNING)
            return;

        rtc_date_t timer_date = read_pod(rtc_.date);
        rtc_time_t timer_time = read_pod(rtc_.time);
        rtc_extended_t timer_extended = read_pod(rtc_.extended);

        datetime.tm_sec = timer_time.second % 60;
        datetime.tm_min = timer_time.minute % 60;
        datetime.tm_hour = timer_time.hour % 24;
        datetime.tm_mday = (timer_date.day - 1)% 31 + 1;
        datetime.tm_mon = (timer_date.month - 1) % 12;
        datetime.tm_year = (timer_date.year % 100) + (timer_extended.century * 100) - 1900;
        datetime.tm_wday = timer_date.week;
        datetime.tm_yday = rtc_get_yday(datetime.tm_year + 1900, datetime.tm_mon + 1, datetime.tm_mday);
        datetime.tm_isdst = -1;
    }

    virtual void set_datetime(const tm &datetime) override
    {
        rtc_date_t timer_date;
        rtc_time_t timer_time;
        rtc_extended_t timer_extended;

        /*
        * Range of tm->tm_sec could be [0,61]
        *
        * Range of tm->tm_sec allows for a positive leap second. Two
        * leap seconds in the same minute are not allowed (the C90
        * range 0..61 was a defect)
        */
        if (rtc_in_range(datetime.tm_sec, 0, 59))
            timer_time.second = datetime.tm_sec;
        else
            configASSERT(!"Invalid second.");

        /* Range of tm->tm_min could be [0,59] */
        if (rtc_in_range(datetime.tm_min, 0, 59))
            timer_time.minute = datetime.tm_min;
        else
            configASSERT(!"Invalid minute.");

        /* Range of tm->tm_hour could be [0, 23] */
        if (rtc_in_range(datetime.tm_hour, 0, 23))
            timer_time.hour = datetime.tm_hour;
        else
            configASSERT(!"Invalid hour.");

        /* Range of tm->tm_mday could be [1, 31] */
        if (rtc_in_range(datetime.tm_mday, 1, 31))
            timer_date.day = datetime.tm_mday;
        else
            configASSERT(!"Invalid day.");

        /*
        * Range of tm->tm_mon could be [0, 11]
        * But in this RTC, date.month should be [1, 12]
        */
        if (rtc_in_range(datetime.tm_mon, 0, 11))
            timer_date.month = datetime.tm_mon + 1;
        else
            configASSERT(!"Invalid month.");

        /*
        * Range of tm->tm_year is the years since 1900
        * But in this RTC, year is split into year and century
        * In this RTC, century range is [0,31], year range is [0,99]
        */
        int human_year = datetime.tm_year + 1900;
        int rtc_year = human_year % 100;
        int rtc_century = human_year / 100;

        if (rtc_in_range(rtc_year, 0, 99) && rtc_in_range(rtc_century, 0, 31))
        {
            timer_date.year = rtc_year;
            timer_extended.century = rtc_century;
        }
        else
        {
            configASSERT(!"Invalid year.");
        }

        /* Range of tm->tm_wday could be [0, 6] */
        if (rtc_in_range(datetime.tm_wday, 0, 6))
            timer_date.week = datetime.tm_wday;
        else
            configASSERT(!"Invalid day.");

        /* Set RTC mode to timer setting mode */
        rtc_timer_set_mode(RTC_TIMER_SETTING);
        /* Write value to RTC */
        write_pod(rtc_.date, timer_date);
        write_pod(rtc_.time, timer_time);
        write_pod(rtc_.extended, timer_extended);
        /* Get CPU current freq */
        unsigned long freq = sysctl_clock_get_freq(SYSCTL_CLOCK_CPU);
        /* Set threshold to 1/26000000 s */
        freq = freq / 26000000;
        /* Get current CPU cycle */
        unsigned long start_cycle = read_csr(mcycle);
        /* Wait for 1/26000000 s to sync data */
        while (read_csr(mcycle) - start_cycle < freq)
            continue;
        /* Set RTC mode to timer running mode */
        rtc_timer_set_mode(RTC_TIMER_RUNNING);
    }

private:
    static int rtc_in_range(int value, int min, int max)
    {
        return ((value >= min) && (value <= max));
    }

    static int rtc_get_wday(int year, int month, int day)
    {
        /* Magic method to get weekday */
        int weekday = (day += month < 3 ? year-- : year - 2, 23 * month / 9 + day + 4 + year / 4 - year / 100 + year / 400) % 7;
        return weekday;
    }

    static int rtc_year_is_leap(int year)
    {
        return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
    }

    static int rtc_get_yday(int year, int month, int day)
    {
        static const int days[2][13] = {
            { 0, 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 },
            { 0, 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335 }
        };
        int leap = rtc_year_is_leap(year);

        return days[leap][month] + day;
    }

    int rtc_timer_set_mode(rtc_timer_mode_t timer_mode)
    {
        rtc_register_ctrl_t register_ctrl = read_pod(rtc_.register_ctrl);

        switch (timer_mode)
        {
        case RTC_TIMER_PAUSE:
            register_ctrl.read_enable = 0;
            register_ctrl.write_enable = 0;
            break;
        case RTC_TIMER_RUNNING:
            register_ctrl.read_enable = 1;
            register_ctrl.write_enable = 0;
            break;
        case RTC_TIMER_SETTING:
            register_ctrl.read_enable = 0;
            register_ctrl.write_enable = 1;
            break;
        default:
            register_ctrl.read_enable = 0;
            register_ctrl.write_enable = 0;
            break;
        }

        write_pod(rtc_.register_ctrl, register_ctrl);

        return 0;
    }

    rtc_timer_mode_t rtc_timer_get_mode(void)
    {
        rtc_register_ctrl_t register_ctrl = read_pod(rtc_.register_ctrl);
        rtc_timer_mode_t timer_mode = RTC_TIMER_PAUSE;

        if ((!register_ctrl.read_enable) && (!register_ctrl.write_enable))
        {
            /* RTC_TIMER_PAUSE */
            timer_mode = RTC_TIMER_PAUSE;
        }
        else if ((register_ctrl.read_enable) && (!register_ctrl.write_enable))
        {
            /* RTC_TIMER_RUNNING */
            timer_mode = RTC_TIMER_RUNNING;
        }
        else if ((!register_ctrl.read_enable) && (register_ctrl.write_enable))
        {
            /* RTC_TIMER_SETTING */
            timer_mode = RTC_TIMER_SETTING;
        }
        else
        {
            /* Something is error, reset timer mode */
            rtc_timer_set_mode(timer_mode);
        }

        return timer_mode;
    }

    int rtc_protect_set(int enable)
    {
        rtc_register_ctrl_t register_ctrl = read_pod(rtc_.register_ctrl);

        rtc_mask_t mask = {
            .resv = 0,
            .second = 1,
            /* Second mask */
            .minute = 1,
            /* Minute mask */
            .hour = 1,
            /* Hour mask */
            .week = 1,
            /* Week mask */
            .day = 1,
            /* Day mask */
            .month = 1,
            /* Month mask */
            .year = 1
        };

        rtc_mask_t unmask = {
            .resv = 0,
            .second = 0,
            /* Second mask */
            .minute = 0,
            /* Minute mask */
            .hour = 0,
            /* Hour mask */
            .week = 0,
            /* Week mask */
            .day = 0,
            /* Day mask */
            .month = 0,
            /* Month mask */
            .year = 0
        };

        if (enable)
        {
            /* Turn RTC in protect mode, no one can write time */
            register_ctrl.timer_mask = *(uint8_t *)&unmask;
            register_ctrl.alarm_mask = *(uint8_t *)&unmask;
            register_ctrl.initial_count_mask = 0;
            register_ctrl.interrupt_register_mask = 0;
        }
        else
        {
            /* Turn RTC in unprotect mode, everyone can write time */
            register_ctrl.timer_mask = *(uint8_t *)&mask;
            register_ctrl.alarm_mask = *(uint8_t *)&mask;
            register_ctrl.initial_count_mask = 1;
            register_ctrl.interrupt_register_mask = 1;
        }

        write_pod(rtc_.register_ctrl, register_ctrl);
        return 0;
    }

    int rtc_timer_set_clock_frequency(unsigned int frequency)
    {
        rtc_initial_count_t initial_count;
        
        initial_count.count = frequency;
        rtc_timer_set_mode(RTC_TIMER_SETTING);
        write_pod(rtc_.initial_count, initial_count);
        rtc_timer_set_mode(RTC_TIMER_RUNNING);
        return 0;
    }

    int rtc_timer_set_clock_count_value(unsigned int count)
    {
        rtc_current_count_t current_count;

        current_count.count = count;
        rtc_timer_set_mode(RTC_TIMER_SETTING);
        write_pod(rtc_.current_count, current_count);
        rtc_timer_set_mode(RTC_TIMER_RUNNING);
        return 0;
    }

private:
    volatile rtc_t &rtc_;
    sysctl_clock_t clock_;
};

static k_rtc_driver dev0_driver(RTC_BASE_ADDR, SYSCTL_CLOCK_RTC);

driver &g_rtc_driver_rtc0 = dev0_driver;
