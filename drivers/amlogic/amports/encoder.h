/*
 * drivers/amlogic/amports/encoder.h
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

#ifndef __H264_H__
#define __H264_H__

#include <linux/mutex.h>
#include <linux/semaphore.h>
#include <linux/list.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/wait.h>
#include <linux/slab.h>
#ifdef CONFIG_AM_GE2D
#include <linux/amlogic/ge2d/ge2d.h>
#endif

#define AMVENC_DEVINFO_M8 "AML-M8"
#define AMVENC_DEVINFO_G9 "AML-G9"
#define AMVENC_DEVINFO_GXBB "AML-GXBB"
#define AMVENC_DEVINFO_GXTVBB "AML-GXTVBB"
#define AMVENC_DEVINFO_GXL "AML-GXL"

#define HCODEC_IRQ_MBOX_CLR HCODEC_ASSIST_MBOX2_CLR_REG
#define HCODEC_IRQ_MBOX_MASK HCODEC_ASSIST_MBOX2_MASK

/* M8: 2550/10 = 255M GX: 2000/10 = 200M */
#define HDEC_L0()   WRITE_HHI_REG(HHI_VDEC_CLK_CNTL, \
			 (2 << 25) | (1 << 16) | (1 << 24) | \
			 (0xffff & READ_HHI_REG(HHI_VDEC_CLK_CNTL)))
/* M8: 2550/8 = 319M GX: 2000/8 = 250M */
#define HDEC_L1()   WRITE_HHI_REG(HHI_VDEC_CLK_CNTL, \
			 (0 << 25) | (1 << 16) | (1 << 24) | \
			 (0xffff & READ_HHI_REG(HHI_VDEC_CLK_CNTL)))
/* M8: 2550/7 = 364M GX: 2000/7 = 285M */
#define HDEC_L2()   WRITE_HHI_REG(HHI_VDEC_CLK_CNTL, \
			 (3 << 25) | (0 << 16) | (1 << 24) | \
			 (0xffff & READ_HHI_REG(HHI_VDEC_CLK_CNTL)))
/* M8: 2550/6 = 425M GX: 2000/6 = 333M */
#define HDEC_L3()   WRITE_HHI_REG(HHI_VDEC_CLK_CNTL, \
			 (1 << 25) | (1 << 16) | (1 << 24) | \
			 (0xffff & READ_HHI_REG(HHI_VDEC_CLK_CNTL)))
/* M8: 2550/5 = 510M GX: 2000/5 = 400M */
#define HDEC_L4()   WRITE_HHI_REG(HHI_VDEC_CLK_CNTL, \
			 (2 << 25) | (0 << 16) | (1 << 24) | \
			 (0xffff & READ_HHI_REG(HHI_VDEC_CLK_CNTL)))
/* M8: 2550/4 = 638M GX: 2000/4 = 500M */
#define HDEC_L5()   WRITE_HHI_REG(HHI_VDEC_CLK_CNTL, \
			 (0 << 25) | (0 << 16) | (1 << 24) | \
			 (0xffff & READ_HHI_REG(HHI_VDEC_CLK_CNTL)))
/* M8: 2550/3 = 850M GX: 2000/3 = 667M */
#define HDEC_L6()   WRITE_HHI_REG(HHI_VDEC_CLK_CNTL, \
			 (1 << 25) | (0 << 16) | (1 << 24) | \
			 (0xffff & READ_HHI_REG(HHI_VDEC_CLK_CNTL)))

#define hvdec_clock_enable(level) \
	do { \
		if (level == 0)  \
			HDEC_L0(); \
		else if (level == 1)  \
			HDEC_L1(); \
		else if (level == 2)  \
			HDEC_L2(); \
		else if (level == 3)  \
			HDEC_L3(); \
		else if (level == 4)  \
			HDEC_L4(); \
		else if (level == 5)  \
			HDEC_L5(); \
		else if (level == 6)  \
			HDEC_L6(); \
		WRITE_VREG_BITS(DOS_GCLK_EN0, 0x7fff, 12, 15); \
	} while (0)

#define hvdec_clock_disable() \
	do { \
		WRITE_VREG_BITS(DOS_GCLK_EN0, 0, 12, 15); \
		WRITE_HHI_REG_BITS(HHI_VDEC_CLK_CNTL,  0, 24, 1); \
	} while (0)

#if 0/* MESON_CPU_TYPE == MESON_CPU_TYPE_MESON8 */
#define USE_VDEC2
#endif

