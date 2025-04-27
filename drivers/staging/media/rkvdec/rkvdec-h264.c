// SPDX-License-Identifier: GPL-2.0
/*
 * Rockchip Video Decoder H264 backend
 *
 * Copyright (C) 2019 Collabora, Ltd.
 *	Boris Brezillon <boris.brezillon@collabora.com>
 *
 * Copyright (C) 2016 Rockchip Electronics Co., Ltd.
 *	Jeffy Chen <jeffy.chen@rock-chips.com>
 */

#include <media/v4l2-h264.h>
#include <media/v4l2-mem2mem.h>
#include <media/v4l2-cabac/rkvdec-cabac.h>

#include "rkvdec.h"
#include "rkvdec-regs.h"

/* Size with u32 units. */
#define RKV_CABAC_INIT_BUFFER_SIZE	(3680 + 128)
#define RKV_RPS_SIZE			((128 + 128) / 4)
#define RKV_ERROR_INFO_SIZE		(256 * 144 * 4)

#define RKVDEC_NUM_REFLIST		3

struct rkvdec_h264_scaling_list {
	u8 scaling_list_4x4[6][16];
	u8 scaling_list_8x8[6][64];
	u8 padding[128];
};

struct rkvdec_sps_pps_packet {
	u32 info[8];
};

struct rkvdec_ps_field {
	u16 offset;
	u8 len;
};

#define PS_FIELD(_offset, _len) \
	((struct rkvdec_ps_field){ _offset, _len })

#define SEQ_PARAMETER_SET_ID				PS_FIELD(0, 4)
#define PROFILE_IDC					PS_FIELD(4, 8)
#define CONSTRAINT_SET3_FLAG				PS_FIELD(12, 1)
#define CHROMA_FORMAT_IDC				PS_FIELD(13, 2)
#define BIT_DEPTH_LUMA					PS_FIELD(15, 3)
#define BIT_DEPTH_CHROMA				PS_FIELD(18, 3)
#define QPPRIME_Y_ZERO_TRANSFORM_BYPASS_FLAG		PS_FIELD(21, 1)
#define LOG2_MAX_FRAME_NUM_MINUS4			PS_FIELD(22, 4)
#define MAX_NUM_REF_FRAMES				PS_FIELD(26, 5)
#define PIC_ORDER_CNT_TYPE				PS_FIELD(31, 2)
#define LOG2_MAX_PIC_ORDER_CNT_LSB_MINUS4		PS_FIELD(33, 4)
#define DELTA_PIC_ORDER_ALWAYS_ZERO_FLAG		PS_FIELD(37, 1)
#define PIC_WIDTH_IN_MBS				PS_FIELD(38, 9)
#define PIC_HEIGHT_IN_MBS				PS_FIELD(47, 9)
#define FRAME_MBS_ONLY_FLAG				PS_FIELD(56, 1)
#define MB_ADAPTIVE_FRAME_FIELD_FLAG			PS_FIELD(57, 1)
#define DIRECT_8X8_INFERENCE_FLAG			PS_FIELD(58, 1)
#define MVC_EXTENSION_ENABLE				PS_FIELD(59, 1)
#define NUM_VIEWS					PS_FIELD(60, 2)
#define VIEW_ID(i)					PS_FIELD(62 + ((i) * 10), 10)
#define NUM_ANCHOR_REFS_L(i)				PS_FIELD(82 + ((i) * 11), 1)
#define ANCHOR_REF_L(i)				PS_FIELD(83 + ((i) * 11), 10)
#define NUM_NON_ANCHOR_REFS_L(i)			PS_FIELD(104 + ((i) * 11), 1)
#define NON_ANCHOR_REFS_L(i)				PS_FIELD(105 + ((i) * 11), 10)
#define PIC_PARAMETER_SET_ID				PS_FIELD(128, 8)
#define PPS_SEQ_PARAMETER_SET_ID			PS_FIELD(136, 5)
#define ENTROPY_CODING_MODE_FLAG			PS_FIELD(141, 1)
#define BOTTOM_FIELD_PIC_ORDER_IN_FRAME_PRESENT_FLAG	PS_FIELD(142, 1)
#define NUM_REF_IDX_L_DEFAULT_ACTIVE_MINUS1(i)		PS_FIELD(143 + ((i) * 5), 5)
#define WEIGHTED_PRED_FLAG				PS_FIELD(153, 1)
#define WEIGHTED_BIPRED_IDC				PS_FIELD(154, 2)
#define PIC_INIT_QP_MINUS26				PS_FIELD(156, 7)
#define PIC_INIT_QS_MINUS26				PS_FIELD(163, 6)
#define CHROMA_QP_INDEX_OFFSET				PS_FIELD(169, 5)
#define DEBLOCKING_FILTER_CONTROL_PRESENT_FLAG		PS_FIELD(174, 1)
#define CONSTRAINED_INTRA_PRED_FLAG			PS_FIELD(175, 1)
#define REDUNDANT_PIC_CNT_PRESENT			PS_FIELD(176, 1)
#define TRANSFORM_8X8_MODE_FLAG			PS_FIELD(177, 1)
#define SECOND_CHROMA_QP_INDEX_OFFSET			PS_FIELD(178, 5)
#define SCALING_LIST_ENABLE_FLAG			PS_FIELD(183, 1)
#define SCALING_LIST_ADDRESS				PS_FIELD(184, 32)
#define IS_LONG_TERM(i)				PS_FIELD(216 + (i), 1)

