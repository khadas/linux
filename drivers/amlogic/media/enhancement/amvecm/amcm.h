/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * drivers/amlogic/media/enhancement/amvecm/amcm.h
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

#ifndef __AM_CM_H
#define __AM_CM_H

#include <linux/amlogic/media/vfm/vframe.h>
#include "linux/amlogic/media/amvecm/cm.h"

struct sr1_regs_s {
	unsigned int addr;
	unsigned int mask;
	unsigned int  val;
};

extern unsigned int vecm_latch_flag;
extern unsigned int cm_size;
extern unsigned int cm2_patch_flag;
extern int cm_en; /* 0:disabel;1:enable */
extern int dnlp_en;/*0:disabel;1:enable */

extern unsigned int sr1_reg_val[101];

/* *********************************************************************** */
/* *** IOCTL-oriented functions ****************************************** */
/* *********************************************************************** */
void am_set_regmap(struct am_regs_s *p);
void amcm_disable(void);
void amcm_enable(void);
void amcm_level_sel(unsigned int cm_level);
void cm2_frame_size_patch(unsigned int width, unsigned int height);
void cm2_frame_switch_patch(void);
void cm_latch_process(void);
int cm_load_reg(struct am_regs_s *arg);
void pd_combing_fix_patch(enum pd_comb_fix_lvl_e level);

#endif

