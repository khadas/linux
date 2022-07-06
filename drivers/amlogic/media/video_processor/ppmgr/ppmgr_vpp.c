// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * drivers/amlogic/media/video_processor/ppmgr/ppmgr_vpp.c
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

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/platform_device.h>
/*#include <mach/am_regs.h>*/
#ifdef CONFIG_AMLOGIC_MEDIA_FRAME_SYNC
#include <linux/amlogic/media/frame_sync/timestamp.h>
#include <linux/amlogic/media/frame_sync/ptsserv.h>
#endif
#include <linux/amlogic/media/canvas/canvas.h>
#include <linux/amlogic/media/canvas/canvas_mgr.h>
#include <linux/amlogic/media/vout/vinfo.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/media/vfm/vframe.h>
/*#include <linux/amlogic/amports/vfp.h>*/
#include "vfp.h"
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>
#include <linux/amlogic/media/utils/amlog.h>
/* media module used media/registers/cpu_version.h since kernel 5.4 */
#include <linux/amlogic/media/registers/cpu_version.h>
/*#include <linux/amlogic/ge2d/ge2d_main.h>*/
#include <linux/amlogic/media/ge2d/ge2d.h>
#include <linux/kthread.h>
#include <linux/delay.h>
//#include <linux/semaphore.h>
#include <linux/sched/rt.h>
#include "ppmgr_log.h"
#include "ppmgr_pri.h"
#include "ppmgr_dev.h"
#include <linux/mm.h>
#include <linux/amlogic/media/ppmgr/ppmgr.h>
#include <linux/amlogic/media/ppmgr/ppmgr_status.h>
/*#include "../amports/video.h"*/
#ifdef CONFIG_AMLOGIC_MEDIA_VIDEO
#include <linux/amlogic/media/video_sink/video.h>
#endif
/*#include "../amports/vdec_reg.h"*/
#include <linux/amlogic/media/utils/vdec_reg.h>
/*#include "../display/osd/osd_reg.h"*/
#include "../../osd/osd_reg.h"
#include "../../common/vfm/vfm.h"
#include "ppmgr_vpp.h"
#ifdef CONFIG_AMLOGIC_MEDIA_CODEC_MM
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#endif
#include <linux/dma-mapping.h>
#include <linux/dma-contiguous.h>
#include <linux/amlogic/media/ppmgr/tbff.h>
#include <uapi/linux/sched/types.h>
/*#if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON6*/
/*#include <mach/mod_gate.h>*/
/*#endif*/

#define PPMGRVPP_INFO(fmt, args...) pr_info("PPMGRVPP: info: " fmt " ", ## args)
#define PPMGRVPP_DBG(fmt, args...) pr_debug("PPMGRVPP: dbg: " fmt " ", ## args)
#define PPMGRVPP_WARN(fmt, args...) pr_warn("PPMGRVPP: warn: " fmt " ", ## args)
#define PPMGRVPP_ERR(fmt, args...) pr_err("PPMGRVPP: err: " fmt " ", ## args)

#define VF_POOL_SIZE 10

#define PPMGR_TB_DETECT
/* #define CANVAS_RESERVED */

#define RECEIVER_NAME "ppmgr"
#define PROVIDER_NAME   "ppmgr"

#define MM_ALLOC_SIZE (SZ_16M + SZ_4M + SZ_2M)
#define MAX_WIDTH  1024
#define MAX_HEIGHT 576
#define THREAD_INTERRUPT 0
#define THREAD_RUNNING 1
#define INTERLACE_DROP_MODE 1
#define enableVideoLayer() \
		SET_MPEG_REG_MASK(VPP_MISC, \
				  VPP_VD1_PREBLEND \
				  | VPP_PREBLEND_EN \
				  | VPP_VD1_POSTBLEND)

#define disableVideoLayer() \
		CLEAR_MPEG_REG_MASK(VPP_MISC, \
				    VPP_VD1_PREBLEND | VPP_VD1_POSTBLEND)

#define disableVideoLayer_PREBELEND() \
		CLEAR_MPEG_REG_MASK(VPP_MISC, \
				    VPP_VD1_PREBLEND)

static int ass_index = -1;

static DEFINE_SPINLOCK(lock);
static bool ppmgr_blocking;
static bool ppmgr_inited;
static int ppmgr_buffer_status;
static struct ppframe_s vfp_pool[VF_POOL_SIZE];
static struct vframe_s *vfp_pool_free[VF_POOL_SIZE + 1];
static struct vframe_s *vfp_pool_ready[VF_POOL_SIZE + 1];
struct buf_status_s {
	int index;
	int dirty;
};

static struct buf_status_s buf_status[VF_POOL_SIZE];

struct vfq_s q_ready, q_free;
//static struct semaphore thread_sem;
static DEFINE_MUTEX(ppmgr_mutex);
static bool ppmgr_quit_flag;

#ifdef PPMGR_TB_DETECT
#define TB_DETECT_BUFFER_MAX_SIZE 16
#define TB_DETECT_W 128
#define TB_DETECT_H 96

struct tb_buf_s {
	ulong vaddr;
	ulong paddr;
};

enum tb_status {
	tb_idle,
	tb_running,
	tb_done,
};

static DEFINE_MUTEX(tb_mutex);

static struct tb_buf_s detect_buf[TB_DETECT_BUFFER_MAX_SIZE];
static struct task_struct *tb_detect_task;
static int tb_task_running;
//static struct semaphore tb_sem;
static atomic_t detect_status;
static atomic_t tb_detect_flag;
static u8 tb_last_flag;
static u32 tb_buff_wptr;
static u32 tb_buff_rptr;
static s8 tb_buffer_status;
static u32 tb_buffer_start;
static u32 tb_buffer_size;
static u8 tb_first_frame_type;
static u32 tb_buffer_len = TB_DETECT_BUFFER_MAX_SIZE;
static atomic_t tb_reset_flag;
static u32 tb_init_mute;
static atomic_t tb_skip_flag;
static atomic_t tb_run_flag;
static bool tb_quit_flag;
static struct TB_DetectFuncPtr *gfunc;
static int tb_buffer_init(void);
static int tb_buffer_uninit(void);
#endif
static s32 ppmgr_src_canvas[3] = {-1, -1, -1};
static s32 ppmgr_dst_canvas[2] = {-1, -1};
static s8 local_canvas_status;
static bool need_data_notify;
static int dumpfirstframe;
static int count_scr;
static int count_dst;

static unsigned int ppmgr_color_format;
MODULE_PARM_DESC(ppmgr_color_format, "\n ppmgr_color_format\n");
module_param(ppmgr_color_format, uint, 0664);

const struct vframe_receiver_op_s *vf_ppmgr_reg_provider(void);

/* void vf_ppmgr_unreg_provider(void);
 * void vf_ppmgr_reset(int type);
 * void ppmgr_vf_put_dec(struct vframe_s *vf);
 */

/* extern u32 timestamp_pcrscr_enable_state(void); */

bool is_valid_ppframe(struct ppframe_s *pp_vf)
{
	return ((pp_vf >= &vfp_pool[0]) && (pp_vf <= &vfp_pool[VF_POOL_SIZE - 1]));
}

/* ***********************************************
 *
 *   Canvas helpers.
 *
 * ************************************************
 */

#include "decontour.h"

#define DCNTR_POOL_SIZE VF_POOL_SIZE

static s8 ppmgr_check_tvp_state(void)
{
	struct provider_state_req_s req;
	s8 ret = -1;
	char *provider_name = vf_get_provider_name(PROVIDER_NAME);

	while (provider_name) {
		if (!vf_get_provider_name(provider_name))
			break;
		provider_name =
			vf_get_provider_name(provider_name);
	}
	if (provider_name) {
		req.vf = NULL;
		req.req_type = REQ_STATE_SECURE;
		req.req_result[0] = 0xffffffff;
		vf_notify_provider_by_name(provider_name,
			VFRAME_EVENT_RECEIVER_REQ_STATE,
			(void *)&req);
		if (req.req_result[0] == 0)
			ret = 0;
		else if (req.req_result[0] != 0xffffffff)
			ret = 1;
	}
	return ret;
}

#define Rd(adr) aml_read_vcbus(adr)
#define Wr(adr, val) aml_write_vcbus(adr, val)

struct dcntr_mem_s dcntr_mem_info[DCNTR_POOL_SIZE];
struct dcntr_mem_s dcntr_mem_info_last;

static struct completion isr_done;
static u32 last_w;
static u32 last_h;
static u32 last_type;

#define DCT_PRINT_INFO       0X1
#define DCT_BYPASS           0X2
#define DCT_USE_DS_SCALE     0X4
#define DCT_DUMP_REG         0X8
#define DCT_PROCESS_I        0X10
#define DCT_REWRITE_REG      0X20

void wr_bits(unsigned int adr,
	unsigned int val,
	unsigned int start,
	unsigned int len)
{
	unsigned int mask = ((1 << len) - 1) << start;

	return aml_vcbus_update_bits(adr, mask, val << start);
}

int dc_print(const char *fmt, ...)
{
	if (ppmgr_device.debug_decontour & DCT_PRINT_INFO) {
		char buf[256];
		int len = 0;
		va_list args;

		va_start(args, fmt);
		len = sprintf(buf, "dc:[%d]", 0);
		vsnprintf(buf + len, 256 - len, fmt, args);
		pr_info("%s", buf);
		va_end(args);
	}
	return 0;
}

static bool is_decontour_supported(void)
{
	bool ret = false;

	if (ppmgr_device.reg_dct_irq_success)
		ret = true;

	return ret;
}

static void decontour_init(void)
{
	int mem_flags;
	int grd_size = SZ_512K;             /*(81*8*16 byte)*45 = 455K*/
	int yds_size = SZ_512K + SZ_32K;    /*960 * 576 = 540K*/
	int cds_size = yds_size / 2;        /*960 * 576 / 2= 270K*/
	int total_size = grd_size + yds_size + cds_size;
	int buffer_count = DCNTR_POOL_SIZE;
	int i;
	bool is_tvp = false;

	if (ppmgr_device.decontour_addr)
		return;

	mem_flags = CODEC_MM_FLAGS_DMA | CODEC_MM_FLAGS_CMA_CLEAR;
	if (ppmgr_check_tvp_state() == 1) {
		mem_flags |= CODEC_MM_FLAGS_TVP;
		is_tvp = true;
	}
	ppmgr_device.decontour_addr = codec_mm_alloc_for_dma("ppmgr-decontour",
				buffer_count * total_size / PAGE_SIZE, 0, mem_flags);
	if (ppmgr_device.decontour_addr == 0)
		pr_err("decontour: alloc fail\n");

	dc_print("decontour:alloc success %d, is_tvp=%d\n",
		 ppmgr_device.decontour_addr, is_tvp);

	for (i = 0; i < DCNTR_POOL_SIZE; i++) {
		dcntr_mem_info[i].index = i;
		dcntr_mem_info[i].grd_addr = 0;
		dcntr_mem_info[i].yds_addr = 0;
		dcntr_mem_info[i].cds_addr = 0;
		dcntr_mem_info[i].grd_size = 0;
		dcntr_mem_info[i].yds_size = 0;
		dcntr_mem_info[i].cds_size = 0;
		dcntr_mem_info[i].yds_canvas_mode = 0;
		dcntr_mem_info[i].yds_canvas_mode = 0;
		dcntr_mem_info[i].ds_ratio = 0;
		dcntr_mem_info[i].free = false;
		dcntr_mem_info[i].use_org = true;
		dcntr_mem_info[i].pre_out_fmt = VIDTYPE_VIU_NV21;
		dcntr_mem_info[i].grd_swap_64bit = false;
		dcntr_mem_info[i].yds_swap_64bit = false;
		dcntr_mem_info[i].cds_swap_64bit = false;
		dcntr_mem_info[i].grd_little_endian = true;
		dcntr_mem_info[i].yds_little_endian = true;
		dcntr_mem_info[i].cds_little_endian = true;
	}

	for (i = 0; i < buffer_count; i++) {
		dcntr_mem_info[i].grd_addr = ppmgr_device.decontour_addr + i * total_size;
		dcntr_mem_info[i].yds_addr = dcntr_mem_info[i].grd_addr + grd_size;
		dcntr_mem_info[i].cds_addr = dcntr_mem_info[i].yds_addr + yds_size;
		dcntr_mem_info[i].grd_size = grd_size;
		dcntr_mem_info[i].yds_size = yds_size;
		dcntr_mem_info[i].cds_size = cds_size;
		dcntr_mem_info[i].free = true;

		dc_print("i=%d, grd_addr=%lx, yds_addr=%lx,cds_addr=%lx, %d, %d, %d\n",
			i,
			dcntr_mem_info[i].grd_addr,
			dcntr_mem_info[i].yds_addr,
			dcntr_mem_info[i].cds_addr,
			grd_size,
			yds_size,
			cds_size);
	}
	init_completion(&isr_done);
	last_w = 0;
	last_h = 0;
	last_type = 0;
	dc_print("decontour: init\n");
}

static void decontour_buf_reset(void)
{
	int i;

	if (ppmgr_device.decontour_addr == 0)
		return;

	pr_info("decontour buf reset\n");
	for (i = 0; i < DCNTR_POOL_SIZE; i++)
		dcntr_mem_info[i].free = true;
}

static void decontour_uninit(void)
{
	if (ppmgr_device.decontour_addr)
		codec_mm_free_for_dma("ppmgr-decontour",
			ppmgr_device.decontour_addr);
	ppmgr_device.decontour_addr = 0;
	dc_print("decontour: uninit\n");
}

irqreturn_t decontour_pre_isr(int irq, void *dev_id)
{
	complete(&isr_done);
	dc_print("decontour: isr\n");
	return IRQ_HANDLED;
}

