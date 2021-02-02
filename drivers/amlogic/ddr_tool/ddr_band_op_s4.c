// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/sysfs.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include "ddr_bandwidth.h"
#include <linux/io.h>
#include <linux/slab.h>

#define DMC_MON_CTRL0			((0x0020  << 2))
#define DMC_MON_TIMER			((0x0021  << 2))
#define DMC_MON_ALL_IDLE_CNT		((0x0022  << 2))
#define DMC_MON_ALL_BW			((0x0023  << 2))
#define DMC_MON_ALL16_BW		((0x0024  << 2))

#define DMC_MON0_CTRL			((0x0025  << 2))
#define DMC_MON0_CTRL1			((0x0026  << 2))
#define DMC_MON0_CTRL2			((0x0027  << 2))
#define DMC_MON0_BW			((0x0028  << 2))

#define DMC_MON1_CTRL			((0x0029  << 2))
#define DMC_MON1_CTRL1			((0x002a  << 2))
#define DMC_MON1_CTRL2			((0x002b  << 2))
#define DMC_MON1_BW			((0x002c  << 2))

#define DMC_MON2_CTRL			((0x002d  << 2))
#define DMC_MON2_CTRL1			((0x002e  << 2))
#define DMC_MON2_CTRL2			((0x002f  << 2))
#define DMC_MON2_BW			((0x0030  << 2))

#define DMC_MON3_CTRL			((0x0031  << 2))
#define DMC_MON3_CTRL1			((0x0032  << 2))
#define DMC_MON3_CTRL2			((0x0033  << 2))
#define DMC_MON3_BW			((0x0034  << 2))

#define DMC_MON4_CTRL			((0x0035  << 2))
#define DMC_MON4_CTRL1			((0x0036  << 2))
#define DMC_MON4_CTRL2			((0x0037  << 2))
#define DMC_MON4_BW			((0x0038  << 2))

#define DMC_MON5_CTRL			((0x0039  << 2))
#define DMC_MON5_CTRL1			((0x003a  << 2))
#define DMC_MON5_CTRL2			((0x003b  << 2))
#define DMC_MON5_BW			((0x003c  << 2))

#define DMC_MON6_CTRL			((0x003d  << 2))
#define DMC_MON6_CTRL1			((0x003e  << 2))
#define DMC_MON6_CTRL2			((0x003f  << 2))
#define DMC_MON6_BW			((0x0040  << 2))

#define DMC_MON7_CTRL			((0x0041  << 2))
#define DMC_MON7_CTRL1			((0x0042  << 2))
#define DMC_MON7_CTRL2			((0x0043  << 2))
#define DMC_MON7_BW			((0x0044  << 2))

static void s4_dmc_port_config(struct ddr_bandwidth *db, int channel, int port)
{
	unsigned int val;
	unsigned int off = 0;
	int subport = -1;

	if (channel < 4)
		off = channel * 16 + DMC_MON0_CTRL;
	else
		off = (channel - 4) * 16 + DMC_MON4_CTRL;

	/* clear all port mask */
	if (port < 0) {
		writel(0, db->ddr_reg1 + off + 4);	/* DMC_MON*_CTRL1 */
		writel(0, db->ddr_reg1 + off + 8);	/* DMC_MON*_CTRL2 */
		return;
	}

	if (port >= PORT_MAJOR)
		subport = port - PORT_MAJOR;

	if (subport < 0) {
		val = readl(db->ddr_reg1 + off + 4);
		val |=  (1 << port);
		writel(val, db->ddr_reg1 + off + 4);	/* DMC_MON*_CTRL1 */
		val = 0xffff;
		writel(val, db->ddr_reg1 + off + 8);	/* DMC_MON*_CTRL2 */
	} else {
		val = (0x1 << 23);	/* select device */
		writel(val, db->ddr_reg1 + off + 4);
		val = readl(db->ddr_reg1 + off + 8);
		val |= (1 << subport);
		writel(val, db->ddr_reg1 + off + 8);
	}
}

static void s4_dmc_range_config(struct ddr_bandwidth *db, int channel,
				unsigned long start, unsigned long end)
{
	unsigned int val;
	unsigned int off = 0;

	if (channel < 4)
		off = channel * 16 + DMC_MON0_CTRL;
	else
		off = (channel - 4) * 16 + DMC_MON4_CTRL;

	val = (start >> 16) | (end & 0xffff0000);
	writel(val, db->ddr_reg1 + off);			/* DMC_MON*_CTRL */
}

static unsigned long s4_get_dmc_freq_quick(struct ddr_bandwidth *db)
{
	unsigned int val;
	unsigned int n, m, od1;
	unsigned int od_div = 0xfff;
	unsigned long freq = 0;

	val = readl(db->pll_reg);
	val = val & 0xfffff;
	switch ((val >> 16) & 7) {
	case 0:
		od_div = 2;
		break;

	case 1:
		od_div = 3;
		break;

	case 2:
		od_div = 4;
		break;

	case 3:
		od_div = 6;
		break;

	case 4:
		od_div = 8;
		break;

	default:
		break;
	}

	m = val & 0x1ff;
	n = ((val >> 10) & 0x1f);
	od1 = (((val >> 19) & 0x1)) == 1 ? 2 : 1;
	freq = DEFAULT_XTAL_FREQ / 1000;	/* avoid overflow */
	if (n)
		freq = ((((freq * m) / n) >> od1) / od_div) * 1000;

	return freq;
}

