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
#include "frc_me.h"

static u8 fallback_level[11]  = {0,0,2,4,6,7,9,10,12,14,15};  //TEMP MODIFY

void frc_me_param_init(struct frc_dev_s *frc_devp)
{
	u8  k, i, j;
	struct frc_fw_data_s *frc_data;
	struct st_me_ctrl_para *g_stMeCtrl_Para;
	struct st_scene_change_detect_para *g_stScnChgDet_Para;
	struct st_fb_ctrl_item *g_stFbCtrl_Item;
	struct st_fb_ctrl_para *g_stFbCtrl_Para;
	struct st_me_ctrl_item *g_stMeCtrl_Item;
	struct st_region_fb_ctrl_item *g_stRegionFbCtrl_Item;
	struct st_me_rule_en *g_stMeRule_EN;

	frc_data = (struct frc_fw_data_s *)frc_devp->fw_data;
	g_stMeCtrl_Para = &frc_data->g_stMeCtrl_Para;
	g_stScnChgDet_Para = &frc_data->g_stScnChgDet_Para;
	g_stFbCtrl_Item = &frc_data->g_stFbCtrl_Item;
	g_stFbCtrl_Para = &frc_data->g_stFbCtrl_Para;
	g_stMeCtrl_Item = &frc_data->g_stMeCtrl_Item;
	g_stRegionFbCtrl_Item = &frc_data->g_stRegionFbCtrl_Item;
	g_stMeRule_EN = &frc_data->g_stMeRule_EN;

	g_stMeCtrl_Para->me_en           = 0 ;
	g_stScnChgDet_Para->schg_det_en0 = 69;   // b0: top enable
						// b1: check global sad enable
						// b2: check global sad diff enable
						// b3: check global bad sad count enable
						// b4: check global apl diff enable
						// b5: check global t consis enable
						// b6: check global unstable enable
	g_stScnChgDet_Para->schg_strict_mod0= 1; // 0: and, 1: or
	g_stScnChgDet_Para->schg_glb_sad_ph_thd0  = 340;
	g_stScnChgDet_Para->schg_glb_sad_cn_thd0  =1023;
	g_stScnChgDet_Para->schg_glb_sad_nc_thd0  =1023;
	g_stScnChgDet_Para->schg_glb_sad_dif_thd0 =120;
	g_stScnChgDet_Para->schg_glb_sad_ph_cnt_rt0  =32;
	g_stScnChgDet_Para->schg_glb_sad_cn_cnt_rt0  =85;
	g_stScnChgDet_Para->schg_glb_sad_nc_cnt_rt0  =85;
	g_stScnChgDet_Para->schg_glb_apl_pc_thd0     =32;
	g_stScnChgDet_Para->schg_glb_apl_cn_thd0     =80;
	g_stScnChgDet_Para->schg_glb_t_consis_ph_thd0=64;
	g_stScnChgDet_Para->schg_glb_t_consis_ph_rt0 =32;
	g_stScnChgDet_Para->schg_glb_t_consis_cn_thd0=160;
	g_stScnChgDet_Para->schg_glb_t_consis_cn_rt0 =85;
	g_stScnChgDet_Para->schg_glb_t_consis_nc_thd0=160;
	g_stScnChgDet_Para->schg_glb_t_consis_nc_rt0 =85;
	g_stScnChgDet_Para->schg_glb_unstable_rt0    =85;

	g_stScnChgDet_Para->schg_det_en1 = 31;   // b0: top enable
						// b1: check global sad enable
						// b2: check global sad diff enable
						// b3: check global bad sad count enable
						// b4: check global apl diff enable
						// b5: check global t consis enable
						// b6: check global unstable enable
	g_stScnChgDet_Para->schg_strict_mod1      = 1; // 0: and, 1: or
	g_stScnChgDet_Para->schg_glb_sad_ph_thd1  =1023;
	g_stScnChgDet_Para->schg_glb_sad_cn_thd1  = 340;
	g_stScnChgDet_Para->schg_glb_sad_nc_thd1  = 340;
	g_stScnChgDet_Para->schg_glb_sad_dif_thd1 =120;
	g_stScnChgDet_Para->schg_glb_sad_ph_cnt_rt1  =85;
	g_stScnChgDet_Para->schg_glb_sad_cn_cnt_rt1  =32;
	g_stScnChgDet_Para->schg_glb_sad_nc_cnt_rt1  =32;
	g_stScnChgDet_Para->schg_glb_apl_pc_thd1     =80;
	g_stScnChgDet_Para->schg_glb_apl_cn_thd1     =32;
	//below (related to mv diff) maybe useless if scene change static 0 happened and vbuf cleared
	g_stScnChgDet_Para->schg_glb_t_consis_ph_thd1=160;
	g_stScnChgDet_Para->schg_glb_t_consis_ph_rt1 =85;
	g_stScnChgDet_Para->schg_glb_t_consis_cn_thd1=64;
	g_stScnChgDet_Para->schg_glb_t_consis_cn_rt1 =32;
	g_stScnChgDet_Para->schg_glb_t_consis_nc_thd1=64;
	g_stScnChgDet_Para->schg_glb_t_consis_nc_rt1 =32;
	g_stScnChgDet_Para->schg_glb_unstable_rt1    =85;

	// global fallback
	g_stFbCtrl_Item->glb_TC_iir_pre           = 0;
	for(k = 0; k < 20; k++)
		g_stFbCtrl_Item->glb_TC_20[k]         = 0;
	g_stFbCtrl_Item->TC_th_s_pre              = 0;
	g_stFbCtrl_Item->TC_th_l_pre              = 0;
	g_stFbCtrl_Item->mc_fallback_level_pre    = 0;

	g_stFbCtrl_Para->glb_tc_iir_up            = 48;	//0x30
	g_stFbCtrl_Para->glb_tc_iir_dn            = 240;	//0xf0
	g_stFbCtrl_Para->TC_th_iir_up             = 224;	//0xE0
	g_stFbCtrl_Para->TC_th_iir_dn             = 64;	//0x40
	g_stFbCtrl_Para->fb_level_iir_up          = 96;	//0x60
	g_stFbCtrl_Para->fb_level_iir_dn          = 176;	//0xB0
	g_stFbCtrl_Para->fb_gain_gmv_th_s         = 48;	//0x30
	g_stFbCtrl_Para->fb_gain_gmv_th_l         = 96;	//0x60
	g_stFbCtrl_Para->fb_gain_gmv_ratio_s      = 8;	//8
	g_stFbCtrl_Para->fb_gain_gmv_ratio_l      = 24; //24
	g_stFbCtrl_Para->fallback_level_max       = 10;
	g_stFbCtrl_Para->base_TC_th_s             = 50000;//13056;   //0xcc00>>2, //TEMP MODIFY
	g_stFbCtrl_Para->base_TC_th_l             = 80000;//43008;   //0x2a000>>2, //TEMP MODIFY

	// region fallback
	for(i = 0; i < 6; i++)
	{
		for(j = 0; j <8; j++)
		{
			g_stRegionFbCtrl_Item->region_TC_iir_pre[i][j] = 0;
			g_stRegionFbCtrl_Item->region_TC_th_s_pre[i][j] = 0;
			g_stRegionFbCtrl_Item->region_TC_th_l_pre[i][j] = 0;
			g_stRegionFbCtrl_Item->region_fb_level_pre[i][j] = 0;
		}
	}

	g_stFbCtrl_Para->region_TC_iir_up = 0x60;	//0x30
	g_stFbCtrl_Para->region_TC_iir_dn = 0x80;	//0xf0
	g_stFbCtrl_Para->region_TC_th_iir_up = 0xE0;	//0xE0
	g_stFbCtrl_Para->region_TC_th_iir_dn = 0x40;	//0x40
	g_stFbCtrl_Para->region_fb_level_iir_up = 0x60;	//0x60
	g_stFbCtrl_Para->region_fb_level_iir_dn = 0x80;	//0xB0
	g_stFbCtrl_Para->region_TC_bad_th = 800; //4096
	g_stFbCtrl_Para->region_fb_level_b_th = 7;
	g_stFbCtrl_Para->region_fb_level_s_th = 4;
	g_stFbCtrl_Para->region_fb_level_ero_cnt_b_th = 5;
	g_stFbCtrl_Para->region_fb_level_dil_cnt_s_th = 1;
	g_stFbCtrl_Para->region_fb_level_dil_cnt_b_th = 1;
	g_stFbCtrl_Para->region_fb_gain_gmv_th_s  = 60;
	g_stFbCtrl_Para->region_fb_gain_gmv_th_l  = 96;
	g_stFbCtrl_Para->region_fb_gain_gmv_ratio_s   = 8;
	g_stFbCtrl_Para->region_fb_gain_gmv_ratio_l   = 24;
	g_stFbCtrl_Para->region_dtl_sum_th = 5000;
	g_stFbCtrl_Para->region_fb_ext_th = 8;
	g_stFbCtrl_Para->region_fb_ext_coef = 4;    //u3,coef<<3,coef=4 equal to 0.5
	g_stFbCtrl_Para->base_region_TC_th_s   =   4000;   //4*6
	g_stFbCtrl_Para->base_region_TC_th_l   =   6000;

	g_stMeCtrl_Para->update_strength_add_value   =   0   ;
	g_stMeCtrl_Para->scene_change_flag           =   0   ;
	g_stMeCtrl_Para->fallback_gmvx_th            =   250 ;
	g_stMeCtrl_Para->fallback_gmvy_th            =   79  ;
	g_stMeCtrl_Para->region_sad_median_num       =   10  ;
	g_stMeCtrl_Para->region_sad_sum_th           =   70000   ;
	g_stMeCtrl_Para->region_sad_cnt_th           =   50      ;
	g_stMeCtrl_Para->region_s_consis_th          =   2000    ;
	g_stMeCtrl_Para->region_win3x3_min           =   1       ;
	g_stMeCtrl_Para->region_win3x3_max           =   5       ;

	g_stMeCtrl_Item->static_scene_frame_count    =   20  ;
	g_stMeCtrl_Item->scene_change_catchin_frame_count=20 ;
	g_stMeCtrl_Item->scene_change_frame_count    =   8   ;
	g_stMeCtrl_Item->scene_change_judder_frame_count=0   ;
	g_stMeCtrl_Item->mixmodein_frame_count       =   0   ;
	g_stMeCtrl_Item->mixmodeout_frame_count      =   0   ;
	g_stMeCtrl_Item->scene_change_reject_frame_count=40  ;

	for(i = 0; i < 48; i++)
	{
		for(j = 0; j< 20; j++)
		{
			g_stMeCtrl_Item->region_sad_sum_20[i][j] =   0;
			g_stMeCtrl_Item->region_sad_cnt_20[i][j] =   0;
			g_stMeCtrl_Item->region_s_consis_20[i][j]=   0;
		}
	}
	g_stMeCtrl_Para->me_rule_ctrl_en = 1;
	g_stMeRule_EN->rule1_en = 1;//rule_1, static scene			    --> small random & zmv_penalty=0
	g_stMeRule_EN->rule2_en = 1;//rule_2, when scene change, big apl --> increase random, small apl-->decrease random
	g_stMeRule_EN->rule3_en = 1;//rule_3, when scene change		    --> enable spatial sad rule for clear v-buf
	g_stMeRule_EN->rule4_en = 1;//rule_4, static scene 			    --> enable glb_mot rule for clear v-buf
	g_stMeRule_EN->rule5_en = 1;//rule_5, pure panning			    --> enable periodic0
	g_stMeRule_EN->rule6_en = 1;//rule_6, MV> search range  		    --> FB
	g_stMeRule_EN->rule7_en = 1;//rule_7, scene change 			    --> two frame FB
	g_stMeRule_EN->rule8_en = 1;//rule_8, demo window||mix mode      --> disable clear v-buf
	g_stMeRule_EN->rule9_en = 1;//rule_9, for fast panning 		    --> control zero panalty
	g_stMeRule_EN->rule10_en = 1;//rule_10, scene change 		    --> dehalo off

	g_stMeCtrl_Para->me_add_7_flag_mode = 0;
	
}

