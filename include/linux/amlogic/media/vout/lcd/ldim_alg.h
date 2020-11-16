/* SPDX-License-Identifier: GPL-2.0+ */
/*
 *
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#ifndef _INC_AML_LDIM_ALG_H_
#define _INC_AML_LDIM_ALG_H_

/*========================================*/
#define LD_STA_BIN_NUM 16
#define LD_STA_LEN_V 17
/*  support maximum 16x4 regions for statistics (16+1) */
#define LD_STA_LEN_H 25
/*  support maximum 16x4 regions for statistics (24+1)*/
#define LD_BLK_LEN_V 25
/*  support maximum 16 led of each side left/right(16+4+4+1)*/
#define LD_BLK_LEN_H 33
/*  support maximum 24 led of each side top/bot  (24+4+4+1)*/
#define LD_LUT_LEN 32
#define LD_BLKHMAX 32
#define LD_BLKVMAX 32

#define LD_BLKREGNUM 384  /* maximum support 24*16*/

struct LDReg_s {
	int reg_ld_pic_row_max;            /*u13*/
	int reg_ld_pic_col_max;            /*u13*/
	int reg_ld_pic_yuv_sum[3];
	/* only output u16*3, (internal ACC will be u32x3)*/
	int reg_ld_pic_rgb_sum[3];
	/* Statistic options */
	int reg_ld_sta_vnum;
	/*u8: statistic region number of V, maximum to (STA_LEN_V-1)   (0~16)*/
	int reg_ld_sta_hnum;
	/*u8: statistic region number of H, maximum to (STA_LEN_H-1)   (0~24)*/
	int reg_ld_blk_vnum;
	/*u8: Maximum to 16*/
	int reg_ld_blk_hnum;
	/*u8: Maximum to 24*/
	int reg_ld_sta1max_lpf;
	/*u1: STA1max statistics on [1 2 1]/4 filtered results*/
	int reg_ld_sta2max_lpf;
	/*u1: STA2max statistics on [1 2 1]/4 filtered results*/
	int reg_ld_sta_hist_lpf;
	/*u1: STAhist statistics on [1 2 1]/4 filtered results*/
	int reg_ld_sta1max_hdlt;
	/* u2: (2^x) extra pixels into Max calculation*/
	int reg_ld_sta1max_vdlt;
	/* u4: extra pixels into Max calculation vertically*/
	int reg_ld_sta2max_hdlt;
	/* u2: (2^x) extra pixels into Max calculation*/
	int reg_ld_sta2max_vdlt;
	/* u4: extra pixels into Max calculation vertically*/
	int reg_ld_sta1max_hidx[LD_STA_LEN_H];  /* U12* STA_LEN_H*/
	int reg_ld_sta1max_vidx[LD_STA_LEN_V];  /* u12x STA_LEN_V*/
	int reg_ld_sta2max_hidx[LD_STA_LEN_H];  /* U12* STA_LEN_H*/
	int reg_ld_sta2max_vidx[LD_STA_LEN_V];  /* u12x STA_LEN_V*/

	int reg_ld_sta_hist_mode;
		/*u3: histogram statistics on XX separately 20bits*16bins:
		 *0: R-only,1:G-only 2:B-only 3:Y-only;
		 *4: MAX(R,G,B), 5/6/7: R&G&B
		 */
	int reg_ld_sta_hist_pix_drop_mode;
		/* u2:histogram statistics pixel drop mode:
		 * 0:no drop; 1:only statistic
		 * x%2=0; 2:only statistic x%4=0; 3: only statistic x%8=0;
		 */
	int reg_ld_sta_hist_hidx[LD_STA_LEN_H];  /* U12* STA_LEN_H*/
	int reg_ld_sta_hist_vidx[LD_STA_LEN_V];  /* u12x STA_LEN_V*/

