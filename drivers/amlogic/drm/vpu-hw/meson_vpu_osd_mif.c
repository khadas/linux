// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

#ifdef CONFIG_AMLOGIC_MEDIA_CANVAS
#include <linux/amlogic/media/canvas/canvas.h>
#include <linux/amlogic/media/canvas/canvas_mgr.h>
#endif
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT
#include <linux/amlogic/media/amvecm/amvecm.h>
#endif
#include "meson_vpu_pipeline.h"
#include "meson_crtc.h"
#include "meson_vpu_reg.h"
#include "meson_vpu_util.h"

static int osd_hold_line = 8;
module_param(osd_hold_line, int, 0664);
MODULE_PARM_DESC(osd_hold_line, "osd_hold_line");

static struct osd_mif_reg_s osd_mif_reg[HW_OSD_MIF_NUM] = {
	{
		VIU_OSD1_CTRL_STAT,
		VIU_OSD1_CTRL_STAT2,
		VIU_OSD1_COLOR_ADDR,
		VIU_OSD1_COLOR,
		VIU_OSD1_TCOLOR_AG0,
		VIU_OSD1_TCOLOR_AG1,
		VIU_OSD1_TCOLOR_AG2,
		VIU_OSD1_TCOLOR_AG3,
		VIU_OSD1_BLK0_CFG_W0,
		VIU_OSD1_BLK0_CFG_W1,
		VIU_OSD1_BLK0_CFG_W2,
		VIU_OSD1_BLK0_CFG_W3,
		VIU_OSD1_BLK0_CFG_W4,
		VIU_OSD1_BLK1_CFG_W4,
		VIU_OSD1_BLK2_CFG_W4,
		VIU_OSD1_FIFO_CTRL_STAT,
		VIU_OSD1_TEST_RDDATA,
		VIU_OSD1_PROT_CTRL,
		VIU_OSD1_MALI_UNPACK_CTRL,
		VIU_OSD1_DIMM_CTRL,
	},
	{
		VIU_OSD2_CTRL_STAT,
		VIU_OSD2_CTRL_STAT2,
		VIU_OSD2_COLOR_ADDR,
		VIU_OSD2_COLOR,
		VIU_OSD2_TCOLOR_AG0,
		VIU_OSD2_TCOLOR_AG1,
		VIU_OSD2_TCOLOR_AG2,
		VIU_OSD2_TCOLOR_AG3,
		VIU_OSD2_BLK0_CFG_W0,
		VIU_OSD2_BLK0_CFG_W1,
		VIU_OSD2_BLK0_CFG_W2,
		VIU_OSD2_BLK0_CFG_W3,
		VIU_OSD2_BLK0_CFG_W4,
		VIU_OSD2_BLK1_CFG_W4,
		VIU_OSD2_BLK2_CFG_W4,
		VIU_OSD2_FIFO_CTRL_STAT,
		VIU_OSD2_TEST_RDDATA,
		VIU_OSD2_PROT_CTRL,
		VIU_OSD2_MALI_UNPACK_CTRL,
		VIU_OSD2_DIMM_CTRL,
	},
	{
		VIU_OSD3_CTRL_STAT,
		VIU_OSD3_CTRL_STAT2,
		VIU_OSD3_COLOR_ADDR,
		VIU_OSD3_COLOR,
		VIU_OSD3_TCOLOR_AG0,
		VIU_OSD3_TCOLOR_AG1,
		VIU_OSD3_TCOLOR_AG2,
		VIU_OSD3_TCOLOR_AG3,
		VIU_OSD3_BLK0_CFG_W0,
		VIU_OSD3_BLK0_CFG_W1,
		VIU_OSD3_BLK0_CFG_W2,
		VIU_OSD3_BLK0_CFG_W3,
		VIU_OSD3_BLK0_CFG_W4,
		VIU_OSD3_BLK1_CFG_W4,
		VIU_OSD3_BLK2_CFG_W4,
		VIU_OSD3_FIFO_CTRL_STAT,
		VIU_OSD3_TEST_RDDATA,
		VIU_OSD3_PROT_CTRL,
		VIU_OSD3_MALI_UNPACK_CTRL,
		VIU_OSD3_DIMM_CTRL,
	},
	{
		VIU_OSD4_CTRL_STAT,
		VIU_OSD4_CTRL_STAT2,
		VIU_OSD4_COLOR_ADDR,
		VIU_OSD4_COLOR,
		VIU_OSD4_TCOLOR_AG0,
		VIU_OSD4_TCOLOR_AG1,
		VIU_OSD4_TCOLOR_AG2,
		VIU_OSD4_TCOLOR_AG3,
		VIU_OSD4_BLK0_CFG_W0,
		VIU_OSD4_BLK0_CFG_W1,
		VIU_OSD4_BLK0_CFG_W2,
		VIU_OSD4_BLK0_CFG_W3,
		VIU_OSD4_BLK0_CFG_W4,
		VIU_OSD4_BLK1_CFG_W4,
		VIU_OSD4_BLK2_CFG_W4,
		VIU_OSD4_FIFO_CTRL_STAT,
		VIU_OSD4_TEST_RDDATA,
		VIU_OSD4_PROT_CTRL,
		VIU_OSD4_MALI_UNPACK_CTRL,
		VIU_OSD4_DIMM_CTRL,
	}
};

