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
#include "frc_scene.h"
#include "frc_common.h"
#include "frc_drv.h"

void frc_scene_detect_input(struct frc_dev_s *frc_devp)
{
	u8 i = 0;
	struct st_frc_rd *g_stfrc_rd;
	struct st_logo_detect_rd *g_stlogodetect_rd;
	struct st_film_detect_rd *g_stfilmdetect_rd;
	struct st_bbd_rd *g_stbbd_rd;
	struct frc_fw_data_s *fw_data;

	fw_data = (struct frc_fw_data_s *)frc_devp->fw_data;
	g_stfrc_rd = &fw_data->g_stfrc_rd;
	g_stlogodetect_rd = &fw_data->g_stlogodetect_rd;
	g_stfilmdetect_rd = &fw_data->g_stfilmdetect_rd;
	g_stbbd_rd = &fw_data->g_stbbd_rd;

	//-------input readbacks---------
	g_stfrc_rd->input_fid = READ_FRC_REG(FRC_REG_FWD_FID) >> 24 & 0xF;	 //  Bit 27~24
	g_stfrc_rd->clear_vbuffer_en = READ_FRC_REG(FRC_REG_FILM_PHS_2) & 0x1;	 //  Bit 0

	//-------Film detect readbacks---------
	g_stfilmdetect_rd->glb_motion_all = READ_FRC_REG(FRC_FD_DIF_GL);
	g_stfilmdetect_rd->glb_motion_all_film = READ_FRC_REG(FRC_FD_DIF_GL_FILM);
	//Bit 0-19
	g_stfilmdetect_rd->glb_motion_all_count = READ_FRC_REG(FRC_FD_DIF_COUNT_GL) & 0xFFFFF;
	g_stfilmdetect_rd->glb_motion_all_film_count = READ_FRC_REG(FRC_FD_DIF_COUNT_GL_FILM);

	for (i = 0; i < 6; i++) {
		g_stfilmdetect_rd->region_motion_wind6[i] =
		READ_FRC_REG(FRC_FD_DIF_6WIND_0 + i * (FRC_FD_DIF_6WIND_1 - FRC_FD_DIF_6WIND_0));
		//Bit0-19
		g_stfilmdetect_rd->region_motion_wind6_count[i] =
		READ_FRC_REG(FRC_FD_DIF_COUNT_6WIND_0 +
			     i * (FRC_FD_DIF_COUNT_6WIND_1 - FRC_FD_DIF_COUNT_6WIND_0)) & 0xFFFFF;
	}

	for (i = 0; i < 12; i++) {
		g_stfilmdetect_rd->region1_motion_wind12[i] =
		READ_FRC_REG(FRC_FD_DIF_12WIND_0 + i * (FRC_FD_DIF_12WIND_1 - FRC_FD_DIF_12WIND_0));
		g_stfilmdetect_rd->region1_motion_wind12_count[i] =
		READ_FRC_REG(FRC_FD_DIF_COUNT_12WIND_0 +
			     i * (FRC_FD_DIF_COUNT_12WIND_1 - FRC_FD_DIF_COUNT_12WIND_0)) & 0xFFFFF;
		g_stfilmdetect_rd->region2_motion_wind12[i] =
		READ_FRC_REG(FRC_FD_DIF_12WIND2_0 +
			     i * (FRC_FD_DIF_12WIND2_1 - FRC_FD_DIF_12WIND2_0));
		g_stfilmdetect_rd->region2_motion_wind12_count[i] =
		READ_FRC_REG(FRC_FD_DIF_COUNT_12WIND2_0 +
			     i * (FRC_FD_DIF_COUNT_12WIND2_1 -
				  FRC_FD_DIF_COUNT_12WIND2_0)) & 0xFFFFF;
	}

	//------ bbd readbacks---------
	g_stbbd_rd->top_valid1	 =  READ_FRC_REG(FRC_BBD_RO_LINE_AND_BLOCK_VALID) >> 11 & 0x1;
	g_stbbd_rd->top_valid2	 =  READ_FRC_REG(FRC_BBD_RO_LINE_AND_BLOCK_VALID) >> 10 & 0x1;
	g_stbbd_rd->bot_valid1	 =  READ_FRC_REG(FRC_BBD_RO_LINE_AND_BLOCK_VALID) >> 9 & 0x1;
	g_stbbd_rd->bot_valid2	 =  READ_FRC_REG(FRC_BBD_RO_LINE_AND_BLOCK_VALID) >> 8 & 0x1;
	g_stbbd_rd->rough_lft_valid1 =  READ_FRC_REG(FRC_BBD_RO_LINE_AND_BLOCK_VALID) >> 7 & 0x1;
	g_stbbd_rd->rough_lft_valid2 =  READ_FRC_REG(FRC_BBD_RO_LINE_AND_BLOCK_VALID) >> 6 & 0x1;
	g_stbbd_rd->rough_rit_valid1 =  READ_FRC_REG(FRC_BBD_RO_LINE_AND_BLOCK_VALID) >> 5 & 0x1;
	g_stbbd_rd->rough_rit_valid2 =  READ_FRC_REG(FRC_BBD_RO_LINE_AND_BLOCK_VALID) >> 4 & 0x1;
	g_stbbd_rd->finer_lft_valid1 =  READ_FRC_REG(FRC_BBD_RO_LINE_AND_BLOCK_VALID) >> 3 & 0x1;
	g_stbbd_rd->finer_lft_valid2 =  READ_FRC_REG(FRC_BBD_RO_LINE_AND_BLOCK_VALID) >> 2 & 0x1;
	g_stbbd_rd->finer_rit_valid1 =  READ_FRC_REG(FRC_BBD_RO_LINE_AND_BLOCK_VALID) >> 1 & 0x1;
	g_stbbd_rd->finer_rit_valid2 =  READ_FRC_REG(FRC_BBD_RO_LINE_AND_BLOCK_VALID) & 0x1;

	g_stbbd_rd->top_posi1	 =  READ_FRC_REG(FRC_BBD_RO_TOP_POSI) >> 16 & 0xFFFF;
	g_stbbd_rd->top_posi2	 =  READ_FRC_REG(FRC_BBD_RO_TOP_POSI) & 0xFFFF; //Bit 0-15;

	g_stbbd_rd->top_cnt1	 =  READ_FRC_REG(FRC_BBD_RO_TOP_BRIGHT_NUM_CNT) >> 16 & 0xFFFF;
	g_stbbd_rd->top_cnt2	 =  READ_FRC_REG(FRC_BBD_RO_TOP_BRIGHT_NUM_CNT)	& 0xFFFF;

	g_stbbd_rd->bot_posi1	 =  READ_FRC_REG(FRC_BBD_RO_BOT_POSI) >> 16 & 0xFFFF;
	g_stbbd_rd->bot_posi2	 =  READ_FRC_REG(FRC_BBD_RO_BOT_POSI) & 0xFFFF;

	g_stbbd_rd->bot_cnt1	 =  READ_FRC_REG(FRC_BBD_RO_BOT_BRIGHT_NUM_CNT) >> 16 & 0xFFFF;
	g_stbbd_rd->bot_cnt2	 =  READ_FRC_REG(FRC_BBD_RO_BOT_BRIGHT_NUM_CNT)	& 0xFFFF;

	g_stbbd_rd->rough_lft_posi1  =  READ_FRC_REG(FRC_BBD_RO_ROUGH_LFT_POSI) >> 16 & 0xFFFF;
	g_stbbd_rd->rough_lft_posi2  =  READ_FRC_REG(FRC_BBD_RO_ROUGH_LFT_POSI) & 0xFFFF;

	g_stbbd_rd->rough_lft_cnt1 =
		READ_FRC_REG(FRC_BBD_RO_ROUGH_LFT_BRIGHT_NUM_CNT) >> 16 & 0xFFFF;  //Bit 16-31;
	g_stbbd_rd->rough_lft_cnt2 =
		READ_FRC_REG(FRC_BBD_RO_ROUGH_LFT_BRIGHT_NUM_CNT) & 0xFFFF;  //Bit 0-15;

	g_stbbd_rd->rough_rit_posi1 = READ_FRC_REG(FRC_BBD_RO_ROUGH_RIT_POSI) >> 16 & 0xFFFF;
	g_stbbd_rd->rough_rit_posi2 = READ_FRC_REG(FRC_BBD_RO_ROUGH_RIT_POSI) >> 0 & 0xFFFF;

	g_stbbd_rd->rough_rit_cnt1 =
		READ_FRC_REG(FRC_BBD_RO_ROUGH_RIT_BRIGHT_NUM_CNT) >> 16 & 0xFFFF;
	g_stbbd_rd->rough_rit_cnt2 =
		READ_FRC_REG(FRC_BBD_RO_ROUGH_RIT_BRIGHT_NUM_CNT) >> 0  & 0xFFFF;

	g_stbbd_rd->rough_lft_index1 = READ_FRC_REG(FRC_BBD_RO_ROUGH_INDEX) >> 24 & 0xFF;
	g_stbbd_rd->rough_lft_index2 = READ_FRC_REG(FRC_BBD_RO_ROUGH_INDEX) >> 16 & 0xFF;
	g_stbbd_rd->rough_rit_index1 = READ_FRC_REG(FRC_BBD_RO_ROUGH_INDEX) >> 8 & 0xFF;
	g_stbbd_rd->rough_rit_index2 = READ_FRC_REG(FRC_BBD_RO_ROUGH_INDEX) & 0xFF;

	g_stbbd_rd->finer_lft_posi1 = READ_FRC_REG(FRC_BBD_RO_LFT_POSI) >> 16 & 0xFFFF;
	g_stbbd_rd->finer_lft_posi2 = READ_FRC_REG(FRC_BBD_RO_LFT_POSI) >> 0 & 0xFFFF;

	g_stbbd_rd->finer_lft_cnt1 = READ_FRC_REG(FRC_BBD_RO_LFT_BRIGHT_NUM_CNT) >> 16 & 0xFFFF;
	g_stbbd_rd->finer_lft_cnt2 = READ_FRC_REG(FRC_BBD_RO_LFT_BRIGHT_NUM_CNT) >> 0 & 0xFFFF;

	g_stbbd_rd->finer_rit_posi1 = READ_FRC_REG(FRC_BBD_RO_RIT_POSI) >> 16 & 0xFFFF;
	g_stbbd_rd->finer_rit_posi2 = READ_FRC_REG(FRC_BBD_RO_RIT_POSI) >> 0 & 0xFFFF;

	g_stbbd_rd->finer_rit_cnt1 = READ_FRC_REG(FRC_BBD_RO_RIT_BRIGHT_NUM_CNT) >> 16 & 0xFFFF;
	g_stbbd_rd->finer_rit_cnt2 = READ_FRC_REG(FRC_BBD_RO_RIT_BRIGHT_NUM_CNT) >> 0 & 0xFFFF;

	g_stbbd_rd->oob_apl_top_sum  =  READ_FRC_REG(FRC_BBD_RO_OOB_APL_TOP_SUM);       //Bit 0-31;
	g_stbbd_rd->oob_apl_bot_sum  =  READ_FRC_REG(FRC_BBD_RO_OOB_APL_BOT_SUM);       //Bit 0-31;
	g_stbbd_rd->oob_apl_lft_sum  =  READ_FRC_REG(FRC_BBD_RO_OOB_APL_LFT_SUM);       //Bit 0-31;
	g_stbbd_rd->oob_apl_rit_sum  =  READ_FRC_REG(FRC_BBD_RO_OOB_APL_RIT_SUM);       //Bit 0-31;

	g_stbbd_rd->oob_h_detail_top_val  =  READ_FRC_REG(FRC_BBD_RO_OOB_H_DETAIL_TOP_VAL);
	g_stbbd_rd->oob_v_detail_top_val  =  READ_FRC_REG(FRC_BBD_RO_OOB_V_DETAIL_TOP_VAL);
	g_stbbd_rd->oob_h_detail_top_cnt  =  READ_FRC_REG(FRC_BBD_RO_OOB_H_DETAIL_TOP_CNT);
	g_stbbd_rd->oob_v_detail_top_cnt  =  READ_FRC_REG(FRC_BBD_RO_OOB_V_DETAIL_TOP_CNT);
	g_stbbd_rd->oob_h_detail_bot_val  =  READ_FRC_REG(FRC_BBD_RO_OOB_H_DETAIL_BOT_VAL);
	g_stbbd_rd->oob_v_detail_bot_val  =  READ_FRC_REG(FRC_BBD_RO_OOB_V_DETAIL_BOT_VAL);
	g_stbbd_rd->oob_h_detail_bot_cnt  =  READ_FRC_REG(FRC_BBD_RO_OOB_H_DETAIL_BOT_CNT);
	g_stbbd_rd->oob_v_detail_bot_cnt  =  READ_FRC_REG(FRC_BBD_RO_OOB_V_DETAIL_BOT_CNT);
	g_stbbd_rd->oob_h_detail_lft_val  =  READ_FRC_REG(FRC_BBD_RO_OOB_H_DETAIL_LFT_VAL);
	g_stbbd_rd->oob_v_detail_lft_val  =  READ_FRC_REG(FRC_BBD_RO_OOB_V_DETAIL_LFT_VAL);
	g_stbbd_rd->oob_h_detail_lft_cnt  =  READ_FRC_REG(FRC_BBD_RO_OOB_H_DETAIL_LFT_CNT);
	g_stbbd_rd->oob_v_detail_lft_cnt  =  READ_FRC_REG(FRC_BBD_RO_OOB_V_DETAIL_LFT_CNT);
	g_stbbd_rd->oob_h_detail_rit_val  =  READ_FRC_REG(FRC_BBD_RO_OOB_H_DETAIL_RIT_VAL);
	g_stbbd_rd->oob_v_detail_rit_val  =  READ_FRC_REG(FRC_BBD_RO_OOB_V_DETAIL_RIT_VAL);
	g_stbbd_rd->oob_h_detail_rit_cnt  =  READ_FRC_REG(FRC_BBD_RO_OOB_H_DETAIL_RIT_CNT);
	g_stbbd_rd->oob_v_detail_rit_cnt  =  READ_FRC_REG(FRC_BBD_RO_OOB_V_DETAIL_RIT_CNT);

	g_stbbd_rd->h_detail_glb_val      =  READ_FRC_REG(FRC_BBD_RO_H_DETAIL_GLB_VAL);  //Bit 0-31;
	g_stbbd_rd->v_detail_glb_val      =  READ_FRC_REG(FRC_BBD_RO_V_DETAIL_GLB_VAL);  //Bit 0-31;
	g_stbbd_rd->h_detail_glb_cnt      =  READ_FRC_REG(FRC_BBD_RO_H_DETAIL_GLB_CNT);  //Bit 0-31;
	g_stbbd_rd->v_detail_glb_cnt      =  READ_FRC_REG(FRC_BBD_RO_V_DETAIL_GLB_CNT);  //Bit 0-31;

	g_stbbd_rd->oob_motion_diff_top_val  =
		READ_FRC_REG(FRC_BBD_RO_REGION_MOTION_DIFF_VAL_TOP_BOT) >> 16 & 0xFFFF;
	g_stbbd_rd->oob_motion_diff_bot_val  =
		READ_FRC_REG(FRC_BBD_RO_REGION_MOTION_DIFF_VAL_TOP_BOT) & 0xFFFF;

	g_stbbd_rd->oob_motion_diff_lft_val  =
		READ_FRC_REG(FRC_BBD_RO_REGION_MOTION_DIFF_VAL_LFT_RIT) >> 16 & 0xFFFF;
	g_stbbd_rd->oob_motion_diff_rit_val  =
		READ_FRC_REG(FRC_BBD_RO_REGION_MOTION_DIFF_VAL_LFT_RIT) & 0xFFFF;

	g_stbbd_rd->flat_top_cnt = READ_FRC_REG(FRC_BBD_RO_FLATNESS_CNT_TOP_BOT) >> 16 & 0xFFFF;
	g_stbbd_rd->flat_bot_cnt = READ_FRC_REG(FRC_BBD_RO_FLATNESS_CNT_TOP_BOT) & 0xFFFF;

	g_stbbd_rd->flat_lft_cnt = READ_FRC_REG(FRC_BBD_RO_FLATNESS_CNT_LFT_RIT) >> 16 & 0xFFFF;
	g_stbbd_rd->flat_rit_cnt = READ_FRC_REG(FRC_BBD_RO_FLATNESS_CNT_LFT_RIT) & 0xFFFF;

	g_stbbd_rd->top_edge_posi_valid1 = READ_FRC_REG(FRC_BBD_RO_EDGE_POSI_VALID) >> 11 & 0x1;
	g_stbbd_rd->bot_edge_posi_valid1 = READ_FRC_REG(FRC_BBD_RO_EDGE_POSI_VALID) >> 10 & 0x1;
	g_stbbd_rd->top_edge_posi_valid2 = READ_FRC_REG(FRC_BBD_RO_EDGE_POSI_VALID) >> 9 & 0x1;
	g_stbbd_rd->bot_edge_posi_valid2 = READ_FRC_REG(FRC_BBD_RO_EDGE_POSI_VALID) >> 8 & 0x1;
	g_stbbd_rd->lft_edge_posi_valid1 = READ_FRC_REG(FRC_BBD_RO_EDGE_POSI_VALID) >> 7 & 0x1;
	g_stbbd_rd->rit_edge_posi_valid1 = READ_FRC_REG(FRC_BBD_RO_EDGE_POSI_VALID) >> 6 & 0x1;
	g_stbbd_rd->lft_edge_posi_valid2 = READ_FRC_REG(FRC_BBD_RO_EDGE_POSI_VALID) >> 5 & 0x1;
	g_stbbd_rd->rit_edge_posi_valid2 = READ_FRC_REG(FRC_BBD_RO_EDGE_POSI_VALID) >> 4 & 0x1;
	g_stbbd_rd->top_edge_first_posi_valid = READ_FRC_REG(FRC_BBD_RO_EDGE_POSI_VALID) >> 3 & 0x1;
	g_stbbd_rd->bot_edge_first_posi_valid = READ_FRC_REG(FRC_BBD_RO_EDGE_POSI_VALID) >> 2 & 0x1;
	g_stbbd_rd->lft_edge_first_posi_valid = READ_FRC_REG(FRC_BBD_RO_EDGE_POSI_VALID) >> 1 & 0x1;
	g_stbbd_rd->rit_edge_first_posi_valid = READ_FRC_REG(FRC_BBD_RO_EDGE_POSI_VALID) >> 0 & 0x1;

	g_stbbd_rd->top_edge_posi1	 =  READ_FRC_REG(FRC_BBD_RO_TOP_EDGE_POSI) >> 16 & 0xFFFF;
	g_stbbd_rd->top_edge_posi2	 =  READ_FRC_REG(FRC_BBD_RO_TOP_EDGE_POSI) >> 0  & 0xFFFF;

	g_stbbd_rd->top_edge_val1	 =  READ_FRC_REG(FRC_BBD_RO_TOP_EDGE_VAL)  >> 16 & 0xFFFF;
	g_stbbd_rd->top_edge_val2	 =  READ_FRC_REG(FRC_BBD_RO_TOP_EDGE_VAL)  >> 0  & 0xFFFF;

	g_stbbd_rd->bot_edge_posi1	 =  READ_FRC_REG(FRC_BBD_RO_BOT_EDGE_POSI) >> 16 & 0xFFFF;
	g_stbbd_rd->bot_edge_posi2	 =  READ_FRC_REG(FRC_BBD_RO_BOT_EDGE_POSI) >> 0  & 0xFFFF;

	g_stbbd_rd->bot_edge_val1	 =  READ_FRC_REG(FRC_BBD_RO_BOT_EDGE_VAL)  >> 16 & 0xFFFF;
	g_stbbd_rd->bot_edge_val2	 =  READ_FRC_REG(FRC_BBD_RO_BOT_EDGE_VAL)  >> 0  & 0xFFFF;

	g_stbbd_rd->lft_edge_posi1	 =  READ_FRC_REG(FRC_BBD_RO_LFT_EDGE_POSI) >> 16 & 0xFFFF;
	g_stbbd_rd->lft_edge_posi2	 =  READ_FRC_REG(FRC_BBD_RO_LFT_EDGE_POSI) >> 0  & 0xFFFF;

	g_stbbd_rd->lft_edge_val1	 =  READ_FRC_REG(FRC_BBD_RO_LFT_EDGE_VAL)  >> 16 & 0xFFFF;
	g_stbbd_rd->lft_edge_val2	 =  READ_FRC_REG(FRC_BBD_RO_LFT_EDGE_VAL)  >> 0  & 0xFFFF;

	g_stbbd_rd->rit_edge_posi1	 =  READ_FRC_REG(FRC_BBD_RO_RIT_EDGE_POSI) >> 16 & 0xFFFF;
	g_stbbd_rd->rit_edge_posi2	 =  READ_FRC_REG(FRC_BBD_RO_RIT_EDGE_POSI) >> 0  & 0xFFFF;

	g_stbbd_rd->rit_edge_val1	 =  READ_FRC_REG(FRC_BBD_RO_RIT_EDGE_VAL)  >> 16 & 0xFFFF;
	g_stbbd_rd->rit_edge_val2	 =  READ_FRC_REG(FRC_BBD_RO_RIT_EDGE_VAL)  >> 0  & 0xFFFF;

	g_stbbd_rd->top_edge_first_posi   = READ_FRC_REG(FRC_BBD_RO_TOP_EDGE_FIRST)  >> 16 & 0xFFFF;
	g_stbbd_rd->top_edge_first_val    = READ_FRC_REG(FRC_BBD_RO_TOP_EDGE_FIRST)  >> 0  & 0xFFFF;

	g_stbbd_rd->bot_edge_first_posi   = READ_FRC_REG(FRC_BBD_RO_BOT_EDGE_FIRST)  >> 16 & 0xFFFF;
	g_stbbd_rd->bot_edge_first_val    = READ_FRC_REG(FRC_BBD_RO_BOT_EDGE_FIRST)  >> 0  & 0xFFFF;

	g_stbbd_rd->lft_edge_first_posi   = READ_FRC_REG(FRC_BBD_RO_LFT_EDGE_FIRST)  >> 16 & 0xFFFF;
	g_stbbd_rd->lft_edge_first_val    = READ_FRC_REG(FRC_BBD_RO_LFT_EDGE_FIRST)  >> 0  & 0xFFFF;

	g_stbbd_rd->rit_edge_first_posi   = READ_FRC_REG(FRC_BBD_RO_RIT_EDGE_FIRST)  >> 16 & 0xFFFF;
	g_stbbd_rd->rit_edge_first_val    = READ_FRC_REG(FRC_BBD_RO_RIT_EDGE_FIRST)  >> 0  & 0xFFFF;

	g_stbbd_rd->rough_lft_motion_posi_valid1 =
		READ_FRC_REG(FRC_BBD_RO_MOTION_POSI_VALID) >> 11 & 0x1;
	g_stbbd_rd->rough_lft_motion_posi_valid2 =
		READ_FRC_REG(FRC_BBD_RO_MOTION_POSI_VALID) >> 10 & 0x1;
	g_stbbd_rd->rough_rit_motion_posi_valid1 =
		READ_FRC_REG(FRC_BBD_RO_MOTION_POSI_VALID) >> 9  & 0x1;
	g_stbbd_rd->rough_rit_motion_posi_valid2 =
		READ_FRC_REG(FRC_BBD_RO_MOTION_POSI_VALID) >> 8  & 0x1;
	g_stbbd_rd->top_motion_posi_valid1 = READ_FRC_REG(FRC_BBD_RO_MOTION_POSI_VALID) >> 7  & 0x1;
	g_stbbd_rd->bot_motion_posi_valid1 = READ_FRC_REG(FRC_BBD_RO_MOTION_POSI_VALID) >> 6  & 0x1;
	g_stbbd_rd->top_motion_posi_valid2 = READ_FRC_REG(FRC_BBD_RO_MOTION_POSI_VALID) >> 5  & 0x1;
	g_stbbd_rd->bot_motion_posi_valid2 = READ_FRC_REG(FRC_BBD_RO_MOTION_POSI_VALID) >> 4  & 0x1;
	g_stbbd_rd->lft_motion_posi_valid1 = READ_FRC_REG(FRC_BBD_RO_MOTION_POSI_VALID) >> 3  & 0x1;
	g_stbbd_rd->rit_motion_posi_valid1 = READ_FRC_REG(FRC_BBD_RO_MOTION_POSI_VALID) >> 2  & 0x1;
	g_stbbd_rd->lft_motion_posi_valid2 = READ_FRC_REG(FRC_BBD_RO_MOTION_POSI_VALID) >> 1  & 0x1;
	g_stbbd_rd->rit_motion_posi_valid2 = READ_FRC_REG(FRC_BBD_RO_MOTION_POSI_VALID) >> 0  & 0x1;

	g_stbbd_rd->finer_lft_motion_th1 =
		READ_FRC_REG(FRC_BBD_RO_FINER_LFT_MOTION_TH)  >> 16 & 0xFFF;
	g_stbbd_rd->finer_lft_motion_th2 =
		READ_FRC_REG(FRC_BBD_RO_FINER_LFT_MOTION_TH)  >> 0  & 0xFFF;

	g_stbbd_rd->finer_rit_motion_th1 =
		READ_FRC_REG(FRC_BBD_RO_FINER_RIT_MOTION_TH)  >> 16 & 0xFFF;
	g_stbbd_rd->finer_rit_motion_th2 =
		READ_FRC_REG(FRC_BBD_RO_FINER_RIT_MOTION_TH)  >> 0  & 0xFFF;

	g_stbbd_rd->top_motion_posi1 = READ_FRC_REG(FRC_BBD_RO_TOP_MOTION_POSI)  >> 16 & 0xFFFF;
	g_stbbd_rd->top_motion_posi2 = READ_FRC_REG(FRC_BBD_RO_TOP_MOTION_POSI)  >> 0  & 0xFFFF;

	g_stbbd_rd->top_motion_cnt1 = READ_FRC_REG(FRC_BBD_RO_TOP_MOTION_CNT)	 >> 16 & 0xFFFF;
	g_stbbd_rd->top_motion_cnt2 = READ_FRC_REG(FRC_BBD_RO_TOP_MOTION_CNT)	 >> 0  & 0xFFFF;

	g_stbbd_rd->bot_motion_posi1 = READ_FRC_REG(FRC_BBD_RO_BOT_MOTION_POSI)  >> 16 & 0xFFFF;
	g_stbbd_rd->bot_motion_posi2 = READ_FRC_REG(FRC_BBD_RO_BOT_MOTION_POSI)  >> 0  & 0xFFFF;

	g_stbbd_rd->bot_motion_cnt1 = READ_FRC_REG(FRC_BBD_RO_BOT_MOTION_CNT)	 >> 16 & 0xFFFF;
	g_stbbd_rd->bot_motion_cnt2 = READ_FRC_REG(FRC_BBD_RO_BOT_MOTION_CNT)	 >> 0  & 0xFFFF;

	g_stbbd_rd->rough_lft_motion_index1 =
		READ_FRC_REG(FRC_BBD_RO_ROUGH_MOTION_POSI)  >> 24 & 0xFF;
	g_stbbd_rd->rough_lft_motion_index2 =
		READ_FRC_REG(FRC_BBD_RO_ROUGH_MOTION_POSI)  >> 16 & 0xFF;
	g_stbbd_rd->rough_rit_motion_index1 =
		READ_FRC_REG(FRC_BBD_RO_ROUGH_MOTION_POSI)  >> 8  & 0xFF;
	g_stbbd_rd->rough_rit_motion_index2 =
		READ_FRC_REG(FRC_BBD_RO_ROUGH_MOTION_POSI)  >> 0  & 0xFF;

	g_stbbd_rd->rough_lft_motion_posi1 =
		READ_FRC_REG(FRC_BBD_RO_ROUGH_LFT_MOTION_POSI)  >> 16 & 0xFFFF;
	g_stbbd_rd->rough_lft_motion_posi2 =
		READ_FRC_REG(FRC_BBD_RO_ROUGH_LFT_MOTION_POSI)  >> 0  & 0xFFFF;

	g_stbbd_rd->rough_rit_motion_posi1 =
		READ_FRC_REG(FRC_BBD_RO_ROUGH_RIT_MOTION_POSI)  >> 16 & 0xFFFF;
	g_stbbd_rd->rough_rit_motion_posi2 =
		READ_FRC_REG(FRC_BBD_RO_ROUGH_RIT_MOTION_POSI)  >> 0  & 0xFFFF;

	g_stbbd_rd->rough_lft_motion_cnt1 =
		READ_FRC_REG(FRC_BBD_RO_ROUGH_LFT_MOTION_CNT)  >> 16 & 0xFFFF;
	g_stbbd_rd->rough_lft_motion_cnt2 =
		READ_FRC_REG(FRC_BBD_RO_ROUGH_LFT_MOTION_CNT)  >> 0  & 0xFFFF;

	g_stbbd_rd->rough_rit_motion_cnt1 =
		READ_FRC_REG(FRC_BBD_RO_ROUGH_RIT_MOTION_CNT)  >> 16 & 0xFFFF;
	g_stbbd_rd->rough_rit_motion_cnt2 =
		READ_FRC_REG(FRC_BBD_RO_ROUGH_RIT_MOTION_CNT)  >> 0  & 0xFFFF;

	g_stbbd_rd->lft_motion_posi1   =  READ_FRC_REG(FRC_BBD_RO_LFT_MOTION_POSI)  >> 16 & 0xFFFF;
	g_stbbd_rd->lft_motion_posi2   =  READ_FRC_REG(FRC_BBD_RO_LFT_MOTION_POSI)  >> 0  & 0xFFFF;

	g_stbbd_rd->lft_motion_cnt1    =  READ_FRC_REG(FRC_BBD_RO_LFT_MOTION_CNT)   >> 16 & 0xFFFF;
	g_stbbd_rd->lft_motion_cnt2    =  READ_FRC_REG(FRC_BBD_RO_LFT_MOTION_CNT)   >> 0  & 0xFFFF;

	g_stbbd_rd->rit_motion_posi1   =  READ_FRC_REG(FRC_BBD_RO_RIT_MOTION_POSI)  >> 16 & 0xFFFF;
	g_stbbd_rd->rit_motion_posi2   =  READ_FRC_REG(FRC_BBD_RO_RIT_MOTION_POSI)  >> 0  & 0xFFFF;

	g_stbbd_rd->rit_motion_cnt1    =  READ_FRC_REG(FRC_BBD_RO_RIT_MOTION_CNT)   >> 16 & 0xFFFF;
	g_stbbd_rd->rit_motion_cnt2    =  READ_FRC_REG(FRC_BBD_RO_RIT_MOTION_CNT)   >> 0  & 0xFFFF;

	g_stbbd_rd->finer_hist_data_0   =  READ_FRC_REG(FRC_BBD_RO_FINER_HIST_DATA_0);   //Bit 0-31;
	g_stbbd_rd->finer_hist_data_1   =  READ_FRC_REG(FRC_BBD_RO_FINER_HIST_DATA_1);   //Bit 0-31;
	g_stbbd_rd->finer_hist_data_2   =  READ_FRC_REG(FRC_BBD_RO_FINER_HIST_DATA_2);   //Bit 0-31;
	g_stbbd_rd->finer_hist_data_3   =  READ_FRC_REG(FRC_BBD_RO_FINER_HIST_DATA_3);   //Bit 0-31;
	g_stbbd_rd->finer_hist_data_4   =  READ_FRC_REG(FRC_BBD_RO_FINER_HIST_DATA_4);   //Bit 0-31;
	g_stbbd_rd->finer_hist_data_5   =  READ_FRC_REG(FRC_BBD_RO_FINER_HIST_DATA_5);   //Bit 0-31;
	g_stbbd_rd->finer_hist_data_6   =  READ_FRC_REG(FRC_BBD_RO_FINER_HIST_DATA_6);   //Bit 0-31;
	g_stbbd_rd->finer_hist_data_7   =  READ_FRC_REG(FRC_BBD_RO_FINER_HIST_DATA_7);   //Bit 0-31;
	g_stbbd_rd->finer_hist_data_8   =  READ_FRC_REG(FRC_BBD_RO_FINER_HIST_DATA_8);   //Bit 0-31;
	g_stbbd_rd->finer_hist_data_9   =  READ_FRC_REG(FRC_BBD_RO_FINER_HIST_DATA_9);   //Bit 0-31;
	g_stbbd_rd->finer_hist_data_10  =  READ_FRC_REG(FRC_BBD_RO_FINER_HIST_DATA_10);  //Bit 0-31;
	g_stbbd_rd->finer_hist_data_11  =  READ_FRC_REG(FRC_BBD_RO_FINER_HIST_DATA_11);  //Bit 0-31;
	g_stbbd_rd->finer_hist_data_12  =  READ_FRC_REG(FRC_BBD_RO_FINER_HIST_DATA_12);  //Bit 0-31;
	g_stbbd_rd->finer_hist_data_13  =  READ_FRC_REG(FRC_BBD_RO_FINER_HIST_DATA_13);  //Bit 0-31;
	g_stbbd_rd->finer_hist_data_14  =  READ_FRC_REG(FRC_BBD_RO_FINER_HIST_DATA_14);  //Bit 0-31;
	g_stbbd_rd->finer_hist_data_15  =  READ_FRC_REG(FRC_BBD_RO_FINER_HIST_DATA_15);  //Bit 0-31;
	g_stbbd_rd->finer_hist_data_16  =  READ_FRC_REG(FRC_BBD_RO_FINER_HIST_DATA_16);  //Bit 0-31;
	g_stbbd_rd->finer_hist_data_17  =  READ_FRC_REG(FRC_BBD_RO_FINER_HIST_DATA_17);  //Bit 0-31;
	g_stbbd_rd->finer_hist_data_18  =  READ_FRC_REG(FRC_BBD_RO_FINER_HIST_DATA_18);  //Bit 0-31;
	g_stbbd_rd->finer_hist_data_19  =  READ_FRC_REG(FRC_BBD_RO_FINER_HIST_DATA_19);  //Bit 0-31;
	g_stbbd_rd->finer_hist_data_20  =  READ_FRC_REG(FRC_BBD_RO_FINER_HIST_DATA_20);  //Bit 0-31;
	g_stbbd_rd->finer_hist_data_21  =  READ_FRC_REG(FRC_BBD_RO_FINER_HIST_DATA_21);  //Bit 0-31;
	g_stbbd_rd->finer_hist_data_22  =  READ_FRC_REG(FRC_BBD_RO_FINER_HIST_DATA_22);  //Bit 0-31;
	g_stbbd_rd->finer_hist_data_23  =  READ_FRC_REG(FRC_BBD_RO_FINER_HIST_DATA_23);  //Bit 0-31;
	g_stbbd_rd->finer_hist_data_24  =  READ_FRC_REG(FRC_BBD_RO_FINER_HIST_DATA_24);  //Bit 0-31;
	g_stbbd_rd->finer_hist_data_25  =  READ_FRC_REG(FRC_BBD_RO_FINER_HIST_DATA_25);  //Bit 0-31;
	g_stbbd_rd->finer_hist_data_26  =  READ_FRC_REG(FRC_BBD_RO_FINER_HIST_DATA_26);  //Bit 0-31;
	g_stbbd_rd->finer_hist_data_27  =  READ_FRC_REG(FRC_BBD_RO_FINER_HIST_DATA_27);  //Bit 0-31;
	g_stbbd_rd->finer_hist_data_28  =  READ_FRC_REG(FRC_BBD_RO_FINER_HIST_DATA_28);  //Bit 0-31;
	g_stbbd_rd->finer_hist_data_29  =  READ_FRC_REG(FRC_BBD_RO_FINER_HIST_DATA_29);  //Bit 0-31;
	g_stbbd_rd->finer_hist_data_30  =  READ_FRC_REG(FRC_BBD_RO_FINER_HIST_DATA_30);  //Bit 0-31;
	g_stbbd_rd->finer_hist_data_31  =  READ_FRC_REG(FRC_BBD_RO_FINER_HIST_DATA_31);  //Bit 0-31;

	g_stbbd_rd->max1_hist_idx  =  READ_FRC_REG(FRC_BBD_RO_HIST_IDX)  >> 16 & 0xFF; //Bit 16-23;
	g_stbbd_rd->max2_hist_idx  =  READ_FRC_REG(FRC_BBD_RO_HIST_IDX)  >> 8  & 0xFF; //Bit 8-15;
	g_stbbd_rd->min1_hist_idx  =  READ_FRC_REG(FRC_BBD_RO_HIST_IDX)  >> 0  & 0xFF; //Bit 0-7;

	g_stbbd_rd->max1_hist_cnt  =  READ_FRC_REG(FRC_BBD_RO_MAX1_HIST_CNT);	       //Bit 0-31;
	g_stbbd_rd->max2_hist_cnt  =  READ_FRC_REG(FRC_BBD_RO_MAX2_HIST_CNT);	       //Bit 0-31;
	g_stbbd_rd->min1_hist_cnt  =  READ_FRC_REG(FRC_BBD_RO_MIN1_HIST_CNT);	       //Bit 0-31;

	g_stbbd_rd->apl_glb_sum    =  READ_FRC_REG(FRC_BBD_RO_APL_GLB_SUM);	       //Bit 0-31;

	//-------ip logo readbacks---------
	g_stlogodetect_rd->iplogo_pxl_cnt =
		READ_FRC_REG(FRC_IPLOGO_RO_LOGOPIX_CNT) & 0xFFFFF;  //Bit 0:19

	for (i = 0; i < 12; i++) {
		g_stlogodetect_rd->iplogo_pxl_rgncnt[i] =
		READ_FRC_REG(FRC_IPLOGO_RO_PIXLOGO_RGN_CNT_0 +
			     i * (FRC_IPLOGO_RO_PIXLOGO_RGN_CNT_1 -
				  FRC_IPLOGO_RO_PIXLOGO_RGN_CNT_0)) & 0xFFFFF;  //Bit 0:19
		g_stlogodetect_rd->iplogo_osd_rgncnt[i] =
		READ_FRC_REG(FRC_IPLOGO_RO_OSDBIT_RGN_CNT_0 +
			     i * (FRC_IPLOGO_RO_OSDBIT_RGN_CNT_1 -
				  FRC_IPLOGO_RO_OSDBIT_RGN_CNT_0)) & 0xFFFFF;  //Bit 0:19
	}
}

