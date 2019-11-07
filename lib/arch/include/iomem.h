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

#ifndef _BSP_IOMEM_H
#define _BSP_IOMEM_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>

#define FIX_CACHE 1
#define IOMEM_BLOCK_SIZE 256

void iomem_free(void *paddr) ;
void iomem_free_isr(void *paddr);
void *iomem_malloc(uint32_t size);
uint32_t iomem_unused();
uint32_t is_memory_cache(uintptr_t address);
#ifdef __cplusplus
}
#endif

#endif /* _BSP_IOMEM_H */
