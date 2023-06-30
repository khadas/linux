// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include "meson_vpu_pipeline.h"
#include "meson_vpu_reg.h"
#include "meson_vpu_util.h"
#include "meson_osd_scaler.h"
#include "meson_osd_afbc.h"


static int osdscaler_force_update;
module_param(osdscaler_force_update, int, 0664);
MODULE_PARM_DESC(osdscaler_force_update, "osdscaler_force_update");

static int osdscaler_v_filter_mode = -1;
module_param(osdscaler_v_filter_mode, int, 0664);
MODULE_PARM_DESC(osdscaler_v_filter_mode, "osdscaler_v_filter_mode");

static int osdscaler_h_filter_mode = -1;
module_param(osdscaler_h_filter_mode, int, 0664);
MODULE_PARM_DESC(osdscaler_h_filter_mode, "osdscaler_h_filter_mode");

static struct osd_scaler_reg_s osd_scaler_reg[HW_OSD_SCALER_NUM] = {
	{
		VPP_OSD_SCALE_COEF_IDX,
		VPP_OSD_SCALE_COEF,
		VPP_OSD_VSC_PHASE_STEP,
		VPP_OSD_VSC_INI_PHASE,
		VPP_OSD_VSC_CTRL0,
		VPP_OSD_HSC_PHASE_STEP,
		VPP_OSD_HSC_INI_PHASE,
		VPP_OSD_HSC_CTRL0,
		VPP_OSD_HSC_INI_PAT_CTRL,
		VPP_OSD_SC_DUMMY_DATA,
		VPP_OSD_SC_CTRL0,
		VPP_OSD_SCI_WH_M1,
		VPP_OSD_SCO_H_START_END,
		VPP_OSD_SCO_V_START_END,
	},
	{
		OSD2_SCALE_COEF_IDX,
		OSD2_SCALE_COEF,
		OSD2_VSC_PHASE_STEP,
		OSD2_VSC_INI_PHASE,
		OSD2_VSC_CTRL0,
		OSD2_HSC_PHASE_STEP,
		OSD2_HSC_INI_PHASE,
		OSD2_HSC_CTRL0,
		OSD2_HSC_INI_PAT_CTRL,
		OSD2_SC_DUMMY_DATA,
		OSD2_SC_CTRL0,
		OSD2_SCI_WH_M1,
		OSD2_SCO_H_START_END,
		OSD2_SCO_V_START_END,
	},
	{
		OSD34_SCALE_COEF_IDX,
		OSD34_SCALE_COEF,
		OSD34_VSC_PHASE_STEP,
		OSD34_VSC_INI_PHASE,
		OSD34_VSC_CTRL0,
		OSD34_HSC_PHASE_STEP,
		OSD34_HSC_INI_PHASE,
		OSD34_HSC_CTRL0,
		OSD34_HSC_INI_PAT_CTRL,
		OSD34_SC_DUMMY_DATA,
		OSD34_SC_CTRL0,
		OSD34_SCI_WH_M1,
		OSD34_SCO_H_START_END,
		OSD34_SCO_V_START_END,
	}
};

static struct osd_scaler_reg_s osd_scaler_t7_reg[HW_OSD_SCALER_NUM] = {
	{
		T7_VPP_OSD_SCALE_COEF_IDX,
		T7_VPP_OSD_SCALE_COEF,
		T7_VPP_OSD_VSC_PHASE_STEP,
		T7_VPP_OSD_VSC_INI_PHASE,
		T7_VPP_OSD_VSC_CTRL0,
		T7_VPP_OSD_HSC_PHASE_STEP,
		T7_VPP_OSD_HSC_INI_PHASE,
		T7_VPP_OSD_HSC_CTRL0,
		T7_VPP_OSD_HSC_INI_PAT_CTRL,
		T7_VPP_OSD_SC_DUMMY_DATA,
		T7_VPP_OSD_SC_CTRL0,
		T7_VPP_OSD_SCI_WH_M1,
		T7_VPP_OSD_SCO_H_START_END,
		T7_VPP_OSD_SCO_V_START_END,
	},
	{
		T7_OSD2_SCALE_COEF_IDX,
		T7_OSD2_SCALE_COEF,
		T7_OSD2_VSC_PHASE_STEP,
		T7_OSD2_VSC_INI_PHASE,
		T7_OSD2_VSC_CTRL0,
		T7_OSD2_HSC_PHASE_STEP,
		T7_OSD2_HSC_INI_PHASE,
		T7_OSD2_HSC_CTRL0,
		T7_OSD2_HSC_INI_PAT_CTRL,
		T7_OSD2_SC_DUMMY_DATA,
		T7_OSD2_SC_CTRL0,
		T7_OSD2_SCI_WH_M1,
		T7_OSD2_SCO_H_START_END,
		T7_OSD2_SCO_V_START_END,
	},
	{
		T7_OSD34_SCALE_COEF_IDX,
		T7_OSD34_SCALE_COEF,
		T7_OSD34_VSC_PHASE_STEP,
		T7_OSD34_VSC_INI_PHASE,
		T7_OSD34_VSC_CTRL0,
		T7_OSD34_HSC_PHASE_STEP,
		T7_OSD34_HSC_INI_PHASE,
		T7_OSD34_HSC_CTRL0,
		T7_OSD34_HSC_INI_PAT_CTRL,
		T7_OSD34_SC_DUMMY_DATA,
		T7_OSD34_SC_CTRL0,
		T7_OSD34_SCI_WH_M1,
		T7_OSD34_SCO_H_START_END,
		T7_OSD34_SCO_V_START_END,
	},
	{
		T7_OSD4_SCALE_COEF_IDX,
		T7_OSD4_SCALE_COEF,
		T7_OSD4_VSC_PHASE_STEP,
		T7_OSD4_VSC_INI_PHASE,
		T7_OSD4_VSC_CTRL0,
		T7_OSD4_HSC_PHASE_STEP,
		T7_OSD4_HSC_INI_PHASE,
		T7_OSD4_HSC_CTRL0,
		T7_OSD4_HSC_INI_PAT_CTRL,
		T7_OSD4_SC_DUMMY_DATA,
		T7_OSD4_SC_CTRL0,
		T7_OSD4_SCI_WH_M1,
		T7_OSD4_SCO_H_START_END,
		T7_OSD4_SCO_V_START_END,
	}
};

