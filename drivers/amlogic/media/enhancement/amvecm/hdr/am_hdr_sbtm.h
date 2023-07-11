/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef _AM_HDR_SBTM_H
#define _AM_HDR_SBTM_H

#include <linux/amlogic/media/amvecm/hdr2_ext.h>

#include "../amcsc.h"

enum sbtm_en_mode {
	SBTM_MODE_OFF = 0,
	SBTM_MODE_GRDM,
	SBTM_MODE_DRDM,
	SBTM_MODE_MAX
};

enum sbtm_type {
	SBTM_TYPE_OFF = 0,
	SBTM_TYPE_COMPOSITED,
	SBTM_TYPE_GAME_PC,
	SBTM_TYPE_VIDEO,
	SBTM_TYPE_MAX
};

enum sbtm_drdm_gamut {
	SBTM_DRDM_GAMUT_SPECIFIED = 0,
	SBTM_DRDM_GAMUT_709,
	SBTM_DRDM_GAMUT_P3,
	SBTM_DRDM_GAMUT_2020,
	SBTM_DRDM_GAMUT_MAX
};

enum sbtm_drdm_hgig_sel {
	SBTM_DRDM_HGIG_SEL_0 = 0,
	SBTM_DRDM_HGIG_SEL_1,
	SBTM_DRDM_HGIG_SEL_2,
	SBTM_DRDM_HGIG_SEL_3,
	SBTM_DRDM_HGIG_SEL_4,
	SBTM_DRDM_HGIG_SEL_MAX
};

enum sbtm_grdm_support {
	SBTM_GRDM_OFF = 0,
	SBTM_GRDM_SDR,
	SBTM_GRDM_HDR,
	SBTM_GRDM_MAX
};

enum sbtm_osd_index_e {
	SBTM_OSD1 = 0,
	SBTM_OSD2,
	SBTM_OSD3,
	SBTM_OSD4,
	SBTM_OSD_MAX,
	SBTM_OSD_ERR
};

enum sbtm_hdr_proc_sel {
	SBTM_OSD_PROC = BIT(0),
	SBTM_SDR_HDR_PROC = BIT(1),
	SBTM_HDR_HDR_PROC = BIT(2),
	SBTM_PROC_MAX = BIT(3),
};

struct sbtmdb_tgt_parm {
	u32 dest_primary[4][2];
	u32 tgt_maxl;     /*lum*/
	u32 pbpct[5];     /*pct * 10000*/
	u32 pbnits[5];    /*lum*/
	u8 numpb;
	u32 frmpblimit;
	u8 minnits;       /*8bit e val*/
};

struct sbtmem_s {
	u8 sbtm_mdoe;
	u8 sbtm_type;
};

extern uint sbtm_en;
extern uint sbtm_mode;
extern uint sbtm_tmo_static;
extern int oo_y_lut_sbtm[HDR2_OOTF_LUT_SIZE];
extern int oo_y_lut_sbtm_osd[HDR2_OOTF_LUT_SIZE];

void sbtm_set_sbtm_enable(uint enable);
void sbtm_mode_set(uint mode);
void sbtm_sbtmdb_set(struct vinfo_s *vinfo);
void sbtm_send_sbtmem_pkt(struct vinfo_s *vinfo);

int register_osd_status_cb(int (*get_osd_enable_status)(u32 index));
void unregister_osd_status_cb(void);

int sbtm_convert_process(enum sbtm_hdr_proc_sel sbtm_proc_sel,
			  struct vinfo_s *vinfo,
			  struct matrix_s *mtx,
			  int mtx_depth);
int sbtm_tmo_hdr2hdr_process(struct vinfo_s *vinfo);

int sbtm_hdr10_tmo_dbg(char **parm);
int sbtm_sbtmdb_reg_dbg(char **parm);

#endif
