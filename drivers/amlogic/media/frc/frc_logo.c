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
#include "frc_logo.h"

void frc_logo_param_init(struct frc_dev_s *frc_devp)
{
	struct frc_fw_data_s *frc_data;
	struct st_iplogo_ctrl_item *g_stiplogoctrl_item;
	struct st_iplogo_ctrl_para *g_stiplogoctrl_para;
	struct st_melogo_ctrl_para *g_stmelogoctrl_para;
	// mc image xsize, depend on the input image xsize
	u32 input_hsize = (frc_devp->in_sts.in_hsize);
	// mc image ysize, depend on the input image ysize
	u32 input_vsize = (frc_devp->in_sts.in_vsize);

	frc_data = (struct frc_fw_data_s *)frc_devp->fw_data;
	g_stiplogoctrl_item = &frc_data->g_stiplogoctrl_item;
	g_stiplogoctrl_para = &frc_data->g_stiplogoctrl_para;
	g_stmelogoctrl_para = &frc_data->g_stmelogoctrl_para;

	// iplogo ctrl
	g_stiplogoctrl_item->blk_dir4_clr_blk_rate		   = 220;
	g_stiplogoctrl_item->doc_index			   = 3;
	// mc image xsize, depend on the input image xsize
	g_stiplogoctrl_para->xsize				   = input_hsize;
	// mc image ysize, depend on the input image ysize
	g_stiplogoctrl_para->ysize				   = input_vsize;
	g_stiplogoctrl_para->logo_en				= 1;
	// u1, dft=0,0:not use me_gmv_invalid check
	g_stiplogoctrl_para->gmv_invalid_check_en		   = 0;
	// u1, enable gmv ctrl corr clr method
	g_stiplogoctrl_para->gmv_ctrl_corr_clr_en		   = 1;
	// u11,threshold of gmv for gmv ctrl corr clr method
	g_stiplogoctrl_para->gmv_ctrl_corr_clr_th		   = 220;
	// u11,threshold of gmv for gmv_plus_minus ctrl corr msize
	g_stiplogoctrl_para->gmv_ctrl_corr_clr_msize_coring    = 10;
	// u1, enable gmv_plus_minus ctrl corr clr msize
	g_stiplogoctrl_para->gmv_ctrl_corr_clr_msize_en	   = 1;
	g_stiplogoctrl_para->fw_iplogo_en			   = 1;
	// dft=0,for fw logo reset, threshold of small mv rate
	g_stiplogoctrl_para->scc_glb_clr_rate_th = 256;
	// u1,dft=0,  0: area corr dir4 clr blk rate disable  (logo weak setting)
	g_stiplogoctrl_para->area_ctrl_dir4_ratio_en	   = 0;
	// u1,dft=1,  0: area ctrl corr thres disable
	g_stiplogoctrl_para->area_ctrl_corr_th_en = 1;
	g_stiplogoctrl_para->area_th_ub			   = 30;// u8,dft=30, 40(logo weak setting)
	g_stiplogoctrl_para->area_th_mb			   = 30;// u8,dft=30, 20(logo weak setting)
	g_stiplogoctrl_para->area_th_lb			   = 10;// u8,dft=10, 10(logo weak setting)

	// melogo ctrl
	g_stmelogoctrl_para->fw_melogo_en = 1;
}

