/*
 * SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note
 *
 * Derived from Xenomai Cobalt, https://xenomai.org/
 * Copyright (C) 2006 Gilles Chanteperdrix <gilles.chanteperdrix@xenomai.org>.
 */
#ifndef _EVL_ARM_ASM_UAPI_FPTEST_H
#define _EVL_ARM_ASM_UAPI_FPTEST_H

#define evl_arm_vfp  0x1

#define evl_set_fpregs(__features, __val)				\
	do {								\
		unsigned int __i;					\
		__u64 __e[16];						\
									\
		if (__features & evl_arm_vfp) {				\
			for (__i = 0; __i < 16; __i++)			\
				__e[__i] = (__val);			\
			/* vldm %0!, {d0-d15}, AKA fldmiax %0!, {d0-d15} */ \
			__asm__ __volatile__("ldc p11, cr0, [%0],#32*4": \
					     "=r"(__i):			\
					     "0"(&__e[0]): "memory");	\
		}							\
	} while (0)

#define evl_check_fpregs(__features, __val, __bad)			\
	({								\
		unsigned int __result = (__val), __i;			\
		__u64 __e[16];						\
									\
		if (__features & evl_arm_vfp) {				\
			/* vstm %0!, {d0-d15}, AKA fstmiax %0!, {d0-d15} */ \
			__asm__ __volatile__("stc p11, cr0, [%0],#32*4": \
					     "=r"(__i):			\
					     "0"(&__e[0]): "memory");	\
			for (__i = 0; __i < 16; __i++)			\
				if (__e[__i] != __val) {		\
					__result = __e[__i];		\
					(__bad) = __i;			\
					break;				\
				}					\
		}							\
		__result;						\
	})

#endif /* !_EVL_ARM_ASM_UAPI_FPTEST_H */
