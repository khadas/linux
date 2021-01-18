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
#include "dev_ion.h"

//MODULE_DESCRIPTION("AMLOGIC ION driver");
//MODULE_LICENSE("GPL v2");
//MODULE_AUTHOR("Amlogic SH");

#define DION_ERROR(fmt, args ...)	pr_err("ion_dev: " fmt, ## args)
#define DION_INFO(fmt, args ...)	pr_info("ion_dev: " fmt, ## args)
#define DION_DEBUG(fmt, args ...)	pr_debug("ion_dev: " fmt, ## args)

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
};

struct device *ion_dev;
static int num_heaps;
static struct ion_cma_heap *heaps[MESON_MAX_ION_HEAP];

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

unsigned int meson_ion_codecmm_heap_id_get(void)
{
	return meson_ion_heap_id_get("codec_mm_cma");
}
EXPORT_SYMBOL(meson_ion_codecmm_heap_id_get);

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
		heaps[num_heaps++] = heap;
		*cma_nr = num_heaps;
	}

	return 0;
}

static int dev_ion_probe(struct platform_device *pdev)
{
	int ret;
	int nr = 0;

	ret = cma_for_each_area(meson_ion_add_heap, &nr);
	if (ret) {
		for (nr = 0; nr < num_heaps && heaps[nr]; nr++) {
			if (heaps[nr]->is_added)
				ion_device_remove_heap(&heaps[nr]->heap);
			kfree(heaps[nr]);
		}
	}
	ion_dev = &pdev->dev;

	return ret;
}

static int dev_ion_remove(struct platform_device *pdev)
{
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

int __init ion_init(void)
{
	return platform_driver_register(&ion_driver);
}

void __exit ion_exit(void)
{
	platform_driver_unregister(&ion_driver);
}

