// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/errno.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/poll.h>
#include <linux/workqueue.h>
#include <linux/time.h>
#include <linux/mm.h>
#include <asm/div64.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_reserved_mem.h>
#include <linux/of_irq.h>
#include <linux/cma.h>
#include <linux/dma-buf.h>
#include <linux/scatterlist.h>
#include <linux/mm_types.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/dma-contiguous.h>
#include <linux/amlogic/iomap.h>
#include <linux/fdtable.h>
#include <linux/dma-buf.h>

/* v4l2 core */
#include <linux/videodev2.h>
#include <linux/v4l2-dv-timings.h>
#include <media/v4l2-common.h>
#include <media/v4l2-device.h>
#include <media/v4l2-dev.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-dv-timings.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-event.h>
#include <media/v4l2-mem2mem.h>
#include <media/videobuf2-v4l2.h>
#include <media/videobuf2-dma-contig.h>
#include <media/videobuf2-memops.h>

/* Amlogic Headers */
/*#include <linux/amlogic/media/vpu/vpu.h>*/
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>
#include <linux/amlogic/media/frame_sync/timestamp.h>
#include <linux/amlogic/media/frame_sync/tsync.h>
#include <linux/amlogic/meson_uvm_core.h>
#include <linux/amlogic/media/vout/vinfo.h>
#include <linux/amlogic/media/vout/vout_notify.h>

/* Local Headers */
/*#include "../tvin_global.h"*/
#include "../tvin_format_table.h"
/*#include "../tvin_frontend.h"*/
/*#include "../tvin_global.h"*/
#include "vdin_regs.h"
#include "vdin_drv.h"
#include "vdin_ctl.h"
#include "vdin_sm.h"
#include "vdin_vf.h"
#include "vdin_canvas.h"
#include "vdin_afbce.h"
#include "vdin_v4l2_if.h"

/*give a default page size*/
#define VDIN_IMG_SIZE		(1024 * 8)

int vdin_v4l_debug;

