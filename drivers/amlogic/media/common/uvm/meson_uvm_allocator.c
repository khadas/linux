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
#include <linux/meson_ion.h>

#include <linux/amlogic/media/meson_uvm_allocator.h>
#include "meson_uvm_nn_processor.h"
#include "meson_uvm_aipq_processor.h"
#include "meson_uvm_dalton_processor.h"
#include "meson_uvm_aiface_processor.h"
#include "meson_uvm_aicolor_processor.h"
#include "meson_uvm_buffer_info.h"

static struct mua_device *mdev;

static int enable_screencap;
module_param_named(enable_screencap, enable_screencap, int, 0664);
static int mua_debug_level = MUA_ERROR;
module_param(mua_debug_level, int, 0644);

#define MUA_PRINTK(level, fmt, arg...) \
	do {	\
		if (mua_debug_level & (level))     \
			pr_info("MUA: " fmt, ## arg); \
	} while (0)

static bool mua_is_valid_dmabuf(int fd)
{
	struct dma_buf *dmabuf = NULL;

	dmabuf = dma_buf_get(fd);
	if (IS_ERR_OR_NULL(dmabuf)) {
		MUA_PRINTK(MUA_ERROR, "invalid dmabuf. %s %d\n",
			   __func__, __LINE__);
		return false;
	}

	if (!dmabuf_is_uvm(dmabuf)) {
		MUA_PRINTK(MUA_ERROR, "dmabuf is not uvm. %s %d\n", __func__, __LINE__);
		dma_buf_put(dmabuf);
		return false;
	}

	dma_buf_put(dmabuf);
	return true;
}

static void mua_handle_free(struct uvm_buf_obj *obj)
{
	struct mua_buffer *buffer;
	struct ion_buffer *ibuffer[2];
	struct dma_buf *idmabuf[2];
	int buf_cnt = 0;

	buffer = container_of(obj, struct mua_buffer, base);
	if (!buffer)
		return;

	ibuffer[0] = buffer->ibuffer[0];
	ibuffer[1] = buffer->ibuffer[1];
	idmabuf[0] = buffer->idmabuf[0];
	idmabuf[1] = buffer->idmabuf[1];
	MUA_PRINTK(MUA_INFO, "%s idmabuf[0]=%px idmabuf[1]=%px\n",
		   __func__, idmabuf[0], idmabuf[1]);
	MUA_PRINTK(MUA_INFO, "%s ibuffer[0]=%px ibuffer[1]=%px\n",
		   __func__, ibuffer[0], ibuffer[1]);

	while (buf_cnt < 2 && ibuffer[buf_cnt] && idmabuf[buf_cnt]) {
		MUA_PRINTK(MUA_INFO, "%s free idmabuf[%d]\n", __func__, buf_cnt);
		dma_buf_put(idmabuf[buf_cnt]);
		buf_cnt++;
	}

	kfree(buffer);
}

static int meson_uvm_fill_pattern(struct mua_buffer *buffer, struct dma_buf *dmabuf, void *vaddr)
{
	struct v4l_data_t val_data;

	memset(&val_data, 0, sizeof(val_data));

	val_data.dst_addr = vaddr;
	val_data.byte_stride = buffer->byte_stride;
	val_data.width = buffer->width;
	val_data.height = buffer->height;
	val_data.phy_addr[0] = buffer->paddr;
	MUA_PRINTK(MUA_INFO, "%s. width=%d height=%d byte_stride=%d align=%d\n",
			__func__, buffer->width, buffer->height,
			buffer->byte_stride, buffer->align);
	v4lvideo_data_copy(&val_data, dmabuf, buffer->align);

	vunmap(vaddr);
	return 0;
}

static int mua_process_gpu_realloc(struct dma_buf *dmabuf,
				   struct uvm_buf_obj *obj, int scalar)
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
	bool skip_fill_buf = false;
	struct sg_table *src_sgt = NULL;
	struct scatterlist *sg = NULL;
	unsigned int id = meson_ion_cma_heap_id_get();
	size_t pre_size = 0;
	size_t new_size = 0;

	buffer = container_of(obj, struct mua_buffer, base);
	MUA_PRINTK(MUA_INFO, "%s. buf_scalar=%d WxH: %dx%d\n",
				__func__, scalar, buffer->width, buffer->height);
	memset(&info, 0, sizeof(info));

	MUA_PRINTK(MUA_INFO, "%s, current->tgid:%d mdev->pid:%d buffer->commit_display:%d.\n",
		__func__, current->tgid, mdev->pid, buffer->commit_display);

	if (!enable_screencap && current->tgid == mdev->pid &&
	    buffer->commit_display) {
		MUA_PRINTK(MUA_DBG, "gpu_realloc: screen cap should not access the uvm buffer.\n");
		skip_fill_buf = true;
		//return -ENODEV;
	}

	if (buffer->idmabuf[1])
		pre_size = buffer->idmabuf[1]->size;
	else
		pre_size = dmabuf->size;

	if (!skip_fill_buf)
		new_size = buffer->size * scalar * scalar;
	else
		new_size = buffer->byte_stride * ALIGN(buffer->height, buffer->align) * 3 / 2;

	MUA_PRINTK(1, "buffer->size:%zu realloc new_size=%zu, pre_size = %zu\n",
			buffer->size, new_size, pre_size);

	if (pre_size != new_size && buffer->idmabuf[1]) {
		dma_buf_put(buffer->idmabuf[1]);
		buffer->idmabuf[1] = NULL;
	}

	if (!buffer->idmabuf[1]) {
		id = meson_ion_codecmm_heap_id_get();
		idmabuf = ion_alloc(new_size,
				    (1 << id), ION_FLAG_EXTEND_MESON_HEAP);
		if (IS_ERR(idmabuf)) {
			MUA_PRINTK(MUA_ERROR, "%s: ion_alloc fail.\n", __func__);
			return -ENOMEM;
		}
		MUA_PRINTK(MUA_INFO, "%s: ion(%p) alloc success.\n", __func__, idmabuf);

		ibuffer = idmabuf->priv;
		if (ibuffer) {
			src_sgt = ibuffer->sg_table;
			page = sg_page(src_sgt->sgl);
			buffer->paddr = PFN_PHYS(page_to_pfn(page));
			buffer->ibuffer[1] = ibuffer;
			buffer->idmabuf[1] = idmabuf;
			buffer->sg_table = ibuffer->sg_table;

			info.sgt = src_sgt;
			dmabuf_bind_uvm_delay_alloc(dmabuf, &info);
		}
	}

	//start to do vmap
	if (!buffer->sg_table) {
		MUA_PRINTK(MUA_ERROR, "none uvm buffer allocated.\n");
		return -ENODEV;
	}
	src_sgt = buffer->sg_table;
	num_pages = PAGE_ALIGN(new_size) / PAGE_SIZE;
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
		MUA_PRINTK(MUA_ERROR, "vmap fail, size: %d\n",
			   num_pages << PAGE_SHIFT);
		vfree(page_array);
		return -ENOMEM;
	}
	vfree(page_array);
	MUA_PRINTK(MUA_INFO, "buffer vaddr: %p.\n", vaddr);

	if (skip_fill_buf) {
		size_t buf_size = new_size * 2 / 3;

		MUA_PRINTK(MUA_INFO, "%s buf size=%zu\n", __func__, buf_size);
		memset(vaddr, 0x15, buf_size);
		memset(vaddr + buf_size, 0x80, buf_size / 2);

		vunmap(vaddr);
	} else {
		//start to filldata
		meson_uvm_fill_pattern(buffer, dmabuf, vaddr);
	}

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
	unsigned int ion_flags = 0;
	unsigned int id = meson_ion_cma_heap_id_get();

	buffer = container_of(obj, struct mua_buffer, base);
	memset(&info, 0, sizeof(info));

	MUA_PRINTK(MUA_INFO, "%p, %d.\n", __func__, __LINE__);

	if (!enable_screencap && current->tgid == mdev->pid &&
	    buffer->commit_display) {
		MUA_PRINTK(MUA_ERROR, "delay_alloc: skip delay alloc.\n");
		return -ENODEV;
	}

	if (!buffer->ibuffer[0]) {
		if (buffer->ion_flags & MUA_USAGE_PROTECTED)
			ion_flags |= ION_FLAG_PROTECTED;
		ion_flags |= ION_FLAG_EXTEND_MESON_HEAP;
		id = meson_ion_codecmm_heap_id_get();
		idmabuf = ion_alloc(dmabuf->size,
				    (1 << id), ion_flags);
		if (IS_ERR(idmabuf)) {
			MUA_PRINTK(MUA_ERROR, "%s: ion_alloc fail.\n", __func__);
			return -ENOMEM;
		}

		ibuffer = idmabuf->priv;
		if (ibuffer) {
			src_sgt = ibuffer->sg_table;
			page = sg_page(src_sgt->sgl);
			buffer->paddr = PFN_PHYS(page_to_pfn(page));
			buffer->ibuffer[0] = ibuffer;
			buffer->sg_table = ibuffer->sg_table;
			buffer->idmabuf[0] = idmabuf;
			info.sgt = src_sgt;
			dmabuf_bind_uvm_delay_alloc(dmabuf, &info);
		}
	}

	//start to do vmap
	if (!buffer->sg_table) {
		MUA_PRINTK(MUA_ERROR, "none uvm buffer allocated.\n");
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
		MUA_PRINTK(MUA_ERROR, "vmap fail, size: %d\n",
			   num_pages << PAGE_SHIFT);
		vfree(page_array);
		return -ENOMEM;
	}
	vfree(page_array);
	MUA_PRINTK(MUA_INFO, "buffer vaddr: %p.\n", vaddr);

	//start to filldata
	meson_uvm_fill_pattern(buffer, dmabuf, vaddr);

	return 0;
}

static int mua_handle_alloc(struct dma_buf *dmabuf, struct uvm_alloc_data *data, int alloc_buf_size)
{
	struct mua_buffer *buffer;
	struct uvm_alloc_info info;
	struct dma_buf *idmabuf;
	struct ion_buffer *ibuffer;
	unsigned int ion_flags = 0;
	unsigned int id = meson_ion_cma_heap_id_get();

	memset(&info, 0, sizeof(info));
	buffer = kzalloc(sizeof(*buffer), GFP_KERNEL);
	buffer->size = alloc_buf_size;
	buffer->dev = mdev;
	buffer->byte_stride = data->byte_stride;
	buffer->width = data->width;
	buffer->height = data->height;
	buffer->ion_flags = data->flags;
	buffer->align = data->align;

	if (data->flags & MUA_USAGE_PROTECTED)
		ion_flags |= ION_FLAG_PROTECTED;

	if (data->flags & MUA_IMM_ALLOC) {
		ion_flags |= ION_FLAG_EXTEND_MESON_HEAP;
		id = meson_ion_codecmm_heap_id_get();
		idmabuf = ion_alloc(alloc_buf_size,
				    (1 << id), ion_flags);
		if (IS_ERR(idmabuf)) {
			MUA_PRINTK(MUA_ERROR, "%s: ion_alloc fail.\n", __func__);
			kfree(buffer);
			return -ENOMEM;
		}

		ibuffer = idmabuf->priv;
		buffer->ibuffer[0] = ibuffer;
		buffer->ibuffer[1] = NULL;
		buffer->idmabuf[0] = idmabuf;
		buffer->idmabuf[1] = NULL;
		info.sgt = ibuffer->sg_table;
		info.obj = &buffer->base;
		info.flags = data->flags;
		info.size = alloc_buf_size;
		info.scalar = data->scalar;
		info.gpu_realloc = mua_process_gpu_realloc;
		info.free = mua_handle_free;
		MUA_PRINTK(MUA_INFO, "UVM FLAGS is MUA_IMM_ALLOC, %px  sgt = %px\n",
			   info.obj, info.sgt);
	} else if (data->flags & MUA_DELAY_ALLOC) {
		info.size = data->size;
		info.obj = &buffer->base;
		info.flags = data->flags;
		info.delay_alloc = mua_process_delay_alloc;
		info.free = mua_handle_free;
		MUA_PRINTK(MUA_INFO, "UVM FLAGS is MUA_DELAY_ALLOC, %px\n", info.obj);
	} else {
		MUA_PRINTK(MUA_ERROR, "unsupported MUA FLAGS.\n");
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
		MUA_PRINTK(MUA_ERROR, "invalid dmabuf fd\n");
		return -EINVAL;
	}

	obj = dmabuf_get_uvm_buf_obj(dmabuf);
	buffer = container_of(obj, struct mua_buffer, base);
	buffer->commit_display = commit_display;

	dma_buf_put(dmabuf);
	return 0;
}

static int mua_get_meta_data(int fd, ulong arg)
{
	struct dma_buf *dmabuf;
	struct vframe_s *vfp;
	struct uvm_meta_data meta;

	dmabuf = dma_buf_get(fd);
	if (IS_ERR_OR_NULL(dmabuf)) {
		MUA_PRINTK(MUA_ERROR, "invalid dmabuf fd\n");
		return -EINVAL;
	}

	vfp = dmabuf_get_vframe(dmabuf);
	if (IS_ERR_OR_NULL(vfp)) {
		dmabuf_put_vframe(dmabuf);
		dma_buf_put(dmabuf);
		MUA_PRINTK(MUA_DBG, "vframe is null.\n");
		return -EINVAL;
	}
	/* check source type. */
	if (!vfp->meta_data_size ||
		vfp->meta_data_size > META_DATA_SIZE) {
		MUA_PRINTK(MUA_DBG, "meta data size: %d is invalid.\n",
			vfp->meta_data_size);
		dmabuf_put_vframe(dmabuf);
		dma_buf_put(dmabuf);
		return -EINVAL;
	}

	meta.fd = fd;
	meta.size = vfp->meta_data_size;
	if (!vfp->meta_data_buf) {
		MUA_PRINTK(MUA_ERROR, "vfp->meta_data_buf is NULL.\n");
		dmabuf_put_vframe(dmabuf);
		dma_buf_put(dmabuf);
		return -EINVAL;
	}
	memcpy(meta.data, vfp->meta_data_buf, vfp->meta_data_size);
	dmabuf_put_vframe(dmabuf);

	if (copy_to_user((void __user *)arg, &meta, sizeof(meta))) {
		MUA_PRINTK(MUA_ERROR, "meta data copy err.\n");
		dma_buf_put(dmabuf);
		return -EFAULT;
	}

	dma_buf_put(dmabuf);

	return 0;
}