void set_me_gmv(struct frc_dev_s *frc_devp)
{
	u8 k;
	struct frc_fw_data_s *frc_data;
	struct st_me_rd *g_stME_RD;

	frc_data = (struct frc_fw_data_s *)frc_devp->fw_data;
	g_stME_RD = &frc_data->g_stme_rd;

	UPDATE_FRC_REG_BITS(FRC_ME_GMV, (g_stME_RD->gmv_invalid)<<15, BIT_15);
	UPDATE_FRC_REG_BITS(FRC_ME_GMV, (g_stME_RD->gmvx)<<16       , 0x1FFF0000);
	UPDATE_FRC_REG_BITS(FRC_ME_GMV, (g_stME_RD->gmvy)           , 0xFFF);
	if(g_stME_RD->zmv_cnt > g_stME_RD->gmv_cnt)
	{
		k = (g_stME_RD->gmv_invalid==0) ? 0 : (g_stME_RD->zmv_cnt > (g_stME_RD->gmv_dtl_cnt>>1));
		UPDATE_FRC_REG_BITS(FRC_ME_GMV_PATCH, k<<15, BIT_15);
		UPDATE_FRC_REG_BITS(FRC_ME_GMV_PATCH, 0<<16, 0x1FFF0000);
		UPDATE_FRC_REG_BITS(FRC_ME_GMV_PATCH, 0    , 0xFFF);
	}
	else
	{
		UPDATE_FRC_REG_BITS(FRC_ME_GMV_PATCH, (g_stME_RD->gmv_invalid)<<15, BIT_15);
		UPDATE_FRC_REG_BITS(FRC_ME_GMV_PATCH, (g_stME_RD->gmvx)<<16       , 0x1FFF0000);
		UPDATE_FRC_REG_BITS(FRC_ME_GMV_PATCH, (g_stME_RD->gmvy)           , 0xFFF);
	}

	UPDATE_FRC_REG_BITS(FRC_ME_GMV_2ND, (g_stME_RD->gmvx_2nd)<<16, 0x1FFF0000);
	UPDATE_FRC_REG_BITS(FRC_ME_GMV_2ND, (g_stME_RD->gmvy_2nd)    , 0xFFF);

	for(k = 0; k < 12; k++)
	{
		UPDATE_FRC_REG_BITS(FRC_ME_STAT_GMV_RGN_0 + k*(FRC_ME_STAT_GMV_RGN_1-FRC_ME_STAT_GMV_RGN_0), (g_stME_RD->region_gmvx[k])<<16, 0x1FFF0000);
		UPDATE_FRC_REG_BITS(FRC_ME_STAT_GMV_RGN_0 + k*(FRC_ME_STAT_GMV_RGN_1-FRC_ME_STAT_GMV_RGN_0), g_stME_RD->region_gmvy[k]      , 0xFFF);

		UPDATE_FRC_REG_BITS(FRC_ME_STAT_GMV_RGN_2ND_0 + k*(FRC_ME_STAT_GMV_RGN_2ND_1-FRC_ME_STAT_GMV_RGN_2ND_0), (g_stME_RD->region_gmvx_2d[k])<<16, 0x1FFF0000);
		UPDATE_FRC_REG_BITS(FRC_ME_STAT_GMV_RGN_2ND_0 + k*(FRC_ME_STAT_GMV_RGN_2ND_1-FRC_ME_STAT_GMV_RGN_2ND_0), (g_stME_RD->region_gmvy_2d[k])    , 0xFFF);
	}
}


void set_region_fb_size(void)
{
	u16 bb_blk_xyxy[4];
	u8  region_fb_xnum,region_fb_ynum;

	bb_blk_xyxy[0] = READ_FRC_REG(FRC_ME_BB_BLK_ST)>>16   & 0x3FF;
	bb_blk_xyxy[1] = READ_FRC_REG(FRC_ME_BB_BLK_ST)       & 0x3FF;
	bb_blk_xyxy[2] = READ_FRC_REG(FRC_ME_BB_BLK_ED)>>16   & 0x3FF;
	bb_blk_xyxy[3] = READ_FRC_REG(FRC_ME_BB_BLK_ED)       & 0x3FF;
	region_fb_xnum = READ_FRC_REG(FRC_ME_STAT_NEW_REGION)>>22 & 0xF;
	region_fb_ynum = READ_FRC_REG(FRC_ME_STAT_NEW_REGION)>>18 & 0xF;

	UPDATE_FRC_REG_BITS(FRC_ME_STAT_NEW_REGION, ((bb_blk_xyxy[2]-bb_blk_xyxy[0]+region_fb_xnum-1)/region_fb_xnum)<<8, 0xFF00);
	UPDATE_FRC_REG_BITS(FRC_ME_STAT_NEW_REGION, ((bb_blk_xyxy[3]-bb_blk_xyxy[1]+region_fb_ynum-1)/region_fb_ynum)   , 0xFF  );
}

void set_me_apl(struct frc_dev_s *frc_devp)
{
	struct frc_fw_data_s *frc_data;
	struct st_me_rd *g_stME_RD;

	frc_data = (struct frc_fw_data_s *)frc_devp->fw_data;
	g_stME_RD = &frc_data->g_stme_rd;

	u32 glb_apl[3];
	glb_apl[0] = (g_stME_RD->glb_cnt[0]==0) ? 0 : (g_stME_RD->glb_apl_sum[0]/g_stME_RD->glb_cnt[0]);
	glb_apl[1] = (g_stME_RD->glb_cnt[1]==0) ? 0 : (g_stME_RD->glb_apl_sum[1]/g_stME_RD->glb_cnt[1]);
	glb_apl[2] = (g_stME_RD->glb_cnt[2]==0) ? 0 : (g_stME_RD->glb_apl_sum[2]/g_stME_RD->glb_cnt[2]);

	UPDATE_FRC_REG_BITS(FRC_ME_STAT_GLB_APL, (glb_apl[0])<<24, 0xFF000000);
	UPDATE_FRC_REG_BITS(FRC_ME_STAT_GLB_APL, (glb_apl[1])<<16, 0xFF0000  );
	UPDATE_FRC_REG_BITS(FRC_ME_STAT_GLB_APL, (glb_apl[2])<<8 , 0xFF00    );
}

