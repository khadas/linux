/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _DV_H_
#define _DV_H_


#include <linux/types.h>
#include <linux/amlogic/media/vout/vinfo.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/amvecm/amvecm.h>

#define AMDV_OUTPUT_MODE_IPT			0
#define AMDV_OUTPUT_MODE_IPT_TUNNEL		1
#define AMDV_OUTPUT_MODE_HDR10			2
#define AMDV_OUTPUT_MODE_SDR10			3
#define AMDV_OUTPUT_MODE_SDR8			4
#define AMDV_OUTPUT_MODE_BYPASS			5

/* STB: if sink support DV, always output DV*/
/*		else always output SDR/HDR */
/* TV:  when source is DV, convert to SDR */
#define AMDV_FOLLOW_SINK		0

/* STB: output DV only if source is DV*/
/*		and sink support DV*/
/*		else always output SDR/HDR */
/* TV:  when source is DV or HDR, convert to SDR */
#define AMDV_FOLLOW_SOURCE		1

/* STB: always follow dolby_vision_mode */
/* TV:  if set dolby_vision_mode to SDR8,*/
/*		convert all format to SDR by TV core,*/
/*		else bypass Dolby Vision */
#define AMDV_FORCE_OUTPUT_MODE	2

#define MUTE_TYPE_NONE	0
#define MUTE_TYPE_YUV	1
#define MUTE_TYPE_RGB	2
#define MUTE_TYPE_IPT	3
struct vframe_s;
#define MD_BUF_SIZE 1024
#define COMP_BUF_SIZE 8196

#define DV_SEI 0x01000000
#define AV1_SEI 0x14000000 /*for both dv and hdr10plus*/
#define HDR10P 0x02000000

enum pq_item_e {
	PQ_BRIGHTNESS = 0,     /*Brightness */
	PQ_CONTRAST = 1,       /*Contrast */
	PQ_COLORSHIFT = 2,     /*ColorShift or Tint*/
	PQ_SATURATION = 3      /*Saturation or color */
};

enum pq_reset_e {
	RESET_ALL = 0,         /*reset picture mode / pq for all picture mode*/
	RESET_PQ_FOR_ALL = 1,  /*reset pq for all picture mode*/
	RESET_PQ_FOR_CUR = 2   /*reset pq for current picture mode */
};

struct pic_mode_info_s {
	int pic_mode_id;
	unsigned char name[32];
} __aligned(8);

struct dv_pq_info_s {
	int pic_mode_id;
	enum pq_item_e item;
	s16 value;
} __aligned(8);

struct dv_full_pq_info_s {
	int pic_mode_id;
	s16  brightness;  /*Brightness */
	s16  contrast;    /*Contrast */
	s16  colorshift;  /*ColorShift or Tint*/
	s16  saturation;  /*Saturation or color */
} __aligned(8);

struct dv_config_file_s {
	unsigned char bin_name[256];
	unsigned char cfg_name[256];
} __aligned(8);

struct ambient_cfg_s {
	u32 update_flag;
	u32 ambient; /* 1<<16 */
	u32 t_rearLum;
	u32 t_frontLux;
	u32 t_whiteX; /* 1<<15 */
	u32 t_whiteY; /* 1<<15 */
	u32 dark_detail;
} __aligned(8);

#define DV_M 'D'

/* get Number of Picture Mode */
#define DV_IOC_GET_DV_PIC_MODE_NUM _IOR((DV_M), 0x0, int)

/* get Picture Mode Name of input pic_mode_id */
#define DV_IOC_GET_DV_PIC_MODE_NAME _IOWR((DV_M), 0x1, struct pic_mode_info_s)

/* get current active picture mode */
#define DV_IOC_GET_DV_PIC_MODE_ID _IOR((DV_M), 0x2, int)

/* select active picture mode */
#define DV_IOC_SET_DV_PIC_MODE_ID _IOW((DV_M), 0x3, int)

/* get single pq(contrast or brightness or colorshift or saturation) */
#define DV_IOC_GET_DV_SINGLE_PQ_VALUE _IOWR((DV_M), 0x4, struct dv_pq_info_s)

/* get all pq(contrast, brightness,colorshift ,saturation) */
#define DV_IOC_GET_DV_FULL_PQ_VALUE _IOWR((DV_M), 0x5, struct dv_full_pq_info_s)

