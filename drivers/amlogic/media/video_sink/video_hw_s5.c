// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * drivers/amlogic/media/video_sink/video_hw_s5.c
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

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/amlogic/major.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/ctype.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>
#include <linux/amlogic/media/utils/amstream.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/poll.h>
#include <linux/clk.h>
#include <linux/arm-smccc.h>
#include <linux/debugfs.h>
#include <linux/amlogic/media/canvas/canvas.h>
#include <linux/amlogic/media/canvas/canvas_mgr.h>
#include <linux/sched.h>
#include <linux/amlogic/media/video_sink/video_keeper.h>
#include <linux/amlogic/media/video_sink/vpp.h>
#include "video_priv.h"
#include "video_hw_s5.h"
#include "video_reg_s5.h"
#include "vpp_post_s5.h"
#include "video_common.h"
#include "video_hw.h"

#if defined(CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_VECM)
#include <linux/amlogic/media/amvecm/amvecm.h>
#endif
#include <linux/amlogic/media/utils/vdec_reg.h>

#include <linux/amlogic/media/registers/register.h>
#include <linux/uaccess.h>
#include <linux/amlogic/media/utils/amports_config.h>
#include <linux/amlogic/media/vpu/vpu.h>
#include "videolog.h"

#include <linux/amlogic/media/video_sink/vpp.h>
#ifdef CONFIG_AMLOGIC_MEDIA_TVIN
#include "linux/amlogic/media/frame_provider/tvin/tvin_v4l2.h"
#endif
#ifdef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
#include "../common/rdma/rdma.h"
#endif
#include <linux/amlogic/media/video_sink/video.h>
#include "../common/vfm/vfm.h"
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
#include <linux/amlogic/media/amdolbyvision/dolby_vision.h>
#endif
#include "video_receiver.h"
#ifdef CONFIG_AMLOGIC_MEDIA_LUT_DMA
#include <linux/amlogic/media/lut_dma/lut_dma.h>
#endif
#ifdef CONFIG_AMLOGIC_MEDIA_SECURITY
#include <linux/amlogic/media/vpu_secure/vpu_secure.h>
#endif
#include <linux/amlogic/media/video_sink/video_signal_notify.h>
#ifdef CONFIG_AMLOGIC_MEDIA_DEINTERLACE
#include <linux/amlogic/media/di/di_interface.h>
#endif
#include "vpp_regs_s5.h"

static struct vd_proc_s g_vd_proc;
struct vpp_post_reg_s vpp_post_reg;
struct vd_proc_reg_s vd_proc_reg;
static u32 vpp_hold_line_s5 = 8;
static u32 vpp_ofifo_size_s5 = 0x800;
static u32 conv_lbuf_len_s5[MAX_VD_LAYER] = {0x100, 0x100, 0x100};
static u32 g_bypass_module = 5;
static struct vpp_post_info_t vpp_post_amdv;
static struct vd_proc_info_t vd_proc_amdv;
#define SIZE_ALIG32(frm_hsize)   ((((frm_hsize) + 31) >> 5) << 5)
#define SIZE_ALIG16(frm_hsize)   ((((frm_hsize) + 15) >> 4) << 4)
#define SIZE_ALIG8(frm_hsize)    ((((frm_hsize) + 7) >> 3) << 3)
#define SIZE_ALIG4(frm_hsize)    ((((frm_hsize) + 3) >> 2) << 2)
#define BYPASS_DV         BIT(0)
#define BYPASS_HDR        BIT(1)
#define BYAPSS_DETUNNEL   BIT(2)

static u32 g_slice_num = 0xff;
MODULE_PARM_DESC(g_slice_num, "\n g_slice_num\n");
module_param(g_slice_num, uint, 0664);

u32 pi_enable = 0xff;
MODULE_PARM_DESC(pi_enable, "\n pi_enable\n");
module_param(pi_enable, uint, 0664);

u32 g_vd1s1_vd2_prebld_en = 0xff;
MODULE_PARM_DESC(g_vd1s1_vd2_prebld_en, "\n g_vd1s1_vd2_prebld_en\n");
module_param(g_vd1s1_vd2_prebld_en, uint, 0664);

u32 debug_flag_s5;
MODULE_PARM_DESC(debug_flag_s5, "\n debug_flag_s5\n");
module_param(debug_flag_s5, uint, 0664);

static void vd1_scaler_setting_s5(struct video_layer_s *layer,
	struct scaler_setting_s *setting,
	u32 slice);
static void vdx_scaler_setting_s5(struct video_layer_s *layer,
	struct scaler_setting_s *setting);
static void _vd_mif_setting_s5(struct video_layer_s *layer,
			struct mif_pos_s *setting);
static void _vd_fgrain_config_s5(struct video_layer_s *layer,
		   struct vpp_frame_par_s *frame_par,
		   struct vframe_s *vf);
static void _vd_fgrain_setting_s5(struct video_layer_s *layer,
		    struct vframe_s *vf);

static inline u32 slice_out_hsize(u32 slice,
	u32 slice_num, u32 frm_hsize)
{
	u32 hsize = 0;

	switch (slice_num) {
	case 1:
		hsize = frm_hsize;
		break;
	case 2:
		if (slice == slice_num - 1)
			hsize = frm_hsize - SIZE_ALIG16(frm_hsize) *
				(slice_num - 1) / slice_num;
		else
			hsize = SIZE_ALIG16(frm_hsize) / slice_num;
		break;
	case 4:
		if (slice == slice_num - 1)
			hsize = frm_hsize - SIZE_ALIG32(frm_hsize) *
				(slice_num - 1) / slice_num;
		else
			hsize = SIZE_ALIG32(frm_hsize) / slice_num;
		break;
	}
	return hsize;
}

static inline u32 slice_in_hsize(u32 slice,
	u32 slice_num, u32 frm_hsize, u32 overlap_hsize)
{
	u32 hsize = 0;

	switch (slice_num) {
	case 1:
		hsize = frm_hsize;
		break;
	case 2:
		if (slice == 0)
			hsize = SIZE_ALIG32(frm_hsize) / 2 + overlap_hsize;
		else
			hsize = (frm_hsize - SIZE_ALIG32(frm_hsize) / 2) +
				overlap_hsize;
		break;
	case 4:
		if (slice == 0)
			hsize = SIZE_ALIG32(frm_hsize) / 4 + overlap_hsize;
		else if (slice == 3)
			hsize = (frm_hsize - SIZE_ALIG32(frm_hsize) * 3 / 4) +
				overlap_hsize;
		else
			hsize = SIZE_ALIG32(frm_hsize) / 4 + overlap_hsize * 2;
		break;
	}
	return hsize;
}

void check_afbc_status(void)
{
	int i;
	u32 reg_addr[4] = {0}, reg_val[4] = {0};
	struct vd_afbc_reg_s *vd_afbc_reg = NULL;

	for (i = 0; i < MAX_VD_LAYER_S5 - 1; i++) {
		vd_afbc_reg = &vd_proc_reg.vd_afbc_reg[i];
		reg_addr[i] = vd_afbc_reg->afbc_stat;
		reg_val[i] = READ_VCBUS_REG(reg_addr[i]);
		if (((reg_val[i] & 0x7ffc) == 0) &&
			((reg_val[i] & 0x30000) != 0x30000))
			pr_info("afbc stats err: 0x%x = 0x%x\n",
				reg_addr[i], reg_val[i]);
		WRITE_VCBUS_REG(reg_addr[i], 1);
	}
}

static void dump_afbc_reg(void)
{
	int i;
	u32 reg_addr, reg_val = 0;
	struct vd_afbc_reg_s *vd_afbc_reg = NULL;

	for (i = 0; i < MAX_VD_LAYER_S5; i++) {
		vd_afbc_reg = &vd_proc_reg.vd_afbc_reg[i];

		pr_info("vd%d afbc regs:\n", i);
		reg_addr = vd_afbc_reg->afbc_enable;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
		reg_addr = vd_afbc_reg->afbc_mode;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
		reg_addr = vd_afbc_reg->afbc_size_in;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
		reg_addr = vd_afbc_reg->afbc_dec_def_color;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
		reg_addr = vd_afbc_reg->afbc_conv_ctrl;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
		reg_addr = vd_afbc_reg->afbc_lbuf_depth;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
		reg_addr = vd_afbc_reg->afbc_head_baddr;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
		reg_addr = vd_afbc_reg->afbc_body_baddr;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
		reg_addr = vd_afbc_reg->afbc_size_out;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
		reg_addr = vd_afbc_reg->afbc_out_yscope;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
		reg_addr = vd_afbc_reg->afbc_vd_cfmt_ctrl;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
		reg_addr = vd_afbc_reg->afbc_vd_cfmt_w;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
		reg_addr = vd_afbc_reg->afbc_mif_hor_scope;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
		reg_addr = vd_afbc_reg->afbc_mif_ver_scope;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
		reg_addr = vd_afbc_reg->afbc_pixel_hor_scope;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
		reg_addr = vd_afbc_reg->afbc_pixel_ver_scope;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
		reg_addr = vd_afbc_reg->afbc_vd_cfmt_h;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
		reg_addr = vd_afbc_reg->afbc_top_ctrl;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
			reg_addr, reg_val);
	}
}

static void dump_mif_reg(void)
{
	int i;
	u32 reg_addr, reg_val = 0;
	struct vd_mif_reg_s *vd_mif_reg = NULL;
	struct vd_mif_linear_reg_s *vd_mif_linear_reg = NULL;

	for (i = 0; i < MAX_VD_LAYER_S5; i++) {
		vd_mif_reg = &vd_proc_reg.vd_mif_reg[i];
		vd_mif_linear_reg = &vd_proc_reg.vd_mif_linear_reg[i];

		pr_info("vd%d mif regs:\n", i);
		reg_addr = vd_mif_reg->vd_if0_gen_reg;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
		reg_addr = vd_mif_reg->vd_if0_canvas0;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
		reg_addr = vd_mif_reg->vd_if0_canvas1;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
		reg_addr = vd_mif_reg->vd_if0_luma_x0;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
		reg_addr = vd_mif_reg->vd_if0_luma_y0;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
		reg_addr = vd_mif_reg->vd_if0_chroma_x0;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
		reg_addr = vd_mif_reg->vd_if0_chroma_y0;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
		reg_addr = vd_mif_reg->vd_if0_luma_x1;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
		reg_addr = vd_mif_reg->vd_if0_luma_y1;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
		reg_addr = vd_mif_reg->vd_if0_chroma_x1;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
		reg_addr = vd_mif_reg->vd_if0_chroma_y1;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
		reg_addr = vd_mif_reg->vd_if0_rpt_loop;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
		reg_addr = vd_mif_reg->vd_if0_luma0_rpt_pat;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
		reg_addr = vd_mif_reg->vd_if0_chroma0_rpt_pat;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
		reg_addr = vd_mif_reg->vd_if0_luma1_rpt_pat;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
		reg_addr = vd_mif_reg->vd_if0_chroma1_rpt_pat;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
		reg_addr = vd_mif_reg->vd_if0_luma_psel;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
		reg_addr = vd_mif_reg->vd_if0_chroma_psel;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
		reg_addr = vd_mif_reg->vd_if0_luma_fifo_size;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
		reg_addr = vd_mif_reg->vd_if0_gen_reg2;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
		reg_addr = vd_mif_reg->vd_if0_gen_reg3;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
		reg_addr = vd_mif_reg->viu_vd_fmt_ctrl;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
		reg_addr = vd_mif_reg->viu_vd_fmt_w;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);

		pr_info("vd%d mif linear regs:\n", i);
		reg_addr = vd_mif_linear_reg->vd_if0_baddr_y;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
		reg_addr = vd_mif_linear_reg->vd_if0_baddr_cb;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
		reg_addr = vd_mif_linear_reg->vd_if0_baddr_cr;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
		reg_addr = vd_mif_linear_reg->vd_if0_stride_0;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
		reg_addr = vd_mif_linear_reg->vd_if0_stride_1;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
		reg_addr = vd_mif_linear_reg->vd_if0_baddr_y_f1;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
		reg_addr = vd_mif_linear_reg->vd_if0_baddr_cb_f1;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
		reg_addr = vd_mif_linear_reg->vd_if0_baddr_cr_f1;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
		reg_addr = vd_mif_linear_reg->vd_if0_stride_0_f1;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
		reg_addr = vd_mif_linear_reg->vd_if0_stride_1_f1;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
	}
}

static void dump_sr_reg(void)
{
	u32 reg_addr, reg_val = 0;
	struct vd_proc_sr_reg_s *vd_sr_reg = NULL;

	vd_sr_reg = &vd_proc_reg.vd_proc_sr_reg;
	pr_info("sr regs:\n");
	reg_addr = vd_sr_reg->vd_proc_s0_sr0_in_size;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = vd_sr_reg->vd_proc_s1_sr0_in_size;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = vd_sr_reg->vd_proc_s0_sr1_in_size;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = vd_sr_reg->vd_proc_sr0_ctrl;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = vd_sr_reg->vd_proc_sr1_ctrl;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = vd_sr_reg->srsharp0_sharp_sr2_ctrl;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = vd_sr_reg->srsharp1_sharp_sr2_ctrl;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = vd_sr_reg->srsharp1_nn_post_top;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
}

static void dump_pps_reg(void)
{
	int i;
	u32 reg_addr, reg_val = 0;
	struct vd_pps_reg_s *vd_pps_reg = NULL;

	for (i = 0; i < MAX_VD_LAYER_S5; i++) {
		vd_pps_reg = &vd_proc_reg.vd_pps_reg[i];

		pr_info("vd%d pps regs:\n", i);
		reg_addr = vd_pps_reg->vd_vsc_region12_startp;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
		reg_addr = vd_pps_reg->vd_vsc_region34_startp;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
		reg_addr = vd_pps_reg->vd_vsc_region4_endp;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
		reg_addr = vd_pps_reg->vd_vsc_start_phase_step;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
		reg_addr = vd_pps_reg->vd_vsc_region1_phase_slope;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
		reg_addr = vd_pps_reg->vd_vsc_region3_phase_slope;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
		reg_addr = vd_pps_reg->vd_vsc_phase_ctrl;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
		reg_addr = vd_pps_reg->vd_vsc_init_phase;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
		reg_addr = vd_pps_reg->vd_hsc_region12_startp;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
		reg_addr = vd_pps_reg->vd_hsc_region34_startp;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
		reg_addr = vd_pps_reg->vd_hsc_region4_endp;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
		reg_addr = vd_pps_reg->vd_hsc_start_phase_step;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
		reg_addr = vd_pps_reg->vd_hsc_region1_phase_slope;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
		reg_addr = vd_pps_reg->vd_hsc_region3_phase_slope;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
		reg_addr = vd_pps_reg->vd_hsc_phase_ctrl;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
		reg_addr = vd_pps_reg->vd_sc_misc;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
		reg_addr = vd_pps_reg->vd_hsc_phase_ctrl1;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
		reg_addr = vd_pps_reg->vd_prehsc_coef;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
		reg_addr = vd_pps_reg->vd_pre_scale_ctrl;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
		reg_addr = vd_pps_reg->vd_prevsc_coef;
		if (reg_addr) {
			reg_val = READ_VCBUS_REG(reg_addr);
			pr_info("[0x%x] = 0x%X\n",
				   reg_addr, reg_val);
		}
		reg_addr = vd_pps_reg->vd_prehsc_coef1;
		if (reg_addr) {
			reg_val = READ_VCBUS_REG(reg_addr);
			pr_info("[0x%x] = 0x%X\n",
				   reg_addr, reg_val);
		}
	}
}

static void dump_fgrain_reg(void)
{
	int i;
	u32 reg_addr, reg_val = 0;
	bool fgrain_support = false;
	struct vd_fg_reg_s *vd_fg_reg = NULL;

	for (i = 0; i < MAX_VD_LAYER_S5; i++) {
		vd_fg_reg = &vd_proc_reg.vd_fg_reg[i];
		if (i < SLICE_NUM)
			fgrain_support = glayer_info[0].fgrain_support;
		else
			fgrain_support = glayer_info[i - SLICE_NUM + 1].fgrain_support;
		if (fgrain_support) {
			pr_info("vd%d fgrain regs:\n", i);
			reg_addr = vd_fg_reg->fgrain_ctrl;
			reg_val = READ_VCBUS_REG(reg_addr);
			pr_info("[0x%x] = 0x%X\n",
				   reg_addr, reg_val);
			reg_addr = vd_fg_reg->fgrain_win_h;
			reg_val = READ_VCBUS_REG(reg_addr);
			pr_info("[0x%x] = 0x%X\n",
				   reg_addr, reg_val);
			reg_addr = vd_fg_reg->fgrain_win_v;
			reg_val = READ_VCBUS_REG(reg_addr);
			pr_info("[0x%x] = 0x%X\n",
				   reg_addr, reg_val);
		}
	}
}

static void dump_vd_slice_reg(void)
{
	int i;
	u32 reg_addr, reg_val = 0;
	struct vd_proc_slice_reg_s *vd_proc_slice_reg = NULL;

	for (i = 0; i < SLICE_NUM; i++) {
		vd_proc_slice_reg = &vd_proc_reg.vd_proc_slice_reg[i];
		pr_info("vd slice%d regs:\n", i);
		reg_addr = vd_proc_slice_reg->vd_proc_s0_in_size;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);

		reg_addr = vd_proc_slice_reg->vd_proc_s0_out_size;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);

		reg_addr = vd_proc_slice_reg->vd_s0_hwin_cut;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);

		reg_addr = vd_proc_slice_reg->vd1_s0_dv_bypass_ctrl;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);

		reg_addr = vd_proc_slice_reg->vd1_s0_clip_misc0;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);

		reg_addr = vd_proc_slice_reg->vd1_s0_clip_misc1;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
	}
}

static void dump_vd_padding_reg(void)
{
	int i;
	u32 reg_addr, reg_val = 0;
	struct vd1_slice_pad_reg_s *vd1_slice_pad_reg = NULL;

	for (i = 0; i < SLICE_NUM; i++) {
		vd1_slice_pad_reg = &vd_proc_reg.vd1_slice_pad_size0_reg[i];
		pr_info("vd slice%d pading regs:\n", i);
		reg_addr = vd1_slice_pad_reg->vd1_slice_pad_h_size;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);

		reg_addr = vd1_slice_pad_reg->vd1_slice_pad_v_size;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
	}
	pr_info("VD1_PAD_CTRL:\n");
	reg_addr = VD1_PAD_CTRL;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);

}

static void dump_vd_blend_reg(void)
{
	u32 reg_addr, reg_val = 0;
	struct vd_proc_blend_reg_s *vd_proc_blend_reg = NULL;

	vd_proc_blend_reg = &vd_proc_reg.vd_proc_blend_reg;
	pr_info("vd blend regs:\n");
	reg_addr = vd_proc_blend_reg->vpp_vd_blnd_h_v_size;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = vd_proc_blend_reg->vpp_vd_blend_dummy_data;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = vd_proc_blend_reg->vpp_vd_blend_dummy_alpha;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = vd_proc_blend_reg->vpp_blend_vd1_s0_h_start_end;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = vd_proc_blend_reg->vpp_blend_vd1_s0_v_start_end;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = vd_proc_blend_reg->vpp_blend_vd1_s1_h_start_end;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = vd_proc_blend_reg->vpp_blend_vd1_s1_v_start_end;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = vd_proc_blend_reg->vpp_blend_vd1_s2_h_start_end;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = vd_proc_blend_reg->vpp_blend_vd1_s2_v_start_end;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = vd_proc_blend_reg->vpp_blend_vd1_s3_h_start_end;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = vd_proc_blend_reg->vpp_blend_vd1_s3_v_start_end;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = vd_proc_blend_reg->vd1_s0_blend_src_ctrl;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = vd_proc_blend_reg->vd1_s1_blend_src_ctrl;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = vd_proc_blend_reg->vd1_s2_blend_src_ctrl;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = vd_proc_blend_reg->vd1_s3_blend_src_ctrl;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = vd_proc_blend_reg->vpp_vd_blnd_ctrl;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
}

static void dump_vd_misc_reg(void)
{
	u32 reg_addr, reg_val = 0;
	struct vd_proc_misc_reg_s *vd_proc_misc_reg = NULL;

	vd_proc_misc_reg = &vd_proc_reg.vd_proc_misc_reg;
	pr_info("vd misc regs:\n");
	reg_addr = vd_proc_misc_reg->vd_proc_sr0_ctrl;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = vd_proc_misc_reg->vd_proc_bypass_ctrl;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = vd_proc_misc_reg->vd1_pi_ctrl;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
}

static void dump_vd2_pre_blend_reg(void)
{
	u32 reg_addr, reg_val = 0;
	struct vd2_pre_blend_reg_s *vd2_pre_blend_reg = NULL;

	vd2_pre_blend_reg = &vd_proc_reg.vd2_pre_blend_reg;
	pr_info("vd2 preblend regs:\n");
	reg_addr = vd2_pre_blend_reg->vpp_vd_preblnd_h_v_size;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = vd2_pre_blend_reg->vpp_vd_pre_blend_dummy_data;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = vd2_pre_blend_reg->vpp_vd_pre_blend_dummy_alpha;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = vd2_pre_blend_reg->vpp_preblend_vd1_h_start_end;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = vd2_pre_blend_reg->vpp_preblend_vd1_v_start_end;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = vd2_pre_blend_reg->vpp_preblend_vd2_h_start_end;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = vd2_pre_blend_reg->vpp_preblend_vd2_v_start_end;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = vd2_pre_blend_reg->vd1_preblend_src_ctrl;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = vd2_pre_blend_reg->vd2_preblend_src_ctrl;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = vd2_pre_blend_reg->vd1_preblend_alpha;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = vd2_pre_blend_reg->vd2_preblend_alpha;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = vd2_pre_blend_reg->vpp_vd_preblnd_ctrl;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
}

static void dump_vd2_proc_misc_reg(void)
{
	u32 reg_addr, reg_val = 0;
	struct vd2_proc_misc_reg_s *vd2_proc_misc_reg = NULL;

	vd2_proc_misc_reg = &vd_proc_reg.vd2_proc_misc_reg;
	pr_info("vd2 proc misc regs:\n");
	reg_addr = vd2_proc_misc_reg->vd2_proc_in_size;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = vd2_proc_misc_reg->vd2_detunl_ctrl;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = vd2_proc_misc_reg->vd2_hdr_ctrl;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = vd2_proc_misc_reg->vd2_pilite_ctrl;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = vd2_proc_misc_reg->vd2_proc_out_size;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = vd2_proc_misc_reg->vd2_dv_bypass_ctrl;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
}

static void dump_vd_pip_alpha_reg(void)
{
	u32 reg_addr, reg_val = 0;
	struct vd_pip_alpha_reg_s *vd_pip_alpha_reg = NULL;
	int i;

	vd_pip_alpha_reg = &vd_proc_reg.vd_pip_alpha_reg[0];
	pr_info("vd1 pip alpha regs:\n");
	reg_addr = vd_pip_alpha_reg->vd_pip_alph_ctrl;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	for (i = 0; i < MAX_PIP_WINDOW; i++) {
		reg_addr = vd_pip_alpha_reg->vd_pip_alph_scp_h + i;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
		reg_addr = vd_pip_alpha_reg->vd_pip_alph_scp_v + i;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
	}
	vd_pip_alpha_reg = &vd_proc_reg.vd_pip_alpha_reg[1];
	pr_info("vd2 pip alpha regs:\n");
	reg_addr = vd_pip_alpha_reg->vd_pip_alph_ctrl;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	for (i = 0; i < MAX_PIP_WINDOW; i++) {
		reg_addr = vd_pip_alpha_reg->vd_pip_alph_scp_h + i;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
		reg_addr = vd_pip_alpha_reg->vd_pip_alph_scp_v + i;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
	}
}

static void dump_vd_aisr_reg(void)
{
	u32 reg_addr, reg_val = 0;
	struct vd_aisr_reshape_reg_s *aisr_reshape_reg;
	struct vd_pps_reg_s *aisr_pps_reg;

	aisr_reshape_reg = &vd_proc_reg.aisr_reshape_reg;
	aisr_pps_reg = &vd_proc_reg.vd_pps_reg[AISR_SCHN];

	pr_info("aisr reshape regs:\n");
	reg_addr = aisr_reshape_reg->aisr_reshape_ctrl0;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = aisr_reshape_reg->aisr_reshape_ctrl1;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = aisr_reshape_reg->aisr_reshape_scope_x;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = aisr_reshape_reg->aisr_reshape_scope_y;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = aisr_reshape_reg->aisr_reshape_baddr00;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = aisr_reshape_reg->aisr_reshape_baddr01;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = aisr_reshape_reg->aisr_reshape_baddr02;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = aisr_reshape_reg->aisr_reshape_baddr03;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = aisr_reshape_reg->aisr_reshape_baddr10;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = aisr_reshape_reg->aisr_reshape_baddr11;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = aisr_reshape_reg->aisr_reshape_baddr12;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = aisr_reshape_reg->aisr_reshape_baddr13;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = aisr_reshape_reg->aisr_reshape_baddr20;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = aisr_reshape_reg->aisr_reshape_baddr21;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = aisr_reshape_reg->aisr_reshape_baddr22;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = aisr_reshape_reg->aisr_reshape_baddr23;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = aisr_reshape_reg->aisr_reshape_baddr30;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = aisr_reshape_reg->aisr_reshape_baddr31;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = aisr_reshape_reg->aisr_reshape_baddr32;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = aisr_reshape_reg->aisr_reshape_baddr33;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = aisr_reshape_reg->aisr_post_ctrl;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = aisr_reshape_reg->aisr_post_size;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = aisr_reshape_reg->aisr_sr1_nn_post_top;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	pr_info("aisr pps regs:\n");
	reg_addr = aisr_pps_reg->vd_vsc_region12_startp;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = aisr_pps_reg->vd_vsc_region34_startp;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = aisr_pps_reg->vd_vsc_region4_endp;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = aisr_pps_reg->vd_vsc_start_phase_step;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = aisr_pps_reg->vd_vsc_region1_phase_slope;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = aisr_pps_reg->vd_vsc_region3_phase_slope;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = aisr_pps_reg->vd_vsc_phase_ctrl;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = aisr_pps_reg->vd_vsc_init_phase;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = aisr_pps_reg->vd_hsc_region12_startp;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = aisr_pps_reg->vd_hsc_region34_startp;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = aisr_pps_reg->vd_hsc_region4_endp;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = aisr_pps_reg->vd_hsc_start_phase_step;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = aisr_pps_reg->vd_hsc_region1_phase_slope;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = aisr_pps_reg->vd_hsc_region3_phase_slope;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = aisr_pps_reg->vd_hsc_phase_ctrl;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = aisr_pps_reg->vd_sc_misc;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = aisr_pps_reg->vd_hsc_phase_ctrl1;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = aisr_pps_reg->vd_prehsc_coef;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = aisr_pps_reg->vd_pre_scale_ctrl;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = aisr_pps_reg->vd_prevsc_coef;
	if (reg_addr) {
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
	}
	reg_addr = aisr_pps_reg->vd_prehsc_coef1;
	if (reg_addr) {
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
	}
}

void dump_s5_vd_proc_regs(void)
{
	dump_mif_reg();
	dump_afbc_reg();
	dump_pps_reg();
	dump_sr_reg();
	dump_fgrain_reg();
	dump_vd_slice_reg();
	dump_vd_padding_reg();
	dump_vd_blend_reg();
	dump_vd_misc_reg();
	dump_vd2_pre_blend_reg();
	dump_vd2_proc_misc_reg();
	dump_vd_pip_alpha_reg();
	if (cur_dev->aisr_support)
		dump_vd_aisr_reg();
}

ssize_t video_vd_proc_state_dump(char *buf)
{
	ssize_t len = 0;
	int slice = 0;
	struct vd_proc_s *vd_proc = &g_vd_proc;
	struct vd_proc_vd1_info_s *vd_proc_vd1_info;
	struct vd_proc_vd2_info_s *vd_proc_vd2_info;
	struct vd_proc_preblend_info_s *vd_proc_preblend_info;
	struct vd_proc_unit_s *vd_proc_unit;
	//struct sr_info_s *sr;
	char work_mode_info[5][24] = {
			{"VD1_4SLICES_MODE"},
			{"VD1_2_2SLICES_MODE"},
			{"VD1_SLICES01_MODE"},
			{"VD1_SLICES23_MODE"},
			{"VD1_1SLICES_MODE"},
		};
	char vd1_slices_dout_dpsel[4][32] = {
			{"VD1_SLICES_DOUT_PI"},
			{"VD1_SLICES_DOUT_4S4P"},
			{"VD1_SLICES_DOUT_2S4P"},
			{"VD1_SLICES_DOUT_1S4P"},
		};
	len += sprintf(buf + len, "%s info list:\n", __func__);
	vd_proc_vd1_info = &vd_proc->vd_proc_vd1_info;
	vd_proc_vd2_info = &vd_proc->vd_proc_vd2_info;
	vd_proc_preblend_info = &vd_proc->vd_proc_preblend_info;

	len += sprintf(buf + len, "slice_num:[%d]\n",
		vd_proc_vd1_info->slice_num);
	len += sprintf(buf + len, "vd1_work_mode:[%s]\n",
		work_mode_info[vd_proc_vd1_info->vd1_work_mode]);
	len += sprintf(buf + len, "vd1_slices_dout_dpsel:[%s]\n",
		vd1_slices_dout_dpsel[vd_proc_vd1_info->vd1_slices_dout_dpsel]);
	len += sprintf(buf + len, "vd1s1_vd2_prebld_en:[%d]\n",
		vd_proc_preblend_info->vd1s1_vd2_prebld_en);
	for (slice = 0; slice < vd_proc_vd1_info->slice_num; slice++) {
		vd_proc_unit = &vd_proc->vd_proc_unit[slice];
		len += sprintf(buf + len, "vd proc slice[%d] input: 0x%x, 0x%x\n",
			slice,
			vd_proc_unit->din_hsize,
			vd_proc_unit->din_vsize);
		len += sprintf(buf + len, "vd proc slice[%d] output: 0x%x, 0x%x\n",
			slice,
			vd_proc_unit->dout_hsize,
			vd_proc_unit->dout_vsize);
		len += sprintf(buf + len, "vd proc slice[%d] slice input: 0x%x, 0x%x\n",
			slice,
			vd_proc_unit->vd_proc_slice.din_hsize,
			vd_proc_unit->vd_proc_slice.din_vsize);
		len += sprintf(buf + len, "vd proc slice[%d] slice output: 0x%x, 0x%x\n",
			slice,
			vd_proc_unit->vd_proc_slice.dout_hsize,
			vd_proc_unit->vd_proc_slice.dout_vsize);
		if (vd_proc_vd1_info->slice_num > 2) {
			if (slice == 0)
				len += sprintf(buf + len, "more than 2 slice, sr top disable\n");
		} else {
			if (vd_proc_unit->sr0_pps_dpsel == SR0_BEFORE_PPS) {
				if (slice == 0 &&
					vd_proc_unit->sr0_dpath_sel == SR0_IN_SLICE0) {
					len += sprintf(buf + len,
						"SR0_IN_SLICE0 sr0 din: 0x%x, 0x%x\n",
						vd_proc_unit->vd_proc_sr0.din_hsize,
						vd_proc_unit->vd_proc_sr0.din_vsize);
					len += sprintf(buf + len, "sr0 dout: 0x%x, 0x%x\n",
						vd_proc_unit->vd_proc_sr0.dout_hsize,
						vd_proc_unit->vd_proc_sr0.dout_vsize);
				} else if (slice == 1 &&
					vd_proc_unit->sr0_dpath_sel == SR0_IN_SLICE1) {
					len += sprintf(buf + len,
						"SR0_IN_SLICE1 sr0 din: 0x%x, 0x%x\n",
						vd_proc_unit->vd_proc_sr0.din_hsize,
						vd_proc_unit->vd_proc_sr0.din_vsize);
					len += sprintf(buf + len, "sr0 dout: 0x%x, 0x%x\n",
						vd_proc_unit->vd_proc_sr0.dout_hsize,
						vd_proc_unit->vd_proc_sr0.dout_vsize);
				}
			}
		}
		len += sprintf(buf + len, "vd proc slice[%d] pps input: 0x%x, 0x%x\n",
			slice,
			vd_proc_unit->vd_proc_pps.din_hsize,
			vd_proc_unit->vd_proc_pps.din_vsize);
		len += sprintf(buf + len, "vd proc slice[%d] pps output: 0x%x, 0x%x\n",
			slice,
			vd_proc_unit->vd_proc_pps.dout_hsize,
			vd_proc_unit->vd_proc_pps.dout_vsize);
		if (vd_proc_vd1_info->slice_num <= 2) {
			if (vd_proc_unit->sr0_pps_dpsel == SR0_AFTER_PPS) {
				if (slice == 0 &&
					vd_proc_unit->sr0_dpath_sel == SR0_IN_SLICE0) {
					len += sprintf(buf + len,
						"SR0_IN_SLICE0 sr0 din: 0x%x, 0x%x\n",
						vd_proc_unit->vd_proc_sr0.din_hsize,
						vd_proc_unit->vd_proc_sr0.din_vsize);
					len += sprintf(buf + len, "sr0 dout: 0x%x, 0x%x\n",
						vd_proc_unit->vd_proc_sr0.dout_hsize,
						vd_proc_unit->vd_proc_sr0.dout_vsize);
				} else if (slice == 1 &&
					vd_proc_unit->sr0_dpath_sel == SR0_IN_SLICE1) {
					len += sprintf(buf + len,
						"SR0_IN_SLICE1 sr0 din: 0x%x, 0x%x\n",
						vd_proc_unit->vd_proc_sr0.din_hsize,
						vd_proc_unit->vd_proc_sr0.din_vsize);
					len += sprintf(buf + len, "sr0 dout: 0x%x, 0x%x\n",
						vd_proc_unit->vd_proc_sr0.dout_hsize,
						vd_proc_unit->vd_proc_sr0.dout_vsize);
				}
			}
			if (slice == 0) {
				len += sprintf(buf + len, "sr1 din: 0x%x, 0x%x\n",
					vd_proc_unit->vd_proc_sr1.din_hsize,
					vd_proc_unit->vd_proc_sr1.din_vsize);
				len += sprintf(buf + len, "sr1 dout: 0x%x, 0x%x\n",
					vd_proc_unit->vd_proc_sr1.dout_hsize,
					vd_proc_unit->vd_proc_sr1.dout_vsize);
			}
		}
		len += sprintf(buf + len,
			"vd proc slice[%d] hwcut en: 0x%x, din:0x%x, hwin_bgn:0x%x, hwin_end:0x%x\n",
			slice,
			vd_proc_unit->vd_proc_hwin.hwin_en,
			vd_proc_unit->vd_proc_hwin.hwin_din_hsize,
			vd_proc_unit->vd_proc_hwin.hwin_bgn,
			vd_proc_unit->vd_proc_hwin.hwin_end);
		if (vd_proc_unit->vd_proc_padding.padding_en)
			len += sprintf(buf + len,
				"vd proc slice[%d] padding axis:0x%x, 0x%x, 0x%x, 0x%x\n",
				slice,
				vd_proc_unit->vd_proc_padding.slice_pad_h_bgn,
				vd_proc_unit->vd_proc_padding.slice_pad_v_bgn,
				vd_proc_unit->vd_proc_padding.slice_pad_h_end,
				vd_proc_unit->vd_proc_padding.slice_pad_v_end);
	}
	if (vd_proc->vd_proc_pi.pi_en)
		len += sprintf(buf + len, "vd proc pi input: 0x%x, 0x%x\n",
			vd_proc->vd_proc_pi.pi_in_hsize,
			vd_proc->vd_proc_pi.pi_in_vsize);
	return len;
}

void set_module_bypass_s5(u32 bypass_module)
{
	g_bypass_module = bypass_module;
}

int get_module_bypass_s5(void)
{
	return g_bypass_module;
}

u32 get_slice_num(u32 layer_id)
{
	if (layer_id >= MAX_VD_CHAN_S5)
		return 1;
	else
		return vd_layer[layer_id].slice_num;
}

u32 get_pi_enabled(u32 layer_id)
{
	if (layer_id >= MAX_VD_CHAN_S5)
		return 1;
	else
		return vd_layer[layer_id].pi_enable;
}

int vpp_crc_check_s5(u32 vpp_crc_en, u8 vpp_index)
{
	u32 val = 0;
	int vpp_crc_result = 0;
	static u32 val_pre, crc_cnt;

	if (vpp_crc_en) {
		cur_dev->rdma_func[vpp_index].rdma_wr(S5_VPP_CRC_CHK, 1);
		if (crc_cnt >= 1) {
			val = cur_dev->rdma_func[vpp_index].rdma_rd(S5_VPP_RO_CRCSUM);
			if (val_pre && val != val_pre)
				vpp_crc_result = -1;
			else
				vpp_crc_result = val;
		}
		val_pre = val;
		crc_cnt++;
	} else {
		crc_cnt  = 0;
	}
	return vpp_crc_result;
}

static void disable_vd1_slice_blend_s5(struct video_layer_s *layer, u8 slice)
{
	u8 vpp_index;
	struct vd_afbc_reg_s *vd_afbc_reg = NULL;
	struct vd_mif_reg_s *vd_mif_reg = NULL;

	if (!layer)
		return;
	vpp_index = layer->vpp_index;
	if (layer->layer_id != 0 || slice >= SLICE_NUM)
		return;

	vd_afbc_reg = &vd_proc_reg.vd_afbc_reg[slice];
	vd_mif_reg = &vd_proc_reg.vd_mif_reg[slice];

	if (layer->global_debug & DEBUG_FLAG_BASIC_INFO)
		pr_info("VIDEO: VD1 AFBC off now. dispbuf:%p vf_ext:%p, *dispbuf_mapping:%p, local: %p, %p, %p, %p\n",
			layer->dispbuf,
			layer->vf_ext,
			layer->dispbuf_mapping ?
			*layer->dispbuf_mapping : NULL,
			&vf_local, &local_pip,
			gvideo_recv[0] ? &gvideo_recv[0]->local_buf : NULL,
			gvideo_recv[1] ? &gvideo_recv[1]->local_buf : NULL);

	cur_dev->rdma_func[vpp_index].rdma_wr
		(vd_afbc_reg->afbc_enable, 0);
	cur_dev->rdma_func[vpp_index].rdma_wr
		(vd_mif_reg->vd_if0_gen_reg, 0);

	if (layer->dispbuf &&
	    is_local_vf(layer->dispbuf)) {
		layer->dispbuf = NULL;
		layer->vf_ext = NULL;
	}
	if (layer->dispbuf_mapping) {
		if (*layer->dispbuf_mapping &&
		    is_local_vf(*layer->dispbuf_mapping))
			*layer->dispbuf_mapping = NULL;
		layer->dispbuf_mapping = NULL;
		layer->dispbuf = NULL;
		layer->vf_ext = NULL;
	}
	layer->new_vframe_count = 0;
}

void disable_vd1_blend_s5(struct video_layer_s *layer)
{
	u8 vpp_index;
	struct vd_afbc_reg_s *vd_afbc_reg = NULL;
	struct vd_mif_reg_s *vd_mif_reg = NULL;
	u8 layer_index = 0, slice = 0;

	if (!layer)
		return;
	if (layer->layer_id == 0 && layer->slice_num > 1) {
		for (slice = 0; slice < layer->slice_num; slice++)
			disable_vd1_slice_blend_s5(layer, slice);
		return;
	}

	vpp_index = layer->vpp_index;
	if (layer->layer_id >= MAX_VD_CHAN_S5)
		return;
	if (!layer->layer_id)
		layer_index = 0;
	else
		layer_index = layer->layer_id + SLICE_NUM - 1;
	vd_afbc_reg = &vd_proc_reg.vd_afbc_reg[layer_index];
	vd_mif_reg = &vd_proc_reg.vd_mif_reg[layer_index];


	if (layer->global_debug & DEBUG_FLAG_BASIC_INFO)
		pr_info("VIDEO: VD1 AFBC off now. dispbuf:%p vf_ext:%p, *dispbuf_mapping:%p, local: %p, %p, %p, %p\n",
			layer->dispbuf,
			layer->vf_ext,
			layer->dispbuf_mapping ?
			*layer->dispbuf_mapping : NULL,
			&vf_local, &local_pip,
			gvideo_recv[0] ? &gvideo_recv[0]->local_buf : NULL,
			gvideo_recv[1] ? &gvideo_recv[1]->local_buf : NULL);

	cur_dev->rdma_func[vpp_index].rdma_wr
		(vd_afbc_reg->afbc_enable, 0);
	cur_dev->rdma_func[vpp_index].rdma_wr
		(vd_mif_reg->vd_if0_gen_reg, 0);

	if (layer->dispbuf &&
	    is_local_vf(layer->dispbuf)) {
		layer->dispbuf = NULL;
		layer->vf_ext = NULL;
	}
	if (layer->dispbuf_mapping) {
		if (*layer->dispbuf_mapping &&
		    is_local_vf(*layer->dispbuf_mapping))
			*layer->dispbuf_mapping = NULL;
		layer->dispbuf_mapping = NULL;
		layer->dispbuf = NULL;
		layer->vf_ext = NULL;
	}
	layer->new_vframe_count = 0;
}

void disable_vd2_blend_s5(struct video_layer_s *layer)
{
	u8 vpp_index;
	u32 layer_id = 0;
	struct vd_afbc_reg_s *vd_afbc_reg = NULL;
	struct vd_mif_reg_s *vd_mif_reg = NULL;

	if (!layer)
		return;
	vpp_index = layer->vpp_index;
	if (layer->layer_id != 0)
		layer_id = layer->layer_id + SLICE_NUM - 1;

	vd_afbc_reg = &vd_proc_reg.vd_afbc_reg[layer_id];
	vd_mif_reg = &vd_proc_reg.vd_mif_reg[layer_id];

	if (layer->global_debug & DEBUG_FLAG_BASIC_INFO)
		pr_info("VIDEO: VD2 AFBC off now. dispbuf:%p, vf_ext:%p, *dispbuf_mapping:%p, local: %p, %p, %p, %p\n",
			layer->dispbuf,
			layer->vf_ext,
			layer->dispbuf_mapping ?
			*layer->dispbuf_mapping : NULL,
			&vf_local, &local_pip,
			gvideo_recv[0] ? &gvideo_recv[0]->local_buf : NULL,
			gvideo_recv[1] ? &gvideo_recv[1]->local_buf : NULL);
	cur_dev->rdma_func[vpp_index].rdma_wr(vd_afbc_reg->afbc_enable, 0);
	cur_dev->rdma_func[vpp_index].rdma_wr(vd_mif_reg->vd_if0_gen_reg, 0);

	if (layer->dispbuf &&
	    is_local_vf(layer->dispbuf)) {
		layer->dispbuf = NULL;
		layer->vf_ext = NULL;
	}
	if (layer->dispbuf_mapping) {
		if (*layer->dispbuf_mapping &&
		    is_local_vf(*layer->dispbuf_mapping))
			*layer->dispbuf_mapping = NULL;
		layer->dispbuf_mapping = NULL;
		layer->dispbuf = NULL;
		layer->vf_ext = NULL;
	}
	/* FIXME: remove global variables */
	last_el_status = 0;
	need_disable_vd2 = false;
	layer->new_vframe_count = 0;
}

static void vpu_module_clk_disable_s5(u32 vpp_index, u32 module, bool async)
{
}

static void vpu_module_clk_enable_s5(u32 vpp_index, u32 module, bool async)
{
}

static int vd_proc_get_slice_num(void)
{
	struct vd_proc_s *vd_proc = get_vd_proc_info();

	return vd_proc->vd_proc_vd1_info.slice_num;
}
/* hw reg set */
static void vd_proc_slice_set(u32 vpp_index,
	u32 slice_index,
	struct vd_proc_slice_s *vd_slice)
{
	rdma_wr_op rdma_wr = cur_dev->rdma_func[vpp_index].rdma_wr;
	struct vd_proc_slice_reg_s *vd_slice_reg = NULL;

	vd_slice_reg = &vd_proc_reg.vd_proc_slice_reg[slice_index];
	rdma_wr(vd_slice_reg->vd_proc_s0_in_size,
		vd_slice->din_hsize << 16 | vd_slice->din_vsize);
	/* NOTE:::The reg is to set size before hwincut */
	rdma_wr(vd_slice_reg->vd_proc_s0_out_size,
		vd_slice->dout_hsize << 16 | vd_slice->dout_vsize);
	if (debug_flag_s5 & DEBUG_VD_PROC)
		pr_info("%s:vd_proc_s0_in_size: %x, vd_proc_s0_out_size: %x\n",
			__func__,
			vd_slice->din_hsize << 16 | vd_slice->din_vsize,
			vd_slice->dout_hsize << 16 | vd_slice->dout_vsize);
}

static void vd_proc_bypass_detunnel(u32 vpp_index,
	u32 slice_index, bool bypass)
{
	rdma_wr_bits_op rdma_wr_bits = cur_dev->rdma_func[vpp_index].rdma_wr_bits;
	struct vd_proc_slice_reg_s *vd_slice_reg = NULL;

	vd_slice_reg = &vd_proc_reg.vd_proc_slice_reg[slice_index];
	if (bypass)
		rdma_wr_bits(vd_slice_reg->vd_s0_detunl_ctrl, !bypass, 0, 1);
}

static void vd_proc_bypass_dv(u32 vpp_index,
	u32 slice_index, bool bypass)
{
	rdma_wr_bits_op rdma_wr_bits = cur_dev->rdma_func[vpp_index].rdma_wr_bits;
	struct vd_proc_slice_reg_s *vd_slice_reg = NULL;

	vd_slice_reg = &vd_proc_reg.vd_proc_slice_reg[slice_index];
	if (bypass)
		rdma_wr_bits(vd_slice_reg->vd1_s0_dv_bypass_ctrl, !bypass, 0, 1);
}

static void vd_proc_bypass_hdr(u32 vpp_index,
	u32 slice_index, bool bypass)
{
	rdma_wr_bits_op rdma_wr_bits = cur_dev->rdma_func[vpp_index].rdma_wr_bits;
	struct vd_proc_slice_reg_s *vd_slice_reg = NULL;

	vd_slice_reg = &vd_proc_reg.vd_proc_slice_reg[slice_index];
	if (bypass)
		rdma_wr_bits(vd_slice_reg->vd1_hdr_s0_ctrl, !bypass, 13, 1);
}

static inline void vd_proc_bypass_ve(u32 vpp_index,
	u32 slice_index, bool bypass)
{
	rdma_wr_bits_op rdma_wr_bits = cur_dev->rdma_func[vpp_index].rdma_wr_bits;
	if (bypass)
		rdma_wr_bits(VD_PROC_BYPASS_CTRL, bypass, 1 + slice_index, 1);
}

static inline void vd_proc_bypass_preblend(u32 vpp_index,
	struct vd_proc_s *vd_proc)
{
	rdma_wr_bits_op rdma_wr_bits = cur_dev->rdma_func[vpp_index].rdma_wr_bits;

	/* Bit 5: reg_bypass_prebld1 0:use vd prebld 1:bypass vd prebld */
	/* Bit 0: reg_bypass_prebld0 0:use vd prebld 1:bypass vd prebld */
	if (debug_flag_s5 & DEBUG_VD_PROC)
		pr_info("%s:reg_bypass_prebld: %d, %d\n",
			__func__,
			vd_proc->vd_proc_unit[0].reg_bypass_prebld,
			vd_proc->vd_proc_unit[1].reg_bypass_prebld);

	rdma_wr_bits(VD_PROC_BYPASS_CTRL,
		vd_proc->vd_proc_unit[0].reg_bypass_prebld, 0, 1);
	rdma_wr_bits(VD_PROC_BYPASS_CTRL,
		vd_proc->vd_proc_unit[1].reg_bypass_prebld, 5, 1);
}

static void vd_proc_sr0_set(u32 vpp_index,
	u32 slice_index,
	struct vd_proc_sr_s *vd_sr)
{
	rdma_rd_op rdma_rd = cur_dev->rdma_func[vpp_index].rdma_rd;
	rdma_wr_op rdma_wr = cur_dev->rdma_func[vpp_index].rdma_wr;
	rdma_wr_bits_op rdma_wr_bits = cur_dev->rdma_func[vpp_index].rdma_wr_bits;
	struct vd_proc_sr_reg_s *vd_sr_reg = NULL;
	u32 tmp_data = 0;
	int sr_core0_max_width = 0;
	int super_scaler = 0;

	vd_sr_reg = &vd_proc_reg.vd_proc_sr_reg;
	if (slice_index == 0)
		rdma_wr(vd_sr_reg->vd_proc_s0_sr0_in_size,
			vd_sr->din_hsize << 16 |
			vd_sr->din_vsize);
	else if (slice_index == 1)
		rdma_wr(vd_sr_reg->vd_proc_s1_sr0_in_size,
			vd_sr->din_hsize << 16 |
			vd_sr->din_vsize);
	if (vd_sr->v_scaleup_en == 0)
		sr_core0_max_width = vd_sr->core_v_disable_width_max;
	else
		sr_core0_max_width = vd_sr->core_v_enable_width_max;

	if (debug_flag_s5 & DEBUG_VD_PROC)
		pr_info("%s:vd_proc_s0_sr0_in_size: %x, vd_proc_s1_sr0_in_size: %x\n",
			__func__,
			vd_sr->din_hsize << 16 |
			vd_sr->din_vsize,
			vd_sr->din_hsize << 16 |
			vd_sr->din_vsize);

	/* top config */
	tmp_data = rdma_rd(vd_sr_reg->vd_proc_sr0_ctrl);

	if (vd_sr->din_hsize > sr_core0_max_width) {
		if (((tmp_data >> 1) & 0x1) != 0)
			rdma_wr_bits(vd_sr_reg->vd_proc_sr0_ctrl,
					       0, 1, 1);
		if ((tmp_data & 0x1) != 0)
			rdma_wr_bits(vd_sr_reg->vd_proc_sr0_ctrl,
					       0, 0, 1);
		vpu_module_clk_disable_s5(VPP0, SR0, 0);
	} else {
		if (((tmp_data >> 1) & 0x1) != 1)
			rdma_wr_bits(vd_sr_reg->vd_proc_sr0_ctrl,
					       1, 1, 1);
		if ((tmp_data & 0x1) != 1)
			vpu_module_clk_enable_s5(VPP0, SR0, 0);
		rdma_wr_bits(vd_sr_reg->vd_proc_sr0_ctrl, 1, 0, 1);
	}

	/* core0 config */
	tmp_data = rdma_rd(vd_sr_reg->srsharp0_sharp_sr2_ctrl);
	if (vd_sr->sr_en) {
		if ((((tmp_data >> 5) & 0x1) !=
			(vd_sr->v_scaleup_en & 0x1)) ||
			(((tmp_data >> 4) & 0x1) !=
			(vd_sr->h_scaleup_en & 0x1)) ||
			(((tmp_data >> 2) & 0x1) != 1)) {
			tmp_data &= ~0x34;
			tmp_data |= ((vd_sr->v_scaleup_en & 0x1) << 5);
			tmp_data |= ((vd_sr->h_scaleup_en & 0x1) << 4);
			tmp_data |= (1 << 2);
		}
	} else {
		tmp_data &= ~0x34;
	}
	rdma_wr(vd_sr_reg->srsharp0_sharp_sr2_ctrl, tmp_data);
	rdma_wr(vd_sr_reg->srsharp0_sharp_sr2_ctrl2, 0x0);

	super_scaler = get_super_scaler_status();
	if (vd_proc_get_slice_num() == SLICE_NUM)
		super_scaler = 0;
	if (!(vd_sr->sr_support & SUPER_CORE0_SUPPORT) ||
		!super_scaler)
		rdma_wr_bits(vd_sr_reg->vd_proc_sr0_ctrl,
			0, 0, 2);
}

static void vd_proc_sr1_set(u32 vpp_index,
	u32 slice_index,
	struct vd_proc_sr_s *vd_sr)
{
	rdma_rd_op rdma_rd = cur_dev->rdma_func[vpp_index].rdma_rd;
	rdma_wr_op rdma_wr = cur_dev->rdma_func[vpp_index].rdma_wr;
	rdma_wr_bits_op rdma_wr_bits = cur_dev->rdma_func[vpp_index].rdma_wr_bits;
	struct vd_proc_sr_reg_s *vd_sr_reg = NULL;
	u32 tmp_data = 0;
	u32 sr_core1_max_width = 0;
	int super_scaler = 0;

	vd_sr_reg = &vd_proc_reg.vd_proc_sr_reg;

	rdma_wr(vd_sr_reg->vd_proc_s0_sr1_in_size,
		vd_sr->din_hsize << 16 |
		vd_sr->din_vsize);

	if (vd_sr->v_scaleup_en == 0)
		sr_core1_max_width = vd_sr->core_v_disable_width_max;
	else
		sr_core1_max_width = vd_sr->core_v_enable_width_max;
	/* top config */
	tmp_data = rdma_rd(vd_sr_reg->vd_proc_sr1_ctrl);

	if (vd_sr->din_hsize > sr_core1_max_width) {
		if (((tmp_data >> 1) & 0x1) != 0)
			rdma_wr_bits(vd_sr_reg->vd_proc_sr1_ctrl,
					       0, 1, 1);
		if ((tmp_data & 0x1) != 0)
			rdma_wr_bits(vd_sr_reg->vd_proc_sr1_ctrl,
					       0, 0, 1);
		if (debug_flag_s5 & DEBUG_SR)
			pr_info("%s:disable sr1 core tmp_data: %x\n",
				__func__,
				tmp_data);
		vpu_module_clk_disable_s5(VPP0, SR1, 0);
	} else {
		if (debug_flag_s5 & DEBUG_SR)
			pr_info("%s:enable sr1 core tmp_data: %x\n",
				__func__,
				tmp_data);
		if (((tmp_data >> 1) & 0x1) != 1)
			rdma_wr_bits(vd_sr_reg->vd_proc_sr1_ctrl,
					       1, 1, 1);
		if ((tmp_data & 0x1) != 1)
			vpu_module_clk_enable_s5(VPP0, SR1, 0);
		rdma_wr_bits(vd_sr_reg->vd_proc_sr1_ctrl, 1, 0, 1);
	}

	if (cur_dev->aisr_support)
		rdma_wr_bits(vd_sr_reg->vd_proc_sr1_ctrl, 3, 4, 2);
	tmp_data = rdma_rd(vd_sr_reg->srsharp1_sharp_sr2_ctrl);
	if (vd_sr->sr_en) {
		if ((((tmp_data >> 5) & 0x1) !=
			(vd_sr->v_scaleup_en & 0x1)) ||
			(((tmp_data >> 4) & 0x1) !=
			(vd_sr->h_scaleup_en & 0x1)) ||
			(((tmp_data >> 2) & 0x1) != 1)) {
			tmp_data &= ~0x34;
			tmp_data |= ((vd_sr->v_scaleup_en & 0x1) << 5);
			tmp_data |= ((vd_sr->h_scaleup_en & 0x1) << 4);
			tmp_data |= (1 << 2);
		}
	} else {
		tmp_data &= ~0x34;
	}
	rdma_wr(vd_sr_reg->srsharp1_sharp_sr2_ctrl, tmp_data);
	if (debug_flag_s5 & DEBUG_VD_PROC)
		pr_info("%s:sr_en:%d, vd_proc_s0_sr1_in_size: %x, sr support=%d\n",
			__func__,
			vd_sr->sr_en,
			vd_sr->din_hsize << 16 |
			vd_sr->din_vsize,
			vd_sr->sr_support);
	super_scaler = get_super_scaler_status();
	if (vd_proc_get_slice_num() == SLICE_NUM)
		super_scaler = 0;
	if (!(vd_sr->sr_support & SUPER_CORE1_SUPPORT) ||
		!super_scaler)
		rdma_wr_bits(vd_sr_reg->vd_proc_sr1_ctrl,
			0, 0, 2);
	// disable ai-sr
	if (!cur_dev->aisr_enable)
		aisr_sr1_nn_enable_s5(0);
}

static void vd_proc_hwin_set(u32 vpp_index,
	u32 slice_index,
	struct vd_proc_hwin_s *vd_hwin)
{
	rdma_wr_op rdma_wr = cur_dev->rdma_func[vpp_index].rdma_wr;
	struct vd_proc_slice_reg_s *vd_slice_reg = NULL;

	if (slice_index >= SLICE_NUM)
		return;
	if (debug_flag_s5 & DEBUG_VD_PROC)
		pr_info("%s: hwin_en=%d, hwin_bgn=%d, hwin_end=%d\n",
			__func__,
			vd_hwin->hwin_en, vd_hwin->hwin_bgn, vd_hwin->hwin_end);
	vd_slice_reg = &vd_proc_reg.vd_proc_slice_reg[slice_index];
	rdma_wr(vd_slice_reg->vd_s0_hwin_cut,
		vd_hwin->hwin_en << 29 |
		vd_hwin->hwin_bgn << 16 |
		vd_hwin->hwin_end);
}

static void vd_proc_pi_set(u32 vpp_index,
	struct vd_proc_pi_s *vd_pi)
{
	rdma_wr_bits_op rdma_wr_bits = cur_dev->rdma_func[vpp_index].rdma_wr_bits;
	struct vd_proc_pi_reg_s *vd_pi_reg = NULL;

	vd_pi_reg = &vd_proc_reg.vd_proc_pi_reg;

	rdma_wr_bits(vd_pi_reg->vd_proc_pi_ctrl, vd_pi->pi_en, 16, 1);
}

static void vd_proc_unit_set(u32 vpp_index,
	struct vd_proc_unit_s *vd_proc_unit)
{
	rdma_wr_bits_op rdma_wr_bits = cur_dev->rdma_func[vpp_index].rdma_wr_bits;
	int slice_index = vd_proc_unit->slice_index;
	struct vd_proc_misc_reg_s *vd_misc_reg = NULL;

	if (slice_index >= SLICE_NUM)
		return;
	vd_misc_reg = &vd_proc_reg.vd_proc_misc_reg;

	vd_proc_slice_set(vpp_index, slice_index,
		&vd_proc_unit->vd_proc_slice);
	/* detunnel*/
	vd_proc_bypass_detunnel(vpp_index, slice_index,
		vd_proc_unit->bypass_detunnel);
	/* hdr */
	vd_proc_bypass_hdr(vpp_index, slice_index, vd_proc_unit->bypass_hdr);
	/* dv */
	vd_proc_bypass_dv(vpp_index, slice_index, vd_proc_unit->bypass_dv);
	/* sr0 */
	vpu_module_clk_enable_s5(vd_layer[0].vpp_index, SR0, 0);
	vpu_module_clk_enable_s5(vd_layer[0].vpp_index, SR1, 0);
	if (slice_index == 0 &&
		vd_proc_unit->sr0_dpath_sel == SR0_IN_SLICE0) {
		vd_proc_sr0_set(vpp_index, slice_index,
			&vd_proc_unit->vd_proc_sr0);
		/* 0:SR0 after PPS 1:SR0 before PPS */
		rdma_wr_bits(vd_misc_reg->vd_proc_sr0_ctrl,
			vd_proc_unit->sr0_pps_dpsel, 31, 1);
	} else if (slice_index == 1 &&
		vd_proc_unit->sr0_dpath_sel == SR0_IN_SLICE1) {
		vd_proc_sr0_set(vpp_index, slice_index,
			&vd_proc_unit->vd_proc_sr0);
	}
	/* SR0 in vd1 slice0 1:SR0 in vd1 slice1 */
	rdma_wr_bits(vd_misc_reg->vd_proc_sr0_ctrl,
		vd_proc_unit->sr0_dpath_sel, 30, 1);

	/* sr1 */
	if (slice_index == 0)
		vd_proc_sr1_set(vpp_index, slice_index,
			&vd_proc_unit->vd_proc_sr1);
	/* ve */
	//vd_proc_bypass_ve(vpp_index, slice_index, vd_proc_unit->bypass_ve);
	/* hwicut */
	vd_proc_hwin_set(vpp_index, slice_index,
		&vd_proc_unit->vd_proc_hwin);
	/* dv */
}

static void vd_proc_pi_path_set(u32 vpp_index, struct vd_proc_s *vd_proc)
{
	rdma_wr_op rdma_wr = cur_dev->rdma_func[vpp_index].rdma_wr;
	struct vd_proc_misc_reg_s *vd_misc_reg = NULL;
	u32 pi_enalbe = 0;

	vd_misc_reg = &vd_proc_reg.vd_proc_misc_reg;
	if (vd_proc->vd_proc_vd1_info.vd1_slices_dout_dpsel ==
		VD1_SLICES_DOUT_PI)
		pi_enalbe = 1;

	rdma_wr(vd_misc_reg->vd1_pi_ctrl, pi_enalbe);
	if (debug_flag_s5 & DEBUG_VD_PROC)
		pr_info("%s: pi_enalbe=%x\n",
			__func__, pi_enalbe);
}

static void vd_slices_padding_set(u32 vpp_index,
	struct vd_proc_s *vd_proc)
{
	u32 vd1_work_mode;
	u32 vd1_slices_dout_dpsel;
	u32 vd1_proc_unit_dout_hsize = 0, vd1_proc_unit_dout_vsize = 0;
	int slice = 0;
	u32 slice_pad_ena[4] = {0, 0, 0, 0};
	u32 slice_pad_h_bgn[4] = {0, 0, 0, 0};
	u32 slice_pad_h_end[4] = {0, 0, 0, 0};
	u32 slice_pad_v_bgn[4] = {0, 0, 0, 0};
	u32 slice_pad_v_end[4] = {0, 0, 0, 0};
	u32 vd1_slice_pad_rpt_last_col = 0;
	struct vd1_slice_pad_reg_s *vd1_slice_pad_reg;
	struct vd_proc_vd1_info_s *vd_proc_vd1_info = NULL;
	struct vd_proc_vd2_info_s *vd_proc_vd2_info = NULL;
	struct vd_proc_padding_s *vd_proc_padding = NULL;
	rdma_wr_op rdma_wr = cur_dev->rdma_func[vpp_index].rdma_wr;

	vd1_work_mode = vd_proc->vd_proc_vd1_info.vd1_work_mode;
	vd1_slices_dout_dpsel = vd_proc->vd_proc_vd1_info.vd1_slices_dout_dpsel;
	vd_proc_vd1_info = &vd_proc->vd_proc_vd1_info;
	vd_proc_vd2_info = &vd_proc->vd_proc_vd2_info;
	/* 1: pad with last col before to slice4ppc;
	 * 0: slice pad with dummy data before to slice4ppc
	 */
	vd1_slice_pad_rpt_last_col = 1;
	/* If each proc_unit output hsize < ALIG32(vd1_dout_hsize)/4,
	 * then must padding to ALIG32(vd1_dout_hsize)/4
	 */
	if (vd1_work_mode == VD1_4SLICES_MODE &&
		vd1_slices_dout_dpsel == VD1_SLICES_DOUT_4S4P) {
		/* 4slices to 4ppc */
		for (slice = 0; slice < SLICE_NUM; slice++) {
			vd1_slice_pad_reg = &vd_proc_reg.vd1_slice_pad_size0_reg[slice];
			vd_proc_padding = &vd_proc->vd_proc_unit[slice].vd_proc_padding;
			vd1_proc_unit_dout_hsize =
				vd_proc_vd1_info->vd1_proc_unit_dout_hsize[slice];
			vd1_proc_unit_dout_vsize =
				vd_proc_vd1_info->vd1_proc_unit_dout_vsize[slice];
			if (vd1_proc_unit_dout_hsize <
				SIZE_ALIG32(vd_proc_vd1_info->vd1_dout_hsize) / SLICE_NUM) {
				slice_pad_ena[slice] = 1;
				slice_pad_h_bgn[slice] = 0;
				slice_pad_h_end[slice] =
					vd1_proc_unit_dout_hsize - 1;
				slice_pad_v_bgn[slice] = 0;
				slice_pad_v_end[slice] =
					vd1_proc_unit_dout_vsize - 1;
				vd_proc_padding->padding_en = slice_pad_ena[slice];
				vd_proc_padding->slice_pad_h_bgn = slice_pad_h_bgn[slice];
				vd_proc_padding->slice_pad_v_bgn = slice_pad_v_bgn[slice];
				vd_proc_padding->slice_pad_h_end = slice_pad_h_end[slice];
				vd_proc_padding->slice_pad_v_end = slice_pad_v_end[slice];
				if (debug_flag_s5 & DEBUG_VD_PROC) {
					pr_info("%s:slice %d need padding, padding axis: %x, %x, %x, %x\n",
						__func__, slice,
						slice_pad_h_bgn[slice],
						slice_pad_h_end[slice],
						slice_pad_v_bgn[slice],
						slice_pad_v_end[slice]);
				}
			}
			rdma_wr(vd1_slice_pad_reg->vd1_slice_pad_h_size,
				slice_pad_h_bgn[slice] << 16 |
				slice_pad_h_end[slice]);
			rdma_wr(vd1_slice_pad_reg->vd1_slice_pad_v_size,
				slice_pad_v_bgn[slice] << 16 |
				slice_pad_v_end[slice]);
		}
	}
	if (vd1_work_mode == VD1_SLICES01_MODE ||
		vd1_work_mode == VD1_2_2SLICES_MODE) {
		/* 2slices to 4ppc or 4s4p mosaic mode */
		for (slice = 0; slice < 2; slice++) {
			vd1_slice_pad_reg = &vd_proc_reg.vd1_slice_pad_size0_reg[slice];
			vd1_proc_unit_dout_hsize =
				vd_proc_vd1_info->vd1_proc_unit_dout_hsize[slice];
			vd1_proc_unit_dout_vsize =
				vd_proc_vd1_info->vd1_proc_unit_dout_vsize[slice];
			if (vd1_proc_unit_dout_hsize <
				SIZE_ALIG16(vd_proc_vd1_info->vd1_dout_hsize) / 2) {
				slice_pad_ena[slice] = 1;
				slice_pad_h_bgn[slice] = 0;
				slice_pad_h_end[slice] =
					vd1_proc_unit_dout_hsize - 1;
				slice_pad_v_bgn[slice] = 0;
				slice_pad_v_end[slice] =
					vd1_proc_unit_dout_vsize - 1;
			}
			rdma_wr(vd1_slice_pad_reg->vd1_slice_pad_h_size,
				slice_pad_h_bgn[slice] << 16 |
				slice_pad_h_end[slice]);
			rdma_wr(vd1_slice_pad_reg->vd1_slice_pad_v_size,
				slice_pad_v_bgn[slice] << 16 |
				slice_pad_v_end[slice]);
		}
	}

	if (vd1_work_mode == VD1_2_2SLICES_MODE) {
		/* 4s4p mosaic mode */
		for (slice = 2; slice < SLICE_NUM; slice++) {
			vd1_slice_pad_reg = &vd_proc_reg.vd1_slice_pad_size0_reg[slice];
			vd1_proc_unit_dout_hsize =
				vd_proc_vd1_info->vd1_proc_unit_dout_hsize[slice];
			vd1_proc_unit_dout_vsize =
				vd_proc_vd1_info->vd1_proc_unit_dout_vsize[slice];
			if (vd1_proc_unit_dout_hsize <
				SIZE_ALIG16(vd_proc_vd2_info->vd2_dout_hsize) / 2) {
				slice_pad_ena[slice] = 1;
				slice_pad_h_bgn[slice] = 0;
				slice_pad_h_end[slice] =
					vd1_proc_unit_dout_hsize - 1;
				slice_pad_v_bgn[slice] = 0;
				slice_pad_v_end[slice] =
					vd1_proc_unit_dout_vsize - 1;
			}
			rdma_wr(vd1_slice_pad_reg->vd1_slice_pad_h_size,
				slice_pad_h_bgn[slice] << 16 |
				slice_pad_h_end[slice]);
			rdma_wr(vd1_slice_pad_reg->vd1_slice_pad_v_size,
				slice_pad_v_bgn[slice] << 16 |
				slice_pad_v_end[slice]);
		}
	}

	rdma_wr(VD1_PAD_CTRL,
		slice_pad_ena[0] << 0 |
		slice_pad_ena[1] << 1 |
		slice_pad_ena[2] << 2 |
		slice_pad_ena[3] << 3 |
		vd1_slice_pad_rpt_last_col << 4 |
		vd1_slice_pad_rpt_last_col << 5 |
		vd1_slice_pad_rpt_last_col << 6 |
		vd1_slice_pad_rpt_last_col << 7 |
		vd_proc_vd1_info->vd1_dout_vsize << 16);
}

static void vd_mosaic_slices_padding_set(u32 vpp_index,
	u32 frm_idx,
	struct vd_proc_s *vd_proc)
{
	int slice = 0, vd1_slice_pad_rpt_last_col = 1;
	u32 VD1_SLICE_PAD_H_SIZE;
	u32 VD1_SLICE_PAD_V_SIZE;
	struct vd_proc_mosaic_s *vd_proc_mosaic;
	rdma_wr_op rdma_wr = cur_dev->rdma_func[vpp_index].rdma_wr;

	/* frm_idx=0: for PCI0,PIC1, frm_idx=1: for PIC2,PIC3 */
	vd_proc_mosaic = &vd_proc->vd_proc_mosaic;

	for (slice = 0; slice < SLICE_NUM; slice++) {
		VD1_SLICE_PAD_H_SIZE = frm_idx == 0 ?
			vd_proc_reg.vd1_slice_pad_size0_reg[slice].vd1_slice_pad_h_size :
			vd_proc_reg.vd1_slice_pad_size1_reg[slice].vd1_slice_pad_h_size;
		VD1_SLICE_PAD_V_SIZE = frm_idx == 0 ?
			vd_proc_reg.vd1_slice_pad_size0_reg[slice].vd1_slice_pad_v_size :
			vd_proc_reg.vd1_slice_pad_size1_reg[slice].vd1_slice_pad_v_size;

		rdma_wr(VD1_SLICE_PAD_H_SIZE,
			vd_proc_mosaic->vd1_proc_slice_pad_h_bgn[slice] << 16 |
			vd_proc_mosaic->vd1_proc_slice_pad_h_end[slice]);
		rdma_wr(VD1_SLICE_PAD_V_SIZE,
			vd_proc_mosaic->vd1_proc_slice_pad_v_bgn[slice] << 16 |
			vd_proc_mosaic->vd1_proc_slice_pad_v_end[slice]);
	}
	rdma_wr(VD1_PAD_CTRL,
		vd_proc_mosaic->vd1_proc_slice_pad_en[0] << 0 |
		vd_proc_mosaic->vd1_proc_slice_pad_en[1] << 1 |
		vd_proc_mosaic->vd1_proc_slice_pad_en[2] << 2 |
		vd_proc_mosaic->vd1_proc_slice_pad_en[3] << 3 |
		vd1_slice_pad_rpt_last_col << 4 |
		vd1_slice_pad_rpt_last_col << 5 |
		vd1_slice_pad_rpt_last_col << 6 |
		vd1_slice_pad_rpt_last_col << 7 |
		vd_proc_mosaic->mosaic_vd1_dout_vsize << 16);
}

/* for video_blend_1ppc */
static void vd_proc_blend_set(u32 vpp_index,
	struct vd_proc_blend_s *vd_blend)
{
	rdma_wr_op rdma_wr = cur_dev->rdma_func[vpp_index].rdma_wr;
	rdma_wr_bits_op rdma_wr_bits = cur_dev->rdma_func[vpp_index].rdma_wr_bits;
	struct vd_proc_blend_reg_s *vd_blend_reg = &vd_proc_reg.vd_proc_blend_reg;

	//vd blend en
	rdma_wr_bits(vd_blend_reg->vpp_vd_blnd_ctrl,
		vd_blend->bld_out_en, 0, 1);

	rdma_wr(vd_blend_reg->vpp_vd_blnd_h_v_size,
		vd_blend->bld_out_w |
		vd_blend->bld_out_h << 16);
	rdma_wr(vd_blend_reg->vpp_vd_blend_dummy_data, vd_blend->bld_dummy_data);
	rdma_wr_bits(vd_blend_reg->vpp_vd_blend_dummy_alpha, 0x100, 0, 32);

	//setting blend scope
	rdma_wr(vd_blend_reg->vpp_blend_vd1_s0_h_start_end,
		(vd_blend->bld_din0_h_start << 16) | vd_blend->bld_din0_h_end);
	rdma_wr(vd_blend_reg->vpp_blend_vd1_s0_v_start_end,
		(vd_blend->bld_din0_v_start << 16) | vd_blend->bld_din0_v_end);
	rdma_wr(vd_blend_reg->vpp_blend_vd1_s1_h_start_end,
		(vd_blend->bld_din1_h_start << 16) | vd_blend->bld_din1_h_end);
	rdma_wr(vd_blend_reg->vpp_blend_vd1_s1_v_start_end,
		(vd_blend->bld_din1_v_start << 16) | vd_blend->bld_din1_v_end);
	rdma_wr(vd_blend_reg->vpp_blend_vd1_s2_h_start_end,
		(vd_blend->bld_din2_h_start << 16) | vd_blend->bld_din2_h_end);
	rdma_wr(vd_blend_reg->vpp_blend_vd1_s2_v_start_end,
		(vd_blend->bld_din2_v_start << 16) | vd_blend->bld_din2_v_end);
	rdma_wr(vd_blend_reg->vpp_blend_vd1_s3_h_start_end,
		(vd_blend->bld_din3_h_start << 16) | vd_blend->bld_din3_h_end);
	rdma_wr(vd_blend_reg->vpp_blend_vd1_s3_v_start_end,
		(vd_blend->bld_din3_v_start << 16) | vd_blend->bld_din3_v_end);

	//src_sel & premult_en
	rdma_wr(vd_blend_reg->vd1_s0_blend_src_ctrl,
		(vd_blend->bld_src1_sel & 0xf) |
		(vd_blend->bld_din0_premult_en & 0x1) << 4);
	rdma_wr(vd_blend_reg->vd1_s1_blend_src_ctrl,
		(vd_blend->bld_src2_sel & 0xf) |
		(vd_blend->bld_din1_premult_en & 0x1) << 4);
	rdma_wr(vd_blend_reg->vd1_s2_blend_src_ctrl,
		(vd_blend->bld_src3_sel & 0xf) |
		(vd_blend->bld_din2_premult_en & 0x1) << 4);
	rdma_wr(vd_blend_reg->vd1_s3_blend_src_ctrl,
		(vd_blend->bld_src4_sel	& 0xf) |
		(vd_blend->bld_din3_premult_en & 0x1) << 4);
	if (debug_flag_s5 & DEBUG_VD_PROC) {
		pr_info("%s: vpp_vd_blnd_h_v_size=%x\n",
			__func__, vd_blend->bld_out_w |
			vd_blend->bld_out_h << 16);
		pr_info("%s: vpp_blend_vd1_s0_h_start_end=%x\n",
			__func__, vd_blend->bld_din0_h_start << 16 | vd_blend->bld_din0_h_end);
		pr_info("%s: vpp_blend_vd1_s0_v_start_end=%x\n",
			__func__, vd_blend->bld_din0_v_start << 16 | vd_blend->bld_din0_v_end);
		pr_info("%s: vpp_blend_vd1_s1_h_start_end=%x\n",
			__func__, vd_blend->bld_din1_h_start << 16 | vd_blend->bld_din1_h_end);
		pr_info("%s: vpp_blend_vd1_s1_v_start_end=%x\n",
			__func__, vd_blend->bld_din1_v_start << 16 | vd_blend->bld_din1_v_end);
		pr_info("%s: vpp_blend_vd1_s2_h_start_end=%x\n",
			__func__, vd_blend->bld_din2_h_start << 16 | vd_blend->bld_din2_h_end);
		pr_info("%s: vpp_blend_vd1_s2_v_start_end=%x\n",
			__func__, vd_blend->bld_din2_v_start << 16 | vd_blend->bld_din2_v_end);
		pr_info("%s: vpp_blend_vd1_s3_h_start_end=%x\n",
			__func__, vd_blend->bld_din3_h_start << 16 | vd_blend->bld_din3_h_end);
		pr_info("%s: vpp_blend_vd1_s3_v_start_end=%x\n",
			__func__, vd_blend->bld_din3_v_start << 16 | vd_blend->bld_din3_v_end);
	}
}

/* for vd2 preblen */
static void vd2_pre_blend_set(u32 vpp_index,
	struct vd_proc_blend_s *vd2_preblend)
{
	rdma_wr_op rdma_wr = cur_dev->rdma_func[vpp_index].rdma_wr;
	rdma_wr_bits_op rdma_wr_bits = cur_dev->rdma_func[vpp_index].rdma_wr_bits;
	struct vd2_pre_blend_reg_s *vd2_preblend_reg = &vd_proc_reg.vd2_pre_blend_reg;

	//vd blend en
	rdma_wr_bits(vd2_preblend_reg->vpp_vd_preblnd_ctrl,
		vd2_preblend->bld_out_en, 0, 1);

	rdma_wr(vd2_preblend_reg->vpp_vd_preblnd_h_v_size,
		vd2_preblend->bld_out_w |
		vd2_preblend->bld_out_h << 16);

	rdma_wr(vd2_preblend_reg->vpp_vd_pre_blend_dummy_data,
		vd2_preblend->bld_dummy_data);
	rdma_wr_bits(vd2_preblend_reg->vpp_vd_pre_blend_dummy_alpha,
		0x100, 0, 32);

	//setting blend scope
	rdma_wr(vd2_preblend_reg->vpp_preblend_vd1_h_start_end,
		(vd2_preblend->bld_din0_h_start << 16) |
		vd2_preblend->bld_din0_h_end);
	rdma_wr(vd2_preblend_reg->vpp_preblend_vd1_v_start_end,
		(vd2_preblend->bld_din0_v_start << 16) |
		vd2_preblend->bld_din0_v_end);
	rdma_wr(vd2_preblend_reg->vpp_preblend_vd2_h_start_end,
		(vd2_preblend->bld_din1_h_start << 16) |
		vd2_preblend->bld_din1_h_end);
	rdma_wr(vd2_preblend_reg->vpp_preblend_vd2_v_start_end,
		(vd2_preblend->bld_din1_v_start << 16) |
		vd2_preblend->bld_din1_v_end);

	//src_sel & premult_en
	rdma_wr(vd2_preblend_reg->vd1_preblend_src_ctrl,
		(vd2_preblend->bld_src1_sel & 0xf) |
		(vd2_preblend->bld_din0_premult_en & 0x1) << 4);
	rdma_wr(vd2_preblend_reg->vd2_preblend_src_ctrl,
		(vd2_preblend->bld_src2_sel & 0xf)  |
		(vd2_preblend->bld_din1_premult_en & 0x1) << 4);
	rdma_wr(vd2_preblend_reg->vd1_preblend_alpha,
		0x100);
	rdma_wr(vd2_preblend_reg->vd2_preblend_alpha,
		0x100);
}

static void vd2_proc_bypass_dv(u32 vpp_index,
	bool bypass)
{
	struct vd2_proc_misc_reg_s *vd2_proc_misc_reg = NULL;
	rdma_wr_bits_op rdma_wr_bits = cur_dev->rdma_func[vpp_index].rdma_wr_bits;

	vd2_proc_misc_reg = &vd_proc_reg.vd2_proc_misc_reg;
	if (bypass)
		rdma_wr_bits(vd2_proc_misc_reg->vd2_dv_bypass_ctrl, !bypass, 0, 1);
}

static void vd2_proc_bypass_hdr(u32 vpp_index,
	bool bypass)
{
	struct vd2_proc_misc_reg_s *vd2_proc_misc_reg = NULL;
	rdma_wr_bits_op rdma_wr_bits = cur_dev->rdma_func[vpp_index].rdma_wr_bits;

	vd2_proc_misc_reg = &vd_proc_reg.vd2_proc_misc_reg;
	if (bypass)
		rdma_wr_bits(vd2_proc_misc_reg->vd2_hdr_ctrl, !bypass, 13, 1);
}

static inline void vd2_proc_bypass_detunnel(u32 vpp_index,
	bool bypass)
{
	struct vd2_proc_misc_reg_s *vd2_proc_misc_reg = NULL;
	rdma_wr_bits_op rdma_wr_bits = cur_dev->rdma_func[vpp_index].rdma_wr_bits;

	vd2_proc_misc_reg = &vd_proc_reg.vd2_proc_misc_reg;
	if (bypass)
		rdma_wr_bits(vd2_proc_misc_reg->vd2_detunl_ctrl,
			!bypass, 0, 1);
}

static void vd1_proc_set(u32 vpp_index, struct vd_proc_s *vd_proc)
{
	int i;
	struct vd_proc_unit_s *vd_proc_unit;

	vd_proc_bypass_preblend(vpp_index, vd_proc);

	for (i = 0; i < vd_proc->vd_proc_vd1_info.slice_num; i++) {
		vd_proc_unit = &vd_proc->vd_proc_unit[i];
		vd_proc_unit->slice_index = i;
		vd_proc_unit->bypass_detunnel = vd_proc->bypass_detunnel;
		vd_proc_unit->bypass_hdr = vd_proc->bypass_hdr;
		vd_proc_unit->bypass_dv = vd_proc->bypass_dv;
		vd_proc_unit->bypass_ve = vd_proc->bypass_ve;
		vd_proc_unit_set(vpp_index, vd_proc_unit);
		/* pps */
		vd1_scaler_setting_s5(&vd_layer[0], &vd_layer[0].sc_setting, i);
	}
	vd_proc_blend_set(vpp_index, &vd_proc->vd_proc_blend);
	vd_proc_pi_path_set(vpp_index, vd_proc);
}

static void vd2_proc_set(u32 vpp_index, struct vd2_proc_s *vd2_proc)
{
	struct vd2_proc_misc_reg_s *vd2_proc_misc_reg = NULL;
	rdma_wr_op rdma_wr = cur_dev->rdma_func[vpp_index].rdma_wr;
	rdma_wr_bits_op rdma_wr_bits = cur_dev->rdma_func[vpp_index].rdma_wr_bits;

	vd2_proc_misc_reg = &vd_proc_reg.vd2_proc_misc_reg;
	rdma_wr(vd2_proc_misc_reg->vd2_proc_in_size,
		vd2_proc->din_hsize << 16 |
		vd2_proc->din_vsize);
	/* detunnel */
	vd2_proc_bypass_detunnel(vpp_index, vd2_proc->bypass_detunnel);
	/* hdr */
	vd2_proc_bypass_hdr(vpp_index, vd2_proc->bypass_hdr);
	/* pps */
	vdx_scaler_setting_s5(&vd_layer[1], &vd_layer[1].sc_setting);
	if (vd2_proc->vd2_dout_dpsel == VD2_DOUT_PI) {
		/* vd2 pi set */
		rdma_wr_bits(vd2_proc_misc_reg->vd2_pilite_ctrl,
			vd2_proc->vd_proc_pi.pi_en, 0, 1);
		rdma_wr(vd2_proc_misc_reg->vd2_proc_out_size,
			(vd2_proc->dout_hsize / 2) << 16 |
			vd2_proc->dout_vsize / 2);
	} else {
		rdma_wr(vd2_proc_misc_reg->vd2_proc_out_size,
			SIZE_ALIG4(vd2_proc->dout_hsize << 16 |
			vd2_proc->dout_vsize));
	}
	vd2_proc_bypass_dv(vpp_index, vd2_proc->bypass_dv);
}

static void vd_proc_set(u32 vpp_index, struct vd_proc_s *vd_proc)
{
	u32 vd1_work_mode = 0;
	u32 vd1_slices_dout_dpsel = 0;
	u32 mosaic_mode, hsize = 0;
	u32 vd1_dout_hsize = 0, vd1_dout_vsize = 0;
	rdma_wr_op rdma_wr = cur_dev->rdma_func[vpp_index].rdma_wr;
	rdma_wr_bits_op rdma_wr_bits = cur_dev->rdma_func[vpp_index].rdma_wr_bits;
	struct vd_proc_mosaic_s *vd_proc_mosaic = NULL;

	vd1_work_mode = vd_proc->vd_proc_vd1_info.vd1_work_mode;
	vd1_slices_dout_dpsel = vd_proc->vd_proc_vd1_info.vd1_slices_dout_dpsel;
	vd1_dout_hsize = vd_proc->vd_proc_vd1_info.vd1_dout_hsize;
	vd1_dout_vsize = vd_proc->vd_proc_vd1_info.vd1_dout_vsize;
	mosaic_mode = vd1_work_mode == VD1_2_2SLICES_MODE &&
			vd1_slices_dout_dpsel == VD1_SLICES_DOUT_4S4P;
	if (debug_flag_s5 & DEBUG_VD_PROC)
		pr_info("%s: vd1_work_mode=%d, vd1_slices_dout_dpsel=%d, vd1_dout_hsize=%d, vd1_dout_vsize=%d\n",
			__func__, vd1_work_mode, vd1_slices_dout_dpsel,
			vd1_dout_hsize, vd1_dout_vsize);
	if (vd_proc->vd1_used)
		vd1_proc_set(vpp_index, vd_proc);
	if (vd_proc->vd2_used)
		vd2_proc_set(vpp_index, &vd_proc->vd2_proc);
	if (vd_proc->vd_proc_preblend_info.vd1s0_vd2_prebld_en ||
		vd_proc->vd_proc_preblend_info.vd1s1_vd2_prebld_en)
		vd2_pre_blend_set(vpp_index, &vd_proc->vd_proc_preblend);

	/* path sel set */
	switch (vd1_work_mode) {
	case VD1_1SLICES_MODE:
		/* output to vd1 */
		rdma_wr_bits(VPP_VD_SYS_CTRL, 0, 2, 1);
		rdma_wr(SLICE2PPC_H_V_SIZE, vd1_dout_vsize << 16 |
			vd1_dout_hsize);
		/* vd1 dout pi path */
		rdma_wr_bits(VPP_VD_SYS_CTRL, 0, 0, 2);
		vd_proc_pi_set(vpp_index, &vd_proc->vd_proc_pi);
		break;
	case VD1_SLICES01_MODE:
		/* output to vd1 */
		rdma_wr_bits(VPP_VD_SYS_CTRL, 0, 2, 1);
		if (vd1_slices_dout_dpsel == VD1_SLICES_DOUT_2S4P) {
			/* vd1 dout 2s2p path */
			rdma_wr_bits(VPP_VD_SYS_CTRL, 2, 0, 2);
			rdma_wr(SLICE2PPC_H_V_SIZE, vd1_dout_vsize << 16 |
				SIZE_ALIG16(vd1_dout_hsize) / 2);
		}
		break;
	case VD1_4SLICES_MODE:
		/* output to vd1 */
		rdma_wr_bits(VPP_VD_SYS_CTRL, 0, 2, 1);
		if (vd1_slices_dout_dpsel == VD1_SLICES_DOUT_PI) {
			/* vd1 dout pi path */
			rdma_wr_bits(VPP_VD_SYS_CTRL, 0, 0, 2);
			vd_proc_pi_set(vpp_index, &vd_proc->vd_proc_pi);
		} else if (vd1_slices_dout_dpsel == VD1_SLICES_DOUT_1S4P) {
			/* vd1 dout pi path */
			rdma_wr_bits(VPP_VD_SYS_CTRL, 0, 0, 2);
			vd_proc_pi_set(vpp_index, &vd_proc->vd_proc_pi);
		} else if (vd1_slices_dout_dpsel == VD1_SLICES_DOUT_4S4P) {
			/* vd1 dout 4s4p path */
			rdma_wr_bits(VPP_VD_SYS_CTRL, 1, 0, 2);
			rdma_wr(SLICE2PPC_H_V_SIZE, vd1_dout_vsize << 16 |
				SIZE_ALIG32(vd1_dout_hsize) / SLICE_NUM);
		}
		break;
	default:
		if (mosaic_mode) {
			/* 4slices mosaic mode */
			vd_proc_mosaic = &vd_proc->vd_proc_mosaic;
			/* output to vd1 */
			rdma_wr_bits(VPP_VD_SYS_CTRL, 0, 2, 1);
			/* vd1 dout 4s4p path */
			rdma_wr_bits(VPP_VD_SYS_CTRL, 1, 0, 2);
			hsize = SIZE_ALIG16(vd_proc_mosaic->mosaic_vd1_dout_hsize) +
				SIZE_ALIG16(vd_proc_mosaic->mosaic_vd2_dout_hsize);
			hsize = SIZE_ALIG32(hsize) / SLICE_NUM;
			rdma_wr(SLICE2PPC_H_V_SIZE, vd1_dout_vsize << 16 |
				hsize);
		}
		break;
	}
	if (!mosaic_mode)
		vd_slices_padding_set(vpp_index, vd_proc);
}

/* hw reg param info set */
static void vd_blend_1ppc_param_set(struct vd_proc_s *vd_proc,
	struct vd_proc_blend_s *vd_proc_blend,
	u32 bld_out_hsize, u32 bld_out_vsize,
	u32 bld_out_en)
{
	vd_proc_blend->bld_out_en = bld_out_en;
	vd_proc_blend->bld_dummy_data = 0x008080;

	switch (vd_proc->vd_proc_vd1_info.slice_num) {
	case 1:
		vd_proc_blend->bld_src1_sel = 1;
		vd_proc_blend->bld_src2_sel = 0;
		vd_proc_blend->bld_src3_sel = 0;
		vd_proc_blend->bld_src4_sel = 0;
		break;
	case 2:
		vd_proc_blend->bld_src1_sel = 1;
		vd_proc_blend->bld_src2_sel = 2;
		vd_proc_blend->bld_src3_sel = 0;
		vd_proc_blend->bld_src4_sel = 0;
		break;
	case 4:
		vd_proc_blend->bld_src1_sel = 1;
		vd_proc_blend->bld_src2_sel = 2;
		vd_proc_blend->bld_src3_sel = 3;
		vd_proc_blend->bld_src4_sel = 4;
		break;
	}

	vd_proc_blend->bld_out_w = bld_out_hsize;
	vd_proc_blend->bld_out_h = bld_out_vsize;

	vd_proc_blend->bld_din0_h_start =
		vd_proc->vd_proc_unit[0].dout_x_start;
	vd_proc_blend->bld_din0_h_end =
		vd_proc->vd_proc_unit[0].dout_x_start +
		vd_proc->vd_proc_unit[0].dout_hsize - 1;
	vd_proc_blend->bld_din0_v_start =
		vd_proc->vd_proc_unit[0].dout_y_start;
	vd_proc_blend->bld_din0_v_end =
		vd_proc->vd_proc_unit[0].dout_y_start +
		vd_proc->vd_proc_unit[0].dout_vsize - 1;

	vd_proc_blend->bld_din1_h_start =
		vd_proc->vd_proc_unit[1].dout_x_start;
	vd_proc_blend->bld_din1_h_end =
		vd_proc->vd_proc_unit[1].dout_x_start +
		vd_proc->vd_proc_unit[1].dout_hsize - 1;
	vd_proc_blend->bld_din1_v_start =
		vd_proc->vd_proc_unit[1].dout_y_start;
	vd_proc_blend->bld_din1_v_end =
		vd_proc->vd_proc_unit[1].dout_y_start +
		vd_proc->vd_proc_unit[1].dout_vsize - 1;

	vd_proc_blend->bld_din2_h_start =
		vd_proc->vd_proc_unit[2].dout_x_start;
	vd_proc_blend->bld_din2_h_end =
		vd_proc->vd_proc_unit[2].dout_x_start +
		vd_proc->vd_proc_unit[2].dout_hsize - 1;
	vd_proc_blend->bld_din2_v_start =
		vd_proc->vd_proc_unit[2].dout_y_start;
	vd_proc_blend->bld_din2_v_end =
		vd_proc->vd_proc_unit[2].dout_y_start +
		vd_proc->vd_proc_unit[2].dout_vsize - 1;

	vd_proc_blend->bld_din3_h_start =
		vd_proc->vd_proc_unit[3].dout_x_start;
	vd_proc_blend->bld_din3_h_end =
		vd_proc->vd_proc_unit[3].dout_x_start +
		vd_proc->vd_proc_unit[3].dout_hsize - 1;
	vd_proc_blend->bld_din3_v_start =
		vd_proc->vd_proc_unit[3].dout_y_start;
	vd_proc_blend->bld_din3_v_end =
		vd_proc->vd_proc_unit[3].dout_y_start +
		vd_proc->vd_proc_unit[3].dout_vsize - 1;
	if (debug_flag_s5 & DEBUG_VD_PROC) {
		pr_info("%s: bld_din0_h_start=%d, bld_din0_h_end=%d, v: %d, %d\n",
			__func__,
			vd_proc_blend->bld_din0_h_start,
			vd_proc_blend->bld_din0_h_end,
			vd_proc_blend->bld_din0_v_start,
			vd_proc_blend->bld_din0_v_end);
		pr_info("%s: slice_num=%d, bld_out_w=%d, bld_out_h=%d\n",
			__func__,
			vd_proc->vd_proc_vd1_info.slice_num,
			vd_proc_blend->bld_out_w,
			vd_proc_blend->bld_out_h);
	}
}

static void vd_preblend_param_set(struct vd_proc_s *vd_proc,
	struct vd_proc_blend_s *vd_proc_preblend,
	u32 bld_out_hsize, u32 bld_out_vsize,
	u32 slice_num, u32 bld_out_en)
{
	vd_proc_preblend->bld_out_en = bld_out_en;
	vd_proc_preblend->bld_dummy_data = 0x008080;

	switch (slice_num) {
	case 0:
		vd_proc_preblend->bld_src1_sel = 0;
		vd_proc_preblend->bld_src2_sel = 0;
		vd_proc_preblend->bld_src3_sel = 0;
		vd_proc_preblend->bld_src4_sel = 0;
		vd_proc_preblend->bld_out_en = 0;
		return;
	case 1:
		vd_proc_preblend->bld_src1_sel = 2;
		vd_proc_preblend->bld_src2_sel = 0;
		vd_proc_preblend->bld_src3_sel = 0;
		vd_proc_preblend->bld_src4_sel = 0;
		break;
	case 2:
		vd_proc_preblend->bld_src1_sel = 1;
		vd_proc_preblend->bld_src2_sel = 2;
		vd_proc_preblend->bld_src3_sel = 0;
		vd_proc_preblend->bld_src4_sel = 0;
		break;
	}

	vd_proc_preblend->bld_out_w = bld_out_hsize;
	vd_proc_preblend->bld_out_h = bld_out_vsize;

	vd_proc_preblend->bld_din0_h_start =
		vd_proc->vd_proc_unit[0].dout_x_start;
	vd_proc_preblend->bld_din0_h_end =
		vd_proc->vd_proc_unit[0].dout_x_start +
		vd_proc->vd_proc_unit[0].dout_hsize - 1;
	vd_proc_preblend->bld_din0_v_start =
		vd_proc->vd_proc_unit[0].dout_y_start;
	vd_proc_preblend->bld_din0_v_end =
		vd_proc->vd_proc_unit[0].dout_y_start +
		vd_proc->vd_proc_unit[0].dout_vsize - 1;

	vd_proc_preblend->bld_din1_h_start =
		vd_proc->vd_proc_unit[1].dout_x_start;
	vd_proc_preblend->bld_din1_h_end =
		vd_proc->vd_proc_unit[1].dout_x_start +
		vd_proc->vd_proc_unit[1].dout_hsize - 1;
	vd_proc_preblend->bld_din1_v_start =
		vd_proc->vd_proc_unit[1].dout_y_start;
	vd_proc_preblend->bld_din1_v_end =
		vd_proc->vd_proc_unit[1].dout_y_start +
		vd_proc->vd_proc_unit[1].dout_vsize - 1;
}

/* o_din_hsize: slice proc unit input hsize */
/* dout_hsize: slice proc unit output hsize */
static void cal_pps_din_hsize(u32 *o_din_hsize,
	u32 dout_hsize,
	u32 horz_phase_step,
	u32 pre_hsc_en)
{
	*o_din_hsize = div_u64(((long long)dout_hsize *
		horz_phase_step >> 4),
		1 << 20);
#ifndef NEW_PRE_SCALER
	if (pre_hsc_en)
		*o_din_hsize <<= 1;
#endif
}

/* o_dout_hsize: slice proc unit dout hsize */
/* din_hsize: slice proc unit input hsize */
static void cal_pps_dout_hsize(u32 *o_dout_hsize,
	u32 ini_phase,
	u32 din_hsize,
	u32 horz_phase_step,
	u32 pre_hsc_en)
{
	*o_dout_hsize = div_u64(((long long)ini_phase +
		(long long)(din_hsize) * (1 << 24)),
		horz_phase_step);
#ifndef NEW_PRE_SCALER
	if (pre_hsc_en)
		*o_dout_hsize >>= 1;
#endif
}

static void set_vd_proc_info(struct video_layer_s *layer)
{
	u32 horz_phase_step = 0, vert_phase_step = 0;
	u32 vpp_pre_hsc_en = 0, vpp_pre_vsc_en = 0;
	u32 sr0_h_scaleup_en = 0, sr1_h_scaleup_en = 0;
	u32 src_w = 0, src_h = 0;
	u32 dst_w = 0, dst_h = 0;
	u32 h_start = 0, v_start = 0;
	u32 slice = 0, slice_num;
	struct vd_proc_s *vd_proc = &g_vd_proc;
	struct vpp_frame_par_s *cur_frame_par = layer->cur_frame_par;
	struct vd_proc_vd1_info_s *vd_proc_vd1_info;
	struct vd_proc_vd2_info_s *vd_proc_vd2_info;
	struct vd_proc_preblend_info_s *vd_proc_preblend_info;
	struct vd_proc_unit_s *vd_proc_unit;
	struct sr_info_s *sr;

	if (cur_dev->display_module != S5_DISPLAY_MODULE)
		return;
	if (!cur_frame_par)
		return;
	slice_num = layer->slice_num;
	/* should set slice later */
	/* set some important input info for vd_proc */
	vd_proc_vd1_info = &vd_proc->vd_proc_vd1_info;
	vd_proc_vd2_info = &vd_proc->vd_proc_vd2_info;
	vd_proc_preblend_info = &vd_proc->vd_proc_preblend_info;
	vd_proc_unit = &vd_proc->vd_proc_unit[slice];
	sr = get_super_scaler_info();

	vd_proc_vd1_info->slice_num = layer->slice_num;
	/* get vd input and output info */
	src_w = cur_frame_par->video_input_w;
	src_h = cur_frame_par->video_input_h;

	dst_w =
		cur_frame_par->VPP_hsc_endp -
		cur_frame_par->VPP_hsc_startp + 1;
	dst_h =
		cur_frame_par->VPP_vsc_endp -
		cur_frame_par->VPP_vsc_startp + 1;

	h_start = cur_frame_par->VPP_hsc_startp;
	v_start = cur_frame_par->VPP_vsc_startp;

	/* get scaler info(include pre_scaler, pps, sr)*/
	horz_phase_step =
		cur_frame_par->vpp_filter.vpp_hsc_start_phase_step;
	vert_phase_step =
		cur_frame_par->vpp_filter.vpp_vsc_start_phase_step;
	vpp_pre_hsc_en =
		cur_frame_par->vpp_filter.vpp_pre_hsc_en;
	vpp_pre_vsc_en =
		cur_frame_par->vpp_filter.vpp_pre_vsc_en;

	/* need add some logic to set this var */
	/* todo */
	//vd_proc->bypass_detunnel
	//vd_proc->bypass_hdr
	//vd_proc->bypass_dv
	//vd_proc->bypass_ve
	//vd_proc_unit->sr0_dpath_sel
	//vd_proc_unit->sr0_pps_dpsel
	//vd_proc_preblend_info->vd1s0_vd2_prebld_en
	//vd_proc_preblend_info->vd1s1_vd2_prebld_en
	//vd_proc_vd1_info->vd1_work_mode
	//vd_proc_vd1_info->vd1_slices_dout_dpsel
	//vd_proc_vd1_info->vd1_overlap_hsize
	//vd_proc_vd2_info->vd2_dout_dpsel
	// when VD2_DOUT_PREBLD need set
	//vd_proc_preblend_info->prebld_dout_hsize
	//vd_proc_preblend_info->prebld_dout_vsize
	//vd_proc_preblend_info->vd1s0_vd2_prebld_en
	//vd_proc_preblend_info->vd1s1_vd2_prebld_en
	vd_proc->bypass_detunnel = g_bypass_module & BYAPSS_DETUNNEL;
	vd_proc->bypass_hdr = g_bypass_module & BYPASS_HDR;
	vd_proc->bypass_dv = g_bypass_module & BYPASS_DV;
	//vd_proc->bypass_ve = 1;
	vd_proc_unit->sr0_dpath_sel = SR0_IN_SLICE0;
	/* SR0_IN_SLICE0 */
	if (cur_frame_par->supscl_path == CORE0_PPS_CORE1)
		vd_proc_unit->sr0_pps_dpsel = SR0_BEFORE_PPS;
	else
		vd_proc_unit->sr0_pps_dpsel = SR0_AFTER_PPS;
	/* SR0_AFTER_PPS */
	/* vd1 preblend disable */
	vd_proc_preblend_info->vd1s0_vd2_prebld_en = 0;
	/* vd2 preblend disable */
	vd_proc_preblend_info->vd1s1_vd2_prebld_en = layer->vd1s1_vd2_prebld_en;
	if (vd_proc_preblend_info->vd1s1_vd2_prebld_en) {
		vd_proc_vd2_info->vd2_dout_dpsel = VD2_DOUT_PREBLD1;
		//vd_proc->vd2_used = 1;
	}
	if (layer->layer_id == 0) {
		vd_proc->vd1_used = 1;
		/* should be set here */
		/* todo */
		if (layer->slice_num == 1) {
			if (layer->pi_enable) {
				vd_proc_vd1_info->vd1_work_mode = VD1_1SLICES_MODE;
				vd_proc_vd1_info->vd1_slices_dout_dpsel = VD1_SLICES_DOUT_PI;
				vd_proc_vd1_info->vd1_overlap_hsize = 0;
			} else {
				vd_proc_vd1_info->vd1_work_mode = VD1_1SLICES_MODE;
				vd_proc_vd1_info->vd1_slices_dout_dpsel = VD1_SLICES_DOUT_1S4P;
				vd_proc_vd1_info->vd1_overlap_hsize = 0;
			}
			vd_proc_unit->sr0_dpath_sel = SR0_IN_SLICE0;
		} else if (layer->slice_num == 2) {
			vd_proc_vd1_info->vd1_work_mode = VD1_SLICES01_MODE;
			vd_proc_vd1_info->vd1_slices_dout_dpsel = VD1_SLICES_DOUT_2S4P;
			vd_proc_vd1_info->vd1_overlap_hsize = 32;
			vd_proc_unit->sr0_dpath_sel = SR0_IN_SLICE1;
			vd_proc_unit->sr0_pps_dpsel = SR0_AFTER_PPS;
		} else if (layer->slice_num == 4) {
			vd_proc_vd1_info->vd1_work_mode = VD1_4SLICES_MODE;
			vd_proc_vd1_info->vd1_slices_dout_dpsel = VD1_SLICES_DOUT_4S4P;
			vd_proc_vd1_info->vd1_overlap_hsize = 32;
		}

		switch (vd_proc_vd1_info->vd1_work_mode) {
		case VD1_1SLICES_MODE:
			/* if one pic */
			/* whole frame in hsize */
			vd_proc_vd1_info->vd1_src_din_hsize[0] = src_w;
			vd_proc_vd1_info->vd1_src_din_vsize[0] = src_h;
			/* without overlap */
			#ifdef CHECK_LATER
			if (vd_proc_vd1_info->vd1_slices_dout_dpsel ==
				VD1_SLICES_DOUT_PI) {
				vd_proc_vd1_info->vd1_proc_unit_dout_hsize[0] = dst_w;
				vd_proc_vd1_info->vd1_proc_unit_dout_vsize[0] = dst_h;
			}
			#endif
			vd_proc_vd1_info->vd1_proc_unit_dout_hsize[0] = dst_w;
			vd_proc_vd1_info->vd1_proc_unit_dout_vsize[0] = dst_h;
			/* whole vd1 output size */
			vd_proc_vd1_info->vd1_dout_hsize = dst_w;
			vd_proc_vd1_info->vd1_dout_vsize = dst_h;
			vd_proc_vd1_info->vd1_dout_x_start[0] = h_start;
			vd_proc_vd1_info->vd1_dout_y_start[0] = v_start;
			vd_proc_vd1_info->vd1_overlap_hsize = 0;
			break;
		case VD1_4SLICES_MODE:
			switch (vd_proc_vd1_info->vd1_slices_dout_dpsel) {
			case VD1_SLICES_DOUT_4S4P:
				/* if one pic */
				/* whole frame in hsize */
				vd_proc_vd1_info->vd1_src_din_hsize[0] = src_w;
				vd_proc_vd1_info->vd1_src_din_vsize[0] = src_h;
				/* without overlap */
				for (slice = 0; slice < SLICE_NUM; slice++) {
					vd_proc_vd1_info->vd1_proc_unit_dout_hsize[slice] =
						slice_out_hsize(slice, SLICE_NUM, dst_w);
					vd_proc_vd1_info->vd1_proc_unit_dout_vsize[slice] = dst_h;
					vd_proc_vd1_info->vd1_dout_x_start[slice] = h_start;
					vd_proc_vd1_info->vd1_dout_y_start[slice] = v_start;
				}
				/* whole vd1 output size */
				vd_proc_vd1_info->vd1_dout_hsize = dst_w;
				vd_proc_vd1_info->vd1_dout_vsize = dst_h;
				vd_proc_vd1_info->vd1_overlap_hsize = 32;
				break;
			case VD1_SLICES_DOUT_PI:
				/* 4 pic */
				break;
			}
			break;
		case VD1_2_2SLICES_MODE:
			break;
		case VD1_SLICES01_MODE:
			switch (vd_proc_vd1_info->vd1_slices_dout_dpsel) {
			case VD1_SLICES_DOUT_2S4P:
				/* if one pic */
				/* whole frame in hsize */
				vd_proc_vd1_info->vd1_src_din_hsize[0] = src_w;
				vd_proc_vd1_info->vd1_src_din_vsize[0] = src_h;
				/* without overlap */
				for (slice = 0; slice < 2; slice++) {
					vd_proc_vd1_info->vd1_proc_unit_dout_hsize[slice] =
						slice_out_hsize(slice, 2, dst_w);
					vd_proc_vd1_info->vd1_proc_unit_dout_vsize[slice] = dst_h;
					vd_proc_vd1_info->vd1_dout_x_start[slice] = h_start;
					vd_proc_vd1_info->vd1_dout_y_start[slice] = v_start;
				}
				/* whole vd1 output size */
				vd_proc_vd1_info->vd1_dout_hsize = dst_w;
				vd_proc_vd1_info->vd1_dout_vsize = dst_h;
				vd_proc_vd1_info->vd1_overlap_hsize = 32;
				break;
			case VD1_SLICES_DOUT_PI:
				/* 4 pic */
				break;
			}
			break;
		}
		for (slice = 0; slice < slice_num; slice++) {
			vd_proc_unit = &vd_proc->vd_proc_unit[slice];
			vd_proc_unit->vd_proc_pps.horz_phase_step =
				horz_phase_step;
			vd_proc_unit->vd_proc_pps.vert_phase_step =
				vert_phase_step;
			vd_proc_unit->vd_proc_pps.prehsc_en = vpp_pre_hsc_en;
			vd_proc_unit->vd_proc_pps.prevsc_en = vpp_pre_vsc_en;
			vd_proc_unit->vd_proc_pps.prehsc_rate = 1;
			vd_proc_unit->vd_proc_pps.prevsc_rate = 1;
			sr0_h_scaleup_en = cur_frame_par->supsc0_enable &&
				cur_frame_par->supsc0_hori_ratio;
			sr1_h_scaleup_en = cur_frame_par->supsc1_enable &&
				cur_frame_par->supsc1_hori_ratio;
			if (slice_num == 2) {
				/* 2 slice case, move sr0 to slice1 */
				if (slice == 0) {
					/* slice0, used sr1, get info from sr0 */
					vd_proc_unit->sr1_en = sr0_h_scaleup_en;
					vd_proc_unit->vd_proc_sr1.sr_en =
						cur_frame_par->supsc0_enable;
					vd_proc_unit->vd_proc_sr1.h_scaleup_en =
						cur_frame_par->supsc0_hori_ratio;
					vd_proc_unit->vd_proc_sr1.v_scaleup_en =
						cur_frame_par->supsc0_vert_ratio;
					vd_proc_unit->vd_proc_sr1.core_v_disable_width_max =
						sr->core0_v_disable_width_max;
					vd_proc_unit->vd_proc_sr1.core_v_enable_width_max =
						sr->core0_v_enable_width_max;
					vd_proc_unit->vd_proc_sr1.sr_support =
						sr->sr_support & SUPER_CORE1_SUPPORT;
					if (debug_flag_s5 & DEBUG_VD_PROC)
						pr_info("s0: sr1_en=%d, h/v_scaleup_en=%d,%d, phase step：%x,%x\n",
							vd_proc_unit->vd_proc_sr1.sr_en,
							vd_proc_unit->vd_proc_sr1.h_scaleup_en,
							vd_proc_unit->vd_proc_sr1.v_scaleup_en,
							vd_proc_unit->vd_proc_pps.horz_phase_step,
							vd_proc_unit->vd_proc_pps.vert_phase_step);
				}
				if (slice == 1) {
					/* slice1, used sr0, get info from sr0 */
					vd_proc_unit->sr0_en = sr0_h_scaleup_en;
					vd_proc_unit->vd_proc_sr0.sr_en =
						cur_frame_par->supsc0_enable;
					vd_proc_unit->vd_proc_sr0.h_scaleup_en =
						cur_frame_par->supsc0_hori_ratio;
					vd_proc_unit->vd_proc_sr0.v_scaleup_en =
						cur_frame_par->supsc0_vert_ratio;
					vd_proc_unit->vd_proc_sr0.core_v_disable_width_max =
						sr->core0_v_disable_width_max;
					vd_proc_unit->vd_proc_sr0.core_v_enable_width_max =
						sr->core0_v_enable_width_max;
					vd_proc_unit->vd_proc_sr0.sr_support =
						sr->sr_support & SUPER_CORE0_SUPPORT;
					vd_proc_unit->sr0_dpath_sel = SR0_IN_SLICE1;
					vd_proc_unit->sr0_pps_dpsel = SR0_AFTER_PPS;
					if (debug_flag_s5 & DEBUG_VD_PROC)
						pr_info("s1: sr0_en=%d, h/v_scaleup_en=%d, %d, phase step：%x, %x\n",
							vd_proc_unit->vd_proc_sr0.sr_en,
							vd_proc_unit->vd_proc_sr0.h_scaleup_en,
							vd_proc_unit->vd_proc_sr0.v_scaleup_en,
							vd_proc_unit->vd_proc_pps.horz_phase_step,
							vd_proc_unit->vd_proc_pps.vert_phase_step);
				}
			} else {
				vd_proc_unit->sr0_en = sr0_h_scaleup_en;
				vd_proc_unit->sr1_en = sr1_h_scaleup_en;
				vd_proc_unit->vd_proc_sr0.sr_en =
					cur_frame_par->supsc0_enable;
				vd_proc_unit->vd_proc_sr0.h_scaleup_en =
					cur_frame_par->supsc0_hori_ratio;
				vd_proc_unit->vd_proc_sr0.v_scaleup_en =
					cur_frame_par->supsc0_vert_ratio;
				vd_proc_unit->vd_proc_sr0.core_v_disable_width_max =
					sr->core0_v_disable_width_max;
				vd_proc_unit->vd_proc_sr0.core_v_enable_width_max =
					sr->core0_v_enable_width_max;
				vd_proc_unit->vd_proc_sr0.sr_support =
					sr->sr_support & SUPER_CORE0_SUPPORT;

				vd_proc_unit->vd_proc_sr1.sr_en =
					cur_frame_par->supsc1_enable;
				vd_proc_unit->vd_proc_sr1.h_scaleup_en =
					cur_frame_par->supsc1_hori_ratio;
				vd_proc_unit->vd_proc_sr1.v_scaleup_en =
					cur_frame_par->supsc1_vert_ratio;
				vd_proc_unit->vd_proc_sr1.core_v_disable_width_max =
					sr->core1_v_disable_width_max;
				vd_proc_unit->vd_proc_sr1.core_v_enable_width_max =
					sr->core1_v_enable_width_max;
				vd_proc_unit->vd_proc_sr1.sr_support =
					sr->sr_support & SUPER_CORE1_SUPPORT;
			}
		}
		/* if 4 pic, todo */
	} else if (layer->layer_id == 1) {
		vd_proc->vd2_used = 1;
		/* todo */
		if (layer->pi_enable)
			vd_proc_vd2_info->vd2_dout_dpsel = VD2_DOUT_PI;
		else
			vd_proc_vd2_info->vd2_dout_dpsel = VD2_DOUT_S2P;
		//todo: for 4k120hz VD2_DOUT_PREBLD1
		vd_proc_vd2_info->vd2_din_hsize = src_w;
		vd_proc_vd2_info->vd2_din_vsize = src_h;
		vd_proc_vd2_info->vd2_dout_hsize = dst_w;
		vd_proc_vd2_info->vd2_dout_vsize = dst_h;
		vd_proc_vd2_info->vd2_dout_x_start = h_start;
		vd_proc_vd2_info->vd2_dout_y_start = v_start;

		vd_proc->vd2_proc.vd_proc_pps.horz_phase_step =
			horz_phase_step;
		vd_proc->vd2_proc.vd_proc_pps.vert_phase_step =
			vert_phase_step;
		vd_proc->vd2_proc.vd_proc_pps.prehsc_en = vpp_pre_hsc_en;
		vd_proc->vd2_proc.vd_proc_pps.prevsc_en = vpp_pre_vsc_en;
		vd_proc_unit->vd_proc_pps.prehsc_rate = 1;
		vd_proc_unit->vd_proc_pps.prevsc_rate = 1;
		if (vd_proc_preblend_info->vd1s0_vd2_prebld_en) {
			/* todo */
			vd_proc_preblend_info->prebld_dout_hsize = src_w;
			vd_proc_preblend_info->prebld_dout_vsize = src_h;
		}
	}
	if (debug_flag_s5 & DEBUG_VD_PROC) {
		if (vd_proc->vd1_used) {
			pr_info("%s:vd_proc_vd1_info->slice_num=%d\n",
				__func__, vd_proc_vd1_info->slice_num);
			pr_info("%s:vd1_work_mode=0x%x, vd1_slices_dout_dpsel=0x%x\n",
				__func__, vd_proc_vd1_info->vd1_work_mode,
				vd_proc_vd1_info->vd1_slices_dout_dpsel);
		} else if (vd_proc->vd2_used) {
			pr_info("%s: vd2_dout_dpsel=%d, vd1s0_vd2_prebld_en=%d\n",
				__func__, vd_proc_vd2_info->vd2_dout_dpsel,
				vd_proc_preblend_info->vd1s0_vd2_prebld_en);
		}
		pr_info("%s:src_w=%d, src_h=%d, dst_w=%d, dst_h=%d\n",
			__func__, src_w, src_h, dst_w, dst_h);
		pr_info("%s:h_start=%d, v_start=%d\n",
			__func__, h_start, v_start);
		pr_info("%s:horz_phase_step=0x%x, vert_phase_step=0x%x\n",
			__func__, horz_phase_step, vert_phase_step);
		pr_info("%s:vpp_pre_hsc_en=0x%x, vpp_pre_vsc_en=0x%x\n",
			__func__, vpp_pre_hsc_en, vpp_pre_vsc_en);
	}
}

static void set_vd_src_info(struct video_layer_s *layer)
{
	u32 slice = 0;
	struct vd_proc_s *vd_proc = &g_vd_proc;
	struct vd_proc_vd1_info_s *vd_proc_vd1_info;
	struct vd_proc_vd2_info_s *vd_proc_vd2_info;
	struct mif_pos_s *mif_setting;
	struct mif_pos_s *slice_mif_setting;
	struct vd_proc_slice_info_s *vd_proc_slice_info;

	vd_proc_vd1_info = &vd_proc->vd_proc_vd1_info;
	vd_proc_vd2_info = &vd_proc->vd_proc_vd2_info;
	mif_setting = &layer->mif_setting;
	vd_proc_slice_info = &vd_proc->vd_proc_slice_info;

	if (layer->layer_id == 0) {
		switch (vd_proc_vd1_info->vd1_work_mode) {
		case VD1_1SLICES_MODE:
			/* skip, used mif_setting */
			break;
		case VD1_4SLICES_MODE:
			switch (vd_proc_vd1_info->vd1_slices_dout_dpsel) {
			case VD1_SLICES_DOUT_4S4P:
				/* if one pic */
				/* whole frame in hsize, need 4slice, recal-mif scope*/
				for (slice = 0; slice < SLICE_NUM; slice++) {
					slice_mif_setting = &layer->slice_mif_setting[slice];
					slice_mif_setting->id = slice;
					slice_mif_setting->reverse = mif_setting->reverse;
					/* whole buffer size */
					slice_mif_setting->src_w = mif_setting->src_w;
					slice_mif_setting->src_h = mif_setting->src_h;
					slice_mif_setting->start_x_lines =
						vd_proc_slice_info->vd1_slice_x_st[slice];
					slice_mif_setting->end_x_lines =
						vd_proc_slice_info->vd1_slice_x_end[slice];
					slice_mif_setting->start_y_lines =
						mif_setting->start_y_lines;
					slice_mif_setting->end_y_lines =
						mif_setting->end_y_lines;

					slice_mif_setting->l_hs_luma =
						slice_mif_setting->start_x_lines;
					slice_mif_setting->l_he_luma =
						slice_mif_setting->end_x_lines;
					slice_mif_setting->l_vs_luma =
						slice_mif_setting->start_y_lines;
					slice_mif_setting->l_ve_luma =
						slice_mif_setting->end_y_lines;
					slice_mif_setting->r_hs_luma =
						slice_mif_setting->start_x_lines;
					slice_mif_setting->r_he_luma =
						slice_mif_setting->end_x_lines;
					slice_mif_setting->r_vs_luma =
						slice_mif_setting->start_y_lines;
					slice_mif_setting->r_ve_luma =
						slice_mif_setting->end_y_lines;
					slice_mif_setting->l_hs_chrm =
						slice_mif_setting->l_hs_luma >> 1;
					slice_mif_setting->l_he_chrm =
						slice_mif_setting->l_he_luma >> 1;
					slice_mif_setting->r_hs_chrm =
						slice_mif_setting->r_hs_luma >> 1;
					slice_mif_setting->r_he_chrm =
						slice_mif_setting->r_he_luma >> 1;
					slice_mif_setting->l_vs_chrm =
						slice_mif_setting->l_vs_luma >> 1;
					slice_mif_setting->l_ve_chrm =
						slice_mif_setting->l_ve_luma >> 1;
					slice_mif_setting->r_vs_chrm =
						slice_mif_setting->r_vs_luma >> 1;
					slice_mif_setting->r_ve_chrm =
						slice_mif_setting->r_ve_luma >> 1;

					slice_mif_setting->h_skip = mif_setting->h_skip;
					slice_mif_setting->v_skip = mif_setting->v_skip;
					slice_mif_setting->hc_skip = mif_setting->hc_skip;
					slice_mif_setting->vc_skip = mif_setting->vc_skip;
					slice_mif_setting->skip_afbc = mif_setting->skip_afbc;
					slice_mif_setting->vpp_3d_mode = 0;
				}
				break;
			case VD1_SLICES_DOUT_PI:
				/* 4 pic */
				break;
			}
			break;
		case VD1_2_2SLICES_MODE:
			break;
		case VD1_SLICES01_MODE:
			switch (vd_proc_vd1_info->vd1_slices_dout_dpsel) {
			case VD1_SLICES_DOUT_2S4P:
				/* if one pic */
				/* whole frame in hsize, need 2slice, recal-mif scope*/
				for (slice = 0; slice < SLICE_NUM / 2; slice++) {
					/* layer->vd1s1_vd2_prebld_en = 1 case, used vd2 mif */
					if (slice == 1 && layer->vd1s1_vd2_prebld_en) {
						u32 vd2_slice = SLICE_NUM;

						slice_mif_setting =
							&layer->slice_mif_setting[slice];
						slice_mif_setting->id = vd2_slice;
					} else {
						slice_mif_setting =
							&layer->slice_mif_setting[slice];
						slice_mif_setting->id = slice;
					}
					slice_mif_setting->reverse = mif_setting->reverse;
					/* whole buffer size */
					slice_mif_setting->src_w = mif_setting->src_w;
					slice_mif_setting->src_h = mif_setting->src_h;
					slice_mif_setting->start_x_lines =
						vd_proc_slice_info->vd1_slice_x_st[slice];
					slice_mif_setting->end_x_lines =
						vd_proc_slice_info->vd1_slice_x_end[slice];
					slice_mif_setting->start_y_lines =
						mif_setting->start_y_lines;
					slice_mif_setting->end_y_lines =
						mif_setting->end_y_lines;

					slice_mif_setting->l_hs_luma =
						slice_mif_setting->start_x_lines;
					slice_mif_setting->l_he_luma =
						slice_mif_setting->end_x_lines;
					slice_mif_setting->l_vs_luma =
						slice_mif_setting->start_y_lines;
					slice_mif_setting->l_ve_luma =
						slice_mif_setting->end_y_lines;
					slice_mif_setting->r_hs_luma =
						slice_mif_setting->start_x_lines;
					slice_mif_setting->r_he_luma =
						slice_mif_setting->end_x_lines;
					slice_mif_setting->r_vs_luma =
						slice_mif_setting->start_y_lines;
					slice_mif_setting->r_ve_luma =
						slice_mif_setting->end_y_lines;
					slice_mif_setting->l_hs_chrm =
						slice_mif_setting->l_hs_luma >> 1;
					slice_mif_setting->l_he_chrm =
						slice_mif_setting->l_he_luma >> 1;
					slice_mif_setting->r_hs_chrm =
						slice_mif_setting->r_hs_luma >> 1;
					slice_mif_setting->r_he_chrm =
						slice_mif_setting->r_he_luma >> 1;
					slice_mif_setting->l_vs_chrm =
						slice_mif_setting->l_vs_luma >> 1;
					slice_mif_setting->l_ve_chrm =
						slice_mif_setting->l_ve_luma >> 1;
					slice_mif_setting->r_vs_chrm =
						slice_mif_setting->r_vs_luma >> 1;
					slice_mif_setting->r_ve_chrm =
						slice_mif_setting->r_ve_luma >> 1;

					slice_mif_setting->h_skip = mif_setting->h_skip;
					slice_mif_setting->v_skip = mif_setting->v_skip;
					slice_mif_setting->hc_skip = mif_setting->hc_skip;
					slice_mif_setting->vc_skip = mif_setting->vc_skip;
					slice_mif_setting->skip_afbc = mif_setting->skip_afbc;
					slice_mif_setting->vpp_3d_mode = 0;
				}
				break;
			case VD1_SLICES_DOUT_PI:
				/* 4 pic */
				break;
			}
			break;
		}
	}
}

static int get_vd1_work_mode(void)
{
	struct vd_proc_s *vd_proc = &g_vd_proc;
	struct vd_proc_vd1_info_s *vd_proc_vd1_info;

	vd_proc_vd1_info = &vd_proc->vd_proc_vd1_info;
	return vd_proc_vd1_info->vd1_work_mode;
}

#ifdef NEW_PRE_SCALER
static u32 get_prehsc_out_size(u32 prehsc_en,
	u32 prehsc_rate, u32 frm_src_w)
{
	u32 pre_src_w, src_w;

	src_w = frm_src_w;
	pre_src_w = prehsc_en ? (prehsc_rate == 0 ? src_w :
		prehsc_rate == 1 ? ((src_w + 1) >> 1) :
		prehsc_rate == 2 ? ((src_w + 3) >> 2) : src_w) : src_w;
	return pre_src_w;
}

static u32 get_prevsc_out_size(u32 prevsc_en,
	u32 prevsc_rate, u32 frm_src_h)
{
	u32 pre_src_h, src_h;

	src_h = frm_src_h;
	pre_src_h = prevsc_en ? (prevsc_en == 0 ? src_h :
		prevsc_en == 1 ? ((src_h + 1) >> 1) :
		prevsc_en == 2 ? ((src_h + 3) >> 2) : src_h) : src_h;
	return pre_src_h;
}
#endif

/* calcuate to din size for each slice */

/* 1. Calculate PPS scale ratio based on whole frame
 * Each slice's PPS scale ratios should be indentical to it.
 * 2. calculate vd1 unit slice input size :
 *    if PI/1S4P :  they are just input size;
 *    if 4SLICES4PPC or 2SLICES4PPC : get unit valid din size(without overlap)
 *    based on unit output size, then Add overlap size
 */
static void get_slice_input_size(struct vd_proc_s *vd_proc)
{
	u32 mosaic_mode, slice;
	/* slice proc unit output hsize(without overlap) */
	u32 valid_pix_pps_dout_hsize[SLICE_NUM];
	/* slice proc unit input hsize(without overlap) */
	u32 o_valid_pix_din_hsize[SLICE_NUM];
	u32 horz_phase_step;
	u32 pre_hsc_en = 0;
	u32 sr0_h_scaleup_en = 0, sr1_h_scaleup_en = 0;
#ifdef NEW_PRE_SCALER
	u32 pps_prehsc_dout_hsize;
	u32 pps_prevsc_dout_vsize;
#endif
	u32 valid_slice_num = 1;
	struct vd_proc_vd1_info_s *vd_proc_vd1_info;
	struct vd_proc_pps_s *vd_proc_pps;
	struct vd_proc_slice_info_s *vd_proc_slice_info;

	vd_proc_vd1_info = &vd_proc->vd_proc_vd1_info;
	vd_proc_slice_info = &vd_proc->vd_proc_slice_info;
	mosaic_mode = vd_proc_vd1_info->vd1_work_mode == VD1_2_2SLICES_MODE &&
		vd_proc_vd1_info->vd1_slices_dout_dpsel == VD1_SLICES_DOUT_4S4P;

	/* caculated vd_proc_slice_info and vd_proc_pps */
	if (vd_proc_vd1_info->vd1_slices_dout_dpsel == VD1_SLICES_DOUT_4S4P &&
		!mosaic_mode) {
#ifdef NEW_PRE_SCALER
		pps_prehsc_dout_hsize  =
			get_prehsc_out_size(vd_proc->vd_proc_unit[0].vd_proc_pps.prehsc_en,
				1, vd_proc_vd1_info->vd1_src_din_hsize[0]);
		pps_prevsc_dout_vsize  =
			get_prevsc_out_size(vd_proc->vd_proc_unit[0].vd_proc_pps.prevsc_en,
				1, vd_proc_vd1_info->vd1_src_din_vsize[0]);
#endif
		for (slice = 0; slice < SLICE_NUM; slice++) {
			vd_proc_pps = &vd_proc->vd_proc_unit[slice].vd_proc_pps;
			horz_phase_step = vd_proc_pps->horz_phase_step;
			pre_hsc_en = vd_proc_pps->prehsc_en;
			valid_pix_pps_dout_hsize[slice] =
				vd_proc_vd1_info->vd1_proc_unit_dout_hsize[slice];
			if (slice == SLICE_NUM - 1)
#ifdef NEW_PRE_SCALER
				o_valid_pix_din_hsize[slice] =
					pps_prehsc_dout_hsize -
					o_valid_pix_din_hsize[0] -
					o_valid_pix_din_hsize[1] -
					o_valid_pix_din_hsize[2];
#else
				o_valid_pix_din_hsize[slice] =
					vd_proc_vd1_info->vd1_src_din_hsize[0] -
					o_valid_pix_din_hsize[0] -
					o_valid_pix_din_hsize[1] -
					o_valid_pix_din_hsize[2];
#endif
			else
				cal_pps_din_hsize(&o_valid_pix_din_hsize[slice],
					valid_pix_pps_dout_hsize[slice],
					horz_phase_step,
					pre_hsc_en);
			if (debug_flag_s5 & DEBUG_VD_PROC)
				pr_info("o_valid_pix_din_hsize[%d]=%d, horz_phase_step=0x%x, pre_hsc_en=%d, valid_pix_pps_dout_hsize[slice]=%d\n",
					slice, o_valid_pix_din_hsize[slice], horz_phase_step,
					pre_hsc_en,
					valid_pix_pps_dout_hsize[slice]);
			vd_proc_slice_info->vd1_slice_din_hsize[slice] =
				o_valid_pix_din_hsize[slice] +
				((slice == 0 || slice == 3) ?
				vd_proc_vd1_info->vd1_overlap_hsize :
				vd_proc_vd1_info->vd1_overlap_hsize * 2);
			switch (slice) {
			case 0:
				vd_proc_slice_info->vd1_slice_x_st[slice] = 0;
				vd_proc_slice_info->vd1_slice_x_end[slice] =
					vd_proc_slice_info->vd1_slice_din_hsize[0] - 1;
				break;
			case 1:
				vd_proc_slice_info->vd1_slice_x_st[slice] =
					vd_proc_slice_info->vd1_slice_din_hsize[0] -
					vd_proc_vd1_info->vd1_overlap_hsize * 2;
				vd_proc_slice_info->vd1_slice_x_end[slice] =
					vd_proc_slice_info->vd1_slice_din_hsize[0] +
					vd_proc_slice_info->vd1_slice_din_hsize[1] -
					vd_proc_vd1_info->vd1_overlap_hsize * 2 - 1;
				break;
			case 2:
				vd_proc_slice_info->vd1_slice_x_st[slice] =
					vd_proc_slice_info->vd1_slice_din_hsize[0] +
					vd_proc_slice_info->vd1_slice_din_hsize[1] -
					vd_proc_vd1_info->vd1_overlap_hsize * 4;
				vd_proc_slice_info->vd1_slice_x_end[slice] =
					vd_proc_slice_info->vd1_slice_din_hsize[0] +
					vd_proc_slice_info->vd1_slice_din_hsize[1] +
					vd_proc_slice_info->vd1_slice_din_hsize[2] -
					vd_proc_vd1_info->vd1_overlap_hsize * 4 - 1;
				break;
			case 3:
#ifdef NEW_PRE_SCALER
				vd_proc_slice_info->vd1_slice_x_st[slice] =
					pps_prehsc_dout_hsize -
					vd_proc_slice_info->vd1_slice_din_hsize[3];
				vd_proc_slice_info->vd1_slice_x_end[slice] =
					pps_prehsc_dout_hsize - 1;
#else
				vd_proc_slice_info->vd1_slice_x_st[slice] =
					vd_proc_vd1_info->vd1_src_din_hsize[0] -
					vd_proc_slice_info->vd1_slice_din_hsize[3];
				vd_proc_slice_info->vd1_slice_x_end[slice] =
					vd_proc_vd1_info->vd1_src_din_hsize[0] - 1;
#endif
				break;
			}
#ifdef NEW_PRE_SCALER
			vd_proc_slice_info->vd1_slice_din_vsize[slice] =
				pps_prevsc_dout_vsize;
#else
			vd_proc_slice_info->vd1_slice_din_vsize[slice] =
				vd_proc_vd1_info->vd1_src_din_vsize[0];
#endif
			vd_proc_pps->slice_x_st = vd_proc_slice_info->vd1_slice_x_st[slice];
			vd_proc_pps->pps_slice = slice;
		}
		valid_slice_num = 4;
	} else if (vd_proc_vd1_info->vd1_slices_dout_dpsel ==
		VD1_SLICES_DOUT_2S4P || mosaic_mode) {
#ifdef NEW_PRE_SCALER
		pps_prehsc_dout_hsize  =
			get_prehsc_out_size(vd_proc->vd_proc_unit[0].vd_proc_pps.prehsc_en,
				1, vd_proc_vd1_info->vd1_src_din_hsize[0]);
		pps_prevsc_dout_vsize  =
			get_prevsc_out_size(vd_proc->vd_proc_unit[0].vd_proc_pps.prevsc_en,
				1, vd_proc_vd1_info->vd1_src_din_vsize[0]);
#endif
		for (slice = 0; slice < 2; slice++) {
			sr0_h_scaleup_en =
				vd_proc->vd_proc_unit[slice].vd_proc_sr0.sr_en &&
				vd_proc->vd_proc_unit[slice].vd_proc_sr0.h_scaleup_en;
			sr1_h_scaleup_en =
				vd_proc->vd_proc_unit[slice].vd_proc_sr1.sr_en &&
				vd_proc->vd_proc_unit[slice].vd_proc_sr1.h_scaleup_en;
			if (sr0_h_scaleup_en || sr1_h_scaleup_en)
				valid_pix_pps_dout_hsize[slice] =
					vd_proc_vd1_info->vd1_proc_unit_dout_hsize[slice] / 2;
			else
				valid_pix_pps_dout_hsize[slice] =
					vd_proc_vd1_info->vd1_proc_unit_dout_hsize[slice];
		}

		for (slice = 0; slice < 2; slice++) {
			vd_proc_pps = &vd_proc->vd_proc_unit[slice].vd_proc_pps;
			horz_phase_step = vd_proc_pps->horz_phase_step;
			pre_hsc_en = vd_proc_pps->prehsc_en;
			if (slice == 1)
#ifdef NEW_PRE_SCALER
				o_valid_pix_din_hsize[slice] =
					pps_prehsc_dout_hsize -
					o_valid_pix_din_hsize[0];
#else
				o_valid_pix_din_hsize[slice] =
					vd_proc_vd1_info->vd1_src_din_hsize[0] -
					o_valid_pix_din_hsize[0];
#endif
			else
				cal_pps_din_hsize(&o_valid_pix_din_hsize[slice],
					valid_pix_pps_dout_hsize[slice],
					horz_phase_step,
					pre_hsc_en);
			vd_proc_slice_info->vd1_slice_din_hsize[slice] =
				o_valid_pix_din_hsize[slice] +
				vd_proc_vd1_info->vd1_overlap_hsize;
			if (slice == 0) {
				vd_proc_slice_info->vd1_slice_x_st[slice] = 0;
				vd_proc_slice_info->vd1_slice_x_end[slice] =
					vd_proc_slice_info->vd1_slice_din_hsize[0] - 1;
			} else {
#ifdef NEW_PRE_SCALER
				vd_proc_slice_info->vd1_slice_x_st[slice] =
					pps_prehsc_dout_hsize -
					vd_proc_slice_info->vd1_slice_din_hsize[1];
				vd_proc_slice_info->vd1_slice_x_end[slice] =
					pps_prehsc_dout_hsize - 1;
#else
				vd_proc_slice_info->vd1_slice_x_st[slice] =
					vd_proc_vd1_info->vd1_src_din_hsize[0] -
					vd_proc_slice_info->vd1_slice_din_hsize[1];
				vd_proc_slice_info->vd1_slice_x_end[slice] =
					vd_proc_vd1_info->vd1_src_din_hsize[0] - 1;
#endif
			}
#ifdef NEW_PRE_SCALER
			vd_proc_slice_info->vd1_slice_din_vsize[slice] =
				pps_prevsc_dout_vsize;
#else
			vd_proc_slice_info->vd1_slice_din_vsize[slice] =
				vd_proc_vd1_info->vd1_src_din_vsize[0];
#endif
			vd_proc_pps->slice_x_st =
				vd_proc_slice_info->vd1_slice_x_st[slice];
			vd_proc_pps->pps_slice = slice;
		}
		valid_slice_num = 2;
	} else if (vd_proc_vd1_info->vd1_slices_dout_dpsel ==
		VD1_SLICES_DOUT_PI ||
		vd_proc_vd1_info->vd1_slices_dout_dpsel == VD1_SLICES_DOUT_1S4P) {
		if (vd_proc_vd1_info->vd1_work_mode == VD1_4SLICES_MODE)
			/* PI mode */
			valid_slice_num = 4;
		else
			valid_slice_num = 1;
		for (slice = 0; slice < valid_slice_num; slice++) {
			vd_proc_pps = &vd_proc->vd_proc_unit[slice].vd_proc_pps;
			horz_phase_step = vd_proc_pps->horz_phase_step;
			pre_hsc_en = vd_proc_pps->prehsc_en;
			vd_proc_slice_info->vd1_slice_din_hsize[slice] =
				vd_proc_vd1_info->vd1_src_din_hsize[slice];
			vd_proc_slice_info->vd1_slice_x_st[slice] = 0;
			vd_proc_slice_info->vd1_slice_x_end[slice] =
				vd_proc_slice_info->vd1_slice_din_hsize[slice]  - 1;
			vd_proc_slice_info->vd1_slice_din_vsize[slice] =
				vd_proc_vd1_info->vd1_src_din_vsize[slice];
			vd_proc_pps->slice_x_st =
				vd_proc_slice_info->vd1_slice_x_st[slice];
			vd_proc_pps->pps_slice = slice;
		}
	}
	/* todo */
	//if (vd_proc_vd1_info->vd1_slices_dout_dpsel == VD1_SLICES_DOUT_PI)
	if (debug_flag_s5 & DEBUG_VD_PROC) {
		pr_info("%s:vd1_work_mode=%d, vd1_slices_dout_dpsel=%d\n",
			__func__,
			vd_proc_vd1_info->vd1_work_mode,
			vd_proc_vd1_info->vd1_slices_dout_dpsel);
		for (slice = 0; slice < valid_slice_num; slice++) {
			pr_info("%s[slice:%d]:vd1_src_din_hsize(whole frame)=%d, vd1_proc_unit_dout_hsize(without overlap)=%d\n",
				__func__, slice,
				vd_proc_vd1_info->vd1_src_din_hsize[slice],
				vd_proc_vd1_info->vd1_proc_unit_dout_hsize[slice]);
			pr_info("%s:vd1_slice_din_hsize=%d, slice_x_st=%d, slice_x_end=%d\n",
				__func__,
				vd_proc_slice_info->vd1_slice_din_hsize[slice],
				vd_proc_slice_info->vd1_slice_x_st[slice],
				vd_proc_slice_info->vd1_slice_x_end[slice]);
		}
	}
}

static void vd1_proc_unit_param_set(struct vd_proc_s *vd_proc, u32 slice)
{
	u32 din_hsize, din_vsize;
	u32 dout_hsize, dout_vsize;
	u32 overlap_hsize;
	u32 s0_din_hsize_tmp = 0, s0_din_vsize_tmp = 0;
	u32 s1_din_hsize_tmp = 0, s1_din_vsize_tmp = 0;
	u32 hwincut_din_hsize = 0;
	u32 sr0_din_hsize = 0, sr0_din_vsize = 0;
	u32 sr0_dout_hsize = 0, sr0_dout_vsize = 0;
	u32 sr1_din_hsize = 0, sr1_din_vsize = 0;
	u32 sr1_dout_hsize = 0, sr1_dout_vsize = 0;
	u32 pps_din_hsize = 0, pps_din_vsize = 0;
	u32 pps_dout_hsize = 0, pps_dout_vsize = 0;
	u32 hwincut_bgn = 0, hwincut_end = 0;
	u32 dout_x_start = 0, dout_y_start = 0;
	struct vd_proc_slice_info_s *vd_proc_slice_info;
	struct vd_proc_vd1_info_s *vd_proc_vd1_info;
	struct vd_proc_unit_s *vd_proc_unit;

	vd_proc_vd1_info = &vd_proc->vd_proc_vd1_info;
	vd_proc_slice_info = &vd_proc->vd_proc_slice_info;
	vd_proc_unit = &vd_proc->vd_proc_unit[slice];

	din_hsize = vd_proc_slice_info->vd1_slice_din_hsize[slice];
	din_vsize = vd_proc_slice_info->vd1_slice_din_vsize[slice];
	dout_hsize = vd_proc_vd1_info->vd1_proc_unit_dout_hsize[slice];
	dout_vsize = vd_proc_vd1_info->vd1_proc_unit_dout_vsize[slice];
	dout_x_start = vd_proc_vd1_info->vd1_dout_x_start[slice];
	dout_y_start = vd_proc_vd1_info->vd1_dout_y_start[slice];
	overlap_hsize = vd_proc_vd1_info->vd1_overlap_hsize;
	if (vd_proc->vd_proc_pi.pi_en) {
		dout_x_start >>= 1;
		dout_y_start >>= 1;
	}
	if (din_hsize < overlap_hsize * 2) {
		pr_info("invalid param: vd1_slice_din_hsize(%d) < overlap_hsize*2(%d)\n",
			din_hsize,
			overlap_hsize * 2);
		return;
	}
	if (!din_hsize || !din_vsize || !dout_hsize || !dout_vsize) {
		pr_info("%s[slice:%d]:invalid input param: %d, %d, %d, %d\n",
			__func__,
			slice,
			din_hsize, din_vsize,
			dout_hsize, dout_vsize);
		return;
	}
	switch (slice) {
	case 0:
		if (vd_proc_unit->reg_bypass_prebld) {
			s0_din_hsize_tmp = din_hsize;
			s0_din_vsize_tmp = din_vsize;
		} else {
			/* s0 prebld work */
			s0_din_hsize_tmp = vd_proc_unit->prebld_hsize;
			s0_din_vsize_tmp = vd_proc_unit->prebld_vsize;
			if (vd_proc_unit->prebld_hsize < din_hsize ||
				vd_proc_unit->prebld_vsize < din_vsize) {
				pr_info("s0 prebld size set error:");
				pr_info("prebld hsize(%d)/vsize(%d), s0 din hsize(%d)/vsize(%d)\n",
					vd_proc_unit->prebld_hsize,
					vd_proc_unit->prebld_vsize,
					din_hsize,
					din_vsize);
			}
		}
		hwincut_din_hsize = s0_din_hsize_tmp * dout_hsize /
			(s0_din_hsize_tmp - overlap_hsize);
		/* sr1 */
		sr1_dout_hsize = hwincut_din_hsize;
		sr1_dout_vsize = dout_vsize;
		if (vd_proc_unit->vd_proc_sr1.h_scaleup_en)
			sr1_din_hsize = sr1_dout_hsize >> 1;
		else
			sr1_din_hsize = sr1_dout_hsize;
		if (vd_proc_unit->vd_proc_sr1.v_scaleup_en)
			sr1_din_vsize = sr1_dout_vsize >> 1;
		else
			sr1_din_vsize = sr1_dout_vsize;
		if (debug_flag_s5 & DEBUG_VD_PROC)
			pr_info("s0_din_h/vsize_tmp=%d, %d, sr 0 h/v_scaleup_en=%d,%d, sr1_din_hsize=%d,dout_hsize=%d\n",
				s0_din_hsize_tmp,
				s0_din_vsize_tmp,
				vd_proc_unit->vd_proc_sr0.h_scaleup_en,
				vd_proc_unit->vd_proc_sr0.v_scaleup_en,
				sr1_din_hsize,
				dout_hsize);

		if (vd_proc_unit->sr0_dpath_sel == SR0_IN_SLICE0) {
			if (vd_proc_unit->sr0_pps_dpsel == SR0_AFTER_PPS) {
				/* pps->sr0->sr1 */
				/* sr0 */
				sr0_dout_hsize = sr1_din_hsize;
				sr0_dout_vsize = sr1_din_vsize;
				if (vd_proc_unit->vd_proc_sr0.h_scaleup_en)
					sr0_din_hsize = sr0_dout_hsize >> 1;
				else
					sr0_din_hsize = sr0_dout_hsize;
				if (vd_proc_unit->vd_proc_sr0.v_scaleup_en)
					sr0_din_vsize = sr0_dout_vsize >> 1;
				else
					sr0_din_vsize = sr0_dout_vsize;
				/* pps */
				pps_dout_hsize = sr0_din_hsize;
				pps_dout_vsize = sr0_din_vsize;
				pps_din_hsize = s0_din_hsize_tmp;
				pps_din_vsize = s0_din_vsize_tmp;
			} else {
				/* sr0->pps->sr1 */
				/* sr0 */
				sr0_din_hsize   = s0_din_hsize_tmp;
				sr0_din_vsize   = s0_din_vsize_tmp;
				if (vd_proc_unit->vd_proc_sr0.h_scaleup_en)
					sr0_dout_hsize = sr0_din_hsize << 1;
				else
					sr0_dout_hsize = sr0_din_hsize;
				if (vd_proc_unit->vd_proc_sr0.v_scaleup_en)
					sr0_dout_vsize = sr0_din_vsize << 1;
				else
					sr0_dout_vsize = sr0_din_vsize;
				/* pps */
				pps_dout_hsize = sr1_din_hsize;
				pps_dout_vsize = sr1_din_vsize;
				pps_din_hsize = sr0_dout_hsize;
				pps_din_vsize = sr0_dout_vsize;
			}
		} else {
			/* sr0 in slice1: pps->sr1 */
			/* pps */
			pps_dout_hsize = sr1_din_hsize;
			pps_dout_vsize = sr1_din_vsize;
			pps_din_hsize = s0_din_hsize_tmp;
			pps_din_vsize = s0_din_vsize_tmp;
		}
		/* h_wincut */
		hwincut_bgn = 0;
		hwincut_end = dout_hsize - 1;
		break;
	case 1:
		if (vd_proc_unit->reg_bypass_prebld) {
			s1_din_hsize_tmp = din_hsize;
			s1_din_vsize_tmp = din_vsize;
		} else {
			/* s1 prebld work */
			s1_din_hsize_tmp = vd_proc_unit->prebld_hsize;
			s1_din_vsize_tmp = vd_proc_unit->prebld_vsize;
			if (vd_proc_unit->prebld_hsize < din_hsize ||
				vd_proc_unit->prebld_vsize < din_vsize) {
				pr_info("s1 prebld size set error:");
				pr_info("prebld hsize(%d)/vsize(%d), s1 din hsize(%d)/vsize(%d)\n",
					vd_proc_unit->prebld_hsize,
					vd_proc_unit->prebld_vsize,
					din_hsize,
					din_vsize);
			}
		}
		if (vd_proc_vd1_info->vd1_work_mode == VD1_4SLICES_MODE)
			hwincut_din_hsize = s1_din_hsize_tmp *
				dout_hsize / (s1_din_hsize_tmp - overlap_hsize * 2);
		else
			/* VD1_SLICES01_MODE */
			hwincut_din_hsize = s1_din_hsize_tmp *
				dout_hsize / (s1_din_hsize_tmp - overlap_hsize);
		if (vd_proc_unit->sr0_dpath_sel == SR0_IN_SLICE0) {
			/* only pps */
			pps_dout_hsize = hwincut_din_hsize;
			pps_dout_vsize = dout_vsize;
			pps_din_hsize = s1_din_hsize_tmp;
			pps_din_vsize = s1_din_vsize_tmp;
		} else {
			/* sr0 in slice1  pps->sr0 */
			/* sr0 */
			sr0_dout_hsize = hwincut_din_hsize;
			sr0_dout_vsize = dout_vsize;
			if (vd_proc_unit->vd_proc_sr0.h_scaleup_en)
				sr0_din_hsize = sr0_dout_hsize >> 1;
			else
				sr0_din_hsize = sr0_dout_hsize;
			if (vd_proc_unit->vd_proc_sr0.v_scaleup_en)
				sr0_din_vsize = sr0_dout_vsize >> 1;
			else
				sr0_din_vsize = sr0_dout_vsize;
			/* pps */
			pps_dout_hsize = sr0_din_hsize;
			pps_dout_vsize = sr0_din_vsize;
			pps_din_hsize = s1_din_hsize_tmp;
			pps_din_vsize = s1_din_vsize_tmp;
		}
		if (vd_proc_vd1_info->vd1_work_mode == VD1_4SLICES_MODE) {
			hwincut_bgn = (hwincut_din_hsize - dout_hsize) >> 1;
			hwincut_end = dout_hsize + hwincut_bgn - 1;
		} else {
			hwincut_bgn = hwincut_din_hsize - dout_hsize;
			hwincut_end = hwincut_din_hsize - 1;
		}
		break;
	case 2:
		if (vd_proc_vd1_info->vd1_work_mode == VD1_4SLICES_MODE)
			hwincut_din_hsize = s1_din_hsize_tmp *
				dout_hsize / (s1_din_hsize_tmp - overlap_hsize * 2);
		else
			/* VD1_SLICES01_MODE */
			hwincut_din_hsize = s1_din_hsize_tmp *
				dout_hsize / (s1_din_hsize_tmp - overlap_hsize);
		/* pps */
		pps_dout_hsize = hwincut_din_hsize;
		pps_dout_vsize = dout_vsize;
		pps_din_hsize = din_hsize;
		pps_din_vsize = din_vsize;
		if (vd_proc_vd1_info->vd1_work_mode == VD1_4SLICES_MODE) {
			hwincut_bgn = (hwincut_din_hsize - dout_hsize) >> 1;
			hwincut_end = dout_hsize + hwincut_bgn - 1;
		} else {
			hwincut_bgn = 0;
			hwincut_end = dout_hsize - 1;
		}
		break;
	case 3:
		hwincut_din_hsize = s1_din_hsize_tmp *
			dout_hsize / (s1_din_hsize_tmp - overlap_hsize);
		/* pps */
		pps_dout_hsize = hwincut_din_hsize;
		pps_dout_vsize = dout_vsize;
		pps_din_hsize = din_hsize;
		pps_din_vsize = din_vsize;

		hwincut_bgn = hwincut_din_hsize - dout_hsize;
		hwincut_end = hwincut_din_hsize - 1;
		break;
	}
	vd_proc_unit->din_hsize = din_hsize;
	vd_proc_unit->din_vsize = din_vsize;
	vd_proc_unit->dout_hsize = dout_hsize;
	vd_proc_unit->dout_vsize = dout_vsize;
	vd_proc_unit->dout_x_start = dout_x_start;
	vd_proc_unit->dout_y_start = dout_y_start;

	/* slice set */
	vd_proc_unit->vd_proc_slice.din_hsize = vd_proc_unit->din_hsize;
	vd_proc_unit->vd_proc_slice.din_vsize = vd_proc_unit->din_vsize;
	vd_proc_unit->vd_proc_slice.dout_hsize =
		hwincut_din_hsize;
	vd_proc_unit->vd_proc_slice.dout_vsize =
		vd_proc_unit->dout_vsize;

	/* pps param set */
	vd_proc_unit->vd_proc_pps.din_hsize = pps_din_hsize;
	vd_proc_unit->vd_proc_pps.din_vsize = pps_din_vsize;
	vd_proc_unit->vd_proc_pps.dout_hsize = pps_dout_hsize;
	vd_proc_unit->vd_proc_pps.dout_vsize = pps_dout_vsize;

	/* sr0 param set */
	vd_proc_unit->vd_proc_sr0.din_hsize = sr0_din_hsize;
	vd_proc_unit->vd_proc_sr0.din_vsize = sr0_din_vsize;
	vd_proc_unit->vd_proc_sr0.dout_hsize = sr0_dout_hsize;
	vd_proc_unit->vd_proc_sr0.dout_vsize = sr0_dout_vsize;

	/* sr1 param set */
	vd_proc_unit->vd_proc_sr1.din_hsize = sr1_din_hsize;
	vd_proc_unit->vd_proc_sr1.din_vsize = sr1_din_vsize;
	vd_proc_unit->vd_proc_sr1.dout_hsize = sr1_dout_hsize;
	vd_proc_unit->vd_proc_sr1.dout_vsize = sr1_dout_vsize;

	/* hwincut param set */
	vd_proc_unit->vd_proc_hwin.hwin_en = 1;
	vd_proc_unit->vd_proc_hwin.hwin_din_hsize = hwincut_din_hsize;
	vd_proc_unit->vd_proc_hwin.hwin_bgn = hwincut_bgn;
	vd_proc_unit->vd_proc_hwin.hwin_end = hwincut_end;

	if (debug_flag_s5 & DEBUG_VD_PROC) {
		pr_info("vd1 s%d: vd1_proc_unit_din_hsize/vsize: %d, %d\n",
			slice, din_hsize, din_vsize);
		pr_info("vd1 s%d: vd1_proc_unit_pps_din_hsize/vsize: %d, %d\n",
			slice, pps_din_hsize, pps_din_vsize);
		pr_info("vd1 s%d: vd1_proc_unit_pps_dout_hsize/vsize: %d, %d\n",
			slice, pps_dout_hsize, pps_dout_vsize);
		pr_info("vd1 s%d: vd1_proc_unit_sr0_din_hsize/vsize: %d, %d\n",
			slice, sr0_din_hsize, sr0_din_vsize);
		pr_info("vd1 s%d: vd1_proc_unit_sr0_dout_hsize/vsize: %d, %d\n",
			slice, sr0_dout_hsize, sr0_dout_vsize);
		pr_info("vd1 s%d: vd1_proc_unit_sr1_din_hsize/vsize: %d, %d\n",
			slice, sr1_din_hsize, sr1_din_vsize);
		pr_info("vd1 s%d: vd1_proc_unit_sr1_dout_hsize/vsize: %d, %d\n",
			slice, sr1_dout_hsize, sr1_dout_vsize);
		pr_info("vd1 s%d: vd1_proc_unit_hwincut_din_hsize: %d\n",
			slice, hwincut_din_hsize);
		pr_info("vd1 s%d: vd1_proc_unit_hwincut_din_bgn/bgn: %d, %d\n",
			slice, hwincut_bgn, hwincut_end);
		pr_info("vd1 s%d: vd1_proc_unit_dout_hsize/vsize: %d, %d\n",
			slice, dout_hsize, dout_vsize);
	}
}

#ifdef NEW_PRE_SCALER
static void recalc_vd1_slices_din_params(struct vd_proc_s *vd_proc, u32 slice)
{
	u32 pre_hsc_en = 0, pre_hsc_rate = 0;
	u32 pre_vsc_en = 0, pre_vsc_rate = 0;
	struct vd_proc_unit_s *vd_proc_unit;
	struct vd_proc_slice_info_s *vd_proc_slice_info;

	vd_proc_unit = &vd_proc->vd_proc_unit[slice];
	vd_proc_slice_info = &vd_proc->vd_proc_slice_info;
	pre_hsc_en = vd_proc_unit->vd_proc_pps.prehsc_en;
	pre_hsc_rate = vd_proc_unit->vd_proc_pps.prehsc_rate;
	pre_vsc_en = vd_proc_unit->vd_proc_pps.prevsc_en;
	pre_vsc_rate = vd_proc_unit->vd_proc_pps.prevsc_rate;
	if (pre_hsc_en && pre_hsc_rate) {
		vd_proc_slice_info->vd1_slice_din_hsize[slice] *= 2;
		vd_proc_slice_info->vd1_slice_x_st[slice] *= 2;
		vd_proc_slice_info->vd1_slice_x_end[slice] =
			(vd_proc_slice_info->vd1_slice_x_end[slice] + 1) * 2 - 1;
	}
	if (pre_vsc_en && pre_vsc_rate)
		vd_proc_slice_info->vd1_slice_din_vsize[slice] *= 2;
	if (debug_flag_s5 & DEBUG_VD_PROC) {
		pr_info("%s: pre_hsc_en: %d rate:%d, pre_vsc_en: %d, rate: %d\n",
			__func__,
			pre_hsc_en, pre_hsc_rate,
			pre_vsc_en, pre_vsc_rate);
		pr_info("vd1 s%d: vd1_proc_unit_din_hsize/vsize: %d, %d\n",
			slice,
			vd_proc_slice_info->vd1_slice_din_hsize[slice],
			vd_proc_slice_info->vd1_slice_din_vsize[slice]);
		pr_info("vd1 s%d: vd1_proc_unit_din:x_st=%d, x_end: %d\n",
			slice,
			vd_proc_slice_info->vd1_slice_x_st[slice],
			vd_proc_slice_info->vd1_slice_x_end[slice]);
	}
}
#endif

static void vd1_proc_unit_param_set_4s4p(struct vd_proc_s *vd_proc, u32 slice)
{
	u32 i, h_no_scale;
	u32 din_hsize, din_vsize;
	u32 dout_hsize[SLICE_NUM], dout_vsize;
	u32 overlap_hsize;
	u32 s0_din_hsize_tmp = 0, s0_din_vsize_tmp = 0;
	u32 s1_din_hsize_tmp = 0, s1_din_vsize_tmp = 0;
	u32 hwincut_din_hsize = 0;
	u32 sr0_din_hsize = 0, sr0_din_vsize = 0;
	u32 sr0_dout_hsize = 0, sr0_dout_vsize = 0;
	u32 sr1_din_hsize = 0, sr1_din_vsize = 0;
	u32 sr1_dout_hsize = 0, sr1_dout_vsize = 0;
	u32 pps_din_hsize = 0, pps_din_vsize = 0;
	u32 pps_dout_hsize = 0, pps_dout_vsize = 0;
	u32 hwincut_bgn = 0, hwincut_end = 0;
	u32 horz_phase_step = 0, slice_x_st = 0;
	u32 pre_hsc_en = 0;
	u32 slice_x_end[SLICE_NUM];
	u32 pps_dout_hsize0 = 0, pps_dout_hsize1 = 0;
	u32 hwincut_bgn0 = 0, hwincut_bgn1 = 0;
	u32 dout_exp = 0, sr_h_scaleup = 0;
	u32 dout_x_start = 0, dout_y_start = 0;
	struct vd_proc_slice_info_s *vd_proc_slice_info;
	struct vd_proc_vd1_info_s *vd_proc_vd1_info;
	struct vd_proc_unit_s *vd_proc_unit;
	struct vd_proc_sr_s *vd_proc_sr0 = NULL;
	struct vd_proc_sr_s *vd_proc_sr1 = NULL;

#ifdef NEW_PRE_SCALER
	recalc_vd1_slices_din_params(vd_proc, slice);
#endif
	vd_proc_vd1_info = &vd_proc->vd_proc_vd1_info;
	vd_proc_slice_info = &vd_proc->vd_proc_slice_info;
	vd_proc_unit = &vd_proc->vd_proc_unit[slice];
	din_hsize = vd_proc_slice_info->vd1_slice_din_hsize[slice];
	din_vsize = vd_proc_slice_info->vd1_slice_din_vsize[slice];

	dout_vsize = vd_proc_vd1_info->vd1_proc_unit_dout_vsize[slice];
	dout_x_start = vd_proc_vd1_info->vd1_dout_x_start[slice];
	dout_y_start = vd_proc_vd1_info->vd1_dout_y_start[slice];
	overlap_hsize = vd_proc_vd1_info->vd1_overlap_hsize;
	horz_phase_step = vd_proc_unit->vd_proc_pps.horz_phase_step;
	pre_hsc_en = vd_proc_unit->vd_proc_pps.prehsc_en;
	slice_x_st = vd_proc_unit->vd_proc_pps.slice_x_st;
	for (i = 0; i < SLICE_NUM; i++) {
		dout_hsize[i] = vd_proc_vd1_info->vd1_proc_unit_dout_hsize[i];
		slice_x_end[i] = vd_proc_slice_info->vd1_slice_x_end[i];
	}
	if (vd_proc_vd1_info->vd1_work_mode == VD1_4SLICES_MODE) {
		h_no_scale = (slice == 0 || slice == 3 ?
			(din_hsize - overlap_hsize) == dout_hsize[slice] :
			(din_hsize - overlap_hsize * 2) == dout_hsize[slice]);
	} else {
		/* 2slices */
		/* suppose SR0 IN SLICE1, SR1 IN SLICE0 if SR0/1 enabled.*/
		vd_proc_sr0 = &vd_proc->vd_proc_unit[1].vd_proc_sr0;
		vd_proc_sr1 = &vd_proc->vd_proc_unit[0].vd_proc_sr1;
		sr_h_scaleup = ((slice == 0) ?
			(vd_proc_sr0->sr_en && vd_proc_sr0->h_scaleup_en) :
			(vd_proc_sr1->sr_en && vd_proc_sr1->h_scaleup_en));
		h_no_scale = ((din_hsize - overlap_hsize) ==
			dout_hsize[slice]) / (1 << sr_h_scaleup);
	}
	if (debug_flag_s5 & DEBUG_PPS)
		pr_info("h_no_scale=0x%x, slice=%d, din_hsize=0x%x, dout_hsize[slice]=0x%x\n",
			h_no_scale,
			slice,
			din_hsize,
			dout_hsize[slice]);

	if (din_hsize < overlap_hsize * 2) {
		pr_info("invalid param: vd1_slice_din_hsize(%d) < overlap_hsize*2(%d)\n",
			din_hsize,
			overlap_hsize * 2);
		return;
	}
	if (!din_hsize || !din_vsize || !dout_hsize[slice] || !dout_vsize) {
		pr_info("invalid input param: din size:%d, %d, dout_size: %d, %d\n",
			din_hsize, din_vsize, dout_hsize[slice], dout_vsize);
		return;
	}
	switch (slice) {
	case 0:
		if (vd_proc_unit->reg_bypass_prebld) {
			s0_din_hsize_tmp = din_hsize;
			s0_din_vsize_tmp = din_vsize;
		} else {
			/* s0 prebld work */
			s0_din_hsize_tmp = vd_proc_unit->prebld_hsize;
			s0_din_vsize_tmp = vd_proc_unit->prebld_vsize;
			if (vd_proc_unit->prebld_hsize < din_hsize ||
				vd_proc_unit->prebld_vsize < din_vsize) {
				pr_info("s0 prebld size set error:");
				pr_info("prebld hsize(%d)/vsize(%d), s0 din hsize(%d)/vsize(%d)\n",
					vd_proc_unit->prebld_hsize,
					vd_proc_unit->prebld_vsize,
					din_hsize,
					din_vsize);
			}
		}
		if (vd_proc_unit->sr0_dpath_sel == SR0_IN_SLICE0 &&
			vd_proc_unit->sr0_en) {
			pr_info("%s error, sr0 in slice0 not support\n", __func__);
		} else {
			/* sr0 in slice1 : ->pps->sr1 */
			/* pps */
			pps_din_hsize = s0_din_hsize_tmp;
			if (h_no_scale)
				pps_dout_hsize = pps_din_hsize;
			else
				cal_pps_dout_hsize(&pps_dout_hsize,
					0,
					pps_din_hsize,
					horz_phase_step,
					pre_hsc_en);
			/* sr1 */
			sr1_din_hsize = pps_dout_hsize;
			if (vd_proc_unit->vd_proc_sr1.h_scaleup_en)
				sr1_dout_hsize = sr1_din_hsize * 2;
			else
				sr1_dout_hsize = sr1_din_hsize;
			sr1_dout_vsize = dout_vsize;
			if (vd_proc_unit->vd_proc_sr1.v_scaleup_en)
				sr1_din_vsize = sr1_dout_vsize / 2;
			else
				sr1_din_vsize = sr1_dout_vsize;
			/* pps */
			pps_dout_vsize = sr1_din_vsize;
			pps_din_vsize = s0_din_vsize_tmp;
			sr0_din_hsize = pps_din_hsize;
			sr0_din_vsize = pps_din_vsize;
			sr0_dout_hsize = pps_din_hsize;
			sr0_dout_vsize = pps_din_vsize;
		}
		/* h_wincut */
		hwincut_bgn = 0;
		hwincut_end = dout_hsize[slice] - 1;
		hwincut_din_hsize = sr1_dout_hsize;
		break;
	case 1:
		if (vd_proc_unit->reg_bypass_prebld) {
			s1_din_hsize_tmp = din_hsize;
			s1_din_vsize_tmp = din_vsize;
		} else {
			/* s1 prebld work */
			s1_din_hsize_tmp = vd_proc_unit->prebld_hsize;
			s1_din_vsize_tmp = vd_proc_unit->prebld_vsize;
			if (vd_proc_unit->prebld_hsize < din_hsize ||
				vd_proc_unit->prebld_vsize < din_vsize) {
				pr_info("s1 prebld size set error:");
				pr_info("prebld hsize(%d)/vsize(%d), s1 din hsize(%d)/vsize(%d)\n",
					vd_proc_unit->prebld_hsize,
					vd_proc_unit->prebld_vsize,
					din_hsize,
					din_vsize);
			}
		}
		if (vd_proc_unit->sr0_dpath_sel == SR0_IN_SLICE0) {
			/* sr0 in slice0 */
			/* only pps */
			pps_din_hsize = s1_din_hsize_tmp;
			if (h_no_scale) {
				pps_dout_hsize = pps_din_hsize;
			} else {
				cal_pps_dout_hsize(&pps_dout_hsize0,
					0, slice_x_st + 1, horz_phase_step,
					pre_hsc_en);
				cal_pps_dout_hsize(&pps_dout_hsize1,
					0, slice_x_end[slice] + 1, horz_phase_step,
					pre_hsc_en);
				pps_dout_hsize = pps_dout_hsize1 - pps_dout_hsize0;
				if (debug_flag_s5 & DEBUG_PPS) {
					pr_info("slice_x_st=0x%x, slice_x_end=0x%x, horz_phase_step=0x%x\n",
						slice_x_st,
						slice_x_end[slice],
						horz_phase_step);

					pr_info("pps_dout_hsize0=0x%x, pps_dout_hsize1=0x%x, pps_dout_hsize=0x%x\n",
						pps_dout_hsize0,
						pps_dout_hsize1,
						pps_dout_hsize);
				}
			}
			hwincut_din_hsize = pps_dout_hsize;
			pps_din_vsize = s1_din_vsize_tmp;
			pps_dout_vsize = dout_vsize;
		} else {
			/* sr0 in slice1  ->pps->sr0 */
			/* pps */
			pps_din_hsize = s1_din_hsize_tmp;
			if (h_no_scale)
				pps_dout_hsize = pps_din_hsize;
			else
				cal_pps_dout_hsize(&pps_dout_hsize,
					0, pps_din_hsize, horz_phase_step,
					pre_hsc_en);
			/* sr0 */
			sr0_din_hsize = pps_dout_hsize;
			if (vd_proc_unit->vd_proc_sr0.h_scaleup_en)
				sr0_dout_hsize = sr0_din_hsize * 2;
			else
				sr0_dout_hsize = sr0_din_hsize;
			hwincut_din_hsize = sr0_dout_hsize;

			sr0_dout_vsize = dout_vsize;
			if (vd_proc_unit->vd_proc_sr0.v_scaleup_en)
				sr0_din_vsize = sr0_dout_vsize / 2;
			else
				sr0_din_vsize = sr0_dout_vsize;
			/* pps */
			pps_dout_vsize = sr0_din_vsize;
			pps_din_vsize = s1_din_vsize_tmp;
		}
		if (h_no_scale) {
			hwincut_bgn = sr_h_scaleup ? overlap_hsize * 2 :
				overlap_hsize;
			hwincut_end = dout_hsize[slice] + hwincut_bgn - 1;
		} else {
			if (vd_proc_vd1_info->vd1_work_mode == VD1_4SLICES_MODE) {
				cal_pps_dout_hsize(&hwincut_bgn0,
					0, slice_x_st + 1, horz_phase_step,
					pre_hsc_en);
				cal_pps_dout_hsize(&hwincut_bgn1,
					0, slice_x_end[0] - overlap_hsize + 1,
					horz_phase_step,
					pre_hsc_en);
				dout_exp = dout_hsize[0] - hwincut_bgn1;
				hwincut_bgn = hwincut_bgn1 - hwincut_bgn0 + dout_exp;
				hwincut_end = hwincut_bgn + dout_hsize[slice] - 1;
			} else {
				hwincut_end =  hwincut_din_hsize - 1;
				hwincut_bgn =  hwincut_din_hsize - dout_hsize[1];
			}
		}
		break;
	case 2:
	case 3:
		/* pps */
		pps_din_hsize = din_hsize;
		if (h_no_scale) {
			pps_dout_hsize = pps_din_hsize;
		} else {
			cal_pps_dout_hsize(&pps_dout_hsize0,
				0, slice_x_st + 1, horz_phase_step,
				pre_hsc_en);
			cal_pps_dout_hsize(&pps_dout_hsize1,
				0, slice_x_end[slice] + 1, horz_phase_step,
				pre_hsc_en);
			pps_dout_hsize = pps_dout_hsize1 - pps_dout_hsize0;
		}
		hwincut_din_hsize = pps_dout_hsize;
		if (slice == 2) {
			if (vd_proc_vd1_info->vd1_work_mode == VD1_4SLICES_MODE) {
				if (h_no_scale) {
					hwincut_bgn = overlap_hsize;
					hwincut_end = dout_hsize[slice] + overlap_hsize - 1;
				} else {
					cal_pps_dout_hsize(&hwincut_bgn0,
						0, slice_x_st + 1, horz_phase_step,
						pre_hsc_en);
					cal_pps_dout_hsize(&hwincut_bgn1,
						0, slice_x_end[1] - (overlap_hsize - 1),
						horz_phase_step,
						pre_hsc_en);
					dout_exp = dout_hsize[0] * 2 - hwincut_bgn1;
					hwincut_bgn = hwincut_bgn1 - hwincut_bgn0 + dout_exp;
					hwincut_end = hwincut_bgn + dout_hsize[slice] - 1;
				}
			} else {
				/* 2slices work */
				hwincut_bgn = 0;
				hwincut_end = dout_hsize[slice] - 1;
			}
		} else {
			/* slice3 */
			if (h_no_scale) {
				hwincut_bgn = overlap_hsize;
				hwincut_end = dout_hsize[slice] + overlap_hsize - 1;
			} else {
				cal_pps_dout_hsize(&hwincut_bgn0,
					0, slice_x_st + 1, horz_phase_step,
					pre_hsc_en);
				cal_pps_dout_hsize(&hwincut_bgn1,
					0, slice_x_end[2] - (overlap_hsize - 1),
					horz_phase_step,
					pre_hsc_en);
				if (vd_proc_vd1_info->vd1_work_mode == VD1_4SLICES_MODE)
					dout_exp = dout_hsize[0] * 3 - hwincut_bgn1;
				else
					dout_exp = dout_hsize[0] - hwincut_bgn1;
				hwincut_bgn = hwincut_bgn1 - hwincut_bgn0 + dout_exp;
				hwincut_end = hwincut_bgn + dout_hsize[slice] - 1;
			}
		}
		pps_din_vsize = din_vsize;
		pps_dout_vsize = dout_vsize;
		break;
	}
	vd_proc_unit->din_hsize = din_hsize;
	vd_proc_unit->din_vsize = din_vsize;
	vd_proc_unit->dout_hsize = dout_hsize[slice];
	vd_proc_unit->dout_vsize = dout_vsize;
	vd_proc_unit->dout_x_start = dout_x_start;
	vd_proc_unit->dout_y_start = dout_y_start;

	/* pps param set */
	vd_proc_unit->vd_proc_pps.din_hsize = pps_din_hsize;
	vd_proc_unit->vd_proc_pps.din_vsize = pps_din_vsize;
	vd_proc_unit->vd_proc_pps.dout_hsize = pps_dout_hsize;
	vd_proc_unit->vd_proc_pps.dout_vsize = pps_dout_vsize;

	/* slice set */
	vd_proc_unit->vd_proc_slice.din_hsize = vd_proc_unit->din_hsize;
	vd_proc_unit->vd_proc_slice.din_vsize = vd_proc_unit->din_vsize;
	vd_proc_unit->vd_proc_slice.dout_hsize =
		hwincut_din_hsize;
	vd_proc_unit->vd_proc_slice.dout_vsize =
		vd_proc_unit->dout_vsize;

	/* sr0 param set */
	vd_proc_unit->vd_proc_sr0.din_hsize = sr0_din_hsize;
	vd_proc_unit->vd_proc_sr0.din_vsize = sr0_din_vsize;
	vd_proc_unit->vd_proc_sr0.dout_hsize = sr0_dout_hsize;
	vd_proc_unit->vd_proc_sr0.dout_vsize = sr0_dout_vsize;

	/* sr1 param set */
	vd_proc_unit->vd_proc_sr1.din_hsize = sr1_din_hsize;
	vd_proc_unit->vd_proc_sr1.din_vsize = sr1_din_vsize;
	vd_proc_unit->vd_proc_sr1.dout_hsize = sr1_dout_hsize;
	vd_proc_unit->vd_proc_sr1.dout_vsize = sr1_dout_vsize;

	/* hwincut param set */
	vd_proc_unit->vd_proc_hwin.hwin_en = 1;
	vd_proc_unit->vd_proc_hwin.hwin_din_hsize = hwincut_din_hsize;
	vd_proc_unit->vd_proc_hwin.hwin_bgn = hwincut_bgn;
	vd_proc_unit->vd_proc_hwin.hwin_end = hwincut_end;

	if (debug_flag_s5 & DEBUG_VD_PROC) {
		pr_info("%s\n",
			__func__);
		pr_info("vd1 s%d: vd1_proc_unit_din_hsize/vsize: %d, %d\n",
			slice, din_hsize, din_vsize);
		pr_info("vd1 s%d: vd1_proc_unit_pps_din_hsize/vsize: %d, %d\n",
			slice, pps_din_hsize, pps_din_vsize);
		pr_info("vd1 s%d: vd1_proc_unit_pps_dout_hsize/vsize: %d, %d\n",
			slice, pps_dout_hsize, pps_dout_vsize);
		pr_info("vd1 s%d: vd1_proc_unit_sr0_din_hsize/vsize: %d, %d\n",
			slice, sr0_din_hsize, sr0_din_vsize);
		pr_info("vd1 s%d: vd1_proc_unit_sr0_dout_hsize/vsize: %d, %d\n",
			slice, sr0_dout_hsize, sr0_dout_vsize);
		pr_info("vd1 s%d: vd1_proc_unit_sr1_din_hsize/vsize: %d, %d\n",
			slice, sr1_din_hsize, sr1_din_vsize);
		pr_info("vd1 s%d: vd1_proc_unit_sr1_dout_hsize/vsize: %d, %d\n",
			slice, sr1_dout_hsize, sr1_dout_vsize);
		pr_info("vd1 s%d: vd1_proc_unit_hwincut_din_hsize: %d\n",
			slice, hwincut_din_hsize);
		pr_info("vd1 s%d: vd1_proc_unit_hwincut_din_bgn/bgn: %d, %d\n",
			slice, hwincut_bgn, hwincut_end);
		pr_info("vd1 s%d: vd1_proc_unit_dout_hsize/vsize: %d, %d\n",
			slice, dout_hsize[slice], dout_vsize);
	}
}

static void vd_proc_param_set_vd1(struct vd_proc_s *vd_proc)
{
	u32 vd1_proc_dout_hsize, vd1_proc_dout_vsize;
	struct vd_proc_vd1_info_s *vd_proc_vd1_info;
	struct vd_proc_mosaic_s *vd_proc_mosaic;
	struct vd_proc_preblend_info_s *vd_proc_preblend_info;
	struct vd_proc_vd2_info_s *vd_proc_vd2_info;
	struct vd_proc_slice_info_s *vd_proc_slice_info;
	int slice;

	if (!vd_proc->vd1_used)
		return;

	vd_proc_vd1_info = &vd_proc->vd_proc_vd1_info;
	vd_proc_vd2_info = &vd_proc->vd_proc_vd2_info;

	if (vd_proc_vd1_info->vd1_work_mode == VD1_1SLICES_MODE)
		vd_proc_vd1_info->vd1_overlap_hsize = 0;

	get_slice_input_size(vd_proc);

	vd1_proc_dout_hsize =  vd_proc_vd1_info->vd1_dout_hsize;
	vd1_proc_dout_vsize =  vd_proc_vd1_info->vd1_dout_vsize;
	vd_proc_slice_info = &vd_proc->vd_proc_slice_info;
	if (vd_proc_vd1_info->vd1_slices_dout_dpsel == VD1_SLICES_DOUT_PI)
		vd_proc->vd_proc_pi.pi_en = 1;
	else
		vd_proc->vd_proc_pi.pi_en = 0;
	/* mosaic related setting, need reset if needed todo */
	vd_proc_mosaic = &vd_proc->vd_proc_mosaic;
	vd_proc_mosaic->mosaic_vd1_dout_hsize = vd_proc_vd1_info->vd1_dout_hsize;
	vd_proc_mosaic->mosaic_vd1_dout_vsize = vd_proc_vd1_info->vd1_dout_vsize;
	vd_proc_mosaic->mosaic_vd2_dout_hsize = vd_proc_vd2_info->vd2_dout_hsize;
	vd_proc_mosaic->mosaic_vd2_dout_vsize = vd_proc_vd2_info->vd2_dout_vsize;
	for (slice = 0; slice < SLICE_NUM; slice++) {
		vd_proc_mosaic->vd1_proc_slice_pad_h_bgn[slice] = 0;
		vd_proc_mosaic->vd1_proc_slice_pad_v_bgn[slice] = 0;
		vd_proc_mosaic->vd1_proc_slice_pad_h_end[slice] =
			vd_proc_mosaic->vd1_proc_slice_pad_h_bgn[slice] +
			vd_proc_vd1_info->vd1_proc_unit_dout_hsize[slice] - 1;
		vd_proc_mosaic->vd1_proc_slice_pad_v_end[slice] =
			vd_proc_mosaic->vd1_proc_slice_pad_v_bgn[slice] +
			vd_proc_vd1_info->vd1_proc_unit_dout_vsize[slice] - 1;
		vd_proc_mosaic->vd1_proc_slice_pad_en[slice] = 0;
	}
	/* slice 0 : preblend */
	vd_proc_preblend_info = &vd_proc->vd_proc_preblend_info;
	if (vd_proc_preblend_info->vd1s0_vd2_prebld_en &&
		vd_proc_preblend_info->vd1s1_vd2_prebld_en) {
		pr_info("vd1s0_vd2_prebld_en and vd1s1_vd2_prebld_en can't set both\nn");
		return;
	}
	vd_proc->vd_proc_unit[0].prebld_hsize =
		vd_proc_preblend_info->prebld_dout_hsize;
	vd_proc->vd_proc_unit[0].prebld_vsize =
		vd_proc_preblend_info->prebld_dout_vsize;

	if (vd_proc_preblend_info->vd1s0_vd2_prebld_en)
		vd_proc->vd_proc_unit[0].reg_bypass_prebld = 0;
	else
		vd_proc->vd_proc_unit[0].reg_bypass_prebld = 1;

	/* for 4k120hz dv, fake vd2 */
	/* slice 1 : preblend */
	if (vd_proc_vd2_info->vd2_dout_dpsel == VD2_DOUT_PREBLD1) {
		vd_proc->vd2_used = 1;
		vd_proc_vd2_info->vd2_din_hsize =
			vd_proc_slice_info->vd1_slice_x_end[1] -
			vd_proc_slice_info->vd1_slice_x_st[1] + 1;
		vd_proc_vd2_info->vd2_din_vsize =
			vd_proc_vd1_info->vd1_src_din_vsize[0];
		vd_proc_vd2_info->vd2_dout_hsize =
			vd_proc_vd2_info->vd2_din_hsize;
		vd_proc_vd2_info->vd2_dout_vsize =
			vd_proc_vd2_info->vd2_din_vsize;
		vd_proc_preblend_info->prebld_dout_hsize =
			vd_proc_vd2_info->vd2_dout_hsize;
		vd_proc_preblend_info->prebld_dout_vsize =
			vd_proc_vd2_info->vd2_dout_vsize;
		vd_proc_vd2_info->vd2_dout_x_start =
			vd_proc->vd_proc_unit[1].dout_x_start;
		vd_proc_vd2_info->vd2_dout_y_start =
			vd_proc->vd_proc_unit[1].dout_y_start;

		vd_proc->vd_proc_unit[1].prebld_hsize =
			vd_proc_preblend_info->prebld_dout_hsize;
		vd_proc->vd_proc_unit[1].prebld_vsize =
			vd_proc_preblend_info->prebld_dout_vsize;
		if (vd_proc_preblend_info->vd1s1_vd2_prebld_en)
			vd_proc->vd_proc_unit[1].reg_bypass_prebld = 0;
		else
			vd_proc->vd_proc_unit[1].reg_bypass_prebld = 1;
		if (debug_flag_s5 & DEBUG_VD_PROC)
			pr_info("vd2 din_hsize/v=0x%x, 0x%x, dout_hsize/v=0x%x,0x%x, dout x/y=%d, %d\n",
				vd_proc_vd2_info->vd2_din_hsize,
				vd_proc_vd2_info->vd2_din_vsize,
				vd_proc_vd2_info->vd2_dout_hsize,
				vd_proc_vd2_info->vd2_dout_vsize,
				vd_proc_vd2_info->vd2_dout_x_start,
				vd_proc_vd2_info->vd2_dout_y_start);
	} else {
		vd_proc->vd_proc_unit[1].reg_bypass_prebld = 1;
	}

	for (slice = 0; slice < vd_proc_vd1_info->slice_num; slice++) {
		if ((vd_proc_vd1_info->vd1_work_mode == VD1_4SLICES_MODE &&
			vd_proc_vd1_info->vd1_slices_dout_dpsel ==
			VD1_SLICES_DOUT_PI) ||
			vd_proc_vd1_info->vd1_work_mode == VD1_1SLICES_MODE)
			/* Calculate modules size from dout->din */
			vd1_proc_unit_param_set(vd_proc, slice);
		else
			/* Calculate modules size from din->dout */
			vd1_proc_unit_param_set_4s4p(vd_proc, slice);
	}

	/* blend_1ppc param init */
	if (vd_proc_vd1_info->vd1_slices_dout_dpsel == VD1_SLICES_DOUT_PI ||
		vd_proc_vd1_info->vd1_slices_dout_dpsel == VD1_SLICES_DOUT_1S4P) {
		u32 blend_1ppc_out_hsize, blend_1ppc_out_vsize;
		const struct vinfo_s *vinfo = NULL;

		vinfo = get_current_vinfo();
		blend_1ppc_out_hsize  = vinfo->width;
		blend_1ppc_out_vsize  = vinfo->field_height;
		if (vd_proc->vd_proc_pi.pi_en) {
			blend_1ppc_out_hsize >>= 1;
			blend_1ppc_out_vsize >>= 1;
		}
		vd_blend_1ppc_param_set(vd_proc,
			&vd_proc->vd_proc_blend,
			blend_1ppc_out_hsize,
			blend_1ppc_out_vsize, 1);
	} else {
		vd_blend_1ppc_param_set(vd_proc,
			&vd_proc->vd_proc_blend,
			0, 0, 1);
	}
}

static void vd_proc_param_set_vd2(struct vd_proc_s *vd_proc)
{
	u32 din_hsize = 0, din_vsize = 0;
	u32 dout_hsize = 0, dout_vsize = 0;
	u32 pps_din_hsize = 0, pps_din_vsize = 0;
	u32 pps_dout_hsize = 0, pps_dout_vsize = 0;
	u32 vd2_dout_x_start = 0, vd2_dout_y_start = 0;
	struct vd_proc_vd2_info_s *vd_proc_vd2_info;
	struct vd2_proc_s *vd2_proc = &vd_proc->vd2_proc;

	if (!vd_proc->vd2_used)
		return;
	vd_proc_vd2_info = &vd_proc->vd_proc_vd2_info;
	din_hsize = vd_proc_vd2_info->vd2_din_hsize;
	din_vsize = vd_proc_vd2_info->vd2_din_vsize;
	dout_hsize = vd_proc_vd2_info->vd2_dout_hsize;
	dout_vsize = vd_proc_vd2_info->vd2_dout_vsize;

	pps_din_hsize = din_hsize;
	pps_din_vsize = din_vsize;
	if (vd_proc_vd2_info->vd2_dout_dpsel == VD2_DOUT_PI) {
		vd2_proc->vd_proc_pi.pi_en = 1;
		pps_dout_hsize = dout_hsize;
		pps_dout_vsize = dout_vsize;
		dout_hsize = dout_hsize * 2;
		dout_vsize = dout_vsize * 2;
	} else {
		pps_dout_hsize = dout_hsize;
		pps_dout_vsize = dout_vsize;
	}
	vd2_dout_x_start = vd_proc_vd2_info->vd2_dout_x_start;
	vd2_dout_y_start = vd_proc_vd2_info->vd2_dout_y_start;

	vd2_proc->din_hsize = din_hsize;
	vd2_proc->din_vsize = din_vsize;
	vd2_proc->dout_hsize = dout_hsize;
	vd2_proc->dout_vsize = dout_vsize;
	vd2_proc->vd2_dout_x_start = vd2_dout_x_start;
	vd2_proc->vd2_dout_y_start = vd2_dout_y_start;
	vd2_proc->vd2_dout_dpsel = vd_proc_vd2_info->vd2_dout_dpsel;

	vd2_proc->vd_proc_pps.din_hsize = pps_din_hsize;
	vd2_proc->vd_proc_pps.din_vsize = pps_din_vsize;
	vd2_proc->vd_proc_pps.dout_hsize = pps_dout_hsize;
	vd2_proc->vd_proc_pps.dout_vsize = pps_dout_vsize;

	vd2_proc->bypass_dv = vd_proc->bypass_dv;
	vd2_proc->bypass_detunnel = vd_proc->bypass_detunnel;
	vd2_proc->bypass_hdr = vd_proc->bypass_hdr;
	//vd2_proc->dolby_en = 0;
	if (debug_flag_s5 & DEBUG_VD_PROC)
		pr_info("%s: din size: %d, %d, dout size: %d, %d, x/y start: %d, %d\n",
			__func__,
			vd2_proc->din_hsize,
			vd2_proc->din_vsize,
			vd2_proc->dout_hsize,
			vd2_proc->dout_vsize,
			vd2_proc->vd2_dout_x_start,
			vd2_proc->vd2_dout_y_start);
}

static void vd_proc_param_set(struct vd_proc_s *vd_proc)
{
	u32 vd2_dout_hsize = 0, vd2_dout_vsize = 0;
	u32 slice_num = 0, bld_out_en = 0;

	if (cur_dev->display_module != S5_DISPLAY_MODULE)
		return;

	if (!vd_proc)
		return;

	if (vd_proc->vd1_used)
		vd_proc_param_set_vd1(vd_proc);
	if (vd_proc->vd2_used)
		vd_proc_param_set_vd2(vd_proc);
	/* for preblend */
	if (vd_proc->vd_proc_preblend_info.vd1s0_vd2_prebld_en) {
		slice_num = 2;
		bld_out_en = 1;
		if (vd_proc->vd_proc_vd2_info.vd2_dout_dpsel ==
			VD2_DOUT_PI) {
			vd2_dout_hsize =
				vd_proc->vd_proc_vd2_info.vd2_dout_hsize / 2;
			vd2_dout_vsize =
				vd_proc->vd_proc_vd2_info.vd2_dout_vsize / 2;
		}
		vd_proc->vd_proc_unit[1].dout_hsize = vd2_dout_hsize;
		vd_proc->vd_proc_unit[1].dout_vsize = vd2_dout_vsize;
		vd_preblend_param_set(vd_proc,
			&vd_proc->vd_proc_preblend,
			vd_proc->vd_proc_unit[1].prebld_hsize,
			vd_proc->vd_proc_unit[1].prebld_vsize,
			slice_num, bld_out_en);

	} else if (vd_proc->vd_proc_preblend_info.vd1s1_vd2_prebld_en) {
		slice_num = 1;
		bld_out_en = 1;
		vd_proc->vd_proc_unit[0].dout_hsize = 0;
		vd_proc->vd_proc_unit[0].dout_vsize = 0;
		vd_proc->vd_proc_unit[1].dout_hsize =
			vd_proc->vd_proc_vd2_info.vd2_dout_hsize;
		vd_proc->vd_proc_unit[1].dout_vsize =
			vd_proc->vd_proc_vd2_info.vd2_dout_vsize;
		vd_preblend_param_set(vd_proc,
			&vd_proc->vd_proc_preblend,
			vd_proc->vd_proc_unit[1].prebld_hsize,
			vd_proc->vd_proc_unit[1].prebld_vsize,
			slice_num, bld_out_en);
		//vd_proc->vd_proc_unit[1].reg_bypass_prebld = 0;
	} else {
		/* disable preblend */
		slice_num = 0;
		bld_out_en = 0;
		vd_preblend_param_set(vd_proc,
			&vd_proc->vd_proc_preblend,
			0, 0, slice_num, bld_out_en);
		vd_proc->vd_proc_unit[0].reg_bypass_prebld = 1;
		vd_proc->vd_proc_unit[1].reg_bypass_prebld = 1;
	}
	if (debug_flag_s5 & DEBUG_VD_PROC) {
		pr_info("%s:reg_bypass_prebld: %d, %d\n",
			__func__,
			vd_proc->vd_proc_unit[0].reg_bypass_prebld,
			vd_proc->vd_proc_unit[1].reg_bypass_prebld);
	}
}

/* sur_idx set to 0 for the first half */
/* sur_idx set to 1 for the second half */
static void vd_switch_frm_idx(u32 vpp_index, u32 frm_idx)
{
	rdma_wr_bits_op rdma_wr_bits = cur_dev->rdma_func[vpp_index].rdma_wr_bits;

	if (frm_idx == 0) {
		rdma_wr_bits(VIU_VIU0_MISC, frm_idx, 0, 1);
		rdma_wr_bits(S5_VIU_VD1_MISC, 1, 8, 6);
	} else if (frm_idx == 1) {
		rdma_wr_bits(VIU_VIU0_MISC, frm_idx, 0, 1);
		rdma_wr_bits(S5_VIU_VD1_MISC, 2, 8, 6);
	}
}

static void update_vd_proc_amdv_info(struct vd_proc_s *vd_proc)
{
	int i;

	vd_proc_amdv.vd2_prebld_4k120_en =
		vd_proc->vd_proc_preblend_info.vd1s1_vd2_prebld_en;
	for (i = 0; i < vd_proc->vd_proc_vd1_info.slice_num; i++) {
		vd_proc_amdv.slice[i].hsize =
			vd_proc->vd_proc_slice_info.vd1_slice_din_hsize[i];
		vd_proc_amdv.slice[i].vsize =
			vd_proc->vd_proc_slice_info.vd1_slice_din_vsize[i];
	}
}

struct vd_proc_info_t *get_vd_proc_amdv_info(void)
{
	return &vd_proc_amdv;
}

static void update_vpp_post_amdv_info(struct vpp_post_s *vpp_post)
{
	int i;

	vpp_post_amdv.slice_num = vpp_post->slice_num;
	vpp_post_amdv.overlap_hsize = vpp_post->overlap_hsize;
	vpp_post_amdv.vpp_post_blend_hsize = vpp_post->vpp_post_blend.bld_out_w;
	vpp_post_amdv.vpp_post_blend_vsize = vpp_post->vpp_post_blend.bld_out_h;
	for (i = 0; i < vpp_post->slice_num; i++) {
		vpp_post_amdv.slice[i].hsize =
			vpp_post->vpp_post_proc.vpp_post_proc_slice.hsize[i];
		vpp_post_amdv.slice[i].vsize =
			vpp_post->vpp_post_proc.vpp_post_proc_slice.vsize[i];
	}
}

struct vpp_post_info_t *get_vpp_post_amdv_info(void)
{
	return &vpp_post_amdv;
}

void vd_s5_hw_set(struct video_layer_s *layer,
	struct vframe_s *dispbuf, struct vpp_frame_par_s *frame_par)
{
	struct vd_proc_s *vd_proc = &g_vd_proc;
	u32 mosaic_mode = 0;
	u32 vpp_index = VPP0;

	if (cur_dev->display_module != S5_DISPLAY_MODULE)
		return;

	if (!layer || !dispbuf || !frame_par)
		return;

	set_vd_proc_info(layer);

	vd_switch_frm_idx(vpp_index, 0);
	vd_proc_param_set(vd_proc);
	if (dispbuf) {
		/* adjust mif/afbc addr, scope, src_w, src_h */
		set_vd_src_info(layer);
		vd_set_dcu_s5(layer->layer_id, layer,
				frame_par, dispbuf);
		_vd_mif_setting_s5(layer, &layer->mif_setting);
		_vd_fgrain_config_s5(layer,
			      frame_par,
			      dispbuf);
		_vd_fgrain_setting_s5(layer, dispbuf);
	}
	/* update info for dv */
	update_vd_proc_amdv_info(vd_proc);

	vd_proc_set(vpp_index, vd_proc);
	if (mosaic_mode) {
		vd_mosaic_slices_padding_set(vpp_index, 0, vd_proc);
		vd_switch_frm_idx(vpp_index, 1);
		//mif read for second half
		//fg config
		//todo
		vd_proc_param_set(vd_proc);
		vd_proc_set(vpp_index, vd_proc);
		vd_mosaic_slices_padding_set(vpp_index, 1, vd_proc);
	}
}

static int get_sr_core_support_s5(struct amvideo_device_data_s *p_amvideo)
{
	u32 core_support = 0;

	if (p_amvideo->sr0_support == 1 &&
		p_amvideo->sr1_support == 1) {
		core_support = NEW_CORE0_CORE1;
	} else if (p_amvideo->sr0_support == 1) {
		core_support = ONLY_CORE0;
	} else if (p_amvideo->sr1_support == 1) {
		core_support = ONLY_CORE1;
	}
	return core_support;
}

static void vpp_sr_init_s5(struct amvideo_device_data_s *p_amvideo)
{
	struct sr_info_s *sr;

	sr = &sr_info;
	/* sr_info */
	if (p_amvideo->sr0_support == 1) {
		sr->sr_support |= SUPER_CORE0_SUPPORT;
		sr->core0_v_disable_width_max =
			p_amvideo->core_v_disable_width_max[0];
		sr->core0_v_enable_width_max =
			p_amvideo->core_v_enable_width_max[0];
	}
	if (p_amvideo->sr1_support == 1) {
		sr->sr_support |= SUPER_CORE1_SUPPORT;
		sr->core1_v_disable_width_max =
			p_amvideo->core_v_disable_width_max[1];
		sr->core1_v_enable_width_max =
			p_amvideo->core_v_enable_width_max[1];
	}
	sr->supscl_path = p_amvideo->supscl_path;
	sr->core_support = get_sr_core_support_s5(p_amvideo);
}

static void vd1_path_select_s5(struct video_layer_s *layer,
			    bool afbc, bool di_afbc,
			    bool di_post, bool di_pre_link)
{
	/* to do */
}

static void vdx_path_select_s5(struct video_layer_s *layer,
			    bool afbc, bool di_afbc)
{
	/* to do */
}

static void vd_set_blk_mode_slice_s5(struct video_layer_s *layer, u32 slice, u8 block_mode)
{
	struct vd_mif_reg_s *vd_mif_reg = NULL;
	struct vd_mif_linear_reg_s *vd_mif_linear_reg = NULL;

	u32 pic_32byte_aligned = 0;
	u8 vpp_index;

	if (slice > SLICE_NUM)
		return;

	vd_mif_reg = &vd_proc_reg.vd_mif_reg[slice];
	vd_mif_linear_reg = &vd_proc_reg.vd_mif_linear_reg[slice];
	vpp_index = layer->vpp_index;
	cur_dev->rdma_func[vpp_index].rdma_wr_bits(vd_mif_reg->vd_if0_gen_reg3,
		block_mode, 12, 2);
	cur_dev->rdma_func[vpp_index].rdma_wr_bits(vd_mif_reg->vd_if0_gen_reg3,
		block_mode, 14, 2);
	if (block_mode)
		pic_32byte_aligned = 7;
	cur_dev->rdma_func[vpp_index].rdma_wr_bits(vd_mif_reg->vd_if0_gen_reg3,
		(pic_32byte_aligned << 6) |
		(block_mode << 4) |
		(block_mode << 2) |
		(block_mode << 0),
		18,
		9);
	/* VD1_IF0_STRIDE_1_F1 bit31:18 same as vd_if0_gen_reg3 */
	if (process_3d_type & MODE_3D_ENABLE) {
		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(vd_mif_linear_reg->vd_if0_stride_1_f1,
			(pic_32byte_aligned << 6) |
			(block_mode << 4) |
			(block_mode << 2) |
			(block_mode << 0),
			18,
			9);
	}
}

static void vd_set_blk_mode_s5(struct video_layer_s *layer, u8 block_mode)
{
	struct vd_mif_reg_s *vd_mif_reg = NULL;
	struct vd_mif_linear_reg_s *vd_mif_linear_reg = NULL;
	u32 layer_index = 0;
	u32 pic_32byte_aligned = 0;
	u8 vpp_index;

	if (layer->layer_id >= MAX_VD_CHAN_S5)
		return;
	if (!layer->layer_id)
		layer_index = 0;
	else
		layer_index = layer->layer_id + SLICE_NUM - 1;

	vd_mif_reg = &vd_proc_reg.vd_mif_reg[layer_index];
	vd_mif_linear_reg = &vd_proc_reg.vd_mif_linear_reg[layer_index];
	vpp_index = layer->vpp_index;
	cur_dev->rdma_func[vpp_index].rdma_wr_bits(vd_mif_reg->vd_if0_gen_reg3,
		block_mode, 12, 2);
	cur_dev->rdma_func[vpp_index].rdma_wr_bits(vd_mif_reg->vd_if0_gen_reg3,
		block_mode, 14, 2);
	if (block_mode)
		pic_32byte_aligned = 7;
	cur_dev->rdma_func[vpp_index].rdma_wr_bits(vd_mif_reg->vd_if0_gen_reg3,
		(pic_32byte_aligned << 6) |
		(block_mode << 4) |
		(block_mode << 2) |
		(block_mode << 0),
		18,
		9);
	/* VD1_IF0_STRIDE_1_F1 bit31:18 same as vd_if0_gen_reg3 */
	if (process_3d_type & MODE_3D_ENABLE) {
		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(vd_mif_linear_reg->vd_if0_stride_1_f1,
			(pic_32byte_aligned << 6) |
			(block_mode << 4) |
			(block_mode << 2) |
			(block_mode << 0),
			18,
			9);
	}
}

static void vd1_set_dcu_s5(struct video_layer_s *layer,
		struct vpp_frame_par_s *frame_par,
		struct vframe_s *vf)
{
	u32 r;
	u32 vphase, vini_phase, vformatter, vrepeat, hphase = 0;
	u32 hformatter;
	u32 pat, loop;
	static const u32 vpat[MAX_VSKIP_COUNT + 1] = {
		0, 0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf};
	u32 u, v;
	u32 type, bit_mode = 0, canvas_w;
	bool is_mvc = false;
	u8 burst_len = 1;
	struct vd_mif_reg_s *vd_mif_reg = NULL;
	struct vd_mif_reg_s *vd2_mif_reg = NULL;
	struct vd_afbc_reg_s *vd_afbc_reg = NULL;
	bool di_post = false, di_pre_link = false;
	u8 vpp_index = layer->vpp_index;
	u8 layer_id = layer->layer_id;

	if (!vf) {
		pr_info("%s vf NULL, return\n", __func__);
		return;
	}
	if (layer_id >= 1)
		return;
	vd_mif_reg = &vd_proc_reg.vd_mif_reg[layer_id];
	vd2_mif_reg = &vd_proc_reg.vd_mif_reg[SLICE_NUM];
	vd_afbc_reg = &vd_proc_reg.vd_afbc_reg[layer_id];

	type = vf->type;
	if (type & VIDTYPE_MVC)
		is_mvc = true;

	pr_debug("%s for vd%d %p, type:0x%x, flag:%x\n",
		 __func__, layer->layer_id, vf, type, vf->flag);

#ifdef CONFIG_AMLOGIC_MEDIA_DEINTERLACE
	if (is_di_post_mode(vf) && is_di_post_on())
		di_post = true;
#ifdef ENABLE_PRE_LINK
	if (is_pre_link_on(layer, vf))
		di_pre_link = true;
#endif
#endif

	if (frame_par->nocomp)
		type &= ~VIDTYPE_COMPRESS;

	if (type & VIDTYPE_COMPRESS) {
		if (conv_lbuf_len_s5[layer->layer_id] == VIDEO_USE_4K_RAM)
			r = 3;
		else
			r = 1;
		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(vd_afbc_reg->afbc_top_ctrl,
			 r, 13, 2);
		burst_len = 2;
		r = (3 << 24) |
			(vpp_hold_line_s5 << 16) |
			(burst_len << 14) | /* burst1 */
			(vf->bitdepth & BITDEPTH_MASK);

		if (frame_par->hscale_skip_count)
			r |= 0x33;
		if (frame_par->vscale_skip_count)
			r |= 0xcc;

		/* FIXME: don't use glayer_info[0].reverse */
		if (glayer_info[0].reverse)
			r |= (1 << 26) | (1 << 27);
		else if (glayer_info[0].mirror == H_MIRROR)
			r |= (1 << 26) | (0 << 27);
		else if (glayer_info[0].mirror == V_MIRROR)
			r |= (0 << 26) | (1 << 27);

		if (vf->bitdepth & BITDEPTH_SAVING_MODE)
			r |= (1 << 28); /* mem_saving_mode */
		if (type & VIDTYPE_SCATTER)
			r |= (1 << 29);
		cur_dev->rdma_func[vpp_index].rdma_wr(vd_afbc_reg->afbc_mode, r);

		r = 0x1700;
		if (vf &&
		  (vf->source_type != VFRAME_SOURCE_TYPE_HDMI &&
		  (!IS_DI_POSTWRTIE(vf->type) && !(vf->flag & VFRAME_FLAG_COMPOSER_DONE))))
			r |= (1 << 19); /* dos_uncomp */
		if (type & VIDTYPE_COMB_MODE)
			r |= (1 << 20);

		cur_dev->rdma_func[vpp_index].rdma_wr(vd_afbc_reg->afbc_enable, r);

		r = conv_lbuf_len_s5[layer->layer_id];
		if ((type & VIDTYPE_VIU_444) ||
		    (type & VIDTYPE_RGB_444))
			r |= 0;
		else if (type & VIDTYPE_VIU_422)
			r |= (1 << 12);
		else
			r |= (2 << 12);
		cur_dev->rdma_func[vpp_index].rdma_wr(vd_afbc_reg->afbc_conv_ctrl, r);

		u = (vf->bitdepth >> (BITDEPTH_U_SHIFT)) & 0x3;
		v = (vf->bitdepth >> (BITDEPTH_V_SHIFT)) & 0x3;
		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_afbc_reg->afbc_dec_def_color,
			0x3FF00000 | /*Y,bit20+*/
			0x80 << (u + 10) |
			0x80 << v);
		/* chroma formatter */
		r = HFORMATTER_REPEAT |
			HFORMATTER_YC_RATIO_2_1 |
			HFORMATTER_EN |
			(0x8 << VFORMATTER_PHASE_BIT) |
			VFORMATTER_EN;
		if (is_dovi_tv_on())
			r |= VFORMATTER_ALWAYS_RPT |
				(0x0 << VFORMATTER_INIPHASE_BIT);
		else
			r |= VFORMATTER_RPTLINE0_EN |
				(0xc << VFORMATTER_INIPHASE_BIT);

		if ((type & VIDTYPE_VIU_444) ||
		    (type & VIDTYPE_RGB_444)) {
			r &= ~HFORMATTER_EN;
			r &= ~VFORMATTER_EN;
			r &= ~HFORMATTER_YC_RATIO_2_1;
		} else if (type & VIDTYPE_VIU_422) {
			r &= ~VFORMATTER_EN;
		}

		cur_dev->rdma_func[vpp_index].rdma_wr(vd_afbc_reg->afbc_vd_cfmt_ctrl, r);

		if (type & VIDTYPE_COMPRESS_LOSS)
			cur_dev->rdma_func[vpp_index].rdma_wr
				(vd_afbc_reg->afbcdec_iquant_enable,
				((1 << 11) |
				(1 << 10) |
				(1 << 4) |
				(1 << 0)));
		else
			cur_dev->rdma_func[vpp_index].rdma_wr
				(vd_afbc_reg->afbcdec_iquant_enable, 0);

		vd1_path_select_s5(layer, true, false, di_post, di_pre_link);
		if (is_mvc)
			vdx_path_select_s5(layer, true, false);
		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_mif_reg->vd_if0_gen_reg, 0);
		return;
	}

	/* DI only output NV21 8bit DW buffer */
	if (frame_par->nocomp &&
	    vf->plane_num == 2 &&
	    (vf->flag & VFRAME_FLAG_DI_DW)) {
		type &= ~(VIDTYPE_VIU_SINGLE_PLANE |
			VIDTYPE_VIU_NV12 |
			VIDTYPE_VIU_422 |
			VIDTYPE_VIU_444 |
			VIDTYPE_RGB_444);
		type |= VIDTYPE_VIU_NV21;
	}

	cur_dev->rdma_func[vpp_index].rdma_wr_bits
		(vd_afbc_reg->afbc_top_ctrl,
		0, 13, 2);

	/* vd mif burst len is 2 as default */
	burst_len = 2;
	if (vf->canvas0Addr != (u32)-1)
		canvas_w = canvas_get_width
			(vf->canvas0Addr & 0xff);
	else
		canvas_w = vf->canvas0_config[0].width;

	if (canvas_w % 32)
		burst_len = 0;
	else if (canvas_w % 64)
		burst_len = 1;
	if (layer->mif_setting.block_mode)
		burst_len = layer->mif_setting.block_mode;
	if ((vf->bitdepth & BITDEPTH_Y10) &&
	    !(vf->flag & VFRAME_FLAG_DI_DW) &&
	    !frame_par->nocomp) {
		if ((vf->type & VIDTYPE_VIU_444) ||
		    (vf->type & VIDTYPE_RGB_444)) {
			bit_mode = 2;
		} else {
			if (vf->bitdepth & FULL_PACK_422_MODE)
				bit_mode = 3;
			else
				bit_mode = 1;
		}
	} else {
		bit_mode = 0;
	}

	cur_dev->rdma_func[vpp_index].rdma_wr_bits
		(vd_mif_reg->vd_if0_gen_reg3,
		(bit_mode & 0x3), 8, 2);
	cur_dev->rdma_func[vpp_index].rdma_wr_bits
		(vd_mif_reg->vd_if0_gen_reg3,
		(burst_len & 0x3), 1, 2);
	if (vf->flag & VFRAME_FLAG_VIDEO_LINEAR)
		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(vd_mif_reg->vd_if0_gen_reg3,
			0, 0, 1);
	else
		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(vd_mif_reg->vd_if0_gen_reg3,
			1, 0, 1);
	vd_set_blk_mode_s5(layer, layer->mif_setting.block_mode);
	if (is_mvc) {
		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(vd2_mif_reg->vd_if0_gen_reg3,
			(bit_mode & 0x3), 8, 2);
		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(vd2_mif_reg->vd_if0_gen_reg3,
			(burst_len & 0x3), 1, 2);
		if (vf->flag & VFRAME_FLAG_VIDEO_LINEAR)
			cur_dev->rdma_func[vpp_index].rdma_wr_bits
				(vd2_mif_reg->vd_if0_gen_reg3,
				0, 0, 1);
		else
			cur_dev->rdma_func[vpp_index].rdma_wr_bits
				(vd2_mif_reg->vd_if0_gen_reg3,
				1, 0, 1);
		vd_set_blk_mode_s5(&vd_layer[1], layer->mif_setting.block_mode);
	}

#ifdef CONFIG_AMLOGIC_MEDIA_DEINTERLACE
	if (is_di_post_mode(vf)) {
		DI_POST_WR_REG_BITS
		(DI_IF1_GEN_REG3,
		(bit_mode & 0x3), 8, 2);
		DI_POST_WR_REG_BITS
			(DI_IF2_GEN_REG3,
			(bit_mode & 0x3), 8, 2);

		DI_POST_WR_REG_BITS
			(DI_IF0_GEN_REG3,
			(bit_mode & 0x3), 8, 2);
	}
#endif
	if (glayer_info[0].need_no_compress ||
	    (vf->type & VIDTYPE_PRE_DI_AFBC)) {
		vd1_path_select_s5(layer, false, true, di_post, di_pre_link);
	} else {
		vd1_path_select_s5(layer, false, false, di_post, di_pre_link);
		if (is_mvc)
			vdx_path_select_s5(layer, false, false);
		if (!layer->vd1_vd2_mux)
			cur_dev->rdma_func[vpp_index].rdma_wr
				(vd_afbc_reg->afbc_enable, 0);
	}

	r = (3 << VDIF_URGENT_BIT) |
		(vpp_hold_line_s5 << VDIF_HOLD_LINES_BIT) |
		VDIF_FORMAT_SPLIT |
		VDIF_CHRO_RPT_LAST | VDIF_ENABLE;
	/*  | VDIF_RESET_ON_GO_FIELD;*/
	if (layer->global_debug & DEBUG_FLAG_GOFIELD_MANUL)
		r |= 1 << 7; /*for manul triggle gofiled.*/

	if ((type & VIDTYPE_VIU_SINGLE_PLANE) == 0) {
		r |= VDIF_SEPARATE_EN;
	} else {
		if (type & VIDTYPE_VIU_422)
			r |= VDIF_FORMAT_422;
		else
			r |= VDIF_FORMAT_RGB888_YUV444 |
			    VDIF_DEMUX_MODE_RGB_444;
	}

	if (frame_par->hscale_skip_count) {
		if ((type & VIDTYPE_VIU_444) || (type & VIDTYPE_RGB_444))
			r |= VDIF_LUMA_HZ_AVG;
		else
			r |= VDIF_CHROMA_HZ_AVG | VDIF_LUMA_HZ_AVG;
	}
	if (vf->flag & VFRAME_FLAG_VIDEO_LINEAR)
		r |= (1 << 4);

	/*enable go field reset default according to vlsi*/
	r |= VDIF_RESET_ON_GO_FIELD;
	cur_dev->rdma_func[vpp_index].rdma_wr(vd_mif_reg->vd_if0_gen_reg, r);
	if (is_mvc)
		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd2_mif_reg->vd_if0_gen_reg, r);

	if (type & VIDTYPE_VIU_NV21)
		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(vd_mif_reg->vd_if0_gen_reg2, 1, 0, 2);
	else if (type & VIDTYPE_VIU_NV12)
		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(vd_mif_reg->vd_if0_gen_reg2, 2, 0, 2);
	else
		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(vd_mif_reg->vd_if0_gen_reg2, 0, 0, 2);

	/* FIXME: don't use glayer_info[0].reverse */
	if (glayer_info[0].reverse) {
		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(vd_mif_reg->vd_if0_gen_reg2, 0xf, 2, 4);
		if (is_mvc)
			cur_dev->rdma_func[vpp_index].rdma_wr_bits
				(vd2_mif_reg->vd_if0_gen_reg2, 0xf, 2, 4);
	} else if (glayer_info[0].mirror == H_MIRROR) {
		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(vd_mif_reg->vd_if0_gen_reg2, 0x5, 2, 4);
		if (is_mvc)
			cur_dev->rdma_func[vpp_index].rdma_wr_bits
				(vd2_mif_reg->vd_if0_gen_reg2, 0x5, 2, 4);
	} else if (glayer_info[0].mirror == V_MIRROR) {
		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(vd_mif_reg->vd_if0_gen_reg2, 0xa, 2, 4);
		if (is_mvc)
			cur_dev->rdma_func[vpp_index].rdma_wr_bits
				(vd2_mif_reg->vd_if0_gen_reg2, 0xa, 2, 4);
	} else {
		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(vd_mif_reg->vd_if0_gen_reg2, 0, 2, 4);
		if (is_mvc)
			cur_dev->rdma_func[vpp_index].rdma_wr_bits
				(vd2_mif_reg->vd_if0_gen_reg2, 0, 2, 4);
	}

	/* chroma formatter */
	if ((type & VIDTYPE_VIU_444) ||
	    (type & VIDTYPE_RGB_444)) {
		r = HFORMATTER_YC_RATIO_1_1;
		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_mif_reg->viu_vd_fmt_ctrl, r);
		if (is_mvc)
			cur_dev->rdma_func[vpp_index].rdma_wr
				(vd2_mif_reg->viu_vd_fmt_ctrl, r);
	} else {
		/* TODO: if always use HFORMATTER_REPEAT */
		if (is_crop_left_odd(frame_par)) {
			if ((type & VIDTYPE_VIU_NV21) ||
			     (type & VIDTYPE_VIU_NV12) ||
			    (type & VIDTYPE_VIU_422))
				hphase = 0x8 << HFORMATTER_PHASE_BIT;
		}
		if (frame_par->hscale_skip_count &&
		    (type & VIDTYPE_VIU_422))
			hformatter =
			(HFORMATTER_YC_RATIO_2_1 |
			hphase);
		else
			hformatter =
				(HFORMATTER_YC_RATIO_2_1 |
				hphase |
				HFORMATTER_EN);
		if (is_amdv_on())
			hformatter |= HFORMATTER_REPEAT;
		vrepeat = VFORMATTER_RPTLINE0_EN;
		vini_phase = (0xc << VFORMATTER_INIPHASE_BIT);
		if (type & VIDTYPE_VIU_422) {
			vformatter = 0;
			vphase = (0x10 << VFORMATTER_PHASE_BIT);
		} else {
			/*vlsi suggest only for yuv420 vformatter should be 1*/
			vformatter = VFORMATTER_EN;
			vphase = (0x08 << VFORMATTER_PHASE_BIT);
		}
		if (is_dovi_tv_on()) {
			/* dolby vision tv mode */
			vini_phase = (0 << VFORMATTER_INIPHASE_BIT);
			/* TODO: check the vrepeat */
			if (type & VIDTYPE_VIU_422)
				vrepeat = VFORMATTER_RPTLINE0_EN;
			else
				vrepeat = VFORMATTER_ALWAYS_RPT;
		} else if (is_mvc) {
			/* mvc source */
			vini_phase = (0xe << VFORMATTER_INIPHASE_BIT);
		} else if (type & VIDTYPE_TYPEMASK) {
			/* interlace source */
			if ((type & VIDTYPE_TYPEMASK) ==
			     VIDTYPE_INTERLACE_TOP)
				vini_phase =
					(0xe << VFORMATTER_INIPHASE_BIT);
			else
				vini_phase =
					(0xa << VFORMATTER_INIPHASE_BIT);
		}
		vformatter |= (vphase | vini_phase | vrepeat);

		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_mif_reg->viu_vd_fmt_ctrl,
			hformatter | vformatter);

		if (is_mvc) {
			vini_phase = (0xa << VFORMATTER_INIPHASE_BIT);
			vformatter = (vphase | vini_phase
				| vrepeat | VFORMATTER_EN);
			cur_dev->rdma_func[vpp_index].rdma_wr
				(vd2_mif_reg->viu_vd_fmt_ctrl,
				hformatter | vformatter);
		}
	}

	cur_dev->rdma_func[vpp_index].rdma_wr_bits
		(vd_mif_reg->viu_vd_fmt_ctrl,
		1, 29, 1);

	/* LOOP/SKIP pattern */
	pat = vpat[frame_par->vscale_skip_count];

	if (type & VIDTYPE_VIU_FIELD) {
		loop = 0;

		if (type & VIDTYPE_INTERLACE)
			pat = vpat[frame_par->vscale_skip_count >> 1];
	} else if (is_mvc) {
		loop = 0x11;
		if (framepacking_support)
			pat = 0;
		else
			pat = 0x80;
	} else if ((type & VIDTYPE_TYPEMASK) == VIDTYPE_INTERLACE_TOP) {
		loop = 0x11;
		pat <<= 4;
	} else {
		loop = 0;
	}

	cur_dev->rdma_func[vpp_index].rdma_wr
		(vd_mif_reg->vd_if0_rpt_loop,
		(loop << VDIF_CHROMA_LOOP1_BIT) |
		(loop << VDIF_LUMA_LOOP1_BIT) |
		(loop << VDIF_CHROMA_LOOP0_BIT) |
		(loop << VDIF_LUMA_LOOP0_BIT));
	if (is_mvc)
		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd2_mif_reg->vd_if0_rpt_loop,
			(loop << VDIF_CHROMA_LOOP1_BIT) |
			(loop << VDIF_LUMA_LOOP1_BIT) |
			(loop << VDIF_CHROMA_LOOP0_BIT) |
			(loop << VDIF_LUMA_LOOP0_BIT));

	cur_dev->rdma_func[vpp_index].rdma_wr(vd_mif_reg->vd_if0_luma0_rpt_pat, pat);
	cur_dev->rdma_func[vpp_index].rdma_wr(vd_mif_reg->vd_if0_chroma0_rpt_pat, pat);
	cur_dev->rdma_func[vpp_index].rdma_wr(vd_mif_reg->vd_if0_luma1_rpt_pat, pat);
	cur_dev->rdma_func[vpp_index].rdma_wr(vd_mif_reg->vd_if0_chroma1_rpt_pat, pat);

	if (is_mvc) {
		if (framepacking_support)
			pat = 0;
		else
			pat = 0x88;
		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd2_mif_reg->vd_if0_luma0_rpt_pat, pat);
		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd2_mif_reg->vd_if0_chroma0_rpt_pat, pat);
		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd2_mif_reg->vd_if0_luma1_rpt_pat, pat);
		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd2_mif_reg->vd_if0_chroma1_rpt_pat, pat);
	}

	/* picture 0/1 control */
	if ((((type & VIDTYPE_INTERLACE) == 0) &&
	     ((type & VIDTYPE_VIU_FIELD) == 0) &&
	     ((type & VIDTYPE_MVC) == 0)) ||
	    (frame_par->vpp_2pic_mode & 0x3)) {
		/* progressive frame in two pictures */
		r = ((2 << 26) | /* two pic mode */
			(2 << 24) | /* use own last line */
			0x01); /* loop pattern */
		if (frame_par->vpp_2pic_mode & VPP_PIC1_FIRST)
			r |= (1 << 8); /* use pic1 first*/
		else
			r |= (2 << 8);	 /* use pic0 first */
		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_mif_reg->vd_if0_luma_psel, r);
		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_mif_reg->vd_if0_chroma_psel, r);
	} else if (process_3d_type & MODE_3D_OUT_FA_MASK) {
		/*FA LR/TB output , do nothing*/
	} else {
		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_mif_reg->vd_if0_luma_psel, 0);
		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_mif_reg->vd_if0_chroma_psel, 0);
		if (is_mvc) {
			cur_dev->rdma_func[vpp_index].rdma_wr
				(vd2_mif_reg->vd_if0_luma_psel, 0);
			cur_dev->rdma_func[vpp_index].rdma_wr
				(vd2_mif_reg->vd_if0_chroma_psel, 0);
		}
	}
}

static void vd1_set_slice_dcu_s5(struct video_layer_s *layer,
		struct vpp_frame_par_s *frame_par,
		struct vframe_s *vf,
		u32 slice)
{
	u32 r;
	u32 vphase, vini_phase, vformatter, vrepeat, hphase = 0;
	u32 hformatter;
	u32 pat, loop;
	static const u32 vpat[MAX_VSKIP_COUNT + 1] = {
		0, 0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf};
	u32 u, v;
	u32 type, bit_mode = 0, canvas_w;
	u8 burst_len = 1;
	struct vd_mif_reg_s *vd_mif_reg = NULL;
	struct vd_afbc_reg_s *vd_afbc_reg = NULL;
	bool di_post = false, di_pre_link = false;
	u8 vpp_index = layer->vpp_index;
	u8 layer_id = layer->layer_id;

	if (!vf) {
		pr_info("%s vf NULL, return\n", __func__);
		return;
	}
	if (layer_id != 0 || slice > SLICE_NUM)
		return;
	vd_mif_reg = &vd_proc_reg.vd_mif_reg[slice];
	vd_afbc_reg = &vd_proc_reg.vd_afbc_reg[slice];

	type = vf->type;
	if (type & VIDTYPE_MVC) {
		pr_info("multi slice not support mvc\n");
		return;
	}
	pr_debug("%s for vd%d %p, type:0x%x, flag:%x\n",
		 __func__, layer->layer_id, vf, type, vf->flag);

#ifdef CONFIG_AMLOGIC_MEDIA_DEINTERLACE
	if (is_di_post_mode(vf) && is_di_post_on())
		di_post = true;
#ifdef ENABLE_PRE_LINK
	if (is_pre_link_on(layer, vf))
		di_pre_link = true;
#endif
#endif

	if (frame_par->nocomp)
		type &= ~VIDTYPE_COMPRESS;

	if (type & VIDTYPE_COMPRESS) {
		if (conv_lbuf_len_s5[layer->layer_id] == VIDEO_USE_4K_RAM)
			r = 3;
		else
			r = 1;
		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(vd_afbc_reg->afbc_top_ctrl,
			 r, 13, 2);
		burst_len = 2;
		r = (3 << 24) |
			(vpp_hold_line_s5 << 16) |
			(burst_len << 14) | /* burst1 */
			(vf->bitdepth & BITDEPTH_MASK);

		if (frame_par->hscale_skip_count)
			r |= 0x33;
		if (frame_par->vscale_skip_count)
			r |= 0xcc;

		/* FIXME: don't use glayer_info[0].reverse */
		if (glayer_info[0].reverse)
			r |= (1 << 26) | (1 << 27);
		else if (glayer_info[0].mirror == H_MIRROR)
			r |= (1 << 26) | (0 << 27);
		else if (glayer_info[0].mirror == V_MIRROR)
			r |= (0 << 26) | (1 << 27);

		if (vf->bitdepth & BITDEPTH_SAVING_MODE)
			r |= (1 << 28); /* mem_saving_mode */
		if (type & VIDTYPE_SCATTER)
			r |= (1 << 29);
		cur_dev->rdma_func[vpp_index].rdma_wr(vd_afbc_reg->afbc_mode, r);

		r = 0x1700;
		if (vf &&
		   (vf->source_type != VFRAME_SOURCE_TYPE_HDMI &&
		   (!IS_DI_POSTWRTIE(vf->type) && !(vf->flag & VFRAME_FLAG_COMPOSER_DONE))))
			r |= (1 << 19); /* dos_uncomp */
		if (type & VIDTYPE_COMB_MODE)
			r |= (1 << 20);

		cur_dev->rdma_func[vpp_index].rdma_wr(vd_afbc_reg->afbc_enable, r);
		r = conv_lbuf_len_s5[layer->layer_id];
		if ((type & VIDTYPE_VIU_444) ||
		    (type & VIDTYPE_RGB_444))
			r |= 0;
		else if (type & VIDTYPE_VIU_422)
			r |= (1 << 12);
		else
			r |= (2 << 12);
		cur_dev->rdma_func[vpp_index].rdma_wr(vd_afbc_reg->afbc_conv_ctrl, r);

		u = (vf->bitdepth >> (BITDEPTH_U_SHIFT)) & 0x3;
		v = (vf->bitdepth >> (BITDEPTH_V_SHIFT)) & 0x3;
		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_afbc_reg->afbc_dec_def_color,
			0x3FF00000 | /*Y,bit20+*/
			0x80 << (u + 10) |
			0x80 << v);
		/* chroma formatter */
		r = HFORMATTER_REPEAT |
			HFORMATTER_YC_RATIO_2_1 |
			HFORMATTER_EN |
			(0x8 << VFORMATTER_PHASE_BIT) |
			VFORMATTER_EN;
		if (is_dovi_tv_on())
			r |= VFORMATTER_ALWAYS_RPT |
				(0x0 << VFORMATTER_INIPHASE_BIT);
		else
			r |= VFORMATTER_RPTLINE0_EN |
				(0xc << VFORMATTER_INIPHASE_BIT);

		if ((type & VIDTYPE_VIU_444) ||
		    (type & VIDTYPE_RGB_444)) {
			r &= ~HFORMATTER_EN;
			r &= ~VFORMATTER_EN;
			r &= ~HFORMATTER_YC_RATIO_2_1;
		} else if (type & VIDTYPE_VIU_422) {
			r &= ~VFORMATTER_EN;
		}

		cur_dev->rdma_func[vpp_index].rdma_wr(vd_afbc_reg->afbc_vd_cfmt_ctrl, r);

		if (type & VIDTYPE_COMPRESS_LOSS)
			cur_dev->rdma_func[vpp_index].rdma_wr
				(vd_afbc_reg->afbcdec_iquant_enable,
				((1 << 11) |
				(1 << 10) |
				(1 << 4) |
				(1 << 0)));
		else
			cur_dev->rdma_func[vpp_index].rdma_wr
				(vd_afbc_reg->afbcdec_iquant_enable, 0);

		vd1_path_select_s5(layer, true, false, di_post, di_pre_link);
		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_mif_reg->vd_if0_gen_reg, 0);
		return;
	}

	/* DI only output NV21 8bit DW buffer */
	if (frame_par->nocomp &&
	    vf->plane_num == 2 &&
	    (vf->flag & VFRAME_FLAG_DI_DW)) {
		type &= ~(VIDTYPE_VIU_SINGLE_PLANE |
			VIDTYPE_VIU_NV12 |
			VIDTYPE_VIU_422 |
			VIDTYPE_VIU_444 |
			VIDTYPE_RGB_444);
		type |= VIDTYPE_VIU_NV21;
	}

	cur_dev->rdma_func[vpp_index].rdma_wr_bits
		(vd_afbc_reg->afbc_top_ctrl,
		0, 13, 2);

	/* vd mif burst len is 2 as default */
	burst_len = 2;
	if (vf->canvas0Addr != (u32)-1)
		canvas_w = canvas_get_width
			(vf->canvas0Addr & 0xff);
	else
		canvas_w = vf->canvas0_config[0].width;

	if (canvas_w % 32)
		burst_len = 0;
	else if (canvas_w % 64)
		burst_len = 1;
	if (layer->mif_setting.block_mode)
		burst_len = layer->mif_setting.block_mode;
	if ((vf->bitdepth & BITDEPTH_Y10) &&
	    !(vf->flag & VFRAME_FLAG_DI_DW) &&
	    !frame_par->nocomp) {
		if ((vf->type & VIDTYPE_VIU_444) ||
		    (vf->type & VIDTYPE_RGB_444)) {
			bit_mode = 2;
		} else {
			if (vf->bitdepth & FULL_PACK_422_MODE)
				bit_mode = 3;
			else
				bit_mode = 1;
		}
	} else {
		bit_mode = 0;
	}

	cur_dev->rdma_func[vpp_index].rdma_wr_bits
		(vd_mif_reg->vd_if0_gen_reg3,
		(bit_mode & 0x3), 8, 2);
	cur_dev->rdma_func[vpp_index].rdma_wr_bits
		(vd_mif_reg->vd_if0_gen_reg3,
		(burst_len & 0x3), 1, 2);
	if (vf->flag & VFRAME_FLAG_VIDEO_LINEAR)
		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(vd_mif_reg->vd_if0_gen_reg3,
			0, 0, 1);
	else
		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(vd_mif_reg->vd_if0_gen_reg3,
			1, 0, 1);
	vd_set_blk_mode_slice_s5(layer, slice, layer->mif_setting.block_mode);

#ifdef CONFIG_AMLOGIC_MEDIA_DEINTERLACE
	if (is_di_post_mode(vf)) {
		DI_POST_WR_REG_BITS
		(DI_IF1_GEN_REG3,
		(bit_mode & 0x3), 8, 2);
		DI_POST_WR_REG_BITS
			(DI_IF2_GEN_REG3,
			(bit_mode & 0x3), 8, 2);

		DI_POST_WR_REG_BITS
			(DI_IF0_GEN_REG3,
			(bit_mode & 0x3), 8, 2);
	}
#endif
	if (glayer_info[0].need_no_compress ||
	    (vf->type & VIDTYPE_PRE_DI_AFBC)) {
		vd1_path_select_s5(layer, false, true, di_post, di_pre_link);
	} else {
		vd1_path_select_s5(layer, false, false, di_post, di_pre_link);
		if (!layer->vd1_vd2_mux)
			cur_dev->rdma_func[vpp_index].rdma_wr
				(vd_afbc_reg->afbc_enable, 0);
	}

	r = (3 << VDIF_URGENT_BIT) |
		(vpp_hold_line_s5 << VDIF_HOLD_LINES_BIT) |
		VDIF_FORMAT_SPLIT |
		VDIF_CHRO_RPT_LAST | VDIF_ENABLE;
	/*  | VDIF_RESET_ON_GO_FIELD;*/
	if (layer->global_debug & DEBUG_FLAG_GOFIELD_MANUL)
		r |= 1 << 7; /*for manul triggle gofiled.*/

	if ((type & VIDTYPE_VIU_SINGLE_PLANE) == 0) {
		r |= VDIF_SEPARATE_EN;
	} else {
		if (type & VIDTYPE_VIU_422)
			r |= VDIF_FORMAT_422;
		else
			r |= VDIF_FORMAT_RGB888_YUV444 |
			    VDIF_DEMUX_MODE_RGB_444;
	}

	if (frame_par->hscale_skip_count) {
		if ((type & VIDTYPE_VIU_444) || (type & VIDTYPE_RGB_444))
			r |= VDIF_LUMA_HZ_AVG;
		else
			r |= VDIF_CHROMA_HZ_AVG | VDIF_LUMA_HZ_AVG;
	}
	if (vf->flag & VFRAME_FLAG_VIDEO_LINEAR)
		r |= (1 << 4);

	/*enable go field reset default according to vlsi*/
	r |= VDIF_RESET_ON_GO_FIELD;
	cur_dev->rdma_func[vpp_index].rdma_wr(vd_mif_reg->vd_if0_gen_reg, r);

	if (type & VIDTYPE_VIU_NV21)
		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(vd_mif_reg->vd_if0_gen_reg2, 1, 0, 2);
	else if (type & VIDTYPE_VIU_NV12)
		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(vd_mif_reg->vd_if0_gen_reg2, 2, 0, 2);
	else
		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(vd_mif_reg->vd_if0_gen_reg2, 0, 0, 2);

	/* FIXME: don't use glayer_info[0].reverse */
	if (glayer_info[0].reverse) {
		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(vd_mif_reg->vd_if0_gen_reg2, 0xf, 2, 4);
	} else if (glayer_info[0].mirror == H_MIRROR) {
		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(vd_mif_reg->vd_if0_gen_reg2, 0x5, 2, 4);
	} else if (glayer_info[0].mirror == V_MIRROR) {
		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(vd_mif_reg->vd_if0_gen_reg2, 0xa, 2, 4);
	} else {
		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(vd_mif_reg->vd_if0_gen_reg2, 0, 2, 4);
	}

	/* chroma formatter */
	if ((type & VIDTYPE_VIU_444) ||
	    (type & VIDTYPE_RGB_444)) {
		r = HFORMATTER_YC_RATIO_1_1;
		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_mif_reg->viu_vd_fmt_ctrl, r);
	} else {
		/* TODO: if always use HFORMATTER_REPEAT */
		if (is_crop_left_odd(frame_par)) {
			if ((type & VIDTYPE_VIU_NV21) ||
			     (type & VIDTYPE_VIU_NV12) ||
			    (type & VIDTYPE_VIU_422))
				hphase = 0x8 << HFORMATTER_PHASE_BIT;
		}
		if (frame_par->hscale_skip_count &&
		    (type & VIDTYPE_VIU_422))
			hformatter =
			(HFORMATTER_YC_RATIO_2_1 |
			hphase);
		else
			hformatter =
				(HFORMATTER_YC_RATIO_2_1 |
				hphase |
				HFORMATTER_EN);
		if (is_amdv_on())
			hformatter |= HFORMATTER_REPEAT;
		vrepeat = VFORMATTER_RPTLINE0_EN;
		vini_phase = (0xc << VFORMATTER_INIPHASE_BIT);
		if (type & VIDTYPE_VIU_422) {
			vformatter = 0;
			vphase = (0x10 << VFORMATTER_PHASE_BIT);
		} else {
			/*vlsi suggest only for yuv420 vformatter should be 1*/
			vformatter = VFORMATTER_EN;
			vphase = (0x08 << VFORMATTER_PHASE_BIT);
		}
		if (is_dovi_tv_on()) {
			/* dolby vision tv mode */
			vini_phase = (0 << VFORMATTER_INIPHASE_BIT);
			/* TODO: check the vrepeat */
			if (type & VIDTYPE_VIU_422)
				vrepeat = VFORMATTER_RPTLINE0_EN;
			else
				vrepeat = VFORMATTER_ALWAYS_RPT;
		} else if (type & VIDTYPE_TYPEMASK) {
			/* interlace source */
			if ((type & VIDTYPE_TYPEMASK) ==
			     VIDTYPE_INTERLACE_TOP)
				vini_phase =
					(0xe << VFORMATTER_INIPHASE_BIT);
			else
				vini_phase =
					(0xa << VFORMATTER_INIPHASE_BIT);
		}
		vformatter |= (vphase | vini_phase | vrepeat);

		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_mif_reg->viu_vd_fmt_ctrl,
			hformatter | vformatter);
	}

	cur_dev->rdma_func[vpp_index].rdma_wr_bits
		(vd_mif_reg->viu_vd_fmt_ctrl,
		1, 29, 1);

	/* LOOP/SKIP pattern */
	pat = vpat[frame_par->vscale_skip_count];

	if (type & VIDTYPE_VIU_FIELD) {
		loop = 0;

		if (type & VIDTYPE_INTERLACE)
			pat = vpat[frame_par->vscale_skip_count >> 1];
	} else if ((type & VIDTYPE_TYPEMASK) == VIDTYPE_INTERLACE_TOP) {
		loop = 0x11;
		pat <<= 4;
	} else {
		loop = 0;
	}

	cur_dev->rdma_func[vpp_index].rdma_wr
		(vd_mif_reg->vd_if0_rpt_loop,
		(loop << VDIF_CHROMA_LOOP1_BIT) |
		(loop << VDIF_LUMA_LOOP1_BIT) |
		(loop << VDIF_CHROMA_LOOP0_BIT) |
		(loop << VDIF_LUMA_LOOP0_BIT));

	cur_dev->rdma_func[vpp_index].rdma_wr(vd_mif_reg->vd_if0_luma0_rpt_pat, pat);
	cur_dev->rdma_func[vpp_index].rdma_wr(vd_mif_reg->vd_if0_chroma0_rpt_pat, pat);
	cur_dev->rdma_func[vpp_index].rdma_wr(vd_mif_reg->vd_if0_luma1_rpt_pat, pat);
	cur_dev->rdma_func[vpp_index].rdma_wr(vd_mif_reg->vd_if0_chroma1_rpt_pat, pat);

	/* picture 0/1 control */
	if ((((type & VIDTYPE_INTERLACE) == 0) &&
	     ((type & VIDTYPE_VIU_FIELD) == 0) &&
	     ((type & VIDTYPE_MVC) == 0)) ||
	    (frame_par->vpp_2pic_mode & 0x3)) {
		/* progressive frame in two pictures */
		r = ((2 << 26) | /* two pic mode */
			(2 << 24) | /* use own last line */
			0x01); /* loop pattern */
		if (frame_par->vpp_2pic_mode & VPP_PIC1_FIRST)
			r |= (1 << 8); /* use pic1 first*/
		else
			r |= (2 << 8);	 /* use pic0 first */
		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_mif_reg->vd_if0_luma_psel, r);
		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_mif_reg->vd_if0_chroma_psel, r);
	} else if (process_3d_type & MODE_3D_OUT_FA_MASK) {
		/*FA LR/TB output , do nothing*/
	} else {
		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_mif_reg->vd_if0_luma_psel, 0);
		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_mif_reg->vd_if0_chroma_psel, 0);
	}
}

static void vdx_set_dcu_s5(struct video_layer_s *layer,
			struct vpp_frame_par_s *frame_par,
			struct vframe_s *vf)
{
	u32 r;
	u32 vphase, vini_phase, vformatter, vrepeat, hphase = 0;
	u32 hformatter;
	u32 pat, loop;
	static const u32 vpat[MAX_VSKIP_COUNT + 1] = {
		0, 0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf};
	u32 u, v;
	u32 type, bit_mode = 0, canvas_w;
	bool is_mvc = false;
	u8 burst_len = 1;
	struct vd_mif_reg_s *vd_mif_reg = NULL;
	struct vd_afbc_reg_s *vd_afbc_reg = NULL;
	int layer_id = layer->layer_id;
	int layer_index = 0;
	u8 vpp_index = layer->vpp_index;

	if (!vf) {
		pr_err("%s vf is NULL\n", __func__);
		return;
	}
	if (!glayer_info[layer_id].layer_support)
		return;
	layer_index = layer_id + SLICE_NUM - 1;
	vd_mif_reg = &vd_proc_reg.vd_mif_reg[layer_index];
	vd_afbc_reg = &vd_proc_reg.vd_afbc_reg[layer_index];

	type = vf->type;
	if (type & VIDTYPE_MVC)
		is_mvc = true;
	pr_debug("%s for vd%d %p, type:0x%x, flag:%x\n",
		 __func__, layer->layer_id, vf, type, vf->flag);

	if (frame_par->nocomp)
		type &= ~VIDTYPE_COMPRESS;

	if (type & VIDTYPE_COMPRESS) {
		if (conv_lbuf_len_s5[layer_id] == VIDEO_USE_4K_RAM)
			r = 3;
		else
			r = 1;

		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(vd_afbc_reg->afbc_top_ctrl,
			 r, 13, 2);

		burst_len = 2;
		r = (3 << 24) |
		    (vpp_hold_line_s5 << 16) |
		    (burst_len << 14) | /* burst1 */
		    (vf->bitdepth & BITDEPTH_MASK);

		if (frame_par->hscale_skip_count)
			r |= 0x33;
		if (frame_par->vscale_skip_count)
			r |= 0xcc;

		/* FIXME: don't use glayer_info[1].reverse */
		if (glayer_info[layer_id].reverse)
			r |= (1 << 26) | (1 << 27);
		else if (glayer_info[layer_id].mirror == H_MIRROR)
			r |= (1 << 26) | (0 << 27);
		else if (glayer_info[layer_id].mirror == V_MIRROR)
			r |= (0 << 26) | (1 << 27);

		if (vf->bitdepth & BITDEPTH_SAVING_MODE)
			r |= (1 << 28); /* mem_saving_mode */
		if (type & VIDTYPE_SCATTER)
			r |= (1 << 29);
		cur_dev->rdma_func[vpp_index].rdma_wr(vd_afbc_reg->afbc_mode, r);

		r = 0x1700;
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
			if (vf &&
			  (vf->source_type != VFRAME_SOURCE_TYPE_HDMI &&
			  (!IS_DI_POSTWRTIE(vf->type) && !(vf->flag & VFRAME_FLAG_COMPOSER_DONE))))
				r |= (1 << 19); /* dos_uncomp */
			if (type & VIDTYPE_COMB_MODE)
				r |= (1 << 20);
		}
		cur_dev->rdma_func[vpp_index].rdma_wr(vd_afbc_reg->afbc_enable, r);

		r = conv_lbuf_len_s5[layer_id];
		if ((type & VIDTYPE_VIU_444) ||
		    (type & VIDTYPE_RGB_444))
			r |= 0;
		else if (type & VIDTYPE_VIU_422)
			r |= (1 << 12);
		else
			r |= (2 << 12);
		cur_dev->rdma_func[vpp_index].rdma_wr(vd_afbc_reg->afbc_conv_ctrl, r);

		u = (vf->bitdepth >> (BITDEPTH_U_SHIFT)) & 0x3;
		v = (vf->bitdepth >> (BITDEPTH_V_SHIFT)) & 0x3;
		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_afbc_reg->afbc_dec_def_color,
			0x3FF00000 | /*Y,bit20+*/
			0x80 << (u + 10) |
			0x80 << v);

		/* chroma formatter */
		r = HFORMATTER_YC_RATIO_2_1 |
			HFORMATTER_EN |
			(0x8 << VFORMATTER_PHASE_BIT) |
			VFORMATTER_EN;
		if (is_dovi_tv_on())
			r |= HFORMATTER_REPEAT |
				VFORMATTER_ALWAYS_RPT |
				(0x0 << VFORMATTER_INIPHASE_BIT);
		else if (is_amdv_on()) /* stb case */
			r |= HFORMATTER_REPEAT |
				VFORMATTER_RPTLINE0_EN |
				(0xc << VFORMATTER_INIPHASE_BIT);
		else
			r |= HFORMATTER_RRT_PIXEL0 |
				VFORMATTER_RPTLINE0_EN |
				(0 << VFORMATTER_INIPHASE_BIT);

		if ((type & VIDTYPE_VIU_444) ||
		    (type & VIDTYPE_RGB_444)) {
			r &= ~HFORMATTER_EN;
			r &= ~VFORMATTER_EN;
			r &= ~HFORMATTER_YC_RATIO_2_1;
		} else if (type & VIDTYPE_VIU_422) {
			r &= ~VFORMATTER_EN;
		}
		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_afbc_reg->afbc_vd_cfmt_ctrl, r);

		if (type & VIDTYPE_COMPRESS_LOSS)
			cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_afbc_reg->afbcdec_iquant_enable,
			((1 << 11) |
			(1 << 10) |
			(1 << 4) |
			(1 << 0)));
		else
			cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_afbc_reg->afbcdec_iquant_enable, 0);
		vdx_path_select_s5(layer, true, false);
		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_mif_reg->vd_if0_gen_reg, 0);
		return;
	}

	/* DI only output NV21 8bit DW buffer */
	if (frame_par->nocomp &&
	    vf->plane_num == 2 &&
	    (vf->flag & VFRAME_FLAG_DI_DW)) {
		type &= ~(VIDTYPE_VIU_SINGLE_PLANE |
			VIDTYPE_VIU_NV12 |
			VIDTYPE_VIU_422 |
			VIDTYPE_VIU_444 |
			VIDTYPE_RGB_444);
		type |= VIDTYPE_VIU_NV21;
	}

	cur_dev->rdma_func[vpp_index].rdma_wr_bits
		(vd_afbc_reg->afbc_top_ctrl,
		0, 13, 2);

	/* vd mif burst len is 2 as default */
	burst_len = 2;
	if (vf->canvas0Addr != (u32)-1)
		canvas_w = canvas_get_width
			(vf->canvas0Addr & 0xff);
	else
		canvas_w = vf->canvas0_config[0].width;

	if (canvas_w % 32)
		burst_len = 0;
	else if (canvas_w % 64)
		burst_len = 1;
	if (layer->mif_setting.block_mode)
		burst_len = layer->mif_setting.block_mode;

	if ((vf->bitdepth & BITDEPTH_Y10) &&
	    !(vf->flag & VFRAME_FLAG_DI_DW) &&
	    !frame_par->nocomp) {
		if ((vf->type & VIDTYPE_VIU_444) ||
		    (vf->type & VIDTYPE_RGB_444)) {
			bit_mode = 2;
		} else {
			if (vf->bitdepth & FULL_PACK_422_MODE)
				bit_mode = 3;
			else
				bit_mode = 1;
		}
	} else {
		bit_mode = 0;
	}

	vdx_path_select_s5(layer, false, false);
	cur_dev->rdma_func[vpp_index].rdma_wr_bits
		(vd_mif_reg->vd_if0_gen_reg3,
		(bit_mode & 0x3), 8, 2);
	cur_dev->rdma_func[vpp_index].rdma_wr_bits
		(vd_mif_reg->vd_if0_gen_reg3,
		(burst_len & 0x3), 1, 2);
	if (vf->flag & VFRAME_FLAG_VIDEO_LINEAR)
		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(vd_mif_reg->vd_if0_gen_reg3,
			0, 0, 1);
	else
		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(vd_mif_reg->vd_if0_gen_reg3,
			1, 0, 1);
	vd_set_blk_mode_s5(layer, layer->mif_setting.block_mode);

	cur_dev->rdma_func[vpp_index].rdma_wr
		(vd_afbc_reg->afbc_enable, 0);

	r = (3 << VDIF_URGENT_BIT) |
		(vpp_hold_line_s5 << VDIF_HOLD_LINES_BIT) |
		VDIF_FORMAT_SPLIT |
		VDIF_CHRO_RPT_LAST | VDIF_ENABLE;
	/*  | VDIF_RESET_ON_GO_FIELD;*/
	if (layer->global_debug & DEBUG_FLAG_GOFIELD_MANUL)
		r |= 1 << 7; /*for manul triggle gofiled.*/

	if ((type & VIDTYPE_VIU_SINGLE_PLANE) == 0) {
		r |= VDIF_SEPARATE_EN;
	} else {
		if (type & VIDTYPE_VIU_422)
			r |= VDIF_FORMAT_422;
		else
			r |= VDIF_FORMAT_RGB888_YUV444 |
			    VDIF_DEMUX_MODE_RGB_444;
	}

	if (frame_par->hscale_skip_count) {
		if ((type & VIDTYPE_VIU_444) || (type & VIDTYPE_RGB_444))
			r |= VDIF_LUMA_HZ_AVG;
		else
			r |= VDIF_CHROMA_HZ_AVG | VDIF_LUMA_HZ_AVG;
	}

	if (vf->flag & VFRAME_FLAG_VIDEO_LINEAR)
		r |= (1 << 4);

	cur_dev->rdma_func[vpp_index].rdma_wr(vd_mif_reg->vd_if0_gen_reg, r);

	if (type & VIDTYPE_VIU_NV21)
		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(vd_mif_reg->vd_if0_gen_reg2,
			1, 0, 2);
	else if (type & VIDTYPE_VIU_NV12)
		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(vd_mif_reg->vd_if0_gen_reg2,
			2, 0, 2);
	else
		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(vd_mif_reg->vd_if0_gen_reg2,
			0, 0, 2);

	/* FIXME: don't use glayer_info[1].reverse */
	if (glayer_info[layer_id].reverse)
		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(vd_mif_reg->vd_if0_gen_reg2,
			0xf, 2, 4);
	else if (glayer_info[layer_id].mirror == H_MIRROR)
		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(vd_mif_reg->vd_if0_gen_reg2,
			0x5, 2, 4);
	else if (glayer_info[layer_id].mirror == V_MIRROR)
		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(vd_mif_reg->vd_if0_gen_reg2,
			0xa, 2, 4);
	else
		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(vd_mif_reg->vd_if0_gen_reg2,
			0, 2, 4);

	/* chroma formatter */
	if ((type & VIDTYPE_VIU_444) ||
	    (type & VIDTYPE_RGB_444)) {
		r = HFORMATTER_YC_RATIO_1_1;
		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_mif_reg->viu_vd_fmt_ctrl, r);
	} else {
		/* TODO: if always use HFORMATTER_REPEAT */
		if (is_crop_left_odd(frame_par)) {
			if ((type & VIDTYPE_VIU_NV21) ||
			     (type & VIDTYPE_VIU_NV12) ||
			    (type & VIDTYPE_VIU_422))
				hphase = 0x8 << HFORMATTER_PHASE_BIT;
		}
		if (frame_par->hscale_skip_count &&
		    (type & VIDTYPE_VIU_422))
			hformatter =
			(HFORMATTER_YC_RATIO_2_1 |
			hphase);
		else
			hformatter =
				(HFORMATTER_YC_RATIO_2_1 |
				hphase |
				HFORMATTER_EN);
		if (is_amdv_on())
			hformatter |= HFORMATTER_REPEAT;
		vrepeat = VFORMATTER_RPTLINE0_EN;
		vini_phase = (0xc << VFORMATTER_INIPHASE_BIT);
		if (type & VIDTYPE_VIU_422) {
			vformatter = 0;
			vphase = (0x10 << VFORMATTER_PHASE_BIT);
		} else {
			/*vlsi suggest only for yuv420 vformatter should be 1*/
			vformatter = VFORMATTER_EN;
			vphase = (0x08 << VFORMATTER_PHASE_BIT);
		}
		if (is_dovi_tv_on()) {
			/* dolby vision tv mode */
			vini_phase = (0 << VFORMATTER_INIPHASE_BIT);
			/* TODO: check the vrepeat */
			if (type & VIDTYPE_VIU_422)
				vrepeat = VFORMATTER_RPTLINE0_EN;
			else
				vrepeat = VFORMATTER_ALWAYS_RPT;
		} else if (is_mvc) {
			/* mvc source */
			/* vini_phase = (0xe << VFORMATTER_INIPHASE_BIT); */
			vini_phase = (0xa << VFORMATTER_INIPHASE_BIT);
		} else if (type & VIDTYPE_TYPEMASK) {
			/* interlace source */
			if ((type & VIDTYPE_TYPEMASK) ==
			    VIDTYPE_INTERLACE_TOP)
				vini_phase =
					(0xe << VFORMATTER_INIPHASE_BIT);
			else
				vini_phase =
					(0xa << VFORMATTER_INIPHASE_BIT);
		}
		vformatter |= (vphase | vini_phase | vrepeat);

		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_mif_reg->viu_vd_fmt_ctrl,
			hformatter | vformatter);
	}
	cur_dev->rdma_func[vpp_index].rdma_wr_bits
		(vd_mif_reg->viu_vd_fmt_ctrl,
		1, 29, 1);
	/* LOOP/SKIP pattern */
	pat = vpat[frame_par->vscale_skip_count];

	if (type & VIDTYPE_VIU_FIELD) {
		loop = 0;

		if (type & VIDTYPE_INTERLACE)
			pat = vpat[frame_par->vscale_skip_count >> 1];
	} else if (is_mvc) {
		loop = 0x11;
		pat = 0x80;
	} else if ((type & VIDTYPE_TYPEMASK) == VIDTYPE_INTERLACE_TOP) {
		loop = 0x11;
		pat <<= 4;
	} else {
		loop = 0;
	}

	cur_dev->rdma_func[vpp_index].rdma_wr
		(vd_mif_reg->vd_if0_rpt_loop,
		(loop << VDIF_CHROMA_LOOP1_BIT) |
		(loop << VDIF_LUMA_LOOP1_BIT) |
		(loop << VDIF_CHROMA_LOOP0_BIT) |
		(loop << VDIF_LUMA_LOOP0_BIT));

	if (is_mvc)
		pat = 0x88;

	cur_dev->rdma_func[vpp_index].rdma_wr
		(vd_mif_reg->vd_if0_luma0_rpt_pat, pat);
	cur_dev->rdma_func[vpp_index].rdma_wr
		(vd_mif_reg->vd_if0_chroma0_rpt_pat, pat);
	cur_dev->rdma_func[vpp_index].rdma_wr
		(vd_mif_reg->vd_if0_luma1_rpt_pat, pat);
	cur_dev->rdma_func[vpp_index].rdma_wr
		(vd_mif_reg->vd_if0_chroma1_rpt_pat, pat);

	/* picture 0/1 control */
	if ((((type & VIDTYPE_INTERLACE) == 0) &&
	     ((type & VIDTYPE_VIU_FIELD) == 0) &&
	     ((type & VIDTYPE_MVC) == 0)) ||
	    (frame_par->vpp_2pic_mode & 0x3)) {
		/* progressive frame in two pictures */
	} else {
		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_mif_reg->vd_if0_luma_psel, 0);
		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_mif_reg->vd_if0_chroma_psel, 0);
	}
}

static void vd1_scaler_setting_s5(struct video_layer_s *layer,
	struct scaler_setting_s *setting,
	u32 slice)
{
	u32 i;
	u32 r1, r2, r3;
	struct vpp_frame_par_s *frame_par;
	struct vppfilter_mode_s *vpp_filter;
	struct vppfilter_mode_s *aisr_vpp_filter = NULL;
	u32 hsc_init_rev_num0 = 4;
	struct vd_pps_reg_s *vd_pps_reg;
	u32 bit9_mode = 0, s11_mode = 0;
	u8 vpp_index;
	const u32 *vpp_vert_coeff;
	const u32 *vpp_horz_coeff;
	struct vd_proc_s *vd_proc = NULL;
	struct vd_proc_pps_s *vd_proc_pps = NULL;
	u32 vd1_work_mode, vd1_slices_dout_dpsel;
	u32 mosaic_mode, use_frm_horz_phase_step;
	u32 frm_horz_phase_step, slice_x_st;
	long long slice_ini_sum = 0;
	u32 slice_ini_phase = 0, slice_ini_phase_exp = 0;
	u32 slice_ini_phase_ori = 0;

	if (!setting || !setting->frame_par)
		return;
	vd_proc = get_vd_proc_info();
	vd_proc_pps = &vd_proc->vd_proc_unit[slice].vd_proc_pps;
	vd1_work_mode = vd_proc->vd_proc_vd1_info.vd1_work_mode;
	vd1_slices_dout_dpsel = vd_proc->vd_proc_vd1_info.vd1_slices_dout_dpsel;
	mosaic_mode = vd1_work_mode == VD1_2_2SLICES_MODE &&
			vd1_slices_dout_dpsel == VD1_SLICES_DOUT_4S4P;
	/* use_frm_horz_phase_step = 1 : 4slices4ppc or 2slices4ppc mode */
	/* use_frm_horz_phase_step = 0: 1slices mode or PI path mode */
	use_frm_horz_phase_step = ((vd1_work_mode == VD1_4SLICES_MODE &&
		vd1_slices_dout_dpsel != VD1_SLICES_DOUT_4S4P &&
		!mosaic_mode) || vd1_work_mode == VD1_1SLICES_MODE) ? 0 : 1;
	frm_horz_phase_step = vd_proc_pps->horz_phase_step;
	slice_x_st = vd_proc_pps->slice_x_st;

	vd_pps_reg = &vd_proc_reg.vd_pps_reg[slice];
	if (use_frm_horz_phase_step) {
		slice_ini_sum = slice == 0 ? 0 :
			div_u64((((long long)(slice_x_st) + 1) << 24),
				frm_horz_phase_step) *
			frm_horz_phase_step;
		slice_ini_phase =  slice_ini_sum & 0xFFFFFF;
		slice_ini_phase_exp = slice_ini_phase & 0xFF;
		slice_ini_phase_ori = slice_ini_phase >> 8;
	}
	frame_par = setting->frame_par;
	vpp_index = layer->vpp_index;
	/* vpp super scaler */

	if (is_amdv_on() &&
	    is_amdv_stb_mode() &&
	    !frame_par->supsc0_enable &&
	    !frame_par->supsc1_enable) {
		//cur_dev->rdma_func[vpp_index].rdma_wr(VPP_SRSHARP0_CTRL, 0);
		//cur_dev->rdma_func[vpp_index].rdma_wr(VPP_SRSHARP1_CTRL, 0);
	}

	vpp_filter = &frame_par->vpp_filter;
	aisr_vpp_filter = &cur_dev->aisr_frame_parms.vpp_filter;
	if (setting->sc_top_enable) {
		u32 sc_misc_val;

		sc_misc_val = VPP_SC_TOP_EN | VPP_SC_V1OUT_EN;
		/* enable seprate luma chroma coef */
		if (cur_dev->scaler_sep_coef_en)
			sc_misc_val |= VPP_HF_SEP_COEF_4SRNET_EN;
		if (setting->sc_h_enable) {
			if (has_pre_hscaler_ntap(0)) {
				/* for sc2/t5 support hscaler 8 tap */
				if (pre_scaler[0].pre_hscaler_ntap_enable) {
					sc_misc_val |=
					(((vpp_filter->vpp_pre_hsc_en & 1)
					<< VPP_SC_PREHORZ_EN_BIT)
					| VPP_SC_HORZ_EN);
				} else {
					sc_misc_val |=
					(((vpp_filter->vpp_pre_hsc_en & 1)
					<< VPP_SC_PREHORZ_EN_BIT_OLD)
					| VPP_SC_HORZ_EN);
				}
			} else {
				sc_misc_val |=
					(((vpp_filter->vpp_pre_hsc_en & 1)
					<< VPP_SC_PREHORZ_EN_BIT)
					| VPP_SC_HORZ_EN);
			}
			if (hscaler_8tap_enable[0]) {
				sc_misc_val |=
				((vpp_filter->vpp_horz_coeff[0] & 0xf)
					<< VPP_SC_HBANK_LENGTH_BIT);

			} else {
				sc_misc_val |=
					((vpp_filter->vpp_horz_coeff[0] & 7)
					<< VPP_SC_HBANK_LENGTH_BIT);
			}
		}

		if (setting->sc_v_enable) {
			sc_misc_val |= (((vpp_filter->vpp_pre_vsc_en & 1)
				<< VPP_SC_PREVERT_EN_BIT)
				| VPP_SC_VERT_EN);
			sc_misc_val |= ((vpp_filter->vpp_pre_vsc_en & 1)
				<< VPP_LINE_BUFFER_EN_BIT);
			sc_misc_val |= ((vpp_filter->vpp_vert_coeff[0] & 7)
				<< VPP_SC_VBANK_LENGTH_BIT);
		}

		if (setting->last_line_fix)
			sc_misc_val |= VPP_PPS_LAST_LINE_FIX;

		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_pps_reg->vd_sc_misc,
			sc_misc_val);
		vpu_module_clk_enable_s5(vpp_index, VD1_SCALER, 0);
	} else {
		setting->sc_v_enable = false;
		setting->sc_h_enable = false;
		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(vd_pps_reg->vd_sc_misc,
			0, VPP_SC_TOP_EN_BIT,
			VPP_SC_TOP_EN_WID);
		vpu_module_clk_disable_s5(vpp_index, VD1_SCALER, 0);
	}

	/* horizontal filter settings */
	if (setting->sc_h_enable) {
		bit9_mode = vpp_filter->vpp_horz_coeff[1] & 0x8000;
		s11_mode = vpp_filter->vpp_horz_coeff[1] & 0x4000;
		if (s11_mode)
			cur_dev->rdma_func[vpp_index].rdma_wr_bits(vd_pps_reg->vd_pre_scale_ctrl,
					       0x199, 12, 9);
		else
			cur_dev->rdma_func[vpp_index].rdma_wr_bits(vd_pps_reg->vd_pre_scale_ctrl,
					       0x77, 12, 9);
		if (layer->aisr_mif_setting.aisr_enable &&
		   cur_dev->pps_auto_calc)
			vpp_horz_coeff = aisr_vpp_filter->vpp_horz_coeff;
		else
			vpp_horz_coeff = vpp_filter->vpp_horz_coeff;
		if (hscaler_8tap_enable[0]) {
			if (bit9_mode || s11_mode) {
				cur_dev->rdma_func[vpp_index].rdma_wr
					(vd_pps_reg->vd_scale_coef_idx,
					VPP_COEF_HORZ | VPP_COEF_9BIT);
				for (i = 0; i < VPP_FILER_COEFS_NUM; i++) {
					cur_dev->rdma_func[vpp_index].rdma_wr
					(vd_pps_reg->vd_scale_coef,
					vpp_horz_coeff[i + 2]);
				}
				for (i = 0; i < VPP_FILER_COEFS_NUM; i++) {
					cur_dev->rdma_func[vpp_index].rdma_wr
					(vd_pps_reg->vd_scale_coef,
					vpp_horz_coeff[i + 2 +
					VPP_FILER_COEFS_NUM]);
				}
				cur_dev->rdma_func[vpp_index].rdma_wr(vd_pps_reg->vd_scale_coef_idx,
						  VPP_COEF_HORZ |
						  VPP_COEF_VERT_CHROMA |
						  VPP_COEF_9BIT);
				for (i = 0; i < VPP_FILER_COEFS_NUM; i++) {
					cur_dev->rdma_func[vpp_index].rdma_wr
					(vd_pps_reg->vd_scale_coef,
					vpp_horz_coeff[i + 2 +
					VPP_FILER_COEFS_NUM * 2]);
				}
				for (i = 0; i < VPP_FILER_COEFS_NUM; i++) {
					cur_dev->rdma_func[vpp_index].rdma_wr
					(vd_pps_reg->vd_scale_coef,
					vpp_horz_coeff[i + 2 +
					VPP_FILER_COEFS_NUM * 3]);
				}
				if (cur_dev->scaler_sep_coef_en) {
					cur_dev->rdma_func[vpp_index].rdma_wr
						(vd_pps_reg->vd_scale_coef_idx,
						VPP_SEP_COEF_HORZ_CHROMA |
						VPP_SEP_COEF |
						VPP_COEF_9BIT);
					for (i = 0; i < VPP_FILER_COEFS_NUM; i++) {
						cur_dev->rdma_func[vpp_index].rdma_wr
						(vd_pps_reg->vd_scale_coef,
						vpp_filter->vpp_horz_coeff[i + 2]);
					}
					for (i = 0; i < VPP_FILER_COEFS_NUM; i++) {
						cur_dev->rdma_func[vpp_index].rdma_wr
						(vd_pps_reg->vd_scale_coef,
						vpp_filter->vpp_horz_coeff[i + 2 +
						VPP_FILER_COEFS_NUM]);
					}
					cur_dev->rdma_func[vpp_index].rdma_wr
						(vd_pps_reg->vd_scale_coef_idx,
						VPP_SEP_COEF_HORZ_CHROMA_PARTB |
						VPP_SEP_COEF |
						VPP_COEF_9BIT);
					for (i = 0; i < VPP_FILER_COEFS_NUM; i++) {
						cur_dev->rdma_func[vpp_index].rdma_wr
						(vd_pps_reg->vd_scale_coef,
						vpp_filter->vpp_horz_coeff[i + 2 +
						VPP_FILER_COEFS_NUM * 2]);
					}
					for (i = 0; i < VPP_FILER_COEFS_NUM; i++) {
						cur_dev->rdma_func[vpp_index].rdma_wr
						(vd_pps_reg->vd_scale_coef,
						vpp_filter->vpp_horz_coeff[i + 2 +
						VPP_FILER_COEFS_NUM * 3]);
					}
				}
			} else {
				cur_dev->rdma_func[vpp_index].rdma_wr(vd_pps_reg->vd_scale_coef_idx,
						  VPP_COEF_HORZ);
				for (i = 0; i < VPP_FILER_COEFS_NUM; i++) {
					cur_dev->rdma_func[vpp_index].rdma_wr
					(vd_pps_reg->vd_scale_coef,
					vpp_horz_coeff[i + 2]);
				}
				cur_dev->rdma_func[vpp_index].rdma_wr(vd_pps_reg->vd_scale_coef_idx,
						  VPP_COEF_HORZ |
						  VPP_COEF_VERT_CHROMA);
				for (i = 0; i < VPP_FILER_COEFS_NUM; i++) {
					cur_dev->rdma_func[vpp_index].rdma_wr
					(vd_pps_reg->vd_scale_coef,
					vpp_horz_coeff[i + 2 +
					VPP_FILER_COEFS_NUM]);
				}
				if (cur_dev->scaler_sep_coef_en) {
					cur_dev->rdma_func[vpp_index].rdma_wr
						(vd_pps_reg->vd_scale_coef_idx,
						VPP_SEP_COEF_HORZ_CHROMA |
						VPP_SEP_COEF);
					for (i = 0; i < VPP_FILER_COEFS_NUM; i++) {
						cur_dev->rdma_func[vpp_index].rdma_wr
						(vd_pps_reg->vd_scale_coef,
						vpp_filter->vpp_horz_coeff[i + 2]);
					}
					cur_dev->rdma_func[vpp_index].rdma_wr
						(vd_pps_reg->vd_scale_coef_idx,
						VPP_SEP_COEF_HORZ_CHROMA_PARTB |
						VPP_SEP_COEF);
					for (i = 0; i < VPP_FILER_COEFS_NUM; i++) {
						cur_dev->rdma_func[vpp_index].rdma_wr
						(vd_pps_reg->vd_scale_coef,
						vpp_filter->vpp_horz_coeff[i + 2 +
						VPP_FILER_COEFS_NUM]);
					}
				}
			}
		} else {
			if (bit9_mode || s11_mode) {
				cur_dev->rdma_func[vpp_index].rdma_wr
					(vd_pps_reg->vd_scale_coef_idx,
					VPP_COEF_HORZ | VPP_COEF_9BIT);
				for (i = 0; i <
					(vpp_filter->vpp_horz_coeff[1]
					& 0xff); i++) {
					cur_dev->rdma_func[vpp_index].rdma_wr
					(vd_pps_reg->vd_scale_coef,
					vpp_horz_coeff[i + 2]);
				}
				for (i = 0; i < VPP_FILER_COEFS_NUM; i++) {
					cur_dev->rdma_func[vpp_index].rdma_wr
					(vd_pps_reg->vd_scale_coef,
					vpp_horz_coeff[i + 2 +
					VPP_FILER_COEFS_NUM]);
				}
				if (cur_dev->scaler_sep_coef_en) {
					cur_dev->rdma_func[vpp_index].rdma_wr
						(vd_pps_reg->vd_scale_coef_idx,
						VPP_SEP_COEF_HORZ_CHROMA |
						VPP_SEP_COEF |
						VPP_COEF_9BIT);
					for (i = 0; i <
						(vpp_filter->vpp_horz_coeff[1]
						& 0xff); i++) {
						cur_dev->rdma_func[vpp_index].rdma_wr
						(vd_pps_reg->vd_scale_coef,
						vpp_filter->vpp_horz_coeff[i + 2]);
					}
					for (i = 0; i < VPP_FILER_COEFS_NUM; i++) {
						cur_dev->rdma_func[vpp_index].rdma_wr
						(vd_pps_reg->vd_scale_coef,
						vpp_filter->vpp_horz_coeff[i + 2 +
						VPP_FILER_COEFS_NUM]);
					}
				}
			} else {
				cur_dev->rdma_func[vpp_index].rdma_wr
					(vd_pps_reg->vd_scale_coef_idx,
					VPP_COEF_HORZ);
				for (i = 0; i < (vpp_filter->vpp_horz_coeff[1]
					& 0xff); i++) {
					cur_dev->rdma_func[vpp_index].rdma_wr
					(vd_pps_reg->vd_scale_coef,
					vpp_horz_coeff[i + 2]);
				}
				if (cur_dev->scaler_sep_coef_en) {
					cur_dev->rdma_func[vpp_index].rdma_wr
						(vd_pps_reg->vd_scale_coef_idx,
						VPP_SEP_COEF_HORZ_CHROMA |
						VPP_SEP_COEF);
					for (i = 0; i < (vpp_filter->vpp_horz_coeff[1]
						& 0xff); i++) {
						cur_dev->rdma_func[vpp_index].rdma_wr
						(vd_pps_reg->vd_scale_coef,
						vpp_filter->vpp_horz_coeff[i + 2]);
					}
				}
			}
		}
		r1 = frame_par->VPP_hsc_linear_startp
			- frame_par->VPP_hsc_startp;

		if (use_frm_horz_phase_step) {
			r3 = vd_proc_pps->dout_hsize - 1;
			r2 = vd_proc_pps->dout_hsize;
			if (debug_flag_s5 & DEBUG_VD_PROC)
				pr_info("%s: dout_hsize=0x%x, dout_vsize=0x%x\n",
					__func__,
					vd_proc_pps->dout_hsize,
					vd_proc_pps->dout_vsize);
		} else {
			r2 = frame_par->VPP_hsc_linear_endp
				- frame_par->VPP_hsc_startp;
			r3 = frame_par->VPP_hsc_endp
				- frame_par->VPP_hsc_startp;
		}
		if (frame_par->supscl_path ==
		     CORE0_PPS_CORE1 ||
		    frame_par->supscl_path ==
		     CORE1_AFTER_PPS ||
		    frame_par->supscl_path ==
		     PPS_CORE0_CORE1 ||
		    frame_par->supscl_path ==
		     PPS_CORE0_POSTBLEND_CORE1 ||
		    frame_par->supscl_path ==
		     PPS_POSTBLEND_CORE1 ||
		    frame_par->supscl_path ==
		     PPS_CORE1_CM)
			r3 >>= frame_par->supsc1_hori_ratio;
		if (frame_par->supscl_path ==
		     CORE0_AFTER_PPS ||
		    frame_par->supscl_path ==
		     PPS_CORE0_CORE1 ||
		    frame_par->supscl_path ==
		     PPS_CORE0_POSTBLEND_CORE1)
			r3 >>= frame_par->supsc0_hori_ratio;

		if (has_pre_hscaler_ntap(0)) {
			int ds_ratio = 1;
			int flt_num = 4;
			int pre_hscaler_table[4] = {
				0x0, 0x0, 0xf8, 0x48};
			get_pre_hscaler_para(0, &ds_ratio, &flt_num);
			if (hscaler_8tap_enable[0]) {
				if (use_frm_horz_phase_step) {
					hsc_init_rev_num0 =
						slice == 0 ? 8 : 8 - (slice_x_st + 1 -
						(slice_ini_sum >> 24));
					frame_par->hsc_rpt_p0_num0 = slice == 0 ? 3 : 2;
				} else {
					hsc_init_rev_num0 = 8;
				}
			} else {
				if (use_frm_horz_phase_step) {
					hsc_init_rev_num0 =
						slice == 0 ? 4 : 4 - (slice_x_st + 1 -
						(slice_ini_sum >> 24));
					frame_par->hsc_rpt_p0_num0 = slice == 0 ? 1 : 0;
				} else {
					hsc_init_rev_num0 = 4;
				}
			}
			cur_dev->rdma_func[vpp_index].rdma_wr_bits
				(vd_pps_reg->vd_hsc_phase_ctrl1,
				frame_par->VPP_hf_ini_phase_,
				VPP_HSC_TOP_INI_PHASE_BIT,
				VPP_HSC_TOP_INI_PHASE_WID);
			cur_dev->rdma_func[vpp_index].rdma_wr_bits
				(vd_pps_reg->vd_hsc_phase_ctrl,
				hsc_init_rev_num0,
				VPP_HSC_INIRCV_NUM_BIT,
				VPP_HSC_INIRCV_NUM_WID);
			cur_dev->rdma_func[vpp_index].rdma_wr_bits
				(vd_pps_reg->vd_hsc_phase_ctrl,
				frame_par->hsc_rpt_p0_num0,
				VPP_HSC_INIRPT_NUM_BIT_8TAP,
				VPP_HSC_INIRPT_NUM_WID_8TAP);
			cur_dev->rdma_func[vpp_index].rdma_wr_bits
				(vd_pps_reg->vd_hsc_phase_ctrl1,
				hsc_init_rev_num0,
				VPP_HSC_INIRCV_NUM_BIT,
				VPP_HSC_INIRCV_NUM_WID);
			cur_dev->rdma_func[vpp_index].rdma_wr_bits
				(vd_pps_reg->vd_hsc_phase_ctrl1,
				frame_par->hsc_rpt_p0_num0,
				VPP_HSC_INIRPT_NUM_BIT_8TAP,
				VPP_HSC_INIRPT_NUM_WID_8TAP);
			if (has_pre_hscaler_8tap(0)) {
				/* 8 tap */
				get_pre_hscaler_coef(0, pre_hscaler_table);
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
					(vd_pps_reg->vd_prehsc_coef,
					pre_hscaler_table[0],
					VPP_PREHSC_8TAP_COEF0_BIT,
					VPP_PREHSC_8TAP_COEF0_WID);
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
					(vd_pps_reg->vd_prehsc_coef,
					pre_hscaler_table[1],
					VPP_PREHSC_8TAP_COEF1_BIT,
					VPP_PREHSC_8TAP_COEF1_WID);
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
					(vd_pps_reg->vd_prehsc_coef1,
					pre_hscaler_table[2],
					VPP_PREHSC_8TAP_COEF2_BIT,
					VPP_PREHSC_8TAP_COEF2_WID);
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
					(vd_pps_reg->vd_prehsc_coef1,
					pre_hscaler_table[3],
					VPP_PREHSC_8TAP_COEF3_BIT,
					VPP_PREHSC_8TAP_COEF3_WID);
			} else {
				/* 2,4 tap */
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
					(vd_pps_reg->vd_prehsc_coef,
					pre_hscaler_table[0],
					VPP_PREHSC_COEF0_BIT,
					VPP_PREHSC_COEF0_WID);
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
					(vd_pps_reg->vd_prehsc_coef,
					pre_hscaler_table[1],
					VPP_PREHSC_COEF1_BIT,
					VPP_PREHSC_COEF1_WID);
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
					(vd_pps_reg->vd_prehsc_coef,
					pre_hscaler_table[2],
					VPP_PREHSC_COEF2_BIT,
					VPP_PREHSC_COEF2_WID);
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
					(vd_pps_reg->vd_prehsc_coef,
					pre_hscaler_table[3],
					VPP_PREHSC_COEF3_BIT,
					VPP_PREHSC_COEF3_WID);
			}
			if (has_pre_vscaler_ntap(0)) {
				/* T5, T7 */
				if (has_pre_hscaler_8tap(0)) {
					/* T7 */
					cur_dev->rdma_func[vpp_index].rdma_wr_bits
						(vd_pps_reg->vd_pre_scale_ctrl,
						flt_num,
						VPP_PREHSC_FLT_NUM_BIT_T7,
						VPP_PREHSC_FLT_NUM_WID_T7);
					cur_dev->rdma_func[vpp_index].rdma_wr_bits
						(vd_pps_reg->vd_pre_scale_ctrl,
						ds_ratio,
						VPP_PREHSC_DS_RATIO_BIT_T7,
						VPP_PREHSC_DS_RATIO_WID_T7);
				} else {
					/* T5 */
					cur_dev->rdma_func[vpp_index].rdma_wr_bits
						(vd_pps_reg->vd_pre_scale_ctrl,
						flt_num,
						VPP_PREHSC_FLT_NUM_BIT_T5,
						VPP_PREHSC_FLT_NUM_WID_T5);
					cur_dev->rdma_func[vpp_index].rdma_wr_bits
						(vd_pps_reg->vd_pre_scale_ctrl,
						ds_ratio,
						VPP_PREHSC_DS_RATIO_BIT_T5,
						VPP_PREHSC_DS_RATIO_WID_T5);
				}
			} else {
				/* SC2 */
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
					(vd_pps_reg->vd_pre_scale_ctrl,
					flt_num,
					VPP_PREHSC_FLT_NUM_BIT,
					VPP_PREHSC_FLT_NUM_WID);
			}
		}
		if (has_pre_vscaler_ntap(0)) {
			int ds_ratio = 1;
			int flt_num = 4;
			int pre_vscaler_table[2];

			get_pre_vscaler_para(0, &ds_ratio, &flt_num);
			get_pre_vscaler_coef(0, pre_vscaler_table);
			if (has_pre_hscaler_8tap(0)) {
				//int pre_vscaler_table[2] = {0xc0, 0x40};

				if (!pre_scaler[0].pre_vscaler_ntap_enable) {
					pre_vscaler_table[0] = 0x100;
					pre_vscaler_table[1] = 0x0;
					flt_num = 2;
				}
				/* T7 */
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
					(vd_pps_reg->vd_prevsc_coef,
					pre_vscaler_table[0],
					VPP_PREVSC_COEF0_BIT_T7,
					VPP_PREVSC_COEF0_WID_T7);
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
					(vd_pps_reg->vd_prevsc_coef,
					pre_vscaler_table[1],
					VPP_PREVSC_COEF1_BIT_T7,
					VPP_PREVSC_COEF1_WID_T7);

				cur_dev->rdma_func[vpp_index].rdma_wr_bits
					(vd_pps_reg->vd_pre_scale_ctrl,
					flt_num,
					VPP_PREVSC_FLT_NUM_BIT_T7,
					VPP_PREVSC_FLT_NUM_WID_T7);
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
					(vd_pps_reg->vd_pre_scale_ctrl,
					ds_ratio,
					VPP_PREVSC_DS_RATIO_BIT_T7,
					VPP_PREVSC_DS_RATIO_WID_T7);
			} else {
				//int pre_vscaler_table[2] = {0xf8, 0x48};

				if (!pre_scaler[0].pre_vscaler_ntap_enable) {
					pre_vscaler_table[0] = 0;
					pre_vscaler_table[1] = 0x40;
					flt_num = 2;
				}
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
					(vd_pps_reg->vd_prevsc_coef,
					pre_vscaler_table[0],
					VPP_PREVSC_COEF0_BIT,
					VPP_PREVSC_COEF0_WID);
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
					(vd_pps_reg->vd_prevsc_coef,
					pre_vscaler_table[1],
					VPP_PREVSC_COEF1_BIT,
					VPP_PREVSC_COEF1_WID);
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
					(vd_pps_reg->vd_pre_scale_ctrl,
					flt_num,
					VPP_PREVSC_FLT_NUM_BIT_T5,
					VPP_PREVSC_FLT_NUM_WID_T5);
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
					(vd_pps_reg->vd_pre_scale_ctrl,
					ds_ratio,
					VPP_PREVSC_DS_RATIO_BIT_T5,
					VPP_PREVSC_DS_RATIO_WID_T5);
			}
		}

		if (use_frm_horz_phase_step) {
			cur_dev->rdma_func[vpp_index].rdma_wr_bits
				(vd_pps_reg->vd_hsc_phase_ctrl,
				slice_ini_phase_ori,
				VPP_HSC_TOP_INI_PHASE_BIT,
				VPP_HSC_TOP_INI_PHASE_WID);
			cur_dev->rdma_func[vpp_index].rdma_wr
				(vd_pps_reg->vd_hsc_region12_startp,
				(0 << VPP_REGION1_BIT) |
				((r1 & VPP_REGION_MASK_8K)
				<< VPP_REGION2_BIT));

			cur_dev->rdma_func[vpp_index].rdma_wr
				(vd_pps_reg->vd_hsc_region34_startp,
				((r2 & VPP_REGION_MASK_8K)
				<< VPP_REGION3_BIT) |
				((r2  & VPP_REGION_MASK_8K)
				<< VPP_REGION4_BIT));
			cur_dev->rdma_func[vpp_index].rdma_wr
				(vd_pps_reg->vd_hsc_region4_endp, r3);
		} else {
			cur_dev->rdma_func[vpp_index].rdma_wr_bits
				(vd_pps_reg->vd_hsc_phase_ctrl,
				frame_par->VPP_hf_ini_phase_,
				VPP_HSC_TOP_INI_PHASE_BIT,
				VPP_HSC_TOP_INI_PHASE_WID);
			cur_dev->rdma_func[vpp_index].rdma_wr
				(vd_pps_reg->vd_hsc_region12_startp,
				(0 << VPP_REGION1_BIT) |
				((r1 & VPP_REGION_MASK_8K)
				<< VPP_REGION2_BIT));

			cur_dev->rdma_func[vpp_index].rdma_wr
				(vd_pps_reg->vd_hsc_region34_startp,
				((r2 & VPP_REGION_MASK_8K)
				<< VPP_REGION3_BIT) |
				((r3 & VPP_REGION_MASK_8K)
				<< VPP_REGION4_BIT));
			cur_dev->rdma_func[vpp_index].rdma_wr
				(vd_pps_reg->vd_hsc_region4_endp,
				slice_ini_phase_exp << 16 |
				r3);
		}

		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_pps_reg->vd_hsc_start_phase_step,
			vpp_filter->vpp_hf_start_phase_step);

		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_pps_reg->vd_hsc_region1_phase_slope,
			vpp_filter->vpp_hf_start_phase_slope);

		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_pps_reg->vd_hsc_region3_phase_slope,
			vpp_filter->vpp_hf_end_phase_slope);
	}

	/* vertical filter settings */
	if (setting->sc_v_enable) {
		if (cur_dev->pps_auto_calc) {
			cur_dev->rdma_func[vpp_index].rdma_wr_bits
				(vd_pps_reg->vd_vsc_phase_ctrl,
				4,
				VPP_PHASECTL_INIRCVNUMT_BIT,
				VPP_PHASECTL_INIRCVNUM_WID);
			cur_dev->rdma_func[vpp_index].rdma_wr_bits
				(vd_pps_reg->vd_vsc_phase_ctrl,
				frame_par->vsc_top_rpt_l0_num,
				VPP_PHASECTL_INIRPTNUMT_BIT,
				VPP_PHASECTL_INIRPTNUM_WID);
			cur_dev->rdma_func[vpp_index].rdma_wr
				(vd_pps_reg->vd_vsc_init_phase,
				frame_par->VPP_vf_init_phase |
				(frame_par->VPP_vf_init_phase << 16));
		}
		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(vd_pps_reg->vd_vsc_phase_ctrl,
			(vpp_filter->vpp_vert_coeff[0] == 2) ? 1 : 0,
			VPP_PHASECTL_DOUBLELINE_BIT,
			VPP_PHASECTL_DOUBLELINE_WID);
		bit9_mode = vpp_filter->vpp_vert_coeff[1] & 0x8000;
		s11_mode = vpp_filter->vpp_vert_coeff[1] & 0x4000;
		if (s11_mode)
			cur_dev->rdma_func[vpp_index].rdma_wr_bits(vd_pps_reg->vd_pre_scale_ctrl,
					       0x199, 12, 9);
		else
			cur_dev->rdma_func[vpp_index].rdma_wr_bits(vd_pps_reg->vd_pre_scale_ctrl,
					       0x77, 12, 9);
		if (layer->aisr_mif_setting.aisr_enable &&
		   cur_dev->pps_auto_calc)
			vpp_vert_coeff = aisr_vpp_filter->vpp_vert_coeff;
		else
			vpp_vert_coeff = vpp_filter->vpp_vert_coeff;
		if (bit9_mode || s11_mode) {
			cur_dev->rdma_func[vpp_index].rdma_wr
				(vd_pps_reg->vd_scale_coef_idx,
				VPP_COEF_VERT |
				VPP_COEF_9BIT);
			for (i = 0; i < (vpp_filter->vpp_vert_coeff[1]
				& 0xff); i++) {
				cur_dev->rdma_func[vpp_index].rdma_wr
					(vd_pps_reg->vd_scale_coef,
					vpp_vert_coeff[i + 2]);
			}
			for (i = 0; i < VPP_FILER_COEFS_NUM; i++) {
				cur_dev->rdma_func[vpp_index].rdma_wr
					(vd_pps_reg->vd_scale_coef,
					vpp_vert_coeff[i + 2 +
					VPP_FILER_COEFS_NUM]);
			}
			if (cur_dev->scaler_sep_coef_en) {
				cur_dev->rdma_func[vpp_index].rdma_wr
					(vd_pps_reg->vd_scale_coef_idx,
					VPP_SEP_COEF_VERT_CHROMA |
					VPP_SEP_COEF |
					VPP_COEF_9BIT);
				for (i = 0; i < (vpp_filter->vpp_vert_coeff[1]
					& 0xff); i++) {
					cur_dev->rdma_func[vpp_index].rdma_wr
						(vd_pps_reg->vd_scale_coef,
						vpp_filter->vpp_vert_coeff[i + 2]);
				}
				for (i = 0; i < VPP_FILER_COEFS_NUM; i++) {
					cur_dev->rdma_func[vpp_index].rdma_wr
						(vd_pps_reg->vd_scale_coef,
						vpp_filter->vpp_vert_coeff[i + 2 +
						VPP_FILER_COEFS_NUM]);
				}
			}
		} else {
			cur_dev->rdma_func[vpp_index].rdma_wr
				(vd_pps_reg->vd_scale_coef_idx,
				VPP_COEF_VERT);
			for (i = 0; i < (vpp_filter->vpp_vert_coeff[1]
				& 0xff); i++) {
				cur_dev->rdma_func[vpp_index].rdma_wr
					(vd_pps_reg->vd_scale_coef,
					vpp_vert_coeff[i + 2]);
			}
			if (cur_dev->scaler_sep_coef_en) {
				cur_dev->rdma_func[vpp_index].rdma_wr
					(vd_pps_reg->vd_scale_coef_idx,
					VPP_SEP_COEF_VERT_CHROMA |
					VPP_SEP_COEF);
				for (i = 0; i < (vpp_filter->vpp_vert_coeff[1]
					& 0xff); i++) {
					cur_dev->rdma_func[vpp_index].rdma_wr
						(vd_pps_reg->vd_scale_coef,
						vpp_filter->vpp_vert_coeff[i + 2]);
				}
			}
		}
		/* vertical chroma filter settings */
		if (vpp_filter->vpp_vert_chroma_filter_en) {
			const u32 *pcoeff = vpp_filter->vpp_vert_chroma_coeff;

			bit9_mode = pcoeff[1] & 0x8000;
			s11_mode = pcoeff[1] & 0x4000;
			if (s11_mode)
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
				(vd_pps_reg->vd_pre_scale_ctrl,
				0x199, 12, 9);
			else
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
				(vd_pps_reg->vd_pre_scale_ctrl,
				0x77, 12, 9);
			if (bit9_mode || s11_mode) {
				cur_dev->rdma_func[vpp_index].rdma_wr
					(vd_pps_reg->vd_scale_coef_idx,
					VPP_COEF_VERT_CHROMA | VPP_COEF_SEP_EN);
				for (i = 0; i < pcoeff[1]; i++)
					cur_dev->rdma_func[vpp_index].rdma_wr
						(vd_pps_reg->vd_scale_coef,
						pcoeff[i + 2]);
				for (i = 0; i < VPP_FILER_COEFS_NUM; i++) {
					cur_dev->rdma_func[vpp_index].rdma_wr
						(vd_pps_reg->vd_scale_coef,
						pcoeff[i + 2 +
						VPP_FILER_COEFS_NUM]);
				}
			} else {
				cur_dev->rdma_func[vpp_index].rdma_wr
					(vd_pps_reg->vd_scale_coef_idx,
					VPP_COEF_VERT_CHROMA | VPP_COEF_SEP_EN);
				for (i = 0; i < pcoeff[1]; i++)
					cur_dev->rdma_func[vpp_index].rdma_wr
						(vd_pps_reg->vd_scale_coef,
						pcoeff[i + 2]);
			}
		}

		r1 = frame_par->VPP_vsc_endp
			- frame_par->VPP_vsc_startp;
		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_pps_reg->vd_vsc_region12_startp, 0);
		if (use_frm_horz_phase_step)
			cur_dev->rdma_func[vpp_index].rdma_wr
				(vd_pps_reg->vd_vsc_region34_startp,
				(((r1 + 1) & VPP_REGION_MASK_8K)
				<< VPP_REGION3_BIT) |
				(((r1 + 1) & VPP_REGION_MASK_8K)
				<< VPP_REGION4_BIT));
		else
			cur_dev->rdma_func[vpp_index].rdma_wr
				(vd_pps_reg->vd_vsc_region34_startp,
				((r1 & VPP_REGION_MASK_8K)
				<< VPP_REGION3_BIT) |
				((r1 & VPP_REGION_MASK_8K)
				<< VPP_REGION4_BIT));
		if (frame_par->supscl_path ==
		     CORE0_PPS_CORE1 ||
		    frame_par->supscl_path ==
		     CORE1_AFTER_PPS ||
		    frame_par->supscl_path ==
		     PPS_CORE0_POSTBLEND_CORE1 ||
		    frame_par->supscl_path ==
		     PPS_POSTBLEND_CORE1 ||
		    frame_par->supscl_path ==
		     PPS_CORE1_CM)
			r1 >>= frame_par->supsc1_vert_ratio;
		if (frame_par->supscl_path ==
		     CORE0_AFTER_PPS ||
		    frame_par->supscl_path ==
		     PPS_CORE0_CORE1 ||
		    frame_par->supscl_path ==
		     PPS_CORE0_POSTBLEND_CORE1 ||
		     frame_par->supscl_path ==
		     PPS_POSTBLEND_CORE1 ||
		     frame_par->supscl_path ==
		     PPS_CORE1_CM)
			r1 >>= frame_par->supsc0_vert_ratio;

		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_pps_reg->vd_vsc_region4_endp, r1);

		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_pps_reg->vd_vsc_start_phase_step,
			vpp_filter->vpp_vsc_start_phase_step);
	}

	/*pps input size setting, only for vd0 slice0 */
	if (slice == 0) {
		if (use_frm_horz_phase_step)
			cur_dev->rdma_func[vpp_index].rdma_wr
				(VD_PROC_S0_PPS_IN_SIZE,
				vd_proc_pps->din_hsize << 16 |
				vd_proc_pps->din_vsize);
		else
			cur_dev->rdma_func[vpp_index].rdma_wr
				(VD_PROC_S0_PPS_IN_SIZE,
				frame_par->VPP_line_in_length_ << 16 |
				frame_par->VPP_pic_in_height_);
	}
}

static void vdx_scaler_setting_s5(struct video_layer_s *layer,
	struct scaler_setting_s *setting)
{
	u32 i;
	u32 r1, r2, r3;
	struct vpp_frame_par_s *frame_par;
	struct vppfilter_mode_s *vpp_filter;
	u32 hsc_rpt_p0_num0 = 1;
	u32 hsc_init_rev_num0 = 4;
	struct vd_pps_reg_s *vd_pps_reg;
	u32 bit9_mode = 0, s11_mode = 0;
	u8 vpp_index, layer_id;

	if (!setting || !setting->frame_par)
		return;

	layer_id = layer->layer_id;
	frame_par = setting->frame_par;
	vpp_filter = &frame_par->vpp_filter;
	vd_pps_reg = &vd_proc_reg.vd_pps_reg[layer_id + SLICE_NUM - 1];
	vpp_index = layer->vpp_index;

	if (setting->sc_top_enable) {
		u32 sc_misc_val;

		sc_misc_val = VPP_SC_TOP_EN | VPP_SC_V1OUT_EN;
		if (setting->sc_h_enable) {
			if (has_pre_hscaler_ntap(layer_id)) {
				if (pre_scaler[layer_id].pre_hscaler_ntap_enable) {
					sc_misc_val |=
					(((vpp_filter->vpp_pre_hsc_en & 1)
					<< VPP_SC_PREHORZ_EN_BIT)
					| VPP_SC_HORZ_EN);
				} else {
					sc_misc_val |=
					(((vpp_filter->vpp_pre_hsc_en & 1)
					<< VPP_SC_PREHORZ_EN_BIT_OLD)
					| VPP_SC_HORZ_EN);
				}
			} else {
				sc_misc_val |=
					(((vpp_filter->vpp_pre_hsc_en & 1)
					<< VPP_SC_PREHORZ_EN_BIT)
					| VPP_SC_HORZ_EN);
			}
			if (hscaler_8tap_enable[layer_id])
				sc_misc_val |=
					((vpp_filter->vpp_horz_coeff[0] & 0xf)
					<< VPP_SC_HBANK_LENGTH_BIT);
			else
				sc_misc_val |=
					((vpp_filter->vpp_horz_coeff[0] & 7)
					<< VPP_SC_HBANK_LENGTH_BIT);
		}

		if (setting->sc_v_enable) {
			sc_misc_val |= (((vpp_filter->vpp_pre_vsc_en & 1)
				<< VPP_SC_PREVERT_EN_BIT)
				| VPP_SC_VERT_EN);
			sc_misc_val |= ((vpp_filter->vpp_pre_vsc_en & 1)
				<< VPP_LINE_BUFFER_EN_BIT);
			sc_misc_val |= ((vpp_filter->vpp_vert_coeff[0] & 7)
				<< VPP_SC_VBANK_LENGTH_BIT);
		}

		if (setting->last_line_fix)
			sc_misc_val |= VPP_PPS_LAST_LINE_FIX;

		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_pps_reg->vd_sc_misc,
			sc_misc_val);
		if (layer_id == 1)
			vpu_module_clk_enable_s5(vpp_index, VD2_SCALER, 0);
	} else {
		setting->sc_v_enable = false;
		setting->sc_h_enable = false;
		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(vd_pps_reg->vd_sc_misc,
			0, VPP_SC_TOP_EN_BIT,
			VPP_SC_TOP_EN_WID);
		if (layer_id == 1)
			vpu_module_clk_disable_s5(vpp_index, VD2_SCALER, 0);
	}

	/* horizontal filter settings */
	if (setting->sc_h_enable) {
		bit9_mode = vpp_filter->vpp_horz_coeff[1] & 0x8000;
		s11_mode = vpp_filter->vpp_horz_coeff[1] & 0x4000;
		if (s11_mode)
			cur_dev->rdma_func[vpp_index].rdma_wr_bits(vd_pps_reg->vd_pre_scale_ctrl,
					       0x199, 12, 9);
		else
			cur_dev->rdma_func[vpp_index].rdma_wr_bits(vd_pps_reg->vd_pre_scale_ctrl,
					       0x77, 12, 9);
		if (hscaler_8tap_enable[layer_id]) {
			if (bit9_mode || s11_mode) {
				cur_dev->rdma_func[vpp_index].rdma_wr
					(vd_pps_reg->vd_scale_coef_idx,
					VPP_COEF_HORZ | VPP_COEF_9BIT);
				for (i = 0; i < VPP_FILER_COEFS_NUM; i++) {
					cur_dev->rdma_func[vpp_index].rdma_wr
					(vd_pps_reg->vd_scale_coef,
					vpp_filter->vpp_horz_coeff[i + 2]);
				}
				for (i = 0; i < VPP_FILER_COEFS_NUM; i++) {
					cur_dev->rdma_func[vpp_index].rdma_wr
					(vd_pps_reg->vd_scale_coef,
					vpp_filter->vpp_horz_coeff[i + 2 +
					VPP_FILER_COEFS_NUM]);
				}
				cur_dev->rdma_func[vpp_index].rdma_wr(vd_pps_reg->vd_scale_coef_idx,
						  VPP_COEF_HORZ |
						  VPP_COEF_VERT_CHROMA |
						  VPP_COEF_9BIT);
				for (i = 0; i < VPP_FILER_COEFS_NUM; i++) {
					cur_dev->rdma_func[vpp_index].rdma_wr
					(vd_pps_reg->vd_scale_coef,
					vpp_filter->vpp_horz_coeff[i + 2 +
					VPP_FILER_COEFS_NUM * 2]);
				}
				for (i = 0; i < VPP_FILER_COEFS_NUM; i++) {
					cur_dev->rdma_func[vpp_index].rdma_wr
					(vd_pps_reg->vd_scale_coef,
					vpp_filter->vpp_horz_coeff[i + 2 +
					VPP_FILER_COEFS_NUM * 3]);
				}
			} else {
				cur_dev->rdma_func[vpp_index].rdma_wr(vd_pps_reg->vd_scale_coef_idx,
						  VPP_COEF_HORZ);
				for (i = 0; i < VPP_FILER_COEFS_NUM; i++) {
					cur_dev->rdma_func[vpp_index].rdma_wr
					(vd_pps_reg->vd_scale_coef,
					vpp_filter->vpp_horz_coeff[i + 2]);
				}
				cur_dev->rdma_func[vpp_index].rdma_wr(vd_pps_reg->vd_scale_coef_idx,
						  VPP_COEF_HORZ |
						  VPP_COEF_VERT_CHROMA);
				for (i = 0; i < VPP_FILER_COEFS_NUM; i++) {
					cur_dev->rdma_func[vpp_index].rdma_wr
					(vd_pps_reg->vd_scale_coef,
					vpp_filter->vpp_horz_coeff[i + 2 +
					VPP_FILER_COEFS_NUM]);
				}
			}
		} else {
			if (bit9_mode || s11_mode) {
				cur_dev->rdma_func[vpp_index].rdma_wr
					(vd_pps_reg->vd_scale_coef_idx,
					VPP_COEF_HORZ | VPP_COEF_9BIT);
				for (i = 0; i < (vpp_filter->vpp_horz_coeff[1]
					& 0xff); i++) {
					cur_dev->rdma_func[vpp_index].rdma_wr
					(vd_pps_reg->vd_scale_coef,
					vpp_filter->vpp_horz_coeff[i + 2]);
				}
				for (i = 0; i < VPP_FILER_COEFS_NUM; i++) {
					cur_dev->rdma_func[vpp_index].rdma_wr
					(vd_pps_reg->vd_scale_coef,
					vpp_filter->vpp_horz_coeff[i + 2 +
					VPP_FILER_COEFS_NUM]);
				}
			} else {
				cur_dev->rdma_func[vpp_index].rdma_wr
					(vd_pps_reg->vd_scale_coef_idx,
					VPP_COEF_HORZ);
				for (i = 0; i < (vpp_filter->vpp_horz_coeff[1]
					& 0xff); i++) {
					cur_dev->rdma_func[vpp_index].rdma_wr
					(vd_pps_reg->vd_scale_coef,
					vpp_filter->vpp_horz_coeff[i + 2]);
				}
			}
		}
		r1 = frame_par->VPP_hsc_linear_startp
			- frame_par->VPP_hsc_startp;
		r2 = frame_par->VPP_hsc_linear_endp
			- frame_par->VPP_hsc_startp;
		r3 = frame_par->VPP_hsc_endp
			- frame_par->VPP_hsc_startp;

		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(vd_pps_reg->vd_hsc_phase_ctrl,
			frame_par->VPP_hf_ini_phase_,
			VPP_HSC_TOP_INI_PHASE_BIT,
			VPP_HSC_TOP_INI_PHASE_WID);
		if (has_pre_hscaler_ntap(layer_id)) {
			int ds_ratio = 1;
			int flt_num = 4;
			int pre_hscaler_table[4] = {
				0x0, 0x0, 0xf8, 0x48};

			get_pre_hscaler_para(layer_id, &ds_ratio, &flt_num);
			cur_dev->rdma_func[vpp_index].rdma_wr_bits
				(vd_pps_reg->vd_hsc_phase_ctrl1,
				frame_par->VPP_hf_ini_phase_,
				VPP_HSC_TOP_INI_PHASE_BIT,
				VPP_HSC_TOP_INI_PHASE_WID);
			if (hscaler_8tap_enable[layer_id]) {
				hsc_rpt_p0_num0 = 3;
				hsc_init_rev_num0 = 8;
			} else {
				hsc_init_rev_num0 = 4;
				hsc_rpt_p0_num0 = 1;
			}
			cur_dev->rdma_func[vpp_index].rdma_wr_bits
				(vd_pps_reg->vd_hsc_phase_ctrl,
				hsc_init_rev_num0,
				VPP_HSC_INIRCV_NUM_BIT,
				VPP_HSC_INIRCV_NUM_WID);
			cur_dev->rdma_func[vpp_index].rdma_wr_bits
				(vd_pps_reg->vd_hsc_phase_ctrl,
				hsc_rpt_p0_num0,
				VPP_HSC_INIRPT_NUM_BIT_8TAP,
				VPP_HSC_INIRPT_NUM_WID_8TAP);
			cur_dev->rdma_func[vpp_index].rdma_wr_bits
				(vd_pps_reg->vd_hsc_phase_ctrl1,
				hsc_init_rev_num0,
				VPP_HSC_INIRCV_NUM_BIT,
				VPP_HSC_INIRCV_NUM_WID);
			cur_dev->rdma_func[vpp_index].rdma_wr_bits
				(vd_pps_reg->vd_hsc_phase_ctrl1,
				hsc_rpt_p0_num0,
				VPP_HSC_INIRPT_NUM_BIT_8TAP,
				VPP_HSC_INIRPT_NUM_WID_8TAP);
			if (has_pre_hscaler_8tap(layer_id)) {
				/* 8 tap */
				get_pre_hscaler_coef(layer_id, pre_hscaler_table);
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
					(vd_pps_reg->vd_prehsc_coef,
					pre_hscaler_table[0],
					VPP_PREHSC_8TAP_COEF0_BIT,
					VPP_PREHSC_8TAP_COEF0_WID);
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
					(vd_pps_reg->vd_prehsc_coef,
					pre_hscaler_table[1],
					VPP_PREHSC_8TAP_COEF1_BIT,
					VPP_PREHSC_8TAP_COEF1_WID);
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
					(vd_pps_reg->vd_prehsc_coef1,
					pre_hscaler_table[2],
					VPP_PREHSC_8TAP_COEF2_BIT,
					VPP_PREHSC_8TAP_COEF2_WID);
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
					(vd_pps_reg->vd_prehsc_coef1,
					pre_hscaler_table[3],
					VPP_PREHSC_8TAP_COEF3_BIT,
					VPP_PREHSC_8TAP_COEF3_WID);
			} else {
				/* 2,4 tap */
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
					(vd_pps_reg->vd_prehsc_coef,
					pre_hscaler_table[0],
					VPP_PREHSC_COEF0_BIT,
					VPP_PREHSC_COEF0_WID);
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
					(vd_pps_reg->vd_prehsc_coef,
					pre_hscaler_table[1],
					VPP_PREHSC_COEF1_BIT,
					VPP_PREHSC_COEF1_WID);
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
					(vd_pps_reg->vd_prehsc_coef,
					pre_hscaler_table[2],
					VPP_PREHSC_COEF2_BIT,
					VPP_PREHSC_COEF2_WID);
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
					(vd_pps_reg->vd_prehsc_coef,
					pre_hscaler_table[3],
					VPP_PREHSC_COEF3_BIT,
					VPP_PREHSC_COEF3_WID);
			}
			if (has_pre_vscaler_ntap(layer_id)) {
				/* T5, T7 */
				if (has_pre_hscaler_8tap(layer_id)) {
					/* T7 */
					cur_dev->rdma_func[vpp_index].rdma_wr_bits
						(vd_pps_reg->vd_pre_scale_ctrl,
						flt_num,
						VPP_PREHSC_FLT_NUM_BIT_T7,
						VPP_PREHSC_FLT_NUM_WID_T7);
					cur_dev->rdma_func[vpp_index].rdma_wr_bits
						(vd_pps_reg->vd_pre_scale_ctrl,
						ds_ratio,
						VPP_PREHSC_DS_RATIO_BIT_T7,
						VPP_PREHSC_DS_RATIO_WID_T7);
				} else {
					/* T5 */
					cur_dev->rdma_func[vpp_index].rdma_wr_bits
						(vd_pps_reg->vd_pre_scale_ctrl,
						flt_num,
						VPP_PREHSC_FLT_NUM_BIT_T5,
						VPP_PREHSC_FLT_NUM_WID_T5);
					cur_dev->rdma_func[vpp_index].rdma_wr_bits
						(vd_pps_reg->vd_pre_scale_ctrl,
						ds_ratio,
						VPP_PREHSC_DS_RATIO_BIT_T5,
						VPP_PREHSC_DS_RATIO_WID_T5);
				}
			} else {
				/* SC2 */
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
					(vd_pps_reg->vd_pre_scale_ctrl,
					flt_num,
					VPP_PREHSC_FLT_NUM_BIT,
					VPP_PREHSC_FLT_NUM_WID);
			}
		}
		if (has_pre_vscaler_ntap(layer_id)) {
			int ds_ratio = 1;
			int flt_num = 4;

			if (has_pre_hscaler_8tap(layer_id)) {
				int pre_vscaler_table[2] = {0xc0, 0x40};

				if (!pre_scaler[layer_id].pre_vscaler_ntap_enable) {
					pre_vscaler_table[0] = 0x100;
					pre_vscaler_table[1] = 0x0;
					flt_num = 2;
				}
				/* T7 */
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
					(vd_pps_reg->vd_prevsc_coef,
					pre_vscaler_table[0],
					VPP_PREVSC_COEF0_BIT_T7,
					VPP_PREVSC_COEF0_WID_T7);
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
					(vd_pps_reg->vd_prevsc_coef,
					pre_vscaler_table[1],
					VPP_PREVSC_COEF1_BIT_T7,
					VPP_PREVSC_COEF1_WID_T7);

				cur_dev->rdma_func[vpp_index].rdma_wr_bits
					(vd_pps_reg->vd_pre_scale_ctrl,
					flt_num,
					VPP_PREVSC_FLT_NUM_BIT_T7,
					VPP_PREVSC_FLT_NUM_WID_T7);
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
					(vd_pps_reg->vd_pre_scale_ctrl,
					ds_ratio,
					VPP_PREVSC_DS_RATIO_BIT_T7,
					VPP_PREVSC_DS_RATIO_WID_T7);

			} else {
				int pre_vscaler_table[2] = {0xf8, 0x48};

				if (!pre_scaler[layer_id].pre_vscaler_ntap_enable) {
					pre_vscaler_table[0] = 0;
					pre_vscaler_table[1] = 0x40;
					flt_num = 2;
				}
				/* T5 */
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
					(vd_pps_reg->vd_prevsc_coef,
					pre_vscaler_table[0],
					VPP_PREVSC_COEF0_BIT,
					VPP_PREVSC_COEF0_WID);
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
					(vd_pps_reg->vd_prevsc_coef,
					pre_vscaler_table[1],
					VPP_PREVSC_COEF1_BIT,
					VPP_PREVSC_COEF1_WID);
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
					(vd_pps_reg->vd_pre_scale_ctrl,
					flt_num,
					VPP_PREVSC_FLT_NUM_BIT_T5,
					VPP_PREVSC_FLT_NUM_WID_T5);
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
					(vd_pps_reg->vd_pre_scale_ctrl,
					ds_ratio,
					VPP_PREVSC_DS_RATIO_BIT_T5,
					VPP_PREVSC_DS_RATIO_WID_T5);
			}
		}
		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_pps_reg->vd_hsc_region12_startp,
			(0 << VPP_REGION1_BIT) |
			((r1 & VPP_REGION_MASK_8K)
			<< VPP_REGION2_BIT));
		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_pps_reg->vd_hsc_region34_startp,
			((r2 & VPP_REGION_MASK_8K)
			<< VPP_REGION3_BIT) |
			((r3 & VPP_REGION_MASK_8K)
			<< VPP_REGION4_BIT));
		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_pps_reg->vd_hsc_region4_endp, r3);

		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_pps_reg->vd_hsc_start_phase_step,
			vpp_filter->vpp_hf_start_phase_step);

		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_pps_reg->vd_hsc_region1_phase_slope,
			vpp_filter->vpp_hf_start_phase_slope);

		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_pps_reg->vd_hsc_region3_phase_slope,
			vpp_filter->vpp_hf_end_phase_slope);
	}

	/* vertical filter settings */
	if (setting->sc_v_enable) {
		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(vd_pps_reg->vd_vsc_phase_ctrl,
			(vpp_filter->vpp_vert_coeff[0] == 2) ? 1 : 0,
			VPP_PHASECTL_DOUBLELINE_BIT,
			VPP_PHASECTL_DOUBLELINE_WID);
		bit9_mode = vpp_filter->vpp_vert_coeff[1] & 0x8000;
		s11_mode = vpp_filter->vpp_vert_coeff[1] & 0x4000;
		if (s11_mode)
			cur_dev->rdma_func[vpp_index].rdma_wr_bits(vd_pps_reg->vd_pre_scale_ctrl,
					       0x199, 12, 9);
		else
			cur_dev->rdma_func[vpp_index].rdma_wr_bits(vd_pps_reg->vd_pre_scale_ctrl,
					       0x77, 12, 9);
		if (bit9_mode || s11_mode) {
			cur_dev->rdma_func[vpp_index].rdma_wr
				(vd_pps_reg->vd_scale_coef_idx,
				VPP_COEF_VERT |
				VPP_COEF_9BIT);
			for (i = 0; i < (vpp_filter->vpp_vert_coeff[1]
				& 0xff); i++) {
				cur_dev->rdma_func[vpp_index].rdma_wr
					(vd_pps_reg->vd_scale_coef,
					vpp_filter->vpp_vert_coeff[i + 2]);
			}
			for (i = 0; i < VPP_FILER_COEFS_NUM; i++) {
				cur_dev->rdma_func[vpp_index].rdma_wr
					(vd_pps_reg->vd_scale_coef,
					vpp_filter->vpp_vert_coeff[i + 2 +
					VPP_FILER_COEFS_NUM]);
			}
		} else {
			cur_dev->rdma_func[vpp_index].rdma_wr
				(vd_pps_reg->vd_scale_coef_idx,
				VPP_COEF_VERT);
			for (i = 0; i < (vpp_filter->vpp_vert_coeff[1]
				& 0xff); i++) {
				cur_dev->rdma_func[vpp_index].rdma_wr
					(vd_pps_reg->vd_scale_coef,
					vpp_filter->vpp_vert_coeff[i + 2]);
			}
		}

		/* vertical chroma filter settings */
		if (vpp_filter->vpp_vert_chroma_filter_en) {
			const u32 *pcoeff = vpp_filter->vpp_vert_chroma_coeff;

			bit9_mode = pcoeff[1] & 0x8000;
			s11_mode = pcoeff[1] & 0x4000;
			if (s11_mode)
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
				(vd_pps_reg->vd_pre_scale_ctrl,
				0x199, 12, 9);
			else
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
				(vd_pps_reg->vd_pre_scale_ctrl,
				0x77, 12, 9);
			if (bit9_mode || s11_mode) {
				cur_dev->rdma_func[vpp_index].rdma_wr
					(vd_pps_reg->vd_scale_coef_idx,
					VPP_COEF_VERT_CHROMA | VPP_COEF_SEP_EN);
				for (i = 0; i < pcoeff[1]; i++)
					cur_dev->rdma_func[vpp_index].rdma_wr
						(vd_pps_reg->vd_scale_coef,
						pcoeff[i + 2]);
				for (i = 0; i < VPP_FILER_COEFS_NUM; i++) {
					cur_dev->rdma_func[vpp_index].rdma_wr
						(vd_pps_reg->vd_scale_coef,
						pcoeff[i + 2 +
						VPP_FILER_COEFS_NUM]);
				}
			} else {
				cur_dev->rdma_func[vpp_index].rdma_wr
					(vd_pps_reg->vd_scale_coef_idx,
					VPP_COEF_VERT_CHROMA | VPP_COEF_SEP_EN);
				for (i = 0; i < pcoeff[1]; i++)
					cur_dev->rdma_func[vpp_index].rdma_wr
						(vd_pps_reg->vd_scale_coef,
						pcoeff[i + 2]);
			}
		}

		r1 = frame_par->VPP_vsc_endp
			- frame_par->VPP_vsc_startp;
		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_pps_reg->vd_vsc_region12_startp, 0);

		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_pps_reg->vd_vsc_region34_startp,
			((r1 & VPP_REGION_MASK_8K)
			<< VPP_REGION3_BIT) |
			((r1 & VPP_REGION_MASK_8K)
			<< VPP_REGION4_BIT));

		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_pps_reg->vd_vsc_region4_endp, r1);

		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_pps_reg->vd_vsc_start_phase_step,
			vpp_filter->vpp_vsc_start_phase_step);
	}
	/* pps input size = slice in size, not need set */
}

/* video hw api */
bool is_sr_phase_changed_s5(void)
{
	struct sr_info_s *sr;
	u32 sr0_sharp_sr2_ctrl2;
	u32 sr1_sharp_sr2_ctrl2;
	static u32 sr0_sharp_sr2_ctrl2_pre;
	static u32 sr1_sharp_sr2_ctrl2_pre;
	bool changed = false;
	struct vd_proc_sr_reg_s *vd_sr_reg = NULL;

	vd_sr_reg = &vd_proc_reg.vd_proc_sr_reg;
	if (!cur_dev->aisr_support ||
	    !cur_dev->pps_auto_calc)
		return false;

	sr = &sr_info;
	sr0_sharp_sr2_ctrl2 =
		cur_dev->rdma_func[VPP0].rdma_rd
			(vd_sr_reg->srsharp0_sharp_sr2_ctrl2);
	sr1_sharp_sr2_ctrl2 =
		cur_dev->rdma_func[VPP0].rdma_rd
			(vd_sr_reg->srsharp1_sharp_sr2_ctrl2);
	sr0_sharp_sr2_ctrl2 &= 0x7fff;
	sr1_sharp_sr2_ctrl2 &= 0x7fff;
	if (sr0_sharp_sr2_ctrl2 != sr0_sharp_sr2_ctrl2_pre ||
	    sr1_sharp_sr2_ctrl2 != sr1_sharp_sr2_ctrl2_pre)
		changed = true;
	sr0_sharp_sr2_ctrl2_pre = sr0_sharp_sr2_ctrl2;
	sr1_sharp_sr2_ctrl2_pre = sr1_sharp_sr2_ctrl2;
	return changed;

}

void vd_set_dcu_s5(u8 layer_id,
		struct video_layer_s *layer,
		struct vpp_frame_par_s *frame_par,
		struct vframe_s *vf)
{
	int slice = 0, temp_slice = 0;

	if (layer_id == 0) {
		if (get_vd1_work_mode() == VD1_1SLICES_MODE) {
			vd1_set_dcu_s5(layer, frame_par, vf);
		} else {
			for (slice = 0; slice < layer->slice_num; slice++) {
				if (layer->vd1s1_vd2_prebld_en &&
					layer->slice_num == 2 &&
					slice == 1)
					temp_slice = SLICE_NUM;
				else
					temp_slice = slice;
				vd1_set_slice_dcu_s5(layer, frame_par, vf, temp_slice);
			}
		}
	} else {
		vdx_set_dcu_s5(layer, frame_par, vf);
	}
}

static void vd_afbc_setting_s5(struct video_layer_s *layer, struct mif_pos_s *setting)
{
	int crop_left, crop_top;
	int vsize_in, hsize_in;
	int mif_blk_bgn_h, mif_blk_end_h;
	int mif_blk_bgn_v, mif_blk_end_v;
	int pix_bgn_h, pix_end_h;
	int pix_bgn_v, pix_end_v;
	struct vd_afbc_reg_s *vd_afbc_reg;
	u8 vpp_index, layer_id;

	if (!setting)
		return;
	layer_id = layer->layer_id;
	vpp_index = layer->vpp_index;

	vd_afbc_reg = &vd_proc_reg.vd_afbc_reg[setting->id];
	/* afbc horizontal setting */
	crop_left = setting->start_x_lines;
	hsize_in = round_up
		((setting->src_w - 1) + 1, 32);
	mif_blk_bgn_h = crop_left / 32;
	mif_blk_end_h = (crop_left + setting->end_x_lines -
		setting->start_x_lines) / 32;
	pix_bgn_h = crop_left - mif_blk_bgn_h * 32;
	pix_end_h = pix_bgn_h + setting->end_x_lines -
		setting->start_x_lines;

	if (((process_3d_type & MODE_3D_FA) ||
	     (process_3d_type & MODE_FORCE_3D_FA_LR)) &&
	    setting->vpp_3d_mode == 1) {
		/* do nothing*/
	} else {
		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_afbc_reg->afbc_mif_hor_scope,
			(mif_blk_bgn_h << 16) |
			mif_blk_end_h);
		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_afbc_reg->afbc_pixel_hor_scope,
			((pix_bgn_h << 16) |
			pix_end_h));
	}

	/* afbc vertical setting */
	crop_top = setting->start_y_lines;
	vsize_in = round_up((setting->src_h - 1) + 1, 4);
	mif_blk_bgn_v = crop_top / 4;
	mif_blk_end_v = (crop_top + setting->end_y_lines -
		setting->start_y_lines) / 4;
	pix_bgn_v = crop_top - mif_blk_bgn_v * 4;
	pix_end_v = pix_bgn_v + setting->end_y_lines -
		setting->start_y_lines;

	if (layer_id != 2 &&
	   ((process_3d_type & MODE_3D_FA) ||
	   (process_3d_type & MODE_FORCE_3D_FA_TB)) &&
	    setting->vpp_3d_mode == 2) {
		int block_h;

		block_h = vsize_in;
		block_h = block_h / 8;
		if (toggle_3d_fa_frame == OUT_FA_B_FRAME) {
			cur_dev->rdma_func[vpp_index].rdma_wr
				(vd_afbc_reg->afbc_mif_ver_scope,
				(block_h << 16) |
				(vsize_in / 4));
		} else {
			cur_dev->rdma_func[vpp_index].rdma_wr
				(vd_afbc_reg->afbc_mif_ver_scope,
				(0 << 16) |
				block_h);
		}
	} else {
		cur_dev->rdma_func[vpp_index].rdma_wr
			(vd_afbc_reg->afbc_mif_ver_scope,
			(mif_blk_bgn_v << 16) |
			mif_blk_end_v);
	}
	cur_dev->rdma_func[vpp_index].rdma_wr
		(vd_afbc_reg->afbc_pixel_ver_scope,
		(pix_bgn_v << 16) |
		pix_end_v);

	cur_dev->rdma_func[vpp_index].rdma_wr
		(vd_afbc_reg->afbc_size_in,
		(hsize_in << 16) |
		(vsize_in & 0xffff));
}

static void set_vd_mif_slice_linear_cs_s5(struct video_layer_s *layer,
				   struct canvas_s *cs0,
				   struct canvas_s *cs1,
				   struct canvas_s *cs2,
				   struct vframe_s *vf,
				   u32 lr_select,
				   u32 slice)
{
	u32 y_line_stride;
	u32 c_line_stride;
	int y_buffer_width, c_buffer_width;
	u64 baddr_y, baddr_cb, baddr_cr;
	struct vd_mif_linear_reg_s *vd_mif_linear_reg = NULL;
	u8 vpp_index;
	u32 vd_if_baddr_y, vd_if_baddr_cb, vd_if_baddr_cr;
	u32 vd_if_stride_0, vd_if_stride_1;

	if (layer->layer_id != 0 || slice > SLICE_NUM)
		return;

	vd_mif_linear_reg = &vd_proc_reg.vd_mif_linear_reg[slice];

	if (!lr_select) {
		vd_if_baddr_y = vd_mif_linear_reg->vd_if0_baddr_y;
		vd_if_baddr_cb = vd_mif_linear_reg->vd_if0_baddr_cb;
		vd_if_baddr_cr = vd_mif_linear_reg->vd_if0_baddr_cr;
		vd_if_stride_0 = vd_mif_linear_reg->vd_if0_stride_0;
		vd_if_stride_1 = vd_mif_linear_reg->vd_if0_stride_1;
	} else {
		vd_if_baddr_y = vd_mif_linear_reg->vd_if0_baddr_y_f1;
		vd_if_baddr_cb = vd_mif_linear_reg->vd_if0_baddr_cb_f1;
		vd_if_baddr_cr = vd_mif_linear_reg->vd_if0_baddr_cr_f1;
		vd_if_stride_0 = vd_mif_linear_reg->vd_if0_stride_0_f1;
		vd_if_stride_1 = vd_mif_linear_reg->vd_if0_stride_1_f1;
	}
	vpp_index = layer->vpp_index;
	if ((vf->type & VIDTYPE_VIU_444) ||
		    (vf->type & VIDTYPE_RGB_444) ||
		    (vf->type & VIDTYPE_VIU_422)) {
		baddr_y = cs0->addr;
		y_buffer_width = cs0->width;
		y_line_stride = viu_line_stride(y_buffer_width);
		baddr_cb = 0;
		baddr_cr = 0;
		cur_dev->rdma_func[vpp_index].rdma_wr(vd_if_baddr_y,
			baddr_y >> 4);
		cur_dev->rdma_func[vpp_index].rdma_wr(vd_if_baddr_cb,
			baddr_cb >> 4);
		cur_dev->rdma_func[vpp_index].rdma_wr(vd_if_baddr_cr,
			baddr_cr >> 4);
		cur_dev->rdma_func[vpp_index].rdma_wr(vd_if_stride_0,
			y_line_stride | 0 << 16);
		cur_dev->rdma_func[vpp_index].rdma_wr_bits(vd_if_stride_1,
			1 << 16 | 0, 0, 16);
	} else {
		baddr_y = cs0->addr;
		y_buffer_width = cs0->width;
		baddr_cb = cs1->addr;
		c_buffer_width = cs1->width;
		baddr_cr = cs2->addr;

		y_line_stride = viu_line_stride(y_buffer_width);
		c_line_stride = viu_line_stride(c_buffer_width);
		cur_dev->rdma_func[vpp_index].rdma_wr(vd_if_baddr_y,
			baddr_y >> 4);
		cur_dev->rdma_func[vpp_index].rdma_wr(vd_if_baddr_cb,
			baddr_cb >> 4);
		cur_dev->rdma_func[vpp_index].rdma_wr(vd_if_baddr_cr,
			baddr_cr >> 4);
		cur_dev->rdma_func[vpp_index].rdma_wr(vd_if_stride_0,
			y_line_stride | c_line_stride << 16);
		cur_dev->rdma_func[vpp_index].rdma_wr_bits(vd_if_stride_1,
			1 << 16 | c_line_stride, 0, 16);
	}
}

void set_vd_mif_linear_cs_s5(struct video_layer_s *layer,
				   struct canvas_s *cs0,
				   struct canvas_s *cs1,
				   struct canvas_s *cs2,
				   struct vframe_s *vf,
				   u32 lr_select)
{
	u32 y_line_stride;
	u32 c_line_stride;
	int y_buffer_width, c_buffer_width;
	u64 baddr_y, baddr_cb, baddr_cr;
	struct vd_mif_linear_reg_s *vd_mif_linear_reg = NULL;
	u8 vpp_index, layer_index;
	u32 vd_if_baddr_y, vd_if_baddr_cb, vd_if_baddr_cr;
	u32 vd_if_stride_0, vd_if_stride_1;

	if (layer->layer_id != 0)
		layer_index = layer->layer_id + SLICE_NUM - 1;
	else
		layer_index = layer->layer_id;
	vd_mif_linear_reg = &vd_proc_reg.vd_mif_linear_reg[layer_index];

	if (!lr_select) {
		vd_if_baddr_y = vd_mif_linear_reg->vd_if0_baddr_y;
		vd_if_baddr_cb = vd_mif_linear_reg->vd_if0_baddr_cb;
		vd_if_baddr_cr = vd_mif_linear_reg->vd_if0_baddr_cr;
		vd_if_stride_0 = vd_mif_linear_reg->vd_if0_stride_0;
		vd_if_stride_1 = vd_mif_linear_reg->vd_if0_stride_1;
	} else {
		vd_if_baddr_y = vd_mif_linear_reg->vd_if0_baddr_y_f1;
		vd_if_baddr_cb = vd_mif_linear_reg->vd_if0_baddr_cb_f1;
		vd_if_baddr_cr = vd_mif_linear_reg->vd_if0_baddr_cr_f1;
		vd_if_stride_0 = vd_mif_linear_reg->vd_if0_stride_0_f1;
		vd_if_stride_1 = vd_mif_linear_reg->vd_if0_stride_1_f1;
	}
	vpp_index = layer->vpp_index;
	if ((vf->type & VIDTYPE_VIU_444) ||
		    (vf->type & VIDTYPE_RGB_444) ||
		    (vf->type & VIDTYPE_VIU_422)) {
		baddr_y = cs0->addr;
		y_buffer_width = cs0->width;
		y_line_stride = viu_line_stride(y_buffer_width);
		baddr_cb = 0;
		baddr_cr = 0;
		cur_dev->rdma_func[vpp_index].rdma_wr(vd_if_baddr_y,
			baddr_y >> 4);
		cur_dev->rdma_func[vpp_index].rdma_wr(vd_if_baddr_cb,
			baddr_cb >> 4);
		cur_dev->rdma_func[vpp_index].rdma_wr(vd_if_baddr_cr,
			baddr_cr >> 4);
		cur_dev->rdma_func[vpp_index].rdma_wr(vd_if_stride_0,
			y_line_stride | 0 << 16);
		cur_dev->rdma_func[vpp_index].rdma_wr_bits(vd_if_stride_1,
			1 << 16 | 0, 0, 16);
	} else {
		baddr_y = cs0->addr;
		y_buffer_width = cs0->width;
		baddr_cb = cs1->addr;
		c_buffer_width = cs1->width;
		baddr_cr = cs2->addr;

		y_line_stride = viu_line_stride(y_buffer_width);
		c_line_stride = viu_line_stride(c_buffer_width);
		cur_dev->rdma_func[vpp_index].rdma_wr(vd_if_baddr_y,
			baddr_y >> 4);
		cur_dev->rdma_func[vpp_index].rdma_wr(vd_if_baddr_cb,
			baddr_cb >> 4);
		cur_dev->rdma_func[vpp_index].rdma_wr(vd_if_baddr_cr,
			baddr_cr >> 4);
		cur_dev->rdma_func[vpp_index].rdma_wr(vd_if_stride_0,
			y_line_stride | c_line_stride << 16);
		cur_dev->rdma_func[vpp_index].rdma_wr_bits(vd_if_stride_1,
			1 << 16 | c_line_stride, 0, 16);
	}
}

void set_vd_mif_slice_linear_s5(struct video_layer_s *layer,
				   struct canvas_config_s *config,
				   u32 planes,
				   struct vframe_s *vf,
				   u32 lr_select,
				   u32 slice)
{
	u32 y_line_stride;
	u32 c_line_stride;
	int y_buffer_width, c_buffer_width;
	u64 baddr_y, baddr_cb, baddr_cr;
	struct vd_mif_linear_reg_s *vd_mif_linear_reg = NULL;
	struct canvas_config_s *cfg = config;
	u8 vpp_index;
	u32 vd_if_baddr_y, vd_if_baddr_cb, vd_if_baddr_cr;
	u32 vd_if_stride_0, vd_if_stride_1;

	if (layer->layer_id != 0 || slice > SLICE_NUM)
		return;
	vd_mif_linear_reg = &vd_proc_reg.vd_mif_linear_reg[slice];
	vpp_index = layer->vpp_index;
	if (!lr_select) {
		vd_if_baddr_y = vd_mif_linear_reg->vd_if0_baddr_y;
		vd_if_baddr_cb = vd_mif_linear_reg->vd_if0_baddr_cb;
		vd_if_baddr_cr = vd_mif_linear_reg->vd_if0_baddr_cr;
		vd_if_stride_0 = vd_mif_linear_reg->vd_if0_stride_0;
		vd_if_stride_1 = vd_mif_linear_reg->vd_if0_stride_1;
	} else {
		vd_if_baddr_y = vd_mif_linear_reg->vd_if0_baddr_y_f1;
		vd_if_baddr_cb = vd_mif_linear_reg->vd_if0_baddr_cb_f1;
		vd_if_baddr_cr = vd_mif_linear_reg->vd_if0_baddr_cr_f1;
		vd_if_stride_0 = vd_mif_linear_reg->vd_if0_stride_0_f1;
		vd_if_stride_1 = vd_mif_linear_reg->vd_if0_stride_1_f1;
	}
	switch (planes) {
	case 1:
		baddr_y = cfg->phy_addr;
		y_buffer_width = cfg->width;
		y_line_stride = viu_line_stride(y_buffer_width);
		baddr_cb = 0;
		baddr_cr = 0;
		cur_dev->rdma_func[vpp_index].rdma_wr(vd_if_baddr_y,
			baddr_y >> 4);
		cur_dev->rdma_func[vpp_index].rdma_wr(vd_if_baddr_cb,
			baddr_cb >> 4);
		cur_dev->rdma_func[vpp_index].rdma_wr(vd_if_baddr_cr,
			baddr_cr >> 4);
		cur_dev->rdma_func[vpp_index].rdma_wr(vd_if_stride_0,
			y_line_stride | 0 << 16);
		cur_dev->rdma_func[vpp_index].rdma_wr_bits(vd_if_stride_1,
			1 << 16 | 0, 0, 16);
		break;
	case 2:
		baddr_y = cfg->phy_addr;
		y_buffer_width = cfg->width;
		cfg++;
		baddr_cb = cfg->phy_addr;
		c_buffer_width = cfg->width;
		baddr_cr = 0;
		y_line_stride = viu_line_stride(y_buffer_width);
		c_line_stride = viu_line_stride(c_buffer_width);
		cur_dev->rdma_func[vpp_index].rdma_wr(vd_if_baddr_y,
			baddr_y >> 4);
		cur_dev->rdma_func[vpp_index].rdma_wr(vd_if_baddr_cb,
			baddr_cb >> 4);
		cur_dev->rdma_func[vpp_index].rdma_wr(vd_if_baddr_cr,
			baddr_cr >> 4);
		cur_dev->rdma_func[vpp_index].rdma_wr(vd_if_stride_0,
			y_line_stride | c_line_stride << 16);
		cur_dev->rdma_func[vpp_index].rdma_wr_bits(vd_if_stride_1,
			1 << 16 | c_line_stride, 0, 16);
		break;
	case 3:
		baddr_y = cfg->phy_addr;
		y_buffer_width = cfg->width;
		cfg++;
		baddr_cb = cfg->phy_addr;
		c_buffer_width = cfg->width;
		cfg++;
		baddr_cr = cfg->phy_addr;
		y_line_stride = viu_line_stride(y_buffer_width);
		c_line_stride = viu_line_stride(c_buffer_width);
		cur_dev->rdma_func[vpp_index].rdma_wr(vd_if_baddr_y,
			baddr_y >> 4);
		cur_dev->rdma_func[vpp_index].rdma_wr(vd_if_baddr_cb,
			baddr_cb >> 4);
		cur_dev->rdma_func[vpp_index].rdma_wr(vd_if_baddr_cr,
			baddr_cr >> 4);
		cur_dev->rdma_func[vpp_index].rdma_wr(vd_if_stride_0,
			y_line_stride | c_line_stride << 16);
		cur_dev->rdma_func[vpp_index].rdma_wr_bits(vd_if_stride_1,
			1 << 16 | c_line_stride, 0, 16);
		break;
	default:
		pr_err("error planes=%d\n", planes);
		break;
	}
}

void set_vd_mif_linear_s5(struct video_layer_s *layer,
				   struct canvas_config_s *config,
				   u32 planes,
				   struct vframe_s *vf,
				   u32 lr_select)
{
	u32 y_line_stride;
	u32 c_line_stride;
	int y_buffer_width, c_buffer_width;
	u64 baddr_y, baddr_cb, baddr_cr;
	struct vd_mif_linear_reg_s *vd_mif_linear_reg = NULL;
	struct canvas_config_s *cfg = config;
	u8 vpp_index, layer_index;
	u32 vd_if_baddr_y, vd_if_baddr_cb, vd_if_baddr_cr;
	u32 vd_if_stride_0, vd_if_stride_1;

	if (layer->layer_id != 0)
		layer_index = layer->layer_id + SLICE_NUM - 1;
	else
		layer_index = layer->layer_id;
	vd_mif_linear_reg = &vd_proc_reg.vd_mif_linear_reg[layer_index];
	vpp_index = layer->vpp_index;
	if (!lr_select) {
		vd_if_baddr_y = vd_mif_linear_reg->vd_if0_baddr_y;
		vd_if_baddr_cb = vd_mif_linear_reg->vd_if0_baddr_cb;
		vd_if_baddr_cr = vd_mif_linear_reg->vd_if0_baddr_cr;
		vd_if_stride_0 = vd_mif_linear_reg->vd_if0_stride_0;
		vd_if_stride_1 = vd_mif_linear_reg->vd_if0_stride_1;
	} else {
		vd_if_baddr_y = vd_mif_linear_reg->vd_if0_baddr_y_f1;
		vd_if_baddr_cb = vd_mif_linear_reg->vd_if0_baddr_cb_f1;
		vd_if_baddr_cr = vd_mif_linear_reg->vd_if0_baddr_cr_f1;
		vd_if_stride_0 = vd_mif_linear_reg->vd_if0_stride_0_f1;
		vd_if_stride_1 = vd_mif_linear_reg->vd_if0_stride_1_f1;
	}
	switch (planes) {
	case 1:
		baddr_y = cfg->phy_addr;
		y_buffer_width = cfg->width;
		y_line_stride = viu_line_stride(y_buffer_width);
		baddr_cb = 0;
		baddr_cr = 0;
		cur_dev->rdma_func[vpp_index].rdma_wr(vd_if_baddr_y,
			baddr_y >> 4);
		cur_dev->rdma_func[vpp_index].rdma_wr(vd_if_baddr_cb,
			baddr_cb >> 4);
		cur_dev->rdma_func[vpp_index].rdma_wr(vd_if_baddr_cr,
			baddr_cr >> 4);
		cur_dev->rdma_func[vpp_index].rdma_wr(vd_if_stride_0,
			y_line_stride | 0 << 16);
		cur_dev->rdma_func[vpp_index].rdma_wr_bits(vd_if_stride_1,
			1 << 16 | 0, 0, 16);
		break;
	case 2:
		baddr_y = cfg->phy_addr;
		y_buffer_width = cfg->width;
		cfg++;
		baddr_cb = cfg->phy_addr;
		c_buffer_width = cfg->width;
		baddr_cr = 0;
		y_line_stride = viu_line_stride(y_buffer_width);
		c_line_stride = viu_line_stride(c_buffer_width);
		cur_dev->rdma_func[vpp_index].rdma_wr(vd_if_baddr_y,
			baddr_y >> 4);
		cur_dev->rdma_func[vpp_index].rdma_wr(vd_if_baddr_cb,
			baddr_cb >> 4);
		cur_dev->rdma_func[vpp_index].rdma_wr(vd_if_baddr_cr,
			baddr_cr >> 4);
		cur_dev->rdma_func[vpp_index].rdma_wr(vd_if_stride_0,
			y_line_stride | c_line_stride << 16);
		cur_dev->rdma_func[vpp_index].rdma_wr_bits(vd_if_stride_1,
			1 << 16 | c_line_stride, 0, 16);
		break;
	case 3:
		baddr_y = cfg->phy_addr;
		y_buffer_width = cfg->width;
		cfg++;
		baddr_cb = cfg->phy_addr;
		c_buffer_width = cfg->width;
		cfg++;
		baddr_cr = cfg->phy_addr;
		y_line_stride = viu_line_stride(y_buffer_width);
		c_line_stride = viu_line_stride(c_buffer_width);
		cur_dev->rdma_func[vpp_index].rdma_wr(vd_if_baddr_y,
			baddr_y >> 4);
		cur_dev->rdma_func[vpp_index].rdma_wr(vd_if_baddr_cb,
			baddr_cb >> 4);
		cur_dev->rdma_func[vpp_index].rdma_wr(vd_if_baddr_cr,
			baddr_cr >> 4);
		cur_dev->rdma_func[vpp_index].rdma_wr(vd_if_stride_0,
			y_line_stride | c_line_stride << 16);
		cur_dev->rdma_func[vpp_index].rdma_wr_bits(vd_if_stride_1,
			1 << 16 | c_line_stride, 0, 16);
		break;
	default:
		pr_err("error planes=%d\n", planes);
		break;
	}
}

void canvas_update_for_mif_slice(struct video_layer_s *layer,
			     struct vframe_s *vf,
			     u32 slice)
{
	struct canvas_s cs0[2], cs1[2], cs2[2];
	u32 *cur_canvas_tbl;
	u8 cur_canvas_id;

	cur_canvas_id = layer->cur_canvas_id;
	cur_canvas_tbl =
		&layer->canvas_tbl[cur_canvas_id][0];

	if (vf->canvas0Addr != (u32)-1) {
		canvas_copy(vf->canvas0Addr & 0xff,
			    cur_canvas_tbl[0]);
		canvas_copy((vf->canvas0Addr >> 8) & 0xff,
			    cur_canvas_tbl[1]);
		canvas_copy((vf->canvas0Addr >> 16) & 0xff,
			    cur_canvas_tbl[2]);
		if (cur_dev->mif_linear) {
			canvas_read(cur_canvas_tbl[0], &cs0[0]);
			canvas_read(cur_canvas_tbl[1], &cs1[0]);
			canvas_read(cur_canvas_tbl[2], &cs2[0]);
			set_vd_mif_slice_linear_cs_s5(layer,
				&cs0[0], &cs1[0], &cs2[0],
				vf,
				0,
				slice);
			layer->mif_setting.block_mode =
				cs0[0].blkmode;
		}
	} else {
		vframe_canvas_set
			(&vf->canvas0_config[0],
			vf->plane_num,
			&cur_canvas_tbl[0]);
		if (cur_dev->mif_linear) {
			set_vd_mif_slice_linear_s5(layer,
				&vf->canvas0_config[0],
				vf->plane_num,
				vf,
				0,
				slice);
			layer->mif_setting.block_mode =
				vf->canvas0_config[0].block_mode;
		}
	}
}

static void vd_mif_setting_s5(struct video_layer_s *layer,
			struct mif_pos_s *setting)
{
	u32 ls = 0, le = 0;
	u32 h_skip, v_skip, hc_skip, vc_skip;
	struct vd_mif_reg_s *vd_mif_reg;
	struct vd_afbc_reg_s *vd_afbc_reg;
	u8 vpp_index, layer_id;

	if (!setting)
		return;
	layer_id = layer->layer_id;
	vpp_index = layer->vpp_index;

	vd_mif_reg = &vd_proc_reg.vd_mif_reg[setting->id];
	vd_afbc_reg = &vd_proc_reg.vd_afbc_reg[setting->id];

	h_skip = setting->h_skip + 1;
	v_skip = setting->v_skip + 1;
	hc_skip = setting->hc_skip;
	vc_skip = setting->vc_skip;

	/* vd horizontal setting */
	cur_dev->rdma_func[vpp_index].rdma_wr
		(vd_mif_reg->vd_if0_luma_x0,
		(setting->l_hs_luma << VDIF_PIC_START_BIT) |
		(setting->l_he_luma << VDIF_PIC_END_BIT));

	cur_dev->rdma_func[vpp_index].rdma_wr
		(vd_mif_reg->vd_if0_chroma_x0,
		(setting->l_hs_chrm << VDIF_PIC_START_BIT) |
		(setting->l_he_chrm << VDIF_PIC_END_BIT));

	cur_dev->rdma_func[vpp_index].rdma_wr
		(vd_mif_reg->vd_if0_luma_x1,
		(setting->r_hs_luma  << VDIF_PIC_START_BIT) |
		(setting->r_he_luma  << VDIF_PIC_END_BIT));

	cur_dev->rdma_func[vpp_index].rdma_wr
		(vd_mif_reg->vd_if0_chroma_x1,
		(setting->r_hs_chrm << VDIF_PIC_START_BIT) |
		(setting->r_he_chrm << VDIF_PIC_END_BIT));

	ls = setting->start_x_lines;
	le = setting->end_x_lines;
	cur_dev->rdma_func[vpp_index].rdma_wr
		(vd_mif_reg->viu_vd_fmt_w,
		(((le - ls + 1) / h_skip)
		<< VD1_FMT_LUMA_WIDTH_BIT) |
		(((le / 2 - ls / 2 + 1) / h_skip)
		<< VD1_FMT_CHROMA_WIDTH_BIT));

	/* vd vertical setting */
	cur_dev->rdma_func[vpp_index].rdma_wr
		(vd_mif_reg->vd_if0_luma_y0,
		(setting->l_vs_luma << VDIF_PIC_START_BIT) |
		(setting->l_ve_luma << VDIF_PIC_END_BIT));

	cur_dev->rdma_func[vpp_index].rdma_wr
		(vd_mif_reg->vd_if0_chroma_y0,
		(setting->l_vs_chrm << VDIF_PIC_START_BIT) |
		(setting->l_ve_chrm << VDIF_PIC_END_BIT));

	cur_dev->rdma_func[vpp_index].rdma_wr
		(vd_mif_reg->vd_if0_luma_y1,
		(setting->r_vs_luma << VDIF_PIC_START_BIT) |
		(setting->r_ve_luma << VDIF_PIC_END_BIT));

	cur_dev->rdma_func[vpp_index].rdma_wr
		(vd_mif_reg->vd_if0_chroma_y1,
		(setting->r_vs_chrm << VDIF_PIC_START_BIT) |
		(setting->r_ve_chrm << VDIF_PIC_END_BIT));

	if (!setting->skip_afbc)
		vd_afbc_setting_s5(layer, setting);
}

static void vd_slice_mif_setting_s5(struct video_layer_s *layer,
			struct mif_pos_s *setting)
{
	u32 ls = 0, le = 0;
	u32 h_skip, v_skip, hc_skip, vc_skip;
	struct vd_mif_reg_s *vd_mif_reg;
	struct vd_afbc_reg_s *vd_afbc_reg;
	u8 vpp_index, layer_id;

	if (!setting)
		return;
	layer_id = layer->layer_id;
	vpp_index = layer->vpp_index;

	vd_mif_reg = &vd_proc_reg.vd_mif_reg[setting->id];
	vd_afbc_reg = &vd_proc_reg.vd_afbc_reg[setting->id];
	h_skip = setting->h_skip + 1;
	v_skip = setting->v_skip + 1;
	hc_skip = setting->hc_skip;
	vc_skip = setting->vc_skip;

	/* vd horizontal setting */
	cur_dev->rdma_func[vpp_index].rdma_wr
		(vd_mif_reg->vd_if0_luma_x0,
		(setting->l_hs_luma << VDIF_PIC_START_BIT) |
		(setting->l_he_luma << VDIF_PIC_END_BIT));

	cur_dev->rdma_func[vpp_index].rdma_wr
		(vd_mif_reg->vd_if0_chroma_x0,
		(setting->l_hs_chrm << VDIF_PIC_START_BIT) |
		(setting->l_he_chrm << VDIF_PIC_END_BIT));

	cur_dev->rdma_func[vpp_index].rdma_wr
		(vd_mif_reg->vd_if0_luma_x1,
		(setting->r_hs_luma  << VDIF_PIC_START_BIT) |
		(setting->r_he_luma  << VDIF_PIC_END_BIT));

	cur_dev->rdma_func[vpp_index].rdma_wr
		(vd_mif_reg->vd_if0_chroma_x1,
		(setting->r_hs_chrm << VDIF_PIC_START_BIT) |
		(setting->r_he_chrm << VDIF_PIC_END_BIT));

	ls = setting->start_x_lines;
	le = setting->end_x_lines;
	cur_dev->rdma_func[vpp_index].rdma_wr
		(vd_mif_reg->viu_vd_fmt_w,
		(((le - ls + 1) / h_skip)
		<< VD1_FMT_LUMA_WIDTH_BIT) |
		(((le / 2 - ls / 2 + 1) / h_skip)
		<< VD1_FMT_CHROMA_WIDTH_BIT));

	/* vd vertical setting */
	cur_dev->rdma_func[vpp_index].rdma_wr
		(vd_mif_reg->vd_if0_luma_y0,
		(setting->l_vs_luma << VDIF_PIC_START_BIT) |
		(setting->l_ve_luma << VDIF_PIC_END_BIT));

	cur_dev->rdma_func[vpp_index].rdma_wr
		(vd_mif_reg->vd_if0_chroma_y0,
		(setting->l_vs_chrm << VDIF_PIC_START_BIT) |
		(setting->l_ve_chrm << VDIF_PIC_END_BIT));

	cur_dev->rdma_func[vpp_index].rdma_wr
		(vd_mif_reg->vd_if0_luma_y1,
		(setting->r_vs_luma << VDIF_PIC_START_BIT) |
		(setting->r_ve_luma << VDIF_PIC_END_BIT));

	cur_dev->rdma_func[vpp_index].rdma_wr
		(vd_mif_reg->vd_if0_chroma_y1,
		(setting->r_vs_chrm << VDIF_PIC_START_BIT) |
		(setting->r_ve_chrm << VDIF_PIC_END_BIT));

	if (!setting->skip_afbc)
		vd_afbc_setting_s5(layer, setting);
}

static void _vd_mif_setting_s5(struct video_layer_s *layer,
			struct mif_pos_s *setting)
{
	int slice = 0;

	if (layer->layer_id == 0) {
		if (get_vd1_work_mode() == VD1_1SLICES_MODE) {
			vd_mif_setting_s5(layer, setting);
		} else {
			for (slice = 0; slice < layer->slice_num; slice++) {
				if (layer->vd1s1_vd2_prebld_en &&
					layer->slice_num == 2 &&
					slice == 1)
					layer->slice_mif_setting[slice].id = SLICE_NUM;
				else
					layer->slice_mif_setting[slice].id = slice;
				vd_slice_mif_setting_s5(layer, &layer->slice_mif_setting[slice]);
			}
		}
	} else {
		setting->id += SLICE_NUM - 1;
		vd_mif_setting_s5(layer, setting);
	}
}

void proc_vd_vsc_phase_per_vsync_s5(struct video_layer_s *layer,
				 struct vpp_frame_par_s *frame_par,
				 struct vframe_s *vf)
{
}

void switch_3d_view_per_vsync_s5(struct video_layer_s *layer)
{
}

void aisr_sr1_nn_enable_s5(u32 enable)
{
	struct vd_proc_sr_reg_s *vd_sr_reg = NULL;

	if (!cur_dev->aisr_support)
		return;
	vd_sr_reg = &vd_proc_reg.vd_proc_sr_reg;

	if (enable)
		cur_dev->rdma_func[VPP0].rdma_wr_bits
			(vd_sr_reg->srsharp1_nn_post_top,
			0x3, 13, 2);
	else
		cur_dev->rdma_func[VPP0].rdma_wr_bits
			(vd_sr_reg->srsharp1_nn_post_top,
			0x0, 13, 2);
}

void aisr_sr1_nn_enable_sync_s5(u32 enable)
{
	struct vd_proc_sr_reg_s *vd_sr_reg = NULL;

	if (!cur_dev->aisr_support)
		return;
	vd_sr_reg = &vd_proc_reg.vd_proc_sr_reg;

	if (enable)
		WRITE_VCBUS_REG_BITS
			(vd_sr_reg->srsharp1_nn_post_top,
			0x3, 13, 2);
	else
		WRITE_VCBUS_REG_BITS
			(vd_sr_reg->srsharp1_nn_post_top,
			0x0, 13, 2);
}

void aisr_scaler_setting_s5(struct video_layer_s *layer,
			     struct scaler_setting_s *setting)
{
	u32 i;
	u32 r1, r2, r3;
	struct vpp_frame_par_s *frame_par;
	struct vppfilter_mode_s *vpp_filter;
	u32 hsc_init_rev_num0 = 4;
	struct vd_pps_reg_s *aisr_pps_reg;
	u32 bit9_mode = 0, s11_mode = 0;
	u8 vpp_index, layer_id;
	u32 aisr_enable = layer->aisr_mif_setting.aisr_enable;

	if (!is_layer_aisr_supported(layer) ||
	    !setting || !setting->frame_par)
		return;

	if (!aisr_enable) {
		aisr_sr1_nn_enable_s5(0);
		video_info_change_status &= ~VIDEO_AISR_FRAME_EVENT;
		return;
	}
	video_info_change_status |= VIDEO_AISR_FRAME_EVENT;
	layer_id = layer->layer_id;
	frame_par = setting->frame_par;
	vpp_filter = &frame_par->vpp_filter;
	aisr_pps_reg = &vd_proc_reg.vd_pps_reg[AISR_SCHN];
	vpp_index = layer->vpp_index;

	if (setting->sc_top_enable) {
		u32 sc_misc_val;

		sc_misc_val = VPP_SC_TOP_EN | VPP_SC_V1OUT_EN;
		if (setting->sc_h_enable) {
			if (has_pre_hscaler_ntap(layer_id)) {
				if (pre_scaler[layer_id].pre_hscaler_ntap_enable) {
					sc_misc_val |=
					(((vpp_filter->vpp_pre_hsc_en & 1)
					<< VPP_SC_PREHORZ_EN_BIT)
					| VPP_SC_HORZ_EN);
				} else {
					sc_misc_val |=
					(((vpp_filter->vpp_pre_hsc_en & 1)
					<< VPP_SC_PREHORZ_EN_BIT_OLD)
					| VPP_SC_HORZ_EN);
				}
			} else {
				sc_misc_val |=
					(((vpp_filter->vpp_pre_hsc_en & 1)
					<< VPP_SC_PREHORZ_EN_BIT)
					| VPP_SC_HORZ_EN);
			}
			if (hscaler_8tap_enable[layer_id])
				sc_misc_val |=
					((vpp_filter->vpp_horz_coeff[0] & 0xf)
					<< VPP_SC_HBANK_LENGTH_BIT);
			else
				sc_misc_val |=
					((vpp_filter->vpp_horz_coeff[0] & 7)
					<< VPP_SC_HBANK_LENGTH_BIT);
		}

		if (setting->sc_v_enable) {
			sc_misc_val |= (((vpp_filter->vpp_pre_vsc_en & 1)
				<< VPP_SC_PREVERT_EN_BIT)
				| VPP_SC_VERT_EN);
			sc_misc_val |= ((vpp_filter->vpp_pre_vsc_en & 1)
				<< VPP_LINE_BUFFER_EN_BIT);
			sc_misc_val |= ((vpp_filter->vpp_vert_coeff[0] & 7)
				<< VPP_SC_VBANK_LENGTH_BIT);
		}

		if (setting->last_line_fix)
			sc_misc_val |= VPP_PPS_LAST_LINE_FIX;

		cur_dev->rdma_func[vpp_index].rdma_wr
			(aisr_pps_reg->vd_sc_misc,
			sc_misc_val);
	} else {
		setting->sc_v_enable = false;
		setting->sc_h_enable = false;
		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(aisr_pps_reg->vd_sc_misc,
			0, VPP_SC_TOP_EN_BIT,
			VPP_SC_TOP_EN_WID);
	}

	/* horizontal filter settings */
	if (setting->sc_h_enable) {
		bit9_mode = vpp_filter->vpp_horz_coeff[1] & 0x8000;
		s11_mode = vpp_filter->vpp_horz_coeff[1] & 0x4000;
		if (s11_mode && cur_dev->display_module == T7_DISPLAY_MODULE)
			cur_dev->rdma_func[vpp_index].rdma_wr_bits(aisr_pps_reg->vd_pre_scale_ctrl,
					       0x199, 12, 9);
		else
			cur_dev->rdma_func[vpp_index].rdma_wr_bits(aisr_pps_reg->vd_pre_scale_ctrl,
					       0x77, 12, 9);
		if (hscaler_8tap_enable[layer_id]) {
			if (bit9_mode || s11_mode) {
				cur_dev->rdma_func[vpp_index].rdma_wr
					(aisr_pps_reg->vd_scale_coef_idx,
					VPP_COEF_HORZ | VPP_COEF_9BIT);
				for (i = 0; i < VPP_FILER_COEFS_NUM; i++) {
					cur_dev->rdma_func[vpp_index].rdma_wr
					(aisr_pps_reg->vd_scale_coef,
					vpp_filter->vpp_horz_coeff[i + 2]);
				}
				for (i = 0; i < VPP_FILER_COEFS_NUM; i++) {
					cur_dev->rdma_func[vpp_index].rdma_wr
					(aisr_pps_reg->vd_scale_coef,
					vpp_filter->vpp_horz_coeff[i + 2 +
					VPP_FILER_COEFS_NUM]);
				}
				cur_dev->rdma_func[vpp_index].rdma_wr
					(aisr_pps_reg->vd_scale_coef_idx,
					VPP_COEF_HORZ |
					VPP_COEF_VERT_CHROMA |
					VPP_COEF_9BIT);
				for (i = 0; i < VPP_FILER_COEFS_NUM; i++) {
					cur_dev->rdma_func[vpp_index].rdma_wr
					(aisr_pps_reg->vd_scale_coef,
					vpp_filter->vpp_horz_coeff[i + 2 +
					VPP_FILER_COEFS_NUM * 2]);
				}
				for (i = 0; i < VPP_FILER_COEFS_NUM; i++) {
					cur_dev->rdma_func[vpp_index].rdma_wr
					(aisr_pps_reg->vd_scale_coef,
					vpp_filter->vpp_horz_coeff[i + 2 +
					VPP_FILER_COEFS_NUM * 3]);
				}
			} else {
				cur_dev->rdma_func[vpp_index].rdma_wr
					(aisr_pps_reg->vd_scale_coef_idx,
					VPP_COEF_HORZ);
				for (i = 0; i < VPP_FILER_COEFS_NUM; i++) {
					cur_dev->rdma_func[vpp_index].rdma_wr
					(aisr_pps_reg->vd_scale_coef,
					vpp_filter->vpp_horz_coeff[i + 2]);
				}
				cur_dev->rdma_func[vpp_index].rdma_wr
					(aisr_pps_reg->vd_scale_coef_idx,
					VPP_COEF_HORZ |
					VPP_COEF_VERT_CHROMA);
				for (i = 0; i < VPP_FILER_COEFS_NUM; i++) {
					cur_dev->rdma_func[vpp_index].rdma_wr
					(aisr_pps_reg->vd_scale_coef,
					vpp_filter->vpp_horz_coeff[i + 2 +
					VPP_FILER_COEFS_NUM]);
				}
			}
		} else {
			if (bit9_mode || s11_mode) {
				cur_dev->rdma_func[vpp_index].rdma_wr
					(aisr_pps_reg->vd_scale_coef_idx,
					VPP_COEF_HORZ | VPP_COEF_9BIT);
				for (i = 0; i < (vpp_filter->vpp_horz_coeff[1]
					& 0xff); i++) {
					cur_dev->rdma_func[vpp_index].rdma_wr
					(aisr_pps_reg->vd_scale_coef,
					vpp_filter->vpp_horz_coeff[i + 2]);
				}
				for (i = 0; i < VPP_FILER_COEFS_NUM; i++) {
					cur_dev->rdma_func[vpp_index].rdma_wr
					(aisr_pps_reg->vd_scale_coef,
					vpp_filter->vpp_horz_coeff[i + 2 +
					VPP_FILER_COEFS_NUM]);
				}
			} else {
				cur_dev->rdma_func[vpp_index].rdma_wr
					(aisr_pps_reg->vd_scale_coef_idx,
					VPP_COEF_HORZ);
				for (i = 0; i < (vpp_filter->vpp_horz_coeff[1]
					& 0xff); i++) {
					cur_dev->rdma_func[vpp_index].rdma_wr
					(aisr_pps_reg->vd_scale_coef,
					vpp_filter->vpp_horz_coeff[i + 2]);
				}
			}
		}
		r1 = frame_par->VPP_hsc_linear_startp
			- frame_par->VPP_hsc_startp;
		r2 = frame_par->VPP_hsc_linear_endp
			- frame_par->VPP_hsc_startp;
		r3 = frame_par->VPP_hsc_endp
			- frame_par->VPP_hsc_startp;

		if (has_pre_hscaler_ntap(layer_id)) {
			int ds_ratio = 1;
			int flt_num = 4;
			int pre_hscaler_table[4] = {
				0x0, 0x0, 0xf8, 0x48};

			get_pre_hscaler_para(layer_id, &ds_ratio, &flt_num);
			if (hscaler_8tap_enable[layer_id])
				hsc_init_rev_num0 = 8;
			else
				hsc_init_rev_num0 = 4;
			cur_dev->rdma_func[vpp_index].rdma_wr_bits
				(aisr_pps_reg->vd_hsc_phase_ctrl1,
				frame_par->VPP_hf_ini_phase_,
				VPP_HSC_TOP_INI_PHASE_BIT,
				VPP_HSC_TOP_INI_PHASE_WID);
			cur_dev->rdma_func[vpp_index].rdma_wr_bits
				(aisr_pps_reg->vd_hsc_phase_ctrl,
				hsc_init_rev_num0,
				VPP_HSC_INIRCV_NUM_BIT,
				VPP_HSC_INIRCV_NUM_WID);
			cur_dev->rdma_func[vpp_index].rdma_wr_bits
				(aisr_pps_reg->vd_hsc_phase_ctrl,
				frame_par->hsc_rpt_p0_num0,
				VPP_HSC_INIRPT_NUM_BIT_8TAP,
				VPP_HSC_INIRPT_NUM_WID_8TAP);
			cur_dev->rdma_func[vpp_index].rdma_wr_bits
				(aisr_pps_reg->vd_hsc_phase_ctrl1,
				hsc_init_rev_num0,
				VPP_HSC_INIRCV_NUM_BIT,
				VPP_HSC_INIRCV_NUM_WID);
			cur_dev->rdma_func[vpp_index].rdma_wr_bits
				(aisr_pps_reg->vd_hsc_phase_ctrl1,
				frame_par->hsc_rpt_p0_num0,
				VPP_HSC_INIRPT_NUM_BIT_8TAP,
				VPP_HSC_INIRPT_NUM_WID_8TAP);
			if (has_pre_hscaler_8tap(layer_id)) {
				/* 8 tap */
				get_pre_hscaler_coef(layer_id, pre_hscaler_table);
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
					(aisr_pps_reg->vd_prehsc_coef,
					pre_hscaler_table[0],
					VPP_PREHSC_8TAP_COEF0_BIT,
					VPP_PREHSC_8TAP_COEF0_WID);
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
					(aisr_pps_reg->vd_prehsc_coef,
					pre_hscaler_table[1],
					VPP_PREHSC_8TAP_COEF1_BIT,
					VPP_PREHSC_8TAP_COEF1_WID);
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
					(aisr_pps_reg->vd_prehsc_coef1,
					pre_hscaler_table[2],
					VPP_PREHSC_8TAP_COEF2_BIT,
					VPP_PREHSC_8TAP_COEF2_WID);
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
					(aisr_pps_reg->vd_prehsc_coef1,
					pre_hscaler_table[3],
					VPP_PREHSC_8TAP_COEF3_BIT,
					VPP_PREHSC_8TAP_COEF3_WID);
			} else {
				/* 2,4 tap */
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
					(aisr_pps_reg->vd_prehsc_coef,
					pre_hscaler_table[0],
					VPP_PREHSC_COEF0_BIT,
					VPP_PREHSC_COEF0_WID);
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
					(aisr_pps_reg->vd_prehsc_coef,
					pre_hscaler_table[1],
					VPP_PREHSC_COEF1_BIT,
					VPP_PREHSC_COEF1_WID);
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
					(aisr_pps_reg->vd_prehsc_coef,
					pre_hscaler_table[2],
					VPP_PREHSC_COEF2_BIT,
					VPP_PREHSC_COEF2_WID);
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
					(aisr_pps_reg->vd_prehsc_coef,
					pre_hscaler_table[3],
					VPP_PREHSC_COEF3_BIT,
					VPP_PREHSC_COEF3_WID);
			}
			if (has_pre_vscaler_ntap(layer_id)) {
				/* T5, T7 */
				if (has_pre_hscaler_8tap(layer_id)) {
					/* T7 */
					cur_dev->rdma_func[vpp_index].rdma_wr_bits
						(aisr_pps_reg->vd_pre_scale_ctrl,
						flt_num,
						VPP_PREHSC_FLT_NUM_BIT_T7,
						VPP_PREHSC_FLT_NUM_WID_T7);
					cur_dev->rdma_func[vpp_index].rdma_wr_bits
						(aisr_pps_reg->vd_pre_scale_ctrl,
						ds_ratio,
						VPP_PREHSC_DS_RATIO_BIT_T7,
						VPP_PREHSC_DS_RATIO_WID_T7);
				} else {
					/* T5 */
					cur_dev->rdma_func[vpp_index].rdma_wr_bits
						(aisr_pps_reg->vd_pre_scale_ctrl,
						flt_num,
						VPP_PREHSC_FLT_NUM_BIT_T5,
						VPP_PREHSC_FLT_NUM_WID_T5);
					cur_dev->rdma_func[vpp_index].rdma_wr_bits
						(aisr_pps_reg->vd_pre_scale_ctrl,
						ds_ratio,
						VPP_PREHSC_DS_RATIO_BIT_T5,
						VPP_PREHSC_DS_RATIO_WID_T5);
				}
			} else {
				/* SC2 */
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
					(aisr_pps_reg->vd_pre_scale_ctrl,
					flt_num,
					VPP_PREHSC_FLT_NUM_BIT,
					VPP_PREHSC_FLT_NUM_WID);
			}
		}
		if (has_pre_vscaler_ntap(layer_id)) {
			int ds_ratio = 1;
			int flt_num = 4;

			if (has_pre_hscaler_8tap(layer_id)) {
				int pre_vscaler_table[2] = {0xc0, 0x40};

				if (!pre_scaler[layer_id].pre_vscaler_ntap_enable) {
					pre_vscaler_table[0] = 0x100;
					pre_vscaler_table[1] = 0x0;
					flt_num = 2;
				}
				/* T7 */
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
					(aisr_pps_reg->vd_prevsc_coef,
					pre_vscaler_table[0],
					VPP_PREVSC_COEF0_BIT_T7,
					VPP_PREVSC_COEF0_WID_T7);
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
					(aisr_pps_reg->vd_prevsc_coef,
					pre_vscaler_table[1],
					VPP_PREVSC_COEF1_BIT_T7,
					VPP_PREVSC_COEF1_WID_T7);

				cur_dev->rdma_func[vpp_index].rdma_wr_bits
					(aisr_pps_reg->vd_pre_scale_ctrl,
					flt_num,
					VPP_PREVSC_FLT_NUM_BIT_T7,
					VPP_PREVSC_FLT_NUM_WID_T7);
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
					(aisr_pps_reg->vd_pre_scale_ctrl,
					ds_ratio,
					VPP_PREVSC_DS_RATIO_BIT_T7,
					VPP_PREVSC_DS_RATIO_WID_T7);

			} else {
				int pre_vscaler_table[2] = {0xf8, 0x48};

				if (!pre_scaler[layer_id].pre_vscaler_ntap_enable) {
					pre_vscaler_table[0] = 0;
					pre_vscaler_table[1] = 0x40;
					flt_num = 2;
				}
				/* T5 */
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
					(aisr_pps_reg->vd_prevsc_coef,
					pre_vscaler_table[0],
					VPP_PREVSC_COEF0_BIT,
					VPP_PREVSC_COEF0_WID);
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
					(aisr_pps_reg->vd_prevsc_coef,
					pre_vscaler_table[1],
					VPP_PREVSC_COEF1_BIT,
					VPP_PREVSC_COEF1_WID);
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
					(aisr_pps_reg->vd_pre_scale_ctrl,
					flt_num,
					VPP_PREVSC_FLT_NUM_BIT_T5,
					VPP_PREVSC_FLT_NUM_WID_T5);
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
					(aisr_pps_reg->vd_pre_scale_ctrl,
					ds_ratio,
					VPP_PREVSC_DS_RATIO_BIT_T5,
					VPP_PREVSC_DS_RATIO_WID_T5);
			}
		}
		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(aisr_pps_reg->vd_hsc_phase_ctrl,
			frame_par->VPP_hf_ini_phase_,
			VPP_HSC_TOP_INI_PHASE_BIT,
			VPP_HSC_TOP_INI_PHASE_WID);
		cur_dev->rdma_func[vpp_index].rdma_wr
			(aisr_pps_reg->vd_hsc_region12_startp,
			(0 << VPP_REGION1_BIT) |
			((r1 & VPP_REGION_MASK)
			<< VPP_REGION2_BIT));
		cur_dev->rdma_func[vpp_index].rdma_wr
			(aisr_pps_reg->vd_hsc_region34_startp,
			((r2 & VPP_REGION_MASK)
			<< VPP_REGION3_BIT) |
			((r3 & VPP_REGION_MASK)
			<< VPP_REGION4_BIT));
		cur_dev->rdma_func[vpp_index].rdma_wr
			(aisr_pps_reg->vd_hsc_region4_endp, r3);

		cur_dev->rdma_func[vpp_index].rdma_wr
			(aisr_pps_reg->vd_hsc_start_phase_step,
			vpp_filter->vpp_hf_start_phase_step);

		cur_dev->rdma_func[vpp_index].rdma_wr
			(aisr_pps_reg->vd_hsc_region1_phase_slope,
			vpp_filter->vpp_hf_start_phase_slope);

		cur_dev->rdma_func[vpp_index].rdma_wr
			(aisr_pps_reg->vd_hsc_region3_phase_slope,
			vpp_filter->vpp_hf_end_phase_slope);
	}

	/* vertical filter settings */
	if (setting->sc_v_enable) {
		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(aisr_pps_reg->vd_vsc_phase_ctrl,
			4,
			VPP_PHASECTL_INIRCVNUMT_BIT,
			VPP_PHASECTL_INIRCVNUM_WID);
		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(aisr_pps_reg->vd_vsc_phase_ctrl,
			frame_par->vsc_top_rpt_l0_num,
			VPP_PHASECTL_INIRPTNUMT_BIT,
			VPP_PHASECTL_INIRPTNUM_WID);
		cur_dev->rdma_func[vpp_index].rdma_wr
			(aisr_pps_reg->vd_vsc_init_phase,
			frame_par->VPP_vf_init_phase |
			(frame_par->VPP_vf_init_phase << 16));
		cur_dev->rdma_func[vpp_index].rdma_wr_bits
			(aisr_pps_reg->vd_vsc_phase_ctrl,
			(vpp_filter->vpp_vert_coeff[0] == 2) ? 1 : 0,
			VPP_PHASECTL_DOUBLELINE_BIT,
			VPP_PHASECTL_DOUBLELINE_WID);
		bit9_mode = vpp_filter->vpp_vert_coeff[1] & 0x8000;
		s11_mode = vpp_filter->vpp_vert_coeff[1] & 0x4000;
		if (s11_mode && cur_dev->display_module == T7_DISPLAY_MODULE)
			cur_dev->rdma_func[vpp_index].rdma_wr_bits(aisr_pps_reg->vd_pre_scale_ctrl,
					       0x199, 12, 9);
		else
			cur_dev->rdma_func[vpp_index].rdma_wr_bits(aisr_pps_reg->vd_pre_scale_ctrl,
					       0x77, 12, 9);
		if (bit9_mode || s11_mode) {
			cur_dev->rdma_func[vpp_index].rdma_wr
				(aisr_pps_reg->vd_scale_coef_idx,
				VPP_COEF_VERT |
				VPP_COEF_9BIT);
			for (i = 0; i < (vpp_filter->vpp_vert_coeff[1]
				& 0xff); i++) {
				cur_dev->rdma_func[vpp_index].rdma_wr
					(aisr_pps_reg->vd_scale_coef,
					vpp_filter->vpp_vert_coeff[i + 2]);
			}
			for (i = 0; i < VPP_FILER_COEFS_NUM; i++) {
				cur_dev->rdma_func[vpp_index].rdma_wr
					(aisr_pps_reg->vd_scale_coef,
					vpp_filter->vpp_vert_coeff[i + 2 +
					VPP_FILER_COEFS_NUM]);
			}
		} else {
			cur_dev->rdma_func[vpp_index].rdma_wr
				(aisr_pps_reg->vd_scale_coef_idx,
				VPP_COEF_VERT);
			for (i = 0; i < (vpp_filter->vpp_vert_coeff[1]
				& 0xff); i++) {
				cur_dev->rdma_func[vpp_index].rdma_wr
					(aisr_pps_reg->vd_scale_coef,
					vpp_filter->vpp_vert_coeff[i + 2]);
			}
		}

		/* vertical chroma filter settings */
		if (vpp_filter->vpp_vert_chroma_filter_en) {
			const u32 *pcoeff = vpp_filter->vpp_vert_chroma_coeff;

			bit9_mode = pcoeff[1] & 0x8000;
			s11_mode = pcoeff[1] & 0x4000;
			if (s11_mode && cur_dev->display_module == T7_DISPLAY_MODULE)
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
				(aisr_pps_reg->vd_pre_scale_ctrl,
				0x199, 12, 9);
			else
				cur_dev->rdma_func[vpp_index].rdma_wr_bits
				(aisr_pps_reg->vd_pre_scale_ctrl,
				0x77, 12, 9);
			if (bit9_mode || s11_mode) {
				cur_dev->rdma_func[vpp_index].rdma_wr
					(aisr_pps_reg->vd_scale_coef_idx,
					VPP_COEF_VERT_CHROMA | VPP_COEF_SEP_EN);
				for (i = 0; i < pcoeff[1]; i++)
					cur_dev->rdma_func[vpp_index].rdma_wr
						(aisr_pps_reg->vd_scale_coef,
						pcoeff[i + 2]);
				for (i = 0; i < VPP_FILER_COEFS_NUM; i++) {
					cur_dev->rdma_func[vpp_index].rdma_wr
						(aisr_pps_reg->vd_scale_coef,
						pcoeff[i + 2 +
						VPP_FILER_COEFS_NUM]);
				}
			} else {
				cur_dev->rdma_func[vpp_index].rdma_wr
					(aisr_pps_reg->vd_scale_coef_idx,
					VPP_COEF_VERT_CHROMA | VPP_COEF_SEP_EN);
				for (i = 0; i < pcoeff[1]; i++)
					cur_dev->rdma_func[vpp_index].rdma_wr
						(aisr_pps_reg->vd_scale_coef,
						pcoeff[i + 2]);
			}
		}

		r1 = frame_par->VPP_vsc_endp
			- frame_par->VPP_vsc_startp;
		cur_dev->rdma_func[vpp_index].rdma_wr
			(aisr_pps_reg->vd_vsc_region12_startp, 0);

		cur_dev->rdma_func[vpp_index].rdma_wr
			(aisr_pps_reg->vd_vsc_region34_startp,
			((r1 & VPP_REGION_MASK)
			<< VPP_REGION3_BIT) |
			((r1 & VPP_REGION_MASK)
			<< VPP_REGION4_BIT));

		cur_dev->rdma_func[vpp_index].rdma_wr
			(aisr_pps_reg->vd_vsc_region4_endp, r1);

		cur_dev->rdma_func[vpp_index].rdma_wr
			(aisr_pps_reg->vd_vsc_start_phase_step,
			vpp_filter->vpp_vsc_start_phase_step);
	}
	if (aisr_en)
		aisr_sr1_nn_enable_s5(1);
	else
		aisr_sr1_nn_enable_s5(0);
}

void aisr_reshape_output_s5(u32 enable)
{
	struct vd_proc_sr_reg_s *vd_sr_reg = NULL;

	if (!cur_dev->aisr_support ||
		!cur_dev->aisr_enable)
		return;
	vd_sr_reg = &vd_proc_reg.vd_proc_sr_reg;
	if (enable) {
		WRITE_VCBUS_REG_BITS
			(vd_sr_reg->srsharp1_nn_post_top,
			0x5, 9, 4);
		WRITE_VCBUS_REG_BITS
			(vd_sr_reg->srsharp1_demo_mode_window_ctrl0,
			0xa, 12, 4);
		WRITE_VCBUS_REG_BITS
			(vd_sr_reg->srsharp1_demo_mode_window_ctrl0,
			0x2, 28, 2);
		WRITE_VCBUS_REG_BITS
			(vd_sr_reg->srsharp1_sharp_sr2_ctrl,
			0x0, 1, 1);
	} else {
		WRITE_VCBUS_REG_BITS
			(vd_sr_reg->srsharp1_nn_post_top,
			0x0, 9, 4);
		WRITE_VCBUS_REG_BITS
			(vd_sr_reg->srsharp1_demo_mode_window_ctrl0,
			0x4, 12, 4);
		WRITE_VCBUS_REG_BITS
			(vd_sr_reg->srsharp1_demo_mode_window_ctrl0,
			0x0, 28, 2);
		}
}

void aisr_demo_enable_s5(void)
{
	struct vd_proc_sr_reg_s *vd_sr_reg = NULL;

	if (!cur_dev->aisr_support)
		return;
	vd_sr_reg = &vd_proc_reg.vd_proc_sr_reg;

	/* reshape and aisr demo is mutex */
	WRITE_VCBUS_REG_BITS
		(vd_sr_reg->srsharp1_demo_mode_window_ctrl0,
		cur_dev->aisr_demo_en, 29, 1);
	WRITE_VCBUS_REG_BITS
		(vd_sr_reg->srsharp1_demo_mode_window_ctrl0,
		1, 12, 4);
}

void aisr_demo_axis_set_s5(void)
{
	struct vd_proc_sr_reg_s *vd_sr_reg = NULL;

	if (!cur_dev->aisr_support)
		return;
	vd_sr_reg = &vd_proc_reg.vd_proc_sr_reg;

	WRITE_VCBUS_REG_BITS
		(vd_sr_reg->srsharp1_demo_mode_window_ctrl0,
		cur_dev->aisr_demo_xstart, 16, 12);
	WRITE_VCBUS_REG_BITS
		(vd_sr_reg->srsharp1_demo_mode_window_ctrl0,
		cur_dev->aisr_demo_xend, 0, 12);
	WRITE_VCBUS_REG_BITS
		(vd_sr_reg->srsharp1_demo_mode_window_ctrl1,
		cur_dev->aisr_demo_ystart, 16, 12);
	WRITE_VCBUS_REG_BITS
		(vd_sr_reg->srsharp1_demo_mode_window_ctrl1,
		cur_dev->aisr_demo_yend, 0, 12);
}

void aisr_reshape_addr_set_s5(struct video_layer_s *layer,
				  struct aisr_setting_s *aisr_mif_setting)
{
	ulong baddr[4][4];
	ulong baddr_base = aisr_mif_setting->phy_addr;
	u32 aisr_stride, aisr_align_h;
	int i, j;
	struct vd_aisr_reshape_reg_s *aisr_reshape_reg;

	if (!is_layer_aisr_supported(layer))
		return;

	aisr_reshape_reg = &vd_proc_reg.aisr_reshape_reg;
	if (!aisr_mif_setting->aisr_enable) {
		cur_dev->rdma_func[VPP0].rdma_wr_bits
			(aisr_reshape_reg->aisr_post_ctrl,
			0,
			31, 1);
		cur_dev->scaler_sep_coef_en = 0;
		cur_dev->aisr_enable = 0;
		cur_dev->pps_auto_calc = 0;
		video_info_change_status &= ~VIDEO_AISR_FRAME_EVENT;
		return;
	}
	cur_dev->scaler_sep_coef_en = 1;
	cur_dev->aisr_enable = 1;
	cur_dev->pps_auto_calc = 1;
	aisr_stride = aisr_mif_setting->buf_align_w;
	aisr_align_h = aisr_mif_setting->buf_align_h;
	video_info_change_status |= VIDEO_AISR_FRAME_EVENT;

	for (i = 0; i < 4; i++)
		for (j = 0; j < 4; j++)
			baddr[i][j] = 0;
	if (glayer_info[0].reverse ||
	    glayer_info[0].mirror == H_MIRROR) {
		switch (aisr_mif_setting->in_ratio) {
		case MODE_2X2:
			baddr[0][0] = baddr_base + aisr_stride * aisr_align_h;
			baddr[0][1] = baddr_base;
			baddr[0][2] = 0;
			baddr[0][3] = 0;
			baddr[1][0] = baddr_base + aisr_stride * aisr_align_h * 3;
			baddr[1][1] = baddr_base + aisr_stride * aisr_align_h * 2;
			baddr[1][2] = 0;
			baddr[1][3] = 0;
			baddr[2][0] = 0;
			baddr[2][1] = 0;
			baddr[2][2] = 0;
			baddr[2][3] = 0;
			baddr[3][0] = 0;
			baddr[3][1] = 0;
			baddr[3][2] = 0;
			baddr[3][3] = 0;
			break;
		case MODE_3X3:
			baddr[0][0] = baddr_base + aisr_stride * aisr_align_h * 2;
			baddr[0][1] = baddr_base + aisr_stride * aisr_align_h;
			baddr[0][2] = baddr_base;
			baddr[0][3] = 0;
			baddr[1][0] = baddr_base + aisr_stride * aisr_align_h * 5;
			baddr[1][1] = baddr_base + aisr_stride * aisr_align_h * 4;
			baddr[1][2] = baddr_base + aisr_stride * aisr_align_h * 3;
			baddr[1][3] = 0;
			baddr[2][0] = baddr_base + aisr_stride * aisr_align_h * 8;
			baddr[2][1] = baddr_base + aisr_stride * aisr_align_h * 7;
			baddr[2][2] = baddr_base + aisr_stride * aisr_align_h * 6;
			baddr[2][3] = 0;
			baddr[3][0] = 0;
			baddr[3][1] = 0;
			baddr[3][2] = 0;
			baddr[3][3] = 0;
			break;
		case MODE_4X4:
			baddr[0][0] = baddr_base + aisr_stride * aisr_align_h * 3;
			baddr[0][1] = baddr_base + aisr_stride * aisr_align_h * 2;
			baddr[0][2] = baddr_base + aisr_stride * aisr_align_h;
			baddr[0][3] = baddr_base;
			baddr[1][0] = baddr_base + aisr_stride * aisr_align_h * 7;
			baddr[1][1] = baddr_base + aisr_stride * aisr_align_h * 6;
			baddr[1][2] = baddr_base + aisr_stride * aisr_align_h * 5;
			baddr[1][3] = baddr_base + aisr_stride * aisr_align_h * 4;
			baddr[2][0] = baddr_base + aisr_stride * aisr_align_h * 11;
			baddr[2][1] = baddr_base + aisr_stride * aisr_align_h * 10;
			baddr[2][2] = baddr_base + aisr_stride * aisr_align_h * 9;
			baddr[2][3] = baddr_base + aisr_stride * aisr_align_h * 8;
			baddr[3][0] = baddr_base + aisr_stride * aisr_align_h * 15;
			baddr[3][1] = baddr_base + aisr_stride * aisr_align_h * 14;
			baddr[3][2] = baddr_base + aisr_stride * aisr_align_h * 13;
			baddr[3][3] = baddr_base + aisr_stride * aisr_align_h * 12;
			break;
		default:
			pr_err("invalid mode=%d\n", aisr_mif_setting->in_ratio);
			break;
		}
	} else {
		switch (aisr_mif_setting->in_ratio) {
		case MODE_2X2:
				baddr[0][0] = baddr_base;
				baddr[0][1] = baddr_base + aisr_stride * aisr_align_h;
				baddr[0][2] = 0;
				baddr[0][3] = 0;
				baddr[1][0] = baddr_base + aisr_stride * aisr_align_h * 2;
				baddr[1][1] = baddr_base + aisr_stride * aisr_align_h * 3;
				baddr[1][2] = 0;
				baddr[1][3] = 0;
				baddr[2][0] = 0;
				baddr[2][1] = 0;
				baddr[2][2] = 0;
				baddr[2][3] = 0;
				baddr[3][0] = 0;
				baddr[3][1] = 0;
				baddr[3][2] = 0;
				baddr[3][3] = 0;
				break;
		case MODE_3X3:
				baddr[0][0] = baddr_base;
				baddr[0][1] = baddr_base + aisr_stride * aisr_align_h;
				baddr[0][2] = baddr_base + aisr_stride * aisr_align_h * 2;
				baddr[0][3] = 0;
				baddr[1][0] = baddr_base + aisr_stride * aisr_align_h * 3;
				baddr[1][1] = baddr_base + aisr_stride * aisr_align_h * 4;
				baddr[1][2] = baddr_base + aisr_stride * aisr_align_h * 5;
				baddr[1][3] = 0;
				baddr[2][0] = baddr_base + aisr_stride * aisr_align_h * 6;
				baddr[2][1] = baddr_base + aisr_stride * aisr_align_h * 7;
				baddr[2][2] = baddr_base + aisr_stride * aisr_align_h * 8;
				baddr[2][3] = 0;
				baddr[3][0] = 0;
				baddr[3][1] = 0;
				baddr[3][2] = 0;
				baddr[3][3] = 0;
				break;
		case MODE_4X4:
				baddr[0][0] = baddr_base;
				baddr[0][1] = baddr_base + aisr_stride * aisr_align_h;
				baddr[0][2] = baddr_base + aisr_stride * aisr_align_h * 2;
				baddr[0][3] = baddr_base + aisr_stride * aisr_align_h * 3;
				baddr[1][0] = baddr_base + aisr_stride * aisr_align_h * 4;
				baddr[1][1] = baddr_base + aisr_stride * aisr_align_h * 5;
				baddr[1][2] = baddr_base + aisr_stride * aisr_align_h * 6;
				baddr[1][3] = baddr_base + aisr_stride * aisr_align_h * 7;
				baddr[2][0] = baddr_base + aisr_stride * aisr_align_h * 8;
				baddr[2][1] = baddr_base + aisr_stride * aisr_align_h * 9;
				baddr[2][2] = baddr_base + aisr_stride * aisr_align_h * 10;
				baddr[2][3] = baddr_base + aisr_stride * aisr_align_h * 11;
				baddr[3][0] = baddr_base + aisr_stride * aisr_align_h * 12;
				baddr[3][1] = baddr_base + aisr_stride * aisr_align_h * 13;
				baddr[3][2] = baddr_base + aisr_stride * aisr_align_h * 14;
				baddr[3][3] = baddr_base + aisr_stride * aisr_align_h * 15;
				break;
		default:
			pr_err("invalid mode=%d\n", aisr_mif_setting->in_ratio);
			break;
		}
	}
	cur_dev->rdma_func[VPP0].rdma_wr
		(aisr_reshape_reg->aisr_reshape_baddr00,
		baddr[0][0] >> 4);
	cur_dev->rdma_func[VPP0].rdma_wr
		(aisr_reshape_reg->aisr_reshape_baddr01,
		baddr[0][1] >> 4);
	cur_dev->rdma_func[VPP0].rdma_wr
		(aisr_reshape_reg->aisr_reshape_baddr02,
		baddr[0][2] >> 4);
	cur_dev->rdma_func[VPP0].rdma_wr
		(aisr_reshape_reg->aisr_reshape_baddr03,
		baddr[0][3] >> 4);
	cur_dev->rdma_func[VPP0].rdma_wr
		(aisr_reshape_reg->aisr_reshape_baddr10,
		baddr[1][0] >> 4);
	cur_dev->rdma_func[VPP0].rdma_wr
		(aisr_reshape_reg->aisr_reshape_baddr11,
		baddr[1][1] >> 4);
	cur_dev->rdma_func[VPP0].rdma_wr
		(aisr_reshape_reg->aisr_reshape_baddr12,
		baddr[1][2] >> 4);
	cur_dev->rdma_func[VPP0].rdma_wr
		(aisr_reshape_reg->aisr_reshape_baddr13,
		baddr[1][3] >> 4);
	cur_dev->rdma_func[VPP0].rdma_wr
		(aisr_reshape_reg->aisr_reshape_baddr20,
		baddr[2][0] >> 4);
	cur_dev->rdma_func[VPP0].rdma_wr
		(aisr_reshape_reg->aisr_reshape_baddr21,
		baddr[2][1] >> 4);
	cur_dev->rdma_func[VPP0].rdma_wr
		(aisr_reshape_reg->aisr_reshape_baddr22,
		baddr[2][2] >> 4);
	cur_dev->rdma_func[VPP0].rdma_wr
		(aisr_reshape_reg->aisr_reshape_baddr23,
		baddr[2][3] >> 4);
	cur_dev->rdma_func[VPP0].rdma_wr
		(aisr_reshape_reg->aisr_reshape_baddr30,
		baddr[3][0] >> 4);
	cur_dev->rdma_func[VPP0].rdma_wr
		(aisr_reshape_reg->aisr_reshape_baddr31,
		baddr[3][1] >> 4);
	cur_dev->rdma_func[VPP0].rdma_wr
		(aisr_reshape_reg->aisr_reshape_baddr32,
		baddr[3][2] >> 4);
	cur_dev->rdma_func[VPP0].rdma_wr
		(aisr_reshape_reg->aisr_reshape_baddr33,
		baddr[3][3] >> 4);
}

void aisr_reshape_cfg_s5(struct video_layer_s *layer,
		     struct aisr_setting_s *aisr_mif_setting)
{
	u32 aisr_hsize = 0;
	u32 aisr_vsize = 0;
	u32 vscale_skip_count;
	int reg_hloop_num, reg_vloop_num;
	int r = 0;
	struct vd_aisr_reshape_reg_s *aisr_reshape_reg;

	if (!is_layer_aisr_supported(layer) ||
	    !aisr_mif_setting)
		return;
	if (!aisr_mif_setting->aisr_enable)
		return;
	aisr_reshape_reg = &vd_proc_reg.aisr_reshape_reg;
	vscale_skip_count = aisr_mif_setting->vscale_skip_count;
	aisr_hsize = aisr_mif_setting->x_end - aisr_mif_setting->x_start + 1;
	aisr_vsize = aisr_mif_setting->y_end - aisr_mif_setting->y_start + 1;
	aisr_hsize *= (aisr_mif_setting->in_ratio + 1);
	aisr_vsize *= (aisr_mif_setting->in_ratio + 1 -
		vscale_skip_count);
	reg_hloop_num = aisr_mif_setting->in_ratio;
	reg_vloop_num = aisr_mif_setting->in_ratio -
		vscale_skip_count;
	if (reg_vloop_num < 0)
		reg_vloop_num = 0;
	cur_dev->rdma_func[VPP0].rdma_wr_bits
		(aisr_reshape_reg->aisr_reshape_ctrl0,
		aisr_mif_setting->swap_64bit, 7, 1);
	cur_dev->rdma_func[VPP0].rdma_wr_bits
		(aisr_reshape_reg->aisr_reshape_ctrl0,
		aisr_mif_setting->little_endian, 6, 1);
	cur_dev->rdma_func[VPP0].rdma_wr_bits
		(aisr_reshape_reg->aisr_reshape_ctrl0,
		1, 0, 3);
	if (aisr_mif_setting->di_hf_y_reverse) {
		if (glayer_info[0].reverse)
			r |= (1 << 0) | (0 << 1);
		else if (glayer_info[0].mirror == H_MIRROR)
			r |= (1 << 0) | (0 << 1);
		else if (glayer_info[0].mirror == V_MIRROR)
			pr_info("not supported v mirror, please used di hf y reverse\n");
	} else {
		if (glayer_info[0].reverse)
			r |= (1 << 0) | (1 << 1);
		else if (glayer_info[0].mirror == H_MIRROR)
			r |= (1 << 0) | (0 << 1);
		else if (glayer_info[0].mirror == V_MIRROR)
			r |= (0 << 0) | (1 << 1);
	}
	cur_dev->rdma_func[VPP0].rdma_wr_bits
		(aisr_reshape_reg->aisr_reshape_ctrl0,
		r, 4, 2);
	/* stride set */
	cur_dev->rdma_func[VPP0].rdma_wr_bits
		(aisr_reshape_reg->aisr_reshape_ctrl1,
		((aisr_mif_setting->src_align_w * 8 + 511) >> 9) << 2,
		0, 13);
	/* h v skip set */
	cur_dev->rdma_func[VPP0].rdma_wr_bits
		(aisr_reshape_reg->aisr_reshape_ctrl1,
		reg_hloop_num << 4 |
		reg_vloop_num,
		20, 7);
	/* scope x, y set */
	cur_dev->rdma_func[VPP0].rdma_wr
		(aisr_reshape_reg->aisr_reshape_scope_x,
		(aisr_mif_setting->x_end << 16) |
		aisr_mif_setting->x_start);
	cur_dev->rdma_func[VPP0].rdma_wr
		(aisr_reshape_reg->aisr_reshape_scope_y,
		(aisr_mif_setting->y_end << 16) |
		aisr_mif_setting->y_start);

	cur_dev->rdma_func[VPP0].rdma_wr
		(aisr_reshape_reg->aisr_post_size,
		(aisr_vsize << 16) |
		aisr_hsize);
	cur_dev->rdma_func[VPP0].rdma_wr_bits
		(aisr_reshape_reg->aisr_post_ctrl,
		aisr_mif_setting->aisr_enable,
		31, 1);
}

void vd_blend_setting_s5(struct video_layer_s *layer, struct blend_setting_s *setting)
{
}

#ifdef CONFIG_AMLOGIC_MEDIA_LUT_DMA
static void fgrain_set_config_s5(struct video_layer_s *layer,
				  struct fgrain_setting_s *setting, u8 vpp_index)
{
	u32 reg_fgrain_glb_en = 1 << 0;
	u32 reg_fgrain_loc_en = 1 << 1;
	u32 reg_block_mode = 1 << 2;
	u32 reg_rev_mode = 0 << 4;
	u32 reg_comp_bits = 0 << 6;
	/* unsigned , RW, default = 0:8bits; 1:10bits, else 12 bits */
	u32 reg_fmt_mode = 2 << 8;
	/* unsigned , RW, default =  0:444; 1:422; 2:420; 3:reserved */
	u32 reg_last_in_mode = 0;
	/* for none-afbc, it need set to 1,  default it is 0 */
	u32 reg_fgrain_ext_imode = 1;
	/*  unsigned , RW, default = 0 to indicate the
	 *input data is *4 in 8bit mode
	 */
	u32 layer_index = 0;
	struct vd_fg_reg_s *fg_reg;

	if (!glayer_info[layer->layer_id].fgrain_support)
		return;
	if (!setting)
		return;
	layer_index = setting->id;
	if (debug_flag_s5 & DEBUG_FG)
		pr_info("%s: layer_index=%d\n",
			__func__, layer_index);

	fg_reg = &vd_proc_reg.vd_fg_reg[layer_index];
	reg_block_mode = setting->afbc << 2;
	reg_rev_mode = setting->reverse << 4;
	reg_comp_bits = setting->bitdepth << 6;
	reg_fmt_mode = setting->fmt_mode << 8;
	reg_last_in_mode = setting->last_in_mode;

	cur_dev->rdma_func[vpp_index].rdma_wr_bits(fg_reg->fgrain_ctrl,
			       reg_fgrain_glb_en |
			       reg_fgrain_loc_en |
			       reg_block_mode |
			       reg_rev_mode |
			       reg_comp_bits |
			       reg_fmt_mode,
			       0, 10);
	cur_dev->rdma_func[vpp_index].rdma_wr_bits(fg_reg->fgrain_ctrl,
			       reg_last_in_mode, 14, 1);
	cur_dev->rdma_func[vpp_index].rdma_wr_bits(fg_reg->fgrain_ctrl,
			       reg_fgrain_ext_imode, 16, 1);
}

static void fgrain_start_s5(struct video_layer_s *layer, u8 vpp_index)
{
	u32 reg_fgrain_glb_en = 1 << 0;
	u32 reg_fgrain_loc_en = 1 << 1;
	struct vd_fg_reg_s *fg_reg;
	u8 layer_id = layer->layer_id;
	u8 layer_index = 0;

	if (!glayer_info[layer_id].fgrain_support)
		return;
	if (glayer_info[layer_id].fgrain_start)
		return;
	if (layer_id >= MAX_VD_CHAN_S5)
		return;
	if (layer_id != 0)
		layer_index = layer_id + SLICE_NUM - 1;
	else
		layer_index = layer_id;
	if (debug_flag_s5 & DEBUG_FG)
		pr_info("%s: layer_index=%d\n",
			__func__, layer_index);

	fg_reg = &vd_proc_reg.vd_fg_reg[layer_index];
	cur_dev->rdma_func[vpp_index].rdma_wr_bits(fg_reg->fgrain_ctrl,
	       reg_fgrain_glb_en |
	       reg_fgrain_loc_en,
	       0, 2);
	glayer_info[layer_id].fgrain_start = true;
	glayer_info[layer_id].fgrain_force_update = true;
}

static void fgrain_slice_start_s5(struct video_layer_s *layer, u8 vpp_index, u8 slice)
{
	u32 reg_fgrain_glb_en = 1 << 0;
	u32 reg_fgrain_loc_en = 1 << 1;
	struct vd_fg_reg_s *fg_reg;
	u8 layer_id = layer->layer_id;

	if (!glayer_info[layer_id].fgrain_support)
		return;
	if (glayer_info[layer_id].fgrain_start)
		return;
	if (layer->layer_id != 0 || slice >= SLICE_NUM)
		return;
	if (debug_flag_s5 & DEBUG_FG)
		pr_info("%s: layer_index=%d\n",
			__func__, slice);

	fg_reg = &vd_proc_reg.vd_fg_reg[slice];
	cur_dev->rdma_func[vpp_index].rdma_wr_bits(fg_reg->fgrain_ctrl,
		reg_fgrain_glb_en |
		reg_fgrain_loc_en,
		0, 2);
	glayer_info[layer_id].fgrain_start = true;
	glayer_info[layer_id].fgrain_force_update = true;
}

static void fgrain_stop_s5(struct video_layer_s *layer, u8 vpp_index)
{
	u32 reg_fgrain_glb_en = 1 << 0;
	u32 reg_fgrain_loc_en = 0 << 1;
	struct vd_fg_reg_s *fg_reg;
	u8 layer_id = layer->layer_id;
	u8 layer_index = 0;

	if (!glayer_info[layer_id].fgrain_support)
		return;
	if (!glayer_info[layer_id].fgrain_start)
		return;
	if (layer_id >= MAX_VD_CHAN_S5)
		return;
	if (layer_id != 0)
		layer_index = layer_id + SLICE_NUM - 1;
	else
		layer_index = layer_id;
	if (debug_flag_s5 & DEBUG_FG)
		pr_info("%s: layer_index=%d\n",
			__func__, layer_index);

	fg_reg = &vd_proc_reg.vd_fg_reg[layer_index];
	cur_dev->rdma_func[vpp_index].rdma_wr_bits(fg_reg->fgrain_ctrl,
		   reg_fgrain_glb_en |
		   reg_fgrain_loc_en,
		   0, 2);
	glayer_info[layer_id].fgrain_start = false;
}

static void fgrain_slice_stop_s5(struct video_layer_s *layer,
	u8 vpp_index, u8 slice)
{
	u32 reg_fgrain_glb_en = 1 << 0;
	u32 reg_fgrain_loc_en = 0 << 1;
	struct vd_fg_reg_s *fg_reg;
	u8 layer_id = layer->layer_id;

	if (!glayer_info[layer_id].fgrain_support)
		return;
	if (!glayer_info[layer_id].fgrain_start)
		return;
	if (layer->layer_id != 0 || slice > SLICE_NUM)
		return;
	if (debug_flag_s5 & DEBUG_FG)
		pr_info("%s: layer_index=%d\n",
			__func__, slice);

	fg_reg = &vd_proc_reg.vd_fg_reg[slice];
	cur_dev->rdma_func[vpp_index].rdma_wr_bits(fg_reg->fgrain_ctrl,
		reg_fgrain_glb_en |
		reg_fgrain_loc_en,
		0, 2);
	glayer_info[layer_id].fgrain_start = false;
}

static void fgrain_set_window_s5(struct video_layer_s *layer,
			      struct fgrain_setting_s *setting,
			      u8 vpp_index)
{
	struct vd_fg_reg_s *fg_reg;

	if (setting->id > MAX_VD_LAYER_S5 - 1)
		return;
	if (debug_flag_s5 & DEBUG_FG)
		pr_info("%s: layer_index=%d\n",
			__func__, setting->id);

	fg_reg = &vd_proc_reg.vd_fg_reg[setting->id];
	cur_dev->rdma_func[vpp_index].rdma_wr(fg_reg->fgrain_win_h,
		(setting->start_x << 0) |
		(setting->end_x << 16));
	cur_dev->rdma_func[vpp_index].rdma_wr(fg_reg->fgrain_win_v,
		(setting->start_y << 0) |
		(setting->end_y << 16));
}

int fgrain_init_s5(u8 layer_id, u32 table_size)
{
	int ret = -1, i = 0;
	u32 channel = FILM_GRAIN1_VD1S0_CHAN_S5;
	struct lut_dma_set_t lut_dma_set;

	if (!glayer_info[layer_id].fgrain_support)
		return -1;
	if (layer_id == 0) {
		for (i = 0; i < SLICE_NUM; i++) {
			channel = FILM_GRAIN1_VD1S0_CHAN_S5 + i;
			lut_dma_set.channel = channel;
			lut_dma_set.dma_dir = LUT_DMA_WR;
			lut_dma_set.irq_source = VIU1_VSYNC;
			lut_dma_set.mode = LUT_DMA_MANUAL;
			lut_dma_set.table_size = table_size;
			ret = lut_dma_register(&lut_dma_set);
		}
	} else if (layer_id == 1) {
		channel = FILM_GRAIN2_CHAN_S5;
		lut_dma_set.channel = channel;
		lut_dma_set.dma_dir = LUT_DMA_WR;
		lut_dma_set.irq_source = VIU1_VSYNC;
		lut_dma_set.mode = LUT_DMA_MANUAL;
		lut_dma_set.table_size = table_size;
		ret = lut_dma_register(&lut_dma_set);
	}
	if (ret >= 0) {
		glayer_info[layer_id].lut_dma_support = 1;

	} else {
		pr_info("%s failed, fg not support\n", __func__);
		glayer_info[layer_id].lut_dma_support = 0;
	}
	return ret;
}

void fgrain_uninit_s5(u8 layer_id)
{
	int i = 0;
	u32 channel = FILM_GRAIN0_CHAN;

	if (!glayer_info[layer_id].fgrain_support)
		return;

	if (layer_id == 0) {
		for (i = 0; i < SLICE_NUM; i++) {
			channel = FILM_GRAIN1_VD1S0_CHAN_S5 + i;
			lut_dma_unregister(LUT_DMA_WR, channel);
		}
	} else if (layer_id == 1) {
		channel = FILM_GRAIN2_CHAN_S5;
		lut_dma_unregister(LUT_DMA_WR, channel);
	}
}

static int fgrain_write_s5(u32 layer_id, ulong fgs_table_addr)
{
	int table_size = FGRAIN_TBL_SIZE;
	u32 channel = 0;

	if (layer_id == 0) {
		channel = FILM_GRAIN1_VD1S0_CHAN_S5;
		lut_dma_write_phy_addr(channel,
			fgs_table_addr,
			table_size);
	} else if (layer_id == 1) {
		channel = FILM_GRAIN2_CHAN_S5;
		lut_dma_write_phy_addr(channel,
			fgs_table_addr,
			table_size);
	}
	return 0;
}

static int fgrain_slice_write_s5(u32 layer_id,
	ulong fgs_table_addr, u8 slice)
{
	int table_size = FGRAIN_TBL_SIZE;
	u32 channel = 0;

	if (layer_id == 0) {
		channel = FILM_GRAIN1_VD1S0_CHAN_S5 + slice;
		lut_dma_write_phy_addr(channel,
			fgs_table_addr,
			table_size);
	}
	return 0;
}

static int get_viu_irq_source_s5(u8 vpp_index)
{
	/* for s5 only one venc0 encp */
	return ENCP_GO_FIELD;
}

static void fgrain_slice_update_irq_source_s5(u8 layer_id,
	u8 vpp_index, u8 slice)
{
	u32 irq_source = ENCP_GO_FIELD;
	u32 channel = 0;

	/* get vpp0 irq source */
	irq_source = get_viu_irq_source_s5(vpp_index);

	if (layer_id == 0)
		channel = FILM_GRAIN1_VD1S0_CHAN_S5 + slice;
	lut_dma_update_irq_source(channel, irq_source);
}

static void fgrain_update_irq_source_s5(u8 layer_id, u8 vpp_index)
{
	u32 irq_source = ENCP_GO_FIELD;
	u32 channel = 0;

	/* get vpp0 irq source */
	irq_source = get_viu_irq_source_s5(vpp_index);

	if (layer_id == 0)
		channel = FILM_GRAIN1_VD1S0_CHAN_S5;
	else if (layer_id == 1)
		channel = FILM_GRAIN2_CHAN_S5;
	lut_dma_update_irq_source(channel, irq_source);
}

static void fgrain_slice_setting_s5(struct video_layer_s *layer,
		    struct vframe_s *vf,
		    struct fgrain_setting_s *setting)
{
	u8 vpp_index, layer_id;

	if (!vf)
		return;

	layer_id = layer->layer_id;
	if (debug_flag_s5 & DEBUG_FG)
		pr_info("%s(%d):fgrain_enable=%d, fgs_valid=%d, fgs_table_adr=%ld\n",
			__func__,
			setting->id,
			glayer_info[layer_id].fgrain_enable,
			vf->fgs_valid,
			vf->fgs_table_adr);

	if (!glayer_info[layer_id].lut_dma_support)
		return;
	vpp_index = layer->vpp_index;
	if (!setting->used || !vf->fgs_valid ||
	    !glayer_info[layer_id].fgrain_enable)
		fgrain_slice_stop_s5(layer, vpp_index, setting->id);
	if (glayer_info[layer_id].fgrain_enable) {
		if (setting->used && vf->fgs_valid &&
		    vf->fgs_table_adr) {
			fgrain_set_config_s5(layer, setting, vpp_index);
			fgrain_set_window_s5(layer, setting, vpp_index);
		}
	}
}

static void fgrain_setting_s5(struct video_layer_s *layer,
		    struct vframe_s *vf,
		    struct fgrain_setting_s *setting)
{
	u8 vpp_index, layer_id;

	if (!vf)
		return;
	layer_id = layer->layer_id;

	if (debug_flag_s5 & DEBUG_FG)
		pr_info("%s(%d):fgrain_enable=%d, fgs_valid=%d, fgs_table_adr=%ld\n",
			__func__,
			layer->layer_id,
			glayer_info[layer_id].fgrain_enable,
			vf->fgs_valid,
			vf->fgs_table_adr);

	if (!glayer_info[layer_id].lut_dma_support)
		return;
	vpp_index = layer->vpp_index;
	if (!setting->used || !vf->fgs_valid ||
	    !glayer_info[layer_id].fgrain_enable)
		fgrain_stop_s5(layer, vpp_index);
	if (glayer_info[layer_id].fgrain_enable) {
		if (setting->used && vf->fgs_valid &&
		    vf->fgs_table_adr) {
			fgrain_set_config_s5(layer, setting, vpp_index);
			fgrain_set_window_s5(layer, setting, vpp_index);
		}
	}
}

static void _vd_fgrain_setting_s5(struct video_layer_s *layer,
		    struct vframe_s *vf)
{
	u8 vpp_index, layer_id;
	int i;

	if (!vf)
		return;

	layer_id = layer->layer_id;
	if (!glayer_info[layer_id].lut_dma_support)
		return;
	vpp_index = layer->vpp_index;
	if (layer_id == 0) {
		if (get_vd1_work_mode() == VD1_1SLICES_MODE) {
			layer->fgrain_setting.id = 0;
			fgrain_setting_s5(layer, vf,
				&layer->fgrain_setting);
		} else {
			for (i = 0; i < layer->slice_num; i++) {
				if (layer->vd1s1_vd2_prebld_en &&
					layer->slice_num == 2 &&
					i == 1)
					layer->slice_fgrain_setting[i].id = SLICE_NUM;
				else
					layer->slice_fgrain_setting[i].id = i;
				fgrain_slice_setting_s5(layer, vf,
					&layer->slice_fgrain_setting[i]);
			}
		}
	} else {
		layer->fgrain_setting.id += SLICE_NUM - 1;
		fgrain_setting_s5(layer, vf,
			&layer->fgrain_setting);
	}
}

void fgrain_config_s5(struct video_layer_s *layer,
		   struct vpp_frame_par_s *frame_par,
		   struct mif_pos_s *mif_setting,
		   struct fgrain_setting_s *setting,
		   struct vframe_s *vf)
{
	u32 type;
	u8 layer_id;

	if (!vf || !mif_setting || !setting || !frame_par)
		return;
	layer_id = layer->layer_id;
	if (!glayer_info[layer_id].fgrain_support)
		return;
	if (!glayer_info[layer_id].lut_dma_support)
		return;
	type = vf->type;
	if (frame_par->nocomp)
		type &= ~VIDTYPE_COMPRESS;

	if (type & VIDTYPE_COMPRESS) {
		/* 1:afbc mode or 0: non-afbc mode  */
		setting->afbc = 1;
		/* bit[2]=0, non-afbc mode */
		setting->last_in_mode = 0;
		/* afbc copress is always 420 */
		setting->fmt_mode = 2;
		setting->used = 1;
		if (vf->bitdepth & BITDEPTH_Y10)
			setting->bitdepth = 1;
		else
			setting->bitdepth = 0;
	} else {
		setting->afbc = 0;
		setting->last_in_mode = 1;
		if (type & VIDTYPE_VIU_NV21) {
			setting->fmt_mode = 2;
			setting->used = 1;
		} else {
			/* only support 420 */
			setting->used = 0;
		}
		/* fg after mif always 10 bits */
		setting->bitdepth = 1;
	}

	if (glayer_info[layer_id].reverse)
		setting->reverse = 3;
	else
		setting->reverse = 0;

	setting->start_x = mif_setting->start_x_lines;
	setting->end_x = mif_setting->end_x_lines;
	setting->start_y = mif_setting->start_y_lines;
	setting->end_y = mif_setting->end_y_lines;
	if (setting->afbc) {
		setting->start_x = setting->start_x / 32 * 32;
		setting->end_x = setting->end_x / 32 * 32;
		setting->start_y = setting->start_y / 4 * 4;
		setting->end_y = setting->end_y / 4 * 4;
	} else {
		setting->end_x = (setting->end_x >> 1) << 1;
		setting->end_y = (setting->end_y >> 1) << 1;

		setting->start_x = (setting->start_x >> 1) << 1;
		setting->start_y = (setting->start_y >> 1) << 1;
	}
}

static void _vd_fgrain_config_s5(struct video_layer_s *layer,
		   struct vpp_frame_par_s *frame_par,
		   struct vframe_s *vf)
{
	int slice = 0;

	if (layer->layer_id == 0) {
		if (get_vd1_work_mode() == VD1_1SLICES_MODE) {
			fgrain_config_s5(layer, frame_par,
				&layer->mif_setting,
				&layer->fgrain_setting,
				vf);
		} else {
			for (slice = 0; slice < layer->slice_num; slice++) {
				fgrain_config_s5(layer, frame_par,
					&layer->slice_mif_setting[slice],
					&layer->slice_fgrain_setting[slice],
					vf);
			}
		}
	} else {
		fgrain_config_s5(layer, frame_par,
				&layer->mif_setting,
				&layer->fgrain_setting,
				vf);
	}
}

static void fgrain_slice_update_table_s5(struct video_layer_s *layer,
			 struct vframe_s *vf, u8 slice)
{
	u8 vpp_index, layer_id;

	if (!vf)
		return;

	layer_id = layer->layer_id;
	if (!glayer_info[layer_id].lut_dma_support)
		return;
	vpp_index = layer->vpp_index;
	if (!vf->fgs_valid || !glayer_info[layer_id].fgrain_enable)
		fgrain_slice_stop_s5(layer, vpp_index, slice);

	if (glayer_info[layer_id].fgrain_enable) {
		if (vf->fgs_valid && vf->fgs_table_adr) {
			fgrain_slice_start_s5(layer, vpp_index, slice);
			fgrain_slice_update_irq_source_s5(layer_id, vpp_index, slice);
			fgrain_slice_write_s5(layer_id, vf->fgs_table_adr, slice);
		}
	}
}

void fgrain_update_table_s5(struct video_layer_s *layer,
			 struct vframe_s *vf)
{
	u8 vpp_index, layer_id;
	u8 slice = 0;

	if (!vf)
		return;

	layer_id = layer->layer_id;
	if (debug_flag_s5 & DEBUG_FG)
		pr_info("%s(%d):fgrain_enable=%d, fgs_valid=%d, fgs_table_adr=%ld\n",
			__func__,
			layer->layer_id,
			glayer_info[layer_id].fgrain_enable,
			vf->fgs_valid,
			vf->fgs_table_adr);

	if (!glayer_info[layer_id].lut_dma_support)
		return;

	if (layer->layer_id == 0 && layer->slice_num > 1) {
		for (slice = 0; slice < layer->slice_num; slice++)
			fgrain_slice_update_table_s5(layer, vf, slice);
		return;
	}

	vpp_index = layer->vpp_index;
	if (!vf->fgs_valid || !glayer_info[layer_id].fgrain_enable)
		fgrain_stop_s5(layer, vpp_index);

	if (glayer_info[layer_id].fgrain_enable) {
		if (vf->fgs_valid && vf->fgs_table_adr) {
			fgrain_start_s5(layer, vpp_index);
			fgrain_update_irq_source_s5(layer_id, vpp_index);
			fgrain_write_s5(layer_id, vf->fgs_table_adr);
		}
	}
}
#endif

void vd_set_alpha_s5(struct video_layer_s *layer,
			     u32 win_en, struct pip_alpha_scpxn_s *alpha_win)
{
	int i;
	u32 alph_gen_mode = 1;
	/* 0:original, 1:  0.5 alpha 2: 0.25/0.5/0.75 */
	u32 alph_gen_byps = 0;
	u8 vpp_index, layer_id = 0;
	struct vd_pip_alpha_reg_s *vd_pip_alpha_reg = NULL;

	layer_id = layer->layer_id;
	if (layer_id >= MAX_VD_CHAN_S5)
		return;
	vd_pip_alpha_reg = &vd_proc_reg.vd_pip_alpha_reg[layer_id];
	vpp_index = layer->vpp_index;
	if (!win_en)
		alph_gen_byps = 1;
	cur_dev->rdma_func[vpp_index].rdma_wr(vd_pip_alpha_reg->vd_pip_alph_ctrl,
			  ((0 & 0x1) << 28) |
			  ((win_en & 0xffff) << 12) |
			  ((0 & 0x1ff) << 3) |
			  ((alph_gen_mode & 0x3) << 1) |
			  ((alph_gen_byps & 0x1) << 0));
	for (i = 0; i < MAX_PIP_WINDOW; i++) {
		cur_dev->rdma_func[vpp_index].rdma_wr(vd_pip_alpha_reg->vd_pip_alph_scp_h + i,
				  (alpha_win->scpxn_end_h[i] & 0x1fff) << 16 |
				  (alpha_win->scpxn_bgn_h[i] & 0x1fff));

		cur_dev->rdma_func[vpp_index].rdma_wr(vd_pip_alpha_reg->vd_pip_alph_scp_v + i,
				  (alpha_win->scpxn_end_v[i] & 0x1fff) << 16 |
				  (alpha_win->scpxn_bgn_v[i] & 0x1fff));
	}
}

static void vd1_clip_setting_s5(struct vd_proc_s *vd_proc,
	struct clip_setting_s *setting)
{
	int slice = 0;
	rdma_wr_op rdma_wr = cur_dev->rdma_func[VPP0].rdma_wr;
	struct vd_proc_slice_reg_s *vd_proc_slice_reg = NULL;
	struct vd_proc_vd1_info_s *vd_proc_vd1_info = NULL;

	pr_info("%s:\n", __func__);
	if (!setting)
		return;
	vd_proc_vd1_info = &vd_proc->vd_proc_vd1_info;
	for (slice = 0; slice < vd_proc_vd1_info->slice_num; slice++) {
		vd_proc_slice_reg = &vd_proc_reg.vd_proc_slice_reg[slice];
		rdma_wr(vd_proc_slice_reg->vd1_s0_clip_misc0,
			setting->clip_max);
		rdma_wr(vd_proc_slice_reg->vd1_s0_clip_misc1,
			setting->clip_min);
	}
}

static void vd2_clip_setting_s5(struct vd_proc_s *vd_proc,
	struct clip_setting_s *setting)
{
	if (!setting)
		return;
}

void vd_clip_setting_s5(u8 layer_id,
	struct clip_setting_s *setting)
{
	struct vd_proc_s *vd_proc = &g_vd_proc;

	if (layer_id == 0)
		vd1_clip_setting_s5(vd_proc, setting);
	else if (layer_id == 1)
		vd2_clip_setting_s5(vd_proc, setting);
}

void vpp_post_blend_update_s5(const struct vinfo_s *vinfo)
{
	struct vpp_post_input_s *vpp_input;
	struct vpp_post_s vpp_post;

	vpp_input = get_vpp_input_info();
	if (debug_flag_s5 & DEBUG_VPP_POST)
		pr_info("%s,slice_num=%d, din_hsize[0]=%d, %d, din[1]:%d, %d, bld_out =%d, %d\n",
			__func__,
			vpp_input->slice_num,
			vpp_input->din_hsize[0],
			vpp_input->din_vsize[0],
			vpp_input->din_hsize[1],
			vpp_input->din_vsize[1],
			vpp_input->bld_out_hsize,
			vpp_input->bld_out_vsize);

	vpp_post_param_set(vpp_input, &vpp_post);
	vpp_post_set(VPP0, &vpp_post);
	update_vpp_post_amdv_info(&vpp_post);
}

struct vd_proc_s *get_vd_proc_info(void)
{
	return &g_vd_proc;
}

void set_video_slice_policy(struct video_layer_s *layer,
	struct vframe_s *vf)
{
	u32 src_width = 0;
	u32 src_height = 0;
	u32 slice_num = 1, pi_en = 0;
	u32 vd1s1_vd2_prebld_en = 0;
	const struct vinfo_s *vinfo = get_current_vinfo();

	if (cur_dev->display_module != S5_DISPLAY_MODULE)
		return;
	/* check input */
	if (vf->type & VIDTYPE_COMPRESS) {
		src_width = vf->compWidth;
		src_height = vf->compHeight;
	} else {
		src_width = vf->width;
		src_height = vf->height;
	}
	update_vd_src_info(layer->layer_id,
		src_width, src_height, vf->compWidth, vf->compHeight);
	if (layer->layer_id == 0) {
		/* check output */
		if (vinfo) {
			/* output: (4k-8k], input <= 4k */
			if ((vinfo->width > 4096 && vinfo->height > 2160) &&
				(src_width <= 4096 && src_height <= 2160)) {
				pi_en = 1;
			/* 4k 120hz */
			} else if (vinfo->width > 1920 && vinfo->height > 1080 &&
				(vinfo->sync_duration_num /
			    vinfo->sync_duration_den > 60)) {
				slice_num = 2;
				//if dv enable, vd1s1_vd2_prebld_en = 1;
			} else {
				slice_num = 1;
			}
		}
		if (src_width > 4096 && src_height > 2160)
			/* input: (4k-8k] */
			slice_num = 4;
		layer->slice_num = slice_num;
		layer->pi_enable = pi_en;
		layer->vd1s1_vd2_prebld_en = vd1s1_vd2_prebld_en;
	} else {
		/* check output */
		if (vinfo) {
			/* output: (4k-8k], input <= 4k */
			if (vinfo->width > 4096 && vinfo->height > 2160)
				pi_en = 1;
		}
		slice_num = 1;
		layer->slice_num = slice_num;
		layer->pi_enable = pi_en;
	}
	if (g_slice_num != 0xff)
		layer->slice_num = g_slice_num;
	if (pi_enable != 0xff)
		layer->pi_enable = pi_enable;
	if (g_vd1s1_vd2_prebld_en != 0xff)
		layer->vd1s1_vd2_prebld_en = g_vd1s1_vd2_prebld_en;
}

int video_hw_init_s5(void)
{
	//u32 cur_hold_line;
	//struct vpu_dev_s *arb_vpu_dev;
	int i;
#ifdef CONFIG_AMLOGIC_MEDIA_SECURITY
	void *video_secure_op[VPP_TOP_MAX] = {VSYNC_WR_MPEG_REG_BITS,
					       VSYNC_WR_MPEG_REG_BITS_VPP1,
					       VSYNC_WR_MPEG_REG_BITS_VPP2};
#endif
	WRITE_VCBUS_REG_BITS
		(S5_VPP_OFIFO_SIZE, vpp_ofifo_size_s5,
		VPP_OFIFO_SIZE_BIT, VPP_OFIFO_SIZE_WID);

	/* postblend: VPP_POST_BLEND_BLEND_DUMMY_DATA */
	/* vd_blend : VPP_VD_BLEND_DUMMY_ALPHA */
	/* vd_preblend: VPP_VD_PRE_BLEND_DUMMY_ALPHA */
	/* black 8bit */
	WRITE_VCBUS_REG(S5_VPP_POST_BLEND_BLEND_DUMMY_DATA, 0x8080);
	WRITE_VCBUS_REG(VPP_VD_BLEND_DUMMY_ALPHA, 0x100);
	WRITE_VCBUS_REG(VPP_VD_PRE_BLEND_DUMMY_ALPHA, 0x100);

	/*disable sr default when power up*/
	WRITE_VCBUS_REG(VD_PROC_SR0_CTRL, 0);
	WRITE_VCBUS_REG(VD_PROC_SR1_CTRL, 0);
	/* disable aisr_sr1_nn func */
	if (cur_dev->aisr_support)
		aisr_sr1_nn_enable_s5(0);
	/* VD_PROC_BYPASS_CTRL default setting */
	/* should not bypass ve, it means connect preblend and ve */
	/* default bypass preblend */
	WRITE_VCBUS_REG(VD_PROC_BYPASS_CTRL, 0x01);
	/* hold line setting: todo */
	/* pre vscaler default set, conflict with ve */
	WRITE_VCBUS_REG(VPP_SLICE1_DNLP_CTRL_01, 0x1fff00);
	WRITE_VCBUS_REG(VPP_SLICE2_DNLP_CTRL_01, 0x1fff00);
	WRITE_VCBUS_REG(VPP_SLICE3_DNLP_CTRL_01, 0x1fff00);
	/* Temp force set dmc：todo */
#ifdef CONFIG_AMLOGIC_MEDIA_LUT_DMA
	for (i = 0; i < MAX_VD_CHAN_S5; i++) {
		if (glayer_info[i].fgrain_support)
			fgrain_init_s5(i, FGRAIN_TBL_SIZE);
	}
#endif
#ifdef CONFIG_AMLOGIC_MEDIA_SECURITY
	secure_register(VIDEO_MODULE, 0, video_secure_op, vpp_secure_cb);
#endif
	return 0;
}

int video_early_init_s5(struct amvideo_device_data_s *p_amvideo)
{
	int r = 0, i = 0;

	/* check super scaler support status */
	vpp_sr_init_s5(p_amvideo);
	/* adaptive config bypass ratio */
	vpp_bypass_ratio_config();

	memset(vd_layer, 0, sizeof(vd_layer));
	memset(vd_layer_vpp, 0, sizeof(vd_layer_vpp));
	memset(&g_vd_proc, 0x0, sizeof(struct vd_proc_s));

	/* only enable vd1 as default */
	vd_layer[0].global_output = 1;
	vd_layer[0].misc_reg_offt = 0 + cur_dev->vpp_off;
	vd_layer[1].misc_reg_offt = 0 + cur_dev->vpp_off;
	vd_layer[2].misc_reg_offt = 0 + cur_dev->vpp_off;
	vd_layer[0].dummy_alpha = 0x7fffffff;
	cur_dev->mif_linear = p_amvideo->mif_linear;
	cur_dev->display_module = p_amvideo->display_module;
	cur_dev->max_vd_layers = p_amvideo->max_vd_layers;
	cur_dev->vd2_independ_blend_ctrl =
		p_amvideo->dev_property.vd2_independ_blend_ctrl;
	cur_dev->aisr_support = p_amvideo->dev_property.aisr_support;
	cur_dev->di_hf_y_reverse = p_amvideo->dev_property.di_hf_y_reverse;
	cur_dev->sr_in_size = p_amvideo->dev_property.sr_in_size;
	if (cur_dev->aisr_support)
		cur_dev->pps_auto_calc = 1;

	for (i = 0; i < cur_dev->max_vd_layers; i++) {
		vd_layer[i].layer_id = i;
		vd_layer[i].cur_canvas_id = 0;
#ifdef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
		vd_layer[i].next_canvas_id = 1;
#endif
		/* vd_layer[i].global_output = 1; */
		vd_layer[i].keep_frame_id = 0xff;
		vd_layer[i].disable_video = VIDEO_DISABLE_FORNEXT;
		vd_layer[i].vpp_index = VPP0;

		/* clip config */
		vd_layer[i].clip_setting.id = i;
		vd_layer[i].clip_setting.misc_reg_offt = cur_dev->vpp_off;
		vd_layer[i].clip_setting.clip_max = 0x3fffffff;
		vd_layer[i].clip_setting.clip_min = 0;
		vd_layer[i].clip_setting.clip_done = true;

		vpp_disp_info_init(&glayer_info[i], i);
		//memset(&gpic_info[i], 0, sizeof(struct vframe_pic_mode_s));
		glayer_info[i].wide_mode = 1;
		glayer_info[i].zorder = reference_zorder - MAX_VD_LAYER_S5 + i;
		glayer_info[i].cur_sel_port = i;
		glayer_info[i].last_sel_port = i;
		glayer_info[i].display_path_id = VFM_PATH_DEF;
		glayer_info[i].need_no_compress = false;
		glayer_info[i].afbc_support = 0;
		if (p_amvideo->layer_support[i] == 0) {
			glayer_info[i].afbc_support = false;
			glayer_info[i].pps_support = false;
			glayer_info[i].dv_support = false;
			glayer_info[i].fgrain_support = false;
			glayer_info[i].fgrain_enable = false;
			glayer_info[i].alpha_support = false;
			hscaler_8tap_enable[i] = false;
			pre_scaler[i].pre_hscaler_ntap_enable = false;
			pre_scaler[i].pre_vscaler_ntap_enable = false;
			pre_scaler[i].pre_hscaler_ntap_set = 0xff;
			pre_scaler[i].pre_vscaler_ntap_set = 0xff;
			continue;
		}

		glayer_info[i].afbc_support =
			p_amvideo->afbc_support[i];
		glayer_info[i].pps_support =
			p_amvideo->pps_support[i];

		if (p_amvideo->dv_support)
			glayer_info[i].dv_support = true;
		else
			glayer_info[i].dv_support = false;
		if (p_amvideo->fgrain_support[i]) {
			glayer_info[i].fgrain_support = true;
			glayer_info[i].fgrain_enable = true;
		} else {
			glayer_info[i].fgrain_support = false;
			glayer_info[i].fgrain_enable = false;
		}
		vd_layer[i].layer_support = p_amvideo->layer_support[i];
		glayer_info[i].layer_support = p_amvideo->layer_support[i];
		glayer_info[i].alpha_support = p_amvideo->alpha_support[i];
		hscaler_8tap_enable[i] = has_hscaler_8tap(i);
		pre_scaler[i].force_pre_scaler = 0;
		pre_scaler[i].pre_hscaler_ntap_enable =
			has_pre_hscaler_ntap(i);
		pre_scaler[i].pre_vscaler_ntap_enable =
			has_pre_vscaler_ntap(i);
		pre_scaler[i].pre_hscaler_ntap_set = 0xff;
		pre_scaler[i].pre_vscaler_ntap_set = 0xff;
		pre_scaler[i].pre_hscaler_ntap = PRE_HSCALER_4TAP;
		if (has_pre_vscaler_ntap(i))
			pre_scaler[i].pre_vscaler_ntap = PRE_VSCALER_4TAP;
		else
			pre_scaler[i].pre_vscaler_ntap = PRE_VSCALER_2TAP;
		pre_scaler[i].pre_hscaler_rate = 1;
		pre_scaler[i].pre_vscaler_rate = 1;
		pre_scaler[i].pre_hscaler_coef_set = 0;
		pre_scaler[i].pre_vscaler_coef_set = 0;
		if (p_amvideo->src_width_max[i] != 0xff)
			glayer_info[i].src_width_max =
				p_amvideo->src_width_max[i];
		else
			glayer_info[i].src_width_max = 4096;
		if (p_amvideo->src_height_max[i] != 0xff)
			glayer_info[i].src_height_max =
				p_amvideo->src_height_max[i];
		else
			glayer_info[i].src_height_max = 2160;
		glayer_info[i].afd_enable = false;
	}

	for (i = 0; i < MAX_VD_LAYER_S5; i++) {
		memcpy(&vd_proc_reg.vd_afbc_reg[i],
			   &vd_afbc_reg_s5_array[i],
			   sizeof(struct vd_afbc_reg_s));
		memcpy(&vd_proc_reg.vd_mif_reg[i],
			   &vd_mif_reg_s5_array[i],
			   sizeof(struct vd_mif_reg_s));
		memcpy(&vd_proc_reg.vd_mif_linear_reg[i],
			  &vd_mif_linear_reg_s5_array[i],
			  sizeof(struct vd_mif_linear_reg_s));
		memcpy(&vd_proc_reg.vd_fg_reg[i],
			   &fg_reg_s5_array[i],
			   sizeof(struct vd_fg_reg_s));
	}
	for (i = 0; i < MAX_VD_LAYER_S5 + 1; i++)
		memcpy(&vd_proc_reg.vd_pps_reg[i],
			   &pps_reg_s5_array[i],
			   sizeof(struct vd_pps_reg_s));

	for (i = 0; i < SLICE_NUM; i++) {
		memcpy(&vd_proc_reg.vd_proc_slice_reg[i],
		   &vd_proc_slice_reg_s5[i],
		   sizeof(struct vd_proc_slice_reg_s));
		memcpy(&vd_proc_reg.vd1_slice_pad_size0_reg[i],
		   &vd1_slice_pad_size0_reg_s5[i],
		   sizeof(struct vd1_slice_pad_reg_s));
		memcpy(&vd_proc_reg.vd1_slice_pad_size1_reg[i],
		   &vd1_slice_pad_size1_reg_s5[i],
		   sizeof(struct vd1_slice_pad_reg_s));
	}

	for (i = 0; i < MAX_VD_CHAN_S5; i++)
		memcpy(&vd_proc_reg.vd_pip_alpha_reg[i],
		   &vd_pip_alpha_reg_s5[i],
		   sizeof(struct vd_pip_alpha_reg_s));

	memcpy(&vd_proc_reg.vd_proc_sr_reg,
	   &vd_proc_sr_reg_s5,
	   sizeof(struct vd_proc_sr_reg_s));
	memcpy(&vd_proc_reg.vd_proc_blend_reg,
	   &vd_proc_blend_reg_s5,
	   sizeof(struct vd_proc_blend_reg_s));
	memcpy(&vd_proc_reg.vd_proc_pi_reg,
	   &vd_proc_pi_reg_s5,
	   sizeof(struct vd_proc_pi_reg_s));
	memcpy(&vd_proc_reg.vd_proc_misc_reg,
	   &vd_proc_misc_reg_s5,
	   sizeof(struct vd_proc_misc_reg_s));
	memcpy(&vd_proc_reg.aisr_reshape_reg,
	   &aisr_reshape_reg_s5,
	   sizeof(struct vd_aisr_reshape_reg_s));
	memcpy(&vd_proc_reg.vd2_pre_blend_reg,
	   &vd2_pre_blend_reg_s5,
	   sizeof(struct vd2_pre_blend_reg_s));
	memcpy(&vd_proc_reg.vd2_proc_misc_reg,
	   &vd2_proc_misc_reg_s5,
	   sizeof(struct vd2_proc_misc_reg_s));

	memcpy(&vpp_post_reg.vpp_post_blend_reg,
	   &vpp_post_blend_reg_s5,
	   sizeof(struct vpp_post_blend_reg_s));
	memcpy(&vpp_post_reg.vpp_post_misc_reg,
	   &vpp_post_misc_reg_s5,
	   sizeof(struct vpp_post_misc_reg_s));

	vd_layer[0].layer_alpha = 0x100;

	/* g12a has no alpha overflow check in hardware */
	vd_layer[1].layer_alpha = 0x100;
	vd_layer[2].layer_alpha = 0x100;
	vpp_ofifo_size_s5 = p_amvideo->ofifo_size;
	memcpy(conv_lbuf_len_s5, p_amvideo->afbc_conv_lbuf_len,
	       sizeof(u32) * MAX_VD_LAYER);

	//init_vpu_work();
	int_vpu_delay_work();

	init_layer_canvas(&vd_layer[0], LAYER1_CANVAS_BASE_INDEX);
	init_layer_canvas(&vd_layer[1], LAYER2_CANVAS_BASE_INDEX);
	init_layer_canvas(&vd_layer[2], LAYER3_CANVAS_BASE_INDEX);
	/* vd_layer_vpp is for multiple vpp */
	memcpy(&vd_layer_vpp[0], &vd_layer[1], sizeof(struct video_layer_s));
	memcpy(&vd_layer_vpp[1], &vd_layer[2], sizeof(struct video_layer_s));
	return r;
}

