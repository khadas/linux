/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __ASM_IRQFLAGS_DEBUG_ARM64_H
#define __ASM_IRQFLAGS_DEBUG_ARM64_H

#ifdef __KERNEL__

/*
 * CPU interrupt mask handling.
 */
#include <linux/amlogic/debug_lockup.h>

static inline void arch_local_irq_enable(void)
{
	irq_trace_stop(0);

	if (system_has_prio_mask_debugging()) {
		u32 pmr = read_sysreg_s(SYS_ICC_PMR_EL1);

		WARN_ON_ONCE(pmr != GIC_PRIO_IRQON && pmr != GIC_PRIO_IRQOFF);
	}

	asm volatile(ALTERNATIVE(
		"msr    daifclr, #2             // arch_local_irq_enable\n"
		"nop",
		__msr_s(SYS_ICC_PMR_EL1, "%0")
		"dsb    sy",
		ARM64_HAS_IRQ_PRIO_MASKING)
		:
		: "r" ((unsigned long)GIC_PRIO_IRQON)
		: "memory");
}

static inline void arch_local_irq_disable(void)
{
	if (system_has_prio_mask_debugging()) {
		u32 pmr = read_sysreg_s(SYS_ICC_PMR_EL1);

		WARN_ON_ONCE(pmr != GIC_PRIO_IRQON && pmr != GIC_PRIO_IRQOFF);
	}

	asm volatile(ALTERNATIVE(
		"msr    daifset, #2             // arch_local_irq_disable",
		__msr_s(SYS_ICC_PMR_EL1, "%0"),
		ARM64_HAS_IRQ_PRIO_MASKING)
		:
		: "r" ((unsigned long)GIC_PRIO_IRQOFF)
		: "memory");
}

#define local_fiq_enable()	asm("msr	daifclr, #1" : : : "memory")
#define local_fiq_disable()	asm("msr	daifset, #1" : : : "memory")

/*
 * Save the current interrupt enable state.
 */
static inline unsigned long arch_local_save_flags(void)
{
	unsigned long flags;

	asm volatile(ALTERNATIVE(
		"mrs    %0, daif",
		__mrs_s("%0", SYS_ICC_PMR_EL1),
		ARM64_HAS_IRQ_PRIO_MASKING)
		: "=&r" (flags)
		:
		: "memory");

	return flags;
}

/*
 * restore saved IRQ state
 */
static inline void arch_local_irq_restore(unsigned long flags)
{
	irq_trace_stop(flags);

	asm volatile(ALTERNATIVE(
			"msr    daif, %0\n"
			"nop",
			__msr_s(SYS_ICC_PMR_EL1, "%0")
			"dsb    sy",
			ARM64_HAS_IRQ_PRIO_MASKING)
		:
		: "r" (flags)
		: "memory");
}

static inline int arch_irqs_disabled_flags(unsigned long flags)
{
	int res;

	asm volatile(ALTERNATIVE(
		"and    %w0, %w1, #" __stringify(PSR_I_BIT),
		"eor    %w0, %w1, #" __stringify(GIC_PRIO_IRQON),
		ARM64_HAS_IRQ_PRIO_MASKING)
		: "=&r" (res)
		: "r" ((int)flags)
		: "memory");

	return res;
}

static inline unsigned long arch_local_irq_save(void)
{
	unsigned long flags;

	flags = arch_local_save_flags();

	/*
	 * There are too many states with IRQs disabled, just keep the current
	 * state if interrupts are already disabled/masked.
	 */
	if (!arch_irqs_disabled_flags(flags))
		arch_local_irq_disable();

	irq_trace_start(flags);
	return flags;
}

#endif
#endif
