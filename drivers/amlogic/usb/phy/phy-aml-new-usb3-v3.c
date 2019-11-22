/*
 * drivers/amlogic/usb/phy/phy-aml-new-usb3-v3.c
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

#include <linux/module.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/irqreturn.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/pm_runtime.h>
#include <linux/delay.h>
#include <linux/usb/phy.h>
#include <linux/amlogic/usb-v2.h>
#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/workqueue.h>
#include <linux/notifier.h>
#include <linux/amlogic/usbtype.h>
#include <linux/amlogic/power_ctrl.h>
#include "phy-aml-new-usb-v2.h"

#define HOST_MODE	0
#define DEVICE_MODE	1

struct usb_aml_regs_v2 usb_new_aml_regs_v3;

static int amlogic_new_usb3_suspend(struct usb_phy *x, int suspend)
{
	return 0;
}

static void amlogic_new_usb3phy_shutdown(struct usb_phy *x)
{
	struct amlogic_usb_v2 *phy = phy_to_amlusb(x);

	if (phy->phy.flags == AML_USB3_PHY_ENABLE) {
		clk_disable_unprepare(phy->clk);
		clk_disable_unprepare(phy->gate1_clk);
		clk_disable_unprepare(phy->gate0_clk);
	}

	phy->suspend_flag = 1;
}

static void cr_bus_addr(struct amlogic_usb_v2 *phy_v3, unsigned int addr)
{
	union phy3_r4 phy_r4 = {.d32 = 0};
	union phy3_r5 phy_r5 = {.d32 = 0};
	unsigned long timeout_jiffies;

	phy_r4.b.phy_cr_data_in = addr;
	writel(phy_r4.d32, phy_v3->phy3_cfg_r4);

	phy_r4.b.phy_cr_cap_addr = 0;
	writel(phy_r4.d32, phy_v3->phy3_cfg_r4);
	phy_r4.b.phy_cr_cap_addr = 1;
	writel(phy_r4.d32, phy_v3->phy3_cfg_r4);
	timeout_jiffies = jiffies +
			msecs_to_jiffies(1000);
	do {
		phy_r5.d32 = readl(phy_v3->phy3_cfg_r5);
	} while (phy_r5.b.phy_cr_ack == 0 &&
		time_is_after_jiffies(timeout_jiffies));

	phy_r4.b.phy_cr_cap_addr = 0;
	writel(phy_r4.d32, phy_v3->phy3_cfg_r4);
	timeout_jiffies = jiffies +
			msecs_to_jiffies(1000);
	do {
		phy_r5.d32 = readl(phy_v3->phy3_cfg_r5);
	} while (phy_r5.b.phy_cr_ack == 1 &&
		time_is_after_jiffies(timeout_jiffies));
}

static int cr_bus_read(struct amlogic_usb_v2 *phy_v3, unsigned int addr)
{
	int data;
	union phy3_r4 phy_r4 = {.d32 = 0};
	union phy3_r5 phy_r5 = {.d32 = 0};
	unsigned long timeout_jiffies;

	cr_bus_addr(phy_v3, addr);

	phy_r4.b.phy_cr_read = 0;
	writel(phy_r4.d32, phy_v3->phy3_cfg_r4);
	phy_r4.b.phy_cr_read = 1;
	writel(phy_r4.d32, phy_v3->phy3_cfg_r4);

	timeout_jiffies = jiffies +
			msecs_to_jiffies(1000);
	do {
		phy_r5.d32 = readl(phy_v3->phy3_cfg_r5);
	} while (phy_r5.b.phy_cr_ack == 0 &&
		time_is_after_jiffies(timeout_jiffies));

	data = phy_r5.b.phy_cr_data_out;

	phy_r4.b.phy_cr_read = 0;
	writel(phy_r4.d32, phy_v3->phy3_cfg_r4);
	timeout_jiffies = jiffies +
			msecs_to_jiffies(1000);
	do {
		phy_r5.d32 = readl(phy_v3->phy3_cfg_r5);
	} while (phy_r5.b.phy_cr_ack == 1 &&
		time_is_after_jiffies(timeout_jiffies));

	return data;
}

static void cr_bus_write(struct amlogic_usb_v2 *phy_v3,
	unsigned int addr, unsigned int data)
{
	union phy3_r4 phy_r4 = {.d32 = 0};
	union phy3_r5 phy_r5 = {.d32 = 0};
	unsigned long timeout_jiffies;

	cr_bus_addr(phy_v3, addr);

	phy_r4.b.phy_cr_data_in = data;
	writel(phy_r4.d32, phy_v3->phy3_cfg_r4);

	phy_r4.b.phy_cr_cap_data = 0;
	writel(phy_r4.d32, phy_v3->phy3_cfg_r4);
	phy_r4.b.phy_cr_cap_data = 1;
	writel(phy_r4.d32, phy_v3->phy3_cfg_r4);
	timeout_jiffies = jiffies +
		msecs_to_jiffies(1000);
	do {
		phy_r5.d32 = readl(phy_v3->phy3_cfg_r5);
	} while (phy_r5.b.phy_cr_ack == 0 &&
		time_is_after_jiffies(timeout_jiffies));

	phy_r4.b.phy_cr_cap_data = 0;
	writel(phy_r4.d32, phy_v3->phy3_cfg_r4);
	timeout_jiffies = jiffies +
		msecs_to_jiffies(1000);
	do {
		phy_r5.d32 = readl(phy_v3->phy3_cfg_r5);
	} while (phy_r5.b.phy_cr_ack == 1 &&
		time_is_after_jiffies(timeout_jiffies));

	phy_r4.b.phy_cr_write = 0;
	writel(phy_r4.d32, phy_v3->phy3_cfg_r4);
	phy_r4.b.phy_cr_write = 1;
	writel(phy_r4.d32, phy_v3->phy3_cfg_r4);
	timeout_jiffies = jiffies +
		msecs_to_jiffies(1000);
	do {
		phy_r5.d32 = readl(phy_v3->phy3_cfg_r5);
	} while (phy_r5.b.phy_cr_ack == 0 &&
		time_is_after_jiffies(timeout_jiffies));

	phy_r4.b.phy_cr_write = 0;
	writel(phy_r4.d32, phy_v3->phy3_cfg_r4);
	timeout_jiffies = jiffies +
		msecs_to_jiffies(1000);
	do {
		phy_r5.d32 = readl(phy_v3->phy3_cfg_r5);
	} while (phy_r5.b.phy_cr_ack == 1 &&
		time_is_after_jiffies(timeout_jiffies));
}

static void cr_bus_addr_31(struct amlogic_usb_v2 *phy_v3, unsigned int addr)
{
	union phy3_r4 phy_r4 = {.d32 = 0};
	union phy3_r5 phy_r5 = {.d32 = 0};
	unsigned long timeout_jiffies;

	phy_r4.b.phy_cr_data_in = addr;
	writel(phy_r4.d32, phy_v3->phy31_cfg_r4);

	phy_r4.b.phy_cr_cap_addr = 0;
	writel(phy_r4.d32, phy_v3->phy31_cfg_r4);
	phy_r4.b.phy_cr_cap_addr = 1;
	writel(phy_r4.d32, phy_v3->phy31_cfg_r4);
	timeout_jiffies = jiffies +
			msecs_to_jiffies(1000);
	do {
		phy_r5.d32 = readl(phy_v3->phy31_cfg_r5);
	} while (phy_r5.b.phy_cr_ack == 0 &&
		time_is_after_jiffies(timeout_jiffies));

	phy_r4.b.phy_cr_cap_addr = 0;
	writel(phy_r4.d32, phy_v3->phy31_cfg_r4);
	timeout_jiffies = jiffies +
			msecs_to_jiffies(1000);
	do {
		phy_r5.d32 = readl(phy_v3->phy31_cfg_r5);
	} while (phy_r5.b.phy_cr_ack == 1 &&
		time_is_after_jiffies(timeout_jiffies));
}

static int cr_bus_read_31(struct amlogic_usb_v2 *phy_v3, unsigned int addr)
{
	int data;
	union phy3_r4 phy_r4 = {.d32 = 0};
	union phy3_r5 phy_r5 = {.d32 = 0};
	unsigned long timeout_jiffies;

	cr_bus_addr_31(phy_v3, addr);

	phy_r4.b.phy_cr_read = 0;
	writel(phy_r4.d32, phy_v3->phy31_cfg_r4);
	phy_r4.b.phy_cr_read = 1;
	writel(phy_r4.d32, phy_v3->phy31_cfg_r4);

	timeout_jiffies = jiffies +
			msecs_to_jiffies(1000);
	do {
		phy_r5.d32 = readl(phy_v3->phy31_cfg_r5);
	} while (phy_r5.b.phy_cr_ack == 0 &&
		time_is_after_jiffies(timeout_jiffies));

	data = phy_r5.b.phy_cr_data_out;

	phy_r4.b.phy_cr_read = 0;
	writel(phy_r4.d32, phy_v3->phy31_cfg_r4);
	timeout_jiffies = jiffies +
			msecs_to_jiffies(1000);
	do {
		phy_r5.d32 = readl(phy_v3->phy31_cfg_r5);
	} while (phy_r5.b.phy_cr_ack == 1 &&
		time_is_after_jiffies(timeout_jiffies));

	return data;
}

static void cr_bus_write_31(struct amlogic_usb_v2 *phy_v3,
	unsigned int addr, unsigned int data)
{
	union phy3_r4 phy_r4 = {.d32 = 0};
	union phy3_r5 phy_r5 = {.d32 = 0};
	unsigned long timeout_jiffies;

	cr_bus_addr_31(phy_v3, addr);

	phy_r4.b.phy_cr_data_in = data;
	writel(phy_r4.d32, phy_v3->phy31_cfg_r4);

	phy_r4.b.phy_cr_cap_data = 0;
	writel(phy_r4.d32, phy_v3->phy31_cfg_r4);
	phy_r4.b.phy_cr_cap_data = 1;
	writel(phy_r4.d32, phy_v3->phy31_cfg_r4);
	timeout_jiffies = jiffies +
		msecs_to_jiffies(1000);
	do {
		phy_r5.d32 = readl(phy_v3->phy31_cfg_r5);
	} while (phy_r5.b.phy_cr_ack == 0 &&
		time_is_after_jiffies(timeout_jiffies));

	phy_r4.b.phy_cr_cap_data = 0;
	writel(phy_r4.d32, phy_v3->phy31_cfg_r4);
	timeout_jiffies = jiffies +
		msecs_to_jiffies(1000);
	do {
		phy_r5.d32 = readl(phy_v3->phy31_cfg_r5);
	} while (phy_r5.b.phy_cr_ack == 1 &&
		time_is_after_jiffies(timeout_jiffies));

	phy_r4.b.phy_cr_write = 0;
	writel(phy_r4.d32, phy_v3->phy31_cfg_r4);
	phy_r4.b.phy_cr_write = 1;
	writel(phy_r4.d32, phy_v3->phy31_cfg_r4);
	timeout_jiffies = jiffies +
		msecs_to_jiffies(1000);
	do {
		phy_r5.d32 = readl(phy_v3->phy31_cfg_r5);
	} while (phy_r5.b.phy_cr_ack == 0 &&
		time_is_after_jiffies(timeout_jiffies));

	phy_r4.b.phy_cr_write = 0;
	writel(phy_r4.d32, phy_v3->phy31_cfg_r4);
	timeout_jiffies = jiffies +
		msecs_to_jiffies(1000);
	do {
		phy_r5.d32 = readl(phy_v3->phy31_cfg_r5);
	} while (phy_r5.b.phy_cr_ack == 1 &&
		time_is_after_jiffies(timeout_jiffies));
}

static void usb3_phy_cr_config_30(struct amlogic_usb_v2 *phy)
{
	u32 data = 0;

	/*
	 * WORKAROUND: There is SSPHY suspend bug due to
	 * which USB enumerates
	 * in HS mode instead of SS mode. Workaround it by asserting
	 * LANE0.TX_ALT_BLOCK.EN_ALT_BUS to enable TX to use alt bus
	 * mode
	 */
	data = cr_bus_read(phy, 0x102d);
	data |= (1 << 7);
	cr_bus_write(phy, 0x102D, data);

	data = cr_bus_read(phy, 0x1010);
	data &= ~0xff0;
	data |= 0x20;
	cr_bus_write(phy, 0x1010, data);

	/*
	 * Fix RX Equalization setting as follows
	 * LANE0.RX_OVRD_IN_HI. RX_EQ_EN set to 0
	 * LANE0.RX_OVRD_IN_HI.RX_EQ_EN_OVRD set to 1
	 * LANE0.RX_OVRD_IN_HI.RX_EQ set to 3
	 * LANE0.RX_OVRD_IN_HI.RX_EQ_OVRD set to 1
	 */
	data = cr_bus_read(phy, 0x1006);
	data &= ~(1 << 6);
	data |= (1 << 7);
	data &= ~(0x7 << 8);
	data |= (0x3 << 8);
	data |= (0x1 << 11);
	cr_bus_write(phy, 0x1006, data);

	/*
	 * S	et EQ and TX launch amplitudes as follows
	 * LANE0.TX_OVRD_DRV_LO.PREEMPH set to 22
	 * LANE0.TX_OVRD_DRV_LO.AMPLITUDE set to 127
	 * LANE0.TX_OVRD_DRV_LO.EN set to 1.
	 */
	data = cr_bus_read(phy, 0x1002);
	data &= ~0x3f80;
	data |= (0x16 << 7);
	data &= ~0x7f;
	data |= (0x7f | (1 << 14));
	cr_bus_write(phy, 0x1002, data);

	/*
	 * MPLL_LOOP_CTL.PROP_CNTRL
	 */
	data = cr_bus_read(phy, 0x30);
	data &= ~(0xf << 4);
	data |= (0x8 << 4);
	cr_bus_write(phy, 0x30, data);
	udelay(2);
}


