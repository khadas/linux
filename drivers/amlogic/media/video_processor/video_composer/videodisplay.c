// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * drivers/amlogic/media/video_processor/video_composer/videodisplay.c
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
 #include <linux/amlogic/meson_uvm_core.h>
 #include <linux/amlogic/media/utils/am_com.h>
 #include <linux/amlogic/media/codec_mm/codec_mm.h>
 #ifdef CONFIG_AMLOGIC_MEDIA_VIDEO
#include <linux/amlogic/media/video_sink/video.h>
#endif
#include "videodisplay.h"
#include "video_composer.h"

static struct timeval vsync_time;
static DEFINE_MUTEX(video_display_mutex);
static struct composer_dev *mdev[3];

static int get_count[MAX_VIDEO_COMPOSER_INSTANCE_NUM];
static unsigned int countinue_vsync_count[MAX_VIDEO_COMPOSER_INSTANCE_NUM];

#define PATTERN_32_DETECT_RANGE 7
#define PATTERN_22_DETECT_RANGE 7
#define PATTERN_22323_DETECT_RANGE 5
#define PATTERN_44_DETECT_RANGE 14
#define PATTERN_55_DETECT_RANGE 17

/*if add new patten, need change dev->patten[X]*/
enum video_refresh_pattern {
	PATTERN_32 = 0,
	PATTERN_22,
	PATTERN_22323,
	PATTERN_44,
	PATTERN_55,
	MAX_NUM_PATTERNS
};

static int patten_trace[MAX_VIDEO_COMPOSER_INSTANCE_NUM];
static int vsync_count[MAX_VIDEO_COMPOSER_INSTANCE_NUM];

void vsync_notify_video_composer(void)
{
	int i;
	int count = MAX_VIDEO_COMPOSER_INSTANCE_NUM;

	for (i = 0; i < count; i++) {
		vsync_count[i]++;
		get_count[i] = 0;
		countinue_vsync_count[i]++;
		patten_trace[i]++;
	}
	do_gettimeofday(&vsync_time);
}

static bool vd_vf_is_tvin(struct vframe_s *vf)
{
	if (!vf)
		return false;

	if (vf->source_type == VFRAME_SOURCE_TYPE_HDMI ||
		vf->source_type == VFRAME_SOURCE_TYPE_CVBS ||
		vf->source_type == VFRAME_SOURCE_TYPE_TUNER)
		return true;
	return false;
}

static void vf_pop_display_q(struct composer_dev *dev, struct vframe_s *vf)
{
	struct vframe_s *dis_vf = NULL;

	int k = kfifo_len(&dev->display_q);

	while (kfifo_len(&dev->display_q) > 0) {
		if (kfifo_get(&dev->display_q, &dis_vf)) {
			if (dis_vf == vf)
				break;
			if (!kfifo_put(&dev->display_q, dis_vf))
				vc_print(dev->index, PRINT_ERROR,
					 "display_q is full!\n");
		}
		k--;
		if (k < 0) {
			vc_print(dev->index, PRINT_ERROR,
				 "can find vf in display_q\n");
			break;
		}
	}
}

static void vd_display_q_uninit(struct composer_dev *dev)
{
	struct vframe_s *vf = NULL;
	struct file *file_vf = NULL;
	int repeat_count;
	int i;
	u32 dma_flag = 0;
	struct vd_prepare_s *vd_prepare;
	struct mbp_buffer_info_t *mpb_buf = NULL;

	vc_print(dev->index, PRINT_QUEUE_STATUS,
		"%s: display_q len=%d\n",
		__func__, kfifo_len(&dev->display_q));

	while (kfifo_len(&dev->display_q) > 0) {
		if (kfifo_get(&dev->display_q, &vf)) {
			vd_prepare = container_of(vf,
						struct vd_prepare_s,
						dst_frame);
			if (IS_ERR_OR_NULL(vd_prepare)) {
				vc_print(dev->index, PRINT_ERROR,
					"%s: prepare is NULL.\n",
					__func__);
				return;
			}
			dma_flag = vf->flag & VFRAME_FLAG_VIDEO_COMPOSER_DMA;
			if (vf->flag & VFRAME_FLAG_VIDEO_COMPOSER_BYPASS) {
				repeat_count = vf->repeat_count[dev->index];
				file_vf = vf->file_vf;
				vc_print(dev->index, PRINT_FENCE,
					 "%s: repeat_count=%d, omx_index=%d\n",
					 __func__,
					 repeat_count,
					 vf->omx_index);
				for (i = 0; i <= repeat_count; i++) {
					if (!dma_flag) {
						dma_buf_put((struct dma_buf *)file_vf);
						dma_fence_signal(vd_prepare->release_fence);
						vc_print(dev->index, PRINT_FENCE,
						"%s: release_fence = %px\n",
						__func__,
						vd_prepare->release_fence);
					} else {
						vc_print(dev->index, PRINT_FENCE,
							"put dma bufer!!!\n");
						mpb_buf = (struct mbp_buffer_info_t *)file_vf;
						vf->vc_private->unlock_buffer_cb(mpb_buf);
					}
					dev->fput_count++;
				}
			} else if (!(vf->flag
				     & VFRAME_FLAG_VIDEO_COMPOSER)) {
				vc_print(dev->index, PRINT_ERROR,
					 "%s: display_q flag is null, omx_index=%d\n",
					 __func__, vf->omx_index);
			}
		}
	}
}

static void vd_ready_q_uninit(struct composer_dev *dev)
{
	struct vframe_s *vf = NULL;
	struct file *file_vf = NULL;
	int repeat_count;
	int i;
	struct vd_prepare_s *vd_prepare;
	struct mbp_buffer_info_t *mpb_buf = NULL;
	u32 dma_flag = 0;

	vc_print(dev->index, PRINT_QUEUE_STATUS,
		"%s: ready_q len=%d\n",
		__func__, kfifo_len(&dev->ready_q));

	while (kfifo_len(&dev->ready_q) > 0) {
		if (kfifo_get(&dev->ready_q, &vf)) {
			vd_prepare = container_of(vf,
						struct vd_prepare_s,
						dst_frame);
			if (IS_ERR_OR_NULL(vd_prepare)) {
				vc_print(dev->index, PRINT_ERROR,
					"%s: prepare is NULL.\n",
					__func__);
				return;
			}

			if (!(vf->flag & VFRAME_FLAG_VIDEO_COMPOSER_BYPASS)) {
				vc_print(dev->index, PRINT_OTHER,
					"%s: no need release.\n",
					__func__);
				return;
			}

			repeat_count = vf->repeat_count[dev->index];
			file_vf = vf->file_vf;
			dma_flag = vf->flag & VFRAME_FLAG_VIDEO_COMPOSER_DMA;
			for (i = 0; i <= repeat_count; i++) {
				if (dma_flag) {
					vc_print(dev->index, PRINT_FENCE,
						"put dma bufer!!!\n");
					mpb_buf = (struct mbp_buffer_info_t *)file_vf;
					if (IS_ERR_OR_NULL(mpb_buf)) {
						vc_print(dev->index, PRINT_ERROR,
							"%s: mpb_buf is NULL.\n",
							__func__);
						continue;
					}
					vf->vc_private->unlock_buffer_cb(mpb_buf);
				} else {
					dma_buf_put((struct dma_buf *)file_vf);
					dma_fence_signal(vd_prepare->release_fence);
					vc_print(dev->index, PRINT_FENCE,
						"%s: release_fence = %px\n",
						__func__,
						vd_prepare->release_fence);
				}
				dev->fput_count++;
			}
		}
	}
}

