/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * drivers/amlogic/media/vout/cvbs/cvbs_out.h
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

#ifndef _CVBS_OUT_H_
#define _CVBS_OUT_H_

/* Standard Linux Headers */
#include <linux/workqueue.h>

/* Amlogic Headers */
#include <linux/amlogic/media/vout/vout_notify.h>
#include "cvbs_mode.h"

/* sync kernel 4.9 code */
#define CVBSOUT_VER "Ref.2020/10/23"

#define CVBS_CLASS_NAME	"cvbs"
#define CVBS_NAME	"cvbs"
#define	MAX_NUMBER_PARA  10

#define _TM_V 'V'

#ifdef CONFIG_AML_VOUT_CC_BYPASS
#define MAX_RING_BUFF_LEN 128
#define VOUT_IOC_CC_OPEN           _IO(_TM_V, 0x01)
#define VOUT_IOC_CC_CLOSE          _IO(_TM_V, 0x02)
#define VOUT_IOC_CC_DATA           _IOW(_TM_V, 0x03, struct vout_cc_parm_s)
#endif

struct reg_s {
	unsigned int reg;
	unsigned int val;
};

enum cvbs_cpu_type {
	CVBS_CPU_TYPE_G12A   = 0,
	CVBS_CPU_TYPE_G12B   = 1,
	CVBS_CPU_TYPE_TL1    = 2,
	CVBS_CPU_TYPE_SM1    = 3,
	CVBS_CPU_TYPE_TM2    = 4,
	CVBS_CPU_TYPE_SC2    = 5,
	CVBS_CPU_TYPE_T5     = 6,
	CVBS_CPU_TYPE_T5D    = 7,
	CVBS_CPU_TYPE_S4     = 8,
};

struct meson_cvbsout_data {
	enum cvbs_cpu_type cpu_id;
	const char *name;
	unsigned int vdac_vref_adj;
	unsigned int vdac_gsw;
	unsigned int reg_vid_pll_clk_div;
	unsigned int reg_vid_clk_div;
	unsigned int reg_vid_clk_ctrl;
	unsigned int reg_vid2_clk_div;
	unsigned int reg_vid2_clk_ctrl;
	unsigned int reg_vid_clk_ctrl2;
};

#define CVBS_PERFORMANCE_CNT_MAX    20
struct performance_config_s {
	unsigned int reg_cnt;
	struct reg_s *reg_table;
};

/* cvbs driver flag */
#define CVBS_FLAG_EN_ENCI   BIT(0)
#define CVBS_FLAG_EN_VDAC   BIT(1)

struct cvbs_drv_s {
	struct vinfo_s *vinfo;
	struct cdev   *cdev;
	dev_t         devno;
	struct class  *base_class;
	struct device *dev;
	struct meson_cvbsout_data *cvbs_data;
	struct performance_config_s perf_conf_pal;
	struct performance_config_s perf_conf_ntsc;
	struct delayed_work vdac_dwork;
	unsigned int flag;

	/* clktree */
	unsigned int clk_gate_state;
	struct clk *venci_top_gate;
	struct clk *venci_0_gate;
	struct clk *venci_1_gate;
	struct clk *vdac_clk_gate;
#ifdef CONFIG_AMLOGIC_VPU
	struct vpu_dev_s *cvbs_vpu_dev;
#endif
};

static  DEFINE_MUTEX(cvbs_mutex);

#ifdef CONFIG_AML_VOUT_CC_BYPASS
struct cc_parm_s {
	unsigned int type;
	unsigned int data;
};

struct cc_ring_mgr_s {
	unsigned int max_len;
	unsigned int rp;
	unsigned int wp;
	int over_flag;
	struct cc_parm_s cc_data[MAX_RING_BUFF_LEN];
};

struct vout_cc_parm_s {
	unsigned int type;
	unsigned char data1;
	unsigned char data2;
};
#endif

struct cvbsregs_set_t {
	enum cvbs_mode_e cvbsmode;
	const struct reg_s *enc_reg_setting;
};

void amvecm_clip_range_limit(bool limit_en);

int cvbs_cpu_type(void);
const struct vinfo_s *get_valid_vinfo(char  *mode);
int cvbs_set_current_vmode(enum vmode_e mode);
struct meson_cvbsout_data *get_cvbs_data(void);

#endif
