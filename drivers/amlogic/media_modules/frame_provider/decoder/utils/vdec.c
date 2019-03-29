/*
 * drivers/amlogic/media/frame_provider/decoder/utils/vdec.c
 *
 * Copyright (C) 2016 Amlogic, Inc. All rights reserved.
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
#define DEBUG
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>
#include <linux/semaphore.h>
#include <linux/sched/rt.h>
#include <linux/interrupt.h>
#include <linux/amlogic/media/utils/vformat.h>
#include <linux/amlogic/iomap.h>
#include <linux/amlogic/media/canvas/canvas.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>
#include <linux/amlogic/media/video_sink/ionvideo_ext.h>
#include <linux/amlogic/media/vfm/vfm_ext.h>
/*for VDEC_DEBUG_SUPPORT*/
#include <linux/time.h>

#include <linux/amlogic/media/utils/vdec_reg.h>
#include "vdec.h"
#include "vdec_trace.h"
#ifdef CONFIG_AMLOGIC_MEDIA_MULTI_DEC
#include "vdec_profile.h"
#endif
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/libfdt_env.h>
#include <linux/of_reserved_mem.h>
#include <linux/dma-contiguous.h>
#include <linux/cma.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/dma-contiguous.h>
#include "../../../stream_input/amports/amports_priv.h"

#include <linux/amlogic/media/utils/amports_config.h>
#include "../utils/amvdec.h"
#include "vdec_input.h"

#include "../../../common/media_clock/clk/clk.h"
#include <linux/reset.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/amlogic/media/video_sink/video_keeper.h>
#include <linux/amlogic/media/codec_mm/configs.h>
#include <linux/amlogic/media/frame_sync/ptsserv.h>
#include "secprot.h"
#include "../../../common/chips/decoder_cpu_ver_info.h"
#include "frame_check.h"

static DEFINE_MUTEX(vdec_mutex);

#define MC_SIZE (4096 * 4)
#define CMA_ALLOC_SIZE SZ_64M
#define MEM_NAME "vdec_prealloc"
static int inited_vcodec_num;
#define jiffies_ms div64_u64(get_jiffies_64() * 1000, HZ)
static int poweron_clock_level;
static int keep_vdec_mem;
static unsigned int debug_trace_num = 16 * 20;
static int step_mode;
static unsigned int clk_config;
/*
   &1: sched_priority to MAX_RT_PRIO -1.
   &2: always reload firmware.
   &4: vdec canvas debug enable
  */
static unsigned int debug;

static int hevc_max_reset_count;

static int no_powerdown;
static int parallel_decode = 1;
static int fps_detection;
static int fps_clear;
static DEFINE_SPINLOCK(vdec_spin_lock);

#define HEVC_TEST_LIMIT 100
#define GXBB_REV_A_MINOR 0xA

#define CANVAS_MAX_SIZE (AMVDEC_CANVAS_MAX1 - AMVDEC_CANVAS_START_INDEX + 1 + AMVDEC_CANVAS_MAX2 + 1)

struct am_reg {
	char *name;
	int offset;
};

struct vdec_isr_context_s {
	int index;
	int irq;
	irq_handler_t dev_isr;
	irq_handler_t dev_threaded_isr;
	void *dev_id;
	struct vdec_s *vdec;
};

struct decode_fps_s {
	u32 frame_count;
	u64 start_timestamp;
	u64 last_timestamp;
	u32 fps;
};

struct vdec_core_s {
	struct list_head connected_vdec_list;
	spinlock_t lock;
	spinlock_t canvas_lock;
	spinlock_t fps_lock;
	struct ida ida;
	atomic_t vdec_nr;
	struct vdec_s *vfm_vdec;
	struct vdec_s *active_vdec;
	struct vdec_s *active_hevc;
	struct vdec_s *hint_fr_vdec;
	struct platform_device *vdec_core_platform_device;
	struct device *cma_dev;
	struct semaphore sem;
	struct task_struct *thread;
	struct workqueue_struct *vdec_core_wq;

	unsigned long sched_mask;
	struct vdec_isr_context_s isr_context[VDEC_IRQ_MAX];
	int power_ref_count[VDEC_MAX];
	struct vdec_s *last_vdec;
	int parallel_dec;
	unsigned long power_ref_mask;
	int vdec_combine_flag;
	struct decode_fps_s decode_fps[MAX_INSTANCE_MUN];
};

struct canvas_status_s {
	int type;
	int canvas_used_flag;
	int id;
};


static struct vdec_core_s *vdec_core;

static const char * const vdec_status_string[] = {
	"VDEC_STATUS_UNINITIALIZED",
	"VDEC_STATUS_DISCONNECTED",
	"VDEC_STATUS_CONNECTED",
	"VDEC_STATUS_ACTIVE"
};

static int debugflags;

static struct canvas_status_s canvas_stat[AMVDEC_CANVAS_MAX1 - AMVDEC_CANVAS_START_INDEX + 1 + AMVDEC_CANVAS_MAX2 + 1];


int vdec_get_debug_flags(void)
{
	return debugflags;
}
EXPORT_SYMBOL(vdec_get_debug_flags);

unsigned char is_mult_inc(unsigned int type)
{
	unsigned char ret = 0;
	if (vdec_get_debug_flags() & 0xf000)
		ret = (vdec_get_debug_flags() & 0x1000)
			? 1 : 0;
	else if (type & PORT_TYPE_DECODER_SCHED)
		ret = 1;
	return ret;
}
EXPORT_SYMBOL(is_mult_inc);

static const bool cores_with_input[VDEC_MAX] = {
	true,   /* VDEC_1 */
	false,  /* VDEC_HCODEC */
	false,  /* VDEC_2 */
	true,   /* VDEC_HEVC / VDEC_HEVC_FRONT */
	false,  /* VDEC_HEVC_BACK */
};

static const int cores_int[VDEC_MAX] = {
	VDEC_IRQ_1,
	VDEC_IRQ_2,
	VDEC_IRQ_0,
	VDEC_IRQ_0,
	VDEC_IRQ_HEVC_BACK
};

unsigned long vdec_canvas_lock(struct vdec_core_s *core)
{
	unsigned long flags;
	spin_lock_irqsave(&core->canvas_lock, flags);

	return flags;
}

void vdec_canvas_unlock(struct vdec_core_s *core, unsigned long flags)
{
	spin_unlock_irqrestore(&core->canvas_lock, flags);
}

unsigned long vdec_fps_lock(struct vdec_core_s *core)
{
	unsigned long flags;
	spin_lock_irqsave(&core->fps_lock, flags);

	return flags;
}

void vdec_fps_unlock(struct vdec_core_s *core, unsigned long flags)
{
	spin_unlock_irqrestore(&core->fps_lock, flags);
}

unsigned long vdec_core_lock(struct vdec_core_s *core)
{
	unsigned long flags;

	spin_lock_irqsave(&core->lock, flags);

	return flags;
}

void vdec_core_unlock(struct vdec_core_s *core, unsigned long flags)
{
	spin_unlock_irqrestore(&core->lock, flags);
}

static u64 vdec_get_us_time_system(void)
{
	struct timeval tv;

	do_gettimeofday(&tv);

	return div64_u64(timeval_to_ns(&tv), 1000);
}

static void vdec_fps_clear(int id)
{
	if (id >= MAX_INSTANCE_MUN)
		return;

	vdec_core->decode_fps[id].frame_count = 0;
	vdec_core->decode_fps[id].start_timestamp = 0;
	vdec_core->decode_fps[id].last_timestamp = 0;
	vdec_core->decode_fps[id].fps = 0;
}

static void vdec_fps_clearall(void)
{
	int i;

	for (i = 0; i < MAX_INSTANCE_MUN; i++) {
		vdec_core->decode_fps[i].frame_count = 0;
		vdec_core->decode_fps[i].start_timestamp = 0;
		vdec_core->decode_fps[i].last_timestamp = 0;
		vdec_core->decode_fps[i].fps = 0;
	}
}

static void vdec_fps_detec(int id)
{
	unsigned long flags;

	if (fps_detection == 0)
		return;

	if (id >= MAX_INSTANCE_MUN)
		return;

	flags = vdec_fps_lock(vdec_core);

	if (fps_clear == 1) {
		vdec_fps_clearall();
		fps_clear = 0;
	}

	vdec_core->decode_fps[id].frame_count++;
	if (vdec_core->decode_fps[id].frame_count == 1) {
		vdec_core->decode_fps[id].start_timestamp =
			vdec_get_us_time_system();
		vdec_core->decode_fps[id].last_timestamp =
			vdec_core->decode_fps[id].start_timestamp;
	} else {
		vdec_core->decode_fps[id].last_timestamp =
			vdec_get_us_time_system();
		vdec_core->decode_fps[id].fps =
				(u32)div_u64(((u64)(vdec_core->decode_fps[id].frame_count) *
					10000000000),
					(vdec_core->decode_fps[id].last_timestamp -
					vdec_core->decode_fps[id].start_timestamp));
	}
	vdec_fps_unlock(vdec_core, flags);
}



static int get_canvas(unsigned int index, unsigned int base)
{
	int start;
	int canvas_index = index * base;
	int ret;

	if ((base > 4) || (base == 0))
		return -1;

	if ((AMVDEC_CANVAS_START_INDEX + canvas_index + base - 1)
		<= AMVDEC_CANVAS_MAX1) {
		start = AMVDEC_CANVAS_START_INDEX + base * index;
	} else {
		canvas_index -= (AMVDEC_CANVAS_MAX1 -
			AMVDEC_CANVAS_START_INDEX + 1) / base * base;
		if (canvas_index <= AMVDEC_CANVAS_MAX2)
			start = canvas_index / base;
		else
			return -1;
	}

	if (base == 1) {
		ret = start;
	} else if (base == 2) {
		ret = ((start + 1) << 16) | ((start + 1) << 8) | start;
	} else if (base == 3) {
		ret = ((start + 2) << 16) | ((start + 1) << 8) | start;
	} else if (base == 4) {
		ret = (((start + 3) << 24) | (start + 2) << 16) |
			((start + 1) << 8) | start;
	}

	return ret;
}

static int get_canvas_ex(int type, int id)
{
	int i;
	unsigned long flags;

	flags = vdec_canvas_lock(vdec_core);

	for (i = 0; i < CANVAS_MAX_SIZE; i++) {
		/*0x10-0x15 has been used by rdma*/
		if ((i >= 0x10) && (i <= 0x15))
				continue;
		if ((canvas_stat[i].type == type) &&
			(canvas_stat[i].id & (1 << id)) == 0) {
			canvas_stat[i].canvas_used_flag++;
			canvas_stat[i].id |= (1 << id);
			if (debug & 4)
				pr_debug("get used canvas %d\n", i);
			vdec_canvas_unlock(vdec_core, flags);
			if (i < AMVDEC_CANVAS_MAX2 + 1)
				return i;
			else
				return (i + AMVDEC_CANVAS_START_INDEX - AMVDEC_CANVAS_MAX2 - 1);
		}
	}

	for (i = 0; i < CANVAS_MAX_SIZE; i++) {
		/*0x10-0x15 has been used by rdma*/
		if ((i >= 0x10) && (i <= 0x15))
				continue;
		if (canvas_stat[i].type == 0) {
			canvas_stat[i].type = type;
			canvas_stat[i].canvas_used_flag = 1;
			canvas_stat[i].id = (1 << id);
			if (debug & 4) {
				pr_debug("get canvas %d\n", i);
				pr_debug("canvas_used_flag %d\n",
					canvas_stat[i].canvas_used_flag);
				pr_debug("canvas_stat[i].id %d\n",
					canvas_stat[i].id);
			}
			vdec_canvas_unlock(vdec_core, flags);
			if (i < AMVDEC_CANVAS_MAX2 + 1)
				return i;
			else
				return (i + AMVDEC_CANVAS_START_INDEX - AMVDEC_CANVAS_MAX2 - 1);
		}
	}
	vdec_canvas_unlock(vdec_core, flags);

	pr_info("cannot get canvas\n");

	return -1;
}

static void free_canvas_ex(int index, int id)
{
	unsigned long flags;
	int offset;

	flags = vdec_canvas_lock(vdec_core);
	if (index >= 0 &&
		index < AMVDEC_CANVAS_MAX2 + 1)
		offset = index;
	else if ((index >= AMVDEC_CANVAS_START_INDEX) &&
		(index <= AMVDEC_CANVAS_MAX1))
		offset = index + AMVDEC_CANVAS_MAX2 + 1 - AMVDEC_CANVAS_START_INDEX;
	else {
		vdec_canvas_unlock(vdec_core, flags);
		return;
	}

	if ((canvas_stat[offset].canvas_used_flag > 0) &&
		(canvas_stat[offset].id & (1 << id))) {
		canvas_stat[offset].canvas_used_flag--;
		canvas_stat[offset].id &= ~(1 << id);
		if (canvas_stat[offset].canvas_used_flag == 0) {
			canvas_stat[offset].type = 0;
			canvas_stat[offset].id = 0;
		}
		if (debug & 4) {
			pr_debug("free index %d used_flag %d, type = %d, id = %d\n",
				offset,
				canvas_stat[offset].canvas_used_flag,
				canvas_stat[offset].type,
				canvas_stat[offset].id);
		}
	}
	vdec_canvas_unlock(vdec_core, flags);

	return;

}




static int vdec_get_hw_type(int value)
{
	int type;
	switch (value) {
		case VFORMAT_HEVC:
		case VFORMAT_VP9:
		case VFORMAT_AVS2:
			type = CORE_MASK_HEVC;
		break;

		case VFORMAT_MPEG12:
		case VFORMAT_MPEG4:
		case VFORMAT_H264:
		case VFORMAT_MJPEG:
		case VFORMAT_REAL:
		case VFORMAT_JPEG:
		case VFORMAT_VC1:
		case VFORMAT_AVS:
		case VFORMAT_YUV:
		case VFORMAT_H264MVC:
		case VFORMAT_H264_4K2K:
		case VFORMAT_H264_ENC:
		case VFORMAT_JPEG_ENC:
			type = CORE_MASK_VDEC_1;
		break;

		default:
			type = -1;
	}

	return type;
}


static void vdec_save_active_hw(struct vdec_s *vdec)
{
	int type;

	type = vdec_get_hw_type(vdec->port->vformat);

	if (type == CORE_MASK_HEVC) {
		vdec_core->active_hevc = vdec;
	} else if (type == CORE_MASK_VDEC_1) {
		vdec_core->active_vdec = vdec;
	} else {
		pr_info("save_active_fw wrong\n");
	}
}


int vdec_status(struct vdec_s *vdec, struct vdec_info *vstatus)
{
	if (vdec && vdec->dec_status &&
		((vdec->status == VDEC_STATUS_CONNECTED ||
		vdec->status == VDEC_STATUS_ACTIVE)))
		return vdec->dec_status(vdec, vstatus);

	return 0;
}
EXPORT_SYMBOL(vdec_status);

int vdec_set_trickmode(struct vdec_s *vdec, unsigned long trickmode)
{
	int r;

	if (vdec->set_trickmode) {
		r = vdec->set_trickmode(vdec, trickmode);

		if ((r == 0) && (vdec->slave) && (vdec->slave->set_trickmode))
			r = vdec->slave->set_trickmode(vdec->slave,
				trickmode);
		return r;
	}

	return -1;
}
EXPORT_SYMBOL(vdec_set_trickmode);

int vdec_set_isreset(struct vdec_s *vdec, int isreset)
{
	vdec->is_reset = isreset;
	pr_info("is_reset=%d\n", isreset);
	if (vdec->set_isreset)
		return vdec->set_isreset(vdec, isreset);
	return 0;
}
EXPORT_SYMBOL(vdec_set_isreset);

int vdec_set_dv_metawithel(struct vdec_s *vdec, int isdvmetawithel)
{
	vdec->dolby_meta_with_el = isdvmetawithel;
	pr_info("isdvmetawithel=%d\n", isdvmetawithel);
	return 0;
}
EXPORT_SYMBOL(vdec_set_dv_metawithel);

void vdec_set_no_powerdown(int flag)
{
	no_powerdown = flag;
	pr_info("no_powerdown=%d\n", no_powerdown);
	return;
}
EXPORT_SYMBOL(vdec_set_no_powerdown);

void  vdec_count_info(struct vdec_info *vs, unsigned int err,
	unsigned int offset)
{
	if (err)
		vs->error_frame_count++;
	if (offset) {
		if (0 == vs->frame_count) {
			vs->offset = 0;
			vs->samp_cnt = 0;
		}
		vs->frame_data = offset > vs->total_data ?
			offset - vs->total_data : vs->total_data - offset;
		vs->total_data = offset;
		if (vs->samp_cnt < 96000 * 2) { /* 2s */
			if (0 == vs->samp_cnt)
				vs->offset = offset;
			vs->samp_cnt += vs->frame_dur;
		} else {
			vs->bit_rate = (offset - vs->offset) / 2;
			/*pr_info("bitrate : %u\n",vs->bit_rate);*/
			vs->samp_cnt = 0;
		}
		vs->frame_count++;
	}
	/*pr_info("size : %u, offset : %u, dur : %u, cnt : %u\n",
		vs->offset,offset,vs->frame_dur,vs->samp_cnt);*/
	return;
}
EXPORT_SYMBOL(vdec_count_info);
int vdec_is_support_4k(void)
{
	return !is_meson_gxl_package_805X();
}
EXPORT_SYMBOL(vdec_is_support_4k);

/*
 * clk_config:
 *0:default
 *1:no gp0_pll;
 *2:always used gp0_pll;
 *>=10:fixed n M clk;
 *== 100 , 100M clks;
 */
unsigned int get_vdec_clk_config_settings(void)
{
	return clk_config;
}
void update_vdec_clk_config_settings(unsigned int config)
{
	clk_config = config;
}
EXPORT_SYMBOL(update_vdec_clk_config_settings);

static bool hevc_workaround_needed(void)
{
	return (get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_GXBB) &&
		(get_meson_cpu_version(MESON_CPU_VERSION_LVL_MINOR)
			== GXBB_REV_A_MINOR);
}

struct device *get_codec_cma_device(void)
{
	return vdec_core->cma_dev;
}

#ifdef CONFIG_AMLOGIC_MEDIA_MULTI_DEC
static const char * const vdec_device_name[] = {
	"amvdec_mpeg12",     "ammvdec_mpeg12",
	"amvdec_mpeg4",      "ammvdec_mpeg4",
	"amvdec_h264",       "ammvdec_h264",
	"amvdec_mjpeg",      "ammvdec_mjpeg",
	"amvdec_real",       "ammvdec_real",
	"amjpegdec",         "ammjpegdec",
	"amvdec_vc1",        "ammvdec_vc1",
	"amvdec_avs",        "ammvdec_avs",
	"amvdec_yuv",        "ammvdec_yuv",
	"amvdec_h264mvc",    "ammvdec_h264mvc",
	"amvdec_h264_4k2k",  "ammvdec_h264_4k2k",
	"amvdec_h265",       "ammvdec_h265",
	"amvenc_avc",        "amvenc_avc",
	"jpegenc",           "jpegenc",
	"amvdec_vp9",        "ammvdec_vp9",
	"amvdec_avs2",       "ammvdec_avs2"
};


