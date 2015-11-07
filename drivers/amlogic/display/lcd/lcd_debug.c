/*
 * drivers/amlogic/display/vout/lcd/lcd_debug.c
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
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/amlogic/vout/lcd_vout.h>
#include "lcd_reg.h"
#include "lcd_common.h"

static const char *lcd_common_usage_str = {
"Usage:\n"
"    echo <0|1> > enable ; 0=disable lcd; 1=enable lcd\n"
"    cat enable ; read current lcd enable status\n"
"\n"
"    echo type <adj_type> > frame_rate ; set lcd frame rate(unit in Hz)\n"
"    echo set <frame_rate> > frame_rate ; set lcd frame rate(unit in Hz)\n"
"    cat frame_rate ; read current lcd frame rate\n"
"\n"
"    echo <num> > test ; show lcd bist pattern(1~7), 0=disable bist\n"
"    cat test ; read current lcd bist pattern\n"
"\n"
"    echo w<v|h|c|p> <reg> <data> > reg ; write data to vcbus|hiu|cbus|periphs reg\n"
"    echo r<v|h|c|p> <reg> > reg ; read vcbus|hiu|cbus|periphs reg\n"
"    echo d<v|h|c|p> <reg> <num> > reg ; dump vcbus|hiu|cbus|periphs regs\n"
"\n"
"    echo <0|1> > print ; 0=disable debug print; 1=enable debug print\n"
"    cat print ; read current debug print flag\n"
"\n"
"    echo <cmd> ... > debug ; lcd common debug, use 'cat debug' for help\n"
"    cat debug ; print help information for debug command\n"
};

static const char *lcd_debug_usage_str = {
"Usage:\n"
"    echo clk <freq> > debug ; set lcd pixel clock, unit in Hz\n"
"    echo basic <h_active> <v_active> <h_period> <v_period> > debug ; set lcd basic config\n"
"    echo sync <hs_width> <hs_backporch> <vs_width> <vs_backporch> > debug ; set lcd sync timing\n"
"    echo ttl <clk_pol> <sync_valid> <swap_ctrl> > debug ; set ttl control config\n"
"    echo lvds <repack> <dual_port> <pn_swap> <port_swap> > debug ; set lvds control config\n"
"    echo vbyone <lane_count> <region_num> <byte_mode> > debug ; set vbyone control config\n"
"    echo mipi <lane_num> <bit_rate_max> <init_disp_mode> <lp_clk_continuous> <factor> > debug ; set mpi control config\n"
"    echo edp <link_rate> <lane_count> <edid_timing_used> <sync_clock_mode> > debug ; set edp control config\n"
"data format:\n"
"    <xx_swap>      : 0=normal, 1=swap\n"
"    <vswing_level> : lvds support level 0~4 (Default=1), edp support level 0~3 (default=0)\n"
"    <lvds_repack>  : 0=JEIDA mode, 1=VESA mode\n"
"    <pn_swap>      : 0=normal, 1=swap lvds p/n channels\n"
"    <bit_rate_max> : unit in MHz\n"
"    <factor>:      : special adjust, 0 for default\n"
"    <xxxx_mode>    : 0=video mode, 1=command mode\n"
"    <lp_clk_auto_stop> : 0=disable, 1=enable\n"
"    <transfer_switch>  : 0=auto, 1=standard, 2=slow\n"
"    <link_rate>        : 0=1.62G, 1=2.7G\n"
"    <edid_timing_used> : 0=no use, 1=use, default=0\n"
"    <sync_clock_mode>  : 0=asyncronous, 1=synchronous, default=1\n"
"\n"
"    echo info > debug ; show lcd infomation\n"
"    echo reg > debug ; show lcd registers\n"
"    echo dump > debug ; show lcd infomation & registers\n"
"    echo reset > debug; reset lcd driver\n"
"    echo power <0|1> > debug ; lcd power control: 0=power off, 1=power on\n"
};

static ssize_t lcd_debug_common_help(struct class *class,
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", lcd_common_usage_str);
}

static ssize_t lcd_debug_help(struct class *class,
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", lcd_debug_usage_str);
}

static void lcd_cpu_gpio_register_print(struct lcd_config_s *pconf)
{
	int i;
	struct lcd_cpu_gpio_s *cpu_gpio;

	pr_info("cpu_gpio register:\n");

	i = 0;
	while (i < LCD_CPU_GPIO_NUM_MAX) {
		cpu_gpio = &pconf->lcd_power->cpu_gpio[i];
		if (cpu_gpio->flag) {
			pr_info("%d: name=%s, gpio=%p\n",
				i, cpu_gpio->name, cpu_gpio->gpio);
		}
		i++;
	}
}

static void lcd_power_info_print(struct lcd_config_s *pconf, int status)
{
	int i;
	struct lcd_power_step_s *power_step;

	if (status)
		pr_info("power on step:\n");
	else
		pr_info("power off step:\n");

	i = 0;
	while (i < LCD_PWR_STEP_MAX) {
		if (status)
			power_step = &pconf->lcd_power->power_on_step[i];
		else
			power_step = &pconf->lcd_power->power_off_step[i];

		if (power_step->type >= LCD_POWER_TYPE_MAX)
			break;
		switch (power_step->type) {
		case LCD_POWER_TYPE_CPU:
		case LCD_POWER_TYPE_PMU:
			pr_info("%d: type=%d, index=%d, value=%d, delay=%d\n",
				i, power_step->type, power_step->index,
				power_step->value, power_step->delay);
			break;
		case LCD_POWER_TYPE_EXTERN:
			pr_info("%d: type=%d, index=%d, delay=%d\n",
				i, power_step->type, power_step->index,
				power_step->delay);
			break;
		case LCD_POWER_TYPE_SIGNAL:
			pr_info("%d: type=%d, delay=%d\n",
				i, power_step->type, power_step->delay);
			break;
		default:
			break;
		}
		i++;
	}
}

static void lcd_info_print(void)
{
	unsigned int lcd_clk, sync_duration;
	struct aml_lcd_drv_s *lcd_drv = aml_lcd_get_driver();
	struct lcd_config_s *pconf;
	struct dsi_config_s *dconf;

	pconf = lcd_drv->lcd_config;
	lcd_clk = (pconf->lcd_timing.lcd_clk / 1000);
	sync_duration = pconf->lcd_timing.sync_duration_num * 10;
	sync_duration = sync_duration / pconf->lcd_timing.sync_duration_den;

	LCDPR("status: %d\n", lcd_drv->lcd_status);
	LCDPR("%s, %s %ubit, %ux%u@%u.%uHz\n",
		pconf->lcd_basic.model_name,
		lcd_type_type_to_str(pconf->lcd_basic.lcd_type),
		pconf->lcd_basic.lcd_bits,
		pconf->lcd_basic.h_active, pconf->lcd_basic.v_active,
		(sync_duration / 10), (sync_duration % 10));

	pr_info("lcd_clk           %u.%03uMHz\n"
		"ss_level          %d\n"
		"clk_auto          %d\n"
		"fr_adj_type       %d\n\n",
		(lcd_clk / 1000), (lcd_clk % 1000),
		pconf->lcd_timing.ss_level, pconf->lcd_timing.clk_auto,
		pconf->lcd_timing.fr_adjust_type);

	pr_info("h_period          %d\n"
		"v_period          %d\n"
		"hs_width          %d\n"
		"hs_backporch      %d\n"
		"hs_pol            %d\n"
		"vs_width          %d\n"
		"vs_backporch      %d\n"
		"vs_pol            %d\n\n",
		pconf->lcd_basic.h_period, pconf->lcd_basic.v_period,
		pconf->lcd_timing.hsync_width, pconf->lcd_timing.hsync_bp,
		pconf->lcd_timing.hsync_pol,
		pconf->lcd_timing.vsync_width, pconf->lcd_timing.vsync_bp,
		pconf->lcd_timing.vsync_pol);

	switch (pconf->lcd_basic.lcd_type) {
	case LCD_TTL:
		pr_info("pol_ctrl          %u\n"
			"hvsync_valid      %u\n"
			"de_valid          %u\n"
			"rb_swap           %u\n"
			"bit_swap          %u\n\n",
			pconf->lcd_control.ttl_config->pol_ctrl,
			((pconf->lcd_control.ttl_config->sync_valid >> 0) & 1),
			((pconf->lcd_control.ttl_config->sync_valid >> 1) & 1),
			((pconf->lcd_control.ttl_config->swap_ctrl >> 1) & 1),
			((pconf->lcd_control.ttl_config->swap_ctrl >> 0) & 1));
		break;
	case LCD_LVDS:
		pr_info("lvds_repack       %u\n"
			"dual_port         %u\n"
			"pn_swap           %u\n"
			"port_swap         %u\n\n",
			pconf->lcd_control.lvds_config->lvds_repack,
			pconf->lcd_control.lvds_config->dual_port,
			pconf->lcd_control.lvds_config->pn_swap,
			pconf->lcd_control.lvds_config->port_swap);
		break;
	case LCD_VBYONE:
		pr_info("lane_count        %u\n"
			"region_num        %u\n"
			"byte_mode         %u\n"
			"color_fmt         %u\n"
			"bit_rate          %u\n\n",
			pconf->lcd_control.vbyone_config->lane_count,
			pconf->lcd_control.vbyone_config->region_num,
			pconf->lcd_control.vbyone_config->byte_mode,
			pconf->lcd_control.vbyone_config->color_fmt,
			pconf->lcd_control.vbyone_config->bit_rate);
		break;
	case LCD_MIPI:
		dconf = pconf->lcd_control.mipi_config;
		pr_info("dsi_lane_num      %u\n"
			"dsi_bit_rate      %u.%03uMHz\n"
			"operation_mode    %u(%s), %u(%s)\n"
			"transfer_ctrl     %u, %u\n\n",
			dconf->lane_num,
			(dconf->bit_rate / 1000000),
			((dconf->bit_rate % 1000000) / 1000),
			((dconf->operation_mode>>BIT_OP_MODE_INIT) & 1),
			(((dconf->operation_mode>>BIT_OP_MODE_INIT) & 1) ?
				"COMMAND" : "VIDEO"),
			((dconf->operation_mode>>BIT_OP_MODE_DISP) & 1),
			(((dconf->operation_mode>>BIT_OP_MODE_DISP) & 1) ?
				"COMMAND" : "VIDEO"),
			((dconf->transfer_ctrl>>BIT_TRANS_CTRL_CLK) & 1),
			((dconf->transfer_ctrl>>BIT_TRANS_CTRL_SWITCH) & 3));
		break;
	case LCD_EDP:
		pr_info("link_rate         %s\n"
			"lane_count        %u\n"
			"link_adaptive     %u\n"
			"vswing            %u\n"
			"max_lane_count    %u\n"
			"sync_clock_mode   %u\n"
			"EDID timing used  %u\n\n",
			((pconf->lcd_control.edp_config->link_rate == 0) ?
				"1.62G":"2.7G"),
			pconf->lcd_control.edp_config->lane_count,
			pconf->lcd_control.edp_config->link_adaptive,
			pconf->lcd_control.edp_config->vswing,
			pconf->lcd_control.edp_config->max_lane_count,
			pconf->lcd_control.edp_config->sync_clock_mode,
			pconf->lcd_control.edp_config->edid_timing_used);
		break;
	default:
		break;
	}

	pr_info("pll_ctrl          0x%08x\n"
		"div_ctrl          0x%08x\n"
		"clk_ctrl          0x%08x\n"
		"video_on_pixel    %d\n"
		"video_on_line     %d\n\n",
		pconf->lcd_timing.pll_ctrl, pconf->lcd_timing.div_ctrl,
		pconf->lcd_timing.clk_ctrl,
		pconf->lcd_timing.video_on_pixel,
		pconf->lcd_timing.video_on_line);

	lcd_power_info_print(pconf, 1);
	lcd_power_info_print(pconf, 0);
	lcd_cpu_gpio_register_print(pconf);
}

static unsigned int lcd_reg_dump_clk[] = {
	HHI_HDMI_PLL_CNTL,
	HHI_HDMI_PLL_CNTL2,
	HHI_HDMI_PLL_CNTL3,
	HHI_HDMI_PLL_CNTL4,
	HHI_HDMI_PLL_CNTL5,
	HHI_HDMI_PLL_CNTL6,
};

static unsigned int lcd_reg_dump_encl[] = {
	VPU_VIU_VENC_MUX_CTRL,
	ENCL_VIDEO_EN,
	ENCL_VIDEO_MODE,
	ENCL_VIDEO_MODE_ADV,
	ENCL_VIDEO_MAX_PXCNT,
	ENCL_VIDEO_MAX_LNCNT,
	ENCL_VIDEO_HAVON_BEGIN,
	ENCL_VIDEO_HAVON_END,
	ENCL_VIDEO_VAVON_BLINE,
	ENCL_VIDEO_VAVON_ELINE,
	ENCL_VIDEO_RGBIN_CTRL,
};

static void lcd_lvds_reg_print(void)
{
	unsigned int reg;

	pr_info("\nlvds registers:\n");
	reg = LVDS_PACK_CNTL_ADDR;
	pr_info("LVDS_PACK_CNTL      [0x%04x] = 0x%08x\n",
		reg, lcd_vcbus_read(reg));
	reg = LVDS_GEN_CNTL;
	pr_info("LVDS_GEN_CNTL       [0x%04x] = 0x%08x\n",
		reg, lcd_vcbus_read(reg));
	reg = HHI_LVDS_TX_PHY_CNTL0;
	pr_info("LVDS_PHY_CNTL0      [0x%04x] = 0x%08x\n",
		reg, lcd_hiu_read(reg));
	reg = HHI_LVDS_TX_PHY_CNTL1;
	pr_info("LVDS_PHY_CNTL1      [0x%04x] = 0x%08x\n",
		reg, lcd_hiu_read(reg));
}

static void lcd_vbyone_reg_print(void)
{
	unsigned int reg;

	pr_info("\nvbyone registers:\n");
	reg = PERIPHS_PIN_MUX_7;
	pr_info("VX1_PINMUX          [0x%04x] = 0x%08x\n",
		reg, lcd_periphs_read(reg));
	reg = VBO_STATUS_L;
	pr_info("VX1_STATUS          [0x%04x] = 0x%08x\n",
		reg, lcd_vcbus_read(reg));
	reg = VBO_FSM_HOLDER_L;
	pr_info("VX1_FSM_HOLDER_L    [0x%04x] = 0x%08x\n",
		reg, lcd_vcbus_read(reg));
	reg = VBO_FSM_HOLDER_H;
	pr_info("VX1_FSM_HOLDER_H    [0x%04x] = 0x%08x\n",
		reg, lcd_vcbus_read(reg));
	reg = VBO_INTR_STATE_CTRL;
	pr_info("VX1_INTR_STATE_CTRL [0x%04x] = 0x%08x\n",
		reg, lcd_vcbus_read(reg));
	reg = VBO_INTR_UNMASK;
	pr_info("VX1_INTR_UNMASK     [0x%04x] = 0x%08x\n",
		reg, lcd_vcbus_read(reg));
	reg = VBO_INTR_STATE;
	pr_info("VX1_INTR_STATE      [0x%04x] = 0x%08x\n",
		reg, lcd_vcbus_read(reg));
	reg = HHI_LVDS_TX_PHY_CNTL0;
	pr_info("VX1_PHY_CNTL0       [0x%04x] = 0x%08x\n",
		reg, lcd_hiu_read(reg));
}

static void lcd_phy_analog_reg_print(void)
{
	unsigned int reg;

	pr_info("\nphy analog registers:\n");
	reg = HHI_DIF_CSI_PHY_CNTL1;
	pr_info("PHY_CNTL1           [0x%04x] = 0x%08x\n",
		reg, lcd_hiu_read(reg));
	reg = HHI_DIF_CSI_PHY_CNTL2;
	pr_info("PHY_CNTL2           [0x%04x] = 0x%08x\n",
		reg, lcd_hiu_read(reg));
	reg = HHI_DIF_CSI_PHY_CNTL3;
	pr_info("PHY_CNTL3           [0x%04x] = 0x%08x\n",
		reg, lcd_hiu_read(reg));
}

static void lcd_reg_print(void)
{
	int i;
	struct aml_lcd_drv_s *lcd_drv = aml_lcd_get_driver();
	struct lcd_config_s *pconf;

	pconf = lcd_drv->lcd_config;
	LCDPR("clk registers:\n");
	for (i = 0; i < ARRAY_SIZE(lcd_reg_dump_clk); i++) {
		pr_info("hiu     [0x%04x] = 0x%08x\n",
			lcd_reg_dump_clk[i],
			lcd_hiu_read(lcd_reg_dump_clk[i]));
	}

	pr_info("\nencl registers:\n");
	for (i = 0; i < ARRAY_SIZE(lcd_reg_dump_encl); i++) {
		pr_info("vcbus   [0x%04x] = 0x%08x\n",
			lcd_reg_dump_encl[i],
			lcd_vcbus_read(lcd_reg_dump_encl[i]));
	}

	switch (pconf->lcd_basic.lcd_type) {
	case LCD_TTL:
		break;
	case LCD_LVDS:
		lcd_lvds_reg_print();
		lcd_phy_analog_reg_print();
		break;
	case LCD_VBYONE:
		lcd_vbyone_reg_print();
		lcd_phy_analog_reg_print();
		break;
	case LCD_MIPI:
		break;
	case LCD_EDP:
		break;
	default:
		break;
	}
}

#define LCD_ENC_TST_NUM_MAX    8
static char *lcd_enc_tst_str[] = {
	"0-None",        /* 0 */
	"1-Color Bar",   /* 1 */
	"2-Thin Line",   /* 2 */
	"3-Dot Grid",    /* 3 */
	"4-Gray",        /* 4 */
	"5-Blue",         /* 5 */
	"6-Red",       /* 6 */
	"7-Green",        /* 7 */
};

