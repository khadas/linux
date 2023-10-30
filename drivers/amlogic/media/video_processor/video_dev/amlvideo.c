// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/pci.h>
#include <linux/random.h>
#include <linux/version.h>
#include <linux/mutex.h>
#include <linux/videodev2.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/highmem.h>
#include <linux/freezer.h>
#include <linux/platform_device.h>
#include <linux/amlogic/media/v4l_util/videobuf-res.h>
#include <media/videobuf2-core.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <linux/types.h>
#include <linux/amlogic/media/canvas/canvas.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>
#include <linux/amlogic/media/ge2d/ge2d.h>
#include <linux/amlogic/media/frame_sync/timestamp.h>
#include <linux/kernel.h>
#include <linux/amlogic/media/frame_sync/tsync.h>
#include "../utils/vfp.h"
#include "amlvideo.h"

#define AVMLVIDEO_MODULE_NAME "amlvideo"
#define KERNEL_ATRACE_TAG KERNEL_ATRACE_TAG_AMLVIDEO
#include <trace/events/meson_atrace.h>

#define AMLVIDEO_INFO(fmt, args...) pr_info("amlvid:info: " fmt " ", ## args)
#define AMLVIDEO_DBG(fmt, args...) pr_debug("amlvid:dbg: " fmt " ", ## args)
#define AMLVIDEO_WARN(fmt, args...) pr_warn("amlvid:warn: " fmt " ", ## args)
#define AMLVIDEO_ERR(fmt, args...) pr_err("amlvid:err: " fmt " ", ## args)

#define AMLVIDEO_MAJOR_VERSION 0
#define AMLVIDEO_MINOR_VERSION 7
#define AMLVIDEO_RELEASE 1
#define AMLVIDEO_VERSION \
	KERNEL_VERSION(AMLVIDEO_MAJOR_VERSION, \
AMLVIDEO_MINOR_VERSION, AMLVIDEO_RELEASE)
#define MAGIC_RE_MEM 0x123039dc

#define RECEIVER_NAME "amlvideo"
#define PROVIDER_NAME "amlvideo"
#define RECEIVER_NAME_PIP "aml_video"
#define PROVIDER_NAME_PIP "aml_video"

#define AMLVIDEO_POOL_SIZE 16
/*extern bool omx_secret_mode;*/

static inline u32 dur2pts(u32 duration)
{
	return (duration - (duration >> 4));
}

#define DUR2PTS_RM(x) ((x) & 0xf)

//MODULE_AUTHOR("amlogic-sh");
//MODULE_LICENSE("GPL");
/* static u32 vpts_remainder; */
static unsigned int video_nr_base = 10;
static unsigned int video_nr_base_second = 23;
/* module_param(video_nr_base, uint, 0644); */
/* MODULE_PARM_DESC(video_nr_base, "videoX start number, 10 is defaut"); */

#ifdef CONFIG_AMLOGIC_MEDIA_MULTI_DEC
static unsigned int n_devs = 2;
#else
static unsigned int n_devs = 1;
#endif

static unsigned int debug;
/* module_param(debug, uint, 0644); */
/* MODULE_PARM_DESC(debug, "activates debug info"); */

static unsigned int vid_limit = 16;
module_param(vid_limit, uint, 0644);
MODULE_PARM_DESC(vid_limit, "capture memory limit in megabytes");

static int video_receiver_event_fun(int type, void *data, void*);

#define dprintk(dev, level, fmt, arg...) \
	v4l2_dbg((level), debug, &(dev)->v4l2_dev, fmt, ## arg)

/* ------------------------------------------------------------------
 * Basic structures
 * ------------------------------------------------------------------
 */

static struct vivi_fmt formats[] = {
	{
		.name = "RGB888 (24)",
		.fourcc = V4L2_PIX_FMT_RGB24, /* 24  RGB-8-8-8 */
		.depth = 24,
	},
	{
		.name = "RGBA8888 (32)",
		.fourcc = V4L2_PIX_FMT_RGB32, /* 24  RGBA-8-8-8-8 */
		.depth = 32,
	},
	{
		.name = "12  Y/CbCr 4:2:0",
		.fourcc = V4L2_PIX_FMT_NV12,
		.depth = 12,
	},
	{
		.name = "21  Y/CbCr 4:2:0",
		.fourcc = V4L2_PIX_FMT_NV21,
		.depth = 12,
	},
	{
		.name = "RGB565 (BE)",
		.fourcc = V4L2_PIX_FMT_RGB565X, /* rrrrrggg gggbbbbb */
		.depth = 16,
	},
};

/* -----------------------------------------------------------------
 *           provider operations
 * -----------------------------------------------------------------
 */
static struct vframe_s *amlvideo_vf_peek(void *op_arg)
{
	struct vivi_dev *dev = (struct vivi_dev *)op_arg;

	return vfq_peek(&dev->q_ready);
}

static struct vframe_s *amlvideo_vf_get(void *op_arg)
{
	struct vframe_s *vf;
	struct vivi_dev *dev = (struct vivi_dev *)op_arg;

	vf = vfq_pop(&dev->q_ready);
	ATRACE_COUNTER(dev->v4l2_dev.name, vfq_level(&dev->q_ready));
	return vf;
}

static void amlvideo_vf_put(struct vframe_s *vf, void *op_arg)
{
	struct vivi_dev *dev = (struct vivi_dev *)op_arg;

	vf_put(vf, dev->vf_receiver_name);
	vf_notify_provider(dev->vf_receiver_name, VFRAME_EVENT_RECEIVER_PUT,
			   NULL);
}

static int amlvideo_event_cb(int type, void *data, void *private_data)
{
	/* printk("ionvideo_event_cb_type=%d\n",type); */
	if (type & VFRAME_EVENT_RECEIVER_PUT) {
		/* printk("video put, avail=%d\n", vfq_level(&q_ready) ); */
	} else if (type & VFRAME_EVENT_RECEIVER_GET) {
		/* printk("video get, avail=%d\n", vfq_level(&q_ready) ); */
	} else if (type & VFRAME_EVENT_RECEIVER_FRAME_WAIT) {
		/* up(&thread_sem); */
		/* printk("receiver is waiting\n"); */
	} else if (type & VFRAME_EVENT_RECEIVER_FRAME_WAIT) {
		/* printk("frame wait\n"); */
	}
	return 0;
}

static int amlvideo_vf_states(struct vframe_states *states, void *op_arg)
{
	/* unsigned long flags; */
	/* spin_lock_irqsave(&lock, flags); */
	struct vivi_dev *dev = (struct vivi_dev *)op_arg;

	states->vf_pool_size = AMLVIDEO_POOL_SIZE;
	states->buf_recycle_num = 0;
	states->buf_free_num = AMLVIDEO_POOL_SIZE - vfq_level(&dev->q_ready);
	states->buf_avail_num = vfq_level(&dev->q_ready);
	/* spin_unlock_irqrestore(&lock, flags); */
	return 0;
}

static const struct vframe_operations_s amlvideo_vf_provider = {
	.peek = amlvideo_vf_peek,
	.get = amlvideo_vf_get,
	.put = amlvideo_vf_put,
	.event_cb = amlvideo_event_cb,
	.vf_states = amlvideo_vf_states,
};

static struct vivi_fmt *get_format(struct v4l2_format *f)
{
	struct vivi_fmt *fmt;
	unsigned int k;

	for (k = 0; k < ARRAY_SIZE(formats); k++) {
		fmt = &formats[k];
		if (fmt->fourcc == f->fmt.pix.pixelformat)
			break;
	}

	if (k == ARRAY_SIZE(formats))
		return NULL;

	return &formats[k];
}

static int video_receiver_event_fun(int type, void *data, void *private_data)
{
	struct vframe_states states;
	struct vivi_dev *dev = (struct vivi_dev *)private_data;

	if (type == VFRAME_EVENT_PROVIDER_UNREG) {
		AMLVIDEO_DBG("AML:VFRAME_EVENT_PROVIDER_UNREG\n");
		mutex_lock(&dev->vf_mutex);
		if (vf_get_receiver(dev->vf_provider_name)) {
			AMLVIDEO_DBG("unreg:amlvideo\n");
			vf_unreg_provider(&dev->video_vf_prov);
			if (dev->inst == 0)
				omx_secret_mode = false;
		}
		dev->first_frame = 0;
		vfq_init(&dev->q_ready, AMLVIDEO_POOL_SIZE + 1,
			 &dev->amlvideo_pool_ready[0]);
		mutex_unlock(&dev->vf_mutex);
	}
	if (type == VFRAME_EVENT_PROVIDER_REG) {
		AMLVIDEO_DBG("AML:VFRAME_EVENT_PROVIDER_REG\n");
		mutex_lock(&dev->vf_mutex);

		dev->vf = NULL;
		dev->first_frame = 0;
		dev->frame_num = 0;
		mutex_unlock(&dev->vf_mutex);
	} else if (type == VFRAME_EVENT_PROVIDER_QUREY_STATE) {
		amlvideo_vf_states(&states, dev);
		if (states.buf_avail_num > 0)
			return RECEIVER_ACTIVE;

		if (vf_notify_receiver
				(dev->vf_provider_name,
				 VFRAME_EVENT_PROVIDER_QUREY_STATE,
				 NULL) == RECEIVER_ACTIVE)
			return RECEIVER_ACTIVE;
		return RECEIVER_INACTIVE;
	/*break;*/
	} else if (type == VFRAME_EVENT_PROVIDER_START) {
		AMLVIDEO_DBG("AML:VFRAME_EVENT_PROVIDER_START\n");
		mutex_lock(&dev->vf_mutex);
		if (vf_get_receiver(dev->vf_provider_name)) {
			struct vframe_receiver_s *aaa = vf_get_receiver
					(dev->vf_provider_name);
			if (aaa)
				AMLVIDEO_DBG("aaa->name=%s", aaa->name);
			if (dev->inst == 0)
				omx_secret_mode = true;
			vfq_init(&dev->q_ready, AMLVIDEO_POOL_SIZE + 1,
				 &dev->amlvideo_pool_ready[0]);
			vf_provider_init(&dev->video_vf_prov,
					 dev->vf_provider_name,
					 &amlvideo_vf_provider, dev);
			vf_reg_provider(&dev->video_vf_prov);
			vf_notify_receiver(dev->vf_provider_name,
					   VFRAME_EVENT_PROVIDER_START,
					   NULL);
		}
		mutex_unlock(&dev->vf_mutex);
	} else if (type == VFRAME_EVENT_PROVIDER_FR_HINT) {
		vf_notify_receiver(dev->vf_provider_name,
				   VFRAME_EVENT_PROVIDER_FR_HINT, data);
	} else if (type == VFRAME_EVENT_PROVIDER_FR_END_HINT) {
		vf_notify_receiver(dev->vf_provider_name,
				   VFRAME_EVENT_PROVIDER_FR_END_HINT, data);
	} else if (type == VFRAME_EVENT_PROVIDER_RESET) {
		dev->first_frame = 0;
		vfq_init(&dev->q_ready, AMLVIDEO_POOL_SIZE + 1,
			 &dev->amlvideo_pool_ready[0]);

		vf_notify_receiver(dev->vf_provider_name,
				   VFRAME_EVENT_PROVIDER_RESET, data);
	} else if (type == VFRAME_EVENT_PROVIDER_VFRAME_READY) {
		if (vf_peek(dev->vf_receiver_name))
			wake_up_interruptible(&dev->wq);
	}
	return 0;
}

static const struct vframe_receiver_op_s video_vf_receiver = {
	.event_cb = video_receiver_event_fun
};

/* ------------------------------------------------------------------
 * Videobuf operations
 * ------------------------------------------------------------------
 */

static int vidioc_g_parm(struct file *file, void *priv,
			 struct v4l2_streamparm *parms)
{
	struct vivi_dev *dev = video_drvdata(file);

	if (parms->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;

	memcpy(parms->parm.raw_data, (u8 *)&dev->am_parm,
	       sizeof(struct v4l2_amlogic_parm));
	return 0;
}

static int buffer_setup(struct videobuf_queue *vq, unsigned int *count,
			unsigned int *size)
{
	struct videobuf_res_privdata *res = (struct videobuf_res_privdata *)vq
		->priv_data;
	struct vivi_fh *fh = (struct vivi_fh *)res->priv;
	struct vivi_dev *dev = fh->dev;
	*size = (fh->width * fh->height * fh->fmt->depth) >> 3;
	if (*count == 0)
		*count = 32;

	while (*size * *count > vid_limit * 1024 * 1024)
		(*count)--;

	dprintk(dev, 1, "%s, count=%d, size=%d\n", __func__, *count, *size);

	return 0;
}

static void free_buffer(struct videobuf_queue *vq, struct vivi_buffer *buf)
{
	struct videobuf_res_privdata *res = (struct videobuf_res_privdata *)vq
		->priv_data;
	struct vivi_fh *fh = (struct vivi_fh *)res->priv;
	struct vivi_dev *dev = fh->dev;

	dprintk(dev, 1, "%s, state: %i\n", __func__, buf->vb.state);
	videobuf_waiton(vq, &buf->vb, 0, 0);
	if (in_interrupt())
		WARN_ON(1);
	videobuf_res_free(vq, &buf->vb);
	dprintk(dev, 1, "%s: freed\n", __func__);
	buf->vb.state = VIDEOBUF_NEEDS_INIT;
}

#define NORM_MAXW 2000
#define NORM_MAXH 1600
static int buffer_prepare(struct videobuf_queue *vq, struct videobuf_buffer *vb,
			  enum v4l2_field field)
{
	struct videobuf_res_privdata *res = (struct videobuf_res_privdata *)vq
		->priv_data;
	struct vivi_fh *fh = (struct vivi_fh *)res->priv;
	struct vivi_dev *dev = fh->dev;
	struct vivi_buffer
	*buf = container_of(vb, struct vivi_buffer, vb);
	int rc;

	dprintk(dev, 1, "%s, field=%d\n", __func__, field);

	WARN_ON(!fh->fmt);

	if (fh->width < 48 ||
	    fh->width > NORM_MAXW ||
	    fh->height < 32 ||
	    fh->height > NORM_MAXH)
		return -EINVAL;

	buf->vb.size = (fh->width * fh->height * fh->fmt->depth) >> 3;
	if (buf->vb.baddr != 0 && buf->vb.bsize < buf->vb.size)
		return -EINVAL;
	/* These properties only change when queue is idle, see s_fmt */
	buf->fmt = fh->fmt;
	buf->vb.width = fh->width;
	buf->vb.height = fh->height;
	buf->vb.field = field;
	if (buf->vb.state == VIDEOBUF_NEEDS_INIT) {
		rc = videobuf_iolock(vq, &buf->vb, NULL);
		if (rc < 0)
			goto fail;
	}
	buf->vb.state = VIDEOBUF_PREPARED;
	return 0;

fail: free_buffer(vq, buf);
	return rc;
}

static void buffer_queue(struct videobuf_queue *vq, struct videobuf_buffer *vb)
{
	struct vivi_buffer
	*buf = container_of(vb, struct vivi_buffer, vb);
	struct videobuf_res_privdata *res = (struct videobuf_res_privdata *)vq
		->priv_data;
	struct vivi_fh *fh = (struct vivi_fh *)res->priv;
	struct vivi_dev *dev = fh->dev;
	struct vivi_dmaqueue *vidq = &dev->vidq;

	dprintk(dev, 1, "%s\n", __func__);
	buf->vb.state = VIDEOBUF_QUEUED;
	list_add_tail(&buf->vb.queue, &vidq->active);
}

static void buffer_release(struct videobuf_queue *vq,
			   struct videobuf_buffer *vb)
{
	struct vivi_buffer
	*buf = container_of(vb, struct vivi_buffer, vb);
	free_buffer(vq, buf);
}

static struct videobuf_queue_ops vivi_video_qops = {
	.buf_setup = buffer_setup,
	.buf_prepare = buffer_prepare,
	.buf_queue = buffer_queue,
	.buf_release = buffer_release,
};

/* ------------------------------------------------------------------
 * IOCTL vidioc handling
 * ------------------------------------------------------------------
 */
static int vidioc_querycap(struct file *file, void *priv,
			   struct v4l2_capability *cap)
{
	struct vivi_fh *fh = priv;
	struct vivi_dev *dev = fh->dev;

	strcpy(cap->driver, "amlvideo");
	strcpy(cap->card, "amlvideo");
	strlcpy(cap->bus_info, dev->v4l2_dev.name, sizeof(cap->bus_info));
	cap->version = AMLVIDEO_VERSION;
	cap->device_caps = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING
			| V4L2_CAP_READWRITE;
	cap->capabilities = cap->device_caps | V4L2_CAP_DEVICE_CAPS;
	return 0;
}

static int vidioc_enum_fmt_vid_cap(struct file *file, void *priv,
				   struct v4l2_fmtdesc *f)
{
	struct vivi_fmt *fmt;

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
	struct vivi_fh *fh = priv;

	f->fmt.pix.width = fh->width;
	f->fmt.pix.height = fh->height;
	f->fmt.pix.field = fh->vb_vidq.field;
	f->fmt.pix.pixelformat = fh->fmt->fourcc;
	f->fmt.pix.bytesperline = (f->fmt.pix.width * fh->fmt->depth) >> 3;
	f->fmt.pix.sizeimage = f->fmt.pix.height * f->fmt.pix.bytesperline;

	return 0;
}

static int vidioc_try_fmt_vid_cap(struct file *file, void *priv,
				  struct v4l2_format *f)
{
	struct vivi_fh *fh = priv;
	int ret = 0;

	fh->fmt = get_format(f);
	fh->width = f->fmt.pix.width;
	fh->height = f->fmt.pix.height;
	fh->vb_vidq.field = f->fmt.pix.field;
	fh->type = f->type;

	return ret;
}

/*FIXME: This seems to be generic enough to be at videodev2 */
static int vidioc_s_fmt_vid_cap(struct file *file, void *priv,
				struct v4l2_format *f)
{
	struct vivi_fh *fh = priv;
	int ret = 0;

	fh->fmt = get_format(f);
	fh->width = f->fmt.pix.width;
	fh->height = f->fmt.pix.height;
	fh->vb_vidq.field = f->fmt.pix.field;
	fh->type = f->type;
	return ret;
}

static int vidioc_reqbufs(struct file *file, void *priv,
			  struct v4l2_requestbuffers *p)
{
	struct vivi_fh *fh = priv;

	return videobuf_reqbufs(&fh->vb_vidq, p);
}

static int vidioc_querybuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	struct vivi_fh *fh = priv;

	return videobuf_querybuf(&fh->vb_vidq, p);
}

static int vidioc_qbuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	return 0;
}

static int vidioc_dqbuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	struct vivi_dev *dev = video_drvdata(file);
	int ret = 0;
	u64 pts_us64 = 0;
	u64 pts_tmp;
	struct vframe_s *next_vf;

	if (vfq_level(&dev->q_ready) > AMLVIDEO_POOL_SIZE - 1)
		return -EAGAIN;

	if (!vf_peek(dev->vf_receiver_name))
		return -EAGAIN;

	mutex_lock(&dev->vf_mutex);

	dev->vf = vf_get(dev->vf_receiver_name);
	if (!dev->vf) {
		/* printk("%s, %s, %d\n", __FILE__, __FUNCTION__, __LINE__); */
		mutex_unlock(&dev->vf_mutex);
		return -EAGAIN;
	}

	if (dev->vf->index == 0xFFFFFFFF) {
		pr_info("%s: Invalid vf\n", __func__);
		mutex_unlock(&dev->vf_mutex);
		return -EAGAIN;
	}

	dev->vf->omx_index = dev->frame_num;
	if (dev->vf->type & VIDTYPE_V4L_EOS) {
		ret = -EAGAIN;
		goto dqbuf_done;
	}
	dev->am_parm.signal_type = dev->vf->signal_type;
	dev->am_parm.master_display_colour =
		dev->vf->prop.master_display_colour;
	if (dev->vf->hdr10p_data_size > 0 && dev->vf->hdr10p_data_buf) {
		if (dev->vf->hdr10p_data_size <= 128) {
			dev->am_parm.hdr10p_data_size =
					dev->vf->hdr10p_data_size;
			memcpy(dev->am_parm.hdr10p_data_buf,
					dev->vf->hdr10p_data_buf,
					dev->vf->hdr10p_data_size);
		} else {
			pr_info("amlvideo: hdr10+ data size is %d, skip it!\n",
					dev->vf->hdr10p_data_size);
			dev->am_parm.hdr10p_data_size = 0;
		}
	} else {
		dev->am_parm.hdr10p_data_size = 0;
	}

	if (dev->vf->pts_us64) {
		dev->first_frame = 1;
		pts_us64 = dev->vf->pts_us64;
	} else if (dev->first_frame == 0) {
		dev->first_frame = 1;
		pts_us64 = 0;
	} else {
		pts_tmp = dur2pts(dev->vf->duration) * 100;
		do_div(pts_tmp, 9);
		pts_us64 = dev->last_pts_us64
			+ pts_tmp;
		pts_tmp = pts_us64 * 9;
		do_div(pts_tmp, 100);
		dev->vf->pts = pts_tmp;
		/*AMLVIDEO_WARN("pts= %d, dev->vf->duration= %d\n",*/
			/*dev->vf->pts, (dur2pts(dev->vf->duration)));*/
	}
	next_vf = vf_peek(dev->vf_receiver_name);
	dev->vf->next_vf_pts_valid = (next_vf ? 1 : 0);
	if (dev->vf->next_vf_pts_valid)
		dev->vf->next_vf_pts = next_vf->pts;

	p->timestamp.tv_sec = pts_us64 >> 32;
	p->timestamp.tv_usec = pts_us64 & 0xFFFFFFFF;
	dev->last_pts_us64 = pts_us64;

	if ((dev->vf->type & VIDTYPE_COMPRESS) != 0) {
		p->timecode.type = dev->vf->compWidth;
		p->timecode.flags = dev->vf->compHeight;
	} else {
		p->timecode.type = dev->vf->width;
		p->timecode.flags = dev->vf->height;
	}
dqbuf_done:
	vfq_push(&dev->q_ready, dev->vf);
	ATRACE_COUNTER(dev->v4l2_dev.name, vfq_level(&dev->q_ready));
	p->index = 0;
	p->sequence = dev->frame_num++;
	mutex_unlock(&dev->vf_mutex);

	vf_notify_receiver(dev->vf_provider_name,
			   VFRAME_EVENT_PROVIDER_VFRAME_READY,
			   NULL);

	return ret;
}

#ifdef CONFIG_VIDEO_V4L1_COMPAT
static int vidiocgmbuf(struct file *file, void *priv, struct video_mbuf *mbuf)
{
	struct vivi_fh *fh = priv;

	return videobuf_cgmbuf(&fh->vb_vidq, mbuf, 8);
}
#endif

static int vidioc_streamon(struct file *file, void *priv, enum v4l2_buf_type i)
{
	struct vivi_fh *fh = priv;
	int ret;

	if (fh->type != V4L2_BUF_TYPE_VIDEO_CAPTURE || i != fh->type)
		return -EINVAL;
	ret = videobuf_streamon(&fh->vb_vidq);
	if (ret == 0)
		fh->is_streamed_on = 1;
	return ret;
}

static int vidioc_streamoff(struct file *file, void *priv, enum v4l2_buf_type i)
{
	struct vivi_fh *fh = priv;
	int ret;

	if (fh->type != V4L2_BUF_TYPE_VIDEO_CAPTURE || i != fh->type)
		return -EINVAL;
	ret = videobuf_streamoff(&fh->vb_vidq);
	if (ret == 0)
		fh->is_streamed_on = 0;
	return ret;
}