static void usb3_phy_cr_config_31(struct amlogic_usb_v2 *phy)
{
	u32 data = 0;

	/*
	 * WORKAROUND: There is SSPHY suspend bug due to
	 * which USB enumerates
	 * in HS mode instead of SS mode. Workaround it by asserting
	 * LANE0.TX_ALT_BLOCK.EN_ALT_BUS to enable TX to use alt bus
	 * mode
	 */
	data = cr_bus_read_31(phy, 0x102d);
	data |= (1 << 7);
	cr_bus_write_31(phy, 0x102D, data);

	data = cr_bus_read_31(phy, 0x1010);
	data &= ~0xff0;
	data |= 0x20;
	cr_bus_write_31(phy, 0x1010, data);

	/*
	 * Fix RX Equalization setting as follows
	 * LANE0.RX_OVRD_IN_HI. RX_EQ_EN set to 0
	 * LANE0.RX_OVRD_IN_HI.RX_EQ_EN_OVRD set to 1
	 * LANE0.RX_OVRD_IN_HI.RX_EQ set to 3
	 * LANE0.RX_OVRD_IN_HI.RX_EQ_OVRD set to 1
	 */
	data = cr_bus_read_31(phy, 0x1006);
	data &= ~(1 << 6);
	data |= (1 << 7);
	data &= ~(0x7 << 8);
	data |= (0x3 << 8);
	data |= (0x1 << 11);
	cr_bus_write_31(phy, 0x1006, data);

	/*
	 * S	et EQ and TX launch amplitudes as follows
	 * LANE0.TX_OVRD_DRV_LO.PREEMPH set to 22
	 * LANE0.TX_OVRD_DRV_LO.AMPLITUDE set to 127
	 * LANE0.TX_OVRD_DRV_LO.EN set to 1.
	 */
	data = cr_bus_read_31(phy, 0x1002);
	data &= ~0x3f80;
	data |= (0x16 << 7);
	data &= ~0x7f;
	data |= (0x7f | (1 << 14));
	cr_bus_write_31(phy, 0x1002, data);

	/*
	 * MPLL_LOOP_CTL.PROP_CNTRL
	 */
	data = cr_bus_read_31(phy, 0x30);
	data &= ~(0xf << 4);
	data |= (0x8 << 4);
	cr_bus_write_31(phy, 0x30, data);
	udelay(2);
}


