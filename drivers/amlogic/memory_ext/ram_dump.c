// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
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
#include <linux/sched/clock.h>
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
#include <linux/of_address.h>

static unsigned long ramdump_base	__initdata;
static unsigned long ramdump_size	__initdata;
static bool ramdump_disable	__initdata	=	1;

#define WAIT_TIMEOUT		(40ULL * 1000 * 1000 * 1000)
#define SAVE_DATA_BY_INIT_RC_SHELL			1
#define RESERVE_MEM_BY_RAMDUMP_DTS_NODE		1

struct ramdump {
	unsigned long		mem_size;
	unsigned long		mem_base;
#ifdef SAVE_DATA_BY_INIT_RC_SHELL
	//void __iomem		*mem_base;
	struct mutex		lock;
	struct kobject		*kobj;
#else
	unsigned long long	tick;
	void				*mnt_buf;
	const char			*storage_device;
#endif
	struct delayed_work	work;
	int			disable;
};

static struct ramdump *ram;

static void ramdump_parse_info(void)
{
#if !IS_MODULE(CONFIG_AMLOGIC_MEMORY_EXTEND)
	pr_info("%s, .text : 0x%px - 0x%px, pa(.text): 0x%lx\n",
			__func__, (unsigned long *)_text,
			(unsigned long *)_etext, (unsigned long)__pa_symbol(_text));
#endif

#ifdef CONFIG_ARM64
	pr_info("%s, kimage_vaddr: 0x%px, v2p: 0x%lx\n",
			__func__, (unsigned long *)kimage_vaddr,
			(unsigned long)__pa_symbol(kimage_vaddr));
	pr_info("%s, KIMAGE_VADDR: 0x%px, v2p: 0x%lx\n",
			__func__, (unsigned long *)KIMAGE_VADDR,
			(unsigned long)__pa_symbol(KIMAGE_VADDR));
	pr_info("%s, kimage_voffset = 0x%lx\n", __func__,
			(unsigned long)kimage_vaddr - (unsigned long)__pa_symbol(kimage_vaddr));
	pr_info("%s, kaslr_offset   = 0x%lx\n", __func__, kaslr_offset());
	pr_info("%s, vabits_actual  = %d\n", __func__, (unsigned int)vabits_actual);
#endif
}

static int early_ramdump_para(char *buf)
{
	int ret;
#ifdef	RESERVE_MEM_BY_RAMDUMP_DTS_NODE
	struct device_node *node;
	struct resource res;
#endif

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
		ramdump_disable = 0;
		pr_info("%s, base:%lx, size:%lx\n",
			__func__, ramdump_base, ramdump_size);
#ifdef	RESERVE_MEM_BY_RAMDUMP_DTS_NODE
		node = of_find_node_by_path("/reserved-memory/ramdump_bl33z");
		ret = of_address_to_resource(node, 0, &res);
		pr_info("%s, dts reserved-memory %lx - %lx\n",
					__func__, (unsigned long)res.start, (unsigned long)res.end);
#else
		ret = memblock_reserve(ramdump_base, PAGE_ALIGN(ramdump_size));
		if (ret < 0) {
			pr_info("%s, reserve memblock %lx - %lx failed\n",
				__func__, ramdump_base,
				ramdump_base + PAGE_ALIGN(ramdump_size));
			ramdump_disable = 1;
		} else {
			pr_info("%s, reserve memblock %lx - %lx OK\n",
				__func__, ramdump_base,
				ramdump_base + PAGE_ALIGN(ramdump_size));
		}
#endif
		ramdump_parse_info();
	}
	return 0;
}

early_param("ramdump", early_ramdump_para);

#ifdef SAVE_DATA_BY_INIT_RC_SHELL
static ssize_t ramdump_bin_read(struct file *filp, struct kobject *kobj,
				struct bin_attribute *attr,
				char *buf, loff_t off, size_t count)
{
	void *p = NULL;

	if (!ram->mem_base || off >= ram->mem_size) {
		pr_info("%s, crash sysfsnode data err.\n", __func__);
		return 0;
	}

	if (off + count > ram->mem_size)
		count = ram->mem_size - off;

	p = (void *)phys_to_virt(ram->mem_base + off);

	mutex_lock(&ram->lock);
	memcpy(buf, p, count);
	mutex_unlock(&ram->lock);

	/* debug when read end */
	if (off + count >= ram->mem_size)
		pr_info("%s, p=%p %p, off:%lli, c:%zi\n",
			__func__, buf, p, off, count);

	return count;
}