static unsigned int lcd_enc_tst[][7] = {
/*tst_mode,    Y,       Cb,     Cr,     tst_en,  vfifo_en  rgbin*/
	{0,    0x200,   0x200,  0x200,   0,      1,        3},  /* 0 */
	{1,    0x200,   0x200,  0x200,   1,      0,        1},  /* 1 */
	{2,    0x200,   0x200,  0x200,   1,      0,        1},  /* 2 */
	{3,    0x200,   0x200,  0x200,   1,      0,        1},  /* 3 */
	{0,    0x200,   0x200,  0x200,   1,      0,        1},  /* 4 */
	{0,    0x130,   0x153,  0x3fd,   1,      0,        1},  /* 5 */
	{0,    0x256,   0x0ae,  0x055,   1,      0,        1},  /* 6 */
	{0,    0x074,   0x3fd,  0x1ad,   1,      0,        1},  /* 7 */
};

static void lcd_test(unsigned int num)
{
	unsigned int h_active;
	unsigned int video_on_pixel;
	struct aml_lcd_drv_s *lcd_drv = aml_lcd_get_driver();

	h_active = lcd_drv->lcd_config->lcd_basic.h_active;
	video_on_pixel = lcd_drv->lcd_config->lcd_timing.video_on_pixel;
	num = (num >= LCD_ENC_TST_NUM_MAX) ? 0 : num;
	if (num >= 0) {
		lcd_vcbus_write(ENCL_VIDEO_RGBIN_CTRL, lcd_enc_tst[num][6]);
		lcd_vcbus_write(ENCL_TST_MDSEL, lcd_enc_tst[num][0]);
		lcd_vcbus_write(ENCL_TST_Y, lcd_enc_tst[num][1]);
		lcd_vcbus_write(ENCL_TST_CB, lcd_enc_tst[num][2]);
		lcd_vcbus_write(ENCL_TST_CR, lcd_enc_tst[num][3]);
		lcd_vcbus_write(ENCL_TST_CLRBAR_STRT, video_on_pixel);
		lcd_vcbus_write(ENCL_TST_CLRBAR_WIDTH, (h_active / 9));
		lcd_vcbus_write(ENCL_TST_EN, lcd_enc_tst[num][4]);
		lcd_vcbus_setb(ENCL_VIDEO_MODE_ADV, lcd_enc_tst[num][5], 3, 1);
		pr_info("lcd: show test pattern: %s\n", lcd_enc_tst_str[num]);
	} else {
		pr_info("lcd: disable test pattern\n");
	}
}

