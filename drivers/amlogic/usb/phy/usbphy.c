/*
 *
 * arch/arm/mach-meson6/usbclock.c
 *
 *  Copyright (C) 2013 AMLOGIC, INC.
 *
 *	by Victor Wan 2013.3 @Shanghai
 *
 * License terms: GNU General Public License (GPL) version 2
 * Platform machine definition.
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
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
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

static int init_count;

int clk_enable_usb_meson8(struct platform_device *pdev,
			const char *s_clock_name, u32 usb_peri_reg)
{
	int port_idx;
	const char *clk_name;
	usb_peri_reg_t *peri;
	usb_config_data_t config;
	usb_ctrl_data_t control;
	usb_adp_bc_data_t adp_bc;
	int clk_sel, clk_div, clk_src;
	int time_dly = 500;
	int i = 0;
	struct reset_control *usb_reset;

	if (!init_count) {
		init_count++;
		aml_cbus_update_bits(0x1102, 0x1<<2, 0x1<<2);
		for (i = 0; i < 1000; i++)
			udelay(time_dly);
	}

	clk_name = s_clock_name;

	if (!strcmp(clk_name, "usb0")) {
		usb_reset = devm_reset_control_get(&pdev->dev, "usb_general");
		reset_control_deassert(usb_reset);
		usb_reset = devm_reset_control_get(&pdev->dev, "usb0");
		reset_control_deassert(usb_reset);
		usb_reset = devm_reset_control_get(&pdev->dev, "usb0_to_ddr");
		reset_control_deassert(usb_reset);

		peri = (usb_peri_reg_t *)usb_peri_reg;
		port_idx = USB_PORT_IDX_A;
	} else if (!strcmp(clk_name, "usb1")) {
		usb_reset = devm_reset_control_get(&pdev->dev, "usb_general");
		reset_control_deassert(usb_reset);
		usb_reset = devm_reset_control_get(&pdev->dev, "usb1");
		reset_control_deassert(usb_reset);
		usb_reset = devm_reset_control_get(&pdev->dev, "usb1_to_ddr");
		reset_control_deassert(usb_reset);

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

	dmb();

	return 0;
}

void clk_disable_usb_meson8(struct platform_device *pdev,
				const char *s_clock_name)
{
	return;
}

int clk_enable_usb(struct platform_device *pdev, const char *s_clock_name,
				u32 usb_peri_reg, const char *cpu_type)
{
	int ret = 0;

	if (!pdev)
		return -1;

	if (!strcmp(cpu_type, MESON8))
		ret = clk_enable_usb_meson8(pdev, s_clock_name, usb_peri_reg);

	/*add other cpu type's usb clock enable*/

	return ret;
}
EXPORT_SYMBOL(clk_enable_usb);

int clk_disable_usb(struct platform_device *pdev,
			const char *s_clock_name, const char *cpu_type)
{
	if (!pdev)
		return -1;

	if (!strcmp(cpu_type, MESON8))
			clk_disable_usb_meson8(pdev, s_clock_name);

	dmb();
	return 0;
}
EXPORT_SYMBOL(clk_disable_usb);