static void s4_dmc_bandwidth_enable(struct ddr_bandwidth *db)
{
	unsigned int val;

	/* enable all channel */
	val =  (0x01 << 31) |	/* enable bit */
	       (0x01 << 20) |	/* use timer  */
	       (0xff <<  0);
	writel(val, db->ddr_reg1 + DMC_MON_CTRL0);
}

static void s4_dmc_bandwidth_init(struct ddr_bandwidth *db)
{
	unsigned int i;
	/* set timer trigger clock_cnt */
	writel(db->clock_count, db->ddr_reg1 + DMC_MON_TIMER);
	s4_dmc_bandwidth_enable(db);

	for (i = 0; i < db->channels; i++) {
		if (!db->port[i])
			s4_dmc_port_config(db, i, -1);
		s4_dmc_range_config(db, i, 0, 0xffffffff);
		db->range[i].start = 0;
		db->range[i].end   = 0xffffffff;
	}
}

static int s4_handle_irq(struct ddr_bandwidth *db, struct ddr_grant *dg)
{
	unsigned int i, val, off;
	int ret = -1;

	val = readl(db->ddr_reg1 + DMC_MON_CTRL0);
	if (val & DMC_QOS_IRQ) {
		/*
		 * get total bytes by each channel, each cycle 16 bytes;
		 */
		dg->all_grant    = readl(db->ddr_reg1 + DMC_MON_ALL_BW);
		dg->all_grant16  = readl(db->ddr_reg1 + DMC_MON_ALL16_BW);
		dg->all_grant   *= 16;
		dg->all_grant16 *= 16;
		for (i = 0; i < db->channels; i++) {
			if (i < 4)
				off = i * 16 + DMC_MON0_BW;
			else
				off = (i - 4) * 16 + DMC_MON4_BW;
			dg->channel_grant[i] = readl(db->ddr_reg1 + off) * 16;
		}
		/* clear irq flags */
		writel(val, db->ddr_reg1 + DMC_MON_CTRL0);
		s4_dmc_bandwidth_enable(db);

		ret = 0;
	}
	return ret;
}

#if DDR_BANDWIDTH_DEBUG
static int s4_dump_reg(struct ddr_bandwidth *db, char *buf)
{
	int s = 0, i;
	unsigned int r, off;

	r  = readl(db->ddr_reg1 + DMC_MON_CTRL0);
	s += sprintf(buf + s, "DMC_MON_CTRL0:        %08x\n", r);
	r  = readl(db->ddr_reg1 + DMC_MON_TIMER);
	s += sprintf(buf + s, "DMC_MON_TIMER:        %08x\n", r);
	r  = readl(db->ddr_reg1 + DMC_MON_ALL_IDLE_CNT);
	s += sprintf(buf + s, "DMC_MON_ALL_IDLE_CNT: %08x\n", r);
	r  = readl(db->ddr_reg1 + DMC_MON_ALL_BW);
	s += sprintf(buf + s, "DMC_MON_ALL_BW:       %08x\n", r);
	r  = readl(db->ddr_reg1 + DMC_MON_ALL16_BW);
	s += sprintf(buf + s, "DMC_MON_ALL16_BW:     %08x\n", r);

	for (i = 0; i < 8; i++) {
		if (i < 4)
			off = i * 16 + DMC_MON0_CTRL;
		else
			off = (i - 4) * 16 + DMC_MON4_CTRL;

		r  = readl(db->ddr_reg1 + off);
		s += sprintf(buf + s, "DMC_MON%d_CTRL:        %08x\n", i, r);
		r  = readl(db->ddr_reg1 + off + 4);
		s += sprintf(buf + s, "DMC_MON%d_CTRL1:       %08x\n", i, r);
		r  = readl(db->ddr_reg1 + off + 8);
		s += sprintf(buf + s, "DMC_MON%d_CTRL2:       %08x\n", i, r);
		r  = readl(db->ddr_reg1 + off + 12);
		s += sprintf(buf + s, "DMC_MON%d_BW:          %08x\n", i, r);
	}
	return s;
}
#endif

struct ddr_bandwidth_ops s4_ddr_bw_ops = {
	.init             = s4_dmc_bandwidth_init,
	.config_port      = s4_dmc_port_config,
	.config_range     = s4_dmc_range_config,
	.get_freq         = s4_get_dmc_freq_quick,
	.handle_irq       = s4_handle_irq,
	.bandwidth_enable = s4_dmc_bandwidth_enable,
#if DDR_BANDWIDTH_DEBUG
	.dump_reg         = s4_dump_reg,
#endif
};
