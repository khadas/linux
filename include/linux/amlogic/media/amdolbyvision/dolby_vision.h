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

bool is_dovi_frame(struct vframe_s *vf);
void update_graphic_width_height(unsigned int width, unsigned int height);
int get_dolby_vision_policy(void);
void set_dolby_vision_policy(int policy);
int get_dolby_vision_src_format(void);
bool is_dolby_vision_el_disable(void);
bool is_dovi_dual_layer_frame(struct vframe_s *vf);
void dolby_vision_set_provider(char *prov_name);
int dolby_vision_check_mvc(struct vframe_s *vf);
bool for_dolby_vision_video_effect(void);
#endif
