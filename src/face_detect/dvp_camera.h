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
#ifndef _DVPH_H
#define _DVPH_H
#include <stdio.h>
#include "image_process.h"

#define DVP_WIDTH 320
#define DVP_HIGHT 240

typedef struct
{
    volatile int dvp_finish_flag;
    image_t *ai_image;
    image_t *lcd_image0;
    image_t *lcd_image1;
    volatile int gram_mux;
} camera_context_t;

void dvp_init(camera_context_t *ctx);
#endif
