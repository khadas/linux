
/*
 * drivers/amlogic/display/lcd/lcd_tablet/lcd_tablet.c
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
#include <linux/amlogic/vout/vinfo.h>
#include <linux/amlogic/vout/vout_notify.h>
#include <linux/amlogic/vout/lcd_vout.h>
#include <linux/amlogic/vout/lcd_unifykey.h>
#include <linux/amlogic/vout/lcd_notify.h>
#include "lcd_tablet.h"
#include "../lcd_reg.h"
#include "../lcd_common.h"

#define PANEL_NAME    "panel"

static DEFINE_MUTEX(lcd_vout_mutex);

/* ************************************************** *
   vout server api
 * ************************************************** */
static enum vmode_e lcd_validate_vmode(char *mode)
{
	if (mode == NULL)
		return VMODE_MAX;

	if ((strncmp(mode, PANEL_NAME, strlen(PANEL_NAME))) == 0)
		return VMODE_LCD;

	return VMODE_MAX;
}

static struct vinfo_s *lcd_get_current_info(void)
{
	struct aml_lcd_drv_s *lcd_drv = aml_lcd_get_driver();

	return lcd_drv->lcd_info;
}

static int lcd_set_current_vmode(enum vmode_e mode)
{
	int ret = 0;
	struct aml_lcd_drv_s *lcd_drv = aml_lcd_get_driver();

	mutex_lock(&lcd_vout_mutex);

	if (!(mode & VMODE_INIT_BIT_MASK)) {
		if (VMODE_LCD == (mode & VMODE_MODE_BIT_MASK)) {
			lcd_drv->driver_init_pre();
			ret = lcd_drv->driver_init();
		} else {
			ret = -EINVAL;
		}
	}

	lcd_vcbus_write(VPP_POSTBLEND_H_SIZE, lcd_drv->lcd_info->width);

	mutex_unlock(&lcd_vout_mutex);
	return ret;
}

static int lcd_vmode_is_supported(enum vmode_e mode)
{
	mode &= VMODE_MODE_BIT_MASK;
	LCDPR("%s vmode = %d\n", __func__, mode);

	if (mode == VMODE_LCD)
		return true;
	return false;
}

static int lcd_vout_disable(enum vmode_e cur_vmod)
{
	aml_lcd_notifier_call_chain(LCD_EVENT_POWER_OFF, NULL);
	LCDPR("%s finished\n", __func__);
	return 0;
}

#ifdef CONFIG_PM
static int lcd_suspend(void)
{
	aml_lcd_notifier_call_chain(LCD_EVENT_POWER_OFF, NULL);
	LCDPR("%s finished\n", __func__);
	return 0;
}
static int lcd_resume(void)
{
	aml_lcd_notifier_call_chain(LCD_EVENT_POWER_ON, NULL);
	LCDPR("%s finished\n", __func__);
	return 0;
}
#endif

static struct vout_server_s lcd_vout_server = {
	.name = "lcd_vout_server",
	.op = {
		.get_vinfo = lcd_get_current_info,
		.set_vmode = lcd_set_current_vmode,
		.validate_vmode = lcd_validate_vmode,
		.vmode_is_supported = lcd_vmode_is_supported,
		.disable = lcd_vout_disable,
#ifdef CONFIG_PM
		.vout_suspend = lcd_suspend,
		.vout_resume = lcd_resume,
#endif
	},
};

static void lcd_tablet_vinfo_update(void)
{
	struct aml_lcd_drv_s *lcd_drv = aml_lcd_get_driver();
	struct vinfo_s *vinfo;
	struct lcd_config_s *pconf;

	if (lcd_drv->lcd_info == NULL) {
		LCDERR("no lcd_info exist\n");
		return;
	}
	vinfo = lcd_drv->lcd_info;
	pconf = lcd_drv->lcd_config;
	if (vinfo) {
		vinfo->name = PANEL_NAME;
		vinfo->mode = VMODE_LCD;
		vinfo->width = pconf->lcd_basic.h_active;
		vinfo->height = pconf->lcd_basic.v_active;
		vinfo->field_height = pconf->lcd_basic.v_active;
		vinfo->aspect_ratio_num = pconf->lcd_basic.screen_width;
		vinfo->aspect_ratio_den = pconf->lcd_basic.screen_height;
		vinfo->screen_real_width = pconf->lcd_basic.screen_width;
		vinfo->screen_real_height = pconf->lcd_basic.screen_height;
		vinfo->sync_duration_num = pconf->lcd_timing.sync_duration_num;
		vinfo->sync_duration_den = pconf->lcd_timing.sync_duration_den;
		vinfo->video_clk = pconf->lcd_timing.lcd_clk;
		vinfo->viu_color_fmt = TVIN_RGB444;
	}
}

