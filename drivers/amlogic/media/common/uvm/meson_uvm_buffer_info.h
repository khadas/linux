/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __MESON_UVM_BUFFER_INFO_H_
#define __MESON_UVM_BUFFER_INFO_H_

enum uvm_video_type {
	AM_VIDEO_NORMAL      = 0x0,
	AM_VIDEO_DV          = 0x1,
	AM_VIDEO_HDR         = 0x2,
	AM_VIDEO_HDR10_PLUS  = 0x4,
	AM_VIDEO_HLG         = 0x8,
	AM_VIDEO_SECURE      = 0x10,
	AM_VIDEO_AFBC        = 0x20,
	AM_VIDEO_DI_POST     = 0x40,
	AM_VIDEO_4K          = 0x80,
	AM_VIDEO_8K          = 0x100,
};

int get_uvm_video_type(int fd);

#endif
