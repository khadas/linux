/*
*
* SPDX-License-Identifier: GPL-2.0
*
* Copyright (C) 2020 Amlogic or its affiliates
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; version 2.
* This program is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
* or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
* for more details.
* You should have received a copy of the GNU General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*
*/

#ifndef __AML_ISP_CFG_H__
#define __AML_ISP_CFG_H__

#define AE_HISTOGRAM_SIZE 1024
#define FED_FLKR_STAT_MAX 1280
#define MAX_CHANNEL     (2)
#define LTM_STA_BIN_NUM (513)
#define LTM_STA_BLK_REG_NUM (96)
#define PST_STA_BIN_NUM_GLB 256
#define W_BLOCKNUM  32
#define H_BLOCKNUM  32
#define BLKREGNUM   96
#define PK_NODS     4
#define DHZ_NODES   5
#define CURV_NODES  6
#define TOL_PK_NUM      (BLKREGNUM * PK_NODS)
#define TOL_DHZ_NODS    (BLKREGNUM * DHZ_NODES)
#define TOL_CURV_NODS   (BLKREGNUM * CURV_NODES)
#define DNLP_STA_BIN_NUM 64

#define AF_STAT_BLKH_NUM    (17)
#define AF_STAT_BLKV_NUM    (15)
#define AE_STAT_BLKH_NUM    (17)
#define AE_STAT_BLKV_NUM    (15)
#define AWB_STAT_BLKH_NUM   (32)
#define AWB_STAT_BLKV_NUM   (24)
#define FW_DISP_CHN_MAX     (4)
#define FW_RECT_CHN_MAX     (8)

/**
  * @struct wb_zone_info_s
  * @brief white balance statistics info on one slice
  * @details
  * @note
  * @attention
  */
typedef struct wb_zone_info_s{
	u16 rg;/**< red gain (r*4096/g) */
	u16 bg;/**< blue gain (b*4096/g) */
	u16 avg_r;/**< average red value normalize to 0x10000  */
	u16 avg_g;/**< average green value normalize to 0x10000  */
	u16 avg_b;/**< average blue value normalize to 0x10000  */
	u16 avg_luma;/**< average blue value normalize to 0x10000  */
	u32 sum;/**< valid pixel sum on the slice */
} wb_zone_info_t;

typedef struct _isp_awb_stats_pack_t {
	u32 pack0;
	u32 pack1;
} isp_awb_stats_pack_t;

typedef struct _isp_awb_stats_mode0_unit_s
{
    u16 rg;
    u16 bg;
    u32 pix_sum;
}isp_awb_stats_mode0_unit_t;

typedef struct _isp_awb_stats_mode1_unit_s
{
    u16 avg_r;
    u16 avg_g;
    u16 avg_b;
    u16 avg_luma;
}isp_awb_stats_mode1_unit_t;

/**
  * @struct wb_stats_info_s
  * @brief white balance statistics info
  * @details
  * @note
  * @attention
  */

typedef struct wb_stats_info_s {
	isp_awb_stats_pack_t data[4*AWB_STAT_BLKH_NUM*AWB_STAT_BLKV_NUM];

	//Need not provid by driver
	wb_zone_info_t  zones[4][AWB_STAT_BLKH_NUM*AWB_STAT_BLKV_NUM];/**< all of WB zone statistic info */
	u32        zones_rows;/**< valid rows of the WB zone table */
	u32        zones_cols;/**< valid cols of the WB zone table */
	u32        zones_num;/**< valid table count of the WB zone table */
	u32        zones_size;/**< valid zone count of the WB zone table */
	u32        zones_mode;/**< wb data mode 0: g/rb or rb/g 1: rgb_avg */
	u32        rsv[3];
} wb_stats_info_t;

/**
  * @struct exps_stats_info_s
  * @brief Exposure statistic info
  * @details
  * @note
  * @attention
  */

typedef struct exps_stats_pack1_s {
	u32 pack0;
	u32 pack1;
	u64 pack2;
	u64 pack3;
} exps_stats_pack1_t;

typedef struct exps_stats_pack2_s {
	u32 pack0;
	u32 pack1;
} exps_stats_pack2_t;

typedef struct isp_exps_stats_pack1_s {
	exps_stats_pack1_t  data[AE_STAT_BLKH_NUM*AE_STAT_BLKV_NUM];
	u32  rsv[2];
	u32  hist[AE_HISTOGRAM_SIZE];/**< Exposure histogram statistic */
} isp_exps_stats_pack1_t;

typedef struct isp_exps_stats_pack2_s {
    exps_stats_pack2_t  data[AE_STAT_BLKH_NUM*AE_STAT_BLKV_NUM];
    u32            rsv[2];
    u32            hist[AE_HISTOGRAM_SIZE];/**< Exposure histogram statistic */
} isp_exps_stats_pack2_t;

typedef struct exps_zone_info_s{
    uint16_t blk_hist[4];/**< local block hist-5bin */
} exps_zone_info_t;

typedef struct exps_stats_info_s{
	union {
		u32 stats[((AE_STAT_BLKH_NUM * AE_STAT_BLKV_NUM * 3 + 1) * 2) + AE_HISTOGRAM_SIZE];
		isp_exps_stats_pack1_t stats1;
		isp_exps_stats_pack2_t stats2;
	};

	//Need not provid by driver
	exps_zone_info_t  zones[AE_STAT_BLKH_NUM*AE_STAT_BLKV_NUM];/**< all of exposure statistic info */
	u32  zones_rows;/**< valid rows of the exposure statistic table */
	u32  zones_cols;/**< valid cols of the exposure statistic table */
	u32  zones_size;/**< valid zone count of the exposure statistic table */
	u32  hist_sum;/**< Exposure histogram pixel total */
	u32  rsv[2];
} exps_stats_info_t;

typedef struct af_zone_info_t {
	u16 i2_mat;
	u16 i4_mat;
	u16 c4_mat;
	u16 c4_exp;
	u16 i2_exp;
	u16 i4_exp;
} af_zone_info_t;

typedef struct af_zone_info2_t {
	u16 h0;
	u16 h1;
	u16 v0;
	u16 v1;
	u16 y;
	u16 y_cnt;
	u8 h_cnt0;
	u8 h_cnt1;
	u8 v_cnt0;
	u8 v_cnt1;
} af_zone_info2_t;

typedef struct af_stats_pack1_s {
	u32 pack0;
	u32 pack1;
} af_stats_pack1_t;

typedef struct af_stats_pack2_s {
	u32 pack0;
	u32 pack1;
	u32 pack2;
	u32 pack3;
} af_stats_pack2_t;

