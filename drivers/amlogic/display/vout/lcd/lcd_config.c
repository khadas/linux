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
#include <linux/of.h>
#include <linux/reset.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/vpu.h>
#include <linux/amlogic/vout/vinfo.h>
#include <linux/amlogic/vout/lcdoutc.h>
#include "lcd_reg.h"
#include "lcd_config.h"
#include "mipi_dsi_util.h"
#include "edp_drv.h"

#define VPP_OUT_SATURATE        (1 << 0)

static spinlock_t gamma_write_lock;
static spinlock_t lcd_clk_lock;

static struct Lcd_Config_s *lcd_Conf;

enum LCD_Chip_e lcd_chip_type = LCD_CHIP_MAX;
const char *lcd_chip_table[] = {
	"M6",
	"M8",
	"M8b",
	"M8M2",
	"invalid",
};

const char *lcd_type_table[] = {
	"MIPI",
	"LVDS",
	"eDP",
	"TTL",
	"miniLVDS",
	"invalid",
};

/* must the same order with enum LCD_Chip_e and enum Lcd_Type_e */
static const unsigned char lcd_chip_cap[][6] = {
	/* mipi, lvds, edp, ttl, mlvds, invalid */
	{  0,    1,    0,   1,   1,     0,},/* M6 */
	{  1,    1,    1,   1,   0,     0,},/* M8 */
	{  1,    1,    0,   1,   0,     0,},/* M8B */
	{  1,    1,    1,   1,   0,     0,},/* M8M2 */
	{  0,    0,    0,   0,   0,     0,},/* none */
};

#define SS_LEVEL_MAX_M6    7
#define SS_LEVEL_MAX_M8    5
#define SS_LEVEL_MAX_M8B   5
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