static struct osd_scaler_reg_s osd_scaler_s5_reg[HW_OSD_SCALER_NUM] = {
	{
		OSD1_PROC_SCALE_COEF_IDX,
		OSD1_PROC_SCALE_COEF,
		OSD1_PROC_VSC_PHASE_STEP,
		OSD1_PROC_VSC_INI_PHASE,
		OSD1_PROC_VSC_CTRL0,
		OSD1_PROC_HSC_PHASE_STEP,
		OSD1_PROC_HSC_INI_PHASE,
		OSD1_PROC_HSC_CTRL0,
		OSD1_PROC_HSC_INI_PAT_CTRL,
		OSD1_PROC_SC_DUMMY_DATA,
		OSD1_PROC_SC_CTRL0,
		OSD1_PROC_SCI_WH_M1,
		OSD1_PROC_SCO_H_START_END,
		OSD1_PROC_SCO_V_START_END,
	},
	{
		OSD2_PROC_SCALE_COEF_IDX,
		OSD2_PROC_SCALE_COEF,
		OSD2_PROC_VSC_PHASE_STEP,
		OSD2_PROC_VSC_INI_PHASE,
		OSD2_PROC_VSC_CTRL0,
		OSD2_PROC_HSC_PHASE_STEP,
		OSD2_PROC_HSC_INI_PHASE,
		OSD2_PROC_HSC_CTRL0,
		OSD2_PROC_HSC_INI_PAT_CTRL,
		OSD2_PROC_SC_DUMMY_DATA,
		OSD2_PROC_SC_CTRL0,
		OSD2_PROC_SCI_WH_M1,
		OSD2_PROC_SCO_H_START_END,
		OSD2_PROC_SCO_V_START_END,
	},
	{
		OSD3_PROC_SCALE_COEF_IDX,
		OSD3_PROC_SCALE_COEF,
		OSD3_PROC_VSC_PHASE_STEP,
		OSD3_PROC_VSC_INI_PHASE,
		OSD3_PROC_VSC_CTRL0,
		OSD3_PROC_HSC_PHASE_STEP,
		OSD3_PROC_HSC_INI_PHASE,
		OSD3_PROC_HSC_CTRL0,
		OSD3_PROC_HSC_INI_PAT_CTRL,
		OSD3_PROC_SC_DUMMY_DATA,
		OSD3_PROC_SC_CTRL0,
		OSD3_PROC_SCI_WH_M1,
		OSD3_PROC_SCO_H_START_END,
		OSD3_PROC_SCO_V_START_END,
	},
	{
		OSD4_PROC_SCALE_COEF_IDX,
		OSD4_PROC_SCALE_COEF,
		OSD4_PROC_VSC_PHASE_STEP,
		OSD4_PROC_VSC_INI_PHASE,
		OSD4_PROC_VSC_CTRL0,
		OSD4_PROC_HSC_PHASE_STEP,
		OSD4_PROC_HSC_INI_PHASE,
		OSD4_PROC_HSC_CTRL0,
		OSD4_PROC_HSC_INI_PAT_CTRL,
		OSD4_PROC_SC_DUMMY_DATA,
		OSD4_PROC_SC_CTRL0,
		OSD4_PROC_SCI_WH_M1,
		OSD4_PROC_SCO_H_START_END,
		OSD4_PROC_SCO_V_START_END,
	},
};

static unsigned int __osd_filter_coefs_bicubic_sharp[] = {
	0x01fa008c, 0x01fa0100, 0xff7f0200, 0xfe7f0300,
	0xfd7e0500, 0xfc7e0600, 0xfb7d0800, 0xfb7c0900,
	0xfa7b0b00, 0xfa7a0dff, 0xf9790fff, 0xf97711ff,
	0xf87613ff, 0xf87416fe, 0xf87218fe, 0xf8701afe,
	0xf76f1dfd, 0xf76d1ffd, 0xf76b21fd, 0xf76824fd,
	0xf76627fc, 0xf76429fc, 0xf7612cfc, 0xf75f2ffb,
	0xf75d31fb, 0xf75a34fb, 0xf75837fa, 0xf7553afa,
	0xf8523cfa, 0xf8503ff9, 0xf84d42f9, 0xf84a45f9,
	0xf84848f8
};

static unsigned int __osd_filter_coefs_bicubic[] = { /* bicubic	coef0 */
	0x00800000, 0x007f0100, 0xff7f0200, 0xfe7f0300, 0xfd7e0500, 0xfc7e0600,
	0xfb7d0800, 0xfb7c0900, 0xfa7b0b00, 0xfa7a0dff, 0xf9790fff, 0xf97711ff,
	0xf87613ff, 0xf87416fe, 0xf87218fe, 0xf8701afe, 0xf76f1dfd, 0xf76d1ffd,
	0xf76b21fd, 0xf76824fd, 0xf76627fc, 0xf76429fc, 0xf7612cfc, 0xf75f2ffb,
	0xf75d31fb, 0xf75a34fb, 0xf75837fa, 0xf7553afa, 0xf8523cfa, 0xf8503ff9,
	0xf84d42f9, 0xf84a45f9, 0xf84848f8
};

static unsigned int __osd_filter_coefs_bilinear[] = { /* 2 point bilinear	coef1 */
	0x00800000, 0x007e0200, 0x007c0400, 0x007a0600, 0x00780800, 0x00760a00,
	0x00740c00, 0x00720e00, 0x00701000, 0x006e1200, 0x006c1400, 0x006a1600,
	0x00681800, 0x00661a00, 0x00641c00, 0x00621e00, 0x00602000, 0x005e2200,
	0x005c2400, 0x005a2600, 0x00582800, 0x00562a00, 0x00542c00, 0x00522e00,
	0x00503000, 0x004e3200, 0x004c3400, 0x004a3600, 0x00483800, 0x00463a00,
	0x00443c00, 0x00423e00, 0x00404000
};

static unsigned int __osd_filter_coefs_2point_bilinear[] = {
	/* 2 point bilinear, bank_length == 2	coef2 */
	0x80000000, 0x7e020000, 0x7c040000, 0x7a060000, 0x78080000, 0x760a0000,
	0x740c0000, 0x720e0000, 0x70100000, 0x6e120000, 0x6c140000, 0x6a160000,
	0x68180000, 0x661a0000, 0x641c0000, 0x621e0000, 0x60200000, 0x5e220000,
	0x5c240000, 0x5a260000, 0x58280000, 0x562a0000, 0x542c0000, 0x522e0000,
	0x50300000, 0x4e320000, 0x4c340000, 0x4a360000, 0x48380000, 0x463a0000,
	0x443c0000, 0x423e0000, 0x40400000
};

/* filt_triangle, point_num =3, filt_len =2.6, group_num = 64 */
static unsigned int __osd_filter_coefs_3point_triangle_sharp[] = {
	0x40400000, 0x3e420000, 0x3d430000, 0x3b450000,
	0x3a460000, 0x38480000, 0x37490000, 0x354b0000,
	0x344c0000, 0x324e0000, 0x314f0000, 0x2f510000,
	0x2e520000, 0x2c540000, 0x2b550000, 0x29570000,
	0x28580000, 0x265a0000, 0x245c0000, 0x235d0000,
	0x215f0000, 0x20600000, 0x1e620000, 0x1d620100,
	0x1b620300, 0x19630400, 0x17630600, 0x15640700,
	0x14640800, 0x12640a00, 0x11640b00, 0x0f650c00,
	0x0d660d00
};

