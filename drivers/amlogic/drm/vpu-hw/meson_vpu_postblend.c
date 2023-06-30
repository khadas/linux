// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

/* Amlogic Headers */
#include <linux/amlogic/media/vout/vout_notify.h>

#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT
#include <linux/amlogic/media/amvecm/amvecm.h>
#endif

#include "meson_crtc.h"
#include "meson_vpu_pipeline.h"
#include "meson_vpu_util.h"
#include "meson_vpu_postblend.h"

static struct postblend_reg_s postblend_reg = {
	VPP_OSD1_BLD_H_SCOPE,
	VPP_OSD1_BLD_V_SCOPE,
	VPP_OSD2_BLD_H_SCOPE,
	VPP_OSD2_BLD_V_SCOPE,
	VD1_BLEND_SRC_CTRL,
	VD2_BLEND_SRC_CTRL,
	OSD1_BLEND_SRC_CTRL,
	OSD2_BLEND_SRC_CTRL,
	VPP_OSD1_IN_SIZE,
};

static struct postblend1_reg_s postblend1_reg[3] = {
	{},
	{
		VPP1_BLD_DIN0_HSCOPE,
		VPP1_BLD_DIN0_VSCOPE,
		VPP1_BLD_DIN1_HSCOPE,
		VPP1_BLD_DIN1_VSCOPE,
		VPP1_BLD_DIN2_HSCOPE,
		VPP1_BLD_DIN2_VSCOPE,
		VPP1_BLD_CTRL,
		VPP1_BLD_OUT_SIZE,
		VPP1_BLEND_BLEND_DUMMY_DATA,
		VPP1_BLEND_DUMMY_ALPHA,
	},
	{
		VPP2_BLD_DIN0_HSCOPE,
		VPP2_BLD_DIN0_VSCOPE,
		VPP2_BLD_DIN1_HSCOPE,
		VPP2_BLD_DIN1_VSCOPE,
		VPP2_BLD_DIN2_HSCOPE,
		VPP2_BLD_DIN2_VSCOPE,
		VPP2_BLD_CTRL,
		VPP2_BLD_OUT_SIZE,
		VPP2_BLEND_BLEND_DUMMY_DATA,
		VPP2_BLEND_DUMMY_ALPHA,
	},
};

static struct postblend_reg_s s5_postblend_reg = {
	VPP_OSD1_BLD_H_SCOPE_S5,
	VPP_OSD1_BLD_V_SCOPE_S5,
	VPP_OSD2_BLD_H_SCOPE_S5,
	VPP_OSD2_BLD_V_SCOPE_S5,
	VD1_BLEND_SRC_CTRL_S5,
	VD2_BLEND_SRC_CTRL_S5,
	OSD1_BLEND_SRC_CTRL_S5,
	OSD2_BLEND_SRC_CTRL_S5,
	VPP_OSD1_IN_SIZE,
};

/*vpp post&post blend for osd1 premult flag config as 0 default*/
static void osd1_blend_premult_set(struct meson_vpu_block *vblk,
				   struct rdma_reg_ops *reg_ops,
				   struct postblend_reg_s *reg)
{
	reg_ops->rdma_write_reg_bits(reg->osd1_blend_src_ctrl, 0, 4, 1);
	reg_ops->rdma_write_reg_bits(reg->osd1_blend_src_ctrl, 0, 16, 1);
}

/*vpp osd1 blend sel*/
static void osd1_blend_switch_set(struct meson_vpu_block *vblk,
				  struct rdma_reg_ops *reg_ops,
				  struct postblend_reg_s *reg,
				  enum vpp_blend_e blend_sel)
{
	reg_ops->rdma_write_reg_bits(reg->osd1_blend_src_ctrl, blend_sel, 20, 1);
}

/*vpp osd2 blend sel*/
static void osd2_blend_switch_set(struct meson_vpu_block *vblk,
				  struct rdma_reg_ops *reg_ops,
				  struct postblend_reg_s *reg,
				  enum vpp_blend_e blend_sel)
{
	reg_ops->rdma_write_reg_bits(reg->osd2_blend_src_ctrl, blend_sel, 20, 1);
}

