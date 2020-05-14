// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/of.h>
#include <linux/rbtree.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/dma-buf.h>
#include <linux/pagemap.h>
#include <linux/ion.h>

#include "meson_uvm_allocator.h"

static struct mua_device *mdev;

static int enable_screencap;
module_param_named(enable_screencap, enable_screencap, int, 0664);
static int mua_debug_level;
module_param(mua_debug_level, int, 0644);

#define MUA_PRINTK(level, fmt, arg...) \
	do {	\
		if (mua_debug_level >= (level))     \
			pr_info("MUA: " fmt, ## arg); \
	} while (0)

static void mua_handle_free(struct uvm_buf_obj *obj)
{
	struct mua_buffer *buffer;
	struct ion_buffer *ibuffer;

	buffer = container_of(obj, struct mua_buffer, base);
	ibuffer = buffer->ibuffer;
	MUA_PRINTK(1, "%s get ion_buffer, %px\n", __func__, obj);
	if (ibuffer) {
		MUA_PRINTK(1, "%s called ion_free\n", __func__);
		ion_free(ibuffer);
	}
	kfree(buffer);
}

static int meson_uvm_fill_pattern(struct mua_buffer *buffer, void *vaddr)
{
	struct v4l_data_t val_data;

	memset(&val_data, 0, sizeof(val_data));

	val_data.vf = buffer->vf;
	val_data.dst_addr = vaddr;
	val_data.byte_stride = buffer->byte_stride;
	val_data.width = buffer->width;
	val_data.height = buffer->height;
	val_data.phy_addr[0] = buffer->paddr;

	if (!buffer->index || buffer->index != buffer->vf->omx_index) {
		v4lvideo_data_copy(&val_data);
		buffer->index = buffer->vf->omx_index;
	}

	vunmap(vaddr);
	return 0;
}

static int mua_process_delay_alloc(struct dma_buf *dmabuf,
				   struct uvm_buf_obj *obj)
{
	int i, j, num_pages;
	struct dma_buf *idmabuf;
	struct ion_buffer *ibuffer;
	struct uvm_alloc_info info;
	struct mua_buffer *buffer;
	struct page *page;
	struct page **tmp;
	struct page **page_array;
	pgprot_t pgprot;
	void *vaddr;
	struct sg_table *src_sgt = NULL;
	struct scatterlist *sg = NULL;

	buffer = container_of(obj, struct mua_buffer, base);
	memset(&info, 0, sizeof(info));

	MUA_PRINTK(1, "%p, %d.\n", __func__, __LINE__);

	if (!enable_screencap && current->tgid == mdev->pid &&
	    buffer->commit_display) {
		MUA_PRINTK(0, "screen cap should not access the uvm buffer.\n");
		return -ENODEV;
	}

	if (!buffer->ibuffer) {
		idmabuf = ion_alloc(dmabuf->size,
				    (1 << ION_HEAP_TYPE_CUSTOM), 0);
		if (IS_ERR(idmabuf)) {
			MUA_PRINTK(0, "%s: ion_alloc fail.\n", __func__);
			return -ENOMEM;
		}

		ibuffer = idmabuf->priv;
		if (ibuffer) {
			src_sgt = ibuffer->sg_table;
			page = sg_page(src_sgt->sgl);
			buffer->paddr = PFN_PHYS(page_to_pfn(page));
			buffer->ibuffer = ibuffer;
			buffer->sg_table = ibuffer->sg_table;

			info.sgt = src_sgt;
			dmabuf_bind_uvm_delay_alloc(dmabuf, &info);
		}
	}

	//start to do vmap
	if (!buffer->sg_table) {
		MUA_PRINTK(0, "none uvm buffer allocated.\n");
		return -ENODEV;
	}

	src_sgt = buffer->sg_table;
	num_pages = PAGE_ALIGN(buffer->size) / PAGE_SIZE;
	tmp = vmalloc(sizeof(struct page *) * num_pages);
	page_array = tmp;

	pgprot = pgprot_writecombine(PAGE_KERNEL);

	for_each_sg(src_sgt->sgl, sg, src_sgt->nents, i) {
		int npages_this_entry =
			PAGE_ALIGN(sg->length) / PAGE_SIZE;
		struct page *page = sg_page(sg);

		for (j = 0; j < npages_this_entry; j++)
			*(tmp++) = page++;
	}

	vaddr = vmap(page_array, num_pages, VM_MAP, pgprot);
	if (!vaddr) {
		MUA_PRINTK(0, "vmap fail, size: %d\n",
			   num_pages << PAGE_SHIFT);
		vfree(page_array);
		return -ENOMEM;
	}
	vfree(page_array);
	MUA_PRINTK(1, "buffer vaddr: %p.\n", vaddr);

	//start to filldata
	meson_uvm_fill_pattern(buffer, vaddr);

	return 0;
}

static int mua_handle_alloc(struct dma_buf *dmabuf, struct uvm_alloc_data *data)
{
	struct mua_buffer *buffer;
	struct uvm_alloc_info info;
	struct dma_buf *idmabuf;
	struct ion_buffer *ibuffer;

	memset(&info, 0, sizeof(info));
	buffer = kzalloc(sizeof(*buffer), GFP_KERNEL);
	buffer->size = data->size;
	buffer->dev = mdev;
	buffer->byte_stride = data->byte_stride;
	buffer->width = data->width;
	buffer->height = data->height;

	if (data->flags & MUA_IMM_ALLOC) {
		idmabuf = ion_alloc(data->size,
				    (1 << ION_HEAP_TYPE_CUSTOM), 0);
		if (IS_ERR(idmabuf)) {
			MUA_PRINTK(0, "%s: ion_alloc fail.\n", __func__);
			kfree(buffer);
			return -ENOMEM;
		}

		ibuffer = idmabuf->priv;
		buffer->ibuffer = ibuffer;
		info.sgt = ibuffer->sg_table;
		info.obj = &buffer->base;
		info.flags = data->flags;
		info.size = data->size;
		info.free = mua_handle_free;
		MUA_PRINTK(1, "UVM FLAGS is MUA_IMM_ALLOC, %px\n", info.obj);
	} else if (data->flags & MUA_DELAY_ALLOC) {
		info.size = data->size;
		info.obj = &buffer->base;
		info.flags = data->flags;
		info.delay_alloc = mua_process_delay_alloc;
		info.free = mua_handle_free;
		MUA_PRINTK(1, "UVM FLAGS is MUA_DELAY_ALLOC, %px\n", info.obj);

		buffer->vf = v4lvideo_get_vf(data->v4l2_fd);
		if (!buffer->vf)
			MUA_PRINTK(0, "v4lvideo_get_vf failed.\n");
	} else {
		MUA_PRINTK(0, "unsupported MUA FLAGS.\n");
		kfree(buffer);
		return -EINVAL;
	}

	dmabuf_bind_uvm_alloc(dmabuf, &info);

	return 0;
}

static int mua_set_commit_display(int fd, int commit_display)
{
	struct dma_buf *dmabuf;
	struct uvm_buf_obj *obj;
	struct mua_buffer *buffer;

	dmabuf = dma_buf_get(fd);
	if (IS_ERR_OR_NULL(dmabuf)) {
		MUA_PRINTK(0, "invalid dmabuf fd\n");
		return -EINVAL;
	}

	obj = dmabuf_get_uvm_buf_obj(dmabuf);
	buffer = container_of(obj, struct mua_buffer, base);
	buffer->commit_display = commit_display;

	dma_buf_put(dmabuf);
	return 0;
}

static long mua_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct mua_device *md;
	union uvm_ioctl_arg data;
	struct dma_buf *dmabuf;
	int pid;
	int ret = 0;
	int fd = 0;

	md = file->private_data;

	if (_IOC_SIZE(cmd) > sizeof(data))
		return -EINVAL;

	if (copy_from_user(&data, (void __user *)arg, _IOC_SIZE(cmd)))
		return -EFAULT;

	switch (cmd) {
	case UVM_IOC_ALLOC:
		data.alloc_data.size = PAGE_ALIGN(data.alloc_data.size);
		dmabuf = uvm_alloc_dmabuf(data.alloc_data.size,
					  data.alloc_data.align,
					  data.alloc_data.flags);
		if (IS_ERR(dmabuf))
			return -ENOMEM;

		ret = mua_handle_alloc(dmabuf, &data.alloc_data);
		if (ret < 0) {
			dma_buf_put(dmabuf);
			return -ENOMEM;
		}

		fd = dma_buf_fd(dmabuf, O_CLOEXEC);
		if (fd < 0) {
			dma_buf_put(dmabuf);
			return -ENOMEM;
		}

		data.alloc_data.fd = fd;

		if (copy_to_user((void __user *)arg, &data, _IOC_SIZE(cmd)))
			return -EFAULT;

		break;
	case UVM_IOC_SET_PID:
		pid = data.pid_data.pid;
		if (pid < 0)
			return -ENOMEM;

		md->pid = pid;
		break;
	case UVM_IOC_SET_FD:
		fd = data.fd_data.fd;
		ret = mua_set_commit_display(fd, data.fd_data.commit_display);

		if (ret < 0) {
			MUA_PRINTK(0, "invalid dmabuf fd.\n");
			return -EINVAL;
		}
		break;
	default:
		return -ENOTTY;
	}

	return ret;
}