static void lcd_tablet_vinfo_update_default(void)
{
	struct aml_lcd_drv_s *lcd_drv = aml_lcd_get_driver();
	struct vinfo_s *vinfo;
	unsigned int h_active, v_active;

	if (lcd_drv->lcd_info == NULL) {
		LCDERR("no lcd_info exist\n");
		return;
	}

	h_active = lcd_vcbus_read(ENCL_VIDEO_HAVON_END)
			- lcd_vcbus_read(ENCL_VIDEO_HAVON_BEGIN) + 1;
	v_active = lcd_vcbus_read(ENCL_VIDEO_VAVON_ELINE)
			- lcd_vcbus_read(ENCL_VIDEO_VAVON_BLINE) + 1;

	vinfo = lcd_drv->lcd_info;
	if (vinfo) {
		vinfo->name = PANEL_NAME;
		vinfo->mode = VMODE_LCD;
		vinfo->width = h_active;
		vinfo->height = v_active;
		vinfo->field_height = v_active;
		vinfo->aspect_ratio_num = h_active;
		vinfo->aspect_ratio_den = v_active;
		vinfo->screen_real_width = h_active;
		vinfo->screen_real_height = v_active;
		vinfo->sync_duration_num = 60;
		vinfo->sync_duration_den = 1;
		vinfo->video_clk = 0;
		vinfo->viu_color_fmt = TVIN_RGB444;
	}
}

void lcd_tablet_vout_server_init(void)
{
	struct aml_lcd_drv_s *lcd_drv = aml_lcd_get_driver();

	lcd_drv->lcd_info = kmalloc(sizeof(struct vinfo_s), GFP_KERNEL);
	if (!lcd_drv->lcd_info) {
		LCDERR("tablet_probe: Not enough memory\n");
		return;
	}

	lcd_tablet_vinfo_update_default();

	vout_register_server(&lcd_vout_server);
}

/* ************************************************** *
   lcd tablet config
 * ************************************************** */
static void lcd_config_print(struct lcd_config_s *pconf)
{
	LCDPR("%s, %s, %dbit, %dx%d\n",
		pconf->lcd_basic.model_name,
		lcd_type_type_to_str(pconf->lcd_basic.lcd_type),
		pconf->lcd_basic.lcd_bits,
		pconf->lcd_basic.h_active, pconf->lcd_basic.v_active);

	if (lcd_debug_print_flag == 0)
		return;

	LCDPR("h_period = %d\n", pconf->lcd_basic.h_period);
	LCDPR("v_period = %d\n", pconf->lcd_basic.v_period);
	LCDPR("screen_width = %d\n", pconf->lcd_basic.screen_width);
	LCDPR("screen_height = %d\n", pconf->lcd_basic.screen_height);

	LCDPR("hsync_width = %d\n", pconf->lcd_timing.hsync_width);
	LCDPR("hsync_bp = %d\n", pconf->lcd_timing.hsync_bp);
	LCDPR("hsync_pol = %d\n", pconf->lcd_timing.hsync_pol);
	LCDPR("vsync_width = %d\n", pconf->lcd_timing.vsync_width);
	LCDPR("vsync_bp = %d\n", pconf->lcd_timing.vsync_bp);
	LCDPR("vsync_pol = %d\n", pconf->lcd_timing.vsync_pol);

	LCDPR("fr_adjust_type = %d\n", pconf->lcd_timing.fr_adjust_type);
	LCDPR("ss_level = %d\n", pconf->lcd_timing.ss_level);
	LCDPR("clk_auto = %d\n", pconf->lcd_timing.clk_auto);

	if (pconf->lcd_basic.lcd_type == LCD_TTL) {
		LCDPR("clk_pol = %d\n",
			pconf->lcd_control.ttl_config->clk_pol);
		LCDPR("sync_valid = %d\n",
			pconf->lcd_control.ttl_config->sync_valid);
		LCDPR("swap_ctrl = %d\n",
			pconf->lcd_control.ttl_config->swap_ctrl);
	} else if (pconf->lcd_basic.lcd_type == LCD_LVDS) {
		LCDPR("lvds_repack = %d\n",
			pconf->lcd_control.lvds_config->lvds_repack);
		LCDPR("pn_swap = %d\n",
			pconf->lcd_control.lvds_config->pn_swap);
		LCDPR("dual_port = %d\n",
			pconf->lcd_control.lvds_config->dual_port);
		LCDPR("port_swap = %d\n",
			pconf->lcd_control.lvds_config->port_swap);
	}
}

