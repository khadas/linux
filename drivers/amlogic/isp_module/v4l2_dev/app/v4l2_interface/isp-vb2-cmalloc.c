/*
 * videobuf2-vmalloc.c - vmalloc memory allocator for videobuf2
 *
 * Copyright (C) 2010 Samsung Electronics
 *
 * Author: Pawel Osciak <pawel@osciak.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 */
#include <linux/version.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/dma-mapping.h>
#include <linux/dma-contiguous.h>

#include "isp-vb2-cmalloc.h"

static void *cma_alloc(struct device *dev, unsigned long size)
{
    struct page *cma_pages = NULL;
    dma_addr_t paddr = 0;
    void *vaddr = NULL;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 10, 0))
    cma_pages = dma_alloc_from_contiguous(dev,
			size >> PAGE_SHIFT, 0, false);
#else
    cma_pages = dma_alloc_from_contiguous(dev,
			size >> PAGE_SHIFT, 0);
#endif

    if (cma_pages) {
        paddr = page_to_phys(cma_pages);
    } else {
        pr_err("Failed to alloc cma pages.\n");
        return NULL;
    }

    vaddr = (void *)cma_pages;

    return vaddr;
}

static void cma_free(void *buf_priv)
{
    struct vb2_cmalloc_buf *buf = buf_priv;
    struct page *cma_pages = NULL;
    struct device *dev = NULL;
    bool rc = -1;

    dev = (void *)(buf->dbuf);

    cma_pages = buf->vaddr;

    rc = dma_release_from_contiguous(dev, cma_pages,
                buf->size >> PAGE_SHIFT);
    if (rc == false) {
        pr_err("Failed to release cma buffer\n");
        return;
    }

    buf->vaddr = NULL;
}


static void vb2_cmalloc_put(void *buf_priv)
{
    struct vb2_cmalloc_buf *buf = buf_priv;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 10, 0))
    if (refcount_dec_and_test(&buf->refcount)) {
#else
    if (atomic_dec_and_test(&buf->refcount)) {
#endif
        cma_free(buf_priv);
        kfree(buf);
    }
}

static void *vb2_cmalloc_alloc(struct device *dev, unsigned long attrs,
                unsigned long size, enum dma_data_direction dma_dir,
                gfp_t gfp_flags)
{
    struct vb2_cmalloc_buf *buf;

    buf = kzalloc(sizeof(*buf), GFP_KERNEL | gfp_flags);
    if (!buf)
        return ERR_PTR(-ENOMEM);

    buf->size = PAGE_ALIGN(size);
    buf->vaddr = cma_alloc(dev, buf->size);
    buf->dma_dir = dma_dir;
    buf->handler.refcount = &buf->refcount;
    buf->handler.put = vb2_cmalloc_put;
    buf->handler.arg = buf;
    buf->dbuf = (void *)dev;

    if (!buf->vaddr) {
        pr_err("cmalloc of size %ld failed\n", buf->size);
        kfree(buf);
        return ERR_PTR(-ENOMEM);
    }

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 10, 0))
    refcount_set(&buf->refcount, 1);
#else
    atomic_inc(&buf->refcount);
#endif

    return buf;
}

void *vb2_get_userptr(struct device *dev, unsigned long vaddr,
                unsigned long size,
                enum dma_data_direction dma_dir)
{
    struct vb2_cmalloc_buf *buf;

    buf = kzalloc(sizeof(*buf), GFP_KERNEL);
    if (buf == NULL) {
        pr_err("%s: Failed to alloc mem\n", __func__);
        return NULL;
    }

    buf->dbuf = (void *)dev;
    buf->vaddr = (void *)vaddr;
    buf->dma_dir = dma_dir;
    buf->size = size;

    return buf;
}

void vb2_put_userptr(void *buf_priv)
{
    if (buf_priv == NULL) {
        pr_err("%s: Error input param\n", __func__);
        return;
    }

    kfree(buf_priv);
}

static void *vb2_cmalloc_vaddr(void *buf_priv)
{
    struct vb2_cmalloc_buf *buf = buf_priv;

    if (!buf->vaddr) {
        pr_err("Address of an unallocated plane requested "
            "or cannot map user pointer\n");
        return NULL;
    }

    return buf->vaddr;
}

static unsigned int vb2_cmalloc_num_users(void *buf_priv)
{
    struct vb2_cmalloc_buf *buf = buf_priv;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 10, 0))
    return refcount_read(&buf->refcount);
#else
    return atomic_read(&buf->refcount);
#endif
}

static int vb2_cmalloc_mmap(void *buf_priv, struct vm_area_struct *vma)
{
    struct vb2_cmalloc_buf *buf = buf_priv;
    unsigned long pfn = 0;
    unsigned long vsize = vma->vm_end - vma->vm_start;
    int ret = -1;
    dma_addr_t paddr = 0;
    struct page *cma_pages = NULL;

    if (!buf || !vma) {
        pr_err("No memory to map\n");
        return -EINVAL;
    }

    cma_pages = buf->vaddr;

    paddr = page_to_phys(cma_pages);

    pfn = paddr >> PAGE_SHIFT;
    ret = remap_pfn_range(vma, vma->vm_start, pfn, vsize, vma->vm_page_prot);

    if (ret) {
        pr_err("Remapping vmalloc memory, error: %d\n", ret);
        return ret;
    }
    /*
    * Make sure that vm_areas for 2 buffers won't be merged together
    */
    vma->vm_flags |= VM_DONTEXPAND;

    /*
    * Use common vm_area operations to track buffer refcount.
    */
    vma->vm_private_data = &buf->handler;
    vma->vm_ops = &vb2_common_vm_ops;

    vma->vm_ops->open(vma);

    return 0;
}

