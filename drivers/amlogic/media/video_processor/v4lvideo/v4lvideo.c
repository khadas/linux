// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * drivers/amlogic/media/video_processor/v4lvideo/v4lvideo.c
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

#undef DEBUG
#define DEBUG
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>
/* media module used media/registers/cpu_version.h since kernel 5.4 */
#include <linux/amlogic/media/registers/cpu_version.h>
#include "v4lvideo.h"
#include <media/videobuf2-core.h>
#include <media/videobuf2-v4l2.h>
#include <linux/platform_device.h>
#include <linux/amlogic/major.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/amlogic/media/codec_mm/codec_mm_keeper.h>
#include <linux/amlogic/media/utils/amstream.h>
#include <linux/amlogic/media/video_sink/video_keeper.h>
#include <linux/anon_inodes.h>
#include <linux/file.h>
#include <linux/amlogic/media/canvas/canvas_mgr.h>
#include <linux/amlogic/media/vfm/amlogic_fbc_hook_v1.h>
#include <linux/amlogic/media/di/di.h>
#include "../../common/vfm/vfm.h"
#include <linux/amlogic/media/utils/am_com.h>
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
#include <linux/amlogic/media/amdolbyvision/dolby_vision.h>
#endif
#include <linux/amlogic/meson_uvm_core.h>

#define V4LVIDEO_MODULE_NAME "v4lvideo"

#define V4LVIDEO_VERSION "1.0"
#define RECEIVER_NAME "v4lvideo"
#define V4LVIDEO_DEVICE_NAME   "v4lvideo"
//#define DUR2PTS(x) ((x) - ((x) >> 4))

static atomic_t global_set_cnt = ATOMIC_INIT(0);
static u32 alloc_sei = 1;

#define V4L2_CID_USER_AMLOGIC_V4LVIDEO_BASE  (V4L2_CID_USER_BASE + 0x1100)

static unsigned int video_nr_base = 30;
static unsigned int n_devs = 9;
#define N_DEVS 9
static unsigned int debug;
static unsigned int get_count[N_DEVS];
static unsigned int put_count[N_DEVS];
static unsigned int q_count[N_DEVS];
static unsigned int dq_count[N_DEVS];
static unsigned int cts_use_di;
/*set 1 means video_composer use dec vf when di NR; only debug!!!*/
static unsigned int render_use_dec;
static unsigned int dec_count;
static unsigned int vf_dump;
static unsigned int open_fd_count;
static unsigned int release_fd_count;
static unsigned int link_fd_count;
static unsigned int link_put_fd_count;
static unsigned int v4lvideo_version = 2;
static unsigned int total_get_count[N_DEVS];
static unsigned int total_put_count[N_DEVS];
static unsigned int total_release_count[N_DEVS];
static unsigned int inactive_check_disp;

bool di_bypass_p;
/*dec set count mutex*/
struct mutex mutex_dec_count;

#define MAX_KEEP_FRAME 64
#define DI_NR_COUNT 1

struct keep_mem_info {
	void *handle;
	int keep_id;
	int count;
};

struct keeper_mgr {
	/* lock*/
	spinlock_t lock;
	struct keep_mem_info keep_list[MAX_KEEP_FRAME];
};

static struct keeper_mgr keeper_mgr_private;

static const struct file_operations v4lvideo_file_fops;

static u32 print_flag;
static u32 cts_video_flag;

const struct video_info_t cts_vp9_videos[] = {
	{
		.width = 426,
		.height = 240,
	},
	{
		.width = 2560,
		.height = 1090,
	},
	{
		.width = 1216,
		.height = 2160,
	},
};

const s32 num_vp9_videos = ARRAY_SIZE(cts_vp9_videos);

#define PRINT_ERROR		0X0
#define PRINT_QUEUE_STATUS	0X0001
#define PRINT_COUNT		0X0002
#define PRINT_FILE		0X0004
#define PRINT_OTHER		0X0040

int v4l_print(int index, int debug_flag, const char *fmt, ...)
{
	if ((print_flag & debug_flag) ||
	    debug_flag == PRINT_ERROR) {
		unsigned char buf[256];
		int len = 0;
		va_list args;

		va_start(args, fmt);
		len = sprintf(buf, "v4lvideo:[%d]", index);
		vsnprintf(buf + len, 256 - len, fmt, args);
		pr_info("%s", buf);
		va_end(args);
	}
	return 0;
}

void v4lvideo_dec_count_increase(void)
{
	mutex_lock(&mutex_dec_count);
	dec_count++;
	if (dec_count > DI_NR_COUNT && !di_bypass_p) {
		dim_polic_cfg(K_DIM_BYPASS_ALL_P, 1);
		di_bypass_p = true;
		pr_info("v4lvideo: di_p bypass 1\n");
	}
	if (dec_count > 9)
		pr_err("v4lvideo: dec count >9\n");
	mutex_unlock(&mutex_dec_count);
}
EXPORT_SYMBOL(v4lvideo_dec_count_increase);

void v4lvideo_dec_count_decrease(void)
{
	mutex_lock(&mutex_dec_count);
	if (dec_count == 0)
		pr_err("v4lvideo: dec count -1\n");
	dec_count--;
	if (dec_count <= DI_NR_COUNT && di_bypass_p) {
		dim_polic_cfg(K_DIM_BYPASS_ALL_P, 0);
		di_bypass_p = false;
		pr_info("v4lvideo: di_p bypass 0\n");
	}
	mutex_unlock(&mutex_dec_count);
}
EXPORT_SYMBOL(v4lvideo_dec_count_decrease);

static struct keeper_mgr *get_keeper_mgr(void)
{
	return &keeper_mgr_private;
}

static inline u64 dur2pts(u32 duration)
{
	u32 ret;

	ret = duration - (duration >> 4);
	return ret;
}

void keeper_mgr_init(void)
{
	struct keeper_mgr *mgr = get_keeper_mgr();

	memset(mgr, 0, sizeof(struct keeper_mgr));
	spin_lock_init(&mgr->lock);
}

static const struct v4lvideo_fmt formats[] = {
	{.name = "RGB32 (LE)",
	.fourcc = V4L2_PIX_FMT_RGB32, /* argb */
	.depth = 32, },

	{.name = "RGB565 (LE)",
	.fourcc = V4L2_PIX_FMT_RGB565, /* gggbbbbb rrrrrggg */
	.depth = 16, },

	{.name = "RGB24 (LE)",
	.fourcc = V4L2_PIX_FMT_RGB24, /* rgb */
	.depth = 24, },

	{.name = "RGB24 (BE)",
	.fourcc = V4L2_PIX_FMT_BGR24, /* bgr */
	.depth = 24, },

	{.name = "12  Y/CbCr 4:2:0",
	.fourcc = V4L2_PIX_FMT_NV12,
	.depth = 12, },

	{.name = "12  Y/CrCb 4:2:0",
	.fourcc = V4L2_PIX_FMT_NV21,
	.depth = 12, },

	{.name = "YUV420P",
	.fourcc = V4L2_PIX_FMT_YUV420,
	.depth = 12, },

	{.name = "YVU420P",
	.fourcc = V4L2_PIX_FMT_YVU420,
	.depth = 12, }
};

static const struct v4lvideo_fmt *__get_format(u32 pixelformat)
{
	const struct v4lvideo_fmt *fmt;
	unsigned int k;

	for (k = 0; k < ARRAY_SIZE(formats); k++) {
		fmt = &formats[k];
		if (fmt->fourcc == pixelformat)
			break;
	}

	if (k == ARRAY_SIZE(formats))
		return NULL;

	return &formats[k];
}

static const struct v4lvideo_fmt *get_format(struct v4l2_format *f)
{
	return __get_format(f->fmt.pix.pixelformat);
}

static void init_active_file_list(struct v4lvideo_dev *dev)
{
	int i = 0;

	for (i = 0; i < V4LVIDEO_POOL_SIZE; i++) {
		dev->active_file[i].index = i;
		atomic_set(&dev->active_file[i].on_use, false);
		dev->active_file[i].p = NULL;
	}
}

static void active_file_list_push(struct v4lvideo_dev *dev,
			      struct file_private_data *file_private_data)
{
	int i;

	for (i = 0; i < V4LVIDEO_POOL_SIZE; i++) {
		if (!atomic_read(&dev->active_file[i].on_use))
			break;
	}

	if (i == V4LVIDEO_POOL_SIZE) {
		v4l_print(dev->inst, PRINT_ERROR, "active file full!!!\n");
		return;
	}

	v4l_print(dev->inst, PRINT_FILE, "active push %d, %p\n",
		i, file_private_data);
	atomic_set(&dev->active_file[i].on_use, true);
	dev->active_file[i].p = file_private_data;
}

static void active_file_list_pop(struct v4lvideo_dev *dev,
	struct file_private_data *file_private_data, bool file_release)
{
	int i = 0;

	for (i = 0; i < V4LVIDEO_POOL_SIZE; i++) {
		if (atomic_read(&dev->active_file[i].on_use) &&
			dev->active_file[i].p == file_private_data) {
			dev->active_file[i].p = NULL;
			atomic_set(&dev->active_file[i].on_use, false);
			break;
		}
	}
	v4l_print(dev->inst, PRINT_FILE, "active pop %d, %p\n",
		i, file_private_data);

	if (!file_release && i == V4LVIDEO_POOL_SIZE) {
		v4l_print(dev->inst, PRINT_ERROR, "file not in active list\n");
		return;
	}
}

static bool active_file_list_check(struct v4lvideo_dev *dev,
	struct file_private_data *file_private_data)
{
	int i = 0;

	v4l_print(dev->inst, PRINT_FILE, "active check %p\n",
		file_private_data);

	for (i = 0; i < V4LVIDEO_POOL_SIZE; i++) {
		if (atomic_read(&dev->active_file[i].on_use) &&
			dev->active_file[i].p == file_private_data) {
			v4l_print(dev->inst, PRINT_FILE,
				"active pop ok i=%d\n", i);
			return true;
		}
	}

	v4l_print(dev->inst, PRINT_FILE, "active check: freed\n");

	return false;
}

static void video_keeper_keep_mem(void *mem_handle, int type, int *id)
{
	int ret;
	int old_id = *id;
	struct keeper_mgr *mgr = get_keeper_mgr();
	int i;
	unsigned long flags;
	int have_samed = 0;
	int keep_id = -1;
	int slot_i = -1;

	if (!mem_handle)
		return;

	spin_lock_irqsave(&mgr->lock, flags);
	for (i = 0; i < MAX_KEEP_FRAME; i++) {
		if (!mgr->keep_list[i].handle && slot_i < 0) {
			slot_i = i;
		} else if (mem_handle == mgr->keep_list[i].handle) {
			mgr->keep_list[i].count++;
			have_samed = true;
			keep_id = mgr->keep_list[i].keep_id;
			break;
		}
	}

	if (have_samed) {
		spin_unlock_irqrestore(&mgr->lock, flags);
		*id = keep_id;
		pr_info("v4lvideo: keep same mem handle=%p, keep_id=%d\n", mem_handle, keep_id);
		return;
	}

	if (slot_i < 0) {
		spin_unlock_irqrestore(&mgr->lock, flags);
		keep_id = -1;
		pr_info("v4lvideo: keep failed because keep_list fulled!!!\n");
		return;
	}

	mgr->keep_list[slot_i].count++;
	if (mgr->keep_list[slot_i].count != 1)
		pr_err("v4lvideo: keep error mem handle=%p\n", mem_handle);
	mgr->keep_list[slot_i].handle = mem_handle;

	spin_unlock_irqrestore(&mgr->lock, flags);

	ret = codec_mm_keeper_mask_keep_mem(mem_handle,
					    type);
	if (ret > 0) {
		if (old_id > 0 && ret != old_id) {
			/* wait 80 ms for vsync post. */
			codec_mm_keeper_unmask_keeper(old_id, 0);
		}
		*id = ret;
	} else {
		spin_lock_irqsave(&mgr->lock, flags);
		mgr->keep_list[slot_i].count--;
		mgr->keep_list[slot_i].keep_id = -1;
		mgr->keep_list[slot_i].handle = NULL;
		spin_unlock_irqrestore(&mgr->lock, flags);
		pr_err("v4lvideo: keep mask error ret=%d\n", ret);
		return;
	}
	mgr->keep_list[slot_i].keep_id = *id;
}

static void video_keeper_free_mem(int keep_id, int delayms)
{
	struct keeper_mgr *mgr = get_keeper_mgr();
	int i;
	unsigned long flags;
	int need_continue_keep = 0;

	if (keep_id <= 0) {
		pr_err("invalid keepid %d\n", keep_id);
		return;
	}

	spin_lock_irqsave(&mgr->lock, flags);
	for (i = 0; i < MAX_KEEP_FRAME; i++) {
		if (keep_id == mgr->keep_list[i].keep_id) {
			mgr->keep_list[i].count--;
			if (mgr->keep_list[i].count > 0) {
				need_continue_keep = 1;
				break;
			} else if (mgr->keep_list[i].count == 0) {
				mgr->keep_list[i].keep_id = -1;
				mgr->keep_list[i].handle = NULL;
			} else {
				pr_err("v4lvideo: free mem err count =%d\n",
				       mgr->keep_list[i].count);
			}
		}
	}
	spin_unlock_irqrestore(&mgr->lock, flags);

	if (need_continue_keep) {
		pr_info("v4lvideo: need_continue_keep keep_id=%d\n", keep_id);
		return;
	}
	codec_mm_keeper_unmask_keeper(keep_id, delayms);
}