static int lcd_init_load_from_dts(struct lcd_config_s *pconf,
		struct device *dev)
{
	int ret = 0;
	struct aml_lcd_drv_s *lcd_drv = aml_lcd_get_driver();

	switch (pconf->lcd_basic.lcd_type) {
	case LCD_TTL:
		if (lcd_drv->lcd_status) /* lock pinmux if lcd in on */
			lcd_ttl_pinmux_set(1);
		else
			lcd_ttl_pinmux_set(0);
		break;
	default:
		break;
	}

	pconf->rstc.encl = devm_reset_control_get(dev, "encl");
	if (IS_ERR(pconf->rstc.encl))
		LCDERR("get reset control encl error\n");
	pconf->rstc.vencl = devm_reset_control_get(dev, "vencl");
	if (IS_ERR(pconf->rstc.vencl))
		LCDERR("get reset control vencl error\n");

	return ret;
}

static int lcd_config_load_from_dts(struct lcd_config_s *pconf,
		struct device *dev)
{
	int ret = 0;
	const char *str;
	unsigned int para[10];
	struct device_node *child;

	child = of_get_child_by_name(dev->of_node, pconf->lcd_propname);
	if (child == NULL) {
		LCDERR("failed to get %s\n", pconf->lcd_propname);
		return -1;
	}

	ret = of_property_read_string(child, "model_name", &str);
	if (ret) {
		LCDERR("failed to get model_name\n");
		strcpy(pconf->lcd_basic.model_name, pconf->lcd_propname);
	} else {
		strcpy(pconf->lcd_basic.model_name, str);
	}

	ret = of_property_read_string(child, "interface", &str);
	if (ret) {
		LCDERR("failed to get interface\n");
		str = "invalid";
	}
	pconf->lcd_basic.lcd_type = lcd_type_str_to_type(str);

	ret = of_property_read_u32_array(child, "basic_setting", &para[0], 7);
	if (ret) {
		LCDERR("failed to get basic_setting\n");
	} else {
		pconf->lcd_basic.h_active = para[0];
		pconf->lcd_basic.v_active = para[1];
		pconf->lcd_basic.h_period = para[2];
		pconf->lcd_basic.v_period = para[3];
		pconf->lcd_basic.lcd_bits = para[4];
		pconf->lcd_basic.screen_width = para[5];
		pconf->lcd_basic.screen_height = para[6];
	}

	ret = of_property_read_u32_array(child, "lcd_timing", &para[0], 6);
	if (ret) {
		LCDERR("failed to get lcd_timing\n");
	} else {
		pconf->lcd_timing.hsync_width = (unsigned short)(para[0]);
		pconf->lcd_timing.hsync_bp = (unsigned short)(para[1]);
		pconf->lcd_timing.hsync_pol = (unsigned short)(para[2]);
		pconf->lcd_timing.vsync_width = (unsigned short)(para[3]);
		pconf->lcd_timing.vsync_bp = (unsigned short)(para[4]);
		pconf->lcd_timing.vsync_pol = (unsigned short)(para[5]);
	}

