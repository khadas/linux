/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * drivers/amlogic/media/enhancement/amvecm/amve.h
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

#ifndef __AM_VE_H
#define __AM_VE_H

#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/amvecm/ve.h>
#include "linux/amlogic/media/amvecm/cm.h"
#include "linux/amlogic/media/amvecm/amvecm.h"

enum pq_ctl_cfg_e {
	TV_CFG_DEF = 0,
	OTT_CFG_DEF,
	TV_DV_BYPASS,
	OTT_DV_BYPASS,
	PQ_CFG_MAX
};

struct ve_regs_s {
	unsigned int val:32;
	unsigned int reg:14;
	unsigned int port:2;
			/* 0    NA  NA direct access */
			/* 1    VPP_CHROMA_ADDR_PORT */
			/* VPP_CHROMA_DATA_PORT CM port registers */
			/* 2    NA NA reserved */
			/* 3    NA NA reserved */
	unsigned int bit:5;
	unsigned int wid:5;
	unsigned int mode:1;
	unsigned int rsv:5;
};

enum pst_hist_mod {
	HIST_VB = 0,
	HIST_UG,
	HIST_YR,
	HIST_MAXRGB
};

enum pst_hist_pos {
	BEFORE_POST2_MTX = 0,
	AFTER_POST2_MTX,
	POS_MAX
};

enum pw_state_e {
	PW_ON = 0,
	PW_OFF,
	PW_MAX,
};

extern unsigned int gamma_loadprotect_en;
extern struct ve_hist_s video_ve_hist;
void ve_hist_gamma_reset(void);
extern unsigned int ve_size;
extern struct ve_dnlp_s am_ve_dnlp;
extern struct tcon_gamma_table_s video_gamma_table_r;
extern struct tcon_gamma_table_s video_gamma_table_g;
extern struct tcon_gamma_table_s video_gamma_table_b;
extern struct tcon_gamma_table_s video_gamma_table_ioctl_set;
extern struct tcon_gamma_table_s video_gamma_table_r_adj;
extern struct tcon_gamma_table_s video_gamma_table_g_adj;
extern struct tcon_gamma_table_s video_gamma_table_b_adj;
extern struct tcon_rgb_ogo_s     video_rgb_ogo;
extern struct gm_tbl_s gt;
extern unsigned int gamma_index;
extern unsigned int gamma_index_sub;
extern unsigned int gm_par_idx;
extern unsigned int *plut3d;

extern struct tcon_gamma_table_s video_gamma_table_r_sub;
extern struct tcon_gamma_table_s video_gamma_table_g_sub;
extern struct tcon_gamma_table_s video_gamma_table_b_sub;
extern struct tcon_rgb_ogo_s video_rgb_ogo_sub;

extern spinlock_t vpp_lcd_gamma_lock;
extern spinlock_t vpp_3dlut_lock;
extern struct mutex vpp_lut3d_lock;
extern int lut3d_en;/*0:disable;1:enable */
extern int lut3d_order;/* 0 RGB 1 GBR */
extern int lut3d_debug;

extern u16 gamma_data_r[257];
extern u16 gamma_data_g[257];
extern u16 gamma_data_b[257];
void vpp_get_lcd_gamma_table(u32 rgb_mask);
void vpp_get_lcd_gamma_table_sub(void);

void ve_on_vs(struct vframe_s *vf);

void ve_set_bext(struct ve_bext_s *p);
void ve_set_dnlp(struct ve_dnlp_s *p);
void ve_set_dnlp_2(void);
void ve_set_hsvs(struct ve_hsvs_s *p);
void ve_set_ccor(struct ve_ccor_s *p);
void ve_set_benh(struct ve_benh_s *p);
void ve_set_demo(struct ve_demo_s *p);
void ve_set_regs(struct ve_regs_s *p);
void ve_set_regmap(struct ve_regmap_s *p);

void ve_enable_dnlp(void);
void ve_disable_dnlp(void);

void ve_dnlp_ctrl_vsync(int enable);

int vpp_get_encl_viu_mux(void);
int vpp_get_vout_viu_mux(void);
void vpp_enable_lcd_gamma_table(int viu_sel, int rdma_write);
void vpp_disable_lcd_gamma_table(int viu_sel, int rdma_write);
void vpp_set_lcd_gamma_table(u16 *data, u32 rgb_mask, int viu_sel);
void amve_write_gamma_table(u16 *data, u32 rgb_mask);
void amve_write_gamma_table_sub(u16 *data, u32 rgb_mask);
void vpp_set_rgb_ogo_sub(struct tcon_rgb_ogo_s *p);
void vpp_set_rgb_ogo(struct tcon_rgb_ogo_s *p);
void vpp_phase_lock_on_vs(unsigned int cycle,
			  unsigned int stamp,
			  bool lock50,
			  unsigned int range_fast,
			  unsigned int range_slow);
