// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/scatterlist.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/dma-buf.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/genalloc.h>
#include <linux/list.h>
#include <linux/mm.h>
#include <linux/compat.h>
#include <linux/amlogic/tee.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/amlogic/media/codec_mm/dmabuf_manage.h>
#include <linux/amlogic/media/codec_mm/configs.h>
#include <media/demux.h>
#include <media/dmxdev.h>

static int dmabuf_manage_debug;
module_param(dmabuf_manage_debug, int, 0644);

static int secure_vdec_def_size_bytes;
static u32 secure_block_align_2n;
static u32 secure_heap_version = SECURE_HEAP_SUPPORT_MULTI_POOL_VERSION;

#define  DEVICE_NAME "secmem"
#define  CLASS_NAME  "dmabuf_manage"

#define CONFIG_PATH "media.dmabuf_manage"
#define CONFIG_PREFIX "media"

#define dprintk(level, fmt, arg...)						\
	do {									\
		if (dmabuf_manage_debug >= (level))						\
			pr_info(CLASS_NAME ": %s: " fmt, __func__, ## arg);\
	} while (0)

#define pr_dbg(fmt, args ...)  dprintk(6, fmt, ## args)
#define pr_error(fmt, args ...) dprintk(1, fmt, ## args)
#define pr_inf(fmt, args ...) dprintk(8, fmt, ## args)
#define pr_enter() pr_inf("enter")

struct kdmabuf_attachment {
	struct sg_table sgt;
	enum dma_data_direction dma_dir;
};

#define MEM_SZ_1_5G							0x60000000
#define SECURE_MM_ALIGNED_2N				20
#define SECURE_MM_BLOCK_ALIGNED_2N			12
#define SECURE_MM_BLOCK_PADDING_SIZE		(256 * 1024)
#define SECURE_MM_MAX_VDEC_SIZE_BYTES		(80 * 1024 * 1024)
#define SECURE_MM_DEF_VDEC_SIZE_BYTES		(28 * 1024 * 1024)
#define SECURE_MM_MID_VDEC_SIZE_BYTES		(20 * 1024 * 1024)
#define SECURE_MM_MIN_VDEC_SIZE_BYTES		(8 * 1024 * 1024)
#define SECURE_ES_DEF_SIZE					(2 * 1024 * 1024)
#define SECURE_VDEC_DEF_SIZE				(4 * 1024 * 1024)
#define SECURE_VDEC_BLOCK_NUM_EVERY_POOL	8
#define SECURE_VDEC_DEF_4K_SIZE				(12 * 1024 * 1024)
#define SECURE_MM_MIN_CMA_SIZE_BYTES		(320 * 1024 * 1024)
#define SECURE_MAX_VDEC_POOL_NUM			4

#define SECURE_BLOCK_CHECKIN_ERROR			1

struct block_node {
	struct list_head list;
	u32 paddr;
	u32 size;
	u32 flag;
	u32 index;
};

struct secure_pool_info {
	struct gen_pool *pool;
	phys_addr_t paddr;
	u32 size;
	u32 instance;
	u32 bind;
	u32 block_num;
	u32 frame_size;
};

struct secure_vdec_info {
	u32 used;
	u32 vdec_handle;
	u32 vdec_paddr;
	u32 vdec_size;
	u32 vdec_align_paddr;
	u32 vdec_align_size;
	struct gen_pool *pool;
	struct secure_pool_info vdec_pool[SECURE_MAX_VDEC_POOL_NUM];
};

typedef int (*decode_info)(struct dmx_demux *demux, struct decoder_mem_info *info);

struct dmx_filter_info {
	struct list_head list;
	__u32 token;
	__u32 filter_fd;
	struct dmx_demux *demux;
	decode_info decode_info_func;
};

static struct list_head block_list;
static struct list_head dmx_filter_list;
static int dev_no;
static struct device *dmabuf_manage_dev;
static DEFINE_MUTEX(g_dmabuf_mutex);
static struct secure_vdec_info g_vdec_info;

static int dmabuf_manage_attach(struct dma_buf *dbuf, struct dma_buf_attachment *attachment)
{
	struct kdmabuf_attachment *attach;
	struct dmabuf_manage_block *block = dbuf->priv;
	struct sg_table *sgt;
	struct page *page;
	phys_addr_t phys = block->paddr;
	int ret;
	int sgnum = 1;
	struct dmabuf_dmx_sec_es_data *es = NULL;
	int len = 0;

	pr_enter();
	attach = (struct kdmabuf_attachment *)
		kzalloc(sizeof(*attach), GFP_KERNEL);
	if (!attach) {
		pr_error("kzalloc failed\n");
		goto error;
	}
	if (block->type == DMA_BUF_TYPE_DMXES) {
		es = (struct dmabuf_dmx_sec_es_data *)block->priv;
		if (es->data_end < es->data_start)
			sgnum = 2;
	}
	sgt = &attach->sgt;
	ret = sg_alloc_table(sgt, sgnum, GFP_KERNEL);
	if (ret) {
		pr_error("No memory for sgtable");
		goto error_alloc;
	}
	if (sgnum == 2) {
		len = PAGE_ALIGN(es->buf_end - es->data_start);
		if (block->paddr != es->data_start) {
			pr_error("Invalid buffer info %x %x",
				block->paddr, es->data_start);
			goto error_attach;
		}
		page = phys_to_page(phys);
		sg_set_page(sgt->sgl, page, len, 0);
		len = PAGE_ALIGN(es->data_end - es->buf_start);
		page = phys_to_page(es->buf_start);
		sg_set_page(sg_next(sgt->sgl), page, len, 0);
	} else {
		page = phys_to_page(phys);
		sg_set_page(sgt->sgl, page, PAGE_ALIGN(block->size), 0);
	}
	attach->dma_dir = DMA_NONE;
	attachment->priv = attach;
	return 0;
error_attach:
	sg_free_table(sgt);
error_alloc:
	kfree(attach);
error:
	return 0;
}

static void dmabuf_manage_detach(struct dma_buf *dbuf,
				struct dma_buf_attachment *attachment)
{
	struct kdmabuf_attachment *attach = attachment->priv;
	struct sg_table *sgt;

	pr_enter();
	if (!attach)
		return;
	sgt = &attach->sgt;
	sg_free_table(sgt);
	kfree(attach);
	attachment->priv = NULL;
}

static struct sg_table *dmabuf_manage_map_dma_buf(struct dma_buf_attachment *attachment,
		enum dma_data_direction dma_dir)
{
	struct kdmabuf_attachment *attach = attachment->priv;
	struct dmabuf_manage_block *block = attachment->dmabuf->priv;
	struct mutex *lock = &attachment->dmabuf->lock;
	struct sg_table *sgt;

	pr_enter();
	mutex_lock(lock);
	sgt = &attach->sgt;
	if (attach->dma_dir == dma_dir) {
		mutex_unlock(lock);
		return sgt;
	}
	sgt->sgl->dma_address = block->paddr;
#ifdef CONFIG_NEED_SG_DMA_LENGTH
	sgt->sgl->dma_length = PAGE_ALIGN(block->size);
#else
	sgt->sgl->length = PAGE_ALIGN(block->size);
#endif
	pr_dbg("nents %d, %x, %d, %d\n", sgt->nents, block->paddr,
			sg_dma_len(sgt->sgl), block->size);
	attach->dma_dir = dma_dir;
	mutex_unlock(lock);
	return sgt;
}

static void dmabuf_manage_unmap_dma_buf(struct dma_buf_attachment *attachment,
		struct sg_table *sgt,
		enum dma_data_direction dma_dir)
{
	pr_enter();
}

static void dmabuf_manage_buf_release(struct dma_buf *dbuf)
{
	struct dmabuf_manage_block *block;
	struct dmabuf_dmx_sec_es_data *es;
	struct dmx_filter_info *node = NULL;
	struct list_head *pos, *tmp;
	int found = 0;
	struct decoder_mem_info rp_info;

	pr_enter();
	block = (struct dmabuf_manage_block *)dbuf->priv;
	if (block && block->priv && block->type == DMA_BUF_TYPE_DMXES) {
		es = (struct dmabuf_dmx_sec_es_data *)block->priv;
		list_for_each_safe(pos, tmp, &dmx_filter_list) {
			node = list_entry(pos, struct dmx_filter_info, list);
			if (node && node->token == es->token) {
				found = 1;
				break;
			}
		}
		if (found) {
			if (es->buf_rp == 0)
				es->buf_rp = es->data_end;
			if (es->buf_rp >= es->buf_start && es->buf_rp <= es->buf_end &&
				node->demux && node->decode_info_func) {
				rp_info.rp_phy = es->buf_rp;
				node->decode_info_func(node->demux, &rp_info);
			}
		}
	}
	if (block) {
		pr_dbg("dma release handle:%x\n", block->handle);
		kfree(block->priv);
		kfree(block);
	}
}

static int dmabuf_manage_mmap(struct dma_buf *dbuf, struct vm_area_struct *vma)
{
	struct dmabuf_manage_block *block;
	struct dmabuf_dmx_sec_es_data *es;
	unsigned long addr = vma->vm_start;
	int len = 0;
	int ret = -EFAULT;

	pr_enter();
	block = (struct dmabuf_manage_block *)dbuf->priv;
	vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);
	if (block->type == DMA_BUF_TYPE_DMXES) {
		es = (struct dmabuf_dmx_sec_es_data *)block->priv;
		if (es->data_end >= es->data_start) {
			len = PAGE_ALIGN(es->data_end - es->data_start);
			if (block->paddr != es->data_start) {
				pr_error("Invalid buffer info %x %x",
					block->paddr, es->data_start);
				goto error;
			}
			ret = remap_pfn_range(vma, addr,
					page_to_pfn(phys_to_page(block->paddr)),
					len, vma->vm_page_prot);
		} else {
			len = PAGE_ALIGN(es->buf_end - es->data_start);
			if (block->paddr != es->data_start) {
				pr_error("Invalid buffer info %x %x",
					block->paddr, es->data_start);
				goto error;
			}
			ret = remap_pfn_range(vma, addr,
					page_to_pfn(phys_to_page(block->paddr)),
					len, vma->vm_page_prot);
			if (ret) {
				pr_error("remap failed %d", ret);
				goto error;
			}
			addr += len;
			if (addr >= vma->vm_end)
				return ret;
			len = PAGE_ALIGN(es->data_end - es->buf_start);
			ret = remap_pfn_range(vma, addr,
					page_to_pfn(phys_to_page(es->buf_start)),
					len, vma->vm_page_prot);
		}
	} else {
		ret = remap_pfn_range(vma, vma->vm_start, page_to_pfn(phys_to_page(block->paddr)),
			block->size, vma->vm_page_prot);
	}
error:
	return ret;
}

