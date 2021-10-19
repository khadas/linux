/*
 * drivers/amlogic/media/enhancement/amvecm/dolby_vision/amdolby_vision.h
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
#ifndef _AMDV_H_
#define _AMDV_H_

#include <linux/types.h>

enum signal_format_e {
	FORMAT_INVALID = -1,
	FORMAT_DOVI = 0,
	FORMAT_HDR10 = 1,
	FORMAT_SDR = 2,
	FORMAT_DOVI_LL = 3,
	FORMAT_HLG = 4,
	FORMAT_HDR10PLUS = 5,
	FORMAT_SDR_2020 = 6,
	FORMAT_MVC = 7
};

enum cpuID_e {
	_CPU_MAJOR_ID_GXM,
	_CPU_MAJOR_ID_TXLX,
	_CPU_MAJOR_ID_G12,
	_CPU_MAJOR_ID_TM2,
	_CPU_MAJOR_ID_TM2_REVB,
	_CPU_MAJOR_ID_UNKNOWN,
};

struct dv_device_data_s {
	enum cpuID_e cpu_id;
};

struct amdolby_vision_port_t {
	const char *name;
	struct device *dev;
	const struct file_operations *fops;
	void *runtime;
};

#ifndef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
#define VSYNC_WR_MPEG_REG(adr, val) WRITE_VPP_REG(adr, val)
#define VSYNC_RD_MPEG_REG(adr) READ_VPP_REG(adr)
#define VSYNC_WR_MPEG_REG_BITS(adr, val, start, len) \
	WRITE_VPP_REG_BITS(adr, val, start, len)
#else
extern int VSYNC_WR_MPEG_REG_BITS(u32 adr, u32 val, u32 start, u32 len);
extern u32 VSYNC_RD_MPEG_REG(u32 adr);
extern int VSYNC_WR_MPEG_REG(u32 adr, u32 val);
#endif

void dv_mem_power_on(enum vpu_mod_e mode);
void dv_mem_power_off(enum vpu_mod_e mode);
int get_dv_mem_power_flag(enum vpu_mod_e mode);
#endif
