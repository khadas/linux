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
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/amlogic/media/vout/lcd/aml_bl.h>
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#include <linux/amlogic/media/vout/lcd/lcd_tcon_data.h>
#include <linux/amlogic/media/vout/lcd/lcd_notify.h>
#include <linux/amlogic/media/vout/lcd/lcd_unifykey.h>
#include "lcd_reg.h"
#include "lcd_common.h"
#ifdef CONFIG_AMLOGIC_LCD_TABLET
#include <linux/amlogic/media/vout/lcd/lcd_mipi.h>
#include "lcd_tablet/mipi_dsi_util.h"
#endif
#include "lcd_debug.h"
#include "./lcd_clk/lcd_clk_config.h"

/*device attribute buf max size 4k*/
#define PR_BUF_MAX          (4 * 1024)

void lcd_debug_parse_param(char *buf_orig, char **parm)
{
	char *ps, *token;
	char str[3] = {' ', '\n', '\0'};
	unsigned int n = 0;

	ps = buf_orig;
	while (1) {
		token = strsep(&ps, str);
		if (!token)
			break;
		if (*token == '\0')
			continue;
		parm[n++] = token;
	}
}

static void lcd_debug_info_print(char *print_buf)
{
	char *ps, *token;
	char str[3] = {'\n', '\0'};

	ps = print_buf;
	while (1) {
		token = strsep(&ps, str);
		if (!token)
			break;
		if (*token == '\0') {
			pr_info("\n");
			continue;
		}
		pr_info("%s\n", token);
	}
}

int lcd_debug_info_len(int num)
{
	int ret = 0;

	if (num >= (PR_BUF_MAX - 1)) {
		pr_info("%s: string length %d is out of support\n",
			__func__, num);
		return 0;
	}

	ret = PR_BUF_MAX - 1 - num;
	return ret;
}

static const char *lcd_common_usage_str = {
"Usage:\n"
"    echo <0|1> > enable ; 0=disable lcd; 1=enable lcd\n"
"\n"
"    echo type <adj_type> > frame_rate ; set lcd frame rate adjust type\n"
"    echo set <frame_rate> > frame_rate ; set lcd frame rate(unit in 1/100Hz)\n"
"    cat frame_rate ; read current lcd frame rate\n"
"\n"
"    echo <num> > test ; show lcd bist pattern(1~7), 0=disable bist\n"
"\n"
"    echo level <val> > ss ; set lcd clk spread spectrum level\n"
"    echo freq <val> > ss ; set lcd clk spread spectrum freq\n"
"    echo mode <val> > ss ; set lcd clk spread spectrum mode\n"
"    cat ss ; show lcd clk spread spectrum information\n"
"\n"
"    echo w<v|h|c|p|mh|mp|t> <reg> <data> > reg ; write data to vcbus|hiu|cbus|periphs|mipi host|mipi phy reg\n"
"    echo r<v|h|c|p|mh|mp|t> <reg> > reg ; read vcbus|hiu|cbus|periphs|mipi host|mipi phy reg\n"
"    echo d<v|h|c|p|mh|mp|t> <reg> <num> > reg ; dump vcbus|hiu|cbus|periphs|mipi host|mipi phy regs\n"
"\n"
"    echo <0|1> > print ; 0=disable debug print; 1=enable debug print\n"
"    cat print ; read current debug print flag\n"
"\n"
"    echo <cmd> > dump ; change dump info\n"
"    cat dump ; read specified info\n"
"data format:\n"
"    <cmd>   : info, interface, reg, reg2, power, hdr\n"
"\n"
"    echo <cmd> ... > debug ; lcd common debug, use 'cat debug' for help\n"
"    cat debug ; print help information for debug command\n"
"\n"
"    echo <0|1> > power; lcd power control: 0=lcd power off, 1=lcd power on\n"
"    echo <on|off> <step_num> <delay> > power_step ; set power on/off step delay(unit: ms)\n"
"    cat power ; print lcd power on/off step\n"
"\n"
"    cat key_valid ; print lcd_key_valid setting\n"
"    cat config_load ; print lcd_config load_id(0=dts, 1=unifykey)\n"
};

static const char *lcd_debug_usage_str = {
"Usage:\n"
"    echo clk <freq> > debug ; set lcd pixel clock, unit in Hz\n"
"    echo bit <lcd_bits> > debug ; set lcd bits\n"
"    echo basic <h_active> <v_active> <h_period> <v_period> <lcd_bits> > debug ; set lcd basic config\n"
"    echo sync <hs_width> <hs_bp> <hs_pol> <vs_width> <vs_bp> <vs_pol> > debug ; set lcd sync timing\n"
"data format:\n"
"    <xx_pol>       : 0=negative, 1=positive\n"
"\n"
"    echo info > debug ; show lcd information\n"
"    echo reg > debug ; show lcd registers\n"
"    echo dump > debug ; show lcd information & registers\n"
"    echo dith <dither_en> <rounding_en> <dither_md>  > debug ; set vpu_vencl_dith_ctrl\n"
"    echo key > debug ; show lcd_key_valid config, and lcd unifykey raw data\n"
"\n"
"    echo reset > debug; reset lcd driver\n"
"    echo power <0|1> > debug ; lcd power control: 0=power off, 1=power on\n"
};

static const char *lcd_debug_change_usage_str = {
"Usage:\n"
"    echo clk <freq> > change ; change lcd pixel clock, unit in Hz\n"
"    echo bit <lcd_bits> > change ; change lcd bits\n"
"    echo basic <h_active> <v_active> <h_period> <v_period> <lcd_bits> > change ; change lcd basic config\n"
"    echo sync <hs_width> <hs_bp> <hs_pol> <vs_width> <vs_bp> <vs_pol> > change ; change lcd sync timing\n"
"data format:\n"
"    <xx_pol>       : 0=negative, 1=positive\n"
"\n"
"    echo set > change; apply lcd config changing\n"
};

static ssize_t lcd_debug_common_help(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", lcd_common_usage_str);
}

static ssize_t lcd_debug_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", lcd_debug_usage_str);
}

static int lcd_cpu_gpio_register_print(struct lcd_config_s *pconf, char *buf, int offset)
{
	int i, n, len = 0;
	struct lcd_cpu_gpio_s *cpu_gpio;

	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n, "cpu_gpio register:\n");

	i = 0;
	while (i < LCD_CPU_GPIO_NUM_MAX) {
		cpu_gpio = &pconf->power.cpu_gpio[i];
		if (cpu_gpio->probe_flag == 0) {
			i++;
			continue;
		}

		if (cpu_gpio->register_flag) {
			n = lcd_debug_info_len(len + offset);
			len += snprintf((buf + len), n,
				"%d: name=%s, gpio=%p\n",
				i, cpu_gpio->name, cpu_gpio->gpio);
		} else {
			n = lcd_debug_info_len(len + offset);
			len += snprintf((buf + len), n,
				"%d: name=%s, no registered\n",
				i, cpu_gpio->name);
		}
		i++;
	}

	return len;
}

static int lcd_power_step_print(struct lcd_config_s *pconf, int status, char *buf, int offset)
{
	int i, n, len = 0;
	struct lcd_power_step_s *power_step;

	n = lcd_debug_info_len(len + offset);
	if (status)
		len += snprintf((buf + len), n, "power on step:\n");
	else
		len += snprintf((buf + len), n, "power off step:\n");

	i = 0;
	while (i < LCD_PWR_STEP_MAX) {
		if (status)
			power_step = &pconf->power.power_on_step[i];
		else
			power_step = &pconf->power.power_off_step[i];

		if (power_step->type >= LCD_POWER_TYPE_MAX)
			break;
		switch (power_step->type) {
		case LCD_POWER_TYPE_CPU:
		case LCD_POWER_TYPE_PMU:
		case LCD_POWER_TYPE_WAIT_GPIO:
		case LCD_POWER_TYPE_CLK_SS:
			n = lcd_debug_info_len(len + offset);
			len += snprintf((buf + len), n,
				"%d: type=%d, index=%d, value=%d, delay=%d\n",
				i, power_step->type, power_step->index,
				power_step->value, power_step->delay);
			break;
		case LCD_POWER_TYPE_EXTERN:
			n = lcd_debug_info_len(len + offset);
			len += snprintf((buf + len), n,
				"%d: type=%d, index=%d, delay=%d\n",
				i, power_step->type, power_step->index,
				power_step->delay);
			break;
		case LCD_POWER_TYPE_SIGNAL:
			n = lcd_debug_info_len(len + offset);
			len += snprintf((buf + len), n,
				"%d: type=%d, delay=%d\n",
				i, power_step->type, power_step->delay);
			break;
		default:
			break;
		}
		i++;
	}

	return len;
}

static int lcd_power_step_info_print(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	int len = 0;

	len += lcd_power_step_print(&pdrv->config, 1,
		(buf + len), (len + offset));
	len += lcd_power_step_print(&pdrv->config, 0,
		(buf + len), (len + offset));
	len += lcd_cpu_gpio_register_print(&pdrv->config,
		(buf + len), (len + offset));

	return len;
}

static int lcd_info_print_rgb(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	int n, len = 0;

	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n,
		"type            %u\n"
		"clk_pol         %u\n"
		"de_valid        %u\n"
		"sync_valid      %u\n"
		"rb_swap         %u\n"
		"bit_swap        %u\n\n",
		pdrv->config.control.rgb_cfg.type,
		pdrv->config.control.rgb_cfg.clk_pol,
		pdrv->config.control.rgb_cfg.de_valid,
		pdrv->config.control.rgb_cfg.sync_valid,
		pdrv->config.control.rgb_cfg.rb_swap,
		pdrv->config.control.rgb_cfg.bit_swap);

	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n,
		"pinmux_flag     %d\n"
		"pinmux_pointer  0x%p\n\n",
		pdrv->config.pinmux_flag,
		pdrv->config.pin);

	return len;
}

static int lcd_info_print_bt(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	int n, len = 0;

	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n,
		"clk_phase       %u\n"
		"field_type      %u\n"
		"mode_422        %u\n"
		"yc_swap         %u\n"
		"cbcr_swap       %u\n\n",
		pdrv->config.control.bt_cfg.clk_phase,
		pdrv->config.control.bt_cfg.field_type,
		pdrv->config.control.bt_cfg.mode_422,
		pdrv->config.control.bt_cfg.yc_swap,
		pdrv->config.control.bt_cfg.cbcr_swap);

	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n,
		"pinmux_flag     %d\n"
		"pinmux_pointer  0x%p\n\n",
		pdrv->config.pinmux_flag,
		pdrv->config.pin);

	return len;
}

static int lcd_info_print_lvds(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	int n, len = 0;

	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n,
		"lvds_repack     %u\n"
		"dual_port       %u\n"
		"pn_swap         %u\n"
		"port_swap       %u\n"
		"lane_reverse    %u\n"
		"phy_vswing      0x%x\n"
		"phy_preem       0x%x\n\n",
		pdrv->config.control.lvds_cfg.lvds_repack,
		pdrv->config.control.lvds_cfg.dual_port,
		pdrv->config.control.lvds_cfg.pn_swap,
		pdrv->config.control.lvds_cfg.port_swap,
		pdrv->config.control.lvds_cfg.lane_reverse,
		pdrv->config.control.lvds_cfg.phy_vswing,
		pdrv->config.control.lvds_cfg.phy_preem);

	return len;
}

static int lcd_info_print_vbyone(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	struct vbyone_config_s *vx1_conf;
	int n, len = 0;

	vx1_conf = &pdrv->config.control.vbyone_cfg;

	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n,
		"lane_count      %u\n"
		"region_num      %u\n"
		"byte_mode       %u\n"
		"color_fmt       %u\n"
		"bit_rate        %lluHz\n"
		"phy_vswing      0x%x\n"
		"phy_preem       0x%x\n"
		"intr_en         %u\n"
		"vsync_intr_en   %u\n"
		"hw_filter_time  0x%x\n"
		"hw_filter_cnt   0x%x\n"
		"ctrl_flag       0x%x\n\n",
		vx1_conf->lane_count,
		vx1_conf->region_num,
		vx1_conf->byte_mode,
		vx1_conf->color_fmt,
		pdrv->config.timing.bit_rate,
		vx1_conf->phy_vswing,
		vx1_conf->phy_preem,
		vx1_conf->intr_en,
		vx1_conf->vsync_intr_en,
		vx1_conf->hw_filter_time,
		vx1_conf->hw_filter_cnt,
		vx1_conf->ctrl_flag);
	if (vx1_conf->ctrl_flag & 0x1) {
		n = lcd_debug_info_len(len + offset);
		len += snprintf((buf + len), n,
			"power_on_reset_en    %u\n"
			"power_on_reset_delay %ums\n\n",
			(vx1_conf->ctrl_flag & 0x1),
			vx1_conf->power_on_reset_delay);
	}
	if (vx1_conf->ctrl_flag & 0x2) {
		n = lcd_debug_info_len(len + offset);
		len += snprintf((buf + len), n,
			"hpd_data_delay_en    %u\n"
			"hpd_data_delay       %ums\n\n",
			((vx1_conf->ctrl_flag >> 1) & 0x1),
			vx1_conf->hpd_data_delay);
	}
	if (vx1_conf->ctrl_flag & 0x4) {
		n = lcd_debug_info_len(len + offset);
		len += snprintf((buf + len), n,
			"cdr_training_hold_en %u\n"
			"cdr_training_hold    %ums\n\n",
			((vx1_conf->ctrl_flag >> 2) & 0x1),
			vx1_conf->cdr_training_hold);
	}

	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n,
		"pinmux_flag          %d\n"
		"pinmux_pointer       0x%p\n\n",
		pdrv->config.pinmux_flag,
		pdrv->config.pin);

	return len;
}

static int lcd_info_print_mipi(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	int len = 0;

#ifdef CONFIG_AMLOGIC_LCD_TABLET
	mipi_dsi_print_info(&pdrv->config);
#endif

	return len;
}

static int lcd_info_print_edp(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	int n, len = 0;

	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n,
		"max_lane_count       %d\n"
		"max_link_rate        %d\n"
		"training_mode        %d\n"
		"dpcd_caps_en         %d\n"
		"sync_clk_mode        %d\n\n"
		"lane_count           %d\n"
		"link_rate            %d\n"
		"bit_rate             %lluHz\n"
		"training_settings    0x%x\n"
		"main_stream_enable   %d\n"
		"phy_vswing           0x%x\n"
		"phy_preem            0x%x\n\n",
		pdrv->config.control.edp_cfg.max_lane_count,
		pdrv->config.control.edp_cfg.max_link_rate,
		pdrv->config.control.edp_cfg.training_mode,
		pdrv->config.control.edp_cfg.dpcd_caps_en,
		pdrv->config.control.edp_cfg.sync_clk_mode,
		pdrv->config.control.edp_cfg.lane_count,
		pdrv->config.control.edp_cfg.link_rate,
		pdrv->config.timing.bit_rate,
		pdrv->config.control.edp_cfg.training_settings,
		pdrv->config.control.edp_cfg.main_stream_enable,
		pdrv->config.control.edp_cfg.phy_vswing_preset,
		pdrv->config.control.edp_cfg.phy_preem_preset);

	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n,
		"pinmux_flag       %d\n"
		"pinmux_pointer    0x%p\n\n",
		pdrv->config.pinmux_flag,
		pdrv->config.pin);

	return len;
}

static int lcd_info_print_mlvds(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	int n, len = 0;

	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n,
		"channel_num       %d\n"
		"channel_sel0      0x%08x\n"
		"channel_sel1      0x%08x\n"
		"clk_phase         0x%04x\n"
		"pn_swap           %u\n"
		"bit_swap          %u\n"
		"phy_vswing        0x%x\n"
		"phy_preem         0x%x\n"
		"bit_rate          %lluHz\n"
		"pi_clk_sel        0x%03x\n\n",
		pdrv->config.control.mlvds_cfg.channel_num,
		pdrv->config.control.mlvds_cfg.channel_sel0,
		pdrv->config.control.mlvds_cfg.channel_sel1,
		pdrv->config.control.mlvds_cfg.clk_phase,
		pdrv->config.control.mlvds_cfg.pn_swap,
		pdrv->config.control.mlvds_cfg.bit_swap,
		pdrv->config.control.mlvds_cfg.phy_vswing,
		pdrv->config.control.mlvds_cfg.phy_preem,
		pdrv->config.timing.bit_rate,
		pdrv->config.control.mlvds_cfg.pi_clk_sel);

	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n,
		"pinmux_flag       %d\n"
		"pinmux_pointer    0x%p\n\n",
		pdrv->config.pinmux_flag,
		pdrv->config.pin);

	return len;
}

static int lcd_info_print_p2p(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	int n, len = 0;

	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n,
		"p2p_type          0x%x\n"
		"lane_num          %d\n"
		"channel_sel0      0x%08x\n"
		"channel_sel1      0x%08x\n"
		"pn_swap           %u\n"
		"bit_swap          %u\n"
		"bit_rate          %lluHz\n"
		"phy_vswing        0x%x\n"
		"phy_preem         0x%x\n\n",
		pdrv->config.control.p2p_cfg.p2p_type,
		pdrv->config.control.p2p_cfg.lane_num,
		pdrv->config.control.p2p_cfg.channel_sel0,
		pdrv->config.control.p2p_cfg.channel_sel1,
		pdrv->config.control.p2p_cfg.pn_swap,
		pdrv->config.control.p2p_cfg.bit_swap,
		pdrv->config.timing.bit_rate,
		pdrv->config.control.p2p_cfg.phy_vswing,
		pdrv->config.control.p2p_cfg.phy_preem);

	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n,
		"pinmux_flag       %d\n"
		"pinmux_pointer    0x%p\n\n",
		pdrv->config.pinmux_flag,
		pdrv->config.pin);

	return len;
}

static int lcd_info_basic_print(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	struct lcd_config_s *pconf;
	unsigned int sync_duration, mute_state = 0;
	int n, len = 0;

	pconf = &pdrv->config;
	sync_duration = pconf->timing.sync_duration_num * 100;
	sync_duration = sync_duration / pconf->timing.sync_duration_den;
#ifdef CONFIG_AMLOGIC_MEDIA_VIDEO
	mute_state = (pdrv->viu_sel == 1) ? get_output_mute() : 0;
#endif

	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n,
		"[%d]: driver version: %s\n"
		"panel_type: %s, chip: %d, mode: %s, status: %d\n"
		"viu_sel: %d, isr_cnt: %d, resume_type: %d, resume_flag: 0x%x\n"
		"fr_auto_flag: 0x%x, fr_mode: %d, fr_duration: %d, frame_rate: %d\n"
		"fr_auto_policy(global): %d, fr_auto_cus: 0x%x, custom_pinmux: %d\n"
		"mute_state: %d, test_flag: 0x%x\n"
		"key_valid: %d, config_load: %d\n",
		pdrv->index, LCD_DRV_VERSION,
		pconf->propname, pdrv->data->chip_type,
		lcd_mode_mode_to_str(pdrv->mode), pdrv->status,
		pdrv->viu_sel, pdrv->vsync_cnt,
		pdrv->resume_type, pdrv->resume_flag,
		pconf->fr_auto_flag, pdrv->fr_mode, pdrv->fr_duration,
		pconf->timing.frame_rate,
		pdrv->fr_auto_policy, pconf->fr_auto_cus, pconf->custom_pinmux,
		mute_state, pdrv->test_flag,
		pdrv->key_valid, pdrv->config_load);

	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n,
		"%s, %s %ubit, %ux%u@%u.%02uHz\n",
		pconf->basic.model_name,
		lcd_type_type_to_str(pconf->basic.lcd_type),
		pconf->basic.lcd_bits,
		pconf->basic.h_active, pconf->basic.v_active,
		(sync_duration / 100), (sync_duration % 100));

	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n,
		"lcd_clk        %uHz\n"
		"ss_level       %d\n"
		"ss_freq        %d\n"
		"ss_mode        %d\n"
		"clk_auto       %d\n"
		"fr_adj_type    %d\n\n",
		pconf->timing.lcd_clk, pconf->timing.ss_level,
		pconf->timing.ss_freq, pconf->timing.ss_mode,
		pconf->timing.clk_auto, pconf->timing.fr_adjust_type);

	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n,
		"h_period       %d\n"
		"v_period       %d\n"
		"hs_width       %d\n"
		"hs_backporch   %d\n"
		"hs_pol         %d\n"
		"vs_width       %d\n"
		"vs_backporch   %d\n"
		"vs_pol         %d\n\n",
		pconf->basic.h_period, pconf->basic.v_period,
		pconf->timing.hsync_width, pconf->timing.hsync_bp,
		pconf->timing.hsync_pol,
		pconf->timing.vsync_width, pconf->timing.vsync_bp,
		pconf->timing.vsync_pol);
	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n,
		"h_period_min   %d\n"
		"h_period_max   %d\n"
		"v_period_min   %d\n"
		"v_period_max   %d\n"
		"frame_rate_min %d\n"
		"frame_rate_max %d\n"
		"pclk_min       %d\n"
		"pclk_max       %d\n\n",
		pconf->basic.h_period_min, pconf->basic.h_period_max,
		pconf->basic.v_period_min, pconf->basic.v_period_max,
		pconf->basic.frame_rate_min, pconf->basic.frame_rate_max,
		pconf->basic.lcd_clk_min, pconf->basic.lcd_clk_max);

	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n,
		"pll_ctrl       0x%08x\n"
		"div_ctrl       0x%08x\n"
		"clk_ctrl       0x%08x\n"
		"video_on_pixel %d\n"
		"video_on_line  %d\n\n",
		pconf->timing.pll_ctrl, pconf->timing.div_ctrl,
		pconf->timing.clk_ctrl,
		pconf->timing.hstart,
		pconf->timing.vstart);

	return len;
}

static int lcd_info_adv_print(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	struct lcd_debug_info_s *lcd_debug_info;
	struct phy_config_s *phy = NULL;
	int i, n, len = 0;

	lcd_debug_info = (struct lcd_debug_info_s *)pdrv->debug_info;
	if (!lcd_debug_info) {
		n = lcd_debug_info_len(len + offset);
		len += snprintf((buf + len), n, "%s: debug_info is null\n", __func__);
		return len;
	}
	phy = &pdrv->config.phy_cfg;

	if (lcd_debug_info->debug_if && lcd_debug_info->debug_if->interface_print)
		len += lcd_debug_info->debug_if->interface_print(pdrv, (buf + len), (len + offset));

	/* phy_attr */
	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n,
			"lcd phy_attr:\n"
			"ctrl_flag:     0x%x\n"
			"vswing_level:  %u\n"
			"ext_pullup:    %u\n"
			"preem_level:   %u\n"
			"vcm:           0x%x\n"
			"ref_bias:      0x%x\n"
			"odt:           0x%x\n",
			phy->flag,
			phy->vswing,
			phy->ext_pullup,
			phy->preem_level,
			phy->vcm,
			phy->ref_bias,
			phy->odt);
	for (i = 0; i < phy->lane_num; i++) {
		n = lcd_debug_info_len(len + offset);
		len += snprintf((buf + len), n,
				"lane%d_amp:    0x%x\n"
				"lane%d_preem:  0x%x\n",
				i, phy->lane[i].amp,
				i, phy->lane[i].preem);
	}

	/* cus_ctrl_attr */
	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n,
			"\nlcd cus_ctrl:\n"
			"ctrl_flag:     0x%x\n"
			"dlg_flag:      %u\n"
			"dlg_time:      %llu\n"
			"attr_0_para0:  %u\n"
			"attr_0_para1:  %u\n",
			pdrv->config.cus_ctrl.flag,
			pdrv->config.cus_ctrl.dlg_flag,
			pdrv->config.cus_ctrl.dlg_time,
			pdrv->config.cus_ctrl.attr_0_para0,
			pdrv->config.cus_ctrl.attr_0_para1);

	return len;
}

static ssize_t lcd_dlg_time_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	ssize_t len = 0;

	if (pdrv->config.cus_ctrl.dlg_flag == 3) {
		len = sprintf(buf, "dlg times attr:\n"
				"ctrl_flag:     0x%x\n"
				"dlg_flag:      %u\n"
				"mute_time:      %llu\n"
				"switch_time:      %llu\n"
				"level_shfit_time:      %llu\n"
				"tcon_reload_time(total):      %llu\n"
				"tcon_reg_set_time:      %llu\n"
				"tcon_data_set_time:      %llu\n"
				"driver_change_time:      %llu\n"
				"unmute_time:      %llu\n"
				"dlg_time:      %llu\n",
				pdrv->config.cus_ctrl.flag,
				pdrv->config.cus_ctrl.dlg_flag,
				pdrv->config.cus_ctrl.mute_time,
				pdrv->config.cus_ctrl.switch_time,
				pdrv->config.cus_ctrl.level_shift_time,
				pdrv->config.cus_ctrl.tcon_reload_time,
				pdrv->config.cus_ctrl.reg_set_time,
				pdrv->config.cus_ctrl.data_set_time,
				pdrv->config.cus_ctrl.driver_change_time,
				pdrv->config.cus_ctrl.unmute_time,
				pdrv->config.cus_ctrl.dlg_time);
	} else if (pdrv->config.cus_ctrl.dlg_flag == 2) {
		len = sprintf(buf, "dlg times attr:\n"
				"ctrl_flag:     0x%x\n"
				"dlg_flag:      %u\n"
				"mute_time:      %llu\n"
				"bl_off_time:      %llu\n"
				"driver_disable_time:      %llu\n"
				"power_off_time:      %llu\n"
				"driver_init_time:      %llu\n"
				"level_shfit_time:      %llu\n"
				"bl_on_time:      %llu\n"
				"unmute_time:      %llu\n"
				"switch_time:      %llu\n"
				"driver_change_time:      %llu\n"
				"dlg_time:      %llu\n",
				pdrv->config.cus_ctrl.flag,
				pdrv->config.cus_ctrl.dlg_flag,
				pdrv->config.cus_ctrl.mute_time,
				pdrv->config.cus_ctrl.bl_off_time,
				pdrv->config.cus_ctrl.driver_disable_time,
				pdrv->config.cus_ctrl.power_off_time,
				pdrv->config.cus_ctrl.driver_init_time,
				pdrv->config.cus_ctrl.level_shift_time,
				pdrv->config.cus_ctrl.bl_on_time,
				pdrv->config.cus_ctrl.unmute_time,
				pdrv->config.cus_ctrl.switch_time,
				pdrv->config.cus_ctrl.driver_change_time,
				pdrv->config.cus_ctrl.dlg_time);
	} else {
	}

	return len;
}

static int lcd_info_tcon_print(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	int len = 0;

	if (pdrv->config.basic.lcd_type != LCD_MLVDS &&
	    pdrv->config.basic.lcd_type != LCD_P2P)
		return len;

	len = lcd_tcon_info_print(buf, offset);

	return len;
}

static int lcd_reg_print_rgb(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	int n, len = 0;

	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n, "\nrgb regs: todo\n");

	return len;
}

static int lcd_reg_print_bt(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	int n, len = 0;

	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n, "\nbt regs: todo\n");

	return len;
}

static int lcd_reg_print_lvds(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	unsigned int reg;
	int n, len = 0;

	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n, "\nlvds regs:\n");

	n = lcd_debug_info_len(len + offset);
	reg = LVDS_PACK_CNTL_ADDR;
	len += snprintf((buf + len), n,
		"LVDS_PACK_CNTL  [0x%04x] = 0x%08x\n",
		reg, lcd_vcbus_read(reg));
	n = lcd_debug_info_len(len + offset);
	reg = LVDS_GEN_CNTL;
	len += snprintf((buf + len), n,
		"LVDS_GEN_CNTL   [0x%04x] = 0x%08x\n",
		reg, lcd_vcbus_read(reg));
	n = lcd_debug_info_len(len + offset);
	reg = P2P_CH_SWAP0;
	len += snprintf((buf + len), n,
		"P2P_CH_SWAP0    [0x%04x] = 0x%08x\n",
		reg, lcd_vcbus_read(reg));
	n = lcd_debug_info_len(len + offset);
	reg = P2P_CH_SWAP1;
	len += snprintf((buf + len), n,
		"P2P_CH_SWAP1    [0x%04x] = 0x%08x\n",
		reg, lcd_vcbus_read(reg));

	return len;
}