void video_dispaly_push_ready(struct composer_dev *dev, struct vframe_s *vf)
{
	u32 vsync_index = vsync_count[dev->index];

	if (vf && vf->vc_private) {
		vf->vc_private->vsync_index = vsync_index;
		vc_print(dev->index, PRINT_OTHER,
			"set vsync_index =%d\n", vsync_index);
	}
}

static void vd_vsync_video_pattern(struct composer_dev *dev, int pattern, struct vframe_s *vf)
{
	int factor1 = 0, factor2 = 0;
	int pattern_range = 0;

	if (pattern >= MAX_NUM_PATTERNS)
		return;

	if (pattern == PATTERN_32) {
		factor1 = 3;
		factor2 = 2;
		pattern_range = PATTERN_32_DETECT_RANGE;
	} else if (pattern == PATTERN_22) {
		factor1 = 2;
		factor2 = 2;
		pattern_range =  PATTERN_22_DETECT_RANGE;
	}  else if (pattern == PATTERN_44) {
		factor1 = 4;
		factor2 = 4;
		pattern_range =  PATTERN_44_DETECT_RANGE;
	}  else if (pattern == PATTERN_55) {
		factor1 = 5;
		factor2 = 5;
		pattern_range =  PATTERN_55_DETECT_RANGE;
	} else if (pattern >= MAX_NUM_PATTERNS) {
		pr_err("not support pattern %d\n", pattern);
		return;
	}

	/* update 3:2 or 2:2 mode detection */
	if ((dev->pre_pat_trace == factor1 && patten_trace[dev->index] == factor2) ||
	    (dev->pre_pat_trace == factor2 && patten_trace[dev->index] == factor1)) {
		if (dev->pattern[pattern] < pattern_range) {
			dev->pattern[pattern]++;
			if (dev->pattern[pattern] == pattern_range) {
				dev->pattern_enter_cnt++;
				dev->pattern_detected = pattern;
				vc_print(dev->index, PRINT_PATTERN,
					"patten: video %d:%d mode detected\n",
					factor1, factor2);
			}
		}
	} else if (dev->pattern[pattern] == pattern_range) {
		if (pattern == PATTERN_22 &&
			patten_trace[dev->index] == 3 &&
			dev->pre_pat_trace == 2 &&
			vf->omx_index == dev->last_vf_index + 1) {
			patten_trace[dev->index] = 2;
			vc_print(dev->index, PRINT_PATTERN,
				"patten: video %d:%d mode force unbroken, pre_pat=%d, %d, index=%d, %d\n",
				factor1, factor2, dev->pre_pat_trace,
				patten_trace[dev->index],
				vf->omx_index, dev->last_vf_index);
			return;
		}
		dev->pattern[pattern] = 0;
		dev->pattern_exit_cnt++;
		vc_print(dev->index, PRINT_PATTERN,
			"patten: video %d:%d mode broken, pre_pat=%d, patten =%d, index=%d, %d\n",
			factor1, factor2, dev->pre_pat_trace, patten_trace[dev->index],
			vf->omx_index, dev->last_vf_index);
	} else {
		dev->pattern[pattern] = 0;
	}
}

static void vd_vsync_video_pattern_22323(struct composer_dev *dev, struct vframe_s *vf)
{
	int factor1 = 2;
	int factor2 = 2;
	int factor3 = 3;
	int factor4 = 2;
	int factor5 = 3;
	int pattern = PATTERN_22323;
	int pattern_range = PATTERN_22323_DETECT_RANGE;
	bool check_ok = false;
	int i = 0;
	int index_1;
	int index_2;
	int index_3;
	int index_4;
	int index_5;
	int cur_factor_index = dev->patten_factor_index;
	int vsync_pts_inc = 16 * 90000 * vsync_pts_inc_scale / vsync_pts_inc_scale_base;
	int vframe_duration = vf->duration * 15;

	if (vsync_pts_inc * 12 != vframe_duration * 5)
		return;

	for (i = 0; i < PATTEN_FACTOR_MAX; i++) {
		index_1 = i;

		index_2 = i + 1;
		if (index_2 >= PATTEN_FACTOR_MAX)
			index_2 -= PATTEN_FACTOR_MAX;

		index_3 = i + 2;
		if (index_3 >= PATTEN_FACTOR_MAX)
			index_3 -= PATTEN_FACTOR_MAX;

		index_4 = i + 3;
		if (index_4 >= PATTEN_FACTOR_MAX)
			index_4 -= PATTEN_FACTOR_MAX;

		index_5 = i + 4;
		if (index_5 >= PATTEN_FACTOR_MAX)
			index_5 -= PATTEN_FACTOR_MAX;

		if (dev->patten_factor[index_1] == factor1 &&
			dev->patten_factor[index_2] == factor2 &&
			dev->patten_factor[index_3] == factor3 &&
			dev->patten_factor[index_4] == factor4 &&
			dev->patten_factor[index_5] == factor5) {
			check_ok = true;
			if (cur_factor_index == index_1)
				dev->next_factor = factor2;
			else if (cur_factor_index == index_2)
				dev->next_factor = factor3;
			else if (cur_factor_index == index_3)
				dev->next_factor = factor4;
			else if (cur_factor_index == index_4)
				dev->next_factor = factor5;
			else if (cur_factor_index == index_5)
				dev->next_factor = factor1;
			break;
		}
	}

	if (check_ok) {
		if (dev->pattern[pattern] < pattern_range) {
			dev->pattern[pattern]++;
			if (dev->pattern[pattern] == pattern_range) {
				dev->pattern_enter_cnt++;
				dev->pattern_detected = pattern;
				vc_print(dev->index, PRINT_PATTERN,
					"patten: video 22323 mode detected\n");
			}
		}
	} else if (dev->pattern[pattern] == pattern_range) {
		dev->pattern[pattern] = 0;
		dev->pattern_exit_cnt++;
		vc_print(dev->index, PRINT_PATTERN,
			"patten: video 22323 mode broken, pre_pat=%d, patten =%d, index=%d, %d\n",
			dev->pre_pat_trace, patten_trace[dev->index],
			vf->omx_index, dev->last_vf_index);
	} else {
		dev->pattern[pattern] = 0;
	}
}

