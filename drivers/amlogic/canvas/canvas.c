

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/major.h>
#include <linux/io.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/kernel.h>

#include <linux/amlogic/canvas/canvas.h>
#include <linux/amlogic/canvas/canvas_mgr.h>

#include <linux/of_address.h>
#include "canvas_reg.h"
#include "canvas_priv.h"



#define DRIVER_NAME "amlogic-canvas"
#define MODULE_NAME "amlogic-canvas"
#define DEVICE_NAME "amcanvas"
#define CLASS_NAME  "amcanvas-class"

#define pr_dbg(fmt, args...) printk(KERN_DEBUG "Canvas: " fmt, ## args)
#define pr_error(fmt, args...) printk(KERN_ERR "Canvas: " fmt, ## args)



#define canvas_io_read(addr) readl(addr)
#define canvas_io_write(addr, val) writel((val), addr);

struct canvas_device_info {
    const char *device_name;
    spinlock_t lock;
    struct resource res;
    unsigned char __iomem   *reg_base;
    canvas_t canvasPool[CANVAS_MAX_NUM];
    int max_canvas_num;
    struct platform_device *canvas_dev;
};
static struct canvas_device_info canvas_info;


#define CANVAS_VALID(n) ((n) >= 0 && (n) < canvas_pool_canvas_num())

void canvas_config(u32 index, ulong addr, u32 width,
                   u32 height, u32 wrap, u32 blkmode)
{
    ulong flags;
    ulong fiq_flag;
    struct canvas_device_info *info = &canvas_info;
    canvas_t *canvasP = &canvas_info.canvasPool[index];

    if (CANVAS_VALID(index)) {
        return;
    }
    if (!canvas_pool_canvas_alloced(index)) {
        printk(KERN_ERR "!!canvas_config without alloc,index=%d\n", index);
        dump_stack();
        return;
    }
    raw_local_save_flags(fiq_flag);
    local_fiq_disable();
    spin_lock_irqsave(&info->lock, flags);

    if (is_meson_m8b_cpu()) {
        canvas_io_write(info->reg_base + DC_CAV_LUT_DATAL_M8M2,
                        (((addr + 7) >> 3) & CANVAS_ADDR_LMASK) |
                        ((((width + 7) >> 3) & CANVAS_WIDTH_LMASK) << CANVAS_WIDTH_LBIT));

        canvas_io_write(info->reg_base + DC_CAV_LUT_DATAH_M8M2,
                        ((((width + 7) >> 3) >> CANVAS_WIDTH_LWID) << CANVAS_WIDTH_HBIT) |
                        ((height & CANVAS_HEIGHT_MASK) << CANVAS_HEIGHT_BIT)    |
                        ((wrap & CANVAS_XWRAP) ? CANVAS_XWRAP : 0)              |
                        ((wrap & CANVAS_YWRAP) ? CANVAS_YWRAP : 0)              |
                        ((blkmode & CANVAS_BLKMODE_MASK) << CANVAS_BLKMODE_BIT));

        canvas_io_write(info->reg_base + DC_CAV_LUT_ADDR_M8M2, CANVAS_LUT_WR_EN | index);

        canvas_io_read(info->reg_base + DC_CAV_LUT_DATAH_M8M2);
    } else {
        canvas_io_write(info->reg_base + DC_CAV_LUT_DATAL,
                        (((addr + 7) >> 3) & CANVAS_ADDR_LMASK) |
                        ((((width + 7) >> 3) & CANVAS_WIDTH_LMASK) << CANVAS_WIDTH_LBIT));

        canvas_io_write(info->reg_base + DC_CAV_LUT_DATAH,
                        ((((width + 7) >> 3) >> CANVAS_WIDTH_LWID) << CANVAS_WIDTH_HBIT) |
                        ((height & CANVAS_HEIGHT_MASK) << CANVAS_HEIGHT_BIT)    |
                        ((wrap & CANVAS_XWRAP) ? CANVAS_XWRAP : 0)              |
                        ((wrap & CANVAS_YWRAP) ? CANVAS_YWRAP : 0)              |
                        ((blkmode & CANVAS_BLKMODE_MASK) << CANVAS_BLKMODE_BIT));

        canvas_io_write(info->reg_base + DC_CAV_LUT_ADDR, CANVAS_LUT_WR_EN | index);

        canvas_io_read(info->reg_base + DC_CAV_LUT_DATAH);
    }
    canvasP->addr = addr;
    canvasP->width = width;
    canvasP->height = height;
    canvasP->wrap = wrap;
    canvasP->blkmode = blkmode;

    spin_unlock_irqrestore(&info->lock, flags);
    raw_local_irq_restore(fiq_flag);
}
EXPORT_SYMBOL(canvas_config);

