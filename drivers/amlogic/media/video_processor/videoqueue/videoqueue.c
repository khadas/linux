// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#define DEBUG

#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/kfifo.h>
#include <linux/of_platform.h>
#include <linux/sync_file.h>
#include <linux/anon_inodes.h>
#include <linux/amlogic/major.h>
#include <linux/platform_device.h>
#include <linux/time.h>
#include <linux/compat.h>
#include <linux/delay.h>
#include <uapi/linux/sched/types.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#ifdef CONFIG_AMLOGIC_MEDIA_VIDEO
#include <linux/amlogic/media/video_sink/v4lvideo_ext.h>
#endif
#include <linux/amlogic/meson_uvm_core.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#ifdef CONFIG_AMLOGIC_MEDIA_VIDEO
#include <linux/amlogic/media/video_sink/video.h>
#endif
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
#include <linux/amlogic/media/amdolbyvision/dolby_vision.h>
#endif

#include "../../common/vfm/vfm.h"
#include "videoqueue.h"
#include "../videotunnel/videotunnel.h"

#define RECEIVER_NAME "videoqueue"
#define videoqueue_DEVICE_NAME "videoqueue"

#define HDMI_DELAY_FIRST_CHECK_COUNT 60
#define HDMI_DELAY_CHECK_INDEX 300

#define P_ERROR		0X0
#define P_OTHER		0X1
#define P_FENCE		0X2
#define P_SYNC		0X4
#define P_AVSYNC	0X8
#define P_THREAD	0X10

#define UNDEQUEU_COUNT 4
#define CHECK_DELAYE_COUNT 6

static unsigned int n_devs = 1;
static int print_close;
static int print_flag;
static int total_get_count;
static int total_put_count;
static int di_get_count;
static int di_put_count;
static int dump_index;
static int dump_index_last;
static u64 vframe_get_delay;
static int force_delay_ms;
static int game_mode;
static int force_game_mode;
static int fence_dq_count;
static int fence_put_count;
static int fence_null_count;
static int resync_open;
static int unknown_check = 1;

int vq_print(int debug_flag, const char *fmt, ...)
{
	if ((print_flag & debug_flag) ||
	    debug_flag == P_ERROR) {
		unsigned char buf[256];
		int len = 0;
		va_list args;

		va_start(args, fmt);
		len = sprintf(buf, "vq:");
		vsnprintf(buf + len, 256 - len, fmt, args);
		pr_info("%s", buf);
		va_end(args);
	}
	return 0;
}

static void dump_vf(struct vframe_s *vf)
{
	struct file *fp;
	char name_buf[32];
	int write_size;
	u8 *data;
	mm_segment_t fs;
	loff_t pos;

	if (!vf)
		return;

	snprintf(name_buf, sizeof(name_buf), "/data/tmp/DI.bin");
	fp = filp_open(name_buf, O_CREAT | O_RDWR, 0644);
	if (IS_ERR(fp))
		return;
	write_size = vf->canvas0_config[0].width * vf->canvas0_config[0].height
		* 2 * 10 / 8;
	data = codec_mm_vmap(vf->canvas0_config[0].phy_addr, write_size);
	if (!data)
		return;
	fs = get_fs();
	set_fs(KERNEL_DS);
	pos = 0;
	vfs_write(fp, data, write_size, &pos);
	vfs_fsync(fp, 0);
	vq_print(P_OTHER, "nn: write %u size to addr%p\n", write_size, data);
	codec_mm_unmap_phyaddr(data);
	filp_close(fp, NULL);
	set_fs(fs);
}

static void dump_hf(struct vframe_s *vf)
{
	struct hf_info_t *hf_info;
	struct file *fp;
	char name_buf[32];
	int write_size;
	u8 *data;
	mm_segment_t fs;
	loff_t pos;

	hf_info = vf->hf_info;

	if (!hf_info)
		return;

	snprintf(name_buf, sizeof(name_buf), "/data/tmp/hf.yuv");
	fp = filp_open(name_buf, O_CREAT | O_RDWR, 0644);
	if (IS_ERR(fp))
		return;
	write_size = hf_info->width * hf_info->height;
	data = codec_mm_vmap(hf_info->phy_addr, write_size);
	if (!data)
		return;
	fs = get_fs();
	set_fs(KERNEL_DS);
	pos = 0;
	vfs_write(fp, data, write_size, &pos);
	vfs_fsync(fp, 0);
	vq_print(P_OTHER, "nn: write %u size to addr%p\n", write_size, data);
	codec_mm_unmap_phyaddr(data);
	filp_close(fp, NULL);
	set_fs(fs);
}

static u64 time_cur;
static u64 vsync_pts_inc;
static u64 vpp_vsync_us;
static u64 pcr_time;
static u64 pcr_margin;
static u32 delay_vsync;
static bool sync_start;
static u32 vsync_no;
static bool videoq_start;
static wait_queue_head_t file_wq;
static int wakeup;
//used for 29.976 59.94 119.88fps
static bool is_special_fps;

void vsync_notify_videoqueue(void)
{
	time_cur = ktime_to_us(ktime_get());
}

void videoqueue_pcrscr_update(s32 inc, u32 base)
{
	bool is_vlock_locked = false;

	if (sync_start)
		is_vlock_locked = vlock_get_vlock_flag();

	vsync_pts_inc = 90000 * 16 * (u64)inc;
	vsync_pts_inc = div64_u64(vsync_pts_inc, base);
	vq_print(P_SYNC, "vlock: %d, special_fps: %d, vsync_pts_inc: %ld.\n",
		is_vlock_locked,
		is_special_fps,
		vsync_pts_inc);
	if (is_vlock_locked && is_special_fps) {
		if (vsync_pts_inc == 24000) {
			vq_print(P_SYNC, "use 59.94 fps.\n");
			vsync_pts_inc = 24024;
		} else if (vsync_pts_inc == 12000) {
			vq_print(P_SYNC, "use 119.88 fps.\n");
			vsync_pts_inc = 12012;
		} else {
			vq_print(P_SYNC, "invalid case.\n");
		}
	}

	// vsync_pts_inc/16 /9us
	vpp_vsync_us = div64_u64(vsync_pts_inc * 100, 144);
	pcr_margin = div64_u64(vsync_pts_inc, 4);

	if (sync_start) {
		pcr_time += vsync_pts_inc;
		vsync_no++;
		vq_print(P_SYNC, "vsync: NO=%d, pcr_time=%lld, time=%lld\n",
			vsync_no, pcr_time, time_cur);
	} else {
		pcr_time = 0;
		vsync_no = 0;
	}
	if (videoq_start) {
		wakeup = 1;
		wake_up_interruptible(&file_wq);
	}
}

static inline int DUR2PTS(int x)
{
	int var = 0, count = 0;

	if ((x % 10) == 0) {
		var = x * 15;
	} else {
		var = 12012;
		count = x / 800;
		var = var * count;
	}

	return var;
}

