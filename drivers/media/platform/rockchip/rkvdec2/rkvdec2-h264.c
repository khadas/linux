// SPDX-License-Identifier: GPL-2.0
/*
 * Rockchip Video Decoder 2 H264 backend
 *
 * Copyright (C) 2024 Collabora, Ltd.
 *  Detlev Casanova <detlev.casanova@collabora.com>
 *
 * Based on rkvdec driver by Boris Brezillon <boris.brezillon@collabora.com>
 */

#include <media/v4l2-h264.h>
#include <media/v4l2-mem2mem.h>
#include <media/v4l2-cabac/rkvdec-cabac.h>

#include "rkvdec2.h"
#include "rkvdec2-regs.h"

#define RKVDEC_NUM_REFLIST		3

struct rkvdec2_h264_scaling_list {
	u8 scaling_list_4x4[6][16];
	u8 scaling_list_8x8[6][64];
	u8 padding[128];
};

struct rkvdec2_sps {
	u16 seq_parameter_set_id:			4;
	u16 profile_idc:				8;
	u16 constraint_set3_flag:			1;
	u16 chroma_format_idc:				2;
	u16 bit_depth_luma:				3;
	u16 bit_depth_chroma:				3;
	u16 qpprime_y_zero_transform_bypass_flag:	1;
	u16 log2_max_frame_num_minus4:			4;
	u16 max_num_ref_frames:				5;
	u16 pic_order_cnt_type:				2;
	u16 log2_max_pic_order_cnt_lsb_minus4:		4;
	u16 delta_pic_order_always_zero_flag:		1;
	u16 pic_width_in_mbs:				12;
	u16 pic_height_in_mbs:				12;
	u16 frame_mbs_only_flag:			1;
	u16 mb_adaptive_frame_field_flag:		1;
	u16 direct_8x8_inference_flag:			1;
	u16 mvc_extension_enable:			1;
	u16 num_views:					2;

	u16 reserved_bits:				12;
	u16 reserved[11];
} __packed;

struct rkvdec2_pps {
	u16 pic_parameter_set_id:				8;
	u16 pps_seq_parameter_set_id:				5;
	u16 entropy_coding_mode_flag:				1;
	u16 bottom_field_pic_order_in_frame_present_flag:	1;
	u16 num_ref_idx_l0_default_active_minus1:		5;
	u16 num_ref_idx_l1_default_active_minus1:		5;
	u16 weighted_pred_flag:					1;
	u16 weighted_bipred_idc:				2;
	u16 pic_init_qp_minus26:				7;
	u16 pic_init_qs_minus26:				6;
	u16 chroma_qp_index_offset:				5;
	u16 deblocking_filter_control_present_flag:		1;
	u16 constrained_intra_pred_flag:			1;
	u16 redundant_pic_cnt_present:				1;
	u16 transform_8x8_mode_flag:				1;
	u16 second_chroma_qp_index_offset:			5;
	u16 scaling_list_enable_flag:				1;
	u32 scaling_list_address;
	u16 is_longterm;

	u8 reserved[3];
} __packed;

struct rkvdec2_rps_entry {
	u32 dpb_info0:		5;
	u32 bottom_flag0:	1;
	u32 view_index_off0:	1;
	u32 dpb_info1:		5;
	u32 bottom_flag1:	1;
	u32 view_index_off1:	1;
	u32 dpb_info2:		5;
	u32 bottom_flag2:	1;
	u32 view_index_off2:	1;
	u32 dpb_info3:		5;
	u32 bottom_flag3:	1;
	u32 view_index_off3:	1;
	u32 dpb_info4:		5;
	u32 bottom_flag4:	1;
	u32 view_index_off4:	1;
	u32 dpb_info5:		5;
	u32 bottom_flag5:	1;
	u32 view_index_off5:	1;
	u32 dpb_info6:		5;
	u32 bottom_flag6:	1;
	u32 view_index_off6:	1;
	u32 dpb_info7:		5;
	u32 bottom_flag7:	1;
	u32 view_index_off7:	1;
} __packed;