static void vsync_video_pattern(struct composer_dev *dev, struct vframe_s *vf)
{
	vd_vsync_video_pattern(dev, PATTERN_32, vf);
	vd_vsync_video_pattern(dev, PATTERN_22, vf);
	vd_vsync_video_pattern_22323(dev, vf);
	vd_vsync_video_pattern(dev, PATTERN_44, vf);
	vd_vsync_video_pattern(dev, PATTERN_55, vf);
	/*vd_vsync_video_pattern(dev, PTS_41_PATTERN);*/
}

static inline int vd_perform_pulldown(struct composer_dev *dev,
					 struct vframe_s *vf,
					 bool *expired)
{
	int pattern_range, expected_curr_interval;
	int expected_prev_interval;

	/* Dont do anything if we have invalid data */
	if (!vf || !vf->vc_private)
		return -1;
	if (vf->vc_private->vsync_index == 0)
		return -1;

	if (vd_vf_is_tvin(vf))
		return -1;

	switch (dev->pattern_detected) {
	case PATTERN_32:
		pattern_range = PATTERN_32_DETECT_RANGE;
		switch (dev->pre_pat_trace) {
		case 3:
			expected_prev_interval = 3;
			expected_curr_interval = 2;
			break;
		case 2:
			expected_prev_interval = 2;
			expected_curr_interval = 3;
			break;
		default:
			return -1;
		}
		break;
	case PATTERN_22:
		if (dev->pre_pat_trace != 2)
			vc_print(dev->index, PRINT_PATTERN,
				"patten:: pre_pat_trace =%d",
				dev->pre_pat_trace);

		pattern_range =  PATTERN_22_DETECT_RANGE;
		expected_prev_interval = 2;
		expected_curr_interval = 2;
		break;
	case PATTERN_22323:
		pattern_range =  PATTERN_22323_DETECT_RANGE;
		expected_curr_interval = dev->next_factor;
		break;
	case PATTERN_44:
		pattern_range =  PATTERN_44_DETECT_RANGE;
		expected_prev_interval = 4;
		expected_curr_interval = 4;
		break;
	case PATTERN_55:
		pattern_range =  PATTERN_55_DETECT_RANGE;
		expected_prev_interval = 5;
		expected_curr_interval = 5;
		break;
	default:
		return -1;
	}

	vc_print(dev->index, PRINT_PATTERN,
		"patten: detected %d, pattern =%d, patten_trace=%d, expected=%d expired=%d\n",
		dev->pattern_detected,
		dev->pattern[dev->pattern_detected],
		patten_trace[dev->index],
		expected_curr_interval,
		*expired);

	/* We do nothing if we dont have enough data*/
	if (dev->pattern[dev->pattern_detected] != pattern_range)
		return -1;

	if (*expired) {
		if (patten_trace[dev->index] < expected_curr_interval) {
			/* 2323232323..2233..2323, prev=2, curr=3,*/
			/* check if next frame will toggle after 3 vsyncs */
			/* 22222...22222 -> 222..2213(2)22...22 */
			/* check if next frame will toggle after 3 vsyncs */
			*expired = false;
			vc_print(dev->index, PRINT_PATTERN,
				"patten:hold frame for pattern: %d",
				dev->pattern_detected);
		}
	} else {
		if (patten_trace[dev->index] >= expected_curr_interval) {
			/* 23232323..233223...2323 curr=2, prev=3 */
			/* check if this frame will expire next vsyncs and */
			/* next frame will expire after 3 vsyncs */
			/* 22222...22222 -> 222..223122...22 */
			/* check if this frame will expire next vsyncs and */
			/* next frame will expire after 2 vsyncs */
			*expired = true;
			vc_print(dev->index, PRINT_PATTERN,
				"patten: pull frame for pattern: %d",
				dev->pattern_detected);
		}
	}
	return 0;
}

bool pulldown_support_vf(u32 duration)
{
	bool support = false;

	/*duration: 800(120fps) 801(119.88fps) 960(100fps) 1600(60fps) 1920(50fps)*/
	/*3200(30fps) 3203(29.97) 3840(25fps) 4000(24fps) 4004(23.976fps)*/

	if (vsync_pts_inc_scale == 1 && vsync_pts_inc_scale_base == 48) {
		/*48hz for 24fps 23.976fps*/
		if (duration == 4004 || duration == 4000)
			support = true;
	} else if (vsync_pts_inc_scale == 1 && vsync_pts_inc_scale_base == 50) {
		/*50hz for 25fps*/
		if (duration == 3840)
			support = true;
	} else if (vsync_pts_inc_scale == 1001 && vsync_pts_inc_scale_base == 60000) {
		/*59.94hz for 23.976, 29.97*/
		if (duration == 4004 ||
			duration == 3203)
			support = true;
	} else if (vsync_pts_inc_scale == 1 && vsync_pts_inc_scale_base == 60) {
		/*60hz for 23.976, 24, 25, 29.97, 30*/
		if (duration == 4004 ||
			duration == 4000 ||
			duration == 3840 ||  /*22323 patten for projector, that vout always 60*/
			duration == 3203 ||
			duration == 3200)
			support = true;
	} else if (vsync_pts_inc_scale == 1 && vsync_pts_inc_scale_base == 100) {
		/*100hz for 25fps, 50fps*/
		if (duration == 3840 || duration == 1920)
			support = true;
	} else if (vsync_pts_inc_scale == 1001 && vsync_pts_inc_scale_base == 120000) {
		/*119.88hz for 23.976,29.97,59.94*/
		if (duration == 4004 ||
			duration == 3203 ||
			duration == 1601)
			support = true;
	} else if (vsync_pts_inc_scale == 1 && vsync_pts_inc_scale_base == 120) {
		/*120hz for 23.976, 24, 29.97, 30, 59.94, 60*/
		if (duration == 4004 ||
			duration == 4000 ||
			duration == 3203 ||
			duration == 3200 ||
			duration == 1601 ||
			duration == 1600)
			support = true;
	}
	return support;
}

/* -----------------------------------------------------------------
 *           provider opeations
 * -----------------------------------------------------------------
 */
