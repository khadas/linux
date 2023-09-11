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

/* 20220124: vf->flag add pc mode flag to bypass ai_pq ai_sr */
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
/* 20220425: keystone function modify */
/* 20220513: add game mode lock debug */
/* 20220516: csc chg flash and dv chg not event */
/* 20220518: dv data abnormal cause reboot */
/* 20220519: dv register isr not rdma write */
/* 20220525: source-led switch to sink-led problem */
/* 20220602: 100 and 120 switch send event */
/* 20220608: t7 screenshot picture abnormal when width greater than vdin1_line_buff_size */
/* 20220609: add get low_latency and dv_vision */
/* 20220610: vdin add dump data debug */
/* 20220615: vdin1 crash addr when str */
/* 20220617: allm and vrr all come in not send vrr */
/* 20220618: use game mode global variable in vdin_isr cause abnormal and
 * fix video lag in old video path caused by unknown disp_mode flag
 */
/* 20220622: loopback implementation */
/* 20220628: send Freesync type */
/* 20220628: use one buffer in game mode 2 when vrr/freesync signal */
/* 20220701: use reality in and out fps check mode */
/* 20220706: probability not into vrr */
/* 20220714: set secure memory not send frame when keystone */
/* 20220719: up set not support port crash */
/* 20220726: add 444 format manual set */
/* 20220804: vdin1 crash addr modify */
/* 20220811: state machine optimization */
/* 20220829: get correct base framerate */
/* 20220830: ioctl node need add more protect */
/* 20220907: add manual_change_csc debug */
/* 20220915: dv not support manual set game mode */
/* 20220927: panel reverse not into game switch */
/* 20220930: 4k dv not afbce avoid afbce done */
/* 20221010: support buffer keeper */
/* 20221018: smr state machine optimization */
/* 20221021: s5 vdin bringup */
/* 20221028: rx calls interface notify information */
/* 20221029: update prop info */
/* 20221101: vdin1 crash when screen recording and keystone */
/* 20221103: change pc mode variable to local */
/* 20221109: interlace min buffer and 23.97 send event */
/* 20221118: add path not lock and filter frame rate */
/* 20221126: tvafe donot dynamically check aspect ratio */
/* 20221129: memory free and id name optimize */
/* 20221209: support aux screen capture */
/* 20221215: fix s5 loopback venc0 issue */
/* 20230105: overseas project display ration control */
/* 20230112: modify for vdin0 v4l2 */
/* 20230216: add address debug info */
/* 20230302: source clk change 25hz calculate to 24hz */
/* 20230307: if not data input not reminder */
/* 20230309: interlace signal not advance send frame */
/* 20230316: add avi ext_colorimetry callback api for vdin */
/* 20230321: not add vdin interrupt lock on tvin_notify_vdin_skip_frame */
/* 20230330: set whether to check the hdr sei error option */
/* 20230404: interlace not into game mode */
/* 20230417: provide uapi headers for user space */
/* 20230420: add osd only support for screencap */
/* 20230427: fix screen cap stress test crash issue */
/* 20230504: keystone interlace update dest value */
/* 20230526: 2560x1440 set bit to 8 */
/* 20230608: vdin not clear ratio_control value */
/* 20230710: bc302 filter unstable vsync and add debug */
/* 20230725: notify fps change event when not game mode */
/* 20230727: drop the first frame for vdin1 */
/* 20230803: pc and game mode switch optimization */
#define VDIN_VER "20230803: pc and game mode switch optimization"

//#define VDIN_BRINGUP_NO_VF
//#define VDIN_BRINGUP_NO_VLOCK
//#define VDIN_BRINGUP_NO_AML_VECM
//#define VDIN_BRINGUP_BYPASS_COLOR_CN_VT
#define K_FORCE_HV_SHRINK	0
#define VDIN_V4L2_INPUT_MAX	7
#define VDIN_DROP_FRAME_NUM_DEF	2

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
	 * tm2 vdin0/vdin1 all support up to 40k
	 */
	VDIN_HW_TM2,
	VDIN_HW_TM2_B,
	/*
	 * sc2, sm1 vdin0 upto 4k, vdin1 up to 1080P (write)
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
	VDIN_HW_S5,
};

/* 20230607: game mode optimize and add debug */
/* 20230609: add get vdin status */
/* 20230713: bc302 get field type */
/* 20230718: optimize get video format process */
/* 20230808: dv debug optimize */
#define VDIN_VER_V1 "20230808: dv debug optimize"

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
	bool vdin1_set_hdr;
	u32 vdin0_en;
	u32 vdin1_en;
	u32 vdin0_line_buff_size;
	u32 vdin1_line_buff_size;
	/* default is 4k */
	u32 vdin0_max_w_h;
};

