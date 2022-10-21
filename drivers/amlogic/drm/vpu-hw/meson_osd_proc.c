// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include "meson_vpu_pipeline.h"
#include "meson_vpu_reg.h"
#include "meson_osd_proc.h"

static struct slice2ppc_reg_s slice2ppc_reg = {
	OSD1_PROC_IN_SIZE,
	OSD2_PROC_IN_SIZE,
	OSD3_PROC_IN_SIZE,
	OSD4_PROC_IN_SIZE,
	OSD1_PROC_OUT_SIZE,
	OSD2_PROC_OUT_SIZE,
	OSD3_PROC_OUT_SIZE,
	OSD4_PROC_OUT_SIZE,
	OSD_BLEND_DOUT0_SIZE,
	OSD_BLEND_DOUT1_SIZE,
	OSD_PROC_1MUX3_SEL,
	OSD_2SLICE2PPC_IN_SIZE,
	OSD_2SLICE2PPC_MODE,
	OSD_PI_BYPASS_EN,
	OSD_SYS_5MUX4_SEL,
	OSD_SYS_HWIN0_CUT,
	OSD_SYS_HWIN1_CUT,
	OSD_SYS_PAD_CTRL,
	OSD_SYS_PAD_DUMMY_DATA0,
	OSD_SYS_PAD_DUMMY_DATA1,
	OSD_SYS_PAD_H_SIZE,
	OSD_SYS_PAD_V_SIZE,
	OSD_SYS_2SLICE_HWIN_CUT,
};

static int slice2ppc_check_state(struct meson_vpu_block *vblk,
				 struct meson_vpu_block_state *state,
				 struct meson_vpu_pipeline_state *mvps)
{
	DRM_DEBUG("%s called\n", __func__);
	return 0;
}

static void slice2ppc_set_state(struct meson_vpu_block *vblk,
				struct meson_vpu_block_state *state,
				struct meson_vpu_block_state *old_state)
{
	struct meson_vpu_slice2ppc *slice;
	struct meson_vpu_pipeline *pipeline;
	struct meson_vpu_pipeline_state *mvps;
	struct meson_vpu_sub_pipeline_state *mvsps;
	struct rdma_reg_ops *reg_ops;
	struct slice2ppc_reg_s *reg;
	u32 slice_pad_h_bgn, slice_pad_h_end;

	slice = to_slice2ppc_block(vblk);
	pipeline = slice->base.pipeline;
	mvps = priv_to_pipeline_state(pipeline->obj.state);
	mvsps = &mvps->sub_states[0];
	reg_ops = state->sub->reg_ops;
	reg = slice->reg;

	//write osd1 proc in size
	reg_ops->rdma_write_reg(reg->osd1_proc_in_size, mvps->scaler_param[0].input_width << 1 |
						mvsps->scaler_din_vsize[OSD1_SLICE0] << 16);
	reg_ops->rdma_write_reg(reg->osd3_proc_in_size, mvps->scaler_param[2].input_width << 1 |
						mvsps->scaler_din_vsize[OSD3_SLICE1] << 16);
	reg_ops->rdma_write_reg_bits(VIU_OSD1_MISC, 1, 17, 1);
	reg_ops->rdma_write_reg_bits(VIU_OSD3_MISC, 1, 17, 1);

	//write osd1 proc out size
	reg_ops->rdma_write_reg(reg->osd1_proc_out_size, mvsps->scaler_dout_hsize[OSD1_SLICE0] |
						mvsps->scaler_dout_vsize[OSD1_SLICE0] << 16);
	reg_ops->rdma_write_reg(reg->osd3_proc_out_size, mvsps->scaler_dout_hsize[OSD3_SLICE1] |
						mvsps->scaler_dout_vsize[OSD3_SLICE1] << 16);

	// write OSD_2SLICE2PPC_IN_SIZE
	// viu_2slice2ppc_hsize = osd_out_hize_real
	reg_ops->rdma_write_reg(reg->osd_2slice2ppc_in_size, (mvsps->osd_out_hsize_real / 2) |
			    mvsps->slice2ppc_vsize << 16);

	//0: 2slice to 2ppc  1: 1 slice to 2ppc
	reg_ops->rdma_write_reg(reg->osd_2slice2ppc_mode, 0);

	//write hwin0 register
	reg_ops->rdma_write_reg_bits(reg->osd_sys_hwin0_cut, 1, 29, 1);
	reg_ops->rdma_write_reg_bits(reg->osd_sys_hwin0_cut,
				     mvsps->hwincut_bgn[OSD1_SLICE0], 16, 13);
	reg_ops->rdma_write_reg_bits(reg->osd_sys_hwin0_cut,
				     mvsps->hwincut_end[OSD1_SLICE0], 0, 13);