static void vf_keep(struct v4lvideo_dev *dev,
		    struct v4lvideo_file_s *v4lvideo_file,
		    struct file_private_data *file_private_data)
{
	struct vframe_s *vf_p;
	struct vframe_s *vf_ext_p = NULL;
	int type = MEM_TYPE_CODEC_MM;
	int keep_id = 0;
	int keep_id_1 = 0;
	int keep_head_id = 0;
	int keep_dw_id = 0;
	u32 flag;
	u32 inst_id = dev->inst;
	bool di_pw = false;

	if (!file_private_data) {
		V4LVID_ERR("%s error: file_private_data is NULL", __func__);
		return;
	}

	if (!active_file_list_check(dev, file_private_data)) {
		vf_p = NULL;
		v4l_print(dev->inst, PRINT_OTHER,
			"keep, but private has been freed\n");
	} else {
		vf_p = file_private_data->vf_p;
	}

	if (!vf_p || vf_p != v4lvideo_file->vf_p) {
		v4l_print(dev->inst, PRINT_ERROR,
			"maybe file has been released\n");
		mutex_lock(&dev->mutex_opened);
		if (dev->opened) {
			mutex_unlock(&dev->mutex_opened);
			v4lvideo_file->free_before_unreg = true;
		} else {
			mutex_unlock(&dev->mutex_opened);
			v4l_print(dev->inst, PRINT_ERROR,
				"vf keep: device has beed closed\n");
			flag = v4lvideo_file->flag;
			if (flag & V4LVIDEO_FLAG_DI_DEC)
				v4l_print(dev->inst, PRINT_ERROR,
					"vf keep shold not here");
			if (v4lvideo_file->vf_type & VIDTYPE_DI_PW) {
				vf_p = v4lvideo_file->vf_p;
				dim_post_keep_cmd_release2(vf_p);
				total_release_count[inst_id]++;
			}
		}
		return;
	}

	if (file_private_data->flag & V4LVIDEO_FLAG_DI_NR) {
		if (vf_p->type & VIDTYPE_DI_PW)
			di_pw = true;
		vf_ext_p = file_private_data->vf_ext_p;
		if (!vf_ext_p) {
			V4LVID_ERR("%s error: vf_ext is NULL", __func__);
			return;
		}
		vf_p = vf_ext_p;
	}

	if (vf_p->type & VIDTYPE_SCATTER)
		type = MEM_TYPE_CODEC_MM_SCATTER;
	/*mem_handle	    yuv data	       afbc body */
	/*mem_handle_1	     NULL	       afbc body only for S5 */
	/*mem_head_handle    NULL	       afbc head (h265 include dw data) */
	/*mem_dw_handle	     NULL	       dw data(non-h265, h265 is NULL)*/

	/*has di buffer, decoder has afbc: decoder only need keep dw*/
	if (di_pw && (vf_p->type & VIDTYPE_SCATTER)) {
		video_keeper_keep_mem(vf_p->mem_head_handle, MEM_TYPE_CODEC_MM,
			&keep_head_id);
		video_keeper_keep_mem(vf_p->mem_dw_handle, MEM_TYPE_CODEC_MM,
			&keep_dw_id);
	} else {
		video_keeper_keep_mem(vf_p->mem_handle,	type, &keep_id);
		video_keeper_keep_mem(vf_p->mem_handle_1, type, &keep_id_1);
		video_keeper_keep_mem(vf_p->mem_head_handle, MEM_TYPE_CODEC_MM,
			&keep_head_id);
		video_keeper_keep_mem(vf_p->mem_dw_handle, MEM_TYPE_CODEC_MM,
			&keep_dw_id);
	}

	file_private_data->keep_id = keep_id;
	file_private_data->keep_id_1 = keep_id_1;
	file_private_data->keep_head_id = keep_head_id;
	file_private_data->keep_dw_id = keep_dw_id;
	file_private_data->is_keep = true;
}

void v4lvideo_keep_vf(struct file *file)
{
	struct vframe_s *vf_p;
	struct vframe_s *vf_ext_p = NULL;
	int type = MEM_TYPE_CODEC_MM;
	int keep_id = 0;
	int keep_id_1 = 0;
	int keep_head_id = 0;
	int keep_dw_id = 0;
	struct file_private_data *file_private_data;

	file_private_data = v4lvideo_get_file_private_data(file, false);

	if (!file_private_data) {
		V4LVID_ERR("vf_keep error: file_private_data is NULL");
		return;
	}

	file_private_data->vf.flag |= VFRAME_FLAG_KEEPED;
	file_private_data->vf_ext.flag |= VFRAME_FLAG_KEEPED;

	vf_p = file_private_data->vf_p;

	if (file_private_data->flag & V4LVIDEO_FLAG_DI_NR) {
		vf_ext_p = file_private_data->vf_ext_p;
		if (!vf_ext_p) {
			V4LVID_ERR("file_vf_keep error: vf_ext is NULL");
			return;
		}
		vf_p = vf_ext_p;
	}

	if (vf_p->type & VIDTYPE_SCATTER)
		type = MEM_TYPE_CODEC_MM_SCATTER;

	/*vdin vf has VIDTYPE_SCATTER, but mem not from scatter*/
	if ((vf_p->source_type == VFRAME_SOURCE_TYPE_HDMI ||
		     vf_p->source_type == VFRAME_SOURCE_TYPE_CVBS ||
		     vf_p->source_type == VFRAME_SOURCE_TYPE_TUNER) &&
		     !(vf_p->type_ext & VIDTYPE_EXT_VDIN_SCATTER))
		type = MEM_TYPE_CODEC_MM;

	video_keeper_keep_mem(vf_p->mem_handle, type, &keep_id);
	video_keeper_keep_mem(vf_p->mem_handle_1, type, &keep_id_1);
	video_keeper_keep_mem(vf_p->mem_head_handle, MEM_TYPE_CODEC_MM,
		&keep_head_id);
	video_keeper_keep_mem(vf_p->mem_dw_handle, MEM_TYPE_CODEC_MM,
		&keep_dw_id);

	file_private_data->keep_id = keep_id;
	file_private_data->keep_id_1 = keep_id_1;
	file_private_data->keep_head_id = keep_head_id;
	file_private_data->keep_dw_id = keep_dw_id;
	file_private_data->is_keep = true;
}

static void vf_free(struct file_private_data *file_private_data)
{
	struct vframe_s *vf_p;
	struct vframe_s *vf;
	u32 flag;
	u32 inst_id;

	inst_id = file_private_data->v4l_inst_id;
	if (file_private_data->keep_id > 0) {
		video_keeper_free_mem(file_private_data->keep_id, 0);
		file_private_data->keep_id = -1;
	}
	if (file_private_data->keep_head_id > 0) {
		video_keeper_free_mem(file_private_data->keep_head_id, 0);
		file_private_data->keep_head_id = -1;
	}
	if (file_private_data->keep_id_1 > 0) {
		video_keeper_free_mem(file_private_data->keep_id_1, 0);
		file_private_data->keep_id_1 = -1;
	}
	if (file_private_data->keep_dw_id > 0) {
		video_keeper_free_mem(file_private_data->keep_dw_id, 0);
		file_private_data->keep_dw_id = -1;
	}

	vf = &file_private_data->vf;
	vf_p = file_private_data->vf_p;
	flag = file_private_data->flag;
	if (flag & V4LVIDEO_FLAG_DI_DEC) {
		vf = &file_private_data->vf_ext;
		vf_p = file_private_data->vf_ext_p;
	}

	if (vf->type & VIDTYPE_DI_PW) {
		v4l_print(inst_id, PRINT_OTHER,
			"free: omx_index=%d\n", vf_p->omx_index);
		dim_post_keep_cmd_release2(vf_p);
		total_release_count[inst_id]++;
		v4l_print(inst_id, PRINT_COUNT,
			"di release1 get=%d, put=%d, release=%d\n",
			total_get_count[inst_id], total_put_count[inst_id],
			total_release_count[inst_id]);
	}
}

static void vf_free_force(struct v4lvideo_file_s *v4lvideo_file)
{
	struct vframe_s *vf_p;
	u32 flag;
	u32 inst_id;

	inst_id = v4lvideo_file->inst_id;
	vf_p = v4lvideo_file->vf_p;
	flag = v4lvideo_file->flag;
	if (flag & V4LVIDEO_FLAG_DI_DEC)
		vf_p = v4lvideo_file->vf_ext_p;

	if (v4lvideo_file->vf_type & VIDTYPE_DI_PW) {
		dim_post_keep_cmd_release2(vf_p);
		total_release_count[inst_id]++;
		v4l_print(inst_id, PRINT_COUNT,
			"di release2 get=%d, put=%d, release=%d\n",
			total_get_count[inst_id], total_put_count[inst_id],
			total_release_count[inst_id]);
	}
}

void init_file_private_data(struct file_private_data *file_private_data)
{
	if (file_private_data) {
		memset(&file_private_data->vf, 0, sizeof(struct vframe_s));
		memset(&file_private_data->vf_ext, 0, sizeof(struct vframe_s));
		file_private_data->vf_p = NULL;
		file_private_data->vf_ext_p = NULL;
		file_private_data->is_keep = false;
		file_private_data->keep_id = -1;
		file_private_data->keep_head_id = -1;
		file_private_data->file = NULL;
		file_private_data->flag = 0;
		file_private_data->cnt_file = NULL;
	} else {
		V4LVID_ERR("%s is NULL!!", __func__);
	}
}

static DEFINE_SPINLOCK(devlist_lock);
static unsigned long v4lvideo_devlist_lock(void)
{
	unsigned long flags;

	spin_lock_irqsave(&devlist_lock, flags);
	return flags;
}

static void v4lvideo_devlist_unlock(unsigned long flags)
{
	spin_unlock_irqrestore(&devlist_lock, flags);
}

static LIST_HEAD(v4lvideo_devlist);

int v4lvideo_assign_map(char **receiver_name, int *inst)
{
	unsigned long flags;
	struct v4lvideo_dev *dev = NULL;
	struct list_head *p;

	flags = v4lvideo_devlist_lock();

	list_for_each(p, &v4lvideo_devlist) {
		dev = list_entry(p, struct v4lvideo_dev, v4lvideo_devlist);

		if (dev->inst == *inst) {
			*receiver_name = dev->vf_receiver_name;
			v4lvideo_devlist_unlock(flags);
			return 0;
		}
	}

	v4lvideo_devlist_unlock(flags);

	return -ENODEV;
}
EXPORT_SYMBOL(v4lvideo_assign_map);

int v4lvideo_alloc_map(int *inst)
{
	unsigned long flags;
	struct v4lvideo_dev *dev = NULL;
	struct list_head *p;

	flags = v4lvideo_devlist_lock();

	list_for_each(p, &v4lvideo_devlist) {
		dev = list_entry(p, struct v4lvideo_dev, v4lvideo_devlist);

		if (dev->inst >= 0 && !dev->mapped) {
			dev->mapped = true;
			*inst = dev->inst;
			v4lvideo_devlist_unlock(flags);
			#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
			dv_inst_map(&dev->dv_inst);
			#endif
			return 0;
		}
	}

	v4lvideo_devlist_unlock(flags);
	return -ENODEV;
}

void v4lvideo_release_map(int inst)
{
	unsigned long flags;
	struct v4lvideo_dev *dev = NULL;
	struct list_head *p;

	flags = v4lvideo_devlist_lock();

	list_for_each(p, &v4lvideo_devlist) {
		dev = list_entry(p, struct v4lvideo_dev, v4lvideo_devlist);
		if (dev->inst == inst && dev->mapped) {
			dev->mapped = false;
			pr_debug("%s %d OK\n", __func__, dev->inst);
			break;
		}
	}

	v4lvideo_devlist_unlock(flags);

	#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	dv_inst_unmap(dev->dv_inst);
	#endif
}

void v4lvideo_release_map_force(struct v4lvideo_dev *dev)
{
	unsigned long flags;

	flags = v4lvideo_devlist_lock();

	if (dev->mapped) {
		dev->mapped = false;
		pr_debug("%s %d OK\n", __func__, dev->inst);
	}

	v4lvideo_devlist_unlock(flags);
	#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	dv_inst_unmap(dev->dv_inst);
	#endif
}

static struct ge2d_context_s *context;
static int canvas_src_id[3];
static int canvas_dst_id[3];

static int get_source_type(struct vframe_s *vf)
{
	enum vframe_source_type ret;
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
	enum vframe_source_type soure_type;

	soure_type = get_source_type(vf);
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
		if (vf->type & VIDTYPE_VIU_422)
			format = GE2D_FORMAT_S16_YUV422;
		else if (vf->type & VIDTYPE_VIU_NV21)
			format = GE2D_FORMAT_M24_NV21;
		else if (vf->type & VIDTYPE_VIU_444)
			format = GE2D_FORMAT_S24_YUV444;
		else
			format = GE2D_FORMAT_M24_YUV420;
		break;
	case VDIN_10BIT_NORMAL:
		if (vf->type & VIDTYPE_VIU_422) {
			if (vf->bitdepth & FULL_PACK_422_MODE)
				format = GE2D_FORMAT_S16_10BIT_YUV422;
			else
				format = GE2D_FORMAT_S16_12BIT_YUV422;
		}
		break;
	default:
		format = GE2D_FORMAT_M24_YUV420;
	}
	return format;
}

static void dump_yuv_data(struct vframe_s *vf,
			struct v4l_data_t *v4l_data)
{
	struct file *fp;
	mm_segment_t fs;
	loff_t pos;
	char name_buf[32];
	u32 write_size;