static void decontour_dump_reg(void)
{
	u32 value;
	int i;

	for (i = 0x4a00; i < 0x4a12; i++) {
		value = Rd(i);
		pr_info("reg=%x, value= %x\n", i, value);
	}
	value = Rd(DCNTR_GRD_WMIF_CTRL1);
	dc_print("cds: DCNTR_GRD_WMIF_CTRL1: %x\n", value);
	value = Rd(DCNTR_GRD_WMIF_CTRL3);
	dc_print("cds: DCNTR_GRD_WMIF_CTRL3: %x\n", value);
	value = Rd(DCNTR_GRD_WMIF_CTRL4);
	dc_print("cds: DCNTR_GRD_WMIF_CTRL4: %x\n", value);
	value = Rd(DCNTR_GRD_WMIF_SCOPE_X);
	dc_print("cds: DCNTR_GRD_WMIF_SCOPE_X: %x\n", value);
	value = Rd(DCNTR_GRD_WMIF_SCOPE_Y);
	dc_print("cds: DCNTR_GRD_WMIF_SCOPE_Y: %x\n", value);

	value = Rd(DCNTR_YDS_WMIF_CTRL1);
	dc_print("grd: DCNTR_YDS_WMIF_CTRL1: %x\n", value);
	value = Rd(DCNTR_YDS_WMIF_CTRL3);
	dc_print("grd: DCNTR_YDS_WMIF_CTRL3: %x\n", value);
	value = Rd(DCNTR_YDS_WMIF_CTRL4);
	dc_print("grd: DCNTR_YDS_WMIF_CTRL4: %x\n", value);
	value = Rd(DCNTR_YDS_WMIF_SCOPE_X);
	dc_print("grd: DCNTR_YDS_WMIF_SCOPE_X: %x\n", value);
	value = Rd(DCNTR_YDS_WMIF_SCOPE_Y);
	dc_print("grd: DCNTR_YDS_WMIF_SCOPE_Y: %x\n", value);

	value = Rd(DCNTR_CDS_WMIF_CTRL1);
	dc_print("yds: DCNTR_CDS_WMIF_CTRL1: %x\n", value);
	value = Rd(DCNTR_CDS_WMIF_CTRL3);
	dc_print("yds: DCNTR_CDS_WMIF_CTRL3: %x\n", value);
	value = Rd(DCNTR_CDS_WMIF_CTRL4);
	dc_print("yds: DCNTR_CDS_WMIF_CTRL4: %x\n", value);
	value = Rd(DCNTR_CDS_WMIF_SCOPE_X);
	dc_print("yds: DCNTR_CDS_WMIF_SCOPE_X: %x\n", value);
	value = Rd(DCNTR_CDS_WMIF_SCOPE_Y);
	dc_print("yds: DCNTR_CDS_WMIF_SCOPE_Y: %x\n", value);

	value = Rd(DCNTR_PRE_ARB_MODE);
	dc_print("DCNTR_PRE_ARB_MODE: %x\n", value);
	value = Rd(DCNTR_PRE_ARB_REQEN_SLV);
	dc_print("DCNTR_PRE_ARB_REQEN_SLV: %x\n", value);
	value = Rd(DCNTR_PRE_ARB_WEIGH0_SLV);
	dc_print("DCNTR_PRE_ARB_WEIGH0_SLV: %x\n", value);
	value = Rd(DCNTR_PRE_ARB_WEIGH1_SLV);
	dc_print("DCNTR_PRE_ARB_WEIGH1_SLV: %x\n", value);
	value = Rd(DCNTR_PRE_ARB_UGT);
	dc_print("DCNTR_PRE_ARB_UGT: %x\n", value);
	value = Rd(DCNTR_PRE_ARB_LIMT0);
	dc_print("DCNTR_PRE_ARB_LIMT0: %x\n", value);

	value = Rd(DCNTR_PRE_ARB_STATUS);
	dc_print("DCNTR_PRE_ARB_STATUS: %x\n", value);
	value = Rd(DCNTR_PRE_ARB_DBG_CTRL);
	dc_print("DCNTR_PRE_ARB_DBG_CTRL: %x\n", value);
	value = Rd(DCNTR_PRE_ARB_PROT);
	dc_print("DCNTR_PRE_ARB_PROT: %x\n", value);

	value = Rd(DCTR_BGRID_TOP_FSIZE);
	dc_print("DCTR_BGRID_TOP_FSIZE: %x\n", value);
	value = Rd(DCTR_BGRID_TOP_HDNUM);
	dc_print("DCTR_BGRID_TOP_HDNUM: %x\n", value);
	value = Rd(DCTR_BGRID_TOP_CTRL0);
	dc_print("DCTR_BGRID_TOP_CTRL0: %x\n", value);
	value = Rd(DCTR_BGRID_TOP_FMT);
	dc_print("DCTR_BGRID_TOP_FMT: %x\n", value);
	value = Rd(DCTR_BGRID_TOP_GCLK);
	dc_print("DCTR_BGRID_TOP_GCLK: %x\n", value);
	value = Rd(DCTR_BGRID_TOP_HOLD);
	dc_print("DCTR_BGRID_TOP_HOLD: %x\n", value);

	value = Rd(DCNTR_GRID_GEN_REG); //grd_num_use
	dc_print("decontour: DCNTR_GRID_GEN_REG=%x\n", value);
	value = Rd(DCNTR_GRID_GEN_REG2); //grd_num_use
	dc_print("decontour: DCNTR_GRID_GEN_REG2=%x\n", value);
	value = Rd(DCNTR_GRID_CANVAS0); //grd_num_use
	dc_print("decontour: DCNTR_GRID_CANVAS0=%x\n", value);
	value = Rd(DCNTR_GRID_LUMA_X0); //grd_num_use
	dc_print("decontour: DCNTR_GRID_LUMA_X0=%x\n", value);
	value = Rd(DCNTR_GRID_LUMA_Y0); //grd_num_use
	dc_print("decontour: DCNTR_GRID_LUMA_Y0=%x\n", value);
	value = Rd(DCNTR_GRID_RPT_LOOP); //grd_num_use
	dc_print("decontour: DCNTR_GRID_RPT_LOOP=%x\n", value);
	value = Rd(DCNTR_GRID_LUMA0_RPT_PAT); //grd_num_use
	dc_print("decontour: DCNTR_GRID_LUMA0_RPT_PAT=%x\n", value);
	value = Rd(DCNTR_GRID_CHROMA0_RPT_PAT); //grd_num_use
	dc_print("decontour: DCNTR_GRID_CHROMA0_RPT_PAT=%x\n", value);
	value = Rd(DCNTR_GRID_DUMMY_PIXEL); //grd_num_use
	dc_print("decontour: DCNTR_GRID_DUMMY_PIXEL=%x\n", value);

	value = Rd(DCNTR_GRID_LUMA_FIFO_SIZE); //grd_num_use
	dc_print("decontour: DCNTR_GRID_LUMA_FIFO_SIZE=%x\n", value);

	value = Rd(DCNTR_GRID_RANGE_MAP_Y); //grd_num_use
	dc_print("decontour: DCNTR_GRID_RANGE_MAP_Y=%x\n", value);
	value = Rd(DCNTR_GRID_RANGE_MAP_CB); //grd_num_use
	dc_print("decontour: DCNTR_GRID_RANGE_MAP_CB=%x\n", value);
	value = Rd(DCNTR_GRID_RANGE_MAP_CR); //grd_num_use
	dc_print("decontour: DCNTR_GRID_RANGE_MAP_CR=%x\n", value);
	value = Rd(DCNTR_GRID_URGENT_CTRL); //grd_num_use
	dc_print("decontour: DCNTR_GRID_URGENT_CTRL=%x\n", value);

	value = Rd(DCNTR_GRID_GEN_REG3); //grd_num_use
	dc_print("decontour: DCNTR_GRID_GEN_REG3=%x\n", value);
	value = Rd(DCNTR_GRID_AXI_CMD_CNT); //grd_num_use
	dc_print("decontour: DCNTR_GRID_AXI_CMD_CNT=%x\n", value);
	value = Rd(DCNTR_GRID_AXI_RDAT_CNT); //grd_num_use
	dc_print("decontour: DCNTR_GRID_AXI_RDAT_CNT=%x\n", value);
	value = Rd(DCNTR_GRID_FMT_CTRL); //grd_num_use
	dc_print("decontour: DCNTR_GRID_FMT_CTRL=%x\n", value);
	value = Rd(DCNTR_GRID_FMT_W); //grd_num_use
	dc_print("decontour: DCNTR_GRID_FMT_W=%x\n", value);

	if (get_cpu_type() <= MESON_CPU_MAJOR_ID_T5)
		return;

	value = Rd(DCNTR_GRID_BADDR_Y);
	dc_print("decontour: DCNTR_GRID_BADDR_Y=%x\n", value);
	value = Rd(DCNTR_GRID_BADDR_CB);
	dc_print("decontour: DCNTR_GRID_BADDR_CB=%x\n", value);
	value = Rd(DCNTR_GRID_BADDR_CR);
	dc_print("decontour: DCNTR_GRID_BADDR_CR=%x\n", value);

	value = Rd(DCNTR_GRID_STRIDE_0);
	dc_print("decontour: DCNTR_GRID_STRIDE_0=%x\n", value);
	value = Rd(DCNTR_GRID_STRIDE_1);
	dc_print("decontour: DCNTR_GRID_STRIDE_1=%x\n", value);

	value = Rd(DCNTR_GRID_BADDR_Y_F1);
	dc_print("decontour: DCNTR_GRID_BADDR_Y_F1=%x\n", value);
	value = Rd(DCNTR_GRID_BADDR_CB_F1);
	dc_print("decontour: DCNTR_GRID_BADDR_CB_F1=%x\n", value);
	value = Rd(DCNTR_GRID_BADDR_CR_F1);
	dc_print("decontour: DCNTR_GRID_BADDR_CR_F1=%x\n", value);
	value = Rd(DCNTR_GRID_STRIDE_0_F1);
	dc_print("decontour: DCNTR_GRID_STRIDE_0_F1=%x\n", value);
	value = Rd(DCNTR_GRID_STRIDE_1_F1);
	dc_print("decontour: DCNTR_GRID_STRIDE_1_F1=%x\n", value);
}

static void decontour_print_parm(struct dcntr_mem_s *dcntr_mem)
{
	dc_print("i=%d,grd_addr=%lx,y_addr=%lx,c_addr=%lx,g_size=%d,y_size=%d,c_size=%d,ratio=%d\n",
		dcntr_mem->index,
		dcntr_mem->grd_addr,
		dcntr_mem->yds_addr,
		dcntr_mem->cds_addr,
		dcntr_mem->grd_size,
		dcntr_mem->yds_size,
		dcntr_mem->cds_size,
		dcntr_mem->ds_ratio);
	dc_print("pre_out_fmt=%d,yflt_wrmif_length=%d,cflt_wrmif_length=%d,use_org=%d\n",
		dcntr_mem->pre_out_fmt,
		dcntr_mem->yflt_wrmif_length,
		dcntr_mem->cflt_wrmif_length,
		dcntr_mem->use_org);

	dc_print("grd_swap=%d,yds_swap=%x,cds_swap=%x,grd_endian=%x,yds_endian=%d,cds_endian=%dn",
		dcntr_mem->grd_swap_64bit,
		dcntr_mem->yds_swap_64bit,
		dcntr_mem->cds_swap_64bit,
		dcntr_mem->grd_little_endian,
		dcntr_mem->yds_little_endian,
		dcntr_mem->cds_little_endian);

	dc_print("yds_canvas_mode=%d,cds_canvas_mode=%d\n",
		dcntr_mem->yds_canvas_mode,
		dcntr_mem->cds_canvas_mode);
}

static int decontour_dump_output(u32 w, u32 h, struct dcntr_mem_s *dcntr_mem, int skip)
{
	struct file *fp;
	mm_segment_t fs;
	loff_t pos;
	char name_buf[32];
	int write_size;
	u8 *data;

	if (w == 0 || h == 0)
		return -7;
	write_size = dcntr_mem->grd_size;
	dc_print("addr =%lx, size=%d\n", dcntr_mem->grd_addr, write_size);
	snprintf(name_buf, sizeof(name_buf), "/data/tmp/%d-%d.grid",
		w, h);
	fp = filp_open(name_buf, O_CREAT | O_RDWR, 0644);

	if (IS_ERR(fp))
		return -1;
	dc_print("decontour:dump_2: name_buf=%s\n", name_buf);
	data = codec_mm_vmap(dcntr_mem->grd_addr, write_size);
	dc_print("(ulong)data =%lx\n", (ulong)data);
	if (!data)
		return -2;
	fs = get_fs();
	set_fs(KERNEL_DS);
	pos = 0;
	vfs_write(fp, (const char *)data, write_size, &pos);
	vfs_fsync(fp, 0);
	dc_print("decontour: write %u size to addr%p\n", write_size, data);
	codec_mm_unmap_phyaddr(data);
	filp_close(fp, NULL);
	set_fs(fs);

	dc_print("decontour:dump_2.0/2");
	write_size = w * h;
	dc_print("decontour:dump_4:yds_addr =%lx, write_size=%d\n",
		dcntr_mem->yds_addr, write_size);
	dc_print("decontour:dump_4:cds_addr =%lx\n", dcntr_mem->cds_addr);
	snprintf(name_buf, sizeof(name_buf), "/data/tmp/ds_out.yuv");

	fp = filp_open(name_buf, O_CREAT | O_RDWR, 0644);
	if (IS_ERR(fp)) {
		pr_err("decontour: create %s fail.\n", name_buf);
		return -4;
	}
	dc_print("decontour:dump_4: name_buf=%s\n", name_buf);

	data = codec_mm_vmap(dcntr_mem->yds_addr, dcntr_mem->yds_size);
	dc_print("decontour:dump_3\n");
	if (!data)
		return -5;
	fs = get_fs();
	set_fs(KERNEL_DS);
	pos = 0;
	vfs_write(fp, (const char *)data, write_size, &pos);
	vfs_fsync(fp, 0);
	dc_print("decontour: write %u size to file.addr%p\n", write_size, data);
	codec_mm_unmap_phyaddr(data);
	pos = write_size;

	write_size = w * h / 2;
	data = codec_mm_vmap(dcntr_mem->cds_addr, dcntr_mem->cds_size);
	dc_print("decontour:dump_4\n");
	if (!data)
		return -6;
	vfs_write(fp, (const char *)data, write_size, &pos);
	vfs_fsync(fp, 0);
	dc_print("decontour: write %u size to file.addr%p\n", write_size, data);
	codec_mm_unmap_phyaddr(data);
	filp_close(fp, NULL);
	set_fs(fs);
	return 0;
}