static int lcd_reg_print_lvds_t7(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	unsigned int reg, reg_offset;
	int n, len = 0;

	reg_offset = pdrv->data->offset_venc_if[pdrv->index];

	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n, "\nlvds regs:\n");

	n = lcd_debug_info_len(len + offset);
	reg = LVDS_SER_EN_T7 + reg_offset;
	len += snprintf((buf + len), n,
		"LVDS_SER_EN     [0x%04x] = 0x%08x\n",
		reg, lcd_vcbus_read(reg));
	n = lcd_debug_info_len(len + offset);
	reg = LVDS_PACK_CNTL_ADDR_T7 + reg_offset;
	len += snprintf((buf + len), n,
		"LVDS_PACK_CNTL  [0x%04x] = 0x%08x\n",
		reg, lcd_vcbus_read(reg));
	n = lcd_debug_info_len(len + offset);
	reg = LVDS_GEN_CNTL_T7 + reg_offset;
	len += snprintf((buf + len), n,
		"LVDS_GEN_CNTL   [0x%04x] = 0x%08x\n",
		reg, lcd_vcbus_read(reg));
	n = lcd_debug_info_len(len + offset);
	reg = P2P_CH_SWAP0_T7 + reg_offset;
	len += snprintf((buf + len), n,
		"P2P_CH_SWAP0    [0x%04x] = 0x%08x\n",
		reg, lcd_vcbus_read(reg));
	n = lcd_debug_info_len(len + offset);
	reg = P2P_CH_SWAP1_T7 + reg_offset;
	len += snprintf((buf + len), n,
		"P2P_CH_SWAP1    [0x%04x] = 0x%08x\n",
		reg, lcd_vcbus_read(reg));
	n = lcd_debug_info_len(len + offset);
	reg = P2P_BIT_REV_T7 + reg_offset;
	len += snprintf((buf + len), n,
		"P2P_BIT_REV     [0x%04x] = 0x%08x\n",
		reg, lcd_vcbus_read(reg));

	return len;
}

static int lcd_reg_print_vbyone(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	unsigned int reg, reg_offset;
	int n, len = 0;

	reg_offset = pdrv->data->offset_venc_if[pdrv->index];

	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n, "\nvbyone regs:\n");

	n = lcd_debug_info_len(len + offset);
	reg = VBO_STATUS_L + reg_offset;
	len += snprintf((buf + len), n,
		"VX1_STATUS                   [0x%04x] = 0x%08x\n",
		reg, lcd_vcbus_read(reg));

	n = lcd_debug_info_len(len + offset);
	reg = VBO_INFILTER_TICK_PERIOD_H + reg_offset;
	len += snprintf((buf + len), n,
		"VBO_INFILTER_TICK_PERIOD_H   [0x%04x] = 0x%08x\n",
		reg, lcd_vcbus_read(reg));
	n = lcd_debug_info_len(len + offset);
	reg = VBO_INFILTER_TICK_PERIOD_L + reg_offset;
	len += snprintf((buf + len), n,
		"VBO_INFILTER_TICK_PERIOD_L   [0x%04x] = 0x%08x\n",
		reg, lcd_vcbus_read(reg));
	n = lcd_debug_info_len(len + offset);
	reg = VBO_INSGN_CTRL + reg_offset;
	len += snprintf((buf + len), n,
		"VBO_INSGN_CTRL               [0x%04x] = 0x%08x\n",
		reg, lcd_vcbus_read(reg));

	n = lcd_debug_info_len(len + offset);
	reg = VBO_FSM_HOLDER_L + reg_offset;
	len += snprintf((buf + len), n,
		"VX1_FSM_HOLDER_L             [0x%04x] = 0x%08x\n",
		reg, lcd_vcbus_read(reg));
	n = lcd_debug_info_len(len + offset);
	reg = VBO_FSM_HOLDER_H + reg_offset;
	len += snprintf((buf + len), n,
		"VX1_FSM_HOLDER_H             [0x%04x] = 0x%08x\n",
		reg, lcd_vcbus_read(reg));
	n = lcd_debug_info_len(len + offset);
	reg = VBO_INTR_STATE_CTRL + reg_offset;
	len += snprintf((buf + len), n,
		"VX1_INTR_STATE_CTRL          [0x%04x] = 0x%08x\n",
		reg, lcd_vcbus_read(reg));
	n = lcd_debug_info_len(len + offset);
	reg = VBO_INTR_UNMASK + reg_offset;
	len += snprintf((buf + len), n,
		"VX1_INTR_UNMASK              [0x%04x] = 0x%08x\n",
		reg, lcd_vcbus_read(reg));
	n = lcd_debug_info_len(len + offset);
	reg = VBO_INTR_STATE + reg_offset;
	len += snprintf((buf + len), n,
		"VX1_INTR_STATE               [0x%04x] = 0x%08x\n",
		reg, lcd_vcbus_read(reg));

	return len;
}

static int lcd_reg_print_vbyone_t7(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	unsigned int reg, reg_offset;
	int n, len = 0;

	reg_offset = pdrv->data->offset_venc_if[pdrv->index];

	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n, "\nvbyone regs:\n");

	n = lcd_debug_info_len(len + offset);
	reg = VBO_STATUS_L_T7 + reg_offset;
	len += snprintf((buf + len), n,
		"VX1_STATUS                   [0x%04x] = 0x%08x\n",
		reg, lcd_vcbus_read(reg));

	n = lcd_debug_info_len(len + offset);
	reg = VBO_INFILTER_CTRL_H_T7 + reg_offset;
	len += snprintf((buf + len), n,
		"VBO_INFILTER_CTRL_H          [0x%04x] = 0x%08x\n",
		reg, lcd_vcbus_read(reg));
	n = lcd_debug_info_len(len + offset);
	reg = VBO_INFILTER_CTRL_T7 + reg_offset;
	len += snprintf((buf + len), n,
		"VBO_INFILTER_CTRL            [0x%04x] = 0x%08x\n",
		reg, lcd_vcbus_read(reg));
	n = lcd_debug_info_len(len + offset);
	reg = VBO_INSGN_CTRL_T7 + reg_offset;
	len += snprintf((buf + len), n,
		"VBO_INSGN_CTRL               [0x%04x] = 0x%08x\n",
		reg, lcd_vcbus_read(reg));

	n = lcd_debug_info_len(len + offset);
	reg = VBO_FSM_HOLDER_L_T7 + reg_offset;
	len += snprintf((buf + len), n,
		"VX1_FSM_HOLDER_L             [0x%04x] = 0x%08x\n",
		reg, lcd_vcbus_read(reg));
	n = lcd_debug_info_len(len + offset);
	reg = VBO_FSM_HOLDER_H_T7 + reg_offset;
	len += snprintf((buf + len), n,
		"VX1_FSM_HOLDER_H             [0x%04x] = 0x%08x\n",
		reg, lcd_vcbus_read(reg));
	n = lcd_debug_info_len(len + offset);
	reg = VBO_INTR_STATE_CTRL_T7 + reg_offset;
	len += snprintf((buf + len), n,
		"VX1_INTR_STATE_CTRL          [0x%04x] = 0x%08x\n",
		reg, lcd_vcbus_read(reg));
	n = lcd_debug_info_len(len + offset);
	reg = VBO_INTR_UNMASK_T7 + reg_offset;
	len += snprintf((buf + len), n,
		"VX1_INTR_UNMASK              [0x%04x] = 0x%08x\n",
		reg, lcd_vcbus_read(reg));
	n = lcd_debug_info_len(len + offset);
	reg = VBO_INTR_STATE_T7 + reg_offset;
	len += snprintf((buf + len), n,
		"VX1_INTR_STATE               [0x%04x] = 0x%08x\n",
		reg, lcd_vcbus_read(reg));

	return len;
}

static int lcd_reg_print_mipi(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	unsigned int reg;
	int n, len = 0;

	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n, "\nmipi_dsi regs:\n");
	n = lcd_debug_info_len(len + offset);
	reg = MIPI_DSI_TOP_CNTL;
	len += snprintf((buf + len), n,
		"MIPI_DSI_TOP_CNTL               [0x%04x] = 0x%08x\n",
		reg, dsi_host_read(pdrv, reg));
	n = lcd_debug_info_len(len + offset);
	reg = MIPI_DSI_TOP_CLK_CNTL;
	len += snprintf((buf + len), n,
		"MIPI_DSI_TOP_CLK_CNTL           [0x%04x] = 0x%08x\n",
		reg, dsi_host_read(pdrv, reg));
	n = lcd_debug_info_len(len + offset);
	reg = MIPI_DSI_DWC_PWR_UP_OS;
	len += snprintf((buf + len), n,
		"MIPI_DSI_DWC_PWR_UP_OS          [0x%04x] = 0x%08x\n",
		reg, dsi_host_read(pdrv, reg));
	n = lcd_debug_info_len(len + offset);
	reg = MIPI_DSI_DWC_PCKHDL_CFG_OS;
	len += snprintf((buf + len), n,
		"MIPI_DSI_DWC_PCKHDL_CFG_OS      [0x%04x] = 0x%08x\n",
		reg, dsi_host_read(pdrv, reg));
	n = lcd_debug_info_len(len + offset);
	reg = MIPI_DSI_DWC_LPCLK_CTRL_OS;
	len += snprintf((buf + len), n,
		"MIPI_DSI_DWC_LPCLK_CTRL_OS      [0x%04x] = 0x%08x\n",
		reg, dsi_host_read(pdrv, reg));
	n = lcd_debug_info_len(len + offset);
	reg = MIPI_DSI_DWC_CMD_MODE_CFG_OS;
	len += snprintf((buf + len), n,
		"MIPI_DSI_DWC_CMD_MODE_CFG_OS    [0x%04x] = 0x%08x\n",
		reg, dsi_host_read(pdrv, reg));
	n = lcd_debug_info_len(len + offset);
	reg = MIPI_DSI_DWC_VID_MODE_CFG_OS;
	len += snprintf((buf + len), n,
		"MIPI_DSI_DWC_VID_MODE_CFG_OS    [0x%04x] = 0x%08x\n",
		reg, dsi_host_read(pdrv, reg));
	n = lcd_debug_info_len(len + offset);
	reg = MIPI_DSI_DWC_MODE_CFG_OS;
	len += snprintf((buf + len), n,
		"MIPI_DSI_DWC_MODE_CFG_OS        [0x%04x] = 0x%08x\n",
		reg, dsi_host_read(pdrv, reg));
	n = lcd_debug_info_len(len + offset);
	reg = MIPI_DSI_DWC_PHY_STATUS_OS;
	len += snprintf((buf + len), n,
		"MIPI_DSI_DWC_PHY_STATUS_OS      [0x%04x] = 0x%08x\n",
		reg, dsi_host_read(pdrv, reg));
	n = lcd_debug_info_len(len + offset);
	reg = MIPI_DSI_DWC_INT_ST0_OS;
	len += snprintf((buf + len), n,
		"MIPI_DSI_DWC_INT_ST0_OS         [0x%04x] = 0x%08x\n",
		reg, dsi_host_read(pdrv, reg));
	n = lcd_debug_info_len(len + offset);
	reg = MIPI_DSI_DWC_INT_ST1_OS;
	len += snprintf((buf + len), n,
		"MIPI_DSI_DWC_INT_ST1_OS         [0x%04x] = 0x%08x\n",
		reg, dsi_host_read(pdrv, reg));
	n = lcd_debug_info_len(len + offset);
	reg = MIPI_DSI_TOP_STAT;
	len += snprintf((buf + len), n,
		"MIPI_DSI_TOP_STAT               [0x%04x] = 0x%08x\n",
		reg, dsi_host_read(pdrv, reg));
	n = lcd_debug_info_len(len + offset);
	reg = MIPI_DSI_TOP_INTR_CNTL_STAT;
	len += snprintf((buf + len), n,
		"MIPI_DSI_TOP_INTR_CNTL_STAT     [0x%04x] = 0x%08x\n",
		reg, dsi_host_read(pdrv, reg));
	n = lcd_debug_info_len(len + offset);
	reg = MIPI_DSI_TOP_MEM_PD;
	len += snprintf((buf + len), n,
		"MIPI_DSI_TOP_MEM_PD             [0x%04x] = 0x%08x\n",
		reg, dsi_host_read(pdrv, reg));

	return len;
}

static int lcd_reg_print_edp(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	unsigned int reg;
	int n, len = 0;

	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n, "\nedp regs:\n");
	n = lcd_debug_info_len(len + offset);
	reg = EDP_TX_LINK_BW_SET;
	len += snprintf((buf + len), n,
		"EDP_TX_LINK_BW_SET                  [0x%04x] = 0x%08x\n",
		reg, dptx_reg_read(pdrv, reg));
	n = lcd_debug_info_len(len + offset);
	reg = EDP_TX_LINK_COUNT_SET;
	len += snprintf((buf + len), n,
		"EDP_TX_LINK_COUNT_SET               [0x%04x] = 0x%08x\n",
		reg, dptx_reg_read(pdrv, reg));
	n = lcd_debug_info_len(len + offset);
	reg = EDP_TX_TRAINING_PATTERN_SET;
	len += snprintf((buf + len), n,
		"EDP_TX_TRAINING_PATTERN_SET         [0x%04x] = 0x%08x\n",
		reg, dptx_reg_read(pdrv, reg));
	n = lcd_debug_info_len(len + offset);
	reg = EDP_TX_SCRAMBLING_DISABLE;
	len += snprintf((buf + len), n,
		"EDP_TX_SCRAMBLING_DISABLE           [0x%04x] = 0x%08x\n",
		reg, dptx_reg_read(pdrv, reg));
	n = lcd_debug_info_len(len + offset);
	reg = EDP_TX_TRANSMITTER_OUTPUT_ENABLE;
	len += snprintf((buf + len), n,
		"EDP_TX_TRANSMITTER_OUTPUT_ENABLE    [0x%04x] = 0x%08x\n",
		reg, dptx_reg_read(pdrv, reg));
	n = lcd_debug_info_len(len + offset);
	reg = EDP_TX_MAIN_STREAM_ENABLE;
	len += snprintf((buf + len), n,
		"EDP_TX_MAIN_STREAM_ENABLE           [0x%04x] = 0x%08x\n",
		reg, dptx_reg_read(pdrv, reg));
	n = lcd_debug_info_len(len + offset);
	reg = EDP_TX_PHY_RESET;
	len += snprintf((buf + len), n,
		"EDP_TX_PHY_RESET                    [0x%04x] = 0x%08x\n",
		reg, dptx_reg_read(pdrv, reg));
	n = lcd_debug_info_len(len + offset);
	reg = EDP_TX_PHY_STATUS;
	len += snprintf((buf + len), n,
		"EDP_TX_PHY_STATUS                   [0x%04x] = 0x%08x\n",
		reg, dptx_reg_read(pdrv, reg));
	n = lcd_debug_info_len(len + offset);
	reg = EDP_TX_AUX_COMMAND;
	len += snprintf((buf + len), n,
		"EDP_TX_AUX_COMMAND                  [0x%04x] = 0x%08x\n",
		reg, dptx_reg_read(pdrv, reg));
	n = lcd_debug_info_len(len + offset);
	reg = EDP_TX_AUX_ADDRESS;
	len += snprintf((buf + len), n,
		"EDP_TX_AUX_ADDRESS                   [0x%04x] = 0x%08x\n",
		reg, dptx_reg_read(pdrv, reg));
	n = lcd_debug_info_len(len + offset);
	reg = EDP_TX_AUX_STATE;
	len += snprintf((buf + len), n,
		"EDP_TX_AUX_STATE                    [0x%04x] = 0x%08x\n",
		reg, dptx_reg_read(pdrv, reg));
	n = lcd_debug_info_len(len + offset);
	reg = EDP_TX_AUX_REPLY_CODE;
	len += snprintf((buf + len), n,
		"EDP_TX_AUX_REPLY_CODE               [0x%04x] = 0x%08x\n",
		reg, dptx_reg_read(pdrv, reg));
	n = lcd_debug_info_len(len + offset);
	reg = EDP_TX_AUX_REPLY_COUNT;
	len += snprintf((buf + len), n,
		"EDP_TX_AUX_REPLY_COUNT              [0x%04x] = 0x%08x\n",
		reg, dptx_reg_read(pdrv, reg));
	n = lcd_debug_info_len(len + offset);
	reg = EDP_TX_AUX_REPLY_DATA_COUNT;
	len += snprintf((buf + len), n,
		"EDP_TX_AUX_REPLY_DATA_COUNT         [0x%04x] = 0x%08x\n",
		reg, dptx_reg_read(pdrv, reg));
	n = lcd_debug_info_len(len + offset);
	reg = EDP_TX_AUX_TRANSFER_STATUS;
	len += snprintf((buf + len), n,
		"EDP_TX_AUX_TRANSFER_STATUS          [0x%04x] = 0x%08x\n",
		reg, dptx_reg_read(pdrv, reg));

	return len;
}

static int lcd_reg_print_tcon_core(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	unsigned int reg;
	int n, len = 0;

	n = lcd_debug_info_len(len + offset);
	reg = TCON_TOP_CTRL;
	len += snprintf((buf + len), n,
		"TCON_TOP_CTRL       [0x%04x] = 0x%08x\n",
		reg, lcd_tcon_read(pdrv, reg));
	n = lcd_debug_info_len(len + offset);
	reg = TCON_RGB_IN_MUX;
	len += snprintf((buf + len), n,
		"TCON_RGB_IN_MUX     [0x%04x] = 0x%08x\n",
		reg, lcd_tcon_read(pdrv, reg));
	n = lcd_debug_info_len(len + offset);
	reg = TCON_OUT_CH_SEL0;
	len += snprintf((buf + len), n,
		"TCON_OUT_CH_SEL0    [0x%04x] = 0x%08x\n",
		reg, lcd_tcon_read(pdrv, reg));
	n = lcd_debug_info_len(len + offset);
	reg = TCON_OUT_CH_SEL1;
	len += snprintf((buf + len), n,
		"TCON_OUT_CH_SEL1    [0x%04x] = 0x%08x\n",
		reg, lcd_tcon_read(pdrv, reg));
	n = lcd_debug_info_len(len + offset);
	reg = TCON_STATUS0;
	len += snprintf((buf + len), n,
		"TCON_STATUS0        [0x%04x] = 0x%08x\n",
		reg, lcd_tcon_read(pdrv, reg));
	n = lcd_debug_info_len(len + offset);
	reg = TCON_PLLLOCK_CNTL;
	len += snprintf((buf + len), n,
		"TCON_PLLLOCK_CNTL   [0x%04x] = 0x%08x\n",
		reg, lcd_tcon_read(pdrv, reg));
	n = lcd_debug_info_len(len + offset);
	reg = TCON_RST_CTRL;
	len += snprintf((buf + len), n,
		"TCON_RST_CTRL       [0x%04x] = 0x%08x\n",
		reg, lcd_tcon_read(pdrv, reg));
	n = lcd_debug_info_len(len + offset);
	reg = TCON_AXI_OFST0;
	len += snprintf((buf + len), n,
		"TCON_AXI_OFST0      [0x%04x] = 0x%08x\n",
		reg, lcd_tcon_read(pdrv, reg));
	n = lcd_debug_info_len(len + offset);
	reg = TCON_AXI_OFST1;
	len += snprintf((buf + len), n,
		"TCON_AXI_OFST1      [0x%04x] = 0x%08x\n",
		reg, lcd_tcon_read(pdrv, reg));
	n = lcd_debug_info_len(len + offset);
	reg = TCON_AXI_OFST2;
	len += snprintf((buf + len), n,
		"TCON_AXI_OFST2      [0x%04x] = 0x%08x\n",
		reg, lcd_tcon_read(pdrv, reg));
	n = lcd_debug_info_len(len + offset);
	reg = TCON_CLK_CTRL;
	len += snprintf((buf + len), n,
		"TCON_CLK_CTRL       [0x%04x] = 0x%08x\n",
		reg, lcd_tcon_read(pdrv, reg));
	n = lcd_debug_info_len(len + offset);
	reg = TCON_STATUS1;
	len += snprintf((buf + len), n,
		"TCON_STATUS1        [0x%04x] = 0x%08x\n",
		reg, lcd_tcon_read(pdrv, reg));
	n = lcd_debug_info_len(len + offset);
	reg = TCON_DDRIF_CTRL1;
	len += snprintf((buf + len), n,
		"TCON_DDRIF_CTRL1    [0x%04x] = 0x%08x\n",
		reg, lcd_tcon_read(pdrv, reg));
	reg = TCON_DDRIF_CTRL2;
	len += snprintf((buf + len), n,
		"TCON_DDRIF_CTRL2    [0x%04x] = 0x%08x\n",
		reg, lcd_tcon_read(pdrv, reg));
	n = lcd_debug_info_len(len + offset);
	reg = TCON_INTR_MASKN;
	len += snprintf((buf + len), n,
		"TCON_INTR_MASKN     [0x%04x] = 0x%08x\n",
		reg, lcd_tcon_read(pdrv, reg));
	reg = TCON_INTR_RO;
	len += snprintf((buf + len), n,
		"TCON_INTR_RO        [0x%04x] = 0x%08x\n",
		reg, lcd_tcon_read(pdrv, reg));

	return len;
}

static int lcd_reg_print_tcon(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	unsigned int reg;
	int n, len = 0;

	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n, "\ntcon regs:\n");

	n = lcd_debug_info_len(len + offset);
	reg = HHI_TCON_CLK_CNTL;
	len += snprintf((buf + len), n,
		"HHI_TCON_CLK_CNTL   [0x%04x] = 0x%08x\n",
		reg, lcd_clk_read(reg));

	len += lcd_reg_print_tcon_core(pdrv, (buf + len), (len + offset));

	n = lcd_debug_info_len(len + offset);
	reg = P2P_CH_SWAP0;
	len += snprintf((buf + len), n,
		"P2P_CH_SWAP0    [0x%04x] = 0x%08x\n",
		reg, lcd_vcbus_read(reg));
	n = lcd_debug_info_len(len + offset);
	reg = P2P_CH_SWAP1;
	len += snprintf((buf + len), n,
		"P2P_CH_SWAP1    [0x%04x] = 0x%08x\n",
		reg, lcd_vcbus_read(reg));

	return len;
}

static int lcd_reg_print_tcon_t3(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	unsigned int reg;
	int n, len = 0;

	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n, "\ntcon regs:\n");

	n = lcd_debug_info_len(len + offset);
	reg = CLKCTRL_TCON_CLK_CNTL;
	len += snprintf((buf + len), n,
		"CLKCTRL_TCON_CLK_CNTL   [0x%04x] = 0x%08x\n",
		reg, lcd_clk_read(reg));

	len += lcd_reg_print_tcon_core(pdrv, (buf + len), (len + offset));

	n = lcd_debug_info_len(len + offset);
	reg = P2P_CH_SWAP0_T7;
	len += snprintf((buf + len), n,
		"P2P_CH_SWAP0        [0x%04x] = 0x%08x\n",
		reg, lcd_vcbus_read(reg));
	n = lcd_debug_info_len(len + offset);
	reg = P2P_CH_SWAP1_T7;
	len += snprintf((buf + len), n,
		"P2P_CH_SWAP1        [0x%04x] = 0x%08x\n",
		reg, lcd_vcbus_read(reg));

	return len;
}

static int lcd_reg_print_tcon_t5w(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	unsigned int reg;
	int n, len = 0;

	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n, "\ntcon regs:\n");

	n = lcd_debug_info_len(len + offset);
	reg = HHI_TCON_CLK_CNTL;
	len += snprintf((buf + len), n,
		"HHI_TCON_CLK_CNTL   [0x%04x] = 0x%08x\n",
		reg, lcd_clk_read(reg));

	len += lcd_reg_print_tcon_core(pdrv, (buf + len), (len + offset));

	n = lcd_debug_info_len(len + offset);
	reg = P2P_CH_SWAP0_T7;
	len += snprintf((buf + len), n,
		"P2P_CH_SWAP0    [0x%04x] = 0x%08x\n",
		reg, lcd_vcbus_read(reg));
	n = lcd_debug_info_len(len + offset);
	reg = P2P_CH_SWAP1_T7;
	len += snprintf((buf + len), n,
		"P2P_CH_SWAP1    [0x%04x] = 0x%08x\n",
		reg, lcd_vcbus_read(reg));

	return len;
}

static int lcd_reg_print_dphy(char *buf, int offset)
{
	unsigned int reg0, reg1;
	int n, len = 0;

	reg0 = HHI_LVDS_TX_PHY_CNTL0;
	reg1 = HHI_LVDS_TX_PHY_CNTL1;

	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n,
		"HHI_LVDS_TX_PHY_CNTL0  [0x%02x] = 0x%08x\n",
		reg0, lcd_ana_read(reg0));
	len += snprintf((buf + len), n,
		"HHI_LVDS_TX_PHY_CNTL1  [0x%02x] = 0x%08x\n",
		reg1, lcd_ana_read(reg1));

	return len;
}

static int lcd_reg_print_phy_analog(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	unsigned int reg;
	int n, len = 0;

	len += lcd_reg_print_dphy((buf + len), (len + offset));

	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n, "\nphy analog regs:\n");

	n = lcd_debug_info_len(len + offset);
	reg = HHI_DIF_CSI_PHY_CNTL1;
	len += snprintf((buf + len), n,
		"HHI_DIF_CSI_PHY_CNTL1  [0x%02x] = 0x%08x\n",
		reg, lcd_ana_read(reg));
	n = lcd_debug_info_len(len + offset);
	reg = HHI_DIF_CSI_PHY_CNTL2;
	len += snprintf((buf + len), n,
		"HHI_DIF_CSI_PHY_CNTL2  [0x%02x] = 0x%08x\n",
		reg, lcd_ana_read(reg));
	n = lcd_debug_info_len(len + offset);
	reg = HHI_DIF_CSI_PHY_CNTL3;
	len += snprintf((buf + len), n,
		"HHI_DIF_CSI_PHY_CNTL3  [0x%02x] = 0x%08x\n",
		reg, lcd_ana_read(reg));
	n = lcd_debug_info_len(len + offset);
	reg = HHI_DIF_CSI_PHY_CNTL4;
	len += snprintf((buf + len), n,
		"HHI_DIF_CSI_PHY_CNTL4  [0x%02x] = 0x%08x\n",
		reg, lcd_ana_read(reg));
	n = lcd_debug_info_len(len + offset);
	reg = HHI_DIF_CSI_PHY_CNTL6;
	len += snprintf((buf + len), n,
		"HHI_DIF_CSI_PHY_CNTL6  [0x%02x] = 0x%08x\n",
		reg, lcd_ana_read(reg));
	n = lcd_debug_info_len(len + offset);
	reg = HHI_DIF_CSI_PHY_CNTL7;
	len += snprintf((buf + len), n,
		"HHI_DIF_CSI_PHY_CNTL7  [0x%02x] = 0x%08x\n",
		reg, lcd_ana_read(reg));
	n = lcd_debug_info_len(len + offset);
	reg = HHI_DIF_CSI_PHY_CNTL8;
	len += snprintf((buf + len), n,
		"HHI_DIF_CSI_PHY_CNTL8  [0x%02x] = 0x%08x\n",
		reg, lcd_ana_read(reg));
	n = lcd_debug_info_len(len + offset);
	reg = HHI_DIF_CSI_PHY_CNTL9;
	len += snprintf((buf + len), n,
		"HHI_DIF_CSI_PHY_CNTL9  [0x%02x] = 0x%08x\n",
		reg, lcd_ana_read(reg));
	n = lcd_debug_info_len(len + offset);
	reg = HHI_DIF_CSI_PHY_CNTL10;
	len += snprintf((buf + len), n,
		"HHI_DIF_CSI_PHY_CNTL10 [0x%02x] = 0x%08x\n",
		reg, lcd_ana_read(reg));
	n = lcd_debug_info_len(len + offset);
	reg = HHI_DIF_CSI_PHY_CNTL11;
	len += snprintf((buf + len), n,
		"HHI_DIF_CSI_PHY_CNTL11 [0x%02x] = 0x%08x\n",
		reg, lcd_ana_read(reg));
	n = lcd_debug_info_len(len + offset);
	reg = HHI_DIF_CSI_PHY_CNTL12;
	len += snprintf((buf + len), n,
		"HHI_DIF_CSI_PHY_CNTL12 [0x%02x] = 0x%08x\n",
		reg, lcd_ana_read(reg));
	n = lcd_debug_info_len(len + offset);
	reg = HHI_DIF_CSI_PHY_CNTL13;
	len += snprintf((buf + len), n,
		"HHI_DIF_CSI_PHY_CNTL13 [0x%02x] = 0x%08x\n",
		reg, lcd_ana_read(reg));
	n = lcd_debug_info_len(len + offset);
	reg = HHI_DIF_CSI_PHY_CNTL14;
	len += snprintf((buf + len), n,
		"HHI_DIF_CSI_PHY_CNTL14 [0x%02x] = 0x%08x\n",
		reg, lcd_ana_read(reg));
	n = lcd_debug_info_len(len + offset);
	reg = HHI_DIF_CSI_PHY_CNTL15;
	len += snprintf((buf + len), n,
		"HHI_DIF_CSI_PHY_CNTL15 [0x%02x] = 0x%08x\n",
		reg, lcd_ana_read(reg));
	n = lcd_debug_info_len(len + offset);
	reg = HHI_DIF_CSI_PHY_CNTL16;
	len += snprintf((buf + len), n,
		"HHI_DIF_CSI_PHY_CNTL16 [0x%02x] = 0x%08x\n",
		reg, lcd_ana_read(reg));

	return len;
}

