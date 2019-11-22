/*
 * drivers/amlogic/drm/meson_vpu_pipeline.h
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#ifndef __MESON_VPU_TOPOLOGY_H
#define __MESON_VPU_TOPOLOGY_H

#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <dt-bindings/display/meson-drm-ids.h>
#include "meson_vpu_reg.h"
#include "meson_drv.h"
#include "meson_vpu_util.h"

#define MESON_OSD1 0
#define MESON_OSD2 1
#define MESON_OSD3 2
#define MESON_MAX_OSDS 4
#define MESON_MAX_OSD_BLEND 3
#define MESON_MAX_OSD_TO_VPP 2
#define MESON_MAX_SCALERS 4
#define MESON_MAX_VIDEO 2
#define MESON_MAX_BLOCKS 32
#define MESON_BLOCK_MAX_INPUTS 6
#define MESON_BLOCK_MAX_OUTPUTS 3
#define MESON_BLOCK_MAX_NAME_LEN 32
/*ratio base for scaler calc;maybe need bigger than 1000*/
#define RATIO_BASE 1000
#define MESON_OSD_INPUT_W_LIMIT 3840

#define MAX_DIN_NUM 4
#define MAX_DOUT_NUM 2

#define MAX_DFS_PATH_NUM 2
/*
 *according to reg description,scale down limit shold be 4bits=16
 *but test result is 10,it will display abnormal if bigger than 10.xx
 *have not get more info from others,so config it as 10 right now.
 *Todo:if you make sure and test okay,you can rechange it.
 */
#define MESON_OSD_SCLAE_DOWN_LIMIT 10
#define MESON_OSD_SCLAE_UP_LIMIT ((1 << 24) - 1)
/*
 *MESON_DRM_VERSION_V0:support modetest and atomictest,
 *backup last commit state
 *MESON_DRM_VERSION_V1:support atomictest,
 *don't support modetest,don't backup last commit state
 */
#define MESON_DRM_VERSION_V0 0

#define SCALER_RATIO_X_CALC_DONE BIT(0)
#define SCALER_RATIO_Y_CALC_DONE BIT(1)
#define SCALER_IN_W_CALC_DONE BIT(2)
#define SCALER_IN_H_CALC_DONE BIT(3)
#define SCALER_OUT_W_CALC_DONE BIT(4)
#define SCALER_OUT_H_CALC_DONE BIT(5)

#define SCALER_INPUT_WIDTH_CHANGED BIT(0)
#define SCALER_INPUT_HEIGHT_CHANGED BIT(1)
#define SCALER_OUTPUT_WIDTH_CHANGED BIT(2)
#define SCALER_OUTPUT_HEIGHT_CHANGED BIT(3)
#define SCALER_OUTPUT_SCAN_MODE_CHANGED BIT(4)

enum meson_vpu_blk_type {
	MESON_BLK_OSD = 0,
	MESON_BLK_AFBC,
	MESON_BLK_SCALER,
	MESON_BLK_OSDBLEND,
	MESON_BLK_HDR,
	MESON_BLK_DOVI,
	MESON_BLK_VPPBLEND,
};

struct meson_vpu_pipeline;
struct meson_vpu_block;
struct meson_vpu_block_state;
struct meson_vpu_pipeline_state;

/* vpu block ops */
struct meson_vpu_block_ops {
	int (*check_state)(struct meson_vpu_block *vblk,
		struct meson_vpu_block_state *state,
		struct meson_vpu_pipeline_state *mvps);
	void (*update_state)(struct meson_vpu_block *vblk,
		struct meson_vpu_block_state *state);
	void (*enable)(struct meson_vpu_block *vblk);
	void (*disable)(struct meson_vpu_block *vblk);
	void (*dump_register)(struct meson_vpu_block *vblk,
			struct seq_file *seq);
	void (*init)(struct meson_vpu_block *vblk);
};

struct meson_vpu_block_link {
	struct meson_vpu_block *link;
	u8 id;
	u8 port;
	int edges_active;
	int edges_visited;
};

