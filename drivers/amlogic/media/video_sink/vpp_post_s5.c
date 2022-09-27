// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * drivers/amlogic/media/video_sink/vpp_post_s5.c
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
#include "video_priv.h"
#include "video_hw_s5.h"
#include "video_reg_s5.h"
#include "vpp_post_s5.h"

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

static struct vpp_post_input_s g_vpp_input;
static struct vpp_post_input_s g_vpp_input_pre;
#define SIZE_ALIG32(frm_hsize)   ((((frm_hsize) + 31) >> 5) << 5)
#define SIZE_ALIG16(frm_hsize)   ((((frm_hsize) + 15) >> 4) << 4)
#define SIZE_ALIG8(frm_hsize)    ((((frm_hsize) + 7) >> 3) << 3)
#define SIZE_ALIG4(frm_hsize)    ((((frm_hsize) + 3) >> 2) << 2)

static u32 g_post_slice_num = 0xff;
MODULE_PARM_DESC(g_post_slice_num, "\n g_post_slice_num\n");
module_param(g_post_slice_num, uint, 0664);

static u32 get_reg_slice_vpost(int reg_addr, int slice_idx)
{
	u32 reg_offset;
	u32 reg_addr_tmp;

	reg_offset = slice_idx == 0 ? 0 :
		slice_idx == 1 ? 0x100 :
		slice_idx == 2 ? 0x700 : 0x1900;
	reg_addr_tmp = reg_addr + reg_offset;
	return reg_addr_tmp;
}

static void dump_vpp_blend_reg(void)
{
	u32 reg_addr, reg_val = 0;
	struct vpp_post_blend_reg_s *vpp_post_blend_reg = NULL;

	vpp_post_blend_reg = &vpp_post_reg.vpp_post_blend_reg;
	pr_info("vpp blend regs:\n");
	reg_addr = vpp_post_blend_reg->vpp_osd1_bld_h_scope;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = vpp_post_blend_reg->vpp_osd1_bld_v_scope;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = vpp_post_blend_reg->vpp_osd2_bld_h_scope;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = vpp_post_blend_reg->vpp_osd2_bld_v_scope;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);

	reg_addr = vpp_post_blend_reg->vpp_postblend_vd1_h_start_end;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = vpp_post_blend_reg->vpp_postblend_vd1_v_start_end;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = vpp_post_blend_reg->vpp_postblend_vd2_h_start_end;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = vpp_post_blend_reg->vpp_postblend_vd2_v_start_end;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = vpp_post_blend_reg->vpp_postblend_vd3_h_start_end;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = vpp_post_blend_reg->vpp_postblend_vd3_v_start_end;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = vpp_post_blend_reg->vpp_postblnd_h_v_size;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = vpp_post_blend_reg->vpp_post_blend_blend_dummy_data;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = vpp_post_blend_reg->vpp_post_blend_dummy_alpha;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = vpp_post_blend_reg->vpp_post_blend_dummy_alpha1;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);

	reg_addr = vpp_post_blend_reg->vd1_blend_src_ctrl;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = vpp_post_blend_reg->vd2_blend_src_ctrl;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = vpp_post_blend_reg->vd3_blend_src_ctrl;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = vpp_post_blend_reg->osd1_blend_src_ctrl;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = vpp_post_blend_reg->osd2_blend_src_ctrl;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = vpp_post_blend_reg->vd1_postblend_alpha;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = vpp_post_blend_reg->vd2_postblend_alpha;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = vpp_post_blend_reg->vd3_postblend_alpha;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = vpp_post_blend_reg->vpp_postblnd_ctrl;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
}

static void dump_vpp_post_misc_reg(void)
{
	int i;
	u32 reg_addr, reg_val = 0;
	struct vpp_post_misc_reg_s *vpp_post_misc_reg = NULL;

	vpp_post_misc_reg = &vpp_post_reg.vpp_post_misc_reg;
	pr_info("vpp post misc regs:\n");
	reg_addr = vpp_post_misc_reg->vpp_postblnd_ctrl;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = vpp_post_misc_reg->vpp_obuf_ram_ctrl;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = vpp_post_misc_reg->vpp_4p4s_ctrl;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = vpp_post_misc_reg->vpp_4s4p_ctrl;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = vpp_post_misc_reg->vpp_post_vd1_win_cut_ctrl;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = vpp_post_misc_reg->vpp_post_win_cut_ctrl;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = vpp_post_misc_reg->vpp_post_pad_hsize;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	reg_addr = vpp_post_misc_reg->vpp_post_pad_ctrl;
	reg_val = READ_VCBUS_REG(reg_addr);
	pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
	for (i = 0; i < SLICE_NUM; i++) {
		reg_addr = vpp_post_misc_reg->vpp_out_h_v_size;
		reg_addr = get_reg_slice_vpost(reg_addr, i);
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
		   reg_addr, reg_val);
		reg_addr = vpp_post_misc_reg->vpp_ofifo_size;
		reg_addr = get_reg_slice_vpost(reg_addr, i);
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
		reg_addr = vpp_post_misc_reg->vpp_slc_deal_ctrl;
		reg_addr = get_reg_slice_vpost(reg_addr, i);
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
		reg_addr = vpp_post_misc_reg->vpp_hwin_size;
		reg_addr = get_reg_slice_vpost(reg_addr, i);
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
		reg_addr = vpp_post_misc_reg->vpp_align_fifo_size;
		reg_addr = get_reg_slice_vpost(reg_addr, i);
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
	}
}

