// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/bitfield.h>
#include "meson_vpu_pipeline.h"
#include "meson_vpu_reg.h"
#include "meson_vpu_util.h"
#include "meson_osd_afbc.h"
#include "meson_vpu_postblend.h"

#define BYTE_8_ALIGNED(x)	(((x) + 7) & ~7)
#define BYTE_32_ALIGNED(x)	(((x) + 31) & ~31)

/* osd_mafbc_irq_clear& irq_mask */
#define OSD_MAFBC_SURFACES_COMPLETED		BIT(0)
#define OSD_MAFBC_CONFI_SWAPPED				BIT(1)
#define OSD_MAFBC_DECODE_ERROR				BIT(2)
#define OSD_MAFBC_DETILING_ERROR			BIT(3)
#define OSD_MAFBC_AXI_ERROR					BIT(4)
#define OSD_MAFBC_SECURE_ID_ERROR			BIT(5)

/* osd_mafbc_command */
#define OSD_MAFBC_DIRECT_SWAP
#define OSD_MAFBC_PENDING_SWAP

/* osd_mafbc_surface_cfg */
#define OSD_MAFBC_S0_ENABLE					BIT(0)
#define OSD_MAFBC_S1_ENABLE					BIT(1)
#define OSD_MAFBC_S2_ENABLE					BIT(2)
#define OSD_MAFBC_S3_ENABLE					BIT(3)
#define OSD_MAFBC_DECODE_ENABLE				BIT(16)

/* osd_mafbc_axi_cfg */
#define OSD_MAFBC_AXI_QOS(val)		FIELD_PREP(GENMASK(3, 0), val)
#define OSD_MAFBC_AXI_CACHE(val)	FIELD_PREP(GENMASK(7, 4), val)

/* osd_mafbc_format_specifier */
#define OSD_MAFBC_PIXEL_FORMAT(val)	FIELD_PREP(GENMASK(3, 0), val)
#define OSD_MAFBC_YUV_TRANSFORM		BIT(8)
#define OSD_MAFBC_BLOCK_SPLIT		BIT(9)
#define OSD_MAFBC_SUPER_BLOCK_ASPECT(val) \
		FIELD_PREP(GENMASK(17, 16), val)
#define OSD_MAFBC_TILED_HEADER_EN	BIT(18)
#define OSD_MAFBC_PAYLOAD_LIMIT_EN	BIT(19)

/* osd_mafbc_prefetch_cfg */
#define OSD_MAFBC_PREFETCH_READ_DIR_X	BIT(0)
#define OSD_MAFBC_PREFETCH_READ_DIR_Y	BIT(1)

#define MALI_AFBC_REG_BACKUP_COUNT 41

static u32 afbc_set_cnt;
static u32 global_afbc_mask;
static u32 afbc_err_cnt;
static u32 afbc_err_irq_clear;
static u32 afbc_order_conf;
module_param(afbc_order_conf, uint, 0664);
MODULE_PARM_DESC(afbc_order_conf, "afbc order conf");

static struct afbc_osd_reg_s afbc_osd_regs[MESON_MAX_OSDS] = {
	{
		VPU_MAFBC_HEADER_BUF_ADDR_LOW_S0,
		VPU_MAFBC_HEADER_BUF_ADDR_HIGH_S0,
		VPU_MAFBC_FORMAT_SPECIFIER_S0,
		VPU_MAFBC_BUFFER_WIDTH_S0,
		VPU_MAFBC_BUFFER_HEIGHT_S0,
		VPU_MAFBC_BOUNDING_BOX_X_START_S0,
		VPU_MAFBC_BOUNDING_BOX_X_END_S0,
		VPU_MAFBC_BOUNDING_BOX_Y_START_S0,
		VPU_MAFBC_BOUNDING_BOX_Y_END_S0,
		VPU_MAFBC_OUTPUT_BUF_ADDR_LOW_S0,
		VPU_MAFBC_OUTPUT_BUF_ADDR_HIGH_S0,
		VPU_MAFBC_OUTPUT_BUF_STRIDE_S0,
		VPU_MAFBC_PREFETCH_CFG_S0,
	},
	{
		VPU_MAFBC_HEADER_BUF_ADDR_LOW_S1,
		VPU_MAFBC_HEADER_BUF_ADDR_HIGH_S1,
		VPU_MAFBC_FORMAT_SPECIFIER_S1,
		VPU_MAFBC_BUFFER_WIDTH_S1,
		VPU_MAFBC_BUFFER_HEIGHT_S1,
		VPU_MAFBC_BOUNDING_BOX_X_START_S1,
		VPU_MAFBC_BOUNDING_BOX_X_END_S1,
		VPU_MAFBC_BOUNDING_BOX_Y_START_S1,
		VPU_MAFBC_BOUNDING_BOX_Y_END_S1,
		VPU_MAFBC_OUTPUT_BUF_ADDR_LOW_S1,
		VPU_MAFBC_OUTPUT_BUF_ADDR_HIGH_S1,
		VPU_MAFBC_OUTPUT_BUF_STRIDE_S1,
		VPU_MAFBC_PREFETCH_CFG_S1,
	},
	{
		VPU_MAFBC_HEADER_BUF_ADDR_LOW_S2,
		VPU_MAFBC_HEADER_BUF_ADDR_HIGH_S2,
		VPU_MAFBC_FORMAT_SPECIFIER_S2,
		VPU_MAFBC_BUFFER_WIDTH_S2,
		VPU_MAFBC_BUFFER_HEIGHT_S2,
		VPU_MAFBC_BOUNDING_BOX_X_START_S2,
		VPU_MAFBC_BOUNDING_BOX_X_END_S2,
		VPU_MAFBC_BOUNDING_BOX_Y_START_S2,
		VPU_MAFBC_BOUNDING_BOX_Y_END_S2,
		VPU_MAFBC_OUTPUT_BUF_ADDR_LOW_S2,
		VPU_MAFBC_OUTPUT_BUF_ADDR_HIGH_S2,
		VPU_MAFBC_OUTPUT_BUF_STRIDE_S2,
		VPU_MAFBC_PREFETCH_CFG_S2,
	}
};

static struct afbc_status_reg_s afbc_status_regs = {
	VPU_MAFBC_BLOCK_ID,
	VPU_MAFBC_IRQ_RAW_STATUS,
	VPU_MAFBC_IRQ_CLEAR,
	VPU_MAFBC_IRQ_MASK,
	VPU_MAFBC_IRQ_STATUS,
	VPU_MAFBC_COMMAND,
	VPU_MAFBC_STATUS,
	VPU_MAFBC_SURFACE_CFG,
};

