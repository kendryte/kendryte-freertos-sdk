# /*  Copyright 2018 Canaan Inc.
#    
#     Licensed under the Apache License, Version 2.0 (the "License");
#     you may not use this file except in compliance with the License.
#     You may obtain a copy of the License at
#    
#         http://www.apache.org/licenses/LICENSE-2.0
#    
#     Unless required by applicable law or agreed to in writing, software
#     distributed under the License is distributed on an "AS IS" BASIS,
#     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#     See the License for the specific language governing permissions and
#     limitations under the License.
#
#     FreeRTOS V9.0.0 - Copyright (C) 2016 Real Time Engineers Ltd.
#     All rights reserved

#     VISIT http://www.FreeRTOS.org TO ENSURE YOU ARE USING THE LATEST VERSION.

#     This file is part of the FreeRTOS distribution and was contributed
#     to the project by Technolution B.V. (www.technolution.nl,
#     freertos-riscv@technolution.eu) under the terms of the FreeRTOS
#     contributors license.

#     FreeRTOS is free software; you can redistribute it and/or modify it under
#     the terms of the GNU General Public License (version 2) as published by the
#     Free Software Foundation >>>> AND MODIFIED BY <<<< the FreeRTOS exception.

#     ***************************************************************************
#     >>!   NOTE: The modification to the GPL is included to allow you to     !<<
#     >>!   distribute a combined work that includes FreeRTOS without being   !<<
#     >>!   obliged to provide the source code for proprietary components     !<<
#     >>!   outside of the FreeRTOS kernel.                                   !<<
#     ***************************************************************************

#     FreeRTOS is distributed in the hope that it will be useful, but WITHOUT ANY
#     WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
#     FOR A PARTICULAR PURPOSE.  Full license text is available on the following
#     link: http://www.freertos.org/a00114.html

#     ***************************************************************************
#      *                                                                       *
#      *    FreeRTOS provides completely free yet professionally developed,    *
#      *    robust, strictly quality controlled, supported, and cross          *
#      *    platform software that is more than just the market leader, it     *
#      *    is the industry's de facto standard.                               *
#      *                                                                       *
#      *    Help yourself get started quickly while simultaneously helping     *
#      *    to support the FreeRTOS project by purchasing a FreeRTOS           *
#      *    tutorial book, reference manual, or both:                          *
#      *    http://www.FreeRTOS.org/Documentation                              *
#      *                                                                       *
#     ***************************************************************************

#     http://www.FreeRTOS.org/FAQHelp.html - Having a problem?  Start by reading
#     the FAQ page "My application does not run, what could be wrong?".  Have you
#     defined configASSERT()?

#     http://www.FreeRTOS.org/support - In return for receiving this top quality
#     embedded software for free we request you assist our global community by
#     participating in the support forum.

#     http://www.FreeRTOS.org/training - Investing in training allows your team to
#     be as productive as possible as early as possible.  Now you can receive
#     FreeRTOS training directly from Richard Barry, CEO of Real Time Engineers
#     Ltd, and the world's leading authority on the world's leading RTOS.

#     http://www.FreeRTOS.org/plus - A selection of FreeRTOS ecosystem products,
#     including FreeRTOS+Trace - an indispensable productivity tool, a DOS
#     compatible FAT file system, and our tiny thread aware UDP/IP stack.

#     http://www.FreeRTOS.org/labs - Where new FreeRTOS products go to incubate.
#     Come and try FreeRTOS+TCP, our new open source TCP/IP stack for FreeRTOS.

#     http://www.OpenRTOS.com - Real Time Engineers ltd. license FreeRTOS to High
#     Integrity Systems ltd. to sell under the OpenRTOS brand.  Low cost OpenRTOS
#     licenses offer ticketed support, indemnification and commercial middleware.

#     http://www.SafeRTOS.com - High Integrity Systems also provide a safety
#     engineered and independently SIL3 certified version for use in safety and
#     mission critical applications that require provable dependability.

#     1 tab == 4 spaces!
# */

#include "encoding.h"

# define STORE    sd
# define LOAD     ld
# define REGBYTES 8

.global portSAVE_CONTEXT
.global portRESTORE_CONTEXT
.global xPortSysTickInt
.global xPortStartScheduler
.global vPortYield
.global vPortYieldFromISR
.global vTaskIncrementTick
.global vPortEndScheduler
.global xExitStack
.global uxPortGetProcessorId