#else

static const char * const vdec_device_name[] = {
	"amvdec_mpeg12",
	"amvdec_mpeg4",
	"amvdec_h264",
	"amvdec_mjpeg",
	"amvdec_real",
	"amjpegdec",
	"amvdec_vc1",
	"amvdec_avs",
	"amvdec_yuv",
	"amvdec_h264mvc",
	"amvdec_h264_4k2k",
	"amvdec_h265",
	"amvenc_avc",
	"jpegenc",
	"amvdec_vp9",
	"amvdec_avs2"
};

#endif

#ifdef VDEC_DEBUG_SUPPORT
static u64 get_current_clk(void)
{
	/*struct timespec xtime = current_kernel_time();
	u64 usec = xtime.tv_sec * 1000000;
	usec += xtime.tv_nsec / 1000;
	*/
	u64 usec = sched_clock();
	return usec;
}

static void inc_profi_count(unsigned long mask, u32 *count)
{
	enum vdec_type_e type;

	for (type = VDEC_1; type < VDEC_MAX; type++) {
		if (mask & (1 << type))
			count[type]++;
	}
}

static void update_profi_clk_run(struct vdec_s *vdec,
	unsigned long mask, u64 clk)
{
	enum vdec_type_e type;

	for (type = VDEC_1; type < VDEC_MAX; type++) {
		if (mask & (1 << type)) {
			vdec->start_run_clk[type] = clk;
			if (vdec->profile_start_clk[type] == 0)
				vdec->profile_start_clk[type] = clk;
			vdec->total_clk[type] = clk
				- vdec->profile_start_clk[type];
			/*pr_info("set start_run_clk %ld\n",
			vdec->start_run_clk);*/

		}
	}
}

static void update_profi_clk_stop(struct vdec_s *vdec,
	unsigned long mask, u64 clk)
{
	enum vdec_type_e type;

	for (type = VDEC_1; type < VDEC_MAX; type++) {
		if (mask & (1 << type)) {
			if (vdec->start_run_clk[type] == 0)
				pr_info("error, start_run_clk[%d] not set\n", type);

			/*pr_info("update run_clk type %d, %ld, %ld, %ld\n",
				type,
				clk,
				vdec->start_run_clk[type],
				vdec->run_clk[type]);*/
			vdec->run_clk[type] +=
				(clk - vdec->start_run_clk[type]);
		}
	}
}

#endif

int vdec_set_decinfo(struct vdec_s *vdec, struct dec_sysinfo *p)
{
	if (copy_from_user((void *)&vdec->sys_info_store, (void *)p,
		sizeof(struct dec_sysinfo)))
		return -EFAULT;

	return 0;
}
EXPORT_SYMBOL(vdec_set_decinfo);

/* construct vdec strcture */
struct vdec_s *vdec_create(struct stream_port_s *port,
			struct vdec_s *master)
{
	struct vdec_s *vdec;
	int type = VDEC_TYPE_SINGLE;
	int id;
	if (is_mult_inc(port->type))
		type = (port->type & PORT_TYPE_FRAME) ?
			VDEC_TYPE_FRAME_BLOCK :
			VDEC_TYPE_STREAM_PARSER;

	id = ida_simple_get(&vdec_core->ida,
			0, MAX_INSTANCE_MUN, GFP_KERNEL);
	if (id < 0) {
		pr_info("vdec_create request id failed!ret =%d\n", id);
		return NULL;
	}
	vdec = vzalloc(sizeof(struct vdec_s));

	/* TBD */
	if (vdec) {
		vdec->magic = 0x43454456;
		vdec->id = -1;
		vdec->type = type;
		vdec->port = port;
		vdec->sys_info = &vdec->sys_info_store;

		INIT_LIST_HEAD(&vdec->list);

		atomic_inc(&vdec_core->vdec_nr);
		vdec->id = id;
		vdec_input_init(&vdec->input, vdec);
		if (master) {
			vdec->master = master;
			master->slave = vdec;
			master->sched = 1;
		}
	}

	pr_debug("vdec_create instance %p, total %d\n", vdec,
		atomic_read(&vdec_core->vdec_nr));

	//trace_vdec_create(vdec); /*DEBUG_TMP*/

	return vdec;
}
EXPORT_SYMBOL(vdec_create);

int vdec_set_format(struct vdec_s *vdec, int format)
{
	vdec->format = format;
	vdec->port_flag |= PORT_FLAG_VFORMAT;

	if (vdec->slave) {
		vdec->slave->format = format;
		vdec->slave->port_flag |= PORT_FLAG_VFORMAT;
	}

	//trace_vdec_set_format(vdec, format);/*DEBUG_TMP*/

	return 0;
}
EXPORT_SYMBOL(vdec_set_format);

int vdec_set_pts(struct vdec_s *vdec, u32 pts)
{
	vdec->pts = pts;
	vdec->pts64 = div64_u64((u64)pts * 100, 9);
	vdec->pts_valid = true;
	//trace_vdec_set_pts(vdec, (u64)pts);/*DEBUG_TMP*/
	return 0;
}
EXPORT_SYMBOL(vdec_set_pts);

void vdec_set_timestamp(struct vdec_s *vdec, u64 timestamp)
{
	vdec->timestamp = timestamp;
	vdec->timestamp_valid = true;
}
EXPORT_SYMBOL(vdec_set_timestamp);

int vdec_set_pts64(struct vdec_s *vdec, u64 pts64)
{
	vdec->pts64 = pts64;
	vdec->pts = (u32)div64_u64(pts64 * 9, 100);
	vdec->pts_valid = true;

	//trace_vdec_set_pts64(vdec, pts64);/*DEBUG_TMP*/
	return 0;
}
EXPORT_SYMBOL(vdec_set_pts64);

int vdec_get_status(struct vdec_s *vdec)
{
	return vdec->status;
}
EXPORT_SYMBOL(vdec_get_status);

void vdec_set_status(struct vdec_s *vdec, int status)
{
	//trace_vdec_set_status(vdec, status);/*DEBUG_TMP*/
	vdec->status = status;
}
EXPORT_SYMBOL(vdec_set_status);

void vdec_set_next_status(struct vdec_s *vdec, int status)
{
	//trace_vdec_set_next_status(vdec, status);/*DEBUG_TMP*/
	vdec->next_status = status;
}
EXPORT_SYMBOL(vdec_set_next_status);

int vdec_set_video_path(struct vdec_s *vdec, int video_path)
{
	vdec->frame_base_video_path = video_path;
	return 0;
}
EXPORT_SYMBOL(vdec_set_video_path);

int vdec_set_receive_id(struct vdec_s *vdec, int receive_id)
{
	vdec->vf_receiver_inst = receive_id;
	return 0;
}
EXPORT_SYMBOL(vdec_set_receive_id);

/* add frame data to input chain */
int vdec_write_vframe(struct vdec_s *vdec, const char *buf, size_t count)
{
	return vdec_input_add_frame(&vdec->input, buf, count);
}
EXPORT_SYMBOL(vdec_write_vframe);

/* add a work queue thread for vdec*/
void vdec_schedule_work(struct work_struct *work)
{
	if (vdec_core->vdec_core_wq)
		queue_work(vdec_core->vdec_core_wq, work);
	else
		schedule_work(work);
}
EXPORT_SYMBOL(vdec_schedule_work);

static struct vdec_s *vdec_get_associate(struct vdec_s *vdec)
{
	if (vdec->master)
		return vdec->master;
	else if (vdec->slave)
		return vdec->slave;
	return NULL;
}

static void vdec_sync_input_read(struct vdec_s *vdec)
{
	if (!vdec_stream_based(vdec))
		return;

	if (vdec_dual(vdec)) {
		u32 me, other;
		if (vdec->input.target == VDEC_INPUT_TARGET_VLD) {
			me = READ_VREG(VLD_MEM_VIFIFO_WRAP_COUNT);
			other =
				vdec_get_associate(vdec)->input.stream_cookie;
			if (me > other)
				return;
			else if (me == other) {
				me = READ_VREG(VLD_MEM_VIFIFO_RP);
				other =
				vdec_get_associate(vdec)->input.swap_rp;
				if (me > other) {
					WRITE_PARSER_REG(PARSER_VIDEO_RP,
						vdec_get_associate(vdec)->
						input.swap_rp);
					return;
				}
			}
			WRITE_PARSER_REG(PARSER_VIDEO_RP,
				READ_VREG(VLD_MEM_VIFIFO_RP));
		} else if (vdec->input.target == VDEC_INPUT_TARGET_HEVC) {
			me = READ_VREG(HEVC_SHIFT_BYTE_COUNT);
			if (((me & 0x80000000) == 0) &&
				(vdec->input.streaming_rp & 0x80000000))
				me += 1ULL << 32;
			other = vdec_get_associate(vdec)->input.streaming_rp;
			if (me > other) {
				WRITE_PARSER_REG(PARSER_VIDEO_RP,
					vdec_get_associate(vdec)->
					input.swap_rp);
				return;
			}

			WRITE_PARSER_REG(PARSER_VIDEO_RP,
				READ_VREG(HEVC_STREAM_RD_PTR));
		}
	} else if (vdec->input.target == VDEC_INPUT_TARGET_VLD) {
		WRITE_PARSER_REG(PARSER_VIDEO_RP,
			READ_VREG(VLD_MEM_VIFIFO_RP));
	} else if (vdec->input.target == VDEC_INPUT_TARGET_HEVC) {
		WRITE_PARSER_REG(PARSER_VIDEO_RP,
			READ_VREG(HEVC_STREAM_RD_PTR));
	}
}

static void vdec_sync_input_write(struct vdec_s *vdec)
{
	if (!vdec_stream_based(vdec))
		return;

	if (vdec->input.target == VDEC_INPUT_TARGET_VLD) {
		WRITE_VREG(VLD_MEM_VIFIFO_WP,
			READ_PARSER_REG(PARSER_VIDEO_WP));
	} else if (vdec->input.target == VDEC_INPUT_TARGET_HEVC) {
		WRITE_VREG(HEVC_STREAM_WR_PTR,
			READ_PARSER_REG(PARSER_VIDEO_WP));
	}
}

/*
 *get next frame from input chain
 */
/*
 *THE VLD_FIFO is 512 bytes and Video buffer level
 * empty interrupt is set to 0x80 bytes threshold
 */
#define VLD_PADDING_SIZE 1024
#define HEVC_PADDING_SIZE (1024*16)
int vdec_prepare_input(struct vdec_s *vdec, struct vframe_chunk_s **p)
{
	struct vdec_input_s *input = &vdec->input;
	struct vframe_chunk_s *chunk = NULL;
	struct vframe_block_list_s *block = NULL;
	int dummy;

	/* full reset to HW input */
	if (input->target == VDEC_INPUT_TARGET_VLD) {
		WRITE_VREG(VLD_MEM_VIFIFO_CONTROL, 0);

		/* reset VLD fifo for all vdec */
		WRITE_VREG(DOS_SW_RESET0, (1<<5) | (1<<4) | (1<<3));
		WRITE_VREG(DOS_SW_RESET0, 0);

		dummy = READ_RESET_REG(RESET0_REGISTER);
		WRITE_VREG(POWER_CTL_VLD, 1 << 4);
	} else if (input->target == VDEC_INPUT_TARGET_HEVC) {
#if 0
		/*move to driver*/
		if (input_frame_based(input))
			WRITE_VREG(HEVC_STREAM_CONTROL, 0);

		/*
		 * 2: assist
		 * 3: parser
		 * 4: parser_state
		 * 8: dblk
		 * 11:mcpu
		 * 12:ccpu
		 * 13:ddr
		 * 14:iqit
		 * 15:ipp
		 * 17:qdct
		 * 18:mpred
		 * 19:sao
		 * 24:hevc_afifo
		 */
		WRITE_VREG(DOS_SW_RESET3,
			(1<<3)|(1<<4)|(1<<8)|(1<<11)|(1<<12)|(1<<14)|(1<<15)|
			(1<<17)|(1<<18)|(1<<19));
		WRITE_VREG(DOS_SW_RESET3, 0);
#endif
	}

	/*
	 *setup HW decoder input buffer (VLD context)
	 * based on input->type and input->target
	 */
	if (input_frame_based(input)) {
		chunk = vdec_input_next_chunk(&vdec->input);

		if (chunk == NULL) {
			*p = NULL;
			return -1;
		}

		block = chunk->block;

		if (input->target == VDEC_INPUT_TARGET_VLD) {
			WRITE_VREG(VLD_MEM_VIFIFO_START_PTR, block->start);
			WRITE_VREG(VLD_MEM_VIFIFO_END_PTR, block->start +
					block->size - 8);
			WRITE_VREG(VLD_MEM_VIFIFO_CURR_PTR,
					round_down(block->start + chunk->offset,
						VDEC_FIFO_ALIGN));

			WRITE_VREG(VLD_MEM_VIFIFO_CONTROL, 1);
			WRITE_VREG(VLD_MEM_VIFIFO_CONTROL, 0);

			/* set to manual mode */
			WRITE_VREG(VLD_MEM_VIFIFO_BUF_CNTL, 2);
			WRITE_VREG(VLD_MEM_VIFIFO_RP,
					round_down(block->start + chunk->offset,
						VDEC_FIFO_ALIGN));
			dummy = chunk->offset + chunk->size +
				VLD_PADDING_SIZE;
			if (dummy >= block->size)
				dummy -= block->size;
			WRITE_VREG(VLD_MEM_VIFIFO_WP,
				round_down(block->start + dummy,
					VDEC_FIFO_ALIGN));

			WRITE_VREG(VLD_MEM_VIFIFO_BUF_CNTL, 3);
			WRITE_VREG(VLD_MEM_VIFIFO_BUF_CNTL, 2);

			WRITE_VREG(VLD_MEM_VIFIFO_CONTROL,
				(0x11 << 16) | (1<<10) | (7<<3));

		} else if (input->target == VDEC_INPUT_TARGET_HEVC) {
			WRITE_VREG(HEVC_STREAM_START_ADDR, block->start);
			WRITE_VREG(HEVC_STREAM_END_ADDR, block->start +
					block->size);
			WRITE_VREG(HEVC_STREAM_RD_PTR, block->start +
					chunk->offset);
			dummy = chunk->offset + chunk->size +
					HEVC_PADDING_SIZE;
			if (dummy >= block->size)
				dummy -= block->size;
			WRITE_VREG(HEVC_STREAM_WR_PTR,
				round_down(block->start + dummy,
					VDEC_FIFO_ALIGN));

			/* set endian */
			SET_VREG_MASK(HEVC_STREAM_CONTROL, 7 << 4);
		}

		*p = chunk;
		return chunk->size;

	} else {
		/* stream based */
		u32 rp = 0, wp = 0, fifo_len = 0;
		int size;
		bool swap_valid = input->swap_valid;
		unsigned long swap_page_phys = input->swap_page_phys;

		if (vdec_dual(vdec) &&
			((vdec->flag & VDEC_FLAG_SELF_INPUT_CONTEXT) == 0)) {
			/* keep using previous input context */
			struct vdec_s *master = (vdec->slave) ?
				vdec : vdec->master;
		    if (master->input.last_swap_slave) {
				swap_valid = master->slave->input.swap_valid;
				swap_page_phys =
					master->slave->input.swap_page_phys;
			} else {
				swap_valid = master->input.swap_valid;
				swap_page_phys = master->input.swap_page_phys;
			}
		}

		if (swap_valid) {
			if (input->target == VDEC_INPUT_TARGET_VLD) {
				if (vdec->format == VFORMAT_H264)
					SET_VREG_MASK(POWER_CTL_VLD,
						(1 << 9));

				WRITE_VREG(VLD_MEM_VIFIFO_CONTROL, 0);

				/* restore read side */
				WRITE_VREG(VLD_MEM_SWAP_ADDR,
					swap_page_phys);
				WRITE_VREG(VLD_MEM_SWAP_CTL, 1);

				while (READ_VREG(VLD_MEM_SWAP_CTL) & (1<<7))
					;
				WRITE_VREG(VLD_MEM_SWAP_CTL, 0);

				/* restore wrap count */
				WRITE_VREG(VLD_MEM_VIFIFO_WRAP_COUNT,
					input->stream_cookie);

				rp = READ_VREG(VLD_MEM_VIFIFO_RP);
				fifo_len = READ_VREG(VLD_MEM_VIFIFO_LEVEL);

				/* enable */
				WRITE_VREG(VLD_MEM_VIFIFO_CONTROL,
					(0x11 << 16) | (1<<10));

				/* sync with front end */
				vdec_sync_input_read(vdec);
				vdec_sync_input_write(vdec);

				wp = READ_VREG(VLD_MEM_VIFIFO_WP);
			} else if (input->target == VDEC_INPUT_TARGET_HEVC) {
				SET_VREG_MASK(HEVC_STREAM_CONTROL, 1);

				/* restore read side */
				WRITE_VREG(HEVC_STREAM_SWAP_ADDR,
					swap_page_phys);
				WRITE_VREG(HEVC_STREAM_SWAP_CTRL, 1);

				while (READ_VREG(HEVC_STREAM_SWAP_CTRL)
					& (1<<7))
					;
				WRITE_VREG(HEVC_STREAM_SWAP_CTRL, 0);

				/* restore stream offset */
				WRITE_VREG(HEVC_SHIFT_BYTE_COUNT,
					input->stream_cookie);

				rp = READ_VREG(HEVC_STREAM_RD_PTR);
				fifo_len = (READ_VREG(HEVC_STREAM_FIFO_CTL)
						>> 16) & 0x7f;


				/* enable */

				/* sync with front end */
				vdec_sync_input_read(vdec);
				vdec_sync_input_write(vdec);

				wp = READ_VREG(HEVC_STREAM_WR_PTR);

				/*pr_info("vdec: restore context\r\n");*/
			}

		} else {
			if (input->target == VDEC_INPUT_TARGET_VLD) {
				WRITE_VREG(VLD_MEM_VIFIFO_START_PTR,
					input->start);
				WRITE_VREG(VLD_MEM_VIFIFO_END_PTR,
					input->start + input->size - 8);
				WRITE_VREG(VLD_MEM_VIFIFO_CURR_PTR,
					input->start);

				WRITE_VREG(VLD_MEM_VIFIFO_CONTROL, 1);
				WRITE_VREG(VLD_MEM_VIFIFO_CONTROL, 0);

				/* set to manual mode */
				WRITE_VREG(VLD_MEM_VIFIFO_BUF_CNTL, 2);
				WRITE_VREG(VLD_MEM_VIFIFO_RP, input->start);
				WRITE_VREG(VLD_MEM_VIFIFO_WP,
					READ_PARSER_REG(PARSER_VIDEO_WP));

				rp = READ_VREG(VLD_MEM_VIFIFO_RP);

				/* enable */
				WRITE_VREG(VLD_MEM_VIFIFO_CONTROL,
					(0x11 << 16) | (1<<10));

				wp = READ_VREG(VLD_MEM_VIFIFO_WP);

			} else if (input->target == VDEC_INPUT_TARGET_HEVC) {
				WRITE_VREG(HEVC_STREAM_START_ADDR,
					input->start);
				WRITE_VREG(HEVC_STREAM_END_ADDR,
					input->start + input->size);
				WRITE_VREG(HEVC_STREAM_RD_PTR,
					input->start);
				WRITE_VREG(HEVC_STREAM_WR_PTR,
					READ_PARSER_REG(PARSER_VIDEO_WP));

				rp = READ_VREG(HEVC_STREAM_RD_PTR);
				wp = READ_VREG(HEVC_STREAM_WR_PTR);
				fifo_len = (READ_VREG(HEVC_STREAM_FIFO_CTL)
						>> 16) & 0x7f;

				/* enable */
			}
		}
		*p = NULL;
		if (wp >= rp)
			size = wp - rp + fifo_len;
		else
			size = wp + input->size - rp + fifo_len;
		if (size < 0) {
			pr_info("%s error: input->size %x wp %x rp %x fifo_len %x => size %x\r\n",
				__func__, input->size, wp, rp, fifo_len, size);
			size = 0;
		}
		return size;
	}
}
EXPORT_SYMBOL(vdec_prepare_input);