/* vpu block */
struct meson_vpu_block {
	struct drm_private_obj obj;
	char name[MESON_BLOCK_MAX_NAME_LEN];

	enum meson_vpu_blk_type type;
	u8 id;
	u8 index;
	u8 max_inputs;
	u8 max_outputs;
	u8 avail_inputs;
	u8 avail_outputs;
	unsigned long inputs_mask;
	unsigned long outputs_mask;
	struct meson_vpu_block_link inputs[MESON_BLOCK_MAX_INPUTS];
	struct meson_vpu_block_link outputs[MESON_BLOCK_MAX_OUTPUTS];
	struct meson_vpu_block_ops *ops;
	struct meson_vpu_pipeline *pipeline;
};

struct meson_vpu_block_state {
	struct drm_private_state obj;
	struct meson_vpu_block *pblk;
	u32 inputs_mask;

	u32 inputs_changed;
	u32 outputs_changed;
	u32 checked;

	u32 state_changed;

	struct meson_vpu_block_link inputs[MESON_BLOCK_MAX_INPUTS];
	struct meson_vpu_block_link outputs[MESON_BLOCK_MAX_OUTPUTS];
	int in_stack;
	int active;


};

struct meson_vpu_osd_layer_info {
	u32 src_x;
	u32 src_y;
	u32 src_w;
	u32 src_h;
	u32 dst_w;
	u32 dst_h;
	int dst_x;
	int dst_y;
	u32 zorder;
	u32 byte_stride;
	u32 pixel_format;
	u32 phy_addr;
	u32 plane_index;
	u32 enable;
	u32 ratio_x;/*input_w/output_w*/
	u32 afbc_inter_format;
	u32 afbc_en;
	u32 fb_size;
	u32 premult_en;
};

struct meson_vpu_osd {
	struct meson_vpu_block base;
	struct osd_mif_reg_s *reg;
};

struct meson_vpu_osd_state {
	struct meson_vpu_block_state base;

	u32 index;
	u32 enable;
	u32 color_key_enable;
	u32 dimm_enable;
	u32 mode_3d_enable;

	u32 color_key;
	u32 alpha;
	u32 global_alpha;
	u32 dimm_color;
	u32 phy_addr;
	u32 pixel_format;
	u32 zorder;
	u32 byte_stride;
	u32 src_x;
	u32 src_y;
	u32 src_w;
	u32 src_h;
	u32 dst_w;
	u32 dst_h;
	int dst_x;
	int dst_y;
	int s_mode;
	int r_mode;
	u32 plane_index;
	u32 fb_size;
	u32 premult_en;
};

struct meson_vpu_afbc {
	struct meson_vpu_block base;
	struct afbc_osd_reg_s *afbc_regs;
	struct afbc_status_reg_s *status_regs;
};

struct meson_vpu_afbc_state {
	struct meson_vpu_block_state base;

	u32 format;
	u32 inter_format;
};

struct meson_vpu_scaler {
	struct meson_vpu_block base;
	struct osd_scaler_reg_s *reg;
	u32 linebuffer;/*base pixel*/
	u32 bank_length;/*base line*/
};

struct meson_vpu_scaler_state {
	struct meson_vpu_block_state base;

	u32 free_scale_mode;
	u32 input_width;
	u32 input_height;
	u32 output_width;
	u32 output_height;
	u32 ratio_x;
	u32 ratio_y;
	u32 scan_mode_out;
	u32 state_changed;
	u32 free_scale_enable;
};

struct meson_vpu_scaler_param {
	u32 input_width;
	u32 input_height;
	u32 output_width;
	u32 output_height;
	u32 ratio_x;
	u32 ratio_y;
	/*calc_done_mask:
	 *bit0:ratio_x,
	 *bit1:ratio_y
	 *bit2:input_width
	 *bit3:input_height
	 *bit4:output_width
	 *bit5:output_height
	 */
	u32 calc_done_mask;
	/*
	 *bit0:plane0
	 *bit1:plane1
	 *bit2:plane2
	 *bit*:plane*
	 */
	u32 plane_mask;
	u32 enable;
	u32 before_osdblend;
};

