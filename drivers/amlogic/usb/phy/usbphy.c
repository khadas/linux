/*
 * drivers/amlogic/usb/phy/usbphy.c
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
/*#include <asm/mach-types.h>
#include <asm/mach/arch.h>*/
#include <linux/delay.h>
#include <linux/amlogic/gpio-amlogic.h>
#include <linux/amlogic/usbtype.h>
#include <linux/amlogic/iomap.h>
#include <linux/reset.h>
#include <linux/platform_device.h>
#include <linux/amlogic/usb-meson8.h>
#include <linux/amlogic/cpu_version.h>

/*
 * M chip USB clock setting
 */

#define MESON8  "meson8"
#define G9TV    "g9TV"
#define GXBABY	"gxbaby"

static int init_count;

struct clk_reset {
	struct reset_control *usb_reset_usb_general;
	struct reset_control *usb_reset_usb;
	struct reset_control *usb_reset_usb_to_ddr;
};

struct clk_reset p_clk_reset[4];

int clk_enable_usb_meson8(struct platform_device *pdev,
			const char *s_clock_name, unsigned long usb_peri_reg)
{
	int port_idx;
	const char *clk_name;
	usb_peri_reg_t *peri;
	usb_config_data_t config;
	usb_ctrl_data_t control;
	usb_adp_bc_data_t adp_bc;
	int clk_sel, clk_div, clk_src;
	int time_dly = 500;
	/*int i = 0;*/
	struct reset_control *usb_reset;

	if (!init_count) {
		init_count++;
		aml_cbus_update_bits(0x1102, 0x1<<2, 0x1<<2);
		/*for (i = 0; i < 1000; i++)
			udelay(time_dly);*/
	}

	clk_name = s_clock_name;

	if (!strcmp(clk_name, "usb0")) {
		usb_reset = devm_reset_control_get(&pdev->dev, "usb_general");
		reset_control_deassert(usb_reset);
		p_clk_reset[pdev->id].usb_reset_usb_general = usb_reset;
		usb_reset = devm_reset_control_get(&pdev->dev, "usb0");
		reset_control_deassert(usb_reset);
		p_clk_reset[pdev->id].usb_reset_usb = usb_reset;
		usb_reset = devm_reset_control_get(&pdev->dev, "usb0_to_ddr");
		reset_control_deassert(usb_reset);
		p_clk_reset[pdev->id].usb_reset_usb_to_ddr = usb_reset;

		peri = (usb_peri_reg_t *)usb_peri_reg;
		port_idx = USB_PORT_IDX_A;
	} else if (!strcmp(clk_name, "usb1")) {
		usb_reset = devm_reset_control_get(&pdev->dev, "usb_general");
		reset_control_deassert(usb_reset);
		p_clk_reset[pdev->id].usb_reset_usb_general = usb_reset;
		usb_reset = devm_reset_control_get(&pdev->dev, "usb1");
		reset_control_deassert(usb_reset);
		p_clk_reset[pdev->id].usb_reset_usb = usb_reset;
		usb_reset = devm_reset_control_get(&pdev->dev, "usb1_to_ddr");
		reset_control_deassert(usb_reset);
		p_clk_reset[pdev->id].usb_reset_usb_to_ddr = usb_reset;

		peri = (usb_peri_reg_t *)usb_peri_reg;
		port_idx = USB_PORT_IDX_B;
	} else {
		dev_err(&pdev->dev, "bad usb clk name: %s\n", clk_name);
		return -1;
	}

	clk_sel = USB_PHY_CLK_SEL_XTAL;
	clk_div = 1;
	clk_src = 24000000;

	config.d32 = peri->config;
	config.b.clk_32k_alt_sel = 1;
	peri->config = config.d32;

	control.d32 = peri->ctrl;
	control.b.fsel = 5;
	control.b.por = 1;
	peri->ctrl = control.d32;
	udelay(time_dly);
	control.b.por = 0;
	peri->ctrl = control.d32;
	udelay(time_dly);

	/* read back clock detected flag*/
	control.d32 = peri->ctrl;

	if (is_meson_m8m2_cpu() && (port_idx == USB_PORT_IDX_B)) {
		adp_bc.d32 = peri->adp_bc;
		adp_bc.b.aca_enable = 1;
		peri->adp_bc = adp_bc.d32;
		udelay(50);
		adp_bc.d32 = peri->adp_bc;
		if (adp_bc.b.aca_pin_float) {
			dev_err(&pdev->dev, "USB-B ID detect failed!\n");
			dev_err(&pdev->dev, "Please use the chip after version RevA1!\n");
			return -1;
		}
	}

	dmb(4);

	return 0;
}