struct rkvdec2_rps {
	u16 frame_num[16];
	u32 reserved0;
	struct rkvdec2_rps_entry entries[12];
	u32 reserved1[66];
}  __packed;

struct rkvdec2_sps_pps {
	struct rkvdec2_sps sps;
	struct rkvdec2_pps pps;
}  __packed;

/* Data structure describing auxiliary buffer format. */
struct rkvdec2_h264_priv_tbl {
	u32 cabac_table[4][464][2];
	struct rkvdec2_h264_scaling_list scaling_list;
	struct rkvdec2_sps_pps param_set[256];
	struct rkvdec2_rps rps;
};

struct rkvdec2_h264_reflists {
	struct v4l2_h264_reference p[V4L2_H264_REF_LIST_LEN];
	struct v4l2_h264_reference b0[V4L2_H264_REF_LIST_LEN];
	struct v4l2_h264_reference b1[V4L2_H264_REF_LIST_LEN];
};

struct rkvdec2_h264_run {
	struct rkvdec2_run base;
	const struct v4l2_ctrl_h264_decode_params *decode_params;
	const struct v4l2_ctrl_h264_sps *sps;
	const struct v4l2_ctrl_h264_pps *pps;
	const struct v4l2_ctrl_h264_scaling_matrix *scaling_matrix;
	struct vb2_buffer *ref_buf[V4L2_H264_NUM_DPB_ENTRIES];
};

struct rkvdec2_h264_ctx {
	struct rkvdec2_aux_buf priv_tbl;
	struct rkvdec2_h264_reflists reflists;
	struct rkvdec2_regs_h264 regs;
};

static void assemble_hw_pps(struct rkvdec2_ctx *ctx,
			    struct rkvdec2_h264_run *run)
{
	struct rkvdec2_h264_ctx *h264_ctx = ctx->priv;
	const struct v4l2_ctrl_h264_sps *sps = run->sps;
	const struct v4l2_ctrl_h264_pps *pps = run->pps;
	const struct v4l2_ctrl_h264_decode_params *dec_params = run->decode_params;
	const struct v4l2_h264_dpb_entry *dpb = dec_params->dpb;
	struct rkvdec2_h264_priv_tbl *priv_tbl = h264_ctx->priv_tbl.cpu;
	struct rkvdec2_sps_pps *hw_ps;
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

	/* write sps */
	hw_ps->sps.seq_parameter_set_id = sps->seq_parameter_set_id;
	hw_ps->sps.profile_idc = sps->profile_idc;
	hw_ps->sps.constraint_set3_flag = !!(sps->constraint_set_flags & (1 << 3));
	hw_ps->sps.chroma_format_idc = sps->chroma_format_idc;
	hw_ps->sps.bit_depth_luma = sps->bit_depth_luma_minus8;
	hw_ps->sps.bit_depth_chroma = sps->bit_depth_chroma_minus8;
	hw_ps->sps.qpprime_y_zero_transform_bypass_flag = !!(sps->flags & V4L2_H264_SPS_FLAG_QPPRIME_Y_ZERO_TRANSFORM_BYPASS);
	hw_ps->sps.log2_max_frame_num_minus4 = sps->log2_max_frame_num_minus4;
	hw_ps->sps.max_num_ref_frames = sps->max_num_ref_frames;
	hw_ps->sps.pic_order_cnt_type = sps->pic_order_cnt_type;
	hw_ps->sps.log2_max_pic_order_cnt_lsb_minus4 =
		sps->log2_max_pic_order_cnt_lsb_minus4;
	hw_ps->sps.delta_pic_order_always_zero_flag =
		!!(sps->flags & V4L2_H264_SPS_FLAG_DELTA_PIC_ORDER_ALWAYS_ZERO);
	hw_ps->sps.mvc_extension_enable = 1;
	hw_ps->sps.num_views = 1;

