/*
 * AMLOGIC lcd controller driver.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the named License,
 * or any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 *
 * Author:  Tim Yao <timyao@amlogic.com>
 *
 * Modify:  Evoke Zhang <evoke.zhang@amlogic.com>
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
#include <asm/fiq.h>
#include <linux/delay.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/of.h>
#include <linux/reset.h>
#include <linux/amlogic/vout/vinfo.h>
#include <linux/amlogic/vout/vout_notify.h>
#include <linux/amlogic/vout/lcdoutc.h>
#ifdef CONFIG_AML_LCD_EXTERN
#include <linux/amlogic/vout/lcd_extern.h>
#endif
#ifdef CONFIG_AMLOGIC_BACKLIGHT
#include <linux/amlogic/vout/aml_tablet_bl.h>
#endif
#ifdef CONFIG_AMLOGIC_BOARD_HAS_PMU
#include <linux/amlogic/aml_pmu_common.h>
#endif
#include "lcd/lcd_reg.h"

#define PANEL_NAME		"panel"

struct lcd_dev_s {
	struct Lcd_Config_s *pConf;
	struct vinfo_s lcd_info;
};

const char *lcd_power_type_table[] = {
	"cpu",
	"pmu",
	"signal",
	"init",
	"null",
};

const char *lcd_power_pmu_gpio_table[] = {
	"GPIO0",
	"GPIO1",
	"GPIO2",
	"GPIO3",
	"GPIO4",
	"null",
};

#ifdef LCD_DEBUG_INFO
unsigned int lcd_print_flag = 1;
#else
unsigned int lcd_print_flag = 0;
#endif

static struct lcd_dev_s *pDev;
#ifdef CONFIG_AML_LCD_GAMMA_DEBUG
static struct class *gamma_debug_class;
#endif
static enum Bool_state_e data_status = ON;
static struct Lcd_CPU_GPIO_s lcd_cpu_gpio[LCD_PWR_CTRL_STEP_MAX];

void lcd_print(const char *fmt, ...)
{
	va_list args;

	if (lcd_print_flag == 0)
		return;
	va_start(args, fmt);
	vprintk(fmt, args);
	va_end(args);
}

static inline void lcd_mdelay(int n)
{
	mdelay(n);
}

#ifdef CONFIG_USE_OF
static void lcd_cpu_gpio_ctrl(int gpio, int value)
{
	struct gpio_desc *gdesc;

	if (gpio < pDev->pConf->lcd_power_ctrl.cpu_gpio_num) {
		gdesc = lcd_cpu_gpio[gpio].desc;
		if (value == LCD_GPIO_OUTPUT_LOW)
			lcd_gpio_output(gdesc, 0);
		else if (value == LCD_GPIO_OUTPUT_HIGH)
			lcd_gpio_output(gdesc, 1);
		else
			lcd_gpio_input(gdesc);
	} else {
		DPRINT("[error]: lcd cpu_gpio wrong index %d\n", gpio);
	}
}

static int lcd_power_ctrl(enum Bool_state_e status)
{
	int i;
	int ret = 0;
	unsigned int gpio;
	unsigned short value;
	struct Lcd_Power_Ctrl_s *pctrl;
#ifdef CONFIG_AMLOGIC_BOARD_HAS_PMU
	struct aml_pmu_driver *pmu_drv;
#endif
#ifdef CONFIG_AML_LCD_EXTERN
	struct aml_lcd_extern_driver_s *lcd_ext;
#endif

	pctrl = &(pDev->pConf->lcd_power_ctrl);
	lcd_print("%s(): %s\n", __func__, (status ? "ON" : "OFF"));
	if (status) {
		for (i = 0; i < pctrl->power_on_step; i++) {
			lcd_print("%s %s step %d\n", __func__,
				(status ? "ON" : "OFF"), i+1);
			switch (pctrl->power_on_config[i].type) {
			case LCD_POWER_TYPE_CPU:
				gpio = pctrl->power_on_config[i].gpio;
				value = pctrl->power_on_config[i].value;
				lcd_cpu_gpio_ctrl(gpio, value);
				break;
			case LCD_POWER_TYPE_PMU:
#ifdef CONFIG_AMLOGIC_BOARD_HAS_PMU
				gpio = pctrl->power_on_config[i].gpio;
				value = pctrl->power_on_config[i].value;
				pmu_drv = aml_pmu_get_driver();
				if (pmu_drv == NULL) {
					DPRINT("no pmu driver\n");
				} else if (pmu_drv->pmu_set_gpio) {
					if (value == LCD_GPIO_OUTPUT_LOW)
						pmu_drv->pmu_set_gpio(gpio, 0);
					else
						pmu_drv->pmu_set_gpio(gpio, 1);
				}
#endif
				break;
			case LCD_POWER_TYPE_SIGNAL:
				if (pctrl->power_level == 0)
					break;
				if (pctrl->ports_ctrl == NULL)
					lcd_print("no lcd_ports_ctrl\n");
				else
					ret = pctrl->ports_ctrl(ON);
				break;
			case LCD_POWER_TYPE_INITIAL:
				if (pctrl->power_level == 0)
					break;
#ifdef CONFIG_AML_LCD_EXTERN
				lcd_ext = aml_lcd_extern_get_driver();
				if (lcd_ext == NULL) {
					DPRINT("no lcd_extern driver\n");
				} else {
					if (lcd_ext->power_on) {
						lcd_ext->power_on();
						DPRINT("%s power on init\n",
							lcd_ext->name);
					}
				}
#endif
				break;
			default:
				DPRINT("lcd power ctrl ON step %d is null\n",
					i+1);
				break;
			}
			if (pctrl->power_on_config[i].delay > 0)
				lcd_mdelay(pctrl->power_on_config[i].delay);
		}
		data_status = status;
	} else {
		data_status = status;
		lcd_mdelay(30);
		for (i = 0; i < pctrl->power_off_step; i++) {
			lcd_print("%s %s step %d\n", __func__,
				(status ? "ON" : "OFF"), i+1);
			switch (pctrl->power_off_config[i].type) {
			case LCD_POWER_TYPE_CPU:
				gpio = pctrl->power_off_config[i].gpio;
				value = pctrl->power_off_config[i].value;
				lcd_cpu_gpio_ctrl(gpio, value);
				break;
			case LCD_POWER_TYPE_PMU:
#ifdef CONFIG_AMLOGIC_BOARD_HAS_PMU
				gpio = pctrl->power_off_config[i].gpio;
				value = pctrl->power_off_config[i].value;
				pmu_drv = aml_pmu_get_driver();
				if (pmu_drv == NULL) {
					DPRINT("no pmu driver\n");
				} else if (pmu_drv->pmu_set_gpio) {
					if (value == LCD_GPIO_OUTPUT_LOW)
						pmu_drv->pmu_set_gpio(gpio, 0);
					else
						pmu_drv->pmu_set_gpio(gpio, 1);
				}
#endif
				break;
			case LCD_POWER_TYPE_SIGNAL:
				if (pctrl->power_level == 0)
					break;
				if (pctrl->ports_ctrl == NULL)
					lcd_print("no lcd_ports_ctrl\n");
				else
					ret = pctrl->ports_ctrl(OFF);
				break;
			case LCD_POWER_TYPE_INITIAL:
				if (pctrl->power_level == 0)
					break;
#ifdef CONFIG_AML_LCD_EXTERN
				lcd_ext = aml_lcd_extern_get_driver();
				if (lcd_ext == NULL) {
					DPRINT("no lcd_extern driver\n");
				} else {
					if (lcd_ext->power_off) {
						lcd_ext->power_off();
						DPRINT("%s power off init\n",
							lcd_ext->name);
					}
				}
#endif
				break;
			default:
				DPRINT("lcd power ctrl OFF step %d is null\n",
					i+1);
				break;
			}
			if (pctrl->power_off_config[i].delay > 0)
				lcd_mdelay(pctrl->power_off_config[i].delay);
		}
	}

	DPRINT("%s(): %s finished\n", __func__, (status ? "ON" : "OFF"));
	return ret;
}
#endif

static void backlight_power_ctrl(enum Bool_state_e status)
{
	if (status == ON) {
		if (data_status == OFF)
			return;
#ifdef CONFIG_AMLOGIC_BACKLIGHT
		bl_power_on(LCD_BL_FLAG);
#endif
	} else {
#ifdef CONFIG_AMLOGIC_BACKLIGHT
		bl_power_off(LCD_BL_FLAG);
#endif
	}
	lcd_print("%s(%s): data_status=%s\n", __func__,
		(status ? "ON" : "OFF"), (data_status ? "ON" : "OFF"));
}

void _enable_backlight(void)
{
	backlight_power_ctrl(ON);
}
void _disable_backlight(void)
{
	backlight_power_ctrl(OFF);
}

static void _lcd_module_enable(void)
{
	pDev->pConf->lcd_misc_ctrl.module_enable();
}

static void _lcd_module_disable(void)
{
	pDev->pConf->lcd_misc_ctrl.module_disable();
}

static const struct vinfo_s *lcd_get_current_info(void)
{
	if (pDev == NULL) {
		DPRINT("[error] no lcd device exist!\n");
		return NULL;
	} else
		return &pDev->lcd_info;
}

DEFINE_MUTEX(lcd_vout_mutex);
static int lcd_set_current_vmode(enum vmode_e mode)
{
	mutex_lock(&lcd_vout_mutex);
	if (VMODE_LCD != (mode & VMODE_MODE_BIT_MASK)) {
		mutex_unlock(&lcd_vout_mutex);
		return -EINVAL;
	}

	pDev->pConf->lcd_misc_ctrl.vpp_sel = 0;
	lcd_reg_write(VPP_POSTBLEND_H_SIZE, pDev->lcd_info.width);

	if (!(mode&VMODE_LOGO_BIT_MASK)) {
		_disable_backlight();
		_lcd_module_enable();
		_enable_backlight();
	}
	if (VMODE_INIT_NULL == pDev->lcd_info.mode)
		pDev->lcd_info.mode = VMODE_LCD;

	mutex_unlock(&lcd_vout_mutex);
	return 0;
}

#ifdef CONFIG_AM_TV_OUTPUT2
static int lcd_set_current_vmode2(enum vmode_e mode)
{
	mutex_lock(&lcd_vout_mutex);
	if (mode != VMODE_LCD) {
		mutex_unlock(&lcd_vout_mutex);
		return -EINVAL;
	}
	_disable_backlight();
	pDev->pConf->lcd_misc_ctrl.vpp_sel = 1;

	lcd_reg_write(VPP2_POSTBLEND_H_SIZE, pDev->lcd_info.width);

	_lcd_module_enable();
	if (VMODE_INIT_NULL == pDev->lcd_info.mode)
		pDev->lcd_info.mode = VMODE_LCD;
	_enable_backlight();
	mutex_unlock(&lcd_vout_mutex);
	return 0;
}
#endif

static enum vmode_e lcd_validate_vmode(char *mode)
{
	if (mode == NULL)
		return VMODE_MAX;

	if ((strncmp(mode, PANEL_NAME, strlen(PANEL_NAME))) == 0)
		return VMODE_LCD;

	return VMODE_MAX;
}
static int lcd_vmode_is_supported(enum vmode_e mode)
{
	mode &= VMODE_MODE_BIT_MASK;
	if (mode == VMODE_LCD)
		return true;
	return false;
}

static int lcd_vout_disable(enum vmode_e cur_vmod)
{
	mutex_lock(&lcd_vout_mutex);
	_disable_backlight();
	_lcd_module_disable();
	mutex_unlock(&lcd_vout_mutex);
	return 0;
}

#ifdef CONFIG_PM
static int lcd_suspend(void)
{
	mutex_lock(&lcd_vout_mutex);
	BUG_ON(pDev == NULL);
	DPRINT("lcd_suspend\n");
	_disable_backlight();
	_lcd_module_disable();
	mutex_unlock(&lcd_vout_mutex);
	return 0;
}
static int lcd_resume(void)
{
	mutex_lock(&lcd_vout_mutex);
	DPRINT("lcd_resume\n");
	_lcd_module_enable();
	_enable_backlight();
	mutex_unlock(&lcd_vout_mutex);
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

#ifdef CONFIG_AM_TV_OUTPUT2
static struct vout_server_s lcd_vout2_server = {
	.name = "lcd_vout2_server",
	.op = {
		.get_vinfo = lcd_get_current_info,
		.set_vmode = lcd_set_current_vmode2,
		.validate_vmode = lcd_validate_vmode,
		.vmode_is_supported = lcd_vmode_is_supported,
		.disable = lcd_vout_disable,
#ifdef CONFIG_PM
		.vout_suspend = lcd_suspend,
		.vout_resume = lcd_resume,
#endif
	},
};
#endif

static void _init_vout(void)
{
	struct vinfo_s *info;

	info = &(pDev->lcd_info);
	info->name = PANEL_NAME;
	info->mode = VMODE_LCD;
	info->width = pDev->pConf->lcd_basic.h_active;
	info->height = pDev->pConf->lcd_basic.v_active;
	info->field_height = pDev->pConf->lcd_basic.v_active;
	info->aspect_ratio_num = pDev->pConf->lcd_basic.screen_ratio_width;
	info->aspect_ratio_den = pDev->pConf->lcd_basic.screen_ratio_height;
	info->screen_real_width = pDev->pConf->lcd_basic.h_active_area;
	info->screen_real_height = pDev->pConf->lcd_basic.v_active_area;
	info->sync_duration_num = pDev->pConf->lcd_timing.sync_duration_num;
	info->sync_duration_den = pDev->pConf->lcd_timing.sync_duration_den;
	info->video_clk = pDev->pConf->lcd_timing.lcd_clk;

	/* add lcd actual active area size */
	DPRINT("lcd actual active area size: %d %d (mm)\n",
		pDev->pConf->lcd_basic.h_active_area,
		pDev->pConf->lcd_basic.v_active_area);
	vout_register_server(&lcd_vout_server);
