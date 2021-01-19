/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _OSD_IO_H_
#define _OSD_IO_H_

int osd_io_remap(int iomap);
u32 osd_cbus_read(u32 reg);
void osd_cbus_write(u32 reg, const u32 val);
u32 osd_reg_read(u32 reg);
void osd_reg_write(u32 reg, const u32 val);
void osd_reg_set_mask(u32 reg, const u32 mask);
void osd_reg_clr_mask(u32 reg, const u32 mask);
void osd_reg_set_bits(u32 reg,
		      const u32 value,
		      const u32 start,
		      const u32 len);

u32 VSYNCOSD_RD_MPEG_REG(u32 reg);
int VSYNCOSD_WR_MPEG_REG(u32 reg, u32 val);
int VSYNCOSD_WR_MPEG_REG_BITS(u32 reg, u32 val, u32 start, u32 len);
int VSYNCOSD_SET_MPEG_REG_MASK(u32 reg, u32 mask);
int VSYNCOSD_CLR_MPEG_REG_MASK(u32 reg, u32 mask);
int VSYNCOSD_IRQ_WR_MPEG_REG(u32 reg, u32 val);

u32 VSYNCOSD_RD_MPEG_REG_VPP1(u32 reg);
int VSYNCOSD_WR_MPEG_REG_VPP1(u32 reg, u32 val);
int VSYNCOSD_WR_MPEG_REG_BITS_VPP1(u32 reg, u32 val, u32 start, u32 len);
int VSYNCOSD_SET_MPEG_REG_MASK_VPP1(u32 reg, u32 mask);
int VSYNCOSD_CLR_MPEG_REG_MASK_VPP1(u32 reg, u32 mask);
int VSYNCOSD_IRQ_WR_MPEG_REG_VPP1(u32 reg, u32 val);

u32 VSYNCOSD_RD_MPEG_REG_VPP2(u32 reg);
int VSYNCOSD_WR_MPEG_REG_VPP2(u32 reg, u32 val);
int VSYNCOSD_WR_MPEG_REG_BITS_VPP2(u32 reg, u32 val, u32 start, u32 len);
int VSYNCOSD_SET_MPEG_REG_MASK_VPP2(u32 reg, u32 mask);
int VSYNCOSD_CLR_MPEG_REG_MASK_VPP2(u32 reg, u32 mask);
int VSYNCOSD_IRQ_WR_MPEG_REG_VPP2(u32 reg, u32 val);

#endif
