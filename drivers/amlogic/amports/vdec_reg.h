/*
 * drivers/amlogic/amports/vdec_reg.h
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


#ifndef VDEC_REG_H
#define VDEC_REG_H

#include <linux/kernel.h>
#include <linux/amlogic/amports/vformat.h>
#include "vdec.h"
#include <linux/amlogic/iomap.h>
#include <linux/io.h>

#define BASE_IRQ 32
#define AM_IRQ(reg)   (reg + BASE_IRQ)
#define INT_DOS_MAILBOX_0       AM_IRQ(43)
#define INT_DOS_MAILBOX_1       AM_IRQ(44)
#define INT_DOS_MAILBOX_2       AM_IRQ(45)
#define INT_VIU_VSYNC           AM_IRQ(3)
#define INT_DEMUX               AM_IRQ(23)
#define INT_PARSER              AM_IRQ(32)

/* #if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON8 */
/* #define READ_AOREG(r) (__raw_readl((volatile void __iomem *)\
   AOBUS_REG_ADDR(r))) */
/* #define WRITE_AOREG(r, val) __raw_writel(val,\
   (volatile void __iomem *)(AOBUS_REG_ADDR(r)))' */
#define READ_AOREG(r) aml_read_aobus(r)
#define WRITE_AOREG(r, val) aml_write_aobus(r, val)
/* aml_read_vcbus(unsigned int reg) */
#define INT_VDEC INT_DOS_MAILBOX_1
#define INT_VDEC2 INT_DOS_MAILBOX_0
/* #else */
/* /#define INT_VDEC INT_MAILBOX_1A */
/* /#endif */

#define READ_VREG(r) aml_read_dosbus(r)
/* static inline u32 READ_VREG(u32 r) */
/* { */
/* if (((r) > 0x2000) && ((r) < 0x3000) && !vdec_on(2)) dump_stack(); */
/* return __raw_readl((volatile void __iomem *)DOS_REG_ADDR(r)); */
/* } */

#define WRITE_VREG(r, val) aml_write_dosbus(r, val)
/* static inline void WRITE_VREG(u32 r, u32 val) */
/* { */
/* if (((r) > 0x2000) && ((r) < 0x3000) && !vdec_on(2)) dump_stack(); */
/* __raw_writel(val, (volatile void __iomem *)(DOS_REG_ADDR(r))); */
/* } */

#define WRITE_VREG_BITS(r, val, start, len) \
	WRITE_VREG(r, (READ_VREG(r) & ~(((1L<<(len))-1)<<(start)))|\
		    ((unsigned)((val)&((1L<<(len))-1)) << (start)))
#define SET_VREG_MASK(r, mask) WRITE_VREG(r, READ_VREG(r) | (mask))
#define CLEAR_VREG_MASK(r, mask) WRITE_VREG(r, READ_VREG(r) & ~(mask))

#define READ_HREG(r) aml_read_dosbus(r|0x1000)
#define WRITE_HREG(r, val) aml_write_dosbus(r|0x1000, val)

#define WRITE_HREG_BITS(r, val, start, len) \
	WRITE_HREG(r, (READ_HREG(r) & ~(((1L<<(len))-1)<<(start)))|\
		    ((unsigned)((val)&((1L<<(len))-1)) << (start)))
#define SET_HREG_MASK(r, mask) WRITE_HREG(r, READ_HREG(r) | (mask))
#define CLEAR_HREG_MASK(r, mask) WRITE_HREG(r, READ_HREG(r) & ~(mask))
/*TODO */
#define READ_SEC_REG(r)
#define WRITE_SEC_REG(r, val)
#define WRITE_SEC_REG_BITS(r, val, start, len) \
	WRITE_SEC_REG(r, (READ_SEC_REG(r) & ~(((1L<<(len))-1)<<(start)))|\
		    ((unsigned)((val)&((1L<<(len))-1)) << (start)))