#ifdef CONFIG_AM_TV_OUTPUT2
	vout2_register_server(&lcd_vout2_server);
#endif
}

/* *********************************************************
 * gamma debug
 * ********************************************************* */
#ifdef CONFIG_AML_LCD_GAMMA_DEBUG
static unsigned short gamma_adj_r[256];
static unsigned short gamma_adj_g[256];
static unsigned short gamma_adj_b[256];
static unsigned short gamma_r_coeff, gamma_g_coeff, gamma_b_coeff;
static unsigned gamma_ctrl;

static void save_original_gamma(struct Lcd_Config_s *pConf)
{
	int i;

	for (i = 0; i < 256; i++) {
		gamma_adj_r[i] = pConf->lcd_effect.GammaTableR[i];
		gamma_adj_g[i] = pConf->lcd_effect.GammaTableG[i];
		gamma_adj_b[i] = pConf->lcd_effect.GammaTableB[i];
	}
	gamma_ctrl = pConf->lcd_effect.gamma_ctrl;
	gamma_r_coeff = pConf->lcd_effect.gamma_r_coeff;
	gamma_g_coeff = pConf->lcd_effect.gamma_g_coeff;
	gamma_b_coeff = pConf->lcd_effect.gamma_b_coeff;
}

static void read_original_gamma_table(void)
{
	unsigned i;

	DPRINT("original gamma: enable=%d, reverse=%d, ",
		((gamma_ctrl >> GAMMA_CTRL_EN) & 1),
		((gamma_ctrl >> GAMMA_CTRL_REV) & 1));
	DPRINT("r_coeff=%u%%, g_coeff=%u%%, b_coeff=%u%%\n",
		gamma_r_coeff, gamma_g_coeff, gamma_b_coeff);
	DPRINT("read original gamma table R:\n");
	for (i = 0; i < 256; i++)
		DPRINT("%u,", gamma_adj_r[i]);
	DPRINT("\n\nread original gamma table G:\n");
	for (i = 0; i < 256; i++)
		DPRINT("%u,", gamma_adj_g[i]);
	DPRINT("\n\nread original gamma table B:\n");
	for (i = 0; i < 256; i++)
		DPRINT("%u,", gamma_adj_b[i]);
	DPRINT("\n");
}

static void read_current_gamma_table(struct Lcd_Config_s *pConf)
{
	unsigned i;

	DPRINT("current gamma: enable=%d, reverse=%d, ",
		((pConf->lcd_effect.gamma_ctrl >> GAMMA_CTRL_EN) & 1),
		((pConf->lcd_effect.gamma_ctrl >> GAMMA_CTRL_REV) & 1));

	DPRINT("r_coeff=%u%%, g_coeff=%u%%, b_coeff=%u%%\n",
		pConf->lcd_effect.gamma_r_coeff,
		pConf->lcd_effect.gamma_g_coeff,
		pConf->lcd_effect.gamma_b_coeff);
	DPRINT("read current gamma table R:\n");
	for (i = 0; i < 256; i++)
		DPRINT("%u ", pConf->lcd_effect.GammaTableR[i]);
	DPRINT("\n\nread current gamma table G:\n");
	for (i = 0; i < 256; i++)
		DPRINT("%u ", pConf->lcd_effect.GammaTableG[i]);
	DPRINT("\n\nread current gamma table B:\n");
	for (i = 0; i < 256; i++)
		DPRINT("%u ", pConf->lcd_effect.GammaTableB[i]);
	DPRINT("\n");
}

static int write_gamma_table(struct Lcd_Config_s *pConf, unsigned int en)
{
	int ret = 0;

	if (pConf->lcd_effect.set_gamma_table == NULL) {
		DPRINT("set gamma table function is null\n");
		ret = -1;
	} else {
		pConf->lcd_effect.set_gamma_table(en);
	}
	return ret;
}

static void set_gamma_coeff(struct Lcd_Config_s *pConf, unsigned r_coeff,
			unsigned g_coeff, unsigned b_coeff)
{
	pConf->lcd_effect.gamma_r_coeff = (unsigned short)(r_coeff);
	pConf->lcd_effect.gamma_g_coeff = (unsigned short)(g_coeff);
	pConf->lcd_effect.gamma_b_coeff = (unsigned short)(b_coeff);
	if (write_gamma_table(pConf, 1) == 0) {
		DPRINT("write gamma table with scale factor ");
		DPRINT("R:%u%%, G:%u%%, B:%u%%\n", r_coeff, g_coeff, b_coeff);
	}
}

static void reset_gamma_table(struct Lcd_Config_s *pConf)
{
	int i;

	for (i = 0; i < 256; i++) {
		pConf->lcd_effect.GammaTableR[i] = gamma_adj_r[i];
		pConf->lcd_effect.GammaTableG[i] = gamma_adj_g[i];
		pConf->lcd_effect.GammaTableB[i] = gamma_adj_b[i];
	}
	pConf->lcd_effect.gamma_ctrl = gamma_ctrl;
	pConf->lcd_effect.gamma_r_coeff = gamma_r_coeff;
	pConf->lcd_effect.gamma_g_coeff = gamma_g_coeff;
	pConf->lcd_effect.gamma_b_coeff = gamma_b_coeff;
	if (write_gamma_table(pConf,
		((pConf->lcd_effect.gamma_ctrl >> GAMMA_CTRL_EN) & 1)) == 0) {
		DPRINT("write gamma table to original\n");
	}
}

static const char *usage_str = {
"Usage:\n"
"    echo coeff <R_coeff> <G_coeff> <B_coeff> > write ; set R,G,B gamma scale factor\n"
"    echo ctrl <enable> <reverse> > write; control gamma table enable and reverse\n"
"data format:\n"
"    <R/G/B_coeff>  : a number in Dec(0~100), means a percent value\n"
"    <enable>       : 0=disable, 1=enable\n"
"    <reverse>      : 0=normal, 1=reverse\n"
"\n"
"    echo [r|g|b] <step> <value> <value> <value> <value> <value> <value> <value> <value> > write ; input R/G/B gamma table\n"
"    echo w [0 | 8 | 10] > write ; apply the original/8bit/10bit gamma table\n"
"data format:\n"
"    <step>  : 0xX, 4bit in Hex, there are 8 steps(0~7, 8bit gamma) or 16 steps(0~f, 10bit gamma) for a single cycle\n"
"    <value> : 0xXXXXXXXX, 32bit in Hex, 2 or 4 gamma table values (8 or 10bit gamma) combia in one <value>\n"
"\n"
"    echo f[r | g | b | w] <level_value> > write ; write R/G/B/white gamma level with fixed level_value\n"
"    echo reset > write ; reset the gamma table to original\n"
"data format:\n"
"    <level_value>  : a number in Dec(0~255)\n"
"\n"
"    echo test <num> > write ; gamma curve test, you should control gamma table and video adjust enable manually by other command\n"
"    echo auto [enable] > write ; gamma curve auto test, auto disable video adjust and run test pattern\n"
"data format:\n"
"    <num>   : a number in Dec(1~18), num=0 will disable gamma test pattern\n"
"    <enable>: 0=disable gamma table(default), 1=enable gamma table\n"
"\n"
"    echo [0 | 1] > read ; readback original/current gamma table\n"
};

static ssize_t gamma_help(struct class *class, struct class_attribute *attr,
		char *buf)
{
	return sprintf(buf, "%s\n", usage_str);
}

static ssize_t aml_lcd_gamma_read(struct class *class,
		struct class_attribute *attr, const char *buf, size_t count)
{
	if (buf[0] == '0')
		read_original_gamma_table();
	else
		read_current_gamma_table(pDev->pConf);

	return count;
}

static unsigned int gamma_adj_r_temp[128];
static unsigned int gamma_adj_g_temp[128];
static unsigned int gamma_adj_b_temp[128];

static void gamma_table_cacl_8bit(struct Lcd_Effect_s *eft)
{
	int i;
	int j;
	unsigned int temp;

	for (i = 0; i < 64; i++) {
		for (j = 0; j < 4; j++) {
			temp = (((gamma_adj_r_temp[i]>>(24-j*8)) & 0xff)<<2);
			eft->GammaTableR[i*4+j] = (unsigned short)temp;
			temp = (((gamma_adj_g_temp[i]>>(24-j*8)) & 0xff)<<2);
			eft->GammaTableG[i*4+j] = (unsigned short)temp;
			temp = (((gamma_adj_b_temp[i]>>(24-j*8)) & 0xff)<<2);
			eft->GammaTableB[i*4+j] = (unsigned short)temp;
		}
	}
	if (write_gamma_table(pDev->pConf, 1) == 0)
		DPRINT("write gamma table 8bit finished\n");
}

static void gamma_table_cacl_10bit(struct Lcd_Effect_s *eft)
{
	int i, j;
	unsigned int temp;

	for (i = 0; i < 128; i++) {
		for (j = 0; j < 2; j++) {
			temp = ((gamma_adj_r_temp[i]>>(16-j*16)) & 0xffff);
			eft->GammaTableR[i*2+j] = (unsigned short)temp;
			temp = ((gamma_adj_g_temp[i]>>(16-j*16)) & 0xffff);
			eft->GammaTableG[i*2+j] = (unsigned short)temp;
			temp = ((gamma_adj_b_temp[i]>>(16-j*16)) & 0xffff);
			eft->GammaTableB[i*2+j] = (unsigned short)temp;
		}
	}
	if (write_gamma_table(pDev->pConf, 1) == 0)
		DPRINT("write gamma table 10bit finished\n");
}

static ssize_t aml_lcd_gamma_debug(struct class *class,
		struct class_attribute *attr, const char *buf, size_t count)
{
	unsigned int ret;
	unsigned int i, j;
	unsigned t[8];
	struct Lcd_Effect_s *eft;

	eft = &(pDev->pConf->lcd_effect);
	switch (buf[0]) {
	case 'c':
		if (buf[1] == 'o') {
			t[0] = 100;
			t[1] = 100;
			t[2] = 100;
			ret = sscanf(buf, "coeff %u %u %u",
				&t[0], &t[1], &t[2]);
			set_gamma_coeff(pDev->pConf, t[0], t[1], t[2]);
		} else if (buf[1] == 't') {
			t[0] = 1;
			t[1] = 0;
			ret = sscanf(buf, "ctrl %u %u", &t[0], &t[1]);
			t[0] = (t[0] > 0) ? 1 : 0;
			t[1] = (t[1] > 0) ? 1 : 0;
			eft->gamma_ctrl = ((t[0] << GAMMA_CTRL_EN) |
					(t[1] << GAMMA_CTRL_REV));
			DPRINT("set gamma table enable=%d, reverse=%d\n",
				t[0], t[1]);
			if (write_gamma_table(pDev->pConf, t[0]) == 0)
				DPRINT("write gamma table finished\n");
		}
		break;
	case 'r':
		if (buf[1] == 'e') { /* reset gamma table */
			reset_gamma_table(pDev->pConf);
		} else { /* write R table */
			ret = sscanf(buf, "r %x %x %x %x %x %x %x %x %x", &i,
				&t[0], &t[1], &t[2], &t[3], &t[4], &t[5],
				&t[6], &t[7]);
			if (i < 16) {
				i =  i * 8;
				for (j = 0; j < 8; j++)
					gamma_adj_r_temp[i+j] = t[j];
				DPRINT("write R table: step %u\n", i/8);
			}
		}
		break;
	case 'g': /* write G table */
		ret = sscanf(buf, "g %x %x %x %x %x %x %x %x %x", &i,
			&t[0], &t[1], &t[2], &t[3], &t[4], &t[5], &t[6], &t[7]);
		if (i < 16) {
			i =  i * 8;
			for (j = 0; j < 8; j++)
				gamma_adj_g_temp[i+j] = t[j];
			DPRINT("write G table: step %u\n", i/8);
		}
		break;
	case 'b': /* write B table */
		ret = sscanf(buf, "b %x %x %x %x %x %x %x %x %x", &i,
			&t[0], &t[1], &t[2], &t[3], &t[4], &t[5], &t[6], &t[7]);
		if (i < 16) {
			i =  i * 8;
			for (j = 0; j < 8; j++)
				gamma_adj_b_temp[i+j] = t[j];
			DPRINT("write B table: step %u\n", i/8);
		}
		break;
	case 'w':
		i = 0;
		ret = sscanf(buf, "w %u", &i);
		if (i == 8)
			gamma_table_cacl_8bit(eft);
		else if (i == 10)
			gamma_table_cacl_10bit(eft);
		else
			reset_gamma_table(pDev->pConf);
		break;
	case 'f':
		i = 255;
		if (buf[1] == 'r') {
			ret = sscanf(buf, "fr %u", &i);
			i &= 0xff;
			for (j = 0; j < 256; j++)
				eft->GammaTableR[j] = i<<2;
			set_gamma_coeff(pDev->pConf, 100, 0, 0);
			DPRINT("R fixed value: %u\n", i);
		} else if (buf[1] == 'g') {
			ret = sscanf(buf, "fg %u", &i);
			i &= 0xff;
			for (j = 0; j < 256; j++)
				eft->GammaTableG[j] = i<<2;
			set_gamma_coeff(pDev->pConf, 0, 100, 0);
			DPRINT("G fixed value: %u\n", i);
		} else if (buf[1] == 'b') {
			ret = sscanf(buf, "fb %u", &i);
			i &= 0xff;
			for (j = 0; j < 256; j++)
				eft->GammaTableB[j] = i<<2;
			set_gamma_coeff(pDev->pConf, 0, 0, 100);
			DPRINT("B fixed value: %u\n", i);
		} else {
			ret = sscanf(buf, "fw %u", &i);
			i &= 0xff;
			for (j = 0; j < 256; j++) {
				eft->GammaTableR[j] = i<<2;
				eft->GammaTableG[j] = i<<2;
				eft->GammaTableB[j] = i<<2;
			}
			set_gamma_coeff(pDev->pConf, 100, 100, 100);
			DPRINT("RGB fixed value: %u\n", i);
		}
		break;
	case 't':
		ret = sscanf(buf, "test %u", &i);
		if (pDev->pConf->lcd_effect.gamma_test)
			pDev->pConf->lcd_effect.gamma_test(i);
		else
			DPRINT("gamma test function is null\n");
		break;
	case 'a':
		i = 0;
		ret = sscanf(buf, "auto %u", &i);
		i = (i > 0) ? 1 : 0;
		if (write_gamma_table(pDev->pConf, i) == 0)
			DPRINT("%s gamma table\n", i ? "enable" : "disable");

		lcd_reg_write(VPP_VADJ_CTRL, 0);
		DPRINT("disable video adjust\n");

		if (pDev->pConf->lcd_effect.gamma_test) {
			for (i = 1; i <= 18; i++) {
				pDev->pConf->lcd_effect.gamma_test(i);
				for (j = 0; j < 10; j++) {
					DPRINT("%d\n", (10-j));
					msleep(2000);
				}
			}
			pDev->pConf->lcd_effect.gamma_test(0);
		} else {
			DPRINT("gamma test function is null\n");
		}
		break;
	default:
		DPRINT("wrong format of gamma table writing\n");
	}