#define DPB_OFFS(i, j)					(288 + ((j) * 32 * 7) + ((i) * 7))
#define DPB_INFO(i, j)					PS_FIELD(DPB_OFFS(i, j), 5)
#define BOTTOM_FLAG(i, j)				PS_FIELD(DPB_OFFS(i, j) + 5, 1)
#define VIEW_INDEX_OFF(i, j)				PS_FIELD(DPB_OFFS(i, j) + 6, 1)

/* Data structure describing auxiliary buffer format. */
struct rkvdec_h264_priv_tbl {
	s8 cabac_table[4][464][2];
	struct rkvdec_h264_scaling_list scaling_list;
	u32 rps[RKV_RPS_SIZE];
	struct rkvdec_sps_pps_packet param_set[256];
	u8 err_info[RKV_ERROR_INFO_SIZE];
};

struct rkvdec_h264_reflists {
	struct v4l2_h264_reference p[V4L2_H264_REF_LIST_LEN];
	struct v4l2_h264_reference b0[V4L2_H264_REF_LIST_LEN];
	struct v4l2_h264_reference b1[V4L2_H264_REF_LIST_LEN];
};

struct rkvdec_h264_run {
	struct rkvdec_run base;
	const struct v4l2_ctrl_h264_decode_params *decode_params;
	const struct v4l2_ctrl_h264_sps *sps;
	const struct v4l2_ctrl_h264_pps *pps;
	const struct v4l2_ctrl_h264_scaling_matrix *scaling_matrix;
	struct vb2_buffer *ref_buf[V4L2_H264_NUM_DPB_ENTRIES];
};

struct rkvdec_h264_ctx {
	struct rkvdec_aux_buf priv_tbl;
	struct rkvdec_h264_reflists reflists;
};

static void set_ps_field(u32 *buf, struct rkvdec_ps_field field, u32 value)
{
	u8 bit = field.offset % 32, word = field.offset / 32;
	u64 mask = GENMASK_ULL(bit + field.len - 1, bit);
	u64 val = ((u64)value << bit) & mask;

	buf[word] &= ~mask;
	buf[word] |= val;
	if (bit + field.len > 32) {
		buf[word + 1] &= ~(mask >> 32);
		buf[word + 1] |= val >> 32;
	}
}

static void assemble_hw_pps(struct rkvdec_ctx *ctx,
			    struct rkvdec_h264_run *run)
{
	struct rkvdec_h264_ctx *h264_ctx = ctx->priv;
	const struct v4l2_ctrl_h264_sps *sps = run->sps;
	const struct v4l2_ctrl_h264_pps *pps = run->pps;
	const struct v4l2_ctrl_h264_decode_params *dec_params = run->decode_params;
	const struct v4l2_h264_dpb_entry *dpb = dec_params->dpb;
	struct rkvdec_h264_priv_tbl *priv_tbl = h264_ctx->priv_tbl.cpu;
	struct rkvdec_sps_pps_packet *hw_ps;
	dma_addr_t scaling_list_address;
	u32 scaling_distance;
	u32 i;