/*vpp osd1 preblend mux sel*/
static void vpp_osd1_preblend_mux_set(struct meson_vpu_block *vblk,
				      struct rdma_reg_ops *reg_ops,
				      struct postblend_reg_s *reg,
				      enum vpp_blend_src_e src_sel)
{
	reg_ops->rdma_write_reg_bits(reg->osd1_blend_src_ctrl, src_sel, 0, 4);
}

/*vpp osd2 preblend mux sel*/
static void vpp_osd2_preblend_mux_set(struct meson_vpu_block *vblk,
				      struct rdma_reg_ops *reg_ops,
				      struct postblend_reg_s *reg,
				      enum vpp_blend_src_e src_sel)
{
	reg_ops->rdma_write_reg_bits(reg->osd2_blend_src_ctrl, src_sel, 0, 4);
}

/*vpp osd1 postblend mux sel*/
static void vpp_osd1_postblend_mux_set(struct meson_vpu_block *vblk,
				       struct rdma_reg_ops *reg_ops,
				       struct postblend_reg_s *reg,
				       enum vpp_blend_src_e src_sel)
{
	reg_ops->rdma_write_reg_bits(reg->osd1_blend_src_ctrl, src_sel, 8, 4);
}

static void vpp_osd1_postblend_5mux_set(struct meson_vpu_block *vblk,
				       struct rdma_reg_ops *reg_ops,
					   struct postblend_reg_s *reg,
				       enum vpp_blend_src_e src_sel)
{
	reg_ops->rdma_write_reg_bits(reg->osd1_blend_src_ctrl, src_sel, 0, 4);
}

/*vpp osd2 postblend mux sel*/
static void vpp_osd2_postblend_mux_set(struct meson_vpu_block *vblk,
				       struct rdma_reg_ops *reg_ops,
				       struct postblend_reg_s *reg,
				       enum vpp_blend_src_e src_sel)
{
	reg_ops->rdma_write_reg_bits(reg->osd2_blend_src_ctrl, src_sel, 8, 4);
}

static void vpp1_osd1_postblend_mux_set(struct meson_vpu_block *vblk,
					struct rdma_reg_ops *reg_ops,
					struct postblend1_reg_s *reg,
					enum vpp_blend_src_e src_sel)
{
	reg_ops->rdma_write_reg_bits(reg->vpp_bld_ctrl, src_sel, 4, 4);
}

/*vpp osd1 blend scope set*/
static void vpp_osd1_blend_scope_set(struct meson_vpu_block *vblk,
				     struct rdma_reg_ops *reg_ops,
				     struct postblend_reg_s *reg,
				     struct osd_scope_s scope)
{
	reg_ops->rdma_write_reg(reg->vpp_osd1_bld_h_scope,
				(scope.h_start << 16) | scope.h_end);
	reg_ops->rdma_write_reg(reg->vpp_osd1_bld_v_scope,
				(scope.v_start << 16) | scope.v_end);
}

static void vpp1_osd1_blend_scope_set(struct meson_vpu_block *vblk,
				      struct rdma_reg_ops *reg_ops,
				      struct postblend1_reg_s *reg,
				      struct osd_scope_s scope)
{
	reg_ops->rdma_write_reg(reg->vpp_bld_din1_hscope,
				(scope.h_start << 16) | scope.h_end);
	reg_ops->rdma_write_reg(reg->vpp_bld_din1_vscope,
				(scope.v_start << 16) | scope.v_end);
}

static void osd_set_vpp_path_default(struct meson_vpu_block *vblk,
				     struct rdma_reg_ops *reg_ops,
				     u32 osd_index, u32 vpp_index)
{
	/* osd_index is vpp mux input */
	/* default setting osd2 route to vpp0 vsync */
	if (osd_index == 3)
		reg_ops->rdma_write_reg_bits(PATH_START_SEL, vpp_index, 24, 2);
	/* default setting osd3 route to vpp0 vsync */
	if (osd_index == 4)
		reg_ops->rdma_write_reg_bits(PATH_START_SEL, vpp_index, 28, 2);
}