void dump_vpp_post_reg(void)
{
	dump_vpp_blend_reg();
	dump_vpp_post_misc_reg();
}

static void wr_slice_vpost(int reg_addr, int val, int slice_idx)
{
	rdma_wr_op rdma_wr = cur_dev->rdma_func[VPP0].rdma_wr;
	u32 reg_offset;
	u32 reg_addr_tmp;

	reg_offset = slice_idx == 0 ? 0 :
		slice_idx == 1 ? 0x100 :
		slice_idx == 2 ? 0x700 : 0x1900;
	reg_addr_tmp = reg_addr + reg_offset;
	rdma_wr(reg_addr_tmp, val);
};

static void wr_reg_bits_slice_vpost(int reg_addr, int val, int start, int len, int slice_idx)
{
	rdma_wr_bits_op rdma_wr_bits = cur_dev->rdma_func[VPP0].rdma_wr_bits;
	u32 reg_offset;
	u32 reg_addr_tmp;

	reg_offset = slice_idx == 0 ? 0 :
		slice_idx == 1 ? 0x100 :
		slice_idx == 2 ? 0x700 : 0x1900;
	reg_addr_tmp = reg_addr + reg_offset;
	rdma_wr_bits(reg_addr_tmp, val, start, len);
};

/* hw reg info set */
static void vpp_post_blend_set(u32 vpp_index,
	struct vpp_post_blend_s *vpp_blend)
{
	rdma_wr_op rdma_wr = cur_dev->rdma_func[vpp_index].rdma_wr;
	rdma_wr_bits_op rdma_wr_bits = cur_dev->rdma_func[vpp_index].rdma_wr_bits;
	struct vpp_post_blend_reg_s *vpp_reg = &vpp_post_reg.vpp_post_blend_reg;
	u32 postbld_src1_sel, postbld_src2_sel;
	u32 postbld_src3_sel, postbld_src4_sel;
	u32 postbld_src5_sel;
	u32 postbld_vd1_premult, postbld_vd2_premult;
	u32 postbld_vd3_premult, postbld_osd1_premult;
	u32 postbld_osd2_premult;

	/* setting blend scope */
	rdma_wr_bits(vpp_reg->vpp_postblend_vd1_h_start_end,
		(vpp_blend->bld_din0_h_start << 16) |
		vpp_blend->bld_din0_h_end, 0, 32);
	rdma_wr_bits(vpp_reg->vpp_postblend_vd1_v_start_end,
		(vpp_blend->bld_din0_v_start << 16) |
		vpp_blend->bld_din0_v_end, 0, 32);
	rdma_wr_bits(vpp_reg->vpp_postblend_vd2_h_start_end,
		(vpp_blend->bld_din1_h_start << 16) |
		vpp_blend->bld_din1_h_end, 0, 32);
	rdma_wr_bits(vpp_reg->vpp_postblend_vd2_v_start_end,
		(vpp_blend->bld_din1_v_start << 16) |
		vpp_blend->bld_din1_v_end, 0, 32);
	rdma_wr_bits(vpp_reg->vpp_postblend_vd3_h_start_end,
		(vpp_blend->bld_din2_h_start << 16) |
		vpp_blend->bld_din2_h_end, 0, 32);
	rdma_wr_bits(vpp_reg->vpp_postblend_vd3_v_start_end,
		(vpp_blend->bld_din2_v_start << 16) |
		vpp_blend->bld_din2_v_end, 0, 32);
	rdma_wr(vpp_reg->vpp_postblnd_h_v_size,
		vpp_blend->bld_out_w | vpp_blend->bld_out_h << 16);
	rdma_wr(vpp_reg->vpp_post_blend_blend_dummy_data,
		vpp_blend->bld_dummy_data);
	rdma_wr_bits(vpp_reg->vpp_post_blend_dummy_alpha,
		0x100 | 0x000 << 16, 0, 32);
	// blend0_dummy_alpha|blend1_dummy_alpha<<16
	rdma_wr_bits(vpp_reg->vpp_post_blend_dummy_alpha1,
		0x000 | 0x000 << 16, 0, 32);
	// blend2_dummy_alpha|blend3_dummy_alpha<<16

	postbld_vd1_premult  = vpp_blend->bld_din0_premult_en;
	postbld_vd2_premult  = vpp_blend->bld_din1_premult_en;
	postbld_vd3_premult  = vpp_blend->bld_din2_premult_en;
	postbld_osd1_premult = vpp_blend->bld_din3_premult_en;
	postbld_osd2_premult = vpp_blend->bld_din4_premult_en;

	postbld_src1_sel     = vpp_blend->bld_src1_sel;
	postbld_src2_sel     = vpp_blend->bld_src2_sel;
	postbld_src3_sel     = vpp_blend->bld_src3_sel;
	postbld_src4_sel     = vpp_blend->bld_src4_sel;
	postbld_src5_sel     = vpp_blend->bld_src5_sel;