static int amlogic_new_usb3_init_v3(struct usb_phy *x)
{
	struct amlogic_usb_v2 *phy = phy_to_amlusb(x);
	union usb_r1_v2 r1 = {.d32 = 0};
	union usb_r2_v2 r2 = {.d32 = 0};
	union usb_r3_v2 r3 = {.d32 = 0};
	union usb_r7_v2 r7 = {.d32 = 0};
	union phy3_r2 p3_r2 = {.d32 = 0};
	union phy3_r1 p3_r1 = {.d32 = 0};
	int i = 0;

	if (phy->suspend_flag) {
		if (phy->phy.flags == AML_USB3_PHY_ENABLE) {
			clk_prepare_enable(phy->gate0_clk);
			clk_prepare_enable(phy->gate1_clk);
			clk_prepare_enable(phy->clk);
		}
		phy->suspend_flag = 0;
		return 0;
	}

	if (phy->phy.flags != AML_USB3_PHY_ENABLE)
		return 0;

	for (i = 0; i < 8; i++) {
		usb_new_aml_regs_v3.usb_r_v2[i] = (void __iomem *)
			((unsigned long)phy->regs + 4*i);
	}

	if (phy->portconfig_30) {
		/* config usb30 phy */
		r3.d32 = readl(usb_new_aml_regs_v3.usb_r_v2[3]);
		r3.b.p30_ssc_en = 1;
		r3.b.p30_ssc_range = 2;
		r3.b.p30_ref_ssp_en = 1;
		writel(r3.d32, usb_new_aml_regs_v3.usb_r_v2[3]);
		udelay(2);
		r2.d32 = readl(usb_new_aml_regs_v3.usb_r_v2[2]);
		r2.b.p30_pcs_tx_deemph_3p5db = 0x15;
		r2.b.p30_pcs_tx_deemph_6db = 0x20;
		writel(r2.d32, usb_new_aml_regs_v3.usb_r_v2[2]);
		udelay(2);
		r1.d32 = readl(usb_new_aml_regs_v3.usb_r_v2[1]);
		r1.b.u3h_host_port_power_control_present = 1;
		r1.b.u3h_fladj_30mhz_reg = 0x20;
		r1.b.p30_pcs_tx_swing_full = 127;
		r1.b.u3h_host_u3_port_disable = 0;
		writel(r1.d32, usb_new_aml_regs_v3.usb_r_v2[1]);
		udelay(2);
		p3_r2.d32 = readl(phy->phy3_cfg_r2);
		p3_r2.b.phy_tx_vboost_lvl = 0x4;
		writel(p3_r2.d32, phy->phy3_cfg_r2);
		udelay(2);

		usb3_phy_cr_config_30(phy);

		/*
		 * LOS_BIAS	to 0x5
		 * LOS_LEVEL to 0x9
		 */
		p3_r1.d32 = readl(phy->phy3_cfg_r1);
		p3_r1.b.phy_los_bias = 0x5;
		p3_r1.b.phy_los_level = 0x9;
		writel(p3_r1.d32, phy->phy3_cfg_r1);
	}

	if (phy->portconfig_31) {
		/* config usb31 phy */
		r7.d32 = readl(usb_new_aml_regs_v3.usb_r_v2[7]);
		r7.b.p31_ssc_en = 1;
		r7.b.p31_ssc_range = 2;
		r7.b.p31_ref_ssp_en = 1;
		writel(r7.d32, usb_new_aml_regs_v3.usb_r_v2[7]);
		udelay(2);
		r7.d32 = readl(usb_new_aml_regs_v3.usb_r_v2[7]);
		r7.b.p31_pcs_tx_deemph_6db = 0x20;
		r7.b.p31_pcs_tx_swing_full = 127;
		writel(r7.d32, usb_new_aml_regs_v3.usb_r_v2[7]);
		udelay(2);
		r3.d32 = readl(usb_new_aml_regs_v3.usb_r_v2[3]);
		r3.b.p31_pcs_tx_deemph_3p5db = 0x15;
		writel(r3.d32, usb_new_aml_regs_v3.usb_r_v2[3]);

		udelay(2);
		p3_r2.d32 = readl(phy->phy31_cfg_r2);
		p3_r2.b.phy_tx_vboost_lvl = 0x4;
		writel(p3_r2.d32, phy->phy31_cfg_r2);
		udelay(2);

		usb3_phy_cr_config_31(phy);

		/*
		 * LOS_BIAS	to 0x5
		 * LOS_LEVEL to 0x9
		 */
		p3_r1.d32 = readl(phy->phy31_cfg_r1);
		p3_r1.b.phy_los_bias = 0x5;
		p3_r1.b.phy_los_level = 0x9;
		writel(p3_r1.d32, phy->phy31_cfg_r1);
	}

	return 0;
}

