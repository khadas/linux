/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * include/linux/amlogic/media/di/di.h
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

#ifndef DI_H
#define DI_H

/************************************************
 * dil_set_diffver_flag
 *	ref to dil_get_diffver_flag
 *	DI_DRV_OLD_DEINTERLACE	: old deinterlace
 *	DI_DRV_MULTI		: di_multi
 ************************************************/
void dil_set_diffver_flag(unsigned int para);

struct reg_acc {
	void (*wr)(unsigned int adr, unsigned int val);
	unsigned int (*rd)(unsigned int adr);
	unsigned int (*bwr)(unsigned int adr, unsigned int val,
			    unsigned int start, unsigned int len);
	unsigned int (*brd)(unsigned int adr, unsigned int start,
			    unsigned int len);
};

void dim_post_keep_cmd_release2(struct vframe_s *vframe);

/************************************************
 * dim_polic_cfg
 ************************************************/
void dim_polic_cfg(unsigned int cmd, bool on);
#define K_DIM_BYPASS_CLEAR_ALL	(0)
#define K_DIM_I_FIRST		(1)
#define K_DIM_BYPASS_ALL_P	(2)

/************************************************
 * di_api_get_instance_id
 *	only for deinterlace
 *	get current instance_id
 ************************************************/
u32 di_api_get_instance_id(void);

/************************************************
 * di_api_post_disable
 *	only for deinterlace
 ************************************************/

void di_api_post_disable(void);

/**************************************************
 * function:
 *	get report information from di;
 * version:
 *
 * spt_bits: bit 0 for decontour,
 *	is 1 when there are decontour information;
 *	is 0 when there are no decontour information;
 * dct_map_0: DCTR_MAP_HIST_0
 * dct_sta_0: DCTR_STA_HIST_0
 * dct_sta_1: DCTR_STA_HIST_1
 **************************************************/
struct dim_rpt_s {
	unsigned int version;
	unsigned int spt_bits;/*bit 0: dct*/
	unsigned int dct_map_0;
	unsigned int dct_map_1;
	unsigned int dct_map_2;
	unsigned int dct_map_3;
	unsigned int dct_map_15;
	unsigned int dct_bld_2;
};

enum DIM_DB_SV {
	DIM_DB_SV_DCT_BL2, /* DCTR_BLENDING2 */
};

#define DIM_DB_SAVE_NUB		1

/**************************************************
 * function:
 *	get report information from di;
 * vfm:
 *	input vframe;
 * return:
 *	NULL: there is no report information from vfm;
 *	other: refer to struct dim_rpt_s
 **************************************************/
struct dim_rpt_s *dim_api_getrpt(struct vframe_s *vfm);

/**************************************************
 * function:
 *	select db value or pq value;
 * idx:
 *	ref to DIM_DB_SV's define
 * mode:
 *	0: db value;
 *	1: value from pdate;
 * pdate: only useful when mode is 1.
 *	value / mask
 **************************************************/
bool dim_pq_db_sel(unsigned int idx, unsigned int mode, unsigned int *pdate);
void dim_pq_db_setreg(unsigned int nub, unsigned int *preg);
bool dim_config_crc_ic(void);
#endif /* VIDEO_H */
