/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef VDEC_REG_H
#define VDEC_REG_H

#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/amlogic/media/registers/register.h>
#include <linux/amlogic/media/registers/register_ops.h>
#include <linux/amlogic/media/registers/register_map.h>

#define READ_DMCREG(r) aml_read_dmcbus(r)
#define WRITE_DMCREG(r, val) aml_write_dmcbus(r, val)

#define READ_AOREG(r) aml_read_aobus(r)
#define WRITE_AOREG(r, val) aml_write_aobus(r, val)

#define READ_VREG(r) aml_read_dosbus(r)
#define WRITE_VREG(r, val) aml_write_dosbus(r, val)

#define BASE_IRQ 32
#define AM_IRQ(reg)   ((reg) + BASE_IRQ)
#define INT_DOS_MAILBOX_0       AM_IRQ(43)
#define INT_DOS_MAILBOX_1       AM_IRQ(44)
#define INT_DOS_MAILBOX_2       AM_IRQ(45)
#define INT_VIU_VSYNC           AM_IRQ(3)

#define INT_DEMUX               AM_IRQ(23)
#define INT_DEMUX_1             AM_IRQ(5)
#define INT_DEMUX_2             AM_IRQ(53)
#define INT_ASYNC_FIFO_FILL     AM_IRQ(18)
#define INT_ASYNC_FIFO_FLUSH    AM_IRQ(19)
#define INT_ASYNC_FIFO2_FILL    AM_IRQ(24)
#define INT_ASYNC_FIFO2_FLUSH   AM_IRQ(25)

#define INT_PARSER              AM_IRQ(32)

#define INT_VDEC INT_DOS_MAILBOX_1
#define INT_VDEC2 INT_DOS_MAILBOX_0

#define WRITE_VREG_BITS(r, val, start, len) \
	codec_set_dosbus_bits(r, val, start, len)
#define SET_VREG_MASK(r, mask) codec_set_dosbus_mask(r, mask)
#define CLEAR_VREG_MASK(r, mask) codec_clear_dosbus_mask(r, mask)

#define READ_HREG(r) aml_read_dosbus((r) | 0x1000)
#define WRITE_HREG(r, val) aml_write_dosbus((r) | 0x1000, val)
#define WRITE_HREG_BITS(r, val, start, len) \
	codec_set_dosbus_bits((r) | 0x1000, val, start, len)
#define SET_HREG_MASK(r, mask) codec_set_dosbus_mask((r) | 0x1000, mask)
#define CLEAR_HREG_MASK(r, mask) codec_clear_dosbus_mask((r) | 0x1000, mask)

/*TODO */
#define READ_SEC_REG(r)
#define WRITE_SEC_REG(r, val)

#define WRITE_MPEG_REG(r, val)      aml_write_cbus(r, val)
#define READ_MPEG_REG(r)		aml_read_cbus(r)
#define WRITE_MPEG_REG_BITS(r, val, start, len) \
	codec_set_cbus_bits(r, val, start, len)
#define CLEAR_MPEG_REG_MASK(r, mask)\
	codec_clear_cbus_mask(r, mask)
#define SET_MPEG_REG_MASK(r, mask)\
	codec_set_cbus_mask(r, mask)

#define WRITE_PARSER_REG(r, val) codec_parsbus_write(r, val)
#define READ_PARSER_REG(r) codec_parsbus_read(r)
#define WRITE_PARSER_REG_BITS(r, val, start, len) \
	codec_set_parsbus_bits(r, val, start, len)
#define CLEAR_PARSER_REG_MASK(r, mask)\
	codec_clear_parsbus_mask(r, mask)
#define SET_PARSER_REG_MASK(r, mask)\
	codec_set_parsbus_mask(r, mask)

#define WRITE_HHI_REG(r, val)      codec_hhibus_write(r, val)
#define READ_HHI_REG(r) codec_hhibus_read(r)
#define WRITE_HHI_REG_BITS(r, val, start, len) \
	codec_set_hhibus_bits(r, val, start, len)

#define WRITE_AIU_REG(r, val) codec_aiubus_write(r, val)
#define READ_AIU_REG(r) codec_aiubus_read(r)
#define WRITE_AIU_REG_BITS(r, val, start, len) \
	codec_set_aiubus_bits(r, val, start, len)
#define CLEAR_AIU_REG_MASK(r, mask)\
	codec_clear_aiubus_mask(r, mask)
#define SET_AIU_REG_MASK(r, mask)\
	codec_set_aiubus_mask(r, mask)

#define WRITE_DEMUX_REG(r, val) codec_demuxbus_write(r, val)
#define READ_DEMUX_REG(r) codec_demuxbus_read(r)
#define WRITE_DEMUX_REG_BITS(r, val, start, len) \
	codec_set_demuxbus_bits(r, val, start, len)
#define CLEAR_DEMUX_REG_MASK(r, mask)\
	codec_clear_demuxbus_mask(r, mask)
#define SET_DEMUX_REG_MASK(r, mask)\
	codec_set_demuxbus_mask(r, mask)

#define WRITE_RESET_REG(r, val) codec_resetbus_write(r, val)
#define READ_RESET_REG(r) codec_resetbus_read(r)
#define WRITE_RESET_REG_BITS(r, val, start, len) \
	codec_set_resetbus_bits(r, val, start, len)
#define CLEAR_RESET_REG_MASK(r, mask)\
	codec_clear_resetbus_mask(r, mask)
#define SET_RESET_REG_MASK(r, mask)\
	codec_set_resetbus_mask(r, mask)

#define WRITE_EFUSE_REG(r, val) codec_efusebus_write(r, val)
#define READ_EFUSE_REG(r) codec_efusebus_read(r)
#define WRITE_EFUSE_REG_BITS(r, val, start, len) \
	codec_set_efusebus_bits(r, val, start, len)
#define CLEAR_EFUSE_REG_MASK(r, mask)\
	codec_clear_efusebus_mask(r, mask)
#define SET_EFUSE_REG_MASK(r, mask)\
	codec_set_efusebus_mask(r, mask)

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
	codec_set_vcbus_bits(r, val, start, len)
#define CLEAR_VCBUS_REG_MASK(r, mask)\
		codec_clear_vcbus_mask(r, mask)
#define SET_VCBUS_REG_MASK(r, mask)\
		codec_set_vcbus_mask(r, mask)
/****************logo relative part *****************************************/
#define ASSIST_MBOX1_CLR_REG VDEC_ASSIST_MBOX1_CLR_REG
#define ASSIST_MBOX1_MASK VDEC_ASSIST_MBOX1_MASK
#define RESET_PSCALE        BIT(4)
#define RESET_IQIDCT        BIT(2)
#define RESET_MC            BIT(3)
//#define MEM_BUFCTRL_MANUAL      BIT(1)
//#define MEM_BUFCTRL_INIT        BIT(0)
#define MEM_LEVEL_CNT_BIT       18
#define MEM_FIFO_CNT_BIT        16
//#define MEM_FILL_ON_LEVEL       BIT(10)
//#define MEM_CTRL_EMPTY_EN       BIT(2)
//#define MEM_CTRL_FILL_EN        BIT(1)
//#define MEM_CTRL_INIT           BIT(0)

#ifndef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
#define VSYNC_WR_MPEG_REG(adr, val) WRITE_VCBUS_REG(adr, val)
#define VSYNC_RD_MPEG_REG(adr) READ_VCBUS_REG(adr)
#define VSYNC_WR_MPEG_REG_BITS(adr, val, start, len) \
	WRITE_VCBUS_REG_BITS(adr, val, start, len)

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
#endif /* VDEC_REG_H */
