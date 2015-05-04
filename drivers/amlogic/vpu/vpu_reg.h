/*
 * drivers/amlogic/vpu/vpu_reg.h
 *
 * Copyright (C) 2015 Amlogic, Inc. All rights reserved.
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

#ifndef __VPU_REG_H__
#define __VPU_REG_H__
#include <linux/amlogic/iomap.h>

extern void __iomem *reg_base_aobus;
extern void __iomem *reg_base_cbus;

/* ********************************
 * register define
 * ********************************* */
/* offset address */
#define AO_RTI_GEN_PWR_SLEEP0           ((0x00 << 10) | (0x3a << 2))

/* M8M2 register */
#define HHI_GP_PLL_CNTL                 0x1010
/* G9TV register */
#define HHI_GP1_PLL_CNTL                0x1016
#define HHI_GP1_PLL_CNTL2               0x1017
#define HHI_GP1_PLL_CNTL3               0x1018
#define HHI_GP1_PLL_CNTL4               0x1019

#define HHI_MEM_PD_REG0                 0x1040
#define HHI_VPU_MEM_PD_REG0             0x1041
#define HHI_VPU_MEM_PD_REG1             0x1042

#define HHI_VPU_CLK_CNTL                0x106f

#define RESET0_REGISTER                 0x1101
#define RESET1_REGISTER                 0x1102
#define RESET2_REGISTER                 0x1103
#define RESET3_REGISTER                 0x1104
#define RESET4_REGISTER                 0x1105
#define RESET0_MASK                     0x1110
#define RESET1_MASK                     0x1111
#define RESET2_MASK                     0x1112
#define RESET3_MASK                     0x1113
#define RESET4_MASK                     0x1114

/* memory mapping */
#define AOBUS_REG_ADDR(reg)             (reg_base_aobus+reg)
#define CBUS_REG_ADDR(reg)              (reg_base_cbus+(reg<<2))

/* physical address */
#define P_AO_RTI_GEN_PWR_SLEEP0         AOBUS_REG_ADDR(AO_RTI_GEN_PWR_SLEEP0)

#define P_HHI_GP_PLL_CNTL               CBUS_REG_ADDR(HHI_GP_PLL_CNTL)
#define P_HHI_GP1_PLL_CNTL              CBUS_REG_ADDR(HHI_GP1_PLL_CNTL)
#define P_HHI_GP1_PLL_CNTL2             CBUS_REG_ADDR(HHI_GP1_PLL_CNTL2)
#define P_HHI_GP1_PLL_CNTL3             CBUS_REG_ADDR(HHI_GP1_PLL_CNTL3)
#define P_HHI_GP1_PLL_CNTL4             CBUS_REG_ADDR(HHI_GP1_PLL_CNTL4)
#define P_HHI_VPU_MEM_PD_REG0           CBUS_REG_ADDR(HHI_VPU_MEM_PD_REG0)
#define P_HHI_VPU_MEM_PD_REG1           CBUS_REG_ADDR(HHI_VPU_MEM_PD_REG1)
#define P_HHI_VPU_CLK_CNTL              CBUS_REG_ADDR(HHI_VPU_CLK_CNTL)
#define P_RESET0_REGISTER               CBUS_REG_ADDR(RESET0_REGISTER)
#define P_RESET1_REGISTER               CBUS_REG_ADDR(RESET1_REGISTER)
#define P_RESET2_REGISTER               CBUS_REG_ADDR(RESET2_REGISTER)
#define P_RESET3_REGISTER               CBUS_REG_ADDR(RESET3_REGISTER)
#define P_RESET4_REGISTER               CBUS_REG_ADDR(RESET4_REGISTER)
#define P_RESET0_MASK                   CBUS_REG_ADDR(RESET0_MASK)
#define P_RESET1_MASK                   CBUS_REG_ADDR(RESET1_MASK)
#define P_RESET2_MASK                   CBUS_REG_ADDR(RESET2_MASK)
#define P_RESET3_MASK                   CBUS_REG_ADDR(RESET3_MASK)
#define P_RESET4_MASK                   CBUS_REG_ADDR(RESET4_MASK)

/* ********************************
 * register access api
 * ********************************* */
#if 0
/* use physical address */
static inline uint32_t aml_read32(void __iomem *_reg)
{
	return readl_relaxed(_reg);
};

static inline void aml_write32(void __iomem *_reg, const uint32_t _value)
{
	writel_relaxed(_value, _reg);
};

static inline void aml_setb32(void __iomem *_reg,
		const uint32_t _value,
		const uint32_t _start,
		const uint32_t _len)
{
	writel_relaxed(((readl_relaxed(_reg) &
			~(((1L << (_len))-1) << (_start))) |
			(((_value)&((1L<<(_len))-1)) << (_start))),
			_reg);
}

static inline uint32_t aml_getb32(void __iomem *_reg,
		const uint32_t _start, const uint32_t _len)
{
	return (readl_relaxed(_reg) >> (_start)) &
			((1L << (_len)) - 1);
}

static inline void aml_set32_mask(void __iomem *_reg,
		const uint32_t _mask)
{
	writel_relaxed((readl_relaxed(_reg) | (_mask)), _reg);
}

static inline void aml_clr32_mask(void __iomem *_reg,
		const uint32_t _mask)
{
	writel_relaxed((readl_relaxed(_reg) & (~(_mask))), _reg);
}
#else
/* use offset address */
static inline uint32_t vpu_reg_read(unsigned int _reg)
{
	return aml_read_cbus(_reg);
};

static inline void vpu_reg_write(unsigned int _reg, unsigned int _value)
{
	aml_write_cbus(_reg, _value);
};

static inline void vpu_reg_setb(unsigned int _reg,
		unsigned int _value,
		unsigned int _start,
		unsigned int _len)
{
	aml_write_cbus(_reg, ((aml_read_cbus(_reg) &
			~(((1L << (_len))-1) << (_start))) |
			(((_value)&((1L<<(_len))-1)) << (_start))));
}

static inline uint32_t vpu_reg_getb(unsigned int _reg,
		unsigned int _start, unsigned int _len)
{
	return (aml_read_cbus(_reg) >> (_start)) & ((1L << (_len)) - 1);
}

static inline void vpu_set_mask(unsigned int _reg, unsigned int _mask)
{
	aml_write_cbus(_reg, (aml_read_cbus(_reg) | (_mask)));
}

static inline void vpu_clr_mask(unsigned int _reg, unsigned int _mask)
{
	aml_write_cbus(_reg, (aml_read_cbus(_reg) & (~(_mask))));
}

static inline void vpu_ao_setb(unsigned int _reg,
		unsigned int _value,
		unsigned int _start,
		unsigned int _len)
{
	aml_write_aobus(_reg, ((aml_read_aobus(_reg) &
			~(((1L << (_len))-1) << (_start))) |
			(((_value)&((1L<<(_len))-1)) << (_start))));
}
#endif

#endif