typedef struct isp_af_stats_pack1_s {
	af_stats_pack1_t data[AF_STAT_BLKH_NUM*AF_STAT_BLKV_NUM];
	u32 rsv[2];
} isp_af_stats_pack1_t;

typedef struct isp_af_stats_pack2_s {
	af_stats_pack2_t data[AF_STAT_BLKH_NUM*AF_STAT_BLKV_NUM];
} isp_af_stats_pack2_t;

/**
  * @struct af_stats_info_s
  * @brief AF statistic info
  * @details
  * @note
  * @attention
  */
typedef struct af_stats_info_s {
	union {
		u32 stats[AF_STAT_BLKH_NUM*AF_STAT_BLKV_NUM*4];
		isp_af_stats_pack1_t stat1;
		isp_af_stats_pack2_t stat2;
	};

	//Need not provid by driver
	union {
		af_zone_info_t  zone_mode0[AF_STAT_BLKH_NUM*AF_STAT_BLKV_NUM];
		af_zone_info2_t zone_mode1[AF_STAT_BLKH_NUM*AF_STAT_BLKV_NUM];
	};
	union {
		af_zone_info_t  glb_mode0;
		af_zone_info2_t glb_mode1;
	};
	u16  zones_rows;/**< valid rows of the AF zone table */
	u16  zones_cols;/**< valid cols of the AF zone table */
	u16  zones_size;/**< valid zone count of the AF zone table */
	u16  zones_mode;/**< valid zone mode */
	u16  reserve[4];
} af_stats_info_t;

/**
  * @struct deflkr_stats_info_s
  * @brief de-flikcer statistic info
  * @details
  * @note
  * @attention
  */
typedef struct deflkr_stats_info_s{
	u32 data[FED_FLKR_STAT_MAX / 10 * 4];/**< each line average value of the input image of the current frame  */

	//Need not provid by driver
	u32 dif[FED_FLKR_STAT_MAX];
} deflkr_stats_info_t;

typedef struct post_stats_info_s {
	u32 data[2128];
} post_stats_info_t;

/**
  * @struct wdr_stats_info_s
  * @brief Wide Dynamic Range statistic
  * @details
  * @note
  * @attention
  */
typedef struct wdr_stats_info_s{
	u32 wdr_expcomb_slope_weight;
	u32 wdr_expcomb_ir_slope_weight;
	u32 fw_wdr_stat_sum[MAX_CHANNEL];
	u32 fw_wdr_stat_cnt[MAX_CHANNEL];
} wdr_stats_info_t;

/**
  * @struct ltm_stats_info_s
  * @brief Local Tone Mapping statistic info
  * @details
  * @note
  * @attention
  */
typedef struct ltm_stats_info_s{
	s32 reg_ltm_stat_hblk_num;                             /**< u5, ltm processing region number of H, maximum to (LTM_STA_LEN_H-1)   (0~16) */
	s32 reg_ltm_stat_vblk_num;                             /**< u5, ltm processing region number of V, maximum to (LTM_STA_LEN_V-1)   (0~16) */
	u32 ro_ltm_gmax_idx;                                   /**< u10, global lmax index */
	u32 ro_ltm_gmin_idx;                                   /**< u10, global lmin index */
	u32 ro_sum_log_4096_l;                                 /**< u32, log_4096 sum lower , to calc log_avg_4096_blk in fw */
	u32 ro_sum_log_4096_h;                                 /**< u17, log_4096 sum higher , to calc log_avg_4096_blk in fw */
	u32 ro_ltm_histbuf[LTM_STA_BIN_NUM+1];                 /**< u24*514, global hist, 514 for ir case, 513 for mrgb case */
	s32 ro_ltm_lmin_blk_in[LTM_STA_BLK_REG_NUM];          /**< u20*96, local lmin */
	s32 ro_ltm_lmax_blk_in[LTM_STA_BLK_REG_NUM];          /**< u20*96, local lmax */
	u32 ro_ltm_sta_hst_blk_sum_h_init[LTM_STA_BLK_REG_NUM];/**< u32*96, local hist sum high 17bits, prepare to calc log_avg_blk_4096 -> local reg_ltm_la_blk; */
	u32 ro_ltm_sta_hst_blk_sum_l_init[LTM_STA_BLK_REG_NUM];/**< u32*96, local hist sum low  32bits, prepare to calc log_avg_blk_4096 -> local reg_ltm_la_blk; */
} ltm_stats_info_t;

/**
  * @struct lc_stats_info_s
  * @brief Local Contrast statistic info
  * @details
  * @note
  * @attention
  */
typedef struct lc_stats_info_s{
	u32 reg_lc_blk_hnum_both;
	u32 reg_lc_blk_vnum_both;
	u32 reg_lc_sta_hnum_both;
	u32 reg_lc_sta_vnum_both;
	s32 ro_min_val_glb;
	s32 ro_max_val_glb;
	s32 hist_matrix[BLKREGNUM * 16];
	s32 max_matrix[BLKREGNUM * 3];
	s32 ro_pst_sta_hst[PST_STA_BIN_NUM_GLB]; //post_stat_info_t
	s32 ro_wdr_stat_yblk_flt[W_BLOCKNUM * H_BLOCKNUM];
	s32 reg_skin_cnt_region[BLKREGNUM]; //post_stat_info_t
	s32 fw_ram_curve_nodes_in[TOL_CURV_NODS];
	s32 fw_ram_pk_idxs_in[TOL_PK_NUM];
	s32 fw_ram_pk_vals_in[TOL_PK_NUM];
} lc_stats_info_t;

/**
  * @struct dnlp_stats_info_s
  * @brief DNLP statistic info
  * @details
  * @note
  * @attention
  */
typedef struct dnlp_stats_info_s{
	//fw input control curves
	//hw input hist info
	u32 ro_dnlp_sta_hst[DNLP_STA_BIN_NUM];
	u32 ro_dnlp_pix_amount;                        //raw_hst_sum
	u64 ve_dnlp_luma_sum;
} dnlp_stats_info_t;

/**
  * @struct dehaze_stats_info_s
  * @brief Dehaze statistic info
  * @details
  * @note
  * @attention
  */
typedef struct dehaze_stats_info_s{
	u32 reg_dhz_blk_hnum_both;
	u32 reg_dhz_blk_vnum_both;
	s32 fw_ram_dhz_nodes_in[TOL_DHZ_NODS];                    //uBL*384region*8cell

	u32 ro_min_val_glb;
	u32 ro_max_val_glb;
/* ro_post_stat no need provide here */
	u32 ro_dhz_sta_hst[PST_STA_BIN_NUM_GLB];     //u32x256: LC global sta on 256 RGBMax bins;
} dehaze_stats_info_t;

typedef struct top_req_info_s{
	u32 raw_mode;
	u32 wdr_stat_en;
} top_req_info_t;