static struct dma_buf_ops dmabuf_manage_ops = {
	.attach = dmabuf_manage_attach,
	.detach = dmabuf_manage_detach,
	.map_dma_buf = dmabuf_manage_map_dma_buf,
	.unmap_dma_buf = dmabuf_manage_unmap_dma_buf,
	.release = dmabuf_manage_buf_release,
	.mmap = dmabuf_manage_mmap
};

static struct dma_buf *get_dmabuf(struct dmabuf_manage_block *block,
				  unsigned long flags)
{
	struct dma_buf *dbuf;
	DEFINE_DMA_BUF_EXPORT_INFO(exp_info);

	pr_dbg("export handle:%x, paddr:%x, size:%x\n",
		block->handle, block->paddr, block->size);
	exp_info.ops = &dmabuf_manage_ops;
	exp_info.size = block->size;
	exp_info.flags = flags;
	exp_info.priv = (void *)block;
	dbuf = dma_buf_export(&exp_info);
	if (IS_ERR(dbuf))
		return NULL;
	return dbuf;
}

static long dmabuf_manage_export(unsigned long args)
{
	int ret;
	struct dmabuf_manage_block *block;
	struct dma_buf *dbuf;
	int fd;

	pr_enter();
	block = (struct dmabuf_manage_block *)
		kzalloc(sizeof(*block), GFP_KERNEL);
	if (!block) {
		pr_error("kmalloc failed\n");
		goto error_alloc_object;
	}
	ret = copy_from_user((void *)block, (void __user *)args,
				sizeof(struct secmem_block));
	if (ret) {
		pr_error("copy_from_user failed\n");
		goto error_copy;
	}
	block->type = DMA_BUF_TYPE_SECMEM;
	dbuf = get_dmabuf(block, 0);
	if (!dbuf) {
		pr_error("get_dmabuf failed\n");
		goto error_export;
	}
	fd = dma_buf_fd(dbuf, O_CLOEXEC);
	if (fd < 0) {
		pr_error("dma_buf_fd failed\n");
		goto error_fd;
	}
	return fd;
error_fd:
	dma_buf_put(dbuf);
error_export:
error_copy:
	kfree(block);
error_alloc_object:
	return -EFAULT;
}

