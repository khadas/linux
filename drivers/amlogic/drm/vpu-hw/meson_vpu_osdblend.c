// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

/* Amlogic Headers */
#include <linux/amlogic/media/vout/vout_notify.h>

#include "meson_vpu_pipeline.h"
#include "meson_vpu_reg.h"
#include "meson_vpu_util.h"
#include "meson_vpu_osdblend.h"
#include "meson_osd_proc.h"
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
#include <linux/amlogic/media/amdolbyvision/dolby_vision.h>
#endif

static int align_proc = 4;
module_param(align_proc, int, 0664);
MODULE_PARM_DESC(align_proc, "align_proc");

#define BLEND_DOUT_DEF_HSIZE 3840
#define BLEND_DOUT_DEF_VSIZE 2160

static int osd_enable[MESON_MAX_OSDS] = {1, 0, 1, 0};

static struct osdblend_reg_s osdblend_reg = {
	VIU_OSD_BLEND_CTRL,
	VIU_OSD_BLEND_DIN0_SCOPE_H,
	VIU_OSD_BLEND_DIN0_SCOPE_V,
	VIU_OSD_BLEND_DIN1_SCOPE_H,
	VIU_OSD_BLEND_DIN1_SCOPE_V,
	VIU_OSD_BLEND_DIN2_SCOPE_H,
	VIU_OSD_BLEND_DIN2_SCOPE_V,
	VIU_OSD_BLEND_DIN3_SCOPE_H,
	VIU_OSD_BLEND_DIN3_SCOPE_V,
	VIU_OSD_BLEND_DUMMY_DATA0,
	VIU_OSD_BLEND_DUMMY_ALPHA,
	VIU_OSD_BLEND_BLEND0_SIZE,
	VIU_OSD_BLEND_BLEND1_SIZE,
	VIU_OSD_BLEND_CTRL1,
};

static struct osdblend_reg_s osdblend_s5_reg = {
	VIU_OSD_BLEND_CTRL_S5,
	VIU_OSD_BLEND_DIN0_SCOPE_H_S5,
	VIU_OSD_BLEND_DIN0_SCOPE_V_S5,
	VIU_OSD_BLEND_DIN1_SCOPE_H_S5,
	VIU_OSD_BLEND_DIN1_SCOPE_V_S5,
	VIU_OSD_BLEND_DIN2_SCOPE_H_S5,
	VIU_OSD_BLEND_DIN2_SCOPE_V_S5,
	VIU_OSD_BLEND_DIN3_SCOPE_H_S5,
	VIU_OSD_BLEND_DIN3_SCOPE_V_S5,
	VIU_OSD_BLEND_DUMMY_DATA0_S5,
	VIU_OSD_BLEND_DUMMY_ALPHA_S5,
	VIU_OSD_BLEND_BLEND0_SIZE_S5,
	VIU_OSD_BLEND_BLEND1_SIZE_S5,
	VIU_OSD_BLEND_CTRL1_S5,
};

/*0:din0 go through blend0,1:bypass blend0,dirct to Dout0*/
static void osd_din0_switch_set(struct meson_vpu_block *vblk,
				struct rdma_reg_ops *reg_ops,
				struct osdblend_reg_s *reg,
				bool bypass_state)
{
	reg_ops->rdma_write_reg_bits(reg->viu_osd_blend_ctrl,
				     bypass_state, 26, 1);
}

/*0:blend1 out to blend2,1:blend1 out to Dout1*/
static void osd_blend1_dout_switch_set(struct meson_vpu_block *vblk,
				       struct rdma_reg_ops *reg_ops,
				       struct osdblend_reg_s *reg,
				       bool bypass_state)
{
	reg_ops->rdma_write_reg_bits(reg->viu_osd_blend_ctrl,
				     bypass_state, 25, 1);
}

/*0:din3 pass through blend1,1:bypass blend1,direct to Dout1*/
static void osd_din3_switch_set(struct meson_vpu_block *vblk,
				struct rdma_reg_ops *reg_ops,
				struct osdblend_reg_s *reg,
				bool bypass_state)
{
	reg_ops->rdma_write_reg_bits(reg->viu_osd_blend_ctrl,
				     bypass_state, 24, 1);
}

/*0:din0 input disable,1:din0 input enable*/
static void osd_din0_input_enable_set(struct meson_vpu_block *vblk,
				      struct rdma_reg_ops *reg_ops,
				      struct osdblend_reg_s *reg,
				      bool input_enable)
{
	reg_ops->rdma_write_reg_bits(reg->viu_osd_blend_ctrl,
				     input_enable, 20, 1);
}

/*0:din1 input disable,1:din1 input enable*/
static void osd_din1_input_enable_set(struct meson_vpu_block *vblk,
				      struct rdma_reg_ops *reg_ops,
				      struct osdblend_reg_s *reg,
				      bool input_enable)
{
	reg_ops->rdma_write_reg_bits(reg->viu_osd_blend_ctrl,
				     input_enable, 21, 1);
}

/*0:din2 input disable,1:din2 input enable*/
static void osd_din2_input_enable_set(struct meson_vpu_block *vblk,
				      struct rdma_reg_ops *reg_ops,
				      struct osdblend_reg_s *reg,
				      bool input_enable)
{
	reg_ops->rdma_write_reg_bits(reg->viu_osd_blend_ctrl,
				     input_enable, 22, 1);
}

/*0:din3 input disable,1:din3 input enable*/
static void osd_din3_input_enable_set(struct meson_vpu_block *vblk,
				      struct rdma_reg_ops *reg_ops,
				      struct osdblend_reg_s *reg,
				      bool input_enable)
{
	reg_ops->rdma_write_reg_bits(reg->viu_osd_blend_ctrl,
				     input_enable, 23, 1);
}

