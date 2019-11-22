/*
 * drivers/amlogic/media/osd/osd_virtual.c
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

/* Linux Headers */
#include <linux/version.h>
#include <linux/compat.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/ctype.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/fb.h>
#include <linux/spinlock.h>
#include <linux/fs.h>
#include <linux/sysfs.h>
#include <linux/file.h>
#include <linux/fdtable.h>
#include <linux/console.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_reserved_mem.h>
#include <linux/uaccess.h>
#include <linux/dma-mapping.h>
#include <ion/ion.h>
#include <meson_ion.h>
#include <linux/fs.h>
#include <linux/cma.h>
#include <linux/dma-contiguous.h>
#include <linux/delay.h>

/* Amlogic Headers */
#include <linux/amlogic/media/vout/vinfo.h>
#include <linux/amlogic/media/vout/vout_notify.h>

/* Local Headers */
#ifdef CONFIG_AMLOGIC_LCD_SPI
#include "../vout/spi/lcd_spi_api.h"
#endif
#include "osd_log.h"
#include "osd_fb.h"
#include "osd_virtual.h"

/* #define SPI_DEBUG */
/* #define SOFTWARE_VSYNC */
#define HW_VSYNC

#define DEFAULT_FPS   (HZ/25)
static u32 fb_memsize;
static __u32 var_screeninfo[5];
static bool b_reserved_mem;
static int ready_post;
static int start_post;
static struct fb_virtual_dev_s *fb_vir_dev;
static struct virt_fb_para_s virt_fb;
static struct fb_var_screeninfo fb_def_var = {

	.xres            = 240,
	.yres            = 320,
	.xres_virtual    = 240,
	.yres_virtual    = 640,
	.xoffset         = 0,
	.yoffset         = 0,
	.bits_per_pixel = 16,
	.grayscale       = 0,
	.red             = {0, 0, 0},
	.green           = {0, 0, 0},
	.blue            = {0, 0, 0},
	.transp          = {0, 0, 0},
	.nonstd          = 0,
	.activate        = FB_ACTIVATE_NOW,
	.height          = -1,
	.width           = -1,
	.accel_flags     = 0,
	.pixclock        = 10,
	.left_margin     = 0,
	.right_margin    = 0,
	.upper_margin    = 0,
	.lower_margin    = 0,
	.hsync_len       = 0,
	.vsync_len       = 0,
	.sync            = 0,
	.vmode           = FB_VMODE_NONINTERLACED,
	.rotate          = 0,
};

static struct fb_fix_screeninfo fb_def_fix = {
	.id         = "VIRTUAL FB",
	.xpanstep   = 1,
	.ypanstep   = 1,
	.type       = FB_TYPE_PACKED_PIXELS,
	.visual     = FB_VISUAL_TRUECOLOR,
	.accel      = FB_ACCEL_NONE,
};

#ifdef SPI_DEBUG
static int spi_write_min;
module_param(spi_write_min, int, 0664);
MODULE_PARM_DESC(spi_write_min, "spi_write_min");

static int spi_write_max;
module_param(spi_write_max, int, 0664);
MODULE_PARM_DESC(spi_write_max, "spi_write_max");
#endif

static void lcd_init(void)
{
	/* set gamma */
}

static int lcd_set_format(int color_format)
{
	/* COLOR_INDEX_16_565 */
	return 0;

}

static void lcd_enable(int blank)
{

}

static void fb_get_fps(u32 index, u32 *osd_fps)
{
	*osd_fps = virt_fb.osd_fps;
}

static void fb_set_fps(u32 index, u32 osd_fps_start)
{
	static int stime, etime;
	int osd_fps;

	virt_fb.osd_fps_start = osd_fps_start;
	if (osd_fps_start) {
		/* start to calc fps */
		stime = ktime_to_us(ktime_get());
		virt_fb.osd_fps = 0;
	} else {
		/* stop to calc fps */
		etime = ktime_to_us(ktime_get());
		osd_fps =
			(virt_fb.osd_fps * 1000000)
			/ (etime - stime);
		osd_log_info("osd fps:=%d\n", osd_fps);
	}
}

static ssize_t show_fb_fps(struct device *device,
				struct device_attribute *attr,
				char *buf)
{
	struct fb_info *fb_info = dev_get_drvdata(device);
	u32 fb_fps;

	fb_get_fps(fb_info->node, &fb_fps);
	return snprintf(buf, 40, "%d\n",
		fb_fps);
}