void vdec_enable_input(struct vdec_s *vdec)
{
	struct vdec_input_s *input = &vdec->input;

	if (vdec->status != VDEC_STATUS_ACTIVE)
		return;

	if (input->target == VDEC_INPUT_TARGET_VLD)
		SET_VREG_MASK(VLD_MEM_VIFIFO_CONTROL, (1<<2) | (1<<1));
	else if (input->target == VDEC_INPUT_TARGET_HEVC) {
		SET_VREG_MASK(HEVC_STREAM_CONTROL, 1);
		if (vdec_stream_based(vdec))
			CLEAR_VREG_MASK(HEVC_STREAM_CONTROL, 7 << 4);
		else
			SET_VREG_MASK(HEVC_STREAM_CONTROL, 7 << 4);
		SET_VREG_MASK(HEVC_STREAM_FIFO_CTL, (1<<29));
	}
}
EXPORT_SYMBOL(vdec_enable_input);

int vdec_set_input_buffer(struct vdec_s *vdec, u32 start, u32 size)
{
	int r = vdec_input_set_buffer(&vdec->input, start, size);

	if (r)
		return r;

	if (vdec->slave)
		r = vdec_input_set_buffer(&vdec->slave->input, start, size);

	return r;
}
EXPORT_SYMBOL(vdec_set_input_buffer);

/*
 * vdec_eos returns the possibility that there are
 * more input can be used by decoder through vdec_prepare_input
 * Note: this function should be called prior to vdec_vframe_dirty
 * by decoder driver to determine if EOS happens for stream based
 * decoding when there is no sufficient data for a frame
 */
bool vdec_has_more_input(struct vdec_s *vdec)
{
	struct vdec_input_s *input = &vdec->input;

	if (!input->eos)
		return true;

	if (input_frame_based(input))
		return vdec_input_next_input_chunk(input) != NULL;
	else {
		if (input->target == VDEC_INPUT_TARGET_VLD)
			return READ_VREG(VLD_MEM_VIFIFO_WP) !=
				READ_PARSER_REG(PARSER_VIDEO_WP);
		else {
			return (READ_VREG(HEVC_STREAM_WR_PTR) & ~0x3) !=
				(READ_PARSER_REG(PARSER_VIDEO_WP) & ~0x3);
		}
	}
}
EXPORT_SYMBOL(vdec_has_more_input);

void vdec_set_prepare_level(struct vdec_s *vdec, int level)
{
	vdec->input.prepare_level = level;
}
EXPORT_SYMBOL(vdec_set_prepare_level);

void vdec_set_flag(struct vdec_s *vdec, u32 flag)
{
	vdec->flag = flag;
}
EXPORT_SYMBOL(vdec_set_flag);

void vdec_set_eos(struct vdec_s *vdec, bool eos)
{
	vdec->input.eos = eos;

	if (vdec->slave)
		vdec->slave->input.eos = eos;
}
EXPORT_SYMBOL(vdec_set_eos);

#ifdef VDEC_DEBUG_SUPPORT
void vdec_set_step_mode(void)
{
	step_mode = 0x1ff;
}
EXPORT_SYMBOL(vdec_set_step_mode);
#endif

void vdec_set_next_sched(struct vdec_s *vdec, struct vdec_s *next_vdec)
{
	if (vdec && next_vdec) {
		vdec->sched = 0;
		next_vdec->sched = 1;
	}
}
EXPORT_SYMBOL(vdec_set_next_sched);

/*
 * Swap Context:       S0     S1     S2     S3     S4
 * Sample sequence:  M     S      M      M      S
 * Master Context:     S0     S0     S2     S3     S3
 * Slave context:      NA     S1     S1     S2     S4
 *                                          ^
 *                                          ^
 *                                          ^
 *                                    the tricky part
 * If there are back to back decoding of master or slave
 * then the context of the counter part should be updated
 * with current decoder. In this example, S1 should be
 * updated to S2.
 * This is done by swap the swap_page and related info
 * between two layers.
 */
static void vdec_borrow_input_context(struct vdec_s *vdec)
{
	struct page *swap_page;
	unsigned long swap_page_phys;
	struct vdec_input_s *me;
	struct vdec_input_s *other;

	if (!vdec_dual(vdec))
		return;

	me = &vdec->input;
	other = &vdec_get_associate(vdec)->input;

	/* swap the swap_context, borrow counter part's
	 * swap context storage and update all related info.
	 * After vdec_vframe_dirty, vdec_save_input_context
	 * will be called to update current vdec's
	 * swap context
	 */
	swap_page = other->swap_page;
	other->swap_page = me->swap_page;
	me->swap_page = swap_page;

	swap_page_phys = other->swap_page_phys;
	other->swap_page_phys = me->swap_page_phys;
	me->swap_page_phys = swap_page_phys;

	other->swap_rp = me->swap_rp;
	other->streaming_rp = me->streaming_rp;
	other->stream_cookie = me->stream_cookie;
	other->swap_valid = me->swap_valid;
}

void vdec_vframe_dirty(struct vdec_s *vdec, struct vframe_chunk_s *chunk)
{
	if (chunk)
		chunk->flag |= VFRAME_CHUNK_FLAG_CONSUMED;

	if (vdec_stream_based(vdec)) {
		vdec->input.swap_needed = true;

		if (vdec_dual(vdec)) {
			vdec_get_associate(vdec)->input.dirty_count = 0;
			vdec->input.dirty_count++;
			if (vdec->input.dirty_count > 1) {
				vdec->input.dirty_count = 1;
				vdec_borrow_input_context(vdec);
			}
		}

		/* for stream based mode, we update read and write pointer
		 * also in case decoder wants to keep working on decoding
		 * for more frames while input front end has more data
		 */
		vdec_sync_input_read(vdec);
		vdec_sync_input_write(vdec);

		vdec->need_more_data |= VDEC_NEED_MORE_DATA_DIRTY;
		vdec->need_more_data &= ~VDEC_NEED_MORE_DATA;
	}
}
EXPORT_SYMBOL(vdec_vframe_dirty);

bool vdec_need_more_data(struct vdec_s *vdec)
{
	if (vdec_stream_based(vdec))
		return vdec->need_more_data & VDEC_NEED_MORE_DATA;

	return false;
}
EXPORT_SYMBOL(vdec_need_more_data);


void hevc_wait_ddr(void)
{
	unsigned long flags;
	spin_lock_irqsave(&vdec_spin_lock, flags);
	codec_dmcbus_write(DMC_REQ_CTRL,
		codec_dmcbus_read(DMC_REQ_CTRL) & (~(1 << 4)));
	spin_unlock_irqrestore(&vdec_spin_lock, flags);

	while (!(codec_dmcbus_read(DMC_CHAN_STS)
		& (1 << 4)))
		;
}

void vdec_save_input_context(struct vdec_s *vdec)
{
	struct vdec_input_s *input = &vdec->input;

#ifdef CONFIG_AMLOGIC_MEDIA_MULTI_DEC
	vdec_profile(vdec, VDEC_PROFILE_EVENT_SAVE_INPUT);
#endif

	if (input->target == VDEC_INPUT_TARGET_VLD)
		WRITE_VREG(VLD_MEM_VIFIFO_CONTROL, 1<<15);

	if (input_stream_based(input) && (input->swap_needed)) {
		if (input->target == VDEC_INPUT_TARGET_VLD) {
			WRITE_VREG(VLD_MEM_SWAP_ADDR,
				input->swap_page_phys);
			WRITE_VREG(VLD_MEM_SWAP_CTL, 3);
			while (READ_VREG(VLD_MEM_SWAP_CTL) & (1<<7))
				;
			WRITE_VREG(VLD_MEM_SWAP_CTL, 0);
			vdec->input.stream_cookie =
				READ_VREG(VLD_MEM_VIFIFO_WRAP_COUNT);
			vdec->input.swap_rp =
				READ_VREG(VLD_MEM_VIFIFO_RP);
			vdec->input.total_rd_count =
				(u64)vdec->input.stream_cookie *
				vdec->input.size + vdec->input.swap_rp -
				READ_VREG(VLD_MEM_VIFIFO_BYTES_AVAIL);
		} else if (input->target == VDEC_INPUT_TARGET_HEVC) {
			WRITE_VREG(HEVC_STREAM_SWAP_ADDR,
				input->swap_page_phys);
			WRITE_VREG(HEVC_STREAM_SWAP_CTRL, 3);

			while (READ_VREG(HEVC_STREAM_SWAP_CTRL) & (1<<7))
				;
			WRITE_VREG(HEVC_STREAM_SWAP_CTRL, 0);

			vdec->input.stream_cookie =
				READ_VREG(HEVC_SHIFT_BYTE_COUNT);
			vdec->input.swap_rp =
				READ_VREG(HEVC_STREAM_RD_PTR);
			if (((vdec->input.stream_cookie & 0x80000000) == 0) &&
				(vdec->input.streaming_rp & 0x80000000))
				vdec->input.streaming_rp += 1ULL << 32;
			vdec->input.streaming_rp &= 0xffffffffULL << 32;
			vdec->input.streaming_rp |= vdec->input.stream_cookie;
			vdec->input.total_rd_count = vdec->input.streaming_rp;
		}

		input->swap_valid = true;
		input->swap_needed = false;
		/*pr_info("vdec: save context\r\n");*/

		vdec_sync_input_read(vdec);

		if (vdec_dual(vdec)) {
			struct vdec_s *master = (vdec->slave) ?
				vdec : vdec->master;
			master->input.last_swap_slave = (master->slave == vdec);
			/* pr_info("master->input.last_swap_slave = %d\n",
				master->input.last_swap_slave); */
		}

		hevc_wait_ddr();
	}
}
EXPORT_SYMBOL(vdec_save_input_context);

void vdec_clean_input(struct vdec_s *vdec)
{
	struct vdec_input_s *input = &vdec->input;

	while (!list_empty(&input->vframe_chunk_list)) {
		struct vframe_chunk_s *chunk =
			vdec_input_next_chunk(input);
		if (chunk && (chunk->flag & VFRAME_CHUNK_FLAG_CONSUMED))
			vdec_input_release_chunk(input, chunk);
		else
			break;
	}
	vdec_save_input_context(vdec);
}
EXPORT_SYMBOL(vdec_clean_input);

int vdec_sync_input(struct vdec_s *vdec)
{
	struct vdec_input_s *input = &vdec->input;
	u32 rp = 0, wp = 0, fifo_len = 0;
	int size;

	vdec_sync_input_read(vdec);
	vdec_sync_input_write(vdec);
	if (input->target == VDEC_INPUT_TARGET_VLD) {
		rp = READ_VREG(VLD_MEM_VIFIFO_RP);
		wp = READ_VREG(VLD_MEM_VIFIFO_WP);

	} else if (input->target == VDEC_INPUT_TARGET_HEVC) {
		rp = READ_VREG(HEVC_STREAM_RD_PTR);
		wp = READ_VREG(HEVC_STREAM_WR_PTR);
		fifo_len = (READ_VREG(HEVC_STREAM_FIFO_CTL)
				>> 16) & 0x7f;
	}
	if (wp >= rp)
		size = wp - rp + fifo_len;
	else
		size = wp + input->size - rp + fifo_len;
	if (size < 0) {
		pr_info("%s error: input->size %x wp %x rp %x fifo_len %x => size %x\r\n",
			__func__, input->size, wp, rp, fifo_len, size);
		size = 0;
	}
	return size;

}
EXPORT_SYMBOL(vdec_sync_input);

const char *vdec_status_str(struct vdec_s *vdec)
{
	return vdec->status < ARRAY_SIZE(vdec_status_string) ?
		vdec_status_string[vdec->status] : "INVALID";
}

const char *vdec_type_str(struct vdec_s *vdec)
{
	switch (vdec->type) {
	case VDEC_TYPE_SINGLE:
		return "VDEC_TYPE_SINGLE";
	case VDEC_TYPE_STREAM_PARSER:
		return "VDEC_TYPE_STREAM_PARSER";
	case VDEC_TYPE_FRAME_BLOCK:
		return "VDEC_TYPE_FRAME_BLOCK";
	case VDEC_TYPE_FRAME_CIRCULAR:
		return "VDEC_TYPE_FRAME_CIRCULAR";
	default:
		return "VDEC_TYPE_INVALID";
	}
}

const char *vdec_device_name_str(struct vdec_s *vdec)
{
	return vdec_device_name[vdec->format * 2 + 1];
}
EXPORT_SYMBOL(vdec_device_name_str);

void walk_vdec_core_list(char *s)
{
	struct vdec_s *vdec;
	struct vdec_core_s *core = vdec_core;
	unsigned long flags;

	pr_info("%s --->\n", s);

	flags = vdec_core_lock(vdec_core);

	if (list_empty(&core->connected_vdec_list)) {
		pr_info("connected vdec list empty\n");
	} else {
		list_for_each_entry(vdec, &core->connected_vdec_list, list) {
			pr_info("\tvdec (%p), status = %s\n", vdec,
				vdec_status_str(vdec));
		}
	}

	vdec_core_unlock(vdec_core, flags);
}
EXPORT_SYMBOL(walk_vdec_core_list);

/* insert vdec to vdec_core for scheduling,
 * for dual running decoders, connect/disconnect always runs in pairs
 */
int vdec_connect(struct vdec_s *vdec)
{
	unsigned long flags;

	//trace_vdec_connect(vdec);/*DEBUG_TMP*/

	if (vdec->status != VDEC_STATUS_DISCONNECTED)
		return 0;

	vdec_set_status(vdec, VDEC_STATUS_CONNECTED);
	vdec_set_next_status(vdec, VDEC_STATUS_CONNECTED);

	init_completion(&vdec->inactive_done);

	if (vdec->slave) {
		vdec_set_status(vdec->slave, VDEC_STATUS_CONNECTED);
		vdec_set_next_status(vdec->slave, VDEC_STATUS_CONNECTED);

		init_completion(&vdec->slave->inactive_done);
	}

	flags = vdec_core_lock(vdec_core);

	list_add_tail(&vdec->list, &vdec_core->connected_vdec_list);

	if (vdec->slave) {
		list_add_tail(&vdec->slave->list,
			&vdec_core->connected_vdec_list);
	}

	vdec_core_unlock(vdec_core, flags);

	up(&vdec_core->sem);

	return 0;
}
EXPORT_SYMBOL(vdec_connect);

/* remove vdec from vdec_core scheduling */
int vdec_disconnect(struct vdec_s *vdec)
{
#ifdef CONFIG_AMLOGIC_MEDIA_MULTI_DEC
	vdec_profile(vdec, VDEC_PROFILE_EVENT_DISCONNECT);
#endif
	//trace_vdec_disconnect(vdec);/*DEBUG_TMP*/

	if ((vdec->status != VDEC_STATUS_CONNECTED) &&
		(vdec->status != VDEC_STATUS_ACTIVE)) {
		return 0;
	}
	mutex_lock(&vdec_mutex);
	/*
	 *when a vdec is under the management of scheduler
	 * the status change will only be from vdec_core_thread
	 */
	vdec_set_next_status(vdec, VDEC_STATUS_DISCONNECTED);

	if (vdec->slave)
		vdec_set_next_status(vdec->slave, VDEC_STATUS_DISCONNECTED);
	else if (vdec->master)
		vdec_set_next_status(vdec->master, VDEC_STATUS_DISCONNECTED);
	mutex_unlock(&vdec_mutex);
	up(&vdec_core->sem);

	if(!wait_for_completion_timeout(&vdec->inactive_done,
		msecs_to_jiffies(2000)))
		goto discon_timeout;

	if (vdec->slave) {
		if(!wait_for_completion_timeout(&vdec->slave->inactive_done,
			msecs_to_jiffies(2000)))
			goto discon_timeout;
	} else if (vdec->master) {
		if(!wait_for_completion_timeout(&vdec->master->inactive_done,
			msecs_to_jiffies(2000)))
			goto discon_timeout;
	}

	return 0;
discon_timeout:
	pr_err("%s timeout!!! status: 0x%x\n", __func__, vdec->status);
	return 0;
}
EXPORT_SYMBOL(vdec_disconnect);

/* release vdec structure */
int vdec_destroy(struct vdec_s *vdec)
{
	//trace_vdec_destroy(vdec);/*DEBUG_TMP*/

	vdec_input_release(&vdec->input);

#ifdef CONFIG_AMLOGIC_MEDIA_MULTI_DEC
	vdec_profile_flush(vdec);
#endif
	ida_simple_remove(&vdec_core->ida, vdec->id);
	vfree(vdec);

	atomic_dec(&vdec_core->vdec_nr);

	return 0;
}
EXPORT_SYMBOL(vdec_destroy);

/*
 * Only support time sliced decoding for frame based input,
 * so legacy decoder can exist with time sliced decoder.
 */
static const char *get_dev_name(bool use_legacy_vdec, int format)
{
#ifdef CONFIG_AMLOGIC_MEDIA_MULTI_DEC
	if (use_legacy_vdec)
		return vdec_device_name[format * 2];
	else
		return vdec_device_name[format * 2 + 1];
#else
	return vdec_device_name[format];
#endif
}

struct vdec_s *vdec_get_with_id(unsigned id)
{
	struct vdec_s *vdec, *ret_vdec = NULL;
	struct vdec_core_s *core = vdec_core;
	unsigned long flags;

	if (id >= MAX_INSTANCE_MUN)
		return NULL;

	flags = vdec_core_lock(vdec_core);
	if (!list_empty(&core->connected_vdec_list)) {
		list_for_each_entry(vdec, &core->connected_vdec_list, list) {
			if (vdec->id == id) {
				pr_info("searched avaliable vdec connected, id = %d\n", id);
				ret_vdec = vdec;
				break;
			}
		}
	}
	vdec_core_unlock(vdec_core, flags);

	return ret_vdec;
}

/*
 *register vdec_device
 * create output, vfm or create ionvideo output
 */
