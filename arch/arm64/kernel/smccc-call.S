/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2015, Linaro Limited
 */
#include <linux/linkage.h>
#include <linux/arm-smccc.h>

#include <asm/asm-offsets.h>
#include <asm/assembler.h>

	.macro SMCCC instr
	.cfi_startproc
	\instr	#0
	ldr	x4, [sp]
	stp	x0, x1, [x4, #ARM_SMCCC_RES_X0_OFFS]
	stp	x2, x3, [x4, #ARM_SMCCC_RES_X2_OFFS]
	ldr	x4, [sp, #8]
	cbz	x4, 1f /* no quirk structure */
	ldr	x9, [x4, #ARM_SMCCC_QUIRK_ID_OFFS]
	cmp	x9, #ARM_SMCCC_QUIRK_QCOM_A6
	b.ne	1f
	str	x6, [x4, ARM_SMCCC_QUIRK_STATE_OFFS]
1:
#ifndef CONFIG_AMLOGIC_DEBUG_LOCKUP
	ret
#endif
	.cfi_endproc
	.endm

/*
 * void arm_smccc_smc(unsigned long a0, unsigned long a1, unsigned long a2,
 *		  unsigned long a3, unsigned long a4, unsigned long a5,
 *		  unsigned long a6, unsigned long a7, struct arm_smccc_res *res,
 *		  struct arm_smccc_quirk *quirk)
 */
ENTRY(__arm_smccc_smc)
#ifdef CONFIG_AMLOGIC_DEBUG_LOCKUP
	stp x0, x1, [sp, #-16]!
	stp x2, x3, [sp, #-16]!
	stp x4, x5, [sp, #-16]!
	stp x6, x7, [sp, #-16]!
	stp x29, x30, [sp, #-16]!
	bl smc_trace_start
	ldp x29, x30, [sp], #16
	ldp x6, x7, [sp], #16
	ldp x4, x5, [sp], #16
	ldp x2, x3, [sp], #16
	ldp x0, x1, [sp], #16
#endif
	SMCCC	smc
#ifdef CONFIG_AMLOGIC_DEBUG_LOCKUP
	stp x29, x30, [sp, #-16]!
	bl smc_trace_stop
	ldp x29, x30, [sp], #16
	ret
#endif
ENDPROC(__arm_smccc_smc)
EXPORT_SYMBOL(__arm_smccc_smc)

/*
 * void arm_smccc_hvc(unsigned long a0, unsigned long a1, unsigned long a2,
 *		  unsigned long a3, unsigned long a4, unsigned long a5,
 *		  unsigned long a6, unsigned long a7, struct arm_smccc_res *res,
 *		  struct arm_smccc_quirk *quirk)
 */
ENTRY(__arm_smccc_hvc)
	SMCCC	hvc
ENDPROC(__arm_smccc_hvc)
EXPORT_SYMBOL(__arm_smccc_hvc)
