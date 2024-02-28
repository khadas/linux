// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * drivers/amlogic/media/video_processor/video_composer/video_composer.c
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

#include <linux/amlogic/major.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/sysfs.h>
#include <linux/time.h>
#include <linux/uaccess.h>
#include <linux/file.h>
#include <uapi/linux/sched/types.h>
#include <linux/amlogic/meson_uvm_core.h>
#include <linux/sched/clock.h>
#include <linux/sync_file.h>
#include <linux/delay.h>
#include <linux/amlogic/aml_sync_api.h>
#include <linux/amlogic/media/video_processor/di_proc_buf_mgr.h>
#include <linux/compat.h>

#include "di_process.h"
#include "di_proc_file.h"

#define DI_PROCESS_DEVICE_NAME   "di_process-dev"
#define WAIT_THREAD_STOPPED_TIMEOUT 20
#define WAIT_READY_Q_TIMEOUT 100
#define MAX_RECEIVE_WAIT_TIME 15  /*15ms*/
#define DI_INSTANCE_COUNT 2

static u32 di_process_instance_num = DI_INSTANCE_COUNT;
static u32 print_flag;
static u32 total_get_count;
static u32 total_put_count;
static u32 total_src_get_count;
static u32 total_src_put_count;
u32 total_fill_count;
static u32 total_fill_done_count;
static u32 total_empty_count;
static u32 total_empty_done_count;
static void *timeline[DI_INSTANCE_COUNT];
static u32 cur_streamline_val[DI_INSTANCE_COUNT];
static u32 q_dropped = 1;

static DEFINE_MUTEX(di_process_mutex);

int dp_print(int index, int debug_flag, const char *fmt, ...)
{
	if ((print_flag & debug_flag) ||
	    debug_flag == PRINT_ERROR) {
		unsigned char buf[256];
		int len = 0;
		va_list args;

		va_start(args, fmt);
		len = sprintf(buf, "dp:[%d]", index);
		vsnprintf(buf + len, 256 - len, fmt, args);
		if (debug_flag == PRINT_ERROR)
			pr_err("%s", buf);
		else
			pr_info("%s", buf);
		va_end(args);
	}
	return 0;
}

static struct di_process_port_s ports[] = {
	{
		.name = "di_process.0",
		.index = 0,
		.open_count = 0,
	},
	{
		.name = "di_process.1",
		.index = 1,
		.open_count = 0,
	},
	{
		.name = "di_process.2",
		.index = 2,
		.open_count = 0,
	},
};

static void dp_timeline_create(int index)
{
	const char *tl_name = "dp_timeline_0";

	if (index == 0)
		tl_name = "dp_timeline_0";
	else if (index == 1)
		tl_name = "dp_timeline_1";

	cur_streamline_val[index] = 0;
	timeline[index] = aml_sync_create_timeline(tl_name);
	dp_print(index, PRINT_FENCE,
		 "timeline create tlName =%s, timeline=%p\n",
		 tl_name, timeline[index]);
}

static int dp_timeline_create_fence(struct di_process_dev *dev)
{
	int out_fence_fd = -1;
	u32 pt_val = 0;

	pt_val = cur_streamline_val[dev->index] + 1;
	dp_print(dev->index, PRINT_FENCE, "creat fence: pt_val %d", pt_val);

	out_fence_fd = aml_sync_create_fence(timeline[dev->index], pt_val);
	if (out_fence_fd >= 0) {
		cur_streamline_val[dev->index]++;
		dev->fence_creat_count++;
	} else {
		dp_print(dev->index, PRINT_ERROR,
			 "create fence returned %d", out_fence_fd);
	}
	return out_fence_fd;
}

static void dp_timeline_increase(struct di_process_dev *dev,
				    unsigned int value)
{
	aml_sync_inc_timeline(timeline[dev->index], value);
	dev->fence_release_count += value;
	dp_print(dev->index, PRINT_FENCE,
		"release fence: fen_creat_cnt=%lld,fen_release_cnt=%lld\n",
		dev->fence_creat_count,
		dev->fence_release_count);
}

static struct file_private_data *dp_get_file_private(struct di_process_dev *dev,
						      struct file *file_vf)
{
	struct file_private_data *file_private_data;
	struct uvm_hook_mod *uhmod;

	if (!file_vf) {
		dp_print(dev->index, PRINT_ERROR, "%s: NULL param.\n", __func__);
		return NULL;
	}

	if (is_v4lvideo_buf_file(file_vf)) {
		file_private_data = (struct file_private_data *)(file_vf->private_data);
	} else {
		uhmod = uvm_get_hook_mod((struct dma_buf *)(file_vf->private_data),
			VF_PROCESS_V4LVIDEO);
		if (!uhmod) {
			dp_print(dev->index, PRINT_ERROR, "hook mod is NULL\n");
			file_private_data =  NULL;
		} else {
			file_private_data = uhmod->arg;
			uvm_put_hook_mod((struct dma_buf *)(file_vf->private_data),
				VF_PROCESS_V4LVIDEO);
		}
	}

	if (IS_ERR_OR_NULL(file_private_data))
		dp_print(dev->index, PRINT_ERROR, "%s: file_private_data is NULL\n", __func__);
	else
		dp_print(dev->index, PRINT_OTHER, "%s: file_private_data is %px.\n",
			__func__, file_private_data);

	return file_private_data;
}

static struct vframe_s *get_vf_from_file(struct di_process_dev *dev,
					 struct file *file_vf)
{
	struct vframe_s *vf = NULL;
	bool is_dec_vf = false;
	bool is_v4l_vf = false;
	struct file_private_data *file_private_data = NULL;

	if (IS_ERR_OR_NULL(dev) || IS_ERR_OR_NULL(file_vf)) {
		dp_print(dev->index, PRINT_ERROR,
			"%s: invalid param.\n",
			__func__);
		return vf;
	}

	is_dec_vf = is_valid_mod_type(file_vf->private_data, VF_SRC_DECODER);
	is_v4l_vf = is_valid_mod_type(file_vf->private_data, VF_PROCESS_V4LVIDEO);

	if (is_dec_vf) {
		dp_print(dev->index, PRINT_OTHER, "vf is from decoder\n");
		vf =
		dmabuf_get_vframe((struct dma_buf *)(file_vf->private_data));
		if (!vf) {
			dp_print(dev->index, PRINT_ERROR, "vf is NULL.\n");
			return vf;
		}

		dp_print(dev->index, PRINT_OTHER,
			"vframe_type = 0x%x, vframe_flag = 0x%x.\n",
			vf->type,
			vf->flag);
		dmabuf_put_vframe((struct dma_buf *)(file_vf->private_data));
		if (vf->omx_index == 0 && vf->index_disp != 0)
			vf->omx_index = vf->index_disp;
	} else if (is_v4l_vf) {
		dp_print(dev->index, PRINT_MORE, "vf is from v4lvideo\n");
		file_private_data = dp_get_file_private(dev, file_vf);
		if (!file_private_data)
			dp_print(dev->index, PRINT_ERROR,
				 "invalid fd: no uvm, no v4lvideo!!\n");
		else
			vf = &file_private_data->vf;
	}
	return vf;
}

static void video_wait_decode_fence(struct di_process_dev *dev,
				    struct vframe_s *vf)
{
	if (vf && vf->fence) {
		u64 timestamp = local_clock();
		s32 ret = dma_fence_wait_timeout(vf->fence, false, 2000);

		dp_print(dev->index, PRINT_FENCE,
			 "%s, fence %lx, state: %d, wait cost time: %lld ns\n",
			 __func__, (ulong)vf->fence, ret,
			 local_clock() - timestamp);
		vf->fence = NULL;
	} else {
		dp_print(dev->index, PRINT_MORE,
			 "decoder fence is NULL\n");
	}
}