	snprintf(name_buf, sizeof(name_buf), "/data/tmp/%d-%d.raw",
		 vf->width, vf->height);
	fs = get_fs();
	set_fs(KERNEL_DS);
	pos = 0;
	fp = filp_open(name_buf, O_CREAT | O_RDWR, 0644);
	if (IS_ERR(fp)) {
		pr_err("create %s fail.\n", name_buf);
	} else {
		write_size = v4l_data->byte_stride *
			v4l_data->height * 3 / 2;
		vfs_write(fp, phys_to_virt(v4l_data->phy_addr[0]),
			  write_size, &pos);
		pr_info("write %u size to file.\n", write_size);
		filp_close(fp, NULL);
	}
	set_fs(fs);
}

static void do_vframe_afbc_soft_decode(struct v4l_data_t *v4l_data,
					struct vframe_s *vf)
{
	int i, j, ret, y_size, free_cnt;
	short *planes[4];
	short *y_src, *u_src, *v_src, *s2c, *s2c1;
	u8 *tmp, *tmp1;
	u8 *y_dst, *vu_dst;
	int bit_10;
	struct timeval start, end;
	unsigned long time_use = 0;

	if ((vf->bitdepth & BITDEPTH_YMASK)  == BITDEPTH_Y10)
		bit_10 = 1;
	else
		bit_10 = 0;

	y_size = vf->compWidth * vf->compHeight * sizeof(short);
	pr_info("width: %d, height: %d, compWidth: %u, compHeight: %u.\n",
		 vf->width, vf->height, vf->compWidth, vf->compHeight);
	for (i = 0; i < 4; i++) {
		planes[i] = vmalloc(y_size);
		if (!planes[i]) {
			free_cnt = i;
			pr_err("vmalloc fail in %s\n", __func__);
			goto free;
		}
		pr_info("plane %d size: %d, vmalloc addr: %p.\n",
			i, y_size, planes[i]);
	}
	free_cnt = 4;

	do_gettimeofday(&start);
	ret = AMLOGIC_FBC_vframe_decoder_v1((void **)planes, vf, 0, 0);
	if (ret < 0) {
		pr_err("amlogic_fbc_lib.ko error %d", ret);
		goto free;
	}

	do_gettimeofday(&end);
	time_use = (end.tv_sec - start.tv_sec) * 1000 +
				(end.tv_usec - start.tv_usec) / 1000;
	pr_debug("FBC Decompress time: %ldms\n", time_use);

	y_src = planes[0];
	u_src = planes[1];
	v_src = planes[2];

	y_dst = v4l_data->dst_addr;
	vu_dst = v4l_data->dst_addr + v4l_data->byte_stride * v4l_data->height;

	do_gettimeofday(&start);
	for (i = 0; i < vf->compHeight; i++) {
		for (j = 0; j < vf->compWidth; j++) {
			s2c = y_src + j;
			tmp = (u8 *)(s2c);
			if (bit_10)
				*(y_dst + j) = *s2c >> 2;
			else
				*(y_dst + j) = tmp[0];
		}

			y_dst += v4l_data->byte_stride;
			y_src += vf->compWidth;
	}

	for (i = 0; i < (vf->compHeight / 2); i++) {
		for (j = 0; j < vf->compWidth; j += 2) {
			s2c = v_src + j / 2;
			s2c1 = u_src + j / 2;
			tmp = (u8 *)(s2c);
			tmp1 = (u8 *)(s2c1);

			if (bit_10) {
				*(vu_dst + j) = *s2c >> 2;
				*(vu_dst + j + 1) = *s2c1 >> 2;
			} else {
				*(vu_dst + j) = tmp[0];
				*(vu_dst + j + 1) = tmp1[0];
			}
		}
		vu_dst += v4l_data->byte_stride;
		u_src += (vf->compWidth / 2);
		v_src += (vf->compWidth / 2);
	}

	do_gettimeofday(&end);
	time_use = (end.tv_sec - start.tv_sec) * 1000 +
				(end.tv_usec - start.tv_usec) / 1000;
	pr_debug("bitblk time: %ldms\n", time_use);
	if (vf_dump)
		dump_yuv_data(vf, v4l_data);

free:
	for (i = 0; i < free_cnt; i++)
		vfree(planes[i]);
}

/* for fbc output video:vp9 */
static bool need_do_extend_one_column_fbc(struct vframe_s *vf,
					  struct v4l_data_t *v4l_data)
{
	u32 video_idx;

	pr_info("vf->compwidth:%d v4l_data->byte_stride:%d num_vp9_videos:%d\n",
		 vf->compWidth, v4l_data->byte_stride, num_vp9_videos);

	if (cts_video_flag || vf->compWidth >= v4l_data->byte_stride)
		return false;

	for (video_idx = 0; video_idx < num_vp9_videos; video_idx++) {
		if (v4l_data->width == cts_vp9_videos[video_idx].width &&
			v4l_data->height == cts_vp9_videos[video_idx].height) {
			pr_info("no need_do_extend_one_column_vp9\n");
			return false;
		}
	}
	return true;
}

static bool need_do_extend_one_row_fbc(struct vframe_s *vf,
				       struct v4l_data_t *v4l_data)
{
	u32 video_idx;

	pr_info("vf->compHeight:%d v4l_data->height:%d num_vp9_videos:%d\n",
		 vf->compHeight, v4l_data->height, num_vp9_videos);

	if (cts_video_flag || vf->compHeight >= v4l_data->height)
		return false;

	for (video_idx = 0; video_idx < num_vp9_videos; video_idx++) {
		if (v4l_data->width == cts_vp9_videos[video_idx].width &&
			v4l_data->height == cts_vp9_videos[video_idx].height) {
			pr_info("no need_do_extend_one_row_vp9\n");
			return false;
		}
	}
	return true;
}

void v4lvideo_data_copy(struct v4l_data_t *v4l_data,
				struct dma_buf *dmabuf,
				u32 align)
{
	struct uvm_hook_mod *uhmod = NULL;
	struct config_para_ex_s ge2d_config;
	struct canvas_config_s dst_canvas_config[3];
	struct vframe_s *vf = NULL;
	const char *keep_owner = "ge2d_dest_comp";
	bool di_mode = false;
	bool is_10bit = false;
	bool is_dec_vf = false;
	u32 aligned_height = 0;
	char *y_vaddr = NULL;
	char *uv_vaddr = NULL;
	char *y_src = NULL;
	char *uv_src = NULL;
	u32 row;
	struct file_private_data *file_private_data = NULL;

	is_dec_vf = is_valid_mod_type(dmabuf, VF_SRC_DECODER);

	if (is_dec_vf) {
		vf = dmabuf_get_vframe(dmabuf);
		pr_debug("vf=%p vf->vf_ext:%p vf->flags:%d\n",
				vf, vf->vf_ext, vf->flag);
		if (vf->vf_ext &&
				(vf->flag & VFRAME_FLAG_CONTAIN_POST_FRAME)) {
			if (print_flag)
				pr_info("vf->type:%d\n", vf->type);
			if (vf->type & VIDTYPE_INTERLACE) {
				vf = vf->vf_ext;
				if (print_flag)
					pr_info("get vf_ext\n");
			}
		}
		dmabuf_put_vframe(dmabuf);
	} else {
		if (!dmabuf) {
			file_private_data = v4l_data->file_private_data;
		} else {
			uhmod = uvm_get_hook_mod(dmabuf, VF_PROCESS_V4LVIDEO);
			if (uhmod && uhmod->arg) {
				file_private_data = uhmod->arg;
				uvm_put_hook_mod(dmabuf, VF_PROCESS_V4LVIDEO);
			}
		}
		pr_debug("%s: uhmod: %p\n", __func__, uhmod);
		if (!file_private_data) {
			pr_err("file_private_data is NULL\n");
			return;
		}

		if (file_private_data->flag & V4LVIDEO_FLAG_DI_NR)
			vf = &file_private_data->vf_ext;
		else
			vf = &file_private_data->vf;
		if (cts_use_di)
			vf = &file_private_data->vf;

		v4l_data->file_private_data = file_private_data;
	}


	if (!vf) {
		pr_err("vf is NULL\n");
		return;
	}

	pr_debug("%s: vf->type: %d vf->compWidth: %d\n",
			__func__, vf->type, vf->compWidth);

	/*
	 * fbc decoder for VIDTYPE_COMPRESS
	 */
	if ((vf->type & VIDTYPE_COMPRESS)) {
		if (print_flag)
			pr_info("fbc decoder path\n");
		do_vframe_afbc_soft_decode(v4l_data, vf);
		y_vaddr = v4l_data->dst_addr;
		uv_vaddr = y_vaddr +
			v4l_data->byte_stride * v4l_data->height;
		if (need_do_extend_one_column_fbc(vf, v4l_data) == true) {
			for (row = 0; row < vf->compHeight; row++) {
				int cnt = vf->compWidth +
					row * v4l_data->byte_stride;

				if (print_flag)
					pr_debug("before move y_vaddr[%d]=%d\n",
						 cnt, *(y_vaddr + cnt));
				*(y_vaddr + cnt) = *(y_vaddr + cnt - 1);
				if (print_flag)
					pr_debug("after move y_vaddr[%d]=%d\n",
						 cnt, *(y_vaddr + cnt));
				if (row < vf->compHeight / 2) {
					cnt = vf->compWidth +
						row * v4l_data->byte_stride;
					if (print_flag)
						pr_debug("before uv_vaddr[%d]=%d\n",
							 cnt,
							 *(uv_vaddr + cnt));
					*(uv_vaddr + cnt) =
						*(uv_vaddr + cnt - 2);
					*(uv_vaddr + cnt + 1) =
						*(uv_vaddr + cnt - 1);
					if (print_flag)
						pr_debug("after uv_vaddr[%d]=%d\n",
							 cnt,
							 *(uv_vaddr + cnt));
				}
			}
		}

		if (need_do_extend_one_row_fbc(vf, v4l_data) == false)
			return;

		if (print_flag)
			pr_info("begin copy row\n");
		y_src = y_vaddr + v4l_data->byte_stride * (vf->compHeight - 1);
		uv_src = uv_vaddr +
			v4l_data->byte_stride * (vf->compHeight / 2 - 1);
		memcpy(y_src + v4l_data->byte_stride, y_src, vf->compWidth + 1);
		memcpy(uv_src + v4l_data->byte_stride,
				uv_src, vf->compWidth + 1);
		return;
	}
	/*
	 * GE2D copy for non-compress
	 */
	is_10bit = vf->bitdepth & BITDEPTH_Y10;
	di_mode = vf->type & VIDTYPE_DI_PW;
	if (is_10bit && !di_mode) {
		pr_err("vframe 10bit copy is not supported.\n");
		return;
	}

	memset(&ge2d_config, 0, sizeof(ge2d_config));
	memset(dst_canvas_config, 0, sizeof(dst_canvas_config));

	if (!context)
		context = create_ge2d_work_queue();

	if (!context) {
		pr_err("create_ge2d_work_queue failed.\n");
		return;
	}
	aligned_height = ALIGN(v4l_data->height, align);
	dst_canvas_config[0].phy_addr = v4l_data->phy_addr[0];
	dst_canvas_config[0].width = v4l_data->byte_stride;

	dst_canvas_config[0].height = v4l_data->height;
	dst_canvas_config[0].block_mode = 0;
	dst_canvas_config[0].endian = 0;
	/*
	 * v4ldecoder:
	 * for non-compress videos like H264
	 * need align width and height
	 * v4lvideo:
	 * no requirement for height align
	 */
	dst_canvas_config[1].phy_addr = v4l_data->phy_addr[0] +
		v4l_data->byte_stride * aligned_height;
	dst_canvas_config[1].width = v4l_data->byte_stride;
	dst_canvas_config[1].height = v4l_data->height / 2;
	dst_canvas_config[1].block_mode = 0;
	dst_canvas_config[1].endian = 0;

	pr_debug("compWidth: %u, compHeight: %u.\n",
		 vf->compWidth, vf->compHeight);
	pr_debug("vf-width:%u, vf-height:%u, vf-widht-align:%u\n",
		 vf->width, vf->height, ALIGN(vf->width, 64));
	pr_debug("umm-bytestride: %d, umm-width:%u, umm-height:%u\n",
		 v4l_data->byte_stride, v4l_data->width, v4l_data->height);
	pr_debug("y_addr:%lu, uv_addr:%lu.\n", dst_canvas_config[0].phy_addr,
		 dst_canvas_config[1].phy_addr);
	if (vf->canvas0Addr == (u32)-1) {
		if (canvas_src_id[0] <= 0)
			canvas_src_id[0] =
			canvas_pool_map_alloc_canvas(keep_owner);

		if (canvas_src_id[1] <= 0)
			canvas_src_id[1] =
			canvas_pool_map_alloc_canvas(keep_owner);

		if (canvas_src_id[0] <= 0 || canvas_src_id[1] <= 0) {
			pr_err("canvas pool alloc fail.%d, %d, %d.\n",
			       canvas_src_id[0],
			       canvas_src_id[1],
			       canvas_src_id[2]);
			return;
		}

		canvas_config_config(canvas_src_id[0], &vf->canvas0_config[0]);
		canvas_config_config(canvas_src_id[1], &vf->canvas0_config[1]);
		ge2d_config.src_para.canvas_index = canvas_src_id[0] |
			canvas_src_id[1] << 8;
		pr_debug("src index: %d.\n", ge2d_config.src_para.canvas_index);

	} else {
		ge2d_config.src_para.canvas_index = vf->canvas0Addr;
		pr_debug("src1 : %d.\n", ge2d_config.src_para.canvas_index);
	}

	if (canvas_dst_id[0] <= 0)
		canvas_dst_id[0] = canvas_pool_map_alloc_canvas(keep_owner);
	if (canvas_dst_id[1] <= 0)
		canvas_dst_id[1] = canvas_pool_map_alloc_canvas(keep_owner);
	if (canvas_dst_id[0] <= 0 || canvas_dst_id[1] <= 0) {
		pr_err("canvas pool alloc dst fail. %d, %d.\n",
		       canvas_dst_id[0], canvas_dst_id[1]);
		return;
	}
	canvas_config_config(canvas_dst_id[0], &dst_canvas_config[0]);
	canvas_config_config(canvas_dst_id[1], &dst_canvas_config[1]);

	ge2d_config.dst_para.canvas_index = canvas_dst_id[0] |
			canvas_dst_id[1] << 8;

	pr_debug("dst canvas index: %d.\n", ge2d_config.dst_para.canvas_index);

	ge2d_config.alu_const_color = 0;
	ge2d_config.bitmask_en = 0;
	ge2d_config.src1_gb_alpha = 0;
	ge2d_config.dst_xy_swap = 0;

	ge2d_config.src_key.key_enable = 0;
	ge2d_config.src_key.key_mask = 0;
	ge2d_config.src_key.key_mode = 0;

	ge2d_config.src_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config.src_para.format = get_input_format(vf);
	/*
	 * also need config ge2d src format
	 * VFRAME_FLAG_VIDEO_LINEAR -> little ednian
	 */
	if (vf->flag & VFRAME_FLAG_VIDEO_LINEAR)
		ge2d_config.src_para.format |= GE2D_LITTLE_ENDIAN;
	pr_debug("%s: ge2d_config.src_para.format: %d\n",
			__func__, ge2d_config.src_para.format);
	ge2d_config.src_para.fill_color_en = 0;
	ge2d_config.src_para.fill_mode = 0;
	ge2d_config.src_para.x_rev = 0;
	ge2d_config.src_para.y_rev = 0;
	ge2d_config.src_para.color = 0xffffffff;
	ge2d_config.src_para.top = 0;
	ge2d_config.src_para.left = 0;
	ge2d_config.src_para.width = vf->width;
	if (vf->type & VIDTYPE_INTERLACE)
		ge2d_config.src_para.height = vf->height >> 1;
	else
		ge2d_config.src_para.height = vf->height;
	ge2d_config.src2_para.mem_type = CANVAS_TYPE_INVALID;

	ge2d_config.dst_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config.dst_para.fill_color_en = 0;
	ge2d_config.dst_para.fill_mode = 0;
	ge2d_config.dst_para.x_rev = 0;
	ge2d_config.dst_para.y_rev = 0;
	ge2d_config.dst_para.color = 0;
	ge2d_config.dst_para.top = 0;
	ge2d_config.dst_para.left = 0;
	ge2d_config.dst_para.format = GE2D_FORMAT_M24_NV21
					| GE2D_LITTLE_ENDIAN;
	ge2d_config.dst_para.width = vf->width;
	ge2d_config.dst_para.height = vf->height;

	if (ge2d_context_config_ex(context, &ge2d_config) < 0) {
		pr_err("ge2d_context_config_ex error.\n");
		return;
	}

	if (vf->type & VIDTYPE_INTERLACE)
		stretchblt_noalpha(context, 0, 0, vf->width, vf->height / 2,
				   0, 0, vf->width, vf->height);
	else
		stretchblt_noalpha(context, 0, 0, vf->width, vf->height,
				   0, 0, vf->width, vf->height);
	if (vf_dump)
		dump_yuv_data(vf, v4l_data);
}

