/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * drivers/amlogic/media/enhancement/amvecm/reg_helper.h
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

#ifndef __REG_HELPER_H
#define __REG_HELPER_H

//#include "arch/vpp_regs.h"
#include <linux/amlogic/media/amvecm/amvecm.h>
#include "arch/ve_regs.h"
#include "arch/cm_regs.h"

#define CLR_BIT(x) (~(0x01 << (x)))
#define CLR_BITS(x, y) ((~((0x01 << (y)) - 1)) << (x))
#define SET_BIT(x) (0x01 << (x))
#define GET_BIT(x) (0x01 << (x))
#define GET_BITS(x, y) (((0x01 << (y)) - 1) << (x))

#define srsharp0_sharp_hvsize 0x3e00
#define srsharp0_pkosht_vsluma_lut_h 0x3e81
#define srsharp1_sharp_hvsize 0x3f00
#define srsharp1_pkosht_vsluma_lut_h 0x3f81
#define srsharp1_lc_input_mux 0x3fb1
#define srsharp1_lc_map_ram_data 0x3ffe

/* useful inline functions to handle different offset */
static inline bool cpu_after_eq_t7(void)
{
	return cpu_after_eq(MESON_CPU_MAJOR_ID_T7);
}

static inline bool cpu_after_eq_tm2b(void)
{
	return cpu_after_eq(MESON_CPU_MAJOR_ID_TM2);
}

static inline bool cpu_after_eq_tl1(void)
{
	return cpu_after_eq(MESON_CPU_MAJOR_ID_TL1);
}

static inline bool is_sr0_reg(u32 addr)
{
	return (addr >= srsharp0_sharp_hvsize &&
		addr <= srsharp0_pkosht_vsluma_lut_h);
}

static inline bool is_sr1_reg(u32 addr)
{
	return (addr >= srsharp1_sharp_hvsize &&
		addr <= srsharp1_pkosht_vsluma_lut_h);
}

static inline bool is_lc_reg(u32 addr)
{
	return (addr >= srsharp1_lc_input_mux &&
		addr <= srsharp1_lc_map_ram_data);
}

static inline bool is_sr0_dnlpv2_reg(u32 addr)
{
	/*because s5 have no sr0 dnlp
	 *old sr0 reg overlap with slice3 hdr reg
	 */
	if (chip_type_id == chip_s5)
		return 0;

	return (addr >= SRSHARP0_DNLP2_00 &&
		addr <= SRSHARP0_DNLP2_31);
}

static inline bool is_sr1_dnlpv2_reg(u32 addr)
{
	return (addr >= SRSHARP1_DNLP2_00 &&
		addr <= SRSHARP1_DNLP2_31);
}

static inline u32 get_sr0_offset(void)
{
	/*sr0  register shfit*/
	if (cpu_after_eq_t7())
		return 0x1200;
	else if (cpu_after_eq_tm2b())
		return 0x1200;
	else if (cpu_after_eq_tl1())
		return 0x0;
	else if (is_meson_g12a_cpu() ||
		 is_meson_g12b_cpu() ||
		is_meson_sm1_cpu())
		return 0x0;

	return 0xfffff400 /*-0xc00*/;
}

static inline u32 get_sr1_offset(void)
{
	/*sr1 register shfit*/
	if (cpu_after_eq_t7())
		return 0x1300;
	else if (cpu_after_eq_tm2b())
		return 0x1300;
	else if (cpu_after_eq_tl1())
		return 0x0;
	else if (is_meson_g12a_cpu() ||
		 is_meson_g12b_cpu() ||
		is_meson_sm1_cpu())
		return 0x0;

	return 0xfffff380; /*-0xc80*/;
}

static inline u32 get_lc_offset(void)
{
	/* lc register shfit*/
	if (cpu_after_eq_t7())
		return 0x1300;
	else if (cpu_after_eq_tm2b())
		return 0x1300;

	return 0;
}

static inline u32 get_sr0_dnlp2_offset(void)
{
	/* SHARP0_DNLP_00 shfit*/
	if (cpu_after_eq_t7())
		return 0x1200;
	else if (cpu_after_eq_tm2b())
		return 0x1200;

	return 0;
}

static inline u32 get_sr1_dnlp2_offset(void)
{
	/* SHARP1_DNLP_00 shfit*/
	if (cpu_after_eq_t7())
		return 0x1300;
	else if (cpu_after_eq_tm2b())
		return 0x1300;

	return 0;
}

static u32 offset_addr(u32 addr)
{
	if (is_sr0_reg(addr))
		return addr + get_sr0_offset();
	else if (is_sr1_reg(addr))
		return addr + get_sr1_offset();
	else if (is_sr0_dnlpv2_reg(addr))
		return addr + get_sr0_dnlp2_offset();
	else if (is_sr1_dnlpv2_reg(addr))
		return addr + get_sr1_dnlp2_offset();
	else if (is_lc_reg(addr))
		return addr + get_lc_offset();

	return addr;
}

