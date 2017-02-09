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

/* ********************************
 * register define
 * ********************************* */
#define REG_OFFSET_CBUS(reg)            ((reg << 2))
#define REG_OFFSET_HIU(reg)             (((reg & 0xff) << 2))
#define REG_OFFSET_VCBUS(reg)           ((reg << 2))

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
#define HHI_VPU_MEM_PD_REG2             0x104d

#define HHI_VPU_CLK_CNTL                0x106f
/* GX register */
#define HHI_VPU_CLKB_CNTL               0x1083
#define HHI_VAPBCLK_CNTL                0x107d

/* vpu clk gate */
/* hiu_bus */
#define HHI_GCLK_OTHER                  0x1054
/* vcbus */
#define VPU_CLK_GATE                    0x2723

#define VDIN0_OFFSET                    0x00
#define VDIN1_OFFSET                    0x80
#define VDIN_COM_GCLK_CTRL              0x121b
#define VDIN_COM_GCLK_CTRL2             0x1270
#define VDIN0_COM_GCLK_CTRL          ((VDIN0_OFFSET << 2) + VDIN_COM_GCLK_CTRL)
#define VDIN0_COM_GCLK_CTRL2         ((VDIN0_OFFSET << 2) + VDIN_COM_GCLK_CTRL2)
#define VDIN1_COM_GCLK_CTRL          ((VDIN1_OFFSET << 2) + VDIN_COM_GCLK_CTRL)
#define VDIN1_COM_GCLK_CTRL2         ((VDIN1_OFFSET << 2) + VDIN_COM_GCLK_CTRL2)

#define DI_CLKG_CTRL                    0x1718

#define VPP_GCLK_CTRL0                  0x1d72
#define VPP_GCLK_CTRL1                  0x1d73
#define VPP_SC_GCLK_CTRL                0x1d74
#define VPP_SRSCL_GCLK_CTRL             0x1d77
#define VPP_OSDSR_GCLK_CTRL             0x1d78
#define VPP_XVYCC_GCLK_CTRL             0x1d79

extern unsigned int vpu_hiu_read(unsigned int _reg);
extern void vpu_hiu_write(unsigned int _reg, unsigned int _value);
extern void vpu_hiu_setb(unsigned int _reg, unsigned int _value,
		unsigned int _start, unsigned int _len);
extern unsigned int vpu_hiu_getb(unsigned int _reg,
		unsigned int _start, unsigned int _len);
extern void vpu_hiu_set_mask(unsigned int _reg, unsigned int _mask);
extern void vpu_hiu_clr_mask(unsigned int _reg, unsigned int _mask);

extern unsigned int vpu_vcbus_read(unsigned int _reg);
extern void vpu_vcbus_write(unsigned int _reg, unsigned int _value);
extern void vpu_vcbus_setb(unsigned int _reg, unsigned int _value,
		unsigned int _start, unsigned int _len);
extern unsigned int vpu_vcbus_getb(unsigned int _reg,
		unsigned int _start, unsigned int _len);
extern void vpu_vcbus_set_mask(unsigned int _reg, unsigned int _mask);
extern void vpu_vcbus_clr_mask(unsigned int _reg, unsigned int _mask);

#endif