static struct afbc_osd_reg_s afbc_osd_t7_regs[MESON_MAX_OSDS] = {
	{
		VPU_MAFBC_HEADER_BUF_ADDR_LOW_S0,
		VPU_MAFBC_HEADER_BUF_ADDR_HIGH_S0,
		VPU_MAFBC_FORMAT_SPECIFIER_S0,
		VPU_MAFBC_BUFFER_WIDTH_S0,
		VPU_MAFBC_BUFFER_HEIGHT_S0,
		VPU_MAFBC_BOUNDING_BOX_X_START_S0,
		VPU_MAFBC_BOUNDING_BOX_X_END_S0,
		VPU_MAFBC_BOUNDING_BOX_Y_START_S0,
		VPU_MAFBC_BOUNDING_BOX_Y_END_S0,
		VPU_MAFBC_OUTPUT_BUF_ADDR_LOW_S0,
		VPU_MAFBC_OUTPUT_BUF_ADDR_HIGH_S0,
		VPU_MAFBC_OUTPUT_BUF_STRIDE_S0,
		VPU_MAFBC_PREFETCH_CFG_S0,
	},
	{
		VPU_MAFBC_HEADER_BUF_ADDR_LOW_S1,
		VPU_MAFBC_HEADER_BUF_ADDR_HIGH_S1,
		VPU_MAFBC_FORMAT_SPECIFIER_S1,
		VPU_MAFBC_BUFFER_WIDTH_S1,
		VPU_MAFBC_BUFFER_HEIGHT_S1,
		VPU_MAFBC_BOUNDING_BOX_X_START_S1,
		VPU_MAFBC_BOUNDING_BOX_X_END_S1,
		VPU_MAFBC_BOUNDING_BOX_Y_START_S1,
		VPU_MAFBC_BOUNDING_BOX_Y_END_S1,
		VPU_MAFBC_OUTPUT_BUF_ADDR_LOW_S1,
		VPU_MAFBC_OUTPUT_BUF_ADDR_HIGH_S1,
		VPU_MAFBC_OUTPUT_BUF_STRIDE_S1,
		VPU_MAFBC_PREFETCH_CFG_S1,
	},
	{
		VPU_MAFBC1_HEADER_BUF_ADDR_LOW_S2,
		VPU_MAFBC1_HEADER_BUF_ADDR_HIGH_S2,
		VPU_MAFBC1_FORMAT_SPECIFIER_S2,
		VPU_MAFBC1_BUFFER_WIDTH_S2,
		VPU_MAFBC1_BUFFER_HEIGHT_S2,
		VPU_MAFBC1_BOUNDING_BOX_X_START_S2,
		VPU_MAFBC1_BOUNDING_BOX_X_END_S2,
		VPU_MAFBC1_BOUNDING_BOX_Y_START_S2,
		VPU_MAFBC1_BOUNDING_BOX_Y_END_S2,
		VPU_MAFBC1_OUTPUT_BUF_ADDR_LOW_S2,
		VPU_MAFBC1_OUTPUT_BUF_ADDR_HIGH_S2,
		VPU_MAFBC1_OUTPUT_BUF_STRIDE_S2,
		VPU_MAFBC1_PREFETCH_CFG_S2,
	},
	{
		VPU_MAFBC2_HEADER_BUF_ADDR_LOW_S3,
		VPU_MAFBC2_HEADER_BUF_ADDR_HIGH_S3,
		VPU_MAFBC2_FORMAT_SPECIFIER_S3,
		VPU_MAFBC2_BUFFER_WIDTH_S3,
		VPU_MAFBC2_BUFFER_HEIGHT_S3,
		VPU_MAFBC2_BOUNDING_BOX_X_START_S3,
		VPU_MAFBC2_BOUNDING_BOX_X_END_S3,
		VPU_MAFBC2_BOUNDING_BOX_Y_START_S3,
		VPU_MAFBC2_BOUNDING_BOX_Y_END_S3,
		VPU_MAFBC2_OUTPUT_BUF_ADDR_LOW_S3,
		VPU_MAFBC2_OUTPUT_BUF_ADDR_HIGH_S3,
		VPU_MAFBC2_OUTPUT_BUF_STRIDE_S3,
		VPU_MAFBC2_PREFETCH_CFG_S3,
	}
};

static struct afbc_status_reg_s afbc_status_t7_regs[3] = {
	{
		VPU_MAFBC_BLOCK_ID,
		VPU_MAFBC_IRQ_RAW_STATUS,
		VPU_MAFBC_IRQ_CLEAR,
		VPU_MAFBC_IRQ_MASK,
		VPU_MAFBC_IRQ_STATUS,
		VPU_MAFBC_COMMAND,
		VPU_MAFBC_STATUS,
		VPU_MAFBC_SURFACE_CFG,
		MALI_AFBCD_TOP_CTRL,
	},
	{
		VPU_MAFBC1_BLOCK_ID,
		VPU_MAFBC1_IRQ_RAW_STATUS,
		VPU_MAFBC1_IRQ_CLEAR,
		VPU_MAFBC1_IRQ_MASK,
		VPU_MAFBC1_IRQ_STATUS,
		VPU_MAFBC1_COMMAND,
		VPU_MAFBC1_STATUS,
		VPU_MAFBC1_SURFACE_CFG,
		MALI_AFBCD1_TOP_CTRL,
	},
	{
		VPU_MAFBC2_BLOCK_ID,
		VPU_MAFBC2_IRQ_RAW_STATUS,
		VPU_MAFBC2_IRQ_CLEAR,
		VPU_MAFBC2_IRQ_MASK,
		VPU_MAFBC2_IRQ_STATUS,
		VPU_MAFBC2_COMMAND,
		VPU_MAFBC2_STATUS,
		VPU_MAFBC2_SURFACE_CFG,
		MALI_AFBCD2_TOP_CTRL,
	},
};

static struct afbc_status_reg_s afbc_status_t3_regs[3] = {
	{
		VPU_MAFBC_BLOCK_ID,
		VPU_MAFBC_IRQ_RAW_STATUS,
		VPU_MAFBC_IRQ_CLEAR,
		VPU_MAFBC_IRQ_MASK,
		VPU_MAFBC_IRQ_STATUS,
		VPU_MAFBC_COMMAND,
		VPU_MAFBC_STATUS,
		VPU_MAFBC_SURFACE_CFG,
		VIU_OSD1_PATH_CTRL,
	},
	{
		VPU_MAFBC1_BLOCK_ID,
		VPU_MAFBC1_IRQ_RAW_STATUS,
		VPU_MAFBC1_IRQ_CLEAR,
		VPU_MAFBC1_IRQ_MASK,
		VPU_MAFBC1_IRQ_STATUS,
		VPU_MAFBC1_COMMAND,
		VPU_MAFBC1_STATUS,
		VPU_MAFBC1_SURFACE_CFG,
		VIU_OSD3_PATH_CTRL,
	},
};

static struct afbc_status_reg_s afbc_status_s5_regs[2] = {
	{
		VPU_MAFBC_BLOCK_ID,
		VPU_MAFBC_IRQ_RAW_STATUS,
		VPU_MAFBC_IRQ_CLEAR,
		VPU_MAFBC_IRQ_MASK,
		VPU_MAFBC_IRQ_STATUS,
		VPU_MAFBC_COMMAND,
		VPU_MAFBC_STATUS,
		VPU_MAFBC_SURFACE_CFG,
		VPP_INTF_OSD1_CTRL,
	},
	{
		VPU_MAFBC1_BLOCK_ID,
		VPU_MAFBC1_IRQ_RAW_STATUS,
		VPU_MAFBC1_IRQ_CLEAR,
		VPU_MAFBC1_IRQ_MASK,
		VPU_MAFBC1_IRQ_STATUS,
		VPU_MAFBC1_COMMAND,
		VPU_MAFBC1_STATUS,
		VPU_MAFBC1_SURFACE_CFG,
		VPP_INTF_OSD3_CTRL,
	},
};
static int afbc_pix_format(u32 fmt_mode)
{
	u32 pix_format = RGBA8888;

	switch (fmt_mode) {
	case DRM_FORMAT_RGB565:
		pix_format = RGB565;
		break;
	case DRM_FORMAT_ARGB1555:
		pix_format = RGBA5551;
		break;
	case DRM_FORMAT_RGBA4444:
	case DRM_FORMAT_ARGB4444:
		pix_format = RGBA4444;
		break;
	case DRM_FORMAT_BGRX8888:
	case DRM_FORMAT_XBGR8888:
	case DRM_FORMAT_RGBX8888:
	case DRM_FORMAT_XRGB8888:
	case DRM_FORMAT_BGRA8888:
	case DRM_FORMAT_ABGR8888:
	case DRM_FORMAT_RGBA8888:
	case DRM_FORMAT_ARGB8888:
		pix_format = RGBA8888;
		break;
	case DRM_FORMAT_RGB888:
	case DRM_FORMAT_BGR888:
		pix_format = RGB888;
		break;
	case DRM_FORMAT_ABGR2101010:
		pix_format = RGBA1010102;
		break;
	default:
		DRM_ERROR("unsupport fmt:%x\n", fmt_mode);
		break;
	}
	return pix_format;
}

