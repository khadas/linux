/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __VPU_REG_H__
#define __VPU_REG_H__
#include <linux/platform_device.h>

#define VPU_HIU_BASE                 0xff63c000
#define VPU_VCBUS_BASE               0xff900000
#define VPU_CBUS_BASE                0xffd00000
#define VPU_AOBUS_BASE               0xff800000

#define VPU_REG_END                  0xffff

/* ********************************
 * register define
 * *********************************
 */

#define AO_RTI_GEN_PWR_SLEEP0        0x03a
#define AO_RTI_GEN_PWR_ISO0          0x03b

/* HIU bus */

#define HHI_MEM_PD_REG0              0x40
#define HHI_VPU_MEM_PD_REG0          0x41
#define HHI_VPU_MEM_PD_REG1          0x42
#define HHI_VPU_MEM_PD_REG2          0x4d
#define HHI_VPU_MEM_PD_REG3          0x4e
#define HHI_VPU_MEM_PD_REG4          0x4c
#define HHI_VPU_MEM_PD_REG3_SM1      0x43
#define HHI_VPU_MEM_PD_REG4_SM1      0x44

#define HHI_VPU_CLKC_CNTL            0x6d
#define HHI_VPU_CLK_CNTL             0x6f
#define HHI_VAPBCLK_CNTL             0x7d

/* cbus */
#define RESET0_LEVEL                 0x0420
#define RESET1_LEVEL                 0x0421
#define RESET2_LEVEL                 0x0422
#define RESET3_LEVEL                 0x0423
#define RESET4_LEVEL                 0x0424
#define RESET5_LEVEL                 0x0425
#define RESET6_LEVEL                 0x0426
#define RESET7_LEVEL                 0x0427

/* vpu clk gate */
/* vcbus */
#define VPU_CLK_GATE                 0x2723

#define VDIN0_OFFSET                 0x00
#define VDIN1_OFFSET                 0x80
#define VDIN_COM_GCLK_CTRL           0x121b
#define VDIN_COM_GCLK_CTRL2          0x1270
#define VDIN0_COM_GCLK_CTRL          (VDIN0_OFFSET + VDIN_COM_GCLK_CTRL)
#define VDIN0_COM_GCLK_CTRL2         (VDIN0_OFFSET + VDIN_COM_GCLK_CTRL2)
#define VDIN1_COM_GCLK_CTRL          (VDIN1_OFFSET + VDIN_COM_GCLK_CTRL)
#define VDIN1_COM_GCLK_CTRL2         (VDIN1_OFFSET + VDIN_COM_GCLK_CTRL2)

#define DI_CLKG_CTRL                 0x1718

#define VPP_GCLK_CTRL0               0x1d72
#define VPP_GCLK_CTRL1               0x1d73
#define VPP_SC_GCLK_CTRL             0x1d74
#define VPP_SRSCL_GCLK_CTRL          0x1d77
#define VPP_OSDSR_GCLK_CTRL          0x1d78
#define VPP_XVYCC_GCLK_CTRL          0x1d79

#define DOLBY_TV_CLKGATE_CTRL        0x33f1
#define DOLBY_CORE1_CLKGATE_CTRL     0x33f2
#define DOLBY_CORE2A_CLKGATE_CTRL    0x3432
#define DOLBY_CORE3_CLKGATE_CTRL     0x36f0

#define VPU_RDARB_MODE_L1C1          0x2790
#define VPU_RDARB_MODE_L1C2          0x2799
#define VPU_RDARB_MODE_L2C1          0x279d
#define VPU_WRARB_MODE_L2C1          0x27a2

int vpu_ioremap(struct platform_device *pdev);

unsigned int vpu_hiu_read(unsigned int _reg);
void vpu_hiu_write(unsigned int _reg, unsigned int _value);
void vpu_hiu_setb(unsigned int _reg, unsigned int _value,
		  unsigned int _start, unsigned int _len);
unsigned int vpu_hiu_getb(unsigned int _reg,
			  unsigned int _start, unsigned int _len);
void vpu_hiu_set_mask(unsigned int _reg, unsigned int _mask);
void vpu_hiu_clr_mask(unsigned int _reg, unsigned int _mask);

unsigned int vpu_cbus_read(unsigned int _reg);
void vpu_cbus_write(unsigned int _reg, unsigned int _value);
void vpu_cbus_setb(unsigned int _reg, unsigned int _value,
		   unsigned int _start, unsigned int _len);
void vpu_cbus_set_mask(unsigned int _reg, unsigned int _mask);
void vpu_cbus_clr_mask(unsigned int _reg, unsigned int _mask);

unsigned int vpu_ao_read(unsigned int _reg);
void vpu_ao_write(unsigned int _reg, unsigned int _value);
void vpu_ao_setb(unsigned int _reg, unsigned int _value,
		 unsigned int _start, unsigned int _len);

#endif