static ssize_t store_fb_fps(struct device *device,
			   struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct fb_info *fb_info = dev_get_drvdata(device);
	int res = 0;
	int ret = 0;

	ret = kstrtoint(buf, 0, &res);
	fb_set_fps(fb_info->node, res);

	return count;
}

static ssize_t show_log_level(struct device *device,
			  struct device_attribute *attr,
			  char *buf)
{
	return snprintf(buf, 40, "%d\n", osd_log_level);
}

static ssize_t store_log_level(struct device *device,
			   struct device_attribute *attr,
			   const char *buf, size_t count)
{
	int res = 0;
	int ret = 0;

	ret = kstrtoint(buf, 0, &res);
	osd_log_info("log_level: %d->%d\n", osd_log_level, res);
	osd_log_level = res;

	return count;
}

static ssize_t show_log_module(struct device *device,
			  struct device_attribute *attr,
			  char *buf)
{
	return snprintf(buf, 40, "0x%x\n", osd_log_module);
}

static ssize_t store_log_module(struct device *device,
			   struct device_attribute *attr,
			   const char *buf, size_t count)
{
	int res = 0;
	int ret = 0;

	ret = kstrtoint(buf, 0, &res);
	osd_log_info("log_module: 0x%x->0x%x\n", osd_log_module, res);
	osd_log_module = res;

	return count;
}

static int virt_osd_check_var(struct fb_var_screeninfo *var,
	struct fb_info *info)
{
	struct fb_fix_screeninfo *fix;
	struct fb_virtual_dev_s *fbdev = (struct fb_virtual_dev_s *)info->par;
	const struct color_bit_define_s *color_format_pt;

	fix = &info->fix;
	color_format_pt = _find_color_format(var);
	if (color_format_pt == NULL || color_format_pt->color_index == 0)
		return -EFAULT;

	osd_log_dbg(MODULE_BASE, "select color format :index %d, bpp %d\n",
		    color_format_pt->color_index,
		    color_format_pt->bpp);
	fbdev->color = color_format_pt;
	var->red.offset = color_format_pt->red_offset;
	var->red.length = color_format_pt->red_length;
	var->red.msb_right = color_format_pt->red_msb_right;
	var->green.offset  = color_format_pt->green_offset;
	var->green.length  = color_format_pt->green_length;
	var->green.msb_right = color_format_pt->green_msb_right;
	var->blue.offset   = color_format_pt->blue_offset;
	var->blue.length   = color_format_pt->blue_length;
	var->blue.msb_right = color_format_pt->blue_msb_right;
	var->transp.offset = color_format_pt->transp_offset;
	var->transp.length = color_format_pt->transp_length;
	var->transp.msb_right = color_format_pt->transp_msb_right;
	var->bits_per_pixel = color_format_pt->bpp;
	osd_log_dbg(MODULE_BASE, "rgba(L/O):%d/%d-%d/%d-%d/%d-%d/%d\n",
		    var->red.length, var->red.offset,
		    var->green.length, var->green.offset,
		    var->blue.length, var->blue.offset,
		    var->transp.length, var->transp.offset);
	fix->visual = color_format_pt->color_type;
	/* adjust memory length. */
	fix->line_length = var->xres_virtual * var->bits_per_pixel / 8;
	osd_log_dbg(MODULE_BASE, "xvirtual=%d, bpp:%d, line_length=%d\n",
		var->xres_virtual, var->bits_per_pixel, fix->line_length);

	if (var->xres_virtual < var->xres)
		var->xres_virtual = var->xres;
	if (var->yres_virtual < var->yres)
		var->yres_virtual = var->yres;
	var->left_margin = var->right_margin = 0;
	var->upper_margin = var->lower_margin = 0;
	if (var->xres + var->xoffset > var->xres_virtual)
		var->xoffset = var->xres_virtual - var->xres;
	if (var->yres + var->yoffset > var->yres_virtual)
		var->yoffset = var->yres_virtual - var->yres;
	return 0;
}