/*1/2/3:din0/1/2/3 select osd1/osd2/osd3,else select null*/
static void osd_din_channel_mux_set(struct meson_vpu_block *vblk,
				    struct rdma_reg_ops *reg_ops,
				    struct osdblend_reg_s *reg,
				    enum osd_channel_e osd_channel,
				    enum din_channel_e din_channel)
{
	reg_ops->rdma_write_reg_bits(reg->viu_osd_blend_ctrl, osd_channel,
				     (0 + din_channel * 4), 4);
}

/*din0 scope config*/
static void osd_din0_scope_set(struct meson_vpu_block *vblk,
			       struct rdma_reg_ops *reg_ops,
			       struct osdblend_reg_s *reg,
			       struct osd_scope_s scope)
{
	reg_ops->rdma_write_reg_bits(reg->viu_osd_blend_din0_scope_h,
				     scope.h_start, 0, 16);
	reg_ops->rdma_write_reg_bits(reg->viu_osd_blend_din0_scope_h,
				     scope.h_end, 16, 16);
	reg_ops->rdma_write_reg_bits(reg->viu_osd_blend_din0_scope_v,
				     scope.v_start, 0, 16);
	reg_ops->rdma_write_reg_bits(reg->viu_osd_blend_din0_scope_v,
				     scope.v_end, 16, 16);
}

/*din1 scope config*/
static void osd_din1_scope_set(struct meson_vpu_block *vblk,
			       struct rdma_reg_ops *reg_ops,
			       struct osdblend_reg_s *reg,
			       struct osd_scope_s scope)
{
	reg_ops->rdma_write_reg_bits(reg->viu_osd_blend_din1_scope_h,
				     scope.h_start, 0, 16);
	reg_ops->rdma_write_reg_bits(reg->viu_osd_blend_din1_scope_h,
				     scope.h_end, 16, 16);
	reg_ops->rdma_write_reg_bits(reg->viu_osd_blend_din1_scope_v,
				     scope.v_start, 0, 16);
	reg_ops->rdma_write_reg_bits(reg->viu_osd_blend_din1_scope_v,
				     scope.v_end, 16, 16);
}

/*din2 scope config*/
static void osd_din2_scope_set(struct meson_vpu_block *vblk,
			       struct rdma_reg_ops *reg_ops,
			       struct osdblend_reg_s *reg,
			       struct osd_scope_s scope)
{
	reg_ops->rdma_write_reg_bits(reg->viu_osd_blend_din2_scope_h,
				     scope.h_start, 0, 16);
	reg_ops->rdma_write_reg_bits(reg->viu_osd_blend_din2_scope_h,
				     scope.h_end, 16, 16);
	reg_ops->rdma_write_reg_bits(reg->viu_osd_blend_din2_scope_v,
				     scope.v_start, 0, 16);
	reg_ops->rdma_write_reg_bits(reg->viu_osd_blend_din2_scope_v,
				     scope.v_end, 16, 16);
}

/*din3 scope config*/
static void osd_din3_scope_set(struct meson_vpu_block *vblk,
			       struct rdma_reg_ops *reg_ops,
			       struct osdblend_reg_s *reg,
			       struct osd_scope_s scope)
{
	reg_ops->rdma_write_reg_bits(reg->viu_osd_blend_din3_scope_h,
				     scope.h_start, 0, 16);
	reg_ops->rdma_write_reg_bits(reg->viu_osd_blend_din3_scope_h,
				     scope.h_end, 16, 16);
	reg_ops->rdma_write_reg_bits(reg->viu_osd_blend_din3_scope_v,
				     scope.v_start, 0, 16);
	reg_ops->rdma_write_reg_bits(reg->viu_osd_blend_din3_scope_v,
				     scope.v_end, 16, 16);
}

/*osd blend dummy data config*/
static void osd_blend_dummy_data_set(struct meson_vpu_block *vblk,
				     struct rdma_reg_ops *reg_ops,
				     struct osdblend_reg_s *reg,
				     struct osd_dummy_data_s dummy_data)
{
	reg_ops->rdma_write_reg(reg->viu_osd_blend_dummy_data0,
				((dummy_data.channel0 & 0xff) << 16) |
				((dummy_data.channel1 & 0xff) << 8) |
				(dummy_data.channel2 & 0xff));
}

/*osd blend0 size config*/
static void osd_blend0_size_set(struct meson_vpu_block *vblk,
				struct rdma_reg_ops *reg_ops,
				struct osdblend_reg_s *reg,
				unsigned int h_size, unsigned int v_size)
{
	reg_ops->rdma_write_reg(reg->viu_osd_blend0_size,
				(v_size << 16) | h_size);
}

/*osd blend1 size config*/
static void osd_blend1_size_set(struct meson_vpu_block *vblk,
				struct rdma_reg_ops *reg_ops,
				struct osdblend_reg_s *reg,
				unsigned int h_size, unsigned int v_size)
{
	reg_ops->rdma_write_reg(reg->viu_osd_blend1_size,
				(v_size << 16) | h_size);
}

/*osd blend0 size config*/
static void osd_dv_core_size_set(u32 h_size, u32 v_size)
{
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	u32 tmp_h = h_size;
	u32 tmp_v = v_size;

	update_dvcore2_timing(&tmp_h, &tmp_v);
	meson_vpu_write_reg(DOLBY_CORE2A_SWAP_CTRL1,
			    ((tmp_h + 0x40) << 16) |
			     (tmp_v + 0x80));
	meson_vpu_write_reg(DOLBY_CORE2A_SWAP_CTRL2,
			    (tmp_h << 16) | tmp_v);

	update_graphic_width_height(h_size, v_size, 0);
#endif
}

