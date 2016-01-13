/*
 * cpu reset routines
 *
 * Copyright (C) 2001 Deep Blue Solutions Ltd.
 * Copyright (C) 2012 ARM Ltd.
 * Copyright (C) Huawei Futurewei Technologies.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/errno.h>
#include <linux/linkage.h>
#include <linux/init.h>
#include <asm/assembler.h>
#include <asm/cpufeature.h>
/*#include <asm/alternative.h>*/
#include <asm/virt.h>

.text
.pushsection    .idmap.text, "ax"

.align 5

/*
 * cpu_reset(addr, el2_switch) - Helper for cpu_soft_restart.
 *
 * @addr: Location to jump to for soft reset.
 * @el2_switch: Flag to indicate a swich to EL2 is needed.
 *
 * Put the CPU into the same state as it would be if it had been reset, and
 * branch to what would be the reset vector. It must be executed with the
 * flat identity mapping.
 */

ENTRY(cpu_reset_kexec)
	mrs	x2, sctlr_el1
	bic	x2, x2, #1
	msr	sctlr_el1, x2			// disable the MMU
	isb

	cbz	x1, 1f				// el2_switch?
	mov	x1, xzr
	mov	x2, xzr
	mov	x3, xzr
	hvc	#HVC_CALL_FUNC			// no return

1:	ret	x0
ENDPROC(cpu_reset_kexec)

/*
 * cpu_soft_restart(cpu_reset, addr, el2_switch) - Perform a cpu soft reset.
 *
 * @cpu_reset: Physical address of the cpu_reset routine.
 * @addr: Location to jump to for soft reset, passed to cpu_reset.
 * @el2_switch: Flag to indicate a swich to EL2 is needed, passed to cpu_reset.
 */

ENTRY(cpu_soft_restart)
	mov	x19, x0				// cpu_reset
	mov	x20, x1				// addr
	mov	x21, x2				// el2_switch

	/* Turn D-cache off */
	mrs	x0, sctlr_el1
	bic	x0, x0, #1 << 2			// clear SCTLR.C
	msr	sctlr_el1, x0
	isb

	mov	x0, x20
	mov	x1, x21
	ret	x19
ENDPROC(cpu_soft_restart)

.popsection