static int decontour_pre_process(struct vframe_s *vf)
{
	int mif_read_width; /*grid read mif input*/
	int mif_read_height;
	int mif_out_width;  /*grid read mif output; also dct input*/
	int mif_out_height;
	u32 ds_out_width = 0; /*dct ds output*/
	u32 ds_out_height = 0;
	int canvas_width;
	int canvas_height;
	u32 vf_org_width;
	u32 vf_org_height;
	u32 grd_num;
	u32 reg_grd_xnum;
	u32 reg_grd_ynum;
	u32 grd_xsize;
	u32 grd_ysize;
	/* u32 grid_wrmif_length; */
	u32 yflt_wrmif_length;
	u32 cflt_wrmif_length;
	int ds_ratio = 0;/*0:(1:1)  1:(1:2)  2:(1:4)*/
	u32 phy_addr_0, phy_addr_1, phy_addr_2;
	struct canvas_s src_cs0, src_cs1, src_cs2;
	int canvas_id_0, canvas_id_1, canvas_id_2;
	int src_fmt = 2; /*default 420*/
	struct dcntr_mem_s *dcntr_mem;
	int timeout = 0;
	int i;
	int mif_reverse;
	int swap_64bit;
	int ret;
	int skip = 0;
	u32 pic_struct;
	u32 h_avg;
	u32 bit_mode;
	bool need_ds = false;
	u32 burst_len = 2;
	bool format_changed = false;
	bool unsupported_resolution = false;
	u32 is_interlace = 0;
	u32 block_mode;
	u32 endian;
	u32 pic_32byte_aligned = 0;
	bool support_canvas = false;
	int src_hsize;
	int src_vsize;

	if (!vf)
		return -1;

	vf->decontour_pre = NULL;

	if (ppmgr_device.bypass_decontour == 1)
		return 0;

	if (get_cpu_type() <= MESON_CPU_MAJOR_ID_T5)
		support_canvas = true;

	if (vf->type & VIDTYPE_INTERLACE)
		if (!ppmgr_device.i_do_decontour)
			return 0;

	if (vf->type & VIDTYPE_COMPRESS) {
		if ((vf->type & VIDTYPE_NO_DW) || vf->canvas0Addr == 0) {
			dc_print("is afbc, but no dw\n");
			return 0;
		}
	}

	if (vf->width == 0 || vf->height == 0) {
		dc_print("w*h =%d*%d\n", vf->width, vf->height);
		return 0;
	}

	if ((vf->width * vf->height) < (160 * 120))
		return 0;

	if ((vf->type & VIDTYPE_VIU_422) && !(vf->type & 0x10000000)) {
		src_fmt = 0;
		need_ds = true;/*422 is one plane, post not support, need pre out nv21*/
	} else if ((vf->type & VIDTYPE_VIU_NV21) ||
		(vf->type & 0x10000000)) {/*hdmi in dw is nv21 VIDTYPE_DW_NV21*/
		src_fmt = 2;
	} else {
		dc_print("not support format vf->type =%x\n", vf->type);
		return 0; /*current one support 422 nv21*/
	}

	if (vf->type & VIDTYPE_COMPRESS) {
		vf_org_width = vf->compWidth;
		vf_org_height = vf->compHeight;
	} else {
		vf_org_width = vf->width;
		vf_org_height = vf->height;
	}

	if (ppmgr_device.decontour_addr == 0)
		decontour_init();

	if (ppmgr_device.decontour_addr == 0)
		return 0;

	pic_struct = 0;
	h_avg = 0;
	mif_out_width = vf->width;
	mif_out_height = vf->height;
	mif_read_width = vf->width;
	mif_read_height = vf->height;

	if (vf->type & VIDTYPE_INTERLACE) {
		is_interlace = 1;
		if (src_fmt == 2) {/*decoder output, top in odd rows*/
			if ((vf->type & VIDTYPE_INTERLACE_BOTTOM) == 0x3)
				pic_struct = 6;/*4 line read 1*/
			else
				pic_struct = 5;/*4 line read 1*/
			h_avg = 1;
			skip = 1;
			mif_out_width = vf->width >> 1;
			mif_out_height = vf->height >> 2;
		} else if (src_fmt == 0) {/*hdmiin output, In the first half of the line*/
			mif_out_width = vf->width;
			mif_out_height = vf->height >> 1;
			mif_read_height = vf->height >> 1;
			need_ds = 1;
			if (mif_out_width > 960 || mif_out_height > 540)
				ds_ratio = 1;
		}
	} else {
		if (vf->width > 1920 || vf->height > 1080) {
			ds_ratio = 1;
			skip = 1;
			mif_out_width = vf->width >> 1;
			mif_out_height = vf->height >> 1;
		} else if (vf->width > 960 || vf->height > 540) {
			if ((ppmgr_device.debug_decontour & DCT_USE_DS_SCALE) ||
				src_fmt == 0) { /*hdmi in always use ds*/
				ds_ratio = 1;
			} else {
				ds_ratio = 0; /*decoder use mif skip for save ddr*/
				skip = 1;
				dc_print("1080p use mif skip\n");
				mif_out_width = vf->width >> 1;
				mif_out_height = vf->height >> 1;
			}
		}

		if (skip && src_fmt == 2) {
			pic_struct = 4;
			h_avg = 1;
		}
	}

	ds_out_width = mif_out_width >> ds_ratio;
	ds_out_height = mif_out_height >> ds_ratio;

	if (ds_out_width < 16 || ds_out_height < 120) {
		dc_print("not supported: vf:%d * %d", vf->width, vf->height);
		return 0;
	}

	dc_print("src_fmt=%d, pic_struct=%d, h_avg =%d, bitdepth=%x\n",
		 src_fmt, pic_struct, h_avg, vf->bitdepth);

	dc_print("vf:%d * %d; mif_read:%d*%d;mif_out:%d * %d; ds_ratio=%d, skip=%d;ds_out:%d*%d\n",
		vf->width, vf->height,
		mif_read_width, mif_read_height,
		mif_out_width, mif_out_height,
		ds_ratio, skip,
		ds_out_width, ds_out_height);

	for (i = 0; i < DCNTR_POOL_SIZE; i++) {
		if (dcntr_mem_info[i].free) {
			dcntr_mem_info[i].free = false;
			break;
		}
	}
	if (i == DCNTR_POOL_SIZE) {
		pr_err("decontour: no free mem\n");
		return -1;
	}
	dcntr_mem = &dcntr_mem_info[i];

	dcntr_mem->use_org = true;
	dcntr_mem->ds_ratio = 0;

	if (ds_ratio || skip || need_ds)
		dcntr_mem->use_org = false;

	if (!dcntr_mem->use_org) {
		dcntr_mem->ds_ratio = ds_ratio;
		if (skip)
			dcntr_mem->ds_ratio = dcntr_mem->ds_ratio + 1;

		if (need_ds && (vf->type & VIDTYPE_COMPRESS))
			dcntr_mem->ds_ratio = (vf->compWidth / vf->width) >> 1;
	}

	if (dcntr_mem->use_org) {
		if (vf->type & VIDTYPE_COMPRESS) {
			if ((vf->compWidth / vf->width) * vf->width != vf->compWidth)
				unsupported_resolution = true;
			if ((vf->compHeight / vf->height) * vf->height != vf->compHeight)
				unsupported_resolution = true;
		}
	} else {
		if ((ds_out_width << dcntr_mem->ds_ratio) != vf_org_width)
			unsupported_resolution = true;
		if ((ds_out_height << dcntr_mem->ds_ratio) != (vf_org_height >> is_interlace))
			unsupported_resolution = true;
	}
	if (unsupported_resolution) {
		dcntr_mem->free = true;
		dc_print("unsupported resolution %d*%d, com %d*%d, ds_ratio=%d\n",
			vf->width, vf->height,
			vf->compWidth, vf->compHeight,
			dcntr_mem->ds_ratio);
		return 0;
	}

	vf->decontour_pre = (void *)dcntr_mem;

	if (vf->canvas0Addr == (u32)-1) {
		phy_addr_0 = vf->canvas0_config[0].phy_addr;
		phy_addr_1 = vf->canvas0_config[1].phy_addr;
		phy_addr_2 = vf->canvas0_config[2].phy_addr;
		canvas_config_config(ppmgr_src_canvas[0],
			&vf->canvas0_config[0]);
		canvas_config_config(ppmgr_src_canvas[1],
			&vf->canvas0_config[1]);
		canvas_config_config(ppmgr_src_canvas[2],
			&vf->canvas0_config[2]);
		canvas_id_0 = ppmgr_src_canvas[0];
		canvas_id_1 = ppmgr_src_canvas[1];
		canvas_id_2 = ppmgr_src_canvas[2];
		canvas_width = vf->canvas0_config[0].width;
		canvas_height = vf->canvas0_config[0].height;
		block_mode = vf->canvas0_config[0].block_mode;
		endian = vf->canvas0_config[0].endian;
	} else {
		dc_print("source is canvas\n");
		canvas_read(vf->canvas0Addr & 0xff, &src_cs0);
		canvas_read(vf->canvas0Addr >> 8 & 0xff, &src_cs1);
		canvas_read(vf->canvas0Addr >> 16 & 0xff, &src_cs2);
		phy_addr_0 = src_cs0.addr;
		phy_addr_1 = src_cs1.addr;
		phy_addr_2 = src_cs2.addr;
		canvas_id_0 = vf->canvas0Addr & 0xff;
		canvas_id_1 = vf->canvas0Addr >> 8 & 0xff;
		canvas_id_2 = vf->canvas0Addr >> 16 & 0xff;
		canvas_width = src_cs0.width;
		canvas_height = src_cs0.height;
		block_mode = src_cs0.blkmode;
		endian = src_cs0.endian;
	}
	dc_print("block_mode=%d, endian=%d\n", block_mode, endian);

	if (last_w != vf->width ||
		last_h != vf->height ||
		last_type != vf->type ||
		(ppmgr_device.debug_decontour & DCT_REWRITE_REG))
		format_changed = true;

	if (!format_changed) {
		dcntr_mem->grd_swap_64bit = dcntr_mem_info_last.grd_swap_64bit;
		dcntr_mem->yds_swap_64bit = dcntr_mem_info_last.yds_swap_64bit;
		dcntr_mem->cds_swap_64bit = dcntr_mem_info_last.cds_swap_64bit;
		dcntr_mem->grd_little_endian = dcntr_mem_info_last.grd_little_endian;
		dcntr_mem->yds_little_endian = dcntr_mem_info_last.yds_little_endian;
		dcntr_mem->cds_little_endian = dcntr_mem_info_last.cds_little_endian;
		dcntr_mem->yflt_wrmif_length = dcntr_mem_info_last.yflt_wrmif_length;
		dcntr_mem->cflt_wrmif_length = dcntr_mem_info_last.cflt_wrmif_length;
		goto SET_ADDR;
	}
	last_w = vf->width;
	last_h = vf->height;
	last_type = vf->type;

	if (src_fmt == 0) {
		src_hsize = canvas_width >> 1;
		src_vsize = canvas_height;
	} else {
		src_hsize = canvas_width;
		src_vsize = canvas_height;
	}
	ini_dcntr_pre(mif_out_width, mif_out_height, 2, ds_ratio);
	grd_num = Rd(DCTR_BGRID_PARAM3_PRE);
	reg_grd_xnum = (grd_num >> 16) & (0x3ff);
	reg_grd_ynum = grd_num & (0x3ff);

	grd_xsize = reg_grd_xnum << 3;
	grd_ysize = reg_grd_ynum;

	/* grid_wrmif_length = 81 * 46 * 8; */
	yflt_wrmif_length = ds_out_width;
	cflt_wrmif_length = ds_out_width;

	dc_print("canvas_width=%d, canvas_height=%d, src_hsize=%d, src_vsize=%d\n",
		canvas_width, canvas_height,
		src_hsize, src_vsize);

	if (canvas_width % 32)
		burst_len = 0;
	else if (canvas_width % 64)
		burst_len = 1;

	if (block_mode && !support_canvas)
		burst_len = block_mode;

	dcntr_grid_rdmif(canvas_id_0,         /*int canvas_id0,*/
		canvas_id_1,         /*int canvas_id1,*/
		canvas_id_2,         /*int canvas_id2,*/
		phy_addr_0,  /*int canvas_baddr0,*/
		phy_addr_1,           /*int canvas_baddr1,*/
		phy_addr_2,           /*int canvas_baddr2,*/
		src_hsize,   /*int src_hsize,*/
		src_vsize,   /*int src_vsize,*/
		src_fmt, /*1 = RGB/YCBCR(3 bytes/pixel), 0=422 (2 bytes/pixel) 2:420 (two canvas)*/
		0,           /*int mif_x_start,*/
		mif_read_width - 1, /*int mif_x_end  ,*/
		0,           /*int mif_y_start,*/
		mif_read_height - 1, /*int mif_y_end  ,*/
		0,            /*int mif_reverse  // 0 : no reverse*/
		pic_struct,/*0 : frame; 2:top_field; 3:bot_field; 4:y skip, uv no skip;*/
				/*5:top_field 1/4; 6 bot_field 1/4*/
		h_avg);    /*0 : no avg 1:y_avg  2:c_avg  3:y&c avg*/

	mif_reverse = 1;
	swap_64bit = 0;
	if (mif_reverse == 1) {
		dcntr_mem->grd_swap_64bit = false;
		dcntr_mem->yds_swap_64bit = false;
		dcntr_mem->cds_swap_64bit = false;
		dcntr_mem->grd_little_endian = true;
		dcntr_mem->yds_little_endian = true;
		dcntr_mem->cds_little_endian = true;
	}
	dcntr_grid_wrmif(1,  /*int mif_index,  //0:cds  1:grd   2:yds*/
		0,  /*int mem_mode,  //0:linear address mode  1:canvas mode*/
		5,  /*int src_fmt ,  //0:4bit 1:8bit 2:16bit 3:32bit 4:64bit 5:128bit*/
		ass_index,  /*int canvas_id*/
		0,  /*int mif_x_start*/
		grd_xsize - 1,  /*int mif_x_end*/
		0,  /*int mif_y_start*/
		grd_ysize - 1,  /*int mif_y_end*/
		swap_64bit,
		mif_reverse,  /*int mif_reverse, /0 : no reverse*/
		dcntr_mem->grd_addr,  /*int linear_baddr*/
		grd_xsize);  /*int linear_length // DDR read length = linear_length*128 bits*/

	if (ds_ratio || skip || need_ds) {
		yflt_wrmif_length = yflt_wrmif_length * 8 / 128;
		cflt_wrmif_length = cflt_wrmif_length * 8 / 128;
		dcntr_mem->yflt_wrmif_length = yflt_wrmif_length;
		dcntr_mem->cflt_wrmif_length = cflt_wrmif_length;

		dcntr_grid_wrmif(2,  /*int mif_index,  //0:cds  1:grd   2:yds*/
			0,  /*int mem_mode,  //0:linear address mode  1:canvas mode*/
			1,  /*int src_fmt,  //0:4bit 1:8bit 2:16bit 3:32bit 4:64bit 5:128bit*/
			ppmgr_dst_canvas[0],  /*int canvas_id*/
			0,  /*int mif_x_start*/
			ds_out_width - 1,  /*int mif_x_end*/
			0,  /*int mif_y_start*/
			ds_out_height - 1,  /*int mif_y_end*/
			swap_64bit,
			mif_reverse,  /*int mif_reverse   ,  //0 : no reverse*/
			dcntr_mem->yds_addr,  /*int linear_baddr*/
			yflt_wrmif_length); /*DDR read length = linear_length*128 bits*/

		dcntr_grid_wrmif(0,  /*int mif_index,  //0:cds  1:grd   2:yds*/
			0,  /*int mem_mode,  //0:linear address mode  1:canvas mode*/
			2,  /*int src_fmt,  //0:4bit 1:8bit 2:16bit 3:32bit 4:64bit 5:128bit*/
			ppmgr_dst_canvas[1],  /*int canvas_id*/
			0,  /*int mif_x_start*/
			(ds_out_width >> 1) - 1,  /*int mif_x_end*/
			0,  /*int mif_y_start*/
			(ds_out_height >> 1) - 1,  /*int mif_y_end*/
			swap_64bit,
			mif_reverse,  /*int mif_reverse   ,  //0 : no reverse*/
			dcntr_mem->cds_addr,  /*int linear_baddr*/
			cflt_wrmif_length); /*DDR read length = linear_length*128 bits*/
	}

	wr_bits(DCTR_BGRID_TOP_CTRL0, 1, 2, 2);/*reg_din_sel: 1:dos 2:vdin  0/3:disable*/

	if (ds_ratio || skip || need_ds) {
		wr_bits(DCTR_BGRID_TOP_CTRL0, 1, 1, 1);/*reg_ds_mif_en: 1=on 0=off*/
		wr_bits(DCTR_BGRID_TOP_FMT, 2, 19, 2); /*reg_fmt_mode, 2=420, 1=422, 0=444*/

		if (ds_ratio == 0)
			wr_bits(DCTR_BGRID_PATH_PRE, 1, 4, 1); /*reg_grd_path 0=ds 1=ori*/
		else
			wr_bits(DCTR_BGRID_PATH_PRE, 0, 4, 1); /*reg_grd_path 0=ds 1=ori*/

		if (ds_ratio == 1)
			wr_bits(DCTR_DS_PRE, 0x5, 0, 4); /*reg_ds_rate_xy*/
		else if (ds_ratio == 0)
			wr_bits(DCTR_DS_PRE, 0x0, 0, 4); /*reg_ds_rate_xy*/
	} else {
		wr_bits(DCTR_BGRID_TOP_CTRL0, 0, 1, 1); /*reg_ds_mif_en: 1=on 0=off*/
		wr_bits(DCTR_BGRID_PATH_PRE, 1, 4, 1); /*reg_grd_path 0=ds 1=ori*/
		wr_bits(DCTR_DS_PRE, 0x0, 0, 4);	   /*reg_ds_rate_xy*/
	}

	if (src_fmt == 0 && (vf->bitdepth & BITDEPTH_Y10) &&
		((vf->type & VIDTYPE_COMPRESS) == 0)) {/*all dw is 8bit*/
		if (vf->bitdepth & FULL_PACK_422_MODE)
			bit_mode = 3;
		else
			bit_mode = 1;
		wr_bits(DCNTR_GRID_GEN_REG3, bit_mode, 8, 2);/*0->8bit; 1->10bit422; 2->10bit444*/
		wr_bits(DCNTR_GRID_GEN_REG3, 0x3, 4, 3); /*cntl_blk_len: vd1 default is 3*/
	}
	wr_bits(DCNTR_GRID_GEN_REG3, (burst_len & 0x3), 1, 2);
	dcntr_mem_info_last = *dcntr_mem;

SET_ADDR:
	if (support_canvas) {
		Wr(DCNTR_GRID_CANVAS0,
			(canvas_id_2 << 16) |
			(canvas_id_1 << 8) |
			(canvas_id_0 << 0));
		Wr(DCNTR_GRD_WMIF_CTRL4, dcntr_mem->grd_addr);
		Wr(DCNTR_YDS_WMIF_CTRL4, dcntr_mem->yds_addr);
		Wr(DCNTR_CDS_WMIF_CTRL4, dcntr_mem->cds_addr);
	} else {
		Wr(DCNTR_GRID_BADDR_Y, phy_addr_0 >> 4);
		Wr(DCNTR_GRID_BADDR_CB, phy_addr_1 >> 4);
		Wr(DCNTR_GRID_BADDR_CR, phy_addr_2 >> 4);

		Wr(DCNTR_GRD_WMIF_CTRL4, dcntr_mem->grd_addr >> 4);
		Wr(DCNTR_YDS_WMIF_CTRL4, dcntr_mem->yds_addr >> 4);
		Wr(DCNTR_CDS_WMIF_CTRL4, dcntr_mem->cds_addr >> 4);

		wr_bits(DCNTR_GRID_GEN_REG3, block_mode, 12, 2);
		wr_bits(DCNTR_GRID_GEN_REG3, block_mode, 14, 2);
		if (block_mode)
			pic_32byte_aligned = 7;
		wr_bits(DCNTR_GRID_GEN_REG3,
			(pic_32byte_aligned << 7) |
			(block_mode << 4) |
			(block_mode << 2) |
			(block_mode << 0),
			18, 9);
		if (vf->flag & VFRAME_FLAG_VIDEO_LINEAR) {
			wr_bits(DCNTR_GRID_GEN_REG3, 0, 0, 1);
			dc_print("LINEAR:flag=%x\n", vf->flag);
		} else {
			wr_bits(DCNTR_GRID_GEN_REG3, 1, 0, 1);
			dc_print("block_mode:flag=%x\n", vf->flag);
		}
	}

	if (ppmgr_device.debug_decontour & DCT_DUMP_REG)
		decontour_dump_reg();

	dc_print("decontour:start_1\n");
	wr_bits(DCTR_BGRID_TOP_CTRL0, 1, 31, 1);
	dc_print("decontour:start_2\n");

	timeout = wait_for_completion_timeout(&isr_done,
		msecs_to_jiffies(200));
	if (!timeout)
		pr_err("decontour:wait isr timeout\n");

	if (ppmgr_device.dump_grid) {
		ret = decontour_dump_output(ds_out_width, ds_out_height,
			dcntr_mem, skip);
		if (ret)
			pr_err("decontour_dump_output error, ret=%d\n", ret);
		ppmgr_device.dump_grid = 0;
	}

	if (ppmgr_device.debug_decontour & DCT_PRINT_INFO)
		decontour_print_parm(dcntr_mem);

	dc_print("decontour:ok index=%d\n", vf->omx_index);
	return 0;
}

/************************************************
 *
 *   ppmgr as a frame provider
 *
 * ***********************************************
 */
static int task_running;
static int still_picture_notify;
/*static int q_free_set = 0 ;*/
static struct vframe_s *ppmgr_vf_peek(void *op_arg)
{
	struct vframe_s *vf;

	if (ppmgr_blocking)
		return NULL;
	vf = vfq_peek(&q_ready);
	return vf;
}

static struct vframe_s *ppmgr_vf_get(void *op_arg)
{
	struct vframe_s *vf;

	if (ppmgr_blocking)
		return NULL;
	vf = vfq_pop(&q_ready);
	if (vf)
		ppmgr_device.get_count++;
	return vf;
}

static void ppmgr_vf_put(struct vframe_s *vf, void *op_arg)
{
	struct ppframe_s *vf_local;
	int i;
	int index;
	struct ppframe_s *pp_vf = to_ppframe(vf);

	if (ppmgr_blocking) {
		pr_info("%s: ppmgr_blocking is 1\n", __func__);
		return;
	}
	ppmgr_device.put_count++;

	i = vfq_level(&q_free);

	while (i > 0) {
		index = (q_free.rp + i - 1) % (q_free.size);
		vf_local = to_ppframe(q_free.pool[index]);
		if (vf_local->index == pp_vf->index) {
			pr_err("ppmgr put error1 %d\n", pp_vf->index);
			return;
		}

		i--;
	}
	i = vfq_level(&q_ready);
	while (i > 0) {
		index = (q_ready.rp + i - 1) % (q_ready.size);
		vf_local = to_ppframe(q_ready.pool[index]);
		if (vf_local->index == pp_vf->index) {
			pr_err("ppmgr put error2 %d\n", pp_vf->index);
			return;
		}

		i--;
	}

	/* the frame is in bypass mode, put the decoder frame */
	if (pp_vf->dec_frame) {
		ppmgr_vf_put_dec(pp_vf->dec_frame);
		pp_vf->dec_frame = NULL;
	}
	vfq_push(&q_free, vf);
}

int vf_ppmgr_get_states(struct vframe_states *states)
{
	int ret = -1;
	unsigned long flags;
	struct vframe_provider_s *vfp = NULL;

#ifdef CONFIG_AMLOGIC_MEDIA_VFM
	vfp = vf_get_provider(RECEIVER_NAME);
#endif
	spin_lock_irqsave(&lock, flags);
	if (vfp && vfp->ops && vfp->ops->vf_states)
		ret = vfp->ops->vf_states(states, vfp->op_arg);

	spin_unlock_irqrestore(&lock, flags);
	return ret;
}

static int get_source_type(struct vframe_s *vf)
{
	enum ppmgr_source_type ret;
	int interlace_mode;

	interlace_mode = vf->type & VIDTYPE_TYPEMASK;
	if (vf->source_type == VFRAME_SOURCE_TYPE_HDMI ||
	    vf->source_type == VFRAME_SOURCE_TYPE_CVBS) {
		if ((vf->bitdepth & BITDEPTH_Y10) &&
		    (!(vf->type & VIDTYPE_COMPRESS)) &&
		    (get_cpu_type() >= MESON_CPU_MAJOR_ID_TXL))
			ret = VDIN_10BIT_NORMAL;
		else
			ret = VDIN_8BIT_NORMAL;
	} else {
		if ((vf->bitdepth & BITDEPTH_Y10) &&
		    (!(vf->type & VIDTYPE_COMPRESS)) &&
		    (get_cpu_type() >= MESON_CPU_MAJOR_ID_TXL)) {
			if (interlace_mode == VIDTYPE_INTERLACE_TOP)
				ret = DECODER_10BIT_TOP;
			else if (interlace_mode == VIDTYPE_INTERLACE_BOTTOM)
				ret = DECODER_10BIT_BOTTOM;
			else
				ret = DECODER_10BIT_NORMAL;
		} else {
			if (interlace_mode == VIDTYPE_INTERLACE_TOP)
				ret = DECODER_8BIT_TOP;
			else if (interlace_mode == VIDTYPE_INTERLACE_BOTTOM)
				ret = DECODER_8BIT_BOTTOM;
			else
				ret = DECODER_8BIT_NORMAL;
		}
	}
	return ret;
}