static void vpp_chk_crc(struct meson_vpu_block *vblk,
			struct rdma_reg_ops *reg_ops,
			struct am_meson_crtc *amcrtc)
{
	if (amcrtc->force_crc_chk ||
	    (amcrtc->vpp_crc_enable && cpu_after_eq(MESON_CPU_MAJOR_ID_SM1))) {
		reg_ops->rdma_write_reg(VPP_CRC_CHK, 1);
		amcrtc->force_crc_chk--;
	}
}

static int postblend_check_state(struct meson_vpu_block *vblk,
				 struct meson_vpu_block_state *state,
				 struct meson_vpu_pipeline_state *mvps)
{
	struct meson_vpu_postblend *postblend = to_postblend_block(vblk);
	u32 video_zorder, osd_zorder, top_flag, bottom_flag, i, j;

	if (state->checked)
		return 0;

	state->checked = true;
	for (i = 0; i < MESON_MAX_VIDEO &&
	     mvps->video_plane_info[i].enable; i++) {
		video_zorder = mvps->video_plane_info[i].zorder;
		top_flag = 0;
		bottom_flag = 0;
		for (j = 0; j < MESON_MAX_OSDS &&
		     mvps->plane_info[j].enable; j++) {
			osd_zorder = mvps->plane_info[j].zorder;
			if (video_zorder > osd_zorder)
				top_flag++;
			else
				bottom_flag++;
		}
		if (top_flag && bottom_flag) {
			MESON_DRM_BLOCK("unsupported zorder\n");
			return -1;
		} else if (top_flag) {
			set_video_zorder(video_zorder +
					 VPP_POST_BLEND_REF_ZORDER, i);
			MESON_DRM_BLOCK("video on the top\n");
		} else if (bottom_flag) {
			set_video_zorder(video_zorder, i);
			MESON_DRM_BLOCK("video on the bottom\n");
		}
	}

	MESON_DRM_BLOCK("%s check_state called.\n", postblend->base.name);
	return 0;
}

static void postblend_set_state(struct meson_vpu_block *vblk,
				struct meson_vpu_block_state *state,
				struct meson_vpu_block_state *old_state)
{
	int crtc_index;
	struct am_meson_crtc *amc;
	struct meson_vpu_pipeline_state *mvps;

	struct meson_vpu_postblend *postblend = to_postblend_block(vblk);
	struct osd_scope_s scope = {0, 1919, 0, 1079};
	struct meson_vpu_pipeline *pipeline = postblend->base.pipeline;
	struct postblend_reg_s *reg = postblend->reg;
	struct rdma_reg_ops *reg_ops = state->sub->reg_ops;

	crtc_index = vblk->index;
	amc = vblk->pipeline->priv->crtcs[crtc_index];

	MESON_DRM_BLOCK("%s set_state called.\n", postblend->base.name);
	mvps = priv_to_pipeline_state(pipeline->obj.state);
	scope.h_start = 0;
	scope.h_end = mvps->scaler_param[0].output_width - 1;
	scope.v_start = 0;
	scope.v_end = mvps->scaler_param[0].output_height - 1;

	if (crtc_index == 0) {
		vpp_osd1_blend_scope_set(vblk, reg_ops, reg, scope);

		if (amc->blank_enable) {
			vpp_osd1_postblend_mux_set(vblk, reg_ops,
						   postblend->reg, VPP_NULL);
		} else {
			/*dout switch config*/
			osd1_blend_switch_set(vblk, reg_ops, postblend->reg,
					      VPP_POSTBLEND);
			/*vpp input config*/
			vpp_osd1_preblend_mux_set(vblk, reg_ops,
						  postblend->reg, VPP_NULL);

			vpp_osd1_postblend_mux_set(vblk, reg_ops,
							   postblend->reg,
							   VPP_OSD1);
		}

		vpp_chk_crc(vblk, reg_ops, amc);
		osd1_blend_premult_set(vblk, reg_ops, reg);
	}

	MESON_DRM_BLOCK("scope h/v start/end [%d,%d,%d,%d].\n",
		  scope.h_start, scope.h_end, scope.v_start, scope.v_end);
}

static void t7_postblend_set_state(struct meson_vpu_block *vblk,
				struct meson_vpu_block_state *state,
				struct meson_vpu_block_state *old_state)
{
	int i, crtc_index;
	struct am_meson_crtc *amc;
	struct meson_vpu_pipeline_state *mvps;