void frc_area_and_gmv_rate(struct frc_dev_s *frc_devp)
{
	struct frc_fw_data_s *frc_data;
	struct st_iplogo_ctrl_item *g_stiplogoctrl_item;
	struct st_iplogo_ctrl_para *g_stiplogoctrl_para;
	struct st_melogo_ctrl_para *g_stmelogoctrl_para;
	struct st_me_rd	*g_stme_rd;
	struct st_logo_detect_rd *g_stlogodetect_rd;

	frc_data = (struct frc_fw_data_s *)frc_devp->fw_data;
	g_stiplogoctrl_item = &frc_data->g_stiplogoctrl_item;
	g_stiplogoctrl_para = &frc_data->g_stiplogoctrl_para;
	g_stmelogoctrl_para = &frc_data->g_stmelogoctrl_para;
	g_stme_rd = &frc_data->g_stme_rd;
	g_stlogodetect_rd = &frc_data->g_stlogodetect_rd;

	u32 sum_pxl = g_stiplogoctrl_item->xsize_logo * g_stiplogoctrl_item->ysize_logo;
	u16  small_nbor_rate;
	u16  area_rate;
	// dft=0,for fw logo reset, threshold of small mv rate
	u16  scc_glb_clr_rate_th	  =   g_stiplogoctrl_para->scc_glb_clr_rate_th;
	// u1,dft=0,  0: area corr dir4 clr blk rate disable  (logo weak setting)
	u8   area_ctrl_dir4_ratio_en =   g_stiplogoctrl_para->area_ctrl_dir4_ratio_en;
	// u1,dft=1,  0: area ctrl corr thres disable
	u8 area_ctrl_corr_th_en = g_stiplogoctrl_para->area_ctrl_corr_th_en;
	u8 area_th_ub = g_stiplogoctrl_para->area_th_ub;// u8,dft=30, 40(logo weak setting)
	u8 area_th_mb = g_stiplogoctrl_para->area_th_mb;// u8,dft=30, 20(logo weak setting)
	u8 area_th_lb = g_stiplogoctrl_para->area_th_lb;// u8,dft=10, 10(logo weak setting)

	// rule2--gmv clr method
	if (g_stme_rd->gmv_total_sum == 0)
		small_nbor_rate = 256;
	else
		small_nbor_rate =
			(g_stme_rd->gmv_small_neighbor_cnt * 256) / g_stme_rd->gmv_total_sum;
	// bit 8-12
	UPDATE_FRC_REG_BITS(FRC_IPLOGO_GLB_REGION_CLR,
			    ((small_nbor_rate >= scc_glb_clr_rate_th) ? 31 : 0) << 8, 0x1F << 8);

	// rule3--iplogo area clr
	// calc area rate
	area_rate = g_stlogodetect_rd->iplogo_pxl_cnt * 256 / sum_pxl;
	//
	// corr clr level based on area rate
	//
	if (area_ctrl_corr_th_en == 1)
		g_stiplogoctrl_item->doc_index = (area_rate > area_th_ub)  ? 0 :
						((area_rate >= area_th_mb) ? 1 :
						((area_rate >= area_th_lb) ? 2 : 3));

	if (area_ctrl_dir4_ratio_en == 1)
		g_stiplogoctrl_item->blk_dir4_clr_blk_rate = (area_rate > area_th_ub)  ? 160 :
					     ((area_rate >= area_th_mb) ? 170 :
					     ((area_rate >= area_th_lb) ? 190 : 200));
}

static u8 gray_dif_th[8]	       =   {24,  22,  20,  18,	16,  15, 14, 0};	// u6
static u16 cur_edge_th[8]	       =   {290, 245, 200, 145, 100, 70, 40, 0};	// u10
static u16 iir_edge_th[8]	       =   {250, 205, 165, 125, 95, 68, 40, 0}; // u10
static u16 edge_dif_th[8]	       =   {420, 390, 360, 330, 300, 270, 250, 0};	// u10
// for dir4 corr for misdet clr (blk cost down)
static u8 blk_logodir4_corr_th[4][8]  =   { {130, 120, 110, 100, 90, 85, 80, 0},
				     {140, 130, 120, 110, 100, 90, 80, 0},
				     {150, 140, 130, 120, 110, 100, 90, 0},
				     {170, 160, 150, 140, 130, 120, 110, 0}};	// u8

static u8 blk_edgedir4_corr_th[4][8]  =   { {150, 140, 130, 120, 110, 100, 90,  0},
				     {160, 150, 140, 130, 120, 110, 100, 0},
				     {170, 160, 150, 140, 130, 120, 110, 0},
				     {180, 170, 160, 150, 140, 130, 120, 0}};	// u8

static u8 avg_gray_dif_th[8] =   {25, 24, 22, 20, 18, 16, 15, 0};		// u6