static long dmabuf_manage_get_handle(unsigned long args)
{
	int ret;
	long res = -EFAULT;
	int fd;
	struct dma_buf *dbuf;
	struct dmabuf_manage_block *block;

	pr_enter();
	ret = copy_from_user((void *)&fd, (void __user *)args, sizeof(fd));
	if (ret) {
		pr_error("copy_from_user failed\n");
		goto error_copy;
	}
	dbuf = dma_buf_get(fd);
	if (IS_ERR(dbuf)) {
		pr_error("dma_buf_get failed\n");
		goto error_get;
	}
	block = dbuf->priv;
	res = (long)(block->handle & (0xffffffff));
	dma_buf_put(dbuf);
error_get:
error_copy:
	return res;
}

static long dmabuf_manage_get_phyaddr(unsigned long args)
{
	int ret;
	long res = -EFAULT;
	int fd;
	struct dma_buf *dbuf;
	struct dmabuf_manage_block *block;

	pr_enter();
	ret = copy_from_user((void *)&fd, (void __user *)args, sizeof(fd));
	if (ret) {
		pr_error("copy_from_user failed\n");
		goto error_copy;
	}
	dbuf = dma_buf_get(fd);
	if (IS_ERR(dbuf)) {
		pr_error("dma_buf_get failed\n");
		goto error_get;
	}
	block = dbuf->priv;
	res = (long)(block->paddr & (0xffffffff));
	dma_buf_put(dbuf);
error_get:
error_copy:
	return res;
}

static long dmabuf_manage_import(unsigned long args)
{
	int ret;
	long res = -EFAULT;
	int fd;
	struct dma_buf *dbuf;
	struct dma_buf_attachment *attach;
	struct sg_table *sgt;
	dma_addr_t paddr;

	pr_enter();
	ret = copy_from_user((void *)&fd, (void __user *)args, sizeof(fd));
	if (ret) {
		pr_error("copy_from_user failed\n");
		goto error_copy;
	}
	dbuf = dma_buf_get(fd);
	if (IS_ERR(dbuf)) {
		pr_error("dma_buf_get failed\n");
		goto error_get;
	}
	attach = dma_buf_attach(dbuf, dmabuf_manage_dev);
	if (IS_ERR(attach)) {
		pr_error("dma_buf_attach failed\n");
		goto error_attach;
	}
	sgt = dma_buf_map_attachment(attach, DMA_FROM_DEVICE);
	if (IS_ERR(sgt)) {
		pr_error("dma_buf_map_attachment failed\n");
		goto error_map;
	}
	paddr = sg_dma_address(sgt->sgl);
	res = (long)(paddr & (0xffffffff));
	dma_buf_unmap_attachment(attach, sgt, DMA_FROM_DEVICE);
error_map:
	dma_buf_detach(dbuf, attach);
error_attach:
	dma_buf_put(dbuf);
error_get:
error_copy:
	return res;
}

static long dmabuf_manage_export_dmabuf(unsigned long args)
{
	long res = -EFAULT;
	struct dmabuf_manage_buffer info;
	struct dmabuf_manage_block *block;
	struct dmabuf_dmx_sec_es_data *dmxes;
	struct dma_buf *dbuf;
	int fd = -1;

	pr_enter();
	res = copy_from_user((void *)&info, (void __user *)args, sizeof(info));
	if (res) {
		pr_error("copy_from_user failed\n");
		goto error_copy;
	}
	if (info.type == DMA_BUF_TYPE_INVALID) {
		pr_error("unknown dma buf type\n");
		goto error_copy;
	}
	block = kzalloc(sizeof(*block), GFP_KERNEL);
	if (!block) {
		pr_error("kmalloc failed\n");
		goto error_copy;
	}
	switch (info.type) {
	case DMA_BUF_TYPE_DMXES:
		dmxes = kzalloc(sizeof(*dmxes), GFP_KERNEL);
		if (!dmxes) {
			pr_error("kmalloc failed\n");
			goto error_alloc_object;
		}
		memcpy(dmxes, &info.buffer.dmxes, sizeof(*dmxes));
		block->priv = dmxes;
		break;
	default:
		block->priv = NULL;
		break;
	}
	block->paddr = info.paddr;
	block->size = PAGE_ALIGN(info.size);
	block->handle = info.handle;
	block->type = info.type;
	dbuf = get_dmabuf(block, 0);
	if (!dbuf) {
		pr_error("get_dmabuf failed\n");
		goto error_alloc_object;
	}
	fd = dma_buf_fd(dbuf, O_CLOEXEC);
	if (fd < 0) {
		pr_error("dma_buf_fd failed\n");
		goto error_fd;
	}
	return fd;
error_fd:
	dma_buf_put(dbuf);
error_alloc_object:
	kfree(block->priv);
	kfree(block);
error_copy:
	return res;
}