struct vb2_cmalloc_attachment {
    struct sg_table sgt;
    enum dma_data_direction dma_dir;
};

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 10, 0))
static int vb2_cmalloc_dmabuf_ops_attach(struct dma_buf *dbuf,
        struct dma_buf_attachment *dbuf_attach)
#else
static int vb2_cmalloc_dmabuf_ops_attach(struct dma_buf *dbuf,
        struct device *dev, struct dma_buf_attachment *dbuf_attach)
#endif
{
    struct vb2_cmalloc_attachment *attach;
    struct vb2_cmalloc_buf *buf = dbuf->priv;
    int num_pages = PAGE_ALIGN(buf->size) / PAGE_SIZE;
    struct sg_table *sgt;
    struct scatterlist *sg;
    void *vaddr = buf->vaddr;
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

    struct page *page = vaddr;
    for_each_sg(sgt->sgl, sg, sgt->nents, i) {
        if (!page) {
            sg_free_table(sgt);
            kfree(attach);
            return -ENOMEM;
        }
        sg_set_page(sg, page, PAGE_SIZE, 0);
        page ++;
    }

    attach->dma_dir = DMA_NONE;
    dbuf_attach->priv = attach;
    return 0;
}

static void vb2_cmalloc_dmabuf_ops_detach(struct dma_buf *dbuf,
    struct dma_buf_attachment *db_attach)
{
    struct vb2_cmalloc_attachment *attach = db_attach->priv;
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

static struct sg_table *vb2_cmalloc_dmabuf_ops_map(
    struct dma_buf_attachment *db_attach, enum dma_data_direction dma_dir)
{
    struct vb2_cmalloc_attachment *attach = db_attach->priv;
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

static void vb2_cmalloc_dmabuf_ops_unmap(struct dma_buf_attachment *db_attach,
    struct sg_table *sgt, enum dma_data_direction dma_dir)
{
    /* nothing to be done here */
}


static void vb2_cmalloc_dmabuf_ops_release(struct dma_buf *dbuf)
{
    vb2_cmalloc_put(dbuf->priv);
}

static void *vb2_cmalloc_dmabuf_ops_kmap(struct dma_buf *dbuf, unsigned long pgnum)
{
    struct vb2_cmalloc_buf *buf = dbuf->priv;

    return buf->vaddr + pgnum * PAGE_SIZE;
}

static void *vb2_cmalloc_dmabuf_ops_vmap(struct dma_buf *dbuf)
{
    struct vb2_cmalloc_buf *buf = dbuf->priv;

#ifdef CONFIG_ANDROID_OS
    return buf->vaddr;
#else
    return page_to_virt(buf->vaddr);
#endif
}

static int vb2_cmalloc_dmabuf_ops_mmap(struct dma_buf *dbuf,
    struct vm_area_struct *vma)
{
    return vb2_cmalloc_mmap(dbuf->priv, vma);
}

static struct dma_buf_ops vb2_cmalloc_dmabuf_ops = {
    .attach = vb2_cmalloc_dmabuf_ops_attach,
    .detach = vb2_cmalloc_dmabuf_ops_detach,
    .map_dma_buf = vb2_cmalloc_dmabuf_ops_map,
    .unmap_dma_buf = vb2_cmalloc_dmabuf_ops_unmap,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 10, 0))
    .map = vb2_cmalloc_dmabuf_ops_kmap,
#else
    .kmap = vb2_cmalloc_dmabuf_ops_kmap,
    .kmap_atomic = vb2_cmalloc_dmabuf_ops_kmap,
#endif
    .vmap = vb2_cmalloc_dmabuf_ops_vmap,
    .mmap = vb2_cmalloc_dmabuf_ops_mmap,
    .release = vb2_cmalloc_dmabuf_ops_release,
};

static struct dma_buf *vb2_cmalloc_get_dmabuf(void *buf_priv, unsigned long flags)
{
    struct vb2_cmalloc_buf *buf = buf_priv;
    struct dma_buf *dbuf;
    DEFINE_DMA_BUF_EXPORT_INFO(exp_info);

    exp_info.ops = &vb2_cmalloc_dmabuf_ops;
    exp_info.size = buf->size;
    exp_info.flags = flags;
    exp_info.priv = buf;

    if (WARN_ON(!buf->vaddr))
        return NULL;

    dbuf = dma_buf_export(&exp_info);
    if (IS_ERR(dbuf))
        return NULL;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 10, 0))
    refcount_inc(&buf->refcount);
#else
    atomic_inc(&buf->refcount);
#endif

    return dbuf;
}

const struct vb2_mem_ops vb2_cmalloc_memops = {
    .alloc		= vb2_cmalloc_alloc,
    .put		= vb2_cmalloc_put,
    .get_userptr	= vb2_get_userptr,
    .put_userptr	= vb2_put_userptr,
#ifdef CONFIG_HAS_DMA
    .get_dmabuf	= vb2_cmalloc_get_dmabuf,
#endif
    .map_dmabuf	= NULL,
    .unmap_dmabuf	= NULL,
    .attach_dmabuf	= NULL,
    .detach_dmabuf	= NULL,
    .vaddr		= vb2_cmalloc_vaddr,
    .mmap		= vb2_cmalloc_mmap,
    .num_users	= vb2_cmalloc_num_users,
};
EXPORT_SYMBOL_GPL(vb2_cmalloc_memops);

MODULE_DESCRIPTION("cmalloc memory handling routines for videobuf2");
MODULE_AUTHOR("Keke Li<keke.li@amlogic.com>");
MODULE_LICENSE("GPL");