s32 vdec_init(struct vdec_s *vdec, int is_4k)
{
	int r = 0;
	struct vdec_s *p = vdec;
	const char *dev_name;
	int id = PLATFORM_DEVID_AUTO;/*if have used my self*/

	dev_name = get_dev_name(vdec_single(vdec), vdec->format);

	if (dev_name == NULL)
		return -ENODEV;

	pr_info("vdec_init, dev_name:%s, vdec_type=%s\n",
		dev_name, vdec_type_str(vdec));

	/*
	 *todo: VFM patch control should be configurable,
	 * for now all stream based input uses default VFM path.
	 */
	if (vdec_stream_based(vdec) && !vdec_dual(vdec)) {
		if (vdec_core->vfm_vdec == NULL) {
			pr_debug("vdec_init set vfm decoder %p\n", vdec);
			vdec_core->vfm_vdec = vdec;
		} else {
			pr_info("vdec_init vfm path busy.\n");
			return -EBUSY;
		}
	}

	mutex_lock(&vdec_mutex);
	inited_vcodec_num++;
	mutex_unlock(&vdec_mutex);

	vdec_input_set_type(&vdec->input, vdec->type,
			(vdec->format == VFORMAT_HEVC ||
			vdec->format == VFORMAT_AVS2 ||
			vdec->format == VFORMAT_VP9) ?
				VDEC_INPUT_TARGET_HEVC :
				VDEC_INPUT_TARGET_VLD);

	p->cma_dev = vdec_core->cma_dev;
	p->get_canvas = get_canvas;
	p->get_canvas_ex = get_canvas_ex;
	p->free_canvas_ex = free_canvas_ex;
	p->vdec_fps_detec = vdec_fps_detec;
	atomic_set(&p->inirq_flag, 0);
	atomic_set(&p->inirq_thread_flag, 0);
	/* todo */
	if (!vdec_dual(vdec))
		p->use_vfm_path = vdec_stream_based(vdec);
	/* vdec_dev_reg.flag = 0; */
	if (vdec->id >= 0)
		id = vdec->id;
	p->parallel_dec = parallel_decode;
	vdec_core->parallel_dec = parallel_decode;
	vdec->canvas_mode = CANVAS_BLKMODE_32X32;
#ifdef FRAME_CHECK
	vdec_frame_check_init(vdec);
#endif
	p->dev = platform_device_register_data(
				&vdec_core->vdec_core_platform_device->dev,
				dev_name,
				id,
				&p, sizeof(struct vdec_s *));

	if (IS_ERR(p->dev)) {
		r = PTR_ERR(p->dev);
		pr_err("vdec: Decoder device %s register failed (%d)\n",
			dev_name, r);

		mutex_lock(&vdec_mutex);
		inited_vcodec_num--;
		mutex_unlock(&vdec_mutex);

		goto error;
	} else if (!p->dev->dev.driver) {
		pr_info("vdec: Decoder device %s driver probe failed.\n",
			dev_name);
		r = -ENODEV;

		goto error;
	}

	if ((p->type == VDEC_TYPE_FRAME_BLOCK) && (p->run == NULL)) {
		r = -ENODEV;
		pr_err("vdec: Decoder device not handled (%s)\n", dev_name);

		mutex_lock(&vdec_mutex);
		inited_vcodec_num--;
		mutex_unlock(&vdec_mutex);

		goto error;
	}

	if (p->use_vfm_path) {
		vdec->vf_receiver_inst = -1;
		vdec->vfm_map_id[0] = 0;
	} else if (!vdec_dual(vdec)) {
		/* create IONVIDEO instance and connect decoder's
		 * vf_provider interface to it
		 */
		if (p->type != VDEC_TYPE_FRAME_BLOCK) {
			r = -ENODEV;
			pr_err("vdec: Incorrect decoder type\n");

			mutex_lock(&vdec_mutex);
			inited_vcodec_num--;
			mutex_unlock(&vdec_mutex);

			goto error;
		}
		if (p->frame_base_video_path == FRAME_BASE_PATH_IONVIDEO) {
#ifdef CONFIG_AMLOGIC_IONVIDEO
#if 1
			r = ionvideo_assign_map(&vdec->vf_receiver_name,
					&vdec->vf_receiver_inst);
#else
			/*
			 * temporarily just use decoder instance ID as iondriver ID
			 * to solve OMX iondriver instance number check time sequence
			 * only the limitation is we can NOT mix different video
			 * decoders since same ID will be used for different decoder
			 * formats.
			 */
			vdec->vf_receiver_inst = p->dev->id;
			r = ionvideo_assign_map(&vdec->vf_receiver_name,
					&vdec->vf_receiver_inst);
#endif
			if (r < 0) {
				pr_err("IonVideo frame receiver allocation failed.\n");

				mutex_lock(&vdec_mutex);
				inited_vcodec_num--;
				mutex_unlock(&vdec_mutex);

				goto error;
			}

#endif
			snprintf(vdec->vfm_map_chain, VDEC_MAP_NAME_SIZE,
				"%s %s", vdec->vf_provider_name,
				vdec->vf_receiver_name);
			snprintf(vdec->vfm_map_id, VDEC_MAP_NAME_SIZE,
				"vdec-map-%d", vdec->id);
		} else if (p->frame_base_video_path ==
				FRAME_BASE_PATH_AMLVIDEO_AMVIDEO) {
			if (vdec_secure(vdec)) {
				snprintf(vdec->vfm_map_chain, VDEC_MAP_NAME_SIZE,
					"%s %s", vdec->vf_provider_name,
					"amlvideo amvideo");
			} else {
				snprintf(vdec->vfm_map_chain, VDEC_MAP_NAME_SIZE,
					"%s %s", vdec->vf_provider_name,
					"amlvideo deinterlace amvideo");
			}
			snprintf(vdec->vfm_map_id, VDEC_MAP_NAME_SIZE,
				"vdec-map-%d", vdec->id);
		} else if (p->frame_base_video_path ==
				FRAME_BASE_PATH_AMLVIDEO1_AMVIDEO2) {
			snprintf(vdec->vfm_map_chain, VDEC_MAP_NAME_SIZE,
				"%s %s", vdec->vf_provider_name,
				"aml_video.1 videosync.0 videopip");
			snprintf(vdec->vfm_map_id, VDEC_MAP_NAME_SIZE,
				"vdec-map-%d", vdec->id);
		} else if (p->frame_base_video_path == FRAME_BASE_PATH_V4L_VIDEO) {
			snprintf(vdec->vfm_map_chain, VDEC_MAP_NAME_SIZE,
				"%s %s", vdec->vf_provider_name,
				vdec->vf_receiver_name);
			snprintf(vdec->vfm_map_id, VDEC_MAP_NAME_SIZE,
				"vdec-map-%d", vdec->id);
		} else if (p->frame_base_video_path == FRAME_BASE_PATH_TUNNEL_MODE) {
			snprintf(vdec->vfm_map_chain, VDEC_MAP_NAME_SIZE,
				"%s %s", vdec->vf_provider_name,
				"amvideo");
			snprintf(vdec->vfm_map_id, VDEC_MAP_NAME_SIZE,
				"vdec-map-%d", vdec->id);
		}

		if (vfm_map_add(vdec->vfm_map_id,
					vdec->vfm_map_chain) < 0) {
			r = -ENOMEM;
			pr_err("Decoder pipeline map creation failed %s.\n",
				vdec->vfm_map_id);
			vdec->vfm_map_id[0] = 0;

			mutex_lock(&vdec_mutex);
			inited_vcodec_num--;
			mutex_unlock(&vdec_mutex);

			goto error;
		}

		pr_debug("vfm map %s created\n", vdec->vfm_map_id);

		/*
		 *assume IONVIDEO driver already have a few vframe_receiver
		 * registered.
		 * 1. Call iondriver function to allocate a IONVIDEO path and
		 *    provide receiver's name and receiver op.
		 * 2. Get decoder driver's provider name from driver instance
		 * 3. vfm_map_add(name, "<decoder provider name>
		 *    <iondriver receiver name>"), e.g.
		 *    vfm_map_add("vdec_ion_map_0", "mpeg4_0 iondriver_1");
		 * 4. vf_reg_provider and vf_reg_receiver
		 * Note: the decoder provider's op uses vdec as op_arg
		 *       the iondriver receiver's op uses iondev device as
		 *       op_arg
		 */

	}

	if (!vdec_single(vdec)) {
		vf_reg_provider(&p->vframe_provider);

		vf_notify_receiver(p->vf_provider_name,
			VFRAME_EVENT_PROVIDER_START,
			vdec);

		if (vdec_core->hint_fr_vdec == NULL)
			vdec_core->hint_fr_vdec = vdec;

		if (vdec_core->hint_fr_vdec == vdec) {
			if (p->sys_info->rate != 0) {
				if (!vdec->is_reset)
					vf_notify_receiver(p->vf_provider_name,
						VFRAME_EVENT_PROVIDER_FR_HINT,
						(void *)
						((unsigned long)
						p->sys_info->rate));
				vdec->fr_hint_state = VDEC_HINTED;
			} else {
				vdec->fr_hint_state = VDEC_NEED_HINT;
			}
		}
	}

	p->dolby_meta_with_el = 0;
	pr_debug("vdec_init, vf_provider_name = %s\n", p->vf_provider_name);
	vdec_input_prepare_bufs(/*prepared buffer for fast playing.*/
		&vdec->input,
		vdec->sys_info->width,
		vdec->sys_info->height);
	/* vdec is now ready to be active */
	vdec_set_status(vdec, VDEC_STATUS_DISCONNECTED);

	return 0;

error:
	return r;
}
EXPORT_SYMBOL(vdec_init);

/* vdec_create/init/release/destroy are applied to both dual running decoders
 */
void vdec_release(struct vdec_s *vdec)
{
	//trace_vdec_release(vdec);/*DEBUG_TMP*/
#ifdef VDEC_DEBUG_SUPPORT
	if (step_mode) {
		pr_info("VDEC_DEBUG: in step_mode, wait release\n");
		while (step_mode)
			udelay(10);
		pr_info("VDEC_DEBUG: step_mode is clear\n");
	}
#endif
	vdec_disconnect(vdec);

	if (vdec->vframe_provider.name) {
		if (!vdec_single(vdec)) {
			if (vdec_core->hint_fr_vdec == vdec
			&& vdec->fr_hint_state == VDEC_HINTED
			&& !vdec->is_reset)
				vf_notify_receiver(
					vdec->vf_provider_name,
					VFRAME_EVENT_PROVIDER_FR_END_HINT,
					NULL);
			vdec->fr_hint_state = VDEC_NO_NEED_HINT;
		}
		vf_unreg_provider(&vdec->vframe_provider);
	}

	if (vdec_core->vfm_vdec == vdec)
		vdec_core->vfm_vdec = NULL;

	if (vdec_core->hint_fr_vdec == vdec)
		vdec_core->hint_fr_vdec = NULL;

	if (vdec->vf_receiver_inst >= 0) {
		if (vdec->vfm_map_id[0]) {
			vfm_map_remove(vdec->vfm_map_id);
			vdec->vfm_map_id[0] = 0;
		}
	}

	while ((atomic_read(&vdec->inirq_flag) > 0)
		|| (atomic_read(&vdec->inirq_thread_flag) > 0))
		schedule();

#ifdef FRAME_CHECK
	vdec_frame_check_exit(vdec);
#endif
	vdec_fps_clear(vdec->id);

	platform_device_unregister(vdec->dev);
	pr_debug("vdec_release instance %p, total %d\n", vdec,
		atomic_read(&vdec_core->vdec_nr));
	vdec_destroy(vdec);

	mutex_lock(&vdec_mutex);
	inited_vcodec_num--;
	mutex_unlock(&vdec_mutex);

}
EXPORT_SYMBOL(vdec_release);

/* For dual running decoders, vdec_reset is only called with master vdec.
 */
int vdec_reset(struct vdec_s *vdec)
{
	//trace_vdec_reset(vdec); /*DEBUG_TMP*/

	vdec_disconnect(vdec);

	if (vdec->vframe_provider.name)
		vf_unreg_provider(&vdec->vframe_provider);

	if ((vdec->slave) && (vdec->slave->vframe_provider.name))
		vf_unreg_provider(&vdec->slave->vframe_provider);

	if (vdec->reset) {
		vdec->reset(vdec);
		if (vdec->slave)
			vdec->slave->reset(vdec->slave);
	}
	vdec->mc_loaded = 0;/*clear for reload firmware*/
	vdec_input_release(&vdec->input);

	vdec_input_init(&vdec->input, vdec);

	vdec_input_prepare_bufs(&vdec->input, vdec->sys_info->width,
		vdec->sys_info->height);

	vf_reg_provider(&vdec->vframe_provider);
	vf_notify_receiver(vdec->vf_provider_name,
			VFRAME_EVENT_PROVIDER_START, vdec);

	if (vdec->slave) {
		vf_reg_provider(&vdec->slave->vframe_provider);
		vf_notify_receiver(vdec->slave->vf_provider_name,
			VFRAME_EVENT_PROVIDER_START, vdec->slave);
		vdec->slave->mc_loaded = 0;/*clear for reload firmware*/
	}

	vdec_connect(vdec);

	return 0;
}
EXPORT_SYMBOL(vdec_reset);

void vdec_free_cmabuf(void)
{
	mutex_lock(&vdec_mutex);

	/*if (inited_vcodec_num > 0) {
		mutex_unlock(&vdec_mutex);
		return;
	}*/
	mutex_unlock(&vdec_mutex);
}

void vdec_core_request(struct vdec_s *vdec, unsigned long mask)
{
	vdec->core_mask |= mask;

	if (vdec->slave)
		vdec->slave->core_mask |= mask;
	if (vdec_core->parallel_dec == 1) {
		if (mask & CORE_MASK_COMBINE)
			vdec_core->vdec_combine_flag++;
	}

}
EXPORT_SYMBOL(vdec_core_request);

int vdec_core_release(struct vdec_s *vdec, unsigned long mask)
{
	vdec->core_mask &= ~mask;

	if (vdec->slave)
		vdec->slave->core_mask &= ~mask;
	if (vdec_core->parallel_dec == 1) {
		if (mask & CORE_MASK_COMBINE)
			vdec_core->vdec_combine_flag--;
	}
	return 0;
}
EXPORT_SYMBOL(vdec_core_release);

bool vdec_core_with_input(unsigned long mask)
{
	enum vdec_type_e type;

	for (type = VDEC_1; type < VDEC_MAX; type++) {
		if ((mask & (1 << type)) && cores_with_input[type])
			return true;
	}

	return false;
}

void vdec_core_finish_run(struct vdec_s *vdec, unsigned long mask)
{
	unsigned long i;
	unsigned long t = mask;
	mutex_lock(&vdec_mutex);
	while (t) {
		i = __ffs(t);
		clear_bit(i, &vdec->active_mask);
		t &= ~(1 << i);
	}

	if (vdec->active_mask == 0)
		vdec_set_status(vdec, VDEC_STATUS_CONNECTED);

	mutex_unlock(&vdec_mutex);
}
EXPORT_SYMBOL(vdec_core_finish_run);
/*
 * find what core resources are available for vdec
 */
static unsigned long vdec_schedule_mask(struct vdec_s *vdec,
	unsigned long active_mask)
{
	unsigned long mask = vdec->core_mask &
		~CORE_MASK_COMBINE;

	if (vdec->core_mask & CORE_MASK_COMBINE) {
		/* combined cores must be granted together */
		if ((mask & ~active_mask) == mask)
			return mask;
		else
			return 0;
	} else
		return mask & ~vdec->sched_mask & ~active_mask;
}

/*
 *Decoder callback
 * Each decoder instance uses this callback to notify status change, e.g. when
 * decoder finished using HW resource.
 * a sample callback from decoder's driver is following:
 *
 *        if (hw->vdec_cb) {
 *            vdec_set_next_status(vdec, VDEC_STATUS_CONNECTED);
 *            hw->vdec_cb(vdec, hw->vdec_cb_arg);
 *        }
 */
static void vdec_callback(struct vdec_s *vdec, void *data)
{
	struct vdec_core_s *core = (struct vdec_core_s *)data;

#ifdef CONFIG_AMLOGIC_MEDIA_MULTI_DEC
	vdec_profile(vdec, VDEC_PROFILE_EVENT_CB);
#endif

	up(&core->sem);
}

static irqreturn_t vdec_isr(int irq, void *dev_id)
{
	struct vdec_isr_context_s *c =
		(struct vdec_isr_context_s *)dev_id;
	struct vdec_s *vdec = vdec_core->last_vdec;
	irqreturn_t ret = IRQ_HANDLED;

	if (vdec_core->parallel_dec == 1) {
		if (irq == vdec_core->isr_context[VDEC_IRQ_0].irq)
			vdec = vdec_core->active_hevc;
		else if (irq == vdec_core->isr_context[VDEC_IRQ_1].irq)
			vdec = vdec_core->active_vdec;
		else
			vdec = NULL;
	}

	if (vdec)
		atomic_set(&vdec->inirq_flag, 1);
	if (c->dev_isr) {
		ret = c->dev_isr(irq, c->dev_id);
		goto isr_done;
	}

	if ((c != &vdec_core->isr_context[VDEC_IRQ_0]) &&
	    (c != &vdec_core->isr_context[VDEC_IRQ_1]) &&
	    (c != &vdec_core->isr_context[VDEC_IRQ_HEVC_BACK])) {
#if 0
		pr_warn("vdec interrupt w/o a valid receiver\n");
#endif
		goto isr_done;
	}

	if (!vdec) {
#if 0
		pr_warn("vdec interrupt w/o an active instance running. core = %p\n",
			core);
#endif
		goto isr_done;
	}

	if (!vdec->irq_handler) {
#if 0
		pr_warn("vdec instance has no irq handle.\n");
#endif
		goto  isr_done;
	}

	ret = vdec->irq_handler(vdec, c->index);
isr_done:
	if (vdec)
		atomic_set(&vdec->inirq_flag, 0);
	return ret;
}

static irqreturn_t vdec_thread_isr(int irq, void *dev_id)
{
	struct vdec_isr_context_s *c =
		(struct vdec_isr_context_s *)dev_id;
	struct vdec_s *vdec = vdec_core->last_vdec;
	irqreturn_t ret = IRQ_HANDLED;

	if (vdec_core->parallel_dec == 1) {
		if (irq == vdec_core->isr_context[VDEC_IRQ_0].irq)
			vdec = vdec_core->active_hevc;
		else if (irq == vdec_core->isr_context[VDEC_IRQ_1].irq)
			vdec = vdec_core->active_vdec;
		else
			vdec = NULL;
	}

	if (vdec)
		atomic_set(&vdec->inirq_thread_flag, 1);
	if (c->dev_threaded_isr) {
		ret = c->dev_threaded_isr(irq, c->dev_id);
		goto thread_isr_done;
	}
	if (!vdec)
		goto thread_isr_done;

	if (!vdec->threaded_irq_handler)
		goto thread_isr_done;
	ret = vdec->threaded_irq_handler(vdec, c->index);
thread_isr_done:
	if (vdec)
		atomic_set(&vdec->inirq_thread_flag, 0);
	return ret;
}

