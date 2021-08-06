/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef VDETECT_H_
#define VDETECT_H_

#include <linux/mutex.h>
#include <linux/completion.h>
#include <media/v4l2-device.h>
#include <linux/amlogic/media/ge2d/ge2d.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>
#include <linux/amlogic/media/v4l_util/videobuf-res.h>

#define RECEIVER_NAME_SIZE 32
#define PROVIDER_NAME_SIZE 32
#define MAX_NN_INFO_BUF_COUNT 16

enum vdetect_vf_status {
	VDETECT_INIT = 0,
	VDETECT_PREPARE,
	VDETECT_DROP_VF,
};

enum vframe_source_type {
	DECODER_8BIT_NORMAL = 0,
	DECODER_8BIT_BOTTOM,
	DECODER_8BIT_TOP,
	DECODER_10BIT_NORMAL,
	DECODER_10BIT_BOTTOM,
	DECODER_10BIT_TOP,
	VDIN_8BIT_NORMAL,
	VDIN_10BIT_NORMAL,
};

struct vdetect_fmt {
	char *name;
	u32 fourcc; /* v4l2 format id */
	u8 depth;
	bool is_yuv;
};

struct vdetect_output {
	int canvas_id;
	void *vbuf;
	int width;
	int height;
	u32 v4l2_format;
};

struct vdetect_buffer {
	/* common v4l buffer stuff -- must be first */
	struct videobuf_buffer vb;

	struct vdetect_fmt *fmt;
};

struct vdetect_dmaqueue {
	struct list_head active;

	/* thread for generating video stream*/
	struct task_struct *kthread;
	wait_queue_head_t wq;
	unsigned char task_running;
#ifdef USE_SEMA_QBUF
	struct completion qbuf_comp;
#endif
};

struct vdetect_frame_nn_info {
	u32 nn_frame_num;
	struct nn_value_t nn_value[AI_PQ_TOP];
};

struct vdetect_dev {
	struct list_head vdetect_devlist;
	struct v4l2_device v4l2_dev;
	struct videobuf_res_privdata res;
	struct video_device *vdev;
	int inst;
	bool mapped;
	int fd_num;
	int src_canvas[3];
	int dst_canvas;
	struct timeval begin_time;
	atomic_t is_playing;
	bool set_format_flag;
	u32 vf_num;
	atomic_t is_dev_reged;
	struct vframe_s *last_vf;
	struct vframe_s *ge2d_vf;
	atomic_t vdect_status;
	unsigned int f_flags;
	struct semaphore thread_sem;
	resource_size_t buffer_start;
	unsigned int buffer_size;
	struct vdetect_fmt *fmt;
	unsigned int width, height;
	unsigned int dst_width, dst_height;
	spinlock_t slock; /*capture for vnn*/
	struct mutex vdetect_mutex /*for v4l2 */;
	struct mutex vf_mutex /*for vframe */;
	enum v4l2_buf_type type;
	struct videobuf_queue vbuf_queue;
	struct vdetect_dmaqueue dma_q;
	struct ge2d_context_s *context;
	struct vframe_receiver_s recv;
	struct vframe_provider_s prov;
	char recv_name[RECEIVER_NAME_SIZE];
	char prov_name[PROVIDER_NAME_SIZE];
	struct nn_value_t nn_value[AI_PQ_TOP];
};

int vdetect_assign_map(char **receiver_name, int *inst);

#define VDETECT_IOC_MAGIC  'I'
#define VDETECT_IOCTL_ALLOC_ID   _IOW(VDETECT_IOC_MAGIC, 0x00, int)
#define VDETECT_IOCTL_FREE_ID    _IOW(VDETECT_IOC_MAGIC, 0x01, int)

#endif /* VDETECT_H_ */