static void osd_dv_core_size_set_s5(u32 h_size, u32 v_size, enum OSD_INDEX index)
{
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	u32 tmp_h = h_size;
	u32 tmp_v = v_size;

	update_dvcore2_timing(&tmp_h, &tmp_v);
	if (index == OSD1_INDEX) {
		meson_vpu_write_reg(DOLBY_CORE2A_SWAP_CTRL1_S5,
				    ((tmp_h + 0x40) << 16) |
				     (tmp_v + 0x80));
		meson_vpu_write_reg(DOLBY_CORE2A_SWAP_CTRL2_S5,
				    (tmp_h << 16) | tmp_v);
	}
	if (index == OSD3_INDEX) {
		meson_vpu_write_reg(DOLBY_CORE2C_SWAP_CTRL1_S5,
				    ((tmp_h + 0x40) << 16) |
				     (tmp_v + 0x80));
		meson_vpu_write_reg(DOLBY_CORE2C_SWAP_CTRL2_S5,
				    (tmp_h << 16) | tmp_v);
	}
	update_graphic_width_height(h_size, v_size, index);
#endif
}

#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
/* -1: invalid osd index
 *  0: osd is disabled
 *  1: osd is enabled
 */
int osd_dv_get_osd_status(enum OSD_INDEX index)
{
	if (index < MESON_MAX_OSDS)
		return osd_enable[index];
	else
		return -1;
}
#endif

/*osd blend0 & blend1 4 din inputs premult flag config as 0 default*/
void osd_blend01_premult_config(struct meson_vpu_block *vblk,
				struct rdma_reg_ops *reg_ops,
				struct osdblend_reg_s *reg)
{
	reg_ops->rdma_write_reg_bits(reg->viu_osd_blend_ctrl, 0, 16, 4);
}

/*osd blend2 2 inputs premult flag config as 1 default*/
void osd_blend2_premult_config(struct meson_vpu_block *vblk,
			       struct rdma_reg_ops *reg_ops,
			       struct osdblend_reg_s *reg)
{
	reg_ops->rdma_write_reg_bits(reg->viu_osd_blend_ctrl, 3, 27, 2);
}

/*osd blend dout0 output div en config as 1,alpha 9bit default*/
void osd_blend_dout0_div_config(struct meson_vpu_block *vblk,
				struct rdma_reg_ops *reg_ops,
				struct osdblend_reg_s *reg)
{
	reg_ops->rdma_write_reg_bits(reg->viu_osd_blend_ctrl1, 3, 4, 2);
	reg_ops->rdma_write_reg_bits(reg->viu_osd_blend_ctrl1, 1, 0, 1);
}

/*osd blend dout1 output div en config as 1,alpha 9bit default*/
void osd_blend_dout1_div_config(struct meson_vpu_block *vblk,
				struct rdma_reg_ops *reg_ops,
				struct osdblend_reg_s *reg)
{
	reg_ops->rdma_write_reg_bits(reg->viu_osd_blend_ctrl1, 3, 16, 2);
	reg_ops->rdma_write_reg_bits(reg->viu_osd_blend_ctrl1, 1, 12, 1);
}

/*osd blend premult config*/
void osdblend_premult_config(struct meson_vpu_block *vblk,
			     struct rdma_reg_ops *reg_ops,
			     struct osdblend_reg_s *reg)
{
	osd_blend01_premult_config(vblk, reg_ops, reg);
	osd_blend2_premult_config(vblk, reg_ops, reg);
	osd_blend_dout0_div_config(vblk, reg_ops, reg);
	osd_blend_dout1_div_config(vblk, reg_ops, reg);
}

enum osd_channel_e osd2channel(u8 osd_index)
{
	u8 din_channel_seq[MAX_DIN_NUM] = {OSD_CHANNEL1, OSD_CHANNEL2,
		OSD_CHANNEL3, OSD_CHANNEL4};

	if (osd_index >= MAX_DIN_NUM) {
		MESON_DRM_BLOCK("osd_index:%d overflow!!.\n", osd_index);
		return OSD_CHANNEL_NUM;
	}
	return din_channel_seq[osd_index];
}

static void osdblend_hw_update(struct meson_vpu_block *vblk,
			       struct rdma_reg_ops *reg_ops,
			       struct osdblend_reg_s *reg,
			       struct meson_vpu_osdblend_state *mvobs)
{
	/*din channel mux config*/
	osd_din_channel_mux_set(vblk, reg_ops, reg,
				mvobs->din_channel_mux[DIN0], DIN0);
	osd_din_channel_mux_set(vblk, reg_ops, reg,
				mvobs->din_channel_mux[DIN1], DIN1);
	osd_din_channel_mux_set(vblk, reg_ops, reg,
				mvobs->din_channel_mux[DIN2], DIN2);
	osd_din_channel_mux_set(vblk, reg_ops, reg,
				mvobs->din_channel_mux[DIN3], DIN3);

	/*internal channel disable default*/
	osd_din0_input_enable_set(vblk, reg_ops, reg,
				  (mvobs->input_mask >> DIN0) & 0x1);
	osd_din1_input_enable_set(vblk, reg_ops, reg,
				  (mvobs->input_mask >> DIN1) & 0x1);
	osd_din2_input_enable_set(vblk, reg_ops, reg,
				  (mvobs->input_mask >> DIN2) & 0x1);
	osd_din3_input_enable_set(vblk, reg_ops, reg,
				  (mvobs->input_mask >> DIN3) & 0x1);

