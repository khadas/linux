// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 *
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#include <linux/init.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/of.h>
#include <linux/reset.h>
#include <linux/sched/clock.h>
#ifdef CONFIG_AMLOGIC_VPU
#include <linux/amlogic/media/vpu/vpu.h>
#endif
#include <linux/amlogic/media/vout/vinfo.h>
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#include <linux/amlogic/media/vout/lcd/lcd_notify.h>
#include "lcd_tv.h"
#include "../lcd_reg.h"
#include "../lcd_common.h"

static int lcd_type_supported(struct lcd_config_s *pconf)
{
	int lcd_type = pconf->basic.lcd_type;
	int ret = -1;

	switch (lcd_type) {
	case LCD_LVDS:
	case LCD_VBYONE:
	case LCD_MLVDS:
	case LCD_P2P:
		ret = 0;
		break;
	default:
		LCDERR("invalid lcd type: %s(%d)\n",
		       lcd_type_type_to_str(lcd_type), lcd_type);
		break;
	}
	return ret;
}

void lcd_tv_driver_init_pre(struct aml_lcd_drv_s *pdrv)
{
	int ret;

	LCDPR("[%d]: tv driver init(ver %s): %s\n",
	      pdrv->index, LCD_DRV_VERSION,
	      lcd_type_type_to_str(pdrv->config.basic.lcd_type));
	ret = lcd_type_supported(&pdrv->config);
	if (ret)
		return;

#ifdef CONFIG_AMLOGIC_VPU
	vpu_dev_clk_request(pdrv->lcd_vpu_dev, pdrv->config.timing.enc_clk);
	vpu_dev_mem_power_on(pdrv->lcd_vpu_dev);
#endif
	lcd_clk_gate_switch(pdrv, 1);

	/* init driver */
	switch (pdrv->config.basic.lcd_type) {
	case LCD_VBYONE:
		lcd_vbyone_interrupt_enable(pdrv, 0);
		break;
	default:
		break;
	}

	lcd_set_clk(pdrv);
	lcd_set_venc(pdrv);
	pdrv->mute_state = 1;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s finished\n", pdrv->index, __func__);
}

void lcd_tv_driver_disable_post(struct aml_lcd_drv_s *pdrv)
{
	lcd_venc_enable(pdrv, 0); /* disable encl */

	lcd_disable_clk(pdrv);
	lcd_clk_gate_switch(pdrv, 0);
#ifdef CONFIG_AMLOGIC_VPU
	vpu_dev_mem_power_down(pdrv->lcd_vpu_dev);
	vpu_dev_clk_release(pdrv->lcd_vpu_dev);
#endif

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s finished\n", pdrv->index, __func__);
}

int lcd_tv_driver_init(struct aml_lcd_drv_s *pdrv)
{
	int ret;
	unsigned long long local_time[3];

	local_time[0] = sched_clock();

	ret = lcd_type_supported(&pdrv->config);
	if (ret)
		return -1;

	switch (pdrv->config.basic.lcd_type) {
	case LCD_LVDS:
		lcd_lvds_dphy_set(pdrv, 1);
		lcd_lvds_enable(pdrv);
		lcd_phy_set(pdrv, LCD_PHY_ON);
		break;
	case LCD_VBYONE:
		lcd_vbyone_pinmux_set(pdrv, 1);
		lcd_vbyone_dphy_set(pdrv, 1);
		lcd_vbyone_enable(pdrv);
		lcd_vbyone_wait_hpd(pdrv);
		lcd_phy_set(pdrv, LCD_PHY_ON);
		lcd_vbyone_power_on_wait_stable(pdrv);
		break;
	case LCD_MLVDS:
		lcd_mlvds_dphy_set(pdrv, 1);
		lcd_tcon_enable(pdrv);
		lcd_mlvds_pinmux_set(pdrv, 1);
		lcd_phy_set(pdrv, LCD_PHY_ON);
		break;
	case LCD_P2P:
		lcd_p2p_pinmux_set(pdrv, 1);
		lcd_phy_set(pdrv, LCD_PHY_ON);
		lcd_p2p_dphy_set(pdrv, 1);
		lcd_tcon_enable(pdrv);
		break;
	default:
		break;
	}

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s finished\n", pdrv->index, __func__);
	local_time[1] = sched_clock();
	pdrv->config.cus_ctrl.driver_init_time = local_time[1] - local_time[0];
	return 0;
}