static struct osd_mif_reg_s s5_osd_mif_reg[HW_OSD_MIF_NUM] = {
	{
		VIU_OSD1_CTRL_STAT_S5,
		VIU_OSD1_CTRL_STAT2_S5,
		VIU_OSD1_COLOR_ADDR_S5,
		VIU_OSD1_COLOR_S5,
		VIU_OSD1_TCOLOR_AG0_S5,
		VIU_OSD1_TCOLOR_AG1_S5,
		VIU_OSD1_TCOLOR_AG2_S5,
		VIU_OSD1_TCOLOR_AG3_S5,
		VIU_OSD1_BLK0_CFG_W0_S5,
		VIU_OSD1_BLK0_CFG_W1_S5,
		VIU_OSD1_BLK0_CFG_W2_S5,
		VIU_OSD1_BLK0_CFG_W3_S5,
		VIU_OSD1_BLK0_CFG_W4_S5,
		VIU_OSD1_BLK1_CFG_W4_S5,
		VIU_OSD1_BLK2_CFG_W4_S5,
		VIU_OSD1_FIFO_CTRL_STAT_S5,
		VIU_OSD1_TEST_RDDATA_S5,
		VIU_OSD1_PROT_CTRL_S5,
		VIU_OSD1_MALI_UNPACK_CTRL_S5,
		VIU_OSD1_DIMM_CTRL_S5,
	},
	{
		VIU_OSD2_CTRL_STAT_S5,
		VIU_OSD2_CTRL_STAT2_S5,
		VIU_OSD2_COLOR_ADDR_S5,
		VIU_OSD2_COLOR_S5,
		VIU_OSD2_TCOLOR_AG0_S5,
		VIU_OSD2_TCOLOR_AG1_S5,
		VIU_OSD2_TCOLOR_AG2_S5,
		VIU_OSD2_TCOLOR_AG3_S5,
		VIU_OSD2_BLK0_CFG_W0_S5,
		VIU_OSD2_BLK0_CFG_W1_S5,
		VIU_OSD2_BLK0_CFG_W2_S5,
		VIU_OSD2_BLK0_CFG_W3_S5,
		VIU_OSD2_BLK0_CFG_W4_S5,
		VIU_OSD2_BLK1_CFG_W4_S5,
		VIU_OSD2_BLK2_CFG_W4_S5,
		VIU_OSD2_FIFO_CTRL_STAT_S5,
		VIU_OSD2_TEST_RDDATA_S5,
		VIU_OSD2_PROT_CTRL_S5,
		VIU_OSD2_MALI_UNPACK_CTRL_S5,
		VIU_OSD2_DIMM_CTRL_S5,
	},
	{
		VIU_OSD3_CTRL_STAT_S5,
		VIU_OSD3_CTRL_STAT2_S5,
		VIU_OSD3_COLOR_ADDR_S5,
		VIU_OSD3_COLOR_S5,
		VIU_OSD3_TCOLOR_AG0_S5,
		VIU_OSD3_TCOLOR_AG1_S5,
		VIU_OSD3_TCOLOR_AG2_S5,
		VIU_OSD3_TCOLOR_AG3_S5,
		VIU_OSD3_BLK0_CFG_W0_S5,
		VIU_OSD3_BLK0_CFG_W1_S5,
		VIU_OSD3_BLK0_CFG_W2_S5,
		VIU_OSD3_BLK0_CFG_W3_S5,
		VIU_OSD3_BLK0_CFG_W4_S5,
		VIU_OSD3_BLK1_CFG_W4_S5,
		VIU_OSD3_BLK2_CFG_W4_S5,
		VIU_OSD3_FIFO_CTRL_STAT_S5,
		VIU_OSD3_TEST_RDDATA_S5,
		VIU_OSD3_PROT_CTRL_S5,
		VIU_OSD3_MALI_UNPACK_CTRL_S5,
		VIU_OSD3_DIMM_CTRL_S5,
	},
	{
		VIU_OSD4_CTRL_STAT_S5,
		VIU_OSD4_CTRL_STAT2_S5,
		VIU_OSD4_COLOR_ADDR_S5,
		VIU_OSD4_COLOR_S5,
		VIU_OSD4_TCOLOR_AG0_S5,
		VIU_OSD4_TCOLOR_AG1_S5,
		VIU_OSD4_TCOLOR_AG2_S5,
		VIU_OSD4_TCOLOR_AG3_S5,
		VIU_OSD4_BLK0_CFG_W0_S5,
		VIU_OSD4_BLK0_CFG_W1_S5,
		VIU_OSD4_BLK0_CFG_W2_S5,
		VIU_OSD4_BLK0_CFG_W3_S5,
		VIU_OSD4_BLK0_CFG_W4_S5,
		VIU_OSD4_BLK1_CFG_W4_S5,
		VIU_OSD4_BLK2_CFG_W4_S5,
		VIU_OSD4_FIFO_CTRL_STAT_S5,
		VIU_OSD4_TEST_RDDATA_S5,
		VIU_OSD4_PROT_CTRL_S5,
		VIU_OSD4_MALI_UNPACK_CTRL_S5,
		VIU_OSD4_DIMM_CTRL_S5,
	},
};

static unsigned int osd_canvas[3][2];
static u32 osd_canvas_index[3] = {0, 0, 0};

/*
 * Internal function to query information for a given format. See
 * meson_drm_format_info() for the public API.
 */
const struct meson_drm_format_info *__meson_drm_format_info(u32 format)
{
	static const struct meson_drm_format_info formats[] = {
		{ .format = DRM_FORMAT_XRGB8888,
			.hw_blkmode = BLOCK_MODE_32BIT,
			.hw_colormat = COLOR_MATRIX_ARGB8888,
			.alpha_replace = 1 },
		{ .format = DRM_FORMAT_XBGR8888,
			.hw_blkmode = BLOCK_MODE_32BIT,
			.hw_colormat = COLOR_MATRIX_ABGR8888,
			.alpha_replace = 1 },
		{ .format = DRM_FORMAT_RGBX8888,
			.hw_blkmode = BLOCK_MODE_32BIT,
			.hw_colormat = COLOR_MATRIX_RGBA8888,
			.alpha_replace = 1 },
		{ .format = DRM_FORMAT_BGRX8888,
			.hw_blkmode = BLOCK_MODE_32BIT,
			.hw_colormat = COLOR_MATRIX_BGRA8888,
			.alpha_replace = 1 },
		{ .format = DRM_FORMAT_ARGB8888,
			.hw_blkmode = BLOCK_MODE_32BIT,
			.hw_colormat = COLOR_MATRIX_ARGB8888,
			.alpha_replace = 0 },
		{ .format = DRM_FORMAT_ABGR8888,
			.hw_blkmode = BLOCK_MODE_32BIT,
			.hw_colormat = COLOR_MATRIX_ABGR8888,
			.alpha_replace = 0 },
		{ .format = DRM_FORMAT_RGBA8888,
			.hw_blkmode = BLOCK_MODE_32BIT,
			.hw_colormat = COLOR_MATRIX_RGBA8888,
			.alpha_replace = 0 },
		{ .format = DRM_FORMAT_BGRA8888,
			.hw_blkmode = BLOCK_MODE_32BIT,
			.hw_colormat = COLOR_MATRIX_BGRA8888,
			.alpha_replace = 0 },
		{ .format = DRM_FORMAT_RGB888,
			.hw_blkmode = BLOCK_MODE_24BIT,
			.hw_colormat = COLOR_MATRIX_RGB888,
			.alpha_replace = 0 },
		{ .format = DRM_FORMAT_RGB565,
			.hw_blkmode = BLOCK_MODE_16BIT,
			.hw_colormat = COLOR_MATRIX_565,
			.alpha_replace = 0 },
	};

	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(formats); ++i) {
		if (formats[i].format == format)
			return &formats[i];
	}

	return NULL;
}