typedef struct awb_req_info_s{
	u32 awb_stat_hblk_num;
	u32 awb_stat_vblk_num;
	u32 awb_stat_luma_div_mode;
	u32 awb_stat_local_mode;
	u32 awb_stat_ratio_mode;
	u32 blc_ofst[5];
	u32 awb_stat_switch;
} awb_req_info_t;

typedef struct ae_req_info_s {
	s32 gtm_en;
	s32 pst_gtm_en; // none
	s32 gtm_lut_mode;
	s32 pst_gamma_mode;
	s32 gtm_lut_stp[8];
	s32 gtm_lut_num[8];
	u32 ae_stat_switch;
	u32 ae_stat_local_mode;
	u32 ae_stat_hblk_num;
	u32 ae_stat_vblk_num;
	u32 ae_stat_glbpixnum;
	u32 roi0_pack0;
	u32 roi0_pack1;
	u32 roi1_pack0;
	u32 roi1_pack1;
} ae_req_info_t;

typedef struct af_req_info_s {
	u32 af_stat_hblk_num;
	u32 af_stat_vblk_num;
	u32 af_stat_sel;
	u32 af_glb_stat_pack0;
	u32 af_glb_stat_pack1;
	u32 af_glb_stat_pack2;
	u32 af_glb_stat_pack3;
} af_req_info_t;
typedef struct flkr_req_info_s{
	u32 flkr_binning_rs;
	u32 flkr_det_en;
	u32 flkr_ro_mode;
	u32 flkr_sta_pos;
} flkr_req_info_t;

typedef struct wdr_req_info_s{
	u32 wdr_en;
	u32 wdr_lexpratio_int64_0;
	u32 wdr_lexpratio_int64_1;
	u32 wdr_expcomb_blend_thd0;
	u32 wdr_expcomb_blend_thd1;
} wdr_req_info_t;

typedef struct ltm_req_info_s{
	u32 ltm_dark_floor_u8;
	u32 ltm_expblend_thd0;
	u32 ltm_expblend_thd1;
	u32 ltm_min_factor_u8;
	u32 ltm_max_factor_u8;
	u32 ltm_bright_floor_u14;
} ltm_req_info_t;

typedef struct lc_req_info_s {
	u32 lc_lmtrat_minmax;

	u32 lc_blackbar_mute_en;   //u1 mute the black bar corresponding bin, 0: no mute, 1: mute enable; default=1
	u32 lc_blackbar_mute_thrd; //u1 mute the black bar corresponding bin, 0: no mute, 1: mute enable; default=1
	u32 lc_histvld_thrd;   //u8 threshold to compare to bin to get number of valid bins
	u32 lc_lmtrat_valid;   //u8 x/1024 of amount
	u32 lc_contrast_low;   //u12 contrast gain to the lc for dark side, normalized 256 as "1", set adaptive TODO
	u32 lc_contrast_hig;   //u12 contrast gain to the lc for bright side, normalized 256 as "1"
	u32 lc_cntstlmt_low[2] ;   //u8 limit for the contrast low, delta_low = MIN(delta_low, MIN( MAX((minBV-min_val)*scl_low/8, lmt_low[0]),lmt_low[1]))
	u32 lc_cntstlmt_hig[2] ;   //u8 limit for the contrast high,delta_hig = MIN(delta_hig, MIN( MAX((max_val-maxBV)*scl_hig/8, lmt_hig[0]),lmt_hig[1]))
	u32 lc_cntstscl_low;   //u8 scale for the contrast low, norm 8 as 1; delta_low = MIN(delta_low, MIN(MAX((minBV-min_val)*scl_low/8, lmt_low[0]),lmt_low[1]))
	u32 lc_cntstscl_hig;   //u8 scale for the contrast high,norm 8 as 1; delta_hig = MIN(delta_hig, MIN(MAX((max_val-maxBV)*scl_hig/8, lmt_hig[0]),lmt_hig[1]))
	u32 lc_cntstbvn_low;   //u8 scale to num_m as limit of min_val to minBV distance, to protect mono-color, default = 32; min_val= MAX(min_val, minBV- MAX(num_m-1,0)*bvn_low)
	u32 lc_cntstbvn_hig;   //u8 scale to num_m as limit of max_val to maxBV distance, to protect mono-color, default = 32; min_val= MIN(max_val, maxBV+ MAX(num_m-1,0)*bvn_low)
	u32 lc_num_m_coring;   //u4 coring to num_m, soft coring,default = 2;
	u32 lc_vbin_min;   //u8 4x is min width of valid histogram bin num,
	u32 lc_ypkbv_ratio[4];   //u8x4, x= ratio*(maxBv-minBv)+min_val as low bound of the ypkBV; normalized to 256 as 1, for pkBV as 0, 256,512, 784above ratio
	u32 lc_ypkbv_slope_lmt[2]  ;   //u6+u8, min max slop for the curves to avoid artifacts, [0] for min_slope, [1] for max_slop, e.g.max_slope= limit*(pkBv-minBv)+min_val as high bound of the ypkBV; normalized to 32 as 1
	u32 lc_slope_max_face;    //u8, maximum slope for the pkBin-maxBV range curve to do face protection, normalized to 32 as 1, default= 48

	u32 lc_yminval_lmt[16];   //u10x16, lmt_val = lmt[minBV(64:64:1023)], and yminV = MAX(yminV,lmt_val), for very dark region boost, default= [48, 80, 120, 60]
	u32 lc_ypkbv_lmt[16];   //u10x16, lmt_val = 4*lmt[pkBV(64:64:1023) or maxBV(64:64:1023)], and ypkBV = MAX(ypkBV,lmt[pkBV]), ymaxV = MAX(ymaxV,lmt[maxBV])&& above min_slop for very dark region boost, default= ...
	u32 lc_ymaxval_lmt[16];   //u10x16, lmt_val = 4*lmt[pkBV(64:64:1023) or maxBV(64:64:1023)], and ypkBV = MAX(ypkBV,lmt[pkBV]), ymaxV = MAX(ymaxV,lmt[maxBV])&& above min_slop for very dark region boost, default= ...

	//deblock
	u32 lc_pk_vld; //u8 threshold to compare to bin to get number of valid bins

	u32 lc_sta_blk_enable;
	u32 pst_sta_enable;
	u32 lc_stat_pack_num;
}lc_req_info_t;

typedef struct ofe_req_info_s{
	u32 bac_mode;
} ofe_req_info_t;