	struct meson_vpu_postblend *postblend = to_postblend_block(vblk);
	struct osd_scope_s scope = {0, 1919, 0, 1079};
	struct meson_vpu_pipeline *pipeline = postblend->base.pipeline;
	struct postblend_reg_s *reg = postblend->reg;
	struct rdma_reg_ops *reg_ops = state->sub->reg_ops;

	crtc_index = vblk->index;
	amc = vblk->pipeline->priv->crtcs[crtc_index];

	MESON_DRM_BLOCK("%s set_state called.\n", postblend->base.name);
	mvps = priv_to_pipeline_state(pipeline->obj.state);
	scope.h_start = 0;
	scope.h_end = mvps->scaler_param[0].output_width - 1;
	scope.v_start = 0;
	scope.v_end = mvps->scaler_param[0].output_height - 1;

	if (crtc_index == 0) {
		vpp_osd1_blend_scope_set(vblk, reg_ops, reg, scope);

		if (amc->blank_enable) {
			vpp_osd1_postblend_mux_set(vblk, reg_ops,
						   postblend->reg, VPP_NULL);
		} else {
			/*dout switch config*/
			osd1_blend_switch_set(vblk, reg_ops, postblend->reg,
					      VPP_POSTBLEND);
			/*vpp input config*/
			vpp_osd1_preblend_mux_set(vblk, reg_ops,
						  postblend->reg, VPP_NULL);

			vpp_osd1_postblend_mux_set(vblk, reg_ops,
						   postblend->reg,
						   VPP_OSD2);
		}

		for (i = 0; i < MESON_MAX_OSDS; i++) {
			if (mvps->plane_info[i].enable &&
			    mvps->plane_info[i].crtc_index == crtc_index) {
				osd_set_vpp_path_default(vblk, reg_ops, i + 1,
							 crtc_index);
			}
		}

		vpp_chk_crc(vblk, reg_ops, amc);
		osd1_blend_premult_set(vblk, reg_ops, reg);
	} else {
		/* 1:vd1-din0, 2:osd1-din1 */

		u32 val, vpp1_bld;
		u32 bld_src2_sel = 2;
		u32 scaler_index = 2;
		u32 bld_w, bld_h;
		struct postblend1_reg_s *reg1 = postblend->reg1;

		for (i = 0; i < MESON_MAX_OSDS; i++) {
			if (mvps->plane_info[i].enable &&
			    mvps->plane_info[i].crtc_index == crtc_index) {
				bld_src2_sel = i;
				scaler_index = i;
				break;
			}
		}
		bld_w = mvps->scaler_param[scaler_index].output_width;
		bld_h = mvps->scaler_param[scaler_index].output_height;

		scope.h_start = 0;
		scope.h_end = mvps->scaler_param[scaler_index].output_width - 1;
		scope.v_start = 0;
		scope.v_end = mvps->scaler_param[scaler_index].output_height - 1;

		vpp1_osd1_blend_scope_set(vblk, reg_ops, reg1, scope);
		reg_ops->rdma_write_reg(reg1->vpp_bld_out_size,
					bld_w | (bld_h << 16));

		vpp1_bld = reg_ops->rdma_read_reg(reg1->vpp_bld_ctrl);
		val = vpp1_bld | 2 << 4 | 1 << 31;
		if (vpp1_bld != val)
			reg_ops->rdma_write_reg(reg1->vpp_bld_ctrl, val);

		if (bld_src2_sel == 2) {
			if (postblend->postblend_path_mask) {
				reg_ops->rdma_write_reg_bits(PATH_START_SEL, crtc_index, 24, 2);
				reg_ops->rdma_write_reg_bits(VIU_OSD3_PATH_CTRL, 1, 2, 1);
				reg_ops->rdma_write_reg_bits(VIU_OSD3_PATH_CTRL, 0x1, 4, 1);
			} else {
				reg_ops->rdma_write_reg(VPP_OSD3_SCALE_CTRL, 0x7);
			}
		} else if (bld_src2_sel == 3) {
			reg_ops->rdma_write_reg(VPP_OSD4_SCALE_CTRL, 0x7);
		} else {
			MESON_DRM_BLOCK("invalid src2_sel %d\n", bld_src2_sel);
		}
	}

