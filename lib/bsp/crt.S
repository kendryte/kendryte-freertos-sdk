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
# include "encoding.h"

# define LREG ld
# define SREG sd
# define LFREG fld
# define SFREG fsd
# define REGBYTES 8
# define STKSHIFT 15

.section .text.start, "ax", @progbits
.globl _start
_start:
  j 1f
  .word 0xdeadbeef
1:
  csrw mideleg, 0
  csrw medeleg, 0
  csrw mie, 0
  csrw mip, 0
  la t0, trap_entry
  csrw mtvec, t0
  
  li  x1, 0
  li  x2, 0
  li  x3, 0
  li  x4, 0
  li  x5, 0
  li  x6, 0
  li  x7, 0
  li  x8, 0
  li  x9, 0
  li  x10,0
  li  x11,0
  li  x12,0
  li  x13,0
  li  x14,0
  li  x15,0
  li  x16,0
  li  x17,0
  li  x18,0
  li  x19,0
  li  x20,0
  li  x21,0
  li  x22,0
  li  x23,0
  li  x24,0
  li  x25,0
  li  x26,0
  li  x27,0
  li  x28,0
  li  x29,0
  li  x30,0
  li  x31,0

  li t0, MSTATUS_FS
  csrs mstatus, t0
  
  fssr    x0
  fmv.d.x f0, x0
  fmv.d.x f1, x0
  fmv.d.x f2, x0
  fmv.d.x f3, x0
  fmv.d.x f4, x0
  fmv.d.x f5, x0
  fmv.d.x f6, x0
  fmv.d.x f7, x0
  fmv.d.x f8, x0
  fmv.d.x f9, x0
  fmv.d.x f10,x0
  fmv.d.x f11,x0
  fmv.d.x f12,x0
  fmv.d.x f13,x0
  fmv.d.x f14,x0
  fmv.d.x f15,x0
  fmv.d.x f16,x0
  fmv.d.x f17,x0
  fmv.d.x f18,x0
  fmv.d.x f19,x0
  fmv.d.x f20,x0
  fmv.d.x f21,x0
  fmv.d.x f22,x0
  fmv.d.x f23,x0
  fmv.d.x f24,x0
  fmv.d.x f25,x0
  fmv.d.x f26,x0
  fmv.d.x f27,x0
  fmv.d.x f28,x0
  fmv.d.x f29,x0
  fmv.d.x f30,x0
  fmv.d.x f31,x0

.option push
.option norelax
  la gp, __global_pointer$
.option pop
  la  tp, _end + 63
  and tp, tp, -64
  csrr a0, mhartid

  sll a2, a0, STKSHIFT
  add tp, tp, a2
  add sp, a0, 1
  sll sp, sp, STKSHIFT
  add sp, sp, tp

  j _init_bsp

  .globl trap_entry
  .type trap_entry, @function
  .align 2
trap_entry:
  addi sp, sp, -REGBYTES
  SREG t0, 0x0(sp)
  csrr t0, mcause
  bgez t0, .handle_syscall
  # Test soft interrupt
  slli t0, t0, 1
  addi t0, t0, -(IRQ_M_SOFT << 1)
  bnez t0, .handle_irq
  # Interupt is soft interrupt
  # Get event
  addi sp, sp, -REGBYTES
  SREG t1, 0x0(sp)
  la   t0, g_core_sync_events
  csrr t1, mhartid
  slli t1, t1, 3
  add  t0, t0, t1
  LREG t1, 0x0(sp)
  addi sp, sp, REGBYTES
  # Test ContextSwitch event
  ld   t0, 0x0(t0)
  addi t0, t0, -2 # CORE_SYNC_CONTEXT_SWITCH
  bnez t0, .handle_irq

  LREG t0, 0x0(sp)
  addi sp, sp, REGBYTES
  # Do not use jal here
  j    xPortSysTickInt
  mret