unsigned long vdec_ready_to_run(struct vdec_s *vdec, unsigned long mask)
{
	unsigned long ready_mask;
	struct vdec_input_s *input = &vdec->input;
	if ((vdec->status != VDEC_STATUS_CONNECTED) &&
	    (vdec->status != VDEC_STATUS_ACTIVE))
		return false;

	if (!vdec->run_ready)
		return false;

	if ((vdec->slave || vdec->master) &&
		(vdec->sched == 0))
		return false;
#ifdef VDEC_DEBUG_SUPPORT
	inc_profi_count(mask, vdec->check_count);
#endif
	if (vdec_core_with_input(mask)) {
		/* check frame based input underrun */
		if (input && !input->eos && input_frame_based(input)
			&& (!vdec_input_next_chunk(input))) {
#ifdef VDEC_DEBUG_SUPPORT
			inc_profi_count(mask, vdec->input_underrun_count);
#endif
			return false;
		}
		/* check streaming prepare level threshold if not EOS */
		if (input && input_stream_based(input) && !input->eos) {
			u32 rp, wp, level;

			rp = READ_PARSER_REG(PARSER_VIDEO_RP);
			wp = READ_PARSER_REG(PARSER_VIDEO_WP);
			if (wp < rp)
				level = input->size + wp - rp;
			else
				level = wp - rp;

			if ((level < input->prepare_level) &&
				(pts_get_rec_num(PTS_TYPE_VIDEO,
					vdec->input.total_rd_count) < 2)) {
				vdec->need_more_data |= VDEC_NEED_MORE_DATA;
#ifdef VDEC_DEBUG_SUPPORT
				inc_profi_count(mask, vdec->input_underrun_count);
				if (step_mode & 0x200) {
					if ((step_mode & 0xff) == vdec->id) {
						step_mode |= 0xff;
						return mask;
					}
				}
#endif
				return false;
			} else if (level > input->prepare_level)
				vdec->need_more_data &= ~VDEC_NEED_MORE_DATA;
		}
	}

	if (step_mode) {
		if ((step_mode & 0xff) != vdec->id)
			return 0;
		step_mode |= 0xff; /*VDEC_DEBUG_SUPPORT*/
	}

	/*step_mode &= ~0xff; not work for id of 0, removed*/

#ifdef CONFIG_AMLOGIC_MEDIA_MULTI_DEC
	vdec_profile(vdec, VDEC_PROFILE_EVENT_CHK_RUN_READY);
#endif

	ready_mask = vdec->run_ready(vdec, mask) & mask;
#ifdef VDEC_DEBUG_SUPPORT
	if (ready_mask != mask)
		inc_profi_count(ready_mask ^ mask, vdec->not_run_ready_count);
#endif
#ifdef CONFIG_AMLOGIC_MEDIA_MULTI_DEC
	if (ready_mask)
		vdec_profile(vdec, VDEC_PROFILE_EVENT_RUN_READY);
#endif

	return ready_mask;
}

/* bridge on/off vdec's interrupt processing to vdec core */
static void vdec_route_interrupt(struct vdec_s *vdec, unsigned long mask,
	bool enable)
{
	enum vdec_type_e type;

	for (type = VDEC_1; type < VDEC_MAX; type++) {
		if (mask & (1 << type)) {
			struct vdec_isr_context_s *c =
				&vdec_core->isr_context[cores_int[type]];
			if (enable)
				c->vdec = vdec;
			else if (c->vdec == vdec)
				c->vdec = NULL;
		}
	}
}

/*
 * Set up secure protection for each decoder instance running.
 * Note: The operation from REE side only resets memory access
 * to a default policy and even a non_secure type will still be
 * changed to secure type automatically when secure source is
 * detected inside TEE.
 * Perform need_more_data checking and set flag is decoder
 * is not consuming data.
 */
void vdec_prepare_run(struct vdec_s *vdec, unsigned long mask)
{
	struct vdec_input_s *input = &vdec->input;
	int secure = (vdec_secure(vdec)) ? DMC_DEV_TYPE_SECURE :
			DMC_DEV_TYPE_NON_SECURE;

	vdec_route_interrupt(vdec, mask, true);

	if (!vdec_core_with_input(mask))
		return;

	if (input->target == VDEC_INPUT_TARGET_VLD)
		tee_config_device_secure(DMC_DEV_ID_VDEC, secure);
	else if (input->target == VDEC_INPUT_TARGET_HEVC)
		tee_config_device_secure(DMC_DEV_ID_HEVC, secure);

	if (vdec_stream_based(vdec) &&
		((vdec->need_more_data & VDEC_NEED_MORE_DATA_RUN) &&
		(vdec->need_more_data & VDEC_NEED_MORE_DATA_DIRTY) == 0)) {
		vdec->need_more_data |= VDEC_NEED_MORE_DATA;
	}

	vdec->need_more_data |= VDEC_NEED_MORE_DATA_RUN;
	vdec->need_more_data &= ~VDEC_NEED_MORE_DATA_DIRTY;
}

/* struct vdec_core_shread manages all decoder instance in active list. When
 * a vdec is added into the active list, it can onlt be in two status:
 * VDEC_STATUS_CONNECTED(the decoder does not own HW resource and ready to run)
 * VDEC_STATUS_ACTIVE(the decoder owns HW resources and is running).
 * Removing a decoder from active list is only performed within core thread.
 * Adding a decoder into active list is performed from user thread.
 */
static int vdec_core_thread(void *data)
{
	struct vdec_core_s *core = (struct vdec_core_s *)data;
	struct sched_param param = {.sched_priority = MAX_RT_PRIO/2};
	int i;

	sched_setscheduler(current, SCHED_FIFO, &param);

	allow_signal(SIGTERM);

	while (down_interruptible(&core->sem) == 0) {
		struct vdec_s *vdec, *tmp, *worker;
		unsigned long sched_mask = 0;
		LIST_HEAD(disconnecting_list);

		if (kthread_should_stop())
			break;
		mutex_lock(&vdec_mutex);

		if (core->parallel_dec == 1) {
			for (i = VDEC_1; i < VDEC_MAX; i++) {
				core->power_ref_mask =
					core->power_ref_count[i] > 0 ?
					(core->power_ref_mask | (1 << i)) :
					(core->power_ref_mask & ~(1 << i));
			}
		}
		/* clean up previous active vdec's input */
		list_for_each_entry(vdec, &core->connected_vdec_list, list) {
			unsigned long mask = vdec->sched_mask &
				(vdec->active_mask ^ vdec->sched_mask);

			vdec_route_interrupt(vdec, mask, false);

#ifdef VDEC_DEBUG_SUPPORT
			update_profi_clk_stop(vdec, mask, get_current_clk());
#endif
			/*
			 * If decoder released some core resources (mask), then
			 * check if these core resources are associated
			 * with any input side and do input clean up accordingly
			 */
			if (vdec_core_with_input(mask)) {
				struct vdec_input_s *input = &vdec->input;
				while (!list_empty(
					&input->vframe_chunk_list)) {
					struct vframe_chunk_s *chunk =
						vdec_input_next_chunk(input);
					if (chunk && (chunk->flag &
						VFRAME_CHUNK_FLAG_CONSUMED))
						vdec_input_release_chunk(input,
							chunk);
					else
						break;
				}

				vdec_save_input_context(vdec);
			}

			vdec->sched_mask &= ~mask;
			core->sched_mask &= ~mask;
		}

		/*
		 *todo:
		 * this is the case when the decoder is in active mode and
		 * the system side wants to stop it. Currently we rely on
		 * the decoder instance to go back to VDEC_STATUS_CONNECTED
		 * from VDEC_STATUS_ACTIVE by its own. However, if for some
		 * reason the decoder can not exist by itself (dead decoding
		 * or whatever), then we may have to add another vdec API
		 * to kill the vdec and release its HW resource and make it
		 * become inactive again.
		 * if ((core->active_vdec) &&
		 * (core->active_vdec->status == VDEC_STATUS_DISCONNECTED)) {
		 * }
		 */

		/* check disconnected decoders */
		list_for_each_entry_safe(vdec, tmp,
			&core->connected_vdec_list, list) {
			if ((vdec->status == VDEC_STATUS_CONNECTED) &&
			    (vdec->next_status == VDEC_STATUS_DISCONNECTED)) {
				if (core->parallel_dec == 1) {
					if (vdec_core->active_hevc == vdec)
						vdec_core->active_hevc = NULL;
					if (vdec_core->active_vdec == vdec)
						vdec_core->active_vdec = NULL;
				}
				if (core->last_vdec == vdec)
					core->last_vdec = NULL;
				list_move(&vdec->list, &disconnecting_list);
			}
		}
		mutex_unlock(&vdec_mutex);
		/* elect next vdec to be scheduled */
		vdec = core->last_vdec;
		if (vdec) {
			vdec = list_entry(vdec->list.next, struct vdec_s, list);
			list_for_each_entry_from(vdec,
				&core->connected_vdec_list, list) {
				sched_mask = vdec_schedule_mask(vdec,
					core->sched_mask);
				if (!sched_mask)
					continue;
				sched_mask = vdec_ready_to_run(vdec,
					sched_mask);
				if (sched_mask)
					break;
			}

			if (&vdec->list == &core->connected_vdec_list)
				vdec = NULL;
		}

		if (!vdec) {
			/* search from beginning */
			list_for_each_entry(vdec,
				&core->connected_vdec_list, list) {
				sched_mask = vdec_schedule_mask(vdec,
					core->sched_mask);
				if (vdec == core->last_vdec) {
					if (!sched_mask) {
						vdec = NULL;
						break;
					}

					sched_mask = vdec_ready_to_run(vdec,
						sched_mask);

					if (!sched_mask) {
						vdec = NULL;
						break;
					}
					break;
				}

				if (!sched_mask)
					continue;

				sched_mask = vdec_ready_to_run(vdec,
					sched_mask);
				if (sched_mask)
					break;
			}

			if (&vdec->list == &core->connected_vdec_list)
				vdec = NULL;
		}

		worker = vdec;

		if (vdec) {
			unsigned long mask = sched_mask;
			unsigned long i;

			/* setting active_mask should be atomic.
			 * it can be modified by decoder driver callbacks.
			 */
			while (sched_mask) {
				i = __ffs(sched_mask);
				set_bit(i, &vdec->active_mask);
				sched_mask &= ~(1 << i);
			}

			/* vdec's sched_mask is only set from core thread */
			vdec->sched_mask |= mask;
			if (core->last_vdec) {
				if ((core->last_vdec != vdec) &&
					(core->last_vdec->mc_type != vdec->mc_type))
					vdec->mc_loaded = 0;/*clear for reload firmware*/
			}
			core->last_vdec = vdec;
			if (debug & 2)
				vdec->mc_loaded = 0;/*alway reload firmware*/
			vdec_set_status(vdec, VDEC_STATUS_ACTIVE);

			core->sched_mask |= mask;
			if (core->parallel_dec == 1)
				vdec_save_active_hw(vdec);
#ifdef CONFIG_AMLOGIC_MEDIA_MULTI_DEC
			vdec_profile(vdec, VDEC_PROFILE_EVENT_RUN);
#endif
			vdec_prepare_run(vdec, mask);
#ifdef VDEC_DEBUG_SUPPORT
			inc_profi_count(mask, vdec->run_count);
			update_profi_clk_run(vdec, mask, get_current_clk());
#endif
			vdec->run(vdec, mask, vdec_callback, core);


			/* we have some cores scheduled, keep working until
			 * all vdecs are checked with no cores to schedule
			 */
			if (core->parallel_dec == 1) {
				if (vdec_core->vdec_combine_flag == 0)
					up(&core->sem);
			} else
				up(&core->sem);
		}

		/* remove disconnected decoder from active list */
		list_for_each_entry_safe(vdec, tmp, &disconnecting_list, list) {
			list_del(&vdec->list);
			vdec_set_status(vdec, VDEC_STATUS_DISCONNECTED);
			/*core->last_vdec = NULL;*/
			complete(&vdec->inactive_done);
		}

		/* if there is no new work scheduled and nothing
		 * is running, sleep 20ms
		 */
		if (core->parallel_dec == 1) {
			if (vdec_core->vdec_combine_flag == 0) {
				if ((!worker) &&
					((core->sched_mask != core->power_ref_mask)) &&
					(atomic_read(&vdec_core->vdec_nr) > 0)) {
						usleep_range(1000, 2000);
						up(&core->sem);
				}
			} else {
				if ((!worker) && (!core->sched_mask) && (atomic_read(&vdec_core->vdec_nr) > 0)) {
					usleep_range(1000, 2000);
					up(&core->sem);
				}
			}
		} else if ((!worker) && (!core->sched_mask) && (atomic_read(&vdec_core->vdec_nr) > 0)) {
			usleep_range(1000, 2000);
			up(&core->sem);
		}

	}

	return 0;
}

#if 1				/* MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON8 */
static bool test_hevc(u32 decomp_addr, u32 us_delay)
{
	int i;

	/* SW_RESET IPP */
	WRITE_VREG(HEVCD_IPP_TOP_CNTL, 1);
	WRITE_VREG(HEVCD_IPP_TOP_CNTL, 0);

	/* initialize all canvas table */
	WRITE_VREG(HEVCD_MPP_ANC2AXI_TBL_CONF_ADDR, 0);
	for (i = 0; i < 32; i++)
		WRITE_VREG(HEVCD_MPP_ANC2AXI_TBL_CMD_ADDR,
			0x1 | (i << 8) | decomp_addr);
	WRITE_VREG(HEVCD_MPP_ANC2AXI_TBL_CONF_ADDR, 1);
	WRITE_VREG(HEVCD_MPP_ANC_CANVAS_ACCCONFIG_ADDR, (0 << 8) | (0<<1) | 1);
	for (i = 0; i < 32; i++)
		WRITE_VREG(HEVCD_MPP_ANC_CANVAS_DATA_ADDR, 0);

	/* Initialize mcrcc */
	WRITE_VREG(HEVCD_MCRCC_CTL1, 0x2);
	WRITE_VREG(HEVCD_MCRCC_CTL2, 0x0);
	WRITE_VREG(HEVCD_MCRCC_CTL3, 0x0);
	WRITE_VREG(HEVCD_MCRCC_CTL1, 0xff0);

	/* Decomp initialize */
	WRITE_VREG(HEVCD_MPP_DECOMP_CTL1, 0x0);
	WRITE_VREG(HEVCD_MPP_DECOMP_CTL2, 0x0);

	/* Frame level initialization */
	WRITE_VREG(HEVCD_IPP_TOP_FRMCONFIG, 0x100 | (0x100 << 16));
	WRITE_VREG(HEVCD_IPP_TOP_TILECONFIG3, 0x0);
	WRITE_VREG(HEVCD_IPP_TOP_LCUCONFIG, 0x1 << 5);
	WRITE_VREG(HEVCD_IPP_BITDEPTH_CONFIG, 0x2 | (0x2 << 2));

	WRITE_VREG(HEVCD_IPP_CONFIG, 0x0);
	WRITE_VREG(HEVCD_IPP_LINEBUFF_BASE, 0x0);

	/* Enable SWIMP mode */
	WRITE_VREG(HEVCD_IPP_SWMPREDIF_CONFIG, 0x1);

	/* Enable frame */
	WRITE_VREG(HEVCD_IPP_TOP_CNTL, 0x2);
	WRITE_VREG(HEVCD_IPP_TOP_FRMCTL, 0x1);

	/* Send SW-command CTB info */
	WRITE_VREG(HEVCD_IPP_SWMPREDIF_CTBINFO, 0x1 << 31);

	/* Send PU_command */
	WRITE_VREG(HEVCD_IPP_SWMPREDIF_PUINFO0, (0x4 << 9) | (0x4 << 16));
	WRITE_VREG(HEVCD_IPP_SWMPREDIF_PUINFO1, 0x1 << 3);
	WRITE_VREG(HEVCD_IPP_SWMPREDIF_PUINFO2, 0x0);
	WRITE_VREG(HEVCD_IPP_SWMPREDIF_PUINFO3, 0x0);

	udelay(us_delay);

	WRITE_VREG(HEVCD_IPP_DBG_SEL, 0x2 << 4);

	return (READ_VREG(HEVCD_IPP_DBG_DATA) & 3) == 1;
}

void vdec_power_reset(void)
{
	/* enable vdec1 isolation */
	WRITE_AOREG(AO_RTI_GEN_PWR_ISO0,
		READ_AOREG(AO_RTI_GEN_PWR_ISO0) | 0xc0);
	/* power off vdec1 memories */
	WRITE_VREG(DOS_MEM_PD_VDEC, 0xffffffffUL);
	/* vdec1 power off */
	WRITE_AOREG(AO_RTI_GEN_PWR_SLEEP0,
		READ_AOREG(AO_RTI_GEN_PWR_SLEEP0) | 0xc);

	if (has_vdec2()) {
		/* enable vdec2 isolation */
		WRITE_AOREG(AO_RTI_GEN_PWR_ISO0,
			READ_AOREG(AO_RTI_GEN_PWR_ISO0) | 0x300);
		/* power off vdec2 memories */
		WRITE_VREG(DOS_MEM_PD_VDEC2, 0xffffffffUL);
		/* vdec2 power off */
		WRITE_AOREG(AO_RTI_GEN_PWR_SLEEP0,
			READ_AOREG(AO_RTI_GEN_PWR_SLEEP0) | 0x30);
	}

	if (has_hdec()) {
		/* enable hcodec isolation */
		WRITE_AOREG(AO_RTI_GEN_PWR_ISO0,
			READ_AOREG(AO_RTI_GEN_PWR_ISO0) | 0x30);
		/* power off hcodec memories */
		WRITE_VREG(DOS_MEM_PD_HCODEC, 0xffffffffUL);
		/* hcodec power off */
		WRITE_AOREG(AO_RTI_GEN_PWR_SLEEP0,
			READ_AOREG(AO_RTI_GEN_PWR_SLEEP0) | 3);
	}

	if (has_hevc_vdec()) {
		/* enable hevc isolation */
		WRITE_AOREG(AO_RTI_GEN_PWR_ISO0,
			READ_AOREG(AO_RTI_GEN_PWR_ISO0) | 0xc00);
		/* power off hevc memories */
		WRITE_VREG(DOS_MEM_PD_HEVC, 0xffffffffUL);
		/* hevc power off */
		WRITE_AOREG(AO_RTI_GEN_PWR_SLEEP0,
			READ_AOREG(AO_RTI_GEN_PWR_SLEEP0) | 0xc0);
	}
}
EXPORT_SYMBOL(vdec_power_reset);