	/*
	 * Use the SPS values since they are already in macroblocks
	 * dimensions, height can be field height (halved) if
	 * V4L2_H264_SPS_FLAG_FRAME_MBS_ONLY is not set and also it allows
	 * decoding smaller images into larger allocation which can be used
	 * to implementing SVC spatial layer support.
	 */
	hw_ps->sps.pic_width_in_mbs = sps->pic_width_in_mbs_minus1 + 1;
	hw_ps->sps.pic_height_in_mbs = sps->pic_height_in_map_units_minus1 + 1;
	hw_ps->sps.frame_mbs_only_flag =
		!!(sps->flags & V4L2_H264_SPS_FLAG_FRAME_MBS_ONLY);
	hw_ps->sps.mb_adaptive_frame_field_flag =
		!!(sps->flags & V4L2_H264_SPS_FLAG_MB_ADAPTIVE_FRAME_FIELD);
	hw_ps->sps.direct_8x8_inference_flag =
		!!(sps->flags & V4L2_H264_SPS_FLAG_DIRECT_8X8_INFERENCE);

	/* write pps */
	hw_ps->pps.pic_parameter_set_id = pps->pic_parameter_set_id;
	hw_ps->pps.pps_seq_parameter_set_id = pps->seq_parameter_set_id;
	hw_ps->pps.entropy_coding_mode_flag =
		!!(pps->flags & V4L2_H264_PPS_FLAG_ENTROPY_CODING_MODE);
	hw_ps->pps.bottom_field_pic_order_in_frame_present_flag =
		!!(pps->flags & V4L2_H264_PPS_FLAG_BOTTOM_FIELD_PIC_ORDER_IN_FRAME_PRESENT);
	hw_ps->pps.num_ref_idx_l0_default_active_minus1 =
		pps->num_ref_idx_l0_default_active_minus1;
	hw_ps->pps.num_ref_idx_l1_default_active_minus1 =
		pps->num_ref_idx_l1_default_active_minus1;
	hw_ps->pps.weighted_pred_flag =
		!!(pps->flags & V4L2_H264_PPS_FLAG_WEIGHTED_PRED);
	hw_ps->pps.weighted_bipred_idc = pps->weighted_bipred_idc;
	hw_ps->pps.pic_init_qp_minus26 = pps->pic_init_qp_minus26;
	hw_ps->pps.pic_init_qs_minus26 = pps->pic_init_qs_minus26;
	hw_ps->pps.chroma_qp_index_offset = pps->chroma_qp_index_offset;
	hw_ps->pps.deblocking_filter_control_present_flag =
		!!(pps->flags & V4L2_H264_PPS_FLAG_DEBLOCKING_FILTER_CONTROL_PRESENT);
	hw_ps->pps.constrained_intra_pred_flag =
		!!(pps->flags & V4L2_H264_PPS_FLAG_CONSTRAINED_INTRA_PRED);
	hw_ps->pps.redundant_pic_cnt_present =
		!!(pps->flags & V4L2_H264_PPS_FLAG_REDUNDANT_PIC_CNT_PRESENT);
	hw_ps->pps.transform_8x8_mode_flag =
		!!(pps->flags & V4L2_H264_PPS_FLAG_TRANSFORM_8X8_MODE);
	hw_ps->pps.second_chroma_qp_index_offset = pps->second_chroma_qp_index_offset;
	hw_ps->pps.scaling_list_enable_flag =
		!!(pps->flags & V4L2_H264_PPS_FLAG_SCALING_MATRIX_PRESENT);

	/*
	 * To be on the safe side, program the scaling matrix address
	 *
	 * With this set here,
	 *  RKVDEC_SWREG12_SENCODARY_EN:sw_scanlist_addr_valid_en
	 * can stay at 0
	 */
	scaling_distance = offsetof(struct rkvdec2_h264_priv_tbl, scaling_list);
	scaling_list_address = h264_ctx->priv_tbl.dma + scaling_distance;
	hw_ps->pps.scaling_list_address = scaling_list_address;

