/*
* Copyright (C) 2017 Amlogic, Inc. All rights reserved.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful, but WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
* FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
* more details.
*
* You should have received a copy of the GNU General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*
* Description:
*/
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/semaphore.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/kfifo.h>
#include <linux/kthread.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/utils/amstream.h>
#include <linux/amlogic/media/utils/vformat.h>
#include <linux/amlogic/media/frame_sync/ptsserv.h>
#include <linux/amlogic/media/canvas/canvas.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>
#include <linux/dma-mapping.h>
#include <linux/dma-contiguous.h>
#include <linux/slab.h>
#include <linux/amlogic/tee.h>
#include "../../../stream_input/amports/amports_priv.h"
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include "../utils/decoder_mmu_box.h"
#include "../utils/decoder_bmmu_box.h"
#include "avs2_global.h"

#include <linux/amlogic/media/utils/vdec_reg.h>
#include "../utils/vdec.h"
#include "../utils/amvdec.h"

#undef pr_info
#define pr_info printk

#define assert(chk_cond) {\
	if (!(chk_cond))\
		pr_info("error line %d\n", __LINE__);\
	while (!(chk_cond))\
		;\
}

int16_t get_param(uint16_t value, int8_t *print_info)
{
	if (is_avs2_print_param())
		pr_info("%s = %x\n", print_info, value);
	return (int16_t)value;
}

void readAlfCoeff(struct avs2_decoder *avs2_dec, struct ALFParam_s *Alfp)
{
	int32_t pos;
	union param_u *rpm_param = &avs2_dec->param;

	int32_t f = 0, symbol, pre_symbole;
	const int32_t numCoeff = (int32_t)ALF_MAX_NUM_COEF;

	switch (Alfp->componentID) {
	case ALF_Cb:
	case ALF_Cr: {
		for (pos = 0; pos < numCoeff; pos++) {
			if (Alfp->componentID == ALF_Cb)
				Alfp->coeffmulti[0][pos] =
					get_param(
					rpm_param->alf.alf_cb_coeffmulti[pos],
					"Chroma ALF coefficients");
			else
				Alfp->coeffmulti[0][pos] =
					get_param(
					rpm_param->alf.alf_cr_coeffmulti[pos],
					"Chroma ALF coefficients");
#if Check_Bitstream
			if (pos <= 7)
				assert(Alfp->coeffmulti[0][pos] >= -64
					&& Alfp->coeffmulti[0][pos] <= 63);
			if (pos == 8)
				assert(Alfp->coeffmulti[0][pos] >= -1088
					&& Alfp->coeffmulti[0][pos] <= 1071);
#endif
		}
	}
	break;
	case ALF_Y: {
		int32_t region_distance_idx = 0;
		Alfp->filters_per_group =
			get_param(rpm_param->alf.alf_filters_num_m_1,
				"ALF_filter_number_minus_1");
#if Check_Bitstream
		assert(Alfp->filters_per_group >= 0
			&& Alfp->filters_per_group <= 15);
#endif
		Alfp->filters_per_group = Alfp->filters_per_group + 1;

		memset(Alfp->filterPattern, 0, NO_VAR_BINS * sizeof(int32_t));
		pre_symbole = 0;
		symbol = 0;
		for (f = 0; f < Alfp->filters_per_group; f++) {
			if (f > 0) {
				if (Alfp->filters_per_group != 16) {
					symbol =
					get_param(rpm_param->alf.region_distance
						[region_distance_idx++],
						"Region distance");
				} else {
					symbol = 1;
				}
				Alfp->filterPattern[symbol + pre_symbole] = 1;
				pre_symbole = symbol + pre_symbole;
			}

			for (pos = 0; pos < numCoeff; pos++) {
				Alfp->coeffmulti[f][pos] =
				get_param(
					rpm_param->alf.alf_y_coeffmulti[f][pos],
					"Luma ALF coefficients");
#if Check_Bitstream
				if (pos <= 7)
					assert(
						Alfp->coeffmulti[f][pos]
						>= -64 &&
						Alfp->coeffmulti[f][pos]
						<= 63);
				if (pos == 8)
					assert(
						Alfp->coeffmulti[f][pos]
						>= -1088 &&
						Alfp->coeffmulti[f][pos]
						<= 1071);
#endif

			}
		}

#if Check_Bitstream
		assert(pre_symbole >= 0 && pre_symbole <= 15);

#endif
	}
	break;
	default: {
		pr_info("Not a legal component ID\n");
		assert(0);
		return; /* exit(-1);*/
	}
	}
}

void Read_ALF_param(struct avs2_decoder *avs2_dec)
{
	struct inp_par    *input = &avs2_dec->input;
	struct ImageParameters_s    *img = &avs2_dec->img;
	union param_u *rpm_param = &avs2_dec->param;
	int32_t compIdx;
	if (input->alf_enable) {
		img->pic_alf_on[0] =
			get_param(
			rpm_param->alf.picture_alf_enable_Y,
			"alf_pic_flag_Y");
		img->pic_alf_on[1] =
			get_param(
			rpm_param->alf.picture_alf_enable_Cb,
			"alf_pic_flag_Cb");
		img->pic_alf_on[2] =
			get_param(
			rpm_param->alf.picture_alf_enable_Cr,
			"alf_pic_flag_Cr");

		avs2_dec->m_alfPictureParam[ALF_Y].alf_flag
			= img->pic_alf_on[ALF_Y];
		avs2_dec->m_alfPictureParam[ALF_Cb].alf_flag
			= img->pic_alf_on[ALF_Cb];
		avs2_dec->m_alfPictureParam[ALF_Cr].alf_flag
			= img->pic_alf_on[ALF_Cr];
		if (img->pic_alf_on[0]
			|| img->pic_alf_on[1]
			|| img->pic_alf_on[2]) {
			for (compIdx = 0;
				compIdx < NUM_ALF_COMPONENT;
				compIdx++) {
				if (img->pic_alf_on[compIdx]) {
					readAlfCoeff(
					avs2_dec,
					&avs2_dec->m_alfPictureParam[compIdx]);
				}
			}
		}
	}

}

