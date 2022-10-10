// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/version.h>
#include <linux/string.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/major.h>
#include <linux/sched.h>
#include <linux/vmalloc.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/amlogic/media/vout/lcd/aml_ldim.h>
#include "../../lcd_common.h"
#include "ldim_drv.h"

void ldc_mem_dump(unsigned char *vaddr, unsigned int size)
{
	char *buf;
	unsigned char *p;
	unsigned int i, j, k, n;

	if (!vaddr) {
		LDIMERR("%s: vaddr is null\n", __func__);
		return;
	}

	buf = kcalloc(128, sizeof(char), GFP_KERNEL);
	if (!buf)
		return;

	p = vaddr;
	for (i = 0; i < size; i += 16) {
		n = 0;
		for (j = 0; j < 16; j++) {
			k = i * 16 + j;
			n += sprintf(buf + n, " %02x", p[k]);
		}
		pr_info("%08x:%s\n", i, buf);
	}
	kfree(buf);
}

void ldc_mem_save(char *path, unsigned long mem_paddr, unsigned int mem_size)
{
	struct file *filp = NULL;
	loff_t pos = 0;
	mm_segment_t old_fs = get_fs();
	unsigned int span = 0, remain = 0, count = 0;
	unsigned long phys;
	void *vaddr = NULL;
	unsigned int highmem_flag = 0;
	int i;

	pos = 0;

	set_fs(KERNEL_DS);
	filp = filp_open(path, O_RDWR | O_CREAT, 0666);
	if (IS_ERR(filp)) {
		pr_info("%s: create %s error\n", __func__, path);
		set_fs(old_fs);
		return;
	}

	highmem_flag = PageHighMem(phys_to_page(mem_paddr));
	LDIMPR("%s: highmem_flag: %d\n", __func__, highmem_flag);
	if (highmem_flag == 0) {
		vaddr = phys_to_virt(mem_paddr);
		if (!vaddr)
			goto lcd_ldc_axi_rmem_save_end;
		LDIMPR("%s: directly write: paddr=0x%lx, vaddr=0x%px, size: 0x%x\n",
		       __func__, mem_paddr, vaddr, mem_size);
		vfs_write(filp, vaddr, mem_size, &pos);
	} else {
		span = SZ_1M;
		count = mem_size / PAGE_ALIGN(span);
		remain = mem_size % PAGE_ALIGN(span);

		for (i = 0; i < count; i++) {
			phys = mem_paddr + i * span;
			vaddr = lcd_vmap(phys, span);
			if (!vaddr)
				goto lcd_ldc_axi_rmem_save_end;
			LDIMPR("%s: write: paddr=0x%lx, vaddr=0x%px, size: 0x%x\n",
			       __func__, phys, vaddr, span);
			vfs_write(filp, vaddr, span, &pos);
			lcd_unmap_phyaddr(vaddr);
		}
		if (remain) {
			phys = mem_paddr + count * span;
			vaddr = lcd_vmap(phys, remain);
			if (!vaddr)
				goto lcd_ldc_axi_rmem_save_end;
			LDIMPR("%s: write: paddr=0x%lx, vaddr=0x%px, size: 0x%x\n",
			       __func__, phys, vaddr, remain);
			vfs_write(filp, vaddr, remain, &pos);
			lcd_unmap_phyaddr(vaddr);
		}
	}

	vfs_fsync(filp, 0);
	filp_close(filp, NULL);
	set_fs(old_fs);
	LDIMPR("save buf to %s finished\n", path);
	return;

lcd_ldc_axi_rmem_save_end:
	vfs_fsync(filp, 0);
	filp_close(filp, NULL);
	set_fs(old_fs);
	LDIMERR("buf mapping failed: 0x%lx\n", mem_paddr);
}