	for (i = 0; i < ARRAY_SIZE(dec_params->dpb); i++) {
		if (dpb[i].flags & V4L2_H264_DPB_ENTRY_FLAG_LONG_TERM)
			hw_ps->pps.is_longterm |= (1 << i);
	}
}

static void lookup_ref_buf_idx(struct rkvdec2_ctx *ctx,
			       struct rkvdec2_h264_run *run)
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
			if (!buf) {
				dev_dbg(ctx->dev->dev, "No buffer for reference_ts %llu",
					dpb[i].reference_ts);
			}
		}

		run->ref_buf[i] = buf;
	}
}

static void set_dpb_info(struct rkvdec2_rps_entry *entries,
			 u8 reflist,
			 u8 refnum,
			 u8 info,
			 bool bottom)
{
	struct rkvdec2_rps_entry *entry = &entries[(reflist * 4) + refnum / 8];
	u8 idx = refnum % 8;

	switch (idx) {
	case 0:
		entry->dpb_info0 = info;
		entry->bottom_flag0 = bottom;
		break;
	case 1:
		entry->dpb_info1 = info;
		entry->bottom_flag1 = bottom;
		break;
	case 2:
		entry->dpb_info2 = info;
		entry->bottom_flag2 = bottom;
		break;
	case 3:
		entry->dpb_info3 = info;
		entry->bottom_flag3 = bottom;
		break;
	case 4:
		entry->dpb_info4 = info;
		entry->bottom_flag4 = bottom;
		break;
	case 5:
		entry->dpb_info5 = info;
		entry->bottom_flag5 = bottom;
		break;
	case 6:
		entry->dpb_info6 = info;
		entry->bottom_flag6 = bottom;
		break;
	case 7:
		entry->dpb_info7 = info;
		entry->bottom_flag7 = bottom;
		break;
	}
}

static void assemble_hw_rps(struct rkvdec2_ctx *ctx,
			    struct v4l2_h264_reflist_builder *builder,
			    struct rkvdec2_h264_run *run)
{
	const struct v4l2_ctrl_h264_decode_params *dec_params = run->decode_params;
	const struct v4l2_h264_dpb_entry *dpb = dec_params->dpb;
	struct rkvdec2_h264_ctx *h264_ctx = ctx->priv;
	struct rkvdec2_h264_priv_tbl *priv_tbl = h264_ctx->priv_tbl.cpu;

	struct rkvdec2_rps *hw_rps = &priv_tbl->rps;
	u32 i, j;

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

		hw_rps->frame_num[i] = builder->refs[i].frame_num;
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

			dpb_valid = !!(run->ref_buf[ref->index]);
			bottom = ref->fields == V4L2_H264_BOTTOM_FIELD_REF;

			set_dpb_info(hw_rps->entries, j, i, ref->index | (dpb_valid << 4), bottom);
		}
	}
}

