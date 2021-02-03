// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/errno.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/module.h>
#include <linux/of_fdt.h>
#include <linux/libfdt_env.h>
#include <linux/of_reserved_mem.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/dma-contiguous.h>
#include <linux/cma.h>
#include <linux/arm-smccc.h>
#undef pr_fmt
#define pr_fmt(fmt) "secmon: " fmt

static void __iomem *sharemem_in_base;
static void __iomem *sharemem_out_base;
static long phy_in_base;
static long phy_out_base;
static unsigned long secmon_start_virt;

#ifdef CONFIG_ARM64
#define IN_SIZE	0x1000
#else
 #define IN_SIZE	0x1000
#endif
 #define OUT_SIZE 0x1000
static DEFINE_MUTEX(sharemem_mutex);
#define DEV_REGISTERED 1
#define DEV_UNREGISTED 0
static int secmon_dev_registered = DEV_UNREGISTED;
static long get_sharemem_info(unsigned int function_id)
{
	struct arm_smccc_res res;

	arm_smccc_smc(function_id, 0, 0, 0, 0, 0, 0, 0, &res);

	return res.a0;
}

#define RESERVE_MEM_SIZE	0x300000

int within_secmon_region(unsigned long addr)
{
	if (!secmon_start_virt)
		return 0;

	if (addr >= secmon_start_virt &&
	    addr <= (secmon_start_virt + RESERVE_MEM_SIZE))
		return 1;

	return 0;
}

static int secmon_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	unsigned int id;
	int ret;
	int mem_size;
	struct page *page;

	if (!of_property_read_u32(np, "in_base_func", &id))
		phy_in_base = get_sharemem_info(id);

	if (!of_property_read_u32(np, "out_base_func", &id))
		phy_out_base = get_sharemem_info(id);

	if (of_property_read_u32(np, "reserve_mem_size", &mem_size)) {
		pr_err("can't get reserve_mem_size, use default value\n");
		mem_size = RESERVE_MEM_SIZE;
	} else {
		pr_info("reserve_mem_size:0x%x\n", mem_size);
	}

	ret = of_reserved_mem_device_init(&pdev->dev);
	if (ret) {
		pr_info("reserve memory init fail:%d\n", ret);
		return ret;
	}

	page = dma_alloc_from_contiguous(&pdev->dev, mem_size >> PAGE_SHIFT, 0, 0);
	if (!page) {
		pr_err("alloc page failed, ret:%p\n", page);
		return -ENOMEM;
	}
	pr_info("get page:%p, %lx\n", page, page_to_pfn(page));
	secmon_start_virt = (unsigned long)page_to_virt(page);

	if (pfn_valid(__phys_to_pfn(phy_in_base)))
		sharemem_in_base = (void __iomem *)__phys_to_virt(phy_in_base);
	else
		sharemem_in_base = ioremap_cache(phy_in_base, IN_SIZE);

	if (!sharemem_in_base) {
		pr_info("secmon share mem in buffer remap fail!\n");
		return -ENOMEM;
	}

	if (pfn_valid(__phys_to_pfn(phy_out_base)))
		sharemem_out_base = (void __iomem *)
				__phys_to_virt(phy_out_base);
	else
		sharemem_out_base = ioremap_cache(phy_out_base, OUT_SIZE);

	if (!sharemem_out_base) {
		pr_info("secmon share mem out buffer remap fail!\n");
		return -ENOMEM;
	}
	secmon_dev_registered = DEV_REGISTERED;
	pr_info("share in base: 0x%lx, share out base: 0x%lx\n",
		(long)sharemem_in_base, (long)sharemem_out_base);
	pr_info("phy_in_base: 0x%lx, phy_out_base: 0x%lx\n",
		phy_in_base, phy_out_base);

	return ret;
}

void __init secmon_clear_cma_mmu(void)
{
	struct device_node *np;
	unsigned int clear[2] = {};

	np = of_find_node_by_name(NULL, "secmon");
	if (!np)
		return;

	if (of_property_read_u32_array(np, "clear_range", clear, 2))
		pr_info("can't fine clear_range\n");
	else
		pr_info("clear_range:%x %x\n", clear[0], clear[1]);

	if (clear[0]) {
		struct page *page = phys_to_page(clear[0]);
		int cnt = clear[1] / PAGE_SIZE;

		cma_mmu_op(page, cnt, 0);
	}
}

static const struct of_device_id secmon_dt_match[] = {
	{ .compatible = "amlogic, secmon" },
	{ /* sentinel */ },
};

static  struct platform_driver secmon_platform_driver = {
	.probe		= secmon_probe,
	.driver		= {
		.owner		= THIS_MODULE,
		.name		= "secmon",
		.of_match_table	= secmon_dt_match,
	},
};

int __init meson_secmon_init(void)
{
	int ret;

	ret = platform_driver_register(&secmon_platform_driver);
	WARN((secmon_dev_registered != DEV_REGISTERED),
			"ERROR: secmon device must be enable!!!\n");
	return ret;
}
subsys_initcall(meson_secmon_init);

void meson_sm_mutex_lock(void)
{
	mutex_lock(&sharemem_mutex);
}
EXPORT_SYMBOL(meson_sm_mutex_lock);

void meson_sm_mutex_unlock(void)
{
	mutex_unlock(&sharemem_mutex);
}
EXPORT_SYMBOL(meson_sm_mutex_unlock);

void __iomem *get_meson_sm_input_base(void)
{
	return sharemem_in_base;
}
EXPORT_SYMBOL(get_meson_sm_input_base);
void __iomem *get_meson_sm_output_base(void)
{
	return sharemem_out_base;
}
EXPORT_SYMBOL(get_meson_sm_output_base);

long get_secmon_phy_input_base(void)
{
	return phy_in_base;
}

long get_secmon_phy_output_base(void)
{
	return phy_out_base;
}
