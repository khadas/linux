/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * drivers/amlogic/media/di_multi/sc2/di_hw_v3.h
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

#ifndef __DI_HW_V3_H__
#define __DI_HW_V3_H__

#include "deinterlace_hw.h"

enum EAFBCE_INDEX_V3 {
	AFBCEX_ENABLE,
	AFBCEX_MODE,
	AFBCEX_SIZE_IN,
	AFBCEX_BLK_SIZE_IN,
	AFBCEX_HEAD_BADDR,
	AFBCEX_MIF_SIZE,
	AFBCEX_PIXEL_IN_HOR_SCOPE,
	AFBCEX_PIXEL_IN_VER_SCOPE,
	AFBCEX_CONV_CTRL,
	AFBCEX_MIF_HOR_SCOPE,
	AFBCEX_MIF_VER_SCOPE,
	AFBCEX_STAT1,
	AFBCEX_STAT2,
	AFBCEX_FORMAT,
	AFBCEX_MODE_EN,
	AFBCEX_DWSCALAR,
	AFBCEX_DEFCOLOR_1,
	AFBCEX_DEFCOLOR_2,
	AFBCEX_QUANT_ENABLE,
	AFBCEX_IQUANT_LUT_1,
	AFBCEX_IQUANT_LUT_2,
	AFBCEX_IQUANT_LUT_3,
	AFBCEX_IQUANT_LUT_4,
	AFBCEX_RQUANT_LUT_1,
	AFBCEX_RQUANT_LUT_2,
	AFBCEX_RQUANT_LUT_3,
	AFBCEX_RQUANT_LUT_4,
	AFBCEX_YUV_FORMAT_CONV_MODE,
	AFBCEX_DUMMY_DATA,
	AFBCEX_CLR_FLAG,
	AFBCEX_STA_FLAGT,
	AFBCEX_MMU_NUM,
	AFBCEX_MMU_RMIF_CTRL1,
	AFBCEX_MMU_RMIF_CTRL2,
	AFBCEX_MMU_RMIF_CTRL3,
	AFBCEX_MMU_RMIF_CTRL4,
	AFBCEX_MMU_RMIF_SCOPE_X,
	AFBCEX_MMU_RMIF_SCOPE_Y,
	AFBCEX_MMU_RMIF_RO_STAT,
	AFBCEX_PIP_CTRL,
	AFBCEX_ROT_CTRL,

};

#define AFBC_ENC_V3_NUB		(3)
#define DIM_AFBCE_V3_NUB	(41)

#define DIM_ERR		(0xffffffff)

#ifdef MARK_SC2
struct SHRK_S {
	u32 hsize_in;
	u32 vsize_in;
	u32 h_shrk_mode;
	u32 v_shrk_mode;
	u32 shrk_en;
	u32 frm_rst;

};
#endif

#ifdef MARK_SC2	//ary use DI_MIF_S
struct DI_MIF0_S {
	u16  luma_x_start0;
	u16  luma_x_end0;
	u16  luma_y_start0;
	u16  luma_y_end0;
	u16  chroma_x_start0;
	u16  chroma_x_end0;
	u16  chroma_y_start0;
	u16  chroma_y_end0;
	u16  set_separate_en : 2;
	// 00 : one canvas 01 : 3 canvas(old 4:2:0).10: 2 canvas. (NV21).
	u16  src_field_mode  : 1;   // 1 frame . 0 field.
	u16  video_mode      : 2;   //00: 4:2:0; 01: 4:2:2; 10: 4:4:4
	u16  output_field_num: 1;   // 0 top field  1 bottom field.
	u16  bits_mode       : 2;
	// 0:8 bits  1:10 bits 422(old mode,12bit)
	// 2: 10bit 444  3:10bit 422(full pack) or 444

	u16  burst_size_y    : 2;
	u16  burst_size_cb   : 2;
	u16  burst_size_cr   : 2;
	u16  canvas0_addr0 : 8;
	u16  canvas0_addr1 : 8;
	u16  canvas0_addr2 : 8;
	u16  rev_x : 1;
	u16  rev_y : 1;
	//ary add----
	unsigned int urgent;
	unsigned int hold_line;
};
#endif

struct DI_MIF1_S {
	u16  start_x;
	u16  end_x;
	u16  start_y;
	u16  end_y;
	u16  canvas_num;
	u16  rev_x;
	u16  rev_y;
#ifdef MARK_SC2
	//bit_mode 0:8 bits 1:10 bits 422(old mode,12bit)
	//2:10bit 444
	//3:10bit 422(full pack) or 444
	unsigned int    bit_mode	   :4;
	//set_separate_en
	//00 : one canvas
	//01 : 3 canvas(old 4:2:0).
	//10: 2 canvas. (NV21).
	unsigned int    set_separate_en :4; //ary add below is only for wr buf
	//video_mode :
	//00: 4:2:0;
	//01: 4:2:2;
	//10: 4:4:4
	//2020-06-02 from 1bit to 2bit
	unsigned int    video_mode	   :4;
	unsigned int    ddr_en	   :1;
	unsigned int    urgent	   :1;
	unsigned int    l_endian	   :1;
	unsigned int    cbcr_swap	   :1;