/* #define VDIN_CRYSTAL               24000000 */
/* #define VDIN_MEAS_CLK              50000000 */
/* values of vdin_dev_t.flags */
#define VDIN_FLAG_NULL			0x00000000
#define VDIN_FLAG_DEC_INIT		0x00000001
#define VDIN_FLAG_DEC_STARTED		0x00000002
#define VDIN_FLAG_DEC_OPENED		0x00000004
#define VDIN_FLAG_DEC_REGISTERED	0x00000008
#define VDIN_FLAG_DEC_STOP_ISR		0x00000010
#define VDIN_FLAG_FORCE_UNSTABLE	0x00000020
#define VDIN_FLAG_FS_OPENED		0x00000100
#define VDIN_FLAG_SKIP_ISR		0x00000200
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
/* vdin_function_sel control bits start */
#define VDIN_SELF_STOP_START		BIT(0)
#define VDIN_V4L2_IOCTL_CHK		BIT(1)
#define VDIN_ADJUST_VLOCK		BIT(4)
#define VDIN_PROP_RX_UPDATE		BIT(5)
#define VDIN_GAME_NOT_TANSFER		BIT(6) //control for tx output when game mode
#define VDIN_FORCE_444_NOT_CONVERT	BIT(7) //commercial display control
#define VDIN_NO_TVAFE_ASPECT_RATIO_CHK	BIT(8) //no tvafe aspect_ratio check
#define VDIN_SET_DISPLAY_RATIO		BIT(9)
#define VDIN_NOT_DATA_INPUT_DROP	BIT(10)
#define VDIN_BYPASS_HDR_SEI_CHECK	BIT(11) //bypass non-standard hdr stream detection
#define VDIN_INTERLACE_GAME_MODE	BIT(12)
#define VDIN_SACALE_4096_2_3840		BIT(13)
#define VDIN_CUTWIN_4096_2_3840		BIT(14)
#define VDIN_MUX_VDIN0_HIST		BIT(15) //sel vdin0 hist for txhd2
#define VDIN_SET_PCS_RESET		BIT(16) //for report_active abnormal callback rx pcs_reset
#define VDIN_AFBCE_DOLBY		BIT(17)
#define VDIN_INTERLACE_DROP_BOTTOM	BIT(18)

/* vdin_function_sel control bits end */

#define VDIN_2K_SIZE			0x07800438 /* 0x780 = 1920 0x438 = 1080 */
#define VDIN_4K_SIZE			0x10000870 /* 0x1000 = 4096 0x870 = 2160 */

#define DURATION_VALUE_120_FPS		800
#define DURATION_VALUE_100_FPS		960
#define DURATION_VALUE_23_97_FPS	4004
// 23.97 Set this parameter to 23 to distinguish between 24
#define FPS_23_97_SET_TO_23		23
#define VDIN_SET_MODE_MAX_FRAME		60

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
	VDIN_WR_COLOR_DEPTH_10BIT_FULL_PACK_MODE = 0x10,
	VDIN_WR_COLOR_DEPTH_FORCE_MEM_YUV422_TO_YUV444 = 0x20,
};

#define VDIN_422_FULL_PK_EN			1
#define VDIN_422_FULL_PK_DIS			0

/* vdin afbce flag */
#define VDIN_AFBCE_EN                   (BIT0)
#define VDIN_AFBCE_EN_LOSSY             (BIT1)
#define VDIN_AFBCE_EN_4K                (BIT4)
#define VDIN_AFBCE_EN_1080P             (BIT5)
#define VDIN_AFBCE_EN_720P              (BIT6)
#define VDIN_AFBCE_EN_SMALL             (BIT7)