static struct vframe_s *vc_vf_peek(void *op_arg)
{
	struct composer_dev *dev = (struct composer_dev *)op_arg;
	struct vframe_s *vf = NULL;
	struct timeval time1;
	struct timeval time2;
	u64 time_vsync;
	int interval_time;
	bool expired = true;
	bool expired_tmp = true;
	bool open_pulldown = false;
	int ready_len;
	u32 vsync_index = 0;
	int ret;
	int max_delay_count = 2;

	time1 = dev->start_time;
	time2 = vsync_time;

	if (kfifo_peek(&dev->ready_q, &vf)) {
		if (vf->vc_private) {
			vsync_index = vf->vc_private->vsync_index;
			vc_print(dev->index, PRINT_OTHER,
				"peek: vsync_index =%d, delay_count=%d, vsync_count=%d\n",
				vsync_index, vd_set_frame_delay[dev->index],
				vsync_count[dev->index]);
			if (vsync_index + vd_set_frame_delay[dev->index] - 1
				>= vsync_count[dev->index])
				return NULL;
		} else {
			vc_print(dev->index, PRINT_OTHER,
				"peek: vf->vc_private is NULL\n");
		}

		/*apk/sf drop 0/3 4; vc receive 1 2 5 in one vsync*/
		/*apk queue 5 and wait 1, it will fence timeout*/
		if (get_count[dev->index] == 2) {
			vc_print(dev->index, PRINT_ERROR,
				 "has already get 2, can not get more\n");
			return NULL;
		}
		if (get_count[dev->index] > 0 &&
			!(vf->flag & VFRAME_FLAG_GAME_MODE)) {
			time_vsync = (u64)1000000
				* (time2.tv_sec - time1.tv_sec)
				+ time2.tv_usec - time1.tv_usec;
			interval_time = abs(time_vsync - vf->pts_us64);
			vc_print(dev->index, PRINT_PERFORMANCE,
				 "time_vsync=%lld, vf->pts_us64=%lld\n",
				 time_vsync, vf->pts_us64);
			//TODO
			if (interval_time < 2000/*margin_time*/) {
				vc_print(dev->index, PRINT_PATTERN,
					 "display next vsync\n");
				return NULL;
			}
		}

		if (dev->enable_pulldown && pulldown_support_vf(vf->duration)) {
			open_pulldown = true;
			ready_len = kfifo_len(&dev->ready_q);
			if ((ready_len > 1 && vd_pulldown_level == 1) ||
				(ready_len > 2 && vd_pulldown_level > 1)) {
				open_pulldown = false;
				vc_print(dev->index, PRINT_PATTERN,
					"ready_q len=%d\n", ready_len);
			}
		}

		if (open_pulldown &&
			!vd_vf_is_tvin(vf) &&
			!(vf->flag & VFRAME_FLAG_FAKE_FRAME)) {
			if (vf->vc_private) {
				vsync_index = vf->vc_private->vsync_index;
				vc_print(dev->index, PRINT_PATTERN,
					"peek: vsync_index: %d, vsync_count:%d, omx_index=%d\n",
					vf->vc_private->vsync_index, vsync_count[dev->index],
					vf->omx_index);
				if (vsync_index + 1 >= vsync_count[dev->index])
					expired = false;
			}
			expired_tmp = expired;
			ret = vd_perform_pulldown(dev, vf, &expired);
			if (!expired) {
				if (vd_pulldown_level > 1)
					max_delay_count += vd_pulldown_level - 1;
				if (vsync_index + max_delay_count < vsync_count[dev->index]) {
					vc_print(dev->index, PRINT_PATTERN,
						"need disp, vsync_index =%d, vsync_count=%d\n",
						vsync_index, vsync_count[dev->index]);
					expired = true;
				} else if (expired_tmp) {
					if (dev->last_hold_index + 1 == vf->omx_index)
						dev->continue_hold_count++;
					else if (dev->last_hold_index != vf->omx_index)
						dev->continue_hold_count = 1;
					vc_print(dev->index, PRINT_PATTERN,
						"patten: hold, omx_index =%d, continue_count=%d\n",
						vf->omx_index, dev->continue_hold_count);
					dev->last_hold_index = vf->omx_index;
					if (dev->continue_hold_count >= vd_max_hold_count) {
						expired = true;
						vc_print(dev->index, PRINT_PATTERN,
						"patten: can not hold too many vf\n");
					}
				}
				if (!expired)
					return NULL;
			}
		}

		if (dev->is_drm_enable)
			return vf;
		else
			return videocomposer_vf_peek(op_arg);
	} else {
		return NULL;
	}
}

static struct vframe_s *vc_vf_get(void *op_arg)
{
	struct composer_dev *dev = (struct composer_dev *)op_arg;
	struct vframe_s *vf = NULL;
	u32 vsync_index_diff = 0;

	if (kfifo_get(&dev->ready_q, &vf)) {
		if (!vf)
			return NULL;

		if (vf->flag & VFRAME_FLAG_FAKE_FRAME)
			return vf;

		if (!kfifo_put(&dev->display_q, vf))
			vc_print(dev->index, PRINT_ERROR,
				 "display_q is full!\n");
		if (!(vf->flag
		      & VFRAME_FLAG_VIDEO_COMPOSER)) {
			pr_err("vc: vf_get: flag is null\n");
		}

		get_count[dev->index]++;
		dev->fget_count++;

		if (vf->vc_private) {
			vsync_index_diff = vf->vc_private->vsync_index - dev->last_vsync_index;
			dev->last_vsync_index = vf->vc_private->vsync_index;
			if (vf->omx_index < dev->last_vf_index) {
				vc_print(dev->index, PRINT_PATTERN,
					 "change source\n");
				vf->vc_private->flag |= VC_FLAG_FIRST_FRAME;
			}
		}

		vc_print(dev->index, PRINT_OTHER | PRINT_PATTERN,
			 "get:omx_index=%d, index_disp=%d, get=%d, total_get=%lld, vsync =%d, diff=%d, duration=%d\n",
			 vf->omx_index,
			 vf->index_disp,
			 get_count[dev->index],
			 dev->fget_count,
			 countinue_vsync_count[dev->index],
			 vsync_index_diff,
			 vf->duration);

		if (vf->vc_private)
			vf->vc_private->last_disp_count =
				countinue_vsync_count[dev->index];

		if (dev->enable_pulldown) {
			dev->patten_factor_index++;
			if (dev->patten_factor_index == PATTEN_FACTOR_MAX)
				dev->patten_factor_index = 0;
			dev->patten_factor[dev->patten_factor_index] = patten_trace[dev->index];

			vsync_video_pattern(dev, vf);
			dev->pre_pat_trace = patten_trace[dev->index];
			patten_trace[dev->index] = 0;
		}

		countinue_vsync_count[dev->index] = 0;
		dev->last_vf_index = vf->omx_index;
		return vf;
	} else {
		return NULL;
	}
}