	/*blend switch config*/
	osd_din0_switch_set(vblk, reg_ops, reg, mvobs->din0_switch);
	osd_din3_switch_set(vblk, reg_ops, reg, mvobs->din3_switch);
	osd_blend1_dout_switch_set(vblk, reg_ops, reg, mvobs->blend1_switch);

	/*scope config*/
	osd_din0_scope_set(vblk, reg_ops, reg, mvobs->din_channel_scope[DIN0]);
	osd_din1_scope_set(vblk, reg_ops, reg, mvobs->din_channel_scope[DIN1]);
	osd_din2_scope_set(vblk, reg_ops, reg, mvobs->din_channel_scope[DIN2]);
	osd_din3_scope_set(vblk, reg_ops, reg, mvobs->din_channel_scope[DIN3]);

	/*premult config*/
	osdblend_premult_config(vblk, reg_ops, reg);

	/*blend0/blend1 size config*/
	osd_blend0_size_set(vblk, reg_ops, reg,
			    mvobs->input_width[OSD_SUB_BLEND0],
			    mvobs->input_height[OSD_SUB_BLEND0]);
	osd_blend1_size_set(vblk, reg_ops, reg,
			    mvobs->input_width[OSD_SUB_BLEND1],
			    mvobs->input_height[OSD_SUB_BLEND1]);
}

static int osdblend_check_state(struct meson_vpu_block *vblk,
				struct meson_vpu_block_state *state,
		struct meson_vpu_pipeline_state *mvps)
{
	int ret, num_planes;
	u32 *out_port;
	u32 i, j, m, n, num_plane_port0, num_plane_port1;
	u32 plane_index_port0[MAX_DIN_NUM], plane_index_port1[MAX_DIN_NUM];
	struct meson_vpu_osdblend_state *mvobs;
	u32 zorder[MAX_DIN_NUM], max_height = 0, max_width = 0;
	int delta_zorder[MAX_DIN_NUM] = {0};
	bool delta_zorder_flag;
	struct osd_scope_s scope_default = {0xffff, 0xffff, 0xffff, 0xffff};

	mvobs = to_osdblend_state(state);
	num_planes = mvps->num_plane;
	out_port = mvps->dout_index;

	if (state->checked)
		return 0;

	state->checked = true;