void ldc_mem_write(char *path, unsigned long mem_paddr, unsigned int mem_size)
{
	unsigned int size;
	struct file *filp = NULL;
	loff_t pos = 0;
	mm_segment_t old_fs = get_fs();
	unsigned int span = 0, remain = 0, count = 0;
	unsigned long phys;
	void *vaddr = NULL;
	unsigned int highmem_flag = 0;
	unsigned char *buf, *p;
	int i, ret;

	pos = 0;

	buf = vmalloc(mem_size);
	if (!buf)
		return;

	set_fs(KERNEL_DS);
	filp = filp_open(path, O_RDONLY, 0);
	if (IS_ERR_OR_NULL(filp)) {
		pr_info("read %s error or filp is NULL.\n", path);
		set_fs(old_fs);
		vfree(buf);
		return;
	}

	ret = vfs_read(filp, buf, mem_size, &pos);
	if (ret < 0) {
		pr_info("read %s error\n", path);
		filp_close(filp, NULL);
		set_fs(old_fs);
		vfree(buf);
		return;
	}
	size = ret;
	pr_info("%s: read %s size %u\n", __func__, path, size);
	vfs_fsync(filp, 0);

	filp_close(filp, NULL);
	set_fs(old_fs);

	highmem_flag = PageHighMem(phys_to_page(mem_paddr));
	if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL)
		LDIMPR("%s: highmem_flag: %d\n", __func__, highmem_flag);
	if (highmem_flag == 0) {
		vaddr = phys_to_virt(mem_paddr);
		if (!vaddr)
			goto lcd_ldc_axi_rmem_write_end;
		if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL)
			LDIMPR("%s: directly write: paddr=0x%lx, vaddr=0x%px, size: 0x%x\n",
		       __func__, mem_paddr, vaddr, size);
		memcpy(vaddr, buf, size);
	} else {
		span = SZ_1M;
		count = size / PAGE_ALIGN(span);
		remain = size % PAGE_ALIGN(span);

		for (i = 0; i < count; i++) {
			phys = mem_paddr + i * span;
			vaddr = lcd_vmap(phys, span);
			if (!vaddr)
				goto lcd_ldc_axi_rmem_write_end;
			if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL)
				LDIMPR("%s: write: paddr=0x%lx, vaddr=0x%px, size: 0x%x\n",
			       __func__, phys, vaddr, span);
			p = buf + i * span;
			memcpy(vaddr, p, span);
			lcd_unmap_phyaddr(vaddr);
		}
		if (remain) {
			phys = mem_paddr + count * span;
			vaddr = lcd_vmap(phys, remain);
			if (!vaddr)
				goto lcd_ldc_axi_rmem_write_end;
			if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL)
				LDIMPR("%s: write: paddr=0x%lx, vaddr=0x%px, size: 0x%x\n",
			       __func__, phys, vaddr, remain);
			p = buf + count * span;
			memcpy(vaddr, p, remain);
			lcd_unmap_phyaddr(vaddr);
		}
	}

	vfree(buf);
	LDIMPR("write %s to buf finished\n", path);
	return;

lcd_ldc_axi_rmem_write_end:
	vfree(buf);
	LDIMERR("buf mapping failed: 0x%lx\n", mem_paddr);
}

void ldc_mem_write_profile(unsigned char *buf, unsigned long mem_paddr, unsigned int size)
{
	unsigned int span = 0, remain = 0, count = 0;
	unsigned long phys;
	void *vaddr = NULL;
	unsigned int highmem_flag = 0;
	unsigned char *p;
	int i;

	if (!buf)
		return;

	highmem_flag = PageHighMem(phys_to_page(mem_paddr));
	if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL)
		LDIMPR("%s: highmem_flag: %d\n", __func__, highmem_flag);
	if (highmem_flag == 0) {
		vaddr = phys_to_virt(mem_paddr);
		if (!vaddr)
			goto lcd_ldc_axi_rmem_write_end1;
		if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL)
			LDIMPR("%s: directly write: paddr=0x%lx, vaddr=0x%px, size: 0x%x\n",
		       __func__, mem_paddr, vaddr, size);
		memcpy(vaddr, buf, size);
	} else {
		span = SZ_1M;
		count = size / PAGE_ALIGN(span);
		remain = size % PAGE_ALIGN(span);

		for (i = 0; i < count; i++) {
			phys = mem_paddr + i * span;
			vaddr = lcd_vmap(phys, span);
			if (!vaddr)
				goto lcd_ldc_axi_rmem_write_end1;
			if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL)
				LDIMPR("%s: write: paddr=0x%lx, vaddr=0x%px, size: 0x%x\n",
			       __func__, phys, vaddr, span);
			p = buf + i * span;
			memcpy(vaddr, p, span);
			lcd_unmap_phyaddr(vaddr);
		}
		if (remain) {
			phys = mem_paddr + count * span;
			vaddr = lcd_vmap(phys, remain);
			if (!vaddr)
				goto lcd_ldc_axi_rmem_write_end1;
			if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL)
				LDIMPR("%s: write: paddr=0x%lx, vaddr=0x%px, size: 0x%x\n",
			       __func__, phys, vaddr, remain);
			p = buf + count * span;
			memcpy(vaddr, p, remain);
			lcd_unmap_phyaddr(vaddr);
		}
	}

	LDIMPR("write buf finished\n");
	return;

