/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __VPP_REG_IO_H__
#define __VPP_REG_IO_H__

static inline void WRITE_VPP_REG(u32 reg,
				 const u32 value)
{
	aml_write_vcbus_s(reg, value);
}

static inline u32 READ_VPP_REG(u32 reg)
{
	return aml_read_vcbus_s(reg);
}

static inline void WRITE_VPP_REG_BITS(u32 reg,
				      const u32 value,
		const u32 start,
		const u32 len)
{
	aml_vcbus_update_bits_s(reg, value, start, len);
}

static inline u32 READ_VPP_REG_BITS(u32 reg,
				    const u32 start,
				    const u32 len)
{
	u32 val;

	val = ((aml_read_vcbus_s(reg) >> (start)) & ((1L << (len)) - 1));

	return val;
}

static inline void WRITE_VPP_REG_MSK(u32 reg,
		const u32 value,
		const u32 msk)
{
	u32 val;

	val = aml_read_vcbus_s(reg);
	val = (val & (~msk)) | (value & msk);
	aml_write_vcbus_s(reg, val);
}

#ifndef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
#define VSYNC_WR_MPEG_REG(adr, val) WRITE_VPP_REG(adr, val)
#define VSYNC_RD_MPEG_REG(adr) READ_VPP_REG(adr)
#define VSYNC_WR_MPEG_REG_BITS(adr, val, start, len) \
	WRITE_VPP_REG_BITS(adr, val, start, len)

#define VSYNC_WR_MPEG_REG_VPP1(adr, val) WRITE_VCBUS_REG(adr, val)
#define VSYNC_RD_MPEG_REG_VPP1(adr) READ_VCBUS_REG(adr)
#define VSYNC_WR_MPEG_REG_BITS_VPP1(adr, val, start, len) \
	WRITE_VCBUS_REG_BITS(adr, val, start, len)

#define VSYNC_WR_MPEG_REG_VPP2(adr, val) WRITE_VCBUS_REG(adr, val)
#define VSYNC_RD_MPEG_REG_VPP2(adr) READ_VCBUS_REG(adr)
#define VSYNC_WR_MPEG_REG_BITS_VPP2(adr, val, start, len) \
	WRITE_VCBUS_REG_BITS(adr, val, start, len)
#else
int VSYNC_WR_MPEG_REG_BITS(u32 adr, u32 val, u32 start, u32 len);
u32 VSYNC_RD_MPEG_REG(u32 adr);
int VSYNC_WR_MPEG_REG(u32 adr, u32 val);

int VSYNC_WR_MPEG_REG_BITS_VPP1(u32 adr, u32 val, u32 start, u32 len);
u32 VSYNC_RD_MPEG_REG_VPP1(u32 adr);
int VSYNC_WR_MPEG_REG_VPP1(u32 adr, u32 val);

int VSYNC_WR_MPEG_REG_BITS_VPP2(u32 adr, u32 val, u32 start, u32 len);
u32 VSYNC_RD_MPEG_REG_VPP2(u32 adr);
int VSYNC_WR_MPEG_REG_VPP2(u32 adr, u32 val);
#endif

#endif

