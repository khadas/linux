/*
 * drivers/amlogic/power/power_domain.c
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

#include <linux/io.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/of_device.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/amlogic/power_domain.h>
#include <dt-bindings/power/amlogic,pd.h>

//dos reg
#define	DOS_SW_RESET0			0x0
#define	DOS_SW_RESET1			0x1c
#define	DOS_SW_RESET3			0xd0
#define	DOS_SW_RESET4			0xdc
#define	DOS_MEM_PD_VDEC			0xc0
#define	DOS_MEM_PD_HCODEC		0xc8
#define	DOS_MEM_PD_HEVC			0xcc
#define	DOS_MEM_PD_WAVE420L		0xe4

//ao reg
#define	AO_RTI_GEN_PWR_SLEEP0		0x0
#define	AO_RTI_GEN_PWR_ISO0		0x4
#define	AO_RTI_GEN_PWR_ACK0		0x8

//mempd reg
#define	HHI_MEM_PD_REG0			0x0
#define	HHI_VPU_MEM_PD_REG0		0x4
#define	HHI_VPU_MEM_PD_REG1		0x8
#define	HHI_DEMOD_MEM_PD_REG		0xc
#define	HHI_DSP_MEM_PD_REG0		0x10
#define	HHI_NANOQ_MEM_PD_REG0		0x18
#define	HHI_NANOQ_MEM_PD_REG1		0x1c
#define	HHI_VPU_MEM_PD_REG2		0x34

//reset reg
#define	RESET0_LEVEL			0x0
#define	RESET1_LEVEL			0x4
#define	RESET2_LEVEL			0x8
#define	RESET3_LEVEL			0xc
#define	RESET4_LEVEL			0x10
#define	RESET5_LEVEL			0x14
#define	RESET6_LEVEL			0x18
#define	RESET7_LEVEL			0x1c

static u32 vpu_mem_pd_reg3;
static u32 vpu_mem_pd_reg4;
static bool probe_done;

struct power_domains {
	void __iomem *dos_addr;
	void __iomem *ao_addr;
	void __iomem *mempd_addr;
	void __iomem *reset_addr;
	/**used for power reg concurrent access protect **/
	spinlock_t power_lock;
	/**used for mempd reg concurrent access protect **/
	spinlock_t mem_pd_lock;
	/**used for reset reg concurrent access protect **/
	spinlock_t reset_lock;
	/**used for iso reg concurrent access protect **/
	spinlock_t iso_lock;
};

static struct power_domains *s_pd;

bool is_support_power_domain(void)
{
	return probe_done;
}
EXPORT_SYMBOL(is_support_power_domain);

static void power_switch(int pwr_domain, bool pwr_switch)
{
	unsigned int value;
	unsigned long flags;

	spin_lock_irqsave(&s_pd->power_lock, flags);
	value = readl(s_pd->ao_addr + AO_RTI_GEN_PWR_SLEEP0);
	if (pwr_switch == PWR_ON)
		value &= ~(1 << pwr_domain);
	else
		value |= (1 << pwr_domain);
	writel(value, (s_pd->ao_addr + AO_RTI_GEN_PWR_SLEEP0));
	spin_unlock_irqrestore(&s_pd->power_lock, flags);
}