void vdec_poweron(enum vdec_type_e core)
{
	void *decomp_addr = NULL;
	dma_addr_t decomp_dma_addr;
	u32 decomp_addr_aligned = 0;
	int hevc_loop = 0;

	if (core >= VDEC_MAX)
		return;

	mutex_lock(&vdec_mutex);

	vdec_core->power_ref_count[core]++;
	if (vdec_core->power_ref_count[core] > 1) {
		mutex_unlock(&vdec_mutex);
		return;
	}

	if (vdec_on(core)) {
		mutex_unlock(&vdec_mutex);
		return;
	}

	if (hevc_workaround_needed() &&
		(core == VDEC_HEVC)) {
		decomp_addr = codec_mm_dma_alloc_coherent(MEM_NAME,
			SZ_64K + SZ_4K, &decomp_dma_addr, GFP_KERNEL, 0);

		if (decomp_addr) {
			decomp_addr_aligned = ALIGN(decomp_dma_addr, SZ_64K);
			memset((u8 *)decomp_addr +
				(decomp_addr_aligned - decomp_dma_addr),
				0xff, SZ_4K);
		} else
			pr_err("vdec: alloc HEVC gxbb decomp buffer failed.\n");
	}

	if (core == VDEC_1) {
		/* vdec1 power on */
		WRITE_AOREG(AO_RTI_GEN_PWR_SLEEP0,
				READ_AOREG(AO_RTI_GEN_PWR_SLEEP0) &
				(get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_SM1
				? ~0x2 : ~0xc));
		/* wait 10uS */
		udelay(10);
		/* vdec1 soft reset */
		WRITE_VREG(DOS_SW_RESET0, 0xfffffffc);
		WRITE_VREG(DOS_SW_RESET0, 0);
		/* enable vdec1 clock */
		/*
		 *add power on vdec clock level setting,only for m8 chip,
		 * m8baby and m8m2 can dynamic adjust vdec clock,
		 * power on with default clock level
		 */
		amports_switch_gate("clk_vdec_mux", 1);
		vdec_clock_hi_enable();
		/* power up vdec memories */
		WRITE_VREG(DOS_MEM_PD_VDEC, 0);
		/* remove vdec1 isolation */
		WRITE_AOREG(AO_RTI_GEN_PWR_ISO0,
				READ_AOREG(AO_RTI_GEN_PWR_ISO0) &
				(get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_SM1
				? ~0x2 : ~0xC0));
		/* reset DOS top registers */
		WRITE_VREG(DOS_VDEC_MCRCC_STALL_CTRL, 0);
		if (get_cpu_major_id() >=
			AM_MESON_CPU_MAJOR_ID_GXBB) {
			/*
			 *enable VDEC_1 DMC request
			 */
			unsigned long flags;

			spin_lock_irqsave(&vdec_spin_lock, flags);
			codec_dmcbus_write(DMC_REQ_CTRL,
				codec_dmcbus_read(DMC_REQ_CTRL) | (1 << 13));
			spin_unlock_irqrestore(&vdec_spin_lock, flags);
		}
	} else if (core == VDEC_2) {
		if (has_vdec2()) {
			/* vdec2 power on */
			WRITE_AOREG(AO_RTI_GEN_PWR_SLEEP0,
					READ_AOREG(AO_RTI_GEN_PWR_SLEEP0) &
					~0x30);
			/* wait 10uS */
			udelay(10);
			/* vdec2 soft reset */
			WRITE_VREG(DOS_SW_RESET2, 0xffffffff);
			WRITE_VREG(DOS_SW_RESET2, 0);
			/* enable vdec1 clock */
			vdec2_clock_hi_enable();
			/* power up vdec memories */
			WRITE_VREG(DOS_MEM_PD_VDEC2, 0);
			/* remove vdec2 isolation */
			WRITE_AOREG(AO_RTI_GEN_PWR_ISO0,
					READ_AOREG(AO_RTI_GEN_PWR_ISO0) &
					~0x300);
			/* reset DOS top registers */
			WRITE_VREG(DOS_VDEC2_MCRCC_STALL_CTRL, 0);
		}
	} else if (core == VDEC_HCODEC) {
		if (has_hdec()) {
			/* hcodec power on */
			WRITE_AOREG(AO_RTI_GEN_PWR_SLEEP0,
				READ_AOREG(AO_RTI_GEN_PWR_SLEEP0) &
				(get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_SM1
				? ~0x1 : ~0x3));
			/* wait 10uS */
			udelay(10);
			/* hcodec soft reset */
			WRITE_VREG(DOS_SW_RESET1, 0xffffffff);
			WRITE_VREG(DOS_SW_RESET1, 0);
			/* enable hcodec clock */
			hcodec_clock_enable();
			/* power up hcodec memories */
			WRITE_VREG(DOS_MEM_PD_HCODEC, 0);
			/* remove hcodec isolation */
			WRITE_AOREG(AO_RTI_GEN_PWR_ISO0,
				READ_AOREG(AO_RTI_GEN_PWR_ISO0) &
				(get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_SM1
				? ~0x1 : ~0x30));
		}
	} else if (core == VDEC_HEVC) {
		if (has_hevc_vdec()) {
			bool hevc_fixed = false;

			while (!hevc_fixed) {
				/* hevc power on */
				WRITE_AOREG(AO_RTI_GEN_PWR_SLEEP0,
					READ_AOREG(AO_RTI_GEN_PWR_SLEEP0) &
					(get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_SM1
					? ~0x4 : ~0xc0));
				/* wait 10uS */
				udelay(10);
				/* hevc soft reset */
				WRITE_VREG(DOS_SW_RESET3, 0xffffffff);
				WRITE_VREG(DOS_SW_RESET3, 0);
				/* enable hevc clock */
				amports_switch_gate("clk_hevc_mux", 1);
				if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_G12A)
					amports_switch_gate("clk_hevcb_mux", 1);
				hevc_clock_hi_enable();
				hevc_back_clock_hi_enable();
				/* power up hevc memories */
				WRITE_VREG(DOS_MEM_PD_HEVC, 0);
				/* remove hevc isolation */
				WRITE_AOREG(AO_RTI_GEN_PWR_ISO0,
					READ_AOREG(AO_RTI_GEN_PWR_ISO0) &
					(get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_SM1
					? ~0x4 : ~0xc00));

				if (!hevc_workaround_needed())
					break;

				if (decomp_addr)
					hevc_fixed = test_hevc(
						decomp_addr_aligned, 20);

				if (!hevc_fixed) {
					hevc_loop++;

					mutex_unlock(&vdec_mutex);

					if (hevc_loop >= HEVC_TEST_LIMIT) {
						pr_warn("hevc power sequence over limit\n");
						pr_warn("=====================================================\n");
						pr_warn(" This chip is identified to have HW failure.\n");
						pr_warn(" Please contact sqa-platform to replace the platform.\n");
						pr_warn("=====================================================\n");

						panic("Force panic for chip detection !!!\n");

						break;
					}

					vdec_poweroff(VDEC_HEVC);

					mdelay(10);

					mutex_lock(&vdec_mutex);
				}
			}

			if (hevc_loop > hevc_max_reset_count)
				hevc_max_reset_count = hevc_loop;

			WRITE_VREG(DOS_SW_RESET3, 0xffffffff);
			udelay(10);
			WRITE_VREG(DOS_SW_RESET3, 0);
		}
	}

	if (decomp_addr)
		codec_mm_dma_free_coherent(MEM_NAME,
			SZ_64K + SZ_4K, decomp_addr, decomp_dma_addr, 0);

	mutex_unlock(&vdec_mutex);
}
EXPORT_SYMBOL(vdec_poweron);

void vdec_poweroff(enum vdec_type_e core)
{
	if (core >= VDEC_MAX)
		return;

	mutex_lock(&vdec_mutex);

	vdec_core->power_ref_count[core]--;
	if (vdec_core->power_ref_count[core] > 0) {
		mutex_unlock(&vdec_mutex);
		return;
	}

	if (core == VDEC_1) {
		if (get_cpu_major_id() >=
			AM_MESON_CPU_MAJOR_ID_GXBB) {
			/* disable VDEC_1 DMC REQ*/
			unsigned long flags;

			spin_lock_irqsave(&vdec_spin_lock, flags);
			codec_dmcbus_write(DMC_REQ_CTRL,
				codec_dmcbus_read(DMC_REQ_CTRL) & (~(1 << 13)));
			spin_unlock_irqrestore(&vdec_spin_lock, flags);
			udelay(10);
		}
		/* enable vdec1 isolation */
		WRITE_AOREG(AO_RTI_GEN_PWR_ISO0,
				READ_AOREG(AO_RTI_GEN_PWR_ISO0) |
				(get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_SM1
				? 0x2 : 0xc0));
		/* power off vdec1 memories */
		WRITE_VREG(DOS_MEM_PD_VDEC, 0xffffffffUL);
		/* disable vdec1 clock */
		vdec_clock_off();
		/* vdec1 power off */
		WRITE_AOREG(AO_RTI_GEN_PWR_SLEEP0,
				READ_AOREG(AO_RTI_GEN_PWR_SLEEP0) |
				(get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_SM1
				? 0x2 : 0xc));
	} else if (core == VDEC_2) {
		if (has_vdec2()) {
			/* enable vdec2 isolation */
			WRITE_AOREG(AO_RTI_GEN_PWR_ISO0,
					READ_AOREG(AO_RTI_GEN_PWR_ISO0) |
					0x300);
			/* power off vdec2 memories */
			WRITE_VREG(DOS_MEM_PD_VDEC2, 0xffffffffUL);
			/* disable vdec2 clock */
			vdec2_clock_off();
			/* vdec2 power off */
			WRITE_AOREG(AO_RTI_GEN_PWR_SLEEP0,
					READ_AOREG(AO_RTI_GEN_PWR_SLEEP0) |
					0x30);
		}
	} else if (core == VDEC_HCODEC) {
		if (has_hdec()) {
			/* enable hcodec isolation */
			WRITE_AOREG(AO_RTI_GEN_PWR_ISO0,
				READ_AOREG(AO_RTI_GEN_PWR_ISO0) |
				(get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_SM1
				? 0x1 : 0x30));
			/* power off hcodec memories */
			WRITE_VREG(DOS_MEM_PD_HCODEC, 0xffffffffUL);
			/* disable hcodec clock */
			hcodec_clock_off();
			/* hcodec power off */
			WRITE_AOREG(AO_RTI_GEN_PWR_SLEEP0,
				READ_AOREG(AO_RTI_GEN_PWR_SLEEP0) |
				(get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_SM1
				? 0x1 : 3));
		}
	} else if (core == VDEC_HEVC) {
		if (has_hevc_vdec()) {
			if (no_powerdown == 0) {
				/* enable hevc isolation */
				WRITE_AOREG(AO_RTI_GEN_PWR_ISO0,
					READ_AOREG(AO_RTI_GEN_PWR_ISO0) |
					(get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_SM1
					? 0x4 : 0xc00));
			/* power off hevc memories */
			WRITE_VREG(DOS_MEM_PD_HEVC, 0xffffffffUL);

			/* disable hevc clock */
			hevc_clock_off();
			if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_G12A)
				hevc_back_clock_off();

			/* hevc power off */
			WRITE_AOREG(AO_RTI_GEN_PWR_SLEEP0,
				READ_AOREG(AO_RTI_GEN_PWR_SLEEP0) |
				(get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_SM1
				? 0x4 : 0xc0));
			} else {
				pr_info("!!!!!!!!not power down\n");
				hevc_reset_core(NULL);
				no_powerdown = 0;
			}
		}
	}
	mutex_unlock(&vdec_mutex);
}
EXPORT_SYMBOL(vdec_poweroff);

bool vdec_on(enum vdec_type_e core)
{
	bool ret = false;

	if (core == VDEC_1) {
		if (((READ_AOREG(AO_RTI_GEN_PWR_SLEEP0) &
			(get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_SM1
			? 0x2 : 0xc)) == 0) &&
			(READ_HHI_REG(HHI_VDEC_CLK_CNTL) & 0x100))
			ret = true;
	} else if (core == VDEC_2) {
		if (has_vdec2()) {
			if (((READ_AOREG(AO_RTI_GEN_PWR_SLEEP0) & 0x30) == 0) &&
				(READ_HHI_REG(HHI_VDEC2_CLK_CNTL) & 0x100))
				ret = true;
		}
	} else if (core == VDEC_HCODEC) {
		if (has_hdec()) {
			if (((READ_AOREG(AO_RTI_GEN_PWR_SLEEP0) &
				(get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_SM1
				? 0x1 : 0x3)) == 0) &&
				(READ_HHI_REG(HHI_VDEC_CLK_CNTL) & 0x1000000))
				ret = true;
		}
	} else if (core == VDEC_HEVC) {
		if (has_hevc_vdec()) {
			if (((READ_AOREG(AO_RTI_GEN_PWR_SLEEP0) &
				(get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_SM1
				? 0x4 : 0xc0)) == 0) &&
				(READ_HHI_REG(HHI_VDEC2_CLK_CNTL) & 0x1000000))
				ret = true;
		}
	}

	return ret;
}
EXPORT_SYMBOL(vdec_on);

#elif 0				/* MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON6TVD */
void vdec_poweron(enum vdec_type_e core)
{
	ulong flags;

	spin_lock_irqsave(&lock, flags);

	if (core == VDEC_1) {
		/* vdec1 soft reset */
		WRITE_VREG(DOS_SW_RESET0, 0xfffffffc);
		WRITE_VREG(DOS_SW_RESET0, 0);
		/* enable vdec1 clock */
		vdec_clock_enable();
		/* reset DOS top registers */
		WRITE_VREG(DOS_VDEC_MCRCC_STALL_CTRL, 0);
	} else if (core == VDEC_2) {
		/* vdec2 soft reset */
		WRITE_VREG(DOS_SW_RESET2, 0xffffffff);
		WRITE_VREG(DOS_SW_RESET2, 0);
		/* enable vdec2 clock */
		vdec2_clock_enable();
		/* reset DOS top registers */
		WRITE_VREG(DOS_VDEC2_MCRCC_STALL_CTRL, 0);
	} else if (core == VDEC_HCODEC) {
		/* hcodec soft reset */
		WRITE_VREG(DOS_SW_RESET1, 0xffffffff);
		WRITE_VREG(DOS_SW_RESET1, 0);
		/* enable hcodec clock */
		hcodec_clock_enable();
	}

	spin_unlock_irqrestore(&lock, flags);
}

void vdec_poweroff(enum vdec_type_e core)
{
	ulong flags;

	spin_lock_irqsave(&lock, flags);

	if (core == VDEC_1) {
		/* disable vdec1 clock */
		vdec_clock_off();
	} else if (core == VDEC_2) {
		/* disable vdec2 clock */
		vdec2_clock_off();
	} else if (core == VDEC_HCODEC) {
		/* disable hcodec clock */
		hcodec_clock_off();
	}

	spin_unlock_irqrestore(&lock, flags);
}

bool vdec_on(enum vdec_type_e core)
{
	bool ret = false;

	if (core == VDEC_1) {
		if (READ_HHI_REG(HHI_VDEC_CLK_CNTL) & 0x100)
			ret = true;
	} else if (core == VDEC_2) {
		if (READ_HHI_REG(HHI_VDEC2_CLK_CNTL) & 0x100)
			ret = true;
	} else if (core == VDEC_HCODEC) {
		if (READ_HHI_REG(HHI_VDEC_CLK_CNTL) & 0x1000000)
			ret = true;
	}

	return ret;
}
#endif

int vdec_source_changed(int format, int width, int height, int fps)
{
	/* todo: add level routines for clock adjustment per chips */
	int ret = -1;
	static int on_setting;

	if (on_setting > 0)
		return ret;/*on changing clk,ignore this change*/

	if (vdec_source_get(VDEC_1) == width * height * fps)
		return ret;


	on_setting = 1;
	ret = vdec_source_changed_for_clk_set(format, width, height, fps);
	pr_debug("vdec1 video changed to %d x %d %d fps clk->%dMHZ\n",
			width, height, fps, vdec_clk_get(VDEC_1));
	on_setting = 0;
	return ret;

}
EXPORT_SYMBOL(vdec_source_changed);

void vdec_disable_DMC(struct vdec_s *vdec)
{
	/*close first,then wait pedding end,timing suggestion from vlsi*/
	unsigned long flags;
	spin_lock_irqsave(&vdec_spin_lock, flags);
	codec_dmcbus_write(DMC_REQ_CTRL,
		codec_dmcbus_read(DMC_REQ_CTRL) & (~(1 << 13)));
	spin_unlock_irqrestore(&vdec_spin_lock, flags);

	while (!(codec_dmcbus_read(DMC_CHAN_STS)
		& (1 << 13)))
		;
}
EXPORT_SYMBOL(vdec_disable_DMC);

void vdec_enable_DMC(struct vdec_s *vdec)
{
	unsigned long flags;
	spin_lock_irqsave(&vdec_spin_lock, flags);
	codec_dmcbus_write(DMC_REQ_CTRL,
	codec_dmcbus_read(DMC_REQ_CTRL) | (1 << 13));
	spin_unlock_irqrestore(&vdec_spin_lock, flags);
}

EXPORT_SYMBOL(vdec_enable_DMC);

void vdec_reset_core(struct vdec_s *vdec)
{
	unsigned long flags;
	spin_lock_irqsave(&vdec_spin_lock, flags);
	codec_dmcbus_write(DMC_REQ_CTRL,
		codec_dmcbus_read(DMC_REQ_CTRL) & (~(1 << 13)));
	spin_unlock_irqrestore(&vdec_spin_lock, flags);

	while (!(codec_dmcbus_read(DMC_CHAN_STS)
		& (1 << 13)))
		;
	/*
	 * 2: assist
	 * 3: vld_reset
	 * 4: vld_part_reset
	 * 5: vfifo reset
	 * 6: iqidct
	 * 7: mc
	 * 8: dblk
	 * 9: pic_dc
	 * 10: psc
	 * 11: mcpu
	 * 12: ccpu
	 * 13: ddr
	 * 14: afifo
	 */

	WRITE_VREG(DOS_SW_RESET0,
		(1<<3)|(1<<4)|(1<<5));

	WRITE_VREG(DOS_SW_RESET0, 0);

	spin_lock_irqsave(&vdec_spin_lock, flags);
	codec_dmcbus_write(DMC_REQ_CTRL,
		codec_dmcbus_read(DMC_REQ_CTRL) | (1 << 13));
	spin_unlock_irqrestore(&vdec_spin_lock, flags);
}
EXPORT_SYMBOL(vdec_reset_core);

void hevc_reset_core(struct vdec_s *vdec)
{
	unsigned long flags;
	WRITE_VREG(HEVC_STREAM_CONTROL, 0);
	spin_lock_irqsave(&vdec_spin_lock, flags);
	codec_dmcbus_write(DMC_REQ_CTRL,
		codec_dmcbus_read(DMC_REQ_CTRL) & (~(1 << 4)));
	spin_unlock_irqrestore(&vdec_spin_lock, flags);

	while (!(codec_dmcbus_read(DMC_CHAN_STS)
		& (1 << 4)))
		;

	if (vdec == NULL || input_frame_based(vdec))
		WRITE_VREG(HEVC_STREAM_CONTROL, 0);

		/*
	 * 2: assist
	 * 3: parser
	 * 4: parser_state
	 * 8: dblk
	 * 11:mcpu
	 * 12:ccpu
	 * 13:ddr
	 * 14:iqit
	 * 15:ipp
	 * 17:qdct
	 * 18:mpred
	 * 19:sao
	 * 24:hevc_afifo
	 */
	WRITE_VREG(DOS_SW_RESET3,
		(1<<3)|(1<<4)|(1<<8)|(1<<11)|
		(1<<12)|(1<<13)|(1<<14)|(1<<15)|
		(1<<17)|(1<<18)|(1<<19)|(1<<24));

	WRITE_VREG(DOS_SW_RESET3, 0);


	spin_lock_irqsave(&vdec_spin_lock, flags);
	codec_dmcbus_write(DMC_REQ_CTRL,
		codec_dmcbus_read(DMC_REQ_CTRL) | (1 << 4));
	spin_unlock_irqrestore(&vdec_spin_lock, flags);

}
EXPORT_SYMBOL(hevc_reset_core);