typedef struct dehaze_req_info_s{
	u32 dhz_minvval_lmt[16];
	u32 dhz_maxvval_lmt[16];
	u32 dhz_lmtrat_lowc;
	u32 dhz_lmtrat_higc;
	u32 dhz_dlt_rat;
	u32 dhz_cc_en;
	u32 dhz_hig_dlt_rat;
	u32 dhz_low_dlt_rat;
} dehaze_req_info_t;

typedef struct module_info_s{
	top_req_info_t top_req_info;
	awb_req_info_t awb_req_info;
	ae_req_info_t ae_req_info;
	af_req_info_t af_req_info;
	flkr_req_info_t flkr_req_info;
	wdr_req_info_t wdr_req_info;
	ltm_req_info_t ltm_req_info;
	lc_req_info_t lc_req_info;
	ofe_req_info_t ofe_req_info;
	dehaze_req_info_t dehaze_req_info;
} module_info_t;

typedef struct frame_info_s {
	int frm_cnt;/**< frame number for HW counter */
	int slice_num;
	int slice_ovlp;
	int reserved[1];
} frame_info_t;

/**
  * @struct aisp_stats_info_s
  * @brief all of statistic info on the ISP system
  * @details
  * @note
  * @attention
  */
typedef struct aisp_stats_info_s{
	frame_info_t frame_info;
	wb_stats_info_t wb_stats;/**< White Balance statistic info */
	exps_stats_info_t exps_stats;/**< Exposure statistic info */
	af_stats_info_t af_stats;/**< Auto Focus statistic info */
	deflkr_stats_info_t deflkr_stats;/**< De-flicker statistic info */
	post_stats_info_t post_stats;
	wdr_stats_info_t  wdr_stats;/**< Wide Dynamic Range statistic info */
	ltm_stats_info_t ltm_stats;/**< Local Tone Mapping statistic info */
	lc_stats_info_t lc_stats;/**< Local Contrast statistic info */
	dnlp_stats_info_t dnlp_stats;/**< DNLP statistic info */
	dehaze_stats_info_t dehaze_stats;/**< Dehaze statistic info */
	module_info_t module_info;/**<module required info>*/
} aisp_stats_info_t;

/********************** 3a **********************/
typedef struct aisp_dgain_cfg_s {
	u32 dg_gain[5];
	u32 idg_gain[5];
} aisp_dgain_cfg_t;

typedef struct aisp_wb_change_cfg_s {
	u32 wb_gain[5];
	u32 wb_limit[5];
	u32 ae_gain_grbgi[5];
	u32 ae_bl12_grbgi[5];
} aisp_wb_change_cfg_t;

typedef struct aisp_wb_luma_cfg_s {
	u32 awb_stat_blc20[4];
	u32 awb_stat_gain10[4];
	u32 awb_stat_satur_low;
	u32 awb_stat_satur_high;
} aisp_wb_luma_cfg_t;

typedef struct aisp_wb_triangle_cfg_s {
	u32 awb_stat_satur_vald;
	u32 awb_stat_rg_min;
	u32 awb_stat_rg_max;
	u32 awb_stat_bg_min;
	u32 awb_stat_bg_max;
	u32 awb_stat_rg_low;
	u32 awb_stat_rg_high;
	u32 awb_stat_bg_low;
	u32 awb_stat_bg_high;
} aisp_wb_triangle_cfg_t;

typedef struct aisp_expo_mode_cfg_s {
	u8 ae_stat_blk_weight[17*15];
	u8 reserved;
} aisp_expo_mode_cfg_t;

typedef struct aisp_awb_weight_cfg_s {
	u32 awb_stat_blk_weight[32*24];
} aisp_awb_weight_cfg_t;

/********************** base **********************/
typedef struct aisp_phase_setting_cfg_s {
	u32 raw_mode;
	u32 src_bit_depth;
	u32 raw_phslut[16];
	u32 losse_raw_phslut[16];
	u32 lossd_raw_phslut[16];
} aisp_phase_setting_cfg_t;

typedef struct aisp_mesh_cfg_s {
	u32 lns_mesh_xnum;
	u32 lns_mesh_ynum;
	u32 lns_mesh_alpmode;
	u32 lns_mesh_xlimit;
	u32 lns_mesh_ylimit;
	u32 lns_mesh_xscale;
	u32 lns_mesh_yscale;
	u32 lns_mesh_prtmode;
	u32 lns_mesh_lutnorm_grbg[4];
	u32 lns_mesh_alp[4];
} aisp_mesh_cfg_t;

typedef struct aisp_setting_fixed_cfg_s {
	u32 bac_hcoef[5];
	u32 bac_vcoef[5];
	u32 sqrt1_mode;
	u32 eotf1_mode;
	u32 wb_rate_rs;
	u32 curve_lc_en;
	u32 curve_dhz_en;
	u32 hsc_tap_num_0;
	u32 vsc_tap_num_0;
	u32 hsc_nor_rs_bits_0;
	u32 vsc_nor_rs_bits_0;
	u32 sqrt1_num[8];
	u32 sqrt1_stp[8];
	u32 eotf1_num[8];
	u32 eotf1_stp[8];
} aisp_setting_fixed_cfg_t;

typedef struct aisp_lut_fixed_cfg_s {
	u32 pat_bar24rgb[2][3][24];
	u32 ae_stat_blk_weight[17*15];
	u32 awb_stat_blk_weight[32*24];
	u32 fpnr_corr_val[2][2048*5];
	u32 sqrt0_lut[33];
	u32 sqrt1_lut[129];
	u32 decmp0_lut[33];
	u32 decmp1_lut[129];
	u32 eotf0_lut[33];
	u32 eotf1_lut[129];
	u32 lns_rad_lut129[129*4];
	u32 lns_mesh_lut[64*64*4];
	u32 pst_gamma_lut[129*4];
	u32 rgb_gamma_lut[129]; // none
	u32 gtm_lut[129];
	u32 CAC_table_Rx[1024];
	u32 CAC_table_Ry[1024];
	u32 CAC_table_Bx[1024];
	u32 CAC_table_By[1024];
	u32 ltm_lmin_blk[96]; //read
	u32 ltm_lmax_blk[96]; // read
	u32 ltm_histxptsbuf_blk[79];
	u32 ltm_ccrat_lut[63];
	u32 ltm_ccalp_lut[256];
	u32 lc_satur_lut[63];
	u32 dhz_sky_prot_lut[64];
	u32 ram_lcmap_nodes[96*6];
	u32 ram_dhzmap_nodes[96*5];
	u32 pps_h_luma_coef[33 * 8];
	u32 pps_v_luma_coef[33 * 8];
	u32 pps_h_chroma_coef[33 * 8];
	u32 pps_v_chroma_coef[33 * 8];
	u32 snr_lpf_phs_sel[96];
} aisp_lut_fixed_cfg_t;

