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

void frc_mc_param_init(struct frc_dev_s *frc_devp)
{
	struct frc_fw_data_s *frc_data;
	struct st_search_range_dynamic_para *g_stsrch_rng_dym_para;
	struct st_pixel_lpf_para *g_stpixlpf_para;

	frc_data = (struct frc_fw_data_s *)frc_devp->fw_data;
	g_stsrch_rng_dym_para = &frc_data->g_stsrch_rng_dym_para;
	g_stpixlpf_para = &frc_data->g_stpixlpf_para;

	g_stsrch_rng_dym_para->srch_rng_mvy_th = 1000;
	g_stsrch_rng_dym_para->force_luma_srch_rng_en = 0;
	g_stsrch_rng_dym_para->force_chrm_srch_rng_en = 0;
	g_stsrch_rng_dym_para->srch_rng_mode_cnt = 0;
	g_stsrch_rng_dym_para->srch_rng_mode_h_th = 10;
	g_stsrch_rng_dym_para->srch_rng_mode_l_th = 5;

	g_stsrch_rng_dym_para->norm_mode_en =   1;
	g_stsrch_rng_dym_para->sing_up_en =   1;
	g_stsrch_rng_dym_para->sing_dn_en  =   1;
	g_stsrch_rng_dym_para->norm_asym_en =   0;
	g_stsrch_rng_dym_para->norm_vd2_en =   1;
	g_stsrch_rng_dym_para->sing_up_vd2_en       =   1;
	g_stsrch_rng_dym_para->sing_dn_vd2_en       =   1;
	g_stsrch_rng_dym_para->norm_vd2hd2_en       =   1;
	g_stsrch_rng_dym_para->sing_up_vd2hd2_en    =   1;
	g_stsrch_rng_dym_para->sing_dn_vd2hd2_en    =   1;
	g_stsrch_rng_dym_para->gred_mode_en	      =   1;

	g_stsrch_rng_dym_para->luma_norm_vect_th     =   24;
	g_stsrch_rng_dym_para->luma_sing_vect_min_th =   8;
	g_stsrch_rng_dym_para->luma_sing_vect_max_th =   40;
	g_stsrch_rng_dym_para->luma_asym_vect_th     =   40;
	g_stsrch_rng_dym_para->luma_gred_vect_th     =   40;

	g_stsrch_rng_dym_para->chrm_norm_vect_th     =   24;
	g_stsrch_rng_dym_para->chrm_sing_vect_min_th =   8;
	g_stsrch_rng_dym_para->chrm_sing_vect_max_th =   40;
	g_stsrch_rng_dym_para->chrm_asym_vect_th     =   40;
	g_stsrch_rng_dym_para->chrm_gred_vect_th     =   40;

	g_stpixlpf_para->osd_ctrl_pixlpf_en = 0;
	g_stpixlpf_para->osd_ctrl_pixlpf_th = 0;
	g_stpixlpf_para->detail_ctrl_pixlpf_en = 0;
	g_stpixlpf_para->detail_ctrl_pixlpf_th = 0;
	//29x8bit
}

void get_vector_range(struct frc_dev_s *frc_devp, s16 *vector_range)
{
	u8 kkk;
	u32 glb_pos_hi_y_dtl_cnt[22];
	u32 glb_neg_lo_y_dtl_cnt[22];
	u16 glb_pos_hi_y_th[22];

	struct frc_fw_data_s *frc_data;
	struct st_search_range_dynamic_para *g_stsrch_rng_dym_para;

	frc_data = (struct frc_fw_data_s *)frc_devp->fw_data;
	g_stsrch_rng_dym_para = &frc_data->g_stsrch_rng_dym_para;

	u8 me_dsx_scale = READ_FRC_REG(FRC_REG_ME_HME_SCALE) >> 6 & 0x3;

	for (kkk = 0; kkk < 22; kkk++) {
		glb_pos_hi_y_th[kkk] =
			READ_FRC_REG(FRC_ME_STAT_POS_HI_TH_0 +
				     (FRC_ME_STAT_POS_HI_TH_1 - FRC_ME_STAT_POS_HI_TH_0) * kkk) &
				     0x7FF;
		glb_pos_hi_y_dtl_cnt[kkk]   =
			READ_FRC_REG(FRC_ME_RO_POS_HI_Y_0 +
				     (FRC_ME_RO_POS_HI_Y_1 -  FRC_ME_RO_POS_HI_Y_0) * kkk) &
				     0x3FFFF;
		glb_neg_lo_y_dtl_cnt[kkk]   =
			READ_FRC_REG(FRC_ME_RO_NEG_LO_Y_0 +
				     (FRC_ME_RO_NEG_LO_Y_1 - FRC_ME_RO_NEG_LO_Y_0) * kkk) &
				     0x3FFFF;
	}

	for (kkk = 0; kkk < 22; kkk++) {
		if (glb_pos_hi_y_dtl_cnt[kkk] < g_stsrch_rng_dym_para->srch_rng_mvy_th) {
			vector_range[1] = glb_pos_hi_y_th[kkk] >> (2 - me_dsx_scale);
			break;
		}
	}

	for (kkk = 0; kkk < 22; kkk++) {
		if (glb_neg_lo_y_dtl_cnt[kkk] < g_stsrch_rng_dym_para->srch_rng_mvy_th) {
			vector_range[0] = -(glb_pos_hi_y_th[kkk] >> (2 - me_dsx_scale));
			break;
		}
	}
}