#define WRITE_MPEG_REG(r, val)      aml_write_cbus(r, val)
#define READ_MPEG_REG(r) aml_read_cbus(r)
#define WRITE_MPEG_REG_BITS(r, val, start, len) \
	WRITE_MPEG_REG(r, (READ_MPEG_REG(r) & ~(((1L<<(len))-1)<<(start)))|\
		    ((unsigned)((val)&((1L<<(len))-1)) << (start)))

#define CLEAR_MPEG_REG_MASK(r, mask)\
	WRITE_MPEG_REG(r, READ_MPEG_REG(r) & ~(mask))
#define SET_MPEG_REG_MASK(r, mask)\
	WRITE_MPEG_REG(r, READ_MPEG_REG(r) | (mask))

#define ASSIST_MBOX1_CLR_REG VDEC_ASSIST_MBOX1_CLR_REG
#define ASSIST_MBOX1_MASK VDEC_ASSIST_MBOX1_MASK
#define ASSIST_AMR1_INT0 VDEC_ASSIST_AMR1_INT0
#define ASSIST_AMR1_INT1 VDEC_ASSIST_AMR1_INT1
#define ASSIST_AMR1_INT2 VDEC_ASSIST_AMR1_INT2
#define ASSIST_AMR1_INT3 VDEC_ASSIST_AMR1_INT3
#define ASSIST_AMR1_INT4 VDEC_ASSIST_AMR1_INT4
#define ASSIST_AMR1_INT5 VDEC_ASSIST_AMR1_INT5
#define ASSIST_AMR1_INT6 VDEC_ASSIST_AMR1_INT6
#define ASSIST_AMR1_INT7 VDEC_ASSIST_AMR1_INT7
#define ASSIST_AMR1_INT8 VDEC_ASSIST_AMR1_INT8
#define ASSIST_AMR1_INT9 VDEC_ASSIST_AMR1_INT9

/*TODO reg*/
#define READ_VCBUS_REG(r) aml_read_vcbus(r)
#define WRITE_VCBUS_REG(r, val) aml_write_vcbus(r, val)
#define WRITE_VCBUS_REG_BITS(r, val, start, len)\
	WRITE_VCBUS_REG(r, (READ_VCBUS_REG(r) & ~(((1L<<(len))-1)<<(start)))|\
		    ((unsigned)((val)&((1L<<(len))-1)) << (start)))
#define CLEAR_VCBUS_REG_MASK(r, mask)\
	WRITE_VCBUS_REG(r, READ_VCBUS_REG(r) & ~(mask))
#define SET_VCBUS_REG_MASK(r, mask)\
	WRITE_VCBUS_REG(r, READ_VCBUS_REG(r) | (mask))
#if 0
#if MESON_CPU_TYPE < MESON_CPU_TYPE_MESON8
#define READ_VCBUS_REG(r) READ_CBUS_REG(r)
#define WRITE_VCBUS_REG(r, val) WRITE_CBUS_REG(r, val)
#define WRITE_VCBUS_REG_BITS(r, val, from, size)\
	WRITE_CBUS_REG_BITS(r, val, from, size)
#define SET_VCBUS_REG_MASK(r, mask) SET_CBUS_REG_MASK(r, mask)
#define CLEAR_VCBUS_REG_MASK(r, mask) CLEAR_CBUS_REG_MASK(r, mask)
#endif
#endif

/****************logo relative part *****************************************/
#define ASSIST_MBOX1_CLR_REG VDEC_ASSIST_MBOX1_CLR_REG
#define ASSIST_MBOX1_MASK VDEC_ASSIST_MBOX1_MASK
#define RESET_PSCALE        (1<<4)
#define RESET_IQIDCT        (1<<2)
#define RESET_MC            (1<<3)
#define MEM_BUFCTRL_MANUAL      (1<<1)
#define MEM_BUFCTRL_INIT        (1<<0)
#define MEM_LEVEL_CNT_BIT       18
#define MEM_FIFO_CNT_BIT        16
#define MEM_FILL_ON_LEVEL       (1<<10)
#define MEM_CTRL_EMPTY_EN       (1<<2)
#define MEM_CTRL_FILL_EN        (1<<1)
#define MEM_CTRL_INIT           (1<<0)

#endif				/* VDEC_REG_H */
