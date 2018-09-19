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
#ifndef _BSP_DUMP_H
#define _BSP_DUMP_H

#include <stdlib.h>
#include <string.h>
#include "syslog.h"
#include "uarths.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define DUMP_PRINTF printk

    static inline void
    dump_core(const char* reason, uintptr_t cause, uintptr_t epc)
    {
        if (CONFIG_LOG_LEVEL >= LOG_ERROR)
        {
            const char unknown_reason[] = "unknown";

            if (!reason)
                reason = unknown_reason;

            DUMP_PRINTF("core %d, core dump: %s\n", (int)read_csr(mhartid), reason);
            DUMP_PRINTF("Cause 0x%016lx, EPC 0x%016lx\n", cause, epc);
        }
    }

#undef DUMP_PRINTF

#ifdef __cplusplus
}
#endif

#endif /* _BSP_DUMP_H */
