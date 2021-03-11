/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __FRC_COMMON_H__
#define __FRC_COMMON_H__

#define MAX(a, b) ({ \
			typeof(a) _a = a; \
			typeof(b) _b = b; \
			_a > _b ? _a : _b; \
		})
#define MIN(a, b) ({ \
			typeof(a) _a = a; \
			typeof(b) _b = b; \
			_a < _b ? _a : _b; \
		})

#define ABS(a)	({ \
			typeof(a) _a = a; \
			((_a) > 0 ? (_a) : -(_a));\
		})

#define BIT_0		0x00000001
#define BIT_1		0x00000002
#define BIT_2		0x00000004
#define BIT_3		0x00000008
#define BIT_4		0x00000010
#define BIT_5		0x00000020
#define BIT_6		0x00000040
#define BIT_7		0x00000080
#define BIT_8		0x00000100
#define BIT_9		0x00000200
#define BIT_10		0x00000400
#define BIT_11		0x00000800
#define BIT_12		0x00001000
#define BIT_13		0x00002000
#define BIT_14		0x00004000
#define BIT_15		0x00008000
#define BIT_16		0x00010000
#define BIT_17		0x00020000
#define BIT_18		0x00040000
#define BIT_19		0x00080000
#define BIT_20		0x00100000
#define BIT_21		0x00200000
#define BIT_22		0x00400000
#define BIT_23		0x00800000
#define BIT_24		0x01000000
#define BIT_25		0x02000000
#define BIT_26		0x04000000
#define BIT_27		0x08000000
#define BIT_28		0x10000000
#define BIT_29		0x20000000
#define BIT_30		0x40000000
#define BIT_31		0x80000000

#define FRC_DS_11	0
#define FRC_DS_12	1
#define FRC_DS_14	2

enum chg_flag {
	FRC_CHG_NONE = 0,
	FRC_CHG_HV_SIZE = 0x1,
};

//-------------------------------------------------------------------sence start
struct st_frc_rd {
	u32 glb_h_dtl;
	u32 glb_v_dtl;
	u8  apl;
	u8  input_fid;
	u8  clear_vbuffer_en;
};

//-----LOGO---------
struct st_logo_detect_rd {
	u32 iplogo_pxl_cnt;           // ro_iplogo_logopix_cnt
	u32 iplogo_pxl_rgncnt[12];    // ro_iplogo_pixlogo_rgn_cnt[12]
	u32 iplogo_osd_rgncnt[12];    // ro_iplogo_osdbit_rgn_cnt[12]
};

//-----------FILM ------------------
struct st_film_detect_rd {
	u32 glb_motion_all;
	u32 glb_motion_all_film;
	u32 glb_motion_all_count;
	u32 glb_motion_all_film_count;
	u32 region_motion_wind6[6];
	u32 region1_motion_wind12[12];
	u32 region2_motion_wind12[12];
	u32 region_motion_wind6_count[6];
	u32 region1_motion_wind12_count[12];
	u32 region2_motion_wind12_count[12];
};

//----------BBD--------------
struct st_bbd_rd {
	u8  top_valid1;                 // u1, ro_bb_top_valid1
	u8  top_valid2;                 // u1, ro_bb_top_valid2
	u8  bot_valid1;                 // u1, ro_bb_bot_valid1
	u8  bot_valid2;                 // u1, ro_bb_bot_valid2
	u8  rough_lft_valid1;              // u1, ro_bb_rough_lfg_valid1
	u8  rough_lft_valid2;              // u1, ro_bb_rough_lfg_valid2
	u8  rough_rit_valid1;              // u1, ro_bb_rough_rit_valid1
	u8  rough_rit_valid2;              // u1, ro_bb_rough_rit_valid2
	u8  finer_lft_valid1;              // u1, ro_bb_finer_lft_valid1
	u8  finer_lft_valid2;              // u1, ro_bb_finer_lft_valid2
	u8  finer_rit_valid1;              // u1, ro_bb_finer_rit_valid1
	u8  finer_rit_valid2;              // u1, ro_bb_finer_rit_valid2

	u16 top_posi1;                  // u16, ro_bb_top_posi1
	u16 top_posi2;                  // u16, ro_bb_top_posi2
	u16 top_cnt1;                   // u16, ro_bb_top_cnt1
	u16 top_cnt2;                   // u16, ro_bb_top_cnt2
	u16 bot_posi1;                  // u16, ro_bb_bot_posi1
	u16 bot_posi2;                  // u16, ro_bb_bot_posi2
	u16 bot_cnt1;                   // u16, ro_bb_bot_cnt1
	u16 bot_cnt2;                   // u16, ro_bb_bot_cnt2

	u16 rough_lft_posi1;               // u16, ro_bb_rough_lft_posi1
	u16 rough_lft_posi2;               // u16, ro_bb_rough_lft_posi2
	u16 rough_lft_cnt1;                // u16, ro_bb_rough_lft_cnt1
	u16 rough_lft_cnt2;                // u16, ro_bb_rough_lft_cnt2
	u16 rough_rit_posi1;               // u16, ro_bb_rough_rit_posi1
	u16 rough_rit_posi2;               // u16, ro_bb_rough_rit_posi2
	u16 rough_rit_cnt1;                // u16, ro_bb_rough_rit_cnt1
	u16 rough_rit_cnt2;                // u16, ro_bb_rough_rit_cnt2

	u8 rough_lft_index1;               // u8, ro_bb_rough_lft_index1
	u8 rough_lft_index2;               // u8, ro_bb_rough_lft_index2
	u8 rough_rit_index1;               // u8, ro_bb_rough_rit_index1
	u8 rough_rit_index2;               // u8, ro_bb_rough_rit_index2

	u16 finer_lft_posi1;               // u16, ro_bb_finer_lft_posi1
	u16 finer_lft_posi2;               // u16, ro_bb_finer_lft_posi2
	u16 finer_lft_cnt1;                // u16, ro_bb_finer_lft_cnt1
	u16 finer_lft_cnt2;                // u16, ro_bb_finer_lft_cnt2
	u16 finer_rit_posi1;               // u16, ro_bb_finer_rit_posi1
	u16 finer_rit_posi2;               // u16, ro_bb_finer_rit_posi2
	u16 finer_rit_cnt1;                // u16, ro_bb_finer_rit_cnt1
	u16 finer_rit_cnt2;                // u16, ro_bb_finer_rit_cnt2