static int lcd_reg_print_dphy_t7(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	unsigned int reg0, reg1;
	int n, len = 0;

	switch (pdrv->index) {
	case 1:
		reg0 = COMBO_DPHY_EDP_LVDS_TX_PHY1_CNTL0;
		reg1 = COMBO_DPHY_EDP_LVDS_TX_PHY1_CNTL1;
		break;
	case 2:
		reg0 = COMBO_DPHY_EDP_LVDS_TX_PHY2_CNTL0;
		reg1 = COMBO_DPHY_EDP_LVDS_TX_PHY2_CNTL1;
		break;
	case 0:
	default:
		reg0 = COMBO_DPHY_EDP_LVDS_TX_PHY0_CNTL0;
		reg1 = COMBO_DPHY_EDP_LVDS_TX_PHY0_CNTL1;
		break;
	}

	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n,
		"\nCOMBO_DPHY_CNTL0    [0x%04x] = 0x%08x\n",
		COMBO_DPHY_CNTL0, lcd_combo_dphy_read(pdrv, COMBO_DPHY_CNTL0));
	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n,
		"COMBO_DPHY_CNTL1    [0x%04x] = 0x%08x\n",
		COMBO_DPHY_CNTL1, lcd_combo_dphy_read(pdrv, COMBO_DPHY_CNTL1));
	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n,
		"COMBO_DPHY_EDP_LVDS_TX_PHY_CNTL0    [0x%04x] = 0x%08x\n",
		reg0, lcd_combo_dphy_read(pdrv, reg0));
	len += snprintf((buf + len), n,
		"COMBO_DPHY_EDP_LVDS_TX_PHY_CNTL1    [0x%04x] = 0x%08x\n",
		reg1, lcd_combo_dphy_read(pdrv, reg1));

	return len;
}

static int lcd_reg_print_phy_analog_t7(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	unsigned int reg;
	int n, len = 0;

	len += lcd_reg_print_dphy_t7(pdrv, (buf + len), (len + offset));

	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n, "\nphy analog regs:\n");

	reg = ANACTRL_DIF_PHY_CNTL1;
	len += snprintf((buf + len), n,
		"PHY_CNTL1  [0x%02x] = 0x%08x\n",
		reg, lcd_ana_read(reg));
	n = lcd_debug_info_len(len + offset);
	reg = ANACTRL_DIF_PHY_CNTL2;
	len += snprintf((buf + len), n,
		"PHY_CNTL2  [0x%02x] = 0x%08x\n",
		reg, lcd_ana_read(reg));
	n = lcd_debug_info_len(len + offset);
	reg = ANACTRL_DIF_PHY_CNTL3;
	len += snprintf((buf + len), n,
		"PHY_CNTL3  [0x%02x] = 0x%08x\n",
		reg, lcd_ana_read(reg));
	n = lcd_debug_info_len(len + offset);
	reg = ANACTRL_DIF_PHY_CNTL4;
	len += snprintf((buf + len), n,
		"PHY_CNTL4  [0x%02x] = 0x%08x\n",
		reg, lcd_ana_read(reg));
	n = lcd_debug_info_len(len + offset);
	reg = ANACTRL_DIF_PHY_CNTL5;
	len += snprintf((buf + len), n,
		"PHY_CNTL5  [0x%02x] = 0x%08x\n",
		reg, lcd_ana_read(reg));
	n = lcd_debug_info_len(len + offset);
	reg = ANACTRL_DIF_PHY_CNTL6;
	len += snprintf((buf + len), n,
		"PHY_CNTL6  [0x%02x] = 0x%08x\n",
		reg, lcd_ana_read(reg));
	n = lcd_debug_info_len(len + offset);
	reg = ANACTRL_DIF_PHY_CNTL7;
	len += snprintf((buf + len), n,
		"PHY_CNTL7  [0x%02x] = 0x%08x\n",
		reg, lcd_ana_read(reg));
	n = lcd_debug_info_len(len + offset);
	reg = ANACTRL_DIF_PHY_CNTL8;
	len += snprintf((buf + len), n,
		"PHY_CNTL8  [0x%02x] = 0x%08x\n",
		reg, lcd_ana_read(reg));
	n = lcd_debug_info_len(len + offset);
	reg = ANACTRL_DIF_PHY_CNTL9;
	len += snprintf((buf + len), n,
		"PHY_CNTL9  [0x%02x] = 0x%08x\n",
		reg, lcd_ana_read(reg));
	n = lcd_debug_info_len(len + offset);
	reg = ANACTRL_DIF_PHY_CNTL10;
	len += snprintf((buf + len), n,
		"PHY_CNTL10 [0x%02x] = 0x%08x\n",
		reg, lcd_ana_read(reg));
	n = lcd_debug_info_len(len + offset);
	reg = ANACTRL_DIF_PHY_CNTL11;
	len += snprintf((buf + len), n,
		"PHY_CNTL11 [0x%02x] = 0x%08x\n",
		reg, lcd_ana_read(reg));
	n = lcd_debug_info_len(len + offset);
	reg = ANACTRL_DIF_PHY_CNTL12;
	len += snprintf((buf + len), n,
		"PHY_CNTL12 [0x%02x] = 0x%08x\n",
		reg, lcd_ana_read(reg));
	n = lcd_debug_info_len(len + offset);
	reg = ANACTRL_DIF_PHY_CNTL13;
	len += snprintf((buf + len), n,
		"PHY_CNTL13 [0x%02x] = 0x%08x\n",
		reg, lcd_ana_read(reg));
	n = lcd_debug_info_len(len + offset);
	reg = ANACTRL_DIF_PHY_CNTL14;
	len += snprintf((buf + len), n,
		"PHY_CNTL14 [0x%02x] = 0x%08x\n",
		reg, lcd_ana_read(reg));
	n = lcd_debug_info_len(len + offset);
	reg = ANACTRL_DIF_PHY_CNTL15;
	len += snprintf((buf + len), n,
		"PHY_CNTL15 [0x%02x] = 0x%08x\n",
		reg, lcd_ana_read(reg));
	n = lcd_debug_info_len(len + offset);
	reg = ANACTRL_DIF_PHY_CNTL16;
	len += snprintf((buf + len), n,
		"PHY_CNTL16 [0x%02x] = 0x%08x\n",
		reg, lcd_ana_read(reg));
	n = lcd_debug_info_len(len + offset);
	reg = ANACTRL_DIF_PHY_CNTL17;
	len += snprintf((buf + len), n,
		"PHY_CNTL17 [0x%02x] = 0x%08x\n",
		reg, lcd_ana_read(reg));
	n = lcd_debug_info_len(len + offset);
	reg = ANACTRL_DIF_PHY_CNTL18;
	len += snprintf((buf + len), n,
		"PHY_CNTL18 [0x%02x] = 0x%08x\n",
		reg, lcd_ana_read(reg));
	n = lcd_debug_info_len(len + offset);
	reg = ANACTRL_DIF_PHY_CNTL19;
	len += snprintf((buf + len), n,
		"PHY_CNTL19 [0x%02x] = 0x%08x\n",
		reg, lcd_ana_read(reg));
	n = lcd_debug_info_len(len + offset);
	reg = ANACTRL_DIF_PHY_CNTL20;
	len += snprintf((buf + len), n,
		"PHY_CNTL20 [0x%02x] = 0x%08x\n",
		reg, lcd_ana_read(reg));
	n = lcd_debug_info_len(len + offset);
	reg = ANACTRL_DIF_PHY_CNTL21;
	len += snprintf((buf + len), n,
		"PHY_CNTL21 [0x%02x] = 0x%08x\n",
		reg, lcd_ana_read(reg));

	return len;
}

static int lcd_reg_print_dphy_t3(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	unsigned int reg0, reg1;
	int n, len = 0;

	switch (pdrv->index) {
	case 1:
		reg0 = ANACTRL_LVDS_TX_PHY_CNTL2;
		reg1 = ANACTRL_LVDS_TX_PHY_CNTL3;
		break;
	case 0:
	default:
		reg0 = ANACTRL_LVDS_TX_PHY_CNTL0;
		reg1 = ANACTRL_LVDS_TX_PHY_CNTL1;
		break;
	}

	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n,
		"ANACTRL_LVDS_TX_PHY_CNTL0    [0x%04x] = 0x%08x\n",
		reg0, lcd_ana_read(reg0));
	len += snprintf((buf + len), n,
		"ANACTRL_LVDS_TX_PHY_CNTL1    [0x%04x] = 0x%08x\n",
		reg1, lcd_ana_read(reg1));

	return len;
}

static int lcd_reg_print_phy_analog_t3(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	unsigned int reg;
	int n, len = 0;

	len += lcd_reg_print_dphy_t3(pdrv, (buf + len), (len + offset));

	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n, "\nphy analog regs:\n");

	n = lcd_debug_info_len(len + offset);
	reg = ANACTRL_DIF_PHY_CNTL1;
	len += snprintf((buf + len), n,
		"ANACTRL_DIF_PHY_CNTL1  [0x%02x] = 0x%08x\n",
		reg, lcd_ana_read(reg));
	n = lcd_debug_info_len(len + offset);
	reg = ANACTRL_DIF_PHY_CNTL2;
	len += snprintf((buf + len), n,
		"ANACTRL_DIF_PHY_CNTL2  [0x%02x] = 0x%08x\n",
		reg, lcd_ana_read(reg));
	n = lcd_debug_info_len(len + offset);
	reg = ANACTRL_DIF_PHY_CNTL3;
	len += snprintf((buf + len), n,
		"ANACTRL_DIF_PHY_CNTL3  [0x%02x] = 0x%08x\n",
		reg, lcd_ana_read(reg));
	n = lcd_debug_info_len(len + offset);
	reg = ANACTRL_DIF_PHY_CNTL4;
	len += snprintf((buf + len), n,
		"ANACTRL_DIF_PHY_CNTL4  [0x%02x] = 0x%08x\n",
		reg, lcd_ana_read(reg));
	n = lcd_debug_info_len(len + offset);
	reg = ANACTRL_DIF_PHY_CNTL6;
	len += snprintf((buf + len), n,
		"ANACTRL_DIF_PHY_CNTL6  [0x%02x] = 0x%08x\n",
		reg, lcd_ana_read(reg));
	n = lcd_debug_info_len(len + offset);
	reg = ANACTRL_DIF_PHY_CNTL7;
	len += snprintf((buf + len), n,
		"ANACTRL_DIF_PHY_CNTL7  [0x%02x] = 0x%08x\n",
		reg, lcd_ana_read(reg));
	n = lcd_debug_info_len(len + offset);
	reg = ANACTRL_DIF_PHY_CNTL8;
	len += snprintf((buf + len), n,
		"ANACTRL_DIF_PHY_CNTL8  [0x%02x] = 0x%08x\n",
		reg, lcd_ana_read(reg));
	n = lcd_debug_info_len(len + offset);
	reg = ANACTRL_DIF_PHY_CNTL9;
	len += snprintf((buf + len), n,
		"ANACTRL_DIF_PHY_CNTL9  [0x%02x] = 0x%08x\n",
		reg, lcd_ana_read(reg));
	n = lcd_debug_info_len(len + offset);
	reg = ANACTRL_DIF_PHY_CNTL10;
	len += snprintf((buf + len), n,
		"ANACTRL_DIF_PHY_CNTL10 [0x%02x] = 0x%08x\n",
		reg, lcd_ana_read(reg));
	n = lcd_debug_info_len(len + offset);
	reg = ANACTRL_DIF_PHY_CNTL11;
	len += snprintf((buf + len), n,
		"ANACTRL_DIF_PHY_CNTL11 [0x%02x] = 0x%08x\n",
		reg, lcd_ana_read(reg));
	n = lcd_debug_info_len(len + offset);
	reg = ANACTRL_DIF_PHY_CNTL12;
	len += snprintf((buf + len), n,
		"ANACTRL_DIF_PHY_CNTL12 [0x%02x] = 0x%08x\n",
		reg, lcd_ana_read(reg));
	n = lcd_debug_info_len(len + offset);
	reg = ANACTRL_DIF_PHY_CNTL13;
	len += snprintf((buf + len), n,
		"ANACTRL_DIF_PHY_CNTL13 [0x%02x] = 0x%08x\n",
		reg, lcd_ana_read(reg));
	n = lcd_debug_info_len(len + offset);
	reg = ANACTRL_DIF_PHY_CNTL14;
	len += snprintf((buf + len), n,
		"ANACTRL_DIF_PHY_CNTL14 [0x%02x] = 0x%08x\n",
		reg, lcd_ana_read(reg));
	n = lcd_debug_info_len(len + offset);
	reg = ANACTRL_DIF_PHY_CNTL15;
	len += snprintf((buf + len), n,
		"ANACTRL_DIF_PHY_CNTL15 [0x%02x] = 0x%08x\n",
		reg, lcd_ana_read(reg));
	n = lcd_debug_info_len(len + offset);
	reg = ANACTRL_DIF_PHY_CNTL16;
	len += snprintf((buf + len), n,
		"ANACTRL_DIF_PHY_CNTL16 [0x%02x] = 0x%08x\n",
		reg, lcd_ana_read(reg));

	return len;
}

static int lcd_reg_print_mipi_phy_analog_axg(struct aml_lcd_drv_s *pdrv,
					 char *buf, int offset)
{
	unsigned int reg;
	int n, len = 0;

	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n, "\nmipi_dsi_phy analog regs:\n");
	n = lcd_debug_info_len(len + offset);
	reg = HHI_MIPI_CNTL0;
	len += snprintf((buf + len), n,
		"HHI_MIPI_CNTL0   [0x%02x] = 0x%08x\n",
		reg, lcd_hiu_read(reg));
	n = lcd_debug_info_len(len + offset);
	reg = HHI_MIPI_CNTL1;
	len += snprintf((buf + len), n,
		"HHI_MIPI_CNTL1   [0x%02x] = 0x%08x\n",
		reg, lcd_hiu_read(reg));
	n = lcd_debug_info_len(len + offset);
	reg = HHI_MIPI_CNTL2;
	len += snprintf((buf + len), n,
		"HHI_MIPI_CNTL2   [0x%02x] = 0x%08x\n",
		reg, lcd_hiu_read(reg));

	return len;
}

static int lcd_reg_print_mipi_phy_analog(struct aml_lcd_drv_s *pdrv,
					 char *buf, int offset)
{
	unsigned int reg;
	int n, len = 0;

	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n, "\nmipi_dsi_phy analog regs:\n");
	n = lcd_debug_info_len(len + offset);
	reg = HHI_MIPI_CNTL0;
	len += snprintf((buf + len), n,
		"HHI_MIPI_CNTL0   [0x%02x] = 0x%08x\n",
		reg, lcd_ana_read(reg));
	n = lcd_debug_info_len(len + offset);
	reg = HHI_MIPI_CNTL1;
	len += snprintf((buf + len), n,
		"HHI_MIPI_CNTL1   [0x%02x] = 0x%08x\n",
		reg, lcd_ana_read(reg));
	n = lcd_debug_info_len(len + offset);
	reg = HHI_MIPI_CNTL2;
	len += snprintf((buf + len), n,
		"HHI_MIPI_CNTL2   [0x%02x] = 0x%08x\n",
		reg, lcd_ana_read(reg));

	return len;
}

static int lcd_reg_clk_print(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	struct lcd_debug_info_s *lcd_debug_info;
	int i, n, len = 0;
	struct lcd_config_s *pconf;
	unsigned int *table;

	lcd_debug_info = (struct lcd_debug_info_s *)pdrv->debug_info;
	if (!lcd_debug_info) {
		n = lcd_debug_info_len(len + offset);
		len += snprintf((buf + len), n, "%s: debug_info is null\n", __func__);
		return len;
	}

	pconf = &pdrv->config;

	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n, "\nclk regs:\n");
	if (lcd_debug_info->reg_pll_table) {
		table = lcd_debug_info->reg_pll_table;
		i = 0;
		while (i < LCD_DEBUG_REG_CNT_MAX) {
			if (table[i] == LCD_DEBUG_REG_END)
				break;
			n = lcd_debug_info_len(len + offset);
			len += snprintf((buf + len), n,
					"ana     [0x%02x] = 0x%08x\n",
					table[i], lcd_ana_read(table[i]));
			i++;
		}
	}

	if (lcd_debug_info->reg_clk_table) {
		table = lcd_debug_info->reg_clk_table;
		i = 0;
		while (i < LCD_DEBUG_REG_CNT_MAX) {
			if (table[i] == LCD_DEBUG_REG_END)
				break;
			n = lcd_debug_info_len(len + offset);
			len += snprintf((buf + len), n,
				"clk     [0x%02x] = 0x%08x\n",
				table[i], lcd_clk_read(table[i]));
			i++;
		}
	}

	if (lcd_debug_info->reg_clk_hiu_table) {
		table = lcd_debug_info->reg_clk_hiu_table;
		i = 0;
		while (i < LCD_DEBUG_REG_CNT_MAX) {
			if (table[i] == LCD_DEBUG_REG_END)
				break;
			n = lcd_debug_info_len(len + offset);
			len += snprintf((buf + len), n,
				"hiu     [0x%02x] = 0x%08x\n",
				table[i], lcd_hiu_read(table[i]));
			i++;
		}
	}

	return len;
}

static int lcd_reg_encl_print(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	struct lcd_debug_info_s *lcd_debug_info;
	int i, n, len = 0;
	unsigned int *table;

	lcd_debug_info = (struct lcd_debug_info_s *)pdrv->debug_info;
	if (!lcd_debug_info) {
		n = lcd_debug_info_len(len + offset);
		len += snprintf((buf + len), n, "%s: debug_info is null\n", __func__);
		return len;
	}
	if (!lcd_debug_info->reg_encl_table) {
		n = lcd_debug_info_len(len + offset);
		len += snprintf((buf + len), n, "%s: reg_encl_table is null\n", __func__);
		return len;
	}

	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n, "\nencl regs:\n");
	table = lcd_debug_info->reg_encl_table;
	i = 0;
	while (i < LCD_DEBUG_REG_CNT_MAX) {
		if (table[i] == LCD_DEBUG_REG_END)
			break;
		n = lcd_debug_info_len(len + offset);
		len += snprintf((buf + len), n,
			"vcbus   [0x%04x] = 0x%08x\n",
			table[i], lcd_vcbus_read(table[i]));
		i++;
	}

	return len;
}

static int lcd_reg_if_print(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	struct lcd_debug_info_s *lcd_debug_info;
	int n, len = 0;

	lcd_debug_info = (struct lcd_debug_info_s *)pdrv->debug_info;
	if (!lcd_debug_info) {
		n = lcd_debug_info_len(len + offset);
		len += snprintf((buf + len), n, "%s: debug_info is null\n", __func__);
		return len;
	}
	if (!lcd_debug_info->debug_if || !lcd_debug_info->debug_if->reg_dump_interface)
		return len;

	len += lcd_debug_info->debug_if->reg_dump_interface(pdrv, (buf + len),
							(len + offset));

	return len;
}

static int lcd_reg_phy_print(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	struct lcd_debug_info_s *lcd_debug_info;
	int n, len = 0;

	lcd_debug_info = (struct lcd_debug_info_s *)pdrv->debug_info;
	if (!lcd_debug_info) {
		n = lcd_debug_info_len(len + offset);
		len += snprintf((buf + len), n, "%s: debug_info is null\n", __func__);
		return len;
	}
	if (!lcd_debug_info->debug_if || !lcd_debug_info->debug_if->reg_dump_phy)
		return len;

	len += lcd_debug_info->debug_if->reg_dump_phy(pdrv, (buf + len),
						(len + offset));

	return len;
}

static int lcd_reg_pinmux_print(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	struct lcd_debug_info_s *lcd_debug_info;
	int i, n, len = 0;
	unsigned int *table;

	lcd_debug_info = (struct lcd_debug_info_s *)pdrv->debug_info;
	if (!lcd_debug_info) {
		n = lcd_debug_info_len(len + offset);
		len += snprintf((buf + len), n, "%s: debug_info is null\n", __func__);
		return len;
	}
	if (!lcd_debug_info->reg_pinmux_table)
		return len;

	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n, "\npinmux regs:\n");
	table = lcd_debug_info->reg_pinmux_table;
	i = 0;
	while (i < LCD_DEBUG_REG_CNT_MAX) {
		if (table[i] == LCD_DEBUG_REG_END)
			break;
		n = lcd_debug_info_len(len + offset);
		len += snprintf((buf + len), n,
			"PERIPHS_PIN_MUX  [0x%02x] = 0x%08x\n",
			table[i], lcd_periphs_read(pdrv, table[i]));
		i++;
	}

	return len;
}

static int lcd_optical_info_print(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	struct lcd_config_s *pconf;
	int n, len = 0;

	pconf = &pdrv->config;
	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n,
		"\nlcd optical info:\n"
		"hdr_support      %d\n"
		"features         %d\n"
		"primaries_r_x    %d\n"
		"primaries_r_y    %d\n"
		"primaries_g_x    %d\n"
		"primaries_g_y    %d\n"
		"primaries_b_x    %d\n"
		"primaries_b_y    %d\n"
		"white_point_x    %d\n"
		"white_point_y    %d\n"
		"luma_max         %d\n"
		"luma_min         %d\n"
		"luma_avg         %d\n\n",
		pconf->optical.hdr_support,
		pconf->optical.features,
		pconf->optical.primaries_r_x,
		pconf->optical.primaries_r_y,
		pconf->optical.primaries_g_x,
		pconf->optical.primaries_g_y,
		pconf->optical.primaries_b_x,
		pconf->optical.primaries_b_y,
		pconf->optical.white_point_x,
		pconf->optical.white_point_y,
		pconf->optical.luma_max,
		pconf->optical.luma_min,
		pconf->optical.luma_avg);
	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n,
		"\nadv_val:\n"
		"ldim_support     %d\n"
		"luma_peak        %d\n\n",
		pconf->optical.ldim_support,
		pconf->optical.luma_peak);

	return len;
}

static void lcd_test_pattern_check(struct work_struct *work)
{
	struct aml_lcd_drv_s *pdrv;

	pdrv = container_of(work, struct aml_lcd_drv_s, test_check_work);
	aml_lcd_notifier_call_chain(LCD_EVENT_TEST_PATTERN, (void *)pdrv);
}

unsigned int lcd_prbs_flag = 0, lcd_prbs_performed = 0, lcd_prbs_err = 0;

static ssize_t lcd_debug_prbs_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf,
		       "lvds prbs performed: %d, error: %d\n"
		       "vx1 prbs performed: %d, error: %d\n"
		       "lcd prbs flag: %d\n",
		       (lcd_prbs_performed & LCD_PRBS_MODE_LVDS) ? 1 : 0,
		       (lcd_prbs_err & LCD_PRBS_MODE_LVDS) ? 1 : 0,
		       (lcd_prbs_performed & LCD_PRBS_MODE_VX1) ? 1 : 0,
		       (lcd_prbs_err & LCD_PRBS_MODE_VX1) ? 1 : 0,
		       lcd_prbs_flag);
}

static ssize_t lcd_debug_prbs_store(struct device *dev, struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	struct lcd_debug_info_s *lcd_debug_info;
	int ret = 0;
	unsigned int temp = 0;
	unsigned int prbs_mode_flag;

	lcd_debug_info = (struct lcd_debug_info_s *)pdrv->debug_info;
	if (!lcd_debug_info) {
		LCDPR("%s: debug_info is null\n", __func__);
		return count;
	}

	switch (buf[0]) {
	case 'v': /* vx1 */
		ret = sscanf(buf, "vx1 %d", &temp);
		if (ret) {
			prbs_mode_flag = LCD_PRBS_MODE_VX1;
		} else {
			LCDERR("invalid data\n");
			return -EINVAL;
		}
		break;
	case 'l': /* lvds */
		ret = sscanf(buf, "lvds %d", &temp);
		if (ret) {
			prbs_mode_flag = LCD_PRBS_MODE_LVDS;
		} else {
			LCDERR("invalid data\n");
			return -EINVAL;
		}
		break;
	default:
		prbs_mode_flag = LCD_PRBS_MODE_LVDS | LCD_PRBS_MODE_VX1;
		ret = kstrtouint(buf, 10, &temp);
		if (ret) {
			LCDERR("invalid data\n");
			return -EINVAL;
		}
		break;
	}

	if (temp) {
		if (lcd_prbs_flag) {
			LCDPR("lcd prbs check is already running\n");
			return count;
		}
		lcd_prbs_flag = 1;
		aml_lcd_prbs_test(pdrv, temp, prbs_mode_flag);
	} else {
		if (lcd_prbs_flag == 0) {
			LCDPR("lcd prbs check is already stopped\n");
			return count;
		}
		lcd_prbs_flag = 0;
	}

	return count;
}

static void lcd_debug_config_update(struct aml_lcd_drv_s *pdrv)
{

	pdrv->module_reset(pdrv);

	lcd_vinfo_update(pdrv);
}

static void lcd_debug_clk_change(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_config_s *pconf;
	unsigned int pclk, sync_duration;

	lcd_vout_notify_mode_change_pre(pdrv);

	pconf = &pdrv->config;
	pclk = pconf->timing.lcd_clk;
	sync_duration = pclk / pconf->basic.h_period;
	sync_duration = sync_duration * 100 / pconf->basic.v_period;
	pconf->timing.frame_rate = sync_duration / 100;
	pconf->timing.sync_duration_num = sync_duration;
	pconf->timing.sync_duration_den = 100;

	/* update vinfo */
	pdrv->vinfo.sync_duration_num = sync_duration;
	pdrv->vinfo.sync_duration_den = 100;
	pdrv->vinfo.std_duration = sync_duration / 100;
	pdrv->vinfo.video_clk = pclk;

	switch (pdrv->mode) {
#ifdef CONFIG_AMLOGIC_LCD_TV
	case LCD_MODE_TV:
		lcd_tv_clk_update(pdrv);
		break;
#endif
#ifdef CONFIG_AMLOGIC_LCD_TABLET
	case LCD_MODE_TABLET:
		lcd_tablet_clk_update(pdrv);
		break;
#endif
	default:
		LCDPR("invalid lcd mode\n");
		break;
	}

	lcd_vout_notify_mode_change(pdrv);
}

