// SPDX-License-Identifier: GPL-2.0
/* Copyright (C) 2023 Rockchip Electronics Co., Ltd */

#include <linux/delay.h>
#include <linux/of_platform.h>
#include <linux/slab.h>

#include "common.h"
#include "dev.h"
#include "regs.h"

void rkvpss_write(struct rkvpss_device *dev, u32 reg, u32 val)
{
	u32 *mem = dev->sw_base_addr + reg;
	u32 *flag = dev->sw_base_addr + reg + RKVPSS_SW_REG_SIZE;

	*mem = val;
	*flag = RKVPSS_REG_CACHE;
	if (dev->hw_dev->is_single)
		rkvpss_hw_write(dev->hw_dev, reg, val);
}

u32 rkvpss_read(struct rkvpss_device *dev, u32 reg)
{
	u32 val;

	if (dev->hw_dev->is_single)
		val = rkvpss_hw_read(dev->hw_dev, reg);
	else
		val = *(u32 *)(dev->sw_base_addr + reg);
	return val;
}

void rkvpss_set_bits(struct rkvpss_device *dev, u32 reg, u32 mask, u32 val)
{
	u32 *mem = dev->sw_base_addr + reg;
	u32 *flag = dev->sw_base_addr + reg + RKVPSS_SW_REG_SIZE;

	*mem &= ~mask;
	*mem |= val;
	*flag = RKVPSS_REG_CACHE;
	if (dev->hw_dev->is_single)
		rkvpss_hw_set_bits(dev->hw_dev, reg, mask, val);
}

void rkvpss_clear_bits(struct rkvpss_device *dev, u32 reg, u32 mask)
{
	u32 *mem = dev->sw_base_addr + reg;
	u32 *flag = dev->sw_base_addr + reg + RKVPSS_SW_REG_SIZE;

	*mem &= ~mask;
	*flag = RKVPSS_REG_CACHE;
	if (dev->hw_dev->is_single)
		rkvpss_hw_clear_bits(dev->hw_dev, reg, mask);
}

void rkvpss_update_regs(struct rkvpss_device *dev, u32 start, u32 end)
{
	struct rkvpss_hw_dev *hw = dev->hw_dev;
	void __iomem *base = hw->base_addr;
	unsigned long lock_flags = 0;
	u32 i, j;

	if (end > RKVPSS_SW_REG_SIZE - 4) {
		dev_err(dev->dev, "%s out of range\n", __func__);
		return;
	}
	for (i = start; i <= end; i += 4) {
		u32 *val = dev->sw_base_addr + i;
		u32 *flag = dev->sw_base_addr + i + RKVPSS_SW_REG_SIZE;

		if (*flag != RKVPSS_REG_CACHE)
			continue;

		if (IS_SYNC_REG(i)) {
			spin_lock_irqsave(&hw->reg_lock, lock_flags);
			if (i == RKVPSS_VPSS_ONLINE) {
				u32 mask = 0;

				for (j = 0; j < RKVPSS_OUTPUT_MAX; j++) {
					if (!hw->is_ofl_ch[j])
						continue;
					mask |= (RKVPSS_ISP2VPSS_CHN0_SEL(3) << j * 2);
				}
				*val |= (readl(base + i) & mask);
			}
		}
		writel(*val, base + i);
		if (IS_SYNC_REG(i))
			spin_unlock_irqrestore(&hw->reg_lock, lock_flags);
	}
}

int rkvpss_attach_hw(struct rkvpss_device *vpss)
{
	struct device_node *np;
	struct platform_device *pdev;
	struct rkvpss_hw_dev *hw;

	np = of_parse_phandle(vpss->dev->of_node, "rockchip,hw", 0);
	if (!np || !of_device_is_available(np)) {
		dev_err(vpss->dev, "failed to get vpss hw node\n");
		return -ENODEV;
	}

	pdev = of_find_device_by_node(np);
	of_node_put(np);
	if (!pdev) {
		dev_err(vpss->dev, "failed to get vpss hw from node\n");
		return -ENODEV;
	}

	hw = platform_get_drvdata(pdev);
	if (!hw) {
		dev_err(vpss->dev, "failed attach vpss hw\n");
		return -EINVAL;
	}

	if (hw->dev_num >= VPSS_MAX_DEV) {
		dev_err(vpss->dev, "failed attach vpss hw, max dev:%d\n", VPSS_MAX_DEV);
		return -EINVAL;
	}

	if (hw->dev_num)
		hw->is_single = false;
	vpss->dev_id = hw->dev_num;
	hw->vpss[hw->dev_num] = vpss;
	hw->dev_num++;
	vpss->hw_dev = hw;
	vpss->vpss_ver = hw->vpss_ver;

	return 0;
}
