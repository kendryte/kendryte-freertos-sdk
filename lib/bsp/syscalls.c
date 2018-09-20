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
#include <machine/syscall.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/unistd.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include "../freertos/device/devices.h"
#include "atomic.h"
#include "clint.h"
#include "dump.h"
#include "fpioa.h"
#include "interrupt.h"
#include "sysctl.h"
#include "uarths.h"

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

#define SYS_RET(epc_val, err_val) \
syscall_ret_t ret =               \
{                                 \
    .err = err_val,               \
    .epc = epc_val                \
};                                \
return ret;

typedef struct _syscall_ret
{
    int err;
    uintptr_t epc;
} syscall_ret_t;

static const char* TAG = "SYSCALL";

extern char _heap_start[];
extern char _heap_end[];
char* _heap_cur = &_heap_start[0];

void __attribute__((noreturn)) sys_exit(int code)
{
    /* First print some diagnostic information. */
    LOGW(TAG, "sys_exit called with 0x%lx\n", (uint64_t)code);
    /* Write exit register to pause netlist simulation */
    volatile uint32_t* reg = (uint32_t*)&sysctl->peri;
    /* Write stop bit and write back */
    *reg = (*reg) | 0x80000000UL;
    /* Send 0 to uart */
    uart_putchar(0);
    while (1)
        continue;
}

static int sys_nosys(long a0, long a1, long a2, long a3, long a4, long a5, unsigned long n)
{
    UNUSED(a3);
    UNUSED(a4);
    UNUSED(a5);

    LOGE(TAG, "Unsupported syscall %ld: a0=%lx, a1=%lx, a2=%lx!\n", n, a0, a1, a2);
    /* Send 0 to uart */
    uart_putchar(0);
    while (1)
        continue;
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
            /* Memory out, return -ENOMEM */
            LOGE(TAG, "Out of memory\n");
            res = -ENOMEM;
        }
        else
        {
            /* Adjust brk pointer. */
            _heap_cur = (char*)(uintptr_t)pos;
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

static ssize_t sys_write(int file, const void* ptr, size_t len)
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
    register size_t length = len;
    /* Get data pointer */
    register char* data = (char*)ptr;

    if (STDOUT_FILENO == file || STDERR_FILENO == file)
    {
        /* Write data */
        while (length-- > 0 && *data != 0)
            uart_putchar(*(data++));

        /* Return the actual size written */
        res = len;
    }
    else
    {
        /* Not support yet */
        res = io_write(file, data, length);
    }

    return res;
}

static int sys_open(const char* name, int flags, int mode)
{
    UNUSED(flags);
    UNUSED(mode);

    uintptr_t ptr = io_open(name);
    return ptr;
}

static int sys_fstat(int file, struct stat* st)
{
    int res = -EBADF;

    /*
     * Status of an open file. The sys/stat.h header file required
     * is
     * distributed in the include subdirectory for this C library.
     *
     * int fstat(int file, struct stat* st)
     *
     * IN : regs[10] = file, regs[11] = st
     * OUT: regs[10] = Upon successful completion, 0 shall be
     * returned.
     * Otherwise, -1 shall be returned and errno set to indicate
     * the error.
     */

    UNUSED(file);

    if (st != NULL)
        memset(st, 0, sizeof(struct stat));
    /* Return the result */
    res = -ENOSYS;
    /*
     * Note: This value will return to syscall wrapper, syscall
     * wrapper will set errno to ENOSYS and return -1
     */

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
    return io_close(file);
}

static int sys_gettimeofday(struct timeval* tp, void* tzp)
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

