/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
#ifndef _EVENLESS_ARM_ASM_SYSCALL_H
#define _EVENLESS_ARM_ASM_SYSCALL_H

#include <linux/uaccess.h>
#include <asm/unistd.h>
#include <asm/ptrace.h>
#include <asm/syscall.h>
#include <uapi/asm/dovetail.h>

#define raw_put_user(src, dst)  __put_user(src, dst)
#define raw_get_user(dst, src)  __get_user(dst, src)

#define is_oob_syscall(__regs)	((__regs)->ARM_r7 == __ARM_NR_dovetail)
#define oob_syscall_nr(__regs)	((__regs)->ARM_ORIG_r0)

#define oob_retval(__regs)	((__regs)->ARM_r0)
#define oob_arg1(__regs)	((__regs)->ARM_r1)
#define oob_arg2(__regs)	((__regs)->ARM_r2)
#define oob_arg3(__regs)	((__regs)->ARM_r3)
#define oob_arg4(__regs)	((__regs)->ARM_r4)
#define oob_arg5(__regs)	((__regs)->ARM_r5)

/*
 * Fetch and test inband syscall number (valid only if
 * !is_oob_syscall(__regs)).
 */
#define inband_syscall_nr(__regs, __nr)					\
	({								\
		*(__nr) = (__regs)->ARM_r7;				\
		*(__nr) < NR_syscalls || *(__nr) >= __ARM_NR_BASE;	\
	})

static inline void
set_oob_error(struct pt_regs *regs, int err)
{
	oob_retval(regs) = err;
}

static inline
void set_oob_retval(struct pt_regs *regs, long ret)
{
	oob_retval(regs) = ret;
}

#endif /* !_EVENLESS_ARM_ASM_SYSCALL_H */