static void lcd_power_interface_ctrl(struct aml_lcd_drv_s *pdrv, int state)
{
	mutex_lock(&lcd_power_mutex);
	LCDPR("[%d]: %s: %d\n", pdrv->index, __func__, state);
	if (state) {
		if (pdrv->status & LCD_STATUS_ENCL_ON) {
			aml_lcd_notifier_call_chain(LCD_EVENT_IF_POWER_ON, (void *)pdrv);
			lcd_if_enable_retry(pdrv);
		} else {
			LCDERR("%s: can't power on when controller is off\n", __func__);
		}
	} else {
		aml_lcd_notifier_call_chain(LCD_EVENT_IF_POWER_OFF, (void *)pdrv);
	}
	mutex_unlock(&lcd_power_mutex);
}

static ssize_t lcd_debug_store(struct device *dev, struct device_attribute *attr,
			       const char *buf, size_t count)
{
	int i, ret = 0;
	unsigned int temp, val[6];
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	struct lcd_config_s *pconf;
	char *print_buf;
	unsigned long flags = 0;

	pconf = &pdrv->config;
	switch (buf[0]) {
	case 'c': /* clk */
		ret = sscanf(buf, "clk %d", &temp);
		if (ret == 1) {
			if (temp > 200) {
				pr_info("set clk: %dHz\n", temp);
			} else {
				pr_info("set frame_rate: %dHz\n", temp);
				temp = pconf->basic.h_period * pconf->basic.v_period * temp;
				pr_info("set clk: %dHz\n", temp);
			}
			pconf->timing.lcd_clk = temp;
			pconf->timing.lcd_clk_dft = pconf->timing.lcd_clk;
			lcd_debug_clk_change(pdrv);
		} else {
			LCDERR("invalid data\n");
			return -EINVAL;
		}
		break;
	case 'b':
		if (buf[1] == 'a') { /* basic */
			ret = sscanf(buf, "basic %d %d %d %d %d",
				     &val[0], &val[1], &val[2], &val[3], &val[4]);
			if (ret == 4) {
				pconf->basic.h_active = val[0];
				pconf->basic.v_active = val[1];
				pconf->basic.h_period = val[2];
				pconf->basic.v_period = val[3];
				pconf->timing.h_period_dft = val[2];
				pconf->timing.v_period_dft = val[3];
				pr_info("set h_active=%d, v_active=%d\n",
					val[0], val[1]);
				pr_info("set h_period=%d, v_period=%d\n",
					val[2], val[3]);
				lcd_timing_init_config(pdrv);
				lcd_debug_config_update(pdrv);
			} else if (ret == 5) {
				pconf->basic.h_active = val[0];
				pconf->basic.v_active = val[1];
				pconf->basic.h_period = val[2];
				pconf->basic.v_period = val[3];
				pconf->timing.h_period_dft = val[2];
				pconf->timing.v_period_dft = val[3];
				pconf->basic.lcd_bits = val[4];
				pr_info("set h_active=%d, v_active=%d\n",
					val[0], val[1]);
				pr_info("set h_period=%d, v_period=%d\n",
					val[2], val[3]);
				pr_info("set lcd_bits=%d\n", val[4]);
				lcd_timing_init_config(pdrv);
				lcd_debug_config_update(pdrv);
			} else {
				LCDERR("invalid data\n");
				return -EINVAL;
			}
		} else if (buf[1] == 'i') { /* bit */
			ret = sscanf(buf, "bit %d", &val[0]);
			if (ret == 1) {
				pconf->basic.lcd_bits = val[4];
				pr_info("set lcd_bits=%d\n", val[4]);
				lcd_debug_config_update(pdrv);
			} else {
				LCDERR("invalid data\n");
				return -EINVAL;
			}
		}
		break;
	case 's': /* sync */
		ret = sscanf(buf, "sync %d %d %d %d %d %d",
			     &val[0], &val[1], &val[2], &val[3], &val[4], &val[5]);
		if (ret == 6) {
			pconf->timing.hsync_width = val[0];
			pconf->timing.hsync_bp =    val[1];
			pconf->timing.hsync_pol =   val[2];
			pconf->timing.vsync_width = val[3];
			pconf->timing.vsync_bp =    val[4];
			pconf->timing.vsync_pol =   val[5];
			pr_info("set hsync width=%d, bp=%d, pol=%d\n",
				val[0], val[1], val[2]);
			pr_info("set vsync width=%d, bp=%d, pol=%d\n",
				val[3], val[4], val[5]);
			lcd_timing_init_config(pdrv);
			lcd_debug_config_update(pdrv);
		} else {
			LCDERR("invalid data\n");
			return -EINVAL;
		}
		break;
	case 't': /* test */
		ret = sscanf(buf, "test %d", &temp);
		if (ret == 1) {
			spin_lock_irqsave(&pdrv->isr_lock, flags);
			pdrv->test_flag = (unsigned char)temp;
			spin_unlock_irqrestore(&pdrv->isr_lock, flags);
			LCDPR("%s: test %d\n", __func__, temp);
			i = 0;
			while (i++ < 5000) {
				if (pdrv->test_state == temp)
					break;
				usleep_range(20, 30);
			}
		} else {
			LCDERR("invalid data\n");
			return -EINVAL;
		}
		break;
	case 'i': /* info */
		print_buf = kcalloc(PR_BUF_MAX, sizeof(char), GFP_KERNEL);
		if (!print_buf) {
			LCDERR("%s: buf malloc error\n", __func__);
			return -EINVAL;
		}
		lcd_info_basic_print(pdrv, print_buf, 0);
		lcd_debug_info_print(print_buf);
		memset(print_buf, 0, PR_BUF_MAX);
		lcd_info_adv_print(pdrv, print_buf, 0);
		lcd_debug_info_print(print_buf);
		memset(print_buf, 0, PR_BUF_MAX);
		lcd_info_tcon_print(pdrv, print_buf, 0);
		lcd_debug_info_print(print_buf);
		memset(print_buf, 0, PR_BUF_MAX);
		lcd_power_step_info_print(pdrv, print_buf, 0);
		lcd_debug_info_print(print_buf);
		kfree(print_buf);
		break;
	case 'r':
		if (buf[2] == 'g') { /* reg */
			print_buf = kcalloc(PR_BUF_MAX, sizeof(char),
					    GFP_KERNEL);
			if (!print_buf) {
				LCDERR("%s: buf malloc error\n", __func__);
				return -EINVAL;
			}
			lcd_reg_clk_print(pdrv, print_buf, 0);
			lcd_debug_info_print(print_buf);
			memset(print_buf, 0, PR_BUF_MAX);
			lcd_reg_encl_print(pdrv, print_buf, 0);
			lcd_debug_info_print(print_buf);
			memset(print_buf, 0, PR_BUF_MAX);
			lcd_reg_if_print(pdrv, print_buf, 0);
			lcd_debug_info_print(print_buf);
			memset(print_buf, 0, PR_BUF_MAX);
			lcd_reg_phy_print(pdrv, print_buf, 0);
			lcd_debug_info_print(print_buf);
			memset(print_buf, 0, PR_BUF_MAX);
			lcd_reg_pinmux_print(pdrv, print_buf, 0);
			lcd_debug_info_print(print_buf);
			kfree(print_buf);
		} else if (buf[2] == 's') { /* reset */
			pdrv->module_reset(pdrv);
		} else if (buf[2] == 'n') { /* range */
			ret = sscanf(buf, "range %d %d %d %d %d %d",
				     &val[0], &val[1], &val[2], &val[3],
				     &val[4], &val[5]);
			if (ret == 6) {
				pconf->basic.h_period_min = val[0];
				pconf->basic.h_period_max = val[1];
				pconf->basic.h_period_min = val[2];
				pconf->basic.v_period_max = val[3];
				pconf->basic.lcd_clk_min  = val[4];
				pconf->basic.lcd_clk_max  = val[5];
				pr_info("set h_period min=%d, max=%d\n",
					pconf->basic.h_period_min,
					pconf->basic.h_period_max);
				pr_info("set v_period min=%d, max=%d\n",
					pconf->basic.v_period_min,
					pconf->basic.v_period_max);
				pr_info("set pclk min=%d, max=%d\n",
					pconf->basic.lcd_clk_min,
					pconf->basic.lcd_clk_max);
			} else {
				LCDERR("invalid data\n");
				return -EINVAL;
			}
		}
		break;
	case 'd': /* dump */
		print_buf = kcalloc(PR_BUF_MAX, sizeof(char), GFP_KERNEL);
		if (!print_buf) {
			LCDERR("%s: buf malloc error\n", __func__);
			return -EINVAL;
		}
		lcd_info_basic_print(pdrv, print_buf, 0);
		lcd_debug_info_print(print_buf);
		memset(print_buf, 0, PR_BUF_MAX);
		lcd_info_adv_print(pdrv, print_buf, 0);
		lcd_debug_info_print(print_buf);
		memset(print_buf, 0, PR_BUF_MAX);
		lcd_info_tcon_print(pdrv, print_buf, 0);
		lcd_debug_info_print(print_buf);
		memset(print_buf, 0, PR_BUF_MAX);
		lcd_power_step_info_print(pdrv, print_buf, 0);
		lcd_debug_info_print(print_buf);
		memset(print_buf, 0, PR_BUF_MAX);
		lcd_reg_clk_print(pdrv, print_buf, 0);
		lcd_debug_info_print(print_buf);
		memset(print_buf, 0, PR_BUF_MAX);
		lcd_reg_encl_print(pdrv, print_buf, 0);
		lcd_debug_info_print(print_buf);
		memset(print_buf, 0, PR_BUF_MAX);
		lcd_reg_if_print(pdrv, print_buf, 0);
		lcd_debug_info_print(print_buf);
		memset(print_buf, 0, PR_BUF_MAX);
		lcd_reg_phy_print(pdrv, print_buf, 0);
		lcd_debug_info_print(print_buf);
		memset(print_buf, 0, PR_BUF_MAX);
		lcd_reg_pinmux_print(pdrv, print_buf, 0);
		lcd_debug_info_print(print_buf);
		memset(print_buf, 0, PR_BUF_MAX);
		lcd_optical_info_print(pdrv, print_buf, 0);
		lcd_debug_info_print(print_buf);
		i = lcd_clk_config_print(pdrv, print_buf, 0);
		lcd_clk_clkmsr_print(pdrv, (print_buf + i), i);
		lcd_debug_info_print(print_buf);
		kfree(print_buf);
		break;
	case 'k': /* key */
		LCDPR("key_valid: %d, config_load: %d\n",
		      pdrv->key_valid, pdrv->config_load);
		if (pdrv->key_valid)
			lcd_unifykey_print();
		break;
	case 'h': /* hdr */
		print_buf = kcalloc(PR_BUF_MAX, sizeof(char), GFP_KERNEL);
		if (!print_buf) {
			LCDERR("%s: buf malloc error\n", __func__);
			return -EINVAL;
		}
		lcd_optical_info_print(pdrv, print_buf, 0);
		lcd_debug_info_print(print_buf);
		kfree(print_buf);
		break;
	case 'p': /* power */
		ret = sscanf(buf, "power %d", &temp);
		if (ret == 1) {
			lcd_power_interface_ctrl(pdrv, temp);
		} else {
			LCDERR("invalid data\n");
			return -EINVAL;
		}
		break;
	case 'v':
		ret = sscanf(buf, "vout %d", &temp);
		if (ret == 1) {
			LCDPR("vout_serve bypass: %d\n", temp);
			lcd_vout_serve_bypass = temp;
		} else {
			LCDERR("invalid data\n");
			return -EINVAL;
		}
		break;
	case 'g':
		ret = sscanf(buf, "gamma %d", &temp);
		if (ret == 1) {
			LCDPR("gamma en: %d\n", temp);
			lcd_gamma_debug_test_en(pdrv, temp);
		} else {
			LCDERR("invalid data\n");
			return -EINVAL;
		}
		break;
	default:
		LCDERR("wrong command\n");
		break;
	}
	return count;
}

static ssize_t lcd_debug_change_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", lcd_debug_change_usage_str);
}

static void lcd_debug_change_clk_change(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_config_s *pconf = &pdrv->config;
	unsigned int pclk, sync_duration;

	pclk = pconf->timing.lcd_clk;
	sync_duration = pclk / pconf->basic.h_period;
	sync_duration = sync_duration * 100 / pconf->basic.v_period;
	pconf->timing.frame_rate = sync_duration / 100;
	pconf->timing.sync_duration_num = sync_duration;
	pconf->timing.sync_duration_den = 100;

	/* update vinfo */
	pdrv->vinfo.sync_duration_num = sync_duration;
	pdrv->vinfo.sync_duration_den = 100;
	pdrv->vinfo.std_duration = sync_duration / 100;
	pdrv->vinfo.video_clk = pclk;

	switch (pdrv->mode) {
#ifdef CONFIG_AMLOGIC_LCD_TV
	case LCD_MODE_TV:
		lcd_tv_clk_config_change(pdrv);
		break;
#endif
#ifdef CONFIG_AMLOGIC_LCD_TABLET
	case LCD_MODE_TABLET:
		lcd_tablet_clk_config_change(pdrv);
		break;
#endif
	default:
		LCDPR("invalid lcd mode\n");
		break;
	}
}

static ssize_t lcd_debug_change_store(struct device *dev, struct device_attribute *attr,
				      const char *buf, size_t count)
{
	int ret = 0;
	unsigned int temp, val[10];
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	struct lcd_config_s *pconf;
	union lcd_ctrl_config_u *pctrl;

	pconf = &pdrv->config;
	pctrl = &pconf->control;

	switch (buf[0]) {
	case 'c': /* clk */
		ret = sscanf(buf, "clk %d", &temp);
		if (ret == 1) {
			if (temp > 200) {
				pr_info("change clk=%dHz\n", temp);
			} else {
				pr_info("change frame_rate=%dHz\n", temp);
				temp = pconf->basic.h_period * pconf->basic.v_period * temp;
				pr_info("change clk=%dHz\n", temp);
			}
			pconf->timing.lcd_clk = temp;
			pconf->timing.lcd_clk_dft = pconf->timing.lcd_clk;
			lcd_debug_change_clk_change(pdrv);
			pconf->change_flag = 1;
		} else {
			LCDERR("invalid data\n");
			return -EINVAL;
		}
		break;
	case 'b':
		if (buf[1] == 'a') { /* basic */
			ret = sscanf(buf, "basic %d %d %d %d %d",
				     &val[0], &val[1], &val[2], &val[3], &val[4]);
			if (ret == 4) {
				pconf->basic.h_active = val[0];
				pconf->basic.v_active = val[1];
				pconf->basic.h_period = val[2];
				pconf->basic.v_period = val[3];
				pconf->timing.h_period_dft = val[2];
				pconf->timing.v_period_dft = val[3];
				pr_info("change h_active=%d, v_active=%d\n",
					val[0], val[1]);
				pr_info("change h_period=%d, v_period=%d\n",
					val[2], val[3]);
				lcd_timing_init_config(pdrv);
				pconf->change_flag = 1;
			} else if (ret == 5) {
				pconf->basic.h_active = val[0];
				pconf->basic.v_active = val[1];
				pconf->basic.h_period = val[2];
				pconf->basic.v_period = val[3];
				pconf->timing.h_period_dft = val[2];
				pconf->timing.v_period_dft = val[3];
				pconf->basic.lcd_bits = val[4];
				pr_info("change h_active=%d, v_active=%d\n",
					val[0], val[1]);
				pr_info("change h_period=%d, v_period=%d\n",
					val[2], val[3]);
				pr_info("change lcd_bits=%d\n", val[4]);
				lcd_timing_init_config(pdrv);
				pconf->change_flag = 1;
			} else {
				LCDERR("invalid data\n");
				return -EINVAL;
			}
		} else if (buf[1] == 'i') { /* bit */
			ret = sscanf(buf, "bit %d", &val[0]);
			if (ret == 1) {
				pconf->basic.lcd_bits = val[4];
				pr_info("change lcd_bits=%d\n", val[4]);
				pconf->change_flag = 1;
			} else {
				LCDERR("invalid data\n");
				return -EINVAL;
			}
		}
		break;
	case 's':
		if (buf[1] == 'e') { /* set */
			if (pconf->change_flag) {
				LCDPR("apply config changing\n");
				lcd_debug_config_update(pdrv);
			} else {
				LCDPR("config is no changing\n");
			}
		} else if (buf[1] == 'y') { /* sync */
			ret = sscanf(buf, "sync %d %d %d %d %d %d",
				     &val[0], &val[1], &val[2], &val[3],
				     &val[4], &val[5]);
			if (ret == 6) {
				pconf->timing.hsync_width = val[0];
				pconf->timing.hsync_bp =    val[1];
				pconf->timing.hsync_pol =   val[2];
				pconf->timing.vsync_width = val[3];
				pconf->timing.vsync_bp =    val[4];
				pconf->timing.vsync_pol =   val[5];
				pr_info("change hs width=%d, bp=%d, pol=%d\n",
					val[0], val[1], val[2]);
				pr_info("change vs width=%d, bp=%d, pol=%d\n",
					val[3], val[4], val[5]);
				lcd_timing_init_config(pdrv);
				pconf->change_flag = 1;
			} else {
				LCDERR("invalid data\n");
				return -EINVAL;
			}
		}
		break;
	case 'l':
		ret = sscanf(buf, "lvds %d %d %d %d %d",
			     &val[0], &val[1], &val[2], &val[3], &val[4]);
		if (ret == 5) {
			pctrl->lvds_cfg.lvds_repack = val[0];
			pctrl->lvds_cfg.dual_port = val[1];
			pctrl->lvds_cfg.pn_swap = val[2];
			pctrl->lvds_cfg.port_swap = val[3];
			pctrl->lvds_cfg.lane_reverse = val[4];
			pr_info("set lvds config:\n"
	"repack=%d, dual_port=%d, pn_swap=%d, port_swap=%d, lane_reverse=%d\n",
				pctrl->lvds_cfg.lvds_repack,
				pctrl->lvds_cfg.dual_port,
				pctrl->lvds_cfg.pn_swap,
				pctrl->lvds_cfg.port_swap,
				pctrl->lvds_cfg.lane_reverse);
			lcd_debug_change_clk_change(pdrv);
			pconf->change_flag = 1;
		} else if (ret == 4) {
			pctrl->lvds_cfg.lvds_repack = val[0];
			pctrl->lvds_cfg.dual_port = val[1];
			pctrl->lvds_cfg.pn_swap = val[2];
			pctrl->lvds_cfg.port_swap = val[3];
			pr_info("set lvds config:\n"
			"repack=%d, dual_port=%d, pn_swap=%d, port_swap=%d\n",
				pctrl->lvds_cfg.lvds_repack,
				pctrl->lvds_cfg.dual_port,
				pctrl->lvds_cfg.pn_swap,
				pctrl->lvds_cfg.port_swap);
			lcd_debug_change_clk_change(pdrv);
			pconf->change_flag = 1;
		} else {
			LCDERR("invalid data\n");
			return -EINVAL;
		}
		break;
	case 'v':
		ret = sscanf(buf, "vbyone %d %d %d %d",
			     &val[0], &val[1], &val[2], &val[3]);
		if (ret == 4 || ret == 3) {
			pctrl->vbyone_cfg.lane_count = val[0];
			pctrl->vbyone_cfg.region_num = val[1];
			pctrl->vbyone_cfg.byte_mode = val[2];
			pr_info("set vbyone config:\n"
				"lane_count=%d, region_num=%d, byte_mode=%d\n",
				pctrl->vbyone_cfg.lane_count,
				pctrl->vbyone_cfg.region_num,
				pctrl->vbyone_cfg.byte_mode);
			lcd_debug_change_clk_change(pdrv);
			pconf->change_flag = 1;
		} else {
			LCDERR("invalid data\n");
			return -EINVAL;
		}
		break;
	case 'm':
		if (buf[1] == 'i') {
			ret = sscanf(buf, "mipi %d %d %d %d %d %d %d %d",
				     &val[0], &val[1], &val[2], &val[3],
				     &val[4], &val[5], &val[6], &val[7]);
			if (ret == 8) {
				pctrl->mipi_cfg.lane_num = (unsigned char)val[0];
				pctrl->mipi_cfg.bit_rate_max = val[1];
				pctrl->mipi_cfg.factor_numerator = val[2];
				pctrl->mipi_cfg.operation_mode_init =
					(unsigned char)val[3];
				pctrl->mipi_cfg.operation_mode_display =
					(unsigned char)val[4];
				pctrl->mipi_cfg.video_mode_type =
					(unsigned char)val[5];
				pctrl->mipi_cfg.clk_always_hs = (unsigned char)val[6];
				pctrl->mipi_cfg.phy_switch = (unsigned char)val[7];
				pr_info("change mipi_dsi config:\n"
			"lane_num=%d, bit_rate_max=%dMhz, factor_numerator=%d\n"
			"operation_mode_init=%d, operation_mode_display=%d\n"
			"video_mode_type=%d, clk_always_hs=%d, phy_switch=%d\n",
					pctrl->mipi_cfg.lane_num,
					pctrl->mipi_cfg.bit_rate_max,
					pctrl->mipi_cfg.factor_numerator,
					pctrl->mipi_cfg.operation_mode_init,
					pctrl->mipi_cfg.operation_mode_display,
					pctrl->mipi_cfg.video_mode_type,
					pctrl->mipi_cfg.clk_always_hs,
					pctrl->mipi_cfg.phy_switch);
				lcd_debug_change_clk_change(pdrv);
				pconf->change_flag = 1;
			} else {
				LCDERR("invalid data\n");
				return -EINVAL;
			}
		} else if (buf[1] == 'l') {
			ret = sscanf(buf, "mlvds %d %x %x %x %d %d",
				     &val[0], &val[1], &val[2], &val[3],
				     &val[4], &val[5]);
			if (ret == 6) {
				pctrl->mlvds_cfg.channel_num = val[0];
				pctrl->mlvds_cfg.channel_sel0 = val[1];
				pctrl->mlvds_cfg.channel_sel1 = val[2];
				pctrl->mlvds_cfg.clk_phase = val[3];
				pctrl->mlvds_cfg.pn_swap = val[4];
				pctrl->mlvds_cfg.bit_swap = val[5];
				pr_info("change mlvds config:\n"
					"channel_num=%d,\n"
				"channel_sel0=0x%08x, channel_sel1=0x%08x,\n"
					"clk_phase=0x%04x,\n"
					"pn_swap=%d, bit_swap=%d\n",
					pctrl->mlvds_cfg.channel_num,
					pctrl->mlvds_cfg.channel_sel0,
					pctrl->mlvds_cfg.channel_sel1,
					pctrl->mlvds_cfg.clk_phase,
					pctrl->mlvds_cfg.pn_swap,
					pctrl->mlvds_cfg.bit_swap);
				lcd_debug_change_clk_change(pdrv);
				pconf->change_flag = 1;
			} else {
				LCDERR("invalid data\n");
				return -EINVAL;
			}
		}
		break;
	case 'p':
		ret = sscanf(buf, "p2p %x %d %x %x %d %d",
			     &val[0], &val[1], &val[2], &val[3], &val[4], &val[5]);
		if (ret == 6) {
			pctrl->p2p_cfg.p2p_type = val[0];
			pctrl->p2p_cfg.lane_num = val[1];
			pctrl->p2p_cfg.channel_sel0 = val[2];
			pctrl->p2p_cfg.channel_sel1 = val[3];
			pctrl->p2p_cfg.pn_swap = val[4];
			pctrl->p2p_cfg.bit_swap = val[5];
			pr_info("change p2p config:\n"
				"p2p_type=0x%x, lane_num=%d,\n"
				"channel_sel0=0x%08x, channel_sel1=0x%08x,\n"
				"pn_swap=%d, bit_swap=%d\n",
				pctrl->p2p_cfg.p2p_type,
				pctrl->p2p_cfg.lane_num,
				pctrl->p2p_cfg.channel_sel0,
				pctrl->p2p_cfg.channel_sel1,
				pctrl->p2p_cfg.pn_swap,
				pctrl->p2p_cfg.bit_swap);
			lcd_debug_change_clk_change(pdrv);
			pconf->change_flag = 1;
		} else {
			LCDERR("invalid data\n");
			return -EINVAL;
		}
		break;
	case 'r':
		ret = sscanf(buf, "rgb %d %d %d %d %d %d",
			     &val[0], &val[1], &val[2], &val[3], &val[4], &val[5]);
		if (ret == 5) {
			pctrl->rgb_cfg.type = val[0];
			pctrl->rgb_cfg.clk_pol = val[1];
			pctrl->rgb_cfg.de_valid = val[2];
			pctrl->rgb_cfg.sync_valid = val[3];
			pctrl->rgb_cfg.rb_swap = val[4];
			pctrl->rgb_cfg.bit_swap = val[5];
			pr_info("set rgb config:\n"
	"type=%d, clk_pol=%d, de_valid=%d, sync_valid=%d, rb_swap=%d, bit_swap=%d\n",
				val[0], val[1], val[2], val[3], val[4], val[5]);
			lcd_debug_change_clk_change(pdrv);
			pconf->change_flag = 1;
		} else {
			LCDERR("invalid data\n");
			return -EINVAL;
		}
		break;
	case 'u': /* update */
		if (pconf->change_flag) {
			LCDPR("apply config changing\n");
			lcd_debug_config_update(pdrv);
		} else {
			LCDPR("config is no changing\n");
		}
		break;
	default:
		LCDERR("wrong command\n");
		break;
	}

	return count;
}

static ssize_t lcd_debug_enable_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);

	return sprintf(buf, "lcd_status: 0x%x\n", pdrv->status);
}

static ssize_t lcd_debug_enable_store(struct device *dev, struct device_attribute *attr,
				      const char *buf, size_t count)
{
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	int ret = 0;
	unsigned int temp = 1;

	ret = kstrtouint(buf, 10, &temp);
	if (ret) {
		LCDERR("invalid data\n");
		return -EINVAL;
	}
	if (temp) {
		mutex_lock(&lcd_power_mutex);
		aml_lcd_notifier_call_chain(LCD_EVENT_POWER_ON, (void *)pdrv);
		lcd_if_enable_retry(pdrv);
		mutex_unlock(&lcd_power_mutex);
	} else {
		mutex_lock(&lcd_power_mutex);
		aml_lcd_notifier_call_chain(LCD_EVENT_POWER_OFF, (void *)pdrv);
		mutex_unlock(&lcd_power_mutex);
	}

	return count;
}

static ssize_t lcd_debug_resume_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);

	return sprintf(buf, "lcd resume type: %d(%s)\n",
		pdrv->resume_type,
		pdrv->resume_type ? "workqueue" : "directly");
}

static ssize_t lcd_debug_resume_store(struct device *dev, struct device_attribute *attr,
				      const char *buf, size_t count)
{
	int ret = 0;
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	unsigned int temp = 1;

	ret = kstrtouint(buf, 10, &temp);
	if (ret) {
		LCDERR("invalid data\n");
		return -EINVAL;
	}
	pdrv->resume_type = (unsigned char)temp;
	LCDPR("set lcd resume flag: %d\n", pdrv->resume_type);

	return count;
}

