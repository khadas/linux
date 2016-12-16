#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/errno.h>

#include "vdec.h"
#include "vdec_reg.h"
#include "amvdec.h"

#include "h264_dpb.h"

#undef pr_info
#define pr_info printk
int dpb_print(int index, int debug_flag, const char *fmt, ...)
{
	if (((h264_debug_flag & debug_flag) &&
		((1 << index) & h264_debug_mask))
		|| (debug_flag == PRINT_FLAG_ERROR)) {
		unsigned char buf[512];
		int len = 0;
		va_list args;
		va_start(args, fmt);
		len = sprintf(buf, "%d: ", index);
		vsnprintf(buf + len, 512-len, fmt, args);
		pr_info("%s", buf);
		va_end(args);
	}
	return 0;
}

int dpb_print_cont(int index, int debug_flag, const char *fmt, ...)
{
	if (((h264_debug_flag & debug_flag) &&
		((1 << index) & h264_debug_mask))
		|| (debug_flag == PRINT_FLAG_ERROR)) {
		unsigned char buf[512];
		int len = 0;
		va_list args;
		va_start(args, fmt);
		vsnprintf(buf + len, 512-len, fmt, args);
		pr_info("%s", buf);
		va_end(args);
	}
	return 0;
}

unsigned char dpb_is_debug(int index, int debug_flag)
{
	if (((h264_debug_flag & debug_flag) &&
		((1 << index) & h264_debug_mask))
		|| (debug_flag == PRINT_FLAG_ERROR))
		return 1;
	return 0;
}

#define CHECK_VALID(list_size, mark) {\
	if (list_size > MAX_LIST_SIZE || list_size < 0) { \
		dpb_print(p_H264_Dpb->decoder_index, PRINT_FLAG_ERROR, \
		"%s(%d): listXsize[%d] %d is larger than max size\r\n",\
		__func__, __LINE__, mark, list_size);\
		list_size = 0; \
	} \
	}

static struct DecRefPicMarking_s
	dummy_dec_ref_pic_marking_buffer
	[DEC_REF_PIC_MARKING_BUFFER_NUM_MAX];
static struct StorablePicture dummy_pic;
static struct FrameStore dummy_fs;
static struct StorablePicture *get_new_pic(
	struct h264_dpb_stru *p_H264_Dpb,
	enum PictureStructure structure, unsigned char is_output);
static void dump_dpb(struct DecodedPictureBuffer *p_Dpb);

static void init_dummy_fs(void)
{
	dummy_fs.frame = &dummy_pic;
	dummy_fs.top_field = &dummy_pic;
	dummy_fs.bottom_field = &dummy_pic;

	dummy_pic.top_field = &dummy_pic;
	dummy_pic.bottom_field = &dummy_pic;
	dummy_pic.frame = &dummy_pic;

	dummy_pic.dec_ref_pic_marking_buffer =
		&dummy_dec_ref_pic_marking_buffer[0];
}

enum {
	LIST_0 = 0,
	LIST_1 = 1,
	BI_PRED = 2,
	BI_PRED_L0 = 3,
	BI_PRED_L1 = 4
};

void ref_pic_list_reordering(struct h264_dpb_stru *p_H264_Dpb,
				struct Slice *currSlice)
{
	/* struct VideoParameters *p_Vid = currSlice->p_Vid;
	   byte dP_nr = assignSE2partition[currSlice->dp_mode][SE_HEADER];
	   DataPartition *partition = &(currSlice->partArr[dP_nr]);
	   Bitstream *currStream = partition->bitstream;
	 */
	int i, j, val;
	unsigned short *reorder_cmd =
		&p_H264_Dpb->dpb_param.mmco.l0_reorder_cmd[0];
	/* alloc_ref_pic_list_reordering_buffer(currSlice); */
	dpb_print(p_H264_Dpb->decoder_index, PRINT_FLAG_DPB_DETAIL,
		"%s\n", __func__);
	if (currSlice->slice_type != I_SLICE &&
		currSlice->slice_type != SI_SLICE) {
		/* val = currSlice->ref_pic_list_reordering_flag[LIST_0] =
			read_u_1 ("SH: ref_pic_list_reordering_flag_l0",
				currStream, &p_Dec->UsedBits); */
		if (reorder_cmd[0] != 3) {
			val = currSlice->
				ref_pic_list_reordering_flag[LIST_0] = 1;
		} else {
			val = currSlice->
				ref_pic_list_reordering_flag[LIST_0] = 0;
		}
		if (val) {
			dpb_print(p_H264_Dpb->decoder_index,
				PRINT_FLAG_DPB_DETAIL,
				"%s, ref_pic_list_reordering_flag[LIST_0] is 1\n",
				__func__);

			j = 0;
			i = 0;
			do {
				val = currSlice->
				modification_of_pic_nums_idc[LIST_0][i] =
					      reorder_cmd[j++];
				/* read_ue_v(
					"SH: modification_of_pic_nums_idc_l0",
					currStream, &p_Dec->UsedBits); */
				dpb_print(p_H264_Dpb->decoder_index,
					PRINT_FLAG_DPB_DETAIL,
					"%d(%d):val %x\n", i, j, val);
				if (j >= 66) {
					currSlice->
					ref_pic_list_reordering_flag[LIST_0] =
					0; /* by rain */
					dpb_print(p_H264_Dpb->decoder_index,
						PRINT_FLAG_ERROR,
						"%s error\n", __func__);
					break;
				}
				if (val == 0 || val == 1) {
					currSlice->
					abs_diff_pic_num_minus1[LIST_0][i] =
					reorder_cmd[j++];
					/* read_ue_v("SH: "
					"abs_diff_pic_num_minus1_l0",
					currStream, &p_Dec->UsedBits); */
				} else {
					if (val == 2) {
						currSlice->
						long_term_pic_idx[LIST_0][i] =
						reorder_cmd[j++];
						/* read_ue_v(
						"SH: long_term_pic_idx_l0",
						currStream,
						&p_Dec->UsedBits); */
					}
				}
				i++;
				/* assert (i>currSlice->
					num_ref_idx_active[LIST_0]); */
				if (/*
				     i>currSlice->num_ref_idx_active[LIST_0] ||
				     */
					i >= REORDERING_COMMAND_MAX_SIZE) {
					dpb_print(p_H264_Dpb->decoder_index,
						PRINT_FLAG_ERROR,
						"%s error %d %d\n",
						__func__, i,
						currSlice->
						num_ref_idx_active[LIST_0]);
					currSlice->
					ref_pic_list_reordering_flag[LIST_0] =
					0; /* by rain */
					break;
				}
				if (j >= 66) {
					dpb_print(p_H264_Dpb->decoder_index,
						PRINT_FLAG_ERROR, "%s error\n",
						__func__);
					currSlice->
					ref_pic_list_reordering_flag[LIST_0] =
					0; /* by rain */
					break;
				}

			} while (val != 3);
		}
	}

	if (currSlice->slice_type == B_SLICE) {
		reorder_cmd = &p_H264_Dpb->dpb_param.mmco.l1_reorder_cmd[0];
		/* val = currSlice->ref_pic_list_reordering_flag[LIST_1]
		= read_u_1 ("SH: ref_pic_list_reordering_flag_l1",
		currStream,
		&p_Dec->UsedBits); */

		if (reorder_cmd[0] != 3) {
			val =
			currSlice->ref_pic_list_reordering_flag[LIST_1] = 1;
		} else {
			val =
			currSlice->ref_pic_list_reordering_flag[LIST_1] = 0;
		}

		if (val) {
			dpb_print(p_H264_Dpb->decoder_index,
				PRINT_FLAG_DPB_DETAIL,
				"%s, ref_pic_list_reordering_flag[LIST_1] is 1\n",
				__func__);

			j = 0;
			i = 0;
			do {
				val = currSlice->
				modification_of_pic_nums_idc[LIST_1][i] =
				reorder_cmd[j++];
				/* read_ue_v(
				"SH: modification_of_pic_nums_idc_l1",
				currStream,
				&p_Dec->UsedBits); */
				dpb_print(p_H264_Dpb->decoder_index,
					PRINT_FLAG_DPB_DETAIL,
					"%d(%d):val %x\n",
					i, j, val);
				if (j >= 66) {
					dpb_print(p_H264_Dpb->decoder_index,
						PRINT_FLAG_ERROR, "%s error\n",
						__func__);
					currSlice->
					ref_pic_list_reordering_flag[LIST_1] =
					0;  /* by rain */
					break;
				}
				if (val == 0 || val == 1) {
					currSlice->
					abs_diff_pic_num_minus1[LIST_1][i] =
						reorder_cmd[j++];
					/* read_ue_v(
					"SH: abs_diff_pic_num_minus1_l1",
					currStream, &p_Dec->UsedBits); */
				} else {
					if (val == 2) {
						currSlice->
						long_term_pic_idx[LIST_1][i] =
						reorder_cmd[j++];
						/* read_ue_v(
						"SH: long_term_pic_idx_l1",
						currStream,
						&p_Dec->UsedBits); */
					}
				}
				i++;
				/* assert(i>currSlice->
					num_ref_idx_active[LIST_1]); */
				if (
				/*i>currSlice->num_ref_idx_active[LIST_1] || */
					i >= REORDERING_COMMAND_MAX_SIZE) {
					dpb_print(p_H264_Dpb->decoder_index,
						PRINT_FLAG_ERROR,
						"%s error %d %d\n",
						__func__, i,
						currSlice->
						num_ref_idx_active[LIST_0]);
					currSlice->
					ref_pic_list_reordering_flag[LIST_1] =
					0;  /* by rain */
					break;
				}
				if (j >= 66) {
					dpb_print(p_H264_Dpb->decoder_index,
						PRINT_FLAG_ERROR,
						"%s error\n", __func__);
					break;
				}
			} while (val != 3);
		}
	}

	/* set reference index of redundant slices. */
	/*
	if(currSlice->redundant_pic_cnt && (currSlice->slice_type != I_SLICE))
	{
	  currSlice->redundant_slice_ref_idx =
		currSlice->abs_diff_pic_num_minus1[LIST_0][0] + 1;
	}*/
}

void slice_prepare(struct h264_dpb_stru *p_H264_Dpb,
		struct DecodedPictureBuffer *p_Dpb,
		struct VideoParameters *p_Vid,
		struct SPSParameters *sps, struct Slice *pSlice)
{
	int i, j;
	/* p_Vid->active_sps = sps; */
	unsigned short *mmco_cmd = &p_H264_Dpb->dpb_param.mmco.mmco_cmd[0];
	/* for decode_poc */
	sps->pic_order_cnt_type =
		p_H264_Dpb->dpb_param.l.data[PIC_ORDER_CNT_TYPE];
	sps->log2_max_pic_order_cnt_lsb_minus4 =
		p_H264_Dpb->dpb_param.l.data[LOG2_MAX_PIC_ORDER_CNT_LSB] - 4;
	sps->num_ref_frames_in_pic_order_cnt_cycle =
		p_H264_Dpb->
		dpb_param.l.data[NUM_REF_FRAMES_IN_PIC_ORDER_CNT_CYCLE];
	for (i = 0; i < 128; i++)
		sps->offset_for_ref_frame[i] =
			(short) p_H264_Dpb->
			dpb_param.mmco.offset_for_ref_frame_base[i];
	sps->offset_for_non_ref_pic =
		(short) p_H264_Dpb->dpb_param.l.data[OFFSET_FOR_NON_REF_PIC];
	sps->offset_for_top_to_bottom_field =
		(short) p_H264_Dpb->dpb_param.l.data
		[OFFSET_FOR_TOP_TO_BOTTOM_FIELD];

	pSlice->frame_num = p_H264_Dpb->dpb_param.dpb.frame_num;
	pSlice->idr_flag =
		(p_H264_Dpb->dpb_param.dpb.NAL_info_mmco & 0x1f)
		== 5 ? 1 : 0;
	pSlice->nal_reference_idc =
		(p_H264_Dpb->dpb_param.dpb.NAL_info_mmco >> 5)
		& 0x3;
	pSlice->pic_order_cnt_lsb =
		p_H264_Dpb->dpb_param.dpb.pic_order_cnt_lsb;
	pSlice->field_pic_flag = 0;
	pSlice->bottom_field_flag = 0;
	pSlice->delta_pic_order_cnt_bottom = val(
		p_H264_Dpb->dpb_param.dpb.delta_pic_order_cnt_bottom);
	pSlice->delta_pic_order_cnt[0] = val(
		p_H264_Dpb->dpb_param.dpb.delta_pic_order_cnt_0);
	pSlice->delta_pic_order_cnt[1] = val(
		p_H264_Dpb->dpb_param.dpb.delta_pic_order_cnt_1);

	p_Vid->last_has_mmco_5 = 0;
	/* last memory_management_control_operation is 5 */
	p_Vid->last_pic_bottom_field = 0;
	p_Vid->max_frame_num = 1 <<
	(p_H264_Dpb->dpb_param.l.data[LOG2_MAX_FRAME_NUM]);

	/**/
	pSlice->structure = (p_H264_Dpb->
		dpb_param.l.data[NEW_PICTURE_STRUCTURE] == 3) ?
		FRAME : p_H264_Dpb->dpb_param.l.data[NEW_PICTURE_STRUCTURE];
	sps->num_ref_frames = p_H264_Dpb->
		dpb_param.l.data[MAX_REFERENCE_FRAME_NUM];
	sps->max_dpb_size = p_H264_Dpb->dpb_param.l.data[MAX_DPB_SIZE];
	if (pSlice->idr_flag) {
		pSlice->long_term_reference_flag = mmco_cmd[0] & 1;
		pSlice->no_output_of_prior_pics_flag = (mmco_cmd[0] >> 1) & 1;
		dpb_print(p_H264_Dpb->decoder_index,
		PRINT_FLAG_DPB_DETAIL,
		"IDR: long_term_reference_flag %d no_output_of_prior_pics_flag %d\r\n",
		pSlice->long_term_reference_flag,
		pSlice->no_output_of_prior_pics_flag);

		dpb_print(p_H264_Dpb->decoder_index,
		PRINT_FLAG_DPB_DETAIL,
		"idr set pre_frame_num(%d) to frame_num (%d)\n",
		p_Vid->pre_frame_num, pSlice->frame_num);

		p_Vid->pre_frame_num = pSlice->frame_num;
	}
	/* pSlice->adaptive_ref_pic_buffering_flag; */
	sps->log2_max_frame_num_minus4 =
		p_H264_Dpb->dpb_param.l.data[LOG2_MAX_FRAME_NUM] - 4;

	p_Vid->non_conforming_stream =
		p_H264_Dpb->dpb_param.l.data[NON_CONFORMING_STREAM];
	p_Vid->recovery_point =
		p_H264_Dpb->dpb_param.l.data[RECOVERY_POINT];
	switch (p_H264_Dpb->dpb_param.l.data[SLICE_TYPE]) {
	case I_Slice:
		pSlice->slice_type = I_SLICE;
		break;
	case P_Slice:
		pSlice->slice_type = P_SLICE;
		break;
	case B_Slice:
		pSlice->slice_type = B_SLICE;
		break;
	default:
		pSlice->slice_type = NUM_SLICE_TYPES;
		break;
	}

	pSlice->num_ref_idx_active[LIST_0] =
		p_H264_Dpb->dpb_param.dpb.num_ref_idx_l0_active_minus1 +
		1;
	/* p_H264_Dpb->dpb_param.l.data[PPS_NUM_REF_IDX_L0_ACTIVE_MINUS1]; */
	pSlice->num_ref_idx_active[LIST_1] =
		p_H264_Dpb->dpb_param.dpb.num_ref_idx_l1_active_minus1 +
		1;
	/* p_H264_Dpb->dpb_param.l.data[PPS_NUM_REF_IDX_L1_ACTIVE_MINUS1]; */

	pSlice->p_Vid = p_Vid;
	pSlice->p_Dpb = p_Dpb;

	p_H264_Dpb->colocated_buf_size =
		p_H264_Dpb->dpb_param.l.data[FRAME_SIZE_IN_MB] * 96;
	pSlice->first_mb_in_slice =
		p_H264_Dpb->dpb_param.l.data[FIRST_MB_IN_SLICE];
	pSlice->mode_8x8_flags = p_H264_Dpb->dpb_param.l.data[MODE_8X8_FLAGS];
	pSlice->picture_structure_mmco =
		p_H264_Dpb->dpb_param.dpb.picture_structure_mmco;
	dpb_print(p_H264_Dpb->decoder_index, PRINT_FLAG_DPB_DETAIL,
		  "%s slice_type is %d, num_ref_idx_active[0]=%d, num_ref_idx_active[1]=%d nal_reference_idc %d\n",
		  __func__, pSlice->slice_type,
		  pSlice->num_ref_idx_active[LIST_0],
		  pSlice->num_ref_idx_active[LIST_1],
		  pSlice->nal_reference_idc);
#ifdef ERROR_CHECK
	if (pSlice->num_ref_idx_active[LIST_0] >= MAX_LIST_SIZE)
		pSlice->num_ref_idx_active[LIST_0] = MAX_LIST_SIZE - 1;
	if (pSlice->num_ref_idx_active[LIST_1] >= MAX_LIST_SIZE)
		pSlice->num_ref_idx_active[LIST_1] = MAX_LIST_SIZE - 1;
#endif

#if 1
	/* dec_ref_pic_marking_buffer */
	pSlice->adaptive_ref_pic_buffering_flag = 0;
	if (pSlice->nal_reference_idc) {
		for (i = 0, j = 0; i < 44; j++) {
			unsigned short val;
			struct DecRefPicMarking_s *tmp_drpm =
				&pSlice->dec_ref_pic_marking_buffer[j];
			memset(tmp_drpm, 0, sizeof(struct DecRefPicMarking_s));
			val = tmp_drpm->
				memory_management_control_operation =
					mmco_cmd[i++];
			tmp_drpm->Next = NULL;
			if (j > 0) {
				pSlice->
				dec_ref_pic_marking_buffer[j - 1].Next =
					tmp_drpm;
			}
			if (val == 0 || i >= 44)
				break;
			pSlice->adaptive_ref_pic_buffering_flag = 1;
			if ((val == 1) || (val == 3)) {
				tmp_drpm->difference_of_pic_nums_minus1 =
					mmco_cmd[i++];
			}
			if (val == 2)
				tmp_drpm->long_term_pic_num = mmco_cmd[i++];
			if (i >= 44)
				break;
			if ((val == 3) || (val == 6))
				tmp_drpm->long_term_frame_idx = mmco_cmd[i++];
			if (val == 4) {
				tmp_drpm->max_long_term_frame_idx_plus1 =
					mmco_cmd[i++];
			}
			dpb_print(p_H264_Dpb->decoder_index,
				PRINT_FLAG_DPB_DETAIL,
				"dec_ref_pic_marking_buffer[%d]:operation %x diff_pic_minus1 %x long_pic_num %x long_frame_idx %x max_long_frame_idx_plus1 %x\n",
				j,
				tmp_drpm->memory_management_control_operation,
				tmp_drpm->difference_of_pic_nums_minus1,
				tmp_drpm->long_term_pic_num,
				tmp_drpm->long_term_frame_idx,
				tmp_drpm->max_long_term_frame_idx_plus1);
		}
	}

	ref_pic_list_reordering(p_H264_Dpb, pSlice);
#endif

	/*VUI*/
	p_H264_Dpb->vui_status = p_H264_Dpb->dpb_param.l.data[VUI_STATUS];
	p_H264_Dpb->aspect_ratio_idc =
		p_H264_Dpb->dpb_param.l.data[ASPECT_RATIO_IDC];
	p_H264_Dpb->aspect_ratio_sar_width =
		p_H264_Dpb->dpb_param.l.data[ASPECT_RATIO_SAR_WIDTH];
	p_H264_Dpb->aspect_ratio_sar_height =
		p_H264_Dpb->dpb_param.l.data[ASPECT_RATIO_SAR_HEIGHT];

	p_H264_Dpb->fixed_frame_rate_flag = p_H264_Dpb->dpb_param.l.data[
		FIXED_FRAME_RATE_FLAG];
	p_H264_Dpb->num_units_in_tick =
		p_H264_Dpb->dpb_param.l.data[NUM_UNITS_IN_TICK];
	p_H264_Dpb->time_scale = p_H264_Dpb->dpb_param.l.data[TIME_SCALE] |
		(p_H264_Dpb->dpb_param.l.data[TIME_SCALE + 1] << 16);
	/**/
	dpb_print(p_H264_Dpb->decoder_index, PRINT_FLAG_DPB_DETAIL,
		"%s return\n", __func__);
}