static u32 afbc_color_order(u32 fmt_mode)
{
	if (afbc_order_conf)
		return afbc_order_conf;

	switch (fmt_mode) {
	case DRM_FORMAT_BGRX8888:
	case DRM_FORMAT_XBGR8888:
	case DRM_FORMAT_BGRA8888:
	case DRM_FORMAT_ABGR8888:
		return 0x1234;
	case DRM_FORMAT_RGBX8888:
	case DRM_FORMAT_XRGB8888:
	case DRM_FORMAT_RGBA8888:
	case DRM_FORMAT_ARGB8888:
		return 0x3214;
	default:
		return 0x1234;
	}
}

static u32 line_stride_calc_afbc(u32 fmt_mode,
		u32 hsize,
		u32 stride_align_32bytes)
{
	u32 line_stride = 0;

	switch (fmt_mode) {
	case R8:
		line_stride = ((hsize << 3) + 127) >> 7;
		break;
	case YUV422_8B:
	case RGB565:
	case RGBA5551:
	case RGBA4444:
		line_stride = ((hsize << 4) + 127) >> 7;
		break;
	case RGBA8888:
	case RGB888:
	case YUV422_10B:
	case RGBA1010102:
		line_stride = ((hsize << 5) + 127) >> 7;
		break;
	}
	/* need wr ddr is 32bytes aligned */
	if (stride_align_32bytes)
		line_stride = ((line_stride + 1) >> 1) << 1;

	return line_stride;
}

static void osd_afbc_enable(struct meson_vpu_block *vblk,
			    struct rdma_reg_ops *reg_ops, u32 osd_index, bool flag)
{
	if (flag) {
		reg_ops->rdma_write_reg_bits(VPU_MAFBC_SURFACE_CFG,
				1, osd_index, 1);
		global_afbc_mask |= 1 << osd_index;
	} else {
		reg_ops->rdma_write_reg_bits(VPU_MAFBC_SURFACE_CFG,
				0, osd_index, 1);
		global_afbc_mask &= ~(1 << osd_index);
	}
}

static void t7_osd_afbc_enable(struct meson_vpu_block *vblk,
			       struct rdma_reg_ops *reg_ops,
			       struct afbc_status_reg_s *reg, u32 osd_index, bool flag)
{
	if (flag) {
		reg_ops->rdma_write_reg_bits(reg->vpu_mafbc_surface_cfg,
				1, osd_index, 1);
		global_afbc_mask |= 1 << osd_index;
	} else {
		reg_ops->rdma_write_reg_bits(reg->vpu_mafbc_surface_cfg,
				0, osd_index, 1);
		global_afbc_mask &= ~(1 << osd_index);
	}
}

/*only supports one 4k fb at most for osd plane*/
static
int osd_afbc_check_uhd_count(struct meson_vpu_osd_layer_info *plane_info,
			     struct meson_vpu_pipeline_state *mvps)
{
	int cnt;
	int uhd_plane_sum = 0;
	int ret = 0;

	if (plane_info->src_w >= MESON_OSD_INPUT_W_LIMIT &&
		plane_info->src_h >= MESON_OSD_INPUT_H_LIMIT &&
		plane_info->afbc_en) {
		plane_info->uhd_plane_index = 1;
	} else {
		plane_info->uhd_plane_index = 0;
	}
	for (cnt = 0; cnt < MESON_MAX_OSDS; cnt++) {
		uhd_plane_sum += mvps->plane_info[cnt].uhd_plane_index;
		if (uhd_plane_sum > 1)
			return -EINVAL;
	}
	return ret;
}

static int osd_afbc_check_state(struct meson_vpu_block *vblk,
				struct meson_vpu_block_state *state,
		struct meson_vpu_pipeline_state *mvps)
{
	struct meson_vpu_afbc *afbc = to_afbc_block(vblk);
	struct meson_vpu_osd_layer_info *plane_info;
	u32 osd_index;
	int ret;

	osd_index = vblk->index;
	plane_info = &mvps->plane_info[osd_index];

	if (state->checked)
		return 0;

	state->checked = true;
	ret = osd_afbc_check_uhd_count(plane_info, mvps);
	if (ret < 0) {
		DRM_INFO("plane%d,uhd osd plane is greater than 1,return!\n",
			 osd_index);
	}
	MESON_DRM_BLOCK("%s check_state called.\n", afbc->base.name);

	return 0;
}

static void g12a_osd_afbc_set_state(struct meson_vpu_block *vblk,
				    struct meson_vpu_block_state *state,
				    struct meson_vpu_block_state *old_state)
{
	u32 pixel_format, line_stride, output_stride;
	u32 frame_width, frame_height;
	u32 osd_index;
	u64 header_addr, out_addr;
	u32 aligned_32, afbc_color_reorder;
	unsigned int depth;
	int bpp;
	bool reverse_x, reverse_y;

	struct meson_vpu_afbc *afbc;
	struct meson_vpu_afbc_state *afbc_state;
	struct meson_vpu_osd_layer_info *plane_info;
	struct meson_vpu_pipeline *pipeline;
	struct meson_vpu_pipeline_state *pipeline_state;
	struct osd_mif_reg_s *osd_reg;
	struct afbc_osd_reg_s *afbc_reg;
	const struct drm_format_info *info;
	struct rdma_reg_ops *reg_ops;

	afbc = to_afbc_block(vblk);
	afbc_state = to_afbc_state(state);
	pipeline = vblk->pipeline;
	osd_index = vblk->index;
	pipeline_state = priv_to_pipeline_state(pipeline->obj.state);
	osd_reg = pipeline->osds[osd_index]->reg;
	afbc_reg = afbc->afbc_regs;
	plane_info = &pipeline_state->plane_info[osd_index];
	reg_ops = state->sub->reg_ops;

	if (!plane_info->afbc_en) {
		osd_afbc_enable(vblk, reg_ops, osd_index, 0);
		return;
	}

	osd_afbc_enable(vblk, reg_ops, osd_index, 1);

	aligned_32 = 1;
	afbc_color_reorder = afbc_color_order(plane_info->pixel_format);

	pixel_format = afbc_pix_format(plane_info->pixel_format);
	info = drm_format_info(plane_info->pixel_format);
	depth = info->depth;
	bpp = info->cpp[0] * 8;
	header_addr = plane_info->phy_addr;

	frame_width = plane_info->byte_stride / 4;
	frame_height = BYTE_8_ALIGNED(plane_info->fb_h);
	line_stride = line_stride_calc_afbc(pixel_format,
					    frame_width, aligned_32);

	output_stride = frame_width * bpp / 8;

	header_addr = plane_info->phy_addr;
	out_addr = ((u64)(vblk->index + 1)) << 24;
	reverse_x = (plane_info->rotation & DRM_MODE_REFLECT_X) ? 1 : 0;
	reverse_y = (plane_info->rotation & DRM_MODE_REFLECT_Y) ? 1 : 0;

	/* set osd path misc ctrl */
	reg_ops->rdma_write_reg_bits(OSD_PATH_MISC_CTRL, 0x1,
				(osd_index + 4), 1);

	/* set linear addr */
	reg_ops->rdma_write_reg_bits(osd_reg->viu_osd_ctrl_stat, 0x1, 2, 1);

	/* set read from mali */
	reg_ops->rdma_write_reg_bits(osd_reg->viu_osd_blk0_cfg_w0, 0x1, 30, 1);
	reg_ops->rdma_write_reg_bits(osd_reg->viu_osd_blk0_cfg_w0, 0, 15, 1);

	/* set line_stride */
	reg_ops->rdma_write_reg_bits(osd_reg->viu_osd_blk2_cfg_w4,
				 line_stride, 0, 12);

	/* set frame addr */
	reg_ops->rdma_write_reg(osd_reg->viu_osd_blk1_cfg_w4,
			out_addr & 0xffffffff);