	ret = 0;
	num_plane_port0 = 0;
	num_plane_port1 = 0;
	MESON_DRM_BLOCK("%s check_state called.\n", vblk->name);
	for (i = 0; i < MESON_MAX_OSDS; i++) {
		if (!mvps->plane_info[i].enable ||
		    mvps->plane_info[i].crtc_index != 0) {
			mvobs->input_osd_mask &= ~BIT(i);
			continue;
		}
		mvobs->input_osd_mask |= BIT(i);
		if (out_port[i] == OSD_LEND_OUT_PORT1) {
			plane_index_port1[num_plane_port1] =
				mvps->plane_index[i];
			num_plane_port1++;
		} else {
			plane_index_port0[num_plane_port0] =
				mvps->plane_index[i];
			num_plane_port0++;
		}
	}
	/*check the unsupport case firstly*/
	if (num_plane_port0 > OSD_LEND_MAX_IN_NUM_PORT0 ||
	    num_plane_port1 > OSD_LEND_MAX_IN_NUM_PORT1) {
		MESON_DRM_BLOCK("unsupport zorder %d\n", __LINE__);
		return -1;
	}
	if (mvps->pipeline->osd_version <= OSD_V2 &&
	    num_plane_port1) {
		MESON_DRM_BLOCK("unsupport zorder %d\n", __LINE__);
		return -1;
	}
	/*zorder check for one dout-port with multi plane*/
	for (i = 0; i < num_plane_port1; i++) {
		m = plane_index_port1[i];
		for (j = 0; j < num_plane_port0; j++) {
			n = plane_index_port0[j];
			delta_zorder[0] = mvps->plane_info[m].zorder -
				mvps->plane_info[n].zorder;
			delta_zorder_flag = ((delta_zorder[0] < 0) !=
				(delta_zorder[1] < 0));
			if (num_plane_port0 >= 2 && j > 0 &&
			    delta_zorder_flag) {
				MESON_DRM_BLOCK("unsupport zorder %d\n", __LINE__);
				return -1;
			}
			delta_zorder[1] = delta_zorder[0];
			/*find the max zorder as dout port zorder*/
			if (mvps->dout_zorder[OSD_LEND_OUT_PORT0] <
				mvps->plane_info[n].zorder)
				mvps->dout_zorder[OSD_LEND_OUT_PORT0] =
					mvps->plane_info[n].zorder;
		}
		delta_zorder[2] = delta_zorder[0];
		delta_zorder_flag = ((delta_zorder[2] < 0) !=
			(delta_zorder[3] < 0));
		if (num_plane_port1 >= 2 && i > 0 && delta_zorder_flag) {
			MESON_DRM_BLOCK("unsupport zorder %d\n", __LINE__);
			return -1;
		}
		delta_zorder[3] = delta_zorder[2];
		if (mvps->dout_zorder[OSD_LEND_OUT_PORT1] <
			mvps->plane_info[m].zorder)
			mvps->dout_zorder[OSD_LEND_OUT_PORT1] =
				mvps->plane_info[m].zorder;
	}
	/*
	 *confirm the Din enable and channel mux and sub blend input size
	 *according to input zorder and dout sel
	 */
	mvobs->input_mask = 0;
	MESON_DRM_BLOCK("num_plane_port0=%d\n", num_plane_port0);
	for (i = 0; i < num_plane_port0; i++) {
		mvobs->input_mask |= 1 << i;
		j = plane_index_port0[i];
		mvobs->din_channel_mux[i] = osd2channel(j);
		zorder[i] = mvps->plane_info[j].zorder;
		/*blend size calc*/
		if (max_width < mvps->osd_scope_pre[j].h_end + 1)
			max_width = mvps->osd_scope_pre[j].h_end + 1;
		if (max_height < mvps->osd_scope_pre[j].v_end + 1)
			max_height = mvps->osd_scope_pre[j].v_end + 1;
	}
	for (i = 0; i < num_plane_port0; i++) {
		for (j = (1 + i); j < num_plane_port0; j++) {
			if (zorder[i] > zorder[j]) {
				swap(zorder[i], zorder[j]);
				swap(mvobs->din_channel_mux[i],
				     mvobs->din_channel_mux[j]);
			}
		}
	}
	MESON_DRM_BLOCK("num_plane_port1=%d\n", num_plane_port1);
	for (i = 0; i < num_plane_port1; i++) {
		m = MAX_DIN_NUM - i - 1;
		mvobs->input_mask |= 1 << m;
		j = plane_index_port1[i];
		mvobs->din_channel_mux[m] = osd2channel(j);
		zorder[i] = mvps->plane_info[j].zorder;
		/*blend size calc*/
		if (max_width < mvps->osd_scope_pre[j].h_end + 1)
			max_width = mvps->osd_scope_pre[j].h_end + 1;
		if (max_height < mvps->osd_scope_pre[j].v_end + 1)
			max_height = mvps->osd_scope_pre[j].v_end + 1;
	}
	for (i = 0; i < num_plane_port1; i++) {
		for (j = (1 + i); j < num_plane_port1; j++) {
			if (zorder[i] > zorder[j]) {
				swap(zorder[i], zorder[j]);
				swap(mvobs->din_channel_mux[i],
				     mvobs->din_channel_mux[j]);
			}
		}
	}
	for (i = 0; i < MAX_DIN_NUM; i++) {
		MESON_DRM_BLOCK("mvobs->din_channel_mux[%d]=%d\n",
			  i, mvobs->din_channel_mux[i]);
		if (!mvobs->din_channel_mux[i])
			mvobs->din_channel_mux[i] = OSD_CHANNEL_NUM;
	}
	/*osdblend switch check*/
	if (mvps->plane_info[0].blend_bypass)
		mvobs->din0_switch = 1;
	else
		mvobs->din0_switch = 0;
	if ((mvobs->input_mask & (BIT(DIN2) | BIT(DIN3))) &&
	    num_plane_port0 == 3 && num_plane_port1 == 1)
		mvobs->din3_switch = 1;
	else
		mvobs->din3_switch = 0;
	if ((mvobs->input_mask & BIT(DIN2)) &&
	    num_plane_port1 == 2)
		mvobs->blend1_switch = 1;
	else
		mvobs->blend1_switch = 0;
	/*scope check*/
	for (i = 0; i < MAX_DIN_NUM; i++) {
		if (mvobs->input_osd_mask & BIT(i))
			memcpy(&mvobs->din_channel_scope[i],
			       &mvps->osd_scope_pre[i],
				sizeof(struct osd_scope_s));
		else
			memcpy(&mvobs->din_channel_scope[i],
			       &scope_default,
				sizeof(struct osd_scope_s));
	}
	/*sub blend size check*/
	mvobs->input_width[OSD_SUB_BLEND0] = max_width;
	mvobs->input_width[OSD_SUB_BLEND1] = max_width;
	mvobs->input_height[OSD_SUB_BLEND0] = max_height;
	mvobs->input_height[OSD_SUB_BLEND1] = max_height;
	MESON_DRM_BLOCK("%s check done.\n", vblk->name);
	return 0;
}

static int s5_osdblend_check_state(struct meson_vpu_block *vblk,
				struct meson_vpu_block_state *state,
		struct meson_vpu_pipeline_state *mvps)
{
	return 0;
}

static void osdblend_set_state(struct meson_vpu_block *vblk,
			       struct meson_vpu_block_state *state,
			       struct meson_vpu_block_state *old_state)
{
	struct meson_vpu_osdblend *osdblend = to_osdblend_block(vblk);
	struct meson_vpu_pipeline *pipeline = osdblend->base.pipeline;
	struct meson_vpu_osdblend_state *mvobs;
	struct meson_vpu_pipeline_state *pipeline_state;
	struct osdblend_reg_s *reg = osdblend->reg;

	MESON_DRM_BLOCK("%s set_state called.\n", osdblend->base.name);
	mvobs = to_osdblend_state(state);
	pipeline_state = priv_to_pipeline_state(pipeline->obj.state);
	if (!pipeline_state) {
		MESON_DRM_BLOCK("pipeline_state is NULL!!\n");
		return;
	}

	#ifdef OSDBLEND_CHECK_METHOD_COMBINATION
	osdblend_layer_set(vblk, state->sub->reg_ops,
			   reg, osdblend, pipeline_state);
	#else
	osdblend_hw_update(vblk, state->sub->reg_ops, reg, mvobs);
	#endif
	/*osd dv core size same with blend0 size*/
	if (vblk->pipeline->osd_version >= OSD_V1)
		osd_dv_core_size_set(mvobs->input_width[OSD_SUB_BLEND0],
				     mvobs->input_height[OSD_SUB_BLEND0]);