static int get_input_format(struct vframe_s *vf)
{
	int format = GE2D_FORMAT_M24_YUV420;
	enum ppmgr_source_type soure_type;

	soure_type = (enum ppmgr_source_type)get_source_type(vf);
	switch (soure_type) {
	case DECODER_8BIT_NORMAL:
		if (vf->type & VIDTYPE_VIU_422)
			format = GE2D_FORMAT_S16_YUV422;
		else if (vf->type & VIDTYPE_VIU_NV21)
			format = GE2D_FORMAT_M24_NV21;
		else if (vf->type & VIDTYPE_VIU_444)
			format = GE2D_FORMAT_S24_YUV444;
		else
			format = GE2D_FORMAT_M24_YUV420;
		break;
	case DECODER_8BIT_BOTTOM:
		if (vf->type & VIDTYPE_VIU_422)
			format = GE2D_FORMAT_S16_YUV422
				| (GE2D_FORMAT_S16_YUV422B & (3 << 3));
		else if (vf->type & VIDTYPE_VIU_NV21)
			format = GE2D_FORMAT_M24_NV21
				| (GE2D_FORMAT_M24_NV21B & (3 << 3));
		else if (vf->type & VIDTYPE_VIU_444)
			format = GE2D_FORMAT_S24_YUV444
				| (GE2D_FORMAT_S24_YUV444B & (3 << 3));
		else
			format = GE2D_FORMAT_M24_YUV420
				| (GE2D_FMT_M24_YUV420B & (3 << 3));
		break;
	case DECODER_8BIT_TOP:
		if (vf->type & VIDTYPE_VIU_422)
			format = GE2D_FORMAT_S16_YUV422
				| (GE2D_FORMAT_S16_YUV422T & (3 << 3));
		else if (vf->type & VIDTYPE_VIU_NV21)
			format = GE2D_FORMAT_M24_NV21
				| (GE2D_FORMAT_M24_NV21T & (3 << 3));
		else if (vf->type & VIDTYPE_VIU_444)
			format = GE2D_FORMAT_S24_YUV444
				| (GE2D_FORMAT_S24_YUV444T & (3 << 3));
		else
			format = GE2D_FORMAT_M24_YUV420
				| (GE2D_FMT_M24_YUV420T & (3 << 3));
		break;
	case DECODER_10BIT_NORMAL:
		if (vf->type & VIDTYPE_VIU_422) {
			if (vf->bitdepth & FULL_PACK_422_MODE)
				format = GE2D_FORMAT_S16_10BIT_YUV422;
			else
				format = GE2D_FORMAT_S16_12BIT_YUV422;
		}
		break;
	case DECODER_10BIT_BOTTOM:
		if (vf->type & VIDTYPE_VIU_422) {
			if (vf->bitdepth & FULL_PACK_422_MODE)
				format = GE2D_FORMAT_S16_10BIT_YUV422
					| (GE2D_FORMAT_S16_10BIT_YUV422B
					& (3 << 3));
			else
				format = GE2D_FORMAT_S16_12BIT_YUV422
					| (GE2D_FORMAT_S16_12BIT_YUV422B
					& (3 << 3));
		}
		break;
	case DECODER_10BIT_TOP:
		if (vf->type & VIDTYPE_VIU_422) {
			if (vf->bitdepth & FULL_PACK_422_MODE)
				format = GE2D_FORMAT_S16_10BIT_YUV422
					| (GE2D_FORMAT_S16_10BIT_YUV422T
					& (3 << 3));
			else
				format = GE2D_FORMAT_S16_12BIT_YUV422
					| (GE2D_FORMAT_S16_12BIT_YUV422T
					& (3 << 3));
		}
		break;
	case VDIN_8BIT_NORMAL:
		if (vf->type & VIDTYPE_VIU_422) {
			format = GE2D_FORMAT_S16_YUV422;
		} else if (vf->type & VIDTYPE_VIU_NV21) {
			format = GE2D_FORMAT_M24_NV21;
		} else if (vf->type & VIDTYPE_VIU_444) {
			format = GE2D_FORMAT_S24_YUV444;
		} else if (vf->type & VIDTYPE_RGB_444) {
			format = GE2D_FORMAT_S24_RGB;
			if (vf->flag & VFRAME_FLAG_VIDEO_LINEAR)
				;
			else
				format &= (~GE2D_LITTLE_ENDIAN);
		} else {
			format = GE2D_FORMAT_M24_YUV420;
		}
		break;
	case VDIN_10BIT_NORMAL:
		if (vf->type & VIDTYPE_VIU_422) {
			if (vf->bitdepth & FULL_PACK_422_MODE)
				format = GE2D_FORMAT_S16_10BIT_YUV422;
			else
				format = GE2D_FORMAT_S16_12BIT_YUV422;
		} else if (vf->type & VIDTYPE_VIU_444) {
			format = GE2D_FORMAT_S24_10BIT_YUV444;
		} else if (vf->type & VIDTYPE_RGB_444) {
			format = GE2D_FORMAT_S24_10BIT_RGB888;
			if (vf->flag & VFRAME_FLAG_VIDEO_LINEAR)
				format |= GE2D_LITTLE_ENDIAN;
		}
		break;
	default:
		format = GE2D_FORMAT_M24_YUV420;
	}

	return format;
}

/* extern int get_property_change(void); */
/* extern void set_property_change(int flag); */

static int ppmgr_event_cb(int type, void *data, void *private_data)
{
	if (type & VFRAME_EVENT_RECEIVER_PUT) {
#ifdef DDD
		PPMGRVPP_WARN("video put, avail=%d, free=%d\n",
			      vfq_level(&q_ready), vfq_level(&q_free));
#endif
		up(&ppmgr_device.ppmgr_sem);
	}
	if (type & VFRAME_EVENT_RECEIVER_FRAME_WAIT) {
		if (task_running) {
#ifdef CONFIG_AMLOGIC_MEDIA_FRAME_SYNC
			if (timestamp_pcrscr_enable_state())
				return 0;
#endif
			if (get_property_change()) {
				/*printk("--ppmgr: get angle changed msg.\n");*/
				set_property_change(0);
				still_picture_notify = 1;
				up(&ppmgr_device.ppmgr_sem);
			} else {
				up(&ppmgr_device.ppmgr_sem);
			}
		}
	}
	return 0;
}

static int ppmgr_vf_states(struct vframe_states *states, void *op_arg)
{
	unsigned long flags;

	spin_lock_irqsave(&lock, flags);

	states->vf_pool_size = VF_POOL_SIZE;
	states->buf_recycle_num = 0;
	states->buf_free_num = vfq_level(&q_free);
	states->buf_avail_num = vfq_level(&q_ready);

	spin_unlock_irqrestore(&lock, flags);

	return 0;
}

static const struct vframe_operations_s ppmgr_vf_provider = {
		.peek = ppmgr_vf_peek,
		.get = ppmgr_vf_get,
		.put = ppmgr_vf_put,
		.event_cb = ppmgr_event_cb,
		.vf_states = ppmgr_vf_states, };
#ifdef CONFIG_AMLOGIC_MEDIA_VFM
static struct vframe_provider_s ppmgr_vf_prov;
#endif
/************************************************
 *
 *   ppmgr as a frame receiver
 *
 *************************************************/

static int ppmgr_receiver_event_fun(int type, void *data, void*);

static const struct vframe_receiver_op_s ppmgr_vf_receiver = {.event_cb =
		ppmgr_receiver_event_fun};
#ifdef CONFIG_AMLOGIC_MEDIA_VFM
static struct vframe_receiver_s ppmgr_vf_recv;
#endif
static int ppmgr_receiver_event_fun(int type, void *data, void *private_data)
{
	struct vframe_states states;

	switch (type) {
	case VFRAME_EVENT_PROVIDER_VFRAME_READY:
#ifdef DDD
		PPMGRVPP_WARN("dec put, avail=%d, free=%d\n",
			      vfq_level(&q_ready), vfq_level(&q_free));
#endif
		up(&ppmgr_device.ppmgr_sem);
		break;
	case VFRAME_EVENT_PROVIDER_QUREY_STATE:
		ppmgr_vf_states(&states, NULL);
		if (states.buf_avail_num > 0)
			return RECEIVER_ACTIVE;
#ifdef CONFIG_AMLOGIC_MEDIA_VFM
		if (vf_notify_receiver(PROVIDER_NAME,
				       VFRAME_EVENT_PROVIDER_QUREY_STATE,
				       NULL) == RECEIVER_ACTIVE)
			return RECEIVER_ACTIVE;
#endif
		return RECEIVER_INACTIVE;
	case VFRAME_EVENT_PROVIDER_START:
#ifdef DDD
		PPMGRVPP_WARN("register now\n");
#endif
		up(&ppmgr_device.ppmgr_sem);
		vf_ppmgr_reg_provider();
#ifdef CONFIG_AMLOGIC_MEDIA_VFM
		vf_notify_receiver(PROVIDER_NAME,
				   VFRAME_EVENT_PROVIDER_START,
				   NULL);
#endif
		break;
	case VFRAME_EVENT_PROVIDER_UNREG:
#ifdef DDD
		PPMGRVPP_WARN("unregister now\n");
#endif
		vf_ppmgr_unreg_provider();
		break;
	case VFRAME_EVENT_PROVIDER_LIGHT_UNREG:
		break;
	case VFRAME_EVENT_PROVIDER_RESET:
		vf_ppmgr_reset(0);
#ifdef CONFIG_AMLOGIC_MEDIA_VFM
		vf_notify_receiver(PROVIDER_NAME,
				   VFRAME_EVENT_PROVIDER_RESET,
				   NULL);
#endif
		break;
	case VFRAME_EVENT_PROVIDER_FR_HINT:
	case VFRAME_EVENT_PROVIDER_FR_END_HINT:
#ifdef CONFIG_AMLOGIC_MEDIA_VFM
		vf_notify_receiver(PROVIDER_NAME, type, data);
#endif
		break;
	default:
		break;
	}
	return 0;
}

void vf_local_init(void)
{
	int i;

	set_property_change(0);
	still_picture_notify = 0;
	vfq_init(&q_free, VF_POOL_SIZE + 1, &vfp_pool_free[0]);
	vfq_init(&q_ready, VF_POOL_SIZE + 1, &vfp_pool_ready[0]);
	ppmgr_device.get_count = 0;
	ppmgr_device.put_count = 0;
	ppmgr_device.get_dec_count = 0;
	ppmgr_device.put_dec_count = 0;
	need_data_notify = true;
	pr_info("ppmgr local_init\n");
	for (i = 0; i < VF_POOL_SIZE; i++) {
		vfp_pool[i].index = i;
		vfp_pool[i].dec_frame = NULL;
		vfq_push(&q_free, &vfp_pool[i].frame);
	}

	for (i = 0; i < VF_POOL_SIZE; i++) {
		buf_status[i].index = -1; /* ppmgr_canvas_tab[i]; */
		buf_status[i].dirty = 1;
	}
	//up(&ppmgr_device.ppmgr_sem);
}

static void local_canvas_uninit(void)
{
	int i;

#ifdef CANVAS_RESERVED
	for (i = 0; i < 3; i++)
		ppmgr_src_canvas[i] = -1;
	for (i = 0; i < 2; i++)
		ppmgr_dst_canvas[i] = -1;
	ass_index = -1;
#else
	for (i = 0; i < 3; i++) {
		if (ppmgr_src_canvas[i] >= 0)
			canvas_pool_map_free_canvas(ppmgr_src_canvas[i]);
		ppmgr_src_canvas[i] = -1;
	}

	for (i = 0; i < 2; i++) {
		if (ppmgr_dst_canvas[i] >= 0)
			canvas_pool_map_free_canvas(ppmgr_dst_canvas[i]);
		ppmgr_dst_canvas[i] = -1;
	}

	if (ass_index >= 0)
		canvas_pool_map_free_canvas(ass_index);
	ass_index = -1;
#endif
	local_canvas_status = 0;
}

static int local_canvas_init(void)
{
	int i;
	bool alloc_fail = false;

	if (local_canvas_status)
		return local_canvas_status;

#ifdef CANVAS_RESERVED
	for (i = 0; i < 3; i++)
		ppmgr_src_canvas[i] = PPMGR_CANVAS_INDEX + i;
	for (i = 0; i < 2; i++)
		ppmgr_src_canvas[i] = PPMGR_CANVAS_INDEX + i + 3;
	ass_index = PPMGR_CANVAS_INDEX + 5;
#else
	for (i = 0; i < 3; i++) {
		if (ppmgr_src_canvas[i] < 0)
			ppmgr_src_canvas[i] =
				canvas_pool_map_alloc_canvas("ppmgr_src");

		if (ppmgr_src_canvas[i] < 0) {
			PPMGRVPP_INFO("%s ppmgr_src_canvas[%d] alloc failed\n",
				__func__, i);
			alloc_fail = true;
			break;
		}
	}

	for (i = 0; i < 2; i++) {
		if (ppmgr_dst_canvas[i] < 0)
			ppmgr_dst_canvas[i] =
				canvas_pool_map_alloc_canvas("ppmgr_dst");

		if (ppmgr_dst_canvas[i] < 0) {
			PPMGRVPP_INFO("%s ppmgr_dst_canvas[%d] alloc failed\n",
				__func__, i);
			alloc_fail = true;
			break;
		}
	}

	if (ass_index < 0)
		ass_index = canvas_pool_map_alloc_canvas("ppmgr_temp");

	if (ass_index < 0) {
		PPMGRVPP_INFO("%s ass_index alloc failed\n", __func__);
		alloc_fail = true;
	}
#endif
	if (alloc_fail)
		local_canvas_uninit();
	else
		local_canvas_status = 1;
	return local_canvas_status;
}

const struct vframe_receiver_op_s *vf_ppmgr_reg_provider(void)
{
	const struct vframe_receiver_op_s *r = NULL;

	ppmgr_device.is_used = true;

	if (ppmgr_device.debug_first_frame == 1) {
		dumpfirstframe = 1;
		count_scr = 0;
		count_dst = 0;
		PPMGRVPP_INFO("need dump first frame!\n");
	}

	mutex_lock(&ppmgr_mutex);

	vf_local_init();
#ifdef CONFIG_AMLOGIC_MEDIA_VFM
	vf_reg_provider(&ppmgr_vf_prov);
#endif
	if (start_ppmgr_task() == 0)
		r = &ppmgr_vf_receiver;

	ppmgr_device.started = 1;

	mutex_unlock(&ppmgr_mutex);

	return r;
}

void vf_ppmgr_unreg_provider(void)
{
	mutex_lock(&ppmgr_mutex);

	stop_ppmgr_task();

	vf_unreg_provider(&ppmgr_vf_prov);

#ifdef PPMGR_TB_DETECT
	tb_buffer_uninit();
#endif
	ppmgr_buffer_uninit();

	ppmgr_device.started = 0;

	mutex_unlock(&ppmgr_mutex);

	if (is_decontour_supported())
		decontour_uninit();
	ppmgr_device.is_used = false;
}

/*skip the second reset request*/
static unsigned long last_reset_time;
static unsigned long current_reset_time;
#define PPMGR_RESET_INTERVAL  (HZ / 5)
void vf_ppmgr_reset(int type)
{
	if (ppmgr_inited) {
		current_reset_time = jiffies;
		if (abs(current_reset_time - last_reset_time)
			< PPMGR_RESET_INTERVAL)
			return;

		ppmgr_blocking = true;
		up(&ppmgr_device.ppmgr_sem);

		last_reset_time = current_reset_time;
	}
}

void vf_ppmgr_init_receiver(void)
{
#ifdef CONFIG_AMLOGIC_MEDIA_VFM
	vf_receiver_init(&ppmgr_vf_recv, RECEIVER_NAME,
			 &ppmgr_vf_receiver, NULL);
#endif
}

void vf_ppmgr_reg_receiver(void)
{
#ifdef CONFIG_AMLOGIC_MEDIA_VFM
	vf_reg_receiver(&ppmgr_vf_recv);
#endif
}

void vf_ppmgr_init_provider(void)
{
#ifdef CONFIG_AMLOGIC_MEDIA_VFM
	vf_provider_init(&ppmgr_vf_prov, PROVIDER_NAME,
			 &ppmgr_vf_provider, NULL);
#endif
}

static inline struct vframe_s *ppmgr_vf_peek_dec(void)
{
	struct vframe_s *vf = NULL;
#ifdef DDD
	struct vframe_provider_s *vfp;
	struct vframe_s *vf;

	vfp = vf_get_provider(RECEIVER_NAME);
	if (!(vfp && vfp->ops && vfp->ops->peek))
		return NULL;

	vf = vfp->ops->peek(vfp->op_arg);
	return vf;
#else
#ifdef CONFIG_AMLOGIC_MEDIA_VFM
	vf = vf_peek(RECEIVER_NAME);
#endif
	return vf;
#endif
}

static inline struct vframe_s *ppmgr_vf_get_dec(void)
{
	struct vframe_s *vf = NULL;
#ifdef DDD
	struct vframe_provider_s *vfp;
	struct vframe_s *vf;

	vfp = vf_get_provider(RECEIVER_NAME);
	if (!(vfp && vfp->ops && vfp->ops->peek))
		return NULL;
	vf = vfp->ops->get(vfp->op_arg);
	return vf;
#else
#ifdef CONFIG_AMLOGIC_MEDIA_VFM
	vf = vf_get(RECEIVER_NAME);
#endif
	if (vf) {
		ppmgr_device.get_dec_count++;
		vf->decontour_pre = NULL;
	}
	return vf;
#endif
}

void ppmgr_vf_put_dec(struct vframe_s *vf)
{
	struct dcntr_mem_s *dcntr_mem;
#ifdef DDD
	struct vframe_provider_s *vfp;

	vfp = vf_get_provider(RECEIVER_NAME);
	if (!(vfp && vfp->ops && vfp->ops->peek))
		return;
	vfp->ops->put(vf, vfp->op_arg);
#else
	if (vf->decontour_pre) {
		dcntr_mem = (struct dcntr_mem_s *)vf->decontour_pre;
		dcntr_mem->free = true;
		vf->decontour_pre = NULL;
	}
	vf_put(vf, RECEIVER_NAME);
	ppmgr_device.put_dec_count++;

#endif
}

/************************************************
 *
 *   main task functions.
 *
 *************************************************/
static void vf_rotate_adjust(struct vframe_s *vf,
			     struct vframe_s *new_vf,
			     int angle)
{
	int w = 0, h = 0, disp_w = 0, disp_h = 0;
	int input_height = 0, input_width = 0;
	int scale_down_value = 1;
	int interlace_mode = 0;

	disp_w = ppmgr_device.disp_width / scale_down_value;
	disp_h = ppmgr_device.disp_height / scale_down_value;
	interlace_mode = vf->type & VIDTYPE_TYPEMASK;
	input_width = vf->width;
	if (interlace_mode)
		input_height = vf->height * 2;
	else
		input_height = vf->height;
	if (ppmgr_device.ppmgr_debug & 1) {
		PPMGRVPP_INFO("disp_width: %d, disp_height: %d\n",
			      disp_w, disp_h);
		PPMGRVPP_INFO("input_width: %d, input_height: %d\n",
			      input_width, input_height);
	}
	if (angle & 1) {
		int ar = (vf->ratio_control
			  >> DISP_RATIO_ASPECT_RATIO_BIT) & 0x3ff;
		if (ppmgr_device.ppmgr_debug == 2)
			ar = 0;

		if (input_width > input_height) {
			h = min_t(int, input_width, disp_h);

			if (ar == 0 || ar == 0x3ff)
				w = input_height * h / input_width;
			else
				w = (ar * h) >> 8;

			if (w > disp_w) {
				h = (h * disp_w) / w;
				w = disp_w;
			}
		} else {
			w = min_t(int, input_height, disp_w);

			if (ar == 0 || ar == 0x3ff)
				h = input_width * w / input_height;
			else
				h = (w << 8) / ar;

			if (h > disp_h) {
				w = (w * disp_h) / h;
				h = disp_h;
			}
		}
		if (ppmgr_device.ppmgr_debug & 1)
			PPMGRVPP_INFO("width: %d, height: %d, ar: %d\n",
				      w, h, ar);
		new_vf->ratio_control = DISP_RATIO_PORTRAIT_MODE;
		new_vf->ratio_control |=
				(h * 0x100 / w) << DISP_RATIO_ASPECT_RATIO_BIT;
		/*set video aspect ratio*/
	} else {
		if (input_width < disp_w && input_height < disp_h) {
			w = input_width;
			h = input_height;
		} else {
			if ((input_width * disp_h) > (disp_w * input_height)) {
				w = disp_w;
				h = disp_w * input_height / input_width;
			} else {
				h = disp_h;
				w = disp_h * input_width / input_height;
			}
		}

		new_vf->ratio_control = vf->ratio_control;
	}

	if (h > 1280) {
		w = w * 1280 / h;
		h = 1280;
	}

	new_vf->width = (w + 0x1f) & ~0x1f;
	new_vf->height = h;
}