static unsigned int __osd_filter_coefs_3point_triangle[] = {
	0x40400000, 0x3f400100, 0x3d410200, 0x3c410300,
	0x3a420400, 0x39420500, 0x37430600, 0x36430700,
	0x35430800, 0x33450800, 0x32450900, 0x31450a00,
	0x30450b00, 0x2e460c00, 0x2d460d00, 0x2c470d00,
	0x2b470e00, 0x29480f00, 0x28481000, 0x27481100,
	0x26491100, 0x25491200, 0x24491300, 0x234a1300,
	0x224a1400, 0x214a1500, 0x204a1600, 0x1f4b1600,
	0x1e4b1700, 0x1d4b1800, 0x1c4c1800, 0x1b4c1900,
	0x1a4c1a00
};

static unsigned int __osd_filter_coefs_4point_triangle[] = {
	0x20402000, 0x20402000, 0x1f3f2101, 0x1f3f2101,
	0x1e3e2202, 0x1e3e2202, 0x1d3d2303, 0x1d3d2303,
	0x1c3c2404, 0x1c3c2404, 0x1b3b2505, 0x1b3b2505,
	0x1a3a2606, 0x1a3a2606, 0x19392707, 0x19392707,
	0x18382808, 0x18382808, 0x17372909, 0x17372909,
	0x16362a0a, 0x16362a0a, 0x15352b0b, 0x15352b0b,
	0x14342c0c, 0x14342c0c, 0x13332d0d, 0x13332d0d,
	0x12322e0e, 0x12322e0e, 0x11312f0f, 0x11312f0f,
	0x10303010
};

/* 4th order (cubic) b-spline */
/* filt_cubic point_num =4, filt_len =4, group_num = 64 */
static unsigned int __vpp_filter_coefs_4point_bspline[] = {
	0x15561500, 0x14561600, 0x13561700, 0x12561800,
	0x11551a00, 0x11541b00, 0x10541c00, 0x0f541d00,
	0x0f531e00, 0x0e531f00, 0x0d522100, 0x0c522200,
	0x0b522300, 0x0b512400, 0x0a502600, 0x0a4f2700,
	0x094e2900, 0x084e2a00, 0x084d2b00, 0x074c2c01,
	0x074b2d01, 0x064a2f01, 0x06493001, 0x05483201,
	0x05473301, 0x05463401, 0x04453601, 0x04433702,
	0x04423802, 0x03413a02, 0x03403b02, 0x033f3c02,
	0x033d3d03
};

/* filt_quadratic, point_num =3, filt_len =3, group_num = 64 */
static unsigned int __osd_filter_coefs_3point_bspline[] = {
	0x40400000, 0x3e420000, 0x3c440000, 0x3a460000,
	0x38480000, 0x364a0000, 0x344b0100, 0x334c0100,
	0x314e0100, 0x304f0100, 0x2e500200, 0x2c520200,
	0x2a540200, 0x29540300, 0x27560300, 0x26570300,
	0x24580400, 0x23590400, 0x215a0500, 0x205b0500,
	0x1e5c0600, 0x1d5c0700, 0x1c5d0700, 0x1a5e0800,
	0x195e0900, 0x185e0a00, 0x175f0a00, 0x15600b00,
	0x14600c00, 0x13600d00, 0x12600e00, 0x11600f00,
	0x10601000
};

static unsigned int __osd_filter_coefs_repeat[] = { /* repeat coef0 */
	0x00800000, 0x00800000, 0x00800000, 0x00800000, 0x00800000, 0x00800000,
	0x00800000, 0x00800000, 0x00800000, 0x00800000, 0x00800000, 0x00800000,
	0x00800000, 0x00800000, 0x00800000, 0x00800000, 0x00800000, 0x00800000,
	0x00800000, 0x00800000, 0x00800000, 0x00800000, 0x00800000, 0x00800000,
	0x00800000, 0x00800000, 0x00800000, 0x00800000, 0x00800000, 0x00800000,
	0x00800000, 0x00800000, 0x00800000
};

static unsigned int *osd_scaler_filter_table[] = {
	__osd_filter_coefs_bicubic_sharp,
	__osd_filter_coefs_bicubic,
	__osd_filter_coefs_bilinear,
	__osd_filter_coefs_2point_bilinear,
	__osd_filter_coefs_3point_triangle_sharp,
	__osd_filter_coefs_3point_triangle,
	__osd_filter_coefs_4point_triangle,
	__vpp_filter_coefs_4point_bspline,
	__osd_filter_coefs_3point_bspline,
	__osd_filter_coefs_repeat
};

/*********vsc config begin**********/
/*vsc phase_step=(v_in << 20)/v_out */
void osd_vsc_phase_step_set(struct meson_vpu_block *vblk,
			    struct rdma_reg_ops *reg_ops,
			    struct osd_scaler_reg_s *reg, u32 phase_step)
{
	reg_ops->rdma_write_reg_bits(reg->vpp_osd_vsc_phase_step,
				     phase_step, 0, 28);
}

/*vsc init phase*/
void osd_vsc_init_phase_set(struct meson_vpu_block *vblk,
			    struct rdma_reg_ops *reg_ops,
			    struct osd_scaler_reg_s *reg,
			    u32 bottom_init_phase, u32 top_init_phase)
{
	reg_ops->rdma_write_reg(reg->vpp_osd_vsc_ini_phase,
				(bottom_init_phase << 16) |
				(top_init_phase << 0));
}

/*vsc control*/
/*vsc enable last line repeate*/
void osd_vsc_repate_last_line_enable_set(struct meson_vpu_block *vblk,
					 struct rdma_reg_ops *reg_ops,
					 struct osd_scaler_reg_s *reg,
					 bool flag)
{
	reg_ops->rdma_write_reg_bits(reg->vpp_osd_vsc_ctrl0, flag, 25, 1);
}

/*vsc enable*/
void osd_vsc_enable_set(struct meson_vpu_block *vblk,
			struct rdma_reg_ops *reg_ops,
			struct osd_scaler_reg_s *reg, bool flag)
{
	reg_ops->rdma_write_reg_bits(reg->vpp_osd_vsc_ctrl0, flag, 24, 1);
}

/*vsc input Interlaced or Progressive:0->P;1->I*/
void osd_vsc_output_format_set(struct meson_vpu_block *vblk,
			       struct rdma_reg_ops *reg_ops,
			       struct osd_scaler_reg_s *reg, bool flag)
{
	reg_ops->rdma_write_reg_bits(reg->vpp_osd_vsc_ctrl0, flag, 23, 1);
}

