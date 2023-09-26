/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * drivers/amlogic/media/di_multi/deinterlace_hw.h
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

#ifndef _DI_HW_H
#define _DI_HW_H
#include <linux/amlogic/media/amvecm/amvecm.h>
#include <linux/amlogic/media/di/di.h>
#include "../deinterlace/di_pqa.h"
//#include "di_pqa.h"

#define DI_NOP_REG1	(0x2fcb)
#define DI_NOP_REG2	(0x2fcd)

/* if post size < 80, filter of ei can't work */
#define MIN_POST_WIDTH	80
#define MIN_BLEND_WIDTH	27

#define	SKIP_CTRE_NUM	16
#define	SC2_OVERLAP_NUM	9

/* from sc2 */
enum DI_MIF0_ID {
	DI_MIF0_ID_INP		= 0,
	DI_MIF0_ID_CHAN2	= 1,
	DI_MIF0_ID_MEM		= 2,
	DI_MIF0_ID_IF1		= 3,
	DI_MIF0_ID_IF0		= 4,
	DI_MIF0_ID_IF2		= 5, /* end */
};

enum DI_MIF0_SEL {
	DI_MIF0_SEL_INP		= 0,
	DI_MIF0_SEL_CHAN2	= 1,
	DI_MIF0_SEL_MEM		= 2,
	DI_MIF0_SEL_IF1		= 0x10,
	DI_MIF0_SEL_IF0		= 0x20,
	DI_MIF0_SEL_IF2		= 0x40, /* end */

	DI_MIF0_SEL_PRE_ALL	= 0x0f,
	DI_MIF0_SEL_PST_ALL	= 0xf0,
	DI_MIF0_SEL_ALL		= 0xff,
	DI_MIF0_SEL_VD1_IF0	= 0x100, /* before g12a */
	DI_MIF0_SEL_VD2_IF0	= 0x200, /* before g12a */
};

enum DI_MIFS_ID {
	 DI_MIFS_ID_MTNWR,
	 DI_MIFS_ID_MCVECWR,
	 DI_MIFS_ID_MCINFRD,
	 DI_MIFS_ID_MCINFWR,
	 DI_MIFS_ID_CONTP1,
	 DI_MIFS_ID_CONTP2,
	 DI_MIFS_ID_CONTWR,
	 DI_MIFS_ID_MCVECRD,
	 DI_MIFS_ID_MTNRD,
};

enum DI_SRC_ID { /* for copy or rotation */
	DI_SRC_ID_MIF_INP		= 0,
	DI_SRC_ID_MIF_CHAN2		= 1,
	DI_SRC_ID_MIF_MEM		= 2,
	DI_SRC_ID_MIF_IF1		= 3,
	DI_SRC_ID_MIF_IF0		= 4,
	DI_SRC_ID_MIF_IF2		= 5,

	DI_SRC_ID_AFBCD_INP		= 8,
	DI_SRC_ID_AFBCD_CHAN2		= 9,
	DI_SRC_ID_AFBCD_MEM		= 0xa,
	DI_SRC_ID_AFBCD_IF1		= 0xb,
	DI_SRC_ID_AFBCD_IF0		= 0xc,
	DI_SRC_ID_AFBCD_IF2		= 0xd,
};

#define MIF_NUB		6
#define MIF_REG_NUB	15
/* keep order with mif_contr_reg*/
/* keep order with mif_contr_reg_v3 */
/* keep order with mif_reg_name */
/* refer to dim_get_mif_reg_name */
enum EDI_MIF_REG_INDEX {
	MIF_GEN_REG,
	MIF_GEN_REG2,
	MIF_GEN_REG3,
	MIF_CANVAS0,
	MIF_LUMA_X0,
	MIF_LUMA_Y0,
	MIF_CHROMA_X0,
	MIF_CHROMA_Y0,
	MIF_RPT_LOOP,
	MIF_LUMA0_RPT_PAT,
	MIF_CHROMA0_RPT_PAT,
	MIF_DUMMY_PIXEL,
	MIF_FMT_CTRL,
	MIF_FMT_W,
	MIF_LUMA_FIFO_SIZE,
};

enum EDPST_MODE {
	EDPST_MODE_NV21_8BIT, /*NV21 NV12*/
	EDPST_MODE_422_10BIT_PACK,
	EDPST_MODE_422_10BIT,
	EDPST_MODE_422_8BIT,
	EDPST_MODE_420_10BIT /* add 2020-11-26 */
};

struct SC2_OVERLAP_REG_s {
	unsigned int addr;
	unsigned int value;
};

struct AFBCD_S {
	u32  index      ;//3bit: 0-5 for di_m0/m5, 6:vd1 7:vd2
	u32  hsize      ;//input size
	u32  vsize;
	u32  head_baddr;
	u32  body_baddr;
	u32  compbits   ;//2 bits   0-8bits 1-9bits 2-10bits
	u32  fmt_mode   ;//2 bits   default = 2, 0:yuv444 1:yuv422 2:yuv420
	u32  ddr_sz_mode;//1 bits   1:mmu mode
	u32  fmt444_comb;//1 bits
	u32  dos_uncomp ;//1 bits   0:afbce   1:dos
	u32  rot_en;
	u32  rot_hbgn;
	u32  rot_vbgn;
	u32  h_skip_en;
	u32  v_skip_en;