	MESON_DRM_BLOCK("scope h/v start/end [%d,%d,%d,%d].\n",
		  scope.h_start, scope.h_end, scope.v_start, scope.v_end);
}

static void s5_postblend_set_state(struct meson_vpu_block *vblk,
				struct meson_vpu_block_state *state,
				struct meson_vpu_block_state *old_state)
{
	int crtc_index, vpp_osd1_mux;
	struct am_meson_crtc *amc;
	struct am_meson_crtc_state *meson_crtc_state;
	struct meson_vpu_pipeline_state *mvps;
	struct meson_vpu_sub_pipeline_state *mvsps;

	struct meson_vpu_postblend *postblend = to_postblend_block(vblk);
	struct osd_scope_s scope = {0, 1919, 0, 1079};
	struct meson_vpu_pipeline *pipeline = postblend->base.pipeline;
	struct postblend_reg_s *reg = postblend->reg;
	struct rdma_reg_ops *reg_ops = state->sub->reg_ops;

	crtc_index = vblk->index;
	amc = vblk->pipeline->priv->crtcs[crtc_index];
	meson_crtc_state = to_am_meson_crtc_state(amc->base.state);

	MESON_DRM_BLOCK("%s set_state called.\n", postblend->base.name);
	mvps = priv_to_pipeline_state(pipeline->obj.state);
	mvsps = &mvps->sub_states[0];

	scope.h_start = 0;
	scope.v_start = 0;
	if (mvsps->more_4k) {
		scope.h_end = mvsps->blend_dout_hsize[0] * 2 - 1;
		scope.v_end = mvsps->blend_dout_vsize[0] * 2 - 1;
	} else {
		scope.h_end = mvsps->blend_dout_hsize[0] - 1;
		scope.v_end = mvsps->blend_dout_vsize[0] - 1;
	}

	vpp_osd1_blend_scope_set(vblk, reg_ops, reg, scope);

	if (amc->blank_enable) {
		vpp_osd1_postblend_5mux_set(vblk, reg_ops, reg, VPP_NULL);
	} else {
		vpp_osd1_mux = VPP_5MUX_OSD1;
		vpp_osd1_postblend_5mux_set(vblk, reg_ops, reg, vpp_osd1_mux);
	}

	osd1_blend_premult_set(vblk, reg_ops, reg);
	reg_ops->rdma_write_reg_bits(VPP_POSTBLND_CTRL_S5, 1, 8, 1);

	MESON_DRM_BLOCK("scope h/v start/end [%d,%d,%d,%d].\n",
		  scope.h_start, scope.h_end, scope.v_start, scope.v_end);
}

static void postblend_hw_enable(struct meson_vpu_block *vblk,
				struct meson_vpu_block_state *state)
{
	struct meson_vpu_postblend *postblend = to_postblend_block(vblk);

	MESON_DRM_BLOCK("%s enable called.\n", postblend->base.name);
}

static void postblend_hw_disable(struct meson_vpu_block *vblk,
				 struct meson_vpu_block_state *state)
{
	struct meson_vpu_postblend *postblend = to_postblend_block(vblk);

	if (vblk->index == 0)
		vpp_osd1_postblend_mux_set(vblk, state->sub->reg_ops, postblend->reg, VPP_NULL);

	MESON_DRM_BLOCK("%s disable called.\n", postblend->base.name);
}

static void s5_postblend_hw_disable(struct meson_vpu_block *vblk,
				    struct meson_vpu_block_state *state)
{
	struct meson_vpu_postblend *postblend = to_postblend_block(vblk);

	vpp_osd1_postblend_5mux_set(vblk, state->sub->reg_ops, postblend->reg, VPP_NULL);
	MESON_DRM_BLOCK("%s disable called.\n", postblend->base.name);
}

static void postblend_dump_register(struct meson_vpu_block *vblk,
				    struct seq_file *seq)
{
	u32 value;
	struct meson_vpu_postblend *postblend;
	struct postblend_reg_s *reg;