static void vc_vf_put(struct vframe_s *vf, void *op_arg)
{
	int repeat_count;
	int i;
	struct file *file_vf;
	struct composer_dev *dev = (struct composer_dev *)op_arg;
	struct vd_prepare_s *vd_prepare_tmp;
	struct mbp_buffer_info_t *mpb_buf = NULL;

	if (!vf)
		return;

	if (dev->is_drm_enable) {
		if (vf->flag & VFRAME_FLAG_FAKE_FRAME) {
			vc_print(dev->index, PRINT_OTHER,
				 "put: fake frame\n");
			return;
		}

		vd_prepare_tmp = container_of(vf,
					struct vd_prepare_s,
					dst_frame);
		if (IS_ERR_OR_NULL(vd_prepare_tmp)) {
			vc_print(dev->index, PRINT_ERROR,
				"%s: prepare is NULL.\n",
				__func__);
			return;
		}

		repeat_count = vf->repeat_count[dev->index];
		file_vf = vf->file_vf;

		vf_pop_display_q(dev, vf);
		for (i = 0; i <= repeat_count; i++) {
			if (!file_vf) {
				vc_print(dev->index, PRINT_ERROR,
					"put: file error!!!\n");
			}

			if (vf->flag & VFRAME_FLAG_VIDEO_COMPOSER_DMA) {
				vc_print(dev->index, PRINT_FENCE,
					"put dma bufer!!!\n");
				mpb_buf = (struct mbp_buffer_info_t *)file_vf;
				if (IS_ERR_OR_NULL(mpb_buf)) {
					vc_print(dev->index, PRINT_ERROR,
						"%s: mpb_buf is NULL.\n",
						__func__);
					continue;
				}
				vf->vc_private->unlock_buffer_cb(mpb_buf);
			} else {
				dma_buf_put((struct dma_buf *)file_vf);
				dma_fence_signal(vd_prepare_tmp->release_fence);
				vc_print(dev->index, PRINT_FENCE,
					"%s: release_fence = %px\n",
					__func__,
					vd_prepare_tmp->release_fence);
			}

			dev->fput_count++;
		}
		vd_prepare_data_q_put(dev, vd_prepare_tmp);
		if (vf->vc_private) {
			vc_private_q_recycle(dev, vf->vc_private);
			vf->vc_private = NULL;
		}
		vc_print(dev->index, PRINT_OTHER | PRINT_PATTERN,
			"%s: omx_index=%d, put_count=%lld.\n",
			__func__, vf->omx_index, dev->fput_count);
	} else {
		vf_pop_display_q(dev, vf);
		videocomposer_vf_put(vf, op_arg);
	}
}

static int vc_event_cb(int type, void *data, void *private_data)
{
	if (type & VFRAME_EVENT_RECEIVER_PUT)
		;
	else if (type & VFRAME_EVENT_RECEIVER_GET)
		;
	else if (type & VFRAME_EVENT_RECEIVER_FRAME_WAIT)
		;
	return 0;
}

static int vc_vf_states(struct vframe_states *states, void *op_arg)
{
	struct composer_dev *dev = (struct composer_dev *)op_arg;

	states->vf_pool_size = COMPOSER_READY_POOL_SIZE;
	states->buf_recycle_num = 0;
	states->buf_free_num = COMPOSER_READY_POOL_SIZE
		- kfifo_len(&dev->ready_q);
	states->buf_avail_num = kfifo_len(&dev->ready_q);
	return 0;
}

static const struct vframe_operations_s vc_vf_provider = {
	.peek = vc_vf_peek,
	.get = vc_vf_get,
	.put = vc_vf_put,
	.event_cb = vc_event_cb,
	.vf_states = vc_vf_states,
};

static struct vframe_s *vd_get_vf_from_buf(struct composer_dev *dev,
						struct dma_buf *buf)
{
	struct vframe_s *vf = NULL;
	struct vframe_s *di_vf = NULL;
	bool is_dec_vf = false;
	struct uvm_hook_mod *uhmod;
	struct file_private_data *temp_file;

	if (IS_ERR_OR_NULL(buf)) {
		vc_print(dev->index, PRINT_ERROR,
			"%s: dma_buf is NULL.\n",
			__func__);
		return vf;
	}

	is_dec_vf = is_valid_mod_type(buf, VF_SRC_DECODER);
	if (is_dec_vf) {
		vc_print(dev->index, PRINT_OTHER, "vf is from decoder.\n");
		vf = dmabuf_get_vframe(buf);
		vc_print(dev->index, PRINT_OTHER,
			"vframe_type = 0x%x, vframe_flag = 0x%x.\n",
			vf->type,
			vf->flag);

		di_vf = vf->vf_ext;
		if (di_vf && (vf->flag & VFRAME_FLAG_CONTAIN_POST_FRAME)) {
			vc_print(dev->index, PRINT_OTHER,
				"di_vf->type = 0x%x, di_vf->org = 0x%x.\n",
				di_vf->type,
				di_vf->type_original);
			vc_print(dev->index, PRINT_OTHER, "use di vf.\n");
			/* link uvm vf into di_vf->vf_ext */
			if (!di_vf->vf_ext)
				di_vf->vf_ext = vf;
			vf = di_vf;
		}
		dmabuf_put_vframe(buf);
	} else {
		vc_print(dev->index, PRINT_OTHER, "vf is from v4lvideo.\n");
		uhmod = uvm_get_hook_mod(buf, VF_PROCESS_V4LVIDEO);
		if (!uhmod) {
			vc_print(dev->index, PRINT_ERROR,
				"get vframe from v4lvideo failed.\n");
			return vf;
		}

		if (IS_ERR_VALUE(uhmod) || !uhmod->arg) {
			vc_print(dev->index, PRINT_ERROR,
				 "vframe in v4lvideo is NULL.\n");
			return vf;
		}
		temp_file = uhmod->arg;
		vf = &temp_file->vf;
		uvm_put_hook_mod(buf, VF_PROCESS_V4LVIDEO);
	}

	return vf;
}

static void vd_disable_video_layer(struct composer_dev *dev, int val)
{
	vc_print(dev->index, PRINT_ERROR, "%s: val is %d.\n", __func__, val);
	if (dev->index == 0) {
		_video_set_disable(val);
	} else {
		_videopip_set_disable(dev->index, val);
	}
}

void vd_prepare_data_q_put(struct composer_dev *dev,
				struct vd_prepare_s *vd_prepare)
{
	if (!vd_prepare)
		return;