void scene_change_detect(struct frc_dev_s *frc_devp)
{
	u8  k, t_consis_option;
	u32 glb_sad[3], bad_sad[3], t_consis[3], glb_apl[3];
	u8  schg_flg[2] = {0, 0};
	u8  cond[8] = {0, 0, 0, 0, 0, 0, 0, 0};
	struct frc_fw_data_s *frc_data;
	struct st_me_rd *g_stME_RD;
	struct st_scene_change_detect_para *g_stScnChgDet_Para;

	frc_data = (struct frc_fw_data_s *)frc_devp->fw_data;
	g_stME_RD = &frc_data->g_stme_rd;
	g_stScnChgDet_Para = &frc_data->g_stScnChgDet_Para;

	glb_apl[0]      = READ_FRC_REG(FRC_ME_STAT_GLB_APL)>>24       & 0xFF;
	glb_apl[1]      = READ_FRC_REG(FRC_ME_STAT_GLB_APL)>>16       & 0xFF;
	glb_apl[2]      = READ_FRC_REG(FRC_ME_STAT_GLB_APL)>>8        & 0xFF;
	t_consis_option = READ_FRC_REG(FRC_ME_STAT_T_CONSIS_TH)>>26   & 0x1;
	// scence change static 0:
	// P        C       N
	// old      old     new

	// check global sad ph/cn/nc
	for(k = 0; k < 3; k++)
		glb_sad[k] = (g_stME_RD->glb_cnt[k]==0) ? 0 : (g_stME_RD->glb_sad_sum[k]/g_stME_RD->glb_cnt[k]);
	cond[0] =   (glb_sad[0] < g_stScnChgDet_Para->schg_glb_sad_ph_thd0) &&
	(glb_sad[1] > g_stScnChgDet_Para->schg_glb_sad_cn_thd0) &&
	(glb_sad[2] > g_stScnChgDet_Para->schg_glb_sad_nc_thd0);

	// check global sad diff phs vs cn/nc
	cond[1] =   ((glb_sad[1]-glb_sad[0]) > g_stScnChgDet_Para->schg_glb_sad_dif_thd0) &&
			((glb_sad[2]-glb_sad[0]) > g_stScnChgDet_Para->schg_glb_sad_dif_thd0);

	// check bad sad cnt
	bad_sad[0] = g_stME_RD->glb_cnt[0] * g_stScnChgDet_Para->schg_glb_sad_ph_cnt_rt0 / 256;
	bad_sad[1] = g_stME_RD->glb_cnt[1] * g_stScnChgDet_Para->schg_glb_sad_cn_cnt_rt0 / 256;
	bad_sad[2] = g_stME_RD->glb_cnt[2] * g_stScnChgDet_Para->schg_glb_sad_nc_cnt_rt0 / 256;
	cond[2] =   (g_stME_RD->glb_bad_sad_cnt[0] < bad_sad[0]) && (g_stME_RD->glb_bad_sad_cnt[1] > bad_sad[1]) && (g_stME_RD->glb_bad_sad_cnt[2] > bad_sad[2]);

	// check global apl diff
	cond[3] =   (ABS(glb_apl[2]-glb_apl[1]) > g_stScnChgDet_Para->schg_glb_apl_cn_thd0 ) &&
	(ABS(glb_apl[1]-glb_apl[0]) < g_stScnChgDet_Para->schg_glb_apl_pc_thd0 );

	// check global temporal consistence
	if(t_consis_option==0)
	{
		for(k = 0; k<3; k++)
			t_consis[k] = (g_stME_RD->glb_cnt[k]==0) ? 0 : (g_stME_RD->glb_t_consis[k]/g_stME_RD->glb_cnt[k]);

		cond[4] =   (t_consis[0] < g_stScnChgDet_Para->schg_glb_t_consis_ph_thd0) &&
				(t_consis[1] > g_stScnChgDet_Para->schg_glb_t_consis_cn_thd0) &&
				(t_consis[2] > g_stScnChgDet_Para->schg_glb_t_consis_nc_thd0);
	}
	else
	{
		t_consis[0] = g_stME_RD->glb_cnt[0] * g_stScnChgDet_Para->schg_glb_t_consis_ph_rt0 / 256;
		t_consis[1] = g_stME_RD->glb_cnt[1] * g_stScnChgDet_Para->schg_glb_t_consis_cn_rt0 / 256;
		t_consis[2] = g_stME_RD->glb_cnt[2] * g_stScnChgDet_Para->schg_glb_t_consis_nc_rt0 / 256;
		cond[4] =   (g_stME_RD->glb_t_consis[0] < t_consis[0]) && (g_stME_RD->glb_t_consis[1] > t_consis[1]) && (g_stME_RD->glb_t_consis[2] > t_consis[2]);
	}

	// check global unstable
	cond[5] = g_stME_RD->glb_unstable_cnt > g_stME_RD->glb_cnt[0]*g_stScnChgDet_Para->schg_glb_unstable_rt0/256;

	// get flag0
	if (g_stScnChgDet_Para->schg_strict_mod0 == 0){
		schg_flg[0] = (g_stScnChgDet_Para->schg_det_en0&1) == 1
			&&(((g_stScnChgDet_Para->schg_det_en0>>1)&1) == 1 && cond[0])
			&&(((g_stScnChgDet_Para->schg_det_en0>>2)&1) == 1 && cond[1])
			&&(((g_stScnChgDet_Para->schg_det_en0>>3)&1) == 1 && cond[2])
			&&(((g_stScnChgDet_Para->schg_det_en0>>4)&1) == 1 && cond[3])
			&&(((g_stScnChgDet_Para->schg_det_en0>>5)&1) == 1 && cond[4])
			&&(((g_stScnChgDet_Para->schg_det_en0>>6)&1) == 1 && cond[5]);
	}
	else {
		schg_flg[0] = (g_stScnChgDet_Para->schg_det_en0&1) == 1 &&
			((((g_stScnChgDet_Para->schg_det_en0>>1)&1) == 1 && cond[0])
			||(((g_stScnChgDet_Para->schg_det_en0>>2)&1) == 1 && cond[1])
			||(((g_stScnChgDet_Para->schg_det_en0>>3)&1) == 1 && cond[2])
			||(((g_stScnChgDet_Para->schg_det_en0>>4)&1) == 1 && cond[3])
			||(((g_stScnChgDet_Para->schg_det_en0>>5)&1) == 1 && cond[4])
			||(((g_stScnChgDet_Para->schg_det_en0>>6)&1) == 1 && cond[5]));
	}

	// scence change static 1:
	// P        C       N
	// old      new     new

	// check global sad ph/cn/nc
	for (k = 0; k < 3; k++)
		glb_sad[k] = (g_stME_RD->glb_cnt[k]==0) ? 0 : (g_stME_RD->glb_sad_sum[k]/g_stME_RD->glb_cnt[k]);
	cond[0] =   (glb_sad[0] > g_stScnChgDet_Para->schg_glb_sad_ph_thd1) &&
	(glb_sad[1] < g_stScnChgDet_Para->schg_glb_sad_cn_thd1) &&
	(glb_sad[2] < g_stScnChgDet_Para->schg_glb_sad_nc_thd1);

	// check global sad diff ph vs cn/nc
	cond[1] =   ((glb_sad[0] - glb_sad[1]) > g_stScnChgDet_Para->schg_glb_sad_dif_thd1) &&
			((glb_sad[0] - glb_sad[2]) > g_stScnChgDet_Para->schg_glb_sad_dif_thd1);

	// check bad sad cnt
	bad_sad[0] = g_stME_RD->glb_cnt[0] * g_stScnChgDet_Para->schg_glb_sad_ph_cnt_rt1 / 256;
	bad_sad[1] = g_stME_RD->glb_cnt[1] * g_stScnChgDet_Para->schg_glb_sad_cn_cnt_rt1 / 256;
	bad_sad[2] = g_stME_RD->glb_cnt[2] * g_stScnChgDet_Para->schg_glb_sad_nc_cnt_rt1 / 256;
	cond[2] =  (g_stME_RD->glb_bad_sad_cnt[0] > bad_sad[0]) && (g_stME_RD->glb_bad_sad_cnt[1] < bad_sad[1]) && (g_stME_RD->glb_bad_sad_cnt[2] < bad_sad[2]);

	// check global apl diff
	cond[3] =   (ABS(glb_apl[2] - glb_apl[1]) < g_stScnChgDet_Para->schg_glb_apl_cn_thd1) &&
	(ABS(glb_apl[1] - glb_apl[0]) > g_stScnChgDet_Para->schg_glb_apl_pc_thd1);

	// check global temporal consistence
	if (t_consis_option==0){
		for (k = 0; k < 3; k++)
			t_consis[k] = (g_stME_RD->glb_cnt[k]==0) ? 0 : (g_stME_RD->glb_t_consis[k]/g_stME_RD->glb_cnt[k]);

		cond[4] =   (t_consis[0] > g_stScnChgDet_Para->schg_glb_t_consis_ph_thd1) &&
				(t_consis[1] > g_stScnChgDet_Para->schg_glb_t_consis_cn_thd1) &&
				(t_consis[2] > g_stScnChgDet_Para->schg_glb_t_consis_nc_thd1);
	}
	else {
		t_consis[0] = g_stME_RD->glb_cnt[0] * g_stScnChgDet_Para->schg_glb_t_consis_ph_rt1 / 256;
		t_consis[1] = g_stME_RD->glb_cnt[1] * g_stScnChgDet_Para->schg_glb_t_consis_cn_rt1 / 256;
		t_consis[2] = g_stME_RD->glb_cnt[2] * g_stScnChgDet_Para->schg_glb_t_consis_nc_rt1 / 256;
		cond[4] = (g_stME_RD->glb_t_consis[0] > t_consis[0]) && (g_stME_RD->glb_t_consis[1] > t_consis[1]) && (g_stME_RD->glb_t_consis[2] > t_consis[2]);
	}

	// check global unstable
	cond[5] = g_stME_RD->glb_unstable_cnt > g_stME_RD->glb_cnt[0]* g_stScnChgDet_Para->schg_glb_unstable_rt1/256;

	// get flag1
	if (g_stScnChgDet_Para->schg_strict_mod1 == 0){
		schg_flg[1] = (g_stScnChgDet_Para->schg_det_en1&1) == 1
			&&(((g_stScnChgDet_Para->schg_det_en1>>1)&1) == 1 && cond[0])
			&&(((g_stScnChgDet_Para->schg_det_en1>>2)&1) == 1 && cond[1])
			&&(((g_stScnChgDet_Para->schg_det_en1>>3)&1) == 1 && cond[2])
			&&(((g_stScnChgDet_Para->schg_det_en1>>4)&1) == 1 && cond[3])
			&&(((g_stScnChgDet_Para->schg_det_en1>>5)&1) == 1 && cond[4])
			&&(((g_stScnChgDet_Para->schg_det_en1>>6)&1) == 1 && cond[5]);
	}
	else {
		schg_flg[1] = (g_stScnChgDet_Para->schg_det_en1&1) == 1 &&
			((((g_stScnChgDet_Para->schg_det_en1>>1)&1) == 1 && cond[0])
			||(((g_stScnChgDet_Para->schg_det_en1>>2)&1) == 1 && cond[1])
			||(((g_stScnChgDet_Para->schg_det_en1>>3)&1) == 1 && cond[2])
			||(((g_stScnChgDet_Para->schg_det_en1>>4)&1) == 1 && cond[3])
			||(((g_stScnChgDet_Para->schg_det_en1>>5)&1) == 1 && cond[4])
			||(((g_stScnChgDet_Para->schg_det_en1>>6)&1) == 1 && cond[5]));
	}

	// get final decision
	UPDATE_FRC_REG_BITS(FRC_ME_GCV_EN, (schg_flg[0] || schg_flg[1])<<17, BIT_17);
}

u32 fb_medianX(u32 *tc_array, u8 x_num)
{
	u8  i,j;
	u32 tmp,temp_buf[40];

	if (x_num >= 40 )
		PR_FRC("%s err\n", __func__);

	for(i=0; i<x_num; i++)
		temp_buf[i] = tc_array[i];

	for(i=0; i<x_num; i++)
	{
		for(j=i+1; j<x_num; j++)
		{
			if(temp_buf[i] > temp_buf[j])
			{
				tmp = temp_buf[i];
				temp_buf[i] = temp_buf[j];
				temp_buf[j] = tmp;
			}
		}
	}

	return temp_buf[x_num/2];
}

void fb_ctrl(struct frc_dev_s *frc_devp)
{
	u8  i, tc_iir_up, tc_iir_dn, fb_level_iir_up, fb_level_iir_dn;
	u8  gmv_ratio_gain, mc_fallback_level;
	u32 TC_Median, glb_TC_iir, TC_th_s, TC_th_l;

	struct frc_fw_data_s *frc_data;
	//struct st_me_ctrl_para *g_stMeCtrl_Para;
	//struct st_scene_change_detect_para *g_stScnChgDet_Para;
	struct st_fb_ctrl_item *g_stFbCtrl_Item;
	struct st_fb_ctrl_para *g_stFbCtrl_Para;
	//struct st_me_ctrl_item *g_stMeCtrl_Item;
	//struct st_region_fb_ctrl_item *g_stRegionFbCtrl_Item;
	struct st_me_rd *g_stME_RD;

	frc_data = (struct frc_fw_data_s *)frc_devp->fw_data;
	//g_stMeCtrl_Para = &frc_data->g_stMeCtrl_Para;
	//g_stScnChgDet_Para = &frc_data->g_stScnChgDet_Para;
	g_stFbCtrl_Item = &frc_data->g_stFbCtrl_Item;
	g_stFbCtrl_Para = &frc_data->g_stFbCtrl_Para;
	//g_stMeCtrl_Item = &frc_data->g_stMeCtrl_Item;
	//g_stRegionFbCtrl_Item = &frc_data->g_stRegionFbCtrl_Item;
	g_stME_RD = &frc_data->g_stme_rd;

	tc_iir_up = g_stFbCtrl_Para->glb_tc_iir_up;
	tc_iir_dn = g_stFbCtrl_Para->glb_tc_iir_dn;
	fb_level_iir_up = g_stFbCtrl_Para->fb_level_iir_up;
	fb_level_iir_dn = g_stFbCtrl_Para->fb_level_iir_dn;

	u8 gmv_ratio = (g_stME_RD->gmv_total_sum==0) ? 0 : ((100*g_stME_RD->gmv_neighbor_cnt)/g_stME_RD->gmv_total_sum);

	//cal global tc median value
	for(i=0; i<19; i++)
	{
		g_stFbCtrl_Item->glb_TC_20[i] = g_stFbCtrl_Item->glb_TC_20[i+1];
	}
	g_stFbCtrl_Item->glb_TC_20[19] = g_stME_RD->glb_t_consis[0];

	TC_Median = fb_medianX(g_stFbCtrl_Item->glb_TC_20, 20);

	//cal global tc iir
	if(TC_Median > g_stFbCtrl_Item->glb_TC_iir_pre)
	{
		glb_TC_iir = (g_stFbCtrl_Item->glb_TC_iir_pre*tc_iir_up + TC_Median*(256-tc_iir_up) + 128)>>8;
	}
	else
	{
		glb_TC_iir = (g_stFbCtrl_Item->glb_TC_iir_pre*tc_iir_dn + TC_Median*(256-tc_iir_dn) + 128)>>8;
	}
	g_stFbCtrl_Item->glb_TC_iir_pre = glb_TC_iir;

	//cal tc_th_s / tc_th_l
	if(gmv_ratio >= g_stFbCtrl_Para->fb_gain_gmv_th_l)
	{
		gmv_ratio_gain = g_stFbCtrl_Para->fb_gain_gmv_ratio_l;
	}
	else if(gmv_ratio <= g_stFbCtrl_Para->fb_gain_gmv_th_s)
	{
		gmv_ratio_gain = g_stFbCtrl_Para->fb_gain_gmv_ratio_s;
	}
	else
	{
		if(g_stFbCtrl_Para->fb_gain_gmv_th_l == g_stFbCtrl_Para->fb_gain_gmv_th_s)
		{
			gmv_ratio_gain = g_stFbCtrl_Para->fb_gain_gmv_ratio_s;
		}
		else
		{
			gmv_ratio_gain = g_stFbCtrl_Para->fb_gain_gmv_ratio_s +
                            (gmv_ratio-g_stFbCtrl_Para->fb_gain_gmv_th_s)*
                            (g_stFbCtrl_Para->fb_gain_gmv_ratio_l-g_stFbCtrl_Para->fb_gain_gmv_ratio_s)/
                            (g_stFbCtrl_Para->fb_gain_gmv_th_l-g_stFbCtrl_Para->fb_gain_gmv_th_s);
		}
	}

	TC_th_s = (g_stFbCtrl_Para->base_TC_th_s*gmv_ratio_gain)>>3;
	TC_th_l = (g_stFbCtrl_Para->base_TC_th_l*gmv_ratio_gain)>>3;

	if(TC_th_s > g_stFbCtrl_Item->TC_th_s_pre)
	{
		TC_th_s = (g_stFbCtrl_Item->TC_th_s_pre*g_stFbCtrl_Para->TC_th_iir_up + TC_th_s*(256-g_stFbCtrl_Para->TC_th_iir_up) + 128)>>8;
		TC_th_l = (g_stFbCtrl_Item->TC_th_l_pre*g_stFbCtrl_Para->TC_th_iir_up + TC_th_l*(256-g_stFbCtrl_Para->TC_th_iir_up) + 128)>>8;
	}
	else
	{
		TC_th_s = (g_stFbCtrl_Item->TC_th_s_pre*g_stFbCtrl_Para->TC_th_iir_dn + TC_th_s*(256-g_stFbCtrl_Para->TC_th_iir_dn) + 128)>>8;
		TC_th_l = (g_stFbCtrl_Item->TC_th_l_pre*g_stFbCtrl_Para->TC_th_iir_dn + TC_th_l*(256-g_stFbCtrl_Para->TC_th_iir_dn) + 128)>>8;
	}

	g_stFbCtrl_Item->TC_th_s_pre = TC_th_s;
	g_stFbCtrl_Item->TC_th_l_pre = TC_th_l;

	//cal mc fallback level
	if(glb_TC_iir < TC_th_s)
	{
		mc_fallback_level = 0;
	}
	else if(glb_TC_iir > TC_th_l)
	{
		mc_fallback_level = g_stFbCtrl_Para->fallback_level_max;
	}
	else
	{
		if(TC_th_l == TC_th_s)
		{
			mc_fallback_level = 0;
		}
		else
		{
			mc_fallback_level = g_stFbCtrl_Para->fallback_level_max*(glb_TC_iir -TC_th_s)/(TC_th_l - TC_th_s);
		}
	}

	//cal mc fallback level iir
	if(mc_fallback_level > g_stFbCtrl_Item->mc_fallback_level_pre)
	{
		mc_fallback_level = (g_stFbCtrl_Item->mc_fallback_level_pre*fb_level_iir_up + mc_fallback_level*(256-fb_level_iir_up) + 128)>>8;
	}
	else
	{
		mc_fallback_level = (g_stFbCtrl_Item->mc_fallback_level_pre*fb_level_iir_dn + mc_fallback_level*(256-fb_level_iir_dn) + 128)>>8;
	}
	g_stFbCtrl_Item->mc_fallback_level_pre = mc_fallback_level;

	UPDATE_FRC_REG_BITS(FRC_MC_LOGO_POST_BLENDER, (fallback_level[mc_fallback_level])<<12, 0xF000);

	pr_frc(dbg_me, "TC_Median=%d, glb_TC_iir=%d\n", TC_Median, glb_TC_iir);
	pr_frc(dbg_me, "gmv_ratio=%d, gmv_ratio_gain=%d\n", gmv_ratio, gmv_ratio_gain);
	pr_frc(dbg_me, "TC_th_s=%d, TC_th_l=%d\n", TC_th_s, TC_th_l);
	pr_frc(dbg_me, "mc_fallback_level=%d, fb_level=%d\n", mc_fallback_level, fallback_level[mc_fallback_level]);
	
	
}