# /* Macro for saving task context */
.macro portSAVE_CONTEXT
	.global pxCurrentTCB
	# /* make room in stack */
	addi    sp, sp, -REGBYTES * 34

	# /* Save Context */
	STORE   x1, 0x0(sp)
	STORE   x2, 1 * REGBYTES(sp)
	STORE   x3, 2 * REGBYTES(sp)
	STORE   x4, 3 * REGBYTES(sp)
	STORE   x5, 4 * REGBYTES(sp)
	STORE   x6, 5 * REGBYTES(sp)
	STORE   x7, 6 * REGBYTES(sp)
	STORE   x8, 7 * REGBYTES(sp)
	STORE   x9, 8 * REGBYTES(sp)
	STORE   x10, 9 * REGBYTES(sp)
	STORE   x11, 10 * REGBYTES(sp)
	STORE   x12, 11 * REGBYTES(sp)
	STORE   x13, 12 * REGBYTES(sp)
	STORE   x14, 13 * REGBYTES(sp)
	STORE   x15, 14 * REGBYTES(sp)
	STORE   x16, 15 * REGBYTES(sp)
	STORE   x17, 16 * REGBYTES(sp)
	STORE   x18, 17 * REGBYTES(sp)
	STORE   x19, 18 * REGBYTES(sp)
	STORE   x20, 19 * REGBYTES(sp)
	STORE   x21, 20 * REGBYTES(sp)
	STORE   x22, 21 * REGBYTES(sp)
	STORE   x23, 22 * REGBYTES(sp)
	STORE   x24, 23 * REGBYTES(sp)
	STORE   x25, 24 * REGBYTES(sp)
	STORE   x26, 25 * REGBYTES(sp)
	STORE   x27, 26 * REGBYTES(sp)
	STORE   x28, 27 * REGBYTES(sp)
	STORE   x29, 28 * REGBYTES(sp)
	STORE   x30, 29 * REGBYTES(sp)
	STORE   x31, 30 * REGBYTES(sp)

	# /* Store current stackpointer in task control block (TCB) */
	la		t0, pxCurrentTCB    # //pointer
	csrr	t1, mhartid
	slli	t1, t1, 3
	add		t0, t0, t1
	LOAD	t0, 0x0(t0)
	STORE   sp, 0x0(t0)

	# csrr    t0, mstatus
	# STORE   t0, 32 * REGBYTES(sp)
	.endm

# /* Saves current error program counter (EPC) as task program counter */
.macro portSAVE_EPC
	csrr    t0, mepc
	STORE   t0, 31 * REGBYTES(sp)
	.endm

# /* Saves current return adress (RA) as task program counter */
.macro portSAVE_RA
	STORE   ra, 31 * REGBYTES(sp)
	.endm

# /* Macro for restoring task context */
.macro portRESTORE_CONTEXT

	.global pxCurrentTCB
	# /* Load stack pointer from the current TCB */
	la		t0, pxCurrentTCB
	csrr	t1, mhartid
	slli	t1, t1, 3
	add		t0, t0, t1
	LOAD	sp, 0x0(t0)
	LOAD    sp, 0x0(sp)

	# /* Load task program counter */
	LOAD    t0, 31 * REGBYTES(sp)
	csrw    mepc, t0

	# LOAD   t0, 32 * REGBYTES(sp)
	# csrw   mstatus, t0

	# /* Run in machine mode */
	li      t0, MSTATUS_MPP | MSTATUS_MPIE
	csrs    mstatus, t0

	# /* Restore registers,
	#    Skip global pointer because that does not change */
	LOAD    x1, 0x0(sp)
#	LOAD	x2, 1 * REGBYTES(sp)
	LOAD	x3, 2 * REGBYTES(sp)
	LOAD    x4, 3 * REGBYTES(sp)
	LOAD    x5, 4 * REGBYTES(sp)
	LOAD    x6, 5 * REGBYTES(sp)
	LOAD    x7, 6 * REGBYTES(sp)
	LOAD    x8, 7 * REGBYTES(sp)
	LOAD    x9, 8 * REGBYTES(sp)
	LOAD    x10, 9 * REGBYTES(sp)
	LOAD    x11, 10 * REGBYTES(sp)
	LOAD    x12, 11 * REGBYTES(sp)
	LOAD    x13, 12 * REGBYTES(sp)
	LOAD    x14, 13 * REGBYTES(sp)
	LOAD    x15, 14 * REGBYTES(sp)
	LOAD    x16, 15 * REGBYTES(sp)
	LOAD    x17, 16 * REGBYTES(sp)
	LOAD    x18, 17 * REGBYTES(sp)
	LOAD    x19, 18 * REGBYTES(sp)
	LOAD    x20, 19 * REGBYTES(sp)
	LOAD    x21, 20 * REGBYTES(sp)
	LOAD    x22, 21 * REGBYTES(sp)
	LOAD    x23, 22 * REGBYTES(sp)
	LOAD    x24, 23 * REGBYTES(sp)
	LOAD    x25, 24 * REGBYTES(sp)
	LOAD    x26, 25 * REGBYTES(sp)
	LOAD    x27, 26 * REGBYTES(sp)
	LOAD    x28, 27 * REGBYTES(sp)
	LOAD    x29, 28 * REGBYTES(sp)
	LOAD    x30, 29 * REGBYTES(sp)
	LOAD    x31, 30 * REGBYTES(sp)

	addi    sp, sp, REGBYTES * 34
	mret
	.endm

# /* Macro for system tick scheduling */
xPortSysTickInt:
	portSAVE_CONTEXT
	portSAVE_EPC
	jal vPortSysTickHandler
	portRESTORE_CONTEXT

xPortStartScheduler:
	jal     vPortSetupTimer
	portRESTORE_CONTEXT

vPortEndScheduler:
	ret
