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

#ifndef __SYSTEM_AM_H__
#define __SYSTEM_AM_H__

#include <linux/module.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include "acamera_command_api.h"
#include "acamera_firmware_settings.h"
#include "acamera.h"
#include "acamera_fw.h"
#include <linux/kfifo.h>
#include <linux/interrupt.h>

#define ISP_SCWR_TOP_CTRL 				(0x30 << 2)
#define ISP_SCWR_GCLK_CTRL 				(0x31 << 2)
#define ISP_SCWR_SYNC_DLY 				(0x32 << 2)
#define ISP_SCWR_HOLD_DLY 				(0x33 << 2)
#define ISP_SCWR_SC_CTRL0 				(0x34 << 2)
#define ISP_SCWR_SC_CTRL1 				(0x35 << 2)
#define ISP_SCWR_MIF_CTRL0 				(0x36 << 2)
#define ISP_SCWR_MIF_CTRL1 				(0x37 << 2)
#define ISP_SCWR_MIF_CTRL2 				(0x38 << 2)
#define ISP_SCWR_MIF_CTRL3 				(0x39 << 2)
#define ISP_SCWR_MIF_CTRL4 				(0x3a << 2)
#define ISP_SCWR_MIF_CTRL5 				(0x3b << 2)
#define ISP_SCWR_MIF_CTRL6 				(0x3c << 2)
#define ISP_SCWR_MIF_CTRL7 				(0x3d << 2)
#define ISP_SCWR_MIF_CTRL8 				(0x3e << 2)
#define ISP_SCWR_MIF_CTRL9 				(0x3f << 2)
#define ISP_SCWR_MIF_CTRL10 			(0x40 << 2)
#define ISP_SCWR_MIF_CTRL11 			(0x41 << 2)
#define ISP_SCWR_MIF_CTRL12 			(0x42 << 2)
#define ISP_SCWR_MIF_CTRL13 			(0x43 << 2)
#define ISP_SCWR_TOP_DBG0 				(0x4a << 2)
#define ISP_SCWR_TOP_DBG1 				(0x4b << 2)
#define ISP_SCWR_TOP_DBG2 				(0x4c << 2)
#define ISP_SCWR_TOP_DBG3 				(0x4d << 2)
#define ISP_SCO_FIFO_CTRL 				(0x4e << 2)
#define ISP_SC_DUMMY_DATA 				(0x50 << 2)
#define ISP_SC_LINE_IN_LENGTH 			(0x51 << 2)
#define ISP_SC_PIC_IN_HEIGHT 			(0x52 << 2)
#define ISP_SC_COEF_IDX 				(0x53 << 2)
#define ISP_SC_COEF						(0x54 << 2)
#define ISP_VSC_REGION12_STARTP 		(0x55 << 2)
#define ISP_VSC_REGION34_STARTP 		(0x56 << 2)
#define ISP_VSC_REGION4_ENDP 			(0x57 << 2)
#define ISP_VSC_START_PHASE_STEP 		(0x58 << 2)
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
#define ISP_HSC_REGION4_PHASE_SLOPE		(0x67 << 2)
#define ISP_HSC_PHASE_CTRL				(0x68 << 2)
#define ISP_SC_MISC						(0x69 << 2)
#define ISP_HSC_PHASE_CTRL1				(0x6a << 2)
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
#define ISP_MATRIX_OFFSET2 				(0x7a << 2)
#define ISP_MATRIX_PRE_OFFSET0_1 		(0x7b << 2)
#define ISP_MATRIX_PRE_OFFSET2 			(0x7c << 2)
#define ISP_MATRIX_EN_CTRL 				(0x7d << 2)

//T7 add
#define ISP_SCWR_MIF_CTRL14     	 (0x44 << 2)
#define ISP_SC_TOP_CTRL			 	 (0x4f << 2)
#define ISP_SC_HOLD_LINE		  	 (0x6d << 2)
#define ISP_SCWR_DMA_CTRL        	 (0x90 << 2)
#define ISP_SCWR_CLIP_CTRL0          (0x91 << 2)
#define ISP_SCWR_CLIP_CTRL1       	 (0x92 << 2)
#define ISP_SCWR_CLIP_CTRL2          (0x93 << 2)
#define ISP_SCWR_CLIP_CTRL3          (0x94 << 2)
#define ISP_SCWR_TOP_GEN             (0x95 << 2)
#define ISP_SCWR_SYNC_DELAY          (0x96 << 2)
#define ISP_SCWR_TOP_DBG4            (0x97 << 2)
#define ISP_SCWR_TOP_DBG5            (0x98 << 2)
#define ISP_SCWR_TOP_DBG6            (0x99 << 2)
#define ISP_SCWR_TOP_DBG7            (0x9a << 2)
#define ISP_PREHSC_COEF              (0x9d << 2)
#define ISP_PREVSC_COEF              (0x9e << 2)
#define ISP_PRE_SCALE_CTRL           (0x9f << 2)

