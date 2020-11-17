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

#include <linux/amlogic/media/vout/hdmi_tx/hdmi_tx_module.h>

#include "common.h"
#include "hdmi_tx_reg.h"
#include "reg_ops.h"

struct reg_map reg_maps[REG_IDX_END] = {0};
int hdmitx_init_reg_map(struct platform_device *pdev)
{
	int i = 0;
	struct resource res;
	struct device_node *np = pdev->dev.of_node;

	for (i = CBUS_REG_IDX; i < REG_IDX_END; i++) {
		if (of_address_to_resource(np, i, &res)) {
			pr_err("not get regbase index %d\n", i);
			return 0;
		}

		reg_maps[i].phy_addr = res.start;
		reg_maps[i].size = resource_size(&res);
		reg_maps[i].p = devm_ioremap(&pdev->dev, res.start,
					     resource_size(&res));
		if (IS_ERR(reg_maps[i].p))
			return -ENOMEM;

		pr_debug("Mapped PHY: 0x%x\n", reg_maps[i].phy_addr);
	}

	return 0;
}

unsigned int get_hdcp22_base(void)
{
	return reg_maps[ELP_ESM_REG_IDX].phy_addr;
}

unsigned int TO_PHY_ADDR(unsigned int addr)
{
	unsigned int index;
	unsigned int offset;

	index = addr >> BASE_REG_OFFSET;
	offset = addr & (((1 << BASE_REG_OFFSET) - 1));

	return (reg_maps[index].phy_addr + offset);
}

void __iomem *TO_PMAP_ADDR(unsigned int addr)
{
	unsigned int index;
	unsigned int offset;

	index = addr >> BASE_REG_OFFSET;
	offset = addr & (((1 << BASE_REG_OFFSET) - 1));

	return (void __iomem *)(reg_maps[index].p + offset);
}

unsigned int hd_read_reg(unsigned int addr)
{
	unsigned int val = 0;
	unsigned int paddr = TO_PHY_ADDR(addr);

	struct hdmitx_dev *hdev = get_hdmitx_device();

	pr_debug(REG "Rd[0x%x] 0x%x\n", paddr, val);

	switch (hdev->data->chip_type) {
	case MESON_CPU_ID_TXLX:
	case MESON_CPU_ID_GXL:
	case MESON_CPU_ID_GXM:
	case MESON_CPU_ID_G12A:
	case MESON_CPU_ID_G12B:
	case MESON_CPU_ID_SM1:
	case MESON_CPU_ID_TM2:
	case MESON_CPU_ID_TM2B:
	case MESON_CPU_ID_SC2:
	default:
		val = readl(TO_PMAP_ADDR(addr));
		break;
	}

	return val;
}
EXPORT_SYMBOL(hd_read_reg);

void hd_write_reg(unsigned int addr, unsigned int val)
{
	unsigned int paddr = TO_PHY_ADDR(addr);

	struct hdmitx_dev *hdev = get_hdmitx_device();

	pr_debug(REG "Wr[0x%x] 0x%x\n", paddr, val);

	switch (hdev->data->chip_type) {
	case MESON_CPU_ID_TXLX:
	case MESON_CPU_ID_GXL:
	case MESON_CPU_ID_GXM:
	case MESON_CPU_ID_G12A:
	case MESON_CPU_ID_G12B:
	case MESON_CPU_ID_SM1:
	case MESON_CPU_ID_TM2:
	case MESON_CPU_ID_TM2B:
	case MESON_CPU_ID_SC2:
	default:
		writel(val, TO_PMAP_ADDR(addr));
		break;
	}
}
EXPORT_SYMBOL(hd_write_reg);

void hd_set_reg_bits(unsigned int addr, unsigned int value,
		     unsigned int offset, unsigned int len)
{
	unsigned int data32 = 0;

	data32 = hd_read_reg(addr);
	data32 &= ~(((1 << len) - 1) << offset);
	data32 |= (value & ((1 << len) - 1)) << offset;
	hd_write_reg(addr, data32);
}
EXPORT_SYMBOL(hd_set_reg_bits);

unsigned int hdmitx_rd_reg_normal(unsigned int addr)
{
	unsigned long offset = (addr & DWC_OFFSET_MASK) >> 24;
	unsigned int data;
	struct arm_smccc_res res;

	arm_smccc_smc(0x82000018, (unsigned long)addr, 0, 0, 0, 0, 0, 0, &res);

	data = (unsigned int)((res.a0) & 0xffffffff);

	pr_debug(REG "%s rd[0x%x] 0x%x\n", offset ? "DWC" : "TOP",
		 addr, data);
	return data;
}

