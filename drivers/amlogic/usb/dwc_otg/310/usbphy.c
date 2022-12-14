// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/of_gpio.h>
#include <linux/amlogic/usbtype.h>
#include <linux/reset.h>
#include <linux/platform_device.h>
#include <linux/amlogic/usb-gxl.h>
#include <linux/amlogic/usb-v2.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>

struct clk_reset {
	struct clk *usb_reset_usb_general;
	struct clk *usb_reset_usb;
	struct clk *usb_reset_usb_to_ddr;
};

struct clk_reset p_clk_reset[4];

int device_status(unsigned long usb_peri_reg)
{
	struct u2p_aml_regs_t u2p_aml_regs;
	union u2p_r2_t reg2;
	int ret = 1;

	u2p_aml_regs.u2p_r[2] = (void __iomem	*)
				((unsigned long)usb_peri_reg +
					PHY_REGISTER_SIZE + 0x8);
	reg2.d32 = readl(u2p_aml_regs.u2p_r[2]);
	if (!reg2.b.device_sess_vld)
		ret = 0;

	return ret;
}
EXPORT_SYMBOL(device_status);

/* ret 1: device plug in */
/* ret 0: device plug out */
int device_status_v2(unsigned long usb_peri_reg)
{
	struct u2p_aml_regs_v2 u2p_aml_regs;
	union u2p_r1_v2 reg1;
	int ret = 1;

	u2p_aml_regs.u2p_r_v2[1] = (void __iomem	*)
				((unsigned long)usb_peri_reg +
					PHY_REGISTER_SIZE + 0x4);
	reg1.d32 = readl(u2p_aml_regs.u2p_r_v2[1]);
	if (!reg1.b.OTGSESSVLD0)
		ret = 0;

	return ret;
}
EXPORT_SYMBOL(device_status_v2);

static void set_device_mode(struct platform_device *pdev,
				unsigned long reg_addr, int controller_type)
{
	struct u2p_aml_regs_t u2p_aml_regs;
	struct usb_aml_regs_t usb_aml_regs;
	union u2p_r0_t reg0;
	union usb_r0_t r0 = {.d32 = 0};
	union usb_r1_t r1 = {.d32 = 0};
	union usb_r4_t r4 = {.d32 = 0};

	u2p_aml_regs.u2p_r[0] = (void __iomem	*)
				((unsigned long)reg_addr + PHY_REGISTER_SIZE);
	reg0.d32 = readl(u2p_aml_regs.u2p_r[0]);
	reg0.b.dmpulldown = 0;
	reg0.b.dppulldown = 0;
	writel(reg0.d32, u2p_aml_regs.u2p_r[0]);

	usb_aml_regs.usb_r[0] = (void __iomem *)
				((unsigned long)reg_addr + 4 * PHY_REGISTER_SIZE
				+ 4 * 0);
	usb_aml_regs.usb_r[1] = (void __iomem *)
				((unsigned long)reg_addr + 4 * PHY_REGISTER_SIZE
				+ 4 * 1);
	usb_aml_regs.usb_r[4] = (void __iomem *)
				((unsigned long)reg_addr + 4 * PHY_REGISTER_SIZE
				+ 4 * 4);
	r0.d32 = readl(usb_aml_regs.usb_r[0]);
	r0.b.u2d_act = 1;
	writel(r0.d32, usb_aml_regs.usb_r[0]);

	r4.d32 = readl(usb_aml_regs.usb_r[4]);
	r4.b.p21_SLEEPM0 = 0x1;
	writel(r4.d32, usb_aml_regs.usb_r[4]);

	if (controller_type != USB_OTG) {
		r1.d32 = readl(usb_aml_regs.usb_r[1]);
		r1.b.u3h_host_u2_port_disable = 0x2;
		writel(r1.d32, usb_aml_regs.usb_r[1]);
	}
}