static int vidioc_s_std(struct file *file, void *priv, v4l2_std_id i)
{
	return 0;
}

/* ------------------------------------------------------------------
 * File operations for the device
 * ------------------------------------------------------------------
 */
/*extern void get_ppmgr_buf_info(char **start, unsigned int *size);*/
static int amlvideo_open(struct file *file)
{
	struct vivi_dev *dev = video_drvdata(file);
	struct vivi_fh *fh = NULL;
	struct videobuf_res_privdata *res = NULL;
	char *bstart = NULL;
	unsigned int bsize = 0;

	dev->vf = NULL;
	dev->index = 0;
	mutex_lock(&dev->mutex);
	dev->users++;
	if (dev->users > 1) {
		dev->users--;
		mutex_unlock(&dev->mutex);
		return -EBUSY;
	}
	res = kzalloc(sizeof(*res), GFP_KERNEL);
	if (!res || dev->res) {
		kfree(res);
		dev->users--;
		mutex_unlock(&dev->mutex);
		return -ENOMEM;
	}
	fh = kzalloc(sizeof(*fh), GFP_KERNEL);
	if (!fh) {
		kfree(res);
		dev->users--;
		mutex_unlock(&dev->mutex);
		return -ENOMEM;
	}

	mutex_unlock(&dev->mutex);

	file->private_data = fh;
	fh->dev = dev;

	fh->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fh->fmt = &formats[0];
	fh->width = 1920;
	fh->height = 1080;

	res->priv = (void *)fh;
	dev->res = res;

	get_ppmgr_buf_info(&bstart, &bsize);
	res->start = (resource_size_t)bstart;
	res->end = (resource_size_t)(bstart + bsize - 1);

	res->magic = MAGIC_RE_MEM;
	videobuf_queue_res_init(&fh->vb_vidq, &vivi_video_qops, NULL,
				&dev->slock, fh->type, V4L2_FIELD_INTERLACED,
				sizeof(struct vivi_buffer), (void *)res, NULL);
	AMLVIDEO_DBG("amlvideo open");
	return 0;
}