enum color_deeps_cfg_e {
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

#define VDIN_ISR_MONITOR_FLAG		BIT(0)
#define VDIN_ISR_MONITOR_EMP		BIT(1)
#define VDIN_ISR_MONITOR_RATIO		BIT(2)
#define VDIN_ISR_MONITOR_GAME		BIT(4)
#define VDIN_ISR_MONITOR_VS		BIT(5)
#define VDIN_ISR_MONITOR_VF		BIT(6)
#define VDIN_ISR_MONITOR_HDR_RAW_DATA	BIT(7)
#define VDIN_ISR_MONITOR_HDR_EMP_DATA	BIT(8)
#define VDIN_ISR_MONITOR_VRR_DATA	BIT(9)
#define VDIN_ISR_MONITOR_AFBCE		BIT(10)
#define VDIN_ISR_MONITOR_BUFFER		BIT(11)
#define VDIN_ISR_MONITOR_AFBCE_STA	BIT(12)
#define VDIN_ISR_MONITOR_WRITE_DONE	BIT(13)
#define VDIN_ISR_MONITOR_HDR_SEI_DATA	BIT(14)
#define VDIN_ISR_MONITOR_SCATTER_MEM	BIT(15)
#define DBG_RX_UPDATE_VDIN_PROP		BIT(20)

#define VDIN_DBG_CNTL_IOCTL	BIT(10)
#define VDIN_DBG_CNTL_FLUSH	BIT(11)

#define SEND_LAST_FRAME_GET_PROP	2

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
	VDIN_MATRIX_MAX,
};

enum vdin_matrix_sel_e {
	VDIN_SEL_MATRIX0 = 0,
	VDIN_SEL_MATRIX1,
	VDIN_SEL_MATRIX_HDR,/*after, equal g12a have*/
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

struct vdin_set_canvas_addr_s {
	long paddr;
	int  size;