	u32  rev_mode;
	u32  lossy_en;
	u32  def_color_y;
	u32  def_color_u;
	u32  def_color_v;
	u32  win_bgn_h;
	u32  win_end_h;
	u32  win_bgn_v;
	u32  win_end_v;
	u32  rot_vshrk;
	u32  rot_hshrk;
	u32  rot_drop_mode;
	u32  rot_ofmt_mode;
	u32  rot_ocompbit;
	u32  pip_src_mode;
	//ary add:
	u32 hold_line_num;		/* def 2*/
	u32 blk_mem_mode;		/* def 0*/
	unsigned int out_horz_bgn;	/* def 0*/
	unsigned int out_vert_bgn;	/* def 0*/
	unsigned int hz_ini_phase;	/* def 0*/
	unsigned int vt_ini_phase;	/* def 0*/
	unsigned int hz_rpt_fst0_en;	/* def 0*/
	unsigned int vt_rpt_fst0_en;	/* def 0*/
	//unsigned int rev_mode;		/* def 0*/
	unsigned int def_color;		/* def no use */
	unsigned int reg_lossy_en;	/* def 0*/
	//unsigned int pip_src_mode;	/* def 0*/
	//unsigned int rot_drop_mode;	/* def 0*/
	//unsigned int rot_vshrk;		/* def 0*/
	//unsigned int rot_hshrk;		/* def 0*/

};

struct AFBCE_S {
	u32 head_baddr     ;//head_addr of afbce
	u32 mmu_info_baddr ;//mmu_linear_addr
	u32 reg_init_ctrl  ;//pip init frame flag
	u32 reg_pip_mode   ;//pip open bit
	u32 reg_ram_comb   ;//ram split bit open in di mult write case
	u32 reg_format_mode;//0:444 1:422 2:420
	u32 reg_compbits_y ;//bits num after compression
	u32 reg_compbits_c ;//bits num after compression
	u32 hsize_in       ;//input data hsize
	u32 vsize_in       ;//input data hsize
	u32 hsize_bgnd     ;//hsize of background
	u32 vsize_bgnd     ;//hsize of background
	u32 enc_win_bgn_h  ;//scope in background buffer
	u32 enc_win_end_h  ;//scope in background buffer
	u32 enc_win_bgn_v  ;//scope in background buffer
	u32 enc_win_end_v  ;//scope in background buffer
	u32 loosy_mode;
	//0:close 1:luma loosy 2:chrma loosy 3: luma & chrma loosy
	u32 rev_mode       ;//0:normal mode
	u32 def_color_0    ;//def_color
	u32 def_color_1    ;//def_color
	u32 def_color_2    ;//def_color
	u32 def_color_3    ;//def_color
	u32 force_444_comb ;//def_color
	u32 rot_en;
	u32 din_swt;
};

struct dim_fmt_s {
	unsigned int vtype; /* ref to vfm type*/
	unsigned int w;
	unsigned int h;
	unsigned int bitdepth; /*copy*/

	unsigned int bit_mode	: 4;	/*count*/
	unsigned int endian	: 1;
	unsigned int block_mode	: 2;
	/* flg for mif */
	unsigned int p_as_i	: 1; /* working mode */
	unsigned int p_top	: 1;

	unsigned int rev1	: 23;

	enum EDPST_MODE mode;	/* count */

	unsigned int src_type;	/*copy*/
	unsigned int trans_fmt; /*copy*/
	unsigned int sig_fmt;	/*copy*//*use for mtn*/
	unsigned int omx_index; /*copy omx_index or other */
	u64	time_get;	/* debug only */
};

struct dim_cvsi_s {
	unsigned int size;
	unsigned int plane_nub;
	unsigned char cvs_id[3];
	struct canvas_config_s	cvs_cfg[3];
};

const char *dim_get_mif_reg_name(enum EDI_MIF_REG_INDEX idx);

struct DI_MIF_S {
	unsigned short	luma_x_start0;
	unsigned short	luma_x_end0;
	unsigned short	luma_y_start0;
	unsigned short	luma_y_end0;
	unsigned short	chroma_x_start0;
	unsigned short	chroma_x_end0;
	unsigned short	chroma_y_start0;
	unsigned short	chroma_y_end0;
	/* set_separate_en
	 *	00 : one canvas
	 *	01 : 3 canvas(old 4:2:0).
	 *	10: 2 canvas. (NV21).
	 */
	unsigned int		set_separate_en	:2;
	/* src_field_mode
	 *	1 frame . 0 field.
	 */
	unsigned int		src_field_mode	:1;
	unsigned int		src_prog	:1;
	/*video_mode :
	 *	00: 4:2:0;
	 *	01: 4:2:2;
	 *	10: 4:4:4
	 * 2020-06-02 from 1bit to 2bit
	 */
	unsigned int		video_mode	:2;

	/*bit_mode
	 *	0:8 bits
	 *	1:10 bits 422(old mode,12bit)
	 *	2:10bit 444
	 *	3:10bit 422(full pack) or 444
	 */
	unsigned int		bit_mode	:2;

	unsigned int		canvas0_addr0:8;
	unsigned int		canvas0_addr1:8;
	unsigned int		canvas0_addr2:8;
	ulong		addr0;	//for t7
	ulong		addr1;	//for t7
	ulong		addr2;	//for t7