	/***** FBC3 fw_hw_alg_frm *****/
	int reg_ldfw_bl_max;/* maximum BL value*/
	int reg_ldfw_blk_norm;
		/*u8: normalization gain for blk number,
		 *1/blk_num= norm>>(rs+8), norm = (1<<(rs+8))/blk_num
		 */
	int reg_ldfw_blk_norm_rs;/*u3: 0~7,  1/blk_num= norm>>(rs+8) */
	int reg_ldfw_sta_hdg_weight[8];
		/*  u8x8, weighting to each
		 * block's max to decide that block's ld.
		 */
	int reg_ldfw_sta_max_mode;
		/* u2: maximum selection for
		 *components: 0: r_max, 1: g_max, 2: b_max; 3: max(r,g,b)
		 */
	int reg_ldfw_sta_max_hist_mode;
		/* u2: mode of reference
		 * max/hist mode:0: MIN(max, hist), 1: MAX(max, hist)
		 * 2: (max+hist)/2, 3: (max(a,b)*3 + min(a,b))/4
		 */
	int reg_ldfw_hist_valid_rate;
		/* u8, norm to 512 as "1", if hist_matrix[i]>(rate*histavg)>>9*/
	int reg_ldfw_hist_valid_ofst;/*u8, hist valid bin upward offset*/
	int reg_ldfw_sedglit_RL;/*u1: single edge lit right/bottom mode*/
	int reg_ldfw_sf_thrd;/*u12: threshold of difference to enable the sf;*/
	int reg_ldfw_boost_gain;
		/* u8: boost gain for the region that
		 * is larger than the average, norm to 16 as "1"
		 */
	int reg_ldfw_tf_alpha_rate;
		/*u8: rate to SFB_BL_matrix from
		 *last frame difference;
		 */
	int reg_ldfw_tf_alpha_ofst;
		/*u8: ofset to alpha SFB_BL_matrix
		 *from last frame difference;
		 */
	int reg_ldfw_tf_disable_th;
		/*u8: 4x is the threshod to disable
		 *tf to the alpha (SFB_BL_matrix from last frame difference;
		 */
	int reg_ldfw_blest_acmode;
		/*u3: 0: est on BLmatrix;
		 *1: est on(BL-DC); 2: est on (BL-MIN);
		 *3: est on (BL-MAX) 4: 2048; 5:1024
		 */
	int reg_ldfw_sf_enable;
	/*u1: enable signal for spatial filter on the tbl_matrix*/
	int reg_ldfw_enable;
	int reg_ldfw_sta_hdg_vflt;
	int reg_ldfw_sta_norm;
	int reg_ldfw_sta_norm_rs;
	int reg_ldfw_tf_enable;
	int reg_ld_lut_hdg_txlx[8][32];
	int reg_ld_lut_vdg_txlx[8][32];
	int reg_ld_lut_vhk_txlx[8][32];
	int reg_ld_lut_id[16 * 24];
	int reg_ld_lut_hdg_lext_txlx[8];
	int reg_ld_lut_vdg_lext_txlx[8];
	int reg_ld_lut_vhk_lext_txlx[8];
	int reg_ldfw_boost_enable;
		/*u1: enable signal for Boost
		 *filter on the tbl_matrix
		 */
	int ro_ldfw_bl_matrix_avg;/*u12: read-only register for bl_matrix*/

