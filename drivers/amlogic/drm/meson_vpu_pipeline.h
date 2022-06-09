/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __MESON_VPU_TOPOLOGY_H
#define __MESON_VPU_TOPOLOGY_H

#include <linux/kfifo.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <dt-bindings/display/meson-drm-ids.h>
#include "meson_vpu_reg.h"
#include "meson_drv.h"
#include "meson_vpu_util.h"

#define MESON_OSD1 0
#define MESON_OSD2 1
#define MESON_OSD3 2
#define MESON_OSD4 3
#define MESON_MAX_OSDS 4
#define MESON_MAX_VIDEOS 3
#define MESON_MAX_OSD_BLEND 3
#define MESON_MAX_OSD_TO_VPP 2
#define MESON_MAX_SCALERS 4
#define MESON_MAX_HDRS 2
#define MESON_MAX_DBS 2
#define MESON_MAX_POSTBLEND 3
#define MESON_MAX_BLOCKS 32
#define MESON_BLOCK_MAX_INPUTS 6
#define MESON_BLOCK_MAX_OUTPUTS 3
#define MESON_BLOCK_MAX_NAME_LEN 32
/*ratio base for scaler calc;maybe need bigger than 1000*/
#define RATIO_BASE 1000
#define MESON_OSD_INPUT_W_LIMIT 3840
#define MESON_OSD_INPUT_H_LIMIT 2160

#define MAX_DIN_NUM 4
#define MAX_DOUT_NUM 2

#define VP_MAP_STRUCT_SIZE	120
#define BUFFER_NUM		4
#define MAX_DFS_PATH_NUM 2
/*
 *according to reg description,scale down limit shold be 4bits=16
 *but test result is 10,it will display abnormal if bigger than 10.xx
 *have not get more info from others,so config it as 10 right now.
 *Todo:if you make sure and test okay,you can rechange it.
 */
#define MESON_OSD_SCLAE_DOWN_LIMIT 10
#define MESON_OSD_SCLAE_UP_LIMIT ((1 << 24) - 1)

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
	MESON_BLK_VIDEO,
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
			     struct meson_vpu_block_state *new_state,
			     struct meson_vpu_block_state *old_state);
	void (*enable)(struct meson_vpu_block *vblk,
		       struct meson_vpu_block_state *new_state);
	void (*disable)(struct meson_vpu_block *vblk,
			struct meson_vpu_block_state *old_state);
	void (*dump_register)(struct meson_vpu_block *vblk,
			      struct seq_file *seq);
	void (*init)(struct meson_vpu_block *vblk);
	void (*fini)(struct meson_vpu_block *vblk);
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
	/*index in same type objects arry,update to object_index*/
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
	struct meson_vpu_sub_pipeline *sub;
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
	u32 fb_w;
	u32 fb_h;
	u32 zorder;
	u32 byte_stride;
	u32 pixel_format;
	u64 phy_addr;
	u32 plane_index;
	u32 uhd_plane_index;
	u32 enable;
	u32 ratio_x;/*input_w/output_w*/
	u32 afbc_inter_format;
	u32 afbc_en;
	u32 fb_size;
	u32 pixel_blend;
	u32 rotation;
	u32 blend_bypass;
	u32 global_alpha;
	u32 scaling_filter;
	u32 crtc_index;
	u32 read_ports;
	u32 status_changed;
};

struct meson_vpu_osd {
	struct meson_vpu_block base;
	struct osd_mif_reg_s *reg;
};

struct meson_vpu_osd_state {
	struct meson_vpu_block_state base;

	u64 phy_addr;
	u32 index;
	u32 enable;
	u32 color_key_enable;
	u32 dimm_enable;
	u32 mode_3d_enable;

	u32 color_key;
	u32 alpha;
	u32 global_alpha;
	u32 dimm_color;
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
	u32 pixel_blend;
	u32 afbc_en;
	u32 rotation;
	u32 blend_bypass;
	u32 crtc_index;
	u32 read_ports;
};

struct meson_vpu_video_layer_info {
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
	u64 phy_addr[2];
	u32 plane_index;
	u32 enable;
	u32 ratio_x;/*input_w/output_w*/
	u32 fb_size[2];
	u32 pixel_blend;
	u32 crtc_index;
	struct vframe_s *vf;
	struct dma_buf *dmabuf;
	u32 vfm_mode;
	bool is_uvm;
};

struct meson_vpu_video {
	struct meson_vpu_block base;
	struct vframe_provider_s vprov;
	char vfm_map_id[VP_MAP_STRUCT_SIZE];
	char vfm_map_chain[VP_MAP_STRUCT_SIZE];
	DECLARE_KFIFO(ready_q, struct vframe_s *, BUFFER_NUM);
	DECLARE_KFIFO(free_q, struct vframe_s *, BUFFER_NUM);
	DECLARE_KFIFO(display_q, struct vframe_s *, BUFFER_NUM);
	struct vframe_s vframe[BUFFER_NUM];
	u32 video_path_reg;
	u32 vfm_mode;
	bool video_enabled;
	struct dma_fence *fence;
	struct list_head vfm_node[MESON_MAX_VIDEO];
};