static DEFINE_SPINLOCK(devlist_lock);
static unsigned long videoqueue_devlist_lock(void)
{
	unsigned long flags;

	spin_lock_irqsave(&devlist_lock, flags);
	return flags;
}

static void videoqueue_devlist_unlock(unsigned long flags)
{
	spin_unlock_irqrestore(&devlist_lock, flags);
}

static LIST_HEAD(videoqueue_devlist);

static void file_pop_display_q(struct video_queue_dev *dev,
	struct file *disp_file)
{
	struct file *file_tmp = NULL;
	int k = kfifo_len(&dev->display_q);

	while (kfifo_len(&dev->display_q) > 0) {
		if (kfifo_get(&dev->display_q, &file_tmp)) {
			if (disp_file == file_tmp)
				break;
			if (!kfifo_put(&dev->display_q, file_tmp))
				vq_print(P_ERROR, "display_q is full!\n");
		}
		k--;
		if (k < 0) {
			vq_print(P_ERROR, "can find vf in display_q\n");
			break;
		}
	}
}

static u32 get_avsync_delay_time(void)
{
	u32 delay_value = get_tvin_delay_duration();

	if (delay_value == 0) {
		delay_value = get_tvin_delay_min_ms();
		vq_print(P_OTHER, "sync: no request delay, need set min %d\n",
			delay_value);
	}
	return delay_value;
}

static void videoq_hdmi_video_sync_2(struct video_queue_dev *dev,
	struct vframe_s *vf)
{
	u64 audio_need_delay;
	u64 vframe_delay;
	s64 vdin_vsync;
	int need_drop = 0;
	s64 diff = 0;
	u64 actual_delay = 0;
	u64 frc_delay = 0;
	u32 disp_delay_count = 2;

	audio_need_delay = get_avsync_delay_time();
	if (audio_need_delay == 0)
		return;

	vdin_vsync = vf->duration / 96;
	vframe_delay = (int)div_u64(((jiffies_64 -
		vf->ready_jiffies64) * 1000), HZ);
	vframe_delay -= vdin_vsync;
	actual_delay = vframe_delay * 1000 + disp_delay_count * vpp_vsync_us;
#ifdef CONFIG_AMLOGIC_MEDIA_FRC
	frc_delay = frc_get_video_latency();
	frc_delay = frc_delay * 1000;
#endif
	actual_delay += frc_delay;
	diff = actual_delay - audio_need_delay * 1000;

	vq_print(P_AVSYNC, "audio_need=%lld,vframe_delay=%lld, actual_delay=%lld,diff=%lld\n",
		audio_need_delay, vframe_delay,
		actual_delay, diff);

	if (diff > vdin_vsync * 1000 / 2) {
		diff = div_u64(diff, 1000);
		need_drop = div_u64(diff + vdin_vsync / 2, vdin_vsync);
		if (need_drop > 0)
			dev->sync_need_drop = true;
		vq_print(P_AVSYNC, "drop=%d,delay=%lld,actual=%lld,need=%lld\n",
			need_drop, vframe_delay,
			actual_delay, audio_need_delay);
		dev->sync_need_drop_count = need_drop;
	} else if (diff + vdin_vsync * 1000 / 2 < 0) {
		vq_print(P_SYNC, "delay: audio need =%lld, delay=%lld\n",
			audio_need_delay, vframe_delay);
		dev->sync_need_delay = true;
		vq_print(P_AVSYNC, "need delay=%lld,actual=%lld,need=%lld\n",
			vframe_delay, actual_delay, audio_need_delay);
	}
}

static enum vframe_disp_mode_e vf_disp_mode_get(struct video_queue_dev *dev,
	struct vframe_s *vf)
{
	struct provider_disp_mode_req_s req;

	req.vf = vf;
	req.disp_mode = VFRAME_DISP_MODE_NULL;
	req.req_mode = 0;

	if (dev->provider_name)
		vf_notify_provider_by_name(dev->provider_name,
			VFRAME_EVENT_RECEIVER_DISP_MODE, (void *)&req);
	return req.disp_mode;
}

void videoqueue_drop_vf(struct video_queue_dev *dev)
{
	struct vframe_s *vf;
	int ret = 0;

	vf = vf_get(dev->vf_receiver_name);
	if (!vf)
		return;
	ret = vf_put(vf, dev->vf_receiver_name);
	if (ret)
		vq_print(P_ERROR, "put: FAIL\n");
	dev->frame_num++;
}