static struct file *dp_get_file_ext(struct di_process_dev *dev, struct file *file_vf)
{
	if (!file_vf) {
		dp_print(dev->index, PRINT_ERROR, "%s: NULL param.\n", __func__);
		return NULL;
	}

	get_file(file_vf);
	dp_print(dev->index, PRINT_OTHER, "%s:get_file=%px, file_count=%ld\n",
		__func__, file_vf, file_count(file_vf));
	total_get_count++;
	total_src_get_count++;
	dev->fget_count++;

	dp_print(dev->index, PRINT_OTHER, "%s:fget_count=%lld.\n",
		__func__, dev->fget_count);
	return file_vf;
}

static struct file *dp_get_file(struct di_process_dev *dev, int fd)
{
	struct file *file_vf = NULL;

	file_vf = fget(fd);
	if (!file_vf) {
		dp_print(dev->index, PRINT_ERROR, "fget fd fail\n");
		return NULL;
	}

	dp_print(dev->index, PRINT_OTHER, "%s:get_file=%px, file_count=%ld\n",
		__func__, file_vf, file_count(file_vf));
	total_get_count++;
	total_src_get_count++;
	dev->fget_count++;

	dp_print(dev->index, PRINT_OTHER, "%s:fget_count=%lld.\n",
		__func__, dev->fget_count);
	return file_vf;
}

void dp_put_file_ext(int dev_index, struct file *file_vf)
{
	if (!file_vf) {
		pr_err("file is NULL!!!\n");
		return;
	}

	dp_print(dev_index, PRINT_OTHER, "%s:put_file=%px, file_count=%ld\n",
		__func__, file_vf, file_count(file_vf));

	fput(file_vf);
	total_put_count++;
	total_src_put_count++;
}
EXPORT_SYMBOL(dp_put_file_ext);

static void dp_put_file(struct di_process_dev *dev, struct file *file_vf)
{
	if (!file_vf) {
		pr_err("file is NULL!!!\n");
		return;
	}

	dp_put_file_ext(dev->index, file_vf);
	dev->fput_count++;
}

int get_received_frame_free_index(struct di_process_dev *dev)
{
	int i = 0;
	int j = 0;

	while (1) {
		for (i = 0; i < DIPR_POOL_SIZE; i++) {
			if (!atomic_read(&dev->received_frame[i].on_use))
				break;
		}
		if (i == DIPR_POOL_SIZE) {
			j++;
			if (j > WAIT_READY_Q_TIMEOUT) {
				dp_print(dev->index, PRINT_ERROR,
					"receive_q is full, wait timeout!\n");
				return -1;
			}
			usleep_range(1000 * MAX_RECEIVE_WAIT_TIME,
				     1000 * (MAX_RECEIVE_WAIT_TIME + 1));
			dp_print(dev->index, PRINT_ERROR,
				"receive_q is full!!! need wait =%d\n", j);
			continue;
		} else {
			break;
		}
	}
	return i;
}

static int check_dropped(struct di_process_dev *dev, struct uvm_di_mgr_t *uvm_di_mgr,
	struct vframe_s *vf)
{
	int index_diff = 0;
	struct dp_buf_mgr_t *buf_mgr;
	int ret = 0;

	if (uvm_di_mgr) {
		buf_mgr = uvm_di_mgr->buf_mgr;
	} else {
		dp_print(dev->index, PRINT_ERROR, "uvm_di_mgr is NULL.\n");
		return 0;
	}

	if (dev->last_dec_type != buf_mgr->dec_type ||
		dev->last_instance_id != buf_mgr->instance_id ||
		dev->last_buf_mgr_reset_id != buf_mgr->reset_id) {
		dp_print(dev->index, PRINT_OTHER, "dec instance changed\n");

		dev->last_dec_type = buf_mgr->dec_type;
		dev->last_instance_id = buf_mgr->instance_id;
		dev->last_buf_mgr_reset_id = buf_mgr->reset_id;
		dev->last_frame_index = vf->omx_index;

		ret = 0;
	} else {
		index_diff = vf->omx_index - dev->last_frame_index;

		dev->last_dec_type = buf_mgr->dec_type;
		dev->last_instance_id = buf_mgr->instance_id;
		dev->last_buf_mgr_reset_id = buf_mgr->reset_id;
		dev->last_frame_index = vf->omx_index;

		ret = index_diff - 1;
	}

	return ret;
}

static int queue_input_to_di(struct di_process_dev *dev, struct vframe_s *vf,
	bool dropped, struct file *src_file, bool dummy)
{
	struct di_buffer *di_buf = NULL;
	int ret;

	if (!kfifo_get(&dev->di_input_free_q, &di_buf)) {
		dp_print(dev->index, PRINT_ERROR, "get di_input_free_q err !!!\n");
		return -1;
	}

	di_buf->vf = vf;
	di_buf->flag = 0;
	di_buf->caller_mng.queued = true;
	di_buf->caller_mng.dropped = dropped;
	di_buf->caller_mng.dummy = dummy;
	di_buf->caller_mng.src_file = src_file;

	/*tmp code, if di can copy caller_mng to dst di_buf, this code shold remove*/
	vf->crop[0] = di_buf->caller_mng.queued;
	vf->crop[1] = di_buf->caller_mng.dropped;
	vf->crop[2] = di_buf->caller_mng.dummy;
	vf->vc_private = (struct video_composer_private *)di_buf->caller_mng.src_file;

	ret = di_empty_input_buffer(dev->di_index, di_buf);
	if (ret != 0) {
		di_buf->caller_mng.queued = false;
		dp_print(dev->index, PRINT_ERROR,
		"empty_input err: ret=%d, di_index=%d\n", ret, dev->di_index);
		return ret;
	}
	dev->empty_count++;
	total_empty_count++;

	dp_print(dev->index, PRINT_OTHER,
		"di_empty_input_buffer omx_index=%d, empty_count = %lld, %d\n",
		vf->omx_index, dev->empty_count, total_empty_count);

	return 0;
}

static void queue_outbuf_to_di(struct di_process_dev *dev, struct di_buffer *di_buf)
{
	di_fill_output_buffer(dev->di_index, di_buf);

	dev->fill_count++;
	total_fill_count++;
	dp_print(dev->index, PRINT_OTHER, "qbuf done: fill_count=%lld, total_fill_count=%d\n",
		dev->fill_count, total_fill_count);
}

static void di_process_task(struct di_process_dev *dev)
{
	struct vframe_s *vf = NULL;
	struct file *file_vf = NULL;
	struct received_frame_t *received_frame = NULL;
	struct uvm_di_mgr_t *uvm_di_mgr = NULL;
	int drop_count;
	bool is_i_frame;
	struct vframe_s *vf_1;
	struct vframe_s *vf_2;
	struct file *file_1 = NULL;
	struct file *file_2 = NULL;
	int ret;
	struct di_buffer *di_buf;
	bool need_q_drop = true;
	int need_process_count;
	bool dummy;

	if (!kfifo_peek(&dev->di_input_free_q, &di_buf)) {
		dp_print(dev->index, PRINT_ERROR, "di_input_free_q empty\n");
		usleep_range(2000, 3000);
		return;
	}

	if (!kfifo_get(&dev->receive_q, &received_frame)) {
		dp_print(dev->index, PRINT_ERROR, "task: get frame failed\n");
		usleep_range(2000, 3000);
		return;
	}

	if (IS_ERR_OR_NULL(received_frame)) {
		dp_print(dev->index, PRINT_ERROR,
			 "task: get received_frames is NULL\n");
		return;
	}

	file_vf = received_frame->file_vf;
	vf = received_frame->vf;
	dummy = received_frame->dummy;
	if (!vf) {
		dp_print(dev->index, PRINT_ERROR, "%s:vf is NULL\n", __func__);
		return;
	}

	if (!file_vf && !dummy)
		dp_print(dev->index, PRINT_ERROR, "file_vf is NULL\n");

	is_i_frame = vf->type & VIDTYPE_INTERLACE_TOP;

	if (dummy) {
		drop_count = 0;
	} else {
		uvm_di_mgr = get_uvm_di_mgr(file_vf);
		drop_count = check_dropped(dev, uvm_di_mgr, vf);
	}

	need_process_count = DIPR_POOL_SIZE - kfifo_len(&dev->di_input_free_q);
	dp_print(dev->index, PRINT_OTHER, "need_process_count %d\n", need_process_count);
	if (need_process_count > 0) {
		need_q_drop = false;
		dp_print(dev->index, PRINT_OTHER, "too many buf need process %d, so not q_drop\n",
			need_process_count);
	}

	if (is_i_frame && drop_count && q_dropped && need_q_drop) {
		dp_print(dev->index, PRINT_OTHER, "drop count %d\n", drop_count);
		buf_mgr_file_lock(uvm_di_mgr);
		ret = di_get_ref_vf(file_vf, &vf_1, &vf_2, &file_1, &file_2);
		/*this time file_1 or file_2 has been free, it will panic when get file*/
		if (drop_count == 1) {
			if (vf_1 && file_1) {
				get_file(file_1);
				buf_mgr_file_unlock(uvm_di_mgr);
				video_wait_decode_fence(dev, vf_1);
				ret = queue_input_to_di(dev, vf_1, true, file_1, false);
				if (ret != 0)
					fput(file_1);
			} else {
				buf_mgr_file_unlock(uvm_di_mgr);
				dp_print(dev->index, PRINT_ERROR, "drop 1 but not find ref\n");
			}
		} else if (drop_count == 2) {
			if (vf_1 && vf_2 && file_1 && file_2) {
				get_file(file_1);
				get_file(file_2);
				buf_mgr_file_unlock(uvm_di_mgr);
				video_wait_decode_fence(dev, vf_2);
				ret = queue_input_to_di(dev, vf_2, true, file_2, false);
				if (ret != 0)
					fput(file_1);
				video_wait_decode_fence(dev, vf_1);
				ret = queue_input_to_di(dev, vf_1, true, file_1, false);
				if (ret != 0)
					fput(file_2);
			} else {
				buf_mgr_file_unlock(uvm_di_mgr);
				dp_print(dev->index, PRINT_ERROR, "drop 2 but not find ref\n");
			}
		} else {
			buf_mgr_file_unlock(uvm_di_mgr);
			dp_print(dev->index, PRINT_OTHER, "drop too many frame %d\n", drop_count);
		}
	}

	received_frame->file_vf = NULL;
	received_frame->vf = NULL;
	received_frame->dummy = false;
	atomic_set(&received_frame->on_use, false);

	if (!dummy) {
		video_wait_decode_fence(dev, vf);
		queue_input_to_di(dev, vf, false, file_vf, false);
	} else {
		queue_input_to_di(dev, vf, true, file_vf, true);
	}
}

static void di_process_wait_event(struct di_process_dev *dev)
{
	wait_event_interruptible_timeout(dev->wq,
					 kfifo_len(&dev->receive_q) > 0 ||
					 dev->thread_need_stop,
					 msecs_to_jiffies(5000));
}

static int di_process_thread(void *data)
{
	struct di_process_dev *dev = data;

	dp_print(dev->index, PRINT_OTHER, "thread: started\n");

	dev->thread_stopped = 0;
	while (1) {
		if (kthread_should_stop())
			break;

		if (kfifo_len(&dev->receive_q) == 0)
			di_process_wait_event(dev);

		if (kthread_should_stop())
			break;

		if (kfifo_len(&dev->receive_q) > 0)
			di_process_task(dev);
	}
	dev->thread_stopped = 1;
	dp_print(dev->index, PRINT_OTHER, "thread: exit\n");
	return 0;
}

/*empty done and fill done when di internal bypass*/
int process_empty_done_buf(struct di_process_dev *dev, struct di_buffer *buf)
{
	int ret = 0;
	struct file *file_vf = NULL;
	bool dropped;

	dropped = buf->caller_mng.dropped;
	file_vf = buf->caller_mng.src_file;

	buf->caller_mng.queued = false;

	if (!kfifo_put(&dev->di_input_free_q, buf))
		dp_print(dev->index, PRINT_ERROR,
			"%s: put di_input_free_q fail\n", __func__);

	if (!dropped) {
		ret = di_processed_checkin(file_vf);
		if (ret)
			dp_print(dev->index, PRINT_ERROR,
				"%s: processed_checkin fail\n", __func__);

		/*bypass and di vf include dec vf, need put dec file after displayed*/
		if (!(buf->flag & DI_FLAG_BUF_BY_PASS) &&
			!(buf->vf->flag & VFRAME_FLAG_DOUBLE_FRAM)) {
			dp_put_file(dev, file_vf);
		} else {
			dp_print(dev->index, PRINT_OTHER, "bypass or double vf, not put %px\n",
				file_vf);
		}
	}
	return ret;
}

enum DI_ERRORTYPE dp_empty_input_done(struct di_buffer *buf)
{
	struct di_process_dev *dev = NULL;
	int ret;

	if (!buf) {
		pr_err("%s: di_buffer is NULL\n", __func__);
		return 0;
	}

	dev = (struct di_process_dev *)(buf->caller_data);
	if (!dev) {
		pr_err("%s: dev is NULL\n", __func__);
		return 0;
	}

	dp_print(dev->index, PRINT_OTHER, "%s: buf->flag: 0x%x.\n", __func__, buf->flag);
	if (buf->flag & DI_FLAG_EOS)
		dp_print(dev->index, PRINT_ERROR,
			"%s: eos\n", __func__);

	if (buf->flag & DI_FLAG_BUF_BY_PASS) {
		dp_print(dev->index, PRINT_OTHER,
			"%s: di driver bypass\n", __func__);
		return 0;
	}

	if (!buf->vf) {
		dp_print(dev->index, PRINT_ERROR,
			"%s: vf is NULL\n", __func__);
		return 0;
	}

	/*tmp code, if di can copy caller_mng to dst di_buf, this code shold remove*/
	buf->caller_mng.queued = buf->vf->crop[0];
	buf->caller_mng.dropped = buf->vf->crop[1];
	buf->caller_mng.dummy = buf->vf->crop[2];
	buf->caller_mng.src_file = (struct file *)buf->vf->vc_private;

	if (buf->caller_mng.dropped && !buf->caller_mng.dummy) {
		dp_print(dev->index, PRINT_OTHER, "%s: fput drop file %px\n",
			__func__, buf->caller_mng.src_file);
		fput(buf->caller_mng.src_file);
	}

	dev->empty_done_count++;
	total_empty_done_count++;

	dp_print(dev->index, PRINT_OTHER,
		  "%s: omx_index=%d, empty_done_count=%lld, total_empty_done_count=%d\n",
		  __func__, buf->vf->omx_index,
		  dev->empty_done_count,
		  total_empty_done_count);

	ret = process_empty_done_buf(dev, buf);

	/*here need release fence*/

	return ret;
}

enum DI_ERRORTYPE dp_fill_output_done(struct di_buffer *buf)
{
	struct di_process_dev *dev = NULL;
	struct dma_buf *dmabuf;
	struct file_private_data *private_data = NULL;
	bool dropped = false;
	bool di_bypass = false;

	if (!buf) {
		pr_err("%s: di_buffer is NULL\n", __func__);
		return 0;
	}

	dev = (struct di_process_dev *)(buf->caller_data);
	if (!dev) {
		pr_err("%s: dev is NULL\n", __func__);
		return 0;
	}

	if (buf->flag & DI_FLAG_EOS)
		dp_print(dev->index, PRINT_ERROR, "%s: eos\n", __func__);

	if (buf->flag & DI_FLAG_BUF_BY_PASS) {
		di_bypass = true;
		dp_print(dev->index, PRINT_OTHER, "%s: di bypass\n", __func__);
	}

	if (!buf->vf) {
		dp_print(dev->index, PRINT_ERROR, "%s: vf is NULL\n", __func__);
		return 0;
	}
	if (!dev->first_out)
		dp_print(dev->index, PRINT_OTHER, "%s: DI output first frame\n", __func__);

	/*tmp code, if di can copy caller_mng to dst di_buf, this code shold remove*/
	buf->caller_mng.queued = buf->vf->crop[0];
	buf->caller_mng.dropped = buf->vf->crop[1];
	buf->caller_mng.dummy = buf->vf->crop[2];
	buf->caller_mng.src_file = (struct file *)buf->vf->vc_private;

	if (buf->caller_mng.dummy) {
		dev->first_out = true;
		dp_print(dev->index, PRINT_OTHER, "dummy frame out\n");
	}

	dropped = buf->caller_mng.dropped;

	if (!di_bypass) {
		dev->fill_done_count++;
		total_fill_done_count++;
	}

	if (dropped && !buf->caller_mng.dummy && di_bypass) {
		dp_print(dev->index, PRINT_OTHER, "%s: fput drop file %px\n",
			__func__, buf->caller_mng.src_file);
		fput(buf->caller_mng.src_file);
	}

	dp_print(dev->index, PRINT_OTHER,
		  "%s: omx_index=%d, flag=%x, fill_done_count =%lld, %d\n",
		  __func__, buf->vf->omx_index, buf->flag,
		  dev->fill_done_count,
		  total_fill_done_count);

	if (dropped) {
		dp_print(dev->index, PRINT_OTHER, "%s:dropped\n", __func__);
		if (di_bypass)
			process_empty_done_buf(dev, buf);
		else
			queue_outbuf_to_di(dev, buf);
		return 0;
	}
	if (!kfifo_get(&dev->file_wait_q, &dmabuf)) {
		dp_print(dev->index, PRINT_ERROR, "get file wait fail!!!\n");
		return -EINVAL;
	}
	private_data = v4lvideo_get_file_private_data(dmabuf->file, false);
	if (!private_data) {
		dp_print(dev->index, PRINT_ERROR, "%s:private_data is null\n", __func__);
		return 0;
	}
	dp_print(dev->index, PRINT_OTHER,
		"%s: dmabuf =%px, dmabuf->file =%px, private_data =%px, di_buffer=%px\n",
		__func__, dmabuf, dmabuf->file, private_data, buf);

	private_data->vf = *buf->vf;
	private_data->vf_p = (struct vframe_s *)buf;
	if (di_bypass)
		private_data->flag = V4LVIDEO_FLAG_DI_BYPASS;
	else
		private_data->flag = V4LVIDEO_FLAG_DI_V3;
	private_data->private2 = (void *)buf;
	private_data->is_keep = true;

	if (di_bypass) {
		process_empty_done_buf(dev, buf);
	} else {
		/*not bypass and di vf include dec vf, need get dec file*/
		dp_print(dev->index, PRINT_OTHER,
			"%s: has dec vf, vf=%px, file=%px\n",
			__func__, buf->vf, buf->caller_mng.src_file);
		if (buf->vf->flag & VFRAME_FLAG_DOUBLE_FRAM) {
			dp_get_file_ext(dev, buf->caller_mng.src_file);
			update_di_process_state(buf->caller_mng.src_file);
		} else {
			dp_print(dev->index, PRINT_OTHER, "no dw vf.\n");
		}
	}

	dp_timeline_increase(dev, 1);
	return 0;
}

static void file_q_init(struct di_process_dev *dev)
{
	int i;
	struct dma_buf *dmabuf;

	INIT_KFIFO(dev->file_wait_q);
	kfifo_reset(&dev->file_wait_q);

	INIT_KFIFO(dev->file_free_q);
	kfifo_reset(&dev->file_free_q);

	for (i = 0; i < DIPR_POOL_SIZE; i++) {
		dmabuf = uvm_alloc_dmabuf(SZ_4K, 0, 0);
		if (!dmabuf_is_uvm(dmabuf))
			dp_print(dev->index, PRINT_ERROR, "dmabuf is not uvm\n");
		dp_print(dev->index, PRINT_OTHER, "dmabuf is uvm:dmabuf=%px\n", dmabuf);

		dev->out_dmabuf[i] = dmabuf;
		if (!kfifo_put(&dev->file_free_q, dev->out_dmabuf[i]))
			dp_print(dev->index, PRINT_ERROR,
				"file_free_q put fail\n");
	}
}

static void file_q_uninit(struct di_process_dev *dev)
{
	int i;

	for (i = 0; i < DIPR_POOL_SIZE; i++) {
		if (dev->out_dmabuf[i])
			dma_buf_put(dev->out_dmabuf[i]);
		else
			dp_print(dev->index, PRINT_ERROR, "unreg: dmabuf is NULL\n");
		dev->out_dmabuf[i] = NULL;
	}
}

static void di_input_free_q_init(struct di_process_dev *dev)
{
	int i = 0;

	INIT_KFIFO(dev->di_input_free_q);
	kfifo_reset(&dev->di_input_free_q);

	for (i = 0; i < DIPR_POOL_SIZE; i++) {
		dev->di_input[i].caller_data = (void *)dev;
		//dev->di_input[i].index = i;
		kfifo_put(&dev->di_input_free_q, &dev->di_input[i]);
	}
}

static void receive_q_init(struct di_process_dev *dev)
{
	int i = 0;

	INIT_KFIFO(dev->receive_q);
	kfifo_reset(&dev->receive_q);

	for (i = 0; i < DIPR_POOL_SIZE; i++) {
		dev->received_frame[i].index = i;
		dev->received_frame[i].file_vf = NULL;
		atomic_set(&dev->received_frame[i].on_use, false);
	}
}

static void receive_q_uninit(struct di_process_dev *dev)
{
	int i = 0;
	struct received_frame_t *received_frame = NULL;

	dp_print(dev->index, PRINT_OTHER, "unit receive_q len=%d\n",
		 kfifo_len(&dev->receive_q));
	while (kfifo_len(&dev->receive_q) > 0) {
		if (kfifo_get(&dev->receive_q, &received_frame))
			dp_put_file(dev, received_frame->file_vf);
	}

	for (i = 0; i < DIPR_POOL_SIZE; i++) {
		atomic_set(&dev->received_frame[i].on_use, false);
		dev->received_frame[i].file_vf = NULL;
	}
}

static int di_process_init(struct di_process_dev *dev)
{
	struct sched_param param = {.sched_priority = 2};

	dev->di_index = -1;

	dev->di_parm.work_mode = WORK_MODE_PRE_POST;
	dev->di_parm.buffer_mode = BUFFER_MODE_ALLOC_BUF;
	dev->di_parm.output_format = DI_OUTPUT_BY_DI_DEFINE;
	dev->di_parm.ops.empty_input_done = dp_empty_input_done;
	dev->di_parm.ops.fill_output_done = dp_fill_output_done;
	dev->di_parm.caller_data = (void *)dev;
	dev->di_parm.buffer_keep = 0;
	dev->di_index = di_create_instance(dev->di_parm);
	if (dev->di_index < 0) {
		dp_print(dev->index, PRINT_ERROR,
			  "creat di fail, di_index=%d\n",
			  dev->di_index);
		return dev->di_index;
	}
	dev->di_is_tvp = false;
	dp_print(dev->index, PRINT_OTHER,
		  "%s: di_index = %d\n", __func__, dev->di_index);

	if (di_set_buffer_num(4, 4) < 0)
		dp_print(dev->index, PRINT_ERROR, "%s: set DI buf num failed.\n", __func__);

	dev->inited = true;
	dev->fput_count = 0;
	dev->fget_count = 0;
	dev->empty_count = 0;
	dev->fill_count = 0;
	dev->empty_done_count = 0;
	dev->fill_done_count = 0;
	dev->fence_creat_count = 0;
	dev->fence_release_count = 0;

	dev->kthread = NULL;
	dev->thread_need_stop = false;
	dev->last_dec_type = DEC_TYPE_MAX;
	dev->last_instance_id = 0xFFFFFFFF;
	dev->last_buf_mgr_reset_id = 0xFFFFFFFF;
	dev->last_frame_index = 0xFFFFFFFF;
	dev->first_out = false;
	dev->q_dummy_frame_done = false;
	dev->last_frame_bypass = false;
	dev->cur_is_i = false;

	receive_q_init(dev);
	di_input_free_q_init(dev);

	file_q_init(dev);

	init_waitqueue_head(&dev->wq);

	dev->kthread = kthread_create(di_process_thread,
				      dev, dev->port->name);
	if (IS_ERR(dev->kthread)) {
		pr_err("di_process_thread creat failed\n");
		return -ENOMEM;
	}

	if (sched_setscheduler(dev->kthread, SCHED_FIFO, &param))
		dp_print(dev->index, PRINT_ERROR, "Could not set realtime priority.\n");

	wake_up_process(dev->kthread);

	return 0;
}

static int di_process_uninit(struct di_process_dev *dev)
{
	int ret = 0;
	int i = 0;
	bool dropped;
	struct di_buffer *buf;

	dev->inited = false;

	if (dev->kthread) {
		dev->thread_need_stop = true;
		kthread_stop(dev->kthread);
		wake_up_interruptible(&dev->wq);
		dev->kthread = NULL;
		dev->thread_need_stop = false;
	}

	while (1) {
		i++;
		if (dev->thread_stopped)
			break;
		usleep_range(9000, 10000);
		if (i > WAIT_THREAD_STOPPED_TIMEOUT) {
			pr_err("wait thread timeout\n");
			break;
		}
	}

	if (dev->di_index >= 0) {
		ret = di_destroy_instance(dev->di_index);
		if (ret != 0)
			dp_print(dev->index, PRINT_ERROR,
				  "destroy di fail, di_index=%d\n",
				  dev->di_index);
		dev->di_index = -1;
	}

	receive_q_uninit(dev);
	file_q_uninit(dev);

	/*input buffer in di side need fput file*/
	for (i = 0; i < DIPR_POOL_SIZE; i++) {
		buf = &dev->di_input[i];
		if (buf->caller_mng.queued) {
			dp_print(dev->index, PRINT_OTHER,
				  "%s omx_index=%d\n", __func__, buf->vf->omx_index);
			dropped = buf->caller_mng.dropped;
			if (!dropped)
				dp_put_file(dev, buf->caller_mng.src_file);

			buf->caller_mng.queued = false;

			if (!kfifo_put(&dev->di_input_free_q, buf))
				dp_print(dev->index, PRINT_ERROR,
				"%s: put di_input_free_q fail\n", __func__);
		}
	}

	if (dev->fget_count != dev->fput_count)
		dp_print(dev->index, PRINT_OTHER,
			  "file leak!!!, fget_count=%lld, fput_count=%lld\n",
			  dev->fget_count, dev->fput_count);

	if (dev->fence_creat_count != dev->fence_release_count) {
		dp_print(dev->index, PRINT_ERROR,
			"fence_creat_count =%lld, fence_release_count=%lld\n",
			dev->fence_creat_count, dev->fence_release_count);
		dp_timeline_increase(dev, dev->fence_creat_count
				- dev->fence_release_count);
	}

	dp_print(dev->index, PRINT_OTHER,
		  "fill_done/fill: cur: %lld/%lld, total: %d/%d\n",
		  dev->fill_done_count, dev->fill_count,
		  total_fill_done_count, total_fill_count);

	return ret;
}

static int di_process_set_tvp(struct di_process_dev *dev, bool is_tvp)
{
	int ret = 0;

	if (is_tvp)
		dev->di_parm.output_format |= DI_OUTPUT_TVP;
	else
		dev->di_parm.output_format &= ~DI_OUTPUT_TVP;

	if (dev->di_index >= 0) {
		ret = di_destroy_instance(dev->di_index);
		if (ret != 0)
			dp_print(dev->index, PRINT_ERROR,
				  "destroy di fail, di_index=%d\n",
				  dev->di_index);
	}

	dev->di_index = di_create_instance(dev->di_parm);
	if (dev->di_index < 0) {
		dp_print(dev->index, PRINT_ERROR,
			  "creat di fail, di_index=%d\n",
			  dev->di_index);
		return dev->di_index;
	}
	dev->di_is_tvp = is_tvp;
	return 0;
}

static int di_process_set_frame(struct di_process_dev *dev, struct frame_info_t *frame_info)
{
	int i;
	struct file *file_vf = NULL;
	struct vframe_s *vf;
	struct dma_buf *dmabuf;
	int out_fd;
	int out_fence_fd;
	struct file_private_data *private_data = NULL;
	int ret;
	u32 is_repeat = false;
	u32 omx_index = 0;
	u32 max_width_new = 0, max_width_last = 0;

	if (!dev || !frame_info) {
		pr_err("%s: param is invalid.\n", __func__);
		return -EINVAL;
	}

	if (!dev->inited) {
		dp_print(dev->index, PRINT_ERROR,
			 "set_frame but not inited\n");
		return -EINVAL;
	}

	file_vf = dp_get_file(dev, frame_info->in_fd);
	if (!file_vf) {
		dp_print(dev->index, PRINT_ERROR,
			 "%s: fd is invalid!!!\n", __func__);
		return -EINVAL;
	}

	/*need add: check file has vf, if not return bypass*/
	vf = get_vf_from_file(dev, file_vf);
	if (!vf) {
		dp_print(dev->index, PRINT_OTHER, "fd no vf, bypass\n");
		ret = -EINVAL;
		goto error1;
	}

	dp_print(dev->index, PRINT_OTHER,
		"%s: len =%d, fd=%d, omx_index=%d, file_vf=%px, file_count=%ld\n",
		__func__, kfifo_len(&dev->receive_q), frame_info->in_fd, vf->omx_index,
		 file_vf, file_count(file_vf));

	/*first vf need check tvp*/
	if (dev->last_vf.type == 0) {
		if ((vf->flag & VFRAME_FLAG_VIDEO_SECURE) && !dev->di_is_tvp) {
			dp_print(dev->index, PRINT_OTHER, "need reinit to tvp");
			di_process_set_tvp(dev, true);
		} else if (!(vf->flag & VFRAME_FLAG_VIDEO_SECURE) && dev->di_is_tvp) {
			dp_print(dev->index, PRINT_OTHER, "need reinit to non-tvp");
			di_process_set_tvp(dev, false);
		}
	} else {
		if ((vf->flag & VFRAME_FLAG_VIDEO_SECURE) && !dev->di_is_tvp) {
			dp_print(dev->index, PRINT_ERROR, "need uplayer reinit to tvp");
			dp_put_file(dev, file_vf);
			frame_info->is_tvp = true;
			return 1;
		} else if (!(vf->flag & VFRAME_FLAG_VIDEO_SECURE) && dev->di_is_tvp) {
			dp_print(dev->index, PRINT_ERROR, "need uplayer reinit to non-tvp");
			dp_put_file(dev, file_vf);
			frame_info->is_tvp = false;
			return 1;
		}
	}

	/*support I and P to switch while open vpp pre link*/
	if (dim_get_pre_link()) {
		dp_print(dev->index, PRINT_OTHER, "chek I/P switch.\n");
		if ((vf->type & VIDTYPE_INTERLACE) && !dev->cur_is_i) {
			dp_print(dev->index, PRINT_ERROR, "need uplayer reinit to I");
			dev->cur_is_i = true;
			dp_put_file(dev, file_vf);
			return 2;
		} else if (!(vf->type & VIDTYPE_INTERLACE) && dev->cur_is_i) {
			dp_print(dev->index, PRINT_ERROR, "need uplayer reinit to P");
			dev->cur_is_i = false;
			dp_put_file(dev, file_vf);
			return 2;
		}
	}

	omx_index = vf->omx_index;

	dp_print(dev->index, PRINT_OTHER,
		"omx_index=%d,type=0x%x,flag=0x%x,compWidth=%d,compHeight=%d,width=%d,height=%d.\n",
		vf->omx_index, vf->type, vf->flag, vf->compWidth, vf->compHeight, vf->width,
		vf->height);

	/*1080p->1080i; 4k->1080i*/
	max_width_new = vf->compWidth >= vf->width ? vf->compWidth : vf->width;
	max_width_last = dev->last_vf.compWidth >= dev->last_vf.width
		? dev->last_vf.compWidth : dev->last_vf.width;
	if ((!(dev->last_vf.type & VIDTYPE_INTERLACE) && (vf->type & VIDTYPE_INTERLACE)) ||
		(max_width_last <= 1920 && max_width_new > 1920) ||
		(max_width_last > 1920 && max_width_new <= 1920)) {
		dp_print(dev->index, PRINT_OTHER, "fmt change\n");
		if (dev->first_out) {
			dev->first_out = false;
			dev->q_dummy_frame_done = false;
		}
	}
	dev->last_vf = *vf;

	if (vf->flag & VFRAME_FLAG_VIDEO_SECURE)
		frame_info->is_tvp = true;
	else
		frame_info->is_tvp = false;

	if (!dev->first_out) {
		dp_print(dev->index, PRINT_OTHER, "not first out.\n");
		dev->last_frame_bypass = true;
		if (dev->q_dummy_frame_done) {
			frame_info->out_fd = -1;
			frame_info->out_fence_fd = -1;
			frame_info->is_i = vf->type & VIDTYPE_INTERLACE;
			frame_info->omx_index = vf->omx_index;
			frame_info->need_bypass = true;
			dev->last_file = file_vf;

			dp_print(dev->index, PRINT_OTHER, "dummy has done, need bypass\n");
			dp_put_file(dev, file_vf);
			return 0;
		}
		dp_print(dev->index, PRINT_OTHER, "q dummy vf to DI\n");
		i = get_received_frame_free_index(dev);

		memcpy(&dev->dummy_vf, vf, sizeof(struct vframe_s));
		dev->received_frame[i].file_vf = NULL;
		dev->received_frame[i].vf = &dev->dummy_vf;
		dev->received_frame[i].dummy = true;

		dev->dummy_vf.decontour_pre = NULL;
		dev->dummy_vf.hdr10p_data_size = 0;
		dev->dummy_vf.hdr10p_data_buf = NULL;
		dev->dummy_vf.meta_data_size = 0;
		dev->dummy_vf.meta_data_buf = NULL;
		dev->dummy_vf.vf_ud_param.magic_code = 0;
		dev->dummy_vf.src_fmt.sei_magic_code = 0;

		atomic_set(&dev->received_frame[i].on_use, true);

		if (!kfifo_put(&dev->receive_q, &dev->received_frame[i]))
			dp_print(dev->index, PRINT_ERROR, "put ready fail\n");

		wake_up_interruptible(&dev->wq);

		if (vf->type & VIDTYPE_INTERLACE) {
			dp_print(dev->index, PRINT_OTHER, "q dummy seconf vf to DI\n");

			i = get_received_frame_free_index(dev);

			memcpy(&dev->dummy_vf1, &dev->dummy_vf, sizeof(struct vframe_s));
			dev->dummy_vf1.type &= ~VIDTYPE_TYPEMASK;
			dev->dummy_vf1.height >>= 1;
			dev->dummy_vf1.compHeight >>= 1;
			dev->received_frame[i].file_vf = NULL;
			dev->received_frame[i].vf = &dev->dummy_vf1;
			dev->received_frame[i].dummy = true;

			atomic_set(&dev->received_frame[i].on_use, true);
			if (!kfifo_put(&dev->receive_q, &dev->received_frame[i]))
				dp_print(dev->index, PRINT_ERROR, "put ready fail\n");
			wake_up_interruptible(&dev->wq);
		}

		dev->q_dummy_frame_done = true;

		frame_info->out_fd = -1;
		frame_info->out_fence_fd = -1;
		frame_info->is_i = vf->type & VIDTYPE_INTERLACE;
		frame_info->omx_index = vf->omx_index;
		frame_info->need_bypass = true;
		dev->last_file = file_vf;

		dp_put_file(dev, file_vf);
		return 0;
	}

	dp_print(dev->index, PRINT_OTHER, "first out.\n");

	if (dev->last_file == file_vf) {
		if (dev->last_frame_bypass) {
			dp_print(dev->index, PRINT_OTHER, "repeat:last bypass, continue bypass\n");
			frame_info->out_fd = -1;
			frame_info->out_fence_fd = -1;
			frame_info->is_i = vf->type & VIDTYPE_INTERLACE;
			frame_info->omx_index = vf->omx_index;
			frame_info->need_bypass = true;
			dev->last_file = file_vf;

			dp_put_file(dev, file_vf);
			return 0;
		}

		dp_put_file(dev, file_vf);
		dmabuf = dev->last_dmabuf;
		dp_print(dev->index, PRINT_OTHER, "repeat frame\n");
		is_repeat = true;
		out_fence_fd = -1;
	} else {
		i = get_received_frame_free_index(dev);

		if (!kfifo_get(&dev->file_free_q, &dmabuf)) {
			dp_print(dev->index, PRINT_ERROR, "peek free dma_buf fail!!!\n");
			ret = -EINVAL;
			goto error1;
		}
		private_data = di_proc_get_file_private_data(dmabuf->file, true);

		//private_data->vf = *vf;
		private_data->vf_p = NULL;
		private_data->vf.omx_index = vf->omx_index;
		private_data->vf.index_disp = vf->index_disp;
		private_data->file = file_vf;
		omx_index = vf->omx_index;

		if (!kfifo_put(&dev->file_wait_q, dmabuf))
			dp_print(dev->index, PRINT_ERROR, "put file_wait fail\n");

		dev->received_frame[i].file_vf = file_vf;
		dev->received_frame[i].vf = vf;
		dev->received_frame[i].dummy = false;

		atomic_set(&dev->received_frame[i].on_use, true);

		if (!kfifo_put(&dev->receive_q, &dev->received_frame[i]))
			dp_print(dev->index, PRINT_ERROR, "put ready fail\n");

		wake_up_interruptible(&dev->wq);

		out_fence_fd = dp_timeline_create_fence(dev);
	}

	frame_info->out_fence_fd = out_fence_fd;

	out_fd = get_unused_fd_flags(O_CLOEXEC);
	if (out_fd < 0) {
		dp_print(dev->index, PRINT_ERROR, "not fd!!!\n");
		return -ENOMEM;
	}
	fd_install(out_fd, dmabuf->file);

	dma_buf_get(out_fd);
	frame_info->out_fd = out_fd;
	frame_info->is_repeat = is_repeat;
	frame_info->is_i = vf->type & VIDTYPE_INTERLACE;
	frame_info->omx_index = omx_index;

	dp_print(dev->index, PRINT_OTHER,
		"%s done: dmabuf =%px, dmabuf->file=%px, out_fd=%d, out_fence_fd =%d, is_i=%d\n",
		__func__, dmabuf, dmabuf->file, out_fd, out_fence_fd, frame_info->is_i);

	dev->last_file = file_vf;
	dev->last_dmabuf = dmabuf;
	dev->last_frame_bypass = false;
	return 0;

error1:
	dp_put_file(dev, file_vf);
	return ret;
}

static int di_process_q_output(struct di_process_dev *dev, u32 fd)
{
	struct file *file_vf = NULL;
	struct di_buffer *di_p = NULL;
	struct file_private_data *private_data = NULL;
	struct dma_buf *dmabuf;
	struct vframe_s *vf;
	int omx_index = -1;

	dp_print(dev->index, PRINT_OTHER, "%s: fd = %d\n", __func__, fd);

	file_vf = fget(fd);
	if (!file_vf) {
		dp_print(dev->index, PRINT_ERROR, "%s: fd invalid!!!\n", __func__);
		return 0;
	}
	if (!file_vf->private_data) {
		dp_print(dev->index, PRINT_ERROR, "%s: private_data NULL\n", __func__);
		return 0;
	}
	total_get_count++;
	dp_print(dev->index, PRINT_OTHER,
		"%s: file_vf=%px, file_vf->private_data=%px\n",
		__func__, file_vf, file_vf->private_data);

	private_data = di_proc_get_file_private_data(file_vf, false);
	if (!private_data) {
		dp_print(dev->index, PRINT_ERROR,
			"%s: private_data null, put file=%px.\n",
			__func__, file_vf);
		fput(file_vf);
		total_put_count++;
		return 0;
	}
	/*if di bypass, not need queue di_buffer to di, it is input di_buffer*/
	if (private_data->flag & V4LVIDEO_FLAG_DI_V3) {
		di_p = (struct di_buffer *)(private_data->vf_p);
		dp_print(dev->index, PRINT_OTHER,
			"%s: no bypss need put file=%px.\n",
			__func__, private_data->file);
		/*di vf has dec vf, need put dec file*/
		if (di_p->vf->flag & VFRAME_FLAG_DOUBLE_FRAM) {
			/*decoder vf maybe free, so should not to use vf struct*/
			vf = di_p->vf->vf_ext;
			if (vf && private_data->file)
				dp_put_file(dev, private_data->file);
			else
				dp_print(dev->index, PRINT_ERROR,
					"%s: has dec vf, but vf/file is null vf=%px.\n",
					__func__, vf);
			private_data->file = NULL;
		}
		omx_index = di_p->vf->omx_index;
		queue_outbuf_to_di(dev, di_p);
	} else if (private_data->flag & V4LVIDEO_FLAG_DI_BYPASS) {
		/*di bypass, need put dec file*/
		/*decoder vf maybe free, so should not to use vf struct*/
		vf = (struct vframe_s *)(private_data->vf_p);
		dp_print(dev->index, PRINT_OTHER,
			"%s: bypss need put file=%px.\n",
			__func__, private_data->file);
		if (vf && private_data->file) {
			omx_index = vf->omx_index;
			dp_put_file(dev, private_data->file);
		} else {
			dp_print(dev->index, PRINT_ERROR,
				"%s: di bypass, but dec vf/file is null vf=%px.\n",
				__func__, vf);
		}
		private_data->file = NULL;
	}

	dp_print(dev->index, PRINT_OTHER, "%s: di_buffer =%px, omx_index=%d.\n",
		__func__, di_p, omx_index);
	private_data->vf_p = NULL;
	private_data->is_keep = false;
	dmabuf = (struct dma_buf *)file_vf->private_data;
	if (!kfifo_put(&dev->file_free_q, dmabuf))
		dp_print(dev->index, PRINT_ERROR, "%s: file_free_q put fail\n", __func__);
	fput(file_vf);
	total_put_count++;
	return 0;
}

static long di_process_ioctl(struct file *file,
				 unsigned int cmd, ulong arg)
{
	long ret = 0;
	void __user *argp = (void __user *)arg;
	struct frame_info_t frame_info;
	struct di_process_dev *dev = (struct di_process_dev *)file->private_data;
	int fd;

	switch (cmd) {
	case DI_PROCESS_IOCTL_INIT:
		ret = di_process_init(dev);
		break;
	case DI_PROCESS_IOCTL_UNINIT:
		ret = di_process_uninit(dev);
		break;
	case DI_PROCESS_IOCTL_SET_FRAME:
		if (copy_from_user(&frame_info, argp,
				   sizeof(frame_info)) == 0) {
			ret = di_process_set_frame(dev, &frame_info);
			if (ret != 0)
				return ret;
			ret = copy_to_user(argp, &frame_info,
					   sizeof(struct frame_info_t));
		} else {
			ret = -EFAULT;
		}
		break;
	case DI_PROCESS_IOCTL_Q_OUTPUT:
		if (copy_from_user(&fd, argp, sizeof(u32)) == 0)
			ret = di_process_q_output(dev, fd);
		else
			ret = -EFAULT;
		break;

	default:
		return -EINVAL;
	}
	return ret;
}

#ifdef CONFIG_COMPAT
static long di_process_compat_ioctl(struct file *file, unsigned int cmd,
					ulong arg)
{
	long ret = 0;

	ret = di_process_ioctl(file, cmd, (ulong)compat_ptr(arg));
	return ret;
}
#endif

static int di_process_open(struct inode *inode, struct file *file)
{
	struct di_process_dev *dev;
	struct di_process_port_s *port;
	int index;

	index = iminor(inode);
	pr_info("%s iminor(inode) =%d\n", __func__, index);
	if (index >= DI_INSTANCE_COUNT)
		return -ENODEV;

	port = &ports[index];

	mutex_lock(&di_process_mutex);

	if (port->open_count > 0) {
		mutex_unlock(&di_process_mutex);
		pr_err("di_process: instance %d is already opened",
		       port->index);
		return -EBUSY;
	}

	dev = vmalloc(sizeof(*dev));
	if (!dev) {
		mutex_unlock(&di_process_mutex);
		pr_err("di_process: instance %d alloc dev failed",
		       port->index);
		return -ENOMEM;
	}
	memset(dev, 0, sizeof(struct di_process_dev));

	dev->port = port;
	file->private_data = dev;
	dev->index = port->index;
	port->open_count++;

	mutex_unlock(&di_process_mutex);

	return 0;
}

