/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _EVL_ARM64_ASM_SYSCALL_H
#define _EVL_ARM64_ASM_SYSCALL_H

#include <linux/uaccess.h>
#include <asm/unistd.h>
#include <asm/ptrace.h>
#include <uapi/asm-generic/dovetail.h>

#define raw_put_user(src, dst)  __put_user(src, dst)
#define raw_get_user(dst, src)  __get_user(dst, src)

#define oob_retval(__regs)	((__regs)->regs[0])
#define oob_arg1(__regs)	((__regs)->regs[0])
#define oob_arg2(__regs)	((__regs)->regs[1])
#define oob_arg3(__regs)	((__regs)->regs[2])
#define oob_arg4(__regs)	((__regs)->regs[3])
#define oob_arg5(__regs)	((__regs)->regs[4])

#define __ARM_NR_BASE_compat		0xf0000

static inline bool is_oob_syscall(const struct pt_regs *regs)
{
	return !!(regs->syscallno & __OOB_SYSCALL_BIT);
}

static inline unsigned int oob_syscall_nr(const struct pt_regs *regs)
{
	return regs->syscallno & ~__OOB_SYSCALL_BIT;
}

static inline bool
inband_syscall_nr(struct pt_regs *regs, unsigned int *nr)
{
	*nr = oob_syscall_nr(regs);

	return !is_oob_syscall(regs);
}

static inline void set_oob_error(struct pt_regs *regs, int err)
{
	oob_retval(regs) = err;
}

static inline void set_oob_retval(struct pt_regs *regs, long ret)
{
	oob_retval(regs) = ret;
}

#ifdef CONFIG_COMPAT
static inline bool is_compat_oob_call(void)
{
	return is_compat_task();
}
#else
static inline bool is_compat_oob_call(void)
{
	return false;
}
#endif

#endif /* !_EVL_ARM64_ASM_SYSCALL_H */
