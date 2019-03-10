/*
 * Derived from Xenomai Cobalt, https://xenomai.org/
 * Copyright (C) 2005 Stelian Pop
 */
#ifndef _EVENLESS_ARM_ASM_THREAD_H
#define _EVENLESS_ARM_ASM_THREAD_H

#define xnarch_fault_pf_p(__trapnr)	((__trapnr) == ARM_TRAP_ACCESS)
#define xnarch_fault_bp_p(__trapnr)	((current->ptrace & PT_PTRACED) && \
					 (__trapnr == ARM_TRAP_BREAK || \
					  (__trapnr) == ARM_TRAP_UNDEFINSTR))

#define xnarch_fault_notify(__trapnr) (!xnarch_fault_bp_p(__trapnr))

#endif /* !_EVENLESS_ARM_ASM_THREAD_H */
