/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * drivers/amlogic/media/enhancement/amvecm/vlock.h
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
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

#ifndef __AM_VLOCK_H
#define __AM_VLOCK_H
#include <linux/notifier.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include "linux/amlogic/media/amvecm/ve.h"

#define VLOCK_VER "Ref.2021/0226: clean log"

#define VLOCK_REG_NUM	33

struct vdin_sts {
	unsigned int lcnt_sts;
	unsigned int com_sts0;
	unsigned int com_sts1;
};

struct vlock_log_s {
	unsigned int pll_m;
	unsigned int pll_frac;
	signed int line_num_adj;
	unsigned int enc_line_max;
	signed int pixel_num_adj;
	unsigned int enc_pixel_max;
	signed int T0;
	signed int vdif_err;
	signed int err_sum;
	signed int margin;
	unsigned int vlock_regs[VLOCK_REG_NUM];
};

struct reg_map {
	unsigned int phy_addr;
	unsigned int size;
	void __iomem *p;
};

enum vlock_regmap_e {
	REG_MAP_VPU = 0,
	REG_MAP_HIU,
	REG_MAP_ANACTRL,/*enc*/
	REG_MAP_END,
};

struct vlk_reg_map_tab {
	unsigned int base;
	unsigned int size;
};

enum vlock_param_e {
	VLOCK_EN = 0x0,
	VLOCK_ADAPT,
	VLOCK_MODE,
	VLOCK_DIS_CNT_LIMIT,
	VLOCK_DELTA_LIMIT,
	VLOCK_PLL_M_LIMIT,
	VLOCK_DELTA_CNT_LIMIT,
	VLOCK_DEBUG,
	VLOCK_DYNAMIC_ADJUST,
	VLOCK_STATE,
	VLOCK_SYNC_LIMIT_FLAG,
	VLOCK_DIS_CNT_NO_VF_LIMIT,
	VLOCK_LINE_LIMIT,
	VLOCK_SUPPORT,
	VLOCK_PARAM_MAX,
};

struct stvlock_sig_sts {
	u32 fsm_sts;
	u32 fsm_prests;
	u32 fsm_pause;
	u32 vf_sts;
	u32 vmd_chg;
	u32 frame_cnt_in;
	u32 frame_cnt_no;
	u32 input_hz;
	u32 output_hz;
	bool md_support;
	u32 video_inverse;
	u32 phlock_percent;
	u32 phlock_sts;
	u32 phlock_en;
	u32 phlock_cnt;
	u32 frqlock_sts;
	/*u32 frqlock_stable_cnt;*/
	u32 ss_sts;
	u32 pll_mode_pause;
	struct vecm_match_data_s *dtdata;
	u32 val_frac;
	u32 val_m;
	struct vdin_sts vdinsts;

	u32 start_chk_ph;
	u32 all_lock_cnt;
};

enum vlock_change {
	VLOCK_CHG_NONE = 0,
	VLOCK_CHG_PH_UNCLOCK,
	VLOCK_CHG_NEED_RESET,
};

#define diff(a, b) ({ \
	typeof(a) _a = a; \
	typeof(b) _b = b; \
	(((_a) > (_b)) ? ((_a) - (_b)) : ((_b) - (_a))); })

void amve_vlock_process(struct vframe_s *vf);
void amve_vlock_resume(void);
void vlock_param_set(unsigned int val, enum vlock_param_e sel);
void vlock_status(void);
void vlock_reg_dump(void);
void vlock_log_start(void);
void vlock_log_stop(void);
void vlock_log_print(void);

#define VLOCK_STATE_NULL 0
#define VLOCK_STATE_ENABLE_STEP1_DONE 1
#define VLOCK_STATE_ENABLE_STEP2_DONE 2
#define VLOCK_STATE_DISABLE_STEP1_DONE 3
#define VLOCK_STATE_DISABLE_STEP2_DONE 4
#define VLOCK_STATE_ENABLE_FORCE_RESET 5

/* video lock */
enum VLOCK_MD {
	VLOCK_MODE_AUTO_ENC = 0x01,
	VLOCK_MODE_AUTO_PLL = 0x02,
	VLOCK_MODE_MANUAL_PLL = 0x04,
	VLOCK_MODE_MANUAL_ENC = 0x08,
	VLOCK_MODE_MANUAL_SOFT_ENC = 0x10,
	VLOCK_MODE_MANUAL_MIX_PLL_ENC = 0x20,
};

/* ------------------ reg ----------------------*/
/*base 0xfe008000*/
#define ANACTRL_TCON_PLL_VLOCK		(0x00f2 << 2)

#define ANACTRL_TCON_PLL0_CNTL0		(0x00e0 << 2)/*M(7:0) N(14:10)*/
#define ANACTRL_TCON_PLL0_CNTL1		(0x00e1 << 2)/*frac(18:0)*/
#define ANACTRL_TCON_PLL1_CNTL0		(0x00e5 << 2)
#define ANACTRL_TCON_PLL1_CNTL1		(0x00e6 << 2)
#define ANACTRL_TCON_PLL2_CNTL0		(0x00ea << 2)
#define ANACTRL_TCON_PLL2_CNTL1		(0x00eb << 2)
/* ------------------ reg ----------------------*/

