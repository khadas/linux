/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef _AMDV_PQ_H_
#define _AMDV_PQ_H_

#include <linux/types.h>

#define BACKLIGHT_LUT_SIZE  4096
#define AMBIENT_LUT_SIZE    8
#define TUNING_LUT_SIZE     14
#define DM4_TUNING_LUT_SIZE 7

#define MAX_DV_PICTUREMODES 40
#define AMBIENT_CFG_FRAMES 46

# pragma pack(push, 1)
struct tgt_out_csc_cfg {
	s32  lms2rgb_mat[3][3];
	s32  lms2rgb_mat_scale;
	u8   white_point[3];
	u8   white_point_scale;
	s32  reserved[3];
};

#pragma pack(pop)

# pragma pack(push, 1)
struct tgt_gc_cfg {
	s32   gd_enable;
	u32  gd_wmin;
	u32  gd_wmax;
	u32  gd_wmm;
	u32  gd_wdyn_rng_sqrt;
	u32  gd_weight_mean;
	u32  gd_weight_std;
	u32  gd_delay_msec_hdmi;
	s32  gd_rgb2yuv_ext;
	s32  gd_m33_rgb2yuv[3][3];
	s32  gd_m33_rgb2yuv_scale2p;
	s32  gd_rgb2yuv_off_ext;
	s32  gd_rgb2yuv_off[3];
	u32  gd_up_bound;
	u32  gd_low_bound;
	u32  last_max_pq;
	u16  gd_wmin_pq;
	u16  gd_wmax_pq;
	u16  gd_wm_pq;
	u16  gd_trigger_period;
	u32  gd_trigger_lin_thresh;
	u32  gd_delay_msec_ott;
	s16  gd_rise_weight;
	s16  gd_fall_weight;
	u32  gd_delay_msec_ll;
	u32  gd_contrast;
	u32  reserved[3];
};

#pragma pack(pop)

# pragma pack(push, 1)
struct ambient_cfg {
	u8  reserved[3];
	u8  dark_detail;
	u32 dark_detail_complum;
	u32 ambient;
	u32 t_front_lux;
	u32 t_front_lux_scale;
	u32 t_rear_lum;
	u32 t_rear_lum_scale;
	u32 t_whitexy[2];
	u32 t_surround_reflection;
	u32 t_screen_reflection;
	u32 al_delay;
	u32 al_rise;
	u32 al_fall;
};

#pragma pack(pop)

# pragma pack(push, 1)
struct tgt_ab_cfg {
	s32  ab_enable;
	u32  ab_highest_tmax;
	u32  ab_lowest_tmax;
	s16  ab_rise_weight;
	s16  ab_fall_weight;
	u32  ab_delay_msec_hdmi;
	u32  ab_delay_msec_ott;
	u32  ab_delay_msec_ll;
	u32  reserved[1];
};

#pragma pack(pop)

# pragma pack(push, 1)
struct target_config {
	u16 gamma;
	u16 eotf;
	u16 range_spec;
	u16 max_pq;
	u16 min_pq;
	u16 max_pq_dm3;
	u32 min_lin;
	u32 max_lin;
	u32 max_lin_dm3;
	s32 t_primaries[8];
	u16 m_sweight;
	s16 trim_slope_bias;
	s16 trim_offset_bias;
	s16 trim_power_bias;
	s16 ms_weight_bias;
	s16 chroma_weight_bias;
	s16 saturation_gain_bias;
	u16 tuning_mode;
	s16 d_brightness;
	s16 d_contrast;
	s16 d_color_shift;
	s16 d_saturation;
	s16 d_backlight;
	s16 dbg_exec_params_print_period;
	s16 dbg_dm_md_print_period;
	s16 dbg_dm_cfg_print_period;
	struct tgt_gc_cfg gd_config;
	struct tgt_ab_cfg ab_config;
	struct ambient_cfg ambient_config;
	u8 vsvdb[7];
	u8 dm31_avail;
	u8 ref_mode_dark_id;
	u8 apply_l11_wp;
	u8 reserved1[1];
	s16 backlight_scaler;
	struct tgt_out_csc_cfg ocsc_config;
	s16 bright_preservation;
	u8 total_viewing_modes_num;
	u8 viewing_mode_valid;
	u32 ambient_frontlux[AMBIENT_LUT_SIZE];
	u32 ambient_complevel[AMBIENT_LUT_SIZE];
	s16 mid_pq_bias_lut[TUNING_LUT_SIZE];
	s16 slope_bias_lut[TUNING_LUT_SIZE];
	s16 backlight_bias_lut[TUNING_LUT_SIZE];
	s16 user_brightness_ui_lut[DM4_TUNING_LUT_SIZE];
	u16 padding2;
	s16 blu_pwm[5];
	s16 blu_light[5];
	s16 padding[36];
};

#pragma pack(pop)

struct pq_config {
	unsigned char backlight_lut[BACKLIGHT_LUT_SIZE];
	struct target_config tdc;
};

struct dv_cfg_info_s {
	int id;
	char pic_mode_name[32];
	s16  brightness;        /*Brightness */
	s16  contrast;          /*Contrast */
	s16  colorshift;        /*ColorShift or Tint*/
	s16  saturation;        /*Saturation or color */
	u8  vsvdb[7];
	int dark_detail;        /*dark detail, on or off*/
};

struct dv_pq_center_value_s {
	s16  brightness;        /*Brightness */
	s16  contrast;          /*Contrast */
	s16  colorshift;        /*ColorShift or Tint*/
	s16  saturation;        /*Saturation or color */
};

struct dv_pq_range_s {
	s16  left;
	s16  right;
};

extern struct pq_config *bin_to_cfg;
extern struct dv_cfg_info_s cfg_info[MAX_DV_PICTUREMODES];
extern const char *pq_item_str[];
extern struct target_config def_tgt_display_cfg_bestpq;
extern struct target_config def_tgt_display_cfg_ll;
extern int cur_pic_mode;/*current picture mode id*/
extern struct ambient_cfg_s ambient_test_cfg[AMBIENT_CFG_FRAMES];
extern struct ambient_cfg_s ambient_test_cfg_2[AMBIENT_CFG_FRAMES];
extern struct ambient_cfg_s ambient_test_cfg_3[AMBIENT_CFG_FRAMES];

void restore_dv_pq_setting(enum pq_reset_e pq_reset);
bool load_dv_pq_config_data(char *bin_path, char *txt_path);
void set_pic_mode(int mode);
int get_pic_mode_num(void);
int get_pic_mode(void);
char *get_cur_pic_mode_name(void);

char *get_pic_mode_name(int mode);
s16 get_single_pq_value(int mode, enum pq_item_e item);
struct dv_full_pq_info_s get_full_pq_value(int mode);
void set_single_pq_value(int mode, enum pq_item_e item, s16 value);
void set_full_pq_value(struct dv_full_pq_info_s full_pq_info);

void get_dv_bin_config(void);
int get_dv_pq_info(char *buf);
int set_dv_pq_info(const char *buf, size_t count);
int set_dv_debug_tprimary(const char *buf, size_t count);
int get_dv_debug_tprimary(char *buf);
bool get_load_config_status(void);
void set_load_config_status(bool flag);
int get_inter_pq_flag(void);
void set_inter_pq_flag(int flag);
void set_cfg_id(uint id);
void update_cp_cfg(void);

#endif