static void lcd_debug_reg_write(unsigned int reg, unsigned int data,
		unsigned int type)
{
	if (reg == 0)
		return;

	switch (type) {
	case 0: /* vcbus */
		lcd_vcbus_write(reg, data);
		pr_info("write vcbus [0x%04x] = 0x%08x, readback 0x%08x\n",
			reg, data, lcd_vcbus_read(reg));
		break;
	case 1: /* hiu */
		lcd_hiu_write(reg, data);
		pr_info("write hiu [0x%04x] = 0x%08x, readback 0x%08x\n",
			reg, data, lcd_hiu_read(reg));
		break;
	case 2: /* cbus */
		lcd_cbus_write(reg, data);
		pr_info("write cbus [0x%04x] = 0x%08x, readback 0x%08x\n",
			reg, data, lcd_cbus_read(reg));
		break;
		break;
	case 3: /* periphs */
		lcd_periphs_write(reg, data);
		pr_info("write periphs [0x%04x] = 0x%08x, readback 0x%08x\n",
			reg, data, lcd_cbus_read(reg));
		break;
		break;
	default:
		break;
	}
}

static void lcd_debug_reg_read(unsigned int reg, unsigned int bus)
{
	if (reg == 0)
		return;

	switch (bus) {
	case 0: /* vcbus */
		pr_info("read vcbus [0x%04x] = 0x%08x\n",
			reg, lcd_vcbus_read(reg));
		break;
	case 1: /* hiu */
		pr_info("read hiu [0x%04x] = 0x%08x\n",
			reg, lcd_hiu_read(reg));
		break;
	case 2: /* cbus */
		pr_info("read cbus [0x%04x] = 0x%08x\n",
			reg, lcd_cbus_read(reg));
		break;
	case 3: /* periphs */
		pr_info("read periphs [0x%04x] = 0x%08x\n",
			reg, lcd_periphs_read(reg));
		break;
	default:
		break;
	}
}