	/*
	 * HW read the SPS/PPS information from PPS packet index by PPS id.
	 * offset from the base can be calculated by PPS_id * 32 (size per PPS
	 * packet unit). so the driver copy SPS/PPS information to the exact PPS
	 * packet unit for HW accessing.
	 */
	hw_ps = &priv_tbl->param_set[pps->pic_parameter_set_id];
	memset(hw_ps, 0, sizeof(*hw_ps));

#define WRITE_PPS(value, field) set_ps_field(hw_ps->info, field, value)
	/* write sps */
	WRITE_PPS(0xf, SEQ_PARAMETER_SET_ID);
	WRITE_PPS(0xff, PROFILE_IDC);
	WRITE_PPS(1, CONSTRAINT_SET3_FLAG);
	WRITE_PPS(sps->chroma_format_idc, CHROMA_FORMAT_IDC);
	WRITE_PPS(sps->bit_depth_luma_minus8, BIT_DEPTH_LUMA);
	WRITE_PPS(sps->bit_depth_chroma_minus8, BIT_DEPTH_CHROMA);
	WRITE_PPS(0, QPPRIME_Y_ZERO_TRANSFORM_BYPASS_FLAG);
	WRITE_PPS(sps->log2_max_frame_num_minus4, LOG2_MAX_FRAME_NUM_MINUS4);
	WRITE_PPS(sps->max_num_ref_frames, MAX_NUM_REF_FRAMES);
	WRITE_PPS(sps->pic_order_cnt_type, PIC_ORDER_CNT_TYPE);
	WRITE_PPS(sps->log2_max_pic_order_cnt_lsb_minus4,
		  LOG2_MAX_PIC_ORDER_CNT_LSB_MINUS4);
	WRITE_PPS(!!(sps->flags & V4L2_H264_SPS_FLAG_DELTA_PIC_ORDER_ALWAYS_ZERO),
		  DELTA_PIC_ORDER_ALWAYS_ZERO_FLAG);

	/*
	 * Use the SPS values since they are already in macroblocks
	 * dimensions, height can be field height (halved) if
	 * V4L2_H264_SPS_FLAG_FRAME_MBS_ONLY is not set and also it allows
	 * decoding smaller images into larger allocation which can be used
	 * to implementing SVC spatial layer support.
	 */
	WRITE_PPS(sps->pic_width_in_mbs_minus1 + 1, PIC_WIDTH_IN_MBS);
	WRITE_PPS(sps->pic_height_in_map_units_minus1 + 1, PIC_HEIGHT_IN_MBS);

	WRITE_PPS(!!(sps->flags & V4L2_H264_SPS_FLAG_FRAME_MBS_ONLY),
		  FRAME_MBS_ONLY_FLAG);
	WRITE_PPS(!!(sps->flags & V4L2_H264_SPS_FLAG_MB_ADAPTIVE_FRAME_FIELD),
		  MB_ADAPTIVE_FRAME_FIELD_FLAG);
	WRITE_PPS(!!(sps->flags & V4L2_H264_SPS_FLAG_DIRECT_8X8_INFERENCE),
		  DIRECT_8X8_INFERENCE_FLAG);

	/* write pps */
	WRITE_PPS(0xff, PIC_PARAMETER_SET_ID);
	WRITE_PPS(0x1f, PPS_SEQ_PARAMETER_SET_ID);
	WRITE_PPS(!!(pps->flags & V4L2_H264_PPS_FLAG_ENTROPY_CODING_MODE),
		  ENTROPY_CODING_MODE_FLAG);
	WRITE_PPS(!!(pps->flags & V4L2_H264_PPS_FLAG_BOTTOM_FIELD_PIC_ORDER_IN_FRAME_PRESENT),
		  BOTTOM_FIELD_PIC_ORDER_IN_FRAME_PRESENT_FLAG);
	WRITE_PPS(pps->num_ref_idx_l0_default_active_minus1,
		  NUM_REF_IDX_L_DEFAULT_ACTIVE_MINUS1(0));
	WRITE_PPS(pps->num_ref_idx_l1_default_active_minus1,
		  NUM_REF_IDX_L_DEFAULT_ACTIVE_MINUS1(1));
	WRITE_PPS(!!(pps->flags & V4L2_H264_PPS_FLAG_WEIGHTED_PRED),
		  WEIGHTED_PRED_FLAG);
	WRITE_PPS(pps->weighted_bipred_idc, WEIGHTED_BIPRED_IDC);
	WRITE_PPS(pps->pic_init_qp_minus26, PIC_INIT_QP_MINUS26);
	WRITE_PPS(pps->pic_init_qs_minus26, PIC_INIT_QS_MINUS26);
	WRITE_PPS(pps->chroma_qp_index_offset, CHROMA_QP_INDEX_OFFSET);
	WRITE_PPS(!!(pps->flags & V4L2_H264_PPS_FLAG_DEBLOCKING_FILTER_CONTROL_PRESENT),
		  DEBLOCKING_FILTER_CONTROL_PRESENT_FLAG);
	WRITE_PPS(!!(pps->flags & V4L2_H264_PPS_FLAG_CONSTRAINED_INTRA_PRED),
		  CONSTRAINED_INTRA_PRED_FLAG);
	WRITE_PPS(!!(pps->flags & V4L2_H264_PPS_FLAG_REDUNDANT_PIC_CNT_PRESENT),
		  REDUNDANT_PIC_CNT_PRESENT);
	WRITE_PPS(!!(pps->flags & V4L2_H264_PPS_FLAG_TRANSFORM_8X8_MODE),
		  TRANSFORM_8X8_MODE_FLAG);
	WRITE_PPS(pps->second_chroma_qp_index_offset,
		  SECOND_CHROMA_QP_INDEX_OFFSET);