#ifdef PPMGR_TB_DETECT
static int process_vf_tb_detect(struct vframe_s *vf,
				struct ge2d_context_s *context,
				struct config_para_ex_s *ge2d_config)
{
	struct canvas_s cs0, cs1, cs2, cd;
	int interlace_mode;
	u32 canvas_id;
	u32 format = GE2D_FORMAT_M24_YUV420;
	u32 h_scale_coef_type =
		context->config.h_scale_coef_type;
	u32 v_scale_coef_type =
		context->config.v_scale_coef_type;

	if (unlikely(!vf) || !context)
		return -1;

	interlace_mode = vf->type & VIDTYPE_TYPEMASK;
	if (!interlace_mode)
		return 0;

	if (vf->type & VIDTYPE_VIU_422)
		format = GE2D_FORMAT_S16_YUV422;
	else if (vf->type & VIDTYPE_VIU_NV21)
		format = GE2D_FORMAT_M24_NV21;
	else
		format = GE2D_FORMAT_M24_YUV420;
	if (tb_buff_wptr & 1) {
		format = format
			| (GE2D_FORMAT_M24_NV21B & (3 << 3));
		context->config.h_scale_coef_type =
			FILTER_TYPE_GAU0;
		context->config.v_scale_coef_type =
			FILTER_TYPE_GAU0_BOT;
	} else {
		format = format
			| (GE2D_FORMAT_M24_NV21T & (3 << 3));
		context->config.h_scale_coef_type =
			FILTER_TYPE_GAU0;
		context->config.v_scale_coef_type =
			FILTER_TYPE_GAU0;
	}
	canvas_config(ppmgr_dst_canvas[0],
		      detect_buf[tb_buff_wptr].paddr,
		      TB_DETECT_W, TB_DETECT_H,
		      CANVAS_ADDR_NOWRAP,
		      CANVAS_BLKMODE_LINEAR);
	memset(ge2d_config, 0, sizeof(struct config_para_ex_s));

	ge2d_config->alu_const_color = 0;
	ge2d_config->bitmask_en = 0;
	ge2d_config->src1_gb_alpha = 0;/* 0xff; */
	ge2d_config->dst_xy_swap = 0;

	if (vf->canvas0Addr == (u32)-1) {
		canvas_config_config(ppmgr_src_canvas[0] & 0xff,
				     &vf->canvas0_config[0]);
		ge2d_config->src_planes[0].addr =
			vf->canvas0_config[0].phy_addr;
		ge2d_config->src_planes[0].w = vf->canvas0_config[0].width;
		ge2d_config->src_planes[0].h = vf->canvas0_config[0].height;

		if (vf->plane_num > 1) {
			canvas_config_config(ppmgr_src_canvas[1] & 0xff,
					     &vf->canvas0_config[1]);
			ge2d_config->src_planes[1].addr =
				vf->canvas0_config[1].phy_addr;
			ge2d_config->src_planes[1].w =
				vf->canvas0_config[1].width;
			ge2d_config->src_planes[1].h =
				vf->canvas0_config[1].height;
		}
		if (vf->plane_num > 2) {
			canvas_config_config(ppmgr_src_canvas[2] & 0xff,
					     &vf->canvas0_config[2]);
			ge2d_config->src_planes[2].addr =
				vf->canvas0_config[2].phy_addr;
			ge2d_config->src_planes[2].w =
				vf->canvas0_config[2].width;
			ge2d_config->src_planes[2].h =
				vf->canvas0_config[2].height;
		}
		canvas_id =
			(ppmgr_src_canvas[0] & 0xff)
			| ((ppmgr_src_canvas[1] & 0xff) << 8)
			| ((ppmgr_src_canvas[2] & 0xff) << 16);
		ge2d_config->src_para.canvas_index = canvas_id;

	} else {
		canvas_read(vf->canvas0Addr & 0xff, &cs0);
		canvas_read((vf->canvas0Addr >> 8) & 0xff, &cs1);
		canvas_read((vf->canvas0Addr >> 16) & 0xff, &cs2);
		ge2d_config->src_planes[0].addr = cs0.addr;
		ge2d_config->src_planes[0].w = cs0.width;
		ge2d_config->src_planes[0].h = cs0.height;
		ge2d_config->src_planes[1].addr = cs1.addr;
		ge2d_config->src_planes[1].w = cs1.width;
		ge2d_config->src_planes[1].h = cs1.height;
		ge2d_config->src_planes[2].addr = cs2.addr;
		ge2d_config->src_planes[2].w = cs2.width;
		ge2d_config->src_planes[2].h = cs2.height;
		ge2d_config->src_para.canvas_index = vf->canvas0Addr;
	}
	canvas_read(ppmgr_dst_canvas[0] & 0xff, &cd);
	ge2d_config->dst_planes[0].addr = cd.addr;
	ge2d_config->dst_planes[0].w = cd.width;
	ge2d_config->dst_planes[0].h = cd.height;
	ge2d_config->src_key.key_enable = 0;
	ge2d_config->src_key.key_mask = 0;
	ge2d_config->src_key.key_mode = 0;
	ge2d_config->src_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->src_para.format = format;
	ge2d_config->src_para.fill_color_en = 0;
	ge2d_config->src_para.fill_mode = 0;
	ge2d_config->src_para.x_rev = 0;
	ge2d_config->src_para.y_rev = 0;
	ge2d_config->src_para.color = 0xffffffff;
	ge2d_config->src_para.top = 0;
	ge2d_config->src_para.left = 0;
	ge2d_config->src_para.width = vf->width;
	ge2d_config->src_para.height = vf->height / 2;
	ge2d_config->src2_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->dst_para.canvas_index = ppmgr_dst_canvas[0];

	ge2d_config->dst_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->dst_para.format =
		GE2D_FORMAT_S8_Y | GE2D_LITTLE_ENDIAN;
	ge2d_config->dst_para.fill_color_en = 0;
	ge2d_config->dst_para.fill_mode = 0;
	ge2d_config->dst_para.x_rev = 0;
	ge2d_config->dst_para.y_rev = 0;
	ge2d_config->dst_para.color = 0;
	ge2d_config->dst_para.top = 0;
	ge2d_config->dst_para.left = 0;
	ge2d_config->dst_para.width = TB_DETECT_W;
	ge2d_config->dst_para.height = TB_DETECT_H;

	if (ge2d_context_config_ex(context, ge2d_config) < 0) {
		pr_err("++ge2d configing error.\n");
		context->config.h_scale_coef_type =
			h_scale_coef_type;
		context->config.v_scale_coef_type =
			v_scale_coef_type;
		return -1;
	}

	stretchblt_noalpha(context, 0, 0, vf->width,
			   vf->height / 2,
			   0, 0, TB_DETECT_W,
			   TB_DETECT_H);
#ifdef CONFIG_AMLOGIC_MEDIA_CODEC_MM
	codec_mm_dma_flush((void *)detect_buf[tb_buff_wptr].vaddr,
			   TB_DETECT_W * TB_DETECT_H,
			   DMA_FROM_DEVICE);
#endif
	context->config.h_scale_coef_type =
		h_scale_coef_type;
	context->config.v_scale_coef_type =
		v_scale_coef_type;
	return 1;
}
#endif

#ifdef CONFIG_AMLOGIC_MEDIA_CODEC_MM
static int copy_phybuf_to_file(ulong phys, u32 size,
			       struct file *fp, loff_t pos)
{
	u32 span = SZ_1M;
	u8 *p;
	int remain_size = 0;
	ssize_t ret;

	remain_size = size;
	while (remain_size > 0) {
		if (remain_size < span)
			span = remain_size;
		p = codec_mm_vmap(phys, PAGE_ALIGN(span));
		if (!p) {
			PPMGRVPP_INFO("vmap failed\n");
			return -1;
		}
		codec_mm_dma_flush(p, span, DMA_FROM_DEVICE);
		ret = vfs_write(fp, (char *)p, span, &pos);
		if (ret <= 0)
			PPMGRVPP_INFO("vfs write failed!\n");
		phys += span;
		codec_mm_unmap_phyaddr(p);
		remain_size -= span;

		PPMGRVPP_INFO("pos: %lld, phys: %lx, remain_size: %d\n",
			      pos, phys, remain_size);
	}
	return 0;
}

/*
 * 1: yuv
 * 0: afbc
 */
static void notify_data(u32 format)
{
	char *provider_name = vf_get_provider_name(RECEIVER_NAME);

	while (provider_name) {
		if (!vf_get_provider_name(provider_name))
			break;
		provider_name =
			vf_get_provider_name(provider_name);
	}
	if (provider_name)
		vf_notify_provider_by_name(provider_name,
					   VFRAME_EVENT_RECEIVER_NEED_NO_COMP,
					   (void *)&format);
	if (ppmgr_device.ppmgr_debug & 1)
		PPMGRVPP_INFO("%s provider_name: %s, data: %d\n",
			      __func__, provider_name, format);
}

#endif

static void process_vf_rotate(struct vframe_s *vf,
			      struct ge2d_context_s *context,
			      struct config_para_ex_s *ge2d_config)
{
	struct vframe_s *new_vf;
	struct ppframe_s *pp_vf;
	u32 canvas_id;
	u32 dst_canvas_id;
	u32 buf_start, buf_start_ref, buf_end;
	u32 buf_size;
	int ret = 0;
	unsigned int cur_angle = 0;
	int interlace_mode;
	struct file *filp_scr = NULL;
	struct file *filp_dst = NULL;
	char source_path[64];
	char dst_path[64];
	int count;
	int result = 0;
	mm_segment_t old_fs;

	if (ppmgr_device.debug_ppmgr_flag)
		pr_info("ppmgr:rotate\n");

	if (!context) {
		pr_info("ppmgr:rotate but no ge2d context\n");
		return;
	}

	new_vf = vfq_pop(&q_free);

	if (unlikely(!new_vf || !vf)) {
		pr_info("ppmgr:rotate null, %p, %p\n", new_vf, vf);
		return;
	}

	memset(new_vf, 0, sizeof(struct vframe_s));

	interlace_mode = vf->type & VIDTYPE_TYPEMASK;

	pp_vf = to_ppframe(new_vf);
	pp_vf->angle = 0;
	cur_angle = (ppmgr_device.videoangle + vf->orientation) % 4;
	pp_vf->dec_frame = (ppmgr_device.bypass ||
			    (cur_angle == 0 &&
			     ppmgr_device.mirror_flag == 0)) ? vf : NULL;

	if (ppmgr_device.ppmgr_debug & 4)
		need_data_notify = true;

	if (vf->type & VIDTYPE_MVC)
		pp_vf->dec_frame = vf;

	if (vf->type & VIDTYPE_PIC)
		pp_vf->dec_frame = vf;

	/* canvas invalid, force bypass */
	if (!local_canvas_status)
		pp_vf->dec_frame = vf;

	if (vf->type & VIDTYPE_COMPRESS) {
		if (cur_angle != 0 && (vf->type & VIDTYPE_NO_DW)) {
			vf->type &= ~VIDTYPE_SUPPORT_COMPRESS;
			vf->type_original &= ~VIDTYPE_SUPPORT_COMPRESS;
			if (need_data_notify) {
				need_data_notify = false;
				notify_data(1);
				PPMGRVPP_INFO("notify need yuv data\n");
			}
		}
		if (vf->canvas0Addr == (u32)-1 && vf->plane_num == 0) {
			pp_vf->dec_frame = vf;
			if (ppmgr_device.ppmgr_debug & 1)
				PPMGRVPP_INFO("vframe is compress!\n");
		}
		if (dumpfirstframe == 1)
			dumpfirstframe = 2;
	}

	if (pp_vf->dec_frame) {
		if (is_decontour_supported() && local_canvas_status) {
			ret = decontour_pre_process(vf);
			if (ret != 0)
				dc_print("pre process fail ret=%d\n", ret);
		}
		/* bypass mode */
		*new_vf = *vf;
		vfq_push(&q_ready, new_vf);
		return;
	}

	if (!(ppmgr_device.ppmgr_debug & 8)) {
		if (vf->width > vf->height) {
			if (cur_angle % 2) {
				ppmgr_device.disp_width = MAX_HEIGHT;
				ppmgr_device.disp_height = MAX_WIDTH;
			} else {
				ppmgr_device.disp_width = MAX_WIDTH;
				ppmgr_device.disp_height = MAX_HEIGHT;
			}
		} else {
			if (cur_angle % 2) {
				ppmgr_device.disp_width = MAX_WIDTH;
				ppmgr_device.disp_height = MAX_HEIGHT;
			} else {
				ppmgr_device.disp_width = MAX_HEIGHT;
				ppmgr_device.disp_height = MAX_WIDTH;
			}
		}
	}

	ret = ppmgr_buffer_init(vf->mem_sec);

	if (ret < 0) {
		pp_vf->dec_frame = vf;
		*new_vf = *vf;
		vfq_push(&q_ready, new_vf);
		return;
	}
	if (vf->type & VIDTYPE_V4L_EOS) {
		new_vf->type |= VIDTYPE_V4L_EOS;
		goto rotate_done;
	}
#ifdef INTERLACE_DROP_MODE
	if (interlace_mode == VIDTYPE_INTERLACE_BOTTOM) {
		ppmgr_vf_put_dec(vf);
		vfq_push(&q_free, new_vf);
		return;
	}
	pp_vf->angle = cur_angle;
	if (interlace_mode)
		new_vf->duration = vf->duration * 2;
	else
		new_vf->duration = vf->duration;
#else
	pp_vf->angle = cur_angle;
	new_vf->duration = vf->duration;
#endif
	new_vf->ready_jiffies64 = vf->ready_jiffies64;
	new_vf->ready_clock[0] = vf->ready_clock[0];
	new_vf->ready_clock[1] = vf->ready_clock[1];
	new_vf->ready_clock[2] = vf->ready_clock[2];
	new_vf->duration_pulldown = vf->duration_pulldown;
	new_vf->mem_sec = vf->mem_sec;
	new_vf->pts = vf->pts;
	new_vf->pts_us64 = vf->pts_us64;
	new_vf->bitdepth = BITDEPTH_Y8 | BITDEPTH_U8 | BITDEPTH_V8;
	new_vf->signal_type = vf->signal_type;
	new_vf->omx_index = vf->omx_index;
	new_vf->type = VIDTYPE_VIU_444 | VIDTYPE_VIU_SINGLE_PLANE
			| VIDTYPE_VIU_FIELD;
	new_vf->canvas0Addr = -1;
	new_vf->canvas1Addr = -1;
	new_vf->plane_num = 1;
	dst_canvas_id = ppmgr_dst_canvas[0];
	new_vf->orientation = vf->orientation;
	new_vf->flag = vf->flag;

	if (interlace_mode == VIDTYPE_INTERLACE_TOP ||
	    interlace_mode == VIDTYPE_INTERLACE_BOTTOM)
		vf->height >>= 1;

	vf_rotate_adjust(vf, new_vf, cur_angle);

	get_ppmgr_buf_info(&buf_start_ref, &buf_size);
	buf_start = buf_start_ref;
	buf_start += pp_vf->index * ppmgr_device.disp_width
			* ppmgr_device.disp_height * 3;
	buf_end = buf_start;
	buf_end += ppmgr_device.disp_width * ppmgr_device.disp_height * 3;
	if (buf_end > buf_start_ref + buf_size) {
		PPMGRVPP_INFO
			("%s dst overflow, pool:%x %x, buf:%x %x, id:%d\n",
			 __func__,
			 buf_start_ref, buf_size,
			 buf_start, buf_end - buf_start,
			 pp_vf->index);
		buf_start = buf_start_ref;
	}
	new_vf->canvas0_config[0].phy_addr = buf_start;
	new_vf->canvas0_config[0].width = new_vf->width * 3;
	new_vf->canvas0_config[0].height = new_vf->height;
	new_vf->canvas0_config[0].block_mode = 0;
	new_vf->canvas0_config[0].endian = 0;

	canvas_config(dst_canvas_id,
		      (ulong)buf_start,
		      new_vf->width * 3,
		      new_vf->height,
		      CANVAS_ADDR_NOWRAP,
		      CANVAS_BLKMODE_LINEAR);

	memset(ge2d_config, 0, sizeof(struct config_para_ex_s));