static void lcd_debug_reg_dump(unsigned int reg, unsigned int num,
		unsigned int bus)
{
	int i;

	if (reg == 0)
		return;

	switch (bus) {
	case 0: /* vcbus */
		pr_info("dump vcbus regs:\n");
		for (i = 0; i < num; i++) {
			pr_info("[0x%04x] = 0x%08x\n",
				(reg + i), lcd_vcbus_read(reg + i));
		}
		break;
	case 1: /* hiu */
		pr_info("dump hiu-bus regs:\n");
		for (i = 0; i < num; i++) {
			pr_info("[0x%04x] = 0x%08x\n",
				(reg + i), lcd_hiu_read(reg + i));
		}
		break;
	case 2: /* cbus */
		pr_info("dump cbus regs:\n");
		for (i = 0; i < num; i++) {
			pr_info("[0x%04x] = 0x%08x\n",
				(reg + i), lcd_cbus_read(reg + i));
		}
		break;
	case 3: /* periphs */
		pr_info("dump periphs-bus regs:\n");
		for (i = 0; i < num; i++) {
			pr_info("[0x%04x] = 0x%08x\n",
				(reg + i), lcd_periphs_read(reg + i));
		}
		break;
	default:
		break;
	}
}

static ssize_t lcd_debug(struct class *class,
		struct class_attribute *attr, const char *buf, size_t count)
{
	unsigned int ret;
	unsigned int temp;
	struct aml_lcd_drv_s *lcd_drv = aml_lcd_get_driver();

