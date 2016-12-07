/*
 * drivers/amlogic/display/vout/lcd/lcd_control.c
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
#include <asm/fiq.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/reset.h>
#ifdef CONFIG_AML_VPU
#include <linux/amlogic/vpu.h>
#endif
#include <linux/amlogic/vout/vinfo.h>
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
#include "lcd_reg.h"
#include "lcd_config.h"
#include "lcd_control.h"
#include "mipi_dsi_util.h"
#include "edp_drv.h"

#define VPP_OUT_SATURATE        (1 << 0)

static spinlock_t gamma_write_lock;
static spinlock_t lcd_clk_lock;

static struct Lcd_Config_s *lcd_Conf;
static enum Bool_state_e data_status = ON;

#define SS_LEVEL_0_M6      0
#define SS_LEVEL_0_M8      7
#define SS_LEVEL_0_M8B     7
const char *lcd_ss_level_table[] = {
	"0",    /*  0 */ /* m6: level 0 */
	"0.5%", /*  1 */ /* m6: level 1 */
	"1%",   /*  2 */ /* m6: level 2 */
	"2%",   /*  3 */ /* m6: level 3 */
	"3%",   /*  4 */ /* m6: level 4 */
	"4%",   /*  5 */ /* m6: level 5 */
	"5%",   /*  6 */ /* m6: level 6 */
	"0",    /*  7 */ /* m8,m8b,m8m2: level 0 */
	"0.5%", /*  8 */ /* m8,m8b,m8m2: level 1 */
	"1%",   /*  9 */ /* m8,m8b,m8m2: level 2 */
	"1.5%", /* 10 */ /* m8,m8b,m8m2: level 3 */
	"2%",   /* 11 */ /* m8,m8b,m8m2: level 4 */
	"invalid",
};

#define LVDS_VSWING_LEVEL_MAX  5
static unsigned int lvds_vswing_ctrl[][LVDS_VSWING_LEVEL_MAX] = {
	/* vswing_ctrl   level   voltage */
	{ /* m6 */
		0xaf20,  /* 0      0.2V */
		0xaf40,  /* 1      0.3V */
		0xa840,  /* 2      0.4V */
		0xa880,  /* 3      0.5V */
		0xa8c0,  /* 4      0.6V */
	},
	{ /* m8,m8b,m8m2 */
		0x028,   /* 0      0.2V */
		0x048,   /* 1      0.4V */
		0x088,   /* 2      0.6V */
		0x0c8,   /* 3      0.8V */
		0x0f8,   /* 4      1.2V */
	},
};

#define EDP_VSWING_LEVEL_MAX  4
static unsigned int edp_vswing_ctrl[] = {/* [7:4]swing b:800mv, step 50mv */
/* vswing_ctrl   level   voltage */
	0x8018,  /* 0      0.4V */
	0x8088,  /* 1      0.6V */
	0x80c8,  /* 2      0.8V */
	0x80f8,  /* 3      1.2V */
};

#define EDP_PREEM_LEVEL_MAX  3
#if 0
static unsigned int edp_preemphasis_ctrl[] = { /* to do */
/* preemp_ctrl   level   amplitude */
	0x0,     /* 0      0.0db */
	0x0,     /* 1      3.5db */
	0x0,     /* 2      6.0db */
	0x0,     /* 3      9.5db */
};
#endif

static struct lcd_clk_config_s clk_Conf = {/* unit: kHz */
	/* IN-OUT parameters */
	.fin = FIN_FREQ,
	.fout = 0,

	/* pll parameters */
	.pll_m = 0,
	.pll_n = 0,
	.pll_od_sel = 0,
	.pll_level = 0,
	.ss_level = 0,
	.edp_div0 = 0,
	.edp_div1 = 0,
	.div_pre = 0,
	.div_post = 0,
	.xd = 0,
	.pll_fout = 0,

	/* clk path node parameters */
	.pll_m_max = 0,
	.pll_m_min = 0,
	.pll_n_max = 0,
	.pll_n_min = 0,
	.pll_od_sel_max = 0,
	.ss_level_max = 0,
	.div_pre_sel_max = 0,
	.xd_max = 0,
	.pll_ref_fmax = 0,
	.pll_ref_fmin = 0,
	.pll_vco_fmax = 0,
	.pll_vco_fmin = 0,
	.pll_out_fmax = 0,
	.pll_out_fmin = 0,
	.div_pre_in_fmax = 0,
	.div_pre_out_fmax = 0,
	.div_post_out_fmax = 0,
	.xd_out_fmax = 0,
};

struct lcd_clk_config_s *get_lcd_clk_config(void)
{
	return &clk_Conf;
}

static inline void lcd_mdelay(int n)
{
	mdelay(n);
}

static void lcd_ports_ctrl_lvds(enum Bool_state_e status)
{
	unsigned int phy_reg, phy_bit, phy_width;
	unsigned int lane_cnt;

	if (status) {
		/* enable lvds fifo */
		lcd_reg_setb(LVDS_GEN_CNTL, 1, 3, 1);

		switch (lcd_chip_type) {
		case LCD_CHIP_M6:
			phy_reg = LVDS_PHY_CNTL4;
			phy_bit = BIT_PHY_LANE_M6;
			phy_width = PHY_LANE_WIDTH_M6;
			lane_cnt = (lcd_Conf->lcd_basic.lcd_bits == 6) ?
				LVDS_LANE_COUNT_3_M6 : LVDS_LANE_COUNT_4_M6;
			break;
		case LCD_CHIP_M8:
		case LCD_CHIP_M8B:
		case LCD_CHIP_M8M2:
		default:
			phy_reg = HHI_DIF_CSI_PHY_CNTL3;
			phy_bit = BIT_PHY_LANE_M8;
			phy_width = PHY_LANE_WIDTH_M8;
			lane_cnt = (lcd_Conf->lcd_basic.lcd_bits == 6) ?
				LVDS_LANE_COUNT_3_M8 : LVDS_LANE_COUNT_4_M8;
			break;
		}
		lcd_reg_setb(phy_reg, lane_cnt, phy_bit, phy_width);
	} else {
		switch (lcd_chip_type) {
		case LCD_CHIP_M6:
			lcd_reg_setb(LVDS_PHY_CNTL4, 0, 0, 7);
			lcd_reg_setb(LVDS_PHY_CNTL3, 0, 0, 1);
			lcd_reg_setb(LVDS_PHY_CNTL5, 0, 11, 1);
			break;
		case LCD_CHIP_M8:
		case LCD_CHIP_M8B:
		case LCD_CHIP_M8M2:
		default:
			lcd_reg_write(HHI_DIF_CSI_PHY_CNTL3, 0x00200000);
			lcd_reg_write(HHI_DIF_CSI_PHY_CNTL1, 0x0);
			lcd_reg_write(HHI_DIF_CSI_PHY_CNTL2, 0x00060000);
			break;
		}
	}

	lcd_print("%s: %s\n", __func__, (status ? "ON" : "OFF"));
}

static void lcd_ports_ctrl_mipi(enum Bool_state_e status)
{
	unsigned int phy_reg, phy_bit, phy_width;
	unsigned int lane_cnt;

	if (status) {
		phy_reg = HHI_DIF_CSI_PHY_CNTL3;
		phy_bit = BIT_PHY_LANE_M8;
		phy_width = PHY_LANE_WIDTH_M8;
		switch (lcd_Conf->lcd_control.mipi_config->lane_num) {
		case 1:
			lane_cnt = DSI_LANE_COUNT_1;
			break;
		case 2:
			lane_cnt = DSI_LANE_COUNT_2;
			break;
		case 3:
			lane_cnt = DSI_LANE_COUNT_3;
			break;
		case 4:
			lane_cnt = DSI_LANE_COUNT_4;
			break;
		default:
			lane_cnt = 0;
			break;
		}
		lcd_reg_setb(phy_reg, lane_cnt, phy_bit, phy_width);
	} else {
		lcd_reg_write(HHI_DIF_CSI_PHY_CNTL3, 0x00200000);
		lcd_reg_write(HHI_DIF_CSI_PHY_CNTL1, 0x0);
		lcd_reg_write(HHI_DIF_CSI_PHY_CNTL2, 0x00060000);
	}

	lcd_print("%s: %s\n", __func__, (status ? "ON" : "OFF"));
}

static void lcd_ports_ctrl_edp(enum Bool_state_e status)
{
	unsigned int phy_reg, phy_bit, phy_width;
	unsigned int lane_cnt;

	if (status) {
		phy_reg = HHI_DIF_CSI_PHY_CNTL3;
		phy_bit = BIT_PHY_LANE_M8;
		phy_width = PHY_LANE_WIDTH_M8;
		switch (lcd_Conf->lcd_control.edp_config->lane_count) {
		case 1:
			lane_cnt = EDP_LANE_COUNT_1;
			break;
		case 2:
			lane_cnt = EDP_LANE_COUNT_2;
			break;
		case 4:
			lane_cnt = EDP_LANE_COUNT_4;
			break;
		default:
			lane_cnt = 0;
			break;
		}
		lcd_reg_setb(phy_reg, lane_cnt, phy_bit, phy_width);
	} else {
		lcd_reg_write(HHI_DIF_CSI_PHY_CNTL3, 0x00200000);
		lcd_reg_write(HHI_DIF_CSI_PHY_CNTL1, 0x0);
		lcd_reg_write(HHI_DIF_CSI_PHY_CNTL2, 0x00060000);
	}
	lcd_print("%s: %s\n", __func__, (status ? "ON" : "OFF"));
}

/* the string must match pinctrl-names in dts */
const char *lcd_ports_ttl_pinmux_str[] = {
	"ttl_6bit_hvsync_on",      /* 0 */
	"ttl_6bit_de_on",          /* 1 */
	"ttl_6bit_hvsync_de_on",   /* 2 */
	"ttl_8bit_hvsync_on",      /* 3 */
	"ttl_8bit_de_on",          /* 4 */
	"ttl_8bit_hvsync_de_on",   /* 5 */
	"ttl_6bit_hvsync_de_off",  /* 6 */
	"ttl_8bit_hvsync_de_off",  /* 7 */
};

static void lcd_ports_ctrl_ttl(enum Bool_state_e status)
{
	struct pinctrl *pin;
	struct pinctrl_state *s;
	unsigned int pinmux_num;
	int ret;

	if (status) {
		if (lcd_Conf->lcd_basic.lcd_bits == 6) {
			if (lcd_Conf->lcd_timing.de_valid == 0)
				pinmux_num = 0;
			else if (lcd_Conf->lcd_timing.hvsync_valid == 0)
				pinmux_num = 1;
			else
				pinmux_num = 2;
		} else {
			if (lcd_Conf->lcd_timing.de_valid == 0)
				pinmux_num = 3;
			else if (lcd_Conf->lcd_timing.hvsync_valid == 0)
				pinmux_num = 4;
			else
				pinmux_num = 5;
		}
	} else {
		if (lcd_Conf->lcd_basic.lcd_bits == 6)
			pinmux_num = 6;
		else
			pinmux_num = 7;
	}

	/* get pinmux control */
	pin = lcd_Conf->lcd_misc_ctrl.pin;
	if (IS_ERR(pin)) {
		DPRINT("set ttl_ports_ctrl pinmux error\n");
		return;
	}

	/* select pinmux */
	s = pinctrl_lookup_state(pin, lcd_ports_ttl_pinmux_str[pinmux_num]);
	if (IS_ERR(s)) {
		DPRINT("set ttl_ports_ctrl pinmux error\n");
		devm_pinctrl_put(pin); /* pinctrl_put(pin); //release pins */
		return;
	}

	/* set pinmux and lock pins */
	ret = pinctrl_select_state(pin, s);
	if (ret < 0) {
		DPRINT("set ttl_ports_ctrl pinmux error\n");
		devm_pinctrl_put(pin);
		return;
	}
	lcd_print("%s: %s\n", __func__, (status ? "ON" : "OFF"));
}

