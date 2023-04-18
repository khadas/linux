/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __REG_HELPER_H
#define __REG_HELPER_H

//#include "arch/vpp_regs.h"

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

static u32 offset_addr(u32 addr)
{
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

static inline void WRITE_VPP_REG_SEL(u32 reg,
				const u32 value,
				const u32 vpp_sel)
{
	aml_write_vcbus_s(offset_addr(reg), value);
}

static inline u32 READ_VCBUS_REG_SEL(u32 reg, const u32 vpp_sel)
{
	return aml_read_vcbus_s(offset_addr(reg));
}

static inline void WRITE_VCBUS_REG_BITS_SEL(u32 reg,
		const u32 value,
		const u32 start,
		const u32 len,
		const u32 vpp_sel)
{
	aml_vcbus_update_bits_s(offset_addr(reg), value, start, len);
}
#endif