void scene_detect(struct frc_dev_s *frc_devp)
{
	u8  reg_me_scn_chg_flg = READ_FRC_REG(FRC_ME_GCV_EN)>>17 & 0x1;
	struct frc_fw_data_s *frc_data;
	struct st_me_ctrl_para *g_stMeCtrl_Para;
	struct st_me_ctrl_item *g_stMeCtrl_Item;
	struct st_film_detect_rd *g_stFilmDetect_RD;
	struct st_frc_rd *g_stFrc_RD;

	frc_data = (struct frc_fw_data_s *)frc_devp->fw_data;
	g_stMeCtrl_Para = &frc_data->g_stMeCtrl_Para;
	g_stMeCtrl_Item = &frc_data->g_stMeCtrl_Item;
	g_stFilmDetect_RD = &frc_data->g_stfilmdetect_rd;
	g_stFrc_RD = &frc_data->g_stfrc_rd;

	if( (g_stFrc_RD->clear_vbuffer_en&&g_stFilmDetect_RD->glb_motion_all_film>=0x100) || reg_me_scn_chg_flg ) {
		if(g_stMeCtrl_Item->scene_change_reject_frame_count > 0)
			g_stMeCtrl_Item->scene_change_reject_frame_count--;
		else {
			g_stMeCtrl_Para->scene_change_flag = 1;
			g_stMeCtrl_Item->scene_change_reject_frame_count = 40;
		}
	}
	else if(g_stMeCtrl_Item->scene_change_reject_frame_count > 0) {
		g_stMeCtrl_Item->scene_change_reject_frame_count--;
		g_stMeCtrl_Para->scene_change_flag = 1;
	}
	else {
		g_stMeCtrl_Para->scene_change_flag = 0;
	}
}

void me_set_update_strength(s32 add_value)
{
	u8 loop_idx,k, add_rad_en[NUM_OF_LOOP][4];
	u32 tmp_value;
	u8  base_me_rad_x1_min[4] = {1,1,1,1};
	u8  base_me_rad_y1_min[4] = {1,1,1,1};
	u8  base_me_rad_x2_min[4] = {1,1,1,1};
	u8  base_me_rad_y2_min[4] = {1,1,1,1};
	u8  base_me_rad_x1_max[4] = {1,5,1,1};
	u8  base_me_rad_y1_max[4] = {1,4,1,1};
	u8  base_me_rad_x2_max[4] = {3,11,2,2};
	u8  base_me_rad_y2_max[4] = {2,8,2,2};

	for(loop_idx = 0;loop_idx<NUM_OF_LOOP;loop_idx++) {
		add_rad_en[loop_idx][0] = READ_FRC_REG(FRC_ME_CAD_RAD_EN_0 + loop_idx*(FRC_ME_CAD_RAD_EN_1-FRC_ME_CAD_RAD_EN_0))>>3 &0x1;
		add_rad_en[loop_idx][1] = READ_FRC_REG(FRC_ME_CAD_RAD_EN_0 + loop_idx*(FRC_ME_CAD_RAD_EN_1-FRC_ME_CAD_RAD_EN_0))>>2 &0x1;
		add_rad_en[loop_idx][2] = READ_FRC_REG(FRC_ME_CAD_RAD_EN_0 + loop_idx*(FRC_ME_CAD_RAD_EN_1-FRC_ME_CAD_RAD_EN_0))>>1 &0x1;
		add_rad_en[loop_idx][3] = READ_FRC_REG(FRC_ME_CAD_RAD_EN_0 + loop_idx*(FRC_ME_CAD_RAD_EN_1-FRC_ME_CAD_RAD_EN_0))    &0x1;

		for(k = 0;k<4;k++) {
			if(add_rad_en[loop_idx][k] == 1) {
				tmp_value   =   MIN(MAX(base_me_rad_x1_min[k] + add_value, 1), 15);
				UPDATE_FRC_REG_BITS(FRC_ME_CAD_RAD0_MM_0 + loop_idx*(FRC_ME_CAD_RAD0_MM_1-FRC_ME_CAD_RAD0_MM_0) + k*(FRC_ME_CAD_RAD1_MM_0-FRC_ME_CAD_RAD0_MM_0), tmp_value<<28, 0xF0000000);
				tmp_value   =   MIN(MAX(base_me_rad_y1_min[k] + add_value, 1), 15);
				UPDATE_FRC_REG_BITS(FRC_ME_CAD_RAD0_MM_0 + loop_idx*(FRC_ME_CAD_RAD0_MM_1-FRC_ME_CAD_RAD0_MM_0) + k*(FRC_ME_CAD_RAD1_MM_0-FRC_ME_CAD_RAD0_MM_0), tmp_value<<24, 0xF000000 );
				tmp_value   =   MIN(MAX(base_me_rad_x2_min[k] + add_value, 1), 15);
				UPDATE_FRC_REG_BITS(FRC_ME_CAD_RAD0_MM_0 + loop_idx*(FRC_ME_CAD_RAD0_MM_1-FRC_ME_CAD_RAD0_MM_0) + k*(FRC_ME_CAD_RAD1_MM_0-FRC_ME_CAD_RAD0_MM_0), tmp_value<<20, 0xF00000  );
				tmp_value   =   MIN(MAX(base_me_rad_y2_min[k] + add_value, 1), 15);
				UPDATE_FRC_REG_BITS(FRC_ME_CAD_RAD0_MM_0 + loop_idx*(FRC_ME_CAD_RAD0_MM_1-FRC_ME_CAD_RAD0_MM_0) + k*(FRC_ME_CAD_RAD1_MM_0-FRC_ME_CAD_RAD0_MM_0), tmp_value<<16, 0xF0000   );
				tmp_value   =   MIN(MAX(base_me_rad_x1_max[k] + add_value, 1), 15);
				UPDATE_FRC_REG_BITS(FRC_ME_CAD_RAD0_MM_0 + loop_idx*(FRC_ME_CAD_RAD0_MM_1-FRC_ME_CAD_RAD0_MM_0) + k*(FRC_ME_CAD_RAD1_MM_0-FRC_ME_CAD_RAD0_MM_0), tmp_value<<12, 0xF000    );
				tmp_value   =   MIN(MAX(base_me_rad_y1_max[k] + add_value, 1), 15);
				UPDATE_FRC_REG_BITS(FRC_ME_CAD_RAD0_MM_0 + loop_idx*(FRC_ME_CAD_RAD0_MM_1-FRC_ME_CAD_RAD0_MM_0) + k*(FRC_ME_CAD_RAD1_MM_0-FRC_ME_CAD_RAD0_MM_0), tmp_value<<8 , 0xF00     );
				tmp_value   =   MIN(MAX(base_me_rad_x2_max[k] + add_value, 1), 15);
				UPDATE_FRC_REG_BITS(FRC_ME_CAD_RAD0_MM_0 + loop_idx*(FRC_ME_CAD_RAD0_MM_1-FRC_ME_CAD_RAD0_MM_0) + k*(FRC_ME_CAD_RAD1_MM_0-FRC_ME_CAD_RAD0_MM_0), tmp_value<<4 , 0xF0      );
				tmp_value   =   MIN(MAX(base_me_rad_y2_max[k] + add_value, 1), 15);
				UPDATE_FRC_REG_BITS(FRC_ME_CAD_RAD0_MM_0 + loop_idx*(FRC_ME_CAD_RAD0_MM_1-FRC_ME_CAD_RAD0_MM_0) + k*(FRC_ME_CAD_RAD1_MM_0-FRC_ME_CAD_RAD0_MM_0), tmp_value    , 0xF      );
			}
		}
	}
}
void me_set_zmv_penalty(u32 set_value)
{
    u8 k;

	for(k=0; k<NUM_OF_LOOP; k++)
		UPDATE_FRC_REG_BITS(FRC_ME_CAD_ZGMV_EN_0+k*(FRC_ME_CAD_ZGMV_EN_1-FRC_ME_CAD_ZGMV_EN_0), set_value<<24, 0xFF000000);
}

void me_set_mvdf_smooth_wt(u32 set_value)
{
	u8 k;

	for(k=0; k<NUM_OF_LOOP; k++)
		UPDATE_FRC_REG_BITS(FRC_ME_P1_EN_0+k*(FRC_ME_P1_EN_1-FRC_ME_P1_EN_0), set_value<<27, 0x8000000);
}

void me_set_limit(u32 set_value)
{
	u8 k;

	for(k=0; k<NUM_OF_LOOP; k++)
		UPDATE_FRC_REG_BITS(FRC_MEPP_LMT_MOTION_0+k*(FRC_MEPP_LMT_MOTION_1-FRC_MEPP_LMT_MOTION_0), set_value<<22, 0x400000);
}

void me_set_smobj(u32 set_value)
{
	UPDATE_FRC_REG_BITS(FRC_MEPP_SMOB_EN, set_value<<31, 0x80000000);
}

void me_set_obme_size_min(u32 set_value)
{
	u8 k;

	for(k=0; k<NUM_OF_LOOP; k++)
	{
		UPDATE_FRC_REG_BITS(FRC_ME_OBME_MODE_0+k*(FRC_ME_OBME_MODE_1-FRC_ME_OBME_MODE_0),set_value<<4, 0x70);
	}
}

void me_set_obme_size_max(u32 set_value)
{
	u8 k;

	for(k=0; k<NUM_OF_LOOP; k++)
	{
		UPDATE_FRC_REG_BITS(FRC_ME_OBME_MODE_0+k*(FRC_ME_OBME_MODE_1-FRC_ME_OBME_MODE_0),set_value<<8, 0x700);
	}
}