typedef enum {
	F2V_IT2IT = 0,
	F2V_IB2IB,
	F2V_IT2IB,
	F2V_IB2IT,
	F2V_P2IT,
	F2V_P2IB,
	F2V_IT2P,
	F2V_IB2P,
	F2V_P2P,
	F2V_TYPE_MAX
} f2v_vphase_type_t;   /* frame to video conversion type */

#define CLIP_MODE                    0x01
#define SC_MODE                      0x02

#define ZOOM_BITS       20
#define PHASE_BITS      16

#define CAM_CTX_NUM       FIRMWARE_CONTEXT_NUMBER
#define START_DELAY_THR   2
#define FRAME_DELAY_QUEUE 3

#define ENABLE_SC_BOTTOM_HALF_TASKLET

typedef struct {
	u8 rcv_num; //0~15
	u8 rpt_num; // 0~3
	u16 phase;
	//s8 repeat_skip_chroma;
	//u8 phase_chroma;
} f2v_vphase_t;

typedef struct ISP_MIF_TYPE {
	int reg_rev_x;
	int reg_rev_y;
	int reg_little_endian;
	int reg_bit10_mode;
	int reg_enable_3ch;
	int reg_only_1ch;
	int reg_words_lim;
	int reg_burst_lim;
	int reg_rgb_mode;
	int reg_hconv_mode;
	int reg_vconv_mode;
	u16 reg_hsizem1;
	u16 reg_vsizem1;
	u16 reg_start_x;
	u16 reg_start_y;
	u16 reg_end_x;
	u16 reg_end_y;
	int reg_pingpong_en;
	u16 reg_canvas_strb_luma;
	u16 reg_canvas_strb_chroma;
	u16 reg_canvas_strb_r;
	u32 reg_canvas_baddr_luma;
	u32 reg_canvas_baddr_chroma;
	u32 reg_canvas_baddr_r;
	u32 reg_canvas_baddr_luma_other;
	u32 reg_canvas_baddr_chroma_other;
	u32 reg_canvas_baddr_r_other;
} ISP_MIF_t;

struct am_sc_info {
    uint32_t clip_sc_mode;
	uint32_t startx;
	uint32_t starty;
	uint32_t c_width;
	uint32_t c_height;
	uint32_t src_w;
	uint32_t src_h;
	uint32_t out_w;
	uint32_t out_h;
	uint32_t in_fmt;
	uint32_t out_fmt;
	uint32_t csc_mode;
	uint32_t c_fps;
	uint32_t t_fps;
};

#ifdef ENABLE_SC_BOTTOM_HALF_TASKLET
// tasklet structure
struct sc_tasklet_t {
	struct tasklet_struct tasklet_obj;
	unsigned int ctx_id;
};
#endif

struct am_sc_multi_frame {
	int cam_id[FRAME_DELAY_QUEUE];
	tframe_t* pre_frame[FRAME_DELAY_QUEUE];
};

struct am_sc {
	struct device_node *of_node;
	struct platform_device *p_dev;
	struct resource reg;
	void __iomem *base_addr;
	ISP_MIF_t isp_frame;
	int irq;
	spinlock_t sc_lock;
	u32 working;
	u32 user;
	u32 refresh;
	u32 dcam_mode;

	struct sc_tasklet_t sc_tasklet;

	bool stop_flag;
	u32 start_delay_cnt;
	u32 last_end_frame;
	u32 no_ready_th;

	u32 cam_id_last;
	u32 cam_id_current;
	u32 cam_id_next;
	u32 cam_id_next_next;

	struct am_sc_multi_frame multi_camera;

	int port;
	struct apb_dma_cmd *ping_cmd;
	struct apb_dma_cmd *pong_cmd;
	struct apb_dma_cmd *ping_cmd_next_ptr;
	struct apb_dma_cmd *pong_cmd_next_ptr;
	int dma_num;
	int dma_ready_flag;
};

struct am_sc_context {
    struct kfifo sc_fifo_out;
    struct kfifo sc_fifo_in;
    int req_buf_num;
    struct am_sc_info info;
    acamera_context_ptr_t ctx;
    buffer_callback_t callback;

    int frame_id;

    tframe_t* temp_buf;

    u8 ch_mode; // 0: normal, 1: force 1 ch, 3: force 3 ch;
    u8 swap_uv;
};

static const int isp_filt_coef0[] =   //bicubic
{
	0x00800000,
	0x007f0100,
	0xff7f0200,
	0xfe7f0300,
	0xfd7e0500,
	0xfc7e0600,
	0xfb7d0800,
	0xfb7c0900,
	0xfa7b0b00,
	0xfa7a0dff,
	0xf9790fff,
	0xf97711ff,
	0xf87613ff,
	0xf87416fe,
	0xf87218fe,
	0xf8701afe,
	0xf76f1dfd,
	0xf76d1ffd,
	0xf76b21fd,
	0xf76824fd,
	0xf76627fc,
	0xf76429fc,
	0xf7612cfc,
	0xf75f2ffb,
	0xf75d31fb,
	0xf75a34fb,
	0xf75837fa,
	0xf7553afa,
	0xf8523cfa,
	0xf8503ff9,
	0xf84d42f9,
	0xf84a45f9,
	0xf84848f8
};

