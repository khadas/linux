// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */
#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_fdt.h>
#include <linux/of_reserved_mem.h>
#include <linux/of_irq.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/errno.h>
#include <linux/uaccess.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/ctype.h>
#include <linux/vmalloc.h>
#include <linux/interrupt.h>

#include "frc_reg.h"
#include "frc_common.h"
#include "frc_drv.h"
#include "frc_bbd.h"
#include "frc_bbd_window.h"

void frc_bbd_param_init(struct frc_dev_s *frc_devp)
{
	struct frc_fw_data_s *frc_data;
	struct st_search_final_line_para *search_final_line_para;
	u32 input_hsize = (frc_devp->in_sts.in_hsize - 1);//depend on the input image xsize - 1
	u32 input_vsize = (frc_devp->in_sts.in_vsize - 1);//depend on the input image ysize - 1

	frc_data = (struct frc_fw_data_s *)frc_devp->fw_data;
	search_final_line_para = &frc_data->search_final_line_para;

	// BBD FW
	//for fw
	search_final_line_para->bbd_en = 1;
	//1bit 0:close, using bb_mode_switch, 1: open, detect more than zeros in img, bbd off;
	search_final_line_para->pattern_detect_en	 = 0;	     //  u8,
	search_final_line_para->pattern_dect_ratio	 = 109;      //  u8,	 7bit 85% ratio/128
	// 0:use pre scheme 1:use new scheme 2 use new soft scheme
	search_final_line_para->mode_switch = 1;
	//  force me bbd full resolution,	 dft1
	search_final_line_para->ds_xyxy_force	 = 1;
	//  0: sel letter box1 soft; 1: sel letter box2 strong; dft1
	search_final_line_para->sel2_high_mode	 = 1;
	//0: use sel2_high_mode for each line; 1: according to motion
	//line posi compared with lb line2, if line2 more than motion, use line1.
	search_final_line_para->motion_sel1_high_mode  = 1;

	search_final_line_para->black_th_gen_en	 = 1;	     //black th generate adaptively en
	search_final_line_para->black_th_max		 = 80;	     //black pixel th max val, dft80
	search_final_line_para->black_th_min		 = 12;	     //black pixel th min val, dft12

	search_final_line_para->edge_th_gen_en = 0;//edge diff th generate adaptively en, default 0
	search_final_line_para->edge_th_max		 = 60;	     //default 60
	search_final_line_para->edge_th_min	     = 10;	     //default 10
	//0:x4 1:x8 2:x16 3:x32   dft 2
	search_final_line_para->hist_scale_depth	     = 2;
	search_final_line_para->black_th_gen_mode	 = 1;	     //0: max 1:min 2:avg
	search_final_line_para->motion_posi_strong_en  = 1;	     //0:outter, 1:inner, dft1
	search_final_line_para->edge_choose_mode	 = 0;	     //
	search_final_line_para->edge_choose_delta	 = 4;	     //
	search_final_line_para->edge_choose_row_th	 = 600;      //
	search_final_line_para->edge_choose_col_th	 = 300;	 //	 u16,

	search_final_line_para->pre_motion_posi[1]	 = 0;
	// u16,     pre bot motion posi,	 dft 1079 depend on the input image ysize - 1
	search_final_line_para->pre_motion_posi[3]	 = input_vsize;
	search_final_line_para->pre_motion_posi[0]	 = 0;
	// u16,     pre rit motion posi,	 dft 1919 depend on the input image xsize - 1
	search_final_line_para->pre_motion_posi[2]	 = input_hsize;

	// caption
	search_final_line_para->caption[0] = 0;		     //lft caption: 0:no 1:exist
	search_final_line_para->caption[1] = 0;		     //top caption: 0:no 1:exist
	search_final_line_para->caption[2] = 0;		     //rit caption: 0:no 1:exist
	search_final_line_para->caption[3] = 0;		     //bot caption: 0:no 1:exist

	search_final_line_para->caption_change[0] = 0;	     //lft caption change? 0: no 1:change
	search_final_line_para->caption_change[1] = 0;	     //top caption change? 0: no 1:change
	search_final_line_para->caption_change[2] = 0;	     //rit caption change? 0: no 1:change
	search_final_line_para->caption_change[3] = 0;	     //bot caption change? 0: no 1:change

	// fw pre line data
	search_final_line_para->pre_final_posi[0] = 0;		 //pre_lft_final posi for fw
	search_final_line_para->pre_final_posi[1] = 0;		 //pre_top_final posi for fw
	//pre_rit_final posi for fw		 depend on the input image xsize - 1
	search_final_line_para->pre_final_posi[2] = input_hsize;
	//pre_bot_final posi for fw		 depend on the input image ysize - 1
	search_final_line_para->pre_final_posi[3] = input_vsize;

	// scheme 0
	search_final_line_para->motion_th1   = 20000;	     //  u16,	 motion th1 in scheme0
	search_final_line_para->motion_th2   = 40000;	     //  u16,	 motion th2 in scheme0
	search_final_line_para->h_flat_th    = 960;//flat pixel num th in row    in scheme0
	search_final_line_para->v_flat_th    = 540;//flat pixel num th in col    in scheme0

	search_final_line_para->alpha_line   = 10;//iir alpha		     in scheme0
	// IIR val in scheme 1
	search_final_line_para->filt_delt = 16;		     //filt param

	search_final_line_para->top_iir = 0;		//  u16,     top iir val
	//bot iir val	depend on the input image ysize - 1
	search_final_line_para->bot_iir = input_vsize;
	search_final_line_para->lft_iir = 0;			     //  u16,	  lft iir val
	//rit iir val	depend on the input image xsize - 1
	search_final_line_para->rit_iir = input_hsize;

	// Fw make decision
	search_final_line_para->top_bot_motion_cnt_th = 10000000;  //  u32
	search_final_line_para->lft_rit_motion_cnt_th = 10000000;  //  u32
	// u8,  0:hard 1:soft
	search_final_line_para->top_bot_pre_motion_cur_motion_choose_edge_mode = 1;
	// u8,  0:hard 1:soft
	search_final_line_para->lft_rit_pre_motion_cur_motion_choose_edge_mode = 1;

	search_final_line_para->top_bot_motion_edge_line_num = 512;	 //  u16,    dft:512
	search_final_line_para->lft_rit_motion_edge_line_num = 512;	 //  u16,    dft:512

	search_final_line_para->top_bot_motion_edge_delta = 4;	 //  u8,     dft:4
	search_final_line_para->lft_rit_motion_edge_delta = 4;	 //  u8,     dft:4

	search_final_line_para->top_bot_lbox_edge_delta = 2;		 //  u8,     dft:2
	search_final_line_para->lft_rit_lbox_edge_delta = 2;		 //  u8,     dft:2

	search_final_line_para->top_bot_caption_motion_delta = 16;	 //  u8,     dft:16
	search_final_line_para->lft_rit_caption_motion_delta = 16;	 //  u8,     dft:16

	search_final_line_para->top_bot_border_edge_delta = 2;	 //  u8,     dft:2
	search_final_line_para->lft_rit_border_edge_delta = 2;	 //  u8,     dft:2

	search_final_line_para->top_bot_lbox_delta = 4;		 //  u8,     dft:4
	search_final_line_para->lft_rit_lbox_delta = 4;		 //  u8,     dft:4

	search_final_line_para->top_bot_lbox_motion_delta = 2;	 //  u8,     dft:2
	search_final_line_para->lft_rit_lbox_motion_delta = 2;	 //  u8,     dft:2

	// bbd confidence
	search_final_line_para->top_confidence = 0;	//  u8,     final top line confidence
	search_final_line_para->bot_confidence = 0;	//  u8,     final bot line confidence
	search_final_line_para->lft_confidence = 0;	//  u8,     final lft line confidence
	search_final_line_para->rit_confidence = 0;	//  u8,     final rit line confidence

	search_final_line_para->top_confidence_max = 24;//  u8,     final top line confidence max th
	search_final_line_para->bot_confidence_max = 24;//  u8,     final top line confidence max th
	search_final_line_para->lft_confidence_max = 24;//  u8,     final top line confidence max th
	search_final_line_para->rit_confidence_max = 24;//  u8,     final top line confidence max th

	// bbd iir
	search_final_line_para->final_iir_mode = 1;//  u8,     0: not do iir, 1: num sel iir
	search_final_line_para->final_iir_num_th1 = 4;		 //  u8
	search_final_line_para->final_iir_num_th2 = 2;		 //  u8
	search_final_line_para->final_iir_sel_rst_num = 9;		 //  u8
	search_final_line_para->final_top_bot_pre_cur_posi_delta = 4;  //  u8
	search_final_line_para->final_lft_rit_pre_cur_posi_delta = 4;  //  u8
	search_final_line_para->final_iir_check_valid_mode = 1;	 //  u8
	search_final_line_para->final_iir_alpha = 128;		 //  u8

	// mc_force_blackbar_posi
	search_final_line_para->force_final_posi_en = 0;//  u8,     force blackbar posi en
	search_final_line_para->force_final_posi[0] = 0;//  u16,    force lft posi
	search_final_line_para->force_final_posi[1] = 0;//  u16,    force top posi
	//  u16,    force rit posi			 depend on the input image xsize - 1
	search_final_line_para->force_final_posi[2] = input_hsize;
	//  u16,    force bot posi			 depend on the input image ysize - 1
	search_final_line_para->force_final_posi[3] = input_vsize;
	search_final_line_para->force_final_each_posi_en[0] = 1;//  u8,     force lft en
	search_final_line_para->force_final_each_posi_en[1] = 1;//  u8,     force top en
	search_final_line_para->force_final_each_posi_en[2] = 1;//  u8,     force rit en
	search_final_line_para->force_final_each_posi_en[3] = 1;//  u8,     force bot en

	search_final_line_para->valid_ratio = 1;//  u8,     ratio for bb valid
	//  32x32bits
}

u8 frc_bbd_switch_ctrl(struct frc_dev_s *frc_devp, u32 s_size)
{
	u8 bb_mode_switch;
	struct frc_fw_data_s *frc_data;
	struct st_bbd_rd *g_stbbd_rd;
	struct st_search_final_line_para *search_final_line_para;

	frc_data = (struct frc_fw_data_s *)frc_devp->fw_data;
	g_stbbd_rd = &frc_data->g_stbbd_rd;
	search_final_line_para = &frc_data->search_final_line_para;

	if ((g_stbbd_rd->max1_hist_cnt > s_size *
	     search_final_line_para->pattern_dect_ratio / 128) &&
	    g_stbbd_rd->max1_hist_idx == 0 && search_final_line_para->pattern_detect_en) {
		bb_mode_switch = 0;
	} else {
		bb_mode_switch = 1;
	}

	return bb_mode_switch;
}

static u16 top_final_line1[24] = {  0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0};

static u16 bot_final_line1[24] = {  2159, 2159, 2159, 2159, 2159, 2159, 2159, 2159,
				2159, 2159, 2159, 2159, 2159, 2159, 2159, 2159,
				2159, 2159, 2159, 2159, 2159, 2159, 2159, 2159};

static u16 lft_final_line1[24] = {  0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0};

static u16 rit_final_line1[24] = {  3839, 3839, 3839, 3839, 3839, 3839, 3839, 3839,
				3839, 3839, 3839, 3839, 3839, 3839, 3839, 3839,
				3839, 3839, 3839, 3839, 3839, 3839, 3839, 3839};

