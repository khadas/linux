/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * include/linux/amlogic/debug_ftrace_ramoops.h
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#ifndef __DEBUG_FTRACE_RAMOOPS_H__
#define  __DEBUG_FTRACE_RAMOOPS_H__
#define __DEBUG_FTRACE_RAMOOPS_H__
#include <linux/ftrace.h>
#include <linux/pstore_ram.h>
#include <linux/arm-smccc.h>

extern unsigned int ramoops_ftrace_en;
extern int ramoops_io_en;
extern int ramoops_io_dump;
extern unsigned int dump_iomap;

#define PSTORE_FLAG_FUNC	0x1
#define PSTORE_FLAG_IO_R	0x2
#define PSTORE_FLAG_IO_W	0x3
#define PSTORE_FLAG_IO_R_END	0x4
#define PSTORE_FLAG_IO_W_END	0x5
#define PSTORE_FLAG_IO_TAG	0x6
#define PSTORE_FLAG_IO_SCHED_SWITCH 0x7
#define PSTORE_FLAG_IO_SMC_IN   0x8
#define PSTORE_FLAG_IO_SMC_OUT  0x9
#define PSTORE_FLAG_IO_SMC_NORET_IN  0xA
#define PSTORE_FLAG_CLK_ENABLE 0XB
#define PSTORE_FLAG_CLK_DISABLE 0XC
#define PSTORE_FLAG_PD_POWER_ON 0XD
#define PSTORE_FLAG_PD_POWER_OFF 0XE
#define PSTORE_FLAG_MASK	0xF

void notrace pstore_io_save(unsigned long reg, unsigned long val,
			    unsigned long parant, unsigned int flag,
			    unsigned long *irq_flag);

void pstore_ftrace_dump_old(struct persistent_ram_zone *prz);

void save_iomap_info(unsigned long virt_addr, unsigned long phys_addr,
		     unsigned int size);

bool notrace is_noret_smcid(unsigned long smcid);

//#define SKIP_IO_TRACE
#if (defined CONFIG_AMLOGIC_DEBUG_FTRACE_PSTORE) && (!defined SKIP_IO_TRACE)
#define pstore_ftrace_io_wr(reg, val)	\
unsigned long irqflg;					\
pstore_io_save(reg, val, CALLER_ADDR0, PSTORE_FLAG_IO_W, &irqflg)

#define pstore_ftrace_io_wr_end(reg, val)	\
pstore_io_save(reg, 0, CALLER_ADDR0, PSTORE_FLAG_IO_W_END, &irqflg)

#define pstore_ftrace_io_rd(reg)		\
unsigned long irqflg;					\
pstore_io_save(reg, 0, CALLER_ADDR0, PSTORE_FLAG_IO_R, &irqflg)
#define pstore_ftrace_io_rd_end(reg)	\
pstore_io_save(reg, 0, CALLER_ADDR0, PSTORE_FLAG_IO_R_END, &irqflg)

#define need_dump_iomap()		(ramoops_io_en | dump_iomap)

#define pstore_ftrace_io_tag(reg, val)	\
	pstore_io_save(reg, val, CALLER_ADDR0, PSTORE_FLAG_IO_TAG, NULL)

#define pstore_ftrace_sched_switch(next_pid, next_comm)	\
pstore_io_save(next_pid, next_comm, 0, PSTORE_FLAG_IO_SCHED_SWITCH, NULL)

#define pstore_ftrace_io_smc_in(a0, a1)	\
pstore_io_save(a0, a1, CALLER_ADDR0, PSTORE_FLAG_IO_SMC_IN, NULL)

#define pstore_ftrace_io_smc_noret_in(a0, a1)	\
pstore_io_save(a0, a1, CALLER_ADDR0, PSTORE_FLAG_IO_SMC_NORET_IN, NULL)

#define pstore_ftrace_io_smc_out(a0, a1) \
pstore_io_save(a0, a1, CALLER_ADDR0, PSTORE_FLAG_IO_SMC_OUT, NULL)

#define pstore_ftrace_clk_enable(name) \
pstore_io_save(0, name, CALLER_ADDR0, PSTORE_FLAG_CLK_ENABLE, NULL)

#define pstore_ftrace_clk_disable(name) \
pstore_io_save(0, name, CALLER_ADDR0, PSTORE_FLAG_CLK_DISABLE, NULL)

#define pstore_ftrace_pd_power_on(name) \
pstore_io_save(0, name, CALLER_ADDR0, PSTORE_FLAG_PD_POWER_ON, NULL)

#define pstore_ftrace_pd_power_off(name) \
pstore_io_save(0, name, CALLER_ADDR0, PSTORE_FLAG_PD_POWER_OFF, NULL)

#else

#define pstore_ftrace_io_wr(reg, val)		do {	} while (0)
#define pstore_ftrace_io_rd(reg)		do {	} while (0)
#define need_dump_iomap()			0
#define pstore_ftrace_io_wr_end(reg, val)	do {	} while (0)
#define pstore_ftrace_io_rd_end(reg)		do {	} while (0)
#define pstore_ftrace_io_tag(reg, val)		do {	} while (0)
#define pstore_ftrace_sched_switch(next_pid, next_comm) do {	} while (0)
#define pstore_ftrace_io_smc_in(a0, a1)		do {	} while (0)
#define pstore_ftrace_io_smc_out(a0, a1)	do {	} while (0)
#define pstore_ftrace_io_smc_noret_in(a0, a1)	do {	} while (0)
#define pstore_ftrace_clk_enable(name)		do {	} while (0)
#define pstore_ftrace_clk_disable(name)		do {	} while (0)
#define pstore_ftrace_pd_power_on(name)		do {	} while (0)
#define pstore_ftrace_pd_power_off(name)	do {	} while (0)

#endif

#endif