static void mem_pd_switch(int pwr_domain, bool pwr_switch)
{
	unsigned int value;
	unsigned long flags;

	spin_lock_irqsave(&s_pd->mem_pd_lock, flags);
	if (pwr_switch == PWR_ON) {
		switch (pwr_domain) {
		case PM_DOS_HCODEC:
			writel(0x0, (s_pd->dos_addr + DOS_MEM_PD_HCODEC));
			break;
		case PM_DOS_VDEC:
			writel(0x0, (s_pd->dos_addr + DOS_MEM_PD_VDEC));
			break;
		case PM_DOS_HEVC:
			writel(0x0, (s_pd->dos_addr + DOS_MEM_PD_HEVC));
			break;
		case PM_WAVE420L:
			writel(0x0, (s_pd->dos_addr + DOS_MEM_PD_WAVE420L));
			break;
		case PM_CSI:
			value = readl(s_pd->mempd_addr + HHI_MEM_PD_REG0);
			value &= ~(0x3 << 6);
			writel(value, (s_pd->mempd_addr + HHI_MEM_PD_REG0));
			break;
		case PM_VPU:
			writel(0x0, (s_pd->mempd_addr + HHI_VPU_MEM_PD_REG0));
			writel(0x0, (s_pd->mempd_addr + HHI_VPU_MEM_PD_REG1));
			writel(0x0, (s_pd->mempd_addr + HHI_VPU_MEM_PD_REG2));
			writel(0x0, (s_pd->mempd_addr + vpu_mem_pd_reg3));
			writel(0x0, (s_pd->mempd_addr + vpu_mem_pd_reg4));
			value = readl(s_pd->mempd_addr + HHI_MEM_PD_REG0);
			value &= ~(0xff << 8);
			writel(value, (s_pd->mempd_addr + HHI_MEM_PD_REG0));
			break;
		case PM_NN:
			writel(0x0, (s_pd->mempd_addr + HHI_NANOQ_MEM_PD_REG0));
			writel(0x0, (s_pd->mempd_addr + HHI_NANOQ_MEM_PD_REG1));
			break;
		case PM_USB:
			value = readl(s_pd->mempd_addr + HHI_MEM_PD_REG0);
			value &= ~(0x3 << 30);
			writel(value, (s_pd->mempd_addr + HHI_MEM_PD_REG0));
			break;
		case PM_PCIE0:
			value = readl(s_pd->mempd_addr + HHI_MEM_PD_REG0);
			value &= ~(0xf << 26);
			writel(value, (s_pd->mempd_addr + HHI_MEM_PD_REG0));
			break;
		case PM_GE2D:
			value = readl(s_pd->mempd_addr + HHI_MEM_PD_REG0);
			value &= ~(0xff << 18);
			writel(value, (s_pd->mempd_addr + HHI_MEM_PD_REG0));
			break;
		case PM_PCIE1:
			value = readl(s_pd->mempd_addr + HHI_MEM_PD_REG0);
			value &= ~(0xf << 4);
			writel(value, (s_pd->mempd_addr + HHI_MEM_PD_REG0));
			break;
		case PM_DSPA:
			value = readl(s_pd->mempd_addr + HHI_DSP_MEM_PD_REG0);
			value &= ~(0xffff);
			writel(value, (s_pd->mempd_addr + HHI_DSP_MEM_PD_REG0));
			break;
		case PM_DSPB:
			value = readl(s_pd->mempd_addr + HHI_DSP_MEM_PD_REG0);
			value &= ~(0xffff << 16);
			writel(value, (s_pd->mempd_addr + HHI_DSP_MEM_PD_REG0));
			break;
		case PM_DEMOD:
			value = readl(s_pd->mempd_addr + HHI_DEMOD_MEM_PD_REG);
			value &= ~0x2fff;
			writel(value, (s_pd->mempd_addr +
			       HHI_DEMOD_MEM_PD_REG));
			break;
		}
	} else {
		switch (pwr_domain) {
		case PM_DOS_HCODEC:
			writel(0xffffffff, (s_pd->dos_addr +
			       DOS_MEM_PD_HCODEC));
			break;
		case PM_DOS_VDEC:
			writel(0xffffffff, (s_pd->dos_addr + DOS_MEM_PD_VDEC));
			break;
		case PM_DOS_HEVC:
			writel(0xffffffff, (s_pd->dos_addr + DOS_MEM_PD_HEVC));
			break;
		case PM_WAVE420L:
			writel(0xffffffff, (s_pd->dos_addr +
			       DOS_MEM_PD_WAVE420L));
			break;
		case PM_CSI:
			value = readl(s_pd->mempd_addr + HHI_MEM_PD_REG0);
			value |= (0x3 << 6);
			writel(value, (s_pd->mempd_addr + HHI_MEM_PD_REG0));
			break;
		case PM_VPU:
			writel(0xffffffff, (s_pd->mempd_addr +
			       HHI_VPU_MEM_PD_REG0));
			writel(0xffffffff, (s_pd->mempd_addr +
			       HHI_VPU_MEM_PD_REG1));
			writel(0xffffffff, (s_pd->mempd_addr +
			       HHI_VPU_MEM_PD_REG2));
			writel(0xffffffff, (s_pd->mempd_addr +
			       vpu_mem_pd_reg3));
			writel(0xffffffff, (s_pd->mempd_addr +
			       vpu_mem_pd_reg4));
			value = readl(s_pd->mempd_addr + HHI_MEM_PD_REG0);
			value |= (0xff << 8);
			writel(value, (s_pd->mempd_addr + HHI_MEM_PD_REG0));
			break;
		case PM_NN:
			writel(0xffffffff, (s_pd->mempd_addr +
			       HHI_NANOQ_MEM_PD_REG0));
			writel(0xffffffff, (s_pd->mempd_addr +
			       HHI_NANOQ_MEM_PD_REG1));
			break;
		case PM_USB:
			value = readl(s_pd->mempd_addr + HHI_MEM_PD_REG0);
			value |= (0x3 << 30);
			writel(value, (s_pd->mempd_addr + HHI_MEM_PD_REG0));
			break;
		case PM_PCIE0:
			value = readl(s_pd->mempd_addr + HHI_MEM_PD_REG0);
			value |= (0xf << 26);
			writel(value, (s_pd->mempd_addr + HHI_MEM_PD_REG0));
			break;
		case PM_GE2D:
			value = readl(s_pd->mempd_addr + HHI_MEM_PD_REG0);
			value |= (0xff << 18);
			writel(value, (s_pd->mempd_addr + HHI_MEM_PD_REG0));
			break;
		case PM_PCIE1:
			value = readl(s_pd->mempd_addr + HHI_MEM_PD_REG0);
			value |= (0xf << 4);
			writel(value, (s_pd->mempd_addr + HHI_MEM_PD_REG0));
			break;
		case PM_DSPA:
			value = readl(s_pd->mempd_addr + HHI_DSP_MEM_PD_REG0);
			value |= (0xffff);
			writel(value, (s_pd->mempd_addr + HHI_DSP_MEM_PD_REG0));
			break;
		case PM_DSPB:
			value = readl(s_pd->mempd_addr + HHI_DSP_MEM_PD_REG0);
			value |= (0xffff << 16);
			writel(value, (s_pd->mempd_addr + HHI_DSP_MEM_PD_REG0));
			break;
		case PM_DEMOD:
			value = readl(s_pd->mempd_addr + HHI_DEMOD_MEM_PD_REG);
			value |= 0x2fff;
			writel(value, (s_pd->mempd_addr +
			       HHI_DEMOD_MEM_PD_REG));
			break;
		}
	}
	spin_unlock_irqrestore(&s_pd->mem_pd_lock, flags);
}

