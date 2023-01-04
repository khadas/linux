// SPDX-License-Identifier: GPL-2.0+
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
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#include <linux/clk.h>
#ifdef CONFIG_AMLOGIC_VPU
#include <linux/amlogic/media/vpu/vpu.h>
#endif
#include "../lcd_reg.h"
#include "../lcd_common.h"
#include "lcd_clk_config.h"
#include "lcd_clk_ctrl.h"
#include "lcd_clk_utils.h"

char *lcd_ss_level_table_tl1[] = {
	"0, disable",
	"1, 2000ppm",
	"2, 4000ppm",
	"3, 6000ppm",
	"4, 8000ppm",
	"5, 10000ppm",
	"6, 12000ppm",
	"7, 14000ppm",
	"8, 16000ppm",
	"9, 18000ppm",
	"10, 20000ppm",
	"11, 22000ppm",
	"12, 24000ppm",
	"13, 25000ppm",
	"14, 28000ppm",
	"15, 30000ppm",
	"16, 32000ppm",
	"17, 33000ppm",
	"18, 36000ppm",
	"19, 38500ppm",
	"20, 40000ppm",
	"21, 42000ppm",
	"22, 44000ppm",
	"23, 45000ppm",
	"24, 48000ppm",
	"25, 50000ppm",
	"26, 50000ppm",
	"27, 54000ppm",
	"28, 55000ppm",
	"29, 55000ppm",
	"30, 60000ppm",
};

char *lcd_ss_freq_table_tl1[] = {
	"0, 29.5KHz",
	"1, 31.5KHz",
	"2, 50KHz",
	"3, 75KHz",
	"4, 100KHz",
	"5, 150KHz",
	"6, 200KHz",
};

char *lcd_ss_mode_table_tl1[] = {
	"0, center ss",
	"1, up ss",
	"2, down ss",
};

unsigned char ss_level_max = 0xff;
unsigned char ss_freq_max = sizeof(lcd_ss_freq_table_tl1) / sizeof(char *);
unsigned char ss_mode_max = sizeof(lcd_ss_freq_table_tl1) / sizeof(char *);

/* for lcd clk init */
spinlock_t lcd_clk_lock;

struct lcd_clk_config_s *get_lcd_clk_config(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;

	if (!pdrv->clk_conf) {
		LCDERR("[%d]: %s: clk_config is null\n", pdrv->index, __func__);
		return NULL;
	}
	cconf = (struct lcd_clk_config_s *)pdrv->clk_conf;
	if (!cconf->data) {
		LCDERR("[%d]: %s: clk config data is null\n",
		       pdrv->index, __func__);
		return NULL;
	}

	return cconf;
}

/* ****************************************************
 * lcd clk function api
 * ****************************************************
 */
void lcd_clk_generate_parameter(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	if (cconf->data->clk_generate_parameter)
		cconf->data->clk_generate_parameter(pdrv);
}

int lcd_get_ss_num(struct aml_lcd_drv_s *pdrv,
	unsigned int *level, unsigned int *freq, unsigned int *mode)
{
	struct lcd_clk_config_s *cconf;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf) {
		LCDERR("[%d] %s: clk_conf is null\n", pdrv->index, __func__);
		return -1;
	}

	if (level)
		*level = (cconf->ss_level >= ss_level_max) ? 0 : cconf->ss_level;

	if (freq)
		*freq = (cconf->ss_freq >= ss_freq_max) ? 0 : cconf->ss_freq;

	if (mode)
		*mode = (cconf->ss_mode >= ss_mode_max) ? 0 : cconf->ss_mode;

	return 0;
}