typedef struct aisp_pat_cfg_s{
	u32 pat_fwin_en_0;
	u32 pat_fwin_en_1;
	u32 pat_xmode_0;
	u32 pat_xmode_1;
	u32 pat_ymode_0;
	u32 pat_ymode_1;
	u32 pat_xinvt_0;
	u32 pat_xinvt_1;
	u32 pat_yinvt_0;
	u32 pat_yinvt_1;
} aisp_pat_cfg_t;

typedef struct aisp_top_cfg_s {
	u32 src_inp_chn;
	u32 wdr_inp_chn;
	u32 decmp_en;
	u32 inp_fmt_en;
	u32 bac_en;
	u32 fpnr_en;
	u32 ge_en_0;
	u32 ge_en_1;
	u32 dpc_en_0;
	u32 dpc_en_1;
	u32 pat_en_0;
	u32 pat_en_1;
	u32 og_en_0;
	u32 og_en_1;
	u32 lcge_enable;
	u32 pdpc_enable;
	u32 cac_en;
	u32 rawcnr_enable;
	u32 snr1_en;
	u32 mc_tnr_en;
	u32 tnr0_en;
	u32 cubic_cs_en;
	u32 sqrt_en;
	u32 eotf_en;
	u32 ltm_en;
	u32 gtm_en;
	u32 lns_mesh_en;
	u32 lns_rad_en;
	u32 wb_en;
	u32 blc_en;
	u32 nr_en;
	u32 pk_en;
	u32 dnlp_en;
	u32 dhz_en;
	u32 lc_en;;
	u32 bsc_en;
	u32 cnr2_en;
	u32 pst_gamma_en;
	u32 ccm_en;
	u32 dmsc_en;
	u32 cm0_en;
	u32 pst_tnr_lite_en;
	u32 amcm_en;
	u32 ae_stat_en;
	u32 awb_stat_en;
	u32 af_stat_en;
	u32 wdr_stat_en;
	u32 expstitch_mode;
	u32 pst_mux_mode;
	u32 flkr_sta_pos;
	u32 awb_stat_switch;
	u32 ae_stat_switch;
	u32 af_stat_switch;
	u32 ae_input_2ln;
	u32 flkr_stat_en;
	u32 flkr_sta_input_format;
	u32 cubic_en;
	u32 pnrmif_en;
	u32 nrmif_en;
} aisp_top_cfg_t;

typedef struct aisp_hlc_cfg_s {
	u32 hlc_en;
	u32 hlc_luma_thd;
	u32 hlc_luma_trgt;
} aisp_hlc_cfg_t;

typedef struct aisp_cvr_cfg_s {
	u32 cvr_rect_en;
	u32 cvr_rect_hstart[FW_RECT_CHN_MAX];
	u32 cvr_rect_vstart[FW_RECT_CHN_MAX];
	u32 cvr_rect_hend[FW_RECT_CHN_MAX];
	u32 cvr_rect_vend[FW_RECT_CHN_MAX];
	u32 cvr_rect_val_y[FW_RECT_CHN_MAX];
	u32 cvr_rect_val_u[FW_RECT_CHN_MAX];
	u32 cvr_rect_val_v[FW_RECT_CHN_MAX];
} aisp_cvr_cfg_t;
typedef struct aisp_base_cfg_s {
	aisp_phase_setting_cfg_t phase_cfg;
	aisp_setting_fixed_cfg_t fxset_cfg;
	aisp_lut_fixed_cfg_t fxlut_cfg;
} aisp_base_cfg_t;

/********************** correction **********************/
typedef struct aisp_ccm_cfg_s {
	u32 ccm_4x3matrix[3][4];
	u32 csc3_en;
	u32 csc1_offset_inp[3];
	u32 csc1_offset_oup[3];
	u32 csc1_3x3mtrx_rs;
	u32 csc1_3x3matrix[3][3];
} aisp_ccm_cfg_t;

typedef struct aisp_csc_cfg_s {
	u32 cm0_offset_inp[3];
	u32 cm0_offset_oup[3];
	u32 cm0_3x3mtrx_rs;
	u32 cm0_3x3matrix[3][3];
} aisp_csc_cfg_t;
typedef struct aisp_mesh_crt_cfg_s {
	u32 lns_mesh_alp[4];
	u32 rad_lut65[65];
} aisp_mesh_crt_cfg_t;

typedef struct aisp_lns_cfg_s {
	u32 lns_rad_strength;
	u32 lns_mesh_strength;
} aisp_lns_cfg_t;

typedef struct aisp_blc_cfg_s {
	u32 fe_bl_ofst[5];
	u32 blc_ofst[5];
	u32 eotf_pre_ofst;
	u32 eotf_pst_ofst;
	u32 idg_ofst;
	u32 dg_ofst;
	u32 sqrt_pre_ofst;
	u32 sqrt_pst_ofst;
} aisp_blc_cfg_t;


/********************** enhance **********************/
typedef struct aisp_ltm_cfg_s{
	u32 ltm_lmin_blk[96]; //read
	u32 ltm_lmax_blk[96]; //read
	u32 ltm_lr_u28;
	u32 ltm_expblend_thd0;
	u32 ltm_expblend_thd1;
	u32 ltm_gmin_total;
	u32 ltm_gmax_total;
	u32 ltm_glbwin_hstart;
	u32 ltm_glbwin_hend;
	u32 ltm_glbwin_vstart;
	u32 ltm_glbwin_vend;
	u32 ltm_lo_gm_u6;
	u32 ltm_hi_gm_u7;
	u32 ltm_pow_y_u20;
	u32 ltm_pow_divisor_u23;
} aisp_ltm_cfg_t;

typedef struct aisp_ltm_enhc_cfg_s{
	u32 ltm_cc_en;
	u32 ltm_dtl_ehn_en;
	u32 ltm_vs_gtm_alpha;
	u32 ltm_lmin_med_en;
	u32 ltm_lmax_med_en;
	u32 ltm_b2luma_alpha;
	u32 ltm_satur_lut[63];
} aisp_ltm_enhc_cfg_t;