static int virt_osd_set_par(struct fb_info *info)
{
	struct fb_virtual_dev_s *fbdev = (struct fb_virtual_dev_s *)info->par;
	const struct color_bit_define_s *color_format_pt;
	struct fb_var_screeninfo *var = NULL;
	u32 stride, fb_len;

	var = &info->var;
	if (!var)
		return -EFAULT;
	stride = var->xres * (var->bits_per_pixel >> 3);
	fb_len = stride * var->yres;
	osd_log_info("%s:stride=%d, fb_len=%d\n", __func__, stride, fb_len);
	virt_fb.screen_size = fb_len;
	virt_fb.offset = 0;
	virt_fb.stride = stride;
	color_format_pt = _find_color_format(&info->var);
	if (color_format_pt == NULL || color_format_pt->color_index == 0)
		return -EFAULT;
	fbdev->color = color_format_pt;
	lcd_set_format(fbdev->color->color_index);
	return 0;
}

static int virt_osd_check_fbsize(struct fb_var_screeninfo *var,
	struct fb_info *info)
{
	struct fb_virtual_dev_s *fbdev = (struct fb_virtual_dev_s *)info->par;

	if (var->xres_virtual * var->yres_virtual *
			var->bits_per_pixel / 8 > fbdev->fb_len) {
		osd_log_err("no enough memory for %d*%d*%d\n",
				var->xres,
				var->yres,
				var->bits_per_pixel);
		return  -ENOMEM;
	}
	return 0;
}

#ifdef SOFTWARE_VSYNC
s64 virt_osd_wait_vsync_event(void)
{
	int ret;
	unsigned long timeout;
	ktime_t stime;

	timeout = msecs_to_jiffies(2000);
	/* waiting for 1s. */
	ret = wait_for_completion_timeout(&fb_vir_dev->fb_com,
				timeout);
	if (ret == 0)
		pr_err("software vsync timeout\n");
	stime = ktime_get();
	osd_log_dbg(MODULE_BASE, "%s\n", __func__);

	return stime.tv64;
}

static void sw_vsync_timer_func(unsigned long arg)
{
	struct fb_virtual_dev_s *fbdev =
		(struct fb_virtual_dev_s *)arg;

	/* gen complete signal*/
	complete(&fbdev->timer_com);
}

static void lcd_post_frame(u32 addr, u32 size)
{
	unsigned char *fb_data;
#ifdef SPI_DEBUG
	int stime, etime, time_write;
	static int cnt;

	stime = ktime_to_us(ktime_get());
#endif

	start_post = 1;
	/* frame post*/
	fb_data = virt_fb.screen_base_vaddr + addr;

#ifdef CONFIG_AMLOGIC_LCD_SPI
	frame_post(fb_data, size);
#endif

#ifdef SPI_DEBUG
	etime = ktime_to_us(ktime_get());
	time_write = etime - stime;
	cnt++;
	if (cnt == 1) {
		spi_write_min = time_write;
		spi_write_max = time_write;
	}
	if (time_write < spi_write_min)
		spi_write_min = time_write;
	if (time_write > spi_write_max)
		spi_write_max = time_write;
#endif
	/* gen complete signal*/
	complete(&fb_vir_dev->post_com);
	/*start_post = 0; */
	osd_log_dbg(MODULE_BASE, "lcd_post_frame:=>addr 0x%x, size=%d\n",
		    addr, size);
}

static int fb_monitor_thread(void *data)
{
	int ret;
	struct fb_virtual_dev_s *fbdev = fb_vir_dev;

	osd_log_info("fb monitor start\n");
	while (fbdev->fb_monitor_run) {
		ret = wait_for_completion_timeout(&fb_vir_dev->timer_com,
					msecs_to_jiffies(2000));
		if (ret == 0)
			pr_err("timer post frame timeout\n");

		if (start_post) {
			/* waiting for 1s. */
			ret = wait_for_completion_timeout(&fb_vir_dev->post_com,
						msecs_to_jiffies(200));
			if (ret == 0)
				pr_err("post frame timeout\n");
		}
		/* gen complete signal*/
		complete(&fbdev->fb_com);

		/* Todo:  if frame_post started, wait frame_pos finished*/
		fbdev->timer.expires = jiffies + DEFAULT_FPS;
		add_timer(&fbdev->timer);
		start_post = 0;
	}
	osd_log_info("exit fb_monitor_thread\n");
	return 0;
}

int te_cb(void)
{
	return 0;
}
#endif

