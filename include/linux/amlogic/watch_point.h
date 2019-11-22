/*
 * include/linux/amlogic/watch_point.h
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

#ifndef __AML_WATCH_POINT_H__
#define __AML_WATCH_POINT_H__

#include <uapi/linux/elf.h>
#include <uapi/linux/hw_breakpoint.h>
#include <linux/perf_event.h>

#define MAX_WATCH_POINTS	16

#ifdef CONFIG_ARM64
extern u64 read_wb_reg(int reg, int n);
#else
extern u32 read_wb_reg(int n);
#endif

#ifdef CONFIG_HAVE_HW_BREAKPOINT
extern int aml_watch_point_register(unsigned long addr,
				    unsigned int len,
				    unsigned int type,
				    perf_overflow_handler_t handle);

extern void aml_watch_point_remove(unsigned long addr);
#else
static inline int aml_watch_point_register(unsigned long addr,
					   unsigned int len,
					   unsigned int type,
					   perf_overflow_handler_t handle)
{
	return -1;
}

static inline void aml_watch_point_remove(unsigned long addr)
{

}
#endif

#endif