	if (pp_vf->index < VF_POOL_SIZE &&
	    buf_status[pp_vf->index].dirty == 1) {
		buf_status[pp_vf->index].dirty = 0;

		ge2d_config->alu_const_color = 0;/*0x000000ff;*/
		ge2d_config->bitmask_en = 0;
		ge2d_config->src1_gb_alpha = 0;/*0xff;*/
		ge2d_config->dst_xy_swap = 0;

		ge2d_config->src_key.key_enable = 0;
		ge2d_config->src_key.key_mask = 0;
		ge2d_config->src_key.key_mode = 0;

		ge2d_config->src_para.canvas_index = dst_canvas_id;
		ge2d_config->src_para.mem_type = CANVAS_TYPE_INVALID;
		ge2d_config->src_para.format = GE2D_FORMAT_S24_YUV444;
		ge2d_config->src_para.fill_color_en = 0;
		ge2d_config->src_para.fill_mode = 0;
		ge2d_config->src_para.x_rev = 0;
		ge2d_config->src_para.y_rev = 0;
		ge2d_config->src_para.color = 0;
		ge2d_config->src_para.top = 0;
		ge2d_config->src_para.left = 0;
		ge2d_config->src_para.width = new_vf->width;
		ge2d_config->src_para.height = new_vf->height;

		ge2d_config->src2_para.mem_type = CANVAS_TYPE_INVALID;

		ge2d_config->dst_para.canvas_index = dst_canvas_id;
		ge2d_config->dst_para.mem_type = CANVAS_TYPE_INVALID;
		ge2d_config->dst_para.format = GE2D_FORMAT_S24_YUV444;
		ge2d_config->dst_para.fill_color_en = 0;
		ge2d_config->dst_para.fill_mode = 0;
		ge2d_config->dst_para.x_rev = 0;
		ge2d_config->dst_para.y_rev = 0;
		ge2d_config->dst_para.color = 0;
		ge2d_config->dst_para.top = 0;
		ge2d_config->dst_para.left = 0;
		ge2d_config->dst_para.width = new_vf->width;
		ge2d_config->dst_para.height = new_vf->height;
		ge2d_config->mem_sec = vf->mem_sec;

		if (ge2d_context_config_ex(context, ge2d_config) < 0) {
			PPMGRVPP_ERR("++ge2d configing error.\n");
			ppmgr_vf_put_dec(vf);
			vfq_push(&q_free, new_vf);
			return;
		}
		fillrect(context, 0, 0,
				new_vf->width, new_vf->height, 0x008080ff);
		memset(ge2d_config, 0, sizeof(struct config_para_ex_s));
	}

	/* data operating. */
	ge2d_config->alu_const_color = 0;/*0x000000ff;*/
	ge2d_config->bitmask_en = 0;
	ge2d_config->src1_gb_alpha = 0;/*0xff;*/
	ge2d_config->dst_xy_swap = 0;

	ge2d_config->src_key.key_enable = 0;
	ge2d_config->src_key.key_mask = 0;
	ge2d_config->src_key.key_mode = 0;
	ge2d_config->src_para.mem_type = CANVAS_TYPE_INVALID;

	if (vf->canvas0Addr != (u32)-1) {
		canvas_copy(vf->canvas0Addr & 0xff,
			ppmgr_src_canvas[0]);
		canvas_copy((vf->canvas0Addr >> 8) & 0xff,
			ppmgr_src_canvas[1]);
		canvas_copy((vf->canvas0Addr >> 16) & 0xff,
			ppmgr_src_canvas[2]);
	} else if (vf->plane_num > 0) {
		canvas_config_config(ppmgr_src_canvas[0],
			&vf->canvas0_config[0]);
		if (vf->plane_num > 1)
			canvas_config_config(ppmgr_src_canvas[1],
				&vf->canvas0_config[1]);
		if (vf->plane_num > 2)
			canvas_config_config(ppmgr_src_canvas[2],
				&vf->canvas0_config[2]);
	}
	canvas_id = ppmgr_src_canvas[0]
		| (ppmgr_src_canvas[1] << 8)
		| (ppmgr_src_canvas[2] << 16);

	ge2d_config->src_para.canvas_index = canvas_id;
	if (ppmgr_color_format == 0)
		ge2d_config->src_para.format = get_input_format(vf);
	else
		ge2d_config->src_para.format = ppmgr_color_format;

	ge2d_config->src_para.fill_color_en = 0;
	ge2d_config->src_para.fill_mode = 0;
	ge2d_config->src_para.x_rev = 0;
	ge2d_config->src_para.y_rev = 0;
	ge2d_config->src_para.color = 0xffffffff;
	ge2d_config->src_para.top = 0;
	ge2d_config->src_para.left = 0;
	ge2d_config->src_para.width = vf->width;
	ge2d_config->src_para.height = vf->height;

	ge2d_config->src2_para.mem_type = CANVAS_TYPE_INVALID;

	ge2d_config->dst_para.canvas_index = dst_canvas_id;
	ge2d_config->dst_para.mem_type = CANVAS_TYPE_INVALID;
	/*ge2d_config->dst_para.mem_type = CANVAS_OSD0;*/
	/*ge2d_config->dst_para.format = GE2D_FORMAT_M24_YUV420;*/
	ge2d_config->dst_para.fill_color_en = 0;
	ge2d_config->dst_para.fill_mode = 0;
	ge2d_config->dst_para.x_rev = 0;
	ge2d_config->dst_para.y_rev = 0;
	ge2d_config->dst_xy_swap = 0;

	if (cur_angle == 1) {
		ge2d_config->dst_xy_swap = 1;
		ge2d_config->dst_para.x_rev = 1;
	} else if (cur_angle == 2) {
		ge2d_config->dst_para.x_rev = 1;
		ge2d_config->dst_para.y_rev = 1;
	} else if (cur_angle == 3) {
		ge2d_config->dst_xy_swap = 1;
		ge2d_config->dst_para.y_rev = 1;
	}
	if (ppmgr_device.mirror_flag != 0) {
		if (ppmgr_device.mirror_flag == 1) {
			if (cur_angle == 2 || cur_angle == 3)
				ge2d_config->dst_para.y_rev = 0;
			else
				ge2d_config->dst_para.y_rev = 1;
		} else if (ppmgr_device.mirror_flag == 2) {
			if (cur_angle == 1 || cur_angle == 2)
				ge2d_config->dst_para.x_rev = 0;
			else
				ge2d_config->dst_para.x_rev = 1;
		}
	}
	ge2d_config->dst_para.color = 0;
	ge2d_config->dst_para.top = 0;
	ge2d_config->dst_para.left = 0;
	if (ppmgr_device.receiver == 0) {
		ge2d_config->dst_para.format = GE2D_FORMAT_S24_YUV444;
		ge2d_config->dst_para.width = new_vf->width;
		ge2d_config->dst_para.height = new_vf->height;
	} else {
		ge2d_config->dst_para.format = ppmgr_device.receiver_format;
		ge2d_config->dst_para.width = ppmgr_device.canvas_width;
		ge2d_config->dst_para.height = ppmgr_device.canvas_height;
		new_vf->width = ppmgr_device.canvas_width;
		new_vf->height = ppmgr_device.canvas_height;
		ge2d_config->dst_para.x_rev = 0;
		ge2d_config->dst_para.y_rev = 0;
		ge2d_config->dst_xy_swap = 0;
	}
	ge2d_config->mem_sec = vf->mem_sec;
	if (ge2d_context_config_ex(context, ge2d_config) < 0) {
		PPMGRVPP_ERR("++ge2d configing error.\n");
		vfq_push(&q_free, new_vf);
		return;
	}

	pp_vf->angle = cur_angle;
	stretchblt_noalpha(context, 0, 0, vf->width, vf->height,
			   0, 0, new_vf->width, new_vf->height);

	if (strstr(ppmgr_device.dump_path, "scr") && dumpfirstframe == 2) {
		old_fs = get_fs();
		set_fs(KERNEL_DS);
		count = strlen(ppmgr_device.dump_path);
		ppmgr_device.dump_path[count] = count_scr;
		sprintf(source_path, "%s_scr", ppmgr_device.dump_path);
		count_scr++;
		filp_scr = filp_open(source_path, O_RDWR | O_CREAT, 0666);
		if (IS_ERR(filp_scr)) {
			PPMGRVPP_INFO("open %s failed\n", source_path);
		} else {
#ifdef CONFIG_AMLOGIC_MEDIA_CODEC_MM
			result = copy_phybuf_to_file(vf->canvas0_config[0]
						     .phy_addr,
						     vf->canvas0_config[0]
						     .width
						     * vf->canvas0_config[0]
						     .height,
						     filp_scr, 0);
#endif
			if (result < 0)
				PPMGRVPP_INFO("write %s failed\n",
					      source_path);
			PPMGRVPP_INFO("scr addr: %lx, width: %d, height: %d\n",
				      vf->canvas0_config[0].phy_addr,
				      vf->canvas0_config[0].width,
				      vf->canvas0_config[0].height);
			PPMGRVPP_INFO("dump source type: %d\n",
				      get_input_format(vf));
			vfs_fsync(filp_scr, 0);
			filp_close(filp_scr, NULL);
			set_fs(old_fs);
		}
	}
rotate_done:
	ppmgr_vf_put_dec(vf);
	new_vf->source_type = VFRAME_SOURCE_TYPE_PPMGR;
	if (dumpfirstframe != 2)
		vfq_push(&q_ready, new_vf);

	if (strstr(ppmgr_device.dump_path, "dst") && dumpfirstframe == 2) {
		old_fs = get_fs();
		set_fs(KERNEL_DS);
		count = strlen(ppmgr_device.dump_path);
		ppmgr_device.dump_path[count] = count_dst;
		sprintf(dst_path, "%s_dst", ppmgr_device.dump_path);
		count_dst++;
		filp_dst = filp_open(dst_path,	O_RDWR | O_CREAT, 0666);
		if (IS_ERR(filp_dst)) {
			PPMGRVPP_INFO("open %s failed\n", dst_path);
		} else {
#ifdef CONFIG_AMLOGIC_MEDIA_CODEC_MM
			ulong phy_addr = new_vf->canvas0_config[0].phy_addr;
			u32 copy_size = new_vf->canvas0_config[0].width *
				new_vf->canvas0_config[0].height;

			result = copy_phybuf_to_file
				(phy_addr, copy_size, filp_dst, 0);
			PPMGRVPP_INFO("dst addr: %lx, width: %d, height: %d\n",
				      phy_addr, new_vf->canvas0_config[0].width,
				      new_vf->canvas0_config[0].height);
			PPMGRVPP_INFO("dump dst type: %d\n",
				      get_input_format(new_vf));
#endif
			if (result < 0)
				PPMGRVPP_INFO("write %s failed\n", dst_path);
			vfs_fsync(filp_dst, 0);
			filp_close(filp_dst, NULL);
			set_fs(old_fs);
		}
		if (count_dst >= ppmgr_device.ppmgr_debug)
			dumpfirstframe = 0;
	}

#ifdef DDD
	PPMGRVPP_WARN("rotate avail=%d, free=%d\n",
		      vfq_level(&q_ready), vfq_level(&q_free));
#endif
}

static void process_vf_change(struct vframe_s *vf,
			      struct ge2d_context_s *context,
			      struct config_para_ex_s *ge2d_config)
{
	static struct vframe_s temp_vf;
	struct ppframe_s *pp_vf = to_ppframe(vf);
	struct canvas_s cs0, cs1, cs2, cd;
	u32 canvas_id;
	u32 dst_canvas_id;
	int interlace_mode;
	unsigned int temp_angle = 0;
	unsigned int cur_angle = 0;
	int ret = 0;
	u32 buf_start, buf_start_ref, buf_end;
	u32 buf_size;

	if (!context || !vf || !local_canvas_status)
		return;

	cur_angle = (ppmgr_device.videoangle + vf->orientation) % 4;
	if (cur_angle == 0 || ppmgr_device.bypass)
		return;

	ret = ppmgr_buffer_init(vf->mem_sec);

	if (ret < 0)
		return;
	if (vf->type & VIDTYPE_V4L_EOS)
		goto change_done;
	temp_vf.duration = vf->duration;
	temp_vf.duration_pulldown = vf->duration_pulldown;
	temp_vf.pts = vf->pts;
	temp_vf.pts_us64 = vf->pts_us64;
	temp_vf.flag = vf->flag;
	temp_vf.bitdepth = BITDEPTH_Y8 | BITDEPTH_U8 | BITDEPTH_V8;
	temp_vf.signal_type = vf->signal_type;
	temp_vf.omx_index = vf->omx_index;
	temp_vf.type = VIDTYPE_VIU_444 | VIDTYPE_VIU_SINGLE_PLANE
			| VIDTYPE_VIU_FIELD;
	temp_vf.canvas0Addr = ass_index;
	temp_vf.canvas1Addr = ass_index;
	temp_angle = (cur_angle >= pp_vf->angle) ?
		(cur_angle - pp_vf->angle) :
		(cur_angle + 4 - pp_vf->angle);

	pp_vf->angle = cur_angle;
	vf_rotate_adjust(vf, &temp_vf, temp_angle);

	get_ppmgr_buf_info(&buf_start_ref, &buf_size);
	buf_start = buf_start_ref;
	buf_start += VF_POOL_SIZE * ppmgr_device.disp_width
			* ppmgr_device.disp_height * 3;
	buf_end = buf_start;
	buf_end += ppmgr_device.disp_width * ppmgr_device.disp_height * 3;
	if (buf_end > buf_start_ref + buf_size) {
		PPMGRVPP_INFO("%s tmp overflow, pool:%x %x, buf:%x %x\n",
			__func__,
			buf_start_ref, buf_size,
			buf_start, buf_end - buf_start);
		return;
	}

	canvas_config(ass_index,
		      (ulong)buf_start,
		      temp_vf.width * 3,
		      temp_vf.height,
		      CANVAS_ADDR_NOWRAP,
		      CANVAS_BLKMODE_LINEAR);

	interlace_mode = vf->type & VIDTYPE_TYPEMASK;

	if (interlace_mode == VIDTYPE_INTERLACE_TOP ||
	    interlace_mode == VIDTYPE_INTERLACE_BOTTOM)
		vf->height >>= 1;

	if (vf->canvas0Addr != (u32)-1) {
		canvas_copy(vf->canvas0Addr & 0xff,
			ppmgr_src_canvas[0]);
		canvas_copy((vf->canvas0Addr >> 8) & 0xff,
			ppmgr_src_canvas[1]);
		canvas_copy((vf->canvas0Addr >> 16) & 0xff,
			ppmgr_src_canvas[2]);
	} else if (vf->plane_num > 0) {
		canvas_config_config(ppmgr_src_canvas[0],
			&vf->canvas0_config[0]);
		if (vf->plane_num > 1)
			canvas_config_config(ppmgr_src_canvas[1],
				&vf->canvas0_config[1]);
		if (vf->plane_num > 2)
			canvas_config_config(ppmgr_src_canvas[2],
				&vf->canvas0_config[2]);
	}
	canvas_id = ppmgr_src_canvas[0]
		| (ppmgr_src_canvas[1] << 8)
		| (ppmgr_src_canvas[2] << 16);

	memset(ge2d_config, 0, sizeof(struct config_para_ex_s));
	/* data operating. */
	ge2d_config->alu_const_color = 0;/*0x000000ff;*/
	ge2d_config->bitmask_en = 0;
	ge2d_config->src1_gb_alpha = 0;/*0xff;*/
	ge2d_config->dst_xy_swap = 0;

	if (pp_vf->dec_frame) {
		canvas_read(canvas_id & 0xff, &cs0);
		canvas_read((canvas_id >> 8) & 0xff, &cs1);
		canvas_read((canvas_id >> 16) & 0xff, &cs2);
		ge2d_config->src_planes[0].addr = cs0.addr;
		ge2d_config->src_planes[0].w = cs0.width;
		ge2d_config->src_planes[0].h = cs0.height;
		ge2d_config->src_planes[1].addr = cs1.addr;
		ge2d_config->src_planes[1].w = cs1.width;
		ge2d_config->src_planes[1].h = cs1.height;
		ge2d_config->src_planes[2].addr = cs2.addr;
		ge2d_config->src_planes[2].w = cs2.width;
		ge2d_config->src_planes[2].h = cs2.height;
		ge2d_config->src_para.format = get_input_format(vf);
	} else {
		canvas_read(canvas_id & 0xff, &cs0);
		ge2d_config->src_planes[0].addr = cs0.addr;
		ge2d_config->src_planes[0].w = cs0.width;
		ge2d_config->src_planes[0].h = cs0.height;
		ge2d_config->src_para.format = get_input_format(vf);
	}

	canvas_read(temp_vf.canvas0Addr & 0xff, &cd);
	ge2d_config->dst_planes[0].addr = cd.addr;
	ge2d_config->dst_planes[0].w = cd.width;
	ge2d_config->dst_planes[0].h = cd.height;

	ge2d_config->src_key.key_enable = 0;
	ge2d_config->src_key.key_mask = 0;
	ge2d_config->src_key.key_mode = 0;

	ge2d_config->src_para.canvas_index = canvas_id;
	ge2d_config->src_para.mem_type = CANVAS_TYPE_INVALID;

	ge2d_config->src_para.fill_color_en = 0;
	ge2d_config->src_para.fill_mode = 0;
	ge2d_config->src_para.x_rev = 0;
	ge2d_config->src_para.y_rev = 0;
	ge2d_config->src_para.color = 0xffffffff;
	ge2d_config->src_para.top = 0;
	ge2d_config->src_para.left = 0;
	ge2d_config->src_para.width = vf->width;
	ge2d_config->src_para.height = vf->height;
	ge2d_config->src2_para.mem_type = CANVAS_TYPE_INVALID;

	ge2d_config->dst_para.canvas_index = temp_vf.canvas0Addr;
	ge2d_config->dst_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->dst_para.format = GE2D_FORMAT_S24_YUV444;
	ge2d_config->dst_para.fill_color_en = 0;
	ge2d_config->dst_para.fill_mode = 0;
	ge2d_config->dst_para.x_rev = 0;
	ge2d_config->dst_para.y_rev = 0;
	ge2d_config->dst_xy_swap = 0;

	ge2d_config->dst_para.color = 0;
	ge2d_config->dst_para.top = 0;
	ge2d_config->dst_para.left = 0;
	ge2d_config->dst_para.width = temp_vf.width;
	ge2d_config->dst_para.height = temp_vf.height;
	ge2d_config->mem_sec = vf->mem_sec;

	if (temp_angle == 1) {
		ge2d_config->dst_xy_swap = 1;
		ge2d_config->dst_para.x_rev = 1;
	} else if (temp_angle == 2) {
		ge2d_config->dst_para.x_rev = 1;
		ge2d_config->dst_para.y_rev = 1;
	} else if (temp_angle == 3) {
		ge2d_config->dst_xy_swap = 1;
		ge2d_config->dst_para.y_rev = 1;
	}
	if (ge2d_context_config_ex(context, ge2d_config) < 0) {
		PPMGRVPP_ERR("++ge2d configing error.\n");
		/*vfq_push(&q_free, new_vf);*/
		return;
	}

	stretchblt_noalpha(context, 0, 0, vf->width,
			   vf->height,
			   0, 0, temp_vf.width,
			   temp_vf.height);

	vf->type = VIDTYPE_VIU_444
			| VIDTYPE_VIU_SINGLE_PLANE
			| VIDTYPE_VIU_FIELD;