static int do_file_thread(struct video_queue_dev *dev)
{
	struct vframe_s *vf;
	int ret = 0;
	struct file *ready_file;
	struct file_private_data *private_data = NULL;
	struct file *free_file, *fence_file;
	int i = 0;
	int dq_count = 0;
	struct sync_file *sync_file = NULL;
	u64 disp_time = 0;
	u64 pts = 0;
	u64 pts_tmp = 0;
	u32 display_vsync_no = 0;
	u32 vsync_diff = 0;
	struct vframe_states states;
	char *provider_name = NULL;
	u32 vframe_disp_mode = VFRAME_DISP_MODE_NULL;
	u32 vframe_walk_delay = 0;
	u32 tvin_delay_duration = 0;
	u64 avsync_diff = 0;
	u32 vdin_hold_count_cur = 0;
	bool vlock_locked = false;
	bool need_resync = false;
	u64 vdin_vsync = 0;
	vq_print(P_OTHER, "enter %s.\n", __func__);
	if (!dev->provider_name) {
		provider_name = vf_get_provider_name(dev->vf_receiver_name);
		while (provider_name) {
			if (!vf_get_provider_name(provider_name))
				break;
			provider_name =
				vf_get_provider_name(provider_name);
		}
		dev->provider_name = provider_name;
		vq_print(P_SYNC, "videoqueue: provider name: %s\n",
			dev->provider_name ? dev->provider_name : "NULL");
	}

	if (!kfifo_peek(&dev->file_q, &ready_file))
		return -1;
	vf = vf_peek(dev->vf_receiver_name);
	if (!vf && !dev->game_mode) {
		/*if do ai_sr, 6ms for ai_sr, 3ms software scheduling*/
		if (dev->need_aisr || !sync_start)
			usleep_range(6000, 7000);
		else
			usleep_range(10000, 11000);

		vf = vf_peek(dev->vf_receiver_name);
		if (vf)
			vq_print(P_SYNC, "peek 2 time ok, need_aisr=%d\n",
				dev->need_aisr);
	}

	if (!vf) {
		vq_print(P_SYNC, "peek err\n");
		return -1;
	}

	if (vf->width == 0 || vf->height == 0) {
		vq_print(P_ERROR, "vframe param invalid.\n");
		vf = vf_get(dev->vf_receiver_name);
		if (vf)
			vf_put(vf, dev->vf_receiver_name);
		vf = vf_peek(dev->vf_receiver_name);
		if (!vf)
			return -1;
	}

	if (get_video_mute_val()) {
		vq_print(P_ERROR, "video_mute_on need drop.\n");
		while (1) {
			vf = vf_get(dev->vf_receiver_name);
			if (vf)
				vf_put(vf, dev->vf_receiver_name);
			vf = vf_peek(dev->vf_receiver_name);
			if (!vf)
				return -1;
		}
	}

	if (dev->provider_name && !(strcmp(dev->provider_name, "dv_vdin"))) {
		if (vf->dv_crc_sts) {
			dev->vdin_err_crc_count = 0;
		} else {
			vq_print(P_ERROR, "invalid vframe.\n");
			vf_get(dev->vf_receiver_name);
			dev->frame_num++;
			vf_put(vf, dev->vf_receiver_name);
			dev->vdin_err_crc_count++;
			vq_print(P_ERROR,
				"vdin_err_crc_count is %d.\n",
				dev->vdin_err_crc_count);
			if (dev->vdin_err_crc_count >= 6) {
				ret = vt_send_cmd(dev->dev_session,
						  dev->tunnel_id,
						  VT_VIDEO_SET_STATUS,
						  1);
				if (ret < 0)
					vq_print(P_ERROR,
						"crc black screen err\n");
			}

			return -1;
		}
	}

	if ((vf->flag & VFRAME_FLAG_GAME_MODE || force_game_mode) &&
		!dev->game_mode) {
		dev->game_mode = true;
		game_mode = 1;
		ret = vt_send_cmd(dev->dev_session, dev->tunnel_id,
			VT_VIDEO_SET_GAME_MODE, 1);
		vq_print(P_ERROR, "set game mode true\n");
		if (ret < 0)
			vq_print(P_ERROR, "set game mode true err\n");
	} else if (!sync_start) {
		ret = vt_send_cmd(dev->dev_session, dev->tunnel_id,
			VT_VIDEO_SET_GAME_MODE, 1);
		if (ret < 0)
			vq_print(P_ERROR, "set game mode true err\n");
	}

	//used to point 29.97 23.976 59.94 119.88 fps
	vq_print(P_SYNC, "vf->duration is %d.\n", vf->duration);
	if (vf->duration == 1601 ||
	    vf->duration == 3203 ||
	    vf->duration == 4004 ||
	    vf->duration == 801) {
		is_special_fps = true;
	} else {
		is_special_fps = false;
	}

	dev->sync_need_delay = false;
	dev->sync_need_drop = false;
	dev->sync_need_drop_count = 0;

	if (dev->frame_num == HDMI_DELAY_FIRST_CHECK_COUNT ||
		dev->frame_num == 1)
		dev->need_check_delay_count = CHECK_DELAYE_COUNT;
	if (get_tvin_delay_start()) {
		vq_print(P_ERROR, "tvin delay start\n");
		dev->need_check_delay_count = CHECK_DELAYE_COUNT;
		set_tvin_delay_start(0);
	}

	//23.976 && 24 fps only lock frequence
	if (vf->duration == 4000 || vf->duration == 4004)
		vlock_locked = vlock_get_vlock_flag();
	else
		vlock_locked = vlock_get_phlock_flag() &&
				vlock_get_vlock_flag();

	if (vlock_locked && !dev->vlock_locked) {
		need_resync = true;
		vq_print(P_OTHER, "vlock locked\n");
	} else if (!vlock_locked && !is_panel_output()) {
		need_resync = true;
		vq_print(P_OTHER, "Not pannel output.\n");
	} else if (resync_open) {
		vq_print(P_OTHER, "Force resync.\n");
		need_resync = true;
	}

	if (dev->game_mode) {
		need_resync = false;
		dev->need_check_delay_count = 0;
	}

	if (dev->frame_num > HDMI_DELAY_CHECK_INDEX &&
		dev->need_check_delay_count == 0 &&
		need_resync) {
		tvin_delay_duration = get_avsync_delay_time();
		vframe_walk_delay = get_tvin_delay();
		avsync_diff = abs(tvin_delay_duration - vframe_walk_delay);

		vdin_vsync = vf->duration;
		vdin_vsync = vdin_vsync * 1000;
		vdin_vsync = div64_u64(vdin_vsync, 96);

		if (avsync_diff * 1000 > vdin_vsync * 3 / 4) {
#ifdef CONFIG_AMLOGIC_MEDIA_VDIN
			vdin_hold_count_cur = get_vdin_add_delay_num();
#endif
			vq_print(P_OTHER, "resync: vdin_hold_count:%d, cur:%d\n",
				dev->vdin_hold_count, vdin_hold_count_cur);

			if (vdin_hold_count_cur == dev->vdin_hold_count) {
				vq_print(P_ERROR,
					"resync:need=%d,actual=%d,diff=%lld\n",
					tvin_delay_duration, vframe_walk_delay,
					avsync_diff);
				dev->need_check_delay_count =
					CHECK_DELAYE_COUNT;
			}
		}
	}
	dev->vlock_locked = vlock_locked;
	/*step 1: audio required*/
	if (!sync_start) {
		vq_print(P_ERROR, "queue first frame\n");
	} else if (dev->need_check_delay_count > 0) {
	/*step 2: recheck video sync after hdmi-in start*/
		videoq_hdmi_video_sync_2(dev, vf);
		dev->need_check_delay_count--;
	}

	if (dev->sync_need_delay && !dev->game_mode)
		return 0;

	vf = vf_peek(dev->vf_receiver_name);
	if (!vf)
		return -1;

	if (get_video_hold_state()) {
		vq_print(P_ERROR, "hold video need drop.\n");
		videoqueue_drop_vf(dev);
		return 0;
	}

	vframe_disp_mode = vf_disp_mode_get(dev, vf);
	if (vframe_disp_mode != VFRAME_DISP_MODE_OK &&
		vframe_disp_mode != VFRAME_DISP_MODE_NULL)
		vq_print(P_OTHER, "disp_mode =%d\n", vframe_disp_mode);

	if (vframe_disp_mode == VFRAME_DISP_MODE_SKIP) {
		vq_print(P_ERROR, "SKIP\n");
		videoqueue_drop_vf(dev);
		return 0;
	}

	if (unknown_check && vframe_disp_mode == VFRAME_DISP_MODE_UNKNOWN &&
		dev->frame_skip_check_cnt < 4) {
		vq_print(P_SYNC, "unknown, need wait next vsync\n");
		dev->frame_skip_check_cnt++;
		return 0;
	}

	if (dev->frame_skip_check_cnt == 4)
		vq_print(P_ERROR, "continue 4 vsync unknown\n");

	dev->frame_skip_check_cnt = 0;

	if (!dev->game_mode && !sync_start) {
		if (dev->delay_vsync_count > 0) {
			dev->delay_vsync_count--;
			vq_print(P_SYNC, "delay one vsync\n");
			return 0;
		}
	}

	if (!sync_start) {
		pcr_time = pts;
		sync_start = true;
#ifdef CONFIG_AMLOGIC_MEDIA_VDIN
		dev->vdin_hold_count = get_vdin_add_delay_num();
#endif
	}

	pts = vf->pts;
	if (pts == 0 && dev->frame_num != 0)
		pts = dev->pts_last + DUR2PTS(vf->duration);

	disp_time = 0;
	display_vsync_no = 0;

	if (!dev->game_mode) {
		if (pts + pcr_margin <= pcr_time) {
			vq_print(P_SYNC,
				"delay pts= %lld, pcr=%lld,omx_index=%d\n",
				pts, pcr_time, dev->frame_num);
			pcr_time = pts;
		}
		pts_tmp = pts + pcr_margin;
		if (pts_tmp > pcr_time &&
			(pts_tmp < pcr_time + vsync_pts_inc)) {
			display_vsync_no = vsync_no;
			vq_print(P_SYNC,
				"vsync=%d, pts=%lld, omx_index=%d\n",
				display_vsync_no,
				pts, dev->frame_num);
		} else if (pts_tmp >= pcr_time + vsync_pts_inc) {
			vq_print(P_SYNC, "early continue\n");
			return 0;
		}
	}

	if (!kfifo_get(&dev->file_q, &ready_file)) {
		pr_info("task: get failed\n");
		return -1;
	}
	private_data = v4lvideo_get_file_private_data(ready_file, true);
	vf = vf_get(dev->vf_receiver_name);
	if (!vf)
		return -1;
	if (vf->hf_info)
		dev->need_aisr = true;
	else
		dev->need_aisr = false;

	//ATV need static frame
	if (vf->source_type == VFRAME_SOURCE_TYPE_TUNER)
		dev->need_keep_frame = true;
	else
		dev->need_keep_frame = false;

	vframe_get_delay = (int)div_u64(((jiffies_64 -
			vf->ready_jiffies64) * 1000), HZ);
	vframe_get_delay -= vf->duration / 96;
	dev->pts_last = pts;
	vf_get_states_by_name(dev->vf_receiver_name, &states);

	total_get_count++;
	if (vf->type & VIDTYPE_DI_PW)
		di_get_count++;

	vf->omx_index = dev->frame_num++;

	if (dump_index != dump_index_last) {
		dump_index_last = dump_index;
		dump_vf(vf);
		dump_hf(vf);
	}
	vq_print(P_OTHER, "get: omx_index=%d, buf_avail_num=%d,delay=%lld",
		vf->omx_index, states.buf_avail_num, vframe_get_delay);
	private_data->vf = *vf;
	private_data->vf_p = vf;
	if (dev->game_mode) {
		private_data->vf.timestamp = ktime_to_us(ktime_get());
		private_data->vf.disp_pts_us64 = dev->ready_time;
	}
	private_data->vf.src_fmt.dv_id = dev->dv_inst;

#ifdef COPY_META_DATA
	v4lvideo_import_sei_data(vf, &private_data->vf, dev->provider_name);
#endif

	vsync_diff = display_vsync_no - vf->omx_index;
	vq_print(P_SYNC, "dis_vsync=%d, omx_index=%d, diff=%d\n",
		display_vsync_no, vf->omx_index,
		vsync_diff);
	if (dev->last_vsync_diff != vsync_diff) {
		vq_print(P_SYNC, "omx_index=%d display_vsync=%d diff=%d\n",
			vf->omx_index, display_vsync_no, vsync_diff);
		dev->last_vsync_diff = vsync_diff;
	}

	ret = vt_queue_buffer(dev->dev_session,
			dev->tunnel_id, ready_file, -1, disp_time);
	if (ret < 0) {
		if (ret != -ENOTCONN)
			pr_err("vt queue buffer error\n");
		else
			vq_print(P_OTHER, "no consumer\n");
		total_put_count++;
		if (vf->type & VIDTYPE_DI_PW)
			di_put_count++;
		vq_print(P_OTHER, "put: omx_index=%d\n", vf->omx_index);
		ret = vf_put(vf, dev->vf_receiver_name);
		if (ret) {
			vq_print(P_ERROR, "put: FAIL\n");
			if (vf->type & VIDTYPE_DI_PW)
				dim_post_keep_cmd_release2(vf);
		}
		mutex_lock(&dev->mutex_file);
		if (!kfifo_put(&dev->file_q, ready_file))
			pr_err("queue error but file_q is full\n");
		mutex_unlock(&dev->mutex_file);
		return -1;
	}

	if (!kfifo_put(&dev->display_q, ready_file))
		pr_err("queue error but display_q is full\n");
	dev->queue_count++;

	vq_print(P_OTHER, "q buf: omx_index=%d, queue_count=%d, file=%px\n",
		vf->omx_index, dev->queue_count, ready_file);

	dq_count = 0;
	while (1) {
		if (dev->queue_count <= FILE_CNT - UNDEQUEU_COUNT)
			break;

		if (kthread_should_stop())
			break;

		ret = vt_dequeue_buffer(dev->dev_session, dev->tunnel_id,
					&free_file, &fence_file);
		if (ret != 0) {
			vq_print(P_OTHER, "dequeue fail\n");
			usleep_range(3000, 4000);
			dq_count++;
			continue;
		}

		if (!IS_ERR_OR_NULL(fence_file)) {
			fence_dq_count++;
			sync_file =
				(struct sync_file *)fence_file->private_data;
		} else {
			fence_null_count++;
		}
		vq_print(P_FENCE | P_OTHER, "dq: free_file=%px, fence_file=%px\n",
			free_file, fence_file);
		file_pop_display_q(dev, free_file);
		dev->dq_count++;
		private_data = v4lvideo_get_file_private_data(free_file, true);
		vf = private_data->vf_p;
		vq_print(P_OTHER, "dq: omx_index=%d,q_count=%d,dq_count=%d\n",
			vf->omx_index, dev->queue_count, dev->dq_count);
		for (i = 0; i < FILE_CNT; i++) {
			if (!dev->dq_info[i].used)
				break;
		}
		if (i == FILE_CNT) {
			vq_print(P_ERROR, "dq_info q is empty!\n");
			i = 0;
		}

		dev->dq_info[i].free_file = free_file;
		dev->dq_info[i].fence_file = fence_file;
		dev->dq_info[i].used = true;
		if (!kfifo_put(&dev->dq_info_q, &dev->dq_info[i]))
			vq_print(P_ERROR, "queue error, fence_q is full\n");
		wake_up_interruptible(&dev->fence_wq);
		break;
	}
	if (dq_count)
		vq_print(P_OTHER, "dequeue count = %d\n", dq_count);

	while (dev->sync_need_drop && dev->sync_need_drop_count) {
		videoqueue_drop_vf(dev);
		vq_print(P_AVSYNC, "drop omx_index=%d\n", dev->frame_num);
		dev->sync_need_drop_count--;
	}

	return 1;
}

static void do_fence_thread(struct video_queue_dev *dev)
{
	int ret;
	struct sync_file *sync_file = NULL;
	struct file *free_file, *fence_file;
	struct dma_fence *fence_obj = NULL;
	struct file_private_data *private_data = NULL;
	struct dequeu_info *dq_info = NULL;
	struct vframe_s *vf;

	if (!kfifo_get(&dev->dq_info_q, &dq_info)) {
		vq_print(P_OTHER, "get fence fail\n");
		return;
	}
	free_file = dq_info->free_file;
	fence_file = dq_info->fence_file;
	dq_info->used = false;

	if (!IS_ERR_OR_NULL(fence_file))
		sync_file = (struct sync_file *)fence_file->private_data;
	else
		vq_print(P_ERROR, "fence_file is NULL\n");

	/*coverity[var_deref_op] IS_ERR_OR_NULL can judge sync_file whether null ptr*/
	if (!IS_ERR_OR_NULL(sync_file))
		fence_obj = sync_file->fence;
	else
		vq_print(P_ERROR, "sync_file is NULL\n");

	if (fence_obj) {
		vq_print(P_FENCE, "sync_file=%px, seqno=%lld\n",
			sync_file, fence_obj->seqno);
		ret = dma_fence_wait_timeout(fence_obj,
					     false, 1000);
		if (ret == 0)
			vq_print(P_ERROR, "fence timeout\n");
	}

	if (!IS_ERR_OR_NULL(fence_file)) {
		fput(fence_file);
		fence_put_count++;
	}

	private_data = v4lvideo_get_file_private_data(free_file, true);
	if (private_data) {
#ifdef COPY_META_DATA
		v4lvideo_release_sei_data(&private_data->vf);
#endif
		vf = private_data->vf_p;
		if (vf) {
			vq_print(P_OTHER, "put: omx_index=%d\n",
				vf->omx_index);
			total_put_count++;
			if (vf->type & VIDTYPE_DI_PW)
				di_put_count++;
			ret = vf_put(vf, dev->vf_receiver_name);
			if (ret) {
				vq_print(P_ERROR, "put: FAIL\n");
				if (vf->type & VIDTYPE_DI_PW)
					dim_post_keep_cmd_release2(vf);
			}
		} else {
			pr_err("private_data vf null\n");
		}
		init_file_private_data(private_data);
	} else {
		pr_err("private_data null");
	}
	mutex_lock(&dev->mutex_file);
	if (!kfifo_put(&dev->file_q, free_file))
		pr_err("queue error but file_q is full\n");
	mutex_unlock(&dev->mutex_file);
}

static int vq_file_thread(void *data)
{
	struct video_queue_dev *dev = data;
	struct sched_param param = {.sched_priority = MAX_RT_PRIO - 1};
	struct vframe_s *vf = NULL;
	int ret = 0, tmp_pts = 0;

	sched_setscheduler(current, SCHED_FIFO, &param);
	dev->delay_vsync_count = delay_vsync;
	while (1) {
		if (kthread_should_stop())
			break;
		if (dev->thread_need_stop) {
			usleep_range(1000, 2000);
			continue;
		}
		wait_event_interruptible_timeout(file_wq,
			wakeup | dev->thread_need_stop,
			msecs_to_jiffies(50));
		wakeup = 0;

		vf = vf_peek(dev->vf_receiver_name);
		if (vf)
			tmp_pts = DUR2PTS(vf->duration);

		vq_print(P_OTHER, "tmp_pts:%d, vsync_pts_inc:%llu\n", tmp_pts, vsync_pts_inc);
		if (tmp_pts > 0 && tmp_pts < vsync_pts_inc) {
			vq_print(P_OTHER, "enter new flow.\n");
			do
				ret = do_file_thread(dev);
			while (ret == 1);
		} else {
			vq_print(P_THREAD, "enter old flow.\n");
			do_file_thread(dev);
		}
	}

	complete(&dev->file_thread_done);
	return 0;
}

static int vq_fence_thread(void *data)
{
	struct video_queue_dev *dev = data;
	struct sched_param param = {.sched_priority = MAX_RT_PRIO - 1};

	sched_setscheduler(current, SCHED_FIFO, &param);

	while (1) {
		if (kthread_should_stop())
			break;
		if (dev->thread_need_stop) {
			usleep_range(1000, 2000);
			continue;
		}
		wait_event_interruptible_timeout(dev->fence_wq,
				 kfifo_len(&dev->dq_info_q) > 0 ||
				 dev->thread_need_stop,
				 msecs_to_jiffies(200));
		do_fence_thread(dev);
	}

	complete(&dev->fence_thread_done);
	return 0;
}

static int init_vt_config(struct video_queue_dev *dev)
{
	int ret = -1;

	vq_print(P_OTHER, "init vt %s id=%d\n",
		dev->vf_receiver_name, dev->tunnel_id);

	dev->dev_session = vt_session_create(dev->vf_receiver_name);
	if (IS_ERR_OR_NULL(dev->dev_session)) {
		pr_err("%s create session fail\n", dev->vf_receiver_name);
		return ret;
	}

	ret = vt_producer_connect(dev->dev_session, dev->tunnel_id);
	if (ret < 0)
		pr_err("%s connect producer fail\n", dev->vf_receiver_name);
	return ret;
}

static int destroy_vt_config(struct video_queue_dev *dev)
{
	int ret = -1;

	ret = vt_producer_disconnect(dev->dev_session, dev->tunnel_id);
	if (ret < 0) {
		pr_err("%s disconnect producer fail\n", dev->vf_receiver_name);
		return ret;
	}

	vt_session_destroy(dev->dev_session);
	return ret;
}

static void videoq_notify_to_amvideo(bool reg)
{
	u32 para[2];

	para[0] = 0;
	para[1] = reg;

#ifdef CONFIG_AMLOGIC_MEDIA_VIDEO
	amvideo_notifier_call_chain(AMVIDEO_UPDATE_VT_REG,
					    (void *)&para[0]);
#endif
}

