/*
 * drivers/amlogic/media/camera/common/cam_expbuf.c
 *
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
 */

#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/version.h>
#include <linux/dma-buf.h>
#include <linux/amlogic/media/v4l_util/videobuf-res.h>

#include "cam_expbuf.h"

struct vb_malloc_attachment {
	struct sg_table sgt;
	enum dma_data_direction dma_dir;
};

static int vb_dmabuf_ops_attach(struct dma_buf *dbuf,
				struct device *dev,
				struct dma_buf_attachment *dbuf_attach)
{
	struct vb_malloc_attachment *attach;
	struct videobuf_buffer *vb = dbuf->priv;
	int num_pages = PAGE_ALIGN(vb->bsize) / PAGE_SIZE;
	struct sg_table *sgt;
	struct scatterlist *sg;
	u32 phy_addr = 0;
	struct page *page = NULL;
	int ret;
	int i;

	attach = kzalloc(sizeof(*attach), GFP_KERNEL);
	if (!attach)
		return -ENOMEM;

	sgt = &attach->sgt;
	ret = sg_alloc_table(sgt, num_pages, GFP_KERNEL);
	if (ret) {
		kfree(attach);
		return ret;
	}

	phy_addr = (u32)videobuf_to_res(vb);

	page = phys_to_page(phy_addr);
	for_each_sg(sgt->sgl, sg, sgt->nents, i) {
		sg_set_page(sg, page, PAGE_SIZE, 0);
		page++;
	}

	attach->dma_dir = DMA_NONE;
	dbuf_attach->priv = (void *)attach;

	return 0;
}

static void vb_dmabuf_ops_detach(struct dma_buf *dbuf,
				 struct dma_buf_attachment *db_attach)
{
	struct vb_malloc_attachment *attach = db_attach->priv;
	struct sg_table *sgt;

	if (!attach)
		return;

	sgt = &attach->sgt;

	/* release the scatterlist cache */
	if (attach->dma_dir != DMA_NONE)
		dma_unmap_sg(db_attach->dev, sgt->sgl, sgt->orig_nents,
			     attach->dma_dir);
	sg_free_table(sgt);
	kfree(attach);
	db_attach->priv = NULL;
}

static struct sg_table *vb_dmabuf_ops_map(
		struct dma_buf_attachment *db_attach,
		enum dma_data_direction dma_dir)
{
	struct vb_malloc_attachment *attach = db_attach->priv;
	/* stealing dmabuf mutex to serialize map/unmap operations */
	struct mutex *lock = &db_attach->dmabuf->lock;
	struct sg_table *sgt;

	mutex_lock(lock);

	sgt = &attach->sgt;
	/* return previously mapped sg table */
	if (attach->dma_dir == dma_dir) {
		mutex_unlock(lock);
		return sgt;
	}

	/* release any previous cache */
	if (attach->dma_dir != DMA_NONE) {
		dma_unmap_sg(db_attach->dev, sgt->sgl, sgt->orig_nents,
			     attach->dma_dir);
		attach->dma_dir = DMA_NONE;
	}

	/* mapping to the client with new direction */
	sgt->nents = dma_map_sg(db_attach->dev, sgt->sgl, sgt->orig_nents,
				dma_dir);
	if (!sgt->nents) {
		pr_err("failed to map scatterlist\n");
		mutex_unlock(lock);
		return ERR_PTR(-EIO);
	}

	attach->dma_dir = dma_dir;

	mutex_unlock(lock);

	return sgt;
}

static void vb_dmabuf_ops_unmap(struct dma_buf_attachment *db_attach,
				struct sg_table *sgt,
				enum dma_data_direction dma_dir)
{
	/* nothing to be done here */
}

static void vb_dmabuf_ops_release(struct dma_buf *dbuf)
{
	/* nothing to be done here */
}

static void *vb_dmabuf_ops_kmap(struct dma_buf *dbuf, unsigned long pgnum)
{
	/* nothing to be done here */
	return NULL;
}

static void *vb_dmabuf_ops_vmap(struct dma_buf *dbuf)
{
	/* nothing to be done here */
	return (void *)0x0000ffff;
}

static int vb_dmabuf_ops_mmap(struct dma_buf *dbuf,
			      struct vm_area_struct *vma)
{
	pr_err("Failed to support mmap\n");

	return -1;
}

static struct dma_buf_ops vb_dmabuf_ops = {
	.attach = vb_dmabuf_ops_attach,
	.detach = vb_dmabuf_ops_detach,
	.map_dma_buf = vb_dmabuf_ops_map,
	.unmap_dma_buf = vb_dmabuf_ops_unmap,
	.kmap = vb_dmabuf_ops_kmap,
	.kmap_atomic = vb_dmabuf_ops_kmap,
	.vmap = vb_dmabuf_ops_vmap,
	.mmap = vb_dmabuf_ops_mmap,
	.release = vb_dmabuf_ops_release,
};

static struct dma_buf *vb_get_dmabuf(void *buf, u32 flags)
{
	struct videobuf_buffer *vb = buf;
	struct dma_buf *dbuf;
	DEFINE_DMA_BUF_EXPORT_INFO(exp_info);

	exp_info.ops = &vb_dmabuf_ops;
	exp_info.size = vb->bsize;
	exp_info.flags = flags;
	exp_info.priv = vb;

	dbuf = dma_buf_export(&exp_info);
	if (IS_ERR(dbuf))
		return NULL;

	return dbuf;
}

int vb_expbuf(void *buf, u32 flags)
{
	int ret;
	struct dma_buf *dbuf;

	dbuf = vb_get_dmabuf(buf, flags & O_ACCMODE);
	if (IS_ERR_OR_NULL(dbuf)) {
		pr_err("Failed to export buffer\n");
		return -EINVAL;
	}

	ret = dma_buf_fd(dbuf, flags & ~O_ACCMODE);
	if (ret < 0) {
		pr_err("Failed to get buff fd %d\n", ret);
		dma_buf_put(dbuf);
	}

	return ret;
}
