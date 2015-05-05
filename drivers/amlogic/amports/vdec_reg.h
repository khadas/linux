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
#include "arch/register.h"
#include "arch/register_ops.h"



#define READ_AOREG(r) codec_aobus_read(r)
#define WRITE_AOREG(r, val) codec_aobus_write(r, val)


#define READ_VREG(r) codec_dosbus_read(r)


#define WRITE_VREG(r, val) codec_dosbus_write(r, val)


#define WRITE_VREG_BITS(r, val, start, len) \
	WRITE_VREG(r, (READ_VREG(r) & ~(((1L<<(len))-1)<<(start)))|\
		    ((unsigned)((val)&((1L<<(len))-1)) << (start)))
#define SET_VREG_MASK(r, mask) WRITE_VREG(r, READ_VREG(r) | (mask))
#define CLEAR_VREG_MASK(r, mask) WRITE_VREG(r, READ_VREG(r) & ~(mask))

#define READ_HREG(r) codec_dosbus_read(r|0x1000)
#define WRITE_HREG(r, val) codec_dosbus_write(r|0x1000, val)

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

#define WRITE_MPEG_REG(r, val)      codec_cbus_write(r, val)
#define READ_MPEG_REG(r) codec_cbus_read(r)
#define WRITE_MPEG_REG_BITS(r, val, start, len) \
	WRITE_MPEG_REG(r, (READ_MPEG_REG(r) & ~(((1L<<(len))-1)<<(start)))|\
		    ((unsigned)((val)&((1L<<(len))-1)) << (start)))

#define CLEAR_MPEG_REG_MASK(r, mask)\
	WRITE_MPEG_REG(r, READ_MPEG_REG(r) & ~(mask))
#define SET_MPEG_REG_MASK(r, mask)\
	WRITE_MPEG_REG(r, READ_MPEG_REG(r) | (mask))

#define WRITE_HHI_REG(r, val)      codec_hhibus_write(r, val)
#define READ_HHI_REG(r) codec_hhibus_read(r)
#define WRITE_HHI_REG_BITS(r, val, start, len) \
	WRITE_HHI_REG(r, (READ_HHI_REG(r) & ~(((1L<<(len))-1)<<(start)))|\
		    ((unsigned)((val)&((1L<<(len))-1)) << (start)))

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
#define READ_VCBUS_REG(r) codec_vcbus_read(r)
#define WRITE_VCBUS_REG(r, val) codec_vcbus_write(r, val)
#define WRITE_VCBUS_REG_BITS(r, val, start, len)\
	WRITE_VCBUS_REG(r, (READ_VCBUS_REG(r) & ~(((1L<<(len))-1)<<(start)))|\
		    ((unsigned)((val)&((1L<<(len))-1)) << (start)))
#define CLEAR_VCBUS_REG_MASK(r, mask)\
	WRITE_VCBUS_REG(r, READ_VCBUS_REG(r) & ~(mask))
#define SET_VCBUS_REG_MASK(r, mask)\
	WRITE_VCBUS_REG(r, READ_VCBUS_REG(r) | (mask))


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

