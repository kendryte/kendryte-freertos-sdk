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

#include <stdio.h>
#include "osdefs.h"

#ifdef __cplusplus
extern "C"
{
#endif
    
/**
 * @brief       Mount a filesystem
 *
 * @param[in]   name                The filesystem path
 * @param[in]   storage_handle      The storage device handle
 *
 * @return      result
 *     - 0      Success
 *     - other  Fail
 */
int filesystem_mount(const char *name, handle_t storage_handle);

/**
 * @brief       Open a file
 *
 * @param[in]   filename        The file path
 * @param[in]   file_access     The file access
 * @param[in]   file_mode       The file mode
 *
 * @return      result
 *     - 0      Fail
 *     - other  The file handle
 */
handle_t filesystem_file_open(const char *filename, file_access_t file_access, file_mode_t file_mode);

/**
 * @brief       Close a file handle
 *
 * @param[in]   file                The file handle
 *
 * @return      result
 *     - 0      Success
 *     - other  Fail
 */
int filesystem_file_close(handle_t file);

/**
 * @brief       Read from a file
 *
 * @param[in[   file        The file handle
 * @param[out]  buffer      The destination buffer
 * @param[in]   len         Maximum bytes to read
 *
 * @return      Actual bytes read
 */
int filesystem_file_read(handle_t file, uint8_t *buffer, size_t buffer_len);

/**
 * @brief       Write to a file
 *
 * @param[in]   file        The file handle
 * @param[in]   buffer      The source buffer
 * @param[in]   len         Bytes to write
 *
 * @return      result
 *     - len    Success
 *     - other  Fail
 */
int filesystem_file_write(handle_t file, const uint8_t *buffer, size_t buffer_len);

/**
 * @brief       Get the position of a file
 *
 * @param[in]   file        The file handle
 *
 * @return      The position
 */
fpos_t filesystem_file_get_position(handle_t file);

/**
 * @brief       Set the position of a file
 *
 * @param[in]   file            The file handle
 * @param[in]   position        The new position
 *
 * @return      result
 *     - 0      Success
 *     - other  Fail
 */
int filesystem_file_set_position(handle_t file, fpos_t position);

/**
 * @brief       Get the size of a file
 *
 * @param[in]   file        The file handle
 *
 * @return      The size
 */
uint64_t filesystem_file_get_size(handle_t file);

/**
 * @brief       Flush the buffer of a file
 *
 * @param[in]   file            The file handle
 *
 * @return      result
 *     - 0      Success
 *     - other  Fail
 */
int filesystem_file_flush(handle_t file);

/**
 * @brief       Find files in a directory
 *
 * @param[in]   path            The directory path
 * @param[in]   pattern         The search pattern
 * @param[out]  find_data       The first find data
 *
 * @return      result
 *     - 0      Fail
 *     - other  The find handle
 */
handle_t filesystem_find_first(const char *path, const char *pattern, find_find_data_t *find_data);

/**
 * @brief       Find the next file in a directory
 *
 * @param[in]   handle          The find handle
 * @param[out]  find_data       The next find data
 *
 * @return      result
 *     - true   Success
 *     - false  Fail
 */
bool filesystem_find_next(handle_t handle, find_find_data_t *find_data);

/**
 * @brief       Close a find handle
 *
 * @param[in]   file                The find handle
 *
 * @return      result
 *     - 0      Success
 *     - other  Fail
 */
int filesystem_find_close(handle_t handle);

#ifdef __cplusplus
}
#endif

#endif /* _FREERTOS_FILESYSTEM_H */