static void reset_switch(int pwr_domain, bool pwr_switch)
{
	unsigned int value;
	unsigned int tmp;
	unsigned long flags;

	spin_lock_irqsave(&s_pd->reset_lock, flags);
	if (pwr_switch == PWR_ON) {
		switch (pwr_domain) {
		case PM_DOS_HCODEC:
			value = readl(s_pd->dos_addr + DOS_SW_RESET1);
			value &= ~(0xffff << 2);
			writel(value, (s_pd->dos_addr + DOS_SW_RESET1));
			break;
		case PM_DOS_VDEC:
			value = readl(s_pd->dos_addr + DOS_SW_RESET0);
			value &= ~(0x1fff << 2);
			writel(value, (s_pd->dos_addr + DOS_SW_RESET0));
			break;
		case PM_DOS_HEVC:
			value = readl(s_pd->dos_addr + DOS_SW_RESET3);
			value &= ~(0x3ffff << 2 | 1 << 24);
			writel(value, (s_pd->dos_addr + DOS_SW_RESET3));
			break;
		case PM_WAVE420L:
			value = readl(s_pd->dos_addr + DOS_SW_RESET4);
			value &= ~(0xf << 8);
			writel(value, (s_pd->dos_addr + DOS_SW_RESET4));
			break;
		case PM_VPU:
			tmp = 0x1 << 5 | 0x1 << 10 | 0x1 << 19 | 0x1 << 13;
			value = readl(s_pd->reset_addr + RESET0_LEVEL);
			value |= tmp;
			writel(value, (s_pd->reset_addr + RESET0_LEVEL));
			tmp = 0x1 << 5 | 0x1 << 4;
			value = readl(s_pd->reset_addr + RESET1_LEVEL);
			value |= tmp;
			writel(value, (s_pd->reset_addr + RESET1_LEVEL));
			tmp = 0x1 << 15;
			value = readl(s_pd->reset_addr + RESET2_LEVEL);
			value |= tmp;
			writel(value, (s_pd->reset_addr + RESET2_LEVEL));
			tmp = 0x1 << 6 | 0x1 << 7 | 0x1 << 13 |
			      0x1 << 5 | 0x1 << 9 | 0x1 << 4 | 0x1 << 12;
			value = readl(s_pd->reset_addr + RESET4_LEVEL);
			value |= tmp;
			writel(value, (s_pd->reset_addr + RESET4_LEVEL));
			tmp = 0x1 << 7;
			value = readl(s_pd->reset_addr + RESET7_LEVEL);
			value |= tmp;
			writel(value, (s_pd->reset_addr + RESET7_LEVEL));
			break;
		case PM_NN:
			value = readl(s_pd->reset_addr + RESET2_LEVEL);
			value |= (0x1 << 12);
			writel(value, (s_pd->reset_addr + RESET2_LEVEL));
			break;
		case PM_USB:
			value = readl(s_pd->reset_addr + RESET1_LEVEL);
			value |= (0x1 << 2);
			writel(value, (s_pd->reset_addr + RESET1_LEVEL));
			break;
		case PM_PCIE0:
			value = readl(s_pd->reset_addr + RESET0_LEVEL);
			value |= ((0x1 << 12) | (0x3 << 14));
			writel(value, (s_pd->reset_addr + RESET0_LEVEL));
			break;
		case PM_GE2D:
			value = readl(s_pd->reset_addr + RESET2_LEVEL);
			value |= (0x1 << 6);
			writel(value, (s_pd->reset_addr + RESET2_LEVEL));
			break;
		case PM_PCIE1:
			value = readl(s_pd->reset_addr + RESET0_LEVEL);
			value |= (0x7 << 28);
			writel(value, (s_pd->reset_addr + RESET0_LEVEL));
			break;
		case PM_DSPA:
			value = readl(s_pd->reset_addr + RESET4_LEVEL);
			value |= 0x1;
			writel(value, (s_pd->reset_addr + RESET4_LEVEL));
			value = readl(s_pd->reset_addr + RESET1_LEVEL);
			value |= (0x1 << 20);
			writel(value, (s_pd->reset_addr + RESET1_LEVEL));
			break;
		case PM_DSPB:
			value = readl(s_pd->reset_addr + RESET4_LEVEL);
			value |= (0x1 << 1);
			writel(value, (s_pd->reset_addr + RESET4_LEVEL));
			value = readl(s_pd->reset_addr + RESET1_LEVEL);
			value |= (0x1 << 21);
			writel(value, (s_pd->reset_addr + RESET1_LEVEL));
			break;
		case PM_DEMOD:
			value = readl(s_pd->reset_addr + RESET0_LEVEL);
			value |= (0x1 << 8);
			writel(value, (s_pd->reset_addr + RESET0_LEVEL));
			break;
		}
	} else {
		switch (pwr_domain) {
		case PM_DOS_HCODEC:
			value = readl(s_pd->dos_addr + DOS_SW_RESET1);
			value |= (0xffff << 2);
			writel(value, (s_pd->dos_addr + DOS_SW_RESET1));
			break;
		case PM_DOS_VDEC:
			value = readl(s_pd->dos_addr + DOS_SW_RESET0);
			value |= (0x1fff << 2);
			writel(value, (s_pd->dos_addr + DOS_SW_RESET0));
			break;
		case PM_DOS_HEVC:
			value = readl(s_pd->dos_addr + DOS_SW_RESET3);
			value |= (0x3ffff << 2 | 1 << 24);
			writel(value, (s_pd->dos_addr + DOS_SW_RESET3));
			break;
		case PM_WAVE420L:
			value = readl(s_pd->dos_addr + DOS_SW_RESET4);
			value |= (0xf << 8);
			writel(value, (s_pd->dos_addr + DOS_SW_RESET4));
			break;
		case PM_VPU:
			tmp = 0x1 << 5 | 0x1 << 10 | 0x1 << 19 | 0x1 << 13;
			value = readl(s_pd->reset_addr + RESET0_LEVEL);
			value &= ~tmp;
			writel(value, (s_pd->reset_addr + RESET0_LEVEL));
			tmp = 0x1 << 5 | 0x1 << 4;
			value = readl(s_pd->reset_addr + RESET1_LEVEL);
			value &= ~tmp;
			writel(value, (s_pd->reset_addr + RESET1_LEVEL));
			tmp = 0x1 << 15;
			value = readl(s_pd->reset_addr + RESET2_LEVEL);
			value &= ~tmp;
			writel(value, (s_pd->reset_addr + RESET2_LEVEL));
			tmp = 0x1 << 6 | 0x1 << 7 | 0x1 << 13 |
			      0x1 << 5 | 0x1 << 9 | 0x1 << 4 | 0x1 << 12;
			value = readl(s_pd->reset_addr + RESET4_LEVEL);
			value &= ~tmp;
			writel(value, (s_pd->reset_addr + RESET4_LEVEL));
			tmp = 0x1 << 7;
			value = readl(s_pd->reset_addr + RESET7_LEVEL);
			value &= ~tmp;
			writel(value, (s_pd->reset_addr + RESET7_LEVEL));
			break;
		case PM_NN:
			value = readl(s_pd->reset_addr + RESET2_LEVEL);
			value &= ~(0x1 << 12);
			writel(value, (s_pd->reset_addr + RESET2_LEVEL));
			break;
		case PM_USB:
			value = readl(s_pd->reset_addr + RESET1_LEVEL);
			value &= ~(0x1 << 2);
			writel(value, (s_pd->reset_addr + RESET1_LEVEL));
			break;
		case PM_PCIE0:
			value = readl(s_pd->reset_addr + RESET0_LEVEL);
			value &= ~((0x1 << 12) | (0x3 << 14));
			writel(value, (s_pd->reset_addr + RESET0_LEVEL));
			break;
		case PM_GE2D:
			value = readl(s_pd->reset_addr + RESET2_LEVEL);
			value &= ~(0x1 << 6);
			writel(value, (s_pd->reset_addr + RESET2_LEVEL));
			break;
		case PM_PCIE1:
			value = readl(s_pd->reset_addr + RESET0_LEVEL);
			value &= ~(0x7 << 28);
			writel(value, (s_pd->reset_addr + RESET0_LEVEL));
			break;
		case PM_DSPA:
			value = readl(s_pd->reset_addr + RESET4_LEVEL);
			value &= ~0x1;
			writel(value, (s_pd->reset_addr + RESET4_LEVEL));
			value = readl(s_pd->reset_addr + RESET1_LEVEL);
			value &= ~(0x1 << 20);
			writel(value, (s_pd->reset_addr + RESET1_LEVEL));
			break;
		case PM_DSPB:
			value = readl(s_pd->reset_addr + RESET4_LEVEL);
			value &= ~(0x1 << 1);
			writel(value, (s_pd->reset_addr + RESET4_LEVEL));
			value = readl(s_pd->reset_addr + RESET1_LEVEL);
			value &= ~(0x1 << 21);
			writel(value, (s_pd->reset_addr + RESET1_LEVEL));
			break;
		case PM_DEMOD:
			value = readl(s_pd->reset_addr + RESET0_LEVEL);
			value &= ~(0x1 << 8);
			writel(value, (s_pd->reset_addr + RESET0_LEVEL));
			break;
		}
	}
	spin_unlock_irqrestore(&s_pd->reset_lock, flags);
}