void frc_iplogo_ctrl_gain(struct frc_dev_s *frc_devp)
{
	struct frc_fw_data_s *frc_data;
	struct st_iplogo_ctrl_item *g_stiplogoctrl_item;
	//struct st_iplogo_ctrl_para *g_stiplogoctrl_para;
	struct st_me_rd	*g_stme_rd;

	frc_data = (struct frc_fw_data_s *)frc_devp->fw_data;
	g_stiplogoctrl_item = &frc_data->g_stiplogoctrl_item;
	//g_stiplogoctrl_para = &frc_data->g_stiplogoctrl_para;
	g_stme_rd = &frc_data->g_stme_rd;

	u8    j;
	u8    doc_index	= g_stiplogoctrl_item->doc_index;

	u16   fwd_final_gain = READ_FRC_REG(FRC_IPLOGO_FINAL_GAIN) >> 16 & 0x3FF;// u10, bit 16-25
	u16   bwd_final_gain = READ_FRC_REG(FRC_IPLOGO_FINAL_GAIN) >> 0 & 0x3FF;// u10, bit 0-9

	UPDATE_FRC_REG_BITS(FRC_IPLOGO_DIR4_RATIO_BLK_PARAM,
			    (g_stiplogoctrl_item->blk_dir4_clr_blk_rate * fwd_final_gain) >> 8,
			    0xFF); // bit 0-7

	for (j = 0; j < 8; j++) {
		// gray dif, bit 0-5
		UPDATE_FRC_REG_BITS(FRC_IPLOGO_PXLCLR_DIF_TH_0	+
				    j * (FRC_IPLOGO_PXLCLR_DIF_TH_1 - FRC_IPLOGO_PXLCLR_DIF_TH_0),
				    ((gray_dif_th[j] * fwd_final_gain) >> 8) << 0,  0x3F << 0);
		// edge dif, bit 16-25
		UPDATE_FRC_REG_BITS(FRC_IPLOGO_PXLCLR_DIF_TH_0	+
				    j * (FRC_IPLOGO_PXLCLR_DIF_TH_1 - FRC_IPLOGO_PXLCLR_DIF_TH_0),
				    ((edge_dif_th[j] * fwd_final_gain) >> 8) << 16, 0x3FF << 16);
		// iir edge, bit 0-9
		UPDATE_FRC_REG_BITS(FRC_IPLOGO_EDGE_STRENGTH_TH_0 +
				    j * (FRC_IPLOGO_EDGE_STRENGTH_TH_1 -
					 FRC_IPLOGO_EDGE_STRENGTH_TH_0),
				    ((iir_edge_th[j] * bwd_final_gain) >> 8) << 0,  0x3FF << 0);
		// cur edge, bit 16-25
		UPDATE_FRC_REG_BITS(FRC_IPLOGO_EDGE_STRENGTH_TH_0 +
				    j * (FRC_IPLOGO_EDGE_STRENGTH_TH_1 -
					 FRC_IPLOGO_EDGE_STRENGTH_TH_0),
				    ((cur_edge_th[j] * bwd_final_gain) >> 8) << 16, 0x3FF << 16);
		// edge dir4 corr clr th, bit 0-7
		UPDATE_FRC_REG_BITS(FRC_IPLOGO_CORR_CLR_TH0_0	+
				    j * (FRC_IPLOGO_CORR_CLR_TH0_1 - FRC_IPLOGO_CORR_CLR_TH0_0),
				    ((blk_edgedir4_corr_th[doc_index][j] *
				      fwd_final_gain) >> 8) << 0, 0xFF << 0);
		UPDATE_FRC_REG_BITS(FRC_IPLOGO_CORR_CLR_TH0_0	+
				    j * (FRC_IPLOGO_CORR_CLR_TH0_1 - FRC_IPLOGO_CORR_CLR_TH0_0),
				    ((blk_logodir4_corr_th[doc_index][j] * fwd_final_gain) >> 8) <<
				    8, 0xFF << 8);  // logo dir4 corr clr th, bit 8-15
		// blk disaper avg dif th, bit 0-5
		UPDATE_FRC_REG_BITS(FRC_IPLOGO_DISAPER_DIF_TH_0	+
				    j * (FRC_IPLOGO_DISAPER_DIF_TH_1 - FRC_IPLOGO_DISAPER_DIF_TH_0),
				    ((avg_gray_dif_th[j] * fwd_final_gain) >> 8) << 0, 0x3F << 0);
	}
}