	struct dma_buf *dma_buffer;
	struct dma_buf_attachment *dmabuf_attach;
	struct sg_table *sg_table;
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

/* debug control bits for scatter memory */
#define DBG_SCT_CTL_DIS			(BIT(0)) /* disable sct memory */
#define DBG_SCT_CTL_NO_FREE_TAIL	(BIT(1)) /* do not free tail */
#define DBG_SCT_CTL_NO_FREE_WR_LIST	(BIT(2)) /* do not free vf mem in wr list */

#define DBG_REG_LENGTH			10
#define DBG_REG_START_READ		(BIT(0))
#define DBG_REG_START_WRITE		(BIT(1))
#define DBG_REG_START_SET_BIT		(BIT(2))
#define DBG_REG_START_READ_WRITE	(BIT(3))
#define DBG_REG_ISR_READ		(BIT(4))
#define DBG_REG_ISR_WRITE		(BIT(5))
#define DBG_REG_ISR_SET_BIT		(BIT(6))
#define DBG_REG_ISR_READ_WRITE		(BIT(7))
#define DBG_REG_TOP_FUNCTION		(BIT(8))
#define DBG_REG_CONVERT_SYNC		(BIT(9))

/*******for debug **********/
struct vdin_debug_s {
	struct tvin_cutwin_s cutwin;
	unsigned int vdin_recycle_num;/* debug for vdin recycle frame by self */
	unsigned int dbg_print_cntl;/* debug for vdin print control */
	unsigned int dbg_pattern;
	unsigned int dbg_device_id;
	unsigned int dbg_sct_ctl;
	unsigned short scaling4h;/* for vertical scaling */
	unsigned short scaling4w;/* for horizontal scaling */
	unsigned short dest_cfmt;/* for color fmt conversion */
	unsigned short vdin1_line_buff;
	unsigned short manual_change_csc;
	unsigned short change_get_drm;
	unsigned char dbg_sel_mat; /* 0x10:mat0,0x11:mat1,0x12:hdr */
	/* vdin1 hdr set bypass */
	bool vdin1_set_hdr_bypass;
	bool dbg_force_shrink_en;
	bool force_pc_mode;
	bool force_game_mode;
	bool bypass_tunnel;
	bool pause_mif_dec;
	bool pause_afbce_dec;
	bool bypass_filter_vsync;
	unsigned int sar_width;
	unsigned int sar_height;
	unsigned int ratio_control;
	unsigned int dbg_rw_reg_en;
	unsigned int dbg_reg_addr[DBG_REG_LENGTH];
	unsigned int dbg_reg_val[DBG_REG_LENGTH];
	unsigned int dbg_reg_bit[DBG_REG_LENGTH];
};

struct vdin_dv_s {
	struct vframe_provider_s dv_vf_provider;
	struct tvin_dv_vsif_raw_s dv_vsif_raw;
	struct delayed_work dv_dwork;
	unsigned int dv_cur_index;
	unsigned int dv_next_index;
	unsigned int dolby_input;
	unsigned int dv_dma_size;
	dma_addr_t dv_dma_paddr;
	void *dv_dma_vaddr;
	void *temp_meta_data;
	dma_addr_t meta_data_raw_p_buffer0;/*for t7*/
	void *meta_data_raw_v_buffer0;/*for t7*/
	void *meta_data_raw_buffer1;/*for t7*/
	unsigned int dv_flag_cnt;/*cnt for no dv input*/
	u8 dv_flag; /*is signal dolby version 1:vsif 2:emp */
	bool dv_config;
	bool dv_path_idx;
	bool dv_crc_check;/*0:fail;1:ok*/
	unsigned int dv_mem_allocated;
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
	void *fm_table_vir_paddr[VDIN_CANVAS_MAX_CNT];
};

enum vdin_game_mode_chg_e {
	VDIN_GAME_MODE_UN_CHG = 0,
	VDIN_GAME_MODE_OFF_TO_ON,
	VDIN_GAME_MODE_ON_TO_OFF,
	VDIN_GAME_MODE_1_TO_2,
	VDIN_GAME_MODE_2_TO_1,
	VDIN_GAME_MODE_NUM
};

struct vdin_dts_config_s {
	/* whether check write done flag */
	bool chk_write_done_en;
	bool urgent_en;
	bool v4l_en;
	unsigned int afbce_flag_cfg;
};

struct vdin_s5_s {
	unsigned short h_scale_out;
	unsigned short v_scale_out;
};

struct vdin_v4l2_stat_s {
	/* frame drop due to framerate control */
	unsigned int drop_divide;
	unsigned int done_cnt;
	unsigned int que_cnt;
	unsigned int dque_cnt;
};

struct vdin_v4l2_dbg_ctl_s {
	unsigned int dbg_pix_fmt;
};

struct vdin_frame_stat_s {
	/* frame drop due to framerate control */
	unsigned int write_done_check;
	unsigned int afbce_abnormal_cnt;
	unsigned int afbce_normal_cnt;
	unsigned int wmif_abnormal_cnt;
	unsigned int wmif_normal_cnt;
	unsigned int wr_done_irq_cnt;
	unsigned int meta_wr_done_irq_cnt;
	/* Continuous cycle error counting */
	unsigned int cycle_err_cnt_con;
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
	unsigned int vrr_mode;
	/* vrr_en in frame_lock_policy */
	bool frame_lock_vrr_en;
	/* vrr states may changed only once and recovery quickly
	 * app may get wrong state if from prop.spd_data[5]
	 */
	u8 cur_spd_data5;
};

/* scatter start */
enum vdin_mem_type_e {
	VDIN_MEM_TYPE_CONTINUOUS = 0,
	VDIN_MEM_TYPE_SCT,
	VDIN_MEM_TYPE_MAX
};

struct vdin_msc_stat_s {
	unsigned int cur_page_cnt;