int lcd_get_ss(struct aml_lcd_drv_s *pdrv, char *buf)
{
	struct lcd_clk_config_s *cconf;
	unsigned int temp;
	int len = 0;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf) {
		len += sprintf(buf + len, "[%d]: clk config data is null\n", pdrv->index);
		return len;
	}

	temp = (cconf->ss_level >= ss_level_max) ? 0 : cconf->ss_level;
	switch (cconf->data->ss_support) {
	case 2:
		len += sprintf(buf + len, "ss_level: %d, %dppm\n", temp, temp * 1000);
		break;
	case 1:
		len += sprintf(buf + len, "ss_level: %s\n", lcd_ss_level_table_tl1[temp]);
		break;
	case 0:
		len += sprintf(buf + len, "[%d]: spread spectrum is invalid\n", pdrv->index);
		return len;
	}

	temp = (cconf->ss_freq >= ss_freq_max) ? 0 : cconf->ss_freq;
	len += sprintf(buf + len, "ss_freq: %s\n", lcd_ss_freq_table_tl1[temp]);

	temp = (cconf->ss_mode >= ss_mode_max) ? 0 : cconf->ss_mode;
	len += sprintf(buf + len, "ss_mode: %s\n", lcd_ss_mode_table_tl1[temp]);

	return len;
}

int lcd_set_ss(struct aml_lcd_drv_s *pdrv, unsigned int level, unsigned int freq, unsigned int mode)
{
	struct lcd_clk_config_s *cconf;
	unsigned long flags = 0;
	int ss_adv = 0, ret = 0;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return -1;

	spin_lock_irqsave(&lcd_clk_lock, flags);

	if (level < 0xff) {
		if (level >= ss_level_max) {
			LCDERR("[%d]: %s: ss_level %d is out of support (max %d)\n",
			       pdrv->index, __func__, level,
			       (ss_level_max - 1));
			ret = -1;
			goto lcd_set_ss_end;
		}
		cconf->ss_level = level;
		if (cconf->data->set_ss_level)
			cconf->data->set_ss_level(pdrv);
	}
	if (freq < 0xff) {
		if (freq >= ss_freq_max) {
			LCDERR("[%d]: %s: ss_freq %d is out of support (max %d)\n",
			       pdrv->index, __func__, freq,
			       (ss_freq_max - 1));
			ret = -1;
			goto lcd_set_ss_end;
		}
		cconf->ss_freq = freq;
		ss_adv = 1;
	}
	if (mode < 0xff) {
		if (mode >= ss_mode_max) {
			LCDERR("[%d]: %s: ss_mode %d is out of support (max %d)\n",
			       pdrv->index, __func__, mode,
			       (ss_mode_max - 1));
			ret = -1;
			goto lcd_set_ss_end;
		}
		cconf->ss_mode = mode;
		ss_adv = 1;
	}
	if (ss_adv == 0)
		goto lcd_set_ss_end;
	if (cconf->data->set_ss_advance)
		cconf->data->set_ss_advance(pdrv);

lcd_set_ss_end:
	spin_unlock_irqrestore(&lcd_clk_lock, flags);

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s\n", pdrv->index, __func__);
	return ret;
}

/* design for vlock, don't save ss_level to clk_config */
int lcd_ss_enable(int index, unsigned int flag)
{
	struct aml_lcd_drv_s *pdrv;
	struct lcd_clk_config_s *cconf;
	unsigned long flags = 0;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s\n", index, __func__);

	spin_lock_irqsave(&lcd_clk_lock, flags);

	pdrv = aml_lcd_get_driver(index);
	if (!pdrv) {
		LCDERR("[%d]: %s: drv is null\n", index, __func__);
		goto lcd_ss_enable_end;
	}
	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		goto lcd_ss_enable_end;

	if (cconf->data->clk_ss_enable)
		cconf->data->clk_ss_enable(pdrv, flag);

lcd_ss_enable_end:
	spin_unlock_irqrestore(&lcd_clk_lock, flags);

	return 0;
}