	/* set afbc color reorder and mali src*/
	reg_ops->rdma_write_reg_bits(osd_reg->viu_osd_mali_unpack_ctrl,
				 afbc_color_reorder, 0, 16);
	reg_ops->rdma_write_reg_bits(osd_reg->viu_osd_mali_unpack_ctrl,
				 0x1, 31, 1);

	/* set header addr */
	reg_ops->rdma_write_reg(afbc_reg->vpu_mafbc_header_buf_addr_low_s,
			    header_addr & 0xffffffff);
	reg_ops->rdma_write_reg(afbc_reg->vpu_mafbc_header_buf_addr_high_s,
			    (header_addr >> 32) & 0xffffffff);

	/* set format specifier */
	reg_ops->rdma_write_reg(afbc_reg->vpu_mafbc_format_specifier_s,
			    plane_info->afbc_inter_format |
			    (pixel_format & 0x0f));

	/* set pic size */
	reg_ops->rdma_write_reg(afbc_reg->vpu_mafbc_buffer_width_s,
			    frame_width);
	reg_ops->rdma_write_reg(afbc_reg->vpu_mafbc_buffer_height_s,
			    frame_height);

	/* set buf stride */
	reg_ops->rdma_write_reg(afbc_reg->vpu_mafbc_output_buf_stride_s,
			    output_stride);
	reg_ops->rdma_write_reg(afbc_reg->vpu_mafbc_output_buf_addr_low_s,
			    out_addr & 0xffffffff);
	reg_ops->rdma_write_reg(afbc_reg->vpu_mafbc_output_buf_addr_high_s,
			    (out_addr >> 32) & 0xffffffff);

	/* set bounding box */
	reg_ops->rdma_write_reg(afbc_reg->vpu_mafbc_bounding_box_x_start_s,
			    plane_info->src_x);
	reg_ops->rdma_write_reg(afbc_reg->vpu_mafbc_bounding_box_x_end_s,
			    (plane_info->src_x + plane_info->src_w - 1));
	reg_ops->rdma_write_reg(afbc_reg->vpu_mafbc_bounding_box_y_start_s,
			    plane_info->src_y);
	reg_ops->rdma_write_reg(afbc_reg->vpu_mafbc_bounding_box_y_end_s,
			    (plane_info->src_y + plane_info->src_h - 1));

	/*reverse config*/
	reg_ops->rdma_write_reg_bits(afbc_reg->vpu_mafbc_prefetch_cfg_s,
				 reverse_x, 0, 1);
	reg_ops->rdma_write_reg_bits(afbc_reg->vpu_mafbc_prefetch_cfg_s,
				 reverse_y, 1, 1);

	MESON_DRM_BLOCK("%s set_state called.\n", afbc->base.name);
}

static void t7_osd_afbc_set_state(struct meson_vpu_block *vblk,
				  struct meson_vpu_block_state *state,
				  struct meson_vpu_block_state *old_state)
{
	int i, start, end, core_enable;
	u32 pixel_format, line_stride, output_stride;
	u32 frame_width, frame_height;
	u32 osd_index, afbc_index;
	u64 header_addr, out_addr;
	u32 aligned_32, afbc_color_reorder;
	unsigned int depth;
	int bpp;
	bool reverse_x, reverse_y;

	struct meson_vpu_afbc *afbc;
	struct meson_vpu_afbc_state *afbc_state;
	struct meson_vpu_osd_layer_info *plane_info;
	struct meson_vpu_pipeline *pipeline;
	struct meson_vpu_pipeline_state *mvps;
	struct osd_mif_reg_s *osd_reg;
	struct afbc_osd_reg_s *afbc_reg;
	struct afbc_status_reg_s *afbc_stat_reg;
	const struct drm_format_info *info;
	struct rdma_reg_ops *reg_ops;

	afbc = to_afbc_block(vblk);
	afbc_state = to_afbc_state(state);
	pipeline = vblk->pipeline;
	afbc_index = vblk->index;
	mvps = priv_to_pipeline_state(pipeline->obj.state);
	reg_ops = state->sub->reg_ops;
	core_enable = 0;

	if (afbc_index == 0) {
		start = 0;
		end = 1;
	} else if (afbc_index == 1) {
		start = 2;
		end = 2;
	} else if (afbc_index == 2) {
		start = 3;
		end = 3;
	} else {
		start = 0;
		end = 0;
	}

	afbc_stat_reg = afbc->status_regs;

	for (i = start; i <= end; i++) {
		if (mvps->plane_info[i].enable && mvps->plane_info[i].afbc_en) {
			core_enable = 1;
			osd_index = i;
			osd_reg = pipeline->osds[osd_index]->reg;
			afbc_reg = &afbc->afbc_regs[osd_index];
			plane_info = &mvps->plane_info[osd_index];

			t7_osd_afbc_enable(vblk, reg_ops, afbc_stat_reg, osd_index, 1);

			aligned_32 = 1;
			afbc_color_reorder = afbc_color_order(plane_info->pixel_format);

			pixel_format = afbc_pix_format(plane_info->pixel_format);
			info = drm_format_info(plane_info->pixel_format);
			depth = info->depth;
			bpp = info->cpp[0] * 8;
			header_addr = plane_info->phy_addr;

			frame_width = plane_info->byte_stride / 4;
			frame_height = BYTE_8_ALIGNED(plane_info->fb_h);
			line_stride = line_stride_calc_afbc(pixel_format,
							frame_width, aligned_32);

			output_stride = frame_width * bpp / 8;

			header_addr = plane_info->phy_addr;
			out_addr = ((u64)(osd_index + 1)) << 24;
			reverse_x = (plane_info->rotation & DRM_MODE_REFLECT_X) ? 1 : 0;
			reverse_y = (plane_info->rotation & DRM_MODE_REFLECT_Y) ? 1 : 0;

			/* set osd path misc ctrl */
			reg_ops->rdma_write_reg_bits(OSD_PATH_MISC_CTRL, (osd_index + 1),
				(osd_index * 4 + 16), 4);

			/* set linear addr */
			reg_ops->rdma_write_reg_bits(osd_reg->viu_osd_ctrl_stat, 0x1, 2, 1);

			/* set read from mali */
			reg_ops->rdma_write_reg_bits(osd_reg->viu_osd_blk0_cfg_w0, 0x1, 30, 1);
			reg_ops->rdma_write_reg_bits(osd_reg->viu_osd_blk0_cfg_w0, 0, 15, 1);

			/* set line_stride */
			reg_ops->rdma_write_reg_bits(osd_reg->viu_osd_blk2_cfg_w4,
					line_stride, 0, 12);

			/* set frame addr */
			reg_ops->rdma_write_reg(osd_reg->viu_osd_blk1_cfg_w4,
					(out_addr >> 4) & 0xffffffff);

			/* set afbc color reorder and mali src*/
			reg_ops->rdma_write_reg_bits(osd_reg->viu_osd_mali_unpack_ctrl,
						afbc_color_reorder, 0, 16);
			reg_ops->rdma_write_reg_bits(osd_reg->viu_osd_mali_unpack_ctrl,
						0x1, 31, 1);

			/* set header addr */
			reg_ops->rdma_write_reg(afbc_reg->vpu_mafbc_header_buf_addr_low_s,
					header_addr & 0xffffffff);
			reg_ops->rdma_write_reg(afbc_reg->vpu_mafbc_header_buf_addr_high_s,
						(header_addr >> 32) & 0xffffffff);

			/* set format specifier */
			reg_ops->rdma_write_reg(afbc_reg->vpu_mafbc_format_specifier_s,
						plane_info->afbc_inter_format |
						(pixel_format & 0x0f));

			/* set pic size */
			reg_ops->rdma_write_reg(afbc_reg->vpu_mafbc_buffer_width_s,
					frame_width);
			reg_ops->rdma_write_reg(afbc_reg->vpu_mafbc_buffer_height_s,
					frame_height);

			/* set buf stride */
			reg_ops->rdma_write_reg(afbc_reg->vpu_mafbc_output_buf_stride_s,
					output_stride);
			reg_ops->rdma_write_reg(afbc_reg->vpu_mafbc_output_buf_addr_low_s,
					out_addr & 0xffffffff);
			reg_ops->rdma_write_reg(afbc_reg->vpu_mafbc_output_buf_addr_high_s,
					(out_addr >> 32) & 0xffffffff);

			/* set bounding box */
			reg_ops->rdma_write_reg(afbc_reg->vpu_mafbc_bounding_box_x_start_s,
					plane_info->src_x);
			reg_ops->rdma_write_reg(afbc_reg->vpu_mafbc_bounding_box_x_end_s,
					(plane_info->src_x + plane_info->src_w - 1));
			reg_ops->rdma_write_reg(afbc_reg->vpu_mafbc_bounding_box_y_start_s,
					plane_info->src_y);
			reg_ops->rdma_write_reg(afbc_reg->vpu_mafbc_bounding_box_y_end_s,
					(plane_info->src_y + plane_info->src_h - 1));

			/*reverse config*/
			reg_ops->rdma_write_reg_bits(afbc_reg->vpu_mafbc_prefetch_cfg_s,
						reverse_x, 0, 1);
			reg_ops->rdma_write_reg_bits(afbc_reg->vpu_mafbc_prefetch_cfg_s,
						reverse_y, 1, 1);
			if (osd_index == 0)
				reg_ops->rdma_write_reg_bits(afbc_stat_reg->mali_afbcd_top_ctrl,
							 1, 16, 1);
			else if (osd_index == 1)
				reg_ops->rdma_write_reg_bits(afbc_stat_reg->mali_afbcd_top_ctrl,
							 1, 21, 1);
			else if (osd_index == 2)
				reg_ops->rdma_write_reg_bits(afbc_stat_reg->mali_afbcd_top_ctrl,
							 1, 21, 1);
			else if (osd_index == 3)
				reg_ops->rdma_write_reg_bits(afbc_stat_reg->mali_afbcd_top_ctrl,
							 1, 21, 1);
			else
				MESON_DRM_BLOCK("%s, invalid afbc top ctrl index\n", __func__);
		} else {
			t7_osd_afbc_enable(vblk, reg_ops, afbc_stat_reg, i, 0);
		}
	}

