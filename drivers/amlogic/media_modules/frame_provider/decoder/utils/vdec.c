/*
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Description:
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
#ifdef CONFIG_AMLOGIC_V4L_VIDEO3
#include <linux/amlogic/media/video_sink/v4lvideo_ext.h>
#endif
#include <linux/amlogic/media/vfm/vfm_ext.h>
#include <linux/sched/clock.h>
#include <uapi/linux/sched/types.h>
#include <linux/signal.h>
/*for VDEC_DEBUG_SUPPORT*/
#include <linux/time.h>
#include <linux/amlogic/media/utils/vdec_reg.h>
#include "../../../stream_input/amports/streambuf.h"
#include "vdec.h"
#include "vdec_trace.h"
#ifdef CONFIG_AMLOGIC_MEDIA_MULTI_DEC
#include "vdec_profile.h"
#endif
#include <linux/sched/clock.h>
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
#include <linux/amlogic/media/registers/cpu_version.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/amlogic/media/video_sink/video_keeper.h>
#include <linux/amlogic/media/codec_mm/configs.h>
#include <linux/amlogic/media/frame_sync/ptsserv.h>
#include "../../../common/chips/decoder_cpu_ver_info.h"
#include "frame_check.h"
#include <linux/amlogic/tee.h>
#include "vdec_canvas_utils.h"
#include "../../../amvdec_ports/aml_vcodec_drv.h"

#if 0
#define PXP_DEBUG
#endif

#ifdef CONFIG_AMLOGIC_POWER
#include <linux/amlogic/power_ctrl.h>
#endif

#ifdef CONFIG_AMLOGIC_IONVIDEO
#include <linux/amlogic/media/video_sink/ionvideo_ext.h>
#endif
//#include <dt-bindings/power/sc2-pd.h>
//#include <linux/amlogic/pwr_ctrl.h>
#include <linux/of_device.h>
#include "vdec_power_ctrl.h"

static DEFINE_MUTEX(vdec_mutex);

#define MC_SIZE (4096 * 4)
#define CMA_ALLOC_SIZE SZ_64M
#define MEM_NAME "vdec_prealloc"
static int inited_vcodec_num;
#define jiffies_ms div64_u64(get_jiffies_64() * 1000, HZ)
static int poweron_clock_level;
static int debug_vdetect = 0;
static int keep_vdec_mem;
static unsigned int debug_trace_num = 16 * 20;
static int step_mode;
static unsigned int clk_config;
/*
0x1 : enable rdma
0x2 : check rdma result
*/
static int rdma_mode = 0x1;

/*
 * 0x1  : sched_priority to MAX_RT_PRIO -1.
 * 0x2  : always reload firmware.
 * 0x4  : vdec canvas debug enable
 * 0x100: enable vdec fence.
 */
#define VDEC_DBG_SCHED_PRIO	(0x1)
#define VDEC_DBG_ALWAYS_LOAD_FW	(0x2)
#define VDEC_DBG_CANVAS_STATUS	(0x4)
#define VDEC_DBG_ENABLE_FENCE	(0x100)


#define HEVC_RDMA_F_CTRL                           0x30f0
#define HEVC_RDMA_F_START_ADDR                     0x30f1
#define HEVC_RDMA_F_END_ADDR                       0x30f2
#define HEVC_RDMA_F_STATUS0                        0x30f3

#define HEVC_RDMA_B_CTRL                           0x30f8
#define HEVC_RDMA_B_START_ADDR                     0x30f9
#define HEVC_RDMA_B_END_ADDR                       0x30fa
#define HEVC_RDMA_B_STATUS0                        0x30fb

#define FRAME_BASE_PATH_DI_V4LVIDEO_0 (29)
#define FRAME_BASE_PATH_DI_V4LVIDEO_1 (30)
#define FRAME_BASE_PATH_DI_V4LVIDEO_2 (31)

static u32 debug = VDEC_DBG_ALWAYS_LOAD_FW;

u32 vdec_get_debug(void)
{
    return debug;
}
EXPORT_SYMBOL(vdec_get_debug);


int hevc_max_reset_count;
EXPORT_SYMBOL(hevc_max_reset_count);

int no_powerdown;
EXPORT_SYMBOL(no_powerdown);
static int parallel_decode = 1;
static int fps_detection;
static int fps_clear;
static bool prog_only;

static int force_nosecure_even_drm;
static int disable_switch_single_to_mult;

static DEFINE_SPINLOCK(vdec_spin_lock);

#define HEVC_TEST_LIMIT 100
#define GXBB_REV_A_MINOR 0xA

#define PRINT_FRAME_INFO 1
#define DISABLE_FRAME_INFO 2

#define RESET7_REGISTER_LEVEL 0x1127
#define P_RESETCTRL_RESET5_LEVEL 0x15

#define str(a) #a
#define xstr(a) str(a)

static int frameinfo_flag = 0;
static int v4lvideo_add_di = 1;
static int v4lvideo_add_ppmgr = 0;
static int max_di_instance = 2;
static int max_supported_di_instance = 4;

//static int path_debug = 0;

static int enable_mvdec_info = 1;

int decode_underflow = 0;
u32 debug_meta;

static int enable_stream_mode_multi_dec;

st_userdata userdata;

typedef void (*vdec_frame_rate_event_func)(int);

#if 1
extern void vframe_rate_uevent(int duration);
vdec_frame_rate_event_func frame_rate_notify = vframe_rate_uevent;
#else
vdec_frame_rate_event_func frame_rate_notify = NULL;
#endif

void vdec_frame_rate_uevent(int dur)
{
	if (frame_rate_notify == NULL)
		return;

	if (unlikely(in_interrupt()))
		return;
	pr_info("vdec_frame_rate_uevent %d\n", dur);
	frame_rate_notify(dur);
}
EXPORT_SYMBOL(vdec_frame_rate_uevent);


void register_frame_rate_uevent_func(vdec_frame_rate_event_func func)
{
	frame_rate_notify = func;
}
EXPORT_SYMBOL(register_frame_rate_uevent_func);

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
	spinlock_t input_lock;
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
	unsigned long buff_flag;
	unsigned long stream_buff_flag;
	struct power_manager_s *pm;
	u32 vdec_resouce_status;
	struct post_task_mgr_s post;
	u32 inst_cnt;
};

static struct vdec_core_s *vdec_core;

static const char * const vdec_status_string[] = {
	"VDEC_STATUS_UNINITIALIZED",
	"VDEC_STATUS_DISCONNECTED",
	"VDEC_STATUS_CONNECTED",
	"VDEC_STATUS_ACTIVE"
};
/*
bit [29] enable steam mode dv multi;
bit [28] enable print
bit [23:16] etc
bit [15:12]
	none 0 and not 0x1: force single
	none 0 and 0x1: force multi
bit [8]
	1: force dual
bit [3]
	1: use mavs for single mode
bit [2]
	1: force vfm path for frame mode
bit [1]
	1: force esparser auto mode
bit [0]
	1: disable audo manual mode ??
*/

static int debugflags;

static char vfm_path[VDEC_MAP_NAME_SIZE] = {"disable"};
static const char vfm_path_node[][VDEC_MAP_NAME_SIZE] =
{
	"video_render.0",
	"video_render.1",
	"amvideo",
	"videopip",
	"deinterlace",
	"dimulti.1",
	"amlvideo",
	"aml_video.1",
	"amlvideo2.0",
	"amlvideo2.1",
	"ppmgr",
	"ionvideo",
	"ionvideo.1",
	"ionvideo.2",
	"ionvideo.3",
	"ionvideo.4",
	"ionvideo.5",
	"ionvideo.6",
	"ionvideo.7",
	"ionvideo.8",
	"videosync.0",
	"v4lvideo.0",
	"v4lvideo.1",
	"v4lvideo.2",
	"v4lvideo.3",
	"v4lvideo.4",
	"v4lvideo.5",
	"v4lvideo.6",
	"v4lvideo.7",
	"v4lvideo.8",
	"fake-amvideo",
	"disable",
	"reserved",
};

int vdec_get_debug_flags(void)
{
	return debugflags;
}
EXPORT_SYMBOL(vdec_get_debug_flags);

void VDEC_PRINT_FUN_LINENO(const char *fun, int line)
{
	if (debugflags & 0x10000000)
		pr_info("%s, %d\n", fun, line);
}
EXPORT_SYMBOL(VDEC_PRINT_FUN_LINENO);

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

bool is_support_no_parser(void)
{
	if ((enable_stream_mode_multi_dec) ||
		(get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_SC2) ||
		(get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_T7))
		return true;
	return false;
}
EXPORT_SYMBOL(is_support_no_parser);

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

static struct vdec_core_s *vdec_core;
static struct vdec_data_core_s vdec_data_core;

static void vdec_data_core_init(void)
{
	spin_lock_init(&vdec_data_core.vdec_data_lock);
}

int vdec_data_get_index(ulong data)
{
	struct vdec_data_info_s *vdata = (struct vdec_data_info_s *)data;
	int i = 0;

	for (i = 0; i < VDEC_DATA_NUM; i++) {
		if (atomic_read(&vdata->data[i].use_count) == 0) {
			return i;
		}
	}

	return -1;
}
EXPORT_SYMBOL(vdec_data_get_index);

void vdec_data_buffer_count_increase(ulong data, int index, int cb_index)
{
	struct vdec_data_info_s *vdata = (struct vdec_data_info_s *)data;

	if (atomic_read(&vdata->data[index].use_count) == 0)
		atomic_inc(&vdata->buffer_count);
	atomic_inc(&vdata->data[index].use_count);

	vdata->release_callback[cb_index].private_data = (void *)&vdata->data[index];
}
EXPORT_SYMBOL(vdec_data_buffer_count_increase);

struct vdec_data_info_s *vdec_data_get(void)
{
	int i, j;
	struct vdec_data_core_s *core = &vdec_data_core;
	ulong flags;

	spin_lock_irqsave(&core->vdec_data_lock, flags);
	for (i = 0; i < VDEC_DATA_MAX_INSTANCE_NUM; i++) {
		if (atomic_read(&core->vdata[i].use_flag) == 0) {
			atomic_set(&core->vdata[i].use_flag, 1);
			for (j = 0; j < VDEC_DATA_NUM; j++) {
				core->vdata[i].release_callback[j].func = vdec_data_release;
				core->vdata[i].data[j].private_data = (void *)&core->vdata[i];
			}
			spin_unlock_irqrestore(&core->vdec_data_lock, flags);
			pr_debug("%s:get %dth vdata %p ok\n", __func__, i, &core->vdata[i]);
			return &core->vdata[i];
		}
	}
	spin_unlock_irqrestore(&core->vdec_data_lock, flags);
	return 0;
}
EXPORT_SYMBOL(vdec_data_get);

void vdec_data_release(struct codec_mm_s *mm, struct codec_mm_cb_s *cb)
{
	struct vdec_data_s *data = (struct vdec_data_s *)cb->private_data;
	struct vdec_data_info_s *vdata = (struct vdec_data_info_s *)data->private_data;

	atomic_dec(&data->use_count);
	if (atomic_read(&data->use_count) == 0) {
		if (data->user_data_buf != NULL)
			vfree(data->user_data_buf);
		data->user_data_buf = NULL;
		atomic_dec(&vdata->buffer_count);
	}

	if (atomic_read(&vdata->buffer_count) == 0) {
		atomic_set(&vdata->use_flag, 0);
		pr_debug("%s:release vdata %p\n", __func__, vdata);
	}

	return ;
}
EXPORT_SYMBOL(vdec_data_release);

unsigned long vdec_canvas_lock(void)
{
	unsigned long flags;
	spin_lock_irqsave(&vdec_core->canvas_lock, flags);

	return flags;
}