const struct meson_drm_format_info *__meson_drm_afbc_format_info(u32 format)
{
	static const struct meson_drm_format_info formats[] = {
		{ .format = DRM_FORMAT_XRGB8888,
			.hw_blkmode = BLOCK_MODE_RGBA8888,
			.alpha_replace = 1 },
		{ .format = DRM_FORMAT_XBGR8888,
			.hw_blkmode = BLOCK_MODE_RGBA8888,
			.alpha_replace = 1 },
		{ .format = DRM_FORMAT_RGBX8888,
			.hw_blkmode = BLOCK_MODE_RGBA8888,
			.alpha_replace = 1 },
		{ .format = DRM_FORMAT_BGRX8888,
			.hw_blkmode = BLOCK_MODE_RGBA8888,
			.alpha_replace = 1 },
		{ .format = DRM_FORMAT_ARGB8888,
			.hw_blkmode = BLOCK_MODE_RGBA8888,
			.alpha_replace = 0 },
		{ .format = DRM_FORMAT_ABGR8888,
			.hw_blkmode = BLOCK_MODE_RGBA8888,
			.alpha_replace = 0 },
		{ .format = DRM_FORMAT_RGBA8888,
			.hw_blkmode = BLOCK_MODE_RGBA8888,
			.alpha_replace = 0 },
		{ .format = DRM_FORMAT_BGRA8888,
			.hw_blkmode = BLOCK_MODE_RGBA8888,
			.alpha_replace = 0 },
		{ .format = DRM_FORMAT_RGB888,
			.hw_blkmode = BLOCK_MODE_RGB888,
			.alpha_replace = 0 },
		{ .format = DRM_FORMAT_RGB565,
			.hw_blkmode = BLOCK_MODE_RGB565,
			.alpha_replace = 0 },
		{ .format = DRM_FORMAT_ABGR2101010,
			.hw_blkmode = BLOCK_MODE_RGBA1010102,
			.alpha_replace = 0 },
	};

	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(formats); ++i) {
		if (formats[i].format == format)
			return &formats[i];
	}

	return NULL;
}

/**
 * meson_drm_format_info - query information for a given format
 * @format: pixel format (DRM_FORMAT_*)
 *
 * The caller should only pass a supported pixel format to this function.
 * Unsupported pixel formats will generate a warning in the kernel log.
 *
 * Returns:
 * The instance of struct meson_drm_format_info that describes the
 * pixel format, or NULL if the format is unsupported.
 */
const struct meson_drm_format_info *meson_drm_format_info(u32 format,
							  bool afbc_en)
{
	const struct meson_drm_format_info *info;

	if (afbc_en)
		info = __meson_drm_afbc_format_info(format);
	else
		info = __meson_drm_format_info(format);
	WARN_ON(!info);
	return info;
}

/**
 * meson_drm_format_hw_blkmode - get the hw_blkmode for format
 * @format: pixel format (DRM_FORMAT_*)
 *
 * Returns:
 * The hw_blkmode match the specified pixel format.
 */
static u8 meson_drm_format_hw_blkmode(u32 format, bool afbc_en)
{
	const struct meson_drm_format_info *info;

	info = meson_drm_format_info(format, afbc_en);
	return info ? info->hw_blkmode : 0;
}

/**
 * meson_drm_format_hw_colormat - get the hw_colormat for format
 * @format: pixel format (DRM_FORMAT_*)
 *
 * Returns:
 * The hw_colormat match the specified pixel format.
 */
static u8 meson_drm_format_hw_colormat(u32 format, bool afbc_en)
{
	const struct meson_drm_format_info *info;

	info = meson_drm_format_info(format, afbc_en);
	return info ? info->hw_colormat : 0;
}

/**
 * meson_drm_format_alpha_replace - get the alpha replace for format
 * @format: pixel format (DRM_FORMAT_*)
 *
 * Returns:
 * The alpha_replace match the specified pixel format.
 */
static u8 meson_drm_format_alpha_replace(u32 format, bool afbc_en)
{
	const struct meson_drm_format_info *info;

	info = meson_drm_format_info(format, afbc_en);
	return info ? info->alpha_replace : 0;
}

/*osd hold line config*/
void ods_hold_line_config(struct meson_vpu_block *vblk,
			  struct rdma_reg_ops *reg_ops,
			  struct osd_mif_reg_s *reg, int hold_line)
{
	u32 data = 0, value = 0;

	data = reg_ops->rdma_read_reg(reg->viu_osd_fifo_ctrl_stat);
	value = (data >> 5) & 0x1f;
	if (value != hold_line)
		reg_ops->rdma_write_reg_bits(reg->viu_osd_fifo_ctrl_stat,
					     hold_line & 0x1f, 5, 5);
}

/*osd input size config*/
void osd_input_size_config(struct meson_vpu_block *vblk,
			   struct rdma_reg_ops *reg_ops,
			   struct osd_mif_reg_s *reg, struct osd_scope_s scope)
{
	reg_ops->rdma_write_reg(reg->viu_osd_blk0_cfg_w1,
			    (scope.h_end << 16) | /*x_end pixels[13bits]*/
		scope.h_start/*x_start pixels[13bits]*/);
	reg_ops->rdma_write_reg(reg->viu_osd_blk0_cfg_w2,
			    (scope.v_end << 16) | /*y_end pixels[13bits]*/
		scope.v_start/*y_start pixels[13bits]*/);
}

/*osd canvas config*/
void osd_canvas_config(struct meson_vpu_block *vblk,
		       struct rdma_reg_ops *reg_ops,
		       struct osd_mif_reg_s *reg, u32 canvas_index)
{
	reg_ops->rdma_write_reg_bits(reg->viu_osd_blk0_cfg_w0,
				     canvas_index, 16, 8);
}

/*osd mali afbc src en
 * 1: read data from mali afbcd;0: read data from DDR directly
 */
