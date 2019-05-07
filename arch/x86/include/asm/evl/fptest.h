/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _EVL_X86_ASM_FPTEST_H
#define _EVL_X86_ASM_FPTEST_H

#include <asm/fpu/api.h>
#include <uapi/asm/evl/fptest.h>

static inline bool evl_begin_fpu(void)
{
	kernel_fpu_begin();
	/*
	 * We need a clean context for testing the sanity of the FPU
	 * register stack across switches in evl_check_fpregs()
	 * (fildl->fistpl), which kernel_fpu_begin() does not
	 * guarantee us. Force this manually.
	 */
	asm volatile("fninit");

	return true;
}

static inline void evl_end_fpu(void)
{
	kernel_fpu_end();
}

static inline u32 evl_detect_fpu(void)
{
	u32 features = 0;

	/* We only test XMM2 and AVX switching when present. */

	if (boot_cpu_has(X86_FEATURE_XMM2))
		features |= evl_x86_xmm2;

	if (boot_cpu_has(X86_FEATURE_AVX))
		features |= evl_x86_avx;

	return features;
}

#endif /* _EVL_X86_ASM_FPTEST_H */
