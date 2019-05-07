/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _EVL_X86_ASM_THREAD_H
#define _EVL_X86_ASM_THREAD_H

#include <asm/traps.h>

#define xnarch_fault_pf_p(__trapnr)	((__trapnr) == X86_TRAP_PF)
#define xnarch_fault_bp_p(__trapnr)	((current->ptrace & PT_PTRACED) && \
					(__trapnr == X86_TRAP_DB ||	\
						(__trapnr) == X86_TRAP_BP))

#define xnarch_fault_notify(__trapnr) (!xnarch_fault_bp_p(__trapnr))

#endif /* !_EVL_X86_ASM_THREAD_H */
