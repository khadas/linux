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

/* memory align used by tee_protect_mem_by_type() */
#define TEE_MEM_ALIGN_SIZE                                 0x10000

/* memory type used by tee_protect_mem_by_type() */
#define TEE_MEM_TYPE_STREAM_OUTPUT                         0x5
#define TEE_MEM_TYPE_GPU                                   0x6
#define TEE_MEM_TYPE_VDIN                                  0x7
#define TEE_MEM_TYPE_HCODEC                                0x8
#define TEE_MEM_TYPE_GE2D                                  0x9
#define TEE_MEM_TYPE_DEMUX                                 0xa
#define TEE_MEM_TYPE_TCON                                  0xb
#define TEE_MEM_TYPE_PCIE                                  0xc
#define TEE_MEM_TYPE_INVALID                               0xff

/* device ID used by tee_config_device_state() */
#define DMC_DEV_ID_GPU                                     1
#define DMC_DEV_ID_HEVC                                    4
#define DMC_DEV_ID_PARSER                                  7
#define DMC_DEV_ID_VPU                                     8
#define DMC_DEV_ID_VDIN                                    9
#define DMC_DEV_ID_VDEC                                    13
#define DMC_DEV_ID_HCODEC                                  14
#define DMC_DEV_ID_GE2D                                    15

/*
 * Desc: tee enabled state
 *
 * Return: 1 if tee enabled, 0 if tee disabled
 */
extern bool tee_enabled(void);

/*
 * Desc: not used
 */
extern int is_secload_get(void);

/*
 * Desc: load video firmware to hardware
 *
 * Input:
 * index: video firmware index
 * vdec:  vdec type
 *
 * Return: 0 if suceess
 */
extern int tee_load_video_fw(uint32_t index, uint32_t vdec);

/*
 * Desc: load video fw to hardware
 *
 * Input:
 * index: video firmware index
 * vdec:  vdec type
 * is_swap: 1 if swapped, 0 if not swpped
 *
 * Return: 0 if suceess
 */
extern int tee_load_video_fw_swap(uint32_t index, uint32_t vdec, bool is_swap);

/*
 * Desc: protect tvp memory, same as
 * tee_protect_mem_by_type(TEE_MEM_TYPE_STREAM_OUTPUT, pa, size, handle)
 *
 * Input:
 * start: the start of physical memory, must aligned with TEE_MEM_ALIGN_SIZE
 * size: the size of the physical memory, must aligned with TEE_MEM_ALIGN_SIZE
 *
 * Output:
 * handle: the handle
 *
 * Return: 0 if suceess
 */
extern uint32_t tee_protect_tvp_mem(uint32_t start, uint32_t size,
			uint32_t *handle);

/*
 * Desc: Unprotect tvp memory, same as tee_unprotect_mem
 *
 * Input:
 * handle: the handle from tee_protect_tvp_mem
 *
 * Return: 0 if suceess
 */

extern void tee_unprotect_tvp_mem(uint32_t handle);

/*
 * Desc: protect memory by type
 *
 * Input:
 * type: memory type
 * start: the start of physical memory, must aligned with TEE_MEM_ALIGN_SIZE
 * size: the size of the physical memory, must aligned with TEE_MEM_ALIGN_SIZE
 *
 * Output:
 * handle: the handle
 *
 * Return: 0 if suceess
 */
extern uint32_t tee_protect_mem_by_type(uint32_t type,
		uint32_t start, uint32_t size,
		uint32_t *handle);

/*
 * Desc: Unprotect memory
 *
 * Input:
 * handle: the handle from tee_protect_mem_by_type
 *
 * Return: 0 if suceess
 */
extern void tee_unprotect_mem(uint32_t handle);

/*
 * Desc: Switch device state
 *
 * Input:
 * dev_id: device type id
 * secure: secure state 0/1
 *
 * Return: 0 if suceess
 */
extern int tee_config_device_state(int dev_id, int secure);

#endif /* __TEE_H__ */