	/* canvas_w: for input not 64 align*/
	unsigned int	canvas_w;
	/* ary move from parameter to here from sc2 */

	unsigned int hold_line	:6; /*ary: use 6bit*/
	unsigned int		reserved	: 1; //for input
	unsigned int vskip_cnt		:4; /*ary 0~8*/
	unsigned int urgent		:1;
	unsigned int wr_en		:1;
	unsigned int rev_x		:1;
	unsigned int rev_y		:1;
	//enable after sc2
	unsigned int burst_size_y	:2; //set 3 as default
	unsigned int burst_size_cb	:2;//set 1 as default
	unsigned int burst_size_cr	:2;//set 1 as default

	unsigned int linear_fromcvs	: 1; //03-31 for test pre-post link
	/* ary no use*/
//	unsigned int nocompress		:1;
	unsigned int		output_field_num:1;
	unsigned int		l_endian : 1; //2020-12-21
	unsigned int		reg_swap : 1;
	unsigned int		cbcr_swap: 1;
	unsigned int		linear : 1;
	unsigned int		buf_crop_en : 1;
	unsigned int		block_mode : 2;
	unsigned int		dbg_from_dec: 1; //
	unsigned int		in_dec	: 1; //for input
	unsigned int	tst_not_setcontr	: 1; //debug for pre-vpp link for di
	unsigned int	buf_hsize;
	unsigned int cvs0_w;
	unsigned int cvs1_w;
	unsigned int cvs2_w;

	/**/
	enum DI_MIF0_ID	mif_index; /* */
	char *name;
};

struct DI_SIM_MIF_S {
	unsigned short	start_x;
	unsigned short	end_x;
	unsigned short	start_y;
	unsigned short	end_y;
	unsigned short	canvas_num;
	/*bit_mode
	 *	0:8 bits
	 *	1:10 bits 422(old mode,12bit)
	 *	2:10bit 444
	 *	3:10bit 422(full pack) or 444
	 */
	unsigned int	bit_mode	:4;
	/* set_separate_en
	 *	00 : one canvas
	 *	01 : 3 canvas(old 4:2:0).
	 *	10: 2 canvas. (NV21).
	 */
	unsigned int	set_separate_en	:2; /*ary add below is only for wr buf*/
	unsigned int	tst_not_setcontr	: 1; //debug for pre-vpp link for di
	unsigned int	res3	: 1;
	/*video_mode :
	 *	00: 4:2:0;
	 *	01: 4:2:2;
	 *	10: 4:4:4
	 * 2020-06-02 from 1bit to 2bit
	 */
	unsigned int	video_mode	:4;
	unsigned int	ddr_en		:1;
	unsigned int	urgent		:1;
	unsigned int	l_endian	:1;
	unsigned int	cbcr_swap	:1;

	unsigned int	en		:1; /* add for sc2*/
	unsigned int	src_i		:1; /* ary add for sc2 */
	unsigned int	reg_swap	:1;
	unsigned int	linear		: 1;
	unsigned int	buf_crop_en	: 1;
	unsigned int	is_dw		: 1;
	unsigned int	rev_x		: 1;	//20220126
	unsigned int	rev_y		: 1;	//20220126
	unsigned int	per_bits	: 8; /* for t7 */

	unsigned int	buf_hsize;
	ulong		addr; //for t7
	ulong		addr1;
	ulong		addr2;	// addr2 or hf buffer v_size;
	//from:struct DI_MC_MIF_s
	unsigned short blend_en;		//20220126
	unsigned short vecrd_offset;	//20220126
	enum DI_MIFS_ID	mif_index; /* */
};

struct DI_MC_MIF_s {
	unsigned short start_x;
	unsigned short start_y;
	unsigned short end_y;
	unsigned short size_x;
	unsigned short size_y;
	unsigned short canvas_num;
	unsigned short blend_en;
	unsigned short vecrd_offset;
	unsigned int	per_bits; /* for t7 */
	ulong	addr; //for t7
	bool linear;
};

enum gate_mode_e {
	GATE_AUTO,
	GATE_ON,
	GATE_OFF,
};

struct mcinfo_lmv_s {
	unsigned char	lock_flag;
	char		lmv;
	unsigned short	lock_cnt;
};

struct di_pq_parm_s {
	struct am_pq_parm_s pq_parm;
	struct am_reg_s *regs;
	struct list_head list;
};

/***********************************************
 * setting rebuild
 *	by ary
 ***********************************************/

enum EDI_MIFSR {
	EDI_MIFS_X, //->WRMIF_X
	EDI_MIFS_Y,	//->WRMIF_Y
	EDI_MIFS_CTRL,	//->WRMIF_CTRL
};

/* keep order with reg_wrmif_v3 */
enum DI_WRMIF_INDEX_V3 {
	WRMIF_X,
	WRMIF_Y,
	WRMIF_CTRL,
	WRMIF_URGENT,
	WRMIF_CANVAS,
	WRMIF_DBG_AXI_CMD_CNT,
	WRMIF_DBG_AXI_DAT_CNT,
};

#define DIM_WRMIF_MIF_V3_NUB		(2)

#define DIM_WRMIF_SET_V3_NUB	(7)