static ssize_t lcd_debug_power_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	int state;

	if ((pdrv->status & LCD_STATUS_ON) == 0) {
		state = 0;
	} else {
		if (pdrv->status & LCD_STATUS_IF_ON)
			state = 1;
		else
			state = 0;
	}
	return sprintf(buf, "lcd power state: %d\n", state);
}

static ssize_t lcd_debug_power_store(struct device *dev, struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	int ret = 0;
	unsigned int temp = 1;

	ret = kstrtouint(buf, 10, &temp);
	if (ret) {
		LCDERR("invalid data\n");
		return -EINVAL;
	}
	if (temp) {
		mutex_lock(&lcd_power_mutex);
		aml_lcd_notifier_call_chain(LCD_EVENT_IF_POWER_ON, (void *)pdrv);
		lcd_if_enable_retry(pdrv);
		mutex_unlock(&lcd_power_mutex);
	} else {
		mutex_lock(&lcd_power_mutex);
		aml_lcd_notifier_call_chain(LCD_EVENT_IF_POWER_OFF, (void *)pdrv);
		mutex_unlock(&lcd_power_mutex);
	}

	return count;
}

static ssize_t lcd_debug_power_step_show(struct device *dev, struct device_attribute *attr,
					 char *buf)
{
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	char *print_buf;
	int n = 0;

	print_buf = kcalloc(PR_BUF_MAX, sizeof(char), GFP_KERNEL);
	if (!print_buf)
		return sprintf(buf, "%s: buf malloc error\n", __func__);

	lcd_power_step_info_print(pdrv, print_buf, 0);

	n = sprintf(buf, "%s\n", print_buf);
	kfree(print_buf);

	return n;
}

static ssize_t lcd_debug_power_step_store(struct device *dev, struct device_attribute *attr,
					  const char *buf, size_t count)
{
	int ret = 0;
	unsigned int i;
	unsigned int tmp[2];
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	struct lcd_power_ctrl_s *power_step;

	power_step = &pdrv->config.power;
	switch (buf[1]) {
	case 'n': /* on */
		ret = sscanf(buf, "on %d %d %d", &i, &tmp[0], &tmp[1]);
		if (ret == 3) {
			if (i >= power_step->power_on_step_max) {
				pr_info("invalid power_on step: %d, step_max: %d\n",
					i, power_step->power_on_step_max);
				return -EINVAL;
			}
			power_step->power_on_step[i].value = tmp[0];
			power_step->power_on_step[i].delay = tmp[1];
			pr_info
			("set power_on step %d value %d delay: %dms\n",
			 i, tmp[0], tmp[1]);
		} else if (ret == 2) {
			if (i >= power_step->power_on_step_max) {
				pr_info("invalid power_on step: %d\n", i);
				return -EINVAL;
			}
			power_step->power_on_step[i].delay = tmp[0];
			pr_info("set power_on step %d delay: %dms\n",
				i, tmp[0]);
		} else {
			pr_info("invalid data\n");
			return -EINVAL;
		}
		break;
	case 'f': /* off */
		ret = sscanf(buf, "off %d %d %d\n", &i, &tmp[0], &tmp[1]);
		if (ret == 3) {
			if (i >= power_step->power_off_step_max) {
				pr_info("invalid power_off step: %d\n", i);
				return -EINVAL;
			}
			power_step->power_off_step[i].value = tmp[0];
			power_step->power_off_step[i].delay = tmp[1];
			pr_info
			("set power_off step %d value %d delay: %dms\n",
			 i, tmp[0], tmp[1]);
		} else if (ret == 2) {
			if (i >= power_step->power_off_step_max) {
				pr_info("invalid power_off step: %d\n", i);
				return -EINVAL;
			}
			power_step->power_off_step[i].delay = tmp[0];
			pr_info("set power_off step %d delay: %dms\n",
				i, tmp[0]);
		} else {
			pr_info("invalid data\n");
			return -EINVAL;
		}
		break;
	default:
		pr_info("wrong command\n");
		break;
	}

	return count;
}

static ssize_t lcd_debug_frame_rate_show(struct device *dev, struct device_attribute *attr,
					 char *buf)
{
	unsigned int sync_duration;
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	struct lcd_config_s *pconf;

	pconf = &pdrv->config;
	sync_duration = pconf->timing.sync_duration_num * 100;
	sync_duration = sync_duration / pconf->timing.sync_duration_den;

	return sprintf(buf, "get frame_rate: %u.%02uHz, fr_adjust_type: %d\n",
		(sync_duration / 100), (sync_duration % 100),
		pconf->timing.fr_adjust_type);
}

static ssize_t lcd_debug_frame_rate_store(struct device *dev, struct device_attribute *attr,
					  const char *buf, size_t count)
{
	int ret = 0;
	unsigned int temp = 0;
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);

	switch (buf[0]) {
	case 't':
		ret = sscanf(buf, "type %d", &temp);
		if (ret == 1) {
			pdrv->config.timing.fr_adjust_type = temp;
			pr_info("set fr_adjust_type: %d\n", temp);
		} else {
			pr_info("invalid data\n");
			return -EINVAL;
		}
		break;
	case 's':
		ret = sscanf(buf, "set %d", &temp);
		if (ret == 1) {
			pr_info("set frame rate(*100): %d\n", temp);
			pdrv->fr_adjust(pdrv, temp);
		} else {
			pr_info("invalid data\n");
			return -EINVAL;
		}
		break;
	default:
		pr_info("wrong command\n");
		break;
	}

	return count;
}

static ssize_t lcd_debug_fr_flag_show(struct device *dev, struct device_attribute *attr,
					char *buf)
{
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);

	return sprintf(buf, "fr_auto_flag: 0x%x\n", pdrv->config.fr_auto_flag);
}

static ssize_t lcd_debug_fr_flag_store(struct device *dev, struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	unsigned int temp = 0;
	int ret = 0;

	ret = kstrtouint(buf, 16, &temp);
	if (ret) {
		pr_info("invalid data\n");
		return -EINVAL;
	}
	pdrv->config.fr_auto_flag = temp;
	pr_info("set fr_auto_flag: 0x%x\n", temp);

	return count;
}

static ssize_t lcd_debug_ss_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	int len;

	len = lcd_get_ss(pdrv, buf);
	return len;
}

static ssize_t lcd_debug_ss_store(struct device *dev, struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	unsigned int value = 0;
	int ret = 0;

	switch (buf[0]) {
	case 'l':
		ret = sscanf(buf, "level %d", &value);
		if (ret == 1) {
			value &= 0xff;
			ret = lcd_set_ss(pdrv, value, 0xff, 0xff);
			if (ret == 0)
				pdrv->config.timing.ss_level = value;
		} else {
			pr_info("invalid data\n");
			return -EINVAL;
		}
		break;
	case 'f':
		ret = sscanf(buf, "freq %d", &value);
		if (ret == 1) {
			value &= 0xf;
			ret = lcd_set_ss(pdrv, 0xff, value, 0xff);
			if (ret == 0)
				pdrv->config.timing.ss_freq = value;
		} else {
			pr_info("invalid data\n");
			return -EINVAL;
		}
		break;
	case 'm':
		ret = sscanf(buf, "mode %d", &value);
		if (ret == 1) {
			value &= 0xf;
			ret = lcd_set_ss(pdrv, 0xff, 0xff, value);
			if (ret == 0)
				pdrv->config.timing.ss_mode = value;
		} else {
			pr_info("invalid data\n");
			return -EINVAL;
		}
		break;
	case 'e':
		ret = sscanf(buf, "en %d", &value);
		if (ret == 1) {
			lcd_ss_enable(pdrv->index, value);
		} else {
			pr_info("invalid data\n");
			return -EINVAL;
		}
		break;
	default:
		pr_info("invalid data\n");
		break;
	}

	return count;
}

static ssize_t lcd_debug_clk_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	char *print_buf;
	int n = 0;

	print_buf = kcalloc(PR_BUF_MAX, sizeof(char), GFP_KERNEL);
	if (!print_buf)
		return sprintf(buf, "%s: buf malloc error\n", __func__);

	n = lcd_clk_config_print(pdrv, print_buf, 0);
	lcd_clk_clkmsr_print(pdrv, (print_buf + n), n);

	n = sprintf(buf, "%s\n", print_buf);
	kfree(print_buf);

	return n;
}

static ssize_t lcd_debug_clk_store(struct device *dev, struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	unsigned int temp = 0;
	int ret = 0;

	switch (buf[0]) {
	case 'p':
		ret = sscanf(buf, "path %d", &temp);
		if (ret == 1) {
			ret = lcd_clk_path_change(pdrv, temp);
			if (ret) {
				pr_info("change clk_path error\n");
			} else {
				pdrv->clk_path = temp;
				lcd_clk_generate_parameter(pdrv);
				pr_info("change clk_path: %d\n", temp);
			}
		} else {
			pr_info("invalid data\n");
			return -EINVAL;
		}
		break;
	default:
		pr_info("wrong command\n");
		break;
	}

	return count;
}

static ssize_t lcd_debug_test_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);

	return sprintf(buf, "[%d]: test pattern: %d\n", pdrv->index, pdrv->test_state);
}

static ssize_t lcd_debug_test_store(struct device *dev, struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	unsigned int temp = 0, i = 0;
	int ret = 0;
	unsigned long flags = 0;

	if (buf[0] == 'f') { /* force test pattern */
		ret = sscanf(buf, "force %d", &temp);
		if (ret == 0)
			goto lcd_debug_test_store_next;
		spin_lock_irqsave(&pdrv->isr_lock, flags);
		pdrv->test_flag = (unsigned char)temp;
		pdrv->test_state = (unsigned char)temp;
		lcd_debug_test(pdrv, pdrv->test_state);
		spin_unlock_irqrestore(&pdrv->isr_lock, flags);
		return count;
	}

lcd_debug_test_store_next:
	ret = kstrtouint(buf, 10, &temp);
	if (ret) {
		pr_info("invalid data\n");
		return -EINVAL;
	}
	spin_lock_irqsave(&pdrv->isr_lock, flags);
	pdrv->test_flag = (unsigned char)temp;
	spin_unlock_irqrestore(&pdrv->isr_lock, flags);

	LCDPR("[%d]: %s: %d\n", pdrv->index, __func__, temp);
	while (i++ < 5000) {
		if (pdrv->test_state == temp)
			break;
		lcd_delay_us(20);
	}

	return count;
}

static ssize_t lcd_debug_mute_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
#ifdef CONFIG_AMLOGIC_MEDIA_VIDEO
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
#endif
	int flag = 0;

#ifdef CONFIG_AMLOGIC_MEDIA_VIDEO
	flag = (pdrv->viu_sel == 1) ? get_output_mute() : 0;
#endif

	return sprintf(buf, "get lcd mute state: %d\n", flag);
}

static ssize_t lcd_debug_mute_store(struct device *dev, struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	unsigned int temp = 0;
	int ret = 0;

	ret = kstrtouint(buf, 10, &temp);
	if (ret) {
		pr_info("invalid data\n");
		return -EINVAL;
	}

	temp = temp ? 1 : 0;
	if (temp)
		lcd_screen_black(pdrv);
	else
		lcd_screen_restore(pdrv);

	return count;
}

static void lcd_debug_reg_write(struct aml_lcd_drv_s *pdrv, unsigned int reg,
				unsigned int data, unsigned int bus)
{
	unsigned char temp = 0;
	int ret;

	switch (bus) {
	case LCD_REG_DBG_VC_BUS:
		lcd_vcbus_write(reg, data);
		pr_info("write vcbus [0x%04x] = 0x%08x, readback 0x%08x\n",
			reg, data, lcd_vcbus_read(reg));
		break;
	case LCD_REG_DBG_ANA_BUS:
		lcd_ana_write(reg, data);
		pr_info("write ana [0x%04x] = 0x%08x, readback 0x%08x\n",
			reg, data, lcd_ana_read(reg));
		break;
	case LCD_REG_DBG_CLK_BUS:
		lcd_clk_write(reg, data);
		pr_info("write clk [0x%04x] = 0x%08x, readback 0x%08x\n",
			reg, data, lcd_clk_read(reg));
		break;
	case LCD_REG_DBG_PERIPHS_BUS:
		lcd_periphs_write(pdrv, reg, data);
		pr_info("write periphs [0x%04x] = 0x%08x, readback 0x%08x\n",
			reg, data, lcd_periphs_read(pdrv, reg));
		break;
	case LCD_REG_DBG_MIPIHOST_BUS:
		dsi_host_write(pdrv, reg, data);
		pr_info("write mipi_dsi_host [0x%04x] = 0x%08x, readback 0x%08x\n",
			reg, data, dsi_host_read(pdrv, reg));
		break;
	case LCD_REG_DBG_MIPIPHY_BUS:
		dsi_phy_write(pdrv, reg, data);
		pr_info("write mipi_dsi_phy [0x%04x] = 0x%08x, readback 0x%08x\n",
			reg, data, dsi_phy_read(pdrv, reg));
		break;
	case LCD_REG_DBG_TCON_BUS:
		lcd_tcon_reg_write(pdrv, reg, data);
		if (reg < TCON_TOP_BASE) {
			pr_info("write tcon [0x%04x] = 0x%02x, readback 0x%02x\n",
				reg, data, lcd_tcon_reg_read(pdrv, reg));
		} else {
			pr_info("write tcon [0x%04x] = 0x%08x, readback 0x%08x\n",
				reg, data, lcd_tcon_reg_read(pdrv, reg));
		}
		break;
	case LCD_REG_DBG_EDPHOST_BUS:
		dptx_reg_write(pdrv, reg, data);
		pr_info("write edp [0x%04x] = 0x%08x, readback 0x%08x\n",
			reg, data, dptx_reg_read(pdrv, reg));
		break;
	case LCD_REG_DBG_EDPDPCD_BUS:
		ret = dptx_aux_write_single(pdrv, reg, data);
		if (ret)
			break;
		ret = dptx_aux_read(pdrv, reg, 1, &temp);
		if (ret)
			break;
		pr_info("write edp dpcd [0x%04x] = 0x%02x, readback 0x%02x\n",
			reg, data, temp);
		break;
	case LCD_REG_DBG_COMBOPHY_BUS:
		lcd_combo_dphy_write(pdrv, reg, data);
		pr_info("write combo dphy [0x%04x] = 0x%08x, readback 0x%08x\n",
			reg, data, lcd_combo_dphy_read(pdrv, reg));
		break;
	case LCD_REG_DBG_RST_BUS:
		lcd_reset_write(pdrv, reg, data);
		pr_info("write rst [0x%04x] = 0x%08x, readback 0x%08x\n",
			reg, data, lcd_reset_read(pdrv, reg));
		break;
	default:
		break;
	}
}

static void lcd_debug_reg_read(struct aml_lcd_drv_s *pdrv,
			       unsigned int reg, unsigned int bus)
{
	unsigned char temp = 0;
	int ret;

	switch (bus) {
	case LCD_REG_DBG_VC_BUS:
		pr_info("read vcbus [0x%04x] = 0x%08x\n",
			reg, lcd_vcbus_read(reg));
		break;
	case LCD_REG_DBG_ANA_BUS:
		pr_info("read ana [0x%04x] = 0x%08x\n",
			reg, lcd_ana_read(reg));
		break;
	case LCD_REG_DBG_CLK_BUS:
		pr_info("read clk [0x%04x] = 0x%08x\n",
			reg, lcd_clk_read(reg));
		break;
	case LCD_REG_DBG_PERIPHS_BUS:
		pr_info("read periphs [0x%04x] = 0x%08x\n",
			reg, lcd_periphs_read(pdrv, reg));
		break;
	case LCD_REG_DBG_MIPIHOST_BUS:
		pr_info("read mipi_dsi_host [0x%04x] = 0x%08x\n",
			reg, dsi_host_read(pdrv, reg));
		break;
	case LCD_REG_DBG_MIPIPHY_BUS:
		pr_info("read mipi_dsi_phy [0x%04x] = 0x%08x\n",
			reg, dsi_phy_read(pdrv, reg));
		break;
	case LCD_REG_DBG_TCON_BUS:
		pr_info("read tcon [0x%04x] = 0x%08x\n",
			reg, lcd_tcon_reg_read(pdrv, reg));
		break;
	case LCD_REG_DBG_EDPHOST_BUS:
		pr_info("read edp [0x%04x] = 0x%08x\n",
			reg, dptx_reg_read(pdrv, reg));
		break;
	case LCD_REG_DBG_EDPDPCD_BUS:
		ret = dptx_aux_read(pdrv, reg, 1, &temp);
		if (ret)
			break;
		pr_info("read edp dpcd [0x%04x] = 0x%02x\n", reg, temp);
		break;
	case LCD_REG_DBG_COMBOPHY_BUS:
		pr_info("read combo dphy [0x%04x] = 0x%08x\n",
			reg, lcd_combo_dphy_read(pdrv, reg));
		break;
	case LCD_REG_DBG_RST_BUS:
		pr_info("read rst [0x%04x] = 0x%08x\n",
			reg, lcd_reset_read(pdrv, reg));
		break;
	default:
		break;
	}
}

static void lcd_debug_reg_dump(struct aml_lcd_drv_s *pdrv, unsigned int reg,
			       unsigned int num, unsigned int bus)
{
	unsigned char *buf;
	int i, ret;

	switch (bus) {
	case LCD_REG_DBG_VC_BUS:
		pr_info("dump vcbus regs:\n");
		for (i = 0; i < num; i++) {
			pr_info("[0x%04x] = 0x%08x\n",
				(reg + i), lcd_vcbus_read(reg + i));
		}
		break;
	case LCD_REG_DBG_ANA_BUS:
		pr_info("dump ana regs:\n");
		for (i = 0; i < num; i++) {
			pr_info("[0x%04x] = 0x%08x\n",
				(reg + i), lcd_ana_read(reg + i));
		}
		break;
	case LCD_REG_DBG_CLK_BUS:
		pr_info("dump clk regs:\n");
		for (i = 0; i < num; i++) {
			pr_info("[0x%04x] = 0x%08x\n",
				(reg + i), lcd_clk_read(reg + i));
		}
		break;
	case LCD_REG_DBG_PERIPHS_BUS:
		pr_info("dump periphs-bus regs:\n");
		for (i = 0; i < num; i++) {
			pr_info("[0x%04x] = 0x%08x\n",
				(reg + i), lcd_periphs_read(pdrv, reg + i));
		}
		break;
	case LCD_REG_DBG_MIPIHOST_BUS:
		pr_info("dump mipi_dsi_host regs:\n");
		for (i = 0; i < num; i++) {
			pr_info("[0x%04x] = 0x%08x\n",
				(reg + i), dsi_host_read(pdrv, reg + i));
		}
		break;
	case LCD_REG_DBG_MIPIPHY_BUS:
		pr_info("dump mipi_dsi_phy regs:\n");
		for (i = 0; i < num; i++) {
			pr_info("[0x%04x] = 0x%08x\n",
				(reg + i), dsi_phy_read(pdrv, reg + i));
		}
		break;
	case LCD_REG_DBG_TCON_BUS:
		pr_info("dump tcon regs:\n");
		for (i = 0; i < num; i++) {
			pr_info("[0x%04x] = 0x%08x\n",
				(reg + i), lcd_tcon_reg_read(pdrv, reg + i));
		}
		break;
	case LCD_REG_DBG_EDPHOST_BUS:
		pr_info("dump edp regs:\n");
		for (i = 0; i < num; i++) {
			pr_info("[0x%04x] = 0x%08x\n",
				(reg + i), dptx_reg_read(pdrv, reg + i));
		}
		break;
	case LCD_REG_DBG_EDPDPCD_BUS:
		pr_info("dump edp dpcd regs:\n");
		buf = kcalloc(num, sizeof(unsigned char), GFP_KERNEL);
		if (!buf)
			break;
		ret = dptx_aux_read(pdrv, reg, num, buf);
		if (ret) {
			kfree(buf);
			break;
		}
		for (i = 0; i < num; i++)
			pr_info("[0x%04x] = 0x%02x\n", (reg + i), buf[i]);
		kfree(buf);
		break;
	case LCD_REG_DBG_COMBOPHY_BUS:
		pr_info("dump combo dphy regs:\n");
		for (i = 0; i < num; i++) {
			pr_info("[0x%04x] = 0x%08x\n",
				(reg + i), lcd_combo_dphy_read(pdrv, reg + i));
		}
		break;
	case LCD_REG_DBG_RST_BUS:
		pr_info("dump rst regs:\n");
		for (i = 0; i < num; i++) {
			pr_info("[0x%04x] = 0x%08x\n",
				(reg + i), lcd_reset_read(pdrv, reg + i));
		}
		break;
	default:
		break;
	}
}

static ssize_t lcd_debug_reg_store(struct device *dev, struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	unsigned int bus = LCD_REG_DBG_MAX_BUS;
	unsigned int reg32 = 0, data32 = 0;
	int ret = 0;

	switch (buf[0]) {
	case 'w':
		if (buf[1] == 'v') { /* vcbus */
			ret = sscanf(buf, "wv %x %x", &reg32, &data32);
			bus = LCD_REG_DBG_VC_BUS;
		} else if (buf[1] == 'a') { /* ana */
			ret = sscanf(buf, "wa %x %x", &reg32, &data32);
			bus = LCD_REG_DBG_ANA_BUS;
		} else if (buf[1] == 'c') { /* clk */
			ret = sscanf(buf, "wc %x %x", &reg32, &data32);
			bus = LCD_REG_DBG_CLK_BUS;
		} else if (buf[1] == 'p') { /* periphs */
			ret = sscanf(buf, "wp %x %x", &reg32, &data32);
			bus = LCD_REG_DBG_PERIPHS_BUS;
		} else if (buf[1] == 'm') {
			if (buf[2] == 'h') { /* mipi host */
				ret = sscanf(buf, "wmh %x %x", &reg32, &data32);
				bus = LCD_REG_DBG_MIPIHOST_BUS;
			} else if (buf[2] == 'p') { /* mipi phy */
				ret = sscanf(buf, "wmp %x %x", &reg32, &data32);
				bus = LCD_REG_DBG_MIPIPHY_BUS;
			}
		} else if (buf[1] == 't') { /* tcon */
			ret = sscanf(buf, "wt %x %x", &reg32, &data32);
			bus = LCD_REG_DBG_TCON_BUS;
		} else if (buf[1] == 'e') {
			if (buf[2] == 'h') { /* edp host */
				ret = sscanf(buf, "weh %x %x", &reg32, &data32);
				bus = LCD_REG_DBG_EDPHOST_BUS;
			} else if (buf[2] == 'd') { /* edp dpcd */
				ret = sscanf(buf, "wed %x %x", &reg32, &data32);
				bus = LCD_REG_DBG_EDPDPCD_BUS;
			}
		} else if (buf[1] == 'd') { /* combo dphy */
			ret = sscanf(buf, "wd %x %x", &reg32, &data32);
			bus = LCD_REG_DBG_COMBOPHY_BUS;
		} else if (buf[1] == 'r') { /* rst */
			ret = sscanf(buf, "wr %x %x", &reg32, &data32);
			bus = LCD_REG_DBG_RST_BUS;
		}
		if (ret == 2) {
			lcd_debug_reg_write(pdrv, reg32, data32, bus);
		} else {
			pr_info("invalid data\n");
			return -EINVAL;
		}
		break;
	case 'r':
		if (buf[1] == 'v') { /* vcbus */
			ret = sscanf(buf, "rv %x", &reg32);
			bus = LCD_REG_DBG_VC_BUS;
		} else if (buf[1] == 'a') { /* ana */
			ret = sscanf(buf, "ra %x", &reg32);
			bus = LCD_REG_DBG_ANA_BUS;
		} else if (buf[1] == 'c') { /* clk */
			ret = sscanf(buf, "rc %x", &reg32);
			bus = LCD_REG_DBG_CLK_BUS;
		} else if (buf[1] == 'p') { /* periphs */
			ret = sscanf(buf, "rp %x", &reg32);
			bus = LCD_REG_DBG_PERIPHS_BUS;
		} else if (buf[1] == 'm') {
			if (buf[2] == 'h') { /* mipi host */
				ret = sscanf(buf, "rmh %x", &reg32);
				bus = LCD_REG_DBG_MIPIHOST_BUS;
			} else if (buf[2] == 'p') { /* mipi phy */
				ret = sscanf(buf, "rmp %x", &reg32);
				bus = LCD_REG_DBG_MIPIPHY_BUS;
			}
		} else if (buf[1] == 't') { /* tcon */
			ret = sscanf(buf, "rt %x", &reg32);
			bus = LCD_REG_DBG_TCON_BUS;
		} else if (buf[1] == 'e') {
			if (buf[2] == 'h') { /* edp host */
				ret = sscanf(buf, "reh %x", &reg32);
				bus = LCD_REG_DBG_EDPHOST_BUS;
			} else if (buf[2] == 'd') { /* edp dpcd */
				ret = sscanf(buf, "red %x", &reg32);
				bus = LCD_REG_DBG_EDPDPCD_BUS;
			}
		} else if (buf[1] == 'd') { /* combo dphy */
			ret = sscanf(buf, "rd %x", &reg32);
			bus = LCD_REG_DBG_COMBOPHY_BUS;
		} else if (buf[1] == 'r') { /* rst */
			ret = sscanf(buf, "rr %x", &reg32);
			bus = LCD_REG_DBG_RST_BUS;
		}
		if (ret == 1) {
			lcd_debug_reg_read(pdrv, reg32, bus);
		} else {
			pr_info("invalid data\n");
			return -EINVAL;
		}
		break;
	case 'd':
		if (buf[1] == 'v') { /* vcbus */
			ret = sscanf(buf, "dv %x %d", &reg32, &data32);
			bus = LCD_REG_DBG_VC_BUS;
		} else if (buf[1] == 'a') { /* ana */
			ret = sscanf(buf, "da %x %d", &reg32, &data32);
			bus = LCD_REG_DBG_ANA_BUS;
		} else if (buf[1] == 'c') { /* clk */
			ret = sscanf(buf, "dc %x %d", &reg32, &data32);
			bus = LCD_REG_DBG_CLK_BUS;
		} else if (buf[1] == 'p') { /* periphs */
			ret = sscanf(buf, "dp %x %d", &reg32, &data32);
			bus = LCD_REG_DBG_PERIPHS_BUS;
		} else if (buf[1] == 'm') {
			if (buf[2] == 'h') { /* mipi host */
				ret = sscanf(buf, "dmh %x %d", &reg32, &data32);
				bus = LCD_REG_DBG_MIPIHOST_BUS;
			} else if (buf[2] == 'p') { /* mipi phy */
				ret = sscanf(buf, "dmp %x %d", &reg32, &data32);
				bus = LCD_REG_DBG_MIPIPHY_BUS;
			}
		} else if (buf[1] == 't') { /* tcon */
			ret = sscanf(buf, "dt %x %d", &reg32, &data32);
			bus = LCD_REG_DBG_TCON_BUS;
		} else if (buf[1] == 'e') {
			if (buf[2] == 'h') { /* edp host */
				ret = sscanf(buf, "deh %x %d", &reg32, &data32);
				bus = LCD_REG_DBG_EDPHOST_BUS;
			} else if (buf[2] == 'd') { /* edp dpcd */
				ret = sscanf(buf, "ded %x %d", &reg32, &data32);
				bus = LCD_REG_DBG_EDPDPCD_BUS;
			}
		} else if (buf[1] == 'd') { /* combo dphy */
			ret = sscanf(buf, "dd %x %x", &reg32, &data32);
			bus = LCD_REG_DBG_COMBOPHY_BUS;
		} else if (buf[1] == 'r') { /* rst */
			ret = sscanf(buf, "dr %x %x", &reg32, &data32);
			bus = LCD_REG_DBG_RST_BUS;
		}
		if (ret == 2) {
			lcd_debug_reg_dump(pdrv, reg32, data32, bus);
		} else {
			pr_info("invalid data\n");
			return -EINVAL;
		}
		break;
	default:
		pr_info("wrong command\n");
		break;
	}

	return count;
}

