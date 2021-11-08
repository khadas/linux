// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/io.h>
#include <linux/of_address.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include "ts_output_reg.h"
#include "dsc_reg.h"
#include "demod_reg.h"
#include "mem_desc_reg.h"
#include "../key_reg.h"
#include "../dmx_log.h"

#define pr_verify(fmt, args...)   \
	dprintk(LOG_VER, debug_rw, "reg:" fmt, ## args)
#define pr_dbg(fmt, args...)      \
	dprintk(LOG_DBG, debug_rw, "reg:" fmt, ## args)
#define dprint(fmt, args...)     \
	dprintk(LOG_ERROR, debug_rw, "reg:" fmt, ## args)

MODULE_PARM_DESC(debug_rw, "\n\t\t Enable rw information");
static int debug_rw;
module_param(debug_rw, int, 0644);

static void *p_hw_base;
/*1:t5w chip*/
static int chip_flag;

void aml_write_self(unsigned int reg, unsigned int val)
{
	void *ptr = (void *)(p_hw_base + reg);

	pr_dbg("write addr:%lx, value:0x%0x\n", (unsigned long)ptr, val);
	writel(val, ptr);
	if (debug_rw & 0x2) {
		int value;

		if (reg == PID_RDY ||
		    reg == TSN_PID_READY ||
		    reg == TSD_PID_READY ||
		    reg == TSE_PID_READY ||
		    reg == KT_REE_RDY ||
		    (reg >= TS_DMA_RCH_READY(0) &&
		     reg <= TS_DMA_RCH_READY(31)) ||
		    ((reg >= TS_DMA_WCH_READY(0) &&
		      reg <= TS_DMA_WCH_READY(127))))
			return;
		value = readl(ptr);
		pr_verify("write addr:%lx, org v:0x%0x, ret v:0x%0x\n",
			  (unsigned long)ptr, val, value);
	}
}

int aml_read_self(unsigned int reg)
{
	void *addr = p_hw_base + reg;
	int ret = readl(addr);

	pr_dbg("read addr:%lx, value:0x%0x\n", (unsigned long)addr, ret);
	return ret;
}

int init_demux_addr(struct platform_device *pdev)
{
	struct resource *res;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dprint("%s fail\n", __func__);
		return -1;
	}

	dprint("%s test addr = %lx\n", __func__,
		       (unsigned long)res->start);

	/*this is T5W base address */
	if (res->start == 0xff610000)
		chip_flag = 1;

	p_hw_base = devm_ioremap_nocache(&pdev->dev, res->start,
					 resource_size(res));
	if (p_hw_base) {
		if (chip_flag != 1)
			p_hw_base += 0x440000;
		dprint("%s base addr = %lx\n", __func__,
		       (unsigned long)p_hw_base);
	} else {
		dprint("%s base addr error\n", __func__);
	}
	return 0;
}

int get_chip_type(void)
{
	return chip_flag;
}