void Get_SequenceHeader(struct avs2_decoder *avs2_dec)
{
	struct inp_par    *input = &avs2_dec->input;
	struct ImageParameters_s    *img = &avs2_dec->img;
	struct Video_Com_data_s *hc = &avs2_dec->hc;
	struct Video_Dec_data_s *hd = &avs2_dec->hd;
	union param_u *rpm_param = &avs2_dec->param;
	/*int32_t i, j;*/

	/*fpr_info(stdout, "Sequence Header\n");*/
	/*memcpy(currStream->streamBuffer, buf, length);*/
	/*currStream->code_len = currStream->bitstream_length = length;*/
	/*currStream->read_len = currStream->frame_bitoffset = (startcodepos +
	  1) * 8;*/

	input->profile_id           =
		get_param(rpm_param->p.profile_id, "profile_id");
	input->level_id             =
		get_param(rpm_param->p.level_id, "level_id");
	hd->progressive_sequence        =
		get_param(
			rpm_param->p.progressive_sequence,
			"progressive_sequence");
#if INTERLACE_CODING
	hd->is_field_sequence           =
		get_param(
			rpm_param->p.is_field_sequence,
			"field_coded_sequence");
#endif
#if HALF_PIXEL_COMPENSATION || HALF_PIXEL_CHROMA
	img->is_field_sequence = hd->is_field_sequence;
#endif
	hd->horizontal_size =
		get_param(rpm_param->p.horizontal_size, "horizontal_size");
	hd->vertical_size =
		get_param(rpm_param->p.vertical_size, "vertical_size");
	input->chroma_format               =
		get_param(rpm_param->p.chroma_format, "chroma_format");
	input->output_bit_depth = 8;
	input->sample_bit_depth = 8;
	hd->sample_precision = 1;
	if (input->profile_id == BASELINE10_PROFILE) { /* 10bit profile (0x52)*/
		input->output_bit_depth =
			get_param(rpm_param->p.sample_precision,
			"sample_precision");
		input->output_bit_depth =
			6 + (input->output_bit_depth) * 2;
		input->sample_bit_depth =
			get_param(rpm_param->p.encoding_precision,
			"encoding_precision");
		input->sample_bit_depth =
			6 + (input->sample_bit_depth) * 2;
	} else { /* other profile*/
		hd->sample_precision =
			get_param(rpm_param->p.sample_precision,
				"sample_precision");
	}
	hd->aspect_ratio_information    =
		get_param(rpm_param->p.aspect_ratio_information,
			"aspect_ratio_information");
	hd->frame_rate_code             =
		get_param(rpm_param->p.frame_rate_code, "frame_rate_code");

	hd->bit_rate_lower              =
		get_param(rpm_param->p.bit_rate_lower, "bit_rate_lower");
	/*hd->marker_bit                  = get_param(rpm_param->p.marker_bit,
	 * "marker bit");*/
	/*CHECKMARKERBIT*/
	hd->bit_rate_upper              =
		get_param(rpm_param->p.bit_rate_upper, "bit_rate_upper");
	hd->low_delay                   =
		get_param(rpm_param->p.low_delay, "low_delay");
	/*hd->marker_bit                  =
		get_param(rpm_param->p.marker_bit2,
	 "marker bit");*/
	/*CHECKMARKERBIT*/
#if M3480_TEMPORAL_SCALABLE
	hd->temporal_id_exist_flag      =
		get_param(rpm_param->p.temporal_id_exist_flag,
			"temporal_id exist flag"); /*get
		Extention Flag*/
#endif
	/*u_v(18, "bbv buffer size");*/
	input->g_uiMaxSizeInBit         =
		get_param(rpm_param->p.g_uiMaxSizeInBit,
		"Largest Coding Block Size");


	/*hd->background_picture_enable = 0x01 ^
		(get_param(rpm_param->p.avs2_seq_flags,
		"background_picture_disable")
		>> BACKGROUND_PICTURE_DISABLE_BIT) & 0x1;*/
	/*rain???*/
	hd->background_picture_enable = 0x01 ^
		((get_param(rpm_param->p.avs2_seq_flags,
		"background_picture_disable")
		>> BACKGROUND_PICTURE_DISABLE_BIT) & 0x1);


	hd->b_dmh_enabled           = 1;

	hd->b_mhpskip_enabled       =
		get_param(rpm_param->p.avs2_seq_flags >> B_MHPSKIP_ENABLED_BIT,
			"mhpskip enabled") & 0x1;
	hd->dhp_enabled             =
		get_param(rpm_param->p.avs2_seq_flags >> DHP_ENABLED_BIT,
			"dhp enabled") & 0x1;
	hd->wsm_enabled             =
		get_param(rpm_param->p.avs2_seq_flags >> WSM_ENABLED_BIT,
			"wsm enabled") & 0x1;

	img->inter_amp_enable       =
		get_param(rpm_param->p.avs2_seq_flags >> INTER_AMP_ENABLE_BIT,
			"Asymmetric Motion Partitions") & 0x1;
	input->useNSQT              =
		get_param(rpm_param->p.avs2_seq_flags >> USENSQT_BIT,
			"useNSQT") & 0x1;
	input->useSDIP              =
		get_param(rpm_param->p.avs2_seq_flags >> USESDIP_BIT,
			"useNSIP") & 0x1;

	hd->b_secT_enabled          =
		get_param(rpm_param->p.avs2_seq_flags >> B_SECT_ENABLED_BIT,
			"secT enabled") & 0x1;

	input->sao_enable           =
		get_param(rpm_param->p.avs2_seq_flags >> SAO_ENABLE_BIT,
			"SAO Enable Flag") & 0x1;
	input->alf_enable           =
		get_param(rpm_param->p.avs2_seq_flags >> ALF_ENABLE_BIT,
			"ALF Enable Flag") & 0x1;
	hd->b_pmvr_enabled          =
		get_param(rpm_param->p.avs2_seq_flags >> B_PMVR_ENABLED_BIT,
			"pmvr enabled") & 0x1;


	hd->gop_size = get_param(rpm_param->p.num_of_RPS,
		"num_of_RPS");
#if Check_Bitstream
	/*assert(hd->gop_size<=32);*/
#endif

	if (hd->low_delay == 0) {
		hd->picture_reorder_delay  =
			get_param(rpm_param->p.picture_reorder_delay,
			"picture_reorder_delay");
	}

	input->crossSliceLoopFilter    =
		get_param(rpm_param->p.avs2_seq_flags
			>> CROSSSLICELOOPFILTER_BIT,
			"Cross Loop Filter Flag") & 0x1;

#if BCBR
	if ((input->profile_id == SCENE_PROFILE ||
		input->profile_id == SCENE10_PROFILE) &&
		hd->background_picture_enable) {
		hd->bcbr_enable = u_v(1,
			"block_composed_background_picture_enable");
		u_v(1, "reserved bits");
	} else {
		hd->bcbr_enable = 0;
		u_v(2, "reserved bits");
	}
#else
	/*u_v(2, "reserved bits");*/
#endif

	img->width          = hd->horizontal_size;
	img->height         = hd->vertical_size;
	img->width_cr       = (img->width >> 1);

	if (input->chroma_format == 1) {
		img->height_cr
		= (img->height >> 1);
	}

	img->PicWidthInMbs  = img->width / MIN_CU_SIZE;
	img->PicHeightInMbs = img->height / MIN_CU_SIZE;
	img->PicSizeInMbs   = img->PicWidthInMbs * img->PicHeightInMbs;
	img->buf_cycle      = input->buf_cycle + 1;
	img->max_mb_nr      = (img->width * img->height)
		/ (MIN_CU_SIZE * MIN_CU_SIZE);

#ifdef AML
avs2_dec->lcu_size =
	get_param(rpm_param->p.lcu_size, "lcu_size");
avs2_dec->lcu_size = 1<<(avs2_dec->lcu_size);
#endif
hc->seq_header++;
}