	ret = of_property_read_u32_array(child, "clk_attr", &para[0], 4);
	if (ret) {
		LCDERR("failed to get clk_attr\n");
		pconf->lcd_timing.fr_adjust_type = 0;
		pconf->lcd_timing.ss_level = 0;
		pconf->lcd_timing.clk_auto = 1;
		pconf->lcd_timing.lcd_clk = 60;
	} else {
		pconf->lcd_timing.fr_adjust_type = (unsigned char)(para[0]);
		pconf->lcd_timing.ss_level = (unsigned char)(para[1]);
		pconf->lcd_timing.clk_auto = (unsigned char)(para[2]);
		if (para[3] > 0) {
			pconf->lcd_timing.lcd_clk = para[3];
		} else { /* avoid 0 mistake */
			pconf->lcd_timing.lcd_clk = 60;
			LCDERR("lcd_clk is 0, default to 60Hz\n");
		}
	}
	if (pconf->lcd_timing.clk_auto == 0) {
		ret = of_property_read_u32_array(child, "clk_para",
			&para[0], 3);
		if (ret) {
			LCDERR("failed to get clk_para\n");
		} else {
			pconf->lcd_timing.pll_ctrl = para[0];
			pconf->lcd_timing.div_ctrl = para[1];
			pconf->lcd_timing.clk_ctrl = para[2];
		}
	}

	switch (pconf->lcd_basic.lcd_type) {
	case LCD_TTL:
		ret = of_property_read_u32_array(child, "ttl_attr",
			&para[0], 5);
		if (ret) {
			LCDERR("failed to get ttl_attr\n");
		} else {
			pconf->lcd_control.ttl_config->clk_pol = para[0];
			pconf->lcd_control.ttl_config->sync_valid =
				((para[1] << 1) | (para[2] << 0));
			pconf->lcd_control.ttl_config->swap_ctrl =
				((para[3] << 1) | (para[4] << 0));
		}
	case LCD_LVDS:
		ret = of_property_read_u32_array(child, "lvds_attr",
			&para[0], 4);
		if (ret) {
			LCDERR("failed to get lvds_attr\n");
		} else {
			pconf->lcd_control.lvds_config->lvds_repack = para[0];
			pconf->lcd_control.lvds_config->dual_port = para[1];
			pconf->lcd_control.lvds_config->pn_swap = para[2];
			pconf->lcd_control.lvds_config->port_swap = para[3];
		}
		ret = of_property_read_u32_array(child, "phy_attr",
			&para[0], 2);
		if (ret) {
			if (lcd_debug_print_flag)
				LCDPR("failed to get phy_attr\n");
		} else {
			pconf->lcd_control.lvds_config->phy_vswing = para[0];
			pconf->lcd_control.lvds_config->phy_preem = para[1];
			LCDPR("set phy vswing=%d, preemphasis=%d\n",
				pconf->lcd_control.lvds_config->phy_vswing,
				pconf->lcd_control.lvds_config->phy_preem);
		}
		break;
	default:
		LCDERR("invalid lcd type\n");
		break;
	}

	ret = lcd_power_load_from_dts(pconf, child);

	return ret;
}