/*
 *vsc double line mode
 *bit0:change line buffer becomes 2 lines
 *bit1:double input width and half input height
 */
void osd_vsc_double_line_mode_set(struct meson_vpu_block *vblk,
				  struct rdma_reg_ops *reg_ops,
				  struct osd_scaler_reg_s *reg, u32 data)
{
	reg_ops->rdma_write_reg_bits(reg->vpp_osd_vsc_ctrl0, data, 21, 2);
}

/*vsc phase always on*/
void osd_vsc_phase_always_on_set(struct meson_vpu_block *vblk,
				 struct rdma_reg_ops *reg_ops,
				 struct osd_scaler_reg_s *reg, bool flag)
{
	reg_ops->rdma_write_reg_bits(reg->vpp_osd_vsc_ctrl0, flag, 20, 1);
}

/*vsc nearest en*/
void osd_vsc_nearest_en_set(struct meson_vpu_block *vblk,
			    struct rdma_reg_ops *reg_ops,
			    struct osd_scaler_reg_s *reg, bool flag)
{
	reg_ops->rdma_write_reg_bits(reg->vpp_osd_vsc_ctrl0, flag, 19, 1);
}

/*vsc repeate bottom field line0 num*/
void osd_vsc_bot_rpt_l0_num_set(struct meson_vpu_block *vblk,
				struct rdma_reg_ops *reg_ops,
				struct osd_scaler_reg_s *reg, u32 data)
{
	reg_ops->rdma_write_reg_bits(reg->vpp_osd_vsc_ctrl0, data, 16, 2);
}

/*vsc bottom field init receive num??*/
void osd_vsc_bot_ini_rcv_num_set(struct meson_vpu_block *vblk,
				 struct rdma_reg_ops *reg_ops,
				 struct osd_scaler_reg_s *reg, u32 data)
{
	reg_ops->rdma_write_reg_bits(reg->vpp_osd_vsc_ctrl0, data, 11, 4);
}

/*vsc repeate top field line0 num*/
void osd_vsc_top_rpt_l0_num_set(struct meson_vpu_block *vblk,
				struct rdma_reg_ops *reg_ops,
				struct osd_scaler_reg_s *reg, u32 flag)
{
	reg_ops->rdma_write_reg_bits(reg->vpp_osd_vsc_ctrl0, flag, 8, 2);
}

/*vsc top field init receive num??*/
void osd_vsc_top_ini_rcv_num_set(struct meson_vpu_block *vblk,
				 struct rdma_reg_ops *reg_ops,
				 struct osd_scaler_reg_s *reg, u32 data)
{
	reg_ops->rdma_write_reg_bits(reg->vpp_osd_vsc_ctrl0, data, 3, 4);
}

/*vsc bank length??*/
void osd_vsc_bank_length_set(struct meson_vpu_block *vblk,
			     struct rdma_reg_ops *reg_ops,
			     struct osd_scaler_reg_s *reg, u32 data)
{
	reg_ops->rdma_write_reg_bits(reg->vpp_osd_vsc_ctrl0, data, 0, 3);
}

/*********vsc config end**********/

/*********hsc config begin**********/
/*hsc phase_step=(v_in << 20)/v_out */
void osd_hsc_phase_step_set(struct meson_vpu_block *vblk,
			    struct rdma_reg_ops *reg_ops,
			    struct osd_scaler_reg_s *reg, u32 phase_step)
{
	reg_ops->rdma_write_reg_bits(reg->vpp_osd_hsc_phase_step,
				     phase_step, 0, 28);
}

/*vsc init phase*/
void osd_hsc_init_phase_set(struct meson_vpu_block *vblk,
			    struct rdma_reg_ops *reg_ops,
			    struct osd_scaler_reg_s *reg,
			    u32 init_phase0, u32 init_phase1)
{
	reg_ops->rdma_write_reg(reg->vpp_osd_hsc_ini_phase,
				(init_phase1 << 16) | (init_phase0 << 0));
}

/*hsc control*/
/*hsc enable*/
void osd_hsc_enable_set(struct meson_vpu_block *vblk,
			struct rdma_reg_ops *reg_ops,
			struct osd_scaler_reg_s *reg, bool flag)
{
	reg_ops->rdma_write_reg_bits(reg->vpp_osd_hsc_ctrl0, flag, 22, 1);
}

/* hsc double pixel mode */
void osd_hsc_double_line_mode_set(struct meson_vpu_block *vblk,
				  struct rdma_reg_ops *reg_ops,
				  struct osd_scaler_reg_s *reg, bool flag)
{
	reg_ops->rdma_write_reg_bits(reg->vpp_osd_hsc_ctrl0, flag, 21, 1);
}

/*hsc phase always on*/
void osd_hsc_phase_always_on_set(struct meson_vpu_block *vblk,
				 struct rdma_reg_ops *reg_ops,
				 struct osd_scaler_reg_s *reg, bool flag)
{
	reg_ops->rdma_write_reg_bits(reg->vpp_osd_hsc_ctrl0, flag, 20, 1);
}

/*hsc nearest en*/
void osd_hsc_nearest_en_set(struct meson_vpu_block *vblk,
			    struct rdma_reg_ops *reg_ops,
			    struct osd_scaler_reg_s *reg, bool flag)
{
	reg_ops->rdma_write_reg_bits(reg->vpp_osd_hsc_ctrl0, flag, 19, 1);
}

/*hsc repeate pixel0 num1??*/
void osd_hsc_rpt_p0_num1_set(struct meson_vpu_block *vblk,
			     struct rdma_reg_ops *reg_ops,
			     struct osd_scaler_reg_s *reg, u32 data)
{
	reg_ops->rdma_write_reg_bits(reg->vpp_osd_hsc_ctrl0, data, 16, 2);
}

/*hsc init receive num1*/
void osd_vsc_ini_rcv_num1_set(struct meson_vpu_block *vblk,
			      struct rdma_reg_ops *reg_ops,
			      struct osd_scaler_reg_s *reg, u32 data)
{
	reg_ops->rdma_write_reg_bits(reg->vpp_osd_hsc_ctrl0, data, 11, 4);
}

/*hsc repeate pixel0 num0*/
void osd_hsc_rpt_p0_num0_set(struct meson_vpu_block *vblk,
			     struct rdma_reg_ops *reg_ops,
			     struct osd_scaler_reg_s *reg, u32 flag)
{
	reg_ops->rdma_write_reg_bits(reg->vpp_osd_hsc_ctrl0, flag, 8, 2);
}