/* set single pq(contrast or brightness or colorshift or saturation) */
#define DV_IOC_SET_DV_SINGLE_PQ_VALUE _IOWR((DV_M), 0x6, struct dv_pq_info_s)

/* set all pq(contrast,brightness ,colorshift , saturation) */
#define DV_IOC_SET_DV_FULL_PQ_VALUE _IOWR((DV_M), 0x7, struct dv_full_pq_info_s)

/* reset all pq item  for current picture mode */
#define DV_IOC_SET_DV_PQ_RESET _IOWR((DV_M), 0x8, enum pq_reset_e)

/* set Amlogic_cfg.txt and dv_config.bin dir */
#define DV_IOC_SET_DV_CONFIG_FILE _IOW((DV_M), 0x9, struct dv_config_file_s)

/* set ambient light */
#define DV_IOC_SET_DV_AMBIENT _IOW((DV_M), 0xa, struct ambient_cfg_s)

/*1: disable dv GD, 0: restore dv GD*/
#define DV_IOC_CONFIG_DV_BL _IOW((DV_M), 0xb, int)

/*1: enable dv dark detail, 0: disable dv GD*/
#define DV_IOC_SET_DV_DARK_DETAIL _IOW((DV_M), 0xc, int)

extern unsigned int debug_dolby;

struct composer_reg_ipcore {
	/* offset 0xc8 */
	u32 composer_mode;
	u32 vdr_resolution;
	u32 bit_depth;
	u32 coefficient_log2_denominator;
	u32 bl_num_pivots_y;
	u32 bl_pivot[5];
	u32 bl_order;
	u32 bl_coefficient_y[8][3];
	u32 el_nlq_offset_y;
	u32 el_coefficient_y[3];
	u32 mapping_idc_u;
	u32 bl_num_pivots_u;
	u32 bl_pivot_u[3];
	u32 bl_order_u;
	u32 bl_coefficient_u[4][3];
	u32 mmr_coefficient_u[22][2];
	u32 mmr_order_u;
	u32 el_nlq_offset_u;
	u32 el_coefficient_u[3];
	u32 mapping_idc_v;
	u32 bl_num_pivots_v;
	u32 bl_pivot_v[3];
	u32 bl_order_v;
	u32 bl_coefficient_v[4][3];
	u32 mmr_coefficient_v[22][2];
	u32 mmr_order_v;
	u32 el_nlq_off_v;
	u32 el_coefficient_v[3];
};

struct dm_reg_ipcore1 {
	u32 s_range;
	u32 s_range_inverse;
	u32 frame_fmt1;
	u32 frame_fmt2;
	u32 frame_pixel_def;
	u32 y2rgb_coeff1;
	u32 y2rgb_coeff2;
	u32 y2rgb_coeff3;
	u32 y2rgb_coeff4;
	u32 y2rgb_coeff5;
	u32 y2rgb_off1;
	u32 y2rgb_off2;
	u32 y2rgb_off3;
	u32 eotf;
	u32 a2b_coeff1;
	u32 a2b_coeff2;
	u32 a2b_coeff3;
	u32 a2b_coeff4;
	u32 a2b_coeff5;
	u32 c2d_coeff1;
	u32 c2d_coeff2;
	u32 c2d_coeff3;
	u32 c2d_coeff4;
	u32 c2d_coeff5;
	u32 c2d_off;
	u32 active_left_top;
	u32 active_bottom_right;
};

/*dm luts for core1 and core2 */
struct dm_lut_ipcore {
	u32 tm_lut_i[64 * 4];
	u32 tm_lut_s[64 * 4];
	u32 sm_lut_i[64 * 4];
	u32 sm_lut_s[64 * 4];
	u32 g_2_l[256];
};

enum OSD_INDEX {
	OSD1_INDEX = 0,
	OSD2_INDEX = 1,
	OSD3_INDEX = 2,
	OSD4_INDEX = 3,
	OSD_MAX_INDEX = 4
};