static int lcd_config_load_from_unifykey(struct lcd_config_s *pconf)
{
	unsigned char *para;
	int key_len, len;
	unsigned char *p;
	const char *str;
	struct aml_lcd_unifykey_header_s lcd_header;
	unsigned int temp;
	int ret;

	para = kmalloc((sizeof(unsigned char) * LCD_UKEY_LCD_SIZE), GFP_KERNEL);
	if (!para) {
		LCDERR("%s: Not enough memory\n", __func__);
		return -1;
	}
	key_len = LCD_UKEY_LCD_SIZE;
	memset(para, 0, (sizeof(unsigned char) * key_len));
	ret = lcd_unifykey_get("lcd", para, &key_len);
	if (ret < 0) {
		kfree(para);
		return -1;
	}

	/* check lcd unifykey length */
	len = 10 + 36 + 18 + 31 + 20;
	ret = lcd_unifykey_len_check(key_len, len);
	if (ret < 0) {
		kfree(para);
		return -1;
	}

	/* header: 10byte */
	lcd_unifykey_header_check(para, &lcd_header);
	if (lcd_debug_print_flag) {
		LCDPR("unifykey header:\n");
		LCDPR("crc32             = 0x%08x\n", lcd_header.crc32);
		LCDPR("data_len          = %d\n", lcd_header.data_len);
		LCDPR("version           = 0x%04x\n", lcd_header.version);
		LCDPR("reserved          = 0x%04x\n", lcd_header.reserved);
	}

	/* basic: 36byte */
	p = para + LCD_UKEY_HEAD_SIZE;
	*(p + LCD_UKEY_MODEL_NAME - 1) = '\0'; /* ensure string ending */
	str = (const char *)p;
	strcpy(pconf->lcd_basic.model_name, str);
	p += LCD_UKEY_MODEL_NAME;
	pconf->lcd_basic.lcd_type = *p;
	p += LCD_UKEY_INTERFACE;
	pconf->lcd_basic.lcd_bits = *p;
	p += LCD_UKEY_LCD_BITS;
	pconf->lcd_basic.screen_width = (*p | ((*(p + 1)) << 8));
	p += LCD_UKEY_SCREEN_WIDTH;
	pconf->lcd_basic.screen_height = (*p | ((*(p + 1)) << 8));
	p += LCD_UKEY_SCREEN_HEIGHT;

	/* timing: 18byte */
	pconf->lcd_basic.h_active = (*p | ((*(p + 1)) << 8));
	p += LCD_UKEY_H_ACTIVE;
	pconf->lcd_basic.v_active = (*p | ((*(p + 1)) << 8));
	p += LCD_UKEY_V_ACTIVE;
	pconf->lcd_basic.h_period = (*p | ((*(p + 1)) << 8));
	p += LCD_UKEY_H_PERIOD;
	pconf->lcd_basic.v_period = (*p | ((*(p + 1)) << 8));
	p += LCD_UKEY_V_PERIOD;
	pconf->lcd_timing.hsync_width = (*p | ((*(p + 1)) << 8));
	p += LCD_UKEY_HS_WIDTH;
	pconf->lcd_timing.hsync_bp = (*p | ((*(p + 1)) << 8));
	p += LCD_UKEY_HS_BP;
	pconf->lcd_timing.hsync_pol = *p;
	p += LCD_UKEY_HS_POL;
	pconf->lcd_timing.vsync_width = (*p | ((*(p + 1)) << 8));
	p += LCD_UKEY_VS_WIDTH;
	pconf->lcd_timing.vsync_bp = (*p | ((*(p + 1)) << 8));
	p += LCD_UKEY_VS_BP;
	pconf->lcd_timing.vsync_pol = *p;
	p += LCD_UKEY_VS_POL;

	/* customer: 31byte */
	pconf->lcd_timing.fr_adjust_type = *p;
	p += LCD_UKEY_FR_ADJ_TYPE;
	pconf->lcd_timing.ss_level = *p;
	p += LCD_UKEY_SS_LEVEL;
	pconf->lcd_timing.clk_auto = *p;
	p += LCD_UKEY_CLK_AUTO_GEN;
	pconf->lcd_timing.lcd_clk = (*p | ((*(p + 1)) << 8) |
		((*(p + 2)) << 16) | ((*(p + 3)) << 24));
	p += LCD_UKEY_PCLK;
	/* dummy pointer */
	p += LCD_UKEY_CUST_VAL_4;
	p += LCD_UKEY_CUST_VAL_5;
	p += LCD_UKEY_CUST_VAL_6;
	p += LCD_UKEY_CUST_VAL_7;
	p += LCD_UKEY_CUST_VAL_8;
	p += LCD_UKEY_CUST_VAL_9;

	/* interface: 20byte */
	if (pconf->lcd_basic.lcd_type == LCD_LVDS) {
		pconf->lcd_control.lvds_config->lvds_repack =
				(*p | ((*(p + 1)) << 8)) & 0xff;
		p += LCD_UKEY_IF_ATTR_0;
		pconf->lcd_control.lvds_config->dual_port =
				(*p | ((*(p + 1)) << 8)) & 0xff;
		p += LCD_UKEY_IF_ATTR_1;
		pconf->lcd_control.lvds_config->pn_swap  =
				(*p | ((*(p + 1)) << 8)) & 0xff;
		p += LCD_UKEY_IF_ATTR_2;
		pconf->lcd_control.lvds_config->port_swap  =
				(*p | ((*(p + 1)) << 8)) & 0xff;
		p += LCD_UKEY_IF_ATTR_3;
		pconf->lcd_control.lvds_config->phy_vswing =
				(*p | ((*(p + 1)) << 8)) & 0xff;
		p += LCD_UKEY_IF_ATTR_4;
		pconf->lcd_control.lvds_config->phy_vswing =
				(*p | ((*(p + 1)) << 8)) & 0xff;
		p += LCD_UKEY_IF_ATTR_5;
		/* dummy pointer */
		p += LCD_UKEY_IF_ATTR_6;
		p += LCD_UKEY_IF_ATTR_7;
		p += LCD_UKEY_IF_ATTR_8;
		p += LCD_UKEY_IF_ATTR_9;
	} else if (pconf->lcd_basic.lcd_type == LCD_TTL) {
		pconf->lcd_control.ttl_config->clk_pol =
			(*p | ((*(p + 1)) << 8)) & 0x1;
		p += LCD_UKEY_IF_ATTR_0;
		temp = (*p | ((*(p + 1)) << 8)) & 0x1; /* de_valid */
		pconf->lcd_control.ttl_config->sync_valid = (temp << 1);
		p += LCD_UKEY_IF_ATTR_1;
		temp = (*p | ((*(p + 1)) << 8)) & 0x1; /* hvsync_valid */
		pconf->lcd_control.ttl_config->sync_valid |= (temp << 0);
		p += LCD_UKEY_IF_ATTR_2;
		temp = (*p | ((*(p + 1)) << 8)) & 0x1; /* rb_swap */
		pconf->lcd_control.ttl_config->swap_ctrl = (temp << 1);
		p += LCD_UKEY_IF_ATTR_3;
		temp = (*p | ((*(p + 1)) << 8)) & 0x1; /* bit_swap */
		pconf->lcd_control.ttl_config->swap_ctrl |= (temp << 0);
		p += LCD_UKEY_IF_ATTR_4;
		/* dummy pointer */
		p += LCD_UKEY_IF_ATTR_5;
		p += LCD_UKEY_IF_ATTR_6;
		p += LCD_UKEY_IF_ATTR_7;
		p += LCD_UKEY_IF_ATTR_8;
		p += LCD_UKEY_IF_ATTR_9;
	} else {
		LCDERR("unsupport lcd_type: %d\n", pconf->lcd_basic.lcd_type);
		p += LCD_UKEY_IF_ATTR_0;
		p += LCD_UKEY_IF_ATTR_1;
		p += LCD_UKEY_IF_ATTR_2;
		p += LCD_UKEY_IF_ATTR_3;
		p += LCD_UKEY_IF_ATTR_4;
		p += LCD_UKEY_IF_ATTR_5;
		p += LCD_UKEY_IF_ATTR_6;
		p += LCD_UKEY_IF_ATTR_7;
		p += LCD_UKEY_IF_ATTR_8;
		p += LCD_UKEY_IF_ATTR_9;
	}

	ret = lcd_power_load_from_unifykey(pconf, para, key_len);
	if (ret < 0) {
		kfree(para);
		return -1;
	}

	kfree(para);
	return 0;
}