	MESON_DRM_BLOCK("%s set_state done.\n", osdblend->base.name);
}

static void s5_osdblend_set_state(struct meson_vpu_block *vblk,
			       struct meson_vpu_block_state *state,
				   struct meson_vpu_block_state *old_state)
{
	int i;
	u32 max_height = 0, max_width = 0;
	struct meson_vpu_osdblend_state *mvobs;
	struct meson_vpu_pipeline_state *mvps;
	struct meson_vpu_sub_pipeline_state *mvsps;
	struct meson_vpu_osdblend *osdblend = to_osdblend_block(vblk);
	struct meson_vpu_pipeline *pipeline = osdblend->base.pipeline;
	struct osdblend_reg_s *reg = osdblend->reg;
	struct rdma_reg_ops *reg_ops = state->sub->reg_ops;
	struct osd_scope_s scope_default = {0xffff, 0xffff, 0xffff, 0xffff};

	MESON_DRM_BLOCK("%s set_state called.\n", osdblend->base.name);
	mvobs = to_osdblend_state(state);
	mvps = priv_to_pipeline_state(pipeline->obj.state);
	if (!mvps) {
		MESON_DRM_BLOCK("pipeline_state is NULL!!\n");
		return;
	}

	mvsps = &mvps->sub_states[0];

	for (i = 0; i < MAX_DIN_NUM; i++) {
		memcpy(&mvobs->din_channel_scope[i], &scope_default,
				sizeof(struct osd_scope_s));
	}

	if (mvps->plane_info[0].blend_bypass)
		mvobs->din0_switch = 1;
	else
		mvobs->din0_switch = 0;

	if (mvsps->more_60) {
		mvobs->din3_switch = 0;
		mvobs->blend1_switch = 0;
		mvobs->din_channel_mux[0] = OSD_CHANNEL1;
		mvobs->din_channel_mux[1] = OSD_CHANNEL_NUM;
		mvobs->din_channel_mux[2] = OSD_CHANNEL_NUM;
		mvobs->din_channel_mux[3] = OSD_CHANNEL_NUM;
		mvobs->input_mask |= 1 << DIN0;
		memcpy(&mvobs->din_channel_scope[0],
			       &mvps->osd_scope_pre[0],
				sizeof(struct osd_scope_s));
	} else {
		mvobs->din3_switch = 0;
		mvobs->blend1_switch = 0;
		mvobs->din_channel_mux[1] = OSD_CHANNEL_NUM;
		mvobs->din_channel_mux[2] = OSD_CHANNEL_NUM;

		if (mvps->plane_info[0].enable) {
			mvobs->din_channel_mux[DIN0] = OSD_CHANNEL2;
			mvobs->input_mask |= 1 << DIN0;
			memcpy(&mvobs->din_channel_scope[DIN1],
				   &mvps->osd_scope_pre[0],
				   sizeof(struct osd_scope_s));
		}

		if (mvps->plane_info[2].enable) {
			if (mvps->plane_info[2].zorder < mvps->plane_info[0].zorder) {
				mvobs->din_channel_mux[DIN0] = OSD_CHANNEL4;
				mvobs->din_channel_mux[DIN3] = OSD_CHANNEL2;
			} else {
				mvobs->din_channel_mux[DIN0] = OSD_CHANNEL2;
				mvobs->din_channel_mux[DIN3] = OSD_CHANNEL4;
			}

			mvobs->input_mask |= 1 << DIN3;
			memcpy(&mvobs->din_channel_scope[DIN3],
			       &mvps->osd_scope_pre[2],
				sizeof(struct osd_scope_s));
		}
	}

	for (i = 0; i < MAX_DIN_NUM; i++) {
		MESON_DRM_BLOCK("%s, scope: %u, %u, %u, %u\n", __func__,
			  mvps->osd_scope_pre[i].h_start, mvps->osd_scope_pre[i].h_end,
			  mvps->osd_scope_pre[i].v_start, mvps->osd_scope_pre[i].v_end);

		if (max_width < mvps->osd_scope_pre[i].h_end + 1)
			max_width = mvps->osd_scope_pre[i].h_end + 1;
		if (max_height < mvps->osd_scope_pre[i].v_end + 1)
			max_height = mvps->osd_scope_pre[i].v_end + 1;
	}
	/*sub blend size check*/
	if (mvsps->more_4k)
		align_proc = 2;
	else
		align_proc = 4;

	if (align_proc == 4) {
		mvobs->input_width[OSD_SUB_BLEND0] = ALIGN(max_width, 4);
		mvobs->input_width[OSD_SUB_BLEND1] = ALIGN(max_width, 4);
	} else if (align_proc == 2) {
		mvobs->input_width[OSD_SUB_BLEND0] = ALIGN(max_width, 2);
		mvobs->input_width[OSD_SUB_BLEND1] = ALIGN(max_width, 2);
	} else {
		mvobs->input_width[OSD_SUB_BLEND0] = max_width;
		mvobs->input_width[OSD_SUB_BLEND1] = max_width;
	}
	mvobs->input_height[OSD_SUB_BLEND0] = max_height;
	mvobs->input_height[OSD_SUB_BLEND1] = max_height;