	WRITE_PPS(!!(pps->flags & V4L2_H264_PPS_FLAG_SCALING_MATRIX_PRESENT),
		  SCALING_LIST_ENABLE_FLAG);
	/* To be on the safe side, program the scaling matrix address */
	scaling_distance = offsetof(struct rkvdec_h264_priv_tbl, scaling_list);
	scaling_list_address = h264_ctx->priv_tbl.dma + scaling_distance;
	WRITE_PPS(scaling_list_address, SCALING_LIST_ADDRESS);

	for (i = 0; i < ARRAY_SIZE(dec_params->dpb); i++) {
		u32 is_longterm = 0;

		if (dpb[i].flags & V4L2_H264_DPB_ENTRY_FLAG_LONG_TERM)
			is_longterm = 1;

		WRITE_PPS(is_longterm, IS_LONG_TERM(i));
	}
}

static void lookup_ref_buf_idx(struct rkvdec_ctx *ctx,
			       struct rkvdec_h264_run *run)
{
	const struct v4l2_ctrl_h264_decode_params *dec_params = run->decode_params;
	u32 i;

	for (i = 0; i < ARRAY_SIZE(dec_params->dpb); i++) {
		struct v4l2_m2m_ctx *m2m_ctx = ctx->fh.m2m_ctx;
		const struct v4l2_h264_dpb_entry *dpb = run->decode_params->dpb;
		struct vb2_queue *cap_q = &m2m_ctx->cap_q_ctx.q;
		struct vb2_buffer *buf = NULL;

		if (dpb[i].flags & V4L2_H264_DPB_ENTRY_FLAG_ACTIVE) {
			buf = vb2_find_buffer(cap_q, dpb[i].reference_ts);
			if (!buf)
				pr_debug("No buffer for reference_ts %llu",
					 dpb[i].reference_ts);
		}

		run->ref_buf[i] = buf;
	}
}

static void assemble_hw_rps(struct rkvdec_ctx *ctx,
			    struct v4l2_h264_reflist_builder *builder,
			    struct rkvdec_h264_run *run)
{
	const struct v4l2_ctrl_h264_decode_params *dec_params = run->decode_params;
	const struct v4l2_h264_dpb_entry *dpb = dec_params->dpb;
	struct rkvdec_h264_ctx *h264_ctx = ctx->priv;
	struct rkvdec_h264_priv_tbl *priv_tbl = h264_ctx->priv_tbl.cpu;

	u32 *hw_rps = priv_tbl->rps;
	u32 i, j;
	u16 *p = (u16 *)hw_rps;

	memset(hw_rps, 0, sizeof(priv_tbl->rps));

	/*
	 * Assign an invalid pic_num if DPB entry at that position is inactive.
	 * If we assign 0 in that position hardware will treat that as a real
	 * reference picture with pic_num 0, triggering output picture
	 * corruption.
	 */
	for (i = 0; i < ARRAY_SIZE(dec_params->dpb); i++) {
		if (!(dpb[i].flags & V4L2_H264_DPB_ENTRY_FLAG_ACTIVE))
			continue;

		p[i] = builder->refs[i].frame_num;
	}

	for (j = 0; j < RKVDEC_NUM_REFLIST; j++) {
		for (i = 0; i < builder->num_valid; i++) {
			struct v4l2_h264_reference *ref;
			bool dpb_valid;
			bool bottom;

			switch (j) {
			case 0:
				ref = &h264_ctx->reflists.p[i];
				break;
			case 1:
				ref = &h264_ctx->reflists.b0[i];
				break;
			case 2:
				ref = &h264_ctx->reflists.b1[i];
				break;
			}

			if (WARN_ON(ref->index >= ARRAY_SIZE(dec_params->dpb)))
				continue;

			dpb_valid = run->ref_buf[ref->index] != NULL;
			bottom = ref->fields == V4L2_H264_BOTTOM_FIELD_REF;

			set_ps_field(hw_rps, DPB_INFO(i, j),
				     ref->index | dpb_valid << 4);
			set_ps_field(hw_rps, BOTTOM_FLAG(i, j), bottom);
		}
	}
}

