/*
 * drivers/amlogic/memory_ext/ram_dump.c
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

#include <linux/version.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/reboot.h>
#include <linux/memblock.h>
#include <linux/amlogic/ramdump.h>
#include <linux/amlogic/reboot.h>
#include <linux/arm-smccc.h>
#include <linux/of.h>
#include <linux/highmem.h>
#include <linux/syscalls.h>
#include <linux/fs_struct.h>
#include <linux/mount.h>
#include <linux/namei.h>
#include <linux/capability.h>
#include <asm/cacheflush.h>

static unsigned long ramdump_base	__initdata;
static unsigned long ramdump_size	__initdata;
static bool ramdump_disable		__initdata;

#define WAIT_TIMEOUT		(40ULL * 1000 * 1000 * 1000)

struct ramdump {
	unsigned long		mem_base;
	unsigned long		mem_size;
	unsigned long long	tick;
	void			*mnt_buf;
	const char		*storage_device;
	struct delayed_work	work;
	int			disable;
};

static struct ramdump *ram;

static int __init early_ramdump_para(char *buf)
{
	int ret;

	if (!buf)
		return -EINVAL;

	pr_info("%s:%s\n", __func__, buf);
	if (strcmp(buf, "disabled") == 0) {
		ramdump_disable = 1;
	} else {
		ret = sscanf(buf, "%lx,%lx", &ramdump_base, &ramdump_size);
		if (ret != 2) {
			pr_err("invalid boot args\n");
			ramdump_disable = 1;
		}
		pr_info("%s, base:%lx, size:%lx\n",
			__func__, ramdump_base, ramdump_size);
		ret = memblock_reserve(ramdump_base, PAGE_ALIGN(ramdump_size));
		if (ret < 0) {
			pr_info("reserve memblock %lx - %lx failed\n",
				ramdump_base,
				ramdump_base + PAGE_ALIGN(ramdump_size));
			ramdump_disable = 1;
		}
	}
	return 0;
}
early_param("ramdump", early_ramdump_para);

/*
 * clear memory to avoid large amount of memory not used.
 * for ramdom data, it's hard to compress
 */
static void lazy_clear_work(struct work_struct *work)
{
	struct page *page;
	struct list_head head, *pos, *next;
	void *virt;
	int order;
	gfp_t flags = __GFP_NORETRY   |
		      __GFP_NOWARN    |
		      __GFP_MOVABLE;
	unsigned long clear = 0, size = 0, free = 0, tick;

	INIT_LIST_HEAD(&head);
	order = MAX_ORDER - 1;
	tick = sched_clock();
	do {
		page = alloc_pages(flags, order);
		if (page) {
			list_add(&page->lru, &head);
			virt = page_address(page);
			size = (1 << order) * PAGE_SIZE;
			memset(virt, 0, size);
			clear += size;
		}
	} while (page);
	tick = sched_clock() - tick;

	list_for_each_safe(pos, next, &head) {
		page = list_entry(pos, struct page, lru);
		list_del(&page->lru);
		__free_pages(page, order);
		free += size;
	}
	pr_info("clear:%lx, free:%lx, tick:%ld us\n",
		clear, free, tick / 1000);
}

