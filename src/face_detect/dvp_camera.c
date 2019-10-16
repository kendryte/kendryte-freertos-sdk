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
#include <sys/unistd.h>
#include <devices.h>
#include "dvp_camera.h"
#include "printf.h"

enum _data_for
{
    DATA_FOR_AI = 0,
    DATA_FOR_DISPLAY = 1,
} ;

enum _enable
{
    DISABLE = 0,
    ENABLE = 1,
} ;

handle_t file_dvp;

void sensor_restart()
{
    dvp_set_signal(file_dvp, DVP_SIG_POWER_DOWN, 1);
    usleep(200 * 1000);
    dvp_set_signal(file_dvp, DVP_SIG_POWER_DOWN, 0);
    usleep(200 * 1000);
    dvp_set_signal(file_dvp, DVP_SIG_RESET, 0);
    usleep(200 * 1000);
    dvp_set_signal(file_dvp, DVP_SIG_RESET, 1);
    usleep(200 * 1000);
}

void on_irq_dvp(dvp_frame_event_t event, void* userdata)
{
    camera_context_t *ctx = (camera_context_t *)userdata;
    //printk("ctx->gram_mux = %d \n", ctx->gram_mux);
    switch (event)
    {
        case VIDEO_FE_BEGIN:
            dvp_enable_frame(file_dvp);
            break;
        case VIDEO_FE_END:
            dvp_set_output_attributes(file_dvp, DATA_FOR_DISPLAY, VIDEO_FMT_RGB565, ctx->gram_mux ? ctx->lcd_image0->addr : ctx->lcd_image1->addr);
            ctx->dvp_finish_flag = 1;
            break;
        default:
            configASSERT(!"Invalid event.");
    }
}

void dvp_init(camera_context_t *ctx)
{
    file_dvp = io_open("/dev/dvp0");
    configASSERT(file_dvp);
    sensor_restart();
    dvp_xclk_set_clock_rate(file_dvp, 20000000); /* 20MHz XCLK*/
    dvp_config(file_dvp, DVP_WIDTH, DVP_HIGHT, DISABLE);

    dvp_set_output_enable(file_dvp, DATA_FOR_AI, ENABLE);
    dvp_set_output_enable(file_dvp, DATA_FOR_DISPLAY, ENABLE);

    dvp_set_output_attributes(file_dvp, DATA_FOR_DISPLAY, VIDEO_FMT_RGB565, (void*)ctx->lcd_image0->addr);

    dvp_set_output_attributes(file_dvp, DATA_FOR_AI, VIDEO_FMT_RGB24_PLANAR, (void*)ctx->ai_image->addr);

    dvp_set_frame_event_enable(file_dvp, VIDEO_FE_END, DISABLE);
    dvp_set_frame_event_enable(file_dvp, VIDEO_FE_BEGIN, DISABLE);

    dvp_set_on_frame_event(file_dvp, on_irq_dvp, ctx);

    dvp_set_frame_event_enable(file_dvp, VIDEO_FE_END, ENABLE);
    dvp_set_frame_event_enable(file_dvp, VIDEO_FE_BEGIN, ENABLE);
}
