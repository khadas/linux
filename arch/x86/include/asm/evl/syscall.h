/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _EVL_X86_ASM_SYSCALL_H
#define _EVL_X86_ASM_SYSCALL_H

#include <linux/uaccess.h>
#include <asm/unistd.h>
#include <asm/ptrace.h>
#include <uapi/asm-generic/dovetail.h>

#define raw_put_user(src, dst)  __put_user(src, dst)
#define raw_get_user(dst, src)  __get_user(dst, src)

static inline bool
is_valid_inband_syscall(unsigned int nr)
{
	return nr < NR_syscalls;
}

static inline bool is_compat_oob_call(void)
{
	return in_ia32_syscall();
}

#endif /* !_EVL_X86_ASM_SYSCALL_H */
