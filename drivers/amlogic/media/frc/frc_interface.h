/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __FRC_INTERFACE_H__
#define __FRC_INTERFACE_H__
//==== amlogci inclue =============
#include <linux/amlogic/media/vpu/vpu.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vout/vinfo.h>
#include <linux/amlogic/media/video_sink/vpp.h>
#include "frc_common.h"

//==== ioctrol define =============
#define FRC_IOC_MAGIC                   'F'
#define FRC_IOC_GET_FRC_EN		_IOR(FRC_IOC_MAGIC, 0x00, unsigned int)
#define FRC_IOC_GET_FRC_STS		_IOR(FRC_IOC_MAGIC, 0x01, unsigned int)
#define FRC_IOC_SET_FRC_CANDENCE	_IOW(FRC_IOC_MAGIC, 0x02, unsigned int)
#define FRC_IOC_GET_VIDEO_LATENCY	_IOR(FRC_IOC_MAGIC, 0x03, unsigned int)
#define FRC_IOC_GET_IS_ON		_IOR(FRC_IOC_MAGIC, 0x04, unsigned int)
#define FRC_IOC_SET_INPUT_VS_RATE	_IOW(FRC_IOC_MAGIC, 0x05, unsigned int)
#define FRC_IOC_SET_MEMC_ON_OFF		_IOW(FRC_IOC_MAGIC, 0x06, unsigned int)
#define FRC_IOC_SET_MEMC_LEVEL		_IOW(FRC_IOC_MAGIC, 0x07, unsigned int)

int frc_input_handle(struct vframe_s *vf, struct vpp_frame_par_s *cur_video_sts);
int frc_set_mode(enum frc_state_e state);
int frc_get_video_latency(void);
int frc_is_on(void);

#endif