lcd_ldc_axi_rmem_write_end1:
	LDIMERR("%s buf mapping failed: 0x%lx\n", __func__, mem_paddr);
}
EXPORT_SYMBOL(ldc_mem_write_profile);


void ldc_mem_clear(unsigned long mem_paddr, unsigned int mem_size)
{
	unsigned int span = 0, remain = 0, count = 0;
	unsigned long phys;
	void *vaddr = NULL;
	unsigned int highmem_flag = 0;
	int i;

	highmem_flag = PageHighMem(phys_to_page(mem_paddr));
	LDIMPR("%s: highmem_flag: %d\n", __func__, highmem_flag);
	if (highmem_flag == 0) {
		vaddr = phys_to_virt(mem_paddr);
		if (!vaddr)
			goto ldc_rmem_clear_end;
		LDIMPR("%s: directly clear: paddr=0x%lx, vaddr=0x%px, size: 0x%x\n",
		       __func__, mem_paddr, vaddr, mem_size);
		memset(vaddr, 0, mem_size);
	} else {
		span = SZ_1M;
		count = mem_size / PAGE_ALIGN(span);
		remain = mem_size % PAGE_ALIGN(span);

		for (i = 0; i < count; i++) {
			phys = mem_paddr + i * span;
			vaddr = lcd_vmap(phys, span);
			if (!vaddr)
				goto ldc_rmem_clear_end;
			LDIMPR("%s: clear: paddr=0x%lx, vaddr=0x%px, size: 0x%x\n",
			       __func__, phys, vaddr, span);
			memset(vaddr, 0, span);
			lcd_unmap_phyaddr(vaddr);
		}
		if (remain) {
			phys = mem_paddr + count * span;
			vaddr = lcd_vmap(phys, remain);
			if (!vaddr)
				goto ldc_rmem_clear_end;
			LDIMPR("%s: clear: paddr=0x%lx, vaddr=0x%px, size: 0x%x\n",
			       __func__, phys, vaddr, remain);
			memset(vaddr, 0, remain);
			lcd_unmap_phyaddr(vaddr);
		}
	}

	LDIMPR("%s: finished\n", __func__);
	return;

ldc_rmem_clear_end:
	LDIMERR("buf mapping failed: 0x%lx\n", mem_paddr);
}

void ldc_mem_set(unsigned long mem_paddr, unsigned int mem_size)
{
	unsigned int span = 0, remain = 0, count = 0;
	unsigned long phys;
	void *vaddr = NULL;
	unsigned int highmem_flag = 0;
	int i;

	highmem_flag = PageHighMem(phys_to_page(mem_paddr));
	LDIMPR("%s: highmem_flag: %d\n", __func__, highmem_flag);
	if (highmem_flag == 0) {
		vaddr = phys_to_virt(mem_paddr);
		if (!vaddr)
			goto ldc_rmem_set_end;
		LDIMPR("%s: set: paddr=0x%lx, vaddr=0x%px, size:0x%x\n",
		       __func__, mem_paddr, vaddr, mem_size);
		memset(vaddr, 0xff, mem_size);
	} else {
		span = SZ_1M;
		count = mem_size / PAGE_ALIGN(span);
		remain = mem_size % PAGE_ALIGN(span);

		for (i = 0; i < count; i++) {
			phys = mem_paddr + i * span;
			vaddr = lcd_vmap(phys, span);
			if (!vaddr)
				goto ldc_rmem_set_end;
			LDIMPR("%s: set: paddr=0x%lx, vaddr=0x%px, size:0x%x\n",
			       __func__, phys, vaddr, span);
			memset(vaddr, 0xff, span);
			lcd_unmap_phyaddr(vaddr);
		}
		if (remain) {
			phys = mem_paddr + count * span;
			vaddr = lcd_vmap(phys, remain);
			if (!vaddr)
				goto ldc_rmem_set_end;
			LDIMPR("%s: set: paddr=0x%lx, vaddr=0x%px, size:0x%x\n",
			       __func__, phys, vaddr, remain);
			memset(vaddr, 0xff, remain);
			lcd_unmap_phyaddr(vaddr);
		}
	}

	LDIMPR("%s: finished\n", __func__);
	return;

ldc_rmem_set_end:
	LDIMERR("buf mapping failed: 0x%lx\n", mem_paddr);
}
