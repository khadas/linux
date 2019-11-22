/*
 * drivers/amlogic/drm/vpu-hw/meson_vpu_osdblend.c
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

/* Amlogic Headers */
#include <linux/amlogic/media/vout/vout_notify.h>

#include "meson_vpu_pipeline.h"
#include "meson_vpu_reg.h"
#include "meson_vpu_util.h"
#include "meson_vpu_osdblend.h"

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

/*0:din0 go through blend0,1:bypass blend0,dirct to Dout0*/
static void osd_din0_switch_set(struct osdblend_reg_s *reg,
	bool bypass_state)
{
	VSYNCOSD_WR_MPEG_REG_BITS(reg->viu_osd_blend_ctrl,
		bypass_state, 26, 1);
}
/*0:blend1 out to blend2,1:blend1 out to Dout1*/
static void osd_blend1_dout_switch_set(struct osdblend_reg_s *reg,
	bool bypass_state)
{
	VSYNCOSD_WR_MPEG_REG_BITS(reg->viu_osd_blend_ctrl,
		bypass_state, 25, 1);
}
/*0:din3 pass through blend1,1:bypass blend1,direct to Dout1*/
static void osd_din3_switch_set(struct osdblend_reg_s *reg,
	bool bypass_state)
{
	VSYNCOSD_WR_MPEG_REG_BITS(reg->viu_osd_blend_ctrl,
		bypass_state, 24, 1);
}
#if 0
/*0:din0/1/2/3 input disable,1:enable*/
static void osd_din_input_enable_set(struct osdblend_reg_s *reg,
	bool enable, enum din_channel_e din_channel)
{
	VSYNCOSD_WR_MPEG_REG_BITS(reg->viu_osd_blend_ctrl, enable,
		(20 + din_channel), 1);
}
#endif
/*0:din0 input disable,1:din0 input enable*/
static void osd_din0_input_enable_set(struct osdblend_reg_s *reg,
	bool input_enable)
{
	VSYNCOSD_WR_MPEG_REG_BITS(reg->viu_osd_blend_ctrl,
		input_enable, 20, 1);
}
/*0:din1 input disable,1:din1 input enable*/
static void osd_din1_input_enable_set(struct osdblend_reg_s *reg,
	bool input_enable)
{
	VSYNCOSD_WR_MPEG_REG_BITS(reg->viu_osd_blend_ctrl,
		input_enable, 21, 1);
}
/*0:din2 input disable,1:din2 input enable*/
static void osd_din2_input_enable_set(struct osdblend_reg_s *reg,
	bool input_enable)
{
	VSYNCOSD_WR_MPEG_REG_BITS(reg->viu_osd_blend_ctrl,
		input_enable, 22, 1);
}
/*0:din3 input disable,1:din3 input enable*/
static void osd_din3_input_enable_set(struct osdblend_reg_s *reg,
	bool input_enable)
{
	VSYNCOSD_WR_MPEG_REG_BITS(reg->viu_osd_blend_ctrl,
		input_enable, 23, 1);
}
#if 0
/*0:din0/1/2/3 premult disable,1:enable*/
static void osd_din_premult_enable_set(struct osdblend_reg_s *reg,
	bool enable, enum din_channel_e din_channel)
{
	VSYNCOSD_WR_MPEG_REG_BITS(reg->viu_osd_blend_ctrl, enable,
		(16 + din_channel), 1);
}
#endif
/*1/2/3:din0/1/2/3 select osd1/osd2/osd3,else select null*/
static void osd_din_channel_mux_set(struct osdblend_reg_s *reg,
	enum osd_channel_e osd_channel,
	enum din_channel_e din_channel)
{
	VSYNCOSD_WR_MPEG_REG_BITS(reg->viu_osd_blend_ctrl, osd_channel,
		(0 + din_channel*4), 4);
}
/*din0 scope config*/
static void osd_din0_scope_set(struct osdblend_reg_s *reg,
	struct osd_scope_s scope)
{
	VSYNCOSD_WR_MPEG_REG_BITS(reg->viu_osd_blend_din0_scope_h,
		scope.h_start, 0, 13);
	VSYNCOSD_WR_MPEG_REG_BITS(reg->viu_osd_blend_din0_scope_h,
		scope.h_end, 16, 13);
	VSYNCOSD_WR_MPEG_REG_BITS(reg->viu_osd_blend_din0_scope_v,
		scope.v_start, 0, 13);
	VSYNCOSD_WR_MPEG_REG_BITS(reg->viu_osd_blend_din0_scope_v,
		scope.v_end, 16, 13);
}
/*din1 scope config*/
static void osd_din1_scope_set(struct osdblend_reg_s *reg,
	struct osd_scope_s scope)
{
	VSYNCOSD_WR_MPEG_REG_BITS(reg->viu_osd_blend_din1_scope_h,
		scope.h_start, 0, 13);
	VSYNCOSD_WR_MPEG_REG_BITS(reg->viu_osd_blend_din1_scope_h,
		scope.h_end, 16, 13);
	VSYNCOSD_WR_MPEG_REG_BITS(reg->viu_osd_blend_din1_scope_v,
		scope.v_start, 0, 13);
	VSYNCOSD_WR_MPEG_REG_BITS(reg->viu_osd_blend_din1_scope_v,
		scope.v_end, 16, 13);
}
/*din2 scope config*/
static void osd_din2_scope_set(struct osdblend_reg_s *reg,
	struct osd_scope_s scope)
{
	VSYNCOSD_WR_MPEG_REG_BITS(reg->viu_osd_blend_din2_scope_h,
		scope.h_start, 0, 13);
	VSYNCOSD_WR_MPEG_REG_BITS(reg->viu_osd_blend_din2_scope_h,
		scope.h_end, 16, 13);
	VSYNCOSD_WR_MPEG_REG_BITS(reg->viu_osd_blend_din2_scope_v,
		scope.v_start, 0, 13);
	VSYNCOSD_WR_MPEG_REG_BITS(reg->viu_osd_blend_din2_scope_v,
		scope.v_end, 16, 13);
}
/*din3 scope config*/
static void osd_din3_scope_set(struct osdblend_reg_s *reg,
	struct osd_scope_s scope)
{
	VSYNCOSD_WR_MPEG_REG_BITS(reg->viu_osd_blend_din3_scope_h,
		scope.h_start, 0, 13);
	VSYNCOSD_WR_MPEG_REG_BITS(reg->viu_osd_blend_din3_scope_h,
		scope.h_end, 16, 13);
	VSYNCOSD_WR_MPEG_REG_BITS(reg->viu_osd_blend_din3_scope_v,
		scope.v_start, 0, 13);
	VSYNCOSD_WR_MPEG_REG_BITS(reg->viu_osd_blend_din3_scope_v,
		scope.v_end, 16, 13);
}
/*osd blend dummy data config*/
static void osd_blend_dummy_data_set(struct osdblend_reg_s *reg,
	struct osd_dummy_data_s dummy_data)
{
	VSYNCOSD_WR_MPEG_REG(reg->viu_osd_blend_dummy_data0,
		((dummy_data.channel0 & 0xff) << 16) |
		((dummy_data.channel1 & 0xff) << 8) |
		(dummy_data.channel2 & 0xff));
}
/*osd blend0 dummy data alpha config*/
static void osd_blend0_dummy_alpha_set(struct osdblend_reg_s *reg,
	unsigned int dummy_alpha)
{
	VSYNCOSD_WR_MPEG_REG_BITS(reg->viu_osd_blend_dummy_alpha,
		dummy_alpha, 20, 9);
}
/*osd blend1 dummy data alpha config*/
static void osd_blend1_dummy_alpha_set(struct osdblend_reg_s *reg,
	unsigned int dummy_alpha)
{
	VSYNCOSD_WR_MPEG_REG_BITS(reg->viu_osd_blend_dummy_alpha,
		dummy_alpha, 11, 9);
}
/*osd blend2 dummy data alpha config*/
static void osd_blend2_dummy_alpha_set(struct osdblend_reg_s *reg,
	unsigned int dummy_alpha)
{
	VSYNCOSD_WR_MPEG_REG_BITS(reg->viu_osd_blend_dummy_alpha,
		dummy_alpha, 0, 9);
}
/*osd blend0 size config*/
static void osd_blend0_size_set(struct osdblend_reg_s *reg,
	unsigned int h_size, unsigned int v_size)
{
	VSYNCOSD_WR_MPEG_REG(reg->viu_osd_blend0_size,
		(v_size << 16) | h_size);
}
/*osd blend1 size config*/
static void osd_blend1_size_set(struct osdblend_reg_s *reg,
	unsigned int h_size, unsigned int v_size)
{
	VSYNCOSD_WR_MPEG_REG(reg->viu_osd_blend1_size,
		(v_size << 16) | h_size);
}
/*osd blend0 & blend1 4 din inputs premult flag config as 0 default*/
void osd_blend01_premult_config(struct osdblend_reg_s *reg)
{
	VSYNCOSD_WR_MPEG_REG_BITS(reg->viu_osd_blend_ctrl, 0, 16, 4);
}
/*osd blend2 2 inputs premult flag config as 1 default*/
void osd_blend2_premult_config(struct osdblend_reg_s *reg)
{
	VSYNCOSD_WR_MPEG_REG_BITS(reg->viu_osd_blend_ctrl, 3, 27, 2);
}
/*osd blend dout0 output div en config as 1,alpha 9bit default*/
void osd_blend_dout0_div_config(struct osdblend_reg_s *reg)
{
	VSYNCOSD_WR_MPEG_REG_BITS(reg->viu_osd_blend_ctrl1, 3, 4, 2);
	VSYNCOSD_WR_MPEG_REG_BITS(reg->viu_osd_blend_ctrl1, 1, 0, 1);
}
/*osd blend dout1 output div en config as 1,alpha 9bit default*/
void osd_blend_dout1_div_config(struct osdblend_reg_s *reg)
{
	VSYNCOSD_WR_MPEG_REG_BITS(reg->viu_osd_blend_ctrl1, 3, 16, 2);
	VSYNCOSD_WR_MPEG_REG_BITS(reg->viu_osd_blend_ctrl1, 1, 12, 1);
}
/*osd blend premult config*/
void osdblend_premult_config(struct osdblend_reg_s *reg)
{
	osd_blend01_premult_config(reg);
	osd_blend2_premult_config(reg);
	osd_blend_dout0_div_config(reg);
	osd_blend_dout1_div_config(reg);
}