void frc_scene_detect_output(struct frc_dev_s *frc_devp)
{
	u8 i = 0, j = 0;
	s32 tmp_value = 0;
	struct st_me_rd *g_stme_rd;
	struct st_vp_rd *g_stvp_rd;
	struct st_mc_rd *g_stmc_rd;
	struct frc_fw_data_s *fw_data;

	fw_data = (struct frc_fw_data_s *)frc_devp->fw_data;
	g_stme_rd = &fw_data->g_stme_rd;
	g_stvp_rd = &fw_data->g_stvp_rd;
	g_stmc_rd = &fw_data->g_stmc_rd;

	//--------me readbacks----------
	g_stme_rd->gmv_invalid = READ_FRC_REG(FRC_ME_RO_GMV) >> 12 & 0x1;
	tmp_value =   READ_FRC_REG(FRC_ME_RO_GMV) >> 16	& 0x1FFF;
	g_stme_rd->gmvx	= negative_convert(tmp_value, 13);
	tmp_value = READ_FRC_REG(FRC_ME_RO_GMV)	& 0xFFF;
	g_stme_rd->gmvy = negative_convert(tmp_value, 12);
	tmp_value = READ_FRC_REG(FRC_ME_RO_GMV_2ND) >> 16	 & 0x1FFF;
	g_stme_rd->gmvx_2nd = negative_convert(tmp_value, 13);
	tmp_value = READ_FRC_REG(FRC_ME_RO_GMV_2ND) & 0xFFF;
	g_stme_rd->gmvy_2nd = negative_convert(tmp_value, 12);
	for (i = 0; i < 12; i++) {
		tmp_value		     =
		READ_FRC_REG(FRC_ME_RO_GMV_RGN_0 +
			    i * (FRC_ME_RO_GMV_RGN_1 - FRC_ME_RO_GMV_RGN_0)) >> 16 & 0x1FFF;
		g_stme_rd->region_gmvx[i]    =	 negative_convert(tmp_value, 13);
		tmp_value		     =
		READ_FRC_REG(FRC_ME_RO_GMV_RGN_0 +
			     i * (FRC_ME_RO_GMV_RGN_1 - FRC_ME_RO_GMV_RGN_0)) & 0xFFF;
		g_stme_rd->region_gmvy[i]    =	 negative_convert(tmp_value, 12);
		tmp_value		     =
		READ_FRC_REG(FRC_ME_RO_GMV_RGN_2ND_0 +
			     i * (FRC_ME_RO_GMV_RGN_2ND_1 -
				  FRC_ME_RO_GMV_RGN_2ND_0)) >> 16 & 0x1FFF;
		g_stme_rd->region_gmvx_2d[i] =	 negative_convert(tmp_value, 13);
		tmp_value		     =
		READ_FRC_REG(FRC_ME_RO_GMV_RGN_2ND_0 +
			     i * (FRC_ME_RO_GMV_RGN_2ND_1 - FRC_ME_RO_GMV_RGN_2ND_0)) & 0xFFF;
		g_stme_rd->region_gmvy_2d[i] =	 negative_convert(tmp_value, 12);
		g_stme_rd->region_t_consis[i] =
		READ_FRC_REG(FRC_ME_RO_RGN_T_CONSIS_0 +
			     i * (FRC_ME_RO_RGN_T_CONSIS_1 - FRC_ME_RO_RGN_T_CONSIS_0));
	}
	tmp_value = READ_FRC_REG(FRC_ME_RO_GMV_MUX) >> 16 & 0x1FFF;
	g_stme_rd->gmvx_mux = negative_convert(tmp_value, 13);
	tmp_value = READ_FRC_REG(FRC_ME_RO_GMV_MUX) & 0xFFF;
	g_stme_rd->gmvy_mux = negative_convert(tmp_value, 12);
	g_stme_rd->gmv_cnt = READ_FRC_REG(FRC_ME_RO_GMV_CNT) & 0x3FFFF;
	g_stme_rd->gmv_dtl_cnt = READ_FRC_REG(FRC_ME_RO_GMV_DTL_CNT) & 0x3FFFF;
	g_stme_rd->gmv_total_sum = READ_FRC_REG(FRC_ME_RO_GMV_TOTAL_SUM) & 0x3FFFF;
	g_stme_rd->gmv_neighbor_cnt = READ_FRC_REG(FRC_ME_RO_GMV_NCNT) & 0x3FFFF;
	g_stme_rd->gmv_mux_invalid = READ_FRC_REG(FRC_ME_RO_GMV_MUX) >> 12 & 0x1;
	g_stme_rd->gmv_small_neighbor_cnt = READ_FRC_REG(FRC_ME_RO_GMV_SMALL_NCNT) & 0x3FFFF;

	for (i = 0; i < 4; i++) {
		g_stme_rd->glb_t_consis[i]   =
		READ_FRC_REG(FRC_ME_RO_T_CONSIS_0 +
			     i * (FRC_ME_RO_T_CONSIS_1 - FRC_ME_RO_T_CONSIS_0));
		g_stme_rd->glb_apl_sum[i]    =
		READ_FRC_REG(FRC_ME_RO_APL_0 + i * (FRC_ME_RO_APL_1 - FRC_ME_RO_APL_0)) & 0xFFFFFF;
		g_stme_rd->glb_sad_sum[i]    =
		READ_FRC_REG(FRC_ME_RO_SAD_0 + i * (FRC_ME_RO_SAD_1 - FRC_ME_RO_SAD_0));
		g_stme_rd->glb_bad_sad_cnt[i] =
		READ_FRC_REG(FRC_ME_RO_BLK_CNT_SAD_0 +
			     i * (FRC_ME_RO_BLK_CNT_SAD_1 - FRC_ME_RO_BLK_CNT_SAD_0)) & 0x3FFFF;
		g_stme_rd->glb_cnt[i]	     =
		READ_FRC_REG(FRC_ME_RO_BLK_CNT_0 +
			     i * (FRC_ME_RO_BLK_CNT_1 - FRC_ME_RO_BLK_CNT_0)) & 0x3FFFF;
	}

	for (i = 0; i < 6; i++) {
		for (j = 0; j < 8; j++) {
			g_stme_rd->region_fb_t_consis[i][j] =
			READ_FRC_REG(FRC_ME_RO_REGION_FB_T_0_0 +
				     i * (FRC_ME_RO_REGION_FB_T_1_0 - FRC_ME_RO_REGION_FB_T_0_0) +
				     j * (FRC_ME_RO_REGION_FB_T_0_1 - FRC_ME_RO_REGION_FB_T_0_0));
			g_stme_rd->region_fb_dtl_sum[i][j] =
			READ_FRC_REG(FRC_ME_RO_REGION_FB_DTL_0_0 +
				    i * (FRC_ME_RO_REGION_FB_DTL_1_0 -
					 FRC_ME_RO_REGION_FB_DTL_0_0) +
				    j * (FRC_ME_RO_REGION_FB_DTL_0_1 -
					 FRC_ME_RO_REGION_FB_DTL_0_0));
			g_stme_rd->region_fb_s_consis[i][j] =
			READ_FRC_REG(FRC_ME_RO_REGION_FB_S_0_0 +
				     i * (FRC_ME_RO_REGION_FB_S_1_0 - FRC_ME_RO_REGION_FB_S_0_0) +
				     j * (FRC_ME_RO_REGION_FB_S_0_1 - FRC_ME_RO_REGION_FB_S_0_0));
			g_stme_rd->region_fb_sad_sum[i][j] =
			READ_FRC_REG(FRC_ME_RO_REGION_FB_SAD_SUM_0_0 +
				     i * (FRC_ME_RO_REGION_FB_SAD_SUM_1_0 -
					  FRC_ME_RO_REGION_FB_SAD_SUM_0_0) +
				     j * (FRC_ME_RO_REGION_FB_SAD_SUM_0_1 -
					  FRC_ME_RO_REGION_FB_SAD_SUM_0_0));
			g_stme_rd->region_fb_sad_cnt[i][j] =
			READ_FRC_REG(FRC_ME_RO_REGION_FB_SAD_CNT_0_0 +
				     i * (FRC_ME_RO_REGION_FB_SAD_CNT_1_0 -
					  FRC_ME_RO_REGION_FB_SAD_CNT_0_0) +
				     j * (FRC_ME_RO_REGION_FB_SAD_CNT_0_1 -
					  FRC_ME_RO_REGION_FB_SAD_CNT_0_0));
		}
	}
	g_stme_rd->zmv_cnt		 =   READ_FRC_REG(FRC_ME_RO_ZMV_CNT) & 0x3FFFF;
	g_stme_rd->glb_unstable_cnt  =   READ_FRC_REG(FRC_ME_RO_FCMV_CNT) & 0x3FFFF;
	g_stme_rd->glb_act_hi_x_cnt  =   READ_FRC_REG(FRC_ME_RO_ACT_HI_X) & 0x3FFFF;
	g_stme_rd->glb_act_hi_y_cnt  =   READ_FRC_REG(FRC_ME_RO_ACT_HI_Y) & 0x3FFFF;
	for (i = 0; i < 22; i++) {
		g_stme_rd->glb_pos_hi_y_cnt[i] =
		READ_FRC_REG(FRC_ME_RO_POS_HI_Y_0 +
			     i * (FRC_ME_RO_POS_HI_Y_1 - FRC_ME_RO_POS_HI_Y_0)) & 0x3FFFF;
		g_stme_rd->glb_neg_lo_y_cnt[i] =
		READ_FRC_REG(FRC_ME_RO_NEG_LO_Y_0 +
			     i * (FRC_ME_RO_NEG_LO_Y_1 - FRC_ME_RO_NEG_LO_Y_0)) & 0x3FFFF;
	}

	//-------vp readbacks----------
	g_stvp_rd->glb_cover_cnt		 =   READ_FRC_REG(FRC_VP_RO_GLOBAL_OCT_COVER_CNT);
	g_stvp_rd->glb_uncov_cnt		 =   READ_FRC_REG(FRC_VP_RO_GLOBAL_OCT_UNCOV_CNT);
	for (i = 0; i < 12; i++) {
		g_stvp_rd->region_cover_cnt[i] =
		READ_FRC_REG(FRC_VP_RO_REGION_OCT_COVER_CNT_0 +
			     i *
			     (FRC_VP_RO_REGION_OCT_COVER_CNT_1 - FRC_VP_RO_REGION_OCT_COVER_CNT_0));
		g_stvp_rd->region_uncov_cnt[i] =
		READ_FRC_REG(FRC_VP_RO_REGION_OCT_UNCOV_CNT_0 +
			     i *
			     (FRC_VP_RO_REGION_OCT_UNCOV_CNT_1 - FRC_VP_RO_REGION_OCT_UNCOV_CNT_0));
	}
	tmp_value				 =   READ_FRC_REG(FRC_VP_RO_GLOBAL_OCT_GMVX_SUM);
	g_stvp_rd->oct_gmvx_sum		 =   negative_convert(tmp_value, 32);
	tmp_value				 =   READ_FRC_REG(FRC_VP_RO_GLOBAL_OCT_GMVY_SUM);
	g_stvp_rd->oct_gmvy_sum		 =   negative_convert(tmp_value, 32);
	g_stvp_rd->oct_gmv_diff		 =   READ_FRC_REG(FRC_VP_RO_GLOBAL_OCT_GMV_DIFF);

	//------me logo readbacks----------
	//-----mc readbacks----------
	//  mc logo part
	g_stmc_rd->pre_mc_logo_cnt = READ_FRC_REG(FRC_MC_PRE_INSIDE_MCLOGO_CNT);
	g_stmc_rd->cur_mc_logo_cnt = READ_FRC_REG(FRC_MC_CUR_INSIDE_MCLOGO_CNT);
	g_stmc_rd->pre_ip_logo_cnt = READ_FRC_REG(FRC_MC_PRE_INSIDE_IPLOGO_CNT);
	g_stmc_rd->cur_ip_logo_cnt = READ_FRC_REG(FRC_MC_CUR_INSIDE_IPLOGO_CNT);
	g_stmc_rd->pre_me_logo_cnt = READ_FRC_REG(FRC_MC_PRE_INSIDE_MELOGO_CNT);
	g_stmc_rd->cur_me_logo_cnt = READ_FRC_REG(FRC_MC_CUR_INSIDE_MELOGO_CNT);

	//  mc srch rng part
	g_stmc_rd->luma_abv_xscale = READ_FRC_REG(FRC_MC_RO_H2V2) >> 7 & 0x1;
	g_stmc_rd->luma_abv_yscale = READ_FRC_REG(FRC_MC_RO_H2V2) >> 6 & 0x1;
	g_stmc_rd->luma_blw_xscale = READ_FRC_REG(FRC_MC_RO_H2V2) >> 5 & 0x1;
	g_stmc_rd->luma_blw_yscale = READ_FRC_REG(FRC_MC_RO_H2V2) >> 4 & 0x1;

	g_stmc_rd->chrm_abv_xscale = READ_FRC_REG(FRC_MC_RO_H2V2) >> 3 & 0x1;
	g_stmc_rd->chrm_abv_yscale = READ_FRC_REG(FRC_MC_RO_H2V2) >> 2 & 0x1;
	g_stmc_rd->chrm_blw_xscale = READ_FRC_REG(FRC_MC_RO_H2V2) >> 1 & 0x1;
	g_stmc_rd->chrm_blw_yscale = READ_FRC_REG(FRC_MC_RO_H2V2) & 0x1;

	g_stmc_rd->pre_luma_lbuf_abv_ofst = READ_FRC_REG(FRC_MC_PRE_LBUF_LUMA_OFST) >> 16 & 0x1FF;
	g_stmc_rd->pre_luma_lbuf_blw_ofst = READ_FRC_REG(FRC_MC_PRE_LBUF_LUMA_OFST) & 0x1FF;
	g_stmc_rd->pre_luma_lbuf_total    =
		READ_FRC_REG(FRC_MC_PRE_LBUF_LUMA_LINE_NUM) >> 23 & 0x1FF;

	g_stmc_rd->pre_chrm_lbuf_abv_ofst = READ_FRC_REG(FRC_MC_PRE_LBUF_CHRM_OFST) >> 16 & 0x1FF;
	g_stmc_rd->pre_chrm_lbuf_blw_ofst = READ_FRC_REG(FRC_MC_PRE_LBUF_CHRM_OFST) & 0x1FF;
	g_stmc_rd->pre_chrm_lbuf_total    =
		READ_FRC_REG(FRC_MC_PRE_LBUF_CHRM_LINE_NUM) >> 23 & 0x1FF;

	g_stmc_rd->cur_luma_lbuf_abv_ofst = READ_FRC_REG(FRC_MC_CUR_LBUF_LUMA_OFST) >> 16 & 0x1FF;
	g_stmc_rd->cur_luma_lbuf_blw_ofst = READ_FRC_REG(FRC_MC_CUR_LBUF_LUMA_OFST) & 0x1FF;
	g_stmc_rd->cur_luma_lbuf_total    =
		READ_FRC_REG(FRC_MC_CUR_LBUF_LUMA_LINE_NUM) >> 23 & 0x1FF;

	g_stmc_rd->cur_chrm_lbuf_abv_ofst = READ_FRC_REG(FRC_MC_CUR_LBUF_CHRM_OFST) >> 16 & 0x1FF;
	g_stmc_rd->cur_chrm_lbuf_blw_ofst = READ_FRC_REG(FRC_MC_CUR_LBUF_CHRM_OFST) & 0x1FF;
	g_stmc_rd->cur_chrm_lbuf_total    =
		READ_FRC_REG(FRC_MC_CUR_LBUF_CHRM_LINE_NUM) >> 23 & 0x1FF;
}