	/* Backlit Modeling registers*/
	int BL_matrix[LD_BLKREGNUM];
	/* Define the RAM Matrix*/
	int reg_ld_blk_xtlk;
	/*u1: 0 no block to block Xtalk model needed;   1: Xtalk model needed*/
	int reg_ld_blk_mode;
	/*u2: 0- LEFT/RIGHT Edge Lit; 1- Top/Bot Edge Lit; 2 - DirectLit
	 *modeled H/V independent; 3- DirectLit modeled HV Circle distribution
	 */
	int reg_ld_reflect_hnum;
	/*u3: numbers of band reflection considered in Horizontal direction;
	 *	0~4
	 */
	int reg_ld_reflect_vnum;
	/*u3: numbers of band reflection considered in Horizontal direction;
	 *	0~4
	 */
	int reg_ld_blk_curmod;
	/*u1: 0: H/V separately, 1 Circle distribution*/
	int reg_ld_bklut_intmod;
	/*u1: 0: linear interpolation, 1 cubical interpolation*/
	int reg_ld_bklit_intmod;
	/*u1: 0: linear interpolation, 1 cubical interpolation*/
	int reg_ld_bklit_lpfmod;
	/*u3: 0: no LPF, 1:[1 14 1]/16;2:[1 6 1]/8; 3: [1 2 1]/4;
	 *	4:[9 14 9]/32  5/6/7: [5 6 5]/16;
	 */
	int reg_ld_bklit_celnum;
	/*u8: 0: 1920~ 61*/
	int reg_bl_matrix_avg;
	/*u12: DC of whole picture BL to be subtract from BL_matrix
	 *	during modeling (Set by FW daynamically)
	 */
	int reg_bl_matrix_compensate;
	/*u12: DC of whole picture BL to be compensated back to
	 *	Litfull after the model (Set by FW dynamically);
	 */
	int reg_ld_reflect_hdgr[20];
	/*20*u6:  cells 1~20 for H Gains of different dist of
	 * Left/Right;
	 */
	int reg_ld_reflect_vdgr[20];
	/*20*u6:  cells 1~20 for V Gains of different dist of Top/Bot;*/
	int reg_ld_reflect_xdgr[4];     /*4*u6:*/
	int reg_ld_vgain;               /*u12*/
	int reg_ld_hgain;               /*u12*/
	int reg_ld_litgain;             /*u12*/
	int reg_ld_litshft;
	/*u3   right shif of bits for the all Lit's sum*/
	int reg_ld_bklit_valid[32];
	/*u1x32: valid bits for the 32 cell Bklit to contribut to current
	 *	position (refer to the backlit padding pattern)
	 */
	/* region division index 1 2 3 4 5(0) 6(1) 7(2)
	 *	8(3) 9(4)  10(5)11(6)12(7)13(8) 14(9)15(10) 16 17 18 19
	 */
	int reg_ld_blk_hidx[LD_BLK_LEN_H]; /* S14* BLK_LEN*/
	int reg_ld_blk_vidx[LD_BLK_LEN_V]; /* S14* BLK_LEN*/
	/* Define the RAM Matrix*/
	int reg_ld_lut_hdg[LD_LUT_LEN];  /*u10*/
	int reg_ld_lut_vdg[LD_LUT_LEN];  /*u10*/
	int reg_ld_lut_vhk[LD_LUT_LEN];  /*u10*/
	/* VHk positive and negative side gain, normalized to 128
	 *	as "1" 20150428
	 */
	int reg_ld_lut_vhk_pos[32];   /* u8*/
	int reg_ld_lut_vhk_neg[32];   /* u8*/
	int reg_ld_lut_hhk[32];
	/* u8 side gain for LED direction hdist gain for different LED*/
	/* VHo possitive and negative side offset, use with LS, (x<<LS)*/
	int reg_ld_lut_vho_pos[32];   /* s8*/
	int reg_ld_lut_vho_neg[32];   /* s8*/
	int reg_ld_lut_vho_ls;/* u3:0~6,left shift bits of VH0_pos/neg*/
	/* adding three cells for left boundary extend during
	 *	Cubic interpolation
	 */
	int reg_ld_lut_hdg_lext;
	int reg_ld_lut_vdg_lext;
	int reg_ld_lut_vhk_lext;
	/* adding demo window mode for LD for RGB lut compensation*/
	int reg_ld_xlut_demo_roi_xstart;
	/* u14 start col index of the region of interest*/
	int reg_ld_xlut_demo_roi_xend;
	/* u14 end col index of the region of interest*/
	int reg_ld_xlut_demo_roi_ystart;
	/* u14 start row index of the region of interest*/
	int reg_ld_xlut_demo_roi_yend;
	/* u14 end row index of the region of interest*/
	int reg_ld_xlut_iroi_enable;
	/* u1: enable rgb LUT remapping inside regon of interest:
	 *	0: no rgb remapping; 1: enable rgb remapping
	 */
	int reg_ld_xlut_oroi_enable;
	/* u1: enable rgb LUT remapping outside regon of interest:
	 *	0: no rgb remapping; 1: enable rgb remapping
	 */
	/* Register used in RGB remapping*/
	int reg_ld_rgb_mapping_demo;
	/*u2: 0 no demo mode 1: display BL_fulpel on RGB*/
	int reg_ld_x_lut_interp_mode[3];
	/*U1 0: using linear interpolation between to neighbour LUT;
	 *	1: use the nearest LUT results
	 */
	int x_idx[1][16];
	/* Changed to 16 Lits define 32 Bin LUT to save cost*/
	int x_nrm[1][16];
	/* Changed to 16 Lits define 32 Bin LUT to save cost*/
	int x_lut[3][16][32];
	/* Changed to 16 Lits define 32 Bin LUT to save cost*/
	/* only do the Lit modleing on the AC part*/
	int x_lut2[3][16][16];
	int fw_ld_bl_est_acmode;
	/*u2: 0: est on BLmatrix; 1: est on (BL-DC);
	 *	2: est on (BL-MIN); 3: est on (BL-MAX)
	 */
};