	//write hwin1 register
	reg_ops->rdma_write_reg_bits(reg->osd_sys_hwin1_cut, 1, 29, 1);
	reg_ops->rdma_write_reg_bits(reg->osd_sys_hwin1_cut,
				     mvsps->hwincut_bgn[OSD3_SLICE1], 16, 13);
	reg_ops->rdma_write_reg_bits(reg->osd_sys_hwin1_cut,
				     mvsps->hwincut_end[OSD3_SLICE1], 0, 13);

	if (mvsps->osd_out_hsize_raw < mvsps->osd_out_hsize_real) {
		slice_pad_h_bgn = 0;
		slice_pad_h_end = mvsps->osd_out_hsize_raw -
				  mvsps->osd_out_hsize_real / 2 - 1;
		reg_ops->rdma_write_reg_bits(reg->osd_sys_pad_ctrl, 1, 0, 1);
		reg_ops->rdma_write_reg(reg->osd_sys_pad_h_size,
					slice_pad_h_bgn << 16 | slice_pad_h_end);
		reg_ops->rdma_write_reg(reg->osd_sys_pad_v_size, mvsps->slice2ppc_vsize - 1);

		reg_ops->rdma_write_reg_bits(reg->osd_sys_2slice_hwin_cut, 1, 29, 1);
		reg_ops->rdma_write_reg_bits(reg->osd_sys_2slice_hwin_cut, 0, 16, 13);
		reg_ops->rdma_write_reg_bits(reg->osd_sys_2slice_hwin_cut,
					     mvsps->osd_out_hsize_raw / 2 - 1, 0, 13);
	} else {
		//write pad register
		reg_ops->rdma_write_reg_bits(reg->osd_sys_pad_ctrl, 0, 0, 1);

		//write cut register
		reg_ops->rdma_write_reg_bits(reg->osd_sys_2slice_hwin_cut, 0, 29, 1);
		reg_ops->rdma_write_reg_bits(reg->osd_sys_2slice_hwin_cut, 0, 16, 13);
		reg_ops->rdma_write_reg_bits(reg->osd_sys_2slice_hwin_cut,
					     mvsps->slice2ppc_hsize, 0, 13);
	}

	// write pi register
	reg_ops->rdma_write_reg_bits(reg->osd_pi_bypass_en, 1, 0, 1); // 1: bypass, 0: scaler up x2

	//write 5mux4 register, din0-osdblendout0, din1-osdblendout1
	//din2-osd1-bypass-slice&blend, din3-NA, din4-osd3-bypass-slice&blend
	//1---vpp-din0, 2---vpp-din1
	reg_ops->rdma_write_reg_bits(reg->osd_sys_5mux4_sel, 0x1, 0, 20);

	DRM_DEBUG("%s called\n", __func__);
}

static void slice2ppc_hw_enable(struct meson_vpu_block *vblk, struct meson_vpu_block_state *state)
{
	DRM_DEBUG("%s called\n", __func__);
}

static void slice2ppc_hw_disable(struct meson_vpu_block *vblk, struct meson_vpu_block_state *state)
{
	DRM_DEBUG("%s called\n", __func__);
}

static void slice2ppc_dump_register(struct meson_vpu_block *vblk,
				    struct seq_file *seq)
{
	u32 value;
	struct meson_vpu_slice2ppc *slice2ppc;
	struct slice2ppc_reg_s *reg;

	slice2ppc = to_slice2ppc_block(vblk);
	reg = slice2ppc->reg;