void osd_mali_src_en(struct meson_vpu_block *vblk,
		     struct rdma_reg_ops *reg_ops,
		     struct osd_mif_reg_s *reg, u8 osd_index, bool flag)
{
	reg_ops->rdma_write_reg_bits(reg->viu_osd_blk0_cfg_w0, flag, 30, 1);
	reg_ops->rdma_write_reg_bits(OSD_PATH_MISC_CTRL,
				     flag, (osd_index + 4), 1);
}

void osd_mali_src_en_linear(struct meson_vpu_block *vblk,
			struct rdma_reg_ops *reg_ops,
			struct osd_mif_reg_s *reg, u8 osd_index, bool flag)
{
	reg_ops->rdma_write_reg_bits(reg->viu_osd_blk0_cfg_w0, flag, 30, 1);

	reg_ops->rdma_write_reg_bits(OSD_PATH_MISC_CTRL, (osd_index + 1),
				     (osd_index * 4 + 16), 4);
}

/*osd endian mode
 * 1: little endian;0: big endian[for mali afbc input]
 */
void osd_endian_mode(struct meson_vpu_block *vblk, struct rdma_reg_ops *reg_ops,
		     struct osd_mif_reg_s *reg, bool flag)
{
	reg_ops->rdma_write_reg_bits(reg->viu_osd_blk0_cfg_w0, flag, 15, 1);
}

/*osd mif enable*/
void osd_block_enable(struct meson_vpu_block *vblk,
		      struct rdma_reg_ops *reg_ops,
		      struct osd_mif_reg_s *reg, bool flag)
{
	reg_ops->rdma_write_reg_bits(reg->viu_osd_ctrl_stat, flag, 0, 1);
}

/*osd mem mode
 * 0: canvas_addr;1:linear_addr[for mali-afbc-mode]
 */
void osd_mem_mode(struct meson_vpu_block *vblk, struct rdma_reg_ops *reg_ops,
		  struct osd_mif_reg_s *reg, bool mode)
{
	reg_ops->rdma_write_reg_bits(reg->viu_osd_ctrl_stat, mode, 2, 1);
}

/*osd alpha_div en
 *if input is premult,alpha_div=1,else alpha_div=0
 */
void osd_global_alpha_set(struct meson_vpu_block *vblk,
			  struct rdma_reg_ops *reg_ops,
			  struct osd_mif_reg_s *reg, u16 val)
{
	reg_ops->rdma_write_reg_bits(reg->viu_osd_ctrl_stat, val, 12, 9);
}

/*osd alpha_div en
 *if input is premult,alpha_div=1,else alpha_div=0
 */
void osd_premult_enable(struct meson_vpu_block *vblk,
			struct rdma_reg_ops *reg_ops,
			struct osd_mif_reg_s *reg, int flag)
{
	/*afbc*/
	reg_ops->rdma_write_reg_bits(reg->viu_osd_mali_unpack_ctrl,
				     flag, 28, 1);
	/*mif*/
	//meson_vpu_write_reg_bits(reg->viu_osd_ctrl_stat, flag, 1, 1);
}

/*osd x reverse en
 *reverse read in X direction
 */
void osd_reverse_x_enable(struct meson_vpu_block *vblk,
			  struct rdma_reg_ops *reg_ops,
			  struct osd_mif_reg_s *reg, bool flag)
{
	reg_ops->rdma_write_reg_bits(reg->viu_osd_blk0_cfg_w0, flag, 28, 1);
}

/*osd y reverse en
 *reverse read in Y direction
 */
void osd_reverse_y_enable(struct meson_vpu_block *vblk,
			  struct rdma_reg_ops *reg_ops,
			  struct osd_mif_reg_s *reg, bool flag)
{
	reg_ops->rdma_write_reg_bits(reg->viu_osd_blk0_cfg_w0, flag, 29, 1);
}

/*osd mali unpack en
 * 1: osd will unpack mali_afbc_src;0:osd will unpack normal src
 */
void osd_mali_unpack_enable(struct meson_vpu_block *vblk,
			    struct rdma_reg_ops *reg_ops,
			    struct osd_mif_reg_s *reg, bool flag)
{
	reg_ops->rdma_write_reg_bits(reg->viu_osd_mali_unpack_ctrl, flag, 31, 1);
}

void osd_ctrl_init(struct meson_vpu_block *vblk, struct rdma_reg_ops *reg_ops,
		   struct osd_mif_reg_s *reg)
{
	/*Need config follow crtc index.*/
	u8 holdline = VIU1_DEFAULT_HOLD_LINE;

	reg_ops->rdma_write_reg(reg->viu_osd_fifo_ctrl_stat,
			    (1 << 31) | /*BURSET_LEN_SEL[2]*/
			    (0 << 30) | /*no swap*/
			    (0 << 29) | /*div swap*/
			    (2 << 24) | /*Fifo_lim 5bits*/
			    (2 << 22) | /*Fifo_ctrl 2bits*/
			    (0x20 << 12) | /*FIFO_DEPATH_VAL 7bits*/
			    (1 << 10) | /*BURSET_LEN_SEL[1:0]*/
			    (holdline << 5) | /*hold fifo lines 5bits*/
			    (0 << 4) | /*CLEAR_ERR*/
			    (0 << 3) | /*fifo_sync_rst*/
			    (0 << 1) | /*ENDIAN:no conversion*/
			    (1 << 0)/*urgent enable*/);
	reg_ops->rdma_write_reg(reg->viu_osd_ctrl_stat,
			    (0 << 31) | /*osd_cfg_sync_en*/
			    (0 << 30) | /*Enable free_clk*/
			    (0x100 << 12) | /*global alpha*/
			    (0 << 11) | /*TEST_RD_EN*/
			    (0 << 2) | /*osd_mem_mode 0:canvas_addr*/
			    (0 << 1) | /*premult_en*/
			    (0 << 0)/*OSD_BLK_ENABLE*/);
}