static long dmabuf_manage_get_dmabufinfo(unsigned long args)
{
	struct dmabuf_manage_buffer info;
	struct dmabuf_manage_block *block;
	struct dma_buf *dbuf;

	pr_enter();
	if (copy_from_user((void *)&info, (void __user *)args, sizeof(info))) {
		pr_error("copy_from_user failed\n");
		goto error;
	}
	if (info.type == DMA_BUF_TYPE_INVALID)
		goto error;
	dbuf = dma_buf_get(info.fd);
	if (IS_ERR(dbuf))
		goto error;
	if (dbuf->priv && dbuf->ops == &dmabuf_manage_ops) {
		block = dbuf->priv;
		if (block->type != info.type)
			goto error;
		info.paddr = block->paddr;
		info.size = block->size;
		info.handle = block->handle;
		switch (info.type) {
		case DMA_BUF_TYPE_DMXES:
			memcpy(&info.buffer.dmxes, block->priv,
				sizeof(struct dmabuf_dmx_sec_es_data));
			break;
		default:
			break;
		}
		if (copy_to_user((void __user *)args, &info, sizeof(info))) {
			pr_error("error copy to use space");
			goto error_fd;
		}
	}
	dma_buf_put(dbuf);
	return 0;
error_fd:
	dma_buf_put(dbuf);
error:
	return -EFAULT;
}

static long dmabuf_manage_set_filterfd(unsigned long args)
{
	struct filter_info info;
	struct dmx_filter_info *node = NULL;
	struct list_head *pos, *tmp;
	int found = 0;
	struct dmx_demux *demux = NULL;
	decode_info decode_info_func = NULL;
	struct fd f;
	struct dmxdev_filter *dmxdevfilter = NULL;
	struct dmxdev *dmxdev = NULL;

	pr_enter();
	if (copy_from_user((void *)&info, (void __user *)args, sizeof(info))) {
		pr_error("copy_from_user failed\n");
		goto error;
	}
	if (list_empty(&dmx_filter_list)) {
		if (info.release) {
			pr_error("No filter info setting\n");
			goto error;
		} else {
			node = kzalloc(sizeof(*node), GFP_KERNEL);
			if (!node)
				goto error;
			list_add_tail(&node->list, &dmx_filter_list);
		}
	} else {
		list_for_each_safe(pos, tmp, &dmx_filter_list) {
			node = list_entry(pos, struct dmx_filter_info, list);
			if (node->token == info.token && node->filter_fd == info.filter_fd) {
				found = 1;
				break;
			}
		}
		if (info.release) {
			if (!found) {
				pr_error("No filter info setting\n");
				goto error;
			} else {
				list_del(&node->list);
				kfree(node);
				node = NULL;
			}
		} else {
			if (!found) {
				node = kzalloc(sizeof(*node), GFP_KERNEL);
				if (!node)
					goto error;
				list_add_tail(&node->list, &dmx_filter_list);
			}
		}
	}
	if (node) {
		node->token = info.token;
		node->filter_fd = info.filter_fd;
#ifdef CONFIG_AMLOGIC_DVB_COMPAT
		f = fdget(node->filter_fd);
		if (f.file && f.file->private_data) {
			dmxdevfilter = f.file->private_data;
			if (dmxdevfilter)
				dmxdev = dmxdevfilter->dev;
		}
		if (dmxdev && dmxdev->demux) {
			demux = dmxdev->demux;
			decode_info_func = dmxdev->demux->decode_info;
		} else {
			pr_error("Invalid filter fd\n");
		}
#endif
		node->demux = demux;
		node->decode_info_func = decode_info_func;
	}
	return 0;
error:
	return -EFAULT;
}

unsigned int dmabuf_manage_get_type(unsigned int fd)
{
	int ret = DMA_BUF_TYPE_INVALID;
	struct dmabuf_manage_block *block;
	struct dma_buf *dbuf;

	dbuf = dma_buf_get(fd);
	if (!dbuf) {
		pr_dbg("acquire dma_buf failed");
		goto error;
	}
	if (dbuf->priv && dbuf->ops == &dmabuf_manage_ops) {
		block = (struct dmabuf_manage_block *)dbuf->priv;
		if (block)
			ret = block->type;
	}
	dma_buf_put(dbuf);
error:
	return ret;
}
EXPORT_SYMBOL(dmabuf_manage_get_type);

void *dmabuf_manage_get_info(unsigned int fd, unsigned int type)
{
	void *buf = NULL;
	struct dmabuf_manage_block *block;
	struct dma_buf *dbuf;

	if (type == DMA_BUF_TYPE_INVALID)
		goto error;
	dbuf = dma_buf_get(fd);
	if (!dbuf) {
		pr_dbg("acquire dma_buf failed");
		goto error;
	}
	if (dbuf->priv && dbuf->ops == &dmabuf_manage_ops) {
		block = (struct dmabuf_manage_block *)dbuf->priv;
		switch (block->type) {
		case DMA_BUF_TYPE_SECMEM:
			buf = block;
			break;
		case DMA_BUF_TYPE_DMXES:
			buf = block->priv;
			break;
		default:
			break;
		}
	}
	dma_buf_put(dbuf);
error:
	return buf;
}
EXPORT_SYMBOL(dmabuf_manage_get_info);

static void *secure_pool_init(phys_addr_t addr, u32 size, u32 order)
{
	struct gen_pool *pool;

	pool = gen_pool_create(order, -1);
	if (!pool)
		return NULL;
	if (gen_pool_add(pool, addr, size, -1) != 0) {
		gen_pool_destroy(pool);
		return NULL;
	}
	return pool;
}

static void secure_pool_destroy(void *handle)
{
	if (handle)
		gen_pool_destroy(handle);
}

static phys_addr_t secure_pool_alloc(void *handle, u32 size)
{
	return gen_pool_alloc(handle, size);
}

static void secure_pool_free(void *handle, u32 addr, u32 size)
{
	return gen_pool_free(handle, addr, size);
}

static size_t secure_pool_free_size(void *handle)
{
	return gen_pool_avail(handle);
}

static inline u32 secure_align_down_2n(u32 size, u32 alg2n)
{
	return ((size) & (~((1 << (alg2n)) - 1)));
}

