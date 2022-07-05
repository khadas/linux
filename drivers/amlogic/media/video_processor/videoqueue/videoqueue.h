/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _videoqueue_H
#define _videoqueue_H
#include <linux/fs.h>
#include <linux/kfifo.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/completion.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>
#include "../videotunnel/videotunnel_priv.h"

#define RECEIVER_NAME_SIZE 32
#define FILE_CNT 8

struct file_fence {
	struct file *free_file;
	int fence_fd;
};

struct dequeu_info {
	int index;
	struct file *free_file;
	struct file *fence_file;
	bool used;
};

struct video_queue_dev {
	struct list_head videoqueue_devlist;
	unsigned int width, height;
	struct vframe_receiver_s video_vf_receiver;
	char vf_receiver_name[RECEIVER_NAME_SIZE];
	struct vt_session *dev_session;
	int tunnel_id;
	struct file *dev_file[FILE_CNT];
	struct dma_buf *dmabuf[FILE_CNT];
	DECLARE_KFIFO(file_q, struct file *, FILE_CNT);
	DECLARE_KFIFO(display_q, struct file *, FILE_CNT);
	DECLARE_KFIFO(dq_info_q, struct dequeu_info *, FILE_CNT);
	int inst;
	struct task_struct *file_thread;
	struct task_struct *fence_thread;
	wait_queue_head_t fence_wq;
	wait_queue_head_t file_wq;
	bool mapped;
	/* mutex_input */
	struct mutex mutex_input;
	/* mutex_output */
	struct mutex mutex_output;
	int queue_count;
	int dq_count;
	struct dequeu_info dq_info[FILE_CNT];
	bool thread_need_stop;
	bool game_mode;
	int frame_num;
	struct completion file_thread_done;
	struct completion fence_thread_done;
	u64 pts_last;
	int last_vsync_diff;
	char *provider_name;
	int check_sync_count;
	bool sync_need_delay;
	bool sync_need_drop;
	u32 sync_need_drop_count;
	u64 ready_time;
	u32 delay_vsync_count;
	u32 need_check_delay_count;
	bool low_latency_mode;
	bool need_aisr;
	u32 frc_delay_first_frame;
	bool vlock_locked;
	int vdin_err_crc_count;
	bool need_keep_frame;
	int dv_inst;
};

#ifdef CONFIG_AMLOGIC_MEDIA_VDIN
int get_vdin_add_delay_num(void);
#endif

bool vlock_get_phlock_flag(void);
bool vlock_get_vlock_flag(void);
int get_video_mute_val(void);


#define videoqueue_IOC_MAGIC  'I'
#define videoqueue_IOCTL_ALLOC_ID   _IOW(videoqueue_IOC_MAGIC, 0x00, int)
#define videoqueue_IOCTL_FREE_ID    _IOW(videoqueue_IOC_MAGIC, 0x01, int)

#endif