static ssize_t lcd_debug_vlock_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	ssize_t len = 0;

	len = sprintf(buf, "custom vlock attr:\n"
		      "vlock_valid:        %d\n"
		      "vlock_en:           %d\n"
		      "vlock_work_mode:    %d\n"
		      "vlock_pll_m_limit:  %d\n"
		      "vlock_line_limit:   %d\n",
		      pdrv->config.vlock_param[0],
		      pdrv->config.vlock_param[1],
		      pdrv->config.vlock_param[2],
		      pdrv->config.vlock_param[3],
		      pdrv->config.vlock_param[4]);

	return len;
}

#define LCD_DEBUG_DUMP_INFO_BASIC     0
#define LCD_DEBUG_DUMP_INFO_ADV       1
#define LCD_DEBUG_DUMP_INFO_TCON      2
#define LCD_DEBUG_DUMP_INFO_POWER     3
#define LCD_DEBUG_DUMP_REG_CLK        4
#define LCD_DEBUG_DUMP_REG_ENCL       5
#define LCD_DEBUG_DUMP_REG_IF         6
#define LCD_DEBUG_DUMP_REG_PHY        7
#define LCD_DEBUG_DUMP_REG_PINMUX     8
#define LCD_DEBUG_DUMP_OPTICAL        9
#define LCD_DEBUG_DUMP_CLK_PARA       10
static int lcd_debug_dump_state;
static ssize_t lcd_debug_dump_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	char *print_buf;
	int i, len = 0;

	print_buf = kcalloc(PR_BUF_MAX, sizeof(char), GFP_KERNEL);
	if (!print_buf)
		return sprintf(buf, "%s: buf malloc error\n", __func__);

	switch (lcd_debug_dump_state) {
	case LCD_DEBUG_DUMP_INFO_BASIC:
		lcd_info_basic_print(pdrv, print_buf, 0);
		break;
	case LCD_DEBUG_DUMP_INFO_ADV:
		lcd_info_adv_print(pdrv, print_buf, 0);
		break;
	case LCD_DEBUG_DUMP_INFO_TCON:
		lcd_info_tcon_print(pdrv, print_buf, 0);
		break;
	case LCD_DEBUG_DUMP_INFO_POWER:
		lcd_power_step_info_print(pdrv, print_buf, 0);
		break;
	case LCD_DEBUG_DUMP_REG_CLK:
		lcd_reg_clk_print(pdrv, print_buf, 0);
		break;
	case LCD_DEBUG_DUMP_REG_ENCL:
		lcd_reg_encl_print(pdrv, print_buf, 0);
		break;
	case LCD_DEBUG_DUMP_REG_IF:
		lcd_reg_if_print(pdrv, print_buf, 0);
		break;
	case LCD_DEBUG_DUMP_REG_PHY:
		lcd_reg_phy_print(pdrv, print_buf, 0);
		break;
	case LCD_DEBUG_DUMP_REG_PINMUX:
		lcd_reg_pinmux_print(pdrv, print_buf, 0);
		break;
	case LCD_DEBUG_DUMP_OPTICAL:
		lcd_optical_info_print(pdrv, print_buf, 0);
		break;
	case LCD_DEBUG_DUMP_CLK_PARA:
		i = lcd_clk_config_print(pdrv, print_buf, 0);
		lcd_clk_clkmsr_print(pdrv, (print_buf + i), i);
		break;
	default:
		sprintf(print_buf, "%s: invalid command\n", __func__);
		break;
	}
	len = sprintf(buf, "%s\n", print_buf);
	kfree(print_buf);

	return len;
}

static ssize_t lcd_debug_dump_store(struct device *dev, struct device_attribute *attr,
				    const char *buf, size_t count)
{
	char *buf_orig;
	char *parm[47] = {NULL};

	if (!buf)
		return count;
	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig) {
		LCDERR("%s: buf malloc error\n", __func__);
		return count;
	}
	lcd_debug_parse_param(buf_orig, (char **)&parm);

	if (strcmp(parm[0], "info") == 0) {
		lcd_debug_dump_state = LCD_DEBUG_DUMP_INFO_BASIC;
	} else if (strcmp(parm[0], "basic") == 0) {
		lcd_debug_dump_state = LCD_DEBUG_DUMP_INFO_BASIC;
	} else if (strcmp(parm[0], "adv") == 0) {
		lcd_debug_dump_state = LCD_DEBUG_DUMP_INFO_ADV;
	} else if (strcmp(parm[0], "tcon") == 0) {
		lcd_debug_dump_state = LCD_DEBUG_DUMP_INFO_TCON;
	} else if (strcmp(parm[0], "power") == 0) {
		lcd_debug_dump_state = LCD_DEBUG_DUMP_INFO_POWER;
	} else if (strcmp(parm[0], "reg_clk") == 0) {
		lcd_debug_dump_state = LCD_DEBUG_DUMP_REG_CLK;
	} else if (strcmp(parm[0], "reg_encl") == 0) {
		lcd_debug_dump_state = LCD_DEBUG_DUMP_REG_ENCL;
	} else if (strcmp(parm[0], "reg_if") == 0) {
		lcd_debug_dump_state = LCD_DEBUG_DUMP_REG_IF;
	} else if (strcmp(parm[0], "reg_phy") == 0) {
		lcd_debug_dump_state = LCD_DEBUG_DUMP_REG_PHY;
	} else if (strcmp(parm[0], "reg_pinmux") == 0) {
		lcd_debug_dump_state = LCD_DEBUG_DUMP_REG_PINMUX;
	} else if (strcmp(parm[0], "opt") == 0) {
		lcd_debug_dump_state = LCD_DEBUG_DUMP_OPTICAL;
	} else if (strcmp(parm[0], "hdr") == 0) {
		lcd_debug_dump_state = LCD_DEBUG_DUMP_OPTICAL;
	} else if (strcmp(parm[0], "clk") == 0) {
		lcd_debug_dump_state = LCD_DEBUG_DUMP_CLK_PARA;
	} else {
		LCDERR("invalid command\n");
		kfree(buf_orig);
		return -EINVAL;
	}

	kfree(buf_orig);
	return count;
}

static ssize_t lcd_debug_print_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "get debug print flag: 0x%x\n",
		       lcd_debug_print_flag);
}

static ssize_t lcd_debug_print_store(struct device *dev, struct device_attribute *attr,
				     const char *buf, size_t count)
{
	int ret = 0;
	unsigned int temp = 0;

	ret = kstrtouint(buf, 16, &temp);
	if (ret) {
		pr_info("invalid data\n");
		return -EINVAL;
	}
	lcd_debug_print_flag = (unsigned char)temp;
	LCDPR("set debug print flag: 0x%x\n", lcd_debug_print_flag);

	return count;
}

static ssize_t lcd_debug_unmute_count_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);

	return sprintf(buf, "get unmute count test: %d\n",
		       pdrv->unmute_count_test);
}

static ssize_t lcd_debug_unmute_count_store(struct device *dev, struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	int ret = 0;
	unsigned int temp = 0;

	ret = kstrtouint(buf, 16, &temp);
	if (ret) {
		pr_info("invalid data\n");
		return -EINVAL;
	}
	pdrv->unmute_count_test = (unsigned char)temp;
	LCDPR("set unmute count: 0x%x\n", pdrv->unmute_count_test);

	return count;
}

static ssize_t lcd_debug_mute_count_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);

	return sprintf(buf, "get mute count test: %d\n",
		       pdrv->mute_count_test);
}

static ssize_t lcd_debug_mute_count_store(struct device *dev, struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	int ret = 0;
	unsigned int temp = 0;

	ret = kstrtouint(buf, 16, &temp);
	if (ret) {
		pr_info("invalid data\n");
		return -EINVAL;
	}
	pdrv->mute_count_test = (unsigned char)temp;
	LCDPR("set mute count: 0x%x\n", pdrv->mute_count_test);

	return count;
}

static ssize_t lcd_debug_cus_ctrl_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);

	return sprintf(buf, "cus_ctrl:\n"
		"dlg_flag: %d\n"
		"attr_0_para0: %d\n"
		"attr_0_para1: %d\n",
		pdrv->config.cus_ctrl.dlg_flag,
		pdrv->config.cus_ctrl.attr_0_para0,
		pdrv->config.cus_ctrl.attr_0_para1);
}

static ssize_t lcd_debug_vinfo_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	ssize_t len = 0;

	len = sprintf(buf, "lcd vinfo:\n"
		      "    lcd_mode:              %s\n"
		      "    name:                  %s\n"
		      "    mode:                  %d\n"
		      "    frac:                  %d\n"
		      "    width:                 %d\n"
		      "    height:                %d\n"
		      "    field_height:          %d\n"
		      "    aspect_ratio_num:      %d\n"
		      "    aspect_ratio_den:      %d\n"
		      "    sync_duration_num:     %d\n"
		      "    sync_duration_den:     %d\n"
		      "    std_duration:          %d\n"
		      "    screen_real_width:     %d\n"
		      "    screen_real_height:    %d\n"
		      "    htotal:                %d\n"
		      "    vtotal:                %d\n"
		      "    fr_adj_type:           %d\n"
		      "    video_clk:             %d\n"
		      "    viu_color_fmt:         %d\n"
		      "    viu_mux:               0x%x\n\n",
		      lcd_mode_mode_to_str(pdrv->mode),
		      pdrv->vinfo.name,
		      pdrv->vinfo.mode,
		      pdrv->vinfo.frac,
		      pdrv->vinfo.width,
		      pdrv->vinfo.height,
		      pdrv->vinfo.field_height,
		      pdrv->vinfo.aspect_ratio_num,
		      pdrv->vinfo.aspect_ratio_den,
		      pdrv->vinfo.sync_duration_num,
		      pdrv->vinfo.sync_duration_den,
		      pdrv->vinfo.std_duration,
		      pdrv->vinfo.screen_real_width,
		      pdrv->vinfo.screen_real_height,
		      pdrv->vinfo.htotal,
		      pdrv->vinfo.vtotal,
		      pdrv->vinfo.fr_adj_type,
		      pdrv->vinfo.video_clk,
		      pdrv->vinfo.viu_color_fmt,
		      pdrv->vinfo.viu_mux);

	return len;
}

//make sure 30min diff is less than half frame
#define LCD_VS_MSR_ERR_MAX    280   //unit:0.000001

#define LCD_VS_MSR_DUMP       1
#define LCD_VS_MSR_AVG        2
#define LCD_VS_MSR_SNAPSHOT   3
static unsigned int lcd_vs_msr_sel;

static ssize_t lcd_debug_vs_msr_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	ssize_t len = 0, n = PR_BUF_MAX;
	unsigned long long sum = 0, target;
	unsigned int avg, err, i;

	if (!pdrv->vs_msr || !pdrv->vs_msr_rt)
		return sprintf(buf, "vs_msr buffer is NULL\n");

	target = pdrv->config.timing.sync_duration_num;
	target = lcd_do_div(target * 1000000, pdrv->config.timing.sync_duration_den);
	switch (lcd_vs_msr_sel) {
	case LCD_VS_MSR_DUMP:
		for (i = 0; i < pdrv->vs_msr_cnt; i++) {
			if (target >= pdrv->vs_msr[i])
				err = target - pdrv->vs_msr[i];
			else
				err = pdrv->vs_msr[i] - target;
			if (len >= PR_BUF_MAX)
				return len;
			n = PR_BUF_MAX - len;
			len += snprintf(buf + len, n, "[%d]: %d, %d%s\n",
				i, pdrv->vs_msr[i],
				err, (err > pdrv->vs_msr_err_th) ? "(X)" : "");
		}
		break;
	case LCD_VS_MSR_AVG:
		for (i = 0; i < pdrv->vs_msr_cnt; i++)
			sum += pdrv->vs_msr[i];
		avg = lcd_do_div(sum, pdrv->vs_msr_cnt);
		if (target >= avg)
			err = target - avg;
		else
			err = avg - target;
		len = sprintf(buf, "vs_msr avg: %d, err: %d%s\n",
			avg, err, (err > pdrv->vs_msr_err_th) ? "(X)" : "");
		break;
	case LCD_VS_MSR_SNAPSHOT:
		for (i = 0; i < pdrv->vs_msr_i; i++) {
			if (target >= pdrv->vs_msr_rt[i])
				err = target - pdrv->vs_msr_rt[i];
			else
				err = pdrv->vs_msr_rt[i] - target;
			if (len >= PR_BUF_MAX)
				return len;
			n = PR_BUF_MAX - len;
			len += snprintf(buf + len, n, "[%d]: %d, %d%s\n",
				i, pdrv->vs_msr_rt[i],
				err, (err > pdrv->vs_msr_err_th) ? "(X)" : "");
		}
		break;
	default:
		break;
	}
	if (len >= PR_BUF_MAX)
		return len;
	n = PR_BUF_MAX - len;
	len += snprintf(buf + len, n,
		"vs_msr max:%d, min:%d\n"
		"vs_msr en:%d, err_th:%d, i:%d, cnt:%d, cnt_max:%d\n",
		pdrv->vs_msr_max, pdrv->vs_msr_min,
		pdrv->vs_msr_en, pdrv->vs_msr_err_th,
		pdrv->vs_msr_i, pdrv->vs_msr_cnt, pdrv->vs_msr_cnt_max);

	return len;
}

static ssize_t lcd_debug_vs_msr_store(struct device *dev, struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	char *buf_orig;
	char *parm[8] = {NULL};
	unsigned int temp, msr_en;
	int ret;

	if (!buf)
		return count;
	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig) {
		LCDERR("%s: buf malloc error\n", __func__);
		return count;
	}
	lcd_debug_parse_param(buf_orig, (char **)&parm);

	if (strcmp(parm[0], "init") == 0) {
		if (!parm[1]) {
			pr_err("invalid data\n");
			goto lcd_debug_vs_msr_store_next;
		}
		ret = kstrtouint(parm[1], 10, &temp);
		if (ret) {
			pr_err("invalid data\n");
			goto lcd_debug_vs_msr_store_next;
		}
		if (temp) {
			pdrv->vs_msr_cnt_max = temp;
			pdrv->vs_msr = kcalloc(pdrv->vs_msr_cnt_max,
					sizeof(unsigned int), GFP_KERNEL);
			if (!pdrv->vs_msr)
				goto lcd_debug_vs_msr_store_next;
			pdrv->vs_msr_rt = kcalloc(300, sizeof(unsigned int), GFP_KERNEL);
			if (!pdrv->vs_msr_rt)
				goto lcd_debug_vs_msr_store_next;
			pr_info("vs_msr buffer init ok, size: %d\n", pdrv->vs_msr_cnt_max);
		} else {
			if (pdrv->vs_msr_en) {
				pr_err("vs_msr is still enabled, can't release buffer\n");
				goto lcd_debug_vs_msr_store_next;
			}
			pdrv->vs_msr_i = 0;
			pdrv->vs_msr_cnt = 0;
			pdrv->vs_msr_cnt_max = 0;

			kfree(pdrv->vs_msr);
			pdrv->vs_msr = NULL;
			kfree(pdrv->vs_msr_rt);
			pdrv->vs_msr_rt = NULL;
		}
	} else if (strcmp(parm[0], "en") == 0) {
		if (parm[1]) {
			ret = kstrtouint(parm[1], 10, &temp);
			if (ret) {
				pr_err("invalid data\n");
				goto lcd_debug_vs_msr_store_next;
			}
			if (temp) {
				if (!pdrv->vs_msr || !pdrv->vs_msr_rt) {
					pr_err("vs_msr buffer is NULL, can't enable\n");
					goto lcd_debug_vs_msr_store_next;
				}
				pdrv->vs_msr_cnt = 0;
				pdrv->vs_msr_i = 0;
				pdrv->vs_msr_max = 0;
				pdrv->vs_msr_min = 0xffffffff;
				msr_en = 1; //waiting for behind parameters
			} else {
				pdrv->vs_msr_en = 0;
			}
		}
		if (parm[2]) {
			ret = kstrtouint(parm[2], 10, &temp);
			if (ret) {
				pr_err("invalid data\n");
				goto lcd_debug_vs_msr_store_next;
			}
			pdrv->vs_msr_err_th = temp;
		} else {
			if (pdrv->vs_msr_err_th == 0)
				pdrv->vs_msr_err_th = LCD_VS_MSR_ERR_MAX;
		}
		if (msr_en)
			pdrv->vs_msr_en = 1;
		pr_info("vs_msr en:%d, err_th:%d\n"
			"i:%d, cnt:%d, cnt_max:%d\n",
			pdrv->vs_msr_en, pdrv->vs_msr_err_th,
			pdrv->vs_msr_i, pdrv->vs_msr_cnt, pdrv->vs_msr_cnt_max);
	} else if (strcmp(parm[0], "dump") == 0) {
		lcd_vs_msr_sel = LCD_VS_MSR_DUMP;
	} else if (strcmp(parm[0], "avg") == 0) {
		lcd_vs_msr_sel = LCD_VS_MSR_AVG;
	} else if (strcmp(parm[0], "snapshot") == 0) {
		lcd_vs_msr_sel = LCD_VS_MSR_SNAPSHOT;
	} else {
		LCDERR("invalid command\n");
		kfree(buf_orig);
		return -EINVAL;
	}

lcd_debug_vs_msr_store_next:
	kfree(buf_orig);
	return count;
}

static struct device_attribute lcd_debug_attrs[] = {
	__ATTR(help,        0444, lcd_debug_common_help, NULL),
	__ATTR(debug,       0644, lcd_debug_show, lcd_debug_store),
	__ATTR(change,      0644, lcd_debug_change_show, lcd_debug_change_store),
	__ATTR(enable,      0644, lcd_debug_enable_show, lcd_debug_enable_store),
	__ATTR(resume_type, 0644, lcd_debug_resume_show, lcd_debug_resume_store),
	__ATTR(power_on,    0644, lcd_debug_power_show, lcd_debug_power_store),
	__ATTR(power_step,  0644, lcd_debug_power_step_show, lcd_debug_power_step_store),
	__ATTR(frame_rate,  0644, lcd_debug_frame_rate_show, lcd_debug_frame_rate_store),
	__ATTR(fr_flag,     0644, lcd_debug_fr_flag_show, lcd_debug_fr_flag_store),
	__ATTR(ss,          0644, lcd_debug_ss_show, lcd_debug_ss_store),
	__ATTR(clk,         0644, lcd_debug_clk_show, lcd_debug_clk_store),
	__ATTR(test,        0644, lcd_debug_test_show, lcd_debug_test_store),
	__ATTR(mute,        0644, lcd_debug_mute_show, lcd_debug_mute_store),
	__ATTR(mute_count,  0644, lcd_debug_mute_count_show, lcd_debug_mute_count_store),
	__ATTR(unmute_count,  0644, lcd_debug_unmute_count_show, lcd_debug_unmute_count_store),
	__ATTR(prbs,        0644, lcd_debug_prbs_show, lcd_debug_prbs_store),
	__ATTR(reg,         0200, NULL, lcd_debug_reg_store),
	__ATTR(vlock,       0444, lcd_debug_vlock_show, NULL),
	__ATTR(dlg_time,    0444, lcd_dlg_time_show, NULL),
	__ATTR(dump,        0644, lcd_debug_dump_show, lcd_debug_dump_store),
	__ATTR(print,       0644, lcd_debug_print_show, lcd_debug_print_store),
	__ATTR(cus_ctrl,    0444, lcd_debug_cus_ctrl_show, NULL),
	__ATTR(vinfo,       0444, lcd_debug_vinfo_show, NULL),
	__ATTR(vs_msr,      0644, lcd_debug_vs_msr_show, lcd_debug_vs_msr_store)
};

static const char *lcd_rgb_debug_usage_str = {
"Usage:\n"
"    echo <type> <clk_pol> <de_valid> <sync_valid> <rb_swpa> <bit_swap> > rgb ; set rgb config\n"
"data format:\n"
"    <type>          : 0=rgb888\n"
"    <clk_pol>      : 0=negative, 1=positive\n"
"    <de_valid>     : for DE, 0=invalid, 1=valid\n"
"    <sync_valid>   : for hvsync, 0=invalid, 1=valid\n"
"    <rb_swap>      : for R/B port, 0=normal, 1=swap\n"
"    <bit_swap>     : for RGB MSB/LSB, 0=normal, 1=swap\n"
"\n"
};

static const char *lcd_lvds_debug_usage_str = {
"Usage:\n"
"    echo <repack> <dual_port> <pn_swap> <port_swap> <lane_reverse> > lvds ; set lvds config\n"
"data format:\n"
"    <repack>    : 0=JEIDA mode, 1=VESA mode(8bit), 2=VESA mode(10bit)\n"
"    <dual_port> : 0=single port, 1=dual port\n"
"    <pn_swap>   : 0=normal, 1=swap p/n channels\n"
"    <port_swap> : 0=normal, 1=swap A/B port\n"
"	 <lane_reverse> : 0=normal, 1=swap A0-A4/B0-B4\n"
"\n"
"    echo <vswing> <preem> > phy ; set vbyone phy config\n"
"data format:\n"
"    <vswing> : vswing level, support 0~7\n"
"    <preem>  : preemphasis level, support 0~7\n"
"\n"
};

static const char *lcd_vbyone_debug_usage_str = {
"Usage:\n"
"    echo <lane_count> <region_num> <byte_mode> > vbyone ; set vbyone config\n"
"data format:\n"
"    <lane_count> : 4/8/16\n"
"    <region_num> : 1/2\n"
"    <byte_mode>  : 3/4/5\n"
"\n"
"    echo <vswing> <preem> > phy ; set vbyone phy config\n"
"data format:\n"
"    <vswing> : vswing level, support 0~7\n"
"    <preem>  : preemphasis level, support 0~7\n"
"    <byte_mode>  : 3/4/5\n"
"\n"
"    echo intr <state> <en> > vbyone; enable or disable vbyone interrupt\n"
"data format:\n"
"    <state> : 0=temp no use intr, 1=temp use intr. keep effect until reset lcd driver\n"
"    <en>    : 0=disable intr, 1=enable intr\n"
"\n"
"    echo vintr <en> > vbyone; enable or disable vbyone interrupt\n"
"data format:\n"
"    <en>    : 0=disable vsync monitor intr, 1=enable vsync monitor intr\n"
"\n"
"    echo ctrl <ctrl_flag> <power_on_reset_delay> <hpd_data_delay> <cdr_training_hold> > vbyone; set ctrl adjust\n"
"data format:\n"
"    <ctrl_flag>    : bit[0]:power_on_reset_en, bit[1]:hpd_data_delay_en, bit[2]:cdr_training_hold_en\n"
"    others         : unit in ms\n"
"\n"
};

static const char *lcd_mipi_debug_usage_str = {
"Usage:\n"
"    echo <lane_num> <bit_rate_max> <factor> <op_mode_init> <op_mode_disp> <vid_mode_type> <clk_always_hs> <phy_switch> > mipi ; set mpi config\n"
"data format:\n"
"    <lane_num>          : 1/2/3/4\n"
"    <bit_rate_max>      : unit in MHz\n"
"    <factor>:           : special adjust, 0 for default\n"
"    <op_mode_init>      : operation mode for init (0=video mode, 1=command mode)\n"
"    <op_mode_disp>      : operation mode for display (0=video mode, 1=command mode)\n"
"    <vid_mode_type>     : video mode type (0=sync_pulse, 1=sync_event, 2=burst)\n"
"    <clk_always_hs>     : 0=disable, 1=enable\n"
"    <phy_switch>        : 0=auto, 1=standard, 2=slow\n"
"\n"
};

static const char *lcd_mipi_cmd_debug_usage_str = {
"Usage:\n"
"   echo <data_type> <N> <data0> <data1> <data2> ...... <dataN-1> > mpcmd ; send mipi cmd\n"
"   support data_type:\n"
"	DT_SHUT_DOWN            = 0x22\n"
"	DT_TURN_ON              = 0x32\n"
"	DT_GEN_SHORT_WR_0       = 0x03\n"
"	DT_GEN_SHORT_WR_1       = 0x13\n"
"	DT_GEN_SHORT_WR_2       = 0x23\n"
"	DT_DCS_SHORT_WR_0       = 0x05\n"
"	DT_DCS_SHORT_WR_1       = 0x15\n"
"	DT_GEN_LONG_WR          = 0x29\n"
"	DT_DCS_LONG_WR          = 0x39\n"
"\n"
};

static const char *lcd_edp_debug_usage_str = {
"Usage:\n"
"    echo <lane_cnt_max> <link_rate_max> <training_mode> > edp ; set edp config\n"
"data format:\n"
"    <lane_cnt_max>      : 1/2/4\n"
"    <link_rate_max>     : 0=1.62G, 1=2.7G\n"
"    <training_mode>:    : 0=fast link training\n"
"\n"
};

static const char *lcd_mlvds_debug_usage_str = {
"Usage:\n"
"    echo <channel_num> <channel_sel0> <channel_sel1> <clk_phase> <pn_swap> <bit_swap> > minilvds ; set minilvds config\n"
"data format:\n"
"    <channel_sel> : minilvds 8 channels mapping in tx 10 channels\n"
"    <clk_phase>   : bit[14:13]=clk01_pi_sel, bit[12]=data byass buffer, bit[11:8]=pi2, bit[7:4]=pi1, bit[3:0]=pi0\n"
"    <pn_swap>     : 0=normal, 1=swap p/n channels\n"
"    <bit_swap>    : 0=normal, 1=swap bit LSB/MSB\n"
"\n"
};

static const char *lcd_p2p_debug_usage_str = {
"Usage:\n"
"    echo <p2p_type> <lane_num> <channel_sel0> <channel_sel1> <pn_swap> <bit_swap> > p2p ; set p2p config\n"
"data format:\n"
"    <p2p_type>    : 0x0=ceds, 0x1=cmpi, 0x2=isp, 0x3=epi,\n"
"                    0x10=chpi, 0x11=cspi, 0x12=usit\n"
"    <channel_sel> : 12 channels mapping\n"
"    <pn_swap>     : 0=normal, 1=swap p/n channels\n"
"    <bit_swap>    : 0=normal, 1=swap bit LSB/MSB\n"
"\n"
};

static ssize_t lcd_rgb_debug_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int len = 0;

	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	struct rgb_config_s *rgb_conf;

	rgb_conf = &pdrv->config.control.rgb_cfg;

	len += sprintf(buf + len,
		"rgb config: type=%d, clk_pol=%d, de_valid=%d, sync_valid=%d,",
		rgb_conf->type, rgb_conf->clk_pol,
		rgb_conf->de_valid, rgb_conf->sync_valid);
	len += sprintf(buf + len, "rb_swap=%d, bit_swap=%d\n\n",
		rgb_conf->rb_swap, rgb_conf->bit_swap);
	len += sprintf(buf + len, "%s\n", lcd_rgb_debug_usage_str);

	return len;
}

static ssize_t lcd_bt_debug_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	struct bt_config_s *bt_conf;
	int len = 0;

	bt_conf = &pdrv->config.control.bt_cfg;

	len += sprintf(buf + len,
		"bt config: clk_phase=%d, field_type=%d, mode_422=%d,",
		bt_conf->clk_phase,
		bt_conf->field_type,
		bt_conf->mode_422);
	len += sprintf(buf + len, "yc_swap=%d, cbcr_swap=%d\n\n",
		bt_conf->yc_swap,
		bt_conf->cbcr_swap);

	return len;
}