void clk_disable_usb_meson8(struct platform_device *pdev,
				const char *s_clock_name,
				unsigned long usb_peri_reg)
{
	return;
}

int clk_resume_usb_meson8(struct platform_device *pdev,
			const char *s_clock_name,
			unsigned long usb_peri_reg)
{
	const char *clk_name;
	struct reset_control *usb_reset;

	clk_name = s_clock_name;

	if (0 == pdev->id) {
		usb_reset = p_clk_reset[pdev->id].usb_reset_usb_general;
		reset_control_deassert(usb_reset);
		usb_reset = p_clk_reset[pdev->id].usb_reset_usb;
		reset_control_deassert(usb_reset);
		usb_reset = p_clk_reset[pdev->id].usb_reset_usb_to_ddr;
		reset_control_deassert(usb_reset);
	} else if (1 == pdev->id) {
		usb_reset = p_clk_reset[pdev->id].usb_reset_usb_general;
		reset_control_deassert(usb_reset);
		usb_reset = p_clk_reset[pdev->id].usb_reset_usb;
		reset_control_deassert(usb_reset);
		usb_reset = p_clk_reset[pdev->id].usb_reset_usb_to_ddr;
		reset_control_deassert(usb_reset);
	} else {
		dev_err(&pdev->dev, "bad usb clk name: %s\n", clk_name);
		return -1;
	}

	dmb(4);

	return 0;
}


int clk_enable_usb_gxbaby(struct platform_device *pdev,
			const char *s_clock_name,
			unsigned long usb_peri_reg)
{
	int port_idx;
	const char *clk_name;
	usb_peri_reg_t *peri;
	usb_config_data_t config;
	usb_ctrl_data_t control;
	usb_adp_bc_data_t adp_bc;
	int clk_sel, clk_div, clk_src;
	int time_dly = 500;
	/*int i = 0;*/
	struct reset_control *usb_reset;

	if (!init_count) {
		init_count++;
		aml_cbus_update_bits(0x1102, 0x1<<2, 0x1<<2);
		/*for (i = 0; i < 1000; i++)
			udelay(time_dly);*/
	}

	clk_name = s_clock_name;

	if (!strcmp(clk_name, "usb0")) {
		usb_reset = devm_reset_control_get(&pdev->dev, "usb_general");
		reset_control_deassert(usb_reset);
		p_clk_reset[pdev->id].usb_reset_usb_general = usb_reset;
		usb_reset = devm_reset_control_get(&pdev->dev, "usb0");
		reset_control_deassert(usb_reset);
		p_clk_reset[pdev->id].usb_reset_usb = usb_reset;
		usb_reset = devm_reset_control_get(&pdev->dev, "usb0_to_ddr");
		reset_control_deassert(usb_reset);
		p_clk_reset[pdev->id].usb_reset_usb_to_ddr = usb_reset;

		peri = (usb_peri_reg_t *)usb_peri_reg;
		port_idx = USB_PORT_IDX_A;
	} else if (!strcmp(clk_name, "usb1")) {
		usb_reset = devm_reset_control_get(&pdev->dev, "usb_general");
		reset_control_deassert(usb_reset);
		p_clk_reset[pdev->id].usb_reset_usb_general = usb_reset;
		usb_reset = devm_reset_control_get(&pdev->dev, "usb1");
		reset_control_deassert(usb_reset);
		p_clk_reset[pdev->id].usb_reset_usb = usb_reset;
		usb_reset = devm_reset_control_get(&pdev->dev, "usb1_to_ddr");
		reset_control_deassert(usb_reset);
		p_clk_reset[pdev->id].usb_reset_usb_to_ddr = usb_reset;

		peri = (usb_peri_reg_t *)usb_peri_reg;
		port_idx = USB_PORT_IDX_B;
	} else {
		dev_err(&pdev->dev, "bad usb clk name: %s\n", clk_name);
		return -1;
	}

	clk_sel = USB_PHY_CLK_SEL_XTAL;
	clk_div = 1;
	clk_src = 24000000;


	config.d32 = peri->config;
	config.b.clk_32k_alt_sel = 1;
	peri->config = config.d32;

	control.d32 = peri->ctrl;
	control.b.fsel = 5;
	control.b.por = 1;
	peri->ctrl = control.d32;
	udelay(time_dly);
	control.b.por = 0;
	peri->ctrl = control.d32;
	udelay(time_dly);

	/* read back clock detected flag*/
	control.d32 = peri->ctrl;

	udelay(time_dly);

	/*to do. set usb port config*/
	if (port_idx == USB_PORT_IDX_B) {
		adp_bc.d32 = peri->adp_bc;
		adp_bc.b.aca_enable = 1;
		peri->adp_bc = adp_bc.d32;
		udelay(50);
		adp_bc.d32 = peri->adp_bc;
		if (adp_bc.b.aca_pin_float) {
			dev_err(&pdev->dev, "USB-B ID detect failed!\n");
			dev_err(&pdev->dev, "Please use the chip after version RevA1!\n");
			return -1;
		}
	}
	dmb(4);

	return 0;
}


