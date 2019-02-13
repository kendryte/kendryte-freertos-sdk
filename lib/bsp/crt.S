# Copyright 2018 Canaan Inc.
# 
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
# 
#     http://www.apache.org/licenses/LICENSE-2.0
# 
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# include <encoding.h>

# define REGBYTES 8
# define STKSHIFT 16
# define IRQ_STACK_SIZE 20480

.global g_wake_address
.global g_irq_count
.global xPortStartScheduler
.global sys_apc_thunk

.section .text.start, "ax", @progbits
.globl _start
_start:
  j 1f
  nop
  .word 0xdeadbeef
1:
  csrw mideleg, 0
  csrw medeleg, 0
  csrw mie, 0
  csrw mip, 0
  la t0, trap_entry
  csrw mtvec, t0

  li t0, 1
  sd t0, _irq_enabled, t1
  
  li x1, 0
  li x2, 0
  li x3, 0
  li x4, 0
  li x5, 0
  li x6, 0
  li x7, 0
  li x8, 0
  li x9, 0
  li x10,0
  li x11,0
  li x12,0
  li x13,0
  li x14,0
  li x15,0
  li x16,0
  li x17,0
  li x18,0
  li x19,0
  li x20,0
  li x21,0
  li x22,0
  li x23,0
  li x24,0
  li x25,0
  li x26,0
  li x27,0
  li x28,0
  li x29,0
  li x30,0
  li x31,0

  li t0, MSTATUS_FS
  csrs mstatus, t0
  
  fssr    x0
  fmv.w.x f0, x0
  fmv.w.x f1, x0
  fmv.w.x f2, x0
  fmv.w.x f3, x0
  fmv.w.x f4, x0
  fmv.w.x f5, x0
  fmv.w.x f6, x0
  fmv.w.x f7, x0
  fmv.w.x f8, x0
  fmv.w.x f9, x0
  fmv.w.x f10,x0
  fmv.w.x f11,x0
  fmv.w.x f12,x0
  fmv.w.x f13,x0
  fmv.w.x f14,x0
  fmv.w.x f15,x0
  fmv.w.x f16,x0
  fmv.w.x f17,x0
  fmv.w.x f18,x0
  fmv.w.x f19,x0
  fmv.w.x f20,x0
  fmv.w.x f21,x0
  fmv.w.x f22,x0
  fmv.w.x f23,x0
  fmv.w.x f24,x0
  fmv.w.x f25,x0
  fmv.w.x f26,x0
  fmv.w.x f27,x0
  fmv.w.x f28,x0
  fmv.w.x f29,x0
  fmv.w.x f30,x0
  fmv.w.x f31,x0

.option push
.option norelax
  la gp, __global_pointer$
.option pop

  csrr t0, mhartid
  beqz t0, 2f
  la sp, _idle_stack1_top
  la t0, g_wake_address
1:
  lr.d t1, 0(t0)
  beqz t1, 1b
  jr t1
2:
  la sp, _idle_stack0_top
  
  # clear the bss segment
  la t0, __bss_start
  la t1, __bss_end
1:
  sd zero, 0(t0)
  addi t0, t0, 8
  bltu t0, t1, 1b

  la t0, _init_bsp
  jr t0
  
  # t0 = mhartid
  # t1 = mhartid * 8
  # t2 = _irq_count + t1
  # t3 = _irq_count[t1]
.macro get_irq_count
  csrr t0, mhartid
  slli t1, t0, 3
  la t2, g_irq_count
  add t2, t2, t1
  ld t3, 0(t2)
.endm

  .globl trap_entry
  .type trap_entry, @function
  .align 2
trap_entry:
  csrw mscratch, t0
  ld t0, _irq_enabled
  bnez t0, 1f
  # Disable interrupt
  li t0, MSTATUS_MPIE
  csrc mstatus, t0
  csrr t0, mscratch
  mret