	u32 oob_apl_top_sum;               // u32, ro_bb_oob_apl_top_sum
	u32 oob_apl_bot_sum;               // u32, ro_bb_oob_apl_bot_sum
	u32 oob_apl_lft_sum;               // u32, ro_bb_oob_apl_lft_sum
	u32 oob_apl_rit_sum;               // u32, ro_bb_oob_apl_rit_sum

	u32 oob_h_detail_top_val;          // u32, ro_bb_oob_h_detail_top_val
	u32 oob_v_detail_top_val;          // u32, ro_bb_oob_v_detail_top_val
	u32 oob_h_detail_top_cnt;          // u32, ro_bb_oob_h_detail_top_cnt
	u32 oob_v_detail_top_cnt;          // u32, ro_bb_oob_v_detail_top_cnt
	u32 oob_h_detail_bot_val;          // u32, ro_bb_oob_h_detail_bot_val
	u32 oob_v_detail_bot_val;          // u32, ro_bb_oob_v_detail_bot_val
	u32 oob_h_detail_bot_cnt;          // u32, ro_bb_oob_h_detail_bot_cnt
	u32 oob_v_detail_bot_cnt;          // u32, ro_bb_oob_v_detail_bot_cnt
	u32 oob_h_detail_lft_val;          // u32, ro_bb_oob_h_detail_lft_val
	u32 oob_v_detail_lft_val;          // u32, ro_bb_oob_v_detail_lft_val
	u32 oob_h_detail_lft_cnt;          // u32, ro_bb_oob_h_detail_lft_cnt
	u32 oob_v_detail_lft_cnt;          // u32, ro_bb_oob_v_detail_lft_cnt
	u32 oob_h_detail_rit_val;          // u32, ro_bb_oob_h_detail_rit_val
	u32 oob_v_detail_rit_val;          // u32, ro_bb_oob_v_detail_rit_val
	u32 oob_h_detail_rit_cnt;          // u32, ro_bb_oob_h_detail_rit_cnt
	u32 oob_v_detail_rit_cnt;          // u32, ro_bb_oob_v_detail_rit_cnt

	u32 h_detail_glb_val;              // u32, ro_bb_h_detail_glb_val
	u32 v_detail_glb_val;              // u32, ro_bb_v_detail_glb_val
	u32 h_detail_glb_cnt;              // u32, ro_bb_h_detail_glb_cnt
	u32 v_detail_glb_cnt;              // u32, ro_bb_v_detail_glb_cnt

	u16 oob_motion_diff_top_val;       // u16, ro_bb_oob_motion_diff_top_val
	u16 oob_motion_diff_bot_val;       // u16, ro_bb_oob_mog_stFrc_RDtion_diff_bot_val
	u16 oob_motion_diff_lft_val;       // u16, ro_bb_oob_motion_diff_lft_val
	u16 oob_motion_diff_rit_val;       // u16, ro_bb_oob_motion_diff_rit_val

	u16 flat_top_cnt;                  // u16, ro_bb_flat_top_cnt
	u16 flat_bot_cnt;                  // u16, ro_bb_flat_bot_cnt
	u16 flat_lft_cnt;                  // u16, ro_bb_flat_lft_cnt
	u16 flat_rit_cnt;                  // u16, ro_bb_flat_rit_cnt

	u8  top_edge_posi_valid1;          // u1, ro_bb_top_edge_posi_valid1
	u8  bot_edge_posi_valid1;          // u1, ro_bb_bot_edge_posi_valid2
	u8  top_edge_posi_valid2;          // u1, ro_top_edge_posi_valid1
	u8  bot_edge_posi_valid2;          // u1, ro_bot_edge_posi_valid2
	u8  lft_edge_posi_valid1;          // u1, ro_bb_lft_edge_posi_valid1
	u8  rit_edge_posi_valid1;          // u1, ro_bb_rit_edge_posi_valid2
	u8  lft_edge_posi_valid2;          // u1, ro_bb_lft_edge_posi_valid1
	u8  rit_edge_posi_valid2;          // u1, ro_bb_rit_edge_posi_valid2
	u8  top_edge_first_posi_valid;     // u1, ro_bb_top_edge_first_posi_valid
	u8  bot_edge_first_posi_valid;     // u1, ro_bb_bot_edge_first_posi_valid
	u8  lft_edge_first_posi_valid;     // u1, ro_bb_lft_edge_first_posi_valid
	u8  rit_edge_first_posi_valid;     // u1, ro_bb_rit_edge_first_posi_valid

	u16 top_edge_posi1;                // u16, ro_bb_top_edge_posi1
	u16 top_edge_posi2;                // u16, ro_bb_top_edge_posi2
	u16 top_edge_val1;                 // u16, ro_bb_top_edge_val1
	u16 top_edge_val2;                 // u16, ro_bb_top_edge_val2
	u16 bot_edge_posi1;                // u16, ro_bb_bot_edge_posi1
	u16 bot_edge_posi2;                // u16, ro_bb_bot_edge_posi2
	u16 bot_edge_val1;                 // u16, ro_bb_bot_edge_val1
	u16 bot_edge_val2;                 // u16, ro_bb_bot_edge_val2
	u16 lft_edge_posi1;                // u16, ro_bb_lft_edge_posi1
	u16 lft_edge_posi2;                // u16, ro_bb_lft_edge_posi2
	u16 lft_edge_val1;                 // u16, ro_bb_lft_edge_val1
	u16 lft_edge_val2;                 // u16, ro_bb_lft_edge_val2
	u16 rit_edge_posi1;                // u16, ro_bb_rit_edge_posi1
	u16 rit_edge_posi2;                // u16, ro_bb_rit_edge_posi2
	u16 rit_edge_val1;                 // u16, ro_bb_rit_edge_val1
	u16 rit_edge_val2;                 // u16, ro_bb_rit_edge_val2

	u16 top_edge_first_posi;           // u16, ro_bb_top_edge_first_posi
	u16 top_edge_first_val;            // u16, ro_bb_top_edge_first_val
	u16 bot_edge_first_posi;           // u16, ro_bb_bot_edge_first_posi
	u16 bot_edge_first_val;            // u16, ro_bb_bot_edge_first_val
	u16 lft_edge_first_posi;           // u16, ro_bb_lft_edge_first_posi
	u16 lft_edge_first_val;            // u16, ro_bb_lft_edge_first_val
	u16 rit_edge_first_posi;           // u16, ro_bb_rit_edge_first_posi
	u16 rit_edge_first_val;            // u16, ro_bb_rit_edge_first_val