int ramdump_disabled(void)
{
	if (ram)
		return ram->disable;
	return 0;
}
EXPORT_SYMBOL(ramdump_disabled);

static void meson_set_reboot_reason(int reboot_reason)
{
	struct arm_smccc_res smccc;

	arm_smccc_smc(SET_REBOOT_REASON,
		      reboot_reason, 0, 0, 0, 0, 0, 0, &smccc);
}

static ssize_t ramdump_bin_write(struct file *filp,
				 struct kobject *kobj,
				 struct bin_attribute *bin_attr,
				 char *buf, loff_t off, size_t count)
{
	if (ram->mem_base && !strncmp("reboot", buf, 6))
		kernel_restart("RAM-DUMP finished\n");

	if (!strncmp("disable", buf, 7)) {
		ram->disable = 1;
		meson_set_reboot_reason(MESON_NORMAL_BOOT);
	}
	if (!strncmp("enable", buf, 6)) {
		ram->disable = 0;
		meson_set_reboot_reason(MESON_KERNEL_PANIC);
	}

	return count;
}

static struct bin_attribute ramdump_attr = {
	.attr = {
		.name = "compmsg",
		.mode = 0664,
	},
	.read  = ramdump_bin_read,
	.write = ramdump_bin_write,
};

#ifdef CONFIG_ARM64
noinline void ramdump_sync_data(void)
{
	/*
	 * back port from old kernel version for function
	 * flush_cache_all(), we need it for ram dump
	 */
	asm volatile
		("mov	x12, x30\n"
		"dsb	sy\n"
		"mrs	x0, clidr_el1\n"
		"and	x3, x0, #0x7000000\n"
		"lsr	x3, x3, #23\n"
		"cbz	x3, finished\n"
		"mov	x10, #0\n"
	"loop1:\n"
		"add	x2, x10, x10, lsr #1\n"
		"lsr	x1, x0, x2\n"
		"and	x1, x1, #7\n"
		"cmp	x1, #2\n"
		"b.lt	skip\n"
		"mrs	x9, daif\n"
		"msr    daifset, #2\n"
		"msr	csselr_el1, x10\n"
		"isb\n"
		"mrs	x1, ccsidr_el1\n"
		"msr    daif, x9\n"
		"and	x2, x1, #7\n"
		"add	x2, x2, #4\n"
		"mov	x4, #0x3ff\n"
		"and	x4, x4, x1, lsr #3\n"
		"clz	w5, w4\n"
		"mov	x7, #0x7fff\n"
		"and	x7, x7, x1, lsr #13\n"
	"loop2:\n"
		"mov	x9, x4\n"
	"loop3:\n"
		"lsl	x6, x9, x5\n"
		"orr	x11, x10, x6\n"
		"lsl	x6, x7, x2\n"
		"orr	x11, x11, x6\n"
		"dc	cisw, x11\n"
		"subs	x9, x9, #1\n"
		"b.ge	loop3\n"
		"subs	x7, x7, #1\n"
		"b.ge	loop2\n"
	"skip:\n"
		"add	x10, x10, #2\n"
		"cmp	x3, x10\n"
		"b.gt	loop1\n"
	"finished:\n"
		"mov	x10, #0\n"
		"msr	csselr_el1, x10\n"
		"dsb	sy\n"
		"isb\n"
		"mov	x0, #0\n"
		"ic	ialluis\n"
		"ret	x12\n");
}
#else
noinline void ramdump_sync_data(void)
{
	flush_cache_all();
}
#endif

#endif /* end SAVE_DATA_BY_INIT_RC_SHELL */