static void iso_switch(int pwr_domain, bool pwr_switch)
{
	unsigned int value;
	unsigned long flags;

	spin_lock_irqsave(&s_pd->iso_lock, flags);
	value = readl(s_pd->ao_addr + AO_RTI_GEN_PWR_ISO0);
	if (pwr_switch == PWR_ON)
		value &= ~(1 << pwr_domain);
	else
		value |= (1 << pwr_domain);
	writel(value, (s_pd->ao_addr + AO_RTI_GEN_PWR_ISO0));
	spin_unlock_irqrestore(&s_pd->iso_lock, flags);
}

static bool check_ack(int pwr_domain)
{
	bool ack;
	unsigned int value;

	value = readl(s_pd->ao_addr + AO_RTI_GEN_PWR_ACK0);
	ack = (value & (0x1 << pwr_domain)) == (0x1 << pwr_domain) ? 1 : 0;

	return ack;
}

void power_domain_switch(int pwr_domain, bool pwr_switch)
{
	bool ack_flag;

	if (pwr_switch == PWR_ON) {
		/* Powerup Power Domain */
		power_switch(pwr_domain, PWR_ON);
		usleep_range(40, 50);

		/* Powerup memories */
		mem_pd_switch(pwr_domain, PWR_ON);

		do {
			ack_flag = check_ack(pwr_domain);
		} while (ack_flag);
		usleep_range(100, 150);

		reset_switch(pwr_domain, PWR_OFF);

		/* remove isolations */
		iso_switch(pwr_domain, PWR_ON);

		/* deassert reset */
		reset_switch(pwr_domain, PWR_ON);

	} else {
		/* reset */
		reset_switch(pwr_domain, PWR_OFF);

		/* add isolation to domain */
		iso_switch(pwr_domain, PWR_OFF);

		/* Power down memories */
		mem_pd_switch(pwr_domain, PWR_OFF);
		usleep_range(40, 50);

		/* Power off  domain */
		power_switch(pwr_domain, PWR_OFF);
	}
}
EXPORT_SYMBOL(power_domain_switch);