static void osd_color_config(struct meson_vpu_block *vblk,
			     struct rdma_reg_ops *reg_ops,
			     struct osd_mif_reg_s *reg,
			     u32 pixel_format, u32 pixel_blend, bool afbc_en)
{
	u8 blk_mode, color, alpha_replace;

	blk_mode = meson_drm_format_hw_blkmode(pixel_format, afbc_en);
	color = meson_drm_format_hw_colormat(pixel_format, afbc_en);
	alpha_replace = (pixel_blend == DRM_MODE_BLEND_PIXEL_NONE) ||
		meson_drm_format_alpha_replace(pixel_format, afbc_en);
	reg_ops->rdma_write_reg_bits(reg->viu_osd_blk0_cfg_w0,
				     blk_mode, 8, 4);
	reg_ops->rdma_write_reg_bits(reg->viu_osd_blk0_cfg_w0,
				     color, 2, 4);

	if (alpha_replace)
		/*replace alpha    : bit 14 enable, 6~13 alpha val.*/
		reg_ops->rdma_write_reg_bits(reg->viu_osd_ctrl_stat2, 0x1ff, 6, 9);
	else
		reg_ops->rdma_write_reg_bits(reg->viu_osd_ctrl_stat2, 0x0, 6, 9);
}

static void osd_afbc_config(struct meson_vpu_block *vblk,
			    struct rdma_reg_ops *reg_ops,
			    struct osd_mif_reg_s *reg,
			    u8 osd_index, bool afbc_en)
{
	if (!afbc_en)
		reg_ops->rdma_write_reg_bits(reg->viu_osd_ctrl_stat2, 0, 1, 1);
	else
		reg_ops->rdma_write_reg_bits(reg->viu_osd_ctrl_stat2, 1, 1, 1);

	osd_mali_unpack_enable(vblk, reg_ops, reg, afbc_en);
	osd_mali_src_en(vblk, reg_ops, reg, osd_index, afbc_en);
	osd_endian_mode(vblk, reg_ops, reg, !afbc_en);
	osd_mem_mode(vblk, reg_ops, reg, afbc_en);
}

static void osd_afbc_config_linear(struct meson_vpu_block *vblk,
			       struct rdma_reg_ops *reg_ops,
			       struct osd_mif_reg_s *reg,
			       u8 osd_index, bool afbc_en)
{
	if (!afbc_en)
		reg_ops->rdma_write_reg_bits(reg->viu_osd_ctrl_stat2, 0, 1, 1);
	else
		reg_ops->rdma_write_reg_bits(reg->viu_osd_ctrl_stat2, 1, 1, 1);

	osd_mali_unpack_enable(vblk, reg_ops, reg, afbc_en);
	osd_endian_mode(vblk, reg_ops, reg, !afbc_en);
	osd_mem_mode(vblk, reg_ops, reg, 1);
	osd_mali_src_en_linear(vblk, reg_ops, reg, osd_index, afbc_en);
}

static void osd_set_dimm_ctrl(struct meson_vpu_block *vblk,
			       struct rdma_reg_ops *reg_ops,
			       struct osd_mif_reg_s *reg,
			       u32 val)
{
	reg_ops->rdma_write_reg(reg->viu_osd_dimm_ctrl, val);
}

/* set osd, video two port */
void osd_set_two_ports(u32 set)
{
	static u32 data32[2];

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_T7)) {
		if (is_meson_t3_cpu()) {
			/* set osd, video two port */
			if (set == 1) {
				/*mali afbcd,dolby0, osd1-4 etc->VPU0*/
				/*aisr reshape, vd1, vd2, tcon p1 read->VPU2*/
				meson_vpu_write_reg(VPP_RDARB_MODE, 0x10c00000);
				meson_vpu_write_reg(VPU_RDARB_MODE_L2C1, 0x920000);
			} else if (set == 0) {
				meson_vpu_write_reg(VPP_RDARB_MODE, 0xaa0000);
				meson_vpu_write_reg(VPU_RDARB_MODE_L2C1, 0x900000);
			}
		}
		return;
	}

	/* set osd, video two port */
	if (!data32[0] && !data32[1]) {
		data32[0] = meson_vpu_read_reg(VPP_RDARB_MODE);
		data32[1] = meson_vpu_read_reg(VPU_RDARB_MODE_L2C1);
	}

	if (set == 1) {
		meson_vpu_write_reg_bits(VPP_RDARB_MODE, 2, 20, 8);
		meson_vpu_write_reg_bits(VPU_RDARB_MODE_L2C1, 2, 16, 8);
	} else if (set == 0) {
		meson_vpu_write_reg(VPP_RDARB_MODE, data32[0]);
		meson_vpu_write_reg(VPU_RDARB_MODE_L2C1, data32[1]);
	}
}

static void osd_scan_mode_config(struct meson_vpu_block *vblk,
				 struct rdma_reg_ops *reg_ops,
				 struct osd_mif_reg_s *reg, int scan_mode)
{
	if (scan_mode)
		reg_ops->rdma_write_reg_bits(reg->viu_osd_blk0_cfg_w0, 0, 1, 1);
}

static void meson_drm_osd_canvas_alloc(void)
{
	if (canvas_pool_alloc_canvas_table("osd_drm",
					   &osd_canvas[0][0],
					   sizeof(osd_canvas) /
					   sizeof(osd_canvas[0][0]),
					   CANVAS_MAP_TYPE_1)) {
		DRM_INFO("allocate drm osd canvas error.\n");
	}
}

static void meson_drm_osd_canvas_free(void)
{
	canvas_pool_free_canvas_table(&osd_canvas[0][0],
				      sizeof(osd_canvas) /
				      sizeof(osd_canvas[0][0]));
}

static void osd_linear_addr_config(struct meson_vpu_block *vblk,
				   struct rdma_reg_ops *reg_ops,
				   struct osd_mif_reg_s *reg, u64 phy_addr,
				   u32 byte_stride)
{
	reg_ops->rdma_write_reg(reg->viu_osd_blk1_cfg_w4, phy_addr >> 4);
	reg_ops->rdma_write_reg_bits(reg->viu_osd_blk2_cfg_w4,
				byte_stride, 0, 12);
}

static bool osd_check_config(struct meson_vpu_osd_state *mvos,
	struct meson_vpu_osd_state *old_mvos)
{
	if (!old_mvos || old_mvos->src_x != mvos->src_x ||
		old_mvos->src_y != mvos->src_y ||
		old_mvos->src_w != mvos->src_w ||
		old_mvos->src_h != mvos->src_h ||
		old_mvos->phy_addr != mvos->phy_addr ||
		old_mvos->pixel_format != mvos->pixel_format ||
		old_mvos->global_alpha != mvos->global_alpha ||
		old_mvos->crtc_index != mvos->crtc_index ||
		old_mvos->afbc_en != mvos->afbc_en) {
		return true;
	} else {
		return false;
	}
}