enum osd_channel_e osd2channel(u8 osd_index)
{
	u8 din_channel_seq[MAX_DIN_NUM] = {OSD_CHANNEL1, OSD_CHANNEL2,
		OSD_CHANNEL3, OSD_CHANNEL_NUM};

	if (osd_index >= MAX_DIN_NUM) {
		DRM_DEBUG("osd_index:%d overflow!!.\n", osd_index);
		return OSD_CHANNEL_NUM;
	}
	return din_channel_seq[osd_index];
}

static void osdblend_hw_update(struct osdblend_reg_s *reg,
			struct meson_vpu_osdblend_state *mvobs)
{
	struct osd_dummy_data_s dummy_data = {0, 0, 0};

	/*din channel mux config*/
	osd_din_channel_mux_set(reg, mvobs->din_channel_mux[DIN0], DIN0);
	osd_din_channel_mux_set(reg, mvobs->din_channel_mux[DIN1], DIN1);
	osd_din_channel_mux_set(reg, mvobs->din_channel_mux[DIN2], DIN2);
	osd_din_channel_mux_set(reg, mvobs->din_channel_mux[DIN3], DIN3);

	/*dummy data config*/
	osd_blend_dummy_data_set(reg, dummy_data);

	/*alpha config*/
	osd_blend0_dummy_alpha_set(reg, 0x1ff);
	osd_blend1_dummy_alpha_set(reg, 0);
	osd_blend2_dummy_alpha_set(reg, 0x1ff);