void me_set_periodic_0(u32 set_value)
{
	u8 k;

	for(k=0; k<NUM_OF_LOOP; k++)
		UPDATE_FRC_REG_BITS(FRC_ME_P0_EN_0+k*(FRC_ME_P0_EN_1-FRC_ME_P0_EN_0), set_value<<31, 0x80000000);
}

void me_set_clear_vbuf_sad_en(u32 set_value)
{
	if (set_value == 0)
		UPDATE_FRC_REG_BITS(FRC_ME_GCV_EN, 0<<30, 0x40000000);
	else
		UPDATE_FRC_REG_BITS(FRC_ME_GCV_EN, 1<<30, 0x40000000);
}

void me_set_clear_vbuf_gmv_static_en(u32 set_value)
{
	UPDATE_FRC_REG_BITS(FRC_ME_GCV_EN, set_value<<23, 0x800000);
}

void me_set_clear_vbuf_top_en(u32 set_value)
{
	UPDATE_FRC_REG_BITS(FRC_ME_GCV_EN, set_value<<16, 0x10000);
}

void me_rule_ctrl_init(struct frc_dev_s *frc_devp)
{
	struct frc_fw_data_s *frc_data;
	struct st_me_ctrl_para *g_stMeCtrl_Para;

	frc_data = (struct frc_fw_data_s *)frc_devp->fw_data;
	g_stMeCtrl_Para = &frc_data->g_stMeCtrl_Para;

	g_stMeCtrl_Para->update_strength_add_value = 0;
	me_set_update_strength(g_stMeCtrl_Para->update_strength_add_value);
	me_set_zmv_penalty(18);
	me_set_mvdf_smooth_wt(1);
	me_set_limit(1);
	me_set_smobj(1);
	me_set_obme_size_min(1);
	me_set_obme_size_max(1);
	me_set_clear_vbuf_sad_en(0);
	me_set_clear_vbuf_gmv_static_en(0);
	me_set_periodic_0(0);
}

void me_rule_ctrl(struct frc_dev_s *frc_devp)
{
	u32 abs_gmv_x, abs_gmv_y, demo_window_en;
	u32 mvx_div_mode, mvy_div_mode, film_mode, total_blk_cnt;
	u8  mc_demo_window1_en, mc_demo_window2_en, mc_demo_window3_en, mc_demo_window4_en, film_mix_mode_fw;
	u8  mc_fallback_level, me_stat_region_xyxy[4];
	u32 glb_mot_all_film;

	struct frc_fw_data_s *frc_data;
	struct st_me_ctrl_para *g_stMeCtrl_Para;
	struct st_scene_change_detect_para *g_stScnChgDet_Para;
	//struct st_fb_ctrl_item *g_stFbCtrl_Item;
	struct st_fb_ctrl_para *g_stFbCtrl_Para;
	struct st_me_ctrl_item *g_stMeCtrl_Item;
	//struct st_region_fb_ctrl_item *g_stRegionFbCtrl_Item;
	struct st_me_rd *g_stME_RD;
	struct st_me_rule_en *g_stMeRule_EN;

	frc_data = (struct frc_fw_data_s *)frc_devp->fw_data;
	g_stMeCtrl_Para = &frc_data->g_stMeCtrl_Para;
	g_stScnChgDet_Para = &frc_data->g_stScnChgDet_Para;
	g_stFbCtrl_Para = &frc_data->g_stFbCtrl_Para;
	g_stMeCtrl_Item = &frc_data->g_stMeCtrl_Item;
	g_stME_RD = &frc_data->g_stme_rd;
	g_stMeRule_EN = &frc_data->g_stMeRule_EN;

	mvx_div_mode =  READ_FRC_REG(FRC_ME_EN)>>4 & 0x3;
	mvy_div_mode =  READ_FRC_REG(FRC_ME_EN)    & 0x3;
	abs_gmv_x = ABS(g_stME_RD->gmvx)>>(mvx_div_mode+ME_MV_FRAC_BIT);
	abs_gmv_y = ABS(g_stME_RD->gmvy)>>(mvy_div_mode+ME_MV_FRAC_BIT);

	u8 gmv_ratio = (g_stME_RD->gmv_total_sum==0) ? 0 : ((100*g_stME_RD->gmv_neighbor_cnt)/g_stME_RD->gmv_total_sum);
	me_stat_region_xyxy[0] = READ_FRC_REG(FRC_ME_STAT_12R_HST)     & 0x3FF;
	me_stat_region_xyxy[1] = READ_FRC_REG(FRC_ME_STAT_12R_V0)>>16  & 0x3FF;
	me_stat_region_xyxy[2] = READ_FRC_REG(FRC_ME_STAT_12R_H23)     & 0x3FF;
	me_stat_region_xyxy[3] = READ_FRC_REG(FRC_ME_STAT_12R_V1)      & 0x3FF;
	total_blk_cnt = (me_stat_region_xyxy[2]-me_stat_region_xyxy[0])*(me_stat_region_xyxy[3]-me_stat_region_xyxy[1]);
	u8 dtl_blk_ratio = (total_blk_cnt==0)? 0 : (100*g_stME_RD->gmv_total_sum/total_blk_cnt);
	glb_mot_all_film = READ_FRC_REG(FRC_FD_DIF_GL_FILM);

	me_rule_ctrl_init(frc_devp);

	if(g_stMeCtrl_Para->me_rule_ctrl_en == 1) {
		//rule_1: static scene-->small random & zmv_penalty=0
		if (g_stMeRule_EN->rule1_en == 1) {
        	if(glb_mot_all_film < 0x500) {
			g_stMeCtrl_Para->update_strength_add_value = -16;
			me_set_update_strength(g_stMeCtrl_Para->update_strength_add_value);

				g_stMeCtrl_Item->static_scene_frame_count = (g_stMeCtrl_Item->static_scene_frame_count < 60) ?
																(g_stMeCtrl_Item->static_scene_frame_count + 1) : 60;
			} else {
				g_stMeCtrl_Item->static_scene_frame_count = (g_stMeCtrl_Item->static_scene_frame_count > 0) ?
																(g_stMeCtrl_Item->static_scene_frame_count - 1) : 0;
			}
			if (g_stMeCtrl_Item->static_scene_frame_count >= 60) { //iir in, no iir out
				me_set_zmv_penalty(0);
			}
		}

		// rule_2, when scene change, big apl -->increase random, small apl-->decrease random
		film_mode =  READ_FRC_REG(FRC_REG_PHS_TABLE)>>8 & 0xFF;
		if (g_stMeRule_EN->rule2_en == 1) {
			if (g_stMeCtrl_Para->scene_change_flag&& film_mode!=0) {
			g_stMeCtrl_Item->scene_change_catchin_frame_count = 20;
		}

			if (g_stMeCtrl_Item->scene_change_catchin_frame_count > 0) {
			g_stMeCtrl_Para->update_strength_add_value = 3;
			me_set_update_strength(g_stMeCtrl_Para->update_strength_add_value);

			me_set_mvdf_smooth_wt(0);

			me_set_limit(0);

			me_set_smobj(0);

			me_set_obme_size_min(3);
			me_set_obme_size_max(3);

			g_stMeCtrl_Item->scene_change_catchin_frame_count --;
		}
		}

		//rule_3, when scene change--> enable spatial sad rule for clear v-buf
		if (g_stMeRule_EN->rule3_en == 1) {
			if (g_stMeCtrl_Para->scene_change_flag == 1) {
			g_stMeCtrl_Item->scene_change_frame_count = 8;
		}

			if (g_stMeCtrl_Item->scene_change_frame_count > 0) {
			me_set_clear_vbuf_sad_en(1);
			g_stMeCtrl_Item->scene_change_frame_count --;
		}
		}

		//rule_4, static scene--> enable glb_mot rule for clear v-buf
        if (g_stMeRule_EN->rule4_en == 1) {
        	if (glb_mot_all_film < 0x10) {
			me_set_clear_vbuf_gmv_static_en(1);
		}
		}

		//rule_5, pure panning--> enable periodic0
		if (g_stMeRule_EN->rule5_en == 1) {
			if ((gmv_ratio >96)&&(g_stME_RD->glb_t_consis[0]< 0xF00)) {
			me_set_periodic_0(1);
		}
		}

		//rule_6, MV> search range -->FB
		if (g_stMeRule_EN->rule6_en == 1) {
			if ((gmv_ratio > 85)&&(abs_gmv_x > g_stMeCtrl_Para->fallback_gmvx_th ||abs_gmv_y > g_stMeCtrl_Para->fallback_gmvy_th)) {
			mc_fallback_level = g_stFbCtrl_Para->fallback_level_max;
		}
		}

		//rule_7, scene change--->two frame FB
		if (g_stMeRule_EN->rule7_en == 1) {
			if (g_stMeCtrl_Para->scene_change_flag == 1) {
			g_stMeCtrl_Item->scene_change_judder_frame_count = 20;
		}

			if (g_stMeCtrl_Item->scene_change_judder_frame_count > 0) {
			mc_fallback_level = g_stFbCtrl_Para->fallback_level_max;
			g_stMeCtrl_Item->scene_change_judder_frame_count--;
		}
		}

		//rule_8, demo window||mix mode --->disable clear v-buf
		mc_demo_window1_en = READ_FRC_REG(FRC_MC_DEMO_WINDOW)>>3 & 0x1;
		mc_demo_window2_en = READ_FRC_REG(FRC_MC_DEMO_WINDOW)>>2 & 0x1;
		mc_demo_window3_en = READ_FRC_REG(FRC_MC_DEMO_WINDOW)>>1 & 0x1;
		mc_demo_window4_en = READ_FRC_REG(FRC_MC_DEMO_WINDOW)    & 0x1;
		demo_window_en = mc_demo_window1_en||mc_demo_window2_en||mc_demo_window3_en||mc_demo_window4_en;

		film_mix_mode_fw = READ_FRC_REG(FRC_REG_FILM_PHS_1)>>17 & 0x1;
		if (g_stMeRule_EN->rule8_en == 1) {
			if ((demo_window_en ==1)&&(film_mix_mode_fw==1)) {
				g_stMeCtrl_Item->mixmodein_frame_count = (g_stMeCtrl_Item->mixmodein_frame_count < 20) ?
															(g_stMeCtrl_Item->mixmodein_frame_count + 1) : 120;
			} else {
				g_stMeCtrl_Item->mixmodein_frame_count = (g_stMeCtrl_Item->mixmodein_frame_count > 0) ?
															(g_stMeCtrl_Item->mixmodein_frame_count - 1) : 0;
			}
			if (g_stMeCtrl_Item->mixmodein_frame_count > 20) {  //iir in, iir out
				mc_fallback_level = 0;
				me_set_clear_vbuf_top_en(0);
			}
		}

		//rule_9, for fast panning, control zero panalty
		if (g_stMeRule_EN->rule9_en == 1) {
			if (gmv_ratio >90 && dtl_blk_ratio <60 &&g_stME_RD->glb_sad_sum[0]< 0x2000)
			me_set_zmv_penalty(255);
		}
		//rule_10, scene change --> dehalo off
		if (g_stMeRule_EN->rule10_en == 1) {
			if (g_stMeCtrl_Para->scene_change_flag == 1)
				g_stMeCtrl_Item->scene_change_dehalooff_frame_count = 8;

			if (g_stMeCtrl_Item->scene_change_dehalooff_frame_count > 0) {
				frc_set_global_dehalo_en(0);
				g_stMeCtrl_Item->scene_change_dehalooff_frame_count--;
			}
		}
	}
}

