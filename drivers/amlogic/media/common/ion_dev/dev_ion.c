// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_reserved_mem.h>
#include <linux/uaccess.h>
#include <linux/scatterlist.h>
#include <linux/dma-buf.h>
#include <linux/cma.h>
#include <linux/ion.h>
#include <linux/amlogic/tee.h>
#include "dev_ion.h"
#include <ion/ion_private.h>

//MODULE_DESCRIPTION("AMLOGIC ION driver");
//MODULE_LICENSE("GPL v2");
//MODULE_AUTHOR("Amlogic SH");

#define MESON_MAX_ION_HEAP 8

static struct heap_type_desc {
	char *name;
	int heap_type;
	struct ion_heap_ops *ops;
	unsigned long flags;
} meson_heap_descs[] = {
	{
		.name = "codec_mm_cma",
		.heap_type = ION_HEAP_TYPE_CUSTOM,
		.ops = &codec_mm_heap_ops,
		.flags = ION_HEAP_FLAG_DEFER_FREE,
	},
	{
		.name = "ion-dev",
		.heap_type = ION_HEAP_TYPE_DMA,
		.ops = &ion_cma_ops,
	},
	{
		.name = "ion-fb",
		.heap_type = ION_HEAP_TYPE_DMA,
		.ops = &ion_cma_ops,
	},
};

struct device *ion_dev;
static int num_heaps;
static struct ion_cma_heap *heaps[MESON_MAX_ION_HEAP];
static struct ion_heap *secure_heap;
static struct ion_device *internal_dev;

struct device *meson_ion_get_dev(void)
{
	return ion_dev;
}
EXPORT_SYMBOL(meson_ion_get_dev);

void meson_ion_buffer_to_phys(struct ion_buffer *buffer,
			      phys_addr_t *addr, size_t *len)
{
	struct sg_table *sg_table;
	struct page *page;

	if (buffer) {
		sg_table = buffer->sg_table;
		page = sg_page(sg_table->sgl);
		*addr = PFN_PHYS(page_to_pfn(page));
		*len = buffer->size;
	}
}
EXPORT_SYMBOL(meson_ion_buffer_to_phys);

int meson_ion_share_fd_to_phys(int fd, phys_addr_t *addr, size_t *len)
{
	struct dma_buf *dmabuf;
	struct ion_buffer *buffer;

	dmabuf = dma_buf_get(fd);
	if (IS_ERR_OR_NULL(dmabuf))
		return PTR_ERR(dmabuf);

	buffer = (struct ion_buffer *)dmabuf->priv;
	meson_ion_buffer_to_phys(buffer, addr, len);
	dma_buf_put(dmabuf);

	return 0;
}
EXPORT_SYMBOL(meson_ion_share_fd_to_phys);

static unsigned int meson_ion_heap_id_get(char *heap_name)
{
	int i;
	struct ion_cma_heap *heap;

	for (i = 0; i < num_heaps; i++) {
		heap = heaps[i];
		if (!strcmp(heap->heap.name, heap_name))
			return heap->heap.id;
	}

	return 0;
}

unsigned int meson_ion_cma_heap_id_get(void)
{
	return meson_ion_heap_id_get("ion-dev");
}
EXPORT_SYMBOL(meson_ion_cma_heap_id_get);

unsigned int meson_ion_fb_heap_id_get(void)
{
	return meson_ion_heap_id_get("ion-fb");
}
EXPORT_SYMBOL(meson_ion_fb_heap_id_get);

unsigned int meson_ion_codecmm_heap_id_get(void)
{
	return meson_ion_heap_id_get("codec_mm_cma");
}
EXPORT_SYMBOL(meson_ion_codecmm_heap_id_get);

unsigned int meson_ion_secure_heap_id_get(void)
{
	unsigned int id = 0;

	if (secure_heap)
		id = secure_heap->id;
	return id;
}
EXPORT_SYMBOL(meson_ion_secure_heap_id_get);

