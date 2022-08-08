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
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of.h>
#include <media/v4l2-event.h>
#include <media/v4l2-mem2mem.h>
#include <media/videobuf2-dma-contig.h>
#include <linux/kthread.h>
#include <linux/compat.h>

#include "aml_vcodec_drv.h"
#include "aml_vcodec_dec.h"
#include "aml_vcodec_util.h"
#include "aml_vcodec_vpp.h"
#include <linux/file.h>
#include <linux/anon_inodes.h>

#define VDEC_HW_ACTIVE		0x10
#define VDEC_IRQ_CFG		0x11
#define VDEC_IRQ_CLR		0x10
#define VDEC_IRQ_CFG_REG	0xa4

#define V4LVIDEO_IOC_MAGIC  'I'
#define V4LVIDEO_IOCTL_ALLOC_FD				_IOW(V4LVIDEO_IOC_MAGIC, 0x02, int)
#define V4LVIDEO_IOCTL_CHECK_FD				_IOW(V4LVIDEO_IOC_MAGIC, 0x03, int)
#define V4LVIDEO_IOCTL_SET_CONFIG_PARAMS	_IOWR(V4LVIDEO_IOC_MAGIC, 0x04, struct v4l2_config_parm)
#define V4LVIDEO_IOCTL_GET_CONFIG_PARAMS	_IOWR(V4LVIDEO_IOC_MAGIC, 0x05, struct v4l2_config_parm)

bool param_sets_from_ucode = 1;
bool enable_drm_mode;
extern void aml_vdec_pic_info_update(struct aml_vcodec_ctx *ctx);
char dump_path[32] = "/data";

static int fops_vcodec_open(struct file *file)
{
	struct aml_vcodec_dev *dev = video_drvdata(file);
	struct aml_vcodec_ctx *ctx = NULL;
	struct aml_video_dec_buf *aml_buf = NULL;
	int ret = 0;
	struct vb2_queue *src_vq;

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;
	kref_init(&ctx->ctx_ref);

	aml_buf = kzalloc(sizeof(*aml_buf), GFP_KERNEL);
	if (!aml_buf) {
		kfree(ctx);
		return -ENOMEM;
	}

	ctx->meta_infos.meta_bufs = vzalloc(sizeof(struct meta_data) * V4L_CAP_BUFF_MAX);
	if (ctx->meta_infos.meta_bufs == NULL) {
		kfree(aml_buf);
		kfree(ctx);
		return -ENOMEM;
	}

	mutex_lock(&dev->dev_mutex);
	ctx->empty_flush_buf = aml_buf;
	ctx->id = dev->id_counter++;
	v4l2_fh_init(&ctx->fh, video_devdata(file));
	file->private_data = &ctx->fh;
	v4l2_fh_add(&ctx->fh);
	INIT_LIST_HEAD(&ctx->list);
	INIT_LIST_HEAD(&ctx->vdec_thread_list);
	INIT_LIST_HEAD(&ctx->task_chain_pool);
	dev->filp = file;
	ctx->dev = dev;
	init_waitqueue_head(&ctx->queue);
	mutex_init(&ctx->capture_buffer_lock);
	mutex_init(&ctx->buff_done_lock);
	mutex_init(&ctx->state_lock);
	mutex_init(&ctx->comp_lock);
	spin_lock_init(&ctx->slock);
	spin_lock_init(&ctx->tsplock);
	spin_lock_init(&ctx->dmabuff_recycle_lock);
	init_completion(&ctx->comp);
	init_waitqueue_head(&ctx->wq);
	init_waitqueue_head(&ctx->cap_wq);
	init_waitqueue_head(&ctx->post_done_wq);
	INIT_WORK(&ctx->dmabuff_recycle_work, dmabuff_recycle_worker);
	INIT_KFIFO(ctx->dmabuff_recycle);
	INIT_KFIFO(ctx->capture_buffer);

	ctx->post_to_upper_done = true;
	ctx->param_sets_from_ucode = param_sets_from_ucode ? 1 : 0;

	if (enable_drm_mode) {
		ctx->is_drm_mode = true;
		ctx->param_sets_from_ucode = true;
	}

	ctx->type = AML_INST_DECODER;
	ret = aml_vcodec_dec_ctrls_setup(ctx);
	if (ret) {
		v4l_dbg(ctx, V4L_DEBUG_CODEC_ERROR,
			"Failed to setup vcodec controls\n");
		goto err_ctrls_setup;
	}
	ctx->m2m_ctx = v4l2_m2m_ctx_init(dev->m2m_dev_dec, ctx,
		&aml_vcodec_dec_queue_init);
	if (IS_ERR((__force void *)ctx->m2m_ctx)) {
		ret = PTR_ERR((__force void *)ctx->m2m_ctx);
		v4l_dbg(ctx, V4L_DEBUG_CODEC_ERROR,
			"Failed to v4l2_m2m_ctx_init() (%d)\n", ret);
		goto err_m2m_ctx_init;
	}
	src_vq = v4l2_m2m_get_vq(ctx->m2m_ctx,
				V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
	ctx->output_thread_ready = true;
	ctx->empty_flush_buf->vb.vb2_buf.vb2_queue = src_vq;
	ctx->empty_flush_buf->lastframe = true;
	ctx->vdec_pic_info_update = aml_vdec_pic_info_update;
	aml_vcodec_dec_set_default_params(ctx);
	ctx->is_stream_off = true;

	ctx->aux_infos.dv_index = 0;
	ctx->aux_infos.sei_index = 0;
	ctx->aux_infos.alloc_buffer = aml_alloc_buffer;
	ctx->aux_infos.free_buffer = aml_free_buffer;
	ctx->aux_infos.bind_sei_buffer = aml_bind_sei_buffer;
	ctx->aux_infos.bind_dv_buffer = aml_bind_dv_buffer;
	ctx->aux_infos.free_one_sei_buffer = aml_free_one_sei_buffer;

	ret = aml_thread_start(ctx, aml_thread_capture_worker, AML_THREAD_CAPTURE, "cap");
	if (ret) {
		v4l_dbg(ctx, V4L_DEBUG_CODEC_ERROR,
			"Failed to creat capture thread.\n");
		goto err_creat_thread;
	}

	list_add(&ctx->list, &dev->ctx_list);

	mutex_unlock(&dev->dev_mutex);
	v4l_dbg(ctx, V4L_DEBUG_CODEC_PRINFO, "%s decoder %lx\n",
		dev_name(&dev->plat_dev->dev), (ulong)ctx);

	return 0;

	/* Deinit when failure occurred */
err_creat_thread:
	v4l2_m2m_ctx_release(ctx->m2m_ctx);
err_m2m_ctx_init:
	v4l2_ctrl_handler_free(&ctx->ctrl_hdl);
err_ctrls_setup:
	v4l2_fh_del(&ctx->fh);
	v4l2_fh_exit(&ctx->fh);
	vfree(ctx->meta_infos.meta_bufs);
	kfree(ctx->empty_flush_buf);
	kfree(ctx);
	mutex_unlock(&dev->dev_mutex);

	return ret;
}

static int fops_vcodec_release(struct file *file)
{
	struct aml_vcodec_dev *dev = video_drvdata(file);
	struct aml_vcodec_ctx *ctx = fh_to_ctx(file->private_data);

	v4l_dbg(ctx, V4L_DEBUG_CODEC_PRINFO, "release decoder %lx\n", (ulong) ctx);
	mutex_lock(&dev->dev_mutex);

	aml_thread_stop(ctx);
	wait_vcodec_ending(ctx);
	vb2_queue_release(&ctx->m2m_ctx->cap_q_ctx.q);
	vb2_queue_release(&ctx->m2m_ctx->out_q_ctx.q);

	aml_vcodec_dec_release(ctx);
	v4l2_fh_del(&ctx->fh);
	v4l2_fh_exit(&ctx->fh);
	v4l2_ctrl_handler_free(&ctx->ctrl_hdl);

	list_del_init(&ctx->list);

	kfree(ctx->empty_flush_buf);
	kref_put(&ctx->ctx_ref, aml_v4l_ctx_release);
	mutex_unlock(&dev->dev_mutex);
	return 0;
}

static int v4l2video_file_release(struct inode *inode, struct file *file)
{
	v4l_dbg(0, V4L_DEBUG_CODEC_BUFMGR, "file: %lx, data: %lx\n",
		(ulong) file, (ulong) file->private_data);

	if (file->private_data)
		vdec_frame_buffer_release(file->private_data);

	return 0;
}

const struct file_operations v4l2_file_fops = {
	.release = v4l2video_file_release,
};

int v4l2_alloc_fd(int *fd)
{
	struct file *file = NULL;
	int file_fd = get_unused_fd_flags(O_CLOEXEC);

	if (file_fd < 0) {
		v4l_dbg(0, V4L_DEBUG_CODEC_ERROR,
			"get unused fd fail\n");
		return -ENODEV;
	}

	file = anon_inode_getfile("v4l2_meta_file", &v4l2_file_fops, NULL, 0);
	if (IS_ERR(file)) {
		put_unused_fd(file_fd);
		v4l_dbg(0, V4L_DEBUG_CODEC_ERROR,
			"anon_inode_getfile fail\n");
		return -ENODEV;
	}

	file->private_data =
		kzalloc(sizeof(struct file_private_data), GFP_KERNEL);
	if (!file->private_data) {
		v4l_dbg(0, V4L_DEBUG_CODEC_ERROR,
			"alloc priv data faild.\n");
		return -ENOMEM;
	}

	v4l_dbg(0, V4L_DEBUG_CODEC_BUFMGR, "fd %d, file %lx, data: %lx\n",
		file_fd, (ulong) file, (ulong) file->private_data);

	fd_install(file_fd, file);
	*fd = file_fd;

	return 0;
}

extern const struct file_operations v4l2_file_fops;
bool is_v4l2_buf_file(struct file *file)
{
	return file->f_op == &v4l2_file_fops;
}

int v4l2_check_fd(int fd)
{
	struct file *file;

	file = fget(fd);

	if (!file) {
		v4l_dbg(0, V4L_DEBUG_CODEC_ERROR,
			"fget fd %d fail!\n", fd);
		return -EBADF;
	}

	if (!is_v4l2_buf_file(file)) {
		fput(file);
		v4l_dbg(0, V4L_DEBUG_CODEC_ERROR,
			"is_v4l2_buf_file fail!\n");
		return -1;
	}

	fput(file);

	v4l_dbg(0, V4L_DEBUG_CODEC_EXINFO,
		"ioctl ok, comm %s, pid %d\n",
		 current->comm, current->pid);

	return 0;
}

int dmabuf_fd_install_data(int fd, void* data, u32 size)
{
	struct file *file;

	file = fget(fd);

	if (!file) {
		v4l_dbg(0, V4L_DEBUG_CODEC_ERROR,
			"fget fd %d fail!, comm %s, pid %d\n",
			fd, current->comm, current->pid);
		return -EBADF;
	}

	if (!is_v4l2_buf_file(file)) {
		fput(file);
		v4l_dbg(0, V4L_DEBUG_CODEC_ERROR,
			"the buf file checked fail!\n");
		return -EBADF;
	}

	memcpy(file->private_data, data, size);

	fput(file);

	return 0;
}

void* v4l_get_vf_handle(int fd)
{
	struct file *file;
	struct file_private_data *data = NULL;
	void *vf_handle = 0;

	file = fget(fd);

	if (!file) {
		v4l_dbg(0, V4L_DEBUG_CODEC_ERROR,
			"fget fd %d fail!, comm %s, pid %d\n",
			fd, current->comm, current->pid);
		return NULL;
	}

	if (!is_v4l2_buf_file(file)) {
		fput(file);
#if 0
		v4l_dbg(0, V4L_DEBUG_CODEC_ERROR,
			"the buf file checked fail!\n");
#endif
		return NULL;
	}

	data = (struct file_private_data*) file->private_data;
	if (data) {
		vf_handle = &data->vf;
		v4l_dbg(0, V4L_DEBUG_CODEC_BUFMGR, "file: %lx, data: %lx\n",
			(ulong) file, (ulong) data);
	}

	fput(file);

	return vf_handle;
}


static long v4l2_vcodec_ioctl(struct file *file,
			unsigned int cmd,
			ulong arg)
{
	long ret = 0;
	void __user *argp = (void __user *)arg;

	switch (cmd) {
	case V4LVIDEO_IOCTL_ALLOC_FD:
	{
		u32 v4lvideo_fd = 0;

		ret = v4l2_alloc_fd(&v4lvideo_fd);
		if (ret != 0)
			break;
		put_user(v4lvideo_fd, (u32 __user *)argp);
		v4l_dbg(0, V4L_DEBUG_CODEC_EXINFO,
			"V4LVIDEO_IOCTL_ALLOC_FD fd %d\n",
			v4lvideo_fd);
		break;
	}
	case V4LVIDEO_IOCTL_CHECK_FD:
	{
		u32 v4lvideo_fd = 0;

		get_user(v4lvideo_fd, (u32 __user *)argp);
		ret = v4l2_check_fd(v4lvideo_fd);
		if (ret != 0)
			break;
		v4l_dbg(0, V4L_DEBUG_CODEC_EXINFO,
			"V4LVIDEO_IOCTL_CHECK_FD fd %d\n",
			v4lvideo_fd);
		break;
	}
	case V4LVIDEO_IOCTL_SET_CONFIG_PARAMS:
	{
		struct aml_vcodec_ctx *ctx = NULL;

		if (is_v4l2_buf_file(file))
			break;

		ctx = fh_to_ctx(file->private_data);
		if (copy_from_user((void *)&ctx->config,
			(void *)argp, sizeof(ctx->config))) {
			v4l_dbg(0, V4L_DEBUG_CODEC_ERROR,
				"set config parm err\n");
			return -EFAULT;
		}
		break;
	}
	case V4LVIDEO_IOCTL_GET_CONFIG_PARAMS:
	{
		struct aml_vcodec_ctx *ctx = NULL;

		if (is_v4l2_buf_file(file))
			break;

		ctx = fh_to_ctx(file->private_data);
		if (copy_to_user((void *)argp,
			(void *)&ctx->config, sizeof(ctx->config))) {
			v4l_dbg(0, V4L_DEBUG_CODEC_ERROR,
				"get config parm err\n");
			return -EFAULT;
		}
		break;
	}
	default:
		return video_ioctl2(file, cmd, arg);
	}
	return ret;
}

#ifdef CONFIG_COMPAT
static long v4l2_compat_ioctl(struct file *file,
	unsigned int cmd, ulong arg)
{
	long ret = 0;

	ret = v4l2_vcodec_ioctl(file, cmd, (ulong)compat_ptr(arg));
	return ret;
}
#endif

static const struct v4l2_file_operations aml_vcodec_fops = {
	.owner		= THIS_MODULE,
	.open		= fops_vcodec_open,
	.release	= fops_vcodec_release,
	.poll		= v4l2_m2m_fop_poll,
	.unlocked_ioctl = v4l2_vcodec_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = v4l2_compat_ioctl,
#endif
	.mmap		= v4l2_m2m_fop_mmap,
};

static ssize_t status_show(struct class *cls,
	struct class_attribute *attr, char *buf)
{
	struct aml_vcodec_dev *dev = container_of(cls,
		struct aml_vcodec_dev, v4ldec_class);
	struct aml_vcodec_ctx *ctx = NULL;
	char *pbuf = buf;

	mutex_lock(&dev->dev_mutex);

	if (list_empty(&dev->ctx_list)) {
		pbuf += sprintf(pbuf, "No v4ldec context.\n");
		goto out;
	}

	list_for_each_entry(ctx, &dev->ctx_list, list) {
		/* basic information. */
		aml_vdec_basic_information(ctx);

		/* buffers status. */
		aml_buffer_status(ctx);
	}
out:
	mutex_unlock(&dev->dev_mutex);

	return pbuf - buf;
}

static ssize_t dump_path_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	return snprintf(buf, sizeof(dump_path), "%s\n", dump_path);
}

static ssize_t dump_path_store(struct class *class,
		struct class_attribute *attr,
		const char *buf, size_t size)
{
	ssize_t n;
	if (size > sizeof(dump_path))
		pr_err("path too long\n");
	n = snprintf(dump_path, sizeof(dump_path), "%s", buf);
	if (n > 0 && dump_path[n-1] == '\n') {
		dump_path[n-1] = 0;
		pr_info("dump path changed to %s\n", dump_path);
	}
	return size;
}

static CLASS_ATTR_RO(status);
static CLASS_ATTR_RW(dump_path);

static struct attribute *v4ldec_class_attrs[] = {
	&class_attr_status.attr,
	&class_attr_dump_path.attr,
	NULL
};

ATTRIBUTE_GROUPS(v4ldec_class);

static int aml_vcodec_probe(struct platform_device *pdev)
{
	struct aml_vcodec_dev *dev;
	struct video_device *vfd_dec;
	int ret = 0;

	dev = devm_kzalloc(&pdev->dev, sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	INIT_LIST_HEAD(&dev->ctx_list);
	dev->plat_dev = pdev;
	atomic_set(&dev->vpp_count, 0);

	mutex_init(&dev->dec_mutex);
	mutex_init(&dev->dev_mutex);
	spin_lock_init(&dev->irqlock);

	snprintf(dev->v4l2_dev.name, sizeof(dev->v4l2_dev.name), "%s",
		"[/AML_V4L2_VDEC]");

	ret = v4l2_device_register(&pdev->dev, &dev->v4l2_dev);
	if (ret) {
		dev_err(&pdev->dev, "v4l2_device_register err=%d\n", ret);
		goto err_res;
	}

	init_waitqueue_head(&dev->queue);

	vfd_dec = video_device_alloc();
	if (!vfd_dec) {
		dev_err(&pdev->dev, "Failed to allocate video device\n");
		ret = -ENOMEM;
		goto err_dec_alloc;
	}

	vfd_dec->fops		= &aml_vcodec_fops;
	vfd_dec->ioctl_ops	= &aml_vdec_ioctl_ops;
	vfd_dec->release	= video_device_release;
	vfd_dec->lock		= &dev->dev_mutex;
	vfd_dec->v4l2_dev	= &dev->v4l2_dev;
	vfd_dec->vfl_dir	= VFL_DIR_M2M;
	vfd_dec->device_caps	= V4L2_CAP_VIDEO_M2M_MPLANE |
					V4L2_CAP_STREAMING;

	snprintf(vfd_dec->name, sizeof(vfd_dec->name), "%s",
		AML_VCODEC_DEC_NAME);
	video_set_drvdata(vfd_dec, dev);
	dev->vfd_dec = vfd_dec;
	platform_set_drvdata(pdev, dev);

	dev->m2m_dev_dec = v4l2_m2m_init(&aml_vdec_m2m_ops);
	if (IS_ERR((__force void *)dev->m2m_dev_dec)) {
		dev_err(&pdev->dev, "Failed to init mem2mem dec device\n");
		ret = PTR_ERR((__force void *)dev->m2m_dev_dec);
		goto err_dec_mem_init;
	}

	dev->decode_workqueue =
		alloc_ordered_workqueue("output-worker",
			__WQ_LEGACY | WQ_MEM_RECLAIM | WQ_HIGHPRI);
	if (!dev->decode_workqueue) {
		dev_err(&pdev->dev, "Failed to create decode workqueue\n");
		ret = -EINVAL;
		goto err_event_workq;
	}

	//dev_set_name(&vdev->dev, "%s%d", name_base, vdev->num);

	ret = video_register_device(vfd_dec, VFL_TYPE_GRABBER, 26);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register video device\n");
		goto err_dec_reg;
	}

	/*init class*/
	dev->v4ldec_class.name = "v4ldec";
	dev->v4ldec_class.owner = THIS_MODULE;
	dev->v4ldec_class.class_groups = v4ldec_class_groups;
	ret = class_register(&dev->v4ldec_class);
	if (ret) {
		dev_err(&pdev->dev, "v4l dec class create fail.\n");
		goto err_reg_class;
	}

	ret = aml_canvas_cache_init(dev);
	if (ret) {
		dev_err(&pdev->dev, "v4l dec alloc canvas fail.\n");
		goto err_alloc_canvas;
	}

	dev_info(&pdev->dev, "v4ldec registered as /dev/video%d\n", vfd_dec->num);

	return 0;

err_alloc_canvas:
	class_unregister(&dev->v4ldec_class);
err_reg_class:
	video_unregister_device(vfd_dec);
err_dec_reg:
	destroy_workqueue(dev->decode_workqueue);
err_event_workq:
	v4l2_m2m_release(dev->m2m_dev_dec);
err_dec_mem_init:
	video_device_release(vfd_dec);
err_dec_alloc:
	v4l2_device_unregister(&dev->v4l2_dev);
err_res:

	return ret;
}

static int aml_vcodec_dec_remove(struct platform_device *pdev)
{
	struct aml_vcodec_dev *dev = platform_get_drvdata(pdev);

	flush_workqueue(dev->decode_workqueue);
	destroy_workqueue(dev->decode_workqueue);

	class_unregister(&dev->v4ldec_class);

	if (dev->m2m_dev_dec)
		v4l2_m2m_release(dev->m2m_dev_dec);

	if (dev->vfd_dec)
		video_unregister_device(dev->vfd_dec);

	v4l2_device_unregister(&dev->v4l2_dev);

	aml_canvas_cache_put(dev);

	dev_info(&pdev->dev, "v4ldec removed.\n");

	return 0;
}

static const struct of_device_id aml_vcodec_match[] = {
	{.compatible = "amlogic, vcodec-dec",},
	{},
};

MODULE_DEVICE_TABLE(of, aml_vcodec_match);

static struct platform_driver aml_vcodec_dec_driver = {
	.probe	= aml_vcodec_probe,
	.remove	= aml_vcodec_dec_remove,
	.driver	= {
		.name	= AML_VCODEC_DEC_NAME,
		.of_match_table = aml_vcodec_match,
	},
};

static int __init amvdec_ports_init(void)
{
	pr_info("v4l dec module init\n");

	if (platform_driver_register(&aml_vcodec_dec_driver)) {
		pr_err("failed to register v4l dec driver\n");
		return -ENODEV;
	}

	return 0;
}

static void __exit amvdec_ports_exit(void)
{
	pr_info("v4l dec module exit\n");

	platform_driver_unregister(&aml_vcodec_dec_driver);
}

module_init(amvdec_ports_init);
module_exit(amvdec_ports_exit);

u32 debug_mode;
EXPORT_SYMBOL(debug_mode);
module_param(debug_mode, uint, 0644);

u32 disable_vpp_dw_mmu;
EXPORT_SYMBOL(disable_vpp_dw_mmu);
module_param(disable_vpp_dw_mmu, uint, 0644);

bool aml_set_vfm_enable;
EXPORT_SYMBOL(aml_set_vfm_enable);
module_param(aml_set_vfm_enable, bool, 0644);

int aml_set_vfm_path;
EXPORT_SYMBOL(aml_set_vfm_path);
module_param(aml_set_vfm_path, int, 0644);

bool aml_set_vdec_type_enable;
EXPORT_SYMBOL(aml_set_vdec_type_enable);
module_param(aml_set_vdec_type_enable, bool, 0644);

int aml_set_vdec_type;
EXPORT_SYMBOL(aml_set_vdec_type);
module_param(aml_set_vdec_type, int, 0644);

int vp9_need_prefix;
EXPORT_SYMBOL(vp9_need_prefix);
module_param(vp9_need_prefix, int, 0644);