struct meson_vpu_video_state {
	struct meson_vpu_block_state base;

	u32 index;
	u32 enable;
	u32 color_key_enable;
	u32 dimm_enable;
	u32 mode_3d_enable;

	u32 crtc_index;
	u32 color_key;
	u32 alpha;
	u32 global_alpha;
	u32 dimm_color;
	u32 phy_addr[2];
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
	u32 fb_size[2];
	u32 pixel_blend;
	u32 afbc_en;
	struct vframe_s *vf;
	struct dma_buf *dmabuf;
	bool is_uvm;
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

	u32 crtc_index;
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
	u32 scaler_filter_mode;
};

struct meson_vpu_scaler_param {
	u32 input_width;
	u32 input_height;
	u32 output_width;
	u32 output_height;

	//u32 ratio_x;
	//u32 ratio_y;

	u32 ratio_w_num;
	u32 ratio_w_den;
	u32 ratio_h_num;
	u32 ratio_h_den;

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

struct meson_vpu_db {
	struct meson_vpu_block base;
};

struct meson_vpu_db_state {
	struct meson_vpu_block_state base;
};

struct meson_vpu_postblend {
	struct meson_vpu_block base;
	struct postblend_reg_s *reg;
	struct postblend1_reg_s *reg1;
};

struct meson_vpu_postblend_state {
	struct meson_vpu_block_state base;
	struct osd_scope_s postblend_scope[MESON_MAX_OSD_TO_VPP];
};

/* vpu pipeline */
struct rdma_reg_ops {
	u32 (*rdma_read_reg)(u32 addr);
	int (*rdma_write_reg)(u32 addr, u32 val);
	int (*rdma_write_reg_bits)(u32 addr, u32 val, u32 start, u32 len);
};

struct meson_vpu_sub_pipeline {
	//todo:update to vpp_index;
	int index;
	//todo: update name
	struct meson_vpu_pipeline *pipeline;
	struct drm_display_mode mode;
	struct rdma_reg_ops *reg_ops;
};

struct meson_vpu_pipeline {
	struct drm_private_obj obj;
	struct meson_vpu_sub_pipeline subs[MESON_MAX_CRTC];
	struct meson_vpu_osd *osds[MESON_MAX_OSDS];
	struct meson_vpu_video *video[MESON_MAX_VIDEOS];
	struct meson_vpu_afbc *afbc_osds[MESON_MAX_OSDS];
	struct meson_vpu_scaler *scalers[MESON_MAX_SCALERS];
	struct meson_vpu_osdblend *osdblend;
	struct meson_vpu_hdr *hdrs[MESON_MAX_HDRS];
	struct meson_vpu_db *dbs[MESON_MAX_DBS];
	struct meson_vpu_postblend *postblends[MESON_MAX_POSTBLEND];
	struct meson_vpu_pipeline_state *state;
	u32 num_osds;
	u32 num_video;
	u32 num_afbc_osds;
	u32 num_scalers;
	u32 num_hdrs;
	u32 num_dbs;
	u32 num_postblend;
	u8 osd_version;

	struct meson_drm *priv;
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

struct meson_vpu_sub_pipeline_state {
	u64 enable_blocks;
};

struct meson_vpu_pipeline_state {
	struct drm_private_state obj;
	struct meson_vpu_common_state common_cfg;
	struct meson_vpu_pipeline *pipeline;
	struct meson_vpu_sub_pipeline_state sub_states[MESON_MAX_CRTC];
	struct meson_vpu_osd_layer_info plane_info[MESON_MAX_OSDS];
	struct meson_vpu_video_layer_info video_plane_info[MESON_MAX_VIDEOS];
	u32 num_plane;
	u32 num_plane_video;
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
	u32 video_plane_index[MESON_MAX_VIDEOS];
	u32 din_index[MAX_DIN_NUM];
	u32 dout_index[MAX_DIN_NUM];
	u32 scaler_cnt[MAX_DIN_NUM];
	struct meson_vpu_block *scale_blk[MESON_MAX_OSDS][MESON_MAX_SCALERS];
	u32 dout_zorder[MAX_DOUT_NUM];
	u32 global_afbc;
};

#define to_osd_block(x) container_of(x, struct meson_vpu_osd, base)
#define to_afbc_block(x) container_of(x, struct meson_vpu_afbc, base)
#define to_scaler_block(x) container_of(x, struct meson_vpu_scaler, base)
#define to_osdblend_block(x) container_of(x, struct meson_vpu_osdblend, base)
#define to_hdr_block(x) container_of(x, struct meson_vpu_hdr, base)
#define to_db_block(x) container_of(x, struct meson_vpu_db, base)
#define to_postblend_block(x) container_of(x, struct meson_vpu_postblend, base)
#define to_video_block(x) container_of(x, struct meson_vpu_video, base)

#define to_osd_state(x) container_of(x, struct meson_vpu_osd_state, base)
#define to_afbc_state(x) container_of(x, struct meson_vpu_afbc_state, base)
#define to_scaler_state(x) container_of(x, struct meson_vpu_scaler_state, base)
#define to_osdblend_state(x) container_of(x, \
		struct meson_vpu_osdblend_state, base)
#define to_hdr_state(x) container_of(x, struct meson_vpu_hdr_state, base)
#define to_db_state(x) container_of(x, struct meson_vpu_db_state, base)
#define to_postblend_state(x) container_of(x, \
		struct meson_vpu_postblend_state, base)
