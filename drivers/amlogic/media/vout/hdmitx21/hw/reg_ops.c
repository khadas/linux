// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/delay.h>
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
#include <linux/io.h>
#include <linux/of_address.h>
#include <linux/arm-smccc.h>

#include <linux/amlogic/media/vout/hdmi_tx21/hdmi_tx_module.h>

#include "common.h"

// TODO
#define HDMITX_TOP_OFFSET 0xfe300000
#define HDMITX_COR_OFFSET 0xfe380000

static int hdmi_dbg;

struct reg_map reg21_maps[REG_IDX_END] = {0};
int hdmitx21_init_reg_map(struct platform_device *pdev)
{
	int i = 0;
	struct resource res;
	struct device_node *np = pdev->dev.of_node;

	for (i = 0; i < REG_IDX_END; i++) {
		if (of_address_to_resource(np, i, &res)) {
			pr_err("not get regbase index %d\n", i);
			return 0;
		}

		reg21_maps[i].phy_addr = res.start;
		reg21_maps[i].size = resource_size(&res);
		reg21_maps[i].p = devm_ioremap(&pdev->dev, res.start,
					     resource_size(&res));
		if (IS_ERR(reg21_maps[i].p))
			return -ENOMEM;

		pr_debug("Mapped PHY: 0x%x\n", reg21_maps[i].phy_addr);
	}

	return 0;
}

u32 get21_hdcp22_base(void)
{
	return 0;
}

static void sec_wr(u32 addr, u32 data)
{
	struct arm_smccc_res res;

	arm_smccc_smc(0x82000019, (unsigned long)addr, data, 32, 0, 0, 0, 0, &res);
	if (hdmi_dbg)
		pr_info("sec_wr32[0x%08x] 0x%08x\n", addr, data);
}

static void sec_wr8(u32 addr, u8 data)
{
	struct arm_smccc_res res;

	arm_smccc_smc(0x82000019, (unsigned long)addr, data & 0xff, 8, 0, 0, 0, 0, &res);
	if (hdmi_dbg)
		pr_info("[0x%08x] 0x%02x\n", addr, data);
}

static u32 sec_rd(u32 addr)
{
	u32 data;
	struct arm_smccc_res res;

	arm_smccc_smc(0x82000018, (unsigned long)addr, 32, 0, 0, 0, 0, 0, &res);
	data = (unsigned int)((res.a0) & 0xffffffff);

	if (hdmi_dbg)
		pr_info("[0x%08x] 0x%08x\n", addr, data);
	return data;
}

static u8 sec_rd8(u32 addr)
{
	u32 data;
	struct arm_smccc_res res;

	arm_smccc_smc(0x82000018, (unsigned long)addr, 8, 0, 0, 0, 0, 0, &res);
	data = (unsigned int)((res.a0) & 0xffffffff);

	if (hdmi_dbg)
		pr_info("[0x%08x] 0x%02x\n", addr, data);
	return data;
}

static u32 TO_PHY_ADDR(u32 addr)
{
	u32 index;
	u32 offset;

	index = addr >> BASE_REG_OFFSET;
	offset = addr & (((1 << BASE_REG_OFFSET) - 1));

	return (reg21_maps[index].phy_addr + offset);
}

static void __iomem *TO_PMAP_ADDR(u32 addr)
{
	u32 index;
	u32 offset;

	index = addr >> BASE_REG_OFFSET;
	offset = addr & (((1 << BASE_REG_OFFSET) - 1));

	return (void __iomem *)(reg21_maps[index].p + offset);
}

u32 hd21_read_reg(u32 addr)
{
	u32 val;

	val = readl(TO_PMAP_ADDR(addr));
	if (hdmi_dbg)
		pr_info("Rd32[0x%08x] 0x%08x\n", TO_PHY_ADDR(addr), val);
	return val;
}
EXPORT_SYMBOL(hd21_read_reg);

void hd21_write_reg(u32 addr, u32 val)
{
	u32 rval;

	writel(val, TO_PMAP_ADDR(addr));
	rval = readl(TO_PMAP_ADDR(addr));
	if (!hdmi_dbg)
		return;
	if (val != rval)
		pr_info("Wr32[0x%08x] 0x%08x != Rd32 0x%08x\n",
			TO_PHY_ADDR(addr), val, rval);
	else
		pr_info("Wr32[0x%08x] 0x%08x\n",
			TO_PHY_ADDR(addr), val);
}
EXPORT_SYMBOL(hd21_write_reg);