void frc_bbd_choose_final_line(struct frc_dev_s *frc_devp)
{
	struct frc_fw_data_s *frc_data;
	struct st_bbd_rd *g_stbbd_rd;
	struct st_search_final_line_para *search_final_line_para;
	struct st_frc_rd *g_stfrc_rd;

	u8 dsx = READ_FRC_REG(FRC_REG_ME_HME_SCALE) >> 6 & 0x3;
	u8 dsy = READ_FRC_REG(FRC_REG_ME_HME_SCALE) >> 4 & 0x3;

	u16 oob_apl_xyxy[4], apl_hist_xyxy[4], blackbar_xyxy[4], flat_xyxy[4];
	u16 oob_h_detail_xyxy[4], oob_v_detail_xyxy[4];
	u16 blackbar_xyxy_me[4], logo_bb_xyxy[4], ds_xyxy[4], motion_xyxy_ds[4];

	u8 k, bb_inner_en, bb_xyxy_inner_ofst[4];
	u16 cnt_min, cnt_max, black_th_pre, black_th, edge_th;
	u32 a_size, b_size, c_size, d_size, e_size;// The size of ABCDE
	u16 clap_min[4], clap_max[4];
	u32 apl_val[5], motion_val[4], flat_val[4];
	u16 motion_posi[4], motion_posi_sub[4], tmp_final_posi[4], tmp_final_posi_new[4];
	u16 pre_final_posi[4], cur_final_posi[4], edge_posi[4], bb_motion_xyxy[4];
	u8 caption[4], caption_change[4];
	u8 sel2_mode[4];
	u8 stable_flag[4] = {0, 0, 0, 0};

	frc_data = (struct frc_fw_data_s *)frc_devp->fw_data;
	g_stbbd_rd = &frc_data->g_stbbd_rd;
	g_stfrc_rd = &frc_data->g_stfrc_rd;
	search_final_line_para = &frc_data->search_final_line_para;

	u16 det_top = READ_FRC_REG(FRC_BBD_DETECT_REGION_TOP2BOT) >> 16 & 0xFFFF;
	u16 det_bot = READ_FRC_REG(FRC_BBD_DETECT_REGION_TOP2BOT)     & 0xFFFF;
	u16 det_lft = READ_FRC_REG(FRC_BBD_DETECT_REGION_LFT2RIT) >> 16 & 0xFFFF;
	u16 det_rit = READ_FRC_REG(FRC_BBD_DETECT_REGION_LFT2RIT)     & 0xFFFF;

	u16 ysize = MAX(det_bot - det_top + 1, 0);
	u16 xsize = MAX(det_rit - det_lft + 1, 0);
	u32 s_size = xsize * ysize;

	for (k = 0; k < 24; k++) {
		top_final_line1[k] = MIN(MAX(top_final_line1[k], det_top), det_bot);
		bot_final_line1[k] = MIN(MAX(bot_final_line1[k], det_top), det_bot);
		lft_final_line1[k] = MIN(MAX(lft_final_line1[k], det_lft), det_rit);
		rit_final_line1[k] = MIN(MAX(rit_final_line1[k], det_lft), det_rit);
	}

	u16 top_bot_motion_edge_line_num = search_final_line_para->top_bot_motion_edge_line_num;
	u16 lft_rit_motion_edge_line_num = search_final_line_para->lft_rit_motion_edge_line_num;

	u16 top_bot_motion_edge_delta = search_final_line_para->top_bot_motion_edge_delta;
	u16 lft_rit_motion_edge_delta = search_final_line_para->lft_rit_motion_edge_delta;

	u16 top_bot_lbox_edge_delta = search_final_line_para->top_bot_lbox_edge_delta;
	u16 lft_rit_lbox_edge_delta = search_final_line_para->lft_rit_lbox_edge_delta;

	u16 top_bot_caption_motion_delta = search_final_line_para->top_bot_caption_motion_delta;
	u16 lft_rit_caption_motion_delta = search_final_line_para->lft_rit_caption_motion_delta;

	u16 top_bot_border_edge_delta = search_final_line_para->top_bot_border_edge_delta;
	u16 lft_rit_border_edge_delta = search_final_line_para->lft_rit_border_edge_delta;

	u16 top_bot_lbox_delta = search_final_line_para->top_bot_lbox_delta;
	u16 lft_rit_lbox_delta = search_final_line_para->lft_rit_lbox_delta;

	u16 top_bot_pre_cur_posi_delta = search_final_line_para->final_top_bot_pre_cur_posi_delta;
	u16 lft_rit_pre_cur_posi_delta = search_final_line_para->final_lft_rit_pre_cur_posi_delta;

	top_bot_motion_edge_line_num = MAX(top_bot_motion_edge_line_num * ysize / 1080, 1);
	lft_rit_motion_edge_line_num = MAX(lft_rit_motion_edge_line_num * xsize / 1920, 1);

	top_bot_motion_edge_delta = MAX(top_bot_motion_edge_delta * ysize / 1080, 1);
	lft_rit_motion_edge_delta = MAX(lft_rit_motion_edge_delta * xsize / 1920, 1);

	top_bot_lbox_edge_delta = MAX(top_bot_lbox_edge_delta * ysize / 1080, 1);
	lft_rit_lbox_edge_delta = MAX(lft_rit_lbox_edge_delta * xsize / 1920, 1);

	top_bot_caption_motion_delta = MAX(top_bot_caption_motion_delta * ysize / 1080, 1);
	lft_rit_caption_motion_delta = MAX(lft_rit_caption_motion_delta * xsize / 1920, 1);

	top_bot_border_edge_delta = MAX(top_bot_border_edge_delta * ysize / 1080, 1);
	lft_rit_border_edge_delta = MAX(lft_rit_border_edge_delta * xsize / 1920, 1);

	top_bot_lbox_delta = MAX(top_bot_lbox_delta * ysize / 1080, 1);
	lft_rit_lbox_delta = MAX(lft_rit_lbox_delta * xsize / 1920, 1);

	top_bot_pre_cur_posi_delta = MAX(top_bot_pre_cur_posi_delta * ysize / 1080, 1);
	lft_rit_pre_cur_posi_delta = MAX(lft_rit_pre_cur_posi_delta * ysize / 1080, 1);

	oob_apl_xyxy[0] = READ_FRC_REG(FRC_BBD_OOB_APL_CAL_LFT_TOP_RANGE) >> 16 & 0xFFFF;
	oob_apl_xyxy[1] = READ_FRC_REG(FRC_BBD_OOB_APL_CAL_LFT_TOP_RANGE)     & 0xFFFF;
	oob_apl_xyxy[2] = READ_FRC_REG(FRC_BBD_OOB_APL_CAL_RIT_BOT_RANGE) >> 16 & 0xFFFF;
	oob_apl_xyxy[3] = READ_FRC_REG(FRC_BBD_OOB_APL_CAL_RIT_BOT_RANGE)     & 0xFFFF;

	apl_hist_xyxy[0] = READ_FRC_REG(FRC_BBD_APL_HIST_WIN_LFT_TOP) >> 16 & 0xFFFF;
	apl_hist_xyxy[1] = READ_FRC_REG(FRC_BBD_APL_HIST_WIN_LFT_TOP)	 & 0xFFFF;
	apl_hist_xyxy[2] = READ_FRC_REG(FRC_BBD_APL_HIST_WIN_RIT_BOT) >> 16 & 0xFFFF;
	apl_hist_xyxy[3] = READ_FRC_REG(FRC_BBD_APL_HIST_WIN_RIT_BOT)	 & 0xFFFF;

	a_size = xsize * (oob_apl_xyxy[1] - det_top);		  //top
	b_size = ysize * (oob_apl_xyxy[0] - det_lft);		  //lft
	d_size = ysize * (det_rit - oob_apl_xyxy[2]);    //rit
	e_size = xsize * (det_bot - oob_apl_xyxy[3]);    //bot
	c_size = (oob_apl_xyxy[3] - oob_apl_xyxy[1] + 1) *
		  (oob_apl_xyxy[2] - oob_apl_xyxy[0] + 1);//(bot - top)x (rit - lft)

	pre_final_posi[0] = search_final_line_para->pre_final_posi[0];    //	lft
	pre_final_posi[1] = search_final_line_para->pre_final_posi[1];    //	top
	pre_final_posi[2] = search_final_line_para->pre_final_posi[2];    //	rit
	pre_final_posi[3] = search_final_line_para->pre_final_posi[3];    //	bot

	tmp_final_posi[0] = det_lft; //  lft
	tmp_final_posi[1] = det_top; //  top
	tmp_final_posi[2] = det_rit; //  rit
	tmp_final_posi[3] = det_bot; //  bot

	cur_final_posi[0] = det_lft; //  lft
	cur_final_posi[1] = det_top; //  top
	cur_final_posi[2] = det_rit; //  rit
	cur_final_posi[3] = det_bot; //  bot

	clap_min[0] = det_lft;   // lft min
	clap_max[0] = det_rit;   // lft max
	clap_min[1] = det_top;   // top min
	clap_max[1] = det_bot;   // top max
	clap_min[2] = det_lft;   // rit min
	clap_max[2] = det_rit;   // rit max
	clap_min[3] = det_top;   // bot min
	clap_max[3] = det_bot;   // bot max

	sel2_mode[0] = 1;
	sel2_mode[1] = 1;
	sel2_mode[2] = 1;
	sel2_mode[3] = 1;

	apl_val[0] = (b_size > 0) ? (g_stbbd_rd->oob_apl_lft_sum / b_size) : 0;// lft
	apl_val[1] = (a_size > 0) ? (g_stbbd_rd->oob_apl_top_sum / a_size) : 0;// top
	apl_val[2] = (d_size > 0) ? (g_stbbd_rd->oob_apl_rit_sum / d_size) : 0;// rit
	apl_val[3] = (e_size > 0) ? (g_stbbd_rd->oob_apl_bot_sum / e_size) : 0;// bot
	apl_val[4] = (c_size > 0) ? (g_stbbd_rd->apl_glb_sum     / c_size) : 0;// center

	motion_val[0] = g_stbbd_rd->oob_motion_diff_lft_val;  //  lft
	motion_val[1] = g_stbbd_rd->oob_motion_diff_top_val;  //  top
	motion_val[2] = g_stbbd_rd->oob_motion_diff_rit_val;  //  rit
	motion_val[3] = g_stbbd_rd->oob_motion_diff_bot_val;  //  bot

	//xil_printf("mov:%d %d %d %d\n\r",motion_val[0],motion_val[1],motion_val[2],motion_val[3]);

	flat_val[0] = g_stbbd_rd->flat_lft_cnt;   //  lft
	flat_val[1] = g_stbbd_rd->flat_top_cnt;   //  top
	flat_val[2] = g_stbbd_rd->flat_rit_cnt;   //  rit
	flat_val[3] = g_stbbd_rd->flat_bot_cnt;   //  bot

	bb_inner_en = (READ_FRC_REG(FRC_MC_SETTING1) >> 24) & 0x1;

	bb_xyxy_inner_ofst[0] = (READ_FRC_REG(FRC_MC_BB_HANDLE_INNER_OFST) >> 24) & 0xFF;
	bb_xyxy_inner_ofst[1] = (READ_FRC_REG(FRC_MC_BB_HANDLE_INNER_OFST) >> 16) & 0xFF;
	bb_xyxy_inner_ofst[2] = (READ_FRC_REG(FRC_MC_BB_HANDLE_INNER_OFST) >> 8)  & 0xFF;
	bb_xyxy_inner_ofst[3] = (READ_FRC_REG(FRC_MC_BB_HANDLE_INNER_OFST))     & 0xFF;

	// bbd motion
	if (search_final_line_para->motion_posi_strong_en) {
		motion_posi[1] = ((g_stbbd_rd->top_motion_posi2) << dsy);
		motion_posi[3] = ((g_stbbd_rd->bot_motion_posi2) << dsy) + 1;
		motion_posi[0] = ((g_stbbd_rd->lft_motion_posi2) << dsx);
		motion_posi[2] = ((g_stbbd_rd->rit_motion_posi2) << dsx) + 1;

		motion_posi_sub[1] = ((g_stbbd_rd->top_motion_posi1) << dsy);
		motion_posi_sub[3] = ((g_stbbd_rd->bot_motion_posi1) << dsy) + 1;
		motion_posi_sub[0] = ((g_stbbd_rd->lft_motion_posi1) << dsx);
		motion_posi_sub[2] = ((g_stbbd_rd->rit_motion_posi1) << dsx) + 1;
	} else {
		motion_posi[1] = ((g_stbbd_rd->top_motion_posi2) << dsy) + 1;
		motion_posi[3] = ((g_stbbd_rd->bot_motion_posi2) << dsy);
		motion_posi[0] = ((g_stbbd_rd->lft_motion_posi2) << dsx) + 1;
		motion_posi[2] = ((g_stbbd_rd->rit_motion_posi2) << dsx);

		motion_posi_sub[1] = ((g_stbbd_rd->top_motion_posi1) << dsy) + 1;
		motion_posi_sub[3] = ((g_stbbd_rd->bot_motion_posi1) << dsy);
		motion_posi_sub[0] = ((g_stbbd_rd->lft_motion_posi1) << dsx) + 1;
		motion_posi_sub[2] = ((g_stbbd_rd->rit_motion_posi1) << dsx);
	}

	motion_posi[1] = MIN(MAX(motion_posi[1], det_top), det_bot);    //  top
	motion_posi[3] = MIN(MAX(motion_posi[3], det_top), det_bot);    //  bot
	motion_posi[0] = MIN(MAX(motion_posi[0], det_lft), det_rit);    //  lft
	motion_posi[2] = MIN(MAX(motion_posi[2], det_lft), det_rit);    //  rit

	motion_posi_sub[1] = MIN(MAX(motion_posi_sub[1], det_top), det_bot);    //  top
	motion_posi_sub[3] = MIN(MAX(motion_posi_sub[3], det_top), det_bot);    //  bot
	motion_posi_sub[0] = MIN(MAX(motion_posi_sub[0], det_lft), det_rit);    //  lft
	motion_posi_sub[2] = MIN(MAX(motion_posi_sub[2], det_lft), det_rit);    //  rit

	// for debug path
	UPDATE_FRC_REG_BITS(FRC_REG_DEBUG_PATH_TOP_BOT_MOTION_POSI2, motion_posi[1] << 16,
			    0xFFFF0000);
	UPDATE_FRC_REG_BITS(FRC_REG_DEBUG_PATH_TOP_BOT_MOTION_POSI2, motion_posi[3], 0x0000FFFF);
	UPDATE_FRC_REG_BITS(FRC_REG_DEBUG_PATH_LFT_RIT_MOTION_POSI2, motion_posi[0] << 16,
			    0xFFFF0000);
	UPDATE_FRC_REG_BITS(FRC_REG_DEBUG_PATH_LFT_RIT_MOTION_POSI2, motion_posi[2], 0x0000FFFF);

	UPDATE_FRC_REG_BITS(FRC_REG_DEBUG_PATH_TOP_BOT_MOTION_POSI1, motion_posi_sub[1] << 16,
			    0xFFFF0000);
	UPDATE_FRC_REG_BITS(FRC_REG_DEBUG_PATH_TOP_BOT_MOTION_POSI1, motion_posi_sub[3],
			    0x0000FFFF);
	UPDATE_FRC_REG_BITS(FRC_REG_DEBUG_PATH_LFT_RIT_MOTION_POSI1, motion_posi_sub[0] << 16,
			    0xFFFF0000);
	UPDATE_FRC_REG_BITS(FRC_REG_DEBUG_PATH_LFT_RIT_MOTION_POSI1, motion_posi_sub[2],
			    0x0000FFFF);

	edge_posi[0] = frc_bbd_choose_edge(frc_devp, g_stbbd_rd->lft_edge_posi2,
					   g_stbbd_rd->lft_edge_posi_valid2,
					   g_stbbd_rd->lft_edge_val2,
					   g_stbbd_rd->lft_edge_first_posi,
					   g_stbbd_rd->lft_edge_first_posi_valid,
					   g_stbbd_rd->lft_edge_first_val,
					   search_final_line_para->edge_choose_col_th);//  lft
	edge_posi[1] = frc_bbd_choose_edge(frc_devp, g_stbbd_rd->top_edge_posi2,
					   g_stbbd_rd->top_edge_posi_valid2,
					   g_stbbd_rd->top_edge_val2,
					   g_stbbd_rd->top_edge_first_posi,
					   g_stbbd_rd->top_edge_first_posi_valid,
					   g_stbbd_rd->top_edge_first_val,
					   search_final_line_para->edge_choose_row_th);//  top
	edge_posi[2] = frc_bbd_choose_edge(frc_devp, g_stbbd_rd->rit_edge_posi2,
					   g_stbbd_rd->rit_edge_posi_valid2,
					   g_stbbd_rd->rit_edge_val2,
					   g_stbbd_rd->rit_edge_first_posi,
					   g_stbbd_rd->rit_edge_first_posi_valid,
					   g_stbbd_rd->rit_edge_first_val,
					   search_final_line_para->edge_choose_col_th);//  rit
	edge_posi[3] = frc_bbd_choose_edge(frc_devp, g_stbbd_rd->bot_edge_posi2,
					   g_stbbd_rd->bot_edge_posi_valid2,
					   g_stbbd_rd->bot_edge_val2,
					   g_stbbd_rd->bot_edge_first_posi,
					   g_stbbd_rd->bot_edge_first_posi_valid,
					   g_stbbd_rd->bot_edge_first_val,
					   search_final_line_para->edge_choose_row_th);//  bot

	// for debug path
	UPDATE_FRC_REG_BITS(FRC_REG_DEBUG_PATH_TOP_BOT_EDGE_POSI2, g_stbbd_rd->top_edge_posi2 << 16,
				0xFFFF0000);
	UPDATE_FRC_REG_BITS(FRC_REG_DEBUG_PATH_TOP_BOT_EDGE_POSI2,
	g_stbbd_rd->bot_edge_posi2, 0x0000FFFF);
	UPDATE_FRC_REG_BITS(FRC_REG_DEBUG_PATH_LFT_RIT_EDGE_POSI2, g_stbbd_rd->lft_edge_posi2 << 16,
				0xFFFF0000);
	UPDATE_FRC_REG_BITS(FRC_REG_DEBUG_PATH_LFT_RIT_EDGE_POSI2,
	g_stbbd_rd->rit_edge_posi2, 0x0000FFFF);
	UPDATE_FRC_REG_BITS(FRC_REG_DEBUG_PATH_TOP_BOT_EDGE_POSI1, g_stbbd_rd->top_edge_posi1 << 16,
				0xFFFF0000);
	UPDATE_FRC_REG_BITS(FRC_REG_DEBUG_PATH_TOP_BOT_EDGE_POSI1, g_stbbd_rd->bot_edge_posi1,
				0x0000FFFF);
	UPDATE_FRC_REG_BITS(FRC_REG_DEBUG_PATH_LFT_RIT_EDGE_POSI1, g_stbbd_rd->lft_edge_posi1 << 16,
				0xFFFF0000);
	UPDATE_FRC_REG_BITS(FRC_REG_DEBUG_PATH_LFT_RIT_EDGE_POSI1, g_stbbd_rd->rit_edge_posi1,
				0x0000FFFF);

	caption[0] = 0; //  lft caption
	caption[1] = 0; //  top caption
	caption[2] = 0; //  rit caption
	caption[3] = 0; //  bot caption

	caption_change[0] = 0;  //  lft caption change
	caption_change[1] = 0;  //  top caption change
	caption_change[2] = 0;  //  rit caption change
	caption_change[3] = 0;  //  bot caption change

	UPDATE_FRC_REG_BITS(FRC_BBD_ADV_ROUGH_LFT_POSI, g_stbbd_rd->rough_lft_posi1 << 16,
			    0xFFFF0000);
	UPDATE_FRC_REG_BITS(FRC_BBD_ADV_ROUGH_LFT_POSI, g_stbbd_rd->rough_lft_posi2, 0x0000FFFF);

	UPDATE_FRC_REG_BITS(FRC_BBD_ADV_ROUGH_RIT_POSI, g_stbbd_rd->rough_rit_posi1 << 16,
			    0xFFFF0000);
	UPDATE_FRC_REG_BITS(FRC_BBD_ADV_ROUGH_RIT_POSI, g_stbbd_rd->rough_rit_posi2,
			    0x0000FFFF);

	UPDATE_FRC_REG_BITS(FRC_BBD_ADV_ROUGH_LFT_MOTION_POSI, g_stbbd_rd->lft_motion_posi1 << 16,
			    0xFFFF0000);
	UPDATE_FRC_REG_BITS(FRC_BBD_ADV_ROUGH_LFT_MOTION_POSI, g_stbbd_rd->lft_motion_posi2,
			    0x0000FFFF);

	UPDATE_FRC_REG_BITS(FRC_BBD_ADV_ROUGH_RIT_MOTION_POSI,
			    g_stbbd_rd->rough_rit_motion_posi1 << 16, 0xFFFF0000);
	UPDATE_FRC_REG_BITS(FRC_BBD_ADV_ROUGH_RIT_MOTION_POSI, g_stbbd_rd->rough_rit_motion_posi2,
			    0x0000FFFF);

	UPDATE_FRC_REG_BITS(FRC_BBD_FINER_LFT_MOTION_TH, g_stbbd_rd->finer_lft_motion_th1 << 16,
			    0x0FFF0000);
	UPDATE_FRC_REG_BITS(FRC_BBD_FINER_LFT_MOTION_TH, g_stbbd_rd->finer_lft_motion_th2,
			    0x00000FFF);
	UPDATE_FRC_REG_BITS(FRC_BBD_FINER_RIT_MOTION_TH, g_stbbd_rd->finer_rit_motion_th1 << 16,
			    0x0FFF0000);
	UPDATE_FRC_REG_BITS(FRC_BBD_FINER_RIT_MOTION_TH, g_stbbd_rd->finer_rit_motion_th2,
			    0x00000FFF);

	search_final_line_para->mode_switch = frc_bbd_switch_ctrl(frc_devp, s_size);

	//  step 1
	//  lft
	tmp_final_posi[0] = frc_bbd_fw_valid12_sel(g_stbbd_rd->finer_lft_valid2,
						   g_stbbd_rd->finer_lft_valid1,
						   g_stbbd_rd->finer_lft_posi2,
						   g_stbbd_rd->finer_lft_posi1, motion_posi[0],
						   det_lft, det_lft, det_rit,
						   search_final_line_para->valid_ratio, xsize,
						   search_final_line_para->sel2_high_mode,
						   search_final_line_para->motion_sel1_high_mode,
						   &sel2_mode[0], 0);
	//  top
	tmp_final_posi[1] = frc_bbd_fw_valid12_sel(g_stbbd_rd->top_valid2, g_stbbd_rd->top_valid1,
						   g_stbbd_rd->top_posi2, g_stbbd_rd->top_posi1,
						   motion_posi[1], det_top, det_top, det_bot,
						   search_final_line_para->valid_ratio, ysize,
						   search_final_line_para->sel2_high_mode,
						   search_final_line_para->motion_sel1_high_mode,
						   &sel2_mode[1], 1);
	//  rit
	tmp_final_posi[2] = frc_bbd_fw_valid12_sel(g_stbbd_rd->finer_rit_valid2,
						   g_stbbd_rd->finer_rit_valid1,
						   g_stbbd_rd->finer_rit_posi2,
						   g_stbbd_rd->finer_rit_posi1, motion_posi[2],
						   det_rit, det_lft, det_rit,
						   search_final_line_para->valid_ratio,
						   xsize, search_final_line_para->sel2_high_mode,
						   search_final_line_para->motion_sel1_high_mode,
						   &sel2_mode[2], 2);
	//  bot
	tmp_final_posi[3] = frc_bbd_fw_valid12_sel(g_stbbd_rd->bot_valid2, g_stbbd_rd->bot_valid1,
						   g_stbbd_rd->bot_posi2, g_stbbd_rd->bot_posi1,
						   motion_posi[3], det_bot, det_top, det_bot,
						   search_final_line_para->valid_ratio, ysize,
						   search_final_line_para->sel2_high_mode,
						   search_final_line_para->motion_sel1_high_mode,
						   &sel2_mode[3], 3);

	pr_frc(dbg_bbd + 2, "\nLB1-%d %d-%d %d-%d %d-%d %d", g_stbbd_rd->finer_lft_posi1,
		g_stbbd_rd->finer_lft_valid1,
		g_stbbd_rd->top_posi1, g_stbbd_rd->top_valid1, g_stbbd_rd->finer_rit_posi1,
		g_stbbd_rd->finer_rit_valid1,
		g_stbbd_rd->bot_posi1, g_stbbd_rd->bot_valid1);
	pr_frc(dbg_bbd + 2, "\nLB2-%d %d-%d %d-%d %d-%d %d", g_stbbd_rd->finer_lft_posi2,
		g_stbbd_rd->finer_lft_valid2,
		g_stbbd_rd->top_posi2, g_stbbd_rd->top_valid2, g_stbbd_rd->finer_rit_posi2,
		g_stbbd_rd->finer_rit_valid2,
		g_stbbd_rd->bot_posi2, g_stbbd_rd->bot_valid2);
	pr_frc(dbg_bbd + 0, "\nmode=%d", search_final_line_para->mode_switch);
	pr_frc(dbg_bbd + 0, "\nLB -%d %d %d %d", tmp_final_posi[0], tmp_final_posi[1],
		tmp_final_posi[2], tmp_final_posi[3]);
	pr_frc(dbg_bbd + 0, "\nEL -%d %d %d %d", edge_posi[0], edge_posi[1],
		edge_posi[2], edge_posi[3]);
	pr_frc(dbg_bbd + 0, "\nML -%d %d %d %d", motion_posi[0], motion_posi[1],
		motion_posi[2], motion_posi[3]);

	// new scheme
	if (search_final_line_para->mode_switch == 0) {
		tmp_final_posi_new[0] = det_lft;    //  lft
		tmp_final_posi_new[1] = det_top;    //  top
		tmp_final_posi_new[2] = det_rit;    //  rit
		tmp_final_posi_new[3] = det_bot;    //  bot
		//set window
		for (k = 0; k < 4; k++) {
			search_final_line_para->caption[k]		= 0;
			search_final_line_para->caption_change[k]	= 0;
			search_final_line_para->pre_final_posi[k]	= tmp_final_posi_new[k];
			search_final_line_para->pre_motion_posi[k]	= tmp_final_posi_new[k];

			logo_bb_xyxy[k]	 = tmp_final_posi_new[k];
			blackbar_xyxy[k] = tmp_final_posi_new[k];

			flat_xyxy[k]	 = tmp_final_posi_new[k];
			bb_motion_xyxy[k] = tmp_final_posi_new[k];
			oob_apl_xyxy[k]	 = tmp_final_posi_new[k];
			apl_hist_xyxy[k] = tmp_final_posi_new[k];
			oob_h_detail_xyxy[k] = tmp_final_posi_new[k];

			// lft and top
			if (k == 0 || k == 1) {
				blackbar_xyxy_me[k] =	(bb_inner_en == 1) ?
				MIN(MAX(tmp_final_posi_new[k] + bb_xyxy_inner_ofst[k], clap_min[k]),
				clap_max[k]) : tmp_final_posi_new[k];
			} else if (k == 2 || k == 3) {
				// rit and bot
				blackbar_xyxy_me[k] =	(bb_inner_en == 1) ?
				MIN(MAX(tmp_final_posi_new[k] - bb_xyxy_inner_ofst[k], clap_min[k]),
				clap_max[k]) : tmp_final_posi_new[k];
			}
		} // k

		//  LOGO
		UPDATE_FRC_REG_BITS(FRC_LOGO_BB_LFT_TOP, logo_bb_xyxy[0] << 16, 0xFFFF0000);
		UPDATE_FRC_REG_BITS(FRC_LOGO_BB_LFT_TOP, logo_bb_xyxy[1],     0x0000FFFF);
		UPDATE_FRC_REG_BITS(FRC_LOGO_BB_RIT_BOT, logo_bb_xyxy[2] << 16, 0xFFFF0000);
		UPDATE_FRC_REG_BITS(FRC_LOGO_BB_RIT_BOT, logo_bb_xyxy[3],     0x0000FFFF);

		//  FRC bbd
		UPDATE_FRC_REG_BITS(FRC_REG_BLACKBAR_XYXY_ST, blackbar_xyxy[0] << 16, 0xFFFF0000);
		UPDATE_FRC_REG_BITS(FRC_REG_BLACKBAR_XYXY_ST, blackbar_xyxy[1],	  0x0000FFFF);
		UPDATE_FRC_REG_BITS(FRC_REG_BLACKBAR_XYXY_ED, blackbar_xyxy[2] << 16, 0xFFFF0000);
		UPDATE_FRC_REG_BITS(FRC_REG_BLACKBAR_XYXY_ED, blackbar_xyxy[3],	  0x0000FFFF);

		//  FLAT
		UPDATE_FRC_REG_BITS(FRC_BBD_FLATNESS_DETEC_REGION_LFT_TOP, flat_xyxy[0] << 16,
				    0xFFFF0000);
		UPDATE_FRC_REG_BITS(FRC_BBD_FLATNESS_DETEC_REGION_LFT_TOP, flat_xyxy[1],
				    0x0000FFFF);
		UPDATE_FRC_REG_BITS(FRC_BBD_FLATNESS_DETEC_REGION_RIT_BOT, flat_xyxy[2] << 16,
				    0xFFFF0000);
		UPDATE_FRC_REG_BITS(FRC_BBD_FLATNESS_DETEC_REGION_RIT_BOT, flat_xyxy[3], 0x0000FFFF);

		//  MOTION
		UPDATE_FRC_REG_BITS(FRC_BBD_MOTION_DETEC_REGION_LFT_TOP_DS, bb_motion_xyxy[0] << 16,
				    0xFFFF0000);
		UPDATE_FRC_REG_BITS(FRC_BBD_MOTION_DETEC_REGION_LFT_TOP_DS, bb_motion_xyxy[1],
				    0x0000FFFF);
		UPDATE_FRC_REG_BITS(FRC_BBD_MOTION_DETEC_REGION_RIT_BOT_DS, bb_motion_xyxy[2] << 16,
				    0xFFFF0000);
		UPDATE_FRC_REG_BITS(FRC_BBD_MOTION_DETEC_REGION_RIT_BOT_DS, bb_motion_xyxy[3],
				    0x0000FFFF);

		//  APL
		UPDATE_FRC_REG_BITS(FRC_BBD_OOB_APL_CAL_LFT_TOP_RANGE, oob_apl_xyxy[0] << 16,
				    0xFFFF0000);
		UPDATE_FRC_REG_BITS(FRC_BBD_OOB_APL_CAL_LFT_TOP_RANGE, oob_apl_xyxy[1],
				    0x0000FFFF);
		UPDATE_FRC_REG_BITS(FRC_BBD_OOB_APL_CAL_RIT_BOT_RANGE, oob_apl_xyxy[2] << 16,
				    0xFFFF0000);
		UPDATE_FRC_REG_BITS(FRC_BBD_OOB_APL_CAL_RIT_BOT_RANGE, oob_apl_xyxy[3],
				    0x0000FFFF);

		//  HIST
		UPDATE_FRC_REG_BITS(FRC_BBD_APL_HIST_WIN_LFT_TOP, apl_hist_xyxy[0] << 16,
				    0xFFFF0000);
		UPDATE_FRC_REG_BITS(FRC_BBD_APL_HIST_WIN_LFT_TOP, apl_hist_xyxy[1],
				    0x0000FFFF);
		UPDATE_FRC_REG_BITS(FRC_BBD_APL_HIST_WIN_RIT_BOT, apl_hist_xyxy[2] << 16,
				    0xFFFF0000);
		UPDATE_FRC_REG_BITS(FRC_BBD_APL_HIST_WIN_RIT_BOT, apl_hist_xyxy[3],
				    0x0000FFFF);

		//  DETAIL
		UPDATE_FRC_REG_BITS(FRC_BBD_OOB_DETAIL_WIN_LFT_TOP, oob_h_detail_xyxy[0] << 16,
				    0xFFFF0000);
		UPDATE_FRC_REG_BITS(FRC_BBD_OOB_DETAIL_WIN_LFT_TOP, oob_h_detail_xyxy[1],
				    0x0000FFFF);
		UPDATE_FRC_REG_BITS(FRC_BBD_OOB_DETAIL_WIN_RIT_BOT, oob_h_detail_xyxy[2] << 16,
				    0xFFFF0000);
		UPDATE_FRC_REG_BITS(FRC_BBD_OOB_DETAIL_WIN_RIT_BOT, oob_h_detail_xyxy[3],
				    0x0000FFFF);

		//  BBD ME
		UPDATE_FRC_REG_BITS(FRC_REG_BLACKBAR_XYXY_ME_ST, blackbar_xyxy_me[0] << 16,
				    0xFFFF0000);
		UPDATE_FRC_REG_BITS(FRC_REG_BLACKBAR_XYXY_ME_ST, blackbar_xyxy_me[1],
				    0x0000FFFF);
		UPDATE_FRC_REG_BITS(FRC_REG_BLACKBAR_XYXY_ME_ED, blackbar_xyxy_me[2] << 16,
				    0xFFFF0000);
		UPDATE_FRC_REG_BITS(FRC_REG_BLACKBAR_XYXY_ME_ED, blackbar_xyxy_me[3], 0x0000FFFF);

		// for down sample img
		ds_xyxy[0] = (search_final_line_para->ds_xyxy_force == 1) ?
				det_lft : tmp_final_posi_new[0];   //  lft
		ds_xyxy[1] = (search_final_line_para->ds_xyxy_force == 1) ?
				det_top : tmp_final_posi_new[1];   //  top
		ds_xyxy[2] = (search_final_line_para->ds_xyxy_force == 1) ?
				det_rit : tmp_final_posi_new[2];   //  rit
		ds_xyxy[3] = (search_final_line_para->ds_xyxy_force == 1) ?
				det_bot : tmp_final_posi_new[3];   //  bot

		UPDATE_FRC_REG_BITS(FRC_REG_DS_WIN_SETTING_LFT_TOP, ds_xyxy[0] << 16, 0xFFFF0000);
		UPDATE_FRC_REG_BITS(FRC_REG_DS_WIN_SETTING_LFT_TOP, ds_xyxy[1], 0x0000FFFF);
		UPDATE_FRC_REG_BITS(FRC_BBD_DS_WIN_SETTING_RIT_BOT, ds_xyxy[2] << 16, 0xFFFF0000);
		UPDATE_FRC_REG_BITS(FRC_BBD_DS_WIN_SETTING_RIT_BOT, ds_xyxy[3], 0x0000FFFF);
	} else if (search_final_line_para->mode_switch == 1) {
		// top
		// motion 1 edge 1
		if (g_stbbd_rd->top_edge_posi_valid2 == 1 &&
		    g_stbbd_rd->top_motion_posi_valid2 == 1) {
			// pre_process 1
			if (ABS(motion_posi[1] - edge_posi[1]) <= top_bot_motion_edge_delta)
				motion_posi[1] = edge_posi[1];

			// pre process 2
			if (ABS(tmp_final_posi[1] - edge_posi[1]) <= top_bot_lbox_edge_delta) {
				tmp_final_posi[1] = MAX(tmp_final_posi[1], edge_posi[1]);
				edge_posi[1] = tmp_final_posi[1];
			}

			// tmp == edge; tmp> edge; tmp < edge
			// tmp == edge;
			if (tmp_final_posi[1] == edge_posi[1]) {
				cur_final_posi[1] = edge_posi[1];
				caption[1] = 0;
				caption_change[1] = 0;
				pr_frc(dbg_bbd + 0, "\nTopCase0\n");
			} else if (tmp_final_posi[1] > edge_posi[1]) {// tmp > edge;
				if (motion_posi[1] > edge_posi[1]) {// motion > edge
					// edge ==0
					if (edge_posi[1] == det_top)
						cur_final_posi[1] = pre_final_posi[1];
					else// edge != 0
						cur_final_posi[1] = edge_posi[1];
					pr_frc(dbg_bbd + 0, "\nTopCase1\n");
				} else {// motion <= edge
					cur_final_posi[1] = det_top;
					pr_frc(dbg_bbd + 0, "\nTopCase2\n");
				}
				caption[1] = 0;
				caption_change[1] = 0;
			} else {// tmp < edge;
				if (ABS(edge_posi[1] - pre_final_posi[1])
				    <= top_bot_border_edge_delta) {
					if (edge_posi[1] <= motion_posi[1]) {
						cur_final_posi[1] = edge_posi[1];
						caption[1] = 1;
						caption_change[1] = 0;
						pr_frc(dbg_bbd + 0, "\nTopCase3\n");
					} else {
						if ((search_final_line_para->pre_motion_posi[1] >
						     motion_posi[1] + top_bot_caption_motion_delta) && !
						    (search_final_line_para->caption[1] == 1 &&
						     search_final_line_para->caption_change[1] == 1)) {
							cur_final_posi[1] = edge_posi[1];
							caption[1] = 1;
							caption_change[1] = 1;
							pr_frc(dbg_bbd + 0, "\nTopCase4\n");
						} else {
							cur_final_posi[1] = tmp_final_posi[1];
							caption[1] = 0;
							caption_change[1] = 0;
							pr_frc(dbg_bbd + 0, "\nTopCase5\n");
						}
					}
				} else {
					if (edge_posi[1] < motion_posi[1]) {
						cur_final_posi[1] =
							MIN(edge_posi[1], tmp_final_posi[1]);
						caption[1] = 1;
						caption_change[1] = 0;
						pr_frc(dbg_bbd + 0, "\nTopCase6\n");
					} else {
						cnt_max = MAX(MIN(det_top + top_bot_motion_edge_line_num / 2 - 1, motion_posi[1]), det_top);

						if ((ABS(g_stbbd_rd->top_edge_posi1 - cnt_max) <= top_bot_motion_edge_delta) && g_stbbd_rd->top_edge_posi_valid1) {
							cur_final_posi[1] = g_stbbd_rd->top_edge_posi1;
							caption[1] = 1;
							caption_change[1] = 0;
							pr_frc(dbg_bbd + 0, "\nTopCase7\n");
						} else {
							if (search_final_line_para->pre_motion_posi[1] < edge_posi[1]) {
								if (sel2_mode[1] == 1)
									cur_final_posi[1] = MIN(search_final_line_para->pre_final_posi[1], tmp_final_posi[1]);//top
								else
									cur_final_posi[1] = tmp_final_posi[1];

								caption[1] = 0;
								caption_change[1] = 0;

								pr_frc(dbg_bbd + 0, "\nTopCase8\n");
							} else if ((search_final_line_para->pre_motion_posi[1] > motion_posi[1] + top_bot_caption_motion_delta) && !
								  (search_final_line_para->caption[1] == 1 && search_final_line_para->caption_change[1] == 1)) {
								if (search_final_line_para->top_bot_pre_motion_cur_motion_choose_edge_mode == 0) {
									cur_final_posi[1] = edge_posi[1];
									caption[1] = 1;
									caption_change[1] = 1;
									pr_frc(dbg_bbd + 0,
										"\nTopCase9\n");
								} else if (search_final_line_para->top_bot_pre_motion_cur_motion_choose_edge_mode == 1) {
									cur_final_posi[1] = tmp_final_posi[1];
									caption[1] = 0;
									caption_change[1] = 0;
									pr_frc(dbg_bbd + 0,
										"\nTopCase10\n");
								}
							} else {
								cur_final_posi[1] = tmp_final_posi[1];
								caption[1] = 0;
								caption_change[1] = 0;
								pr_frc(dbg_bbd + 0, "\nTopCase11\n");
							}
						}
					}
				}
			}
		} else if ((g_stbbd_rd->top_edge_posi_valid2 == 0) &&
			   (g_stbbd_rd->top_motion_posi_valid2 == 0)) {// motion 0 edge 0
			if (g_stbbd_rd->top_posi1 == g_stbbd_rd->top_posi2 &&
			    (g_stbbd_rd->top_posi1 <= (det_top + (ysize >> 2)))) {
				cur_final_posi[1] = g_stbbd_rd->top_posi1;
			} else {
				if (g_stbbd_rd->top_valid1 && g_stbbd_rd->top_valid2 &&
				    (ABS(g_stbbd_rd->top_posi1 - g_stbbd_rd->top_posi2) <=
				     top_bot_lbox_delta)) {
					cur_final_posi[1] = MIN(tmp_final_posi[1],
								MIN(g_stbbd_rd->top_posi2,
								    g_stbbd_rd->top_posi1));
				} else if (g_stbbd_rd->top_valid1 && !g_stbbd_rd->top_valid2) {
					cur_final_posi[1] = tmp_final_posi[1];
				} else if (!g_stbbd_rd->top_valid1 && g_stbbd_rd->top_valid2) {
					cur_final_posi[1] = tmp_final_posi[1];
				} else {
					cur_final_posi[1] = det_top;
					//cur_final_posi[1] = pre_final_top_posi;
				}
			} // motion 0 edge 0 -end
		} else if (g_stbbd_rd->top_edge_posi_valid2 == 1 &&
			   g_stbbd_rd->top_motion_posi_valid2 == 0) {
			// motion 0 edge 1
			// edge  near lbox line
			if (ABS(tmp_final_posi[1] - edge_posi[1]) <= top_bot_lbox_edge_delta) {
				tmp_final_posi[1] = MAX(tmp_final_posi[1], edge_posi[1]);
				edge_posi[1] = tmp_final_posi[1];
			}

			// edge = lbox
			if (tmp_final_posi[1] == edge_posi[1]) {
				cur_final_posi[1] = edge_posi[1];
				caption[1] = 0;
				caption_change[1] = 0;
			} else if (tmp_final_posi[1] > edge_posi[1]) {
				// edge outside of lbox let lbox false
				if (g_stbbd_rd->top_motion_posi1 < edge_posi[1] &&
				    g_stbbd_rd->top_motion_posi_valid1) {
					cur_final_posi[1] = g_stbbd_rd->top_motion_posi1;
				} else {
					cur_final_posi[1] = edge_posi[1];
				}
			} else if (tmp_final_posi[1] < edge_posi[1]) {
				// edge inside of lbox
				if (ABS(edge_posi[1] - pre_final_posi[1]) <=
				    top_bot_border_edge_delta)
					cur_final_posi[1] = edge_posi[1];
				else
					cur_final_posi[1] = tmp_final_posi[1];
			}
		} else if (g_stbbd_rd->top_edge_posi_valid2 == 0 &&
			   g_stbbd_rd->top_motion_posi_valid2 == 1) {
			// motion 1 edge 0
			cnt_max = MAX(motion_posi[1], tmp_final_posi[1]);

			if (g_stbbd_rd->top_edge_posi1 < cnt_max &&
			    g_stbbd_rd->top_edge_posi_valid1) {
				// pre_process 1
				if (ABS(motion_posi[1] - g_stbbd_rd->top_edge_posi1)
				    <= top_bot_motion_edge_delta)
					motion_posi[1] = g_stbbd_rd->top_edge_posi1;

				// pre process 2
				if (ABS(tmp_final_posi[1] - g_stbbd_rd->top_edge_posi1) <=
				    top_bot_lbox_edge_delta) {
					tmp_final_posi[1] =
						MAX(tmp_final_posi[1], g_stbbd_rd->top_edge_posi1);
					g_stbbd_rd->top_edge_posi1 = tmp_final_posi[1];
				}

				if (g_stbbd_rd->top_edge_posi1 == tmp_final_posi[1]) {
					cur_final_posi[1] = g_stbbd_rd->top_edge_posi1;
					caption[1] = 0;
					caption_change[1] = 0;
				} else if (tmp_final_posi[1] > g_stbbd_rd->top_edge_posi1) {
					if (motion_posi[1] > g_stbbd_rd->top_edge_posi1) {
						if (g_stbbd_rd->top_edge_posi1 == det_top)
							//top
							cur_final_posi[1] =
							search_final_line_para->pre_final_posi[1];
						else
							cur_final_posi[1] =
								g_stbbd_rd->top_edge_posi1;
					} else {
						cur_final_posi[1] = det_top;
					}
					caption[1] = 0;
					caption_change[1] = 0;
				} else {
					if (ABS(g_stbbd_rd->top_edge_posi1 - pre_final_posi[1]) <=
					    top_bot_border_edge_delta) {
						cur_final_posi[1] = g_stbbd_rd->top_edge_posi1;
					} else {
						cur_final_posi[1] =
							MIN(edge_posi[1], tmp_final_posi[1]);
					}
				}
			} else if ((motion_posi[1] >= tmp_final_posi[1]) ||
				  (ABS(motion_posi[1] - tmp_final_posi[1]) <= 1)) {
				cur_final_posi[1] = tmp_final_posi[1];
			} else {
				cur_final_posi[1] = det_top;
			}
		} // motion 1 edge 0

		tmp_final_posi[1] = cur_final_posi[1];

		// top end
		// bot
		if (g_stbbd_rd->bot_motion_posi_valid2 == 1 &&
		    g_stbbd_rd->bot_edge_posi_valid2 == 1) {
			// pre_process 1
			if (ABS(motion_posi[3] - edge_posi[3]) <= top_bot_motion_edge_delta)
				motion_posi[3] = edge_posi[3];

			// pre process 2
			if (ABS(tmp_final_posi[3] - edge_posi[3]) <= top_bot_lbox_edge_delta) {
				tmp_final_posi[3] = MIN(tmp_final_posi[3], edge_posi[3]);
				edge_posi[3] = tmp_final_posi[3];
			}

			// tmp == edge; tmp> edge; tmp < edge
			// tmp == edge;
			if (tmp_final_posi[3] == edge_posi[3]) {
				cur_final_posi[3] = edge_posi[3];
				caption[3] = 0;
				caption_change[3] = 0;
				pr_frc(dbg_bbd + 0, "\nBotCase0\n");
			} else if (tmp_final_posi[3] < edge_posi[3]) {
				// tmp < edge;
				// motion > edge
				if (motion_posi[3] < edge_posi[3]) {
					// edge == det bot
					if (edge_posi[3] == det_bot)
						cur_final_posi[3] = pre_final_posi[3];
					else // edge != det bot
						cur_final_posi[3] = edge_posi[3];
					pr_frc(dbg_bbd + 0, "\nBotCase1\n");
				} else {// motion <= edge
					cur_final_posi[3] = det_bot;
					pr_frc(dbg_bbd + 0, "\nBotCase2\n");
				}
				caption[3] = 0;
				caption_change[3] = 0;
			} else {// tmp < edge;
				if (ABS(edge_posi[3] - pre_final_posi[3])
				    <= top_bot_border_edge_delta) {
					if (edge_posi[3] >= motion_posi[3]) {
						cur_final_posi[3] = edge_posi[3];
						caption[3] = 1;
						caption_change[3] = 0;
						pr_frc(dbg_bbd + 0, "\nBotCase3\n");
					} else {
						if ((search_final_line_para->pre_motion_posi[3] <
						     motion_posi[3] -
						     top_bot_caption_motion_delta) && !
						    (search_final_line_para->caption[3] == 1 &&
						     search_final_line_para->caption_change[3] ==
						     1)) {
							cur_final_posi[3] = edge_posi[3];
							caption[3] = 1;
							caption_change[3] = 1;
							pr_frc(dbg_bbd + 0, "\nBotCase4\n");
						} else {
							cur_final_posi[3] = tmp_final_posi[3];
							caption[3] = 0;
							caption_change[3] = 0;
							pr_frc(dbg_bbd + 0, "\nBotCase5\n");
						}
					}
				} else {
					if (edge_posi[3] > motion_posi[3]) {
						cur_final_posi[3] = MAX(edge_posi[3],
									tmp_final_posi[3]);
						caption[3] = 1;
						caption_change[3] = 0;
						pr_frc(dbg_bbd + 0, "\nBotCase6\n");
					} else {
						cnt_max =
						MIN(MAX(det_bot + 1 -
							top_bot_motion_edge_line_num / 2,
							motion_posi[3]),
							det_bot);

						if ((ABS(g_stbbd_rd->bot_edge_posi1 - cnt_max) <= top_bot_motion_edge_delta) &&
						    g_stbbd_rd->bot_edge_posi_valid1) {
							cur_final_posi[3] =
								g_stbbd_rd->bot_edge_posi1;
							caption[3] = 1;
							caption_change[3] = 0;
							pr_frc(dbg_bbd + 0, "\nBotCase7\n");
						} else {
							if (search_final_line_para->pre_motion_posi[3] > edge_posi[3]) {
								if (sel2_mode[3] == 1)
									cur_final_posi[3] = MAX(search_final_line_para->pre_final_posi[3], tmp_final_posi[3]);//bot
								else
									cur_final_posi[3] = tmp_final_posi[3];//bot

								caption[3] = 0;
								caption_change[3] = 0;
								pr_frc(dbg_bbd + 0, "\nBotCase8\n");
							} else if ((search_final_line_para->pre_motion_posi[3] <
								   motion_posi[3] - top_bot_caption_motion_delta) && !
								  (search_final_line_para->caption[3] == 1 && search_final_line_para->caption_change[3] == 1)) {
								if (search_final_line_para->top_bot_pre_motion_cur_motion_choose_edge_mode == 0) {
									cur_final_posi[3] = edge_posi[3];
									caption[3] = 1;
									caption_change[3] = 1;
									pr_frc(dbg_bbd + 0, "\nBotCase9\n");
								} else if (search_final_line_para->top_bot_pre_motion_cur_motion_choose_edge_mode == 1) {
									cur_final_posi[3] = tmp_final_posi[3];
									caption[3] = 0;
									caption_change[3] = 0;
									pr_frc(dbg_bbd + 0, "\nBotCase10\n");
								}
							} else {
								cur_final_posi[3] =
									tmp_final_posi[3];
								caption[3] = 0;
								caption_change[3] = 0;
								pr_frc(dbg_bbd + 0, "\nBotCase11\n");
							}
						}
					}
				}
			}
		} else if ((g_stbbd_rd->bot_motion_posi_valid2 == 0) &&
			   (g_stbbd_rd->bot_edge_posi_valid2 == 0)) {
			// motion 0 edge 0
			if (g_stbbd_rd->bot_posi1 == g_stbbd_rd->bot_posi2 &&
			    (g_stbbd_rd->bot_posi1 >= (det_bot + 1 - (ysize >> 2)))) {
				cur_final_posi[3] = g_stbbd_rd->bot_posi1;
			} else {
				if (g_stbbd_rd->bot_valid1 && g_stbbd_rd->bot_valid2 &&
				    (ABS(g_stbbd_rd->bot_posi1 - g_stbbd_rd->bot_posi2) <=
				     top_bot_lbox_delta)) {
					cur_final_posi[3] =
						MAX(tmp_final_posi[3],
						    MAX(g_stbbd_rd->bot_posi1,
						    g_stbbd_rd->bot_posi2));
				} else if (g_stbbd_rd->bot_valid1 && !g_stbbd_rd->bot_valid2) {
					cur_final_posi[3] = tmp_final_posi[3];
				} else if (!g_stbbd_rd->bot_valid1 && g_stbbd_rd->bot_valid2) {
					cur_final_posi[3] = tmp_final_posi[3];
				} else {
					cur_final_posi[3] = det_bot;
					//cur_final_posi[3] = pre_final_bot_posi;
				}
			}
		}

		// motion 0 edge 1
		else if ((g_stbbd_rd->bot_edge_posi_valid2 == 1) &&
			 (g_stbbd_rd->bot_motion_posi_valid2 == 0)) {
			// edge  near lbox line
			if (ABS(tmp_final_posi[3] - edge_posi[3]) <= top_bot_lbox_edge_delta) {
				tmp_final_posi[3] = MIN(tmp_final_posi[3], edge_posi[3]);
				edge_posi[3] = tmp_final_posi[3];
			}

			// edge = lbox
			if (tmp_final_posi[3] == edge_posi[3]) {
				cur_final_posi[3] = edge_posi[3];
				caption[3] = 0;
				caption_change[3] = 0;
			} else if (tmp_final_posi[3] < edge_posi[3]) {
				// edge outside of lbox let lbox false
				if (g_stbbd_rd->bot_motion_posi1 > edge_posi[3] &&
				    g_stbbd_rd->bot_motion_posi_valid1)
					cur_final_posi[3] = g_stbbd_rd->bot_motion_posi1;
				else
					cur_final_posi[3] = edge_posi[3];
			} else if (tmp_final_posi[3] > edge_posi[3]) {
				// edge inside of lbox
				if (ABS(edge_posi[3] - pre_final_posi[3]) <=
				    top_bot_border_edge_delta)
					cur_final_posi[3] = edge_posi[3];
				else
					cur_final_posi[3] = tmp_final_posi[3];
			}
		} else if (g_stbbd_rd->bot_edge_posi_valid2 == 0 &&
			   g_stbbd_rd->bot_motion_posi_valid2 == 1) {
			// motion 1 edge 0
			cnt_min = MIN(motion_posi[3], tmp_final_posi[3]);
			if (g_stbbd_rd->bot_edge_posi1 > cnt_min &&
			    g_stbbd_rd->bot_edge_posi_valid1) {
				// pre_process 1
				if (ABS(motion_posi[3] - g_stbbd_rd->bot_edge_posi1) <=
				    top_bot_motion_edge_delta) {
					motion_posi[3] = g_stbbd_rd->bot_edge_posi1;
				}

				// pre process 2
				if (ABS(tmp_final_posi[3] - g_stbbd_rd->bot_edge_posi1)
				    <= top_bot_lbox_edge_delta) {
					tmp_final_posi[3] = MIN(tmp_final_posi[3],
								g_stbbd_rd->bot_edge_posi1);
					g_stbbd_rd->bot_edge_posi1 = tmp_final_posi[3];
				}

				if (g_stbbd_rd->bot_edge_posi1 == tmp_final_posi[3]) {
					cur_final_posi[3] = g_stbbd_rd->bot_edge_posi1;
					caption[3] = 0;
					caption_change[3] = 0;
				} else if (tmp_final_posi[3] < g_stbbd_rd->bot_edge_posi1) {
					if (motion_posi[3] < g_stbbd_rd->bot_edge_posi1) {
						if (g_stbbd_rd->bot_edge_posi1 == det_bot)
							cur_final_posi[3] =
							search_final_line_para->pre_final_posi[3];
						else
							cur_final_posi[3] =
								g_stbbd_rd->bot_edge_posi1;
					} else {
						cur_final_posi[3] = det_bot;
					}
					caption[3] = 0;
					caption_change[3] = 0;
				} else {
					if (ABS(g_stbbd_rd->bot_edge_posi1 - pre_final_posi[3]) <=
						top_bot_border_edge_delta) {
						cur_final_posi[3] = g_stbbd_rd->bot_edge_posi1;
					} else {
						cur_final_posi[3] =
							MAX(edge_posi[3], tmp_final_posi[3]);
					}
				}

			} else if ((motion_posi[3] <= tmp_final_posi[3]) ||
				   (ABS(motion_posi[3] - tmp_final_posi[3]) <= 1)) {
				cur_final_posi[3] = tmp_final_posi[3];
			} else {
				cur_final_posi[3] = det_bot;
			}
		} // motion 1 edge 0

		//xil_printf("tmp: %d, cur:%d,posi1:%d, posi2:%d\n",tmp_final_bot_posi,
		//	     cur_final_posi[3], g_stbbd_rd.bot_posi1, g_stbbd_rd.bot_posi2);

		tmp_final_posi[3] = cur_final_posi[3];

		// bot end
		// lft
		if (g_stbbd_rd->lft_motion_posi_valid2 == 1 &&
		    g_stbbd_rd->lft_edge_posi_valid2 == 1) {
			// pre_process 1
			if (ABS(motion_posi[0] - edge_posi[0]) <= lft_rit_motion_edge_delta)
				motion_posi[0] = edge_posi[0];

			// pre process 2
			if (ABS(tmp_final_posi[0] - edge_posi[0]) <= lft_rit_lbox_edge_delta) {
				tmp_final_posi[0] = MAX(tmp_final_posi[0], edge_posi[0]);
				edge_posi[0] = tmp_final_posi[0];
			}

			// tmp == edge; tmp> edge; tmp < edge
			// tmp == edge;
			if (tmp_final_posi[0] == edge_posi[0]) {
				cur_final_posi[0] = edge_posi[0];
				caption[0] = 0;
				caption_change[0] = 0;
				pr_frc(dbg_bbd + 0, "\nLftCase0\n");
			} else if (tmp_final_posi[0] > edge_posi[0]) {
				// motion > edge
				if (motion_posi[0] > edge_posi[0]) {
					// edge == det lft
					if (edge_posi[0] == det_lft) {
						cur_final_posi[0] = pre_final_posi[0];
					} else {// edge != det lft
						cur_final_posi[0] = edge_posi[0];
					}
					pr_frc(dbg_bbd + 0, "\nLftCase1\n");
				} else {// motion <= edge
					cur_final_posi[0] = det_lft;
					pr_frc(dbg_bbd + 0, "\nLftCase2\n");
				}
				caption[0] = 0;
				caption_change[0] = 0;
			} else {// tmp < edge;
				if (ABS(edge_posi[0] - pre_final_posi[0]) <=
				    lft_rit_border_edge_delta) {
					if (edge_posi[0] <= motion_posi[0])	{
						cur_final_posi[0] = edge_posi[0];
						caption[0] = 1;
						caption_change[0] = 0;
						pr_frc(dbg_bbd + 0, "\nLftCase3\n");
					} else {
						if ((search_final_line_para->pre_motion_posi[0] >
						     motion_posi[0] +
						     lft_rit_caption_motion_delta) && !
						    (search_final_line_para->caption[0] == 1 &&
						     search_final_line_para->caption_change[0]
						     == 1)) {
							cur_final_posi[0] = edge_posi[0];
							caption[0] = 1;
							caption_change[0] = 1;
							pr_frc(dbg_bbd + 0, "\nLftCase4\n");
						} else {
							cur_final_posi[0] = tmp_final_posi[0];
							caption[0] = 0;
							caption_change[0] = 0;
							pr_frc(dbg_bbd + 0, "\nLftCase5\n");
						}
					}
				} else {
					if (edge_posi[0] < motion_posi[0]) {
						cur_final_posi[0] =
							MIN(edge_posi[0], tmp_final_posi[0]);
						caption[0] = 1;
						caption_change[0] = 0;
						pr_frc(dbg_bbd + 0, "\nLftCase6\n");
					} else {
						cnt_max = MAX(MIN(det_lft +
								  lft_rit_motion_edge_line_num / 2 - 1,
								  motion_posi[0]),
								  det_lft);

						if ((ABS(g_stbbd_rd->lft_edge_posi1 - cnt_max) <= lft_rit_motion_edge_delta) &&
						    g_stbbd_rd->lft_edge_posi_valid1) {
							cur_final_posi[0] = g_stbbd_rd->lft_edge_posi1;
							caption[0] = 1;
							caption_change[0] = 0;
							pr_frc(dbg_bbd + 0, "\nLftCase7\n");
						} else {
							if (search_final_line_para->pre_motion_posi[0] < edge_posi[0]) {
								if (sel2_mode[0] == 1)
									cur_final_posi[0] = MIN(search_final_line_para->pre_final_posi[0], tmp_final_posi[0]);//lft
								else
									cur_final_posi[0] = tmp_final_posi[0];//lft

								caption[0] = 0;
								caption_change[0] = 0;
								pr_frc(dbg_bbd + 0, "\nLftCase8\n");
							} else if ((search_final_line_para->pre_motion_posi[0] > motion_posi[0] + lft_rit_caption_motion_delta) && !
								   (search_final_line_para->caption[0] == 1 && search_final_line_para->caption_change[0] == 1)) {
								if (search_final_line_para->lft_rit_pre_motion_cur_motion_choose_edge_mode == 0) {
									cur_final_posi[0] = edge_posi[0];
									caption[0] = 1;
									caption_change[0] = 1;
									pr_frc(dbg_bbd + 0, "\nLftCase9\n");
								} else if (search_final_line_para->lft_rit_pre_motion_cur_motion_choose_edge_mode == 1) {
									cur_final_posi[0] = tmp_final_posi[0];
									caption[0] = 0;
									caption_change[0] = 0;
									pr_frc(dbg_bbd + 0, "\nLftCase10\n");
								}
							} else {
								cur_final_posi[0] = tmp_final_posi[0];
								caption[0] = 0;
								caption_change[0] = 0;
								pr_frc(dbg_bbd + 0, "\nLftCase11\n");
							}
						}
					}
				}
			}
		} else if ((g_stbbd_rd->lft_motion_posi_valid2 == 0) &&
			   (g_stbbd_rd->lft_edge_posi_valid2 == 0)) {
			// motion 0 edge 0
			if (g_stbbd_rd->finer_lft_posi1 == g_stbbd_rd->finer_lft_posi2 &&
			    (g_stbbd_rd->finer_lft_posi1 <= (det_lft + (xsize >> 2)))) {
				cur_final_posi[0] = g_stbbd_rd->finer_lft_posi1;
			} else {
				if (g_stbbd_rd->finer_lft_valid1 && g_stbbd_rd->finer_lft_valid2 &&
				    (ABS(g_stbbd_rd->finer_lft_posi1 -
				     g_stbbd_rd->finer_lft_posi2) <= lft_rit_lbox_delta)) {
					cur_final_posi[0] = MIN(tmp_final_posi[0],
								MIN(g_stbbd_rd->finer_lft_posi1,
								g_stbbd_rd->finer_lft_posi2));
				} else if (g_stbbd_rd->finer_lft_valid1 &&
					   !g_stbbd_rd->finer_lft_valid2) {
					cur_final_posi[0] = tmp_final_posi[0];
				} else if (!g_stbbd_rd->finer_lft_valid1 &&
					   g_stbbd_rd->finer_lft_valid2) {
					cur_final_posi[0] = tmp_final_posi[0];
				} else {
					cur_final_posi[0] = det_lft;
					//cur_final_posi[0] = pre_final_lft_posi;
				}
			}
			// motion 0 edge 0
		} else if (g_stbbd_rd->lft_edge_posi_valid2 == 1 &&
			   g_stbbd_rd->lft_motion_posi_valid2 == 0) {
			// motion 0 edge 1
			// edge  near lbox line
			if (ABS(tmp_final_posi[0] - edge_posi[0]) <= lft_rit_lbox_edge_delta) {
				tmp_final_posi[0] = MAX(tmp_final_posi[0], edge_posi[0]);
				edge_posi[0] = tmp_final_posi[0];
			}

			// edge = lbox
			if (tmp_final_posi[0] == edge_posi[0]) {
				cur_final_posi[0] = edge_posi[0];
				caption[0] = 0;
				caption_change[0] = 0;
			} else if (tmp_final_posi[0] > edge_posi[0]) {
				// edge outside of lbox let lbox false
				if (g_stbbd_rd->lft_motion_posi1 < edge_posi[0] &&
				    g_stbbd_rd->lft_motion_posi_valid1) {
					cur_final_posi[0] = g_stbbd_rd->lft_motion_posi1;
				} else {
					cur_final_posi[0] = edge_posi[0];
				}
			}  else if (tmp_final_posi[0] < edge_posi[0]) {
				// edge inside of lbox
				if (ABS(edge_posi[0] - pre_final_posi[0]) <=
				    lft_rit_border_edge_delta)
					cur_final_posi[0] = edge_posi[0];
				else
					cur_final_posi[0] = tmp_final_posi[0];
			}

		} else if (g_stbbd_rd->lft_edge_posi_valid2 == 0 &&
			   g_stbbd_rd->lft_motion_posi_valid2 == 1) {
			// motion 1 edge 0
			cnt_max = MAX(motion_posi[0], tmp_final_posi[0]);

			if (g_stbbd_rd->lft_edge_posi1 < cnt_max &&
			    g_stbbd_rd->lft_edge_posi_valid1) {
				// pre_process 1
				if (ABS(motion_posi[0] - g_stbbd_rd->lft_edge_posi1) <=
				    lft_rit_motion_edge_delta) {
					motion_posi[0] = g_stbbd_rd->lft_edge_posi1;
				}

				// pre process 2
				if (ABS(tmp_final_posi[0] - g_stbbd_rd->lft_edge_posi1) <=
				    lft_rit_lbox_edge_delta) {
					tmp_final_posi[0] = MAX(tmp_final_posi[0],
								g_stbbd_rd->lft_edge_posi1);
					g_stbbd_rd->lft_edge_posi1 = tmp_final_posi[0];
				}

				if (g_stbbd_rd->lft_edge_posi1 == tmp_final_posi[0]) {
					cur_final_posi[0] = g_stbbd_rd->lft_edge_posi1;
					caption[0] = 0;
					caption_change[0] = 0;
				} else if (tmp_final_posi[0] > g_stbbd_rd->lft_edge_posi1) {
					if (motion_posi[0] > g_stbbd_rd->lft_edge_posi1) {
						if (g_stbbd_rd->lft_edge_posi1 == det_lft)
							//lft
							cur_final_posi[0] =
							search_final_line_para->pre_final_posi[0];
						else
							cur_final_posi[0] =
								g_stbbd_rd->lft_edge_posi1;
					} else {
						cur_final_posi[0] = det_lft;
					}
						caption[0] = 0;
						caption_change[0] = 0;
				} else {
					if (ABS(g_stbbd_rd->lft_edge_posi1 - pre_final_posi[0]) <=
					    lft_rit_border_edge_delta) {
						cur_final_posi[0] =
							g_stbbd_rd->lft_edge_posi1;
					} else {
						cur_final_posi[0] =
						MIN(edge_posi[0], tmp_final_posi[0]);
					}
				}
			} else if ((motion_posi[0] >= tmp_final_posi[0]) ||
				   (ABS(motion_posi[0] - tmp_final_posi[0]) <= 1)) {
				cur_final_posi[0] = tmp_final_posi[0];
			} else {
				cur_final_posi[0] = det_lft;
			}
		} // motion 1 edge 0

		tmp_final_posi[0] = cur_final_posi[0];
		// lft end

		// rit
		// motion 1 edge 1
		if (g_stbbd_rd->rit_motion_posi_valid2 == 1 &&
		    g_stbbd_rd->rit_edge_posi_valid2 == 1) {
			// pre_process 1
			if (ABS(motion_posi[2] - edge_posi[2]) <= lft_rit_motion_edge_delta)
				motion_posi[2] = edge_posi[2];

			// pre process 2
			if (ABS(tmp_final_posi[2] - edge_posi[2]) <= lft_rit_lbox_edge_delta) {
				tmp_final_posi[2] = MIN(tmp_final_posi[2], edge_posi[2]);
				edge_posi[2] = tmp_final_posi[2];
			}

			// tmp == edge; tmp> edge; tmp < edge
			// tmp == edge;
			if (tmp_final_posi[2] == edge_posi[2]) {
				cur_final_posi[2] = edge_posi[2];
				caption[2] = 0;
				caption_change[2] = 0;
				pr_frc(dbg_bbd + 0, "\nRitCase0\n");
			} else if (tmp_final_posi[2] < edge_posi[2]) { // tmp < edge;
				// motion > edge
				if (motion_posi[2] < edge_posi[2]) {
					// edge == det rit
					if (edge_posi[2] == det_rit) {
						cur_final_posi[2] = pre_final_posi[2];
					} else {// edge != det rit
						cur_final_posi[2] = edge_posi[2];
					}
					pr_frc(dbg_bbd + 0, "\nRitCase1\n");
				} else {// motion <= edge
					cur_final_posi[2] = det_rit;
					pr_frc(dbg_bbd + 0, "\nRitCase2\n");
				}
				caption[2] = 0;
				caption_change[2] = 0;
			} else {
				// tmp < edge;
				if (ABS(edge_posi[2] - pre_final_posi[2]) <=
				    lft_rit_border_edge_delta) {
					if (edge_posi[2] >= motion_posi[2]) {
						cur_final_posi[2] = edge_posi[2];
						caption[2] = 1;
						caption_change[2] = 0;
						pr_frc(dbg_bbd + 0, "\nRitCase3\n");
					} else {
						if ((search_final_line_para->pre_motion_posi[2] <
						     motion_posi[2] -
						     lft_rit_caption_motion_delta) && !
						    (search_final_line_para->caption[2] == 1 &&
						     search_final_line_para->caption_change[2] ==
						     1)) {
							cur_final_posi[2] = edge_posi[2];
							caption[2] = 1;
							caption_change[2] = 1;
							pr_frc(dbg_bbd + 0, "\nRitCase4\n");
						} else {
							cur_final_posi[2] = tmp_final_posi[2];
							caption[2] = 0;
							caption_change[2] = 0;
							pr_frc(dbg_bbd + 0, "\nRitCase5\n");
						}
					}
				} else {
					if (edge_posi[2] > motion_posi[2]) {
						cur_final_posi[2] =
							MAX(edge_posi[2], tmp_final_posi[2]);
						caption[2] = 1;
						caption_change[2] = 0;
						pr_frc(dbg_bbd + 0, "\nRitCase6\n");
					} else {
						cnt_max = MIN(MAX(det_rit + 1 - lft_rit_motion_edge_line_num / 2, motion_posi[2]), det_rit);

						if ((ABS(g_stbbd_rd->rit_edge_posi1 - cnt_max) <= lft_rit_motion_edge_delta) && g_stbbd_rd->rit_edge_posi_valid1) {
							cur_final_posi[2] = g_stbbd_rd->rit_edge_posi1;
							caption[2] = 1;
							caption_change[2] = 0;
							pr_frc(dbg_bbd + 0, "\nRitCase7\n");
						} else {
							if (search_final_line_para->pre_motion_posi[2] > edge_posi[2]) {
								if (sel2_mode[2] == 1)
									cur_final_posi[2] = MAX(search_final_line_para->pre_final_posi[2], tmp_final_posi[2]);//rit
								else
									cur_final_posi[2] = tmp_final_posi[2];//rit

								caption[2] = 0;
								caption_change[2] = 0;
								pr_frc(dbg_bbd + 0, "\nRitCase8\n");
							} else if ((search_final_line_para->pre_motion_posi[2] < motion_posi[2] - lft_rit_caption_motion_delta) && !
								   (search_final_line_para->caption[2] == 1 && search_final_line_para->caption_change[2] == 1)) {
								if (search_final_line_para->lft_rit_pre_motion_cur_motion_choose_edge_mode == 0) {
									cur_final_posi[2] = edge_posi[2];
									caption[2] = 1;
									caption_change[2] = 1;
									pr_frc(dbg_bbd + 0, "\nRitCase9\n");
								} else if (search_final_line_para->lft_rit_pre_motion_cur_motion_choose_edge_mode == 1) {
									cur_final_posi[2] = tmp_final_posi[2];
									caption[2] = 0;
									caption_change[2] = 0;
									pr_frc(dbg_bbd + 0, "\nRitCase10\n");
								}
							} else {
								cur_final_posi[2] =
									tmp_final_posi[2];
								caption[2] = 0;
								caption_change[2] = 0;
								pr_frc(dbg_bbd + 0,
									"\nRitCase11\n");
							}
						}
					}
				}
			}
		} else if ((g_stbbd_rd->rit_motion_posi_valid2 == 0) &&
			(g_stbbd_rd->rit_edge_posi_valid2 == 0)) {
			// motion 0 edge 0
			if (g_stbbd_rd->finer_rit_posi1 == g_stbbd_rd->finer_rit_posi2 &&
			    (g_stbbd_rd->finer_rit_posi1 >= (det_rit + 1 - (xsize >> 2)))) {
				cur_final_posi[2] = g_stbbd_rd->finer_rit_posi1;
			} else {
				if (g_stbbd_rd->finer_rit_valid1 && g_stbbd_rd->finer_rit_valid2 &&
				    (ABS(g_stbbd_rd->finer_rit_posi1 - g_stbbd_rd->finer_rit_posi2)
				     <= lft_rit_lbox_delta)) {
					cur_final_posi[2] = MAX(tmp_final_posi[2],
								MAX(g_stbbd_rd->finer_rit_posi1,
								    g_stbbd_rd->finer_rit_posi2));
				} else if (g_stbbd_rd->finer_rit_valid1 &&
					   !g_stbbd_rd->finer_rit_valid2) {
					cur_final_posi[2] = tmp_final_posi[2];
				} else if (!g_stbbd_rd->finer_rit_valid1 &&
					   g_stbbd_rd->finer_rit_valid2) {
					cur_final_posi[2] = tmp_final_posi[2];
				} else {
					cur_final_posi[2] = det_rit;
					//cur_final_posi[2] = pre_final_rit_posi;
				}
			}
		} else if (g_stbbd_rd->rit_edge_posi_valid2 == 1 &&
			   g_stbbd_rd->rit_motion_posi_valid2 == 0) {
			// motion 0 edge 1
			// edge  near lbox line
			if (ABS(tmp_final_posi[2] - edge_posi[2]) <=  lft_rit_lbox_edge_delta) {
				tmp_final_posi[2] = MIN(tmp_final_posi[2], edge_posi[2]);
				edge_posi[2] = tmp_final_posi[2];
			}

			// edge = lbox
			if (tmp_final_posi[2] == edge_posi[2]) {
				cur_final_posi[2] = edge_posi[2];
				caption[2] = 0;
				caption_change[2] = 0;
			} else if (tmp_final_posi[2] < edge_posi[2]) {
				// edge outside of lbox let lbox false
				if (g_stbbd_rd->rit_motion_posi1 > edge_posi[2] &&
				    g_stbbd_rd->rit_motion_posi_valid1) {
					cur_final_posi[2] = g_stbbd_rd->rit_motion_posi1;
				} else {
					cur_final_posi[2] = edge_posi[2];
				}
			} else if (tmp_final_posi[2] > edge_posi[2]) {
				// edge inside of lbox
				if (ABS(edge_posi[2] - pre_final_posi[2]) <=
				    lft_rit_border_edge_delta)
					cur_final_posi[2] = edge_posi[2];
				else
					cur_final_posi[2] = tmp_final_posi[2];
			}
		} else if (g_stbbd_rd->rit_edge_posi_valid2 == 0 &&
			   g_stbbd_rd->rit_motion_posi_valid2 == 1) {
			// motion 1 edge 0
			cnt_min = MIN(motion_posi[2], tmp_final_posi[2]);

			if (g_stbbd_rd->rit_edge_posi1 > cnt_min &&
			    g_stbbd_rd->rit_edge_posi_valid1) {
				// pre_process 1
				if (ABS(motion_posi[2] - g_stbbd_rd->rit_edge_posi1)
				    <= lft_rit_motion_edge_delta) {
					motion_posi[2] = g_stbbd_rd->rit_edge_posi1;
				}

				// pre process 2
				if (ABS(tmp_final_posi[2] - g_stbbd_rd->rit_edge_posi1) <=
				    lft_rit_lbox_edge_delta) {
					tmp_final_posi[2] = MIN(tmp_final_posi[2],
								g_stbbd_rd->rit_edge_posi1);
					g_stbbd_rd->rit_edge_posi1 = tmp_final_posi[2];
				}

				if (g_stbbd_rd->rit_edge_posi1 == tmp_final_posi[2]) {
					cur_final_posi[2] = g_stbbd_rd->rit_edge_posi1;
					caption[2] = 0;
					caption_change[2] = 0;
				} else if (tmp_final_posi[2] < g_stbbd_rd->rit_edge_posi1) {
					if (motion_posi[2] < g_stbbd_rd->rit_edge_posi1) {
						if (g_stbbd_rd->rit_edge_posi1 == det_rit)
							//rit
							cur_final_posi[2] =
							search_final_line_para->pre_final_posi[2];
						else
							cur_final_posi[2] =
								g_stbbd_rd->rit_edge_posi1;
					} else {
						cur_final_posi[2] = det_rit;
					}
					caption[2] = 0;
					caption_change[2] = 0;

				} else {
					if (ABS(g_stbbd_rd->rit_edge_posi1 - pre_final_posi[2])
					    <= lft_rit_border_edge_delta) {
						cur_final_posi[2] = g_stbbd_rd->rit_edge_posi1;
					} else {
						cur_final_posi[2] =
							MAX(edge_posi[2], tmp_final_posi[2]);
					}
				}
			} else if (motion_posi[2] <= tmp_final_posi[2]) {
				cur_final_posi[2] = tmp_final_posi[2];
			} else {
				cur_final_posi[2] = det_rit;
			}
		} // motion 1 edge 0

		tmp_final_posi[2] = cur_final_posi[2];

		// rit end

		// BBD IIR
		if (search_final_line_para->final_iir_mode == 1) {
			//xil_printf("\n before sel top:%4d bot:%4d lft:%4d rit:%4d]",
			//tmp_final_posi[1], tmp_final_posi[3], tmp_final_posi[0],
			//tmp_final_posi[2]);
			lft_final_line1[search_final_line_para->final_iir_sel_rst_num - 1] =
				tmp_final_posi[0];
			top_final_line1[search_final_line_para->final_iir_sel_rst_num - 1] =
				tmp_final_posi[1];
			rit_final_line1[search_final_line_para->final_iir_sel_rst_num - 1] =
				tmp_final_posi[2];
			bot_final_line1[search_final_line_para->final_iir_sel_rst_num - 1] =
				tmp_final_posi[3];

			if (tmp_final_posi[0] + lft_rit_pre_cur_posi_delta < pre_final_posi[0])
				tmp_final_posi_new[0] = tmp_final_posi[0];
			else
				tmp_final_posi_new[0] =
					frc_bbd_line_stable(frc_devp, &stable_flag[0],
							    lft_final_line1,
							    pre_final_posi[0],
							    lft_rit_pre_cur_posi_delta,
							    det_lft, det_rit, 0);

			if (tmp_final_posi[1] + top_bot_pre_cur_posi_delta < pre_final_posi[1])
				tmp_final_posi_new[1] = tmp_final_posi[1];
			else
				tmp_final_posi_new[1] =
					frc_bbd_line_stable(frc_devp, &stable_flag[1],
							    top_final_line1,
							    pre_final_posi[1],
							    top_bot_pre_cur_posi_delta,
							    det_top, det_bot, 1);

			if (tmp_final_posi[2] - lft_rit_pre_cur_posi_delta > pre_final_posi[2])
				tmp_final_posi_new[2] = tmp_final_posi[2];
			else
				tmp_final_posi_new[2] =
					frc_bbd_line_stable(frc_devp, &stable_flag[2],
							    rit_final_line1,
							    pre_final_posi[2],
							    lft_rit_pre_cur_posi_delta,
							    det_lft, det_rit, 2);

			if (tmp_final_posi[3] - top_bot_pre_cur_posi_delta > pre_final_posi[3])
				tmp_final_posi_new[3] = tmp_final_posi[3];
			else
				tmp_final_posi_new[3] =
					frc_bbd_line_stable(frc_devp, &stable_flag[3],
							    bot_final_line1,
							    pre_final_posi[3],
							    top_bot_pre_cur_posi_delta,
							    det_top, det_bot, 3);

			for (k = 0; k < search_final_line_para->final_iir_sel_rst_num - 1; k++) {
				lft_final_line1[k] = lft_final_line1[k + 1];
				top_final_line1[k] = top_final_line1[k + 1];
				rit_final_line1[k] = rit_final_line1[k + 1];
				bot_final_line1[k] = bot_final_line1[k + 1];
			}
		} else if (search_final_line_para->final_iir_mode == 0) {
			tmp_final_posi_new[0] = tmp_final_posi[0];
			tmp_final_posi_new[1] = tmp_final_posi[1];
			tmp_final_posi_new[2] = tmp_final_posi[2];
			tmp_final_posi_new[3] = tmp_final_posi[3];
		}

		// force window val
		if (search_final_line_para->force_final_posi_en == 1) {
			for (k = 0; k < 4; k++) {
				if (search_final_line_para->force_final_each_posi_en[k] == 1) {
					tmp_final_posi_new[k] =
					search_final_line_para->force_final_posi[k];
				}
			}
		}

		tmp_final_posi_new[0] = MIN(MAX(tmp_final_posi_new[0], det_lft), det_rit);
		tmp_final_posi_new[1] = MIN(MAX(tmp_final_posi_new[1], det_top), det_bot);
		tmp_final_posi_new[2] = MIN(MAX(tmp_final_posi_new[2], det_lft), det_rit);
		tmp_final_posi_new[3] = MIN(MAX(tmp_final_posi_new[3], det_top), det_bot);

		//set window
		for (k = 0; k < 4; k++) {
			search_final_line_para->caption[k]		= caption[k];
			search_final_line_para->caption_change[k]	= caption_change[k];
			search_final_line_para->pre_final_posi[k]	= tmp_final_posi_new[k];
			search_final_line_para->pre_motion_posi[k]	= motion_posi[k];

			logo_bb_xyxy[k]	 = tmp_final_posi_new[k];
			blackbar_xyxy[k]	 = tmp_final_posi_new[k];

			flat_xyxy[k]	 = tmp_final_posi_new[k];
			bb_motion_xyxy[k]	 = tmp_final_posi_new[k];
			oob_apl_xyxy[k]	 = tmp_final_posi_new[k];
			apl_hist_xyxy[k]	 = tmp_final_posi_new[k];
			oob_h_detail_xyxy[k] = tmp_final_posi_new[k];

			// lft and top
			if (k == 0 || k == 1) {
				blackbar_xyxy_me[k] = (bb_inner_en == 1) ?
				MIN(MAX(tmp_final_posi_new[k] + bb_xyxy_inner_ofst[k],
					clap_min[k]), clap_max[k]) : tmp_final_posi_new[k];
			} else if (k == 2 || k == 3) {
				// rit and bot
				blackbar_xyxy_me[k] = (bb_inner_en == 1) ?
				MIN(MAX(tmp_final_posi_new[k] - bb_xyxy_inner_ofst[k],
					clap_min[k]), clap_max[k]) : tmp_final_posi_new[k];
			}
		} // k

		//  LOGO
		UPDATE_FRC_REG_BITS(FRC_LOGO_BB_LFT_TOP, logo_bb_xyxy[0] << 16, 0xFFFF0000);
		UPDATE_FRC_REG_BITS(FRC_LOGO_BB_LFT_TOP, logo_bb_xyxy[1],     0x0000FFFF);
		UPDATE_FRC_REG_BITS(FRC_LOGO_BB_RIT_BOT, logo_bb_xyxy[2] << 16, 0xFFFF0000);
		UPDATE_FRC_REG_BITS(FRC_LOGO_BB_RIT_BOT, logo_bb_xyxy[3],     0x0000FFFF);

		//  FRC bbd
		UPDATE_FRC_REG_BITS(FRC_REG_BLACKBAR_XYXY_ST, blackbar_xyxy[0] << 16, 0xFFFF0000);
		UPDATE_FRC_REG_BITS(FRC_REG_BLACKBAR_XYXY_ST, blackbar_xyxy[1], 0x0000FFFF);
		UPDATE_FRC_REG_BITS(FRC_REG_BLACKBAR_XYXY_ED, blackbar_xyxy[2] << 16, 0xFFFF0000);
		UPDATE_FRC_REG_BITS(FRC_REG_BLACKBAR_XYXY_ED, blackbar_xyxy[3], 0x0000FFFF);

		//  FLAT
		UPDATE_FRC_REG_BITS(FRC_BBD_FLATNESS_DETEC_REGION_LFT_TOP, flat_xyxy[0] << 16,
				    0xFFFF0000);
		UPDATE_FRC_REG_BITS(FRC_BBD_FLATNESS_DETEC_REGION_LFT_TOP, flat_xyxy[1],
				    0x0000FFFF);
		UPDATE_FRC_REG_BITS(FRC_BBD_FLATNESS_DETEC_REGION_RIT_BOT, flat_xyxy[2] << 16,
				    0xFFFF0000);
		UPDATE_FRC_REG_BITS(FRC_BBD_FLATNESS_DETEC_REGION_RIT_BOT, flat_xyxy[3],
				    0x0000FFFF);

		//  MOTION
		UPDATE_FRC_REG_BITS(FRC_BBD_MOTION_DETEC_REGION_LFT_TOP_DS, bb_motion_xyxy[0] << 16,
				    0xFFFF0000);
		UPDATE_FRC_REG_BITS(FRC_BBD_MOTION_DETEC_REGION_LFT_TOP_DS, bb_motion_xyxy[1],
				    0x0000FFFF);
		UPDATE_FRC_REG_BITS(FRC_BBD_MOTION_DETEC_REGION_RIT_BOT_DS, bb_motion_xyxy[2] << 16,
				    0xFFFF0000);
		UPDATE_FRC_REG_BITS(FRC_BBD_MOTION_DETEC_REGION_RIT_BOT_DS, bb_motion_xyxy[3],
				    0x0000FFFF);

		//  APL
		UPDATE_FRC_REG_BITS(FRC_BBD_OOB_APL_CAL_LFT_TOP_RANGE, oob_apl_xyxy[0] << 16,
				    0xFFFF0000);
		UPDATE_FRC_REG_BITS(FRC_BBD_OOB_APL_CAL_LFT_TOP_RANGE, oob_apl_xyxy[1],
				    0x0000FFFF);
		UPDATE_FRC_REG_BITS(FRC_BBD_OOB_APL_CAL_RIT_BOT_RANGE, oob_apl_xyxy[2] << 16,
				    0xFFFF0000);
		UPDATE_FRC_REG_BITS(FRC_BBD_OOB_APL_CAL_RIT_BOT_RANGE, oob_apl_xyxy[3],
				    0x0000FFFF);

		//  HIST
		UPDATE_FRC_REG_BITS(FRC_BBD_APL_HIST_WIN_LFT_TOP, apl_hist_xyxy[0] << 16,
				    0xFFFF0000);
		UPDATE_FRC_REG_BITS(FRC_BBD_APL_HIST_WIN_LFT_TOP, apl_hist_xyxy[1], 0x0000FFFF);
		UPDATE_FRC_REG_BITS(FRC_BBD_APL_HIST_WIN_RIT_BOT, apl_hist_xyxy[2] << 16,
				    0xFFFF0000);
		UPDATE_FRC_REG_BITS(FRC_BBD_APL_HIST_WIN_RIT_BOT, apl_hist_xyxy[3], 0x0000FFFF);

		//  DETAIL
		UPDATE_FRC_REG_BITS(FRC_BBD_OOB_DETAIL_WIN_LFT_TOP,
				    oob_h_detail_xyxy[0] << 16, 0xFFFF0000);
		UPDATE_FRC_REG_BITS(FRC_BBD_OOB_DETAIL_WIN_LFT_TOP,
				    oob_h_detail_xyxy[1], 0x0000FFFF);
		UPDATE_FRC_REG_BITS(FRC_BBD_OOB_DETAIL_WIN_RIT_BOT,
				    oob_h_detail_xyxy[2] << 16, 0xFFFF0000);
		UPDATE_FRC_REG_BITS(FRC_BBD_OOB_DETAIL_WIN_RIT_BOT,
				    oob_h_detail_xyxy[3], 0x0000FFFF);

		//  BBD ME
		UPDATE_FRC_REG_BITS(FRC_REG_BLACKBAR_XYXY_ME_ST, blackbar_xyxy_me[0] << 16,
				    0xFFFF0000);
		UPDATE_FRC_REG_BITS(FRC_REG_BLACKBAR_XYXY_ME_ST, blackbar_xyxy_me[1],
				    0x0000FFFF);
		UPDATE_FRC_REG_BITS(FRC_REG_BLACKBAR_XYXY_ME_ED, blackbar_xyxy_me[2] << 16,
				    0xFFFF0000);
		UPDATE_FRC_REG_BITS(FRC_REG_BLACKBAR_XYXY_ME_ED, blackbar_xyxy_me[3], 0x0000FFFF);

		// for down sample img
		ds_xyxy[0]   = (search_final_line_para->ds_xyxy_force == 1) ?
				det_lft : tmp_final_posi_new[0];   //  lft
		ds_xyxy[1]   = (search_final_line_para->ds_xyxy_force == 1) ?
				det_top : tmp_final_posi_new[1];   //  top
		ds_xyxy[2]   = (search_final_line_para->ds_xyxy_force == 1) ?
				det_rit : tmp_final_posi_new[2];   //  rit
		ds_xyxy[3]   = (search_final_line_para->ds_xyxy_force == 1) ?
				det_bot : tmp_final_posi_new[3];   //  bot

		UPDATE_FRC_REG_BITS(FRC_REG_DS_WIN_SETTING_LFT_TOP, ds_xyxy[0] << 16, 0xFFFF0000);
		UPDATE_FRC_REG_BITS(FRC_REG_DS_WIN_SETTING_LFT_TOP, ds_xyxy[1],	  0x0000FFFF);
		UPDATE_FRC_REG_BITS(FRC_BBD_DS_WIN_SETTING_RIT_BOT, ds_xyxy[2] << 16, 0xFFFF0000);
		UPDATE_FRC_REG_BITS(FRC_BBD_DS_WIN_SETTING_RIT_BOT, ds_xyxy[3],	  0x0000FFFF);
	} // debug1

	// for motion downsample img
	motion_xyxy_ds[0] = (bb_motion_xyxy[0]) >> dsx;
	motion_xyxy_ds[1] = (bb_motion_xyxy[1]) >> dsy;
	motion_xyxy_ds[2] = ((bb_motion_xyxy[2]) >> dsx) + 1;
	motion_xyxy_ds[3] = ((bb_motion_xyxy[3]) >> dsy) + 1;

	UPDATE_FRC_REG_BITS(FRC_BBD_MOTION_DETEC_REGION_LFT_TOP_DS, motion_xyxy_ds[0] << 16,
			    0xFFFF0000);
	UPDATE_FRC_REG_BITS(FRC_BBD_MOTION_DETEC_REGION_LFT_TOP_DS, motion_xyxy_ds[1],
			    0x0000FFFF);
	UPDATE_FRC_REG_BITS(FRC_BBD_MOTION_DETEC_REGION_RIT_BOT_DS, motion_xyxy_ds[2] << 16,
			    0xFFFF0000);
	UPDATE_FRC_REG_BITS(FRC_BBD_MOTION_DETEC_REGION_RIT_BOT_DS, motion_xyxy_ds[3], 0x0000FFFF);
	//lft
	oob_v_detail_xyxy[0] = (dsx == 0) ? oob_h_detail_xyxy[0] : (oob_h_detail_xyxy[0] >> dsx);
	oob_v_detail_xyxy[1] = oob_h_detail_xyxy[1];
	//rit
	oob_v_detail_xyxy[2] = (dsx == 0) ? oob_h_detail_xyxy[2] : (oob_h_detail_xyxy[2] >> dsx);
	oob_v_detail_xyxy[3] = oob_h_detail_xyxy[3];

	UPDATE_FRC_REG_BITS(FRC_BBD_OOB_V_DETAIL_WIN_LFT_TOP, oob_v_detail_xyxy[0] << 16,
			    0xFFFF0000);
	UPDATE_FRC_REG_BITS(FRC_BBD_OOB_V_DETAIL_WIN_LFT_TOP, oob_v_detail_xyxy[1], 0x0000FFFF);
	UPDATE_FRC_REG_BITS(FRC_BBD_OOB_V_DETAIL_WIN_RIT_BOT, oob_v_detail_xyxy[2] << 16,
			    0xFFFF0000);
	UPDATE_FRC_REG_BITS(FRC_BBD_OOB_V_DETAIL_WIN_RIT_BOT, oob_v_detail_xyxy[3], 0x0000FFFF);

	black_th_pre = READ_FRC_REG(FRC_BBD_BLACK_TH_PRE_CUR) & 0x3FF;
	UPDATE_FRC_REG_BITS(FRC_BBD_BLACK_TH_PRE_CUR, black_th_pre << 16, 0x03FF0000);

	if (search_final_line_para->black_th_gen_en) {
		black_th = frc_bbd_black_th_gen(frc_devp, &apl_val[0]);
		UPDATE_FRC_REG_BITS(FRC_BBD_BLACK_TH_PRE_CUR, black_th, 0x000003FF);
		//xil_printf("bt%d\n\r",black_th);
	}

	if (search_final_line_para->edge_th_gen_en) {
		edge_th = frc_bbd_edge_th_gen(frc_devp, &apl_val[0]);
		UPDATE_FRC_REG_BITS(FRC_BBD_EDGE_DIFF, edge_th, 0x0000FFFF);
	}
}