	/*blend dout size */
	if (mvsps->more_4k) {
		mvsps->blend_dout_hsize[OSD_SUB_BLEND0] = BLEND_DOUT_DEF_HSIZE;
		mvsps->blend_dout_vsize[OSD_SUB_BLEND0] = BLEND_DOUT_DEF_VSIZE;
		mvsps->blend_dout_hsize[OSD_SUB_BLEND1] = BLEND_DOUT_DEF_HSIZE;
		mvsps->blend_dout_vsize[OSD_SUB_BLEND1] = BLEND_DOUT_DEF_VSIZE;
		mvobs->input_width[OSD_SUB_BLEND0] = BLEND_DOUT_DEF_HSIZE;
		mvobs->input_height[OSD_SUB_BLEND0] = BLEND_DOUT_DEF_VSIZE;
	} else {
		mvsps->blend_dout_hsize[OSD_SUB_BLEND0] = ALIGN(max_width, 2);
		mvsps->blend_dout_vsize[OSD_SUB_BLEND0] = max_height;
		mvsps->blend_dout_hsize[OSD_SUB_BLEND1] = ALIGN(max_width, 2);
		mvsps->blend_dout_vsize[OSD_SUB_BLEND1] = max_height;
	}

	osdblend_hw_update(vblk, state->sub->reg_ops, reg, mvobs);

	meson_vpu_write_reg(OSD_BLEND_DOUT0_SIZE,
			    (mvsps->blend_dout_vsize[OSD_SUB_BLEND0] << 16) |
			    mvsps->blend_dout_hsize[OSD_SUB_BLEND0]);

	if (mvps->plane_info[0].enable) {
		reg_ops->rdma_write_reg(OSD1_PROC_IN_SIZE,
				    (mvsps->scaler_din_vsize[0] << 16) |
				    mvsps->scaler_din_hsize[0]);
		reg_ops->rdma_write_reg(OSD1_PROC_OUT_SIZE,
				    (mvsps->scaler_dout_vsize[0] << 16) |
				    mvsps->scaler_dout_hsize[0]);
	}

	if (mvps->plane_info[2].enable) {
		reg_ops->rdma_write_reg(OSD3_PROC_IN_SIZE,
				    (mvsps->scaler_din_vsize[2] << 16) |
				    mvsps->scaler_din_hsize[2]);
		reg_ops->rdma_write_reg(OSD3_PROC_OUT_SIZE,
				    (mvsps->scaler_dout_vsize[2] << 16) |
				    mvsps->scaler_dout_hsize[2]);
	}

	if (mvsps->more_4k)
		reg_ops->rdma_write_reg_bits(OSD_PI_BYPASS_EN, 0, 0, 1);
	else
		reg_ops->rdma_write_reg_bits(OSD_PI_BYPASS_EN, 1, 0, 1);

	//osd 1
	if (mvsps->more_60) {
		reg_ops->rdma_write_reg_bits(OSD_PROC_1MUX3_SEL, 1, 0, 2);
		reg_ops->rdma_write_reg_bits(OSD_PROC_1MUX3_SEL, 1, 6, 4);
	} else {
		// bypass slice and go blend
		reg_ops->rdma_write_reg_bits(OSD_PROC_1MUX3_SEL, 2, 0, 2);
	}

	//osd 3
	if (mvsps->more_60) {
		reg_ops->rdma_write_reg_bits(OSD_PROC_1MUX3_SEL, 1, 4, 2);
		reg_ops->rdma_write_reg_bits(OSD_PROC_1MUX3_SEL, 3, 10, 4);
	} else {
		// bypass slice and go blend
		reg_ops->rdma_write_reg_bits(OSD_PROC_1MUX3_SEL, 2, 4, 2);
	}

	reg_ops->rdma_write_reg_bits(OSD_SYS_5MUX4_SEL, 0x1, 0, 20);
	if (mvps->plane_info[0].enable) {
		reg_ops->rdma_write_reg_bits(VIU_OSD1_MISC, 1, 17, 1);
		osd_enable[OSD1_INDEX] = 1;
		osd_dv_core_size_set_s5(mvsps->scaler_din_hsize[0],
					mvsps->scaler_din_vsize[0], OSD1_INDEX);
	} else {
		osd_enable[OSD1_INDEX] = 0;
	}

	if (mvps->plane_info[2].enable) {
		reg_ops->rdma_write_reg_bits(VIU_OSD3_MISC, 1, 17, 1);
		osd_enable[OSD3_INDEX] = 1;
		osd_dv_core_size_set_s5(mvsps->scaler_din_hsize[2],
					mvsps->scaler_din_vsize[2], OSD3_INDEX);
	} else {
		osd_enable[OSD3_INDEX] = 0;
	}

	MESON_DRM_BLOCK("%s set_state done.\n", osdblend->base.name);
}

static void osdblend_hw_enable(struct meson_vpu_block *vblk,
			       struct meson_vpu_block_state *state)
{
	struct meson_vpu_osdblend *osdblend = to_osdblend_block(vblk);

	MESON_DRM_BLOCK("%s enable called.\n", osdblend->base.name);
}

static void osdblend_hw_disable(struct meson_vpu_block *vblk,
				struct meson_vpu_block_state *state)
{
	struct meson_vpu_osdblend *osdblend = to_osdblend_block(vblk);

	MESON_DRM_BLOCK("%s disable called.\n", osdblend->base.name);
}

static void osdblend_dump_register(struct meson_vpu_block *vblk,
				   struct seq_file *seq)
{
	u32 value;
	struct meson_vpu_osdblend *osdblend;
	struct osdblend_reg_s *reg;

	osdblend = to_osdblend_block(vblk);
	reg = osdblend->reg;

	value = meson_drm_read_reg(reg->viu_osd_blend_ctrl);
	seq_printf(seq, "%-35s\t\t0x%08X\n", "VIU_OSD_BLEND_CTRL:",  value);