	if (core_enable) {
		reg_ops->rdma_write_reg(afbc_stat_reg->vpu_mafbc_irq_mask, 0xf);
		reg_ops->rdma_write_reg(afbc_stat_reg->vpu_mafbc_irq_clear, 0x3f);
		reg_ops->rdma_write_reg(afbc_stat_reg->vpu_mafbc_command, 1);
		mvps->global_afbc = 1;
		afbc_err_irq_clear = 1;
	}

	MESON_DRM_BLOCK("%s set_state called.\n", afbc->base.name);
}

static void t3_osd_afbc_set_state(struct meson_vpu_block *vblk,
				  struct meson_vpu_block_state *state,
				  struct meson_vpu_block_state *old_state)
{
	int i, start, end, core_enable;
	u32 pixel_format, line_stride, output_stride;
	u32 frame_width, frame_height;
	u32 osd_index, afbc_index;
	u64 header_addr, out_addr;
	u32 aligned_32, afbc_color_reorder;
	unsigned int depth;
	int bpp;
	bool reverse_x, reverse_y;

	struct meson_vpu_afbc *afbc;
	struct meson_vpu_afbc_state *afbc_state;
	struct meson_vpu_osd_layer_info *plane_info;
	struct meson_vpu_pipeline *pipeline;
	struct meson_vpu_pipeline_state *mvps;
	struct meson_vpu_sub_pipeline_state *mvsps;
	struct osd_mif_reg_s *osd_reg;
	struct afbc_osd_reg_s *afbc_reg;
	struct afbc_status_reg_s *afbc_stat_reg;
	const struct drm_format_info *info;
	struct rdma_reg_ops *reg_ops;

	afbc = to_afbc_block(vblk);
	afbc_state = to_afbc_state(state);
	pipeline = vblk->pipeline;
	afbc_index = vblk->index;
	mvps = priv_to_pipeline_state(pipeline->obj.state);
	mvsps = &mvps->sub_states[0];
	reg_ops = state->sub->reg_ops;

	if (afbc_index == 0) {
		start = 0;
		end = 1;
	} else if (afbc_index == 1) {
		start = 2;
		end = 2;
	} else if (afbc_index == 2) {
		start = 3;
		end = 3;
	} else {
		start = 0;
		end = 0;
	}

	afbc_stat_reg = afbc->status_regs;
	core_enable = 0;