	unsigned int    en		   :1; /* add for sc2*/
	unsigned int    src_i	   :1; /* ary add for sc2 */
	unsigned int    reserved	   : 15;
#endif
	enum DI_MIFS_ID mif_index; /* */
};

struct DI_PRE_S {
	struct DI_MIF_S *inp_mif;
	struct DI_MIF_S *mem_mif;
	struct DI_MIF_S *chan2_mif;
	struct DI_MIF_S *nrwr_mif;
	struct DI_MIF1_S *mtnwr_mif;
	struct DI_MIF1_S *mcvecwr_mif;
	struct DI_MIF1_S *mcinfrd_mif;
	struct DI_MIF1_S *mcinfwr_mif;
	struct DI_MIF1_S *contp1_mif;
	struct DI_MIF1_S *contp2_mif;
	struct DI_MIF1_S *contwr_mif;
	struct AFBCD_S *inp_afbc;
	struct AFBCD_S *mem_afbc;
	struct AFBCD_S *chan2_afbc;
	struct AFBCE_S *nrwr_afbc;

	unsigned int	      afbc_en	     : 1;//
	unsigned int	      nr_en	     : 1;
	unsigned int	      mcdi_en	     : 1;
	unsigned int	      mtn_en	     : 1;

	unsigned int	      dnr_en	     : 1;
	unsigned int	      cue_en	     : 1;
	unsigned int	      cont_ini_en    : 1;
	unsigned int	      mcinfo_rd_en   : 1;

	unsigned int	      pd32_check_en  : 1;
	unsigned int	      pd22_check_en  : 1;
	unsigned int	      hist_check_en  : 1;
	unsigned int	      pre_field_num  : 1;

	unsigned int	      pre_viu_link   : 1;// pre link to VPP
	unsigned int	resv1		: 3;
	unsigned int	      hold_line      : 8;
	unsigned int	resv2		: 8;

};

struct DI_PST_S {
	struct DI_MIF_S *buf0_mif;
	struct DI_MIF_S *buf1_mif;
	struct DI_MIF_S *buf2_mif;
	struct DI_MIF_S *wr_mif;
	struct DI_MIF1_S *mtn_mif;
	struct DI_MIF1_S *mcvec_mif;
	struct AFBCD_S *buf0_afbc;
	struct AFBCD_S *buf1_afbc;
	struct AFBCD_S *buf2_afbc;
	struct AFBCE_S *wr_afbc;

	int afbc_en;
	int post_en;
	int blend_en;
	int ei_en;
	int mux_en;
	int mc_en;
	int ddr_en;
	int vpp_en;
	int post_field_num;
	int hold_line;

};

struct DI_MULTI_WR_S {
	u32 pre_path_sel;
	u32 post_path_sel;

	struct DI_MIF_S *pre_mif;
	struct AFBCE_S *pre_afbce;
	struct DI_MIF_S *post_mif;
	struct AFBCE_S *post_afbce;

};

struct DI_PREPST_AFBC_S {
	struct AFBCD_S     *inp_afbc	   ;	 // PRE  : current input date
	struct AFBCD_S     *mem_afbc	    ;	  // PRE  : pre 2 data
	struct AFBCD_S     *chan2_afbc    ;	  // PRE  : pre 1 data
	struct AFBCE_S     *nrwr_afbc     ;	  // PRE  : nr write
	struct DI_MIF1_S   *mtnwr_mif     ;	  // PRE  : motion write
	struct DI_MIF1_S   *contp2rd_mif  ;	  // PRE  : 1bit motion read p2
	struct DI_MIF1_S   *contprd_mif   ;	  // PRE  : 1bit motion read p1
	struct DI_MIF1_S   *contwr_mif    ;	  // PRE  : 1bit motion write
	struct DI_MIF1_S   *mcinford_mif  ;	  // PRE  : mcdi lmv info read
	struct DI_MIF1_S   *mcinfowr_mif  ;	  // PRE  : mcdi lmv info write
	struct DI_MIF1_S   *mcvecwr_mif   ; // PRE  : mcdi motion vector write
	struct AFBCD_S     *if1_afbc	    ;	  // POST : pre field data
	struct AFBCE_S     *diwr_afbc     ;	  // POST : post write
	struct DI_MIF1_S   *mcvecrd_mif   ;	  // POST : mc vector read
	struct DI_MIF1_S   *mtnrd_mif     ;	  // POST : motion read
	int	link_vpp;
	int	post_wr_en;
	int	cont_ini_en;
	int	mcinfo_rd_en;
	int	pre_field_num;     //
	int	hold_line;
};