void frc_iplogo_ctrl(struct frc_dev_s *frc_devp)
{
	struct frc_fw_data_s *frc_data;
	struct st_iplogo_ctrl_item *g_stiplogoctrl_item;
	struct st_iplogo_ctrl_para *g_stiplogoctrl_para;
	struct st_me_rd	*g_stme_rd;

	frc_data = (struct frc_fw_data_s *)frc_devp->fw_data;
	g_stiplogoctrl_item = &frc_data->g_stiplogoctrl_item;
	g_stiplogoctrl_para = &frc_data->g_stiplogoctrl_para;
	g_stme_rd = &frc_data->g_stme_rd;

	if (g_stiplogoctrl_para->logo_en == 0)
		return;

	u8      i;
	// me image size
	u8      dsx	     =	 READ_FRC_REG(FRC_REG_ME_HME_SCALE) >> 6  & 0x3; // bit 6-7
	u8      dsy	     =	 READ_FRC_REG(FRC_REG_ME_HME_SCALE) >> 4  & 0x3; // bit 4-5
	u16     xsize_me =	 (g_stiplogoctrl_para->xsize + (1 << dsx) - 1) >> dsx;
	u16     ysize_me =	 (g_stiplogoctrl_para->ysize + (1 << dsy) - 1) >> dsy;
	// ip logo image size
	u8      me_logo_dsx     =	 READ_FRC_REG(FRC_REG_BLK_SIZE_XY) >> 25  & 0x1; // bit 25
	u8      me_logo_dsy     =	 READ_FRC_REG(FRC_REG_BLK_SIZE_XY) >> 24  & 0x1; // bit 24

	g_stiplogoctrl_item->xsize_logo	 =   (xsize_me + (1 << me_logo_dsx) - 1) >> me_logo_dsx;
	g_stiplogoctrl_item->ysize_logo	 =   (ysize_me + (1 << me_logo_dsy) - 1) >> me_logo_dsy;

	// gmv info
	u8      me_mvx_div_mode = READ_FRC_REG(FRC_REG_BLK_SIZE_XY) >> 14  & 0x3; // bit 14-15
	u8      me_mvy_div_mode = READ_FRC_REG(FRC_REG_BLK_SIZE_XY) >> 12  & 0x3; // bit 12-13
	u16     gmv_h_1nd	     =	 g_stme_rd->gmvx_mux  >>  me_mvx_div_mode;
	u16     gmv_v_1nd	     =	 g_stme_rd->gmvy_mux  >>  me_mvy_div_mode;
	u16     gmv_max_1nd     =	 MAX(ABS(gmv_h_1nd), ABS(gmv_v_1nd));
	u16     gmv_h_2nd	     =	 g_stme_rd->gmvx_2nd  >>  me_mvx_div_mode;
	u16     gmv_v_2nd	     =	 g_stme_rd->gmvy_2nd  >>  me_mvy_div_mode;
	u16     gmv_h;
	u16     gmv_v;
	u16     gmv_max;

	//u8      logo_area_rate;
	//u8      logo_input_fid  = READ_FRC_REG(FRC_REG_PAT_POINTER) & 0xF; // bit 0-3
	//
	u16     region_mv_h_1nd[12];
	u16     region_mv_v_1nd[12];
	u16     region_mv_max_1nd[12];
	u16     region_mv_h_2nd[12];
	u16     region_mv_v_2nd[12];
	u16     region_mv_h[12];
	u16     region_mv_v[12];
	u16     region_mv_max[12];

	// if gmv is zero, then use 2nd
	if (gmv_max_1nd == 0) {
		gmv_h	 =   gmv_h_2nd;
		gmv_v	 =   gmv_v_2nd;
	} else {
		gmv_h	 =   gmv_h_1nd;
		gmv_v	 =   gmv_v_1nd;
	}
	gmv_max	 =   MAX(ABS(gmv_h), ABS(gmv_v));

	if (g_stiplogoctrl_para->fw_iplogo_en == 1) {
			// rule1,region 12 clear
		for (i = 0; i < 12; i++) {
			region_mv_h_1nd[i]   =   g_stme_rd->region_gmvx[i];
			region_mv_v_1nd[i]   =   g_stme_rd->region_gmvy[i];
			region_mv_max_1nd[i] =   MAX(ABS(region_mv_h_1nd[i]),
						     ABS(region_mv_v_1nd[i]));
			region_mv_h_2nd[i]   =   g_stme_rd->region_gmvx_2d[i];
			region_mv_v_2nd[i]   =   g_stme_rd->region_gmvy_2d[i];
			if (region_mv_max_1nd[i] == 0) {
				region_mv_h[i]   =   region_mv_h_2nd[i];
				region_mv_v[i]   =   region_mv_v_2nd[i];
			} else {
				region_mv_h[i]   =   region_mv_h_1nd[i];
				region_mv_v[i]   =   region_mv_v_1nd[i];
			}
			region_mv_max[i] =   MAX(ABS(region_mv_h[i]), ABS(region_mv_v[i]));

			// write reg
			if (i < 4)
				UPDATE_FRC_REG_BITS(FRC_IPLOGO_REGION_CLR_STEP0,
						    ((region_mv_max[i] == 0) ? 31 : 0) << (i * 8),
						    0x1F << (i * 8)); // u5
			else if (i < 8)
				UPDATE_FRC_REG_BITS(FRC_IPLOGO_REGION_CLR_STEP1,
						    ((region_mv_max[i] == 0) ? 31 : 0) <<
						    ((i - 4) * 8), 0x1F << ((i - 4) * 8)); // u5
			else
				UPDATE_FRC_REG_BITS(FRC_IPLOGO_REGION_CLR_STEP2,
						    ((region_mv_max[i] == 0) ? 31 : 0) <<
						    ((i - 8) * 8), 0x1F << ((i - 8) * 8)); // u5
		}

		// rule2--ip logo area clr
		// rule3--gmv clr method
		frc_area_and_gmv_rate(frc_devp);
		// rule2 && rule3 end

		// rule4--if gmv very big, then close corr clr module

		if (g_stiplogoctrl_para->gmv_ctrl_corr_clr_en == 1 &&
		    (g_stme_rd->gmv_mux_invalid == 0 ||
		     g_stiplogoctrl_para->gmv_invalid_check_en == 0)) {
			UPDATE_FRC_REG_BITS(FRC_IPLOGO_EN,
					    ((gmv_max <
					      g_stiplogoctrl_para->gmv_ctrl_corr_clr_th) ? 1 : 0) <<
					      7, BIT_7); // bit 7,blk_logodir4_corr_clr_en
			UPDATE_FRC_REG_BITS(FRC_IPLOGO_EN,
					    ((gmv_max <
					      g_stiplogoctrl_para->gmv_ctrl_corr_clr_th) ? 1 : 0) <<
					      6, BIT_6); // bit 6,blk_edgedir4_corr_clr_en
		}
		// rule4 end

		// rule5
		if (g_stiplogoctrl_para->gmv_ctrl_corr_clr_msize_en == 1 &&
		    (g_stme_rd->gmv_mux_invalid == 0 ||
		     g_stiplogoctrl_para->gmv_invalid_check_en == 0)) {
			if (ABS(gmv_h) <= g_stiplogoctrl_para->gmv_ctrl_corr_clr_msize_coring) {
				UPDATE_FRC_REG_BITS(FRC_IPLOGO_CORR_CLR_PARAM, 5 << 4, 0xF << 4);
				UPDATE_FRC_REG_BITS(FRC_IPLOGO_CORR_CLR_PARAM, 5 << 0, 0xF << 0);
			} else if (gmv_h < 0) {
				UPDATE_FRC_REG_BITS(FRC_IPLOGO_CORR_CLR_PARAM, 3 << 4, 0xF << 4);
				UPDATE_FRC_REG_BITS(FRC_IPLOGO_CORR_CLR_PARAM, 7 << 0, 0xF << 0);
			} else if (gmv_h > 0) {
				UPDATE_FRC_REG_BITS(FRC_IPLOGO_CORR_CLR_PARAM, 5 << 4, 0xF << 4);
				UPDATE_FRC_REG_BITS(FRC_IPLOGO_CORR_CLR_PARAM, 5 << 0, 0xF << 0);
			} else {
				UPDATE_FRC_REG_BITS(FRC_IPLOGO_CORR_CLR_PARAM, 3 << 4, 0xF << 4);
				UPDATE_FRC_REG_BITS(FRC_IPLOGO_CORR_CLR_PARAM, 7 << 0, 0xF << 0);
			}
		}

		frc_iplogo_ctrl_gain(frc_devp);
	}
	// fw iplogo end
}