static void assemble_hw_scaling_list(struct rkvdec2_ctx *ctx,
				     struct rkvdec2_h264_run *run)
{
	const struct v4l2_ctrl_h264_scaling_matrix *scaling = run->scaling_matrix;
	const struct v4l2_ctrl_h264_pps *pps = run->pps;
	struct rkvdec2_h264_ctx *h264_ctx = ctx->priv;
	struct rkvdec2_h264_priv_tbl *tbl = h264_ctx->priv_tbl.cpu;

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

static inline void rkvdec2_memcpy_toio(void __iomem *dst, void *src, size_t len)
{
#ifdef CONFIG_ARM64
	__iowrite32_copy(dst, src, len);
#else
	memcpy_toio(dst, src, len);
#endif
}

static void rkvdec2_write_regs(struct rkvdec2_ctx *ctx)
{
	struct rkvdec2_dev *rkvdec = ctx->dev;
	struct rkvdec2_h264_ctx *h264_ctx = ctx->priv;

	rkvdec2_memcpy_toio(rkvdec->regs + OFFSET_COMMON_REGS,
			    &h264_ctx->regs.common,
			    sizeof(h264_ctx->regs.common));
	rkvdec2_memcpy_toio(rkvdec->regs + OFFSET_CODEC_PARAMS_REGS,
			    &h264_ctx->regs.h264_param,
			    sizeof(h264_ctx->regs.h264_param));
	rkvdec2_memcpy_toio(rkvdec->regs + OFFSET_COMMON_ADDR_REGS,
			    &h264_ctx->regs.common_addr,
			    sizeof(h264_ctx->regs.common_addr));
	rkvdec2_memcpy_toio(rkvdec->regs + OFFSET_CODEC_ADDR_REGS,
			    &h264_ctx->regs.h264_addr,
			    sizeof(h264_ctx->regs.h264_addr));
	rkvdec2_memcpy_toio(rkvdec->regs + OFFSET_POC_HIGHBIT_REGS,
			    &h264_ctx->regs.h264_highpoc,
			    sizeof(h264_ctx->regs.h264_highpoc));
}

static void config_registers(struct rkvdec2_ctx *ctx,
			     struct rkvdec2_h264_run *run)
{
	const struct v4l2_ctrl_h264_decode_params *dec_params = run->decode_params;
	const struct v4l2_ctrl_h264_sps *sps = run->sps;
	const struct v4l2_h264_dpb_entry *dpb = dec_params->dpb;
	struct rkvdec2_h264_ctx *h264_ctx = ctx->priv;
	dma_addr_t priv_start_addr = h264_ctx->priv_tbl.dma;
	const struct v4l2_pix_format_mplane *dst_fmt;
	struct vb2_v4l2_buffer *src_buf = run->base.bufs.src;
	struct vb2_v4l2_buffer *dst_buf = run->base.bufs.dst;
	struct rkvdec2_regs_h264 *regs = &h264_ctx->regs;
	const struct v4l2_format *f;
	dma_addr_t rlc_addr;
	dma_addr_t dst_addr;
	u32 hor_virstride = 0;
	u32 ver_virstride = 0;
	u32 y_virstride = 0;
	u32 offset;
	u32 pixels;
	u32 i;

	memset(regs, 0, sizeof(*regs));

	/* Set H264 mode */
	regs->common.reg009.dec_mode = RKVDEC2_MODE_H264;

	/* Set config */
	regs->common.reg011.buf_empty_en = 1;
	regs->common.reg011.dec_clkgate_e = 1;
	regs->common.reg011.dec_timeout_e = 1;
	regs->common.reg011.pix_range_detection_e = 1;

	/* Set IDR flag */
	regs->common.reg013.cur_pic_is_idr =
		!!(dec_params->flags & V4L2_H264_DECODE_PARAM_FLAG_IDR_PIC);

	/* Set input stream length */
	regs->common.stream_len = vb2_get_plane_payload(&src_buf->vb2_buf, 0);

	/* Set max slice number */
	regs->common.reg017.slice_num = MAX_SLICE_NUMBER;

	/* Set strides */
	f = &ctx->decoded_fmt;
	dst_fmt = &f->fmt.pix_mp;
	hor_virstride = (sps->bit_depth_luma_minus8 + 8) * dst_fmt->width / 8;
	ver_virstride = round_up(dst_fmt->height, 16);
	y_virstride = hor_virstride * ver_virstride;
	pixels = dst_fmt->height * dst_fmt->width;

	regs->common.reg018.y_hor_virstride = hor_virstride / 16;
	regs->common.reg019.uv_hor_virstride = hor_virstride / 16;
	regs->common.reg020.y_virstride = y_virstride / 16;

	/* Activate block gating */
	regs->common.reg026.swreg_block_gating_e = 0xfffef;
	regs->common.reg026.reg_cfg_gating_en = 1;