struct meson_vpu_osdblend {
	struct meson_vpu_block base;
	struct osdblend_reg_s *reg;
};

struct meson_vpu_osdblend_state {
	struct meson_vpu_block_state base;

	u32 input_num;
	/*bit0/bit1/bit2-->osd0/osd1/osd2*/
	u32 input_osd_mask;
	/*Din mask:bit0:DIN0;bit1:DIN1;bit2:DIN2;bit3:DIN3*/
	u32 input_mask;
	/*Din0~3 select which input channel or osd(0/1/2)*/
	u32 din_channel_mux[MAX_DIN_NUM];
	/*osd(0/1/2) go through osdblend to dout0 or dout1*/
	u32 dout_mux[MAX_DIN_NUM];
	/*scope position before mux,tied with osd0/osd1/osd2*/
	struct osd_scope_s din_channel_scope[MAX_DIN_NUM];
	/*sub-blend0 and sub-blend1 size*/
	u32 input_width[MESON_MAX_OSD_BLEND];
	u32 input_height[MESON_MAX_OSD_BLEND];
	/*0:din0-->blend0;1:din0-->Dout0,bypass OsdBlend*/
	u32 din0_switch;
	/*0:din3-->blend1;1:din3-->Dout1,bypass OsdBlend*/
	u32 din3_switch;
	/*0:blend1-->blend2;1:blend1-->Dout1,bypass Blend2*/
	u32 blend1_switch;

};

struct meson_vpu_hdr {
	struct meson_vpu_block base;

};

struct meson_vpu_hdr_state {
	struct meson_vpu_block_state base;
};

struct meson_vpu_dolby {
	struct meson_vpu_block base;
};

struct meson_vpu_dolby_state {
	struct meson_vpu_block_state base;
};

struct meson_vpu_postblend {
	struct meson_vpu_block base;
	struct postblend_reg_s *reg;
};

struct meson_vpu_postblend_state {
	struct meson_vpu_block_state base;
	struct osd_scope_s postblend_scope[MESON_MAX_OSD_TO_VPP];
};

/* vpu pipeline */
struct meson_vpu_pipeline {
	struct drm_private_obj obj;
	struct drm_display_mode mode;
	struct meson_vpu_osd *osds[MESON_MAX_OSDS];
	struct meson_vpu_afbc *afbc_osds[MESON_MAX_OSDS];
	struct meson_vpu_scaler *scalers[MESON_MAX_SCALERS];
	struct meson_vpu_osdblend *osdblend;
	struct meson_vpu_hdr *hdr;
	struct meson_vpu_dolby *dolby;
	struct meson_vpu_postblend *postblend;
	struct meson_vpu_pipeline_state *state;
	u32 num_osds;
	u32 num_afbc_osds;
	u32 num_scalers;
	u8 osd_version;

	struct drm_crtc *crtc;
	struct meson_vpu_block **mvbs;
	int num_blocks;
};

struct meson_vpu_common_state {
	u32 color_format;
	u64 block_mask;
};

struct meson_vpu_stack {
	int num_blocks;
	int top;
	struct meson_vpu_block *stack[MESON_MAX_BLOCKS];
};

struct meson_vpu_traverse {
	struct meson_vpu_block *path[MAX_DFS_PATH_NUM][MESON_MAX_BLOCKS];
	int num_path;
};

struct meson_vpu_pipeline_state {
	struct drm_private_state obj;
	struct meson_vpu_common_state common_cfg;
	struct meson_vpu_pipeline *pipeline;
	u64 enable_blocks;
	struct meson_vpu_osd_layer_info plane_info[MESON_MAX_OSDS];
	u32 num_plane;
	/*min --> max*/
	u32 zorder_plane_index[MESON_MAX_OSDS];
	u32 ratio_plane_index[MESON_MAX_OSDS];
	struct meson_vpu_scaler_param scaler_param[MESON_MAX_SCALERS];
	/*pre_osd_scope is before DIN*/
	struct osd_scope_s osd_scope_pre[MAX_DIN_NUM];

	/*some traverse help structure*/
	struct meson_vpu_stack osd_stack[MESON_MAX_OSDS];

