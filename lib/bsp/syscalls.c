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
#include "syscalls/syscalls.h"
#include <atomic.h>
#include <clint.h>
#include <devices.h>
#include <dump.h>
#include <errno.h>
#include <filesystem.h>
#include <fpioa.h>
#include <interrupt.h>
#include <limits.h>
#include <machine/syscall.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/unistd.h>
#include <sysctl.h>
#include <syslog.h>
#include <uarths.h>

/*
 * @note       System call list
 *
 * See also riscv-newlib/libgloss/riscv/syscalls.c
 *
 * | System call      | Number |
 * |------------------|--------|
 * | SYS_exit         | 93     |
 * | SYS_exit_group   | 94     |
 * | SYS_getpid       | 172    |
 * | SYS_kill         | 129    |
 * | SYS_read         | 63     |
 * | SYS_write        | 64     |
 * | SYS_open         | 1024   |
 * | SYS_openat       | 56     |
 * | SYS_close        | 57     |
 * | SYS_lseek        | 62     |
 * | SYS_brk          | 214    |
 * | SYS_link         | 1025   |
 * | SYS_unlink       | 1026   |
 * | SYS_mkdir        | 1030   |
 * | SYS_chdir        | 49     |
 * | SYS_getcwd       | 17     |
 * | SYS_stat         | 1038   |
 * | SYS_fstat        | 80     |
 * | SYS_lstat        | 1039   |
 * | SYS_fstatat      | 79     |
 * | SYS_access       | 1033   |
 * | SYS_faccessat    | 48     |
 * | SYS_pread        | 67     |
 * | SYS_pwrite       | 68     |
 * | SYS_uname        | 160    |
 * | SYS_getuid       | 174    |
 * | SYS_geteuid      | 175    |
 * | SYS_getgid       | 176    |
 * | SYS_getegid      | 177    |
 * | SYS_mmap         | 222    |
 * | SYS_munmap       | 215    |
 * | SYS_mremap       | 216    |
 * | SYS_time         | 1062   |
 * | SYS_getmainvars  | 2011   |
 * | SYS_rt_sigaction | 134    |
 * | SYS_writev       | 66     |
 * | SYS_gettimeofday | 169    |
 * | SYS_times        | 153    |
 * | SYS_fcntl        | 25     |
 * | SYS_getdents     | 61     |
 * | SYS_dup          | 23     |
 *
 */

#ifndef UNUSED
#define UNUSED(x) (void)(x)
#endif

static const char *TAG = "SYSCALL";

extern char _heap_start[];
extern char _heap_end[];
extern void sys_apc_thunk();
char *_heap_cur = &_heap_start[0];
char *_heap_line = &_heap_start[0];
char *_ioheap_line = &_heap_end[0]-0x40000000;

void __attribute__((noreturn)) sys_exit(int code)
{
    /* First print some diagnostic information. */
    LOGW(TAG, "sys_exit called with 0x%lx\n", (uint64_t)code);
    while (1)
        ;
}

static int sys_nosys(long a0, long a1, long a2, long a3, long a4, long a5, unsigned long n)
{
    UNUSED(a3);
    UNUSED(a4);
    UNUSED(a5);

    LOGW(TAG, "Unimplemented syscall 0x%lx\n", (uint64_t)n);
    exit(ENOSYS);

    return -ENOSYS;
}

static int sys_success(void)
{
    return 0;
}