/* keep order with reg_bits_wr */
/* mif bits define */
enum ENR_MIF_INDEX {
	ENR_MIF_INDEX_X_ST	= 0,
	ENR_MIF_INDEX_X_END,
	ENR_MIF_INDEX_Y_ST,
	ENR_MIF_INDEX_Y_END,
	ENR_MIF_INDEX_CVS,
	ENR_MIF_INDEX_EN,
	ENR_MIF_INDEX_BIT_MODE,
	ENR_MIF_INDEX_ENDIAN,
	ENR_MIF_INDEX_URGENT,
	ENR_MIF_INDEX_CBCR_SW,
	ENR_MIF_INDEX_VCON,
	ENR_MIF_INDEX_HCON,
	ENR_MIF_INDEX_RGB_MODE,
	/*below is only for sc2*/
	ENR_MIF_INDEX_DBG_CMD_CNT,
	ENR_MIF_INDEX_DBG_DAT_CNT,
};

/*keep same as EDI_MIFSM*/
#define EDI_MIFS_NUB	(2)

/*keep same as EDI_MIFSR*/
#define EDI_MIFS_REG_NUB (3)

struct cfg_mifset_s {
	bool ddr_en;
	bool urgent;
	bool l_endian; /* little_endian */
	bool cbcr_swap;
	//unsigned int bit_mode;
};

union hw_sc2_ctr_pre_s {
	unsigned int d32;
	struct {
	unsigned int afbc_en		: 1;	//nrwr_path_sel
	unsigned int mif_en		: 1; /*nr mif*/
	unsigned int nr_ch0_en		: 1;
	unsigned int is_4k		: 1;
	/*0:internal  1:pre-post link  2:viu  3:vcp(vdin)*/
	unsigned int pre_frm_sel	: 4;

	unsigned int afbc_nr_en		: 1;
	unsigned int afbc_inp		: 1;
	unsigned int afbc_mem		: 1;
	unsigned int afbc_chan2		: 1;

	unsigned int is_inp_4k		: 1;
	unsigned int is_chan2_4k	: 1;
	unsigned int is_mem_4k		: 1;
	unsigned int reserve1		: 1;

	unsigned int mode_4k		: 2;
	unsigned int reserve2		: 14;
	} b;
};

union hw_sc2_ctr_pst_s {
	unsigned int d32;
	struct {
	unsigned int afbc_en		: 1;	//nrwr_path_sel
	unsigned int mif_en		: 1;
	unsigned int is_4k		: 1;
	unsigned int reserve1		: 1;
	/*0:viu  1:internal  2:pre-post link (post timming)*/
	unsigned int post_frm_sel	: 4;

	unsigned int afbc_if0		: 1;
	unsigned int afbc_if1		: 1;
	unsigned int afbc_if2		: 1;
	unsigned int afbc_wr		: 1;

	unsigned int is_if0_4k		: 1;
	unsigned int is_if1_4k		: 1;
	unsigned int is_if2_4k		: 1;
	unsigned int reserve2		: 1;

	unsigned int reserve3		: 16;
	} b;
};

struct mem_cpy_s {
	struct AFBCD_S	*in_afbcd;
	struct vframe_s *afbcd_vf;
	enum EAFBC_DEC afbcd_dec;
	struct AFBCD_CFG_S *afbcd_cfg;

	struct AFBCE_S	*out_afbce;
	struct DI_MIF_S	*in_rdmif;
	//struct DI_MIF_S	*out_wrmif;
	struct DI_SIM_MIF_S *out_wrmif;

	unsigned int hold_line	: 4;
	unsigned int afbc_en	: 1;
	unsigned int en_print	: 1;
	unsigned int rev1	: 2;

	unsigned int port	: 8;
	unsigned int rev2	: 16;

	const struct reg_acc *opin;
};

/**********************************************/
void dim_read_pulldown_info(unsigned int *glb_frm_mot_num,
			    unsigned int *glb_fid_mot_num);
unsigned int dim_rd_mcdi_fldcnt(void);

#ifdef MARK_HIS
void read_new_pulldown_info(struct FlmModReg_t *pFMRegp);
#endif
void dim_pulldown_info_clear_g12a(const struct reg_acc *op);
void dimh_combing_pd22_window_config(unsigned int width, unsigned int height);
void dimh_hw_init(bool pulldown_en, bool mc_enable);
void dimh_hw_uninit(void);
void dimh_set_slv_mcvec(unsigned int en);
unsigned int dimh_get_slv_mcvec(void);
void dimh_enable_di_pre_aml(struct DI_MIF_S	*di_inp_mif,
			    struct DI_MIF_S	*di_mem_mif,
			    struct DI_MIF_S	*di_chan2_mif,
			    struct DI_SIM_MIF_S	*di_nrwr_mif,
			    struct DI_SIM_MIF_S	*di_mtnwr_mif,
			    struct DI_SIM_MIF_S	*di_contp2rd_mif,
			    struct DI_SIM_MIF_S	*di_contprd_mif,
			    struct DI_SIM_MIF_S	*di_contwr_mif,
			    unsigned char madi_en,
			    unsigned char pre_field_num,
			    unsigned char pre_vdin_link,
			    void *ppre, unsigned int channel);
//void dimh_enable_afbc_input(struct vframe_s *vf);

