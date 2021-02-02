/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Based on arch/arm/include/asm/system_misc.h
 *
 * Copyright (C) 2012 ARM Ltd.
 */
#ifndef __ASM_SYSTEM_MISC_H
#define __ASM_SYSTEM_MISC_H

#ifndef __ASSEMBLY__

#include <linux/compiler.h>
#include <linux/linkage.h>
#include <linux/irqflags.h>
#include <linux/signal.h>
#include <linux/ratelimit.h>
#include <linux/reboot.h>

struct pt_regs;

void die(const char *msg, struct pt_regs *regs, int err);

struct siginfo;
void arm64_notify_die(const char *str, struct pt_regs *regs,
		      int signo, int sicode, void __user *addr,
		      int err);

void hook_debug_fault_code(int nr, int (*fn)(unsigned long, unsigned int,
					     struct pt_regs *),
			   int sig, int code, const char *name);

struct mm_struct;
#ifdef CONFIG_AMLOGIC_USER_FAULT
void show_all_pfn(struct task_struct *task, struct pt_regs *regs);
void show_vma(struct mm_struct *mm, unsigned long addr);
void _dump_dmc_reg(void);
#else
static inline void show_all_pfn(struct task_struct *task, struct pt_regs *regs)
{
}

static inline void show_vma(struct mm_struct *mm, unsigned long addr)
{
}

static inline void _dump_dmc_reg(void)
{
}
#endif /* CONFIG_AMLOGIC_USER_FAULT */
extern void __show_regs(struct pt_regs *);

#endif	/* __ASSEMBLY__ */

#endif	/* __ASM_SYSTEM_MISC_H */