static inline u32 secure_align_up2n(u32 size, u32 alg2n)
{
	return ((size + (1 << alg2n) - 1) & (~((1 << alg2n) - 1)));
}

static inline u32 secure_addr_is_aligned(u32 addr, u32 alg2n)
{
	if ((addr & (~((1 << alg2n) - 1))) == addr)
		return 1;
	return 0;
}

static int secure_block_pool_init(void)
{
	int ret;
	u32 max_cma_size_bytes = codec_mm_get_total_size();
	u32 vdec_size = secure_vdec_def_size_bytes;
	u32 total_mem = totalram_pages() << PAGE_SHIFT;

	if (g_vdec_info.used)
		return 0;
	if (secure_heap_version >= SECURE_HEAP_SUPPORT_MULTI_POOL_VERSION &&
		vdec_size > SECURE_VDEC_DEF_SIZE) {
		pr_info("Use customer defined vdec size %x", vdec_size);
	} else {
		if (vdec_size < SECURE_MM_MIN_VDEC_SIZE_BYTES ||
			vdec_size > SECURE_MM_MAX_VDEC_SIZE_BYTES) {
			if (total_mem <= SZ_1G) {
				vdec_size = SECURE_MM_MIN_VDEC_SIZE_BYTES;
			} else if (total_mem > SZ_1G && total_mem < MEM_SZ_1_5G) {
				if (max_cma_size_bytes >= SECURE_MM_MIN_CMA_SIZE_BYTES)
					vdec_size = SECURE_MM_DEF_VDEC_SIZE_BYTES;
				else
					vdec_size = SECURE_MM_MID_VDEC_SIZE_BYTES;
			} else {
				vdec_size = SECURE_MM_DEF_VDEC_SIZE_BYTES;
			}
		}
		pr_info("Use default vdec size %x", vdec_size);
	}
	vdec_size += 1024 * 1024;
	g_vdec_info.vdec_size = secure_align_up2n(vdec_size, SECURE_MM_ALIGNED_2N);
	g_vdec_info.vdec_paddr = codec_mm_alloc_for_dma("dma-secure-buf",
		g_vdec_info.vdec_size / PAGE_SIZE, 0, CODEC_MM_FLAGS_DMA);
	if (!g_vdec_info.vdec_paddr) {
		pr_error("allocate cma size failed %x\n", g_vdec_info.vdec_size);
		goto error_init;
	}
	if (secure_addr_is_aligned(g_vdec_info.vdec_paddr, SECURE_MM_ALIGNED_2N)) {
		g_vdec_info.vdec_align_paddr = g_vdec_info.vdec_paddr;
		g_vdec_info.vdec_align_size = g_vdec_info.vdec_size;
	} else {
		g_vdec_info.vdec_align_paddr =
			secure_align_up2n(g_vdec_info.vdec_paddr, SECURE_MM_ALIGNED_2N);
		vdec_size = g_vdec_info.vdec_paddr +
			g_vdec_info.vdec_size - g_vdec_info.vdec_align_paddr;
		g_vdec_info.vdec_align_size = secure_align_down_2n(vdec_size,
			SECURE_MM_ALIGNED_2N);
	}
	ret = tee_protect_mem_by_type(TEE_MEM_TYPE_STREAM_INPUT,
				(u32)g_vdec_info.vdec_align_paddr,
				(u32)g_vdec_info.vdec_align_size,
				&g_vdec_info.vdec_handle);
	if (ret) {
		pr_error("protect vdec failed addr %x %x ret is %x\n",
			g_vdec_info.vdec_align_paddr, g_vdec_info.vdec_align_size, ret);
		goto error_init;
	}
	if (secure_block_align_2n == 0)
		secure_block_align_2n = SECURE_MM_BLOCK_ALIGNED_2N;
	g_vdec_info.pool = secure_pool_init(g_vdec_info.vdec_align_paddr,
			g_vdec_info.vdec_align_size, secure_block_align_2n);
	if (!g_vdec_info.pool) {
		pr_error("Init global pool failed\n");
		goto error_init;
	}
	INIT_LIST_HEAD(&block_list);
	g_vdec_info.used = 1;
	return 0;
error_init:
	if (g_vdec_info.vdec_handle)
		tee_unprotect_mem(g_vdec_info.vdec_handle);
	if (g_vdec_info.vdec_paddr) {
		if (codec_mm_free_for_dma("dma-secure-buf", g_vdec_info.vdec_paddr))
			pr_error("free dma buf failed in init");
	}
	memset(&g_vdec_info, 0, sizeof(struct secure_vdec_info));
	return -1;
}

static ssize_t dmabuf_manage_dump_vdec_info(int forceshow)
{
	char buf[1024] = { 0 };
	char *pbuf = buf;
	u32 i = 0;
	u32 size = 1024;
	u32 s = 0;
	u32 tsize = 0;
	u32 freesize = 0;
	u32 blocknum = 0;
	struct block_node *block = NULL;
	struct list_head *pos, *tmp;
	struct secure_pool_info *vdec_pool;

	if (forceshow == 0 && dmabuf_manage_debug < 8)
		return 0;
	s = snprintf(pbuf, size - tsize,
			"secure mem info:\nused %d\n", g_vdec_info.used);
	tsize += s;
	pbuf += s;
	s = snprintf(pbuf, size - tsize,
			"vdec size: %d\ndef vdec size: %d\n",
			g_vdec_info.vdec_size, secure_vdec_def_size_bytes);
	tsize += s;
	pbuf += s;
	if (g_vdec_info.used) {
		for (i = 0; i < SECURE_MAX_VDEC_POOL_NUM; i++) {
			vdec_pool = &g_vdec_info.vdec_pool[i];
			if (vdec_pool->pool)
				freesize = secure_pool_free_size(vdec_pool->pool);
			else
				freesize = 0;
			s = snprintf(pbuf, size - tsize,
			"Pool %d \t total:%d \t alloc:%d \t free:%d \t framesize:%d \t blocknum:%d\n",
			i, vdec_pool->size, vdec_pool->size - freesize, freesize,
			vdec_pool->frame_size, vdec_pool->block_num);
			tsize += s;
			pbuf += s;
		}
		list_for_each_safe(pos, tmp, &block_list) {
			block = list_entry(pos, struct block_node, list);
			s = snprintf(pbuf, size - tsize, "[%d]\t addr %x size %d flag %x %x\n",
				blocknum, block->paddr, block->size, block->flag, block->index);
			blocknum++;
			tsize += s;
			pbuf += s;
		}
	}
	s = snprintf(pbuf, size - tsize, "Total block num %d\n", blocknum);
	pr_error("%s", buf);
	return 0;
}

