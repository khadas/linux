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

static inline bool
is_valid_inband_syscall(unsigned int nr)
{
	return nr < NR_syscalls || nr >= __ARM_NR_BASE;
}

static inline bool is_compat_oob_call(void)
{
	return false;
}

#endif /* !_EVL_ARM_ASM_SYSCALL_H */
