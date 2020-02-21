/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * include/linux/amlogic/tee.h
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

#ifndef __TEE_H__
#define __TEE_H__

bool tee_enabled(void);
int is_secload_get(void);
int tee_load_video_fw(u32 index, u32 vdec);
int tee_load_video_fw_swap(u32 index, u32 vdec, bool is_swap);
u32 tee_protect_tvp_mem(u32 start, u32 size,
			u32 *handle);
void tee_unprotect_tvp_mem(u32 handle);
#endif /* __TEE_H__ */

