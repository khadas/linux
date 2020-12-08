/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * include/linux/amlogic/media/vout/vdac_dev.h
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

#ifndef _INC_VDAC_DEV_H_
#define _INC_VDAC_DEV_H_

#define VDAC_MODULE_MASK      (0x1f)
#define VDAC_MODULE_AVOUT_ATV (0x1)
#define VDAC_MODULE_DTV_DEMOD (0x2)
#define VDAC_MODULE_AVOUT_AV  (0x4)
#define VDAC_MODULE_CVBS_OUT  (0x8)
#define VDAC_MODULE_AUDIO_OUT (0x10) /*0x10*/

void vdac_enable(bool on, unsigned int module_sel);
int vdac_enable_check_dtv(void);
int vdac_enable_check_cvbs(void);
int vdac_vref_adj(unsigned int value);
unsigned int vdac_get_reg_addr(unsigned int index);

#endif