#define IS_MANUAL_MODE(md)	((md) & \
				(VLOCK_MODE_MANUAL_PLL | \
				VLOCK_MODE_MANUAL_ENC |	\
				VLOCK_MODE_MANUAL_SOFT_ENC))

#define IS_AUTO_MODE(md)	((md) & \
				(VLOCK_MODE_AUTO_PLL | \
				VLOCK_MODE_AUTO_ENC))

#define IS_PLL_MODE(md)	((md) & \
				(VLOCK_MODE_MANUAL_PLL | \
				VLOCK_MODE_AUTO_PLL))

#define IS_ENC_MODE(md)	((md) & \
				(VLOCK_MODE_MANUAL_ENC | \
				VLOCK_MODE_MANUAL_SOFT_ENC | \
				VLOCK_MODE_AUTO_ENC))

#define IS_AUTO_PLL_MODE(md) ((md) & \
					VLOCK_MODE_AUTO_PLL)

#define IS_AUTO_ENC_MODE(md) ((md) & \
				VLOCK_MODE_AUTO_ENC)

#define IS_MANUAL_ENC_MODE(md) ((md) & \
				VLOCK_MODE_MANUAL_ENC)

#define IS_MANUAL_PLL_MODE(md) ((md) & \
				VLOCK_MODE_MANUAL_PLL)

#define IS_MANUAL_SOFTENC_MODE(md) ((md) & \
				VLOCK_MODE_MANUAL_SOFT_ENC)

enum vlock_pll_sel {
	vlock_pll_sel_tcon = 0,
	vlock_pll_sel_hdmi,
	vlock_pll_sel_disable = 0xf,
};

#define VLOCK_START_CNT		50
#define VLOCK_WORK_CNT	(VLOCK_START_CNT + 10)

#define VLOCK_UPDATE_M_CNT	8
#define VLOCK_UPDATE_F_CNT	4

#define XTAL_VLOCK_CLOCK   24000000/*vlock use xtal clock*/

#define VLOCK_SUPPORT_HDMI BIT(0)
#define VLOCK_SUPPORT_CVBS BIT(1)
/*25 to 50, 30 to 60*/
#define VLOCK_SUPPORT_1TO2 0x4

#define VLOCK_SUP_MODE	(VLOCK_SUPPORT_HDMI | VLOCK_SUPPORT_CVBS | \
			 VLOCK_SUPPORT_1TO2)

/*10s for 60hz input,vlock pll stabel cnt limit*/
#define VLOCK_PLL_STABLE_LIMIT	600
#define VLOCK_ENC_STABLE_CNT	180/*vlock enc stabel cnt limit*/
#define VLOCK_PLL_ADJ_LIMIT 9/*vlock pll adj limit(0x300a default)*/

/*vlock_debug mask*/
#define VLOCK_DEBUG_INFO (0x1)
#define VLOCK_DEBUG_FLUSH_REG_DIS (0x2)
#define VLOCK_DEBUG_ENC_LINE_ADJ_DIS (0x4)
#define VLOCK_DEBUG_ENC_PIXEL_ADJ_DIS (0x8)
#define VLOCK_DEBUG_AUTO_MODE_LOG_EN (0x10)
#define VLOCK_DEBUG_PLL2ENC_DIS (0x20)
#define VLOCK_DEBUG_FSM_DIS (0x40)
#define VLOCK_DEBUG_INFO_ERR	(BIT(15))

/* 0:enc;1:pll;2:manual pll */
extern unsigned int vlock_mode;
extern unsigned int vlock_en;
extern unsigned int vecm_latch_flag;
/*extern void __iomem *amvecm_hiu_reg_base;*/
extern unsigned int probe_ok;
extern u32 phase_en_after_frqlock;
extern u32 vlock_ss_en;

void lcd_ss_enable(bool flag);
unsigned int lcd_ss_status(void);
int amvecm_hiu_reg_read(unsigned int reg, unsigned int *val);
int amvecm_hiu_reg_write(unsigned int reg, unsigned int val);
void vdin_vlock_input_sel(unsigned int type,
			  enum vframe_source_type_e source_type);
void vlock_param_config(struct device_node *node);
#ifdef CONFIG_AMLOGIC_LCD
extern struct work_struct aml_lcd_vlock_param_work;
void vlock_lcd_param_work(struct work_struct *p_work);
#endif
int vlock_notify_callback(struct notifier_block *block,
			  unsigned long cmd, void *para);
#endif
void vlock_status_init(void);
void vlock_dt_match_init(struct vecm_match_data_s *pdata);
void vlock_set_en(bool en);
void vlock_set_phase(u32 percent);
void vlock_set_phase_en(u32 en);

void lcd_vlock_m_update(unsigned int vlock_m);
void lcd_vlock_frac_update(unsigned int vlock_frac);
int lcd_set_ss(unsigned int level, unsigned int freq, unsigned int mode);
ssize_t vlock_debug_store(struct class *cla,
			  struct class_attribute *attr,
			  const char *buf, size_t count);
ssize_t vlock_debug_show(struct class *cla,
			 struct class_attribute *attr, char *buf);
void vlock_clk_config(struct device *dev);