static void assemble_hw_scaling_list(struct rkvdec_ctx *ctx,
				     struct rkvdec_h264_run *run)
{
	const struct v4l2_ctrl_h264_scaling_matrix *scaling = run->scaling_matrix;
	const struct v4l2_ctrl_h264_pps *pps = run->pps;
	struct rkvdec_h264_ctx *h264_ctx = ctx->priv;
	struct rkvdec_h264_priv_tbl *tbl = h264_ctx->priv_tbl.cpu;

	if (!(pps->flags & V4L2_H264_PPS_FLAG_SCALING_MATRIX_PRESENT))
		return;

	BUILD_BUG_ON(sizeof(tbl->scaling_list.scaling_list_4x4) !=
		     sizeof(scaling->scaling_list_4x4));
	BUILD_BUG_ON(sizeof(tbl->scaling_list.scaling_list_8x8) !=
		     sizeof(scaling->scaling_list_8x8));

	memcpy(tbl->scaling_list.scaling_list_4x4,
	       scaling->scaling_list_4x4,
	       sizeof(scaling->scaling_list_4x4));

	memcpy(tbl->scaling_list.scaling_list_8x8,
	       scaling->scaling_list_8x8,
	       sizeof(scaling->scaling_list_8x8));
}

/*
 * dpb poc related registers table
 */
static const u32 poc_reg_tbl_top_field[16] = {
	RKVDEC_REG_H264_POC_REFER0(0),
	RKVDEC_REG_H264_POC_REFER0(2),
	RKVDEC_REG_H264_POC_REFER0(4),
	RKVDEC_REG_H264_POC_REFER0(6),
	RKVDEC_REG_H264_POC_REFER0(8),
	RKVDEC_REG_H264_POC_REFER0(10),
	RKVDEC_REG_H264_POC_REFER0(12),
	RKVDEC_REG_H264_POC_REFER0(14),
	RKVDEC_REG_H264_POC_REFER1(1),
	RKVDEC_REG_H264_POC_REFER1(3),
	RKVDEC_REG_H264_POC_REFER1(5),
	RKVDEC_REG_H264_POC_REFER1(7),
	RKVDEC_REG_H264_POC_REFER1(9),
	RKVDEC_REG_H264_POC_REFER1(11),
	RKVDEC_REG_H264_POC_REFER1(13),
	RKVDEC_REG_H264_POC_REFER2(0)
};

static const u32 poc_reg_tbl_bottom_field[16] = {
	RKVDEC_REG_H264_POC_REFER0(1),
	RKVDEC_REG_H264_POC_REFER0(3),
	RKVDEC_REG_H264_POC_REFER0(5),
	RKVDEC_REG_H264_POC_REFER0(7),
	RKVDEC_REG_H264_POC_REFER0(9),
	RKVDEC_REG_H264_POC_REFER0(11),
	RKVDEC_REG_H264_POC_REFER0(13),
	RKVDEC_REG_H264_POC_REFER1(0),
	RKVDEC_REG_H264_POC_REFER1(2),
	RKVDEC_REG_H264_POC_REFER1(4),
	RKVDEC_REG_H264_POC_REFER1(6),
	RKVDEC_REG_H264_POC_REFER1(8),
	RKVDEC_REG_H264_POC_REFER1(10),
	RKVDEC_REG_H264_POC_REFER1(12),
	RKVDEC_REG_H264_POC_REFER1(14),
	RKVDEC_REG_H264_POC_REFER2(1)
};

static void config_registers(struct rkvdec_ctx *ctx,
			     struct rkvdec_h264_run *run)
{
	struct rkvdec_dev *rkvdec = ctx->dev;
	const struct v4l2_ctrl_h264_decode_params *dec_params = run->decode_params;
	const struct v4l2_ctrl_h264_sps *sps = run->sps;
	const struct v4l2_h264_dpb_entry *dpb = dec_params->dpb;
	struct rkvdec_h264_ctx *h264_ctx = ctx->priv;
	dma_addr_t priv_start_addr = h264_ctx->priv_tbl.dma;
	const struct v4l2_pix_format_mplane *dst_fmt;
	struct vb2_v4l2_buffer *src_buf = run->base.bufs.src;
	struct vb2_v4l2_buffer *dst_buf = run->base.bufs.dst;
	const struct v4l2_format *f;
	dma_addr_t rlc_addr;
	dma_addr_t refer_addr;
	u32 rlc_len;
	u32 hor_virstride = 0;
	u32 ver_virstride = 0;
	u32 y_virstride = 0;
	u32 yuv_virstride = 0;
	u32 offset;
	dma_addr_t dst_addr;
	u32 reg, i;