static ssize_t amlvideo_read(struct file *file,
			     char __user *data, size_t count, loff_t *ppos)
{
	struct vivi_fh *fh = file->private_data;

	if (fh->type == V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return videobuf_read_stream(&fh->vb_vidq, data, count,
					    ppos, 0,
					    file->f_flags & O_NONBLOCK);

	return 0;
}

static unsigned int amlvideo_poll(struct file *file,
				  struct poll_table_struct *wait)
{
	struct vivi_fh *fh = file->private_data;
	struct vivi_dev *dev = fh->dev;

	dprintk(dev, 1, "%s\n", __func__);

	if (fh->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return POLLERR;

	if (vf_peek(dev->vf_receiver_name))
		return POLL_IN | POLLRDNORM;

	poll_wait(file, &dev->wq, wait);

	if (vf_peek(dev->vf_receiver_name))
		return POLL_IN | POLLRDNORM;
	else
		return 0;
}

static int amlvideo_close(struct file *file)
{
	struct vivi_fh *fh = file->private_data;
	struct vivi_dev *dev = fh->dev;

	videobuf_stop(&fh->vb_vidq);
	videobuf_mmap_free(&fh->vb_vidq);
	kfree(fh);
	dev->index = 8;
/* if (dev->res) { */
	kfree(dev->res);
	dev->res = NULL;
/* } */
	mutex_lock(&dev->mutex);
	dev->users--;
	mutex_unlock(&dev->mutex);
	if (dev->inst == 0)
		video_inuse = 0;
	AMLVIDEO_DBG("amlvideo close");
	return 0;
}

static int amlvideo_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct vivi_fh *fh = file->private_data;

	struct vivi_dev *dev = fh->dev;
	int ret;

	dprintk(dev, 1, "mmap called, vma=0x%08lx\n", (unsigned long)vma);
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	ret = videobuf_mmap_mapper(&fh->vb_vidq, vma);

	dprintk(dev, 1, "vma start=0x%08lx, size=%ld, ret=%d\n",
		(unsigned long)vma->vm_start,
		(unsigned long)vma->vm_end - (unsigned long)vma->vm_start,
		ret);

	return ret;
}

static const struct v4l2_file_operations amlvideo_fops = {
	.owner = THIS_MODULE,
	.open = amlvideo_open,
	.release = amlvideo_close,
	.read = amlvideo_read,
	.poll = amlvideo_poll,
	.unlocked_ioctl = video_ioctl2, /* V4L2 ioctl handler */
	.mmap = amlvideo_mmap,
};

static const struct v4l2_ioctl_ops amlvideo_ioctl_ops = {
	.vidioc_querycap = vidioc_querycap,
	.vidioc_enum_fmt_vid_cap = vidioc_enum_fmt_vid_cap,
	.vidioc_g_fmt_vid_cap = vidioc_g_fmt_vid_cap,
	.vidioc_try_fmt_vid_cap = vidioc_try_fmt_vid_cap,
	.vidioc_s_fmt_vid_cap = vidioc_s_fmt_vid_cap,
	.vidioc_reqbufs = vidioc_reqbufs,
	.vidioc_querybuf = vidioc_querybuf,
	.vidioc_qbuf = vidioc_qbuf,
	.vidioc_dqbuf = vidioc_dqbuf,
	.vidioc_s_std = vidioc_s_std,
	/*.vidioc_queryctrl = vidioc_queryctrl,
	 *.vidioc_g_ctrl = vidioc_g_ctrl,
	 *.vidioc_s_ctrl = vidioc_s_ctrl,
	 */
	.vidioc_streamon = vidioc_streamon,
	.vidioc_streamoff = vidioc_streamoff,
#ifdef CONFIG_VIDEO_V4L1_COMPAT
	.vidiocgmbuf = vidiocgmbuf,
#endif
	.vidioc_g_parm = vidioc_g_parm,

};

static struct video_device amlvideo_template = {
	.name = "amlvideo",
	.fops = &amlvideo_fops,
	.ioctl_ops = &amlvideo_ioctl_ops,
	.release = video_device_release,
	.tvnorms = V4L2_STD_525_60,
/* .current_norm	= V4L2_STD_NTSC_M , */
};

/* -----------------------------------------------------------------
 * Initialization and module stuff
 * -----------------------------------------------------------------
 */

static int amlvideo_release(void)
{
	struct vivi_dev *dev;
	struct list_head *list;

	while (!list_empty(&vivi_devlist)) {
		list = vivi_devlist.next;
		list_del(list);
		dev = list_entry(list, struct vivi_dev, vivi_devlist);

		v4l2_info(&dev->v4l2_dev, "unregistering %s\n",
			  video_device_node_name(dev->vfd));
		video_unregister_device(dev->vfd);
		v4l2_device_unregister(&dev->v4l2_dev);
		kfree(dev);
	}

	return 0;
}

static int __init amlvideo_create_instance(int inst)
{
	struct vivi_dev *dev;
	struct video_device *vfd;
	int ret;

	AMLVIDEO_INFO("%s called\n", __func__);
	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	snprintf(dev->v4l2_dev.name,
		 sizeof(dev->v4l2_dev.name),
		"%s-%03d", AVMLVIDEO_MODULE_NAME, inst);
	if (inst == n_devs - 1)
		AMLVIDEO_INFO("v4l2_dev.name=:%s\n", dev->v4l2_dev.name);
	ret = v4l2_device_register(NULL, &dev->v4l2_dev);

	if (ret)
		goto free_dev;

	/* init video dma queues */
	init_waitqueue_head(&dev->wq);
	INIT_LIST_HEAD(&dev->vidq.active);
	init_waitqueue_head(&dev->vidq.wq);

	/* initialize locks */
	spin_lock_init(&dev->slock);
	mutex_init(&dev->mutex);
	mutex_init(&dev->vf_mutex);

	ret = -ENOMEM;
	vfd = video_device_alloc();

	if (!vfd)
		goto unreg_dev;

	*vfd = amlvideo_template;

	vfd->dev_debug = debug;
	vfd->v4l2_dev = &dev->v4l2_dev;
	vfd->device_caps = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING
			| V4L2_CAP_READWRITE;
	dev->amlvideo_v4l_num = inst * 10 + video_nr_base;
	if (inst == 0)
		dev->amlvideo_v4l_num = video_nr_base;
	else
		dev->amlvideo_v4l_num = (inst - 1) + video_nr_base_second;
	/* //////////////////////////////////////// */
	/* vfd->v4l2_dev = &dev->v4l2_dev; */
	/* //////////////////////////////////////// */
	ret = video_register_device(vfd, VFL_TYPE_GRABBER,
				    dev->amlvideo_v4l_num);

	if (ret < 0)
		goto rel_vdev;

	dev->inst = inst;

	if (inst != 0) {
		snprintf(dev->vf_receiver_name, AMLVIDEO_VF_NAME_SIZE,
			 RECEIVER_NAME_PIP ".%x", inst & 0xff);

		snprintf(dev->vf_provider_name, AMLVIDEO_VF_NAME_SIZE,
			 PROVIDER_NAME_PIP ".%x", inst & 0xff);
	} else {
		memcpy(dev->vf_receiver_name, RECEIVER_NAME,
		       sizeof(RECEIVER_NAME));
		memcpy(dev->vf_provider_name, PROVIDER_NAME,
		       sizeof(PROVIDER_NAME));
	}

	vf_receiver_init(&dev->video_vf_recv,
			 dev->vf_receiver_name,
			 &video_vf_receiver, dev);
	vf_reg_receiver(&dev->video_vf_recv);

	video_set_drvdata(vfd, dev);

	/* Now that everything is fine, let's add it to device list */
	list_add_tail(&dev->vivi_devlist, &vivi_devlist);

	dev->vfd = vfd;
	if (inst == n_devs - 1 || inst == 0)
		v4l2_dbg(0, debug, &dev->v4l2_dev,
			  "V4L2 device registered as %s\n",
			  video_device_node_name(vfd));
	return 0;

rel_vdev: video_device_release(vfd);
unreg_dev: v4l2_device_unregister(&dev->v4l2_dev);
free_dev:
	dev->res = NULL;
	kfree(dev);
	return ret;
}

#undef NORM_MAXW
#undef NORM_MAXH
/* #define __init */
/* This routine allocates from 1 to n_devs virtual drivers.*/
/*
 *The real maximum number of virtual drivers will depend on how many drivers
 *will succeed. This is limited to the maximum number of devices that
 *videodev supports, which is equal to VIDEO_NUM_DEVICES.
 */
int __init amlvideo_init(void)
{
	int ret = 0, i;

	AMLVIDEO_INFO("%s called", __func__);
	if (n_devs <= 0)
		n_devs = 1;

	for (i = 0; i < n_devs; i++) {
		ret = amlvideo_create_instance(i);
		if (ret) {
			/* If some instantiations succeeded,
			 * keep driver
			 */
			if (i)
				ret = 0;
			break;
		}
	}

	if (ret < 0) {
		/* printk(KERN_INFO "Error %d while
		 *  loading vivi driver\n", ret);
		 */
		return ret;
	}

	/* printk(KERN_INFO "Video Technology Magazine Virtual Video " */
	/* "Capture Board ver %u.%u.%u successfully loaded.\n", */
	/* (AMLVIDEO_VERSION >> 16) & 0xFF, (AMLVIDEO_VERSION >> 8) & 0xFF, */
	/* AMLVIDEO_VERSION & 0xFF); */

	/* n_devs will reflect the actual number of allocated devices */
	n_devs = i;
	return ret;
}

void __exit amlvideo_exit(void)
{
	/*vf_unreg_receiver(&video_vf_recv);*/
	amlvideo_release();
}