#ifdef CONFIG_ARM64
void ramdump_sync_data(void)
{
	/*
	 * back port from old kernel verion for function
	 * flush_cache_all(), we need it for ram dump
	 */
	asm volatile (
		"mov	x12, x30			\n"
		"dsb	sy				\n"
		"mrs	x0, clidr_el1			\n"
		"and	x3, x0, #0x7000000		\n"
		"lsr	x3, x3, #23			\n"
		"cbz	x3, finished			\n"
		"mov	x10, #0				\n"
	"loop1:						\n"
		"add	x2, x10, x10, lsr #1		\n"
		"lsr	x1, x0, x2			\n"
		"and	x1, x1, #7			\n"
		"cmp	x1, #2				\n"
		"b.lt	skip				\n"
		"mrs	x9, daif			\n"
		"msr    daifset, #2 			\n"
		"msr	csselr_el1, x10			\n"
		"isb					\n"
		"mrs	x1, ccsidr_el1			\n"
		"msr    daif, x9			\n"
		"and	x2, x1, #7			\n"
		"add	x2, x2, #4			\n"
		"mov	x4, #0x3ff			\n"
		"and	x4, x4, x1, lsr #3		\n"
		"clz	w5, w4				\n"
		"mov	x7, #0x7fff			\n"
		"and	x7, x7, x1, lsr #13		\n"
	"loop2:						\n"
		"mov	x9, x4				\n"
	"loop3:						\n"
		"lsl	x6, x9, x5			\n"
		"orr	x11, x10, x6			\n"
		"lsl	x6, x7, x2			\n"
		"orr	x11, x11, x6			\n"
		"dc	cisw, x11			\n"
		"subs	x9, x9, #1			\n"
		"b.ge	loop3				\n"
		"subs	x7, x7, #1			\n"
		"b.ge	loop2				\n"
	"skip:						\n"
		"add	x10, x10, #2			\n"
		"cmp	x3, x10				\n"
		"b.gt	loop1				\n"
	"finished:					\n"
		"mov	x10, #0				\n"
		"msr	csselr_el1, x10			\n"
		"dsb	sy				\n"
		"isb					\n"
		"mov	x0, #0				\n"
		"ic	ialluis				\n"
		"ret	x12				\n"
	);
}
#else
void ramdump_sync_data(void)
{
	flush_cache_all();
}
#endif

#ifdef CONFIG_64BIT
static void free_reserved_highmem(unsigned long start, unsigned long end)
{
}
#else
static void free_reserved_highmem(unsigned long start, unsigned long end)
{
	for (; start < end; ) {
		free_highmem_page(phys_to_page(start));
		start += PAGE_SIZE;
	}
}
#endif

static void free_reserved_mem(unsigned long start, unsigned long size)
{
	unsigned long end = PAGE_ALIGN(start + size);
	struct page *page, *epage;

	page = phys_to_page(start);
	if (PageHighMem(page)) {
		free_reserved_highmem(start, end);
	} else {
		epage = phys_to_page(end);
		if (!PageHighMem(epage)) {
			free_reserved_area(__va(start),
					   __va(end), 0, "ramdump");
		} else {
			/* reserved area cross zone */
			struct zone *zone;
			unsigned long bound;

			zone  = page_zone(page);
			bound = zone_end_pfn(zone);
			free_reserved_area(__va(start),
					   __va(bound << PAGE_SHIFT),
					   0, "ramdump");
			zone  = page_zone(epage);
			bound = zone->zone_start_pfn;
			free_reserved_highmem(bound << PAGE_SHIFT, end);
		}
	}
}

static int check_storage_mounted(char **root)
{
	int fd, cnt, ret = 0;
	char mnt_dev[64] =  {}, *mnt_ptr, *root_dir;

	fd = sys_open("/proc/mounts", O_RDONLY, 0);
	if (!fd) {
		pr_debug("%s, open mounts failed:%d\n", __func__, fd);
		return -EINVAL;
	}
	cnt = sys_read(fd, ram->mnt_buf, PAGE_SIZE);
	if (cnt < 0) {
		pr_debug("%s, read mounts failed:%d\n", __func__, cnt);
		ret = -ENODEV;
		goto exit;
	}
	pr_debug("read:%d, %s\n", cnt, (char *)ram->mnt_buf);
	sprintf(mnt_dev, "/dev/block/%s", ram->storage_device);
	mnt_ptr = strstr((char *)ram->mnt_buf, mnt_dev);
	if (mnt_ptr) {
		pr_debug("%s, find %s in buffer, ptr:%p\n",
			__func__, mnt_dev, mnt_ptr);
		root_dir = strstr(mnt_ptr, " ");
		root_dir++;
		*root = root_dir;
		pr_debug("mount:%s root:%s\n", mnt_ptr, root_dir);
	} else
		ret = -ENODEV;
exit:
	sys_close(fd);
	return ret;
}