void Get_I_Picture_Header(struct avs2_decoder *avs2_dec)
{
	struct ImageParameters_s    *img = &avs2_dec->img;
	struct Video_Dec_data_s *hd = &avs2_dec->hd;
	union param_u *rpm_param = &avs2_dec->param;

#if RD1501_FIX_BG /*//Longfei.Wang@mediatek.com*/
	hd->background_picture_flag = 0;
	hd->background_picture_output_flag = 0;
	img->typeb = 0;
#endif

	hd->time_code_flag       =
		get_param(rpm_param->p.time_code_flag,
		"time_code_flag");

	if (hd->time_code_flag) {
		hd->time_code        =
			get_param(rpm_param->p.time_code,
			"time_code");
	}
	if (hd->background_picture_enable) {
		hd->background_picture_flag =
			get_param(rpm_param->p.background_picture_flag,
			"background_picture_flag");

		if (hd->background_picture_flag) {
			img->typeb =
				BACKGROUND_IMG;
		} else {
			img->typeb = 0;
		}

		if (img->typeb == BACKGROUND_IMG) {
			hd->background_picture_output_flag =
				get_param(
				rpm_param->p.background_picture_output_flag,
				"background_picture_output_flag");
		}
	}


	{
		img->coding_order         =
			get_param(rpm_param->p.coding_order,
			"coding_order");



#if M3480_TEMPORAL_SCALABLE
		if (hd->temporal_id_exist_flag == 1) {
			hd->cur_layer =
				get_param(rpm_param->p.cur_layer,
				"temporal_id");
		}
#endif
#if RD1501_FIX_BG  /*Longfei.Wang@mediatek.com*/
		if (hd->low_delay == 0
			&& !(hd->background_picture_flag &&
		!hd->background_picture_output_flag)) { /*cdp*/
#else
			if (hd->low_delay == 0 &&
			    !(hd->background_picture_enable &&
			      !hd->background_picture_output_flag)) { /*cdp*/
#endif
				hd->displaydelay =
					get_param(rpm_param->p.displaydelay,
					"picture_output_delay");
			}

		}
		{
			int32_t RPS_idx;/* = (img->coding_order-1) % gop_size;*/
			int32_t predict;
			int32_t j;
			predict =
				get_param(rpm_param->p.predict,
				"use RCS in SPS");
			/*if (predict) {*/
			RPS_idx =
				get_param(rpm_param->p.RPS_idx,
				"predict for RCS");
			/*    hd->curr_RPS = hd->decod_RPS[RPS_idx];*/
			/*} else {*/
			/*gop size16*/
			hd->curr_RPS.referd_by_others =
				get_param(rpm_param->p.referd_by_others_cur,
				"refered by others");
			hd->curr_RPS.num_of_ref =
				get_param(rpm_param->p.num_of_ref_cur,
				"num of reference picture");
			for (j = 0; j < hd->curr_RPS.num_of_ref; j++) {
				hd->curr_RPS.ref_pic[j] =
					get_param(rpm_param->p.ref_pic_cur[j],
					"delta COI of ref pic");
			}
			hd->curr_RPS.num_to_remove =
				get_param(rpm_param->p.num_to_remove_cur,
				"num of removed picture");
#ifdef SANITY_CHECK
			if (hd->curr_RPS.num_to_remove > 8)	{
				hd->curr_RPS.num_to_remove = 8;
				pr_info("Warning, %s: num_to_remove %d beyond range, force to 8\n",
					__func__, hd->curr_RPS.num_to_remove);
			}
#endif

			for (j = 0; j < hd->curr_RPS.num_to_remove; j++) {
				hd->curr_RPS.remove_pic[j] =
					get_param(
					rpm_param->p.remove_pic_cur[j],
					"delta COI of removed pic");
			}
			/*u_v(1, "marker bit");*/

			/*}*/
		}
		/*xyji 12.23*/
		if (hd->low_delay) {
			/*ue_v(
			"bbv check times");*/
		}

		hd->progressive_frame =
			get_param(rpm_param->p.progressive_frame,
			"progressive_frame");

		if (!hd->progressive_frame) {
			img->picture_structure   =
				get_param(rpm_param->p.picture_structure,
				"picture_structure");
		} else {
			img->picture_structure
				= 1;
		}

		hd->top_field_first =
			get_param(rpm_param->p.top_field_first,
			"top_field_first");
		hd->repeat_first_field =
			get_param(rpm_param->p.repeat_first_field,
			"repeat_first_field");
#if INTERLACE_CODING
		if (hd->is_field_sequence) {
			hd->is_top_field         =
				get_param(rpm_param->p.is_top_field,
				"is_top_field");
#if HALF_PIXEL_COMPENSATION || HALF_PIXEL_CHROMA
			img->is_top_field       = hd->is_top_field;
#endif
		}
#endif


	img->qp                = hd->picture_qp;

	img->type              = I_IMG;

}

/*
 * Function:pb picture header
 * Input:
 * Output:
 * Return:
 * Attention:
 */

void Get_PB_Picture_Header(struct avs2_decoder *avs2_dec)
{
	struct ImageParameters_s    *img = &avs2_dec->img;
	struct Video_Dec_data_s *hd = &avs2_dec->hd;
	union param_u *rpm_param = &avs2_dec->param;


	/*u_v(32, "bbv delay");*/

	hd->picture_coding_type                =
		get_param(rpm_param->p.picture_coding_type,
		"picture_coding_type");

	if (hd->background_picture_enable &&
		(hd->picture_coding_type == 1 ||
			hd->picture_coding_type == 3)) {
		if (hd->picture_coding_type == 1) {
			hd->background_pred_flag       =
				get_param(
				rpm_param->p.background_pred_flag,
				"background_pred_flag");
		} else {
			hd->background_pred_flag = 0;
		}
		if (hd->background_pred_flag == 0) {

			hd->background_reference_enable =
				get_param(
				rpm_param->
				p.background_reference_enable,
				"background_reference_enable");

		} else {
#if RD170_FIX_BG
			hd->background_reference_enable = 1;
#else
			hd->background_reference_enable = 0;
#endif
		}

	} else {
		hd->background_pred_flag = 0;
		hd->background_reference_enable = 0;
	}



	if (hd->picture_coding_type == 1) {
		img->type =
			P_IMG;
	} else if (hd->picture_coding_type == 3) {
		img->type =
			F_IMG;
	} else {
		img->type =
			B_IMG;
	}


	if (hd->picture_coding_type == 1 &&
		hd->background_pred_flag) {
		img->typeb = BP_IMG;
	} else {
		img->typeb = 0;
	}


	{
		img->coding_order         =
			get_param(
			rpm_param->p.coding_order,
			"coding_order");


#if M3480_TEMPORAL_SCALABLE
		if (hd->temporal_id_exist_flag == 1) {
			hd->cur_layer =
				get_param(rpm_param->p.cur_layer,
				"temporal_id");
		}
#endif

		if (hd->low_delay == 0) {
			hd->displaydelay      =
			get_param(rpm_param->p.displaydelay,
			"displaydelay");
		}
	}
	{
		int32_t RPS_idx;/* = (img->coding_order-1) % gop_size;*/
		int32_t predict;
		predict    =
			get_param(rpm_param->p.predict,
			"use RPS in SPS");
		if (predict) {
			RPS_idx =
				get_param(rpm_param->p.RPS_idx,
				"predict for RPS");
			hd->curr_RPS = hd->decod_RPS[RPS_idx];
		} /*else*/
		{
			/*gop size16*/
			int32_t j;
			hd->curr_RPS.referd_by_others =
				get_param(
				rpm_param->p.referd_by_others_cur,
				"refered by others");
			hd->curr_RPS.num_of_ref =
				get_param(
				rpm_param->p.num_of_ref_cur,
				"num of reference picture");
			for (j = 0; j < hd->curr_RPS.num_of_ref; j++) {
				hd->curr_RPS.ref_pic[j] =
					get_param(
					rpm_param->p.ref_pic_cur[j],
					"delta COI of ref pic");
			}
			hd->curr_RPS.num_to_remove =
				get_param(
				rpm_param->p.num_to_remove_cur,
				"num of removed picture");
#ifdef SANITY_CHECK
			if (hd->curr_RPS.num_to_remove > 8)	{
				hd->curr_RPS.num_to_remove = 8;
				pr_info("Warning, %s: num_to_remove %d beyond range, force to 8\n",
					__func__, hd->curr_RPS.num_to_remove);
			}
#endif
			for (j = 0;
				j < hd->curr_RPS.num_to_remove; j++) {
				hd->curr_RPS.remove_pic[j] =
				get_param(
					rpm_param->p.remove_pic_cur[j],
					"delta COI of removed pic");
			}
			/*u_v(1, "marker bit");*/

		}
	}
	/*xyji 12.23*/
	if (hd->low_delay) {
		/*ue_v(
		"bbv check times");*/
	}

	hd->progressive_frame       =
	get_param(rpm_param->p.progressive_frame,
		"progressive_frame");

	if (!hd->progressive_frame) {
		img->picture_structure  =
		get_param(rpm_param->p.picture_structure,
			"picture_structure");
	} else {
		img->picture_structure  = 1;
	}

	hd->top_field_first         =
	get_param(rpm_param->p.top_field_first,
		"top_field_first");
	hd->repeat_first_field      =
	get_param(rpm_param->p.repeat_first_field,
		"repeat_first_field");
#if INTERLACE_CODING
	if (hd->is_field_sequence) {
		hd->is_top_field        =
		get_param(rpm_param->p.is_top_field,
			"is_top_field");
#if HALF_PIXEL_COMPENSATION || HALF_PIXEL_CHROMA
		img->is_top_field = hd->is_top_field;
#endif
		/*u_v(1, "reserved bit for interlace coding");*/
	}
#endif

#if Check_Bitstream
	/*assert(hd->picture_qp>=0&&hd->picture_qp<=(63 + 8 *
	  (input->sample_bit_depth - 8)));*/
#endif

	img->random_access_decodable_flag =
	get_param(rpm_param->p.random_access_decodable_flag,
		"random_access_decodable_flag");

	img->qp                = hd->picture_qp;
}




void calc_picture_distance(struct avs2_decoder *avs2_dec)
{
	struct ImageParameters_s    *img = &avs2_dec->img;
	struct Video_Com_data_s *hc = &avs2_dec->hc;
	struct Video_Dec_data_s *hd = &avs2_dec->hd;
	/*
	union param_u *rpm_param = &avs2_dec->param;

	for POC mode 0:
	uint32_t        MaxPicDistanceLsb = (1 << 8);
	 */
	if (img->coding_order  <  img->PrevPicDistanceLsb)

	{
		int32_t i, j;

		hc->total_frames++;
		for (i = 0; i < avs2_dec->ref_maxbuffer; i++) {
			if (
				avs2_dec->fref[i]->imgtr_fwRefDistance
				>= 0) {
				avs2_dec->fref[i]->
				imgtr_fwRefDistance -= 256;
				avs2_dec->fref[i]->
				imgcoi_ref -= 256;
			}
#if RD170_FIX_BG
	for (j = 0; j < MAXREF; j++) {
#else
		for (j = 0; j < 4; j++) {
#endif
			avs2_dec->fref[i]->ref_poc[j] -= 256;
		}
	}
	for (i = 0; i < avs2_dec->outprint.buffer_num; i++) {
		avs2_dec->outprint.stdoutdata[i].framenum -= 256;
		avs2_dec->outprint.stdoutdata[i].tr -= 256;
	}

	hd->last_output -= 256;
	hd->curr_IDRtr -= 256;
	hd->curr_IDRcoi -= 256;
	hd->next_IDRtr -= 256;
	hd->next_IDRcoi -= 256;
	}
	if (hd->low_delay == 0) {
		img->tr = img->coding_order +
		hd->displaydelay - hd->picture_reorder_delay;
	} else {
		img->tr =
		img->coding_order;
	}

#if REMOVE_UNUSED
	img->pic_distance = img->tr;
#else
	img->pic_distance = img->tr % 256;
#endif
	hc->picture_distance = img->pic_distance;

}

int32_t avs2_init_global_buffers(struct avs2_decoder *avs2_dec)
{
	struct inp_par    *input = &avs2_dec->input;
	struct ImageParameters_s    *img = &avs2_dec->img;
	struct Video_Com_data_s *hc = &avs2_dec->hc;

	int32_t refnum;

	int32_t memory_size = 0;
	/*
int32_t img_height = (hd->vertical_size + img->auto_crop_bottom);
	 */
	img->buf_cycle = input->buf_cycle + 1;

	img->buf_cycle *= 2;

	hc->background_ref = hc->backgroundReferenceFrame;

	for (refnum = 0; refnum < REF_MAXBUFFER; refnum++) {
		avs2_dec->fref[refnum] = &avs2_dec->frm_pool[refnum];

		/*//avs2_dec->fref[i] memory allocation*/
		if (is_avs2_print_bufmgr_detail())
			pr_info("[t] avs2_dec->fref[%d]@0x%p\n",
			refnum, avs2_dec->fref[refnum]);
		avs2_dec->fref[refnum]->imgcoi_ref = -257;
		avs2_dec->fref[refnum]->is_output = -1;
		avs2_dec->fref[refnum]->refered_by_others = -1;
		avs2_dec->fref[refnum]->
			imgtr_fwRefDistance = -256;
		init_frame_t(avs2_dec->fref[refnum]);
#ifdef AML
		avs2_dec->fref[refnum]->index = refnum;
#endif
	}
#ifdef AML
	avs2_dec->f_bg = NULL;

	avs2_dec->m_bg = &avs2_dec->frm_pool[REF_MAXBUFFER];
		/*///avs2_dec->fref[i] memory allocation*/
	if (is_avs2_print_bufmgr_detail())
		pr_info("[t] avs2_dec->m_bg@0x%p\n",
		avs2_dec->m_bg);
	avs2_dec->m_bg->imgcoi_ref = -257;
	avs2_dec->m_bg->is_output = -1;
	avs2_dec->m_bg->refered_by_others = -1;
	avs2_dec->m_bg->imgtr_fwRefDistance = -256;
	init_frame_t(avs2_dec->m_bg);
	avs2_dec->m_bg->index = refnum;
#endif

#if BCBR
	/*init BCBR related*/
	img->iNumCUsInFrame =
		((img->width + MAX_CU_SIZE - 1) / MAX_CU_SIZE)
		* ((img->height + MAX_CU_SIZE - 1)
		/ MAX_CU_SIZE);
	/*img->BLCUidx =  (int32_t*) calloc(
	  img->iNumCUsInFrame, sizeof(int32_t));*/
	/*memset( img->BLCUidx, 0, img->iNumCUsInFrame);*/
#endif
	return memory_size;
}

#ifdef AML
static void free_unused_buffers(struct avs2_decoder *avs2_dec)
{
	struct inp_par    *input = &avs2_dec->input;
	struct ImageParameters_s    *img = &avs2_dec->img;
	struct Video_Com_data_s *hc = &avs2_dec->hc;

	int32_t refnum;

	img->buf_cycle = input->buf_cycle + 1;

	img->buf_cycle *= 2;

	hc->background_ref = hc->backgroundReferenceFrame;

	for (refnum = 0; refnum < REF_MAXBUFFER; refnum++) {
#ifndef NO_DISPLAY
		if (avs2_dec->fref[refnum]->vf_ref > 0 ||
			avs2_dec->fref[refnum]->to_prepare_disp)
			continue;
#endif
		if (is_avs2_print_bufmgr_detail())
			pr_info("%s[t] avs2_dec->fref[%d]@0x%p\n",
			__func__, refnum, avs2_dec->fref[refnum]);
		avs2_dec->fref[refnum]->imgcoi_ref = -257;
		avs2_dec->fref[refnum]->is_output = -1;
		avs2_dec->fref[refnum]->refered_by_others = -1;
		avs2_dec->fref[refnum]->
			imgtr_fwRefDistance = -256;
		memset(avs2_dec->fref[refnum]->ref_poc, 0,
			sizeof(avs2_dec->fref[refnum]->ref_poc));
	}
	avs2_dec->f_bg = NULL;

	if (is_avs2_print_bufmgr_detail())
		pr_info("%s[t] avs2_dec->m_bg@0x%p\n",
		__func__, avs2_dec->m_bg);
	avs2_dec->m_bg->imgcoi_ref = -257;
	avs2_dec->m_bg->is_output = -1;
	avs2_dec->m_bg->refered_by_others = -1;
	avs2_dec->m_bg->imgtr_fwRefDistance = -256;
	memset(avs2_dec->m_bg->ref_poc, 0,
		sizeof(avs2_dec->m_bg->ref_poc));

#if BCBR
	/*init BCBR related*/
	img->iNumCUsInFrame =
		((img->width + MAX_CU_SIZE - 1) / MAX_CU_SIZE)
		* ((img->height + MAX_CU_SIZE - 1)
		/ MAX_CU_SIZE);
	/*img->BLCUidx =  (int32_t*) calloc(
	  img->iNumCUsInFrame, sizeof(int32_t));*/
	/*memset( img->BLCUidx, 0, img->iNumCUsInFrame);*/
#endif
}
#endif

void init_frame_t(struct avs2_frame_s *currfref)
{
	memset(currfref, 0, sizeof(struct avs2_frame_s));
	currfref->imgcoi_ref          = -257;
	currfref->is_output           = -1;
	currfref->refered_by_others   = -1;
	currfref->imgtr_fwRefDistance = -256;
	memset(currfref->ref_poc, 0, sizeof(currfref->ref_poc));
}

void get_reference_list_info(struct avs2_decoder *avs2_dec, int8_t *str)
{
	struct ImageParameters_s    *img = &avs2_dec->img;
	struct Video_Com_data_s *hc = &avs2_dec->hc;

	int8_t str_tmp[16];
	int32_t i;
	/* int32_t poc = hc->f_rec->imgtr_fwRefDistance;
	  fred.chiu@mediatek.com*/

	if (img->num_of_references > 0) {
		strcpy(str, "[");
		for (i = 0; i < img->num_of_references; i++) {
#if RD1510_FIX_BG
			if (img->type == B_IMG) {
				sprintf(str_tmp, "%4d    ",
					hc->f_rec->
					ref_poc[
					img->num_of_references - 1 - i]);
			} else {
				sprintf(str_tmp, "%4d    ",
					hc->f_rec->ref_poc[i]);
			}
#else
			sprintf(str_tmp, "%4d     ",
				avs2_dec->fref[i]->imgtr_fwRefDistance);
#endif

			str_tmp[5] = '\0';
			strcat(str, str_tmp);
		}
		strcat(str, "]");
	} else {
		str[0] = '\0';
	}
}

void prepare_RefInfo(struct avs2_decoder *avs2_dec)
{
	struct ImageParameters_s    *img = &avs2_dec->img;
	struct Video_Com_data_s *hc = &avs2_dec->hc;
	struct Video_Dec_data_s *hd = &avs2_dec->hd;

	int32_t i, j;
	int32_t ii;
	struct avs2_frame_s *tmp_fref;

	/*update IDR frame*/
	if (img->tr > hd->next_IDRtr && hd->curr_IDRtr != hd->next_IDRtr) {
		hd->curr_IDRtr  = hd->next_IDRtr;
		hd->curr_IDRcoi = hd->next_IDRcoi;
	}
	/* re-order the ref buffer according to RPS*/
	img->num_of_references = hd->curr_RPS.num_of_ref;

#if 1
	/*rain*/
	if (is_avs2_print_bufmgr_detail()) {
		pr_info("%s: coding_order is %d, curr_IDRcoi is %d\n",
		__func__, img->coding_order, hd->curr_IDRcoi);
		for (ii = 0; ii < MAXREF; ii++) {
			pr_info("ref_pic(%d)=%d\n",
			ii, hd->curr_RPS.ref_pic[ii]);
	}
	for (ii = 0; ii < avs2_dec->ref_maxbuffer; ii++) {
		pr_info(
			"fref[%d]: index %d imgcoi_ref %d imgtr_fwRefDistance %d\n",
			ii, avs2_dec->fref[ii]->index,
			avs2_dec->fref[ii]->imgcoi_ref,
			avs2_dec->fref[ii]->imgtr_fwRefDistance);
	}
	}
#endif

	for (i = 0; i < hd->curr_RPS.num_of_ref; i++) {
		/*int32_t accumulate = 0;*/
		/* copy tmp_fref from avs2_dec->fref[i] */
		tmp_fref = avs2_dec->fref[i];

#if REMOVE_UNUSED
		for (j = i; j < avs2_dec->ref_maxbuffer; j++) {
			/*/////////////to be modified  IDR*/
			if (avs2_dec->fref[j]->imgcoi_ref ==
				img->coding_order -
				hd->curr_RPS.ref_pic[i]) {
				break;
			}
		}
#else

		for (j = i; j < avs2_dec->ref_maxbuffer; j++) {
			/*/////////////to be modified  IDR*/
			int32_t k , tmp_tr;
			for (k = 0; k < avs2_dec->ref_maxbuffer; k++) {
				if (((int32_t)img->coding_order -
					(int32_t)hd->curr_RPS.ref_pic[i]) ==
					avs2_dec->fref[k]->imgcoi_ref &&
					avs2_dec->fref[k]->imgcoi_ref >= -256) {
					break;
				}
			}
			if (k == avs2_dec->ref_maxbuffer) {
				tmp_tr =
				-1-1;
			} else {
				tmp_tr =
					avs2_dec->fref[k]->imgtr_fwRefDistance;
			}
			if (tmp_tr < hd->curr_IDRtr) {
				hd->curr_RPS.ref_pic[i] =
					img->coding_order - hd->curr_IDRcoi;

				for (k = 0; k < i; k++) {
					if (hd->curr_RPS.ref_pic[k] ==
						hd->curr_RPS.ref_pic[i]) {
						accumulate++;
						break;
					}
				}
			}
			if (avs2_dec->fref[j]->imgcoi_ref ==
				img->coding_order - hd->curr_RPS.ref_pic[i]) {
				break;
			}
		}
		if (j == avs2_dec->ref_maxbuffer || accumulate)
			img->num_of_references--;
#endif
		if (j != avs2_dec->ref_maxbuffer) {
			/* copy avs2_dec->fref[i] from avs2_dec->fref[j] */
			avs2_dec->fref[i] = avs2_dec->fref[j];
			/* copy avs2_dec->fref[j] from ferf[tmp] */
			avs2_dec->fref[j] = tmp_fref;
			if (is_avs2_print_bufmgr_detail()) {
				pr_info("%s, switch %d %d: ", __func__, i, j);
				for (ii = 0; ii < hd->curr_RPS.num_of_ref
				|| ii <= j; ii++)
					pr_info("%d ",
					avs2_dec->fref[ii]->index);
				pr_info("\n");
			}
		}
	}
	if (img->type == B_IMG &&
		(avs2_dec->fref[0]->imgtr_fwRefDistance <= img->tr
		|| avs2_dec->fref[1]->imgtr_fwRefDistance >= img->tr)) {

		pr_info("wrong reference configuration for B frame\n");
		pr_info(
			"fref0 imgtr_fwRefDistance %d, fref1 imgtr_fwRefDistance %d, img->tr %d\n",
				avs2_dec->fref[0]->imgtr_fwRefDistance,
				avs2_dec->fref[1]->imgtr_fwRefDistance,
				img->tr);
		hc->f_rec->error_mark = 1;
		avs2_dec->bufmgr_error_flag = 1;
		return; /* exit(-1);*/
		/*******************************************/
	}

#if !FIX_PROFILE_LEVEL_DPB_RPS_1
	/* delete the frame that will never be used*/
	for (i = 0; i < hd->curr_RPS.num_to_remove; i++) {
		for (j = 0; j < avs2_dec->ref_maxbuffer; j++) {
			if (avs2_dec->fref[j]->imgcoi_ref >= -256
				&& avs2_dec->fref[j]->imgcoi_ref
				== img->coding_order -
				hd->curr_RPS.remove_pic[i]) {
				break;
			}
		}
		if (j < avs2_dec->ref_maxbuffer &&
			j >= img->num_of_references) {
			avs2_dec->fref[j]->imgcoi_ref = -257;
#if M3480_TEMPORAL_SCALABLE
			avs2_dec->fref[j]->temporal_id = -1;
#endif
			if (avs2_dec->fref[j]->is_output == -1) {
				avs2_dec->fref[j]->
				imgtr_fwRefDistance = -256;
			}
		}
	}
#endif

	/* add inter-view reference picture*/

	/*   add current frame to ref buffer*/
	for (i = 0; i < avs2_dec->ref_maxbuffer; i++) {
		if ((avs2_dec->fref[i]->imgcoi_ref < -256
			|| abs(avs2_dec->fref[i]->
				imgtr_fwRefDistance - img->tr) >= 128)
				&& avs2_dec->fref[i]->is_output == -1
				&& avs2_dec->fref[i]->bg_flag == 0
#ifndef NO_DISPLAY
				&& avs2_dec->fref[i]->vf_ref == 0
				&& avs2_dec->fref[i]->to_prepare_disp == 0
#endif
				) {
			break;
		}
	}
	if (i == avs2_dec->ref_maxbuffer) {
		pr_info(
			"%s, warning, no enough buf\n",
			__func__);
		i--;
	}

	hc->f_rec        = avs2_dec->fref[i];
	hc->currentFrame = hc->f_rec->ref;
	hc->f_rec->imgtr_fwRefDistance = img->tr;
	hc->f_rec->imgcoi_ref = img->coding_order;
#if M3480_TEMPORAL_SCALABLE
	hc->f_rec->temporal_id = hd->cur_layer;
#endif
	hc->f_rec->is_output = 1;
#ifdef AML
	hc->f_rec->error_mark = 0;
	hc->f_rec->decoded_lcu = 0;
	hc->f_rec->slice_type = img->type;
#endif
	hc->f_rec->refered_by_others = hd->curr_RPS.referd_by_others;
	if (is_avs2_print_bufmgr_detail())
		pr_info(
			"%s, set f_rec (cur_pic) <= fref[%d] img->tr %d coding_order %d img_type %d\n",
			__func__, i, img->tr, img->coding_order,
			img->type);

	if (img->type != B_IMG) {
		for (j = 0;
			j < img->num_of_references; j++) {
			hc->f_rec->ref_poc[j] =
			avs2_dec->fref[j]->imgtr_fwRefDistance;
		}
	} else {
		hc->f_rec->ref_poc[0] =
			avs2_dec->fref[1]->imgtr_fwRefDistance;
		hc->f_rec->ref_poc[1] =
			avs2_dec->fref[0]->imgtr_fwRefDistance;
	}

#if M3480_TEMPORAL_SCALABLE

	for (j = img->num_of_references;
		j < 4; j++) {
		/**/
		hc->f_rec->ref_poc[j] = 0;
	}

	if (img->type == INTRA_IMG) {
		int32_t l;
		for (l = 0; l < 4; l++) {
			hc->f_rec->ref_poc[l]
			= img->tr;
		}
	}

#endif

/*////////////////////////////////////////////////////////////////////////*/
	/* updata ref pointer*/

	if (img->type != I_IMG) {

		img->imgtr_next_P = img->type == B_IMG ?
			avs2_dec->fref[0]->imgtr_fwRefDistance : img->tr;
		if (img->type == B_IMG) {
			hd->trtmp = avs2_dec->fref[0]->imgtr_fwRefDistance;
			avs2_dec->fref[0]->imgtr_fwRefDistance =
			avs2_dec->fref[1]->imgtr_fwRefDistance;
		}
	}
#if 1
	/*rain*/
	if (is_avs2_print_bufmgr_detail()) {
		for (ii = 0; ii < avs2_dec->ref_maxbuffer; ii++) {
			pr_info(
			"fref[%d]: index %d imgcoi_ref %d imgtr_fwRefDistance %d refered %d, is_out %d, bg %d, vf_ref %d ref_pos(%d,%d,%d,%d,%d,%d,%d)\n",
			ii, avs2_dec->fref[ii]->index,
			avs2_dec->fref[ii]->imgcoi_ref,
			avs2_dec->fref[ii]->imgtr_fwRefDistance,
			avs2_dec->fref[ii]->refered_by_others,
			avs2_dec->fref[ii]->is_output,
			avs2_dec->fref[ii]->bg_flag,
			avs2_dec->fref[ii]->vf_ref,
			avs2_dec->fref[ii]->ref_poc[0],
			avs2_dec->fref[ii]->ref_poc[1],
			avs2_dec->fref[ii]->ref_poc[2],
			avs2_dec->fref[ii]->ref_poc[3],
			avs2_dec->fref[ii]->ref_poc[4],
			avs2_dec->fref[ii]->ref_poc[5],
			avs2_dec->fref[ii]->ref_poc[6]
		);
	}
	}
#endif
}

int32_t init_frame(struct avs2_decoder *avs2_dec)
{
	struct ImageParameters_s    *img = &avs2_dec->img;
	struct Video_Com_data_s *hc = &avs2_dec->hc;
	struct Video_Dec_data_s *hd = &avs2_dec->hd;


#if RD1510_FIX_BG
	if (img->type == I_IMG &&
		img->typeb == BACKGROUND_IMG) { /*G/GB frame*/
		img->num_of_references = 0;
	} else if (img->type == P_IMG && img->typeb == BP_IMG) {
		/* only one reference frame(G\GB) for S frame*/
		img->num_of_references = 1;
	}
#endif

	if (img->typeb == BACKGROUND_IMG &&
		hd->background_picture_output_flag == 0) {
		hc->currentFrame = hc->background_ref;
#ifdef AML
		hc->cur_pic = avs2_dec->m_bg;
#endif
	} else {
		prepare_RefInfo(avs2_dec);
#ifdef AML
		hc->cur_pic = hc->f_rec;
#endif
	}


#ifdef FIX_CHROMA_FIELD_MV_BK_DIST
	if (img->typeb == BACKGROUND_IMG
		&& img->is_field_sequence) {
		avs2_dec->bk_img_is_top_field
			= img->is_top_field;
	}
#endif
	return 0;
}

void delete_trbuffer(struct outdata_s *data, int32_t pos)
{
	int32_t i;
	for (i = pos;
		i < data->buffer_num - 1; i++) {
		data->stdoutdata[i] =
		data->stdoutdata[i + 1];
	}
	data->buffer_num--;
}

#if RD170_FIX_BG
void flushDPB(struct avs2_decoder *avs2_dec)
{
	struct Video_Dec_data_s *hd = &avs2_dec->hd;
	int j, tmp_min, i, pos = -1;
	int search_times = avs2_dec->outprint.buffer_num;

	tmp_min = 1 << 20;
	i = 0, j = 0;
	pos = -1;

	for (j = 0; j < search_times; j++) {
		pos = -1;
		tmp_min = (1 << 20);
		/*search for min poi picture to display*/
		for (i = 0; i < avs2_dec->outprint.buffer_num; i++) {
			if (avs2_dec->outprint.stdoutdata[i].tr < tmp_min) {
				pos = i;
				tmp_min = avs2_dec->outprint.stdoutdata[i].tr;
			}
		}

		if (pos != -1) {
			hd->last_output = avs2_dec->outprint.stdoutdata[pos].tr;
			report_frame(avs2_dec, &avs2_dec->outprint, pos);
			if (avs2_dec->outprint.stdoutdata[pos].typeb
				== BACKGROUND_IMG &&
			avs2_dec->outprint.stdoutdata[pos].
			background_picture_output_flag
			== 0) {
				/*write_GB_frame(hd->p_out_background);*/
			} else {
				write_frame(avs2_dec,
					avs2_dec->outprint.stdoutdata[pos].tr);
			}

			delete_trbuffer(&avs2_dec->outprint, pos);
		}
	}

	/*clear dpb info*/
	for (j = 0; j < REF_MAXBUFFER; j++) {
		avs2_dec->fref[j]->imgtr_fwRefDistance = -256;
		avs2_dec->fref[j]->imgcoi_ref = -257;
		avs2_dec->fref[j]->temporal_id = -1;
		avs2_dec->fref[j]->refered_by_others = 0;
	}
}
#endif



#if M3480_TEMPORAL_SCALABLE
void cleanRefMVBufRef(int pos)
{
#if 0
	int k, x, y;
	/*re-init mvbuf*/
	for (k = 0; k < 2; k++) {
		for (y = 0; y < img->height / MIN_BLOCK_SIZE; y++) {
			for (x = 0; x < img->width / MIN_BLOCK_SIZE; x++)
				fref[pos]->mvbuf[y][x][k] = 0;

		}
	}
	/*re-init refbuf*/
	for (y = 0; y < img->height / MIN_BLOCK_SIZE; y++) {
		for (x = 0; x < img->width / MIN_BLOCK_SIZE ; x++)
			fref[pos]->refbuf[y][x] = -1;

	}
#endif
}
#endif

static int frame_postprocessing(struct avs2_decoder *avs2_dec)
{
	struct ImageParameters_s    *img = &avs2_dec->img;
	struct Video_Com_data_s *hc = &avs2_dec->hc;
	struct Video_Dec_data_s *hd = &avs2_dec->hd;

	int32_t pointer_tmp = avs2_dec->outprint.buffer_num;
	int32_t i;
	struct STDOUT_DATA_s *p_outdata;
#if RD160_FIX_BG
	int32_t j, tmp_min, output_cur_dec_pic, pos = -1;
	int32_t search_times = avs2_dec->outprint.buffer_num;
#endif
	/*pic dist by Grandview Semi. @ [06-07-20 15:25]*/
	img->PrevPicDistanceLsb = (img->coding_order % 256);

	pointer_tmp = avs2_dec->outprint.buffer_num;
	p_outdata   = &avs2_dec->outprint.stdoutdata[pointer_tmp];

	p_outdata->type = img->type;
	p_outdata->typeb = img->typeb;
	p_outdata->framenum = img->tr;
	p_outdata->tr = img->tr;
#if 0 /*def ORI*/
	p_outdata->qp = img->qp;
#else
	p_outdata->qp = 0;
#endif
	/*p_outdata->snr_y = snr->snr_y;*/
	/*p_outdata->snr_u = snr->snr_u;*/
	/*p_outdata->snr_v = snr->snr_v;*/
	p_outdata->tmp_time = hd->tmp_time;
	p_outdata->picture_structure = img->picture_structure;
	/*p_outdata->curr_frame_bits =
	  StatBitsPtr->curr_frame_bits;*/
	/*p_outdata->emulate_bits = StatBitsPtr->emulate_bits;*/
#if RD1501_FIX_BG
	p_outdata->background_picture_output_flag
		= hd->background_picture_output_flag;
		/*Longfei.Wang@mediatek.com*/
#endif

#if RD160_FIX_BG
	p_outdata->picture_reorder_delay = hd->picture_reorder_delay;
#endif
	avs2_dec->outprint.buffer_num++;

#if RD170_FIX_BG
	search_times = avs2_dec->outprint.buffer_num;
#endif
	/* record the reference list*/
	strcpy(p_outdata->str_reference_list, hc->str_list_reference);

#if !REF_OUTPUT
	#error "!!!REF_OUTPUT should be 1"
	for (i = 0; i < avs2_dec->outprint.buffer_num; i++) {
		min_tr(avs2_dec->outprint, &pos);
		if (avs2_dec->outprint.stdoutdata[pos].tr < img->tr
			|| avs2_dec->outprint.stdoutdata[pos].tr
			== (hd->last_output + 1)) {
			hd->last_output = avs2_dec->outprint.stdoutdata[pos].tr;
			report_frame(avs2_dec, &avs2_dec->outprint, pos);
#if 0 /*def ORI*/
			write_frame(hd->p_out,
			avs2_dec->outprint.stdoutdata[pos].tr);
#endif
			delete_trbuffer(&avs2_dec->outprint, pos);
			i--;
		} else {
			break;
		}
	}
#else
#if RD160_FIX_BG /*Longfei.Wang@mediatek.com*/
	tmp_min = 1 << 20;
	i = 0, j = 0;
	output_cur_dec_pic = 0;
	pos = -1;
	for (j = 0; j < search_times; j++) {
		pos = -1;
		tmp_min = (1 << 20);
		/*search for min poi picture to display*/
		for (i = 0; i < avs2_dec->outprint.buffer_num; i++) {
			if ((avs2_dec->outprint.stdoutdata[i].tr < tmp_min) &&
				((avs2_dec->outprint.stdoutdata[i].tr
				+ avs2_dec->outprint.stdoutdata[i].
					picture_reorder_delay)
				<= (int32_t)img->coding_order)) {
				pos = i;
				tmp_min = avs2_dec->outprint.stdoutdata[i].tr;
			}
		}

		if ((0 == hd->displaydelay) && (0 == output_cur_dec_pic)) {
			if (img->tr <= tmp_min)	{/*fred.chiu@mediatek.com*/
				/*output current decode picture
				  right now*/
				pos = avs2_dec->outprint.buffer_num - 1;
				output_cur_dec_pic = 1;
			}
		}
		if (pos != -1) {
			hd->last_output = avs2_dec->outprint.stdoutdata[pos].tr;
			report_frame(avs2_dec, &avs2_dec->outprint, pos);
#if 1 /*def ORI*/
			if (avs2_dec->outprint.stdoutdata[pos].typeb
				== BACKGROUND_IMG &&
				avs2_dec->outprint.stdoutdata[pos].
				background_picture_output_flag == 0) {
				/**/
				/**/
			} else {
				write_frame(avs2_dec,
				avs2_dec->outprint.stdoutdata[pos].tr);
			}
#endif
			delete_trbuffer(&avs2_dec->outprint, pos);
		}

	}

#else
	#error "!!!RD160_FIX_BG should be defined"
	if (img->coding_order +
		(uint32_t)hc->total_frames * 256 >=
		(uint32_t)hd->picture_reorder_delay) {
		int32_t tmp_min, pos = -1;
		tmp_min = 1 << 20;

		for (i = 0; i <
			avs2_dec->outprint.buffer_num; i++) {
			if (avs2_dec->outprint.stdoutdata[i].tr
				< tmp_min &&
				avs2_dec->outprint.stdoutdata[i].tr
				>= hd->last_output) {
				/*GB has the same "tr" with "last_output"*/
				pos = i;
				tmp_min =
				avs2_dec->outprint.stdoutdata[i].tr;
			}
		}

		if (pos != -1) {
			hd->last_output = avs2_dec->outprint.stdoutdata[pos].tr;
			report_frame(avs2_dec, &avs2_dec->outprint, pos);
#if RD1501_FIX_BG
			if (avs2_dec->outprint.stdoutdata[pos].typeb
				== BACKGROUND_IMG && avs2_dec->
				outprint.stdoutdata[pos].
				background_picture_output_flag == 0) {
#else
				if (avs2_dec->outprint.stdoutdata[pos].typeb
					== BACKGROUND_IMG &&
					hd->background_picture_output_flag
					== 0) {
#endif
					write_GB_frame(
						hd->p_out_background);
				} else {
					write_frame(avs2_dec,
					avs2_dec->outprint.stdoutdata[pos].tr);
				}
				delete_trbuffer(&avs2_dec->outprint, pos);

			}

		}
#endif
#endif
	return pos;

	}

void write_frame(struct avs2_decoder *avs2_dec, int32_t pos)
{
	int32_t j;

	if (is_avs2_print_bufmgr_detail())
		pr_info("%s(pos = %d)\n", __func__, pos);

	for (j = 0; j < avs2_dec->ref_maxbuffer; j++) {
		if (avs2_dec->fref[j]->imgtr_fwRefDistance == pos) {
			avs2_dec->fref[j]->imgtr_fwRefDistance_bak = pos;
			avs2_dec->fref[j]->is_output = -1;
			avs2_dec->fref[j]->to_prepare_disp =
				avs2_dec->to_prepare_disp_count++;
			if (avs2_dec->fref[j]->refered_by_others == 0
				|| avs2_dec->fref[j]->imgcoi_ref
				== -257) {
				avs2_dec->fref[j]->imgtr_fwRefDistance
					= -256;
				avs2_dec->fref[j]->imgcoi_ref = -257;
#if M3480_TEMPORAL_SCALABLE
				avs2_dec->fref[j]->temporal_id = -1;
#endif
				if (is_avs2_print_bufmgr_detail())
					pr_info("%s, fref index %d\n",
						 __func__, j);
			}
			break;
		}
	}
}

/*rain???, outdata *data*/
void report_frame(struct avs2_decoder *avs2_dec,
	struct outdata_s *data, int32_t pos)
{
	struct ImageParameters_s    *img = &avs2_dec->img;
	struct Video_Com_data_s *hc = &avs2_dec->hc;
	struct Video_Dec_data_s *hd = &avs2_dec->hd;

	int8_t *Frmfld;
	int8_t Frm[] = "FRM";
	int8_t Fld[] = "FLD";
	struct STDOUT_DATA_s *p_stdoutdata
	= &data->stdoutdata[pos];
	const int8_t *typ;

#if 0
	if (input->MD5Enable & 0x02) {
		sprintf(MD5str, "%08X%08X%08X%08X\0",
				p_stdoutdata->DecMD5Value[0],
				p_stdoutdata->DecMD5Value[1],
				p_stdoutdata->DecMD5Value[2],
				p_stdoutdata->DecMD5Value[3]);
	} else {
		memset(MD5val, 0, 16);
		memset(MD5str, 0, 33);
	}
#endif

	if (p_stdoutdata->
		picture_structure) {
		Frmfld = Frm;
	} else {
		Frmfld = Fld;
	}
#if INTERLACE_CODING
	if (img->is_field_sequence) { /*rcs??*/
		Frmfld = Fld;
	}
#endif
	if ((p_stdoutdata->tr + hc->total_frames * 256)
	    == hd->end_SeqTr) {   /* I picture*/
		/*if ( img->new_sequence_flag == 1 )*/
		{
			img->sequence_end_flag = 0;
			/*fprintf(stdout, "Sequence
			  End\n\n");*/
		}
	}
	if ((p_stdoutdata->tr + hc->total_frames * 256)
		== hd->next_IDRtr) {
#if !RD170_FIX_BG
		if (hd->vec_flag) /**/
#endif
		{
			hd->vec_flag = 0;
			/*fprintf(stdout, "Video Edit
			  Code\n");*/
		}
	}

	if (p_stdoutdata->typeb == BACKGROUND_IMG) {
		typ = (hd->background_picture_output_flag != 0) ? "G" : "GB";
	} else {
#if REMOVE_UNUSED
		typ = (p_stdoutdata->type == INTRA_IMG)
			? "I" : (p_stdoutdata->type == INTER_IMG) ?
			((p_stdoutdata->typeb == BP_IMG) ? "S" : "P")
			: (p_stdoutdata->type == F_IMG ? "F" : "B");
#else
		typ = (p_stdoutdata->type == INTRA_IMG) ? "I" :
			(p_stdoutdata->type == INTER_IMG) ?
			((p_stdoutdata->type == BP_IMG) ? "S" : "P")
			: (p_stdoutdata->type == F_IMG ? "F" : "B");
#endif
	}

#if 0
	/*rain???*/
	pr_info("%3d(%s)  %3d %5d %7.4f %7.4f %7.4f %5d\t\t%s %8d %6d\t%s",
			p_stdoutdata->framenum + hc->total_frames * 256,
			typ, p_stdoutdata->tr + hc->total_frames * 256,
			p_stdoutdata->qp, p_stdoutdata->snr_y,
			p_stdoutdata->snr_u, p_stdoutdata->snr_v,
			p_stdoutdata->tmp_time, Frmfld,
			p_stdoutdata->curr_frame_bits,
			p_stdoutdata->emulate_bits,
			"");
#endif
	if (is_avs2_print_bufmgr_detail())
		pr_info(" %s\n", p_stdoutdata->str_reference_list);

	/*fflush(stdout);*/
	hd->FrameNum++;
}

void avs2_prepare_header(struct avs2_decoder *avs2_dec, int32_t start_code)
{
	struct ImageParameters_s    *img = &avs2_dec->img;
	struct Video_Dec_data_s *hd = &avs2_dec->hd;

	switch (start_code) {
	case SEQUENCE_HEADER_CODE:
		img->new_sequence_flag = 1;
		if (is_avs2_print_bufmgr_detail())
			pr_info("SEQUENCE\n");
#ifdef TO_CHECK
#if SEQ_CHANGE_CHECKER
		if (seq_checker_buf == NULL) {
			seq_checker_buf = malloc(length);
			seq_checker_length = length;
			memcpy(seq_checker_buf, Buf, length);
		} else {
			if ((seq_checker_length != length) ||
				(memcmp(seq_checker_buf, Buf, length) != 0)) {
				free(seq_checker_buf);
				/*fprintf(stdout,
				  "Non-conformance
				  stream: sequence
				  header cannot change
				  !!\n");*/
#if RD170_FIX_BG
				seq_checker_buf = NULL;
				seq_checker_length = 0;
				seq_checker_buf = malloc(length);
				seq_checker_length = length;
				memcpy(seq_checker_buf, Buf, length);
#endif
			}


		}
#endif
#if RD170_FIX_BG
		if (input->alf_enable
			&& alfParAllcoated == 1) {
			ReleaseAlfGlobalBuffer();
			alfParAllcoated = 0;
		}
#endif
/*TO_CHECK*/
#endif
#if FIX_FLUSH_DPB_BY_LF
		if (hd->vec_flag) {
			int32_t k;
			if (is_avs2_print_bufmgr_detail())
				pr_info("vec_flag is 1, flushDPB and reinit bugmgr\n");

			flushDPB(avs2_dec);
			for (k = 0; k < avs2_dec->ref_maxbuffer; k++)
				cleanRefMVBufRef(k);

			hd->vec_flag = 0;
#ifdef AML
			free_unused_buffers(avs2_dec);
#else
			free_global_buffers(avs2_dec);
#endif
			img->number = 0;
			img->PrevPicDistanceLsb = 0;
			avs2_dec->init_hw_flag = 0;
		}
#endif

#if FIX_SEQ_END_FLUSH_DPB_BY_LF
		if (img->new_sequence_flag
			&& img->sequence_end_flag) {
			int32_t k;
			if (is_avs2_print_bufmgr_detail())
				pr_info(
				"new_sequence_flag after sequence_end_flag, flushDPB and reinit bugmgr\n");
			flushDPB(avs2_dec);
			for (k = 0; k < avs2_dec->ref_maxbuffer; k++)
				cleanRefMVBufRef(k);

#ifdef AML
			free_unused_buffers(avs2_dec);
#else
			free_global_buffers(avs2_dec);
#endif
			img->number = 0;
			img->PrevPicDistanceLsb = 0;
			avs2_dec->init_hw_flag = 0;
		}
#endif
		img->seq_header_indicate = 1;
		break;
	case I_PICTURE_START_CODE:
		if (is_avs2_print_bufmgr_detail())
			pr_info("PIC-I\n");
		Get_SequenceHeader(avs2_dec);
		Get_I_Picture_Header(avs2_dec);
		calc_picture_distance(avs2_dec);
		Read_ALF_param(avs2_dec);
		if (!img->seq_header_indicate) {
			img->B_discard_flag = 1;
			/*fprintf(stdout, "    I
			  %3d\t\tDIDSCARD!!\n",
			  img->tr);*/
			break;
		}
		break;
	case PB_PICTURE_START_CODE:
		if (is_avs2_print_bufmgr_detail())
			pr_info("PIC-PB\n");
		Get_SequenceHeader(avs2_dec);
		Get_PB_Picture_Header(avs2_dec);
		calc_picture_distance(avs2_dec);
		Read_ALF_param(avs2_dec);
		/* xiaozhen zheng, 20071009*/
		if (!img->seq_header_indicate) {
			img->B_discard_flag = 1;

			if (img->type == P_IMG) {
				/*fprintf(stdout, "    P
				  %3d\t\tDIDSCARD!!\n",
				  img->tr);*/
			}
			if (img->type == F_IMG) {
				/*fprintf(stdout, "    F
				  %3d\t\tDIDSCARD!!\n",
				  img->tr);*/
			} else {
				/*fprintf(stdout, "    B
				  %3d\t\tDIDSCARD!!\n",
				  img->tr);*/
			}

			break;
		}

		if (img->seq_header_indicate == 1
			&& img->type != B_IMG) {
			img->B_discard_flag = 0;
		}
		if (img->type == B_IMG && img->B_discard_flag == 1
			&& !img->random_access_decodable_flag) {
			/*fprintf(stdout, "    B
			  %3d\t\tDIDSCARD!!\n",
			  img->tr);*/
			break;
		}

		break;
	case SEQUENCE_END_CODE:
		if (is_avs2_print_bufmgr_detail())
			pr_info("SEQUENCE_END_CODE\n");
#ifdef TO_CHECK
#if SEQ_CHANGE_CHECKER
		if (seq_checker_buf != NULL) {
			free(seq_checker_buf);
			seq_checker_buf = NULL;
			seq_checker_length = 0;
		}
#endif
#endif
img->new_sequence_flag = 1;
img->sequence_end_flag = 1;
break;
	case VIDEO_EDIT_CODE:
		if (is_avs2_print_bufmgr_detail())
			pr_info("VIDEO_EDIT_CODE\n");
		/*video_edit_code_data(Buf, startcodepos, length);*/
		hd->vec_flag = 1;
#ifdef TO_CHECK
#if SEQ_CHANGE_CHECKER
		if (seq_checker_buf != NULL) {
			free(seq_checker_buf);
			seq_checker_buf = NULL;
			seq_checker_length = 0;
		}
#endif
#endif

break;
	}
}

#ifdef AML
static uint32_t log2i(uint32_t val)
{
	uint32_t ret = -1;
	while (val != 0) {
		val >>= 1;
		ret++;
	}
	return ret;
}
#endif

int32_t avs2_process_header(struct avs2_decoder *avs2_dec)
{
	struct inp_par    *input = &avs2_dec->input;
	struct ImageParameters_s    *img = &avs2_dec->img;
	struct Video_Dec_data_s *hd = &avs2_dec->hd;
	int32_t lcu_x_num_div;
	int32_t lcu_y_num_div;

	int32_t N8_SizeScale;
	/*pr_info("%s\n", __func__);*/
	{
		N8_SizeScale = 1;

		if (hd->horizontal_size %
			(MIN_CU_SIZE * N8_SizeScale) != 0) {
			img->auto_crop_right =
				(MIN_CU_SIZE * N8_SizeScale) -
				(hd->horizontal_size %
				(MIN_CU_SIZE * N8_SizeScale));
		} else
			img->auto_crop_right = 0;

#if !INTERLACE_CODING
		if (hd->progressive_sequence) /**/
#endif
		{
			if (hd->vertical_size %
				(MIN_CU_SIZE * N8_SizeScale) != 0) {
				img->auto_crop_bottom =
				(MIN_CU_SIZE * N8_SizeScale) -
				(hd->vertical_size %
				(MIN_CU_SIZE * N8_SizeScale));
			} else
				img->auto_crop_bottom = 0;
		}

		/* Reinit parameters (NOTE: need to do
		  before init_frame //*/
		img->width          =
			(hd->horizontal_size + img->auto_crop_right);
		img->height         =
			(hd->vertical_size + img->auto_crop_bottom);
		img->width_cr       = (img->width >> 1);

		if (input->chroma_format == 1)
			img->height_cr      = (img->height >> 1);

		img->PicWidthInMbs  = img->width / MIN_CU_SIZE;
		img->PicHeightInMbs = img->height / MIN_CU_SIZE;
		img->PicSizeInMbs   = img->PicWidthInMbs * img->PicHeightInMbs;
		img->max_mb_nr      = (img->width * img->height) /
			(MIN_CU_SIZE * MIN_CU_SIZE);
	}

	if (img->new_sequence_flag && img->sequence_end_flag) {
#if 0/*RD170_FIX_BG //*/
		int32_t k;
		flushDPB();
		for (k = 0; k < avs2_dec->ref_maxbuffer; k++)
			cleanRefMVBufRef(k);

		free_global_buffers();
		img->number = 0;
#endif
		hd->end_SeqTr = img->tr;
		img->sequence_end_flag = 0;
	}
	if (img->new_sequence_flag) {
		hd->next_IDRtr = img->tr;
		hd->next_IDRcoi = img->coding_order;
		img->new_sequence_flag = 0;
	}
#if 0/*RD170_FIX_BG*/
	if (hd->vec_flag) {
		int32_t k;
		flushDPB();
		for (k = 0; k < avs2_dec->ref_maxbuffer; k++)
			cleanRefMVBufRef(k);

		hd->vec_flag = 0;
		free_global_buffers();
		img->number = 0;
	}
#endif
/* allocate memory for frame buffers*/
#if 0
/* called in vavs2.c*/
	if (img->number == 0)
		avs2_init_global_buffers(avs2_dec);
#endif
	img->current_mb_nr = 0;

	init_frame(avs2_dec);

	img->types = img->type;   /* jlzheng 7.15*/

	if (img->type != B_IMG) {
		hd->pre_img_type = img->type;
		hd->pre_img_types = img->types;
	}

#ifdef AML
	avs2_dec->lcu_size_log2 = log2i(avs2_dec->lcu_size);
	lcu_x_num_div = (img->width/avs2_dec->lcu_size);
	lcu_y_num_div = (img->height/avs2_dec->lcu_size);
	avs2_dec->lcu_x_num = ((img->width % avs2_dec->lcu_size) == 0) ?
		lcu_x_num_div : lcu_x_num_div+1;
	avs2_dec->lcu_y_num = ((img->height % avs2_dec->lcu_size) == 0) ?
		lcu_y_num_div : lcu_y_num_div+1;
	avs2_dec->lcu_total = avs2_dec->lcu_x_num*avs2_dec->lcu_y_num;
#endif
	return SOP;
}

int avs2_post_process(struct avs2_decoder *avs2_dec)
{
	struct ImageParameters_s    *img = &avs2_dec->img;
	struct Video_Com_data_s *hc = &avs2_dec->hc;
	struct Video_Dec_data_s *hd = &avs2_dec->hd;
	int32_t i;
	int ret;
	if (img->typeb == BACKGROUND_IMG && hd->background_picture_enable) {
#ifdef AML
		for (i = 0; i < avs2_dec->ref_maxbuffer; i++) {
			if (avs2_dec->fref[i]->bg_flag != 0) {
				avs2_dec->fref[i]->bg_flag = 0;
				if (is_avs2_print_bufmgr_detail())
					pr_info(
					"clear old BACKGROUND_IMG for index %d\r\n",
					avs2_dec->fref[i]->index);
			}
		}
		if (is_avs2_print_bufmgr_detail())
			pr_info(
			"post_process: set BACKGROUND_IMG flag for %d\r\n",
			hc->cur_pic->index);
		avs2_dec->f_bg = hc->cur_pic;
		hc->cur_pic->bg_flag = 1;
#endif
	}

#if BCBR
	if (hd->background_picture_enable
		&& hd->bcbr_enable && img->number > 0)
		updateBgReference();
#endif

	if (img->typeb == BACKGROUND_IMG &&
		hd->background_picture_output_flag == 0)
		hd->background_number++;

	if (img->type == B_IMG) {
		avs2_dec->fref[0]->imgtr_fwRefDistance
		= hd->trtmp;
	}

	/* record the reference list information*/
	get_reference_list_info(avs2_dec, avs2_dec->hc.str_list_reference);

	/*pr_info("%s\n", __func__);*/
	ret = frame_postprocessing(avs2_dec);

#if FIX_PROFILE_LEVEL_DPB_RPS_1
	/* delete the frame that will never be used*/
	{
		int32_t i, j;
		if (is_avs2_print_bufmgr_detail()) {
			pr_info(
				"%s, coding_order %d to remove %d buf: ",
				__func__,
				img->coding_order,
				hd->curr_RPS.num_to_remove);
			for (i = 0; i < hd->curr_RPS.num_to_remove; i++)
				pr_info("%d ", hd->curr_RPS.remove_pic[i]);
			pr_info("\n");
		}
		for (i = 0; i < hd->curr_RPS.num_to_remove; i++) {
			for (j = 0; j < avs2_dec->ref_maxbuffer; j++) {

				if (avs2_dec->fref[j]->imgcoi_ref >= -256
					&& avs2_dec->fref[j]->imgcoi_ref ==
					img->coding_order -
					hd->curr_RPS.remove_pic[i])
					break;
			}
			if (j < avs2_dec->ref_maxbuffer) { /**/
#if FIX_RPS_PICTURE_REMOVE
/* Label new frames as "un-referenced" */
				avs2_dec->fref[j]->refered_by_others = 0;

				/* remove frames which have been outputted */
				if (avs2_dec->fref[j]->is_output == -1) {
					avs2_dec->fref[j]->
					imgtr_fwRefDistance = -256;
					avs2_dec->fref[j]->imgcoi_ref = -257;
					avs2_dec->fref[j]->temporal_id = -1;

				}
#else
				avs2_dec->fref[j]->imgcoi_ref = -257;
#if M3480_TEMPORAL_SCALABLE
				avs2_dec->fref[j]->temporal_id = -1;
#endif
				if (avs2_dec->fref[j]->is_output == -1) {
					avs2_dec->fref[j]->imgtr_fwRefDistance
						= -256;
				}
#endif
			}
		}
	}
#endif


	/*! TO 19.11.2001 Known Problem: for init_frame
	 * we have to know the picture type of the
	 * actual frame*/
	/*! in case the first slice of the P-Frame
	 * following the I-Frame was lost we decode this
	 * P-Frame but! do not write it because it
	 * was
	 * assumed to be an I-Frame in init_frame.So we
	 * force the decoder to*/
	/*! guess the right picture type. This is a hack
	 * a should be removed by the time there is a
	 * clean*/
	/*! solution where we do not have to know the
	 * picture type for the function init_frame.*/
	/*! End TO 19.11.2001//Lou*/

	{
		if (img->type == I_IMG ||
			img->type == P_IMG ||
			img->type == F_IMG)
			img->number++;
		else {
			hc->Bframe_ctr++;  /* B
					      pictures*/
		}
	}
	return ret;
}

void init_avs2_decoder(struct avs2_decoder *avs2_dec)
{
	int32_t i, j, k;

	struct inp_par    *input = &avs2_dec->input;
	struct ImageParameters_s    *img = &avs2_dec->img;
	struct Video_Com_data_s *hc = &avs2_dec->hc;
	struct Video_Dec_data_s *hd = &avs2_dec->hd;
	if (is_avs2_print_bufmgr_detail())
		pr_info("[t] struct avs2_dec @0x%p\n", avs2_dec);
	memset(avs2_dec, 0, sizeof(struct avs2_decoder));
#ifdef AML
	avs2_dec->to_prepare_disp_count = 1;
#endif
	/*
	 * ALFParam init
	 */
	for (i = 0; i < 3; i++) {
		avs2_dec->m_alfPictureParam[i].alf_flag = 0; /*1*/
		avs2_dec->m_alfPictureParam[i].num_coeff = 9; /*1*/
		avs2_dec->m_alfPictureParam[i].filters_per_group = 3;  /*1*/
		avs2_dec->m_alfPictureParam[i].componentID = i; /*1*/
		for (j = 0; j < 16; j++) {
			avs2_dec->m_alfPictureParam[i].filterPattern[j]	= 0;
			/*16*/
		}
		for (j = 0; j < 16; j++) {
			for (k = 0; k < 9; k++) {
				avs2_dec->
				m_alfPictureParam[i].coeffmulti[j][k] = 0;
				/*16*9*/
			}
		}
	}

	img->seq_header_indicate = 0;
	img->B_discard_flag = 0;

	hd->eos = 0;

	if (input->ref_pic_order) {   /*ref order*/
		hd->dec_ref_num = 0;
	}

	/*
	memset(g_log2size, -1, MAX_CU_SIZE + 1);
	c = 2;
	for (k = 4; k <= MAX_CU_SIZE; k *= 2) {
		g_log2size[k] = c;
		c++;
	}
	 */

	avs2_dec->outprint.buffer_num = 0;

	hd->last_output = -1;
	hd->end_SeqTr = -1;
	hd->curr_IDRtr = 0;
	hd->curr_IDRcoi = 0;
	hd->next_IDRtr = 0;
	hd->next_IDRcoi = 0;
	/* Allocate Slice data struct*/
	img->number = 0;
	img->type = I_IMG;

	img->imgtr_next_P = 0;

	img->imgcoi_next_ref = 0;


	img->num_of_references = 0;
	hc->seq_header = 0;

	img->new_sequence_flag   = 1;

	hd->vec_flag = 0;

	hd->FrameNum = 0;

	/* B pictures*/
	hc->Bframe_ctr = 0;
	hc->total_frames = 0;

	/* time for total decoding session*/
	hc->tot_time = 0;

}