	reg = RKVDEC_MODE(RKVDEC_MODE_H264);
	writel_relaxed(reg, rkvdec->regs + RKVDEC_REG_SYSCTRL);

	f = &ctx->decoded_fmt;
	dst_fmt = &f->fmt.pix_mp;
	hor_virstride = (sps->bit_depth_luma_minus8 + 8) * dst_fmt->width / 8;
	ver_virstride = round_up(dst_fmt->height, 16);
	y_virstride = hor_virstride * ver_virstride;

	if (sps->chroma_format_idc == 0)
		yuv_virstride = y_virstride;
	else if (sps->chroma_format_idc == 1)
		yuv_virstride += y_virstride + y_virstride / 2;
	else if (sps->chroma_format_idc == 2)
		yuv_virstride += 2 * y_virstride;

	reg = RKVDEC_Y_HOR_VIRSTRIDE(hor_virstride / 16) |
	      RKVDEC_UV_HOR_VIRSTRIDE(hor_virstride / 16) |
	      RKVDEC_SLICE_NUM_HIGHBIT |
	      RKVDEC_SLICE_NUM_LOWBITS(0x7ff);
	writel_relaxed(reg, rkvdec->regs + RKVDEC_REG_PICPAR);

	/* config rlc base address */
	rlc_addr = vb2_dma_contig_plane_dma_addr(&src_buf->vb2_buf, 0);
	writel_relaxed(rlc_addr, rkvdec->regs + RKVDEC_REG_STRM_RLC_BASE);
	writel_relaxed(rlc_addr, rkvdec->regs + RKVDEC_REG_RLCWRITE_BASE);

	rlc_len = vb2_get_plane_payload(&src_buf->vb2_buf, 0);
	reg = RKVDEC_STRM_LEN(rlc_len);
	writel_relaxed(reg, rkvdec->regs + RKVDEC_REG_STRM_LEN);

	/* config cabac table */
	offset = offsetof(struct rkvdec_h264_priv_tbl, cabac_table);
	writel_relaxed(priv_start_addr + offset,
		       rkvdec->regs + RKVDEC_REG_CABACTBL_PROB_BASE);

	/* config output base address */
	dst_addr = vb2_dma_contig_plane_dma_addr(&dst_buf->vb2_buf, 0);
	writel_relaxed(dst_addr, rkvdec->regs + RKVDEC_REG_DECOUT_BASE);

	reg = RKVDEC_Y_VIRSTRIDE(y_virstride / 16);
	writel_relaxed(reg, rkvdec->regs + RKVDEC_REG_Y_VIRSTRIDE);

	reg = RKVDEC_YUV_VIRSTRIDE(yuv_virstride / 16);
	writel_relaxed(reg, rkvdec->regs + RKVDEC_REG_YUV_VIRSTRIDE);

	/* config ref pic address & poc */
	for (i = 0; i < ARRAY_SIZE(dec_params->dpb); i++) {
		struct vb2_buffer *vb_buf = run->ref_buf[i];

		/*
		 * If a DPB entry is unused or invalid, address of current destination
		 * buffer is returned.
		 */
		if (!vb_buf)
			vb_buf = &dst_buf->vb2_buf;
		refer_addr = vb2_dma_contig_plane_dma_addr(vb_buf, 0);

		if (dpb[i].flags & V4L2_H264_DPB_ENTRY_FLAG_ACTIVE)
			refer_addr |= RKVDEC_COLMV_USED_FLAG_REF;
		if (dpb[i].flags & V4L2_H264_DPB_ENTRY_FLAG_FIELD)
			refer_addr |= RKVDEC_FIELD_REF;

		if (dpb[i].fields & V4L2_H264_TOP_FIELD_REF)
			refer_addr |= RKVDEC_TOPFIELD_USED_REF;
		if (dpb[i].fields & V4L2_H264_BOTTOM_FIELD_REF)
			refer_addr |= RKVDEC_BOTFIELD_USED_REF;

		writel_relaxed(dpb[i].top_field_order_cnt,
			       rkvdec->regs +  poc_reg_tbl_top_field[i]);
		writel_relaxed(dpb[i].bottom_field_order_cnt,
			       rkvdec->regs + poc_reg_tbl_bottom_field[i]);

		if (i < V4L2_H264_NUM_DPB_ENTRIES - 1)
			writel_relaxed(refer_addr,
				       rkvdec->regs + RKVDEC_REG_H264_BASE_REFER(i));
		else
			writel_relaxed(refer_addr,
				       rkvdec->regs + RKVDEC_REG_H264_BASE_REFER15);
	}