1:
  csrr t0, mscratch
  # Save context
  addi sp, sp, -NUM_XCEPT_REGS * REGBYTES
  
  sd x1,   1 * REGBYTES(sp)
  sd x2,   2 * REGBYTES(sp)
  sd x4,   4 * REGBYTES(sp)
  sd x5,   5 * REGBYTES(sp)
  sd x6,   6 * REGBYTES(sp)
  sd x7,   7 * REGBYTES(sp)
  sd x8,   8 * REGBYTES(sp)
  sd x9,   9 * REGBYTES(sp)
  sd x10, 10 * REGBYTES(sp)
  sd x11, 11 * REGBYTES(sp)
  sd x12, 12 * REGBYTES(sp)
  sd x13, 13 * REGBYTES(sp)
  sd x14, 14 * REGBYTES(sp)
  sd x15, 15 * REGBYTES(sp)
  sd x16, 16 * REGBYTES(sp)
  sd x17, 17 * REGBYTES(sp)
  sd x18, 18 * REGBYTES(sp)
  sd x19, 19 * REGBYTES(sp)
  sd x20, 20 * REGBYTES(sp)
  sd x21, 21 * REGBYTES(sp)
  sd x22, 22 * REGBYTES(sp)
  sd x23, 23 * REGBYTES(sp)
  sd x24, 24 * REGBYTES(sp)
  sd x25, 25 * REGBYTES(sp)
  sd x26, 26 * REGBYTES(sp)
  sd x27, 27 * REGBYTES(sp)
  sd x28, 28 * REGBYTES(sp)
  sd x29, 29 * REGBYTES(sp)
  sd x30, 30 * REGBYTES(sp)
  sd x31, 31 * REGBYTES(sp)

  fsw f0,  ( 0 + 32) * REGBYTES(sp)
  fsw f1,  ( 1 + 32) * REGBYTES(sp)
  fsw f2,  ( 2 + 32) * REGBYTES(sp)
  fsw f3,  ( 3 + 32) * REGBYTES(sp)
  fsw f4,  ( 4 + 32) * REGBYTES(sp)
  fsw f5,  ( 5 + 32) * REGBYTES(sp)
  fsw f6,  ( 6 + 32) * REGBYTES(sp)
  fsw f7,  ( 7 + 32) * REGBYTES(sp)
  fsw f8,  ( 8 + 32) * REGBYTES(sp)
  fsw f9,  ( 9 + 32) * REGBYTES(sp)
  fsw f10, (10 + 32) * REGBYTES(sp)
  fsw f11, (11 + 32) * REGBYTES(sp)
  fsw f12, (12 + 32) * REGBYTES(sp)
  fsw f13, (13 + 32) * REGBYTES(sp)
  fsw f14, (14 + 32) * REGBYTES(sp)
  fsw f15, (15 + 32) * REGBYTES(sp)
  fsw f16, (16 + 32) * REGBYTES(sp)
  fsw f17, (17 + 32) * REGBYTES(sp)
  fsw f18, (18 + 32) * REGBYTES(sp)
  fsw f19, (19 + 32) * REGBYTES(sp)
  fsw f20, (20 + 32) * REGBYTES(sp)
  fsw f21, (21 + 32) * REGBYTES(sp)
  fsw f22, (22 + 32) * REGBYTES(sp)
  fsw f23, (23 + 32) * REGBYTES(sp)
  fsw f24, (24 + 32) * REGBYTES(sp)
  fsw f25, (25 + 32) * REGBYTES(sp)
  fsw f26, (26 + 32) * REGBYTES(sp)
  fsw f27, (27 + 32) * REGBYTES(sp)
  fsw f28, (28 + 32) * REGBYTES(sp)
  fsw f29, (29 + 32) * REGBYTES(sp)
  fsw f30, (30 + 32) * REGBYTES(sp)
  fsw f31, (31 + 32) * REGBYTES(sp)

  # Store mepc
  csrr t0, mepc
  sd t0, 0 * REGBYTES(sp)

  # a0 = regs
  mv a0, sp
  # a1 = mcause
  csrr a1, mcause
  
  get_irq_count
  # _irq_count++
  addi t4, t3, 1
  sd t4, 0(t2)
  # Test if already in trap handler
  bnez t3, 3f
  # If not in trap handler, store sp
  la t2, pxCurrentTCB
  add t2, t2, t1
  ld t2, 0(t2)
  beqz t2, 1f
  sd sp, 0(t2)
1:
  # Then set sp to irq stack top
  beqz t0, 2f
  la sp, _irq_stack1_top
  j 3f
2:
  la sp, _irq_stack0_top
3:
  la t0, handle_irq
  jalr t0

  # Disable interrupt
  li t0, MSTATUS_MIE
  csrc mstatus, t0
  
  get_irq_count
  # _irq_count--
  addi t4, t3, -1
  sd t4, 0(t2)

  # If restore to task, use pxCurrentTCB instead of a0
  bnez t4, 1f
