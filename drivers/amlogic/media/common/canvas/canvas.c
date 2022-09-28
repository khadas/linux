// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#define pr_fmt(fmt) "Canvas: " fmt

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/major.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/slab.h>
/* media module used media/registers/cpu_version.h since kernel 5.4 */
#include <linux/amlogic/media/registers/cpu_version.h>
#include <linux/amlogic/media/canvas/canvas.h>
#include <linux/amlogic/media/canvas/canvas_mgr.h>

#include <linux/of_address.h>
#include "canvas_reg.h"
#include "canvas_priv.h"

#define DRIVER_NAME "amlogic-canvas"
#define MODULE_NAME "amlogic-canvas"
#define DEVICE_NAME "amcanvas"
#define CLASS_NAME  "amcanvas-class"

#define pr_dbg(fmt, args...) pr_info("Canvas: " fmt, ## args)
#define pr_error(fmt, args...) pr_err("Canvas: " fmt, ## args)

#define canvas_io_read(addr) readl(addr)
#define canvas_io_write(addr, val) writel((val), addr)

struct canvas_device_info {
	const char *device_name;
	spinlock_t lock; /* spinlock for canvas */
	struct resource res;
	unsigned char __iomem *reg_base;
	struct canvas_s canvasPool[CANVAS_MAX_NUM];
	int max_canvas_num;
	struct platform_device *canvas_dev;
	ulong flags;
	ulong fiq_flag;
};

static struct canvas_device_info *canvas_info;

#define CANVAS_VALID(n) ((n) < canvas_pool_canvas_num())

static void
canvas_lut_data_build(ulong addr, u32 width, u32 height,
		      u32 wrap, u32 blkmode,
		      u32 endian, u32 *data_l, u32 *data_h)
{
	/*
	 *DMC_CAV_LUT_DATAL/DMC_CAV_LUT_DATAH
	 *high 32bits of canvas data which need to be configured
	 *to canvas memory.
	 *64bits CANVAS look up table
	 *bit 61:58   Endian control.
	 *bit 61:  1 : switch 2 64bits data inside 128bits boundary.
	 *0 : no change.
	 *bit 60:  1 : switch 2 32bits data inside 64bits data boundary.
	 *0 : no change.
	 *bit 59:  1 : switch 2 16bits data inside 32bits data boundary.
	 *0 : no change.
	 *bit 58:  1 : switch 2 8bits data  inside 16bits data bournday.
	 *0 : no change.
	 *bit 57:56.   Canvas block mode.  2 : 64x32, 1: 32x32;
	 *0 : linear mode.
	 *bit 55:      canvas Y direction wrap control.
	 *1: wrap back in y.  0: not wrap back.
	 *bit 54:      canvas X direction wrap control.
	 *1: wrap back in X.  0: not wrap back.
	 *bit 53:41.   canvas Hight.
	 *bit 40:29.   canvas Width, unit: 8bytes. must in 32bytes boundary.
	 *that means last 2 bits must be 0.
	 *bit 28:0.    canvas start address.   unit. 8 bytes. must be in
	 *32bytes boundary. that means last 2bits must be 0.
	 */

#define CANVAS_WADDR_LBIT       0
#define CANVAS_WIDTH_LBIT       29
#define CANVAS_WIDTH_HBIT       0
#define CANVAS_HEIGHT_HBIT      (41 - 32)
#define CANVAS_WRAPX_HBIT       (54 - 32)
#define CANVAS_WRAPY_HBIT       (55 - 32)
#define CANVAS_BLKMODE_HBIT     (56 - 32)
#define CANVAS_ENDIAN_HBIT      (58 - 32)

	u32 addr_bits_l = ((addr + 7) >> 3 & CANVAS_ADDR_LMASK)
		<< CANVAS_WADDR_LBIT;

	u32 width_l = ((((width + 7) >> 3) & CANVAS_WIDTH_LMASK)
		<< CANVAS_WIDTH_LBIT);

	u32 width_h = ((((width + 7) >> 3) >> CANVAS_WIDTH_LWID)
		<< CANVAS_WIDTH_HBIT);

	u32 height_h = (height & CANVAS_HEIGHT_MASK)
		<< CANVAS_HEIGHT_BIT;

	u32 wrap_h = (wrap & (CANVAS_ADDR_WRAPX | CANVAS_ADDR_WRAPY));

	u32 blkmod_h = (blkmode & CANVAS_BLKMODE_MASK)
		<< CANVAS_BLKMODE_HBIT;

	u32 switch_bits_ctl = (endian & 0xf)
		<< CANVAS_ENDIAN_HBIT;

	*data_l = addr_bits_l | width_l;
	*data_h = width_h | height_h | wrap_h | blkmod_h | switch_bits_ctl;
}