	switch (buf[0]) {
	case 't':
		temp = 0;
		ret = sscanf(buf, "test %d", &temp);
		lcd_test(temp);
		break;
	case 'e':
		lcd_drv->module_enable();
		break;
	case 'i':
		LCDPR("driver version: %s\n", lcd_drv->version);
		lcd_info_print();
		break;
	case 'r':
		LCDPR("driver version: %s\n", lcd_drv->version);
		lcd_reg_print();
		break;
	case 'd':
		LCDPR("driver version: %s\n", lcd_drv->version);
		lcd_info_print();
		pr_info("\n");
		lcd_reg_print();
		break;
	default:
		LCDPR("wrong command\n");
		break;
	}

	if (ret != 1 || ret != 2)
		return -EINVAL;

	return count;
}

static ssize_t lcd_debug_enable(struct class *class,
		struct class_attribute *attr, const char *buf, size_t count)
{
	unsigned int ret;
	unsigned int temp = 1;
	struct aml_lcd_drv_s *lcd_drv = aml_lcd_get_driver();

	ret = sscanf(buf, "%d", &temp);
	if (temp)
		lcd_drv->module_enable();
	else
		lcd_drv->module_disable();

	if (ret != 1 || ret != 2)
		return -EINVAL;

	return count;
}

static ssize_t lcd_debug_frame_rate(struct class *class,
		struct class_attribute *attr, const char *buf, size_t count)
{
	unsigned int ret;
	unsigned int temp;
	struct aml_lcd_drv_s *lcd_drv = aml_lcd_get_driver();