	rdma_wr(vpp_reg->vd1_blend_src_ctrl,
		(postbld_src1_sel & 0xf) |
		(postbld_vd1_premult & 0x1) << 4);
	rdma_wr(vpp_reg->vd2_blend_src_ctrl,
		(postbld_src2_sel & 0xf) |
		(postbld_vd2_premult & 0x1) << 4);
	rdma_wr(vpp_reg->vd3_blend_src_ctrl,
		(postbld_src3_sel & 0xf) |
		(postbld_vd3_premult & 0x1) << 4);
	rdma_wr(vpp_reg->vd1_postblend_alpha,
		vpp_blend->bld_din0_alpha);
	rdma_wr(vpp_reg->vd2_postblend_alpha,
		vpp_blend->bld_din1_alpha);
	rdma_wr(vpp_reg->vd3_postblend_alpha,
		vpp_blend->bld_din2_alpha);

	rdma_wr_bits(vpp_reg->vpp_postblnd_ctrl,
		vpp_blend->bld_out_en, 8, 1);
	if (debug_flag_s5 & DEBUG_VPP_POST) {
		pr_info("%s: vpp_postblnd_h_v_size=%x\n",
			__func__, vpp_blend->bld_out_w | vpp_blend->bld_out_h << 16);
		pr_info("%s: vpp_postblend_vd1_h_start_end=%x\n",
			__func__, vpp_blend->bld_din0_h_start << 16 |
			vpp_blend->bld_din0_h_end);
		pr_info("%s: vpp_postblend_vd1_v_start_end=%x\n",
			__func__, vpp_blend->bld_din0_v_start << 16 |
			vpp_blend->bld_din0_v_end);
		pr_info("%s: vpp_postblend_vd2_h_start_end=%x\n",
			__func__, vpp_blend->bld_din1_h_start << 16 |
			vpp_blend->bld_din1_h_end);
		pr_info("%s: vpp_postblend_vd2_v_start_end=%x\n",
			__func__, vpp_blend->bld_din1_v_start << 16 |
			vpp_blend->bld_din1_v_end);
	}
}

static void vpp_post_slice_set(u32 vpp_index,
	struct vpp_post_s *vpp_post)
{
	rdma_wr_bits_op rdma_wr_bits = cur_dev->rdma_func[vpp_index].rdma_wr_bits;
	struct vpp_post_misc_reg_s *vpp_reg = &vpp_post_reg.vpp_post_misc_reg;
	u32 slice_set;

	/* 2ppc2slice overlap size */
	rdma_wr_bits(vpp_reg->vpp_postblnd_ctrl,
		vpp_post->overlap_hsize, 0, 8);
    /* slice mode */
	rdma_wr_bits(vpp_reg->vpp_obuf_ram_ctrl,
		vpp_post->slice_num - 1, 0, 2);

	/* default = 0, 0: 4ppc to 4slice
	 * 1: 4ppc to 2slice
	 * 2: 4ppc to 1slice
	 * 3: disable
	 */
	switch (vpp_post->slice_num) {
	case 1:
		slice_set = 2;
		break;
	case 2:
		slice_set = 1;
		break;
	case 4:
		slice_set = 0;
		break;
	default:
		slice_set = 3;
		break;
	}
	rdma_wr_bits(vpp_reg->vpp_4p4s_ctrl, slice_set, 0, 2);
	rdma_wr_bits(vpp_reg->vpp_4s4p_ctrl, slice_set, 0, 2);
	if (debug_flag_s5 & DEBUG_VPP_POST) {
		pr_info("%s: vpp_4p4s_ctrl=%x\n",
			__func__, slice_set);
	}
}

static void vpp_vd1_hwin_set(u32 vpp_index,
	struct vpp_post_s *vpp_post)
{
	rdma_wr_op rdma_wr = cur_dev->rdma_func[vpp_index].rdma_wr;
	struct vpp_post_misc_reg_s *vpp_reg = &vpp_post_reg.vpp_post_misc_reg;
	u32 vd1_win_in_hsize = 0;

	if (vpp_post->vd1_hwin.vd1_hwin_en) {
		vd1_win_in_hsize = (vpp_post->vd1_hwin.vd1_hwin_in_hsize +
			SLICE_NUM - 1) / SLICE_NUM;

		rdma_wr(vpp_reg->vpp_post_vd1_win_cut_ctrl,
			 vpp_post->vd1_hwin.vd1_hwin_en << 31  |
			 vd1_win_in_hsize);
		if (debug_flag_s5 & DEBUG_VPP_POST)
			pr_info("%s: vpp_post_vd1_win_cut_ctrl:vd1_win_in_hsize=%d\n",
				__func__, vd1_win_in_hsize);
	} else {
		rdma_wr(vpp_reg->vpp_post_vd1_win_cut_ctrl, 0);
	}
}

static void vpp_post_proc_set(u32 vpp_index,
	struct vpp_post_s *vpp_post)
{
	struct vpp_post_proc_s *vpp_post_proc = NULL;
	struct vpp_post_proc_slice_s *vpp_post_proc_slice = NULL;
	struct vpp_post_proc_hwin_s *vpp_post_proc_hwin = NULL;
	struct vpp_post_misc_reg_s *vpp_reg = &vpp_post_reg.vpp_post_misc_reg;
	u32 slice_num;
	int i;