	if (ret != 1 || ret != 2)
		return -EINVAL;

	return count;
}

static struct class_attribute aml_lcd_gamma_class_attrs[] = {
	__ATTR(write,  S_IRUGO | S_IWUSR, gamma_help, aml_lcd_gamma_debug),
	__ATTR(read,  S_IRUGO | S_IWUSR, gamma_help, aml_lcd_gamma_read),
	__ATTR(help,  S_IRUGO | S_IWUSR, gamma_help, NULL),
};

static int creat_lcd_gamma_attr(void)
{
	int i;

	gamma_debug_class = class_create(THIS_MODULE, "gamma");
	if (IS_ERR(gamma_debug_class)) {
		DPRINT("create gamma debug class fail\n");
		return -1;
	}

	for (i = 0; i < ARRAY_SIZE(aml_lcd_gamma_class_attrs); i++) {
		if (class_create_file(gamma_debug_class,
			&aml_lcd_gamma_class_attrs[i])) {
			DPRINT("create gamma debug attribute %s fail\n",
				aml_lcd_gamma_class_attrs[i].attr.name);
		}
	}

	return 0;
}

static int remove_lcd_gamma_attr(void)
{
	int i;

	if (gamma_debug_class == NULL)
		return -1;

	for (i = 0; i < ARRAY_SIZE(aml_lcd_gamma_class_attrs); i++) {
		class_remove_file(gamma_debug_class,
			&aml_lcd_gamma_class_attrs[i]);
	}
	class_destroy(gamma_debug_class);
	gamma_debug_class = NULL;

	return 0;
}
#endif
/* ********************************************************* */

/* *********************************************************
 * LCD debug
 * ********************************************************* */
static struct Lcd_Basic_s temp_lcd_basic;
static struct Lcd_Timing_s temp_lcd_timing;
static unsigned short temp_dith_user, temp_dith_ctrl;
static unsigned int temp_vadj_brightness;
static unsigned int temp_vadj_contrast;
static unsigned int temp_vadj_saturation;
static int temp_ttl_rb_swap, temp_ttl_bit_swap;
static int temp_lvds_repack, temp_pn_swap, temp_lvds_vswing;
static unsigned char temp_dsi_lane_num;
static unsigned int temp_dsi_bit_rate_min, temp_dsi_bit_rate_max;
static unsigned int temp_factor_denominator, temp_factor_numerator;
static unsigned char temp_edp_link_rate, temp_edp_lane_count;
static unsigned char temp_edp_vswing, temp_edp_preemphasis;
static unsigned char temp_edp_edid_timing_used;
static unsigned int temp_edp_sync_clock_mode;

static const char *lcd_common_usage_str = {
"Usage:\n"
"    echo 0/1 > status ; 0=disable lcd; 1=enable lcd\n"
"    cat status ; read current lcd status\n"
"\n"
"    echo 0/1 > print ; 0=disable debug print; 1=enable debug print\n"
"    cat print ; read current debug print flag\n"
"\n"
"    echo <cmd> ... > debug ; lcd common debug, use 'cat debug' for help\n"
"    cat debug ; print help information for debug command\n"
"\n"
"    echo <cmd> ... > dsi ; mipi-dsi debug, use 'cat dsi' for help\n"
"    cat dsi ; print help information for dsi command\n"
"\n"
"    echo <cmd> ... > print ; edp debug, use 'cat edp' for help\n"
"    cat print ; print help information for edp command\n"
};

static const char *lcd_usage_str = {
"Usage:\n"
"    echo basic <h_active> <v_active> <h_period> <v_period> > debug ; write lcd basic config\n"
"    echo type <lcd_type> <lcd_bits> > debug ; write lcd type & bits\n"
"    echo clock <lcd_clk> <ss_level> <clk_pol> > debug ; write lcd clk (Hz)\n"
"    echo sync <hs_width> <hs_backporch> <hs_pol> <vs_width> <vs_backporch> <vs_pol> > debug ; write lcd sync timing\n"
"    echo valid <hvsync_valid> <de_valid> > debug ; enable lcd sync signals\n"
"data format:\n"
"    <lcd_type> : 0=mipi, 1=lvds, 2=edp, 3=ttl\n"
"    <lcd_bits> : 6=6bit(RGB18bit), 8=8bit(RGB24bit)\n"
"    <ss_level> : lcd clock spread spectrum level, 0 for disable\n"
"    <xx_pol>   : 0=negative, 1=positive\n"
"    <xx_valid> : 0=disable, 1=enable\n"
"\n"
"    echo ttl <rb_swap> <bit_swap> > debug ; write ttl RGB swap config\n"
"    echo lvds <vswing_level> <lvds_repack> <pn_swap> > debug ; write lvds config\n"
"    echo mdsi <lane_num> <bit_rate_max> <factor> > debug ; write mipi-dsi clock config\n"
"    echo mctl <init_mode> <disp_mode> <lp_clk_auto_stop> <transfer_switch> > debug ; write mipi-dsi control config\n"
"    echo edp <link_rate> <lane_count> <vswing_level> > debug ; write edp lane config\n"
"    echo ectl <edid_timing_used> <sync_clock_mode> > debug; write edp control config\n"
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
"    echo offset <h_sign> <h_offset> <v_sign> <v_offset> > debug ; write ttl display offset\n"
"    echo dither <dither_user> <dither_ctrl> > debug ; write user dither ctrl config\n"
"    echo vadj <brightness> <contrast> <saturation> > debug ; write video adjust config\n"
"data format:\n"
"    <xx_sign>     : 0=positive, 1=negative\n"
"    <dither_user> : 0=disable user control, 1=enable user control\n"
"    <dither_ctrl> : dither ctrl in Hex, such as 0x400 or 0x600\n"
"    <brightness>  : negative 0x1ff~0x101, positive 0x0~0xff, signed value in Hex, default is 0x0\n"
"    <contrast>    : 0x0~0xff, unsigned value in Hex, default is 0x80\n"
"    <saturation>  : 0x0~0x1ff, unsigned value in Hex, default is 0x100\n"
"\n"
"    echo write > debug ; update lcd driver\n"
"    echo reset > debug ; reset lcd config & driver\n"
"    echo read > debug ; read current lcd config\n"
"    echo test <num> > debug ; bist pattern test, 0=pattern off, 1~7=different pattern\n"
};

static ssize_t lcd_debug_common_help(struct class *class,
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", lcd_common_usage_str);
}

static ssize_t lcd_debug_help(struct class *class,
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", lcd_usage_str);
}

static void read_current_lcd_config(struct Lcd_Config_s *pConf)
{
	unsigned lcd_clk;
	int h_adj, v_adj;
	struct DSI_Config_s *dsi_cfg;

	lcd_clk = (pConf->lcd_timing.lcd_clk / 1000);
	h_adj = ((pConf->lcd_timing.h_offset >> 31) & 1);
	v_adj = ((pConf->lcd_timing.v_offset >> 31) & 1);

	pConf->lcd_misc_ctrl.print_version();
	DPRINT("LCD mode: %s, %s %ubit, %ux%u@%u.%uHz\n"
		"lcd_clk           %u.%03uMHz\n"
		"ss_level          %d\n"
		"clk_pol           %d\n\n",
		pConf->lcd_basic.model_name,
		lcd_type_table[pConf->lcd_basic.lcd_type],
		pConf->lcd_basic.lcd_bits,
		pConf->lcd_basic.h_active, pConf->lcd_basic.v_active,
		(pConf->lcd_timing.sync_duration_num / 10),
		(pConf->lcd_timing.sync_duration_num % 10),
		(lcd_clk / 1000), (lcd_clk % 1000),
		((pConf->lcd_timing.clk_ctrl >> CLK_CTRL_SS) & 0xf),
		((pConf->lcd_timing.pol_ctrl >> POL_CTRL_CLK) & 1));

	DPRINT("h_period          %d\n"
		"v_period          %d\n"
		"hs_width          %d\n"
		"hs_backporch      %d\n"
		"hs_pol            %d\n"
		"vs_width          %d\n"
		"vs_backporch      %d\n"
		"vs_pol            %d\n"
		"vs_h_phase        %s%d\n"
		"hvsync_valid      %d\n"
		"de_valid          %d\n"
		"h_offset          %s%d\n"
		"v_offset          %s%d\n\n",
		pConf->lcd_basic.h_period, pConf->lcd_basic.v_period,
		pConf->lcd_timing.hsync_width, pConf->lcd_timing.hsync_bp,
		((pConf->lcd_timing.pol_ctrl >> POL_CTRL_HS) & 1),
		pConf->lcd_timing.vsync_width, pConf->lcd_timing.vsync_bp,
		((pConf->lcd_timing.pol_ctrl >> POL_CTRL_VS) & 1),
		(((pConf->lcd_timing.vsync_h_phase >> 31) & 1) ? "-":""),
		(pConf->lcd_timing.vsync_h_phase & 0xffff),
		pConf->lcd_timing.hvsync_valid, pConf->lcd_timing.de_valid,
		(h_adj ? "-" : ""), (pConf->lcd_timing.h_offset & 0xffff),
		(v_adj ? "-" : ""), (pConf->lcd_timing.v_offset & 0xffff));

	switch (pConf->lcd_basic.lcd_type) {
	case LCD_DIGITAL_TTL:
		DPRINT("rb_swap           %u\n"
			"bit_swap          %u\n\n",
			pConf->lcd_control.ttl_config->rb_swap,
			pConf->lcd_control.ttl_config->bit_swap);
		break;
	case LCD_DIGITAL_LVDS:
		DPRINT("vswing_level      %u\n"
			"lvds_repack       %u\n"
			"pn_swap           %u\n\n",
			pConf->lcd_control.lvds_config->lvds_vswing,
			pConf->lcd_control.lvds_config->lvds_repack,
			pConf->lcd_control.lvds_config->pn_swap);
		break;
	case LCD_DIGITAL_MIPI:
		dsi_cfg = pConf->lcd_control.mipi_config;
		DPRINT("dsi_lane_num      %u\n"
			"dsi_bit_rate      %u.%03uMHz\n"
			"operation_mode    %u(%s), %u(%s)\n"
			"transfer_ctrl     %u, %u\n\n",
			dsi_cfg->lane_num,
			(dsi_cfg->bit_rate / 1000000),
			((dsi_cfg->bit_rate % 1000000) / 1000),
			((dsi_cfg->operation_mode>>BIT_OP_MODE_INIT) & 1),
			(((dsi_cfg->operation_mode>>BIT_OP_MODE_INIT) & 1) ?
				"COMMAND" : "VIDEO"),
			((dsi_cfg->operation_mode>>BIT_OP_MODE_DISP) & 1),
			(((dsi_cfg->operation_mode>>BIT_OP_MODE_DISP) & 1) ?
				"COMMAND" : "VIDEO"),
			((dsi_cfg->transfer_ctrl>>BIT_TRANS_CTRL_CLK) & 1),
			((dsi_cfg->transfer_ctrl>>BIT_TRANS_CTRL_SWITCH) & 3));
		break;
	case LCD_DIGITAL_EDP:
		DPRINT("link_rate         %s\n"
			"lane_count        %u\n"
			"link_adaptive     %u\n"
			"vswing            %u\n"
			"max_lane_count    %u\n"
			"sync_clock_mode   %u\n"
			"EDID timing used  %u\n\n",
			((pConf->lcd_control.edp_config->link_rate == 0) ?
				"1.62G":"2.7G"),
			pConf->lcd_control.edp_config->lane_count,
			pConf->lcd_control.edp_config->link_adaptive,
			pConf->lcd_control.edp_config->vswing,
			pConf->lcd_control.edp_config->max_lane_count,
			pConf->lcd_control.edp_config->sync_clock_mode,
			pConf->lcd_control.edp_config->edid_timing_used);
		break;
	default:
		break;
	}

	if (pConf->lcd_effect.dith_user) {
		DPRINT("dither_ctrl       0x%x\n",
			pConf->lcd_effect.dith_cntl_addr);
	}

	DPRINT("pll_ctrl          0x%08x\n"
		"div_ctrl          0x%08x\n"
		"clk_ctrl          0x%08x\n"
		"video_on_pixel    %d\n"
		"video_on_line     %d\n\n",
		pConf->lcd_timing.pll_ctrl, pConf->lcd_timing.div_ctrl,
		pConf->lcd_timing.clk_ctrl,
		pConf->lcd_timing.video_on_pixel,
		pConf->lcd_timing.video_on_line);
}

