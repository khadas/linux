/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef CANVAS_H
#define CANVAS_H

#include <linux/types.h>
#include <linux/kobject.h>

struct canvas_s {
	struct kobject kobj;
	u32 index;
	ulong addr;
	u32 width;
	u32 height;
	u32 wrap;
	u32 blkmode;
	u32 endian;
	u32 dataL;
	u32 dataH;
};

struct canvas_config_s {
	ulong phy_addr;
	u32 width;
	u32 height;
	u32 block_mode;
	u32 endian;
};

#define CANVAS_ADDR_NOWRAP      0x00
#define CANVAS_ADDR_WRAPX       0x01
#define CANVAS_ADDR_WRAPY       0x02
#define CANVAS_BLKMODE_MASK     3
#define CANVAS_BLKMODE_BIT      24
#define CANVAS_BLKMODE_LINEAR   0x00
#define CANVAS_BLKMODE_32X32    0x01
#define CANVAS_BLKMODE_64X32    0x02

#define PPMGR_CANVAS_INDEX 0x70
#define PPMGR_DOUBLE_CANVAS_INDEX 0x74	/*for double canvas use */
#define PPMGR_DEINTERLACE_BUF_CANVAS 0x7a	/*for progressive mjpeg use */

/*for progressive mjpeg (nv21 output)use*/
#define PPMGR_DEINTERLACE_BUF_NV21_CANVAS 0x7e

#define PPMGR2_MAX_CANVAS 16
#define PPMGR2_CANVAS_INDEX 0x70    /* 0x70-0x7f for PPMGR2 (IONVIDEO)/ */

/*
 *the following reserved canvas index value
 * should match the configurations defined
 * in canvas_mgr.c canvas_pool_config().
 */
#define AMVDEC_CANVAS_MAX1        0xbf
#define AMVDEC_CANVAS_MAX2        0x1a
#define AMVDEC_CANVAS_START_INDEX 0x78

void canvas_config_config(u32 index, struct canvas_config_s *cfg);
void canvas_config(u32 index, ulong addr, u32 width, u32 height,
		   u32 wrap, u32 blkmode);
void canvas_config_ex(u32 index, ulong addr, u32 width,
		      u32 height, u32 wrap, u32 blkmode, u32 endian);
void canvas_read(u32 index, struct canvas_s *p);
void canvas_copy(unsigned int src, unsigned int dst);
void canvas_update_addr(u32 index, ulong addr);
ulong canvas_get_addr(u32 index);

unsigned int canvas_get_width(u32 index);

unsigned int canvas_get_height(u32 index);
#endif /* CANVAS_H */