static void set_device_mode_v2(struct platform_device *pdev,
				unsigned long reg_addr, int controller_type)
{
	struct u2p_aml_regs_v2 u2p_aml_regs;
	struct usb_aml_regs_v2 usb_aml_regs;
	union u2p_r0_v2 reg0;
	union usb_r0_v2 r0 = {.d32 = 0};
	union usb_r1_v2 r1 = {.d32 = 0};
	union usb_r4_v2 r4 = {.d32 = 0};

	force_disable_xhci_port_a();

	u2p_aml_regs.u2p_r_v2[0] = (void __iomem	*)
				((unsigned long)reg_addr + PHY_REGISTER_SIZE);
	usb_aml_regs.usb_r_v2[0] = (void __iomem *)
				((unsigned long)reg_addr + 4*PHY_REGISTER_SIZE
				+ 4*0);
	usb_aml_regs.usb_r_v2[1] = (void __iomem *)
				((unsigned long)reg_addr + 4*PHY_REGISTER_SIZE
				+ 4*1);
	usb_aml_regs.usb_r_v2[4] = (void __iomem *)
				((unsigned long)reg_addr + 4*PHY_REGISTER_SIZE
				+ 4*4);
	r0.d32 = readl(usb_aml_regs.usb_r_v2[0]);
	r0.b.u2d_act = 1;
	r0.b.u2d_ss_scaledown_mode = 0;
	writel(r0.d32, usb_aml_regs.usb_r_v2[0]);

	r4.d32 = readl(usb_aml_regs.usb_r_v2[4]);
	r4.b.p21_SLEEPM0 = 0x1;
	writel(r4.d32, usb_aml_regs.usb_r_v2[4]);

	if (controller_type != USB_OTG) {
		r1.d32 = readl(usb_aml_regs.usb_r_v2[1]);
		r1.b.u3h_host_u2_port_disable = 0x2;
		writel(r1.d32, usb_aml_regs.usb_r_v2[1]);
	}

	reg0.d32 = readl(u2p_aml_regs.u2p_r_v2[0]);
	reg0.b.host_device = 0;
	reg0.b.POR = 0;
	writel(reg0.d32, u2p_aml_regs.u2p_r_v2[0]);
}


int clk_enable_usb_gxl(struct platform_device *pdev,
			const char *s_clock_name, unsigned long usb_peri_reg,
			int controller_type)
{
	struct clk *usb_reset;

	usb_reset = devm_clk_get(&pdev->dev, "usb_general");
	clk_prepare_enable(usb_reset);
	p_clk_reset[pdev->id].usb_reset_usb_general = usb_reset;
	usb_reset = devm_clk_get(&pdev->dev, "usb1");
	clk_prepare_enable(usb_reset);
	p_clk_reset[pdev->id].usb_reset_usb_to_ddr = usb_reset;
	set_device_mode(pdev, usb_peri_reg, controller_type);
	return 0;
}

int clk_enable_usb_v2(struct platform_device *pdev,
			const char *s_clock_name, unsigned long usb_peri_reg,
			int controller_type)
{
	struct clk *usb_reset;

	usb_reset = devm_clk_get(&pdev->dev, "usb_general");
	clk_prepare_enable(usb_reset);
	p_clk_reset[pdev->id].usb_reset_usb_general = usb_reset;
	usb_reset = devm_clk_get(&pdev->dev, "usb1");
	clk_prepare_enable(usb_reset);
	p_clk_reset[pdev->id].usb_reset_usb_to_ddr = usb_reset;
	set_device_mode_v2(pdev, usb_peri_reg, controller_type);
	return 0;
}


void clk_disable_usb_gxl(struct platform_device *pdev,
				const char *s_clock_name,
				unsigned long usb_peri_reg)
{
	struct clk *usb_reset;

	usb_reset = p_clk_reset[pdev->id].usb_reset_usb_general;
	clk_disable_unprepare(usb_reset);
	usb_reset = p_clk_reset[pdev->id].usb_reset_usb_to_ddr;
	clk_disable_unprepare(usb_reset);
}

void clk_disable_usb_v2(struct platform_device *pdev,
				const char *s_clock_name,
				unsigned long usb_peri_reg)
{
	struct clk *usb_reset;

	usb_reset = p_clk_reset[pdev->id].usb_reset_usb_general;
	clk_disable_unprepare(usb_reset);
	usb_reset = p_clk_reset[pdev->id].usb_reset_usb_to_ddr;
	clk_disable_unprepare(usb_reset);
}

