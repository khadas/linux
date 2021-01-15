/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _DV_H_
#define _DV_H_

#define V1_5
#define V2_4

#include <linux/types.h>
#include <linux/amlogic/media/vout/vinfo.h>
#include <linux/amlogic/media/vfm/vframe.h>

#define DOLBY_VISION_OUTPUT_MODE_IPT			0
#define DOLBY_VISION_OUTPUT_MODE_IPT_TUNNEL		1
#define DOLBY_VISION_OUTPUT_MODE_HDR10			2
#define DOLBY_VISION_OUTPUT_MODE_SDR10			3
#define DOLBY_VISION_OUTPUT_MODE_SDR8			4
#define DOLBY_VISION_OUTPUT_MODE_BYPASS			5

/* STB: if sink support DV, always output DV*/
/*		else always output SDR/HDR */
/* TV:  when source is DV, convert to SDR */
#define DOLBY_VISION_FOLLOW_SINK		0

/* STB: output DV only if source is DV*/
/*		and sink support DV*/
/*		else always output SDR/HDR */
/* TV:  when source is DV or HDR, convert to SDR */
#define DOLBY_VISION_FOLLOW_SOURCE		1

/* STB: always follow dolby_vision_mode */
/* TV:  if set dolby_vision_mode to SDR8,*/
/*		convert all format to SDR by TV core,*/
/*		else bypass Dolby Vision */
#define DOLBY_VISION_FORCE_OUTPUT_MODE	2

#define MUTE_TYPE_NONE	0
#define MUTE_TYPE_YUV	1
#define MUTE_TYPE_RGB	2
#define MUTE_TYPE_IPT	3
struct vframe_s;
#define MD_BUF_SIZE 1024
#define COMP_BUF_SIZE 8196

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

#define DV_SEI 0x01000000
#define DV_AV1_SEI 0x14000000
#define HDR10P 0x02000000

void enable_dolby_vision(int enable);
bool is_dolby_vision_enable(void);
bool is_dolby_vision_on(void);
bool is_dolby_vision_video_on(void);
bool for_dolby_vision_certification(void);
void set_dolby_vision_mode(int mode);
int get_dolby_vision_mode(void);
int get_dolby_vision_target_mode(void);
void dolby_vision_set_toggle_flag(int flag);
int dolby_vision_wait_metadata(struct vframe_s *vf);
int dolby_vision_pop_metadata(void);
int dolby_vision_update_metadata(struct vframe_s *vf, bool drop_flag);
int dolby_vision_process
	(struct vframe_s *vf, u32 display_size,
	u8 toggle_mode, u8 pps_state);
void dolby_vision_init_receiver(void *pdev);
void dolby_vision_vf_put(struct vframe_s *vf);
struct vframe_s *dolby_vision_vf_peek_el(struct vframe_s *vf);
void dolby_vision_dump_setting(int debug_flag);
void dolby_vision_dump_struct(void);
void enable_osd_path(int on, int shadow_mode);
void tv_dolby_vision_config(int config);
void dolby_vision_update_pq_config(char *pq_config_buf);
int dolby_vision_update_setting(void);
bool is_dolby_vision_stb_mode(void);
void tv_dolby_vision_crc_clear(int flag);
char *tv_dolby_vision_get_crc(u32 *len);
void tv_dolby_vision_insert_crc(bool print);
int dolby_vision_check_hdr10(struct vframe_s *vf);
int dolby_vision_check_hlg(struct vframe_s *vf);
int dolby_vision_check_hdr10plus(struct vframe_s *vf);
void tv_dolby_vision_dma_table_modify
	(u32 tbl_id, uint64_t value);
void tv_dolby_vision_efuse_info(void);
int dolby_vision_parse_metadata
	(struct vframe_s *vf, u8 toggle_mode,
	 bool bypass_release, bool drop_flag);
void dolby_vision_update_vsvdb_config
	(char *vsvdb_buf, u32 tbl_size);
void tv_dolby_vision_el_info(void);

int enable_rgb_to_yuv_matrix_for_dvll
	(s32 on, uint32_t *coeff_orig, uint32_t bits);

int is_dovi_frame(struct vframe_s *vf);
void update_graphic_width_height(unsigned int width, unsigned int height);
int get_dolby_vision_policy(void);
void set_dolby_vision_policy(int policy);
int get_dolby_vision_src_format(void);
bool is_dolby_vision_el_disable(void);
bool is_dovi_dual_layer_frame(struct vframe_s *vf);
void dolby_vision_set_provider(char *prov_name);
int dolby_vision_check_mvc(struct vframe_s *vf);
bool for_dolby_vision_video_effect(void);
int get_dolby_vision_hdr_policy(void);
int get_dv_support_info(void);
void dv_vf_light_reg_provider(void);
void dv_vf_light_unreg_provider(void);
void dolby_vision_update_backlight(void);
int dolby_vision_update_src_format(struct vframe_s *vf, u8 toggle_mode);
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
void dolby_vision_clear_buf(void);
#endif
