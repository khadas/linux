/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * drivers/amlogic/media/enhancement/amvecm/pattern_detection.h
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

#ifndef __PATTERN_DETECTION__
#define __PATTERN_DETECTION__
#include <linux/amlogic/media/amvecm/cm.h>

#define PATTERN_PARAM_COUNT  64
#define HIST_BIN_COUNT       32
#define PATTERN_START        0
#define PATTERN_MAX          8
#define PATTERN_MASK(i) (1 << (i))

#define MULTICAST_REG_SIZE 6
#define MULTICAST_REG_LEV 2

struct di_reg_s {
	unsigned int val[MULTICAST_REG_SIZE];
};

#define setting_reg_count 32
struct setting_regs_s {
	unsigned int    length; /* Length of total am_reg */
	struct am_reg_s am_reg[setting_reg_count];
};

enum epattern {
	PATTERN_UNKNOWN = -1,
	PATTERN_75COLORBAR = PATTERN_START,
	PATTERN_SKIN_TONE_FACE,
	PATTERN_GREEN_CORN,
	PATTERN_MULTICAST,
	PATTERN4,
	PATTERN5,
	PATTERN6,
	PATTERN7,
	PATTERNMAX = PATTERN_MAX,
};

struct pattern {
	int pattern_id;
	unsigned int *pattern_param;
	struct setting_regs_s *cvd2_settings;
	struct setting_regs_s *vpp_settings;
	int (*checker)(struct vframe_s *vf);
	int (*default_loader)(struct vframe_s *vf);
	int (*handler)(struct vframe_s *vf, int flag);
};

extern int enable_pattern_detect;
extern int pattern_detect_debug;
extern int detected_pattern;
extern int last_detected_pattern;
extern uint pattern_mask;
extern uint mltcast_ratio1;
extern uint mltcast_ratio2;
extern int mltcast_skip_en;

int pattern_detect_add_checker(int id,
			       int (*checker)(struct vframe_s *vf));
int pattern_detect_add_handler(int id,
			       int (*handler)(struct vframe_s *vf, int flag));
int pattern_detect_add_defaultloader(int id,
				     int (*loader)(struct vframe_s *vf));
int pattern_detect_add_cvd2_setting_table(int id,
					  struct setting_regs_s *new_settings);
int pattern_detect_add_vpp_setting_table(int id,
					 struct setting_regs_s *new_settings);
int init_pattern_detect(void);
int pattern_detect(struct vframe_s *vf);

#endif

