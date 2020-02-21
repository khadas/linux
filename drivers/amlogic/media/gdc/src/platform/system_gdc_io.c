// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include "system_log.h"

#include <linux/io.h>
#include <linux/of_address.h>
#include <linux/errno.h>
#include <linux/kernel.h>

static void *p_hw_base;

int32_t init_gdc_io(struct device_node *dn)
{
	p_hw_base = of_iomap(dn, 0);

	gdc_log(LOG_INFO, "reg base = %p\n", p_hw_base);
	if (!p_hw_base) {
		gdc_log(LOG_DEBUG, "failed to map register, %p\n", p_hw_base);
		return -1;
	}

	return 0;
}

void close_gdc_io(struct device_node *dn)
{
	gdc_log(LOG_DEBUG, "IO functionality has been closed");
}

u32 system_gdc_read_32(u32 addr)
{
	u32 result = 0;

	if (!p_hw_base) {
		gdc_log(LOG_ERR, "Failed to base address %d\n", addr);
		return 0;
	}

	result = ioread32(p_hw_base + addr);
	gdc_log(LOG_DEBUG, "r [0x%04x]= %08x\n", addr, result);
	return result;
}

void system_gdc_write_32(u32 addr, u32 data)
{
	if (!p_hw_base) {
		gdc_log(LOG_ERR, "Failed to write %d to addr %d\n", data, addr);
		return;
	}

	void *ptr = (void *)(p_hw_base + addr);

	iowrite32(data, ptr);
	gdc_log(LOG_DEBUG, "w [0x%04x]= %08x\n", addr, data);
}