	unsigned int curr_tt_size;
	unsigned int max_size; /* max for one frame */
//	unsigned int sum_max_tt_size;
	unsigned int max_tt_size2;
	unsigned char curr_nub;
	unsigned char max_nub;
	unsigned char mts_pst_ready;
	unsigned char mts_pst_display;
	unsigned char mts_pst_back;
	unsigned char mts_pst_free;
	unsigned char mts_sct_rcc;
	unsigned char mts_sct_ready;
	unsigned char mts_sct_used;
};

/* vdin scatter memory */
struct vdin_msct_top_s {
	void *box;
	unsigned int max_buf_num;
	unsigned int mmu_4k_number; /* mmu 4k number in full size */
	unsigned int buffer_size_nub; /* 4k number per frame */
	unsigned int tail_cnt;
	bool	 sct_pause_dec; /* pause dec flag on sct mem */
	/* statistics info*/
	unsigned int que_work_cnt;
	unsigned int worker_run_cnt;

	struct mutex lock_ready; /* for sct ready */
	struct vdin_msc_stat_s sct_stat[VDIN_CANVAS_MAX_CNT];
	u64 sc_start_time;
	struct vf_entry *vfe;
};

/* scatter end */

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
	struct vframe_provider_s vf_provider;
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
	struct vdin_s5_s s5_data;

	 /* 0:from gpio A,1:from csi2 , 2:gpio B*/
	enum bt_path_e bt_path;
	const struct match_data_s *dtdata;
	struct timer_list timer;
	spinlock_t isr_lock;/*isr lock*/
	spinlock_t hist_lock;/*pq history lock*/
	struct mutex fe_lock;/*front end lock*/
	struct clk *msr_clk;
	unsigned int msr_clk_val;
	bool vdin_clk_flag;/*vdin on_off msr clk flag*/

	struct vdin_debug_s debug;
	enum vdin_format_convert_e format_convert;
	unsigned int mif_fmt;/*enum vdin_mif_fmt*/
	enum vdin_color_deeps_e source_bitdepth;
	enum vdin_matrix_csc_e csc_idx;
	struct vf_entry *curr_wr_vfe;
	struct vf_entry *last_wr_vfe;
	unsigned int vdin_delay_vfe2rd_list;
	unsigned int curr_field_type;
	unsigned int curr_dv_flag;
	unsigned int drop_hdr_set_sts;
	unsigned int starting_chg;
	u64 cur_us; //isr cur_us
	u64 pre_us; //isr pre_us

	char name[15];
	/* bit0 TVIN_PARM_FLAG_CAP bit31: TVIN_PARM_FLAG_WORK_ON */
	/* bit[1] 1:already TVIN_IOC_START_DEC OK 0:already TVIN_IOC_STOP_DEC OK*/
	/* bit[2] 1:already TVIN_IOC_OPEN OK 0:already TVIN_IOC_CLOSE OK*/
	unsigned int flags;
	unsigned int flags_isr;
	unsigned int index;
	unsigned int vdin_max_pixel_clk;

	unsigned long mem_start;
	unsigned int mem_size;
	unsigned long vf_mem_start[VDIN_CANVAS_MAX_CNT];
	struct page *vf_venc_pages[VDIN_CANVAS_MAX_CNT];
	struct codec_mm_s *vf_codec_mem[VDIN_CANVAS_MAX_CNT];

	/* save secure handle */
	unsigned int secure_handle;
	bool secure_en;
	bool mem_protected;
	unsigned int vf_mem_size;
	unsigned int vf_mem_size_small;/* double write use */
	unsigned int frame_size;
	unsigned int vf_mem_max_cnt;/*real buffer number*/
	unsigned int frame_buff_num;/*dts config data*/
	unsigned int frame_buff_num_bak;/* backup */
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
	unsigned int canvas_active_w;/* mif stride */
	unsigned int canvas_align_w;
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
	struct vdin_frame_stat_s stats;
	unsigned int stamp;
	unsigned int h_cnt64;
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
	enum color_deeps_cfg_e color_depth_config;
	/* new add from txl:color depth mode for 10bit
	 *1: full pack mode;config 10bit as 10bit
	 *0: config 10bit as 12bit
	 */
	unsigned int full_pack;
	/* yuv422 malloc policy for vdin0 debug:
	 *1: force yuv422 memory alloc up to yuv444 10bit(2.5->4)
	 *0: use yuv422 memory alloc by default
	 */
	unsigned int force_malloc_yuv_422_to_444;
	/* output_color_depth:
	 * when tv_input is 4k50hz_10bit or 4k60hz_10bit,
	 * choose output color depth from dts
	 */
	unsigned int output_color_depth;
	/* cut window config */
	bool cut_window_cfg;
	bool auto_cut_window_en;
	/*
	 *0:vdin out limit range
	 *1:vdin out full range
	 */
	unsigned int color_range_mode;
	/*
	 *game_mode:
	 *bit0:enable/disable
	 *bit1:for true bypass and put vframe in advance one vsync
	 *bit2:for true bypass and put vframe in advance two vsync,
	 *vdin & vpp read/write same buffer may happen
	 */
	unsigned int game_mode;
	/* game mode state before game change in ISR */
	unsigned int game_mode_pre;
	enum vdin_game_mode_chg_e game_mode_chg;
	/* game mode state before ioctl change game mode */
	unsigned int game_mode_bak;
	unsigned char af_num;/* for one buffer mode */