/*hsc init receive num0*/
void osd_hsc_ini_rcv_num0_set(struct meson_vpu_block *vblk,
			      struct rdma_reg_ops *reg_ops,
			      struct osd_scaler_reg_s *reg, u32 data)
{
	reg_ops->rdma_write_reg_bits(reg->vpp_osd_hsc_ctrl0, data, 3, 4);
}

/*hsc bank length*/
void osd_hsc_bank_length_set(struct meson_vpu_block *vblk,
			     struct rdma_reg_ops *reg_ops,
			     struct osd_scaler_reg_s *reg, u32 data)
{
	reg_ops->rdma_write_reg_bits(reg->vpp_osd_hsc_ctrl0, data, 0, 3);
}

/*
 *hsc init pattern
 *[15:8]pattern
 *[6:4]pattern start
 *[2:0]pattern end
 */
void osd_hsc_ini_pat_set(struct meson_vpu_block *vblk,
			 struct rdma_reg_ops *reg_ops,
			 struct osd_scaler_reg_s *reg, u32 data)
{
	reg_ops->rdma_write_reg(reg->vpp_osd_hsc_ini_pat_ctrl, data);
}

/*********hsc config end**********/

/*********sc top ctrl start**********/
/*
 *dummy data:
 *[31:24]componet0
 *[23:16]componet1
 *[15:8]componet2
 *[7:0]alpha
 */
void osd_sc_dummy_data_set(struct meson_vpu_block *vblk,
			   struct rdma_reg_ops *reg_ops,
			   struct osd_scaler_reg_s *reg, u32 data)
{
	reg_ops->rdma_write_reg(reg->vpp_osd_sc_dummy_data, data);
}

/*sc gate clock*/
void osd_sc_gclk_set(struct meson_vpu_block *vblk,
		     struct rdma_reg_ops *reg_ops,
		     struct osd_scaler_reg_s *reg, u32 data)
{
	reg_ops->rdma_write_reg_bits(reg->vpp_osd_sc_ctrl0, data, 16, 12);
}

/*
 *sc input data alpha mode
 *0:(alpha>=128)?alpha-1:alpha
 *1:(alpha>=1)?alpha-1:alpha
 */
void osd_sc_din_alpha_mode_set(struct meson_vpu_block *vblk,
			       struct rdma_reg_ops *reg_ops,
			       struct osd_scaler_reg_s *reg, bool flag)
{
	reg_ops->rdma_write_reg_bits(reg->vpp_osd_sc_ctrl0, flag, 13, 1);
}

/*
 *sc output data alpha mode
 *0:(alpha>=128)?alpha+1:alpha
 *1:(alpha>=1)?alpha+1:alpha
 */
void osd_sc_dout_alpha_mode_set(struct meson_vpu_block *vblk,
				struct rdma_reg_ops *reg_ops,
				struct osd_scaler_reg_s *reg, bool flag)
{
	reg_ops->rdma_write_reg_bits(reg->vpp_osd_sc_ctrl0, flag, 12, 1);
}

/*sc alpha*/
void osd_sc_alpha_set(struct meson_vpu_block *vblk,
		      struct rdma_reg_ops *reg_ops,
		      struct osd_scaler_reg_s *reg, u32 data)
{
	reg_ops->rdma_write_reg_bits(reg->vpp_osd_sc_ctrl0, data, 4, 8);
}

/*sc path en*/
void osd_sc_path_en_set(struct meson_vpu_block *vblk,
			struct rdma_reg_ops *reg_ops,
			struct osd_scaler_reg_s *reg, bool flag)
{
	reg_ops->rdma_write_reg_bits(reg->vpp_osd_sc_ctrl0, flag, 3, 1);
}

/*sc en*/
void osd_sc_en_set(struct meson_vpu_block *vblk,
		   struct rdma_reg_ops *reg_ops,
		   struct osd_scaler_reg_s *reg, bool flag)
{
	reg_ops->rdma_write_reg_bits(reg->vpp_osd_sc_ctrl0, flag, 2, 1);
}

/*sc input width minus 1*/
void osd_sc_in_w_set(struct meson_vpu_block *vblk,
		     struct rdma_reg_ops *reg_ops,
		     struct osd_scaler_reg_s *reg, u32 size)
{
	reg_ops->rdma_write_reg_bits(reg->vpp_osd_sci_wh_m1, (size - 1), 16, 13);
}

/*sc input height minus 1*/
void osd_sc_in_h_set(struct meson_vpu_block *vblk,
		     struct rdma_reg_ops *reg_ops,
		     struct osd_scaler_reg_s *reg, u32 size)
{
	reg_ops->rdma_write_reg_bits(reg->vpp_osd_sci_wh_m1, (size - 1), 0, 13);
}

/*sc output horizontal size = end - start + 1*/
void osd_sc_out_horz_set(struct meson_vpu_block *vblk,
			 struct rdma_reg_ops *reg_ops,
			 struct osd_scaler_reg_s *reg, u32 start, u32 end)
{
	reg_ops->rdma_write_reg(reg->vpp_osd_sco_h_start_end,
				(start & 0xfff << 16) | (end & 0xfff));
}

/*sc output vertical size = end - start + 1*/
void osd_sc_out_vert_set(struct meson_vpu_block *vblk,
			 struct rdma_reg_ops *reg_ops,
			 struct osd_scaler_reg_s *reg, u32 start, u32 end)
{
	reg_ops->rdma_write_reg(reg->vpp_osd_sco_v_start_end,
				(start & 0xfff << 16) | (end & 0xfff));
}

/*
 *sc h/v coef
 *1:config horizontal coef
 *0:config vertical coef
 */
void osd_sc_coef_set(struct meson_vpu_block *vblk,
		     struct rdma_reg_ops *reg_ops,
		     struct osd_scaler_reg_s *reg, bool flag, u32 *coef)
{
	u8 i;

	reg_ops->rdma_write_reg(reg->vpp_osd_scale_coef_idx,
				(0 << 15) | /*index increment. 1bits*/
				(0 << 14) | /*read coef enable, 1bits*/
				(0 << 9) | /*coef bit mode 8 or 9. 1bits*/
				(flag << 8) |
				(0 << 0)/*coef index 7bits*/);
	for (i = 0; i < 33; i++)
		reg_ops->rdma_write_reg(reg->vpp_osd_scale_coef, coef[i]);
}

/*********sc top ctrl end************/
static void f2v_get_vertical_phase(u32 zoom_ratio,
				   enum osd_scaler_f2v_vphase_type_e type,
				   u8 bank_length,
				   struct osd_scaler_f2v_vphase_s *vphase)
{
	u8 f2v_420_in_pos_luma[OSD_SCALER_F2V_TYPE_MAX] = {
		0, 2, 0, 2, 0, 0, 0, 2, 0};
	u8 f2v_420_out_pos[OSD_SCALER_F2V_TYPE_MAX] = {
		0, 2, 2, 0, 0, 2, 0, 0, 0};
	s32 offset_in, offset_out;

