// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * drivers/amlogic/media/video_processor/video_composer/videodisplay.c
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
 #include <linux/amlogic/meson_uvm_core.h>
 #include <linux/amlogic/media/utils/am_com.h>
 #ifdef CONFIG_AMLOGIC_MEDIA_VIDEO
#include <linux/amlogic/media/video_sink/video.h>
#endif
#include "videodisplay.h"

static struct timeval vsync_time;
static DEFINE_MUTEX(video_display_mutex);
static struct composer_dev *mdev[3];

#define MAX_VIDEO_COMPOSER_INSTANCE_NUM 3
static int get_count[MAX_VIDEO_COMPOSER_INSTANCE_NUM];
static unsigned int countinue_vsync_count[MAX_VD_LAYERS];

void vsync_notify_video_composer(void)
{
	int i;
	int count = MAX_VIDEO_COMPOSER_INSTANCE_NUM;

	countinue_vsync_count[0]++;
	countinue_vsync_count[1]++;

	for (i = 0; i < count; i++)
		get_count[i] = 0;
	do_gettimeofday(&vsync_time);
}

static void vf_pop_display_q(struct composer_dev *dev, struct vframe_s *vf)
{
	struct vframe_s *dis_vf = NULL;

	int k = kfifo_len(&dev->display_q);

	while (kfifo_len(&dev->display_q) > 0) {
		if (kfifo_get(&dev->display_q, &dis_vf)) {
			if (dis_vf == vf)
				break;
			if (!kfifo_put(&dev->display_q, dis_vf))
				vc_print(dev->index, PRINT_ERROR,
					 "display_q is full!\n");
		}
		k--;
		if (k < 0) {
			vc_print(dev->index, PRINT_ERROR,
				 "can find vf in display_q\n");
			break;
		}
	}
}

static void vc_display_q_uninit(struct composer_dev *dev)
{
	struct vframe_s *dis_vf = NULL;
	struct file *file_vf = NULL;
	int repeat_count;
	int i;
	struct vd_prepare_s *vd_prepare;

	vc_print(dev->index, PRINT_QUEUE_STATUS,
		"%s: display_q len=%d\n",
		__func__, kfifo_len(&dev->display_q));

	while (kfifo_len(&dev->display_q) > 0) {
		if (kfifo_get(&dev->display_q, &dis_vf)) {
			vd_prepare = container_of(dis_vf,
						struct vd_prepare_s,
						dst_frame);
			if (IS_ERR_OR_NULL(vd_prepare)) {
				vc_print(dev->index, PRINT_ERROR,
					"%s: prepare is NULL.\n",
					__func__);
				return;
			}
			if (dis_vf->flag
			    & VFRAME_FLAG_VIDEO_COMPOSER_BYPASS) {
				repeat_count = dis_vf->repeat_count[dev->index];
				file_vf = dis_vf->file_vf;
				vc_print(dev->index, PRINT_FENCE,
					 "%s: repeat_count=%d, omx_index=%d\n",
					 __func__,
					 repeat_count,
					 dis_vf->omx_index);
				for (i = 0; i <= repeat_count; i++) {
					dma_buf_put((struct dma_buf *)file_vf);
					dma_fence_signal(vd_prepare->release_fence);
					dev->fput_count++;
				}
			} else if (!(dis_vf->flag
				     & VFRAME_FLAG_VIDEO_COMPOSER)) {
				vc_print(dev->index, PRINT_ERROR,
					 "%s: display_q flag is null, omx_index=%d\n",
					 __func__, dis_vf->omx_index);
			}
		}
	}
}

static void vc_ready_q_uninit(struct composer_dev *dev)
{
	struct vframe_s *dis_vf = NULL;
	struct file *file_vf = NULL;
	int repeat_count;
	int i;
	struct vd_prepare_s *vd_prepare;

	vc_print(dev->index, PRINT_QUEUE_STATUS,
		"%s: ready_q len=%d\n",
		__func__, kfifo_len(&dev->ready_q));

	while (kfifo_len(&dev->ready_q) > 0) {
		if (kfifo_get(&dev->ready_q, &dis_vf)) {
			vd_prepare = container_of(dis_vf,
						struct vd_prepare_s,
						dst_frame);
			if (IS_ERR_OR_NULL(vd_prepare)) {
				vc_print(dev->index, PRINT_ERROR,
					"%s: prepare is NULL.\n",
					__func__);
				return;
			}
			if (dis_vf->flag
			    & VFRAME_FLAG_VIDEO_COMPOSER_BYPASS) {
				repeat_count = dis_vf->repeat_count[dev->index];
				file_vf = dis_vf->file_vf;
				for (i = 0; i <= repeat_count; i++) {
					dma_buf_put((struct dma_buf *)file_vf);
					dma_fence_signal(vd_prepare->release_fence);
					dev->fput_count++;
				}
			}
		}
	}
}

/* -----------------------------------------------------------------
 *           provider opeations
 * -----------------------------------------------------------------
 */
static struct vframe_s *vc_vf_peek(void *op_arg)
{
	struct composer_dev *dev = (struct composer_dev *)op_arg;
	struct vframe_s *vf = NULL;
	struct timeval time1;
	struct timeval time2;
	u64 time_vsync;
	int interval_time;

	/*apk/sf drop 0/3 4; vc receive 1 2 5 in one vsync*/
	/*apk queue 5 and wait 1, it will fence timeout*/
	if (get_count[dev->index] == 2) {
		vc_print(dev->index, PRINT_ERROR,
			 "has already get 2, can not get more\n");
		return NULL;
	}

	time1 = dev->start_time;
	time2 = vsync_time;
	if (kfifo_peek(&dev->ready_q, &vf)) {
		if (vf && get_count[dev->index] > 0 &&
			!(vf->flag & VFRAME_FLAG_GAME_MODE)) {
			time_vsync = (u64)1000000
				* (time2.tv_sec - time1.tv_sec)
				+ time2.tv_usec - time1.tv_usec;
			interval_time = abs(time_vsync - vf->pts_us64);
			vc_print(dev->index, PRINT_PERFORMANCE,
				 "time_vsync=%lld, vf->pts_us64=%lld\n",
				 time_vsync, vf->pts_us64);
			//TODO
			if (interval_time < 2000/*margin_time*/) {
				vc_print(dev->index, PRINT_PATTERN,
					 "display next vsync\n");
				return NULL;
			}
		}
		if (!vf)
			return NULL;

		if (dev->is_drm_enable)
			return vf;
		else
			return videocomposer_vf_peek(op_arg);
	} else {
		return NULL;
	}
}

static struct vframe_s *vc_vf_get(void *op_arg)
{
	struct composer_dev *dev = (struct composer_dev *)op_arg;
	struct vframe_s *vf = NULL;

	if (kfifo_get(&dev->ready_q, &vf)) {
		if (!vf)
			return NULL;

		if (vf->flag & VFRAME_FLAG_FAKE_FRAME)
			return vf;
		if (!kfifo_put(&dev->display_q, vf))
			vc_print(dev->index, PRINT_ERROR,
				 "display_q is full!\n");
		if (!(vf->flag
		      & VFRAME_FLAG_VIDEO_COMPOSER)) {
			pr_err("vc: vf_get: flag is null\n");
		}

		get_count[dev->index]++;
		dev->fget_count++;

		vc_print(dev->index, PRINT_OTHER | PRINT_PATTERN,
			 "%s: omx_index=%d, index_disp=%d, get_count=%d, total_get_count=%lld, vsync =%d, vf=%p\n",
			 __func__,
			 vf->omx_index,
			 vf->index_disp,
			 get_count[dev->index],
			 dev->fget_count,
			 countinue_vsync_count[dev->index],
			 vf);
		countinue_vsync_count[dev->index] = 0;

		return vf;
	} else {
		return NULL;
	}
}

static void vc_vf_put(struct vframe_s *vf, void *op_arg)
{
	int repeat_count;
	int i;
	struct file *file_vf;
	struct composer_dev *dev = (struct composer_dev *)op_arg;
	struct vd_prepare_s *vd_prepare_tmp;

	if (!vf)
		return;

	if (dev->is_drm_enable) {
		if (vf->flag & VFRAME_FLAG_FAKE_FRAME) {
			vc_print(dev->index, PRINT_OTHER,
				 "put: fake frame\n");
			return;
		}

		vd_prepare_tmp = container_of(vf,
					struct vd_prepare_s,
					dst_frame);
		if (IS_ERR_OR_NULL(vd_prepare_tmp)) {
			vc_print(dev->index, PRINT_ERROR,
				"%s: prepare is NULL.\n",
				__func__);
			return;
		}
		repeat_count = vf->repeat_count[dev->index];
		file_vf = vf->file_vf;

		vf_pop_display_q(dev, vf);
		for (i = 0; i <= repeat_count; i++) {
			if (file_vf) {
				dma_buf_put((struct dma_buf *)file_vf);
				dma_fence_signal(vd_prepare_tmp->release_fence);
				dev->fput_count++;
			} else {
				vc_print(dev->index, PRINT_ERROR,
					"put: file error!!!\n");
			}
		}
		vd_prepare_data_q_put(dev, vd_prepare_tmp);
		vc_print(dev->index, PRINT_OTHER | PRINT_PATTERN,
			"%s: omx_index=%d, put_count=%lld.\n",
			__func__, vf->omx_index, dev->fput_count);
	} else {
		vf_pop_display_q(dev, vf);
		videocomposer_vf_put(vf, op_arg);
	}
}

static int vc_event_cb(int type, void *data, void *private_data)
{
	if (type & VFRAME_EVENT_RECEIVER_PUT)
		;
	else if (type & VFRAME_EVENT_RECEIVER_GET)
		;
	else if (type & VFRAME_EVENT_RECEIVER_FRAME_WAIT)
		;
	return 0;
}

static int vc_vf_states(struct vframe_states *states, void *op_arg)
{
	struct composer_dev *dev = (struct composer_dev *)op_arg;

	states->vf_pool_size = COMPOSER_READY_POOL_SIZE;
	states->buf_recycle_num = 0;
	states->buf_free_num = COMPOSER_READY_POOL_SIZE
		- kfifo_len(&dev->ready_q);
	states->buf_avail_num = kfifo_len(&dev->ready_q);
	return 0;
}

static const struct vframe_operations_s vc_vf_provider = {
	.peek = vc_vf_peek,
	.get = vc_vf_get,
	.put = vc_vf_put,
	.event_cb = vc_event_cb,
	.vf_states = vc_vf_states,
};

static struct vframe_s *vd_get_vf_from_buf(struct composer_dev *dev,
						struct dma_buf *buf)
{
	struct vframe_s *vf = NULL;
	bool is_dec_vf = false;
	struct uvm_hook_mod *uhmod;
	struct file_private_data *temp_file;

	if (IS_ERR_OR_NULL(buf)) {
		vc_print(dev->index, PRINT_ERROR,
			"%s: dma_buf is NULL.\n",
			__func__);
		return vf;
	}

	is_dec_vf = is_valid_mod_type(buf, VF_SRC_DECODER);
	if (is_dec_vf) {
		vc_print(dev->index, PRINT_OTHER, "vf is from decoder.\n");
		vf = dmabuf_get_vframe(buf);
		dmabuf_put_vframe(buf);
	} else {
		vc_print(dev->index, PRINT_OTHER, "vf is from v4lvideo.\n");
		uhmod = uvm_get_hook_mod(buf, VF_PROCESS_V4LVIDEO);
		if (!uhmod) {
			vc_print(dev->index, PRINT_ERROR,
				"get vframe from v4lvideo failed.\n");
			return vf;
		}

		if (IS_ERR_VALUE(uhmod) || !uhmod->arg) {
			vc_print(dev->index, PRINT_ERROR,
				 "vframe in v4lvideo is NULL.\n");
			return vf;
		}
		temp_file = uhmod->arg;
		vf = &temp_file->vf;
		uvm_put_hook_mod(buf, VF_PROCESS_V4LVIDEO);
	}

	return vf;
}

static void vd_disable_video_layer(struct composer_dev *dev, int val)
{
	vc_print(dev->index, PRINT_ERROR, "%s: val is %d.\n", __func__, val);
	if (dev->index == 0) {
		_video_set_disable(val);
	} else {
		_videopip_set_disable(dev->index, val);
	}
}

void vd_prepare_data_q_put(struct composer_dev *dev,
				struct vd_prepare_s *vd_prepare)
{
	if (!vd_prepare)
		return;

	if (!kfifo_put(&dev->vc_prepare_data_q, vd_prepare))
		vc_print(dev->index, PRINT_ERROR,
			"%s: vc_prepare_data_q is full!\n",
			__func__);
}

struct vd_prepare_s *vd_prepare_data_q_get(struct composer_dev *dev)
{
	struct vd_prepare_s *vd_prepare = NULL;

	if (!kfifo_get(&dev->vc_prepare_data_q, &vd_prepare)) {
		vc_print(dev->index, PRINT_ERROR,
			 "%s: get vc_prepare_data failed\n",
			 __func__);
		vd_prepare = NULL;
	} else {
		vd_prepare->src_frame = NULL;
		memset(&vd_prepare->dst_frame, 0, sizeof(struct vframe_s));
		vd_prepare->release_fence = NULL;
	}

	return vd_prepare;
}

int vd_render_index_get(struct composer_dev *dev)
{
	int reveiver_id = 0;
	int render_index = 0;

	if (!dev) {
		pr_info("%s: dev is null.\n", __func__);
	} else {
		if (dev->index >= MAX_VD_LAYERS) {
			vc_print(dev->index, PRINT_ERROR,
				"%s: invalid param.\n",
				__func__);
		} else {
			reveiver_id = get_receiver_id(dev->index);
			if (reveiver_id >= 5)
				render_index = reveiver_id - 3;
			else if (reveiver_id <= 3)
				render_index = reveiver_id - 2;
			else
				render_index = 0;
		}
		vc_print(dev->index, PRINT_ERROR,
			"%s: render_index is %d.\n",
			__func__, render_index);
	}
	return render_index;
}

int video_display_create_path(struct composer_dev *dev)
{
	char render_layer[16] = "";

	sprintf(render_layer, "video_render.%d", dev->video_render_index);
	snprintf(dev->vfm_map_chain, VCOM_MAP_NAME_SIZE,
		 "%s %s", dev->vf_provider_name, render_layer);

	snprintf(dev->vfm_map_id, VCOM_MAP_NAME_SIZE,
		 "vcom-map-%d", dev->index);

	if (vfm_map_add(dev->vfm_map_id, dev->vfm_map_chain) < 0) {
		vc_print(dev->index, PRINT_ERROR,
			"%s: vcom pipeline map creation failed %s.\n",
			__func__, dev->vfm_map_id);
		dev->vfm_map_id[0] = 0;
		return -ENOMEM;
	}

	vf_provider_init(&dev->vc_vf_prov, dev->vf_provider_name,
			 &vc_vf_provider, dev);

	vf_reg_provider(&dev->vc_vf_prov);

	vf_notify_receiver(dev->vf_provider_name,
			   VFRAME_EVENT_PROVIDER_START, NULL);
	return 0;
}

int video_display_release_path(struct composer_dev *dev)
{
	vf_unreg_provider(&dev->vc_vf_prov);

	if (dev->vfm_map_id[0]) {
		vfm_map_remove(dev->vfm_map_id);
		dev->vfm_map_id[0] = 0;
	}
	return 0;
}

static struct composer_dev *video_display_getdev(int layer_index)
{
	struct composer_dev *dev;
	struct video_composer_port_s *port;

	vc_print(layer_index, PRINT_OTHER,
		"%s: video_composerdev_%d.\n",
		__func__, layer_index);
	if (layer_index >= MAX_VIDEO_COMPOSER_INSTANCE_NUM) {
		vc_print(layer_index, PRINT_ERROR,
			"%s: layer-%d out of range.\n",
			__func__, layer_index);
		return NULL;
	}

	if (!mdev[layer_index]) {
		mutex_lock(&video_display_mutex);
		dev = kzalloc(sizeof(*dev), GFP_KERNEL);
		if (!dev) {
			mutex_unlock(&video_display_mutex);
			vc_print(layer_index, PRINT_ERROR,
				"%s: alloc dev failed.\n",
				__func__);
			return NULL;
		}

		port = video_composer_get_port(layer_index);
		dev->port = port;
		dev->index = port->index;
		mdev[layer_index] = dev;
		mutex_unlock(&video_display_mutex);
	}

	return mdev[layer_index];
}

static int video_display_init(int layer_index)
{
	int ret = 0, i = 0;
	struct composer_dev *dev;
	char render_layer[16] = "";

	dev = video_display_getdev(layer_index);
	if (!dev) {
		pr_info("%s: get dev failed.\n", __func__);
		return -EBUSY;
	}

	mutex_lock(&video_display_mutex);
	INIT_KFIFO(dev->ready_q);
	INIT_KFIFO(dev->display_q);
	INIT_KFIFO(dev->vc_prepare_data_q);
	kfifo_reset(&dev->ready_q);
	kfifo_reset(&dev->display_q);
	kfifo_reset(&dev->vc_prepare_data_q);

	for (i = 0; i < COMPOSER_READY_POOL_SIZE; i++)
		vd_prepare_data_q_put(dev, &dev->vd_prepare[i]);

	dev->fput_count = 0;
	dev->drop_frame_count = 0;
	dev->fget_count = 0;
	dev->received_count = 0;
	dev->last_file = NULL;
	dev->vd_prepare_last = NULL;
	dev->is_drm_enable = true;
	dev->video_render_index = vd_render_index_get(dev);
	memcpy(dev->vf_provider_name,
		dev->port->name,
		strlen(dev->port->name) + 1);
	dev->port->open_count++;
	do_gettimeofday(&dev->start_time);
	mutex_unlock(&video_display_mutex);

	vd_disable_video_layer(dev, 2);
	video_set_global_output(dev->index, 1);
	ret = video_display_create_path(dev);
	sprintf(render_layer, "video_render.%d", dev->video_render_index);
	set_video_path_select(render_layer, dev->index);

	return ret;
}

static int video_display_uninit(int layer_index)
{
	int ret;
	struct composer_dev *dev;

	dev = video_display_getdev(layer_index);
	if (!dev) {
		vc_print(layer_index, PRINT_ERROR,
			"%s: get dev failed.\n",
			__func__);
		return -EBUSY;
	}
	if (dev->index == 0)
		set_video_path_select("default", 0);
	set_blackout_policy(1);
	vd_disable_video_layer(dev, 1);
	video_set_global_output(dev->index, 0);
	ret = video_display_release_path(dev);
	vc_ready_q_uninit(dev);
	vc_display_q_uninit(dev);
	vc_print(dev->index, PRINT_OTHER | PRINT_PATTERN,
		 "%s: total put count is %lld.\n",
		 __func__, dev->fput_count);

	dev->port->open_count--;
	kfree(dev);
	mdev[layer_index] = NULL;
	return ret;
}

int video_display_setenable(int layer_index, int is_enable)
{
	int ret = 0;
	struct composer_dev *dev;

	if (is_enable > VIDEO_DISPLAY_ENABLE_NORMAL ||
	    is_enable < VIDEO_DISPLAY_ENABLE_NONE) {
		vc_print(layer_index, PRINT_ERROR,
			"%s: invalid param.\n",
			__func__);
		return -EINVAL;
	}

	vc_print(layer_index, PRINT_ERROR,
		"%s: val=%d.\n",
		 __func__, is_enable);

	dev = video_display_getdev(layer_index);
	if (!dev) {
		vc_print(layer_index, PRINT_ERROR,
			"%s: get dev failed.\n",
			__func__);
		return -EBUSY;
	}

	if (dev->enable_composer == is_enable) {
		vc_print(dev->index, PRINT_ERROR,
			"%s: same status, no need set.\n",
			__func__);
		return ret;
	}

	dev->enable_composer = is_enable;
	if (is_enable == VIDEO_DISPLAY_ENABLE_NORMAL)
		ret = video_display_init(layer_index);
	else
		ret = video_display_uninit(layer_index);

	return ret;
}
EXPORT_SYMBOL(video_display_setenable);

int video_display_setframe(int layer_index,
			struct video_display_frame_info_t *frame_info,
			int flags)
{
	struct composer_dev *dev;
	struct vframe_s *vf = NULL;
	int ready_count = 0;
	bool is_repeat_vf = false;
	struct vd_prepare_s *vd_prepare = NULL;

	if (IS_ERR_OR_NULL(frame_info)) {
		vc_print(layer_index, PRINT_ERROR,
			"%s: frame_info is NULL.\n",
			__func__);
		return -EINVAL;
	}

	dev = video_display_getdev(layer_index);
	if (!dev) {
		vc_print(layer_index, PRINT_ERROR,
			"%s: get dev failed.\n",
			__func__);
		return -EBUSY;
	}

	if (dev->enable_composer != 1)
		video_display_setenable(layer_index, 1);

	vf = vd_get_vf_from_buf(dev, frame_info->dmabuf);
	if (!vf) {
		vc_print(layer_index, PRINT_ERROR,
			"%s: get vf failed.\n",
			__func__);
		return -ENOENT;
	}

	get_dma_buf(frame_info->dmabuf);
	dev->received_count++;
	vc_print(layer_index, PRINT_OTHER | PRINT_PATTERN,
		"%s: total receive_count is %lld.\n",
		__func__, dev->received_count);

	if (dev->last_file == (struct file *)frame_info->dmabuf)
		is_repeat_vf = true;

	if (is_repeat_vf) {
		vd_prepare = dev->vd_prepare_last;
	} else {
		vd_prepare = vd_prepare_data_q_get(dev);
		if (!vd_prepare) {
			vc_print(dev->index, PRINT_ERROR,
				 "%s: get prepare_data failed.\n",
				 __func__);
			return -ENOENT;
		}

		vd_prepare->src_frame = vf;
		vd_prepare->dst_frame = *vf;
		vd_prepare->release_fence = frame_info->release_fence;
		dev->vd_prepare_last = vd_prepare;
	}

	vf = &vd_prepare->dst_frame;

	vf->axis[0] = frame_info->dst_x;
	vf->axis[1] = frame_info->dst_y;
	vf->axis[2] = frame_info->dst_w + frame_info->dst_x - 1;
	vf->axis[3] = frame_info->dst_h + frame_info->dst_y - 1;
	vf->crop[0] = frame_info->crop_y;
	vf->crop[1] = frame_info->crop_x;
	vf->crop[2] = frame_info->buffer_h
		- frame_info->crop_h
		- frame_info->crop_y;
	vf->crop[3] = frame_info->buffer_w
		- frame_info->crop_w
		- frame_info->crop_x;
	vf->zorder = frame_info->zorder;
	vf->flag |= VFRAME_FLAG_VIDEO_COMPOSER
		| VFRAME_FLAG_VIDEO_COMPOSER_BYPASS;
	vf->disp_pts = 0;

	if (is_repeat_vf) {
		vf->repeat_count[dev->index]++;
		vc_print(layer_index, PRINT_ERROR,
			"%s: repeat frame, repeat_count is %d.\n",
			__func__, vf->repeat_count[dev->index]);
		return 0;
	}

	dev->last_file = (struct file *)frame_info->dmabuf;
	vf->file_vf = (struct file *)(frame_info->dmabuf);
	vf->repeat_count[dev->index] = 0;
	if (!kfifo_put(&dev->ready_q, (const struct vframe_s *)vf)) {
		vc_print(layer_index, PRINT_ERROR,
			"%s: ready_q is full.\n",
			__func__);
		return -EAGAIN;
	}
	ready_count = kfifo_len(&dev->ready_q);
	vc_print(layer_index, PRINT_OTHER,
		"%s: ready_q count is %d.\n",
		__func__, ready_count);

	return 0;
}
EXPORT_SYMBOL(video_display_setframe);