	vpp_post_proc = &vpp_post->vpp_post_proc;
	vpp_post_proc_slice = &vpp_post_proc->vpp_post_proc_slice;
	vpp_post_proc_hwin = &vpp_post_proc->vpp_post_proc_hwin;
	slice_num = vpp_post->slice_num;

	for (i = 0; i < slice_num; i++) {
		wr_slice_vpost(vpp_reg->vpp_out_h_v_size,
			vpp_post_proc_slice->hsize[i] << 16 |
			vpp_post_proc_slice->vsize[i], i);
		wr_reg_bits_slice_vpost(vpp_reg->vpp_ofifo_size,
			0x800, 0, 14, i);
		/* slice hwin deal */
		wr_reg_bits_slice_vpost(vpp_reg->vpp_slc_deal_ctrl,
			vpp_post_proc_hwin->hwin_en[i], 3, 1, i);
		wr_slice_vpost(vpp_reg->vpp_hwin_size,
			vpp_post_proc_hwin->hwin_end[i] << 16 |
			vpp_post_proc_hwin->hwin_bgn[i], i);
		wr_reg_bits_slice_vpost(vpp_reg->vpp_align_fifo_size,
			vpp_post_proc->align_fifo_size[i], 0, 14, i);
		/* todo: for other unit bypass handle */
		if (debug_flag_s5 & DEBUG_VPP_POST) {
			pr_info("%s: vpp_out_h_v_size=%x\n",
				__func__, vpp_post_proc_slice->hsize[i] << 16 |
			vpp_post_proc_slice->vsize[i]);
			pr_info("%s: vpp_hwin_size=%x\n",
				__func__, vpp_post_proc_hwin->hwin_end[i] << 16 |
			vpp_post_proc_hwin->hwin_bgn[i]);
		}
	}
}

static void vpp_post_padding_set(u32 vpp_index,
	struct vpp_post_s *vpp_post)
{
	rdma_wr_op rdma_wr = cur_dev->rdma_func[vpp_index].rdma_wr;
	struct vpp_post_misc_reg_s *vpp_reg = &vpp_post_reg.vpp_post_misc_reg;

	if (vpp_post->vpp_post_pad.vpp_post_pad_en) {
		/* reg_pad_hsize */
		rdma_wr(vpp_reg->vpp_post_pad_hsize,
			(vpp_post->vpp_post_pad.vpp_post_pad_hsize)	<< 0);
		rdma_wr(vpp_reg->vpp_post_pad_ctrl,
			vpp_post->vpp_post_pad.vpp_post_pad_dummy << 0 |
			vpp_post->vpp_post_pad.vpp_post_pad_rpt_lcol << 30 |
			vpp_post->vpp_post_pad.vpp_post_pad_en << 31);
		if (debug_flag_s5 & DEBUG_VPP_POST) {
			pr_info("%s: vpp_post_pad_hsize=%x\n",
				__func__, vpp_post->vpp_post_pad.vpp_post_pad_hsize);
		}
	} else {
		rdma_wr(vpp_reg->vpp_post_pad_ctrl, 0);
	}
}

static void vpp_post_win_cut_set(u32 vpp_index,
	struct vpp_post_s *vpp_post)
{
	//rdma_wr_op rdma_wr = cur_dev->rdma_func[vpp_index].rdma_wr;
	//struct vpp_post_misc_reg_s *vpp_reg = &vpp_post_reg.vpp_post_misc_reg;
	struct vpp_post_pad_s *vpp_post_pad = NULL;

	vpp_post_pad = &vpp_post->vpp_post_pad;
	//if (vpp_post_pad->vpp_post_pad_en &&
	//	)
}

void vpp_post_set(u32 vpp_index, struct vpp_post_s *vpp_post)
{
	if (!vpp_post)
		return;
	/* cfg slice mode */
	vpp_post_slice_set(vpp_index, vpp_post);
	/* cfg vd1 hwin cut */
	vpp_vd1_hwin_set(vpp_index, vpp_post);
	/* cfg vpp_blend */
	vpp_post_blend_set(vpp_index, &vpp_post->vpp_post_blend);
	/* vpp post units set */
	vpp_post_proc_set(vpp_index, vpp_post);
	/* cfg vpp_post pad if enable */
	vpp_post_padding_set(vpp_index, vpp_post);
	/* cfg vpp_post hwin cut if expected vpp post
	 * dout hsize less than blend or pad hsize
	 */
	vpp_post_win_cut_set(vpp_index, vpp_post);
}

/* hw reg param info set */
static int vpp_post_hwincut_param_set(struct vpp_post_input_s *vpp_input,
	struct vpp_post_s *vpp_post)
{
	if (!vpp_input || !vpp_post)
		return -1;
	/* need check vd1 padding or not */
	if (vpp_input->vd1_padding_en) {
		vpp_post->vd1_hwin.vd1_hwin_en = 1;
		vpp_post->vd1_hwin.vd1_hwin_in_hsize =
			vpp_input->vd1_size_after_padding;
		vpp_post->vd1_hwin.vd1_hwin_out_hsize =
			vpp_input->vd1_size_before_padding;

		vpp_input->din_hsize[0] = vpp_post->vd1_hwin.vd1_hwin_out_hsize;
	} else {
		vpp_post->vd1_hwin.vd1_hwin_en = 0;
	}
	return 0;
}