static void save_lcd_config(struct Lcd_Config_s *pConf)
{
	struct Lcd_Control_Config_s *ctrl;

	temp_lcd_basic.h_active = pConf->lcd_basic.h_active;
	temp_lcd_basic.v_active = pConf->lcd_basic.v_active;
	temp_lcd_basic.h_period = pConf->lcd_basic.h_period;
	temp_lcd_basic.v_period = pConf->lcd_basic.v_period;
	temp_lcd_basic.lcd_type = pConf->lcd_basic.lcd_type;
	temp_lcd_basic.lcd_bits = pConf->lcd_basic.lcd_bits;

	temp_lcd_timing.pll_ctrl = pConf->lcd_timing.pll_ctrl;
	temp_lcd_timing.div_ctrl = pConf->lcd_timing.div_ctrl;
	temp_lcd_timing.clk_ctrl = pConf->lcd_timing.clk_ctrl;
	temp_lcd_timing.lcd_clk = pConf->lcd_timing.lcd_clk;
	temp_lcd_timing.hsync_width = pConf->lcd_timing.hsync_width;
	temp_lcd_timing.hsync_bp = pConf->lcd_timing.hsync_bp;
	temp_lcd_timing.vsync_width = pConf->lcd_timing.vsync_width;
	temp_lcd_timing.vsync_bp = pConf->lcd_timing.vsync_bp;
	temp_lcd_timing.hvsync_valid = pConf->lcd_timing.hvsync_valid;
	/* temp_lcd_timing.de_hstart = pConf->lcd_timing.de_hstart; */
	/* temp_lcd_timing.de_vstart = pConf->lcd_timing.de_vstart; */
	temp_lcd_timing.de_valid = pConf->lcd_timing.de_valid;
	temp_lcd_timing.h_offset = pConf->lcd_timing.h_offset;
	temp_lcd_timing.v_offset = pConf->lcd_timing.v_offset;
	temp_lcd_timing.pol_ctrl = pConf->lcd_timing.pol_ctrl;

	ctrl = &(pConf->lcd_control);
	switch (pConf->lcd_basic.lcd_type) {
	case LCD_DIGITAL_MIPI:
		temp_dsi_lane_num = ctrl->mipi_config->lane_num;
		temp_dsi_bit_rate_min = ctrl->mipi_config->bit_rate_min;
		temp_dsi_bit_rate_max = ctrl->mipi_config->bit_rate_max;
		temp_factor_denominator = ctrl->mipi_config->factor_denominator;
		temp_factor_numerator = ctrl->mipi_config->factor_numerator;
		break;
	case LCD_DIGITAL_EDP:
		temp_edp_link_rate = ctrl->edp_config->link_rate;
		temp_edp_lane_count = ctrl->edp_config->lane_count;
		temp_edp_vswing = ctrl->edp_config->vswing;
		temp_edp_preemphasis = ctrl->edp_config->preemphasis;
		temp_edp_sync_clock_mode = ctrl->edp_config->sync_clock_mode;
		temp_edp_edid_timing_used = ctrl->edp_config->edid_timing_used;
		break;
	case LCD_DIGITAL_LVDS:
		temp_lvds_repack = ctrl->lvds_config->lvds_repack;
		temp_pn_swap = ctrl->lvds_config->pn_swap;
		temp_lvds_vswing = ctrl->lvds_config->lvds_vswing;
		break;
	case LCD_DIGITAL_TTL:
		temp_ttl_rb_swap = ctrl->ttl_config->rb_swap;
		temp_ttl_bit_swap = ctrl->ttl_config->bit_swap;
		break;
	default:
		break;
	}

	temp_dith_user = pConf->lcd_effect.dith_user;
	temp_dith_ctrl = pConf->lcd_effect.dith_cntl_addr;
	temp_vadj_brightness = pConf->lcd_effect.vadj_brightness;
	temp_vadj_contrast = pConf->lcd_effect.vadj_contrast;
	temp_vadj_saturation = pConf->lcd_effect.vadj_saturation;
}

static void reset_lcd_config(struct Lcd_Config_s *pConf)
{
	struct Lcd_Control_Config_s *ctrl;

	DPRINT("reset lcd config\n");

	_disable_backlight();
	_lcd_module_disable();
	mdelay(200);

	pConf->lcd_basic.h_active = temp_lcd_basic.h_active;
	pConf->lcd_basic.v_active = temp_lcd_basic.v_active;
	pConf->lcd_basic.h_period = temp_lcd_basic.h_period;
	pConf->lcd_basic.v_period = temp_lcd_basic.v_period;
	pConf->lcd_basic.lcd_type = temp_lcd_basic.lcd_type;
	pConf->lcd_basic.lcd_bits = temp_lcd_basic.lcd_bits;

	pConf->lcd_timing.pll_ctrl = temp_lcd_timing.pll_ctrl;
	pConf->lcd_timing.div_ctrl = temp_lcd_timing.div_ctrl;
	pConf->lcd_timing.clk_ctrl = temp_lcd_timing.clk_ctrl;
	pConf->lcd_timing.lcd_clk = temp_lcd_timing.lcd_clk;
	pConf->lcd_timing.hsync_width = temp_lcd_timing.hsync_width;
	pConf->lcd_timing.hsync_bp = temp_lcd_timing.hsync_bp;
	pConf->lcd_timing.vsync_width = temp_lcd_timing.vsync_width;
	pConf->lcd_timing.vsync_bp = temp_lcd_timing.vsync_bp;
	pConf->lcd_timing.hvsync_valid = temp_lcd_timing.hvsync_valid;
	/* pConf->lcd_timing.de_hstart = temp_lcd_timing.de_hstart; */
	/* pConf->lcd_timing.de_vstart = temp_lcd_timing.de_vstart; */
	pConf->lcd_timing.de_valid = temp_lcd_timing.de_valid;
	pConf->lcd_timing.h_offset = temp_lcd_timing.h_offset;
	pConf->lcd_timing.v_offset = temp_lcd_timing.v_offset;
	pConf->lcd_timing.pol_ctrl = temp_lcd_timing.pol_ctrl;

	pConf->lcd_effect.dith_user = temp_dith_user;
	pConf->lcd_effect.dith_cntl_addr = temp_dith_ctrl;
	pConf->lcd_effect.vadj_brightness = temp_vadj_brightness;
	pConf->lcd_effect.vadj_contrast = temp_vadj_contrast;
	pConf->lcd_effect.vadj_saturation = temp_vadj_saturation;

	ctrl = &(pConf->lcd_control);
	switch (pConf->lcd_basic.lcd_type) {
	case LCD_DIGITAL_MIPI:
		ctrl->mipi_config->lane_num = temp_dsi_lane_num;
		ctrl->mipi_config->bit_rate_min = temp_dsi_bit_rate_min;
		ctrl->mipi_config->bit_rate_max = temp_dsi_bit_rate_max;
		ctrl->mipi_config->factor_denominator = temp_factor_denominator;
		ctrl->mipi_config->factor_numerator = temp_factor_numerator;
		break;
	case LCD_DIGITAL_EDP:
		ctrl->edp_config->link_rate = temp_edp_link_rate;
		ctrl->edp_config->lane_count = temp_edp_lane_count;
		ctrl->edp_config->vswing = temp_edp_vswing;
		ctrl->edp_config->preemphasis = temp_edp_preemphasis;
		ctrl->edp_config->sync_clock_mode = temp_edp_sync_clock_mode;
		ctrl->edp_config->edid_timing_used = temp_edp_edid_timing_used;
		break;
	case LCD_DIGITAL_LVDS:
		ctrl->lvds_config->lvds_repack = temp_lvds_repack;
		ctrl->lvds_config->pn_swap = temp_pn_swap;
		ctrl->lvds_config->lvds_vswing = temp_lvds_vswing;
		break;
	case LCD_DIGITAL_TTL:
		ctrl->ttl_config->rb_swap = temp_ttl_rb_swap;
		ctrl->ttl_config->bit_swap = temp_ttl_bit_swap;
		break;
	default:
		break;
	}

	lcd_config_init(pConf);
	_init_vout();
	_lcd_module_enable();
	_enable_backlight();
}