int av1_need_prefix;
EXPORT_SYMBOL(av1_need_prefix);
module_param(av1_need_prefix, int, 0644);

bool multiplanar;
EXPORT_SYMBOL(multiplanar);
module_param(multiplanar, bool, 0644);

int dump_capture_frame;
EXPORT_SYMBOL(dump_capture_frame);
module_param(dump_capture_frame, int, 0644);

int dump_vpp_input;
EXPORT_SYMBOL(dump_vpp_input);
module_param(dump_vpp_input, int, 0644);

int dump_ge2d_input;
EXPORT_SYMBOL(dump_ge2d_input);
module_param(dump_ge2d_input, int, 0644);

int dump_output_frame;
EXPORT_SYMBOL(dump_output_frame);
module_param(dump_output_frame, int, 0644);

u32 dump_output_start_position;
EXPORT_SYMBOL(dump_output_start_position);
module_param(dump_output_start_position, uint, 0644);

EXPORT_SYMBOL(param_sets_from_ucode);
module_param(param_sets_from_ucode, bool, 0644);

EXPORT_SYMBOL(enable_drm_mode);
module_param(enable_drm_mode, bool, 0644);

int bypass_vpp;
EXPORT_SYMBOL(bypass_vpp);
module_param(bypass_vpp, int, 0644);

int bypass_ge2d;
EXPORT_SYMBOL(bypass_ge2d);
module_param(bypass_ge2d, int, 0644);

int max_di_instance = 2;
EXPORT_SYMBOL(max_di_instance);
module_param(max_di_instance, int, 0644);

int bypass_progressive = 1;
EXPORT_SYMBOL(bypass_progressive);
module_param(bypass_progressive, int, 0644);

bool support_mjpeg;
EXPORT_SYMBOL(support_mjpeg);
module_param(support_mjpeg, bool, 0644);

bool support_format_I420;
EXPORT_SYMBOL(support_format_I420);
module_param(support_format_I420, bool, 0644);

int force_enable_nr;
EXPORT_SYMBOL(force_enable_nr);
module_param(force_enable_nr, int, 0644);

int force_enable_di_local_buffer;
EXPORT_SYMBOL(force_enable_di_local_buffer);
module_param(force_enable_di_local_buffer, int, 0644);

int vpp_bypass_frames;
EXPORT_SYMBOL(vpp_bypass_frames);
module_param(vpp_bypass_frames, int, 0644);

int bypass_nr_flag;
EXPORT_SYMBOL(bypass_nr_flag);
module_param(bypass_nr_flag, int, 0644);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("AML video codec V4L2 decoder driver");

