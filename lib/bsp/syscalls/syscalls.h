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
#ifndef _BSP_SYSCALLS_H
#define _BSP_SYSCALLS_H

#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C"
{
#endif

int sys_open(const char *name, int flags, int mode);
off_t sys_lseek(int fildes, off_t offset, int whence);
int sys_fstat(int fd, struct stat *buf);

#ifdef __cplusplus
}
#endif

#endif /* _BSP_SYSCALLS_H */