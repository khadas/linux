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
#ifdef CONFIG_AMLOGIC_VPU
#include <linux/amlogic/media/vpu/vpu.h>
#endif
#include <linux/amlogic/media/vout/vinfo.h>
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#include <linux/amlogic/media/vout/lcd/lcd_notify.h>
#include "lcd_tablet.h"
#include "mipi_dsi_util.h"
#include "../lcd_reg.h"
#include "../lcd_common.h"

static int lcd_type_supported(struct lcd_config_s *pconf)
{
	int lcd_type = pconf->basic.lcd_type;
	int ret = -1;

	switch (lcd_type) {
	case LCD_RGB:
	case LCD_BT656:
	case LCD_BT1120:
	case LCD_LVDS:
	case LCD_VBYONE:
	case LCD_MIPI:
	case LCD_EDP:
		ret = 0;
		break;
	default:
		LCDERR("invalid lcd type: %s(%d)\n", lcd_type_type_to_str(lcd_type), lcd_type);
		break;
	}
	return ret;
}

static void lcd_rgb_control_set(struct aml_lcd_drv_s *pdrv, unsigned char on_off)
{
	//todo
}

static void lcd_bt_control_set(struct aml_lcd_drv_s *pdrv, unsigned char on_off)
{
	struct lcd_config_s *pconf = &pdrv->config;
	unsigned int field_type, mode_422, yc_swap, cbcr_swap;

	field_type = pconf->control.bt_cfg.field_type;
	mode_422 = pconf->control.bt_cfg.mode_422;
	yc_swap = pconf->control.bt_cfg.yc_swap;
	cbcr_swap = pconf->control.bt_cfg.cbcr_swap;

	if (on_off) {
		lcd_vcbus_setb(VPU_VOUT_BT_CTRL, field_type, 12, 1);
		lcd_vcbus_setb(VPU_VOUT_BT_CTRL, yc_swap, 0, 1);
		//0:cb first   1:cr first
		lcd_vcbus_setb(VPU_VOUT_BT_CTRL, cbcr_swap, 1, 1);
		//0:left, 1:right, 2:average
		lcd_vcbus_setb(VPU_VOUT_BT_CTRL, mode_422, 2, 2);
	}
}

void lcd_tablet_config_post_update(struct aml_lcd_drv_s *pdrv)
{
	/* update interface timing */
	switch (pdrv->config.basic.lcd_type) {
	case LCD_MIPI:
		lcd_mipi_dsi_config_post(pdrv);
		break;
	case LCD_EDP:
		DPCD_capability_detect(pdrv);
		dptx_uboot_config_load(pdrv);
		dptx_load_uboot_edid_timing(pdrv);
		break;
	default:
		break;
	}
}

void lcd_tablet_driver_init_pre(struct aml_lcd_drv_s *pdrv)
{
	int ret;

	LCDPR("[%d]: tablet driver init(ver %s): %s\n",
	      pdrv->index, LCD_DRV_VERSION,
	      lcd_type_type_to_str(pdrv->config.basic.lcd_type));
	ret = lcd_type_supported(&pdrv->config);
	if (ret)
		return;

	lcd_frame_rate_change(pdrv);
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

void lcd_tablet_driver_disable_post(struct aml_lcd_drv_s *pdrv)
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

int lcd_tablet_driver_init(struct aml_lcd_drv_s *pdrv)
{
	int ret;

	ret = lcd_type_supported(&pdrv->config);
	if (ret)
		return -1;

	switch (pdrv->config.basic.lcd_type) {
	case LCD_RGB:
		lcd_rgb_control_set(pdrv, 1);
		lcd_rgb_pinmux_set(pdrv, 1);
		break;
	case LCD_BT656:
	case LCD_BT1120:
		lcd_bt_control_set(pdrv, 1);
		lcd_bt_pinmux_set(pdrv, 1);
		break;
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
	case LCD_MIPI:
		lcd_phy_set(pdrv, LCD_PHY_ON);
		lcd_mipi_dphy_set(pdrv, 1);
		mipi_dsi_tx_ctrl(pdrv, 1);
		break;
	case LCD_EDP:
		lcd_edp_pinmux_set(pdrv, 1);
		lcd_phy_set(pdrv, LCD_PHY_ON);
		lcd_edp_dphy_set(pdrv, 1);
		edp_tx_ctrl(pdrv, 1);
		break;
	default:
		break;
	}

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s finished\n", pdrv->index, __func__);
	return 0;
}

void lcd_tablet_driver_disable(struct aml_lcd_drv_s *pdrv)
{
	int ret;

	LCDPR("[%d]: disable driver\n", pdrv->index);
	ret = lcd_type_supported(&pdrv->config);
	if (ret)
		return;

	switch (pdrv->config.basic.lcd_type) {
	case LCD_RGB:
		lcd_rgb_pinmux_set(pdrv, 0);
		break;
	case LCD_BT656:
	case LCD_BT1120:
		lcd_bt_pinmux_set(pdrv, 0);
		break;
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
	case LCD_MIPI:
		mipi_dsi_link_off(pdrv);
		lcd_phy_set(pdrv, LCD_PHY_OFF);
		lcd_mipi_dphy_set(pdrv, 0);
		mipi_dsi_tx_ctrl(pdrv, 0);
		break;
	case LCD_EDP:
		edp_tx_ctrl(pdrv, 0);
		lcd_edp_dphy_set(pdrv, 0);
		lcd_phy_set(pdrv, LCD_PHY_OFF);
		lcd_edp_pinmux_set(pdrv, 0);
		break;
	default:
		break;
	}

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s finished\n", pdrv->index, __func__);
}