u16 frc_bbd_choose_edge(struct frc_dev_s *frc_devp, u16 edge_posi2, u8 edge_posi_valid2,
				u16 edge_val2, u16 edge_first_posi,
				u8 edge_first_posi_valid, u16 edge_first_val, u16 edge_line_th)
{
	struct frc_fw_data_s *frc_data;
	struct st_search_final_line_para *search_final_line_para;

	frc_data = (struct frc_fw_data_s *)frc_devp->fw_data;
	search_final_line_para = &frc_data->search_final_line_para;

	if (search_final_line_para->edge_choose_mode == 0)
		return edge_posi2;

	if (edge_first_val > edge_line_th && edge_first_posi_valid &&
	    (ABS(edge_first_posi - edge_posi2) <=
	     search_final_line_para->edge_choose_delta)) {
		return edge_first_posi;
	} else {
		return edge_posi2;
	}
}

u16 frc_bbd_black_th_gen(struct frc_dev_s *frc_devp, u32 *apl_val)
{
	int hist_bld_val, apl_bld_val, black_th, final_bld_val;
	// int apl_lft = apl_val[0];
	//  int apl_top = apl_val[1];
	//  int apl_rit = apl_val[2];
	//  int apl_bot = apl_val[3];
	int apl_cen = apl_val[4];
	struct frc_fw_data_s *frc_data;
	struct st_search_final_line_para *search_final_line_para;
	struct st_bbd_rd *g_stbbd_rd;

	frc_data = (struct frc_fw_data_s *)frc_devp->fw_data;
	search_final_line_para = &frc_data->search_final_line_para;
	g_stbbd_rd = &frc_data->g_stbbd_rd;

	int hist_max1_val = (search_final_line_para->hist_scale_depth == 0) ?
			g_stbbd_rd->max1_hist_idx * 4 + 2 :
			(search_final_line_para->hist_scale_depth == 1) ?
			g_stbbd_rd->max1_hist_idx * 8 + 4 :
			(search_final_line_para->hist_scale_depth == 2) ?
			g_stbbd_rd->max1_hist_idx * 16 + 8 :
			g_stbbd_rd->max1_hist_idx * 32 + 16;

	int hist_max2_val = (search_final_line_para->hist_scale_depth == 0) ?
			g_stbbd_rd->max2_hist_idx * 4 + 2 :
			(search_final_line_para->hist_scale_depth == 1) ?
			g_stbbd_rd->max2_hist_idx * 8 + 4 :
			(search_final_line_para->hist_scale_depth == 2) ?
			g_stbbd_rd->max2_hist_idx * 16 + 8 :
			g_stbbd_rd->max2_hist_idx * 32 + 16;

	hist_bld_val = (hist_max1_val * g_stbbd_rd->max1_hist_cnt +
			hist_max2_val * g_stbbd_rd->max2_hist_cnt) /
			(g_stbbd_rd->max1_hist_cnt + g_stbbd_rd->max2_hist_cnt);

	apl_bld_val = ((apl_cen >> 1) + (apl_cen >> 2));

	final_bld_val = (search_final_line_para->black_th_gen_mode == 0) ?
			MAX(hist_bld_val,  apl_bld_val) :
			(search_final_line_para->black_th_gen_mode == 1) ?
			MIN(hist_bld_val,  apl_bld_val) :
			(hist_bld_val + apl_bld_val) / 2;

	black_th = MAX(MIN(search_final_line_para->black_th_max,  final_bld_val),
		       search_final_line_para->black_th_min);
	//black_th = 100;

	return black_th;
}