s32 v4lvideo_release_sei_data(struct vframe_s *vf)
{
	void *p;
	s32 ret = -2;
	u32 size = 0;

	if (!vf)
		return ret;

	p = get_sei_from_src_fmt(vf, &size);
	if (p) {
		vfree(p);
		atomic_dec(&global_set_cnt);
	}
	ret = clear_vframe_src_fmt(vf);
	return ret;
}

static void v4lvideo_private_data_release(struct file_private_data *data)
{
	struct v4lvideo_dev *dev = NULL;
	struct v4lvideo_file_s *v4lvideo_file = NULL;

	if (!data)
		return;

	v4lvideo_file = (struct v4lvideo_file_s *)data->private;
	if (v4lvideo_file)
		dev = v4lvideo_file->dev;
	if (data->is_keep)
		vf_free(data);
	v4lvideo_release_sei_data(&data->vf);

	if (dev) {
		mutex_lock(&dev->mutex_input);
		active_file_list_pop(dev, data, true);
		mutex_unlock(&dev->mutex_input);
	}
	if (data->md.p_md) {
		vfree(data->md.p_md);
		data->md.p_md = NULL;
	}
	if (data->md.p_comp) {
		vfree(data->md.p_comp);
		data->md.p_comp = NULL;
	}
	if (alloc_sei & 2)
		pr_info("v4lvideo private data release\n");

	memset(data, 0, sizeof(struct file_private_data));
}

void free_fd_private(void *arg)
{
	if (arg) {
		v4lvideo_private_data_release(arg);
		kfree((u8 *)arg);
	} else {
		pr_err("free: arg is NULL\n");
	}
}

struct file_private_data *v4lvideo_get_file_private_data(struct file *file_vf,
							 bool alloc_if_null)
{
	struct file_private_data *file_private_data;
	bool is_v4lvideo_fd = false;
	struct uvm_hook_mod *uhmod;
	struct uvm_hook_mod_info info;
	int ret;

	if (!file_vf) {
		pr_err("v4lvideo: get_file_private_data fail\n");
		return NULL;
	}

	if (is_v4lvideo_buf_file(file_vf))
		is_v4lvideo_fd = true;

	if (is_v4lvideo_fd) {
		file_private_data =
			(struct file_private_data *)(file_vf->private_data);
		return file_private_data;
	}

	if (!dmabuf_is_uvm((struct dma_buf *)file_vf->private_data)) {
		pr_err("v4lvideo: dma file private data is not uvm\n");
		return NULL;
	}

	uhmod = uvm_get_hook_mod((struct dma_buf *)(file_vf->private_data),
				 VF_PROCESS_V4LVIDEO);
	if (uhmod && uhmod->arg) {
		file_private_data = uhmod->arg;
		uvm_put_hook_mod((struct dma_buf *)(file_vf->private_data),
				 VF_PROCESS_V4LVIDEO);
		return file_private_data;
	} else if (!alloc_if_null) {
		return NULL;
	}

	file_private_data = kzalloc(sizeof(*file_private_data), GFP_KERNEL);
	if (!file_private_data)
		return NULL;

	file_private_data->md.p_md  = vmalloc(MD_BUF_SIZE);
	if (!file_private_data->md.p_md) {
		kfree((u8 *)file_private_data);
		return NULL;
	}

	file_private_data->md.p_comp = vmalloc(COMP_BUF_SIZE);
	if (!file_private_data->md.p_comp) {
		if (file_private_data->md.p_md) {
			vfree(file_private_data->md.p_md);
			file_private_data->md.p_md = NULL;
		}
		kfree((u8 *)file_private_data);
		return NULL;
	}
	memset(&info, 0, sizeof(struct uvm_hook_mod_info));
	info.type = VF_PROCESS_V4LVIDEO;
	info.arg = file_private_data;
	info.free = free_fd_private;
	info.acquire_fence = NULL;
	ret =  uvm_attach_hook_mod((struct dma_buf *)(file_vf->private_data),
				   &info);

	return file_private_data;
}

struct file_private_data *v4lvideo_get_vf(int fd)
{
	struct file *file_vf = NULL;
	struct file_private_data *file_private_data;

	file_vf = fget(fd);
	if (!file_vf) {
		pr_err("%s file_vf is NULL\n", __func__);
		return NULL;
	}

	file_private_data = v4lvideo_get_file_private_data(file_vf, false);
	fput(file_vf);
	if (!file_private_data) {
		pr_err("%s private is NULL\n", __func__);
		return NULL;
	}

	return file_private_data;
}

s32 v4lvideo_import_sei_data(struct vframe_s *vf,
				    struct vframe_s *dup_vf,
				    char *provider)
{
	struct provider_aux_req_s req;
	s32 ret = -2;
	char *p;
	u32 try_count = 0;
	u32 max_count = 1;
	bool fmt_update = false;
	u32 sei_size = 0;

	if (!vf || !dup_vf || !provider || !alloc_sei)
		return ret;

	/*if ((!(vf->flag & VFRAME_FLAG_DOUBLE_FRAM)) &&*/
	/*    (vf->type & VIDTYPE_DI_PW))*/
	/*	return ret;*/

	if (vf->type & VIDTYPE_DI_PW &&
		vf->src_fmt.sei_magic_code == SEI_MAGIC_CODE) {
		req.aux_buf = (char *)get_sei_from_src_fmt(vf, &sei_size);
		req.aux_size = sei_size;
		req.dv_enhance_exist = vf->src_fmt.dual_layer ? 1 : 0;
		if (req.aux_buf && req.aux_size) {
			p = vmalloc(req.aux_size);
			if (p) {
				memcpy(p, req.aux_buf, req.aux_size);
				ret = update_vframe_src_fmt(dup_vf, (void *)p,
					(u32)req.aux_size,
					vf->src_fmt.dual_layer ? true : false,
					provider, NULL);
				if (!ret)
					atomic_inc(&global_set_cnt);
				else
					vfree(p);
			}
		} else {
			ret = update_vframe_src_fmt(dup_vf, NULL, 0,
				false, provider, NULL);
		}
		goto finish_import;
	}

	if (!strcmp(provider, "dvbldec") && dup_vf->omx_index < 2)
		max_count = 10;

	while (try_count++ < max_count) {
		req.vf = vf;
		req.bot_flag = 0;
		req.aux_buf = NULL;
		req.aux_size = 0;
		req.dv_enhance_exist = 0;
		vf_notify_provider_by_name(provider,
			VFRAME_EVENT_RECEIVER_GET_AUX_DATA,
			(void *)&req);
		if (req.aux_buf && req.aux_size) {
			p = vmalloc(req.aux_size);
			if (p) {
				memcpy(p, req.aux_buf, req.aux_size);
				ret = update_vframe_src_fmt(dup_vf, (void *)p,
							    (u32)req.aux_size,
							    req.dv_enhance_exist
							    ? true : false,
							    provider, NULL);
				if (!ret) {
				/* FIXME: work around for sei/el out of sync */
					/*if (dup_vf->src_fmt.fmt == */
					/*     VFRAME_SIGNAL_FMT_SDR && */
					/*    !strcmp(provider, "dvbldec")) */
					/*	dup_vf->src_fmt.fmt = */
					/*	VFRAME_SIGNAL_FMT_DOVI; */
					atomic_inc(&global_set_cnt);
				} else {
					vfree(p);
				}
			} else {
				ret = update_vframe_src_fmt(dup_vf, NULL,
							    0, false,
							    provider, NULL);
			}
			fmt_update = true;
			break;
		} else {
			if (max_count > 1)
				usleep_range(1000, 2000);
		}
	}
	if (!fmt_update) {
		ret = update_vframe_src_fmt(dup_vf, NULL, 0,
					    false, provider, NULL);
		if ((alloc_sei & 2) && max_count > 1)
			pr_info("try %d, no aux data\n", try_count);
	}

	if ((alloc_sei & 2) && max_count > 1)
		pr_info("sei try_count %d\n", try_count);
finish_import:
	if (alloc_sei & 2)
		pr_info("import sei: provider:%s, vf:%p, dup_vf:%p, req.aux_buf:%p, req.aux_size:%d, req.dv_enhance_exist:%d, vf->src_fmt.fmt:%d\n",
			provider, vf, dup_vf,
			req.aux_buf, req.aux_size,
			req.dv_enhance_exist, dup_vf->src_fmt.fmt);
	return ret;
}

/* ------------------------------------------------------------------
 * DMA and thread functions
 * ------------------------------------------------------------------
 */
unsigned int get_v4lvideo_debug(void)
{
	return debug;
}
EXPORT_SYMBOL(get_v4lvideo_debug);

static int vidioc_streamon(struct file *file, void *priv, enum v4l2_buf_type i)
{
	struct v4lvideo_dev *dev = video_drvdata(file);

	dprintk(dev, 2, "%s\n", __func__);

	dev->provider_name = NULL;

	dprintk(dev, 2, "returning from %s\n", __func__);

	return 0;
}

static int vidioc_streamoff(struct file *file, void *priv, enum v4l2_buf_type i)
{
	struct v4lvideo_dev *dev = video_drvdata(file);

	dprintk(dev, 2, "%s\n", __func__);

	/*
	 * Typical driver might need to wait here until dma engine stops.
	 * In this case we can abort immediately, so it's just a noop.
	 */
	v4l2q_init(&dev->input_queue, V4LVIDEO_POOL_SIZE,
		   (void **)&dev->v4lvideo_input_queue[0]);
	return 0;
}

/* ------------------------------------------------------------------
 * Videobuf operations
 * ------------------------------------------------------------------
 */

static int vidioc_g_parm(struct file *file, void *priv,
			 struct v4l2_streamparm *parms)
{
	struct v4lvideo_dev *dev = video_drvdata(file);