	reg = RKVDEC_CUR_POC(dec_params->top_field_order_cnt);
	writel_relaxed(reg, rkvdec->regs + RKVDEC_REG_CUR_POC0);

	reg = RKVDEC_CUR_POC(dec_params->bottom_field_order_cnt);
	writel_relaxed(reg, rkvdec->regs + RKVDEC_REG_CUR_POC1);

	/* config hw pps address */
	offset = offsetof(struct rkvdec_h264_priv_tbl, param_set);
	writel_relaxed(priv_start_addr + offset,
		       rkvdec->regs + RKVDEC_REG_PPS_BASE);

	/* config hw rps address */
	offset = offsetof(struct rkvdec_h264_priv_tbl, rps);
	writel_relaxed(priv_start_addr + offset,
		       rkvdec->regs + RKVDEC_REG_RPS_BASE);

	reg = RKVDEC_AXI_DDR_RDATA(0);
	writel_relaxed(reg, rkvdec->regs + RKVDEC_REG_AXI_DDR_RDATA);

	reg = RKVDEC_AXI_DDR_WDATA(0);
	writel_relaxed(reg, rkvdec->regs + RKVDEC_REG_AXI_DDR_WDATA);

	offset = offsetof(struct rkvdec_h264_priv_tbl, err_info);
	writel_relaxed(priv_start_addr + offset,
		       rkvdec->regs + RKVDEC_REG_H264_ERRINFO_BASE);
}

#define RKVDEC_H264_MAX_DEPTH_IN_BYTES		2

static int rkvdec_h264_adjust_fmt(struct rkvdec_ctx *ctx,
				  struct v4l2_format *f)
{
	struct v4l2_pix_format_mplane *fmt = &f->fmt.pix_mp;

	fmt->num_planes = 1;
	if (!fmt->plane_fmt[0].sizeimage)
		fmt->plane_fmt[0].sizeimage = fmt->width * fmt->height *
					      RKVDEC_H264_MAX_DEPTH_IN_BYTES;
	return 0;
}

static int rkvdec_h264_validate_sps(struct rkvdec_ctx *ctx,
				    const struct v4l2_ctrl_h264_sps *sps)
{
	unsigned int width, height;

	/*
	 * TODO: The hardware supports 10-bit and 4:2:2 profiles,
	 * but it's currently broken in the driver.
	 * Reject them for now, until it's fixed.
	 */
	if (sps->chroma_format_idc > 1)
		/* Only 4:0:0 and 4:2:0 are supported */
		return -EINVAL;
	if (sps->bit_depth_luma_minus8 != sps->bit_depth_chroma_minus8)
		/* Luma and chroma bit depth mismatch */
		return -EINVAL;
	if (sps->bit_depth_luma_minus8 != 0)
		/* Only 8-bit is supported */
		return -EINVAL;

	width = (sps->pic_width_in_mbs_minus1 + 1) * 16;
	height = (sps->pic_height_in_map_units_minus1 + 1) * 16;

	/*
	 * When frame_mbs_only_flag is not set, this is field height,
	 * which is half the final height (see (7-18) in the
	 * specification)
	 */
	if (!(sps->flags & V4L2_H264_SPS_FLAG_FRAME_MBS_ONLY))
		height *= 2;

	if (width > ctx->coded_fmt.fmt.pix_mp.width ||
	    height > ctx->coded_fmt.fmt.pix_mp.height)
		return -EINVAL;

	return 0;
}

static int rkvdec_h264_start(struct rkvdec_ctx *ctx)
{
	struct rkvdec_dev *rkvdec = ctx->dev;
	struct rkvdec_h264_priv_tbl *priv_tbl;
	struct rkvdec_h264_ctx *h264_ctx;
	struct v4l2_ctrl *ctrl;
	int ret;

	ctrl = v4l2_ctrl_find(&ctx->ctrl_hdl,
			      V4L2_CID_STATELESS_H264_SPS);
	if (!ctrl)
		return -EINVAL;

	ret = rkvdec_h264_validate_sps(ctx, ctrl->p_new.p_h264_sps);
	if (ret)
		return ret;

	h264_ctx = kzalloc(sizeof(*h264_ctx), GFP_KERNEL);
	if (!h264_ctx)
		return -ENOMEM;

	priv_tbl = dma_alloc_coherent(rkvdec->dev, sizeof(*priv_tbl),
				      &h264_ctx->priv_tbl.dma, GFP_KERNEL);
	if (!priv_tbl) {
		ret = -ENOMEM;
		goto err_free_ctx;
	}

	h264_ctx->priv_tbl.size = sizeof(*priv_tbl);
	h264_ctx->priv_tbl.cpu = priv_tbl;
	memcpy(priv_tbl->cabac_table, rkvdec_h264_cabac_table,
	       sizeof(rkvdec_h264_cabac_table));

	ctx->priv = h264_ctx;
	return 0;

err_free_ctx:
	kfree(h264_ctx);
	return ret;
}