static void canvas_lut_data_parser(struct canvas_s *canvas,
				   u32 data_l, u32 data_h)
{
	ulong addr;
	u32 width;
	u32 height;
	u32 wrap;
	u32 blkmode;
	u32 endian;

	addr = (data_l & CANVAS_ADDR_LMASK) << 3;
	width = (data_l >> 29) & 0x7;
	width |= (data_h & 0x1ff) << 3;
	width = width << 3;

	height = (data_h >> (41 - 32)) & 0xfff;

	wrap = (data_h >> (54 - 32)) & 0x3;
	blkmode = (data_h >> (56 - 32)) & 0x3;
	endian = (data_h >> (58 - 32)) & 0xf;
	canvas->addr = addr;
	canvas->width = width;
	canvas->height = height;
	canvas->wrap = wrap;
	canvas->blkmode = blkmode;
	canvas->endian = endian;
}

static void canvas_config_locked(u32 index, struct canvas_s *p)
{
	struct canvas_device_info *info = canvas_info;
	u32 datal, datah;
	int reg_add = -DC_CAV_LUT_DATAL;

	if (!hw_canvas_support)
		return;
	canvas_lut_data_build(p->addr,
			      p->width,
			      p->height,
			      p->wrap,
			      p->blkmode,
			      p->endian, &datal, &datah);

	canvas_io_write(info->reg_base + reg_add + DC_CAV_LUT_DATAL, datal);

	canvas_io_write(info->reg_base + reg_add + DC_CAV_LUT_DATAH, datah);

	canvas_io_write(info->reg_base + reg_add + DC_CAV_LUT_ADDR,
			CANVAS_LUT_WR_EN | index);

	canvas_io_read(info->reg_base + reg_add + DC_CAV_LUT_DATAH);

	p->dataL = datal;
	p->dataH = datah;
}

int canvas_read_hw(u32 index, struct canvas_s *canvas)
{
	struct canvas_device_info *info = canvas_info;
	u32 datal, datah;
	int reg_add = -DC_CAV_LUT_DATAL;

	if (!hw_canvas_support)
		return 0;
	if (!CANVAS_VALID(index))
		return -1;
	datal = 0;
	datah = 0;
	canvas_io_write(info->reg_base + reg_add + DC_CAV_LUT_ADDR,
			CANVAS_LUT_RD_EN | (index & 0xff));
	datal = canvas_io_read(info->reg_base + reg_add + DC_CAV_LUT_RDATAL);
	datah = canvas_io_read(info->reg_base + reg_add + DC_CAV_LUT_RDATAH);
	canvas->dataL = datal;
	canvas->dataH = datah;
	canvas_lut_data_parser(canvas, datal, datah);
	return 0;
}
EXPORT_SYMBOL(canvas_read_hw);

#define canvas_lock(info, f) spin_lock_irqsave(&(info)->lock, f)

#define canvas_unlock(info, f) spin_unlock_irqrestore(&(info)->lock, f)

void canvas_config_ex(u32 index, ulong addr, u32 width,
		      u32 height, u32 wrap, u32 blkmode, u32 endian)
{
	struct canvas_device_info *info = canvas_info;
	struct canvas_s *canvas;
	unsigned long flags;

	if (!CANVAS_VALID(index))
		return;

	if (!canvas_pool_canvas_alloced(index)) {
		pr_info("Try config not allocked canvas[%d]\n", index);
		dump_stack();
		return;
	}
	canvas_lock(info, flags);
	canvas = &info->canvasPool[index];
	canvas->addr = addr;
	canvas->width = width;
	canvas->height = height;
	canvas->wrap = wrap;
	canvas->blkmode = blkmode;
	canvas->endian = endian;
	canvas_config_locked(index, canvas);
	canvas_unlock(info, flags);
}
EXPORT_SYMBOL(canvas_config_ex);

void canvas_config_config(u32 index, struct canvas_config_s *cfg)
{
	canvas_config_ex(index, cfg->phy_addr,
			 cfg->width, cfg->height,
			 CANVAS_ADDR_NOWRAP,
			 cfg->block_mode, cfg->endian);
}
EXPORT_SYMBOL(canvas_config_config);

void canvas_config(u32 index, ulong addr, u32 width,
		   u32 height, u32 wrap, u32 blkmode)
{
	return canvas_config_ex(index, addr, width, height, wrap, blkmode, 0);
}
EXPORT_SYMBOL(canvas_config);

void canvas_read(u32 index, struct canvas_s *p)
{
	struct canvas_device_info *info = canvas_info;

	if (CANVAS_VALID(index))
		*p = info->canvasPool[index];
}
EXPORT_SYMBOL(canvas_read);