	u8  rough_lft_motion_posi_valid1;  // u1, ro_bb_rough_lft_motion_posi_valid1
	u8  rough_lft_motion_posi_valid2;  // u1, ro_bb_rough_lft_motion_posi_valid2
	u8  rough_rit_motion_posi_valid1;  // u1, ro_bb_rough_rit_motion_posi_valid1
	u8  rough_rit_motion_posi_valid2;  // u1, ro_bb_rough_rit_motion_posi_valid2
	u8  top_motion_posi_valid1;        // u1, ro_bb_top_motion_posi_valid1
	u8  bot_motion_posi_valid1;        // u1, ro_bb_bot_motion_posi_valid1
	u8  top_motion_posi_valid2;        // u1, ro_bb_top_motion_posi_valid2
	u8  bot_motion_posi_valid2;        // u1, ro_bb_bot_motion_posi_valid2
	u8  lft_motion_posi_valid1;        // u1, ro_bb_lft_motion_posi_valid1
	u8  rit_motion_posi_valid1;        // u1, ro_bb_rit_motion_posi_valid1
	u8  lft_motion_posi_valid2;        // u1, ro_bb_lft_motion_posi_valid2
	u8  rit_motion_posi_valid2;        // u1, ro_bb_rit_motion_posi_valid2

	u16 finer_lft_motion_th1;          // u12, ro_bb_finer_lft_motion_th1
	u16 finer_lft_motion_th2;          // u12, ro_bb_finer_lft_motion_th2
	u16 finer_rit_motion_th1;          // u12, ro_bb_finer_rit_motion_th1
	u16 finer_rit_motion_th2;          // u12, ro_bb_finer_rit_motion_th2

	u16 top_motion_posi1;              // u16, ro_bb_top_motion_posi1
	u16 top_motion_posi2;              // u16, ro_bb_top_motion_posi2
	u16 top_motion_cnt1;               // u16, ro_bb_top_motion_cnt1
	u16 top_motion_cnt2;               // u16, ro_bb_top_motion_cnt2
	u16 bot_motion_posi1;              // u16, ro_bb_bot_motion_posi1
	u16 bot_motion_posi2;              // u16, ro_bb_bot_motion_posi2
	u16 bot_motion_cnt1;               // u16, ro_bb_bot_motion_cnt1
	u16 bot_motion_cnt2;               // u16, ro_bb_bot_motion_cnt2

	u8  rough_lft_motion_index1;       // u8, ro_bb_rough_lft_motion_index1
	u8  rough_lft_motion_index2;       // u8, ro_bb_rough_lft_motion_index2
	u8  rough_rit_motion_index1;       // u8, ro_bb_rough_rit_motion_index1
	u8  rough_rit_motion_index2;       // u8, ro_bb_rough_rit_motion_index2

	u16 rough_lft_motion_posi1;        // u16, ro_bb_rough_lft_motion_posi1
	u16 rough_lft_motion_posi2;        // u16, ro_bb_rough_lft_motion_posi2
	u16 rough_rit_motion_posi1;        // u16, ro_bb_rough_rit_motion_posi1
	u16 rough_rit_motion_posi2;        // u16, ro_bb_rough_rit_motion_posi2
	u16 rough_lft_motion_cnt1;         // u16, ro_bb_rough_lft_motion_cnt1
	u16 rough_lft_motion_cnt2;         // u16, ro_bb_rough_lft_motion_cnt2
	u16 rough_rit_motion_cnt1;         // u16, ro_bb_rough_rit_motion_cnt1
	u16 rough_rit_motion_cnt2;         // u16, ro_bb_rough_rit_motion_cnt2

	u16 lft_motion_posi1;              // u16, ro_bb_lft_motion_posi1
	u16 lft_motion_posi2;              // u16, ro_bb_lft_motion_posi2
	u16 lft_motion_cnt1;               // u16, ro_bb_lft_motion_cnt1
	u16 lft_motion_cnt2;               // u16, ro_bb_lft_motion_cnt2
	u16 rit_motion_posi1;              // u16, ro_bb_rit_motion_posi1
	u16 rit_motion_posi2;              // u16, ro_bb_rit_motion_posi2
	u16 rit_motion_cnt1;               // u16, ro_bb_rit_motion_cnt1
	u16 rit_motion_cnt2;               // u16, ro_bb_rit_motion_cnt1

	u32 finer_hist_data_0;             // u32, ro_bb_finer_hist_data_0
	u32 finer_hist_data_1;             // u32, ro_bb_finer_hist_data_1
	u32 finer_hist_data_2;             // u32, ro_bb_finer_hist_data_2
	u32 finer_hist_data_3;             // u32, ro_bb_finer_hist_data_3
	u32 finer_hist_data_4;             // u32, ro_bb_finer_hist_data_4
	u32 finer_hist_data_5;             // u32, ro_bb_finer_hist_data_5
	u32 finer_hist_data_6;             // u32, ro_bb_finer_hist_data_6
	u32 finer_hist_data_7;             // u32, ro_bb_finer_hist_data_7
	u32 finer_hist_data_8;             // u32, ro_bb_finer_hist_data_8
	u32 finer_hist_data_9;             // u32, ro_bb_finer_hist_data_9
	u32 finer_hist_data_10;            // u32, ro_bb_finer_hist_data_10
	u32 finer_hist_data_11;            // u32, ro_bb_finer_hist_data_11
	u32 finer_hist_data_12;            // u32, ro_bb_finer_hist_data_12
	u32 finer_hist_data_13;            // u32, ro_bb_finer_hist_data_13
	u32 finer_hist_data_14;            // u32, ro_bb_finer_hist_data_14
	u32 finer_hist_data_15;            // u32, ro_bb_finer_hist_data_15
	u32 finer_hist_data_16;            // u32, ro_bb_finer_hist_data_16
	u32 finer_hist_data_17;            // u32, ro_bb_finer_hist_data_17
	u32 finer_hist_data_18;            // u32, ro_bb_finer_hist_data_18
	u32 finer_hist_data_19;            // u32, ro_bb_finer_hist_data_19
	u32 finer_hist_data_20;            // u32, ro_bb_finer_hist_data_20
	u32 finer_hist_data_21;            // u32, ro_bb_finer_hist_data_21
	u32 finer_hist_data_22;            // u32, ro_bb_finer_hist_data_22
	u32 finer_hist_data_23;            // u32, ro_bb_finer_hist_data_23
	u32 finer_hist_data_24;            // u32, ro_bb_finer_hist_data_24
	u32 finer_hist_data_25;            // u32, ro_bb_finer_hist_data_25
	u32 finer_hist_data_26;            // u32, ro_bb_finer_hist_data_26
	u32 finer_hist_data_27;            // u32, ro_bb_finer_hist_data_27
	u32 finer_hist_data_28;            // u32, ro_bb_finer_hist_data_28
	u32 finer_hist_data_29;            // u32, ro_bb_finer_hist_data_29
	u32 finer_hist_data_30;            // u32, ro_bb_finer_hist_data_30
	u32 finer_hist_data_31;            // u32, ro_bb_finer_hist_data_31