#ifdef HW_VSYNC
s64 virt_osd_wait_vsync_event(void)
{
	int ret;
	unsigned long timeout;
	ktime_t stime;

	timeout = msecs_to_jiffies(2000);
	/* waiting for 1s. */
	ret = wait_for_completion_timeout(&fb_vir_dev->fb_com,
					  timeout);
	if (ret == 0)
		pr_err("software vsync timeout\n");
	stime = ktime_get();
	osd_log_dbg(MODULE_BASE, "%s\n", __func__);

	return stime.tv64;
}

static void lcd_post_frame(u32 addr, u32 size)
{
	unsigned char *fb_data;
#ifdef SPI_DEBUG
	int stime, etime, time_write;
	static int cnt;

	stime = ktime_to_us(ktime_get());
#endif
	/* frame post*/
	fb_data = virt_fb.screen_base_vaddr + addr;

#ifdef CONFIG_AMLOGIC_LCD_SPI
	start_post = 1;
	frame_post(fb_data, size);
	start_post = 0;
#endif

#ifdef SPI_DEBUG
	etime = ktime_to_us(ktime_get());
	time_write = etime - stime;
	cnt++;
	if (cnt == 1) {
		spi_write_min = time_write;
		spi_write_max = time_write;
	}
	if (time_write < spi_write_min)
		spi_write_min = time_write;
	if (time_write > spi_write_max)
		spi_write_max = time_write;
#endif
	osd_log_dbg(MODULE_BASE, "lcd_post_frame:=>addr 0x%x, size=%d\n",
		    addr, size);
}

static int fb_monitor_thread(void *data)
{
	struct fb_virtual_dev_s *fbdev = fb_vir_dev;

	osd_log_info("fb monitor start\n");
	ready_post = 0;
	while (fbdev->fb_monitor_run) {
		/* waiting for 1s. */
		wait_for_completion(&fb_vir_dev->post_com);
		/* call frame_post*/
		lcd_post_frame(virt_fb.offset, virt_fb.size);
		/* gen complete signal*/
		complete(&fbdev->fb_com);
	}
	osd_log_info("exit fb_monitor_thread\n");
	return 0;
}

int te_cb(void)
{
	if (ready_post && (start_post == 0)) {
		/* gen complete signal*/
		complete(&fb_vir_dev->post_com);
		ready_post = 0;
	}
	return 0;
}
#endif
EXPORT_SYMBOL(te_cb);

static int virt_fb_start_monitor(void)
{
	int ret = 0;
	struct fb_virtual_dev_s *fbdev = fb_vir_dev;

	osd_log_info("virt fb start monitor\n");
	fbdev->fb_monitor_run = 1;
	fbdev->fb_thread = kthread_run(fb_monitor_thread,
		&fbdev, "fb_monitor");
	if (IS_ERR(fbdev->fb_thread)) {
		ret = PTR_ERR(fbdev->fb_thread);
		osd_log_err("osd failed to start kthread (%d)\n", ret);
	}
	return ret;
}

static int virt_fb_stop_monitor(void)
{
	struct fb_virtual_dev_s *fbdev = fb_vir_dev;

	osd_log_info("stop virt fb monitor thread\n");
	if (fbdev->fb_thread) {
		fbdev->fb_monitor_run = 0;
		kthread_stop(fbdev->fb_thread);
		fbdev->fb_thread = NULL;
	}
	return  0;
}

static void *aml_mm_vmap(phys_addr_t phys, unsigned long size)
{
	u32 offset, npages;
	struct page **pages = NULL;
	pgprot_t pgprot = PAGE_KERNEL;
	void *vaddr;
	int i;

	offset = offset_in_page(phys);
	npages = DIV_ROUND_UP(size + offset, PAGE_SIZE);

	pages = vmalloc(sizeof(struct page *) * npages);
	if (!pages)
		return NULL;
	for (i = 0; i < npages; i++) {
		pages[i] = phys_to_page(phys);
		phys += PAGE_SIZE;
	}
	vaddr = vmap(pages, npages, VM_MAP, pgprot);
	if (!vaddr) {
		pr_err("vmaped fail, size: %d\n",
			npages << PAGE_SHIFT);
		vfree(pages);
		return NULL;
	}
	vfree(pages);
	osd_log_info("[HIGH-MEM-MAP] pa(%lx) to va(%p), size: %d\n",
		(unsigned long)phys, vaddr, npages << PAGE_SHIFT);
	return vaddr;
}

