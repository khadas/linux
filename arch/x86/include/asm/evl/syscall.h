/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _EVL_X86_ASM_SYSCALL_H
#define _EVL_X86_ASM_SYSCALL_H

#include <linux/uaccess.h>
#include <asm/unistd.h>
#include <asm/ptrace.h>
#include <uapi/asm/evl/syscall.h>

#define raw_put_user(src, dst)  __put_user(src, dst)
#define raw_get_user(dst, src)  __get_user(dst, src)

#define is_oob_syscall(__regs)	((__regs)->orig_ax & __EVL_SYSCALL_BIT)
#define oob_syscall_nr(__regs)	((__regs)->orig_ax & ~__EVL_SYSCALL_BIT)

#define oob_retval(__regs)	((__regs)->ax)
#define oob_arg1(__regs)	((__regs)->di)
#define oob_arg2(__regs)	((__regs)->si)
#define oob_arg3(__regs)	((__regs)->dx)
#define oob_arg4(__regs)	((__regs)->r10)
#define oob_arg5(__regs)	((__regs)->r8)

/*
 * Fetch and test inband syscall number (valid only if
 * !is_oob_syscall(__regs)).
 */
#define inband_syscall_nr(__regs, __nr)			\
	({						\
		*(__nr) = oob_syscall_nr(__regs);	\
		!is_oob_syscall(__regs);		\
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

#endif /* !_EVL_X86_ASM_SYSCALL_H */