static size_t sys_brk(size_t pos)
{
    uintptr_t res = 0;
    /*
     * brk() sets the end of the data segment to the value
     * specified by addr, when that value is reasonable, the system
     * has enough memory, and the process does not exceed its
     * maximum data size.
     *
     * sbrk() increments the program's data space by increment
     * bytes. Calling sbrk() with an increment of 0 can be used to
     * find the current location of the program break.
     *
     * uintptr_t brk(uintptr_t ptr);
     *
     * IN : regs[10] = ptr
     * OUT: regs[10] = ptr
     */

    /*
      * - First call: Initialization brk pointer. newlib will pass
      *   ptr = 0 when it is first called. In this case the address
      *   _heap_start will be return.
      *
      * - Call again: Adjust brk pointer. The ptr never equal with
      *   0. If ptr is below _heap_end, then allocate memory.
      *   Otherwise throw out of memory error, return -1.
      */

    if (pos)
    {
        /* Call again */
        if ((uintptr_t)pos > (uintptr_t)&_heap_end[0])
        {
            res = -ENOMEM;
            printk("OUT OF MEMORY \n");
        }
        else
        {
            if((uintptr_t)pos > (uintptr_t)_heap_line)
            {
                _heap_line = (char *)(uintptr_t)pos;
                if((uintptr_t)_heap_line-0x40000000 > (uintptr_t)_ioheap_line)
                {
                    LOGE(TAG, "WARNING: cache heap line %p > iomem heap line %p!\r\n", _heap_line, _ioheap_line);
                }
            }
            /* Adjust brk pointer. */
            _heap_cur = (char *)(uintptr_t)pos;
            /* Return current address. */
            res = (uintptr_t)_heap_cur;
        }
    }
    else
    {
        /* First call, return initial address */
        res = (uintptr_t)&_heap_start[0];
    }
    return (size_t)res;
}

static ssize_t sys_write(int file, const void *ptr, size_t len)
{
    /*
     * Write to a file.
     *
     * ssize_t write(int file, const void* ptr, size_t len)
     *
     * IN : regs[10] = file, regs[11] = ptr, regs[12] = len
     * OUT: regs[10] = len
     */

    ssize_t res = -EBADF;
    /* Get size to write */
    size_t length = len;
    /* Get data pointer */
    const uint8_t *data = (const uint8_t *)ptr;

    if (STDOUT_FILENO == file || STDERR_FILENO == file)
    {
        /* Write data */
        while (length--)
            uarths_write_byte(*data++);

        /* Return the actual size written */
        res = len;
    }
    else
    {
        res = io_write(file, data, length);
    }

    return res;
}

static ssize_t sys_read(int file, void *ptr, size_t len)
{
    ssize_t res = -EBADF;

    if (STDIN_FILENO == file)
    {
        return uarths_read((uint8_t *)ptr, len);
    }
    else
    {
        res = io_read(file, (uint8_t *)ptr, len);
    }

    return res;
}

static int sys_close(int file)
{
    /*
     * Close a file.
     *
     * int close(int file)
     *
     * IN : regs[10] = file
     * OUT: regs[10] = Upon successful completion, 0 shall be
     * returned.
     * Otherwise, -1 shall be returned and errno set to indicate
     * the error.
     */
    if (STDOUT_FILENO == file || STDERR_FILENO == file)
    {
        return 0;
    }
    else
    {
        return io_close(file);
    }
}

static int sys_gettimeofday(struct timeval *tp, void *tzp)
{
    /*
     * Get the current time.  Only relatively correct.
     *
     * int gettimeofday(struct timeval* tp, void* tzp)
     *
     * IN : regs[10] = tp
     * OUT: regs[10] = Upon successful completion, 0 shall be
     * returned.
     * Otherwise, -1 shall be returned and errno set to indicate
     * the error.
     */
    UNUSED(tzp);

    if (tp != NULL)
    {
        uint64_t clint_usec = clint->mtime * CLINT_CLOCK_DIV / (sysctl_clock_get_freq(SYSCTL_CLOCK_CPU) / 1000000UL);

        tp->tv_sec = clint_usec / 1000000UL;
        tp->tv_usec = clint_usec % 1000000UL;
    }
    /* Return the result */
    return 0;
}