	for (i = start; i <= end; i++) {
		if (mvps->plane_info[i].enable && mvps->plane_info[i].afbc_en) {
			core_enable = 1;
			osd_index = i;
			osd_reg = pipeline->osds[osd_index]->reg;
			afbc_reg = &afbc->afbc_regs[osd_index];
			plane_info = &mvps->plane_info[osd_index];

			t7_osd_afbc_enable(vblk, reg_ops, afbc_stat_reg, osd_index, 1);

			aligned_32 = 1;
			afbc_color_reorder = afbc_color_order(plane_info->pixel_format);

			pixel_format = afbc_pix_format(plane_info->pixel_format);
			info = drm_format_info(plane_info->pixel_format);
			depth = info->depth;
			bpp = info->cpp[0] * 8;
			header_addr = plane_info->phy_addr;

			frame_width = plane_info->byte_stride / 4;
			frame_height = BYTE_8_ALIGNED(plane_info->fb_h);
			line_stride = line_stride_calc_afbc(pixel_format,
							frame_width, aligned_32);

			output_stride = frame_width * bpp / 8;

			header_addr = plane_info->phy_addr;
			out_addr = ((u64)(osd_index + 1)) << 24;
			reverse_x = (plane_info->rotation & DRM_MODE_REFLECT_X) ? 1 : 0;
			reverse_y = (plane_info->rotation & DRM_MODE_REFLECT_Y) ? 1 : 0;

			/* set osd path misc ctrl */
			reg_ops->rdma_write_reg_bits(OSD_PATH_MISC_CTRL, (osd_index + 1),
				(osd_index * 4 + 16), 4);

			/* set linear addr */
			reg_ops->rdma_write_reg_bits(osd_reg->viu_osd_ctrl_stat, 0x1, 2, 1);

			/* set read from mali */
			reg_ops->rdma_write_reg_bits(osd_reg->viu_osd_blk0_cfg_w0, 0x1, 30, 1);
			reg_ops->rdma_write_reg_bits(osd_reg->viu_osd_blk0_cfg_w0, 0, 15, 1);

			/* set line_stride */
			reg_ops->rdma_write_reg_bits(osd_reg->viu_osd_blk2_cfg_w4,
					line_stride, 0, 12);

			/* set frame addr */
			reg_ops->rdma_write_reg(osd_reg->viu_osd_blk1_cfg_w4,
					(out_addr >> 4) & 0xffffffff);

			/* set afbc color reorder and mali src*/
			reg_ops->rdma_write_reg_bits(osd_reg->viu_osd_mali_unpack_ctrl,
						afbc_color_reorder, 0, 16);
			reg_ops->rdma_write_reg_bits(osd_reg->viu_osd_mali_unpack_ctrl,
						0x1, 31, 1);

			/* set header addr */
			reg_ops->rdma_write_reg(afbc_reg->vpu_mafbc_header_buf_addr_low_s,
					header_addr & 0xffffffff);
			reg_ops->rdma_write_reg(afbc_reg->vpu_mafbc_header_buf_addr_high_s,
						(header_addr >> 32) & 0xffffffff);

			/* set format specifier */
			reg_ops->rdma_write_reg(afbc_reg->vpu_mafbc_format_specifier_s,
						plane_info->afbc_inter_format |
						(pixel_format & 0x0f));

			/* set pic size */
			reg_ops->rdma_write_reg(afbc_reg->vpu_mafbc_buffer_width_s,
					frame_width);
			reg_ops->rdma_write_reg(afbc_reg->vpu_mafbc_buffer_height_s,
					frame_height);

			/* set buf stride */
			reg_ops->rdma_write_reg(afbc_reg->vpu_mafbc_output_buf_stride_s,
					output_stride);
			reg_ops->rdma_write_reg(afbc_reg->vpu_mafbc_output_buf_addr_low_s,
					out_addr & 0xffffffff);
			reg_ops->rdma_write_reg(afbc_reg->vpu_mafbc_output_buf_addr_high_s,
					(out_addr >> 32) & 0xffffffff);

			/* set bounding box */
			reg_ops->rdma_write_reg(afbc_reg->vpu_mafbc_bounding_box_x_start_s,
						plane_info->src_x);
			reg_ops->rdma_write_reg(afbc_reg->vpu_mafbc_bounding_box_x_end_s,
						(plane_info->src_x + plane_info->src_w - 1));
			reg_ops->rdma_write_reg(afbc_reg->vpu_mafbc_bounding_box_y_start_s,
						plane_info->src_y);
			reg_ops->rdma_write_reg(afbc_reg->vpu_mafbc_bounding_box_y_end_s,
						(plane_info->src_y + plane_info->src_h - 1));
			if (mvsps->more_60 && osd_index == OSD1_SLICE0)
				reg_ops->rdma_write_reg(afbc_reg->vpu_mafbc_bounding_box_x_end_s,
							plane_info->src_x - 1 +
							mvps->scaler_param[osd_index].input_width);
			if (mvsps->more_60 && osd_index == OSD3_SLICE1)
				reg_ops->rdma_write_reg(afbc_reg->vpu_mafbc_bounding_box_x_start_s,
							plane_info->src_x + plane_info->src_w -
							mvps->scaler_param[osd_index].input_width);

			/*reverse config*/
			reg_ops->rdma_write_reg_bits(afbc_reg->vpu_mafbc_prefetch_cfg_s,
						reverse_x, 0, 1);
			reg_ops->rdma_write_reg_bits(afbc_reg->vpu_mafbc_prefetch_cfg_s,
						reverse_y, 1, 1);
			if (osd_index == 0)
				reg_ops->rdma_write_reg_bits(VIU_OSD1_PATH_CTRL, 1, 31, 1);
			else if (osd_index == 1)
				reg_ops->rdma_write_reg_bits(VIU_OSD2_PATH_CTRL, 1, 31, 1);
			else if (osd_index == 2)
				reg_ops->rdma_write_reg_bits(VIU_OSD3_PATH_CTRL, 1, 31, 1);
			else
				MESON_DRM_BLOCK("%s, invalid afbc top ctrl index\n", __func__);
		} else {
			if (i == 0)
				meson_vpu_write_reg_bits(VIU_OSD1_PATH_CTRL, 0, 31, 1);
			else if (i == 1)
				meson_vpu_write_reg_bits(VIU_OSD2_PATH_CTRL, 0, 31, 1);
			else if (i == 2)
				meson_vpu_write_reg_bits(VIU_OSD3_PATH_CTRL, 0, 31, 1);
			else
				MESON_DRM_BLOCK("%s, invalid afbc top ctrl index\n", __func__);

			t7_osd_afbc_enable(vblk, reg_ops, afbc_stat_reg, i, 0);
		}
	}

	if (core_enable) {
		reg_ops->rdma_write_reg(afbc_stat_reg->vpu_mafbc_irq_mask, 0xf);
		reg_ops->rdma_write_reg(afbc_stat_reg->vpu_mafbc_irq_clear, 0x3f);
		reg_ops->rdma_write_reg(afbc_stat_reg->vpu_mafbc_command, 1);
		mvps->global_afbc = 1;
		afbc_err_irq_clear = 1;
	}

	MESON_DRM_BLOCK("%s set_state called.\n", afbc->base.name);
}

static void s5_osd_afbc_set_state(struct meson_vpu_block *vblk,
				  struct meson_vpu_block_state *state,
				  struct meson_vpu_block_state *old_state)
{
	int i, start, end, core_enable;
	u32 pixel_format, line_stride, output_stride;
	u32 frame_width, frame_height;
	u32 osd_index, afbc_index;
	u64 header_addr, out_addr;
	u32 aligned_32, afbc_color_reorder;
	unsigned int depth;
	int bpp;
	bool reverse_x, reverse_y;

	struct meson_vpu_afbc *afbc;
	struct meson_vpu_afbc_state *afbc_state;
	struct meson_vpu_osd_layer_info *plane_info;
	struct meson_vpu_pipeline *pipeline;
	struct meson_vpu_pipeline_state *mvps;
	struct meson_vpu_sub_pipeline_state *mvsps;
	struct osd_mif_reg_s *osd_reg;
	struct afbc_osd_reg_s *afbc_reg;
	struct afbc_status_reg_s *afbc_stat_reg;
	const struct drm_format_info *info;
	struct rdma_reg_ops *reg_ops;

	afbc = to_afbc_block(vblk);
	afbc_state = to_afbc_state(state);
	pipeline = vblk->pipeline;
	afbc_index = vblk->index;
	mvps = priv_to_pipeline_state(pipeline->obj.state);
	mvsps = &mvps->sub_states[0];
	reg_ops = state->sub->reg_ops;

	if (afbc_index == 0) {
		start = 0;
		end = 1;
	} else if (afbc_index == 1) {
		start = 2;
		end = 2;
	} else if (afbc_index == 2) {
		start = 3;
		end = 3;
	} else {
		start = 0;
		end = 0;
	}

	afbc_stat_reg = afbc->status_regs;
	core_enable = 0;