struct DI_PREPST_S {
	struct DI_MIF_S   *inp_mif	   ;	 // PRE  : current input date
	struct DI_MIF_S   *mem_mif       ;     // PRE  : pre 2 data
	struct DI_MIF_S   *chan2_mif     ;     // PRE  : pre 1 data
	struct DI_MIF_S   *nrwr_mif      ;     // PRE  : nr write
	struct DI_MIF1_S   *mtnwr_mif     ;     // PRE  : motion write
	struct DI_MIF1_S   *contp2rd_mif  ;     // PRE  : 1bit motion read p2
	struct DI_MIF1_S   *contprd_mif   ;     // PRE  : 1bit motion read p1
	struct DI_MIF1_S   *contwr_mif    ;     // PRE  : 1bit motion write
	struct DI_MIF1_S   *mcinford_mif  ;     // PRE  : mcdi lmv info read
	struct DI_MIF1_S   *mcinfowr_mif  ;     // PRE  : mcdi lmv info write
	struct DI_MIF1_S   *mcvecwr_mif   ; // PRE  : mcdi motion vector write
	struct DI_MIF_S   *if1_mif       ;     // POST : pre field data
	struct DI_MIF_S   *diwr_mif      ;     // POST : post write
	struct DI_MIF1_S   *mcvecrd_mif   ;     // POST : mc vector read
	struct DI_MIF1_S   *mtnrd_mif     ;     // POST : motion read
	int link_vpp;
	int post_wr_en;
	int cont_ini_en;
	int mcinfo_rd_en;
	int pre_field_num;     //
	int hold_line;
};

struct di_hw_ops_info_s {
	char name[128];
	char update[80];
	unsigned int version_main;
	unsigned int version_sub;
};

extern const struct dim_hw_opsv_s dim_ops_l1_v3;
extern const struct dim_hw_opsv_s dim_ops_l1_v4;

struct hw_ops_s {
	struct di_hw_ops_info_s info;
	unsigned int (*afbcd_set)(int index,
				  struct AFBCD_S *inp_afbcd,
				  const struct reg_acc *op);
	unsigned int (*afbce_set)(int index,
				  //0:vdin_afbce 1:di_afbce0 2:di_afbce1
				  int enable,//open nbit of afbce
				  struct AFBCE_S  *afbce,
				  const struct reg_acc *op);
	void (*shrk_set)(struct SHRK_S *srkcfg,
			 const struct reg_acc *op);
	void (*shrk_disable)(void);
	void (*wrmif_set)(int index, int enable,
			  struct DI_MIF_S *wr_mif, const struct reg_acc *op);
	void (*mult_wr)(struct DI_MULTI_WR_S *mwcfg, const struct reg_acc *op);
	void (*pre_set)(struct DI_PRE_S *pcfg, const struct reg_acc *op);
	void (*post_set)(struct DI_PST_S *ptcfg, const struct reg_acc *op);
	void (*prepost_link)(struct DI_PREPST_S *ppcfg,
			     const struct reg_acc *op);
	void (*prepost_link_afbc)(struct DI_PREPST_AFBC_S *pafcfg,
				  const struct reg_acc *op);
	void (*memcpy_rot)(struct mem_cpy_s *cfg);
	void (*memcpy)(struct mem_cpy_s *cfg);
};

bool di_attach_ops_v3(const struct hw_ops_s **ops);
extern const struct reg_acc sc2reg;

enum SC2_REG_MSK {
	SC2_REG_MSK_GEN_PRE,
	SC2_REG_MSK_GEN_PST,
	SC2_REG_MSK_nr,
	SC2_DW_EN,
	SC2_DW_SHOW,
	SC2_DW_SHRK_EN,
	SC2_ROT_WR,
	SC2_ROT_PST,
	SC2_MEM_CPY,
	SC2_BYPASS_RESET,
	SC2_DISABLE_CHAN2,
	SC2_POST_TRIG,
	SC2_POST_TRIG_MSK1,
	SC2_POST_TRIG_MSK2,
	SC2_POST_TRIG_MSK3,
	SC2_POST_TRIG_MSK4,
	SC2_POST_TRIG_MSK5,
	SC2_POST_TRIG_MSK6,
	SC2_POST_TRIG_MSK7,
	SC2_LOG_POST_REG_OUT,
};

bool is_mask(unsigned int cmd);
void dim_sc2_contr_pre(union hw_sc2_ctr_pre_s *cfg);
void dim_sc2_contr_pst(union hw_sc2_ctr_pst_s *cfg);
void dim_sc2_4k_set(unsigned int mode_4k);
void dim_sc2_afbce_rst(unsigned int ec_nub);
void afbce_sw(enum EAFBC_ENC enc, bool on);//tmp
unsigned int afbce_read_used(enum EAFBC_ENC enc);//tmp

void hpre_gl_read(void);
void cvsi_cfg(struct dim_cvsi_s	*pcvsi);
void dim_secure_sw_pre(unsigned char ch);
void dim_secure_sw_post(unsigned char ch);

#endif /* __DI_HW_V3_H__ */