/* following is vpp post parameters calc for hw setting */
static int vpp_blend_param_set(struct vpp_post_input_s *vpp_input,
	struct vpp_post_blend_s *vpp_post_blend)
{
	if (!vpp_input || !vpp_post_blend)
		return -1;
	vpp_post_blend->bld_dummy_data = 0x008080;
	vpp_post_blend->bld_out_en = 1;

	/* 1:din0	2:din1 3:din2 4:din3 5:din4 else :close */
	if (vd_layer[0].post_blend_en)
		vpp_post_blend->bld_src1_sel = 1;
	else
		vpp_post_blend->bld_src1_sel = 0;
	if (vd_layer[1].post_blend_en)
		vpp_post_blend->bld_src2_sel = 2;
	else
		vpp_post_blend->bld_src2_sel = 0;
#ifdef CHECK_LATER
	vpp_post_blend->bld_src3_sel = 0;
	vpp_post_blend->bld_src4_sel = 0;
	vpp_post_blend->bld_src5_sel = 0;
#endif
	vpp_post_blend->bld_out_w = vpp_input->bld_out_hsize;
	vpp_post_blend->bld_out_h = vpp_input->bld_out_vsize;

	vpp_post_blend->bld_din0_h_start = vpp_input->din_x_start[0];
	vpp_post_blend->bld_din0_h_end = vpp_post_blend->bld_din0_h_start +
		vpp_input->din_hsize[0] - 1;
	vpp_post_blend->bld_din0_v_start = vpp_input->din_y_start[0];
	vpp_post_blend->bld_din0_v_end = vpp_post_blend->bld_din0_v_start +
		vpp_input->din_vsize[0] - 1;
	vpp_post_blend->bld_din0_alpha = 0x100;
	vpp_post_blend->bld_din0_premult_en	= 1;

	vpp_post_blend->bld_din1_h_start = vpp_input->din_x_start[1];
	vpp_post_blend->bld_din1_h_end = vpp_post_blend->bld_din1_h_start +
		vpp_input->din_hsize[1] - 1;
	vpp_post_blend->bld_din1_v_start = vpp_input->din_y_start[1];
	vpp_post_blend->bld_din1_v_end = vpp_post_blend->bld_din1_v_start +
		vpp_input->din_vsize[1] - 1;
	vpp_post_blend->bld_din1_alpha = 0x100;
	vpp_post_blend->bld_din1_premult_en	= 0;

	vpp_post_blend->bld_din2_h_start = vpp_input->din_x_start[2];
	vpp_post_blend->bld_din2_h_end = vpp_post_blend->bld_din2_h_start +
		vpp_input->din_hsize[2] - 1;
	vpp_post_blend->bld_din2_v_start = vpp_input->din_y_start[2];
	vpp_post_blend->bld_din2_v_end = vpp_post_blend->bld_din2_v_start +
		vpp_input->din_vsize[2] - 1;
	vpp_post_blend->bld_din2_alpha = 0x100;
	vpp_post_blend->bld_din2_premult_en	= 0;

	vpp_post_blend->bld_din3_h_start = vpp_input->din_x_start[3];
	vpp_post_blend->bld_din3_h_end = vpp_post_blend->bld_din3_h_start +
		vpp_input->din_hsize[3] - 1;
	vpp_post_blend->bld_din3_v_start = vpp_input->din_x_start[3];
	vpp_post_blend->bld_din3_v_end = vpp_post_blend->bld_din3_v_start +
		vpp_input->din_vsize[3] - 1;
	vpp_post_blend->bld_din3_premult_en	= 0;

	vpp_post_blend->bld_din4_h_start = 0;
	vpp_post_blend->bld_din4_h_end = vpp_input->din_hsize[4] - 1;
	vpp_post_blend->bld_din4_v_start = 0;
	vpp_post_blend->bld_din4_v_end = vpp_input->din_vsize[4] - 1;
	vpp_post_blend->bld_din4_premult_en	= 0;
	if (debug_flag_s5 & DEBUG_VPP_POST)
		pr_info("vpp_post_blend:bld_out: %d, %d,bld_din0: %d, %d, %d, %d, bld_din1: %d, %d, %d, %d\n",
			vpp_post_blend->bld_out_w,
			vpp_post_blend->bld_out_h,
			vpp_post_blend->bld_din0_h_start,
			vpp_post_blend->bld_din0_h_end,
			vpp_post_blend->bld_din0_v_start,
			vpp_post_blend->bld_din0_v_end,
			vpp_post_blend->bld_din1_h_start,
			vpp_post_blend->bld_din1_h_end,
			vpp_post_blend->bld_din1_v_start,
			vpp_post_blend->bld_din1_v_end);
	return 0;
}