	int chg_drop_frame_cnt;
	unsigned int vdin_pc_mode;
	unsigned int vrr_mode;
	unsigned int vrr_on_add_cnt;
	unsigned int vrr_off_add_cnt;
	unsigned int rdma_enable;
	/* afbce_mode: (amlogic frame buff compression encoder)
	 * 0: normal mode, not use afbce
	 * 1: use afbce non-mmu mode: head/body addr set by code
	 * 2: use afbce mmu mode: head set by code, body addr assigning by hw
	 */
	/*afbce_flag:
	 *bit[0]: enable afbce
	 *bit[1]: enable afbce_lossy
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
	bool pre_h_scale_en;
	bool v_shrink_en;
	bool double_wr_cfg;
	bool double_wr;
	bool double_wr_10bit_sup;
	bool black_bar_enable;
	bool hist_bar_enable;
	bool rdma_not_register;
	bool interlace_drop_bottom;
	unsigned int ignore_frames;
	/*use frame rate to cal duration*/
	unsigned int use_frame_rate;
	unsigned int irq_cnt;
	unsigned int vpu_crash_cnt;
	unsigned int frame_cnt;
	unsigned int vdin_drop_num;
	unsigned int put_frame_cnt;
	unsigned int rdma_irq_cnt;
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
	unsigned int sg_chg_fps_cnt;

	unsigned int matrix_pattern_mode;
	unsigned int pause_num;
	unsigned int hv_reverse_en;

	/*v4l interface ---- start*/
	enum vdin_work_mode_e work_mode;

	struct v4l2_device v4l2_dev;
	struct video_device video_dev;
	struct vb2_queue vb_queue;
	struct v4l2_format v4l2_fmt;

	struct mutex lock;/*v4l lock*/
	//struct mutex ioctl_lock;/*vl2 io control lock*/
	spinlock_t list_head_lock; /*v4l2 list lock*/
	struct list_head buf_list;	/* buffer list head */
	struct vdin_vb_buff *cur_buff;	/* vdin video frame buffer */
	/*struct v4l2_fh fh;*/
	unsigned long vf_mem_c_start[VDIN_CANVAS_MAX_CNT];/* Y/C non-contiguous mem */
	unsigned int dbg_v4l_pause;
	unsigned int dbg_v4l_no_vdin_ioctl;
	unsigned int dbg_v4l_no_vdin_event;
	struct vdin_v4l2_dbg_ctl_s v4l2_dbg_ctl;
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