	if (!kfifo_put(&dev->vc_prepare_data_q, vd_prepare))
		vc_print(dev->index, PRINT_ERROR,
			"%s: vc_prepare_data_q is full!\n",
			__func__);
}

struct vd_prepare_s *vd_prepare_data_q_get(struct composer_dev *dev)
{
	struct vd_prepare_s *vd_prepare = NULL;

	if (!kfifo_get(&dev->vc_prepare_data_q, &vd_prepare)) {
		vc_print(dev->index, PRINT_ERROR,
			 "%s: get vc_prepare_data failed\n",
			 __func__);
		vd_prepare = NULL;
	} else {
		vd_prepare->src_frame = NULL;
		memset(&vd_prepare->dst_frame, 0, sizeof(struct vframe_s));
		vd_prepare->release_fence = NULL;
	}

	return vd_prepare;
}

int vd_render_index_get(struct composer_dev *dev)
{
	int reveiver_id = 0;
	int render_index = 0;

	if (!dev) {
		pr_info("%s: dev is null.\n", __func__);
	} else {
		if (dev->index >= MAX_VD_LAYERS) {
			vc_print(dev->index, PRINT_ERROR,
				"%s: invalid param.\n",
				__func__);
		} else {
			reveiver_id = get_receiver_id(dev->index);
			if (reveiver_id >= 5)
				render_index = reveiver_id - 3;
			else if (reveiver_id <= 3)
				render_index = reveiver_id - 2;
			else
				render_index = 0;
		}
		vc_print(dev->index, PRINT_ERROR,
			"%s: render_index is %d.\n",
			__func__, render_index);
	}
	return render_index;
}

int video_display_create_path(struct composer_dev *dev)
{
	int i = 0;
	char render_layer[16] = "";

	sprintf(render_layer, "video_render.%d", dev->video_render_index);
	snprintf(dev->vfm_map_chain, VCOM_MAP_NAME_SIZE,
		 "%s %s", dev->vf_provider_name, render_layer);

	snprintf(dev->vfm_map_id, VCOM_MAP_NAME_SIZE,
		 "vcom-map-%d", dev->index);

	if (vfm_map_add(dev->vfm_map_id, dev->vfm_map_chain) < 0) {
		vc_print(dev->index, PRINT_ERROR,
			"%s: vcom pipeline map creation failed %s.\n",
			__func__, dev->vfm_map_id);
		dev->vfm_map_id[0] = 0;
		return -ENOMEM;
	}

	vf_provider_init(&dev->vc_vf_prov, dev->vf_provider_name,
			 &vc_vf_provider, dev);

	vf_reg_provider(&dev->vc_vf_prov);

	vf_notify_receiver(dev->vf_provider_name,
			   VFRAME_EVENT_PROVIDER_START, NULL);

	vsync_count[dev->index] = 0;
	dev->last_vsync_index = 0;
	dev->last_vf_index = 0xffffffff;
	dev->enable_pulldown = false;
	for (i = 0; i < PATTEN_FACTOR_MAX; i++)
		dev->patten_factor[i] = 0;
	dev->patten_factor_index = 0;
	if (vd_pulldown_level && frc_get_video_latency() != 0) {
		dev->enable_pulldown = true;
		vc_print(dev->index, PRINT_ERROR,
			"enable pulldown\n");
	}
	return 0;
}

int video_display_release_path(struct composer_dev *dev)
{
	vf_unreg_provider(&dev->vc_vf_prov);

	if (dev->vfm_map_id[0]) {
		vfm_map_remove(dev->vfm_map_id);
		dev->vfm_map_id[0] = 0;
	}
	return 0;
}

static struct composer_dev *video_display_getdev(int layer_index)
{
	struct composer_dev *dev;
	struct video_composer_port_s *port;

	vc_print(layer_index, PRINT_OTHER,
		"%s: video_composerdev_%d.\n",
		__func__, layer_index);
	if (layer_index >= MAX_VIDEO_COMPOSER_INSTANCE_NUM) {
		vc_print(layer_index, PRINT_ERROR,
			"%s: layer-%d out of range.\n",
			__func__, layer_index);
		return NULL;
	}

	if (!mdev[layer_index]) {
		mutex_lock(&video_display_mutex);
		dev = kzalloc(sizeof(*dev), GFP_KERNEL);
		if (!dev) {
			mutex_unlock(&video_display_mutex);
			vc_print(layer_index, PRINT_ERROR,
				"%s: alloc dev failed.\n",
				__func__);
			return NULL;
		}

		port = video_composer_get_port(layer_index);
		dev->port = port;
		dev->index = port->index;
		mdev[layer_index] = dev;
		mutex_unlock(&video_display_mutex);
	}

	return mdev[layer_index];
}

static int video_display_init(int layer_index)
{
	int ret = 0, i = 0;
	struct composer_dev *dev;
	char render_layer[16] = "";

	dev = video_display_getdev(layer_index);
	if (!dev) {
		pr_info("%s: get dev failed.\n", __func__);
		return -EBUSY;
	}

	mutex_lock(&video_display_mutex);
	INIT_KFIFO(dev->ready_q);
	INIT_KFIFO(dev->display_q);
	INIT_KFIFO(dev->vc_prepare_data_q);
	kfifo_reset(&dev->ready_q);
	kfifo_reset(&dev->display_q);
	kfifo_reset(&dev->vc_prepare_data_q);
	vc_private_q_init(dev);

	for (i = 0; i < COMPOSER_READY_POOL_SIZE; i++)
		vd_prepare_data_q_put(dev, &dev->vd_prepare[i]);

	vsync_count[dev->index] = 0;
	dev->fput_count = 0;
	dev->drop_frame_count = 0;
	dev->fget_count = 0;
	dev->received_count = 0;
	dev->last_file = NULL;
	dev->vd_prepare_last = NULL;
	dev->is_drm_enable = true;
	dev->video_render_index = vd_render_index_get(dev);
	memcpy(dev->vf_provider_name,
		dev->port->name,
		strlen(dev->port->name) + 1);
	dev->port->open_count++;
	do_gettimeofday(&dev->start_time);
	mutex_unlock(&video_display_mutex);

	vd_disable_video_layer(dev, 2);
	video_set_global_output(dev->index, 1);
	ret = video_display_create_path(dev);
	sprintf(render_layer, "video_render.%d", dev->video_render_index);
	set_video_path_select(render_layer, dev->index);

	return ret;
}