void lcd_tv_driver_disable(struct aml_lcd_drv_s *pdrv)
{
	int ret;
	unsigned long long local_time[3];

	local_time[0] = sched_clock();

	LCDPR("[%d]: disable driver\n", pdrv->index);
	ret = lcd_type_supported(&pdrv->config);
	if (ret)
		return;

	switch (pdrv->config.basic.lcd_type) {
	case LCD_LVDS:
		lcd_phy_set(pdrv, LCD_PHY_OFF);
		lcd_lvds_disable(pdrv);
		lcd_lvds_dphy_set(pdrv, 0);
		break;
	case LCD_VBYONE:
		lcd_vbyone_link_maintain_clear();
		lcd_vbyone_interrupt_enable(pdrv, 0);
		lcd_phy_set(pdrv, LCD_PHY_OFF);
		lcd_vbyone_pinmux_set(pdrv, 0);
		lcd_vbyone_disable(pdrv);
		lcd_vbyone_dphy_set(pdrv, 0);
		break;
	case LCD_MLVDS:
		lcd_tcon_disable(pdrv);
		lcd_mlvds_dphy_set(pdrv, 0);
		lcd_phy_set(pdrv, LCD_PHY_OFF);
		lcd_mlvds_pinmux_set(pdrv, 0);
		break;
	case LCD_P2P:
		lcd_tcon_disable(pdrv);
		lcd_p2p_dphy_set(pdrv, 0);
		lcd_phy_set(pdrv, LCD_PHY_OFF);
		lcd_p2p_pinmux_set(pdrv, 0);
		break;
	default:
		break;
	}

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s finished\n", pdrv->index, __func__);
	local_time[1] = sched_clock();
	pdrv->config.cus_ctrl.driver_disable_time = local_time[1] - local_time[0];
}

int lcd_tv_driver_change(struct aml_lcd_drv_s *pdrv)
{
	int ret;
	unsigned long long local_time[3];

	local_time[0] = sched_clock();

	LCDPR("[%d]: tv driver change(ver %s): %s\n",
	      pdrv->index, LCD_DRV_VERSION,
	      lcd_type_type_to_str(pdrv->config.basic.lcd_type));
	ret = lcd_type_supported(&pdrv->config);
	if (ret)
		return -1;

#ifdef CONFIG_AMLOGIC_VPU
	vpu_dev_clk_request(pdrv->lcd_vpu_dev, pdrv->config.timing.enc_clk);
#endif

	if (pdrv->status & LCD_STATUS_ENCL_ON) {
		if (pdrv->config.basic.lcd_type == LCD_VBYONE) {
			if (pdrv->status & LCD_STATUS_IF_ON)
				lcd_vbyone_interrupt_enable(pdrv, 0);
		}

		lcd_clk_change(pdrv);
		lcd_venc_change(pdrv);

		if (pdrv->config.basic.lcd_type == LCD_VBYONE) {
			if (pdrv->status & LCD_STATUS_IF_ON)
				lcd_vbyone_wait_stable(pdrv);
		}
	} else {
		/* only change parameters when panel is off */
		switch (pdrv->config.timing.clk_change) {
		case LCD_CLK_PLL_CHANGE:
			lcd_clk_generate_parameter(pdrv);
			break;
		default:
			break;
		}
	}

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s finished\n", pdrv->index, __func__);
	local_time[1] = sched_clock();
	pdrv->config.cus_ctrl.driver_change_time = local_time[1] - local_time[0];
	return 0;
}