	if (parms->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;

	memcpy(parms->parm.raw_data, (u8 *)&dev->am_parm,
	       sizeof(struct v4l2_amlogic_parm));

	return 0;
}

/* ------------------------------------------------------------------
 * IOCTL vidioc handling
 * ------------------------------------------------------------------
 */
static int vidioc_open(struct file *file)
{
	struct v4lvideo_dev *dev = video_drvdata(file);
	int i;

	if (dev->fd_num > 0) {
		pr_err("%s error\n", __func__);
		return -EBUSY;
	}

	dev->fd_num++;
	dev->vf_wait_cnt = 0;

	v4l2q_init(&dev->input_queue,
		   V4LVIDEO_POOL_SIZE,
		   (void **)&dev->v4lvideo_input_queue[0]);

	memset(&dev->v4lvideo_file[0], 0,
	       sizeof(struct v4lvideo_file_s) * V4LVIDEO_POOL_SIZE);
	for (i = 0; i < V4LVIDEO_POOL_SIZE; i++) {
		atomic_set(&dev->v4lvideo_file[i].on_use, false);
		dev->v4lvideo_file[i].free_before_unreg = false;
	}

	//dprintk(dev, 2, "vidioc_open\n");
	V4LVID_DBG("v4lvideo open\n");
	mutex_lock(&dev->mutex_opened);
	dev->opened = true;
	mutex_unlock(&dev->mutex_opened);
	v4l_print(dev->inst, PRINT_COUNT, "open\n");
	return 0;
}

static int vidioc_close(struct file *file)
{
	struct v4lvideo_dev *dev = video_drvdata(file);
	int i;
	u32 inst_id = dev->inst;

	V4LVID_DBG("vidioc_close!!!!\n");
	if (dev->mapped)
		v4lvideo_release_map_force(dev);

	for (i = 0; i < V4LVIDEO_POOL_SIZE; i++) {
		if (dev->v4lvideo_file[i].free_before_unreg) {
			vf_free_force(&dev->v4lvideo_file[i]);
			dev->v4lvideo_file[i].free_before_unreg = false;
		}
	}

	if (dev->fd_num > 0)
		dev->fd_num--;

	mutex_lock(&dev->mutex_opened);
	dev->opened = false;
	mutex_unlock(&dev->mutex_opened);

	v4l_print(dev->inst, PRINT_COUNT,
		"close get=%d, put=%d, release=%d, %d\n",
		total_get_count[inst_id], total_put_count[inst_id],
		total_release_count[inst_id],
		total_get_count[inst_id] - total_put_count[inst_id]
		- total_release_count[inst_id]);

	return 0;
}

static ssize_t vidioc_read(struct file *file, char __user *data,
			   size_t count, loff_t *ppos)
{
	pr_info("v4lvideo read\n");
	return 0;
}

static unsigned int v4lvideo_poll(struct file *file,
					struct poll_table_struct *wait)
{
	struct v4lvideo_dev *dev = video_drvdata(file);

	if (dev->receiver_register) {
		if (vf_peek(dev->vf_receiver_name))
			return POLL_IN | POLLRDNORM;
		else
			return 0;
	} else {
			return 0;
	}
}

static int vidioc_mmap(struct file *file, struct vm_area_struct *vma)
{
	pr_info("v4lvideo mmap\n");
	return 0;
}

static int vidioc_querycap(struct file *file,
			   void *priv,
			   struct v4l2_capability *cap)
{
	struct v4lvideo_dev *dev = video_drvdata(file);

	strcpy(cap->driver, "v4lvideo");
	strcpy(cap->card, "v4lvideo");
	snprintf(cap->bus_info,
		 sizeof(cap->bus_info),
		 "platform:%s",
		 dev->v4l2_dev.name);
	cap->device_caps = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING
				| V4L2_CAP_READWRITE;
	cap->capabilities = cap->device_caps | V4L2_CAP_DEVICE_CAPS;
	return 0;
}

static int vidioc_enum_fmt_vid_cap(struct file *file, void *priv,
				   struct v4l2_fmtdesc *f)
{
	const struct v4lvideo_fmt *fmt;

	if (f->index >= ARRAY_SIZE(formats))
		return -EINVAL;

	fmt = &formats[f->index];

	strlcpy(f->description, fmt->name, sizeof(f->description));
	f->pixelformat = fmt->fourcc;
	return 0;
}

static int vidioc_g_fmt_vid_cap(struct file *file, void *priv,
				struct v4l2_format *f)
{
	struct v4lvideo_dev *dev = video_drvdata(file);

	f->fmt.pix.width = dev->width;
	f->fmt.pix.height = dev->height;

	f->fmt.pix.field = V4L2_FIELD_INTERLACED;
	f->fmt.pix.pixelformat = dev->fmt->fourcc;
	f->fmt.pix.bytesperline = (f->fmt.pix.width * dev->fmt->depth) >> 3;
	f->fmt.pix.sizeimage = f->fmt.pix.height * f->fmt.pix.bytesperline;
	if (dev->fmt->is_yuv)
		f->fmt.pix.colorspace = V4L2_COLORSPACE_SMPTE170M;
	else
		f->fmt.pix.colorspace = V4L2_COLORSPACE_SRGB;

	return 0;
}

static int vidioc_try_fmt_vid_cap(struct file *file,
				  void *priv,
				  struct v4l2_format *f)
{
	struct v4lvideo_dev *dev = video_drvdata(file);
	const struct v4lvideo_fmt *fmt;

	fmt = get_format(f);
	if (!fmt) {
		dprintk(dev, 1, "Fourcc format (0x%08x) unknown.\n",
			f->fmt.pix.pixelformat);
		return -EINVAL;
	}

	f->fmt.pix.field = V4L2_FIELD_INTERLACED;
	v4l_bound_align_image(&f->fmt.pix.width,
			      48,
			      MAX_WIDTH,
			      4,
			      &f->fmt.pix.height,
			      32,
			      MAX_HEIGHT,
			      0,
			      0);
	f->fmt.pix.bytesperline = (f->fmt.pix.width * fmt->depth) >> 3;
	f->fmt.pix.sizeimage = f->fmt.pix.height * f->fmt.pix.bytesperline;
	if (fmt->is_yuv)
		f->fmt.pix.colorspace = V4L2_COLORSPACE_SMPTE170M;
	else
		f->fmt.pix.colorspace = V4L2_COLORSPACE_SRGB;
	f->fmt.pix.priv = 0;
	return 0;
}

static int vidioc_s_fmt_vid_cap(struct file *file, void *priv,
				struct v4l2_format *f)
{
	struct v4lvideo_dev *dev = video_drvdata(file);

	int ret = vidioc_try_fmt_vid_cap(file, priv, f);

	if (ret < 0)
		return ret;

	dev->fmt = get_format(f);
	dev->width = f->fmt.pix.width;
	dev->height = f->fmt.pix.height;
	if (dev->width == 0 || dev->height == 0)
		V4LVID_ERR("ion buffer w/h info is invalid!!!!!!!!!!!\n");

	return 0;
}

static void push_to_display_q(struct v4lvideo_dev *dev,
			      struct file_private_data *file_private_data)
{
	int i;

	for (i = 0; i < V4LVIDEO_POOL_SIZE; i++) {
		if (!atomic_read(&dev->v4lvideo_file[i].on_use))
			break;
	}

	if (i == V4LVIDEO_POOL_SIZE) {
		pr_err("v4lvideo: dq not find v4lvideo_file\n");
		return;
	}

	atomic_set(&dev->v4lvideo_file[i].on_use, true);
	file_private_data->private = (void *)&dev->v4lvideo_file[i];
	dev->v4lvideo_file[i].private_data_p = file_private_data;
	dev->v4lvideo_file[i].vf_p = file_private_data->vf_p;
	dev->v4lvideo_file[i].vf_ext_p = file_private_data->vf_ext_p;
	dev->v4lvideo_file[i].flag = file_private_data->flag;
	dev->v4lvideo_file[i].vf_type = file_private_data->vf_p->type;
	dev->v4lvideo_file[i].inst_id = file_private_data->v4l_inst_id;
	dev->v4lvideo_file[i].dev = dev;

	v4l2q_push(&dev->display_queue, &dev->v4lvideo_file[i]);
}

static struct v4lvideo_file_s *pop_from_display_q(struct v4lvideo_dev *dev)
{
	struct v4lvideo_file_s *v4lvideo_file;

	v4lvideo_file = v4l2q_pop(&dev->display_queue);
	if (!v4lvideo_file)
		return NULL;

	atomic_set(&v4lvideo_file->on_use, false);
	return v4lvideo_file;
}

static bool pop_specific_from_display_q(struct v4lvideo_dev *dev,
					struct v4lvideo_file_s *v4lvideo_file)
{
	bool ret;

	ret = v4l2q_pop_specific(&dev->display_queue, v4lvideo_file);
	if (ret)
		atomic_set(&v4lvideo_file->on_use, false);
	return ret;
}

static void v4lvideo_vf_put(struct v4lvideo_dev *dev, struct vframe_s *vf)
{
	bool is_di_pw = false;

	if (vf->type & VIDTYPE_DI_PW)
		is_di_pw = true;

	if (vf_put(vf, dev->vf_receiver_name) < 0) {
		pr_err("v4lvideo: put err!!!\n");
		if (is_di_pw) {
			pr_err("v4lvideo: put err, release di vf\n");
			v4l_print(dev->inst, PRINT_OTHER,
				"put: release omx_index=%d\n", vf->omx_index);
			dim_post_keep_cmd_release2(vf);
			total_release_count[dev->inst]++;
		}
	} else {
		put_count[dev->inst]++;
		if (is_di_pw)
			total_put_count[dev->inst]++;
	}
}

static int vidioc_qbuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	struct v4lvideo_dev *dev = video_drvdata(file);
	struct vframe_s *vf_p;
	struct vframe_s *vf_ext_p;
	struct file *file_vf = NULL;
	struct file_private_data *file_private_data = NULL;
	u32 flag;
	struct v4lvideo_file_s *v4lvideo_file;
	u32 inst_id = dev->inst;

	if (p->index >= V4LVIDEO_POOL_SIZE) {
		pr_err("ionvideo: dbuf: err index=%d\n", p->index);
		return -EINVAL;
	}

	dev->v4lvideo_input[p->index] = *p;

	q_count[inst_id]++;
	file_vf = fget(p->m.fd);
	if (!file_vf) {
		pr_err("v4lvideo: qbuf fget fail\n");
		return 0;
	}

	file_private_data = v4lvideo_get_file_private_data(file_vf, true);
	if (!file_private_data) {
		pr_err("v4lvideo: qbuf file_private_data NULL\n");
		fput(file_vf);
		return 0;
	}

	vf_p = file_private_data->vf_p;
	vf_ext_p = file_private_data->vf_ext_p;
	flag = file_private_data->flag;
	v4lvideo_file = (struct v4lvideo_file_s *)file_private_data->private;

	mutex_lock(&dev->mutex_input);
	if (vf_p) {
		if (!v4lvideo_file) {
			pr_err("%s: v4lvideo_file NULL\n", __func__);
			mutex_unlock(&dev->mutex_input);
			return 0;
		}
		if (file_private_data->is_keep) {
			vf_free(file_private_data);
		} else {
			if (pop_specific_from_display_q(dev, v4lvideo_file)) {
				active_file_list_pop(dev, file_private_data, false);
				if (dev->receiver_register) {
					if (flag & V4LVIDEO_FLAG_DI_DEC)
						vf_p = vf_ext_p;
					v4lvideo_vf_put(dev, vf_p);
				} else {
					vf_free(file_private_data);
					pr_err("%s: vfm is unreg\n", __func__);
				}
			} else {
				pr_err("%s: maybe in unreg\n", __func__);
				if (v4lvideo_file->vf_type & VIDTYPE_DI_PW) {
					dim_post_keep_cmd_release2(vf_p);
					total_release_count[inst_id]++;
					v4l_print(inst_id, PRINT_COUNT,
						"qbuf: get=%d, put=%d, release=%d\n",
						total_get_count[inst_id],
						total_put_count[inst_id],
						total_release_count[inst_id]);
				}
			}
		}
	} else {
		dprintk(dev, 1,
			"%s: vf is NULL, at the start of playback\n", __func__);
	}

	v4lvideo_release_sei_data(&file_private_data->vf);
	init_file_private_data(file_private_data);
	fput(file_vf);

	v4l2q_push(&dev->input_queue, &dev->v4lvideo_input[p->index]);
	mutex_unlock(&dev->mutex_input);

	return 0;
}