static int videoqueue_reg_provider(struct video_queue_dev *dev)
{
	int ret;
	int i;
	struct dma_buf *dmabuf;
	//struct file *file = NULL;

	sync_start = false;
	dev->pts_last = 0;

	dev->queue_count = 0;
	dev->dq_count = 0;
	dev->frame_num = 0;
	dev->tunnel_id = 0;
	dev->check_sync_count = 0;
	dev->provider_name = NULL;
	dev->need_check_delay_count = 0;
	dev->vdin_hold_count = 0;
	dev->vlock_locked = false;
	vframe_get_delay = 0;
	dev->game_mode = false;
	game_mode = 0;
	dev->vdin_err_crc_count = 0;
	is_special_fps = false;

	INIT_KFIFO(dev->file_q);
	INIT_KFIFO(dev->display_q);
	INIT_KFIFO(dev->dq_info_q);
	kfifo_reset(&dev->file_q);
	kfifo_reset(&dev->display_q);
	kfifo_reset(&dev->dq_info_q);
	init_completion(&dev->file_thread_done);
	init_completion(&dev->fence_thread_done);

	for (i = 0; i < FILE_CNT; i++) {
		dev->dq_info[i].index = i;
		dev->dq_info[i].used = false;
		dev->dq_info[i].free_file = NULL;
		dev->dq_info[i].fence_file = NULL;
	}

	ret = init_vt_config(dev);
	if (ret < 0)
		return ret;
	for (i = 0; i < FILE_CNT; i++) {
		dmabuf = uvm_alloc_dmabuf(SZ_4K, 0, 0);
		if (!dmabuf_is_uvm(dmabuf))
			vq_print(P_ERROR, "dmabuf is not uvm\n");
		vq_print(P_OTHER, "dmabuf is uvm:dmabuf=%px\n", dmabuf);

		dev->dev_file[i] = dmabuf->file;
		dev->dmabuf[i] = dmabuf;
		if (!kfifo_put(&dev->file_q, dev->dev_file[i]))
			pr_info("%s file_q is full\n", dev->vf_receiver_name);
	}

	init_waitqueue_head(&dev->fence_wq);

	dev->thread_need_stop = false;

	dev->file_thread = kthread_create(vq_file_thread,
					  dev, dev->vf_receiver_name);
	dev->fence_thread = kthread_create(vq_fence_thread,
					   dev, dev->vf_receiver_name);
	videoq_notify_to_amvideo(true);

	return ret;
}

static int videoqueue_unreg_provider(struct video_queue_dev *dev)
{
	int ret = -1;
	int i;
	struct file *disp_file = NULL;
	struct dequeu_info *dq_info = NULL;
	struct file *free_file, *fence_file;
	int time_left = 0;

	vq_print(P_ERROR, "unreg: in\n");

	dev->thread_need_stop = true;

	wake_up_interruptible(&dev->fence_wq);
	wakeup = 1;
	wake_up_interruptible(&file_wq);

	if (!dev->need_keep_frame) {
		vq_print(P_ERROR, "Other source, no need keep frame.\n");
		videoq_notify_to_amvideo(false);
		ret = vt_send_cmd(dev->dev_session, dev->tunnel_id,
			VT_VIDEO_SET_STATUS, 1);
		if (ret < 0)
			vq_print(P_ERROR, "set VT_VIDEO_SET_STATUS err\n");
	} else {
		vq_print(P_OTHER, "ATV source need keep frame.\n");
	}

	if (dev->file_thread)
		kthread_stop(dev->file_thread);

	if (dev->fence_thread)
		kthread_stop(dev->fence_thread);

	time_left = wait_for_completion_timeout(&dev->file_thread_done,
						msecs_to_jiffies(500));
	if (!time_left)
		vq_print(P_ERROR, "unreg:wait file_thread timeout\n");
	else if (time_left < 100)
		vq_print(P_ERROR,
			 "unreg:wait file_thread time %d\n", time_left);
	dev->file_thread = NULL;

	time_left = wait_for_completion_timeout(&dev->fence_thread_done,
						msecs_to_jiffies(500));
	if (!time_left)
		vq_print(P_ERROR, "unreg:wait fence_thread timeout\n");
	else if (time_left < 100)
		vq_print(P_ERROR,
			 "unreg:wait fence_thread time %d\n", time_left);
	dev->fence_thread = NULL;

	ret = vt_send_cmd(dev->dev_session, dev->tunnel_id,
		VT_VIDEO_SET_GAME_MODE, 0);
	vq_print(P_ERROR, "set no game mode\n");
	dev->game_mode = false;
	if (ret < 0)
		vq_print(P_ERROR, "set game mode false err\n");

	ret = destroy_vt_config(dev);
	if (ret < 0)
		return ret;

	while (kfifo_len(&dev->display_q) > 0) {
		if (kfifo_get(&dev->display_q, &disp_file)) {
			vq_print(P_OTHER, "unreg: disp_list keep vf\n");
			if (disp_file)
				v4lvideo_keep_vf(disp_file);
		}
	}

	while (kfifo_len(&dev->dq_info_q) > 0) {
		if (kfifo_get(&dev->dq_info_q, &dq_info)) {
			free_file = dq_info->free_file;
			fence_file = dq_info->fence_file;
			vq_print(P_ERROR, "unreg: dq_info_q keep vf\n");
			if (free_file)
				v4lvideo_keep_vf(free_file);
			if (!IS_ERR_OR_NULL(fence_file)) {
				fput(fence_file);
				fence_put_count++;
			}
		}
	}

	for (i = 0; i < FILE_CNT; i++) {
		if (dev->dmabuf[i])
			dma_buf_put(dev->dmabuf[i]);
		else
			vq_print(P_ERROR, "unreg: dmabuf is NULL\n");
		dev->dmabuf[i] = NULL;
	}

	vq_print(P_ERROR, "total get=%d put=%d, di_get=%d, di_put=%d\n",
			total_get_count, total_put_count,
			di_get_count, di_put_count);
	vq_print(P_ERROR, "unreg: out fence_dq=%d, fence_put=%d\n",
		fence_dq_count, fence_put_count);

	sync_start = false;
	game_mode = 0;
	dev->game_mode = false;

	return ret;
}

static void videoqueue_start(struct video_queue_dev *dev)
{
	wake_up_process(dev->file_thread);
	wake_up_process(dev->fence_thread);
}

static int video_receiver_event_fun(int type, void *data,
	void *private_data)
{
	struct video_queue_dev *dev = (struct video_queue_dev *)private_data;
	//struct file_private_data *file_private_data = NULL;
	int ret = 0;

	switch (type) {
	case VFRAME_EVENT_PROVIDER_UNREG:
	case VFRAME_EVENT_PROVIDER_LIGHT_UNREG:
		videoqueue_unreg_provider(dev);
		pr_info("%s unreg end!!\n", dev->vf_receiver_name);
		#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
		dv_inst_unmap(dev->dv_inst);
		#endif
		break;
	case VFRAME_EVENT_PROVIDER_REG:
		videoqueue_reg_provider(dev);
		pr_info("%s reg end!!\n", dev->vf_receiver_name);
		#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
		dv_inst_map(&dev->dv_inst);
		#endif
		break;
	case VFRAME_EVENT_PROVIDER_QUREY_STATE:
		ret = RECEIVER_ACTIVE;
		break;
	case VFRAME_EVENT_PROVIDER_START:
		videoqueue_start(dev);
		break;
	case VFRAME_EVENT_PROVIDER_VFRAME_READY:
		if (dev->game_mode) {
			dev->ready_time = ktime_to_us(ktime_get());
			vq_print(P_SYNC, "VFRAME_READY\n");
			wakeup = 1;
			wake_up_interruptible(&file_wq);
			vq_print(P_SYNC, "VFRAME_READY_1\n");
		}
		break;
	case VFRAME_EVENT_PROVIDER_RESET:
		videoqueue_unreg_provider(dev);
		videoqueue_reg_provider(dev);
		break;
	default:
		break;
	}
	return 0;
}

static const struct vframe_receiver_op_s video_vf_receiver = {
	.event_cb = video_receiver_event_fun
};

static ssize_t print_close_show(struct class *cla,
				struct class_attribute *attr,
				char *buf)
{
	return snprintf(buf, 80,
			"current print_close is %d\n",
			print_close);
}

static ssize_t print_close_store(struct class *cla,
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
	print_close = tmp;
	return count;
}

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

static ssize_t buf_count_show(struct class *cla,
			       struct class_attribute *attr,
			       char *buf)
{
	return snprintf(buf, 80,
			"total_get=%d total_pu=%d, di_get=%d, di_pu=%d\n",
			total_get_count, total_put_count,
			di_get_count, di_put_count);
}

static ssize_t dump_index_show(struct class *cla,
			       struct class_attribute *attr,
			       char *buf)
{
	return snprintf(buf, 80,
			"current dump_index is %d\n",
			dump_index);
}

static ssize_t dump_index_store(struct class *cla,
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
	dump_index = tmp;
	return count;
}

static ssize_t delay_vsync_show(struct class *cla,
			       struct class_attribute *attr,
			       char *buf)
{
	return snprintf(buf, 80,
			"current delay_vsync is %d\n",
			delay_vsync);
}

static ssize_t delay_vsync_store(struct class *cla,
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
	delay_vsync = tmp;
	return count;
}

static ssize_t vframe_get_delay_show(struct class *cla,
			       struct class_attribute *attr,
			       char *buf)
{
	return snprintf(buf, 80,
			"current vframe_get_delay is %lld\n",
			vframe_get_delay);
}

static ssize_t force_delay_ms_show(struct class *cla,
			       struct class_attribute *attr,
			       char *buf)
{
	return snprintf(buf, 80,
			"current force_delay_ms is %d\n",
			force_delay_ms);
}

static ssize_t force_delay_ms_store(struct class *cla,
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
	force_delay_ms = tmp;
	return count;
}

static ssize_t game_mode_show(struct class *cla,
			       struct class_attribute *attr,
			       char *buf)
{
	return snprintf(buf, 80,
			"current game_mode is %d\n",
			game_mode);
}

static ssize_t game_mode_store(struct class *cla,
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
	game_mode = tmp;
	return count;
}

static ssize_t force_game_mode_show(struct class *cla,
			       struct class_attribute *attr,
			       char *buf)
{
	return snprintf(buf, 80,
			"current force_game_mode is %d\n",
			force_game_mode);
}

static ssize_t force_game_mode_store(struct class *cla,
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
	force_game_mode = tmp;
	return count;
}

static ssize_t fence_dq_count_show(struct class *cla,
			       struct class_attribute *attr,
			       char *buf)
{
	return snprintf(buf, 80,
			"current fence_dq_count is %d\n",
			fence_dq_count);
}

static ssize_t fence_put_count_show(struct class *cla,
			       struct class_attribute *attr,
			       char *buf)
{
	return snprintf(buf, 80,
			"current fence_put_count is %d\n",
			fence_put_count);
}

static ssize_t fence_null_count_show(struct class *cla,
			       struct class_attribute *attr,
			       char *buf)
{
	return snprintf(buf, 80,
			"current fence_null_count is %d\n",
			fence_null_count);
}

static ssize_t resync_open_show(struct class *cla,
			       struct class_attribute *attr,
			       char *buf)
{
	return snprintf(buf, 80,
			"current resync_open is %d\n",
			resync_open);
}

static ssize_t resync_open_store(struct class *cla,
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
	resync_open = tmp;
	return count;
}

static ssize_t unknown_check_show(struct class *cla,
			       struct class_attribute *attr,
			       char *buf)
{
	return snprintf(buf, 80,
			"current resync_open is %d\n",
			unknown_check);
}

static ssize_t unknown_check_store(struct class *cla,
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
	unknown_check = tmp;
	return count;
}

static CLASS_ATTR_RW(print_close);
static CLASS_ATTR_RW(print_flag);
static CLASS_ATTR_RO(buf_count);
static CLASS_ATTR_RW(dump_index);
static CLASS_ATTR_RW(delay_vsync);
static CLASS_ATTR_RO(vframe_get_delay);
static CLASS_ATTR_RW(force_delay_ms);
static CLASS_ATTR_RW(game_mode);
static CLASS_ATTR_RW(force_game_mode);
static CLASS_ATTR_RO(fence_dq_count);
static CLASS_ATTR_RO(fence_put_count);
static CLASS_ATTR_RO(fence_null_count);
static CLASS_ATTR_RW(resync_open);
static CLASS_ATTR_RW(unknown_check);

static struct attribute *videoqueue_class_attrs[] = {
	&class_attr_print_close.attr,
	&class_attr_print_flag.attr,
	&class_attr_buf_count.attr,
	&class_attr_dump_index.attr,
	&class_attr_delay_vsync.attr,
	&class_attr_vframe_get_delay.attr,
	&class_attr_force_delay_ms.attr,
	&class_attr_game_mode.attr,
	&class_attr_force_game_mode.attr,
	&class_attr_fence_dq_count.attr,
	&class_attr_fence_put_count.attr,
	&class_attr_fence_null_count.attr,
	&class_attr_resync_open.attr,
	&class_attr_unknown_check.attr,
	NULL
};

ATTRIBUTE_GROUPS(videoqueue_class);

static struct class videoqueue_class = {
	.name = "videoqueue",
	.class_groups = videoqueue_class_groups,
};

static int videoqueue_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int videoqueue_release(struct inode *inode, struct file *file)
{
	return 0;
}

int videoqueue_assign_map(char **receiver_name, int *inst)
{
	unsigned long flags;
	struct video_queue_dev *dev = NULL;
	struct list_head *p;

	flags = videoqueue_devlist_lock();

	list_for_each(p, &videoqueue_devlist) {
		dev = list_entry(p, struct video_queue_dev,
				 videoqueue_devlist);

		if (dev->inst == *inst) {
			*receiver_name = dev->vf_receiver_name;
			videoqueue_devlist_unlock(flags);
			return 0;
		}
	}

	videoqueue_devlist_unlock(flags);
	return -ENODEV;
}
EXPORT_SYMBOL(videoqueue_assign_map);