	u8  max1_hist_idx;                 // u8,  ro_bb_max1_hist_idx
	u8  max2_hist_idx;                 // u8,  ro_bb_max2_hist_idx
	u8  min1_hist_idx;                 // u8,  ro_bb_min1_hist_idx
	u32 max1_hist_cnt;                 // u32, ro_bb_max1_hist_cnt
	u32 max2_hist_cnt;                 // u32, ro_bb_max2_hist_cnt
	u32 min1_hist_cnt;                 // u32, ro_bb_min1_hist_cnt

	u32 apl_glb_sum;                   // u32, ro_bb_apl_glb_sum
};

//--------ME-------------
struct st_me_rd {
	//u32 Glb_TC;
	// gmv
	u8  gmv_invalid;        //ro_me_gmv_inalid
	s16 gmvx;               //ro_me_gmv.vector[0]
	s16 gmvy;               //ro_me_gmv.vector[1]
	s16 gmvx_2nd;           //ro_me_gmv_2nd.vector[0]
	s16 gmvy_2nd;           //ro_me_gmv_2nd.vector[1]
	s16 region_gmvx[12];    //ro_me_region_gmv.vector[0]
	s16 region_gmvy[12];    //ro_me_region_gmv.vector[1]
	s16 region_gmvx_2d[12]; //ro_me_region_gmv_2nd.vector[0]
	s16 region_gmvy_2d[12]; //ro_me_region_gmv_2nd.vector[1]
	s16 gmvx_mux;           //ro_me_gmv_mux.vector[0]
	s16 gmvy_mux;           //ro_me_gmv_mux.vector[1]
	u32 gmv_cnt;            //ro_me_gmv_cnt
	u32 gmv_dtl_cnt;        //ro_me_gmv_dtl_cnt
	u32 gmv_total_sum;      //ro_me_gmv_total_sum
	u32 gmv_neighbor_cnt;   //ro_me_gmv_neighbor_cnt
	u8  gmv_mux_invalid;    //ro_me_gmv_mux_invalid
	u32 gmv_small_neighbor_cnt; // ro_me_gmv_small_neighbor_cnt

	// temporal consistence
	u32 glb_t_consis[4];            //ro_me_glb_t_consis
	u32 region_t_consis[12];        //ro_me_region_t_consis

	// fall back
	u32 region_fb_dtl_sum[6][8];    //ro_me_region_fb_dtl_sum
	u32 region_fb_t_consis[6][8];   //ro_me_region_fb_t_consis
	u32 region_fb_s_consis[6][8];   //ro_me_region_fb_s_consis
	u32 region_fb_sad_sum[6][8];    //ro_me_region_fb_sad_sum
	u32 region_fb_sad_cnt[6][8];    //ro_me_region_fb_sad_cnt

	// zmv
	u32 zmv_cnt;            //ro_me_zmv_cnt

	// apl
	u32 glb_apl_sum[4];     //ro_me_glb_apl_sum

	// sad
	u32 glb_sad_sum[4];     //ro_me_glb_sad_sum
	u32 glb_bad_sad_cnt[4]; //ro_me_glb_bad_sad_cnt

	// glb
	u32 glb_cnt[4];         //ro_me_glb_cnt
	u32 glb_unstable_cnt;   //ro_me_glb_unstable_cnt

	// activity
	u32 glb_act_hi_x_cnt;   //ro_me_glb_act_hi_x_dtl_cnt
	u32 glb_act_hi_y_cnt;   //ro_me_glb_act_hi_y_dtl_cnt
	u32 glb_pos_hi_y_cnt[22];   //ro_me_glb_pos_hi_y_dtl_cnt
	u32 glb_neg_lo_y_cnt[22];   //ro_me_glb_neg_lo_y_dtl_cnt
};

//------VP--------
struct st_vp_rd {
	//u32 cover_cnt;

	// oct
	u32 glb_cover_cnt;              //ro_vp_global_oct_cover_cnt
	u32 glb_uncov_cnt;              //ro_vp_global_oct_uncov_cnt
	u32 region_cover_cnt[12];       //ro_vp_region_oct_cover_cnt
	u32 region_uncov_cnt[12];       //ro_vp_region_cot_uncov_cnt
	s32 oct_gmvx_sum;               //ro_vp_oct_gmvx_sum
	s32 oct_gmvy_sum;               //ro_vp_oct_gmvy_sum
	u32 oct_gmv_diff;               //ro_vp_oct_gmv_diff
};

//------MC--------
struct st_mc_rd {
	//  logo ro cnt
	u32 pre_mc_logo_cnt;    //  ro_mc_pre_inside_mclogo_cnt
	u32 cur_mc_logo_cnt;    //  ro_mc_cur_inside_mclogo_cnt

	u32 pre_ip_logo_cnt;    //  ro_mc_pre_inside_iplogo_cnt
	u32 cur_ip_logo_cnt;    //  ro_mc_cur_inside_iplogo_cnt

	u32 pre_me_logo_cnt;    //  ro_mc_pre_inside_melogo_cnt
	u32 cur_me_logo_cnt;    //  ro_mc_cur_inside_melogo_cnt

	//  srch range scale
	u8  luma_abv_xscale;    //  u1  ro_mc_srch_rng_luma_abv_xscale
	u8  luma_abv_yscale;    //  u1  ro_mc_srch_rng_luma_abv_yscale
	u8  luma_blw_xscale;    //  u1  ro_mc_srch_rng_luma_blw_xscale
	u8  luma_blw_yscale;    //  u1  ro_mc_srch_rng_luma_blw_yscale

	u8  chrm_abv_xscale;    //  u1  ro_mc_srch_rng_chrm_abv_xscale
	u8  chrm_abv_yscale;    //  u1  ro_mc_srch_rng_chrm_abv_yscale
	u8  chrm_blw_xscale;    //  u1  ro_mc_srch_rng_chrm_blw_xscale
	u8  chrm_blw_yscale;    //  u1  ro_mc_srch_rng_chrm_blw_yscale

