// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
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
#include <linux/amlogic/tee.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/amlogic/media/codec_mm/secmem.h>

static int secmem_debug;
module_param(secmem_debug, int, 0644);

static int secmem_vdec_def_size_bytes;
module_param(secmem_vdec_def_size_bytes, int, 0644);

#define  DEVICE_NAME "secmem"
#define  CLASS_NAME  "secmem"

#define dprintk(level, fmt, arg...)					      \
	do {								      \
		if (secmem_debug >= (level))					      \
			pr_info(DEVICE_NAME ": %s: " fmt,  __func__, ## arg); \
	} while (0)

#define pr_dbg(fmt, args ...)  dprintk(6, fmt, ## args)
#define pr_error(fmt, args ...) dprintk(1, fmt, ## args)
#define pr_inf(fmt, args ...) dprintk(8, fmt, ## args)
#define pr_enter() pr_inf("enter")

struct ksecmem_attachment {
	struct sg_table sgt;
	enum dma_data_direction dma_dir;
};

#define SZ_1_5G				0x60000000
#define SECMEM_MM_ALIGNED_2N		20
#define SECMEM_MM_BLOCK_ALIGNED_2N	16
#define SECMEM_MM_MAX_VDEC_SIZE_BYTES		(32 * 1024 * 1024)
#define SECMEM_MM_DEF_VDEC_SIZE_BYTES		(28 * 1024 * 1024)
#define SECMEM_MM_MID_VDEC_SIZE_BYTES		(20 * 1024 * 1024)
#define SECMEM_MM_MIN_VDEC_SIZE_BYTES		(8 * 1024 * 1024)
#define SECMEM_MM_MIN_CMA_SIZE_BYTES		(230 * 1024 * 1024)

struct secmem_vdec_info {
	u32 used;
	struct gen_pool *pool;
	u32 vdec_handle;
	u32 vdec_paddr;
	u32 vdec_size;
	u32 vdec_pool_paddr;
	u32 vdec_pool_size;
};

static int dev_no;
static struct device *secmem_dev;
static DEFINE_MUTEX(g_secmem_mutex);
static struct secmem_vdec_info g_vdec_info;

static int secmem_dma_attach(struct dma_buf *dbuf, struct dma_buf_attachment *attachment)
{
	struct ksecmem_attachment *attach;
	struct secmem_block *info = dbuf->priv;
	struct sg_table *sgt;
	struct page *page;
	phys_addr_t phys = info->paddr;
	int ret;

	pr_enter();
	attach = (struct ksecmem_attachment *)
		kzalloc(sizeof(*attach), GFP_KERNEL);
	if (!attach) {
		pr_error("kzalloc failed\n");
		return -ENOMEM;
	}

	sgt = &attach->sgt;
	ret = sg_alloc_table(sgt, 1, GFP_KERNEL);
	if (ret) {
		kfree(attach);
		return -ENOMEM;
	}

	page = phys_to_page(phys);
	sg_set_page(sgt->sgl, page, PAGE_ALIGN(info->size), 0);

	attach->dma_dir = DMA_NONE;
	attachment->priv = attach;

	return 0;
}

static void secmem_dma_detach(struct dma_buf *dbuf,
			      struct dma_buf_attachment *attachment)
{
	struct ksecmem_attachment *attach = attachment->priv;
	struct sg_table *sgt;

	pr_enter();
	if (!attach)
		return;

	sgt = &attach->sgt;

//	if (attach->dma_dir != DMA_NONE)
//		dma_unmap_sg(attachment->dev, sgt->sgl, sgt->orig_nents,
//			     attach->dma_dir);
	sg_free_table(sgt);
	kfree(attach);
	attachment->priv = NULL;
}

static struct sg_table *secmem_dma_map_dma_buf(struct dma_buf_attachment *attachment,
		enum dma_data_direction dma_dir)
{
	struct ksecmem_attachment *attach = attachment->priv;
	struct secmem_block *info = attachment->dmabuf->priv;
	struct mutex *lock = &attachment->dmabuf->lock;
	struct sg_table *sgt;

	pr_enter();
	mutex_lock(lock);
	sgt = &attach->sgt;
	if (attach->dma_dir == dma_dir) {
		mutex_unlock(lock);
		return sgt;
	}

	sgt->sgl->dma_address = info->paddr;
#ifdef CONFIG_NEED_SG_DMA_LENGTH
	sgt->sgl->dma_length = PAGE_ALIGN(info->size);
#else
	sgt->sgl->length = PAGE_ALIGN(info->size);
#endif
	pr_dbg("nents %d, %x, %d, %d\n", sgt->nents, info->paddr,
			sg_dma_len(sgt->sgl), info->size);
	attach->dma_dir = dma_dir;

	mutex_unlock(lock);
	return sgt;
}

static void secmem_dma_unmap_dma_buf(struct dma_buf_attachment *attachment,
		struct sg_table *sgt,
		enum dma_data_direction dma_dir)
{
	pr_enter();
}

static void secmem_dma_release(struct dma_buf *dbuf)
{
	struct secmem_block *info;

	pr_enter();
	info = (struct secmem_block *)dbuf->priv;
	pr_dbg("secmem dma release handle:%x\n", info->handle);
	kfree(info);
}

static int secmem_dma_mmap(struct dma_buf *dbuf, struct vm_area_struct *vma)
{
	pr_enter();
	return -EINVAL;
}

static struct dma_buf_ops secmem_dmabuf_ops = {
	.attach = secmem_dma_attach,
	.detach = secmem_dma_detach,
	.map_dma_buf = secmem_dma_map_dma_buf,
	.unmap_dma_buf = secmem_dma_unmap_dma_buf,
	.release = secmem_dma_release,
	.mmap = secmem_dma_mmap
};

static struct dma_buf *get_dmabuf(struct secmem_block *info,
				  unsigned long flags)
{
	struct dma_buf *dbuf;
	DEFINE_DMA_BUF_EXPORT_INFO(exp_info);

	pr_dbg("export handle:%x, paddr:%x, size:%d\n",
		info->handle, info->paddr, info->size);
	exp_info.ops = &secmem_dmabuf_ops;
	exp_info.size = info->size;
	exp_info.flags = flags;
	exp_info.priv = (void *)info;
	dbuf = dma_buf_export(&exp_info);
	if (IS_ERR(dbuf))
		return NULL;
	return dbuf;
}

static long secmem_export(unsigned long args)
{
	int ret;
	struct secmem_block *info;
	struct dma_buf *dbuf;
	int fd;

	pr_enter();
	info = (struct secmem_block *)
		kzalloc(sizeof(*info), GFP_KERNEL);
	if (!info) {
		pr_error("kmalloc failed\n");
		goto error_alloc_object;
	}
	ret = copy_from_user((void *)info, (void __user *)args,
			     sizeof(struct secmem_block));
	if (ret) {
		pr_error("copy_from_user failed\n");
		goto error_copy;
	}

	dbuf = get_dmabuf(info, 0);
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
	kfree(info);
error_alloc_object:
	return -EFAULT;
}

static long secmem_get_handle(unsigned long args)
{
	int ret;
	long res = -EFAULT;
	int fd;
	struct dma_buf *dbuf;
	struct secmem_block *info;

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
	info = dbuf->priv;
	res = (long)(info->handle & (0xffffffff));
	dma_buf_put(dbuf);
error_get:
error_copy:
	return res;
}

static long secmem_get_phyaddr(unsigned long args)
{
	int ret;
	long res = -EFAULT;
	int fd;
	struct dma_buf *dbuf;
	struct secmem_block *info;

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
	info = dbuf->priv;
	res = (long)(info->paddr & (0xffffffff));
	dma_buf_put(dbuf);
error_get:
error_copy:
	return res;
}

static long secmem_import(unsigned long args)
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
	attach = dma_buf_attach(dbuf, secmem_dev);
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

static phys_addr_t secure_pool_alloc(void *handle, u32 size)
{
	return gen_pool_alloc(handle, size);
}

static void secure_pool_free(void *handle, u32 addr, u32 size)
{
	return gen_pool_free(handle, addr, size);
}

static size_t secure_pool_size(void *handle)
{
	return gen_pool_size(handle);
}

static size_t secure_pool_free_size(void *handle)
{
	return gen_pool_avail(handle);
}

static inline u32 secmem_align_up2n(u32 size, u32 alg2n)
{
	return ((size + (1 << alg2n) - 1) & (~((1 << alg2n) - 1)));
}

static inline u32 secmem_addr_is_aligned(u32 addr, u32 alg2n)
{
	if ((addr & (~((1 << alg2n) - 1))) == addr)
		return 1;
	return 0;
}

static int secure_block_pool_init(void)
{
	int ret;
	u32 max_cma_size_bytes = codec_mm_get_total_size();
	u32 vdec_size = secmem_vdec_def_size_bytes;
	u32 total_mem = totalram_pages() << PAGE_SHIFT;

	if (g_vdec_info.used)
		return 0;
	if (vdec_size < SECMEM_MM_MIN_VDEC_SIZE_BYTES ||
		vdec_size > SECMEM_MM_MAX_VDEC_SIZE_BYTES) {
		if (total_mem <= SZ_1G) {
			vdec_size = SECMEM_MM_MIN_VDEC_SIZE_BYTES;
		} else if (total_mem > SZ_1G && total_mem < SZ_1_5G) {
			if (max_cma_size_bytes >= SECMEM_MM_MIN_CMA_SIZE_BYTES)
				vdec_size = SECMEM_MM_DEF_VDEC_SIZE_BYTES;
			else
				vdec_size = SECMEM_MM_MID_VDEC_SIZE_BYTES;
		} else {
			vdec_size = SECMEM_MM_DEF_VDEC_SIZE_BYTES;
		}
	}
	vdec_size += SZ_1M;
	g_vdec_info.vdec_size = secmem_align_up2n(vdec_size,
			SECMEM_MM_ALIGNED_2N);
	g_vdec_info.vdec_paddr = codec_mm_alloc_for_dma("dma-secure-buf",
		g_vdec_info.vdec_size / PAGE_SIZE,
		SECMEM_MM_BLOCK_ALIGNED_2N, CODEC_MM_FLAGS_DMA);
	if (!g_vdec_info.vdec_paddr) {
		pr_error("allocate cma size failed %x\n",
			g_vdec_info.vdec_size);
		goto error_init;
	}
	ret = tee_protect_mem_by_type(TEE_MEM_TYPE_STREAM_INPUT,
				(u32)g_vdec_info.vdec_paddr,
				(u32)g_vdec_info.vdec_size,
				&g_vdec_info.vdec_handle);
	if (ret) {
		pr_error("protect vdec failed  addr %x %x ret is %x\n",
			g_vdec_info.vdec_paddr, g_vdec_info.vdec_size, ret);
		goto error_init;
	}
	if (secmem_addr_is_aligned(g_vdec_info.vdec_paddr,
				SECMEM_MM_ALIGNED_2N)) {
		g_vdec_info.vdec_pool_paddr = g_vdec_info.vdec_paddr;
		g_vdec_info.vdec_pool_size = g_vdec_info.vdec_size;
	} else {
		g_vdec_info.vdec_pool_paddr =
			secmem_align_up2n(g_vdec_info.vdec_paddr, SECMEM_MM_ALIGNED_2N);
		g_vdec_info.vdec_pool_size = g_vdec_info.vdec_paddr +
			g_vdec_info.vdec_size - g_vdec_info.vdec_pool_paddr;
	}
	g_vdec_info.pool = secure_pool_init(g_vdec_info.vdec_pool_paddr,
		g_vdec_info.vdec_pool_size, SECMEM_MM_ALIGNED_2N);
	if (!g_vdec_info.pool) {
		pr_error("secmem pool create failed\n");
		goto error_init;
	}
	g_vdec_info.used = 1;
	return 0;
error_init:
	if (g_vdec_info.vdec_handle)
		tee_unprotect_mem(g_vdec_info.vdec_handle);
	if (g_vdec_info.vdec_paddr) {
		if (codec_mm_free_for_dma("dma-secure-buf", g_vdec_info.vdec_paddr))
			pr_error("free dma buf failed in init");
	}
	memset(&g_vdec_info, 0, sizeof(struct secmem_vdec_info));
	return -1;
}

static ssize_t secmem_mm_dump_vdec_info(int forceshow)
{
	char buf[512] = { 0 };
	char *pbuf = buf;
	u32 size = 512;
	u32 s = 0;
	u32 tsize = 0;
	u32 freesize = 0;

	if (forceshow == 0 && secmem_debug < 8)
		return 0;
	s = snprintf(pbuf, size - tsize,
			"secmem mem info:\nused %d\n",
			g_vdec_info.used);
	tsize += s;
	pbuf += s;
	s = snprintf(pbuf, size - tsize,
			"vdec size= %d MB\ndef vdec size: %dMB\n",
			g_vdec_info.vdec_pool_size / SZ_1M,
			secmem_vdec_def_size_bytes / SZ_1M);
	tsize += s;
	pbuf += s;
	if (g_vdec_info.used) {
		freesize = secure_pool_free_size(g_vdec_info.pool);
		s = snprintf(pbuf, size - tsize,
				"Allocate = %zx bytes\nfreed: %d bytes\n",
				secure_pool_size(g_vdec_info.pool) - freesize,
				freesize);
	}
	pr_error("%s", buf);
	return 0;
}

phys_addr_t secure_block_alloc(unsigned long size, unsigned long flags)
{
	phys_addr_t paddr = 0;
	int ret;

	mutex_lock(&g_secmem_mutex);
	pr_enter();
	if (!g_vdec_info.used)
		goto error_alloc;
	paddr = secure_pool_alloc(g_vdec_info.pool, size);
	if (paddr > 0) {
		ret = tee_check_in_mem(paddr, size);
		if (ret)
			pr_error("checkin err \n");
	}
error_alloc:
	secmem_mm_dump_vdec_info(0);
	mutex_unlock(&g_secmem_mutex);
	return paddr;
}

unsigned long secure_block_free(phys_addr_t addr, unsigned long size)
{
	int ret;

	mutex_lock(&g_secmem_mutex);
	pr_enter();
	if (!g_vdec_info.used)
		goto error_free;
	secure_pool_free(g_vdec_info.pool, addr, size);
	ret = tee_check_out_mem(addr, size);
	if (ret) {
		pr_error("checkout err \n");
		goto error_free;
	}
	secmem_mm_dump_vdec_info(0);
	mutex_unlock(&g_secmem_mutex);
	return 0;
error_free:
	secmem_mm_dump_vdec_info(0);
	mutex_unlock(&g_secmem_mutex);
	return -1;
}

static int secmem_open(struct inode *inodep, struct file *filep)
{
	pr_enter();
	return 0;
}

static ssize_t secmem_read(struct file *filep, char *buffer,
			   size_t len, loff_t *offset)
{
	pr_enter();
	return 0;
}

static ssize_t secmem_write(struct file *filep, const char *buffer,
			    size_t len, loff_t *offset)
{
	pr_enter();
	return 0;
}

static int secmem_release(struct inode *inodep, struct file *filep)
{
	pr_enter();
	return 0;
}

static long secmem_ioctl(struct file *filep, unsigned int cmd,
			 unsigned long args)
{
	long ret = -EINVAL;

	pr_inf("cmd %x\n", cmd);
	switch (cmd) {
	case SECMEM_EXPORT_DMA:
		ret = secmem_export(args);
		break;
	case SECMEM_GET_HANDLE:
		ret = secmem_get_handle(args);
		break;
	case SECMEM_GET_PHYADDR:
		ret = secmem_get_phyaddr(args);
		break;
	case SECMEM_IMPORT_DMA:
		ret = secmem_import(args);
		break;
	case SECMEM_SET_VDEC_INFO:
		ret = secure_block_pool_init();
		break;
	default:
		break;
	}
	return ret;
}

#ifdef CONFIG_COMPAT
static long secmem_compat_ioctl(struct file *filep, unsigned int cmd,
				unsigned long args)
{
	unsigned long ret;

	args = (unsigned long)compat_ptr(args);
	ret = secmem_ioctl(filep, cmd, args);
	return ret;
}
#endif

const struct file_operations fops = {
	.owner = THIS_MODULE,
	.open = secmem_open,
	.read = secmem_read,
	.write = secmem_write,
	.release = secmem_release,
	.unlocked_ioctl = secmem_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = secmem_compat_ioctl,
#endif
};

static ssize_t secmem_mm_dump_show(struct class *class,
				  struct class_attribute *attr, char *buf)
{
	mutex_lock(&g_secmem_mutex);
	secmem_mm_dump_vdec_info(1);
	mutex_unlock(&g_secmem_mutex);
	return 0;
}

static CLASS_ATTR_RO(secmem_mm_dump);

static struct attribute *secmem_class_attrs[] = {
	&class_attr_secmem_mm_dump.attr,
	NULL
};

ATTRIBUTE_GROUPS(secmem_class);

static struct class secmem_class = {
	.name = CLASS_NAME,
	.class_groups = secmem_class_groups,
};

int __init secmem_init(void)
{
	int ret;

	ret = register_chrdev(0, DEVICE_NAME, &fops);
	if (ret < 0) {
		pr_error("register_chrdev failed\n");
		goto error_register;
	}
	dev_no = ret;
	ret = class_register(&secmem_class);
	if (ret < 0) {
		pr_error("class_register failed\n");
		goto error_class;
	}

	secmem_dev = device_create(&secmem_class,
				   NULL, MKDEV(dev_no, 0),
				   NULL, DEVICE_NAME);
	if (IS_ERR(secmem_dev)) {
		pr_error("device_create failed\n");
		ret = PTR_ERR(secmem_dev);
		goto error_create;
	}
	pr_dbg("init done\n");
	return 0;
error_create:
	class_unregister(&secmem_class);
error_class:
	unregister_chrdev(dev_no, DEVICE_NAME);
error_register:
	return ret;
}

void __exit secmem_exit(void)
{
	device_destroy(&secmem_class, MKDEV(dev_no, 0));
	class_unregister(&secmem_class);
	class_destroy(&secmem_class);
	unregister_chrdev(dev_no, DEVICE_NAME);
	pr_dbg("exit done\n");
}

MODULE_LICENSE("GPL");
