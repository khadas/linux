/*
 * cpu reset routines
 *
 * Copyright (C) 2015 Huawei Futurewei Technologies.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#if !defined(_ARM64_CPU_RESET_H)
#define _ARM64_CPU_RESET_H
#include <asm/virt.h>


void __attribute__((noreturn)) cpu_reset_kexec(unsigned long addr);
void __attribute__((noreturn)) cpu_soft_restart(phys_addr_t cpu_reset,
	unsigned long addr, unsigned long el2_switch);

#endif