typedef struct aisp_wdr_cfg_s {
	u32 wdr_motiondect_en;
	u32 wdr_mdetc_withblc_mode;
	u32 wdr_mdetc_chksat_mode;
	u32 wdr_mdetc_motionmap_mode;
	u32 wdr_mdeci_chkstill_mode;
	u32 wdr_mdeci_addlong;
	u32 wdr_mdeci_still_thd;
	u32 wdr_forcelong_en;
	u32 wdr_forcelong_thdmode;
	u32 wdr_expcomb_maxavg_mode;
	u32 wdr_expcomb_maxavg_ratio;
	u32 wdr_stat_flt_en;
	u32 wdr_lexpratio_int64[4];
	u32 wdr_lmapratio_int64[3][2];
	u32 wdr_lexpcomp_gr_int64[3];
	u32 wdr_lexpcomp_gb_int64[3];
	u32 wdr_lexpcomp_rg_int64[3];
	u32 wdr_lexpcomp_bg_int64[3];
	u32 wdr_lexpcomp_ir_int64[3];
	u32 wdr_mdetc_sat_gr_thd;
	u32 wdr_mdetc_sat_gb_thd;
	u32 wdr_mdetc_sat_rg_thd;
	u32 wdr_mdetc_sat_bg_thd;
	u32 wdr_mdetc_sqrt_again_rg;
	u32 wdr_mdetc_sqrt_again_g;
	u32 wdr_mdetc_sqrt_again_bg;
	u32 wdr_mdetc_sqrt_again_ir;
	u32 wdr_mdetc_sqrt_dgain_rg;
	u32 wdr_mdetc_sqrt_dgain_g;
	u32 wdr_mdetc_sqrt_dgain_bg;
	u32 wdr_mdetc_sqrt_dgain_ir;
	u32 wdr_mdetc_lo_weight[3];
	u32 wdr_mdetc_hi_weight[3];
	u32 wdr_mdetc_noisefloor_g[2];
	u32 wdr_mdetc_noisefloor_rg[2];
	u32 wdr_mdetc_noisefloor_bg[2];
	u32 wdr_mdeci_sexpstill_gr_lsthd[2];
	u32 wdr_mdeci_sexpstill_gb_lsthd[2];
	u32 wdr_mdeci_sexpstill_rg_lsthd[2];
	u32 wdr_mdeci_sexpstill_bg_lsthd[2];
	u32 wdr_flong2_thd0[2];
	u32 wdr_flong2_thd1[2];
	u32 wdr_flong1_thd0;
	u32 wdr_flong1_thd1;
	u32 wdr_expcomb_maxratio;
	u32 wdr_expcomb_blend_slope;
	u32 wdr_expcomb_blend_thd0;
	u32 wdr_expcomb_blend_thd1;
	u32 wdr_expcomb_ir_blend_slope;
	u32 wdr_expcomb_ir_blend_thd0;
	u32 wdr_expcomb_ir_blend_thd1;
	u32 wdr_expcomb_maxsat_gr_thd;
	u32 wdr_expcomb_maxsat_gb_thd;
	u32 wdr_expcomb_maxsat_rg_thd;
	u32 wdr_expcomb_maxsat_bg_thd;
	u32 wdr_expcomb_maxsat_ir_thd;
	u32 wdr_expcomb_slope_weight;
	u32 wdr_expcomb_ir_slope_weight;
	u32 wdr_expcomb_maxavg_winsize;
	u32 wdr_flong2_colorcorrect_en;
	u32 wdr_mdeci_fullmot_thd;
	u32 comb_expratio_int64[3];
	u32 comb_exprratio_int1024[2];
	u32 comb_g_lsbarrier[4];
	u32 comb_rg_lsbarrier[4];
	u32 comb_bg_lsbarrier[4];
	u32 comb_ir_lsbarrier[4];
	u32 comb_maxratio;
	u32 comb_shortexp_mode;
	u32 wdr_force_exp_en;
	u32 wdr_force_exp_mode;
} aisp_wdr_cfg_t;

typedef struct aisp_wdr_blc_cfg_s {
	u32 wdr_blacklevel_gr;
	u32 wdr_blacklevel_gb;
	u32 wdr_blacklevel_rg;
	u32 wdr_blacklevel_bg;
	u32 wdr_blacklevel_ir;
	u32 wdr_blacklevel_wdr;
} aisp_wdr_blc_cfg_t;

typedef struct aisp_wdr_fmt_cfg_s{
	u32 sensor_bitdepth;
} aisp_wdr_fmt_cfg_t;

typedef struct aisp_wb_enhc_cfg_s{
	u32 awb_gain_256[5];
} aisp_wb_enhc_cfg_t;

typedef struct aisp_lc_cfg_s {
	u32 lc_histvld_thrd;
	u32 lc_blackbar_mute_thrd;
	u32 lc_pk_vld;
	u32 lc_pk_no_trd_mrgn;
	u32 lc_pk_1stb_th;
	u32 ram_lcmap_nodes[96*6];
} aisp_lc_cfg_t;

typedef struct aisp_lc_enhc_cfg_s {
	u32 lc_en;
	u32 lc_cc_en;
	u32 lc_blkblend_mode;
	u32 lc_lmtrat_minmax;
	u32 lc_contrast_low;
	u32 lc_contrast_hig;
	u32 lc_cntstscl_low;
	u32 lc_cntstscl_hig;
	u32 lc_cntstbvn_low;
	u32 lc_cntstbvn_hig;
	u32 lc_ypkbv_slope_lmt_1;
	u32 lc_ypkbv_slope_lmt_0;
	u32 lc_ypkbv_ratio_2;
	u32 lc_ypkbv_ratio_1;
	u32 lc_satur_lut[63];
} aisp_lc_enhc_cfg_t;

typedef struct aisp_dnlp_cfg_s {
	u32 dnlp_ygrid[64];
	u32 rgb_gamma_gain[4]; // none
	u32 rgb_gamma_ofst[4]; // none
} aisp_dnlp_cfg_t;

typedef struct aisp_dhz_cfg_s{
	u32 ram_dhz_nodes[96*5];
	u32 dhz_atmos_light;
	u32 dhz_atmos_light_inver;
	u32 dhz_sky_prot_stre;
	u32 dhz_sky_prot_offset;
	u32 dhz_satura_ratio_sky;
} aisp_dhz_cfg_t;

typedef struct aisp_dhz_enhc_cfg_s {
	u32 dhz_dlt_rat;
	u32 dhz_hig_dlt_rat;
	u32 dhz_low_dlt_rat;
	u32 dhz_lmtrat_lowc;
	u32 dhz_lmtrat_higc;
	u32 dhz_cc_en;
	u32 dhz_sky_prot_en;
	u32 dhz_sky_prot_stre;
	u32 dhz_sky_prot_lut[64];
} aisp_dhz_enhc_cfg_t;