static void power_switch_to_pcie(struct amlogic_usb_v2 *phy)
{
	u32 val;

	if (phy->portconfig_30) {
		power_ctrl_sleep(1, phy->u30_ctrl_sleep_shift);
		power_ctrl_mempd0(1, phy->u30_hhi_mem_pd_mask,
				phy->u30_hhi_mem_pd_shift);
		udelay(100);

		val = readl((void __iomem *)
			((unsigned long)phy->reset_regs + (0x20 * 4 - 0x8)));
		writel((val & (~(0x1 << phy->usb30_ctrl_rst_bit))),
			(void __iomem *)((unsigned long)phy->reset_regs +
			(0x20 * 4 - 0x8)));

		udelay(100);

		power_ctrl_iso(1, phy->u30_ctrl_iso_shift);

		val = readl((void __iomem *)
			((unsigned long)phy->reset_regs + (0x20 * 4 - 0x8)));

		writel((val | (0x1 << phy->usb30_ctrl_rst_bit)),
			(void __iomem	*)((unsigned long)phy->reset_regs +
			(0x20 * 4 - 0x8)));

		udelay(100);
	}

	if (phy->portconfig_31) {
		power_ctrl_sleep(1, phy->u31_ctrl_sleep_shift);
		power_ctrl_mempd0(1, phy->u31_hhi_mem_pd_mask,
			phy->u31_hhi_mem_pd_shift);
		udelay(100);

		val = readl((void __iomem *)
			((unsigned long)phy->reset_regs + (0x20 * 4 - 0x8)));
		writel((val & (~(0x1 << phy->usb31_ctrl_rst_bit))),
			(void __iomem *)((unsigned long)phy->reset_regs +
			(0x20 * 4 - 0x8)));

		udelay(100);

		power_ctrl_iso(1, phy->u31_ctrl_iso_shift);

		val = readl((void __iomem *)
			((unsigned long)phy->reset_regs + (0x20 * 4 - 0x8)));

		writel((val | (0x1 << phy->usb31_ctrl_rst_bit)),
			(void __iomem	*)((unsigned long)phy->reset_regs +
			(0x20 * 4 - 0x8)));

		udelay(100);
	}
}