void canvas_read(u32 index, canvas_t *p)
{
    struct canvas_device_info *info = &canvas_info;
    if (CANVAS_VALID(index)) {
        *p = info->canvasPool[index];
    }
}
EXPORT_SYMBOL(canvas_read);

void canvas_copy(u32 src, u32 dst)
{
    unsigned long addr;
    unsigned width, height, wrap, blkmode;
    struct canvas_device_info *info = &canvas_info;
    canvas_t *canvas_src = &info->canvasPool[src];
    canvas_t *canvas_dst = &info->canvasPool[dst];

    if (CANVAS_VALID(src) || CANVAS_VALID(dst)) {
        return;
    }

    if (!canvas_pool_canvas_alloced(src) || !canvas_pool_canvas_alloced(dst)) {
        printk(KERN_ERR "!!canvas_copy  without alloc,src=%d,dst=%d\n", src, dst);
        dump_stack();
        return;
    }
    addr = canvas_src->addr;
    width = canvas_src->width;
    height = canvas_src->height;
    wrap = canvas_src->wrap;
    blkmode = canvas_src->blkmode;
    if (is_meson_m8b_cpu()) {

        canvas_io_write(info->reg_base + DC_CAV_LUT_DATAL_M8M2,
                        (((addr + 7) >> 3) & CANVAS_ADDR_LMASK) |
                        ((((width + 7) >> 3) & CANVAS_WIDTH_LMASK) << CANVAS_WIDTH_LBIT));

        canvas_io_write(info->reg_base + DC_CAV_LUT_DATAH_M8M2,
                        ((((width + 7) >> 3) >> CANVAS_WIDTH_LWID) << CANVAS_WIDTH_HBIT) |
                        ((height & CANVAS_HEIGHT_MASK) << CANVAS_HEIGHT_BIT)    |
                        ((wrap & CANVAS_XWRAP) ? CANVAS_XWRAP : 0)              |
                        ((wrap & CANVAS_YWRAP) ? CANVAS_YWRAP : 0)              |
                        ((blkmode & CANVAS_BLKMODE_MASK) << CANVAS_BLKMODE_BIT));

        canvas_io_write(info->reg_base + DC_CAV_LUT_ADDR_M8M2, CANVAS_LUT_WR_EN | dst);

        /* read a cbus to make sure last write finish. */
        canvas_io_read(info->reg_base + DC_CAV_LUT_DATAH_M8M2);
    } else {

        canvas_io_write(info->reg_base + DC_CAV_LUT_DATAL,
                        (((addr + 7) >> 3) & CANVAS_ADDR_LMASK) |
                        ((((width + 7) >> 3) & CANVAS_WIDTH_LMASK) << CANVAS_WIDTH_LBIT));

        canvas_io_write(info->reg_base + DC_CAV_LUT_DATAH,
                        ((((width + 7) >> 3) >> CANVAS_WIDTH_LWID) << CANVAS_WIDTH_HBIT) |
                        ((height & CANVAS_HEIGHT_MASK) << CANVAS_HEIGHT_BIT)    |
                        ((wrap & CANVAS_XWRAP) ? CANVAS_XWRAP : 0)              |
                        ((wrap & CANVAS_YWRAP) ? CANVAS_YWRAP : 0)              |
                        ((blkmode & CANVAS_BLKMODE_MASK) << CANVAS_BLKMODE_BIT));

        canvas_io_write(info->reg_base + DC_CAV_LUT_ADDR, CANVAS_LUT_WR_EN | dst);

        /* read a cbus to make sure last write finish. */
        canvas_io_read(info->reg_base + DC_CAV_LUT_DATAH);
    }
    canvas_dst->addr = addr;
    canvas_dst->width = width;
    canvas_dst->height = height;
    canvas_dst->wrap = wrap;
    canvas_dst->blkmode = blkmode;

    return;
}
EXPORT_SYMBOL(canvas_copy);