static int vpp_post_padding_param_set(struct vpp_post_s *vpp_post)
{
	u32 bld_out_w;
	u32 padding_en = 0, pad_hsize = 0;

	if (!vpp_post)
		return -1;

	/* need check post blend out hsize */
	bld_out_w = vpp_post->vpp_post_blend.bld_out_w;
	switch (vpp_post->slice_num) {
	case 4:
		/* bld out need 32 aligned if 4 slices */
		if (bld_out_w % 32) {
			padding_en = 1;
			pad_hsize = ALIGN(bld_out_w, 32);
		} else {
			padding_en = 0;
			pad_hsize = bld_out_w;
		}
		break;
	case 2:
		/* bld out need 8 aligned if 2 slices */
		if (bld_out_w % 8) {
			padding_en = 1;
			pad_hsize = ALIGN(bld_out_w, 8);
		} else {
			padding_en = 0;
			pad_hsize = bld_out_w;
		}
		break;
	case 1:
		padding_en = 0;
		pad_hsize = bld_out_w;
		break;
	default:
		pr_err("invalid slice_num[%d] number\n", vpp_post->slice_num);
		return -1;
	}
	vpp_post->vpp_post_pad.vpp_post_pad_en = padding_en;
	vpp_post->vpp_post_pad.vpp_post_pad_hsize = pad_hsize;
	vpp_post->vpp_post_pad.vpp_post_pad_rpt_lcol = 1;
	return 0;
}

static int vpp_post_proc_slice_param_set(struct vpp_post_s *vpp_post)
{
	u32 frm_hsize, frm_vsize;
	u32 slice_num, overlap_hsize;
	struct vpp_post_proc_slice_s *vpp_post_proc_slice = NULL;
	int i;

	if (!vpp_post)
		return -1;

	vpp_post_proc_slice = &vpp_post->vpp_post_proc.vpp_post_proc_slice;
	frm_hsize = vpp_post->vpp_post_pad.vpp_post_pad_hsize;
	frm_vsize = vpp_post->vpp_post_blend.bld_out_h;
	slice_num = vpp_post->slice_num;
	overlap_hsize = vpp_post->overlap_hsize;
	switch (slice_num) {
	case 4:
		for (i = 0; i < POST_SLICE_NUM; i++) {
			if (i == 0 || i == 3)
				vpp_post_proc_slice->hsize[i] =
					(frm_hsize + POST_SLICE_NUM - 1) /
					POST_SLICE_NUM + overlap_hsize;
			else
				vpp_post_proc_slice->hsize[i] =
					(frm_hsize + POST_SLICE_NUM - 1) /
					POST_SLICE_NUM + overlap_hsize * 2;
			vpp_post_proc_slice->vsize[i] = frm_vsize;
		}
		break;
	case 2:
		for (i = 0; i < POST_SLICE_NUM; i++) {
			if (i < 2) {
				vpp_post_proc_slice->hsize[i] =
					(frm_hsize + 2 - 1) /
					2 + overlap_hsize;
				vpp_post_proc_slice->vsize[i] = frm_vsize;
			} else {
				vpp_post_proc_slice->hsize[i] = 0;
				vpp_post_proc_slice->vsize[i] = 0;
			}
		}
		break;
	case 1:
		for (i = 0; i < POST_SLICE_NUM; i++) {
			if (i == 0) {
				vpp_post_proc_slice->hsize[i] = frm_hsize;
				vpp_post_proc_slice->vsize[i] = frm_vsize;
			} else {
				vpp_post_proc_slice->hsize[i] = 0;
				vpp_post_proc_slice->vsize[i] = 0;
			}
		}
		break;
	default:
		break;
	}
	return 0;
}