#ifdef USE_VDEC2
#define VDEC2_DEF_BUF_START_ADDR            0x01000000
#endif

#define LOG_ALL 0
#define LOG_INFO 1
#define LOG_DEBUG 2
#define LOG_ERROR 3

#define enc_pr(level, x...) \
	do { \
		if (level >= encode_print_level) \
			printk(x); \
	} while (0)

#define AMVENC_AVC_IOC_MAGIC  'E'

#define AMVENC_AVC_IOC_GET_DEVINFO	_IOW(AMVENC_AVC_IOC_MAGIC, 0xf0, u32)
#define AMVENC_AVC_IOC_MAX_INSTANCE	_IOW(AMVENC_AVC_IOC_MAGIC, 0xf1, u32)

#define AMVENC_AVC_IOC_GET_ADDR _IOW(AMVENC_AVC_IOC_MAGIC, 0x00, u32)
#define AMVENC_AVC_IOC_INPUT_UPDATE	_IOW(AMVENC_AVC_IOC_MAGIC, 0x01, u32)
#define AMVENC_AVC_IOC_NEW_CMD _IOW(AMVENC_AVC_IOC_MAGIC, 0x02, u32)
#define AMVENC_AVC_IOC_GET_STAGE _IOW(AMVENC_AVC_IOC_MAGIC, 0x03, u32)
#define AMVENC_AVC_IOC_GET_OUTPUT_SIZE _IOW(AMVENC_AVC_IOC_MAGIC, 0x04, u32)
#define AMVENC_AVC_IOC_CONFIG_INIT _IOW(AMVENC_AVC_IOC_MAGIC, 0x05, u32)
#define AMVENC_AVC_IOC_FLUSH_CACHE _IOW(AMVENC_AVC_IOC_MAGIC, 0x06, u32)
#define AMVENC_AVC_IOC_FLUSH_DMA _IOW(AMVENC_AVC_IOC_MAGIC, 0x07, u32)
#define AMVENC_AVC_IOC_GET_BUFFINFO _IOW(AMVENC_AVC_IOC_MAGIC, 0x08, u32)
#define AMVENC_AVC_IOC_SUBMIT	_IOW(AMVENC_AVC_IOC_MAGIC, 0x09, u32)
#define AMVENC_AVC_IOC_READ_CANVAS _IOW(AMVENC_AVC_IOC_MAGIC, 0x0a, u32)


#define IE_PIPPELINE_BLOCK_SHIFT 0
#define IE_PIPPELINE_BLOCK_MASK  0x1f
#define ME_PIXEL_MODE_SHIFT 5
#define ME_PIXEL_MODE_MASK  0x3

enum amvenc_mem_type_e {
	LOCAL_BUFF = 0,
	CANVAS_BUFF,
	PHYSICAL_BUFF,
	MAX_BUFF_TYPE
};

enum amvenc_frame_fmt_e {
	FMT_YUV422_SINGLE = 0,
	FMT_YUV444_SINGLE,
	FMT_NV21,
	FMT_NV12,
	FMT_YUV420,
	FMT_YUV444_PLANE,
	FMT_RGB888,
	FMT_RGB888_PLANE,
	FMT_RGB565,
	FMT_RGBA8888,
	FMT_YUV422_12BIT,
	FMT_YUV444_10BIT,
	FMT_YUV422_10BIT,
	MAX_FRAME_FMT
};

#define AMVENC_BUFFER_LEVEL_480P   0
#define AMVENC_BUFFER_LEVEL_720P   1
#define AMVENC_BUFFER_LEVEL_1080P 2

#define MAX_ENCODE_REQUEST  8   /* 64 */

#define MAX_ENCODE_INSTANCE  8   /* 64 */

#define ENCODE_PROCESS_QUEUE_START	0
#define ENCODE_PROCESS_QUEUE_STOP	1

#define AMVENC_FLUSH_FLAG_INPUT			0x1
#define AMVENC_FLUSH_FLAG_OUTPUT		0x2
#define AMVENC_FLUSH_FLAG_REFERENCE		0x4
#define AMVENC_FLUSH_FLAG_INTRA_INFO	0x8
#define AMVENC_FLUSH_FLAG_INTER_INFO	0x10
#define AMVENC_FLUSH_FLAG_QP				0x20

#define ENCODER_BUFFER_INPUT              0
#define ENCODER_BUFFER_REF0                1
#define ENCODER_BUFFER_REF1                2
#define ENCODER_BUFFER_OUTPUT           3
#define ENCODER_BUFFER_INTER_INFO    4
#define ENCODER_BUFFER_INTRA_INFO    5
#define ENCODER_BUFFER_QP		    6

struct encode_wq_s;

struct encode_request_s {
	u32 quant;
	u32 cmd;
	u32 ucode_mode;

	u32 src;

	u32 framesize;
	u32 qp_info_size;

	u32 me_weight;
	u32 i4_weight;
	u32 i16_weight;

	u32 crop_top;
	u32 crop_bottom;
	u32 crop_left;
	u32 crop_right;
	u32 src_w;
	u32 src_h;
	u32 scale_enable;

	u32 nr_mode;
	u32 flush_flag;
	u32 timeout;
	enum amvenc_mem_type_e type;
	enum amvenc_frame_fmt_e fmt;
	struct encode_wq_s *parent;
};

struct encode_queue_item_s {
	struct list_head list;
	struct encode_request_s request;
};

struct Buff_s {
	u32 buf_start;
	u32 buf_size;
	bool used;
};

struct BuffInfo_s {
	u32 lev_id;
	u32 min_buffsize;
	u32 max_width;
	u32 max_height;
	struct Buff_s dct;
	struct Buff_s dec0_y;
	struct Buff_s dec0_uv;
	struct Buff_s dec1_y;
	struct Buff_s dec1_uv;
	struct Buff_s assit;
	struct Buff_s bitstream;
	struct Buff_s inter_bits_info;
	struct Buff_s inter_mv_info;
	struct Buff_s intra_bits_info;
	struct Buff_s intra_pred_info;
	struct Buff_s qp_info;
	struct Buff_s scale_buff;
#ifdef USE_VDEC2
	struct Buff_s vdec2_info;
#endif
};

struct encode_meminfo_s {
	u32 buf_start;
	u32 buf_size;
	u8 cur_buf_lev;

	u32 BitstreamStart;
	u32 BitstreamEnd;

	/*input buffer define*/
	u32 dct_buff_start_addr;
	u32 dct_buff_end_addr;

	/*microcode assitant buffer*/
	u32 assit_buffer_offset;

	u32 inter_bits_info_ddr_start_addr;
	u32 inter_mv_info_ddr_start_addr;
	u32 intra_bits_info_ddr_start_addr;
	u32 intra_pred_info_ddr_start_addr;
	u32 sw_ctl_info_start_addr;
	u32 scaler_buff_start_addr;
#ifdef USE_VDEC2
	u32 vdec2_start_addr;
#endif

	u32 dump_info_ddr_start_addr;
	u32 dump_info_ddr_size;

	s32 dblk_buf_canvas;
	s32 ref_buf_canvas;
	struct BuffInfo_s bufspec;
#ifdef CONFIG_CMA
	struct page *venc_pages;
#endif
};

struct encode_picinfo_s {
	u32 encoder_width;
	u32 encoder_height;

	u32 rows_per_slice;

	u32 idr_pic_id;  /* need reset as 0 for IDR */
	u32 frame_number;   /* need plus each frame */
	/* need reset as 0 for IDR and plus 2 for NON-IDR */
	u32 pic_order_cnt_lsb;

	u32 log2_max_pic_order_cnt_lsb;
	u32 log2_max_frame_num;
	u32 init_qppicture;
};

struct encode_control_s {
	u32 dct_buffer_write_ptr;
	u32 dct_flush_start;
	bool can_update;
	bool finish;
};

struct encode_wq_s {
	struct list_head list;

	/* dev info */
	u32 ucode_index;
	u32 hw_status;
	u32 output_size;

	u32 sps_size;
	u32 pps_size;

	u32 me_weight;
	u32 i4_weight;
	u32 i16_weight;

	u32 quant_tbl_i4[2][8];
	u32 quant_tbl_i16[2][8];
	u32 quant_tbl_me[2][8];
	u32 qp_table_id;

	struct encode_meminfo_s mem;
	struct encode_picinfo_s pic;
	struct encode_control_s control;
	struct encode_request_s request;
	atomic_t request_ready;
	wait_queue_head_t request_complete;
};

struct encode_event_s {
	wait_queue_head_t hw_complete;
	struct completion process_complete;
	spinlock_t sem_lock; /* for queue switch and create destroy queue. */
	struct completion request_in_com;
};

struct encode_manager_s {
	struct list_head wq;
	struct list_head process_queue;
	struct list_head free_queue;

	u32 encode_hw_status;
	u32 process_queue_state;
	s32 irq_num;
	u32 wq_count;
	u32 ucode_index;
	u32 max_instance;
#ifdef CONFIG_AM_GE2D
	struct ge2d_context_s *context;
#endif
	bool irq_requested;
	bool dblk_fix_flag;
	bool need_reset;
	bool process_irq;
	bool inited; /* power on encode */
	bool remove_flag; /* remove wq; */
	bool uninit_flag; /* power off encode */
	bool use_reserve;

#ifdef CONFIG_CMA
	bool check_cma;
	ulong cma_pool_size;
#endif
	struct platform_device *this_pdev;
	struct Buff_s *reserve_buff;
	struct encode_wq_s *current_wq;
	struct encode_wq_s *last_wq;
	struct encode_queue_item_s *current_item;
	struct task_struct *encode_thread;
	struct Buff_s reserve_mem;
	struct encode_event_s event;
	struct tasklet_struct encode_tasklet;
};

extern s32 encode_wq_add_request(struct encode_wq_s *wq);
extern struct encode_wq_s *create_encode_work_queue(void);
extern s32 destroy_encode_work_queue(struct encode_wq_s *encode_work_queue);

/* Memory Address */
/* ///////////////////////////////////////////////////////////////////////// */
#define MicrocodeStart        0x0000
#define MicrocodeEnd          0x3fff  /* 4kx32bits */
#define HencTopStart          0x4000
#define HencTopEnd            0x4fff  /* 128*32 = 0x1000 */
#define PredTopStart          0x5000
#define PredTopEnd            0x5fff  /* 128x32 = 0x1000 */
#define MBBOT_START_0         0x6000
#define MBBOT_START_1         0x8000

/* 256 Lmem can hold MB_PER_DMA TOP Info */

#define MB_PER_DMA            (256*16/64)
#define MB_PER_DMA_COUNT_I    (MB_PER_DMA*(64/16))
/* 256 Lmem can hold MB_PER_DMA TOP Info */
#define MB_PER_DMA_P          (256*16/160)
#define MB_PER_DMA_COUNT_P    (MB_PER_DMA_P*(160/16))

/********************************************
 *  Interrupt
********************************************/
#define VB_FULL_REQ            0x01
#define MAIN_REQ               0x02
#define VLC_REQ                0x04
#define QDCT_REQ               0x08
#define LDMA_REQ               0x10

/********************************************
 *  Regsiter
********************************************/
#define COMMON_REG_0              r0
#define COMMON_REG_1              r1

#define VB_FULL_REG_0             r2

#define PROCESS_VLC_REG           r3
#define PROCESS_QDCT_REG          r4

#define MAIN_REG_0                r8
#define MAIN_REG_1                r9
#define MAIN_REG_2                r10
#define MAIN_REG_3                r11
#define MAIN_REG_4                r12
#define MAIN_REG_5                r13
#define MAIN_REG_6                r14
#define MAIN_REG_7                r15

#define VLC_REG_0                 r8
#define VLC_REG_1                 r9
#define VLC_REG_2                 r10
#define VLC_REG_3                 r11
#define VLC_REG_4                 r12
#define VLC_REG_5                 r13
#define VLC_REG_6                 r14
#define VLC_REG_7                 r15
#define VLC_REG_8                 r16
#define VLC_REG_9                 r17
#define VLC_REG_10                r18
#define VLC_REG_11                r19
#define VLC_REG_12                r20

#define QDCT_REG_0                r8
#define QDCT_REG_1                r9
#define QDCT_REG_2                r10
#define QDCT_REG_3                r11
#define QDCT_REG_4                r12
#define QDCT_REG_5                r13
#define QDCT_REG_6                r14
#define QDCT_REG_7                r15

#ifdef USE_SW_IF
#else
#define MB_QUANT_CHANGED          r21
#endif
#define LAST_MB_MV_BITS           r22
#define LAST_MB_COEFF_BITS        r23

#define TOP_INFO_0                r24
#define TOP_INFO_1                r25
#define TOP_INFO_1_NEXT           r26
#define TOP_MV_0                  r27
#define TOP_MV_1                  r28
#define TOP_MV_2                  r29
#define TOP_MV_3                  r30

#define vr00                      r8
#define vr01                      r9
#define vr02                      r10
#define vr03                      r11

#define MEM_OFFSET                r31


#ifdef INTRA_IN_P_TOP
#define TOP_Y_DDR_SWAP_LEFT_REG   r32
#define CURRENT_SLICE_QUANT       r33
#define TOP_C_DDR_SWAP_LEFT_REG   r34

#define CURRENT_INTRA_REG         r35
#define TOP_INFO_0_NEXT           r36
#define TOP_INFO_0_READ           r37
#define SW_IF_REG_0               r38
#define SW_IF_REG_1               r39
/* bit[31:1] top */
/* bit[0] left */
#define INTRA_STATUS_REG          r40
/* bit[1] next_top */
/* bit[0] next_left */
#define NEXT_INTRA_STATUS_REG     r41

#define PRED_U                    r42
#define PRED_UR                   r43
#define PRED_L                    r44

#define TOP_Y_DDR_ADDR            r45
#ifdef DBLK_FIX
#define T_L_INFO_REG              r46
#else
#define TOP_C_DDR_ADDR            r46
#endif
#define TOP_W_DDR_ADDR            r47
#define NEXT_TOP_CONTROL_REG      r48
#define TOP_MV_0_d1               r49
#define TOP_MV_0_d2               r50
#else
#define TOTAL_BITS_REG            r32
#define CURRENT_SLICE_QUANT       r33
#define SUM_BITS_8                r34
#define I4x4_MODE_HI_REG          r35
#define I4x4_MODE_LO_REG          r36
#define C_PRED_MODE_REG           r37

#define MV_0_REG                  r35
#define MV_1_REG                  r36
#define MV_2_REG                  r37
#define MV_3_REG                  r38
#define MV_4_REG                  r39
#define MV_5_REG                  r40
#define MV_6_REG                  r41
#define MV_7_REG                  r42
#define MV_8_REG                  r43
#define MV_9_REG                  r44
#define MV_A_REG                  r45
#define MV_B_REG                  r46
#define MV_C_REG                  r47
#define MV_D_REG                  r48
#define MV_E_REG                  r49
#define MV_F_REG                  r50
#endif
#define VLC_MB_INFO_REG           r51


#define MAIN_LOOP_REG_0           r52
#define MAIN_LOOP_REG_1           r53

#define dbg_r0                  r54
#define dbg_r1                  r55
#define dbg_r2                  r56

/********************************************
 *  AV Scratch Register Re-Define
********************************************/
#define ENCODER_STATUS            HCODEC_HENC_SCRATCH_0
#define MEM_OFFSET_REG            HCODEC_HENC_SCRATCH_1
#define DEBUG_REG                 HCODEC_HENC_SCRATCH_2
/* #define MB_COUNT               HENC_SCRATCH_3 */
/* #define IDR_INIT_COUNT         HENC_SCRATCH_4 */

#define IDR_PIC_ID                HCODEC_HENC_SCRATCH_5
#define FRAME_NUMBER              HCODEC_HENC_SCRATCH_6
#define PIC_ORDER_CNT_LSB         HCODEC_HENC_SCRATCH_7

#define LOG2_MAX_PIC_ORDER_CNT_LSB  HCODEC_HENC_SCRATCH_8
#define LOG2_MAX_FRAME_NUM          HCODEC_HENC_SCRATCH_9
#define ANC0_BUFFER_ID              HCODEC_HENC_SCRATCH_A
#define QPPICTURE                   HCODEC_HENC_SCRATCH_B

/* #define START_POSITION              HCODEC_HENC_SCRATCH_C */
#define IE_ME_MB_TYPE               HCODEC_HENC_SCRATCH_D

/* bit 0-4, IE_PIPPELINE_BLOCK
 * bit 5    me half pixel in m8
 *		disable i4x4 in gxbb
 * bit 6    me step2 sub pixel in m8
 *		disable i16x16 in gxbb
 */
#define IE_ME_MODE                  HCODEC_HENC_SCRATCH_E
#define IE_REF_SEL                  HCODEC_HENC_SCRATCH_F


/* [21:16] P_INTRA_QUANT */
/* [15:0]  INTRA_MIN_BITS */
#define P_INTRA_CONFIG              HCODEC_HENC_SCRATCH_G

/* [31:16] TARGET_BITS_PER_MB */
/* [15:8] MIN_QUANT */
/* [7:0] MAX_QUANT */
#define P_MB_QUANT_CONFIG           HCODEC_HENC_SCRATCH_I
/* [31:24] INC_4_BITS */
/* [23:16] INC_3_BITS */
/* [15:8]  INC_2_BITS */
/* [7:0]   INC_1_BITS */
#define P_MB_QUANT_INC_CFG          HCODEC_HENC_SCRATCH_J
/* [31:24] DEC_4_BITS */
/* [23:16] DEC_3_BITS */
/* [15:8]  DEC_2_BITS */
/* [7:0]   DEC_1_BITS */
#define P_MB_QUANT_DEC_CFG          HCODEC_HENC_SCRATCH_K

/* [31:0] NUM_ROWS_PER_SLICE_P */
/* [15:0] NUM_ROWS_PER_SLICE_I */
#define FIXED_SLICE_CFG             HCODEC_HENC_SCRATCH_L

/* Each MB have 32 bits : */
/* 12-bits MV_BITS, 4-bits MB_TYPE,  and 16-bits COEFF_BITS */
#define BITS_INFO_DDR_START         HCODEC_HENC_SCRATCH_M
/* Each MV has 16 x 32 bits */
#define MV_INFO_DDR_START           HCODEC_HENC_SCRATCH_N
/* Each I4x4 has 64 bits */
/* shared will not dump I4x4 and MV at same time */
#define I4x4_INFO_DDR_START         MV_INFO_DDR_START

/* can be shared by BITS_INFO_DDR_START */
/* bit[7] - 0-same slice, 1-new slice */
/* bit[6] - 0-inter, 1-intra */
/* bit[5:0] - quant */
#define SW_CTL_INFO_DDR_START       BITS_INFO_DDR_START

#define CURRENT_Y_CANVAS_START      HCODEC_HENC_SCRATCH_3
#define CURRENT_C_CANVAS_START      HCODEC_HENC_SCRATCH_4
/* For Block Mode 1 - 32x32 */
/* If CAVAS width = 1920, then row_size = 1920/32 * 32 * 32 = 61440 (0xf000) */
#define CANVAS_ROW_SIZE             HCODEC_HENC_SCRATCH_C

/* For GX */
#define INFO_DUMP_START_ADDR      HCODEC_HENC_SCRATCH_I

#define LOW_LATENCY_EN_REG          DOS_SCRATCH9
#define PREVIOUS_FNUM_REG           DOS_SCRATCH10

/* --------------------------------------------------- */
/* ENCODER_STATUS define */
/* --------------------------------------------------- */
#define ENCODER_IDLE              0
#define ENCODER_SEQUENCE          1
#define ENCODER_PICTURE           2
#define ENCODER_IDR               3
#define ENCODER_NON_IDR           4
#define ENCODER_MB_HEADER         5
#define ENCODER_MB_DATA           6

#define ENCODER_SEQUENCE_DONE          7
#define ENCODER_PICTURE_DONE           8
#define ENCODER_IDR_DONE               9
#define ENCODER_NON_IDR_DONE           10
#define ENCODER_MB_HEADER_DONE         11
#define ENCODER_MB_DATA_DONE           12

#define ENCODER_NON_IDR_INTRA     13
#define ENCODER_NON_IDR_INTER     14

#define ENCODER_ERROR     0xff

/* --------------------------------------------------- */
/* NAL start code define */
/* --------------------------------------------------- */
/* defines for H.264 */
#define Coded_slice_of_a_non_IDR_picture      1
#define Coded_slice_of_an_IDR_picture         5
#define Supplemental_enhancement_information  6
#define Sequence_parameter_set                7
#define Picture_parameter_set                 8

/* defines for H.264 slice_type */
#define I_Slice                               2
#define P_Slice                               5
#define B_Slice                               6

#define P_Slice_0                             0
#define B_Slice_1                             1
#define I_Slice_7                             7

#define nal_reference_idc_idr     3
#define nal_reference_idc_non_idr 2

#define SEQUENCE_NAL ((nal_reference_idc_idr<<5) | Sequence_parameter_set)
#define PICTURE_NAL  ((nal_reference_idc_idr<<5) | Picture_parameter_set)
#define IDR_NAL      ((nal_reference_idc_idr<<5) | \
			 Coded_slice_of_an_IDR_picture)
#define NON_IDR_NAL  ((nal_reference_idc_non_idr<<5) | \
			 Coded_slice_of_a_non_IDR_picture)

/* --------------------------------------------------- */
/* I_IN_P TOP Status */
/* --------------------------------------------------- */
#define I_IN_P_TOP_STATUS_IDLE    0
#define I_IN_P_TOP_STATUS_READ_Y  1
#define I_IN_P_TOP_STATUS_READ_C  2
#define I_IN_P_TOP_STATUS_WRITE   3
/********************************************
 *  Local Memory
********************************************/
/* #define INTR_MSK_SAVE                  0x000 */
/* #define QPPicture                      0x001 */
/* #define i_pred_mbx                     0x002 */
/* #define i_pred_mby                     0x003 */
/* #define log2_max_pic_order_cnt_lsb     0x004 */
/* #define log2_max_frame_num             0x005 */
/* #define frame_num                      0x006 */
/* #define idr_pic_id                     0x007 */
/* #define pic_order_cnt_lsb              0x008 */
/* #define picture_type                   0x009 */
/* #define top_mb_begin                   0x00a */
/* #define top_mb_end                     0x00b */
/* #define pic_width_in_mbs_minus1        0x00c */
/* #define pic_height_in_map_units_minus1 0x00d */
/* #define anc0_buffer_id                 0x00e */

/* #define enc_header_ready               0x00f */


/* #define cur_mv_bits                    0x010 */
/* #define cur_coeff_bits                 0x011 */
/* #define slice_mb_num                   0x012 */
/* #define current_slice_quant            0x013 */

/* #define insert_slice_header            0x014 */

/* #define Quant_change_bits              0x015 */

/* #define prev_mb_quant                  0x016 */
/* #define current_mb_quant               0x017 */
/* #define next_mb_quant                  0x018 */
/* #define delta_qp_data                  0x019 */
/* #define qp_change_mbx                  0x01a */

/* #define process_vlc_mbx                0x01b */
/* #define process_vlc_mby                0x01c */
/* #define big_delta_qp                   0x01d */

/* #define current_mb_type                0x01e */
/* #define next_mb_type                   0x01f */

/* #define T_BITS_0                       0x020 */
/* #define T_BITS_1                       0x021 */
/* #define T_BITS_2                       0x022 */
/* #define T_BITS_3                       0x023 */
/* #define T_BITS_4                       0x024 */
/* #define T_BITS_5                       0x025 */
/* #define T_BITS_6                       0x026 */

/* #define just_changed_status            0x027 */
/* #define mb_type                        0x028 */
/* #define top_store_intra                0x029 */
/* #define MB_SKIP_RUN_I_IN_P             0x02a */
/* #define I_IN_P_TOP_STATUS              0x02b */
/* #define WAIT_I_IN_P_TOP_STATUS         0x02c */
/* #define top_pre_load_times             0x02d */

/* #define MB_INC_1_BITS                  0x030 */
/* #define MB_INC_2_BITS                  0x031 */
/* #define MB_INC_3_BITS                  0x032 */
/* #define MB_INC_4_BITS                  0x033 */
/* #define MB_DEC_1_BITS                  0x034 */
/* #define MB_DEC_2_BITS                  0x035 */
/* #define MB_DEC_3_BITS                  0x036 */
/* #define MB_DEC_4_BITS                  0x037 */
/* #define MB_MIN_QUANT                   0x038 */
/* #define MB_MAX_QUANT                   0x039 */

/* #define ie_me_mode                     0x040 */

/* there are 32 bits BITS_INFO Per MB */
/* 12-bits MV_BITS, 4-bits MB_TYPE,  and 16-bits COEFF_BITS */
/* #define BITS_INFO_START                0x100 */

/* #ifdef HDEC_BLK_MODE_LINEAR */
/* #define INTRA_LEFT_START               0x200 */
/* #define INTRA_LEFT_YC_START            0x280 */
/* #define INTRA_TOP_START                0x200 */
/* #define INTRA_TOP_Y_START              0x220 */
/* #define INTRA_TOP_C_START              0x240 */
/* #else */
/* #define INTRA_LEFT_C_START             0x180 */
/* #define INTRA_LEFT_START               0x200 */
/* #define INTRA_LEFT_Y_START             0x200 */
/* #endif */


/* #define SW_CTL_INFO_START              BITS_INFO_START */

/* there are 64 bits I4x4_INFO per MB */
/* #define I4x4_INFO_START                0x200 */

/* there are 16x32 bits MV_INFO per MB */
/* #define MV_INFO_START                  0x200 */

