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

#define TEE_MEM_TYPE_GPU                                   0x6
#define TEE_MEM_TYPE_VDIN                                  0x7
#define TEE_MEM_TYPE_ENCODER                               0x8
#define TEE_MEM_TYPE_GE2D                                  0x9
#define TEE_MEM_TYPE_DEMUX                                 0xa

extern bool tee_enabled(void);
extern int is_secload_get(void);
extern int tee_load_video_fw(uint32_t index, uint32_t vdec);
extern int tee_load_video_fw_swap(uint32_t index, uint32_t vdec, bool is_swap);
extern uint32_t tee_protect_tvp_mem(uint32_t start, uint32_t size,
			uint32_t *handle);
extern void tee_unprotect_tvp_mem(uint32_t handle);
extern uint32_t tee_protect_mem_by_type(uint32_t type,
		uint32_t start, uint32_t size,
		uint32_t *handle);
#endif /* __TEE_H__ */