static void decode_poc(struct VideoParameters *p_Vid, struct Slice *pSlice)
{
	struct h264_dpb_stru *p_H264_Dpb = container_of(p_Vid,
					struct h264_dpb_stru, mVideo);
	struct SPSParameters *active_sps = p_Vid->active_sps;
	int i;
	/* for POC mode 0: */
	unsigned int MaxPicOrderCntLsb = (1 <<
		(active_sps->log2_max_pic_order_cnt_lsb_minus4 + 4));

	dpb_print(p_H264_Dpb->decoder_index,
		PRINT_FLAG_DEBUG_POC,
		"%s:pic_order_cnt_type %d, idr_flag %d last_has_mmco_5 %d last_pic_bottom_field %d pic_order_cnt_lsb %d PrevPicOrderCntLsb %d\r\n",
		__func__,
		active_sps->pic_order_cnt_type,
		pSlice->idr_flag,
		p_Vid->last_has_mmco_5,
		p_Vid->last_pic_bottom_field,
		pSlice->pic_order_cnt_lsb,
		p_Vid->PrevPicOrderCntLsb
	);

	dpb_print(p_H264_Dpb->decoder_index, PRINT_FLAG_DEBUG_POC,
	"%s:field_pic_flag %d, bottom_field_flag %d frame_num %d PreviousFrameNum %d PreviousFrameNumOffset %d ax_frame_num %d num_ref_frames_in_pic_order_cnt_cycle %d offset_for_non_ref_pic %d\r\n",
		__func__,
		pSlice->field_pic_flag,
		pSlice->bottom_field_flag,
		pSlice->frame_num,
		p_Vid->PreviousFrameNum,
		p_Vid->PreviousFrameNumOffset,
		p_Vid->max_frame_num,
		active_sps->num_ref_frames_in_pic_order_cnt_cycle,
		active_sps->offset_for_non_ref_pic
	);

	dpb_print(p_H264_Dpb->decoder_index, PRINT_FLAG_DEBUG_POC,
	"%s: delta_pic_order_cnt %d %d nal_reference_idc %d\r\n",
	__func__,
	pSlice->delta_pic_order_cnt[0], pSlice->delta_pic_order_cnt[1],
	pSlice->nal_reference_idc
	);


	switch (active_sps->pic_order_cnt_type) {
	case 0: /* POC MODE 0 */
		/* 1st */
		if (pSlice->idr_flag) {
			p_Vid->PrevPicOrderCntMsb = 0;
			p_Vid->PrevPicOrderCntLsb = 0;
		} else {
			if (p_Vid->last_has_mmco_5) {
				if (p_Vid->last_pic_bottom_field) {
					p_Vid->PrevPicOrderCntMsb = 0;
					p_Vid->PrevPicOrderCntLsb = 0;
				} else {
					p_Vid->PrevPicOrderCntMsb = 0;
					p_Vid->PrevPicOrderCntLsb =
						pSlice->toppoc;
				}
			}
		}
		/* Calculate the MSBs of current picture */
		if (pSlice->pic_order_cnt_lsb < p_Vid->PrevPicOrderCntLsb &&
		    (p_Vid->PrevPicOrderCntLsb - pSlice->pic_order_cnt_lsb) >=
		    (MaxPicOrderCntLsb / 2))
			pSlice->PicOrderCntMsb = p_Vid->PrevPicOrderCntMsb +
					MaxPicOrderCntLsb;
		else if (pSlice->pic_order_cnt_lsb >
				p_Vid->PrevPicOrderCntLsb &&
			 (pSlice->pic_order_cnt_lsb -
				p_Vid->PrevPicOrderCntLsb)  >
				 (MaxPicOrderCntLsb / 2))
			pSlice->PicOrderCntMsb = p_Vid->PrevPicOrderCntMsb -
					MaxPicOrderCntLsb;
		else
			pSlice->PicOrderCntMsb = p_Vid->PrevPicOrderCntMsb;

		/* 2nd */
		if (pSlice->field_pic_flag == 0) {
			/* frame pix */
			pSlice->toppoc = pSlice->PicOrderCntMsb +
					pSlice->pic_order_cnt_lsb;
			pSlice->bottompoc = pSlice->toppoc +
					pSlice->delta_pic_order_cnt_bottom;
			pSlice->ThisPOC = pSlice->framepoc =
				(pSlice->toppoc < pSlice->bottompoc) ?
				 pSlice->toppoc : pSlice->bottompoc;
					/* POC200301 */
		} else if (pSlice->bottom_field_flag == FALSE) {
			/* top field */
			pSlice->ThisPOC = pSlice->toppoc =
				pSlice->PicOrderCntMsb +
				pSlice->pic_order_cnt_lsb;
		} else {
			/* bottom field */
			pSlice->ThisPOC = pSlice->bottompoc =
				pSlice->PicOrderCntMsb +
				pSlice->pic_order_cnt_lsb;
		}
		pSlice->framepoc = pSlice->ThisPOC;

		p_Vid->ThisPOC = pSlice->ThisPOC;

		/* if ( pSlice->frame_num != p_Vid->PreviousFrameNum)
			Seems redundant */
		p_Vid->PreviousFrameNum = pSlice->frame_num;

		if (pSlice->nal_reference_idc) {
			p_Vid->PrevPicOrderCntLsb = pSlice->pic_order_cnt_lsb;
			p_Vid->PrevPicOrderCntMsb = pSlice->PicOrderCntMsb;
		}

		break;

	case 1: /* POC MODE 1 */
		/* 1st */
		if (pSlice->idr_flag) {
			p_Vid->FrameNumOffset = 0;   /* first pix of IDRGOP */
			if (pSlice->frame_num)
				dpb_print(p_H264_Dpb->decoder_index,
					PRINT_FLAG_DPB_DETAIL,
					"frame_num not equal to zero in IDR picture %d",
					-1020);
		} else {
			if (p_Vid->last_has_mmco_5) {
				p_Vid->PreviousFrameNumOffset = 0;
				p_Vid->PreviousFrameNum = 0;
			}
			if (pSlice->frame_num < p_Vid->PreviousFrameNum) {
				/* not first pix of IDRGOP */
				p_Vid->FrameNumOffset =
					p_Vid->PreviousFrameNumOffset +
						p_Vid->max_frame_num;
			} else {
				p_Vid->FrameNumOffset =
					p_Vid->PreviousFrameNumOffset;
			}
		}

		/* 2nd */
		if (active_sps->num_ref_frames_in_pic_order_cnt_cycle)
			pSlice->AbsFrameNum =
				p_Vid->FrameNumOffset + pSlice->frame_num;
		else
			pSlice->AbsFrameNum = 0;
		if ((!pSlice->nal_reference_idc) && pSlice->AbsFrameNum > 0)
			pSlice->AbsFrameNum--;

		/* 3rd */
		p_Vid->ExpectedDeltaPerPicOrderCntCycle = 0;

		if (active_sps->num_ref_frames_in_pic_order_cnt_cycle)
			for (i = 0; i < (int) active_sps->
				num_ref_frames_in_pic_order_cnt_cycle; i++) {
				p_Vid->ExpectedDeltaPerPicOrderCntCycle +=
					active_sps->offset_for_ref_frame[i];
				dpb_print(p_H264_Dpb->decoder_index,
					PRINT_FLAG_DEBUG_POC,
					"%s: offset_for_ref_frame %d\r\n",
					__func__,
					active_sps->
					offset_for_ref_frame[i]);
			}

		if (pSlice->AbsFrameNum) {
			p_Vid->PicOrderCntCycleCnt =
				(pSlice->AbsFrameNum - 1) /
				active_sps->
				num_ref_frames_in_pic_order_cnt_cycle;
			p_Vid->FrameNumInPicOrderCntCycle =
				(pSlice->AbsFrameNum - 1) %
				active_sps->
				num_ref_frames_in_pic_order_cnt_cycle;
			p_Vid->ExpectedPicOrderCnt =
				p_Vid->PicOrderCntCycleCnt *
				p_Vid->ExpectedDeltaPerPicOrderCntCycle;
			for (i = 0; i <= (int)p_Vid->
				FrameNumInPicOrderCntCycle; i++) {
				p_Vid->ExpectedPicOrderCnt +=
					active_sps->offset_for_ref_frame[i];
				dpb_print(p_H264_Dpb->decoder_index,
					PRINT_FLAG_DEBUG_POC,
					"%s: offset_for_ref_frame %d\r\n",
					__func__,
					active_sps->
					offset_for_ref_frame[i]);
			}
		} else
			p_Vid->ExpectedPicOrderCnt = 0;

		if (!pSlice->nal_reference_idc)
			p_Vid->ExpectedPicOrderCnt +=
				active_sps->offset_for_non_ref_pic;

		if (pSlice->field_pic_flag == 0) {
			/* frame pix */
			pSlice->toppoc = p_Vid->ExpectedPicOrderCnt +
				pSlice->delta_pic_order_cnt[0];
			pSlice->bottompoc = pSlice->toppoc +
				active_sps->offset_for_top_to_bottom_field +
				pSlice->delta_pic_order_cnt[1];
			pSlice->ThisPOC = pSlice->framepoc =
				(pSlice->toppoc < pSlice->bottompoc) ?
				pSlice->toppoc : pSlice->bottompoc;
				/* POC200301 */
		} else if (pSlice->bottom_field_flag == FALSE) {
			/* top field */
			pSlice->ThisPOC = pSlice->toppoc =
				p_Vid->ExpectedPicOrderCnt +
				pSlice->delta_pic_order_cnt[0];
		} else {
			/* bottom field */
			pSlice->ThisPOC = pSlice->bottompoc =
				p_Vid->ExpectedPicOrderCnt +
				active_sps->offset_for_top_to_bottom_field +
				pSlice->delta_pic_order_cnt[0];
		}
		pSlice->framepoc = pSlice->ThisPOC;

		p_Vid->PreviousFrameNum = pSlice->frame_num;
		p_Vid->PreviousFrameNumOffset = p_Vid->FrameNumOffset;

		break;


	case 2: /* POC MODE 2 */
		if (pSlice->idr_flag) { /* IDR picture */
			p_Vid->FrameNumOffset = 0;   /* first pix of IDRGOP */
			pSlice->ThisPOC = pSlice->framepoc = pSlice->toppoc =
				pSlice->bottompoc = 0;
			if (pSlice->frame_num)
				dpb_print(p_H264_Dpb->decoder_index,
					PRINT_FLAG_DPB_DETAIL,
					"frame_num not equal to zero in IDR picture %d",
					-1020);
		} else {
			if (p_Vid->last_has_mmco_5) {
				p_Vid->PreviousFrameNum = 0;
				p_Vid->PreviousFrameNumOffset = 0;
			}
			if (pSlice->frame_num < p_Vid->PreviousFrameNum)
				p_Vid->FrameNumOffset =
					p_Vid->PreviousFrameNumOffset +
					p_Vid->max_frame_num;
			else
				p_Vid->FrameNumOffset =
					p_Vid->PreviousFrameNumOffset;

			pSlice->AbsFrameNum = p_Vid->FrameNumOffset +
				pSlice->frame_num;
			if (!pSlice->nal_reference_idc)
				pSlice->ThisPOC =
					(2 * pSlice->AbsFrameNum - 1);
			else
				pSlice->ThisPOC = (2 * pSlice->AbsFrameNum);

			if (pSlice->field_pic_flag == 0)
				pSlice->toppoc = pSlice->bottompoc =
					pSlice->framepoc = pSlice->ThisPOC;
			else if (pSlice->bottom_field_flag == FALSE)
				pSlice->toppoc = pSlice->framepoc =
				pSlice->ThisPOC;
			else
				pSlice->bottompoc = pSlice->framepoc =
				pSlice->ThisPOC;
		}

		p_Vid->PreviousFrameNum = pSlice->frame_num;
		p_Vid->PreviousFrameNumOffset = p_Vid->FrameNumOffset;
		break;


	default:
		/* error must occurs */
		/* assert( 1==0 ); */
		break;
	}
}

void fill_frame_num_gap(struct VideoParameters *p_Vid, struct Slice *currSlice)
{
	struct h264_dpb_stru *p_H264_Dpb =
		container_of(p_Vid, struct h264_dpb_stru, mVideo);
	struct SPSParameters *active_sps = p_Vid->active_sps;
	int CurrFrameNum;
	int UnusedShortTermFrameNum;
	struct StorablePicture *picture = NULL;
	int tmp1 = currSlice->delta_pic_order_cnt[0];
	int tmp2 = currSlice->delta_pic_order_cnt[1];
	currSlice->delta_pic_order_cnt[0] =
		currSlice->delta_pic_order_cnt[1] = 0;

	dpb_print(p_H264_Dpb->decoder_index,
		PRINT_FLAG_DPB_DETAIL,
		"A gap in frame number is found, try to fill it.(pre_frame_num %d, max_frame_num %d\n",
		p_Vid->pre_frame_num, p_Vid->max_frame_num
	);

	UnusedShortTermFrameNum = (p_Vid->pre_frame_num + 1)
		% p_Vid->max_frame_num;
	CurrFrameNum = currSlice->frame_num; /*p_Vid->frame_num;*/

	while (CurrFrameNum != UnusedShortTermFrameNum) {
		/*picture = alloc_storable_picture
		(p_Vid, FRAME, p_Vid->width,
		p_Vid->height,
		p_Vid->width_cr,
		p_Vid->height_cr, 1);*/
		picture = get_new_pic(p_H264_Dpb,
			p_H264_Dpb->mSlice.structure,
		/*p_Vid->width, p_Vid->height,
		p_Vid->width_cr,
		p_Vid->height_cr,*/ 1);

		if (picture == NULL) {
			struct DecodedPictureBuffer *p_Dpb = &p_H264_Dpb->mDPB;
			dpb_print(p_H264_Dpb->decoder_index,
				PRINT_FLAG_ERROR,
				"%s Error: get_new_pic return NULL\r\n",
				__func__);
			h264_debug_flag |= PRINT_FLAG_DUMP_DPB;
			dump_dpb(p_Dpb);
			return;
		}

		picture->colocated_buf_index = -1;
		picture->buf_spec_num = -1;

		picture->coded_frame = 1;
		picture->pic_num = UnusedShortTermFrameNum;
		picture->frame_num = UnusedShortTermFrameNum;
		picture->non_existing = 1;
		picture->is_output = 1;
		picture->used_for_reference = 1;
		picture->adaptive_ref_pic_buffering_flag = 0;
		#if (MVC_EXTENSION_ENABLE)
		picture->view_id = currSlice->view_id;
		#endif

		currSlice->frame_num = UnusedShortTermFrameNum;
		if (active_sps->pic_order_cnt_type != 0) {
			/*decode_poc(p_Vid, p_Vid->ppSliceList[0]);*/
			decode_poc(&p_H264_Dpb->mVideo, &p_H264_Dpb->mSlice);
		}
		picture->top_poc    = currSlice->toppoc;
		picture->bottom_poc = currSlice->bottompoc;
		picture->frame_poc  = currSlice->framepoc;
		picture->poc        = currSlice->framepoc;

		store_picture_in_dpb(p_H264_Dpb, picture, OTHER_DATA);

		picture = NULL;
		p_Vid->pre_frame_num = UnusedShortTermFrameNum;
		UnusedShortTermFrameNum =
			(UnusedShortTermFrameNum + 1) %
			p_Vid->max_frame_num;
	}
	currSlice->delta_pic_order_cnt[0] = tmp1;
	currSlice->delta_pic_order_cnt[1] = tmp2;
	currSlice->frame_num = CurrFrameNum;
}

void dpb_init_global(struct h264_dpb_stru *p_H264_Dpb,
	int id, int actual_dpb_size, int max_reference_size)
{
	int i;
	init_dummy_fs();

	memset(&p_H264_Dpb->mDPB, 0, sizeof(struct DecodedPictureBuffer));

	memset(&p_H264_Dpb->mSlice, 0, sizeof(struct Slice));
	memset(&p_H264_Dpb->mVideo, 0, sizeof(struct VideoParameters));
	memset(&p_H264_Dpb->mSPS, 0, sizeof(struct SPSParameters));

	for (i = 0; i < DPB_SIZE_MAX; i++) {
		memset(&(p_H264_Dpb->mFrameStore[i]), 0,
			sizeof(struct FrameStore));
	}

	for (i = 0; i < MAX_PIC_BUF_NUM; i++) {
		memset(&(p_H264_Dpb->m_PIC[i]), 0,
			sizeof(struct StorablePicture));
		p_H264_Dpb->m_PIC[i].index = i;
	}
	p_H264_Dpb->decoder_index = id;

    /* make sure dpb_init_global
    can be called during decoding
    (in DECODE_STATE_IDLE or DECODE_STATE_READY state) */
	p_H264_Dpb->mDPB.size = actual_dpb_size;
	p_H264_Dpb->max_reference_size = max_reference_size;
}

static void init_picture(struct h264_dpb_stru *p_H264_Dpb,
			 struct Slice *currSlice,
			 struct StorablePicture *dec_picture)
{
	/* struct VideoParameters *p_Vid = &(p_H264_Dpb->mVideo); */
	dpb_print(p_H264_Dpb->decoder_index, PRINT_FLAG_DPB_DETAIL,
		  "%s dec_picture %p\n", __func__, dec_picture);
	dec_picture->top_poc = currSlice->toppoc;
	dec_picture->bottom_poc = currSlice->bottompoc;
	dec_picture->frame_poc = currSlice->framepoc;
	switch (currSlice->structure) {
	case TOP_FIELD: {
		dec_picture->poc = currSlice->toppoc;
		/* p_Vid->number *= 2; */
		break;
	}
	case BOTTOM_FIELD: {
		dec_picture->poc = currSlice->bottompoc;
		/* p_Vid->number = p_Vid->number * 2 + 1; */
		break;
	}
	case FRAME: {
		dec_picture->poc = currSlice->framepoc;
		break;
	}
	default:
		dpb_print(p_H264_Dpb->decoder_index, PRINT_FLAG_DPB_DETAIL,
			  "p_Vid->structure not initialized %d\n", 235);
	}

	/* dec_picture->slice_type = p_Vid->type; */
	dec_picture->used_for_reference = (currSlice->nal_reference_idc != 0);
	dec_picture->idr_flag = currSlice->idr_flag;
	dec_picture->no_output_of_prior_pics_flag =
		currSlice->no_output_of_prior_pics_flag;
	dec_picture->long_term_reference_flag =
		currSlice->long_term_reference_flag;
#if 1
	dec_picture->adaptive_ref_pic_buffering_flag =
		currSlice->adaptive_ref_pic_buffering_flag;
	dec_picture->dec_ref_pic_marking_buffer =
		&currSlice->dec_ref_pic_marking_buffer[0];
#endif
	/* currSlice->dec_ref_pic_marking_buffer   = NULL; */

	/* dec_picture->mb_aff_frame_flag = currSlice->mb_aff_frame_flag; */
	/* dec_picture->PicWidthInMbs     = p_Vid->PicWidthInMbs; */

	/* p_Vid->get_mb_block_pos =
		dec_picture->mb_aff_frame_flag ? get_mb_block_pos_mbaff :
		get_mb_block_pos_normal; */
	/* p_Vid->getNeighbour     =
		dec_picture->mb_aff_frame_flag ? getAffNeighbour :
		getNonAffNeighbour; */

	dec_picture->pic_num   = currSlice->frame_num;
	dec_picture->frame_num = currSlice->frame_num;

	/* dec_picture->recovery_frame =
		(unsigned int) ((int) currSlice->frame_num ==
		p_Vid->recovery_frame_num); */

	dec_picture->coded_frame = (currSlice->structure == FRAME);

	/* dec_picture->chroma_format_idc = active_sps->chroma_format_idc; */

	/* dec_picture->frame_mbs_only_flag =
		active_sps->frame_mbs_only_flag; */
	/* dec_picture->frame_cropping_flag =
		active_sps->frame_cropping_flag; */

	if ((currSlice->picture_structure_mmco & 0x3) == 3) {
		dec_picture->mb_aff_frame_flag = 1;
		dpb_print(p_H264_Dpb->decoder_index, PRINT_FLAG_DPB_DETAIL,
			"%s, picture_structure_mmco is %x, set mb_aff_frame_flag to 1\n",
			__func__,
			currSlice->picture_structure_mmco);
	}

}

static struct StorablePicture *get_new_pic(struct h264_dpb_stru *p_H264_Dpb,
		enum PictureStructure structure, unsigned char is_output)
{
	struct StorablePicture *s = NULL;
	struct StorablePicture *pic;
	struct VideoParameters *p_Vid = &(p_H264_Dpb->mVideo);
	/* recycle un-used pic */
	int ii = 0;

	for (ii = 0; ii < MAX_PIC_BUF_NUM; ii++) {
		pic = &(p_H264_Dpb->m_PIC[ii]);
		if (pic->is_used == 0) {
			pic->is_used = 1;
			s = pic;
			break;
		}
	}

	if (s) {
		s->pic_num   = 0;
		s->frame_num = 0;
		s->long_term_frame_idx = 0;
		s->long_term_pic_num   = 0;
		s->used_for_reference  = 0;
		s->is_long_term        = 0;
		s->non_existing        = 0;
		s->is_output           = 0;
		s->pre_output          = 0;
		s->max_slice_id        = 0;
#if (MVC_EXTENSION_ENABLE)
		s->view_id = -1;
#endif

		s->structure = structure;

#if 0
		s->size_x = size_x;
		s->size_y = size_y;
		s->size_x_cr = size_x_cr;
		s->size_y_cr = size_y_cr;
		s->size_x_m1 = size_x - 1;
		s->size_y_m1 = size_y - 1;
		s->size_x_cr_m1 = size_x_cr - 1;
		s->size_y_cr_m1 = size_y_cr - 1;

		s->top_field    = p_Vid->no_reference_picture;
		s->bottom_field = p_Vid->no_reference_picture;
		s->frame        = p_Vid->no_reference_picture;
#endif
		/* s->dec_ref_pic_marking_buffer = NULL; */

		s->coded_frame  = 0;
		s->mb_aff_frame_flag  = 0;

		s->top_poc = s->bottom_poc = s->poc = 0;
		s->seiHasTone_mapping = 0;

		if (!p_Vid->active_sps->frame_mbs_only_flag &&
			structure != FRAME) {
			int i, j;
			for (j = 0; j < MAX_NUM_SLICES; j++) {
				for (i = 0; i < 2; i++) {
					/* s->listX[j][i] =
					calloc(MAX_LIST_SIZE,
					sizeof (struct StorablePicture *));
					+1 for reordering   ???

					if (NULL==s->listX[j][i])
					    no_mem_exit("alloc_storable_picture:
						s->listX[i]"); */
				}
			}
		}
	}
	dpb_print(p_H264_Dpb->decoder_index, PRINT_FLAG_DPB_DETAIL,
			"%s %p\n", __func__, s);
	return s;
}

static void free_picture(struct h264_dpb_stru *p_H264_Dpb,
			 struct StorablePicture *pic)
{
	dpb_print(p_H264_Dpb->decoder_index, PRINT_FLAG_DPB_DETAIL,
			"%s %p %d\n", __func__, pic, pic->index);
	/* assert(pic->index<MAX_PIC_BUF_NUM); */
	p_H264_Dpb->m_PIC[pic->index].is_used = 0;
}

static void gen_field_ref_ids(struct VideoParameters *p_Vid,
			      struct StorablePicture *p)
{
	int i, j;
	struct h264_dpb_stru *p_H264_Dpb = container_of(p_Vid,
					struct h264_dpb_stru, mVideo);
	/* ! Generate Frame parameters from field information. */
	dpb_print(p_H264_Dpb->decoder_index, PRINT_FLAG_DPB_DETAIL,
					"%s\n", __func__);

	/* copy the list; */
	for (j = 0; j < p_Vid->iSliceNumOfCurrPic; j++) {
		if (p->listX[j][LIST_0]) {
			p->listXsize[j][LIST_0] =
				p_Vid->ppSliceList[j]->listXsize[LIST_0];
			for (i = 0; i < p->listXsize[j][LIST_0]; i++)
				p->listX[j][LIST_0][i] =
				p_Vid->ppSliceList[j]->listX[LIST_0][i];
		}
		if (p->listX[j][LIST_1]) {
			p->listXsize[j][LIST_1] =
				p_Vid->ppSliceList[j]->listXsize[LIST_1];
			for (i = 0; i < p->listXsize[j][LIST_1]; i++)
				p->listX[j][LIST_1][i] =
				p_Vid->ppSliceList[j]->listX[LIST_1][i];
		}
	}
}