static int mua_open(struct inode *inode, struct file *file)
{
	struct miscdevice *miscdev = file->private_data;
	struct mua_device *md = container_of(miscdev, struct mua_device, dev);

	MUA_PRINTK(1, "%s: %d\n", __func__, __LINE__);
	file->private_data = md;

	return 0;
}

static int mua_release(struct inode *inode, struct file *file)
{
	return 0;
}

static const struct file_operations mua_fops = {
	.owner = THIS_MODULE,
	.open = mua_open,
	.release = mua_release,
	.unlocked_ioctl = mua_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = mua_ioctl,
#endif
};

static int mua_probe(struct platform_device *pdev)
{
	mdev = kzalloc(sizeof(*mdev), GFP_KERNEL);
	if (!mdev)
		return -ENOMEM;

	mdev->dev.minor = MISC_DYNAMIC_MINOR;
	mdev->dev.name = "uvm";
	mdev->dev.fops = &mua_fops;
	mutex_init(&mdev->buffer_lock);

	return misc_register(&mdev->dev);
}

static int mua_remove(struct platform_device *pdev)
{
	misc_deregister(&mdev->dev);
	return 0;
}

static const struct of_device_id mua_match[] = {
	{.compatible = "amlogic, meson_uvm",},
	{},
};

static struct platform_driver mua_driver = {
	.driver = {
		.name = "meson_uvm_allocator",
		.owner = THIS_MODULE,
		.of_match_table = mua_match,
	},
	.probe = mua_probe,
	.remove = mua_remove,
};

int __init mua_init(void)
{
	return platform_driver_register(&mua_driver);
}

void __exit mua_exit(void)
{
	platform_driver_unregister(&mua_driver);
}

#ifndef MODULE
module_init(mua_init);
module_exit(mua_exit);
#endif
//MODULE_DESCRIPTION("AMLOGIC uvm dmabuf allocator device driver");
//MODULE_LICENSE("GPL");