static inline void WRITE_VPP_REG(u32 reg,
				 const u32 value)
{
	aml_write_vcbus_s(offset_addr(reg), value);
}

static inline void WRITE_VPP_REG_S5(u32 reg,
				 const u32 value)
{
	aml_write_vcbus(reg, value);
}

static inline u32 READ_VPP_REG(u32 reg)
{
	return aml_read_vcbus_s(offset_addr(reg));
}

static inline u32 READ_VPP_REG_S5(u32 reg)
{
	return aml_read_vcbus(reg);
}

static inline void WRITE_VPP_REG_EX(u32 reg,
				    const u32 value,
				    bool add_offset)
{
	if (add_offset)
		reg = offset_addr(reg);

	aml_write_vcbus_s(reg, value);
}

static inline u32 READ_VPP_REG_EX(u32 reg,
				  bool add_offset)
{
	if (add_offset)
		reg = offset_addr(reg);

	return aml_read_vcbus_s(reg);
}

static inline void WRITE_VPP_REG_BITS(u32 reg,
				      const u32 value,
		const u32 start,
		const u32 len)
{
	aml_vcbus_update_bits_s(offset_addr(reg), value, start, len);
}

static inline void WRITE_VPP_REG_BITS_S5(u32 reg,
				      const u32 value,
		const u32 start,
		const u32 len)
{
	aml_write_vcbus(reg, ((aml_read_vcbus(reg) &
			     ~(((1L << (len)) - 1) << (start))) |
			    (((value) & ((1L << (len)) - 1)) << (start))));
}

static inline u32 READ_VPP_REG_BITS(u32 reg,
				    const u32 start,
				    const u32 len)
{
	u32 val;
	u32 reg1 = offset_addr(reg);

	val = ((aml_read_vcbus_s(reg1) >> (start)) & ((1L << (len)) - 1));

	return val;
}

#ifndef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
#define VSYNC_WR_MPEG_REG(adr, val) WRITE_VPP_REG(adr, val)
#define VSYNC_RD_MPEG_REG(adr) READ_VPP_REG(adr)
#define VSYNC_WR_MPEG_REG_BITS(adr, val, start, len) \
	WRITE_VPP_REG_BITS(adr, val, start, len)

#define _VSYNC_WR_MPEG_REG(adr, val) WRITE_VPP_REG(adr, val)
#define _VSYNC_RD_MPEG_REG(adr) READ_VPP_REG(adr)
#define _VSYNC_WR_MPEG_REG_BITS(adr, val, start, len) \
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
int VSYNC_WR_MPEG_REG_BITS(u32 adr, u32 val, u32 start, u32 len);
u32 VSYNC_RD_MPEG_REG(u32 adr);
int VSYNC_WR_MPEG_REG(u32 adr, u32 val);

int _VSYNC_WR_MPEG_REG(u32 adr, u32 val);
int _VSYNC_WR_MPEG_REG_BITS(u32 adr, u32 val, u32 start, u32 len);
u32 _VSYNC_RD_MPEG_REG(u32 adr);

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

/* vsync for vpp_top0 */
static inline void VSYNC_WRITE_VPP_REG(u32 reg,
				       const u32 value)
{
	VSYNC_WR_MPEG_REG(offset_addr(reg), value);
}

static inline u32 VSYNC_READ_VPP_REG(u32 reg)
{
	return VSYNC_RD_MPEG_REG(offset_addr(reg));
}

static inline void VSYNC_WRITE_VPP_REG_EX(u32 reg,
					  const u32 value,
					  bool add_offset)
{
	if (add_offset)
		reg = offset_addr(reg);
	VSYNC_WR_MPEG_REG(reg, value);
}

static inline u32 VSYNC_READ_VPP_REG_EX(u32 reg,
					bool add_offset)
{
	if (add_offset)
		reg = offset_addr(reg);
	return VSYNC_RD_MPEG_REG(reg);
}

static inline void VSYNC_WRITE_VPP_REG_BITS(u32 reg,
					    const u32 value,
		const u32 start,
		const u32 len)
{
	VSYNC_WR_MPEG_REG_BITS(offset_addr(reg), value, start, len);
}

/* vsync for vpp_top1 */
static inline void VSYNC_WRITE_VPP_REG_VPP1(u32 reg,
				       const u32 value)
{
	VSYNC_WR_MPEG_REG_VPP1(offset_addr(reg), value);
}

static inline u32 VSYNC_READ_VPP_REG_VPP1(u32 reg)
{
	return VSYNC_RD_MPEG_REG_VPP1(offset_addr(reg));
}

static inline void VSYNC_WRITE_VPP_REG_EX_VPP1(u32 reg,
					  const u32 value,
					  bool add_offset)
{
	if (add_offset)
		reg = offset_addr(reg);
	VSYNC_WR_MPEG_REG_VPP1(reg, value);
}

static inline u32 VSYNC_READ_VPP_REG_EX_VPP1(u32 reg,
					bool add_offset)
{
	if (add_offset)
		reg = offset_addr(reg);
	return VSYNC_RD_MPEG_REG_VPP1(reg);
}

