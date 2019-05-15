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
* FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
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

#include "aml_vcodec_drv.h"
#include "aml_vcodec_dec.h"
#include "aml_vcodec_dec_pm.h"
//#include "aml_vcodec_intr.h"
#include "aml_vcodec_util.h"
#include "aml_vcodec_vfm.h"

#define VDEC_HW_ACTIVE	0x10
#define VDEC_IRQ_CFG	0x11
#define VDEC_IRQ_CLR	0x10
#define VDEC_IRQ_CFG_REG	0xa4

module_param(aml_v4l2_dbg_level, int, 0644);
module_param(aml_vcodec_dbg, bool, 0644);

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
	aml_buf = kzalloc(sizeof(*aml_buf), GFP_KERNEL);
	if (!aml_buf) {
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
	INIT_LIST_HEAD(&ctx->capture_list);
	INIT_LIST_HEAD(&ctx->vdec_thread_list);
	dev->filp = file;
	ctx->dev = dev;
	init_waitqueue_head(&ctx->queue);
	mutex_init(&ctx->state_lock);
	mutex_init(&ctx->lock);
	init_waitqueue_head(&ctx->wq);

	ctx->type = AML_INST_DECODER;
	ret = aml_vcodec_dec_ctrls_setup(ctx);
	if (ret) {
		aml_v4l2_err("Failed to setup vcodec controls");
		goto err_ctrls_setup;
	}
	ctx->m2m_ctx = v4l2_m2m_ctx_init(dev->m2m_dev_dec, ctx,
		&aml_vcodec_dec_queue_init);
	if (IS_ERR((__force void *)ctx->m2m_ctx)) {
		ret = PTR_ERR((__force void *)ctx->m2m_ctx);
		aml_v4l2_err("Failed to v4l2_m2m_ctx_init() (%d)", ret);
		goto err_m2m_ctx_init;
	}
	src_vq = v4l2_m2m_get_vq(ctx->m2m_ctx,
				V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
	ctx->empty_flush_buf->vb.vb2_buf.vb2_queue = src_vq;
	ctx->empty_flush_buf->lastframe = true;
	aml_vcodec_dec_set_default_params(ctx);

	ret = aml_thread_start(ctx, try_to_capture, AML_THREAD_CAPTURE, "cap");
	if (ret) {
		aml_v4l2_err("Failed to creat capture thread.");
		goto err_creat_thread;
	}

	list_add(&ctx->list, &dev->ctx_list);

	mutex_unlock(&dev->dev_mutex);
	pr_info("[%d] %s decoder\n", ctx->id, dev_name(&dev->plat_dev->dev));

	return ret;

	/* Deinit when failure occurred */
err_creat_thread:
	v4l2_m2m_ctx_release(ctx->m2m_ctx);
err_m2m_ctx_init:
	v4l2_ctrl_handler_free(&ctx->ctrl_hdl);
err_ctrls_setup:
	v4l2_fh_del(&ctx->fh);
	v4l2_fh_exit(&ctx->fh);
	kfree(ctx->empty_flush_buf);
	kfree(ctx);
	mutex_unlock(&dev->dev_mutex);

	return ret;
}

static int fops_vcodec_release(struct file *file)
{
	struct aml_vcodec_dev *dev = video_drvdata(file);
	struct aml_vcodec_ctx *ctx = fh_to_ctx(file->private_data);

	pr_info("[%d] release decoder\n", ctx->id);
	mutex_lock(&dev->dev_mutex);

	/*
	 * Call v4l2_m2m_ctx_release before aml_vcodec_dec_release. First, it
	 * makes sure the worker thread is not running after vdec_if_deinit.
	 * Second, the decoder will be flushed and all the buffers will be
	 * returned in stop_streaming.
	 */
	aml_vcodec_dec_release(ctx);
	v4l2_m2m_ctx_release(ctx->m2m_ctx);

	v4l2_fh_del(&ctx->fh);
	v4l2_fh_exit(&ctx->fh);
	v4l2_ctrl_handler_free(&ctx->ctrl_hdl);

	aml_thread_stop(ctx);

	list_del_init(&ctx->list);
	kfree(ctx->empty_flush_buf);
	kfree(ctx);
	mutex_unlock(&dev->dev_mutex);
	return 0;
}

static const struct v4l2_file_operations aml_vcodec_fops = {
	.owner		= THIS_MODULE,
	.open		= fops_vcodec_open,
	.release	= fops_vcodec_release,
	.poll		= v4l2_m2m_fop_poll,
	.unlocked_ioctl	= video_ioctl2,
	.mmap		= v4l2_m2m_fop_mmap,
};

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

	mutex_init(&dev->dec_mutex);
	mutex_init(&dev->dev_mutex);
	spin_lock_init(&dev->irqlock);

	snprintf(dev->v4l2_dev.name, sizeof(dev->v4l2_dev.name), "%s",
		"[/AML_V4L2_VDEC]");

	ret = v4l2_device_register(&pdev->dev, &dev->v4l2_dev);
	if (ret) {
		aml_v4l2_err("v4l2_device_register err=%d", ret);
		goto err_res;
	}

	init_waitqueue_head(&dev->queue);

	vfd_dec = video_device_alloc();
	if (!vfd_dec) {
		aml_v4l2_err("Failed to allocate video device");
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
		aml_v4l2_err("Failed to init mem2mem dec device");
		ret = PTR_ERR((__force void *)dev->m2m_dev_dec);
		goto err_dec_mem_init;
	}

	dev->decode_workqueue =
		alloc_ordered_workqueue(AML_VCODEC_DEC_NAME,
			WQ_MEM_RECLAIM | WQ_FREEZABLE);
	if (!dev->decode_workqueue) {
		aml_v4l2_err("Failed to create decode workqueue");
		ret = -EINVAL;
		goto err_event_workq;
	}

	dev->reset_workqueue =
		alloc_ordered_workqueue("aml-vcodec-reset",
			WQ_MEM_RECLAIM | WQ_FREEZABLE);
	if (!dev->reset_workqueue) {
		aml_v4l2_err("Failed to create decode workqueue");
		ret = -EINVAL;
		destroy_workqueue(dev->decode_workqueue);
		goto err_event_workq;
	}

	//dev_set_name(&vdev->dev, "%s%d", name_base, vdev->num);

	ret = video_register_device(vfd_dec, VFL_TYPE_GRABBER, 26);
	if (ret) {
		pr_err("Failed to register video device\n");
		goto err_dec_reg;
	}

	pr_info("decoder registered as /dev/video%d\n", vfd_dec->num);

	return 0;

err_dec_reg:
	destroy_workqueue(dev->reset_workqueue);
	destroy_workqueue(dev->decode_workqueue);
err_event_workq:
	v4l2_m2m_release(dev->m2m_dev_dec);
err_dec_mem_init:
	video_unregister_device(vfd_dec);
err_dec_alloc:
	v4l2_device_unregister(&dev->v4l2_dev);
err_res:

	return ret;
}