static int vpp_post_proc_hwin_param_set(struct vpp_post_s *vpp_post)
{
	u32 slice_num, overlap_hsize;
	struct vpp_post_proc_slice_s *vpp_post_proc_slice = NULL;
	struct vpp_post_proc_hwin_s *vpp_post_proc_hwin = NULL;
	int i;

	if (!vpp_post)
		return -1;
	vpp_post_proc_slice = &vpp_post->vpp_post_proc.vpp_post_proc_slice;
	vpp_post_proc_hwin = &vpp_post->vpp_post_proc.vpp_post_proc_hwin;
	slice_num = vpp_post->slice_num;
	overlap_hsize = vpp_post->overlap_hsize;

	switch (slice_num) {
	case 4:
		vpp_post_proc_hwin->hwin_en[0] = 1;
		vpp_post_proc_hwin->hwin_bgn[0] = 0;
		vpp_post_proc_hwin->hwin_end[0] =
			vpp_post_proc_slice->hsize[0] - overlap_hsize - 1;

		vpp_post_proc_hwin->hwin_en[1] = 1;
		vpp_post_proc_hwin->hwin_bgn[1] = overlap_hsize;
		vpp_post_proc_hwin->hwin_end[1] =
			vpp_post_proc_slice->hsize[1] - overlap_hsize - 1;

		vpp_post_proc_hwin->hwin_en[2] = 1;
		vpp_post_proc_hwin->hwin_bgn[2] = overlap_hsize;
		vpp_post_proc_hwin->hwin_end[2] =
			vpp_post_proc_slice->hsize[2] - overlap_hsize - 1;

		vpp_post_proc_hwin->hwin_en[3] = 1;
		vpp_post_proc_hwin->hwin_bgn[3] = overlap_hsize;
		vpp_post_proc_hwin->hwin_end[3] =
			vpp_post_proc_slice->hsize[3] - 1;
		break;
	case 2:
		vpp_post_proc_hwin->hwin_en[0] = 1;
		vpp_post_proc_hwin->hwin_bgn[0] = 0;
		vpp_post_proc_hwin->hwin_end[0] =
			vpp_post_proc_slice->hsize[0] - overlap_hsize - 1;

		vpp_post_proc_hwin->hwin_en[1] = 1;
		vpp_post_proc_hwin->hwin_bgn[1] = overlap_hsize;
		vpp_post_proc_hwin->hwin_end[1] =
			vpp_post_proc_slice->hsize[1] - 1;
		for (i = 2; i < POST_SLICE_NUM; i++) {
			vpp_post_proc_hwin->hwin_en[i] = 0;
			vpp_post_proc_hwin->hwin_bgn[i] = 0;
			vpp_post_proc_hwin->hwin_end[i] = 0;
		}
		break;
	case 1:
		vpp_post_proc_hwin->hwin_en[0] = 1;
		vpp_post_proc_hwin->hwin_bgn[0] = 0;
		vpp_post_proc_hwin->hwin_end[0] =
			vpp_post_proc_slice->hsize[0] - 1;
		for (i = 1; i < POST_SLICE_NUM; i++) {
			vpp_post_proc_hwin->hwin_en[i] = 0;
			vpp_post_proc_hwin->hwin_bgn[i] = 0;
			vpp_post_proc_hwin->hwin_end[i] = 0;
		}
		break;
	default:
		break;
	}
	return 0;
}

static int vpp_post_proc_param_set(struct vpp_post_s *vpp_post)
{
	struct vpp_post_proc_s *vpp_post_proc = NULL;

	vpp_post_proc = &vpp_post->vpp_post_proc;
	vpp_post_proc_slice_param_set(vpp_post);
	vpp_post_proc_hwin_param_set(vpp_post);
	vpp_post_proc->align_fifo_size[0] = 2048;
	vpp_post_proc->align_fifo_size[1] = 1536;
	vpp_post_proc->align_fifo_size[2] = 1024;
	vpp_post_proc->align_fifo_size[3] = 512;
	return 0;
}

/* calc all related vpp_post_param */
int vpp_post_param_set(struct vpp_post_input_s *vpp_input,
	struct vpp_post_s *vpp_post)
{
	int ret = 0;

	if (!vpp_input || !vpp_post)
		return -1;
	memset(vpp_post, 0, sizeof(struct vpp_post_s));

	vpp_post->slice_num = vpp_input->slice_num;
	vpp_post->overlap_hsize = vpp_input->overlap_hsize;
	vpp_post_hwincut_param_set(vpp_input, vpp_post);

	ret = vpp_blend_param_set(vpp_input, &vpp_post->vpp_post_blend);
	if (ret < 0)
		return ret;

	ret = vpp_post_padding_param_set(vpp_post);
	if (ret < 0)
		return ret;

	ret = vpp_post_proc_param_set(vpp_post);
	if (ret < 0)
		return ret;

	return 0;
}

/* need some logic to calc vpp_input */
static int get_vpp_slice_num(const struct vinfo_s *info)
{
	int slice_num = 1;

	/* 8k case 4 slice */
	if (info->width > 4096 && info->field_height > 2160)
		slice_num = 4;
	/* 4k120hz */
	else if (info->width == 3840 &&
		info->field_height == 2160 &&
		info->sync_duration_num / info->sync_duration_den > 60)
		slice_num = 2;
	else
		slice_num = 1;
	if (g_post_slice_num != 0xff)
		slice_num = g_post_slice_num;
	return slice_num;
}

static int check_vpp_info_changed(struct vpp_post_input_s *vpp_input)
{
	int i = 0, changed = 0;

	/* check input param */
	for (i = 0; i < 3; i++) {
		if (vpp_input->din_hsize[i] != g_vpp_input_pre.din_hsize[i] ||
			vpp_input->din_vsize[i] != g_vpp_input_pre.din_vsize[i] ||
			vpp_input->din_x_start[i] != g_vpp_input_pre.din_x_start[i] ||
			vpp_input->din_y_start[i] != g_vpp_input_pre.din_y_start[i]) {
			changed = 1;
			pr_info("hit vpp_input vd[%d]:new:%d, %d, %d, %d, pre: %d, %d, %d, %d\n",
			i,
			vpp_input->din_x_start[i],
			vpp_input->din_y_start[i],
			vpp_input->din_hsize[i],
			vpp_input->din_vsize[i],
			g_vpp_input_pre.din_x_start[i],
			g_vpp_input_pre.din_y_start[i],
			g_vpp_input_pre.din_hsize[i],
			g_vpp_input_pre.din_vsize[i]);
			break;
		}
	}
	/* check output param */
	if (!changed) {
		if (vpp_input->slice_num != g_vpp_input_pre.slice_num ||
			vpp_input->bld_out_hsize != g_vpp_input_pre.bld_out_hsize ||
			vpp_input->bld_out_vsize != g_vpp_input_pre.bld_out_vsize) {
			changed = 1;
			pr_info("hit vpp_input->slice_num=%d, %d, %d, %d, %d, %d\n",
				vpp_input->slice_num,
				vpp_input->bld_out_hsize,
				vpp_input->bld_out_vsize,
				g_vpp_input_pre.slice_num,
				g_vpp_input_pre.bld_out_hsize,
				g_vpp_input_pre.bld_out_vsize);
		}
	}
	memcpy(&g_vpp_input, vpp_input, sizeof(struct vpp_post_input_s));
	memcpy(&g_vpp_input_pre, vpp_input, sizeof(struct vpp_post_input_s));
	return changed;
}