#define to_video_state(x) container_of(x, struct meson_vpu_video_state, base)

#define priv_to_block(x) container_of(x, struct meson_vpu_block, obj)
#define priv_to_block_state(x) container_of(x, \
		struct meson_vpu_block_state, obj)
#define priv_to_pipeline(x) container_of(x, struct meson_vpu_pipeline, obj)
#define priv_to_pipeline_state(x) container_of(x, \
		struct meson_vpu_pipeline_state, obj)

int vpu_pipeline_video_check(struct meson_vpu_pipeline *pipeline,
		       struct drm_atomic_state *state);
int vpu_pipeline_video_update(struct meson_vpu_sub_pipeline *pipeline,
			struct drm_atomic_state *old_state);

int vpu_pipeline_osd_check(struct meson_vpu_pipeline *pipeline,
		       struct drm_atomic_state *state);
int vpu_pipeline_osd_update(struct meson_vpu_sub_pipeline *pipeline,
			struct drm_atomic_state *old_state);

int vpu_topology_populate(struct meson_vpu_pipeline *pipeline);
int vpu_topology_init(struct platform_device *pdev, struct meson_drm *private);
int vpu_pipeline_check(struct meson_vpu_pipeline *pipeline,
		       struct drm_atomic_state *state);
int vpu_video_plane_update(struct meson_vpu_sub_pipeline *pipeline,
			struct drm_atomic_state *old_state, int plane_index);
int vpu_osd_pipeline_update(struct meson_vpu_sub_pipeline *pipeline,
			struct drm_atomic_state *old_state);
void vpu_pipeline_init(struct meson_vpu_pipeline *pipeline);
void vpu_pipeline_fini(struct meson_vpu_pipeline *pipeline);

int vpu_pipeline_read_scanout_pos(struct meson_vpu_pipeline *pipeline,
			int *vpos, int *hpos, int crtc_index);
void vpu_pipeline_prepare_update(struct meson_vpu_pipeline *pipeline,
	int vdisplay, int vrefresh, int crtc_index);
void vpu_pipeline_finish_update(struct meson_vpu_pipeline *pipeline, int crtc_index);

/* meson_vpu_pipeline_private.c */
struct meson_vpu_block_state *
meson_vpu_block_get_state(struct meson_vpu_block *block,
			  struct drm_atomic_state *state);

struct meson_vpu_block_state *
meson_vpu_block_get_old_state(struct meson_vpu_block *mvb,
			struct drm_atomic_state *state);

struct meson_vpu_pipeline_state *
meson_vpu_pipeline_get_state(struct meson_vpu_pipeline *pipeline,
			     struct drm_atomic_state *state);
int meson_vpu_block_state_init(struct meson_drm *private,
			       struct meson_vpu_pipeline *pipeline);

int combination_traverse(struct meson_vpu_pipeline_state *mvps,
			 struct drm_atomic_state *state);
int vpu_pipeline_traverse(struct meson_vpu_pipeline_state *mvps,
			  struct drm_atomic_state *state);
int vpu_pipeline_check_osdblend(u32 *out_port, int num_planes,
				struct meson_vpu_pipeline_state *mvps,
					struct drm_atomic_state *state);
int vpu_video_pipeline_check_block(struct meson_vpu_pipeline_state *mvps,
				   struct drm_atomic_state *state);
void vpu_pipeline_check_finish_reg(int crtc_index);

void meson_rdma_ops_init(struct meson_vpu_pipeline *pipeline, int num_crtc);

extern struct meson_vpu_block_ops video_ops;
extern struct meson_vpu_block_ops osd_ops;
extern struct meson_vpu_block_ops afbc_ops;
extern struct meson_vpu_block_ops scaler_ops;
extern struct meson_vpu_block_ops osdblend_ops;
extern struct meson_vpu_block_ops hdr_ops;
extern struct meson_vpu_block_ops db_ops;
extern struct meson_vpu_block_ops postblend_ops;

extern struct meson_vpu_block_ops t7_osd_ops;
extern struct meson_vpu_block_ops t7_afbc_ops;
extern struct meson_vpu_block_ops t3_afbc_ops;
extern struct meson_vpu_block_ops t7_postblend_ops;
extern struct meson_vpu_block_ops t3_postblend_ops;

#ifdef CONFIG_DEBUG_FS
extern u32 overwrite_reg[256];
extern u32 overwrite_val[256];
extern int overwrite_enable;
extern int reg_num;
#endif

#endif