static const struct of_device_id aml_vcodec_match[] = {
	{.compatible = "amlogic, vcodec-dec",},
	{},
};

MODULE_DEVICE_TABLE(of, aml_vcodec_match);

static int aml_vcodec_dec_remove(struct platform_device *pdev)
{
	struct aml_vcodec_dev *dev = platform_get_drvdata(pdev);

	flush_workqueue(dev->reset_workqueue);
	destroy_workqueue(dev->reset_workqueue);

	flush_workqueue(dev->decode_workqueue);
	destroy_workqueue(dev->decode_workqueue);

	if (dev->m2m_dev_dec)
		v4l2_m2m_release(dev->m2m_dev_dec);

	if (dev->vfd_dec)
		video_unregister_device(dev->vfd_dec);

	v4l2_device_unregister(&dev->v4l2_dev);

	return 0;
}

/*static void aml_vcodec_dev_release(struct device *dev)
{
}*/

static struct platform_driver aml_vcodec_dec_driver = {
	.probe	= aml_vcodec_probe,
	.remove	= aml_vcodec_dec_remove,
	.driver	= {
		.name	= AML_VCODEC_DEC_NAME,
		.of_match_table = aml_vcodec_match,
	},
};

/*
static struct platform_device aml_vcodec_dec_device = {
	.name		= AML_VCODEC_DEC_NAME,
	.dev.release	= aml_vcodec_dev_release,
};*/

module_platform_driver(aml_vcodec_dec_driver);

/*
static int __init amvdec_ports_init(void)
{
	int ret;

	ret = platform_device_register(&aml_vcodec_dec_device);
	if (ret)
		return ret;

	ret = platform_driver_register(&aml_vcodec_dec_driver);
	if (ret)
		platform_device_unregister(&aml_vcodec_dec_device);

	return ret;
}

static void __exit amvdec_ports_exit(void)
{
	platform_driver_unregister(&aml_vcodec_dec_driver);
	platform_device_unregister(&aml_vcodec_dec_device);
}

module_init(amvdec_ports_init);
module_exit(amvdec_ports_exit);
*/

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("AML video codec V4L2 decoder driver");