void dimh_mc_pre_mv_irq(void);
void dimh_enable_mc_di_pre(struct DI_MC_MIF_s *di_mcinford_mif,
			   struct DI_MC_MIF_s *di_mcinfowr_mif,
			   struct DI_MC_MIF_s *di_mcvecwr_mif,
			   unsigned char mcdi_en);
void dimh_enable_mc_di_pre_g12(struct DI_MC_MIF_s *di_mcinford_mif,
			       struct DI_MC_MIF_s *di_mcinfowr_mif,
			       struct DI_MC_MIF_s *di_mcvecwr_mif,
			       unsigned char mcdi_en);

void dimh_enable_mc_di_post(struct DI_MC_MIF_s *di_mcvecrd_mif,
			    int urgent, bool reverse, int invert_mv);
void dimh_en_mc_di_post_g12(struct DI_MC_MIF_s *di_mcvecrd_mif,
			    int urgent, bool reverse, int invert_mv);

void dimh_disable_post_deinterlace_2(void);
void dimh_initial_di_post_2(int hsize_post, int vsize_post,
			    int hold_line, bool write_en);
void dimh_enable_di_post_2(struct DI_MIF_S *di_buf0_mif,
			   struct DI_MIF_S *di_buf1_mif,
			   struct DI_MIF_S *di_buf2_mif,
			   struct DI_SIM_MIF_S *di_diwr_mif,
			   struct DI_SIM_MIF_S *di_mtnprd_mif,
			   int ei_en, int blend_en, int blend_mtn_en,
			   int blend_mode, int di_vpp_en,
			   int di_ddr_en, int post_field_num,
			   int hold_line, int urgent,
			   int invert_mv, int vskip_cnt
);

void dimh_post_switch_buffer(struct DI_MIF_S *di_buf0_mif,
			     struct DI_MIF_S *di_buf1_mif,
			     struct DI_MIF_S *di_buf2_mif,
			     struct DI_SIM_MIF_S *di_diwr_mif,
			     struct DI_SIM_MIF_S *di_mtnprd_mif,
			     struct DI_MC_MIF_s	*di_mcvecrd_mif,
			     int ei_en, int blend_en,
			     int blend_mtn_en, int blend_mode,
			     int di_vpp_en, int di_ddr_en,
			     int post_field_num, int hold_line,
			     int urgent,
			     int invert_mv, bool pd_en, bool mc_enable,
			     int vskip_cnt
);

struct pst_cfg_afbc_s {
	struct di_buf_s *buf_mif[3];
	struct di_buf_s *buf_o;
	struct DI_SIM_MIF_S    *di_diwr_mif;
	struct DI_SIM_MIF_S    *di_mtnprd_mif;
	int ei_en;
	int blend_en;
	int blend_mtn_en;
	int blend_mode;
	int di_vpp_en;
	int di_ddr_en;
	int post_field_num;
	int hold_line;
	int urgent;
	int invert_mv;
	int vskip_cnt;
};

void dimh_enable_di_post_afbc(struct pst_cfg_afbc_s *cfg);

void dim_post_read_reverse_irq(bool reverse,
			       unsigned char mc_pre_flag, bool mc_enable);
void dim_top_gate_control(bool top_en, bool mc_en);
void dim_top_gate_control_sc2(bool top_en, bool mc_en);
void dim_pre_gate_control(bool enable, bool mc_enable);
void dim_pre_gate_control_sc2(bool enable, bool mc_enable);
void dim_post_gate_control(bool gate);
void dim_post_gate_control_sc2(bool gate);
void dim_set_power_control(unsigned char enable);
void dim_hw_disable(bool mc_enable);
void dimh_enable_di_pre_mif(bool enable, bool mc_enable);
void dimh_enable_di_post_mif(enum gate_mode_e mode);

void dimh_combing_pd22_window_config(unsigned int width, unsigned int height);
void dimh_calc_lmv_init(void);
void dimh_calc_lmv_base_mcinfo(unsigned int vf_height,
			       unsigned short *mcinfo_adr_v,
			       unsigned int mcinfo_size);
void dimh_init_field_mode(unsigned short height);
void dim_film_mode_win_config(unsigned int width, unsigned int height);
void dimh_pulldown_vof_win_config(struct pulldown_detected_s *wins);
void dimh_load_regs(struct di_pq_parm_s *di_pq_ptr);
void dim_pre_frame_reset_g12(unsigned char madi_en, unsigned char mcdi_en);
void dim_pre_frame_reset(void);
void dimh_interrupt_ctrl(unsigned char ma_en,
			 unsigned char det3d_en, unsigned char nrds_en,
			 unsigned char post_wr, unsigned char mc_en);
void dimh_txl_patch_prog(int prog_flg, unsigned int cnt, bool mc_en);
int dim_print(const char *fmt, ...);

#define DI_MC_SW_OTHER	(0x1 << 0)
#define DI_MC_SW_REG	(0x1 << 1)
/*#define DI_MC_SW_POST	(1 << 2)*/
#define DI_MC_SW_IC	(0x1 << 2)

#define DI_MC_SW_ON_MASK	(DI_MC_SW_REG | DI_MC_SW_OTHER | DI_MC_SW_IC)

void dimh_patch_post_update_mc(void);
void dimh_patch_post_update_mc_sw(unsigned int cmd, bool on);