unsigned int hdmitx_rd_reg_g12a(unsigned int addr)
{
	unsigned int large_offset = addr >> 24;
	unsigned int small_offset = addr & ((1 << 24)  - 1);
	unsigned long hdmitx_addr = 0;
	unsigned int val;

	switch  (large_offset) {
	case 0x10:
		/*DWC*/
		hdmitx_addr = HDMITX_SEC_REG_ADDR(small_offset);
		val = readb(TO_PMAP_ADDR(hdmitx_addr));
		break;
	case 0x11:
	case 0x01:
		/*SECURITY DWC/TOP*/
		val = hdmitx_rd_reg_normal(addr);
		break;
	case 00:
	default:
		/*TOP*/
		if (small_offset >= 0x2000 && small_offset <= 0x365E) {
			hdmitx_addr = HDMITX_REG_ADDR(small_offset);
			val = readb(TO_PMAP_ADDR(hdmitx_addr));
		} else {
			hdmitx_addr = HDMITX_REG_ADDR((small_offset << 2));
			val = readl(TO_PMAP_ADDR(hdmitx_addr));
		}
		break;
	}
	return val;
}

unsigned int hdmitx_rd_reg(unsigned int addr)
{
	unsigned int data;
	struct hdmitx_dev *hdev = get_hdmitx_device();

	if (hdev->data->chip_type >= MESON_CPU_ID_G12A)
		data = hdmitx_rd_reg_g12a(addr);
	else
		data = hdmitx_rd_reg_normal(addr);
	return data;
}
EXPORT_SYMBOL(hdmitx_rd_reg);

bool hdmitx_get_bit(unsigned int addr, unsigned int bit_nr)
{
	return (hdmitx_rd_reg(addr) & (1 << bit_nr)) == (1 << bit_nr);
}

void hdmitx_wr_reg_normal(unsigned int addr, unsigned int data)
{
	unsigned long offset = (addr & DWC_OFFSET_MASK) >> 24;
	struct arm_smccc_res res;

	arm_smccc_smc(0x82000019,
		      (unsigned long)addr, data,
		      0, 0, 0, 0, 0, &res);

	pr_debug("%s wr[0x%x] 0x%x\n", offset ? "DWC" : "TOP",
		 addr, data);
}

void hdmitx_wr_reg_g12a(unsigned int addr, unsigned int data)
{
	unsigned int large_offset = addr >> 24;
	unsigned int small_offset = addr & ((1 << 24)  - 1);
	unsigned long hdmitx_addr = 0;

	switch (large_offset) {
	case 0x10:
		/*DWC*/
		hdmitx_addr = HDMITX_SEC_REG_ADDR(small_offset);
		writeb(data & 0xff, TO_PMAP_ADDR(hdmitx_addr));
		break;
	case 0x11:
	case 0x01:
		/*SECURITY DWC/TOP*/
		hdmitx_wr_reg_normal(addr, data);
		break;
	case 00:
	default:
		/*TOP*/
		if (small_offset >= 0x2000 && small_offset <= 0x365E) {
			hdmitx_addr = HDMITX_REG_ADDR(small_offset);
			writeb(data & 0xff, TO_PMAP_ADDR(hdmitx_addr));
		} else {
			hdmitx_addr = HDMITX_REG_ADDR((small_offset << 2));
			writel(data, TO_PMAP_ADDR(hdmitx_addr));
		}
	}
}

void hdmitx_wr_reg(unsigned int addr, unsigned int data)
{
	struct hdmitx_dev *hdev = get_hdmitx_device();

	if (hdev->data->chip_type >= MESON_CPU_ID_G12A)
		hdmitx_wr_reg_g12a(addr, data);
	else
		hdmitx_wr_reg_normal(addr, data);
}
EXPORT_SYMBOL(hdmitx_wr_reg);

void hdmitx_set_reg_bits(unsigned int addr, unsigned int value,
			 unsigned int offset, unsigned int len)
{
	unsigned int data32 = 0;

	data32 = hdmitx_rd_reg(addr);
	data32 &= ~(((1 << len) - 1) << offset);
	data32 |= (value & ((1 << len) - 1)) << offset;
	hdmitx_wr_reg(addr, data32);
}
EXPORT_SYMBOL(hdmitx_set_reg_bits);

void hdmitx_poll_reg(unsigned int addr, unsigned int val, unsigned long timeout)
{
	unsigned long time = 0;

	time = jiffies;
	while ((!(hdmitx_rd_reg(addr) & val)) &&
	       time_before(jiffies, time + timeout)) {
		mdelay(2);
	}
	if (time_after(jiffies, time + timeout))
		pr_info(REG "hdmitx poll:0x%x  val:0x%x T1=%lu t=%lu T2=%lu timeout\n",
			addr, val, time, timeout, jiffies);
}
EXPORT_SYMBOL(hdmitx_poll_reg);

unsigned int hdmitx_rd_check_reg(unsigned int addr, unsigned int exp_data,
				 unsigned int mask)
{
	unsigned long rd_data;

	rd_data = hdmitx_rd_reg(addr);
	if ((rd_data | mask) != (exp_data | mask)) {
		pr_info(REG "HDMITX-DWC addr=0x%04x rd_data=0x%02x\n",
			(unsigned int)addr, (unsigned int)rd_data);
		pr_info(REG "HDMITX-DWC exp_data=0x%02x mask=0x%02x\n",
			(unsigned int)exp_data, (unsigned int)mask);
		return 1;
	}
	return 0;
}
EXPORT_SYMBOL(hdmitx_rd_check_reg);