void frc_melogo_ctrl(struct frc_dev_s *frc_devp)
{
	struct frc_fw_data_s *frc_data;
	struct st_melogo_ctrl_para *g_stmelogoctrl_para;

	frc_data = (struct frc_fw_data_s *)frc_devp->fw_data;
	g_stmelogoctrl_para = &frc_data->g_stmelogoctrl_para;

	u8      j;
	u16     region_mv_h_1nd[12];
	u16     region_mv_v_1nd[12];
	u16     region_mv_max_1nd[12];
	u16     region_mv_h_2nd[12];
	u16     region_mv_v_2nd[12];
	u16     region_mv_h[12];
	u16     region_mv_v[12];
	u16     region_mv_max[12];

	if (g_stmelogoctrl_para->fw_melogo_en == 1) {
		// rule1,region 12 clear
		for (j = 0; j < 12; j++) {
			region_mv_h_1nd[j]   =
			READ_FRC_REG(FRC_ME_STAT_GMV_RGN_0 + j * (FRC_ME_STAT_GMV_RGN_1 -
				     FRC_ME_STAT_GMV_RGN_0)) >> 16 & 0x1FFF;
			region_mv_v_1nd[j]   =
			READ_FRC_REG(FRC_ME_STAT_GMV_RGN_0 + j * (FRC_ME_STAT_GMV_RGN_1 -
				     FRC_ME_STAT_GMV_RGN_0)) >> 0  & 0xFFF;
			region_mv_max_1nd[j] =   MAX(ABS(region_mv_h_1nd[j]),
							 ABS(region_mv_v_1nd[j]));
			region_mv_h_2nd[j]   =
			READ_FRC_REG(FRC_ME_STAT_GMV_RGN_2ND_0 + j * (FRC_ME_STAT_GMV_RGN_2ND_1 -
				     FRC_ME_STAT_GMV_RGN_2ND_0)) >> 16 & 0x1FFF;
			region_mv_v_2nd[j]   =
			READ_FRC_REG(FRC_ME_STAT_GMV_RGN_2ND_0 + j * (FRC_ME_STAT_GMV_RGN_2ND_1 -
				     FRC_ME_STAT_GMV_RGN_2ND_0)) >> 0  & 0xFFF;

			if (region_mv_max_1nd[j] == 0) {
				region_mv_h[j]   =   region_mv_h_2nd[j];
				region_mv_v[j]   =   region_mv_v_2nd[j];
			} else {
				region_mv_h[j]   =   region_mv_h_1nd[j];
				region_mv_v[j]   =   region_mv_v_1nd[j];
			}
			region_mv_max[j] =   MAX(ABS(region_mv_h[j]), ABS(region_mv_v[j]));
			UPDATE_FRC_REG_BITS(FRC_MELOGO_REGION_CLR_STEP,
					    ((region_mv_max[j] == 0) ? 1 : 0) << j, 0x1 << j);
		}
	}
}