void region_fb_ctrl(struct frc_dev_s *frc_devp)
{
	u8 	region_TC_gain_buf[9] = {24, 16, 8, 8, 8, 8, 8, 8, 8};
	u8  k, i, j, win_x, win_y, iii, jjj, tc_iir_up, tc_iir_dn, fb_level_iir_up, fb_level_iir_dn;
	u8  cur_idxi, cur_idxj, y_index_type, x_index_type, region_y_index_shift[4], region_x_index_shift[4], max_level_value, cur_level_value;
	u32 gmv_ratio_gain, region_TC_bad_cnt, region_TC_gain;
	u32 dil_cnt, ero_cnt, max_level, min_level, region_fb_xnum, region_fb_ynum;
	u32 region_TC_iir[6][8], region_TC_th_s[6][8], region_TC_th_l[6][8];
	u8  region_fb_level[6][8], region_fb_level_pp[6][8], region_fb_level_ext[8][12];
	u8  mc_region_fb_bitwidth, region_fallback;

	struct frc_fw_data_s *frc_data;
	struct st_me_ctrl_para *g_stMeCtrl_Para;
	struct st_scene_change_detect_para *g_stScnChgDet_Para;
	struct st_fb_ctrl_item *g_stFbCtrl_Item;
	struct st_fb_ctrl_para *g_stFbCtrl_Para;
	struct st_region_fb_ctrl_item *g_stRegionFbCtrl_Item;
	struct st_me_rd *g_stME_RD;

	frc_data = (struct frc_fw_data_s *)frc_devp->fw_data;
	g_stMeCtrl_Para = &frc_data->g_stMeCtrl_Para;
	g_stScnChgDet_Para = &frc_data->g_stScnChgDet_Para;
	g_stFbCtrl_Item = &frc_data->g_stFbCtrl_Item;
	g_stFbCtrl_Para = &frc_data->g_stFbCtrl_Para;
	g_stRegionFbCtrl_Item = &frc_data->g_stRegionFbCtrl_Item;
	g_stME_RD = &frc_data->g_stme_rd;

	region_fb_ynum = READ_FRC_REG(FRC_ME_STAT_NEW_REGION)>>18 &0xF;
	region_fb_xnum = READ_FRC_REG(FRC_ME_STAT_NEW_REGION)>>22 &0xF;
	mc_region_fb_bitwidth = READ_FRC_REG(FRC_MC_REGION_FALLBACK)>>4 &0xF;
	tc_iir_up = g_stFbCtrl_Para->region_TC_iir_up;
	tc_iir_dn = g_stFbCtrl_Para->region_TC_iir_dn;
	fb_level_iir_up = g_stFbCtrl_Para->region_fb_level_iir_up;
	fb_level_iir_dn = g_stFbCtrl_Para->region_fb_level_iir_dn;

	u8 gmv_ratio = (g_stME_RD->gmv_total_sum==0) ? 0 : ((100*g_stME_RD->gmv_neighbor_cnt)/g_stME_RD->gmv_total_sum);

	for(i=0; i<region_fb_ynum; i++)
	{
		for(j=0; j<region_fb_xnum; j++)
		{
			region_TC_bad_cnt = 0;

			//cal region_tc_bad_cnt : 4x6 consider 3x3, 6*8 consider 5*5??
			for(win_y=-1; win_y<=1; win_y++)
			{
				for(win_x=-1; win_x<=1; win_x++)
				{
					if(win_x!=0 ||win_y!=0)
					{
						iii = MIN(MAX(i+win_y, 0),region_fb_ynum-1);
						jjj = MIN(MAX(j+win_x, 0),region_fb_xnum-1);

						if(g_stME_RD->region_fb_t_consis[iii][jjj] >g_stFbCtrl_Para->region_TC_bad_th)
							region_TC_bad_cnt++;
					}
				}
			}

			//cal region tc iir
			if(g_stME_RD->region_fb_t_consis[i][j] > g_stRegionFbCtrl_Item->region_TC_iir_pre[i][j])
				region_TC_iir[i][j] = (g_stRegionFbCtrl_Item->region_TC_iir_pre[i][j]*tc_iir_up + g_stME_RD->region_fb_t_consis[i][j]*(256-tc_iir_up) + 128)>>8;
			else
				region_TC_iir[i][j] = (g_stRegionFbCtrl_Item->region_TC_iir_pre[i][j]*tc_iir_dn + g_stME_RD->region_fb_t_consis[i][j]*(256-tc_iir_dn) + 128)>>8;

			g_stRegionFbCtrl_Item->region_TC_iir_pre[i][j] = region_TC_iir[i][j];


			//cal tc_th_s / tc_th_l
			if(gmv_ratio >= g_stFbCtrl_Para->region_fb_gain_gmv_th_l)
				gmv_ratio_gain = g_stFbCtrl_Para->region_fb_gain_gmv_ratio_l;
			else if(gmv_ratio <= g_stFbCtrl_Para->region_fb_gain_gmv_th_s)
				gmv_ratio_gain = g_stFbCtrl_Para->region_fb_gain_gmv_ratio_s;
			else
			{
				if(g_stFbCtrl_Para->region_fb_gain_gmv_th_l == g_stFbCtrl_Para->region_fb_gain_gmv_th_s)
					gmv_ratio_gain = g_stFbCtrl_Para->region_fb_gain_gmv_ratio_s;
				else
					gmv_ratio_gain = g_stFbCtrl_Para->region_fb_gain_gmv_ratio_s +
                                    (gmv_ratio-g_stFbCtrl_Para->region_fb_gain_gmv_th_s)*
                                    (g_stFbCtrl_Para->region_fb_gain_gmv_ratio_l-g_stFbCtrl_Para->region_fb_gain_gmv_ratio_s)/
                                    (g_stFbCtrl_Para->region_fb_gain_gmv_th_l-g_stFbCtrl_Para->region_fb_gain_gmv_th_s);
			}

			region_TC_gain = region_TC_gain_buf[region_TC_bad_cnt];

			region_TC_th_s[i][j] = (g_stFbCtrl_Para->base_region_TC_th_s*gmv_ratio_gain*region_TC_gain)>>6;
			region_TC_th_l[i][j] = (g_stFbCtrl_Para->base_region_TC_th_l*gmv_ratio_gain*region_TC_gain)>>6;

			if(region_TC_th_s[i][j]  > g_stRegionFbCtrl_Item->region_TC_th_s_pre[i][j])
			{
				region_TC_th_s[i][j] = (g_stRegionFbCtrl_Item->region_TC_th_s_pre[i][j]*g_stFbCtrl_Para->region_TC_th_iir_up + region_TC_th_s[i][j]*(256-g_stFbCtrl_Para->region_TC_th_iir_up) + 128)>>8;
				region_TC_th_l[i][j] = (g_stRegionFbCtrl_Item->region_TC_th_l_pre[i][j]*g_stFbCtrl_Para->region_TC_th_iir_up + region_TC_th_l[i][j]*(256-g_stFbCtrl_Para->region_TC_th_iir_up) + 128)>>8;
			}
			else
			{
				region_TC_th_s[i][j] = (g_stRegionFbCtrl_Item->region_TC_th_s_pre[i][j]*g_stFbCtrl_Para->region_TC_th_iir_dn + region_TC_th_s[i][j]*(256-g_stFbCtrl_Para->region_TC_th_iir_dn) + 128)>>8;
				region_TC_th_l[i][j] = (g_stRegionFbCtrl_Item->region_TC_th_l_pre[i][j]*g_stFbCtrl_Para->region_TC_th_iir_dn + region_TC_th_l[i][j]*(256-g_stFbCtrl_Para->region_TC_th_iir_dn) + 128)>>8;
			}

			g_stRegionFbCtrl_Item->region_TC_th_s_pre[i][j] = region_TC_th_s[i][j];
			g_stRegionFbCtrl_Item->region_TC_th_l_pre[i][j] = region_TC_th_l[i][j];


			//cal region fb level
			if(region_TC_iir[i][j] < g_stRegionFbCtrl_Item->region_TC_th_s_pre[i][j])
				region_fb_level[i][j] = 0;
			else if(region_TC_iir[i][j] > g_stRegionFbCtrl_Item->region_TC_th_l_pre[i][j])
				region_fb_level[i][j] = g_stFbCtrl_Para->fallback_level_max;
			else
			{
				if(g_stRegionFbCtrl_Item->region_TC_th_s_pre[i][j] == g_stRegionFbCtrl_Item->region_TC_th_l_pre[i][j])
					region_fb_level[i][j] = 0;
				else
					region_fb_level[i][j] = g_stFbCtrl_Para->fallback_level_max*(region_TC_iir[i][j] - g_stRegionFbCtrl_Item->region_TC_th_s_pre[i][j])/(g_stRegionFbCtrl_Item->region_TC_th_l_pre[i][j] - g_stRegionFbCtrl_Item->region_TC_th_s_pre[i][j]);
			}

			//cal region fb level iir
			if(region_fb_level[i][j] > g_stRegionFbCtrl_Item->region_fb_level_pre[i][j])
				region_fb_level[i][j] = (g_stRegionFbCtrl_Item->region_fb_level_pre[i][j]*fb_level_iir_up + region_fb_level[i][j]*(256-fb_level_iir_up) + 128)>>8;
			else
				region_fb_level[i][j] = (g_stRegionFbCtrl_Item->region_fb_level_pre[i][j]*fb_level_iir_dn + region_fb_level[i][j]*(256-fb_level_iir_dn) + 128)>>8;

			g_stRegionFbCtrl_Item->region_fb_level_pre[i][j] = region_fb_level[i][j];
		}
	}

	for(i=0; i<region_fb_ynum; i++)
	{
		for(j=0; j<region_fb_xnum; j++)
		{
			region_fb_level_pp[i][j] = region_fb_level[i][j];

			dil_cnt = ero_cnt = 0;
			max_level = 0;
			min_level = 11;
			for ( win_y = -1; win_y <= 1; win_y++ )
			{
				for ( win_x = -1; win_x <= 1; win_x++ )
				{
					if(win_x!=0 ||win_y!=0)
					{
						iii = MIN(MAX(i+win_y, 0),region_fb_ynum-1);
						jjj = MIN(MAX(j+win_x, 0),region_fb_xnum-1);


						if(region_fb_level[iii][jjj] > g_stFbCtrl_Para->region_fb_level_b_th)
							dil_cnt++;

						if(region_fb_level[iii][jjj] < g_stFbCtrl_Para->region_fb_level_s_th)
							ero_cnt++;

						max_level = (region_fb_level[iii][jjj] > max_level)? region_fb_level[iii][jjj] :max_level ;
						min_level = (region_fb_level[iii][jjj] < min_level)? region_fb_level[iii][jjj] :min_level ;
					}
				} //end of win_x
			}// end of win_y

			if(ero_cnt > g_stFbCtrl_Para->region_fb_level_ero_cnt_b_th && dil_cnt < g_stFbCtrl_Para->region_fb_level_dil_cnt_s_th
					&& region_fb_level[i][j] < g_stFbCtrl_Para->region_fb_level_b_th)
				region_fb_level_pp[i][j] = min_level;

			if(dil_cnt> g_stFbCtrl_Para->region_fb_level_dil_cnt_b_th&&region_fb_level[i][j] > g_stFbCtrl_Para->region_fb_level_s_th)
				region_fb_level_pp[i][j] = max_level;

			region_fb_level_pp[i][j] = (g_stME_RD->region_fb_dtl_sum[i][j] > g_stFbCtrl_Para->region_dtl_sum_th)?region_fb_level_pp[i][j] :0;
		}
	}

	// extend fb_level_pp 4x6 to fb_level_extend 8x12
	for(i=0; i<2*region_fb_ynum; i++)
	{
		for(j=0; j<2*region_fb_xnum; j++)
		{
			cur_idxi = i/2;
			cur_idxj = j/2;
			y_index_type = i%2;
			x_index_type = j%2;
			if(y_index_type==0)
			{
				region_y_index_shift[0] = -1;
				region_y_index_shift[1] = -1;
				region_y_index_shift[2] = 0;
				region_y_index_shift[3] = 0;
			}
			else
			{
				region_y_index_shift[0] = 0;
				region_y_index_shift[1] = 0;
				region_y_index_shift[2] = 1;
				region_y_index_shift[3] = 1;
			}

			if(x_index_type==0)
			{
				region_x_index_shift[0] = -1;
				region_x_index_shift[1] = 0;
				region_x_index_shift[2] = -1;
				region_x_index_shift[3] = 0;
			}
			else
			{
				region_x_index_shift[0] = 0;
				region_x_index_shift[1] = 1;
				region_x_index_shift[2] = 0;
				region_x_index_shift[3] = 1;
			}

			if(region_fb_level_pp[cur_idxi][cur_idxj]>=g_stFbCtrl_Para->region_fb_ext_th)  //only extend itself
			{
				region_fb_level_ext[i][j] = region_fb_level_pp[cur_idxi][cur_idxj];
			}
			else                                                                                //select max value and blend with current block
			{
				max_level_value = 0;
				cur_level_value = 0;
				for(k=0;k<4;k++)
				{
					iii = MIN(MAX(cur_idxi+region_y_index_shift[k], 0), region_fb_ynum-1);
					jjj = MIN(MAX(cur_idxj+region_x_index_shift[k], 0), region_fb_xnum-1);
					if(region_y_index_shift[k]!=0 || region_x_index_shift[k]!=0)
						max_level_value = region_fb_level_pp[iii][jjj]>max_level_value ? region_fb_level_pp[iii][jjj] : max_level_value;
					else
						cur_level_value = region_fb_level_pp[cur_idxi][cur_idxj];
				}
				region_fb_level_ext[i][j] = (g_stFbCtrl_Para->region_fb_ext_coef*cur_level_value + (8-g_stFbCtrl_Para->region_fb_ext_coef)*max_level_value + 4)>>3;
			}
			region_fallback = (fallback_level[region_fb_level_ext[i][j]])<<(mc_region_fb_bitwidth-4) ;

			if(i<4)
				UPDATE_FRC_REG_BITS(FRC_MC_REGION_COEF2_0+j*(FRC_MC_REGION_COEF2_1-FRC_MC_REGION_COEF2_0),region_fallback<<(8*i),(1<<(8*(i+1)))-(1<<(8*i)));
			else
				UPDATE_FRC_REG_BITS(FRC_MC_REGION_COEF1_0+j*(FRC_MC_REGION_COEF1_1-FRC_MC_REGION_COEF1_0),region_fallback<<(8*(i-4)),(1<<(8*(i-4+1)))-(1<<(8*(i-4))));
		}
	}
}

