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

#ifndef __SYSTEM_AM_SC_H__
#define __SYSTEM_AM_SC_H__

#include <linux/module.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include "acamera_command_api.h"
#include "acamera_firmware_settings.h"
#include "acamera.h"
#include "acamera_fw.h"
#include <linux/kfifo.h>

#define ISP_SCWR_TOP_CTRL 			(0x30 << 2)
#define ISP_SCWR_GCLK_CTRL 		(0x31 << 2)
#define ISP_SCWR_SYNC_DLY 			(0x32 << 2)
#define ISP_SCWR_HOLD_DLY 			(0x33 << 2)
#define ISP_SCWR_SC_CTRL0 			(0x34 << 2)
#define ISP_SCWR_SC_CTRL1 			(0x35 << 2)
#define ISP_SCWR_MIF_CTRL0 		(0x36 << 2)
#define ISP_SCWR_MIF_CTRL1 		(0x37 << 2)
#define ISP_SCWR_MIF_CTRL2 		(0x38 << 2)
#define ISP_SCWR_MIF_CTRL3 		(0x39 << 2)
#define ISP_SCWR_MIF_CTRL4 		(0x3a << 2)
#define ISP_SCWR_MIF_CTRL5 		(0x3b << 2)
#define ISP_SCWR_MIF_CTRL6 		(0x3c << 2)
#define ISP_SCWR_MIF_CTRL7 		(0x3d << 2)
#define ISP_SCWR_MIF_CTRL8 		(0x3e << 2)
#define ISP_SCWR_MIF_CTRL9 		(0x3f << 2)
#define ISP_SCWR_MIF_CTRL10 		(0x40 << 2)
#define ISP_SCWR_MIF_CTRL11 		(0x41 << 2)
#define ISP_SCWR_MIF_CTRL12 		(0x42 << 2)
#define ISP_SCWR_MIF_CTRL13 		(0x43 << 2)
#define ISP_SCWR_TOP_DBG0 			(0x4a << 2)
#define ISP_SCWR_TOP_DBG1 			(0x4b << 2)
#define ISP_SCWR_TOP_DBG2 			(0x4c << 2)
#define ISP_SCWR_TOP_DBG3 			(0x4d << 2)
#define ISP_SCO_FIFO_CTRL 			(0x4e << 2)
#define ISP_SC_DUMMY_DATA 			(0x50 << 2)
#define ISP_SC_LINE_IN_LENGTH 		(0x51 << 2)
#define ISP_SC_PIC_IN_HEIGHT 		(0x52 << 2)
#define ISP_SC_COEF_IDX 			(0x53 << 2)
#define ISP_SC_COEF					(0x54 << 2)
#define ISP_VSC_REGION12_STARTP 	(0x55 << 2)
#define ISP_VSC_REGION34_STARTP 	(0x56 << 2)
#define ISP_VSC_REGION4_ENDP 		(0x57 << 2)
#define ISP_VSC_START_PHASE_STEP 	(0x58 << 2)
#define ISP_VSC_REGION0_PHASE_SLOPE 	(0x59 << 2)
#define ISP_VSC_REGION1_PHASE_SLOPE 	(0x5a << 2)
#define ISP_VSC_REGION3_PHASE_SLOPE 	(0x5b << 2)
#define ISP_VSC_REGION4_PHASE_SLOPE 	(0x5c << 2)
#define ISP_VSC_PHASE_CTRL 				(0x5d << 2)
#define ISP_VSC_INI_PHASE 				(0x5e << 2)
#define ISP_HSC_REGION12_STARTP 		(0x60 << 2)
#define ISP_HSC_REGION34_STARTP 		(0x61 << 2)
#define ISP_HSC_REGION4_ENDP 			(0x62 << 2)
#define ISP_HSC_START_PHASE_STEP 		(0x63 << 2)
#define ISP_HSC_REGION0_PHASE_SLOPE 	(0x64 << 2)
#define ISP_HSC_REGION1_PHASE_SLOPE 	(0x65 << 2)
#define ISP_HSC_REGION3_PHASE_SLOPE 	(0x66 << 2)
#define ISP_HSC_REGION4_PHASE_SLOPE	(0x67 << 2)
#define ISP_HSC_PHASE_CTRL				(0x68 << 2)
#define ISP_SC_MISC						(0x69 << 2)
#define ISP_HSC_PHASE_CTRL1			(0x6a << 2)
#define ISP_HSC_INI_PAT_CTRL			(0x6b << 2)
#define ISP_SC_GCLK_CTRL				(0x6c << 2)
#define ISP_MATRIX_COEF00_01			(0x70 << 2)
#define ISP_MATRIX_COEF02_10			(0x71 << 2)
#define ISP_MATRIX_COEF11_12			(0x72 << 2)
#define ISP_MATRIX_COEF20_21			(0x73 << 2)
#define ISP_MATRIX_COEF22				(0x74 << 2)
#define ISP_MATRIX_COEF30_31			(0x75 << 2)
#define ISP_MATRIX_COEF32_40			(0x76 << 2)
#define ISP_MATRIX_COEF41_42			(0x77 << 2)
#define ISP_MATRIX_CLIP					(0x78 << 2)
#define ISP_MATRIX_OFFSET0_1 			(0x79 << 2)
#define ISP_MATRIX_OFFSET2 			(0x7a << 2)
#define ISP_MATRIX_PRE_OFFSET0_1 		(0x7b << 2)
#define ISP_MATRIX_PRE_OFFSET2 		(0x7c << 2)
#define ISP_MATRIX_EN_CTRL 				(0x7d << 2)

struct am_sc_info {
	uint32_t src_w;
	uint32_t src_h;
	uint32_t out_w;
	uint32_t out_h;
	uint32_t in_fmt;
	uint32_t out_fmt;
	uint32_t csc_mode;
};

struct am_sc {
	struct device_node *of_node;
	struct platform_device *p_dev;
	struct resource reg;
	void __iomem *base_addr;
	int irq;
	spinlock_t sc_lock;
	struct kfifo sc_fifo_in;
	int req_buf_num;
	struct am_sc_info info;
	acamera_context_ptr_t ctx;
	buffer_callback_t callback;
};

extern int am_sc_parse_dt(struct device_node *node);
extern void am_sc_deinit_parse_dt(void);
extern void am_sc_api_dma_buffer(tframe_t * data, unsigned int index);
extern uint32_t am_sc_get_width(void);
extern void am_sc_set_width(uint32_t src_w, uint32_t out_w);
extern uint32_t am_sc_get_height(void);
extern void am_sc_set_height(uint32_t src_h, uint32_t out_h);
extern void am_sc_set_input_format(uint32_t value);
extern void am_sc_set_output_format(uint32_t value);
extern void am_sc_set_buf_num(uint32_t num);
extern int am_sc_set_callback(acamera_context_ptr_t p_ctx, buffer_callback_t ds2_callback);
extern int am_sc_system_init(void);
extern int am_sc_hw_init(void);
extern int am_sc_start(void);
extern int am_sc_reset(void);
extern int am_sc_stop(void);
extern int am_sc_system_deinit(void);
extern int am_sc_hw_deinit(void);
extern void am_sc_set_src_width(uint32_t src_w);
extern void am_sc_set_src_height(uint32_t src_h);


#endif