static size_t save_data(int fd)
{
	unsigned long saved = 0, off = 0, s = 0, e;
	void *buffer;
	int block = (1 << (PAGE_SHIFT + MAX_ORDER - 1)), wsize = 0, ret, i;
	struct vm_struct *area;
	struct page *page, **pages = NULL;

	area = get_vm_area(block, VM_ALLOC);
	if (!area) {
		pr_err("%s, get vma failed\n", __func__);
		return -ENOMEM;
	}

	pages = kzalloc(sizeof(unsigned long) * MAX_ORDER_NR_PAGES, GFP_KERNEL);
	if (!pages)
		goto out;

	buffer = area->addr;
	while (saved < ram->mem_size) {
		s = ram->mem_size - saved;
		if (s >= block)
			wsize = block;
		else
			wsize = s;

		s = ram->mem_base + off;
		e = s + PAGE_ALIGN(wsize);

		page = phys_to_page(s);
		for (i = 0; i < MAX_ORDER_NR_PAGES; i++) {
			pages[i] = page;
			page++;
		}
		ret = map_kernel_range_noflush((unsigned long)buffer,
						PAGE_ALIGN(wsize),
						PAGE_KERNEL,
						pages);
		if (!ret) {
			pr_err("map page:%lx failed\n", page_to_pfn(page));
			goto out;
		}
		ret = sys_write(fd, buffer, wsize);
		if (ret != wsize) {
			unmap_kernel_range((unsigned long)buffer,
					   PAGE_ALIGN(wsize));
			pr_err("%s, write failed\n", __func__);
			goto out;
		}
		unmap_kernel_range((unsigned long)buffer, PAGE_ALIGN(wsize));
		free_reserved_mem(s, PAGE_ALIGN(wsize));
		saved += wsize;
		off   += wsize;
		pr_debug("%s, write %08lx, size:%08x, saved:%08lx, off:%lx\n",
			__func__, s, wsize, saved, off);
	}
out:
	free_vm_area(area);
	kfree(pages);
	pr_info("%s, write %08lx, size:%08x, saved:%08lx, off:%lx\n",
		__func__, s, wsize, saved, off);
	return saved;
}

#define OPEN_FLAGS	(O_WRONLY | O_CREAT | O_DSYNC | O_TRUNC)

static void wait_to_save(struct work_struct *work)
{
	char *root;
	int fd, ret = 0;
	int need_reboot = 0;

	if ((sched_clock() - ram->tick) >= WAIT_TIMEOUT) {
		pr_err("can't find mounted device, free saved data\n");
		need_reboot = 3;
		goto exit;
	}

	if (!check_storage_mounted(&root)) {
		char wname[64] = {}, *next_token;

		/* write compressed data to storage device */
		next_token = strstr(root, " ");
		if (next_token)
			*next_token = '\0';
		sprintf(wname, "%s/crashdump-1.bin", root);
		fd = sys_open(wname, OPEN_FLAGS, 0644);
		if (fd < 0) {
			pr_info("open %s failed:%d\n", wname, fd);
			need_reboot = 3;
			goto exit;
		}
		ret = save_data(fd);
		if (ret != ram->mem_size) {
			pr_err("write size %d not match %ld\n",
				ret, ram->mem_size);
		}
		sys_fsync(fd);
		sys_close(fd);
		sys_sync();
		need_reboot = 1;
	} else
		schedule_delayed_work(&ram->work, 500);

exit:
	/* Nomatter what happened, reboot must be done in this function */
	if (need_reboot) {
		if (need_reboot & 0x1)
			kfree(ram->mnt_buf);
		if (need_reboot & 0x02)
			free_reserved_mem(ram->mem_base, ram->mem_size);
		kernel_restart("RAM-DUMP finished");
	}
}