	//  srch range ofst
	s16 pre_luma_lbuf_abv_ofst; //  s9  ro_mc_pre_lbuf_luma_ofst_0
	s16 pre_luma_lbuf_blw_ofst; //  s9  ro_mc_pre_lbuf_luma_ofst_1
	u16 pre_luma_lbuf_total;    //  u9  ro_mc_pre_lbuf_luma_ofst_2

	s16 pre_chrm_lbuf_abv_ofst; //  s9  ro_mc_pre_lbuf_chrm_ofst_0
	s16 pre_chrm_lbuf_blw_ofst; //  s9  ro_mc_pre_lbuf_chrm_ofst_1
	u16 pre_chrm_lbuf_total;    //  u9  ro_mc_pre_lbuf_chrm_ofst_2

	s16 cur_luma_lbuf_abv_ofst; //  s9  ro_mc_cur_lbuf_luma_ofst_0
	s16 cur_luma_lbuf_blw_ofst; //  s9  ro_mc_cur_lbuf_luma_ofst_1
	u16 cur_luma_lbuf_total;    //  u9  ro_mc_cur_lbuf_luma_ofst_2

	s16 cur_chrm_lbuf_abv_ofst; //  s9  ro_mc_cur_lbuf_chrm_ofst_0
	s16 cur_chrm_lbuf_blw_ofst; //  s9  ro_mc_cur_lbuf_chrm_ofst_1
	u16 cur_chrm_lbuf_total;    //  u9  ro_mc_cur_lbuf_chrm_ofst_2
};

//---------------------------------------------------------scene end

//---------------------------------------------------------bbd start
struct st_search_final_line_para {
	// BBD FW
	//for fw
	u8 pattern_detect_en; // 1;	//  u1,     0:close, using bb_mode_switch,
					//1: open, detect more than zeros in img, bbd off;
	u8 pattern_dect_ratio; // 109;	//  u7,     85% ratio/128
	//  u4,     0:use pre scheme 1:use new scheme 2 use new soft scheme
	u8 mode_switch;
	u8 ds_xyxy_force; // 1;	       //  u1,	   force me bbd full resolution, dft1
	//  u1,	   0: sel letter box1 soft; 1: sel letter box2 strong; dft1
	u8 sel2_high_mode; // 1;
	//  u1,     0: use sel2_high_mode for each line; 1: according to
	u8 motion_sel1_high_mode;
					//motion line posi compared with lb line2,
					//if line2 more than motion, use line1.
	u8 black_th_gen_en;
	u16 black_th_max; // 80;		 //  u10,    black pixel th max val,	 dft80
	u16 black_th_min; // 12;		 //  u10,    black pixel th min val,	 dft12
	u8 edge_th_gen_en;
	u16 edge_th_max;// = 60;				 //	 u10
	u16 edge_th_min;// = 6;				 //	 u10
	u8 hist_scale_depth; // 2;	    //	u2,	0:x4 1:x8 2:x16 3:x32	dft 2
	u8 black_th_gen_mode; // 1;	//  u2,     0: max 1:min 2:avg
	u8 motion_posi_strong_en; // 1;	       //  u1,	     0:outter, 1:inner, dft1
	u8 edge_choose_mode;
	u8 edge_choose_delta;
	u16 edge_choose_row_th;// = 600;		 //	 u16
	u16 edge_choose_col_th;// = 300;		 //	 u16

	u16 pre_motion_posi[4] ; // 0;	 // u16:     pre top motion posi,	 dft0

	// caption
	u8 caption[4]; // 0;		   // u1:      lft caption: 0:no 1:exist

	u8 caption_change[4]; // 0;	   // u1:      lft caption change? 0: no 1:change

	// fw pre line data
	u16 pre_final_posi[4]; // 0;	     // u16:	 pre_lft_final posi for fw

	// scheme 0
	u16 motion_th1  ; // 20000;	//  u16,    motion th1			in scheme0
	u16 motion_th2  ; // 40000;	//  u16,    motion th2			in scheme0
	u16 h_flat_th   ; // 960;		//  u16,    flat pixel num th in row	in scheme0
	u16 v_flat_th   ; // 540;		//  u16,    flat pixel num th in col	in scheme0

	u16  alpha_line  ; // 10;		 //  u8,     iir alpha			 in scheme0
	// IIR val in scheme 1
	u8 filt_delt; // 16;	     //  u8,	  filt param

	u16 top_iir; // 0;	     // u16,	 top iir val
	u16 bot_iir; // 1079;	     // u16,	 bot iir val
	u16 lft_iir; // 0;	 // u16,     lft iir val
	u16 rit_iir; // 1919;	 // u16,     rit iir val

	// Fw make decision
	u32 top_bot_motion_cnt_th; // 10000000;
	u32 lft_rit_motion_cnt_th; // 10000000;

	u8 top_bot_pre_motion_cur_motion_choose_edge_mode; // 0; // 0:hard 1:soft
	u8 lft_rit_pre_motion_cur_motion_choose_edge_mode; // 0; // 0:hard 1:soft

	u16 top_bot_motion_edge_line_num; // 512;	 // dft:512
	u16 lft_rit_motion_edge_line_num; // 512;	 // dft:512

	u8 top_bot_motion_edge_delta; // 4;	// dft:4
	u8 lft_rit_motion_edge_delta; // 4;	// dft:4

	u8 top_bot_lbox_edge_delta; // 2;		// dft:2
	u8 lft_rit_lbox_edge_delta; // 2;		// dft:2

	u8 top_bot_caption_motion_delta; // 16;	// dft:16
	u8 lft_rit_caption_motion_delta; // 16;	// dft:16

	u8 top_bot_border_edge_delta; // 2;	// dft:2
	u8 lft_rit_border_edge_delta; // 2;	// dft:2

	u8 top_bot_lbox_delta; // 4;		// dft:4
	u8 lft_rit_lbox_delta; // 4;		// dft:4

	u8 top_bot_lbox_motion_delta; // 2;	// dft:2
	u8 lft_rit_lbox_motion_delta; // 2;	// dft:2

	// bbd confidence
	u8 top_confidence; // 0;	     // u8, final top line confidence
	u8 bot_confidence; // 0;	 // u8, final bot line confidence
	u8 lft_confidence; // 0;	 // u8, final lft line confidence
	u8 rit_confidence; // 0;	 // u8, final rit line confidence