.restore:
  la t2, pxCurrentTCB
  add t2, t2, t1
  ld t2, 0(t2)
  beqz t2, 1f
  ld a0, 0(t2)

  # Restore context
  li t0, MSTATUS_MPP | MSTATUS_MPIE
  csrs mstatus, t0
1:
  # a0 = regs

  # Restore mepc
  ld t0, 0 * REGBYTES(a0)
  csrw mepc, t0
  
  # Store a0 to mscratch
  ld t0, 10 * REGBYTES(a0)
  csrw mscratch, t0
  
  ld x1,   1 * REGBYTES(a0)
  ld x2,   2 * REGBYTES(a0)
  ld x4,   4 * REGBYTES(a0)
  ld x5,   5 * REGBYTES(a0)
  ld x6,   6 * REGBYTES(a0)
  ld x7,   7 * REGBYTES(a0)
  ld x8,   8 * REGBYTES(a0)
  ld x9,   9 * REGBYTES(a0)
  ld x11, 11 * REGBYTES(a0)
  ld x12, 12 * REGBYTES(a0)
  ld x13, 13 * REGBYTES(a0)
  ld x14, 14 * REGBYTES(a0)
  ld x15, 15 * REGBYTES(a0)
  ld x16, 16 * REGBYTES(a0)
  ld x17, 17 * REGBYTES(a0)
  ld x18, 18 * REGBYTES(a0)
  ld x19, 19 * REGBYTES(a0)
  ld x20, 20 * REGBYTES(a0)
  ld x21, 21 * REGBYTES(a0)
  ld x22, 22 * REGBYTES(a0)
  ld x23, 23 * REGBYTES(a0)
  ld x24, 24 * REGBYTES(a0)
  ld x25, 25 * REGBYTES(a0)
  ld x26, 26 * REGBYTES(a0)
  ld x27, 27 * REGBYTES(a0)
  ld x28, 28 * REGBYTES(a0)
  ld x29, 29 * REGBYTES(a0)
  ld x30, 30 * REGBYTES(a0)
  ld x31, 31 * REGBYTES(a0)

  flw f0,  ( 0 + 32) * REGBYTES(a0)
  flw f1,  ( 1 + 32) * REGBYTES(a0)
  flw f2,  ( 2 + 32) * REGBYTES(a0)
  flw f3,  ( 3 + 32) * REGBYTES(a0)
  flw f4,  ( 4 + 32) * REGBYTES(a0)
  flw f5,  ( 5 + 32) * REGBYTES(a0)
  flw f6,  ( 6 + 32) * REGBYTES(a0)
  flw f7,  ( 7 + 32) * REGBYTES(a0)
  flw f8,  ( 8 + 32) * REGBYTES(a0)
  flw f9,  ( 9 + 32) * REGBYTES(a0)
  flw f10, (10 + 32) * REGBYTES(a0)
  flw f11, (11 + 32) * REGBYTES(a0)
  flw f12, (12 + 32) * REGBYTES(a0)
  flw f13, (13 + 32) * REGBYTES(a0)
  flw f14, (14 + 32) * REGBYTES(a0)
  flw f15, (15 + 32) * REGBYTES(a0)
  flw f16, (16 + 32) * REGBYTES(a0)
  flw f17, (17 + 32) * REGBYTES(a0)
  flw f18, (18 + 32) * REGBYTES(a0)
  flw f19, (19 + 32) * REGBYTES(a0)
  flw f20, (20 + 32) * REGBYTES(a0)
  flw f21, (21 + 32) * REGBYTES(a0)
  flw f22, (22 + 32) * REGBYTES(a0)
  flw f23, (23 + 32) * REGBYTES(a0)
  flw f24, (24 + 32) * REGBYTES(a0)
  flw f25, (25 + 32) * REGBYTES(a0)
  flw f26, (26 + 32) * REGBYTES(a0)
  flw f27, (27 + 32) * REGBYTES(a0)
  flw f28, (28 + 32) * REGBYTES(a0)
  flw f29, (29 + 32) * REGBYTES(a0)
  flw f30, (30 + 32) * REGBYTES(a0)
  flw f31, (31 + 32) * REGBYTES(a0)

  # Restore a0
  addi sp, sp, NUM_XCEPT_REGS * REGBYTES
  csrr a0, mscratch
  mret