static ssize_t lcd_debug(struct class *class, struct class_attribute *attr,
		const char *buf, size_t count)
{
	unsigned int ret;
	unsigned int t[6];
	struct Lcd_Config_s *Conf;
	struct DSI_Config_s *mconf;
	struct EDP_Config_s *econf;

	Conf = pDev->pConf;
	mconf = pDev->pConf->lcd_control.mipi_config;
	econf = pDev->pConf->lcd_control.edp_config;
	switch (buf[0]) {
	case 'b': /* write basic config */
		t[0] = 1024;
		t[1] = 768;
		t[2] = 1344;
		t[3] = 806;
		ret = sscanf(buf, "basic %d %d %d %d",
			&t[0], &t[1], &t[2], &t[3]);
		Conf->lcd_basic.h_active = t[0];
		Conf->lcd_basic.v_active = t[1];
		Conf->lcd_basic.h_period = t[2];
		Conf->lcd_basic.v_period = t[3];
		DPRINT("h_active=%d, v_active=%d, ", t[0], t[1]);
		DPRINT("h_period=%d, v_period=%d\n", t[2], t[3]);
		break;
	case 't':
		if (buf[1] == 'y') {/* type */
			t[0] = 1;
			t[1] = 6;
			ret = sscanf(buf, "type %d %d", &t[0], &t[1]);
			Conf->lcd_basic.lcd_type = t[0];
			Conf->lcd_basic.lcd_bits = t[1];
			DPRINT("lcd_type: %s, lcd_bits: %d\n",
				lcd_type_table[t[0]], t[1]);
		} else if (buf[1] == 'e') {/* test */
			t[0] = 0;
			ret = sscanf(buf, "test %d", &t[0]);
			if (Conf->lcd_misc_ctrl.lcd_status == 0) {
				DPRINT("lcd is already OFF, ");
				DPRINT("can't display test pattern\n");
			} else {
				Conf->lcd_misc_ctrl.lcd_test(t[0]);
			}
		} else if (buf[1] == 't') {/* ttl */
			t[0] = 0;
			t[1] = 0;
			ret = sscanf(buf, "ttl %d %d", &t[0], &t[1]);
			Conf->lcd_control.ttl_config->rb_swap = t[0];
			Conf->lcd_control.ttl_config->bit_swap = t[1];
			DPRINT("ttl rb_swap: %s, bit_swap: %s\n",
				((t[0] == 0) ? "disable" : "enable"),
				((t[1] == 0) ? "disable" : "enable"));
		}
		break;
	case 'c':
		t[0] = 40000000;
		t[1] = 0;
		t[2] = 0;
		ret = sscanf(buf, "clock %d %d %d", &t[0], &t[1], &t[2]);
		Conf->lcd_timing.lcd_clk = t[0];
		Conf->lcd_timing.clk_ctrl = ((Conf->lcd_timing.clk_ctrl &
			~((1 << CLK_CTRL_AUTO) | (0xf << CLK_CTRL_SS))) |
			((1 << CLK_CTRL_AUTO) | (t[1] << CLK_CTRL_SS)));
		Conf->lcd_timing.pol_ctrl = ((Conf->lcd_timing.pol_ctrl &
			~(1 << POL_CTRL_CLK)) | (t[2] << POL_CTRL_CLK));
		DPRINT("lcd_clk=%dHz, ss_level=%d, clk_pol=%s\n", t[0], t[1],
			((t[2] == 0) ? "negative" : "positive"));
		break;
	case 's':/* sync */
		t[0] = 10;
		t[1] = 60;
		t[2] = 0;
		t[3] = 3;
		t[4] = 20;
		t[5] = 0;
		ret = sscanf(buf, "sync %d %d %d %d %d %d",
			&t[0], &t[1], &t[2], &t[3], &t[4], &t[5]);
		Conf->lcd_timing.hsync_width = t[0];
		Conf->lcd_timing.hsync_bp = t[1];
		Conf->lcd_timing.vsync_width = t[3];
		Conf->lcd_timing.vsync_bp = t[4];
		Conf->lcd_timing.pol_ctrl = ((Conf->lcd_timing.pol_ctrl &
			~((1 << POL_CTRL_HS) | (1 << POL_CTRL_VS))) |
			((t[2] << POL_CTRL_HS) | (t[5] << POL_CTRL_VS)));
		DPRINT("hs_width=%d, hs_bp=%d, hs_pol=%s, ", t[0], t[1],
			((t[2] == 0) ? "negative" : "positive"));
		DPRINT("vs_width=%d, vs_bp=%d, vs_pol=%s\n", t[3], t[4],
			((t[5] == 0) ? "negative" : "positive"));
		break;
	case 'v':
		if (buf[2] == 'l') { /* valid */
			t[0] = 0;
			t[1] = 0;
			t[2] = 1;
			ret = sscanf(buf, "valid %d %d", &t[0], &t[1]);
			Conf->lcd_timing.hvsync_valid = t[0];
			Conf->lcd_timing.de_valid = t[1];
			DPRINT("hvsync: %s, de: %s\n",
				((t[0] == 0) ? "disable" : "enable"),
				((t[1] == 0) ? "disable" : "enable"));
		} else if (buf[2] == 'd') { /* vadj */
			t[0] = 0x0;
			t[1] = 0x80;
			t[2] = 0x100;
			ret = sscanf(buf, "vadj %d %d %d", &t[0], &t[1], &t[2]);
			Conf->lcd_effect.vadj_brightness = t[0];
			Conf->lcd_effect.vadj_contrast = t[1];
			Conf->lcd_effect.vadj_saturation = t[2];
			DPRINT("video adjust: brightness=0x%x, ", t[0]);
			DPRINT("contrast=0x%x, stauration=0x%x\n", t[1], t[2]);
		}
		break;
	case 'o':
		t[0] = 1;
		t[1] = 0;
		t[2] = 1;
		t[3] = 0;
		ret = sscanf(buf, "offset %d %d %d %d",
			&t[0], &t[1], &t[2], &t[3]);
		Conf->lcd_timing.h_offset = ((t[0] << 31) |
					((t[1] & 0xffff) << 0));
		Conf->lcd_timing.v_offset = ((t[2] << 31) |
					((t[3] & 0xffff) << 0));
		DPRINT("h_offset = %s%u, v_offset = %s%u\n",
			(t[0] ? "+" : "-"), (t[1] & 0xffff),
			(t[2] ? "+" : "-"), (t[3] & 0xffff));
		break;
	case 'l': /* write lvds config (lvds_repack, pn_swap) */
		t[0] = 1;
		t[1] = 1;
		t[2] = 0;
		ret = sscanf(buf, "lvds %d %d %d", &t[0], &t[1], &t[2]);
		Conf->lcd_control.lvds_config->lvds_vswing = t[0];
		Conf->lcd_control.lvds_config->lvds_repack = t[1];
		Conf->lcd_control.lvds_config->pn_swap = t[2];
		DPRINT("vswing_level: %u, lvds_repack: %s, rb_swap: %s\n",
			t[0], ((t[1] == 1) ? "VESA mode" : "JEIDA mode"),
			((t[2] == 0) ? "disable" : "enable"));
		break;
	case 'm': /* write mipi config */
		if (buf[1] == 'd') {
			t[0] = 0;
			t[1] = 4;
			t[2] = 0;
			ret = sscanf(buf, "mdsi %d %d %d", &t[0], &t[1], &t[2]);
			mconf->lane_num = (unsigned char)(t[0]);
			mconf->bit_rate_max = t[1]*1000;
			mconf->factor_numerator = t[2];
			mconf->factor_denominator = 10;
			DPRINT("dsi lane_num = %d, bit_rate max=%dMHz, ",
				t[0], t[1]);
			DPRINT("factor=%d\n", mconf->factor_numerator);
		} else if (buf[1] == 'c') {
			t[0] = 1;
			t[1] = 0;
			t[2] = 0;
			t[3] = 0;
			ret = sscanf(buf, "mctl %d %d %d %d",
				&t[0], &t[1], &t[2], &t[3]);
			mconf->operation_mode = ((t[0] << BIT_OP_MODE_INIT) |
					(t[1] << BIT_OP_MODE_DISP));
			mconf->transfer_ctrl = ((t[2] << BIT_TRANS_CTRL_CLK) |
					(t[3] << BIT_TRANS_CTRL_SWITCH));
			DPRINT("dsi operation mode init=%s(%d), ",
				(t[0] ? "command" : "video"), t[0]);
			DPRINT("display=%s(%d), ",
				(t[1] ? "command" : "video"), t[1]);
			DPRINT("lp_clk_auto_stop=%d, transfer_switch=%d\n",
				t[2], t[3]);
		}
		break;
	case 'd':
		if (buf[2] == 't') {
			t[0] = 0;
			t[1] = 0x600;
			ret = sscanf(buf, "dither %d %x", &t[0], &t[1]);
			Conf->lcd_effect.dith_user = t[0];
			Conf->lcd_effect.dith_cntl_addr = t[1];
			DPRINT("dither user_ctrl: %s, 0x%x\n",
				((t[0] == 0) ? "disable" : "enable"), t[1]);
		} else {
			DPRINT("power off lcd\n");
			_disable_backlight();
			Conf->lcd_power_ctrl.power_ctrl(OFF);
		}
		break;
	case 'w': /* update display config */
		if (Conf->lcd_basic.lcd_type == LCD_DIGITAL_MINILVDS) {
			DPRINT("Don't support miniLVDS yet. ");
			DPRINT("Will reset to original lcd config\n");
			reset_lcd_config(Conf);
		} else {
			_lcd_module_disable();
			mdelay(200);
			lcd_config_init(Conf);
			_init_vout();
			_lcd_module_enable();
		}
		break;
	case 'r':
		if (buf[2] == 'a') /* read lcd config */
			read_current_lcd_config(Conf);
		else if (buf[2] == 's') /* reset lcd config */
			reset_lcd_config(Conf);
		break;
	case 'e':
		if (buf[1] == 'n') {
			DPRINT("power on lcd\n");
			_lcd_module_disable();
			mdelay(200);
			_lcd_module_enable();
			_enable_backlight();
		} else if (buf[1] == 'd') {
			t[0] = 1;
			t[1] = 4;
			t[2] = 0;
			ret = sscanf(buf, "edp %u %u %u", &t[0], &t[1], &t[2]);
			econf->link_rate = ((t[0] == 0) ? 0 : 1);
			switch (t[1]) {
			case 1:
			case 2:
				econf->lane_count = t[1];
				break;
			default:
				econf->lane_count = 4;
				break;
			}
			econf->vswing = t[2];
			DPRINT("set edp link_rate = %s, lane_count = %u, ",
				((econf->link_rate == 0) ? "1.62G":"2.7G"),
				econf->lane_count);
			DPRINT("vswing_level = %u\n", econf->vswing);
		} else if (buf[1] == 'c') {
			t[0] = 0;
			t[1] = 1;
			ret = sscanf(buf, "ectl %u %u", &t[0], &t[1]);
			econf->edid_timing_used = ((t[0] == 0) ? 0 : 1);
			econf->sync_clock_mode = ((t[1] == 0) ? 0 : 1);
			DPRINT("set edp edid_timing_used = %u, ",
				econf->edid_timing_used);
			DPRINT("sync_clock_mode = %u\n",
				econf->sync_clock_mode);
			Conf->lcd_misc_ctrl.edp_edid_load();
		} else {
			DPRINT("wrong format of lcd debug command\n");
		}
		break;
	default:
		DPRINT("wrong format of lcd debug command\n");
	}

	if (ret != 1 || ret != 2)
		return -EINVAL;

	return count;
}

static ssize_t lcd_status_read(struct class *class,
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "read lcd status: %s\n",
		(pDev->pConf->lcd_misc_ctrl.lcd_status ? "ON":"OFF"));
}

static ssize_t lcd_status_write(struct class *class,
		struct class_attribute *attr, const char *buf, size_t count)
{
	unsigned int ret;
	unsigned temp;

	temp = 1;
	ret = sscanf(buf, "%d", &temp);
	if (temp) {
		if (pDev->pConf->lcd_misc_ctrl.lcd_status == 0) {
			mutex_lock(&lcd_vout_mutex);
			_lcd_module_enable();
			_enable_backlight();
			mutex_unlock(&lcd_vout_mutex);
		} else {
			DPRINT("lcd is already ON\n");
		}
	} else {
		if (pDev->pConf->lcd_misc_ctrl.lcd_status == 1) {
			mutex_lock(&lcd_vout_mutex);
			_disable_backlight();
			_lcd_module_disable();
			mutex_unlock(&lcd_vout_mutex);
		} else {
			DPRINT("lcd is already OFF\n");
		}
	}

	if (ret != 1 || ret != 2)
		return -EINVAL;

	return count;
}

static ssize_t lcd_print_read(struct class *class,
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "lcd print flag: %u\n", lcd_print_flag);
}

static ssize_t lcd_print_write(struct class *class,
		struct class_attribute *attr, const char *buf, size_t count)
{
	unsigned int ret;

	ret = sscanf(buf, "%u", &lcd_print_flag);
	DPRINT("write lcd print flag: %u\n", lcd_print_flag);

	if (ret != 1 || ret != 2)
		return -EINVAL;

	return count;
}

static struct class_attribute lcd_debug_class_attrs[] = {
	__ATTR(debug,  S_IRUGO | S_IWUSR, lcd_debug_help, lcd_debug),
	__ATTR(help,  S_IRUGO | S_IWUSR, lcd_debug_common_help, NULL),
	__ATTR(status,  S_IRUGO | S_IWUSR, lcd_status_read, lcd_status_write),
	__ATTR(print,  S_IRUGO | S_IWUSR, lcd_print_read, lcd_print_write),
};

static int creat_lcd_class(void)
{
	struct Lcd_Misc_Ctrl_s *misc;

	misc = &(pDev->pConf->lcd_misc_ctrl);
	misc->debug_class = class_create(THIS_MODULE, "lcd");
	if (IS_ERR(misc->debug_class)) {
		DPRINT("create lcd debug class fail\n");
		return -1;
	}
	return 0;
}

static int remove_lcd_class(void)
{
	if (pDev->pConf->lcd_misc_ctrl.debug_class == NULL)
		return -1;

	class_destroy(pDev->pConf->lcd_misc_ctrl.debug_class);
	pDev->pConf->lcd_misc_ctrl.debug_class = NULL;
	return 0;
}

static int creat_lcd_attr(void)
{
	int i;

	if (pDev->pConf->lcd_misc_ctrl.debug_class == NULL) {
		DPRINT("no lcd debug class exist\n");
		return -1;
	}

	for (i = 0; i < ARRAY_SIZE(lcd_debug_class_attrs); i++) {
		if (class_create_file(pDev->pConf->lcd_misc_ctrl.debug_class,
				&lcd_debug_class_attrs[i])) {
			DPRINT("create lcd debug attribute %s fail\n",
				lcd_debug_class_attrs[i].attr.name);
		}
	}

	return 0;
}

static int remove_lcd_attr(void)
{
	int i;

	if (pDev->pConf->lcd_misc_ctrl.debug_class == NULL)
		return -1;

	for (i = 0; i < ARRAY_SIZE(lcd_debug_class_attrs); i++) {
		class_remove_file(pDev->pConf->lcd_misc_ctrl.debug_class,
			&lcd_debug_class_attrs[i]);
	}

	return 0;
}
/* **************************** */

static int lcd_pmu_gpio_name_map_num(const char *name)
{
	int index;

	for (index = 0; index < LCD_POWER_PMU_GPIO_MAX; index++) {
		if (!strcasecmp(name, lcd_power_pmu_gpio_table[index]))
			break;
	}
	return index;
}

static int lcd_cpu_gpio_register_check(const char *name, int total_num)
{
	int i;

	if (total_num == 0)
		return LCD_PWR_CTRL_STEP_MAX;

	if (total_num >= LCD_PWR_CTRL_STEP_MAX)
		total_num = LCD_PWR_CTRL_STEP_MAX;
	for (i = 0; i < total_num; i++) {
		if (!strcasecmp(name, lcd_cpu_gpio[i].name))
			break;
	}
	if (i >= total_num)
		return LCD_PWR_CTRL_STEP_MAX;
	else
		return i;
}

/* return lcd_cpu_gpio struct index */
static int lcd_cpu_gpio_register(const char *name, struct platform_device *pdev)
{
	int index;
	int total_num;
	struct gpio_desc *gdesc;

	total_num = pDev->pConf->lcd_power_ctrl.cpu_gpio_num;
	index = lcd_cpu_gpio_register_check(name, total_num);
	if (index >= LCD_PWR_CTRL_STEP_MAX) { /* cpu gpio didn't register */
		/* register gpio and save index */
		gdesc = lcd_gpio_request(&pdev->dev, name);
		if (IS_ERR(gdesc)) {
			DPRINT("failed to alloc lcd cpu gpio (%s)\n", name);
			index = LCD_PWR_CTRL_STEP_MAX;
		} else {
			index = total_num;
			strcpy(lcd_cpu_gpio[index].name, name);
			lcd_cpu_gpio[index].desc = gdesc;
			pDev->pConf->lcd_power_ctrl.cpu_gpio_num++;
		}
	}
	return index;
}

static void lcd_power_gpio_register(struct Lcd_Power_Ctrl_s *pctrl,
		struct platform_device *pdev)
{
	int i;
	int gpio;
	const char *str = NULL;

	/* initial lcd_cpu_gpio struct num */
	pctrl->cpu_gpio_num = 0;
	for (i = 0; i < LCD_PWR_CTRL_STEP_MAX; i++)
		strcpy(lcd_cpu_gpio[i].name, "n");

	for (i = 0; i < pctrl->power_on_step; i++) {
		str = pctrl->power_on_config[i].gpio_name;
		switch (pctrl->power_on_config[i].type) {
		case LCD_POWER_TYPE_CPU:
			/* for cpu gpio, it is lcd_cpu_gpio struct index */
			gpio = lcd_cpu_gpio_register(str, pdev);
			break;
		case LCD_POWER_TYPE_PMU:
			/* for pmu gpio, it is gpio mapping number */
			gpio = lcd_pmu_gpio_name_map_num(str);
			break;
		default:
			gpio = 0;
			break;
		}
		pctrl->power_on_config[i].gpio = gpio;

		lcd_print("power on step %d: type = %s(%d)\n", i+1,
			lcd_power_type_table[pctrl->power_on_config[i].type],
			pctrl->power_on_config[i].type);
		lcd_print("power on step %d: gpio = %s(%d)\n", i+1,
			((gpio < pctrl->cpu_gpio_num) ?
			lcd_cpu_gpio[gpio].name : "none"),
			pctrl->power_on_config[i].gpio);
		lcd_print("power on step %d: value = %d\n", i+1,
			pctrl->power_on_config[i].value);
		lcd_print("power on step %d: delay = %d\n", i+1,
			pctrl->power_on_config[i].delay);
	}