	u8 top_confidence_max; // 24;   // u8, final top line confidence max th
	u8 bot_confidence_max; // 24;   // u8, final top line confidence max th
	u8 lft_confidence_max; // 24;   // u8, final top line confidence max th
	u8 rit_confidence_max; // 24;   // u8, final top line confidence max th

	// bbd iir
	u8 final_iir_mode; // 0;	     // 0: not do iir, 1: num sel iir
	u8 final_iir_num_th1; // 4;
	u8 final_iir_num_th2; // 2;
	u8 final_iir_sel_rst_num; // 9;
	u8 final_top_bot_pre_cur_posi_delta; // 4;
	u8 final_lft_rit_pre_cur_posi_delta; // 4;
	u8 final_iir_check_valid_mode; // 1;
	u8 final_iir_alpha; // 128;

	// mc_force_blackbar_posi
	u8 force_final_posi_en; // 0;	   //  u1, force blackbar posi en
	u16 force_final_posi[4]; // 0;	    //	u12,force lft posi
	u8 force_final_each_posi_en[4]; // 1;    //  u1, force lft en

	u8 valid_ratio; // 2;			// u3, ratio for bb valid
};

//---------------------------------------------------------bbd end

//---------------------------------------------------------vp end
struct st_vp_ctrl_para {
	u8  vp_en;
	u8  vp_ctrl_en;
	u32 global_oct_cnt_th;
	u8  global_oct_cnt_en;
	u8  region_oct_cnt_en;
	u32 region_oct_cnt_th;
	u32 global_dtl_cnt_th;
	u8  global_dtl_cnt_en;
	u32 mv_activity_th;
	u8  mv_activity_en;
	u32 global_t_consis_th;
	u8  global_t_consis_en;
	u32 region_t_consis_th;
	u8  region_t_consis_en;
	u32 occl_mv_diff_th;
	u8  occl_mv_diff_en;
	u32 occl_exist_most_th;
	u8  occl_exist_most_en;
	u32 occl_exist_region_th;
};

//---------------------------------------------------------vp end

//----------------------------------------------------------logo start

struct st_iplogo_ctrl_item {
	u16     xsize_logo;
	u16     ysize_logo;
	u8      blk_dir4_clr_blk_rate;
	u8      doc_index;
};

struct st_iplogo_ctrl_para {
	u16 xsize;
	u16 ysize;                            // mc image size
	u8 gmv_invalid_check_en;             // u1, dft=0,0:not use me_gmv_invalid check
	u8 gmv_ctrl_corr_clr_en;             // u1, enable gmv ctrl corr clr method
	// u11,threshold of gmv for gmv ctrl corr clr method
	u16 gmv_ctrl_corr_clr_th;
	// u11,threshold of gmv for gmv_plus_minus ctrl corr msize
	u16 gmv_ctrl_corr_clr_msize_coring;
	// u1, enable gmv_plus_minus ctrl corr clr msize
	u8 gmv_ctrl_corr_clr_msize_en;
	u8 fw_iplogo_en;                     // u1

	// func: area and gmv rate
	// dft=0,for fw logo reset, threshold of small mv rate
	u16 scc_glb_clr_rate_th;
	// u1,dft=0,  0: area corr dir4 clr blk rate disable  (logo weak setting)
	u8 area_ctrl_dir4_ratio_en;
	u8 area_ctrl_corr_th_en;             // u1,dft=1,  0: area ctrl corr thres disable
	u8 area_th_ub;                       // u8,dft=30, 40(logo weak setting)
	u8 area_th_mb;                       // u8,dft=30, 20(logo weak setting)
	u8 area_th_lb;                       // u8,dft=10, 10(logo weak setting)
};

struct st_melogo_ctrl_para {
	u8 fw_melogo_en;
};

//----------------------------------------------------------logo end

//----------------------------------------------------------file detect start

#define FLMMODNUM	18
#define HISDIFNUM	30
#define MAX_CYCLE	25
#define PHS_FRAC_BIT	7
#define WINNUM		12

struct st_film_detect_item
{
	u8      flmDetLog;                   //flim mod
	u8      cadencCnt;                   //flim mod candence count
	u8      mode_enter_th;               //enter mode  threshold
	u32     phase_match_data;            //phase check
	u32     phase_mask_value;
	u8      quit_mode_th;                 //quit mode threshold
	u8      quit_reset_th;                //reset threshold
	//global average ratio average = average * ratio/16
	u8      glb_ratio;
	//global diff threshold ofset th = average + glb_ofset
	u16     glb_ofset;
	//each window average ratio, each widow average = average * ratio/16
	u8      wind_ratio;
	//window diff threshold ofset threshold = average + wind_ofset
	u16     wind_ofset;
	//min diff threshold, if motion<min_diff_th  "0"
	u32     min_diff_th;
	u8      phase_error_flag;             //Possible phase error
	u8      filmMod;                      //film mode type
	u8      phase;                        //film mode phase

	u8      force_phase;                   //force mode phase
	u8      force_mode;                    //force mode
	u8      force_mode_en;                 //force mode en


	//mix mode
	u8      mm_cown_thd;                   //
	u8      mm_cpre_thd;
	u8      mm_cother_thd;
	u8      mm_reset_thd;
	u8      mm_difminthd;
	u8      mm_chk_mmdifthd;

	//badedit
	u8      step;                        //frc phase detla
	u8      input_n;
	u8      output_m;
	u8      mode_shift;
	u8      inout_table_point;
	u8      pre_table_point;
	u8      cur_table_point;
	u8      nex_table_point;
	u8      pre_idx;
	u8      cur_idx;
	u8      nex_idx;
	u8      inout_idx;

	u8      input_id_point;
	u8      input_id_length;
	u8      input_fid_fw;

	u8      cadence_chang_mode;
	u8      replace_id_point;
	u8      jump_simple_mode;
	u8      mode_change_adjust_en;
	u8      otb_start;

	u8      frame_buf_num;
};

struct st_film_table_item
{
	u16     film_mode;    //flim mod
	u8      cadence_cnt;
	u8      enter_th;
	u32     match_data;
	u8      cycle_num;
	u8      average_num;
	u8      quit_th;
	u32     mask_value;

	//bad edit
	u32/*u8*/      step_ofset;/*Qy modify: u8 to u32*/
	u8      delay_num;
	u8      max_zeros_cnt;
	u8      jump_id_flag;
	u8      simple_jump_add_buf;

	//mode change
	u8      mode_enter_level;
};