static void lcd_config_init(struct lcd_config_s *pconf)
{
	struct lcd_clk_config_s *cconf = get_lcd_clk_config();
	unsigned int ss_level;
	unsigned int clk;
	unsigned int sync_duration, h_period, v_period;

	clk = pconf->lcd_timing.lcd_clk;
	h_period = pconf->lcd_basic.h_period;
	v_period = pconf->lcd_basic.v_period;
	if (clk < 200) { /* regard as frame_rate */
		sync_duration = clk * 100;
		pconf->lcd_timing.lcd_clk = clk * h_period * v_period;
	} else { /* regard as pixel clock */
		sync_duration = ((clk / h_period) * 100) / v_period;
	}
	pconf->lcd_timing.sync_duration_num = sync_duration;
	pconf->lcd_timing.sync_duration_den = 100;

	pconf->lcd_timing.lcd_clk_dft = pconf->lcd_timing.lcd_clk;
	lcd_tcon_config(pconf);
	lcd_clk_generate_parameter(pconf);
	ss_level = pconf->lcd_timing.ss_level;
	cconf->ss_level = (ss_level >= cconf->ss_level_max) ? 0 : ss_level;
}

static int lcd_get_config(struct lcd_config_s *pconf, struct device *dev)
{
	struct aml_lcd_drv_s *lcd_drv = aml_lcd_get_driver();
	int load_id = 0;
	int ret;

	if (dev->of_node == NULL) {
		LCDERR("dev of_node is null\n");
		return -1;
	}
	if (lcd_drv->lcd_key_valid) {
		ret = lcd_unifykey_check("lcd");
		if (ret < 0)
			load_id = 0;
		else
			load_id = 1;
	}
	if (load_id) {
		LCDPR("%s from unifykey\n", __func__);
		lcd_drv->lcd_config_load = 1;
		lcd_config_load_from_unifykey(pconf);
	} else {
		LCDPR("%s from dts\n", __func__);
		lcd_drv->lcd_config_load = 0;
		lcd_config_load_from_dts(pconf, dev);
	}
	lcd_init_load_from_dts(pconf, dev);
	lcd_config_print(pconf);
	lcd_tablet_vinfo_update();

	lcd_config_init(pconf);

	return 0;
}

