/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
#ifndef _EVL_ARM_ASM_SYSCALL_H
#define _EVL_ARM_ASM_SYSCALL_H

#include <linux/uaccess.h>
#include <asm/unistd.h>
#include <asm/ptrace.h>
#include <asm/syscall.h>
#include <uapi/asm-generic/dovetail.h>

#define raw_put_user(src, dst)  __put_user(src, dst)
#define raw_get_user(dst, src)  __get_user(dst, src)

#define oob_retval(__regs)	((__regs)->ARM_r0)
#define oob_arg1(__regs)	((__regs)->ARM_r0)
#define oob_arg2(__regs)	((__regs)->ARM_r1)
#define oob_arg3(__regs)	((__regs)->ARM_r2)
#define oob_arg4(__regs)	((__regs)->ARM_r3)
#define oob_arg5(__regs)	((__regs)->ARM_r4)

static inline bool is_oob_syscall(struct pt_regs *regs)
{
	return !!(regs->ARM_r7 & __OOB_SYSCALL_BIT);
}

static inline unsigned int oob_syscall_nr(struct pt_regs *regs)
{
	return regs->ARM_r7 & ~__OOB_SYSCALL_BIT;
}

static inline
bool inband_syscall_nr(struct pt_regs *regs, unsigned int *nr)
{
	*nr = regs->ARM_r7;
	return *nr < NR_syscalls || *nr >= __ARM_NR_BASE;
}

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

static inline bool is_compat_oob_call(void)
{
	return false;
}

#endif /* !_EVL_ARM_ASM_SYSCALL_H */