int videoqueue_alloc_map(int *inst)
{
	unsigned long flags;
	struct video_queue_dev *dev = NULL;
	struct list_head *p;

	flags = videoqueue_devlist_lock();

	list_for_each(p, &videoqueue_devlist) {
		dev = list_entry(p, struct video_queue_dev,
				 videoqueue_devlist);

		if (dev->inst >= 0 && !dev->mapped) {
			dev->mapped = true;
			*inst = dev->inst;
			videoqueue_devlist_unlock(flags);
			return 0;
		}
	}

	videoqueue_devlist_unlock(flags);
	return -ENODEV;
}

void videoqueue_release_map(int inst)
{
	unsigned long flags;
	struct video_queue_dev *dev = NULL;
	struct list_head *p;

	flags = videoqueue_devlist_lock();

	list_for_each(p, &videoqueue_devlist) {
		dev = list_entry(p, struct video_queue_dev,
				 videoqueue_devlist);
		if (dev->inst == inst && dev->mapped) {
			dev->mapped = false;
			pr_info("%s %d OK\n", __func__, dev->inst);
			break;
		}
	}
	videoqueue_devlist_unlock(flags);
}

static long videoqueue_ioctl(struct file *file,
			     unsigned int cmd, ulong arg)
{
	long ret = 0;
	u32 videoque_id = 0;
	void __user *argp = (void __user *)arg;

	switch (cmd) {
	case videoqueue_IOCTL_ALLOC_ID:{
			ret = videoqueue_alloc_map(&videoque_id);
			if (ret != 0)
				break;
			put_user(videoque_id, (u32 __user *)argp);
		}
		break;
	case videoqueue_IOCTL_FREE_ID:{
			get_user(videoque_id, (u32 __user *)argp);
			videoqueue_release_map(videoque_id);
		}
		break;
	default:
		return -EINVAL;
	}
	return ret;
}

#ifdef CONFIG_COMPAT
static long videoqueue_compat_ioctl(struct file *file,
				  unsigned int cmd,
				  ulong arg)
{
	long ret = 0;

	ret = videoqueue_ioctl(file, cmd, (ulong)compat_ptr(arg));
	return ret;
}
#endif
static const struct file_operations videoqueue_fops = {
	.owner = THIS_MODULE,
	.open = videoqueue_open,
	.release = videoqueue_release,
	.unlocked_ioctl = videoqueue_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = videoqueue_compat_ioctl,
#endif
	.poll = NULL,
};

static int videoqueue_create_instance(int inst)
{
	struct video_queue_dev *dev;
	unsigned long flags;

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;
	dev->inst = inst;
	snprintf(dev->vf_receiver_name, RECEIVER_NAME_SIZE,
		 RECEIVER_NAME ".%x", inst & 0xff);

	vf_receiver_init(&dev->video_vf_receiver,
			 dev->vf_receiver_name, &video_vf_receiver, dev);
	vf_reg_receiver(&dev->video_vf_receiver);

	/* add to device list */
	flags = videoqueue_devlist_lock();
	list_add_tail(&dev->videoqueue_devlist, &videoqueue_devlist);
	videoqueue_devlist_unlock(flags);

	mutex_init(&dev->mutex_input);
	mutex_init(&dev->mutex_output);

	return 0;
}

static int video_queue_release(void)
{
	struct video_queue_dev *dev;
	struct list_head *list;
	unsigned long flags;

	flags = videoqueue_devlist_lock();

	while (!list_empty(&videoqueue_devlist)) {
		list = videoqueue_devlist.next;
		list_del(list);
		videoqueue_devlist_unlock(flags);
		dev = list_entry(list, struct video_queue_dev,
				 videoqueue_devlist);
		kfree(dev);
		flags = videoqueue_devlist_lock();
	}
	videoqueue_devlist_unlock(flags);
	return 0;
}

static int video_queue_probe(struct platform_device *pdev)
{
	int ret = 0, i = 0;
	struct device *devp;

	ret = class_register(&videoqueue_class);
	if (ret < 0)
		return ret;
	ret = register_chrdev(VIDEOQUEUE_MAJOR,
		"videoqueue", &videoqueue_fops);
	if (ret < 0) {
		pr_err("Can't allocate major for videoqueue device\n");
		goto error1;
	}
	devp = device_create(&videoqueue_class,
			     NULL,
			     MKDEV(VIDEOQUEUE_MAJOR, 0),
			     NULL,
			     videoqueue_DEVICE_NAME);
	if (IS_ERR(devp)) {
		pr_err("failed to create videoqueue device node\n");
		ret = PTR_ERR(devp);
		return ret;
	}

	if (n_devs <= 0)
		n_devs = 1;
	for (i = 0; i < n_devs; i++) {
		ret = videoqueue_create_instance(i);
		if (ret) {
			/* If some instantiations succeeded, keep driver */
			if (i)
				ret = 0;
			break;
		}
	}

	if (ret < 0) {
		pr_err("videoqueue: error %d while loading driver\n", ret);
		goto error1;
	}

	return 0;
error1:
	unregister_chrdev(VIDEOQUEUE_MAJOR, videoqueue_DEVICE_NAME);
	class_unregister(&videoqueue_class);
	return ret;
}

static int video_queue_remove(struct platform_device *pdev)
{
	video_queue_release();
	device_destroy(&videoqueue_class, MKDEV(VIDEOQUEUE_MAJOR, 0));
	unregister_chrdev(VIDEOQUEUE_MAJOR, videoqueue_DEVICE_NAME);
	class_unregister(&videoqueue_class);
	return 0;
}

#ifdef CONFIG_PM
static int video_queue_suspend(struct platform_device *pdev,
				 pm_message_t state)
{
	return 0;
}

static int video_queue_resume(struct platform_device *pdev)
{
	return 0;
}
#endif

static const struct of_device_id video_queue_dt_match[] = {
	{
		.compatible = "amlogic, video_queue",
	},
	{},
};

static struct platform_driver videoqueue_driver = {
	.probe = video_queue_probe,
	.remove = video_queue_remove,
#ifdef CONFIG_PM
	.suspend = video_queue_suspend,
	.resume = video_queue_resume,
#endif
	.driver = {
		.owner = THIS_MODULE,
		.name = "videoqueue",
		.of_match_table = video_queue_dt_match,
	}
};

int __init videoqueue_init(void)
{
	pr_info("%s___1\n", __func__);
	init_waitqueue_head(&file_wq);
	videoq_start = true;
	if (platform_driver_register(&videoqueue_driver)) {
		pr_err("failed to register videoqueue module\n");
		return -ENODEV;
	}
	return 0;
}

void __exit videoqueue_exit(void)
{
	videoq_start = false;
	platform_driver_unregister(&videoqueue_driver);
}