static void *aml_map_phyaddr_to_virt(dma_addr_t phys, unsigned long size)
{
	void *vaddr = NULL;

	if (!PageHighMem(phys_to_page(phys)))
		return phys_to_virt(phys);
	vaddr = aml_mm_vmap(phys, size);
	return vaddr;
}

static int malloc_fb_memory(struct fb_info *info)
{
	int ret = 0;
	u32 fb_index, stride, fb_len;
	struct fb_virtual_dev_s *fbdev;
	struct fb_fix_screeninfo *fix = NULL;
	struct fb_var_screeninfo *var = NULL;
	struct platform_device *pdev = NULL;
	phys_addr_t base = 0;
	unsigned long size = 0;
	unsigned long fb_memsize_total  = 0;
	struct cma *cma = NULL;

	static void __iomem *fb_rmem_vaddr;
	static phys_addr_t fb_rmem_paddr;
	static struct page *fb_page;
	static struct ion_client *fb_ion_client;
	static struct ion_handle *fb_ion_handle;

	cma = dev_get_cma_area(info->device);
	if (cma) {
		base = cma_get_base(cma);
		size = cma_get_size(cma);
		pr_info("%s, cma:%p\n", __func__, cma);
	}

	osd_log_info("%s, %d, base:%pa, size:%ld\n",
		__func__, __LINE__, &base, size);
	fbdev = (struct fb_virtual_dev_s *)info->par;
	pdev = fbdev->dev;
	fb_index = fbdev->fb_index;
	fix = &info->fix;
	var = &info->var;
	fb_ion_client = meson_ion_client_create(-1, "meson-fb");
	fb_memsize_total = fb_memsize;
	/* read cma/fb-reserved memory first */
	if ((b_reserved_mem == true) &&
		(fb_memsize_total <= size) &&
		(fb_memsize > 0)) {
		pr_info("%s, %d, fb_index=%d,fb_rmem_size=%d\n",
			__func__, __LINE__, fb_index,
			fb_memsize);
		fb_page = dma_alloc_from_contiguous(
					info->device,
					fb_memsize >> PAGE_SHIFT,
					0);
		if (!fb_page) {
			pr_err("allocate buffer failed:%d\n", fb_memsize);
			return -ENOMEM;
		}
		fb_rmem_paddr = page_to_phys(fb_page);
		fb_rmem_vaddr =
			aml_map_phyaddr_to_virt(fb_rmem_paddr, fb_memsize);
		osd_log_dbg(MODULE_BASE, "fb_index=%d dma_alloc=0x%x\n",
			fb_index, fb_memsize);
	} else {
#ifdef CONFIG_AMLOGIC_ION
		pr_info("use ion buffer for fb memory, fb_index=%d\n",
			fb_index);
		fb_ion_handle =
			ion_alloc(fb_ion_client,
				fb_memsize,
				0,
				(1 << ION_HEAP_TYPE_DMA),
				0);
		if (IS_ERR(fb_ion_handle)) {
			osd_log_err("%s: size=%x, FAILED.\n",
				__func__, fb_memsize);
			return -ENOMEM;
		}
		ret = ion_phys(fb_ion_client,
			fb_ion_handle,
			(ion_phys_addr_t *)
			&fb_rmem_paddr,
			(size_t *)&fb_memsize_total);
		fb_rmem_vaddr =
			ion_map_kernel(fb_ion_client,
				fb_ion_handle);
		dev_notice(&pdev->dev,
			"create ion_client %p, handle=%p\n",
			fb_ion_client,
			fb_ion_handle);
		dev_notice(&pdev->dev,
			"ion memory(%d): created fb at 0x%p, size %ld MiB\n",
			fb_index,
			(void *)fb_rmem_paddr,
			(unsigned long)
			fb_memsize / SZ_1M);
#endif
	}
	fbdev->fb_len = fb_memsize;
	fbdev->fb_mem_paddr = fb_rmem_paddr;
	fbdev->fb_mem_vaddr = fb_rmem_vaddr;
	if (!fbdev->fb_mem_vaddr) {
		osd_log_err("failed to ioremap frame buffer\n");
		return -ENOMEM;
	}
	osd_log_info("Frame buffer memory assigned at");
	osd_log_info(" %d, phy: 0x%p, vir:0x%p, size=%dK\n\n",
		fb_index, (void *)fbdev->fb_mem_paddr,
		fbdev->fb_mem_vaddr, fbdev->fb_len >> 10);
	stride = var->xres * (var->bits_per_pixel >> 3);
	fb_len = stride * var->yres_virtual;
	fix->smem_start = fbdev->fb_mem_paddr;
	if (fb_len > fbdev->fb_len)
		fb_len = fbdev->fb_len;
	osd_log_info(" %d, stride=%d,var->yres_virtual=%d,fb_len=%d\n",
		fb_index, stride, var->yres_virtual, fb_len);
	fix->smem_len = fb_len;
	info->screen_base = (char __iomem *)fbdev->fb_mem_vaddr;
	info->screen_size = fb_len;
	virt_fb.screen_base_paddr = fbdev->fb_mem_paddr;
	virt_fb.screen_base_vaddr = fbdev->fb_mem_vaddr;
	virt_fb.screen_size = stride * var->yres;
	virt_fb.offset = 0;
	virt_fb.stride = stride;
	if (virt_osd_check_fbsize(var, info))
		return -ENOMEM;
	/* clear osd buffer */
	osd_log_info("---------------clear fb%d memory %p\n",
		fb_index, fbdev->fb_mem_vaddr);
	if (fbdev->fb_mem_vaddr)
		memset(fbdev->fb_mem_vaddr, 0x0, fb_len);
	return 0;
}

