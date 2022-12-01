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

	if (hdmi_dbg)
		pr_info("sec_wr32[0x%08x] 0x%08x\n", addr, data);
	arm_smccc_smc(0x82000019, (unsigned long)addr, data, 32, 0, 0, 0, 0, &res);
}

static void sec_wr8(u32 addr, u8 data)
{
	struct arm_smccc_res res;

	if (hdmi_dbg)
		pr_info("sec_wr8[0x%08x] 0x%02x\n", addr, data);
	arm_smccc_smc(0x82000019, (unsigned long)addr, data & 0xff, 8, 0, 0, 0, 0, &res);
}

static u32 sec_rd(u32 addr)
{
	u32 data;
	struct arm_smccc_res res;

	if (hdmi_dbg)
		pr_info("sec_rd32[0x%08x]\n", addr);
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

	if (hdmi_dbg)
		pr_info("%s[0x%08x]\n", __func__, addr);
	arm_smccc_smc(0x82000018, (unsigned long)addr, 8, 0, 0, 0, 0, 0, &res);
	data = (unsigned int)((res.a0) & 0xffffffff);

	if (hdmi_dbg)
		pr_info("[0x%08x] 0x%02x\n", addr, data);
	return data;
}

u32 TO21_PHY_ADDR(u32 addr)
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

static u32 get_enc_paddr(unsigned int addr)
{
	struct hdmitx_dev *hdev = get_hdmitx21_device();
	unsigned int idx = addr >> BASE_REG_OFFSET;
	unsigned int offset = (addr & 0xffff) >> 2;

	if (hdev->enc_idx == 2 && idx == VPUCTRL_REG_IDX) {
		if (offset >= 0x1b00 && offset < 0x1d00)
			return addr + (0x800 << 2);
	}
	return addr;
}

u32 hd21_read_reg(u32 vaddr)
{
	u32 val;
	u32 paddr = get_enc_paddr(vaddr);

	val = readl(TO_PMAP_ADDR(paddr));
	if (hdmi_dbg)
		pr_info("Rd32[0x%08x] 0x%08x\n", TO21_PHY_ADDR(paddr), val);
	return val;
}
EXPORT_SYMBOL(hd21_read_reg);

void hd21_write_reg(u32 vaddr, u32 val)
{
	u32 rval;
	u32 paddr = get_enc_paddr(vaddr);

	writel(val, TO_PMAP_ADDR(paddr));
	rval = readl(TO_PMAP_ADDR(paddr));
	if (!hdmi_dbg)
		return;
	if (val != rval)
		pr_info("Wr32[0x%08x] 0x%08x != Rd32 0x%08x\n",
			TO21_PHY_ADDR(paddr), val, rval);
	else
		pr_info("Wr32[0x%08x] 0x%08x\n",
			TO21_PHY_ADDR(paddr), val);
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

	base_offset = reg21_maps[2].phy_addr;

	data = sec_rd(base_offset + addr);
	return data;
} /* hdmitx_rd_top */

static u8 hdmitx_rd_cor(u32 addr)
{
	u32 base_offset;
	u8 data;

	base_offset = reg21_maps[1].phy_addr;
	data = sec_rd8(base_offset + addr);
	return data;
} /* hdmitx_rd_cor */

static void hdmitx_wr_top(u32 addr, u32 data)
{
	u32 base_offset;

	base_offset = reg21_maps[2].phy_addr;
	sec_wr(base_offset + addr, data);
} /* hdmitx_wr_top */

static void hdmitx_wr_cor(u32 addr, u8 data)
{
	u32 base_offset;

	base_offset = reg21_maps[1].phy_addr;
	sec_wr8(base_offset + addr, data);
} /* hdmitx_wr_cor */

static DEFINE_SPINLOCK(reg_lock);

u32 hdmitx21_rd_reg(u32 addr)
{
	u32 offset;
	u32 data;
	unsigned long flags;

	spin_lock_irqsave(&reg_lock, flags);
	offset = (addr & TOP_OFFSET_MASK) >> 24;

	addr = addr & 0xffff;
	if (offset)
		data = hdmitx_rd_top(addr);
	else
		data = hdmitx_rd_cor(addr);
	spin_unlock_irqrestore(&reg_lock, flags);

	return data;
}

void hdmitx21_wr_reg(u32 addr, u32 val)
{
	unsigned long flags;
	u32 offset;

	spin_lock_irqsave(&reg_lock, flags);
	offset = (addr & TOP_OFFSET_MASK) >> 24;

	addr = addr & 0xffff;
	if (offset) {
		hdmitx_wr_top(addr, val);
	} else {
		/* don't clear hdcp2.2 interrupt if hdcp22 core is under reset */
		if ((addr != CP2TX_INTR0_IVCTX &&
			addr != CP2TX_INTR1_IVCTX &&
			addr != CP2TX_INTR2_IVCTX &&
			addr != CP2TX_INTR3_IVCTX) ||
			!(hdmitx_rd_cor(HDCP2X_TX_SRST_IVCTX) & 0x30))
			hdmitx_wr_cor(addr, val);
	}
	spin_unlock_irqrestore(&reg_lock, flags);
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

void hdmitx21_set_bit(u32 addr, u32 bit_val, bool st)
{
	u32 data32 = 0;

	data32 = hdmitx21_rd_reg(addr);
	data32 = st ? (data32 | bit_val) : (data32 & ~bit_val);
	hdmitx21_wr_reg(addr, data32);
}
EXPORT_SYMBOL(hdmitx21_set_bit);

void hdmitx21_poll_reg(u32 addr, u8 exp_data, u8 mask, ulong timeout)
{
	u8 rd_data;
	u8 done = 0;
	ulong time = 0;

	time = jiffies;

	rd_data = hdmitx21_rd_reg(addr);
	while (time_before(jiffies, time + timeout)) {
		if ((rd_data | mask) == (exp_data | mask)) {
			done = 1;
			break;
		}
		rd_data = hdmitx21_rd_reg(addr);
		usleep_range(10, 20);
	}
	if (done == 0)
		pr_info("%s 0x%x poll time-out!\n", __func__, addr);
} /* hdmitx21_poll_reg */
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

void hdmitx21_seq_rd_reg(u16 offset, u8 *buf, u16 cnt)
{
	int i = 0;

	while (cnt--) {
		buf[i] = hdmitx21_rd_reg(offset + i);
		i++;
	}
}
EXPORT_SYMBOL(hdmitx21_seq_rd_reg);

void hdmitx21_fifo_read(u16 offset, u8 *buf, u16 cnt)
{
	int i = 0;

	if (!buf)
		return;
	for (i = 0; i < cnt; i++)
		buf[i] = hdmitx21_rd_reg(offset);
}
EXPORT_SYMBOL(hdmitx21_fifo_read);

MODULE_PARM_DESC(hdmi_dbg, "\n hdmi_dbg\n");
module_param(hdmi_dbg, int, 0644);