void lcd_clk_ss_config_init(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf = get_lcd_clk_config(pdrv);
	unsigned int ss_level, ss_freq, ss_mode;

	if (!cconf || !cconf->data)
		return;

	ss_level = pdrv->config.timing.ss_level;
	cconf->ss_level = (ss_level >= ss_level_max) ? 0 : ss_level;
	if (cconf->ss_level == 0)
		cconf->ss_en = 0;
	else
		cconf->ss_en = 1;
	ss_freq = pdrv->config.timing.ss_freq;
	cconf->ss_freq = (ss_freq >= ss_freq_max) ? 0 : ss_freq;
	ss_mode = pdrv->config.timing.ss_mode;
	cconf->ss_mode = (ss_mode >= ss_mode_max) ? 0 : ss_mode;
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
		LCDPR("[%d]: %s: ss_level=%d, ss_freq=%d, ss_mode=%d\n",
		      pdrv->index, __func__,
		      cconf->ss_level, cconf->ss_freq, cconf->ss_mode);
	}
}

int lcd_encl_clk_msr(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;
	int clk_mux;
	int encl_clk = 0;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return 0;

	clk_mux = cconf->data->enc_clk_msr_id;
	if (clk_mux == -1)
		return 0;
	encl_clk = meson_clk_measure(clk_mux);

	return encl_clk;
}

void lcd_pll_reset(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;
	struct lcd_clk_ctrl_s *table;
	int i = 0;
	unsigned long flags = 0;

	spin_lock_irqsave(&lcd_clk_lock, flags);

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		goto lcd_pll_reset_end;
	if (!cconf->data->pll_ctrl_table)
		goto lcd_pll_reset_end;

	table = cconf->data->pll_ctrl_table;
	while (i < LCD_CLK_CTRL_CNT_MAX) {
		if (table[i].flag == LCD_CLK_CTRL_END)
			break;
		if (table[i].flag == LCD_CLK_CTRL_RST) {
			lcd_ana_setb(table[i].reg, 1, table[i].bit, table[i].len);
			udelay(10);
			lcd_ana_setb(table[i].reg, 0, table[i].bit, table[i].len);
		}
		i++;
	}

lcd_pll_reset_end:
	spin_unlock_irqrestore(&lcd_clk_lock, flags);
	LCDPR("[%d]: %s\n", pdrv->index, __func__);
}

void lcd_vlock_m_update(int index, unsigned int vlock_m)
{
	struct aml_lcd_drv_s *pdrv;
	struct lcd_clk_config_s *cconf;
	struct lcd_clk_ctrl_s *table;
	int i = 0;
	unsigned long flags = 0;

	spin_lock_irqsave(&lcd_clk_lock, flags);

	pdrv = aml_lcd_get_driver(index);
	if (!pdrv) {
		LCDERR("[%d]: %s: drv is null\n", index, __func__);
		goto lcd_vlock_m_update_end;
	}
	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		goto lcd_vlock_m_update_end;
	if (!cconf->data->pll_ctrl_table) {
		LCDERR("[%d]: %s: pll_ctrl_table null\n",
		       index, __func__);
		goto lcd_vlock_m_update_end;
	}

	vlock_m &= 0xff;
	if (lcd_debug_print_flag & LCD_DBG_PR_ADV2) {
		LCDPR("[%d]: %s, vlcok_m: 0x%x,",
		      index, __func__, vlock_m);
	}

	table = cconf->data->pll_ctrl_table;
	while (i < LCD_CLK_CTRL_CNT_MAX) {
		if (table[i].flag == LCD_CLK_CTRL_M) {
			lcd_ana_setb(table[i].reg, vlock_m, table[i].bit, table[i].len);
			break;
		}
		i++;
	}

lcd_vlock_m_update_end:
	spin_unlock_irqrestore(&lcd_clk_lock, flags);
}