static int virt_osd_open(struct fb_info *info, int arg)
{
	struct fb_virtual_dev_s *fbdev;
	int ret = 0;

	fbdev = (struct fb_virtual_dev_s *)info->par;
	fbdev->open_count++;
	osd_log_dbg(MODULE_BASE, "osd_open index=%d,open_count=%d\n",
		fbdev->fb_index, fbdev->open_count);
	if (info->screen_base != NULL)
		return 0;

	if (info->screen_base == NULL) {
		ret = malloc_fb_memory(info);
		if (ret < 0)
			return ret;
	}
	return 0;
}

static int virt_osd_mmap(struct fb_info *info, struct vm_area_struct *vma)
{
	unsigned long mmio_pgoff;
	unsigned long start;
	u32 len;

	start = info->fix.smem_start;
	len = info->fix.smem_len;
	mmio_pgoff = PAGE_ALIGN((start & ~PAGE_MASK) + len) >> PAGE_SHIFT;
	if (vma->vm_pgoff >= mmio_pgoff) {
		if (info->var.accel_flags) {
			return -EINVAL;
		}

		vma->vm_pgoff -= mmio_pgoff;
		start = info->fix.mmio_start;
		len = info->fix.mmio_len;
	}

	vma->vm_page_prot = vm_get_page_prot(vma->vm_flags);
	vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);
	return vm_iomap_memory(vma, start, len);
}

int virt_osd_blank(int blank_mode, struct fb_info *info)
{
	osd_log_dbg(MODULE_BASE, "blank_mode=%d\n",
		blank_mode);
	lcd_enable((blank_mode != 0) ? 0 : 1);
	return 0;
}

static int virt_osd_pan_display(struct fb_var_screeninfo *var,
			   struct fb_info *fbi)
{
	long diff_x, diff_y;
	u32 offset = 0, size = 0, stride = 0;

	if (var->xoffset != virt_fb.pandata.x_start
	    || var->yoffset != virt_fb.pandata.y_start) {
		diff_x = var->xoffset - virt_fb.pandata.x_start;
		diff_y = var->yoffset - virt_fb.pandata.y_start;
		virt_fb.pandata.x_start += diff_x;
		virt_fb.pandata.x_end   += diff_x;
		virt_fb.pandata.y_start += diff_y;
		virt_fb.pandata.y_end   += diff_y;
		if (virt_fb.osd_fps_start)
			virt_fb.osd_fps++;
		stride = var->xres * (var->bits_per_pixel >> 3);
		offset = stride * virt_fb.pandata.y_start;
		virt_fb.offset = offset;
		size = stride * var->yres;
		virt_fb.size = size;
		ready_post = 1;
		osd_log_dbg(MODULE_BASE, "offset[%d-%d]x[%d-%d]y[%d-%d]\n",
				var->xoffset, var->yoffset,
				virt_fb.pandata.x_start,
				virt_fb.pandata.x_end,
				virt_fb.pandata.y_start,
				virt_fb.pandata.y_end);
#ifdef SOFTWARE_VSYNC
		lcd_post_frame(offset, size);
#endif
	}
	return 0;
}