static int set_control_edp(struct Lcd_Config_s *pConf);
static int lcd_ports_ctrl(enum Bool_state_e status)
{
	int ret = 0;

	if (status) {
		switch (lcd_Conf->lcd_basic.lcd_type) {
		case LCD_DIGITAL_MIPI:
			lcd_ports_ctrl_mipi(ON);
			mipi_dsi_link_on(lcd_Conf);
			break;
		case LCD_DIGITAL_LVDS:
			lcd_ports_ctrl_lvds(ON);
			break;
		case LCD_DIGITAL_EDP:
			lcd_ports_ctrl_edp(ON);
			ret = set_control_edp(lcd_Conf);
			break;
		case LCD_DIGITAL_TTL:
			lcd_ports_ctrl_ttl(ON);
			break;
		default:
			DPRINT("Invalid LCD type\n");
			break;
		}
	} else {
		switch (lcd_Conf->lcd_basic.lcd_type) {
		case LCD_DIGITAL_MIPI:
			mipi_dsi_link_off(lcd_Conf); /* link off command */
			lcd_ports_ctrl_mipi(OFF);
			break;
		case LCD_DIGITAL_LVDS:
			lcd_ports_ctrl_lvds(OFF);
			break;
		case LCD_DIGITAL_EDP:
			ret = edp_link_off(); /* link off command */
			lcd_ports_ctrl_edp(OFF);
			break;
		case LCD_DIGITAL_TTL:
			lcd_ports_ctrl_ttl(OFF);
			break;
		default:
			DPRINT("Invalid LCD type\n");
			break;
		}
	}

	return ret;
}

/* *********************************************************
 * lcd power control
 * ********************************************************* */