void clk_disable_usb_gxbaby(struct platform_device *pdev,
				const char *s_clock_name,
				unsigned long usb_peri_reg)
{
	struct reset_control *usb_reset;

	if (0 == pdev->id) {
		usb_reset = p_clk_reset[pdev->id].usb_reset_usb_general;
		reset_control_assert(usb_reset);
		usb_reset = p_clk_reset[pdev->id].usb_reset_usb;
		reset_control_assert(usb_reset);
		usb_reset = p_clk_reset[pdev->id].usb_reset_usb_to_ddr;
		reset_control_assert(usb_reset);
	} else if (1 == pdev->id) {
		usb_reset = p_clk_reset[pdev->id].usb_reset_usb_general;
		reset_control_assert(usb_reset);
		usb_reset = p_clk_reset[pdev->id].usb_reset_usb;
		reset_control_assert(usb_reset);
		usb_reset = p_clk_reset[pdev->id].usb_reset_usb_to_ddr;
		reset_control_assert(usb_reset);
	} else {
		dev_err(&pdev->dev, "bad usb clk name.\n");
		return;
	}

	return;
}

int clk_resume_usb_gxbaby(struct platform_device *pdev,
			const char *s_clock_name,
			unsigned long usb_peri_reg)
{
	struct reset_control *usb_reset;

	if (0 == pdev->id) {
		usb_reset = p_clk_reset[pdev->id].usb_reset_usb_general;
		reset_control_deassert(usb_reset);
		usb_reset = p_clk_reset[pdev->id].usb_reset_usb;
		reset_control_deassert(usb_reset);
		usb_reset = p_clk_reset[pdev->id].usb_reset_usb_to_ddr;
		reset_control_deassert(usb_reset);
	} else if (1 == pdev->id) {
		usb_reset = p_clk_reset[pdev->id].usb_reset_usb_general;
		reset_control_deassert(usb_reset);
		usb_reset = p_clk_reset[pdev->id].usb_reset_usb;
		reset_control_deassert(usb_reset);
		usb_reset = p_clk_reset[pdev->id].usb_reset_usb_to_ddr;
		reset_control_deassert(usb_reset);
	} else {
		dev_err(&pdev->dev, "bad usb clk name.\n");
		return -1;
	}

	dmb(4);

	return 0;
}

int clk_enable_usb(struct platform_device *pdev, const char *s_clock_name,
		unsigned long usb_peri_reg,
		const char *cpu_type)
{
	int ret = 0;

	if (!pdev)
		return -1;

	if (!strcmp(cpu_type, MESON8))
		ret = clk_enable_usb_meson8(pdev,
				s_clock_name, usb_peri_reg);
	else if (!strcmp(cpu_type, GXBABY))
		ret = clk_enable_usb_gxbaby(pdev,
				s_clock_name, usb_peri_reg);

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

	if (!strcmp(cpu_type, MESON8))
			clk_disable_usb_meson8(pdev,
				s_clock_name, usb_peri_reg);

	if (!strcmp(cpu_type, GXBABY))
			clk_disable_usb_gxbaby(pdev,
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

	if (!strcmp(cpu_type, MESON8))
		ret = clk_resume_usb_meson8(pdev,
			s_clock_name, usb_peri_reg);
	else if (!strcmp(cpu_type, GXBABY))
		ret = clk_resume_usb_gxbaby(pdev,
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

	if (!strcmp(cpu_type, MESON8))
			clk_disable_usb_meson8(pdev,
				s_clock_name, usb_peri_reg);

	if (!strcmp(cpu_type, GXBABY))
			clk_disable_usb_gxbaby(pdev,
				s_clock_name, usb_peri_reg);

	dmb(4);
	return 0;
}
EXPORT_SYMBOL(clk_suspend_usb);