u16 frc_bbd_edge_th_gen(struct frc_dev_s *frc_devp, u32 *apl_val)
{
	u16 edge_th;
	// int apl_lft = apl_val[0];
	//  int apl_top = apl_val[1];
	//  int apl_rit = apl_val[2];
	//  int apl_bot = apl_val[3];
	u16 apl_cen = apl_val[4];
	struct frc_fw_data_s *frc_data;
	struct st_search_final_line_para *search_final_line_para;

	frc_data = (struct frc_fw_data_s *)frc_devp->fw_data;
	search_final_line_para = &frc_data->search_final_line_para;

	edge_th = MAX(MIN(search_final_line_para->edge_th_max, apl_cen >> 2),
		      search_final_line_para->edge_th_min);
	//black_th = 100;

	return edge_th;
}

u16 frc_bbd_line_stable(struct frc_dev_s *frc_devp, u8 *stable_flag, u16 *final_line1,
				u16 pre_final_posi, u16 pre_cur_posi_delta, u16 det_st,
				u16 det_ed, u8 line_idx)
{
	int new_line, kkk, jjj, flag, num_idx, sel_rst_num;
	int max_val, max_idx;
	int min_val, min_idx;
	int pre_val, pre_idx;
	int hist[2][24];//[0][i] save num [1][i] save number of num
	struct frc_fw_data_s *frc_data;
	struct st_search_final_line_para *search_final_line_para;

	frc_data = (struct frc_fw_data_s *)frc_devp->fw_data;
	search_final_line_para = &frc_data->search_final_line_para;

	sel_rst_num = search_final_line_para->final_iir_sel_rst_num;
	num_idx = 0;
	max_val = 0;
	max_idx = 0;
	min_val = sel_rst_num + 1;
	min_idx = 0;
	pre_val = 0;
	pre_idx = 0;

	// initial hist
	for (kkk = 0; kkk < 2; kkk++)
		for (jjj = 0; jjj < sel_rst_num; jjj++)
			hist[kkk][jjj] = 0;

	for (kkk = 0; kkk < sel_rst_num; kkk++) {
		flag = 0;
		for (jjj = 0; jjj < num_idx; jjj++) {
			if (final_line1[kkk] == hist[0][jjj]) {
				hist[1][jjj] += 1;
				flag = 1;
			}
		}

		if (flag == 0) {
			hist[0][num_idx] = final_line1[kkk];
			hist[1][num_idx] += 1;
			num_idx += 1;
		}
	}

	for (kkk = 0; kkk < num_idx; kkk++) {
		if (max_val < hist[1][kkk]) {
			max_val = hist[1][kkk];
			max_idx = kkk;
		}
		if (min_val > hist[1][kkk]) {
			min_val = hist[1][kkk];
			min_idx = kkk;
		}

		if (pre_final_posi ==	hist[0][kkk]) {
			pre_val = hist[1][kkk];
			pre_idx = kkk;
		}
	}

	if (search_final_line_para->final_iir_check_valid_mode == 0) {
		if (max_val > search_final_line_para->final_iir_num_th1) {
			new_line = hist[0][max_idx];
			stable_flag[0] = 1;
		} else {
			new_line = ((line_idx == 2) || (line_idx == 3)) ? det_ed : det_st;
			stable_flag[0] = 0;
		}
	} else {
		if  (pre_val > search_final_line_para->final_iir_num_th1) {
			new_line = pre_final_posi;
			stable_flag[0] = 1;
		} else if (pre_val > search_final_line_para->final_iir_num_th2) {
			new_line = pre_final_posi;
			stable_flag[0] = 0;
		} else if (max_val > search_final_line_para->final_iir_num_th1) {
			new_line = hist[0][max_idx];
			stable_flag[0] = 1;
		} else {
			if ((max_val + min_val == sel_rst_num) &&
			    (ABS(hist[0][max_idx] - hist[0][min_idx]) <= pre_cur_posi_delta))
				new_line = pre_final_posi;
			else
				new_line = ((line_idx == 2) || (line_idx == 3)) ? det_ed : det_st;
			stable_flag[0] = 0;
		}
	}

	new_line = MIN(MAX(new_line, det_st), det_ed);

	return new_line;
}