	/*store traverse result for every path*/
	struct meson_vpu_traverse osd_traverse[MESON_MAX_OSDS];

	u32 plane_index[MESON_MAX_OSDS];
	u32 din_index[MAX_DIN_NUM];
	u32 dout_index[MAX_DIN_NUM];
	u32 scaler_cnt[MAX_DIN_NUM];
	struct meson_vpu_block *scale_blk[MESON_MAX_OSDS][MESON_MAX_SCALERS];
	u32 dout_zorder[MAX_DOUT_NUM];
};

#define to_osd_block(x) container_of(x, struct meson_vpu_osd, base)
#define to_afbc_block(x) container_of(x, struct meson_vpu_afbc, base)
#define to_scaler_block(x) container_of(x, struct meson_vpu_scaler, base)
#define to_osdblend_block(x) container_of(x, struct meson_vpu_osdblend, base)
#define to_hdr_block(x) container_of(x, struct meson_vpu_hdr, base)
#define to_dolby_block(x) container_of(x, struct meson_vpu_dolby, base)
#define to_postblend_block(x) container_of(x, struct meson_vpu_postblend, base)

#define to_osd_state(x) container_of(x, struct meson_vpu_osd_state, base)
#define to_afbc_state(x) container_of(x, struct meson_vpu_afbc_state, base)
#define to_scaler_state(x) container_of(x, struct meson_vpu_scaler_state, base)
#define to_osdblend_state(x) container_of(x, \
		struct meson_vpu_osdblend_state, base)
#define to_hdr_state(x) container_of(x, struct meson_vpu_hdr_state, base)
#define to_dolby_state(x) container_of(x, struct meson_vpu_dolby_state, base)
#define to_postblend_state(x) container_of(x, \
		struct meson_vpu_postblend_state, base)

#define priv_to_block(x) container_of(x, struct meson_vpu_block, obj)
#define priv_to_block_state(x) container_of(x, \
		struct meson_vpu_block_state, obj)
#define priv_to_pipeline(x) container_of(x, struct meson_vpu_pipeline, obj)
#define priv_to_pipeline_state(x) container_of(x, \
		struct meson_vpu_pipeline_state, obj)

int vpu_topology_init(struct platform_device *pdev, struct meson_drm *private);
int vpu_pipeline_check(struct meson_vpu_pipeline *pipeline,
		struct drm_atomic_state *state);
int vpu_pipeline_update(struct meson_vpu_pipeline *pipeline,
		struct drm_atomic_state *old_state);
void vpu_pipeline_init(struct meson_vpu_pipeline *pipeline);

/* meson_vpu_pipeline_private.c */
struct meson_vpu_block_state *
meson_vpu_block_get_state(struct meson_vpu_block *block,
			  struct drm_atomic_state *state);
struct meson_vpu_pipeline_state *
meson_vpu_pipeline_get_state(struct meson_vpu_pipeline *pipeline,
			     struct drm_atomic_state *state);
int meson_vpu_block_state_init(struct meson_drm *private,
	struct meson_vpu_pipeline *pipeline);
#ifdef MESON_DRM_VERSION_V0
void meson_vpu_pipeline_atomic_backup_state(
	struct meson_vpu_pipeline_state *mvps);
#endif

int combination_traverse(struct meson_vpu_pipeline_state *mvps,
				struct drm_atomic_state *state);
int vpu_pipeline_traverse(struct meson_vpu_pipeline_state *mvps,
			  struct drm_atomic_state *state);
int vpu_pipeline_check_osdblend(u32 *out_port, int num_planes,
				struct meson_vpu_pipeline_state *mvps,
					struct drm_atomic_state *state);

extern struct meson_vpu_block_ops osd_ops;
extern struct meson_vpu_block_ops afbc_ops;
extern struct meson_vpu_block_ops scaler_ops;
extern struct meson_vpu_block_ops osdblend_ops;
extern struct meson_vpu_block_ops hdr_ops;
extern struct meson_vpu_block_ops dolby_ops;
extern struct meson_vpu_block_ops postblend_ops;


 #endif