	vf->canvas0Addr = -1;
	vf->canvas1Addr = -1;
	vf->plane_num = 1;
	dst_canvas_id = ppmgr_dst_canvas[0];

	get_ppmgr_buf_info(&buf_start_ref, &buf_size);
	buf_start = buf_start_ref;
	buf_start += pp_vf->index * ppmgr_device.disp_width
			* ppmgr_device.disp_height * 3;
	buf_end = buf_start;
	buf_end += ppmgr_device.disp_width * ppmgr_device.disp_height * 3;
	if (buf_end > buf_start_ref + buf_size) {
		PPMGRVPP_INFO
			("%s dst overflow, pool:%x %x, buf:%x %x, id:%d\n",
			 __func__,
			 buf_start_ref, buf_size,
			 buf_start, buf_end - buf_start,
			 pp_vf->index);
		buf_start = buf_start_ref;
	}
	vf->canvas0_config[0].phy_addr = buf_start;
	vf->canvas0_config[0].width = temp_vf.width * 3;
	vf->canvas0_config[0].height = temp_vf.height;
	vf->canvas0_config[0].block_mode = 0;
	vf->canvas0_config[0].endian = 0;

	canvas_config(dst_canvas_id,
		      (ulong)buf_start,
		      temp_vf.width * 3,
		      temp_vf.height,
		      CANVAS_ADDR_NOWRAP,
		      CANVAS_BLKMODE_LINEAR);

	memset(ge2d_config, 0, sizeof(struct config_para_ex_s));
	/* data operating. */
	ge2d_config->alu_const_color = 0;/*0x000000ff;*/
	ge2d_config->bitmask_en = 0;
	ge2d_config->src1_gb_alpha = 0;/*0xff;*/
	ge2d_config->dst_xy_swap = 0;

	canvas_read(temp_vf.canvas0Addr & 0xff, &cd);
	ge2d_config->src_planes[0].addr = cd.addr;
	ge2d_config->src_planes[0].w = cd.width;
	ge2d_config->src_planes[0].h = cd.height;
	ge2d_config->src_para.format = GE2D_FORMAT_S24_YUV444;

	canvas_read(dst_canvas_id & 0xff, &cd);
	ge2d_config->dst_planes[0].addr = cd.addr;
	ge2d_config->dst_planes[0].w = cd.width;
	ge2d_config->dst_planes[0].h = cd.height;

	ge2d_config->src_key.key_enable = 0;
	ge2d_config->src_key.key_mask = 0;
	ge2d_config->src_key.key_mode = 0;

	ge2d_config->src_para.canvas_index = dst_canvas_id;
	ge2d_config->src_para.mem_type = CANVAS_TYPE_INVALID;

	ge2d_config->src_para.fill_color_en = 0;
	ge2d_config->src_para.fill_mode = 0;
	ge2d_config->src_para.x_rev = 0;
	ge2d_config->src_para.y_rev = 0;
	ge2d_config->src_para.color = 0xffffffff;
	ge2d_config->src_para.top = 0;
	ge2d_config->src_para.left = 0;
	ge2d_config->src_para.width = temp_vf.width;
	ge2d_config->src_para.height = temp_vf.height;

	vf->width = temp_vf.width;
	vf->height = temp_vf.height;
	ge2d_config->src2_para.mem_type = CANVAS_TYPE_INVALID;

	ge2d_config->dst_para.canvas_index = vf->canvas0Addr;
	ge2d_config->dst_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->dst_para.format = GE2D_FORMAT_S24_YUV444;
	ge2d_config->dst_para.fill_color_en = 0;
	ge2d_config->dst_para.fill_mode = 0;
	ge2d_config->dst_para.x_rev = 0;
	ge2d_config->dst_para.y_rev = 0;
	ge2d_config->dst_xy_swap = 0;
	ge2d_config->dst_para.color = 0;
	ge2d_config->dst_para.top = 0;
	ge2d_config->dst_para.left = 0;
	ge2d_config->dst_para.width = vf->width;
	ge2d_config->dst_para.height = vf->height;
	ge2d_config->mem_sec = vf->mem_sec;

	if (ge2d_context_config_ex(context, ge2d_config) < 0) {
		PPMGRVPP_ERR("++ge2d configing error.\n");
		/*vfq_push(&q_free, new_vf);*/
		return;
	}
	stretchblt_noalpha(context, 0, 0, temp_vf.width, temp_vf.height, 0, 0,
			   vf->width, vf->height);
	/*vf->duration = 0 ;*/
change_done:
	if (pp_vf->dec_frame) {
		ppmgr_vf_put_dec(pp_vf->dec_frame);
		pp_vf->dec_frame = 0;
	}
	vf->ratio_control = 0;
}

static struct task_struct *task;

void ppmgr_vf_peek_dec_debug(void)
{
	struct vframe_s *vf;

	vf = ppmgr_vf_peek_dec();
	PPMGRVPP_INFO("peek vf=%p\n", vf);
}

static int ppmgr_task(void *data)
{
	struct sched_param param = {.sched_priority = MAX_RT_PRIO - 1};
	struct ge2d_context_s *context = create_ge2d_work_queue();
	struct config_para_ex_s ge2d_config;
#ifdef PPMGR_TB_DETECT
	bool first_frame = true;
	int first_frame_type = 0;
	unsigned int skip_picture = 0;
	u8 cur_invert = 0;
	u8 last_type = 0;
	u32 last_width = 0;
	u32 last_height = 0;
	u8 reset_tb = 0;
	u32 init_mute = 0;
#endif
	unsigned int newvideoangle = 0;
	memset(&ge2d_config, 0, sizeof(struct config_para_ex_s));
	sched_setscheduler(current, SCHED_FIFO, &param);
	allow_signal(SIGTERM);

	while (down_interruptible(&ppmgr_device.ppmgr_sem) == 0) {
		struct vframe_s *vf = NULL;

		if (ppmgr_device.debug_ppmgr_flag)
			PPMGRVPP_INFO("task_1, dec %p, free %d, avail %d\n",
				      ppmgr_vf_peek_dec(),
				      vfq_level(&q_free),
				      vfq_level(&q_ready));

		if (kthread_should_stop() || ppmgr_quit_flag) {
			PPMGRVPP_INFO("task: quit\n");
			break;
		}

		local_canvas_init();

		if (still_picture_notify) {
			still_picture_notify = 0;
			/* disableVideoLayer(); */
#ifdef CONFIG_AMLOGIC_MEDIA_VIDEO
			vf = get_cur_dispbuf();
#endif
			if (!is_valid_ppframe(to_ppframe(vf)))
				continue;
			if ((vf->type & VIDTYPE_COMPRESS) &&
			    vf->plane_num < 1 &&
			    vf->canvas0Addr == (u32)-1) {
				continue;
			}
			process_vf_change(vf, context, &ge2d_config);
#ifdef CONFIG_AMLOGIC_MEDIA_VFM
			vf_notify_receiver(PROVIDER_NAME,
				   VFRAME_EVENT_PROVIDER_PROPERTY_CHANGED,
				   NULL);
#endif
			vfq_lookup_start(&q_ready);
			vf = vfq_peek(&q_ready);

			while (vf) {
				vf = vfq_pop(&q_ready);
				process_vf_change(vf, context, &ge2d_config);
				vf = vfq_peek(&q_ready);
			}
			vfq_lookup_end(&q_ready);
			/* enableVideoLayer(); */
			up(&ppmgr_device.ppmgr_sem);
			continue;
		}

		/* process when we have both input and output space */
		while (ppmgr_vf_peek_dec() &&
		       !vfq_empty(&q_free) && (!ppmgr_blocking)) {
			int ret = 0;

			vf = ppmgr_vf_get_dec();
			if (!vf)
				break;
			if (ppmgr_secure_debug)
				vf->mem_sec = ppmgr_secure_mode;
			if (vf && ppmgr_device.started) {
				ppmgr_device.videoangle =
					(ppmgr_device.angle +
					 ppmgr_device.orientation) % 4;
				set_property_change(1);
				ppmgr_device.started = 0;
			} else {
				newvideoangle = (ppmgr_device.angle + ppmgr_device.orientation) % 4;
				if (newvideoangle == ppmgr_device.videoangle)
					set_property_change(0);
			}
			vf->video_angle = (ppmgr_device.angle
					   + ppmgr_device.orientation
					   + vf->orientation) % 4;
#ifdef PPMGR_TB_DETECT
			if (vf->source_type !=
				VFRAME_SOURCE_TYPE_OTHERS)
				goto SKIP_DETECT;
			if ((vf->width * vf->height)
				> (1920 * 1088) || vf->mem_sec == 1) {
				/* greater than (1920 * 1088) or secure mode,
				 * do not detect
				 */
				goto SKIP_DETECT;
			}
			if (vf->type & VIDTYPE_V4L_EOS)
				goto SKIP_DETECT;
			if (first_frame) {
				last_type = vf->type & VIDTYPE_TYPEMASK;
				last_width = vf->width;
				last_height = vf->height;
				first_frame_type = last_type;
				tb_first_frame_type = last_type;
				first_frame = false;
				reset_tb = 0;
				skip_picture = 0;
				cur_invert = 0;
				init_mute = tb_init_mute;
				atomic_set(&tb_skip_flag, 1);
				atomic_set(&tb_reset_flag, 0);
				if (ppmgr_device.tb_detect & 0xe)
					PPMGRVPP_INFO("tb first type: %d\n",
						      last_type);
			} else if ((last_type ==
				    (vf->type & VIDTYPE_TYPEMASK)) &&
				    last_type) {
				/* interlace seq changed */
				first_frame_type =
					vf->type & VIDTYPE_TYPEMASK;
				tb_first_frame_type =
					first_frame_type;
				reset_tb = 1;
				/* keep old invert */
				if (ppmgr_device.tb_detect & 0xe)
					pr_info("PPMGRVPP: info: tb interlace seq change, old: %d, new: %d, invert: %d\n",
						last_type,
						first_frame_type,
						cur_invert);
			} else if (last_type == 0 &&
				   (vf->type & VIDTYPE_TYPEMASK) != 0) {
				/* prog -> interlace changed */
				first_frame_type =
					vf->type & VIDTYPE_TYPEMASK;
				tb_first_frame_type =
					first_frame_type;
				reset_tb = 1;
				if (ppmgr_device.tb_detect & 0xe)
					pr_info("PPMGRVPP: info: tb prog -> interlace, new type: %d, invert: %d\n",
						first_frame_type, cur_invert);
				/* not invert */
				cur_invert = 0;
			} else if ((last_width != vf->width ||
				    last_height != vf->height) &&
				    (vf->type & VIDTYPE_TYPEMASK) != 0) {
				/* size changed and next seq is interlace */
				first_frame_type =
					vf->type & VIDTYPE_TYPEMASK;
				tb_first_frame_type =
					first_frame_type;
				reset_tb = 1;
				/* keep old invert */
				if (ppmgr_device.tb_detect & 0xe)
					pr_info("PPMGRVPP: info: tb size change new type: %d, invert: %d\n",
						first_frame_type, cur_invert);
			} else if (last_type != 0 &&
				   (vf->type & VIDTYPE_TYPEMASK) == 0) {
				/* interlace -> prog changed */
				if (ppmgr_device.tb_detect & 0xe)
					pr_info("PPMGRVPP: info: tb interlace -> prog, invert: %d\n",
						cur_invert);
				/* not invert */
				cur_invert = 0;
			}
			last_type = vf->type & VIDTYPE_TYPEMASK;
			last_width = vf->width;
			last_height = vf->height;
			if (ppmgr_device.tb_detect) {
				ret = 0;
				if (tb_buffer_status < 0)
					goto SKIP_DETECT;
				if (tb_buffer_status == 0)
					if (tb_buffer_init() <= 0)
						goto SKIP_DETECT;
				ppmgr_device.tb_detect_buf_len = tb_buffer_len;
				vf->type = (vf->type & ~TB_DETECT_MASK);
				if (init_mute > 0) {
					init_mute--;
					atomic_set(&tb_skip_flag, 1);
					goto SKIP_DETECT;
				}
				if (last_type == 0) {/* cur type is prog */
					skip_picture++;
					cur_invert = 0;
					goto SKIP_DETECT;
				}
				vf->type |= cur_invert << TB_DETECT_MASK_BIT;
				if (reset_tb) {
					/* wait tb task done */
					while (tb_buff_wptr >= 5 &&
					       tb_buff_rptr <=
					       (tb_buff_wptr - 5) &&
					       atomic_read(&tb_run_flag) == 1)
						usleep_range(4000, 5000);
					atomic_set(&detect_status, tb_idle);
					tb_buff_wptr = 0;
					tb_buff_rptr = 0;
					atomic_set(&tb_detect_flag,
						   TB_DETECT_NC);
					atomic_set(&tb_reset_flag, 1);
					atomic_set(&tb_skip_flag, 1);
					skip_picture = 0;
					reset_tb  = 0;
					if (ppmgr_device.tb_detect & 0xc)
						PPMGRVPP_INFO("tb reset\n");
				}
				if (atomic_read(&detect_status) == tb_done &&
				    skip_picture >=
				    ppmgr_device.tb_detect_period &&
				    last_type == first_frame_type) {
					int tbf_flag =
						atomic_read(&tb_detect_flag);
					u8 old_invert = cur_invert;

					atomic_set(&detect_status, tb_idle);
					tb_buff_wptr = 0;
					tb_buff_rptr = 0;
					skip_picture = 0;
					if (tbf_flag == TB_DETECT_TBF &&
					    first_frame_type ==
						VIDTYPE_INTERLACE_TOP) {
						/* TBF sams as BFF */
						vf->type |= TB_DETECT_INVERT
							<< TB_DETECT_MASK_BIT;
						cur_invert = 1;
					} else if (tbf_flag == TB_DETECT_BFF &&
						   first_frame_type ==
						   VIDTYPE_INTERLACE_TOP) {
						vf->type |= TB_DETECT_INVERT
							<< TB_DETECT_MASK_BIT;
						cur_invert = 1;
					} else if (tbf_flag == TB_DETECT_TFF &&
						   first_frame_type ==
						   VIDTYPE_INTERLACE_BOTTOM) {
						vf->type |= TB_DETECT_INVERT
							<< TB_DETECT_MASK_BIT;
						cur_invert = 1;
					} else if (tbf_flag != TB_DETECT_NC) {
						cur_invert = 0;
					}
					vf->type = (vf->type & ~TB_DETECT_MASK);
					vf->type |= cur_invert <<
						TB_DETECT_MASK_BIT;
					if (old_invert != cur_invert &&
					    (ppmgr_device.tb_detect & 0xe))
						pr_info("PPMGRVPP: info: tb detect flag: %d->%d, invert: %d->%d\n",
							tb_last_flag,
							tbf_flag,
							old_invert,
							cur_invert);
					else if (tb_last_flag != tbf_flag &&
						 (ppmgr_device.tb_detect
						  & 0xc))
						pr_info("PPMGRVPP: info: tb detect flag %d->%d, invert: %d\n",
							tb_last_flag,
							tbf_flag,
							cur_invert);
					tb_last_flag = tbf_flag;
					atomic_set(&tb_detect_flag,
						   TB_DETECT_NC);
				}
				if (tb_buff_wptr == 0 &&
				    last_type != first_frame_type) {
					skip_picture++;
					atomic_set(&tb_skip_flag, 1);
					if (ppmgr_device.tb_detect & 0xc)
						pr_info("PPMGRVPP: info: tb detect skip case1\n");
					goto SKIP_DETECT;
				}
				if (tb_buff_wptr < tb_buffer_len &&
				    atomic_read(&tb_run_flag) == 1) {
					ret = process_vf_tb_detect(vf, context,
								   &ge2d_config
								   );
				} else {
					if (ppmgr_device.tb_detect & 0xc)
						pr_info("PPMGRVPP: info: tb detect skip case2\n");
					atomic_set(&tb_skip_flag, 1);
					skip_picture++;
				}
				if (ret > 0) {
					tb_buff_wptr++;
					if (tb_buff_wptr >= 5 &&
					    atomic_read(&detect_status)
					    == tb_idle)
						atomic_set(&detect_status,
							   tb_running);
					if (tb_buff_wptr >= 5)
						up(&ppmgr_device.tb_sem);
				}
			} else {
				reset_tb = 1;
				skip_picture++;
				cur_invert = 0;
				if (init_mute > 0)
					init_mute--;
			}
SKIP_DETECT:
			if (skip_picture > ppmgr_device.tb_detect_period)
				skip_picture = ppmgr_device.tb_detect_period;
#endif
			process_vf_rotate(vf, context, &ge2d_config);
#ifdef CONFIG_AMLOGIC_MEDIA_VFM
			vf_notify_receiver(PROVIDER_NAME,
					   VFRAME_EVENT_PROVIDER_VFRAME_READY,
					   NULL);
#endif
		}

		if (ppmgr_blocking) {
			/***recycle buffer to decoder***/
			vf_local_init();
#ifdef CONFIG_AMLOGIC_MEDIA_VFM
			vf_light_unreg_provider(&ppmgr_vf_prov);
#endif
			PPMGRVPP_WARN("ppmgr rebuild light-unregister_1\n");
			vf_unreg_provider(&ppmgr_vf_prov);
			omx_cur_session = 0xffffffff;
			usleep_range(4000, 5000);
			vf_reg_provider(&ppmgr_vf_prov);
			vf_local_init();
			if (is_decontour_supported())
				decontour_buf_reset();
			ppmgr_blocking = false;
			up(&ppmgr_device.ppmgr_sem);
			PPMGRVPP_WARN("ppmgr rebuild light-unregister_2\n");
			PPMGRVPP_WARN("ppmgr, reset, free %d, avail %d\n",
				      vfq_level(&q_free),
				      vfq_level(&q_ready));
		}
		if (ppmgr_device.debug_ppmgr_flag)
			PPMGRVPP_WARN("ppmgr, dec %p, free %d, avail %d\n",
				      ppmgr_vf_peek_dec(),
				      vfq_level(&q_free),
				      vfq_level(&q_ready));

#ifdef DDD
		PPMGRVPP_WARN("process paused, dec %p, free %d, avail %d\n",
			      ppmgr_vf_peek_dec(),
			      vfq_level(&q_free),
			      vfq_level(&q_ready));
#endif
	}

	destroy_ge2d_work_queue(context);
	local_canvas_uninit();

	while (!kthread_should_stop()) {
		/* may not call stop, wait..
		 * it is killed by SIGTERM,eixt on down_interruptible
		 * if not call stop,this thread may on do_exit and
		 * kthread_stop may not work good;
		 */
		/* msleep(10); */
		usleep_range(9000, 10000);
	}
	return 0;
}