static int osd_check_state(struct meson_vpu_block *vblk,
			   struct meson_vpu_block_state *state,
		struct meson_vpu_pipeline_state *mvps)
{
	struct meson_vpu_osd_layer_info *plane_info;
	struct meson_vpu_osd *osd = to_osd_block(vblk);
	struct meson_vpu_osd_state *mvos = to_osd_state(state);

	if (state->checked)
		return 0;

	state->checked = true;

	if (!mvos || mvos->plane_index >= MESON_MAX_OSDS) {
		DRM_INFO("mvos is NULL!\n");
		return -1;
	}
	MESON_DRM_BLOCK("%s - %d check_state called.\n", osd->base.name, vblk->index);
	plane_info = &mvps->plane_info[vblk->index];
	mvos->src_x = plane_info->src_x;
	mvos->src_y = plane_info->src_y;
	mvos->src_w = plane_info->src_w;
	mvos->src_h = plane_info->src_h;
	mvos->byte_stride = plane_info->byte_stride;
	mvos->phy_addr = plane_info->phy_addr;
	mvos->pixel_format = plane_info->pixel_format;
	mvos->fb_size = plane_info->fb_size;
	mvos->pixel_blend = plane_info->pixel_blend;
	mvos->rotation = plane_info->rotation;
	mvos->afbc_en = plane_info->afbc_en;
	mvos->blend_bypass = plane_info->blend_bypass;
	mvos->plane_index = plane_info->plane_index;
	mvos->global_alpha = plane_info->global_alpha;
	mvos->crtc_index = plane_info->crtc_index;
	mvos->read_ports = plane_info->read_ports;

	return 0;
}

/* the return stride unit is 128bit(16bytes) */
static u32 line_stride_calc(u32 drm_format,
		u32 hsize,
		u32 stride_align_32bytes)
{
	u32 line_stride = 0;
	u32 line_stride_32bytes;
	u32 line_stride_64bytes;

	switch (drm_format) {
	/* 8-bit */
	case DRM_FORMAT_R8:
	case DRM_FORMAT_RGB332:
	case DRM_FORMAT_BGR233:
		line_stride = ((hsize << 3) + 127) >> 7;
		break;

		/* 16-bit */
	case DRM_FORMAT_R16:
	case DRM_FORMAT_RG88:
	case DRM_FORMAT_GR88:
	case DRM_FORMAT_XRGB4444:
	case DRM_FORMAT_XBGR4444:
	case DRM_FORMAT_RGBX4444:
	case DRM_FORMAT_BGRX4444:
	case DRM_FORMAT_ARGB4444:
	case DRM_FORMAT_ABGR4444:
	case DRM_FORMAT_RGBA4444:
	case DRM_FORMAT_BGRA4444:
	case DRM_FORMAT_XRGB1555:
	case DRM_FORMAT_XBGR1555:
	case DRM_FORMAT_RGBX5551:
	case DRM_FORMAT_BGRX5551:
	case DRM_FORMAT_ARGB1555:
	case DRM_FORMAT_ABGR1555:
	case DRM_FORMAT_RGBA5551:
	case DRM_FORMAT_BGRA5551:
	case DRM_FORMAT_RGB565:
	case DRM_FORMAT_BGR565:
		line_stride = ((hsize << 4) + 127) >> 7;
		break;

		/* 24-bit */
	case DRM_FORMAT_RGB888:
	case DRM_FORMAT_BGR888:
		line_stride = ((hsize << 4) + (hsize << 3) + 127) >> 7;
		break;
		/* 32-bit */
	case DRM_FORMAT_XRGB8888:
	case DRM_FORMAT_XBGR8888:
	case DRM_FORMAT_RGBX8888:
	case DRM_FORMAT_BGRX8888:
	case DRM_FORMAT_ARGB8888:
	case DRM_FORMAT_ABGR8888:
	case DRM_FORMAT_RGBA8888:
	case DRM_FORMAT_BGRA8888:
	case DRM_FORMAT_XRGB2101010:
	case DRM_FORMAT_XBGR2101010:
	case DRM_FORMAT_RGBX1010102:
	case DRM_FORMAT_BGRX1010102:
	case DRM_FORMAT_ARGB2101010:
	case DRM_FORMAT_ABGR2101010:
	case DRM_FORMAT_RGBA1010102:
	case DRM_FORMAT_BGRA1010102:
		line_stride = ((hsize << 5) + 127) >> 7;
		break;
	}

	line_stride_32bytes = ((line_stride + 1) >> 1) << 1;
	line_stride_64bytes = ((line_stride + 3) >> 2) << 2;

	/* need wr ddr is 32bytes aligned */
	if (stride_align_32bytes)
		line_stride = line_stride_32bytes;
	else
		line_stride = line_stride_64bytes;

	return line_stride;
}

static void osd_set_state(struct meson_vpu_block *vblk,
			  struct meson_vpu_block_state *state,
			  struct meson_vpu_block_state *old_state)
{
	struct meson_vpu_osd *osd;
	struct meson_vpu_osd_state *mvos, *old_mvos = NULL;
	struct meson_vpu_pipeline *pipe;
	struct meson_vpu_sub_pipeline_state *mvsps;
	struct meson_vpu_pipeline_state *mvps;
	struct rdma_reg_ops *reg_ops;
	int crtc_index;
	u32 pixel_format, canvas_index, src_h, byte_stride, flush_reg;
	struct osd_scope_s scope_src = {0, 1919, 0, 1079};
	struct osd_mif_reg_s *reg;
	bool alpha_div_en = 0, reverse_x, reverse_y, afbc_en;
	u64 phy_addr;
	u16 global_alpha = 256; /*range 0~256*/

	if (!vblk || !state) {
		MESON_DRM_BLOCK("set_state break for NULL.\n");
		return;
	}

	mvos = to_osd_state(state);
	osd = to_osd_block(vblk);
	crtc_index = mvos->crtc_index;
	reg_ops = state->sub->reg_ops;
	pipe = vblk->pipeline;
	mvps = priv_to_pipeline_state(pipe->obj.state);
	mvsps = &mvps->sub_states[0];
	reg = osd->reg;
	if (!reg) {
		MESON_DRM_BLOCK("set_state break for NULL OSD mixer reg.\n");
		return;
	}

	if (old_state)
		old_mvos = to_osd_state(old_state);