void lcd_vlock_frac_update(int index, unsigned int vlock_frac)
{
	struct aml_lcd_drv_s *pdrv;
	struct lcd_clk_config_s *cconf;
	struct lcd_clk_ctrl_s *table;
	int i = 0;
	unsigned long flags = 0;

	spin_lock_irqsave(&lcd_clk_lock, flags);

	pdrv = aml_lcd_get_driver(index);
	if (!pdrv) {
		LCDERR("[%d]: %s: drv is null\n", index, __func__);
		goto lcd_vlock_frac_update_end;
	}
	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		goto lcd_vlock_frac_update_end;
	if (!cconf->data->pll_ctrl_table) {
		LCDERR("[%d]: %s: pll_ctrl_table null\n",
		       index, __func__);
		goto lcd_vlock_frac_update_end;
	}

	vlock_frac &= 0x1ffff;
	if (lcd_debug_print_flag & LCD_DBG_PR_ADV2) {
		LCDPR("[%d]: %s, vlock_frac: 0x%x\n",
		      index, __func__, vlock_frac);
	}

	table = cconf->data->pll_ctrl_table;
	while (i < LCD_CLK_CTRL_CNT_MAX) {
		if (table[i].flag == LCD_CLK_CTRL_FRAC) {
			lcd_ana_setb(table[i].reg, vlock_frac, table[i].bit, table[i].len);
			break;
		}
		i++;
	}

lcd_vlock_frac_update_end:
	spin_unlock_irqrestore(&lcd_clk_lock, flags);
}

/* for frame rate change */
void lcd_update_clk(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;
	struct lcd_clk_ctrl_s *table;
	unsigned int offset, reg, val;
	int i = 0;
	unsigned long flags = 0;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return; // goto lcd_clk_update_end;

	spin_lock_irqsave(&lcd_clk_lock, flags);

	if (cconf->data->pll_frac_generate)
		cconf->data->pll_frac_generate(pdrv);

	offset = cconf->pll_offset;

	if (!cconf->data->pll_ctrl_table)
		goto lcd_clk_update_end;
	table = cconf->data->pll_ctrl_table;
	while (i < LCD_CLK_CTRL_CNT_MAX) {
		if (table[i].flag == LCD_CLK_CTRL_END)
			break;
		if (table[i].flag == LCD_CLK_CTRL_FRAC) {
			reg = table[i].reg + offset;
			val = lcd_ana_read(reg);
			lcd_ana_setb(reg, cconf->pll_frac, table[i].bit, table[i].len);
			if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
				LCDPR("[%d]: %s: pll_frac reg 0x%x: 0x%08x->0x%08x\n",
					pdrv->index, __func__, reg,
					val, lcd_ana_read(reg));
			}
		}
		i++;
	}

lcd_clk_update_end:
	spin_unlock_irqrestore(&lcd_clk_lock, flags);
	LCDPR("[%d]: %s: pll_frac=0x%x\n", pdrv->index, __func__, cconf->pll_frac);
}

/* for timing change */
void lcd_set_clk(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;
	unsigned long flags = 0;
	int cnt = 0;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	if (pdrv->lcd_pxp) {
		if (cconf->data->vclk_crt_set)
			cconf->data->vclk_crt_set(pdrv);
		return;
	}

lcd_set_clk_retry:
	spin_lock_irqsave(&lcd_clk_lock, flags);
	if (cconf->data->clk_set)
		cconf->data->clk_set(pdrv);
	if (cconf->data->vclk_crt_set)
		cconf->data->vclk_crt_set(pdrv);
	spin_unlock_irqrestore(&lcd_clk_lock, flags);
	usleep_range(10000, 10001);

	while (lcd_clk_msr_check(pdrv)) {
		if (cnt++ >= 10) {
			LCDERR("[%d]: %s timeout\n", pdrv->index, __func__);
			break;
		}
		goto lcd_set_clk_retry;
	}

	if (cconf->data->clktree_set)
		cconf->data->clktree_set(pdrv);

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s\n", pdrv->index, __func__);
}