u8 get_mclbuf_mode(struct frc_dev_s *frc_devp, s16 *vector_range, u8 luma_chrm_mode)
{
	s16 min_vector = MIN(vector_range[0], -8);
	s16 max_vector = MAX(vector_range[1],  8);
	//  int abs_vector_diff = max_vector - min_vector;

	u8 is_luma_mode = luma_chrm_mode == 0;
	//    u8 is_chrm_mode = luma_chrm_mode == 1;

	u8 norm_vector_th, norm_vector_vd2_th, norm_vector_vd2hd2_th;
	u8 gred_vector_th, gred_vector_vd2_th, gred_vector_vd2hd2_th;
	u8 sing_vector_min_th, sing_vector_min_vd2_th, sing_vector_min_vd2hd2_th;
	u8 sing_vector_max_th, sing_vector_max_vd2_th, sing_vector_max_vd2hd2_th;
	u8 asym_vector_vd2_th;
	struct frc_fw_data_s *frc_data;
	struct st_search_range_dynamic_para *g_stsrch_rng_dym_para;

	frc_data = (struct frc_fw_data_s *)frc_devp->fw_data;
	g_stsrch_rng_dym_para = &frc_data->g_stsrch_rng_dym_para;

	if (is_luma_mode) {
		//  norm mode
		norm_vector_th = g_stsrch_rng_dym_para->luma_norm_vect_th;//40
		norm_vector_vd2_th  = norm_vector_th << 1;
		norm_vector_vd2hd2_th = norm_vector_th << 2;

		// single mode
		sing_vector_min_th = g_stsrch_rng_dym_para->luma_sing_vect_min_th;//24
		sing_vector_min_vd2_th = sing_vector_min_th << 1;
		sing_vector_min_vd2hd2_th = sing_vector_min_th << 2;

		sing_vector_max_th = g_stsrch_rng_dym_para->luma_sing_vect_max_th;//56
		sing_vector_max_vd2_th = sing_vector_max_th << 1;
		sing_vector_max_vd2hd2_th   = sing_vector_max_th << 2;

		// greedy mode
		gred_vector_th = g_stsrch_rng_dym_para->luma_gred_vect_th;//80
		gred_vector_vd2_th  = gred_vector_th << 1;
		gred_vector_vd2hd2_th = gred_vector_th << 2;

		// normal asym vd2 mode
		asym_vector_vd2_th  = g_stsrch_rng_dym_para->luma_asym_vect_th;//48
	} else {
		//  norm mode
		norm_vector_th  = g_stsrch_rng_dym_para->chrm_norm_vect_th;//24
		norm_vector_vd2_th = norm_vector_th << 1;
		norm_vector_vd2hd2_th = norm_vector_th << 2;

		// single mode
		sing_vector_min_th = g_stsrch_rng_dym_para->chrm_sing_vect_min_th;//8
		sing_vector_min_vd2_th      = sing_vector_min_th << 1;
		sing_vector_min_vd2hd2_th   = sing_vector_min_th << 2;

		sing_vector_max_th = g_stsrch_rng_dym_para->chrm_sing_vect_max_th;//48
		sing_vector_max_vd2_th = sing_vector_max_th << 1;
		sing_vector_max_vd2hd2_th = sing_vector_max_th << 2;

		// greedy mode
		gred_vector_th = g_stsrch_rng_dym_para->chrm_gred_vect_th;//48
		gred_vector_vd2_th = gred_vector_th << 1;
		gred_vector_vd2hd2_th = gred_vector_th << 2;

		// normal asym vd2 mode
		asym_vector_vd2_th	     = g_stsrch_rng_dym_para->chrm_asym_vect_th;//32
	}

	if (min_vector >= -norm_vector_th && max_vector <= norm_vector_th &&
	    g_stsrch_rng_dym_para->norm_mode_en)
		return 0;   //  normal
	else if (((min_vector <= -norm_vector_th) && (min_vector >= -sing_vector_max_th)) &&
		(max_vector <= sing_vector_min_th) && g_stsrch_rng_dym_para->sing_up_en)
		return 1;   //  single up
	else if ((min_vector >= -sing_vector_min_th) &&
		 ((max_vector <= sing_vector_max_th) && (max_vector > norm_vector_th)) &&
		 g_stsrch_rng_dym_para->sing_dn_en)
		return 2;   //  single dn
	else if ((min_vector >= -asym_vector_vd2_th) && (max_vector <= asym_vector_vd2_th) &&
		 g_stsrch_rng_dym_para->norm_asym_en)
		return 3;	 //  normal+vd2asym
	else if ((min_vector >= -norm_vector_vd2_th) && (max_vector <= norm_vector_vd2_th) &&
		 g_stsrch_rng_dym_para->norm_vd2_en)
		return 4;   //  normal + vd2
	else if (((min_vector <= -norm_vector_vd2_th) &&
		 (min_vector >= -sing_vector_max_vd2_th)) &&
		 (max_vector <= sing_vector_min_vd2_th) && g_stsrch_rng_dym_para->sing_up_vd2_en)
		return 5;   //  single dn + vd2
	else if (min_vector >= -sing_vector_min_vd2_th &&
		 max_vector <= sing_vector_max_vd2_th  &&
		 max_vector > norm_vector_vd2_th &&
		 g_stsrch_rng_dym_para->sing_dn_vd2_en)
		return 6;   //  single up + vd2
	else if (min_vector >= -norm_vector_vd2hd2_th &&
		 max_vector <= norm_vector_vd2hd2_th &&
		 g_stsrch_rng_dym_para->norm_vd2hd2_en)
		return 7;   //  normal + vd2hd2
	else if (min_vector <= -norm_vector_vd2hd2_th &&
		 min_vector >= -sing_vector_max_vd2hd2_th &&
		 max_vector <= sing_vector_min_vd2hd2_th &&
		 g_stsrch_rng_dym_para->sing_up_vd2hd2_en)
		return 8;   //  single dn + vd2hd2
	else if (min_vector >= -sing_vector_min_vd2hd2_th &&
		 max_vector <= sing_vector_max_vd2hd2_th &&
		 max_vector > norm_vector_vd2hd2_th &&
		 g_stsrch_rng_dym_para->sing_dn_vd2hd2_en)
		return 9;   //  single up + vd2hd2
	else if ((min_vector >= -gred_vector_vd2hd2_th) && (max_vector <= gred_vector_vd2hd2_th) &&
		 g_stsrch_rng_dym_para->gred_mode_en)
		return 10;	 //  greedy
	else
		return 0;
}