	flush_reg = osd_check_config(mvos, old_mvos);
	MESON_DRM_BLOCK("flush_reg-%d index-%d\n", flush_reg, vblk->index);
	if (!flush_reg) {
		/*after v7 chips, always linear addr*/
		if (osd->mif_acc_mode == LINEAR_MIF)
			osd_mem_mode(vblk, reg_ops, reg, 1);

		MESON_DRM_BLOCK("%s-%d not need to flush mif register.\n",
			osd->base.name, vblk->index);
		return;
	}

	afbc_en = mvos->afbc_en ? 1 : 0;
	if (mvos->pixel_blend == DRM_MODE_BLEND_PREMULTI)
		alpha_div_en = 1;

	/*drm alaph 16bit, amlogic alpha 8bit*/
	global_alpha = mvos->global_alpha >> 8;
	if (global_alpha == 0xff)
		global_alpha = 0x100;

	src_h = mvos->src_h + mvos->src_y;
	byte_stride = mvos->byte_stride;
	if (osd->mif_acc_mode == LINEAR_MIF) {
		if (mvsps->more_60)
			byte_stride = line_stride_calc(mvos->pixel_format,
						mvos->src_w * 2, 0);
		else
			byte_stride = line_stride_calc(mvos->pixel_format,
						mvos->src_w, 0);
	}
	phy_addr = mvos->phy_addr;
	scope_src.h_start = mvos->src_x;
	scope_src.h_end = mvos->src_x + mvos->src_w - 1;
	scope_src.v_start = mvos->src_y;
	scope_src.v_end = mvos->src_y + mvos->src_h - 1;
	if (mvsps->more_60) {
		if (vblk->index == OSD1_SLICE0)
			scope_src.h_end = mvos->src_x +
				mvps->scaler_param[vblk->index].input_width - 1;
		if (vblk->index == OSD3_SLICE1)
			scope_src.h_start = scope_src.h_end -
				mvps->scaler_param[vblk->index].input_width + 1;
	}

	pixel_format = mvos->pixel_format;

	reverse_x = (mvos->rotation & DRM_MODE_REFLECT_X) ? 1 : 0;
	reverse_y = (mvos->rotation & DRM_MODE_REFLECT_Y) ? 1 : 0;
	osd_reverse_x_enable(vblk, reg_ops, reg, reverse_x);
	osd_reverse_y_enable(vblk, reg_ops, reg, reverse_y);
	if (osd->mif_acc_mode == LINEAR_MIF) {
		osd_linear_addr_config(vblk, reg_ops, reg, phy_addr, byte_stride);
		MESON_DRM_BLOCK("byte stride=0x%x,phy_addr=0x%pa\n",
			  byte_stride, &phy_addr);
	} else {
		u32 canvas_index_idx = osd_canvas_index[vblk->index];

		canvas_index = osd_canvas[vblk->index][canvas_index_idx];
		canvas_config(canvas_index, phy_addr, byte_stride, src_h,
				CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
		osd_canvas_config(vblk, reg_ops, reg, canvas_index);
		MESON_DRM_BLOCK("canvas_index[%d]=0x%x,phy_addr=0x%pa\n",
			  canvas_index_idx, canvas_index, &phy_addr);
		osd_canvas_index[vblk->index] ^= 1;
	}

	osd_input_size_config(vblk, reg_ops, reg, scope_src);
	osd_color_config(vblk, reg_ops, reg, pixel_format, mvos->pixel_blend, afbc_en);

	if (osd->mif_acc_mode == LINEAR_MIF)
		osd_afbc_config_linear(vblk, reg_ops, reg, vblk->index, afbc_en);
	else
		osd_afbc_config(vblk, reg_ops, reg, vblk->index, afbc_en);

	osd_premult_enable(vblk, reg_ops, reg, alpha_div_en);
	osd_global_alpha_set(vblk, reg_ops, reg, global_alpha);
	osd_scan_mode_config(vblk, reg_ops, reg, pipe->subs[crtc_index].mode.flags &
				 DRM_MODE_FLAG_INTERLACE);
	osd_set_dimm_ctrl(vblk, reg_ops, reg, 0);
	ods_hold_line_config(vblk, reg_ops, reg, osd_hold_line);
	osd_set_two_ports(mvos->read_ports);

	MESON_DRM_BLOCK("plane_index=%d,HW-OSD=%d\n",
		  mvos->plane_index, vblk->index);
	MESON_DRM_BLOCK("scope h/v start/end:[%d/%d/%d/%d]\n",
		  scope_src.h_start, scope_src.h_end,
		scope_src.v_start, scope_src.v_end);
	MESON_DRM_BLOCK("%s set_state done.\n", osd->base.name);
}

static void osd_hw_enable(struct meson_vpu_block *vblk,
			  struct meson_vpu_block_state *state)
{
	struct meson_vpu_osd *osd = to_osd_block(vblk);
	struct osd_mif_reg_s *reg = osd->reg;

	if (!vblk) {
		MESON_DRM_BLOCK("enable break for NULL.\n");
		return;
	}
	osd_block_enable(vblk, state->sub->reg_ops, reg, 1);
	MESON_DRM_BLOCK("%s enable done.\n", osd->base.name);
}

static void osd_hw_disable(struct meson_vpu_block *vblk,
			   struct meson_vpu_block_state *state)
{
	struct meson_vpu_osd *osd;
	struct osd_mif_reg_s *reg;
	u8 version;

	if (!vblk) {
		MESON_DRM_BLOCK("disable break for NULL.\n");
		return;
	}

	osd = to_osd_block(vblk);
	reg = osd->reg;
	version = vblk->pipeline->osd_version;

	/*G12B should always enable,avoid afbc decoder error*/
	if (version != OSD_V2 && version != OSD_V3)
		osd_block_enable(vblk, state->sub->reg_ops, reg, 0);
	MESON_DRM_BLOCK("%s disable done.\n", osd->base.name);
}

static void osd_dump_register(struct meson_vpu_block *vblk,
			      struct seq_file *seq)
{
	int osd_index;
	u32 value;
	char buff[8];
	struct meson_vpu_osd *osd;
	struct osd_mif_reg_s *reg;

	osd_index = vblk->index;
	osd = to_osd_block(vblk);
	reg = osd->reg;

	snprintf(buff, 8, "OSD%d", osd_index + 1);

	value = meson_drm_read_reg(reg->viu_osd_fifo_ctrl_stat);
	seq_printf(seq, "%s_%-35s\t0x%08X\n", buff, "FIFO_CTRL_STAT:", value);

