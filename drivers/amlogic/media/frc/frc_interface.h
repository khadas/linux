/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __FRC_INTERFACE_H__
#define __FRC_INTERFACE_H__
//==== amlogic include =============
#include <linux/amlogic/media/vpu/vpu.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vout/vinfo.h>
#include <linux/amlogic/media/video_sink/vpp.h>
#include <linux/amlogic/media/frc/frc_common.h>
#include <uapi/linux/amlogic/frc.h>

enum frc_state_e {
	FRC_STATE_DISABLE = 0,
	FRC_STATE_ENABLE,
	FRC_STATE_BYPASS,
	FRC_STATE_NULL,
};

int frc_input_handle(struct vframe_s *vf, struct vpp_frame_par_s *cur_video_sts);
int frc_set_mode(enum frc_state_e state);
int frc_get_video_latency(void);
int frc_is_on(void);
int frc_is_supported(void);
int frc_memc_set_level(u8 level);
int frc_fpp_memc_set_level(u8 level, u8 num);
int frc_set_seg_display(u8 enable, u8 seg1, u8 seg2, u8 seg3);
int frc_lge_memc_set_level(struct v4l2_ext_memc_motion_comp_info comp_info);
void frc_lge_memc_get_level(struct v4l2_ext_memc_motion_comp_info *comp_info);

#endif