/* #if (MESON_CPU_TYPE>=MESON_CPU_TYPE_MESON6TVD) */
void ve_frame_size_patch(unsigned int width, unsigned int height);
/* #endif */
void ve_dnlp_latch_process(void);
void ve_lcd_gamma_process(void);
void lvds_freq_process(void);
void ve_dnlp_param_update(void);
void ve_new_dnlp_param_update(void);
void ve_lc_curve_update(void);
void ve_lc_latch_process(void);
void ve_ogo_param_update_sub(void);
void ve_ogo_param_update(void);
void am_set_regmap(struct am_regs_s *p);
void sharpness_process(struct vframe_s *vf);
void amvecm_bricon_process(signed int bri_val,
			   signed int cont_val,
			   struct vframe_s *vf);
void amvecm_color_process(signed int sat_val,
			  signed int hue_val,
			  struct vframe_s *vf);
void amvecm_3d_black_process(void);
void amvecm_3d_sync_process(void);
extern unsigned int vecm_latch_flag;
extern unsigned int cm_size;
extern unsigned int sync_3d_h_start;
extern unsigned int sync_3d_h_end;
extern unsigned int sync_3d_v_start;
extern unsigned int sync_3d_v_end;
extern unsigned int sync_3d_polarity;
extern unsigned int sync_3d_out_inv;
extern unsigned int sync_3d_black_color;
extern unsigned int sync_3d_sync_to_vbo;

extern int fmeter_en;
extern int cur_sr_level;
extern int pre_fmeter_level, cur_fmeter_level, fmeter_flag;
void amve_fmeter_init(int enable);
void amve_fmetersize_config(u32 sr0_w, u32 sr0_h, u32 sr1_w, u32 sr1_h);

/* #if defined(CONFIG_ARCH_MESON2) */
/* unsigned long long ve_get_vs_cnt(void); */
/* #endif */
extern int video_rgb_ogo_xvy_mtx;

#define GAMMA_SIZE 256

extern unsigned int dnlp_sel;
void ve_dnlp_load_reg(void);

/*gxlx sr adaptive setting*/
void amve_sharpness_adaptive_setting(struct vframe_s *vf,
				     unsigned int sps_h_en,
				     unsigned int sps_v_en);
void amve_sharpness_init(void);
extern struct am_regs_s sr1reg_sd_scale;
extern struct am_regs_s sr1reg_hd_scale;
extern struct am_regs_s sr1reg_cvbs;
extern struct am_regs_s sr1reg_hv_noscale;
void amvecm_fresh_overscan(struct vframe_s *vf);
void amvecm_reset_overscan(void);
void ve_hist_gamma_tgt(struct vframe_s *vf);
int vpp_set_lut3d(int bfromkey,
		  int keyindex,
		  unsigned int p3dlut_in[][3],
		  int blut3dcheck);
int vpp_write_lut3d_section(int index,
			    int section_len,
			    unsigned int *p3dlut_section_in);
int vpp_read_lut3d_section(int index,
			   int section_len,
			   unsigned int *p3dlut_section_out);
void vpp_lut3d_table_init(int r, int g, int b);
void vpp_lut3d_table_release(void);
int vpp_enable_lut3d(int enable);
void dump_plut3d_table(void);
void dump_plut3d_reg_table(void);

void amvecm_gamma_init(bool en);
void set_gamma_regs(int en, int sel);
void amvecm_wb_enable(int enable);
void amvecm_wb_enable_sub(int enable);
int vpp_pq_ctrl_config(struct pq_ctrl_s pq_cfg, enum wr_md_e md);
unsigned int skip_pq_ctrl_load(struct am_reg_s *p);
void set_pre_gamma_reg(struct pre_gamma_table_s *pre_gma_tb);
void lcd_gamma_api(unsigned int index, u16 *r_data, u16 *g_data,
	u16 *b_data, enum wr_md_e wr_mod, enum rw_md_e rw_mod);
void vpp_pst_hist_sta_config(int en,
	enum pst_hist_mod mod,
	enum pst_hist_pos pos,
	struct vinfo_s *vinfo);
void vpp_pst_hist_sta_read(unsigned int *hist);
void eye_proc(int mtx_ep[][4], int mtx_on);
void set_vpp_enh_clk(struct vframe_s *vf, struct vframe_s *rpt_vf);
void lut3d_update(unsigned int p3dlut_in[][3]);
#endif

