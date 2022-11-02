/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef AMVE_V2_H
#define AMVE_V2_H

struct cm_port_s {
	int cm_addr_port[4];
	int cm_data_port[4];
};

struct cm_port_s get_cm_port(void);
void post_gainoff_set(struct tcon_rgb_ogo_s *p,
	enum wr_md_e mode);
void ve_brigtness_set(int val,
	enum vadj_index_e vadj_idx,
	enum wr_md_e mode);
void ve_contrast_set(int val,
	enum vadj_index_e vadj_idx,
	enum wr_md_e mode);
void ve_color_mab_set(int mab,
	enum vadj_index_e vadj_idx,
	enum wr_md_e mode);
void ve_color_mcd_set(int mcd,
	enum vadj_index_e vadj_idx,
	enum wr_md_e mode);
int ve_brightness_contrast_get(enum vadj_index_e vadj_idx);
/*vpp module control*/
void ve_vadj_ctl(enum wr_md_e mode, enum vadj_index_e vadj_idx, int en);
void ve_bs_ctl(enum wr_md_e mode, int en);
void ve_ble_ctl(enum wr_md_e mode, int en);
void ve_cc_ctl(enum wr_md_e mode, int en);
void post_wb_ctl(enum wr_md_e mode, int en);
void post_pre_gamma_set(int *lut);
void vpp_luma_hist_init(void);
void get_luma_hist(struct vframe_s *vf);
void cm_top_ctl(enum wr_md_e mode, int en);
#endif