	/* luma */
	offset_in = f2v_420_in_pos_luma[type]
		<< OSD_PHASE_BITS;
	offset_out = (f2v_420_out_pos[type] * zoom_ratio)
		>> (OSD_ZOOM_HEIGHT_BITS - OSD_PHASE_BITS);

	vphase->rcv_num = bank_length;
	if (bank_length == 4 || bank_length == 3)
		vphase->rpt_num = 1;
	else
		vphase->rpt_num = 0;

	if (offset_in > offset_out) {
		vphase->rpt_num = vphase->rpt_num + 1;
		vphase->phase =
			((4 << OSD_PHASE_BITS) + offset_out - offset_in)
			>> 2;
	} else {
		while ((offset_in + (4 << OSD_PHASE_BITS))
			<= offset_out) {
			if (vphase->rpt_num == 1)
				vphase->rpt_num = 0;
			else
				vphase->rcv_num++;
			offset_in += 4 << OSD_PHASE_BITS;
		}
		vphase->phase = (offset_out - offset_in) >> 2;
	}
}

void osd_scaler_config(struct osd_scaler_reg_s *reg,
		       struct meson_vpu_scaler_state *scaler_state,
		       struct meson_vpu_block *vblk,
		       struct rdma_reg_ops *reg_ops)
{
	struct meson_vpu_scaler *scaler = to_scaler_block(vblk);
	u32 vsc_top_init_rec_num, vsc_bank_length;
	u32 hsc_init_rec_num, hsc_init_rpt_p0_num, hsc_bank_length;
	u32 vsc_bot_init_rec_num, vsc_top_rpt_l0_num, vsc_bot_rpt_l0_num;
	u32 vsc_top_init_phase, vsc_bot_init_phase;
	struct osd_scaler_f2v_vphase_s vphase;
	u8 version = vblk->pipeline->osd_version;
	u32 linebuffer = scaler->linebuffer;
	u32 bank_length = scaler->bank_length;
	u32 width_in = scaler_state->input_width;
	u32 height_in = scaler_state->input_height;
	u32 width_out = scaler_state->output_width;
	u32 height_out = scaler_state->output_height;
	u32 scan_mode_out = scaler_state->scan_mode_out;
	u32 scaler_filter_mode = scaler_state->scaler_filter_mode;
	u32 vsc_double_line_mode;
	u32 *coef_h, *coef_v;
	u64 phase_step_v, phase_step_h;
	bool scaler_enable;

	if (width_in == width_out && height_in == height_out &&
	    version > OSD_V2)
		scaler_enable = false;
	else
		scaler_enable = true;

	if (width_in > linebuffer) {
		vsc_bank_length = bank_length >> 1;
		vsc_double_line_mode = 1;
	} else {
		vsc_bank_length = bank_length;
		vsc_double_line_mode = 0;
	}
	hsc_init_rec_num = bank_length;
	hsc_bank_length = bank_length;
	hsc_init_rpt_p0_num = bank_length / 2 - 1;

	if (version <= OSD_V2)
		phase_step_v = (u64)(height_in - 1) << OSD_ZOOM_HEIGHT_BITS;
	else
		phase_step_v = (u64)height_in << OSD_ZOOM_HEIGHT_BITS;
	do_div(phase_step_v, height_out);
	if (scan_mode_out) {
		f2v_get_vertical_phase(phase_step_v, OSD_SCALER_F2V_P2IT,
				       vsc_bank_length, &vphase);
		vsc_top_init_rec_num = vphase.rcv_num;
		vsc_top_rpt_l0_num = vphase.rpt_num;
		vsc_top_init_phase = vphase.phase;
		f2v_get_vertical_phase(phase_step_v, OSD_SCALER_F2V_P2IB,
				       vsc_bank_length, &vphase);
		vsc_bot_init_rec_num = vphase.rcv_num;
		vsc_bot_rpt_l0_num = vphase.rpt_num;
		vsc_bot_init_phase = vphase.phase;
		if ((phase_step_v >> OSD_ZOOM_HEIGHT_BITS) == 2) {
			vsc_bot_init_phase |= 0x8000;
			vsc_top_init_phase |= 0x8000;
		}
	} else {
		f2v_get_vertical_phase(phase_step_v, OSD_SCALER_F2V_P2P,
				       vsc_bank_length, &vphase);
		vsc_top_init_rec_num = vphase.rcv_num;
		vsc_top_rpt_l0_num = vphase.rpt_num;
		vsc_top_init_phase = vphase.phase;
		vsc_bot_init_rec_num = 0;
		vsc_bot_rpt_l0_num = 0;
		vsc_bot_init_phase = 0;
	}
	if (version <= OSD_V2)
		vsc_top_init_rec_num++;
	if (version <= OSD_V2 && scan_mode_out)
		vsc_bot_init_rec_num++;
	phase_step_v <<= (OSD_ZOOM_TOTAL_BITS - OSD_ZOOM_HEIGHT_BITS);
	phase_step_h = (u64)width_in << OSD_ZOOM_WIDTH_BITS;
	do_div(phase_step_h, width_out);
	phase_step_h <<= (OSD_ZOOM_TOTAL_BITS - OSD_ZOOM_WIDTH_BITS);
	/*check coef*/

	if (vsc_double_line_mode == 1) {
		coef_h = osd_scaler_filter_table[COEFS_BICUBIC];
		coef_v = osd_scaler_filter_table[COEFS_2POINT_BILINEAR];
	} else if (width_out <= 720) {
		coef_h = osd_scaler_filter_table[COEFS_4POINT_TRIANGLE];
		coef_v = osd_scaler_filter_table[COEFS_4POINT_TRIANGLE];
	} else if (scaler_filter_mode >= DRM_SCALING_FILTER_BICUBIC_SHARP &&
		   scaler_filter_mode <=  DRM_SCALING_FILTER_REPEATE){
		coef_h = osd_scaler_filter_table[scaler_filter_mode -
			DRM_SCALING_FILTER_BICUBIC_SHARP];
		coef_v = osd_scaler_filter_table[scaler_filter_mode -
			DRM_SCALING_FILTER_BICUBIC_SHARP];
	} else {
		coef_h = osd_scaler_filter_table[COEFS_BICUBIC];
		coef_v = osd_scaler_filter_table[COEFS_BICUBIC];
	}
	if (osdscaler_v_filter_mode != -1 &&
	    osdscaler_v_filter_mode < COEFS_MAX)
		coef_v = osd_scaler_filter_table[osdscaler_v_filter_mode];
	if (osdscaler_h_filter_mode != -1 &&
	    osdscaler_h_filter_mode < COEFS_MAX)
		coef_h = osd_scaler_filter_table[osdscaler_h_filter_mode];