static void init_dpb(struct h264_dpb_stru *p_H264_Dpb, int type)
{
	unsigned i;
	struct VideoParameters *p_Vid  = &p_H264_Dpb->mVideo;
	struct DecodedPictureBuffer *p_Dpb = &p_H264_Dpb->mDPB;
	struct SPSParameters *active_sps = &p_H264_Dpb->mSPS;

	p_Vid->active_sps = active_sps;
	dpb_print(p_H264_Dpb->decoder_index, PRINT_FLAG_DPB_DETAIL,
		"%s\n", __func__);

	p_Dpb->p_Vid = p_Vid;
	if (p_Dpb->init_done) {
		/* free_dpb(p_Dpb); */
		if (p_Vid->no_reference_picture) {
			free_picture(p_H264_Dpb, p_Vid->no_reference_picture);
			p_Vid->no_reference_picture = NULL;
		}
		p_Dpb->init_done = 0;
	}

	/* p_Dpb->size = 10; //active_sps->max_dpb_size; //16;
	   getDpbSize(p_Vid, active_sps) +
		p_Vid->p_Inp->dpb_plus[type==2? 1: 0];
	   p_Dpb->size = active_sps->max_dpb_size; //16;
	   getDpbSize(p_Vid, active_sps) +
		p_Vid->p_Inp->dpb_plus[type==2? 1: 0];
	   p_Dpb->size initialzie in vh264.c */
	p_Dpb->num_ref_frames = active_sps->num_ref_frames;
	/* p_Dpb->num_ref_frames initialzie in vh264.c */
	dpb_print(p_H264_Dpb->decoder_index, PRINT_FLAG_DPB_DETAIL,
		  "%s dpb_size is %d (%d) num_ref_frames = %d (%d)\n",
		  __func__, p_Dpb->size, active_sps->max_dpb_size,
		  p_Dpb->num_ref_frames,
		  active_sps->num_ref_frames);

#if 0
	/* ??? */
#if (MVC_EXTENSION_ENABLE)
	if ((unsigned int)active_sps->max_dec_frame_buffering <
	    active_sps->num_ref_frames) {
#else
	if (p_Dpb->size < active_sps->num_ref_frames) {
#endif
		error(
		"DPB size at specified level is smaller than the specified number of reference frames. This is not allowed.\n",
		1000);
	}
#endif

	p_Dpb->used_size = 0;
	p_Dpb->last_picture = NULL;

	p_Dpb->ref_frames_in_buffer = 0;
	p_Dpb->ltref_frames_in_buffer = 0;

#if 0
	p_Dpb->fs = calloc(p_Dpb->size, sizeof(struct FrameStore *));
	if (NULL == p_Dpb->fs)
		no_mem_exit("init_dpb: p_Dpb->fs");

	p_Dpb->fs_ref = calloc(p_Dpb->size, sizeof(struct FrameStore *));
	if (NULL == p_Dpb->fs_ref)
		no_mem_exit("init_dpb: p_Dpb->fs_ref");

	p_Dpb->fs_ltref = calloc(p_Dpb->size, sizeof(struct FrameStore *));
	if (NULL == p_Dpb->fs_ltref)
		no_mem_exit("init_dpb: p_Dpb->fs_ltref");
#endif

#if (MVC_EXTENSION_ENABLE)
	p_Dpb->fs_ilref = calloc(1, sizeof(struct FrameStore *));
	if (NULL == p_Dpb->fs_ilref)
		no_mem_exit("init_dpb: p_Dpb->fs_ilref");
#endif

	for (i = 0; i < p_Dpb->size; i++) {
		p_Dpb->fs[i] = &(p_H264_Dpb->mFrameStore[i]);
		/* alloc_frame_store(); */
		p_Dpb->fs[i]->index       = i;
		p_Dpb->fs_ref[i]   = NULL;
		p_Dpb->fs_ltref[i] = NULL;
		p_Dpb->fs[i]->layer_id = 0; /* MVC_INIT_VIEW_ID; */
#if (MVC_EXTENSION_ENABLE)
		p_Dpb->fs[i]->view_id = MVC_INIT_VIEW_ID;
		p_Dpb->fs[i]->inter_view_flag[0] =
			p_Dpb->fs[i]->inter_view_flag[1] = 0;
		p_Dpb->fs[i]->anchor_pic_flag[0] =
			p_Dpb->fs[i]->anchor_pic_flag[1] = 0;
#endif
	}
#if (MVC_EXTENSION_ENABLE)
	if (type == 2) {
		p_Dpb->fs_ilref[0] = alloc_frame_store();
		/* These may need some cleanups */
		p_Dpb->fs_ilref[0]->view_id = MVC_INIT_VIEW_ID;
		p_Dpb->fs_ilref[0]->inter_view_flag[0] =
			p_Dpb->fs_ilref[0]->inter_view_flag[1] = 0;
		p_Dpb->fs_ilref[0]->anchor_pic_flag[0] =
			p_Dpb->fs_ilref[0]->anchor_pic_flag[1] = 0;
		/* given that this is in a different buffer,
			do we even need proc_flag anymore? */
	} else
		p_Dpb->fs_ilref[0] = NULL;
#endif

	/*
	for (i = 0; i < 6; i++)
	{
	currSlice->listX[i] =
		calloc(MAX_LIST_SIZE, sizeof (struct StorablePicture *));
		+1 for reordering
	if (NULL==currSlice->listX[i])
	no_mem_exit("init_dpb: currSlice->listX[i]");
	}
	*/
	/* allocate a dummy storable picture */
	if (!p_Vid->no_reference_picture) {
		p_Vid->no_reference_picture = get_new_pic(p_H264_Dpb,
					      FRAME,
		/*p_Vid->width, p_Vid->height,
		p_Vid->width_cr, p_Vid->height_cr,*/ 1);
		p_Vid->no_reference_picture->top_field =
			p_Vid->no_reference_picture;
		p_Vid->no_reference_picture->bottom_field =
			p_Vid->no_reference_picture;
		p_Vid->no_reference_picture->frame =
			p_Vid->no_reference_picture;
	}
	p_Dpb->last_output_poc = INT_MIN;

#if (MVC_EXTENSION_ENABLE)
	p_Dpb->last_output_view_id = -1;
#endif

	p_Vid->last_has_mmco_5 = 0;

	init_colocate_buf(p_H264_Dpb, p_H264_Dpb->max_reference_size);

	p_Dpb->init_done = 1;

#if 0
/* ??? */
	/* picture error concealment */
	if (p_Vid->conceal_mode != 0 && !p_Vid->last_out_fs)
		p_Vid->last_out_fs = alloc_frame_store();
#endif
}

static void dpb_split_field(struct h264_dpb_stru *p_H264_Dpb,
			    struct FrameStore *fs)
{
	struct StorablePicture *fs_top = NULL, *fs_btm = NULL;
	struct StorablePicture *frame = fs->frame;

	dpb_print(p_H264_Dpb->decoder_index, PRINT_FLAG_DPB_DETAIL,
		"%s %p %p\n", __func__, fs, frame);

	fs->poc = frame->poc;

	if (!frame->frame_mbs_only_flag) {
		fs_top = fs->top_field = get_new_pic(p_H264_Dpb,
			TOP_FIELD,
			/* frame->size_x, frame->size_y,
			frame->size_x_cr, frame->size_y_cr,*/ 1);
		fs_btm = fs->bottom_field = get_new_pic(p_H264_Dpb,
			BOTTOM_FIELD,
			/*frame->size_x, frame->size_y,
			frame->size_x_cr, frame->size_y_cr,*/ 1);
		if (fs_top == NULL || fs_btm == NULL)
			return;
#if 1
/* rain */
		fs_top->buf_spec_num = frame->buf_spec_num;
		fs_btm->buf_spec_num = frame->buf_spec_num;

		fs_top->colocated_buf_index = frame->colocated_buf_index;
		fs_btm->colocated_buf_index = frame->colocated_buf_index;
#endif
		fs_top->poc = frame->top_poc;
		fs_btm->poc = frame->bottom_poc;

#if (MVC_EXTENSION_ENABLE)
		fs_top->view_id = frame->view_id;
		fs_btm->view_id = frame->view_id;
#endif

		fs_top->frame_poc =  frame->frame_poc;

		fs_top->bottom_poc = fs_btm->bottom_poc =  frame->bottom_poc;
		fs_top->top_poc    = fs_btm->top_poc    =  frame->top_poc;
		fs_btm->frame_poc  = frame->frame_poc;

		fs_top->used_for_reference = fs_btm->used_for_reference
					     = frame->used_for_reference;
		fs_top->is_long_term = fs_btm->is_long_term
				       = frame->is_long_term;
		fs->long_term_frame_idx = fs_top->long_term_frame_idx
					  = fs_btm->long_term_frame_idx
					    = frame->long_term_frame_idx;

		fs_top->coded_frame = fs_btm->coded_frame = 1;
		fs_top->mb_aff_frame_flag = fs_btm->mb_aff_frame_flag
					    = frame->mb_aff_frame_flag;

		frame->top_field    = fs_top;
		frame->bottom_field = fs_btm;
		frame->frame         = frame;
		fs_top->bottom_field = fs_btm;
		fs_top->frame        = frame;
		fs_top->top_field = fs_top;
		fs_btm->top_field = fs_top;
		fs_btm->frame     = frame;
		fs_btm->bottom_field = fs_btm;

#if (MVC_EXTENSION_ENABLE)
		fs_top->view_id = fs_btm->view_id = fs->view_id;
		fs_top->inter_view_flag = fs->inter_view_flag[0];
		fs_btm->inter_view_flag = fs->inter_view_flag[1];
#endif

		fs_top->chroma_format_idc = fs_btm->chroma_format_idc =
						    frame->chroma_format_idc;
		fs_top->iCodingType = fs_btm->iCodingType = frame->iCodingType;
	} else {
		fs->top_field       = NULL;
		fs->bottom_field    = NULL;
		frame->top_field    = NULL;
		frame->bottom_field = NULL;
		frame->frame = frame;
	}

}


static void dpb_combine_field(struct h264_dpb_stru *p_H264_Dpb,
			      struct FrameStore *fs)
{

	dpb_print(p_H264_Dpb->decoder_index, PRINT_FLAG_DPB_DETAIL,
			"%s\n", __func__);

	if (!fs->frame) {
		fs->frame = get_new_pic(p_H264_Dpb,
			FRAME,
			/* fs->top_field->size_x, fs->top_field->size_y*2,
			fs->top_field->size_x_cr, fs->top_field->size_y_cr*2,
			*/ 1);
	}
	if (!fs->frame)
		return;
#if 1
/* rain */
	fs->frame->buf_spec_num = fs->top_field->buf_spec_num;
	fs->frame->colocated_buf_index = fs->top_field->colocated_buf_index;
#endif


	fs->poc = fs->frame->poc = fs->frame->frame_poc = imin(
			fs->top_field->poc, fs->bottom_field->poc);

	fs->bottom_field->frame_poc = fs->top_field->frame_poc = fs->frame->poc;

	fs->bottom_field->top_poc = fs->frame->top_poc = fs->top_field->poc;
	fs->top_field->bottom_poc = fs->frame->bottom_poc =
			fs->bottom_field->poc;

	fs->frame->used_for_reference = (fs->top_field->used_for_reference &&
					 fs->bottom_field->used_for_reference);
	fs->frame->is_long_term = (fs->top_field->is_long_term &&
				   fs->bottom_field->is_long_term);

	if (fs->frame->is_long_term)
		fs->frame->long_term_frame_idx = fs->long_term_frame_idx;

	fs->frame->top_field    = fs->top_field;
	fs->frame->bottom_field = fs->bottom_field;
	fs->frame->frame = fs->frame;

	fs->frame->coded_frame = 0;

	fs->frame->chroma_format_idc = fs->top_field->chroma_format_idc;
	fs->frame->frame_cropping_flag = fs->top_field->frame_cropping_flag;
	if (fs->frame->frame_cropping_flag) {
		fs->frame->frame_crop_top_offset =
			fs->top_field->frame_crop_top_offset;
		fs->frame->frame_crop_bottom_offset =
			fs->top_field->frame_crop_bottom_offset;
		fs->frame->frame_crop_left_offset =
			fs->top_field->frame_crop_left_offset;
		fs->frame->frame_crop_right_offset =
			fs->top_field->frame_crop_right_offset;
	}

	fs->top_field->frame = fs->bottom_field->frame = fs->frame;
	fs->top_field->top_field = fs->top_field;
	fs->top_field->bottom_field = fs->bottom_field;
	fs->bottom_field->top_field = fs->top_field;
	fs->bottom_field->bottom_field = fs->bottom_field;

	/**/
#if (MVC_EXTENSION_ENABLE)
	fs->frame->view_id = fs->view_id;
#endif
	fs->frame->iCodingType = fs->top_field->iCodingType;
	/* FIELD_CODING ;*/
}

static void calculate_frame_no(struct VideoParameters *p_Vid,
			       struct StorablePicture *p)
{
#if 0
/* ??? */
	InputParameters *p_Inp = p_Vid->p_Inp;
	/* calculate frame number */
	int psnrPOC = p_Vid->active_sps->mb_adaptive_frame_field_flag ?
		p->poc / (p_Inp->poc_scale) : p->poc / (p_Inp->poc_scale);

	if (psnrPOC == 0) { /* && p_Vid->psnr_number) */
		p_Vid->idr_psnr_number =
		p_Vid->g_nFrame * p_Vid->ref_poc_gap / (p_Inp->poc_scale);
	}
	p_Vid->psnr_number = imax(p_Vid->psnr_number,
		p_Vid->idr_psnr_number + psnrPOC);

	p_Vid->frame_no = p_Vid->idr_psnr_number + psnrPOC;
#endif
}

static void insert_picture_in_dpb(struct h264_dpb_stru *p_H264_Dpb,
				  struct FrameStore *fs,
				  struct StorablePicture *p,
				  unsigned char data_flag)
{
	struct VideoParameters *p_Vid = &p_H264_Dpb->mVideo;
	/* InputParameters *p_Inp = p_Vid->p_Inp;
	   dpb_print(p_H264_Dpb->decoder_index, PRINT_FLAG_DPB_DETAIL,
		"insert (%s) pic with frame_num #%d, poc %d\n",
		(p->structure == FRAME)?"FRAME":
		(p->structure == TOP_FIELD)?"TOP_FIELD":
		"BOTTOM_FIELD", p->pic_num, p->poc);
	   assert (p!=NULL);
	   assert (fs!=NULL);*/
	dpb_print(p_H264_Dpb->decoder_index, PRINT_FLAG_DPB_DETAIL,
		"%s %p %p\n", __func__, fs, p);
#if 1
/* rain */
/* p->buf_spec_num = fs->index; */
	fs->buf_spec_num = p->buf_spec_num;
	fs->colocated_buf_index = p->colocated_buf_index;
#endif
	switch (p->structure) {
	case FRAME:
		fs->frame = p;
		fs->is_used = 3;
		if (p->used_for_reference) {
			fs->is_reference = 3;
			fs->is_orig_reference = 3;
			if (p->is_long_term) {
				fs->is_long_term = 3;
				fs->long_term_frame_idx =
					p->long_term_frame_idx;
			}
		}
		fs->layer_id = p->layer_id;
#if (MVC_EXTENSION_ENABLE)
		fs->view_id = p->view_id;
		fs->inter_view_flag[0] = fs->inter_view_flag[1] =
			p->inter_view_flag;
		fs->anchor_pic_flag[0] = fs->anchor_pic_flag[1] =
			p->anchor_pic_flag;
#endif
		/* generate field views */
		/* return; */
		dpb_split_field(p_H264_Dpb, fs);
		/* return; */
		break;
	case TOP_FIELD:
		fs->top_field = p;
		fs->is_used |= 1;
		fs->layer_id = p->layer_id;
#if (MVC_EXTENSION_ENABLE)
		fs->view_id = p->view_id;
		fs->inter_view_flag[0] = p->inter_view_flag;
		fs->anchor_pic_flag[0] = p->anchor_pic_flag;
#endif
		if (p->used_for_reference) {
			fs->is_reference |= 1;
			fs->is_orig_reference |= 1;
			if (p->is_long_term) {
				fs->is_long_term |= 1;
				fs->long_term_frame_idx =
					p->long_term_frame_idx;
			}
		}
		if (fs->is_used == 3) {
			/* generate frame view */
			dpb_combine_field(p_H264_Dpb, fs);
		} else {
			fs->poc = p->poc;
		}
		gen_field_ref_ids(p_Vid, p);
		break;
	case BOTTOM_FIELD:
		fs->bottom_field = p;
		fs->is_used |= 2;
		fs->layer_id = p->layer_id;
#if (MVC_EXTENSION_ENABLE)
		fs->view_id = p->view_id;
		fs->inter_view_flag[1] = p->inter_view_flag;
		fs->anchor_pic_flag[1] = p->anchor_pic_flag;
#endif
		if (p->used_for_reference) {
			fs->is_reference |= 2;
			fs->is_orig_reference |= 2;
			if (p->is_long_term) {
				fs->is_long_term |= 2;
				fs->long_term_frame_idx =
					p->long_term_frame_idx;
			}
		}
		if (fs->is_used == 3) {
			/* generate frame view */
			dpb_combine_field(p_H264_Dpb, fs);
		} else {
			fs->poc = p->poc;
		}
		gen_field_ref_ids(p_Vid, p);
		break;
	}
	fs->frame_num = p->pic_num;
	fs->recovery_frame = p->recovery_frame;

	fs->is_output = p->is_output;
	fs->pre_output = p->pre_output;
	fs->data_flag = data_flag;

	if (fs->is_used == 3) {
		calculate_frame_no(p_Vid, p);
#if 0
/* ??? */
	if (-1 != p_Vid->p_ref && !p_Inp->silent)
		find_snr(p_Vid, fs->frame, &p_Vid->p_ref);
#endif
	}

	fs->pts = p->pts;
	fs->pts64 = p->pts64;
}

void reset_frame_store(struct h264_dpb_stru *p_H264_Dpb,
			      struct FrameStore *f)
{
	dpb_print(p_H264_Dpb->decoder_index, PRINT_FLAG_DPB_DETAIL,
		"%s\n", __func__);

	if (f) {
		if (f->frame) {
			free_picture(p_H264_Dpb, f->frame);
			f->frame = NULL;
		}
		if (f->top_field) {
			free_picture(p_H264_Dpb, f->top_field);
			f->top_field = NULL;
		}
		if (f->bottom_field) {
			free_picture(p_H264_Dpb, f->bottom_field);
			f->bottom_field = NULL;
		}

		/**/
		f->is_used      = 0;
		f->is_reference = 0;
		f->is_long_term = 0;
		f->is_orig_reference = 0;

		f->is_output = 0;
		f->pre_output = 0;

		f->frame        = NULL;
		f->top_field    = NULL;
		f->bottom_field = NULL;

		/* free(f); */
	}
}

static void unmark_for_reference(struct DecodedPictureBuffer *p_Dpb,
				 struct FrameStore *fs)
{
	struct h264_dpb_stru *p_H264_Dpb = container_of(p_Dpb,
				struct h264_dpb_stru, mDPB);
	dpb_print(p_H264_Dpb->decoder_index, PRINT_FLAG_DPB_DETAIL,
		"%s %p %p %p %p\n", __func__,
		fs, fs->frame, fs->top_field, fs->bottom_field);
	/* return; */
	if (fs->is_used & 1) {
		if (fs->top_field)
			fs->top_field->used_for_reference = 0;
	}
	if (fs->is_used & 2) {
		if (fs->bottom_field)
			fs->bottom_field->used_for_reference = 0;
	}
	if (fs->is_used == 3) {
		if (fs->top_field && fs->bottom_field) {
			fs->top_field->used_for_reference = 0;
			fs->bottom_field->used_for_reference = 0;
		}
		fs->frame->used_for_reference = 0;
	}

	fs->is_reference = 0;

}

int get_long_term_flag_by_buf_spec_num(struct h264_dpb_stru *p_H264_Dpb,
	int buf_spec_num)
{
	struct DecodedPictureBuffer *p_Dpb = &p_H264_Dpb->mDPB;
	unsigned i;
	for (i = 0; i < p_Dpb->used_size; i++) {
		if (p_Dpb->fs[i]->buf_spec_num == buf_spec_num)
			return p_Dpb->fs[i]->is_long_term;
	}
	return -1;
}

static void update_pic_num(struct Slice *currSlice)
{
	unsigned int i;
	struct VideoParameters *p_Vid = currSlice->p_Vid;
	struct DecodedPictureBuffer *p_Dpb = currSlice->p_Dpb;
	struct SPSParameters *active_sps = p_Vid->active_sps;

	int add_top = 0, add_bottom = 0;
	int max_frame_num = 1 << (active_sps->log2_max_frame_num_minus4 + 4);

	if (currSlice->structure == FRAME) {
		for (i = 0; i < p_Dpb->ref_frames_in_buffer; i++) {
			if (p_Dpb->fs_ref[i]->is_used == 3) {
				if ((p_Dpb->fs_ref[i]->frame->
					used_for_reference) &&
				    (!p_Dpb->fs_ref[i]->frame->
					is_long_term)) {
					if (p_Dpb->fs_ref[i]->frame_num >
						currSlice->frame_num) {
						p_Dpb->fs_ref[i]->
						frame_num_wrap =
						p_Dpb->fs_ref[i]->frame_num
							- max_frame_num;
					} else {
						p_Dpb->fs_ref[i]->
						frame_num_wrap =
						p_Dpb->fs_ref[i]->frame_num;
					}
					p_Dpb->fs_ref[i]->frame->pic_num =
					p_Dpb->fs_ref[i]->frame_num_wrap;
				}
			}
		}
		/* update long_term_pic_num */
		for (i = 0; i < p_Dpb->ltref_frames_in_buffer; i++) {
			if (p_Dpb->fs_ltref[i]->is_used == 3) {
				if (p_Dpb->fs_ltref[i]->frame->is_long_term) {
					p_Dpb->fs_ltref[i]->frame->
						long_term_pic_num =
						p_Dpb->fs_ltref[i]->frame->
							long_term_frame_idx;
				}
			}
		}
	} else {
		if (currSlice->structure == TOP_FIELD) {
			add_top    = 1;
			add_bottom = 0;
		} else {
			add_top    = 0;
			add_bottom = 1;
		}

		for (i = 0; i < p_Dpb->ref_frames_in_buffer; i++) {
			if (p_Dpb->fs_ref[i]->is_reference) {
				if (p_Dpb->fs_ref[i]->frame_num > currSlice->
					frame_num) {
					p_Dpb->fs_ref[i]->frame_num_wrap =
					p_Dpb->fs_ref[i]->frame_num -
					max_frame_num;
				} else {
					p_Dpb->fs_ref[i]->frame_num_wrap =
					p_Dpb->fs_ref[i]->frame_num;
				}
				if (p_Dpb->fs_ref[i]->is_reference & 1) {
					p_Dpb->fs_ref[i]->top_field->
					pic_num = (2 * p_Dpb->fs_ref[i]->
						frame_num_wrap) + add_top;
				}
				if (p_Dpb->fs_ref[i]->is_reference & 2) {
					p_Dpb->fs_ref[i]->bottom_field->
					pic_num = (2 * p_Dpb->fs_ref[i]->
						frame_num_wrap) + add_bottom;
				}
			}
		}
		/* update long_term_pic_num */
		for (i = 0; i < p_Dpb->ltref_frames_in_buffer; i++) {
			if (p_Dpb->fs_ltref[i]->is_long_term & 1) {
				p_Dpb->fs_ltref[i]->top_field->
					long_term_pic_num = 2 *
					p_Dpb->fs_ltref[i]->top_field->
					long_term_frame_idx + add_top;
			}
			if (p_Dpb->fs_ltref[i]->is_long_term & 2) {
				p_Dpb->fs_ltref[i]->bottom_field->
					long_term_pic_num = 2 *
					p_Dpb->fs_ltref[i]->bottom_field->
					long_term_frame_idx + add_bottom;
			}
		}
	}
}

static void remove_frame_from_dpb(struct h264_dpb_stru *p_H264_Dpb, int pos)
{
	struct DecodedPictureBuffer *p_Dpb = &p_H264_Dpb->mDPB;
	struct FrameStore *fs = p_Dpb->fs[pos];
	struct FrameStore *tmp;
	unsigned i;

	dpb_print(p_H264_Dpb->decoder_index, PRINT_FLAG_DPB_DETAIL,
		"%s pos %d %p\n", __func__, pos, fs);

	/* dpb_print(p_H264_Dpb->decoder_index, PRINT_FLAG_DPB_DETAIL,
		"remove frame with frame_num #%d\n", fs->frame_num); */
	switch (fs->is_used) {
	case 3:
		free_picture(p_H264_Dpb, fs->frame);
		free_picture(p_H264_Dpb, fs->top_field);
		free_picture(p_H264_Dpb, fs->bottom_field);
		fs->frame = NULL;
		fs->top_field = NULL;
		fs->bottom_field = NULL;
		break;
	case 2:
		free_picture(p_H264_Dpb, fs->bottom_field);
		fs->bottom_field = NULL;
		break;
	case 1:
		free_picture(p_H264_Dpb, fs->top_field);
		fs->top_field = NULL;
		break;
	case 0:
		break;
	default:
		dpb_print(p_H264_Dpb->decoder_index, PRINT_FLAG_DPB_DETAIL,
			  "invalid frame store type %x", 500);
	}
	fs->is_used = 0;
	fs->is_long_term = 0;
	fs->is_reference = 0;
	fs->is_orig_reference = 0;

	/* move empty framestore to end of buffer */
	tmp = p_Dpb->fs[pos];

	for (i = pos; i < p_Dpb->used_size - 1; i++)
		p_Dpb->fs[i] = p_Dpb->fs[i + 1];
	p_Dpb->fs[p_Dpb->used_size - 1] = tmp;
	p_Dpb->used_size--;
}

static int is_used_for_reference(struct FrameStore *fs)
{
	if (fs->is_reference)
		return 1;

	if (fs->is_used == 3) { /* frame */
		if (fs->frame->used_for_reference)
			return 1;
	}

	if (fs->is_used & 1) { /* top field */
		if (fs->top_field) {
			if (fs->top_field->used_for_reference)
				return 1;
		}
	}

	if (fs->is_used & 2) { /* bottom field */
		if (fs->bottom_field) {
			if (fs->bottom_field->used_for_reference)
				return 1;
		}
	}
	return 0;
}

static int remove_unused_frame_from_dpb(struct h264_dpb_stru *p_H264_Dpb)
{
	unsigned i;
	struct DecodedPictureBuffer *p_Dpb = &p_H264_Dpb->mDPB;
	/* check for frames that were already output and no longer
		used for reference */
	for (i = 0; i < p_Dpb->used_size; i++) {
		if ((!is_used_for_reference(p_Dpb->fs[i])) &&
		    (p_Dpb->fs[i]->colocated_buf_index >= 0)) {
			dpb_print(p_H264_Dpb->decoder_index,
			PRINT_FLAG_DPB_DETAIL,
			"release_colocate_buf[%d] for fs[%d]\n",
			p_Dpb->fs[i]->colocated_buf_index, i);

			release_colocate_buf(p_H264_Dpb,
				p_Dpb->fs[i]->colocated_buf_index); /* rain */
			p_Dpb->fs[i]->colocated_buf_index = -1;
		}
	}

	for (i = 0; i < p_Dpb->used_size; i++) {
		if (p_Dpb->fs[i]->is_output &&
			(!is_used_for_reference(p_Dpb->fs[i]))) {
			release_buf_spec_num(p_H264_Dpb->vdec,
				p_Dpb->fs[i]->buf_spec_num);
			p_Dpb->fs[i]->buf_spec_num = -1;
			remove_frame_from_dpb(p_H264_Dpb, i);

			dpb_print(p_H264_Dpb->decoder_index,
				PRINT_FLAG_DPB_DETAIL, "%s[%d]\n",
				__func__, i);

			return 1;
		}
	}
	return 0;
}

void bufmgr_h264_remove_unused_frame(struct h264_dpb_stru *p_H264_Dpb)
{
	struct DecodedPictureBuffer *p_Dpb = &p_H264_Dpb->mDPB;
	int ret = 0;
	unsigned char print_flag = 0;
	do {
		ret = remove_unused_frame_from_dpb(p_H264_Dpb);
		if (ret != 0)
			print_flag = 1;
	} while (ret != 0);
	if (print_flag) {
		dpb_print(p_H264_Dpb->decoder_index,
			PRINT_FLAG_DPB_DETAIL, "%s\r\n", __func__);
		dump_dpb(p_Dpb);
	}
}

#ifdef OUTPUT_BUFFER_IN_C
int is_there_unused_frame_from_dpb(struct DecodedPictureBuffer *p_Dpb)
{
	unsigned i;

	/* check for frames that were already output and no longer
	 * used for reference */
	for (i = 0; i < p_Dpb->used_size; i++) {
		if (p_Dpb->fs[i]->is_output &&
			(!is_used_for_reference(p_Dpb->fs[i]))) {
			return 1;
		}
	}
	return 0;
}
#endif

static void get_smallest_poc(struct DecodedPictureBuffer *p_Dpb, int *poc,
			     int *pos)
{
	unsigned  i;
	struct h264_dpb_stru *p_H264_Dpb = container_of(p_Dpb,
				struct h264_dpb_stru, mDPB);
	dpb_print(p_H264_Dpb->decoder_index,
		PRINT_FLAG_DPB_DETAIL, "%s\n", __func__);
	if (p_Dpb->used_size < 1) {
		dpb_print(p_H264_Dpb->decoder_index, PRINT_FLAG_DPB_DETAIL,
			  "Cannot determine smallest POC, DPB empty. %d\n",
			 150);
	}

	*pos = -1;
	*poc = INT_MAX;
	for (i = 0; i < p_Dpb->used_size; i++) {
#ifdef OUTPUT_BUFFER_IN_C
		/* rain */
		if ((*poc > p_Dpb->fs[i]->poc) &&
			(!p_Dpb->fs[i]->is_output) &&
			(!p_Dpb->fs[i]->pre_output)) {
#else
		if ((*poc > p_Dpb->fs[i]->poc) && (!p_Dpb->fs[i]->is_output)) {
#endif
			*poc = p_Dpb->fs[i]->poc;
			*pos = i;
		}
	}
}

int output_frames(struct h264_dpb_stru *p_H264_Dpb, unsigned char flush_flag)
{
	int poc, pos;
	struct DecodedPictureBuffer *p_Dpb = &p_H264_Dpb->mDPB;
	int i;
	int none_displayed_num = 0;
	if (!flush_flag) {
		for (i = 0; i < p_Dpb->used_size; i++) {
			if ((!p_Dpb->fs[i]->is_output) &&
				(!p_Dpb->fs[i]->pre_output))
				none_displayed_num++;
		}
		if (none_displayed_num < p_H264_Dpb->reorder_pic_num)
			return 0;
	}

	get_smallest_poc(p_Dpb, &poc, &pos);

	if (pos == -1)
		return 0;
#if 0
	if (is_used_for_reference(p_Dpb->fs[pos]))
		return 0;
#endif
	p_Dpb->last_output_poc = poc;

	if (prepare_display_buf(p_H264_Dpb->vdec, p_Dpb->fs[pos]) >= 0)
		p_Dpb->fs[pos]->pre_output = 1;

	dpb_print(p_H264_Dpb->decoder_index, PRINT_FLAG_DPB_DETAIL,
		"%s[%d] poc %d\n", __func__, pos, poc);

	return 1;

}


void flush_dpb(struct h264_dpb_stru *p_H264_Dpb)
{
	/* struct VideoParameters *p_Vid = p_Dpb->p_Vid; */
	struct DecodedPictureBuffer *p_Dpb = &p_H264_Dpb->mDPB;
	unsigned  i;

	dpb_print(p_H264_Dpb->decoder_index, PRINT_FLAG_DPB_DETAIL,
		"%s\n", __func__);

	/* diagnostics */
	/* dpb_print(p_H264_Dpb->decoder_index,
	PRINT_FLAG_DPB_DETAIL,
	"Flush remaining frames from the dpb."
	"p_Dpb->size = %d, p_Dpb->used_size = %d\n",
	p_Dpb->size, p_Dpb->used_size);
	*/

	if (!p_Dpb->init_done)
		return;
/*  if(p_Vid->conceal_mode == 0) */
#if 0
/* ??? */
	if (p_Vid->conceal_mode != 0)
		conceal_non_ref_pics(p_Dpb, 0);
#endif
	/* mark all frames unused */
	for (i = 0; i < p_Dpb->used_size; i++) {
#if MVC_EXTENSION_ENABLE
		assert(p_Dpb->fs[i]->view_id == p_Dpb->layer_id);
#endif
		unmark_for_reference(p_Dpb, p_Dpb->fs[i]);

	}

	while (remove_unused_frame_from_dpb(p_H264_Dpb))
		;

	/* output frames in POC order */
	while (output_frames(p_H264_Dpb, 1))
		;


	p_Dpb->last_output_poc = INT_MIN;
}

static int is_short_term_reference(struct DecodedPictureBuffer *p_Dpb,
				   struct FrameStore *fs)
{
	struct h264_dpb_stru *p_H264_Dpb = container_of(p_Dpb,
			struct h264_dpb_stru, mDPB);
	if (fs->is_used == 3) { /* frame */
		if ((fs->frame->used_for_reference) &&
			(!fs->frame->is_long_term)) {
			dpb_print(p_H264_Dpb->decoder_index,
				PRINT_FLAG_DPB_DETAIL, "[[%s 1]]",
				__func__);
			return 1;
		}
	}

	if (fs->is_used & 1) { /* top field */
		if (fs->top_field) {
			if ((fs->top_field->used_for_reference) &&
				(!fs->top_field->is_long_term)) {
				dpb_print(p_H264_Dpb->decoder_index,
				PRINT_FLAG_DPB_DETAIL, "[[%s 2]]",
				__func__);
				return 1;
			}
		}
	}

	if (fs->is_used & 2) { /* bottom field */
		if (fs->bottom_field) {
			if ((fs->bottom_field->used_for_reference) &&
			    (!fs->bottom_field->is_long_term)) {
				dpb_print(p_H264_Dpb->decoder_index,
				PRINT_FLAG_DPB_DETAIL, "[[%s 3]]",
				__func__);
				return 1;
			}
		}
	}
	return 0;
}

static int is_long_term_reference(struct FrameStore *fs)
{

	if (fs->is_used == 3) { /* frame */
		if ((fs->frame->used_for_reference) &&
			(fs->frame->is_long_term)) {
			return 1;
		}
	}

	if (fs->is_used & 1) { /* top field */
		if (fs->top_field) {
			if ((fs->top_field->used_for_reference) &&
				(fs->top_field->is_long_term)) {
				return 1;
			}
		}
	}

	if (fs->is_used & 2) { /* bottom field */
		if (fs->bottom_field) {
			if ((fs->bottom_field->used_for_reference) &&
			    (fs->bottom_field->is_long_term)) {
				return 1;
			}
		}
	}
	return 0;
}

static void update_ref_list(struct DecodedPictureBuffer *p_Dpb)
{
	unsigned i, j;

	struct h264_dpb_stru *p_H264_Dpb = container_of(p_Dpb,
		struct h264_dpb_stru, mDPB);
	dpb_print(p_H264_Dpb->decoder_index, PRINT_FLAG_DPB_DETAIL,
		"%s (%d, %d)\n", __func__, p_Dpb->size, p_Dpb->used_size);
	for (i = 0, j = 0; i < p_Dpb->used_size; i++) {
#if 1
		dpb_print(p_H264_Dpb->decoder_index, PRINT_FLAG_DPB_DETAIL,
			  "fs[%d]: fs %p frame %p is_reference %d %d %d\n",
			  i, p_Dpb->fs[i], p_Dpb->fs[i]->frame,
			  p_Dpb->fs[i]->frame != NULL ?
			  p_Dpb->fs[i]->frame->used_for_reference : 0,
			  p_Dpb->fs[i]->top_field != NULL ?
			  p_Dpb->fs[i]->top_field->used_for_reference :
			  0,
			  p_Dpb->fs[i]->bottom_field != NULL ?
			  p_Dpb->fs[i]->bottom_field->used_for_reference : 0);
#endif
		if (is_short_term_reference(p_Dpb, p_Dpb->fs[i])) {
			dpb_print(p_H264_Dpb->decoder_index,
				PRINT_FLAG_DPB_DETAIL,
			"fs_ref[%d]=fs[%d]: fs %p\n", j, i, p_Dpb->fs[i]);
			p_Dpb->fs_ref[j++] = p_Dpb->fs[i];
		}
	}

	p_Dpb->ref_frames_in_buffer = j;
	dpb_print(p_H264_Dpb->decoder_index, PRINT_FLAG_DPB_DETAIL,
		"%s dpb size is %d, %d\n", __func__, p_Dpb->size, j);
	while (j < p_Dpb->size) {
		/* dpb_print(p_H264_Dpb->decoder_index, PRINT_FLAG_DPB_DETAIL,
		"fs_ref[%d]=null\n", j); */
		p_Dpb->fs_ref[j++] = NULL;
	}
#ifdef ERROR_CHECK
	for (i = 0; i < DPB_SIZE_MAX; i++) {
		if (p_Dpb->fs_ref[i] == NULL)
			p_Dpb->fs_ref[i] = &dummy_fs;
	}
#endif
}

static void update_ltref_list(struct DecodedPictureBuffer *p_Dpb)
{
	unsigned i, j;
	struct h264_dpb_stru *p_H264_Dpb = container_of(p_Dpb,
		struct h264_dpb_stru, mDPB);

	dpb_print(p_H264_Dpb->decoder_index, PRINT_FLAG_DPB_DETAIL,
		"%s\n", __func__);
	for (i = 0, j = 0; i < p_Dpb->used_size; i++) {
		if (is_long_term_reference(p_Dpb->fs[i]))
			p_Dpb->fs_ltref[j++] = p_Dpb->fs[i];
	}

	p_Dpb->ltref_frames_in_buffer = j;

	while (j < p_Dpb->size)
		p_Dpb->fs_ltref[j++] = NULL;
#ifdef ERROR_CHECK
	for (i = 0; i < DPB_SIZE_MAX; i++) {
		if (p_Dpb->fs_ltref[i] == NULL)
			p_Dpb->fs_ltref[i] = &dummy_fs;
	}
#endif
}

static void idr_memory_management(struct h264_dpb_stru *p_H264_Dpb,
				  struct StorablePicture *p)
{
	struct DecodedPictureBuffer *p_Dpb = &p_H264_Dpb->mDPB;

	dpb_print(p_H264_Dpb->decoder_index, PRINT_FLAG_DPB_DETAIL,
	"%s %d %d\n", __func__, p_Dpb->ref_frames_in_buffer,
	p_Dpb->ltref_frames_in_buffer);

	if (p->no_output_of_prior_pics_flag) {
#if 0
		/*???*/
		/* free all stored pictures */
		int i;
		for (i = 0; i < p_Dpb->used_size; i++) {
			/* reset all reference settings
			 * free_frame_store(p_Dpb->fs[i]);
			 * p_Dpb->fs[i] = alloc_frame_store();
			 */
			reset_frame_store(p_H264_Dpb, p_Dpb->fs[i]); /* ??? */
		}
		for (i = 0; i < p_Dpb->ref_frames_in_buffer; i++)
			p_Dpb->fs_ref[i] = NULL;
		for (i = 0; i < p_Dpb->ltref_frames_in_buffer; i++)
			p_Dpb->fs_ltref[i] = NULL;
		p_Dpb->used_size = 0;
#endif
	} else {
		flush_dpb(p_H264_Dpb);
	}
	p_Dpb->last_picture = NULL;

	update_ref_list(p_Dpb);
	update_ltref_list(p_Dpb);
	p_Dpb->last_output_poc = INT_MIN;

	if (p->long_term_reference_flag) {
		p_Dpb->max_long_term_pic_idx = 0;
		p->is_long_term           = 1;
		p->long_term_frame_idx    = 0;
	} else {
		p_Dpb->max_long_term_pic_idx = -1;
		p->is_long_term           = 0;
	}

#if (MVC_EXTENSION_ENABLE)
	p_Dpb->last_output_view_id = -1;
#endif

}

static void sliding_window_memory_management(
		struct DecodedPictureBuffer *p_Dpb,
		struct StorablePicture *p)
{
	unsigned  i;
	struct h264_dpb_stru *p_H264_Dpb = container_of(p_Dpb,
		struct h264_dpb_stru, mDPB);

	/* assert (!p->idr_flag); */
	dpb_print(p_H264_Dpb->decoder_index, PRINT_FLAG_DPB_DETAIL,
		"%s\n", __func__);

	/* if this is a reference pic with sliding window,
	   unmark first ref frame */
	if (p_Dpb->ref_frames_in_buffer == imax(
		1, p_Dpb->num_ref_frames) - p_Dpb->ltref_frames_in_buffer) {
		for (i = 0; i < p_Dpb->used_size; i++) {
			if (p_Dpb->fs[i]->is_reference &&
				(!(p_Dpb->fs[i]->is_long_term))) {
				dpb_print(p_H264_Dpb->decoder_index,
				PRINT_FLAG_DPB_DETAIL, "unmark %d\n", i);
				unmark_for_reference(p_Dpb, p_Dpb->fs[i]);
				update_ref_list(p_Dpb);
				break;
			}
		}
	}

	p->is_long_term = 0;
}

static void check_num_ref(struct DecodedPictureBuffer *p_Dpb)
{
	if ((int)(p_Dpb->ltref_frames_in_buffer +
			p_Dpb->ref_frames_in_buffer) >
			imax(1, p_Dpb->num_ref_frames)) {
		struct h264_dpb_stru *p_H264_Dpb = container_of(p_Dpb,
					 struct h264_dpb_stru, mDPB);
		dpb_print(p_H264_Dpb->decoder_index, PRINT_FLAG_DPB_DETAIL,
			  "Max. number of reference frames exceeded. Invalid stream. lt %d ref %d mum_ref %d\n",
			  p_Dpb->ltref_frames_in_buffer,
			  p_Dpb->ref_frames_in_buffer,
			  p_Dpb->num_ref_frames);
	}
}

static void dump_dpb(struct DecodedPictureBuffer *p_Dpb)
{
	unsigned i;
	struct h264_dpb_stru *p_H264_Dpb =
		container_of(p_Dpb, struct h264_dpb_stru, mDPB);
	if ((h264_debug_flag & PRINT_FLAG_DUMP_DPB) == 0)
		return;
	for (i = 0; i < p_Dpb->used_size; i++) {
		dpb_print(p_H264_Dpb->decoder_index,
			PRINT_FLAG_DUMP_DPB,
			"(");
		dpb_print_cont(p_H264_Dpb->decoder_index,
			PRINT_FLAG_DUMP_DPB,
			"fn=%d  ", p_Dpb->fs[i]->frame_num);
		if (p_Dpb->fs[i]->is_used & 1) {
			if (p_Dpb->fs[i]->top_field)
				dpb_print_cont(p_H264_Dpb->decoder_index,
				PRINT_FLAG_DUMP_DPB,
				"T: poc=%d  ",
				p_Dpb->fs[i]->top_field->poc);
			else
				dpb_print_cont(p_H264_Dpb->decoder_index,
				PRINT_FLAG_DUMP_DPB,
				"T: poc=%d  ",
				p_Dpb->fs[i]->frame->top_poc);
		}
		if (p_Dpb->fs[i]->is_used & 2) {
			if (p_Dpb->fs[i]->bottom_field)
				dpb_print_cont(p_H264_Dpb->decoder_index,
				PRINT_FLAG_DUMP_DPB,
				"B: poc=%d  ",
				p_Dpb->fs[i]->bottom_field->poc);
			else
				dpb_print_cont(p_H264_Dpb->decoder_index,
				PRINT_FLAG_DUMP_DPB,
				"B: poc=%d  ",
				p_Dpb->fs[i]->frame->bottom_poc);
		}
		if (p_Dpb->fs[i]->is_used == 3)
			dpb_print_cont(p_H264_Dpb->decoder_index,
			PRINT_FLAG_DUMP_DPB,
			"F: poc=%d  ",
			p_Dpb->fs[i]->frame->poc);
		dpb_print_cont(p_H264_Dpb->decoder_index,
			PRINT_FLAG_DUMP_DPB,
			"G: poc=%d)  ", p_Dpb->fs[i]->poc);
		if (p_Dpb->fs[i]->is_reference)
			dpb_print_cont(p_H264_Dpb->decoder_index,
			PRINT_FLAG_DUMP_DPB,
			"ref (%d) ", p_Dpb->fs[i]->is_reference);
		if (p_Dpb->fs[i]->is_long_term)
			dpb_print_cont(p_H264_Dpb->decoder_index,
			PRINT_FLAG_DUMP_DPB,
			"lt_ref (%d) ", p_Dpb->fs[i]->is_reference);
		if (p_Dpb->fs[i]->is_output)
			dpb_print_cont(p_H264_Dpb->decoder_index,
			PRINT_FLAG_DUMP_DPB,
			"out  ");
		if (p_Dpb->fs[i]->pre_output)
			dpb_print_cont(p_H264_Dpb->decoder_index,
			PRINT_FLAG_DUMP_DPB,
			"for_out  ");
		if (p_Dpb->fs[i]->is_used == 3) {
			if (p_Dpb->fs[i]->frame->non_existing)
				dpb_print_cont(p_H264_Dpb->decoder_index,
				PRINT_FLAG_DUMP_DPB,
				"ne  ");
		}
#if (MVC_EXTENSION_ENABLE)
		if (p_Dpb->fs[i]->is_reference)
			dpb_print_cont(p_H264_Dpb->decoder_index,
			PRINT_FLAG_DUMP_DPB,
			"view_id (%d) ", p_Dpb->fs[i]->view_id);
#endif
		dpb_print_cont(p_H264_Dpb->decoder_index,
			PRINT_FLAG_DUMP_DPB,
			"\n");
	}
}


/*!
 ************************************************************************
 * \brief
 *    adaptive memory management
 *
 ************************************************************************
 */

static int get_pic_num_x(struct StorablePicture *p,
			 int difference_of_pic_nums_minus1)
{
	int currPicNum;

	if (p->structure == FRAME)
		currPicNum = p->frame_num;
	else
		currPicNum = 2 * p->frame_num + 1;

	return currPicNum - (difference_of_pic_nums_minus1 + 1);
}

/*!
 ************************************************************************
 * \brief
 *    Adaptive Memory Management: Mark short term picture unused
 ************************************************************************
 */
static void mm_unmark_short_term_for_reference(struct DecodedPictureBuffer
		*p_Dpb, struct StorablePicture *p,
		int difference_of_pic_nums_minus1)
{
	int picNumX;

	unsigned int i;

	picNumX = get_pic_num_x(p, difference_of_pic_nums_minus1);

	for (i = 0; i < p_Dpb->ref_frames_in_buffer; i++) {
		if (p->structure == FRAME) {
			if ((p_Dpb->fs_ref[i]->is_reference == 3) &&
			    (p_Dpb->fs_ref[i]->is_long_term == 0)) {
				if (p_Dpb->fs_ref[i]->frame->pic_num ==
					picNumX) {
					unmark_for_reference(p_Dpb,
						p_Dpb->fs_ref[i]);
					return;
				}
			}
		} else {
			if ((p_Dpb->fs_ref[i]->is_reference & 1) &&
			    (!(p_Dpb->fs_ref[i]->is_long_term & 1))) {
				if (p_Dpb->fs_ref[i]->top_field->pic_num ==
					picNumX) {
					p_Dpb->fs_ref[i]->
					top_field->used_for_reference = 0;
					p_Dpb->fs_ref[i]->is_reference &= 2;
					if (p_Dpb->fs_ref[i]->is_used == 3) {
						p_Dpb->fs_ref[i]->frame->
							used_for_reference = 0;
					}
					return;
				}
			}
			if ((p_Dpb->fs_ref[i]->is_reference & 2) &&
			    (!(p_Dpb->fs_ref[i]->is_long_term & 2))) {
				if (p_Dpb->fs_ref[i]->bottom_field->pic_num ==
					picNumX) {
					p_Dpb->fs_ref[i]->bottom_field->
					used_for_reference = 0;
					p_Dpb->fs_ref[i]->is_reference &= 1;
					if (p_Dpb->fs_ref[i]->is_used == 3) {
						p_Dpb->fs_ref[i]->frame->
						used_for_reference = 0;
					}
					return;
				}
			}
		}
	}
}

static void unmark_for_long_term_reference(struct FrameStore *fs)
{
	if (fs->is_used & 1) {
		if (fs->top_field) {
			fs->top_field->used_for_reference = 0;
			fs->top_field->is_long_term = 0;
		}
	}
	if (fs->is_used & 2) {
		if (fs->bottom_field) {
			fs->bottom_field->used_for_reference = 0;
			fs->bottom_field->is_long_term = 0;
		}
	}
	if (fs->is_used == 3) {
		if (fs->top_field && fs->bottom_field) {
			fs->top_field->used_for_reference = 0;
			fs->top_field->is_long_term = 0;
			fs->bottom_field->used_for_reference = 0;
			fs->bottom_field->is_long_term = 0;
		}
		fs->frame->used_for_reference = 0;
		fs->frame->is_long_term = 0;
	}

	fs->is_reference = 0;
	fs->is_long_term = 0;
}

/*!
 ************************************************************************
 * \brief
 *    Adaptive Memory Management: Mark long term picture unused
 ************************************************************************
 */
static void mm_unmark_long_term_for_reference(struct DecodedPictureBuffer
		*p_Dpb, struct StorablePicture *p, int long_term_pic_num)
{
	unsigned int i;
	for (i = 0; i < p_Dpb->ltref_frames_in_buffer; i++) {
		if (p->structure == FRAME) {
			if ((p_Dpb->fs_ltref[i]->is_reference == 3) &&
			    (p_Dpb->fs_ltref[i]->is_long_term == 3)) {
				if (p_Dpb->fs_ltref[i]->frame->
					long_term_pic_num ==
					long_term_pic_num) {
					unmark_for_long_term_reference(
						p_Dpb->fs_ltref[i]);
				}
			}
		} else {
			if ((p_Dpb->fs_ltref[i]->is_reference & 1) &&
			    ((p_Dpb->fs_ltref[i]->is_long_term & 1))) {
				if (p_Dpb->fs_ltref[i]->top_field->
					long_term_pic_num ==
					long_term_pic_num) {
					p_Dpb->fs_ltref[i]->top_field->
						used_for_reference = 0;
					p_Dpb->fs_ltref[i]->top_field->
						is_long_term = 0;
					p_Dpb->fs_ltref[i]->is_reference &= 2;
					p_Dpb->fs_ltref[i]->is_long_term &= 2;
					if (p_Dpb->fs_ltref[i]->is_used == 3) {
						p_Dpb->fs_ltref[i]->frame->
							used_for_reference = 0;
						p_Dpb->fs_ltref[i]->frame->
							is_long_term = 0;
					}
					return;
				}
			}
			if ((p_Dpb->fs_ltref[i]->is_reference & 2) &&
			    ((p_Dpb->fs_ltref[i]->is_long_term & 2))) {
				if (p_Dpb->fs_ltref[i]->bottom_field->
					long_term_pic_num ==
					long_term_pic_num) {
					p_Dpb->fs_ltref[i]->bottom_field->
						used_for_reference = 0;
					p_Dpb->fs_ltref[i]->bottom_field->
						is_long_term = 0;
					p_Dpb->fs_ltref[i]->is_reference &= 1;
					p_Dpb->fs_ltref[i]->is_long_term &= 1;
					if (p_Dpb->fs_ltref[i]->is_used == 3) {
						p_Dpb->fs_ltref[i]->frame->
							used_for_reference = 0;
						p_Dpb->fs_ltref[i]->frame->
							is_long_term = 0;
					}
					return;
				}
			}
		}
	}
}


/*!
 ************************************************************************
 * \brief
 *    Mark a long-term reference frame or complementary
 *    field pair unused for referemce
 ************************************************************************
 */
static void unmark_long_term_frame_for_reference_by_frame_idx(
	struct DecodedPictureBuffer *p_Dpb, int long_term_frame_idx)
{
	unsigned int i;
	for (i = 0; i < p_Dpb->ltref_frames_in_buffer; i++) {
		if (p_Dpb->fs_ltref[i]->long_term_frame_idx ==
			long_term_frame_idx)
			unmark_for_long_term_reference(p_Dpb->fs_ltref[i]);
	}
}


static void unmark1(struct DecodedPictureBuffer *p_Dpb,
	unsigned curr_frame_num, int i)
{
	if (p_Dpb->last_picture) {
		if ((p_Dpb->last_picture != p_Dpb->fs_ltref[i]) ||
			p_Dpb->last_picture->frame_num != curr_frame_num) {
			unmark_for_long_term_reference(p_Dpb->fs_ltref[i]);
		} else {
			unmark_for_long_term_reference(p_Dpb->fs_ltref[i]);
		}
	}
}

static void unmark2(struct DecodedPictureBuffer *p_Dpb,
	int curr_pic_num, int i)
{
	if ((p_Dpb->fs_ltref[i]->frame_num) != (unsigned)(curr_pic_num >> 1))
		unmark_for_long_term_reference(p_Dpb->fs_ltref[i]);
}

static void unmark3_top(struct DecodedPictureBuffer *p_Dpb,
	unsigned curr_frame_num, int curr_pic_num, int mark_current, int i)
{
	if (p_Dpb->fs_ltref[i]->is_long_term == 3) {
		unmark_for_long_term_reference(p_Dpb->fs_ltref[i]);
	} else {
		if (p_Dpb->fs_ltref[i]->is_long_term == 1) {
			unmark_for_long_term_reference(p_Dpb->fs_ltref[i]);
		} else {
			if (mark_current)
				unmark1(p_Dpb, curr_frame_num, i);
			else
				unmark2(p_Dpb, curr_pic_num, i);
		}
	}
}

static void unmark3_bottom(struct DecodedPictureBuffer *p_Dpb,
	unsigned curr_frame_num, int curr_pic_num, int mark_current, int i)
{
	if (p_Dpb->fs_ltref[i]->is_long_term == 2) {
		unmark_for_long_term_reference(p_Dpb->fs_ltref[i]);
	} else {
		if (mark_current)
			unmark1(p_Dpb, curr_frame_num, i);
		else
			unmark2(p_Dpb, curr_pic_num, i);
	}
}

static void unmark_long_term_field_for_reference_by_frame_idx(
	struct DecodedPictureBuffer *p_Dpb, enum PictureStructure structure,
	int long_term_frame_idx, int mark_current, unsigned curr_frame_num,
	int curr_pic_num)
{
	struct VideoParameters *p_Vid = p_Dpb->p_Vid;
	unsigned i;

	/* assert(structure!=FRAME); */
	if (curr_pic_num < 0)
		curr_pic_num += (2 * p_Vid->max_frame_num);

	for (i = 0; i < p_Dpb->ltref_frames_in_buffer; i++) {
		if (p_Dpb->fs_ltref[i]->long_term_frame_idx ==
			long_term_frame_idx) {
			if (structure == TOP_FIELD)
				unmark3_top(p_Dpb, curr_frame_num,
					curr_pic_num, mark_current, i);

			if (structure == BOTTOM_FIELD)
				unmark3_bottom(p_Dpb, curr_frame_num,
					curr_pic_num, mark_current, i);
		}
	}
}

/*!
 ************************************************************************
 * \brief
 *    mark a picture as long-term reference
 ************************************************************************
 */
static void mark_pic_long_term(struct DecodedPictureBuffer *p_Dpb,
			       struct StorablePicture *p,
			       int long_term_frame_idx, int picNumX)
{
	struct h264_dpb_stru *p_H264_Dpb = container_of(p_Dpb,
				struct h264_dpb_stru, mDPB);
	unsigned int i;
	int add_top, add_bottom;

	if (p->structure == FRAME) {
		for (i = 0; i < p_Dpb->ref_frames_in_buffer; i++) {
			if (p_Dpb->fs_ref[i]->is_reference == 3) {
				if ((!p_Dpb->fs_ref[i]->frame->
					is_long_term) &&
				    (p_Dpb->fs_ref[i]->frame->pic_num ==
					picNumX)) {
					p_Dpb->fs_ref[i]->
						long_term_frame_idx =
					p_Dpb->fs_ref[i]->frame->
						long_term_frame_idx =
						long_term_frame_idx;
					p_Dpb->fs_ref[i]->frame->
						long_term_pic_num =
						long_term_frame_idx;
					p_Dpb->fs_ref[i]->frame->
						is_long_term = 1;

					if (p_Dpb->fs_ref[i]->top_field &&
					    p_Dpb->fs_ref[i]->bottom_field) {
						p_Dpb->fs_ref[i]->top_field->
						long_term_frame_idx =
							p_Dpb->fs_ref[i]->
							bottom_field->
							long_term_frame_idx =
							long_term_frame_idx;
						p_Dpb->fs_ref[i]->top_field->
							long_term_pic_num =
							long_term_frame_idx;
						p_Dpb->fs_ref[i]->
							bottom_field->
							long_term_pic_num =
							long_term_frame_idx;

						p_Dpb->fs_ref[i]->top_field->
							is_long_term =
							p_Dpb->fs_ref[i]->
							bottom_field->
							is_long_term
							= 1;

					}
					p_Dpb->fs_ref[i]->is_long_term = 3;
					return;
				}
			}
		}
		dpb_print(p_H264_Dpb->decoder_index, PRINT_FLAG_DPB_DETAIL,
			  "Warning: reference frame for long term marking not found\n");
	} else {
		if (p->structure == TOP_FIELD) {
			add_top    = 1;
			add_bottom = 0;
		} else {
			add_top    = 0;
			add_bottom = 1;
		}
		for (i = 0; i < p_Dpb->ref_frames_in_buffer; i++) {
			if (p_Dpb->fs_ref[i]->is_reference & 1) {
				if ((!p_Dpb->fs_ref[i]->top_field->
					is_long_term) &&
				    (p_Dpb->fs_ref[i]->top_field->pic_num ==
					picNumX)) {
					if ((p_Dpb->fs_ref[i]->
						is_long_term) &&
					    (p_Dpb->fs_ref[i]->
						long_term_frame_idx !=
						long_term_frame_idx)) {
						dpb_print(p_H264_Dpb->
						decoder_index,
						PRINT_FLAG_DPB_DETAIL,
						"Warning: assigning long_term_frame_idx different from other field\n");
					}

					p_Dpb->fs_ref[i]->
						long_term_frame_idx =
						p_Dpb->fs_ref[i]->top_field->
						long_term_frame_idx
						= long_term_frame_idx;
					p_Dpb->fs_ref[i]->top_field->
						long_term_pic_num =
						2 * long_term_frame_idx +
						add_top;
					p_Dpb->fs_ref[i]->top_field->
						is_long_term = 1;
					p_Dpb->fs_ref[i]->is_long_term |= 1;
					if (p_Dpb->fs_ref[i]->is_long_term ==
						3) {
						p_Dpb->fs_ref[i]->frame->
							is_long_term = 1;
						p_Dpb->fs_ref[i]->frame->
							long_term_frame_idx =
							p_Dpb->fs_ref[i]->
							frame->
							long_term_pic_num =
							long_term_frame_idx;
					}
					return;
				}
			}
			if (p_Dpb->fs_ref[i]->is_reference & 2) {
				if ((!p_Dpb->fs_ref[i]->bottom_field->
					is_long_term) &&
				    (p_Dpb->fs_ref[i]->bottom_field->pic_num
					== picNumX)) {
					if ((p_Dpb->fs_ref[i]->
						is_long_term) &&
					    (p_Dpb->fs_ref[i]->
						long_term_frame_idx !=
						long_term_frame_idx)) {
						dpb_print(p_H264_Dpb->
						decoder_index,
						PRINT_FLAG_DPB_DETAIL,
						"Warning: assigning long_term_frame_idx different from other field\n");
					}

					p_Dpb->fs_ref[i]->
						long_term_frame_idx =
						p_Dpb->fs_ref[i]->bottom_field
						->long_term_frame_idx
						= long_term_frame_idx;
					p_Dpb->fs_ref[i]->bottom_field->
						long_term_pic_num = 2 *
						long_term_frame_idx +
						add_bottom;
					p_Dpb->fs_ref[i]->bottom_field->
						is_long_term = 1;
					p_Dpb->fs_ref[i]->is_long_term |= 2;
					if (p_Dpb->fs_ref[i]->
						is_long_term == 3) {
						p_Dpb->fs_ref[i]->frame->
							is_long_term = 1;
						p_Dpb->fs_ref[i]->frame->
							long_term_frame_idx =
							p_Dpb->fs_ref[i]->
							frame->
							long_term_pic_num =
							long_term_frame_idx;
					}
					return;
				}
			}
		}
		dpb_print(p_H264_Dpb->decoder_index, PRINT_FLAG_DPB_DETAIL,
			  "Warning: reference field for long term marking not found\n");
	}
}


/*!
 ************************************************************************
 * \brief
 *    Assign a long term frame index to a short term picture
 ************************************************************************
 */
static void mm_assign_long_term_frame_idx(struct DecodedPictureBuffer *p_Dpb,
		struct StorablePicture *p, int difference_of_pic_nums_minus1,
		int long_term_frame_idx)
{
	struct h264_dpb_stru *p_H264_Dpb = container_of(p_Dpb,
			struct h264_dpb_stru, mDPB);
	int picNumX = get_pic_num_x(p, difference_of_pic_nums_minus1);

	/* remove frames/fields with same long_term_frame_idx */
	if (p->structure == FRAME) {
		unmark_long_term_frame_for_reference_by_frame_idx(p_Dpb,
				long_term_frame_idx);
	} else {
		unsigned i;
		enum PictureStructure structure = FRAME;

		for (i = 0; i < p_Dpb->ref_frames_in_buffer; i++) {
			if (p_Dpb->fs_ref[i]->is_reference & 1) {
				if (p_Dpb->fs_ref[i]->top_field->
					pic_num == picNumX) {
					structure = TOP_FIELD;
					break;
				}
			}
			if (p_Dpb->fs_ref[i]->is_reference & 2) {
				if (p_Dpb->fs_ref[i]->bottom_field->
					pic_num == picNumX) {
					structure = BOTTOM_FIELD;
					break;
				}
			}
		}
		if (structure == FRAME) {
			dpb_print(p_H264_Dpb->decoder_index,
				  PRINT_FLAG_DPB_DETAIL,
				  "field for long term marking not found %d",
				  200);
		}

		unmark_long_term_field_for_reference_by_frame_idx(p_Dpb,
				structure,
				long_term_frame_idx, 0, 0, picNumX);
	}

	mark_pic_long_term(p_Dpb, p, long_term_frame_idx, picNumX);
}

/*!
 ************************************************************************
 * \brief
 *    Set new max long_term_frame_idx
 ************************************************************************
 */
static void mm_update_max_long_term_frame_idx(struct DecodedPictureBuffer
		*p_Dpb, int max_long_term_frame_idx_plus1)
{
	unsigned int i;

	p_Dpb->max_long_term_pic_idx = max_long_term_frame_idx_plus1 - 1;

	/* check for invalid frames */
	for (i = 0; i < p_Dpb->ltref_frames_in_buffer; i++) {
		if (p_Dpb->fs_ltref[i]->long_term_frame_idx >
			p_Dpb->max_long_term_pic_idx) {
			unmark_for_long_term_reference(p_Dpb->fs_ltref[i]);
		}
	}
}


/*!
 ************************************************************************
 * \brief
 *    Mark all long term reference pictures unused for reference
 ************************************************************************
 */
static void mm_unmark_all_long_term_for_reference(struct DecodedPictureBuffer
		*p_Dpb)
{
	mm_update_max_long_term_frame_idx(p_Dpb, 0);
}

/*!
 ************************************************************************
 * \brief
 *    Mark all short term reference pictures unused for reference
 ************************************************************************
 */
static void mm_unmark_all_short_term_for_reference(struct DecodedPictureBuffer
		*p_Dpb)
{
	unsigned int i;
	for (i = 0; i < p_Dpb->ref_frames_in_buffer; i++)
		unmark_for_reference(p_Dpb, p_Dpb->fs_ref[i]);
	update_ref_list(p_Dpb);
}


/*!
 ************************************************************************
 * \brief
 *    Mark the current picture used for long term reference
 ************************************************************************
 */
static void mm_mark_current_picture_long_term(struct DecodedPictureBuffer
		*p_Dpb, struct StorablePicture *p, int long_term_frame_idx)
{
	/* remove long term pictures with same long_term_frame_idx */
	if (p->structure == FRAME) {
		unmark_long_term_frame_for_reference_by_frame_idx(p_Dpb,
				long_term_frame_idx);
	} else {
		unmark_long_term_field_for_reference_by_frame_idx(p_Dpb,
				p->structure, long_term_frame_idx,
				1, p->pic_num, 0);
	}

	p->is_long_term = 1;
	p->long_term_frame_idx = long_term_frame_idx;
}

static void adaptive_memory_management(struct h264_dpb_stru *p_H264_Dpb,
				       struct StorablePicture *p)
{
	struct DecodedPictureBuffer *p_Dpb = &p_H264_Dpb->mDPB;
	struct DecRefPicMarking_s *tmp_drpm;
	struct VideoParameters *p_Vid = p_Dpb->p_Vid;
	dpb_print(p_H264_Dpb->decoder_index, PRINT_FLAG_DPB_DETAIL,
				"%s\n", __func__);
	p_Vid->last_has_mmco_5 = 0;

	/* assert (!p->idr_flag); */
	/* assert (p->adaptive_ref_pic_buffering_flag); */

	while (p->dec_ref_pic_marking_buffer) {
		tmp_drpm = p->dec_ref_pic_marking_buffer;
		switch (tmp_drpm->memory_management_control_operation) {
		case 0:
			if (tmp_drpm->Next != NULL)
				dpb_print(p_H264_Dpb->decoder_index,
					PRINT_FLAG_ERROR,
					"error, memory_management_control_operation = 0 not last operation in buffer\n");
			break;
		case 1:
			mm_unmark_short_term_for_reference(p_Dpb, p,
				tmp_drpm->difference_of_pic_nums_minus1);
			update_ref_list(p_Dpb);
			break;
		case 2:
			mm_unmark_long_term_for_reference(p_Dpb, p,
				tmp_drpm->long_term_pic_num);
			update_ltref_list(p_Dpb);
			break;
		case 3:
			mm_assign_long_term_frame_idx(p_Dpb, p,
				tmp_drpm->difference_of_pic_nums_minus1,
				tmp_drpm->long_term_frame_idx);
			update_ref_list(p_Dpb);
			update_ltref_list(p_Dpb);
			break;
		case 4:
			mm_update_max_long_term_frame_idx(p_Dpb,
				tmp_drpm->max_long_term_frame_idx_plus1);
			update_ltref_list(p_Dpb);
			break;
		case 5:
			mm_unmark_all_short_term_for_reference(p_Dpb);
			mm_unmark_all_long_term_for_reference(p_Dpb);
			p_Vid->last_has_mmco_5 = 1;
			break;
		case 6:
			mm_mark_current_picture_long_term(p_Dpb, p,
				tmp_drpm->long_term_frame_idx);
			check_num_ref(p_Dpb);
			break;
		default:
			dpb_print(p_H264_Dpb->decoder_index,
				PRINT_FLAG_ERROR,
				"error, invalid memory_management_control_operation in buffer\n");
		}
		p->dec_ref_pic_marking_buffer = tmp_drpm->Next;
		/* free (tmp_drpm); */
	}
	if (p_Vid->last_has_mmco_5) {
		p->pic_num = p->frame_num = 0;

		switch (p->structure) {
		case TOP_FIELD: {
			/* p->poc = p->top_poc = p_Vid->toppoc =0; */
			p->poc = p->top_poc = 0;
			break;
		}
		case BOTTOM_FIELD: {
			/* p->poc = p->bottom_poc = p_Vid->bottompoc = 0; */
			p->poc = p->bottom_poc = 0;
			break;
		}
		case FRAME: {
			p->top_poc    -= p->poc;
			p->bottom_poc -= p->poc;

			/* p_Vid->toppoc = p->top_poc; */
			/* p_Vid->bottompoc = p->bottom_poc; */

			p->poc = imin(p->top_poc, p->bottom_poc);
			/* p_Vid->framepoc = p->poc; */
			break;
		}
		}
		/* currSlice->ThisPOC = p->poc; */
#if (MVC_EXTENSION_ENABLE)
		if (p->view_id == 0) {
			flush_dpb(p_Vid->p_Dpb_layer[0]);
			flush_dpb(p_Vid->p_Dpb_layer[1]);
		} else {
			flush_dpb(p_Dpb);
		}
#else
		flush_dpb(p_H264_Dpb);
#endif
	}
}


void store_picture_in_dpb(struct h264_dpb_stru *p_H264_Dpb,
			  struct StorablePicture *p,
			  unsigned char data_flag)
{
	/* struct VideoParameters *p_Vid = p_Dpb->p_Vid; */
	struct VideoParameters *p_Vid = &p_H264_Dpb->mVideo;
	struct DecodedPictureBuffer *p_Dpb = &p_H264_Dpb->mDPB;
	unsigned i;
#if 0
	int poc, pos;
#endif
	dpb_print(p_H264_Dpb->decoder_index, PRINT_FLAG_DPB_DETAIL,
		"%s p_Vid %p\n", __func__, p_Vid);

	/* picture error concealment */

	/* diagnostics */
	/* dpb_print(p_H264_Dpb->decoder_index, PRINT_FLAG_DPB_DETAIL,
		"Storing (%s) non-ref pic with frame_num #%d\n",
		(p->type == FRAME)?"FRAME":(p->type == TOP_FIELD)?
		"TOP_FIELD":"BOTTOM_FIELD", p->pic_num); */
	/* if frame, check for new store, */
	/* assert (p!=NULL); */

	p_Vid->last_has_mmco_5 = 0;
	p_Vid->last_pic_bottom_field = (p->structure == BOTTOM_FIELD);

	if (p->idr_flag) {
		idr_memory_management(p_H264_Dpb, p);
#if 0
/* ??? */
		/* picture error concealment */
		memset(p_Vid->pocs_in_dpb, 0, sizeof(int) * 100);
#endif
	} else {
#if 1
/* ??? */
		/* adaptive memory management */
		if (p->used_for_reference &&
			(p->adaptive_ref_pic_buffering_flag))
			adaptive_memory_management(p_H264_Dpb, p);
#endif
	}

	if ((p->structure == TOP_FIELD) || (p->structure == BOTTOM_FIELD)) {
		/* check for frame store with same pic_number */
		if (p_Dpb->last_picture) {
			if ((int)p_Dpb->last_picture->frame_num ==
				p->pic_num) {
				if (((p->structure == TOP_FIELD) &&
				    (p_Dpb->last_picture->is_used == 2)) ||
				    ((p->structure == BOTTOM_FIELD) &&
				    (p_Dpb->last_picture->is_used == 1))) {
					if ((p->used_for_reference &&
					    (p_Dpb->last_picture->
						is_orig_reference != 0)) ||
					    (!p->used_for_reference &&
						(p_Dpb->last_picture->
						is_orig_reference == 0))) {
						insert_picture_in_dpb(
							p_H264_Dpb,
							p_Dpb->last_picture,
							p, data_flag);
						update_ref_list(p_Dpb);
						update_ltref_list(p_Dpb);
						dump_dpb(p_Dpb);
						p_Dpb->last_picture = NULL;
						return;
					}
				}
			}
		}
	}
	/* this is a frame or a field which has no stored
	 * complementary field */

	/* sliding window, if necessary */
	if ((!p->idr_flag) && (p->used_for_reference &&
			       (!p->adaptive_ref_pic_buffering_flag))) {
		sliding_window_memory_management(p_Dpb, p);
	}

	/* picture error concealment */
	if (p_Vid->conceal_mode != 0) {
		for (i = 0; i < p_Dpb->size; i++)
			if (p_Dpb->fs[i]->is_reference)
				p_Dpb->fs[i]->concealment_reference = 1;
	}

	while (remove_unused_frame_from_dpb(p_H264_Dpb))
		;

	while (output_frames(p_H264_Dpb, 0))
		;

	/* check for duplicate frame number in short term reference buffer */
	if ((p->used_for_reference) && (!p->is_long_term)) {
		for (i = 0; i < p_Dpb->ref_frames_in_buffer; i++) {
			if (p_Dpb->fs_ref[i]->frame_num == p->frame_num) {
				dpb_print(p_H264_Dpb->decoder_index,
					  PRINT_FLAG_DPB_DETAIL,
					  "duplicate frame_num in short-term reference picture buffer %d\n",
					   500);
			}
		}
	}
	/* store at end of buffer */

	dpb_print(p_H264_Dpb->decoder_index, PRINT_FLAG_DPB_DETAIL,
		  "%s p_Dpb->used_size %d\n", __func__, p_Dpb->used_size);
	if (p_Dpb->used_size >= p_Dpb->size) {
		dpb_print(p_H264_Dpb->decoder_index,
			PRINT_FLAG_ERROR,
			"%s Error: used_sizd %d is large than dpb size\r\n",
			__func__, p_Dpb->used_size);
		h264_debug_flag |= PRINT_FLAG_DUMP_DPB;
		dump_dpb(p_Dpb);
		return;
	}

	insert_picture_in_dpb(p_H264_Dpb, p_Dpb->fs[p_Dpb->used_size],
		p, data_flag);

	/* picture error concealment */
	if (p->idr_flag)
		p_Vid->earlier_missing_poc = 0;

	if (p->structure != FRAME)
		p_Dpb->last_picture = p_Dpb->fs[p_Dpb->used_size];
	else
		p_Dpb->last_picture = NULL;

	p_Dpb->used_size++;
#if 0
/* ??? */
	if (p_Vid->conceal_mode != 0)
		p_Vid->pocs_in_dpb[p_Dpb->used_size - 1] = p->poc;
#endif
	update_ref_list(p_Dpb);
	update_ltref_list(p_Dpb);

	check_num_ref(p_Dpb);

	dump_dpb(p_Dpb);
}

void bufmgr_post(struct h264_dpb_stru *p_H264_Dpb)
{
	/*VideoParameters *p_Vid = p_Dpb->p_Vid;*/
	struct VideoParameters *p_Vid = &p_H264_Dpb->mVideo;
	if (p_Vid->last_has_mmco_5)
		p_Vid->pre_frame_num = 0;
}
/**********************************
*
*   Initialize reference lists
***********************************/
#define __COMPARE(context, p1, p2) comp(p1, p2)
#define __SHORTSORT(lo, hi, width, comp, context) \
	shortsort(lo, hi, width, comp)
#define CUTOFF 8            /* testing shows that this is good value */
#define STKSIZ (8*sizeof(void *) - 2)

#undef swap
static void swap(
	char *a,
	char *b,
	size_t width
)
{
	char tmp;

	if (a != b)
		/* Do the swap one character at a time to avoid potential
		   alignment problems. */
		while (width--) {
			tmp = *a;
			*a++ = *b;
			*b++ = tmp;
		}
}

static void shortsort(
	char *lo,
	char *hi,
	size_t width,
	int (*comp)(const void *, const void *)
)
{
	char *p, *max;

	/* Note: in assertions below, i and j are alway inside original
	   bound of array to sort. */

	while (hi > lo) {
		/* A[i] <= A[j] for i <= j, j > hi */
		max = lo;
		for (p = lo + width; p <= hi; p += width) {
			/* A[i] <= A[max] for lo <= i < p */
			if (__COMPARE(context, p, max) > 0)
				max = p;
			/* A[i] <= A[max] for lo <= i <= p */
		}

		/* A[i] <= A[max] for lo <= i <= hi */

		swap(max, hi, width);

		/* A[i] <= A[hi] for i <= hi, so A[i] <= A[j] for i <= j,
		   j >= hi */

		hi -= width;

		/* A[i] <= A[j] for i <= j, j > hi, loop top condition
		   established */
	}
	/* A[i] <= A[j] for i <= j, j > lo, which implies A[i] <= A[j]
	   for i < j, so array is sorted */
}

static void qsort(
	void *base,
	size_t num,
	size_t width,
	int (*comp)(const void *, const void *)
)
{
	char *lo, *hi;              /* ends of sub-array currently sorting */
	char *mid;                  /* points to middle of subarray */
	char *loguy, *higuy;        /* traveling pointers for partition step */
	size_t size;                /* size of the sub-array */
	char *lostk[STKSIZ], *histk[STKSIZ];
	int stkptr;                 /* stack for saving sub-array to be
					processed */
#if 0
	/* validation section */
	_VALIDATE_RETURN_VOID(base != NULL || num == 0, EINVAL);
	_VALIDATE_RETURN_VOID(width > 0, EINVAL);
	_VALIDATE_RETURN_VOID(comp != NULL, EINVAL);
#endif
	if (num < 2)
		return;                 /* nothing to do */

	stkptr = 0;                 /* initialize stack */

	lo = (char *)base;
	hi = (char *)base + width * (num - 1);      /* initialize limits */

	/* this entry point is for pseudo-recursion calling: setting
	   lo and hi and jumping to here is like recursion, but stkptr is
	   preserved, locals aren't, so we preserve stuff on the stack */
recurse:

	size = (hi - lo) / width + 1;        /* number of el's to sort */

	/* below a certain size, it is faster to use a O(n^2) sorting method */
	if (size <= CUTOFF) {
		__SHORTSORT(lo, hi, width, comp, context);
	} else {
		/* First we pick a partitioning element.  The efficiency of
		   the algorithm demands that we find one that is approximately
		   the median of the values, but also that we select one fast.
		   We choose the median of the first, middle, and last
		   elements, to avoid bad performance in the face of already
		   sorted data, or data that is made up of multiple sorted
		   runs appended together.  Testing shows that a
		   median-of-three algorithm provides better performance than
		   simply picking the middle element for the latter case. */

		mid = lo + (size / 2) * width;      /* find middle element */

		/* Sort the first, middle, last elements into order */
		if (__COMPARE(context, lo, mid) > 0)
			swap(lo, mid, width);
		if (__COMPARE(context, lo, hi) > 0)
			swap(lo, hi, width);
		if (__COMPARE(context, mid, hi) > 0)
			swap(mid, hi, width);

		/* We now wish to partition the array into three pieces, one
		   consisting of elements <= partition element, one of elements
		   equal to the partition element, and one of elements > than
		   it. This is done below; comments indicate conditions
		   established at every step. */

		loguy = lo;
		higuy = hi;

		/* Note that higuy decreases and loguy increases on every
		   iteration, so loop must terminate. */
		for (;;) {
			/* lo <= loguy < hi, lo < higuy <= hi,
			   A[i] <= A[mid] for lo <= i <= loguy,
			   A[i] > A[mid] for higuy <= i < hi,
			   A[hi] >= A[mid] */

			/* The doubled loop is to avoid calling comp(mid,mid),
			   since some existing comparison funcs don't work
			   when passed the same value for both pointers. */

			if (mid > loguy) {
				do  {
					loguy += width;
				} while (loguy < mid &&
					__COMPARE(context, loguy, mid) <= 0);
			}
			if (mid <= loguy) {
				do  {
					loguy += width;
				} while (loguy <= hi &&
					__COMPARE(context, loguy, mid) <= 0);
			}

			/* lo < loguy <= hi+1, A[i] <= A[mid] for
			   lo <= i < loguy,
			   either loguy > hi or A[loguy] > A[mid] */

			do  {
				higuy -= width;
			} while (higuy > mid &&
					__COMPARE(context, higuy, mid) > 0);

			/* lo <= higuy < hi, A[i] > A[mid] for higuy < i < hi,
			   either higuy == lo or A[higuy] <= A[mid] */

			if (higuy < loguy)
				break;

			/* if loguy > hi or higuy == lo, then we would have
			   exited, so A[loguy] > A[mid], A[higuy] <= A[mid],
			   loguy <= hi, higuy > lo */

			swap(loguy, higuy, width);

			/* If the partition element was moved, follow it.
			   Only need to check for mid == higuy, since before
			   the swap, A[loguy] > A[mid] implies loguy != mid. */

			if (mid == higuy)
				mid = loguy;

			/* A[loguy] <= A[mid], A[higuy] > A[mid]; so condition
			   at top of loop is re-established */
		}

		/*     A[i] <= A[mid] for lo <= i < loguy,
		       A[i] > A[mid] for higuy < i < hi,
		       A[hi] >= A[mid]
		       higuy < loguy
		   implying:
		       higuy == loguy-1
		       or higuy == hi - 1, loguy == hi + 1, A[hi] == A[mid] */

		/* Find adjacent elements equal to the partition element.  The
		   doubled loop is to avoid calling comp(mid,mid), since some
		   existing comparison funcs don't work when passed the same
		   value for both pointers. */

		higuy += width;
		if (mid < higuy) {
			do  {
				higuy -= width;
			} while (higuy > mid &&
				__COMPARE(context, higuy, mid) == 0);
		}
		if (mid >= higuy) {
			do  {
				higuy -= width;
			} while (higuy > lo &&
				__COMPARE(context, higuy, mid) == 0);
		}

		/* OK, now we have the following:
		      higuy < loguy
		      lo <= higuy <= hi
		      A[i]  <= A[mid] for lo <= i <= higuy
		      A[i]  == A[mid] for higuy < i < loguy
		      A[i]  >  A[mid] for loguy <= i < hi
		      A[hi] >= A[mid] */

		/* We've finished the partition, now we want to sort the
		   subarrays [lo, higuy] and [loguy, hi].
		   We do the smaller one first to minimize stack usage.
		   We only sort arrays of length 2 or more.*/

		if (higuy - lo >= hi - loguy) {
			if (lo < higuy) {
				lostk[stkptr] = lo;
				histk[stkptr] = higuy;
				++stkptr;
			}                    /* save big recursion for later */

			if (loguy < hi) {
				lo = loguy;
				goto recurse;          /* do small recursion */
			}
		} else {
			if (loguy < hi) {
				lostk[stkptr] = loguy;
				histk[stkptr] = hi;
				++stkptr;    /* save big recursion for later */
			}

			if (lo < higuy) {
				hi = higuy;
				goto recurse;          /* do small recursion */
			}
		}
	}

	/* We have sorted the array, except for any pending sorts on the stack.
	   Check if there are any, and do them. */

	--stkptr;
	if (stkptr >= 0) {
		lo = lostk[stkptr];
		hi = histk[stkptr];
		goto recurse;           /* pop subarray from stack */
	} else
		return;                 /* all subarrays done */
}

/*!
 ************************************************************************
 * \brief
 *    compares two stored pictures by picture number for qsort in
 *    descending order
 *
 ************************************************************************
 */
static inline int compare_pic_by_pic_num_desc(const void *arg1,
		const void *arg2)
{
	int pic_num1 = (*(struct StorablePicture **)arg1)->pic_num;
	int pic_num2 = (*(struct StorablePicture **)arg2)->pic_num;

	if (pic_num1 < pic_num2)
		return 1;
	if (pic_num1 > pic_num2)
		return -1;
	else
		return 0;
}

/*!
 ************************************************************************
 * \brief
 *    compares two stored pictures by picture number for qsort in
 *    descending order
 *
 ************************************************************************
 */
static inline int compare_pic_by_lt_pic_num_asc(const void *arg1,
		const void *arg2)
{
	int long_term_pic_num1 =
		(*(struct StorablePicture **)arg1)->long_term_pic_num;
	int long_term_pic_num2 =
		(*(struct StorablePicture **)arg2)->long_term_pic_num;

	if (long_term_pic_num1 < long_term_pic_num2)
		return -1;
	if (long_term_pic_num1 > long_term_pic_num2)
		return 1;
	else
		return 0;
}

/*!
 ************************************************************************
 * \brief
 *    compares two frame stores by pic_num for qsort in descending order
 *
 ************************************************************************
 */
static inline int compare_fs_by_frame_num_desc(const void *arg1,
		const void *arg2)
{
	int frame_num_wrap1 = (*(struct FrameStore **)arg1)->frame_num_wrap;
	int frame_num_wrap2 = (*(struct FrameStore **)arg2)->frame_num_wrap;
	if (frame_num_wrap1 < frame_num_wrap2)
		return 1;
	if (frame_num_wrap1 > frame_num_wrap2)
		return -1;
	else
		return 0;
}


/*!
 ************************************************************************
 * \brief
 *    compares two frame stores by lt_pic_num for qsort in descending order
 *
 ************************************************************************
 */
static inline int compare_fs_by_lt_pic_idx_asc(const void *arg1,
		const void *arg2)
{
	int long_term_frame_idx1 =
		(*(struct FrameStore **)arg1)->long_term_frame_idx;
	int long_term_frame_idx2 =
		(*(struct FrameStore **)arg2)->long_term_frame_idx;

	if (long_term_frame_idx1 < long_term_frame_idx2)
		return -1;
	else if (long_term_frame_idx1 > long_term_frame_idx2)
		return 1;
	else
		return 0;
}


/*!
 ************************************************************************
 * \brief
 *    compares two stored pictures by poc for qsort in ascending order
 *
 ************************************************************************
 */
static inline int compare_pic_by_poc_asc(const void *arg1, const void *arg2)
{
	int poc1 = (*(struct StorablePicture **)arg1)->poc;
	int poc2 = (*(struct StorablePicture **)arg2)->poc;

	if (poc1 < poc2)
		return -1;
	else if (poc1 > poc2)
		return 1;
	else
		return 0;
}


/*!
 ************************************************************************
 * \brief
 *    compares two stored pictures by poc for qsort in descending order
 *
 ************************************************************************
 */
static inline int compare_pic_by_poc_desc(const void *arg1, const void *arg2)
{
	int poc1 = (*(struct StorablePicture **)arg1)->poc;
	int poc2 = (*(struct StorablePicture **)arg2)->poc;

	if (poc1 < poc2)
		return 1;
	else if (poc1 > poc2)
		return -1;
	else
		return 0;
}


/*!
 ************************************************************************
 * \brief
 *    compares two frame stores by poc for qsort in ascending order
 *
 ************************************************************************
 */
static inline int compare_fs_by_poc_asc(const void *arg1, const void *arg2)
{
	int poc1 = (*(struct FrameStore **)arg1)->poc;
	int poc2 = (*(struct FrameStore **)arg2)->poc;

	if (poc1 < poc2)
		return -1;
	else if (poc1 > poc2)
		return 1;
	else
		return 0;
}


/*!
 ************************************************************************
 * \brief
 *    compares two frame stores by poc for qsort in descending order
 *
 ************************************************************************
 */
static inline int compare_fs_by_poc_desc(const void *arg1, const void *arg2)
{
	int poc1 = (*(struct FrameStore **)arg1)->poc;
	int poc2 = (*(struct FrameStore **)arg2)->poc;

	if (poc1 < poc2)
		return 1;
	else if (poc1 > poc2)
		return -1;
	else
		return 0;
}

/*!
 ************************************************************************
 * \brief
 *    returns true, if picture is short term reference picture
 *
 ************************************************************************
 */
static inline int is_short_ref(struct StorablePicture *s)
{
#ifdef ERROR_CHECK
	return (s &&
		(s->used_for_reference) && (!(s->is_long_term)));
#else
	return (s->used_for_reference) && (!(s->is_long_term));
#endif
}


/*!
 ************************************************************************
 * \brief
 *    returns true, if picture is long term reference picture
 *
 ************************************************************************
 */
static inline int is_long_ref(struct StorablePicture *s)
{
#ifdef ERROR_CHECK
	return (s &&
		s->used_for_reference) && (s->is_long_term);
#else
	return (s->used_for_reference) && (s->is_long_term);
#endif
}

/*!
 ************************************************************************
 * \brief
 *    Initialize reference lists for a P Slice
 *
 ************************************************************************
 */
/*!
 ************************************************************************
 * \brief
 *    Generates a alternating field list from a given FrameStore list
 *
 ************************************************************************
 */
static void gen_pic_list_from_frame_list(enum PictureStructure currStructure,
		struct FrameStore **fs_list, int list_idx,
		struct StorablePicture **list,
		char *list_size, int long_term)
{
	int top_idx = 0;
	int bot_idx = 0;

	int (*is_ref)(struct StorablePicture *s) = (long_term) ? is_long_ref :
			is_short_ref;


	if (currStructure == TOP_FIELD) {
		while ((top_idx < list_idx) || (bot_idx < list_idx)) {
			for (; top_idx < list_idx; top_idx++) {
				if (fs_list[top_idx]->is_used & 1) {
					if (is_ref(fs_list[top_idx]->
						top_field)) {
						/* short term ref pic */
						list[(short) *list_size] =
						fs_list[top_idx]->top_field;
						(*list_size)++;
						top_idx++;
						break;
					}
				}
			}
			for (; bot_idx < list_idx; bot_idx++) {
				if (fs_list[bot_idx]->is_used & 2) {
					if (is_ref(fs_list[bot_idx]->
						bottom_field)) {
						/* short term ref pic */
						list[(short) *list_size] =
						fs_list[bot_idx]->bottom_field;
						(*list_size)++;
						bot_idx++;
						break;
					}
				}
			}
		}
	}
	if (currStructure == BOTTOM_FIELD) {
		while ((top_idx < list_idx) || (bot_idx < list_idx)) {
			for (; bot_idx < list_idx; bot_idx++) {
				if (fs_list[bot_idx]->is_used & 2) {
					if (is_ref(fs_list[bot_idx]->
						bottom_field)) {
						/* short term ref pic */
						list[(short) *list_size] =
						fs_list[bot_idx]->bottom_field;
						(*list_size)++;
						bot_idx++;
						break;
					}
				}
			}
			for (; top_idx < list_idx; top_idx++) {
				if (fs_list[top_idx]->is_used & 1) {
					if (is_ref(fs_list[top_idx]->
						top_field)) {
						/* short term ref pic */
						list[(short) *list_size] =
						fs_list[top_idx]->top_field;
						(*list_size)++;
						top_idx++;
						break;
					}
				}
			}
		}
	}
}

static void init_lists_p_slice(struct Slice *currSlice)
{
	struct VideoParameters *p_Vid = currSlice->p_Vid;
	struct DecodedPictureBuffer *p_Dpb = currSlice->p_Dpb;
	struct h264_dpb_stru *p_H264_Dpb = container_of(p_Dpb,
				struct h264_dpb_stru, mDPB);

	unsigned int i;

	int list0idx = 0;
	int listltidx = 0;

	struct FrameStore **fs_list0;
	struct FrameStore **fs_listlt;

#if (MVC_EXTENSION_ENABLE)
	currSlice->listinterviewidx0 = 0;
	currSlice->listinterviewidx1 = 0;
#endif

	if (currSlice->structure == FRAME) {
		for (i = 0; i < p_Dpb->ref_frames_in_buffer; i++) {
			if (p_Dpb->fs_ref[i]->is_used == 3) {
				if ((p_Dpb->fs_ref[i]->frame->
					used_for_reference) &&
				    (!p_Dpb->fs_ref[i]->frame->
					is_long_term)) {
					currSlice->listX[0][list0idx++] =
					p_Dpb->fs_ref[i]->frame;
				}
			}
		}
		/* order list 0 by PicNum */
		qsort((void *)currSlice->listX[0], list0idx,
			sizeof(struct StorablePicture *),
			compare_pic_by_pic_num_desc);
		currSlice->listXsize[0] = (char) list0idx;
		CHECK_VALID(currSlice->listXsize[0], 0);
		if (h264_debug_flag & PRINT_FLAG_DPB_DETAIL) {
			dpb_print(p_H264_Dpb->decoder_index,
				PRINT_FLAG_DPB_DETAIL,
				  "listX[0] (PicNum): ");
			for (i = 0; i < list0idx; i++) {
				dpb_print(p_H264_Dpb->decoder_index,
					PRINT_FLAG_DPB_DETAIL, "%d  ",
					currSlice->listX[0][i]->pic_num);
			}
			dpb_print(p_H264_Dpb->decoder_index,
				PRINT_FLAG_DPB_DETAIL, "\n");
		}
		/* long term handling */
		for (i = 0; i < p_Dpb->ltref_frames_in_buffer; i++) {
			if (p_Dpb->fs_ltref[i]->is_used == 3) {
				if (p_Dpb->fs_ltref[i]->frame->is_long_term) {
					currSlice->listX[0][list0idx++] =
						p_Dpb->fs_ltref[i]->frame;
				}
			}
		}
		qsort((void *)&currSlice->listX[0][
			(short) currSlice->listXsize[0]],
			list0idx - currSlice->listXsize[0],
			sizeof(struct StorablePicture *),
			compare_pic_by_lt_pic_num_asc);
		currSlice->listXsize[0] = (char) list0idx;
		CHECK_VALID(currSlice->listXsize[0], 0);
	} else {
#if 0
		fs_list0 = calloc(p_Dpb->size, sizeof(struct FrameStore *));
		if (NULL == fs_list0)
			no_mem_exit("init_lists: fs_list0");
		fs_listlt = calloc(p_Dpb->size, sizeof(struct FrameStore *));
		if (NULL == fs_listlt)
			no_mem_exit("init_lists: fs_listlt");
#else
		fs_list0 = &(p_Dpb->fs_list0[0]);
		fs_listlt = &(p_Dpb->fs_listlt[0]);
#endif
		for (i = 0; i < p_Dpb->ref_frames_in_buffer; i++) {
			if (p_Dpb->fs_ref[i]->is_reference)
				fs_list0[list0idx++] = p_Dpb->fs_ref[i];
		}

		qsort((void *)fs_list0, list0idx, sizeof(struct FrameStore *),
		      compare_fs_by_frame_num_desc);

		dpb_print(p_H264_Dpb->decoder_index, PRINT_FLAG_DPB_DETAIL,
			  "fs_list0 (FrameNum): ");
		for (i = 0; i < list0idx; i++) {
			dpb_print(p_H264_Dpb->decoder_index,
				  PRINT_FLAG_DPB_DETAIL, "%d  ",
				  fs_list0[i]->frame_num_wrap);
		}
		dpb_print(p_H264_Dpb->decoder_index, PRINT_FLAG_DPB_DETAIL,
			  "\n");

		currSlice->listXsize[0] = 0;
		gen_pic_list_from_frame_list(currSlice->structure, fs_list0,
						list0idx, currSlice->listX[0],
						&currSlice->listXsize[0], 0);

		dpb_print(p_H264_Dpb->decoder_index, PRINT_FLAG_DPB_DETAIL,
			  "listX[0] (PicNum): ");
		for (i = 0; i < currSlice->listXsize[0]; i++) {
			dpb_print(p_H264_Dpb->decoder_index,
				PRINT_FLAG_DPB_DETAIL, "%d  ",
				currSlice->listX[0][i]->pic_num);
		}
		dpb_print(p_H264_Dpb->decoder_index, PRINT_FLAG_DPB_DETAIL,
			  "\n");

		/* long term handling */
		for (i = 0; i < p_Dpb->ltref_frames_in_buffer; i++)
			fs_listlt[listltidx++] = p_Dpb->fs_ltref[i];

		qsort((void *)fs_listlt, listltidx, sizeof(struct FrameStore *),
		      compare_fs_by_lt_pic_idx_asc);

		gen_pic_list_from_frame_list(currSlice->structure, fs_listlt,
					listltidx, currSlice->listX[0],
					&currSlice->listXsize[0], 1);

		/* free(fs_list0); */
		/* free(fs_listlt); */
	}
	currSlice->listXsize[1] = 0;


	/* set max size */
	currSlice->listXsize[0] = (char) imin(currSlice->listXsize[0],
			currSlice->num_ref_idx_active[LIST_0]);
	currSlice->listXsize[1] = (char) imin(currSlice->listXsize[1],
			currSlice->num_ref_idx_active[LIST_1]);
	CHECK_VALID(currSlice->listXsize[0], 0);
	CHECK_VALID(currSlice->listXsize[1], 1);

	/* set the unused list entries to NULL */
	for (i = currSlice->listXsize[0]; i < (MAX_LIST_SIZE); i++)
		currSlice->listX[0][i] = p_Vid->no_reference_picture;
	for (i = currSlice->listXsize[1]; i < (MAX_LIST_SIZE); i++)
		currSlice->listX[1][i] = p_Vid->no_reference_picture;

#if PRINTREFLIST
#if (MVC_EXTENSION_ENABLE)
	/* print out for h264_debug_flag purpose */
	if ((p_Vid->profile_idc == MVC_HIGH ||
		p_Vid->profile_idc == STEREO_HIGH) &&
	    currSlice->current_slice_nr == 0) {
		if (currSlice->listXsize[0] > 0) {
			dpb_print(p_H264_Dpb->decoder_index,
				PRINT_FLAG_DPB_DETAIL, "\n");
			dpb_print(p_H264_Dpb->decoder_index,
				PRINT_FLAG_DPB_DETAIL,
				  " ** (CurViewID:%d %d) %s Ref Pic List 0 ****\n",
				currSlice->view_id,
				currSlice->ThisPOC,
				currSlice->structure == FRAME ? "FRM" :
				  (currSlice->structure == TOP_FIELD ?
					"TOP" : "BOT"));
			for (i = 0; i < (unsigned int)(currSlice->
				listXsize[0]); i++) { /* ref list 0 */
				dpb_print(p_H264_Dpb->decoder_index,
				PRINT_FLAG_DPB_DETAIL,
				"   %2d -> POC: %4d PicNum: %4d ViewID: %d\n",
				i,
				currSlice->listX[0][i]->poc,
				currSlice->listX[0][i]->pic_num,
				currSlice->listX[0][i]->view_id);
			}
		}
	}
#endif
#endif
}


/*!
 ************************************************************************
 * \brief
 *    Initialize reference lists
 *
 ************************************************************************
 */
static void init_mbaff_lists(struct VideoParameters *p_Vid,
			     struct Slice *currSlice)
{
	unsigned j;
	int i;

	for (i = 2; i < 6; i++) {
		for (j = 0; j < MAX_LIST_SIZE; j++)
			currSlice->listX[i][j] = p_Vid->no_reference_picture;
		currSlice->listXsize[i] = 0;
	}

	for (i = 0; i < currSlice->listXsize[0]; i++) {
#ifdef ERROR_CHECK
		if (currSlice->listX[0][i] == NULL) {
			pr_info(
			"error currSlice->listX[0][%d] is NULL\r\n", i);
			break;
		}
#endif
		currSlice->listX[2][2 * i] =
			currSlice->listX[0][i]->top_field;
		currSlice->listX[2][2 * i + 1] =
			currSlice->listX[0][i]->bottom_field;
		currSlice->listX[4][2 * i] =
			currSlice->listX[0][i]->bottom_field;
		currSlice->listX[4][2 * i + 1] =
			currSlice->listX[0][i]->top_field;
	}
	currSlice->listXsize[2] = currSlice->listXsize[4] =
		currSlice->listXsize[0] * 2;

	for (i = 0; i < currSlice->listXsize[1]; i++) {
#ifdef ERROR_CHECK
		if (currSlice->listX[1][i] == NULL) {
			pr_info(
			"error currSlice->listX[1][%d] is NULL\r\n", i);
			break;
		}
#endif
		currSlice->listX[3][2 * i] =
			currSlice->listX[1][i]->top_field;
		currSlice->listX[3][2 * i + 1] =
			currSlice->listX[1][i]->bottom_field;
		currSlice->listX[5][2 * i] =
			currSlice->listX[1][i]->bottom_field;
		currSlice->listX[5][2 * i + 1] =
			currSlice->listX[1][i]->top_field;
	}
	currSlice->listXsize[3] = currSlice->listXsize[5] =
		currSlice->listXsize[1] * 2;
}



static void init_lists_i_slice(struct Slice *currSlice)
{

#if (MVC_EXTENSION_ENABLE)
	currSlice->listinterviewidx0 = 0;
	currSlice->listinterviewidx1 = 0;
#endif

	currSlice->listXsize[0] = 0;
	currSlice->listXsize[1] = 0;
}

static void init_lists_b_slice(struct Slice *currSlice)
{
	struct VideoParameters *p_Vid = currSlice->p_Vid;
	struct DecodedPictureBuffer *p_Dpb = currSlice->p_Dpb;
	struct h264_dpb_stru *p_H264_Dpb = container_of(p_Dpb,
		struct h264_dpb_stru, mDPB);

	unsigned int i;
	int j;

	int list0idx = 0;
	int list0idx_1 = 0;
	int listltidx = 0;

	struct FrameStore **fs_list0;
	struct FrameStore **fs_list1;
	struct FrameStore **fs_listlt;

#if (MVC_EXTENSION_ENABLE)
	currSlice->listinterviewidx0 = 0;
	currSlice->listinterviewidx1 = 0;
#endif

	{
		/* B-Slice */
		if (currSlice->structure == FRAME) {
			for (i = 0; i < p_Dpb->ref_frames_in_buffer; i++) {
				if ((p_Dpb->fs_ref[i]->is_used == 3) &&
					((p_Dpb->fs_ref[i]->frame->
					used_for_reference) &&
					(!p_Dpb->fs_ref[i]->frame->
					is_long_term)) &&
					(currSlice->framepoc >=
					p_Dpb->fs_ref[i]->frame->poc)) {
					/* !KS use >= for error
						concealment */
					currSlice->listX[0][list0idx++] =
						p_Dpb->fs_ref[i]->frame;
				}
			}
			qsort((void *)currSlice->listX[0], list0idx,
				sizeof(struct StorablePicture *),
				compare_pic_by_poc_desc);

			/* get the backward reference picture
			   (POC>current POC) in list0; */
			list0idx_1 = list0idx;
			for (i = 0; i < p_Dpb->ref_frames_in_buffer; i++) {
				if ((p_Dpb->fs_ref[i]->is_used == 3) &&
					((p_Dpb->fs_ref[i]->frame->
					used_for_reference) &&
					(!p_Dpb->fs_ref[i]->frame->
					is_long_term)) &&
					(currSlice->framepoc <
					p_Dpb->fs_ref[i]->frame->poc)) {
					currSlice->
					listX[0][list0idx++] =
						p_Dpb->fs_ref[i]->frame;
				}
			}
			qsort((void *)&currSlice->listX[0][list0idx_1],
				list0idx - list0idx_1,
				sizeof(struct StorablePicture *),
				compare_pic_by_poc_asc);

			for (j = 0; j < list0idx_1; j++) {
				currSlice->
				listX[1][list0idx - list0idx_1 + j] =
					currSlice->listX[0][j];
			}
			for (j = list0idx_1; j < list0idx; j++) {
				currSlice->listX[1][j - list0idx_1] =
					currSlice->listX[0][j];
			}

			currSlice->listXsize[0] = currSlice->listXsize[1] =
				(char) list0idx;
			CHECK_VALID(currSlice->listXsize[0], 0);
			CHECK_VALID(currSlice->listXsize[1], 1);

			dpb_print(p_H264_Dpb->decoder_index,
				PRINT_FLAG_DPB_DETAIL,
				"listX[0] (PicNum): ");
			for (i = 0; i < currSlice->listXsize[0]; i++) {
				dpb_print(p_H264_Dpb->decoder_index,
				PRINT_FLAG_DPB_DETAIL, "%d  ",
				currSlice->listX[0][i]->pic_num);
			}
			dpb_print(p_H264_Dpb->decoder_index,
				PRINT_FLAG_DPB_DETAIL, "\n");
			dpb_print(p_H264_Dpb->decoder_index,
				PRINT_FLAG_DPB_DETAIL,
				"listX[1] (PicNum): ");
			for (i = 0; i < currSlice->listXsize[1]; i++) {
				dpb_print(p_H264_Dpb->decoder_index,
					PRINT_FLAG_DPB_DETAIL, "%d  ",
					currSlice->listX[1][i]->pic_num);
			}
			dpb_print(p_H264_Dpb->decoder_index,
				PRINT_FLAG_DPB_DETAIL, "\n");
			/* dpb_print(p_H264_Dpb->decoder_index,
				PRINT_FLAG_DPB_DETAIL,
				"currSlice->listX[0] currPoc=%d (Poc): ",
				p_Vid->framepoc);
			   for (i=0; i<currSlice->listXsize[0]; i++) {
				dpb_print(p_H264_Dpb->decoder_index,
					PRINT_FLAG_DPB_DETAIL,
					"%d  ", currSlice->listX[0][i]->poc);
			   }
			   dpb_print(p_H264_Dpb->decoder_index,
				PRINT_FLAG_DPB_DETAIL, "\n");
			   dpb_print(p_H264_Dpb->decoder_index,
				PRINT_FLAG_DPB_DETAIL,
				"currSlice->listX[1] currPoc=%d (Poc): ",
				p_Vid->framepoc);
			   for (i=0; i<currSlice->listXsize[1]; i++) {
				dpb_print(p_H264_Dpb->decoder_index,
				PRINT_FLAG_DPB_DETAIL,
				"%d  ",
				currSlice->listX[1][i]->poc);
			   }
			   dpb_print(p_H264_Dpb->decoder_index,
				PRINT_FLAG_DPB_DETAIL, "\n"); */

			/* long term handling */
			for (i = 0; i < p_Dpb->ltref_frames_in_buffer; i++) {
				if (p_Dpb->fs_ltref[i]->is_used == 3) {
					if (p_Dpb->fs_ltref[i]->frame->
						is_long_term) {
						currSlice->
						listX[0][list0idx] =
						p_Dpb->fs_ltref[i]->frame;
						currSlice->
						listX[1][list0idx++] =
						p_Dpb->fs_ltref[i]->frame;
					}
				}
			}
			qsort((void *)&currSlice->
				listX[0][(short) currSlice->listXsize[0]],
				list0idx - currSlice->listXsize[0],
				sizeof(struct StorablePicture *),
				compare_pic_by_lt_pic_num_asc);
			qsort((void *)&currSlice->
				listX[1][(short) currSlice->listXsize[0]],
				list0idx - currSlice->listXsize[0],
				sizeof(struct StorablePicture *),
				compare_pic_by_lt_pic_num_asc);
			currSlice->listXsize[0] = currSlice->listXsize[1] =
				(char) list0idx;
			CHECK_VALID(currSlice->listXsize[0], 0);
			CHECK_VALID(currSlice->listXsize[1], 1);
		} else {
#if 0
			fs_list0 = calloc(p_Dpb->size,
				sizeof(struct FrameStore *));
			if (NULL == fs_list0)
				no_mem_exit("init_lists: fs_list0");
			fs_list1 = calloc(p_Dpb->size,
				sizeof(struct FrameStore *));
			if (NULL == fs_list1)
				no_mem_exit("init_lists: fs_list1");
			fs_listlt = calloc(p_Dpb->size,
				sizeof(struct FrameStore *));
			if (NULL == fs_listlt)
				no_mem_exit("init_lists: fs_listlt");
#else
			fs_list0 = &(p_Dpb->fs_list0[0]);
			fs_list1 = &(p_Dpb->fs_list1[0]);
			fs_listlt = &(p_Dpb->fs_listlt[0]);

#endif
			currSlice->listXsize[0] = 0;
			currSlice->listXsize[1] = 1;

			for (i = 0; i < p_Dpb->ref_frames_in_buffer; i++) {
				if (p_Dpb->fs_ref[i]->is_used) {
					if (currSlice->ThisPOC >=
						p_Dpb->fs_ref[i]->poc) {
						fs_list0[list0idx++] =
							p_Dpb->fs_ref[i];
					}
				}
			}
			qsort((void *)fs_list0, list0idx,
				sizeof(struct FrameStore *),
				compare_fs_by_poc_desc);
			list0idx_1 = list0idx;
			for (i = 0; i < p_Dpb->ref_frames_in_buffer; i++) {
				if (p_Dpb->fs_ref[i]->is_used) {
					if (currSlice->ThisPOC <
						p_Dpb->fs_ref[i]->poc) {
						fs_list0[list0idx++] =
							p_Dpb->fs_ref[i];
					}
				}
			}
			qsort((void *)&fs_list0[list0idx_1],
				list0idx - list0idx_1,
				sizeof(struct FrameStore *),
				compare_fs_by_poc_asc);

			for (j = 0; j < list0idx_1; j++) {
				fs_list1[list0idx - list0idx_1 + j] =
				fs_list0[j];
			}
			for (j = list0idx_1; j < list0idx; j++)
				fs_list1[j - list0idx_1] = fs_list0[j];

			dpb_print(p_H264_Dpb->decoder_index,
				PRINT_FLAG_DPB_DETAIL,
				"fs_list0 currPoc=%d (Poc): ",
				currSlice->ThisPOC);
			for (i = 0; i < list0idx; i++) {
				dpb_print(p_H264_Dpb->decoder_index,
				PRINT_FLAG_DPB_DETAIL, "%d  ",
				fs_list0[i]->poc);
			}
			dpb_print(p_H264_Dpb->decoder_index,
				PRINT_FLAG_DPB_DETAIL, "\n");
			dpb_print(p_H264_Dpb->decoder_index,
				PRINT_FLAG_DPB_DETAIL,
				"fs_list1 currPoc=%d (Poc): ",
				currSlice->ThisPOC);
			for (i = 0; i < list0idx; i++) {
				dpb_print(p_H264_Dpb->decoder_index,
					PRINT_FLAG_DPB_DETAIL, "%d  ",
					fs_list1[i]->poc);
			}
			dpb_print(p_H264_Dpb->decoder_index,
					PRINT_FLAG_DPB_DETAIL, "\n");

			currSlice->listXsize[0] = 0;
			currSlice->listXsize[1] = 0;
			gen_pic_list_from_frame_list(currSlice->structure,
						fs_list0, list0idx,
						currSlice->listX[0],
						&currSlice->listXsize[0], 0);
			gen_pic_list_from_frame_list(currSlice->structure,
						fs_list1, list0idx,
						currSlice->listX[1],
						&currSlice->listXsize[1], 0);

			/* dpb_print(p_H264_Dpb->decoder_index,
				PRINT_FLAG_DPB_DETAIL,
				"currSlice->listX[0] currPoc=%d (Poc): ",
				p_Vid->framepoc);
			  for (i=0; i<currSlice->listXsize[0]; i++) {
				dpb_print(p_H264_Dpb->decoder_index,
				PRINT_FLAG_DPB_DETAIL, "%d  ",
				currSlice->listX[0][i]->poc);
			  }
			  dpb_print(p_H264_Dpb->decoder_index,
				PRINT_FLAG_DPB_DETAIL, "\n"); */
			/* dpb_print(p_H264_Dpb->decoder_index,
				PRINT_FLAG_DPB_DETAIL,
				"currSlice->listX[1] currPoc=%d (Poc): ",
				p_Vid->framepoc);
			  for (i=0; i<currSlice->listXsize[1]; i++) {
				dpb_print(p_H264_Dpb->decoder_index,
				PRINT_FLAG_DPB_DETAIL, "%d  ",
				currSlice->listX[1][i]->poc);
			  }
			  dpb_print(p_H264_Dpb->decoder_index,
				PRINT_FLAG_DPB_DETAIL,
				"\n"); */

			/* long term handling */
			for (i = 0; i < p_Dpb->ltref_frames_in_buffer; i++)
				fs_listlt[listltidx++] = p_Dpb->fs_ltref[i];

			qsort((void *)fs_listlt, listltidx,
				sizeof(struct FrameStore *),
				compare_fs_by_lt_pic_idx_asc);

			gen_pic_list_from_frame_list(currSlice->structure,
				fs_listlt, listltidx,
				currSlice->listX[0],
				&currSlice->listXsize[0], 1);
			gen_pic_list_from_frame_list(currSlice->structure,
				fs_listlt, listltidx,
				currSlice->listX[1],
				&currSlice->listXsize[1], 1);

			/* free(fs_list0); */
			/* free(fs_list1); */
			/* free(fs_listlt); */
		}
	}

	if ((currSlice->listXsize[0] == currSlice->listXsize[1]) &&
	    (currSlice->listXsize[0] > 1)) {
		/* check if lists are identical,
		if yes swap first two elements of currSlice->listX[1] */
		int diff = 0;
		for (j = 0; j < currSlice->listXsize[0]; j++) {
			if (currSlice->listX[0][j] !=
				currSlice->listX[1][j]) {
				diff = 1;
				break;
			}
		}
		if (!diff) {
			struct StorablePicture *tmp_s =
				currSlice->listX[1][0];
			currSlice->listX[1][0] = currSlice->listX[1][1];
			currSlice->listX[1][1] = tmp_s;
		}
	}

	/* set max size */
	currSlice->listXsize[0] = (char) imin(currSlice->listXsize[0],
		currSlice->num_ref_idx_active[LIST_0]);
	currSlice->listXsize[1] = (char) imin(currSlice->listXsize[1],
		currSlice->num_ref_idx_active[LIST_1]);
	CHECK_VALID(currSlice->listXsize[0], 0);
	CHECK_VALID(currSlice->listXsize[1], 1);

	/* set the unused list entries to NULL */
	for (i = currSlice->listXsize[0]; i < (MAX_LIST_SIZE); i++)
		currSlice->listX[0][i] = p_Vid->no_reference_picture;
	for (i = currSlice->listXsize[1]; i < (MAX_LIST_SIZE); i++)
		currSlice->listX[1][i] = p_Vid->no_reference_picture;

#if PRINTREFLIST
#if (MVC_EXTENSION_ENABLE)
	/* print out for h264_debug_flag purpose */
	if ((p_Vid->profile_idc == MVC_HIGH ||
	    p_Vid->profile_idc == STEREO_HIGH) &&
	    currSlice->current_slice_nr == 0) {
		if ((currSlice->listXsize[0] > 0) ||
		    (currSlice->listXsize[1] > 0))
			dpb_print(p_H264_Dpb->decoder_index,
				PRINT_FLAG_DPB_DETAIL, "\n");
		if (currSlice->listXsize[0] > 0) {
			dpb_print(p_H264_Dpb->decoder_index,
				PRINT_FLAG_DPB_DETAIL,
				" ** (CurViewID:%d %d) %s Ref Pic List 0 ****\n",
				currSlice->view_id,
				currSlice->ThisPOC,
				currSlice->structure == FRAME ? "FRM" :
				(currSlice->structure == TOP_FIELD ?
				"TOP" : "BOT"));
			for (i = 0; i < (unsigned int)(currSlice->
				listXsize[0]); i++) { /* ref list 0 */
				dpb_print(p_H264_Dpb->decoder_index,
					PRINT_FLAG_DPB_DETAIL,
					"   %2d -> POC: %4d PicNum: %4d ViewID: %d\n",
					i,
					currSlice->listX[0][i]->poc,
					currSlice->listX[0][i]->pic_num,
					currSlice->listX[0][i]->view_id);
			}
		}
		if (currSlice->listXsize[1] > 0) {
			dpb_print(p_H264_Dpb->decoder_index,
				PRINT_FLAG_DPB_DETAIL,
				" ** (CurViewID:%d %d) %s Ref Pic List 1 ****\n",
				currSlice->view_id,
				currSlice->ThisPOC,
				currSlice->structure == FRAME ? "FRM" :
				(currSlice->structure == TOP_FIELD ? "TOP" :
				"BOT"));
			for (i = 0; i < (unsigned int)(currSlice->
				listXsize[1]); i++) { /* ref list 1 */
				dpb_print(p_H264_Dpb->decoder_index,
					PRINT_FLAG_DPB_DETAIL,
					"   %2d -> POC: %4d PicNum: %4d	ViewID: %d\n",
					i,
					currSlice->listX[1][i]->poc,
					currSlice->listX[1][i]->pic_num,
					currSlice->listX[1][i]->view_id);
			}
		}
	}
#endif
#endif
}

static struct StorablePicture *get_short_term_pic(struct Slice *currSlice,
		struct DecodedPictureBuffer *p_Dpb, int picNum)
{
	unsigned i;

	for (i = 0; i < p_Dpb->ref_frames_in_buffer; i++) {
		if (currSlice->structure == FRAME) {
			if (p_Dpb->fs_ref[i]->is_reference == 3)
				if ((!p_Dpb->fs_ref[i]->frame->
					is_long_term) &&
				    (p_Dpb->fs_ref[i]->frame->
					pic_num == picNum))
					return p_Dpb->fs_ref[i]->frame;
		} else {
			if (p_Dpb->fs_ref[i]->is_reference & 1)
				if ((!p_Dpb->fs_ref[i]->top_field->
					is_long_term) &&
				    (p_Dpb->fs_ref[i]->top_field->
					pic_num == picNum))
					return p_Dpb->fs_ref[i]->top_field;
			if (p_Dpb->fs_ref[i]->is_reference & 2)
				if ((!p_Dpb->fs_ref[i]->bottom_field->
					is_long_term) &&
				    (p_Dpb->fs_ref[i]->bottom_field->
					pic_num == picNum))
					return p_Dpb->fs_ref[i]->bottom_field;
		}
	}

	return currSlice->p_Vid->no_reference_picture;
}


static void reorder_short_term(struct Slice *currSlice, int cur_list,
				int num_ref_idx_lX_active_minus1,
				int picNumLX, int *refIdxLX)
{
	struct h264_dpb_stru *p_H264_Dpb = container_of(currSlice->p_Vid,
					struct h264_dpb_stru, mVideo);

	struct StorablePicture **RefPicListX = currSlice->listX[cur_list];
	int cIdx, nIdx;

	struct StorablePicture *picLX;

	picLX = get_short_term_pic(currSlice, currSlice->p_Dpb, picNumLX);

	for (cIdx = num_ref_idx_lX_active_minus1 + 1; cIdx > *refIdxLX;
		cIdx--) {
		dpb_print(p_H264_Dpb->decoder_index, PRINT_FLAG_DPB_DETAIL,
			"%s: RefPicListX[ %d ] = RefPicListX[ %d ]\n",
			__func__, cIdx, cIdx - 1);
		RefPicListX[cIdx] = RefPicListX[cIdx - 1];
	}

	dpb_print(p_H264_Dpb->decoder_index, PRINT_FLAG_DPB_DETAIL,
		"%s: RefPicListX[ %d ] = pic %x (%d)\n", __func__,
		*refIdxLX, picLX, picNumLX);

	RefPicListX[(*refIdxLX)++] = picLX;

	nIdx = *refIdxLX;

	for (cIdx = *refIdxLX; cIdx <= num_ref_idx_lX_active_minus1 + 1;
		cIdx++) {
		if (RefPicListX[cIdx])
			if ((RefPicListX[cIdx]->is_long_term) ||
			    (RefPicListX[cIdx]->pic_num != picNumLX)) {
				dpb_print(p_H264_Dpb->decoder_index,
					PRINT_FLAG_DPB_DETAIL,
					"%s: RefPicListX[ %d ] = RefPicListX[ %d ]\n",
					__func__, nIdx, cIdx);
				RefPicListX[nIdx++] = RefPicListX[cIdx];
			}
	}
}


static struct StorablePicture *get_long_term_pic(struct Slice *currSlice,
		struct DecodedPictureBuffer *p_Dpb, int LongtermPicNum)
{
	unsigned int i;

	for (i = 0; i < p_Dpb->ltref_frames_in_buffer; i++) {
		if (currSlice->structure == FRAME) {
			if (p_Dpb->fs_ltref[i]->is_reference == 3)
				if ((p_Dpb->fs_ltref[i]->frame->
					is_long_term) &&
				    (p_Dpb->fs_ltref[i]->frame->
					long_term_pic_num ==
					LongtermPicNum))
					return p_Dpb->fs_ltref[i]->frame;
		} else {
			if (p_Dpb->fs_ltref[i]->is_reference & 1)
				if ((p_Dpb->fs_ltref[i]->top_field->
					is_long_term) &&
				    (p_Dpb->fs_ltref[i]->top_field->
					long_term_pic_num == LongtermPicNum))
					return p_Dpb->fs_ltref[i]->top_field;
			if (p_Dpb->fs_ltref[i]->is_reference & 2)
				if ((p_Dpb->fs_ltref[i]->bottom_field->
					is_long_term) &&
				    (p_Dpb->fs_ltref[i]->bottom_field->
					long_term_pic_num ==
					LongtermPicNum))
					return p_Dpb->fs_ltref[i]->
						bottom_field;
		}
	}
	return NULL;
}

/*!
 ************************************************************************
 * \brief
 *    Reordering process for long-term reference pictures
 *
 ************************************************************************
 */
static void reorder_long_term(struct Slice *currSlice,
				struct StorablePicture **RefPicListX,
				int num_ref_idx_lX_active_minus1,
			      int LongTermPicNum, int *refIdxLX)
{
	int cIdx, nIdx;

	struct StorablePicture *picLX;

	picLX = get_long_term_pic(currSlice, currSlice->p_Dpb, LongTermPicNum);

	for (cIdx = num_ref_idx_lX_active_minus1 + 1; cIdx > *refIdxLX; cIdx--)
		RefPicListX[cIdx] = RefPicListX[cIdx - 1];

	RefPicListX[(*refIdxLX)++] = picLX;

	nIdx = *refIdxLX;

	for (cIdx = *refIdxLX; cIdx <= num_ref_idx_lX_active_minus1 + 1;
		cIdx++) {
		if (RefPicListX[cIdx]) {
			if ((!RefPicListX[cIdx]->is_long_term) ||
			    (RefPicListX[cIdx]->long_term_pic_num !=
				LongTermPicNum))
				RefPicListX[nIdx++] = RefPicListX[cIdx];
		}
	}
}

static void reorder_ref_pic_list(struct Slice *currSlice, int cur_list)
{
	int *modification_of_pic_nums_idc =
		currSlice->modification_of_pic_nums_idc[cur_list];
	int *abs_diff_pic_num_minus1 =
		currSlice->abs_diff_pic_num_minus1[cur_list];
	int *long_term_pic_idx = currSlice->long_term_pic_idx[cur_list];
	int num_ref_idx_lX_active_minus1 =
		currSlice->num_ref_idx_active[cur_list] - 1;

	struct VideoParameters *p_Vid = currSlice->p_Vid;
	int i;

	int maxPicNum, currPicNum, picNumLXNoWrap, picNumLXPred, picNumLX;
	int refIdxLX = 0;

	if (currSlice->structure == FRAME) {
		maxPicNum  = p_Vid->max_frame_num;
		currPicNum = currSlice->frame_num;
	} else {
		maxPicNum  = 2 * p_Vid->max_frame_num;
		currPicNum = 2 * currSlice->frame_num + 1;
	}

	picNumLXPred = currPicNum;

	for (i = 0;  i < REORDERING_COMMAND_MAX_SIZE &&
		modification_of_pic_nums_idc[i] != 3; i++) {
		if (modification_of_pic_nums_idc[i] > 3) {
			struct h264_dpb_stru *p_H264_Dpb =
			container_of(p_Vid, struct h264_dpb_stru, mVideo);
			dpb_print(p_H264_Dpb->decoder_index,
				PRINT_FLAG_ERROR,
				"error, Invalid modification_of_pic_nums_idc command\n");
			/*h264_debug_flag = 0x1f;*/
			break;
		}
		if (modification_of_pic_nums_idc[i] < 2) {
			if (modification_of_pic_nums_idc[i] == 0) {
				if (picNumLXPred - (abs_diff_pic_num_minus1[i]
					+ 1) < 0)
					picNumLXNoWrap = picNumLXPred -
					(abs_diff_pic_num_minus1[i] + 1) +
					maxPicNum;
				else
					picNumLXNoWrap = picNumLXPred -
					(abs_diff_pic_num_minus1[i] + 1);
			} else { /* (modification_of_pic_nums_idc[i] == 1) */
				if (picNumLXPred + (abs_diff_pic_num_minus1[i]
					+ 1)  >=  maxPicNum)
					picNumLXNoWrap = picNumLXPred +
					(abs_diff_pic_num_minus1[i] + 1) -
					maxPicNum;
				else
					picNumLXNoWrap = picNumLXPred +
					(abs_diff_pic_num_minus1[i] + 1);
			}
			picNumLXPred = picNumLXNoWrap;

			if (picNumLXNoWrap > currPicNum)
				picNumLX = picNumLXNoWrap - maxPicNum;
			else
				picNumLX = picNumLXNoWrap;

#if (MVC_EXTENSION_ENABLE)
			reorder_short_term(currSlice, cur_list,
					num_ref_idx_lX_active_minus1, picNumLX,
					&refIdxLX, -1);
#else
			reorder_short_term(currSlice, cur_list,
					num_ref_idx_lX_active_minus1, picNumLX,
					&refIdxLX);
#endif
		} else { /* (modification_of_pic_nums_idc[i] == 2) */
#if (MVC_EXTENSION_ENABLE)
			reorder_long_term(currSlice, currSlice->listX[cur_list],
					num_ref_idx_lX_active_minus1,
					long_term_pic_idx[i], &refIdxLX, -1);
#else
			reorder_long_term(currSlice, currSlice->listX[cur_list],
					num_ref_idx_lX_active_minus1,
					long_term_pic_idx[i], &refIdxLX);
#endif
		}

	}
	/* that's a definition */
	currSlice->listXsize[cur_list] =
		(char)(num_ref_idx_lX_active_minus1 + 1);
}


static void reorder_lists(struct Slice *currSlice)
{
	struct VideoParameters *p_Vid = currSlice->p_Vid;
	struct h264_dpb_stru *p_H264_Dpb = container_of(p_Vid,
		struct h264_dpb_stru, mVideo);
	int i;
	if ((currSlice->slice_type != I_SLICE) &&
		(currSlice->slice_type != SI_SLICE)) {
		if (currSlice->ref_pic_list_reordering_flag[LIST_0])
			reorder_ref_pic_list(currSlice, LIST_0);
		if (p_Vid->no_reference_picture ==
		    currSlice->
			listX[0][currSlice->num_ref_idx_active[LIST_0] - 1]) {
			if (p_Vid->non_conforming_stream)
				dpb_print(p_H264_Dpb->decoder_index,
					PRINT_FLAG_DPB_DETAIL,
					"RefPicList0[ %d ] is equal to 'no reference picture'\n",
					currSlice->
					num_ref_idx_active[LIST_0] - 1);
			else
				dpb_print(p_H264_Dpb->decoder_index,
					PRINT_FLAG_DPB_DETAIL,
					"RefPicList0 [ num_ref_idx_l0_active_minus1 ] is equal to 'no reference picture', invalid bitstream %d\n",
					500);
		}
		/* that's a definition */
		currSlice->listXsize[0] =
			(char)currSlice->num_ref_idx_active[LIST_0];
		CHECK_VALID(currSlice->listXsize[0], 0);
		if (h264_debug_flag & PRINT_FLAG_DPB_DETAIL) {
			dpb_print(p_H264_Dpb->decoder_index,
				PRINT_FLAG_DPB_DETAIL,
				"listX[0] reorder (PicNum): ");
			for (i = 0; i < currSlice->listXsize[0]; i++) {
				dpb_print(p_H264_Dpb->decoder_index,
					PRINT_FLAG_DPB_DETAIL, "%d  ",
					currSlice->listX[0][i]->pic_num);
			}
			dpb_print(p_H264_Dpb->decoder_index,
				PRINT_FLAG_DPB_DETAIL, "\n");
		}
	}

	if (currSlice->slice_type == B_SLICE) {
		if (currSlice->ref_pic_list_reordering_flag[LIST_1])
			reorder_ref_pic_list(currSlice, LIST_1);
		if (p_Vid->no_reference_picture ==
		    currSlice->listX[1][currSlice->
			num_ref_idx_active[LIST_1] - 1]) {
			if (p_Vid->non_conforming_stream)
				dpb_print(p_H264_Dpb->decoder_index,
					PRINT_FLAG_DPB_DETAIL,
					"RefPicList1[ %d ] is equal to 'no reference picture'\n",
					currSlice->
					num_ref_idx_active[LIST_1] - 1);
			else
				dpb_print(p_H264_Dpb->decoder_index,
					PRINT_FLAG_DPB_DETAIL,
					"RefPicList1 [ num_ref_idx_l1_active_minus1 ] is equal to 'no reference picture', invalid bitstream %d\n",
					500);
		}
		/* that's a definition */
		currSlice->listXsize[1] =
			(char)currSlice->num_ref_idx_active[LIST_1];
		if (h264_debug_flag & PRINT_FLAG_DPB_DETAIL) {
			dpb_print(p_H264_Dpb->decoder_index,
			PRINT_FLAG_DPB_DETAIL,
				  "listX[1] reorder (PicNum): ");
			for (i = 0; i < currSlice->listXsize[1]; i++) {
				dpb_print(p_H264_Dpb->decoder_index,
					PRINT_FLAG_DPB_DETAIL, "%d  ",
					currSlice->listX[1][i]->pic_num);
			}
			dpb_print(p_H264_Dpb->decoder_index,
				PRINT_FLAG_DPB_DETAIL, "\n");
		}
	}

	/* free_ref_pic_list_reordering_buffer(currSlice); */

	if (currSlice->slice_type == P_SLICE) {
#if PRINTREFLIST
		unsigned int i;
#if (MVC_EXTENSION_ENABLE)
		/* print out for h264_debug_flag purpose */
		if ((p_Vid->profile_idc == MVC_HIGH ||
			p_Vid->profile_idc == STEREO_HIGH) &&
			currSlice->current_slice_nr == 0) {
			if (currSlice->listXsize[0] > 0
				&& (h264_debug_flag & PRINT_FLAG_DPB_DETAIL)) {
				dpb_print(p_H264_Dpb->decoder_index,
					PRINT_FLAG_DPB_DETAIL, "\n");
				dpb_print(p_H264_Dpb->decoder_index,
					PRINT_FLAG_DPB_DETAIL,
					  " ** (FinalViewID:%d) %s Ref Pic List 0 ****\n",
					currSlice->view_id,
					currSlice->structure == FRAME ?
					"FRM" :
					(currSlice->structure == TOP_FIELD ?
					"TOP" : "BOT"));
				for (i = 0; i < (unsigned int)(currSlice->
					listXsize[0]); i++) { /* ref list 0 */
					dpb_print(p_H264_Dpb->decoder_index,
						PRINT_FLAG_DPB_DETAIL,
						"   %2d -> POC: %4d PicNum: %4d ViewID: %d\n",
						i,
						currSlice->listX[0][i]->poc,
						currSlice->listX[0][i]->
							pic_num,
						currSlice->listX[0][i]->
							view_id);
				}
			}
		}
#endif
#endif
	} else if (currSlice->slice_type == B_SLICE) {
#if PRINTREFLIST
		unsigned int i;
#if (MVC_EXTENSION_ENABLE)
		/* print out for h264_debug_flag purpose */
		if ((p_Vid->profile_idc == MVC_HIGH ||
			p_Vid->profile_idc == STEREO_HIGH) &&
		    currSlice->current_slice_nr == 0) {
			if ((currSlice->listXsize[0] > 0) ||
				(currSlice->listXsize[1] > 0))
				dpb_print(p_H264_Dpb->decoder_index,
					PRINT_FLAG_DPB_DETAIL, "\n");
			if (currSlice->listXsize[0] > 0
				&& (h264_debug_flag & PRINT_FLAG_DPB_DETAIL)) {
				dpb_print(p_H264_Dpb->decoder_index,
					PRINT_FLAG_DPB_DETAIL,
					" ** (FinalViewID:%d) %s Ref Pic List 0 ****\n",
					currSlice->view_id,
					currSlice->structure == FRAME ?
					"FRM" :
					(currSlice->structure == TOP_FIELD ?
					"TOP" : "BOT"));
				for (i = 0; i < (unsigned int)(currSlice->
					listXsize[0]); i++) { /* ref list 0 */
					dpb_print(p_H264_Dpb->decoder_index,
						PRINT_FLAG_DPB_DETAIL,
						"   %2d -> POC: %4d PicNum: %4d ViewID: %d\n",
						i,
						currSlice->listX[0][i]->poc,
						currSlice->listX[0][i]->
							pic_num,
						currSlice->listX[0][i]->
							view_id);
				}
			}
			if (currSlice->listXsize[1] > 0
				 && (h264_debug_flag & PRINT_FLAG_DPB_DETAIL)) {
				dpb_print(p_H264_Dpb->decoder_index,
					PRINT_FLAG_DPB_DETAIL,
					" ** (FinalViewID:%d) %s Ref Pic List 1 ****\n",
					currSlice->view_id,
					currSlice->structure == FRAME ?
					"FRM" :
					(currSlice->structure == TOP_FIELD ?
					"TOP" : "BOT"));
				for (i = 0; i < (unsigned int)(currSlice->
					listXsize[1]); i++) { /* ref list 1 */
					dpb_print(p_H264_Dpb->decoder_index,
						PRINT_FLAG_DPB_DETAIL,
						"   %2d -> POC: %4d PicNum: %4d ViewID: %d\n",
						i,
						currSlice->listX[1][i]->poc,
						currSlice->listX[1][i]->
							pic_num,
						currSlice->listX[1][i]->
							view_id);
				}
			}
		}
#endif

#endif
	}
}

void init_colocate_buf(struct h264_dpb_stru *p_H264_Dpb, int count)
{
	p_H264_Dpb->colocated_buf_map = 0;
	p_H264_Dpb->colocated_buf_count = count;
}

int allocate_colocate_buf(struct h264_dpb_stru *p_H264_Dpb)
{
	int i;
	for (i = 0; i < p_H264_Dpb->colocated_buf_count; i++) {
		if (((p_H264_Dpb->colocated_buf_map >> i) & 0x1) == 0) {
			p_H264_Dpb->colocated_buf_map |= (1 << i);
			break;
		}
	}
	if (i == p_H264_Dpb->colocated_buf_count)
		i = -1;
	return i;
}

int release_colocate_buf(struct h264_dpb_stru *p_H264_Dpb, int index)
{
	if (index >= 0) {
		if (index >= p_H264_Dpb->colocated_buf_count) {
			dpb_print(p_H264_Dpb->decoder_index, PRINT_FLAG_ERROR,
				"%s error, index %d is bigger than buf count %d\n",
				__func__, index,
				p_H264_Dpb->colocated_buf_count);
		} else {
			if (((p_H264_Dpb->colocated_buf_map >>
				index) & 0x1) == 0x1) {
				p_H264_Dpb->colocated_buf_map &=
					(~(1 << index));
			} else {
				dpb_print(p_H264_Dpb->decoder_index,
					PRINT_FLAG_ERROR,
					"%s error, index %d is not allocated\n",
					__func__, index);
			}
		}
	}
	return 0;
}

void set_frame_output_flag(struct h264_dpb_stru *p_H264_Dpb, int index)
{
	struct DecodedPictureBuffer *p_Dpb = &p_H264_Dpb->mDPB;
	p_H264_Dpb->mFrameStore[index].is_output = 1;
	p_H264_Dpb->mFrameStore[index].pre_output = 0;
	dump_dpb(p_Dpb);
}

#if 0
void init_old_slice(OldSliceParams *p_old_slice)
{
	p_old_slice->field_pic_flag = 0;
	p_old_slice->pps_id         = INT_MAX;
	p_old_slice->frame_num      = INT_MAX;
	p_old_slice->nal_ref_idc    = INT_MAX;
	p_old_slice->idr_flag       = FALSE;

	p_old_slice->pic_oder_cnt_lsb          = UINT_MAX;
	p_old_slice->delta_pic_oder_cnt_bottom = INT_MAX;

	p_old_slice->delta_pic_order_cnt[0] = INT_MAX;
	p_old_slice->delta_pic_order_cnt[1] = INT_MAX;
}


void copy_slice_info(struct Slice *currSlice, OldSliceParams *p_old_slice)
{
	struct VideoParameters *p_Vid = currSlice->p_Vid;

	p_old_slice->pps_id         = currSlice->pic_parameter_set_id;
	p_old_slice->frame_num      = currSlice->frame_num;
	/* p_Vid->frame_num; */
	p_old_slice->field_pic_flag =
		currSlice->field_pic_flag;
	/* p_Vid->field_pic_flag; */

	if (currSlice->field_pic_flag)
		p_old_slice->bottom_field_flag = currSlice->bottom_field_flag;

	p_old_slice->nal_ref_idc = currSlice->nal_reference_idc;
	p_old_slice->idr_flag    = (byte) currSlice->idr_flag;

	if (currSlice->idr_flag)
		p_old_slice->idr_pic_id = currSlice->idr_pic_id;

	if (p_Vid->active_sps->pic_order_cnt_type == 0) {
		p_old_slice->pic_oder_cnt_lsb =
			currSlice->pic_order_cnt_lsb;
		p_old_slice->delta_pic_oder_cnt_bottom =
			currSlice->delta_pic_order_cnt_bottom;
	}

	if (p_Vid->active_sps->pic_order_cnt_type == 1) {
		p_old_slice->delta_pic_order_cnt[0] =
			currSlice->delta_pic_order_cnt[0];
		p_old_slice->delta_pic_order_cnt[1] =
			currSlice->delta_pic_order_cnt[1];
	}
#if (MVC_EXTENSION_ENABLE)
	p_old_slice->view_id = currSlice->view_id;
	p_old_slice->inter_view_flag = currSlice->inter_view_flag;
	p_old_slice->anchor_pic_flag = currSlice->anchor_pic_flag;
#endif
	p_old_slice->layer_id = currSlice->layer_id;
}

int is_new_picture(StorablePicture *dec_picture, struct Slice *currSlice,
		   OldSliceParams *p_old_slice)
{
	struct VideoParameters *p_Vid = currSlice->p_Vid;

	int result = 0;

	result |= (NULL == dec_picture);

	result |= (p_old_slice->pps_id != currSlice->pic_parameter_set_id);

	result |= (p_old_slice->frame_num != currSlice->frame_num);

	result |= (p_old_slice->field_pic_flag != currSlice->field_pic_flag);

	if (currSlice->field_pic_flag && p_old_slice->field_pic_flag) {
		result |= (p_old_slice->bottom_field_flag !=
				currSlice->bottom_field_flag);
	}

	result |= (p_old_slice->nal_ref_idc !=
			currSlice->nal_reference_idc) &&
		  ((p_old_slice->nal_ref_idc == 0) ||
			(currSlice->nal_reference_idc == 0));
	result |= (p_old_slice->idr_flag    != currSlice->idr_flag);

	if (currSlice->idr_flag && p_old_slice->idr_flag)
		result |= (p_old_slice->idr_pic_id != currSlice->idr_pic_id);

	if (p_Vid->active_sps->pic_order_cnt_type == 0) {
		result |= (p_old_slice->pic_oder_cnt_lsb !=
			   currSlice->pic_order_cnt_lsb);
		if (p_Vid->active_pps->
			bottom_field_pic_order_in_frame_present_flag  ==  1 &&
		    !currSlice->field_pic_flag) {
			result |= (p_old_slice->delta_pic_oder_cnt_bottom !=
				   currSlice->delta_pic_order_cnt_bottom);
		}
	}

	if (p_Vid->active_sps->pic_order_cnt_type == 1) {
		if (!p_Vid->active_sps->delta_pic_order_always_zero_flag) {
			result |= (p_old_slice->delta_pic_order_cnt[0] !=
				   currSlice->delta_pic_order_cnt[0]);
			if (p_Vid->active_pps->
			bottom_field_pic_order_in_frame_present_flag  ==  1 &&
			    !currSlice->field_pic_flag) {
				result |= (p_old_slice->
					delta_pic_order_cnt[1] !=
					currSlice->delta_pic_order_cnt[1]);
			}
		}
	}

#if (MVC_EXTENSION_ENABLE)
	result |= (currSlice->view_id != p_old_slice->view_id);
	result |= (currSlice->inter_view_flag != p_old_slice->inter_view_flag);
	result |= (currSlice->anchor_pic_flag != p_old_slice->anchor_pic_flag);
#endif
	result |= (currSlice->layer_id != p_old_slice->layer_id);
	return result;
}
#else
int is_new_picture(struct StorablePicture *dec_picture,
		   struct h264_dpb_stru *p_H264_Dpb,
		   struct OldSliceParams *p_old_slice)
{
	int ret = 0;
	if (p_H264_Dpb->dpb_param.l.data[FIRST_MB_IN_SLICE] == 0)
		ret = 1;
	return ret;
}

#endif

int remove_picture(struct h264_dpb_stru *p_H264_Dpb,
		   struct StorablePicture *pic)
{
	struct DecodedPictureBuffer *p_Dpb = &p_H264_Dpb->mDPB;
	if (p_Dpb->last_picture == NULL) {
		if (pic->colocated_buf_index >= 0) {
			release_colocate_buf(p_H264_Dpb,
			pic->colocated_buf_index);
			pic->colocated_buf_index = -1;
		}
		release_buf_spec_num(p_H264_Dpb->vdec, pic->buf_spec_num);
	}
	free_picture(p_H264_Dpb, pic);
	return 0;
}

static void check_frame_store_same_pic_num(struct DecodedPictureBuffer *p_Dpb,
	struct StorablePicture *p, struct Slice *currSlice)
{
	if (p_Dpb->last_picture) {
		if ((int)p_Dpb->last_picture->frame_num == p->pic_num) {
			if (((p->structure == TOP_FIELD) &&
				(p_Dpb->last_picture->is_used == 2)) ||
			    ((p->structure == BOTTOM_FIELD) &&
				(p_Dpb->last_picture->is_used == 1))) {
				if ((p->used_for_reference &&
					(p_Dpb->last_picture->
					is_orig_reference != 0)) ||
				    (!p->used_for_reference &&
					(p_Dpb->last_picture->
					is_orig_reference == 0))) {
					p->buf_spec_num =
						p_Dpb->last_picture->
						buf_spec_num;
					p->colocated_buf_index = p_Dpb->
						last_picture->
						colocated_buf_index;
					if (currSlice->structure ==
						TOP_FIELD) {
						p->bottom_poc =
							p_Dpb->last_picture->
							bottom_field->poc;
					} else {
						p->top_poc =
							p_Dpb->last_picture->
							top_field->poc;
					}
					p->frame_poc = imin(p->bottom_poc,
						p->top_poc);
				}
			}
		}
	}
}

int h264_slice_header_process(struct h264_dpb_stru *p_H264_Dpb)
{

	int new_pic_flag = 0;
	struct Slice *currSlice = &p_H264_Dpb->mSlice;
	struct VideoParameters *p_Vid = &p_H264_Dpb->mVideo;
#if 0
	new_pic_flag = is_new_picture(p_H264_Dpb->mVideo.dec_picture,
				      p_H264_Dpb,
				      &p_H264_Dpb->mVideo.old_slice);

	if (new_pic_flag) { /* new picture */
		if (p_H264_Dpb->mVideo.dec_picture) {
			store_picture_in_dpb(p_H264_Dpb,
				p_H264_Dpb->mVideo.dec_picture);
			/* dump_dpb(&p_H264_Dpb->mDPB); */
		}
	}
#else
	new_pic_flag = (p_H264_Dpb->mVideo.dec_picture == NULL);
#endif

	slice_prepare(p_H264_Dpb, &p_H264_Dpb->mDPB, &p_H264_Dpb->mVideo,
		      &p_H264_Dpb->mSPS, &p_H264_Dpb->mSlice);

	/* if (p_Vid->active_sps != sps) { */
	if (p_H264_Dpb->mDPB.init_done == 0) {
		/*init_global_buffers(p_Vid, 0);

		if (!p_Vid->no_output_of_prior_pics_flag)
		{
		    flush_dpb(p_Vid->p_Dpb_layer[0]);
		}
		init_dpb(p_Vid, p_Vid->p_Dpb_layer[0], 0);
		*/
		init_dpb(p_H264_Dpb, 0);
	}


	if (new_pic_flag) { /* new picture */
		dpb_print(p_H264_Dpb->decoder_index, PRINT_FLAG_DPB_DETAIL,
		"check frame_num gap: cur frame_num %d pre_frame_num %d max_frmae_num %d\r\n",
		currSlice->frame_num,
		p_Vid->pre_frame_num,
		p_Vid->max_frame_num);
		if (p_Vid->recovery_point == 0 &&
			currSlice->frame_num != p_Vid->pre_frame_num &&
			currSlice->frame_num !=
			(p_Vid->pre_frame_num + 1) % p_Vid->max_frame_num) {
			/*if (active_sps->
			gaps_in_frame_num_value_allowed_flag
			== 0) {
			  error("An unintentional
			  loss of pictures occurs! Exit\n",
			  100);
			}
			if (p_Vid->conceal_mode == 0)*/
			fill_frame_num_gap(p_Vid, currSlice);
		}

		if (currSlice->nal_reference_idc) {
			dpb_print(p_H264_Dpb->decoder_index,
				PRINT_FLAG_DPB_DETAIL,
			"nal_reference_idc not 0, set pre_frame_num(%d) to frame_num (%d)\n",
			p_Vid->pre_frame_num, currSlice->frame_num);
			p_Vid->pre_frame_num = currSlice->frame_num;
		}

		decode_poc(&p_H264_Dpb->mVideo, &p_H264_Dpb->mSlice);
		p_H264_Dpb->mVideo.dec_picture = get_new_pic(p_H264_Dpb,
						 p_H264_Dpb->mSlice.structure,
						/*p_Vid->width, p_Vid->height,
						  p_Vid->width_cr,
						  p_Vid->height_cr,*/
						 1);
		if (p_H264_Dpb->mVideo.dec_picture) {
			struct DecodedPictureBuffer *p_Dpb =
				&p_H264_Dpb->mDPB;
			struct StorablePicture *p =
				p_H264_Dpb->mVideo.dec_picture;
			init_picture(p_H264_Dpb, &p_H264_Dpb->mSlice,
				p_H264_Dpb->mVideo.dec_picture);
#if 1
			/* rain */
			p_H264_Dpb->mVideo.dec_picture->offset_delimiter_lo  =
			p_H264_Dpb->dpb_param.l.data[OFFSET_DELIMITER_LO];
			p_H264_Dpb->mVideo.dec_picture->offset_delimiter_hi  =
			p_H264_Dpb->dpb_param.l.data[OFFSET_DELIMITER_HI];

			p_H264_Dpb->mVideo.dec_picture->buf_spec_num  = -1;
			p_H264_Dpb->mVideo.dec_picture->
				colocated_buf_index = -1;
			update_pic_num(&p_H264_Dpb->mSlice);

			if ((currSlice->structure == TOP_FIELD) ||
			    (currSlice->structure == BOTTOM_FIELD)) {
				/* check for frame store with same
				   pic_number */
				check_frame_store_same_pic_num(p_Dpb, p,
					currSlice);
			}

			if (p_H264_Dpb->mVideo.dec_picture->buf_spec_num ==
				-1) {
				p_H264_Dpb->mVideo.dec_picture->buf_spec_num =
					get_free_buf_idx(p_H264_Dpb->vdec);
				if (p_H264_Dpb->mVideo.dec_picture->
					used_for_reference) {
					p_H264_Dpb->mVideo.dec_picture->
						colocated_buf_index =
						allocate_colocate_buf(
							p_H264_Dpb);
				}
			}
#endif
		}
	}

	if (p_H264_Dpb->mSlice.slice_type == P_SLICE)
		init_lists_p_slice(&p_H264_Dpb->mSlice);
	else if (p_H264_Dpb->mSlice.slice_type == B_SLICE)
		init_lists_b_slice(&p_H264_Dpb->mSlice);
	else
		init_lists_i_slice(&p_H264_Dpb->mSlice);

	reorder_lists(&p_H264_Dpb->mSlice);

	if (p_H264_Dpb->mSlice.structure == FRAME)
		init_mbaff_lists(&p_H264_Dpb->mVideo, &p_H264_Dpb->mSlice);

	if (new_pic_flag)
		return 1;

	return 0;
}
