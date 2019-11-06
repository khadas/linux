/*
*
* SPDX-License-Identifier: GPL-2.0
*
* Copyright (C) 2018 Amlogic or its affiliates
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; version 2.
* This program is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
* or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
* for more details.
* You should have received a copy of the GNU General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*
*/

#ifndef __SYSTEM_AM_ADAP_H__
#define __SYSTEM_AM_ADAP_H__

#include <linux/module.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/i2c.h>

#define FRONTEND_BASE               0x00004800
#define FRONTEND1_BASE              0x00004C00
#define FTE1_OFFSET                 0x00000400


#define CSI2_CLK_RESET				0x00
#define CSI2_GEN_CTRL0				0x04
#define CSI2_GEN_CTRL1				0x08
#define CSI2_X_START_END_ISP		0x0C
#define CSI2_Y_START_END_ISP		0x10
#define CSI2_X_START_END_MEM		0x14
#define CSI2_Y_START_END_MEM		0x18
#define CSI2_VC_MODE				0x1C
#define CSI2_VC_MODE2_MATCH_MASK_L	0x20
#define CSI2_VC_MODE2_MATCH_MASK_H	0x24
#define CSI2_VC_MODE2_MATCH_TO_VC_L	0x28
#define CSI2_VC_MODE2_MATCH_TO_VC_H	0x2C
#define CSI2_VC_MODE2_MATCH_TO_IGNORE_L	0x30
#define CSI2_VC_MODE2_MATCH_TO_IGNORE_H	0x34
#define CSI2_DDR_START_PIX			0x38
#define CSI2_DDR_START_PIX_ALT		0x3C
#define CSI2_DDR_STRIDE_PIX			0x40
#define CSI2_DDR_START_OTHER		0x44
#define CSI2_DDR_START_OTHER_ALT	0x48
#define CSI2_DDR_MAX_BYTES_OTHER	0x4C
#define CSI2_INTERRUPT_CTRL_STAT	0x50

#define CSI2_GEN_STAT0				0x80
#define CSI2_ERR_STAT0				0x84
#define CSI2_PIC_SIZE_STAT			0x88
#define CSI2_DDR_WPTR_STAT_PIX		0x8C
#define CSI2_DDR_WPTR_STAT_OTHER	0x90
#define CSI2_STAT_MEM_0				0x94
#define CSI2_STAT_MEM_1				0x98

#define CSI2_STAT_GEN_SHORT_08		0xA0
#define CSI2_STAT_GEN_SHORT_09		0xA4
#define CSI2_STAT_GEN_SHORT_0A		0xA8
#define CSI2_STAT_GEN_SHORT_0B		0xAC
#define CSI2_STAT_GEN_SHORT_0C		0xB0
#define CSI2_STAT_GEN_SHORT_0D		0xB4
#define CSI2_STAT_GEN_SHORT_0E		0xB8
#define CSI2_STAT_GEN_SHORT_0F		0xBC

#define RD_BASE                     0x00005000
#define MIPI_ADAPT_DDR_RD0_CNTL0    0x00
#define MIPI_ADAPT_DDR_RD0_CNTL1    0x04
#define MIPI_ADAPT_DDR_RD0_CNTL2    0x08
#define MIPI_ADAPT_DDR_RD0_CNTL3    0x0C
#define MIPI_ADAPT_DDR_RD1_CNTL0    0x40
#define MIPI_ADAPT_DDR_RD1_CNTL1    0x44
#define MIPI_ADAPT_DDR_RD1_CNTL2    0x48
#define MIPI_ADAPT_DDR_RD1_CNTL3    0x4C

#define PIXEL_BASE                  0x00005000
#define MIPI_ADAPT_PIXEL0_CNTL0     0x80
#define MIPI_ADAPT_PIXEL0_CNTL1     0x84
#define MIPI_ADAPT_PIXEL1_CNTL0     0x88
#define MIPI_ADAPT_PIXEL1_CNTL1     0x8C


#define ALIGN_BASE                  0x00005000
#define MIPI_ADAPT_ALIG_CNTL0       0xC0
#define MIPI_ADAPT_ALIG_CNTL1       0xC4
#define MIPI_ADAPT_ALIG_CNTL2       0xC8
#define MIPI_ADAPT_ALIG_CNTL6       0xD8
#define MIPI_ADAPT_ALIG_CNTL7       0xDC
#define MIPI_ADAPT_ALIG_CNTL8       0xE0
#define MIPI_OTHER_CNTL0           0x100
#define MIPI_ADAPT_IRQ_MASK0       0x180
#define MIPI_ADAPT_IRQ_PENDING0    0x184

#define MISC_BASE                  0x00005000


typedef enum {
	FRONTEND_IO,
	RD_IO,
	PIXEL_IO,
	ALIGN_IO,
	MISC_IO,
} adap_io_type_t;

typedef enum {
	DDR_MODE,
	DIR_MODE,
	DOL_MODE,
} adap_mode_t;

typedef enum {
	PATH0,
	PATH1,
} adap_path_t;

typedef struct {
	uint32_t width;
	uint32_t height;
} adap_img_t;

typedef enum {
	AM_RAW6 = 1,
	AM_RAW7,
	AM_RAW8,
	AM_RAW10,
	AM_RAW12,
	AM_RAW14,
} img_fmt_t;

typedef enum {
	DOL_NON = 0,
	DOL_VC,
	DOL_LINEINFO,
} dol_type_t;

typedef enum {
	FTE_DONE = 0,
	FTE0_DONE,
	FTE1_DONE,
} dol_state_t;


typedef struct exp_offset {
	int long_offset;
	int short_offset;
} exp_offset_t;

struct am_adap {
	struct device_node *of_node;
	struct platform_device *p_dev;
	struct resource reg;
	void __iomem *base_addr;
	int f_end_irq;
	int rd_irq;
	unsigned int adap_buf_size;
};

struct am_adap_info {
	adap_path_t path;
	adap_mode_t mode;
	adap_img_t img;
	img_fmt_t fmt;
	dol_type_t type;
	exp_offset_t offset;
};


int am_adap_parse_dt(struct device_node *node);
void am_adap_deinit_parse_dt(void);
int am_adap_init(void);
int am_adap_start(int idx);
int am_adap_reset(void);
int am_adap_deinit(void);
void am_adap_set_info(struct am_adap_info *info);
int get_fte1_flag(void);
int am_adap_get_depth(void);

#endif