static void canvas_to_addr(struct vframe_s *vf)
{
	struct canvas_s src_cs0, src_cs1, src_cs2;

	if (vf->canvas0Addr == (u32)-1)
		return;

	canvas_read(vf->canvas0Addr & 0xff, &src_cs0);
	canvas_read(vf->canvas0Addr >> 8 & 0xff, &src_cs1);
	canvas_read(vf->canvas0Addr >> 16 & 0xff, &src_cs2);

	vf->canvas0_config[0].phy_addr = src_cs0.addr;
	vf->canvas0_config[0].width = src_cs0.width;
	vf->canvas0_config[0].height = src_cs0.height;
	vf->canvas0_config[0].block_mode = src_cs0.blkmode;
	vf->canvas0_config[0].endian = src_cs0.endian;

	vf->canvas0_config[1].phy_addr = src_cs1.addr;
	vf->canvas0_config[1].width = src_cs1.width;
	vf->canvas0_config[1].height = src_cs1.height;
	vf->canvas0_config[1].block_mode = src_cs1.blkmode;
	vf->canvas0_config[1].endian = src_cs1.endian;

	vf->canvas0_config[2].phy_addr = src_cs2.addr;
	vf->canvas0_config[2].width = src_cs2.width;
	vf->canvas0_config[2].height = src_cs2.height;
	vf->canvas0_config[2].block_mode = src_cs2.blkmode;
	vf->canvas0_config[2].endian = src_cs2.endian;

	if ((vf->type & VIDTYPE_MVC) &&
	    (vf->canvas1Addr != (u32)-1)) {
		canvas_read(vf->canvas1Addr & 0xff, &src_cs0);
		canvas_read(vf->canvas1Addr >> 8 & 0xff, &src_cs1);
		canvas_read(vf->canvas1Addr >> 16 & 0xff, &src_cs2);

		vf->canvas1_config[0].phy_addr = src_cs0.addr;
		vf->canvas1_config[0].width = src_cs0.width;
		vf->canvas1_config[0].height = src_cs0.height;
		vf->canvas1_config[0].block_mode = src_cs0.blkmode;
		vf->canvas1_config[0].endian = src_cs0.endian;

		vf->canvas1_config[1].phy_addr = src_cs1.addr;
		vf->canvas1_config[1].width = src_cs1.width;
		vf->canvas1_config[1].height = src_cs1.height;
		vf->canvas1_config[1].block_mode = src_cs1.blkmode;
		vf->canvas1_config[1].endian = src_cs1.endian;

		vf->canvas1_config[2].phy_addr = src_cs2.addr;
		vf->canvas1_config[2].width = src_cs2.width;
		vf->canvas1_config[2].height = src_cs2.height;
		vf->canvas1_config[2].block_mode = src_cs2.blkmode;
		vf->canvas1_config[2].endian = src_cs2.endian;
	}

	if (vf->type & VIDTYPE_VIU_NV21)
		vf->plane_num = 2;
	else if (vf->type & VIDTYPE_VIU_444)
		vf->plane_num = 1;
	else if (vf->type & VIDTYPE_VIU_422)
		vf->plane_num = 1;
	else
		vf->plane_num = 3;

	vf->canvas0Addr = (u32)-1;
	vf->canvas1Addr = (u32)-1;
}

static int vidioc_dqbuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	struct v4lvideo_dev *dev = video_drvdata(file);
	struct v4l2_buffer *buf = NULL;
	struct vframe_s *vf;
	struct file *file_vf = NULL;
	struct file_private_data *file_private_data = NULL;
	u64 pts_us64 = 0;
	u64 pts_tmp;
	char *provider_name = NULL;
	struct vframe_s *vf_ext = NULL;
	u32 inst_id = dev->inst;

	mutex_lock(&dev->mutex_input);

	if (!dev->receiver_register) {
		mutex_unlock(&dev->mutex_input);
		return -EAGAIN;
	}

	buf = v4l2q_peek(&dev->input_queue);
	if (!buf) {
		dprintk(dev, 3, "No active queue to serve\n");
		mutex_unlock(&dev->mutex_input);
		return -EAGAIN;
	}

	vf = vf_peek(dev->vf_receiver_name);
	if (!vf) {
		dev->vf_wait_cnt++;
		mutex_unlock(&dev->mutex_input);
		return -EAGAIN;
	}
	vf = vf_get(dev->vf_receiver_name);
	if (!vf) {
		mutex_unlock(&dev->mutex_input);
		return -EAGAIN;
	}
	if (vf->type & VIDTYPE_DI_PW)
		total_get_count[inst_id]++;

	if (vf->type & VIDTYPE_V4L_EOS) {
		v4lvideo_vf_put(dev, vf);
		mutex_unlock(&dev->mutex_input);
		return -EAGAIN;
	}

	if (!dev->provider_name) {
		provider_name = vf_get_provider_name(dev->vf_receiver_name);
		while (provider_name) {
			if (!vf_get_provider_name(provider_name))
				break;
			provider_name =
				vf_get_provider_name(provider_name);
		}
		dev->provider_name = provider_name;
		pr_info("v4lvideo: provider name: %s\n",
			dev->provider_name ? dev->provider_name : "NULL");
	}

	get_count[inst_id]++;
	vf->omx_index = dev->frame_num;
	dev->am_parm.signal_type = vf->signal_type;
	dev->am_parm.master_display_colour =
			 vf->prop.master_display_colour;
	if (vf->hdr10p_data_size > 0 && vf->hdr10p_data_buf) {
		if (vf->hdr10p_data_size <= 128) {
			dev->am_parm.hdr10p_data_size =
					vf->hdr10p_data_size;
			memcpy(dev->am_parm.hdr10p_data_buf,
					vf->hdr10p_data_buf,
					vf->hdr10p_data_size);
		} else {
			pr_info("v4lvideo: hdr10+ data size is %d, skip it!\n",
					vf->hdr10p_data_size);
			dev->am_parm.hdr10p_data_size = 0;
		}
	} else {
		dev->am_parm.hdr10p_data_size = 0;
	}

	buf = v4l2q_pop(&dev->input_queue);
	if (!buf) {
		pr_err("pop buf is NULL\n");
		v4lvideo_vf_put(dev, vf);
		mutex_unlock(&dev->mutex_input);
		return -EAGAIN;
	}
	dev->vf_wait_cnt = 0;
	file_vf = fget(buf->m.fd);
	if (!file_vf) {
		mutex_unlock(&dev->mutex_input);
		pr_err("v4lvideo: dqbuf fget fail\n");
		return -EAGAIN;
	}

	file_private_data = v4lvideo_get_file_private_data(file_vf, false);
	if (!file_private_data) {
		v4lvideo_vf_put(dev, vf);
		mutex_unlock(&dev->mutex_input);
		fput(file_vf);
		pr_err("v4lvideo: file_private_data NULL\n");
		return -EAGAIN;
	}

	if (vf->flag & VFRAME_FLAG_DOUBLE_FRAM) {
		vf_ext = (struct vframe_s *)vf->vf_ext;
		vf_ext->omx_index = vf->omx_index;
		if (render_use_dec) {
			file_private_data->vf = *vf_ext;
			file_private_data->vf_p = vf_ext;
			file_private_data->vf_ext = *vf;
			file_private_data->vf_ext_p = vf;
			file_private_data->flag |= V4LVIDEO_FLAG_DI_DEC;
			canvas_to_addr(&file_private_data->vf);
			canvas_to_addr(&file_private_data->vf_ext);
		} else {
			file_private_data->vf = *vf;
			file_private_data->vf_p = vf;
			file_private_data->vf_ext = *vf_ext;
			file_private_data->vf_ext_p = vf_ext;
			file_private_data->flag |= V4LVIDEO_FLAG_DI_NR;
			canvas_to_addr(&file_private_data->vf);
			canvas_to_addr(&file_private_data->vf_ext);
			file_private_data->vf.canvas0_config[0].block_mode = 0;
			file_private_data->vf.canvas0_config[0].endian = 0;
			file_private_data->vf.vf_ext =
				&file_private_data->vf_ext;
		}
		file_private_data->vf.vf_ext = &file_private_data->vf_ext;
	} else {
		file_private_data->vf = *vf;
		file_private_data->vf_p = vf;
		canvas_to_addr(&file_private_data->vf);
	}

	file_private_data->vf.src_fmt.md_buf = file_private_data->md.p_md;
	file_private_data->vf.src_fmt.comp_buf = file_private_data->md.p_comp;
	file_private_data->v4l_inst_id = dev->inst;
	file_private_data->vf.src_fmt.dv_id = dev->dv_inst;
	if ((alloc_sei & 2) && vf)
		pr_info("vidioc dqbuf: vf %p(index %d), type %x, md_buf %p\n",
			vf, vf->omx_index, vf->type, file_private_data->vf.src_fmt.md_buf);

	v4lvideo_import_sei_data(vf,
		&file_private_data->vf,
		dev->provider_name);

	//pr_err("dqbuf: file_private_data=%p, vf=%p\n", file_private_data, vf);

	push_to_display_q(dev, file_private_data);
	active_file_list_push(dev, file_private_data);

	fput(file_vf);

	if (vf->pts_us64) {
		dev->first_frame = 1;
		pts_us64 = vf->pts_us64;
	} else if (dev->first_frame == 0) {
		dev->first_frame = 1;
		pts_us64 = 0;
	} else {
		pts_tmp = dur2pts(vf->duration) * 100;
		do_div(pts_tmp, 9);
		pts_us64 = dev->last_pts_us64
			+ pts_tmp;
		pts_tmp = pts_us64 * 9;
		do_div(pts_tmp, 100);
		vf->pts = pts_tmp;
	}
	/*workrun for decoder i pts err, if decoder fix it, this should remove*/
	if ((!(vf->flag & VFRAME_FLAG_DOUBLE_FRAM)) &&
		(vf->type & VIDTYPE_DI_PW || vf->type & VIDTYPE_INTERLACE) &&
		vf->pts_us64 == dev->last_pts_us64) {
		dprintk(dev, 1, "pts same\n");
		pts_tmp = dur2pts(vf->duration) * 100;
		do_div(pts_tmp, 9);
		pts_us64 = dev->last_pts_us64
			+ pts_tmp;
		pts_tmp = pts_us64 * 9;
		do_div(pts_tmp, 100);
		vf->pts = pts_tmp;
	}

	*p = *buf;
	 p->timestamp.tv_sec = pts_us64 >> 32;
	 p->timestamp.tv_usec = pts_us64 & 0xFFFFFFFF;
	 dev->last_pts_us64 = pts_us64;

	if ((vf->type & VIDTYPE_COMPRESS) != 0) {
		p->timecode.type = vf->compWidth;
		p->timecode.flags = vf->compHeight;
	} else {
		p->timecode.type = vf->width;
		p->timecode.flags = vf->height;
	}
	p->sequence = dev->frame_num++;

	if (vf->type_original & VIDTYPE_INTERLACE || vf->type & VIDTYPE_INTERLACE)
		p->field = V4L2_FIELD_INTERLACED;

	mutex_unlock(&dev->mutex_input);
	//pr_err("dqbuf: frame_num=%d\n", p->sequence);
	dq_count[inst_id]++;
	v4l_print(dev->inst, PRINT_OTHER,
		"v4lvideo: %s return vf:%p, index:%d, pts_us64:0x%llx video_id:%d\n",
		__func__, vf, vf->index, vf->pts_us64,
		vf->vf_ud_param.ud_param.instance_id);
	return 0;
}

/* ------------------------------------------------------------------
 * File operations for the device
 * ------------------------------------------------------------------
 */
static const struct v4l2_file_operations v4lvideo_v4l2_fops = {
	.owner = THIS_MODULE,
	.open = vidioc_open,
	.release = vidioc_close,
	.read = vidioc_read,
	.poll = v4lvideo_poll,
	.unlocked_ioctl = video_ioctl2,/* V4L2 ioctl handler */
	.mmap = vidioc_mmap,
};

static const struct v4l2_ioctl_ops v4lvideo_ioctl_ops = {
	.vidioc_querycap = vidioc_querycap,
	.vidioc_enum_fmt_vid_cap = vidioc_enum_fmt_vid_cap,
	.vidioc_g_fmt_vid_cap = vidioc_g_fmt_vid_cap,
	.vidioc_try_fmt_vid_cap = vidioc_try_fmt_vid_cap,
	.vidioc_s_fmt_vid_cap = vidioc_s_fmt_vid_cap,
	.vidioc_qbuf = vidioc_qbuf,
	.vidioc_dqbuf = vidioc_dqbuf,
	.vidioc_streamon = vidioc_streamon,
	.vidioc_streamoff = vidioc_streamoff,
	.vidioc_g_parm = vidioc_g_parm,
};

static const struct video_device v4lvideo_template = {
	.name = "v4lvideo",
	.fops = &v4lvideo_v4l2_fops,
	.ioctl_ops = &v4lvideo_ioctl_ops,
	.release = video_device_release,
};

static int v4lvideo_v4l2_release(void)
{
	struct v4lvideo_dev *dev;
	struct list_head *list;
	unsigned long flags;

	flags = v4lvideo_devlist_lock();

	while (!list_empty(&v4lvideo_devlist)) {
		list = v4lvideo_devlist.next;
		list_del(list);
		v4lvideo_devlist_unlock(flags);

		dev = list_entry(list, struct v4lvideo_dev, v4lvideo_devlist);

		v4l2_info(&dev->v4l2_dev,
			  "unregistering %s\n",
			  video_device_node_name(&dev->vdev));
		video_unregister_device(&dev->vdev);
		v4l2_device_unregister(&dev->v4l2_dev);
		kfree(dev);

		flags = v4lvideo_devlist_lock();
	}
	/* vb2_dma_contig_cleanup_ctx(v4lvideo_dma_ctx); */

	v4lvideo_devlist_unlock(flags);

	return 0;
}

