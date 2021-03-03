// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/export.h>
#include <linux/errno.h>
#include <asm/sections.h>
#include <linux/sizes.h>
#include <asm/memory.h>
#include <linux/init.h>
#include <linux/initrd.h>
#include <linux/gfp.h>
#include <linux/memblock.h>
#include <linux/dma-mapping.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <linux/crash_dump.h>
#include <asm/boot.h>
#include <asm/fixmap.h>
#include <linux/kasan.h>
#include <linux/seq_file.h>

static void dump_mem_layout(char *buf)
{
#define MLK(b, t) b, t, ((t) - (b)) >> 10
#define MLM(b, t) b, t, ((t) - (b)) >> 20
#define MLG(b, t) b, t, ((t) - (b)) >> 30
#define MLK_ROUNDUP(b, t) b, t, DIV_ROUND_UP(((t) - (b)), SZ_1K)
	int pos = 0;

#ifdef CONFIG_ARM64
	pos += sprintf(buf + pos, "Virtual kernel memory layout:\n");
#ifdef CONFIG_KASAN
	pos += sprintf(buf + pos, "    kasan   : 0x%16lx - 0x%16lx   (%6ld GB)\n",
		MLG(KASAN_SHADOW_START, KASAN_SHADOW_END));
#endif
	pos += sprintf(buf + pos, "    modules : 0x%16lx - 0x%16lx   (%6ld MB)\n",
		MLM(MODULES_VADDR, MODULES_END));
	pos += sprintf(buf + pos, "    vmalloc : 0x%16lx - 0x%16lx   (%6ld GB)\n",
		MLG(VMALLOC_START, VMALLOC_END));
	pos += sprintf(buf + pos, "      .text : 0x%px" " - 0x%px" "   (%6ld KB) 0x%lx\n",
		MLK_ROUNDUP(_text, _etext), (unsigned long)__pa_symbol(_text));
	pos += sprintf(buf + pos, "    .rodata : 0x%px" " - 0x%px" "   (%6ld KB) 0x%lx\n",
		MLK_ROUNDUP(__start_rodata, __init_begin),
		(unsigned long)__pa_symbol(__start_rodata));
	pos += sprintf(buf + pos, "      .init : 0x%px" " - 0x%px" "   (%6ld KB) 0x%lx\n",
		MLK_ROUNDUP(__init_begin, __init_end),
		(unsigned long)__pa_symbol(__start_rodata));
	pos += sprintf(buf + pos, "      .data : 0x%px" " - 0x%px" "   (%6ld KB) 0x%lx\n",
		MLK_ROUNDUP(_sdata, _edata),
		(unsigned long)__pa_symbol(_sdata));
	pos += sprintf(buf + pos, "       .bss : 0x%px" " - 0x%px" "   (%6ld KB) 0x%lx\n",
		MLK_ROUNDUP(__bss_start, __bss_stop),
		(unsigned long)__pa_symbol(__bss_start));
	pos += sprintf(buf + pos, "    fixed   : 0x%16lx - 0x%16lx   (%6ld KB)\n",
		MLK(FIXADDR_START, FIXADDR_TOP));
	pos += sprintf(buf + pos, "    PCI I/O : 0x%16lx - 0x%16lx   (%6ld MB)\n",
		MLM(PCI_IO_START, PCI_IO_END));
#ifdef CONFIG_SPARSEMEM_VMEMMAP
	pos += sprintf(buf + pos, "    vmemmap : 0x%16lx - 0x%16lx   (%6ld GB maximum)\n",
		MLG(VMEMMAP_START, VMEMMAP_START + VMEMMAP_SIZE));
	pos += sprintf(buf + pos, "              0x%16lx - 0x%16lx   (%6ld MB actual)\n",
		MLM((unsigned long)phys_to_page(memblock_start_of_DRAM()),
		    (unsigned long)virt_to_page(high_memory)));
#endif
	pos += sprintf(buf + pos, "    memory  : 0x%16lx - 0x%16lx   (%6ld MB) 0x%lx\n",
		MLM(__phys_to_virt(memblock_start_of_DRAM()),
		    (unsigned long)high_memory), (unsigned long)memblock_start_of_DRAM());
#else

#endif
}

static int mdebug_show(struct seq_file *m, void *arg)
{
	char *buf = kmalloc(4096, GFP_KERNEL);

	if (!buf) {
		pr_err("%s failed\n", __func__);
		return -1;
	}

	/* update only once */
	dump_mem_layout(buf);
	seq_printf(m, "%s\n", buf);

	kfree(buf);

	return 0;
}

static int mdebug_open(struct inode *inode, struct file *file)
{
	return single_open(file, mdebug_show, NULL);
}

static const struct file_operations mdebug_ops = {
	.open		= mdebug_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release		= single_release,
};

static int __init memory_debug_init(void)
{
	struct proc_dir_entry *d_mdebug;

	d_mdebug = proc_create("mem_debug", 0444, NULL, &mdebug_ops);
	if (IS_ERR_OR_NULL(d_mdebug)) {
		pr_err("%s, create proc failed\n", __func__);
		return -1;
	}

	return 0;
}
rootfs_initcall(memory_debug_init);

