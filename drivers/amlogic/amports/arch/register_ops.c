/*
 * drivers/amlogic/amports/arch/register_ops.c
 *
 * Copyright (C) 2015 Amlogic, Inc. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/amlogic/cpu_version.h>

#include "register_ops.h"

static struct chip_register_ops *amports_ops[BUS_MAX];

int codec_reg_read(u32 bus_type, unsigned int reg)
{
	struct chip_register_ops *ops = amports_ops[bus_type];
	if (ops && ops->read)
		return ops->read(ops->ext_offset + reg);
	return 0;
}

void codec_reg_write(u32 bus_type, unsigned int reg, unsigned int val)
{
	struct chip_register_ops *ops = amports_ops[bus_type];
	if (ops && ops->write)
		return ops->write(ops->ext_offset + reg, val);
}

void codec_reg_write_bits(u32 bus_type, unsigned int reg,
			u32 val, int start, int len)
{
/*
#define WRITE_SEC_REG_BITS(r, val, start, len) \
	WRITE_SEC_REG(r, (READ_SEC_REG(r) & ~(((1L<<(len))-1)<<(start)))|\
		    ((unsigned)((val)&((1L<<(len))-1)) << (start)))

*/
	u32 toval = codec_reg_read(bus_type, reg);
	u32 mask = (((1L<<(len))-1)<<(start));
	toval &= ~mask;
	toval |= (val<<start) & mask;
	codec_reg_write(bus_type, reg, toval);
}


static int register_reg_onebus_ops(struct chip_register_ops *ops)
{
	if (ops->bus_type >= BUS_MAX)
		return -1;
	pr_info("register amports ops for bus[%d]\n", ops->bus_type);
	if (amports_ops[ops->bus_type] != NULL)
		kfree(amports_ops[ops->bus_type]);
	amports_ops[ops->bus_type] = ops;
	return 0;
}

int register_reg_ops_per_cpu(int cputype,
	struct chip_register_ops *sops, int ops_size)
{
	struct chip_register_ops *ops;
	int i;

	if (cputype != get_cpu_type()) {
		pr_info("ignore bus ops for cpu=%d\n",
			cputype);
		return 0;	/* ignore don't needed firmare. */
	}
	ops = kmalloc(sizeof(struct chip_register_ops) * ops_size, GFP_KERNEL);
	if (!ops)
		return -ENOMEM;
	memcpy(ops, sops, sizeof(struct chip_register_ops) * ops_size);
	for (i = 0; i < ops_size; i++)
		register_reg_onebus_ops(&ops[i]);
	return 0;
}

int register_reg_ops_mgr(int cputype[],
	struct chip_register_ops *sops_list, int ops_size)
{
	int i = 0;
	while (cputype[i] > 0) {
		register_reg_ops_per_cpu(cputype[i], sops_list, ops_size);
		i++;
	}
	return 0;
}