static int di_process_release(struct inode *inode, struct file *file)
{
	struct di_process_dev *dev = file->private_data;
	struct di_process_port_s *port = dev->port;
	int ret = 0;

	pr_info("di process release\n");

	if (iminor(inode) >= di_process_instance_num)
		return -ENODEV;

	if (dev->inited) {
		dp_print(dev->index, PRINT_ERROR,
			"no uninit before close, so need uninit first\n");
		ret = di_process_uninit(dev);
		if (ret != 0)
			pr_err("%s, disable fail\n", __func__);
	}

	mutex_lock(&di_process_mutex);
	port->open_count--;
	mutex_unlock(&di_process_mutex);

	vfree(dev);
	return 0;
}

static const struct file_operations di_process_fops = {
	.owner = THIS_MODULE,
	.open = di_process_open,
	.release = di_process_release,
	.unlocked_ioctl = di_process_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = di_process_compat_ioctl,
#endif
	.poll = NULL,
};

static ssize_t print_flag_show(struct class *cla,
			       struct class_attribute *attr,
			       char *buf)
{
	return snprintf(buf, 80,
			"current print_flag is %d\n",
			print_flag);
}

static ssize_t print_flag_store(struct class *cla,
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
	print_flag = tmp;
	return count;
}

static ssize_t total_get_count_show(struct class *class,
				      struct class_attribute *attr,
				      char *buf)
{
	return sprintf(buf, "%d\n", total_get_count);
}

static ssize_t total_put_count_show(struct class *class,
				      struct class_attribute *attr,
				      char *buf)
{
	return sprintf(buf, "%d\n", total_put_count);
}

static ssize_t total_src_get_count_show(struct class *class,
				      struct class_attribute *attr,
				      char *buf)
{
	return sprintf(buf, "%d\n", total_src_get_count);
}

static ssize_t total_src_put_count_show(struct class *class,
				      struct class_attribute *attr,
				      char *buf)
{
	return sprintf(buf, "%d\n", total_src_put_count);
}

static ssize_t total_fill_count_show(struct class *class,
				      struct class_attribute *attr,
				      char *buf)
{
	return sprintf(buf, "%d\n", total_fill_count);
}

static ssize_t total_fill_done_count_show(struct class *class,
				      struct class_attribute *attr,
				      char *buf)
{
	return sprintf(buf, "%d\n", total_fill_done_count);
}

static ssize_t total_empty_count_show(struct class *class,
				      struct class_attribute *attr,
				      char *buf)
{
	return sprintf(buf, "%d\n", total_empty_count);
}

static ssize_t total_empty_done_count_show(struct class *class,
				      struct class_attribute *attr,
				      char *buf)
{
	return sprintf(buf, "%d\n", total_empty_done_count);
}

static ssize_t buf_mgr_print_flag_show(struct class *class,
				      struct class_attribute *attr,
				      char *buf)
{
	return sprintf(buf, "%d\n", dp_buf_mgr_print_flag);
}

static ssize_t buf_mgr_print_flag_store(struct class *cla,
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
	dp_buf_mgr_print_flag = tmp;
	return count;
}

static ssize_t di_proc_enable_show(struct class *class,
				      struct class_attribute *attr,
				      char *buf)
{
	return sprintf(buf, "%d\n", di_proc_enable);
}

static ssize_t di_proc_enable_store(struct class *cla,
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
	di_proc_enable = tmp;
	return count;
}

static ssize_t q_dropped_show(struct class *class,
				      struct class_attribute *attr,
				      char *buf)
{
	return sprintf(buf, "%d\n", q_dropped);
}

static ssize_t q_dropped_store(struct class *cla,
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
	q_dropped = tmp;
	return count;
}

static CLASS_ATTR_RW(print_flag);
static CLASS_ATTR_RO(total_get_count);
static CLASS_ATTR_RO(total_put_count);
static CLASS_ATTR_RO(total_src_get_count);
static CLASS_ATTR_RO(total_src_put_count);
static CLASS_ATTR_RO(total_fill_count);
static CLASS_ATTR_RO(total_fill_done_count);
static CLASS_ATTR_RO(total_empty_count);
static CLASS_ATTR_RO(total_empty_done_count);
static CLASS_ATTR_RW(buf_mgr_print_flag);
static CLASS_ATTR_RW(di_proc_enable);
static CLASS_ATTR_RW(q_dropped);

static struct attribute *di_process_class_attrs[] = {
	&class_attr_print_flag.attr,
	&class_attr_total_get_count.attr,
	&class_attr_total_put_count.attr,
	&class_attr_total_src_get_count.attr,
	&class_attr_total_src_put_count.attr,
	&class_attr_total_fill_count.attr,
	&class_attr_total_fill_done_count.attr,
	&class_attr_total_empty_count.attr,
	&class_attr_total_empty_done_count.attr,
	&class_attr_buf_mgr_print_flag.attr,
	&class_attr_di_proc_enable.attr,
	&class_attr_q_dropped.attr,
	NULL
};

ATTRIBUTE_GROUPS(di_process_class);

static struct class di_process_class = {
	.name = "di_process",
	.class_groups = di_process_class_groups,
};

static const struct of_device_id amlogic_di_process_dt_match[] = {
	{.compatible = "amlogic, di_process",
	},
	{},
};

static int di_process_probe(struct platform_device *pdev)
{
	int ret = 0;
	int i = 0;
	struct di_process_port_s *st;

	pr_debug("di process probe\n");
	/*need read from dts*/

	ret = class_register(&di_process_class);
	if (ret < 0)
		return ret;
	ret = register_chrdev(DI_PROCESS_MAJOR,
			      "di_process", &di_process_fops);
	if (ret < 0) {
		pr_err("Can't allocate major for di_process device\n");
		goto error1;
	}

	for (st = &ports[0], i = 0;
	     i < di_process_instance_num; i++, st++) {
		pr_debug("%s:ports[i].name=%s, i=%d\n", __func__,
		       ports[i].name, i);
		st->pdev = &pdev->dev;
		st->class_dev = device_create(&di_process_class, NULL,
					      MKDEV(DI_PROCESS_MAJOR, i),
					      NULL, ports[i].name);
		dp_timeline_create(i);
	}
	pr_debug("%s num=%d\n", __func__, di_process_instance_num);
	return ret;

error1:
	pr_err("%s error\n", __func__);
	unregister_chrdev(DI_PROCESS_MAJOR, "di_process");
	class_unregister(&di_process_class);
	return ret;
}

static int di_process_remove(struct platform_device *pdev)
{
	int i;
	struct di_process_port_s *st;

	for (st = &ports[0], i = 0;
	     i < di_process_instance_num; i++, st++)
		device_destroy(&di_process_class,
			       MKDEV(DI_PROCESS_MAJOR, i));

	unregister_chrdev(DI_PROCESS_MAJOR, DI_PROCESS_DEVICE_NAME);
	class_destroy(&di_process_class);
	return 0;
};

static struct platform_driver di_process_driver = {
	.probe = di_process_probe,
	.remove = di_process_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = "di_process",
		.of_match_table = amlogic_di_process_dt_match,
	}
};

int __init di_process_module_init(void)
{
	pr_debug("di process module init\n");
	if (platform_driver_register(&di_process_driver)) {
		pr_err("failed to register di_process module\n");
		return -ENODEV;
	}
	return 0;
}

void __exit di_process_module_exit(void)
{
	platform_driver_unregister(&di_process_driver);
}

//MODULE_DESCRIPTION("Video Technology Magazine di_process Capture Boad");
//MODULE_AUTHOR("Amlogic, Jintao Xu<jintao.xu@amlogic.com>");
//MODULE_LICENSE("GPL");
//MODULE_VERSION(DI_PROCESS_VERSION);

