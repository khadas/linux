/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _EVL_ARM64_ASM_THREAD_H
#define _EVL_ARM64_ASM_THREAD_H

#define xnarch_fault_pf_p(__trapnr)	((__trapnr) == ARM64_TRAP_ACCESS)
#define xnarch_fault_bp_p(__trapnr)	((current->ptrace & PT_PTRACED) && \
					 (__trapnr == ARM64_TRAP_DEBUG || \
					  (__trapnr) == ARM64_TRAP_UNDI))

#define xnarch_fault_notify(__trapnr) (!xnarch_fault_bp_p(__trapnr))

#endif /* !_EVL_ARM64_ASM_THREAD_H */