struct st_film_data_item
{
	u8      ModCount[FLMMODNUM];                  //film mode count
	u8      quitCount[2];                          // quitcunt num
	u8      quit_reset_Cnt[2];                    //
	u8      phase[FLMMODNUM];                     // cadence phase
	u8      phase_new[FLMMODNUM];                 // another phase

	//mix mode
	//mix mode num compare with own window
	u8      mm_num_cown[6];
	//mix mode num compare with other window
	u8      mm_num_cother[6];
	//mix mode num compare with history (diff gradually increase,may enter mix mode)
	u8      mm_num_cpre[6];
	u8      mm_num_reset[6];
	//mix mode num compare with history(diff gradually reduced,may quit mix mode)
	u8      mm_num_cpre_quit[6];

	//bad edit
	u8      A1A2_change_flag[65];
	u8      frame_buffer_flag[16];
	u8      frame_buffer_id[16];

	//long candence simple mode
	u8      input_id_table[27];       //16+11
	u8      inout_id_table[16][8];    //after calculation input id lut
	u8      input_cover_flag_table[16];
	u8      input_cover_point_table[16];
	u8      input_cover_idx_table[16];
};

//----------------------------------------------------------file detect end

//----------------------------------------------------------me start
#define ME_MV_FRAC_BIT		2
#define NUM_OF_LOOP 		3

struct st_scene_change_detect_para {
	u8  schg_det_en0;
	u8  schg_strict_mod0;   //0:and, 1:or
	u32 schg_glb_sad_ph_thd0;
	u32 schg_glb_sad_cn_thd0;
	u32 schg_glb_sad_nc_thd0;
	u32 schg_glb_sad_dif_thd0;
	u32 schg_glb_sad_ph_cnt_rt0;
	u32 schg_glb_sad_cn_cnt_rt0;
	u32 schg_glb_sad_nc_cnt_rt0;
	u32 schg_glb_apl_pc_thd0;
	u32 schg_glb_apl_cn_thd0;
	u32 schg_glb_t_consis_ph_thd0;
	u32 schg_glb_t_consis_ph_rt0;
	u32 schg_glb_t_consis_cn_thd0;
	u32 schg_glb_t_consis_cn_rt0;
	u32 schg_glb_t_consis_nc_thd0;
	u32 schg_glb_t_consis_nc_rt0;
	u32 schg_glb_unstable_rt0;

	u8  schg_det_en1;
	u32 schg_strict_mod1;
	u32 schg_glb_sad_ph_thd1;
	u32 schg_glb_sad_cn_thd1;
	u32 schg_glb_sad_nc_thd1;
	u32 schg_glb_sad_dif_thd1;
	u32 schg_glb_sad_ph_cnt_rt1;
	u32 schg_glb_sad_cn_cnt_rt1;
	u32 schg_glb_sad_nc_cnt_rt1;
	u32 schg_glb_apl_pc_thd1;
	u32 schg_glb_apl_cn_thd1;
	//below (related to mv diff) maybe useless if scene change static 0 happened and vbuf cleared
	u32 schg_glb_t_consis_ph_thd1;
	u32 schg_glb_t_consis_ph_rt1;
	u32 schg_glb_t_consis_cn_thd1;
	u32 schg_glb_t_consis_cn_rt1;
	u32 schg_glb_t_consis_nc_thd1;
	u32 schg_glb_t_consis_nc_rt1;
	u32 schg_glb_unstable_rt1;
};

struct st_fb_ctrl_item {
	u32 glb_TC_20[20];
	u32 glb_TC_iir_pre;
	u32 TC_th_s_pre;
	u32 TC_th_l_pre;
	u8  mc_fallback_level_pre;
};

struct st_fb_ctrl_para {
	// global fallback
	u8  glb_tc_iir_up;
	u8  glb_tc_iir_dn;
	u8  fb_level_iir_up;
	u8  fb_level_iir_dn;
	u8  fb_gain_gmv_th_l;
	u8  fb_gain_gmv_th_s;
	u8  fb_gain_gmv_ratio_l;
	u8  fb_gain_gmv_ratio_s;
	u32 base_TC_th_s;
	u32 base_TC_th_l;
	u8  TC_th_iir_up;
	u8  TC_th_iir_dn;
	u8  fallback_level_max;

	// region fallback
	u8  region_TC_iir_up;	//0x30
	u8  region_TC_iir_dn;	//0xf0
	u8  region_TC_th_iir_up;	//0xE0
	u8  region_TC_th_iir_dn;	//0x40
	u8  region_fb_level_iir_up;	//0x60
	u8  region_fb_level_iir_dn;	//0xB0
	u32 region_TC_bad_th; //4096
	u8  region_fb_level_b_th;
	u8  region_fb_level_s_th;
	u8  region_fb_level_ero_cnt_b_th;
	u8  region_fb_level_dil_cnt_s_th;
	u8  region_fb_level_dil_cnt_b_th;
	u8  region_fb_gain_gmv_th_s;
	u8  region_fb_gain_gmv_th_l;
	u8  region_fb_gain_gmv_ratio_s;
	u8  region_fb_gain_gmv_ratio_l;
	u32 region_dtl_sum_th;
	u8  region_fb_ext_th;
	u8  region_fb_ext_coef;    //u3,coef<<3,coef=4 equal to 0.5
	u32 base_region_TC_th_s;
	u32 base_region_TC_th_l;
};

struct st_region_fb_ctrl_item
{
	u32 region_TC_iir_pre[6][8];
	u32 region_TC_th_s_pre[6][8];
	u32 region_TC_th_l_pre[6][8];
	u8  region_fb_level_pre[6][8];
};

struct st_me_ctrl_para {
	u8  me_en;
	u8  me_rule_ctrl_en;
	s32 update_strength_add_value;
	u8  scene_change_flag;
	u32 fallback_gmvx_th;
	u32 fallback_gmvy_th;

	u8  region_sad_median_num;
	u32 region_sad_sum_th;
	u32 region_sad_cnt_th;
	u32 region_s_consis_th;
	u8  region_win3x3_min;
	u8  region_win3x3_max;
};

struct st_me_ctrl_item {
	u32 static_scene_frame_count;
	u32 scene_change_catchin_frame_count;
	u32 scene_change_frame_count;
	u32 scene_change_judder_frame_count;
	u32 mixmodein_frame_count;
	u32 mixmodeout_frame_count;
	u32 scene_change_reject_frame_count;

