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
/* Local Headers */
/*#include "../tvin_global.h"*/
/*#include "../tvin_format_table.h"*/
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

/*
 * function define
 *
 */
int vdin_v4l2_if_isr(struct vdin_dev_s *pdev, struct vframe_s *vfp)
{
	/*struct vb2_buffer *vb2_buf = NULL;*/
	unsigned int num_planes = 0;
	struct dma_buf *dmabuf;
	unsigned int i = 0;
	unsigned int fd, index;

	if (!pdev->vbqueue.streaming) {
		dprintk(2, "not streaming\n");
		return -1;
	}

	if (pdev->dbg_v4l_pause)
		return -1;

	spin_lock(&pdev->qlock);

	if (list_empty(&pdev->buf_list)) {
		dprintk(2, "warning: buffer empty\n");
		spin_unlock(&pdev->qlock);
		return -1;
	}
	/* pop a buffer */
	pdev->cur_buff = list_first_entry(&pdev->buf_list,
					  struct vdin_vb_buff, list);

	num_planes = pdev->cur_buff->vb.vb2_buf.num_planes;
	if (pdev->cur_buff->vb.vb2_buf.memory == V4L2_MEMORY_DMABUF) {
		for (i = 0; i < num_planes; i++) {
			fd = pdev->cur_buff->vb.vb2_buf.planes[i].m.fd;
			index = pdev->cur_buff->vb.vb2_buf.index;
			dmabuf = pdev->cur_buff->dmabuf[i];
			if (IS_ERR(dmabuf)) {
				spin_unlock(&pdev->qlock);
				dprintk(2, "%s fd:%d get dma buff fail\n",
					__func__, fd);
				return -1;
			}
			dprintk(2, "vb2_buf idx:%d pl:%d dmabuf:0x%p fd:%d vf:(%d:0x%p)\n",
				index, i, dmabuf, fd, vfp->index, vfp);
			if (dmabuf_set_vframe(dmabuf, vfp, VF_SRC_VDIN))
				dprintk(0, "set vf fail\n");
		}
	}

	list_del(&pdev->cur_buff->list);
	spin_unlock(&pdev->qlock);
	/*dprintk(3, "tag:0x%x\n", pdev->cur_buff->tag);*/
	pdev->dbg_v4l_done_cnt++;
	vb2_buffer_done(&pdev->cur_buff->vb.vb2_buf, VB2_BUF_STATE_DONE);
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
		return "VB2_BUF_STATE_DEQUEUED";
	case VB2_BUF_STATE_IN_REQUEST:
		return "VB2_BUF_STATE_IN_REQUEST";
	case VB2_BUF_STATE_PREPARING:
		return "VB2_BUF_STATE_PREPARING";
	/*case VB2_BUF_STATE_PREPARED:*/
	/*	return "VB2_BUF_STATE_PREPARED";*/
	case VB2_BUF_STATE_QUEUED:
		return "VB2_BUF_STATE_QUEUED";
	/*case VB2_BUF_STATE_REQUEUEING:*/
	/*	return "VB2_BUF_STATE_REQUEUEING";*/
	case VB2_BUF_STATE_ACTIVE:
		return "VB2_BUF_STATE_ACTIVE";
	case VB2_BUF_STATE_DONE:
		return "VB2_BUF_STATE_DONE";
	default:
		return "VB2_BUF_STATE_ERROR";
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

/*
 *
 *
 */
void vdin_fill_pix_format(struct vdin_dev_s *pdev)
{
	struct v4l2_format *v4lfmt = NULL;
	//unsigned int scan_mod = TVIN_SCAN_MODE_PROGRESSIVE;

	if (IS_ERR_OR_NULL(pdev))
		return;

	v4lfmt = &pdev->v4lfmt;
	/*give a default size*/
	if (v4lfmt->fmt.pix_mp.num_planes == 1) {
		v4lfmt->fmt.pix_mp.plane_fmt[0].sizeimage = VDIN_IMG_SIZE;
		dprintk(1, "plane 0:sizeimage=%d\n ",
			v4lfmt->fmt.pix_mp.plane_fmt[0].sizeimage);
	} else if (v4lfmt->fmt.pix_mp.num_planes == 2) {
		v4lfmt->fmt.pix_mp.plane_fmt[0].sizeimage = VDIN_IMG_SIZE;
		v4lfmt->fmt.pix_mp.plane_fmt[1].sizeimage = VDIN_IMG_SIZE / 2;
		dprintk(1, "plane 0:sizeimage=%d\n ",
			v4lfmt->fmt.pix_mp.plane_fmt[0].sizeimage);
		dprintk(1, "plane 1:sizeimage=%d\n ",
			v4lfmt->fmt.pix_mp.plane_fmt[1].sizeimage);
	}
}

/*
 * Query device capability
 * cmd ID:VIDIOC_QUERYCAP
 */
static int vdin_vidioc_querycap(struct file *file, void *priv,
				struct v4l2_capability *cap)
{
	struct vdin_dev_s *pdev = video_drvdata(file);

	if (IS_ERR_OR_NULL(pdev))
		return -EFAULT;

	cap->version = VDIN_DEV_VER;
	cap->capabilities = cap->device_caps | V4L2_CAP_DEVICE_CAPS;
	dprintk(2, "%s :0x%x\n", __func__, cap->capabilities);

	strlcpy(cap->driver, VDIN_V4L_DV_NAME, sizeof(cap->driver));
	strlcpy(cap->bus_info, VDIN_V4L_DV_NAME, sizeof(cap->bus_info));
	strlcpy(cap->card, VDIN_V4L_DV_NAME, sizeof(cap->card));
	return 0;
}

/*
 * query video standerd
 * cmd ID: VIDIOC_QUERYSTD
 */
static int vdin_vidioc_querystd(struct file *file, void *priv,
				v4l2_std_id *std)
{
	struct vdin_dev_s *pdev = video_drvdata(file);

	dprintk(2, "%s\n", __func__);
	if (IS_ERR_OR_NULL(pdev))
		return -EFAULT;

	if (pdev->work_mode == VDIN_WORK_MD_NORMAL)
		dprintk(0, "%s err work mode\n", __func__);

	return 0;
}

/*
 * set current input
 * cmd ID: VIDIOC_ENUMINPUT
 */
static int vdin_vidioc_enum_input(struct file *file,
				  void *fh, struct v4l2_input *inp)
{
	struct vdin_dev_s *pdev = video_drvdata(file);
	unsigned int ret = 0;

	dprintk(2, "%s -start\n", __func__);
	if (IS_ERR_OR_NULL(pdev))
		return -EFAULT;

	if (pdev->work_mode == VDIN_WORK_MD_NORMAL)
		dprintk(0, "%s err work mode\n", __func__);

	mutex_lock(&pdev->lock);
	/*inp->type = V4L2_INPUT_TYPE_CAMERA;*/
	inp->std = V4L2_STD_ALL;

	if (ret) {
		dprintk(0, "open vdin fe error\n");
		ret = -EFAULT;
	}
	dprintk(2, "%s :%d\n", __func__, inp->index);
	mutex_unlock(&pdev->lock);
	return ret;
}

/*
 * alloca the video frame buffer
 * cmd ID:VIDIOC_REQBUFS
 */
static int vdin_vidioc_reqbufs(struct file *file, void *priv,
			       struct v4l2_requestbuffers *reqbufs)
{
	struct vdin_dev_s *pdev = video_drvdata(file);
	unsigned int req_buffs_num = reqbufs->count;
	unsigned int type = reqbufs->type;
	int ret = 0;
	unsigned int i = 0;
	struct vb2_v4l2_buffer *vbbuf = NULL;
	struct vdin_vb_buff *vdin_buf = NULL;

	dprintk(2, "%s type:%d buff_num:%d\n", __func__, type, req_buffs_num);

	if (IS_ERR_OR_NULL(pdev))
		return -EPERM;

	if (reqbufs->memory != V4L2_MEMORY_DMABUF) {
		dprintk(0, "err buffer mode, only support dma buf uvm mode\n");
		return -EINVAL;
	}

	/*need config by input surce type*/
	pdev->source_bitdepth = VDIN_COLOR_DEEPS_8BIT;
	pdev->vbqueue.type = reqbufs->type;

	//vdin_buffer_calculate(pdev, req_buffs_num);
	vdin_fill_pix_format(pdev);

	ret = vb2_ioctl_reqbufs(file, priv, reqbufs);
	if (ret < 0)
		dprintk(0, "vb2_ioctl_reqbufs fail\n");

	vbbuf = to_vb2_v4l2_buffer(pdev->vbqueue.bufs[i]);
	vdin_buf = to_vdin_vb_buf(vbbuf);

	/*check buffer*/
	dprintk(1, "%s num_buffers %d -end\n", __func__,
		pdev->vbqueue.num_buffers);
	return ret;
}

/*
 *
 * cmd ID:VIDIOC_CREATE_BUFS
 */
int vdin_vidioc_create_bufs(struct file *file, void *priv,
			    struct v4l2_create_buffers *p)
{
	/*struct vdin_dev_s *pdev = video_drvdata(file);*/
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
	struct vdin_dev_s *pdev = video_drvdata(file);
	struct vb2_queue *vbque = NULL;
	unsigned int ret = 0;
	struct vb2_buffer *vb2buf = NULL;

	//struct v4l2_plane *planes = NULL;
	//dma_addr_t dma_addr = 0;
	//unsigned int i = 0;

	/*struct vb2_buffer *dst_buf = NULL;*/

	if (IS_ERR_OR_NULL(pdev))
		return -EFAULT;

	/*pr_iotrl("%s idx:%d\n", __func__, v4lbuf->index);*/

	vbque = &pdev->vbqueue;
	vb2buf = vbque->bufs[v4lbuf->index];

	ret = vb2_ioctl_querybuf(file, priv, v4lbuf);

	//if (!(v4lbuf->index < vbque->num_buffers)) {
	//	dprintk("err buff idx %d max is %d\n",
	//		v4lbuf->index, vbque->num_buffers);
	//	return -EFAULT;
	//}
	//dprintk("%s %d is_multiplanar:%d\n", __func__, v4lbuf->index, vbque->is_multiplanar);

	//if (vbque->is_multiplanar && vb2buf->num_planes <= 2) {
	//	for (i = 0; i < vb2buf->num_planes; i++) {
	//		dma_addr = vb2_dma_contig_plane_dma_addr(vb2buf, i);
	//		planes = &v4lbuf->m.planes[0];
	//		dprintk("L%d:dma_addr:0x%x mem_offset=0x%x bytesused=0x%x\n",
	//			i, dma_addr, planes->m.mem_offset,
	//			planes->bytesused);
	//	}

	//} else {
	//	/*only one plane, index is 0*/
	//	dma_addr = vb2_dma_contig_plane_dma_addr(vb2buf, 0);
	//	pr_vdin("%s dma_addr: 0x%x\n", __func__, dma_addr);
	//}

	return ret;
}

/*
 * user put a empty vframe to driver empty video buffer
 * cmd ID:VIDIOC_QBUF
 */
static int vdin_vidioc_qbuf(struct file *file, void *priv,
			    struct v4l2_buffer *p)
{
	struct vdin_dev_s *pdev = video_drvdata(file);
	int ret = 0;
	struct dma_buf *dmabuf;
	struct vb2_v4l2_buffer *vb = NULL;
	struct vdin_vb_buff *vdin_buf = NULL;
	unsigned int fd, i, planes;
	struct uvm_handle *handle;

	if (IS_ERR_OR_NULL(pdev))
		return -EFAULT;

	vb = to_vb2_v4l2_buffer(pdev->vbqueue.bufs[p->index]);
	vdin_buf = to_vdin_vb_buf(vb);
	planes = vb->vb2_buf.num_planes;
	dprintk(3, "%s id:%d planes:%d\n", __func__, p->index, planes);

	/*dprintk(1, "streaming:%d\n", pdev->vbqueue.streaming);*/
	ret = vb2_ioctl_qbuf(file, priv, p);
	if (ret < 0)
		dprintk(0, "%s err\n", __func__);

	/* recycle buffer */
	if (pdev->vbqueue.streaming && p->memory == V4L2_MEMORY_DMABUF) {
		pdev->dbg_v4l_que_cnt++;
		for (i = 0; i < planes; i++) {
			fd = p->m.planes[i].m.fd;
			dmabuf = dma_buf_get(fd);
			handle = dmabuf->priv;
			/*save and backup dmabuf address*/
			vdin_buf->dmabuf[i] = dmabuf;
			if (IS_ERR(dmabuf)) {
				dprintk(0, "%s get dmabuff fail0\n", __func__);
				return -1;
			}
			dprintk(3, "%s dmabuf fd:%d addr:0x%p\n", __func__, fd,
				dmabuf);
			if (!IS_ERR(handle->vfp)) {
				receiver_vf_put(handle->vfp, pdev->vfp);
				dprintk(3, "vf idx:%d (0x%p) put back to pool\n",
					handle->vfp->index, handle->vfp);
			} else {
				dprintk(2, "err vf null\n");
			}
		}
	} else if (p->memory == V4L2_MEMORY_DMABUF) {
		for (i = 0; i < planes; i++) {
			fd = p->m.planes[i].m.fd;
			dmabuf = dma_buf_get(fd);
			/*save and backup dmabuf address*/
			vdin_buf->dmabuf[i] = dmabuf;
			if (IS_ERR(dmabuf)) {
				dprintk(0, "%s get dmabuff fail1\n", __func__);
				return -1;
			}
			dprintk(3, "%s dmabuf fd:%d addr:0x%p\n", __func__, fd,
				dmabuf);
		}
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
	struct vdin_dev_s *pdev = video_drvdata(file);
	struct vb2_v4l2_buffer *vb = NULL;
	struct vdin_vb_buff *vdin_buf = NULL;
	/*struct vf_entry *vfe;*/
	unsigned int ret = 0;
	unsigned int fd, i, planes;
	struct dma_buf *dmabuf;
	struct uvm_handle *handle;
	/*static unsigned int framecnt;*/
	/*struct vframe_s *vf;*/

	/*pr_iotrl("%s\n", __func__);*/
	ret = vb2_ioctl_dqbuf(file, priv, p);
	if (ret) {
		dprintk(0, "DQ error\n");
		return -1;
	}
	/*framecnt++;*/
	vb = to_vb2_v4l2_buffer(pdev->vbqueue.bufs[p->index]);
	vdin_buf = to_vdin_vb_buf(vb);
	planes = vb->vb2_buf.num_planes;
	if (pdev->vbqueue.streaming && p->memory == V4L2_MEMORY_DMABUF) {
		pdev->dbg_v4l_dque_cnt++;
		for (i = 0; i < planes; i++) {
			fd = p->m.planes[i].m.fd;
			dmabuf = dma_buf_get(fd);
			if (!IS_ERR(dmabuf)) {
				handle = dmabuf->priv;
				dprintk(3, "dQue idx:%d dmabuf fd:%d vf(%d:0x%p)\n",
					p->index, fd, handle->vfp->index,
					handle->vfp);
			} else {
				dprintk(0, "%s get dmabuf fail\n", __func__);
			}
		}
	}

	return ret;
}

/*
 *
 * cmd ID:VIDIOC_EXPBUF
 */
static int vdin_vidioc_expbuf(struct file *file, void *priv,
			      struct v4l2_exportbuffer *p)
{
	struct vdin_dev_s *pdev = video_drvdata(file);
	struct dma_buf *dmabuf;
	//struct vb2_v4l2_buffer *vbbuf = NULL;
	//struct vdin_vb_buff *vdin_buf = NULL;
	int ret;

	if (IS_ERR_OR_NULL(pdev))
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
	struct vdin_dev_s *pdev = video_drvdata(file);
	unsigned int ret = 0;

	if (IS_ERR_OR_NULL(pdev))
		return -EFAULT;

	//vdin_start_dec(devp);
	//if (devp->parm.port != TVIN_PORT_VIU1 ||
	//    viu_hw_irq != 0) {
	//	enable_irq(devp->irq);

	//	dprintk(1, "%s START_DEC vdin.%d enable_irq\n",
	//		__func__, devp->index);
	//}

	//devp->flags |= VDIN_FLAG_DEC_STARTED;

	ret = vb2_ioctl_streamon(file, priv, i);
	pdev->dbg_v4l_done_cnt = 0;
	pdev->dbg_v4l_que_cnt = 0;
	pdev->dbg_v4l_dque_cnt = 0;

	dprintk(2, "%s\n", __func__);
	return 0;
}

static int vdin_vidioc_streamoff(struct file *file, void *priv,
				 enum v4l2_buf_type i)
{
	struct vdin_dev_s *pdev = video_drvdata(file);
	unsigned int ret = 0;

	dprintk(0, "%s\n", __func__);
	if (IS_ERR_OR_NULL(pdev))
		return -EFAULT;

	ret = vb2_ioctl_streamoff(file, priv, i);

	return 0;
}

/*
 * for single plane
 * cmd ID:VIDIOC_G_FMT
 */
static int vdin_vidioc_g_fmt_vid_cap(struct file *file, void *priv,
				     struct v4l2_format *f)
{
	struct vdin_dev_s *pdev = video_drvdata(file);

	dprintk(2, "%s\n", __func__);
	if (IS_ERR_OR_NULL(pdev))
		return -EFAULT;
	/*for test set a default value*/
	pdev->v4lfmt.fmt.pix.width = 1280;
	pdev->v4lfmt.fmt.pix.height = 720;
	pdev->v4lfmt.fmt.pix.quantization = V4L2_QUANTIZATION_DEFAULT;
	pdev->v4lfmt.fmt.pix.ycbcr_enc = V4L2_YCBCR_ENC_709;
	pdev->v4lfmt.fmt.pix.field = V4L2_FIELD_ANY;
	pdev->v4lfmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;

	memcpy(&f->fmt.pix, &pdev->v4lfmt.fmt.pix,
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
	struct vdin_dev_s *pdev = video_drvdata(file);
	/*struct v4l2_format *pfmt = pdev->pixfmt;*/

	dprintk(2, "%s\n", __func__);
	if (IS_ERR_OR_NULL(pdev))
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
	struct vdin_dev_s *pdev = video_drvdata(file);

	if (IS_ERR_OR_NULL(pdev))
		return -EFAULT;

	/* for test set a default value
	 * mult-planes mode
	 */
	pdev->v4lfmt.fmt.pix_mp.width = 320;
	pdev->v4lfmt.fmt.pix_mp.height = 240;
	pdev->v4lfmt.fmt.pix_mp.quantization = V4L2_QUANTIZATION_DEFAULT;
	pdev->v4lfmt.fmt.pix_mp.ycbcr_enc = V4L2_YCBCR_ENC_709;
	pdev->v4lfmt.fmt.pix_mp.field = V4L2_FIELD_ANY;
	pdev->v4lfmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_YUYV;
	pdev->v4lfmt.fmt.pix_mp.num_planes = VDIN_NUM_PLANES;

	f->fmt.pix_mp.width = pdev->v4lfmt.fmt.pix_mp.width;
	f->fmt.pix_mp.height = pdev->v4lfmt.fmt.pix_mp.height;
	f->fmt.pix_mp.quantization = pdev->v4lfmt.fmt.pix_mp.quantization;
	f->fmt.pix_mp.ycbcr_enc = pdev->v4lfmt.fmt.pix_mp.ycbcr_enc;
	f->fmt.pix_mp.field = pdev->v4lfmt.fmt.pix_mp.field;
	f->fmt.pix_mp.pixelformat = pdev->v4lfmt.fmt.pix_mp.pixelformat;
	f->fmt.pix_mp.num_planes = pdev->v4lfmt.fmt.pix_mp.num_planes;
	dprintk(1, "%s num_planes=%d\n", __func__, f->fmt.pix_mp.num_planes);
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
	struct vdin_dev_s *pdev = video_drvdata(file);

	if (IS_ERR_OR_NULL(pdev))
		return -EFAULT;

	dprintk(1, "width=%d height=%d\n", fmt->fmt.pix_mp.width,
		fmt->fmt.pix_mp.height);
	dprintk(1, "pixfmt=0x%x num_planes=0x%x\n",
		fmt->fmt.pix_mp.pixelformat, fmt->fmt.pix_mp.num_planes);

	return 0;
}

static const struct v4l2_ioctl_ops vdin_v4l2_ioctl_ops = {
	.vidioc_querystd = vdin_vidioc_querystd,
	.vidioc_enum_input = vdin_vidioc_enum_input,
	.vidioc_querycap = vdin_vidioc_querycap,

	/*queue ioctrol*/
	.vidioc_reqbufs = vdin_vidioc_reqbufs,
	.vidioc_create_bufs = vdin_vidioc_create_bufs,
	.vidioc_querybuf = vdin_vidioc_querybuf,
	.vidioc_qbuf = vdin_vidioc_qbuf,
	.vidioc_dqbuf = vdin_vidioc_dqbuf,
	.vidioc_expbuf = vdin_vidioc_expbuf,
	.vidioc_streamon = vdin_vidioc_streamon,
	.vidioc_streamoff = vdin_vidioc_streamoff,

	.vidioc_s_fmt_vid_cap_mplane	= vdin_vidioc_s_fmt_vid_cap_mplane,
	/*VIDIOC_S_FMT*/
	.vidioc_s_fmt_vid_cap		= vdin_vidioc_s_fmt_vid_cap,
	.vidioc_s_fmt_vid_out_mplane	= vdin_vidioc_s_fmt_vid_cap_mplane,
	.vidioc_s_fmt_vid_out		= vdin_vidioc_s_fmt_vid_cap,

	.vidioc_g_fmt_vid_cap_mplane	= vdin_vidioc_g_fmt_vid_cap_mplane,
	.vidioc_g_fmt_vid_cap		= vdin_vidioc_g_fmt_vid_cap,
	.vidioc_g_fmt_vid_out_mplane	= vdin_vidioc_g_fmt_vid_cap_mplane,
	.vidioc_g_fmt_vid_out		= vdin_vidioc_g_fmt_vid_cap,
};

static void vdin_vdev_release(struct video_device *vdev)
{
	dprintk(2, "%s\n", __func__);
}

static int vdin_v4l2_open(struct file *file)
{
	struct vdin_dev_s *pdev = video_drvdata(file);

	if (IS_ERR_OR_NULL(pdev))

		return -EFAULT;
	dprintk(0, "%s\n", __func__);
	/*dump_stack();*/

	v4l2_fh_open(file);

	INIT_LIST_HEAD(&pdev->buf_list);

	/*pdev->work_mode = VDIN_WORK_MD_V4L;*/
	if (pdev->work_mode == VDIN_WORK_MD_NORMAL)
		dprintk(0, "%s err:vdin v4l mode not enable\n", __func__);

	return 0;
}

static int vdin_v4l2_release(struct file *file)
{
	struct vdin_dev_s *pdev = video_drvdata(file);
	int ret = 0;

	dprintk(0, "%s\n", __func__);

	if (IS_ERR_OR_NULL(pdev))
		return -EFAULT;

	/*release que*/
	ret = vb2_fop_release(file);

	if (pdev->work_mode == VDIN_WORK_MD_NORMAL)
		dprintk(0, "%s err:vdin v4l mode not enable\n", __func__);

	return ret;
}

static unsigned int vdin_v4l2_poll(struct file *file,
				   struct poll_table_struct *wait)
{
	/*struct vdin_dev_s *pdev = video_drvdata(file);*/
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
	struct vdin_dev_s *pdev = video_drvdata(file);
	long ret = 0;

	mutex_lock(&pdev->ioctrl_lock);
	ret = video_ioctl2(file, cmd, arg);
	mutex_unlock(&pdev->ioctrl_lock);
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

static void vdin_return_all_buffers(struct vdin_dev_s *pdev,
				    enum vb2_buffer_state state)
{
	struct vdin_vb_buff *buf, *node;
	unsigned long flags = 0;

	dprintk(2, "%s\n", __func__);
	spin_lock_irqsave(&pdev->qlock, flags);
	list_for_each_entry_safe(buf, node, &pdev->buf_list, list) {
		dprintk(2, "%s idx:%d\n", __func__, buf->vb.vb2_buf.index);
		vb2_buffer_done(&buf->vb.vb2_buf, state);
		list_del(&buf->list);
	}
	INIT_LIST_HEAD(&pdev->buf_list);
	spin_unlock_irqrestore(&pdev->qlock, flags);
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
	struct vdin_dev_s *pdev = vb2_get_drv_priv(vq);
	unsigned int i = 0;

	/*
	 * NV12 every frame need two buffer
	 * one for Y, one for UV
	 * need return the num_planes per buffer
	 */
	*num_planes = VDIN_NUM_PLANES;/*NV12 need two*/
	for (i = 0; i < *num_planes; i++) {
		sizes[i] = pdev->v4lfmt.fmt.pix_mp.plane_fmt[i].sizeimage;
		dprintk(1, "plane %d, size %d\n", i, sizes[i]);
	}

	dprintk(1, "type: %d, plane: %d, buf cnt: %d, size: [Y: %u, C: %u]\n",
		vq->type, *num_planes, *num_buffers, sizes[0], sizes[1]);
	return 0;
}

/*
 * buf_prepare
 *
 */
static int vdin_vb2ops_buffer_prepare(struct vb2_buffer *vb)
{
	struct vdin_dev_s *pdev = vb2_get_drv_priv(vb->vb2_queue);
	struct vb2_queue *p_vbque = NULL;
	/*uint size = 0;*/
	/*unsigned int i;*/

	if (IS_ERR_OR_NULL(pdev))
		return -EINVAL;

	p_vbque = &pdev->vbqueue;
	dprintk(1, "buf prepare idx:%d bufs:%d planes:%d quedcnt:%d, bufsts:%s\n",
		vb->index, p_vbque->num_buffers,
		vb->num_planes, p_vbque->queued_count,
		vb2_buf_sts_to_str(vb->state));

	//if (vb->num_planes > 1) {
	//	for (i = 0; i < vb->num_planes; i++) {
	//		size = pdev->v4lfmt.fmt.pix_mp.plane_fmt[i].sizeimage;
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
	struct vdin_dev_s *pdev = vb2_get_drv_priv(vb->vb2_queue);
	struct vb2_v4l2_buffer *v4lbuf = to_vb2_v4l2_buffer(vb);
	struct vdin_vb_buff *buf = to_vdin_vb_buf(v4lbuf);
	unsigned long flags = 0;

	dprintk(2, "%s buf:%d, state:%s\n", __func__, vb->index,
		vb2_buf_sts_to_str(buf->vb.vb2_buf.state));

	spin_lock_irqsave(&pdev->qlock, flags);
	list_add_tail(&buf->list, &pdev->buf_list);

	spin_unlock_irqrestore(&pdev->qlock, flags);
	/* TODO: Update any DMA pointers if necessary */
	dprintk(2, "num_buf:%d, qued_cnt:%d, after state:%s\n",
		pdev->vbqueue.num_buffers,
		pdev->vbqueue.queued_count,
		vb2_buf_sts_to_str(buf->vb.vb2_buf.state));
}

static int vdin_v4l2_start_streaming(struct vdin_dev_s *pdev)
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
	struct vdin_dev_s *pdev = vb2_get_drv_priv(vq);
	struct list_head *buf_head = NULL;
	/*struct vb2_buffer *vb2_buf;*/
	uint ret = 0;

	if (IS_ERR_OR_NULL(pdev))
		return -EINVAL;

	buf_head = &pdev->buf_list;
	if (list_empty(buf_head)) {
		dprintk(0, "buf_list is empty\n");
		return -EINVAL;
	}

	pdev->frame_cnt = 0;

	ret = vdin_v4l2_start_streaming(pdev);
	if (ret) {
		/*
		 * In case of an error, return all active buffers to the
		 * QUEUED state
		 */
		vdin_return_all_buffers(pdev, VB2_BUF_STATE_QUEUED);
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
	struct vdin_dev_s *pdev = vb2_get_drv_priv(vq);

	dprintk(2, "%s\n", __func__);
	/* TODO: stop DMA */

	pdev->vbqueue.streaming = false;

	/* Release all active buffers */
	vdin_return_all_buffers(pdev, VB2_BUF_STATE_ERROR);
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
	/*dma_addr_t phy_addr = 0;*/
	/*unsigned int i = 0;*/
	struct vb2_v4l2_buffer *vbbuf = NULL;
	struct vdin_vb_buff *vdin_buf = NULL;

	dprintk(1, "%s idx:%d, type:0x%x, memory:0x%x, num_planes:%d\n",
		__func__,
		vb->index, vb->type, vb->memory, vb->num_planes);

	dprintk(2, "vb2q type:0x%x numbuff:%d request id:%d\n", vb2q->type,
		vb2q->num_buffers, vb2q->queued_count);

	//for (i = 0; i < vb->num_planes; i++) {
	//	phy_addr = vb2_dma_contig_plane_dma_addr(vb, i);
	//	dprintk(1, "planes %d phy_addr=0x%x\n", i, phy_addr);
	//}

	vbbuf = to_vb2_v4l2_buffer(vb);
	vdin_buf = to_vdin_vb_buf(vbbuf);

	/* for test add a tag*/
	vdin_buf->tag = 0xa5a6ff00 + vb->index;
	/* set a tag for test*/
	dprintk(2, "vbbuf:0x%p vdin_buf:0x%p tag:0x%x\n",
		vbbuf, vdin_buf, vdin_buf->tag);

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
	struct vdin_dev_s *pdev = vb2_get_drv_priv(vb->vb2_queue);
	struct vb2_queue *p_vbque = NULL;

	if (IS_ERR_OR_NULL(pdev))
		return;
	p_vbque = &pdev->vbqueue;

	/*dprintk("%s idx:%d, qued cnt:%d, vbuf:%s\n", __func__, vb->index,*/
	/*	  p_vbque->queued_count, vb2_buf_sts_to_str(vb->state));*/
}

/*
 * The vb2 queue ops. Note that since q->lock is set we can use the standard
 * vb2_ops_wait_prepare/finish helper functions. If q->lock would be NULL,
 * then this driver would have to provide these ops.
 */
static struct vb2_ops vdin_vb2ops = {
	.queue_setup		= vdin_vb2ops_queue_setup,
	.buf_prepare		= vdin_vb2ops_buffer_prepare,
	.buf_queue		= vdin_vb2ops_buffer_queue,

	.buf_init		= vdin_vb2ops_buf_init,
	.buf_finish		= vdin_vb2ops_buf_finish,

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
	.mmap = vdin_v4l2_mmap, /*files memory mapp*/
	.unlocked_ioctl = vdin_v4l2_ioctl, /*iocontorl op interface*/
};

static int vdin_v4l2_queue_init(struct vdin_dev_s *devp,
				struct vb2_queue *que)
{
	int ret;

	/*unsigned int ret = 0;*/
	que->type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	/*que->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;*/
	que->io_modes = VB2_MMAP | VB2_DMABUF;
	que->dev = &devp->vdev.dev/*devp->dev*/;
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
	que->min_buffers_needed = 2;
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
int vdin_v4l2_probe(struct platform_device *pldev,
		    struct vdin_dev_s *pvdindev)
{
	struct video_device *video_dev = NULL;
	//struct vb2_queue *queue = NULL;
	int ret = 0;

	if (IS_ERR_OR_NULL(pvdindev)) {
		ret = -ENODEV;
		dprintk(0, "[vdin] vdevp err\n");
		return ret;
	}

	/*vdin 1 not support v4l2 interface*/
	if (pvdindev->index == 1)
		return 0;

	/*dprintk(1, "%s vdin[%d] start\n", __func__, pvdindev->index);*/
	snprintf(pvdindev->v4l2_dev.name, sizeof(pvdindev->v4l2_dev.name),
		 "%s", VDIN_V4L_DV_NAME);

	/*video device initial*/
	video_dev = &pvdindev->vdev;
	video_dev->v4l2_dev = &pvdindev->v4l2_dev;
	/*dprintk(1, "video_device addr:0x%p\n", video_dev);*/
	/* Initialize the top-level structure */
	ret = v4l2_device_register(&pldev->dev, &pvdindev->v4l2_dev);
	if (ret) {
		dprintk(0, "v4l2 dev register fail\n");
		ret = -ENODEV;
		goto v4l2_device_register_fail;
	}

	/* Initialize the vb2 queue */
	//queue = &pvdindev->vbqueue;
	ret = vdin_v4l2_queue_init(pvdindev, &pvdindev->vbqueue);
	if (ret)
		goto video_register_device_fail;

	INIT_LIST_HEAD(&pvdindev->buf_list);
	mutex_init(&pvdindev->lock);
	spin_lock_init(&pvdindev->qlock);
	mutex_init(&pvdindev->ioctrl_lock);

	strlcpy(video_dev->name, VDIN_V4L_DV_NAME, sizeof(video_dev->name));
	video_dev->fops = &vdin_v4l2_fops,
	video_dev->ioctl_ops = &vdin_v4l2_ioctl_ops,
	video_dev->release = vdin_vdev_release;
	video_dev->lock = &pvdindev->lock;
	video_dev->queue = &pvdindev->vbqueue;
	video_dev->v4l2_dev = &pvdindev->v4l2_dev;/*v4l2_device_register*/
	video_dev->device_caps = V4L2_CAP_VIDEO_CAPTURE_MPLANE;//V4L2_CAP_VIDEO_CAPTURE;

	video_dev->vfl_type = VFL_TYPE_GRABBER;
	video_dev->vfl_dir   = VFL_DIR_RX;
	video_dev->dev_debug = (V4L2_DEV_DEBUG_IOCTL | V4L2_DEV_DEBUG_IOCTL_ARG);

	/*
	 * init the device node name, v4l2 will create
	 * a video device. the name is:videoXX (VDIN_V4L_DV_NAME)
	 * otherwise will be a videoXX, XX is a number
	 */
	/*video_dev->dev.init_name = VDIN_V4L_DV_NAME;*/
	video_set_drvdata(video_dev, pvdindev);

	ret = video_register_device(video_dev, VFL_TYPE_GRABBER,/* -1*/
				    (VDIN_VD_NUMBER + (pvdindev->index)));
	if (ret) {
		dprintk(0, "register dev fail.\n");
		goto video_register_device_fail;
	}

	pldev->dev.dma_mask = &pldev->dev.coherent_dma_mask;
	if (dma_set_coherent_mask(&pldev->dev, 0xffffffff) < 0)
		dprintk(0, "dev set_coherent_mask fail\n");

	if (dma_set_mask(&pldev->dev, 0xffffffff) < 0)
		dprintk(0, "set dma maks fail\n");

	video_dev->dev.dma_mask = &video_dev->dev.coherent_dma_mask;
	if (dma_set_coherent_mask(&video_dev->dev, 0xffffffff) < 0)
		dprintk(0, "dev set_coherent_mask fail\n");

	if (dma_set_mask(&video_dev->dev, 0xffffffff) < 0)
		dprintk(0, "set dma maks fail\n");

	/*device node need attach device tree vdin dts tree*/
	video_dev->dev.of_node = pldev->dev.of_node;
	ret = of_reserved_mem_device_init(&video_dev->dev);
	if (ret == 0)
		dprintk(0, "rev memory resource ok\n");
	else
		dprintk(0, "rev memory resource undefined!!!\n");

	dprintk(0, "dev registered as %s\n",
		video_device_node_name(video_dev));

	dprintk(0, "vdin[%d] %s ok\n", pvdindev->index, __func__);
	//dprintk(2, "vdin_dev_data p=0x%p\n", pvdindev);
	//dprintk(2, "video_device p=0x%p\n", video_dev);
	//dprintk(2, "vbqueue p=0x%p\n", &pvdindev->vbqueue);
	//dprintk(2, "video_dev vbqueue p=0x%p\n", video_dev->queue);
	//dprintk(2, "v4l2_dev p=0x%p\n", &pvdindev->v4l2_dev);

	return 0;

video_register_device_fail:
	v4l2_device_unregister(&pvdindev->v4l2_dev);

v4l2_device_register_fail:
	return ret;
}