static u32 secure_get_subpoolsize_by_framesize(unsigned long framesize)
{
	return framesize >= SECURE_ES_DEF_SIZE ? SECURE_VDEC_DEF_4K_SIZE :
		SECURE_VDEC_DEF_SIZE;
}

static u32 secure_get_subpoolindex_by_framesize(u32 framesize, u32 ignorebind)
{
	u32 index = 0;

	for (index = 0; index < SECURE_MAX_VDEC_POOL_NUM; index++) {
		if (g_vdec_info.vdec_pool[index].pool &&
			g_vdec_info.vdec_pool[index].frame_size == framesize &&
			g_vdec_info.vdec_pool[index].block_num < SECURE_VDEC_BLOCK_NUM_EVERY_POOL) {
			if (!ignorebind && g_vdec_info.vdec_pool[index].bind)
				continue;
			break;
		}
	}
	return index;
}

static u32 secure_get_vdec_pool_by_instance(unsigned long id)
{
	u32 index = 0;

	for (index = 0; index < SECURE_MAX_VDEC_POOL_NUM; index++) {
		if (g_vdec_info.vdec_pool[index].instance == id)
			break;
	}
	return index;
}

static u32 secure_get_vdec_pool_by_paddr(phys_addr_t paddr)
{
	u32 index = 0;
	struct secure_pool_info *vdec_pool;

	for (index = 0; index < SECURE_MAX_VDEC_POOL_NUM; index++) {
		vdec_pool = &g_vdec_info.vdec_pool[index];
		if (paddr >= vdec_pool->paddr && paddr < vdec_pool->paddr + vdec_pool->size)
			break;
	}
	return index;
}

static struct secure_pool_info *secure_subpool_init(int poolsize, int *subindex, int framesize)
{
	struct secure_pool_info *vdec_pool = NULL;
	int index = 0;

	for (index = 0; index < SECURE_MAX_VDEC_POOL_NUM; index++) {
		if (!g_vdec_info.vdec_pool[index].pool)
			break;
	}
	if (index >= SECURE_MAX_VDEC_POOL_NUM) {
		pr_error("Invalid subpool %d\n", index);
		goto error_init_subpool;
	}
	vdec_pool = &g_vdec_info.vdec_pool[index];
	do {
		vdec_pool->paddr = secure_pool_alloc(g_vdec_info.pool, poolsize);
		if (vdec_pool->paddr > 0) {
			vdec_pool->size = poolsize;
			vdec_pool->pool = secure_pool_init(vdec_pool->paddr, vdec_pool->size,
				secure_block_align_2n);
		} else {
			poolsize -= 1024 * 1024;
		}
	} while (!vdec_pool->pool && poolsize > SECURE_VDEC_DEF_SIZE);
	if (!vdec_pool->pool) {
		pr_error("secure pool create failed\n");
		goto error_init_subpool1;
	}
	vdec_pool->instance = 0;
	vdec_pool->bind = 0;
	vdec_pool->block_num = 0;
	vdec_pool->frame_size = framesize;
	if (subindex)
		*subindex = index;
	return vdec_pool;
error_init_subpool1:
	memset(vdec_pool, 0, sizeof(*vdec_pool));
error_init_subpool:
	return NULL;
}

static void secure_subpool_destroy(struct secure_pool_info *vdec_pool)
{
	if (vdec_pool) {
		if (vdec_pool->pool) {
			secure_pool_destroy(vdec_pool->pool);
			secure_pool_free(g_vdec_info.pool, vdec_pool->paddr, vdec_pool->size);
		}
		memset(vdec_pool, 0, sizeof(*vdec_pool));
	}
}

phys_addr_t secure_block_alloc(unsigned long size, unsigned long maxsize, unsigned long id)
{
	phys_addr_t paddr = 0;
	int ret = 0;
	u32 alignsize = 0;
	struct block_node *block = NULL;
	struct secure_pool_info *vdec_pool;
	u32 index = 0;
	u32 freesize = 0;
	u32 subpool_size = 0;
	u32 forcedebug = 1;

	mutex_lock(&g_dmabuf_mutex);
	pr_enter();
	if (!g_vdec_info.used)
		goto error_alloc;
	alignsize = secure_align_up2n(size, SECURE_MM_BLOCK_ALIGNED_2N);
	if (secure_heap_version == SECURE_HEAP_DEFAULT_VERSION)
		alignsize += SECURE_MM_BLOCK_PADDING_SIZE;
	if (secure_heap_version <= SECURE_HEAP_SUPPORT_DELAY_ALLOC_VERSION) {
		if (!g_vdec_info.vdec_pool[index].pool) {
			subpool_size = secure_pool_free_size(g_vdec_info.pool);
			vdec_pool = secure_subpool_init(subpool_size, &index, 0);
			if (!vdec_pool) {
				pr_inf("init subpool failed\n");
				goto error_alloc;
			}
		}
		vdec_pool = &g_vdec_info.vdec_pool[index];
	} else {
		index = SECURE_MAX_VDEC_POOL_NUM;
		if (id > 0)
			index = secure_get_vdec_pool_by_instance(id);
		if (index >= SECURE_MAX_VDEC_POOL_NUM)
			index = secure_get_subpoolindex_by_framesize(maxsize, 1);
		if (index >= SECURE_MAX_VDEC_POOL_NUM) {
			freesize = secure_pool_free_size(g_vdec_info.pool);
			subpool_size = secure_get_subpoolsize_by_framesize(maxsize);
			if (subpool_size > freesize)
				subpool_size = freesize;
			vdec_pool = secure_subpool_init(subpool_size, &index, maxsize);
			if (!vdec_pool) {
				pr_inf("init subpool %d failed\n", index);
				goto error_alloc;
			}
		}
		vdec_pool = &g_vdec_info.vdec_pool[index];
	}
	paddr = secure_pool_alloc(vdec_pool->pool, alignsize);
	if (paddr > 0) {
		ret = tee_check_in_mem(paddr, alignsize);
		if (ret)
			pr_error("Secmem checkin err %x\n", ret);
		if (dmabuf_manage_debug >= 6) {
			block = kzalloc(sizeof(*block), GFP_KERNEL);
			if (!block)
				goto error_alloc;
			block->paddr = paddr;
			block->size = alignsize;
			block->index = index;
			list_add_tail(&block->list, &block_list);
			if (ret)
				block->flag = SECURE_BLOCK_CHECKIN_ERROR;
		}
	} else {
		pr_error("Alloc block %d %d failed\n", alignsize, index);
		goto error_alloc;
	}
	vdec_pool->block_num++;
	forcedebug = 0;
error_alloc:
	dmabuf_manage_dump_vdec_info(forcedebug);
	mutex_unlock(&g_dmabuf_mutex);
	return paddr;
}