int vdec2_source_changed(int format, int width, int height, int fps)
{
	int ret = -1;
	static int on_setting;

	if (has_vdec2()) {
		/* todo: add level routines for clock adjustment per chips */
		if (on_setting != 0)
			return ret;/*on changing clk,ignore this change*/

		if (vdec_source_get(VDEC_2) == width * height * fps)
			return ret;

		on_setting = 1;
		ret = vdec_source_changed_for_clk_set(format,
					width, height, fps);
		pr_debug("vdec2 video changed to %d x %d %d fps clk->%dMHZ\n",
			width, height, fps, vdec_clk_get(VDEC_2));
		on_setting = 0;
		return ret;
	}
	return 0;
}
EXPORT_SYMBOL(vdec2_source_changed);

int hevc_source_changed(int format, int width, int height, int fps)
{
	/* todo: add level routines for clock adjustment per chips */
	int ret = -1;
	static int on_setting;

	if (on_setting != 0)
		return ret;/*on changing clk,ignore this change*/

	if (vdec_source_get(VDEC_HEVC) == width * height * fps)
		return ret;

	on_setting = 1;
	ret = vdec_source_changed_for_clk_set(format, width, height, fps);
	pr_debug("hevc video changed to %d x %d %d fps clk->%dMHZ\n",
			width, height, fps, vdec_clk_get(VDEC_HEVC));
	on_setting = 0;

	return ret;
}
EXPORT_SYMBOL(hevc_source_changed);

static struct am_reg am_risc[] = {
	{"MSP", 0x300},
	{"MPSR", 0x301},
	{"MCPU_INT_BASE", 0x302},
	{"MCPU_INTR_GRP", 0x303},
	{"MCPU_INTR_MSK", 0x304},
	{"MCPU_INTR_REQ", 0x305},
	{"MPC-P", 0x306},
	{"MPC-D", 0x307},
	{"MPC_E", 0x308},
	{"MPC_W", 0x309},
	{"CSP", 0x320},
	{"CPSR", 0x321},
	{"CCPU_INT_BASE", 0x322},
	{"CCPU_INTR_GRP", 0x323},
	{"CCPU_INTR_MSK", 0x324},
	{"CCPU_INTR_REQ", 0x325},
	{"CPC-P", 0x326},
	{"CPC-D", 0x327},
	{"CPC_E", 0x328},
	{"CPC_W", 0x329},
	{"AV_SCRATCH_0", 0x09c0},
	{"AV_SCRATCH_1", 0x09c1},
	{"AV_SCRATCH_2", 0x09c2},
	{"AV_SCRATCH_3", 0x09c3},
	{"AV_SCRATCH_4", 0x09c4},
	{"AV_SCRATCH_5", 0x09c5},
	{"AV_SCRATCH_6", 0x09c6},
	{"AV_SCRATCH_7", 0x09c7},
	{"AV_SCRATCH_8", 0x09c8},
	{"AV_SCRATCH_9", 0x09c9},
	{"AV_SCRATCH_A", 0x09ca},
	{"AV_SCRATCH_B", 0x09cb},
	{"AV_SCRATCH_C", 0x09cc},
	{"AV_SCRATCH_D", 0x09cd},
	{"AV_SCRATCH_E", 0x09ce},
	{"AV_SCRATCH_F", 0x09cf},
	{"AV_SCRATCH_G", 0x09d0},
	{"AV_SCRATCH_H", 0x09d1},
	{"AV_SCRATCH_I", 0x09d2},
	{"AV_SCRATCH_J", 0x09d3},
	{"AV_SCRATCH_K", 0x09d4},
	{"AV_SCRATCH_L", 0x09d5},
	{"AV_SCRATCH_M", 0x09d6},
	{"AV_SCRATCH_N", 0x09d7},
};

static ssize_t amrisc_regs_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	char *pbuf = buf;
	struct am_reg *regs = am_risc;
	int rsize = sizeof(am_risc) / sizeof(struct am_reg);
	int i;
	unsigned int val;
	ssize_t ret;

	if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_M8) {
		mutex_lock(&vdec_mutex);
		if (!vdec_on(VDEC_1)) {
			mutex_unlock(&vdec_mutex);
			pbuf += sprintf(pbuf, "amrisc is power off\n");
			ret = pbuf - buf;
			return ret;
		}
	} else if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_M6) {
		/*TODO:M6 define */
		/*
		 *  switch_mod_gate_by_type(MOD_VDEC, 1);
		 */
		amports_switch_gate("vdec", 1);
	}
	pbuf += sprintf(pbuf, "amrisc registers show:\n");
	for (i = 0; i < rsize; i++) {
		val = READ_VREG(regs[i].offset);
		pbuf += sprintf(pbuf, "%s(%#x)\t:%#x(%d)\n",
				regs[i].name, regs[i].offset, val, val);
	}
	if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_M8)
		mutex_unlock(&vdec_mutex);
	else if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_M6) {
		/*TODO:M6 define */
		/*
		 *  switch_mod_gate_by_type(MOD_VDEC, 0);
		 */
		amports_switch_gate("vdec", 0);
	}
	ret = pbuf - buf;
	return ret;
}

static ssize_t dump_trace_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	int i;
	char *pbuf = buf;
	ssize_t ret;
	u16 *trace_buf = kmalloc(debug_trace_num * 2, GFP_KERNEL);

	if (!trace_buf) {
		pbuf += sprintf(pbuf, "No Memory bug\n");
		ret = pbuf - buf;
		return ret;
	}
	if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_M8) {
		mutex_lock(&vdec_mutex);
		if (!vdec_on(VDEC_1)) {
			mutex_unlock(&vdec_mutex);
			kfree(trace_buf);
			pbuf += sprintf(pbuf, "amrisc is power off\n");
			ret = pbuf - buf;
			return ret;
		}
	} else if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_M6) {
		/*TODO:M6 define */
		/*
		 *  switch_mod_gate_by_type(MOD_VDEC, 1);
		 */
		amports_switch_gate("vdec", 1);
	}
	pr_info("dump trace steps:%d start\n", debug_trace_num);
	i = 0;
	while (i <= debug_trace_num - 16) {
		trace_buf[i] = READ_VREG(MPC_E);
		trace_buf[i + 1] = READ_VREG(MPC_E);
		trace_buf[i + 2] = READ_VREG(MPC_E);
		trace_buf[i + 3] = READ_VREG(MPC_E);
		trace_buf[i + 4] = READ_VREG(MPC_E);
		trace_buf[i + 5] = READ_VREG(MPC_E);
		trace_buf[i + 6] = READ_VREG(MPC_E);
		trace_buf[i + 7] = READ_VREG(MPC_E);
		trace_buf[i + 8] = READ_VREG(MPC_E);
		trace_buf[i + 9] = READ_VREG(MPC_E);
		trace_buf[i + 10] = READ_VREG(MPC_E);
		trace_buf[i + 11] = READ_VREG(MPC_E);
		trace_buf[i + 12] = READ_VREG(MPC_E);
		trace_buf[i + 13] = READ_VREG(MPC_E);
		trace_buf[i + 14] = READ_VREG(MPC_E);
		trace_buf[i + 15] = READ_VREG(MPC_E);
		i += 16;
	};
	pr_info("dump trace steps:%d finished\n", debug_trace_num);
	if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_M8)
		mutex_unlock(&vdec_mutex);
	else if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_M6) {
		/*TODO:M6 define */
		/*
		 *  switch_mod_gate_by_type(MOD_VDEC, 0);
		 */
		amports_switch_gate("vdec", 0);
	}
	for (i = 0; i < debug_trace_num; i++) {
		if (i % 4 == 0) {
			if (i % 16 == 0)
				pbuf += sprintf(pbuf, "\n");
			else if (i % 8 == 0)
				pbuf += sprintf(pbuf, "  ");
			else	/* 4 */
				pbuf += sprintf(pbuf, " ");
		}
		pbuf += sprintf(pbuf, "%04x:", trace_buf[i]);
	}
	while (i < debug_trace_num)
		;
	kfree(trace_buf);
	pbuf += sprintf(pbuf, "\n");
	ret = pbuf - buf;
	return ret;
}

static ssize_t clock_level_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	char *pbuf = buf;
	size_t ret;

	pbuf += sprintf(pbuf, "%dMHZ\n", vdec_clk_get(VDEC_1));

	if (has_vdec2())
		pbuf += sprintf(pbuf, "%dMHZ\n", vdec_clk_get(VDEC_2));

	if (has_hevc_vdec())
		pbuf += sprintf(pbuf, "%dMHZ\n", vdec_clk_get(VDEC_HEVC));

	ret = pbuf - buf;
	return ret;
}

static ssize_t store_poweron_clock_level(struct class *class,
		struct class_attribute *attr,
		const char *buf, size_t size)
{
	unsigned int val;
	ssize_t ret;

	/*ret = sscanf(buf, "%d", &val);*/
	ret = kstrtoint(buf, 0, &val);

	if (ret != 0)
		return -EINVAL;
	poweron_clock_level = val;
	return size;
}

static ssize_t show_poweron_clock_level(struct class *class,
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", poweron_clock_level);
}

/*
 *if keep_vdec_mem == 1
 *always don't release
 *vdec 64 memory for fast play.
 */
static ssize_t store_keep_vdec_mem(struct class *class,
		struct class_attribute *attr,
		const char *buf, size_t size)
{
	unsigned int val;
	ssize_t ret;

	/*ret = sscanf(buf, "%d", &val);*/
	ret = kstrtoint(buf, 0, &val);
	if (ret != 0)
		return -EINVAL;
	keep_vdec_mem = val;
	return size;
}

static ssize_t show_keep_vdec_mem(struct class *class,
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", keep_vdec_mem);
}

#ifdef VDEC_DEBUG_SUPPORT
static ssize_t store_debug(struct class *class,
		struct class_attribute *attr,
		const char *buf, size_t size)
{
	struct vdec_s *vdec;
	struct vdec_core_s *core = vdec_core;
	unsigned long flags;

	unsigned id;
	unsigned val;
	ssize_t ret;
	char cbuf[32];

	cbuf[0] = 0;
	ret = sscanf(buf, "%s %x %x", cbuf, &id, &val);
	/*pr_info(
	"%s(%s)=>ret %ld: %s, %x, %x\n",
	__func__, buf, ret, cbuf, id, val);*/
	if (strcmp(cbuf, "schedule") == 0) {
		pr_info("VDEC_DEBUG: force schedule\n");
		up(&core->sem);
	} else if (strcmp(cbuf, "power_off") == 0) {
		pr_info("VDEC_DEBUG: power off core %d\n", id);
		vdec_poweroff(id);
	} else if (strcmp(cbuf, "power_on") == 0) {
		pr_info("VDEC_DEBUG: power_on core %d\n", id);
		vdec_poweron(id);
	} else if (strcmp(cbuf, "wr") == 0) {
		pr_info("VDEC_DEBUG: WRITE_VREG(0x%x, 0x%x)\n",
			id, val);
		WRITE_VREG(id, val);
	} else if (strcmp(cbuf, "rd") == 0) {
		pr_info("VDEC_DEBUG: READ_VREG(0x%x) = 0x%x\n",
			id, READ_VREG(id));
	} else if (strcmp(cbuf, "read_hevc_clk_reg") == 0) {
		pr_info(
			"VDEC_DEBUG: HHI_VDEC4_CLK_CNTL = 0x%x, HHI_VDEC2_CLK_CNTL = 0x%x\n",
			READ_HHI_REG(HHI_VDEC4_CLK_CNTL),
			READ_HHI_REG(HHI_VDEC2_CLK_CNTL));
	}

	flags = vdec_core_lock(vdec_core);

	list_for_each_entry(vdec,
		&core->connected_vdec_list, list) {
		pr_info("vdec: status %d, id %d\n", vdec->status, vdec->id);
		if (((vdec->status == VDEC_STATUS_CONNECTED
			|| vdec->status == VDEC_STATUS_ACTIVE)) &&
			(vdec->id == id)) {
				/*to add*/
				break;
		}
	}
	vdec_core_unlock(vdec_core, flags);
	return size;
}

static ssize_t show_debug(struct class *class,
		struct class_attribute *attr, char *buf)
{
	char *pbuf = buf;
	struct vdec_s *vdec;
	struct vdec_core_s *core = vdec_core;
	unsigned long flags = vdec_core_lock(vdec_core);
	u64 tmp;

	pbuf += sprintf(pbuf,
		"============== help:\n");
	pbuf += sprintf(pbuf,
		"'echo xxx > debug' usuage:\n");
	pbuf += sprintf(pbuf,
		"schedule - trigger schedule thread to run\n");
	pbuf += sprintf(pbuf,
		"power_off core_num - call vdec_poweroff(core_num)\n");
	pbuf += sprintf(pbuf,
		"power_on core_num - call vdec_poweron(core_num)\n");
	pbuf += sprintf(pbuf,
		"wr adr val - call WRITE_VREG(adr, val)\n");
	pbuf += sprintf(pbuf,
		"rd adr - call READ_VREG(adr)\n");
	pbuf += sprintf(pbuf,
		"read_hevc_clk_reg - read HHI register for hevc clk\n");
	pbuf += sprintf(pbuf,
		"===================\n");

	pbuf += sprintf(pbuf,
		"name(core)\tschedule_count\trun_count\tinput_underrun\tdecbuf_not_ready\trun_time\n");
	list_for_each_entry(vdec,
		&core->connected_vdec_list, list) {
		enum vdec_type_e type;
		if ((vdec->status == VDEC_STATUS_CONNECTED
			|| vdec->status == VDEC_STATUS_ACTIVE)) {
		for (type = VDEC_1; type < VDEC_MAX; type++) {
			if (vdec->core_mask & (1 << type)) {
				pbuf += sprintf(pbuf, "%s(%d):",
					vdec->vf_provider_name, type);
				pbuf += sprintf(pbuf, "\t%d",
					vdec->check_count[type]);
				pbuf += sprintf(pbuf, "\t%d",
					vdec->run_count[type]);
				pbuf += sprintf(pbuf, "\t%d",
					vdec->input_underrun_count[type]);
				pbuf += sprintf(pbuf, "\t%d",
					vdec->not_run_ready_count[type]);
				tmp = vdec->run_clk[type] * 100;
				do_div(tmp, vdec->total_clk[type]);
				pbuf += sprintf(pbuf,
					"\t%d%%\n",
					vdec->total_clk[type] == 0 ? 0 :
					(u32)tmp);
			}
		}
	  }
	}

	vdec_core_unlock(vdec_core, flags);
	return pbuf - buf;

}
#endif

/*irq num as same as .dts*/
/*
 *	interrupts = <0 3 1
 *		0 23 1
 *		0 32 1
 *		0 43 1
 *		0 44 1
 *		0 45 1>;
 *	interrupt-names = "vsync",
 *		"demux",
 *		"parser",
 *		"mailbox_0",
 *		"mailbox_1",
 *		"mailbox_2";
 */
s32 vdec_request_threaded_irq(enum vdec_irq_num num,
			irq_handler_t handler,
			irq_handler_t thread_fn,
			unsigned long irqflags,
			const char *devname, void *dev)
{
	s32 res_irq;
	s32 ret = 0;

	if (num >= VDEC_IRQ_MAX) {
		pr_err("[%s] request irq error, irq num too big!", __func__);
		return -EINVAL;
	}

	if (vdec_core->isr_context[num].irq < 0) {
		res_irq = platform_get_irq(
			vdec_core->vdec_core_platform_device, num);
		if (res_irq < 0) {
			pr_err("[%s] get irq error!", __func__);
			return -EINVAL;
		}

		vdec_core->isr_context[num].irq = res_irq;
		vdec_core->isr_context[num].dev_isr = handler;
		vdec_core->isr_context[num].dev_threaded_isr = thread_fn;
		vdec_core->isr_context[num].dev_id = dev;

		ret = request_threaded_irq(res_irq,
			vdec_isr,
			vdec_thread_isr,
			(thread_fn) ? IRQF_ONESHOT : irqflags,
			devname,
			&vdec_core->isr_context[num]);

		if (ret) {
			vdec_core->isr_context[num].irq = -1;
			vdec_core->isr_context[num].dev_isr = NULL;
			vdec_core->isr_context[num].dev_threaded_isr = NULL;
			vdec_core->isr_context[num].dev_id = NULL;

			pr_err("vdec irq register error for %s.\n", devname);
			return -EIO;
		}
	} else {
		vdec_core->isr_context[num].dev_isr = handler;
		vdec_core->isr_context[num].dev_threaded_isr = thread_fn;
		vdec_core->isr_context[num].dev_id = dev;
	}

	return ret;
}
EXPORT_SYMBOL(vdec_request_threaded_irq);

s32 vdec_request_irq(enum vdec_irq_num num, irq_handler_t handler,
				const char *devname, void *dev)
{
	pr_debug("vdec_request_irq %p, %s\n", handler, devname);

	return vdec_request_threaded_irq(num,
		handler,
		NULL,/*no thread_fn*/
		IRQF_SHARED,
		devname,
		dev);
}
EXPORT_SYMBOL(vdec_request_irq);

void vdec_free_irq(enum vdec_irq_num num, void *dev)
{
	if (num >= VDEC_IRQ_MAX) {
		pr_err("[%s] request irq error, irq num too big!", __func__);
		return;
	}
	/*
	 *assume amrisc is stopped already and there is no mailbox interrupt
	 * when we reset pointers here.
	 */
	vdec_core->isr_context[num].dev_isr = NULL;
	vdec_core->isr_context[num].dev_threaded_isr = NULL;
	vdec_core->isr_context[num].dev_id = NULL;
	synchronize_irq(vdec_core->isr_context[num].irq);
}
EXPORT_SYMBOL(vdec_free_irq);

struct vdec_s *vdec_get_default_vdec_for_userdata(void)
{
	struct vdec_s *vdec;
	struct vdec_s *ret_vdec;
	struct vdec_core_s *core = vdec_core;
	unsigned long flags;
	int id;

	flags = vdec_core_lock(vdec_core);

	id = 0x10000000;
	ret_vdec = NULL;
	if (!list_empty(&core->connected_vdec_list)) {
		list_for_each_entry(vdec, &core->connected_vdec_list, list) {
			if (vdec->id < id) {
				id = vdec->id;
				ret_vdec = vdec;
			}
		}
	}

	vdec_core_unlock(vdec_core, flags);

	return ret_vdec;
}
EXPORT_SYMBOL(vdec_get_default_vdec_for_userdata);

