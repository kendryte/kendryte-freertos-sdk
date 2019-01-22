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
//
// Core Synchronization

#ifndef CORE_SYNC_H
#define CORE_SYNC_H

#include "FreeRTOS.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define REG_EPC 0
#define REG_RA  1
#define REG_SP  2
#define REG_A0 10
#define REG_A1 11
#define REG_A2 12
#define REG_A3 13
#define REG_A4 14
#define REG_A5 15
#define REG_A6 16
#define REG_A7 17

#define NUM_XCEPT_REGS (64)

typedef enum
{
    CORE_SYNC_NONE,
    CORE_SYNC_ADD_TCB
} core_sync_event_t;

void core_sync_request(uint64_t core_id, int event);
void core_sync_complete(uint64_t core_id);
void core_sync_awaken(uintptr_t address);

#ifdef __cplusplus
}
#endif

#endif /* CORE_SYNC_H */