#define dprintk(level, fmt, arg...)				\
	do {							\
		if (vdin_v4l_debug >= (level))			\
			pr_info("vdin-v4l: " fmt, ## arg);	\
	} while (0)

static struct v4l2_frmsize_discrete vdin_v4l2_frmsize_dis[] = {
	{320, 240},		{640, 480},		{960, 540},
	{1280, 720},	{1920, 1080},	{3840, 2160}
};

static struct v4l2_fract fract_discrete[] = {
	{.numerator = 1, .denominator = 24,},
	{.numerator = 1, .denominator = 25,},
	{.numerator = 1, .denominator = 30,},
	{.numerator = 1, .denominator = 50,},
	{.numerator = 1, .denominator = 60,},
};

static struct vdin_v4l2_pix_fmt pix_formats[] = {
	{.fourcc = V4L2_PIX_FMT_NV12,
	 .depth  = 12, },

	{.fourcc = V4L2_PIX_FMT_NV21,
	 .depth  = 12, },

	{.fourcc = V4L2_PIX_FMT_NV12M,
	 .depth  = 12, },

	{.fourcc = V4L2_PIX_FMT_NV21M,
	 .depth  = 12, },

	{.fourcc = V4L2_PIX_FMT_UYVY,
	 .depth  = 16, },
};

static struct v4l2_capability g_vdin_v4l2_cap[VDIN_MAX_DEVS] = {
	{.driver = VDIN_V4L_DRV_NAME,	 .card = VDIN_V4L_CARD_NAME,
	 .bus_info = VDIN0_V4L_BUS_INFO, .version = VDIN_DEV_VER,
	 .capabilities = VDIN_DEVICE_CAPS | V4L2_CAP_DEVICE_CAPS,
	 .device_caps = VDIN_DEVICE_CAPS},

	{.driver = VDIN_V4L_DRV_NAME,	 .card = VDIN_V4L_CARD_NAME,
	 .bus_info = VDIN1_V4L_BUS_INFO, .version = VDIN_DEV_VER,
	 .capabilities = VDIN_DEVICE_CAPS | V4L2_CAP_DEVICE_CAPS,
	 .device_caps = VDIN_DEVICE_CAPS},
};

int vdin_v4l2_if_isr(struct vdin_dev_s *devp, struct vframe_s *vfp)
{
	if (!devp->vb_queue.streaming) {
		dprintk(2, "not streaming\n");
		return -1;
	}

	if (devp->dbg_v4l_pause)
		return -1;
	/* do framerate control */
	if (devp->vdin_v4l2.divide > 1 && (devp->frame_cnt % devp->vdin_v4l2.divide) != 0) {
		devp->vdin_v4l2.stats.drop_divide++;
		dprintk(3, "%s,drop_divide:%u\n", __func__,
			devp->vdin_v4l2.stats.drop_divide);
		return -1;
	}
	spin_lock(&devp->list_head_lock);

	if (list_empty(&devp->buf_list)) {
		dprintk(2, "warning: buffer empty\n");
		spin_unlock(&devp->list_head_lock);
		return -1;
	}
	/* pop a buffer */
	devp->cur_buff = list_first_entry(&devp->buf_list,
					  struct vdin_vb_buff, list);

	dprintk(3, "[%s]vf index = %d\n", __func__, vfp->index);
	devp->cur_buff->v4l2_vframe_s = vfp;
	list_del(&devp->cur_buff->list);
	spin_unlock(&devp->list_head_lock);
	devp->vdin_v4l2.stats.done_cnt++;
	vb2_buffer_done(&devp->cur_buff->vb.vb2_buf, VB2_BUF_STATE_DONE);

	return 0;
}

/**
 * enum vb2_buffer_state - current video buffer state
 * @VB2_BUF_STATE_DEQUEUED:	buffer under userspace control
 * @VB2_BUF_STATE_PREPARING:	buffer is being prepared in videobuf
 * @VB2_BUF_STATE_PREPARED:	buffer prepared in videobuf and by the driver
 * @VB2_BUF_STATE_QUEUED:	buffer queued in videobuf, but not in driver
 * @VB2_BUF_STATE_REQUEUEING:	re-queue a buffer to the driver
 * @VB2_BUF_STATE_ACTIVE:	buffer queued in driver and possibly used
 *				in a hardware operation
 * @VB2_BUF_STATE_DONE:		buffer returned from driver to videobuf, but
 *				not yet dequeued to userspace
 * @VB2_BUF_STATE_ERROR:	same as above, but the operation on the buffer
 *				has ended with an error, which will be reported
 *				to the userspace when it is dequeued
 */
char *vb2_buf_sts_to_str(uint32_t state)
{
	switch (state) {
	case VB2_BUF_STATE_DEQUEUED:
		return "VB2_BUF_STATE_DEQUEUED(0)";
	case VB2_BUF_STATE_IN_REQUEST:
		return "VB2_BUF_STATE_PREPARED(1)";
	case VB2_BUF_STATE_PREPARING:
		return "VB2_BUF_STATE_PREPARING(2)";
	case VB2_BUF_STATE_QUEUED:
		return "VB2_BUF_STATE_QUEUED(3)";
	case VB2_BUF_STATE_ACTIVE:
		return "VB2_BUF_STATE_ACTIVE(4)";
	case VB2_BUF_STATE_DONE:
		return "VB2_BUF_STATE_DONE(5)";
	case VB2_BUF_STATE_ERROR:
		return "VB2_BUF_STATE_ERROR(6)";
	default:
		return "VB2_BUF_STATE_UNKNOWN";
	}
}

char *vb2_memory_sts_to_str(uint32_t memory)
{
	switch (memory) {
	case VB2_MEMORY_MMAP:
		return "VB2_MEMORY_MMAP";
	case VB2_MEMORY_USERPTR:
		return "VB2_MEMORY_USERPTR";
	case VB2_MEMORY_DMABUF:
		return "VB2_MEMORY_DMABUF";
	default:
		return "VB2_MEMORY_UNKNOWN";
	}
}

void vdin_fill_pix_format(struct vdin_dev_s *devp)
{
	struct v4l2_format *v4l2_fmt = NULL;
	//unsigned int scan_mod = TVIN_SCAN_MODE_PROGRESSIVE;

	if (IS_ERR_OR_NULL(devp))
		return;

	v4l2_fmt = &devp->v4l2_fmt;

	if (v4l2_fmt->fmt.pix_mp.num_planes == 1) {
		if (v4l2_fmt->fmt.pix_mp.pixelformat == V4L2_PIX_FMT_UYVY) {
			v4l2_fmt->fmt.pix_mp.plane_fmt[0].sizeimage =
				v4l2_fmt->fmt.pix_mp.width * v4l2_fmt->fmt.pix_mp.height * 2;
			v4l2_fmt->fmt.pix_mp.plane_fmt[0].bytesperline =
				(v4l2_fmt->fmt.pix_mp.width * 16) >> 3;
		} else if (v4l2_fmt->fmt.pix_mp.pixelformat == V4L2_PIX_FMT_NV12 ||
				   v4l2_fmt->fmt.pix_mp.pixelformat == V4L2_PIX_FMT_NV21) {
			v4l2_fmt->fmt.pix_mp.plane_fmt[0].sizeimage =
				v4l2_fmt->fmt.pix_mp.width * v4l2_fmt->fmt.pix_mp.height * 3 / 2;
			v4l2_fmt->fmt.pix_mp.plane_fmt[0].bytesperline =
				(v4l2_fmt->fmt.pix_mp.width * 12) >> 3;
		}
		v4l2_fmt->fmt.pix_mp.plane_fmt[1].sizeimage = 0;
		v4l2_fmt->fmt.pix_mp.plane_fmt[1].bytesperline = 0;
	} else if (v4l2_fmt->fmt.pix_mp.num_planes == 2) {
		if (v4l2_fmt->fmt.pix_mp.pixelformat == V4L2_PIX_FMT_NV16M ||
			v4l2_fmt->fmt.pix_mp.pixelformat == V4L2_PIX_FMT_NV61M) {
			v4l2_fmt->fmt.pix_mp.plane_fmt[0].sizeimage =
				v4l2_fmt->fmt.pix_mp.width * v4l2_fmt->fmt.pix_mp.height;
			v4l2_fmt->fmt.pix_mp.plane_fmt[1].sizeimage =
				v4l2_fmt->fmt.pix_mp.width * v4l2_fmt->fmt.pix_mp.height;
			v4l2_fmt->fmt.pix_mp.plane_fmt[0].bytesperline =
				v4l2_fmt->fmt.pix_mp.width;
			v4l2_fmt->fmt.pix_mp.plane_fmt[1].bytesperline =
				v4l2_fmt->fmt.pix_mp.width;
		} else if (v4l2_fmt->fmt.pix_mp.pixelformat == V4L2_PIX_FMT_NV12M ||
				   v4l2_fmt->fmt.pix_mp.pixelformat == V4L2_PIX_FMT_NV21M) {
			v4l2_fmt->fmt.pix_mp.plane_fmt[0].sizeimage =
				v4l2_fmt->fmt.pix_mp.width * v4l2_fmt->fmt.pix_mp.height;
			v4l2_fmt->fmt.pix_mp.plane_fmt[1].sizeimage =
				v4l2_fmt->fmt.pix_mp.width * v4l2_fmt->fmt.pix_mp.height / 2;
			v4l2_fmt->fmt.pix_mp.plane_fmt[0].bytesperline =
				v4l2_fmt->fmt.pix_mp.width;
			v4l2_fmt->fmt.pix_mp.plane_fmt[1].bytesperline =
				v4l2_fmt->fmt.pix_mp.width / 2;
		}
	} else {
		pr_err("vdin%d,err.not support num_planes=%d\n ",
			devp->index, v4l2_fmt->fmt.pix_mp.num_planes);
		return;
	}
	v4l2_fmt->fmt.pix_mp.plane_fmt[0].sizeimage =
		PAGE_ALIGN(v4l2_fmt->fmt.pix_mp.plane_fmt[0].sizeimage);
	v4l2_fmt->fmt.pix_mp.plane_fmt[1].sizeimage =
		PAGE_ALIGN(v4l2_fmt->fmt.pix_mp.plane_fmt[1].sizeimage);

	dprintk(1, "vdin%d,num_planes=%d\n ",
		devp->index, v4l2_fmt->fmt.pix_mp.num_planes);
	dprintk(1, "plane 0:sizeimage=%x;plane 1:sizeimage=%x\n ",
		v4l2_fmt->fmt.pix_mp.plane_fmt[0].sizeimage,
		v4l2_fmt->fmt.pix_mp.plane_fmt[1].sizeimage);
}

static int vdin_v4l2_get_phy_addr(struct vdin_dev_s *devp,
	struct v4l2_buffer *p, unsigned int plane_no)
{
	int idx;
	int fd;
	struct page *page;
	struct vb2_queue *vb_que = NULL;
	struct vb2_buffer *vb2buf = NULL;

	if (p->m.planes[plane_no].m.fd < 0 || p->index >= devp->v4l2_req_buf_num) {
		dprintk(0, "v4l2 buffer index=%d or fd=%d,out of range!!!\n ",
			   p->index, p->m.planes[plane_no].m.fd);
		return -EINVAL;
	}

	fd		= p->m.planes[0].m.fd;
	idx		= p->index;

	if (p->memory == V4L2_MEMORY_MMAP) {
		vb_que  = &devp->vb_queue;
		vb2buf = vb_que->bufs[p->index];
		devp->st_vdin_set_canvas_addr[idx][plane_no].index = idx;
		devp->st_vdin_set_canvas_addr[idx][plane_no].paddr =
			vb2_dma_contig_plane_dma_addr(vb2buf, plane_no);
		devp->st_vdin_set_canvas_addr[idx][plane_no].size  =
			vb2buf->planes[plane_no].length;
	} else if (p->memory == V4L2_MEMORY_DMABUF) {
		fd = p->m.planes[plane_no].m.fd;
		if (fd <= 0) {
			dprintk(0, "VDIN err plane:%d,buf idx:%d,dmabuf == NULL\n",
				plane_no, idx);
			return -EINVAL;
		}
		devp->st_vdin_set_canvas_addr[idx][plane_no].fd    = fd;
		devp->st_vdin_set_canvas_addr[idx][plane_no].index = idx;
		devp->st_vdin_set_canvas_addr[idx][plane_no].dma_buffer = dma_buf_get(fd);
		if (!devp->st_vdin_set_canvas_addr[idx][plane_no].dma_buffer) {
			dprintk(0, "VDIN err plane:%d,buf idx:%d,dmabuf == NULL\n",
				plane_no, idx);
			return -1;
		}
		devp->st_vdin_set_canvas_addr[idx][plane_no].dmabuf_attach =
			dma_buf_attach(devp->st_vdin_set_canvas_addr[idx][plane_no].dma_buffer,
				devp->dev);
		devp->st_vdin_set_canvas_addr[idx][plane_no].sg_table =
			dma_buf_map_attachment(devp->st_vdin_set_canvas_addr[idx][plane_no]
				.dmabuf_attach, DMA_BIDIRECTIONAL);
		page = sg_page(devp->st_vdin_set_canvas_addr[idx][plane_no].sg_table->sgl);
		devp->st_vdin_set_canvas_addr[idx][plane_no].paddr = PFN_PHYS(page_to_pfn(page));
		devp->st_vdin_set_canvas_addr[idx][plane_no].size  =
			devp->st_vdin_set_canvas_addr[idx][plane_no].dma_buffer->size;
		dprintk(2, "vdin%d,fd:%d,phy_addr:%lx,size:%#x\n", devp->index, fd,
			devp->st_vdin_set_canvas_addr[idx][plane_no].paddr,
			devp->st_vdin_set_canvas_addr[idx][plane_no].size);
	} else {
		dprintk(0, "err.idx:%d,unsupported memory:%d\n", idx, p->memory);
		return -1;
	}

	if (plane_no == VDIN_PLANES_IDX_Y)
		devp->vf_mem_start[idx] =
			devp->st_vdin_set_canvas_addr[idx][plane_no].paddr;
	else
		devp->vf_mem_c_start[idx] =
			devp->st_vdin_set_canvas_addr[idx][plane_no].paddr;

	devp->vf_mem_start[idx] =
		roundup(devp->vf_mem_start[idx], devp->canvas_align);
	devp->vf_mem_c_start[idx] =
		roundup(devp->vf_mem_c_start[idx], devp->canvas_align);
	dprintk(1, "%s vdin%d,paddr[%d][%d] = %lx,size=%x\n", __func__,
		devp->index, idx, plane_no,
		devp->st_vdin_set_canvas_addr[idx][plane_no].paddr,
		devp->st_vdin_set_canvas_addr[idx][plane_no].size);

	return 0;
}

/*
 * Query device capability
 * cmd ID:VIDIOC_QUERYCAP
 */
static int vdin_vidioc_querycap(struct file *file, void *priv,
				struct v4l2_capability *cap)
{
	struct vdin_dev_s *devp = video_drvdata(file);

	if (IS_ERR_OR_NULL(devp))
		return -EFAULT;

	cap->version	  = g_vdin_v4l2_cap[devp->index].version;
	cap->device_caps  = g_vdin_v4l2_cap[devp->index].device_caps;
	cap->capabilities = g_vdin_v4l2_cap[devp->index].capabilities;

	strcpy(cap->driver,	  g_vdin_v4l2_cap[devp->index].driver);
	strcpy(cap->bus_info, g_vdin_v4l2_cap[devp->index].bus_info);
	strcpy(cap->card,	  g_vdin_v4l2_cap[devp->index].card);

	return 0;
}

/*
 * query video standard
 * cmd ID: VIDIOC_QUERYSTD
 */
static int vdin_vidioc_querystd(struct file *file, void *priv,
				v4l2_std_id *std)
{
	struct vdin_dev_s *devp = video_drvdata(file);

	dprintk(2, "%s\n", __func__);
	if (IS_ERR_OR_NULL(devp))
		return -EFAULT;

	return 0;
}

/*
 * enum current input
 * cmd ID: VIDIOC_ENUMINPUT
 */
static int vdin_vidioc_enum_input(struct file *file,
				  void *fh, struct v4l2_input *inp)
{
	const char *str = NULL;
	struct vdin_dev_s *devp = video_drvdata(file);

	if (IS_ERR_OR_NULL(devp))
		return -EFAULT;

	if (inp->index >= devp->v4l2_port_num) {
		dprintk(0, "%s index:%d,v4l2 port num = %d,end\n",
			__func__, inp->index, devp->v4l2_port_num);
		return -EINVAL;
	}

	inp->std = V4L2_STD_ALL;
	str = tvin_port_str(devp->v4l2_port[inp->index]);

	if (str)
		strlcpy(inp->name, str, sizeof(inp->name));

	dprintk(1, "%s,port[%d]:%s\n", __func__, inp->index, inp->name);
	return 0;
}

/*
 * allocate the video frame buffer
 * cmd ID:VIDIOC_REQBUFS
 */
static int vdin_vidioc_reqbufs(struct file *file, void *priv,
			       struct v4l2_requestbuffers *reqbufs)
{
	struct vdin_dev_s *devp = video_drvdata(file);
	int ret = 0;
	unsigned int i = 0;
	struct vb2_v4l2_buffer *vb_buf = NULL;
	struct vdin_vb_buff *vdin_buf = NULL;

	if (IS_ERR_OR_NULL(devp))
		return -EPERM;

	if (reqbufs->memory != V4L2_MEMORY_DMABUF &&
		reqbufs->memory != V4L2_MEMORY_MMAP) {
		dprintk(0, "%s err,memory=%d,only support DMABUF and MMAP\n",
			__func__, reqbufs->memory);
		return -EINVAL;
	}

	if (reqbufs->count == 0) {
		dprintk(0, "%s type:%d count:%d\n", __func__,
			reqbufs->type, reqbufs->count);
		return 0;
	}
	if (reqbufs->count < devp->vb_queue.min_buffers_needed ||
		reqbufs->count > VDIN_CANVAS_MAX_CNT) {
		dprintk(0, "%s err,count=%d,out of range[%d,%d]\n", __func__,
			reqbufs->count, devp->vb_queue.min_buffers_needed, VDIN_CANVAS_MAX_CNT);
		return -EINVAL;
	}

	dprintk(1, "%s type:%d buff_num:%d\n", __func__, reqbufs->type, reqbufs->count);

	/*need config by input source type*/
	devp->source_bitdepth = VDIN_COLOR_DEEPS_8BIT;
	devp->vb_queue.type = reqbufs->type;

	//vdin_buffer_calculate(devp, req_buffs_num);
	vdin_fill_pix_format(devp);

	ret = vb2_ioctl_reqbufs(file, priv, reqbufs);
	if (ret < 0)
		dprintk(0, "vb2_ioctl_reqbufs fail\n");

	devp->v4l2_req_buf_num = reqbufs->count;

	vb_buf = to_vb2_v4l2_buffer(devp->vb_queue.bufs[i]);
	vdin_buf = to_vdin_vb_buf(vb_buf);

	/*check buffer*/
	dprintk(1, "%s num_buffers %d -end\n", __func__,
		devp->vb_queue.num_buffers);
	return ret;
}

/*
 *
 * cmd ID:VIDIOC_CREATE_BUFS
 */
int vdin_vidioc_create_bufs(struct file *file, void *priv,
			    struct v4l2_create_buffers *p)
{
	/*struct vdin_dev_s *devp = video_drvdata(file);*/
	unsigned int ret = 0;

	dprintk(2, "%s\n", __func__);
	ret = vb2_ioctl_create_bufs(file, priv, p);
	return ret;
}

/*
 * Got every buffer info, and mmp to userspace
 * cmd ID:VIDIOC_QUERYBUF
 */
static int vdin_vidioc_querybuf(struct file *file, void *priv,
				struct v4l2_buffer *v4lbuf)
{
	struct vdin_dev_s *devp = video_drvdata(file);
	struct vb2_queue *vb_que = NULL;
	unsigned int ret = 0;
	struct vb2_buffer *vb2buf = NULL;

	if (IS_ERR_OR_NULL(devp))
		return -EFAULT;

	dprintk(1, "%s idx:%d\n", __func__, v4lbuf->index);

	vb_que = &devp->vb_queue;
	vb2buf = vb_que->bufs[v4lbuf->index];

	ret = vb2_ioctl_querybuf(file, priv, v4lbuf);

	return ret;
}

/*
 * user put a empty vframe to driver empty video buffer
 * cmd ID:VIDIOC_QBUF
 */
static int vdin_vidioc_qbuf(struct file *file, void *priv,
			    struct v4l2_buffer *p)
{
	struct vdin_dev_s *devp = video_drvdata(file);
	int ret = 0;
	struct vb2_v4l2_buffer *vb = NULL;
	struct vdin_vb_buff *vdin_buf = NULL;
	int i;
	unsigned int num_planes;

	if (IS_ERR_OR_NULL(devp))
		return -EFAULT;

	vb = to_vb2_v4l2_buffer(devp->vb_queue.bufs[p->index]);
	vdin_buf = to_vdin_vb_buf(vb);
	num_planes = vb->vb2_buf.num_planes;
	dprintk(3, "%s idx:%d planes:%d,streaming:%d\n", __func__,
		p->index, num_planes, devp->vb_queue.streaming);

	ret = vb2_ioctl_qbuf(file, priv, p);
	if (ret < 0)
		dprintk(0, "%s err\n", __func__);

	/* recycle buffer */
	if (devp->vb_queue.streaming) {
		devp->vdin_v4l2.stats.que_cnt++;
		if (!IS_ERR(vdin_buf->v4l2_vframe_s)) {
			receiver_vf_put(vdin_buf->v4l2_vframe_s, devp->vfp);
			dprintk(3, "[%s]vf idx:%d (0x%p) put back to pool,fd=%d,canvas0Addr:%#x\n",
				__func__,
				vdin_buf->v4l2_vframe_s->index, devp->vfp, p->m.fd,
				vdin_buf->v4l2_vframe_s->canvas0Addr);

			vdin_buf->v4l2_vframe_s = NULL;
		} else {
			dprintk(0, "err vf null\n");
		}
	} else {
		for (i = 0; i < num_planes; i++)
			vdin_v4l2_get_phy_addr(devp, p, i);
	}

	return ret;
}

/*
 * user get a vframe from driver filled video buffer
 * cmd ID:VIDIOC_DQBUF
 */
static int vdin_vidioc_dqbuf(struct file *file, void *priv,
			     struct v4l2_buffer *p)
{
	unsigned int ret = 0, i;
	struct vdin_dev_s *devp = video_drvdata(file);
	struct vb2_v4l2_buffer *vb = NULL;
	struct vdin_vb_buff *vdin_buf = NULL;

	ret = vb2_ioctl_dqbuf(file, priv, p);
	if (ret) {
		dprintk(0, "DQ error,ret=%d,%#x\n", ret, file->f_flags);
		return -1;
	}

	for (i = 0; i < devp->v4l2_fmt.fmt.pix_mp.num_planes; i++)
		p->m.planes[i].bytesused = p->m.planes[i].length;

	/*frame_cnt++;*/
	vb = to_vb2_v4l2_buffer(devp->vb_queue.bufs[p->index]);
	vdin_buf = to_vdin_vb_buf(vb);
	devp->vdin_v4l2.stats.dque_cnt++;
	dprintk(3, "%s index=%d,fd = %d;vf_index:%d,canvas0Addr:%#x\n", __func__, p->index, p->m.fd,
		vdin_buf->v4l2_vframe_s->index, vdin_buf->v4l2_vframe_s->canvas0Addr);

	return ret;
}

/*
 *
 * cmd ID:VIDIOC_EXPBUF
 */
static int vdin_vidioc_expbuf(struct file *file, void *priv,
			      struct v4l2_exportbuffer *p)
{
	struct vdin_dev_s *devp = video_drvdata(file);
	struct dma_buf *dmabuf;
	int ret;

	if (IS_ERR_OR_NULL(devp))
		return -EFAULT;

	dprintk(1, "%s buf:%d\n", __func__, p->index);

	ret = vb2_ioctl_expbuf(file, priv, p);

	dmabuf = dma_buf_get(p->fd);
	if (IS_ERR_OR_NULL(dmabuf))
		dprintk(0, "get dma buf err\n");

	return ret;
}

static int vdin_vidioc_streamon(struct file *file, void *priv,
				enum v4l2_buf_type i)
{
	struct vdin_dev_s *devp = video_drvdata(file);
	unsigned int ret = 0;

	dprintk(2, "%s\n", __func__);

	if (IS_ERR_OR_NULL(devp))
		return -EFAULT;

	ret = vb2_ioctl_streamon(file, priv, i);
	vdin_v4l2_start_tvin(devp);
	memset(&devp->vdin_v4l2.stats, 0, sizeof(devp->vdin_v4l2.stats));
	dprintk(2, "%s\n", __func__);
	return 0;
}

static int vdin_vidioc_streamoff(struct file *file, void *priv,
				 enum v4l2_buf_type i)
{
	struct vdin_dev_s *devp = video_drvdata(file);
	int ret = 0;

	dprintk(0, "%s\n", __func__);
	if (IS_ERR_OR_NULL(devp))
		return -EFAULT;

	ret = vb2_ioctl_streamoff(file, priv, i);
	if (ret < 0)
		dprintk(0, "%s failed with %d\n", __func__, ret);

	ret = vdin_v4l2_stop_tvin(devp);

	return ret;
}

/*
 * for single plane
 * cmd ID:VIDIOC_G_FMT
 */
static int vdin_vidioc_g_fmt_vid_cap(struct file *file, void *priv,
				     struct v4l2_format *f)
{
	struct vdin_dev_s *devp = video_drvdata(file);

	dprintk(2, "%s\n", __func__);
	if (IS_ERR_OR_NULL(devp))
		return -EFAULT;
	/*for test set a default value*/
	devp->v4l2_fmt.fmt.pix.width = 1280;
	devp->v4l2_fmt.fmt.pix.height = 720;
	devp->v4l2_fmt.fmt.pix.quantization = V4L2_QUANTIZATION_DEFAULT;
	devp->v4l2_fmt.fmt.pix.ycbcr_enc = V4L2_YCBCR_ENC_709;
	devp->v4l2_fmt.fmt.pix.field = V4L2_FIELD_ANY;
	devp->v4l2_fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;

	memcpy(&f->fmt.pix, &devp->v4l2_fmt.fmt.pix,
	       sizeof(struct v4l2_pix_format));
	return 0;
}

/*
 * user get a vframe from driver filled video buffer
 * cmd ID:VIDIOC_S_FMT
 */
static int vdin_vidioc_s_fmt_vid_cap(struct file *file, void *priv,
				     struct v4l2_format *fmt)
{
	struct vdin_dev_s *devp = video_drvdata(file);
	/*struct v4l2_format *fmt = devp->pixfmt;*/

	dprintk(2, "%s\n", __func__);
	if (IS_ERR_OR_NULL(devp))
		return -EFAULT;

	dprintk(1, "width=%d\n", fmt->fmt.pix.width);
	dprintk(1, "height=%d\n", fmt->fmt.pix.height);
	dprintk(1, "pixfmt=0x%x\n", fmt->fmt.pix.pixelformat);

	return 0;
}

/*
 * for mplane
 * cmd ID:VIDIOC_G_FMT
 */
static int vdin_vidioc_g_fmt_vid_cap_mplane(struct file *file,
					    void *priv,
					    struct v4l2_format *f)
{
	struct vdin_dev_s *devp = video_drvdata(file);

	if (IS_ERR_OR_NULL(devp))
		return -EFAULT;

	/* for test set a default value
	 * mult-planes mode
	 */
//	devp->v4l2_fmt.fmt.pix_mp.width = 1920;
//	devp->v4l2_fmt.fmt.pix_mp.height = 1080;
//	devp->v4l2_fmt.fmt.pix_mp.quantization = V4L2_QUANTIZATION_DEFAULT;
//	devp->v4l2_fmt.fmt.pix_mp.ycbcr_enc = V4L2_YCBCR_ENC_709;
//	devp->v4l2_fmt.fmt.pix_mp.field = V4L2_FIELD_ANY;
//	devp->v4l2_fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_YUYV;
//	devp->v4l2_fmt.fmt.pix_mp.num_planes = VDIN_NUM_PLANES;

	f->fmt.pix_mp.width = devp->v4l2_fmt.fmt.pix_mp.width;
	f->fmt.pix_mp.height = devp->v4l2_fmt.fmt.pix_mp.height;
	f->fmt.pix_mp.quantization = devp->v4l2_fmt.fmt.pix_mp.quantization;
	f->fmt.pix_mp.ycbcr_enc = devp->v4l2_fmt.fmt.pix_mp.ycbcr_enc;
	f->fmt.pix_mp.field = devp->v4l2_fmt.fmt.pix_mp.field;
	f->fmt.pix_mp.pixelformat = devp->v4l2_fmt.fmt.pix_mp.pixelformat;
	f->fmt.pix_mp.num_planes = devp->v4l2_fmt.fmt.pix_mp.num_planes;
	dprintk(1, "%s:wxh:%dx%d,quant:%d,enc:%d,field:%d,pixelformat:0x%x num_planes=%d\n",
		__func__,
		f->fmt.pix_mp.width, f->fmt.pix_mp.height,
		f->fmt.pix_mp.quantization, f->fmt.pix_mp.ycbcr_enc,
		f->fmt.pix_mp.field, f->fmt.pix_mp.pixelformat,
		f->fmt.pix_mp.num_planes);
	return 0;
}

/*
 * for mplane
 * cmd ID:VIDIOC_S_FMT
 */
static int vdin_vidioc_s_fmt_vid_cap_mplane(struct file *file,
					    void *priv,
					    struct v4l2_format *fmt)
{
	int i;
	struct vdin_dev_s *devp = video_drvdata(file);

	if (IS_ERR_OR_NULL(devp))
		return -EFAULT;

	dprintk(2, "%s:wxh:%dx%d,quant:%d,enc:%d,field:%d,pixelformat:0x%x num_planes=%d\n",
		__func__,
		fmt->fmt.pix_mp.width,	        fmt->fmt.pix_mp.height,
		fmt->fmt.pix_mp.quantization,	fmt->fmt.pix_mp.ycbcr_enc,
		fmt->fmt.pix_mp.field,		fmt->fmt.pix_mp.pixelformat,
		fmt->fmt.pix_mp.num_planes);

	devp->v4l2_fmt.fmt.pix_mp.width	       = fmt->fmt.pix_mp.width;
	devp->v4l2_fmt.fmt.pix_mp.height       = fmt->fmt.pix_mp.height;
	devp->v4l2_fmt.fmt.pix_mp.quantization = fmt->fmt.pix_mp.quantization;
	devp->v4l2_fmt.fmt.pix_mp.ycbcr_enc    = fmt->fmt.pix_mp.ycbcr_enc;
	devp->v4l2_fmt.fmt.pix_mp.field        = fmt->fmt.pix_mp.field;
	devp->v4l2_fmt.fmt.pix_mp.pixelformat  = fmt->fmt.pix_mp.pixelformat;
	devp->v4l2_fmt.fmt.pix_mp.num_planes   = fmt->fmt.pix_mp.num_planes;

	if (devp->v4l2_dbg_ctl.dbg_pix_fmt) {
		devp->v4l2_fmt.fmt.pix_mp.pixelformat = devp->v4l2_dbg_ctl.dbg_pix_fmt;
		dprintk(2, "%s:force pixelformat to 0x%x\n", __func__,
			devp->v4l2_fmt.fmt.pix_mp.pixelformat);
	}
	vdin_fill_pix_format(devp);
	for (i = 0; i < fmt->fmt.pix_mp.num_planes; i++) {
		fmt->fmt.pix_mp.plane_fmt[i].bytesperline =
			devp->v4l2_fmt.fmt.pix_mp.plane_fmt[i].bytesperline;
		fmt->fmt.pix_mp.plane_fmt[i].sizeimage =
			devp->v4l2_fmt.fmt.pix_mp.plane_fmt[i].sizeimage;
	}
	dprintk(2, "width=%d height=%d,quant:%d,enc:%x,\n",
		fmt->fmt.pix_mp.width, fmt->fmt.pix_mp.height,
		fmt->fmt.pix_mp.quantization, fmt->fmt.pix_mp.ycbcr_enc);
	dprintk(2, "field:%x,pixfmt=0x%x num_planes=0x%x\n", fmt->fmt.pix_mp.field,
		fmt->fmt.pix_mp.pixelformat, fmt->fmt.pix_mp.num_planes);

	return 0;
}

/* V4L2_CID_EXT_CAPTURE_DIVIDE_FRAMERATE */
static int vdin_vidioc_s_divide_fr(struct vdin_dev_s *devp,
			 struct v4l2_control *ctrl)
{
	if (ctrl->value < 0 || ctrl->value > 240) {
		dprintk(0, "%s divide value=%d,over range\n",
			__func__, ctrl->value);
		return -EINVAL;
	}
	devp->vdin_v4l2.divide = ctrl->value;
	dprintk(2, "%s set divide value to %d\n",
		__func__, devp->vdin_v4l2.divide);

	return 0;
}

/* V4L2_CID_EXT_CAPTURE_DONE_USER_PROCESSING */
static int vdin_vidioc_s_done_user_process(struct vdin_dev_s *devp,
			 struct v4l2_control *ctrl)
{
	if (ctrl->value < 0 || ctrl->value >= devp->vfp->size) {
		dprintk(0, "%s divide value=%d,over range\n",
			__func__, ctrl->value);
		return -EINVAL;
	}
	/* TODO */

	return -ENOTTY;
}

/* AML_V4L2_SET_DRM_MODE */
static int vdin_vidioc_s_drm_mode(struct vdin_dev_s *devp,
			 struct v4l2_control *ctrl)
{
	if (ctrl->value < 0) {
		dprintk(0, "%s divide value=%d,over range\n",
			__func__, ctrl->value);
		return -EINVAL;
	}

	devp->vdin_v4l2.secure_flg = !!ctrl->value;
	dprintk(0, "%s vdin%d,secure flag:%d\n",
		__func__, devp->index, devp->vdin_v4l2.secure_flg);

	return 0;
}

static int vdin_vidioc_s_ctrl(struct file *file, void *priv,
			 struct v4l2_control *ctrl)
{
	int ret = 0;
	struct vdin_dev_s *devp = video_drvdata(file);

	if (ctrl->id == V4L2_CID_EXT_CAPTURE_DIVIDE_FRAMERATE)
		ret = vdin_vidioc_s_divide_fr(devp, ctrl);
	else if (ctrl->id == V4L2_CID_EXT_CAPTURE_DONE_USER_PROCESSING)
		ret = vdin_vidioc_s_done_user_process(devp, ctrl);
	else if (ctrl->id == AML_V4L2_SET_DRM_MODE)
		ret = vdin_vidioc_s_drm_mode(devp, ctrl);

	return ret;
}

/* V4L2_CID_EXT_CAPTURE_DIVIDE_FRAMERATE */
static int vdin_vidioc_g_divide_fr(struct vdin_dev_s *devp,
			 struct v4l2_control *ctrl)
{
	ctrl->value = devp->vdin_v4l2.divide;
	dprintk(2, "%s get divide value %d\n",
		__func__, ctrl->value);

	return 0;
}

/* V4L2_CID_EXT_CAPTURE_OUTPUT_FRAMERATE */
static int vdin_vidioc_g_output_fr(struct vdin_dev_s *devp,
			 struct v4l2_control *ctrl)
{
	ctrl->value = devp->prop.fps;
	dprintk(2, "%s get fps value %d\n",
		__func__, ctrl->value);

	return 0;
}

static int vdin_vidioc_g_ctrl(struct file *file, void *priv,
			 struct v4l2_control *ctrl)
{
	int ret = 0;
	struct vdin_dev_s *devp = video_drvdata(file);

	if (ctrl->id == V4L2_CID_EXT_CAPTURE_DIVIDE_FRAMERATE)
		ret = vdin_vidioc_g_divide_fr(devp, ctrl);
	else if (ctrl->id == V4L2_CID_EXT_CAPTURE_OUTPUT_FRAMERATE)
		ret = vdin_vidioc_g_output_fr(devp, ctrl);

	return 0;
}

/* V4L2_CID_EXT_CAPTURE_CAPABILITY_INFO */
static int vdin_vidioc_g_cid_cap_info(struct vdin_dev_s *devp,
	struct v4l2_ext_control *control)
{
	if (control->size == sizeof(struct v4l2_ext_capture_capability_info)) {
		if (copy_to_user(control->ptr, &devp->ext_cap_cap_info,
				sizeof(struct v4l2_ext_capture_capability_info)))
			return -EFAULT;
	} else {
		dprintk(0, "%s,vdin%d,invalid args\n", __func__, devp->index);
		return -EINVAL;
	}

	return 0;
}

/* V4L2_CID_EXT_CAPTURE_PLANE_INFO */
static int vdin_vidioc_g_cid_plane_info(struct vdin_dev_s *devp,
	struct v4l2_ext_control *control)
{
	if (control->size == sizeof(struct v4l2_ext_capture_plane_info)) {
		if (copy_to_user(control->ptr, &devp->ext_cap_plane_info,
				sizeof(struct v4l2_ext_capture_plane_info)))
			return -EFAULT;
	} else {
		dprintk(0, "%s,LINE:%d,invalid args\n", __func__, devp->index);
		return -EINVAL;
	}

	return 0;
}

/* V4L2_CID_EXT_CAPTURE_VIDEO_WIN_INFO */
static int vdin_vidioc_g_cid_video_win_info(struct vdin_dev_s *devp,
	struct v4l2_ext_control *control)
{
	if (control->size == sizeof(struct v4l2_ext_capture_video_win_info)) {
		if (copy_to_user(control->ptr, &devp->ext_cap_video_win_info,
				sizeof(struct v4l2_ext_capture_video_win_info)))
			return -EFAULT;
	} else {
		dprintk(0, "%s,vdin%d,invalid args\n", __func__, devp->index);
		return -EINVAL;
	}

	return 0;
}

/* V4L2_CID_EXT_CAPTURE_FREEZE_MODE */
static int vdin_vidioc_g_cid_freeze_mode(struct vdin_dev_s *devp,
	struct v4l2_ext_control *control)
{
	if (control->size == sizeof(struct v4l2_ext_capture_freeze_mode)) {
		if (copy_to_user(control->ptr, &devp->ext_cap_freezee_mode,
				sizeof(struct v4l2_ext_capture_freeze_mode)))
			return -EFAULT;
	} else {
		dprintk(0, "%s,vdin%d,invalid args\n", __func__, devp->index);
		return -EINVAL;
	}

	return 0;
}

/* V4L2_CID_EXT_CAPTURE_PHYSICAL_MEMORY_INFO */
static int vdin_vidioc_g_cid_phy_mem_info(struct vdin_dev_s *devp,
	struct v4l2_ext_control *control)
{
	int ret = 0;
	unsigned int chroma_size = 0;
	struct v4l2_ext_capture_physical_memory_info info;

	if (control->size == sizeof(struct v4l2_ext_capture_physical_memory_info)) {
		if (copy_from_user(&info, control->ptr,
				sizeof(struct v4l2_ext_capture_physical_memory_info)))
			ret = -EFAULT;
		if (info.buf_index < 0 || info.buf_index >= devp->vfp->size) {
			dprintk(0, "%s,LINE:%d,buf_index:%d,over range\n",
				__func__, __LINE__, info.buf_index);
			return -EINVAL;
		}

		switch (devp->format_convert) {
		case VDIN_FORMAT_CONVERT_YUV_NV12:
		case VDIN_FORMAT_CONVERT_YUV_NV21:
		case VDIN_FORMAT_CONVERT_RGB_NV12:
		case VDIN_FORMAT_CONVERT_RGB_NV21:
			chroma_size = devp->canvas_w * devp->canvas_h / 2;
			break;
		default:
			break;
		}

		info.compat_y_data = (unsigned int)devp->vf_mem_start[info.buf_index];
		info.compat_c_data = info.compat_y_data + chroma_size;
		info.buf_location = V4L2_EXT_CAPTURE_INPUT_BUF;

		dprintk(1, "%s,index:%d,y:%#x,c:%#x,buf_loc:%d\n", __func__,
			info.buf_index, info.compat_y_data, info.compat_c_data, info.buf_location);
		if (copy_to_user(control->ptr, &info,
				sizeof(struct v4l2_ext_capture_physical_memory_info)))
			ret = -EFAULT;
	} else {
		dprintk(0, "%s,LINE:%d,vdin%d,invalid args\n", __func__, __LINE__, devp->index);
	}

	return 0;
}

/* V4L2_CID_EXT_CAPTURE_PLANE_PROP */
static int vdin_vidioc_g_cid_plane_prop(struct vdin_dev_s *devp,
	struct v4l2_ext_control *control)
{
	if (control->size == sizeof(struct v4l2_ext_capture_plane_prop)) {
		if (copy_to_user(control->ptr, &devp->ext_cap_plane_prop,
				sizeof(struct v4l2_ext_capture_plane_prop)))
			return -EFAULT;
	} else {
		dprintk(0, "%s,vdin%d,invalid args\n", __func__, devp->index);
		return -EINVAL;
	}

	return 0;
}

static int vdin_vidioc_g_ext_ctrls(struct file *file, void *fh,
			  struct v4l2_ext_controls *a)
{
	int i, ret = 0;
	struct v4l2_ext_control *ctrl = a->controls;
	struct vdin_dev_s *devp = video_drvdata(file);

	/* all controls in the control array must belong
	 * to the same control class
	 */

	for (i = 0; i < a->count; ctrl++, i++) {
		/* check control valid */
		switch (ctrl->id) {
		case V4L2_CID_EXT_CAPTURE_CAPABILITY_INFO:
			ret = vdin_vidioc_g_cid_cap_info(devp, ctrl);
			break;
		case V4L2_CID_EXT_CAPTURE_PLANE_INFO:
			ret = vdin_vidioc_g_cid_plane_info(devp, ctrl);
			break;
		case V4L2_CID_EXT_CAPTURE_VIDEO_WIN_INFO:
			ret = vdin_vidioc_g_cid_video_win_info(devp, ctrl);
			break;
		case V4L2_CID_EXT_CAPTURE_FREEZE_MODE:
			ret = vdin_vidioc_g_cid_freeze_mode(devp, ctrl);
			break;
		case V4L2_CID_EXT_CAPTURE_PHYSICAL_MEMORY_INFO:
			ret = vdin_vidioc_g_cid_phy_mem_info(devp, ctrl);
			break;
		case V4L2_CID_EXT_CAPTURE_PLANE_PROP:
			ret = vdin_vidioc_g_cid_plane_prop(devp, ctrl);
			break;
		default:
			break;
		}
		if (ret)
			break;
	}

	dprintk(1, "%s,vdin%d exit\n", __func__, devp->index);

	return ret;
}

/* V4L2_CID_EXT_CAPTURE_PLANE_PROP */
static int vdin_vidioc_s_cid_plane_prop(struct vdin_dev_s *devp,
	struct v4l2_ext_control *control)
{
	struct v4l2_ext_capture_plane_prop prop;
	enum tvin_port_e v4l2_port = TVIN_PORT_VIU1_WB0_VPP;

	if (control->size == sizeof(struct v4l2_ext_capture_plane_prop)) {
		if (copy_from_user(&prop, control->ptr,
				sizeof(struct v4l2_ext_capture_plane_prop)))
			return -EFAULT;

		if (prop.l <= V4L2_EXT_CAPTURE_SCALER_INPUT ||
			prop.l > V4L2_EXT_CAPTURE_OSD_OUTPUT)
			return -ENOTTY;
		devp->vdin_v4l2.l = prop.l;

		switch (devp->vdin_v4l2.l) {
		case V4L2_EXT_CAPTURE_SCALER_OUTPUT:
			v4l2_port = TVIN_PORT_VIU1_WB0_VD1;
			break;
		case V4L2_EXT_CAPTURE_DISPLAY_OUTPUT:
			/* no match loopback point */
			v4l2_port = TVIN_PORT_VIU1_WB0_VD1;
			break;
		case V4L2_EXT_CAPTURE_BLENDED_OUTPUT:
			v4l2_port = TVIN_PORT_VIU1_WB0_VPP;
			break;
		case V4L2_EXT_CAPTURE_OSD_OUTPUT:
			v4l2_port = TVIN_PORT_VIU1_WB0_OSD1;
			break;
		default:
			break;
		}
		mutex_lock(&devp->fe_lock);
		devp->parm.index	= devp->index;
		devp->parm.port		= v4l2_port;
		devp->v4l2_port_cur = v4l2_port;
		devp->unstable_flag = false;
		mutex_unlock(&devp->fe_lock);

		dprintk(1, "%s,LINE:%d,location:%#x,cur:%d(%s)\n", __func__, __LINE__,
			devp->vdin_v4l2.l, devp->v4l2_port_cur, tvin_port_str(devp->v4l2_port_cur));
	} else {
		dprintk(0, "%s,LINE:%d,invalid args\n", __func__, __LINE__);
		return -EINVAL;
	}

	return 0;
}

/* V4L2_CID_EXT_CAPTURE_FREEZE_MODE */
static int vdin_vidioc_s_cid_freeze_mode(struct vdin_dev_s *devp,
	struct v4l2_ext_control *control)
{
	struct v4l2_ext_capture_freeze_mode mode;

	if (copy_from_user(&mode, control->ptr, sizeof(struct v4l2_ext_capture_freeze_mode)))
		return -EFAULT;
	/* check args valid */
	//freeze one frame or all capture frames ?
	devp->ext_cap_freezee_mode.plane_index = mode.plane_index;
	devp->ext_cap_freezee_mode.val = mode.val;
	//devp->pause_dec = control->value;
	dprintk(1, "%s,plane_index:%d,mode.val:%#x\n", __func__,
		mode.plane_index, mode.val);

	return 0;
}

/* V4L2_CID_EXT_CAPTURE_VIDEO_WIN_INFO */
static int vdin_vidioc_s_cid_video_win_info(struct vdin_dev_s *devp,
	struct v4l2_ext_control *control)
{
	/* LG chip is only supported */
	return -ENOTTY;
}

static int vdin_vidioc_s_ext_ctrls(struct file *file, void *fh,
			  struct v4l2_ext_controls *a)
{
	int i;
	struct v4l2_ext_control *ctrl = a->controls;
	struct vdin_dev_s *devp = video_drvdata(file);

	/* all controls in the control array must belong
	 * to the same control class
	 */

	for (i = 0; i < a->count; ctrl++, i++) {
		switch (ctrl->id) {
		case V4L2_CID_EXT_CAPTURE_PLANE_PROP:
			vdin_vidioc_s_cid_plane_prop(devp, ctrl);
			break;
		case V4L2_CID_EXT_CAPTURE_FREEZE_MODE:
			vdin_vidioc_s_cid_freeze_mode(devp, ctrl);
			break;
		case V4L2_CID_EXT_CAPTURE_VIDEO_WIN_INFO:
			vdin_vidioc_s_cid_video_win_info(devp, ctrl);
			break;
		default:
			break;
		}
	}

	return 0;
}

static int vdin_vidioc_try_ext_ctrls(struct file *file, void *fh,
				    struct v4l2_ext_controls *a)
{
	/* struct vdin_dev_s *devp = video_drvdata(file); */
	return 0;
}

static int vdin_vidioc_g_input(struct file *file, void *priv, unsigned int *i)
{
	int idx;
	struct vdin_dev_s *devp = video_drvdata(file);

	for (idx = 0; idx < devp->v4l2_port_num; idx++) {
		if (devp->v4l2_port_cur == devp->v4l2_port[idx]) {
			*i = idx;
			break;
		}
	}

	return 0;
}

/* set input and open vdin fe */
static int vdin_vidioc_s_input(struct file *file, void *priv, unsigned int i)
{
	int ret;
	struct vdin_dev_s *devp = video_drvdata(file);

	if (i >= devp->v4l2_port_num) {
		dprintk(0, "%s  index(%d) is out of bounds.v4l2_port_num=%d\n",
			__func__, i, devp->v4l2_port_num);
		return -EINVAL;
	}

	mutex_lock(&devp->fe_lock);

	if (devp->flags & VDIN_FLAG_DEC_OPENED &&
		devp->v4l2_port_cur != devp->v4l2_port[i]) {
		dprintk(0, "%s current port:%d is opened already,close it\n",
			__func__, devp->v4l2_port_cur);
		vdin_close_fe(devp);
	}

	devp->parm.index    = devp->index;
	devp->parm.port     = devp->v4l2_port[i];
	devp->v4l2_port_cur = devp->v4l2_port[i];
	devp->unstable_flag = false;

	devp->work_mode = VDIN_WORK_MD_V4L;
	devp->afbce_flag = 0;

	if (devp->index == 0 && !(devp->flags & VDIN_FLAG_DEC_OPENED)) {
		ret = vdin_open_fe(devp->parm.port, 0, devp);
		if (ret) {
			pr_err("TVIN_IOC_OPEN(%d) failed to open port 0x%x\n",
				   devp->index, devp->parm.port);
			mutex_unlock(&devp->fe_lock);
			return -EFAULT;
		}
	}
	mutex_unlock(&devp->fe_lock);

	pr_info("%s current port:%#x(%s)\n", __func__,
		devp->v4l2_port_cur, tvin_port_str(devp->v4l2_port_cur));
	return 0;
}

static int vdin_vidioc_enum_fmt_vid_cap(struct file *file, void *priv,
				   struct v4l2_fmtdesc *f)
{
	struct vdin_v4l2_pix_fmt *fmt;

	if (f->index >= ARRAY_SIZE(pix_formats))
		return -EINVAL;

	fmt = &pix_formats[f->index];
	/* description will be filled by v4l_fill_fmtdesc */
	f->pixelformat = fmt->fourcc;

	dprintk(0, "%s index:%d pixelformat:%x\n",
		__func__, f->index, f->pixelformat);

	return 0;
}

static int vidioc_try_fmt_vid_cap_mplane(struct file *file, void *priv,
				  struct v4l2_format *f)
{
	int i = 0;
	struct vdin_dev_s *devp = video_drvdata(file);

	dprintk(1, "%s width[%d] height[%d] num_planes[%ds]\n", __func__,
		f->fmt.pix_mp.width, f->fmt.pix_mp.height, f->fmt.pix_mp.num_planes);

	if (f->type != V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		dprintk(0, "%s vdin%d v4l2 do not support type=%d\n",
			__func__, devp->index, f->type);
		return -EINVAL;
	}
	if (f->fmt.pix_mp.width > 4096 || f->fmt.pix_mp.height > 2160) {
		dprintk(0, "%s vdin%d v4l2 do not support w=%d,h=%d\n",
			__func__, devp->index, f->fmt.pix_mp.width, f->fmt.pix_mp.height);
		return -EINVAL;
	}
	if (f->fmt.pix_mp.num_planes > 2) {
		dprintk(0, "%s vdin%d v4l2 do not support num_planes=%d\n",
			__func__, devp->index, f->fmt.pix_mp.num_planes);
		return -EINVAL;
	}
	for (i = 0; i < ARRAY_SIZE(pix_formats); i++) {
		if (f->fmt.pix_mp.pixelformat == pix_formats[i].fourcc)
			break;
	}
	if (i >= ARRAY_SIZE(pix_formats)) {
		dprintk(0, "%s vdin%d v4l2 do not support pixelformat=%#x\n",
			__func__, devp->index, f->fmt.pix_mp.pixelformat);
		return -EINVAL;
	}

	return 0;
}

/* This is an experimental interface */
static int vdin_vidioc_enum_framesizes(struct file *file, void *fh,
				  struct v4l2_frmsizeenum *fsize)
{
	int ret = 0, i = 0;
	struct vdin_v4l2_pix_fmt *fmt = NULL;
	struct v4l2_frmsize_discrete *frmsize = NULL;

	for (i = 0; i < ARRAY_SIZE(pix_formats); i++) {
		if (pix_formats[i].fourcc == fsize->pixel_format) {
			fmt = &pix_formats[i];
			break;
		}
	}
	if (!fmt)
		return -EINVAL;
	if (fsize->index >= ARRAY_SIZE(vdin_v4l2_frmsize_dis))
		return -EINVAL;

	frmsize = &vdin_v4l2_frmsize_dis[fsize->index];
	/* TODO:vdin can only scale down
	 * width & height should less than real format size.
	 */
	fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
	fsize->discrete.width  = frmsize->width;
	fsize->discrete.height = frmsize->height;

	return ret;
}

static int vdin_vidioc_enum_frameintervals(struct file *file, void *priv,
				      struct v4l2_frmivalenum *fival)
{
	if (fival->index >= ARRAY_SIZE(fract_discrete))
		return -EINVAL;

	fival->type = V4L2_FRMIVAL_TYPE_DISCRETE;
	fival->discrete.numerator   = fract_discrete[fival->index].numerator;
	fival->discrete.denominator = fract_discrete[fival->index].denominator;

	return 0;
}

static int vdin_vidioc_g_parm(struct file *file, void *priv,
			 struct v4l2_streamparm *parms)
{
	return 0;
}

static int vdin_vidioc_s_parm(struct file *file, void *priv,
			 struct v4l2_streamparm *parms)
{
	return 0;
}

static const struct v4l2_ioctl_ops vdin_v4l2_ioctl_ops = {
	.vidioc_querystd	= vdin_vidioc_querystd,
	.vidioc_enum_input	= vdin_vidioc_enum_input,
	.vidioc_s_input		= vdin_vidioc_s_input,
	.vidioc_g_input		= vdin_vidioc_g_input,
	.vidioc_enum_fmt_vid_cap	= vdin_vidioc_enum_fmt_vid_cap,
	.vidioc_try_fmt_vid_cap_mplane = vidioc_try_fmt_vid_cap_mplane,
	.vidioc_querycap			= vdin_vidioc_querycap,
	.vidioc_enum_framesizes		= vdin_vidioc_enum_framesizes,
	.vidioc_enum_frameintervals = vdin_vidioc_enum_frameintervals,

	/* Stream type-dependent parameter ioctls */
	.vidioc_g_parm = vdin_vidioc_g_parm,
	.vidioc_s_parm = vdin_vidioc_s_parm,

	/*queue io control*/
	.vidioc_reqbufs = vdin_vidioc_reqbufs,
	.vidioc_create_bufs = vdin_vidioc_create_bufs,
	.vidioc_querybuf = vdin_vidioc_querybuf,
	.vidioc_qbuf = vdin_vidioc_qbuf,
	.vidioc_dqbuf = vdin_vidioc_dqbuf,
	.vidioc_expbuf = vdin_vidioc_expbuf,
	.vidioc_streamon = vdin_vidioc_streamon,
	.vidioc_streamoff = vdin_vidioc_streamoff,

	.vidioc_g_ctrl = vdin_vidioc_g_ctrl,
	.vidioc_s_ctrl = vdin_vidioc_s_ctrl,

	.vidioc_g_ext_ctrls		= vdin_vidioc_g_ext_ctrls,
	.vidioc_s_ext_ctrls		= vdin_vidioc_s_ext_ctrls,
	.vidioc_try_ext_ctrls	= vdin_vidioc_try_ext_ctrls,

	.vidioc_s_fmt_vid_cap_mplane	= vdin_vidioc_s_fmt_vid_cap_mplane,
	.vidioc_s_fmt_vid_cap			= vdin_vidioc_s_fmt_vid_cap,
	.vidioc_s_fmt_vid_out_mplane	= vdin_vidioc_s_fmt_vid_cap_mplane,
	.vidioc_s_fmt_vid_out			= vdin_vidioc_s_fmt_vid_cap,

	.vidioc_g_fmt_vid_cap_mplane	= vdin_vidioc_g_fmt_vid_cap_mplane,
	.vidioc_g_fmt_vid_cap			= vdin_vidioc_g_fmt_vid_cap,
	.vidioc_g_fmt_vid_out_mplane	= vdin_vidioc_g_fmt_vid_cap_mplane,
	.vidioc_g_fmt_vid_out			= vdin_vidioc_g_fmt_vid_cap,
};

static void vdin_vdev_release(struct video_device *vdev)
{
	dprintk(2, "%s\n", __func__);
}

static int vdin_v4l2_open(struct file *file)
{
	struct vdin_dev_s *devp = video_drvdata(file);

	if (IS_ERR_OR_NULL(devp))
		return -EFAULT;

	if (devp->flags & VDIN_FLAG_DEC_STARTED) {
		dprintk(0, "%s error VDIN_FLAG_DEC_STARTED\n", __func__);
		return -EPERM;
	}

	dprintk(0, "%s\n", __func__);
	/*dump_stack();*/
	devp->afbce_flag_backup = devp->afbce_flag;

	v4l2_fh_open(file);

	INIT_LIST_HEAD(&devp->buf_list);

	return 0;
}

static int vdin_v4l2_release(struct file *file)
{
	int ret = 0, i = 0;
	int plane_idx = 0;
	struct vdin_dev_s *devp = video_drvdata(file);
	struct vdin_set_canvas_addr_s *p_addr = NULL;

	dprintk(0, "%s\n", __func__);

	if (IS_ERR_OR_NULL(devp))
		return -EFAULT;

	if (devp->flags & VDIN_FLAG_DEC_STARTED) {
		dprintk(0, "%s error VDIN_FLAG_DEC_STARTED\n", __func__);
		return -EPERM;
	}

	/*release que*/
	ret = vb2_fop_release(file);

	if (devp->work_mode == VDIN_WORK_MD_V4L) {
		for (i = 0; i < VDIN_CANVAS_MAX_CNT; i++) {
			for (plane_idx = 0;
				plane_idx < devp->v4l2_fmt.fmt.pix_mp.num_planes;
				plane_idx++) {
				p_addr = &devp->st_vdin_set_canvas_addr[i][plane_idx];
				if (p_addr->dma_buffer == 0)
					break;
				dma_buf_unmap_attachment(p_addr->dmabuf_attach,
					p_addr->sg_table, DMA_BIDIRECTIONAL);
				dma_buf_detach(p_addr->dma_buffer, p_addr->dmabuf_attach);
				dma_buf_put(p_addr->dma_buffer);
			}
		}
		memset(devp->st_vdin_set_canvas_addr, 0, sizeof(devp->st_vdin_set_canvas_addr));

		devp->afbce_flag = devp->afbce_flag_backup;
	}
	devp->work_mode = VDIN_WORK_MD_NORMAL;

	return ret;
}

static unsigned int vdin_v4l2_poll(struct file *file,
				   struct poll_table_struct *wait)
{
	/*struct vdin_dev_s *devp = video_drvdata(file);*/
	int ret;

	ret = vb2_fop_poll(file, wait);

	return ret;
}

static int vdin_v4l2_mmap(struct file *file, struct vm_area_struct *va)
{
	dprintk(2, "%s\n", __func__);
	return vb2_fop_mmap(file, va);
}

static long vdin_v4l2_ioctl(struct file *file,
			    unsigned int cmd, unsigned long arg)
{
	/*struct vdin_dev_s *devp = video_drvdata(file);*/
	long ret = 0;

	ret = video_ioctl2(file, cmd, arg);

	return ret;
}

static ssize_t vdin_v4l2_read(struct file *fd, char __user *a,
			      size_t b, loff_t *c)
{
	dprintk(2, "%s\n", __func__);
	/*vb2_fop_read(fd, a, b, c);*/

	return 0;
}

static ssize_t vdin_v4l2_write(struct file *fd, const char __user *a,
			       size_t b, loff_t *c)
{
	dprintk(2, "%s\n", __func__);
	/*vb2_fop_write(fd, a, b, c);*/
	return 0;
}

static void vdin_return_all_buffers(struct vdin_dev_s *devp,
				    enum vb2_buffer_state state)
{
	struct vdin_vb_buff *buf, *node;
	unsigned long flags = 0;

	dprintk(2, "%s\n", __func__);
	spin_lock_irqsave(&devp->list_head_lock, flags);
	list_for_each_entry_safe(buf, node, &devp->buf_list, list) {
		dprintk(2, "%s idx:%d\n", __func__, buf->vb.vb2_buf.index);
		vb2_buffer_done(&buf->vb.vb2_buf, state);
		list_del(&buf->list);
	}
	INIT_LIST_HEAD(&devp->buf_list);
	spin_unlock_irqrestore(&devp->list_head_lock, flags);
}

/*
 * op queue_setup
 * called from VIDIOC_REQBUFS() and VIDIOC_CREATE_BUFS() handlers
 * before memory allocation,
 * need return the num_planes per buffer
 */
static int vdin_vb2ops_queue_setup(struct vb2_queue *vq,
				   unsigned int *num_buffers,
				   unsigned int *num_planes,
		       unsigned int sizes[], struct device *alloc_devs[])
{
	struct vdin_dev_s *devp = vb2_get_drv_priv(vq);
	unsigned int i = 0;

	/*
	 * NV12 every frame need two buffer
	 * one for Y, one for UV
	 * need return the num_planes per buffer
	 */
	*num_planes = devp->v4l2_fmt.fmt.pix_mp.num_planes;
	for (i = 0; i < *num_planes; i++) {
		sizes[i] = devp->v4l2_fmt.fmt.pix_mp.plane_fmt[i].sizeimage;
		dprintk(1, "plane %d, size %x\n", i, sizes[i]);
		//if (devp->index == 0)
			alloc_devs[i] = v4l_get_dev_from_codec_mm();/* codec_mm_cma area */
			//alloc_devs[i] = &devp->this_pdev->dev;/* vdin0_cma area */
			//alloc_devs[i] = &devp->dev;/* CMA reserved area */
		//else
			//alloc_devs[i] = &devp->this_pdev->dev;/* vdin1_cma area */
	}

	dprintk(1, "type: %d, plane: %d, buf cnt: %d, size: [Y: %x, C: %x]\n",
		vq->type, *num_planes, *num_buffers, sizes[0], sizes[1]);
	return 0;
}

/*
 * buf_prepare
 *
 */
static int vdin_vb2ops_buffer_prepare(struct vb2_buffer *vb)
{
	struct vdin_dev_s *devp = vb2_get_drv_priv(vb->vb2_queue);
	struct vb2_queue *queue = NULL;
	/*uint size = 0;*/
	/*unsigned int i;*/

	if (IS_ERR_OR_NULL(devp))
		return -EINVAL;

	queue = &devp->vb_queue;
	dprintk(3, "buf prepare idx:%d bufs:%d planes:%d queue_cnt:%d, buf sts:%s\n",
		vb->index, queue->num_buffers,
		vb->num_planes, queue->queued_count,
		vb2_buf_sts_to_str(vb->state));

	//if (vb->num_planes > 1) {
	//	for (i = 0; i < vb->num_planes; i++) {
	//		size = devp->v4l2_fmt.fmt.pix_mp.plane_fmt[i].sizeimage;
	//		if (vb2_plane_size(vb, i) < size) {
	//			dprintk(0, "buffer too small (%lu < %u)\n",
	//				vb2_plane_size(vb, i), size);
	//			return -EINVAL;
	//		}
	//	}
	//}

	/*vb2_set_plane_payload(vb, 0, size);*/
	return 0;
}

/*
 * buf_queue
 * passes buffer vb to the driver; driver may start hardware operation
 * on this buffer; driver should give the buffer back by calling
 * vb2_buffer_done() function; it is always called after calling
 * VIDIOC_STREAMON() ioctl; might be called before
 * start_streaming callback if user pre-queued buffers before calling
 * VIDIOC_STREAMON().
 */
static void vdin_vb2ops_buffer_queue(struct vb2_buffer *vb)
{
	struct vdin_dev_s *devp = vb2_get_drv_priv(vb->vb2_queue);
	struct vb2_v4l2_buffer *v4lbuf = to_vb2_v4l2_buffer(vb);
	struct vdin_vb_buff *buf = to_vdin_vb_buf(v4lbuf);
	unsigned long flags = 0;

	dprintk(3, "%s buf:%d, state:%s\n", __func__, vb->index,
		vb2_buf_sts_to_str(buf->vb.vb2_buf.state));

	spin_lock_irqsave(&devp->list_head_lock, flags);
	list_add_tail(&buf->list, &devp->buf_list);

	spin_unlock_irqrestore(&devp->list_head_lock, flags);
	/* TODO: Update any DMA pointers if necessary */
	dprintk(3, "num_buf:%d, queue_cnt:%d, after state:%s\n",
		devp->vb_queue.num_buffers,
		devp->vb_queue.queued_count,
		vb2_buf_sts_to_str(buf->vb.vb2_buf.state));
}

static int vdin_v4l2_start_streaming(struct vdin_dev_s *devp)
{
	return 0;
}

/*
 * start_streaming
 * Start streaming. First check if the minimum number of buffers have been
 * queued. If not, then return -ENOBUFS and the vb2 framework will call
 * this function again the next time a buffer has been queued until enough
 * buffers are available to actually start the DMA engine.
 */
static int vdin_vb2ops_start_streaming(struct vb2_queue *vq, unsigned int count)
{
	struct vdin_dev_s *devp = vb2_get_drv_priv(vq);
	struct list_head *buf_head = NULL;
	/*struct vb2_buffer *vb2_buf;*/
	int ret = 0;

	if (IS_ERR_OR_NULL(devp))
		return -EINVAL;

	buf_head = &devp->buf_list;
	if (list_empty(buf_head)) {
		dprintk(0, "buf_list is empty\n");
		return -EINVAL;
	}

	devp->frame_cnt = 0;

	ret = vdin_v4l2_start_streaming(devp);
	if (ret) {
		/*
		 * In case of an error, return all active buffers to the
		 * QUEUED state
		 */
		vdin_return_all_buffers(devp, VB2_BUF_STATE_QUEUED);
	}
	dprintk(2, "%s\n", __func__);
	return ret;
}

/*
 * stop_streaming
 * Stop the DMA engine. Any remaining buffers in the DMA queue are dequeued
 * and passed on to the vb2 framework marked as STATE_ERROR.
 */
static void vdin_vb2ops_stop_streaming(struct vb2_queue *vq)
{
	struct vdin_dev_s *devp = vb2_get_drv_priv(vq);

	dprintk(2, "%s\n", __func__);
	/* TODO: stop DMA */

	devp->vb_queue.streaming = false;

	/* Release all active buffers */
	vdin_return_all_buffers(devp, VB2_BUF_STATE_ERROR);
}

/*
 * buf_init
 * called once after allocating a buffer (in MMAP case) or after acquiring a
 * new USERPTR buffer; drivers may perform additional buffer-related
 * initialization; initialization failure (return != 0)
 * will prevent queue setup from completing successfully; optional.
 */
static int vdin_vb2ops_buf_init(struct vb2_buffer *vb)
{
	struct vb2_queue *vb2q = vb->vb2_queue;
	struct vb2_v4l2_buffer *vb_buf = NULL;
	struct vdin_vb_buff *vdin_buf = NULL;

	dprintk(1, "%s idx:%d, type:0x%x, memory:0x%x, num_planes:%d\n",
		__func__, vb->index, vb->type, vb->memory, vb->num_planes);
	dprintk(1, "vb2q type:0x%x num buffer:%d request id:%d\n", vb2q->type,
		vb2q->num_buffers, vb2q->queued_count);

	vb_buf = to_vb2_v4l2_buffer(vb);
	vdin_buf = to_vdin_vb_buf(vb_buf);

	/* for test add a tag*/
	vdin_buf->tag = 0xa5a6ff00 + vb->index;
	/* set a tag for test*/
	dprintk(2, "vb_buf:0x%p vdin_buf:0x%p tag:0x%x\n",
		vb_buf, vdin_buf, vdin_buf->tag);

	return 0;
}

/*
 * buf_finish
 * called before every dequeue of the buffer back to userspace; the
 * buffer is synced for the CPU, so drivers can access/modify the
 * buffer contents; drivers may perform any operations required
 * before userspace accesses the buffer; optional. The buffer state
 * can be one of the following: DONE and ERROR occur while
 * streaming is in progress, and the PREPARED state occurs when
 * the queue has been canceled and all pending buffers are being
 * returned to their default DEQUEUED state. Typically you only
 * have to do something if the state is VB2_BUF_STATE_DONE,
 * since in all other cases the buffer contents will be ignored anyway.
 */
static void vdin_vb2ops_buf_finish(struct vb2_buffer *vb)
{
	struct vdin_dev_s *devp = vb2_get_drv_priv(vb->vb2_queue);
	//struct vb2_queue *vb_que = NULL;

	if (IS_ERR_OR_NULL(devp))
		return;
	//vb_que = &devp->vb_queue;

	/*dprintk("%s idx:%d, queue cnt:%d, buf:%s\n", __func__, vb->index,*/
	/*	  vb_que->queued_count, vb2_buf_sts_to_str(vb->state));*/
}

/*
 * The vb2 queue ops. Note that since q->lock is set we can use the standard
 * vb2_ops_wait_prepare/finish helper functions. If q->lock would be NULL,
 * then this driver would have to provide these ops.
 */
static struct vb2_ops vdin_vb2ops = {
	.queue_setup		= vdin_vb2ops_queue_setup,
	.buf_prepare		= vdin_vb2ops_buffer_prepare,
	.buf_queue			= vdin_vb2ops_buffer_queue,

	.buf_init			= vdin_vb2ops_buf_init,
	.buf_finish			= vdin_vb2ops_buf_finish,

	.start_streaming	= vdin_vb2ops_start_streaming,
	.stop_streaming		= vdin_vb2ops_stop_streaming,

	.wait_prepare		= vb2_ops_wait_prepare,
	.wait_finish		= vb2_ops_wait_finish,
};

static const struct v4l2_file_operations vdin_v4l2_fops = {
	.owner = THIS_MODULE,
	.open = vdin_v4l2_open, /*open files */
	.read = vdin_v4l2_read,
	.write = vdin_v4l2_write,
	.release = vdin_v4l2_release, /*release files resource*/
	.poll = vdin_v4l2_poll, /*files poll interface*/
	.mmap = vdin_v4l2_mmap, /*files memory mmap*/
	.unlocked_ioctl = vdin_v4l2_ioctl, /*io control op interface*/
};

static int vdin_v4l2_queue_init(struct vdin_dev_s *devp,
				struct vb2_queue *que)
{
	int ret;

	/*unsigned int ret = 0;*/
	que->type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	/*que->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;*/
	que->io_modes = VB2_MMAP | VB2_DMABUF;
	/*devp->dev*/
	//que->dev = &devp->vdev.dev;
	que->dev = &devp->this_pdev->dev;
	que->drv_priv = devp;
	que->buf_struct_size = sizeof(struct vdin_vb_buff);
	que->ops = &vdin_vb2ops;

	/*que->mem_ops = &vdin_vb2_dma_contig_memops;*/
	que->mem_ops = &vb2_dma_contig_memops;
	/*que->mem_ops = &vb2_vmalloc_memops;*/
	/*que->mem_ops = &vb2_dma_sg_memops;*/

	que->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	/*
	 * Assume that this DMA engine needs to have at least two buffers
	 * available before it can be started. The start_streaming() op
	 * won't be called until at least this many buffers are queued up.
	 */
	que->min_buffers_needed = 3;
	/*
	 * The serialization lock for the streaming ioctls. This is the same
	 * as the main serialization lock, but if some of the non-streaming
	 * ioctls could take a long time to execute, then you might want to
	 * have a different lock here to prevent VIDIOC_DQBUF from being
	 * blocked while waiting for another action to finish. This is
	 * generally not needed for PCI devices, but USB devices usually do
	 * want a separate lock here.
	 */
	que->lock = &devp->lock;
	/*
	 * Since this driver can only do 32-bit DMA we must make sure that
	 * the vb2 core will allocate the buffers in 32-bit DMA memory.
	 */
	que->gfp_flags = GFP_DMA32;

	ret = vb2_queue_init(que);
	if (ret < 0)
		dprintk(0, "vb2_queue_init fail\n");

	dprintk(1, "is_multiplanar:%d\n", que->is_multiplanar);

	return ret;
}

/*
 * vdin v4l2 video device register and device node create
 */
int vdin_v4l2_probe(struct platform_device *pl_dev,
		    struct vdin_dev_s *devp)
{
	struct video_device *video_dev = NULL;
	//struct vb2_queue *queue = NULL;
	int ret = 0;
	int v4l_vd_num;

	if (IS_ERR_OR_NULL(devp)) {
		ret = -ENODEV;
		dprintk(0, "[vdin] devp err\n");
		return ret;
	}

	devp->dts_config.v4l_en = of_property_read_bool(pl_dev->dev.of_node,
						"v4l_support_en");
	if (devp->dts_config.v4l_en == 0) {
		dprintk(0, "vdin[%d] %s v4l2 disabled!\n",
			devp->index, __func__);
		return -1;
	}
	/*dprintk(1, "%s vdin[%d] start\n", __func__, devp->index);*/
	snprintf(devp->v4l2_dev.name, sizeof(devp->v4l2_dev.name),
		 "%s", VDIN_V4L_DV_NAME);

	/*video device initial*/
	video_dev = &devp->video_dev;
	video_dev->v4l2_dev = &devp->v4l2_dev;
	/*dprintk(1, "video_device addr:0x%p\n", video_dev);*/
	/* Initialize the top-level structure */
	ret = v4l2_device_register(&pl_dev->dev, &devp->v4l2_dev);
	if (ret) {
		dprintk(0, "v4l2 dev register fail\n");
		ret = -ENODEV;
		goto v4l2_device_register_fail;
	}

	/* Initialize the vb2 queue */
	//queue = &devp->vb_queue;
	ret = vdin_v4l2_queue_init(devp, &devp->vb_queue);
	if (ret)
		goto video_register_device_fail;

	INIT_LIST_HEAD(&devp->buf_list);
	mutex_init(&devp->lock);
	spin_lock_init(&devp->list_head_lock);

	strlcpy(video_dev->name, VDIN_V4L_DV_NAME, sizeof(video_dev->name));
	video_dev->fops = &vdin_v4l2_fops,
	video_dev->ioctl_ops = &vdin_v4l2_ioctl_ops,
	video_dev->release = vdin_vdev_release;
	video_dev->lock = &devp->lock;
	video_dev->queue = &devp->vb_queue;
	video_dev->v4l2_dev = &devp->v4l2_dev;/*v4l2_device_register*/
	video_dev->device_caps = VDIN_DEVICE_CAPS;

	video_dev->vfl_type = VFL_TYPE_GRABBER;
	video_dev->vfl_dir   = VFL_DIR_RX;
	video_dev->dev_debug = (V4L2_DEV_DEBUG_IOCTL | V4L2_DEV_DEBUG_IOCTL_ARG);

	/*
	 * init the device node name, v4l2 will create
	 * a video device. the name is:videoXX (VDIN_V4L_DV_NAME)
	 * otherwise will be a videoXX, XX is a number
	 */
	/*video_dev->dev.init_name = VDIN_V4L_DV_NAME;*/
	video_set_drvdata(video_dev, devp);

	ret = of_property_read_u32(pl_dev->dev.of_node, "v4l_vd_num",
			&v4l_vd_num);
	dprintk(0, "vdin%d,ret = %d,v4l_vd_num=%d\n",
		devp->index, ret, v4l_vd_num);
	if (ret)
		v4l_vd_num = VDIN_VD_NUMBER + (devp->index);

	ret = video_register_device(video_dev, VFL_TYPE_GRABBER,
			v4l_vd_num);
	if (ret) {
		dprintk(0, "register dev fail.\n");
		goto video_register_device_fail;
	}

	pl_dev->dev.dma_mask = &pl_dev->dev.coherent_dma_mask;
	if (dma_set_coherent_mask(&pl_dev->dev, 0xffffffff) < 0)
		dprintk(0, "dev set_coherent_mask fail\n");

	if (dma_set_mask(&pl_dev->dev, 0xffffffff) < 0)
		dprintk(0, "set dma mask fail\n");

	video_dev->dev.dma_mask = &video_dev->dev.coherent_dma_mask;
	if (dma_set_coherent_mask(&video_dev->dev, 0xffffffff) < 0)
		dprintk(0, "dev set_coherent_mask fail\n");

	if (dma_set_mask(&video_dev->dev, 0xffffffff) < 0)
		dprintk(0, "set dma mask fail\n");

	/*device node need attach device tree vdin dts tree*/
	video_dev->dev.of_node = pl_dev->dev.of_node;
	ret = of_reserved_mem_device_init(&video_dev->dev);
	if (ret == 0)
		dprintk(0, "rev memory resource ok\n");
	else
		dprintk(0, "rev memory resource undefined!!!\n");

	vdin_v4l2_init(devp, pl_dev);

	dprintk(0, "dev registered as %s\n",
		video_device_node_name(video_dev));
	dprintk(0, "vdin[%d] %s ok\n", devp->index, __func__);

	return 0;

video_register_device_fail:
	v4l2_device_unregister(&devp->v4l2_dev);

v4l2_device_register_fail:
	return ret;
}

/* Check loopback source format */
int vdin_v4l2_loopback_fmt(struct vdin_dev_s *devp)
{
	int h, v;
	int err_range = 10;
	const struct vinfo_s *vinfo;

	dprintk(1, "%s,fmt:%s,cfmt:%s,status:%#x,fps:%d\n",
		__func__, tvin_sig_fmt_str(devp->parm.info.fmt),
		tvin_color_fmt_str(devp->parm.info.cfmt),
		devp->parm.info.status, devp->parm.info.fps);
#ifdef VDIN_V4L2_GET_LOOPBACK_FMT_BY_VOUT_SERVE
	vinfo = get_current_vinfo();
	h = vinfo->width;
	v = vinfo->height;
	dprintk(1, "%s,%d; %dx%d,vmode:%d\n", __func__, __LINE__,
		vinfo->width, vinfo->height, vinfo->mode);
#else
	h = vdin_get_active_h(devp->addr_offset);
	v = vdin_get_active_v(devp->addr_offset);
	dprintk(1, "%s,%d; %dx%d\n", __func__, __LINE__, vinfo->width, vinfo->height);
#endif
	if (h >= 4096 - err_range && h <= 4096 + err_range &&
		v >= 2160 - err_range && v <= 2160 + err_range)
		devp->parm.info.fmt = TVIN_SIG_FMT_HDMI_4096_2160_00HZ;
	else if (h >= 3840 - err_range && h <= 3840 + err_range &&
			 v >= 2160 - err_range && v <= 2160 + err_range)
		devp->parm.info.fmt = TVIN_SIG_FMT_HDMI_3840_2160_00HZ;
	else if (h >= 1920 - err_range && h <= 1920 + err_range &&
			 v >= 1080 - err_range && v <= 1080 + err_range)
		devp->parm.info.fmt = TVIN_SIG_FMT_HDMI_1920X1080P_60HZ;
	else if (h >= 1280 - err_range && h <= 1280 + err_range &&
			 v >= 720 - err_range && v <= 720 + err_range)
		devp->parm.info.fmt = TVIN_SIG_FMT_HDMI_1280X720P_60HZ;
	else
		devp->parm.info.fmt = TVIN_SIG_FMT_NULL;

	dprintk(1, "vdin%d,active,h=%d,v=%d\n", devp->index, h, v);

	return 0;
}

/* start vdin dec for v4l2 */
int vdin_v4l2_start_tvin(struct vdin_dev_s *devp)
{
	int ret;
	struct vdin_parm_s vdin_cap_param;
	const struct tvin_format_s *fmt_info_p;

	/* TODO:get vd1/osd loopback source fmt */
	if (devp->index)
		vdin_v4l2_loopback_fmt(devp);

	/* Check args */
	fmt_info_p = tvin_get_fmt_info(devp->parm.info.fmt);
	if (!fmt_info_p) {
		pr_warn("%s,invalid fmt:%s\n",
			__func__, tvin_sig_fmt_str(devp->parm.info.fmt));
		return -1;
	}
	dprintk(1, "%s,fmt:%s,cfmt:%s,status:%#x,fps:%d,scan_mode:%d,h=%d,v=%d\n",
		__func__, tvin_sig_fmt_str(devp->parm.info.fmt),
		tvin_color_fmt_str(devp->parm.info.cfmt),
		devp->parm.info.status, devp->parm.info.fps, fmt_info_p->scan_mode,
		fmt_info_p->h_active, fmt_info_p->v_active);

	memset(&vdin_cap_param, 0, sizeof(struct vdin_parm_s));

	vdin_cap_param.scan_mode	= fmt_info_p->scan_mode;
	vdin_cap_param.h_active		= fmt_info_p->h_active;
	vdin_cap_param.v_active		= fmt_info_p->v_active;
	vdin_cap_param.fmt			= devp->parm.info.fmt;
	vdin_cap_param.cfmt			= devp->parm.info.cfmt;
	vdin_cap_param.frame_rate   = devp->parm.info.fps;
	vdin_cap_param.port         = devp->v4l2_port_cur;
	vdin_cap_param.dest_h_active = devp->v4l2_fmt.fmt.pix_mp.width;
	vdin_cap_param.dest_v_active = devp->v4l2_fmt.fmt.pix_mp.height;
	//vdin_cap_param.reserved    |= PARAM_STATE_SCREEN_CAP;

	if (devp->index) {
		vdin_cap_param.cfmt = TVIN_YUV422;
		vdin_cap_param.scan_mode  = TVIN_SCAN_MODE_PROGRESSIVE;
//		vdin_cap_param.fmt  = TVIN_SIG_FMT_HDMI_1920X1080P_60HZ;
//		vdin_cap_param.h_active   = 1920;
//		vdin_cap_param.v_active   = 1080;
//		vdin_cap_param.frame_rate = 60;
	}
	if (vdin_cap_param.frame_rate == 0)
		vdin_cap_param.frame_rate = 60;
	/* vdin can not do scale up */
	if (devp->v4l2_fmt.fmt.pix_mp.width > vdin_cap_param.h_active ||
		devp->v4l2_fmt.fmt.pix_mp.height > vdin_cap_param.v_active) {
		pr_err("%s,out of range!v4l2_fmt:%dx%d > active:%dx%d\n",
			__func__, devp->v4l2_fmt.fmt.pix_mp.width,
			devp->v4l2_fmt.fmt.pix_mp.height, vdin_cap_param.h_active,
			vdin_cap_param.v_active);
		return -EINVAL;
	}

	if (devp->v4l2_fmt.fmt.pix_mp.pixelformat == V4L2_PIX_FMT_NV21 ||
		devp->v4l2_fmt.fmt.pix_mp.pixelformat == V4L2_PIX_FMT_NV21M)
		vdin_cap_param.dfmt = TVIN_NV21;
	else if (devp->v4l2_fmt.fmt.pix_mp.pixelformat == V4L2_PIX_FMT_NV12 ||
		devp->v4l2_fmt.fmt.pix_mp.pixelformat == V4L2_PIX_FMT_NV12M)
		vdin_cap_param.dfmt = TVIN_NV12;
	else
		vdin_cap_param.dfmt = TVIN_YUV422;

	if (vdin_cap_param.h_active && vdin_cap_param.v_active &&
	   (vdin_cap_param.h_active > vdin_cap_param.dest_h_active ||
		vdin_cap_param.v_active > vdin_cap_param.dest_v_active)) {
		devp->prop.scaling4w = vdin_cap_param.dest_h_active;
		devp->prop.scaling4h = vdin_cap_param.dest_v_active;
	}

	dprintk(1, "%s,dest:%dx%d;scaling:%dx%d\n", __func__,
		vdin_cap_param.dest_h_active, vdin_cap_param.dest_v_active,
		devp->prop.scaling4w, devp->prop.scaling4h);
	ret = start_tvin_service(devp->index, &vdin_cap_param);

	return ret;
}

int vdin_v4l2_stop_tvin(struct vdin_dev_s *devp)
{
	int ret;

	ret = stop_tvin_service(devp->index);

	mutex_lock(&devp->fe_lock);
	vdin_close_fe(devp);
	mutex_unlock(&devp->fe_lock);

	return ret;
}

void vdin_v4l2_init(struct vdin_dev_s *devp, struct platform_device *pl_dev)
{
	int fe_ports_num;
	int i, ret;
	u32 tmp_u32;
	const char *str = NULL;

	memset(devp->v4l2_port, TVIN_PORT_NULL, sizeof(devp->v4l2_port));

	fe_ports_num =
		of_property_count_u32_elems(pl_dev->dev.of_node, "fe_ports");
	if (fe_ports_num < 0) {
		dprintk(0, "No 'fe_ports' property found\n");
		if (devp->index == 0) {
			devp->v4l2_port[0] = TVIN_PORT_HDMI0;
			devp->v4l2_port[1] = TVIN_PORT_HDMI1;
			devp->v4l2_port[2] = TVIN_PORT_HDMI2;
			devp->v4l2_port[3] = TVIN_PORT_CVBS0;
			devp->v4l2_port_num = 4;
		} else {
			devp->v4l2_port[0] = TVIN_PORT_VIU1_WB0_VD1;
			devp->v4l2_port[1] = TVIN_PORT_VIU1_WB0_VD2;
			devp->v4l2_port[2] = TVIN_PORT_VIU1_WB0_OSD1;
			devp->v4l2_port[3] = TVIN_PORT_VIU1_WB0_OSD2;
			devp->v4l2_port[4] = TVIN_PORT_VIU1_WB0_VPP;
			//devp->v4l2_port[5] = TVIN_PORT_VIU1_WB0_VDIN_BIST;
			devp->v4l2_port[5] = TVIN_PORT_VIU1_WB0_POST_BLEND;
			devp->v4l2_port_num = 6;
		}
	} else {
		devp->v4l2_port_num = 0;
	}

	for (i = 0; i < fe_ports_num; i++) {
		ret = of_property_read_u32_index(pl_dev->dev.of_node, "fe_ports", i,
				&tmp_u32);
		if (ret || tmp_u32 <= TVIN_PORT_NULL || tmp_u32 >= TVIN_PORT_MAX) {
			dprintk(0, "Invalid fe_port:%#x in property\n", tmp_u32);
			continue;
		}
		devp->v4l2_port[devp->v4l2_port_num] = tmp_u32;
		devp->v4l2_port_num++;
		dprintk(1, "index:%d,fe_port[%d]:%#x\n", i, devp->v4l2_port_num,
			devp->v4l2_port[i]);
	}
	/* default port */
	devp->v4l2_port_cur = TVIN_PORT_MAX;//devp->v4l2_port[0];

	ret = of_property_read_string(pl_dev->dev.of_node, "driver", &str);
	if (ret == 0) {
		strlcpy(g_vdin_v4l2_cap[devp->index].driver, str,
			sizeof(g_vdin_v4l2_cap[devp->index].driver));
		dprintk(0, "vdin%d,driver:%s\n", devp->index, str);
	}

	ret = of_property_read_string(pl_dev->dev.of_node, "card", &str);
	if (ret == 0) {
		strlcpy(g_vdin_v4l2_cap[devp->index].card, str,
			sizeof(g_vdin_v4l2_cap[devp->index].card));
		dprintk(0, "vdin%d,card:%s\n", devp->index, str);
	}

	ret = of_property_read_string(pl_dev->dev.of_node, "bus_info", &str);
	if (ret == 0) {
		strlcpy(g_vdin_v4l2_cap[devp->index].bus_info, str,
			sizeof(g_vdin_v4l2_cap[devp->index].bus_info));
		dprintk(0, "vdin%d,bus_info:%s\n", devp->index, str);
	}

	ret = of_property_read_u32(pl_dev->dev.of_node, "version", &tmp_u32);
	if (ret == 0) {
		g_vdin_v4l2_cap[devp->index].version = tmp_u32;
		dprintk(0, "vdin%d,version:%#x\n", devp->index, tmp_u32);
	}

	ret = of_property_read_u32(pl_dev->dev.of_node, "capabilities", &tmp_u32);
	if (ret == 0) {
		g_vdin_v4l2_cap[devp->index].capabilities = tmp_u32;
		dprintk(0, "vdin%d,capabilities:%#x\n", devp->index, tmp_u32);
	}

	ret = of_property_read_u32(pl_dev->dev.of_node, "device_caps", &tmp_u32);
	if (ret == 0) {
		g_vdin_v4l2_cap[devp->index].device_caps = tmp_u32;
		dprintk(0, "vdin%d,device_caps:%#x\n", devp->index, tmp_u32);
	}
	/* fill struct v4l2_ext_capture_capability_info with default value */
	devp->ext_cap_cap_info.flags =
		V4L2_EXT_CAPTURE_CAP_SCALE_DOWN |
		V4L2_EXT_CAPTURE_CAP_DIVIDE_FRAMERATE |
		V4L2_EXT_CAPTURE_CAP_INPUT_VIDEO_DEINTERLACE |
		V4L2_EXT_CAPTURE_CAP_DISPLAY_VIDEO_DEINTERLACE;
	devp->ext_cap_cap_info.scale_up_limit_w = 3840;
	devp->ext_cap_cap_info.scale_up_limit_h = 2160;
	devp->ext_cap_cap_info.scale_down_limit_w = 360;
	devp->ext_cap_cap_info.scale_down_limit_h = 240;
	/* LGE chips only */
	devp->ext_cap_cap_info.num_video_frame_buffer = 5;
	devp->ext_cap_cap_info.max_res.x = 0;
	devp->ext_cap_cap_info.max_res.y = 0;
	devp->ext_cap_cap_info.max_res.w = 3840;
	devp->ext_cap_cap_info.max_res.h = 2160;
	/* LGE chips only end */
	devp->ext_cap_cap_info.num_plane =
		V4L2_EXT_CAPTURE_VIDEO_FRAME_BUFFER_PLANE_INTERLEAVED;
	devp->ext_cap_cap_info.pixel_format =
		V4L2_EXT_CAPTURE_VIDEO_FRAME_BUFFER_PIXEL_FORMAT_YUV422_INTERLEAVED;
	/* fill struct v4l2_ext_capture_plane_info with default value */
	devp->ext_cap_plane_info.stride = 1920;
	devp->ext_cap_plane_info.plane_region.x = 0;
	devp->ext_cap_plane_info.plane_region.y = 0;
	devp->ext_cap_plane_info.plane_region.w = 1920;
	devp->ext_cap_plane_info.plane_region.h = 1080;
	devp->ext_cap_plane_info.active_region.x = 0;
	devp->ext_cap_plane_info.active_region.y = 0;
	devp->ext_cap_plane_info.active_region.w = 1920;
	devp->ext_cap_plane_info.active_region.h = 1080;

	/* fill struct v4l2_ext_capture_freeze_mode with default value */
	devp->ext_cap_freezee_mode.plane_index = 0;
	devp->ext_cap_freezee_mode.val = 0;

	/* fill struct v4l2_ext_capture_plane_prop with default value */
	devp->ext_cap_plane_prop.l = V4L2_EXT_CAPTURE_BLENDED_OUTPUT;

	devp->vdin_v4l2.divide = 1;
}