const char *edp_link_rate_string_table[] = {
	"1.62Gbps",
	"2.70Gbps",
	"5.40Gbps",
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

struct lcd_clk_config_s clk_Conf = {/* unit: kHz */
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

static void print_lcd_driver_version(void)
{
	DPRINT("lcd driver version: %sc%u\n\n", LCD_DRV_DATE, lcd_chip_type);
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
	else
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
	else
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
static void select_edp_link_config(struct Lcd_Config_s *pConf)
{
	unsigned bit_rate, band_width;
	unsigned i, j, done = 0;
	struct EDP_Config_s *econf;

	econf = pConf->lcd_control.edp_config;
	bit_rate = (pConf->lcd_timing.lcd_clk / 1000) *
		pConf->lcd_basic.lcd_bits * 3 / 1000;  /* Mbps */
	econf->bit_rate = bit_rate;

	if (econf->link_user == 0) {/* auto calculate */
		i = 0;
		while ((edp_lane_count_table[i] <= econf->max_lane_count) &&
			(done == 0)) {
			for (j = 0; j <= 1; j++) {
				band_width = edp_link_capacity_table[j] *
					edp_lane_count_table[i];
				if (band_width > bit_rate) {
					econf->link_rate = j;
					econf->lane_count =
						edp_lane_count_table[i];
					done = 1;
					break;
				}
			}
			if (done == 0)
				i++;
		}
		if (done == 0) {
			econf->link_rate = 1;
			econf->lane_count = econf->max_lane_count;
			DPRINT("Error: bit_rate is out of support, ");
			DPRINT("should reduce frame rate(pixel clock)\n");
		} else {
			DPRINT("select edp link_rate=%s, lane_count=%u\n",
				edp_link_rate_string_table[econf->link_rate],
				econf->lane_count);
		}
	} else { /* verify user define */
		i = get_edp_config_index(&edp_lane_count_table[0],
			econf->lane_count);
		while ((edp_lane_count_table[i] <= econf->max_lane_count) &&
			(done == 0)) {
			band_width = edp_link_capacity_table[econf->link_rate] *
				edp_lane_count_table[i];
			if (band_width > bit_rate) {
				econf->lane_count = edp_lane_count_table[i];
				done = 1;
			} else {
				i++;
			}
		}
		if (done == 0) {
			econf->lane_count = econf->max_lane_count;
			DPRINT("Error: bandwidth is not enough at ");
			DPRINT("link_rate=%s, lane_count=%d\n",
				edp_link_rate_string_table[econf->link_rate],
				econf->lane_count);
		} else {
			DPRINT("set edp link_rate=%s, lane_count=%u\n",
				edp_link_rate_string_table[econf->link_rate],
				econf->lane_count);
		}
	}
}

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

static void lcd_config_edp_edid_load(void)
{
	if (lcd_chip_valid_check(lcd_Conf) == FALSE)
		return;
	if (lcd_Conf->lcd_control.edp_config->edid_timing_used == 0)
		return;

	if (lcd_Conf->lcd_misc_ctrl.lcd_status == 0) {
		/* enable edp power, phy and tx */
		/* fclk_div5---fixed 510M, div to 170M, edp apb clk */
		if (lcd_chip_type == LCD_CHIP_M8) {
			lcd_reg_write(HHI_EDP_APB_CLK_CNTL,
				(1 << 7) | (2 << 0));
		} else if (lcd_chip_type == LCD_CHIP_M8M2) {
			lcd_reg_write(HHI_EDP_APB_CLK_CNTL_M8M2,
				(1 << 7) | (2 << 0));
		}

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

static void switch_lcd_clk_gate(enum Bool_state_e status)
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

static void _init_lcd_driver(struct Lcd_Config_s *pConf)
{
	enum Lcd_Type_e lcd_type;
	unsigned char ss_level, ss_base;

	print_lcd_driver_version();
	request_vpu_clk_vmod(pConf->lcd_timing.lcd_clk, VMODE_LCD);
	switch_vpu_mem_pd_vmod(VMODE_LCD, VPU_MEM_POWER_ON);

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

	if (lcd_chip_valid_check(pConf) == FALSE)
		return;

	switch_lcd_clk_gate(ON);
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
	if (lcd_chip_valid_check(pConf) == FALSE)
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

	switch_lcd_clk_gate(OFF);
	switch_vpu_mem_pd_vmod(VMODE_LCD, VPU_MEM_POWER_DOWN);
	release_vpu_clk_vmod(VMODE_LCD);
	DPRINT("disable lcd display driver\n");
}

static void _enable_vsync_interrupt(void)
{
	/* Progressive encoder filed interrupt enable: 0x1b6e[9] */
	lcd_reg_write(VENC_INTCTRL, 0x200);

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

/* *********************************************** */
/* sysfs api for video */
/* *********************************************** */
static ssize_t lcd_vid_vso_read(struct class *class,
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "read vso start: %u\n",
		lcd_Conf->lcd_timing.vso_vstart);
}

static ssize_t lcd_vid_vso_write(struct class *class,
		struct class_attribute *attr, const char *buf, size_t count)
{
	unsigned int ret;
	unsigned int temp;

	temp = 10;
	ret = sscanf(buf, "%u", &temp);
	lcd_Conf->lcd_timing.vso_vstart = (unsigned short)temp;
	lcd_Conf->lcd_timing.vso_user = 1;
	lcd_reg_write(ENCL_VIDEO_VSO_BLINE, temp);
	lcd_reg_write(ENCL_VIDEO_VSO_ELINE, temp + 2);
	DPRINT("set vso start: %u\n", temp);

	if (ret != 1 || ret != 2)
		return -EINVAL;

	return count;
}

static struct class_attribute lcd_video_class_attrs[] = {
	__ATTR(vso,  S_IRUGO | S_IWUSR, lcd_vid_vso_read, lcd_vid_vso_write),
};

static int creat_lcd_video_attr(struct Lcd_Config_s *pConf)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(lcd_video_class_attrs); i++) {
		if (class_create_file(pConf->lcd_misc_ctrl.debug_class,
			&lcd_video_class_attrs[i])) {
			DPRINT("create lcd_video attribute %s fail\n",
				lcd_video_class_attrs[i].attr.name);
		}
	}

	return 0;
}

static int remove_lcd_video_attr(struct Lcd_Config_s *pConf)
{
	int i;

	if (pConf->lcd_misc_ctrl.debug_class == NULL)
		return -1;

	for (i = 0; i < ARRAY_SIZE(lcd_video_class_attrs); i++) {
		class_remove_file(pConf->lcd_misc_ctrl.debug_class,
			&lcd_video_class_attrs[i]);
	}

	return 0;
}
/* *********************************************** */

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

static enum Bool_check_e check_pll(struct lcd_clk_config_s *cConf,
		unsigned int pll_fout)
{
	unsigned int m, n, od_sel, od;
	unsigned int pll_fvco;
	unsigned int od_fb = 0, pll_frac, pll_level = 0;
	enum Bool_check_e done;

	done = FALSE;
	if ((pll_fout > cConf->pll_out_fmax) ||
		(pll_fout < cConf->pll_out_fmin)) {
		return done;
	}
	for (od_sel = cConf->pll_od_sel_max; od_sel > 0; od_sel--) {
		od = od_table[od_sel - 1];
		pll_fvco = pll_fout * od;
		if ((pll_fvco < cConf->pll_vco_fmin) ||
			(pll_fvco > cConf->pll_vco_fmax)) {
			continue;
		}
		cConf->pll_od_sel = od_sel - 1;
		cConf->pll_fout = pll_fout;
		lcd_print("od_sel=%d, pll_fvco=%d\n", (od_sel - 1), pll_fvco);
		if ((pll_fvco >= 2500000) &&
			(pll_fvco <= cConf->pll_vco_fmax)) {
			od_fb = 1;
			pll_level = 4;
		} else if ((pll_fvco >= 2000000) && (pll_fvco < 2500000)) {
			od_fb = 1;
			pll_level = 3;
		} else if ((pll_fvco >= 1700000) && (pll_fvco < 2000000)) {
			/* special adjust */
			od_fb = 1;
			pll_level = 2;
		} else if ((pll_fvco >= cConf->pll_vco_fmin) &&
			(pll_fvco < 1700000)) {
			od_fb = 0;
			pll_level = 1;
		}
		n = 1;
		m = pll_fvco / (cConf->fin * (od_fb + 1));
		pll_frac = (pll_fvco % (cConf->fin * (od_fb + 1))) * 4096 /
			(cConf->fin * (od_fb + 1));
		cConf->pll_m = m;
		cConf->pll_n = n;
		cConf->pll_frac = pll_frac;
		cConf->pll_level = pll_level;
		lcd_print("pll_m=%d, pll_n=%d, ", m, n);
		lcd_print("pll_frac=%d, pll_level=%d\n", pll_frac, pll_level);
		done = TRUE;
	}
	return done;
}

static unsigned int error_abs(unsigned int num1, int unsigned num2)
{
	if (num1 >= num2)
		return num1 - num2;
	else
		return num2 - num1;
}

static enum Bool_check_e check_divider(struct lcd_clk_config_s *cConf,
		unsigned int div_clk_in)
{
	int done;
	unsigned int div_pre_sel;
	unsigned int div_pre, div_post;
	unsigned int div_pre_out, div_post_out;
	unsigned int xd, final_freq;
	unsigned int error;

	done = FALSE;
	if (div_clk_in > cConf->div_pre_in_fmax)
		return done;
	for (div_pre_sel = 0; div_pre_sel < cConf->div_pre_sel_max;
		div_pre_sel++) {
		div_pre = div_pre_table[div_pre_sel];
		div_pre_out = div_clk_in / div_pre;
		if (div_pre_out > cConf->div_pre_out_fmax)
			continue;
		div_post = cConf->div_post + 1;
		div_post_out = div_pre_out / div_post;
		if (div_post_out > cConf->div_post_out_fmax)
			continue;
		cConf->div_pre = div_pre_sel;
		lcd_print("div_pre=%d, div_pre_fout=%d, div_post_fout=%d, ",
			div_pre, div_pre_out, div_post_out);
		for (xd = 1; xd <= cConf->xd_max; xd++) {
			final_freq = div_post_out / xd;
			if (final_freq > cConf->xd_out_fmax)
				continue;
			/* error = cConf->fout - final_freq; */ /* for edp */
			error = error_abs(final_freq, cConf->fout);
			if (error < cConf->err_fmin) {
				cConf->err_fmin = error;
				cConf->xd = xd;
				lcd_print("xd=%d, final_freq=%d\n",
					xd, final_freq);
				done = TRUE;
			}
		}
	}
	return done;
}

static void generate_clk_parameter(struct Lcd_Config_s *pConf,
		struct lcd_clk_config_s *cConf)
{
	unsigned int pll_fref, pll_fvco, pll_fout;
	unsigned int edp_phy_out;
	unsigned int div_pre_in, div_pre_out, div_post_out;
	unsigned int m, n, od_sel, od, pll_level, pll_frac;
	unsigned int edp_div0, edp_div1, edp_div0_sel, edp_div1_sel;
	unsigned int div_pre_sel, div_pre, div_post, xd;
	unsigned int dsi_bit_rate_max = 0, dsi_bit_rate_min = 0;
	unsigned int tmp;
	enum Bool_check_e done;

	done = FALSE;
	cConf->fout = pConf->lcd_timing.lcd_clk / 1000; /* kHz */
	cConf->err_fmin = MAX_ERROR;

	switch (pConf->lcd_basic.lcd_type) {
	case LCD_DIGITAL_MIPI:
		cConf->div_pre_sel_max = DIV_PRE_SEL_MAX;
		div_post = 1;
		cConf->xd_max = CRT_VID_DIV_MAX;
		tmp = pConf->lcd_control.mipi_config->bit_rate_max;
		dsi_bit_rate_max = tmp * 1000; /* change to kHz */
		dsi_bit_rate_min = dsi_bit_rate_max - cConf->fout;
		break;
	case LCD_DIGITAL_EDP:
		cConf->div_pre_sel_max = 1;
		div_post = 1;
		cConf->xd_max = 1;
		cConf->err_fmin = 30 * 1000;
		break;
	case LCD_DIGITAL_LVDS:
		cConf->div_pre_sel_max = DIV_PRE_SEL_MAX;
		div_post = 7;
		cConf->xd_max = 1;
		break;
	case LCD_DIGITAL_TTL:
		cConf->div_pre_sel_max = DIV_PRE_SEL_MAX;
		div_post = 1;
		cConf->xd_max = CRT_VID_DIV_MAX;
		break;
	default:
		cConf->div_pre_sel_max = DIV_PRE_SEL_MAX;
		div_post = 1;
		cConf->xd_max = 1;
		break;
	}
	cConf->div_post = div_post - 1;

	switch (pConf->lcd_basic.lcd_type) {
	case LCD_DIGITAL_MIPI:
		if (cConf->fout < cConf->xd_out_fmax)
			break;
		for (xd = 1; xd <= cConf->xd_max; xd++) {
			div_post_out = cConf->fout * xd;
			if (div_post_out > cConf->div_post_out_fmax)
				continue;
			div_pre_out = div_post_out * div_post;
			if (div_pre_out > cConf->div_pre_out_fmax)
				continue;
			cConf->xd = xd;
			lcd_print("fout=%d, xd=%d, ", cConf->fout, xd);
			lcd_print("div_post_fout=%d, div_pre_fout=%d\n",
				div_post_out, div_pre_out);
			for (div_pre_sel = 0;
				div_pre_sel < cConf->div_pre_sel_max;
				div_pre_sel++) {
				div_pre = div_pre_table[div_pre_sel];
				div_pre_in = div_pre_out * div_pre;
				if (div_pre_in > cConf->div_pre_in_fmax)
					continue;
				pll_fout = div_pre_in;
				if ((pll_fout > dsi_bit_rate_max) ||
					(pll_fout < dsi_bit_rate_min)) {
					continue;
				}
				cConf->div_pre = div_pre_sel;
				lcd_print("div_pre=%d, pll_fout=%d\n",
					div_pre, pll_fout);
				done = check_pll(cConf, pll_fout);
				if (done == TRUE)
					goto generate_clk_done;
			}
		}
		break;
	case LCD_DIGITAL_EDP:
		switch (pConf->lcd_control.edp_config->link_rate) {
		case 0:
			n = 1;
			m = 67;
			od_sel = 0;
			pll_level = 1;
			pll_frac = 0x800;
			pll_fout = 1620000;
			break;
		case 1:
		default:
			n = 1;
			m = 56;
			od_sel = 0;
			pll_level = 4;
			pll_frac = 0x400;
			pll_fout = 2700000;
			break;
		}
		cConf->pll_m = m;
		cConf->pll_n = n;
		cConf->pll_frac = pll_frac;
		cConf->pll_level = pll_level;
		cConf->pll_od_sel = od_sel;
		cConf->pll_fout = pll_fout;
		for (edp_div1_sel = 0; edp_div1_sel < EDP_DIV1_SEL_MAX;
			edp_div1_sel++) {
			edp_div1 = edp_div1_table[edp_div1_sel];
			cConf->edp_div1 = edp_div1_sel;
			for (edp_div0_sel = 0; edp_div0_sel < EDP_DIV0_SEL_MAX;
				edp_div0_sel++) {
				edp_div0 = edp_div0_table[edp_div0_sel];
				edp_phy_out = pll_fout / (edp_div0 * edp_div1);
				cConf->edp_div0 = edp_div0_sel;
				div_pre_in = edp_phy_out;
				if (check_divider(cConf, div_pre_in) == TRUE)
					done = TRUE;
			}
		}
		break;
	case LCD_DIGITAL_LVDS:
	case LCD_DIGITAL_TTL:
		if (cConf->fout < cConf->xd_out_fmax)
			break;
		if (lcd_chip_type == LCD_CHIP_M6) {
			n = 1;
			pll_fref = cConf->fin / n;
			cConf->pll_n = n;
			cConf->pll_frac = 0;
			cConf->pll_level = 0;
			for (m = cConf->pll_m_min; m <= cConf->pll_m_max; m++) {
				pll_fvco = pll_fref * m;
				if ((pll_fvco < cConf->pll_vco_fmin) ||
					(pll_fvco > cConf->pll_vco_fmax)) {
					continue;
				}
				cConf->pll_m = m;
				lcd_print("pll_m=%d, pll_fvco=%d, ",
					m, pll_fvco);
				for (od_sel = cConf->pll_od_sel_max;
					od_sel > 0; od_sel--) {
					od = od_table[od_sel - 1];
					pll_fout = pll_fvco / od;
					if (pll_fout > cConf->pll_out_fmax)
						continue;
					cConf->pll_od_sel = od_sel - 1;
					cConf->pll_fout = pll_fout;
					lcd_print("od_sel=%d, pll_fout=%d, ",
						(od_sel - 1), pll_fout);
					div_pre_in = pll_fout;
					if (check_divider(cConf, div_pre_in) ==
						TRUE) {
						done = TRUE;
					}
				}
			}
		} else {
			for (xd = 1; xd <= cConf->xd_max; xd++) {
				div_post_out = cConf->fout * xd;
				if (div_post_out > cConf->div_post_out_fmax)
					continue;
				div_pre_out = div_post_out * div_post;
				if (div_pre_out <= cConf->div_pre_out_fmax)
					continue;
				cConf->xd = xd;
				lcd_print("fout=%d, xd=%d, ", cConf->fout, xd);
				lcd_print("div_post_out=%d, div_pre_out=%d\n",
					div_post_out, div_pre_out);
				for (div_pre_sel = 0;
					div_pre_sel < cConf->div_pre_sel_max;
					div_pre_sel++) {
					div_pre = div_pre_table[div_pre_sel];
					div_pre_in = div_pre_out * div_pre;
					if (div_pre_in > cConf->div_pre_in_fmax)
						continue;
					cConf->div_pre = div_pre_sel;
					pll_fout = div_pre_in;
					lcd_print("div_pre=%d, pll_fout=%d\n",
						div_pre, pll_fout);
					done = check_pll(cConf, pll_fout);
					if (done == TRUE)
						goto generate_clk_done;
				}
			}
		}
		break;
	default:
		break;
	}

generate_clk_done:
	if (done == TRUE) {
		pConf->lcd_timing.pll_ctrl =
			(cConf->pll_od_sel << PLL_CTRL_OD) |
			(cConf->pll_n << PLL_CTRL_N) |
			(cConf->pll_m << PLL_CTRL_M);
		pConf->lcd_timing.div_ctrl = 0x18803 |
			(cConf->edp_div1 << DIV_CTRL_EDP_DIV1) |
			(cConf->edp_div0 << DIV_CTRL_EDP_DIV0) |
			(cConf->div_pre << DIV_CTRL_DIV_PRE);
		tmp = (pConf->lcd_timing.clk_ctrl & ~((0xff << CLK_CTRL_XD) |
			(0x7 << CLK_CTRL_LEVEL) | (0xfff << CLK_CTRL_FRAC)));
		pConf->lcd_timing.clk_ctrl = (tmp |
			((cConf->xd << CLK_CTRL_XD) |
			(cConf->pll_level << CLK_CTRL_LEVEL) |
			(cConf->pll_frac << CLK_CTRL_FRAC)));
	} else {
		pConf->lcd_timing.pll_ctrl = (1 << PLL_CTRL_OD) |
			(1 << PLL_CTRL_N) | (50 << PLL_CTRL_M);
		pConf->lcd_timing.div_ctrl = 0x18803 |
			(0 << DIV_CTRL_EDP_DIV1) | (0 << DIV_CTRL_EDP_DIV0) |
			(1 << DIV_CTRL_DIV_PRE);
		pConf->lcd_timing.clk_ctrl = (pConf->lcd_timing.clk_ctrl &
			~(0xff << CLK_CTRL_XD)) | (7 << CLK_CTRL_XD);
		DPRINT("Out of clock range, reset to default setting!\n");
	}
}

static void parse_clk_parameter(struct Lcd_Config_s *pConf,
		struct lcd_clk_config_s *cConf)
{
	unsigned int od_fb, od;
	unsigned int div_post;
	unsigned int temp;
	struct Lcd_Timing_s *tconf;

	tconf = &(pConf->lcd_timing);
	cConf->pll_m = (tconf->pll_ctrl >> PLL_CTRL_M) & 0x1ff;
	cConf->pll_n = (tconf->pll_ctrl >> PLL_CTRL_N) & 0x1f;
	cConf->pll_od_sel = (tconf->pll_ctrl >> PLL_CTRL_OD) & 0x3;
	cConf->pll_frac = (tconf->clk_ctrl >> CLK_CTRL_FRAC) & 0xfff;
	cConf->pll_level = (tconf->clk_ctrl >> CLK_CTRL_LEVEL) & 0x7;
	cConf->div_pre = (tconf->div_ctrl >> DIV_CTRL_DIV_PRE) & 0x7;

	cConf->edp_div0 = 0;
	cConf->edp_div1 = 0;
	switch (pConf->lcd_basic.lcd_type) {
	case LCD_DIGITAL_MIPI:
		cConf->xd = ((tconf->clk_ctrl) >> CLK_CTRL_XD) & 0xff;
		div_post = 1;
		break;
	case LCD_DIGITAL_EDP:
		cConf->edp_div0 = (tconf->div_ctrl >> DIV_CTRL_EDP_DIV0) & 0xf;
		cConf->edp_div1 = (tconf->div_ctrl >> DIV_CTRL_EDP_DIV1) & 0x7;
		cConf->xd = 1;
		div_post = 1;
		break;
	case LCD_DIGITAL_LVDS:
		cConf->xd = 1;
		div_post = 7;
		break;
	case LCD_DIGITAL_TTL:
		cConf->xd = ((tconf->clk_ctrl) >> CLK_CTRL_XD) & 0xff;
		div_post = 1;
		break;
	default:
		cConf->xd = ((tconf->clk_ctrl) >> CLK_CTRL_XD) & 0xff;
		div_post = 1;
		break;
	}
	cConf->div_post = div_post - 1;

	/* calculate pll out frequency (unit: kHz) */
	od_fb = (cConf->pll_level > 1) ? 1 : 0;
	od = od_table[cConf->pll_od_sel];
	temp = (cConf->pll_frac * (od_fb + 1) * cConf->fin) / 4096;
	cConf->pll_fout = (cConf->pll_m * (od_fb + 1) * cConf->fin + temp) /
		(cConf->pll_n * od);
}

static void lcd_sync_duration(struct Lcd_Config_s *pConf,
		struct lcd_clk_config_s *cConf)
{
	unsigned int edp_div0, edp_div1, div_pre, div_post;
	unsigned int h_period, v_period, sync_duration;
	unsigned int temp;

	/* save mipi bit rate */
	if (pConf->lcd_basic.lcd_type == LCD_DIGITAL_MIPI) {
		pConf->lcd_control.mipi_config->bit_rate =
			cConf->pll_fout * 1000;
		DPRINT("mipi-dsi bit rate: %d.%03dMHz\n",
			(cConf->pll_fout / 1000), (cConf->pll_fout % 1000));
	}

	/* calculate final frequency, equal to pixel clock (unit: kHz) */
	edp_div0 = edp_div0_table[cConf->edp_div0];
	edp_div1 = edp_div1_table[cConf->edp_div1];
	div_pre = div_pre_table[cConf->div_pre];
	div_post = cConf->div_post + 1;
	cConf->fout = cConf->pll_fout /
		(edp_div0 * edp_div1 * div_pre * div_post * cConf->xd);

	pConf->lcd_timing.lcd_clk = cConf->fout * 1000; /* calc to unit Hz */
	h_period = pConf->lcd_basic.h_period;
	v_period = pConf->lcd_basic.v_period;
	temp = ((pConf->lcd_timing.lcd_clk / h_period) * 100) / v_period;
	sync_duration = (temp + 5) / 10;

	pConf->lcd_timing.sync_duration_num = sync_duration;
	pConf->lcd_timing.sync_duration_den = 10;
	temp = pConf->lcd_timing.sync_duration_den;
	DPRINT("lcd_clk=%u.%03uMHz, frame_rate=%u.%uHz.\n\n",
		(pConf->lcd_timing.lcd_clk / 1000000),
		((pConf->lcd_timing.lcd_clk / 1000) % 1000),
		(sync_duration / temp), ((sync_duration * 10 / temp) % 10));
}

static void lcd_tcon_config(struct Lcd_Config_s *pConf)
{
	unsigned short h_period, h_active, v_period, v_active;
	unsigned short de_hstart, de_vstart;
	unsigned short hstart, hend, vstart, vend;
	unsigned short h_delay = 0;
	unsigned short h_offset = 0, v_offset = 0, vsync_h_phase = 0;

	switch (pConf->lcd_basic.lcd_type) {
	case LCD_DIGITAL_MIPI:
		h_delay = MIPI_DELAY;
		break;
	case LCD_DIGITAL_EDP:
		h_delay = EDP_DELAY;
		break;
	case LCD_DIGITAL_LVDS:
		h_delay = LVDS_DELAY;
		break;
	case LCD_DIGITAL_TTL:
		h_delay = TTL_DELAY;
		break;
	default:
		h_delay = 0;
		break;
	}

	h_period = pConf->lcd_basic.h_period;
	h_active = pConf->lcd_basic.h_active;
	v_period = pConf->lcd_basic.v_period;
	v_active = pConf->lcd_basic.v_active;

	pConf->lcd_timing.video_on_pixel = h_period - h_active - 1 - h_delay;
	pConf->lcd_timing.video_on_line = v_period - v_active;

	h_offset = (pConf->lcd_timing.h_offset & 0xffff);
	v_offset = (pConf->lcd_timing.v_offset & 0xffff);
	if ((pConf->lcd_timing.h_offset >> 31) & 1) {
		de_hstart = (h_period - h_active - 1 + h_period - h_offset) %
			h_period;
	} else {
		de_hstart = (h_period - h_active - 1 + h_offset) % h_period;
	}
	if ((pConf->lcd_timing.v_offset >> 31) & 1) {
		de_vstart = (v_period - v_active + v_period - v_offset) %
			v_period;
	} else {
		de_vstart = (v_period - v_active + v_offset) % v_period;
	}

	hstart = (de_hstart + h_period - pConf->lcd_timing.hsync_bp -
		pConf->lcd_timing.hsync_width) % h_period;
	hend = (de_hstart + h_period - pConf->lcd_timing.hsync_bp) % h_period;
	pConf->lcd_timing.hs_hs_addr = hstart;
	pConf->lcd_timing.hs_he_addr = hend;
	pConf->lcd_timing.hs_vs_addr = 0;
	pConf->lcd_timing.hs_ve_addr = v_period - 1;

	vsync_h_phase = (pConf->lcd_timing.vsync_h_phase & 0xffff);
	if ((pConf->lcd_timing.vsync_h_phase >> 31) & 1) /* negative */
		vsync_h_phase = (hstart + h_period - vsync_h_phase) % h_period;
	else /* positive */
		vsync_h_phase = (hstart + h_period + vsync_h_phase) % h_period;
	pConf->lcd_timing.vs_hs_addr = vsync_h_phase;
	pConf->lcd_timing.vs_he_addr = vsync_h_phase;
	vstart = (de_vstart + v_period - pConf->lcd_timing.vsync_bp -
		pConf->lcd_timing.vsync_width) % v_period;
	vend = (de_vstart + v_period - pConf->lcd_timing.vsync_bp) % v_period;
	pConf->lcd_timing.vs_vs_addr = vstart;
	pConf->lcd_timing.vs_ve_addr = vend;

	pConf->lcd_timing.de_hs_addr = de_hstart;
	pConf->lcd_timing.de_he_addr = (de_hstart + h_active) % h_period;
	pConf->lcd_timing.de_vs_addr = de_vstart;
	pConf->lcd_timing.de_ve_addr = (de_vstart + v_active - 1) % v_period;

	if (pConf->lcd_timing.vso_user == 0) {
		/*pConf->lcd_timing.vso_hstart = pConf->lcd_timing.vs_hs_addr;*/
		pConf->lcd_timing.vso_vstart = pConf->lcd_timing.vs_vs_addr;
	}

	/* lcd_print("hs_hs_addr=%d, hs_he_addr=%d, ",
		pConf->lcd_timing.hs_hs_addr, pConf->lcd_timing.hs_he_addr); */
	/* lcd_print("hs_vs_addr=%d, hs_ve_addr=%d\n",
		pConf->lcd_timing.hs_vs_addr, pConf->lcd_timing.hs_ve_addr); */
	/* lcd_print("vs_hs_addr=%d, vs_he_addr=%d, ",
		pConf->lcd_timing.vs_hs_addr, pConf->lcd_timing.vs_he_addr); */
	/* lcd_print("vs_vs_addr=%d, vs_ve_addr=%d\n",
		pConf->lcd_timing.vs_vs_addr, pConf->lcd_timing.vs_ve_addr); */
	/* lcd_print("de_hs_addr=%d, de_he_addr=%d, ",
		pConf->lcd_timing.de_hs_addr, pConf->lcd_timing.de_he_addr); */
	/* lcd_print("de_vs_addr=%d, de_ve_addr=%d\n",
		pConf->lcd_timing.de_vs_addr, pConf->lcd_timing.de_ve_addr); */
}

static void lcd_control_config_pre(struct Lcd_Config_s *pConf)
{
	unsigned int ss_level;
	unsigned int pclk;

	/* prepare refer clock for frame_rate setting */
	pclk = pConf->lcd_timing.lcd_clk;
	if (pclk < 200) {
		pclk *= (pConf->lcd_basic.h_period * pConf->lcd_basic.v_period);
		pConf->lcd_timing.lcd_clk = pclk;
	}

	ss_level = ((pConf->lcd_timing.clk_ctrl >> CLK_CTRL_SS) & 0xf);
	if (ss_level >= clk_Conf.ss_level_max)
		ss_level = clk_Conf.ss_level_max - 1;

	switch (pConf->lcd_basic.lcd_type) {
	case LCD_DIGITAL_MIPI:
		ss_level = ((ss_level > 0) ? 1 : 0);
		set_mipi_dsi_control_config(pConf);
		break;
	case LCD_DIGITAL_EDP:
		/* prepare EDP_APB_CLK to access EDP registers */
		/* fclk_div5---fixed 510M, div to 170M, edp apb clk */
		if (lcd_chip_type == LCD_CHIP_M8) {
			lcd_reg_write(HHI_EDP_APB_CLK_CNTL,
				(1 << 7) | (2 << 0));
		} else if (lcd_chip_type == LCD_CHIP_M8M2) {
			lcd_reg_write(HHI_EDP_APB_CLK_CNTL_M8M2,
				(1<<7) | (2<<0));
		}

		ss_level = ((ss_level > 0) ? 1 : 0);
		select_edp_link_config(pConf);
		if (pConf->lcd_control.edp_config->link_adaptive == 1) {
			pConf->lcd_control.edp_config->vswing = 0;
			pConf->lcd_control.edp_config->preemphasis = 0;
		}
		DPRINT("edp vswing_level=%u, preemphasis_level=%u\n",
			pConf->lcd_control.edp_config->vswing,
			pConf->lcd_control.edp_config->preemphasis);
		break;
	case LCD_DIGITAL_LVDS:
		if (pConf->lcd_control.lvds_config->lvds_repack_user == 0) {
			if (pConf->lcd_basic.lcd_bits == 6)
				pConf->lcd_control.lvds_config->lvds_repack = 0;
			else
				pConf->lcd_control.lvds_config->lvds_repack = 1;
		}
		break;
	default:
		break;
	}
	pConf->lcd_timing.clk_ctrl = ((pConf->lcd_timing.clk_ctrl &
		(~(0xf << CLK_CTRL_SS))) | (ss_level << CLK_CTRL_SS));
	clk_Conf.ss_level = ss_level;
}

/* for special interface config after clk setting */
static void lcd_control_config_post(struct Lcd_Config_s *pConf)
{
	switch (pConf->lcd_basic.lcd_type) {
	case LCD_DIGITAL_MIPI:
		set_mipi_dsi_control_config_post(pConf);
		break;
	default:
		break;
	}
}

#ifdef CONFIG_USE_OF
static unsigned char dsi_init_on_table[DSI_INIT_ON_MAX] = {0xff, 0xff};
static unsigned char dsi_init_off_table[DSI_INIT_OFF_MAX] = {0xff, 0xff};
static struct DSI_Config_s lcd_mipi_config = {
	.lane_num = 4,
	.bit_rate_min = 0,
	.bit_rate_max = 0,
	.factor_numerator = 0,
	.factor_denominator = 10,
	.transfer_ctrl = 0,
	.dsi_init_on = &dsi_init_on_table[0],
	.dsi_init_off = &dsi_init_off_table[0],
	.lcd_extern_init = 0,
};

static struct EDP_Config_s lcd_edp_config = {
	.max_lane_count = 4,
	.link_user = 0,
	.link_rate = 1,
	.lane_count = 4,
	.link_adaptive = 0,
	.vswing = 0,
	.preemphasis = 0,
	.sync_clock_mode = 1,
};

static struct LVDS_Config_s lcd_lvds_config = {
	.lvds_vswing = 1,
	.lvds_repack_user = 0,
	.lvds_repack = 0,
	.pn_swap = 0,
};

static struct TTL_Config_s lcd_ttl_config = {
	.rb_swap = 0,
	.bit_swap = 0,
};

static struct Lcd_Config_s lcd_config = {
	.lcd_timing = {
		.lcd_clk = 40000000,
		.clk_ctrl = ((1 << CLK_CTRL_AUTO) | (0 << CLK_CTRL_SS)),
		.hvsync_valid = 1,
		.de_valid = 1,
		.pol_ctrl = ((0 << POL_CTRL_CLK) | (1 << POL_CTRL_DE) |
			(0 << POL_CTRL_VS) | (0 << POL_CTRL_HS)),
	},
	.lcd_effect = {
		.rgb_base_addr = 0xf0,
		.rgb_coeff_addr = 0x74a,
		.dith_user = 0,
		.vadj_brightness = 0x0,
		.vadj_contrast = 0x80,
		.vadj_saturation = 0x100,
		.gamma_ctrl = ((0 << GAMMA_CTRL_REV) | (1 << GAMMA_CTRL_EN)),
		.gamma_r_coeff = 100,
		.gamma_g_coeff = 100,
		.gamma_b_coeff = 100,
		.set_gamma_table = set_gamma_table_lcd,
		.gamma_test = lcd_gamma_test,
	},
	.lcd_control = {
		.mipi_config = &lcd_mipi_config,
		.edp_config = &lcd_edp_config,
		.lvds_config = &lcd_lvds_config,
		.ttl_config = &lcd_ttl_config,
	},
	.lcd_power_ctrl = {
		.power_on_step = 0,
		.power_off_step = 0,
		.power_ctrl = NULL,
		.ports_ctrl = NULL,
	},
};

struct Lcd_Config_s *get_lcd_config(void)
{
	return &lcd_config;
}
#endif

static void lcd_config_assign(struct Lcd_Config_s *pConf)
{
	pConf->lcd_timing.vso_hstart = 10; /* for video process */
	pConf->lcd_timing.vso_vstart = 10; /* for video process */
	pConf->lcd_timing.vso_user = 0; /* use default config */

	pConf->lcd_power_ctrl.ports_ctrl = lcd_ports_ctrl;

	pConf->lcd_misc_ctrl.vpp_sel = 0;
	pConf->lcd_misc_ctrl.module_enable = lcd_module_enable;
	pConf->lcd_misc_ctrl.module_disable = lcd_module_disable;
	pConf->lcd_misc_ctrl.lcd_test = lcd_test;
	pConf->lcd_misc_ctrl.print_version = print_lcd_driver_version;
	pConf->lcd_misc_ctrl.edp_edid_load = lcd_config_edp_edid_load;
}

enum Bool_check_e lcd_chip_valid_check(struct Lcd_Config_s *pConf)
{
	enum Lcd_Type_e lcd_type;
	int lcd_valid;

	lcd_type = pConf->lcd_basic.lcd_type;
	lcd_valid = lcd_chip_cap[lcd_chip_type][lcd_type];
	if (lcd_valid == 0) { /* lcd type not support by current chip */
		DPRINT("[error]lcd type: Don't support %s(%u) ",
			lcd_type_table[lcd_type], lcd_type);
		DPRINT("in current Chip!\n");
		return FALSE;
	}
	return TRUE;
}

static enum LCD_Chip_e lcd_detect_chip(void)
{
	unsigned int cpu_type;
	enum LCD_Chip_e lcd_chip = LCD_CHIP_MAX;

	cpu_type = get_cpu_type();
	switch (cpu_type) {
	case MESON_CPU_MAJOR_ID_M6:
		lcd_chip = LCD_CHIP_M6;
		break;
	case MESON_CPU_MAJOR_ID_M8:
		lcd_chip = LCD_CHIP_M8;
		break;
	case MESON_CPU_MAJOR_ID_M8B:
		lcd_chip = LCD_CHIP_M8B;
		break;
	case MESON_CPU_MAJOR_ID_M8M2:
		lcd_chip = LCD_CHIP_M8M2;
		break;
	default:
		lcd_chip = LCD_CHIP_MAX;
	}

	DPRINT("lcd driver check chip : %s\n", lcd_chip_table[lcd_chip]);
	return lcd_chip;
}

static void lcd_clk_config_init_by_chip(struct lcd_clk_config_s *cConf)
{
	switch (lcd_chip_type) {
	case LCD_CHIP_M6:
		cConf->ss_level_max = SS_LEVEL_MAX_M6;
		cConf->pll_m_max = PLL_M_MAX_M6;
		cConf->pll_m_min = PLL_M_MIN_M6;
		cConf->pll_n_max = PLL_N_MAX_M6;
		cConf->pll_n_min = PLL_N_MIN_M6;
		cConf->pll_od_sel_max = PLL_OD_SEL_MAX_M6;
		cConf->pll_ref_fmax = PLL_FREF_MAX_M6;
		cConf->pll_ref_fmin = PLL_FREF_MIN_M6;
		cConf->pll_vco_fmax = PLL_VCO_MAX_M6;
		cConf->pll_vco_fmin = PLL_VCO_MIN_M6;
		cConf->pll_out_fmax = DIV_PRE_CLK_IN_MAX_M6;
		cConf->div_pre_in_fmax = DIV_PRE_CLK_IN_MAX_M6;
		cConf->div_pre_out_fmax = DIV_POST_CLK_IN_MAX_M6;
		cConf->div_post_out_fmax = CRT_VID_CLK_IN_MAX_M6;
		cConf->xd_out_fmax = ENCL_CLK_IN_MAX_M6;
		break;
	case LCD_CHIP_M8:
	case LCD_CHIP_M8M2:
		cConf->ss_level_max = SS_LEVEL_MAX_M8;
		cConf->pll_m_max = PLL_M_MAX_M8;
		cConf->pll_m_min = PLL_M_MIN_M8;
		cConf->pll_n_max = PLL_N_MAX_M8;
		cConf->pll_n_min = PLL_N_MIN_M8;
		cConf->pll_od_sel_max = PLL_OD_SEL_MAX_M8;
		cConf->pll_ref_fmax = PLL_FREF_MAX_M8;
		cConf->pll_ref_fmin = PLL_FREF_MIN_M8;
		cConf->pll_vco_fmax = PLL_VCO_MAX_M8;
		cConf->pll_vco_fmin = PLL_VCO_MIN_M8;
		cConf->pll_out_fmax = DIV_PRE_CLK_IN_MAX_M8;
		cConf->div_pre_in_fmax = DIV_PRE_CLK_IN_MAX_M8;
		cConf->div_pre_out_fmax = DIV_POST_CLK_IN_MAX_M8;
		cConf->div_post_out_fmax = CRT_VID_CLK_IN_MAX_M8;
		cConf->xd_out_fmax = ENCL_CLK_IN_MAX_M8;
		break;
	case LCD_CHIP_M8B:
		cConf->ss_level_max = SS_LEVEL_MAX_M8B;
		cConf->pll_m_max = PLL_M_MAX_M8B;
		cConf->pll_m_min = PLL_M_MIN_M8B;
		cConf->pll_n_max = PLL_N_MAX_M8B;
		cConf->pll_n_min = PLL_N_MIN_M8B;
		cConf->pll_od_sel_max = PLL_OD_SEL_MAX_M8B;
		cConf->pll_ref_fmax = PLL_FREF_MAX_M8B;
		cConf->pll_ref_fmin = PLL_FREF_MIN_M8B;
		cConf->pll_vco_fmax = PLL_VCO_MAX_M8B;
		cConf->pll_vco_fmin = PLL_VCO_MIN_M8B;
		cConf->pll_out_fmax = DIV_PRE_CLK_IN_MAX_M8B;
		cConf->div_pre_in_fmax = DIV_PRE_CLK_IN_MAX_M8B;
		cConf->div_pre_out_fmax = DIV_POST_CLK_IN_MAX_M8B;
		cConf->div_post_out_fmax = CRT_VID_CLK_IN_MAX_M8B;
		cConf->xd_out_fmax = ENCL_CLK_IN_MAX_M8B;
		break;
	default:
		break;
	}
	cConf->pll_out_fmin = cConf->pll_vco_fmin /
		od_table[cConf->pll_od_sel_max - 1];
}

void lcd_config_init(struct Lcd_Config_s *pConf)
{
	if (lcd_chip_valid_check(pConf) == FALSE)
		return;

	/* must before generate_clk_parameter, otherwise the clk parameter will
	   not update base on the edp_link_rate */
	lcd_control_config_pre(pConf);

	if ((pConf->lcd_timing.clk_ctrl >> CLK_CTRL_AUTO) & 1) {
		DPRINT("\nAuto generate clock parameters\n");
		generate_clk_parameter(pConf, &clk_Conf);
	} else {
		DPRINT("\nCustome clock parameters\n");
		parse_clk_parameter(pConf, &clk_Conf);
	}
	DPRINT("pll_ctrl=0x%x, div_ctrl=0x%x, clk_ctrl=0x%x\n",
		pConf->lcd_timing.pll_ctrl, pConf->lcd_timing.div_ctrl,
		pConf->lcd_timing.clk_ctrl);

	lcd_sync_duration(pConf, &clk_Conf);
	lcd_tcon_config(pConf);

	lcd_control_config_post(pConf);
}

void lcd_config_probe(struct Lcd_Config_s *pConf)
{
	spin_lock_init(&gamma_write_lock);
	spin_lock_init(&lcd_clk_lock);

	lcd_Conf = pConf;
	lcd_chip_type = lcd_detect_chip();
	lcd_clk_config_init_by_chip(&clk_Conf);
	if (lcd_chip_valid_check(pConf) == FALSE)
		return;

	lcd_config_assign(pConf);
	if (lcd_reg_read(ENCL_VIDEO_EN) & 1)
		pConf->lcd_misc_ctrl.lcd_status = 1;
	else
		pConf->lcd_misc_ctrl.lcd_status = 0;

	switch (pConf->lcd_basic.lcd_type) {
	case LCD_DIGITAL_MIPI:
		dsi_probe(pConf);
		break;
	case LCD_DIGITAL_EDP:
		edp_probe(pConf);
		lcd_config_edp_edid_load();
		break;
	default:
		break;
	}

	creat_lcd_video_attr(pConf);
}

void lcd_config_remove(struct Lcd_Config_s *pConf)
{
	if (lcd_chip_valid_check(pConf) == FALSE)
		return;

	remove_lcd_video_attr(pConf);

	switch (pConf->lcd_basic.lcd_type) {
	case LCD_DIGITAL_MIPI:
		dsi_remove(pConf);
		break;
	case LCD_DIGITAL_EDP:
		edp_remove(pConf);
		break;
	default:
		break;
	}
}