	value = meson_drm_read_reg(reg->viu_osd_blend_din0_scope_h);
	seq_printf(seq, "%-35s\t\t0x%08X\n", "VIU_OSD_BLEND_DIN0_SCOPE_H:",
		   value);

	value = meson_drm_read_reg(reg->viu_osd_blend_din0_scope_v);
	seq_printf(seq, "%-35s\t\t0x%08X\n", "VIU_OSD_BLEND_DIN0_SCOPE_V:",
		   value);

	value = meson_drm_read_reg(reg->viu_osd_blend_din1_scope_h);
	seq_printf(seq, "%-35s\t\t0x%08X\n", "VIU_OSD_BLEND_DIN1_SCOPE_H:",
		   value);

	value = meson_drm_read_reg(reg->viu_osd_blend_din1_scope_v);
	seq_printf(seq, "%-35s\t\t0x%08X\n", "VIU_OSD_BLEND_DIN1_SCOPE_V:",
		   value);

	value = meson_drm_read_reg(reg->viu_osd_blend_din2_scope_h);
	seq_printf(seq, "%-35s\t\t0x%08X\n", "VIU_OSD_BLEND_DIN2_SCOPE_H:",
		   value);

	value = meson_drm_read_reg(reg->viu_osd_blend_din2_scope_v);
	seq_printf(seq, "%-35s\t\t0x%08X\n", "VIU_OSD_BLEND_DIN2_SCOPE_V:",
		   value);

	value = meson_drm_read_reg(reg->viu_osd_blend_din3_scope_h);
	seq_printf(seq, "%-35s\t\t0x%08X\n", "VIU_OSD_BLEND_DIN3_SCOPE_H:",
		   value);

	value = meson_drm_read_reg(reg->viu_osd_blend_din3_scope_v);
	seq_printf(seq, "%-35s\t\t0x%08X\n", "VIU_OSD_BLEND_DIN3_SCOPE_V:",
		   value);

	value = meson_drm_read_reg(reg->viu_osd_blend_dummy_data0);
	seq_printf(seq, "%-35s\t\t0x%08X\n", "VIU_OSD_BLEND_DUMMY_DATA0:",
		   value);

	value = meson_drm_read_reg(reg->viu_osd_blend_dummy_alpha);
	seq_printf(seq, "%-35s\t\t0x%08X\n", "VIU_OSD_BLEND_DUMMY_ALPHA:",
		   value);

	value = meson_drm_read_reg(reg->viu_osd_blend0_size);
	seq_printf(seq, "%-35s\t\t0x%08X\n", "VIU_OSD_BLEND_BLEND0_SIZE:",
		   value);

	value = meson_drm_read_reg(reg->viu_osd_blend1_size);
	seq_printf(seq, "%-35s\t\t0x%08X\n", "VIU_OSD_BLEND_BLEND1_SIZE:",
		   value);

	value = meson_drm_read_reg(reg->viu_osd_blend_ctrl1);
	seq_printf(seq, "%-35s\t\t0x%08X\n", "VIU_OSD_BLEND_CTRL1:", value);
}

static void osdblend_hw_init(struct meson_vpu_block *vblk)
{
	struct meson_vpu_osdblend *osdblend = to_osdblend_block(vblk);
	struct osd_dummy_data_s dummy_data = {0x80, 0x80, 0x80};
	struct rdma_reg_ops *reg_ops = vblk->pipeline->subs[0].reg_ops;

	osdblend->reg = &osdblend_reg;

	/*dummy data/alpha config*/
	osd_blend_dummy_data_set(vblk, reg_ops, osdblend->reg, dummy_data);
	reg_ops->rdma_write_reg(osdblend->reg->viu_osd_blend_dummy_data0, 0);

	/*reset blend ctrl hold line*/
	reg_ops->rdma_write_reg_bits(osdblend->reg->viu_osd_blend_ctrl, 0, 29, 3);

	MESON_DRM_BLOCK("%s hw_init called.\n", osdblend->base.name);
}

static void s5_osdblend_hw_init(struct meson_vpu_block *vblk)
{
	struct meson_vpu_osdblend *osdblend = to_osdblend_block(vblk);
	struct osd_dummy_data_s dummy_data = {0x80, 0x80, 0x80};
	struct rdma_reg_ops *reg_ops = vblk->pipeline->subs[0].reg_ops;

	osdblend->reg = &osdblend_s5_reg;

	/*dummy data/alpha config*/
	osd_blend_dummy_data_set(vblk, reg_ops, osdblend->reg, dummy_data);
	reg_ops->rdma_write_reg(osdblend->reg->viu_osd_blend_dummy_data0, 0);

	/*reset blend ctrl hold line*/
	reg_ops->rdma_write_reg_bits(osdblend->reg->viu_osd_blend_ctrl, 4, 29, 3);

	register_osd_func(osd_dv_get_osd_status);

	MESON_DRM_BLOCK("%s hw_init called.\n", osdblend->base.name);
}

struct meson_vpu_block_ops osdblend_ops = {
	.check_state = osdblend_check_state,
	.update_state = osdblend_set_state,
	.enable = osdblend_hw_enable,
	.disable = osdblend_hw_disable,
	.dump_register = osdblend_dump_register,
	.init = osdblend_hw_init,
};

struct meson_vpu_block_ops s5_osdblend_ops = {
	.check_state = s5_osdblend_check_state,
	.update_state = s5_osdblend_set_state,
	.enable = osdblend_hw_enable,
	.disable = osdblend_hw_disable,
	.dump_register = osdblend_dump_register,
	.init = s5_osdblend_hw_init,
};