	/* scatter start */
	enum vdin_mem_type_e mem_type;
	struct work_struct	sct_work;
	struct workqueue_struct	*wq;
	struct vdin_msct_top_s	msct_top;
	/* scatter end */
	unsigned int tx_fmt;
	unsigned int vd1_fmt;
	unsigned int dbg_force_stop_frame_num;
	unsigned int force_disp_skip_num;
	unsigned int dbg_dump_frames;
	unsigned int dbg_stop_dec_delay;
	unsigned int vinfo_std_duration; /* get vinfo out fps value */
	unsigned int vdin_std_duration; /* get in fps value */
	unsigned int dbg_no_swap_en:1;
	/* 0:auto;1:force on;2:force off */
	unsigned int dbg_force_one_buffer;
	unsigned int dbg_afbce_monitor:8;
	unsigned int dbg_no_wr_check:1;/* disable write done check */
	unsigned int dbg_fr_ctl:7;/* disable write done check */
	unsigned int vdin_drop_ctl_cnt;/* drop frames casued by dbg_drop_ctl */
	unsigned int vdin_function_sel;
	unsigned int self_stop_start;
	unsigned int quit_flag;
	unsigned int vdin_stable_cnt;
	struct task_struct *kthread;
	struct semaphore sem;
	struct vf_entry *vfe_tmp;
	struct vdin_dts_config_s dts_config;
	unsigned int common_divisor;
	unsigned int vrr_frame_rate_min;
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
int start_tvin_capture_ex(int dev_num, enum port_vpp_e port, struct vdin_parm_s  *para);
int stop_tvin_service(int no);
int vdin_reg_v4l2(struct vdin_v4l2_ops_s *v4l2_ops);
void vdin_unregister_v4l2(void);
int vdin_create_class_files(struct class *vdin_class);
void vdin_remove_class_files(struct class *vdin_class);
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
void tvafe_snow_config(unsigned int on_off);
void tvafe_snow_config_clamp(unsigned int on_off);
void vdin_vf_reg(struct vdin_dev_s *devp);
void vdin_vf_unreg(struct vdin_dev_s *devp);
void vdin_pause_dec(struct vdin_dev_s *devp, unsigned int type);
void vdin_resume_dec(struct vdin_dev_s *devp, unsigned int type);
bool is_amdv_enable(void);

void vdin_debugfs_init(struct vdin_dev_s *devp);
void vdin_debugfs_exit(struct vdin_dev_s *devp);
void vdin_dump_frames(struct vdin_dev_s *devp);
void vdin_dbg_access_reg(struct vdin_dev_s *devp, unsigned int update_site);

bool vlock_get_phlock_flag(void);
bool vlock_get_vlock_flag(void);
bool frame_lock_vrr_lock_status(void);

u32 vlock_get_phase_en(u32 enc_idx);
void vdin_change_matrix0(u32 offset, u32 matrix_csc);
void vdin_change_matrix1(u32 offset, u32 matrix_csc);
void vdin_change_matrix_hdr(u32 offset, u32 matrix_csc);

struct vdin_dev_s *vdin_get_dev(unsigned int index);
void vdin_mif_config_init(struct vdin_dev_s *devp);
void vdin_drop_frame_info(struct vdin_dev_s *devp, char *info);
int vdin_create_debug_files(struct device *dev);
void vdin_remove_debug_files(struct device *dev);
void vdin_vpu_dev_register(struct vdin_dev_s *devp);
void vdin_vpu_clk_gate_on_off(struct vdin_dev_s *devp, unsigned int on);
void vdin_vpu_clk_mem_pd(struct vdin_dev_s *devp, unsigned int on);
void vdin_afbce_vpu_clk_mem_pd(struct vdin_dev_s *devp, unsigned int on);

int vdin_v4l2_probe(struct platform_device *pdev,
		    struct vdin_dev_s *devp);
int vdin_v4l2_if_isr(struct vdin_dev_s *pdev, struct vframe_s *vfp);
void vdin_frame_write_ctrl_set(struct vdin_dev_s *devp,
				struct vf_entry *vfe, bool rdma_en);
irqreturn_t vdin_write_done_isr(int irq, void *dev_id);
void vdin_game_mode_chg(struct vdin_dev_s *devp,
	unsigned int old_mode, unsigned int new_mode);
void vdin_frame_lock_check(struct vdin_dev_s *devp, int state);
void vdin_v4l2_init(struct vdin_dev_s *devp, struct platform_device *pl_dev);
int vdin_afbce_compression_ratio_monitor(struct vdin_dev_s *devp, struct vf_entry *vfe);
void vdin_pause_hw_write(struct vdin_dev_s *devp, bool rdma_en);
void vdin_reg_dmc_notifier(unsigned int index);
void vdin_unreg_dmc_notifier(unsigned int index);
#endif /* __TVIN_VDIN_DRV_H */