void lcd_disable_clk(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	if (cconf->data->clk_disable)
		cconf->data->clk_disable(pdrv);

	LCDPR("[%d]: %s\n", pdrv->index, __func__);
}

static void lcd_clk_gate_optional_switch(struct aml_lcd_drv_s *pdrv, int status)
{
	struct lcd_clk_config_s *cconf;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	if (!cconf->data->clk_gate_optional_switch)
		return;

	if (status) {
		if (cconf->clktree.clk_gate_optional_state) {
			LCDPR("[%d]: clk_gate_optional is already on\n",
			      pdrv->index);
		} else {
			cconf->data->clk_gate_optional_switch(pdrv, 1);
		}
	} else {
		if (cconf->clktree.clk_gate_optional_state == 0) {
			LCDPR("[%d]: clk_gate_optional is already off\n",
			      pdrv->index);
		} else {
			cconf->data->clk_gate_optional_switch(pdrv, 0);
		}
	}
}

void lcd_clk_gate_switch(struct aml_lcd_drv_s *pdrv, int status)
{
	struct lcd_clk_config_s *cconf;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	if (status) {
		if (cconf->clktree.clk_gate_state) {
			LCDPR("[%d]: clk_gate is already on\n", pdrv->index);
		} else {
#ifdef CONFIG_AMLOGIC_VPU
			vpu_dev_clk_gate_on(pdrv->lcd_vpu_dev);
#endif
			if (cconf->data->clk_gate_switch)
				cconf->data->clk_gate_switch(pdrv, 1);
			cconf->clktree.clk_gate_state = 1;
		}
		lcd_clk_gate_optional_switch(pdrv, 1);
	} else {
		lcd_clk_gate_optional_switch(pdrv, 0);
		if (cconf->clktree.clk_gate_state == 0) {
			LCDPR("[%d]: clk_gate is already off\n",  pdrv->index);
		} else {
			if (cconf->data->clk_gate_switch)
				cconf->data->clk_gate_switch(pdrv, 0);
#ifdef CONFIG_AMLOGIC_VPU
			vpu_dev_clk_gate_off(pdrv->lcd_vpu_dev);
#endif
			cconf->clktree.clk_gate_state = 0;
		}
	}
}

int lcd_clk_clkmsr_print(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	struct lcd_clk_config_s *cconf;
	int clk;
	int n, len = 0;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf) {
		n = lcd_debug_info_len(len + offset);
		len += snprintf((buf + len), n, "[%d]: %s: clk config is null\n",
				pdrv->index, __func__);
		return len;
	}

	if (cconf->data->enc_clk_msr_id == -1)
		goto lcd_clk_clkmsr_print_step_1;
	clk = meson_clk_measure(cconf->data->enc_clk_msr_id);
	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n,
		"encl_clk:    %u\n", clk);

lcd_clk_clkmsr_print_step_1:
	if (cconf->data->fifo_clk_msr_id == -1)
		goto lcd_clk_clkmsr_print_step_2;
	clk = meson_clk_measure(cconf->data->fifo_clk_msr_id);
	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n,
		"fifo_clk:    %u\n", clk);

lcd_clk_clkmsr_print_step_2:
	switch (pdrv->config.basic.lcd_type) {
	case LCD_MLVDS:
	case LCD_P2P:
		if (cconf->data->tcon_clk_msr_id == -1)
			break;
		clk = meson_clk_measure(cconf->data->tcon_clk_msr_id);
		n = lcd_debug_info_len(len + offset);
		len += snprintf((buf + len), n,
			"tcon_clk:    %u\n", clk);
	default:
		break;
	}

	return len;
}

int lcd_clk_config_print(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	struct lcd_clk_config_s *cconf;
	int n, len = 0;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf) {
		n = lcd_debug_info_len(len + offset);
		len += snprintf((buf + len), n, "[%d]: %s: clk config is null\n",
				pdrv->index, __func__);
		return len;
	}

	if (cconf->data->clk_config_print)
		len = cconf->data->clk_config_print(pdrv, buf, offset);

	return len;
}