static int video_receiver_event_fun(int type, void *data, void *private_data)
{
	struct v4lvideo_dev *dev = (struct v4lvideo_dev *)private_data;
	struct file_private_data *file_private_data = NULL;
	struct v4lvideo_file_s *v4lvideo_file;
	u32 inst_id = dev->inst;

	if (type == VFRAME_EVENT_PROVIDER_UNREG) {
		dev->receiver_register = false;
		mutex_lock(&dev->mutex_input);
		while (!v4l2q_empty(&dev->display_queue)) {
			v4lvideo_file = pop_from_display_q(dev);
			if (!v4lvideo_file) {
				mutex_unlock(&dev->mutex_input);
				pr_err("v4lvideo: unreg pop fail\n");
				return 0;
			}
			file_private_data = v4lvideo_file->private_data_p;
			vf_keep(dev, v4lvideo_file, file_private_data);
			/*pr_err("unreg:v4lvideo, keep last frame\n");*/
		}
		mutex_unlock(&dev->mutex_input);
		pr_err("unreg:v4lvideo inst=%d\n", dev->inst);
		v4l_print(dev->inst, PRINT_COUNT,
			"unreg get=%d, put=%d, release=%d\n",
			total_get_count[inst_id], total_put_count[inst_id],
			total_release_count[inst_id]);
	} else if (type == VFRAME_EVENT_PROVIDER_REG) {
		mutex_lock(&dev->mutex_input);
		v4l2q_init(&dev->display_queue,
			   V4LVIDEO_POOL_SIZE,
			   (void **)&dev->v4lvideo_display_queue[0]);
		dev->frame_num = 0;
		dev->first_frame = 0;
		dev->last_pts_us64 = U64_MAX;
		init_active_file_list(dev);
		mutex_unlock(&dev->mutex_input);
		get_count[inst_id] = 0;
		put_count[inst_id] = 0;
		q_count[inst_id] = 0;
		dq_count[inst_id] = 0;
		pr_err("reg:v4lvideo inst=%d\n", dev->inst);
		dev->receiver_register = true;
	} else if (type == VFRAME_EVENT_PROVIDER_QUREY_STATE) {
		if (dev->vf_wait_cnt > 1) {
			if (!inactive_check_disp)
				return RECEIVER_INACTIVE;
			else if (v4l2q_empty(&dev->display_queue))
				return RECEIVER_INACTIVE;
		}
		return RECEIVER_ACTIVE;
	}
	return 0;
}

static const struct vframe_receiver_op_s video_vf_receiver = {
	.event_cb = video_receiver_event_fun
};

static int __init v4lvideo_create_instance(int inst)
{
	struct v4lvideo_dev *dev;
	struct video_device *vfd;
	int ret;
	unsigned long flags;

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	snprintf(dev->v4l2_dev.name,
		 sizeof(dev->v4l2_dev.name),
		 "%s-%03d",
		 V4LVIDEO_MODULE_NAME,
		 inst);
	ret = v4l2_device_register(NULL, &dev->v4l2_dev);
	if (ret)
		goto free_dev;

	dev->fmt = &formats[0];
	dev->width = 640;
	dev->height = 480;
	dev->fd_num = 0;

	vfd = &dev->vdev;
	*vfd = v4lvideo_template;
	vfd->dev_debug = debug;
	vfd->v4l2_dev = &dev->v4l2_dev;
	vfd->device_caps = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING
				| V4L2_CAP_READWRITE;

	/*
	 * Provide a mutex to v4l2 core. It will be used to protect
	 * all fops and v4l2 ioctls.
	 */
	ret = video_register_device(vfd,
				    VFL_TYPE_GRABBER,
				    inst + video_nr_base);
	if (ret < 0)
		goto unreg_dev;

	video_set_drvdata(vfd, dev);
	dev->inst = inst;
	snprintf(dev->vf_receiver_name,
		 ION_VF_RECEIVER_NAME_SIZE,
		 RECEIVER_NAME ".%x",
		 inst & 0xff);

	vf_receiver_init(&dev->video_vf_receiver,
			 dev->vf_receiver_name,
			 &video_vf_receiver, dev);
	vf_reg_receiver(&dev->video_vf_receiver);
	if (inst == 0 || inst == n_devs - 1)
		v4l2_dbg(0, debug, &dev->v4l2_dev,
			  "V4L2 device registered as %s\n",
			  video_device_node_name(vfd));

	/* add to device list */
	flags = v4lvideo_devlist_lock();
	list_add_tail(&dev->v4lvideo_devlist, &v4lvideo_devlist);
	v4lvideo_devlist_unlock(flags);

	mutex_init(&dev->mutex_input);
	mutex_init(&dev->mutex_opened);

	return 0;

unreg_dev:
	v4l2_device_unregister(&dev->v4l2_dev);
free_dev:
	kfree(dev);
	return ret;
}

static int v4lvideo_file_release(struct inode *inode, struct file *file)
{
	struct file_private_data *file_private_data = file->private_data;
	/*pr_err("v4lvideo_file_release\n");*/

	if (file_private_data) {
		if (file_private_data->cnt_file) {
			fput(file_private_data->cnt_file);
			link_put_fd_count++;
			pr_debug("v4lvideo: %s pre file: %p\n",
				__func__, file_private_data->cnt_file);
			file_private_data->cnt_file = NULL;
		} else {
			if (file_private_data->is_keep)
				vf_free(file_private_data);
			v4lvideo_release_sei_data(&file_private_data->vf);
			if (file_private_data->md.p_md)
				vfree(file_private_data->md.p_md);
			if (file_private_data->md.p_comp)
				vfree(file_private_data->md.p_comp);
		}
		file_private_data->md.p_md = NULL;
		file_private_data->md.p_comp = NULL;

		memset(file_private_data, 0, sizeof(struct file_private_data));
		v4lvideo_private_data_release(file_private_data);
		kfree((u8 *)file_private_data);
		file->private_data = NULL;
		release_fd_count++;
	}
	return 0;
}

static const struct file_operations v4lvideo_file_fops = {
	.release = v4lvideo_file_release,
	//.poll = v4lvideo_file_poll,
	//.unlocked_ioctl = v4lvideo_file_ioctl,
	//.compat_ioctl = v4lvideo_file_ioctl,
};

int is_v4lvideo_buf_file(struct file *file)
{
	return file->f_op == &v4lvideo_file_fops;
}

struct file *v4lvideo_alloc_file(void)
{
	struct file *file = NULL;
	struct file_private_data *private_data = NULL;

	private_data = kzalloc(sizeof(*private_data), GFP_KERNEL);
	if (!private_data)
		return NULL;

	init_file_private_data(private_data);

	file = anon_inode_getfile("v4lvideo_file",
				  &v4lvideo_file_fops,
				  private_data, 0);
	if (IS_ERR(file)) {
		kfree((u8 *)private_data);
		pr_err("%s: anon_inode_getfile fail\n", __func__);
		return NULL;
	}

	private_data->md.p_md  = vmalloc(MD_BUF_SIZE);
	if (!private_data->md.p_md) {
		kfree((u8 *)private_data);
		return NULL;
	}

	private_data->md.p_comp = vmalloc(COMP_BUF_SIZE);
	if (!private_data->md.p_comp) {
		if (private_data->md.p_md) {
			vfree(private_data->md.p_md);
			private_data->md.p_md = NULL;
		}
		kfree((u8 *)private_data);
		return NULL;
	}

	return file;
}

int v4lvideo_alloc_fd(int *fd)
{
	struct file *file = NULL;
	struct file_private_data *private_data = NULL;
	int file_fd = get_unused_fd_flags(O_CLOEXEC);

	if (file_fd < 0) {
		pr_err("%s: get unused fd fail\n", __func__);
		return -ENODEV;
	}

	private_data = kzalloc(sizeof(*private_data), GFP_KERNEL);
	if (!private_data) {
		put_unused_fd(file_fd);
		pr_err("%s: private_date fail\n", __func__);
		return -ENOMEM;
	}
	init_file_private_data(private_data);

	file = anon_inode_getfile("v4lvideo_file",
				  &v4lvideo_file_fops,
				  private_data, 0);
	if (IS_ERR(file)) {
		kfree((u8 *)private_data);
		put_unused_fd(file_fd);
		pr_err("%s: anon_inode_getfile fail\n", __func__);
		return -ENODEV;
	}

	private_data->md.p_md  = vmalloc(MD_BUF_SIZE);
	if (!private_data->md.p_md) {
		kfree((u8 *)private_data);
		put_unused_fd(file_fd);
		return -ENOMEM;
	}

	private_data->md.p_comp = vmalloc(COMP_BUF_SIZE);
	if (!private_data->md.p_comp) {
		if (private_data->md.p_md) {
			vfree(private_data->md.p_md);
			private_data->md.p_md = NULL;
		}
		kfree((u8 *)private_data);
		put_unused_fd(file_fd);
		return -ENOMEM;
	}

	fd_install(file_fd, file);
	*fd = file_fd;
	open_fd_count++;
	return 0;
}

static int v4lvideo_fd_link(int src_fd, int dst_fd)
{
	struct file *file0 = NULL;
	struct file *file1 = NULL;
	struct file_private_data *private_data0 = NULL;
	struct file_private_data *private_data1 = NULL;

	if (src_fd == dst_fd) {
		pr_err("%s:invalid param: src_fd:%d, dst_fd:%d.\n", __func__, src_fd, dst_fd);
		return -EINVAL;
	}

	file0 = fget(src_fd);
	if (!file0) {
		pr_err("v4lvideo: %s source file is NULL\n", __func__);
		return -EINVAL;
	}
	file1 = fget(dst_fd);
	if (!file1) {
		fput(file0);
		pr_err("v4lvideo: %s dst file is NULL\n", __func__);
		return -EINVAL;
	}

	if (!is_v4lvideo_buf_file(file0) || !is_v4lvideo_buf_file(file1)) {
		pr_err("%s: file is not v4lvideo.\n", __func__);
		fput(file0);
		fput(file1);
		return -EINVAL;
	}

	private_data0 = (struct file_private_data *)(file0->private_data);
	private_data1 = (struct file_private_data *)(file1->private_data);

	if (private_data1->cnt_file) {
		fput(private_data1->cnt_file);
		link_put_fd_count++;
	} else {
		if (private_data1->md.p_md) {
			vfree(private_data1->md.p_md);
			private_data1->md.p_md = NULL;
		}
		if (private_data1->md.p_comp) {
			vfree(private_data1->md.p_comp);
			private_data1->md.p_comp = NULL;
		}
	}

	*private_data1 = *private_data0;
	private_data1->cnt_file = file0;
	fput(file1);
	link_fd_count++;
	return 0;
}
static ssize_t sei_cnt_show(struct class *class,
			    struct class_attribute *attr,
			    char *buf)
{
	ssize_t r;
	int cnt;

	cnt = atomic_read(&global_set_cnt);
	r = sprintf(buf, "allocated sei buffer cnt: %d\n", cnt);
	return r;
}

static ssize_t sei_cnt_store(struct class *class,
			     struct class_attribute *attr,
			     const char *buf, size_t count)
{
	ssize_t r;
	int val;

	r = kstrtoint(buf, 0, &val);
	if (r < 0)
		return -EINVAL;

	pr_info("set sei_cnt val:%d\n", val);
	atomic_set(&global_set_cnt, val);
	return count;
}

static ssize_t alloc_sei_debug_show(struct class *class,
				    struct class_attribute *attr,
				    char *buf)
{
	return sprintf(buf, "alloc sei: %d\n", alloc_sei);
}

static ssize_t alloc_sei_debug_store(struct class *class,
				     struct class_attribute *attr,
				     const char *buf, size_t count)
{
	ssize_t r;
	int val;

	r = kstrtoint(buf, 0, &val);
	if (r < 0)
		return -EINVAL;

	if (val > 0)
		alloc_sei = val;
	else
		alloc_sei = 0;
	pr_info("set alloc_sei val:%d\n", alloc_sei);
	return count;
}

static ssize_t video_nr_base_show(struct class *cla,
				  struct class_attribute *attr,
				  char *buf)
{
	return snprintf(buf, 80,
			"current video_nr_base is %d\n",
			video_nr_base);
}

static ssize_t video_nr_base_store(struct class *cla,
				   struct class_attribute *attr,
				   const char *buf, size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0)
		return ret;
	video_nr_base = tmp;
	return count;
}

static ssize_t n_devs_show(struct class *cla,
			   struct class_attribute *attr,
			   char *buf)
{
	return snprintf(buf, 80,
			"current n_devs is %d\n",
			n_devs);
}

static ssize_t n_devs_store(struct class *cla,
			    struct class_attribute *attr,
			    const char *buf, size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	n_devs = tmp;
	return count;
}

static ssize_t debug_show(struct class *cla,
			  struct class_attribute *attr,
			  char *buf)
{
	return snprintf(buf, 80,
			"current debug is %d\n",
			debug);
}

static ssize_t debug_store(struct class *cla,
			   struct class_attribute *attr,
			   const char *buf, size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	debug = tmp;
	return count;
}

static ssize_t get_count_show(struct class *cla,
			      struct class_attribute *attr,
			      char *buf)
{
	return snprintf(buf, 80, "get_count: %d,%d,%d,%d,%d,%d,%d,%d,%d\n",
		get_count[0], get_count[1], get_count[2],
		get_count[3], get_count[4], get_count[5],
		get_count[6], get_count[7], get_count[8]);
}

static ssize_t get_count_store(struct class *cla,
			       struct class_attribute *attr,
			       const char *buf, size_t count)
{
	long tmp;
	int ret;
	int i;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	for (i = 0; i < N_DEVS; i++) {
		if (tmp > 0)
			get_count[i] = tmp;
		else
			get_count[i] = 0;
	}
	return count;
}

static ssize_t put_count_show(struct class *cla,
			      struct class_attribute *attr,
			      char *buf)
{
	return snprintf(buf, 80, "put_count: %d,%d,%d,%d,%d,%d,%d,%d,%d\n",
		put_count[0], put_count[1], put_count[2],
		put_count[3], put_count[4], put_count[5],
		put_count[6], put_count[7], put_count[8]);
}

static ssize_t put_count_store(struct class *cla,
			       struct class_attribute *attr,
			       const char *buf, size_t count)
{
	long tmp;
	int ret;
	int i;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	for (i = 0; i < N_DEVS; i++) {
		if (tmp > 0)
			put_count[i] = tmp;
		else
			put_count[i] = 0;
	}
	return count;
}

static ssize_t q_count_show(struct class *class,
			    struct class_attribute *attr, char *buf)
{
	return snprintf(buf, 80, "q_count: %d,%d,%d,%d,%d,%d,%d,%d,%d\n",
		q_count[0], q_count[1], q_count[2],
		q_count[3], q_count[4], q_count[5],
		q_count[6], q_count[7], q_count[8]);
}