static ssize_t lcd_lvds_debug_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int len = 0;
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	struct lvds_config_s *lvds_conf;

	lvds_conf = &pdrv->config.control.lvds_cfg;

	len +=
	sprintf(buf + len, "lvds config: repack=%d, dual_port=%d,",
		lvds_conf->lvds_repack, lvds_conf->dual_port);
	len +=
	sprintf(buf + len, "pn_swap=%d, port_swap=%d, lane_reverse=%d\n\n",
		lvds_conf->pn_swap, lvds_conf->port_swap,
		lvds_conf->lane_reverse);
	len += sprintf(buf + len, "%s\n", lcd_lvds_debug_usage_str);

	return len;
}

static ssize_t lcd_vx1_debug_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int len = 0;
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	struct vbyone_config_s *vx1_conf;

	vx1_conf = &pdrv->config.control.vbyone_cfg;

	len += sprintf(buf + len, "vbyone config: lane_count=%d,",
		vx1_conf->lane_count);
	len += sprintf(buf + len, "region_num=%d, byte_mode=%d\n\n",
		vx1_conf->region_num, vx1_conf->byte_mode);
	len += sprintf(buf + len, "%s\n", lcd_vbyone_debug_usage_str);

	return len;
}

static ssize_t lcd_mipi_debug_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int len = 0;
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	struct dsi_config_s *dsi_conf;

	dsi_conf = &pdrv->config.control.mipi_cfg;

	len += sprintf(buf + len, "mipi_dsi config: lane_num=%d, ",
		dsi_conf->lane_num);
	len += sprintf(buf + len, "bit_rate_max=%dMhz, factor_numerator=%d, ",
		dsi_conf->bit_rate_max, dsi_conf->factor_numerator);
	len += sprintf(buf + len,
		"operation_mode_init=%d, operation_mode_display=%d, ",
		dsi_conf->operation_mode_init,
		dsi_conf->operation_mode_display);
	len += sprintf(buf + len,
		"video_mode_type=%d, clk_always_hs=%d, phy_switch=%d\n\n",
		dsi_conf->video_mode_type, dsi_conf->clk_always_hs,
		dsi_conf->phy_switch);
	len += sprintf(buf + len, "%s\n", lcd_mipi_debug_usage_str);

	return len;
}

static ssize_t lcd_edp_debug_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	struct edp_config_s *edp_conf;
	int len = 0;

	edp_conf = &pdrv->config.control.edp_cfg;

	len += sprintf(buf + len, "edp config: max_lane_count=%d, ",
		edp_conf->max_lane_count);
	len += sprintf(buf + len, "max_link_rate=%dMhz, training_mode=%d, ",
		edp_conf->max_link_rate, edp_conf->training_mode);
	len += sprintf(buf + len, "%s\n", lcd_edp_debug_usage_str);

	return len;
}

static ssize_t lcd_mlvds_debug_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	int len = 0;
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	struct mlvds_config_s *mlvds_conf;

	mlvds_conf = &pdrv->config.control.mlvds_cfg;

	len +=
	sprintf(buf + len, "minilvds config: channel_num=%d, ",
		mlvds_conf->channel_num);
	len +=
	sprintf(buf + len, "channel_sel0=0x%08x, channel_sel1=0x%08x, ",
		mlvds_conf->channel_sel0, mlvds_conf->channel_sel1);
	len +=
	sprintf(buf + len, "clk_phase=0x%04x, pn_swap=%d, bit_swap=%d\n\n",
		mlvds_conf->clk_phase,
		mlvds_conf->pn_swap, mlvds_conf->bit_swap);
	len += sprintf(buf + len, "%s\n", lcd_mlvds_debug_usage_str);

	return len;
}

static ssize_t lcd_p2p_debug_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	int len = 0;
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	struct p2p_config_s *p2p_conf;

	p2p_conf = &pdrv->config.control.p2p_cfg;

	len += sprintf(buf + len, "p2p config: p2p_type=0x%x, lane_num=%d, ",
		p2p_conf->p2p_type, p2p_conf->lane_num);
	len += sprintf(buf + len, "channel_sel0=0x%08x, channel_sel1=0x%08x, ",
		p2p_conf->channel_sel0, p2p_conf->channel_sel1);
	len += sprintf(buf + len, "pn_swap=%d, bit_swap=%d\n\n",
		p2p_conf->pn_swap, p2p_conf->bit_swap);
	len += sprintf(buf + len, "%s\n", lcd_p2p_debug_usage_str);

	return len;
}

static ssize_t lcd_rgb_debug_store(struct device *dev, struct device_attribute *attr,
				   const char *buf, size_t count)
{
	int ret = 0;
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	struct rgb_config_s *rgb_conf;
	unsigned int temp[6];

	rgb_conf = &pdrv->config.control.rgb_cfg;
	ret = sscanf(buf, "%d %d %d %d %d %d",
		     &temp[0], &temp[1], &temp[2], &temp[3], &temp[4], &temp[5]);
	if (ret == 5) {
		pr_info("set rgb config:\n"
		"type=%d, clk_pol=%d, de_valid=%d, sync_valid=%d, rb_swap=%d, bit_swap=%d\n",
			temp[0], temp[1], temp[2], temp[3], temp[4], temp[5]);
		rgb_conf->type = temp[0];
		rgb_conf->clk_pol = temp[1];
		rgb_conf->de_valid = temp[2];
		rgb_conf->sync_valid = temp[3];
		rgb_conf->rb_swap = temp[4];
		rgb_conf->bit_swap = temp[5];
		lcd_debug_config_update(pdrv);
	} else {
		pr_info("invalid data\n");
		return -EINVAL;
	}

	return count;
}

static ssize_t lcd_bt_debug_store(struct device *dev, struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	struct bt_config_s *bt_conf;
	unsigned int temp[5];
	int ret = 0;

	bt_conf = &pdrv->config.control.bt_cfg;
	ret = sscanf(buf, "%d %d %d %d %d",
		     &temp[0], &temp[1], &temp[2], &temp[3], &temp[4]);
	if (ret == 5) {
		pr_info("set bt config:\n"
			"clk_phase=%d, field_type=%d, mode_422=%d\n"
			"yc_swap=%d, cbcr_swap=%d\n",
			temp[0], temp[1], temp[2], temp[3], temp[4]);
		bt_conf->clk_phase = temp[0];
		bt_conf->field_type = temp[1];
		bt_conf->mode_422 = temp[2];
		bt_conf->yc_swap = temp[3];
		bt_conf->cbcr_swap = temp[4];
		lcd_debug_config_update(pdrv);
	} else {
		pr_info("invalid data\n");
		return -EINVAL;
	}

	return count;
}

static ssize_t lcd_lvds_debug_store(struct device *dev, struct device_attribute *attr,
				    const char *buf, size_t count)
{
	int ret = 0;
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	struct lvds_config_s *lvds_conf;

	lvds_conf = &pdrv->config.control.lvds_cfg;
	ret = sscanf(buf, "%d %d %d %d %d",
		     &lvds_conf->lvds_repack, &lvds_conf->dual_port,
		     &lvds_conf->pn_swap, &lvds_conf->port_swap,
		     &lvds_conf->lane_reverse);
	if (ret == 5 || ret == 4) {
		pr_info("set lvds config:\n"
			"repack=%d, dual_port=%d, pn_swap=%d, port_swap=%d, lane_reverse=%d\n",
			lvds_conf->lvds_repack, lvds_conf->dual_port,
			lvds_conf->pn_swap, lvds_conf->port_swap,
			lvds_conf->lane_reverse);
		lcd_debug_config_update(pdrv);
	} else {
		pr_info("invalid data\n");
		return -EINVAL;
	}

	return count;
}

static ssize_t lcd_vx1_debug_store(struct device *dev, struct device_attribute *attr,
				   const char *buf, size_t count)
{
	int ret = 0;
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	struct vbyone_config_s *vx1_conf;
	int val[5];
	unsigned int offset;

	offset = pdrv->data->offset_venc_if[pdrv->index];

	vx1_conf = &pdrv->config.control.vbyone_cfg;
	if (buf[0] == 'i') { /* intr */
		ret = sscanf(buf, "intr %d %d", &val[0], &val[1]);
		if (ret == 1) {
			pr_info("set vbyone interrupt enable: %d\n", val[0]);
			vx1_conf->intr_state = val[0];
			lcd_vbyone_interrupt_enable(pdrv, vx1_conf->intr_state);
		} else if (ret == 2) {
			pr_info("set vbyone interrupt enable: %d %d\n",
				val[0], val[1]);
			vx1_conf->intr_state = val[0];
			vx1_conf->intr_en = val[1];
			lcd_vbyone_interrupt_enable(pdrv, vx1_conf->intr_state);
		} else {
			pr_info("vx1_intr_enable: %d %d\n",
				vx1_conf->intr_state, vx1_conf->intr_en);
			return -EINVAL;
		}
	} else if (buf[0] == 'v') { /* vintr */
		ret = sscanf(buf, "vintr %d", &val[0]);
		if (ret == 1) {
			pr_info("set vbyone vsync interrupt enable: %d\n",
				val[0]);
			vx1_conf->vsync_intr_en = val[0];
			lcd_vbyone_interrupt_enable(pdrv, vx1_conf->intr_state);
		} else {
			pr_info("vx1_vsync_intr_enable: %d\n",
				vx1_conf->vsync_intr_en);
			return -EINVAL;
		}
	} else if (buf[0] == 'c') {
		if (buf[1] == 't') { /* ctrl */
			ret = sscanf(buf, "ctrl %x %d %d %d",
				     &val[0], &val[1], &val[2], &val[3]);
			if (ret == 4) {
				pr_info("set vbyone ctrl_flag: 0x%x\n", val[0]);
				pr_info("power_on_reset_delay: %dms\n", val[1]);
				pr_info("hpd_data_delay: %dms\n", val[2]);
				pr_info("cdr_training_hold: %dms\n", val[3]);
				vx1_conf->ctrl_flag = val[0];
				vx1_conf->power_on_reset_delay = val[1];
				vx1_conf->hpd_data_delay = val[2];
				vx1_conf->cdr_training_hold = val[3];
				lcd_debug_config_update(pdrv);
			} else {
				pr_info("vbyone ctrl_flag: 0x%x\n",
					vx1_conf->ctrl_flag);
				pr_info("power_on_reset_delay: %dms\n",
					vx1_conf->power_on_reset_delay);
				pr_info("hpd_data_delay: %dms\n",
					vx1_conf->hpd_data_delay);
				pr_info("cdr_training_hold: %dms\n",
					vx1_conf->cdr_training_hold);
				return -EINVAL;
			}
		} else if (buf[1] == 'd') { /* cdr */
			lcd_vbyone_debug_cdr(pdrv);
		}
	} else if (buf[0] == 'f') { /* filter */
		ret = sscanf(buf, "filter %x %x", &val[0], &val[1]);
		if (ret == 2) {
			pr_info("set vbyone hw_filter_time: 0x%x, hw_filter_cnt: 0x%x\n",
				val[0], val[1]);
			vx1_conf->hw_filter_time = val[0];
			vx1_conf->hw_filter_cnt = val[1];
			lcd_debug_config_update(pdrv);
		} else {
			pr_info("vbyone hw_filter_time: 0x%x, hw_filter_cnt: 0x%x\n",
				vx1_conf->hw_filter_time,
				vx1_conf->hw_filter_cnt);
			return -EINVAL;
		}
	} else if (buf[0] == 'r') { /* rst */
		lcd_vbyone_debug_reset(pdrv);
	} else if (buf[0] == 'l') { /* lock */
		lcd_vbyone_debug_lock(pdrv);
	} else {
		ret = sscanf(buf, "%d %d %d", &vx1_conf->lane_count,
			     &vx1_conf->region_num, &vx1_conf->byte_mode);
		if (ret == 3) {
			pr_info("set vbyone config:\n"
				"lane_count=%d, region_num=%d, byte_mode=%d\n",
				vx1_conf->lane_count, vx1_conf->region_num,
				vx1_conf->byte_mode);
			lcd_debug_config_update(pdrv);
		} else {
			pr_info("invalid data\n");
			return -EINVAL;
		}
	}

	return count;
}

static ssize_t lcd_mipi_debug_store(struct device *dev, struct device_attribute *attr,
				    const char *buf, size_t count)
{
	int ret = 0;
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	struct dsi_config_s *dsi_conf;
	int val[8];

	dsi_conf = &pdrv->config.control.mipi_cfg;
	ret = sscanf(buf, "%d %d %d %d %d %d %d %d",
		     &val[0], &val[1], &val[2], &val[3],
		     &val[4], &val[5], &val[6], &val[7]);
	if (ret >= 2) {
		dsi_conf->lane_num = (unsigned char)val[0];
		dsi_conf->bit_rate_max = val[1];
		dsi_conf->factor_numerator = val[2];
		dsi_conf->operation_mode_init = (unsigned char)val[3];
		dsi_conf->operation_mode_display = (unsigned char)val[4];
		dsi_conf->video_mode_type = (unsigned char)val[5];
		dsi_conf->clk_always_hs = (unsigned char)val[6];
		dsi_conf->phy_switch = (unsigned char)val[7];
		pr_info("set mipi_dsi config:\n"
			"lane_num=%d, bit_rate_max=%dMhz, factor_numerator=%d\n"
			"operation_mode_init=%d, operation_mode_display=%d\n"
			"video_mode_type=%d\n"
			"clk_always_hs=%d, phy_switch=%d\n\n",
			dsi_conf->lane_num,
			dsi_conf->bit_rate_max,
			dsi_conf->factor_numerator,
			dsi_conf->operation_mode_init,
			dsi_conf->operation_mode_display,
			dsi_conf->video_mode_type,
			dsi_conf->clk_always_hs,
			dsi_conf->phy_switch);
		lcd_debug_config_update(pdrv);
	} else {
		pr_info("invalid data\n");
		return -EINVAL;
	}

	return count;
}

static ssize_t lcd_edp_debug_store(struct device *dev, struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	struct edp_config_s *edp_conf;
	unsigned int val[3];
	unsigned char cmd[20];
	int ret = 0;

	edp_conf = &pdrv->config.control.edp_cfg;
	ret = sscanf(buf, "%d %d %d", &val[0], &val[1], &val[2]);
	if (ret == 3) {
		edp_conf->max_lane_count = (unsigned char)val[0];
		edp_conf->max_link_rate = (unsigned char)val[1];
		edp_conf->training_mode = (unsigned char)val[2];
		pr_info("set edp config:\n"
			"lane_num=%d, bit_rate_max=%dMhz, factor_numerator=%d\n\n",
			edp_conf->max_lane_count,
			edp_conf->max_link_rate,
			edp_conf->training_mode);
		lcd_debug_config_update(pdrv);
		return count;
	}
	if (count > 19) {
		LCDERR("eDP debug cmd error\n");
		return count;
	}

	ret = sscanf(buf, "%s %d", cmd, &val[0]);
#ifdef CONFIG_AMLOGIC_LCD_TABLET
	edp_debug_test(pdrv, cmd, val[0]);
#endif
	return count;
}

static ssize_t lcd_edp_dpcd_debug_store(struct device *dev, struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
#ifdef CONFIG_AMLOGIC_LCD_TABLET
	dptx_DPCD_dump(pdrv);
#endif
	return count;
}

static ssize_t lcd_edp_edid_debug_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	unsigned char len;
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
#ifdef CONFIG_AMLOGIC_LCD_TABLET
	dptx_EDID_dump(pdrv);
#endif
	len = sprintf(buf, "edid_en: %d\n", pdrv->config.control.edp_cfg.edid_en);
	return len;
}

static ssize_t lcd_mlvds_debug_store(struct device *dev, struct device_attribute *attr,
				     const char *buf, size_t count)
{
	int ret = 0;
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	struct mlvds_config_s *mlvds_conf;

	mlvds_conf = &pdrv->config.control.mlvds_cfg;
	ret = sscanf(buf, "%d %x %x %x %d %d",
		     &mlvds_conf->channel_num,
		     &mlvds_conf->channel_sel0, &mlvds_conf->channel_sel1,
		     &mlvds_conf->clk_phase,
		     &mlvds_conf->pn_swap, &mlvds_conf->bit_swap);
	if (ret == 6) {
		pr_info("set minilvds config:\n"
			"channel_num=%d,\n"
			"channel_sel0=0x%08x, channel_sel1=0x%08x,\n"
			"clk_phase=0x%04x,\n"
			"pn_swap=%d, bit_swap=%d\n",
			mlvds_conf->channel_num,
			mlvds_conf->channel_sel0, mlvds_conf->channel_sel1,
			mlvds_conf->clk_phase,
			mlvds_conf->pn_swap, mlvds_conf->bit_swap);
		lcd_debug_config_update(pdrv);
	} else {
		pr_info("invalid data\n");
		return -EINVAL;
	}

	return count;
}

static ssize_t lcd_p2p_debug_store(struct device *dev, struct device_attribute *attr,
				   const char *buf, size_t count)
{
	int ret = 0;
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	struct p2p_config_s *p2p_conf;

	p2p_conf = &pdrv->config.control.p2p_cfg;
	ret = sscanf(buf, "%x %d %x %x %d %d",
		     &p2p_conf->p2p_type, &p2p_conf->lane_num,
		     &p2p_conf->channel_sel0, &p2p_conf->channel_sel1,
		     &p2p_conf->pn_swap, &p2p_conf->bit_swap);
	if (ret == 6) {
		pr_info("set p2p config:\n"
			"p2p_type=0x%x, lane_num=%d,\n"
			"channel_sel0=0x%08x, channel_sel1=0x%08x,\n"
			"pn_swap=%d, bit_swap=%d\n",
			p2p_conf->p2p_type, p2p_conf->lane_num,
			p2p_conf->channel_sel0, p2p_conf->channel_sel1,
			p2p_conf->pn_swap, p2p_conf->bit_swap);
		lcd_debug_config_update(pdrv);
	} else {
		pr_info("invalid data\n");
		return -EINVAL;
	}

	return count;
}

static ssize_t lcd_phy_debug_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	struct phy_config_s *phy_cfg;
	ssize_t len = 0;
	int i = 0;

	phy_cfg = &pdrv->config.phy_cfg;
	len = sprintf(buf, "vswing_level=0x%x\n"
					"ext_pullup=%d\n"
					"preem_level=0x%x\n"
					"vcm=0x%x\n"
					"odt=0x%x\n"
					"ref_bias=0x%x\n",
		      phy_cfg->vswing_level, phy_cfg->ext_pullup, phy_cfg->preem_level,
		      phy_cfg->vcm, phy_cfg->odt, phy_cfg->ref_bias);
	for (i = 0; i < phy_cfg->lane_num; i++) {
		len += sprintf(buf + len, "lane[%d] amp=0x%x, preem=0x%x\n",
		      i, phy_cfg->lane[i].amp, phy_cfg->lane[i].preem);
	}

	return len;
}

static ssize_t lcd_phy_debug_store(struct device *dev, struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	union lcd_ctrl_config_u *pctrl = &pdrv->config.control;
	struct phy_config_s *phy_cfg;
	unsigned int para[4];
	int ret = 0;
	int i = 0;

	phy_cfg = &pdrv->config.phy_cfg;
	if (buf[0] == 'l') {
		ret = sscanf(buf, "lane %d %x %x", &para[0], &para[1], &para[2]);
		if (ret == 3) {
			if (para[0] >= 12) {
				pr_info("%s: invalid lane num %d\n",
					__func__, para[0]);
				return -EINVAL;
			}
			phy_cfg->lane[para[0]].amp = para[1];
			phy_cfg->lane[para[0]].preem = para[2];
			pr_info("%s: update lane[%d] amp=0x%x, preem=0x%x\n",
				__func__, para[0], para[1], para[2]);
		} else {
			pr_info("invalid data\n");
			return -EINVAL;
		}
	} else if (buf[0] == 'v') {
		if (buf[1] == 'c')  {//vcm
			ret = sscanf(buf, "vcm %x", &para[0]);
			if (ret == 1) {
				phy_cfg->vcm = para[0];
				phy_cfg->flag |= (1 << 1);
				pr_info("%s: update vcm_value=0x%x\n",
					__func__, para[0]);
			} else {
				pr_info("invalid data\n");
				return -EINVAL;
			}
		} else {
			ret = sscanf(buf, "vswing %x", &para[0]);
			if (ret == 1) {
				phy_cfg->vswing = para[0];
				pr_info("%s: update vswing=0x%x\n", __func__, para[0]);
			} else {
				pr_info("invalid data\n");
				return -EINVAL;
			}
		}
	} else if (buf[0] == 'o') {
		ret = sscanf(buf, "odt %x", &para[0]);
		if (ret == 1) {
			phy_cfg->odt = para[0];
			phy_cfg->flag |= (1 << 3);
			pr_info("%s: update odt=0x%x\n",
				__func__, para[0]);
		} else {
			pr_info("invalid data\n");
			return -EINVAL;
		}
	} else if (buf[0] == 'r') {
		ret = sscanf(buf, "ref_bias %x", &para[0]);
		if (ret == 1) {
			phy_cfg->ref_bias = para[0];
			phy_cfg->flag |= (1 << 2);
			pr_info("%s: update ref_bias=0x%x\n",
				__func__, para[0]);
		} else {
			pr_info("invalid data\n");
			return -EINVAL;
		}
	} else {
		ret = sscanf(buf, "%x %x", &para[0], &para[1]);
		if (ret == 2) {
			switch (pdrv->config.basic.lcd_type) {
			case LCD_LVDS:
				pctrl->lvds_cfg.phy_vswing = para[0];
				pctrl->lvds_cfg.phy_preem = para[1];
				break;
			case LCD_VBYONE:
				pctrl->vbyone_cfg.phy_vswing = para[0];
				pctrl->vbyone_cfg.phy_preem = para[1];
				break;
			case LCD_MLVDS:
				pctrl->mlvds_cfg.phy_vswing = para[0];
				pctrl->mlvds_cfg.phy_preem = para[1];
				break;
			case LCD_P2P:
				pctrl->p2p_cfg.phy_vswing = para[0];
				pctrl->p2p_cfg.phy_preem = para[1];
				break;
			case LCD_EDP:
				pctrl->edp_cfg.phy_vswing_preset = para[0];
				pctrl->edp_cfg.phy_preem_preset = para[1];
				break;
			default:
				LCDERR("%s: not support lcd_type: %s\n",
				       __func__,
				       lcd_type_type_to_str(pdrv->config.basic.lcd_type));
				return -EINVAL;
			}
			phy_cfg->vswing_level = para[0] & 0xf;
			phy_cfg->ext_pullup = (para[0] >> 4) & 0x3;
			phy_cfg->preem_level = para[1];
			phy_cfg->vswing =
				lcd_phy_vswing_level_to_value(pdrv, phy_cfg->vswing_level);
			para[2] = lcd_phy_preem_level_to_value(pdrv, phy_cfg->preem_level);
			for (i = 0; i < phy_cfg->lane_num; i++)
				phy_cfg->lane[i].preem = para[2];
			pr_info("%s: update vswing_level=0x%x, preem_level=0x%x\n",
				__func__, para[0], para[1]);
		} else {
			pr_info("invalid data\n");
			return -EINVAL;
		}
	}
	if (pdrv->status & LCD_STATUS_IF_ON)
		lcd_phy_set(pdrv, LCD_PHY_ON);

	return count;
}

static ssize_t lcd_vx1_status_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	unsigned int offset;

	offset = pdrv->data->offset_venc_if[pdrv->index];
	return sprintf(buf, "vbyone status: lockn = %d hpdn = %d\n",
		       ((lcd_vcbus_read(VBO_STATUS_L + offset) >> 7) & 0x1),
		       ((lcd_vcbus_read(VBO_STATUS_L + offset) >> 6) & 0x1));
}

static ssize_t lcd_mipi_cmd_debug_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", lcd_mipi_cmd_debug_usage_str);
}

static ssize_t lcd_mipi_cmd_debug_store(struct device *dev, struct device_attribute *attr,
					const char *buf, size_t count)
{
#ifdef CONFIG_AMLOGIC_LCD_TABLET
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
#endif
	unsigned char *cmd_table = NULL;
	unsigned int para[24];
	int ret = 0;
	int i;

	ret = sscanf(buf,
		"%x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x",
		&para[0], &para[1], &para[2], &para[3], &para[4], &para[5],
		&para[6], &para[7], &para[8], &para[9], &para[10], &para[11],
		&para[12], &para[13], &para[14], &para[15], &para[16],
		&para[17], &para[18], &para[19], &para[20], &para[21],
		&para[22], &para[23]);

	if (ret < 2) {
		pr_info("invalid mipi cmd\n");
		return count;
	}
	if (ret < (2 + para[1])) {
		pr_info("invalid data num\n");
		return count;
	}

	cmd_table = kcalloc((2 + para[1] + 2), sizeof(unsigned char), GFP_KERNEL);
	if (!cmd_table) {
		pr_err("error for mipi cmd\n");
		return -ENOMEM;
	}
	for (i = 0; i < (2 + para[1]); i++)
		cmd_table[i] = (unsigned char)para[i];
	cmd_table[2 + para[1]] = 0xff;
	cmd_table[2 + para[1] + 1] = 0xff;

#ifdef CONFIG_AMLOGIC_LCD_TABLET
	dsi_write_cmd(pdrv, cmd_table);
#endif

	kfree(cmd_table);

	return count;
}

#define MIPI_RD_RET_CODE_MAX    5
static char *mipi_read_ret_code_table[] = {
	"success",
	"read null",
	"read error",
	"read back cnt is wrong",
	"timeout",
	"unknown error",
};

static struct dsi_read_s dread = {
	.flag = 0,
	.reg = 0xff,
	.cnt = 0,
	.value = NULL,
	.ret_code = 4,

	.line_start = 0x1fff,
	.line_end = 0x1fff,
};

static ssize_t lcd_mipi_read_debug_show(struct device *dev,
					struct device_attribute *attr, char *buf)
{
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	unsigned int i = 0, len;

	if ((pdrv->status & LCD_STATUS_IF_ON) == 0)
		return sprintf(buf, "error: panel is disabled\n");

	if (dread.reg == 0xff)
		return sprintf(buf, "error: reg address is invalid\n");
	if (dread.cnt == 0)
		return sprintf(buf, "error: read count is invalid\n");

	if (dread.cnt > DSI_READ_CNT_MAX)
		return sprintf(buf, "error: mipi read cnt is out of support\n");

	dread.line_start = 0x1fff;
	dread.line_end = 0x1fff;
	dread.ret_code = 4;

#ifdef CONFIG_AMLOGIC_LCD_TABLET
	dread.value = kcalloc(dread.cnt, sizeof(unsigned char), GFP_KERNEL);
	if (!dread.value)
		return sprintf(buf, "error: mipi read return value is null\n");
	if (pdrv->config.control.mipi_cfg.current_mode == 0) {
		dread.flag = 1;
		while (i++ < 5000) {
			if (dread.flag == 0)
				break;
			lcd_delay_us(20);
		}
	} else {
		lcd_mipi_test_read(pdrv, &dread);
	}
#endif

	if (dread.ret_code) {
		kfree(dread.value);
		dread.value = NULL;
		dread.ret_code = (dread.ret_code >= MIPI_RD_RET_CODE_MAX) ?
					MIPI_RD_RET_CODE_MAX : dread.ret_code;
		return sprintf(buf, "read error: %s(%d)\n",
			mipi_read_ret_code_table[dread.ret_code],
			dread.ret_code);
	}

	len = sprintf(buf, "read reg 0x%02x: ", dread.reg);
	for (i = 0; i < dread.cnt; i++) {
		if (i == 0)
			len += sprintf(buf + len, "0x%02x", dread.value[i]);
		else
			len += sprintf(buf + len, ",0x%02x", dread.value[i]);
	}

	len += sprintf(buf + len, "\nread line start=%d, end=%d\n",
		dread.line_start, dread.line_end);

	kfree(dread.value);
	dread.value = NULL;
	return len;
}