typedef struct aisp_peaking_cfg_s{
	u32 pk_debug_edge;
	u32 drtlpf_theta_min_idx_replace;
	u32 pk_motion_adp_en;
	u32 bp_final_gain;
	u32 hp_final_gain;
	u32 pre_flt_strength;
	u32 hp_motion_adp_gain_lut[8];
	u32 bp_motion_adp_gain_lut[8];
	u32 pk_cir_hp_con2gain[5];
	u32 pk_cir_bp_con2gain[5];
	u32 pk_drt_hp_con2gain[5];
	u32 pk_drt_bp_con2gain[5];
	u32 pkgain_vsluma_lut[9];
	u32 pk_os_up;
	u32 pk_os_down;
	u32 pre_bpc_margin;
	s32 pk_bpf_vdtap05[3];
	s32 pk_hpf_vdtap05[3];
	s32 pk_bpf_hztap09[5];
	s32 pk_hpf_hztap09[5];
	s32 pkosht_vsluma_lut[9];
	s32 pk_circ_bpf_2d5x7[3][4];
	s32 pk_circ_hpf_2d5x7[3][4];
	u32 pk_osh_winsize;
	u32 pk_osv_winsize;
	u32 ltm_shrp_base_alpha;
	u32 ltm_shrp_r_u6;
	u32 ltm_shrp_s_u8;
	u32 ltm_shrp_smth_lvlsft;
} aisp_peaking_cfg_t;

typedef struct aisp_cm2_cfg_s {
	u32 cm2_adj_satglbgain_via_y[9];
	u32 cm2_global_sat;
	u32 cm2_global_hue;
	u32 cm2_luma_contrast;
	u32 cm2_luma_brightness;
	u32 cm2_adj_luma_via_hue[32];
	u32 cm2_adj_sat_via_hs[3][32];
	u32 cm2_adj_satgain_via_y[5][32];
	u32 cm2_adj_hue_via_h[32];
	u32 cm2_adj_hue_via_s[5][32];
	u32 cm2_adj_hue_via_y[5][32];
} aisp_cm2_cfg_t;
/********************** restoration **********************/
typedef struct aisp_ge_cfg_s {
	u32 ge_stat_edge_thd[2];
	u32 ge_hv_thrd[2];
	u32 ge_hv_wtlut_0[4];
	u32 ge_hv_wtlut_1[4];
} aisp_ge_cfg_t;

typedef struct aisp_fpnr_cfg_s {
	u32 fpnr_corr_gain0[2][5];
	u32 fpnr_corr_gain1[2][5];
} aisp_fpnr_cfg_t;

typedef struct aisp_dpc_cfg_s {
	u32 dpc_avg_gain_l0[2];
	u32 dpc_avg_gain_h0[2];
	u32 dpc_avg_gain_l1[2];
	u32 dpc_avg_gain_h1[2];
	u32 dpc_avg_gain_l2[2];
	u32 dpc_avg_gain_h2[2];
	u32 dpc_cond_en[2];
	u32 dpc_max_min_bias_thd[2];
	u32 dpc_std_diff_gain[2];
	u32 dpc_std_gain[2];
	u32 dpc_avg_dev_offset[2];

	u32 dpc_cor_en[2];
	u32 dpc_avg_dev_mode[2];
	u32 dpc_avg_mode[2];
	u32 dpc_avg_thd2_en[2];
	u32 dpc_highlight_en[2];
	u32 dpc_correct_mode[2];
	u32 dpc_write_to_lut[2];
} aisp_dpc_cfg_t;

typedef struct aisp_tnr_cfg_s {
	u32 rad_tnr0_en;
	u32 ma_mix_ratio;
	u32 ma_mix_th_x0;
	u32 ma_mix_th_x1;
	u32 ma_mix_th_x2;
	u32 ma_mix_h_th_y0;
	u32 ma_mix_h_th_y1;
	u32 ma_mix_h_th_y2;
	u32 ma_mix_l_th_y0;
	u32 ma_mix_l_th_y1;
	u32 ma_mix_l_th_y2;
	u32 ma_sad_pdtl4_x0;
	u32 ma_sad_pdtl4_x1;
	u32 ma_sad_pdtl4_x2;
	u32 ma_sad_pdtl4_y0;
	u32 ma_sad_pdtl4_y1;
	u32 ma_sad_pdtl4_y2;
	u32 ma_adp_dtl_mix_th_nfl;
	u32 ma_sad_th_mask_gain[4];
	u32 ma_mix_th_mask_gain[4];
	u32 ma_np_lut16[16];
	u32 pst_tnr_alp_lut[8];
	u32 ma_tnr_sad_cor_np_gain;
	u32 ma_tnr_sad_cor_np_ofst;
	u32 me_sad_cor_np_gain;
	u32 me_sad_cor_np_ofst;
	u32 ma_mix_h_th_gain[4];
	u32 lut_meta_sad_2alpha[64];
	u32 mc_meta2alpha[8][8];
	u32 ma_sad_var_th_x0;
	u32 ma_sad_var_th_x1;
	u32 ma_sad_var_th_x2;
	u32 ma_sad_var_th_y_0;
	u32 ma_sad_var_th_y_1;
	u32 ma_sad_var_th_y_2;
	u32 me_meta_sad_th0[3];
	u32 me_meta_sad_th1[3];
	u32 ma_sad_luma_adj_x[4];
	u32 ma_sad_luma_adj_y[5];
} aisp_tnr_cfg_t;

typedef struct aisp_snr_cfg_s {
	u32 snr_wt_lut[16];
	u32 snr_np_lut16[16];
	u32 me_np_lut16[16];
	u32 rawcnr_np_lut16[16];
	u32 snr_sad_cor_profile_adj;
	u32 snr_sad_cor_profile_ofst;
	u32 snr_sad_wt_sum_th[2];
	u32 snr_var_flat_th_x0;
	u32 snr_var_flat_th_x1;
	u32 snr_var_flat_th_x2;
	u32 snr_var_flat_th_y[3];
	u32 snr_sad_meta_ratio[4];
	u32 snr_wt_luma_gain[8];
	u32 snr_sad_meta2alp[8];
	u32 snr_mask_adj[8];
	u32 snr_meta_adj[8];
	u32 snr_cur_wt[8];
	u32 nry_strength;
	u32 nrc_strength;
	u32 snr_luma_adj_en;
	u32 snr_sad_wt_adjust_en;
	u32 snr_mask_en;
	u32 snr_meta_en;
	u32 rad_snr1_en;
	u32 snr_wt_var_adj_en;
	u32 snr_wt_var_meta_th;
	u32 snr_wt_var_th_x0;
	u32 snr_wt_var_th_x1;
	u32 snr_wt_var_th_x2;
	u32 snr_wt_var_th_y[3];
	u32 snr_grad_gain[5];
	u32 snr_sad_th_mask_gain[4];
	u32 snr_coring_mv_gain_x;
	u32 snr_coring_mv_gain_xn;
	u32 snr_coring_mv_gain_y[2];
} aisp_snr_cfg_t;