static syscall_ret_t handle_ecall(uintptr_t a0, uintptr_t a1, uintptr_t a2, uintptr_t a3, uintptr_t a4, uintptr_t a5, uintptr_t epc, uintptr_t n)
{
    enum syscall_id_e
    {
        SYS_ID_NOSYS,
        SYS_ID_SUCCESS,
        SYS_ID_EXIT,
        SYS_ID_BRK,
        SYS_ID_WRITE,
        SYS_ID_OPEN,
        SYS_ID_FSTAT,
        SYS_ID_CLOSE,
        SYS_ID_GETTIMEOFDAY,
        SYS_ID_MAX
    };

    static uintptr_t(*const syscall_table[])(long a0, long a1, long a2, long a3, long a4, long a5, unsigned long n) = {
        [SYS_ID_NOSYS] = (void*)sys_nosys,
        [SYS_ID_SUCCESS] = (void*)sys_success,
        [SYS_ID_EXIT] = (void*)sys_exit,
        [SYS_ID_BRK] = (void*)sys_brk,
        [SYS_ID_WRITE] = (void*)sys_write,
        [SYS_ID_OPEN] = (void*)sys_open,
        [SYS_ID_FSTAT] = (void*)sys_fstat,
        [SYS_ID_CLOSE] = (void*)sys_close,
        [SYS_ID_GETTIMEOFDAY] = (void*)sys_gettimeofday,
    };

#if defined(__GNUC__)
#pragma GCC diagnostic ignored "-Woverride-init"
#endif
    static const uint8_t syscall_id_table[0x100] = {
        [0x00 ... 0xFF] = SYS_ID_NOSYS,
        [0xFF & SYS_exit] = SYS_ID_EXIT,
        [0xFF & SYS_exit_group] = SYS_ID_EXIT,
        [0xFF & SYS_getpid] = SYS_ID_NOSYS,
        [0xFF & SYS_kill] = SYS_ID_NOSYS,
        [0xFF & SYS_read] = SYS_ID_NOSYS,
        [0xFF & SYS_write] = SYS_ID_WRITE,
        [0xFF & SYS_open] = SYS_ID_OPEN,
        [0xFF & SYS_openat] = SYS_ID_NOSYS,
        [0xFF & SYS_close] = SYS_ID_CLOSE,
        [0xFF & SYS_lseek] = SYS_ID_NOSYS,
        [0xFF & SYS_brk] = SYS_ID_BRK,
        [0xFF & SYS_link] = SYS_ID_NOSYS,
        [0xFF & SYS_unlink] = SYS_ID_NOSYS,
        [0xFF & SYS_mkdir] = SYS_ID_NOSYS,
        [0xFF & SYS_chdir] = SYS_ID_NOSYS,
        [0xFF & SYS_getcwd] = SYS_ID_NOSYS,
        [0xFF & SYS_stat] = SYS_ID_NOSYS,
        [0xFF & SYS_fstat] = SYS_ID_FSTAT,
        [0xFF & SYS_lstat] = SYS_ID_NOSYS,
        [0xFF & SYS_fstatat] = SYS_ID_NOSYS,
        [0xFF & SYS_access] = SYS_ID_NOSYS,
        [0xFF & SYS_faccessat] = SYS_ID_NOSYS,
        [0xFF & SYS_pread] = SYS_ID_NOSYS,
        [0xFF & SYS_pwrite] = SYS_ID_NOSYS,
        [0xFF & SYS_uname] = SYS_ID_NOSYS,
        [0xFF & SYS_getuid] = SYS_ID_NOSYS,
        [0xFF & SYS_geteuid] = SYS_ID_NOSYS,
        [0xFF & SYS_getgid] = SYS_ID_NOSYS,
        [0xFF & SYS_getegid] = SYS_ID_NOSYS,
        [0xFF & SYS_mmap] = SYS_ID_NOSYS,
        [0xFF & SYS_munmap] = SYS_ID_NOSYS,
        [0xFF & SYS_mremap] = SYS_ID_NOSYS,
        [0xFF & SYS_time] = SYS_ID_NOSYS,
        [0xFF & SYS_getmainvars] = SYS_ID_NOSYS,
        [0xFF & SYS_rt_sigaction] = SYS_ID_NOSYS,
        [0xFF & SYS_writev] = SYS_ID_NOSYS,
        [0xFF & SYS_gettimeofday] = SYS_ID_GETTIMEOFDAY,
        [0xFF & SYS_times] = SYS_ID_NOSYS,
        [0xFF & SYS_fcntl] = SYS_ID_NOSYS,
        [0xFF & SYS_getdents] = SYS_ID_NOSYS,
        [0xFF & SYS_dup] = SYS_ID_NOSYS,
    };
#if defined(__GNUC__)
#pragma GCC diagnostic warning "-Woverride-init"
#endif

    int err = syscall_table[syscall_id_table[0xFF & n]](
        a0, /* a0 */
        a1, /* a1 */
        a2, /* a2 */
        a3, /* a3 */
        a4, /* a4 */
        a5, /* a5 */
        n /* n */
        );

    epc += ((*(unsigned short*)epc & 3) == 3 ? 4 : 2);
    SYS_RET(epc, err);
}

syscall_ret_t __attribute__((weak, alias("handle_ecall")))
handle_ecall_u(uintptr_t a0, uintptr_t a1, uintptr_t a2, uintptr_t a3, uintptr_t a4, uintptr_t a5, uintptr_t epc, uintptr_t n);

syscall_ret_t __attribute__((weak, alias("handle_ecall")))
handle_ecall_h(uintptr_t a0, uintptr_t a1, uintptr_t a2, uintptr_t a3, uintptr_t a4, uintptr_t a5, uintptr_t epc, uintptr_t n);

syscall_ret_t __attribute__((weak, alias("handle_ecall")))
handle_ecall_s(uintptr_t a0, uintptr_t a1, uintptr_t a2, uintptr_t a3, uintptr_t a4, uintptr_t a5, uintptr_t epc, uintptr_t n);

syscall_ret_t __attribute__((weak, alias("handle_ecall")))
handle_ecall_m(uintptr_t a0, uintptr_t a1, uintptr_t a2, uintptr_t a3, uintptr_t a4, uintptr_t a5, uintptr_t epc, uintptr_t n);

syscall_ret_t handle_syscall(uintptr_t a0, uintptr_t a1, uintptr_t a2, uintptr_t a3, uintptr_t a4, uintptr_t a5, uintptr_t epc, uintptr_t n)
{
    static syscall_ret_t(*const cause_table[])(uintptr_t a0, uintptr_t a1, uintptr_t a2, uintptr_t a3, uintptr_t a4, uintptr_t a5, uintptr_t epc, uintptr_t n) = {
        [CAUSE_USER_ECALL] = handle_ecall_u,
        [CAUSE_SUPERVISOR_ECALL] = handle_ecall_h,
        [CAUSE_HYPERVISOR_ECALL] = handle_ecall_s,
        [CAUSE_MACHINE_ECALL] = handle_ecall_m,
    };

    return cause_table[read_csr(mcause)](a0, a1, a2, a3, a4, a5, epc, n);
}
