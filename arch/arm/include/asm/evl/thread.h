/*
 * Derived from Xenomai Cobalt, https://xenomai.org/
 * Copyright (C) 2005 Stelian Pop
 */
#ifndef _EVL_ARM_ASM_THREAD_H
#define _EVL_ARM_ASM_THREAD_H

static inline bool evl_is_breakpoint(int trapnr)
{
	return trapnr == ARM_TRAP_BREAK || trapnr == ARM_TRAP_UNDEFINSTR;
}

#endif /* !_EVL_ARM_ASM_THREAD_H */