	u32 region_sad_sum_20[48][20];
	u32 region_sad_cnt_20[48][20];
	u32 region_s_consis_20[48][20];
};

//----------------------------------------------------------me end

//----------------------------------------------------------mc define start
struct st_search_range_dynamic_para {
	u16 srch_rng_mvy_th;
	u8  force_luma_srch_rng_en;
	u8  force_chrm_srch_rng_en;
	u8  srch_rng_mode_cnt;
	u8  srch_rng_mode_h_th;
	u8  srch_rng_mode_l_th;

	u8 norm_mode_en;//         =   1;          //  u1
	u8 sing_up_en;//            =   1;          //  u1
	u8 sing_dn_en;//            =   1;          //  u1
	u8 norm_asym_en;//          =   0;          //  u1
	u8 norm_vd2_en;//           =   1;          //  u1
	u8 sing_up_vd2_en;//        =   1;          //  u1
	u8 sing_dn_vd2_en;//        =   1;          //  u1
	u8 norm_vd2hd2_en;//        =   1;          //  u1
	u8 sing_up_vd2hd2_en;//     =   1;          //  u1
	u8 sing_dn_vd2hd2_en;//     =   1;          //  u1
	u8 gred_mode_en;//          =   1;          //  u1

	u8 luma_norm_vect_th;//      =   40;        //  u8
	u8 luma_sing_vect_min_th;//  =   24;        //  u8
	u8 luma_sing_vect_max_th;//  =   56;        //  u8
	u8 luma_asym_vect_th;//      =   48;        //  u8
	u8 luma_gred_vect_th;//      =   80;        //  u8

	u8 chrm_norm_vect_th;//      =   24;        //  u8
	u8 chrm_sing_vect_min_th;//  =   8;         //  u8
	u8 chrm_sing_vect_max_th;//  =   48;        //  u8
	u8 chrm_asym_vect_th;//      =   32;        //  u8
	u8 chrm_gred_vect_th;//      =   48;        //  u8
};

struct st_pixel_lpf_para {
	u8  osd_ctrl_pixlpf_en;// = 0;
	u16 osd_ctrl_pixlpf_th;// = 0;
	u8  detail_ctrl_pixlpf_en;// = 0;
	u16 detail_ctrl_pixlpf_th;// = 0;
};

//----------------------------------------------------------- mc define end

//-----------------------------------------------------------frc top cfg
enum frc_ratio_mode_type {
	FRC_RATIO_1_2 = 0,
	FRC_RATIO_2_3,
	FRC_RATIO_2_5,
	FRC_RATIO_5_6,
	FRC_RATIO_5_12,
	FRC_RATIO_2_9,
	FRC_RATIO_1_1
};

enum en_film_mode {
	EN_VIDEO=0,
	EN_FILM22,
	EN_FILM32,
	EN_FILM3223,
	EN_FILM2224,
	EN_FILM32322,
	EN_FILM44,
	EN_FILM21111,
	EN_FILM23322,
	EN_FILM2111,
	EN_FILM22224,
	EN_FILM33,
	EN_FILM334,
	EN_FILM55,
	EN_FILM64,
	EN_FILM66,
	EN_FILM87,
	EN_FILM212
};

struct frc_holdline_s {
	u32 me_hold_line;//me_hold_line
	u32 mc_hold_line;//mc_hold_line
	u32 inp_hold_line;
	u32 reg_post_dly_vofst;//fixed
	u32 reg_mc_dly_vofst0;//fixed
};

struct frc_top_type_s {
	/*input*/
	u32       hsize;
	u32       vsize;
	u32       vfp;//line num before vsync,VIDEO_VSO_BLINE
	u32       vfb;//line num before de   ,VIDEO_VAVON_BLINE
	u32       frc_fb_num;//buffer num for frc loop
	enum frc_ratio_mode_type frc_ratio_mode;//frc_ratio_mode = frame rate of input : frame rate of output
	enum en_film_mode    film_mode;//film_mode
	u32       film_hwfw_sel;//0:hw 1:fw
	u32       is_me1mc4;//1: me:mc=1/4, 0 : me:mc=1/2, default 0
	u32       loss_en;//default 0
	u32       frc_prot_mode;//0:memc prefetch acorrding mode frame 1:memc prefetch 1 frame
	/*output*/
	u32 out_hsize;
	u32 out_vsize;
};

//-----------------------------------------------------------frc top cfg end

struct frc_fw_data_s {
	/*scene*/
	struct st_frc_rd g_stfrc_rd;
	struct st_logo_detect_rd g_stlogodetect_rd;
	struct st_film_detect_rd g_stfilmdetect_rd;
	struct st_bbd_rd g_stbbd_rd;
	struct st_me_rd g_stme_rd;
	struct st_vp_rd g_stvp_rd;
	struct st_mc_rd g_stmc_rd;

	/*bbd*/
	struct st_search_final_line_para search_final_line_para;

	struct st_vp_ctrl_para g_stvpctrl_para;

	/*logo*/
	struct st_iplogo_ctrl_item g_stiplogoctrl_item;// fw iplogo param
	struct st_iplogo_ctrl_para g_stiplogoctrl_para;// fw iplogo param (adjustable)
	struct st_melogo_ctrl_para g_stmelogoctrl_para;// fw melogo param (adjustable)

	/*film detect*/
	struct st_film_data_item g_stfilmdata_item;
	struct st_film_detect_item g_stfilm_detect_item;

	/*me*/
	struct st_scene_change_detect_para g_stScnChgDet_Para;
	struct st_fb_ctrl_para g_stFbCtrl_Para;
	struct st_me_ctrl_para g_stMeCtrl_Para;
	struct st_fb_ctrl_item g_stFbCtrl_Item;
	struct st_region_fb_ctrl_item g_stRegionFbCtrl_Item;
	struct st_me_ctrl_item g_stMeCtrl_Item;

	/*mc*/
	struct st_search_range_dynamic_para g_stsrch_rng_dym_para;
	struct st_pixel_lpf_para g_stpixlpf_para;

	/*frc top type config*/
	struct frc_top_type_s frc_top_type;
	struct frc_holdline_s holdline_parm;
};

enum frc_state_e {
	FRC_STATE_DISABLE = 0,
	FRC_STATE_ENABLE,
	FRC_STATE_BYPASS,
	FRC_STATE_NULL,
};

struct tool_debug_s {
	u32 reg_read;
	u32 reg_read_val;
};

struct dbg_dump_tab {
	u8 *name;
	u32 addr;
};

#endif
