/* * Amlogic M2
 * frame buffer driver-----------Deinterlace
 * Author: Rain Zhang <rain.zhang@amlogic.com>
 * Copyright (C) 2010 Amlogic, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the named License,
 * or any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 */

#include <linux/version.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/list.h>
#include <linux/ctype.h>
#include <linux/string.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/amports/vframe.h>
#include <linux/amlogic/canvas/canvas.h>
#include <linux/amlogic/canvas/canvas_mgr.h>
#include <linux/amlogic/vout/vout_notify.h>
#include "deinterlace_dbg.h"

void dump_di_reg(void)
{
	unsigned int i = 0, base_addr = 0;
	unsigned int size_reg_addr[57] = {
		0x1702, 0x1703, 0x2d01,
		0x2d01, 0x2d8f, 0x2d08,
		0x2d09, 0x2f00, 0x2f01,
		0x17d0, 0x17d1, 0x17d2,
		0x17d3, 0x17dd, 0x17de,
		0x17df, 0x17e0, 0x17f7,
		0x17f8, 0x17f9, 0x17fa,
		0x17c0, 0x17c1,	0x17a0,
		0x17a1, 0x17c3, 0x17c4,
		0x17cb, 0x17cc, 0x17a3,
		0x17a4, 0x17a5, 0x17a6,
		0x2f92, 0x2f93, 0x2f95,
		0x2f96, 0x2f98,	0x2f99,
		0x2f9b, 0x2f9c, 0x2f65,
		0x2f66, 0x2f67, 0x2f68,
		0x1a53, 0x1a54, 0x1a55,
		0x1a56, 0x17ea, 0x17eb,
		0x17ec, 0x17ed, 0x2012,
		0x2013, 0x2014, 0x2015
	};
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXLX))
		base_addr = 0xff900000;
	else
		base_addr = 0xd0100000;

	pr_info("----dump di reg----\n");
	for (i = 0; i < 255; i++) {
		if (i == 0x45)
			pr_info("----nr reg----");
		if (i == 0x80)
			pr_info("----3d reg----");
		if (i == 0x9e)
			pr_info("---nr reg done---");
		if (i == 0x9c)
			pr_info("---3d reg done---");
		pr_info("[0x%x][0x%x]=0x%x\n",
			base_addr + ((0x1700 + i) << 2),
			0x1700 + i, RDMA_RD(0x1700 + i));
	}
	pr_info("----dump mcdi reg----\n");
	for (i = 0; i < 201; i++)
		pr_info("[0x%x][0x%x]=0x%x\n",
			base_addr + ((0x2f00 + i) << 2),
			0x2f00 + i, RDMA_RD(0x2f00 + i));
	pr_info("----dump pulldown reg----\n");
	for (i = 0; i < 26; i++)
		pr_info("[0x%x][0x%x]=0x%x\n",
			base_addr + ((0x2fd0 + i) << 2),
			0x2fd0 + i, RDMA_RD(0x2fd0 + i));
	pr_info("----dump bit mode reg----\n");
	for (i = 0; i < 4; i++)
		pr_info("[0x%x][0x%x]=0x%x\n",
			base_addr + ((0x20a7 + i) << 2),
			0x20a7 + i, RDMA_RD(0x20a7 + i));
	pr_info("[0x%x][0x%x]=0x%x\n",
		base_addr + (0x2022 << 2),
		0x2022, RDMA_RD(0x2022));
	pr_info("[0x%x][0x%x]=0x%x\n",
		base_addr + (0x17c1 << 2),
		0x17c1, RDMA_RD(0x17c1));
	pr_info("[0x%x][0x%x]=0x%x\n",
		base_addr + (0x17c2 << 2),
		0x17c2, RDMA_RD(0x17c2));
	pr_info("[0x%x][0x%x]=0x%x\n",
		base_addr + (0x1aa7 << 2),
		0x1aa7, RDMA_RD(0x1aa7));
	pr_info("----dump dnr reg----\n");
	for (i = 0; i < 29; i++)
		pr_info("[0x%x][0x%x]=0x%x\n",
			base_addr + ((0x2d00 + i) << 2),
			0x2d00 + i, RDMA_RD(0x2d00 + i));
	pr_info("----dump if0 reg----\n");
	for (i = 0; i < 26; i++)
		pr_info("[0x%x][0x%x]=0x%x\n",
			base_addr + ((0x1a60 + i) << 2),
			0x1a50 + i, RDMA_RD(0x1a50 + i));
	pr_info("----dump gate reg----\n");
	for (i = 0; i < 5; i++)
		pr_info("[0x%x][0x%x]=0x%x\n",
			base_addr + ((0x2006 + i) << 2),
			0x2006 + i, RDMA_RD(0x2006 + i));
	pr_info("[0x%x][0x%x]=0x%x\n",
		base_addr + ((0x2dff) << 2),
		0x2dff, RDMA_RD(0x2dff));
	pr_info("----dump if2 reg----\n");
	for (i = 0; i < 29; i++)
		pr_info("[0x%x][0x%x]=0x%x\n",
			base_addr + ((0x2010 + i) << 2),
			0x2010 + i, RDMA_RD(0x2010 + i));
	pr_info("----dump size reg---");
	for (i = 0; i < 57; i++)
		pr_info("[0x%x][0x%x]=0x%x\n",
			base_addr + ((size_reg_addr[i]) << 2),
			0x2010 + i, RDMA_RD(size_reg_addr[i]));
	pr_info("----dump reg done----\n");
}