void dynamic_mclbuf_mode(struct frc_dev_s *frc_devp)
{
	s16 vector_range[2];
	u8 luma_srch_rng_mode, chrm_srch_rng_mode;
	struct frc_fw_data_s *frc_data;
	struct st_search_range_dynamic_para *g_stsrch_rng_dym_para;

	frc_data = (struct frc_fw_data_s *)frc_devp->fw_data;
	g_stsrch_rng_dym_para = &frc_data->g_stsrch_rng_dym_para;

	//  initial
	luma_srch_rng_mode = READ_FRC_REG(FRC_SRCH_RNG_MODE) >> 4 & 0xF;
	chrm_srch_rng_mode = READ_FRC_REG(FRC_SRCH_RNG_MODE)	 & 0xF;

	//  dynamic choose search range
	get_vector_range(frc_devp, &vector_range[0]);

	if (g_stsrch_rng_dym_para->force_luma_srch_rng_en == 0)
		luma_srch_rng_mode = get_mclbuf_mode(frc_devp, &vector_range[0], 0);

	if (g_stsrch_rng_dym_para->force_chrm_srch_rng_en == 0)
		chrm_srch_rng_mode = get_mclbuf_mode(frc_devp, &vector_range[0], 1);

	UPDATE_FRC_REG_BITS(FRC_SRCH_RNG_MODE, luma_srch_rng_mode << 4, 0xF0);
	UPDATE_FRC_REG_BITS(FRC_SRCH_RNG_MODE, chrm_srch_rng_mode, 0xF);
}

void pixlpf_ctrl(struct frc_dev_s *frc_devp)
{
	u32 osd_cnt, logo_osd_th;
	u32 high_detail_cnt, high_detail_th;
	struct frc_fw_data_s *frc_data;
	//struct st_search_range_dynamic_para *g_stSrchRngDym_Para;
	struct st_pixel_lpf_para *g_stpixlpf_para;
	struct st_mc_rd *g_stmc_rd;

	frc_data = (struct frc_fw_data_s *)frc_devp->fw_data;
	//g_stSrchRngDym_Para = &frc_data->g_stSrchRngDym_Para;
	g_stpixlpf_para = &frc_data->g_stpixlpf_para;
	g_stmc_rd = &frc_data->g_stmc_rd;

	osd_cnt = MAX(g_stmc_rd->pre_ip_logo_cnt, g_stmc_rd->pre_ip_logo_cnt);
	logo_osd_th = g_stpixlpf_para->osd_ctrl_pixlpf_th;

	high_detail_cnt = 0;
	high_detail_th = g_stpixlpf_para->detail_ctrl_pixlpf_th;

	if ((osd_cnt > logo_osd_th && g_stpixlpf_para->osd_ctrl_pixlpf_en) ||
	    (high_detail_cnt > high_detail_th && g_stpixlpf_para->detail_ctrl_pixlpf_en))
		UPDATE_FRC_REG_BITS(FRC_MC_PIXLPF_CTRL, 0 << 27, BIT_27);
	else
		UPDATE_FRC_REG_BITS(FRC_MC_PIXLPF_CTRL, 1 << 27, BIT_27);
}

void frc_mc_ctrl(struct frc_dev_s *frc_devp)
{
	pixlpf_ctrl(frc_devp);
	dynamic_mclbuf_mode(frc_devp);
}

