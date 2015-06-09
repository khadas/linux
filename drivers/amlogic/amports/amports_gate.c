/*
 * drivers/amlogic/amports/amports_gate.c
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

#include <linux/reset.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include "amports_gate.h"
#include "vdec_reg.h"
#include "amports_priv.h"

#define DEBUG_REF 0
#define GATE_RESET_OK

struct gate_swtch_node {
	struct reset_control *reset_ctl;
	const char *name;
	spinlock_t lock;
	unsigned long flags;
	int ref_count;
};
#ifdef GATE_RESET_OK

struct gate_swtch_node gates[] = {
	{
		.name = "demux",
	},
	{
		.name = "parser_top",
	},
	{
		.name = "vpu_intr",
	},
	{
		.name = "vdec",
	},
};

/*
resets = <&clock GCLK_IDX_HIU_PARSER_TOP
	    &clock GCLK_IDX_VPU_INTR
	    &clock GCLK_IDX_DEMUX
	    &clock GCLK_IDX_DOS>;
resets-names = "parser_top",
		"vpu_intr",
		"demux",
		"vdec";
*/

int amports_clock_gate_init(struct device *dev)
{
	int i;
	for (i = 0; i < sizeof(gates) / sizeof(struct gate_swtch_node); i++) {
		gates[i].reset_ctl = devm_reset_control_get(
			dev, gates[i].name);
		if (IS_ERR_OR_NULL(gates[i].reset_ctl)) {
			gates[i].reset_ctl = NULL;
			pr_info("get gate %s control failed %p\n",
				gates[i].name,
				gates[i].reset_ctl);
		} else {
			pr_info("get gate %s control ok %p\n",
				gates[i].name,
				gates[i].reset_ctl);
		}
		gates[i].ref_count = 0;
		spin_lock_init(&gates[i].lock);
	}
	return 0;
}

static int amports_gate_reset(struct gate_swtch_node *gate_node, int enable)
{
	spin_lock_irqsave(&gate_node->lock, gate_node->flags);
	if (enable) {
		if (DEBUG_REF)
			pr_info("amports_gate_reset,count: %d\n",
				gate_node->ref_count);
		if (gate_node->ref_count == 0)
			reset_control_deassert(
				gate_node->reset_ctl);
		gate_node->ref_count++;
	} else {
		gate_node->ref_count--;
		if (DEBUG_REF)
			pr_info("amports_gate_reset,count: %d\n",
				gate_node->ref_count);

		if (gate_node->ref_count == 0)
			reset_control_assert(
				gate_node->reset_ctl);
	}
	spin_unlock_irqrestore(&gate_node->lock, gate_node->flags);
	return 0;
}

int amports_switch_gate(const char *name, int enable)
{
	int i;
	for (i = 0; i < sizeof(gates) / sizeof(struct gate_swtch_node); i++) {
		if (!strcmp(name, gates[i].name)) {
			/*pr_info("openclose:%d gate %s control\n", enable,
				   gates[i].name);
			*/
			if (gates[i].reset_ctl)
				amports_gate_reset(&gates[i], enable);
		}
	}
	return 0;
}
#else
/*
can used for debug.
on chip bringup.
*/
int amports_clock_gate_init(struct device *dev)
{
	static int gate_inited;
	if (gate_inited)
		return 0;
/*
#define HHI_GCLK_MPEG0    0x1050
#define HHI_GCLK_MPEG1    0x1051
#define HHI_GCLK_MPEG2    0x1052
#define HHI_GCLK_OTHER    0x1054
#define HHI_GCLK_AO       0x1055
*/
	WRITE_HHI_REG_BITS(HHI_GCLK_MPEG0, 1, 1, 1);/*dos*/
	WRITE_HHI_REG_BITS(HHI_GCLK_MPEG1, 1, 25, 1);/*U_parser_top()*/
	WRITE_HHI_REG_BITS(HHI_GCLK_MPEG1, 0xff, 6, 8);/*aiu()*/
	WRITE_HHI_REG_BITS(HHI_GCLK_MPEG1, 1, 4, 1);/*demux()*/
	WRITE_HHI_REG_BITS(HHI_GCLK_MPEG1, 1, 2, 1);/*audio in()*/
	WRITE_HHI_REG_BITS(HHI_GCLK_MPEG2, 1, 25, 1);/*VPU Interrupt*/
	gate_inited++;



	return 0;
}

static int amports_gate_reset(struct gate_swtch_node *gate_node, int enable)
{
	return 0;
}

int amports_switch_gate(const char *name, int enable)
{
	amports_gate_reset(0, 0);
	return 0;
}

#endif