static int __meson_ion_add_heap(struct ion_heap *heap,
				struct heap_type_desc *desc)
{
	int ret;

	heap->ops = desc->ops;
	heap->type = desc->heap_type;
	heap->flags = desc->flags;
	heap->name = desc->name;

	ret = ion_device_add_heap(heap);
	if (ret)
		DION_ERROR("%s fail\n", __func__);
	else
		DION_INFO("%s,heap->name=%s heap->id=%d\n",
				  __func__, heap->name, heap->id);

	return ret;
}

static int meson_ion_add_heap(struct cma *cma, void *data)
{
	int i, ret, heap_type;
	bool need_add = false;
	struct heap_type_desc *desc;
	struct ion_cma_heap *heap;
	int *cma_nr = data;
	const char *cma_name = cma_get_name(cma);

	for (i = 0; i < ARRAY_SIZE(meson_heap_descs); i++) {
		desc = &meson_heap_descs[i];

		if (strstr(cma_name, desc->name)) {
			heap_type = desc->heap_type;
			need_add = true;
			break;
		}
	}

	if (need_add) {
		heap = kzalloc(sizeof(*heap), GFP_KERNEL);
		if (!heap)
			return -ENOMEM;
		ret = __meson_ion_add_heap(&heap->heap, desc);
		if (!ret)
			heap->is_added = true;

		heap->cma = cma;
		mutex_init(&heap->mutex);
		heaps[num_heaps++] = heap;
		*cma_nr = num_heaps;
	}

	return 0;
}

static int ion_secure_mem_init(struct reserved_mem *rmem, struct device *dev)
{
#ifdef CONFIG_AMLOGIC_TEE
	u32 secure_heap_handle;
#endif
	int ret;

	secure_heap = ion_secure_heap_create(rmem->base, rmem->size);
	ret = ion_device_add_heap(secure_heap);
	if (ret)
		DION_ERROR("%s fail\n", __func__);
	else
		DION_INFO("%s,secure_heap->name=%s secure_heap->id=%d\n",
			  __func__, secure_heap->name, secure_heap->id);
#ifdef CONFIG_AMLOGIC_TEE
	ret = tee_protect_mem_by_type(TEE_MEM_TYPE_GPU,
					(u32)rmem->base,
					(u32)rmem->size,
					&secure_heap_handle);
	if (ret)
		DION_ERROR("tee protect gpu mem fail!\n");
	else
		DION_INFO("tee protect gpu mem done\n");
#endif
	DION_INFO("ion secure_mem_init size=0x%pa, paddr=0x%pa\n",
		  &rmem->size, &rmem->base);

	return 0;
}

static const struct reserved_mem_ops rmem_ion_secure_ops = {
	.device_init = ion_secure_mem_init,
};

static int __init rmem_ion_secure_setup(struct reserved_mem *rmem)
{
	rmem->ops = &rmem_ion_secure_ops;
	return 0;
}

static int ion_open(struct inode *inode, struct file *filp)
{
	filp->private_data = internal_dev;
	return 0;
}

static int ion_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static const struct file_operations ion_fops = {
	.owner          = THIS_MODULE,
	.open           = ion_open,
	.release        = ion_release,
	.unlocked_ioctl = ion_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= ion_ioctl,
#endif
};

static ssize_t ion_heap_info_show(struct device *dev,
				  struct device_attribute *attr, char * const buf)
{
	ssize_t ret = 0;
	int nr;

	ret += scnprintf(buf + ret, PAGE_SIZE - ret, "%-20s %-20s %-20s %-20s %-20s\n",
			 "name", "heap_id", "num_of_buffers",
			 "num_of_alloc_bytes", "alloc_bytes_wm");
	ret += scnprintf(buf + ret, PAGE_SIZE - ret,
			"--------------------------------------------------------------------------------------------------------\n");

