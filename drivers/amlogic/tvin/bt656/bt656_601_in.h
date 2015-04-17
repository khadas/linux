/*
 * BT656IN Driver
 *
 * Author: Xintan Chen <lin.xu@amlogic.com>
 *
 * Copyright (C) 2010 Amlogic Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */


#ifndef __BT656_601_INPUT_H
#define __BT656_601_INPUT_H
#include <linux/cdev.h>
#include <linux/amlogic/tvin/tvin_v4l2.h>
/* #include <mach/pinmux.h> */
#include "../tvin_frontend.h"
#include "../tvin_global.h"
#include <linux/amlogic/cpu_version.h>


/* registers */
#define BT_CTRL 0x2f40	/* /../ucode/register.h:11418 */
#define P_BT_CTRL		VCBUS_REG_ADDR(BT_CTRL)
#define BT_VBISTART 0x2f41	/* /../ucode/register.h:11439 */
#define P_BT_VBISTART		VCBUS_REG_ADDR(BT_VBISTART)
#define BT_VBIEND 0x2f42	/* /../ucode/register.h:11440 */
#define P_BT_VBIEND		VCBUS_REG_ADDR(BT_VBIEND)
#define BT_FIELDSADR 0x2f43	/* /../ucode/register.h:11441 */
#define P_BT_FIELDSADR		VCBUS_REG_ADDR(BT_FIELDSADR)
#define BT_LINECTRL 0x2f44	/* /../ucode/register.h:11442 */
#define P_BT_LINECTRL		VCBUS_REG_ADDR(BT_LINECTRL)
#define BT_VIDEOSTART 0x2f45	/* /../ucode/register.h:11443 */
#define P_BT_VIDEOSTART		VCBUS_REG_ADDR(BT_VIDEOSTART)
#define BT_VIDEOEND 0x2f46	/* /../ucode/register.h:11444 */
#define P_BT_VIDEOEND		VCBUS_REG_ADDR(BT_VIDEOEND)
#define BT_SLICELINE0 0x2f47	/* /../ucode/register.h:11445 */
#define P_BT_SLICELINE0		VCBUS_REG_ADDR(BT_SLICELINE0)
#define BT_SLICELINE1 0x2f48	/* /../ucode/register.h:11446 */
#define P_BT_SLICELINE1		VCBUS_REG_ADDR(BT_SLICELINE1)
#define BT_PORT_CTRL 0x2f49	/* /../ucode/register.h:11447 */
#define P_BT_PORT_CTRL		VCBUS_REG_ADDR(BT_PORT_CTRL)
#define BT_SWAP_CTRL 0x2f4a	/* /../ucode/register.h:11461 */
#define P_BT_SWAP_CTRL		VCBUS_REG_ADDR(BT_SWAP_CTRL)
#define BT_ANCISADR 0x2f4b	/* /../ucode/register.h:11462 */
#define P_BT_ANCISADR		VCBUS_REG_ADDR(BT_ANCISADR)
#define BT_ANCIEADR 0x2f4c	/* /../ucode/register.h:11463 */
#define P_BT_ANCIEADR		VCBUS_REG_ADDR(BT_ANCIEADR)
#define BT_AFIFO_CTRL 0x2f4d	/* /../ucode/register.h:11464 */
#define P_BT_AFIFO_CTRL		VCBUS_REG_ADDR(BT_AFIFO_CTRL)
#define BT_601_CTRL0 0x2f4e	/* /../ucode/register.h:11465 */
#define P_BT_601_CTRL0		VCBUS_REG_ADDR(BT_601_CTRL0)
#define BT_601_CTRL1 0x2f4f	/* /../ucode/register.h:11466 */
#define P_BT_601_CTRL1		VCBUS_REG_ADDR(BT_601_CTRL1)
#define BT_601_CTRL2 0x2f50	/* /../ucode/register.h:11467 */
#define P_BT_601_CTRL2		VCBUS_REG_ADDR(BT_601_CTRL2)
#define BT_601_CTRL3 0x2f51	/* /../ucode/register.h:11468 */
#define P_BT_601_CTRL3		VCBUS_REG_ADDR(BT_601_CTRL3)
#define BT_FIELD_LUMA 0x2f52	/* /../ucode/register.h:11469 */
#define P_BT_FIELD_LUMA		VCBUS_REG_ADDR(BT_FIELD_LUMA)
#define BT_RAW_CTRL 0x2f53	/* /../ucode/register.h:11470 */
#define P_BT_RAW_CTRL		VCBUS_REG_ADDR(BT_RAW_CTRL)
#define BT_STATUS 0x2f54	/* /../ucode/register.h:11471 */
#define P_BT_STATUS		VCBUS_REG_ADDR(BT_STATUS)
#define BT_INT_CTRL 0x2f55	/* /../ucode/register.h:11472 */
#define P_BT_INT_CTRL		VCBUS_REG_ADDR(BT_INT_CTRL)
#define BT_ANCI_STATUS 0x2f56	/* /../ucode/register.h:11473 */
#define P_BT_ANCI_STATUS		VCBUS_REG_ADDR(BT_ANCI_STATUS)
#define BT_VLINE_STATUS 0x2f57	/* /../ucode/register.h:11474 */
#define P_BT_VLINE_STATUS		VCBUS_REG_ADDR(BT_VLINE_STATUS)
#define BT_AFIFO_PTR 0x2f58	/* /../ucode/register.h:11475 */
#define P_BT_AFIFO_PTR		VCBUS_REG_ADDR(BT_AFIFO_PTR)
#define BT_JPEGBYTENUM 0x2f59	/* /../ucode/register.h:11476 */
#define P_BT_JPEGBYTENUM		VCBUS_REG_ADDR(BT_JPEGBYTENUM)
#define BT_ERR_CNT 0x2f5a	/* /../ucode/register.h:11477 */
#define P_BT_ERR_CNT		VCBUS_REG_ADDR(BT_ERR_CNT)
#define BT_JPEG_STATUS0 0x2f5b	/* /../ucode/register.h:11478 */
#define P_BT_JPEG_STATUS0		VCBUS_REG_ADDR(BT_JPEG_STATUS0)
#define BT_JPEG_STATUS1 0x2f5c	/* /../ucode/register.h:11479 */
#define P_BT_JPEG_STATUS1		VCBUS_REG_ADDR(BT_JPEG_STATUS1)
#define BT_LCNT_STATUS 0x2f5d	/* /../ucode/register.h:11480 */
#define P_BT_LCNT_STATUS		VCBUS_REG_ADDR(BT_LCNT_STATUS)
#define BT_PCNT_STATUS 0x2f5e	/* /../ucode/register.h:11481 */
#define P_BT_PCNT_STATUS		VCBUS_REG_ADDR(BT_PCNT_STATUS)
#define BT656_ADDR_END 0x2f5f	/* /../ucode/register.h:11482 */
#define P_BT656_ADDR_END		VCBUS_REG_ADDR(BT656_ADDR_END)