struct vdec_s *vdec_get_vdec_by_id(int vdec_id)
{
	struct vdec_s *vdec;
	struct vdec_s *ret_vdec;
	struct vdec_core_s *core = vdec_core;
	unsigned long flags;

	flags = vdec_core_lock(vdec_core);

	ret_vdec = NULL;
	if (!list_empty(&core->connected_vdec_list)) {
		list_for_each_entry(vdec, &core->connected_vdec_list, list) {
			if (vdec->id == vdec_id) {
				ret_vdec = vdec;
				break;
			}
		}
	}

	vdec_core_unlock(vdec_core, flags);

	return ret_vdec;
}
EXPORT_SYMBOL(vdec_get_vdec_by_id);

int vdec_read_user_data(struct vdec_s *vdec,
			struct userdata_param_t *p_userdata_param)
{
	int ret = 0;

	if (!vdec)
		vdec = vdec_get_default_vdec_for_userdata();

	if (vdec) {
		if (vdec->user_data_read)
			ret = vdec->user_data_read(vdec, p_userdata_param);
	}
	return ret;
}
EXPORT_SYMBOL(vdec_read_user_data);

int vdec_wakeup_userdata_poll(struct vdec_s *vdec)
{
	if (vdec) {
		if (vdec->wakeup_userdata_poll)
			vdec->wakeup_userdata_poll(vdec);
	}

	return 0;
}
EXPORT_SYMBOL(vdec_wakeup_userdata_poll);

void vdec_reset_userdata_fifo(struct vdec_s *vdec, int bInit)
{
	if (!vdec)
		vdec = vdec_get_default_vdec_for_userdata();

	if (vdec) {
		if (vdec->reset_userdata_fifo)
			vdec->reset_userdata_fifo(vdec, bInit);
	}
}
EXPORT_SYMBOL(vdec_reset_userdata_fifo);

static int dump_mode;
static ssize_t dump_risc_mem_store(struct class *class,
		struct class_attribute *attr,
		const char *buf, size_t size)/*set*/
{
	unsigned int val;
	ssize_t ret;
	char dump_mode_str[4] = "PRL";

	/*ret = sscanf(buf, "%d", &val);*/
	ret = kstrtoint(buf, 0, &val);

	if (ret != 0)
		return -EINVAL;
	dump_mode = val & 0x3;
	pr_info("set dump mode to %d,%c_mem\n",
		dump_mode, dump_mode_str[dump_mode]);
	return size;
}
static u32 read_amrisc_reg(int reg)
{
	WRITE_VREG(0x31b, reg);
	return READ_VREG(0x31c);
}

static void dump_pmem(void)
{
	int i;

	WRITE_VREG(0x301, 0x8000);
	WRITE_VREG(0x31d, 0);
	pr_info("start dump amrisc pmem of risc\n");
	for (i = 0; i < 0xfff; i++) {
		/*same as .o format*/
		pr_info("%08x // 0x%04x:\n", read_amrisc_reg(i), i);
	}
}

static void dump_lmem(void)
{
	int i;

	WRITE_VREG(0x301, 0x8000);
	WRITE_VREG(0x31d, 2);
	pr_info("start dump amrisc lmem\n");
	for (i = 0; i < 0x3ff; i++) {
		/*same as */
		pr_info("[%04x] = 0x%08x:\n", i, read_amrisc_reg(i));
	}
}

static ssize_t dump_risc_mem_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	char *pbuf = buf;
	int ret;

	if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_M8) {
		mutex_lock(&vdec_mutex);
		if (!vdec_on(VDEC_1)) {
			mutex_unlock(&vdec_mutex);
			pbuf += sprintf(pbuf, "amrisc is power off\n");
			ret = pbuf - buf;
			return ret;
		}
	} else if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_M6) {
		/*TODO:M6 define */
		/*
		 *  switch_mod_gate_by_type(MOD_VDEC, 1);
		 */
		amports_switch_gate("vdec", 1);
	}
	/*start do**/
	switch (dump_mode) {
	case 0:
		dump_pmem();
		break;
	case 2:
		dump_lmem();
		break;
	default:
		break;
	}

	/*done*/
	if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_M8)
		mutex_unlock(&vdec_mutex);
	else if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_M6) {
		/*TODO:M6 define */
		/*
		 *  switch_mod_gate_by_type(MOD_VDEC, 0);
		 */
		amports_switch_gate("vdec", 0);
	}
	return sprintf(buf, "done\n");
}

static ssize_t core_show(struct class *class, struct class_attribute *attr,
			char *buf)
{
	struct vdec_core_s *core = vdec_core;
	char *pbuf = buf;

	if (list_empty(&core->connected_vdec_list))
		pbuf += sprintf(pbuf, "connected vdec list empty\n");
	else {
		struct vdec_s *vdec;

		pbuf += sprintf(pbuf,
			" Core: last_sched %p, sched_mask %lx\n",
			core->last_vdec,
			core->sched_mask);

		list_for_each_entry(vdec, &core->connected_vdec_list, list) {
			pbuf += sprintf(pbuf,
				"\tvdec.%d (%p (%s)), status = %s,\ttype = %s, \tactive_mask = %lx\n",
				vdec->id,
				vdec,
				vdec_device_name[vdec->format * 2],
				vdec_status_str(vdec),
				vdec_type_str(vdec),
				vdec->active_mask);
		}
	}

	return pbuf - buf;
}

static ssize_t vdec_status_show(struct class *class,
			struct class_attribute *attr, char *buf)
{
	char *pbuf = buf;
	struct vdec_s *vdec;
	struct vdec_info vs;
	unsigned char vdec_num = 0;
	struct vdec_core_s *core = vdec_core;
	unsigned long flags = vdec_core_lock(vdec_core);

	if (list_empty(&core->connected_vdec_list)) {
		pbuf += sprintf(pbuf, "No vdec.\n");
		goto out;
	}

	list_for_each_entry(vdec, &core->connected_vdec_list, list) {
		if ((vdec->status == VDEC_STATUS_CONNECTED
			|| vdec->status == VDEC_STATUS_ACTIVE)) {
			memset(&vs, 0, sizeof(vs));
			if (vdec_status(vdec, &vs)) {
				pbuf += sprintf(pbuf, "err.\n");
				goto out;
			}
			pbuf += sprintf(pbuf,
				"vdec channel %u statistics:\n",
				vdec_num);
			pbuf += sprintf(pbuf,
				"%13s : %s\n", "device name",
				vs.vdec_name);
			pbuf += sprintf(pbuf,
				"%13s : %u\n", "frame width",
				vs.frame_width);
			pbuf += sprintf(pbuf,
				"%13s : %u\n", "frame height",
				vs.frame_height);
			pbuf += sprintf(pbuf,
				"%13s : %u %s\n", "frame rate",
				vs.frame_rate, "fps");
			pbuf += sprintf(pbuf,
				"%13s : %u %s\n", "bit rate",
				vs.bit_rate / 1024 * 8, "kbps");
			pbuf += sprintf(pbuf,
				"%13s : %u\n", "status",
				vs.status);
			pbuf += sprintf(pbuf,
				"%13s : %u\n", "frame dur",
				vs.frame_dur);
			pbuf += sprintf(pbuf,
				"%13s : %u %s\n", "frame data",
				vs.frame_data / 1024, "KB");
			pbuf += sprintf(pbuf,
				"%13s : %u\n", "frame count",
				vs.frame_count);
			pbuf += sprintf(pbuf,
				"%13s : %u\n", "drop count",
				vs.drop_frame_count);
			pbuf += sprintf(pbuf,
				"%13s : %u\n", "fra err count",
				vs.error_frame_count);
			pbuf += sprintf(pbuf,
				"%13s : %u\n", "hw err count",
				vs.error_count);
			pbuf += sprintf(pbuf,
				"%13s : %llu %s\n\n", "total data",
				vs.total_data / 1024, "KB");

			vdec_num++;
		}
	}
out:
	vdec_core_unlock(vdec_core, flags);
	return pbuf - buf;
}

static ssize_t dump_vdec_blocks_show(struct class *class,
			struct class_attribute *attr, char *buf)
{
	struct vdec_core_s *core = vdec_core;
	char *pbuf = buf;

	if (list_empty(&core->connected_vdec_list))
		pbuf += sprintf(pbuf, "connected vdec list empty\n");
	else {
		struct vdec_s *vdec;
		list_for_each_entry(vdec, &core->connected_vdec_list, list) {
			pbuf += vdec_input_dump_blocks(&vdec->input,
				pbuf, PAGE_SIZE - (pbuf - buf));
		}
	}

	return pbuf - buf;
}
static ssize_t dump_vdec_chunks_show(struct class *class,
			struct class_attribute *attr, char *buf)
{
	struct vdec_core_s *core = vdec_core;
	char *pbuf = buf;

	if (list_empty(&core->connected_vdec_list))
		pbuf += sprintf(pbuf, "connected vdec list empty\n");
	else {
		struct vdec_s *vdec;
		list_for_each_entry(vdec, &core->connected_vdec_list, list) {
			pbuf += vdec_input_dump_chunks(&vdec->input,
				pbuf, PAGE_SIZE - (pbuf - buf));
		}
	}

	return pbuf - buf;
}

static ssize_t dump_decoder_state_show(struct class *class,
			struct class_attribute *attr, char *buf)
{
	char *pbuf = buf;
	struct vdec_s *vdec;
	struct vdec_core_s *core = vdec_core;
	unsigned long flags = vdec_core_lock(vdec_core);

	if (list_empty(&core->connected_vdec_list)) {
		pbuf += sprintf(pbuf, "No vdec.\n");
	} else {
		list_for_each_entry(vdec,
			&core->connected_vdec_list, list) {
			if ((vdec->status == VDEC_STATUS_CONNECTED
				|| vdec->status == VDEC_STATUS_ACTIVE)
				&& vdec->dump_state)
					vdec->dump_state(vdec);
		}
	}
	vdec_core_unlock(vdec_core, flags);

	return pbuf - buf;
}

static ssize_t dump_fps_show(struct class *class,
			struct class_attribute *attr, char *buf)
{
	char *pbuf = buf;
	struct vdec_core_s *core = vdec_core;
	int i;

	unsigned long flags = vdec_fps_lock(vdec_core);
	for (i = 0; i < MAX_INSTANCE_MUN; i++)
		pbuf += sprintf(pbuf, "%d ", core->decode_fps[i].fps);

	pbuf += sprintf(pbuf, "\n");
	vdec_fps_unlock(vdec_core, flags);

	return pbuf - buf;
}



static struct class_attribute vdec_class_attrs[] = {
	__ATTR_RO(amrisc_regs),
	__ATTR_RO(dump_trace),
	__ATTR_RO(clock_level),
	__ATTR(poweron_clock_level, S_IRUGO | S_IWUSR | S_IWGRP,
	show_poweron_clock_level, store_poweron_clock_level),
	__ATTR(dump_risc_mem, S_IRUGO | S_IWUSR | S_IWGRP,
	dump_risc_mem_show, dump_risc_mem_store),
	__ATTR(keep_vdec_mem, S_IRUGO | S_IWUSR | S_IWGRP,
	show_keep_vdec_mem, store_keep_vdec_mem),
	__ATTR_RO(core),
	__ATTR_RO(vdec_status),
	__ATTR_RO(dump_vdec_blocks),
	__ATTR_RO(dump_vdec_chunks),
	__ATTR_RO(dump_decoder_state),
#ifdef VDEC_DEBUG_SUPPORT
	__ATTR(debug, S_IRUGO | S_IWUSR | S_IWGRP,
	show_debug, store_debug),
#endif
#ifdef FRAME_CHECK
	__ATTR(dump_yuv, S_IRUGO | S_IWUSR | S_IWGRP,
	dump_yuv_show, dump_yuv_store),
	__ATTR(frame_check, S_IRUGO | S_IWUSR | S_IWGRP,
	frame_check_show, frame_check_store),
#endif
	__ATTR_RO(dump_fps),
	__ATTR_NULL
};

static struct class vdec_class = {
		.name = "vdec",
		.class_attrs = vdec_class_attrs,
	};

struct device *get_vdec_device(void)
{
	return &vdec_core->vdec_core_platform_device->dev;
}
EXPORT_SYMBOL(get_vdec_device);

static int vdec_probe(struct platform_device *pdev)
{
	s32 i, r;

	vdec_core = (struct vdec_core_s *)devm_kzalloc(&pdev->dev,
			sizeof(struct vdec_core_s), GFP_KERNEL);
	if (vdec_core == NULL) {
		pr_err("vdec core allocation failed.\n");
		return -ENOMEM;
	}

	atomic_set(&vdec_core->vdec_nr, 0);
	sema_init(&vdec_core->sem, 1);

	r = class_register(&vdec_class);
	if (r) {
		pr_info("vdec class create fail.\n");
		return r;
	}

	vdec_core->vdec_core_platform_device = pdev;

	platform_set_drvdata(pdev, vdec_core);

	for (i = 0; i < VDEC_IRQ_MAX; i++) {
		vdec_core->isr_context[i].index = i;
		vdec_core->isr_context[i].irq = -1;
	}

	r = vdec_request_threaded_irq(VDEC_IRQ_0, NULL, NULL,
		IRQF_ONESHOT, "vdec-0", NULL);
	if (r < 0) {
		pr_err("vdec interrupt request failed\n");
		return r;
	}

	r = vdec_request_threaded_irq(VDEC_IRQ_1, NULL, NULL,
		IRQF_ONESHOT, "vdec-1", NULL);
	if (r < 0) {
		pr_err("vdec interrupt request failed\n");
		return r;
	}
#if 0
	if (get_cpu_major_id() >= MESON_CPU_MAJOR_ID_G12A) {
		r = vdec_request_threaded_irq(VDEC_IRQ_HEVC_BACK, NULL, NULL,
			IRQF_ONESHOT, "vdec-hevc_back", NULL);
		if (r < 0) {
			pr_err("vdec interrupt request failed\n");
			return r;
		}
	}
#endif
	r = of_reserved_mem_device_init(&pdev->dev);
	if (r == 0)
		pr_info("vdec_probe done\n");

	vdec_core->cma_dev = &pdev->dev;

	if (get_cpu_major_id() < AM_MESON_CPU_MAJOR_ID_M8) {
		/* default to 250MHz */
		vdec_clock_hi_enable();
	}

	if (get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_GXBB) {
		/* set vdec dmc request to urgent */
		WRITE_DMCREG(DMC_AM5_CHAN_CTRL, 0x3f203cf);
	}
	INIT_LIST_HEAD(&vdec_core->connected_vdec_list);
	spin_lock_init(&vdec_core->lock);
	spin_lock_init(&vdec_core->canvas_lock);
	spin_lock_init(&vdec_core->fps_lock);
	ida_init(&vdec_core->ida);
	vdec_core->thread = kthread_run(vdec_core_thread, vdec_core,
					"vdec-core");

	vdec_core->vdec_core_wq = alloc_ordered_workqueue("%s",__WQ_LEGACY |
		WQ_MEM_RECLAIM |WQ_HIGHPRI/*high priority*/, "vdec-work");
	/*work queue priority lower than vdec-core.*/
	return 0;
}

static int vdec_remove(struct platform_device *pdev)
{
	int i;

	for (i = 0; i < VDEC_IRQ_MAX; i++) {
		if (vdec_core->isr_context[i].irq >= 0) {
			free_irq(vdec_core->isr_context[i].irq,
				&vdec_core->isr_context[i]);
			vdec_core->isr_context[i].irq = -1;
			vdec_core->isr_context[i].dev_isr = NULL;
			vdec_core->isr_context[i].dev_threaded_isr = NULL;
			vdec_core->isr_context[i].dev_id = NULL;
		}
	}

	kthread_stop(vdec_core->thread);

	destroy_workqueue(vdec_core->vdec_core_wq);
	class_unregister(&vdec_class);

	return 0;
}

static const struct of_device_id amlogic_vdec_dt_match[] = {
	{
		.compatible = "amlogic, vdec",
	},
	{},
};

static struct mconfig vdec_configs[] = {
	MC_PU32("debug_trace_num", &debug_trace_num),
	MC_PI32("hevc_max_reset_count", &hevc_max_reset_count),
	MC_PU32("clk_config", &clk_config),
	MC_PI32("step_mode", &step_mode),
	MC_PI32("poweron_clock_level", &poweron_clock_level),
};
static struct mconfig_node vdec_node;

static struct platform_driver vdec_driver = {
	.probe = vdec_probe,
	.remove = vdec_remove,
	.driver = {
		.name = "vdec",
		.of_match_table = amlogic_vdec_dt_match,
	}
};

static struct codec_profile_t amvdec_input_profile = {
	.name = "vdec_input",
	.profile = "drm_framemode"
};

int vdec_module_init(void)
{
	if (platform_driver_register(&vdec_driver)) {
		pr_info("failed to register vdec module\n");
		return -ENODEV;
	}
	INIT_REG_NODE_CONFIGS("media.decoder", &vdec_node,
		"vdec", vdec_configs, CONFIG_FOR_RW);
	vcodec_profile_register(&amvdec_input_profile);
	return 0;
}
EXPORT_SYMBOL(vdec_module_init);

void vdec_module_exit(void)
{
	platform_driver_unregister(&vdec_driver);
}
EXPORT_SYMBOL(vdec_module_exit);

#if 0
static int __init vdec_module_init(void)
{
	if (platform_driver_register(&vdec_driver)) {
		pr_info("failed to register vdec module\n");
		return -ENODEV;
	}
	INIT_REG_NODE_CONFIGS("media.decoder", &vdec_node,
		"vdec", vdec_configs, CONFIG_FOR_RW);
	return 0;
}

static void __exit vdec_module_exit(void)
{
	platform_driver_unregister(&vdec_driver);
}
#endif

static int vdec_mem_device_init(struct reserved_mem *rmem, struct device *dev)
{
	vdec_core->cma_dev = dev;

	return 0;
}

static const struct reserved_mem_ops rmem_vdec_ops = {
	.device_init = vdec_mem_device_init,
};

static int __init vdec_mem_setup(struct reserved_mem *rmem)
{
	rmem->ops = &rmem_vdec_ops;
	pr_info("vdec: reserved mem setup\n");

	return 0;
}

RESERVEDMEM_OF_DECLARE(vdec, "amlogic, vdec-memory", vdec_mem_setup);
/*
uint force_hevc_clock_cntl;
EXPORT_SYMBOL(force_hevc_clock_cntl);

module_param(force_hevc_clock_cntl, uint, 0664);
*/
module_param(debug, uint, 0664);
module_param(debug_trace_num, uint, 0664);
module_param(hevc_max_reset_count, int, 0664);
module_param(clk_config, uint, 0664);
module_param(step_mode, int, 0664);
module_param(debugflags, int, 0664);
module_param(parallel_decode, int, 0664);
module_param(fps_detection, int, 0664);
module_param(fps_clear, int, 0664);

/*
*module_init(vdec_module_init);
*module_exit(vdec_module_exit);
*/
#define CREATE_TRACE_POINTS
#include "vdec_trace.h"
MODULE_DESCRIPTION("AMLOGIC vdec driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tim Yao <timyao@amlogic.com>");