static void handle_ecall(uintptr_t *regs)
{
    enum syscall_id_e
    {
        SYS_ID_NOSYS,
        SYS_ID_SUCCESS,
        SYS_ID_EXIT,
        SYS_ID_BRK,
        SYS_ID_READ,
        SYS_ID_WRITE,
        SYS_ID_OPEN,
        SYS_ID_FSTAT,
        SYS_ID_CLOSE,
        SYS_ID_GETTIMEOFDAY,
        SYS_ID_LSEEK,
        SYS_ID_MAX
    };

    static uintptr_t (*const syscall_table[])(long a0, long a1, long a2, long a3, long a4, long a5, unsigned long n) = {
        [SYS_ID_NOSYS] = (void *)sys_nosys,
        [SYS_ID_SUCCESS] = (void *)sys_success,
        [SYS_ID_EXIT] = (void *)sys_exit,
        [SYS_ID_BRK] = (void *)sys_brk,
        [SYS_ID_READ] = (void *)sys_read,
        [SYS_ID_WRITE] = (void *)sys_write,
        [SYS_ID_OPEN] = (void *)sys_open,
        [SYS_ID_FSTAT] = (void *)sys_fstat,
        [SYS_ID_CLOSE] = (void *)sys_close,
        [SYS_ID_GETTIMEOFDAY] = (void *)sys_gettimeofday,
        [SYS_ID_LSEEK] = (void *)sys_lseek
    };

#if defined(__GNUC__)
#pragma GCC diagnostic ignored "-Woverride-init"
#endif

    uintptr_t n = regs[REG_A7];

    if (n == SYS_apc_return)
    {
        regs[REG_EPC] = regs[REG_APC_RET];
        regs[REG_A7] = regs[REG_APC_PROC];
    }
    else
    {
        uintptr_t syscall_id = SYS_ID_NOSYS;
        switch (n)
        {
        case SYS_exit:
        case SYS_exit_group:
            syscall_id = SYS_ID_EXIT;
            break;
        case SYS_read:
            syscall_id = SYS_ID_READ;
            break;
        case SYS_write:
            syscall_id = SYS_ID_WRITE;
            break;
        case SYS_open:
            syscall_id = SYS_ID_OPEN;
            break;
        case SYS_close:
            syscall_id = SYS_ID_CLOSE;
            break;
        case SYS_lseek:
            syscall_id = SYS_ID_LSEEK;
            break;
        case SYS_brk:
            syscall_id = SYS_ID_BRK;
            break;
        case SYS_fstat:
            syscall_id = SYS_ID_FSTAT;
            break;
        case SYS_gettimeofday:
            syscall_id = SYS_ID_GETTIMEOFDAY;
            break;
        }
#if defined(__GNUC__)
#pragma GCC diagnostic warning "-Woverride-init"
#endif
        regs[REG_APC_PROC] = (uintptr_t)syscall_table[syscall_id];
        regs[REG_APC_RET] = regs[REG_EPC] + 4;
        regs[REG_EPC] = (uintptr_t)sys_apc_thunk;
    }
}

void __attribute__((weak, alias("handle_ecall"))) handle_ecall_u(uintptr_t *regs);
void __attribute__((weak, alias("handle_ecall"))) handle_ecall_h(uintptr_t *regs);
void __attribute__((weak, alias("handle_ecall"))) handle_ecall_s(uintptr_t *regs);
void __attribute__((weak, alias("handle_ecall"))) handle_ecall_m(uintptr_t *regs);

void handle_syscall(uintptr_t *regs, uintptr_t cause)
{
    static void (*const cause_table[])(uintptr_t * regs) = {
        [CAUSE_USER_ECALL] = handle_ecall_u,
        [CAUSE_SUPERVISOR_ECALL] = handle_ecall_h,
        [CAUSE_HYPERVISOR_ECALL] = handle_ecall_s,
        [CAUSE_MACHINE_ECALL] = handle_ecall_m,
    };

    cause_table[cause & CAUSE_MACHINE_IRQ_REASON_MASK](regs);
}