#define VDIN_WR_V_START_END			0x1222
#define P_VDIN_WR_V_START_END		VCBUS_REG_ADDR(VDIN_WR_V_START_END)
#define RESET1_REGISTER                            0x1102


/* bit defines */
/*BT656 MACRO */
/* #define BT_CTRL 0x2240	///../ucode/register.h */
/* Sync fifo soft  reset_n at system clock domain.
 * Level reset. 0 = reset. 1 : normal mode. */
#define BT_SYSCLOCK_RESET    30

/* Sync fifo soft reset_n at bt656 clock domain.
 * Level reset.  0 = reset.  1 : normal mode. */
#define BT_656CLOCK_RESET    29

/* 25:26 VDIN VS selection.   00 :  SOF.  01: EOF.
 *  10: vbi start point.  11 : vbi end point. */
/* #define BT_VSYNC_SEL              25 */

/* 24:23 VDIN HS selection.  00 : EAV.  01: SAV.
 * 10:  EOL.  11: SOL */
/* #define BT_HSYNC_SEL              23 */


#define BT_CAMERA_MODE        22      /* Camera_mode */
/* 1: enable bt656 clock. 0: disable bt656 clock. */
#define BT_CLOCK_ENABLE        7

/* #define BT_PORT_CTRL 0x2249	///../ucode/register.h */
 /* 1: use  vsync  as the VBI start point. 0: use the regular vref. */