	/*internal channel disable default*/
	osd_din0_input_enable_set(reg, (mvobs->input_mask >> DIN0) & 0x1);
	osd_din1_input_enable_set(reg, (mvobs->input_mask >> DIN1) & 0x1);
	osd_din2_input_enable_set(reg, (mvobs->input_mask >> DIN2) & 0x1);
	osd_din3_input_enable_set(reg, (mvobs->input_mask >> DIN3) & 0x1);

	/*blend switch config*/
	osd_din0_switch_set(reg, mvobs->din0_switch);
	osd_din3_switch_set(reg, mvobs->din3_switch);
	osd_blend1_dout_switch_set(reg, mvobs->blend1_switch);

	/*scope config*/
	osd_din0_scope_set(reg, mvobs->din_channel_scope[DIN0]);
	osd_din1_scope_set(reg, mvobs->din_channel_scope[DIN1]);
	osd_din2_scope_set(reg, mvobs->din_channel_scope[DIN2]);
	osd_din3_scope_set(reg, mvobs->din_channel_scope[DIN3]);

	/*premult config*/
	osdblend_premult_config(reg);

	/*blend0/blend1 size config*/
	osd_blend0_size_set(reg, mvobs->input_width[OSD_SUB_BLEND0],
		mvobs->input_height[OSD_SUB_BLEND0]);
	osd_blend1_size_set(reg, mvobs->input_width[OSD_SUB_BLEND1],
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
	DRM_DEBUG("%s check_state called.\n", vblk->name);
	for (i = 0; i < MESON_MAX_OSDS; i++) {
		if (!mvps->plane_info[i].enable) {
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
		num_plane_port1 > OSD_LEND_MAX_IN_NUM_PORT1)
		return -1;
	if (mvps->pipeline->osd_version <= OSD_V2 &&
		num_plane_port1)
		return -1;
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
				delta_zorder_flag)
				return -1;
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
		if (num_plane_port1 >= 2 && i > 0 && delta_zorder_flag)
			return -1;
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
		for (j = 1; j < num_plane_port0; j++) {
			if (zorder[i] > zorder[j]) {
				swap(zorder[i], zorder[j]);
				swap(mvobs->din_channel_mux[i],
					mvobs->din_channel_mux[j]);
			}
		}
	}
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
		for (j = 1; j < num_plane_port1; j++) {
			if (zorder[i] > zorder[j]) {
				swap(zorder[i], zorder[j]);
				swap(mvobs->din_channel_mux[i],
					mvobs->din_channel_mux[j]);
			}
		}
	}
	for (i = 0; i < MAX_DIN_NUM; i++) {
		if (!mvobs->din_channel_mux[i])
			mvobs->din_channel_mux[i] = OSD_CHANNEL_NUM;
	}
	/*osdblend switch check*/
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
	DRM_DEBUG("%s check done.\n", vblk->name);
	return ret;
}