static int video_display_uninit(int layer_index)
{
	int ret;
	struct composer_dev *dev;

	dev = video_display_getdev(layer_index);
	if (!dev) {
		vc_print(layer_index, PRINT_ERROR,
			"%s: get dev failed.\n",
			__func__);
		return -EBUSY;
	}
	if (dev->index == 0)
		set_video_path_select("default", 0);
	set_blackout_policy(1);
	vd_disable_video_layer(dev, 1);
	video_set_global_output(dev->index, 0);
	ret = video_display_release_path(dev);
	vd_ready_q_uninit(dev);
	vd_display_q_uninit(dev);
	vc_print(dev->index, PRINT_OTHER | PRINT_PATTERN,
		 "%s: total put count is %lld.\n",
		 __func__, dev->fput_count);

	dev->port->open_count--;
	kfree(dev);
	mdev[layer_index] = NULL;
	return ret;
}

static unsigned long get_dma_phy_addr(struct dma_buf *dbuf,
					struct composer_dev *dev)
{
	unsigned long phy_addr = 0;
	struct sg_table *table = NULL;
	struct page *page = NULL;
	struct dma_buf_attachment *attach = NULL;

	if (IS_ERR_OR_NULL(dbuf) || IS_ERR_OR_NULL(dev)) {
		vc_print(dev->index, PRINT_ERROR,
			 "%s: param is NULL.\n",
			 __func__);
		return 0;
	}

	attach = dma_buf_attach(dbuf, dev->port->pdev);
	if (IS_ERR(attach))
		return 0;

	table = dma_buf_map_attachment(attach, DMA_BIDIRECTIONAL);
	page = sg_page(table->sgl);
	phy_addr = PFN_PHYS(page_to_pfn(page));
	dma_buf_unmap_attachment(attach, table, DMA_BIDIRECTIONAL);
	dma_buf_detach(dbuf, attach);
	return phy_addr;
}

int video_display_setenable(int layer_index, int is_enable)
{
	int ret = 0;
	struct composer_dev *dev;

	if (is_enable > VIDEO_DISPLAY_ENABLE_NORMAL ||
	    is_enable < VIDEO_DISPLAY_ENABLE_NONE) {
		vc_print(layer_index, PRINT_ERROR,
			"%s: invalid param.\n",
			__func__);
		return -EINVAL;
	}

	vc_print(layer_index, PRINT_ERROR,
		"%s: val=%d.\n",
		 __func__, is_enable);

	dev = video_display_getdev(layer_index);
	if (!dev) {
		vc_print(layer_index, PRINT_ERROR,
			"%s: get dev failed.\n",
			__func__);
		return -EBUSY;
	}

	if (dev->enable_composer == is_enable) {
		vc_print(dev->index, PRINT_ERROR,
			"%s: same status, no need set.\n",
			__func__);
		return ret;
	}

	dev->enable_composer = is_enable;
	if (is_enable == VIDEO_DISPLAY_ENABLE_NORMAL)
		ret = video_display_init(layer_index);
	else
		ret = video_display_uninit(layer_index);

	return ret;
}
EXPORT_SYMBOL(video_display_setenable);

int video_display_setframe(int layer_index,
			struct video_display_frame_info_t *frame_info,
			int flags)
{
	struct composer_dev *dev;
	struct vframe_s *vf = NULL;
	int ready_count = 0;
	bool is_dec_vf = false, is_v4l_vf = false, is_repeat_vf = false;
	struct vd_prepare_s *vd_prepare = NULL;

	if (IS_ERR_OR_NULL(frame_info)) {
		vc_print(layer_index, PRINT_ERROR,
			"%s: frame_info is NULL.\n",
			__func__);
		return -EINVAL;
	}

	dev = video_display_getdev(layer_index);
	if (!dev) {
		vc_print(layer_index, PRINT_ERROR,
			"%s: get dev failed.\n",
			__func__);
		return -EBUSY;
	}
	if (dev->enable_composer != 1)
		video_display_setenable(layer_index, 1);

	is_dec_vf = is_valid_mod_type(frame_info->dmabuf, VF_SRC_DECODER);
	is_v4l_vf = is_valid_mod_type(frame_info->dmabuf, VF_PROCESS_V4LVIDEO);
	get_dma_buf(frame_info->dmabuf);
	dev->received_count++;
	vc_print(layer_index, PRINT_OTHER | PRINT_PATTERN,
		"%s: total receive_count is %lld.\n",
		__func__, dev->received_count);

	if ((dev->last_file == (struct file *)frame_info->dmabuf) &&
	    (is_dec_vf || is_v4l_vf))
		is_repeat_vf = true;

	if (is_repeat_vf) {
		vd_prepare = dev->vd_prepare_last;
	} else {
		vd_prepare = vd_prepare_data_q_get(dev);
		if (!vd_prepare) {
			vc_print(dev->index, PRINT_ERROR,
				 "%s: get prepare_data failed.\n",
				 __func__);
			return -ENOENT;
		}

		if (is_dec_vf || is_v4l_vf) {
			vf = vd_get_vf_from_buf(dev, frame_info->dmabuf);
			if (!vf) {
				vc_print(layer_index, PRINT_ERROR,
					"%s: get vf failed.\n",
					__func__);
				return -ENOENT;
			}
			vd_prepare->src_frame = vf;
			vd_prepare->dst_frame = *vf;
		} else {/*dma buf*/
			vd_prepare->src_frame = &vd_prepare->dst_frame;
		}
		vd_prepare->release_fence = frame_info->release_fence;
		dev->vd_prepare_last = vd_prepare;
	}

	vf = &vd_prepare->dst_frame;

	vf->vc_private = vc_private_q_pop(dev);
	vf->axis[0] = frame_info->dst_x;
	vf->axis[1] = frame_info->dst_y;
	vf->axis[2] = frame_info->dst_w + frame_info->dst_x - 1;
	vf->axis[3] = frame_info->dst_h + frame_info->dst_y - 1;
	vf->crop[0] = frame_info->crop_y;
	vf->crop[1] = frame_info->crop_x;
	vf->crop[2] = frame_info->buffer_h
		- frame_info->crop_h
		- frame_info->crop_y;
	vf->crop[3] = frame_info->buffer_w
		- frame_info->crop_w
		- frame_info->crop_x;
	vf->zorder = frame_info->zorder;
	vf->flag |= VFRAME_FLAG_VIDEO_COMPOSER
		| VFRAME_FLAG_VIDEO_COMPOSER_BYPASS;
	vf->disp_pts = 0;

	if (!(is_dec_vf || is_v4l_vf)) {
		vf->flag |= VFRAME_FLAG_VIDEO_LINEAR;
		vf->canvas0Addr = -1;
		vf->canvas0_config[0].phy_addr =
			get_dma_phy_addr(frame_info->dmabuf, dev);
			vf->canvas0_config[0].width =
					frame_info->buffer_w;
			vf->canvas0_config[0].height =
					frame_info->buffer_h;
			vc_print(dev->index, PRINT_PATTERN,
				 "buffer: w %d, h %d.\n",
				 frame_info->buffer_w,
				 frame_info->buffer_h);
		vf->canvas1Addr = -1;
		vf->canvas0_config[1].phy_addr =
			get_dma_phy_addr(frame_info->dmabuf, dev)
			+ vf->canvas0_config[0].width
			* vf->canvas0_config[0].height;
		vf->canvas0_config[1].width =
			vf->canvas0_config[0].width;
		vf->canvas0_config[1].height =
			vf->canvas0_config[0].height;
		vf->width = frame_info->buffer_w;
		vf->height = frame_info->buffer_h;
		vf->plane_num = 2;
		vf->type = VIDTYPE_PROGRESSIVE
				| VIDTYPE_VIU_FIELD
				| VIDTYPE_VIU_NV21;
		vf->bitdepth =
			BITDEPTH_Y8 | BITDEPTH_U8 | BITDEPTH_V8;
	}

	if (is_repeat_vf) {
		vf->repeat_count[dev->index]++;
		vc_print(layer_index, PRINT_ERROR,
			"%s: repeat frame, repeat_count is %d.\n",
			__func__, vf->repeat_count[dev->index]);
		return 0;
	}
	dev->last_file = (struct file *)frame_info->dmabuf;
	vf->file_vf = (struct file *)(frame_info->dmabuf);
	vf->repeat_count[dev->index] = 0;
	dev->vd_prepare_last = vd_prepare;

	video_dispaly_push_ready(dev, vf);

	if (!kfifo_put(&dev->ready_q, (const struct vframe_s *)vf)) {
		vc_print(layer_index, PRINT_ERROR,
			"%s: ready_q is full.\n",
			__func__);
		return -EAGAIN;
	}
	ready_count = kfifo_len(&dev->ready_q);
	vc_print(layer_index, PRINT_OTHER,
		"%s: ready_q count is %d.\n",
		__func__, ready_count);

	return 0;
}
EXPORT_SYMBOL(video_display_setframe);