static int __init ramdump_probe(struct platform_device *pdev)
{
	void __iomem *p = NULL;
	struct device_node *np;
	unsigned long dts_memory[2] = {0}, total_mem;
	struct resource *res;
	unsigned int dump_set;
	int ret;
	void __iomem *base;
	const char *dev_name = NULL;

	np = of_find_node_by_name(NULL, "memory");
	if (!np)
		return -EINVAL;

#ifdef CONFIG_64BIT
	ret = of_property_read_u64_array(np, "linux,usable-memory",
					 (u64 *)&dts_memory, 2);
	if (ret)
		ret = of_property_read_u64_array(np, "reg",
						 (u64 *)&dts_memory, 2);
#else
	ret = of_property_read_u32_array(np, "linux,usable-memory",
					 (u32 *)&dts_memory, 2);
	if (ret)
		ret = of_property_read_u32_array(np, "reg",
						 (u32 *)&dts_memory, 2);
#endif
	if (ret)
		pr_info("can't get dts memory\n");
	else
		pr_info("MEMORY:[%lx+%lx]\n", dts_memory[0], dts_memory[1]);
	of_node_put(np);

	/*
	 * memory in dts is [start_addr size] patten. For amlogic soc,
	 * ddr address range is started from 0x0, usually start_addr in
	 * dts should be started with 0x0, but some soc must reserve a
	 * small framgment of memory at 0x0 for start up code. So start_addr
	 * can be 0x100000/0x1000000. But we always using 0x0 to get real
	 * DDR size for ramdump. So we using following formula to get total
	 * DDR size.
	 */
	total_mem = dts_memory[0] + dts_memory[1];

	ram = kzalloc(sizeof(struct ramdump), GFP_KERNEL);
	if (!ram)
		return -ENOMEM;

	if (ramdump_disable)
		ram->disable = 1;

	np  = pdev->dev.of_node;
	ret = of_property_read_string(np, "store_device", &dev_name);
	if (!ret) {
		ram->storage_device = dev_name;
		pr_info("%s, storage device:%s\n", __func__, dev_name);
	}

	if (!ramdump_base || !ramdump_size) {
		pr_info("NO valid ramdump args:%lx %lx\n",
			ramdump_base, ramdump_size);
	} else {
		ram->mem_base = ramdump_base;
		ram->mem_size = ramdump_size;
		pr_info("%s, mem_base:%p, %lx, size:%lx\n",
			__func__, p, ramdump_base, ramdump_size);
	}

	if (!ram->disable) {
		if (!ram->mem_base) {	/* No compressed data */
			INIT_DELAYED_WORK(&ram->work, lazy_clear_work);
		} else {		/* with compressed data */
			INIT_DELAYED_WORK(&ram->work, wait_to_save);
			ram->tick = sched_clock();
			ram->mnt_buf = kmalloc(PAGE_SIZE, GFP_KERNEL);
			WARN_ON(!ram->mnt_buf);
		}
		schedule_delayed_work(&ram->work, 1);
		/* if ramdump is disabled in env, no need to set sticky reg */
		res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						   "PREG_STICKY_REG8");
		if (res) {
			base = devm_ioremap(&pdev->dev, res->start,
					    res->end - res->start);
			if (!base) {
				pr_err("%s, map reg failed\n", __func__);
				goto err;
			}
			dump_set = readl(base);
			dump_set &= ~RAMDUMP_STICKY_DATA_MASK;
			dump_set |= ((total_mem >> 20) | AMLOGIC_KERNEL_BOOTED);
			writel(dump_set, base);
			pr_info("%s, set sticky to %x\n", __func__, dump_set);
		}
	}
	return 0;

err:
	kfree(ram);

	return -EINVAL;
}

static int ramdump_remove(struct platform_device *pdev)
{
	kfree(ram);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id ramdump_dt_match[] = {
	{
		.compatible = "amlogic, ram_dump",
	},
	{},
};
#endif

static struct platform_driver ramdump_driver = {
	.driver = {
		.name  = "mdump",
		.owner = THIS_MODULE,
	#ifdef CONFIG_OF
		.of_match_table = ramdump_dt_match,
	#endif
	},
	.remove = ramdump_remove,
};

static int __init ramdump_init(void)
{
	int ret;

	ret = platform_driver_probe(&ramdump_driver, ramdump_probe);

	return ret;
}

static void __exit ramdump_uninit(void)
{
	platform_driver_unregister(&ramdump_driver);
}

subsys_initcall(ramdump_init);
module_exit(ramdump_uninit);
MODULE_DESCRIPTION("AMLOGIC ramdump driver");
MODULE_LICENSE("GPL");