static void osdblend_set_state(struct meson_vpu_block *vblk,
		struct meson_vpu_block_state *state)
{
	struct meson_vpu_osdblend *osdblend = to_osdblend_block(vblk);
	struct meson_vpu_pipeline *pipeline = osdblend->base.pipeline;
	struct meson_vpu_osdblend_state *mvobs;
	struct meson_vpu_pipeline_state *pipeline_state;
	struct osdblend_reg_s *reg = osdblend->reg;

	DRM_DEBUG("%s set_state called.\n", osdblend->base.name);
	mvobs = to_osdblend_state(state);
	pipeline_state = priv_to_pipeline_state(pipeline->obj.state);
	if (!pipeline_state) {
		DRM_DEBUG("pipeline_state is NULL!!\n");
		return;
	}

	#ifdef OSDBLEND_CHECK_METHOD_COMBINATION
	osdblend_layer_set(reg, osdblend, pipeline_state);
	#else
	osdblend_hw_update(reg, mvobs);
	#endif

	DRM_DEBUG("%s set_state done.\n", osdblend->base.name);
}

static void osdblend_hw_enable(struct meson_vpu_block *vblk)
{
	struct meson_vpu_osdblend *osdblend = to_osdblend_block(vblk);

	DRM_DEBUG("%s enable called.\n", osdblend->base.name);
}

static void osdblend_hw_disable(struct meson_vpu_block *vblk)
{
	struct meson_vpu_osdblend *osdblend = to_osdblend_block(vblk);

	DRM_DEBUG("%s disable called.\n", osdblend->base.name);
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

	osdblend->reg = &osdblend_reg;
	DRM_DEBUG("%s hw_init called.\n", osdblend->base.name);
}

struct meson_vpu_block_ops osdblend_ops = {
	.check_state = osdblend_check_state,
	.update_state = osdblend_set_state,
	.enable = osdblend_hw_enable,
	.disable = osdblend_hw_disable,
	.dump_register = osdblend_dump_register,
	.init = osdblend_hw_init,
};

