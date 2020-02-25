/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _DI_PULLDOWN_H
#define _DI_PULLDOWN_H
#include "../deinterlace/film_mode_fmw/film_vof_soft.h"
#include "../deinterlace/deinterlace_mtn.h"

#define MAX_VOF_WIN_NUM	4

enum pulldown_mode_e {
	PULL_DOWN_BLEND_0 = 0,/* buf1=dup[0] */
	PULL_DOWN_BLEND_2 = 1,/* buf1=dup[2] */
	PULL_DOWN_MTN	  = 2,/* mtn only */
	PULL_DOWN_BUF1	  = 3,/* do wave with dup[0] */
	PULL_DOWN_EI	  = 4,/* ei only */
	PULL_DOWN_NORMAL  = 5,/* normal di */
	PULL_DOWN_NORMAL_2  = 6,/* di */
};

struct pulldown_vof_win_s {
	unsigned short win_vs;
	unsigned short win_ve;
	enum pulldown_mode_e blend_mode;
};

struct pulldown_detected_s {
	enum pulldown_mode_e global_mode;
	struct pulldown_vof_win_s regs[4];
};

unsigned char pulldown_init(unsigned short width, unsigned short height);

unsigned int pulldown_detection(struct pulldown_detected_s *res,
	struct combing_status_s *cmb_sts, bool reverse, struct vframe_s *vf);

void pulldown_vof_win_vshift(struct pulldown_detected_s *wins,
	unsigned short v_offset);

void pd_device_files_add(struct device *dev);

void pd_device_files_del(struct device *dev);
#endif