/*
 * clear memory to avoid large amount of memory not used.
 * for random data, it's hard to compress
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
	order = MAX_ORDER - 3;
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
	pr_info("ramdump, clear:%lx, free:%lx, tick:%ld us\n",
		clear, free, tick / 1000);
}

void do_flush_cpu_cache(void)
{
	int cpu = smp_processor_id();

	pr_info("ramdump: CPU-%d flush cache ...\n", cpu);
	ramdump_sync_data();
	pr_info("ramdump: CPU-%d flush cache finish.\n", cpu);
}

static int panic_notify(struct notifier_block *self,
			unsigned long cmd, void *ptr)
{
	do_flush_cpu_cache();
	return NOTIFY_DONE;
}

static struct notifier_block panic_notifier = {
	.notifier_call	= panic_notify,
};

static int __init register_flush_panic_cpu_cache(void)
{
	/* flush cache for panic cpu */
	atomic_notifier_chain_register(&panic_notifier_list, &panic_notifier);

	return 0;
}
device_initcall(register_flush_panic_cpu_cache);

static int __init ramdump_probe(struct platform_device *pdev)
{
	unsigned long total_mem;
	struct resource *res;
	unsigned int dump_set;
	void __iomem *base;
	void *vaddr = NULL;
	int ret = 0;

	total_mem = get_num_physpages() << PAGE_SHIFT;
	if (total_mem)
		pr_info("ramdump, Total Memory:[%ld MB]\n", total_mem / 1024 / 1024);

	ram = kzalloc(sizeof(*ram), GFP_KERNEL);
	if (!ram)
		return -ENOMEM;

	if (ramdump_disable)
		ram->disable = 1;

	ram->mem_base = 0;
	ram->mem_size = ramdump_size;
	if (!ramdump_base || !ramdump_size) {
		pr_info("NO valid ramdump args:%lx %lx\n",
			ramdump_base, ramdump_size);
	} else {
		pr_info("%s, memremap start, paddr area: 0x%08lx - 0x%08lx\n",
				__func__, ramdump_base, ramdump_base + PAGE_ALIGN(ramdump_size));
		//vaddr = ioremap_cache(ramdump_base, PAGE_ALIGN(ramdump_size));
		vaddr = memremap(ramdump_base, PAGE_ALIGN(ramdump_size), MEMREMAP_WB);
		if (vaddr)
			ram->mem_base = (unsigned long)vaddr;

		pr_info("%s, memremap end, vaddr_base:%lx, size:%lx\n",
			__func__, ram->mem_base, ram->mem_size);
	}

	if (!ram->disable) {
		if (!ram->mem_base) {	/* No compressed data */
			INIT_DELAYED_WORK(&ram->work, lazy_clear_work);
			schedule_delayed_work(&ram->work, msecs_to_jiffies(100));
		} else {		/* with compressed data */
#ifdef	SAVE_DATA_BY_INIT_RC_SHELL
			pr_info("%s, SAVE_DATA_BY_INIT_RC_SHELL\n", __func__);
			ram->kobj = kobject_create_and_add("mdump", kernel_kobj);
			if (!ram->kobj) {
				pr_err("%s, create sysfs /mdump failed\n", __func__);
				goto err;
			}

			ramdump_attr.size = ram->mem_size;
			pr_info("%s, creat /sys/kernel/mdump/compmsg\n", __func__);
			if (sysfs_create_bin_file(ram->kobj, &ramdump_attr)) {
				pr_err("%s, create sysfs compmsg failed\n", __func__);
				goto err1;
			}
#endif	/* end SAVE_DATA_BY_INIT_RC_SHELL */
		}

		/* if ramdump is disabled in env, no need to set sticky reg */
		res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						   "SYSCTRL_STICKY_REG6");
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
			pr_info("%s, set sticky(0x%08x) to 0x%x\n",
					__func__, (unsigned int)res->start, dump_set);
		}
	}
	return ret;
#ifdef	SAVE_DATA_BY_INIT_RC_SHELL
err1:
	kobject_put(ram->kobj);
#endif

err:
	kfree(ram);

	return -EINVAL;
}

static int ramdump_remove(struct platform_device *pdev)
{
#ifdef SAVE_DATA_BY_INIT_RC_SHELL
	sysfs_remove_bin_file(ram->kobj, &ramdump_attr);
	iounmap((void *)ram->mem_base);
	kobject_put(ram->kobj);
#endif
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

fs_initcall(ramdump_init);
module_exit(ramdump_uninit);
MODULE_DESCRIPTION("AMLOGIC ramdump driver");
MODULE_LICENSE("GPL");