static int mua_attach(int fd, int type, char *buf)
{
	struct uvm_hook_mod_info info;
	int ret = 0;
	struct dma_buf *dmabuf = NULL;

	memset(&info, 0, sizeof(struct uvm_hook_mod_info));
	info.type = PROCESS_INVALID;
	info.arg = NULL;
	info.acquire_fence = NULL;

	switch (type) {
	case PROCESS_NN:
		ret = attach_nn_hook_mod_info(fd, buf, &info);
		if (ret)
			return -EINVAL;
		break;
	case PROCESS_AIPQ:
		ret = attach_aipq_hook_mod_info(fd, buf, &info);
		if (ret)
			return -EINVAL;
		break;
	case PROCESS_DALTON:
		ret = attach_dalton_hook_mod_info(fd, buf, &info);
		if (ret)
			return -EINVAL;
		break;
	case PROCESS_AIFACE:
		ret = attach_aiface_hook_mod_info(fd, buf, &info);
		if (ret)
			return -EINVAL;
		break;
	case PROCESS_AICOLOR:
		ret = attach_aicolor_hook_mod_info(fd, buf, &info);
		if (ret)
			return -EINVAL;
		break;
	default:
		MUA_PRINTK(MUA_ERROR, "mod_type is not valid.\n");
	}
	if (ret) {
		MUA_PRINTK(MUA_ERROR, "get hook_mod_info failed\n");
		return -EINVAL;
	}

	dmabuf = dma_buf_get(fd);
	MUA_PRINTK(MUA_INFO, "core_attach: type:%d dmabuf:%p.\n",
		type, dmabuf);

	if (IS_ERR_OR_NULL(dmabuf)) {
		MUA_PRINTK(MUA_ERROR, "Invalid dmabuf %s %d\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (!dmabuf_is_uvm(dmabuf)) {
		dma_buf_put(dmabuf);
		MUA_PRINTK(MUA_ERROR, "dmabuf is not uvm. %s %d\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (info.type >= VF_SRC_DECODER && info.type < PROCESS_INVALID)
		ret = uvm_attach_hook_mod(dmabuf, &info);

	dma_buf_put(dmabuf);
	return ret;
}

static int mua_detach(int fd, int type)
{
	int ret = 0;
	int index = 0;
	struct dma_buf *dmabuf = NULL;

	dmabuf = dma_buf_get(fd);

	if (IS_ERR_OR_NULL(dmabuf)) {
		MUA_PRINTK(MUA_ERROR, "Invalid dmabuf %s %d\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (!dmabuf_is_uvm(dmabuf)) {
		dma_buf_put(dmabuf);
		MUA_PRINTK(MUA_ERROR, "dmabuf is not uvm. %s %d\n", __func__, __LINE__);
		return -EINVAL;
	}


	MUA_PRINTK(MUA_INFO, "[%s]: dmabuf %p.\n",  __func__, dmabuf);

	for (index = VF_SRC_DECODER; index < PROCESS_INVALID; index++) {
		if (type & BIT(index)) {
			index |= BIT(PROCESS_HWC);
			ret &= uvm_detach_hook_mod(dmabuf, index);
		}
	}

	dma_buf_put(dmabuf);

	return ret;
}

static long mua_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct mua_device *md;
	union uvm_ioctl_arg data;
	struct dma_buf *dmabuf = NULL;
	int pid;
	int ret = 0;
	int fd = 0;
	size_t usage = 0;
	int alloc_buf_size = 0;
	int videotype = 0;
	u64 timestamp = 0;

	md = file->private_data;

	if (_IOC_SIZE(cmd) > sizeof(data))
		return -EINVAL;

	if (copy_from_user(&data, (void __user *)arg, _IOC_SIZE(cmd)))
		return -EFAULT;

	switch (cmd) {
	case UVM_IOC_ATTATCH:
		MUA_PRINTK(MUA_INFO, "mua_ioctl_attach: shared_fd=%d, mode_type=%d\n",
			data.hook_data.shared_fd,
			data.hook_data.mode_type);
		ret = mua_attach(data.hook_data.shared_fd,
						data.hook_data.mode_type,
						data.hook_data.data_buf);
		if (ret < 0) {
			MUA_PRINTK(MUA_INFO, "mua_attach fail.\n");
			return -EINVAL;
		}
		if (copy_to_user((void __user *)arg, &data, _IOC_SIZE(cmd)))
			return -EFAULT;
		break;
	case UVM_IOC_DETATCH:
		MUA_PRINTK(MUA_INFO, "mua_ioctl_detach: shared_fd=%d, mode_type=%d\n",
			data.hook_data.shared_fd,
			data.hook_data.mode_type);
		ret = mua_detach(data.hook_data.shared_fd,
						data.hook_data.mode_type);
		if (ret < 0) {
			MUA_PRINTK(MUA_INFO, "mua_detach fail.\n");
			return -EINVAL;
		}
		break;
	case UVM_IOC_GET_INFO:
		fd = data.hook_data.shared_fd;
		if (!mua_is_valid_dmabuf(fd))
			return -EINVAL;
		dmabuf = dma_buf_get(fd);
		ret = meson_uvm_getinfo(dmabuf,
						data.hook_data.mode_type,
						data.hook_data.data_buf);
		dma_buf_put(dmabuf);
		if (ret < 0) {
			MUA_PRINTK(MUA_INFO, "meson_uvm_getinfo fail.\n");
			return -EINVAL;
		}
		if (copy_to_user((void __user *)arg, &data, _IOC_SIZE(cmd)))
			return -EFAULT;
		break;
	case UVM_IOC_SET_INFO:
		fd = data.hook_data.shared_fd;
		if (!mua_is_valid_dmabuf(fd))
			return -EINVAL;
		dmabuf = dma_buf_get(fd);
		ret = meson_uvm_setinfo(dmabuf,
						data.hook_data.mode_type,
						data.hook_data.data_buf);
		dma_buf_put(dmabuf);
		if (ret < 0) {
			MUA_PRINTK(MUA_INFO, "meson_uvm_setinfo fail.\n");
			return -EINVAL;
		}
		break;
	case UVM_IOC_ALLOC:
		MUA_PRINTK(MUA_INFO, "%s. original buf size:%d width:%d height:%d\n",
					__func__, data.alloc_data.size,
					data.alloc_data.width, data.alloc_data.height);

		MUA_PRINTK(MUA_INFO, "scaled_buf_size:%d align=%d flags=%d\n",
			   data.alloc_data.scaled_buf_size,
			   data.alloc_data.align,
			   data.alloc_data.flags);

		data.alloc_data.size = PAGE_ALIGN(data.alloc_data.size);
		data.alloc_data.scaled_buf_size =
				PAGE_ALIGN(data.alloc_data.scaled_buf_size);
		if (data.alloc_data.scalar > 1)
			alloc_buf_size = data.alloc_data.scaled_buf_size;
		else
			alloc_buf_size = data.alloc_data.size;
		MUA_PRINTK(MUA_INFO, "%s. buf_scalar=%d scaled_buf_size=%d\n",
					__func__, data.alloc_data.scalar,
					data.alloc_data.scaled_buf_size);

		dmabuf = uvm_alloc_dmabuf(alloc_buf_size,
					  data.alloc_data.align,
					  data.alloc_data.flags);
		if (IS_ERR(dmabuf))
			return -ENOMEM;

		ret = mua_handle_alloc(dmabuf, &data.alloc_data, alloc_buf_size);
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
		ret = mua_set_commit_display(fd, data.fd_data.data);

		if (ret < 0) {
			MUA_PRINTK(MUA_ERROR, "invalid dmabuf fd.\n");
			return -EINVAL;
		}
		break;
	case UVM_IOC_SET_USAGE:
		fd = data.usage_data.fd;
		if (!mua_is_valid_dmabuf(fd))
			return -EINVAL;
		dmabuf = dma_buf_get(fd);
		usage = data.usage_data.uvm_data_usage;
		ret = meson_uvm_set_usage(dmabuf, usage);
		dma_buf_put(dmabuf);
		if (ret < 0) {
			MUA_PRINTK(MUA_INFO, "meson_uvm_set_usage fail.\n");
			return -EINVAL;
		}
		break;
	case UVM_IOC_GET_USAGE:
		fd = data.usage_data.fd;
		if (!mua_is_valid_dmabuf(fd))
			return -EINVAL;
		dmabuf = dma_buf_get(fd);
		ret = meson_uvm_get_usage(dmabuf, &usage);
		dma_buf_put(dmabuf);
		if (ret < 0) {
			MUA_PRINTK(MUA_INFO, "meson_uvm_get_usage fail.\n");
			return -EINVAL;
		}
		data.usage_data.uvm_data_usage = usage;
		if (copy_to_user((void __user *)arg, &data, _IOC_SIZE(cmd)))
			return -EFAULT;
		break;
	case UVM_IOC_GET_METADATA:
		MUA_PRINTK(MUA_DBG, "%s LINE:%d.\n", __func__, __LINE__);
		ret = mua_get_meta_data(data.meta_data.fd, arg);
		if (ret < 0) {
			MUA_PRINTK(MUA_INFO, "get meta data fail.\n");
			return -EINVAL;
		}
		break;
	case UVM_IOC_GET_VIDEO_INFO:
		fd = data.fd_info.fd;
		if (!mua_is_valid_dmabuf(fd))
			return -EINVAL;

		ret = get_uvm_video_info(fd, &videotype, &timestamp);
		if (ret < 0) {
			MUA_PRINTK(MUA_INFO, "get video %d info fail.\n", fd);
			return ret;
		} else {
			MUA_PRINTK(MUA_INFO, "get video %d info type:%x, timestamp:%lld.\n",
					fd, videotype, timestamp);
		}

		data.fd_info.type = videotype;
		data.fd_info.timestamp = timestamp;

		if (copy_to_user((void __user *)arg, &data, _IOC_SIZE(cmd)))
			return -EFAULT;
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

	MUA_PRINTK(MUA_INFO, "%s: %d\n", __func__, __LINE__);
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

//MODULE_DESCRIPTION("AMLOGIC uvm dmabuf allocator device driver");
//MODULE_LICENSE("GPL");