unsigned long secure_block_free(phys_addr_t addr, unsigned long size)
{
	int ret = -1;
	u32 alignsize = 0;
	struct block_node *block = NULL;
	struct list_head *pos, *tmp;
	u32 index = 0;
	struct secure_pool_info *vdec_pool;
	u32 forcedebug = 1;

	mutex_lock(&g_dmabuf_mutex);
	pr_enter();
	if (!g_vdec_info.used)
		goto error_free;
	alignsize = secure_align_up2n(size, SECURE_MM_BLOCK_ALIGNED_2N);
	if (secure_heap_version == SECURE_HEAP_DEFAULT_VERSION)
		alignsize += SECURE_MM_BLOCK_PADDING_SIZE;
	index = secure_get_vdec_pool_by_paddr(addr);
	if (index >= SECURE_MAX_VDEC_POOL_NUM)
		goto error_free;
	vdec_pool = &g_vdec_info.vdec_pool[index];
	secure_pool_free(vdec_pool->pool, addr, alignsize);
	ret = tee_check_out_mem(addr, alignsize);
	if (ret) {
		pr_inf("Secmem checkout err %x\n", ret);
		ret = 0;
	}
	if (dmabuf_manage_debug >= 6) {
		list_for_each_safe(pos, tmp, &block_list) {
			block = list_entry(pos, struct block_node, list);
			if (block->size == alignsize && block->paddr == addr) {
				list_del(&block->list);
				kfree(block);
			}
		}
	}
	vdec_pool->block_num--;
	if (vdec_pool->block_num == 0)
		secure_subpool_destroy(vdec_pool);
	forcedebug = 0;
error_free:
	dmabuf_manage_dump_vdec_info(forcedebug);
	mutex_unlock(&g_dmabuf_mutex);
	return ret;
}

unsigned long secure_get_pool_freesize(phys_addr_t paddr, unsigned long id, unsigned long maxsize)
{
	u32 index = 0;
	struct secure_pool_info *vdec_pool;
	u32 freesize = 0;
	u32 new_pool = 0;

	mutex_lock(&g_dmabuf_mutex);
	pr_enter();
	if (!g_vdec_info.used)
		goto error;
	vdec_pool = &g_vdec_info.vdec_pool[index];
	if (secure_heap_version > SECURE_HEAP_SUPPORT_DELAY_ALLOC_VERSION && id > 0) {
		index = secure_get_vdec_pool_by_instance(id);
		if (index >= SECURE_MAX_VDEC_POOL_NUM) { //no bind
			index = secure_get_vdec_pool_by_paddr(paddr);
			if (index >= SECURE_MAX_VDEC_POOL_NUM) { //all free
				new_pool = 1;
			} else {
				if (g_vdec_info.vdec_pool[index].bind) {
					index = secure_get_subpoolindex_by_framesize(maxsize, 0);
					if (index >= SECURE_MAX_VDEC_POOL_NUM) {//no pool
						new_pool = 1;
					}
				}
			}
			if (new_pool) {
				freesize = secure_get_subpoolsize_by_framesize(maxsize);
				goto error;
			}
		}
		vdec_pool = &g_vdec_info.vdec_pool[index];
		if (!vdec_pool->bind) {
			vdec_pool->instance = id;
			vdec_pool->bind = 1;
		}
	}
	freesize = secure_pool_free_size(vdec_pool->pool);
error:
	mutex_unlock(&g_dmabuf_mutex);
	return freesize;
}

unsigned int dmabuf_manage_get_blocknum(unsigned long id)
{
	u32 num = 0;
	struct secure_pool_info *vdec_pool;
	u32 index = 0;

	mutex_lock(&g_dmabuf_mutex);
	pr_enter();
	if (g_vdec_info.used && id > 0) {
		index = secure_get_vdec_pool_by_instance(id);
		if (index < SECURE_MAX_VDEC_POOL_NUM) {
			vdec_pool = &g_vdec_info.vdec_pool[index];
			num = vdec_pool->block_num;
		}
	}
	mutex_unlock(&g_dmabuf_mutex);
	return num;
}

unsigned int dmabuf_manage_get_secure_heap_version(void)
{
	u32 version = 0;

	pr_enter();
	if (secure_heap_version < SECURE_HEAP_DEFAULT_VERSION ||
		secure_heap_version >= SECURE_HEAP_MAX_VERSION)
		version = SECURE_HEAP_DEFAULT_VERSION;
	else
		version = secure_heap_version;
	pr_inf("version %d\n", version);
	return version;
}

static int dmabuf_manage_open(struct inode *inodep, struct file *filep)
{
	pr_enter();
	return 0;
}