	postblend = to_postblend_block(vblk);
	reg = postblend->reg;

	value = meson_drm_read_reg(reg->osd1_blend_src_ctrl);
	seq_printf(seq, "%-35s\t\t0x%08X\n", "OSD1_BLEND_SRC_CTRL:", value);

	value = meson_drm_read_reg(reg->osd2_blend_src_ctrl);
	seq_printf(seq, "%-35s\t\t0x%08X\n", "OSD2_BLEND_SRC_CTRL:", value);

	value = meson_drm_read_reg(reg->vd1_blend_src_ctrl);
	seq_printf(seq, "%-35s\t\t0x%08X\n", "VD1_BLEND_SRC_CTRL:", value);

	value = meson_drm_read_reg(reg->vd2_blend_src_ctrl);
	seq_printf(seq, "%-35s\t\t0x%08X\n", "VD2_BLEND_SRC_CTRL:", value);

	value = meson_drm_read_reg(reg->vpp_osd1_in_size);
	seq_printf(seq, "%-35s\t\t0x%08X\n", "VPP_OSD1_IN_SIZE:", value);

	value = meson_drm_read_reg(reg->vpp_osd1_bld_h_scope);
	seq_printf(seq, "%-35s\t\t0x%08X\n", "VPP_OSD1_BLD_H_SCOPE:", value);

	value = meson_drm_read_reg(reg->vpp_osd1_bld_v_scope);
	seq_printf(seq, "%-35s\t\t0x%08X\n", "VPP_OSD1_BLD_V_SCOPE:", value);

	value = meson_drm_read_reg(reg->vpp_osd2_bld_h_scope);
	seq_printf(seq, "%-35s\t\t0x%08X\n", "VPP_OSD2_BLD_H_SCOPE:", value);

	value = meson_drm_read_reg(reg->vpp_osd2_bld_v_scope);
	seq_printf(seq, "%-35s\t\t0x%08X\n", "VPP_OSD2_BLD_V_SCOPE:", value);
}

static void fix_vpu_clk2_default_regs(struct meson_vpu_block *vblk,
				      struct rdma_reg_ops *reg_ops)
{
	/* default: osd byp osd_blend */
	reg_ops->rdma_write_reg_bits(VPP_OSD1_SCALE_CTRL, 0x2, 0, 3);
	reg_ops->rdma_write_reg_bits(VPP_OSD2_SCALE_CTRL, 0x3, 0, 3);
	reg_ops->rdma_write_reg_bits(VPP_OSD3_SCALE_CTRL, 0x3, 0, 3);
	reg_ops->rdma_write_reg_bits(VPP_OSD4_SCALE_CTRL, 0x3, 0, 3);

	/* default: osd byp dolby */
	reg_ops->rdma_write_reg_bits(VPP_VD1_DSC_CTRL, 0x1, 4, 1);
	reg_ops->rdma_write_reg_bits(VPP_VD2_DSC_CTRL, 0x1, 4, 1);
	reg_ops->rdma_write_reg_bits(VPP_VD3_DSC_CTRL, 0x1, 4, 1);
	reg_ops->rdma_write_reg_bits(MALI_AFBCD_TOP_CTRL, 0x1, 14, 1);
	reg_ops->rdma_write_reg_bits(MALI_AFBCD_TOP_CTRL, 0x1, 19, 1);
	reg_ops->rdma_write_reg_bits(MALI_AFBCD1_TOP_CTRL, 0x1, 19, 1);
	reg_ops->rdma_write_reg_bits(MALI_AFBCD1_TOP_CTRL, 0x1, 19, 1);

	/* default: osd 12bit path */
	reg_ops->rdma_write_reg_bits(VPP_VD1_DSC_CTRL, 0x0, 5, 1);
	reg_ops->rdma_write_reg_bits(VPP_VD2_DSC_CTRL, 0x0, 5, 1);
	reg_ops->rdma_write_reg_bits(VPP_VD3_DSC_CTRL, 0x0, 5, 1);
	reg_ops->rdma_write_reg_bits(MALI_AFBCD_TOP_CTRL, 0x0, 15, 1);
	reg_ops->rdma_write_reg_bits(MALI_AFBCD_TOP_CTRL, 0x0, 20, 1);
	reg_ops->rdma_write_reg_bits(MALI_AFBCD1_TOP_CTRL, 0x0, 20, 1);
	reg_ops->rdma_write_reg_bits(MALI_AFBCD1_TOP_CTRL, 0x0, 20, 1);

