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
#include <linux/ion.h>
#include "dev_ion.h"

//MODULE_DESCRIPTION("AMLOGIC ION driver");
//MODULE_LICENSE("GPL v2");
//MODULE_AUTHOR("Amlogic SH");

#define DION_ERROR(fmt, args ...)	pr_err("ion_dev: " fmt, ## args)
#define DION_INFO(fmt, args ...)	pr_info("ion_dev: " fmt, ## args)
#define DION_DEBUG(fmt, args ...)	pr_debug("ion_dev: " fmt, ## args)

static const char *ion_rmem_name;
static unsigned int ion_heap_id;
struct device *ion_dev;

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

/*return:1:match,0:unmatch*/
int meson_ion_cma_heap_match(const char *name)
{
	int ret;

	if (!ion_rmem_name) {
		ret = 0;
		DION_INFO("%s, ion_rmem_name is NULL!!\n", __func__);
	} else if (strcmp(name, ion_rmem_name)) {
		ret = 0;
		DION_INFO("%s, ion_rmem.name(%s) unmatch input(%s)\n",
			  __func__, ion_rmem_name, name);
	} else {
		ret = 1;
		DION_INFO("%s, ion_rmem.name(%s) match input(%s)\n",
			  __func__, ion_rmem_name, name);
	}
	return ret;
}
EXPORT_SYMBOL(meson_ion_cma_heap_match);

void meson_ion_cma_heap_id_set(unsigned int id)
{
	ion_heap_id = id;
}
EXPORT_SYMBOL(meson_ion_cma_heap_id_set);

unsigned int meson_ion_cma_heap_id_get(void)
{
	return ion_heap_id;
}

int dev_ion_probe(struct platform_device *pdev)
{
	int err = 0;
	struct device_node *mem_node;
	struct reserved_mem *rmem = NULL;

	/* init reserved memory */
	err = of_reserved_mem_device_init(&pdev->dev);
	if (err != 0) {
		DION_INFO("failed get reserved memory\n");
		return err;
	}
	ion_dev = &pdev->dev;
	mem_node = of_parse_phandle(pdev->dev.of_node, "memory-region", 0);
	if (mem_node)
		rmem = of_reserved_mem_lookup(mem_node);
	of_node_put(mem_node);

	if (rmem) {
		ion_rmem_name = rmem->name;
		DION_INFO("%s, create [%s] heap done\n", __func__, rmem->name);
	} else {
		ion_rmem_name = NULL;
		DION_ERROR("%s, done with NULL rmem!!\n", __func__);
	}
	return 0;
}

int dev_ion_remove(struct platform_device *pdev)
{
	of_reserved_mem_device_release(&pdev->dev);
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

#ifndef MODULE
fs_initcall(ion_init);
module_exit(ion_exit);
#endif
