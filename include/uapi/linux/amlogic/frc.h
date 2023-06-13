/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __FRC_H__
#define __FRC_H__

enum frc_fpp_state_e {
	FPP_MEMC_OFF = 0,    // MEMC OFF
	FPP_MEMC_LOW,        // MEMC LOW, default level9
	FPP_MEMC_MID,        // MEMC MID, default level10
	FPP_MEMC_HIGH,       // MEMC HIGH, default level10 and fullback
	FPP_MEMC_CUSTOME,    // Retain customization
	FPP_MEMC_24PFILM,    // 24P Film mode, 32 Pulldown out 48fps
	FPP_MEMC_MAX,
};

// MEMC Type Definition
enum v4l2_ext_memc_type_old {
	V4L2_EXT_MEMC_TYPE_OFF = 0,
	V4L2_EXT_MEMC_TYPE_LOW,
	V4L2_EXT_MEMC_TYPE_HIGH,
	V4L2_EXT_MEMC_TYPE_USER,
	V4L2_EXT_MEMC_TYPE_55_PULLDOWN,
	V4L2_EXT_MEMC_TYPE_MEDIUM
};

enum v4l2_ext_memc_type {
	V4L2_EXT_MEMC_OFF          = V4L2_EXT_MEMC_TYPE_OFF,
	V4L2_EXT_MEMC_CINEMA_CLEAR = V4L2_EXT_MEMC_TYPE_MEDIUM,
	V4L2_EXT_MEMC_NATURAL      = V4L2_EXT_MEMC_TYPE_LOW,
	V4L2_EXT_MEMC_SMOOTH       = V4L2_EXT_MEMC_TYPE_HIGH, //
	V4L2_EXT_MEMC_USER         = V4L2_EXT_MEMC_TYPE_USER,
	V4L2_EXT_MEMC_PULLDOWN_55  = V4L2_EXT_MEMC_TYPE_55_PULLDOWN
};

struct v4l2_ext_memc_motion_comp_info {
	unsigned char blur_level;
	unsigned char judder_level;
	enum v4l2_ext_memc_type memc_type;
};

//==== ioctl define =============
#define FRC_IOC_MAGIC                   'F'
#define FRC_IOC_GET_FRC_EN      _IOR(FRC_IOC_MAGIC, 0x00, unsigned int)
#define FRC_IOC_GET_FRC_STS     _IOR(FRC_IOC_MAGIC, 0x01, unsigned int)
#define FRC_IOC_SET_FRC_CADENCE    _IOW(FRC_IOC_MAGIC, 0x02, unsigned int)
#define FRC_IOC_GET_VIDEO_LATENCY   _IOR(FRC_IOC_MAGIC, 0x03, unsigned int)
#define FRC_IOC_GET_IS_ON       _IOR(FRC_IOC_MAGIC, 0x04, unsigned int)
#define FRC_IOC_SET_INPUT_VS_RATE   _IOW(FRC_IOC_MAGIC, 0x05, unsigned int)
#define FRC_IOC_SET_MEMC_ON_OFF     _IOW(FRC_IOC_MAGIC, 0x06, unsigned int)
#define FRC_IOC_SET_MEMC_LEVEL      _IOW(FRC_IOC_MAGIC, 0x07, unsigned int)
#define FRC_IOC_SET_MEMC_DMEO_MODE  _IOW(FRC_IOC_MAGIC, 0x08, unsigned int)
#define FRC_IOC_SET_FPP_MEMC_LEVEL  _IOW(FRC_IOC_MAGIC, 0x09, enum frc_fpp_state_e)
#define FRC_IOC_SET_MEMC_VENDOR         _IOW(FRC_IOC_MAGIC, 0x0A, unsigned int)
#define FRC_IOC_SET_MEMC_FB     _IOW(FRC_IOC_MAGIC, 0x0B, unsigned int)
#define FRC_IOC_SET_MEMC_FILM       _IOW(FRC_IOC_MAGIC, 0x0C, unsigned int)
#define PQ_MEMC_IOC_SET_LGE_MEMC_LEVEL	_IOW(FRC_IOC_MAGIC, 0x0D, \
	struct v4l2_ext_memc_motion_comp_info)
#define PQ_MEMC_IOC_GET_LGE_MEMC_LEVEL	_IOR(FRC_IOC_MAGIC, 0x0E, \
	struct v4l2_ext_memc_motion_comp_info)
#define FRC_IOC_GET_MEMC_VERSION    _IOR(FRC_IOC_MAGIC, 0x0F, unsigned char[32])
#define PQ_MEMC_IOC_LGE_SET_MEMC_INIT	_IOW(FRC_IOC_MAGIC, 0x10, unsigned int)
#define FRC_IOC_SET_MEMC_N2M        _IOW(FRC_IOC_MAGIC, 0x11, unsigned int)

#endif