int lcd_clk_path_change(struct aml_lcd_drv_s *pdrv, int sel)
{
	struct lcd_clk_config_s *cconf;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return -1;

	if (cconf->clk_path_change)
		cconf->clk_path_change(pdrv, sel);

	if (lcd_debug_print_flag & LCD_DBG_PR_ADV2) {
		if (cconf->data->clk_config_init_print)
			cconf->data->clk_config_init_print(pdrv);
	}

	return 0;
}

static void lcd_clk_config_chip_init(struct aml_lcd_drv_s *pdrv, struct lcd_clk_config_s *cconf)
{
	cconf->pll_id = 0;
	cconf->pll_offset = 0;
	cconf->fin = FIN_FREQ;
	switch (pdrv->data->chip_type) {
	case LCD_CHIP_AXG:
		lcd_clk_config_chip_init_axg(pdrv, cconf);
		break;
	case LCD_CHIP_G12A:
	case LCD_CHIP_SM1:
		lcd_clk_config_chip_init_g12a(pdrv, cconf);
		break;
	case LCD_CHIP_G12B:
		lcd_clk_config_chip_init_g12b(pdrv, cconf);
		break;
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
	case LCD_CHIP_TL1:
		lcd_clk_config_chip_init_tl1(pdrv, cconf);
		break;
#endif
	case LCD_CHIP_TM2:
		lcd_clk_config_chip_init_tm2(pdrv, cconf);
		break;
	case LCD_CHIP_T5:
		lcd_clk_config_chip_init_t5(pdrv, cconf);
		break;
	case LCD_CHIP_T5D:
		lcd_clk_config_chip_init_t5d(pdrv, cconf);
		break;
	case LCD_CHIP_T7:
		lcd_clk_config_chip_init_t7(pdrv, cconf);
		break;
	case LCD_CHIP_T3: /* only one pll */
		lcd_clk_config_chip_init_t3(pdrv, cconf);
		break;
	case LCD_CHIP_T5W:
		lcd_clk_config_chip_init_t5w(pdrv, cconf);
		break;
	default:
		LCDPR("[%d]: %s: invalid chip type\n", pdrv->index, __func__);
		return;
	}

	ss_level_max = (cconf->data->ss_support == 1) ?
				sizeof(lcd_ss_level_table_tl1) / sizeof(char *) : ss_level_max;

	cconf->pll_od_fb = cconf->data->pll_od_fb;
	if (lcd_debug_print_flag & LCD_DBG_PR_ADV2) {
		if (cconf->data->clk_config_init_print)
			cconf->data->clk_config_init_print(pdrv);
	}
}

void lcd_clk_config_probe(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;

	cconf = kzalloc(sizeof(*cconf), GFP_KERNEL);
	if (!cconf)
		return;
	pdrv->clk_conf = (void *)cconf;

	lcd_clk_config_chip_init(pdrv, cconf);
	if (!cconf->data)
		return;

	if (cconf->data->clktree_probe)
		cconf->data->clktree_probe(pdrv);
}

void aml_lcd_prbs_test(struct aml_lcd_drv_s *pdrv, unsigned int ms, unsigned int mode_flag)
{
	struct lcd_clk_config_s *cconf;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	if (cconf->data->prbs_test)
		cconf->data->prbs_test(pdrv, ms, mode_flag);
}

void lcd_clk_config_remove(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;

	if (!pdrv->clk_conf)
		return;

	cconf = (struct lcd_clk_config_s *)pdrv->clk_conf;
	if (cconf->data && cconf->data->clktree_remove)
		cconf->data->clktree_remove(pdrv);

	kfree(cconf);
	pdrv->clk_conf = NULL;
}

void lcd_clk_config_init(void)
{
	spin_lock_init(&lcd_clk_lock);
}