sys_apc_thunk:
  addi sp, sp, -NUM_XCEPT_REGS * REGBYTES
  ld t0, REG_APC_PROC * REGBYTES(sp)
  sd a7, REG_APC_PROC * REGBYTES(sp)
  jalr t0
  
  ld x1,   1 * REGBYTES(sp)
  ld x4,   4 * REGBYTES(sp)
  ld x5,   5 * REGBYTES(sp)
  ld x6,   6 * REGBYTES(sp)
  ld x7,   7 * REGBYTES(sp)
  ld x8,   8 * REGBYTES(sp)
  ld x9,   9 * REGBYTES(sp)
  ld x11, 11 * REGBYTES(sp)
  ld x12, 12 * REGBYTES(sp)
  ld x13, 13 * REGBYTES(sp)
  ld x14, 14 * REGBYTES(sp)
  ld x15, 15 * REGBYTES(sp)
  ld x16, 16 * REGBYTES(sp)
  ld x17, 17 * REGBYTES(sp)
  ld x18, 18 * REGBYTES(sp)
  ld x19, 19 * REGBYTES(sp)
  ld x20, 20 * REGBYTES(sp)
  ld x21, 21 * REGBYTES(sp)
  ld x22, 22 * REGBYTES(sp)
  ld x23, 23 * REGBYTES(sp)
  ld x24, 24 * REGBYTES(sp)
  ld x25, 25 * REGBYTES(sp)
  ld x26, 26 * REGBYTES(sp)
  ld x27, 27 * REGBYTES(sp)
  ld x28, 28 * REGBYTES(sp)
  ld x29, 29 * REGBYTES(sp)
  ld x30, 30 * REGBYTES(sp)
  ld x31, 31 * REGBYTES(sp)

  flw f0,  ( 0 + 32) * REGBYTES(sp)
  flw f1,  ( 1 + 32) * REGBYTES(sp)
  flw f2,  ( 2 + 32) * REGBYTES(sp)
  flw f3,  ( 3 + 32) * REGBYTES(sp)
  flw f4,  ( 4 + 32) * REGBYTES(sp)
  flw f5,  ( 5 + 32) * REGBYTES(sp)
  flw f6,  ( 6 + 32) * REGBYTES(sp)
  flw f7,  ( 7 + 32) * REGBYTES(sp)
  flw f8,  ( 8 + 32) * REGBYTES(sp)
  flw f9,  ( 9 + 32) * REGBYTES(sp)
  flw f10, (10 + 32) * REGBYTES(sp)
  flw f11, (11 + 32) * REGBYTES(sp)
  flw f12, (12 + 32) * REGBYTES(sp)
  flw f13, (13 + 32) * REGBYTES(sp)
  flw f14, (14 + 32) * REGBYTES(sp)
  flw f15, (15 + 32) * REGBYTES(sp)
  flw f16, (16 + 32) * REGBYTES(sp)
  flw f17, (17 + 32) * REGBYTES(sp)
  flw f18, (18 + 32) * REGBYTES(sp)
  flw f19, (19 + 32) * REGBYTES(sp)
  flw f20, (20 + 32) * REGBYTES(sp)
  flw f21, (21 + 32) * REGBYTES(sp)
  flw f22, (22 + 32) * REGBYTES(sp)
  flw f23, (23 + 32) * REGBYTES(sp)
  flw f24, (24 + 32) * REGBYTES(sp)
  flw f25, (25 + 32) * REGBYTES(sp)
  flw f26, (26 + 32) * REGBYTES(sp)
  flw f27, (27 + 32) * REGBYTES(sp)
  flw f28, (28 + 32) * REGBYTES(sp)
  flw f29, (29 + 32) * REGBYTES(sp)
  flw f30, (30 + 32) * REGBYTES(sp)
  flw f31, (31 + 32) * REGBYTES(sp)

  addi sp, sp, NUM_XCEPT_REGS * REGBYTES
  li a7, SYS_apc_return
  ecall
  j .
  
.global _init
.type   _init, @function
.global _fini
.type   _fini, @function
_init:
_fini:
  ret
  .size  _init, .-_init
  .size  _fini, .-_fini

xPortStartScheduler:
  la t0, vPortSetupTimer
  jalr t0
  csrr t0, mhartid
  slli t1, t0, 3
  j .restore

  .align 3
g_irq_count:
  .8byte 0
  .8byte 0
_irq_enabled:
  .8byte 0
g_wake_address:
  .8byte 0

  .section .bss
  .align 3
_irq_stack_base:
  .space IRQ_STACK_SIZE
_irq_stack0_top:
  .space IRQ_STACK_SIZE
_irq_stack1_top:
  .space IRQ_STACK_SIZE
_idle_stack0_top:
  .space IRQ_STACK_SIZE
_idle_stack1_top: