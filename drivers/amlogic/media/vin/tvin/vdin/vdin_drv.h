/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * drivers/amlogic/media/vin/tvin/vdin/vdin_drv.h
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
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

#ifndef __TVIN_VDIN_DRV_H
#define __TVIN_VDIN_DRV_H

/* Standard Linux Headers */
#include <linux/cdev.h>
#include <linux/irqreturn.h>
#include <linux/timer.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/device.h>
#include <linux/clk.h>
#include <linux/workqueue.h>

/* Amlogic Headers */
#include <linux/amlogic/media/registers/cpu_version.h>
#include <linux/amlogic/media/canvas/canvas.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/frame_provider/tvin/tvin_v4l2.h>
#include <linux/amlogic/media/video_sink/video_signal_notify.h>
#include <linux/amlogic/media/amvecm/amvecm.h>
#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
#include <linux/amlogic/media/rdma/rdma_mgr.h>
#endif

/* v4l2 header */
#include <media/v4l2-common.h>
#include <linux/videodev2.h>
#include <uapi/linux/v4l2-ext/videodev2-ext.h>
#include <uapi/linux/v4l2-ext/v4l2-controls-ext.h>
#include <uapi/linux/linuxtv-ext-ver.h>
#include <linux/v4l2-dv-timings.h>
#include <media/v4l2-device.h>
#include <media/v4l2-dev.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-dv-timings.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-event.h>
#include <media/videobuf2-v4l2.h>
#include <media/videobuf2-dma-contig.h>

/* Local Headers */
#include "../tvin_frontend.h"
#include "vdin_vf.h"
#include "vdin_regs.h"
//#include "vdin_v4l2_if.h"

/* 20220124: vf->flag add pc mode flag to bypass aipq aisr */
/* 20220214: The desktop screenshot probability gray screen */
/* 20220216: t5d dmc write 1 set to supper urgent */
/* 20220222: reduce hdmi signal confirm stable time */
/* 20220224: keystone stuck when str stress test */
/* 20220301: 119.88 get duration value is 800 not correct */
/* 20220308: screen capture 720 and then 1080 fail */
/* 20220310: vdin and vrr/dlg support */
/* 20220314: get vdin frontend info */
/* 20220328: return the same vf repeat causes crash */
/* 20220331: starting state chg not send event */
/* 20220401: 59.94 duration need set to 1601 */
/* 20220402: add vdin v4l2 feature */
/* 20220408: get format convert when started */
/* 20220415: transmit freesync data */
/* 20220416: dv game mode picture display abnormal */
/* 20220422: linux system pc mode myself restart dec */
#define VDIN_VER "20220422:linux system pc mode myself restart dec"

//#define VDIN_BRINGUP_NO_VF
//#define VDIN_BRINGUP_NO_VLOCK
//#define VDIN_BRINGUP_NO_AMLVECM
//#define VDIN_BRINGUP_BYPASS_COLOR_CNVT
#define K_FORCE_HV_SHRINK	0
#define VDIN_V4L2_INPUT_MAX		7

enum vdin_work_mode_e {
	VDIN_WORK_MD_NORMAL = 0,
	VDIN_WORK_MD_V4L = 1,
};

/*the counter of vdin*/
#define VDIN_MAX_DEVS			2

enum vdin_hw_ver_e {
	VDIN_HW_ORG = 0,
	VDIN_HW_SM1,
	VDIN_HW_TL1,
	/*
	 * tm2 vdin0/vdin1 all support upto 40k
	 */
	VDIN_HW_TM2,
	VDIN_HW_TM2_B,
	/*
	 * sc2, sm1 vdin0 upto 4k, vdin1 upto 1080P (write)
	 * no afbce
	 */
	VDIN_HW_SC2,
	VDIN_HW_T5,
	VDIN_HW_T5D,
	/*
	 * t7 removed canvas
	 * no DV meta data slicer
	 * dv 422 to 444, and to 10 bit 422 (fix shrink issue)
	 */
	VDIN_HW_T7,
	/*
	 * 1.remove vdin0,	2.v scaling down input frame h max is 2k
	 * 3.buff 2k		4.post matrix (hdr matrix)
	 */
	VDIN_HW_S4,
	VDIN_HW_S4D,
	VDIN_HW_T3,
	VDIN_HW_T5W,
};

enum vdin_irq_flg_e {
	VDIN_IRQ_FLG_NO_END = 1,
	VDIN_IRQ_FLG_IRQ_STOP = 2,
	VDIN_IRQ_FLG_FAKE_IRQ = 3,
	VDIN_IRQ_FLG_DROP_FRAME = 4,
	VDIN_IRQ_FLG_DV_CHK_SUM_ERR = 5,
	VDIN_IRQ_FLG_CYCLE_CHK,
	VDIN_IRQ_FLG_SIG_NOT_STABLE,
	VDIN_IRQ_FLG_FMT_TRANS_CHG,
	VDIN_IRQ_FLG_CSC_CHG,
	VDIN_IRQ_FLG_BUFF_SKIP, /* 10 */
	VDIN_IRQ_FLG_IGNORE_FRAME,
	VDIN_IRQ_FLG_SKIP_FRAME,
	VDIN_IRQ_FLG_GM_DV_CHK_SUM_ERR,
	VDIN_IRQ_FLG_NO_WR_FE,
	VDIN_IRQ_FLG_NO_NEXT_FE, /* 15 */
	VDIN_IRQ_FLG_SECURE_MD,
	VDIN_IRQ_FLG_VRR_CHG,
};

/* for config hw function support */
struct match_data_s {
	char *name;
	enum vdin_hw_ver_e hw_ver;
	bool de_tunnel_tunnel;
	/* tm2 verb :444 de-tunnel and wr mif 12 bit mode*/
	bool ipt444_to_422_12bit;
	u32 vdin0_en;
	u32 vdin1_en;
	u32 vdin0_line_buff_size;
	u32 vdin1_line_buff_size;
};

/* #define VDIN_CRYSTAL               24000000 */
/* #define VDIN_MEAS_CLK              50000000 */
/* values of vdin_dev_t.flags */
#define VDIN_FLAG_NULL			0x00000000
#define VDIN_FLAG_DEC_INIT		0x00000001
#define VDIN_FLAG_DEC_STARTED		0x00000002
#define VDIN_FLAG_DEC_OPENED		0x00000004
#define VDIN_FLAG_DEC_REGED		0x00000008
#define VDIN_FLAG_DEC_STOP_ISR		0x00000010
#define VDIN_FLAG_FORCE_UNSTABLE	0x00000020
#define VDIN_FLAG_FS_OPENED		0x00000100
#define VDIN_FLAG_SKIP_ISR              0x00000200
/*flag for vdin0 output*/
#define VDIN_FLAG_OUTPUT_TO_NR		0x00000400
/*flag for force vdin buffer recycle*/
#define VDIN_FLAG_FORCE_RECYCLE         0x00000800
/*flag for vdin scale down,color fmt conversion*/
#define VDIN_FLAG_MANUAL_CONVERSION     0x00001000
/*flag for vdin rdma enable*/
#define VDIN_FLAG_RDMA_ENABLE           0x00002000
/*flag for vdin snow on&off*/
#define VDIN_FLAG_SNOW_FLAG             0x00004000
/*flag for disable vdin sm*/
#define VDIN_FLAG_SM_DISABLE            0x00008000
/*flag for vdin suspend state*/
#define VDIN_FLAG_SUSPEND               0x00010000
/*flag for vdin-v4l2 debug*/
#define VDIN_FLAG_V4L2_DEBUG            0x00020000
/*flag for isr req&free*/
#define VDIN_FLAG_ISR_REQ               0x00040000
#define VDIN_FLAG_ISR_EN		0x00100000

/* flag for rdma done in isr*/
#define VDIN_FLAG_RDMA_DONE             0x00080000

/*values of vdin isr bypass check flag */
#define VDIN_BYPASS_STOP_CHECK          0x00000001
#define VDIN_BYPASS_CYC_CHECK           0x00000002
#define VDIN_BYPASS_VGA_CHECK           0x00000008

/*values of vdin game mode process flag */
/*enable*/
#define VDIN_GAME_MODE_0                (BIT0)
/*delay 1 frame, vdin have one frame buffer delay*/
#define VDIN_GAME_MODE_1                (BIT1)
/*delay 2 frame write, read at same buffer*/
#define VDIN_GAME_MODE_2                (BIT2)
/*when phase lock, will switch 2 to 1*/
#define VDIN_GAME_MODE_SWITCH_EN        (BIT3)

/*flag for flush vdin buff*/
#define VDIN_FLAG_BLACK_SCREEN_ON	1
#define VDIN_FLAG_BLACK_SCREEN_OFF	0
/* size for rdma table */
#define RDMA_TABLE_SIZE			(PAGE_SIZE >> 3)
/* #define VDIN_DEBUG */
#define VDIN_SELF_STOP_START		(BIT0)


#define IS_HDMI_SRC(src)	\
		({typeof(src) src_ = src; \
		 (((src_) >= TVIN_PORT_HDMI0) && \
		 ((src_) <= TVIN_PORT_HDMI7)); })

#define IS_CVBS_SRC(src)	\
		({typeof(src) src_ = src; \
		 (((src_) >= TVIN_PORT_CVBS0) && \
		 ((src_) <= TVIN_PORT_CVBS3)); })

#define H_SHRINK_TIMES_4k	4
#define V_SHRINK_TIMES_4k	4
#define H_SHRINK_TIMES_1080	2
#define V_SHRINK_TIMES_1080	2

/*vdin write mem color-depth support*/
enum vdin_wr_color_depth {
	VDIN_WR_COLOR_DEPTH_8BIT = 0x01,
	VDIN_WR_COLOR_DEPTH_9BIT = 0x02,
	VDIN_WR_COLOR_DEPTH_10BIT = 0x04,
	VDIN_WR_COLOR_DEPTH_12BIT = 0x08,
	/*TXL new add*/
	VDIN_WR_COLOR_DEPTH_10BIT_FULL_PCAK_MODE = 0x10,
	VDIN_WR_COLOR_DEPTH_FORCE_MEM_YUV422_TO_YUV444 = 0x20,
};

#define VDIN_422_FULL_PK_EN			1
#define VDIN_422_FULL_PK_DIS			0

/* vdin afbce flag */
#define VDIN_AFBCE_EN                   (BIT0)
#define VDIN_AFBCE_EN_LOOSY             (BIT1)
#define VDIN_AFBCE_EN_4K                (BIT4)
#define VDIN_AFBCE_EN_1080P             (BIT5)
#define VDIN_AFBCE_EN_720P              (BIT6)
#define VDIN_AFBCE_EN_SMALL             (BIT7)

enum color_deeps_cfge {
	COLOR_DEEPS_AUTO = 0,
	COLOR_DEEPS_8BIT = 8,
	COLOR_DEEPS_10BIT = 10,
	COLOR_DEEPS_12BIT = 12,
	COLOR_DEEPS_MANUAL = 0x100,
};

enum vdin_color_deeps_e {
	VDIN_COLOR_DEEPS_8BIT = 8,
	VDIN_COLOR_DEEPS_9BIT = 9,
	VDIN_COLOR_DEEPS_10BIT = 10,
	VDIN_COLOR_DEEPS_12BIT = 12,
};

enum vdin_vf_put_md {
	VDIN_VF_PUT,
	VDIN_VF_RECYCLE,
};

#define VDIN_ISR_MONITOR_HDR	BIT(0)
#define VDIN_ISR_MONITOR_EMP	BIT(1)
#define VDIN_ISR_MONITOR_RATIO	BIT(2)
#define VDIN_ISR_MONITOR_GAME	BIT(4)
#define VDIN_ISR_MONITOR_VS	BIT(5)
#define VDIN_ISR_MONITOR_VF	BIT(6)
#define VDIN_ISR_MONITOR_VRR_DATA	BIT(9)

/* *********************************************************************** */
/* *** enum definitions ********************************************* */
/* *********************************************************************** */
/*
 *YUV601:  SDTV BT.601            YCbCr (16~235, 16~240, 16~240)
 *YUV601F: SDTV BT.601 Full_Range YCbCr ( 0~255,  0~255,  0~255)
 *YUV709:  HDTV BT.709            YCbCr (16~235, 16~240, 16~240)
 *YUV709F: HDTV BT.709 Full_Range YCbCr ( 0~255,  0~255,  0~255)
 *RGBS:                       StudioRGB (16~235, 16~235, 16~235)
 *RGB:                              RGB ( 0~255,  0~255,  0~255)
 */
enum vdin_matrix_csc_e {
	VDIN_MATRIX_NULL = 0,
	VDIN_MATRIX_XXX_YUV601_BLACK,/*1*/
	VDIN_MATRIX_RGB_YUV601,
	VDIN_MATRIX_GBR_YUV601,
	VDIN_MATRIX_BRG_YUV601,
	VDIN_MATRIX_YUV601_RGB,/*5*/
	VDIN_MATRIX_YUV601_GBR,
	VDIN_MATRIX_YUV601_BRG,
	VDIN_MATRIX_RGB_YUV601F,
	VDIN_MATRIX_YUV601F_RGB,/*9*/
	VDIN_MATRIX_RGBS_YUV601,/*10*/
	VDIN_MATRIX_YUV601_RGBS,
	VDIN_MATRIX_RGBS_YUV601F,
	VDIN_MATRIX_YUV601F_RGBS,
	VDIN_MATRIX_YUV601F_YUV601,
	VDIN_MATRIX_YUV601_YUV601F,/*15*/
	VDIN_MATRIX_RGB_YUV709,
	VDIN_MATRIX_YUV709_RGB,
	VDIN_MATRIX_YUV709_GBR,
	VDIN_MATRIX_YUV709_BRG,
	VDIN_MATRIX_RGB_YUV709F,/*20*/
	VDIN_MATRIX_YUV709F_RGB,
	VDIN_MATRIX_RGBS_YUV709,
	VDIN_MATRIX_YUV709_RGBS,
	VDIN_MATRIX_RGBS_YUV709F,
	VDIN_MATRIX_YUV709F_RGBS,/*25*/
	VDIN_MATRIX_YUV709F_YUV709,
	VDIN_MATRIX_YUV709_YUV709F,
	VDIN_MATRIX_YUV601_YUV709,
	VDIN_MATRIX_YUV709_YUV601,
	VDIN_MATRIX_YUV601_YUV709F,/*30*/
	VDIN_MATRIX_YUV709F_YUV601,
	VDIN_MATRIX_YUV601F_YUV709,
	VDIN_MATRIX_YUV709_YUV601F,
	VDIN_MATRIX_YUV601F_YUV709F,
	VDIN_MATRIX_YUV709F_YUV601F,/*35*/
	VDIN_MATRIX_RGBS_RGB,
	VDIN_MATRIX_RGB_RGBS,
	VDIN_MATRIX_RGB2020_YUV2020,
	VDIN_MATRIX_YUV2020F_YUV2020,
};

enum vdin_matrix_sel_e {
	VDIN_SEL_MATRIX0 = 0,
	VDIN_SEL_MATRIX1,
	VDIN_SEL_MATRIXHDR,/*after, equeal g12a have*/
};

static inline const char
*vdin_fmt_convert_str(enum vdin_format_convert_e fmt_cvt)
{
	switch (fmt_cvt) {
	case VDIN_FORMAT_CONVERT_YUV_YUV422:
		return "FMT_CONVERT_YUV_YUV422";
	case VDIN_FORMAT_CONVERT_YUV_YUV444:
		return "FMT_CONVERT_YUV_YUV444";
	case VDIN_FORMAT_CONVERT_YUV_RGB:
		return "FMT_CONVERT_YUV_RGB";
	case VDIN_FORMAT_CONVERT_RGB_YUV422:
		return "FMT_CONVERT_RGB_YUV422";
	case VDIN_FORMAT_CONVERT_RGB_YUV444:
		return "FMT_CONVERT_RGB_YUV444";
	case VDIN_FORMAT_CONVERT_RGB_RGB:
		return "FMT_CONVERT_RGB_RGB";
	case VDIN_FORMAT_CONVERT_YUV_NV12:
		return "VDIN_FORMAT_CONVERT_YUV_NV12";
	case VDIN_FORMAT_CONVERT_YUV_NV21:
		return "VDIN_FORMAT_CONVERT_YUV_NV21";
	case VDIN_FORMAT_CONVERT_RGB_NV12:
		return "VDIN_FORMAT_CONVERT_RGB_NV12";
	case VDIN_FORMAT_CONVERT_RGB_NV21:
		return "VDIN_FORMAT_CONVERT_RGB_NV21";
	default:
		return "FMT_CONVERT_NULL";
	}
}

/* only for keystone use begin */
struct vdin_set_canvas_s {
	int fd;
	int index;
};

struct vdin_set_canvas_addr_s {
	long paddr;
	int  size;

	struct dma_buf *dmabuff;
	struct dma_buf_attachment *dmabufattach;
	struct sg_table *sgtable;
	unsigned long vfmem_start;
	int fd;
	int index;
};
extern struct vdin_set_canvas_addr_s vdin_set_canvas_addr[VDIN_CANVAS_MAX_CNT];

struct vdin_vf_info {
	int index;
	unsigned int crc;

	/* [0]:vdin get frame time,
	 * [1]: vdin put frame time
	 * [2]: receiver get frame time
	 */
	long long ready_clock[3];/* ns */
};

/*******for debug **********/
struct vdin_debug_s {
	struct tvin_cutwin_s cutwin;
	unsigned short scaler4h;/* for vscaler */
	unsigned short scaler4w;/* for hscaler */
	unsigned short dest_cfmt;/* for color fmt conversion */
};

struct vdin_dv_s {
	struct vframe_provider_s vprov_dv;
	struct tvin_dv_vsif_raw_s dv_vsif_raw;
	struct delayed_work dv_dwork;
	unsigned int dv_cur_index;
	unsigned int dv_next_index;
	unsigned int dolby_input;
	unsigned int dv_dma_size;
	dma_addr_t dv_dma_paddr;
	void *dv_dma_vaddr;
	void *temp_meta_data;
	dma_addr_t meta_data_raw_pbuff0;/*for t7*/
	void *meta_data_raw_vbuff0;/*for t7*/
	void *meta_data_raw_buff1;/*for t7*/
	unsigned int dv_flag_cnt;/*cnt for no dv input*/
	u8 dv_flag;
	bool dv_config;
	bool dv_path_idx;
	bool dv_crc_check;/*0:fail;1:ok*/
	unsigned int dv_mem_alloced;
	struct tvin_dv_vsif_s dv_vsif;/*dolby vsi info*/
	bool low_latency;
	unsigned int chg_cnt;
	unsigned int allm_chg_cnt;
};

struct vdin_afbce_s {
	unsigned int  frame_head_size;/*1 frame head size*/
	unsigned int  frame_table_size;/*1 frame table size*/
	unsigned int  frame_body_size;/*1 frame body size*/
	unsigned long head_paddr;
	unsigned long table_paddr;
	/*every frame head addr*/
	unsigned long fm_head_paddr[VDIN_CANVAS_MAX_CNT];
	/*every frame tab addr*/
	unsigned long fm_table_paddr[VDIN_CANVAS_MAX_CNT];
	/*every body head addr*/
	unsigned long fm_body_paddr[VDIN_CANVAS_MAX_CNT];
};

struct vdin_event_info {
	/*enum tvin_sg_chg_flg*/
	u32 event_sts;
};

enum vdin_game_mode_chg_e {
	VDIN_GAME_MODE_UNCHG = 0,
	VDIN_GAME_MODE_OFF_2_ON,
	VDIN_GAME_MODE_ON_2_OFF,
	VDIN_GAME_MODE_NUM
};

struct vdin_v4l2_stat_s {
	/* frame drop due to framerate control */
	unsigned int drop_divide;
	unsigned int done_cnt;
	unsigned int que_cnt;
	unsigned int dque_cnt;
};

struct vdin_v4l2_s {
	int divide;
	unsigned int secure_flg:1;
	enum v4l2_ext_capture_location l;
	struct vdin_v4l2_stat_s stats;
};

struct vdin_vrr_s {
	unsigned int vdin_vrr_en_flag;
	struct tvin_vtem_data_s vtem_data;
	struct tvin_spd_data_s spd_data;
	unsigned int vrr_chg_cnt;
};

struct vdin_dev_s {
	struct cdev cdev;
	struct device *dev;
	struct tvin_parm_s parm;
	struct tvin_format_s *fmt_info_p;
	struct vf_pool *vfp;
	struct tvin_frontend_s *frontend;
	struct tvin_frontend_s	vdin_frontend;
	struct tvin_sig_property_s pre_prop;
	struct tvin_sig_property_s prop;
	struct vframe_provider_s vprov;
	struct vdin_dv_s dv;
	struct delayed_work vlock_dwork;
	struct vdin_afbce_s *afbce_info;
	struct tvin_to_vpp_info_s vdin2vpp_info;
	/*vdin event*/
	struct vdin_event_info event_info;
	struct vdin_event_info pre_event_info;
	/*struct extcon_dev *extcon_event;*/
	struct delayed_work event_dwork;
	struct vdin_vrr_s vrr_data;

	 /* 0:from gpio A,1:from csi2 , 2:gpio B*/
	enum bt_path_e bt_path;
	const struct match_data_s *dtdata;
	struct timer_list timer;
	spinlock_t isr_lock;/*isr lock*/
	spinlock_t hist_lock;/*pq history lock*/
	struct mutex fe_lock;/*front end lock*/
	struct clk *msr_clk;
	unsigned int msr_clk_val;

	struct vdin_debug_s debug;
	enum vdin_format_convert_e format_convert;
	unsigned int mif_fmt;/*enum vdin_mif_fmt*/
	enum vdin_color_deeps_e source_bitdepth;
	enum vdin_matrix_csc_e csc_idx;
	struct vf_entry *curr_wr_vfe;
	struct vf_entry *last_wr_vfe;
	unsigned int vdin_delay_vfe2rdlist;
	unsigned int curr_field_type;
	unsigned int curr_dv_flag;
	unsigned int drop_hdr_set_sts;
	unsigned int starting_chg;

	char name[15];
	/* bit0 TVIN_PARM_FLAG_CAP bit31: TVIN_PARM_FLAG_WORK_ON */
	unsigned int flags;
	unsigned int flags_isr;
	unsigned int index;
	unsigned int vdin_max_pixelclk;

	unsigned long mem_start;
	unsigned int mem_size;
	unsigned long vfmem_start[VDIN_CANVAS_MAX_CNT];
	struct page *vfvenc_pages[VDIN_CANVAS_MAX_CNT];

	/* save secure handle */
	unsigned int secure_handle;
	bool secure_en;
	bool mem_protected;
	unsigned int vfmem_size;
	unsigned int vfmem_size_small;/* double write use */
	unsigned int frame_size;
	unsigned int vfmem_max_cnt;/*real buffer number*/
	unsigned int frame_buff_num;/*dts config data*/

	unsigned int h_active;
	unsigned int v_active;
	unsigned int h_shrink_out;/* double write use */
	unsigned int v_shrink_out;/* double write use */
	unsigned int h_shrink_times;/* double write use */
	unsigned int v_shrink_times;/* double write use */
	unsigned int h_active_org;/*vdin scaler in*/
	unsigned int v_active_org;/*vdin scaler in*/
	unsigned int canvas_h;
	unsigned int canvas_w;
	unsigned int canvas_active_w;
	unsigned int canvas_alin_w;
	unsigned int canvas_max_size;
	unsigned int canvas_max_num;
	unsigned int vf_canvas_id[VDIN_CANVAS_MAX_CNT];
	/*before G12A:32byte(256bit)align;
	 *after G12A:64byte(512bit)align
	 */
	unsigned int canvas_align;

	int irq;
	unsigned int rdma_irq;
	int vpu_crash_irq;
	int wr_done_irq;
	int vdin2_meta_wr_done_irq;
	char irq_name[12];
	char vpu_crash_irq_name[20];
	char wr_done_irq_name[20];
	char vdin2_meta_wr_done_irq_name[20];
	/* address offset(vdin0/vdin1/...) */
	unsigned int addr_offset;

	unsigned int unstable_flag;
	unsigned int wr_done_abnormal_cnt;
	unsigned int stamp;
	unsigned int hcnt64;
	unsigned int cycle;
	unsigned int start_time;/* ms vdin start time */
	int rdma_handle;
	/*for unreliable vsync interrupt check*/
	unsigned long long vs_time_stamp;
	unsigned int unreliable_vs_cnt;
	unsigned int unreliable_vs_cnt_pre;
	unsigned int unreliable_vs_idx;
	unsigned int unreliable_vs_time[10];

	bool cma_config_en;
	/*cma_config_flag:
	 *bit0: (1:share with codec_mm;0:cma alone)
	 *bit8: (1:discontinuous alloc way;0:continuous alloc way)
	 */
	unsigned int cma_config_flag;
#ifdef CONFIG_CMA
	struct platform_device	*this_pdev;
	struct page *venc_pages;
	unsigned int cma_mem_size;/*BYTE*/
	unsigned int cma_mem_alloc;
	/*cma_mem_mode:0:according to input size and output fmt;
	 **1:according to input size and output fmt force as YUV444
	 */
	unsigned int cma_mem_mode;
	unsigned int force_yuv444_malloc;
#endif
	/* bit0: enable/disable; bit4: luma range info */
	bool csc_cfg;
	/* duration of current timing */
	unsigned int duration;
	/* color-depth for vdin write */
	/*vdin write mem color depth support:
	 *bit0:support 8bit
	 *bit1:support 9bit
	 *bit2:support 10bit
	 *bit3:support 12bit
	 *bit4:support yuv422 10bit full pack mode (from txl new add)
	 */
	enum vdin_wr_color_depth color_depth_support;
	/*color depth config
	 *0:auto config as frontend
	 *8:force config as 8bit
	 *10:force config as 10bit
	 *12:force config as 12bit
	 */
	enum color_deeps_cfge color_depth_config;
	/* new add from txl:color depth mode for 10bit
	 *1: full pack mode;config 10bit as 10bit
	 *0: config 10bit as 12bit
	 */
	unsigned int full_pack;
	/* yuv422 malloc policy for vdin0 debug:
	 *1: force yuv422 memory alloc up to yuv444 10bit(2.5->4)
	 *0: use yuv422 memory alloc defautly
	 */
	unsigned int force_malloc_yuv_422_to_444;
	/* output_color_depth:
	 * when tv_input is 4k50hz_10bit or 4k60hz_10bit,
	 * choose output color depth from dts
	 */
	unsigned int output_color_depth;
	/* cutwindow config */
	bool cutwindow_cfg;
	bool auto_cutwindow_en;
	/*
	 *1:vdin out limit range
	 *0:vdin out full range
	 */
	unsigned int color_range_mode;
	/*
	 *game_mode:
	 *bit0:enable/disable
	 *bit1:for true bypas and put vframe in advance one vsync
	 *bit2:for true bypas and put vframe in advance two vsync,
	 *vdin & vpp read/write same buffer may happen
	 */
	unsigned int game_mode;
	unsigned int game_mode_pre;
	enum vdin_game_mode_chg_e game_mode_chg;
	/* game mode current state before ioctl change game mode */
	unsigned int game_mode_bak;
	int game_chg_drop_frame_cnt;
	unsigned int vrr_mode;
	unsigned int rdma_enable;
	/* afbce_mode: (amlogic frame buff compression encoder)
	 * 0: normal mode, not use afbce
	 * 1: use afbce non-mmu mode: head/body addr set by code
	 * 2: use afbce mmu mode: head set by code, body addr assigning by hw
	 */
	/*afbce_flag:
	 *bit[0]: enable afbce
	 *bit[1]: enable afbce_loosy
	 *bit[4]: afbce enable for 4k
	 *bit[5]: afbce enable for 1080p
	 *bit[6]: afbce enable for 720p
	 *bit[7]: afbce enable for other small resolution
	 */
	unsigned int afbce_flag;
	unsigned int afbce_mode_pre;
	unsigned int afbce_mode;
	unsigned int afbce_valid;

	unsigned int cfg_dma_buf;
	/*fot 'T correction' on projector*/
	unsigned int set_canvas_manual;
	unsigned int keystone_vframe_ready;
	struct vf_entry *keystone_entry[VDIN_CANVAS_MAX_CNT];
	unsigned int canvas_config_mode;
	bool prehsc_en;
	bool vshrk_en;
	bool urgent_en;
	bool double_wr_cfg;
	bool double_wr;
	bool double_wr_10bit_sup;
	bool black_bar_enable;
	bool hist_bar_enable;
	unsigned int ignore_frames;
	/*use frame rate to cal duraton*/
	unsigned int use_frame_rate;
	unsigned int irq_cnt;
	unsigned int vpu_crash_cnt;
	unsigned int frame_cnt;
	unsigned int puted_frame_cnt;
	unsigned int rdma_irq_cnt;
	unsigned int wr_done_irq_cnt;
	unsigned int meta_wr_done_irq_cnt;
	unsigned int vdin_irq_flag;
	unsigned int vdin_reset_flag;
	unsigned int vdin_dev_ssize;
	wait_queue_head_t queue;
	struct dentry *dbg_root;	/*dbg_fs*/

	/*atv non-std signal,force drop the field if previous already dropped*/
	unsigned int interlace_force_drop;
	unsigned int frame_drop_num;
	unsigned int skip_disp_md_check;
	unsigned int vframe_wr_en;
	unsigned int vframe_wr_en_pre;
	unsigned int pause_dec;

	/*signal change counter*/
	unsigned int sg_chg_afd_cnt;

	unsigned int matrix_pattern_mode;
	unsigned int pause_num;
	unsigned int hv_reverse_en;

	/*v4l interface ---- start*/
	enum vdin_work_mode_e work_mode;

	struct v4l2_device v4l2_dev;
	struct video_device vdev;
	struct vb2_queue vbqueue;
	struct v4l2_format v4lfmt;

	struct mutex lock;/*v4l lock*/
	struct mutex ioctrl_lock;/*vl2 ioctrol lock*/
	spinlock_t qlock; /*v4l qlock*/
	struct list_head buf_list;	/* buffer list head */
	struct vdin_vb_buff *cur_buff;	/* vdin video frame buffer */
	bool v4l_support_en;

	/*struct v4l2_fh fh;*/
	unsigned long vfmem_c_start[VDIN_CANVAS_MAX_CNT];/* Y/C non-contiguous mem */
	unsigned int dbg_v4l_pause;
	unsigned int dbg_v4l_no_vdin_ioctl;
	unsigned int dbg_v4l_no_vdin_event;
	struct vdin_set_canvas_addr_s st_vdin_set_canvas_addr[VDIN_CANVAS_MAX_CNT][VDIN_MAX_PLANES];
	bool vdin_set_canvas_flag;
	enum tvin_port_e v4l2_port_cur;
	unsigned int v4l2_port_num;
	enum tvin_port_e v4l2_port[VDIN_V4L2_INPUT_MAX];
	unsigned int afbce_flag_backup;
	unsigned int v4l2_req_buf_num;
	struct vdin_v4l2_s vdin_v4l2;
	//struct v4l2_ctrl_handler ctrl_handler;
	//struct v4l2_ctrl *p_ctrl;
	struct v4l2_ext_capture_capability_info ext_cap_cap_info;
	struct v4l2_ext_capture_plane_info ext_cap_plane_info;
	struct v4l2_ext_capture_video_win_info ext_cap_video_win_info;
	struct v4l2_ext_capture_freeze_mode ext_cap_freezee_mode;
	struct v4l2_ext_capture_plane_prop ext_cap_plane_prop;
	/*v4l interface ---- end*/

	unsigned int tx_fmt;
	unsigned int vd1_fmt;
	unsigned int dbg_force_stop_frame_num;
	unsigned int force_disp_skip_num;
	unsigned int dbg_dump_frames;
	unsigned int dbg_stop_dec_delay;
	unsigned int vinfo_std_duration; /* get vinfo fps value */
	unsigned int dbg_no_swap_en:1;
	unsigned int vdin_function_sel;
	unsigned int self_stop_start;
};

struct vdin_hist_s {
	ulong sum;
	int width;
	int height;
	int ave;
	unsigned short hist[64];
};

struct vdin_v4l2_param_s {
	int width;
	int height;
	int fps;
};

enum vdin_vrr_mode_e {
	VDIN_VRR_OFF = 0,
	VDIN_VRR_BASIC,
	VDIN_VRR_FREESYNC,
	VDIN_VRR_FREESYNC_PREMIUM,
	VDIN_VRR_FREESYNC_PREMIUM_PRO,
	VDIN_VRR_FREESYNC_PREMIUM_G_SYNC,
	VDIN_VRR_NUM
};

struct vdin_vrr_freesync_param_s {
	enum vdin_vrr_mode_e cur_vrr_status;
	u8 tone_mapping_en;
	u8 local_dimming_disable;
	u8 native_color_en;
};

extern unsigned int max_ignore_frame_cnt;
extern unsigned int skip_frame_debug;
extern unsigned int vdin_drop_cnt;
extern unsigned int vdin0_afbce_debug_force;
extern unsigned int vdin_dv_de_scramble;

struct vframe_provider_s *vf_get_provider_by_name(const char *provider_name);
extern bool enable_reset;
extern unsigned int dolby_size_byte;
extern unsigned int dv_dbg_mask;
extern u32 vdin_cfg_444_to_422_wmif_en;
extern unsigned int vdin_isr_monitor;
extern unsigned int vdin_get_prop_in_vs_en;
extern unsigned int vdin_prop_monitor;
extern unsigned int vdin_get_prop_in_fe_en;
extern struct vdin_hist_s vdin1_hist;

struct vframe_provider_s *vf_get_provider_by_name(const
						  char *provider_name);
char *vf_get_receiver_name(const char *provider_name);
int start_tvin_service(int no, struct vdin_parm_s *para);
int start_tvin_capture_ex(int dev_num, int port, struct vdin_parm_s  *para);
int stop_tvin_service(int no);
int vdin_reg_v4l2(struct vdin_v4l2_ops_s *v4l2_ops);
void vdin_unreg_v4l2(void);
int vdin_create_class_files(struct class *vdin_clsp);
void vdin_remove_class_files(struct class *vdin_clsp);
int vdin_create_device_files(struct device *dev);
void vdin_remove_device_files(struct device *dev);
int vdin_open_fe(enum tvin_port_e port, int index,
		 struct vdin_dev_s *devp);
void vdin_close_fe(struct vdin_dev_s *devp);
int vdin_start_dec(struct vdin_dev_s *devp);
void vdin_self_start_dec(struct vdin_dev_s *devp);
void vdin_stop_dec(struct vdin_dev_s *devp);
void vdin_self_stop_dec(struct vdin_dev_s *devp);
irqreturn_t vdin_isr_simple(int irq, void *dev_id);
irqreturn_t vdin_isr(int irq, void *dev_id);
irqreturn_t vdin_v4l2_isr(int irq, void *dev_id);
void ldim_get_matrix(int *data, int reg_sel);
void ldim_set_matrix(int *data, int reg_sel);
void tvafe_snow_config(unsigned int onoff);
void tvafe_snow_config_clamp(unsigned int onoff);
void vdin_vf_reg(struct vdin_dev_s *devp);
void vdin_vf_unreg(struct vdin_dev_s *devp);
void vdin_pause_dec(struct vdin_dev_s *devp);
void vdin_resume_dec(struct vdin_dev_s *devp);
bool is_dolby_vision_enable(void);

void vdin_debugfs_init(struct vdin_dev_s *vdevp);
void vdin_debugfs_exit(struct vdin_dev_s *vdevp);
void vdin_dump_frames(struct vdin_dev_s *devp);

bool vlock_get_phlock_flag(void);
bool vlock_get_vlock_flag(void);
bool frame_lock_vrr_lock_status(void);

u32 vlock_get_phase_en(u32 enc_idx);
void vdin_change_matrix0(u32 offset, u32 matrix_csc);
void vdin_change_matrix1(u32 offset, u32 matrix_csc);
void vdin_change_matrixhdr(u32 offset, u32 matrix_csc);

struct vdin_dev_s *vdin_get_dev(unsigned int index);
void vdin_mif_config_init(struct vdin_dev_s *devp);
void vdin_drop_frame_info(struct vdin_dev_s *devp, char *info);
int vdin_create_debug_files(struct device *dev);
void vdin_remove_debug_files(struct device *dev);
void vdin_vpu_dev_register(struct vdin_dev_s *vdevp);
void vdin_vpu_clk_gate_on_off(struct vdin_dev_s *vdevp, unsigned int on);
void vdin_vpu_clk_mem_pd(struct vdin_dev_s *vdevp, unsigned int on);
void vdin_afbce_vpu_clk_mem_pd(struct vdin_dev_s *vdevp, unsigned int on);

int vdin_v4l2_probe(struct platform_device *pdev,
		    struct vdin_dev_s *vdindevp);
int vdin_v4l2_if_isr(struct vdin_dev_s *pdev, struct vframe_s *vfp);
void vdin_frame_write_ctrl_set(struct vdin_dev_s *devp,
				struct vf_entry *vfe, bool rdma_en);
irqreturn_t vdin_write_done_isr(int irq, void *dev_id);
void vdin_game_mode_chg(struct vdin_dev_s *devp,
	unsigned int old_mode, unsigned int new_mode);
void vdin_frame_lock_check(struct vdin_dev_s *devp, int state);
void vdin_v4l2_init(struct vdin_dev_s *pdevp, struct platform_device *pldev);

#endif /* __TVIN_VDIN_DRV_H */

