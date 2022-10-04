/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef VIDEO_LOW_LATENCTY_HH
#define VIDEO_LOW_LATENCTY_HH
#if defined(CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_VECM)
#include "vpp_pq.h"
#endif
#include <linux/amlogic/media/video_sink/video_signal_notify.h>
#include <linux/amlogic/media/video_sink/vpp.h>
#include "video_reg.h"

extern u32 lowlatency_overrun_recovery_cnt;
extern u32 blackout;
extern u32 blackout_pip;
extern u32 blackout_pip2;
extern char old_vmode[32];
extern char new_vmode[32];
extern struct vframe_s *cur_dispbuf2;
extern int vframe_walk_delay;
extern struct vpp_frame_par_s *cur_frame_par;
extern u32 new_frame_count;
extern u32 vsync_proc_drop;
extern ulong lowlatency_vsync_count;
extern bool overrun_flag;
extern u32 vsync_pts_inc_scale;
extern u32 vsync_pts_inc_scale_base;
extern atomic_t axis_changed;
extern const char *src_fmt_str[10];
extern int ai_pq_disable;
extern int ai_pq_value;
extern int ai_pq_debug;
extern int ai_pq_policy;
extern struct ai_scenes_pq vpp_scenes[AI_SCENES_MAX];
extern struct nn_value_t nn_scenes_value[AI_PQ_TOP];
extern atomic_t video_prop_change;
extern wait_queue_head_t amvideo_prop_change_wait;
extern u32 vpp_crc_en;
extern int vpp_crc_result;
extern atomic_t video_proc_lock;
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
extern bool dvel_status;
#endif

ssize_t lowlatency_states_store(struct class *cla,
	struct class_attribute *attr,
	const char *buf, size_t count);
ssize_t lowlatency_states_show(struct class *cla,
	struct class_attribute *attr,
	char *buf);
void hdmi_in_delay_maxmin_new(struct vframe_s *vf);
bool tvin_vf_is_keeped(struct vframe_s *vf);
void _set_video_mirror(struct disp_info_s *layer, int mirror);
void primary_swap_frame(struct video_layer_s *layer, struct vframe_s *vf1, int line);
int dvel_swap_frame(struct vframe_s *vf);
s32 primary_render_frame(struct video_layer_s *layer);
void vf_pq_process(struct vframe_s *vf,
		   struct ai_scenes_pq *vpp_scenes,
		   int *pq_debug);
#endif
/*VIDEO_LOW_LATENCTY_HH*/