	/* OSD3  uses VPP1*/
	osd_set_vpp_path_default(vblk, reg_ops, 3, 1);
	/* OSD4  uses VPP2*/
	osd_set_vpp_path_default(vblk, reg_ops, 4, 2);
}

static void independ_path_default_regs(struct meson_vpu_block *vblk,
				       struct rdma_reg_ops *reg_ops)
{
	/* default: osd1_bld_din_sel -- do not osd_data_byp osd_blend */
	reg_ops->rdma_write_reg_bits(VIU_OSD1_PATH_CTRL, 0x0, 4, 1);
	reg_ops->rdma_write_reg_bits(VIU_OSD2_PATH_CTRL, 0x0, 4, 1);
	reg_ops->rdma_write_reg_bits(VIU_OSD3_PATH_CTRL, 0x0, 4, 1);

	/* default: osd1_sc_path_sel -- before osd_blend or after hdr */
	reg_ops->rdma_write_reg_bits(VIU_OSD1_PATH_CTRL, 0x0, 0, 1);
	reg_ops->rdma_write_reg_bits(VIU_OSD2_PATH_CTRL, 0x1, 0, 1);
	reg_ops->rdma_write_reg_bits(VIU_OSD3_PATH_CTRL, 0x1, 0, 1);

	/* default: osd byp dolby */
	reg_ops->rdma_write_reg_bits(VIU_VD1_PATH_CTRL, 0x1, 16, 1);
	reg_ops->rdma_write_reg_bits(VIU_VD2_PATH_CTRL, 0x1, 16, 1);
	reg_ops->rdma_write_reg_bits(VIU_OSD1_PATH_CTRL, 0x1, 16, 1);
	reg_ops->rdma_write_reg_bits(VIU_OSD2_PATH_CTRL, 0x1, 16, 1);
	reg_ops->rdma_write_reg_bits(VIU_OSD3_PATH_CTRL, 0x1, 16, 1);

	/* default: osd 12bit path */
	reg_ops->rdma_write_reg_bits(VIU_VD1_PATH_CTRL, 0x0, 17, 1);
	reg_ops->rdma_write_reg_bits(VIU_VD2_PATH_CTRL, 0x0, 17, 1);
	reg_ops->rdma_write_reg_bits(VIU_OSD1_PATH_CTRL, 0x0, 17, 1);
	reg_ops->rdma_write_reg_bits(VIU_OSD2_PATH_CTRL, 0x0, 17, 1);
	reg_ops->rdma_write_reg_bits(VIU_OSD3_PATH_CTRL, 0x0, 17, 1);

	/* OSD3  uses VPP0*/
	osd_set_vpp_path_default(vblk, reg_ops, 3, 0);
	/* OSD4  uses VPP0*/
	osd_set_vpp_path_default(vblk, reg_ops, 4, 0);
}

static void postblend_osd2_def_conf(struct meson_vpu_block *vblk)
{
	struct meson_vpu_postblend *postblend = to_postblend_block(vblk);

	osd2_blend_switch_set(vblk, vblk->pipeline->subs[0].reg_ops,
			      postblend->reg, VPP_POSTBLEND);
	vpp_osd2_preblend_mux_set(vblk, vblk->pipeline->subs[0].reg_ops,
				  postblend->reg, VPP_NULL);
	vpp_osd2_postblend_mux_set(vblk, vblk->pipeline->subs[0].reg_ops,
				   postblend->reg, VPP_NULL);
}

static void postblend_hw_init(struct meson_vpu_block *vblk)
{
	struct meson_vpu_postblend *postblend = to_postblend_block(vblk);

	postblend->reg = &postblend_reg;
	postblend_osd2_def_conf(vblk);
	MESON_DRM_BLOCK("%s hw_init called.\n", postblend->base.name);
}