	switch (buf[0]) {
	case 't':
		temp = 0;
		ret = sscanf(buf, "set %d", &temp);
		lcd_drv->lcd_config->lcd_timing.fr_adjust_type = temp;
		pr_info("set fr_adjust_type: %d\n", temp);
		break;
	case 's':
		temp = 60;
		ret = sscanf(buf, "set %d", &temp);
		pr_info("to do\n");
		pr_info("set frame rate: %d\n", temp);
		break;
	default:
		pr_info("wrong command\n");
		break;
	}

	if (ret != 1 || ret != 2)
		return -EINVAL;

	return count;
}

static ssize_t lcd_debug_ss(struct class *class,
		struct class_attribute *attr, const char *buf, size_t count)
{
	unsigned int ret;
	unsigned int temp = 0;

	ret = sscanf(buf, "%d", &temp);
	pr_info("to do\n");

	if (ret != 1 || ret != 2)
		return -EINVAL;

	return count;
}

static ssize_t lcd_debug_test(struct class *class,
		struct class_attribute *attr, const char *buf, size_t count)
{
	unsigned int ret;
	unsigned int temp = 0;

	ret = sscanf(buf, "%d", &temp);
	lcd_test(temp);

	if (ret != 1 || ret != 2)
		return -EINVAL;