	/* Set timeout threshold */
	if (pixels < RKVDEC2_1080P_PIXELS)
		regs->common.timeout_threshold = RKVDEC2_TIMEOUT_1080p;
	else if (pixels < RKVDEC2_4K_PIXELS)
		regs->common.timeout_threshold = RKVDEC2_TIMEOUT_4K;
	else if (pixels < RKVDEC2_8K_PIXELS)
		regs->common.timeout_threshold = RKVDEC2_TIMEOUT_8K;
	else
		regs->common.timeout_threshold = RKVDEC2_TIMEOUT_MAX;

	/* Set TOP and BOTTOM POCs */
	regs->h264_param.cur_top_poc = dec_params->top_field_order_cnt;
	regs->h264_param.cur_bot_poc = dec_params->bottom_field_order_cnt;

	/* Set ref pic address & poc */
	for (i = 0; i < ARRAY_SIZE(dec_params->dpb); i++) {
		struct vb2_buffer *vb_buf = run->ref_buf[i];
		dma_addr_t buf_dma;

		/*
		 * If a DPB entry is unused or invalid, address of current destination
		 * buffer is returned.
		 */
		if (!vb_buf)
			vb_buf = &dst_buf->vb2_buf;

		buf_dma = vb2_dma_contig_plane_dma_addr(vb_buf, 0);

		/* Set reference addresses */
		regs->h264_addr.ref_base[i] = buf_dma;

		/* Set COLMV addresses */
		regs->h264_addr.colmv_base[i] = buf_dma + ctx->colmv_offset;

		struct rkvdec2_h264_ref_info *ref_info =
			&regs->h264_param.ref_info_regs[i / 4].ref_info[i % 4];

		ref_info->ref_field =
			!!(dpb[i].flags & V4L2_H264_DPB_ENTRY_FLAG_FIELD);
		ref_info->ref_colmv_use_flag =
			!!(dpb[i].flags & V4L2_H264_DPB_ENTRY_FLAG_ACTIVE);
		ref_info->ref_topfield_used =
			!!(dpb[i].fields & V4L2_H264_TOP_FIELD_REF);
		ref_info->ref_botfield_used =
			!!(dpb[i].fields & V4L2_H264_BOTTOM_FIELD_REF);

		regs->h264_param.ref_pocs[i * 2] =
			dpb[i].top_field_order_cnt;
		regs->h264_param.ref_pocs[i * 2 + 1] =
			dpb[i].bottom_field_order_cnt;
	}

	/* Set rlc base address (input stream) */
	rlc_addr = vb2_dma_contig_plane_dma_addr(&src_buf->vb2_buf, 0);
	regs->common_addr.rlc_base = rlc_addr;
	regs->common_addr.rlcwrite_base = rlc_addr;

	/* Set output base address */
	dst_addr = vb2_dma_contig_plane_dma_addr(&dst_buf->vb2_buf, 0);
	regs->common_addr.decout_base = dst_addr;

	/* Set colmv address */
	regs->common_addr.colmv_cur_base = dst_addr + ctx->colmv_offset;

	/* Set RCB addresses */
	for (i = 0; i < RKVDEC2_RCB_COUNT; i++)
		regs->common_addr.rcb_base[i] = ctx->rcb_bufs[i].dma;

	/* Set hw pps address */
	offset = offsetof(struct rkvdec2_h264_priv_tbl, param_set);
	regs->h264_addr.pps_base = priv_start_addr + offset;

	/* Set hw rps address */
	offset = offsetof(struct rkvdec2_h264_priv_tbl, rps);
	regs->h264_addr.rps_base = priv_start_addr + offset;

	/* Set cabac table */
	offset = offsetof(struct rkvdec2_h264_priv_tbl, cabac_table);
	regs->h264_addr.cabactbl_base = priv_start_addr + offset;

	rkvdec2_write_regs(ctx);
}

#define RKVDEC_H264_MAX_DEPTH_IN_BYTES		2

static int rkvdec2_h264_adjust_fmt(struct rkvdec2_ctx *ctx,
				   struct v4l2_format *f)
{
	struct v4l2_pix_format_mplane *fmt = &f->fmt.pix_mp;