void me_add_7_seg(struct frc_dev_s *frc_devp)
{
	u32 mvx_div_mode, mvy_div_mode;
	s32 gmvx, gmvy, mv_abs, mv_100, mv_10, mv_1, mv_sign;
	u32 cnt10k, cnt1k, cnt100, cnt10, cnt;
	u8 	global_fb_level, data_10, data_1, data_tmp;
	u8  clr_vbuf_trigger, clr_vbuf_en;
	struct frc_fw_data_s *frc_data;
	struct st_me_rd *g_stME_RD;
	struct st_me_ctrl_para *g_stMeCtrl_Para;
	struct st_fb_ctrl_item *g_stFbCtrl_Item;

	frc_data = (struct frc_fw_data_s *)frc_devp->fw_data;
	g_stME_RD = &frc_data->g_stme_rd;
	g_stMeCtrl_Para = &frc_data->g_stMeCtrl_Para;
	g_stFbCtrl_Item = &frc_data->g_stFbCtrl_Item;

	if (g_stMeCtrl_Para->me_add_7_flag_mode == 1) {
		mvx_div_mode = READ_FRC_REG(FRC_ME_EN)>>4 & 0x3;
		mvy_div_mode = READ_FRC_REG(FRC_ME_EN)    & 0x3;

		UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_POSI_AND_NUM11_NUM12,1<<15,BIT_15);//flag1_1_en
		UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_POSI_AND_NUM11_NUM12,1<<7,BIT_7 );//flag1_2_en
		UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM13_NUM14_NUM15_NUM16,1<<31,BIT_31);//flag1_3_en
		UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM13_NUM14_NUM15_NUM16,1<<23,BIT_23);//flag1_4_en
		UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM13_NUM14_NUM15_NUM16,1<<15,BIT_15);//flag1_5_en
		UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM13_NUM14_NUM15_NUM16,1<<7 ,BIT_7 );//flag1_6_en
		UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM17_NUM18_NUM21_NUM22,1<<31,BIT_31);//flag1_7_en
		UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM17_NUM18_NUM21_NUM22,1<<23,BIT_23);//flag1_8_en
		UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM17_NUM18_NUM21_NUM22,1<<15,BIT_15);//flag2_1_en
		UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM17_NUM18_NUM21_NUM22,1<<7 ,BIT_7 );//flag2_2_en
		UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM23_NUM24_NUM25_NUM26,1<<31,BIT_31);//flag2_3_en
		UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM23_NUM24_NUM25_NUM26,1<<23,BIT_23);//flag2_4_en
		UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM23_NUM24_NUM25_NUM26,1<<15,BIT_15);//flag2_5_en
		UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM23_NUM24_NUM25_NUM26,1<<7 ,BIT_7 );//flag2_6_en
		UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM23_NUM24_NUM25_NUM26,1<<31,BIT_31);//flag2_7_en
		UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM23_NUM24_NUM25_NUM26,1<<23,BIT_23);//flag2_8_en

		cnt = g_stME_RD->gmv_total_sum;
		cnt10k   = cnt/10000;
		cnt      = (cnt - cnt10k  *10000);
		cnt1k    = cnt/1000;
		cnt      = (cnt - cnt1k   *1000);
		cnt100   = cnt/100;
		cnt      = (cnt - cnt100 *100);
		cnt10    = cnt/10;
		UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_POSI_AND_NUM11_NUM12,cnt10k<<8, 0xF00);
		UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_POSI_AND_NUM11_NUM12,cnt1k    , 0xF  );
		UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM13_NUM14_NUM15_NUM16,cnt100<<24 , 0xF000000);
		UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM13_NUM14_NUM15_NUM16,cnt10 <<16 , 0xF0000  );

		cnt = g_stME_RD->gmv_neighbor_cnt;
		cnt10k   = cnt/10000;
		cnt      = (cnt - cnt10k  *10000);
		cnt1k    = cnt/1000;
		cnt      = (cnt - cnt1k   *1000);
		cnt100   = cnt/100;
		cnt      = (cnt - cnt100 *100);
		cnt10    = cnt/10;
		UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM13_NUM14_NUM15_NUM16,cnt10k<<8 , 0xF00);
		UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM13_NUM14_NUM15_NUM16,cnt1k     , 0xF  );
		UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM17_NUM18_NUM21_NUM22,cnt100<<24, 0xF000000);
		UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM17_NUM18_NUM21_NUM22,cnt10<<16 , 0xF0000  );

		gmvx = g_stME_RD->gmvx >> (mvx_div_mode+2);
		gmvy = g_stME_RD->gmvy >> (mvy_div_mode+2);

		mv_sign = (gmvx>=0) ? 0 : 1;
		mv_abs      = ABS(gmvx);
		mv_100 = mv_abs/100;
		mv_10  = (mv_abs - mv_100*100 )/10;
		mv_1   =  mv_abs - mv_100*100 - mv_10*10;
		UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM17_NUM18_NUM21_NUM22, mv_sign<<8 , 0xF00    );
		UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM17_NUM18_NUM21_NUM22, mv_100     , 0xF      );
		UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM23_NUM24_NUM25_NUM26, mv_10 <<24 , 0xF000000);
		UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM23_NUM24_NUM25_NUM26, mv_1  <<16 , 0xF0000  );

		mv_sign = (gmvy>=0) ? 0 : 1;
		mv_abs  = ABS(gmvy);
		mv_100 = mv_abs/100;
		mv_10  = (mv_abs - mv_100*100 )/10;
		mv_1   =  mv_abs - mv_100*100 - mv_10*10;
		UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM23_NUM24_NUM25_NUM26, mv_sign <<8 , 0xF00);
		UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM23_NUM24_NUM25_NUM26, mv_100      , 0xF  );
		UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM27_NUM28            , mv_10   <<24, 0xF000000);
		UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM27_NUM28            , mv_1    <<16, 0xF0000 );
	}

	if (g_stMeCtrl_Para->me_add_7_flag_mode == 2) {
		UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_POSI_AND_NUM11_NUM12,1<<15,BIT_15);//flag1_1_en
		UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_POSI_AND_NUM11_NUM12,1<<7,BIT_7 );//flag1_2_en

		global_fb_level = g_stFbCtrl_Item->mc_fallback_level_pre;
		data_tmp = fallback_level[global_fb_level];
		data_10  = data_tmp/10;
		data_1	 = data_tmp - data_10*10;

		UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_POSI_AND_NUM11_NUM12,data_10<<8, 0xF00);
		UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_POSI_AND_NUM11_NUM12,data_1    , 0xF  );
	}

	if (g_stMeCtrl_Para->me_add_7_flag_mode == 3) {
		UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_POSI_AND_NUM11_NUM12,1<<15,BIT_15);//flag1_1_en
		UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_POSI_AND_NUM11_NUM12,1<<7,BIT_7 );//flag1_2_en

		clr_vbuf_trigger = READ_FRC_BITS(FRC_REG_FILM_PHS_2, 1, 1);
		clr_vbuf_en      = READ_FRC_BITS(FRC_REG_FILM_PHS_2, 0, 1);

		UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_POSI_AND_NUM11_NUM12, clr_vbuf_trigger<<8, 0xF00);
		UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_POSI_AND_NUM11_NUM12, clr_vbuf_en, 0xF  );
	}
	
}

void region_readback_control(struct frc_dev_s *frc_devp)
{
	u32 i, j, k, region_idx, median_num, ii, jj, i_tmp, j_tmp;
	u32 win3x3, region_fb_ynum, region_fb_xnum;
	u32 region_sad_sum_median[6][8], region_sad_cnt_median[6][8], region_s_consis_median[6][8];
	u8  region_ro_en[6][8], region_ro_pp[6][8];

	struct frc_fw_data_s *frc_data;
	struct st_me_ctrl_para *g_stMeCtrl_Para;
	struct st_me_ctrl_item *g_stMeCtrl_Item;
	struct st_me_rd *g_stME_RD;

	frc_data = (struct frc_fw_data_s *)frc_devp->fw_data;
	g_stMeCtrl_Para = &frc_data->g_stMeCtrl_Para;
	g_stMeCtrl_Item = &frc_data->g_stMeCtrl_Item;
	g_stME_RD = &frc_data->g_stme_rd;


	median_num = g_stMeCtrl_Para->region_sad_median_num;
	median_num = MIN(median_num,20);
	region_fb_ynum = READ_FRC_REG(FRC_ME_STAT_NEW_REGION)>>18 &0xF;
	region_fb_xnum = READ_FRC_REG(FRC_ME_STAT_NEW_REGION)>>22 &0xF;

	//  calc region_sad_sum median value
	for(i = 0; i < region_fb_ynum; i++)
	{
		for(j = 0; j < region_fb_xnum; j++)
		{
			region_idx = i*region_fb_xnum + j;
			for(k = 0; k < median_num-1; k++)
			{
				g_stMeCtrl_Item->region_sad_sum_20[region_idx][k] = g_stMeCtrl_Item->region_sad_sum_20[region_idx][k+1];
				g_stMeCtrl_Item->region_sad_cnt_20[region_idx][k] = g_stMeCtrl_Item->region_sad_cnt_20[region_idx][k+1];
				g_stMeCtrl_Item->region_s_consis_20[region_idx][k]= g_stMeCtrl_Item->region_s_consis_20[region_idx][k+1];
			}
			g_stMeCtrl_Item->region_sad_sum_20[region_idx][median_num-1] = g_stME_RD->region_fb_sad_sum[i][j];
			g_stMeCtrl_Item->region_sad_cnt_20[region_idx][median_num-1] = g_stME_RD->region_fb_sad_cnt[i][j];
			g_stMeCtrl_Item->region_s_consis_20[region_idx][median_num-1]= g_stME_RD->region_fb_s_consis[i][j];

			region_sad_sum_median[i][j] = fb_medianX(g_stMeCtrl_Item->region_sad_sum_20[region_idx],median_num);
			region_sad_cnt_median[i][j] = fb_medianX(g_stMeCtrl_Item->region_sad_cnt_20[region_idx],median_num);
			region_s_consis_median[i][j]= fb_medianX(g_stMeCtrl_Item->region_s_consis_20[region_idx],median_num);

			if((region_sad_sum_median[i][j] >= g_stMeCtrl_Para->region_sad_sum_th) &&
			   (region_sad_cnt_median[i][j] >= g_stMeCtrl_Para->region_sad_cnt_th) &&
			   (region_s_consis_median[i][j]>= g_stMeCtrl_Para->region_s_consis_th))
				region_ro_en[i][j] = 1;
			else
				region_ro_en[i][j] = 0;
		}
	}

	//post processing
	for(i = 0; i < region_fb_ynum; i++)
	{
		for(j = 0; j < region_fb_xnum; j++)
		{
			win3x3 = 0;
			for(ii = -1; ii <= 1; ii++)
			{
				i_tmp = MIN(MAX(i+ii, 0), region_fb_ynum-1);
				for(jj = -1; jj <= 1; jj++)
				{
					j_tmp = MIN(MAX(j+jj, 0), region_fb_xnum-1);
					win3x3 += (region_ro_en[i_tmp][j_tmp] == 1) ? 1 : 0;
				}
			}

			if(win3x3 <= g_stMeCtrl_Para->region_win3x3_min && region_ro_en[i][j] ==1)
				region_ro_pp[i][j] = 0;
			else if(win3x3 >= g_stMeCtrl_Para->region_win3x3_max && region_ro_en[i][j] ==0)
				region_ro_pp[i][j] = 1;
			else
				region_ro_pp[i][j] = region_ro_en[i][j];

			UPDATE_FRC_REG_BITS(FRC_ME_REGION_READONLY_EN_0 + i*(FRC_ME_REGION_READONLY_EN_1-FRC_ME_REGION_READONLY_EN_0), region_ro_pp[i][j]<<j, 1<<j);
		}
	}
}