	value = meson_drm_read_reg(reg->viu_osd_ctrl_stat);
	seq_printf(seq, "%s_%-35s\t0x%08X\n", buff, "CTRL_STAT:", value);

	value = meson_drm_read_reg(reg->viu_osd_ctrl_stat2);
	seq_printf(seq, "%s_%-35s\t0x%08X\n", buff, "CTRL_STAT2:", value);

	value = meson_drm_read_reg(reg->viu_osd_blk0_cfg_w0);
	seq_printf(seq, "%s_%-35s\t0x%08X\n", buff, "BLK0_CFG_W0:", value);

	value = meson_drm_read_reg(reg->viu_osd_blk0_cfg_w1);
	seq_printf(seq, "%s_%-35s\t0x%08X\n", buff, "BLK0_CFG_W1:", value);

	value = meson_drm_read_reg(reg->viu_osd_blk0_cfg_w2);
	seq_printf(seq, "%s_%-35s\t0x%08X\n", buff, "BLK0_CFG_W2:", value);

	value = meson_drm_read_reg(reg->viu_osd_blk0_cfg_w3);
	seq_printf(seq, "%s_%-35s\t0x%08X\n", buff, "BLK0_CFG_W3:", value);

	value = meson_drm_read_reg(reg->viu_osd_blk0_cfg_w4);
	seq_printf(seq, "%s_%-35s\t0x%08X\n", buff, "BLK0_CFG_W4:", value);

	value = meson_drm_read_reg(reg->viu_osd_blk1_cfg_w4);
	seq_printf(seq, "%s_%-35s\t0x%08X\n", buff, "BLK1_CFG_W4:", value);

	value = meson_drm_read_reg(reg->viu_osd_blk2_cfg_w4);
	seq_printf(seq, "%s_%-35s\t0x%08X\n", buff, "BLK2_CFG_W4:", value);

	value = meson_drm_read_reg(reg->viu_osd_prot_ctrl);
	seq_printf(seq, "%s_%-35s\t0x%08X\n", buff, "PROT_CTRL:", value);

	value = meson_drm_read_reg(reg->viu_osd_mali_unpack_ctrl);
	seq_printf(seq, "%s_%-35s\t0x%08X\n", buff, "MALI_UNPACK_CTRL:", value);

	value = meson_drm_read_reg(reg->viu_osd_dimm_ctrl);
	seq_printf(seq, "%s_%-35s\t0x%08X\n", buff, "DIMM_CTRL:", value);
}

static void osd_hw_init(struct meson_vpu_block *vblk)
{
	struct meson_vpu_pipeline *pipeline;
	struct meson_vpu_osd *osd = to_osd_block(vblk);

	if (!vblk || !osd) {
		MESON_DRM_BLOCK("hw_init break for NULL.\n");
		return;
	}

	pipeline = osd->base.pipeline;
	if (!pipeline) {
		MESON_DRM_BLOCK("hw_init break for NULL.\n");
		return;
	}

	meson_drm_osd_canvas_alloc();

	osd->reg = &osd_mif_reg[vblk->index];
	osd_ctrl_init(vblk, pipeline->subs[0].reg_ops, osd->reg);
	osd->mif_acc_mode = CANVAS_MODE;

	MESON_DRM_BLOCK("%s hw_init done.\n", osd->base.name);
}

static void t7_osd_hw_init(struct meson_vpu_block *vblk)
{
	struct meson_vpu_pipeline *pipeline;
	struct meson_vpu_osd *osd = to_osd_block(vblk);

	if (!vblk || !osd) {
		MESON_DRM_BLOCK("hw_init break for NULL.\n");
		return;
	}

	pipeline = osd->base.pipeline;
	if (!pipeline) {
		MESON_DRM_BLOCK("hw_init break for NULL.\n");
		return;
	}

	osd->reg = &osd_mif_reg[vblk->index];
	osd_ctrl_init(vblk, pipeline->subs[0].reg_ops, osd->reg);
	osd->mif_acc_mode = LINEAR_MIF;

	MESON_DRM_BLOCK("%s hw_init done.\n", osd->base.name);
}

static void s5_osd_hw_init(struct meson_vpu_block *vblk)
{
	struct meson_vpu_pipeline *pipeline;
	struct meson_vpu_osd *osd = to_osd_block(vblk);

	if (!vblk || !osd) {
		MESON_DRM_BLOCK("hw_init break for NULL.\n");
		return;
	}

	pipeline = osd->base.pipeline;
	if (!pipeline) {
		MESON_DRM_BLOCK("hw_init break for NULL.\n");
		return;
	}

	osd->reg = &s5_osd_mif_reg[vblk->index];
	osd_ctrl_init(vblk, pipeline->subs[0].reg_ops, osd->reg);
	osd->mif_acc_mode = LINEAR_MIF;

	MESON_DRM_BLOCK("%s hw_init done.\n", osd->base.name);
}

static void osd_hw_fini(struct meson_vpu_block *vblk)
{
	struct meson_vpu_osd *osd = to_osd_block(vblk);
	struct meson_vpu_pipeline *pipeline;

	if (!vblk || !osd) {
		MESON_DRM_BLOCK("hw_fini break for NULL.\n");
		return;
	}

	pipeline = osd->base.pipeline;
	if (!pipeline) {
		MESON_DRM_BLOCK("hw_fini break for NULL.\n");
		return;
	}

	if (osd->mif_acc_mode == CANVAS_MODE)
		meson_drm_osd_canvas_free();
}

struct meson_vpu_block_ops osd_ops = {
	.check_state = osd_check_state,
	.update_state = osd_set_state,
	.enable = osd_hw_enable,
	.disable = osd_hw_disable,
	.dump_register = osd_dump_register,
	.init = osd_hw_init,
	.fini = osd_hw_fini,
};

struct meson_vpu_block_ops t7_osd_ops = {
	.check_state = osd_check_state,
	.update_state = osd_set_state,
	.enable = osd_hw_enable,
	.disable = osd_hw_disable,
	.dump_register = osd_dump_register,
	.init = t7_osd_hw_init,
	.fini = osd_hw_fini,
};

struct meson_vpu_block_ops s5_osd_ops = {
	.check_state = osd_check_state,
	.update_state = osd_set_state,
	.enable = osd_hw_enable,
	.disable = osd_hw_disable,
	.dump_register = osd_dump_register,
	.init = s5_osd_hw_init,
	.fini = osd_hw_fini,
};