/* #define BT_VSYNC_MODE      23 */
/* 1: use hsync as the active video start point.  0. Use regular sav and eav. */
/* #define BT_HSYNC_MODE      22 */


#define BT_SOFT_RESET           31	/* Soft reset */
/* #define BT_JPEG_START           30 */
/* #define BT_JPEG_IGNORE_BYTES    18	//20:18 */
/* #define BT_JPEG_IGNORE_LAST     17 */
#define BT_UPDATE_ST_SEL        16
#define BT_COLOR_REPEAT         15
/* #define BT_VIDEO_MODE           13	// 14:13 */
#define BT_AUTO_FMT             12
#define BT_PROG_MODE            11
/* #define BT_JPEG_MODE            10 */
/* 1 : xclk27 is input.     0 : xclk27 is output. */
#define BT_XCLK27_EN_BIT        9

#define BT_FID_EN_BIT           8	/* 1 : enable use FID port. */
/* 1 : external xclk27      0 : internal clk27. */
#define BT_CLK27_SEL_BIT        7
/* 1 : no inverted          0 : inverted. */
/* #define BT_CLK27_PHASE_BIT      6 */
/* 1 : auto cover error by hardware. */
/* #define BT_ACE_MODE_BIT         5 */
/* 1 : no ancillay flag     0 : with ancillay flag. */
#define BT_SLICE_MODE_BIT       4
/* 1 : ntsc                 0 : pal. */
#define BT_FMT_MODE_BIT         3
/* 1 : from bit stream.     0 : from ports. */
#define BT_REF_MODE_BIT         2
/* 1 : BT656 model          0 : SAA7118 mode. */
#define BT_MODE_BIT             1

#define BT_EN_BIT               0	/* 1 : enable. */
#define BT_VSYNC_PHASE      0
#define BT_HSYNC_PHASE      1
/* #define BT_VSYNC_PULSE      2 */
/* #define BT_HSYNC_PULSE      3 */
/* #define BT_FID_PHASE        4 */
#define BT_FID_HSVS         5
#define BT_IDQ_EN           6
#define BT_IDQ_PHASE        7
#define BT_D8B              8
/* #define BT_10BTO8B          9 */
#define BT_FID_DELAY       10	/* 12:10 */
#define BT_VSYNC_DELAY     13	/*  */
#define BT_HSYNC_DELAY     16


static inline uint32_t rd(uint32_t reg)
{
	return (uint32_t)aml_read_vcbus(reg);
}

static inline void wr(uint32_t reg,
				 const uint32_t val)
{
	aml_write_vcbus(reg, val);
}

static inline void wr_bits(uint32_t reg,
				    const uint32_t value,
				    const uint32_t start,
				    const uint32_t len)
{
	aml_write_vcbus(reg, ((aml_read_vcbus(reg) &
			     ~(((1L << (len)) - 1) << (start))) |
			    (((value) & ((1L << (len)) - 1)) << (start))));
}

static inline uint32_t rd_bits(uint32_t reg,
				    const uint32_t start,
				    const uint32_t len)
{
	uint32_t val;

	val = ((aml_read_vcbus(reg) >> (start)) & ((1L << (len)) - 1));

	return val;
}

enum am656_status_e {
	TVIN_AM656_STOP,
	TVIN_AM656_RUNING,
};
struct am656in_dev_s {
	int                     index;
	dev_t                   devt;
	struct cdev             cdev;
	struct device          *dev;
	unsigned int            overflow_cnt;
	unsigned int            skip_vdin_frame_count;
	enum am656_status_e     dec_status;
	struct vdin_parm_s      para;
	struct tvin_frontend_s  frontend;
};

#endif

