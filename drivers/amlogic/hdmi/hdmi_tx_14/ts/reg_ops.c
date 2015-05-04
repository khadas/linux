/*
 * drivers/amlogic/hdmi/hdmi_tx_14/ts/reg_ops.c
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

#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/major.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/amlogic/iomap.h>

#include "mach_reg.h"
#include "hdmi_tx_reg.h"

static DEFINE_SPINLOCK(reg_lock);
static DEFINE_SPINLOCK(reg_lock2);

/*
 * RePacket HDMI related registers rd/wr
 */
unsigned int hd_read_reg(unsigned int addr)
{
	int ret = 0;
	unsigned int val = 0;
	unsigned int type = (addr >> OFFSET);
	unsigned int reg = addr & ((1 << OFFSET) - 1);

	ret = aml_reg_read(type, reg, &val);

	if (ret < 0)
		pr_info("Rd[0x%x] Error\n", addr);
	return val;
}

void hd_write_reg(unsigned int addr, unsigned int val)
{
	int ret = 0;
	unsigned int type = (addr >> OFFSET);
	unsigned int reg = addr & ((1 << OFFSET) - 1);

	ret = aml_reg_write(type, reg, val);

	if (ret < 0)
		pr_info("Wr[0x%x]0x%x Error\n", addr, val);
}

/* if the following bits are 0,
 * then access HDMI IP Port will cause system hungup
 */
#define GATE_NUM    2
static struct Hdmi_Gate hdmi_gate[GATE_NUM] = {
	{ P_HHI_HDMI_CLK_CNTL, 8 },
	{ P_HHI_GCLK_MPEG2, 4 },
};


/* In order to prevent system hangup,
 * add check_cts_hdmi_sys_clk_status() to check
 */
static void check_cts_hdmi_sys_clk_status(void)
{
	int i;
	unsigned int val;

	for (i = 0; i < GATE_NUM; i++) {
		val = hd_read_reg(hdmi_gate[i].cbus_addr);
		if (!(val & (1 << hdmi_gate[i].gate_bit))) {
			hd_set_reg_bits(hdmi_gate[i].cbus_addr, 1,
				hdmi_gate[i].gate_bit, 1);
		}
	}
}

unsigned int hdmi_rd_reg(unsigned int addr)
{
	unsigned int data = 0;

	unsigned long flags, fiq_flag;

	spin_lock_irqsave(&reg_lock, flags);
	raw_local_save_flags(fiq_flag);
	local_fiq_disable();

	check_cts_hdmi_sys_clk_status();
	hd_write_reg(P_HDMI_ADDR_PORT, addr);
	hd_write_reg(P_HDMI_ADDR_PORT, addr);
	data = hd_read_reg(P_HDMI_DATA_PORT);

	raw_local_irq_restore(fiq_flag);
	spin_unlock_irqrestore(&reg_lock, flags);
	return data;
}

void hdmi_wr_reg(unsigned int addr, unsigned int data)
{
	unsigned long flags, fiq_flag;
	spin_lock_irqsave(&reg_lock, flags);
	raw_local_save_flags(fiq_flag);
	local_fiq_disable();

	check_cts_hdmi_sys_clk_status();
	hd_write_reg(P_HDMI_ADDR_PORT, addr);
	hd_write_reg(P_HDMI_ADDR_PORT, addr);
	hd_write_reg(P_HDMI_DATA_PORT, data);
	raw_local_irq_restore(fiq_flag);
	spin_unlock_irqrestore(&reg_lock, flags);
}

#define waiting_aocec_free() \
	do {\
		unsigned long cnt = 0;\
		while (hd_read_reg(P_AO_CEC_RW_REG) & (1<<23)) {\
			if (3500 == cnt++) { \
				pr_info("waiting aocec free time out.\n");\
				break;\
			} \
		} \
	} while (0)

unsigned long aocec_rd_reg(unsigned long addr)
{
	unsigned long data32;
	unsigned long flags;
	waiting_aocec_free();
	spin_lock_irqsave(&reg_lock2, flags);
	data32 = 0;
	data32 |= 0 << 16; /* [16]     cec_reg_wr */
	data32 |= 0 << 8; /* [15:8]   cec_reg_wrdata */
	data32 |= addr << 0; /* [7:0]    cec_reg_addr */
	hd_write_reg(P_AO_CEC_RW_REG, data32);

	waiting_aocec_free();
	data32 = ((hd_read_reg(P_AO_CEC_RW_REG)) >> 24) & 0xff;
	spin_unlock_irqrestore(&reg_lock2, flags);
	return data32;
} /* aocec_rd_reg */

void aocec_wr_reg(unsigned long addr, unsigned long data)
{
	unsigned long data32;
	unsigned long flags;
	waiting_aocec_free();
	spin_lock_irqsave(&reg_lock2, flags);
	data32 = 0;
	data32 |= 1 << 16; /* [16]     cec_reg_wr */
	data32 |= data << 8; /* [15:8]   cec_reg_wrdata */
	data32 |= addr << 0; /* [7:0]    cec_reg_addr */
	hd_write_reg(P_AO_CEC_RW_REG, data32);
	spin_unlock_irqrestore(&reg_lock2, flags);
} /* aocec_wr_only_reg */