void vdec_canvas_unlock(unsigned long flags)
{
	spin_unlock_irqrestore(&vdec_core->canvas_lock, flags);
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

unsigned long vdec_inputbuff_lock(struct vdec_core_s *core)
{
	unsigned long flags;

	spin_lock_irqsave(&core->input_lock, flags);

	return flags;
}

void vdec_inputbuff_unlock(struct vdec_core_s *core, unsigned long flags)
{
	spin_unlock_irqrestore(&core->input_lock, flags);
}


static bool vdec_is_input_frame_empty(struct vdec_s *vdec) {
	struct vdec_core_s *core = vdec_core;
	bool ret;
	unsigned long flags;

	flags = vdec_inputbuff_lock(core);
	ret = !(vdec->core_mask & core->buff_flag);
	vdec_inputbuff_unlock(core, flags);

	return ret;
}

static void vdec_up(struct vdec_s *vdec)
{
	struct vdec_core_s *core = vdec_core;

	if (debug & 8)
		pr_info("vdec_up, id:%d\n", vdec->id);
	up(&core->sem);
}

static u64 vdec_get_us_time_system(void)
{
	return div64_u64(local_clock(), 1000);
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

static void vdec_dmc_pipeline_reset(void)
{
	/*
	 * bit15: vdec_piple
	 * bit14: hevc_dmc_piple
	 * bit13: hevcf_dmc_pipl
	 * bit12: wave420_dmc_pipl
	 * bit11: hcodec_dmc_pipl
	 */

	WRITE_RESET_REG(RESET7_REGISTER,
		(1 << 15) | (1 << 14) | (1 << 13) |
		(1 << 12) | (1 << 11));
}

static void vdec_stop_armrisc(int hw)
{
	ulong timeout = jiffies + HZ;

	if (hw == VDEC_INPUT_TARGET_VLD) {
		WRITE_VREG(MPSR, 0);
		WRITE_VREG(CPSR, 0);

		while (READ_VREG(IMEM_DMA_CTRL) & 0x8000) {
			if (time_after(jiffies, timeout))
				break;
		}

		timeout = jiffies + HZ;
		while (READ_VREG(LMEM_DMA_CTRL) & 0x8000) {
			if (time_after(jiffies, timeout))
				break;
		}
	} else if (hw == VDEC_INPUT_TARGET_HEVC) {
		WRITE_VREG(HEVC_MPSR, 0);
		WRITE_VREG(HEVC_CPSR, 0);

		while (READ_VREG(HEVC_IMEM_DMA_CTRL) & 0x8000) {
			if (time_after(jiffies, timeout))
				break;
		}

		timeout = jiffies + HZ/10;
		while (READ_VREG(HEVC_LMEM_DMA_CTRL) & 0x8000) {
			if (time_after(jiffies, timeout))
				break;
		}
	}
}

#define VDEC_ASSIST_DBUS_DISABLE		0x0046
#define HEVC_ASSIST_AXI_STATUS2_LO		0x307f

static void vdec_dbus_ctrl(bool enable)
{
	if (enable) {
		WRITE_VREG(VDEC_ASSIST_DBUS_DISABLE, 0);
	} else {
		u32 nop_cnt = 200;
		WRITE_VREG(VDEC_ASSIST_DBUS_DISABLE, 0xffff);
		while (READ_VREG(VDEC_ASSIST_DBUS_DISABLE) != 0xffff);
		while (nop_cnt--);
	}
}

static void hevc_arb_ctrl(bool enable)
{
	u32 axi_ctrl, axi_status, nop_cnt = 200;

	if (enable) {
		axi_ctrl = READ_VREG(HEVC_ASSIST_AXI_CTRL);
		axi_ctrl &= (~((1 << 6) | (1 << 14)));
		WRITE_VREG(HEVC_ASSIST_AXI_CTRL, axi_ctrl);		//enable front/back arbitor
	} else {
		axi_ctrl = READ_VREG(HEVC_ASSIST_AXI_CTRL);
		axi_ctrl |= (1 << 6);
		WRITE_VREG(HEVC_ASSIST_AXI_CTRL, axi_ctrl);	 // disable front arbitor

		do {
			axi_status = READ_VREG(HEVC_ASSIST_AXI_STATUS);
			if (axi_status & ((1 << 15) | (1 << 11)))		//read/write disable
				break;
		} while (1);

		axi_ctrl |= (1 << 14);
		WRITE_VREG(HEVC_ASSIST_AXI_CTRL, axi_ctrl);	 // disable back arbitor

		do {
			axi_status = READ_VREG(HEVC_ASSIST_AXI_STATUS2_LO);
			if (axi_status & ((1 << 15) | (1 << 11)))		//read/write disable
				break;
		} while (1);

		while (nop_cnt--);
	}
}

static void dec_dmc_port_ctrl(bool dmc_on, u32 target)
{
	unsigned long flags;
	unsigned int sts_reg_addr = DMC_CHAN_STS;
	unsigned int mask = 0;
	unsigned int cpu_type = get_cpu_major_id();

	if (target == VDEC_INPUT_TARGET_VLD) {
		mask = (1 << 13);	/*bit13: DOS VDEC interface*/
		if (cpu_type >= AM_MESON_CPU_MAJOR_ID_G12A)
			mask = (1 << 21);
	} else if (target == VDEC_INPUT_TARGET_HEVC) {
		mask = (1 << 4); /*hevc*/
		if (cpu_type >= AM_MESON_CPU_MAJOR_ID_G12A)
			mask |= (1 << 8); /*hevcb */
	}

	if (!mask) {
		pr_info("debug dmc ctrl return\n");
		return;
	}

	if (dmc_on) {
		/* dmc async on requset */
		spin_lock_irqsave(&vdec_spin_lock, flags);

		codec_dmcbus_write(DMC_REQ_CTRL,
			codec_dmcbus_read(DMC_REQ_CTRL) | mask);

		spin_unlock_irqrestore(&vdec_spin_lock, flags);
	} else {
		/* dmc async off requset */
		spin_lock_irqsave(&vdec_spin_lock, flags);

		codec_dmcbus_write(DMC_REQ_CTRL,
			codec_dmcbus_read(DMC_REQ_CTRL) & ~mask);

		spin_unlock_irqrestore(&vdec_spin_lock, flags);

		switch (cpu_type) {
		case AM_MESON_CPU_MAJOR_ID_S4:
		case AM_MESON_CPU_MAJOR_ID_S4D:
		case AM_MESON_CPU_MAJOR_ID_T5W:
			sts_reg_addr = S4_DMC_CHAN_STS;
			break;
		case AM_MESON_CPU_MAJOR_ID_T5:
		case AM_MESON_CPU_MAJOR_ID_T5D:
			sts_reg_addr = T5_DMC_CHAN_STS;
			break;
		case AM_MESON_CPU_MAJOR_ID_SC2:
			sts_reg_addr = TM2_REVB_DMC_CHAN_STS;
			break;
		case AM_MESON_CPU_MAJOR_ID_TM2:
			if (is_cpu_meson_revb())
				sts_reg_addr = TM2_REVB_DMC_CHAN_STS;
			else
				sts_reg_addr = DMC_CHAN_STS;
			break;
		default:
			sts_reg_addr = DMC_CHAN_STS;
			break;
		}
		while (!(codec_dmcbus_read(sts_reg_addr)
				& mask))
				;
	}
}

static void vdec_disable_DMC(struct vdec_s *vdec)
{
	/*close first,then wait pedding end,timing suggestion from vlsi*/
	struct vdec_input_s *input = &vdec->input;

	/* need to stop armrisc. */
	if (!IS_ERR_OR_NULL(vdec->dev))
		vdec_stop_armrisc(input->target);

	if ((get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_T7) ||
		(get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_T3)) {
		if (input->target == VDEC_INPUT_TARGET_VLD) {
			if (!vdec_on(VDEC_1))
				return;
			vdec_dbus_ctrl(0);
		} else if (input->target == VDEC_INPUT_TARGET_HEVC) {
			if (!vdec_on(VDEC_HEVC))
				return;
			hevc_arb_ctrl(0);
		}
	} else
		dec_dmc_port_ctrl(0, input->target);

	pr_debug("%s input->target= 0x%x\n", __func__,  input->target);
}

static void vdec_enable_DMC(struct vdec_s *vdec)
{
	struct vdec_input_s *input = &vdec->input;

	if ((get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_T7) ||
		(get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_T3)) {
		if (input->target == VDEC_INPUT_TARGET_VLD)
			vdec_dbus_ctrl(1);
		else if (input->target == VDEC_INPUT_TARGET_HEVC)
			hevc_arb_ctrl(1);
		return;
	}

	/*must to be reset the dmc pipeline if it's g12b.*/
	if (get_cpu_type() == AM_MESON_CPU_MAJOR_ID_G12B)
		vdec_dmc_pipeline_reset();

	dec_dmc_port_ctrl(1, input->target);

	pr_debug("%s input->target= 0x%x\n", __func__,  input->target);
}



static int vdec_get_hw_type(int value)
{
	int type;
	switch (value) {
		case VFORMAT_HEVC:
		case VFORMAT_VP9:
		case VFORMAT_AVS2:
		case VFORMAT_AV1:
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

static void vdec_update_buff_status(void)
{
	struct vdec_core_s *core = vdec_core;
	unsigned long flags;
	struct vdec_s *vdec;

	flags = vdec_inputbuff_lock(core);
	core->buff_flag = 0;
	core->stream_buff_flag = 0;
	list_for_each_entry(vdec, &core->connected_vdec_list, list) {
		struct vdec_input_s *input = &vdec->input;
		if (input_frame_based(input)) {
			if (input->have_frame_num || input->eos)
				core->buff_flag |= vdec->core_mask;
		} else if (input_stream_based(input)) {
			core->stream_buff_flag |= vdec->core_mask;
		}
		/* slave el pre_decode_level wp update */
		if ((is_support_no_parser()) && (vdec->slave)) {
			STBUF_WRITE(&vdec->slave->vbuf, set_wp,
				STBUF_READ(&vdec->vbuf, get_wp));
		}
	}
	vdec_inputbuff_unlock(core, flags);
}

#if 0
void vdec_update_streambuff_status(void)
{
	struct vdec_core_s *core = vdec_core;
	struct vdec_s *vdec;

	/* check streaming prepare level threshold if not EOS */
	list_for_each_entry(vdec, &core->connected_vdec_list, list) {
		struct vdec_input_s *input = &vdec->input;
		if (input && input_stream_based(input) && !input->eos &&
			(vdec->need_more_data & VDEC_NEED_MORE_DATA)) {
			u32 rp, wp, level;

			rp = STBUF_READ(&vdec->vbuf, get_rp);
			wp = STBUF_READ(&vdec->vbuf, get_wp);
			if (wp < rp)
				level = input->size + wp - rp;
			else
				level = wp - rp;
			if ((level < input->prepare_level) &&
				(pts_get_rec_num(PTS_TYPE_VIDEO,
					vdec->input.total_rd_count) < 2)) {
				break;
			} else if (level > input->prepare_level) {
				vdec->need_more_data &= ~VDEC_NEED_MORE_DATA;
				if (debug & 8)
					pr_info("vdec_flush_streambuff_status up\n");
				vdec_up(vdec);
			}
			break;
		}
	}
}
EXPORT_SYMBOL(vdec_update_streambuff_status);
#endif

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
	}
		vs->frame_count++;
	/*pr_info("size : %u, offset : %u, dur : %u, cnt : %u\n",
		vs->offset,offset,vs->frame_dur,vs->samp_cnt);*/
	return;
}
EXPORT_SYMBOL(vdec_count_info);
int vdec_is_support_4k(void)
{
	return ((!is_meson_gxl_package_805X()) &&
			(!is_cpu_s4_s805x2()) &&
			(get_cpu_major_id() != AM_MESON_CPU_MAJOR_ID_T5D));
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

struct device *get_codec_cma_device(void)
{
	return vdec_core->cma_dev;
}

int vdec_get_core_nr(void)
{
	return (int)atomic_read(&vdec_core->vdec_nr);
}
EXPORT_SYMBOL(vdec_get_core_nr);

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
	"amvdec_avs2",       "ammvdec_avs2",
	"amvdec_av1",        "ammvdec_av1",
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
	"amvdec_avs2",
	"amvdec_av1"
};

#endif

/*
 * Only support time sliced decoding for frame based input,
 * so legacy decoder can exist with time sliced decoder.
 */
static const char *get_dev_name(bool use_legacy_vdec, int format)
{
#ifdef CONFIG_AMLOGIC_MEDIA_MULTI_DEC
	if (use_legacy_vdec && (debugflags & 0x8) == 0)
		return vdec_device_name[format * 2];
	else
		return vdec_device_name[format * 2 + 1];
#else
	return vdec_device_name[format];
#endif
}

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

	/* force switch to mult instance if supports this profile. */
	if ((vdec->type == VDEC_TYPE_SINGLE) &&
		!disable_switch_single_to_mult) {
		const char *str = NULL;
		char fmt[16] = {0};

		str = strchr(get_dev_name(false, vdec->format), '_');
		if (!str)
			return -1;

		sprintf(fmt, "m%s", ++str);
		if (is_support_profile(fmt) &&
			vdec->sys_info->format != VIDEO_DEC_FORMAT_H263 &&
			vdec->format != VFORMAT_AV1)
			vdec->type = VDEC_TYPE_STREAM_PARSER;
	}

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

		init_waitqueue_head(&vdec->idle_wait);

		atomic_inc(&vdec_core->vdec_nr);
#ifdef CONFIG_AMLOGIC_V4L_VIDEO3
		v4lvideo_dec_count_increase();
#endif
		vdec->id = id;
		vdec->video_id = 0xffffffff;
		vdec_input_init(&vdec->input, vdec);
		vdec->input.vdec_is_input_frame_empty = vdec_is_input_frame_empty;
		vdec->input.vdec_up = vdec_up;
		if (master) {
			vdec->master = master;
			master->slave = vdec;
			master->sched = 1;
		}
		if (enable_mvdec_info) {
			vdec->mvfrm = (struct vdec_frames_s *)
				vzalloc(sizeof(struct vdec_frames_s));
			if (!vdec->mvfrm)
				pr_err("vzalloc: vdec_frames_s failed\n");
		}
	}

	pr_debug("vdec_create instance %p, total %d, PM: %s\n", vdec,
		atomic_read(&vdec_core->vdec_nr),
		get_pm_name(vdec_core->pm->pm_type));

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

void vdec_set_metadata(struct vdec_s *vdec, ulong meta_ptr)
{
	char *tmp_buf = NULL;
	u32 size = 0;

	if (!meta_ptr)
		return;

	tmp_buf = vmalloc(META_DATA_SIZE + 4);
	if (!tmp_buf) {
		pr_err("%s:vmalloc 256+4 fail\n", __func__);
		return;
	}
	memcpy(tmp_buf, (void *)meta_ptr, META_DATA_SIZE + 4);

	size = tmp_buf[0] + (tmp_buf[1] << 8) +
		(tmp_buf[2] << 16) + (tmp_buf[3] << 24);

	if ((size > 0) && (size <= META_DATA_SIZE)) {
		memcpy(vdec->hdr10p_data_buf, tmp_buf + 4, size);
		vdec->hdr10p_data_size = size;
		vdec->hdr10p_data_valid = true;
	}

	vfree(tmp_buf);
}
EXPORT_SYMBOL(vdec_set_metadata);

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

int vdec_get_frame_num(struct vdec_s *vdec)
{
	return vdec->input.have_frame_num;
}
EXPORT_SYMBOL(vdec_get_frame_num);

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

int vdec_write_vframe_with_dma(struct vdec_s *vdec,
	ulong addr, size_t count, u32 handle, chunk_free free, void* priv)
{
	return vdec_input_add_frame_with_dma(&vdec->input,
		addr, count, handle, free, priv);
}
EXPORT_SYMBOL(vdec_write_vframe_with_dma);

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
					STBUF_WRITE(&vdec->vbuf, set_rp,
						vdec_get_associate(vdec)->input.swap_rp);
					return;
				}
			}

			STBUF_WRITE(&vdec->vbuf, set_rp,
				READ_VREG(VLD_MEM_VIFIFO_RP));
		} else if (vdec->input.target == VDEC_INPUT_TARGET_HEVC) {
			me = READ_VREG(HEVC_SHIFT_BYTE_COUNT);
			if (((me & 0x80000000) == 0) &&
				(vdec->input.streaming_rp & 0x80000000))
				me += 1ULL << 32;
			other = vdec_get_associate(vdec)->input.streaming_rp;
			if (me > other) {
				STBUF_WRITE(&vdec->vbuf, set_rp,
					vdec_get_associate(vdec)->input.swap_rp);
				return;
			}

			STBUF_WRITE(&vdec->vbuf, set_rp,
				READ_VREG(HEVC_STREAM_RD_PTR));
		}
	} else if (vdec->input.target == VDEC_INPUT_TARGET_VLD) {
		STBUF_WRITE(&vdec->vbuf, set_rp,
			READ_VREG(VLD_MEM_VIFIFO_RP));
	} else if (vdec->input.target == VDEC_INPUT_TARGET_HEVC) {
		STBUF_WRITE(&vdec->vbuf, set_rp,
			READ_VREG(HEVC_STREAM_RD_PTR));
	}
}

static void vdec_sync_input_write(struct vdec_s *vdec)
{
	if (!vdec_stream_based(vdec))
		return;

	if (vdec->input.target == VDEC_INPUT_TARGET_VLD) {
		if (is_support_no_parser()) {
			if (!vdec->master) {
				WRITE_VREG(VLD_MEM_VIFIFO_WP,
					STBUF_READ(&vdec->vbuf, get_wp));
			} else {
				STBUF_WRITE(&vdec->vbuf, set_wp,
					STBUF_READ(&vdec->master->vbuf, get_wp));
			}
		} else {
			WRITE_VREG(VLD_MEM_VIFIFO_WP,
				STBUF_READ(&vdec->vbuf, get_wp));
		}
	} else if (vdec->input.target == VDEC_INPUT_TARGET_HEVC) {
		if (is_support_no_parser()) {
			if (!vdec->master) {
				WRITE_VREG(HEVC_STREAM_WR_PTR,
					STBUF_READ(&vdec->vbuf, get_wp));
			} else {
				STBUF_WRITE(&vdec->vbuf, set_wp,
					STBUF_READ(&vdec->master->vbuf, get_wp));
			}
		} else {
			WRITE_VREG(HEVC_STREAM_WR_PTR,
				STBUF_READ(&vdec->vbuf, get_wp));
		}
	}
}