void dim_rst_protect(bool on);
void dim_pre_nr_wr_done_sel(bool on);
void dim_arb_sw(bool on);
void dbg_set_DI_PRE_CTRL(void);
void di_async_reset2(void);	/*2019-04-05 add for debug*/
void di_pre_data_mif_ctrl(bool enable, const struct reg_acc *op_in,
			  bool en_link);
void di_async_txhd2(void);

enum DI_HW_POST_CTRL {
	DI_HW_POST_CTRL_INIT,
	DI_HW_POST_CTRL_RESET,
};

void dimh_post_ctrl(enum DI_HW_POST_CTRL contr,
		    unsigned int post_write_en);
void dimh_int_ctr(unsigned int set_mod, unsigned char ma_en,
		  unsigned char det3d_en, unsigned char nrds_en,
		  unsigned char post_wr, unsigned char mc_en);

void h_dbg_reg_set(unsigned int val);
enum EDI_POST_FLOW {
	EDI_POST_FLOW_STEP1_STOP,
	EDI_POST_FLOW_STEP2_START,
	EDI_POST_FLOW_STEP3_IRQ,
	EDI_POST_FLOW_STEP4_CP_START, /*add for test copy*/
};

void di_post_set_flow(unsigned int post_wr_en, enum EDI_POST_FLOW step);
void post_mif_sw(bool on);
void post_dbg_contr(void);
void post_close_new(void);
void di_post_reset(void);
void dimh_pst_trig_resize(void);

void hpst_power_ctr(bool on);
void hpst_dbg_power_ctr_trig(unsigned int cmd);
void hpst_dbg_mem_pd_trig(unsigned int cmd);
void hpst_dbg_trig_gate(unsigned int cmd);
void hpst_dbg_trig_mif(unsigned int cmd);
void hpst_mem_pd_sw(unsigned int on);
void hpst_vd1_sw(unsigned int on);
void hpre_gl_sw(bool on);

void dim_init_setting_once(void);
void dim_hw_init_reg(void);
void dim_vpu_vmod_mem_pd_on_off(unsigned int mode, bool on);
/******************************************************************
 * hw ops
 *****************************************************************/
struct dim_hw_opsv_info_s {
	char name[32];
	char update[32];
	unsigned int main_version;
	unsigned int sub_version;
};

struct regs_t {
	unsigned int add;/*add index*/
	unsigned int bit;
	unsigned int wid;
	unsigned int id;
//	unsigned int df_val;
	char *name;
};

struct dsub_vf_s {
	/*from vframe_s */
	unsigned int type;
	unsigned int canvas0Addr;
	unsigned int canvas1Addr;
	unsigned long compHeadAddr;	//afbc_addr
	unsigned long compBodyAddr;
	unsigned int plane_num;
	struct canvas_config_s canvas0_config[3];
	unsigned int width;
	unsigned int height;
	unsigned int bitdepth;
	unsigned int flag;
	unsigned int di_flag;//new
	void *decontour_pre;
	unsigned int source_type;

	u32 video_angle;
	u32 signal_type;
	enum tvin_sig_fmt_e sig_fmt;
	enum vframe_signal_fmt_e fmt;
	u32 sei_magic_code;
	void *mem_handle;	/* di use this for struct dim_mm_blk_s */
};

/* use this to replace vframe_s in di */
struct dvfm_s {
	struct dsub_vf_s vfs;

	/* below is for di */
	unsigned long afbct_adr;//for afbce;
	unsigned int nub_in_frm;// 1~ xx
	unsigned int vfm_type_in;
	unsigned int sts_di; /* dbg only */
	unsigned int cvs_nu[2];//tmp
	unsigned int buf_hsize;
	bool is_dw;
	bool is_dec;/* add for mif_cfg_v2 */
	bool is_prvpp_link;
	bool is_p_pw;
	bool is_itf_vfm;//
	bool is_itf_ins_local;
	bool	flg_afbce_set;//afbce from di_buf_s
	bool	afbce_out_yuv420_10; //afbce from di_buf_s
	bool	is_in_linear;	//tmp for input or out put linear.
	bool	is_linear;	//for ic is linear or not
	bool	en_win;	/**/
	bool	is_4k;
	struct di_win_s win;
	unsigned int src_w;//temp
	unsigned int src_h; //temp
	void *vf_ext;
	unsigned int sum_reg_cnt;
};

struct di_buf_s;
struct di_win_s;
struct SHRK_S;