static int amlogic_new_usb3_v3_probe(struct platform_device *pdev)
{
	struct amlogic_usb_v2			*phy;
	struct device *dev = &pdev->dev;
	struct resource *phy_mem;
	void __iomem	*phy_base;
	void __iomem *phy3_base;
	void __iomem *phy31_base;
	void __iomem	*reset_base = NULL;
	unsigned int phy3_mem;
	unsigned int phy3_mem_size = 0;
	unsigned int phy31_mem;
	unsigned int phy31_mem_size = 0;
	unsigned int reset_mem;
	unsigned int reset_mem_size = 0;

	const void *prop;
	int portnum = 0;
	int retval;
	int ret;
	u32 pwr_ctl = 0;
	u32 u30_ctrl_sleep_shift = 0;
	u32 u30_hhi_mem_pd_shift = 0;
	u32 u30_hhi_mem_pd_mask = 0;
	u32 u30_ctrl_iso_shift = 0;
	u32 usb30_ctrl_rst_bit = 0;
	u32 u31_ctrl_sleep_shift = 0;
	u32 u31_hhi_mem_pd_shift = 0;
	u32 u31_hhi_mem_pd_mask = 0;
	u32 u31_ctrl_iso_shift = 0;
	u32 usb31_ctrl_rst_bit = 0;
	u32 portconfig_30 = 0;
	u32 portconfig_31 = 0;
	unsigned long rate;
#define PCIE_PLL_RATE 100000000
	u32 val;

	prop = of_get_property(dev->of_node, "portnum", NULL);
	if (prop)
		portnum = of_read_ulong(prop, 1);

	if (!portnum)
		dev_err(&pdev->dev, "This phy has no usb port\n");

	phy_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	phy_base = devm_ioremap_resource(dev, phy_mem);
	if (IS_ERR(phy_base))
		return PTR_ERR(phy_base);

	retval = of_property_read_u32(dev->of_node, "phy0-reg", &phy3_mem);
	if (retval < 0)
		return -EINVAL;

	retval = of_property_read_u32
				(dev->of_node, "phy0-reg-size", &phy3_mem_size);
	if (retval < 0)
		return -EINVAL;

	phy3_base = devm_ioremap_nocache
				(&(pdev->dev), (resource_size_t)phy3_mem,
				(unsigned long)phy3_mem_size);
	if (!phy3_base)
		return -ENOMEM;

	retval = of_property_read_u32(dev->of_node, "phy1-reg", &phy31_mem);
	if (retval < 0)
		return -EINVAL;

	retval = of_property_read_u32
				(dev->of_node, "phy1-reg-size",
					&phy31_mem_size);
	if (retval < 0)
		return -EINVAL;

	phy31_base = devm_ioremap_nocache
				(&(pdev->dev), (resource_size_t)phy31_mem,
				(unsigned long)phy31_mem_size);
	if (!phy31_base)
		return -ENOMEM;

	prop = of_get_property(dev->of_node, "pwr-ctl", NULL);
	if (prop)
		pwr_ctl = of_read_ulong(prop, 1);
	else
		pwr_ctl = 0;

	if (pwr_ctl) {
		retval = of_property_read_u32(dev->of_node,
				"reset-reg", &reset_mem);
		if (retval < 0)
			return -EINVAL;

		retval = of_property_read_u32
				(dev->of_node, "reset-reg-size",
					&reset_mem_size);
		if (retval < 0)
			return -EINVAL;

		reset_base = devm_ioremap_nocache
				(&(pdev->dev), (resource_size_t)reset_mem,
				(unsigned long)reset_mem_size);
		if (!reset_base)
			return -ENOMEM;

		prop = of_get_property(dev->of_node,
			"u30-ctrl-sleep-shift", NULL);
		if (prop)
			u30_ctrl_sleep_shift = of_read_ulong(prop, 1);
		else
			pwr_ctl = 0;

		prop = of_get_property(dev->of_node,
			"u30-hhi-mem-pd-shift", NULL);
		if (prop)
			u30_hhi_mem_pd_shift = of_read_ulong(prop, 1);
		else
			pwr_ctl = 0;

		prop = of_get_property(dev->of_node,
			"u30-hhi-mem-pd-mask", NULL);
		if (prop)
			u30_hhi_mem_pd_mask = of_read_ulong(prop, 1);
		else
			pwr_ctl = 0;

		prop = of_get_property(dev->of_node,
			"u30-ctrl-iso-shift", NULL);
		if (prop)
			u30_ctrl_iso_shift = of_read_ulong(prop, 1);
		else
			pwr_ctl = 0;

		prop = of_get_property(dev->of_node,
			"usb30-ctrl-a-rst-bit", NULL);
		if (prop)
			usb30_ctrl_rst_bit = of_read_ulong(prop, 1);
		else
			pwr_ctl = 0;

		prop = of_get_property(dev->of_node,
			"u31-ctrl-sleep-shift", NULL);
		if (prop)
			u31_ctrl_sleep_shift = of_read_ulong(prop, 1);
		else
			pwr_ctl = 0;

		prop = of_get_property(dev->of_node,
			"u31-hhi-mem-pd-shift", NULL);
		if (prop)
			u31_hhi_mem_pd_shift = of_read_ulong(prop, 1);
		else
			pwr_ctl = 0;

		prop = of_get_property(dev->of_node,
			"u31-hhi-mem-pd-mask", NULL);
		if (prop)
			u31_hhi_mem_pd_mask = of_read_ulong(prop, 1);
		else
			pwr_ctl = 0;

		prop = of_get_property(dev->of_node,
			"u31-ctrl-iso-shift", NULL);
		if (prop)
			u31_ctrl_iso_shift = of_read_ulong(prop, 1);
		else
			pwr_ctl = 0;

		prop = of_get_property(dev->of_node,
			"usb31-ctrl-a-rst-bit", NULL);
		if (prop)
			usb31_ctrl_rst_bit = of_read_ulong(prop, 1);
		else
			pwr_ctl = 0;
	}

	prop = of_get_property(dev->of_node,
			"portconfig-30", NULL);
	if (prop)
		portconfig_30 = of_read_ulong(prop, 1);

	prop = of_get_property(dev->of_node,
			"portconfig-31", NULL);
	if (prop)
		portconfig_31 = of_read_ulong(prop, 1);

	phy = devm_kzalloc(&pdev->dev, sizeof(*phy), GFP_KERNEL);
	if (!phy)
		return -ENOMEM;

	dev_info(&pdev->dev, "USB3 phy probe:phy_mem:0x%lx, iomap phy_base:0x%lx\n",
			(unsigned long)phy_mem->start, (unsigned long)phy_base);

	phy->dev		= dev;
	phy->regs		= phy_base;
	phy->phy3_cfg	= phy3_base;
	phy->phy3_cfg_r1 = (void __iomem *)
			((unsigned long)phy->phy3_cfg + 4 * 1);
	phy->phy3_cfg_r2 = (void __iomem *)
			((unsigned long)phy->phy3_cfg + 4 * 2);
	phy->phy3_cfg_r4 = (void __iomem *)
			((unsigned long)phy->phy3_cfg + 4 * 4);
	phy->phy3_cfg_r5 = (void __iomem *)
			((unsigned long)phy->phy3_cfg + 4 * 5);
	phy->phy31_cfg	= phy31_base;
	phy->phy31_cfg_r1 = (void __iomem *)
			((unsigned long)phy->phy31_cfg + 4 * 1);
	phy->phy31_cfg_r2 = (void __iomem *)
			((unsigned long)phy->phy31_cfg + 4 * 2);
	phy->phy31_cfg_r4 = (void __iomem *)
			((unsigned long)phy->phy31_cfg + 4 * 4);
	phy->phy31_cfg_r5 = (void __iomem *)
			((unsigned long)phy->phy31_cfg + 4 * 5);
	phy->portconfig_30 = portconfig_30;
	phy->portconfig_31 = portconfig_31;
	phy->portnum      = portnum;
	phy->suspend_flag = 0;
	phy->phy.dev		= phy->dev;
	phy->phy.label		= "amlogic-usbphy3";
	phy->phy.init		= amlogic_new_usb3_init_v3;
	phy->phy.set_suspend	= amlogic_new_usb3_suspend;
	phy->phy.shutdown	= amlogic_new_usb3phy_shutdown;
	phy->phy.type		= USB_PHY_TYPE_USB3;
	phy->phy.flags		= AML_USB3_PHY_DISABLE;
	phy->pwr_ctl = pwr_ctl;

	/* set the phy from pcie to usb3 */
	if (phy->portnum > 0) {
		if (phy->pwr_ctl) {
			phy->u30_ctrl_sleep_shift = u30_ctrl_sleep_shift;
			phy->u30_hhi_mem_pd_shift = u30_hhi_mem_pd_shift;
			phy->u30_hhi_mem_pd_mask = u30_hhi_mem_pd_mask;
			phy->u30_ctrl_iso_shift = u30_ctrl_iso_shift;
			phy->reset_regs = reset_base;
			phy->usb30_ctrl_rst_bit = usb30_ctrl_rst_bit;
			phy->u31_ctrl_sleep_shift = u31_ctrl_sleep_shift;
			phy->u31_hhi_mem_pd_shift = u31_hhi_mem_pd_shift;
			phy->u31_hhi_mem_pd_mask = u31_hhi_mem_pd_mask;
			phy->u31_ctrl_iso_shift = u31_ctrl_iso_shift;
			phy->usb31_ctrl_rst_bit = usb31_ctrl_rst_bit;
			power_switch_to_pcie(phy);
			udelay(100);
		}

		if (portconfig_30) {
			val = readl((void __iomem *)
			((unsigned long)phy->reset_regs + (0x20 * 4 - 0x8)));
			writel((val & (~(0x1 << 14))), (void __iomem *)
			((unsigned long)phy->reset_regs + (0x20 * 4 - 0x8)));

			udelay(100);

			val = readl((void __iomem *)
			((unsigned long)phy->reset_regs + (0x20 * 4 - 0x8)));

			writel((val | (0x1 << 14)), (void __iomem	*)
			((unsigned long)phy->reset_regs + (0x20 * 4 - 0x8)));

			udelay(100);
			writel((readl(phy->phy3_cfg) | (3<<5)), phy->phy3_cfg);
		}
		udelay(100);

		if (portconfig_31) {
			val = readl((void __iomem *)
			((unsigned long)phy->reset_regs + (0x20 * 4 - 0x8)));
			writel((val & (~(0x1 << 29))), (void __iomem *)
			((unsigned long)phy->reset_regs + (0x20 * 4 - 0x8)));

			udelay(100);

			val = readl((void __iomem *)
			((unsigned long)phy->reset_regs + (0x20 * 4 - 0x8)));

			writel((val | (0x1 << 29)), (void __iomem	*)
			((unsigned long)phy->reset_regs + (0x20 * 4 - 0x8)));
			udelay(100);

			writel((readl(phy->phy31_cfg) | (3<<5)),
				phy->phy31_cfg);
		}

		if (portconfig_30) {
			phy->gate0_clk = devm_clk_get(dev, "pcie0_gate");
			if (IS_ERR(phy->gate0_clk)) {
				dev_err(dev, "Failed to get usb3 bus clock\n");
				ret = PTR_ERR(phy->gate0_clk);
				return ret;
			}

			ret = clk_prepare_enable(phy->gate0_clk);
			if (ret) {
				dev_err(dev, "Failed to enable usb3 bus clock\n");
				ret = PTR_ERR(phy->gate0_clk);
				return ret;
			}
		}

		if (portconfig_31) {
			phy->gate1_clk = devm_clk_get(dev, "pcie1_gate");
			if (IS_ERR(phy->gate1_clk)) {
				dev_err(dev, "Failed to get usb3 bus clock\n");
				ret = PTR_ERR(phy->gate1_clk);
				return ret;
			}

			ret = clk_prepare_enable(phy->gate1_clk);
			if (ret) {
				dev_err(dev, "Failed to enable usb3 bus clock\n");
				ret = PTR_ERR(phy->gate1_clk);
				return ret;
			}
		}

		phy->clk = devm_clk_get(dev, "pcie_refpll");
		if (IS_ERR(phy->clk)) {
			dev_err(dev, "Failed to get usb3 bus clock\n");
			ret = PTR_ERR(phy->clk);
			return ret;
		}

		ret = clk_prepare_enable(phy->clk);
		if (ret) {
			dev_err(dev, "Failed to enable usb3 bus clock\n");
			ret = PTR_ERR(phy->clk);
			return ret;
		}

		rate = clk_get_rate(phy->clk);
		if (rate != PCIE_PLL_RATE)
			dev_err(dev, "pcie_refpll is not 100M, it is %ld\n",
					rate);

		phy->phy.flags = AML_USB3_PHY_ENABLE;
	}

	usb_add_phy_dev(&phy->phy);

	platform_set_drvdata(pdev, phy);

	pm_runtime_enable(phy->dev);

	return 0;
}

