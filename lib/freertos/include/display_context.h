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
#ifndef _FREERTOS_DISPLAY_CONTEXT_H
#define _FREERTOS_DISPLAY_CONTEXT_H

#include <osdefs.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
/* clang-format off */
#define LCD_X_MAX   240
#define LCD_Y_MAX   320

#define BLACK       {0, 0, 0, 0}            //0x0000
#define NAVY        {0, 0, 0.48, 0}         //0x000F
#define DARKGREEN   {0, 0.49, 0, 0}         //0x03E0
#define DARKCYAN    {0, 0.49, 0.48, 0}      //0x03EF
#define MAROON      {0.48, 0, 0, 0}         //0x7800
#define PURPLE      {0.48, 0, 0.48, 0}      //0x780F
#define OLIVE       {0.48, 0.49, 0, 0}      //0x7BE0
#define LIGHTGREY   {0.77, 0.76, 0.77, 0}   //0xC618
#define DARKGREY    {0.48, 0.49, 0.48, 0}   //0x7BEF
#define BLUE        {0, 0, 1, 0}            //0x001F
#define GREEN       {0, 1, 0, 0}            //0x07E0
#define CYAN        {0, 1, 1, 0}            //0x07FF
#define RED         {1, 0, 0, 0}            //0xF800
#define MAGENTA     {1, 0, 1, 0}            //0xF81F
#define YELLOW      {1, 1, 0, 0}            //0xFFE0
#define WHITE       {1, 1, 1, 0}            //0xFFFF
#define ORANGE      {1, 0.65, 0, 0}         //0xFD20
#define GREENYELLOW {0.68, 1, 0.16, 0}      //0xAFE5
#define PINK        {1, 0, 1, 0}            //0xF81F
/* clang-format on */

/**
 * @brief       Create a display context
 *
 * @param[in]   lcd_handle          The LCD controller handle
 *
 * @return      result
 *     - 0      Fail
 *     - other  The display context handle
 */
handle_t create_display_context(handle_t lcd_handle);

int display_screen(handle_t display_handle, const point_u_t *position, uint32_t width, uint32_t height, const uint8_t *picture);
int capture_picture(handle_t display_handle, const point_u_t *position, uint32_t width, uint32_t height, uint8_t *picture);
int clear_screen(handle_t display_handle, const point_u_t *position, uint32_t width, uint32_t height, const color_value_t *color);
#ifdef __cplusplus
}
#endif

#endif /* _DRIVERS_SDCARD_H */