	/*input size config*/
	osd_sc_in_h_set(vblk, reg_ops, reg, height_in);
	osd_sc_in_w_set(vblk, reg_ops, reg, width_in);

	/*output size config*/
	osd_sc_out_horz_set(vblk, reg_ops, reg, 0, width_out - 1);
	osd_sc_out_vert_set(vblk, reg_ops, reg, 0, height_out - 1);

	/*phase step config*/
	osd_vsc_phase_step_set(vblk, reg_ops, reg, phase_step_v);
	osd_hsc_phase_step_set(vblk, reg_ops, reg, phase_step_h);

	/*dummy data config*/
	osd_sc_dummy_data_set(vblk, reg_ops, reg, 0x80808080);

	/*h/v coef config*/
	osd_sc_coef_set(vblk, reg_ops, reg, OSD_SCALER_COEFF_H, coef_h);
	osd_sc_coef_set(vblk, reg_ops, reg, OSD_SCALER_COEFF_V, coef_v);

	/*init recv line num*/
	osd_vsc_top_ini_rcv_num_set(vblk, reg_ops, reg, vsc_top_init_rec_num);
	osd_vsc_bot_ini_rcv_num_set(vblk, reg_ops, reg, vsc_bot_init_rec_num);
	osd_hsc_ini_rcv_num0_set(vblk, reg_ops, reg, hsc_init_rec_num);
	osd_vsc_double_line_mode_set(vblk, reg_ops, reg, vsc_double_line_mode);

	/*repeate line0 num*/
	osd_vsc_top_rpt_l0_num_set(vblk, reg_ops, reg, vsc_top_rpt_l0_num);
	osd_vsc_bot_rpt_l0_num_set(vblk, reg_ops, reg, vsc_bot_rpt_l0_num);
	osd_hsc_rpt_p0_num0_set(vblk, reg_ops, reg, hsc_init_rpt_p0_num);

	/*init phase*/
	osd_vsc_init_phase_set(vblk, reg_ops, reg, vsc_bot_init_phase, vsc_top_init_phase);
	osd_hsc_init_phase_set(vblk, reg_ops, reg, 0, 0);

	/*vsc bank length*/
	osd_vsc_bank_length_set(vblk, reg_ops, reg, vsc_bank_length);
	osd_hsc_bank_length_set(vblk, reg_ops, reg, hsc_bank_length);

	/*out scan mode*/
	osd_vsc_output_format_set(vblk, reg_ops, reg, scan_mode_out ? 1 : 0);

	/*repeate last line*/
	if (version >= OSD_V2)
		osd_vsc_repate_last_line_enable_set(vblk, reg_ops, reg, 1);

	/*enable sc*/
	osd_vsc_enable_set(vblk, reg_ops, reg, scaler_enable);
	osd_hsc_enable_set(vblk, reg_ops, reg, scaler_enable);
	osd_sc_en_set(vblk, reg_ops, reg, scaler_enable);
	osd_sc_path_en_set(vblk, reg_ops, reg, scaler_enable);
}

static void scaler_size_check(struct meson_vpu_block *vblk,
			      struct meson_vpu_block_state *state)
{
	struct meson_vpu_pipeline *pipeline = vblk->pipeline;
	struct meson_vpu_pipeline_state *pipeline_state;
	struct meson_vpu_scaler_state *scaler_state = to_scaler_state(state);

	pipeline_state = priv_to_pipeline_state(pipeline->obj.state);
	if (!pipeline_state) {
		MESON_DRM_BLOCK("pipeline_state is NULL!!\n");
		return;
	}
	if (scaler_state->input_width !=
		pipeline_state->scaler_param[vblk->index].input_width) {
		scaler_state->input_width =
			pipeline_state->scaler_param[vblk->index].input_width;
		scaler_state->state_changed |= SCALER_INPUT_WIDTH_CHANGED;
	}
	if (scaler_state->input_height !=
		pipeline_state->scaler_param[vblk->index].input_height) {
		scaler_state->input_height =
			pipeline_state->scaler_param[vblk->index].input_height;
		scaler_state->state_changed |= SCALER_INPUT_HEIGHT_CHANGED;
	}
	if (scaler_state->output_width !=
		pipeline_state->scaler_param[vblk->index].output_width) {
		scaler_state->output_width =
			pipeline_state->scaler_param[vblk->index].output_width;
		scaler_state->state_changed |= SCALER_OUTPUT_WIDTH_CHANGED;
	}
	if (scaler_state->output_height !=
		pipeline_state->scaler_param[vblk->index].output_height) {
		scaler_state->output_height =
			pipeline_state->scaler_param[vblk->index].output_height;
		scaler_state->state_changed |= SCALER_OUTPUT_HEIGHT_CHANGED;
	}
}

void scan_mode_check(struct meson_vpu_pipeline *pipeline,
		     struct meson_vpu_scaler_state *scaler_state)
{
	int crtc_index = scaler_state->crtc_index;
	u32 scan_mode_out = pipeline->subs[crtc_index].mode.flags &
				DRM_MODE_FLAG_INTERLACE;

	if (scaler_state->scan_mode_out != scan_mode_out) {
		scaler_state->scan_mode_out = scan_mode_out;
		scaler_state->state_changed |=
			SCALER_OUTPUT_SCAN_MODE_CHANGED;
	}
}

void scaler_filter_mode_check(struct meson_vpu_block *vblk,
		     struct meson_vpu_scaler_state *scaler_state,
		struct meson_vpu_pipeline_state *mvps)
{
	u32 scaling_filter_mode = mvps->plane_info[vblk->index].scaling_filter;

	if (scaler_state->scaler_filter_mode != scaling_filter_mode) {
		scaler_state->scaler_filter_mode = scaling_filter_mode;
		osdscaler_force_update = 1;
	}
}

static int scaler_check_state(struct meson_vpu_block *vblk,
			      struct meson_vpu_block_state *state,
		struct meson_vpu_pipeline_state *mvps)
{
	struct meson_vpu_osd_layer_info *plane_info;
	struct meson_vpu_scaler *scaler = to_scaler_block(vblk);
	struct meson_vpu_scaler_state *scaler_state = to_scaler_state(state);

	if (state->checked)
		return 0;

	state->checked = true;
	plane_info = &mvps->plane_info[vblk->index];
	scaler_state->crtc_index = plane_info->crtc_index;
	MESON_DRM_BLOCK("%s check_state called.\n", scaler->base.name);

	return 0;
}

static void scaler_set_state(struct meson_vpu_block *vblk,
			     struct meson_vpu_block_state *state,
			     struct meson_vpu_block_state *old_state)
{
	struct meson_vpu_pipeline_state *mvps;