	return count;
}

static ssize_t lcd_debug_reg(struct class *class,
		struct class_attribute *attr, const char *buf, size_t count)
{
	unsigned int ret;
	unsigned int bus = 0;
	unsigned int reg32, data32;

	switch (buf[0]) {
	case 'w':
		reg32 = 0;
		if (buf[1] == 'v') {
			ret = sscanf(buf, "wv %x %x", &reg32, &data32);
			bus = 0;
		} else if (buf[1] == 'h') {
			ret = sscanf(buf, "wh %x %x", &reg32, &data32);
			bus = 1;
		} else if (buf[1] == 'c') {
			ret = sscanf(buf, "wc %x %x", &reg32, &data32);
			bus = 2;
		} else if (buf[1] == 'p') {
			ret = sscanf(buf, "wp %x %x", &reg32, &data32);
			bus = 3;
		}
		lcd_debug_reg_write(reg32, data32, bus);
		break;
	case 'r':
		reg32 = 0;
		if (buf[1] == 'v') {
			ret = sscanf(buf, "rv %x", &reg32);
			bus = 0;
		} else if (buf[1] == 'h') {
			ret = sscanf(buf, "rh %x", &reg32);
			bus = 1;
		} else if (buf[1] == 'c') {
			ret = sscanf(buf, "rc %x", &reg32);
			bus = 2;
		} else if (buf[1] == 'p') {
			ret = sscanf(buf, "rp %x", &reg32);
			bus = 3;
		}
		lcd_debug_reg_read(reg32, bus);
		break;
	case 'd':
		reg32 = 0;
		if (buf[1] == 'v') {
			ret = sscanf(buf, "dv %x %d", &reg32, &data32);
			bus = 0;
		} else if (buf[1] == 'h') {
			ret = sscanf(buf, "dh %x %d", &reg32, &data32);
			bus = 1;
		} else if (buf[1] == 'c') {
			ret = sscanf(buf, "dc %x %d", &reg32, &data32);
			bus = 2;
		} else if (buf[1] == 'p') {
			ret = sscanf(buf, "dp %x %d", &reg32, &data32);
			bus = 3;
		}
		lcd_debug_reg_dump(reg32, data32, bus);
		break;
	default:
		break;
	}