	fmt->num_planes = 1;
	if (!fmt->plane_fmt[0].sizeimage)
		fmt->plane_fmt[0].sizeimage = fmt->width * fmt->height *
					      RKVDEC_H264_MAX_DEPTH_IN_BYTES;
	return 0;
}

static int rkvdec2_h264_validate_sps(struct rkvdec2_ctx *ctx,
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
	 * which is half the final height (see (7-8) in the
	 * specification)
	 */
	if (!(sps->flags & V4L2_H264_SPS_FLAG_FRAME_MBS_ONLY))
		height *= 2;

	if (width > ctx->coded_fmt.fmt.pix_mp.width ||
	    height > ctx->coded_fmt.fmt.pix_mp.height)
		return -EINVAL;

	return 0;
}

static int rkvdec2_h264_start(struct rkvdec2_ctx *ctx)
{
	struct rkvdec2_dev *rkvdec = ctx->dev;
	struct rkvdec2_h264_priv_tbl *priv_tbl;
	struct rkvdec2_h264_ctx *h264_ctx;
	struct v4l2_ctrl *ctrl;
	int ret;

	ctrl = v4l2_ctrl_find(&ctx->ctrl_hdl,
			      V4L2_CID_STATELESS_H264_SPS);
	if (!ctrl)
		return -EINVAL;

	ret = rkvdec2_h264_validate_sps(ctx, ctrl->p_new.p_h264_sps);
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

static void rkvdec2_h264_stop(struct rkvdec2_ctx *ctx)
{
	struct rkvdec2_h264_ctx *h264_ctx = ctx->priv;
	struct rkvdec2_dev *rkvdec = ctx->dev;

	dma_free_coherent(rkvdec->dev, h264_ctx->priv_tbl.size,
			  h264_ctx->priv_tbl.cpu, h264_ctx->priv_tbl.dma);
	kfree(h264_ctx);
}

static void rkvdec2_h264_run_preamble(struct rkvdec2_ctx *ctx,
				      struct rkvdec2_h264_run *run)
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

	rkvdec2_run_preamble(ctx, &run->base);
}

static int rkvdec2_h264_run(struct rkvdec2_ctx *ctx)
{
	struct v4l2_h264_reflist_builder reflist_builder;
	struct rkvdec2_dev *rkvdec = ctx->dev;
	struct rkvdec2_h264_ctx *h264_ctx = ctx->priv;
	struct rkvdec2_h264_run run;
	uint32_t watchdog_time;

	rkvdec2_h264_run_preamble(ctx, &run);

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

	rkvdec2_run_postamble(ctx, &run.base);

	/* Set watchdog at 2 times the hardware timeout threshold */
	u64 timeout_threshold = h264_ctx->regs.common.timeout_threshold;
	watchdog_time = 2 * (1000 * timeout_threshold) / clk_get_rate(rkvdec->clocks[0].clk);
	schedule_delayed_work(&rkvdec->watchdog_work, msecs_to_jiffies(watchdog_time));

	/* Start decoding! */
	writel(RKVDEC2_REG_DEC_E_BIT, rkvdec->regs + RKVDEC2_REG_DEC_E);

	return 0;
}

static int rkvdec2_h264_try_ctrl(struct rkvdec2_ctx *ctx, struct v4l2_ctrl *ctrl)
{
	if (ctrl->id == V4L2_CID_STATELESS_H264_SPS)
		return rkvdec2_h264_validate_sps(ctx, ctrl->p_new.p_h264_sps);

	return 0;
}

const struct rkvdec2_coded_fmt_ops rkvdec2_h264_fmt_ops = {
	.adjust_fmt = rkvdec2_h264_adjust_fmt,
	.start = rkvdec2_h264_start,
	.stop = rkvdec2_h264_stop,
	.run = rkvdec2_h264_run,
	.try_ctrl = rkvdec2_h264_try_ctrl,
};