static ssize_t lcd_mipi_read_debug_store(struct device *dev, struct device_attribute *attr,
					 const char *buf, size_t count)
{
	unsigned int para[2];
	int ret = 0;

	ret = sscanf(buf, "%x %d", &para[0], &para[1]);
	if (ret < 2) {
		dread.reg = 0xff;
		dread.cnt = 0;
		pr_info("invalid data\n");
		return count;
	}
	dread.reg = (unsigned char)para[0];
	dread.cnt = (unsigned char)para[1];
	if (dread.cnt > DSI_READ_CNT_MAX) {
		LCDERR("mipi read cnt is out of support\n");
		return count;
	}

	pr_info("set mipi read reg: 0x%02x, cnt: %d\n", dread.reg, dread.cnt);

	return count;
}

static ssize_t lcd_mipi_state_debug_show(struct device *dev,
					 struct device_attribute *attr, char *buf)
{
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	unsigned int state_save, offset;

	offset = pdrv->data->offset_venc_if[pdrv->index];
	state_save = lcd_vcbus_getb(L_VCOM_VS_ADDR + offset, 12, 1);

	return sprintf(buf, "state: %d, check_en: %d, state_save: %d\n",
		pdrv->config.control.mipi_cfg.check_state,
		pdrv->config.control.mipi_cfg.check_en,
		state_save);
}

static ssize_t lcd_mipi_mode_debug_show(struct device *dev,
					struct device_attribute *attr, char *buf)
{
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	unsigned char mode;

	if ((pdrv->status & LCD_STATUS_IF_ON) == 0)
		return sprintf(buf, "error: panel is disabled\n");

	mode = pdrv->config.control.mipi_cfg.current_mode;
	return sprintf(buf, "current mipi-dsi operation mode: %s(%d)\n",
		(mode ? "command" : "video"), mode);
}

static ssize_t lcd_mipi_mode_debug_store(struct device *dev, struct device_attribute *attr,
					 const char *buf, size_t count)
{
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	unsigned int temp;
	unsigned char mode;
	int ret = 0;

	if ((pdrv->status & LCD_STATUS_IF_ON) == 0) {
		LCDERR("panel is disabled\n");
		return count;
	}

	ret = kstrtouint(buf, 10, &temp);
	if (ret) {
		pr_info("invalid data\n");
		return -EINVAL;
	}
	mode = (unsigned char)temp;
#ifdef CONFIG_AMLOGIC_LCD_TABLET
	dsi_set_operation_mode(pdrv, mode);
#endif

	return count;
}

static struct device_attribute lcd_debug_attrs_rgb[] = {
	__ATTR(rgb,    0644, lcd_rgb_debug_show, lcd_rgb_debug_store),
	__ATTR(null,   0644, NULL, NULL)
};

static struct device_attribute lcd_debug_attrs_bt[] = {
	__ATTR(bt,     0644, lcd_bt_debug_show, lcd_bt_debug_store),
	__ATTR(null,   0644, NULL, NULL)
};

static struct device_attribute lcd_debug_attrs_lvds[] = {
	__ATTR(lvds,   0644, lcd_lvds_debug_show, lcd_lvds_debug_store),
	__ATTR(phy,    0644, lcd_phy_debug_show, lcd_phy_debug_store),
	__ATTR(null,   0644, NULL, NULL)
};

static struct device_attribute lcd_debug_attrs_vbyone[] = {
	__ATTR(vbyone, 0644, lcd_vx1_debug_show, lcd_vx1_debug_store),
	__ATTR(phy,    0644, lcd_phy_debug_show, lcd_phy_debug_store),
	__ATTR(status, 0444, lcd_vx1_status_show, NULL),
	__ATTR(null,   0644, NULL, NULL)
};

static struct device_attribute lcd_debug_attrs_mlvds[] = {
	__ATTR(mlvds,  0644, lcd_mlvds_debug_show, lcd_mlvds_debug_store),
	__ATTR(phy,    0644, lcd_phy_debug_show, lcd_phy_debug_store),
	__ATTR(tcon,   0644, lcd_tcon_debug_show, lcd_tcon_debug_store),
	__ATTR(tcon_status,   0444, lcd_tcon_status_show, NULL),
	__ATTR(tcon_reg,   0644, lcd_tcon_reg_debug_show, lcd_tcon_reg_debug_store),
	__ATTR(tcon_fw,   0644, lcd_tcon_fw_dbg_show, lcd_tcon_fw_dbg_store),
	__ATTR(null,   0644, NULL, NULL)
};

static struct device_attribute lcd_debug_attrs_p2p[] = {
	__ATTR(p2p,    0644, lcd_p2p_debug_show, lcd_p2p_debug_store),
	__ATTR(phy,    0644, lcd_phy_debug_show, lcd_phy_debug_store),
	__ATTR(tcon,   0644, lcd_tcon_debug_show, lcd_tcon_debug_store),
	__ATTR(tcon_status,   0444, lcd_tcon_status_show, NULL),
	__ATTR(tcon_reg,   0644, lcd_tcon_reg_debug_show, lcd_tcon_reg_debug_store),
	__ATTR(tcon_fw,   0644, lcd_tcon_fw_dbg_show, lcd_tcon_fw_dbg_store),
	__ATTR(null,   0644, NULL, NULL)
};

static struct device_attribute lcd_debug_attrs_mipi[] = {
	__ATTR(mipi,     0644, lcd_mipi_debug_show, lcd_mipi_debug_store),
	__ATTR(mpcmd,    0644, lcd_mipi_cmd_debug_show, lcd_mipi_cmd_debug_store),
	__ATTR(mpread,   0644, lcd_mipi_read_debug_show, lcd_mipi_read_debug_store),
	__ATTR(mpstate,  0444, lcd_mipi_state_debug_show, NULL),
	__ATTR(mpmode,   0644, lcd_mipi_mode_debug_show, lcd_mipi_mode_debug_store),
	__ATTR(null,     0644, NULL, NULL)
};

static struct device_attribute lcd_debug_attrs_edp[] = {
	__ATTR(edp,   0644, lcd_edp_debug_show, lcd_edp_debug_store),
	__ATTR(dpcd,  0200, NULL, lcd_edp_dpcd_debug_store),
	__ATTR(phy,   0644, lcd_phy_debug_show, lcd_phy_debug_store),
	__ATTR(edid,  0444, lcd_edp_edid_debug_show, NULL),
	__ATTR(null,  0644, NULL, NULL)
};

static int lcd_debug_file_creat(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_debug_info_s *lcd_debug_info;
	struct device_attribute *lcd_attr;
	int i;

	INIT_WORK(&pdrv->test_check_work, lcd_test_pattern_check);

	for (i = 0; i < ARRAY_SIZE(lcd_debug_attrs); i++) {
		if (device_create_file(pdrv->dev, &lcd_debug_attrs[i])) {
			LCDERR("create lcd debug attribute %s fail\n",
			       lcd_debug_attrs[i].attr.name);
		}
	}

	lcd_debug_info = (struct lcd_debug_info_s *)pdrv->debug_info;
	if (!lcd_debug_info) {
		LCDPR("%s: debug_info is null\n", __func__);
		return 0;
	}

	if (!lcd_debug_info->debug_if || !lcd_debug_info->debug_if->attrs)
		return 0;

	lcd_attr = lcd_debug_info->debug_if->attrs;
	while (lcd_attr) {
		if (strcmp(lcd_attr->attr.name, "null") == 0)
			break;
		if (device_create_file(pdrv->dev, lcd_attr)) {
			LCDERR("create interface debug attribute %s fail\n",
			       lcd_attr->attr.name);
		}
		lcd_attr++;
	}

	return 0;
}

static int lcd_debug_file_remove(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_debug_info_s *lcd_debug_info;
	struct device_attribute *lcd_attr;
	int i;

	for (i = 0; i < ARRAY_SIZE(lcd_debug_attrs); i++)
		device_remove_file(pdrv->dev, &lcd_debug_attrs[i]);

	lcd_debug_info = (struct lcd_debug_info_s *)pdrv->debug_info;
	if (!lcd_debug_info) {
		LCDPR("%s: debug_info is null\n", __func__);
		return 0;
	}

	if (!lcd_debug_info->debug_if || !lcd_debug_info->debug_if->attrs)
		return 0;

	lcd_attr = lcd_debug_info->debug_if->attrs;
	while (lcd_attr) {
		if (strcmp(lcd_attr->attr.name, "null") == 0)
			break;
		device_remove_file(pdrv->dev, lcd_attr);
		lcd_attr++;
	}

	return 0;
}

/* **********************************
 * lcd debug match data
 * **********************************
 */
/* interface data */
static struct lcd_debug_info_if_s lcd_debug_info_if_rgb = {
	.interface_print = lcd_info_print_rgb,
	.reg_dump_interface = lcd_reg_print_rgb,
	.reg_dump_phy = NULL,
	.attrs = lcd_debug_attrs_rgb,
};

static struct lcd_debug_info_if_s __maybe_unused lcd_debug_info_if_bt = {
	.interface_print = lcd_info_print_bt,
	.reg_dump_interface = lcd_reg_print_bt,
	.reg_dump_phy = NULL,
	.attrs = lcd_debug_attrs_bt,
};

static struct lcd_debug_info_if_s lcd_debug_info_if_lvds = {
	.interface_print = lcd_info_print_lvds,
	.reg_dump_interface = lcd_reg_print_lvds,
	.reg_dump_phy = lcd_reg_print_phy_analog,
	.attrs = lcd_debug_attrs_lvds,
};

static struct lcd_debug_info_if_s lcd_debug_info_if_lvds_t7 = {
	.interface_print = lcd_info_print_lvds,
	.reg_dump_interface = lcd_reg_print_lvds_t7,
	.reg_dump_phy = lcd_reg_print_phy_analog_t7,
	.attrs = lcd_debug_attrs_lvds,
};

static struct lcd_debug_info_if_s lcd_debug_info_if_lvds_t3 = {
	.interface_print = lcd_info_print_lvds,
	.reg_dump_interface = lcd_reg_print_lvds_t7,
	.reg_dump_phy = lcd_reg_print_phy_analog_t3,
	.attrs = lcd_debug_attrs_lvds,
};

static struct lcd_debug_info_if_s lcd_debug_info_if_lvds_t5w = {
	.interface_print = lcd_info_print_lvds,
	.reg_dump_interface = lcd_reg_print_lvds_t7,
	.reg_dump_phy = lcd_reg_print_phy_analog,
	.attrs = lcd_debug_attrs_lvds,
};

static struct lcd_debug_info_if_s lcd_debug_info_if_vbyone = {
	.interface_print = lcd_info_print_vbyone,
	.reg_dump_interface = lcd_reg_print_vbyone,
	.reg_dump_phy = lcd_reg_print_phy_analog,
	.attrs = lcd_debug_attrs_vbyone,
};

static struct lcd_debug_info_if_s lcd_debug_info_if_vbyone_t7 = {
	.interface_print = lcd_info_print_vbyone,
	.reg_dump_interface = lcd_reg_print_vbyone_t7,
	.reg_dump_phy = lcd_reg_print_phy_analog_t7,
	.attrs = lcd_debug_attrs_vbyone,
};

static struct lcd_debug_info_if_s lcd_debug_info_if_vbyone_t3 = {
	.interface_print = lcd_info_print_vbyone,
	.reg_dump_interface = lcd_reg_print_vbyone_t7,
	.reg_dump_phy = lcd_reg_print_phy_analog_t3,
	.attrs = lcd_debug_attrs_vbyone,
};

static struct lcd_debug_info_if_s lcd_debug_info_if_vbyone_t5w = {
	.interface_print = lcd_info_print_vbyone,
	.reg_dump_interface = lcd_reg_print_vbyone_t7,
	.reg_dump_phy = lcd_reg_print_phy_analog,
	.attrs = lcd_debug_attrs_vbyone,
};

static struct lcd_debug_info_if_s lcd_debug_info_if_mipi = {
	.interface_print = lcd_info_print_mipi,
	.reg_dump_interface = lcd_reg_print_mipi,
	.reg_dump_phy = lcd_reg_print_mipi_phy_analog,
	.attrs = lcd_debug_attrs_mipi,
};

static struct lcd_debug_info_if_s lcd_debug_info_if_mipi_axg = {
	.interface_print = lcd_info_print_mipi,
	.reg_dump_interface = lcd_reg_print_mipi,
	.reg_dump_phy = lcd_reg_print_mipi_phy_analog_axg,
	.attrs = lcd_debug_attrs_mipi,
};

static struct lcd_debug_info_if_s lcd_debug_info_if_mipi_t7 = {
	.interface_print = lcd_info_print_mipi,
	.reg_dump_interface = lcd_reg_print_mipi,
	.reg_dump_phy = lcd_reg_print_phy_analog_t7,
	.attrs = lcd_debug_attrs_mipi,
};

static struct lcd_debug_info_if_s lcd_debug_info_if_edp = {
	.interface_print = lcd_info_print_edp,
	.reg_dump_interface = lcd_reg_print_edp,
	.reg_dump_phy = lcd_reg_print_phy_analog_t7,
	.attrs = lcd_debug_attrs_edp,
};

static struct lcd_debug_info_if_s lcd_debug_info_if_mlvds = {
	.interface_print = lcd_info_print_mlvds,
	.reg_dump_interface = lcd_reg_print_tcon,
	.reg_dump_phy = lcd_reg_print_phy_analog,
	.attrs = lcd_debug_attrs_mlvds,
};

static struct lcd_debug_info_if_s lcd_debug_info_if_mlvds_t3 = {
	.interface_print = lcd_info_print_mlvds,
	.reg_dump_interface = lcd_reg_print_tcon_t3,
	.reg_dump_phy = lcd_reg_print_phy_analog_t3,
	.attrs = lcd_debug_attrs_mlvds,
};

static struct lcd_debug_info_if_s lcd_debug_info_if_mlvds_t5w = {
	.interface_print = lcd_info_print_mlvds,
	.reg_dump_interface = lcd_reg_print_tcon_t5w,
	.reg_dump_phy = lcd_reg_print_phy_analog,
	.attrs = lcd_debug_attrs_mlvds,
};

static struct lcd_debug_info_if_s lcd_debug_info_if_p2p = {
	.interface_print = lcd_info_print_p2p,
	.reg_dump_interface = lcd_reg_print_tcon,
	.reg_dump_phy = lcd_reg_print_phy_analog,
	.attrs = lcd_debug_attrs_p2p,
};

static struct lcd_debug_info_if_s lcd_debug_info_if_p2p_t3 = {
	.interface_print = lcd_info_print_p2p,
	.reg_dump_interface = lcd_reg_print_tcon_t3,
	.reg_dump_phy = lcd_reg_print_phy_analog_t3,
	.attrs = lcd_debug_attrs_p2p,
};

static struct lcd_debug_info_if_s lcd_debug_info_if_p2p_t5w = {
	.interface_print = lcd_info_print_p2p,
	.reg_dump_interface = lcd_reg_print_tcon_t5w,
	.reg_dump_phy = lcd_reg_print_phy_analog,
	.attrs = lcd_debug_attrs_p2p,
};

/* chip_type data */
static struct lcd_debug_info_s lcd_debug_info_axg = {
	.reg_pll_table = NULL,
	.reg_clk_table = NULL,
	.reg_clk_hiu_table = lcd_reg_dump_clk_axg,
	.reg_encl_table = lcd_reg_dump_encl_dft,
	.reg_pinmux_table = NULL,

	.debug_if_rgb = NULL,
	.debug_if_bt = NULL,
	.debug_if_lvds = NULL,
	.debug_if_vbyone = NULL,
	.debug_if_mlvds = NULL,
	.debug_if_p2p = NULL,
	.debug_if_mipi = &lcd_debug_info_if_mipi_axg,
	.debug_if_edp = NULL,
	.debug_if = NULL,
};

static struct lcd_debug_info_s lcd_debug_info_g12a_clk_path0 = {
	.reg_pll_table = NULL,
	.reg_clk_table = lcd_reg_dump_clk_hpll_g12a,
	.reg_clk_hiu_table = NULL,
	.reg_encl_table = lcd_reg_dump_encl_dft,
	.reg_pinmux_table = NULL,

	.debug_if_rgb = NULL,
	.debug_if_bt = NULL,
	.debug_if_lvds = NULL,
	.debug_if_vbyone = NULL,
	.debug_if_mlvds = NULL,
	.debug_if_p2p = NULL,
	.debug_if_mipi = &lcd_debug_info_if_mipi,
	.debug_if_edp = NULL,
	.debug_if = NULL,
};

static struct lcd_debug_info_s lcd_debug_info_g12a_clk_path1 = {
	.reg_pll_table = NULL,
	.reg_clk_table = lcd_reg_dump_clk_gp0_g12a,
	.reg_clk_hiu_table = NULL,
	.reg_encl_table = lcd_reg_dump_encl_dft,
	.reg_pinmux_table = NULL,

	.debug_if_rgb = NULL,
	.debug_if_bt = NULL,
	.debug_if_lvds = NULL,
	.debug_if_vbyone = NULL,
	.debug_if_mlvds = NULL,
	.debug_if_p2p = NULL,
	.debug_if_mipi = &lcd_debug_info_if_mipi,
	.debug_if_edp = NULL,
	.debug_if = NULL,
};

static struct lcd_debug_info_s lcd_debug_info_tl1 = {
	.reg_pll_table = NULL,
	.reg_clk_table = lcd_reg_dump_clk_tl1,
	.reg_clk_hiu_table = NULL,
	.reg_encl_table = lcd_reg_dump_encl_tl1,
	.reg_pinmux_table = lcd_reg_dump_pinmux_tl1,

	.debug_if_rgb = NULL,
	.debug_if_bt = NULL,
	.debug_if_lvds = &lcd_debug_info_if_lvds,
	.debug_if_vbyone = &lcd_debug_info_if_vbyone,
	.debug_if_mlvds = &lcd_debug_info_if_mlvds,
	.debug_if_p2p = &lcd_debug_info_if_p2p,
	.debug_if_mipi = NULL,
	.debug_if_edp = NULL,
	.debug_if = NULL,
};

static struct lcd_debug_info_s lcd_debug_info_t5 = {
	.reg_pll_table = lcd_reg_dump_pll_t5,
	.reg_clk_table = lcd_reg_dump_clk_t5,
	.reg_clk_hiu_table = NULL,
	.reg_encl_table = lcd_reg_dump_encl_tl1,
	.reg_pinmux_table = lcd_reg_dump_pinmux_t5,

	.debug_if_rgb = NULL,
	.debug_if_bt = NULL,
	.debug_if_lvds = &lcd_debug_info_if_lvds,
	.debug_if_vbyone = &lcd_debug_info_if_vbyone,
	.debug_if_mlvds = &lcd_debug_info_if_mlvds,
	.debug_if_p2p = &lcd_debug_info_if_p2p,
	.debug_if_mipi = NULL,
	.debug_if_edp = NULL,
	.debug_if = NULL,
};

static struct lcd_debug_info_s lcd_debug_info_t7_0 = {
	.reg_pll_table = lcd_reg_dump_pll_t7_0,
	.reg_clk_table = lcd_reg_dump_clk_t7_0,
	.reg_clk_hiu_table = NULL,
	.reg_encl_table = lcd_reg_dump_encl_t7_0,
	.reg_pinmux_table = lcd_reg_dump_pinmux_t7,

	.debug_if_rgb = NULL,
	.debug_if_bt = NULL,
	.debug_if_lvds = &lcd_debug_info_if_lvds_t7,
	.debug_if_vbyone = &lcd_debug_info_if_vbyone_t7,
	.debug_if_mlvds = NULL,
	.debug_if_p2p = NULL,
	.debug_if_mipi = &lcd_debug_info_if_mipi_t7,
	.debug_if_edp = &lcd_debug_info_if_edp,
	.debug_if = NULL,
};

static struct lcd_debug_info_s lcd_debug_info_t7_1 = {
	.reg_pll_table = lcd_reg_dump_pll_t7_1,
	.reg_clk_table = lcd_reg_dump_clk_t7_1,
	.reg_clk_hiu_table = NULL,
	.reg_encl_table = lcd_reg_dump_encl_t7_1,
	.reg_pinmux_table = lcd_reg_dump_pinmux_t7,

	.debug_if_rgb = NULL,
	.debug_if_bt = NULL,
	.debug_if_lvds = &lcd_debug_info_if_lvds_t7,
	.debug_if_vbyone = &lcd_debug_info_if_vbyone_t7,
	.debug_if_mlvds = NULL,
	.debug_if_p2p = NULL,
	.debug_if_mipi = &lcd_debug_info_if_mipi_t7,
	.debug_if_edp = &lcd_debug_info_if_edp,
	.debug_if = NULL,
};

static struct lcd_debug_info_s lcd_debug_info_t7_2 = {
	.reg_pll_table = lcd_reg_dump_pll_t7_2,
	.reg_clk_table = lcd_reg_dump_clk_t7_2,
	.reg_clk_hiu_table = NULL,
	.reg_encl_table = lcd_reg_dump_encl_t7_2,
	.reg_pinmux_table = lcd_reg_dump_pinmux_t7,

	.debug_if_rgb = NULL,
	.debug_if_bt = NULL,
	.debug_if_lvds = &lcd_debug_info_if_lvds_t7,
	.debug_if_vbyone = NULL,
	.debug_if_mlvds = NULL,
	.debug_if_p2p = NULL,
	.debug_if_mipi = NULL,
	.debug_if_edp = NULL,
	.debug_if = NULL,
};

static struct lcd_debug_info_s lcd_debug_info_t3_0 = {
	.reg_pll_table = lcd_reg_dump_pll_t3_0,
	.reg_clk_table = lcd_reg_dump_clk_t7_0,
	.reg_clk_hiu_table = NULL,
	.reg_encl_table = lcd_reg_dump_encl_t7_0,
	.reg_pinmux_table = lcd_reg_dump_pinmux_t3,

	.debug_if_rgb = NULL,
	.debug_if_bt = NULL,
	.debug_if_lvds = &lcd_debug_info_if_lvds_t3,
	.debug_if_vbyone = &lcd_debug_info_if_vbyone_t3,
	.debug_if_mlvds = &lcd_debug_info_if_mlvds_t3,
	.debug_if_p2p = &lcd_debug_info_if_p2p_t3,
	.debug_if_mipi = NULL,
	.debug_if_edp = NULL,
	.debug_if = NULL,
};

static struct lcd_debug_info_s lcd_debug_info_t3_1 = {
	.reg_pll_table = lcd_reg_dump_pll_t3_0,
	.reg_clk_table = lcd_reg_dump_clk_t7_1,
	.reg_clk_hiu_table = NULL,
	.reg_encl_table = lcd_reg_dump_encl_t7_1,
	.reg_pinmux_table = lcd_reg_dump_pinmux_t3,

	.debug_if_rgb = NULL,
	.debug_if_bt = NULL,
	.debug_if_lvds = NULL,
	.debug_if_vbyone = &lcd_debug_info_if_vbyone_t3,
	.debug_if_mlvds = NULL,
	.debug_if_p2p = NULL,
	.debug_if_mipi = NULL,
	.debug_if_edp = NULL,
	.debug_if = NULL,
};

static struct lcd_debug_info_s lcd_debug_info_t5w = {
	.reg_pll_table = lcd_reg_dump_pll_t5,
	.reg_clk_table = lcd_reg_dump_clk_t5w,
	.reg_clk_hiu_table = NULL,
	.reg_encl_table = lcd_reg_dump_encl_t7_0,
	.reg_pinmux_table = lcd_reg_dump_pinmux_tl1,

	.debug_if_rgb = NULL,
	.debug_if_bt = NULL,
	.debug_if_lvds = &lcd_debug_info_if_lvds_t5w,
	.debug_if_vbyone = &lcd_debug_info_if_vbyone_t5w,
	.debug_if_mlvds = &lcd_debug_info_if_mlvds_t5w,
	.debug_if_p2p = &lcd_debug_info_if_p2p_t5w,
	.debug_if_mipi = NULL,
	.debug_if_edp = NULL,
	.debug_if = NULL,
};

static struct lcd_debug_info_s lcd_debug_info_a4 = {
	.reg_pll_table = NULL,
	.reg_clk_table = NULL,
	.reg_clk_hiu_table = lcd_reg_dump_clk_a4,
	.reg_encl_table = lcd_reg_dump_encl_a4,
	.reg_pinmux_table = NULL,

	.debug_if_rgb = &lcd_debug_info_if_rgb,
	.debug_if_bt = NULL,
	.debug_if_lvds = NULL,
	.debug_if_vbyone = NULL,
	.debug_if_mlvds = NULL,
	.debug_if_p2p = NULL,
	.debug_if_mipi = NULL,
	.debug_if_edp = NULL,
	.debug_if = NULL,
};

int lcd_debug_probe(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_debug_info_s *lcd_debug_info;
	int lcd_type;

	lcd_type = pdrv->config.basic.lcd_type;

	switch (pdrv->data->chip_type) {
	case LCD_CHIP_TL1:
	case LCD_CHIP_TM2:
		lcd_debug_info = &lcd_debug_info_tl1;
		break;
	case LCD_CHIP_T5:
	case LCD_CHIP_T5D:
		lcd_debug_info = &lcd_debug_info_t5;
		break;
	case LCD_CHIP_T5W:
		lcd_debug_info = &lcd_debug_info_t5w;
		break;
	case LCD_CHIP_T7:
		switch (pdrv->index) {
		case 1:
			lcd_debug_info = &lcd_debug_info_t7_1;
			break;
		case 2:
			lcd_debug_info = &lcd_debug_info_t7_2;
			break;
		case 0:
		default:
			lcd_debug_info = &lcd_debug_info_t7_0;
			break;
		}
		break;
	case LCD_CHIP_T3:
		switch (pdrv->index) {
		case 1:
			lcd_debug_info = &lcd_debug_info_t3_1;
			break;
		default:
			lcd_debug_info = &lcd_debug_info_t3_0;
			break;
		}
		break;
	case LCD_CHIP_AXG:
		lcd_debug_info = &lcd_debug_info_axg;
		break;
	case LCD_CHIP_G12A:
	case LCD_CHIP_G12B:
	case LCD_CHIP_SM1:
		if (pdrv->clk_path)
			lcd_debug_info = &lcd_debug_info_g12a_clk_path1;
		else
			lcd_debug_info = &lcd_debug_info_g12a_clk_path0;
		break;
	case LCD_CHIP_A4:
		lcd_debug_info = &lcd_debug_info_a4;
		break;
	default:
		lcd_debug_info = NULL;
		return -1;
	}

	switch (lcd_type) {
	case LCD_RGB:
		lcd_debug_info->debug_if = lcd_debug_info->debug_if_rgb;
		break;
	case LCD_BT656:
	case LCD_BT1120:
		lcd_debug_info->debug_if = lcd_debug_info->debug_if_bt;
		break;
	case LCD_LVDS:
		lcd_debug_info->debug_if = lcd_debug_info->debug_if_lvds;
		break;
	case LCD_VBYONE:
		lcd_debug_info->debug_if = lcd_debug_info->debug_if_vbyone;
		break;
	case LCD_MIPI:
		lcd_debug_info->debug_if = lcd_debug_info->debug_if_mipi;
		break;
	case LCD_EDP:
		lcd_debug_info->debug_if = lcd_debug_info->debug_if_edp;
		break;
	case LCD_MLVDS:
		lcd_debug_info->debug_if = lcd_debug_info->debug_if_mlvds;
		break;
	case LCD_P2P:
		lcd_debug_info->debug_if = lcd_debug_info->debug_if_p2p;
		break;
	default:
		lcd_debug_info->debug_if = NULL;
		break;
	}
	pdrv->debug_info = (void *)lcd_debug_info;

	return lcd_debug_file_creat(pdrv);
}

int lcd_debug_remove(struct aml_lcd_drv_s *pdrv)
{
	return lcd_debug_file_remove(pdrv);
}