static void lcd_cpu_gpio_ctrl(int gpio, int value)
{
	struct gpio_desc *gdesc;

	if (gpio < lcd_Conf->lcd_power_ctrl.cpu_gpio_num) {
		gdesc = lcd_Conf->lcd_power_ctrl.cpu_gpio[gpio].desc;
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

	pctrl = &(lcd_Conf->lcd_power_ctrl);
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
				ret = lcd_ports_ctrl(ON);
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
				ret = lcd_ports_ctrl(OFF);
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

#define GAMMA_RETRY        1000
/* gamma ready check flag */
#define GAMMA_ADDRESS      0
#define GAMMA_WRITE        1
#define GAMMA_READ         2
static int wait_gamma_ready(unsigned int reg, unsigned int flag)
{
	int cnt;
	unsigned int bit;

	cnt = 0;
	if (flag == GAMMA_ADDRESS) /* address */
		bit = ADR_RDY;
	else if (flag == GAMMA_WRITE) /* write */
		bit = WR_RDY;
	else /* read */
		bit = RD_RDY;
	while ((lcd_reg_getb(reg, bit, 1) == 0) && (cnt < GAMMA_RETRY)) {
		udelay(10);
		cnt++;
	}

	if (cnt >= GAMMA_RETRY)
		return -1;
	else
		return 0;
}

static int write_gamma_table(u16 *data, u32 rgb_mask, u16 gamma_coeff,
		u32 gamma_reverse, unsigned int reg_offset)
{
	int i;
	unsigned int temp = 0;
	unsigned int cntl_reg, addr_reg, data_reg;
	unsigned long flags = 0;
	int ret;

	spin_lock_irqsave(&gamma_write_lock, flags);

	cntl_reg = L_GAMMA_CNTL_PORT + reg_offset;
	addr_reg = L_GAMMA_ADDR_PORT + reg_offset;
	data_reg = L_GAMMA_DATA_PORT + reg_offset;

	ret = wait_gamma_ready(cntl_reg, GAMMA_ADDRESS);
	if (ret)
		goto write_gamma_table_err;
	lcd_reg_write(addr_reg,
		(0x1 << H_AUTO_INC) |
		(0x1 << rgb_mask)   |
		(0x0 << HADR));
	if (gamma_reverse == 0) {
		for (i = 0; i < 256; i++) {
			ret = wait_gamma_ready(cntl_reg, GAMMA_WRITE);
			if (ret)
				goto write_gamma_table_err;
			temp = (data[i] * gamma_coeff / 100);
			lcd_reg_write(data_reg, temp);
		}
	} else {
		for (i = 0; i < 256; i++) {
			ret = wait_gamma_ready(cntl_reg, GAMMA_WRITE);
			if (ret)
				goto write_gamma_table_err;
			temp = (data[255-i] * gamma_coeff / 100);
			lcd_reg_write(data_reg, temp);
		}
	}
	ret = wait_gamma_ready(cntl_reg, GAMMA_ADDRESS);
	if (ret)
		goto write_gamma_table_err;
	lcd_reg_write(addr_reg,
		(0x1 << H_AUTO_INC) |
		(0x1 << rgb_mask)   |
		(0x23 << HADR));

write_gamma_table_err:
	spin_unlock_irqrestore(&gamma_write_lock, flags);
	return ret;
}

static void set_gamma_table_lcd(unsigned int gamma_en)
{
	unsigned int reg_offset;
	unsigned short *gtable;
	unsigned int reverse, coeff;
	int ret;

	lcd_print("%s\n", __func__);

	if (lcd_chip_type == LCD_CHIP_M6) {
		if (lcd_Conf->lcd_basic.lcd_type == LCD_DIGITAL_TTL)
			reg_offset = 0x80;
		else
			reg_offset = 0;
	} else {
		reg_offset = 0;
	}
	reverse = ((lcd_Conf->lcd_effect.gamma_ctrl >> GAMMA_CTRL_REV) & 1);
	gtable = lcd_Conf->lcd_effect.GammaTableR;
	coeff = lcd_Conf->lcd_effect.gamma_r_coeff;
	ret = write_gamma_table(gtable, H_SEL_R, coeff, reverse, reg_offset);
	if (ret)
		goto set_gamma_table_lcd_err;

	gtable = lcd_Conf->lcd_effect.GammaTableG;
	coeff = lcd_Conf->lcd_effect.gamma_g_coeff;
	ret = write_gamma_table(gtable, H_SEL_G, coeff, reverse, reg_offset);
	if (ret)
		goto set_gamma_table_lcd_err;

	gtable = lcd_Conf->lcd_effect.GammaTableB;
	coeff = lcd_Conf->lcd_effect.gamma_b_coeff;
	ret = write_gamma_table(gtable, H_SEL_B, coeff, reverse, reg_offset);
	if (ret)
		goto set_gamma_table_lcd_err;
	lcd_reg_setb((L_GAMMA_CNTL_PORT + reg_offset), gamma_en, GAMMA_EN, 1);

set_gamma_table_lcd_err:
	lcd_reg_setb((L_GAMMA_CNTL_PORT + reg_offset), 0, GAMMA_EN, 1);
	DPRINT("[warning]: write gamma table error, gamma table disabled\n");
}

static void lcd_set_tcon_t(struct Lcd_Config_s *pConf)
{
	struct Lcd_Timing_s *tcon_adr;
	unsigned int tcon_pol_ctrl;
	unsigned hs_pol_adj, vs_pol_adj, clk_pol;
	unsigned int gamma_en;

	lcd_print("%s\n", __func__);

	tcon_adr = &(pConf->lcd_timing);
	tcon_pol_ctrl = pConf->lcd_timing.pol_ctrl;
	gamma_en = (pConf->lcd_effect.gamma_ctrl >> GAMMA_CTRL_EN) & 1;
	set_gamma_table_lcd(gamma_en);

	lcd_reg_write(RGB_BASE_ADDR,  pConf->lcd_effect.rgb_base_addr);
	lcd_reg_write(RGB_COEFF_ADDR, pConf->lcd_effect.rgb_coeff_addr);
	if (pConf->lcd_effect.dith_user) {
		lcd_reg_write(DITH_CNTL_ADDR, pConf->lcd_effect.dith_cntl_addr);
	} else {
		if (pConf->lcd_basic.lcd_bits == 8)
			lcd_reg_write(DITH_CNTL_ADDR,  0x400);
		else
			lcd_reg_write(DITH_CNTL_ADDR,  0x600);
	}

	clk_pol = ((tcon_pol_ctrl >> POL_CTRL_CLK) & 1);
	lcd_reg_write(POL_CNTL_ADDR, (clk_pol << CPH1_POL));

	/* 1 for low active, 0 for high active */
	hs_pol_adj = (((tcon_pol_ctrl >> POL_CTRL_HS) & 1) ? 0 : 1);
	vs_pol_adj = (((tcon_pol_ctrl >> POL_CTRL_VS) & 1) ? 0 : 1);

	/* DE signal */
	lcd_reg_write(OEH_HS_ADDR, tcon_adr->de_hs_addr);
	lcd_reg_write(OEH_HE_ADDR, tcon_adr->de_he_addr);
	lcd_reg_write(OEH_VS_ADDR, tcon_adr->de_vs_addr);
	lcd_reg_write(OEH_VE_ADDR, tcon_adr->de_ve_addr);

	/* Hsync signal */
	if (hs_pol_adj == 0) {
		lcd_reg_write(STH1_HS_ADDR, tcon_adr->hs_hs_addr);
		lcd_reg_write(STH1_HE_ADDR, tcon_adr->hs_he_addr);
	} else {
		lcd_reg_write(STH1_HS_ADDR, tcon_adr->hs_he_addr);
		lcd_reg_write(STH1_HE_ADDR, tcon_adr->hs_hs_addr);
	}
	lcd_reg_write(STH1_VS_ADDR, tcon_adr->hs_vs_addr);
	lcd_reg_write(STH1_VE_ADDR, tcon_adr->hs_ve_addr);

	/* Vsync signal */
	lcd_reg_write(STV1_HS_ADDR, tcon_adr->vs_hs_addr);
	lcd_reg_write(STV1_HE_ADDR, tcon_adr->vs_he_addr);
	if (vs_pol_adj == 0) {
		lcd_reg_write(STV1_VS_ADDR, tcon_adr->vs_vs_addr);
		lcd_reg_write(STV1_VE_ADDR, tcon_adr->vs_ve_addr);
	} else {
		lcd_reg_write(STV1_VS_ADDR, tcon_adr->vs_ve_addr);
		lcd_reg_write(STV1_VE_ADDR, tcon_adr->vs_vs_addr);
	}

	lcd_reg_write(INV_CNT_ADDR, 0);
	lcd_reg_write(TCON_MISC_SEL_ADDR, ((1 << STV1_SEL) | (1 << STV2_SEL)));

	if (pConf->lcd_misc_ctrl.vpp_sel)
		lcd_reg_clr_mask(VPP2_MISC, (VPP_OUT_SATURATE));
	else if (lcd_reg_read(VPP_MISC) & VPP_OUT_SATURATE)
		lcd_reg_clr_mask(VPP_MISC, (VPP_OUT_SATURATE));
}

static void lcd_set_tcon_l(struct Lcd_Config_s *pConf)
{
	struct Lcd_Timing_s *tcon_adr;
	unsigned int tcon_pol_ctrl;
	unsigned int hs_pol_adj, vs_pol_adj, clk_pol;
	unsigned int gamma_en;

	lcd_print("%s\n", __func__);

	tcon_adr = &(pConf->lcd_timing);
	tcon_pol_ctrl = pConf->lcd_timing.pol_ctrl;
	gamma_en = (pConf->lcd_effect.gamma_ctrl >> GAMMA_CTRL_EN) & 1;
	set_gamma_table_lcd(gamma_en);

	lcd_reg_write(L_RGB_BASE_ADDR,  pConf->lcd_effect.rgb_base_addr);
	lcd_reg_write(L_RGB_COEFF_ADDR, pConf->lcd_effect.rgb_coeff_addr);
	if (pConf->lcd_effect.dith_user) {
		lcd_reg_write(L_DITH_CNTL_ADDR,
			pConf->lcd_effect.dith_cntl_addr);
	} else {
		if (pConf->lcd_basic.lcd_bits == 8)
			lcd_reg_write(L_DITH_CNTL_ADDR, 0x400);
		else
			lcd_reg_write(L_DITH_CNTL_ADDR, 0x600);
	}

	clk_pol = ((tcon_pol_ctrl >> POL_CTRL_CLK) & 1);
	lcd_reg_write(L_POL_CNTL_ADDR, (clk_pol << CPH1_POL));

	switch (pConf->lcd_basic.lcd_type) {
	case LCD_DIGITAL_MIPI:
		/* 1 for low active, 0 for high active */
		hs_pol_adj = 1;
		vs_pol_adj = 1;
		/* adjust hvsync pol */
		lcd_reg_set_mask(L_POL_CNTL_ADDR, ((0 << DE_POL) |
			(vs_pol_adj << VS_POL) | (hs_pol_adj << HS_POL)));
		/* enable tcon DE, Hsync, Vsync */
		lcd_reg_set_mask(L_POL_CNTL_ADDR, ((1 << TCON_DE_SEL) |
			(1 << TCON_VS_SEL) | (1 << TCON_HS_SEL)));
		break;
	case LCD_DIGITAL_LVDS:
	case LCD_DIGITAL_TTL:
		/* 1 for low active, 0 for high active */
		hs_pol_adj = (((tcon_pol_ctrl >> POL_CTRL_HS) & 1) ? 0 : 1);
		vs_pol_adj = (((tcon_pol_ctrl >> POL_CTRL_VS) & 1) ? 0 : 1);
		/* adjust hvsync pol */
		lcd_reg_set_mask(L_POL_CNTL_ADDR, ((0 << DE_POL) |
			(vs_pol_adj << VS_POL) | (hs_pol_adj << HS_POL)));
		/* enable tcon DE, Hsync, Vsync */
		lcd_reg_set_mask(L_POL_CNTL_ADDR, ((1 << TCON_DE_SEL) |
			(1 << TCON_VS_SEL) | (1 << TCON_HS_SEL)));
		break;
	case LCD_DIGITAL_EDP:
		/* 1 for low active, 0 for high active */
		hs_pol_adj = 0;
		vs_pol_adj = 0;
		/* adjust hvsync pol */
		lcd_reg_set_mask(L_POL_CNTL_ADDR, ((0 << DE_POL) |
			(vs_pol_adj << VS_POL) | (hs_pol_adj << HS_POL)));
		/* enable tcon DE, Hsync, Vsync */
		lcd_reg_set_mask(L_POL_CNTL_ADDR, ((1 << TCON_DE_SEL) |
			(1 << TCON_VS_SEL) | (1 << TCON_HS_SEL)));
		break;
	default:
		hs_pol_adj = 0;
		vs_pol_adj = 0;
		break;
	}

	/* DE signal for TTL m8,m8m2 */
	lcd_reg_write(L_OEH_HS_ADDR, tcon_adr->de_hs_addr);
	lcd_reg_write(L_OEH_HE_ADDR, tcon_adr->de_he_addr);
	lcd_reg_write(L_OEH_VS_ADDR, tcon_adr->de_vs_addr);
	lcd_reg_write(L_OEH_VE_ADDR, tcon_adr->de_ve_addr);
	/* DE signal for TTL m8b */
	lcd_reg_write(L_OEV1_HS_ADDR,  tcon_adr->de_hs_addr);
	lcd_reg_write(L_OEV1_HE_ADDR,  tcon_adr->de_he_addr);
	lcd_reg_write(L_OEV1_VS_ADDR,  tcon_adr->de_vs_addr);
	lcd_reg_write(L_OEV1_VE_ADDR,  tcon_adr->de_ve_addr);

	/* Hsync signal for TTL m8,m8m2 */
	if (hs_pol_adj == 0) {
		lcd_reg_write(L_STH1_HS_ADDR, tcon_adr->hs_hs_addr);
		lcd_reg_write(L_STH1_HE_ADDR, tcon_adr->hs_he_addr);
	} else {
		lcd_reg_write(L_STH1_HS_ADDR, tcon_adr->hs_he_addr);
		lcd_reg_write(L_STH1_HE_ADDR, tcon_adr->hs_hs_addr);
	}
	lcd_reg_write(L_STH1_VS_ADDR, tcon_adr->hs_vs_addr);
	lcd_reg_write(L_STH1_VE_ADDR, tcon_adr->hs_ve_addr);

	/* Vsync signal for TTL m8,m8m2 */
	lcd_reg_write(L_STV1_HS_ADDR, tcon_adr->vs_hs_addr);
	lcd_reg_write(L_STV1_HE_ADDR, tcon_adr->vs_he_addr);
	if (vs_pol_adj == 0) {
		lcd_reg_write(L_STV1_VS_ADDR, tcon_adr->vs_vs_addr);
		lcd_reg_write(L_STV1_VE_ADDR, tcon_adr->vs_ve_addr);
	} else {
		lcd_reg_write(L_STV1_VS_ADDR, tcon_adr->vs_ve_addr);
		lcd_reg_write(L_STV1_VE_ADDR, tcon_adr->vs_vs_addr);
	}

	/* DE signal */
	lcd_reg_write(L_DE_HS_ADDR,    tcon_adr->de_hs_addr);
	lcd_reg_write(L_DE_HE_ADDR,    tcon_adr->de_he_addr);
	lcd_reg_write(L_DE_VS_ADDR,    tcon_adr->de_vs_addr);
	lcd_reg_write(L_DE_VE_ADDR,    tcon_adr->de_ve_addr);

	/* Hsync signal */
	lcd_reg_write(L_HSYNC_HS_ADDR,  tcon_adr->hs_hs_addr);
	lcd_reg_write(L_HSYNC_HE_ADDR,  tcon_adr->hs_he_addr);
	lcd_reg_write(L_HSYNC_VS_ADDR,  tcon_adr->hs_vs_addr);
	lcd_reg_write(L_HSYNC_VE_ADDR,  tcon_adr->hs_ve_addr);

	/* Vsync signal */
	lcd_reg_write(L_VSYNC_HS_ADDR,  tcon_adr->vs_hs_addr);
	lcd_reg_write(L_VSYNC_HE_ADDR,  tcon_adr->vs_he_addr);
	lcd_reg_write(L_VSYNC_VS_ADDR,  tcon_adr->vs_vs_addr);
	lcd_reg_write(L_VSYNC_VE_ADDR,  tcon_adr->vs_ve_addr);

	lcd_reg_write(L_INV_CNT_ADDR, 0);
	lcd_reg_write(L_TCON_MISC_SEL_ADDR,
		((1 << STV1_SEL) | (1 << STV2_SEL)));

	if (pConf->lcd_misc_ctrl.vpp_sel)
		lcd_reg_clr_mask(VPP2_MISC, (VPP_OUT_SATURATE));
	else if (lcd_reg_read(VPP_MISC) & VPP_OUT_SATURATE)
		lcd_reg_clr_mask(VPP_MISC, (VPP_OUT_SATURATE));
}

static void set_tcon_lcd(struct Lcd_Config_s *pConf)
{
	if (lcd_chip_type == LCD_CHIP_M6) {
		if (pConf->lcd_basic.lcd_type == LCD_DIGITAL_TTL)
			lcd_set_tcon_t(pConf);
		else
			lcd_set_tcon_l(pConf);
	} else {
		lcd_set_tcon_l(pConf);
	}
}

static int lcd_pll_wait_lock(unsigned int reg, unsigned int lock_bit)
{
	unsigned int pll_lock;
	int wait_loop = PLL_WAIT_LOCK_CNT;
	int ret = 0;

	do {
		udelay(50);
		pll_lock = lcd_reg_getb(reg, lock_bit, 1);
		wait_loop--;
	} while ((pll_lock == 0) && (wait_loop > 0));
	if (wait_loop == 0)
		ret = -1;
	return ret;
}

static unsigned int ss_ctrl_m6[][3] = {
	/* ctrl2,      ctrl3,       ctrl4 */
	{0x814d3928,  0x6b425012,  0x110}, /* level0, 0%, disable */
	{0x16110696,  0x6d625012,  0x130}, /* level1, 0.5% */
	{0x16110696,  0x4d625012,  0x130}, /* level2, 1% */
	{0x16110696,  0x2d425012,  0x130}, /* level3, 2% */
	{0x16110696,  0x1d425012,  0x130}, /* level4, 3% */
	{0x16110696,  0x0d125012,  0x130}, /* level5, 4% */
	{0x16110696,  0x0e425012,  0x130}, /* level6, 5% */
	{0x814d3928,  0x6b425012,  0x110}, /* disable */
};

static void lcd_set_pll_m6(struct lcd_clk_config_s *cConf)
{
	unsigned int pll_reg;
	int ret;

	lcd_print("%s\n", __func__);
	pll_reg = ((cConf->pll_od_sel << LCD_PLL_OD_M6) |
		(cConf->pll_n << LCD_PLL_N_M6)          |
		(cConf->pll_m << LCD_PLL_M_M6));

	lcd_reg_setb(HHI_VIID_PLL_CNTL, 1, LCD_PLL_RST_M6, 1);
	lcd_reg_write(HHI_VIID_PLL_CNTL, pll_reg | (1 << LCD_PLL_RST_M6));
	lcd_reg_write(HHI_VIID_PLL_CNTL2, 0x814d3928);
	lcd_reg_write(HHI_VIID_PLL_CNTL3, 0x6b425012);
	lcd_reg_write(HHI_VIID_PLL_CNTL4, 0x110);
	lcd_reg_write(HHI_VIID_PLL_CNTL, pll_reg);
	ret = lcd_pll_wait_lock(HHI_VIID_PLL_CNTL, LCD_PLL_LOCK_M6);
	if (ret)
		DPRINT("[error]: lcd vid2_pll lock failed\n");

	if (cConf->ss_level == 0)
		return;
	/* spread_spectrum */
	lcd_reg_write(HHI_VIID_PLL_CNTL2, ss_ctrl_m6[cConf->ss_level][0]);
	lcd_reg_write(HHI_VIID_PLL_CNTL3, ss_ctrl_m6[cConf->ss_level][1]);
	lcd_reg_write(HHI_VIID_PLL_CNTL4, ss_ctrl_m6[cConf->ss_level][2]);
}

static void lcd_set_pll_m8(struct lcd_clk_config_s *cConf)
{
	unsigned int pll_reg;
	unsigned int pll_ctrl2, pll_ctrl3, pll_ctrl4, od_fb;
	int ret;

	lcd_print("%s\n", __func__);
	pll_reg = ((1 << LCD_PLL_EN_M8) |
		(cConf->pll_od_sel << LCD_PLL_OD_M8) |
		(cConf->pll_n << LCD_PLL_N_M8) |
		(cConf->pll_m << LCD_PLL_M_M8));

	if (cConf->pll_frac == 0)
		pll_ctrl2 = 0x0421a000;
	else
		pll_ctrl2 = (0x0431a000 | cConf->pll_frac);

	pll_ctrl4 = (0xd4000d67 & ~((1<<13) | (0xf<<14) | (0xf<<18)));
	switch (cConf->ss_level) {
	case 1: /* 0.5% */
		pll_ctrl4 |= ((1<<13) | (2<<18) | (1<<14));
		break;
	case 2: /* 1% */
		pll_ctrl4 |= ((1<<13) | (1<<18) | (1<<14));
		break;
	case 3: /* 1.5% */
		pll_ctrl4 |= ((1<<13) | (8<<18) | (1<<14));
		break;
	case 4: /* 2% */
		pll_ctrl4 |= ((1<<13) | (0<<18) | (1<<14));
		break;
	case 0:
	default:
		cConf->ss_level = 0;
		break;
	}

	switch (cConf->pll_level) {
	case 1: /* <=1.7G */
		/* special adjust for M8M2 vid2 pll 1.2G lock failed */
		if (lcd_chip_type == LCD_CHIP_M8M2)
			pll_ctrl2 &= ~(0xf << 12);
		pll_ctrl3 = (cConf->ss_level > 0) ? 0xca7e3823 : 0xca45b823;
		od_fb = 0;
		break;
	case 2: /* 1.7G~2.0G */
		pll_ctrl2 |= (1 << 19); /* special adjust */
		pll_ctrl3 = (cConf->ss_level > 0) ? 0xca7e3823 : 0xca49b823;
		od_fb = 1;
		break;
	case 3: /* 2.0G~2.5G */
		pll_ctrl3 = (cConf->ss_level > 0) ? 0xca7e3823 : 0xca49b823;
		od_fb = 1;
		break;
	case 4: /* >=2.5G */
		pll_ctrl3 = (cConf->ss_level > 0) ? 0xca7e3823 : 0xce49c022;
		od_fb = 1;
		break;
	default:
		pll_ctrl3 = 0xca7e3823;
		od_fb = 0;
		break;
	}
	lcd_reg_setb(HHI_VID_PLL_CNTL5, 1, 16, 1); /* enable bandgap */
	lcd_reg_write(HHI_VID2_PLL_CNTL2, pll_ctrl2);
	lcd_reg_write(HHI_VID2_PLL_CNTL3, pll_ctrl3);
	lcd_reg_write(HHI_VID2_PLL_CNTL4, pll_ctrl4);
	/* bit[8] od_fb */
	lcd_reg_write(HHI_VID2_PLL_CNTL5, (0x00700001 | (od_fb << 8)));
	lcd_reg_write(HHI_VID2_PLL_CNTL, pll_reg | (1 << LCD_PLL_RST_M8));
	lcd_reg_write(HHI_VID2_PLL_CNTL, pll_reg);
	ret = lcd_pll_wait_lock(HHI_VID2_PLL_CNTL, LCD_PLL_LOCK_M8);
	if (ret) { /* adjust pll for lock */
		if (cConf->pll_level == 2) {
			/* change setting if can't lock */
			lcd_reg_setb(HHI_VID2_PLL_CNTL2, 1, 18, 1);
			lcd_reg_setb(HHI_VID2_PLL_CNTL, 1, LCD_PLL_RST_M8, 1);
			udelay(5);
			lcd_reg_setb(HHI_VID2_PLL_CNTL, 0, LCD_PLL_RST_M8, 1);
			DPRINT("change setting for lcd pll stability\n");
		}
		ret = lcd_pll_wait_lock(HHI_VID2_PLL_CNTL, LCD_PLL_LOCK_M8);
	}
	if (ret)
		DPRINT("[error]: lcd vid2_pll lock failed\n");
}


static void lcd_set_pll_m8b(struct lcd_clk_config_s *cConf)
{
	unsigned int pll_reg;
	unsigned int pll_ctrl2, pll_ctrl3, pll_ctrl4, od_fb;
	int ret;

	lcd_print("%s\n", __func__);
	pll_reg = ((1 << LCD_PLL_EN_M8B) |
		(cConf->pll_od_sel << LCD_PLL_OD_M8B) |
		(cConf->pll_n << LCD_PLL_N_M8B) |
		(cConf->pll_m << LCD_PLL_M_M8B));

	if (cConf->pll_frac == 0)
		pll_ctrl2 = 0x59c88000;
	else
		pll_ctrl2 = (0x59c8c000 | cConf->pll_frac);

	pll_ctrl4 = (0x00238100 & ~((1<<9) | (0xf<<4) | (0xf<<0)));
	switch (cConf->ss_level) {
	case 1: /* 0.5% */
		pll_ctrl4 |= ((1<<9) | (2<<4) | (1<<0));
		break;
	case 2: /* 1% */
		pll_ctrl4 |= ((1<<9) | (1<<4) | (1<<0));
		break;
	case 3: /* 1.5% */
		pll_ctrl4 |= ((1<<9) | (8<<4) | (1<<0));
		break;
	case 4: /* 2% */
		pll_ctrl4 |= ((1<<9) | (0<<4) | (1<<0));
		break;
	case 0:
	default:
		cConf->ss_level = 0;
		break;
	}

	switch (cConf->pll_level) {
	case 1: /* <=1.7G */
		pll_ctrl3 = (cConf->ss_level > 0) ? 0xca7e3823 : 0xca49b022;
		od_fb = 0;
		break;
	case 2: /* 1.7G~2.0G */
		pll_ctrl2 |= (1 << 19); /* special adjust */
		pll_ctrl3 = (cConf->ss_level > 0) ? 0xca7e3823 : 0xca493822;
		od_fb = 1;
		break;
	case 3: /* 2.0G~2.5G */
		pll_ctrl3 = (cConf->ss_level > 0) ? 0xca7e3823 : 0xca493822;
		od_fb = 1;
		break;
	case 4: /* >=2.5G */
		pll_ctrl3 = (cConf->ss_level > 0) ? 0xca7e3823 : 0xce49c022;
		od_fb = 1;
		break;
	default:
		pll_ctrl3 = 0xca7e3823;
		od_fb = 0;
		break;
	}
	lcd_reg_setb(HHI_VID2_PLL_CNTL2, 1, 16, 1); /* enable ext LDO */
	lcd_reg_write(HHI_VID_PLL_CNTL2, pll_ctrl2);
	lcd_reg_write(HHI_VID_PLL_CNTL3, pll_ctrl3);
	/* cntl4 bit[24] od_fb */
	lcd_reg_write(HHI_VID_PLL_CNTL4, (pll_ctrl4 | (od_fb << 24)));
	lcd_reg_write(HHI_VID_PLL_CNTL5, 0x00012385);
	lcd_reg_write(HHI_VID_PLL_CNTL, pll_reg | (1 << LCD_PLL_RST_M8B));
	lcd_reg_write(HHI_VID_PLL_CNTL, pll_reg);
	ret = lcd_pll_wait_lock(HHI_VID_PLL_CNTL, LCD_PLL_LOCK_M8B);
	if (ret) { /* adjust pll for can't lock */
		if (cConf->pll_level == 2) {
			/* change setting if can't lock */
			lcd_reg_setb(HHI_VID_PLL_CNTL2, 1, 12, 1);
			lcd_reg_setb(HHI_VID_PLL_CNTL, 1, LCD_PLL_RST_M8B, 1);
			udelay(5);
			lcd_reg_setb(HHI_VID_PLL_CNTL, 0, LCD_PLL_RST_M8B, 1);
			DPRINT("change setting for lcd pll stability\n");
		}
		ret = lcd_pll_wait_lock(HHI_VID_PLL_CNTL, LCD_PLL_LOCK_M8B);
	}
	if (ret)
		DPRINT("[error]: lcd vid2_pll lock failed\n");
}

static void lcd_set_vclk(int lcd_type, struct lcd_clk_config_s *cConf)
{
	unsigned int div_reg;

	lcd_print("%s\n", __func__);

	/* select vid2_pll and enable clk */
	div_reg = ((1 << DIV_CLK_IN_EN) | (1 << DIV_CLK_SEL) | (0x3 << 0));
	div_reg |= (cConf->div_pre << DIV_PRE_SEL); /* set div_pre */
	/* set div_post */
	if (cConf->div_post > 0) {/* div_post > 1 */
		div_reg |= ((1 << DIV_POST_SEL) |
			(cConf->div_post << DIV_POST_TCNT));
	}
	if (lcd_type == LCD_DIGITAL_LVDS) /* enable lvds_clk */
		div_reg |= (1 << DIV_LVDS_CLK_EN);

	lcd_reg_setb(HHI_VIID_CLK_CNTL, 0, VCLK2_EN, 1);
	udelay(5);

	if ((lcd_chip_type == LCD_CHIP_M8) ||
		(lcd_chip_type == LCD_CHIP_M8M2)) {
		/* select vclk from pll */
		switch (lcd_type) {
		case LCD_DIGITAL_MIPI:
			/* pll_out mux to mipi-dsi phy & vid2_pll */
			lcd_reg_setb(HHI_VID2_PLL_CNTL5, 3, 23, 3);
			lcd_reg_setb(HHI_DSI_LVDS_EDP_CNTL1, 0, 4, 1);
			break;
		case LCD_DIGITAL_EDP:
			/* reset edp tx phy */
			lcd_reg_write(HHI_EDP_TX_PHY_CNTL0, (1 << 16));

			/* pll_out mux to edp phy */
			lcd_reg_setb(HHI_VID2_PLL_CNTL5, 4, 23, 3);
			lcd_reg_setb(HHI_DSI_LVDS_EDP_CNTL1, 1, 4, 1);

			/* enable edp phy channel & serializer clk,
			and release reset */
			lcd_reg_write(HHI_EDP_TX_PHY_CNTL0,
				((0xf << 0) | (1 << 4)));
			/* set edptx_phy_clk_div0, div1 */
			lcd_reg_setb(HHI_EDP_TX_PHY_CNTL0,
				cConf->edp_div0, 20, 4);
			lcd_reg_setb(HHI_EDP_TX_PHY_CNTL0,
				cConf->edp_div1, 24, 3);
			/* enable divider N, for vid2_pll_in */
			lcd_reg_setb(HHI_EDP_TX_PHY_CNTL0, 1, 5, 1);
			break;
		case LCD_DIGITAL_LVDS:
		case LCD_DIGITAL_TTL:
		default:
			/* pll_out mux to vid2_pll */
			lcd_reg_setb(HHI_VID2_PLL_CNTL5, 2, 23, 3);
			lcd_reg_setb(HHI_DSI_LVDS_EDP_CNTL1, 0, 4, 1);
			break;
		}
		udelay(10);
	}

	/* pll_div2 */
	lcd_reg_write(HHI_VIID_DIVIDER_CNTL, div_reg);
	lcd_reg_setb(HHI_VIID_DIVIDER_CNTL, 1, DIV_POST_SOFT_RST, 1);
	lcd_reg_setb(HHI_VIID_DIVIDER_CNTL, 1, DIV_PRE_SOFT_RST, 1);
	lcd_reg_setb(HHI_VIID_DIVIDER_CNTL, 0, DIV_POST_RST, 1);
	lcd_reg_setb(HHI_VIID_DIVIDER_CNTL, 0, DIV_PRE_RST, 1);
	udelay(5);
	lcd_reg_setb(HHI_VIID_DIVIDER_CNTL, 0, DIV_PRE_SOFT_RST, 1);
	lcd_reg_setb(HHI_VIID_DIVIDER_CNTL, 0, DIV_POST_SOFT_RST, 1);
	lcd_reg_setb(HHI_VIID_DIVIDER_CNTL, 1, DIV_PRE_RST, 1);
	lcd_reg_setb(HHI_VIID_DIVIDER_CNTL, 1, DIV_POST_RST, 1);
	udelay(5);

	/* setup the XD divider value */
	lcd_reg_setb(HHI_VIID_CLK_DIV, (cConf->xd-1), VCLK2_XD, 8);
	udelay(5);
	/* Bit[18:16] - v2_cntl_clk_in_sel */
	lcd_reg_setb(HHI_VIID_CLK_CNTL, 4, VCLK2_CLK_IN_SEL, 3);
	lcd_reg_setb(HHI_VIID_CLK_CNTL, 1, VCLK2_EN, 1);
	udelay(2);

	if (lcd_chip_type == LCD_CHIP_M6) {
		if (lcd_type == LCD_DIGITAL_TTL) {
			/* [23:20] enct_clk_sel, select vclk2_div1 */
			lcd_reg_setb(HHI_VID_CLK_DIV, 8, ENCT_CLK_SEL, 4);
		} else {
			/* [15:12] encl_clk_sel, select vclk2_div1 */
			lcd_reg_setb(HHI_VIID_CLK_DIV, 8, ENCL_CLK_SEL, 4);
		}
	} else {
		/* [15:12] encl_clk_sel, select vclk2_div1 */
		lcd_reg_setb(HHI_VIID_CLK_DIV, 8, ENCL_CLK_SEL, 4);
	}
	/* release vclk2_div_reset and enable vclk2_div */
	lcd_reg_setb(HHI_VIID_CLK_DIV, 1, VCLK2_XD_EN, 2);
	udelay(5);

	lcd_reg_setb(HHI_VIID_CLK_CNTL, 1, VCLK2_DIV1_EN, 1);
	lcd_reg_setb(HHI_VIID_CLK_CNTL, 1, VCLK2_SOFT_RST, 1);
	udelay(10);
	lcd_reg_setb(HHI_VIID_CLK_CNTL, 0, VCLK2_SOFT_RST, 1);
	udelay(5);

	/* enable CTS_ENCL clk gate, new added in m8b & m8m2 */
	if ((lcd_chip_type == LCD_CHIP_M8M2) || (lcd_chip_type == LCD_CHIP_M8B))
		lcd_reg_setb(HHI_VID_CLK_CNTL2, 1, ENCL_GATE_VCLK, 1);
}

static void set_clk_lcd(struct Lcd_Config_s *pConf)
{
	unsigned long flags = 0;
	spin_lock_irqsave(&lcd_clk_lock, flags);

	lcd_print("%s\n", __func__);

	switch (lcd_chip_type) {
	case LCD_CHIP_M6:
		lcd_set_pll_m6(&clk_Conf);
		break;
	case LCD_CHIP_M8:
	case LCD_CHIP_M8M2:
		lcd_set_pll_m8(&clk_Conf);
		break;
	case LCD_CHIP_M8B:
		lcd_set_pll_m8b(&clk_Conf);
		break;
	default:
		break;
	}
	lcd_set_vclk(pConf->lcd_basic.lcd_type, &clk_Conf);

	spin_unlock_irqrestore(&lcd_clk_lock, flags);
}

static void lcd_set_enct(struct Lcd_Config_s *pConf)
{
	lcd_print("%s\n", __func__);

	lcd_reg_write(ENCT_VIDEO_EN, 0);
#ifdef CONFIG_AM_TV_OUTPUT2
	if (pConf->lcd_misc_ctrl.vpp_sel) {
		/* viu2 select enct */
		lcd_reg_setb(VPU_VIU_VENC_MUX_CTRL, 3, 2, 2);
	} else {
		/* viu1 select enct */
		lcd_reg_setb(VPU_VIU_VENC_MUX_CTRL, 3, 0, 2);
	}
#else
	/* viu1, viu2 select enct */
	lcd_reg_setb(VPU_VIU_VENC_MUX_CTRL, 0xf, 0, 4);
#endif
	lcd_reg_write(ENCT_VIDEO_MODE,        0);
	lcd_reg_write(ENCT_VIDEO_MODE_ADV,    0x0008); /* Sampling rate: 1 */

	lcd_reg_write(ENCT_VIDEO_FILT_CTRL,    0x1000); /* bypass filter */

	lcd_reg_write(ENCT_VIDEO_MAX_PXCNT,   pConf->lcd_basic.h_period - 1);
	lcd_reg_write(ENCT_VIDEO_MAX_LNCNT,   pConf->lcd_basic.v_period - 1);

	lcd_reg_write(ENCT_VIDEO_HAVON_BEGIN, pConf->lcd_timing.video_on_pixel);
	lcd_reg_write(ENCT_VIDEO_HAVON_END,   pConf->lcd_basic.h_active - 1 +
		pConf->lcd_timing.video_on_pixel);
	lcd_reg_write(ENCT_VIDEO_VAVON_BLINE, pConf->lcd_timing.video_on_line);
	lcd_reg_write(ENCT_VIDEO_VAVON_ELINE, pConf->lcd_basic.v_active - 1 +
		pConf->lcd_timing.video_on_line);

	lcd_reg_write(ENCT_VIDEO_HSO_BEGIN,   15);
	lcd_reg_write(ENCT_VIDEO_HSO_END,     31);
	lcd_reg_write(ENCT_VIDEO_VSO_BEGIN,   15);
	lcd_reg_write(ENCT_VIDEO_VSO_END,     31);
	lcd_reg_write(ENCT_VIDEO_VSO_BLINE,   0);
	lcd_reg_write(ENCT_VIDEO_VSO_ELINE,   2);

	/* bit[0] 1:RGB, 0:YUV */
	lcd_reg_write(ENCT_VIDEO_RGBIN_CTRL,  (1 << 0));

	lcd_reg_write(ENCT_VIDEO_EN,           1); /* enable enct */
}

static void lcd_set_encl(struct Lcd_Config_s *pConf)
{
	lcd_print("%s\n", __func__);

	lcd_reg_write(ENCL_VIDEO_EN, 0);
#ifdef CONFIG_AM_TV_OUTPUT2
	if (pConf->lcd_misc_ctrl.vpp_sel) {
		/* viu2 select encl */
		lcd_reg_setb(VPU_VIU_VENC_MUX_CTRL, 0, 2, 2);
	} else {
		/* viu1 select encl */
		lcd_reg_setb(VPU_VIU_VENC_MUX_CTRL, 0, 0, 2);
	}
#else
	/* viu1, viu2 select encl */
	lcd_reg_setb(VPU_VIU_VENC_MUX_CTRL, 0, 0, 4);
#endif

	lcd_reg_write(ENCL_VIDEO_MODE,        0);
	lcd_reg_write(ENCL_VIDEO_MODE_ADV,    0x8); /* Sampling rate: 1 */

	lcd_reg_write(ENCL_VIDEO_FILT_CTRL,   0x1000); /* bypass filter */

	lcd_reg_write(ENCL_VIDEO_MAX_PXCNT,   pConf->lcd_basic.h_period - 1);
	lcd_reg_write(ENCL_VIDEO_MAX_LNCNT,   pConf->lcd_basic.v_period - 1);

	lcd_reg_write(ENCL_VIDEO_HAVON_BEGIN, pConf->lcd_timing.video_on_pixel);
	lcd_reg_write(ENCL_VIDEO_HAVON_END,   pConf->lcd_basic.h_active - 1 +
		pConf->lcd_timing.video_on_pixel);
	lcd_reg_write(ENCL_VIDEO_VAVON_BLINE, pConf->lcd_timing.video_on_line);
	lcd_reg_write(ENCL_VIDEO_VAVON_ELINE, pConf->lcd_basic.v_active - 1  +
		pConf->lcd_timing.video_on_line);

	lcd_reg_write(ENCL_VIDEO_HSO_BEGIN,   10); /* hs_hs_addr); */
	lcd_reg_write(ENCL_VIDEO_HSO_END,     16); /* hs_he_addr); */
	lcd_reg_write(ENCL_VIDEO_VSO_BEGIN,   pConf->lcd_timing.vso_hstart);
	lcd_reg_write(ENCL_VIDEO_VSO_END,     pConf->lcd_timing.vso_hstart);
	lcd_reg_write(ENCL_VIDEO_VSO_BLINE,   pConf->lcd_timing.vso_vstart);
	lcd_reg_write(ENCL_VIDEO_VSO_ELINE,   pConf->lcd_timing.vso_vstart + 2);

	/* bit[0] 1:RGB, 0:YUV */
	lcd_reg_write(ENCL_VIDEO_RGBIN_CTRL,  (1 << 0)); /* (1<<1) | (1<<0)) */

	lcd_reg_write(ENCL_VIDEO_EN,          1); /* enable encl */
}

static void set_venc_lcd(struct Lcd_Config_s *pConf)
{
	if (lcd_chip_type == LCD_CHIP_M6) {
		if (pConf->lcd_basic.lcd_type == LCD_DIGITAL_TTL)
			lcd_set_enct(pConf);
		else
			lcd_set_encl(pConf);
	} else {
		lcd_set_encl(pConf);
	}
}

static void set_lvds_clk_util(void)
{
	if (lcd_chip_type == LCD_CHIP_M6) {
		/* lvds_div_phy_clk_en  = phy_clk_cntl[10]; */
		/* lvds_div_div2_sel    = phy_clk_cntl[9]; */
		/* lvds_div_sel         = phy_clk_cntl[8:7]; */
		/* lvds_div_tcnt        = phy_clk_cntl[6:4]; */
		lcd_reg_write(LVDS_PHY_CLK_CNTL,
			((1 << 10) | /* enable div_phy_clk */
			(0 << 9) |   /* div2_en */
			(1 << 7) |   /* divn_sel */
			(6 << 4)));  /* divn_tcnt */
	} else {
		/* *** enable all serializers, divide by 7 *** */
		/* wire    [4:0]   cntl_ser_en         = control[20:16]; */
		/* wire            cntl_prbs_en        = control[13]; */
		/* wire            cntl_prbs_err_en    = control[12]; */
		/* wire    [1:0]   cntl_mode_set_high  = control[11:10]; */
		/* wire    [1:0]   cntl_mode_set_low   = control[9:8]; */
		/*  */
		/* wire    [1:0]   fifo_clk_sel        = control[7:6] */
		/*  */
		/* wire            mode_port_rev       = control[4]; */
		/* wire            mode_bit_rev        = control[3]; */
		/* wire            mode_inv_p_n        = control[2]; */
		/* wire            phy_clk_en          = control[1]; */
		/* wire            soft_reset_int      = control[0]; */
		lcd_reg_write(HHI_LVDS_TX_PHY_CNTL0, (0x1f << 16) | (0x1 << 6));
	}

	/* lvds_gen_cntl   <= {10'h0,    // [15:4] unused */
	/*			2'h1,    // [5:4] divide by 7 in the PHYrr */
	/*			1'b0,    // [3] fifo_en */
	/*			1'b0,    // [2] wr_bist_gate */
	/*			2'b00};  // [1:0] fifo_wr mode */
	/* FIFO_CLK_SEL = 1; // div7 */
	lcd_reg_setb(LVDS_GEN_CNTL, 1, 4, 2); /* lvds fifo clk div 7 */

	if (lcd_chip_type == LCD_CHIP_M6) {
		/* lvds phy div reset */
		lcd_reg_setb(LVDS_PHY_CLK_CNTL, 0, 15, 1);
		udelay(5);
		/* Release lvds phy div reset */
		lcd_reg_setb(LVDS_PHY_CLK_CNTL, 1, 15, 1);
	} else { /* ***** new added **** */
		/* lvds phy div reset */
		lcd_reg_setb(HHI_LVDS_TX_PHY_CNTL0, 1, 0, 1);
		udelay(5);
		/* Release lvds div reset */
		lcd_reg_setb(HHI_LVDS_TX_PHY_CNTL0, 0, 0, 1);
	}
}

static void set_control_lvds(struct Lcd_Config_s *pConf)
{
	unsigned int lvds_repack, pn_swap, bit_num;
	unsigned int data32;

	lcd_print("%s\n", __func__);
	set_lvds_clk_util(); /* set lvds phy & fifo divider */

	lcd_reg_setb(LVDS_GEN_CNTL, 0, 3, 1); /* disable lvds fifo */

	data32 = (0x00 << LVDS_blank_data_r) |
		 (0x00 << LVDS_blank_data_g) |
		 (0x00 << LVDS_blank_data_b);
	lcd_reg_write(LVDS_BLANK_DATA_HI, (data32 >> 16));
	lcd_reg_write(LVDS_BLANK_DATA_LO, (data32 & 0xffff));

	if (lcd_chip_type == LCD_CHIP_M6)
		lcd_reg_setb(MLVDS_CONTROL, 0, mLVDS_en, 1); /* disable mlvds */

	lvds_repack = (pConf->lcd_control.lvds_config->lvds_repack) & 0x1;
	pn_swap = (pConf->lcd_control.lvds_config->pn_swap) & 0x1;
	switch (pConf->lcd_basic.lcd_bits) {
	case 10:
		bit_num = 0;
		break;
	case 8:
		bit_num = 1;
		break;
	case 6:
		bit_num = 2;
		break;
	case 4:
		bit_num = 3;
		break;
	default:
		bit_num = 1;
		break;
	}

	lcd_reg_write(LVDS_PACK_CNTL_ADDR,
		(lvds_repack << 0) | /* repack */
		(0 << 2) |       /* odd_even */
		(0 << 3) |       /* reserve */
		(0 << 4) |       /* lsb first */
		(pn_swap << 5) | /* pn swap */
		(0 << 6) |       /* dual port */
		(0 << 7) |       /* use tcon control */
		(bit_num << 8) | /* 0:10bits, 1:8bits, 2:6bits, 3:4bits */
		(0 << 10) |      /* r_select  //0:R, 1:G, 2:B, 3:0 */
		(1 << 12) |      /* g_select  //0:R, 1:G, 2:B, 3:0 */
		(2 << 14));      /* b_select  //0:R, 1:G, 2:B, 3:0; */

	lcd_reg_setb(LVDS_GEN_CNTL, 1, 0, 1);  /* fifo enable */
	/* lcd_reg_setb(LVDS_GEN_CNTL, 1, 3, 1);  //enable fifo */
}

static void set_control_mipi(struct Lcd_Config_s *pConf)
{
	lcd_print("%s\n", __func__);
	mipi_dsi_host_on(pConf);
}

/* ************************************************** */
/* for edp control */
/* ************************************************** */
void edp_phy_config_update(unsigned char vswing, unsigned char preemp)
{
	vswing =  get_edp_config_index(&edp_vswing_table[0], vswing);
	vswing = (vswing >= EDP_VSWING_LEVEL_MAX) ?
		(EDP_VSWING_LEVEL_MAX - 1) : vswing;
	preemp =  get_edp_config_index(&edp_preemphasis_table[0], preemp);
	preemp = (preemp >= EDP_PREEM_LEVEL_MAX) ?
		(EDP_PREEM_LEVEL_MAX - 1) : preemp;

	lcd_reg_write(HHI_DIF_CSI_PHY_CNTL1, edp_vswing_ctrl[vswing]);
	DPRINT("edp link adaptive update: ");
	DPRINT("vswing_level=%u, preemphasis_level=%u\n", vswing, preemp);
}

static void edp_edid_pre_power_ctrl(enum Bool_state_e status)
{
	if (status) {
		/* change power_level to 0(only power) for edp_edid_probe */
		lcd_Conf->lcd_power_ctrl.power_level = 0;
		lcd_Conf->lcd_power_ctrl.power_ctrl(ON);
	} else {
		lcd_Conf->lcd_power_ctrl.power_ctrl(OFF);
		/* change power_level back to 1(power+signal) for normal */
		lcd_Conf->lcd_power_ctrl.power_level = 1;
	}
}

static void edp_apb_clk_prepare(void)
{
	/* prepare EDP_APB_CLK to access EDP registers */
	/* fclk_div5---fixed 510M, div to 170M, edp apb clk */
	if (lcd_chip_type == LCD_CHIP_M8)
		lcd_reg_write(HHI_EDP_APB_CLK_CNTL, (1 << 7) | (2 << 0));
	else if (lcd_chip_type == LCD_CHIP_M8M2)
		lcd_reg_write(HHI_EDP_APB_CLK_CNTL_M8M2, (1 << 7) | (2 << 0));
}

static void lcd_config_edp_edid_load(void)
{
	if (lcd_chip_valid_check() == FALSE)
		return;
	if (lcd_Conf->lcd_control.edp_config->edid_timing_used == 0)
		return;

	if (lcd_Conf->lcd_misc_ctrl.lcd_status == 0) {
		/* enable edp power, phy and tx */
		edp_apb_clk_prepare();

		/* dphy select by interface */
		lcd_reg_write(HHI_DSI_LVDS_EDP_CNTL0, LCD_DIGITAL_EDP);
		/* [7:4]swing b:800mv, step 50mv */
		lcd_reg_write(HHI_DIF_CSI_PHY_CNTL1, 0x8018);
		lcd_reg_write(HHI_DIF_CSI_PHY_CNTL2,
			((0x6 << 16) | (0xf5d7 << 0)));
		lcd_reg_write(HHI_DIF_CSI_PHY_CNTL3,
			((0xc2b2 << 16) | (0x600 << 0))); /* 0xd2b0fe00 */
		edp_edid_pre_power_ctrl(ON);
		/* enable AUX channel */
		lcd_reg_setb(HHI_DIF_CSI_PHY_CNTL3,
			EDP_LANE_AUX, BIT_PHY_LANE_M8,
			PHY_LANE_WIDTH_M8);
		edp_edid_pre_enable();

		edp_edid_timing_probe(lcd_Conf);

		/* disable edp tx, phy and power */
		edp_edid_pre_disable();
		lcd_reg_write(HHI_DIF_CSI_PHY_CNTL3, 0x00200000);
		lcd_reg_write(HHI_DIF_CSI_PHY_CNTL1, 0x0);
		lcd_reg_write(HHI_DIF_CSI_PHY_CNTL2, 0x00060000);
		edp_edid_pre_power_ctrl(OFF);
	} else {
		edp_edid_timing_probe(lcd_Conf);
	}
}
/* ************************************************** */

static int set_control_edp(struct Lcd_Config_s *pConf)
{
	int ret = 0;

	lcd_print("%s\n", __func__);

	ret = edp_host_on(pConf);
	return ret;
}

static void set_control_ttl(struct Lcd_Config_s *pConf)
{
	unsigned int rb_swap, bit_swap;
	unsigned int cntl_reg;

	rb_swap = (unsigned int)(pConf->lcd_control.ttl_config->rb_swap);
	bit_swap = (unsigned int)(pConf->lcd_control.ttl_config->bit_swap);

	if (lcd_chip_type == LCD_CHIP_M6)
		cntl_reg = DUAL_PORT_CNTL_ADDR;
	else
		cntl_reg = L_DUAL_PORT_CNTL_ADDR;
	lcd_reg_write(cntl_reg, (rb_swap << RGB_SWP) | (bit_swap << BIT_SWP));
}

static void init_phy_lvds(struct Lcd_Config_s *pConf)
{
	unsigned int swing_level;
	lcd_print("%s\n", __func__);

	swing_level = pConf->lcd_control.lvds_config->lvds_vswing;
	if (swing_level >= LVDS_VSWING_LEVEL_MAX)
		swing_level = (LVDS_VSWING_LEVEL_MAX - 1);
	if (lcd_chip_type == LCD_CHIP_M6) {
		lcd_reg_write(LVDS_PHY_CNTL3, 0xee1);
		lcd_reg_write(LVDS_PHY_CNTL4, 0);

		lcd_reg_write(LVDS_PHY_CNTL5, lvds_vswing_ctrl[0][swing_level]);

		lcd_reg_write(LVDS_PHY_CNTL0, 0x001f);
		lcd_reg_write(LVDS_PHY_CNTL1, 0xffff);

		lcd_reg_write(LVDS_PHY_CNTL6, 0xcccc);
		lcd_reg_write(LVDS_PHY_CNTL7, 0xcccc);
		lcd_reg_write(LVDS_PHY_CNTL8, 0xcccc);
	} else {
		lcd_reg_write(LVDS_SER_EN, 0xfff); /* Enable the serializers */

		lcd_reg_write(LVDS_PHY_CNTL0, 0xffff);
		lcd_reg_write(LVDS_PHY_CNTL1, 0xff00);
		lcd_reg_write(LVDS_PHY_CNTL4, 0x007f);

		lcd_reg_write(HHI_DIF_CSI_PHY_CNTL1,
			lvds_vswing_ctrl[1][swing_level]);
		lcd_reg_write(HHI_DIF_CSI_PHY_CNTL2, 0x000665b7);
		lcd_reg_write(HHI_DIF_CSI_PHY_CNTL3, 0x84070000);
	}
}

static void init_phy_mipi(struct Lcd_Config_s *pConf)
{
	lcd_print("%s\n", __func__);

	/* swap mipi channels, only for m8baby */
	if (lcd_chip_type == LCD_CHIP_M8B)
		lcd_reg_setb(HHI_DSI_LVDS_EDP_CNTL1, 1, 4, 1);

	lcd_reg_write(HHI_DIF_CSI_PHY_CNTL1, 0x8);/* DIF_REF_CTL0 */
	/* DIF_REF_CTL2:31-16bit, DIF_REF_CTL1:15-0bit */
	lcd_reg_write(HHI_DIF_CSI_PHY_CNTL2, (0x3e << 16) | (0xa5b8 << 0));
	/* DIF_TX_CTL1:31-16bit, DIF_TX_CTL0:15-0bit */
	lcd_reg_write(HHI_DIF_CSI_PHY_CNTL3, (0x26e0 << 16) | (0x459 << 0));
}

static void init_phy_edp(struct Lcd_Config_s *pConf)
{
	unsigned char swing_level;
	lcd_print("%s\n", __func__);

	swing_level = pConf->lcd_control.edp_config->vswing;
	if (swing_level >= EDP_VSWING_LEVEL_MAX)
		swing_level = (EDP_VSWING_LEVEL_MAX - 1);

	lcd_reg_write(HHI_DIF_CSI_PHY_CNTL1, edp_vswing_ctrl[swing_level]);
	lcd_reg_write(HHI_DIF_CSI_PHY_CNTL2, ((0x6 << 16) | (0xf5d7 << 0)));
	/* 0xd2b0fe00 */
	lcd_reg_write(HHI_DIF_CSI_PHY_CNTL3, ((0xc2b2 << 16) | (0x600 << 0)));
}

static void init_dphy(struct Lcd_Config_s *pConf)
{
	enum Lcd_Type_e lcd_type;

	lcd_type = pConf->lcd_basic.lcd_type;
	/* dphy select by interface */
	switch (lcd_type) {
	case LCD_DIGITAL_MIPI:
		lcd_reg_write(HHI_DSI_LVDS_EDP_CNTL0, lcd_type);
		init_phy_mipi(pConf);
		break;
	case LCD_DIGITAL_EDP:
		lcd_reg_write(HHI_DSI_LVDS_EDP_CNTL0, lcd_type);
		init_phy_edp(pConf);
		break;
	case LCD_DIGITAL_LVDS:
		lcd_reg_write(HHI_DSI_LVDS_EDP_CNTL0, lcd_type);
		init_phy_lvds(pConf);
		break;
	default:
		break;
	}
}

static void set_video_adjust(struct Lcd_Config_s *pConf)
{
	unsigned int brightness, contrast, saturation;

	brightness = pConf->lcd_effect.vadj_brightness;
	contrast = pConf->lcd_effect.vadj_contrast;
	saturation = pConf->lcd_effect.vadj_saturation;

	lcd_print("video adjust: ");
	lcd_print("brightness = 0x%x, contrast = 0x%x, saturation = 0x%x\n",
		brightness, contrast, saturation);
	lcd_reg_write(VPP_VADJ2_Y, (brightness << 8) | (contrast << 0));
	lcd_reg_write(VPP_VADJ2_MA_MB, (saturation << 16));
	lcd_reg_write(VPP_VADJ2_MC_MD, (saturation << 0));
	lcd_reg_write(VPP_VADJ_CTRL, 0xf); /* enable video adjust */
}

static void lcd_switch_clk_gate(enum Bool_state_e status)
{
	struct Lcd_Clk_Gate_Ctrl_s *rstc;

	rstc = &lcd_Conf->lcd_misc_ctrl.rstc;
	if (status) {
		switch (lcd_Conf->lcd_basic.lcd_type) {
		case LCD_DIGITAL_EDP:
			reset_control_deassert(rstc->edp);
			reset_control_deassert(rstc->encl);
			reset_control_deassert(rstc->vencl);
			break;
		case LCD_DIGITAL_MIPI:
		case LCD_DIGITAL_LVDS:
			reset_control_deassert(rstc->encl);
			reset_control_deassert(rstc->vencl);
			break;
		case LCD_DIGITAL_TTL:
			if (lcd_chip_type == LCD_CHIP_M6) {
				reset_control_deassert(rstc->enct);
				reset_control_deassert(rstc->venct);
				reset_control_deassert(rstc->venct1);
			} else {
				reset_control_deassert(rstc->encl);
				reset_control_deassert(rstc->vencl);
			}
			break;
		default:
			break;
		}
	} else {
		switch (lcd_Conf->lcd_basic.lcd_type) {
		case LCD_DIGITAL_EDP:
			reset_control_assert(rstc->edp);
			reset_control_assert(rstc->encl);
			reset_control_assert(rstc->vencl);
			break;
		case LCD_DIGITAL_MIPI:
		case LCD_DIGITAL_LVDS:
			reset_control_assert(rstc->encl);
			reset_control_assert(rstc->vencl);
			break;
		case LCD_DIGITAL_TTL:
			if (lcd_chip_type == LCD_CHIP_M6) {
				reset_control_assert(rstc->enct);
				reset_control_assert(rstc->venct);
				reset_control_assert(rstc->venct1);
			} else {
				reset_control_assert(rstc->encl);
				reset_control_assert(rstc->vencl);
			}
			break;
		default:
			break;
		}
	}
}

#ifdef CONFIG_AML_VPU
static void lcd_vpu_power_ctrl(int status)
{
	if (lcd_chip_type < LCD_CHIP_M8)
		return;

	if (status) {
		request_vpu_clk_vmod(lcd_Conf->lcd_timing.lcd_clk, VPU_VENCL);
		switch_vpu_mem_pd_vmod(VPU_VENCL, VPU_MEM_POWER_ON);
	} else {
		switch_vpu_mem_pd_vmod(VPU_VENCL, VPU_MEM_POWER_DOWN);
		release_vpu_clk_vmod(VPU_VENCL);
	}
}
#endif

static void _init_lcd_driver(struct Lcd_Config_s *pConf)
{
	enum Lcd_Type_e lcd_type;
	unsigned char ss_level, ss_base;

	pConf->lcd_misc_ctrl.print_version();
	lcd_type = pConf->lcd_basic.lcd_type;
	ss_level = (pConf->lcd_timing.clk_ctrl >> CLK_CTRL_SS) & 0xf;
	switch (lcd_chip_type) {
	case LCD_CHIP_M6:
		ss_base = SS_LEVEL_0_M6;
		break;
	case LCD_CHIP_M8:
	case LCD_CHIP_M8M2:
		ss_base = SS_LEVEL_0_M8;
		break;
	case LCD_CHIP_M8B:
		ss_base = SS_LEVEL_0_M8B;
		break;
	default:
		ss_base = 0;
		break;
	}
	DPRINT("Init LCD mode: %s, %s(%u) %ubit, ",
		pConf->lcd_basic.model_name,
		lcd_type_table[lcd_type], lcd_type,
		pConf->lcd_basic.lcd_bits);
	DPRINT("%ux%u@%u.%uHz, ss_level=%u(%s)\n",
		pConf->lcd_basic.h_active, pConf->lcd_basic.v_active,
		(pConf->lcd_timing.sync_duration_num / 10),
		(pConf->lcd_timing.sync_duration_num % 10),
		ss_level, lcd_ss_level_table[ss_level + ss_base]);

	if (lcd_chip_valid_check() == FALSE)
		return;

#ifdef CONFIG_AML_VPU
	lcd_vpu_power_ctrl(1);
#endif
	lcd_switch_clk_gate(ON);
	set_clk_lcd(pConf);
	set_venc_lcd(pConf);
	set_tcon_lcd(pConf);
	switch (lcd_type) {
	case LCD_DIGITAL_MIPI:
		init_dphy(pConf);
		set_control_mipi(lcd_Conf);
		break;
	case LCD_DIGITAL_LVDS:
		init_dphy(pConf);
		set_control_lvds(pConf);
		break;
	case LCD_DIGITAL_EDP:
		init_dphy(pConf);
		break;
	case LCD_DIGITAL_TTL:
		set_control_ttl(pConf);
		break;
	default:
		DPRINT("Invalid LCD type\n");
		break;
	}
	set_video_adjust(pConf);
	DPRINT("%s finished\n", __func__);
}

static void _disable_lcd_driver(struct Lcd_Config_s *pConf)
{
	if (lcd_chip_valid_check() == FALSE)
		return;

	switch (pConf->lcd_basic.lcd_type) {
	case LCD_DIGITAL_MIPI:
		mipi_dsi_host_off();
		break;
	case LCD_DIGITAL_EDP:
		edp_host_off();
		break;
	case LCD_DIGITAL_LVDS:
		/* close lvds phy clk gate: 0x104c[11] */
		lcd_reg_setb(HHI_VIID_DIVIDER_CNTL, 0, DIV_LVDS_CLK_EN, 1);
		lcd_reg_setb(LVDS_GEN_CNTL, 0, 3, 1); /* disable lvds fifo */
		break;
	case LCD_DIGITAL_TTL:
	default:
		break;
	}

	lcd_reg_write(ENCL_VIDEO_EN, 0); /* disable encl */
	if (lcd_chip_type == LCD_CHIP_M6)
		lcd_reg_write(ENCT_VIDEO_EN, 0); /* disable enct */

	/* disable CTS_ENCL clk gate, new added in m8m2 */
	if ((lcd_chip_type == LCD_CHIP_M8M2) || (lcd_chip_type == LCD_CHIP_M8B))
		lcd_reg_setb(HHI_VID_CLK_CNTL2, 0, ENCL_GATE_VCLK, 1);

	/* close vclk2_div gate: 0x104b[4:0] */
	lcd_reg_setb(HHI_VIID_CLK_CNTL, 0, 0, 5);
	lcd_reg_setb(HHI_VIID_CLK_CNTL, 0, VCLK2_EN, 1);

	/* close vid2_pll gate: 0x104c[16] */
	lcd_reg_setb(HHI_VIID_DIVIDER_CNTL, 0, DIV_CLK_IN_EN, 1);

	/* disable pll */
	switch (lcd_chip_type) {
	case LCD_CHIP_M6:
		/* power down viid_pll: 0x1047[30] */
		lcd_reg_setb(HHI_VIID_PLL_CNTL, 1, LCD_PLL_PD_M6, 1);
		break;
	case LCD_CHIP_M8:
	case LCD_CHIP_M8M2:
		/* disable pll_out mux */
		lcd_reg_setb(HHI_VID2_PLL_CNTL5, 0, 23, 3);
		/* disable vid2_pll: 0x10e0[30] */
		lcd_reg_setb(HHI_VID2_PLL_CNTL, 0, LCD_PLL_EN_M8, 1);
		break;
	case LCD_CHIP_M8B:
		/* disable vid_pll: 0x10c8[30] */
		lcd_reg_setb(HHI_VID_PLL_CNTL, 0, LCD_PLL_EN_M8B, 1);
		break;
	default:
		break;
	}

	lcd_switch_clk_gate(OFF);
#ifdef CONFIG_AML_VPU
	lcd_vpu_power_ctrl(0);
#endif
	DPRINT("disable lcd display driver\n");
}

static void _enable_vsync_interrupt(void)
{
	/* Progressive encoder filed interrupt enable: 0x1b6e[9] */
	lcd_reg_write(VENC_INTCTRL, 0x200);

}

/* *********************************************************
 * lcd debug function
 * ********************************************************* */
static void lcd_vso_adjust(unsigned int val)
{
	lcd_reg_write(ENCL_VIDEO_VSO_BLINE, val);
	lcd_reg_write(ENCL_VIDEO_VSO_ELINE, val + 2);
	DPRINT("set vso start: %u\n", val);
}

#define LCD_ENC_TST_NUM_MAX    8
static char *lcd_enc_tst_str[] = {
	"None",        /* 0 */
	"Color Bar",   /* 1 */
	"Thin Line",   /* 2 */
	"Dot Grid",    /* 3 */
	"Gray",        /* 4 */
	"Red",         /* 5 */
	"Green",       /* 6 */
	"Blue",        /* 7 */
};

static unsigned int lcd_enc_tst[][6] = {
	/* tst_mode,  Y,       Cb,     Cr,    tst_en,  vfifo_en */
	{  0,         0x200,   0x200,  0x200,  0,      1},  /* 0 */
	{  1,         0x200,   0x200,  0x200,  1,      0},  /* 1 */
	{  2,         0x200,   0x200,  0x200,  1,      0},  /* 2 */
	{  3,         0x200,   0x200,  0x200,  1,      0},  /* 3 */
	{  0,         0x200,   0x200,  0x200,  1,      0},  /* 4 */
	{  0,         0x130,   0x153,  0x3fd,  1,      0},  /* 5 */
	{  0,         0x256,   0x0ae,  0x055,  1,      0},  /* 6 */
	{  0,         0x074,   0x3fd,  0x1ad,  1,      0},  /* 7 */
};

static void lcd_test_enct(unsigned int num)
{
	lcd_reg_write(ENCT_TST_MDSEL, lcd_enc_tst[num][0]);
	lcd_reg_write(ENCT_TST_Y, lcd_enc_tst[num][1]);
	lcd_reg_write(ENCT_TST_CB, lcd_enc_tst[num][2]);
	lcd_reg_write(ENCT_TST_CR, lcd_enc_tst[num][3]);
	lcd_reg_write(ENCT_TST_CLRBAR_STRT,
		lcd_Conf->lcd_timing.video_on_pixel);
	lcd_reg_write(ENCT_TST_CLRBAR_WIDTH,
		(lcd_Conf->lcd_basic.h_active / 9));
	lcd_reg_write(ENCT_TST_EN, lcd_enc_tst[num][4]);
	lcd_reg_setb(ENCT_VIDEO_MODE_ADV, lcd_enc_tst[num][5], 3, 1);
}

static void lcd_test_encl(unsigned int num)
{
	lcd_reg_write(ENCL_TST_MDSEL, lcd_enc_tst[num][0]);
	lcd_reg_write(ENCL_TST_Y, lcd_enc_tst[num][1]);
	lcd_reg_write(ENCL_TST_CB, lcd_enc_tst[num][2]);
	lcd_reg_write(ENCL_TST_CR, lcd_enc_tst[num][3]);
	lcd_reg_write(ENCL_TST_CLRBAR_STRT,
		lcd_Conf->lcd_timing.video_on_pixel);
	lcd_reg_write(ENCL_TST_CLRBAR_WIDTH,
		(lcd_Conf->lcd_basic.h_active / 9));
	lcd_reg_write(ENCL_TST_EN, lcd_enc_tst[num][4]);
	lcd_reg_setb(ENCL_VIDEO_MODE_ADV, lcd_enc_tst[num][5], 3, 1);
}

static void lcd_test(unsigned int num)
{
	num = (num >= LCD_ENC_TST_NUM_MAX) ? 0 : num;

	if (lcd_chip_type == LCD_CHIP_M6) {
		if (lcd_Conf->lcd_basic.lcd_type == LCD_DIGITAL_TTL)
			lcd_test_enct(num);
		else
			lcd_test_encl(num);
	} else {
		lcd_test_encl(num);
	}

	if (num > 0)
		DPRINT("show test pattern %d: %s\n", num, lcd_enc_tst_str[num]);
	else
		DPRINT("disable test pattern\n");
}

static unsigned int gamma_tst_y[18] = {
	/* Y_val    test_level   gray_level */
	0x000,      /* 1          //0 */
	0x072,      /* 2          //15 */
	0x0a4,      /* 3          //30 */
	0x0d7,      /* 4          //45 */
	0x10b,      /* 5          //60 */
	0x13f,      /* 6          //75 */
	0x175,      /* 7          //90 */
	0x1a8,      /* 8          //105 */
	0x1dc,      /* 9          //120 */
	0x210,      /* 10         //135 */
	0x23f,      /* 11         //150 */
	0x274,      /* 12         //165 */
	0x2a8,      /* 13         //180 */
	0x2da,      /* 14         //195 */
	0x30e,      /* 15         //210 */
	0x342,      /* 16         //225 */
	0x376,      /* 17         //240 */
	0x3aa,      /* 18         //255 */
};

static void lcd_gamma_test_enct(unsigned int num)
{
	if ((num >= 1) && (num <= 18)) {
		lcd_reg_write(ENCT_TST_MDSEL, 0);
		lcd_reg_write(ENCT_TST_Y,  gamma_tst_y[num-1]);
		lcd_reg_write(ENCT_TST_CB, 0x200);
		lcd_reg_write(ENCT_TST_CR, 0x200);
		lcd_reg_write(ENCT_TST_EN, 1);
		lcd_reg_setb(ENCT_VIDEO_MODE_ADV, 0, 3, 1);
		DPRINT("gamma test level: %d, gray level: %d\n",
			num, ((num - 1) * 15));
	} else {
		lcd_reg_setb(ENCT_VIDEO_MODE_ADV, 1, 3, 1);
		lcd_reg_write(ENCT_TST_EN, 0);
		DPRINT("gamma test pattern disabled\n");
	}
}

static void lcd_gamma_test_encl(unsigned int num)
{
	if ((num >= 1) && (num <= 18)) {
		lcd_reg_write(ENCL_TST_MDSEL, 0);
		lcd_reg_write(ENCL_TST_Y,  gamma_tst_y[num-1]);
		lcd_reg_write(ENCL_TST_CB, 0x200);
		lcd_reg_write(ENCL_TST_CR, 0x200);
		lcd_reg_write(ENCL_TST_EN, 1);
		lcd_reg_setb(ENCL_VIDEO_MODE_ADV, 0, 3, 1);
		DPRINT("gamma test level: %d, gray level: %d\n",
			num, ((num - 1) * 15));
	} else {
		lcd_reg_setb(ENCL_VIDEO_MODE_ADV, 1, 3, 1);
		lcd_reg_write(ENCL_TST_EN, 0);
		DPRINT("gamma test pattern disabled\n");
	}
}

static void lcd_gamma_test(unsigned int num)
{
	if (lcd_chip_type == LCD_CHIP_M6) {
		if (lcd_Conf->lcd_basic.lcd_type == LCD_DIGITAL_TTL)
			lcd_gamma_test_enct(num);
		else
			lcd_gamma_test_encl(num);
	} else {
		lcd_gamma_test_encl(num);
	}
}

static DEFINE_MUTEX(lcd_init_mutex);
static void lcd_module_enable(void)
{
	int ret = 0;

	mutex_lock(&lcd_init_mutex);

	_init_lcd_driver(lcd_Conf);
	ret = lcd_Conf->lcd_power_ctrl.power_ctrl(ON);
	if (lcd_Conf->lcd_basic.lcd_type == LCD_DIGITAL_EDP) {
		if (ret > 0) {
			lcd_Conf->lcd_power_ctrl.power_ctrl(OFF);
			_disable_lcd_driver(lcd_Conf);
			mdelay(30);
			lcd_config_init(lcd_Conf);
			_init_lcd_driver(lcd_Conf);
			lcd_Conf->lcd_power_ctrl.power_ctrl(ON);
		}
	}
	_enable_vsync_interrupt();
	lcd_Conf->lcd_misc_ctrl.lcd_status = 1;
	mutex_unlock(&lcd_init_mutex);
}

static void lcd_module_disable(void)
{
	mutex_lock(&lcd_init_mutex);
	lcd_Conf->lcd_misc_ctrl.lcd_status = 0;
	lcd_Conf->lcd_power_ctrl.power_ctrl(OFF);
	_disable_lcd_driver(lcd_Conf);
	mutex_unlock(&lcd_init_mutex);
}

void lcd_control_assign(struct Lcd_Config_s *pConf)
{
	pConf->lcd_power_ctrl.power_ctrl = lcd_power_ctrl;
	pConf->lcd_power_ctrl.backlight_power_ctrl = backlight_power_ctrl;

	pConf->lcd_effect.set_gamma_table = set_gamma_table_lcd;
	pConf->lcd_effect.gamma_test = lcd_gamma_test;

	pConf->lcd_misc_ctrl.module_enable = lcd_module_enable;
	pConf->lcd_misc_ctrl.module_disable = lcd_module_disable;
	pConf->lcd_misc_ctrl.vso_adjust = lcd_vso_adjust;
	pConf->lcd_misc_ctrl.lcd_test = lcd_test;
	pConf->lcd_misc_ctrl.edp_apb_clk_prepare = edp_apb_clk_prepare;
	pConf->lcd_misc_ctrl.edp_edid_load = lcd_config_edp_edid_load;
}

void lcd_control_probe(struct Lcd_Config_s *pConf)
{
	spin_lock_init(&gamma_write_lock);
	spin_lock_init(&lcd_clk_lock);

	lcd_Conf = pConf;
	if (lcd_chip_valid_check() == FALSE)
		return;

	if (lcd_reg_read(ENCL_VIDEO_EN) & 1)
		pConf->lcd_misc_ctrl.lcd_status = 1;
	else
		pConf->lcd_misc_ctrl.lcd_status = 0;

	switch (pConf->lcd_basic.lcd_type) {
	case LCD_DIGITAL_MIPI:
		dsi_probe(pConf);
		break;
	case LCD_DIGITAL_EDP:
		lcd_config_edp_edid_load();
		break;
	default:
		break;
	}
}

void lcd_control_remove(struct Lcd_Config_s *pConf)
{
	lcd_Conf = NULL;
	if (lcd_chip_valid_check() == FALSE)
		return;
	switch (pConf->lcd_basic.lcd_type) {
	case LCD_DIGITAL_MIPI:
		dsi_remove();
		break;
	default:
		break;
	}
}