static int virt_osd_release(struct fb_info *info, int arg)
{
	struct osd_fb_dev_s *fbdev;
	int err = 0;

	fbdev = (struct osd_fb_dev_s *)info->par;

	if (!fbdev->open_count) {
		err = -EINVAL;
		osd_log_info("osd already released. index=%d\n",
			fbdev->fb_index);
		goto done;
	} else if (fbdev->open_count == 1) {
	osd_log_info("osd_release now.index=%d,open_count=%d\n",
		fbdev->fb_index, fbdev->open_count);
	}
	fbdev->open_count--;
done:
	return err;
}

static int virt_osd_ioctl(struct fb_info *info,
	unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	u32 blank = 0;
	int ret = 0;
	s32 vsync_timestamp;

	switch (cmd) {
	case FBIO_WAITFORVSYNC:
		vsync_timestamp = (s32)virt_osd_wait_vsync_event();
		ret = copy_to_user(argp, &vsync_timestamp, sizeof(s32));
		break;
	case FBIOPUT_OSD_BLANK:
		ret = copy_from_user(&blank, argp, sizeof(u32));
		lcd_enable((blank != 0) ? 0 : 1);
		break;
	default:
		osd_log_err("command 0x%x not supported (%s)\n",
			cmd, current->comm);
		return -1;
	}
	return  ret;
}

#ifdef CONFIG_COMPAT
static int virt_osd_compat_ioctl(struct fb_info *info,
		unsigned int cmd, unsigned long arg)
{
	unsigned long ret;

	arg = (unsigned long)compat_ptr(arg);
	ret = virt_osd_ioctl(info, cmd, arg);
	return ret;
}
#endif

static void fbdev_virt_set_default(struct fb_virtual_dev_s *fbdev, int index)
{
	/* setup default value */
	fbdev->fb_info->var = fb_def_var;
	fbdev->fb_info->fix = fb_def_fix;
	fbdev->color = _find_color_format(&fbdev->fb_info->var);
}

static struct device_attribute virt_fb_attrs[] = {
	#if 0
	__ATTR(debug, 0644,
			show_debug, store_debug),
	#endif
	__ATTR(log_level, 0644,
			show_log_level, store_log_level),
	__ATTR(log_module, 0644,
			show_log_module, store_log_module),
	__ATTR(fps, 0644,
			show_fb_fps, store_fb_fps),
};

/* fb_ops structures */
static struct fb_ops virtual_fb_ops = {
	.owner          = THIS_MODULE,
	.fb_check_var   = virt_osd_check_var,
	.fb_set_par     = virt_osd_set_par,
	.fb_fillrect    = cfb_fillrect,
	.fb_copyarea    = cfb_copyarea,
	.fb_imageblit   = cfb_imageblit,
	.fb_ioctl       = virt_osd_ioctl,
#ifdef CONFIG_COMPAT
	.fb_compat_ioctl = virt_osd_compat_ioctl,
#endif
	.fb_open        = virt_osd_open,
	.fb_mmap        = virt_osd_mmap,
	.fb_blank       = virt_osd_blank,
	.fb_pan_display = virt_osd_pan_display,
	.fb_release     = virt_osd_release,
};