/************************************************
 *
 *   init functions.
 *
 *************************************************/
static int vout_notify_callback(struct notifier_block *block,
				unsigned long cmd, void *para)
{
	if (cmd == VOUT_EVENT_MODE_CHANGE)
		ppmgr_device.vinfo = get_current_vinfo();

	return 0;
}

static struct notifier_block vout_notifier = {.notifier_call =
		vout_notify_callback, };
int ppmgr_register(void)
{
	vf_ppmgr_init_provider();
	vf_ppmgr_init_receiver();
	vf_ppmgr_reg_receiver();
	vout_register_client(&vout_notifier);
	return 0;
}

int ppmgr_buffer_uninit(void)
{
	if (!ppmgr_device.use_reserved &&
	    ppmgr_device.buffer_start) {
		PPMGRVPP_INFO("cma free addr is %x , size is  %x\n",
			      (unsigned int)ppmgr_device.buffer_start,
			      (unsigned int)ppmgr_device.buffer_size);
#ifdef CONFIG_AMLOGIC_MEDIA_CODEC_MM
		codec_mm_free_for_dma("ppmgr", ppmgr_device.buffer_start);
#endif
		ppmgr_device.buffer_start = 0;
		ppmgr_device.buffer_size = 0;
	}

	ppmgr_buffer_status = 0;
	return 0;
}

int ppmgr_buffer_init(int secure_mode)
{
	u32 canvas_width, canvas_height;
	int mem_sec_flag;
	int flags;
	struct vinfo_s vinfo = {.width = 1280, .height = 720, };

	mem_sec_flag = secure_mode == 1 ? CODEC_MM_FLAGS_TVP :
					  CODEC_MM_FLAGS_CMA_CLEAR;
	/* int flags = CODEC_MM_FLAGS_DMA; */
	flags = CODEC_MM_FLAGS_DMA | mem_sec_flag;

	switch (ppmgr_buffer_status) {
	case 0:/*not config*/
		break;
	case 1:/*config before , return ok*/
		return 0;
	case 2:/*config fail, won't retry , return failure*/
		return -1;
	default:
		return -1;
	}
	if (ppmgr_device.mirror_flag) {
		PPMGRVPP_INFO("CMA memory force config fail\n");
		ppmgr_buffer_status = 2;
		return -1;
	}
	if (!ppmgr_device.use_reserved &&
	    ppmgr_device.buffer_start == 0) {
		PPMGRVPP_INFO("reserved memory config fail,use CMA.\n");
#ifdef CONFIG_AMLOGIC_MEDIA_CODEC_MM
		ppmgr_device.buffer_start =
			codec_mm_alloc_for_dma("ppmgr",
					       MM_ALLOC_SIZE / PAGE_SIZE,
					       0, flags);
		ppmgr_device.buffer_size = MM_ALLOC_SIZE;
		PPMGRVPP_INFO("cma memory is %x , size is  %x\n",
			      (unsigned int)ppmgr_device.buffer_start,
			      (unsigned int)ppmgr_device.buffer_size);
#endif
		if (ppmgr_device.buffer_start == 0) {
			ppmgr_buffer_status = 2;
			PPMGRVPP_ERR("cma memory config fail\n");
			return -1;
		}
	}

	ppmgr_buffer_status = 1;
	ppmgr_device.vinfo = get_current_vinfo();
	if (IS_ERR_OR_NULL(ppmgr_device.vinfo)) {
		pr_info("PPMGRVPP: info: failed to get_currnt_vinfo! Try to MAKE one!");
		ppmgr_device.vinfo = &vinfo;
	}

	if (ppmgr_device.disp_width == 0) {
		if (ppmgr_device.vinfo->width <= MAX_WIDTH)
			ppmgr_device.disp_width = ppmgr_device.vinfo->width;
		else
			ppmgr_device.disp_width = MAX_WIDTH;
	}

	if (ppmgr_device.disp_height == 0) {
		if (ppmgr_device.vinfo->height <= MAX_HEIGHT)
			ppmgr_device.disp_height = ppmgr_device.vinfo->height;
		else
			ppmgr_device.disp_height = MAX_HEIGHT;
	}
	if (get_platform_type() == PLATFORM_MID_VERTICAL) {
		int DISP_SIZE =
			ppmgr_device.disp_width >
			ppmgr_device.disp_height ?
			ppmgr_device.disp_width :
			ppmgr_device.disp_height;

		canvas_width = (DISP_SIZE + 0x1f) & ~0x1f;
		canvas_height = (DISP_SIZE + 0x1f) & ~0x1f;
	} else {
		canvas_width = (ppmgr_device.disp_width + 0x1f) & ~0x1f;
		canvas_height = (ppmgr_device.disp_height + 0x1f) & ~0x1f;
	}
	ppmgr_device.canvas_width = canvas_width;
	ppmgr_device.canvas_height = canvas_height;

	ppmgr_blocking = false;
	ppmgr_inited = true;
	//up(&ppmgr_device.ppmgr_sem);
	return 0;
}

int start_ppmgr_task(void)
{
	/*    if (get_cpu_type()>= MESON_CPU_TYPE_MESON6)*/
	/*	    switch_mod_gate_by_name("ge2d", 1);*/
	/*#endif*/
#ifdef PPMGR_TB_DETECT
	start_tb_task();
#endif
	if (!task) {
		vf_local_init();
		ppmgr_blocking = false;
		ppmgr_inited = true;
		ppmgr_buffer_status = 0;
		ppmgr_quit_flag = false;
		task = kthread_run(ppmgr_task, 0, "ppmgr");
	}
	if (!IS_ERR_OR_NULL(task))
		task_running = 1;
	else
		task = NULL;
	return 0;
}

void stop_ppmgr_task(void)
{
#ifdef PPMGR_TB_DETECT
	stop_tb_task();
#endif
	if (!IS_ERR_OR_NULL(task)) {
		/* send_sig(SIGTERM, task, 1); */
		ppmgr_quit_flag = true;
		up(&ppmgr_device.ppmgr_sem);
		kthread_stop(task);
		ppmgr_quit_flag = false;
		task = NULL;
	}
	task_running = 0;
	vf_local_init();
	/*#if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON6*/
	/*    switch_mod_gate_by_name("ge2d", 0);*/
	/*#endif*/
}

#ifdef PPMGR_TB_DETECT
static int tb_buffer_init(void)
{
	ulong i;
	//int flags = CODEC_MM_FLAGS_DMA_CPU | CODEC_MM_FLAGS_CMA_CLEAR;
#ifdef CONFIG_AMLOGIC_MEDIA_CODEC_MM
	int flags = 0;
#endif

	if (tb_buffer_status)
		return tb_buffer_status;

	if (ppmgr_src_canvas[0] < 0 || ppmgr_src_canvas[1] < 0 ||
	    ppmgr_src_canvas[2] < 0 ||
	    ppmgr_dst_canvas[0] < 0 || ppmgr_dst_canvas[1] < 0) {
		PPMGRVPP_INFO("%s canvas check fail\n", __func__);
		return -1;
	}

	if (tb_buffer_start == 0) {
		if (!ppmgr_device.tb_detect_buf_len)
			ppmgr_device.tb_detect_buf_len = 8;
		tb_buffer_len = ppmgr_device.tb_detect_buf_len;
		tb_buffer_size = TB_DETECT_H * TB_DETECT_W
			* tb_buffer_len;
		tb_buffer_size = PAGE_ALIGN(tb_buffer_size);
#ifdef CONFIG_AMLOGIC_MEDIA_CODEC_MM
		tb_buffer_start = codec_mm_alloc_for_dma("tb_detect",
							 tb_buffer_size
							 / PAGE_SIZE,
							 0, flags);
		PPMGRVPP_INFO("tb cma memory %x, size %x, item %d\n",
			      (unsigned int)tb_buffer_start,
			      (unsigned int)tb_buffer_size,
			      tb_buffer_len);
#endif
		if (tb_buffer_start == 0) {
			PPMGRVPP_ERR("tb cma memory config fail\n");
			tb_buffer_status = -1;
			return -1;
		}
		for (i = 0; i < tb_buffer_len; i++) {
			detect_buf[i].paddr = tb_buffer_start +
				TB_DETECT_H * TB_DETECT_W * i;
#ifdef CONFIG_AMLOGIC_MEDIA_CODEC_MM
			detect_buf[i].vaddr =
				(ulong)codec_mm_vmap(detect_buf[i].paddr,
						     TB_DETECT_H
						     * TB_DETECT_W);
			if (ppmgr_device.tb_detect & 0xc) {
				PPMGRVPP_INFO("detect buff(%ld)", i);
				PPMGRVPP_INFO(" paddr: %lx, vaddr: %lx\n",
					      detect_buf[i].paddr,
					      detect_buf[i].vaddr);
			}
#endif
		}
	}
	tb_buffer_status = 1;
	return 1;
}

static int tb_buffer_uninit(void)
{
#ifdef CONFIG_AMLOGIC_MEDIA_CODEC_MM
	int i;
#endif

	if (tb_buffer_start) {
#ifdef CONFIG_AMLOGIC_MEDIA_CODEC_MM
		PPMGRVPP_INFO("tb cma free addr is %x, size is %x\n",
			      (unsigned int)tb_buffer_start,
			      (unsigned int)tb_buffer_size);
		for (i = 0; i < tb_buffer_len; i++) {
			if (detect_buf[i].vaddr) {
				codec_mm_unmap_phyaddr((u8 *)detect_buf[i]
						       .vaddr);
				detect_buf[i].vaddr = 0;
			}
		}
		codec_mm_free_for_dma("tb_detect", tb_buffer_start);
#endif
		tb_buffer_start = 0;
		tb_buffer_size = 0;
	}
	tb_buffer_status = 0;
	return 0;
}

static void tb_detect_init(void)
{
	memset(detect_buf, 0, sizeof(detect_buf));
	atomic_set(&detect_status, tb_idle);
	atomic_set(&tb_detect_flag, TB_DETECT_NC);
	atomic_set(&tb_reset_flag, 0);
	atomic_set(&tb_skip_flag, 0);
	atomic_set(&tb_run_flag, 1);
	tb_last_flag = TB_DETECT_NC;
	tb_buff_wptr = 0;
	tb_buff_rptr = 0;
	tb_buffer_status = 0;
	tb_buffer_start = 0;
	tb_buffer_size = 0;
	tb_first_frame_type = 0;
	tb_quit_flag = false;
	tb_init_mute =
		ppmgr_device.tb_detect_init_mute;
}

static int tb_task(void *data)
{
	int tbff_flag;
	struct tbff_stats *tb_reg = NULL;
	ulong y5fld[5];
	int is_top;
	int inited = 0;
	int i;
	int inter_flag;
	static const char * const detect_type[] = {"NC", "TFF", "BFF", "TBF"};
	struct sched_param param = {.sched_priority = MAX_RT_PRIO - 1};

	sched_setscheduler(current, SCHED_FIFO, &param);

	inter_flag = 0;
	tb_reg = kmalloc(sizeof(*tb_reg), GFP_KERNEL);
	if (!tb_reg) {
		PPMGRVPP_INFO("tb_reg malloc fail\n");
		return 0;
	}
	memset(tb_reg, 0, sizeof(struct tbff_stats));

	if (gfunc)
		gfunc->stats_init(tb_reg, TB_DETECT_H, TB_DETECT_W);
	allow_signal(SIGTERM);
	while (down_interruptible(&ppmgr_device.tb_sem) == 0) {
		if (kthread_should_stop() || tb_quit_flag)
			break;
		if (tb_buff_rptr == 0) {
			if (atomic_read(&tb_reset_flag) != 0)
				inited = 0;
			atomic_set(&tb_reset_flag, 0);
			if (gfunc)
				gfunc->fwalg_init(inited);
		}
		inited = 1;
		is_top = (tb_buff_rptr & 1) ? 0 : 1;
		/* new -> old */
		y5fld[0] = detect_buf[tb_buff_rptr + 4].vaddr;
		y5fld[1] = detect_buf[tb_buff_rptr + 3].vaddr;
		y5fld[2] = detect_buf[tb_buff_rptr + 2].vaddr;
		y5fld[3] = detect_buf[tb_buff_rptr + 1].vaddr;
		y5fld[4] = detect_buf[tb_buff_rptr].vaddr;
		if (gfunc) {
			if (IS_ERR_OR_NULL(tb_reg)) {
				kfree(tb_reg);
				PPMGRVPP_INFO("tb_reg is NULL!\n");
				return 0;
			}
			for (i = 0; i < 5; i++) {
				if (IS_ERR_OR_NULL((void *)y5fld[i])) {
					PPMGRVPP_INFO("y5fld[%d] is NULL!\n",
						      i);
					inter_flag = 1;
					break;
				}
			}
			if (inter_flag) {
				inter_flag = 0;
				continue;
			}
			gfunc->stats_get(y5fld, tb_reg);
		}
		is_top = is_top ^ 1;
		tbff_flag = -1;
		if (gfunc)
			tbff_flag = gfunc->fwalg_get(tb_reg, is_top,
						    (tb_first_frame_type
						     == 3) ? 0 : 1,
						    tb_buff_rptr,
						    atomic_read(&tb_skip_flag),
						    (ppmgr_device.tb_detect
						     & 0x8) ? 1 : 0);

		if (tb_buff_rptr == 0)
			atomic_set(&tb_skip_flag, 0);

		if (tbff_flag < -1 || tbff_flag > 2) {
			PPMGRVPP_ERR("get tb detect flag error: %d\n",
				     tbff_flag);
		}

		if (tbff_flag == -1 && gfunc)
			tbff_flag = gfunc->majority_get();

		if (tbff_flag == -1)
			tbff_flag = TB_DETECT_NC;
		else if (tbff_flag == 0)
			tbff_flag = TB_DETECT_TFF;
		else if (tbff_flag == 1)
			tbff_flag = TB_DETECT_BFF;
		else if (tbff_flag == 2)
			tbff_flag = TB_DETECT_TBF;
		else
			tbff_flag = TB_DETECT_NC;
		tb_buff_rptr++;
		if (tb_buff_rptr > (tb_buffer_len - 5) &&
		    atomic_read(&detect_status) == tb_running) {
			atomic_set(&tb_detect_flag, tbff_flag);
			if (ppmgr_device.tb_detect & 0xc)
				PPMGRVPP_INFO("get tb detect final flag: %s\n",
					      detect_type[tbff_flag]);
			atomic_set(&detect_status, tb_done);
		}
	}
	atomic_set(&tb_run_flag, 0);
	kfree(tb_reg);
	while (!kthread_should_stop())
		usleep_range(9000, 10000);
	return 0;
}

int start_tb_task(void)
{
	if (!tb_detect_task) {
		tb_detect_init();
		tb_detect_task = kthread_run(tb_task, 0, "tb_detect");
	}
	if (!IS_ERR_OR_NULL(tb_detect_task))
		tb_task_running = 1;
	else
		tb_detect_task = NULL;
	return 0;
}

void stop_tb_task(void)
{
	if (!IS_ERR_OR_NULL(tb_detect_task)) {
		tb_quit_flag = true;
		up(&ppmgr_device.tb_sem);
		/* send_sig(SIGTERM, tb_detect_task, 1); */
		kthread_stop(tb_detect_task);
		tb_quit_flag = false;
		//sema_init(&tb_sem, val);
		tb_detect_task = NULL;
	}
	tb_task_running = 0;
}
#endif

void get_tb_detect_status(void)
{
#ifdef PPMGR_TB_DETECT
	static const char * const tb_type[] = {"Prog", "Top", "N/C", "Bottom"};
	static const char * const detect_type[] = {"NC", "TFF", "BFF", "TBF"};
	static const char * const status_str[] = {
		"Idle", "Run", "Done", "N/C"};
	u32 status = atomic_read(&detect_status);
	u32 flag = atomic_read(&tb_detect_flag);
	u32 reset_flag = atomic_read(&tb_reset_flag);

	PPMGRVPP_INFO("T/B detect buffer addr: 0x%x, size: 0x%x\n",
		      (unsigned int)tb_buffer_start,
		      (unsigned int)tb_buffer_size);
	PPMGRVPP_INFO("T/B detect canvas: %x, len: %d, buff status: %d\n",
		      (unsigned int)ppmgr_dst_canvas[0],
		      tb_buffer_len,
		      (unsigned int)tb_buffer_status);
	PPMGRVPP_INFO("T/B detect buffer wptr: %d, rptr: %d, reset: %d\n",
		      tb_buff_wptr, tb_buff_rptr, reset_flag);
	PPMGRVPP_INFO("T/B detect first frame type: %s, period: %d\n",
		      tb_type[tb_first_frame_type],
		      ppmgr_device.tb_detect_period);
	PPMGRVPP_INFO("T/B detect status: %s, cur flag: %s, last flag: %s\n",
		      status_str[status], detect_type[flag],
		      detect_type[tb_last_flag]);
	PPMGRVPP_INFO("T/B detect init mute is %d\n", tb_init_mute);
	PPMGRVPP_INFO("T/B detect tb_detect_task: %p, running: %d\n",
		      tb_detect_task, tb_task_running);
	PPMGRVPP_INFO("current T/B detect mode is %d\n",
		      ppmgr_device.tb_detect);
#endif
}

int RegisterTB_Function(struct TB_DetectFuncPtr *func, const char *ver)
{
	int ret = -1;
#ifdef PPMGR_TB_DETECT
	mutex_lock(&tb_mutex);
	PPMGRVPP_INFO("%s: gfunc %p, func: %p, ver:%s\n",
		      __func__, gfunc, func, ver);
	if (!gfunc && func) {
		gfunc = func;
		ret = 0;
	}
	mutex_unlock(&tb_mutex);
#endif
	return ret;
}
EXPORT_SYMBOL(RegisterTB_Function);

int UnRegisterTB_Function(struct TB_DetectFuncPtr *func)
{
	int ret = -1;
#ifdef PPMGR_TB_DETECT
	mutex_lock(&tb_mutex);
	PPMGRVPP_INFO("%s: gfunc %p, func: %p\n",
		      __func__, gfunc, func);
	if (func && func == gfunc) {
		gfunc = NULL;
		ret = 0;
	}
	mutex_unlock(&tb_mutex);
#endif
	return ret;
}
EXPORT_SYMBOL(UnRegisterTB_Function);