void canvas_update_addr(u32 index, u32 addr)
{
    ulong flags;
    ulong fiq_flag;
    struct canvas_device_info *info = &canvas_info;
    canvas_t *canvas;


    if (CANVAS_VALID(index)) {
        return;
    }
    canvas = &info->canvasPool[index];
    if (!canvas_pool_canvas_alloced(index)) {
        printk(KERN_ERR "!!canvas_update_addr  without alloc,index=%d\n", index);
        dump_stack();
        return;
    }
    raw_local_save_flags(fiq_flag);
    local_fiq_disable();
    spin_lock_irqsave(&info->lock, flags);

    info->canvasPool[index].addr = addr;
    if (is_meson_m8b_cpu()) {

        canvas_io_write(info->reg_base + DC_CAV_LUT_DATAL_M8M2,
                        (((canvas->addr + 7) >> 3) & CANVAS_ADDR_LMASK) |
                        ((((canvas->width + 7) >> 3) & CANVAS_WIDTH_LMASK) << CANVAS_WIDTH_LBIT));

        canvas_io_write(info->reg_base + DC_CAV_LUT_DATAH_M8M2,
                        ((((canvas->width + 7) >> 3) >> CANVAS_WIDTH_LWID) << CANVAS_WIDTH_HBIT) |
                        ((canvas->height & CANVAS_HEIGHT_MASK) << CANVAS_HEIGHT_BIT)   |
                        ((canvas->wrap & CANVAS_XWRAP) ? CANVAS_XWRAP : 0)             |
                        ((canvas->wrap & CANVAS_YWRAP) ? CANVAS_YWRAP : 0)             |
                        ((canvas->blkmode & CANVAS_BLKMODE_MASK) << CANVAS_BLKMODE_BIT));

        canvas_io_write(info->reg_base + DC_CAV_LUT_ADDR_M8M2, CANVAS_LUT_WR_EN | index);

        /* read a cbus to make sure last write finish. */
        canvas_io_read(info->reg_base + DC_CAV_LUT_DATAH_M8M2);
    } else {

        canvas_io_write(info->reg_base + DC_CAV_LUT_DATAL,
                        (((canvas->addr + 7) >> 3) & CANVAS_ADDR_LMASK) |
                        ((((canvas->width + 7) >> 3) & CANVAS_WIDTH_LMASK) << CANVAS_WIDTH_LBIT));

        canvas_io_write(info->reg_base + DC_CAV_LUT_DATAH,
                        ((((canvas->width + 7) >> 3) >> CANVAS_WIDTH_LWID) << CANVAS_WIDTH_HBIT) |
                        ((canvas->height & CANVAS_HEIGHT_MASK) << CANVAS_HEIGHT_BIT)   |
                        ((canvas->wrap & CANVAS_XWRAP) ? CANVAS_XWRAP : 0)             |
                        ((canvas->wrap & CANVAS_YWRAP) ? CANVAS_YWRAP : 0)             |
                        ((canvas->blkmode & CANVAS_BLKMODE_MASK) << CANVAS_BLKMODE_BIT));

        canvas_io_write(info->reg_base + DC_CAV_LUT_ADDR, CANVAS_LUT_WR_EN | index);

        /* read a cbus to make sure last write finish. */
        canvas_io_read(info->reg_base + DC_CAV_LUT_DATAH);
    }
    spin_unlock_irqrestore(&info->lock, flags);
    raw_local_irq_restore(fiq_flag);

    return;
}
EXPORT_SYMBOL(canvas_update_addr);

unsigned int canvas_get_addr(u32 index)
{
    struct canvas_device_info *info = &canvas_info;
    return info->canvasPool[index].addr;
}
EXPORT_SYMBOL(canvas_get_addr);

/*********************************************************/
#define to_canvas(kobj) container_of(kobj, canvas_t, kobj)
static ssize_t addr_show(canvas_t *canvas, char *buf)
{
    return sprintf(buf, "0x%lx\n", canvas->addr);
}

static ssize_t width_show(canvas_t *canvas, char *buf)
{
    return sprintf(buf, "%d\n", canvas->width);
}

static ssize_t height_show(canvas_t *canvas, char *buf)
{
    return sprintf(buf, "%d\n", canvas->height);
}

struct canvas_sysfs_entry {
    struct attribute attr;
    ssize_t (*show)(canvas_t *, char *);
};
static struct canvas_sysfs_entry addr_attribute = __ATTR_RO(addr);
static struct canvas_sysfs_entry width_attribute = __ATTR_RO(width);
static struct canvas_sysfs_entry height_attribute = __ATTR_RO(height);

static void canvas_release(struct kobject *kobj)
{
}

static ssize_t canvas_type_show(struct kobject *kobj, struct attribute *attr,
                                char *buf)
{
    canvas_t *canvas = to_canvas(kobj);
    struct canvas_sysfs_entry *entry;

    entry = container_of(attr, struct canvas_sysfs_entry, attr);

    if (!entry->show) {
        return -EIO;
    }

    return entry->show(canvas, buf);
}