int amlfb_virtual_probe(struct platform_device *pdev)
{
	struct fb_info *fbi = NULL;
	struct fb_var_screeninfo *var;
	struct fb_fix_screeninfo *fix;
	int  index = 0, bpp;
	struct fb_virtual_dev_s *fbdev = NULL;
	int i;
	int ret = 0;

	/* get buffer size from dt */
	ret = of_property_read_u32(pdev->dev.of_node,
			"mem_size", &fb_memsize);
	if (ret) {
		osd_log_err("not found mem_size from dtd\n");
		goto failed1;
	}
	if (!fb_memsize)
		goto failed1;
	osd_log_info("fb_memsize=0x%x\n", fb_memsize);
	/* init reserved memory */
	ret = of_reserved_mem_device_init(&pdev->dev);
	if (ret != 0) {
		osd_log_err("failed to init reserved memory\n");
		b_reserved_mem = false;
	} else {
		b_reserved_mem = true;
	}
	/* register frame buffer memory */
	fbi = framebuffer_alloc(sizeof(struct fb_virtual_dev_s),
			&pdev->dev);
	if (!fbi) {
		ret = -ENOMEM;
		goto failed1;
	}
	fbdev = (struct fb_virtual_dev_s *)fbi->par;
	fbdev->fb_index = 0;
	fbdev->fb_info = fbi;
	fbdev->dev = pdev;
	mutex_init(&fbdev->lock);
	var = &fbi->var;
	fix = &fbi->fix;
	fb_vir_dev = fbdev;
	fbdev->fb_len = 0;
	fbdev->fb_mem_paddr = 0;
	fbdev->fb_mem_vaddr = 0;
	/* setup fb0 display size */
	ret = of_property_read_u32_array(pdev->dev.of_node,
				"display_size_default",
				&var_screeninfo[0], 5);
	if (ret)
		osd_log_info("not found display_size_default\n");
	else {
		fb_def_var.xres = var_screeninfo[0];
		fb_def_var.yres = var_screeninfo[1];
		fb_def_var.xres_virtual =
			var_screeninfo[2];
		fb_def_var.yres_virtual =
			var_screeninfo[3];
		fb_def_var.bits_per_pixel =
			var_screeninfo[4];
		osd_log_info("init fbdev bpp is:%d\n",
			fb_def_var.bits_per_pixel);
		if (fb_def_var.bits_per_pixel > 32)
			fb_def_var.bits_per_pixel = 32;
	}

	fbdev_virt_set_default(fbdev, index);
	if (fbdev->color == NULL) {
		osd_log_err("fbdev->color NULL\n");
		ret = -ENOENT;
		goto failed1;
	}
	bpp = (fbdev->color->color_index > 8 ?
			(fbdev->color->color_index > 16 ?
			(fbdev->color->color_index > 24 ?
			 4 : 3) : 2) : 1);
	fix->line_length = var->xres_virtual * bpp;
	fix->smem_start = fbdev->fb_mem_paddr;
	fix->smem_len = fbdev->fb_len;
	if (fb_alloc_cmap(&fbi->cmap, 16, 0) != 0) {
		osd_log_err("unable to allocate color map memory\n");
		ret = -ENOMEM;
		goto failed2;
	}
	fbi->pseudo_palette = kmalloc(sizeof(u32) * 16, GFP_KERNEL);
	if (!(fbi->pseudo_palette)) {
		osd_log_err("unable to allocate pseudo palette mem\n");
		ret = -ENOMEM;
		goto failed2;
	}
	memset(fbi->pseudo_palette, 0, sizeof(u32) * 16);
	fbi->fbops = &virtual_fb_ops;
	fbi->screen_base = (char __iomem *)fbdev->fb_mem_vaddr;
	fbi->screen_size = fix->smem_len;
	virt_osd_check_var(var, fbi);
	/* register frame buffer */
	register_framebuffer(fbi);
	/* create device attribute files */
	for (i = 0; i < ARRAY_SIZE(virt_fb_attrs); i++)
		ret = device_create_file(
			fbi->dev, &virt_fb_attrs[i]);

	init_completion(&fbdev->fb_com);
	init_completion(&fbdev->post_com);
#ifdef SOFTWARE_VSYNC
	init_completion(&fbdev->timer_com);
#endif
	lcd_init();
	virt_fb_start_monitor();
	#ifdef SOFTWARE_VSYNC
	/* add timer to simulate software vsync */
	init_timer(&fbdev->timer);
	fbdev->timer.data = (ulong) fbdev;
	fbdev->timer.function = sw_vsync_timer_func;
	fbdev->timer.expires = jiffies + DEFAULT_FPS;
	add_timer(&fbdev->timer);
	#endif
	osd_log_info("virtual osd probe OK\n");
	return 0;
failed2:
	fb_dealloc_cmap(&fbi->cmap);
failed1:
	osd_log_err("osd probe failed.\n");
	return ret;
}

void amlfb_virtual_remove(struct platform_device *pdev)
{
	struct fb_info *fbi;

	if (fb_vir_dev) {
		struct fb_virtual_dev_s *fbdev = fb_vir_dev;

		virt_fb_stop_monitor();
		fbi = fbdev->fb_info;
		iounmap(fbdev->fb_mem_vaddr);
		kfree(fbi->pseudo_palette);
		fb_dealloc_cmap(&fbi->cmap);
		unregister_framebuffer(fbi);
		framebuffer_release(fbi);
	}
}
