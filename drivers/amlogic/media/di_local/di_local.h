/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * drivers/amlogic/media/di_local/di_local.h
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

#ifndef __DI_LOCAL_H__
#define __DI_LOCAL_H__

int get_current_vscale_skip_count(struct vframe_s *vf);

void dil_set_cpuver_flag(unsigned int para);

unsigned int dil_get_cpuver_flag(void);

struct di_ext_ops {
	unsigned int (*di_post_reg_rd)(unsigned int addr);
	int (*di_post_wr_reg_bits)(u32 adr, u32 val, u32 start, u32 len);
	void (*post_update_mc)(void);
	void (*post_keep_cmd_release2)(struct vframe_s *vframe);
	void (*polic_cfg)(unsigned int cmd, bool on);
};

#endif	/*__DI_LOCAL_H__*/