	for (i = 0; i < pctrl->power_off_step; i++) {
		switch (pctrl->power_off_config[i].type) {
		str = pctrl->power_off_config[i].gpio_name;
		case LCD_POWER_TYPE_CPU:
			/* for cpu gpio, it is lcd_cpu_gpio struct index */
			gpio = lcd_cpu_gpio_register(str, pdev);
			break;
		case LCD_POWER_TYPE_PMU:
			/* for pmu gpio, it is gpio mapping number */
			gpio = lcd_pmu_gpio_name_map_num(str);
			break;
		default:
			gpio = 0;
			break;
		}
		pctrl->power_off_config[i].gpio = gpio;

		lcd_print("power off step %d: type = %s(%d)\n", i+1,
			lcd_power_type_table[pctrl->power_off_config[i].type],
			pctrl->power_off_config[i].type);
		lcd_print("power off step %d: gpio = %s(%d)\n", i+1,
			((gpio < pctrl->cpu_gpio_num) ?
			lcd_cpu_gpio[gpio].name : "none"),
			pctrl->power_off_config[i].gpio);
		lcd_print("power off step %d: value = %d\n", i+1,
			pctrl->power_off_config[i].value);
		lcd_print("power off step %d: delay = %d\n", i+1,
			pctrl->power_off_config[i].delay);
	}
}

#ifdef CONFIG_USE_OF
static void lcd_default_gamma_table(struct Lcd_Config_s *pConf,
		unsigned int flag)
{
	int i;

	if (flag == 0) { /* r */
		for (i = 0; i < 256; i++)
			pConf->lcd_effect.GammaTableR[i] = (i << 2);
	} else if (flag == 1) { /* g */
		for (i = 0; i < 256; i++)
			pConf->lcd_effect.GammaTableG[i] = (i << 2);
	} else if (flag == 2) { /* b */
		for (i = 0; i < 256; i++)
			pConf->lcd_effect.GammaTableB[i] = (i << 2);
	} else if (flag == 3) { /* rgb */
		for (i = 0; i < 256; i++) {
			pConf->lcd_effect.GammaTableR[i] = (i << 2);
			pConf->lcd_effect.GammaTableG[i] = (i << 2);
			pConf->lcd_effect.GammaTableB[i] = (i << 2);
		}
	}
}

#define MOD_LEN_MAX        30
static int _get_lcd_model_timing(struct Lcd_Config_s *pConf,
		struct platform_device *pdev)
{
	int ret = 0;
	const char *str;
	unsigned int val;
	unsigned int para[10];
	struct device_node *m_node;
	phandle fhandle;
	struct DSI_Config_s *mconf;
	struct EDP_Config_s *econf;

	mconf = pConf->lcd_control.mipi_config;
	econf = pConf->lcd_control.edp_config;

	if (pdev->dev.of_node == NULL) {
		DPRINT("dev of_node is null\n");
		return -1;
	}

	ret = of_property_read_u32(pdev->dev.of_node,
		"lcd_model_config", &fhandle);
	m_node = of_find_node_by_phandle(fhandle);
	ret = of_property_read_string(m_node, "model_name", &str);
	if (ret) {
		str = "none";
		DPRINT("lcd: failed to get lcd_model_name!\n");
	}
	pConf->lcd_basic.model_name =
		/* (char *)kmalloc(sizeof(char) * MOD_LEN_MAX, GFP_KERNEL); */
		kmalloc(sizeof(char) * MOD_LEN_MAX, GFP_KERNEL);
	if (pConf->lcd_basic.model_name == NULL) {
		DPRINT("[_get_lcd_model_timing]: Not enough memory\n");
	} else {
		memset(pConf->lcd_basic.model_name, 0, MOD_LEN_MAX);
		strcpy(pConf->lcd_basic.model_name, str);
		DPRINT("load lcd model in dtb: %s\n",
			pConf->lcd_basic.model_name);
	}

	ret = of_property_read_string(m_node, "interface", &str);
	if (ret) {
		DPRINT("failed to get lcd_type!\n");
		str = "invalid";
	}
	for (val = 0; val < LCD_TYPE_MAX; val++) {
		if (!strcasecmp(str, lcd_type_table[val]))
			break;
	}
	pConf->lcd_basic.lcd_type = val;
	lcd_print("lcd_type= %s(%u)\n",
		lcd_type_table[pConf->lcd_basic.lcd_type],
		pConf->lcd_basic.lcd_type);
	ret = of_property_read_u32_array(m_node, "active_area", &para[0], 2);
	if (ret) {
		DPRINT("failed to get active_area\n");
	} else {
		pConf->lcd_basic.h_active_area = para[0];
		pConf->lcd_basic.v_active_area = para[1];
		pConf->lcd_basic.screen_ratio_width = para[0];
		pConf->lcd_basic.screen_ratio_height = para[1];
	}
	lcd_print("h_active_area = %umm, v_active_area =%umm\n",
		pConf->lcd_basic.h_active_area, pConf->lcd_basic.v_active_area);
	ret = of_property_read_u32_array(m_node,
		"lcd_bits_option", &para[0], 2);
	if (ret) {
		DPRINT("failed to get lcd_bits_option\n");
	} else {
		pConf->lcd_basic.lcd_bits = (unsigned short)(para[0]);
		pConf->lcd_basic.lcd_bits_option = (unsigned short)(para[1]);
	}
	lcd_print("lcd_bits = %u, lcd_bits_option = %u\n",
		pConf->lcd_basic.lcd_bits, pConf->lcd_basic.lcd_bits_option);
	ret = of_property_read_u32_array(m_node, "resolution", &para[0], 2);
	if (ret) {
		DPRINT("failed to get resolution\n");
	} else {
		pConf->lcd_basic.h_active = (unsigned short)(para[0]);
		pConf->lcd_basic.v_active = (unsigned short)(para[1]);
	}
	ret = of_property_read_u32_array(m_node, "period", &para[0], 2);
	if (ret) {
		DPRINT("failed to get period\n");
	} else {
		pConf->lcd_basic.h_period = (unsigned short)(para[0]);
		pConf->lcd_basic.v_period = (unsigned short)(para[1]);
	}
	lcd_print("h_active = %u, v_active =%u, ",
		pConf->lcd_basic.h_active, pConf->lcd_basic.v_active);
	lcd_print("h_period = %u, v_period = %u\n",
		pConf->lcd_basic.h_period, pConf->lcd_basic.v_period);
	ret = of_property_read_u32_array(m_node, "clock_hz_pol", &para[0], 2);
	if (ret) {
		DPRINT("failed to get clock_hz_pol\n");
	} else {
		pConf->lcd_timing.lcd_clk = para[0];
		pConf->lcd_timing.pol_ctrl = (para[1] << POL_CTRL_CLK);
	}
	lcd_print("pclk = %uHz, pol=%u\n", pConf->lcd_timing.lcd_clk,
		(pConf->lcd_timing.pol_ctrl >> POL_CTRL_CLK) & 1);
	ret = of_property_read_u32_array(m_node,
		"hsync_width_backporch", &para[0], 2);
	if (ret) {
		DPRINT("failed to get hsync_width_backporch\n");
	} else {
		pConf->lcd_timing.hsync_width = (unsigned short)(para[0]);
		pConf->lcd_timing.hsync_bp = (unsigned short)(para[1]);
	}
	lcd_print("hsync width = %u, backporch = %u\n",
		pConf->lcd_timing.hsync_width, pConf->lcd_timing.hsync_bp);
	ret = of_property_read_u32_array(m_node,
		"vsync_width_backporch", &para[0], 2);
	if (ret) {
		DPRINT("failed to get vsync_width_backporch\n");
	} else {
		pConf->lcd_timing.vsync_width = (unsigned short)(para[0]);
		pConf->lcd_timing.vsync_bp = (unsigned short)(para[1]);
	}
	lcd_print("vsync width = %u, backporch = %u\n",
		pConf->lcd_timing.vsync_width, pConf->lcd_timing.vsync_bp);
	ret = of_property_read_u32_array(m_node,
		"pol_hsync_vsync", &para[0], 2);
	if (ret) {
		DPRINT("failed to get pol_hsync_vsync\n");
	} else {
		pConf->lcd_timing.pol_ctrl = (pConf->lcd_timing.pol_ctrl &
			~((1 << POL_CTRL_HS) | (1 << POL_CTRL_VS))) |
			((para[0] << POL_CTRL_HS) | (para[1] << POL_CTRL_VS));
	}
	lcd_print("pol hsync = %u, vsync = %u\n",
		(pConf->lcd_timing.pol_ctrl >> POL_CTRL_HS) & 1,
		(pConf->lcd_timing.pol_ctrl >> POL_CTRL_VS) & 1);
	ret = of_property_read_u32_array(m_node,
		"vsync_horizontal_phase", &para[0], 2);
	if (ret) {
		lcd_print("failed to get vsync_horizontal_phase\n");
		pConf->lcd_timing.vsync_h_phase = 0;
	} else {
		pConf->lcd_timing.vsync_h_phase = ((para[0] << 31) |
			((para[1] & 0xffff) << 0));
	}
	if (para[0] == 0)
		lcd_print("vsync_horizontal_phase= %d\n", para[1]);
	else
		lcd_print("vsync_horizontal_phase= -%d\n", para[1]);

	if (pConf->lcd_basic.lcd_type == LCD_DIGITAL_MIPI) {
		ret = of_property_read_u32(m_node, "dsi_lane_num", &val);
		if (ret) {
			DPRINT("failed to get dsi_lane_num\n");
			mconf->lane_num = 4;
		} else {
			mconf->lane_num = (unsigned char)val;
		}
		lcd_print("dsi_lane_num= %d\n",  mconf->lane_num);
		ret = of_property_read_u32(m_node, "dsi_bit_rate_max", &val);
		if (ret) {
			DPRINT("failed to get dsi_bit_rate_max\n");
			mconf->bit_rate_max = 0;
		} else {
			mconf->bit_rate_max = val;
		}
		lcd_print("dsi bit_rate max = %dMHz\n", mconf->bit_rate_max);
		ret = of_property_read_u32(m_node,
			"pclk_lanebyteclk_factor", &val);
		if (ret) {
			DPRINT("failed to get pclk_lanebyteclk_factor\n");
			mconf->factor_numerator = 0;
		} else {
			mconf->factor_numerator = val;
		}
		mconf->factor_denominator = 10;
		lcd_print("pclk_lanebyteclk factor= %d\n",
			mconf->factor_numerator);
		ret = of_property_read_u32_array(m_node,
			"dsi_operation_mode", &para[0], 2);
		if (ret) {
			DPRINT("failed to get dsi_operation_mode\n");
			mconf->operation_mode = ((1 << BIT_OP_MODE_INIT) |
				(0 << BIT_OP_MODE_DISP));
		} else {
			mconf->operation_mode = ((para[0] << BIT_OP_MODE_INIT) |
				(para[1] << BIT_OP_MODE_DISP));
		}
		lcd_print("dsi_operation_mode init=%d, display=%d\n",
			(mconf->operation_mode>>BIT_OP_MODE_INIT) & 1,
			(mconf->operation_mode>>BIT_OP_MODE_DISP) & 1);
		ret = of_property_read_u32_array(m_node,
			"dsi_transfer_ctrl", &para[0], 2);
		if (ret) {
			DPRINT("failed to get dsi_transfer_ctrl\n");
			mconf->transfer_ctrl = ((0 << BIT_TRANS_CTRL_CLK) |
				(0 << BIT_TRANS_CTRL_SWITCH));
		} else {
			mconf->transfer_ctrl =
				((para[0] << BIT_TRANS_CTRL_CLK) |
				(para[1] << BIT_TRANS_CTRL_SWITCH));
		}
		lcd_print("dsi_transfer_ctrl clk=%d, switch=%d\n",
			(mconf->transfer_ctrl >> BIT_TRANS_CTRL_CLK) & 1,
			(mconf->transfer_ctrl >> BIT_TRANS_CTRL_SWITCH) & 3);
		/* detect dsi init on table */
		detect_dsi_init_table(m_node, 1);
		/* detect dsi init off table */
		detect_dsi_init_table(m_node, 0);
		ret = of_property_read_u32(m_node, "lcd_extern_init", &val);
		if (ret) {
			DPRINT("failed to get lcd_extern_init\n");
			mconf->lcd_extern_init = 0;
		} else {
			mconf->lcd_extern_init = (unsigned char)(val);
		}
		lcd_print("lcd_extern_init = %d\n",  mconf->lcd_extern_init);
	} else if (pConf->lcd_basic.lcd_type == LCD_DIGITAL_EDP) {
		ret = of_property_read_u32(m_node, "max_lane_count", &val);
		if (ret) {
			DPRINT("failed to get max_lane_count\n");
			econf->max_lane_count = 4;
		} else {
			econf->max_lane_count = (unsigned char)(val);
		}
		lcd_print("max_lane_count = %d\n", econf->max_lane_count);
	}

	return ret;
}

static int _get_lcd_default_config(struct Lcd_Config_s *pConf,
		struct platform_device *pdev)
{
	int ret = 0;
	unsigned int val;
	unsigned int para[5];
	unsigned int *temp; /* num 256 array */
	int i;
	unsigned int gamma_multi = 0;
	char *dft_str;
	struct LVDS_Config_s *lconf;
	struct EDP_Config_s *econf;

	lconf = pConf->lcd_control.lvds_config;
	econf = pConf->lcd_control.edp_config;

	/*temp =(unsigned int *)kmalloc(sizeof(unsigned int)*256, GFP_KERNEL);*/
	temp = kmalloc(sizeof(unsigned int)*256, GFP_KERNEL);
	if (temp == NULL) {
		DPRINT("[_get_lcd_default_config]: Not enough memory\n");
		return -1;
	}
	dft_str = "use default setting";