struct dim_hw_opsv_s {
	struct dim_hw_opsv_info_s	info;
	void (*pre_mif_set)(struct DI_MIF_S *mif,
			    enum DI_MIF0_ID mif_index,
			    const struct reg_acc *op);
	void (*mif_rd_update_addr)(struct DI_MIF_S *mif, enum DI_MIF0_ID mif_index,
		   const struct reg_acc *opin); /* pre-vpp link */
	void (*set_wrmif_pp)(struct DI_MIF_S *mif,
				const struct reg_acc *ops,
				enum EDI_MIFSM mifsel); /* for pre-vpp link */
	void (*wrmif_update_addr)(struct DI_MIF_S *mif,
				const struct reg_acc *ops,
				enum EDI_MIFSM mifsel); /* for pre-vpp link */
	void (*pst_mif_set)(struct DI_MIF_S *mif,
			    enum DI_MIF0_ID mif_index,
			    const struct reg_acc *op);
	void (*pst_mif_update_csv)(struct DI_MIF_S *mif,
				   enum DI_MIF0_ID mif_index,
				   const struct reg_acc *op);
	void (*pre_mif_sw)(bool enable, const struct reg_acc *op, bool en_link);
	void (*pst_mif_sw)(bool on, enum DI_MIF0_SEL sel);
	void (*pst_mif_rst)(enum DI_MIF0_SEL sel);
	void (*pst_mif_rev)(bool rev, enum DI_MIF0_SEL sel);
	void (*pst_dbg_contr)(void);
	void (*pst_set_flow)(unsigned int post_wr_en, enum EDI_POST_FLOW step);
	void (*pst_bit_mode_cfg)(unsigned char if0,
				 unsigned char if1,
				 unsigned char if2,
				 unsigned char post_wr);
	void (*wr_cfg_mif)(struct DI_SIM_MIF_S *wr_mif,
			   enum EDI_MIFSM index,
			   /*struct di_buf_s *di_buf,*/
			   void *di_vf,
			   struct di_win_s *win);
	void (*wr_cfg_mif_dvfm)(struct DI_SIM_MIF_S *wr_mif,
				enum EDI_MIFSM index,
				struct dvfm_s *dvfm,
				struct di_win_s *win);
	void (*wrmif_set)(struct DI_SIM_MIF_S *cfg_mif,
			  const struct reg_acc *ops,
			  enum EDI_MIFSM mifsel);
	void (*wrmif_sw_buf)(struct DI_SIM_MIF_S *cfg_mif,
			     const struct reg_acc *ops,
			     enum EDI_MIFSM mifsel);
	void (*shrk_set)(struct SHRK_S *srkcfg,
			 const struct reg_acc *op);
	void (*shrk_disable)(const struct reg_acc *opin);
	/* for t7 */
	void (*pre_ma_mif_set)(void *ppre,
			       unsigned short urgent);
	void (*post_mtnrd_mif_set)(struct DI_SIM_MIF_S *mtnprd_mif);
	void (*pre_enable_mc)(struct DI_MC_MIF_s *mcinford_mif,
				       struct DI_MC_MIF_s *mcinfowr_mif,
				       struct DI_MC_MIF_s *mcvecwr_mif,
				       unsigned char mcdi_en);
	/* from t3:*/
	bool (*aisr_pre)(struct DI_SIM_MIF_S *mif, bool sel, bool para);
	void (*aisr_disable)(void);
	void (*wrmif_trig)(enum EDI_MIFSM mifsel);
	void (*wr_rst_protect)(bool on);
	void (*hw_init)(void);
	void (*pre_hold_block_txlx)(void);
	void (*pre_cfg_mif)(struct DI_MIF_S *di_mif,
			    enum DI_MIF0_ID mif_index,
			    struct di_buf_s *di_buf,
			    unsigned int ch);
	void (*dbg_reg_pre_mif_print)(void);
	void (*dbg_reg_pst_mif_print)(void);
	void (*dbg_reg_pre_mif_print2)(void);
	void (*dbg_reg_pst_mif_print2)(void);
	void (*dbg_reg_pre_mif_show)(struct seq_file *s);
	void (*dbg_reg_pst_mif_show)(struct seq_file *s);

	/* control */
	void (*pre_gl_sw)(bool on);
	void (*pre_gl_thd)(void);
	void (*pst_gl_thd)(unsigned int hold_line);
	const unsigned int *reg_mif_tab[MIF_NUB];
	/*debug*/
	const unsigned int *reg_mif_wr_tab[EDI_MIFS_NUB];
	const struct regs_t *reg_mif_wr_bits_tab;
	const struct reg_t *rtab_contr_bits_tab;
};

void dbg_mif_reg(struct seq_file *s, enum DI_MIF0_ID eidx);

extern const struct dim_hw_opsv_s dim_ops_l1_v2;
const unsigned int *mif_reg_get_v2(enum DI_MIF0_ID mif_index);//debug
extern const struct reg_acc di_pre_regset;
void config_di_mif_v3(struct DI_MIF_S *di_mif,
		      enum DI_MIF0_ID mif_index,
		      struct di_buf_s *di_buf,
		      unsigned int ch);//debug only

void set_di_mif_v3(struct DI_MIF_S *mif,
		   enum DI_MIF0_ID mif_index,
		   const struct reg_acc *op);//debug only

const char *dim_get_mif_id_name(enum DI_MIF0_ID idx);
/*********************************************************/
struct SHRK_S {
	unsigned int hsize_in;
	unsigned int vsize_in;
	unsigned int h_shrk_mode : 4;
	unsigned int v_shrk_mode : 4;
	unsigned int shrk_en : 1;
	unsigned int frm_rst : 1;
	unsigned int pre_post : 1;
	unsigned int rev : 21;
	unsigned int out_h;
	unsigned int out_v;
};

struct mm_size_in_s {
	unsigned int w;
	unsigned int h;

	/* flg for mif */
	unsigned int p_as_i	: 1; /* working mode */
	unsigned int is_i	: 1;
	unsigned int en_afbce	: 1;
	unsigned int rev2	: 1;