void frc_me_ctrl(struct frc_dev_s *frc_devp)
{
	struct frc_fw_data_s *frc_data;
	struct st_me_ctrl_para *g_stMeCtrl_Para;

	frc_data = (struct frc_fw_data_s *)frc_devp->fw_data;
	g_stMeCtrl_Para = &frc_data->g_stMeCtrl_Para;

	if (g_stMeCtrl_Para->me_en == 1) {
		set_me_gmv(frc_devp);
		set_region_fb_size();
		set_me_apl(frc_devp);
		scene_change_detect(frc_devp);
		//scene_detect();
		fb_ctrl(frc_devp);
		//region_fb_ctrl();
		me_rule_ctrl(frc_devp);
		region_readback_control(frc_devp);
		me_add_7_seg(frc_devp);
		//check_ro();
		//xil_printf("me all finished\n\r");
	}
}

void check_ro(void)
{
	u8  k;
	u32 me_gmv_rough_cnt_3x3[9], me_gmv_cnt_3x3[9], me_gmv_rough_cnt, me_gmv_cnt;

	// 1.
	for(k=0; k<9; k++)
	{
		me_gmv_rough_cnt_3x3[k] = READ_FRC_REG(FRC_ME_RO_GMV_ROUGH_CNT_3X3_0 + k*(FRC_ME_RO_GMV_ROUGH_CNT_3X3_1-FRC_ME_RO_GMV_ROUGH_CNT_3X3_0)) & 0x3FFFF;
		me_gmv_cnt_3x3[k]       = READ_FRC_REG(FRC_ME_RO_GMV_CNT_3X3_0 + k*(FRC_ME_RO_GMV_CNT_3X3_1-FRC_ME_RO_GMV_CNT_3X3_0)) & 0x3FFFF;
	}
	me_gmv_rough_cnt = READ_FRC_REG(FRC_ME_RO_GMV_ROUGH_CNT) & 0x3FFFF;
	me_gmv_cnt       = READ_FRC_REG(FRC_ME_RO_GMV_CNT)       & 0x3FFFF;

	// 2.
	s32 me_gmvx_rough, me_gmvy_rough, me_gmvx_rough_2nd, me_gmvy_rough_2nd;
	s32 me_gmvx, me_gmvy, me_gmvx_2nd, me_gmvy_2nd, me_gmvx_mux, me_gmvy_mux;

	me_gmvx_rough        = READ_FRC_REG(FRC_ME_RO_GMV_ROUGH)>>16     & 0x1FFF;
	me_gmvy_rough        = READ_FRC_REG(FRC_ME_RO_GMV_ROUGH)         & 0xFFF ;
	me_gmvx_rough_2nd    = READ_FRC_REG(FRC_ME_RO_GMV_ROUGH_2ND)>>16 & 0x1FFF;
	me_gmvy_rough_2nd    = READ_FRC_REG(FRC_ME_RO_GMV_ROUGH_2ND)     & 0xFFF ;
	me_gmvx              = READ_FRC_REG(FRC_ME_RO_GMV)>>16           & 0x1FFF;
	me_gmvy              = READ_FRC_REG(FRC_ME_RO_GMV)               & 0xFFF ;
	me_gmvx_2nd          = READ_FRC_REG(FRC_ME_RO_GMV_2ND)>>16       & 0x1FFF;
	me_gmvy_2nd          = READ_FRC_REG(FRC_ME_RO_GMV_2ND)           & 0xFFF ;
	me_gmvx_mux			 = READ_FRC_REG(FRC_ME_RO_GMV_MUX)>>16       & 0x1FFF;
	me_gmvy_mux			 = READ_FRC_REG(FRC_ME_RO_GMV_MUX)	       & 0xFFF ;

	me_gmvx_rough		 = negative_convert(me_gmvx_rough, 13);
	me_gmvy_rough		 = negative_convert(me_gmvy_rough, 12);
	me_gmvx_rough_2nd	 = negative_convert(me_gmvx_rough_2nd, 13);
	me_gmvy_rough_2nd	 = negative_convert(me_gmvy_rough_2nd, 12);
	me_gmvx		 		 = negative_convert(me_gmvx, 13);
	me_gmvy		 		 = negative_convert(me_gmvy, 12);
	me_gmvx_2nd	 		 = negative_convert(me_gmvx_2nd, 13);
	me_gmvy_2nd	 		 = negative_convert(me_gmvy_2nd, 12);
	me_gmvx_mux	 		 = negative_convert(me_gmvx_mux, 13);
	me_gmvy_mux	 		 = negative_convert(me_gmvy_mux, 12);

	// 3.
	u32 me_gmv_rough_neighbor_cnt, me_gmv_neighbor_cnt;
	me_gmv_rough_neighbor_cnt    = READ_FRC_REG(FRC_ME_RO_GMV_ROUGH_NCNT) & 0x3FFFF;
	me_gmv_rough_cnt             = READ_FRC_REG(FRC_ME_RO_GMV_ROUGH_CNT)  & 0x3FFFF;
	me_gmv_neighbor_cnt          = READ_FRC_REG(FRC_ME_RO_GMV_NCNT)       & 0x3FFFF;
	me_gmv_cnt                   = READ_FRC_REG(FRC_ME_RO_GMV_CNT)        & 0x3FFFF;

	// 4.
	s32 me_region_gmvx_rough[12], me_region_gmvy_rough[12], me_region_gmvx[12], me_region_gmvy[12];
	u32 region_rp_gmv_neighbor_cnt[12], region_rp_gmv_cnt[12];

	for(k=0; k<12; k++)
	{
		me_region_gmvx_rough[k]  = READ_FRC_REG(FRC_ME_RO_GMV_ROUGH_RGN_0 + k*(FRC_ME_RO_GMV_ROUGH_RGN_1-FRC_ME_RO_GMV_ROUGH_RGN_0))>>16 & 0x1FFF;
		me_region_gmvy_rough[k]  = READ_FRC_REG(FRC_ME_RO_GMV_ROUGH_RGN_0 + k*(FRC_ME_RO_GMV_ROUGH_RGN_1-FRC_ME_RO_GMV_ROUGH_RGN_0))     & 0xFFF ;
		me_region_gmvx[k]        = READ_FRC_REG(FRC_ME_RO_GMV_RGN_0       + k*(FRC_ME_RO_GMV_RGN_1-FRC_ME_RO_GMV_RGN_0))>>16             & 0x1FFF;
		me_region_gmvy[k]        = READ_FRC_REG(FRC_ME_RO_GMV_RGN_0       + k*(FRC_ME_RO_GMV_RGN_1-FRC_ME_RO_GMV_RGN_0))                 & 0xFFF ;
		region_rp_gmv_neighbor_cnt[k]    = READ_FRC_REG(FRC_ME_RO_REGION_RP_GMV_CNT_0  + k*(FRC_ME_RO_REGION_RP_GMV_CNT_1-FRC_ME_RO_REGION_RP_GMV_CNT_0))>>16 & 0xFFFF;
		region_rp_gmv_cnt[k]             = READ_FRC_REG(FRC_ME_RO_REGION_RP_GMV_CNT_0  + k*(FRC_ME_RO_REGION_RP_GMV_CNT_1-FRC_ME_RO_REGION_RP_GMV_CNT_0))     & 0xFFFF;
	}

	// 5.
	u32 me_glb_dtl_sum[NUM_OF_LOOP];
	for(k=0; k<NUM_OF_LOOP; k++){
		me_glb_dtl_sum[k]    = READ_FRC_REG(FRC_ME_RO_DTL_0 + k*(FRC_ME_RO_DTL_1-FRC_ME_RO_DTL_0));
		//xil_printf("%d:%d\n\r", k,me_glb_dtl_sum[k]);
	}

	// 6.
	u32 vp_global_oct2_cover_cnt, vp_global_oct2_uncov_cnt;
	u32 vp_region_oct2_cover_cnt[12], vp_region_oct2_uncov_cnt[12];

	vp_global_oct2_cover_cnt = READ_FRC_REG(FRC_VP_RO_GLOBAL_OCT2_COVER_CNT);
	vp_global_oct2_uncov_cnt = READ_FRC_REG(FRC_VP_RO_GLOBAL_OCT2_UNCOV_CNT);

	for(k=0; k<NUM_OF_LOOP; k++){
		vp_region_oct2_cover_cnt[k]  = READ_FRC_REG(FRC_VP_RO_REGION_OCT2_COVER_CNT_0 + k*(FRC_VP_RO_REGION_OCT2_COVER_CNT_1-FRC_VP_RO_REGION_OCT2_COVER_CNT_0));
		vp_region_oct2_uncov_cnt[k]  = READ_FRC_REG(FRC_VP_RO_REGION_OCT2_UNCOV_CNT_0 + k*(FRC_VP_RO_REGION_OCT2_UNCOV_CNT_1-FRC_VP_RO_REGION_OCT2_UNCOV_CNT_0));
	}

	// 7.
	s32 vp_oct_gmvx_sum, vp_oct_gmvy_sum, vp_oct_gmvx, vp_oct_gmvy, vp_oct_total_cnt;
	u32 vp_oct_gmv_diff;

	vp_oct_gmvx_sum = READ_FRC_REG(FRC_VP_RO_GLOBAL_OCT_GMVX_SUM);
	vp_oct_gmvy_sum = READ_FRC_REG(FRC_VP_RO_GLOBAL_OCT_GMVY_SUM);
	vp_oct_gmv_diff = READ_FRC_REG(FRC_VP_RO_GLOBAL_OCT_GMV_DIFF);
	vp_oct_total_cnt= READ_FRC_REG(FRC_VP_RO_GLOBAL_OCT_COVER_CNT)+READ_FRC_REG(FRC_VP_RO_GLOBAL_OCT_UNCOV_CNT);
	vp_oct_gmvx_sum = negative_convert(vp_oct_gmvx_sum, 32);
	vp_oct_gmvy_sum = negative_convert(vp_oct_gmvy_sum, 32);
	if(vp_oct_total_cnt<5)
	{
		vp_oct_gmvx=0;
		vp_oct_gmvy=0;
	}
	else
	{
		vp_oct_gmvx = (vp_oct_gmvx_sum/vp_oct_total_cnt);
		vp_oct_gmvy = (vp_oct_gmvy_sum/vp_oct_total_cnt);
	}
}