void frc_bbd_fw_confidence_cal(int tmp_final_posi, int pre_blackbar_xyxy,
						int bb_confidence, int bb_confidence_max)
{
	if (tmp_final_posi == pre_blackbar_xyxy) {
		bb_confidence += 1;
		bb_confidence = MIN(bb_confidence, bb_confidence_max);
	} else {
		bb_confidence = 0;
	}
}

u16 frc_bbd_fw_valid12_sel(u8  bb_valid2, u8  bb_valid1,
				u16 bb_posi2, u16 bb_posi1, u16 bb_motion,
				u16 det_init, u16 det_st, u16 det_ed,
				u8 bb_valid_ratio, u16 lsize, u8 reg_sel2_high_en,
				u8 motion_sel1_high_en, u8 *sel2_high_mode, u8 bb_idx)
{
	// when bb_idx = 0 or 1, means lft line or rit line, respectively;
	// otherwise, means rit or bot line.
	int tmp_final_posi = bb_posi2;

	// lft or top
	if (bb_idx == 0 || bb_idx == 1) {
		//  bb_posi2 is not good
		if (motion_sel1_high_en == 1) {
			if (bb_motion < bb_posi2)
				sel2_high_mode[0] = 0;
			else
				sel2_high_mode[0] = 1;
		} else {
			sel2_high_mode[0] = reg_sel2_high_en;
		}

		if (sel2_high_mode[0]) {
			if (bb_valid2 == 1 && (bb_posi2 < det_st + (lsize >> bb_valid_ratio))) {
				tmp_final_posi = bb_posi2;
			} else if ((bb_valid1 == 1) &&
				   (bb_posi1 < det_st + (lsize >> bb_valid_ratio))) {
				tmp_final_posi = bb_posi1;
			} else {
				tmp_final_posi = det_init;
			}
		} else {
			if (bb_valid1 == 1 && bb_posi1 < det_st + (lsize >> bb_valid_ratio)) {
				tmp_final_posi = bb_posi1;
			} else if ((bb_valid2 == 1) &&
				   (bb_posi2 < det_st + (lsize >> bb_valid_ratio))) {
				tmp_final_posi = bb_posi2;
			} else {
				tmp_final_posi = det_init;
			}
		}
	} else if ((bb_idx == 2) || (bb_idx == 3)) {
		// rit or bot
		if (motion_sel1_high_en == 1) {
			if (bb_motion > bb_posi2)
				sel2_high_mode[0] = 0;
			else
				sel2_high_mode[0] = 1;
		} else {
			sel2_high_mode[0] = reg_sel2_high_en;
		}

		if (sel2_high_mode[0]) {
			if (bb_valid2 == 1 && bb_posi2 <= det_ed &&
			    (bb_posi2 > (det_ed - (lsize >> bb_valid_ratio)))) {
				tmp_final_posi = bb_posi2;
			} else if (bb_valid1 == 1 && bb_posi1 <= det_ed &&
				   (bb_posi1 > (det_ed - (lsize >> bb_valid_ratio)))) {
				tmp_final_posi = bb_posi1;
			} else {
				tmp_final_posi = det_init;
			}
		} else {
			if (bb_valid1 == 1 && bb_posi1 <= det_ed &&
			    (bb_posi1 > (det_ed - (lsize >> bb_valid_ratio)))) {
				tmp_final_posi = bb_posi1;
			} else if (bb_valid2 == 1 && bb_posi2 <= det_ed &&
				   (bb_posi2 > (det_ed - (lsize >> bb_valid_ratio)))) {
				tmp_final_posi = bb_posi2;
			} else {
				tmp_final_posi = det_init;
			}
		}
	}

	return tmp_final_posi;
}