	value = meson_drm_read_reg(reg->osd1_proc_in_size);
	seq_printf(seq, "%-35s\t\t0x%08X\n", "OSD1_PROC_IN_SIZE:",  value);
	value = meson_drm_read_reg(reg->osd2_proc_in_size);
	seq_printf(seq, "%-35s\t\t0x%08X\n", "OSD2_PROC_IN_SIZE:",  value);
	value = meson_drm_read_reg(reg->osd3_proc_in_size);
	seq_printf(seq, "%-35s\t\t0x%08X\n", "OSD3_PROC_IN_SIZE:",  value);
	value = meson_drm_read_reg(reg->osd4_proc_in_size);
	seq_printf(seq, "%-35s\t\t0x%08X\n", "OSD4_PROC_IN_SIZE:",  value);
	value = meson_drm_read_reg(reg->osd1_proc_out_size);
	seq_printf(seq, "%-35s\t\t0x%08X\n", "OSD1_PROC_OUT_SIZE:",  value);
	value = meson_drm_read_reg(reg->osd2_proc_out_size);
	seq_printf(seq, "%-35s\t\t0x%08X\n", "OSD2_PROC_OUT_SIZE:",  value);
	value = meson_drm_read_reg(reg->osd3_proc_out_size);
	seq_printf(seq, "%-35s\t\t0x%08X\n", "OSD3_PROC_OUT_SIZE:",  value);
	value = meson_drm_read_reg(reg->osd4_proc_out_size);
	seq_printf(seq, "%-35s\t\t0x%08X\n", "OSD4_PROC_OUT_SIZE:",  value);
	value = meson_drm_read_reg(reg->osd_blend_dout0_size);
	seq_printf(seq, "%-35s\t\t0x%08X\n", "OSD_BLEND_DOUT0_SIZE:",  value);
	value = meson_drm_read_reg(reg->osd_blend_dout1_size);
	seq_printf(seq, "%-35s\t\t0x%08X\n", "OSD_BLEND_DOUT1_SIZE:",  value);
	value = meson_drm_read_reg(reg->osd_proc_1mux3_sel);
	seq_printf(seq, "%-35s\t\t0x%08X\n", "OSD_PROC_1MUX3_SEL:",  value);
	value = meson_drm_read_reg(reg->osd_2slice2ppc_in_size);
	seq_printf(seq, "%-35s\t\t0x%08X\n", "OSD_2SLICE2PPC_IN_SIZE:",  value);
	value = meson_drm_read_reg(reg->osd_2slice2ppc_mode);
	seq_printf(seq, "%-35s\t\t0x%08X\n", "OSD_2SLICE2PPC_MODE:",  value);
	value = meson_drm_read_reg(reg->osd_pi_bypass_en);
	seq_printf(seq, "%-35s\t\t0x%08X\n", "OSD_PI_BYPASS_EN:",  value);
	value = meson_drm_read_reg(reg->osd_sys_5mux4_sel);
	seq_printf(seq, "%-35s\t\t0x%08X\n", "OSD_SYS_5MUX4_SEL:",  value);
	value = meson_drm_read_reg(reg->osd_sys_hwin0_cut);
	seq_printf(seq, "%-35s\t\t0x%08X\n", "OSD_SYS_HWIN0_CUT:",  value);
	value = meson_drm_read_reg(reg->osd_sys_hwin1_cut);
	seq_printf(seq, "%-35s\t\t0x%08X\n", "OSD_SYS_HWIN1_CUT:",  value);
	value = meson_drm_read_reg(reg->osd_sys_pad_ctrl);
	seq_printf(seq, "%-35s\t\t0x%08X\n", "OSD_SYS_PAD_CTRL:",  value);
	value = meson_drm_read_reg(reg->osd_sys_pad_dummy_data0);
	seq_printf(seq, "%-35s\t\t0x%08X\n", "OSD_SYS_PAD_DUMMY_DATA0:",  value);
	value = meson_drm_read_reg(reg->osd_sys_pad_dummy_data1);
	seq_printf(seq, "%-35s\t\t0x%08X\n", "OSD_SYS_PAD_DUMMY_DATA1:",  value);
	value = meson_drm_read_reg(reg->osd_sys_pad_h_size);
	seq_printf(seq, "%-35s\t\t0x%08X\n", "OSD_SYS_PAD_H_SIZE:",  value);
	value = meson_drm_read_reg(reg->osd_sys_pad_v_size);
	seq_printf(seq, "%-35s\t\t0x%08X\n", "OSD_SYS_PAD_V_SIZE:",  value);
	value = meson_drm_read_reg(reg->osd_sys_2slice_hwin_cut);
	seq_printf(seq, "%-35s\t\t0x%08X\n", "OSD_SYS_2SLICE_HWIN_CUT:",  value);
}

static void slice2ppc_hw_init(struct meson_vpu_block *vblk)
{
	struct meson_vpu_slice2ppc *slice2ppc = to_slice2ppc_block(vblk);

	slice2ppc->reg = &slice2ppc_reg;
}

static void slice2ppc_hw_fini(struct meson_vpu_block *vblk)
{
	DRM_DEBUG("%s called\n", __func__);
}

struct meson_vpu_block_ops slice2ppc_ops = {
	.check_state = slice2ppc_check_state,
	.update_state = slice2ppc_set_state,
	.enable = slice2ppc_hw_enable,
	.disable = slice2ppc_hw_disable,
	.dump_register = slice2ppc_dump_register,
	.init = slice2ppc_hw_init,
	.fini = slice2ppc_hw_fini,
};