#if 0
const int isp_filt_coef1[] =  // 2point bilinear
{
	0x00800000,
	0x007e0200,
	0x007c0400,
	0x007a0600,
	0x00780800,
	0x00760a00,
	0x00740c00,
	0x00720e00,
	0x00701000,
	0x006e1200,
	0x006c1400,
	0x006a1600,
	0x00681800,
	0x00661a00,
	0x00641c00,
	0x00621e00,
	0x00602000,
	0x005e2200,
	0x005c2400,
	0x005a2600,
	0x00582800,
	0x00562a00,
	0x00542c00,
	0x00522e00,
	0x00503000,
	0x004e3200,
	0x004c3400,
	0x004a3600,
	0x00483800,
	0x00463a00,
	0x00443c00,
	0x00423e00,
	0x00404000
};
#endif

static int isp_filt_coef2[] =  // 2point bilinear, bank_length == 2
{
	0x80000000,
	0x7e020000,
	0x7c040000,
	0x7a060000,
	0x78080000,
	0x760a0000,
	0x740c0000,
	0x720e0000,
	0x70100000,
	0x6e120000,
	0x6c140000,
	0x6a160000,
	0x68180000,
	0x661a0000,
	0x641c0000,
	0x621e0000,
	0x60200000,
	0x5e220000,
	0x5c240000,
	0x5a260000,
	0x58280000,
	0x562a0000,
	0x542c0000,
	0x522e0000,
	0x50300000,
	0x4e320000,
	0x4c340000,
	0x4a360000,
	0x48380000,
	0x463a0000,
	0x443c0000,
	0x423e0000,
	0x40400000
};

static const u8 f2v_420_in_pos_luma[F2V_TYPE_MAX] = {0, 2, 0, 2, 0, 0, 0, 2, 0};
//static const u8 f2v_420_in_pos_chroma[F2V_TYPE_MAX] = {1, 5, 1, 5, 2, 2, 1, 5, 2};
static const u8 f2v_420_out_pos[F2V_TYPE_MAX] = {0, 2, 2, 0, 0, 2, 0, 0, 0};

static int rgb2yuvpre[3] = {0, 0, 0};
//static int rgb2yuvpos[3] = {64, 512, 512}; //BT601_fullrange
static int rgb2yuvpos[3] = {0, 512, 512}; //BT601_limit
static int rgb2yuvpos_invert[3] = {512,  512, 64};
static int yuv2rgbpre[3] = {-64, -512, -512};
static int yuv2rgbpos[3] = {0, 0, 0};
//static int rgb2ycbcr[15] = {230,594,52,-125,-323,448,448,-412,-36,0,0,0,0,0,0};
//static int rgb2ycbcr[15] = {263,516,100,-152,-298,450,450,-377,-73,0,0,0,0,0,0}; //BT601_fullrange
static int rgb2ycbcr[15] = {306,601,117,-173,-339,512,512,-429,-83,0,0,0,0,0,0}; //BT601_limit
static int rgb2ycbcr_invert[15] = {-125, -323, 448, 448,-412,-36, 230, 594, 52, 0,0,0,0,0,0};
static int ycbcr2rgb[15] = {1197,0,1726,1197,-193,-669,1197,2202,0,0,0,0,0,0,0};
static ISP_MIF_t isp_frame = {
	0, // int  reg_rev_x
	0, // int  reg_rev_y
	1, // int  reg_little_endian
	0, // int  reg_bit10_mode
	0, // int  reg_enable_3ch
	0, // int  reg_only_1ch
	4, // int  reg_words_lim
	3, // int  reg_burst_lim
	1, // int  reg_rgb_mode
	2, // int  reg_hconv_mode
	2, // int  reg_vconv_mode
	1279, // int  reg_hsizem1
	719, // int  reg_vsizem1
	0, // int  reg_start_x
	0, // int  reg_start_y
	1279, // int  reg_end_x
	719, // int  reg_end_y
	0, // pingpong en
	0x780, // int16_t reg_canvas_strb_luma
	0x780, // int16_t reg_canvas_strb_chroma
	0x780, // int16_t reg_canvas_strb_r
	0x4000000, // int32_t reg_canvas_baddr_luma
	0x5000000, // int32_t reg_canvas_baddr_chroma
	0x6000000, // int32_t reg_canvas_baddr_r
	0x7000000, // int32_t reg_canvas_baddr_luma_other
	0x8000000, // int32_t reg_canvas_baddr_chroma_other
	0x9000000 // int32_t reg_canvas_baddr_r_other
};

#endif