/* #define HENC_TOP_LMEM_BEGIN            0x300 */

/********************************************
* defines for HENC command
********************************************/
#define HENC_SEND_MB_TYPE_COMMAND           1
#define HENC_SEND_I_PRED_MODE_COMMAND       2
#define HENC_SEND_C_I_PRED_MODE_COMMAND     3
#define HENC_SEND_CBP_COMMAND               4
#define HENC_SEND_DELTA_QP_COMMAND          5
#define HENC_SEND_COEFF_COMMAND             6
#define HENC_SEND_SKIP_COMMAND              7
#define HENC_SEND_B8_MODE_COMMAND           8
#define HENC_SEND_MVD_COMMAND               9
#define HENC_SEND_MB_DONE_COMMAND          15

/* defines for picture type*/
#define HENC_I_PICTURE      0
#define HENC_P_PICTURE      1
#define HENC_B_PICTURE      2

/********************************************
* defines for H.264 mb_type
********************************************/
#define HENC_MB_Type_PBSKIP                      0x0
#define HENC_MB_Type_PSKIP                       0x0
#define HENC_MB_Type_BSKIP_DIRECT                0x0
#define HENC_MB_Type_P16x16                      0x1
#define HENC_MB_Type_P16x8                       0x2
#define HENC_MB_Type_P8x16                       0x3
#define HENC_MB_Type_SMB8x8                      0x4
#define HENC_MB_Type_SMB8x4                      0x5
#define HENC_MB_Type_SMB4x8                      0x6
#define HENC_MB_Type_SMB4x4                      0x7
#define HENC_MB_Type_P8x8                        0x8
#define HENC_MB_Type_I4MB                        0x9
#define HENC_MB_Type_I16MB                       0xa
#define HENC_MB_Type_IBLOCK                      0xb
#define HENC_MB_Type_SI4MB                       0xc
#define HENC_MB_Type_I8MB                        0xd
#define HENC_MB_Type_IPCM                        0xe
#define HENC_MB_Type_AUTO                        0xf

#define HENC_MB_CBP_AUTO                         0xff
#define HENC_SKIP_RUN_AUTO                     0xffff


/* ///////////////////////////////////////////////////////////////////////// */
/* TOP/LEFT INFO Define */
/* ///////////////////////////////////////////////////////////////////////// */

/* For I Slice */
#define DEFAULT_INTRA_TYPE      0xffff
#define DEFAULT_CBP_BLK         0x0000
#define DEFAULT_C_NNZ           0x0000
#define DEFAULT_Y_NNZ           0x0000

#define DEFAULT_MVX             0x8000
#define DEFAULT_MVY             0x4000

/* For I Slice */
/* Bit[31:20] Reserved */
/* Bit[19:16] cbp */
/* Bit[15:0] IntraType */
/* `define     HENC_TOP_INFO_0        8'h37 */
/* `define     HENC_LEFT_INFO_0       8'h38 */

/* For I Slice and Intra/Inter Mixed Slice */
/* Bit[31:24] V_nnz */
/* Bit[23:16] U_nnz */
/* Bit[15:0]  Y_nnz */
/* `define     HENC_TOP_INFO_1        8'h39 */
/* `define     HENC_LEFT_INFO_1       8'h3a */

/* For Intra/Inter Mixed Slice */
/*  */
/* bit[31] -  cbp[3] */
/* bit[30:16] - MVY ( 0x3fff Means Intra MB) */
/* bit[15:0]  - MVX ( IntraType for Intra MB) */
/* `define     HENC_TOP_MV_0 */
/* bit[31] -  cbp[2] */
/* bit[30:16] - MVY */
/* bit[15:0]  - MVX */
/* `define     HENC_TOP_MV_1 */
/* bit[31] -  cbp[1] */
/* bit[30:16] - MVY */
/* bit[15:0]  - MVX */
/* `define     HENC_TOP_MV_2 */
/* bit[31] -  cbp[0] */
/* bit[30:16] - MVY */
/* bit[15:0]  - MVX */
/* `define     HENC_TOP_MV_3 */

/* `define     HENC_LEFT_MV_0 */
/* `define     HENC_LEFT_MV_1 */
/* `define     HENC_LEFT_MV_2 */
/* `define     HENC_LEFT_MV_3 */

/* ///////////////////////////////////////////////////////////////////////// */
/*  */
/* ///////////////////////////////////////////////////////////////////////// */

extern bool amvenc_avc_on(void);
#endif