.handle_irq:
  LREG t0, 0x0(sp)
  addi sp, sp, REGBYTES
  addi sp, sp, -36*REGBYTES

  SREG ra, 0*REGBYTES(sp)
  SREG t0, 1*REGBYTES(sp)
  SREG t1, 2*REGBYTES(sp)
  SREG t2, 3*REGBYTES(sp)
  SREG a0, 4*REGBYTES(sp)
  SREG a1, 5*REGBYTES(sp)
  SREG a2, 6*REGBYTES(sp)
  SREG a3, 7*REGBYTES(sp)
  SREG a4, 8*REGBYTES(sp)
  SREG a5, 9*REGBYTES(sp)
  SREG a6, 10*REGBYTES(sp)
  SREG a7, 11*REGBYTES(sp)
  SREG t3, 12*REGBYTES(sp)
  SREG t4, 13*REGBYTES(sp)
  SREG t5, 14*REGBYTES(sp)
  SREG t6, 15*REGBYTES(sp)

  SFREG ft0, ( 0  + 16)*REGBYTES(sp)
  SFREG ft1, ( 1  + 16)*REGBYTES(sp)
  SFREG ft2, ( 2  + 16)*REGBYTES(sp)
  SFREG ft3, ( 3  + 16)*REGBYTES(sp)
  SFREG ft4, ( 4  + 16)*REGBYTES(sp)
  SFREG ft5, ( 5  + 16)*REGBYTES(sp)
  SFREG ft6, ( 6  + 16)*REGBYTES(sp)
  SFREG ft7, ( 7  + 16)*REGBYTES(sp)
  SFREG fa0, ( 8  + 16)*REGBYTES(sp)
  SFREG fa1, ( 9  + 16)*REGBYTES(sp)
  SFREG fa2, ( 10 + 16)*REGBYTES(sp)
  SFREG fa3, ( 11 + 16)*REGBYTES(sp)
  SFREG fa4, ( 12 + 16)*REGBYTES(sp)
  SFREG fa5, ( 13 + 16)*REGBYTES(sp)
  SFREG fa6, ( 14 + 16)*REGBYTES(sp)
  SFREG fa7, ( 15 + 16)*REGBYTES(sp)
  SFREG ft8, ( 16 + 16)*REGBYTES(sp)
  SFREG ft9, ( 17 + 16)*REGBYTES(sp)
  SFREG ft10,( 18 + 16)*REGBYTES(sp)
  SFREG ft11,( 19 + 16)*REGBYTES(sp)

  csrr a0, mcause
  csrr a1, mepc
  jal handle_irq

  LREG ra, 0*REGBYTES(sp)
  LREG t0, 1*REGBYTES(sp)
  LREG t1, 2*REGBYTES(sp)
  LREG t2, 3*REGBYTES(sp)
  LREG a0, 4*REGBYTES(sp)
  LREG a1, 5*REGBYTES(sp)
  LREG a2, 6*REGBYTES(sp)
  LREG a3, 7*REGBYTES(sp)
  LREG a4, 8*REGBYTES(sp)
  LREG a5, 9*REGBYTES(sp)
  LREG a6, 10*REGBYTES(sp)
  LREG a7, 11*REGBYTES(sp)
  LREG t3, 12*REGBYTES(sp)
  LREG t4, 13*REGBYTES(sp)
  LREG t5, 14*REGBYTES(sp)
  LREG t6, 15*REGBYTES(sp)

  LFREG ft0, ( 0  + 16)*REGBYTES(sp)
  LFREG ft1, ( 1  + 16)*REGBYTES(sp)
  LFREG ft2, ( 2  + 16)*REGBYTES(sp)
  LFREG ft3, ( 3  + 16)*REGBYTES(sp)
  LFREG ft4, ( 4  + 16)*REGBYTES(sp)
  LFREG ft5, ( 5  + 16)*REGBYTES(sp)
  LFREG ft6, ( 6  + 16)*REGBYTES(sp)
  LFREG ft7, ( 7  + 16)*REGBYTES(sp)
  LFREG fa0, ( 8  + 16)*REGBYTES(sp)
  LFREG fa1, ( 9  + 16)*REGBYTES(sp)
  LFREG fa2, ( 10 + 16)*REGBYTES(sp)
  LFREG fa3, ( 11 + 16)*REGBYTES(sp)
  LFREG fa4, ( 12 + 16)*REGBYTES(sp)
  LFREG fa5, ( 13 + 16)*REGBYTES(sp)
  LFREG fa6, ( 14 + 16)*REGBYTES(sp)
  LFREG fa7, ( 15 + 16)*REGBYTES(sp)
  LFREG ft8, ( 16 + 16)*REGBYTES(sp)
  LFREG ft9, ( 17 + 16)*REGBYTES(sp)
  LFREG ft10,( 18 + 16)*REGBYTES(sp)
  LFREG ft11,( 19 + 16)*REGBYTES(sp)

  addi sp, sp, 36*REGBYTES
  mret
.handle_syscall:
  LREG t0, 0x0(sp)
  addi sp, sp, REGBYTES
  addi sp, sp, -36*REGBYTES

  SREG ra, 0*REGBYTES(sp)
  SREG t0, 1*REGBYTES(sp)
  SREG t1, 2*REGBYTES(sp)
  SREG t2, 3*REGBYTES(sp)
  SREG a0, 4*REGBYTES(sp)
  SREG a1, 5*REGBYTES(sp)
  SREG a2, 6*REGBYTES(sp)
  SREG a3, 7*REGBYTES(sp)
  SREG a4, 8*REGBYTES(sp)
  SREG a5, 9*REGBYTES(sp)
  SREG a6, 10*REGBYTES(sp)
  SREG a7, 11*REGBYTES(sp)
  SREG t3, 12*REGBYTES(sp)
  SREG t4, 13*REGBYTES(sp)
  SREG t5, 14*REGBYTES(sp)
  SREG t6, 15*REGBYTES(sp)

  csrr a0, mcause
  csrr a1, mepc
  mv a2, sp

  jal handle_syscall
  # Remain in M-mode after eret
  li t0, MSTATUS_MPP
  csrs mstatus, t0
  csrw mepc, a0
  
  LREG a0, 4*REGBYTES(sp)

  addi sp, sp, 36*REGBYTES
  mret
  
.global _init
.type   _init, @function
.global _fini
.type   _fini, @function
_init:
_fini:
  ret
  .size  _init, .-_init
  .size  _fini, .-_fini

.section ".tdata.begin"
.globl _tdata_begin
_tdata_begin:

.section ".tdata.end"
.globl _tdata_end
_tdata_end:

.section ".tbss.end"
.globl _tbss_end
_tbss_end: