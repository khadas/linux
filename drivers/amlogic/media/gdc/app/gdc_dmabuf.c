// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/mm_types.h>
#include <linux/mutex.h>
#include <linux/dma-buf.h>
#include <linux/scatterlist.h>
#include <linux/pagemap.h>
#include <linux/dma-mapping.h>
#include <api/gdc_api.h>
#include <linux/dma-contiguous.h>

#include "system_log.h"
#include "gdc_dmabuf.h"
#include "gdc_config.h"

static void clear_dma_buffer(struct aml_dma_buffer *buffer, int index);

static void *aml_mm_vmap(phys_addr_t phys, unsigned long size)
{
	u32 offset, npages;
	struct page **pages = NULL;
	pgprot_t pgprot = PAGE_KERNEL;
	void *vaddr;
	int i;

	offset = offset_in_page(phys);
	npages = DIV_ROUND_UP(size + offset, PAGE_SIZE);

	pages = vmalloc(sizeof(struct page *) * npages);
	if (!pages)
		return NULL;
	for (i = 0; i < npages; i++) {
		pages[i] = phys_to_page(phys);
		phys += PAGE_SIZE;
	}
	/* pgprot = pgprot_writecombine(PAGE_KERNEL); */

	vaddr = vmap(pages, npages, VM_MAP, pgprot);
	if (!vaddr) {
		pr_err("vmaped fail, size: %d\n",
		       npages << PAGE_SHIFT);
		vfree(pages);
		return NULL;
	}
	vfree(pages);
	gdc_log(LOG_DEBUG, "[HIGH-MEM-MAP] pa(%lx) to va(%p), size: %d\n",
		(unsigned long)phys, vaddr, npages << PAGE_SHIFT);
	return vaddr;
}

static void *aml_map_phyaddr_to_virt(dma_addr_t phys, unsigned long size)
{
	void *vaddr = NULL;

	if (!PageHighMem(phys_to_page(phys)))
		return phys_to_virt(phys);
	vaddr = aml_mm_vmap(phys, size);
	return vaddr;
}

/* refer to is_vmalloc_addr() and is_vmalloc_or_module_addr() */
#ifdef CONFIG_MMU
int _is_vmalloc_or_module_addr(const void *x)
{
	/*
	 * ARM, x86-64 and sparc64 put modules in a special place,
	 * and fall back on vmalloc() if that fails. Others
	 * just put it in the vmalloc space.
	 */
	unsigned long addr = (unsigned long)x;

#if defined(CONFIG_MODULES) && defined(MODULES_VADDR)
	if (addr >= MODULES_VADDR && addr < MODULES_END)
		return 1;
#endif

	return addr >= VMALLOC_START && addr < VMALLOC_END;
}
#else
static inline int _is_vmalloc_or_module_addr(const void *x)
{
	return 0;
}
#endif

/* dma free*/
static void aml_dma_put(void *buf_priv)
{
	struct aml_dma_buf *buf = buf_priv;
	struct page *cma_pages = NULL;
	void *vaddr = (void *)(PAGE_MASK & (ulong)buf->vaddr);

	if (!atomic_dec_and_test(&buf->refcount)) {
		gdc_log(LOG_DEBUG, "%s, refcont=%d\n",
			__func__, atomic_read(&buf->refcount));
		return;
	}

	if (buf->sgt_base) {
		sg_free_table(buf->sgt_base);
		kfree(buf->sgt_base);
	}

	if (gdc_smmu_enable) {
		dma_free_attrs(buf->dev, buf->size, buf->vaddr, buf->dma_addr,
			       buf->attrs);
	} else {
		cma_pages = phys_to_page(buf->dma_addr);
		if (_is_vmalloc_or_module_addr(vaddr))
			vunmap(vaddr);
		if (!dma_release_from_contiguous(buf->dev, cma_pages,
						 buf->size >> PAGE_SHIFT)) {
			pr_err("failed to release cma buffer\n");
		}
	}

	buf->vaddr = NULL;
	buf->sgt_base = NULL;
	if (buf->index < AML_MAX_DMABUF && buf->priv)
		clear_dma_buffer((struct aml_dma_buffer *)buf->priv,
				 buf->index);
	put_device(buf->dev);

	gdc_log(LOG_DEBUG, "gdc free:aml_dma_buf=0x%p,buf->index=%d\n",
		buf, buf->index);
	kfree(buf);
}

static void *aml_dma_alloc(struct device *dev, unsigned long attrs,
			   unsigned long size, enum dma_data_direction dma_dir,
			   gfp_t gfp_flags)
{
	struct aml_dma_buf *buf;
	struct page *cma_pages = NULL;
	dma_addr_t paddr = 0;

	if (WARN_ON(!dev))
		return (void *)(-EINVAL);

	buf = kzalloc(sizeof(*buf), GFP_KERNEL);
	if (!buf)
		return NULL;

	if (attrs)
		buf->attrs = attrs;

	if (gdc_smmu_enable) {
		buf->vaddr = dma_alloc_attrs(dev, size, &buf->dma_addr,
						gfp_flags, buf->attrs);
		if (!buf->vaddr) {
			pr_err("failed to dma_alloc_coherent\n");
			kfree(buf);
			return NULL;
		}

	} else {
		cma_pages = dma_alloc_from_contiguous(dev,
						      size >> PAGE_SHIFT, 0, 0);
		if (cma_pages) {
			paddr = page_to_phys(cma_pages);
		} else {
			kfree(buf);
			pr_err("failed to alloc cma pages.\n");
			return NULL;
		}
		buf->vaddr = aml_map_phyaddr_to_virt(paddr, size);
		buf->dma_addr = paddr;
	}

	buf->dev = get_device(dev);
	buf->size = size;
	buf->dma_dir = dma_dir;
	atomic_inc(&buf->refcount);
	gdc_log(LOG_DEBUG, "aml_dma_buf=0x%p, refcont=%d\n",
		buf, atomic_read(&buf->refcount));

	return buf;
}

static int aml_dma_mmap(void *buf_priv, struct vm_area_struct *vma)
{
	struct aml_dma_buf *buf = buf_priv;
	unsigned long pfn = 0;
	unsigned long vsize;
	int ret = -1;

	if (gdc_smmu_enable) {
		if (!buf) {
			pr_err("No buffer to map\n");
			return -EINVAL;
		}

		/*
		 * dma_mmap_* uses vm_pgoff as in-buffer offset, but we want to
		 * map whole buffer
		 */
		vma->vm_pgoff = 0;

		ret = dma_mmap_attrs(buf->dev, vma, buf->vaddr,
			buf->dma_addr, buf->size, buf->attrs);

		if (ret) {
			pr_err("Remapping memory failed, error: %d\n", ret);
			return ret;
		}
	} else {
		if (!buf || !vma) {
			pr_err("No memory to map\n");
			return -EINVAL;
		}

		vsize = vma->vm_end - vma->vm_start;

		pfn = buf->dma_addr >> PAGE_SHIFT;
		ret = remap_pfn_range(vma, vma->vm_start, pfn,
				      vsize, vma->vm_page_prot);
		if (ret) {
			pr_err("Remapping memory, error: %d\n", ret);
			return ret;
		}
		vma->vm_flags |= VM_DONTEXPAND;
	}

	gdc_log(LOG_DEBUG, "mapped dma addr 0x%08lx at 0x%08lx, size %d\n",
		(unsigned long)buf->dma_addr, vma->vm_start,
		buf->size);
	return 0;
}

/*********************************************/
/*         DMABUF ops for exporters          */
/*********************************************/
struct aml_attachment {
	struct sg_table sgt;
	enum dma_data_direction dma_dir;
};

static int aml_dmabuf_ops_attach(struct dma_buf *dbuf,
				 struct dma_buf_attachment *dbuf_attach)
{
	struct aml_attachment *attach;
	struct aml_dma_buf *buf = dbuf->priv;
	struct scatterlist *rd, *wr;
	struct sg_table *sgt;
	dma_addr_t dev_addr = buf->dma_addr;
	unsigned int i;
	int ret;

	attach = kzalloc(sizeof(*attach), GFP_KERNEL);
	if (!attach)
		return -ENOMEM;

	sgt = &attach->sgt;

	ret = sg_alloc_table(sgt, buf->sgt_base->orig_nents, GFP_KERNEL);
	if (ret) {
		kfree(attach);
		return -ENOMEM;
	}

	rd = buf->sgt_base->sgl;
	wr = sgt->sgl;
	for (i = 0; i < sgt->orig_nents; i++) {
		sg_dma_address(wr) = dev_addr;
		sg_set_page(wr, sg_page(rd), rd->length, rd->offset);
		dev_addr += rd->length;
		rd = sg_next(rd);
		wr = sg_next(wr);
	}

	attach->dma_dir = DMA_NONE;
	dbuf_attach->priv = attach;

	return 0;
}

static void aml_dmabuf_ops_detach(struct dma_buf *dbuf,
				  struct dma_buf_attachment *db_attach)
{
	struct aml_attachment *attach = db_attach->priv;
	struct sg_table *sgt;

	if (!attach)
		return;

	sgt = &attach->sgt;

	if (!gdc_smmu_enable) {
		/* release the scatterlist cache */
		if (attach->dma_dir != DMA_NONE)
			dma_unmap_sg(db_attach->dev, sgt->sgl, sgt->orig_nents,
				     attach->dma_dir);
	}
	sg_free_table(sgt);
	kfree(attach);
	db_attach->priv = NULL;
}

static struct sg_table *aml_dmabuf_ops_map(struct dma_buf_attachment *db_attach,
					   enum dma_data_direction dma_dir)
{
	struct aml_attachment *attach = db_attach->priv;
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

	if (!gdc_smmu_enable) {
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
			return (void *)(-EIO);
		}
	}
	attach->dma_dir = dma_dir;

	mutex_unlock(lock);
	return sgt;
}

static void aml_dmabuf_ops_unmap(struct dma_buf_attachment *db_attach,
				 struct sg_table *sgt,
				 enum dma_data_direction dma_dir)
{
	/* nothing to be done here */
}

static void aml_dmabuf_ops_release(struct dma_buf *dbuf)
{
	/* drop reference obtained in vb2_dc_get_dmabuf */
	aml_dma_put(dbuf->priv);
}

static void *aml_dmabuf_ops_kmap(struct dma_buf *dbuf, unsigned long pgnum)
{
	struct aml_dma_buf *buf = dbuf->priv;

	return buf->vaddr ? buf->vaddr + pgnum * PAGE_SIZE : NULL;
}

static void *aml_dmabuf_ops_vmap(struct dma_buf *dbuf)
{
	struct aml_dma_buf *buf = dbuf->priv;

	return buf->vaddr;
}

static int aml_dmabuf_ops_mmap(struct dma_buf *dbuf,
			       struct vm_area_struct *vma)
{
	return aml_dma_mmap(dbuf->priv, vma);
}

static struct dma_buf_ops gdc_dmabuf_ops = {
	.attach = aml_dmabuf_ops_attach,
	.detach = aml_dmabuf_ops_detach,
	.map_dma_buf = aml_dmabuf_ops_map,
	.unmap_dma_buf = aml_dmabuf_ops_unmap,
	.map = aml_dmabuf_ops_kmap,
	.vmap = aml_dmabuf_ops_vmap,
	.mmap = aml_dmabuf_ops_mmap,
	.release = aml_dmabuf_ops_release,
};

static struct sg_table *gdc_dma_get_base_sgt(struct aml_dma_buf *buf)
{
	int ret;
	struct sg_table *sgt;

	sgt = kmalloc(sizeof(*sgt), GFP_KERNEL);
	if (!sgt)
		return NULL;

	ret = dma_get_sgtable_attrs(buf->dev, sgt, buf->vaddr, buf->dma_addr,
		buf->size, buf->attrs);
	if (ret < 0) {
		pr_err("failed to get scatterlist from DMA API\n");
		kfree(sgt);
		return NULL;
	}

	return sgt;
}

static struct dma_buf *get_dmabuf(void *buf_priv, unsigned long flags)
{
	struct aml_dma_buf *buf = buf_priv;
	struct dma_buf *dbuf;
	DEFINE_DMA_BUF_EXPORT_INFO(exp_info);

	if (!buf->sgt_base)
		buf->sgt_base = gdc_dma_get_base_sgt(buf);
	if (WARN_ON(!buf->sgt_base))
		return NULL;

	exp_info.ops = &gdc_dmabuf_ops;
	exp_info.size = buf->size;
	exp_info.flags = flags;
	exp_info.priv = buf;

	if (WARN_ON(!buf->vaddr))
		return NULL;

	dbuf = dma_buf_export(&exp_info);
	if (IS_ERR(dbuf))
		return NULL;

	/* dmabuf keeps reference to vb2 buffer */
	atomic_inc(&buf->refcount);
	gdc_log(LOG_DEBUG, "%s, refcount=%d\n",
		__func__, atomic_read(&buf->refcount));

	return dbuf;
}

/* ge2d dma-buf api.h*/
static int find_empty_dma_buffer(struct aml_dma_buffer *buffer)
{
	int i;
	int found = 0;

	for (i = 0; i < AML_MAX_DMABUF; i++) {
		if (buffer->gd_buffer[i].alloc) {
			continue;
		} else {
			gdc_log(LOG_DEBUG, "%s i=%d\n", __func__, i);
			found = 1;
			break;
		}
	}
	if (found)
		return i;
	else
		return -1;
}

static void clear_dma_buffer(struct aml_dma_buffer *buffer, int index)
{
	mutex_lock(&buffer->lock);
	buffer->gd_buffer[index].mem_priv = NULL;
	buffer->gd_buffer[index].index = 0;
	buffer->gd_buffer[index].alloc = 0;
	mutex_unlock(&buffer->lock);
}

void *gdc_dma_buffer_create(void)
{
	int i;
	struct aml_dma_buffer *buffer;

	buffer = kzalloc(sizeof(*buffer), GFP_KERNEL);
	if (!buffer)
		return NULL;

	mutex_init(&buffer->lock);
	for (i = 0; i < AML_MAX_DMABUF; i++) {
		buffer->gd_buffer[i].mem_priv = NULL;
		buffer->gd_buffer[i].index = 0;
		buffer->gd_buffer[i].alloc = 0;
	}
	return buffer;
}

void gdc_dma_buffer_destroy(struct aml_dma_buffer *buffer)
{
	kfree(buffer);
}

int gdc_dma_buffer_alloc(struct aml_dma_buffer *buffer,
			 struct device *dev,
			 struct gdc_dmabuf_req_s *gdc_req_buf)
{
	void *buf;
	struct aml_dma_buf *dma_buf;
	unsigned int size;
	int index;

	if (WARN_ON(!dev))
		return (-EINVAL);
	if (!gdc_req_buf)
		return (-EINVAL);
	if (!buffer)
		return (-EINVAL);

	size = PAGE_ALIGN(gdc_req_buf->len);
	if (size == 0)
		return (-EINVAL);
	buf = aml_dma_alloc(dev, 0, size, gdc_req_buf->dma_dir,
			    GFP_HIGHUSER | __GFP_ZERO);
	if (!buf)
		return (-ENOMEM);
	mutex_lock(&buffer->lock);
	index = find_empty_dma_buffer(buffer);
	if (index < 0 || index >= AML_MAX_DMABUF) {
		pr_err("no empty buffer found\n");
		mutex_unlock(&buffer->lock);
		aml_dma_put(buf);
		return (-ENOMEM);
	}
	((struct aml_dma_buf *)buf)->priv = buffer;
	((struct aml_dma_buf *)buf)->index = index;
	buffer->gd_buffer[index].mem_priv = buf;
	buffer->gd_buffer[index].index = index;
	buffer->gd_buffer[index].alloc = 1;
	mutex_unlock(&buffer->lock);
	gdc_req_buf->index = index;
	dma_buf = (struct aml_dma_buf *)buf;
	if (dma_buf->dma_dir == DMA_FROM_DEVICE)
		dma_sync_single_for_cpu(dma_buf->dev,
					dma_buf->dma_addr,
					dma_buf->size,
					DMA_FROM_DEVICE);

