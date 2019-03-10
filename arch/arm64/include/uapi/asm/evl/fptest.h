/*
 * SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note
 *
 * Derived from Xenomai Cobalt, https://xenomai.org/
 * Copyright (C) 2006 Gilles Chanteperdrix <gilles.chanteperdrix@xenomai.org>.
 */
#ifndef _EVL_ARM64_ASM_UAPI_FPTEST_H
#define _EVL_ARM64_ASM_UAPI_FPTEST_H

#define evl_arm64_fpsimd  0x1
#define evl_arm64_sve     0x2

/*
 * CAUTION: keep this code strictly inlined in macros: we don't want
 * GCC to apply any callee-saved logic to fpsimd registers in
 * evl_set_fpregs() before evl_check_fpregs() can verify their
 * contents, but we still want GCC to know about the registers we have
 * clobbered.
 */

#define evl_set_fpregs(__features, __val)				\
	do {								\
		unsigned int __i;					\
		__u64 __e[32];						\
									\
		if (__features & evl_arm64_fpsimd) {			\
			for (__i = 0; __i < 32; __i++)			\
				__e[__i] = (__val);			\
			__asm__ __volatile__("ldp  d0, d1, [%0, #8 * 0] \n"	\
					     "ldp  d2, d3, [%0, #8 * 2] \n"	\
					     "ldp  d4, d5, [%0, #8 * 4]\n"	\
					     "ldp  d6, d7, [%0, #8 * 6]\n"	\
					     "ldp  d8, d9, [%0, #8 * 8]\n"	\
					     "ldp  d10, d11, [%0, #8 * 10]\n"	\
					     "ldp  d12, d13, [%0, #8 * 12]\n"	\
					     "ldp  d14, d15, [%0, #8 * 14]\n"	\
					     "ldp  d16, d17, [%0, #8 * 16]\n"	\
					     "ldp  d18, d19, [%0, #8 * 18]\n"	\
					     "ldp  d20, d21, [%0, #8 * 20]\n"	\
					     "ldp  d22, d23, [%0, #8 * 22]\n"	\
					     "ldp  d24, d25, [%0, #8 * 24]\n"	\
					     "ldp  d26, d27, [%0, #8 * 26]\n"	\
					     "ldp  d28, d29, [%0, #8 * 28]\n"	\
					     "ldp  d30, d31, [%0, #8 * 30]\n"	\
					     : /* No outputs. */	\
					     : "r"(&__e[0])		\
					     : "d0", "d1", "d2", "d3", "d4", "d5", "d6",	\
					       "d7", "d8", "d9", "d10", "d11", "d12", "d13",	\
					       "d14", "d15", "d16", "d17", "d18", "d19",	\
					       "d20", "d21", "d22", "d23", "d24", "d25",	\
					       "d26", "d27", "d28", "d29", "d30", "d31",	\
					       "memory");		\
		}							\
	} while (0)

#define evl_check_fpregs(__features, __val, __bad)			\
	({								\
		unsigned int __result = (__val), __i;			\
		__u64 __e[32];						\
									\
		if (__features & evl_arm64_fpsimd) {			\
			__asm__ __volatile__("stp  d0, d1, [%0, #8 * 0] \n"	\
					     "stp  d2, d3, [%0, #8 * 2] \n"	\
					     "stp  d4, d5, [%0, #8 * 4]\n"	\
					     "stp  d6, d7, [%0, #8 * 6]\n"	\
					     "stp  d8, d9, [%0, #8 * 8]\n"	\
					     "stp  d10, d11, [%0, #8 * 10]\n"	\
					     "stp  d12, d13, [%0, #8 * 12]\n"	\
					     "stp  d14, d15, [%0, #8 * 14]\n"	\
					     "stp  d16, d17, [%0, #8 * 16]\n"	\
					     "stp  d18, d19, [%0, #8 * 18]\n"	\
					     "stp  d20, d21, [%0, #8 * 20]\n"	\
					     "stp  d22, d23, [%0, #8 * 22]\n"	\
					     "stp  d24, d25, [%0, #8 * 24]\n"	\
					     "stp  d26, d27, [%0, #8 * 26]\n"	\
					     "stp  d28, d29, [%0, #8 * 28]\n"	\
					     "stp  d30, d31, [%0, #8 * 30]\n"	\
					     :  /* No outputs. */	\
					     : "r"(&__e[0])		\
					     : "memory");		\
									\
			for (__i = 0; __i < 32; __i++)			\
				if (__e[__i] != __val) {		\
					__result = __e[__i];		\
					(__bad) = __i;			\
					break;				\
				}					\
		}							\
		__result;						\
	})

#endif /* !_EVL_ARM64_ASM_UAPI_FPTEST_H */
