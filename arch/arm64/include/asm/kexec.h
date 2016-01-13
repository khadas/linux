/*
 * kexec for arm64
 *
 * Copyright (C) Linaro.
 * Copyright (C) Huawei Futurewei Technologies.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#if !defined(_ARM64_KEXEC_H)
#define _ARM64_KEXEC_H

/* Maximum physical address we can use pages from */

#define KEXEC_SOURCE_MEMORY_LIMIT (-1UL)

/* Maximum address we can reach in physical address mode */

#define KEXEC_DESTINATION_MEMORY_LIMIT (-1UL)

/* Maximum address we can use for the control code buffer */

#define KEXEC_CONTROL_MEMORY_LIMIT (-1UL)

#define KEXEC_CONTROL_PAGE_SIZE	4096

#define KEXEC_ARCH KEXEC_ARCH_ARM64

#if !defined(__ASSEMBLY__)
extern bool in_crash_kexec;
extern const unsigned char arm64_relocate_new_kernel[];
extern const unsigned long arm64_relocate_new_kernel_size;
extern unsigned long arm64_kexec_kimage_head_offset;
extern unsigned long arm64_kexec_kimage_start_offset;


/**
 * crash_setup_regs() - save registers for the panic kernel
 *
 * @newregs: registers are saved here
 * @oldregs: registers to be saved (may be %NULL)
 */

static inline void crash_setup_regs(struct pt_regs *newregs,
				    struct pt_regs *oldregs)
{
	if (oldregs) {
		memcpy(newregs, oldregs, sizeof(*newregs));
	} else {
		__asm__ __volatile__ (
			"stp	 x0,   x1, [%3, #16 *  0]\n"
			"stp	 x2,   x3, [%3, #16 *  1]\n"
			"stp	 x4,   x5, [%3, #16 *  2]\n"
			"stp	 x6,   x7, [%3, #16 *  3]\n"
			"stp	 x8,   x9, [%3, #16 *  4]\n"
			"stp	x10,  x11, [%3, #16 *  5]\n"
			"stp	x12,  x13, [%3, #16 *  6]\n"
			"stp	x14,  x15, [%3, #16 *  7]\n"
			"stp	x16,  x17, [%3, #16 *  8]\n"
			"stp	x18,  x19, [%3, #16 *  9]\n"
			"stp	x20,  x21, [%3, #16 * 10]\n"
			"stp	x22,  x23, [%3, #16 * 11]\n"
			"stp	x24,  x25, [%3, #16 * 12]\n"
			"stp	x26,  x27, [%3, #16 * 13]\n"
			"stp	x28,  x29, [%3, #16 * 14]\n"
			"str	x30,	   [%3, #16 * 15]\n"
			"mov	%0, sp\n"
			"adr	%1, 1f\n"
			"mrs	%2, spsr_el1\n"
		"1:"
			: "=r" (newregs->sp),
			  "=r" (newregs->pc),
			  "=r" (newregs->pstate)
			: "r"  (&newregs->regs)
			: "memory"
		);
	}
}

#endif /* !defined(__ASSEMBLY__) */

#endif