	return 0;
}

int gdc_dma_buffer_free(struct aml_dma_buffer *buffer, int index)
{
	struct aml_dma_buf *buf;

	if (!buffer)
		return (-EINVAL);
	if (index < 0 || index >= AML_MAX_DMABUF)
		return (-EINVAL);

	buf = buffer->gd_buffer[index].mem_priv;
	if (!buf) {
		pr_err("aml_dma_buf is null\n");
		return (-EINVAL);
	}
	aml_dma_put(buf);
	return 0;
}

int gdc_dma_buffer_export(struct aml_dma_buffer *buffer,
			  struct gdc_dmabuf_exp_s *gdc_exp_buf)
{
	struct aml_dma_buf *buf;
	struct dma_buf *dbuf;
	int ret, index;
	unsigned int flags;

	if (!gdc_exp_buf)
		return (-EINVAL);
	if (!buffer)
		return (-EINVAL);

	index = gdc_exp_buf->index;
	if (index < 0 || index >= AML_MAX_DMABUF)
		return (-EINVAL);

	flags = gdc_exp_buf->flags;
	buf = buffer->gd_buffer[index].mem_priv;
	if (!buf) {
		pr_err("aml_dma_buf is null\n");
		return (-EINVAL);
	}
	dbuf = get_dmabuf(buf, flags & O_ACCMODE);
	if (IS_ERR_OR_NULL(dbuf)) {
		pr_err("failed to export buffer %d\n", index);
		return -EINVAL;
	}
	ret = dma_buf_fd(dbuf, flags & ~O_ACCMODE);
	if (ret < 0) {
		pr_err("buffer %d, failed to export (%d)\n",
		       index, ret);
		dma_buf_put(dbuf);
		return ret;
	}
	gdc_log(LOG_DEBUG, "buffer %d,exported as %d descriptor\n",
		index, ret);
	buffer->gd_buffer[index].fd = ret;
	buffer->gd_buffer[index].dbuf = dbuf;
	gdc_exp_buf->fd = ret;
	return 0;
}

int gdc_dma_buffer_map(struct aml_dma_cfg *cfg)
{
	long ret = -1;
	int fd = -1;
	struct dma_buf *dbuf = NULL;
	struct dma_buf_attachment *d_att = NULL;
	struct sg_table *sg = NULL;
	struct device *dev = NULL;
	enum dma_data_direction dir;

	if (!cfg || cfg->fd < 0 || !cfg->dev) {
		pr_err("%s: error input param\n", __func__);
		return -EINVAL;
	}
	fd = cfg->fd;
	dev = cfg->dev;
	dir = cfg->dir;
	dbuf = dma_buf_get(fd);
	if (IS_ERR(dbuf)) {
		pr_err("failed to get dma buffer");
		return -EINVAL;
	}
	d_att = dma_buf_attach(dbuf, dev);
	if (IS_ERR(d_att)) {
		pr_err("failed to set dma attach");
		goto attach_err;
	}

	sg = dma_buf_map_attachment(d_att, dir);
	if (IS_ERR(sg)) {
		pr_err("failed to get dma sg");
		goto map_attach_err;
	}

	ret = dma_buf_begin_cpu_access(dbuf, dir);
	if (ret != 0) {
		pr_err("failed to access dma buff");
		goto access_err;
	}

	cfg->dbuf = dbuf;
	cfg->attach = d_att;
	cfg->sg = sg;
	gdc_log(LOG_DEBUG, "%s, dbuf=0x%p\n", __func__, dbuf);

	return ret;

access_err:
	dma_buf_unmap_attachment(d_att, sg, dir);

map_attach_err:
	dma_buf_detach(dbuf, d_att);

attach_err:
	dma_buf_put(dbuf);
	return ret;
}

