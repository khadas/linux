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

#define DEBUG_REF 0
struct gate_swtch_node {
	struct reset_control *reset_ctl;
	const char *name;
	spinlock_t lock;
	unsigned long flags;
	int ref_count;
};
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
	{
		.name = "audio",
	}
};

/*
resets = <&clock GCLK_IDX_HIU_PARSER_TOP
	    &clock GCLK_IDX_VPU_INTR
	    &clock GCLK_IDX_DEMUX
	    &clock GCLK_IDX_DOS
	    &clock GCLK_IDX_MEDIA_CPU>;
resets-names = "parser_top",
		"vpu_intr",
		"demux",
		"vdec",
		"audio";
*/

int amports_clock_gate_init(struct device *dev)
{
	int i;
	for (i = 0; i < sizeof(gates) / sizeof(struct gate_swtch_node); i++) {
		pr_info("try get gate %s\n", gates[i].name);
		gates[i].reset_ctl = devm_reset_control_get(
			dev, gates[i].name);
		if (IS_ERR_OR_NULL(gates[i].reset_ctl))
			gates[i].reset_ctl = NULL;
		else {
			pr_info("get gate %s control ok %p\n", gates[i].name,
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
			pr_info("openclose:%d gate %s control\n", enable,
				   gates[i].name);
			if (gates[i].reset_ctl)
				amports_gate_reset(&gates[i], enable);
		}
	}
	return 0;
}