static struct sysfs_ops canvas_sysfs_ops = {
    .show = canvas_type_show,
};

static struct attribute *canvas_attrs[] = {
    &addr_attribute.attr,
    &width_attribute.attr,
    &height_attribute.attr,
    NULL,
};

static struct kobj_type canvas_attr_type = {
    .release        = canvas_release,
    .sysfs_ops      = &canvas_sysfs_ops,
    .default_attrs  = canvas_attrs,
};



/* static int __devinit canvas_probe(struct platform_device *pdev) */
static int canvas_probe(struct platform_device *pdev)
{


    int i, r;
    struct canvas_device_info *info = &canvas_info;
    struct resource *res;
    int size;
    r = 0;

    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    if (!res) {
        dev_err(&pdev->dev, "cannot obtain I/O memory region");
        return -ENODEV;
    }
    info->res = *res;
    size = resource_size(res);
    printk("canvas_probe reg=%x,size=%x\n", res->start, resource_size(res));
    if (!devm_request_mem_region(&pdev->dev, res->start, size,
                                 pdev->name)) {
        dev_err(&pdev->dev, "Memory region busy\n");
        return -EBUSY;
    }
    info->reg_base = devm_ioremap_nocache(&pdev->dev,
                                          res->start,
                                          size);
    if (info->reg_base == NULL) {
        dev_err(&pdev->dev, "devm_ioremap_nocache for canvas failed!\n");
        goto err1;
    }
    printk("canvas maped reg_base =%x\n", (int)info->reg_base);
    amcanvas_manager_init();
    memset(info->canvasPool, 0, CANVAS_MAX_NUM * sizeof(canvas_t));
    info->max_canvas_num = canvas_pool_canvas_num();
    spin_lock_init(&info->lock);
    for (i = 0; i < info->max_canvas_num; i++) {
        r = kobject_init_and_add(&info->canvasPool[i].kobj, &canvas_attr_type,
                                 &pdev->dev.kobj, "%d", i);
        if (r) {
            pr_error("Unable to create canvas objects %d\n", i);
            goto err2;
        }
    }
    info->canvas_dev = pdev;
    if (1) {
        u8 buf[2560];
        int max = 256;
        int r;
        r = canvas_pool_get_static_canvas_by_name("amvdec", buf, max);
        printk("get canvas %s:num%d\n", "amvdec", r);
        if (r > 0) {
            int i;
            for (i = 0; i < r; i++) {
                /*printk("get canvas %s:%d\n", "amvdec", buf[i]);
                */
            }
        }

        r = canvas_pool_get_static_canvas_by_name("osd", buf, max);
        printk("get canvas %s:num%d\n", "osd", r);
    }
    return 0;

err2:
    for (i--; i >= 0; i--) {
        kobject_put(&info->canvasPool[i].kobj);
    }
    amcanvas_manager_exit();
    devm_iounmap(&pdev->dev, info->reg_base);
err1:
    devm_release_mem_region(&pdev->dev, res->start, size);
    pr_error("Canvas driver probe failed\n");
    return r;

    return 0;
}

/* static int __devexit canvas_remove(struct platform_device *pdev) */
static int canvas_remove(struct platform_device *pdev)
{
    int i;
    struct canvas_device_info *info = &canvas_info;
    for (i = 0; i < info->max_canvas_num; i++) {
        kobject_put(&info->canvasPool[i].kobj);
    }
    amcanvas_manager_exit();
    devm_iounmap(&pdev->dev, info->reg_base);
    devm_release_mem_region(&pdev->dev, info->res.start, resource_size(&info->res));
    pr_error("Canvas driver removed.\n");

    return 0;
}

static const struct of_device_id canvas_dt_match[] = {
    {
        .compatible     = "amlogic, meson, canvas",
    },
    {},
};

static struct platform_driver canvas_driver = {
    .probe  = canvas_probe,
    .remove = canvas_remove,
    .driver = {
        .name = "amlogic-canvas",
        .of_match_table = canvas_dt_match,
    },
};

static int __init amcanvas_init(void)
{
    int r;

    r = platform_driver_register(&canvas_driver);
    if (r) {
        pr_error("Unable to register canvas driver\n");
        return r;
    }
    printk("register canvas platform driver\n");

    return 0;
}

static void __exit amcanvas_exit(void)
{

    platform_driver_unregister(&canvas_driver);
}

postcore_initcall(amcanvas_init);
module_exit(amcanvas_exit);

MODULE_DESCRIPTION("AMLOGIC Canvas management driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tim Yao <timyao@amlogic.com>");