	for (nr = 0; nr < num_heaps; nr++)
		ret += scnprintf(buf + ret, PAGE_SIZE - ret,
				 "%-20s %-20u %-20llu %-20llu %-20llu\n",
				 heaps[nr]->heap.name,
				 heaps[nr]->heap.id,
				 heaps[nr]->heap.num_of_buffers,
				 heaps[nr]->heap.num_of_alloc_bytes,
				 heaps[nr]->heap.alloc_bytes_wm);

	return ret;
}

static DEVICE_ATTR_RO(ion_heap_info);

static struct attribute *ion_heap_info_attrs[] = {
	&dev_attr_ion_heap_info.attr,
	NULL
};

static const struct attribute_group ion_heap_info_attr_group = {
	.attrs = ion_heap_info_attrs,
};

static int dev_ion_probe(struct platform_device *pdev)
{
	int ret;
	int nr = 0;
	int err = 0;
	struct ion_device *idev;
	int secure_flag = 0;
	int secure_region_index = 0;
	struct device_node *search_target = NULL;
#ifdef MODULE
	struct device_node *target = NULL;
	struct reserved_mem *mem = NULL;
#endif

	while (1) {
		search_target = of_parse_phandle(pdev->dev.of_node,
						"memory-region",
						secure_region_index);
		if (!search_target)
			break;
		if (!strcmp(search_target->name, "linux,ion-secure")) {
			secure_flag = 1;
			break;
		}
		secure_region_index++;
	}

	if (secure_flag) {
#ifdef MODULE
		target = of_parse_phandle(pdev->dev.of_node,
					  "memory-region",
					  secure_region_index);
		if (target)
			mem = of_reserved_mem_lookup(target);
		if (mem) {
			ret = rmem_ion_secure_setup(mem);
			if (ret != 0)
				DION_ERROR("failed to ion_secure_mem_init\n");
			else
				DION_INFO("ion_secure_mem_init succeed\n");
		}
#endif

		ret = of_reserved_mem_device_init_by_idx(&pdev->dev,
				pdev->dev.of_node, secure_region_index);
		if (ret != 0)
			DION_ERROR("failed get secure memory\n");
	}

	ret = cma_for_each_area(meson_ion_add_heap, &nr);
	if (ret) {
		for (nr = 0; nr < num_heaps && heaps[nr]; nr++) {
			if (heaps[nr]->is_added)
				ion_device_remove_heap(&heaps[nr]->heap);
			kfree(heaps[nr]);
		}
	}
	ion_dev = &pdev->dev;

	idev = kzalloc(sizeof(*idev), GFP_KERNEL);
	if (!idev)
		return -ENOMEM;

	idev->dev.minor = MISC_DYNAMIC_MINOR;
	idev->dev.name = "ion";
	idev->dev.fops = &ion_fops;
	idev->dev.parent = get_device(ion_dev);

	err = sysfs_create_group(&ion_dev->kobj, &ion_heap_info_attr_group);
	if (err) {
		DION_ERROR("ion: failed to register ion_heap_info.\n");
		sysfs_remove_group(&ion_dev->kobj, &ion_heap_info_attr_group);
	}

	ret = misc_register(&idev->dev);
	if (ret) {
		DION_ERROR("ion: failed to register misc device.\n");
		kfree(idev);
	}

	return ret;
}

static int dev_ion_remove(struct platform_device *pdev)
{
	if (secure_heap)
		ion_secure_heap_destroy(secure_heap);
	return 0;
}

static const struct of_device_id amlogic_ion_dev_dt_match[] = {
	{ .compatible = "amlogic, ion_dev", },
	{ },
};

static struct platform_driver ion_driver = {
	.probe = dev_ion_probe,
	.remove = dev_ion_remove,
	.driver = {
		.name = "ion_dev",
		.owner = THIS_MODULE,
		.of_match_table = amlogic_ion_dev_dt_match
	}
};

RESERVEDMEM_OF_DECLARE(ion_secure_mem,
					"amlogic, ion-secure-mem",
					rmem_ion_secure_setup);

int __init ion_init(void)
{
	return platform_driver_register(&ion_driver);
}

void __exit ion_exit(void)
{
	platform_driver_unregister(&ion_driver);
}