static void rkvdec_h264_stop(struct rkvdec_ctx *ctx)
{
	struct rkvdec_h264_ctx *h264_ctx = ctx->priv;
	struct rkvdec_dev *rkvdec = ctx->dev;

	dma_free_coherent(rkvdec->dev, h264_ctx->priv_tbl.size,
			  h264_ctx->priv_tbl.cpu, h264_ctx->priv_tbl.dma);
	kfree(h264_ctx);
}

static void rkvdec_h264_run_preamble(struct rkvdec_ctx *ctx,
				     struct rkvdec_h264_run *run)
{
	struct v4l2_ctrl *ctrl;

	ctrl = v4l2_ctrl_find(&ctx->ctrl_hdl,
			      V4L2_CID_STATELESS_H264_DECODE_PARAMS);
	run->decode_params = ctrl ? ctrl->p_cur.p : NULL;
	ctrl = v4l2_ctrl_find(&ctx->ctrl_hdl,
			      V4L2_CID_STATELESS_H264_SPS);
	run->sps = ctrl ? ctrl->p_cur.p : NULL;
	ctrl = v4l2_ctrl_find(&ctx->ctrl_hdl,
			      V4L2_CID_STATELESS_H264_PPS);
	run->pps = ctrl ? ctrl->p_cur.p : NULL;
	ctrl = v4l2_ctrl_find(&ctx->ctrl_hdl,
			      V4L2_CID_STATELESS_H264_SCALING_MATRIX);
	run->scaling_matrix = ctrl ? ctrl->p_cur.p : NULL;

	rkvdec_run_preamble(ctx, &run->base);
}

static int rkvdec_h264_run(struct rkvdec_ctx *ctx)
{
	struct v4l2_h264_reflist_builder reflist_builder;
	struct rkvdec_dev *rkvdec = ctx->dev;
	struct rkvdec_h264_ctx *h264_ctx = ctx->priv;
	struct rkvdec_h264_run run;

	rkvdec_h264_run_preamble(ctx, &run);

	/* Build the P/B{0,1} ref lists. */
	v4l2_h264_init_reflist_builder(&reflist_builder, run.decode_params,
				       run.sps, run.decode_params->dpb);
	v4l2_h264_build_p_ref_list(&reflist_builder, h264_ctx->reflists.p);
	v4l2_h264_build_b_ref_lists(&reflist_builder, h264_ctx->reflists.b0,
				    h264_ctx->reflists.b1);

	assemble_hw_scaling_list(ctx, &run);
	assemble_hw_pps(ctx, &run);
	lookup_ref_buf_idx(ctx, &run);
	assemble_hw_rps(ctx, &reflist_builder, &run);
	config_registers(ctx, &run);

	rkvdec_run_postamble(ctx, &run.base);

	schedule_delayed_work(&rkvdec->watchdog_work, msecs_to_jiffies(2000));

	writel(0, rkvdec->regs + RKVDEC_REG_STRMD_ERR_EN);
	writel(0, rkvdec->regs + RKVDEC_REG_H264_ERR_E);
	writel(1, rkvdec->regs + RKVDEC_REG_PREF_LUMA_CACHE_COMMAND);
	writel(1, rkvdec->regs + RKVDEC_REG_PREF_CHR_CACHE_COMMAND);

	/* Start decoding! */
	writel(RKVDEC_INTERRUPT_DEC_E | RKVDEC_CONFIG_DEC_CLK_GATE_E |
	       RKVDEC_TIMEOUT_E | RKVDEC_BUF_EMPTY_E,
	       rkvdec->regs + RKVDEC_REG_INTERRUPT);

	return 0;
}

static int rkvdec_h264_try_ctrl(struct rkvdec_ctx *ctx, struct v4l2_ctrl *ctrl)
{
	if (ctrl->id == V4L2_CID_STATELESS_H264_SPS)
		return rkvdec_h264_validate_sps(ctx, ctrl->p_new.p_h264_sps);

	return 0;
}

const struct rkvdec_coded_fmt_ops rkvdec_h264_fmt_ops = {
	.adjust_fmt = rkvdec_h264_adjust_fmt,
	.start = rkvdec_h264_start,
	.stop = rkvdec_h264_stop,
	.run = rkvdec_h264_run,
	.try_ctrl = rkvdec_h264_try_ctrl,
};
