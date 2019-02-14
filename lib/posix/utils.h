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
#ifndef _POSIX_UTILS_H
#define _POSIX_UTILS_H

#include <cstdint>
#include <sys/time.h>

#ifdef __cplusplus
extern "C" {
#endif

uint32_t timespec_to_ticks(const struct timespec &ts);

#ifdef __cplusplus
}
#endif
#endif