	struct meson_vpu_scaler *scaler = to_scaler_block(vblk);
	struct meson_vpu_scaler_state *scaler_state = to_scaler_state(state);
	struct osd_scaler_reg_s *reg = scaler->reg;
	struct meson_vpu_pipeline *pipeline = scaler->base.pipeline;

	mvps = priv_to_pipeline_state(pipeline->obj.state);
	/*todo:move afbc start to afbc block.*/
	if (pipeline->osd_version < OSD_V7)
		arm_fbc_start(mvps);

	if (!scaler_state) {
		MESON_DRM_BLOCK("scaler or scaler_state is NULL!!\n");
		return;
	}
	scaler_size_check(vblk, state);
	scan_mode_check(vblk->pipeline, scaler_state);
	scaler_filter_mode_check(vblk, scaler_state, mvps);
	MESON_DRM_BLOCK("scaler_state=0x%x\n", scaler_state->state_changed);
	if (scaler_state->state_changed || osdscaler_force_update) {
		osd_scaler_config(reg, scaler_state, vblk, state->sub->reg_ops);
		scaler_state->state_changed = 0;
		osdscaler_force_update = 0;
	}
	MESON_DRM_BLOCK("scaler%d input/output w/h[%d, %d, %d, %d].\n",
		  scaler->base.index,
		scaler_state->input_width, scaler_state->input_height,
		scaler_state->output_width, scaler_state->output_height);
}

static void scaler_hw_enable(struct meson_vpu_block *vblk,
			     struct meson_vpu_block_state *state)
{
	struct meson_vpu_scaler *scaler = to_scaler_block(vblk);

	MESON_DRM_BLOCK("%s enable done.\n", scaler->base.name);
}

static void scaler_hw_disable(struct meson_vpu_block *vblk,
			      struct meson_vpu_block_state *state)
{
	struct meson_vpu_scaler *scaler = to_scaler_block(vblk);
	struct osd_scaler_reg_s *reg = scaler->reg;

	/*disable sc*/
	meson_vpu_write_reg(reg->vpp_osd_sc_ctrl0, 0);
	MESON_DRM_BLOCK("%s disable called.\n", scaler->base.name);
}

static void scaler_dump_register(struct meson_vpu_block *vblk,
				 struct seq_file *seq)
{
	int osd_index;
	u32 value;
	char buff[8];
	struct meson_vpu_scaler *scaler;
	struct osd_scaler_reg_s *reg;

	osd_index = vblk->index;
	scaler = to_scaler_block(vblk);
	reg = scaler->reg;

	snprintf(buff, 8, "OSD%d", osd_index + 1);

	value = meson_drm_read_reg(reg->vpp_osd_vsc_phase_step);
	seq_printf(seq, "%s_%-35s\t0x%08X\n", buff, "VSC_PHASE_STEP:", value);

	value = meson_drm_read_reg(reg->vpp_osd_vsc_ini_phase);
	seq_printf(seq, "%s_%-35s\t0x%08X\n", buff, "VSC_INIT_PHASE:", value);

	value = meson_drm_read_reg(reg->vpp_osd_vsc_ctrl0);
	seq_printf(seq, "%s_%-35s\t0x%08X\n", buff, "VSC_CTRL0:", value);

	value = meson_drm_read_reg(reg->vpp_osd_hsc_phase_step);
	seq_printf(seq, "%s_%-35s\t0x%08X\n", buff, "HSC_PHASE_STEP:", value);

	value = meson_drm_read_reg(reg->vpp_osd_hsc_ini_phase);
	seq_printf(seq, "%s_%-35s\t0x%08X\n", buff, "HSC_INIT_PHASE:", value);

	value = meson_drm_read_reg(reg->vpp_osd_hsc_ctrl0);
	seq_printf(seq, "%s_%-35s\t0x%08X\n", buff, "HSC_CTRL0:", value);

	value = meson_drm_read_reg(reg->vpp_osd_sc_dummy_data);
	seq_printf(seq, "%s_%-35s\t0x%08X\n", buff, "SC_DUMMY_DATA:", value);

	value = meson_drm_read_reg(reg->vpp_osd_sc_ctrl0);
	seq_printf(seq, "%s_%-35s\t0x%08X\n", buff, "SC_CTRL0:", value);

	value = meson_drm_read_reg(reg->vpp_osd_sci_wh_m1);
	seq_printf(seq, "%s_%-35s\t0x%08X\n", buff, "SCI_WH_M1:", value);

	value = meson_drm_read_reg(reg->vpp_osd_sco_h_start_end);
	seq_printf(seq, "%s_%-35s\t0x%08X\n", buff, "SCO_H_START_END:", value);

	value = meson_drm_read_reg(reg->vpp_osd_sco_v_start_end);
	seq_printf(seq, "%s_%-35s\t0x%08X\n", buff, "SCO_V_START_END:", value);
}

static void scaler_hw_init(struct meson_vpu_block *vblk)
{
	struct meson_vpu_scaler *scaler = to_scaler_block(vblk);

	if (vblk->pipeline->osd_version != OSD_V7)
		scaler->reg = &osd_scaler_reg[vblk->index];
	else
		scaler->reg = &osd_scaler_t7_reg[vblk->index];
	scaler->linebuffer = OSD_SCALE_LINEBUFFER;
	scaler->bank_length = OSD_SCALE_BANK_LENGTH;

	meson_vpu_write_reg(scaler->reg->vpp_osd_sc_ctrl0, 0);
	MESON_DRM_BLOCK("%s hw_init called.\n", scaler->base.name);
}

static void s5_scaler_hw_init(struct meson_vpu_block *vblk)
{
	struct meson_vpu_scaler *scaler = to_scaler_block(vblk);

	scaler->reg = &osd_scaler_s5_reg[vblk->index];
	scaler->linebuffer = OSD_SCALE_LINEBUFFER;
	scaler->bank_length = OSD_SCALE_BANK_LENGTH;

	meson_vpu_write_reg(scaler->reg->vpp_osd_sc_ctrl0, 0);
	MESON_DRM_BLOCK("%s hw_init called.\n", scaler->base.name);
}

struct meson_vpu_block_ops scaler_ops = {
	.check_state = scaler_check_state,
	.update_state = scaler_set_state,
	.enable = scaler_hw_enable,
	.disable = scaler_hw_disable,
	.dump_register = scaler_dump_register,
	.init = scaler_hw_init,
};

struct meson_vpu_block_ops s5_scaler_ops = {
	.check_state = scaler_check_state,
	.update_state = scaler_set_state,
	.enable = scaler_hw_enable,
	.disable = scaler_hw_disable,
	.dump_register = scaler_dump_register,
	.init = s5_scaler_hw_init,
};