static inline void VSYNC_WRITE_VPP_REG_BITS_VPP1(u32 reg,
					    const u32 value,
		const u32 start,
		const u32 len)
{
	VSYNC_WR_MPEG_REG_BITS_VPP1(offset_addr(reg), value, start, len);
}

/* vsync for vpp_top2 */
static inline void VSYNC_WRITE_VPP_REG_VPP2(u32 reg,
				       const u32 value)
{
	VSYNC_WR_MPEG_REG_VPP2(offset_addr(reg), value);
}

static inline u32 VSYNC_READ_VPP_REG_VPP2(u32 reg)
{
	return VSYNC_RD_MPEG_REG_VPP2(offset_addr(reg));
}

static inline void VSYNC_WRITE_VPP_REG_EX_VPP2(u32 reg,
					  const u32 value,
					  bool add_offset)
{
	if (add_offset)
		reg = offset_addr(reg);
	VSYNC_WR_MPEG_REG_VPP2(reg, value);
}

static inline u32 VSYNC_READ_VPP_REG_EX_VPP2(u32 reg,
					bool add_offset)
{
	if (add_offset)
		reg = offset_addr(reg);
	return VSYNC_RD_MPEG_REG_VPP2(reg);
}

static inline void VSYNC_WRITE_VPP_REG_BITS_VPP2(u32 reg,
					    const u32 value,
		const u32 start,
		const u32 len)
{
	VSYNC_WR_MPEG_REG_BITS_VPP2(offset_addr(reg), value, start, len);
}

static inline void VSYNC_WR_MPEG_REG_BITS_S5(u32 reg,
		      const u32 value,
		      const u32 start,
		      const u32 len)
{
	aml_write_vcbus(reg, ((aml_read_vcbus(reg) &
			     ~(((1L << (len)) - 1) << (start))) |
			    (((value) & ((1L << (len)) - 1)) << (start))));
}

/* vsync for vpp_top_sel */
static inline void VSYNC_WRITE_VPP_REG_VPP_SEL(u32 reg,
				       const u32 value, int vpp_sel)
{
	if (vpp_sel == 0xff)
		aml_write_vcbus_s(offset_addr(reg), value);
	else if (vpp_sel == 0xfe)
		aml_write_vcbus(reg, value);
	else if (vpp_sel == 2)
		VSYNC_WR_MPEG_REG_VPP2(offset_addr(reg), value);
	else if (vpp_sel == 1)
		VSYNC_WR_MPEG_REG_VPP1(offset_addr(reg), value);
	else
		VSYNC_WR_MPEG_REG(offset_addr(reg), value);
}

static inline u32 VSYNC_READ_VPP_REG_VPP_SEL(u32 reg, int vpp_sel)
{
	if (vpp_sel == 0xff)
		return aml_read_vcbus_s(offset_addr(reg));
	else if (vpp_sel == 0xfe)
		return aml_read_vcbus(reg);
	else if (vpp_sel == 2)
		return VSYNC_RD_MPEG_REG_VPP2(offset_addr(reg));
	else if (vpp_sel == 1)
		return VSYNC_RD_MPEG_REG_VPP1(offset_addr(reg));
	else
		return VSYNC_RD_MPEG_REG(offset_addr(reg));
}

static inline void VSYNC_WRITE_VPP_REG_EX_VPP_SEL(u32 reg,
					  const u32 value,
					  bool add_offset, int vpp_sel)
{
	if (add_offset)
		reg = offset_addr(reg);
	if (vpp_sel == 2)
		VSYNC_WR_MPEG_REG_VPP2(reg, value);
	else if (vpp_sel == 1)
		VSYNC_WR_MPEG_REG_VPP1(reg, value);
	else
		VSYNC_WR_MPEG_REG(reg, value);
}

static inline u32 VSYNC_READ_VPP_REG_EX_VPP_SEL(u32 reg,
					bool add_offset, int vpp_sel)
{
	if (add_offset)
		reg = offset_addr(reg);
	if (vpp_sel == 2)
		return VSYNC_RD_MPEG_REG_VPP2(reg);
	else if (vpp_sel == 1)
		return VSYNC_RD_MPEG_REG_VPP1(reg);
	else
		return VSYNC_RD_MPEG_REG(reg);
}

static inline void VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(u32 reg,
					    const u32 value,
		const u32 start,
		const u32 len, int vpp_sel)
{
	if (vpp_sel == 0xff)
		aml_vcbus_update_bits_s(offset_addr(reg), value, start, len);
	else if (vpp_sel == 0xfe)
		VSYNC_WR_MPEG_REG_BITS_S5(reg, value, start, len);
	else if (vpp_sel == 2)
		VSYNC_WR_MPEG_REG_BITS_VPP2(offset_addr(reg), value, start, len);
	else if (vpp_sel == 1)
		VSYNC_WR_MPEG_REG_BITS_VPP1(offset_addr(reg), value, start, len);
	else
		VSYNC_WR_MPEG_REG_BITS(offset_addr(reg), value, start, len);
}

#endif