void enable_amdv(int enable);
bool is_amdv_enable(void);
bool is_amdv_on(void);
bool is_amdv_video_on(void);
bool is_amdv_graphic_on(void);
bool for_amdv_certification(void);
void set_amdv_mode(int mode);
int get_amdv_mode(void);
int get_amdv_target_mode(void);
void amdv_set_toggle_flag(int flag);
int amdv_wait_metadata(struct vframe_s *vf, enum vd_path_e vd_path);
int amdv_pop_metadata(void);
int amdv_update_metadata(struct vframe_s *vf, enum vd_path_e vd_path, bool drop_flag);
int amdolby_vision_process
	(struct vframe_s *vf, u32 display_size,
	 struct vframe_s *vf_2, u32 display_size_2,
	 u8 toggle_mode_1, u8 toggle_mode_2, u8 pps_state);
void amdv_init_receiver(void *pdev);
void amdv_vf_put(struct vframe_s *vf);
struct vframe_s *amdv_vf_peek_el(struct vframe_s *vf);
void amdv_dump_setting(int debug_flag);
void amdv_dump_struct(void);
void enable_osd_path(int on, int shadow_mode);
void tv_amdv_config(int config);
void amdv_update_pq_config
	(char *pq_config_buf);
int amdv_update_setting(struct vframe_s *vf);
bool is_amdv_stb_mode(void);
bool is_aml_tvmode(void);
void amdv_crc_clear(int flag);
char *amdv_get_crc(u32 *len);
void amdv_insert_crc(bool print);
int amdv_check_hdr10(struct vframe_s *vf);
int amdv_check_hlg(struct vframe_s *vf);
int amdv_check_hdr10plus(struct vframe_s *vf);
int amdv_check_primesl(struct vframe_s *vf);
int amdv_check_cuva(struct vframe_s *vf);
void tv_amdv_dma_table_modify
	(u32 tbl_id, uint64_t value);
void tv_amdv_efuse_info(void);
int amdv_parse_metadata
	(struct vframe_s *vf, enum vd_path_e vd_path, u8 toggle_mode,
	 bool bypass_release, bool drop_flag);
void amdv_update_vsvdb_config
	(char *vsvdb_buf, u32 tbl_size);
void tv_amdv_el_info(void);

int enable_rgb_to_yuv_matrix_for_dvll
	(s32 on, uint32_t *coeff_orig, uint32_t bits);

int is_amdv_frame(struct vframe_s *vf);
void update_graphic_width_height(unsigned int width, unsigned int height, enum OSD_INDEX index);
int get_amdv_policy(void);
void set_amdv_policy(int policy);
int get_amdv_src_format(enum vd_path_e vd_path);
bool is_amdv_el_disable(void);
bool is_dovi_dual_layer_frame(struct vframe_s *vf);
void amdv_set_provider(char *prov_name, enum vd_path_e vd_layer);
int amdv_check_mvc(struct vframe_s *vf);
bool for_amdv_video_effect(void);
int get_amdv_hdr_policy(void);
int get_dv_support_info(void);
void dv_vf_light_reg_provider(void);
void dv_vf_light_unreg_provider(void);
void amdv_update_backlight(void);
int amdv_update_src_format(struct vframe_s *vf, u8 toggle_mode, enum vd_path_e vd_path);
void update_graphic_status(void);
int parse_sei_and_meta_ext
	(struct vframe_s *vf,
	 char *aux_buf,
	 int aux_size,
	 int *total_comp_size,
	 int *total_md_size,
	 void *fmt,
	 int *ret_flags,
	 char *md_buf,
	 char *comp_buf);
bool is_dv_control_backlight(void);
bool get_force_bypass_from_prebld_to_vadj1(void);
void update_dvcore2_timing(u32 *hsize, u32 *vsize);
int dv_inst_map(int *inst);
void dv_inst_unmap(int inst);
bool is_hdmi_ll_as_hdr10(void);
bool is_multi_dv_mode(void);
bool support_multi_core1(void);

#define AMDV_UPDATE_OSD_MODE 0x00000001
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
int amdv_notifier_call_chain(unsigned long val, void *v);
int register_osd_func(int (*get_osd_enable_status)(u32 index));
#else
static inline int amdv_notifier_call_chain(unsigned long val, void *v)
{
	return 0;
}

static inline int register_osd_func(int (*get_osd_enable_status)(u32 index))
{
	return 0;
}
#endif
#endif