	for (i = start; i <= end; i++) {
		if (mvps->plane_info[i].enable && mvps->plane_info[i].afbc_en) {
			core_enable = 1;
			osd_index = i;
			osd_reg = pipeline->osds[osd_index]->reg;
			afbc_reg = &afbc->afbc_regs[osd_index];
			plane_info = &mvps->plane_info[osd_index];

			t7_osd_afbc_enable(vblk, reg_ops, afbc_stat_reg, osd_index, 1);

			aligned_32 = 1;
			afbc_color_reorder = afbc_color_order(plane_info->pixel_format);

			pixel_format = afbc_pix_format(plane_info->pixel_format);
			info = drm_format_info(plane_info->pixel_format);
			depth = info->depth;
			bpp = info->cpp[0] * 8;
			header_addr = plane_info->phy_addr;

			frame_width = plane_info->byte_stride / 4;
			frame_height = BYTE_8_ALIGNED(plane_info->fb_h);
			line_stride = line_stride_calc_afbc(pixel_format,
							frame_width, aligned_32);

			output_stride = frame_width * bpp / 8;

			header_addr = plane_info->phy_addr;
			out_addr = ((u64)(osd_index + 1)) << 24;
			reverse_x = (plane_info->rotation & DRM_MODE_REFLECT_X) ? 1 : 0;
			reverse_y = (plane_info->rotation & DRM_MODE_REFLECT_Y) ? 1 : 0;

			/* set osd path misc ctrl */
			reg_ops->rdma_write_reg_bits(OSD_PATH_MISC_CTRL, (osd_index + 1),
				(osd_index * 4 + 16), 4);

			/* set linear addr */
			reg_ops->rdma_write_reg_bits(osd_reg->viu_osd_ctrl_stat, 0x1, 2, 1);

			/* set read from mali */
			reg_ops->rdma_write_reg_bits(osd_reg->viu_osd_blk0_cfg_w0, 0x1, 30, 1);
			reg_ops->rdma_write_reg_bits(osd_reg->viu_osd_blk0_cfg_w0, 0, 15, 1);

			/* set line_stride */
			reg_ops->rdma_write_reg_bits(osd_reg->viu_osd_blk2_cfg_w4,
					line_stride, 0, 12);

			/* set frame addr */
			reg_ops->rdma_write_reg(osd_reg->viu_osd_blk1_cfg_w4,
					(out_addr >> 4) & 0xffffffff);

			/* set afbc color reorder and mali src*/
			reg_ops->rdma_write_reg_bits(osd_reg->viu_osd_mali_unpack_ctrl,
						afbc_color_reorder, 0, 16);
			reg_ops->rdma_write_reg_bits(osd_reg->viu_osd_mali_unpack_ctrl,
						0x1, 31, 1);

			/* set header addr */
			reg_ops->rdma_write_reg(afbc_reg->vpu_mafbc_header_buf_addr_low_s,
					header_addr & 0xffffffff);
			reg_ops->rdma_write_reg(afbc_reg->vpu_mafbc_header_buf_addr_high_s,
						(header_addr >> 32) & 0xffffffff);

			/* set format specifier */
			reg_ops->rdma_write_reg(afbc_reg->vpu_mafbc_format_specifier_s,
						plane_info->afbc_inter_format |
						(pixel_format & 0x0f));

			/* set pic size */
			reg_ops->rdma_write_reg(afbc_reg->vpu_mafbc_buffer_width_s,
					frame_width);
			reg_ops->rdma_write_reg(afbc_reg->vpu_mafbc_buffer_height_s,
					frame_height);

			/* set buf stride */
			reg_ops->rdma_write_reg(afbc_reg->vpu_mafbc_output_buf_stride_s,
					output_stride);
			reg_ops->rdma_write_reg(afbc_reg->vpu_mafbc_output_buf_addr_low_s,
					out_addr & 0xffffffff);
			reg_ops->rdma_write_reg(afbc_reg->vpu_mafbc_output_buf_addr_high_s,
					(out_addr >> 32) & 0xffffffff);

			/* set bounding box */
			reg_ops->rdma_write_reg(afbc_reg->vpu_mafbc_bounding_box_x_start_s,
						plane_info->src_x);
			reg_ops->rdma_write_reg(afbc_reg->vpu_mafbc_bounding_box_x_end_s,
						(plane_info->src_x + plane_info->src_w - 1));
			reg_ops->rdma_write_reg(afbc_reg->vpu_mafbc_bounding_box_y_start_s,
						plane_info->src_y);
			reg_ops->rdma_write_reg(afbc_reg->vpu_mafbc_bounding_box_y_end_s,
						(plane_info->src_y + plane_info->src_h - 1));
			if (mvsps->more_60 && osd_index == OSD1_SLICE0)
				reg_ops->rdma_write_reg(afbc_reg->vpu_mafbc_bounding_box_x_end_s,
							plane_info->src_x - 1 +
							mvps->scaler_param[osd_index].input_width);
			if (mvsps->more_60 && osd_index == OSD3_SLICE1)
				reg_ops->rdma_write_reg(afbc_reg->vpu_mafbc_bounding_box_x_start_s,
							plane_info->src_x + plane_info->src_w -
							mvps->scaler_param[osd_index].input_width);

			/*reverse config*/
			reg_ops->rdma_write_reg_bits(afbc_reg->vpu_mafbc_prefetch_cfg_s,
						reverse_x, 0, 1);
			reg_ops->rdma_write_reg_bits(afbc_reg->vpu_mafbc_prefetch_cfg_s,
						reverse_y, 1, 1);
			if (osd_index == 0)
				meson_vpu_write_reg_bits(VPP_INTF_OSD1_CTRL, 1, 0, 1);
			else if (osd_index == 1)
				meson_vpu_write_reg_bits(VPP_INTF_OSD2_CTRL, 1, 0, 1);
			else if (osd_index == 2)
				meson_vpu_write_reg_bits(VPP_INTF_OSD3_CTRL, 1, 0, 1);
			else
				MESON_DRM_BLOCK("%s, invalid afbc top ctrl index\n", __func__);
		} else {
			if (i == 0)
				meson_vpu_write_reg_bits(VPP_INTF_OSD1_CTRL, 0, 0, 1);
			else if (i == 2)
				meson_vpu_write_reg_bits(VPP_INTF_OSD3_CTRL, 0, 0, 1);
			else
				MESON_DRM_BLOCK("%s, invalid afbc top ctrl index\n", __func__);

			t7_osd_afbc_enable(vblk, reg_ops, afbc_stat_reg, i, 0);
		}
	}

	if (core_enable) {
		reg_ops->rdma_write_reg(afbc_stat_reg->vpu_mafbc_irq_mask, 0xf);
		reg_ops->rdma_write_reg(afbc_stat_reg->vpu_mafbc_irq_clear, 0x3f);
		reg_ops->rdma_write_reg(afbc_stat_reg->vpu_mafbc_command, 1);
		mvps->global_afbc = 1;
		afbc_err_irq_clear = 1;
	}

	MESON_DRM_BLOCK("%s set_state called.\n", afbc->base.name);
}

static void osd_afbc_dump_register(struct meson_vpu_block *vblk,
				   struct seq_file *seq)
{
	int osd_index;
	u32 value;
	char buff[8];
	struct meson_vpu_afbc *afbc;
	struct afbc_osd_reg_s *reg;

	osd_index = vblk->index;
	afbc = to_afbc_block(vblk);
	reg = afbc->afbc_regs;

	snprintf(buff, 8, "OSD%d", osd_index + 1);
	seq_printf(seq, "afbc error [%d]\n", afbc_err_cnt);

	value = meson_drm_read_reg(VPU_MAFBC_SURFACE_CFG);
	seq_printf(seq, "%s_%-35s\t0x%08X\n", buff, "VPU_MAFBC_SURFACE_CFG:",
		   value);

	value = meson_drm_read_reg(reg->vpu_mafbc_header_buf_addr_low_s);
	seq_printf(seq, "%s_%-35s\t0x%08X\n", buff, "AFBC_HEADER_BUF_ADDR_LOW:",
		   value);

	value = meson_drm_read_reg(reg->vpu_mafbc_header_buf_addr_high_s);
	seq_printf(seq, "%s_%-35s\t0x%08X\n", buff,
		   "AFBC_HEADER_BUF_ADDR_HIGH:", value);

	value = meson_drm_read_reg(reg->vpu_mafbc_format_specifier_s);
	seq_printf(seq, "%s_%-35s\t0x%08X\n", buff, "AFBC_FORMAT_SPECIFIER:",
		   value);

	value = meson_drm_read_reg(reg->vpu_mafbc_buffer_width_s);
	seq_printf(seq, "%s_%-35s\t0x%08X\n", buff, "AFBC_BUFFER_WIDTH:",
		   value);

	value = meson_drm_read_reg(reg->vpu_mafbc_buffer_height_s);
	seq_printf(seq, "%s_%-35s\t0x%08X\n", buff, "AFBC_BUFFER_HEIGHT:",
		   value);

	value = meson_drm_read_reg(reg->vpu_mafbc_bounding_box_x_start_s);
	seq_printf(seq, "%s_%-35s\t0x%08X\n", buff,
		   "AFBC_BOUNDINGS_BOX_X_START:", value);

	value = meson_drm_read_reg(reg->vpu_mafbc_bounding_box_x_end_s);
	seq_printf(seq, "%s_%-35s\t0x%08X\n", buff, "AFBC_BOUNDINGS_BOX_X_END:",
		   value);

	value = meson_drm_read_reg(reg->vpu_mafbc_bounding_box_y_start_s);
	seq_printf(seq, "%s_%-35s\t0x%08X\n", buff,
		   "AFBC_BOUNDINGS_BOX_Y_START:", value);

	value = meson_drm_read_reg(reg->vpu_mafbc_bounding_box_y_end_s);
	seq_printf(seq, "%s_%-35s\t0x%08X\n", buff,
		   "AFBC_BOUNDINGS_BOX_Y_END:", value);

