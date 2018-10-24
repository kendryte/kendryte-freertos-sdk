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
#ifndef _FREERTOS_FILESYSTEM_H
#define _FREERTOS_FILESYSTEM_H

#include <driver.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
    
/**
 * @brief       Mount a filesystem
 *
 * @param[in]   name                    The filesystem path
 * @param[in]   storage_device_name     The storage device path
 *
 * @return      result
 *     - 0      Fail
 *     - other  The filesystem handle
 */
handle_t filesystem_mount(const char *name, const char *storage_device_name);

handle_t filesystem_file_open(handle_t filesystem, const char* name);

#ifdef __cplusplus
}
#endif

#endif /* _FREERTOS_FILESYSTEM_H */