	unsigned int mode	: 4;	/* EDPST_MODE */
	unsigned int out_format : 4; /* 0:default; 1: nv21; 2: nv12;*/

	/* 1: nv21; 2: pother; 3: i */
	unsigned int o_mode	: 4; /*cnt by */
	unsigned int rev1	: 16;

//	};

};

struct mm_size_p_nv21 {
	unsigned int size_buf;
	unsigned int size_total;
	unsigned int size_page;/*2020 for blk*/

	unsigned int cvs_w;
	unsigned int cvs_h;
	unsigned int off_y;
	unsigned int size_buf_uv; /* nv21 / nv12*/
};

struct mm_size_p {
	unsigned int size_buf;
	//unsigned int size_buf_uv; /* nv21 / nv12*/
	unsigned int size_total;
	unsigned int size_page;/*2020 for blk*/

	unsigned int cvs_w;
	unsigned int cvs_h;
};

struct mm_size_out_s {
	struct mm_size_in_s info_in;
	union {
		struct mm_size_p_nv21	nv21;
		struct mm_size_p	p;
	};
};

void ma_di_init(void);
void mc_blend_sc2_init(void);
void dimh_set_crc_init(int count);

void dw_fill_outvf(struct vframe_s *vfm,
		   struct di_buf_s *di_buf);
/* hw_v3:*/
struct DI_PREPST_S {
	struct DI_MIF_S   *inp_mif;	 // PRE  : current input date
	struct DI_MIF_S   *mem_mif;     // PRE  : pre 2 data
	struct DI_MIF_S   *chan2_mif;     // PRE  : pre 1 data
	struct DI_MIF_S   *nrwr_mif;     // PRE  : nr write
	struct DI_SIM_MIF_S   *mtnwr_mif;     // PRE  : motion write
	struct DI_SIM_MIF_S   *contp2rd_mif;     // PRE  : 1bit motion read p2
	struct DI_SIM_MIF_S   *contprd_mif;     // PRE  : 1bit motion read p1
	struct DI_SIM_MIF_S   *contwr_mif;     // PRE  : 1bit motion write
	struct DI_SIM_MIF_S   *mcinford_mif;     // PRE  : mcdi lmv info read
	struct DI_SIM_MIF_S   *mcinfowr_mif;     // PRE  : mcdi lmv info write
	struct DI_SIM_MIF_S   *mcvecwr_mif;     // PRE  : mcdi motion vector write
	struct DI_MIF_S   *if1_mif;     // POST : pre field data
	struct DI_MIF_S   *diwr_mif;     // POST : post write
	struct DI_SIM_MIF_S   *mcvecrd_mif;     // POST : mc vector read
	struct DI_SIM_MIF_S   *mtnrd_mif;     // POST : motion read

	unsigned int link_vpp	: 1;
	unsigned int post_wr_en : 1;
	unsigned int cont_ini_en : 1;
	unsigned int mcinfo_rd_en : 1;
	unsigned int pre_field_num : 1;     //
	unsigned int mc_en	: 1;
	unsigned int rev1	: 2;
	unsigned int hold_line : 8;
	/* ary add:*/
	unsigned char cvs_nub_inp[2];
	unsigned char cvs_nub_mem[2];
	unsigned char cvs_nub_chan2[2];
	unsigned char cvs_nub_nr[2];
	unsigned char cvs_nub_wr[2];
	unsigned char cvs_nub_if1[2];
	unsigned char cvs_nub_mtnw;
	unsigned char cvs_nub_contp2rd;
	unsigned char cvs_nub_contprd;
	unsigned char cvs_nub_contwr;
	unsigned char cvs_nub_mcrd;
	unsigned char cvs_nub_mcwr;
	unsigned char cvs_nub_mvwr;
	unsigned char cvs_nub_mvrd;
	unsigned char cvs_nub_mtnrd;

	struct canvas_config_s *bmtnw;
	struct canvas_config_s *bmtnr;
	struct canvas_config_s *bmcwr;
	struct canvas_config_s *bmcrd;
	struct canvas_config_s *bmvwr;
	struct canvas_config_s *bmvrd;
	struct canvas_config_s *bcontp2rd;
	struct canvas_config_s *bcontprd;
	struct canvas_config_s *bcontwr;
};

unsigned int dw_get_h(void);

void di_mif1_linear_rd_cfg(struct DI_SIM_MIF_S *mif,
			unsigned int CTRL1,
			unsigned int CTRL2,
			unsigned int BADDR);
void di_mcmif_linear_rd_cfg(struct DI_MC_MIF_s *mif,
			unsigned int CTRL1,
			unsigned int CTRL2,
			unsigned int BADDR);
void di_mif1_linear_wr_cfgds(unsigned long addr, unsigned int STRIDE,
			   unsigned int BADDR);
bool dip_is_linear(void);
bool dim_dbg_cfg_post_byapss(void);
void dbg_reg_mem(unsigned int dbgid);
bool dim_aisr_test(struct DI_SIM_MIF_S *mif, bool sel);//test only
extern const struct reg_acc di_pst_regset;

void set_sc2overlap_table(unsigned int addr, unsigned int value,
			  unsigned int start,
			  unsigned int len);
void disable_afbcd_t5dvb(void);

#endif