void frc_bbd_add_7_seg(void)
{
	u16 lft_line, top_line, rit_line, bot_line;
	u16 cnt, cnt10, cnt100, cnt1k;

	lft_line = READ_FRC_REG(FRC_REG_BLACKBAR_XYXY_ST) >> 16 & 0xFFFF;
	top_line = READ_FRC_REG(FRC_REG_BLACKBAR_XYXY_ST)     & 0xFFFF;
	rit_line = READ_FRC_REG(FRC_REG_BLACKBAR_XYXY_ED) >> 16 & 0xFFFF;
	bot_line = READ_FRC_REG(FRC_REG_BLACKBAR_XYXY_ED)     & 0xFFFF;

	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_POSI_AND_NUM11_NUM12, 1 << 15, BIT_15);//flag1_1_en
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_POSI_AND_NUM11_NUM12, 1 << 7, BIT_7);//flag1_2_en
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM13_NUM14_NUM15_NUM16, 1 << 31, BIT_31);//flag1_3_en
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM13_NUM14_NUM15_NUM16, 1 << 23, BIT_23);//flag1_4_en
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM13_NUM14_NUM15_NUM16, 1 << 15, BIT_15);//flag1_5_en
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM13_NUM14_NUM15_NUM16, 1 << 7, BIT_7);//flag1_6_en
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM17_NUM18_NUM21_NUM22, 1 << 31, BIT_31);//flag1_7_en
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM17_NUM18_NUM21_NUM22, 1 << 23, BIT_23);//flag1_8_en
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM17_NUM18_NUM21_NUM22, 1 << 15, BIT_15);//flag2_1_en
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM17_NUM18_NUM21_NUM22, 1 << 7, BIT_7);//flag2_2_en
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM23_NUM24_NUM25_NUM26, 1 << 31, BIT_31);//flag2_3_en
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM23_NUM24_NUM25_NUM26, 1 << 23, BIT_23);//flag2_4_en
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM23_NUM24_NUM25_NUM26, 1 << 15, BIT_15);//flag2_5_en
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM23_NUM24_NUM25_NUM26, 1 << 7, BIT_7);//flag2_6_en
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM27_NUM28, 1 << 31, BIT_31);//flag2_7_en
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM27_NUM28, 1 << 23, BIT_23);//flag2_8_en

	cnt	 = top_line;
	cnt1k	 = cnt / 1000;
	cnt	 = (cnt - cnt1k * 1000);
	cnt100	 = cnt / 100;
	cnt	 = (cnt - cnt100 * 100);
	cnt10	 = cnt / 10;
	cnt	 = (cnt - cnt10 * 10);

	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_POSI_AND_NUM11_NUM12, cnt1k << 8, 0xF00);
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_POSI_AND_NUM11_NUM12, cnt100, 0xF);
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM13_NUM14_NUM15_NUM16, cnt10 << 24, 0xF000000);
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM13_NUM14_NUM15_NUM16, cnt << 16, 0xF0000);
	//  color
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_POSI_AND_NUM11_NUM12, 3 << 12, 0x7000);
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_POSI_AND_NUM11_NUM12, 3 << 4, 0x70);
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM13_NUM14_NUM15_NUM16, 3 << 28, 0x70000000);
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM13_NUM14_NUM15_NUM16, 3 << 20, 0x700000);

	cnt	 = bot_line;
	cnt1k	 = cnt / 1000;
	cnt	 = (cnt - cnt1k * 1000);
	cnt100  = cnt / 100;
	cnt	 = (cnt - cnt100 * 100);
	cnt10	 = cnt / 10;
	cnt	 = (cnt - cnt10 * 10);

	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM13_NUM14_NUM15_NUM16, cnt1k << 8, 0xF00);
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM13_NUM14_NUM15_NUM16, cnt100, 0xF);
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM17_NUM18_NUM21_NUM22, cnt10 << 24, 0xF000000);
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM17_NUM18_NUM21_NUM22, cnt << 16, 0xF0000);
	//  color
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM13_NUM14_NUM15_NUM16, 3 << 12, 0x7000);
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM13_NUM14_NUM15_NUM16, 3 << 4, 0x70);
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM17_NUM18_NUM21_NUM22, 3 << 28, 0x70000000);
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM17_NUM18_NUM21_NUM22, 3 << 20, 0x700000);

	cnt	 = lft_line;
	cnt1k	 = cnt / 1000;
	cnt	 = (cnt - cnt1k * 1000);
	cnt100  = cnt / 100;
	cnt	 = (cnt - cnt100 * 100);
	cnt10	 = cnt / 10;
	cnt	 = (cnt - cnt10 * 10);

	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM17_NUM18_NUM21_NUM22, cnt1k << 8, 0xF00);
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM17_NUM18_NUM21_NUM22, cnt100, 0xF);
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM23_NUM24_NUM25_NUM26, cnt10 << 24, 0xF000000);
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM23_NUM24_NUM25_NUM26, cnt << 16, 0xF0000);
	//  color
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM17_NUM18_NUM21_NUM22, 3 << 12, 0x7000);
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM17_NUM18_NUM21_NUM22, 3 << 4, 0x70);
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM23_NUM24_NUM25_NUM26, 3 << 28, 0x70000000);
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM23_NUM24_NUM25_NUM26, 3 << 20, 0x700000);

	cnt	 = rit_line;
	cnt1k	 = cnt / 1000;
	cnt	 = (cnt - cnt1k * 1000);
	cnt100  = cnt / 100;
	cnt	 = (cnt - cnt100 * 100);
	cnt10	 = cnt / 10;
	cnt	 = (cnt - cnt10 * 10);

	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM23_NUM24_NUM25_NUM26, cnt1k << 8, 0xF00);
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM23_NUM24_NUM25_NUM26, cnt100, 0xF);
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM27_NUM28, cnt10 << 24, 0xF000000);
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM27_NUM28, cnt << 16, 0xF0000);

	//  color
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM23_NUM24_NUM25_NUM26, 3 << 12, 0x7000);
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM23_NUM24_NUM25_NUM26, 3 << 4, 0x70);
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM27_NUM28, 3 << 28, 0x70000000);
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM27_NUM28, 3 << 20, 0x700000);
}

void frc_bbd_ctrl(struct frc_dev_s *frc_devp)
{
	struct frc_fw_data_s *fw_data;
	struct st_search_final_line_para *search_final_line_para;

	fw_data = (struct frc_fw_data_s *)frc_devp->fw_data;
	search_final_line_para = &fw_data->search_final_line_para;

	if (fw_data->search_final_line_para.bbd_en == 0)
		return;

	frc_bbd_choose_final_line(frc_devp);
	frc_bbd_window_ctrl();
	if (search_final_line_para->bbd_7_seg_en)
		frc_bbd_add_7_seg();
}