static int pd_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct power_domains *power_domains;
	int ret;
	u32 offset;

	power_domains = devm_kzalloc(&pdev->dev, sizeof(*power_domains),
				     GFP_KERNEL);
	if (!power_domains)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "Fail to get dos addr res\n");
		return -ENXIO;
	}

	power_domains->dos_addr = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(power_domains->dos_addr))
		return PTR_ERR(power_domains->dos_addr);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!res) {
		dev_err(&pdev->dev, "Fail to get ao addr res\n");
		return -ENXIO;
	}

	power_domains->ao_addr = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(power_domains->ao_addr))
		return PTR_ERR(power_domains->ao_addr);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	if (!res) {
		dev_err(&pdev->dev, "Fail to get mempd addr res\n");
		return -ENXIO;
	}

	power_domains->mempd_addr = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(power_domains->mempd_addr))
		return PTR_ERR(power_domains->mempd_addr);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 3);
	if (!res) {
		dev_err(&pdev->dev, "Fail to get reset addr res\n");
		return -ENXIO;
	}

	power_domains->reset_addr = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(power_domains->reset_addr))
		return PTR_ERR(power_domains->reset_addr);

	ret = of_property_read_u32(pdev->dev.of_node,
				   "vpu_mempd_reg3", &offset);
	if (!ret) {
		pr_info("vpu_mempd_reg3: 0x%x\n", offset);
		vpu_mem_pd_reg3 = offset;
	}

	ret = of_property_read_u32(pdev->dev.of_node,
				   "vpu_mempd_reg4", &offset);
	if (!ret) {
		pr_info("vpu_mempd_reg4: 0x%x\n", offset);
		vpu_mem_pd_reg4 = offset;
	}

	spin_lock_init(&power_domains->power_lock);
	spin_lock_init(&power_domains->mem_pd_lock);
	spin_lock_init(&power_domains->reset_lock);
	spin_lock_init(&power_domains->iso_lock);

	s_pd = power_domains;
	probe_done = 1;

	return 0;
}

static const struct of_device_id pd_match_table[] = {
	{ .compatible = "amlogic,tm2-power-domain", },
	{ .compatible = "amlogic,sm1-power-domain", },
	{}
};

static struct platform_driver pd_driver = {
	.driver = {
		.name = "amlogic,power-domain",
		.of_match_table = pd_match_table,
	},
	.probe = pd_probe,
};

static int __init pd_init(void)
{
	return platform_driver_register(&pd_driver);
}
arch_initcall(pd_init);