/* ************************************************** *
   lcd notify
 * ************************************************** */
/* sync_duration is real_value * 100 */
static void lcd_set_vinfo(unsigned int sync_duration)
{
	struct aml_lcd_drv_s *lcd_drv = aml_lcd_get_driver();

	LCDPR("%s: sync_duration=%d\n", __func__, sync_duration);

	/* update vinfo */
	if (lcd_drv->lcd_info) {
		lcd_drv->lcd_info->sync_duration_num = sync_duration;
		lcd_drv->lcd_info->sync_duration_den = 100;
	} else {
		LCDERR("no lcd_info exist\n");
	}
	/* update lcd config sync_duration */
	lcd_drv->lcd_config->lcd_timing.sync_duration_num = sync_duration;
	lcd_drv->lcd_config->lcd_timing.sync_duration_den = 100;

	/* update clk & timing config */
	lcd_vmode_change(lcd_drv->lcd_config);
	/* update interface timing if needed, current no need */
	/* update clk parameter */
	lcd_clk_generate_parameter(lcd_drv->lcd_config);
}

static int lcd_frame_rate_adjust_notifier(struct notifier_block *nb,
		unsigned long event, void *data)
{
	unsigned int *sync_duration;

	/* LCDPR("%s: 0x%lx\n", __func__, event); */
	if ((event & LCD_EVENT_FRAME_RATE_ADJUST) == 0)
		return NOTIFY_DONE;

	sync_duration = (unsigned int *)data;
	lcd_set_vinfo(*sync_duration);

	return NOTIFY_OK;
}

static struct notifier_block lcd_frame_rate_adjust_nb = {
	.notifier_call = lcd_frame_rate_adjust_notifier,
};

/* ************************************************** *
   lcd tablet
 * ************************************************** */
int lcd_tablet_probe(struct device *dev)
{
	struct aml_lcd_drv_s *lcd_drv = aml_lcd_get_driver();
	int ret;

	lcd_drv->version = LCD_DRV_VERSION;
	lcd_drv->vout_server_init = lcd_tablet_vout_server_init;
	lcd_drv->driver_init_pre = lcd_tablet_driver_init_pre;
	lcd_drv->driver_init = lcd_tablet_driver_init;
	lcd_drv->driver_disable = lcd_tablet_driver_disable;

	lcd_get_config(lcd_drv->lcd_config, dev);

	ret = aml_lcd_notifier_register(&lcd_frame_rate_adjust_nb);
	if (ret)
		LCDERR("register lcd_frame_rate_adjust_nb failed\n");

	return 0;
}

int lcd_tablet_remove(struct device *dev)
{
	struct aml_lcd_drv_s *lcd_drv = aml_lcd_get_driver();

	aml_lcd_notifier_unregister(&lcd_frame_rate_adjust_nb);
	kfree(lcd_drv->lcd_info);
	lcd_drv->lcd_info = NULL;
	return 0;
}