void vdec_stream_skip_data(struct vdec_s *vdec, int skip_size)
{
	u32 rp_set;
	struct vdec_input_s *input = &vdec->input;
	u32 rp = 0, wp = 0, level;

	rp = STBUF_READ(&vdec->vbuf, get_rp);
	wp = STBUF_READ(&vdec->vbuf, get_wp);

	if (wp > rp)
		level = wp - rp;
	else
		level = wp + vdec->input.size - rp ;

	if (level <= skip_size) {
		pr_err("skip size is error, buffer level = 0x%x, skip size = 0x%x\n", level, skip_size);
		return;
	}

	if (wp >= rp) {
		rp_set = rp + skip_size;
	}
	else if ((rp + skip_size) < (input->start + input->size)) {
		rp_set = rp + skip_size;
	} else {
		rp_set = rp + skip_size - input->size;
		input->stream_cookie++;
	}

	if (vdec->format == VFORMAT_H264)
		SET_VREG_MASK(POWER_CTL_VLD,
			(1 << 9));

	WRITE_VREG(VLD_MEM_VIFIFO_CONTROL, 0);

	/* restore read side */
	WRITE_VREG(VLD_MEM_SWAP_ADDR,
		input->swap_page_phys);
	WRITE_VREG(VLD_MEM_SWAP_CTL, 1);

	while (READ_VREG(VLD_MEM_SWAP_CTL) & (1<<7))
		;
	WRITE_VREG(VLD_MEM_SWAP_CTL, 0);

	WRITE_VREG(VLD_MEM_VIFIFO_CURR_PTR,
		rp_set);
		WRITE_VREG(VLD_MEM_VIFIFO_CONTROL, 1);
		WRITE_VREG(VLD_MEM_VIFIFO_CONTROL, 0);
	STBUF_WRITE(&vdec->vbuf, set_rp,
		rp_set);
	WRITE_VREG(VLD_MEM_SWAP_ADDR,
		input->swap_page_phys);
	WRITE_VREG(VLD_MEM_SWAP_CTL, 3);
	while (READ_VREG(VLD_MEM_SWAP_CTL) & (1<<7))
		;
	WRITE_VREG(VLD_MEM_SWAP_CTL, 0);
}
EXPORT_SYMBOL(vdec_stream_skip_data);



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
		if (get_cpu_major_id() < AM_MESON_CPU_MAJOR_ID_SC2)
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
		u32 rp = 0, wp = 0, fifo_len = 0, first_set_rp = 0;
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

				if (vdec->vbuf.no_parser)
					SET_VREG_MASK(VLD_MEM_VIFIFO_CONTROL,
						7 << 3);

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

				if (vdec->vbuf.no_parser)
					SET_VREG_MASK(HEVC_STREAM_CONTROL,
						7 << 4);
				/*pr_info("vdec: restore context\r\n");*/
			}

		} else {
			if (vdec->vbuf.ext_buf_addr)
				first_set_rp = STBUF_READ(&vdec->vbuf, get_rp);
			else {
				if (vdec->discard_start_data_flag)
					first_set_rp = STBUF_READ(&vdec->vbuf, get_rp);
				else
					first_set_rp = input->start;
			}
			if (input->target == VDEC_INPUT_TARGET_VLD) {
				WRITE_VREG(VLD_MEM_VIFIFO_START_PTR,
					input->start);
				WRITE_VREG(VLD_MEM_VIFIFO_END_PTR,
					input->start + input->size - 8);
				WRITE_VREG(VLD_MEM_VIFIFO_CURR_PTR,
					first_set_rp);

				WRITE_VREG(VLD_MEM_VIFIFO_CONTROL, 1);
				WRITE_VREG(VLD_MEM_VIFIFO_CONTROL, 0);

				/* set to manual mode */
				WRITE_VREG(VLD_MEM_VIFIFO_BUF_CNTL, 2);
				WRITE_VREG(VLD_MEM_VIFIFO_RP, first_set_rp);
				WRITE_VREG(VLD_MEM_VIFIFO_WP,
					STBUF_READ(&vdec->vbuf, get_wp));
				rp = READ_VREG(VLD_MEM_VIFIFO_RP);

				/* enable */
				WRITE_VREG(VLD_MEM_VIFIFO_CONTROL,
					(0x11 << 16) | (1<<10));
				if (vdec->vbuf.no_parser)
					SET_VREG_MASK(VLD_MEM_VIFIFO_CONTROL,
						7 << 3);

				wp = READ_VREG(VLD_MEM_VIFIFO_WP);

			} else if (input->target == VDEC_INPUT_TARGET_HEVC) {
				WRITE_VREG(HEVC_STREAM_START_ADDR,
					input->start);
				WRITE_VREG(HEVC_STREAM_END_ADDR,
					input->start + input->size);
				WRITE_VREG(HEVC_STREAM_RD_PTR,
					first_set_rp);
				WRITE_VREG(HEVC_STREAM_WR_PTR,
					STBUF_READ(&vdec->vbuf, get_wp));
				rp = READ_VREG(HEVC_STREAM_RD_PTR);
				wp = READ_VREG(HEVC_STREAM_WR_PTR);
				fifo_len = (READ_VREG(HEVC_STREAM_FIFO_CTL)
						>> 16) & 0x7f;
				if (vdec->vbuf.no_parser)
					SET_VREG_MASK(HEVC_STREAM_CONTROL,
						7 << 4);
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

u32 vdec_offset_prepare_input(struct vdec_s *vdec, u32 consume_byte, u32 data_offset, u32 data_size)
{
	struct vdec_input_s *input = &vdec->input;
	u32 res_byte, header_offset, header_data_size, data_invalid;

	res_byte = data_size - consume_byte;
	header_offset = data_offset;
	header_data_size = data_size;
	data_offset += consume_byte;
	data_size = res_byte;

	if (input->target == VDEC_INPUT_TARGET_VLD) {
		//to do
	} else if (input->target == VDEC_INPUT_TARGET_HEVC) {
		data_invalid = data_offset - round_down(data_offset, 0x40);
		data_offset -= data_invalid;
		data_size += data_invalid;

		if (data_offset < header_offset) {
			data_invalid = consume_byte;
			data_offset = header_offset;
			data_size = header_data_size;
		}

		if (input_frame_based(input)) {
			struct vframe_chunk_s *chunk = vdec_input_next_chunk(&vdec->input);
			struct vframe_block_list_s *block = NULL;
			int dummy;

			block = chunk->block;
			WRITE_VREG(HEVC_STREAM_START_ADDR, block->start);
			WRITE_VREG(HEVC_STREAM_END_ADDR, block->start +
					block->size);
			WRITE_VREG(HEVC_STREAM_RD_PTR, block->start +
					data_offset);
			dummy = data_offset + data_size +
					HEVC_PADDING_SIZE;
			if (dummy >= block->size)
				dummy -= block->size;
			WRITE_VREG(HEVC_STREAM_WR_PTR,
				round_down(block->start + dummy,
					VDEC_FIFO_ALIGN));

			/* set endian */
			SET_VREG_MASK(HEVC_STREAM_CONTROL, 7 << 4);
		}
	}
	return data_invalid;
}
EXPORT_SYMBOL(vdec_offset_prepare_input);

void vdec_enable_input(struct vdec_s *vdec)
{
	struct vdec_input_s *input = &vdec->input;

	if (vdec->status != VDEC_STATUS_ACTIVE)
		return;

	if (input->target == VDEC_INPUT_TARGET_VLD)
		SET_VREG_MASK(VLD_MEM_VIFIFO_CONTROL, (1<<2) | (1<<1));
	else if (input->target == VDEC_INPUT_TARGET_HEVC) {
		SET_VREG_MASK(HEVC_STREAM_CONTROL, 1);
		if (vdec_stream_based(vdec)) {
			if (vdec->vbuf.no_parser)
				/*set endian for non-parser mode. */
				SET_VREG_MASK(HEVC_STREAM_CONTROL, 7 << 4);
			else
				CLEAR_VREG_MASK(HEVC_STREAM_CONTROL, 7 << 4);
		} else
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
				STBUF_READ(&vdec->vbuf, get_wp);
		else {
			return (READ_VREG(HEVC_STREAM_WR_PTR) & ~0x3) !=
				(STBUF_READ(&vdec->vbuf, get_wp) & ~0x3);
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
	struct vdec_core_s *core = vdec_core;

	vdec->input.eos = eos;

	if (vdec->slave)
		vdec->slave->input.eos = eos;
	up(&core->sem);
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


static void hevc_wait_ddr(void)
{
	if ((get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_T7) ||
		(get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_T3)) {
		hevc_arb_ctrl(0);
	} else {
		dec_dmc_port_ctrl(0, VDEC_INPUT_TARGET_HEVC);
	}
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
			hevc_wait_ddr();
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


static int vdec_input_read_restore(struct vdec_s *vdec)
{
	struct vdec_input_s *input = &vdec->input;

	if (!vdec_stream_based(vdec))
		return 0;

	if (!input->swap_valid) {
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
		} else if (input->target == VDEC_INPUT_TARGET_HEVC) {
				WRITE_VREG(HEVC_STREAM_START_ADDR,
					input->start);
				WRITE_VREG(HEVC_STREAM_END_ADDR,
					input->start + input->size);
				WRITE_VREG(HEVC_STREAM_RD_PTR,
					input->start);
		}
		return 0;
	}
	if (input->target == VDEC_INPUT_TARGET_VLD) {
		/* restore read side */
		WRITE_VREG(VLD_MEM_SWAP_ADDR,
			input->swap_page_phys);

		/*swap active*/
		WRITE_VREG(VLD_MEM_SWAP_CTL, 1);

		/*wait swap busy*/
		while (READ_VREG(VLD_MEM_SWAP_CTL) & (1<<7))
			;

		WRITE_VREG(VLD_MEM_SWAP_CTL, 0);
	} else if (input->target == VDEC_INPUT_TARGET_HEVC) {
		/* restore read side */
		WRITE_VREG(HEVC_STREAM_SWAP_ADDR,
			input->swap_page_phys);
		WRITE_VREG(HEVC_STREAM_SWAP_CTRL, 1);

		while (READ_VREG(HEVC_STREAM_SWAP_CTRL)
			& (1<<7))
			;
		WRITE_VREG(HEVC_STREAM_SWAP_CTRL, 0);
	}

	return 0;
}


int vdec_sync_input(struct vdec_s *vdec)
{
	struct vdec_input_s *input = &vdec->input;
	u32 rp = 0, wp = 0, fifo_len = 0;
	int size;

	vdec_input_read_restore(vdec);
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
	if (vdec->status < 0)
		return "INVALID";
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
	pr_err("%s timeout!!! status: 0x%x force it to 2\n", __func__, vdec->status);
	if (vdec->status == VDEC_STATUS_ACTIVE) {
		if (vdec->input.target == VDEC_INPUT_TARGET_VLD) {
			amvdec_stop();
			WRITE_VREG(ASSIST_MBOX1_MASK, 0);
			vdec_free_irq(VDEC_IRQ_1, NULL);
		} else if (vdec->input.target == VDEC_INPUT_TARGET_HEVC) {
			amhevc_stop();
			WRITE_VREG(HEVC_ASSIST_MBOX0_IRQ_REG, 0);
			vdec_free_irq(VDEC_IRQ_0, NULL);
		}
	}

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
	if (vdec->mvfrm)
		vfree(vdec->mvfrm);
	vfree(vdec);

#ifdef CONFIG_AMLOGIC_V4L_VIDEO3
	v4lvideo_dec_count_decrease();
#endif
	atomic_dec(&vdec_core->vdec_nr);

	return 0;
}
EXPORT_SYMBOL(vdec_destroy);

static bool is_tunnel_pipeline(u32 pl)
{
	return ((pl & BIT(FRAME_BASE_PATH_DTV_TUNNEL_MODE)) ||
		(pl & BIT(FRAME_BASE_PATH_AMLVIDEO_AMVIDEO))) ?
		true : false;
}

static bool is_nontunnel_pipeline(u32 pl)
{
	return (pl & BIT(FRAME_BASE_PATH_DI_V4LVIDEO)) ? true : false;
}

static bool is_v4lvideo_already_used(u32 pre, int vf_receiver_inst)
{
	if (vf_receiver_inst == 0) {
		if (pre & BIT(FRAME_BASE_PATH_DI_V4LVIDEO_0)) {
			return true;
		}
	} else if (vf_receiver_inst == 1) {
		if (pre & BIT(FRAME_BASE_PATH_DI_V4LVIDEO_1)) {
			return true;
		}
	} else if (vf_receiver_inst == 2) {
		if (pre & BIT(FRAME_BASE_PATH_DI_V4LVIDEO_2)) {
			return true;
		}
	}
	return false;
}

static bool is_res_locked(u32 pre, u32 cur, int vf_receiver_inst)
{
	if (is_tunnel_pipeline(pre)) {
		if (is_tunnel_pipeline(cur)) {
			return true;
		}
	} else if (is_nontunnel_pipeline(cur)) {
		if (is_v4lvideo_already_used(pre,vf_receiver_inst)) {
			return true;
		}
	}
	return false;
}

int vdec_resource_checking(struct vdec_s *vdec)
{
	/*
	 * If it is the single instance that the pipeline of DTV used,
	 * then have to check that the resources which is belong tunnel
	 * pipeline these are being released.
	 */
	ulong expires = jiffies + msecs_to_jiffies(2000);
	while (is_res_locked(vdec_core->vdec_resouce_status,
		BIT(vdec->frame_base_video_path),
		vdec->vf_receiver_inst)) {
		if (time_after(jiffies, expires)) {
			pr_err("wait vdec resource timeout.\n");
			return -EBUSY;
		}
		schedule();
	}

	return 0;
}
EXPORT_SYMBOL(vdec_resource_checking);

/*
 *register vdec_device
 * create output, vfm or create ionvideo output
 */
s32 vdec_init(struct vdec_s *vdec, int is_4k, bool is_v4l)
{
	int r = 0;
	struct vdec_s *p = vdec;
	const char *pdev_name;
	char dev_name[32] = {0};
	int id = PLATFORM_DEVID_AUTO;/*if have used my self*/
	int max_di_count = max_di_instance;
	char postprocess_name[64] = {0};
	if (vdec_stream_based(vdec))
		max_di_count = max_supported_di_instance;
	vdec->is_v4l = is_v4l ? 1 : 0;
	if (is_res_locked(vdec_core->vdec_resouce_status,
		BIT(vdec->frame_base_video_path),
		vdec->vf_receiver_inst))
		return -EBUSY;

	//pr_err("%s [pid=%d,tgid=%d]\n", __func__, current->pid, current->tgid);
	pdev_name = get_dev_name(vdec_single(vdec), vdec->format);
	if (pdev_name == NULL)
		return -ENODEV;

	snprintf(dev_name, sizeof(dev_name),
		"%s%s", pdev_name, is_v4l ? "_v4l": "");

	pr_info("vdec_init, dev_name:%s, vdec_type=%s, format: %d\n",
		dev_name, vdec_type_str(vdec), vdec->format);

	snprintf(vdec->name, sizeof(vdec->name),
		 "vdec-%d", vdec->id);
	snprintf(vdec->dec_spend_time, sizeof(vdec->dec_spend_time),
		 "%s-dec_spend_time", vdec->name);
	snprintf(vdec->dec_spend_time_ave, sizeof(vdec->dec_spend_time_ave),
		 "%s-dec_spend_time_ave", vdec->name);

	/*
	 *todo: VFM patch control should be configurable,
	 * for now all stream based input uses default VFM path.
	 */
	if (!is_support_no_parser()) {
		if (vdec_stream_based(vdec) && !vdec_dual(vdec)) {
			if (vdec_core->vfm_vdec == NULL) {
				pr_debug("vdec_init set vfm decoder %p\n", vdec);
				vdec_core->vfm_vdec = vdec;
			} else {
				pr_info("vdec_init vfm path busy.\n");
				return -EBUSY;
			}
		}
	}

	mutex_lock(&vdec_mutex);
	inited_vcodec_num++;
	mutex_unlock(&vdec_mutex);

	vdec_input_set_type(&vdec->input, vdec->type,
			(vdec->format == VFORMAT_HEVC ||
			vdec->format == VFORMAT_AVS2 ||
			vdec->format == VFORMAT_VP9 ||
			vdec->format == VFORMAT_AV1
			) ?
				VDEC_INPUT_TARGET_HEVC :
				VDEC_INPUT_TARGET_VLD);
	if (vdec_single(vdec) || (vdec_get_debug_flags() & 0x2))
		vdec_enable_DMC(vdec);
	p->cma_dev = vdec_core->cma_dev;

	vdec_canvas_port_register(vdec);

	p->vdec_fps_detec = vdec_fps_detec;
	/* todo */
	if (!vdec_dual(vdec)) {
		p->use_vfm_path =
			is_support_no_parser() ?
			vdec_single(vdec) :
			vdec_stream_based(vdec);
	}

	if (debugflags & (1 << 29))
		p->is_stream_mode_dv_multi = true;
	else
		p->is_stream_mode_dv_multi = false;

	if (debugflags & 0x4)
		p->use_vfm_path = 1;
	/* vdec_dev_reg.flag = 0; */
	if (vdec->id >= 0)
		id = vdec->id;
	p->parallel_dec = parallel_decode;
	p->prog_only = prog_only;

	vdec_core->parallel_dec = parallel_decode;
	vdec->canvas_mode = CANVAS_BLKMODE_32X32;
#ifdef FRAME_CHECK
	vdec_frame_check_init(vdec);
#endif
	/* stream buffer init. */
	if (vdec->vbuf.ops && !vdec->master) {
		r = vdec->vbuf.ops->init(&vdec->vbuf, vdec);
		if (r) {
			pr_err("%s stream buffer init err (%d)\n", dev_name, r);

			mutex_lock(&vdec_mutex);
			inited_vcodec_num--;
			mutex_unlock(&vdec_mutex);

			goto error;
		}

		if (vdec->slave) {
			memcpy(&vdec->slave->vbuf, &vdec->vbuf,
				sizeof(vdec->vbuf));
		}
	}

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
	} else if (!vdec_dual(vdec) && !vdec->disable_vfm) {
		/* create IONVIDEO instance and connect decoder's
		 * vf_provider interface to it
		 */
		if (!is_support_no_parser()) {
			if (p->type != VDEC_TYPE_FRAME_BLOCK) {
				r = -ENODEV;
				pr_err("vdec: Incorrect decoder type\n");

				mutex_lock(&vdec_mutex);
				inited_vcodec_num--;
				mutex_unlock(&vdec_mutex);

				goto error;
			}
		}

		if (strncmp("disable", vfm_path, strlen("disable"))) {
			snprintf(vdec->vfm_map_chain, VDEC_MAP_NAME_SIZE,
				"%s %s", vdec->vf_provider_name, vfm_path);
			snprintf(vdec->vfm_map_id, VDEC_MAP_NAME_SIZE,
				"vdec-map-%d", vdec->id);
		} else if (p->frame_base_video_path == FRAME_BASE_PATH_IONVIDEO) {
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
#ifdef CONFIG_AMLOGIC_IONVIDEO
			snprintf(vdec->vfm_map_chain, VDEC_MAP_NAME_SIZE,
				"%s %s", vdec->vf_provider_name,
				vdec->vf_receiver_name);
			snprintf(vdec->vfm_map_id, VDEC_MAP_NAME_SIZE,
				"vdec-map-%d", vdec->id);
#endif
		} else if (p->frame_base_video_path ==
				FRAME_BASE_PATH_AMLVIDEO_AMVIDEO) {
			if (vdec_secure(vdec)) {
				snprintf(vdec->vfm_map_chain, VDEC_MAP_NAME_SIZE,
					"%s %s", vdec->vf_provider_name,
					"amlvideo amvideo");
			} else {
				if (debug_vdetect)
					snprintf(vdec->vfm_map_chain,
						 VDEC_MAP_NAME_SIZE,
						 "%s vdetect.0 %s",
						 vdec->vf_provider_name,
						 "amlvideo ppmgr deinterlace amvideo");
				else
					snprintf(vdec->vfm_map_chain,
						 VDEC_MAP_NAME_SIZE, "%s %s",
						 vdec->vf_provider_name,
						 "amlvideo ppmgr deinterlace amvideo");
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
		} else if (p->frame_base_video_path == FRAME_BASE_PATH_V4L_OSD) {
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
		} else if (p->frame_base_video_path == FRAME_BASE_PATH_PIP_TUNNEL_MODE) {
			snprintf(vdec->vfm_map_chain, VDEC_MAP_NAME_SIZE,
				"%s %s", vdec->vf_provider_name,
				"videosync.0 videopip");
			snprintf(vdec->vfm_map_id, VDEC_MAP_NAME_SIZE,
				"vdec-map-%d", vdec->id);
		} else if (p->frame_base_video_path == FRAME_BASE_PATH_V4L_VIDEO) {
			snprintf(vdec->vfm_map_chain, VDEC_MAP_NAME_SIZE,
				"%s %s %s", vdec->vf_provider_name,
				vdec->vf_receiver_name, "amvideo");
			snprintf(vdec->vfm_map_id, VDEC_MAP_NAME_SIZE,
				"vdec-map-%d", vdec->id);
		} else if (p->frame_base_video_path ==
				FRAME_BASE_PATH_DI_V4LVIDEO) {
#ifdef CONFIG_AMLOGIC_V4L_VIDEO3
			r = v4lvideo_assign_map(&vdec->vf_receiver_name,
					&vdec->vf_receiver_inst);
#else
			r = -1;
#endif
			 if (r < 0) {
				pr_err("V4lVideo frame receiver allocation failed.\n");
				mutex_lock(&vdec_mutex);
				inited_vcodec_num--;
				mutex_unlock(&vdec_mutex);
				goto error;
			}
#ifdef CONFIG_AMLOGIC_V4L_VIDEO3
			if (v4lvideo_add_ppmgr)
				snprintf(postprocess_name, sizeof(postprocess_name),
					"%s ", "ppmgr");
			if (debug_vdetect && (vdec->vf_receiver_inst == 0))
				snprintf(postprocess_name + strlen(postprocess_name), sizeof(postprocess_name),
					 "%s ", "vdetect.0");
			/* 8K remove di */
			if ((vdec->sys_info->width * vdec->sys_info->height > (4096 * 2304))
				|| (!v4lvideo_add_di))
				snprintf(vdec->vfm_map_chain, VDEC_MAP_NAME_SIZE,
					"%s %s%s", vdec->vf_provider_name,
					postprocess_name,
					vdec->vf_receiver_name);
			else {
				if ((vdec->vf_receiver_inst == 0)
					&& (max_di_count > 0))
					if (max_di_count == 1)
						snprintf(vdec->vfm_map_chain, VDEC_MAP_NAME_SIZE,
							"%s %s%s %s", vdec->vf_provider_name,
							postprocess_name,
							"deinterlace",
							vdec->vf_receiver_name);
					else
						snprintf(vdec->vfm_map_chain, VDEC_MAP_NAME_SIZE,
							"%s %s%s %s", vdec->vf_provider_name,
							postprocess_name,
							"dimulti.1",
							vdec->vf_receiver_name);
				else if ((vdec->vf_receiver_inst <
					  max_di_count) &&
					  (vdec->vf_receiver_inst == 1))
					snprintf(vdec->vfm_map_chain,
						 VDEC_MAP_NAME_SIZE,
						 "%s %s %s",
						 vdec->vf_provider_name,
						 "deinterlace",
						 vdec->vf_receiver_name);
				else if (vdec->vf_receiver_inst <
					 max_di_count)
					snprintf(vdec->vfm_map_chain,
						 VDEC_MAP_NAME_SIZE,
						 "%s %s%d %s",
						 vdec->vf_provider_name,
						 "dimulti.",
						 vdec->vf_receiver_inst,
						 vdec->vf_receiver_name);
				else
					snprintf(vdec->vfm_map_chain, VDEC_MAP_NAME_SIZE,
						"%s %s", vdec->vf_provider_name,
						vdec->vf_receiver_name);
			}
			snprintf(vdec->vfm_map_id, VDEC_MAP_NAME_SIZE,
				"vdec-map-%d", vdec->id);
#endif
		} else if (p->frame_base_video_path ==
				FRAME_BASE_PATH_V4LVIDEO) {
#ifdef CONFIG_AMLOGIC_V4L_VIDEO3
			r = v4lvideo_assign_map(&vdec->vf_receiver_name,
					&vdec->vf_receiver_inst);
#else
			r = -1;
#endif
			if (r < 0) {
				pr_err("V4lVideo frame receiver allocation failed.\n");
				mutex_lock(&vdec_mutex);
				inited_vcodec_num--;
				mutex_unlock(&vdec_mutex);
				goto error;
			}
#ifdef CONFIG_AMLOGIC_V4L_VIDEO3
			snprintf(vdec->vfm_map_chain, VDEC_MAP_NAME_SIZE,
				"%s %s", vdec->vf_provider_name,
				vdec->vf_receiver_name);
			snprintf(vdec->vfm_map_id, VDEC_MAP_NAME_SIZE,
				"vdec-map-%d", vdec->id);
#endif
		} else if (p->frame_base_video_path ==
			FRAME_BASE_PATH_DTV_TUNNEL_MODE) {
			snprintf(vdec->vfm_map_chain, VDEC_MAP_NAME_SIZE,
				"%s deinterlace %s", vdec->vf_provider_name,
				"amvideo");
			snprintf(vdec->vfm_map_id, VDEC_MAP_NAME_SIZE,
				"vdec-map-%d", vdec->id);
		} else if (p->frame_base_video_path ==
			FRAME_BASE_PATH_AMLVIDEO_FENCE) {
			snprintf(vdec->vfm_map_chain, VDEC_MAP_NAME_SIZE,
				"%s %s", vdec->vf_provider_name,
				"amlvideo amvideo");
			snprintf(vdec->vfm_map_id, VDEC_MAP_NAME_SIZE,
				"vdec-map-%d", vdec->id);
		} else if (p->frame_base_video_path ==
			FRAME_BASE_PATH_V4LVIDEO_FENCE) {
#ifdef CONFIG_AMLOGIC_V4L_VIDEO3
			r = v4lvideo_assign_map(&vdec->vf_receiver_name,
					&vdec->vf_receiver_inst);
#else
			r = -1;
#endif
			if (r < 0) {
				pr_err("V4lVideo frame receiver allocation failed.\n");
				mutex_lock(&vdec_mutex);
				inited_vcodec_num--;
				mutex_unlock(&vdec_mutex);
				goto error;
			}
#ifdef CONFIG_AMLOGIC_V4L_VIDEO3
			snprintf(vdec->vfm_map_chain, VDEC_MAP_NAME_SIZE,
					"%s %s", vdec->vf_provider_name,
					vdec->vf_receiver_name);
			snprintf(vdec->vfm_map_id, VDEC_MAP_NAME_SIZE,
					"vdec-map-%d", vdec->id);
#endif
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

	if (!vdec_single(vdec) && !vdec->disable_vfm) {
		vf_reg_provider(&p->vframe_provider);

		vf_notify_receiver(p->vf_provider_name,
			VFRAME_EVENT_PROVIDER_START,
			vdec);

		if (vdec_core->hint_fr_vdec == NULL)
			vdec_core->hint_fr_vdec = vdec;

		if (vdec_core->hint_fr_vdec == vdec) {
			if (p->sys_info->rate != 0) {
				if (!vdec->is_reset) {
					vf_notify_receiver(p->vf_provider_name,
						VFRAME_EVENT_PROVIDER_FR_HINT,
						(void *)
						((unsigned long)
						p->sys_info->rate));
					vdec->fr_hint_state = VDEC_HINTED;
				}
			} else {
				vdec->fr_hint_state = VDEC_NEED_HINT;
			}
		}
	}
	if (vdec_single(vdec) && !vdec_secure(vdec)) {
		if (!is_support_no_parser())
			tee_config_device_state(DMC_DEV_ID_PARSER, 0);
		tee_config_device_state(DMC_DEV_ID_VDEC, 0);
	}
	p->dolby_meta_with_el = 0;
	pr_debug("vdec_init, vf_provider_name = %s, b %d\n",
		p->vf_provider_name, is_cpu_tm2_revb());

	mutex_lock(&vdec_mutex);
	vdec_core->vdec_resouce_status |= BIT(p->frame_base_video_path);
	if (p->frame_base_video_path == FRAME_BASE_PATH_DI_V4LVIDEO) {
		if (p->vf_receiver_inst == 0) {
			vdec_core->vdec_resouce_status |= BIT(FRAME_BASE_PATH_DI_V4LVIDEO_0);
		} else if (p->vf_receiver_inst == 1) {
			vdec_core->vdec_resouce_status |= BIT(FRAME_BASE_PATH_DI_V4LVIDEO_1);
		} else if (p->vf_receiver_inst == 2) {
			vdec_core->vdec_resouce_status |= BIT(FRAME_BASE_PATH_DI_V4LVIDEO_2);
		}
	}
	if (vdec_dual(vdec) && (!(vdec->port->type & PORT_TYPE_FRAME))) {
		//DV stream mode
		if (vdec->slave)
			vdec_core->inst_cnt++;
	} else {
		//DV frame mode
		vdec_core->inst_cnt++;
	}
	vdec->inst_cnt = vdec_core->inst_cnt;
	mutex_unlock(&vdec_mutex);
	pr_debug("vdec_init, inst_cnt = %d, port type 0x%x\n", vdec->inst_cnt, vdec->port->type);

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

/*
 *Remove the vdec after timeout happens both in vdec_disconnect
 *and platform_device_unregister. Then after, we can release the vdec.
 */
static void vdec_connect_list_force_clear(struct vdec_core_s *core, struct vdec_s *v_ref)
{
	struct vdec_s *vdec, *tmp;
	unsigned long flags;

	flags = vdec_core_lock(core);

	list_for_each_entry_safe(vdec, tmp,
		&core->connected_vdec_list, list) {
		if ((vdec->status == VDEC_STATUS_DISCONNECTED) &&
		    (vdec == v_ref)) {
		    pr_err("%s, vdec = %p, active vdec = %p\n",
				__func__, vdec, core->active_vdec);
			if (v_ref->active_mask)
				core->sched_mask &= ~v_ref->active_mask;
			if (core->active_vdec == v_ref)
				core->active_vdec = NULL;
			if (core->active_hevc == v_ref)
				core->active_hevc = NULL;
			if (core->last_vdec == v_ref)
				core->last_vdec = NULL;
			list_del(&vdec->list);
		}
	}

	vdec_core_unlock(core, flags);
}

st_userdata *get_vdec_userdata_ctx()
{
	return &userdata;
}
EXPORT_SYMBOL(get_vdec_userdata_ctx);

static void vdec_userdata_ctx_release(struct vdec_s *vdec)
{
	int i;
	st_userdata *userdata = get_vdec_userdata_ctx();

	mutex_lock(&userdata->mutex);

	for (i = 0; i < MAX_USERDATA_CHANNEL_NUM; i++) {
		if (userdata->used[i] == 1 && vdec->video_id != 0xffffffff) {
			if (vdec_get_debug_flags() & 0x10000000)
				pr_info("ctx_release i: %d userdata.id %d\n",
				i, userdata->id[i]);
			userdata->ready_flag[i] = 0;
			userdata->id[i] = -1;
			userdata->used[i] = 0;
			userdata->set_id_flag = 0;
		}
	}

	mutex_unlock(&userdata->mutex);

	return;
}

/* vdec_create/init/release/destroy are applied to both dual running decoders
 */
void vdec_release(struct vdec_s *vdec)
{
	u32 wcount = 0;

	//trace_vdec_release(vdec);/*DEBUG_TMP*/
#ifdef VDEC_DEBUG_SUPPORT
	if (step_mode) {
		pr_info("VDEC_DEBUG: in step_mode, wait release\n");
		while (step_mode)
			udelay(10);
		pr_info("VDEC_DEBUG: step_mode is clear\n");
	}
#endif
	/* When release, userspace systemctl need this duration 0 event */
	vdec_frame_rate_uevent(0);
	vdec_disconnect(vdec);

	if (!vdec->disable_vfm && vdec->vframe_provider.name) {
		if (!vdec_single(vdec)) {
			if (vdec_core->hint_fr_vdec == vdec
			&& vdec->fr_hint_state == VDEC_HINTED)
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

	while (vdec->irq_cnt > vdec->irq_thread_cnt) {
		if ((wcount & 0x1f) == 0)
			pr_debug("%s vdec[%lx]: %lld > %lld, loop %u times\n",__func__, (unsigned long)vdec,
				vdec->irq_cnt,vdec->irq_thread_cnt, wcount);
		/*
		 * Wait at most 2000 ms.
		 * In suspend scenario, the system may disable thread_fn,
		 * thus can NOT always waiting the thread_fn happen
		 */
		if (++wcount > 1000)
			break;
		usleep_range(1000, 2000);
	}

#ifdef FRAME_CHECK
	vdec_frame_check_exit(vdec);
#endif
	vdec_fps_clear(vdec->id);
	if (atomic_read(&vdec_core->vdec_nr) == 1)
		vdec_disable_DMC(vdec);
	platform_device_unregister(vdec->dev);
	/*Check if the vdec still in connected list, if yes, delete it*/
	vdec_connect_list_force_clear(vdec_core, vdec);

	if (vdec->vbuf.ops && !vdec->master)
		vdec->vbuf.ops->release(&vdec->vbuf);

	vdec_userdata_ctx_release(vdec);

	pr_debug("vdec_release instance %p, total %d\n", vdec,
		atomic_read(&vdec_core->vdec_nr));

	mutex_lock(&vdec_mutex);
	if (vdec->frame_base_video_path == FRAME_BASE_PATH_DI_V4LVIDEO) {
		if (vdec->vf_receiver_inst == 0) {
			vdec_core->vdec_resouce_status &= ~BIT(FRAME_BASE_PATH_DI_V4LVIDEO_0);
		} else if (vdec->vf_receiver_inst == 1) {
			vdec_core->vdec_resouce_status &= ~BIT(FRAME_BASE_PATH_DI_V4LVIDEO_1);
		} else if (vdec->vf_receiver_inst == 2) {
			vdec_core->vdec_resouce_status &= ~BIT(FRAME_BASE_PATH_DI_V4LVIDEO_2);
		}
	}
	vdec_core->vdec_resouce_status &= ~BIT(vdec->frame_base_video_path);
	mutex_unlock(&vdec_mutex);
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

	if (!vdec->disable_vfm) {
		if (vdec->vframe_provider.name)
			vf_unreg_provider(&vdec->vframe_provider);

		if ((vdec->slave) && (vdec->slave->vframe_provider.name))
			vf_unreg_provider(&vdec->slave->vframe_provider);
	}

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

	if (!vdec->disable_vfm) {
		vf_reg_provider(&vdec->vframe_provider);
		vf_notify_receiver(vdec->vf_provider_name,
				VFRAME_EVENT_PROVIDER_START, vdec);

		if (vdec->slave) {
			vf_reg_provider(&vdec->slave->vframe_provider);
			vf_notify_receiver(vdec->slave->vf_provider_name,
				VFRAME_EVENT_PROVIDER_START, vdec->slave);
			vdec->slave->mc_loaded = 0;/*clear for reload firmware*/
		}
	}

	vdec_connect(vdec);

	return 0;
}
EXPORT_SYMBOL(vdec_reset);

int vdec_v4l2_reset(struct vdec_s *vdec, int flag)
{
	//trace_vdec_reset(vdec); /*DEBUG_TMP*/
	pr_debug("vdec_v4l2_reset %d\n", flag);
	vdec_disconnect(vdec);
	if (flag != 2) {
		if (!vdec->disable_vfm) {
			if (vdec->vframe_provider.name)
				vf_unreg_provider(&vdec->vframe_provider);

			if ((vdec->slave) && (vdec->slave->vframe_provider.name))
				vf_unreg_provider(&vdec->slave->vframe_provider);
		}

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

		if (!vdec->disable_vfm) {
			vf_reg_provider(&vdec->vframe_provider);
			vf_notify_receiver(vdec->vf_provider_name,
					VFRAME_EVENT_PROVIDER_START, vdec);

			if (vdec->slave) {
				vf_reg_provider(&vdec->slave->vframe_provider);
				vf_notify_receiver(vdec->slave->vf_provider_name,
					VFRAME_EVENT_PROVIDER_START, vdec->slave);
				vdec->slave->mc_loaded = 0;/*clear for reload firmware*/
			}
		}
	} else {
		if (vdec->reset) {
			vdec->reset(vdec);
			if (vdec->slave)
				vdec->slave->reset(vdec->slave);
		}
	}

	vdec_connect(vdec);

	return 0;
}
EXPORT_SYMBOL(vdec_v4l2_reset);

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

	if (vdec->active_mask == 0) {
		vdec_set_status(vdec, VDEC_STATUS_CONNECTED);
		wake_up_interruptible(&vdec->idle_wait);
	}

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
	if (vdec && ret == IRQ_WAKE_THREAD)
		vdec->irq_cnt++;

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
		vdec->irq_thread_cnt++;
	return ret;
}

int vdec_check_rec_num_enough(struct vdec_s *vdec) {

	if (vdec->vbuf.use_ptsserv) {
		return (pts_get_rec_num(PTS_TYPE_VIDEO,
					vdec->input.total_rd_count) >= 2);
	} else {
		u64 total_rd_count = vdec->input.total_rd_count;

		if (vdec->input.target == VDEC_INPUT_TARGET_VLD) {
			//total_rd_count -= vdec->input.start;
			/*just like use ptsserv, alway return true*/
			return 1;
		}
		if ((total_rd_count >= vdec->vbuf.last_offset[0]) &&
			(total_rd_count - vdec->vbuf.last_offset[0] < 0x80000000))
			return 0;
		else if ((total_rd_count >= vdec->vbuf.last_offset[1]) &&
			(total_rd_count - vdec->vbuf.last_offset[1] < 0x80000000))
			return 0;

		return 1;
	}
}

unsigned long vdec_ready_to_run(struct vdec_s *vdec, unsigned long mask)
{
	unsigned long ready_mask;
	struct vdec_input_s *input = &vdec->input;

	/* Wait the matching irq_thread finished */
	if (vdec->irq_cnt > vdec->irq_thread_cnt)
		return false;

	if ((vdec->status != VDEC_STATUS_CONNECTED) &&
	    (vdec->status != VDEC_STATUS_ACTIVE))
		return false;

	if (!vdec->run_ready)
		return false;

	/* when crc32 error, block at error frame */
	if (vdec->vfc.err_crc_block)
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

			rp = STBUF_READ(&vdec->vbuf, get_rp);
			wp = STBUF_READ(&vdec->vbuf, get_wp);
			if (wp < rp)
				level = input->size + wp - rp;
			else
				level = wp - rp;

			if ((level < input->prepare_level) &&
				!vdec_check_rec_num_enough(vdec)) {
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
#define DMC_DEV_TYPE_NON_SECURE        0
#define DMC_DEV_TYPE_SECURE            1

void vdec_prepare_run(struct vdec_s *vdec, unsigned long mask)
{
	struct vdec_input_s *input = &vdec->input;
	int secure = (vdec_secure(vdec)) ? DMC_DEV_TYPE_SECURE :
			DMC_DEV_TYPE_NON_SECURE;

	vdec_route_interrupt(vdec, mask, true);

	if (!vdec_core_with_input(mask))
		return;

	if (vdec_stream_based(vdec) && !vdec_secure(vdec))
	{
		tee_config_device_state(DMC_DEV_ID_PARSER, 0);
	}
	if (input->target == VDEC_INPUT_TARGET_VLD)
		tee_config_device_state(DMC_DEV_ID_VDEC, secure);
	else if (input->target == VDEC_INPUT_TARGET_HEVC)
		tee_config_device_state(DMC_DEV_ID_HEVC, secure);

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
	unsigned long flags;
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
		vdec_update_buff_status();
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
		flags = vdec_core_lock(vdec_core);
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
		vdec_core_unlock(vdec_core, flags);
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
			} else
				vdec->mc_loaded = 0;
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
					(atomic_read(&vdec_core->vdec_nr) > 0) &&
					((core->buff_flag | core->stream_buff_flag) &
					(core->sched_mask ^ core->power_ref_mask))) {
						usleep_range(1000, 2000);
						up(&core->sem);
				}
			} else {
				if ((!worker) && (!core->sched_mask) &&
					(atomic_read(&vdec_core->vdec_nr) > 0) &&
					(core->buff_flag | core->stream_buff_flag)) {
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

	vdec_core->pm->power_on(vdec_core->cma_dev, core);

	mutex_unlock(&vdec_mutex);
}
EXPORT_SYMBOL(vdec_poweron);

void vdec_poweroff(enum vdec_type_e core)
{
	if (core >= VDEC_MAX)
		return;

	mutex_lock(&vdec_mutex);
	if (vdec_core->power_ref_count[core] == 0) {
		mutex_unlock(&vdec_mutex);
		return;
	}

	vdec_core->power_ref_count[core]--;
	if (vdec_core->power_ref_count[core] > 0) {
		mutex_unlock(&vdec_mutex);
		return;
	}

	vdec_core->pm->power_off(vdec_core->cma_dev, core);

	mutex_unlock(&vdec_mutex);
}
EXPORT_SYMBOL(vdec_poweroff);

bool vdec_on(enum vdec_type_e core)
{
	return vdec_core->pm->power_state(vdec_core->cma_dev, core);
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

void vdec_reset_core(struct vdec_s *vdec)
{
	if ((get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_T7) ||
		(get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_T3)) {
		/* t7 no dmc req for vdec only */
		vdec_dbus_ctrl(0);
	} else {
		dec_dmc_port_ctrl(0, VDEC_INPUT_TARGET_VLD);
	}
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
	WRITE_VREG(DOS_SW_RESET0, (1<<3)|(1<<4)|(1<<5)|(1<<7)|(1<<8)|(1<<9));
	WRITE_VREG(DOS_SW_RESET0, 0);

	if ((get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_T7) ||
		(get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_T3))
		vdec_dbus_ctrl(1);
	else
		dec_dmc_port_ctrl(1, VDEC_INPUT_TARGET_VLD);
}
EXPORT_SYMBOL(vdec_reset_core);

void hevc_mmu_dma_check(struct vdec_s *vdec)
{
	ulong timeout;
	u32 data;
	if (get_cpu_major_id() < AM_MESON_CPU_MAJOR_ID_G12A)
		return;
	timeout = jiffies + HZ/100;
	while (1) {
		data  = READ_VREG(HEVC_CM_CORE_STATUS);
		if ((data & 0x1) == 0)
			break;
		if (time_after(jiffies, timeout)) {
			if (debug & 0x10)
				pr_info(" %s sao mmu dma idle\n", __func__);
			break;
		}
	}
	/*disable sao mmu dma */
	CLEAR_VREG_MASK(HEVC_SAO_MMU_DMA_CTRL, 1 << 0);
	timeout = jiffies + HZ/100;
	while (1) {
		data  = READ_VREG(HEVC_SAO_MMU_DMA_STATUS);
		if ((data & 0x1))
			break;
		if (time_after(jiffies, timeout)) {
			if (debug & 0x10)
				pr_err("%s sao mmu dma timeout, num_buf_used = 0x%x\n",
				__func__, (READ_VREG(HEVC_SAO_MMU_STATUS) >> 16));
			break;
		}
	}
}
EXPORT_SYMBOL(hevc_mmu_dma_check);

void hevc_reset_core(struct vdec_s *vdec)
{
	int cpu_type = get_cpu_major_id();

	if ((cpu_type == AM_MESON_CPU_MAJOR_ID_T7) ||
		(cpu_type == AM_MESON_CPU_MAJOR_ID_T3)) {
		/* t7 no dmc req for hevc only */
		hevc_arb_ctrl(0);
	} else {
		WRITE_VREG(HEVC_STREAM_CONTROL, 0);

		dec_dmc_port_ctrl(0, VDEC_INPUT_TARGET_HEVC);
	}

	if (vdec == NULL || input_frame_based(vdec))
		WRITE_VREG(HEVC_STREAM_CONTROL, 0);

	WRITE_VREG(HEVC_SAO_MMU_RESET_CTRL,
			READ_VREG(HEVC_SAO_MMU_RESET_CTRL) | 1);

	/*
	 * 2: assist
	 * 3: parser
	 * 4: parser_state
	 * 8: dblk
	 * 10:wrrsp lmem
	 * 11:mcpu
	 * 12:ccpu
	 * 13:ddr
	 * 14:iqit
	 * 15:ipp
	 * 17:qdct
	 * 18:mpred
	 * 19:sao
	 * 24:hevc_afifo
	 * 26:rst_mmu_n
	 */
	WRITE_VREG(DOS_SW_RESET3,
		(1<<3)|(1<<4)|(1<<8)|(1<<10)|(1<<11)|
		(1<<12)|(1<<13)|(1<<14)|(1<<15)|
		(1<<17)|(1<<18)|(1<<19)|(1<<24)|(1<<26));

	WRITE_VREG(DOS_SW_RESET3, 0);
	while (READ_VREG(HEVC_WRRSP_LMEM) & 0xfff)
		;
	WRITE_VREG(HEVC_SAO_MMU_RESET_CTRL,
			READ_VREG(HEVC_SAO_MMU_RESET_CTRL) & (~1));

	if (cpu_type == AM_MESON_CPU_MAJOR_ID_TL1 &&
			is_meson_rev_b())
		cpu_type = AM_MESON_CPU_MAJOR_ID_G12B;
	switch (cpu_type) {
	case AM_MESON_CPU_MAJOR_ID_G12B:
		WRITE_RESET_REG((RESET7_REGISTER_LEVEL),
				READ_RESET_REG(RESET7_REGISTER_LEVEL) & (~((1<<13)|(1<<14))));
		WRITE_RESET_REG((RESET7_REGISTER_LEVEL),
				READ_RESET_REG((RESET7_REGISTER_LEVEL)) | ((1<<13)|(1<<14)));
		break;
	case AM_MESON_CPU_MAJOR_ID_G12A:
	case AM_MESON_CPU_MAJOR_ID_SM1:
	case AM_MESON_CPU_MAJOR_ID_TL1:
	case AM_MESON_CPU_MAJOR_ID_TM2:
	case AM_MESON_CPU_MAJOR_ID_T5:
	case AM_MESON_CPU_MAJOR_ID_T5D:
	case AM_MESON_CPU_MAJOR_ID_T5W:
		WRITE_RESET_REG((RESET7_REGISTER_LEVEL),
				READ_RESET_REG(RESET7_REGISTER_LEVEL) & (~((1<<13))));
		WRITE_RESET_REG((RESET7_REGISTER_LEVEL),
				READ_RESET_REG((RESET7_REGISTER_LEVEL)) | ((1<<13)));
		break;
	case AM_MESON_CPU_MAJOR_ID_SC2:
	case AM_MESON_CPU_MAJOR_ID_S4:
	case AM_MESON_CPU_MAJOR_ID_S4D:
		WRITE_RESET_REG(P_RESETCTRL_RESET5_LEVEL,
				READ_RESET_REG(P_RESETCTRL_RESET5_LEVEL) & (~((1<<1)|(1<<12)|(1<<13))));
		WRITE_RESET_REG(P_RESETCTRL_RESET5_LEVEL,
				READ_RESET_REG(P_RESETCTRL_RESET5_LEVEL) | ((1<<1)|(1<<12)|(1<<13)));
		break;
	default:
		break;
	}

	if ((cpu_type == AM_MESON_CPU_MAJOR_ID_T7) ||
		(cpu_type == AM_MESON_CPU_MAJOR_ID_T3))
		hevc_arb_ctrl(1);
	else
		dec_dmc_port_ctrl(1, VDEC_INPUT_TARGET_HEVC);
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

static ssize_t enable_mvdec_info_show(struct class *cla,
				  struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", enable_mvdec_info);
}

static ssize_t enable_mvdec_info_store(struct class *cla,
				   struct class_attribute *attr,
				   const char *buf, size_t count)
{
	int r;
	int val;

	r = kstrtoint(buf, 0, &val);
	if (r < 0)
		return -EINVAL;
	enable_mvdec_info = val;

	return count;
}
static ssize_t poweron_clock_level_store(struct class *class,
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

static ssize_t poweron_clock_level_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", poweron_clock_level);
}

/*
 *if keep_vdec_mem == 1
 *always don't release
 *vdec 64 memory for fast play.
 */
static ssize_t keep_vdec_mem_store(struct class *class,
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

static ssize_t keep_vdec_mem_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", keep_vdec_mem);
}

#ifdef VDEC_DEBUG_SUPPORT
static ssize_t debug_store(struct class *class,
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
	} else if (strcmp(cbuf, "no_interlace") == 0) {
		prog_only ^= 1;
		pr_info("set prog only %d, %s output\n",
			prog_only, prog_only?"force one filed only":"interlace");
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

static ssize_t debug_show(struct class *class,
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
		"no_interlace - force v4l no_interlace output. %d\n", prog_only);
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

#ifdef PXP_DEBUG
static unsigned pxp_buf_alloc_size = 0;
static void *pxp_buf_addr = NULL;
static ulong pxp_buf_phy_addr = 0;
static ulong pxp_buf_mem_handle = 0;

static ssize_t pxp_buf_store(struct class *class,
		struct class_attribute *attr,
		const char *buf, size_t size)
{
	ssize_t ret;
	char cbuf[32];
	unsigned val;

	cbuf[0] = 0;
	ret = sscanf(buf, "%s %x", cbuf, &val);
	/*pr_info(
	"%s(%s)=>ret %ld: %s, %x, %x\n",
	__func__, buf, ret, cbuf, id, val);*/
	if (strcmp(cbuf, "alloc") == 0) {
		if (pxp_buf_addr) {
			codec_mm_dma_free_coherent(pxp_buf_mem_handle);
			pxp_buf_addr = NULL;
			pxp_buf_alloc_size = 0;
			pxp_buf_phy_addr = 0;
		}
		pxp_buf_alloc_size = PAGE_ALIGN(val);
		if (pxp_buf_alloc_size>0) {
			pxp_buf_addr = codec_mm_dma_alloc_coherent(&pxp_buf_mem_handle,
				&pxp_buf_phy_addr, pxp_buf_alloc_size, "pxp_buf");
			if (!pxp_buf_addr) {
				pr_err("%s: failed to alloc pxp_buf buf\n", __func__);
				pxp_buf_alloc_size = 0;
				return size;
			}
		}
		pr_info("alloc pxp_buf (phy adr 0x%x, size 0x%x)\n",
			pxp_buf_phy_addr, pxp_buf_alloc_size);

	} else if (strcmp(cbuf, "free") == 0) {
		if (pxp_buf_addr) {
			codec_mm_dma_free_coherent(pxp_buf_mem_handle);
			pxp_buf_addr = NULL;
		}
		pxp_buf_alloc_size = 0;
	} else if (strcmp(cbuf, "fill") == 0) {
		memset(pxp_buf_addr, val&0xff, pxp_buf_alloc_size);
	} else if (strcmp(cbuf, "info") == 0) {
		pr_info("pxp_buf_alloc_size = 0x%x\n", pxp_buf_alloc_size);
		pr_info("pxp_buf_addr = 0x%p\n", pxp_buf_addr);
		pr_info("pxp_buf_phy_addr = 0x%p\n", pxp_buf_phy_addr);
	} else if (strcmp(cbuf, "show") == 0) {
		unsigned char* ptr = (unsigned char *)pxp_buf_addr;
		pr_info("0x%x: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
			val, ptr[val+0], ptr[val+1], ptr[val+2], ptr[val+3], ptr[val+4], ptr[val+5], ptr[val+6], ptr[val+7],
			ptr[val+8], ptr[val+9], ptr[val+10], ptr[val+11], ptr[val+12], ptr[val+13], ptr[val+14], ptr[val+15]
			);
	} else if (strcmp(cbuf, "save") == 0) {
		unsigned file_size = 0;
		struct file* fp;
		loff_t pos;
		mm_segment_t old_fs;
		unsigned int wr_size;
		char path[32];
		sscanf(buf, "%s %s %d", cbuf, path, &file_size);
		if (file_size == 0)
			file_size = pxp_buf_alloc_size;
		fp = filp_open(path, O_CREAT | O_WRONLY | O_TRUNC, 0666);
		if (IS_ERR(fp)) {
			fp = NULL;
			pr_info("open %s failed\n", path);
		} else {
			old_fs = get_fs();
			set_fs(KERNEL_DS);
			wr_size = vfs_write(fp,
					pxp_buf_addr,
					file_size, &pos);
			pr_info("write %d to %s\n", wr_size, path);
			set_fs(old_fs);
			vfs_fsync(fp, 0);
			filp_close(fp, current->files);
		}
	} else {
		pr_info("command examples:\n");
		pr_info("echo info\n");
		pr_info("echo alloc a0 (size: hex)\n");
		pr_info("echo free\n");
		pr_info("echo show 10 (offset: hex)\n");
		pr_info("echo fill a5\n");
		pr_info("echo save /streams/aa.tar.gz 512 (size: decimal\n");
	}
	return size;
}

static ssize_t pxp_buf_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	return 0;
}
#endif


int show_stream_buffer_status(char *buf,
	int (*callback) (struct stream_buf_s *, char *))
{
	char *pbuf = buf;
	struct vdec_s *vdec;
	struct vdec_core_s *core = vdec_core;
	u64 flags = vdec_core_lock(vdec_core);

	list_for_each_entry(vdec,
		&core->connected_vdec_list, list) {
		if ((vdec->status == VDEC_STATUS_CONNECTED
			|| vdec->status == VDEC_STATUS_ACTIVE)) {
			if (vdec_frame_based(vdec))
				continue;
			pbuf += callback(&vdec->vbuf, pbuf);
		}
	}
	vdec_core_unlock(vdec_core, flags);

	return pbuf - buf;
}
EXPORT_SYMBOL(show_stream_buffer_status);

static ssize_t vdec_vfm_path_store(struct class *class,
		 struct class_attribute *attr,
		 const char *buf, size_t count)
{
	char *buf_dup, *ps, *token;
	char str[VDEC_MAP_NAME_SIZE] = "\0";
	bool found = false;
	int i;

	if (strlen(buf) >= VDEC_MAP_NAME_SIZE) {
		pr_info("parameter is overflow\n");
		return -1;
	}

	buf_dup = kstrdup(buf, GFP_KERNEL);
	ps = buf_dup;
	while (1) {
		token = strsep(&ps, "\n ");
		if (token == NULL)
			break;
		if (*token == '\0')
			continue;

		for (i = 0; strcmp("reserved", vfm_path_node[i]) != 0; i++) {
			if (!strncmp (vfm_path_node[i], token, strlen(vfm_path_node[i]))) {
			        break;
			}
		}

		if (strcmp("reserved", vfm_path_node[i]) == 0 ||
			strncmp("help", buf, strlen("help")) == 0) {
			if (strncmp("help", buf, strlen("help")) != 0) {
				pr_info("warnning! Input parameter is invalid. set failed!\n");
			}
			pr_info("\nusage for example: \n");
			pr_info("echo help > /sys/class/vdec/vfm_path \n");
			pr_info("echo disable > /sys/class/vdec/vfm_path \n");
			pr_info("echo amlvideo ppmgr amvideo > /sys/class/vdec/vfm_path \n");
			found = false;

			break;
		} else {
			strcat(str, vfm_path_node[i]);
			strcat(str, " ");
			found = true;
		}
	}

	if (found == true) {
		memset(vfm_path, 0, sizeof(vfm_path));
		strncpy(vfm_path, str, strlen(str));
		vfm_path[VDEC_MAP_NAME_SIZE - 1] = '\0';
		pr_info("cfg path success: decoder %s\n", vfm_path);
	}
	kfree(buf_dup);

	return count;
}

static ssize_t vdec_vfm_path_show(struct class *class,
	struct class_attribute *attr, char *buf)
{
	int len = 0;
	int i;
	len += sprintf(buf + len, "cfg vfm path: decoder %s\n", vfm_path);
	len += sprintf(buf + len, "\nvfm path node list: \n");
	for (i = 0; strcmp("reserved", vfm_path_node[i]) != 0; i++) {
		len += sprintf(buf + len, "\t%s \n", vfm_path_node[i]);
	}

	return len;
}

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

void vdec_sync_irq(enum vdec_irq_num num)
{
#if 0
	if (!vdec)
		return;
	if (vdec->input.target == VDEC_INPUT_TARGET_HEVC)
		synchronize_irq(vdec_core->isr_context[VDEC_IRQ_0].irq);
	else (vdec->input.target == VDEC_INPUT_TARGET_VLD)
		synchronize_irq(vdec_core->isr_context[VDEC_IRQ_1].irq);
#endif
	synchronize_irq(vdec_core->isr_context[num].irq);
}
EXPORT_SYMBOL(vdec_sync_irq);

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

struct vdec_s *vdec_get_vdec_by_video_id(int video_id)
{
	struct vdec_s *vdec;
	struct vdec_s *ret_vdec;
	struct vdec_core_s *core = vdec_core;
	unsigned long flags;

	flags = vdec_core_lock(vdec_core);

	ret_vdec = NULL;
	if (!list_empty(&core->connected_vdec_list)) {
		list_for_each_entry(vdec, &core->connected_vdec_list, list) {
			if (vdec->video_id == video_id) {
				ret_vdec = vdec;
				break;
			}
		}
	}

	vdec_core_unlock(vdec_core, flags);

	return ret_vdec;
}
EXPORT_SYMBOL(vdec_get_vdec_by_video_id);

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

void vdec_set_profile_level(struct vdec_s *vdec, u32 profile_idc, u32 level_idc)
{
	if (vdec) {
		vdec->profile_idc = profile_idc;
		vdec->level_idc = level_idc;
	}
}
EXPORT_SYMBOL(vdec_set_profile_level);

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
	unsigned long flags = vdec_core_lock(vdec_core);

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
				"\tvdec.%d (%p (%s%s)), status = %s,\ttype = %s, \tactive_mask = %lx\n",
				vdec->id,
				vdec,
				vdec_device_name[vdec->format * 2],
				(vdec->is_v4l == 1) ? "_v4l" : "",
				vdec_status_str(vdec),
				vdec_type_str(vdec),
				vdec->active_mask);
		}
	}

	vdec_core_unlock(vdec_core, flags);
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
				"%13s : %llu %s\n", "total data",
				vs.total_data / 1024, "KB");
			pbuf += sprintf(pbuf,
				"%13s : %x\n\n", "ratio_control",
				vs.ratio_control);

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
	unsigned long flags = vdec_core_lock(vdec_core);

	if (list_empty(&core->connected_vdec_list))
		pbuf += sprintf(pbuf, "connected vdec list empty\n");
	else {
		struct vdec_s *vdec;
		list_for_each_entry(vdec, &core->connected_vdec_list, list) {
			pbuf += vdec_input_dump_blocks(&vdec->input,
				pbuf, PAGE_SIZE - (pbuf - buf));
		}
	}
	vdec_core_unlock(vdec_core, flags);

	return pbuf - buf;
}
static ssize_t dump_vdec_chunks_show(struct class *class,
			struct class_attribute *attr, char *buf)
{
	struct vdec_core_s *core = vdec_core;
	char *pbuf = buf;
	unsigned long flags = vdec_core_lock(vdec_core);

	if (list_empty(&core->connected_vdec_list))
		pbuf += sprintf(pbuf, "connected vdec list empty\n");
	else {
		struct vdec_s *vdec;
		list_for_each_entry(vdec, &core->connected_vdec_list, list) {
			pbuf += vdec_input_dump_chunks(vdec->id, &vdec->input,
				pbuf, PAGE_SIZE - (pbuf - buf));
		}
	}
	vdec_core_unlock(vdec_core, flags);

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

static ssize_t version_show(struct class *class,
			struct class_attribute *attr, char *buf)
{
	char *pbuf = buf;
#ifdef DECODER_VERSION
	pbuf += sprintf(pbuf, "DECODER VERSION: V" xstr(DECODER_VERSION) "\n");
#else
#ifdef RELEASED_VERSION
		pbuf += sprintf(pbuf, "Due to project compilation environment problems,\
the current decoder version could not be detected,\
Please Use The DECODER BASE Version for traceability\n");
		pbuf += sprintf(pbuf, "DECODER BASE Version: " xstr(RELEASED_VERSION) "\n");
#endif
#endif

#ifdef UCODE_VERSION
		pbuf += sprintf(pbuf, "UCODE VERSION: V" xstr(UCODE_VERSION) "\n");
#endif
	return pbuf - buf;
}

static char * parser_h264_profile(char *pbuf, struct vdec_s *vdec)
{
	switch (vdec->profile_idc) {
		case 66:
			pbuf += sprintf(pbuf, "%d: Baseline Profile(%u)\n",
			vdec->id, vdec->profile_idc);
			break;
		case 77:
			pbuf += sprintf(pbuf, "%d: Main Profile(%u)\n",
			vdec->id, vdec->profile_idc);
			break;
		case 88:
			pbuf += sprintf(pbuf, "%d: Extended Profile(%u)\n",
			vdec->id, vdec->profile_idc);
			break;
		case 100:
			pbuf += sprintf(pbuf, "%d: High Profile(%u)\n",
			vdec->id, vdec->profile_idc);
			break;
		case 110:
			pbuf += sprintf(pbuf, "%d: High 10 Profile(%u)\n",
			vdec->id, vdec->profile_idc);
			break;
		default:
			pbuf += sprintf(pbuf, "%d: Not Support Profile(%u)\n",
			vdec->id, vdec->profile_idc);
			break;
	}

	return pbuf;
}

static char * parser_mpeg2_profile(char *pbuf, struct vdec_s *vdec)
{
	switch (vdec->profile_idc) {
		case 5:
			pbuf += sprintf(pbuf, "%d: Simple Profile(%u)\n",
			vdec->id, vdec->profile_idc);
			break;
		case 4:
			pbuf += sprintf(pbuf, "%d: Main Profile(%u)\n",
			vdec->id, vdec->profile_idc);
			break;
		case 3:
			pbuf += sprintf(pbuf, "%d: SNR Scalable Profile(%u)\n",
			vdec->id, vdec->profile_idc);
			break;
		case 2:
			pbuf += sprintf(pbuf, "%d: Airspace Profile(%u)\n",
			vdec->id, vdec->profile_idc);
			break;
		case 1:
			pbuf += sprintf(pbuf, "%d: High Profile(%u)\n",
			vdec->id, vdec->profile_idc);
			break;
		default:
			pbuf += sprintf(pbuf, "%d: Not Support Profile(%u)\n",
			vdec->id, vdec->profile_idc);
			break;
	}
	return pbuf;
}

static char * parser_mpeg4_profile(char *pbuf, struct vdec_s *vdec)
{
	switch (vdec->profile_idc) {
		case 0:
			pbuf += sprintf(pbuf, "%d: Simple Profile(%u)\n",
			vdec->id, vdec->profile_idc);
			break;
		case 1:
			pbuf += sprintf(pbuf, "%d: Simple Scalable Profile(%u)\n",
			vdec->id, vdec->profile_idc);
			break;
		case 2:
			pbuf += sprintf(pbuf, "%d: Core Profile(%u)\n",
			vdec->id, vdec->profile_idc);
			break;
		case 3:
			pbuf += sprintf(pbuf, "%d: Main Profile(%u)\n",
			vdec->id, vdec->profile_idc);
			break;
		case 4:
			pbuf += sprintf(pbuf, "%d: N-bit Profile(%u)\n",
			vdec->id, vdec->profile_idc);
			break;
		case 5:
			pbuf += sprintf(pbuf, "%d: Scalable Texture Profile(%u)\n",
			vdec->id, vdec->profile_idc);
			break;
		case 6:
			if (vdec->profile_idc == 1 || vdec->profile_idc == 2)
				pbuf += sprintf(pbuf, "%d: Simple Face Animation Profile(%u)\n",
				vdec->id, vdec->profile_idc);
			else
				pbuf += sprintf(pbuf, "%d: Simple FBA Profile(%u)\n",
				vdec->id, vdec->profile_idc);
			break;
		case 7:
			pbuf += sprintf(pbuf, "%d: Basic Animated Texture Profile(%u)\n",
			vdec->id, vdec->profile_idc);
			break;
		case 8:
			pbuf += sprintf(pbuf, "%d: Hybrid Profile(%u)\n",
			vdec->id, vdec->profile_idc);
			break;
		case 9:
			pbuf += sprintf(pbuf, "%d: Advanced Real Time Simple Profile(%u)\n",
			vdec->id, vdec->profile_idc);
			break;
		case 10:
			pbuf += sprintf(pbuf, "%d: Core Scalable Profile(%u)\n",
			vdec->id, vdec->profile_idc);
			break;
		case 11:
			pbuf += sprintf(pbuf, "%d: Advanced Coding Efficiency Profile(%u)\n",
			vdec->id, vdec->profile_idc);
			break;
		case 12:
			pbuf += sprintf(pbuf, "%d: Advanced Core Profile(%u)\n",
			vdec->id, vdec->profile_idc);
			break;
		case 13:
			pbuf += sprintf(pbuf, "%d: Advanced Scalable Texture Profile(%u)\n",
			vdec->id, vdec->profile_idc);
			break;
		case 14:
		case 15:
			pbuf += sprintf(pbuf, "%d: Advanced Simple Profile(%u)\n",
			vdec->id, vdec->profile_idc);
			break;
		default:
			pbuf += sprintf(pbuf, "%d: Not Support Profile(%u)\n",
			vdec->id, vdec->profile_idc);
			break;
	}

	return pbuf;
}

static ssize_t profile_idc_show(struct class *class, struct class_attribute *attr,
			char *buf)
{
	struct vdec_core_s *core = vdec_core;
	char *pbuf = buf;
	unsigned long flags = vdec_core_lock(vdec_core);

	if (list_empty(&core->connected_vdec_list))
		pbuf += sprintf(pbuf, "connected vdec list empty\n");
	else {
		struct vdec_s *vdec;
		list_for_each_entry(vdec, &core->connected_vdec_list, list) {
			if (vdec->format == 0) {
				pbuf = parser_mpeg2_profile(pbuf, vdec);
			} else if (vdec->format == 1) {
				pbuf = parser_mpeg4_profile(pbuf, vdec);
			} else if (vdec->format == 2) {
				pbuf = parser_h264_profile(pbuf, vdec);
			} else {
				pbuf += sprintf(pbuf,
				"%d: Not Support\n", vdec->id);
			}
		}
	}

	vdec_core_unlock(vdec_core, flags);
	return pbuf - buf;
}

static char * parser_h264_level(char *pbuf, struct vdec_s *vdec)
{

	pbuf += sprintf(pbuf, "%d: Level %d.%d(%u)\n",
			vdec->id, vdec->level_idc/10, vdec->level_idc%10, vdec->level_idc);

	return pbuf;
}

static char * parser_mpeg2_level(char *pbuf, struct vdec_s *vdec)
{
	switch (vdec->level_idc) {
		case 10:
			pbuf += sprintf(pbuf, "%d: Low Level(%u)\n",
			vdec->id, vdec->level_idc);
			break;
		case 8:
			pbuf += sprintf(pbuf, "%d: Main Level(%u)\n",
			vdec->id, vdec->level_idc);
			break;
		case 6:
			pbuf += sprintf(pbuf, "%d: High 1440 Level(%u)\n",
			vdec->id, vdec->level_idc);
			break;
		case 4:
			pbuf += sprintf(pbuf, "%d: High Level(%u)\n",
			vdec->id, vdec->level_idc);
			break;
		default:
			pbuf += sprintf(pbuf, "%d: Not Support Level(%u)\n",
			vdec->id, vdec->level_idc);
			break;
	}

	return pbuf;
}

static char * parser_mpeg4_level(char *pbuf, struct vdec_s *vdec)
{
	switch (vdec->level_idc) {
		case 1:
			pbuf += sprintf(pbuf, "%d: Level 1(%u)\n",
			vdec->id, vdec->level_idc);
			break;
		case 2:
			pbuf += sprintf(pbuf, "%d: Level 2(%u)\n",
			vdec->id, vdec->level_idc);
			break;
		case 3:
			pbuf += sprintf(pbuf, "%d: Level 3(%u)\n",
			vdec->id, vdec->level_idc);
			break;
		case 4:
			pbuf += sprintf(pbuf, "%d: Level 4(%u)\n",
			vdec->id, vdec->level_idc);
			break;
		case 5:
			pbuf += sprintf(pbuf, "%d: Level 5(%u)\n",
			vdec->id, vdec->level_idc);
			break;
		default:
			pbuf += sprintf(pbuf, "%d: Not Support Level(%u)\n",
			vdec->id, vdec->level_idc);
			break;
	}

	return pbuf;
}

static ssize_t level_idc_show(struct class *class, struct class_attribute *attr,
			char *buf)
{
	struct vdec_core_s *core = vdec_core;
	char *pbuf = buf;
	unsigned long flags = vdec_core_lock(vdec_core);

	if (list_empty(&core->connected_vdec_list))
		pbuf += sprintf(pbuf, "connected vdec list empty\n");
	else {
		struct vdec_s *vdec;
		list_for_each_entry(vdec, &core->connected_vdec_list, list) {
			if (vdec->format == 0) {
				pbuf = parser_mpeg2_level(pbuf, vdec);
			} else if (vdec->format == 1) {
				pbuf = parser_mpeg4_level(pbuf, vdec);
			} else if (vdec->format == 2) {
				pbuf = parser_h264_level(pbuf, vdec);
			} else {
				pbuf += sprintf(pbuf,
				"%d: Not Support\n", vdec->id);
			}
		}
	}

	vdec_core_unlock(vdec_core, flags);
	return pbuf - buf;
}

static CLASS_ATTR_RO(amrisc_regs);
static CLASS_ATTR_RO(dump_trace);
static CLASS_ATTR_RO(clock_level);
static CLASS_ATTR_RW(poweron_clock_level);
static CLASS_ATTR_RW(dump_risc_mem);
static CLASS_ATTR_RW(keep_vdec_mem);
static CLASS_ATTR_RW(enable_mvdec_info);
static CLASS_ATTR_RO(core);
static CLASS_ATTR_RO(vdec_status);
static CLASS_ATTR_RO(dump_vdec_blocks);
static CLASS_ATTR_RO(dump_vdec_chunks);
static CLASS_ATTR_RO(dump_decoder_state);
#ifdef VDEC_DEBUG_SUPPORT
static CLASS_ATTR_RW(debug);
#endif
#ifdef PXP_DEBUG
static CLASS_ATTR_RW(pxp_buf);
#endif
static CLASS_ATTR_RW(vdec_vfm_path);
#ifdef FRAME_CHECK
static CLASS_ATTR_RW(dump_yuv);
static CLASS_ATTR_RW(frame_check);
#endif
static CLASS_ATTR_RO(dump_fps);
static CLASS_ATTR_RO(profile_idc);
static CLASS_ATTR_RO(level_idc);
static CLASS_ATTR_RO(version);

static struct attribute *vdec_class_attrs[] = {
	&class_attr_amrisc_regs.attr,
	&class_attr_dump_trace.attr,
	&class_attr_clock_level.attr,
	&class_attr_poweron_clock_level.attr,
	&class_attr_dump_risc_mem.attr,
	&class_attr_keep_vdec_mem.attr,
	&class_attr_enable_mvdec_info.attr,
	&class_attr_core.attr,
	&class_attr_vdec_status.attr,
	&class_attr_dump_vdec_blocks.attr,
	&class_attr_dump_vdec_chunks.attr,
	&class_attr_dump_decoder_state.attr,
#ifdef VDEC_DEBUG_SUPPORT
	&class_attr_debug.attr,
#endif
#ifdef PXP_DEBUG
	&class_attr_pxp_buf.attr,
#endif
	&class_attr_vdec_vfm_path.attr,
#ifdef FRAME_CHECK
	&class_attr_dump_yuv.attr,
	&class_attr_frame_check.attr,
#endif
	&class_attr_dump_fps.attr,
	&class_attr_profile_idc.attr,
	&class_attr_level_idc.attr,
	&class_attr_version.attr,
	NULL
};

ATTRIBUTE_GROUPS(vdec_class);

static struct class vdec_class = {
	.name = "vdec",
	.class_groups = vdec_class_groups,
};

struct device *get_vdec_device(void)
{
	return &vdec_core->vdec_core_platform_device->dev;
}
EXPORT_SYMBOL(get_vdec_device);

static int vdec_post_task_recycle(void *args)
{
	struct post_task_mgr_s *post =
		(struct post_task_mgr_s *)args;

	while (post->running &&
		down_interruptible(&post->sem) == 0) {
		if (kthread_should_stop())
			break;
		mutex_lock(&post->mutex);
		if (!list_empty(&post->task_recycle)) {
			struct vdec_post_task_parms_s *parms, *tmp;
			list_for_each_entry_safe(parms, tmp, &post->task_recycle, recycle) {
				if (parms->scheduled) {
					list_del(&parms->recycle);
					kthread_stop(parms->task);
					kfree(parms);
					parms = NULL;
				}
			}
		}
		mutex_unlock(&post->mutex);
	}

	return 0;
}

static void vdec_post_task_exit(void)
{
	struct post_task_mgr_s *post = &vdec_core->post;

	post->running = false;
	up(&post->sem);

	kthread_stop(post->task);
}

static int vdec_post_task_init(void)
{
	struct post_task_mgr_s *post = &vdec_core->post;

	sema_init(&post->sem, 0);
	INIT_LIST_HEAD(&post->task_recycle);
	mutex_init(&post->mutex);
	post->running = true;

	post->task = kthread_run(vdec_post_task_recycle,
		post, "task-post-daemon-thread");
	if (IS_ERR(post->task)) {
		pr_err("%s, creat task post daemon thread faild %ld\n",
			__func__, PTR_ERR(post->task));
		return PTR_ERR(post->task);
	}

	return 0;
}

static int vdec_post_handler(void *args)
{
	struct vdec_post_task_parms_s *parms =
		(struct vdec_post_task_parms_s *) args;
	struct post_task_mgr_s *post = &vdec_core->post;

	complete(&parms->park);

	/* process client task. */
	parms->func(parms->private);
	parms->scheduled = 1;
	up(&post->sem);

	while (!kthread_should_stop()) {
		set_current_state(TASK_INTERRUPTIBLE);
		usleep_range(1000, 2000);
	}

	return 0;
}

int vdec_post_task(post_task_handler func, void *args)
{
	struct vdec_post_task_parms_s *parms;
	struct post_task_mgr_s *post = &vdec_core->post;

	parms = kzalloc(sizeof(*parms), GFP_KERNEL);
	if (parms == NULL)
		return -ENOMEM;

	parms->func	= func;
	parms->private	= args;
	init_completion(&parms->park);
	parms->scheduled = 0;
	parms->task	= kthread_run(vdec_post_handler,
				parms, "task-post-thread");
	if (IS_ERR(parms->task)) {
		pr_err("%s, creat task post thread faild %ld\n",
			__func__, PTR_ERR(parms->task));
		kfree(parms);
		return PTR_ERR(parms->task);
	}
	if (!__kthread_should_park(parms->task))
		wait_for_completion(&parms->park);

	mutex_lock(&post->mutex);
	/* add to list for resource recycle in post daemon kthread */
	list_add_tail(&parms->recycle, &post->task_recycle);
	mutex_unlock(&post->mutex);

	return 0;
}
EXPORT_SYMBOL(vdec_post_task);



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
	spin_lock_init(&vdec_core->input_lock);
	ida_init(&vdec_core->ida);
	vdec_core->thread = kthread_run(vdec_core_thread, vdec_core,
					"vdec-core");

	vdec_core->vdec_core_wq = alloc_ordered_workqueue("%s",__WQ_LEGACY |
		WQ_MEM_RECLAIM |WQ_HIGHPRI/*high priority*/, "vdec-work");
	/*work queue priority lower than vdec-core.*/

	vdec_post_task_init();

	vdec_data_core_init();

	/* power manager init. */
	vdec_core->pm = (struct power_manager_s *)
		of_device_get_match_data(&pdev->dev);
	if (vdec_core->pm->init) {
		r = vdec_core->pm->init(&pdev->dev);
		if (r) {
			pr_err("vdec power manager init failed\n");
			return r;
		}
		pr_err("vdec power init success!\n");
	}

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

	vdec_post_task_exit();

	kthread_stop(vdec_core->thread);

	destroy_workqueue(vdec_core->vdec_core_wq);

	if (vdec_core->pm->release)
		vdec_core->pm->release(&pdev->dev);

	class_unregister(&vdec_class);

	return 0;
}

static struct mconfig vdec_configs[] = {
	MC_PU32("debug_trace_num", &debug_trace_num),
	MC_PI32("hevc_max_reset_count", &hevc_max_reset_count),
	MC_PU32("clk_config", &clk_config),
	MC_PI32("step_mode", &step_mode),
	MC_PI32("poweron_clock_level", &poweron_clock_level),
};
static struct mconfig_node vdec_node;

extern const struct of_device_id amlogic_vdec_matches[];

static struct platform_driver vdec_driver = {
	.probe = vdec_probe,
	.remove = vdec_remove,
	.driver = {
		.name = "vdec",
		.of_match_table = amlogic_vdec_matches,
	}
};

static struct codec_profile_t amvdec_common_profile = {
	.name = "vdec_common",
	.profile = "vdec"
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
	vcodec_profile_register(&amvdec_common_profile);

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


void vdec_set_vframe_comm(struct vdec_s *vdec, char *n)
{
	struct vdec_frames_s *mvfrm = vdec->mvfrm;

	if (!mvfrm)
		return;

	mvfrm->comm.vdec_id = vdec->id;

	snprintf(mvfrm->comm.vdec_name, sizeof(mvfrm->comm.vdec_name)-1,
		"%s", n);
	mvfrm->comm.vdec_type = vdec->type;
}
EXPORT_SYMBOL(vdec_set_vframe_comm);

u32 diff_pts(u32 a, u32 b)
{
	if (!a || !b)
		return 0;
	else
		return abs(a - b);
}

/*
 * We only use the first 5 frames to calc duration.
 * The fifo[0]~fifo[4] means the frame 0 to frame 4.
 * we start to calculate the duration from frame 1.
 * And the caller guarantees that slot > 0.
 */
static void cal_dur_from_pts(struct vdec_s *vdec, u32 slot)
{
#define DURATION_THRESHOD 10
	static u32 must_send = 0, ready = 0;
	u32 old = 0, cur, diff;
	struct vframe_counter_s *fifo = vdec->mvfrm->fifo_buf;

	if (vdec->mvfrm->wr == 1) {
		ready = 0;
		must_send = 0;
	}

	if (must_send == 2)
		return ;

	if (ready)
		++must_send;

	if ((vdec->format != VFORMAT_H264 && vdec->format != VFORMAT_HEVC) ||
	    !fifo[slot].pts) {
		if (fifo[slot].frame_dur != ready) {
			if (must_send)
				ready = (ready + fifo[slot].frame_dur) / 2;
			else
				ready = fifo[slot].frame_dur;
			pr_debug("%s inner driver dur%u \n",__func__, ready);
		}
		goto end_handle;
	}

	if (slot == 1) {
		cur = diff_pts(fifo[1].pts, fifo[0].pts);
	} else {
		old = diff_pts(fifo[slot - 1].pts, fifo[slot - 2].pts);
		cur = diff_pts(fifo[slot].pts, fifo[slot - 1].pts);
	}

	diff = abs(cur - old);
	if (diff > DURATION_THRESHOD) {
		u32 dur, cur2;

		cur2 = (cur << 4) / 15;
		diff = abs(cur2 - fifo[slot].frame_dur);
		if (fifo[slot].frame_dur == 3600)
			dur = cur2;
		else if (diff < DURATION_THRESHOD || diff > fifo[slot].frame_dur)
			dur = fifo[slot].frame_dur;
		else
			dur = cur2;

		if (ready == dur)
			goto end_handle;

		if (must_send)
			ready = (ready + dur) / 2;
		else
			ready = dur;
		pr_debug("%s vstatus %u dur%u -> %u, revised %u\n",__func__,fifo[slot].frame_dur, cur,cur2, dur);
		if (diff > 10 && slot >= 2)
			pr_debug("wr=%u,slot=%u pts %u, %u, %u\n",vdec->mvfrm->wr,slot,
				fifo[slot].pts, fifo[slot-1].pts,fifo[slot-2].pts);
	}

end_handle:
	if (must_send) {
		++must_send;
		vdec_frame_rate_uevent(ready);
	}
}

void vdec_fill_vdec_frame(struct vdec_s *vdec, struct vframe_qos_s *vframe_qos,
				struct vdec_info *vinfo,struct vframe_s *vf,
				u32 hw_dec_time)
{
#define MINIMUM_FRAMES 5
	u32 i;
	struct vframe_counter_s *fifo_buf;
	struct vdec_frames_s *mvfrm = vdec->mvfrm;

	if (!mvfrm)
		return;
	fifo_buf = mvfrm->fifo_buf;

	/* assume fps==60,mv->wr max value can support system running 828 days,
	 this is enough for us */
	i = mvfrm->wr & (NUM_FRAME_VDEC-1); //find the slot num in fifo_buf
	mvfrm->fifo_buf[i].decode_time_cost = hw_dec_time;
	if (vframe_qos)
		memcpy(&fifo_buf[i].qos, vframe_qos, sizeof(struct vframe_qos_s));
	if (vinfo) {
		memcpy(&fifo_buf[i].frame_width, &vinfo->frame_width,
		        ((char*)&vinfo->reserved[0] - (char*)&vinfo->frame_width));
		/*copy for ipb report*/
		memcpy(&fifo_buf[i].i_decoded_frames, &vinfo->i_decoded_frames,
		        ((char*)&vinfo->endipb_line[0] - (char*)&vinfo->i_decoded_frames));
		fifo_buf[i].av_resynch_counter = timestamp_avsync_counter_get();
	}
	if (vf) {
		fifo_buf[i].vf_type = vf->type;
		fifo_buf[i].signal_type = vf->signal_type;
		fifo_buf[i].pts = vf->pts;
		fifo_buf[i].pts_us64 = vf->pts_us64;

		/* Calculate the duration from pts */
		if (!vdec->is_v4l && (mvfrm->wr < MINIMUM_FRAMES && mvfrm->wr > 0))
			cal_dur_from_pts(vdec, i);
	}
	mvfrm->wr++;
}
EXPORT_SYMBOL(vdec_fill_vdec_frame);

void vdec_vframe_ready(struct vdec_s *vdec, struct vframe_s *vf) {
	if (vdec_secure(vdec)) {
		vf->flag |= VFRAME_FLAG_VIDEO_SECURE;
	} else {
		vf->flag &= ~VFRAME_FLAG_VIDEO_SECURE;
	}
}
EXPORT_SYMBOL(vdec_vframe_ready);

void set_meta_data_to_vf(struct vframe_s *vf, u32 type, void *v4l2_ctx)
{
	struct aml_vcodec_ctx *ctx =
			(struct aml_vcodec_ctx *)(v4l2_ctx);
	struct aml_meta_head_s		head;
	struct aml_vf_base_info_s	vfb_infos;

	if ((ctx == NULL) || (vf == NULL))
		return ;

	if (vf->meta_data_buf == NULL) {
		vf->meta_data_buf = ctx->meta_infos.meta_bufs[ctx->meta_infos.index].buf;
		ctx->meta_infos.index = (ctx->meta_infos.index + 1) % V4L_CAP_BUFF_MAX;
	}

	switch (type) {
	case UVM_META_DATA_VF_BASE_INFOS:
		if ((vf->meta_data_size + sizeof(struct aml_vf_base_info_s) + AML_META_HEAD_SIZE) <= META_DATA_SIZE) {
			head.magic = META_DATA_MAGIC;
			head.type = UVM_META_DATA_VF_BASE_INFOS;
			head.data_size = sizeof(struct aml_vf_base_info_s);

			memcpy(vf->meta_data_buf + vf->meta_data_size,
				&head, AML_META_HEAD_SIZE);
			vf->meta_data_size += AML_META_HEAD_SIZE;

			vfb_infos.width = vf->width;
			vfb_infos.height = vf->height;
			vfb_infos.duration = vf->duration;
			vfb_infos.frame_type = vf->frame_type;
			vfb_infos.type = vf->type;

			memcpy(vf->meta_data_buf + vf->meta_data_size,
				&vfb_infos, sizeof(struct aml_vf_base_info_s));
			vf->meta_data_size += sizeof(struct aml_vf_base_info_s);

			if (debug_meta) {
				pr_debug("vf->meta_data_size = %d\n", vf->meta_data_size);
				pr_debug("vf:width:%d height:%d duration:%d frame_type:%d type:%d\n",
					vfb_infos.width, vfb_infos.height, vfb_infos.duration,
					vfb_infos.frame_type, vfb_infos.type);
			}
		}
		break;
	case UVM_META_DATA_HDR10P_DATA:
		if ((vf->meta_data_size + vf->hdr10p_data_size + AML_META_HEAD_SIZE) <= META_DATA_SIZE) {
			head.magic = META_DATA_MAGIC;
			head.type = UVM_META_DATA_HDR10P_DATA;
			head.data_size = vf->hdr10p_data_size;

			memcpy(vf->meta_data_buf + vf->meta_data_size,
				&head, AML_META_HEAD_SIZE);
			vf->meta_data_size += AML_META_HEAD_SIZE;

			memcpy(vf->meta_data_buf + vf->meta_data_size,
				vf->hdr10p_data_buf, vf->hdr10p_data_size);
			vf->meta_data_size += vf->hdr10p_data_size;

			if (debug_meta) {
				pr_debug("vf->meta_data_size = %d\n", vf->meta_data_size);
				pr_debug("vf->hdr10p_data_size = %d\n", vf->hdr10p_data_size);
			}
		}
		break;
	default:
		break;
	}
}
EXPORT_SYMBOL(set_meta_data_to_vf);

/* In this function,if we use copy_to_user, we may encounter sleep,
which may block the vdec_fill_vdec_frame,this is not acceptable.
So, we should use a tmp buffer(passed by caller) to get the content */
u32  vdec_get_frame_vdec(struct vdec_s *vdec,  struct vframe_counter_s *tmpbuf)
{
	u32 toread = 0;
	u32 slot_rd;
	struct vframe_counter_s *fifo_buf = NULL;
	struct vdec_frames_s *mvfrm = NULL;

	/*
	switch (version) {
	case version_1:
		f1();
	case version_2:
		f2();
	default:
		break;
	}
	*/

	if (!vdec)
		return 0;
	mvfrm = vdec->mvfrm;
	if (!mvfrm)
		return 0;

	fifo_buf = &mvfrm->fifo_buf[0];

	toread = mvfrm->wr - mvfrm->rd;
	if (toread) {
		if (toread >= NUM_FRAME_VDEC - QOS_FRAME_NUM) {
			/* round the fifo_buf length happens, give QOS_FRAME_NUM for buffer */
			mvfrm->rd = mvfrm->wr - (NUM_FRAME_VDEC - QOS_FRAME_NUM);
		}

		if (toread >= QOS_FRAME_NUM) {
			toread = QOS_FRAME_NUM; //by default, we use this num
		}

		slot_rd = mvfrm->rd &( NUM_FRAME_VDEC-1); //In this case it equals to x%y
		if (slot_rd + toread <= NUM_FRAME_VDEC) {
			memcpy(tmpbuf, &fifo_buf[slot_rd], toread*sizeof(struct vframe_counter_s));
		} else {
			u32 exeed;
			exeed = slot_rd + toread - NUM_FRAME_VDEC;
			memcpy(tmpbuf, &fifo_buf[slot_rd], (NUM_FRAME_VDEC - slot_rd)*sizeof(struct vframe_counter_s));
			memcpy(&tmpbuf[NUM_FRAME_VDEC-slot_rd], &fifo_buf[0], exeed*sizeof(struct vframe_counter_s));
		}

		mvfrm->rd += toread;
	}
	return toread;
}
EXPORT_SYMBOL(vdec_get_frame_vdec);

int get_double_write_ratio(int dw_mode)
{
	int ratio = 1;

	if ((dw_mode == 2) ||
			(dw_mode == 3))
		ratio = 4;
	else if ((dw_mode == 4) ||
				(dw_mode == 5))
		ratio = 2;
	else if ((dw_mode == 8) ||
		(dw_mode == 9))
		ratio = 8;
	return ratio;
}
EXPORT_SYMBOL(get_double_write_ratio);

void vdec_set_vld_wp(struct vdec_s *vdec, u32 wp)
{
	if (vdec_single(vdec)) {
		WRITE_VREG(VLD_MEM_VIFIFO_WP, wp);
	}
}
EXPORT_SYMBOL(vdec_set_vld_wp);

void vdec_config_vld_reg(struct vdec_s *vdec, u32 addr, u32 size)
{
	if (vdec_single(vdec)) {
		WRITE_VREG(VLD_MEM_VIFIFO_CONTROL, 0);
		/* reset VLD before setting all pointers */
		WRITE_VREG(VLD_MEM_VIFIFO_WRAP_COUNT, 0);
		/*TODO: only > m6*/
		WRITE_VREG(DOS_SW_RESET0, (1 << 4));
		WRITE_VREG(DOS_SW_RESET0, 0);


		WRITE_VREG(POWER_CTL_VLD, 1 << 4);

		WRITE_VREG(VLD_MEM_VIFIFO_START_PTR,
		       addr);
		WRITE_VREG(VLD_MEM_VIFIFO_END_PTR,
		       addr + size - 8);
		WRITE_VREG(VLD_MEM_VIFIFO_CURR_PTR,
		       addr);

		WRITE_VREG(VLD_MEM_VIFIFO_CONTROL, 1);
		WRITE_VREG(VLD_MEM_VIFIFO_CONTROL, 0);

		/* set to manual mode */
		WRITE_VREG(VLD_MEM_VIFIFO_BUF_CNTL, 2);
		WRITE_VREG(VLD_MEM_VIFIFO_WP, addr);

		WRITE_VREG(VLD_MEM_VIFIFO_BUF_CNTL, 3);
		WRITE_VREG(VLD_MEM_VIFIFO_BUF_CNTL, 2);

		/* enable */
		WRITE_VREG(VLD_MEM_VIFIFO_CONTROL,
			(0x11 << 16) | (1<<10) | (1 << 1) | (1 << 2));
		SET_VREG_MASK(VLD_MEM_VIFIFO_CONTROL,
			7 << 3);
	}
}
EXPORT_SYMBOL(vdec_config_vld_reg);

static void check_rdma_result(int num)
{
	int i, wr,rd,data;
	int flag = 1;
	for (i = 0; i < num; i++) {
		wr = READ_VREG(HEVC_IQIT_SCALELUT_WR_ADDR);
		rd = READ_VREG(HEVC_IQIT_SCALELUT_RD_ADDR);
		data = READ_VREG(HEVC_IQIT_SCALELUT_DATA);
		if (wr != (num & 0x3ff)) {
			pr_info("--->HEVC_IQIT_SCALELUT_WR_ADDR = 0x%x\n", wr);
			flag = 0;
			break;
		}
		if (rd != i) {
			pr_info("--->HEVC_IQIT_SCALELUT_RD_ADDR = 0x%x\n", rd);
			flag = 0;
			break;
		}

		if (data != 0) {
			pr_info("--->HEVC_IQIT_SCALELUT_DATA = 0x%x\n", data);
			flag = 0;
			break;
		}
	}
	if (flag == 0)
		pr_info("-->%d--rdma flail\n", i);
	else
		pr_info("rdma ok\n");
	return;

}

int is_rdma_enable(void)
{
	if (rdma_mode && (get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_T3))
		return 1;
	else
		return 0;
}
EXPORT_SYMBOL(is_rdma_enable);

void rdma_front_end_wrok(dma_addr_t ddr_phy_addr, u32 size)
{
	ulong expires;

	WRITE_VREG(HEVC_RDMA_F_CTRL,
		(0X0 << 7) | //axi id
		(0X7 << 3) | //axi length
		(0X0 << 2) | //rdma force cg en
		(0X1 << 1)); //rdma path en
	WRITE_VREG(HEVC_RDMA_F_START_ADDR, ddr_phy_addr); //rdma start address
	WRITE_VREG(HEVC_RDMA_F_END_ADDR,   ddr_phy_addr + 0xff); //rdma end address
	WRITE_VREG(HEVC_RDMA_F_STATUS0,    0X1); //trigger rdma start to work

	expires = jiffies + msecs_to_jiffies(2000);
	while (1) {
		if ((READ_VREG(HEVC_RDMA_F_STATUS0) & 0x1) == 0) {
			//pr_info("rdma front_end done\n");
			break;
		}
		if (time_after(jiffies, expires)) {
			pr_info("wait rdma done timeout\n");
			break;
		}
	}

	if (rdma_mode & 0x2)
		check_rdma_result(SCALELUT_DATA_WRITE_NUM);

	return;
}
EXPORT_SYMBOL(rdma_front_end_wrok);

void rdma_back_end_work(dma_addr_t back_ddr_phy_addr, u32 size)
{
	ulong expires;

	WRITE_VREG(HEVC_RDMA_B_CTRL,
		(0X0 << 7) | //axi id
		(0X7 << 3) | //axi length
		(0X0 << 2) | //rdma force cg en
		(0X1 << 1)); //rdma path en
	WRITE_VREG(HEVC_RDMA_B_START_ADDR, back_ddr_phy_addr); //rdma start address
	WRITE_VREG(HEVC_RDMA_B_END_ADDR,   back_ddr_phy_addr + size -1); //rdma end address
	WRITE_VREG(HEVC_RDMA_B_STATUS0,    0X1); //trigger rdma start to work

	expires = jiffies + msecs_to_jiffies(2000);
	while (1) {
		if ((READ_VREG(HEVC_RDMA_B_STATUS0) & 0x1) == 0) {
			//pr_info("rdma back_end done\n");
			break;
		}
		if (time_after(jiffies, expires)) {
			pr_info("wait rdma done timeout\n");
			break;
		}
	}
	if (rdma_mode & 0x2)
		check_rdma_result(SCALELUT_DATA_WRITE_NUM);

	return;
}
EXPORT_SYMBOL(rdma_back_end_work);

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
module_param(force_nosecure_even_drm, int, 0664);
module_param(disable_switch_single_to_mult, int, 0664);

module_param(debug_meta, uint, 0664);

module_param(frameinfo_flag, int, 0664);
MODULE_PARM_DESC(frameinfo_flag,
				"\n frameinfo_flag\n");
module_param(v4lvideo_add_di, int, 0664);
MODULE_PARM_DESC(v4lvideo_add_di,
				"\n v4lvideo_add_di\n");

module_param(v4lvideo_add_ppmgr, int, 0664);
MODULE_PARM_DESC(v4lvideo_add_ppmgr,
				"\n v4lvideo_add_ppmgr\n");

module_param(max_di_instance, int, 0664);
MODULE_PARM_DESC(max_di_instance,
				"\n max_di_instance\n");

module_param(max_supported_di_instance, int, 0664);
MODULE_PARM_DESC(max_supported_di_instance,
				"\n max_supported_di_instance\n");
module_param(debug_vdetect, int, 0664);
MODULE_PARM_DESC(debug_vdetect, "\n debug_vdetect\n");

module_param(enable_stream_mode_multi_dec, int, 0664);
MODULE_PARM_DESC(enable_stream_mode_multi_dec,
	"\n enable multi-decoding on stream mode. \n");

module_param(rdma_mode, int, 0664);
MODULE_PARM_DESC(rdma_mode, "\n rdma_enable\n");

/*
*module_init(vdec_module_init);
*module_exit(vdec_module_exit);
*/
#define CREATE_TRACE_POINTS
#include "vdec_trace.h"
MODULE_DESCRIPTION("AMLOGIC vdec driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tim Yao <timyao@amlogic.com>");
