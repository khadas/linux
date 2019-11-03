/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _EVL_X86_ASM_THREAD_H
#define _EVL_X86_ASM_THREAD_H

#include <asm/traps.h>

static inline bool evl_is_breakpoint(int trapnr)
{
	return trapnr == X86_TRAP_DB || trapnr == X86_TRAP_BP;
}

#endif /* !_EVL_X86_ASM_THREAD_H */
