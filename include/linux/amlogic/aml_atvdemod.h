/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __AML_ATVDEMOD_H__
#define __AML_ATVDEMOD_H__

#include <linux/i2c.h>
#include <uapi/linux/videodev2.h>

#include <linux/amlogic/aml_demod_common.h>

/*COLOR MODULATION TYPE*/
#define V4L2_COLOR_STD_PAL    ((v4l2_std_id) 0x04000000)
#define V4L2_COLOR_STD_NTSC   ((v4l2_std_id) 0x08000000)
#define V4L2_COLOR_STD_SECAM  ((v4l2_std_id) 0x10000000)

/* For convert v4l2 std define to string */
static inline const char *v4l2_std_to_str(v4l2_std_id std)
{
	switch (std) {
	case V4L2_STD_PAL_B:
		return "V4L2_STD_PAL_B";
	case V4L2_STD_PAL_B1:
		return "V4L2_STD_PAL_B1";
	case V4L2_STD_PAL_G:
		return "V4L2_STD_PAL_G";
	case V4L2_STD_PAL_H:
		return "V4L2_STD_PAL_H";
	case V4L2_STD_PAL_I:
		return "V4L2_STD_PAL_I";
	case V4L2_STD_PAL_D:
		return "V4L2_STD_PAL_D";
	case V4L2_STD_PAL_D1:
		return "V4L2_STD_PAL_D1";
	case V4L2_STD_PAL_K:
		return "V4L2_STD_PAL_K";
	case V4L2_STD_PAL_M:
		return "V4L2_STD_PAL_M";
	case V4L2_STD_PAL_N:
		return "V4L2_STD_PAL_N";
	case V4L2_STD_PAL_Nc:
		return "V4L2_STD_PAL_Nc";
	case V4L2_STD_PAL_60:
		return "V4L2_STD_PAL_60";
	case V4L2_STD_NTSC_M:
		return "V4L2_STD_NTSC_M";
	case V4L2_STD_NTSC_M_JP:
		return "V4L2_STD_NTSC_M_JP";
	case V4L2_STD_NTSC_443:
		return "V4L2_STD_NTSC_443";
	case V4L2_STD_NTSC_M_KR:
		return "V4L2_STD_NTSC_M_KR";
	case V4L2_STD_SECAM_B:
		return "V4L2_STD_SECAM_B";
	case V4L2_STD_SECAM_D:
		return "V4L2_STD_SECAM_D";
	case V4L2_STD_SECAM_G:
		return "V4L2_STD_SECAM_G";
	case V4L2_STD_SECAM_H:
		return "V4L2_STD_SECAM_H";
	case V4L2_STD_SECAM_K:
		return "V4L2_STD_SECAM_K";
	case V4L2_STD_SECAM_K1:
		return "V4L2_STD_SECAM_K1";
	case V4L2_STD_SECAM_L:
		return "V4L2_STD_SECAM_L";
	case V4L2_STD_SECAM_LC:
		return "V4L2_STD_SECAM_LC";
	case V4L2_STD_ATSC_8_VSB:
		return "V4L2_STD_ATSC_8_VSB";
	case V4L2_STD_ATSC_16_VSB:
		return "V4L2_STD_ATSC_16_VSB";
	case V4L2_COLOR_STD_PAL:
		return "V4L2_COLOR_STD_PAL";
	case V4L2_COLOR_STD_NTSC:
		return "V4L2_COLOR_STD_NTSC";
	case V4L2_COLOR_STD_SECAM:
		return "V4L2_COLOR_STD_SECAM";
	case V4L2_STD_MN:
		return "V4L2_STD_MN";
	case V4L2_STD_B:
		return "V4L2_STD_B";
	case V4L2_STD_GH:
		return "V4L2_STD_GH";
	case V4L2_STD_DK:
		return "V4L2_STD_DK";
	case V4L2_STD_PAL_BG:
		return "V4L2_STD_PAL_BG";
	case V4L2_STD_PAL_DK:
		return "V4L2_STD_PAL_DK";
	case V4L2_STD_PAL:
		return "V4L2_STD_PAL";
	case V4L2_STD_NTSC:
		return "V4L2_STD_NTSC";
	case V4L2_STD_SECAM_DK:
		return "V4L2_STD_SECAM_DK";
	case (V4L2_STD_SECAM_B | V4L2_STD_SECAM_G):
		return "V4L2_STD_SECAM_BG";
	case V4L2_STD_SECAM:
		return "V4L2_STD_SECAM";
	case V4L2_STD_525_60:
		return "V4L2_STD_525_60";
	case V4L2_STD_625_50:
		return "V4L2_STD_625_50";
	case V4L2_STD_ATSC:
		return "V4L2_STD_ATSC";
	case V4L2_STD_ALL:
		return "V4L2_STD_ALL";
	default:
		return "V4L2_STD_UNKNOWN";
	}
}

/* For audio driver get atv audio state */
void aml_fe_get_atvaudio_state(int *state);

#endif /* __AML_ATVDEMOD_H__ */