struct FW_DAT_s {
	/* for temporary Firmware algorithm */
	unsigned int *tf_bl_alpha;
	unsigned int *last_yuv_sum;
	unsigned int *last_rgb_sum;
	unsigned int *last_sta1_maxrgb;
	unsigned int *sf_bl_matrix;
	unsigned int *tf_bl_matrix;
	unsigned int *tf_bl_matrix_2;
};

struct fw_ctrl_config_s {
	unsigned int fw_ld_thsf_l;
	unsigned int fw_ld_thtf_l;
	unsigned int boost_gain; /*norm 256 to 1,T960 finally use*/
	unsigned int tf_alpha; /*256;*/
	unsigned int lpf_gain;  /* [0~128~256], norm 128 as 1*/

	unsigned int boost_gain_neg;
	unsigned int alpha_delta;

	/*LPF tap: 0-lpf_res 41,1-lpf_res 114,...*/
	unsigned int lpf_res;    /* 1024/9*9 = 13,LPF_method=3 */
	unsigned int rgb_base;

	unsigned int ov_gain;
	/*unsigned int incr_dif_gain; //16 */

	unsigned int avg_gain;

	unsigned int fw_rgb_diff_th;
	unsigned int max_luma;
	unsigned int lmh_avg_th;/*for woman flicker*/
	unsigned int fw_tf_sum_th;/*20180530*/

	unsigned int lpf_method;
	unsigned int ld_tf_step_th;
	unsigned int tf_step_method;
	unsigned int tf_fresh_bl;

	unsigned int tf_blk_fresh_bl;
	unsigned int side_blk_diff_th;
	unsigned int bbd_th;
	unsigned char bbd_detect_en;
	unsigned char diff_blk_luma_en;

	unsigned char sf_bypass;
	unsigned char boost_light_bypass;
	unsigned char lpf_bypass;
	unsigned char ld_remap_bypass;
	unsigned char black_frm;

	unsigned char white_area_remap_en;
	unsigned int white_area;
	unsigned int white_lvl;
	unsigned int white_area_th_max;
	unsigned int white_area_th_min;
	unsigned int white_lvl_th_max;
	unsigned int white_lvl_th_min;
};

struct ldim_fw_para_s {
	/* header */
	unsigned int para_ver;
	unsigned int para_size;
	char ver_str[20];
	unsigned char ver_num;

	unsigned char hist_col;
	unsigned char hist_row;

	/* for debug print */
	unsigned char fw_hist_print;/*20180525*/
	unsigned int fw_print_frequent;/*20180606,print every 8 frame*/
	unsigned int fw_dbgprint_lv;

	struct LDReg_s *nprm;
	struct FW_DAT_s *fdat;
	unsigned int *bl_remap_curve; /* size: 16 */
	unsigned int *fw_ld_whist;    /* size: 16 */

	struct fw_ctrl_config_s *ctrl;

	void (*fw_alg_frm)(struct ldim_fw_para_s *fw_para,
			   unsigned int *max_matrix,
			   unsigned int *hist_matrix);
	void (*fw_alg_para_print)(struct ldim_fw_para_s *fw_para);
};

/* if struct ldim_fw_para_s changed, FW_PARA_VER must be update */
#define FW_PARA_VER    2

struct ldim_fw_para_s *aml_ldim_get_fw_para(void);

#endif