static void t7_postblend_hw_init(struct meson_vpu_block *vblk)
{
	struct meson_vpu_postblend *postblend = to_postblend_block(vblk);

	postblend->reg = &postblend_reg;
	postblend->reg1 = &postblend1_reg[vblk->index];

	if (vblk->index == 0) {
		fix_vpu_clk2_default_regs(vblk, vblk->pipeline->subs[0].reg_ops);
		postblend_osd2_def_conf(vblk);
	} else if (vblk->index == 1) {
		/*vpp1_bld_ctrl keep on osd3 enable*/
		vpp1_osd1_postblend_mux_set(vblk, vblk->pipeline->subs[0].reg_ops,
			postblend->reg1, 2);
	}

	MESON_DRM_BLOCK("%s hw_init called.\n", postblend->base.name);
}

static void t3_postblend_hw_init(struct meson_vpu_block *vblk)
{
	struct meson_vpu_postblend *postblend = to_postblend_block(vblk);

	postblend->reg = &postblend_reg;

	independ_path_default_regs(vblk, vblk->pipeline->subs[0].reg_ops);
	/*t3 t5w t5m paht crtl flag*/
	postblend->postblend_path_mask = true;
	MESON_DRM_BLOCK("%s hw_init called.\n", postblend->base.name);
}

static void s5_postblend_hw_init(struct meson_vpu_block *vblk)
{
	struct meson_vpu_postblend *postblend = to_postblend_block(vblk);
	struct rdma_reg_ops *reg_ops = vblk->pipeline->subs[0].reg_ops;

	postblend->reg = &s5_postblend_reg;

	/* default: osd byp dolby */
	/*core2a core2c dv init in uboot*/
	/*reg_ops->rdma_write_reg_bits(OSD_DOLBY_BYPASS_EN, 0x1, 0, 1);*/
	/*reg_ops->rdma_write_reg_bits(OSD_DOLBY_BYPASS_EN, 0x1, 2, 1);*/
	/*reg_ops->rdma_write_reg_bits(OSD_DOLBY_BYPASS_EN, 0x1, 4, 1);*/
	/*reg_ops->rdma_write_reg_bits(OSD_DOLBY_BYPASS_EN, 0x1, 6, 1);*/

	/* default: osd 12bit path */
	/*reg_ops->rdma_write_reg_bits(OSD_DOLBY_BYPASS_EN, 0x0, 1, 1);*/
	/*reg_ops->rdma_write_reg_bits(OSD_DOLBY_BYPASS_EN, 0x0, 3, 1);*/
	/*reg_ops->rdma_write_reg_bits(OSD_DOLBY_BYPASS_EN, 0x0, 5, 1);*/
	/*reg_ops->rdma_write_reg_bits(OSD_DOLBY_BYPASS_EN, 0x0, 7, 1);*/

	reg_ops->rdma_write_reg_bits(VPP_INTF_OSD3_CTRL, 0, 1, 1);
}

struct meson_vpu_block_ops postblend_ops = {
	.check_state = postblend_check_state,
	.update_state = postblend_set_state,
	.enable = postblend_hw_enable,
	.disable = postblend_hw_disable,
	.dump_register = postblend_dump_register,
	.init = postblend_hw_init,
};

struct meson_vpu_block_ops t7_postblend_ops = {
	.check_state = postblend_check_state,
	.update_state = t7_postblend_set_state,
	.enable = postblend_hw_enable,
	.disable = postblend_hw_disable,
	.dump_register = postblend_dump_register,
	.init = t7_postblend_hw_init,
};

struct meson_vpu_block_ops t3_postblend_ops = {
	.check_state = postblend_check_state,
	.update_state = t7_postblend_set_state,
	.enable = postblend_hw_enable,
	.disable = postblend_hw_disable,
	.dump_register = postblend_dump_register,
	.init = t3_postblend_hw_init,
};

struct meson_vpu_block_ops s5_postblend_ops = {
	.check_state = postblend_check_state,
	.update_state = s5_postblend_set_state,
	.enable = postblend_hw_enable,
	.disable = s5_postblend_hw_disable,
	.dump_register = postblend_dump_register,
	.init = s5_postblend_hw_init,
};
