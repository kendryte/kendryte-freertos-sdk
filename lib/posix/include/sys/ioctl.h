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

#ifndef _POSIX_SYS_IOCTL_H
#define _POSIX_SYS_IOCTL_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define IOCPARM_MASK    0x7fU                   /* parameters must be < 128 bytes */
#define IOC_VOID        0x20000000UL            /* no parameters */
#define IOC_OUT         0x40000000UL            /* copy out parameters */
#define IOC_IN          0x80000000UL            /* copy in parameters */
#define IOC_INOUT       (IOC_IN | IOC_OUT)      /* 0x20000000 distinguishes new & old ioctl's */
#define _IO(x, y)       (IOC_VOID | ((x) << 8) | (y))

#define _IOR(x, y, t) (IOC_OUT | (((unsigned int)sizeof(t) & IOCPARM_MASK) << 16) | ((x) << 8) | (y))

#define _IOW(x, y, t) (IOC_IN | (((unsigned int)sizeof(t) & IOCPARM_MASK) << 16) | ((x) << 8) | (y))

#ifndef FIONREAD
#define FIONREAD _IOR('f', 127, unsigned int)   /* get # bytes to read */
#endif
#ifndef FIONBIO
#define FIONBIO _IOW('f', 126, unsigned int)    /* set/clear non-blocking i/o */
#endif

int ioctl(int handle, unsigned int cmd, void *argp);

#ifdef __cplusplus
}
#endif

#endif