	if (ret != 1 || ret != 2)
		return -EINVAL;

	return count;
}

static ssize_t lcd_debug_print(struct class *class,
		struct class_attribute *attr, const char *buf, size_t count)
{
	unsigned int ret;
	unsigned int temp = 0;

	ret = sscanf(buf, "%d", &temp);
	lcd_debug_print_flag = temp;
	LCDPR("set debug print flag: %d\n", lcd_debug_print_flag);

	if (ret != 1 || ret != 2)
		return -EINVAL;

	return count;
}

static struct class_attribute lcd_debug_class_attrs[] = {
	__ATTR(help,  S_IRUGO | S_IWUSR, lcd_debug_common_help, NULL),
	__ATTR(debug,  S_IRUGO | S_IWUSR, lcd_debug_help, lcd_debug),
	__ATTR(enable,  S_IRUGO | S_IWUSR, NULL, lcd_debug_enable),
	__ATTR(frame_rate,  S_IRUGO | S_IWUSR, NULL, lcd_debug_frame_rate),
	__ATTR(ss,  S_IRUGO | S_IWUSR, NULL, lcd_debug_ss),
	__ATTR(test,  S_IRUGO | S_IWUSR, NULL, lcd_debug_test),
	__ATTR(reg,  S_IRUGO | S_IWUSR, NULL, lcd_debug_reg),
	__ATTR(print,  S_IRUGO | S_IWUSR, NULL, lcd_debug_print),
};

static struct class *lcd_debug_class;

int lcd_class_creat(void)
{
	int i;

	lcd_debug_class = class_create(THIS_MODULE, "lcd");
	if (IS_ERR(lcd_debug_class)) {
		pr_info("create lcd debug class fail\n");
		return -1;
	}

	for (i = 0; i < ARRAY_SIZE(lcd_debug_class_attrs); i++) {
		if (class_create_file(lcd_debug_class,
			&lcd_debug_class_attrs[i])) {
			pr_info("create lcd debug attribute %s fail\n",
				lcd_debug_class_attrs[i].attr.name);
		}
	}

	return 0;
}

int lcd_class_remove(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(lcd_debug_class_attrs); i++)
		class_remove_file(lcd_debug_class, &lcd_debug_class_attrs[i]);

	class_destroy(lcd_debug_class);
	lcd_debug_class = NULL;

	return 0;
}