typedef struct aisp_rawcnr_cfg_s {
	u32 rawcnr_totblk_higfrq_en;
	u32 rawcnr_curblk_higfrq_en;
	u32 rawcnr_ishigfreq_mode;
	u32 rawcnr_sad_cor_np_gain;
	u32 rawcnr_meta_gain_lut[8];
	u32 rawcnr_higfrq_sublk_sum_dif_thd[2];
	u32 rawcnr_higfrq_curblk_sum_difnxn_thd[2];
	u32 rawcnr_thrd_ya_min;
	u32 rawcnr_thrd_ya_max;
	u32 rawcnr_thrd_ca_min;
	u32 rawcnr_thrd_ca_max;
	u32 rawcnr_sps_csig_weight5x5[25];
} aisp_rawcnr_cfg_t;

typedef struct aisp_cnr_cfg_s {
	u32 cnr2_satur_blk[1024];
	u32 cnr2_tdif_rng_lut[16];
	u32 cnr2_ydif_rng_lut[16];
	u32 cnr2_umargin_up;
	u32 cnr2_umargin_dw;
	u32 cnr2_vmargin_up;
	u32 cnr2_vmargin_dw;
	u32 cnr2_luma_osat_thd;
	u32 cnr2_fin_alp_mode;
	u32 cnr2_ctrst_xthd;
	u32 cnr2_adp_desat_vrt;
	u32 cnr2_adp_desat_hrz;
} aisp_cnr_cfg_t;

typedef struct aisp_dms_cfg_s{
	u32 plp_alp;
	u32 drt_ambg_mxerr_lmt_0;
	u32 drt_ambg_mxerr_lmt_1;
} aisp_dmsc_cfg_t;

typedef union {
	u32  key;
	struct {
		u32  aisp_tnr                 : 1 ;
		u32  aisp_snr                 : 1 ;
		u32  aisp_rawcnr              : 1 ;
		u32  aisp_cnr                 : 1 ;
		u32  aisp_dmsc                : 1 ;
		u32  bitRsv                   : 27; /* H  ; [5:31] */
	};
}  aisp_nr_valid_ctrl;

typedef struct aisp_nr_cfg_s {
	aisp_nr_valid_ctrl   pvalid;
	aisp_tnr_cfg_t       tnr_cfg;
	aisp_snr_cfg_t       snr_cfg;
	aisp_rawcnr_cfg_t    rawcnr_cfg;
	aisp_cnr_cfg_t       cnr_cfg;
	aisp_dmsc_cfg_t      dmsc_cfg;
} aisp_nr_cfg_t;

typedef struct aisp_misc_cfg_s{
	aisp_pat_cfg_t pat_cfg;
	u32 tnr_bits;
} aisp_misc_cfg_t;

typedef struct _isp_hwreg_t {
	u32 addr;
	u32 val;
	u32 mask;
	u32 len;
} isp_hwreg_t;

typedef struct aisp_custom_cfg_s {
	isp_hwreg_t settings[200];
} aisp_custom_cfg_t;


typedef union {
	u64  key;
	struct {
		u64  aisp_dgain       : 1;
		u64  aisp_wb_change   : 1;
		u64  aisp_wb_luma     : 1;
		u64  aisp_wb_triangle : 1;
		u64  aisp_expo_mode   : 1;
		u64  aisp_mesh        : 1;
		u64  aisp_top         : 1;
		u64  aisp_hlc         : 1;
		u64  aisp_cvr         : 1;
		u64  aisp_base        : 1;
		u64  aisp_ge          : 1;
		u64  aisp_fpnr        : 1;
		u64  aisp_dpc         : 1;
		u64  aisp_cm2         : 1;
		u64  aisp_nr          : 1;
		u64  aisp_ccm         : 1;
		u64  aisp_csc         : 1;
		u64  aisp_lns         : 1;
		u64  aisp_blc         : 1;
		u64  aisp_mesh_crt    : 1;
		u64  aisp_ltm         : 1;
		u64  aisp_ltm_enhc    : 1;
		u64  aisp_wdr         : 1;
		u64  aisp_wdr_blc     : 1;
		u64  aisp_wdr_fmt     : 1;
		u64  aisp_wb_enhc     : 1;
		u64  aisp_lc          : 1;
		u64  aisp_lc_enhc     : 1;
		u64  aisp_dnlp        : 1;
		u64  aisp_dhz         : 1;
		u64  aisp_dhz_enhc    : 1;
		u64  aisp_peaking     : 1;
		u64  aisp_misc        : 1;
		u64  aisp_custom      : 1 ;
		u64  bitRsv           : 29; /* H  ; [35:63] */
	};
}  aisp_param_ctrl;

typedef struct aisp_param_s{
	aisp_param_ctrl           pvalid;
	aisp_dgain_cfg_t          dgain;
	aisp_wb_change_cfg_t      wb_change;
	aisp_wb_luma_cfg_t        wb_luma;
	aisp_wb_triangle_cfg_t    wb_triangle;
	aisp_expo_mode_cfg_t      expo_mode;

	aisp_mesh_cfg_t           mesh_cfg;
	aisp_top_cfg_t            top_cfg;
	aisp_hlc_cfg_t            hlc_cfg;
	aisp_cvr_cfg_t            cvr_cfg;
	aisp_base_cfg_t           base_cfg;

	aisp_ge_cfg_t             ge_cfg;
	aisp_fpnr_cfg_t           fpnr_cfg;
	aisp_dpc_cfg_t            dpc_cfg;
	aisp_cm2_cfg_t            cm2_cfg;
	aisp_nr_cfg_t             nr_cfg;

	aisp_ccm_cfg_t            ccm_cfg;
	aisp_csc_cfg_t            csc_cfg;
	aisp_lns_cfg_t            lns_cfg;
	aisp_blc_cfg_t            blc_cfg;
	aisp_mesh_crt_cfg_t       mesh_crt_cfg;

	aisp_ltm_cfg_t            ltm_cfg;
	aisp_ltm_enhc_cfg_t       ltm_enhc_cfg;
	aisp_wdr_cfg_t            wdr_cfg;
	aisp_wdr_blc_cfg_t        wdr_blc;
	aisp_wdr_fmt_cfg_t        wdr_fmt;
	aisp_wb_enhc_cfg_t        wb_enhc;
	aisp_lc_cfg_t             lc_cfg;
	aisp_lc_enhc_cfg_t        lc_enhc_cfg;
	aisp_dnlp_cfg_t           dnlp_cfg;
	aisp_dhz_cfg_t            dhz_cfg;
	aisp_dhz_enhc_cfg_t       dhz_enhc_cfg;
	aisp_peaking_cfg_t        peaking_cfg;

	aisp_misc_cfg_t           misc_cfg;
	aisp_custom_cfg_t         custom_cfg;
} aisp_param_t;

#endif // __AML_ISP_CFG_H__