static int amlogic_new_usb3_v3_remove(struct platform_device *pdev)
{
	return 0;
}

#ifdef CONFIG_PM_RUNTIME

static int amlogic_new_usb3_v3_runtime_suspend(struct device *dev)
{
	return 0;
}

static int amlogic_new_usb3_v3_runtime_resume(struct device *dev)
{
	u32 ret = 0;

	return ret;
}

static const struct dev_pm_ops amlogic_new_usb3_pm_ops = {
	SET_RUNTIME_PM_OPS(amlogic_new_usb3_v3_runtime_suspend,
		amlogic_new_usb3_v3_runtime_resume,
		NULL)
};

#define DEV_PM_OPS     (&amlogic_new_usb3_pm_ops)
#else
#define DEV_PM_OPS     NULL
#endif

#ifdef CONFIG_OF
static const struct of_device_id amlogic_new_usb3_v3_id_table[] = {
	{ .compatible = "amlogic, amlogic-new-usb3-v3" },
	{}
};
MODULE_DEVICE_TABLE(of, amlogic_new_usb3_v3_id_table);
#endif

static struct platform_driver amlogic_new_usb3_v3_driver = {
	.probe		= amlogic_new_usb3_v3_probe,
	.remove		= amlogic_new_usb3_v3_remove,
	.driver		= {
		.name	= "amlogic-new-usb3-v3",
		.owner	= THIS_MODULE,
		.pm	= DEV_PM_OPS,
		.of_match_table = of_match_ptr(amlogic_new_usb3_v3_id_table),
	},
};

module_platform_driver(amlogic_new_usb3_v3_driver);

MODULE_ALIAS("platform: amlogic_usb3_v2");
MODULE_AUTHOR("Amlogic Inc.");
MODULE_DESCRIPTION("amlogic USB3 v2 phy driver");
MODULE_LICENSE("GPL v2");