static ssize_t q_count_store(struct class *class,
			     struct class_attribute *attr,
			     const char *buf, size_t count)
{
	ssize_t r;
	int val;
	int i;

	r = kstrtoint(buf, 0, &val);
	if (r < 0)
		return -EINVAL;

	for (i = 0; i < N_DEVS; i++) {
		if (val > 0)
			put_count[i] = val;
		else
			put_count[i] = 0;
	}
	return count;
}

static ssize_t dq_count_show(struct class *class,
			     struct class_attribute *attr, char *buf)
{
	return snprintf(buf, 80, "dq_count: %d,%d,%d,%d,%d,%d,%d,%d,%d\n",
		dq_count[0], dq_count[1], dq_count[2],
		dq_count[3], dq_count[4], dq_count[5],
		dq_count[6], dq_count[7], dq_count[8]);
}

static ssize_t dq_count_store(struct class *class,
			      struct class_attribute *attr,
			      const char *buf, size_t count)
{
	ssize_t r;
	int val;
	int i;

	r = kstrtoint(buf, 0, &val);
	if (r < 0)
		return -EINVAL;

	for (i = 0; i < N_DEVS; i++) {
		if (val > 0)
			put_count[i] = val;
		else
			put_count[i] = 0;
	}
	return count;
}

static ssize_t cts_use_di_show(struct class *class,
			       struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "cts_use_di: %d\n", cts_use_di);
}

static ssize_t cts_use_di_store(struct class *class,
				struct class_attribute *attr,
				const char *buf, size_t count)
{
	ssize_t r;
	int val;

	r = kstrtoint(buf, 0, &val);
	if (r < 0)
		return -EINVAL;

	if (val > 0)
		cts_use_di = val;
	else
		cts_use_di = 0;
	return count;
}

static ssize_t render_use_dec_show(struct class *class,
				   struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "render_use_dec: %d\n", render_use_dec);
}

static ssize_t render_use_dec_store(struct class *class,
				    struct class_attribute *attr,
				    const char *buf, size_t count)
{
	ssize_t r;
	int val;

	r = kstrtoint(buf, 0, &val);
	if (r < 0)
		return -EINVAL;

	if (val > 0)
		render_use_dec = val;
	else
		render_use_dec = 0;
	return count;
}

static ssize_t dec_count_show(struct class *class,
			      struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "dec_count: %d\n", dec_count);
}

static ssize_t dec_count_store(struct class *class,
			       struct class_attribute *attr,
			       const char *buf, size_t count)
{
	ssize_t r;
	int val;

	r = kstrtoint(buf, 0, &val);
	if (r < 0)
		return -EINVAL;

	if (val > 0)
		dec_count = val;
	else
		dec_count = 0;
	return count;
}

static ssize_t vf_dump_show(struct class *cla,
			    struct class_attribute *attr,
			    char *buf)
{
	return snprintf(buf, 80,
			"current vf_dump is %d\n",
			vf_dump);
}

static ssize_t vf_dump_store(struct class *cla,
			     struct class_attribute *attr,
			     const char *buf, size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0)
		return ret;

	vf_dump = tmp;
	return count;
}

static ssize_t open_fd_count_show(struct class *cla,
			    struct class_attribute *attr,
			    char *buf)
{
	return snprintf(buf, 80, "open_fd_count is %d\n", open_fd_count);
}

static ssize_t release_fd_count_show(struct class *cla,
			    struct class_attribute *attr,
			    char *buf)
{
	return snprintf(buf, 80, "release_fd_count is %d\n", release_fd_count);
}

static ssize_t link_fd_count_show(struct class *cla,
			    struct class_attribute *attr,
			    char *buf)
{
	return snprintf(buf, 80, "link_fd_count is %d\n", link_fd_count);
}

static ssize_t link_put_fd_count_show(struct class *cla,
			    struct class_attribute *attr,
			    char *buf)
{
	return snprintf(buf, 80, "link_put_fd_count is %d\n", link_put_fd_count);
}

static ssize_t v4lvideo_version_show(struct class *cla,
			    struct class_attribute *attr,
			    char *buf)
{
	return snprintf(buf, 80, "v4lvideo_version is %d\n", v4lvideo_version);
}

static ssize_t total_get_count_show(struct class *cla,
			    struct class_attribute *attr,
			    char *buf)
{
	return snprintf(buf, 80, "total_get_count: %d,%d,%d,%d,%d,%d,%d,%d,%d\n",
		total_get_count[0], total_get_count[1], total_get_count[2],
		total_get_count[3], total_get_count[4], total_get_count[5],
		total_get_count[6], total_get_count[7], total_get_count[8]);
}

static ssize_t total_put_count_show(struct class *cla,
			    struct class_attribute *attr,
			    char *buf)
{
	return snprintf(buf, 80, "total_put_count: %d,%d,%d,%d,%d,%d,%d,%d,%d\n",
		total_put_count[0], total_put_count[1], total_put_count[2],
		total_put_count[3], total_put_count[4], total_put_count[5],
		total_put_count[6], total_put_count[7], total_put_count[8]);
}

static ssize_t total_release_count_show(struct class *cla,
			    struct class_attribute *attr,
			    char *buf)
{
	return snprintf(buf, 80, "total_release_count: %d,%d,%d,%d,%d,%d,%d,%d,%d\n",
		total_release_count[0], total_release_count[1],
		total_release_count[2], total_release_count[3],
		total_release_count[4], total_release_count[5],
		total_release_count[6], total_release_count[7],
		total_release_count[8]);
}

static ssize_t print_flag_show(struct class *cla,
			    struct class_attribute *attr,
			    char *buf)
{
	return snprintf(buf, 80, "print_flag is %d\n", print_flag);
}

static ssize_t print_flag_store(struct class *cla,
			     struct class_attribute *attr,
			     const char *buf, size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0)
		return ret;

	print_flag = tmp;
	return count;
}

static ssize_t cts_video_flag_show(struct class *class,
				   struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "cts_video_flag: %d\n", cts_video_flag);
}

static ssize_t cts_video_flag_store(struct class *class,
				    struct class_attribute *attr,
				    const char *buf, size_t count)
{
	ssize_t r;
	int val;

	r = kstrtoint(buf, 0, &val);
	if (r < 0)
		return -EINVAL;

	if (val > 0)
		cts_video_flag = val;
	else
		cts_video_flag = 0;
	return count;
}

static ssize_t inactive_check_disp_show(struct class *cla,
			    struct class_attribute *attr,
			    char *buf)
{
	return snprintf(buf, 80,
			"current inactive_check_disp is %d\n",
			inactive_check_disp);
}

static ssize_t inactive_check_disp_store(struct class *cla,
			     struct class_attribute *attr,
			     const char *buf, size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0)
		return ret;

	inactive_check_disp = tmp;
	return count;
}

static CLASS_ATTR_RW(sei_cnt);
static CLASS_ATTR_RW(alloc_sei_debug);
static CLASS_ATTR_RW(video_nr_base);
static CLASS_ATTR_RW(n_devs);
static CLASS_ATTR_RW(debug);
static CLASS_ATTR_RW(get_count);
static CLASS_ATTR_RW(put_count);
static CLASS_ATTR_RW(q_count);
static CLASS_ATTR_RW(dq_count);
static CLASS_ATTR_RW(cts_use_di);
static CLASS_ATTR_RW(render_use_dec);
static CLASS_ATTR_RW(dec_count);
static CLASS_ATTR_RW(vf_dump);
static CLASS_ATTR_RO(open_fd_count);
static CLASS_ATTR_RO(release_fd_count);
static CLASS_ATTR_RO(link_fd_count);
static CLASS_ATTR_RO(link_put_fd_count);
static CLASS_ATTR_RO(v4lvideo_version);
static CLASS_ATTR_RO(total_get_count);
static CLASS_ATTR_RO(total_put_count);
static CLASS_ATTR_RO(total_release_count);
static CLASS_ATTR_RW(print_flag);
static CLASS_ATTR_RW(cts_video_flag);
static CLASS_ATTR_RW(inactive_check_disp);

static struct attribute *v4lvideo_class_attrs[] = {
	&class_attr_sei_cnt.attr,
	&class_attr_alloc_sei_debug.attr,
	&class_attr_video_nr_base.attr,
	&class_attr_n_devs.attr,
	&class_attr_debug.attr,
	&class_attr_get_count.attr,
	&class_attr_put_count.attr,
	&class_attr_q_count.attr,
	&class_attr_dq_count.attr,
	&class_attr_cts_use_di.attr,
	&class_attr_render_use_dec.attr,
	&class_attr_dec_count.attr,
	&class_attr_vf_dump.attr,
	&class_attr_open_fd_count.attr,
	&class_attr_release_fd_count.attr,
	&class_attr_link_fd_count.attr,
	&class_attr_link_put_fd_count.attr,
	&class_attr_v4lvideo_version.attr,
	&class_attr_total_get_count.attr,
	&class_attr_total_put_count.attr,
	&class_attr_total_release_count.attr,
	&class_attr_print_flag.attr,
	&class_attr_inactive_check_disp.attr,
	&class_attr_cts_video_flag.attr,
	NULL
};

ATTRIBUTE_GROUPS(v4lvideo_class);

static struct class v4lvideo_class = {
	.name = "v4lvideo",
	.class_groups = v4lvideo_class_groups,
};

static int v4lvideo_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int v4lvideo_release(struct inode *inode, struct file *file)
{
	return 0;
}

static long v4lvideo_ioctl(struct file *file,
			   unsigned int cmd,
			   ulong arg)
{
	long ret = 0;
	void __user *argp = (void __user *)arg;

	switch (cmd) {
	case V4LVIDEO_IOCTL_ALLOC_ID:{
			u32 v4lvideo_id = 0;

			ret = v4lvideo_alloc_map(&v4lvideo_id);
			if (ret != 0)
				break;
			put_user(v4lvideo_id, (u32 __user *)argp);
		}
		break;
	case V4LVIDEO_IOCTL_FREE_ID:{
			u32 v4lvideo_id;

			get_user(v4lvideo_id, (u32 __user *)argp);
			v4lvideo_release_map(v4lvideo_id);
		}
		break;
	case V4LVIDEO_IOCTL_ALLOC_FD:{
			u32 v4lvideo_fd = 0;

			ret = v4lvideo_alloc_fd(&v4lvideo_fd);
			if (ret != 0)
				break;
			put_user(v4lvideo_fd, (u32 __user *)argp);
		}
		break;
	case V4LVIDEO_IOCTL_LINK_FD: {
			int data[2];
			int src_fd, dst_fd;

			if (copy_from_user(data, argp, sizeof(data)) == 0) {
				src_fd = data[0];
				dst_fd = data[1];
				ret = v4lvideo_fd_link(src_fd, dst_fd);
			} else {
				ret = -EFAULT;
			}
		}
		break;
	default:
		return -EINVAL;
	}
	return ret;
}

#ifdef CONFIG_COMPAT
static long v4lvideo_compat_ioctl(struct file *file,
				  unsigned int cmd,
				  ulong arg)
{
	long ret = 0;

	ret = v4lvideo_ioctl(file, cmd, (ulong)compat_ptr(arg));
	return ret;
}
#endif
static const struct file_operations v4lvideo_fops = {
	.owner = THIS_MODULE,
	.open = v4lvideo_open,
	.release = v4lvideo_release,
	.unlocked_ioctl = v4lvideo_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = v4lvideo_compat_ioctl,
#endif
	.poll = NULL,
};

int __init v4lvideo_init(void)
{
	int ret = -1, i;
	struct device *devp;
	mutex_init(&mutex_dec_count);

	keeper_mgr_init();

	ret = class_register(&v4lvideo_class);
	if (ret < 0)
		return ret;

	ret = register_chrdev(V4LVIDEO_MAJOR, "v4lvideo", &v4lvideo_fops);
	if (ret < 0) {
		pr_err("Can't allocate major for v4lvideo device\n");
		goto error1;
	}

	devp = device_create(&v4lvideo_class,
			     NULL,
			     MKDEV(V4LVIDEO_MAJOR, 0),
			     NULL,
			     V4LVIDEO_DEVICE_NAME);
	if (IS_ERR(devp)) {
		pr_err("failed to create v4lvideo device node\n");
		ret = PTR_ERR(devp);
		return ret;
	}

	if (n_devs <= 0)
		n_devs = 1;
	for (i = 0; i < n_devs; i++) {
		ret = v4lvideo_create_instance(i);
		if (ret) {
			/* If some instantiations succeeded, keep driver */
			if (i)
				ret = 0;
			break;
		}
	}

	if (ret < 0) {
		V4LVID_ERR("v4lvideo: error %d while loading driver\n", ret);
		goto error1;
	}

	return 0;

error1:
	unregister_chrdev(V4LVIDEO_MAJOR, "v4lvideo");
	class_unregister(&v4lvideo_class);
	return ret;
}

void __exit v4lvideo_exit(void)
{
	v4lvideo_v4l2_release();
	device_destroy(&v4lvideo_class, MKDEV(V4LVIDEO_MAJOR, 0));
	unregister_chrdev(V4LVIDEO_MAJOR, V4LVIDEO_DEVICE_NAME);
	class_unregister(&v4lvideo_class);
}

//MODULE_DESCRIPTION("Video Technology Magazine V4l Video Capture Board");
//MODULE_AUTHOR("Amlogic, Jintao Xu<jintao.xu@amlogic.com>");
//MODULE_LICENSE("Dual BSD/GPL");
//MODULE_VERSION(V4LVIDEO_VERSION);