	/* pdev->dev.of_node = of_find_node_by_name(NULL,"lcd"); */
	if (pdev->dev.of_node == NULL) {
		DPRINT("dev of_node is null\n");
		return -1;
	}
	if (pConf->lcd_basic.lcd_bits_option == 1) {
		ret = of_property_read_u32(pdev->dev.of_node,
			"lcd_bits_user", &val);
		if (ret) {
			DPRINT("failed to get lcd_bits_user, ");
			DPRINT("use panel typical setting\n");
		} else {
			pConf->lcd_basic.lcd_bits = (unsigned short)(val);
			DPRINT("lcd_bits = %u\n", pConf->lcd_basic.lcd_bits);
		}
	}
	/* ttl & lvds config */
	if (pConf->lcd_basic.lcd_type == LCD_DIGITAL_TTL) {
		ret = of_property_read_u32_array(pdev->dev.of_node,
			"ttl_rb_bit_swap", &para[0], 2);
		if (ret) {
			DPRINT("failed to get ttl_rb_bit_swap, %s\n", dft_str);
		} else {
			pConf->lcd_control.ttl_config->rb_swap =
				(unsigned char)(para[0]);
			pConf->lcd_control.ttl_config->bit_swap =
				(unsigned char)(para[1]);
			DPRINT("ttl rb_swap = %u, bit_swap = %u\n",
				pConf->lcd_control.ttl_config->rb_swap,
				pConf->lcd_control.ttl_config->bit_swap);
		}
	} else if (pConf->lcd_basic.lcd_type == LCD_DIGITAL_LVDS) {
		ret = of_property_read_u32(pdev->dev.of_node,
			"lvds_channel_pn_swap", &val);
		if (ret) {
			DPRINT("failed to get lvds_channel_pn_swap, %s\n",
				dft_str);
		} else {
			lconf->pn_swap = val;
			DPRINT("lvds_pn_swap = %u\n", lconf->pn_swap);
		}
	}

	/* recommend setting */
	ret = of_property_read_u32_array(pdev->dev.of_node,
		"valid_hvsync_de", &para[0], 2);
	if (ret) {
		DPRINT("failed to get valid_hvsync_de, %s\n", dft_str);
	} else {
		pConf->lcd_timing.hvsync_valid = (unsigned short)(para[0]);
		pConf->lcd_timing.de_valid = (unsigned short)(para[1]);
		lcd_print("valid hvsync = %u, de = %u\n",
			pConf->lcd_timing.hvsync_valid,
			pConf->lcd_timing.de_valid);
	}
	ret = of_property_read_u32_array(pdev->dev.of_node,
		"hsign_hoffset_vsign_voffset", &para[0], 4);
	if (ret) {
		DPRINT("failed to get hsign_hoffset_vsign_voffset, %s\n",
			dft_str);
		pConf->lcd_timing.h_offset = 0;
		pConf->lcd_timing.v_offset = 0;
	} else {
		pConf->lcd_timing.h_offset = ((para[0] << 31) |
			((para[1] & 0xffff) << 0));
		pConf->lcd_timing.v_offset = ((para[2] << 31) |
			((para[3] & 0xffff) << 0));
		lcd_print("h_offset = %s%u, ",
			(((pConf->lcd_timing.h_offset >> 31) & 1) ? "-" : ""),
			(pConf->lcd_timing.h_offset & 0xffff));
		lcd_print("v_offset = %s%u\n",
			(((pConf->lcd_timing.v_offset >> 31) & 1) ? "-" : ""),
			(pConf->lcd_timing.v_offset & 0xffff));
	}
	ret = of_property_read_u32_array(pdev->dev.of_node,
		"dither_user_ctrl", &para[0], 2);
	if (ret) {
		DPRINT("failed to get dither_user_ctrl, %s\n", dft_str);
		pConf->lcd_effect.dith_user = 0;
	} else {
		pConf->lcd_effect.dith_user = (unsigned short)(para[0]);
		pConf->lcd_effect.dith_cntl_addr = (unsigned short)(para[1]);
		lcd_print("dither_user = %u, dither_ctrl = 0x%x\n",
			pConf->lcd_effect.dith_user,
			pConf->lcd_effect.dith_cntl_addr);
	}
	ret = of_property_read_u32_array(pdev->dev.of_node,
		"vadj_brightness_contrast_saturation", &para[0], 3);
	if (ret) {
		DPRINT("failed to get vadj_brightness_contrast_saturation,");
		DPRINT("%s\n", dft_str);
	} else {
		pConf->lcd_effect.vadj_brightness = para[0];
		pConf->lcd_effect.vadj_contrast = para[1];
		pConf->lcd_effect.vadj_saturation = para[2];
		lcd_print("vadj_brightness = 0x%x, vadj_contrast = 0x%x, ",
			pConf->lcd_effect.vadj_brightness,
			pConf->lcd_effect.vadj_contrast);
		lcd_print("vadj_saturation = 0x%x\n",
			pConf->lcd_effect.vadj_saturation);
	}
	ret = of_property_read_u32_array(pdev->dev.of_node,
		"gamma_en_reverse", &para[0], 2);
	if (ret) {
		DPRINT("failed to get gamma_en_reverse, %s\n", dft_str);
	} else {
		pConf->lcd_effect.gamma_ctrl = ((para[0] << GAMMA_CTRL_EN) |
			(para[1] << GAMMA_CTRL_REV));
		lcd_print("gamma_en = %u, gamma_reverse=%u\n",
			((pConf->lcd_effect.gamma_ctrl >> GAMMA_CTRL_EN) & 1),
			((pConf->lcd_effect.gamma_ctrl >> GAMMA_CTRL_REV) & 1));
	}
	ret = of_property_read_u32_array(pdev->dev.of_node,
		"gamma_multi_rgb_coeff", &para[0], 4);
	if (ret) {
		DPRINT("failed to get gamma_multi_rgb_coeff, %s\n", dft_str);
	} else {
		gamma_multi = para[0];
		pConf->lcd_effect.gamma_r_coeff = (unsigned short)(para[1]);
		pConf->lcd_effect.gamma_g_coeff = (unsigned short)(para[2]);
		pConf->lcd_effect.gamma_b_coeff = (unsigned short)(para[3]);
		lcd_print("gamma_multi = %u, gamma_r_coeff = %u, ",
			gamma_multi, pConf->lcd_effect.gamma_r_coeff);
		lcd_print("gamma_g_coeff = %u, gamma_b_coeff = %u\n",
			pConf->lcd_effect.gamma_g_coeff,
			pConf->lcd_effect.gamma_b_coeff);
	}
	if (gamma_multi == 1) {
		ret = of_property_read_u32_array(pdev->dev.of_node,
			"gamma_table_r", &temp[0], 256);
		if (ret) {
			DPRINT("failed to get gamma_table_r, %s\n", dft_str);
			lcd_default_gamma_table(pConf, 0);
		} else {
			for (i = 0; i < 256; i++)
				pConf->lcd_effect.GammaTableR[i] =
					(unsigned short)(temp[i] << 2);
			lcd_print("load gamma_table_r\n");
		}
		ret = of_property_read_u32_array(pdev->dev.of_node,
			"gamma_table_g", &temp[0], 256);
		if (ret) {
			DPRINT("failed to get gamma_table_g, %s\n", dft_str);
			lcd_default_gamma_table(pConf, 1);
		} else {
			for (i = 0; i < 256; i++)
				pConf->lcd_effect.GammaTableG[i] =
					(unsigned short)(temp[i] << 2);
			lcd_print("load gamma_table_g\n");
		}
		ret = of_property_read_u32_array(pdev->dev.of_node,
			"gamma_table_b", &temp[0], 256);
		if (ret) {
			DPRINT("failed to get gamma_table_b, %s\n", dft_str);
			lcd_default_gamma_table(pConf, 2);
		} else {
			for (i = 0; i < 256; i++)
				pConf->lcd_effect.GammaTableB[i] =
					(unsigned short)(temp[i] << 2);
			lcd_print("load gamma_table_b\n");
		}
	} else {
		ret = of_property_read_u32_array(pdev->dev.of_node,
			"gamma_table", &temp[0], 256);
		if (ret) {
			DPRINT("failed to get gamma_table, %s\n", dft_str);
			lcd_default_gamma_table(pConf, 3);
		} else {
			for (i = 0; i < 256; i++) {
				pConf->lcd_effect.GammaTableR[i] =
					(unsigned short)(temp[i] << 2);
				pConf->lcd_effect.GammaTableG[i] =
					(unsigned short)(temp[i] << 2);
				pConf->lcd_effect.GammaTableB[i] =
					(unsigned short)(temp[i] << 2);
			}
			lcd_print("load gamma_table\n");
		}
	}

	/* default setting */
	ret = of_property_read_u32(pdev->dev.of_node,
		"clock_spread_spectrum", &val);
	if (ret) {
		DPRINT("failed to get clock_spread_spectrum, %s\n", dft_str);
	} else {
		pConf->lcd_timing.clk_ctrl = ((pConf->lcd_timing.clk_ctrl &
			~(0xf << CLK_CTRL_SS)) | (val << CLK_CTRL_SS));
		lcd_print("lcd clock spread spectrum = %u\n",
			(pConf->lcd_timing.clk_ctrl >> CLK_CTRL_SS) & 0xf);
	}
	ret = of_property_read_u32(pdev->dev.of_node,
		"clock_auto_generation", &val);
	if (ret) {
		DPRINT("failed to get clock_auto_generation, %s\n", dft_str);
	} else {
		pConf->lcd_timing.clk_ctrl = ((pConf->lcd_timing.clk_ctrl &
			~(1 << CLK_CTRL_AUTO)) | (val << CLK_CTRL_AUTO));
		lcd_print("lcd clock auto_generation = %u\n",
			(pConf->lcd_timing.clk_ctrl >> CLK_CTRL_AUTO) & 0x1);
	}
	if (((pConf->lcd_timing.clk_ctrl >> CLK_CTRL_AUTO) & 0x1) == 0) {
		ret = of_property_read_u32_array(pdev->dev.of_node,
			"clk_pll_div_clk_ctrl", &para[0], 3);
		if (ret) {
			DPRINT("failed to get clk_pll_div_clk_ctrl, %s\n",
				dft_str);
		} else {
			pConf->lcd_timing.pll_ctrl = para[0];
			pConf->lcd_timing.div_ctrl = para[1];
			pConf->lcd_timing.clk_ctrl = para[2];
			DPRINT("pll_ctrl = 0x%x, div_ctrl = 0x%x, ",
				pConf->lcd_timing.pll_ctrl,
				pConf->lcd_timing.div_ctrl);
			DPRINT("clk_ctrl=0x%x\n",
				(pConf->lcd_timing.clk_ctrl & 0xffff));
		}
	}
	if (pConf->lcd_basic.lcd_type == LCD_DIGITAL_LVDS) {
		ret = of_property_read_u32(pdev->dev.of_node,
			"lvds_vswing", &val);
		if (ret) {
			DPRINT("failed to get lvds_vswing, %s\n", dft_str);
		} else {
			lconf->lvds_vswing = val;
			DPRINT("lvds_vswing level = %u\n", lconf->lvds_vswing);
		}
		ret = of_property_read_u32_array(pdev->dev.of_node,
			"lvds_user_repack", &para[0], 2);
		if (ret) {
			DPRINT("failed to get lvds_user_repack, %s\n", dft_str);
			lconf->lvds_repack_user = 0;
			lconf->lvds_repack = 1;
		} else {
			lconf->lvds_repack_user = para[0];
			lconf->lvds_repack = para[1];
			if (para[0] > 0) {
				DPRINT("lvds_repack = %u\n",
					lconf->lvds_repack);
			} else {
				lcd_print("lvds_repack_user = %u, ",
					lconf->lvds_repack_user);
				lcd_print("lvds_repack = %u\n",
					lconf->lvds_repack);
			}
		}
	} else if (pConf->lcd_basic.lcd_type == LCD_DIGITAL_EDP) {
		ret = of_property_read_u32_array(pdev->dev.of_node,
			"edp_user_link_rate_lane_count", &para[0], 3);
		if (ret) {
			econf->link_user = 0;
			econf->link_rate = 1;
			econf->lane_count = 4;
			DPRINT("failed to get edp_user_link_rate_lane_count, ");
			DPRINT("%s\n", dft_str);
		} else {
			econf->link_user = (unsigned char)(para[0]);
			econf->link_rate = (unsigned char)(para[1]);
			econf->lane_count = (unsigned char)(para[2]);
			if (econf->link_user > 0) {
				DPRINT("edp link_rate = %s, lane_count = %u\n",
				((econf->link_rate == 0) ? "1.62G" : "2.7G"),
				econf->lane_count);
			} else {
				lcd_print("edp user = %u, link_rate = %s, ",
					econf->link_user,
					((econf->link_rate == 0) ?
						"1.62G":"2.7G"));
				lcd_print("lane_count = %u\n",
					econf->lane_count);
			}
		}
		ret = of_property_read_u32_array(pdev->dev.of_node,
			"edp_link_adaptive_vswing", &para[0], 2);
		if (ret) {
			DPRINT("failed to get edp_link_adaptive_vswing, %s\n",
				dft_str);
			econf->link_adaptive = 0;
			econf->vswing = 0;
			econf->preemphasis = 0;
		} else {
			econf->link_adaptive = (unsigned char)(para[0]);
			econf->vswing = (unsigned char)(para[1]);
			econf->preemphasis = 0;
			if (econf->link_adaptive == 0) {
				DPRINT("edp swing_level = %u\n", econf->vswing);
			} else {
				lcd_print("edp link_adaptive = %u, ",
					econf->link_adaptive);
				lcd_print("swing_level = %u\n",
					econf->vswing);
			}
		}
		ret = of_property_read_u32(pdev->dev.of_node,
			"edp_sync_clock_mode", &val);
		if (ret) {
			DPRINT("failed to get edp_sync_clock_mode, %s\n",
				dft_str);
			econf->sync_clock_mode = 1;
		} else {
			econf->sync_clock_mode = (val & 1);
			DPRINT("edp sync_clock_mode = %u\n",
				econf->sync_clock_mode);
		}
		ret = of_property_read_u32(pdev->dev.of_node,
			"edp_edid_timing_used", &val);
		if (ret) {
			DPRINT("failed to get edp_edid_timing_used, %s\n",
				dft_str);
			econf->edid_timing_used = 0;
		} else {
			econf->edid_timing_used = (unsigned char)(val & 1);
			DPRINT("edp edid_timing_used = %u\n",
				econf->edid_timing_used);
		}
	}
	ret = of_property_read_u32_array(pdev->dev.of_node,
		"rgb_base_coeff", &para[0], 2);
	if (ret) {
		DPRINT("failed to get rgb_base_coeff, %s\n", dft_str);
	} else {
		pConf->lcd_effect.rgb_base_addr = (unsigned short)(para[0]);
		pConf->lcd_effect.rgb_coeff_addr = (unsigned short)(para[1]);
		lcd_print("rgb_base = 0x%x, rgb_coeff = 0x%x\n",
			pConf->lcd_effect.rgb_base_addr,
			pConf->lcd_effect.rgb_coeff_addr);
	}
	/* ret = of_property_read_u32_array(pdev->dev.of_node,
		"video_on_pixel_line", &para[0], 2);
	if(ret){
		DPRINT("failed to get video_on_pixel_line, %s\n", dft_str);
	}
	else {
		pConf->lcd_timing.video_on_pixel = (unsigned short)(para[0]);
		pConf->lcd_timing.video_on_line = (unsigned short)(para[1]);
		lcd_print("video_on_pixel = %u, video_on_line = %u\n",
			pConf->lcd_timing.video_on_pixel,
			pConf->lcd_timing.video_on_line);
	} */