int mbd_video_display_setframe(int layer_index,
			struct mbd_video_frame_info_t *frame_info,
			int flags)
{
	struct composer_dev *dev;
	struct vframe_s *vf = NULL;
	int ready_count = 0, i = 0;
	bool is_repeat_vf = false;
	struct vd_prepare_s *vd_prepare = NULL;

	if (IS_ERR_OR_NULL(frame_info)) {
		vc_print(layer_index, PRINT_ERROR,
			"%s: frame_info is NULL.\n",
			__func__);
		return -EINVAL;
	}

	dev = video_display_getdev(layer_index);
	if (!dev) {
		vc_print(layer_index, PRINT_ERROR,
			"%s: get dev failed.\n",
			__func__);
		return -EBUSY;
	}
	if (dev->enable_composer != 1)
		video_display_setenable(layer_index, 1);

	dev->received_count++;
	vc_print(layer_index, PRINT_OTHER | PRINT_PATTERN,
		"%s: total receive_count is %lld.\n",
		__func__, dev->received_count);

	if (dev->last_file == (struct file *)frame_info->buffer_info)
		is_repeat_vf = true;

	if (is_repeat_vf) {
		vd_prepare = dev->vd_prepare_last;
	} else {
		vd_prepare = vd_prepare_data_q_get(dev);
		if (!vd_prepare) {
			vc_print(dev->index, PRINT_ERROR,
				 "%s: get prepare_data failed.\n",
				 __func__);
			return -ENOENT;
		}

		vd_prepare->src_frame = &vd_prepare->dst_frame;
		dev->vd_prepare_last = vd_prepare;
	}

	vf = &vd_prepare->dst_frame;
	vf->vc_private->lock_buffer_cb = frame_info->lock_buffer_cb;
	vf->vc_private->unlock_buffer_cb = frame_info->unlock_buffer_cb;
	vf->vc_private->lock_buffer_cb((void *)frame_info->buffer_info);
	vf->axis[0] = frame_info->dst_x;
	vf->axis[1] = frame_info->dst_y;
	vf->axis[2] = frame_info->dst_w + frame_info->dst_x - 1;
	vf->axis[3] = frame_info->dst_h + frame_info->dst_y - 1;
	vf->crop[0] = frame_info->crop_y;
	vf->crop[1] = frame_info->crop_x;
	vf->crop[2] = frame_info->buffer_h
			- frame_info->crop_h
			- frame_info->crop_y;
	vf->crop[3] = frame_info->buffer_w
			- frame_info->crop_w
			- frame_info->crop_x;
	vf->zorder = frame_info->zorder;
	vf->disp_pts = 0;
	vf->flag |= VFRAME_FLAG_VIDEO_COMPOSER | VFRAME_FLAG_VIDEO_COMPOSER_DMA
		| VFRAME_FLAG_VIDEO_COMPOSER_BYPASS;
	vf->flag |= VFRAME_FLAG_VIDEO_LINEAR;
	vf->canvas0Addr = -1;
	vf->canvas1Addr = -1;
	for (i = 0; i < 3; i++) {
		vf->canvas0_config[i].phy_addr =
			frame_info->buffer_info->phys_addr[i];
		vf->canvas0_config[i].width = frame_info->stride[i];
		vf->canvas0_config[i].height = frame_info->buffer_h;
	}

	vf->plane_num = i;
	vf->width = frame_info->buffer_w;
	vf->height = frame_info->buffer_h;
	vf->type = frame_info->type;
	vf->bitdepth = BITDEPTH_Y8 | BITDEPTH_U8 | BITDEPTH_V8;

	if (is_repeat_vf) {
		vf->repeat_count[dev->index]++;
		vc_print(layer_index, PRINT_ERROR,
			"%s: repeat frame, repeat_count is %d.\n",
			__func__, vf->repeat_count[dev->index]);
		return 0;
	}
	dev->last_file = (struct file *)frame_info->buffer_info;
	vf->file_vf = (struct file *)(frame_info->buffer_info);
	vf->repeat_count[dev->index] = 0;
	dev->vd_prepare_last = vd_prepare;
	if (!kfifo_put(&dev->ready_q, (const struct vframe_s *)vf)) {
		vc_print(layer_index, PRINT_ERROR,
			"%s: ready_q is full.\n",
			__func__);
		return -EAGAIN;
	}
	ready_count = kfifo_len(&dev->ready_q);
	vc_print(layer_index, PRINT_OTHER,
		"%s: ready_q count is %d.\n",
		__func__, ready_count);

	return 0;
}
EXPORT_SYMBOL(mbd_video_display_setframe);