void canvas_copy(u32 src, u32 dst)
{
	struct canvas_device_info *info = canvas_info;
	struct canvas_s *canvas_src = &info->canvasPool[src];
	struct canvas_s *canvas_dst = &info->canvasPool[dst];
	unsigned long flags;

	if (!CANVAS_VALID(src) || !CANVAS_VALID(dst))
		return;

	if (!canvas_pool_canvas_alloced(src) ||
	    !canvas_pool_canvas_alloced(dst)) {
		pr_info("!%s without alloc,src=%d,dst=%d\n",
			__func__, src, dst);
		dump_stack();
		return;
	}

	canvas_lock(info, flags);
	canvas_dst->addr = canvas_src->addr;
	canvas_dst->width = canvas_src->width;
	canvas_dst->height = canvas_src->height;
	canvas_dst->wrap = canvas_src->wrap;
	canvas_dst->blkmode = canvas_src->blkmode;
	canvas_dst->endian = canvas_src->endian;
	canvas_dst->dataH = canvas_src->dataH;
	canvas_dst->dataL = canvas_src->dataL;
	canvas_config_locked(dst, canvas_dst);
	canvas_unlock(info, flags);
}
EXPORT_SYMBOL(canvas_copy);

void canvas_update_addr(u32 index, ulong addr)
{
	struct canvas_device_info *info = canvas_info;
	struct canvas_s *canvas;
	unsigned long flags;

	if (!CANVAS_VALID(index))
		return;
	canvas = &info->canvasPool[index];
	if (!canvas_pool_canvas_alloced(index)) {
		pr_info("canvas_update_addrwithout alloc,index=%d\n",
			index);
		dump_stack();
		return;
	}
	canvas_lock(info, flags);
	canvas->addr = addr;
	canvas_config_locked(index, canvas);
	canvas_unlock(info, flags);
}
EXPORT_SYMBOL(canvas_update_addr);

ulong canvas_get_addr(u32 index)
{
	struct canvas_device_info *info = canvas_info;

	return info->canvasPool[index].addr;
}
EXPORT_SYMBOL(canvas_get_addr);

unsigned int canvas_get_width(u32 index)
{
	struct canvas_device_info *info = canvas_info;

	if (!CANVAS_VALID(index))
		return 0;

	return info->canvasPool[index].width;
}
EXPORT_SYMBOL(canvas_get_width);

unsigned int canvas_get_height(u32 index)
{
	struct canvas_device_info *info = canvas_info;

	if (!CANVAS_VALID(index))
		return 0;

	return info->canvasPool[index].height;
}
EXPORT_SYMBOL(canvas_get_height);
/*********************************************************/
static int __init canvas_probe(struct platform_device *pdev)
{
	int r = 0;
	struct canvas_device_info *info = NULL;
	struct resource *res;
	int size;

	info = devm_kzalloc(&pdev->dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "I/O memory region is not used\n");
		r = -ENOMEM;
		goto err1;
	} else {
		info->res = *res;
		size = (int)resource_size(res);
		info->reg_base = devm_ioremap_resource(&pdev->dev, res);
		if (!info->reg_base) {
			dev_err(&pdev->dev,
				"devm_ioremap_nocache canvas failed!\n");
			goto err1;
		}
	}
	amcanvas_manager_init();
	info->max_canvas_num = canvas_pool_canvas_num();
	spin_lock_init(&info->lock);
	info->canvas_dev = pdev;
	canvas_info = info;

	pr_info("%s ok, reg=%lx, size=%x base =%px\n", __func__,
		(unsigned long)res->start, size, info->reg_base);
	return 0;
err1:
	devm_kfree(&pdev->dev, info);
	pr_error("Canvas driver probe failed\n");
	return r;
}

/* static int __devexit canvas_remove(struct platform_device *pdev) */
static int canvas_remove(struct platform_device *pdev)
{
	int i;
	struct canvas_device_info *info = canvas_info;

	for (i = 0; i < info->max_canvas_num; i++)
		kobject_put(&info->canvasPool[i].kobj);
	amcanvas_manager_exit();
	devm_iounmap(&pdev->dev, info->reg_base);
	devm_release_mem_region(&pdev->dev,
				info->res.start,
				resource_size(&info->res));
	kfree(info);
	info = NULL;
	pr_error("Canvas driver removed.\n");

	return 0;
}

static const struct of_device_id canvas_dt_match[] = {
	{
			.compatible = "amlogic, meson, canvas",
	},
	{},
};

static struct platform_driver canvas_driver = {
	.remove = canvas_remove,
	.driver = {
			.name = "amlogic-canvas",
			.of_match_table = canvas_dt_match,
	},
};

int __init amcanvas_init(void)
{
	int r;

	r = platform_driver_probe(&canvas_driver, canvas_probe);
	if (r) {
		pr_error("Unable to register canvas driver\n");
		return r;
	}
	return 0;
}

void __exit amcanvas_exit(void)
{
	platform_driver_unregister(&canvas_driver);
}

//MODULE_DESCRIPTION("AMLOGIC Canvas management driver");
//MODULE_LICENSE("GPL");
//MODULE_AUTHOR("Tim Yao <timyao@amlogic.com>");