	kfree(temp);
	return ret;
}

static int _get_lcd_power_config(struct Lcd_Config_s *pConf,
		struct platform_device *pdev)
{
	int ret = 0;
	const char *str;
	unsigned char propname[20];
	unsigned int para[LCD_PWR_CTRL_STEP_MAX];
	int i;
	int index;
	struct Lcd_Power_Ctrl_s *pctrl;
	struct Lcd_Misc_Ctrl_s *mctrl;

	if (pdev->dev.of_node == NULL) {
		DPRINT("dev of_node is null\n");
		return -1;
	}
	pctrl = &(pConf->lcd_power_ctrl);
	/* lcd power on */
	for (i = 0; i < LCD_PWR_CTRL_STEP_MAX; i++) {
		/*propname = kasprintf(GFP_KERNEL, "power_on_step_%d", i+1); */
		sprintf(propname, "power_on_step_%d", i+1);
		ret = of_property_read_string_index(pdev->dev.of_node,
			propname, 0, &str);
		if (ret) {
			lcd_print("failed to get %s\n", propname);
			break;
		}
		if ((strcasecmp(str, "null") == 0) ||
			((strcasecmp(str, "n") == 0))) {
			break;
		}
		lcd_print("%s 0: %s\n", propname, str);
		for (index = 0; index < LCD_POWER_TYPE_MAX; index++) {
			if (!strcasecmp(str, lcd_power_type_table[index]))
				break;
		}
		pctrl->power_on_config[i].type = index;

		/* only for gpio control */
		if (pctrl->power_on_config[i].type >= LCD_POWER_TYPE_SIGNAL)
			continue;
		ret = of_property_read_string_index(pdev->dev.of_node,
			propname, 1, &str);
		if (ret) {
			DPRINT("failed to get %s index 1\n", propname);
			strcpy(pctrl->power_on_config[i].gpio_name, "none");
		} else {
			strcpy(pctrl->power_on_config[i].gpio_name, str);
		}
		ret = of_property_read_string_index(pdev->dev.of_node,
			propname, 2, &str);
		if (ret) {
			DPRINT("failed to get %s\n", propname);
			pctrl->power_on_config[i].value = LCD_GPIO_INPUT;
		} else {
			if ((strcasecmp(str, "output_low") == 0) ||
				(strcasecmp(str, "0") == 0)) {
				pctrl->power_on_config[i].value =
					LCD_GPIO_OUTPUT_LOW;
			} else if ((strcasecmp(str, "output_high") == 0)
				|| (strcasecmp(str, "1") == 0)) {
				pctrl->power_on_config[i].value =
					LCD_GPIO_OUTPUT_HIGH;
			} else if ((strcasecmp(str, "input") == 0) ||
				(strcasecmp(str, "2") == 0)) {
				pctrl->power_on_config[i].value =
					LCD_GPIO_INPUT;
			}
		}
	}
	pctrl->power_on_step = i;
	lcd_print("lcd_power_on_step = %d\n", pctrl->power_on_step);

	ret = of_property_read_u32_array(pdev->dev.of_node,
		"power_on_delay", &para[0], pctrl->power_on_step);
	if (ret) {
		DPRINT("failed to get power_on_delay\n");
	} else {
		for (i = 0; i < pctrl->power_on_step; i++) {
			pctrl->power_on_config[i].delay =
				(unsigned short)(para[i]);
		}
	}
	/* lcd power off */
	for (i = 0; i < LCD_PWR_CTRL_STEP_MAX; i++) {
		sprintf(propname, "power_off_step_%d", i+1);
		/* propname = kasprintf(GFP_KERNEL, "power_off_step_%d", i+1);*/
		ret = of_property_read_string_index(pdev->dev.of_node,
			propname, 0, &str);
		if (ret) {
			lcd_print("failed to get %s index 0\n", propname);
			break;
		}
		if ((strcasecmp(str, "null") == 0) ||
			((strcasecmp(str, "n") == 0))) {
			break;
		}
		lcd_print("%s 0: %s\n", propname, str);
		for (index = 0; index < LCD_POWER_TYPE_MAX; index++) {
			if (!strcasecmp(str, lcd_power_type_table[index]))
				break;
		}
		pctrl->power_off_config[i].type = index;

		/* only for gpio control */
		if (pctrl->power_off_config[i].type >= LCD_POWER_TYPE_SIGNAL)
			continue;
		ret = of_property_read_string_index(pdev->dev.of_node,
			propname, 1, &str);
		if (ret) {
			DPRINT("failed to get %s index 1\n", propname);
			strcpy(pctrl->power_off_config[i].gpio_name, "none");
		} else {
			strcpy(pctrl->power_off_config[i].gpio_name, str);
		}
		ret = of_property_read_string_index(pdev->dev.of_node,
			propname, 2, &str);
		if (ret) {
			DPRINT("failed to get %s index 2\n", propname);
			pctrl->power_off_config[i].value = LCD_GPIO_INPUT;
		} else {
			if ((strcasecmp(str, "output_low") == 0) ||
				(strcasecmp(str, "0") == 0)) {
				pctrl->power_off_config[i].value =
					LCD_GPIO_OUTPUT_LOW;
			} else if ((strcasecmp(str, "output_high") == 0)
				|| (strcasecmp(str, "1") == 0)) {
				pctrl->power_off_config[i].value =
					LCD_GPIO_OUTPUT_HIGH;
			} else if ((strcasecmp(str, "input") == 0) ||
				(strcasecmp(str, "2") == 0)) {
				pctrl->power_off_config[i].value =
					LCD_GPIO_INPUT;
			}
		}
	}
	pctrl->power_off_step = i;
	lcd_print("lcd_power_off_step = %d\n", pctrl->power_off_step);

	ret = of_property_read_u32_array(pdev->dev.of_node,
		"power_off_delay", &para[0], pctrl->power_off_step);
	if (ret) {
		DPRINT("failed to get power_off_delay\n");
	} else {
		for (i = 0; i < pctrl->power_off_step; i++) {
			pctrl->power_off_config[i].delay =
			(unsigned short)(para[i]);
		}
	}

	mctrl = &pConf->lcd_misc_ctrl;
	mctrl->pin = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(mctrl->pin))
		DPRINT("get lcd ttl ports pinmux error\n");

	if (lcd_chip_type == LCD_CHIP_M6) {
		mctrl->rstc.enct = devm_reset_control_get(&pdev->dev, "enct");
		if (IS_ERR(mctrl->rstc.enct)) {
			DPRINT("get reset control enct error\n");
			ret = -1;
		}
		mctrl->rstc.venct = devm_reset_control_get(&pdev->dev, "venct");
		if (IS_ERR(mctrl->rstc.venct)) {
			DPRINT("get reset control venct error\n");
			ret = -1;
		}
		mctrl->rstc.venct1 = devm_reset_control_get(&pdev->dev,
					"venct1");
		if (IS_ERR(mctrl->rstc.venct1)) {
			DPRINT("get reset control venct1 error\n");
			ret = -1;
		}
	}
	mctrl->rstc.encl = devm_reset_control_get(&pdev->dev, "encl");
	if (IS_ERR(mctrl->rstc.encl)) {
		DPRINT("get reset control encl error\n");
		ret = -1;
	}
	mctrl->rstc.vencl = devm_reset_control_get(&pdev->dev, "vencl");
	if (IS_ERR(mctrl->rstc.vencl)) {
		DPRINT("get reset control vencl error\n");
		ret = -1;
	}
	if ((lcd_chip_type == LCD_CHIP_M8) ||
		(lcd_chip_type == LCD_CHIP_M8M2)) {
		mctrl->rstc.edp = devm_reset_control_get(&pdev->dev, "edp");
		if (IS_ERR(mctrl->rstc.edp)) {
			DPRINT("get reset control edp error\n");
			ret = -1;
		}
	}

	return ret;
}
#endif

static void lcd_config_assign(struct Lcd_Config_s *pConf,
		struct platform_device *pdev)
{
	lcd_power_gpio_register(&pConf->lcd_power_ctrl, pdev);

	/* internal control:
	0=only power for edp_edid_probe, 1=power+signal for normal */
	pConf->lcd_power_ctrl.power_level = 1;
	pConf->lcd_power_ctrl.power_ctrl = lcd_power_ctrl;

	creat_lcd_class();
}

static int lcd_reboot_notifier(struct notifier_block *nb,
		unsigned long state, void *cmd)
{
	lcd_print("[%s]: %lu\n", __func__, state);
	if (pDev->pConf->lcd_misc_ctrl.lcd_status == 0)
		return NOTIFY_DONE;

	_disable_backlight();
	_lcd_module_disable();

	return NOTIFY_OK;
}

#ifdef CONFIG_USE_OF
static const struct of_device_id lcd_dt_match[] = {
	{
		.compatible = "amlogic, lcd",
	},
	{},
};
#else
#define lcd_dt_match NULL
#endif

static struct notifier_block lcd_reboot_nb;
static int lcd_probe(struct platform_device *pdev)
{
	int ret = 0;

	pDev = kmalloc(sizeof(*pDev), GFP_KERNEL);
	if (!pDev) {
		DPRINT("[lcd probe]: Not enough memory\n");
		return -ENOMEM;
	}

#ifdef CONFIG_USE_OF
	pDev->pConf = get_lcd_config();
	_get_lcd_model_timing(pDev->pConf, pdev);
	_get_lcd_default_config(pDev->pConf, pdev);
	_get_lcd_power_config(pDev->pConf, pdev);
#else
	pDev->pConf = (struct Lcd_Config_s *)(pdev->dev.platform_data->pConf);
#endif

	lcd_config_assign(pDev->pConf, pdev);
	lcd_config_probe(pDev->pConf);
	save_lcd_config(pDev->pConf);

	pDev->pConf->lcd_misc_ctrl.print_version();
	lcd_config_init(pDev->pConf);
	_init_vout();

	ret = creat_lcd_attr();
#ifdef CONFIG_AML_LCD_GAMMA_DEBUG
	save_original_gamma(pDev->pConf);
	ret = creat_lcd_gamma_attr();
#endif

	lcd_reboot_nb.notifier_call = lcd_reboot_notifier;
	ret = register_reboot_notifier(&lcd_reboot_nb);
	if (ret)
		DPRINT("notifier register lcd_reboot_notifier fail!\n");

	DPRINT("LCD probe ok\n");
	return 0;
}

static int lcd_remove(struct platform_device *pdev)
{
	unregister_reboot_notifier(&lcd_reboot_nb);

	lcd_config_remove(pDev->pConf);
	remove_lcd_attr();
	remove_lcd_class();
#ifdef CONFIG_AML_LCD_GAMMA_DEBUG
	remove_lcd_gamma_attr();
#endif

	kfree(pDev->pConf->lcd_basic.model_name);
	kfree(pDev);

	return 0;
}

/* device tree */
static struct platform_driver lcd_driver = {
	.probe = lcd_probe,
	.remove = lcd_remove,
	.driver = {
		.name = "mesonlcd",
		.owner = THIS_MODULE,
#ifdef CONFIG_USE_OF
		.of_match_table = lcd_dt_match,
#endif
	},
};

static int __init lcd_init(void)
{
	lcd_print("LCD driver init\n");
	if (platform_driver_register(&lcd_driver)) {
		DPRINT("failed to register lcd driver module\n");
		return -ENODEV;
	}

	return 0;
}

static void __exit lcd_exit(void)
{
	platform_driver_unregister(&lcd_driver);
}

subsys_initcall(lcd_init);
module_exit(lcd_exit);

MODULE_DESCRIPTION("Meson LCD Panel Driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Amlogic, Inc.");