static int gdc_dma_buffer_get_phys_internal(struct aml_dma_buffer *buffer,
					    struct aml_dma_cfg *cfg,
					    unsigned long *addr)
{
	int i = 0, ret = -1;
	struct aml_dma_buf *dma_buf;
	struct dma_buf *dbuf = NULL;

	for (i = 0; i < AML_MAX_DMABUF; i++) {
		if (buffer->gd_buffer[i].alloc) {
			dbuf = dma_buf_get(cfg->fd);
			if (IS_ERR(dbuf)) {
				pr_err("%s: failed to get dma buffer,fd=%d, dbuf=%p\n",
				       __func__, cfg->fd, dbuf);
				return -EINVAL;
			}
			dma_buf_put(dbuf);
			if (dbuf == buffer->gd_buffer[i].dbuf) {
				cfg->dbuf = dbuf;
				dma_buf = buffer->gd_buffer[i].mem_priv;
				*addr = dma_buf->dma_addr;
				cfg->vaddr = dma_buf->vaddr;
				ret = 0;
				break;
			}
		}
	}
	return ret;
}

void gdc_recycle_linear_config(struct aml_dma_cfg *cfg)
{
	dma_unmap_single(cfg->dev, cfg->linear_config.dma_addr,
			 cfg->linear_config.buf_size, DMA_FROM_DEVICE);
	kfree(cfg->linear_config.buf_vaddr);
}

int gdc_dma_buffer_get_phys(struct aml_dma_buffer *buffer,
			    struct aml_dma_cfg *cfg, unsigned long *addr)
{
	struct sg_table *sg_table;
	int ret = -1, buf_size = 0, rc = 0;
	void *buf_vaddr;

	if (!cfg || cfg->fd < 0) {
		pr_err("%s: error input param\n", __func__);
		return -EINVAL;
	}

	ret = gdc_dma_buffer_get_phys_internal(buffer, cfg, addr);
	if (ret < 0) {
		gdc_log(LOG_DEBUG, "get_phys(fd=%d) is external.\n", cfg->fd);
		ret = gdc_dma_buffer_map(cfg);
		if (ret < 0) {
			pr_err("gdc_dma_buffer_map failed\n");
			return ret;
		}
		if (cfg->sg) {
			sg_table = cfg->sg;
			if (gdc_smmu_enable)
				*addr = sg_dma_address(sg_table->sgl);
			else
				*addr = sg_phys(sg_table->sgl);

			ret = 0;
		}
		/* config(firmware) buffer must be a physically continuous block memory
		 * for smmu case, copy it to a new linear block.
		 */
		if (gdc_smmu_enable && cfg->is_config_buf && cfg->sg) {
			sg_table = cfg->sg;
			buf_size = cfg->dbuf->size;
			buf_vaddr = kmalloc(buf_size, GFP_KERNEL);
			if (!buf_vaddr) {
				gdc_log(LOG_ERR, "%s %d, kmalloc err.\n", __func__, __LINE__);
				return -ENOMEM;
			}
			rc = sg_copy_to_buffer(sg_table->sgl,
					       sg_nents_for_len(sg_table->sgl,
								buf_size),
					       buf_vaddr, buf_size);
			if (!rc) {
				gdc_log(LOG_ERR, "sg_copy_to_buffer err.\n");
				gdc_dma_buffer_unmap(cfg);
				kfree(buf_vaddr);
				return -ENOMEM;
			}
			*addr = dma_map_single(cfg->dev, buf_vaddr, buf_size,
						DMA_TO_DEVICE);
			if (dma_mapping_error(cfg->dev, *addr)) {
				gdc_log(LOG_ERR, "dma_map_single err.\n");
				gdc_dma_buffer_unmap(cfg);
				kfree(buf_vaddr);
				return -ENOMEM;
			}

			cfg->linear_config.buf_vaddr = buf_vaddr;
			cfg->linear_config.buf_size = buf_size;
			cfg->linear_config.dma_addr = *addr;
		}
		gdc_dma_buffer_unmap(cfg);
	} else {
		/* config(firmware) buffer must be a physically continuous block memory
		 * for smmu case, copy it to a new linear block.
		 */
		if (gdc_smmu_enable && cfg->is_config_buf) {
			buf_size = cfg->dbuf->size;
			buf_vaddr = kmalloc(buf_size, GFP_KERNEL);
			if (!buf_vaddr) {
				gdc_log(LOG_ERR, "%s %d, kmalloc err.\n", __func__, __LINE__);
				return -ENOMEM;
			}

			memcpy(buf_vaddr, cfg->vaddr, buf_size);
			*addr = dma_map_single(cfg->dev, buf_vaddr, buf_size,
						DMA_TO_DEVICE);
			if (dma_mapping_error(cfg->dev, *addr)) {
				gdc_log(LOG_ERR, "dma_map_single err.\n");
				kfree(buf_vaddr);
				return -ENOMEM;
			}

			cfg->linear_config.buf_vaddr = buf_vaddr;
			cfg->linear_config.buf_size = buf_size;
			cfg->linear_config.dma_addr = *addr;
		}
	}
	return ret;
}