void hd21_set_reg_bits(u32 addr, u32 value,
		     u32 offset, u32 len)
{
	u32 data32 = 0;

	data32 = hd21_read_reg(addr);
	data32 &= ~(((1 << len) - 1) << offset);
	data32 |= (value & ((1 << len) - 1)) << offset;
	hd21_write_reg(addr, data32);
}
EXPORT_SYMBOL(hd21_set_reg_bits);

static u32 hdmitx_rd_top(u32 addr)
{
	u32 base_offset;
	u32 data;

	base_offset = HDMITX_TOP_OFFSET;

	data = sec_rd(base_offset + addr);
	return data;
} /* hdmitx_rd_top */

static u8 hdmitx_rd_cor(u32 addr)
{
	u32 base_offset;
	u8 data;

	base_offset = HDMITX_COR_OFFSET;
	data = sec_rd8(base_offset + addr);
	return data;
} /* hdmitx_rd_cor */

static void hdmitx_wr_top(u32 addr, u32 data)
{
	u32 base_offset;

	base_offset = HDMITX_TOP_OFFSET;
	sec_wr(base_offset + addr, data);
} /* hdmitx_wr_top */

static void hdmitx_wr_cor(u32 addr, u8 data)
{
	u32 base_offset;

	base_offset = HDMITX_COR_OFFSET;
	sec_wr8(base_offset + addr, data);
} /* hdmitx_wr_cor */

u32 hdmitx21_rd_reg(u32 addr)
{
	u32 offset = (addr & TOP_OFFSET_MASK) >> 24;
	u32 data;

	addr = addr & 0xffff;
	if (offset)
		data = hdmitx_rd_top(addr);
	else
		data = hdmitx_rd_cor(addr);

	return data;
}

void hdmitx21_wr_reg(u32 addr, u32 val)
{
	u32 offset = (addr & TOP_OFFSET_MASK) >> 24;

	addr = addr & 0xffff;
	if (offset)
		hdmitx_wr_top(addr, val);
	else
		hdmitx_wr_cor(addr, val);
}

bool hdmitx21_get_bit(u32 addr, u32 bit_nr)
{
	return (hdmitx21_rd_reg(addr) & (1 << bit_nr)) == (1 << bit_nr);
}

void hdmitx21_set_reg_bits(u32 addr, u32 value,
			 u32 offset, u32 len)
{
	u32 data32 = 0;

	data32 = hdmitx21_rd_reg(addr);
	data32 &= ~(((1 << len) - 1) << offset);
	data32 |= (value & ((1 << len) - 1)) << offset;
	hdmitx21_wr_reg(addr, data32);
}
EXPORT_SYMBOL(hdmitx21_set_reg_bits);

void hdmitx21_poll_reg(u32 addr, u32 val, unsigned long timeout)
{
	unsigned long time = 0;

	time = jiffies;
	while ((!(hdmitx21_rd_reg(addr) & val)) &&
	       time_before(jiffies, time + timeout)) {
		mdelay(2);
	}
	if (time_after(jiffies, time + timeout))
		pr_info(REG "hdmitx poll:0x%x  val:0x%x T1=%lu t=%lu T2=%lu timeout\n",
			addr, val, time, timeout, jiffies);
}
EXPORT_SYMBOL(hdmitx21_poll_reg);

u32 hdmitx21_rd_check_reg(u32 addr, u32 exp_data,
				 u32 mask)
{
	unsigned long rd_data;

	rd_data = hdmitx21_rd_reg(addr);
	if ((rd_data | mask) != (exp_data | mask)) {
		pr_info(REG "HDMITX-DWC addr=0x%04x rd_data=0x%02x\n",
			(unsigned int)addr, (unsigned int)rd_data);
		pr_info(REG "HDMITX-DWC exp_data=0x%02x mask=0x%02x\n",
			(unsigned int)exp_data, (unsigned int)mask);
		return 1;
	}
	return 0;
}
EXPORT_SYMBOL(hdmitx21_rd_check_reg);

MODULE_PARM_DESC(hdmi_dbg, "\n hdmi_dbg\n");
module_param(hdmi_dbg, int, 0644);
