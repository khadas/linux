/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __VPP_REG_IO_H__
#define __VPP_REG_IO_H__

#define COEFF_NORM(a)  ((int)((((a) * 2048.0) + 1) / 2))
#define ADDR_PARAM(page, reg)  (((page) << 8) | (reg))

enum io_mode_e {
	EN_MODE_DIR = 0,
	EN_MODE_RDMA
};

struct _bit_s {
	unsigned char start;
	unsigned char len;
};

static inline u32 READ_VPP_REG(u32 reg)
{
	return aml_read_vcbus_s(reg);
}

static inline u32 READ_VPP_REG_BITS(u32 reg,
		const u32 start,
		const u32 len)
{
	u32 val;

	val = ((aml_read_vcbus_s(reg) >> (start)) & ((1L << (len)) - 1));
	return val;
}

static inline void WRITE_VPP_REG(u32 reg, const u32 value)
{
	aml_write_vcbus_s(reg, value);
}

static inline void WRITE_VPP_REG_BITS(u32 reg,
		const u32 value,
		const u32 start,
		const u32 len)
{
	aml_vcbus_update_bits_s(reg, value, start, len);
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
#define VSYNC_RD_MPEG_REG(adr) READ_VPP_REG(adr)
#define VSYNC_WR_MPEG_REG(adr, val) WRITE_VPP_REG(adr, val)
#define VSYNC_WR_MPEG_REG_BITS(adr, val, start, len)\
	WRITE_VPP_REG_BITS(adr, val, start, len)

#define VSYNC_WR_MPEG_REG_VPP1(adr, val) WRITE_VCBUS_REG(adr, val)
#define VSYNC_RD_MPEG_REG_VPP1(adr) READ_VCBUS_REG(adr)
#define VSYNC_WR_MPEG_REG_BITS_VPP1(adr, val, start, len) \
	WRITE_VCBUS_REG_BITS(adr, val, start, len)

#define VSYNC_WR_MPEG_REG_VPP2(adr, val) WRITE_VCBUS_REG(adr, val)
#define VSYNC_RD_MPEG_REG_VPP2(adr) READ_VCBUS_REG(adr)
#define VSYNC_WR_MPEG_REG_BITS_VPP2(adr, val, start, len) \
	WRITE_VCBUS_REG_BITS(adr, val, start, len)

int VSYNC_WR_MPEG_REG_BITS_VPP_SEL(u32 adr, u32 val, u32 start, u32 len, int vpp_sel);
u32 VSYNC_RD_MPEG_REG_VPP_SEL(u32 adr, int vpp_sel);
int VSYNC_WR_MPEG_REG_VPP_SEL(u32 adr, u32 val, int vpp_sel);
#else
u32 VSYNC_RD_MPEG_REG(u32 adr);
int VSYNC_WR_MPEG_REG(u32 adr, u32 val);
int VSYNC_WR_MPEG_REG_BITS(u32 adr, u32 val, u32 start, u32 len);

int VSYNC_WR_MPEG_REG_BITS_VPP1(u32 adr, u32 val, u32 start, u32 len);
u32 VSYNC_RD_MPEG_REG_VPP1(u32 adr);
int VSYNC_WR_MPEG_REG_VPP1(u32 adr, u32 val);

int VSYNC_WR_MPEG_REG_BITS_VPP2(u32 adr, u32 val, u32 start, u32 len);
u32 VSYNC_RD_MPEG_REG_VPP2(u32 adr);
int VSYNC_WR_MPEG_REG_VPP2(u32 adr, u32 val);

int VSYNC_WR_MPEG_REG_BITS_VPP_SEL(u32 adr, u32 val, u32 start, u32 len, int vpp_sel);
u32 VSYNC_RD_MPEG_REG_VPP_SEL(u32 adr, int vpp_sel);
int VSYNC_WR_MPEG_REG_VPP_SEL(u32 adr, u32 val, int vpp_sel);
#endif

static inline u32 READ_VPP_REG_BY_MODE(enum io_mode_e mode, u32 reg)
{
	if (mode == EN_MODE_DIR)
		return READ_VPP_REG(reg);
	else
		return VSYNC_RD_MPEG_REG(reg);
}

static inline void WRITE_VPP_REG_BY_MODE(enum io_mode_e mode, u32 reg, const u32 value)
{
	if (mode == EN_MODE_DIR)
		WRITE_VPP_REG(reg, value);
	else
		VSYNC_WR_MPEG_REG(reg, value);
}

static inline void WRITE_VPP_REG_BITS_BY_MODE(enum io_mode_e mode, u32 reg,
	const u32 value, const u32 start, const u32 len)
{
	if (mode == EN_MODE_DIR)
		WRITE_VPP_REG_BITS(reg, value, start, len);
	else
		VSYNC_WR_MPEG_REG_BITS(reg, value, start, len);
}

/*vsync for vpp_top_sel*/
static inline u32 VSYNC_READ_VPP_REG_VPP_SEL(u32 reg, int vpp_sel)
{
	if (vpp_sel == 2)
		return VSYNC_RD_MPEG_REG_VPP2(reg);
	else if (vpp_sel == 1)
		return VSYNC_RD_MPEG_REG_VPP1(reg);
	else
		return VSYNC_RD_MPEG_REG(reg);
}

static inline void VSYNC_WRITE_VPP_REG_VPP_SEL(u32 reg,
	const u32 value, int vpp_sel)
{
	if (vpp_sel == 2)
		VSYNC_WR_MPEG_REG_VPP2(reg, value);
	else if (vpp_sel == 1)
		VSYNC_WR_MPEG_REG_VPP1(reg, value);
	else
		VSYNC_WR_MPEG_REG(reg, value);
}

static inline void VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(u32 reg,
	const u32 value, const u32 start, const u32 len, int vpp_sel)
{
	if (vpp_sel == 2)
		VSYNC_WR_MPEG_REG_BITS_VPP2(reg, value, start, len);
	else if (vpp_sel == 1)
		VSYNC_WR_MPEG_REG_BITS_VPP1(reg, value, start, len);
	else
		VSYNC_WR_MPEG_REG_BITS(reg, value, start, len);
}

#endif