void gdc_dma_buffer_unmap(struct aml_dma_cfg *cfg)
{
	int fd = -1;
	struct dma_buf *dbuf = NULL;
	struct dma_buf_attachment *d_att = NULL;
	struct sg_table *sg = NULL;
	struct device *dev = NULL;
	enum dma_data_direction dir;

	if (!cfg || cfg->fd < 0 || !cfg->dev ||
	    !cfg->dbuf || !cfg->attach ||
	    !cfg->sg) {
		pr_err("%s: error input param\n", __func__);
		return;
	}

	fd = cfg->fd;
	dev = cfg->dev;
	dir = cfg->dir;
	dbuf = cfg->dbuf;
	d_att = cfg->attach;
	sg = cfg->sg;

	dma_buf_end_cpu_access(dbuf, dir);

	dma_buf_unmap_attachment(d_att, sg, dir);

	dma_buf_detach(dbuf, d_att);

	dma_buf_put(dbuf);
	gdc_log(LOG_DEBUG, "%s, dbuf=0x%p\n", __func__, dbuf);
}

void gdc_dma_buffer_dma_flush(struct device *dev, int fd)
{
	struct dma_buf *dmabuf;
	struct aml_dma_buf *buf;

	gdc_log(LOG_DEBUG, "%s fd=%d\n", __func__, fd);
	dmabuf = dma_buf_get(fd);
	if (IS_ERR(dmabuf)) {
		pr_err("dma_buf_get failed\n");
		return;
	}
	buf = dmabuf->priv;
	if (!buf) {
		pr_err("%s: error input param\n", __func__);
		return;
	}
	if (buf->size > 0 && buf->dev == dev)
		dma_sync_single_for_device(buf->dev, buf->dma_addr,
					   buf->size, DMA_TO_DEVICE);
	dma_buf_put(dmabuf);
}

void gdc_dma_buffer_cache_flush(struct device *dev, int fd)
{
	struct dma_buf *dmabuf;
	struct aml_dma_buf *buf;

	gdc_log(LOG_DEBUG, "%s fd=%d\n", __func__, fd);
	dmabuf = dma_buf_get(fd);
	if (IS_ERR(dmabuf)) {
		pr_err("dma_buf_get failed\n");
		return;
	}
	buf = dmabuf->priv;
	if (!buf) {
		pr_err("%s: error input param\n", __func__);
		return;
	}
	if (buf->size > 0 && buf->dev == dev)
		dma_sync_single_for_cpu(buf->dev, buf->dma_addr,
					buf->size, DMA_FROM_DEVICE);
	dma_buf_put(dmabuf);
}