	value = meson_drm_read_reg(reg->vpu_mafbc_output_buf_addr_low_s);
	seq_printf(seq, "%s_%-35s\t0x%08X\n", buff,
		   "AFBC_OUTPUT_BUF_ADDR_LOW:", value);

	value = meson_drm_read_reg(reg->vpu_mafbc_output_buf_addr_high_s);
	seq_printf(seq, "%s_%-35s\t0x%08X\n", buff,
		   "AFBC_OUTPUT_BUF_ADDR_HIGH:", value);

	value = meson_drm_read_reg(reg->vpu_mafbc_output_buf_stride_s);
	seq_printf(seq, "%s_%-35s\t0x%08X\n", buff,
		   "AFBC_OUTPUT_BUF_STRIDE:", value);

	value = meson_drm_read_reg(reg->vpu_mafbc_prefetch_cfg_s);
	seq_printf(seq, "%s_%-35s\t0x%08X\n", buff, "AFBC_PREFETCH_CFG:",
		   value);
}

static void osd_afbc_hw_enable(struct meson_vpu_block *vblk,
			       struct meson_vpu_block_state *state)
{
	struct meson_vpu_afbc *afbc = to_afbc_block(vblk);

	MESON_DRM_BLOCK("%s enable called.\n", afbc->base.name);
}

static void osd_afbc_hw_disable(struct meson_vpu_block *vblk,
				struct meson_vpu_block_state *state)
{
	struct meson_vpu_afbc *afbc = to_afbc_block(vblk);
	u32 osd_index = vblk->index;

	osd_afbc_enable(vblk, state->sub->reg_ops, osd_index, 0);
	MESON_DRM_BLOCK("%s disable called.\n", afbc->base.name);
}

static void t7_osd_afbc_hw_disable(struct meson_vpu_block *vblk,
				   struct meson_vpu_block_state *state)
{
	struct meson_vpu_afbc *afbc = to_afbc_block(vblk);

	t7_osd_afbc_enable(vblk, state->sub->reg_ops,
					afbc->status_regs, vblk->index, 0);
	MESON_DRM_BLOCK("%s disable called.\n", afbc->base.name);
}

static void osd_afbc_hw_init(struct meson_vpu_block *vblk)
{
	struct meson_vpu_pipeline *pipeline = vblk->pipeline;
	struct meson_vpu_afbc *afbc = to_afbc_block(vblk);

	afbc->afbc_regs = &afbc_osd_regs[vblk->index];
	afbc->status_regs = &afbc_status_regs;

	pipeline->subs[0].reg_ops->rdma_write_reg_bits(MALI_AFBCD_TOP_CTRL, 0, 23, 1);

    /* disable osd1 afbc */
	osd_afbc_enable(vblk, pipeline->subs[0].reg_ops, vblk->index, 0);

	MESON_DRM_BLOCK("%s hw_init called.\n", afbc->base.name);
}

static void t7_osd_afbc_hw_init(struct meson_vpu_block *vblk)
{
	struct meson_vpu_pipeline *pipeline = vblk->pipeline;
	struct meson_vpu_afbc *afbc = to_afbc_block(vblk);

	afbc->afbc_regs = &afbc_osd_t7_regs[0];
	afbc->status_regs = &afbc_status_t7_regs[vblk->index];

	meson_vpu_write_reg_bits(MALI_AFBCD_TOP_CTRL, 0, 23, 1);

	/* disable osd1 afbc */
	t7_osd_afbc_enable(vblk, pipeline->subs[0].reg_ops,
						afbc->status_regs, vblk->index, 0);

	MESON_DRM_BLOCK("%s hw_init called.\n", afbc->base.name);
}

static void t3_osd_afbc_hw_init(struct meson_vpu_block *vblk)
{
	struct meson_vpu_pipeline *pipeline = vblk->pipeline;
	struct meson_vpu_afbc *afbc = to_afbc_block(vblk);

	afbc->afbc_regs = &afbc_osd_t7_regs[0];
	afbc->status_regs = &afbc_status_t3_regs[vblk->index];

	/* disable osd1 afbc */
	t7_osd_afbc_enable(vblk, pipeline->subs[0].reg_ops,
					   afbc->status_regs, vblk->index, 0);

	MESON_DRM_BLOCK("%s hw_init called.\n", afbc->base.name);
}

static void s5_osd_afbc_hw_init(struct meson_vpu_block *vblk)
{
	struct meson_vpu_pipeline *pipeline = vblk->pipeline;
	struct meson_vpu_afbc *afbc = to_afbc_block(vblk);

	afbc->afbc_regs = &afbc_osd_t7_regs[0];
	afbc->status_regs = &afbc_status_s5_regs[vblk->index];

	/* disable osd1 afbc */
	t7_osd_afbc_enable(vblk, pipeline->subs[0].reg_ops, afbc->status_regs, vblk->index, 0);

	MESON_DRM_BLOCK("%s hw_init called.\n", afbc->base.name);
}

void arm_fbc_start(struct meson_vpu_pipeline_state *pipeline_state)
{
	if (!pipeline_state->global_afbc && global_afbc_mask) {
		meson_vpu_write_reg(VPU_MAFBC_IRQ_MASK, 0xf);
		meson_vpu_write_reg(VPU_MAFBC_IRQ_CLEAR, 0x3f);
		meson_vpu_write_reg(VPU_MAFBC_COMMAND, 1);
		pipeline_state->global_afbc = 1;
		afbc_err_irq_clear = 1;
	}

	afbc_set_cnt = pipeline_state->global_afbc;
}

void arm_fbc_check_error(void)
{
	u32 val;

	if (afbc_err_irq_clear) {
		/*check afbc error*/
		val = meson_drm_read_reg(VPU_MAFBC_IRQ_RAW_STATUS);
		if (val & 0x3c) {
			DRM_ERROR("afbc error happened, %x-%x\n", val, global_afbc_mask);
			afbc_err_cnt++;
			afbc_err_irq_clear = 0;
		}
	}

	if (0) {
		/*check surface config*/
		val = meson_drm_read_reg(VPU_MAFBC_SURFACE_CFG);
		if ((val & 0xf) != global_afbc_mask)
			DRM_ERROR("afbc surface cfg error, %x-%x\n", val, global_afbc_mask);

		if (global_afbc_mask && !afbc_set_cnt)
			DRM_ERROR("afbc not start, %x-%x\n", global_afbc_mask, afbc_set_cnt);
	}
}

struct meson_vpu_block_ops afbc_ops = {
	.check_state = osd_afbc_check_state,
	.update_state = g12a_osd_afbc_set_state,
	.enable = osd_afbc_hw_enable,
	.disable = osd_afbc_hw_disable,
	.dump_register = osd_afbc_dump_register,
	.init = osd_afbc_hw_init,
};

struct meson_vpu_block_ops t7_afbc_ops = {
	.check_state = osd_afbc_check_state,
	.update_state = t7_osd_afbc_set_state,
	.enable = osd_afbc_hw_enable,
	.disable = t7_osd_afbc_hw_disable,
	.dump_register = osd_afbc_dump_register,
	.init = t7_osd_afbc_hw_init,
};

struct meson_vpu_block_ops t3_afbc_ops = {
	.check_state = osd_afbc_check_state,
	.update_state = t3_osd_afbc_set_state,
	.enable = osd_afbc_hw_enable,
	.disable = t7_osd_afbc_hw_disable,
	.dump_register = osd_afbc_dump_register,
	.init = t3_osd_afbc_hw_init,
};

struct meson_vpu_block_ops s5_afbc_ops = {
	.check_state = osd_afbc_check_state,
	.update_state = s5_osd_afbc_set_state,
	.enable = osd_afbc_hw_enable,
	.disable = t7_osd_afbc_hw_disable,
	.dump_register = osd_afbc_dump_register,
	.init = s5_osd_afbc_hw_init,
};