struct vpp_post_input_s *get_vpp_input_info(void)
{
	return &g_vpp_input;
}

int update_vpp_input_info(const struct vinfo_s *info)
{
	int update = 0;
	struct vd_proc_s *vd_proc;
	struct vpp_post_input_s vpp_input;
	struct vd_proc_vd1_info_s *vd_proc_vd1_info;
	struct vd_proc_vd2_info_s *vd_proc_vd2_info;

	vpp_input.slice_num = get_vpp_slice_num(info);
	vpp_input.overlap_hsize = 32;
	vpp_input.bld_out_hsize = info->width;
	vpp_input.bld_out_vsize = info->field_height;
	/* need set vdx and osd input size */
	/* for hard code test */
	/* vd1 vd2 vd3 osd1 osd2 */
	vd_proc = get_vd_proc_info();
	vd_proc_vd1_info = &vd_proc->vd_proc_vd1_info;
	vd_proc_vd2_info = &vd_proc->vd_proc_vd2_info;
	/* vd1 */
	if (vd_proc_vd1_info->vd1_slices_dout_dpsel == VD1_SLICES_DOUT_4S4P) {
		vpp_input.din_hsize[0] = SIZE_ALIG32(vd_proc_vd1_info->vd1_dout_hsize);
		vpp_input.din_vsize[0] = vd_proc_vd1_info->vd1_dout_vsize;
		vpp_input.din_x_start[0] = vd_proc_vd1_info->vd1_dout_x_start[0];
		vpp_input.din_y_start[0] = vd_proc_vd1_info->vd1_dout_y_start[0];
	} else if (vd_proc_vd1_info->vd1_slices_dout_dpsel == VD1_SLICES_DOUT_2S4P) {
		vpp_input.din_hsize[0] = SIZE_ALIG16(vd_proc_vd1_info->vd1_dout_hsize);
		vpp_input.din_vsize[0] = vd_proc_vd1_info->vd1_dout_vsize;
		vpp_input.din_x_start[0] = vd_proc_vd1_info->vd1_dout_x_start[0];
		vpp_input.din_y_start[0] = vd_proc_vd1_info->vd1_dout_y_start[0];
	} else {
		if (vd_proc->vd_proc_pi.pi_en) {
			vpp_input.din_hsize[0] = vd_proc->vd_proc_blend.bld_out_w * 2;
			vpp_input.din_vsize[0] = vd_proc->vd_proc_blend.bld_out_h * 2;
		} else {
			vpp_input.din_hsize[0] = vd_proc->vd_proc_blend.bld_out_w;
			vpp_input.din_vsize[0] = vd_proc->vd_proc_blend.bld_out_h;
		}
		vpp_input.din_x_start[0] = 0;
		vpp_input.din_y_start[0] = 0;
	}
	/* vd2 */
	vpp_input.din_hsize[1] = vd_proc->vd2_proc.dout_hsize;
	vpp_input.din_vsize[1] = vd_proc->vd2_proc.dout_vsize;
	vpp_input.din_x_start[1] = vd_proc->vd2_proc.vd2_dout_x_start;
	vpp_input.din_y_start[1] = vd_proc->vd2_proc.vd2_dout_y_start;
	/* vd3 */
	vpp_input.din_hsize[2] = 0;
	vpp_input.din_vsize[2] = 0;

	/* osd1 */
	vpp_input.din_hsize[3] = 0;
	vpp_input.din_vsize[3] = 0;
	/* osd2 */
	vpp_input.din_hsize[4] = 0;
	vpp_input.din_vsize[4] = 0;

	if (vd_proc_vd1_info->vd1_slices_dout_dpsel == VD1_SLICES_DOUT_4S4P ||
		vd_proc_vd1_info->vd1_slices_dout_dpsel == VD1_SLICES_DOUT_2S4P) {
		vpp_input.vd1_padding_en = 1;
		vpp_input.vd1_size_before_padding = vd_proc_vd1_info->vd1_dout_hsize;
		vpp_input.vd1_size_after_padding = vpp_input.din_hsize[0];
	} else {
		vpp_input.vd1_padding_en = 0;
		vpp_input.vd1_size_before_padding = vpp_input.din_hsize[0];
		vpp_input.vd1_size_after_padding = vpp_input.din_hsize[0];
	}
	update = check_vpp_info_changed(&vpp_input);
	return update;
}

