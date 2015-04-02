/*
amlogic canvas.h
*/

#ifndef CANVAS_H
#define CANVAS_H

#include <linux/types.h>
#include <linux/kobject.h>

struct canvas_s {
	struct kobject kobj;
	ulong addr;
	u32 width;
	u32 height;
	u32 wrap;
	u32 blkmode;
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
#define PPMGR_DOUBLE_CANVAS_INDEX 0x74  /*for double canvas use*/
#define PPMGR_DEINTERLACE_BUF_CANVAS 0x77   /*for progressive mjpeg use*/
/*for progressive mjpeg (nv21 output)use*/
#define PPMGR_DEINTERLACE_BUF_NV21_CANVAS 0x7a

extern void canvas_config(u32 index, ulong addr, u32 width, u32 height,
			  u32 wrap, u32 blkmode);

extern void canvas_read(u32 index, struct canvas_s *p);

extern void canvas_copy(unsigned src, unsigned dst);

extern void canvas_update_addr(u32 index, u32 addr);

extern unsigned int canvas_get_addr(u32 index);

#endif/* CANVAS_H */