int clk_resume_usb_gxl(struct platform_device *pdev,
			const char *s_clock_name,
			unsigned long usb_peri_reg)
{
	struct clk *usb_reset;

	if (pdev->id == 0) {
		usb_reset = p_clk_reset[pdev->id].usb_reset_usb_general;
		clk_prepare_enable(usb_reset);
		usb_reset = p_clk_reset[pdev->id].usb_reset_usb_to_ddr;
		clk_prepare_enable(usb_reset);
	} else if (pdev->id == 1) {
		usb_reset = p_clk_reset[pdev->id].usb_reset_usb_general;
		clk_prepare_enable(usb_reset);
		usb_reset = p_clk_reset[pdev->id].usb_reset_usb_to_ddr;
		clk_prepare_enable(usb_reset);
	} else {
		dev_err(&pdev->dev, "bad usb clk name.\n");
		return -1;
	}

	dmb(4);

	return 0;
}

int clk_resume_usb_v2(struct platform_device *pdev,
			const char *s_clock_name,
			unsigned long usb_peri_reg)
{
	struct clk *usb_reset;

	if (pdev->id == 0) {
		usb_reset = p_clk_reset[pdev->id].usb_reset_usb_general;
		clk_prepare_enable(usb_reset);
		usb_reset = p_clk_reset[pdev->id].usb_reset_usb_to_ddr;
		clk_prepare_enable(usb_reset);
	} else if (pdev->id == 1) {
		usb_reset = p_clk_reset[pdev->id].usb_reset_usb_general;
		clk_prepare_enable(usb_reset);
		usb_reset = p_clk_reset[pdev->id].usb_reset_usb_to_ddr;
		clk_prepare_enable(usb_reset);
	} else {
		dev_err(&pdev->dev, "bad usb clk name.\n");
		return -1;
	}

	dmb(4);

	return 0;
}


int clk_enable_usb(struct platform_device *pdev, const char *s_clock_name,
		unsigned long usb_peri_reg, const char *cpu_type,
		int controller_type)
{
	int ret = 0;

	if (!pdev)
		return -1;

	if (!strcmp(cpu_type, V2))
		ret = clk_enable_usb_v2(pdev,
				s_clock_name, usb_peri_reg, controller_type);
	else if (!strcmp(cpu_type, GXL))
		ret = clk_enable_usb_gxl(pdev,
				s_clock_name, usb_peri_reg, controller_type);
	/*add other cpu type's usb clock enable*/

	return ret;
}
EXPORT_SYMBOL(clk_enable_usb);

int clk_disable_usb(struct platform_device *pdev, const char *s_clock_name,
		unsigned long usb_peri_reg,
		const char *cpu_type)
{
	if (!pdev)
		return -1;

	if (!strcmp(cpu_type, V2))
		clk_disable_usb_v2(pdev,
			s_clock_name, usb_peri_reg);
	else if (!strcmp(cpu_type, GXL))
		clk_disable_usb_gxl(pdev,
			s_clock_name, usb_peri_reg);

	dmb(4);
	return 0;
}
EXPORT_SYMBOL(clk_disable_usb);

int clk_resume_usb(struct platform_device *pdev, const char *s_clock_name,
		unsigned long usb_peri_reg,
		const char *cpu_type)
{
	int ret = 0;

	if (!pdev)
		return -1;

	if (!strcmp(cpu_type, V2))
		ret = clk_resume_usb_v2(pdev,
			s_clock_name, usb_peri_reg);
	else if (!strcmp(cpu_type, GXL))
		ret = clk_resume_usb_gxl(pdev,
			s_clock_name, usb_peri_reg);

	/*add other cpu type's usb clock enable*/

	return ret;
}
EXPORT_SYMBOL(clk_resume_usb);

int clk_suspend_usb(struct platform_device *pdev, const char *s_clock_name,
		unsigned long usb_peri_reg,
		const char *cpu_type)
{
	if (!pdev)
		return -1;

	if (!strcmp(cpu_type, V2))
		clk_disable_usb_v2(pdev,
			s_clock_name, usb_peri_reg);
	else if (!strcmp(cpu_type, GXL))
		clk_disable_usb_gxl(pdev,
			s_clock_name, usb_peri_reg);

	dmb(4);
	return 0;
}
EXPORT_SYMBOL(clk_suspend_usb);