static ssize_t dmabuf_manage_read(struct file *filep, char *buffer,
			   size_t len, loff_t *offset)
{
	pr_enter();
	return 0;
}

static ssize_t dmabuf_manage_write(struct file *filep, const char *buffer,
				size_t len, loff_t *offset)
{
	pr_enter();
	return 0;
}

static int dmabuf_manage_release(struct inode *inodep, struct file *filep)
{
	pr_enter();
	return 0;
}

static long dmabuf_manage_ioctl(struct file *filep, unsigned int cmd,
			 unsigned long args)
{
	long ret = -EINVAL;

	pr_inf("cmd %x\n", cmd);
	switch (cmd) {
	case DMABUF_MANAGE_EXPORT_DMA:
		ret = dmabuf_manage_export(args);
		break;
	case DMABUF_MANAGE_GET_HANDLE:
		ret = dmabuf_manage_get_handle(args);
		break;
	case DMABUF_MANAGE_GET_PHYADDR:
		ret = dmabuf_manage_get_phyaddr(args);
		break;
	case DMABUF_MANAGE_IMPORT_DMA:
		ret = dmabuf_manage_import(args);
		break;
	case DMABUF_MANAGE_SET_VDEC_INFO:
		ret = secure_block_pool_init();
		break;
	case DMABUF_MANAGE_VERSION:
		ret = AML_DMA_BUF_MANAGER_VERSION;
		break;
	case DMABUF_MANAGE_EXPORT_DMABUF:
		ret = dmabuf_manage_export_dmabuf(args);
		break;
	case DMABUF_MANAGE_GET_DMABUFINFO:
		ret = dmabuf_manage_get_dmabufinfo(args);
		break;
	case DMABUF_MANAGE_SET_FILTERFD:
		ret = dmabuf_manage_set_filterfd(args);
		break;
	default:
		break;
	}
	return ret;
}

#ifdef CONFIG_COMPAT
static long dmabuf_manage_compat_ioctl(struct file *filep, unsigned int cmd,
				unsigned long args)
{
	unsigned long ret;

	args = (unsigned long)compat_ptr(args);
	ret = dmabuf_manage_ioctl(filep, cmd, args);
	return ret;
}
#endif

const struct file_operations fops = {
	.owner = THIS_MODULE,
	.open = dmabuf_manage_open,
	.read = dmabuf_manage_read,
	.write = dmabuf_manage_write,
	.release = dmabuf_manage_release,
	.unlocked_ioctl = dmabuf_manage_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = dmabuf_manage_compat_ioctl,
#endif
};

static ssize_t dmabuf_manage_dump_show(struct class *class,
				  struct class_attribute *attr, char *buf)
{
	mutex_lock(&g_dmabuf_mutex);
	dmabuf_manage_dump_vdec_info(1);
	mutex_unlock(&g_dmabuf_mutex);
	return 0;
}

static ssize_t dmabuf_manage_config_show(struct class *class,
	struct class_attribute *attr, char *buf)
{
	ssize_t ret;

	ret = configs_list_path_nodes(CONFIG_PATH, buf, PAGE_SIZE,
					  LIST_MODE_NODE_CMDVAL_ALL);
	return ret;
}

static ssize_t dmabuf_manage_config_store(struct class *class,
			struct class_attribute *attr,
			const char *buf, size_t size)
{
	int ret;

	ret = configs_set_prefix_path_valonpath(CONFIG_PREFIX, buf);
	if (ret < 0) {
		pr_err("set config failed %s\n", buf);
	} else {
		if (buf && strstr(buf, "secure_vdec_def_size_bytes"))
			secure_block_pool_init();
	}
	return size;
}

static struct mconfig dmabuf_manage_configs[] = {
	MC_PI32("secure_vdec_def_size_bytes", &secure_vdec_def_size_bytes),
	MC_PI32("secure_block_align_2n", &secure_block_align_2n),
	MC_PI32("secure_heap_version", &secure_heap_version)
};

static CLASS_ATTR_RO(dmabuf_manage_dump);
static CLASS_ATTR_RW(dmabuf_manage_config);

static struct attribute *dmabuf_manage_class_attrs[] = {
	&class_attr_dmabuf_manage_dump.attr,
	&class_attr_dmabuf_manage_config.attr,
	NULL
};

ATTRIBUTE_GROUPS(dmabuf_manage_class);

static struct class dmabuf_manage_class = {
	.name = CLASS_NAME,
	.class_groups = dmabuf_manage_class_groups,
};

int __init dmabuf_manage_init(void)
{
	int ret;

	ret = register_chrdev(0, DEVICE_NAME, &fops);
	if (ret < 0) {
		pr_error("register_chrdev failed\n");
		goto error_register;
	}
	dev_no = ret;
	ret = class_register(&dmabuf_manage_class);
	if (ret < 0) {
		pr_error("class_register failed\n");
		goto error_class;
	}
	dmabuf_manage_dev = device_create(&dmabuf_manage_class,
				   NULL, MKDEV(dev_no, 0),
				   NULL, DEVICE_NAME);
	if (IS_ERR(dmabuf_manage_dev)) {
		pr_error("device_create failed\n");
		ret = PTR_ERR(dmabuf_manage_dev);
		goto error_create;
	}
	REG_PATH_CONFIGS(CONFIG_PATH, dmabuf_manage_configs);
	INIT_LIST_HEAD(&dmx_filter_list);
	pr_dbg("init done\n");
	return 0;
error_create:
	class_unregister(&dmabuf_manage_class);
error_class:
	unregister_chrdev(dev_no, DEVICE_NAME);
error_register:
	return ret;
}

void __exit dmabuf_manage_exit(void)
{
	device_destroy(&dmabuf_manage_class, MKDEV(dev_no, 0));
	class_unregister(&dmabuf_manage_class);
	class_destroy(&dmabuf_manage_class);
	unregister_chrdev(dev_no, DEVICE_NAME);
	pr_dbg("exit done\n");
}

MODULE_LICENSE("GPL");
