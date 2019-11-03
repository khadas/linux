/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _EVL_ARM64_ASM_THREAD_H
#define _EVL_ARM64_ASM_THREAD_H

static inline bool evl_is_breakpoint(int trapnr)
{
	return trapnr == ARM64_TRAP_DEBUG || trapnr == ARM64_TRAP_UNDI;
}

#endif /* !_EVL_ARM64_ASM_THREAD_H */
