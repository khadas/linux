/*
*
* SPDX-License-Identifier: GPL-2.0
*
* Copyright (C) 2011-2018 ARM or its affiliates
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; version 2.
* This program is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
* or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
* for more details.
* You should have received a copy of the GNU General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*
*/

/*
 * ACamera PCI Express UIO driver
 *
 */
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/uio_driver.h>
#include <linux/interrupt.h>
#include <asm/io.h>
#include <asm/signal.h>
#include <linux/of_reserved_mem.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include "acamera_firmware_config.h"
#include "acamera_logger.h"
#include "system_hw_io.h"
#include "system_sw_io.h"
#include "system_am_sc.h"
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <asm/unaligned.h>
#include <linux/delay.h>
#include "v4l2_interface/isp-v4l2.h"


#define LOG_CONTEXT "[ ACamera ]"
#define ISP_V4L2_MODULE_NAME "isp-v4l2"

#define AO_RTI_GEN_PWR_SLEEP0 	(0xff800000 + 0x3a * 4)
#define AO_RTI_GEN_PWR_ISO0		(0xff800000 + 0x3b * 4)
#define HHI_ISP_MEM_PD_REG0		(0xff63c000 + 0x45 * 4)
#define HHI_ISP_MEM_PD_REG1		(0xff63c000 + 0x46 * 4)
#define HHI_CSI_PHY_CNTL0		(0xff630000 + 0xd3 * 4)
#define HHI_CSI_PHY_CNTL1		(0xff630000 + 0x114 * 4)


struct device_info {
    struct clk* clk_isp_0;
    struct clk* clk_mipi_0;
    struct device_node *am_sc;
};

extern uint8_t *isp_kaddr;
extern resource_size_t isp_paddr;

extern void system_interrupts_set_irq( int irq_num, int flags );
extern void system_interrupts_init(void);
extern void system_interrupts_deinit(void);
extern uintptr_t acamera_get_isp_sw_setting_base( void );

//map and unmap fpga memory
extern int32_t init_hw_io( resource_size_t addr, resource_size_t size );
extern int32_t close_hw_io( void );

static struct v4l2_device v4l2_dev;
static struct platform_device *isp_pdev;
static int initialized = 0;
static struct device_info dev_info;

#if V4L2_SOC_SUBDEV_ENABLE
extern uint32_t fw_calibration_update( void );
#include "soc_iq.h"

static int v4l2devs_running = 0;

struct acamera_v4l2_subdev_t {

    struct v4l2_subdev *soc_subdevs[V4L2_SOC_SUBDEV_NUMBER];
    struct v4l2_async_subdev soc_async_sd[V4L2_SOC_SUBDEV_NUMBER];
    struct v4l2_async_subdev *soc_async_sd_ptr[V4L2_SOC_SUBDEV_NUMBER];
    int subdev_counter;
    struct v4l2_async_notifier notifier;
};

static struct acamera_v4l2_subdev_t g_subdevs;


void *acamera_camera_v4l2_get_subdev_by_name( const char *name )
{
    int idx = 0;
    void *result = NULL;
    LOG( LOG_ERR, "Requested a pointer to the subdev with a name %s", name );
    for ( idx = 0; idx < V4L2_SOC_SUBDEV_NUMBER; idx++ ) {
        if ( g_subdevs.soc_subdevs[idx] && strcmp( g_subdevs.soc_subdevs[idx]->name, name ) == 0 ) {
            result = g_subdevs.soc_subdevs[idx];
            break;
        }
    }
    LOG( LOG_ERR, "Return subdev pointer 0x%x", result );
    return result;
}

int acamera_camera_v4l2_get_index_by_name( const char *name )
{
    int idx = 0;
    LOG( LOG_ERR, "Requested a index pointer with a name %s", name );
    for ( idx = 0; idx < V4L2_SOC_SUBDEV_NUMBER; idx++ ) {
        if ( g_subdevs.soc_subdevs[idx] && strcmp( g_subdevs.soc_subdevs[idx]->name, name ) == 0 ) {
            break;
        }
    }
    LOG( LOG_ERR, "Return index pointer 0x%x", idx );
    return idx;
}

static int acamera_camera_async_bound( struct v4l2_async_notifier *notifier,
                                       struct v4l2_subdev *sd,
                                       struct v4l2_async_subdev *asd )
{
    int rc = 0;
    LOG( LOG_ERR, "bound called with sd 0x%x, asd 0x%x, sd->dev 0x%x, name %s", sd, asd, sd->dev, sd->name );
    int idx = 0;
    for ( idx = 0; idx < V4L2_SOC_SUBDEV_NUMBER; idx++ ) {
        if ( g_subdevs.soc_subdevs[idx] == 0 ) {
            break;
        }
    }

    if ( idx < V4L2_SOC_SUBDEV_NUMBER ) {
        g_subdevs.soc_subdevs[idx] = sd;
        g_subdevs.subdev_counter++;

        if ( strcmp( g_subdevs.soc_subdevs[idx]->name, V4L2_SOC_IQ_NAME ) == 0 && v4l2devs_running == 1 ) { //update calibration
            fw_calibration_update();
        }

    } else {
        rc = -1;
        LOG( LOG_CRIT, "Inserted more subdevices than expected. Driver is configured to support %d subdevs only", V4L2_SOC_SUBDEV_NUMBER );
    }


    return rc;
}


static void acamera_camera_async_unbind( struct v4l2_async_notifier *notifier,
                                         struct v4l2_subdev *sd,
                                         struct v4l2_async_subdev *asd )
{
    LOG( LOG_ERR, "unbind called for subdevice sd 0x%x, asd 0x%x, sd->dev 0x%x, name %s", sd, asd, sd->dev, sd->name );

    int idx = acamera_camera_v4l2_get_index_by_name( sd->name );

    if ( strcmp( g_subdevs.soc_subdevs[idx]->name, V4L2_SOC_IQ_NAME ) != 0 ) { //any other subdevs need to stop firmware
        if ( v4l2devs_running == 1 ) {
            LOG( LOG_ERR, "stopping V4L2 firmware" );
            isp_v4l2_destroy_instance(isp_pdev);
            initialized = 0;
            v4l2devs_running = 0;
        }
    }

    if ( idx < V4L2_SOC_SUBDEV_NUMBER ) {
        g_subdevs.soc_subdevs[idx] = 0;
        g_subdevs.subdev_counter--;
    }
}


static int acamera_camera_async_complete( struct v4l2_async_notifier *notifier )
{
    int rc = 0;

    LOG( LOG_ERR, "complete called" );
    if ( v4l2devs_running == 0 ) {
        LOG( LOG_ERR, "starting V4L2 firmware" );
        rc = v4l2_device_register_subdev_nodes( &v4l2_dev );

        if ( rc == 0 ) {
            rc = isp_v4l2_create_instance( &v4l2_dev, isp_pdev);

            if ( rc == 0 ) {
                initialized = 1;
            } else {
                LOG( LOG_ERR, "Failed to register ISP v4l2 driver." );
                initialized = 0;
                rc = -1;
            }
        } else {
            rc = -1;
            LOG( LOG_CRIT, "Failed to create subdevice nodes under /dev/v4l-subdevX" );
        }
        v4l2devs_running = 1;
    }
    return rc;
}

#endif

void cache_flush(uint32_t buf_start, uint32_t buf_size)
{
    if ((buf_start == 0) || (buf_size == 0))
        return;
    if ( isp_pdev != NULL)
        dma_sync_single_for_cpu(&isp_pdev->dev, buf_start, buf_size, DMA_FROM_DEVICE);
    else {
        pr_err("%s: isp_pdev is null, cache_flush failed!\n", __func__);
        return;
    }
}

int  write_to_file (char *fname, char *buf, int size)
{
    int ret = 0;
    struct file *fp = NULL;
    mm_segment_t old_fs;
    loff_t pos = 0;
    int nwrite = 0;
    int offset = 0;
    int first_flag=0;

    old_fs = get_fs();
    set_fs(KERNEL_DS);

    pr_err("%s: buff addr %p, size 0x%x\n",__func__, buf, size);

    if (first_flag == 0) {
        first_flag = 1;
    /* open file to write */
        fp = filp_open(fname, O_RDWR | O_CREAT, 0666);
        if (!fp) {
            pr_err("%s: open file error\n", __func__);
            ret = -1;
            goto exit;
        }
    }

    pos = (unsigned long)offset;

	/* Write buf to file */
    nwrite=vfs_write(fp, buf, size, &pos);
    offset +=nwrite;

    if (fp) {
        filp_close(fp, NULL);
    }

exit:
    set_fs(old_fs);
    return 0;
}

static void parse_param(
    char *buf_orig, char **parm)
{
    char *ps, *token;
    unsigned int n = 0;
    char delim1[3] = " ";
    char delim2[2] = "\n";

    ps = buf_orig;
    strcat(delim1, delim2);
    while (1) {
        token = strsep(&ps, delim1);
        if (token == NULL)
            break;
        if (*token == '\0')
            continue;
        parm[n++] = token;
    }
}

static const char *isp_reg_usage_str = {
    "Usage:\n"
    "echo r addr(H) > /sys/devices/platform/ff140000.isp/reg;\n"
    "echo w addr(H) value(H) > /sys/devices/platform/ff140000.isp/reg;\n"
    "echo d addr(H) num(D) > /sys/devices/platform/ff140000.isp/reg; dump reg from addr\n"
};

static ssize_t reg_read(
    struct device *dev,
    struct device_attribute *attr,
    char *buf)
{
    return sprintf(buf, "%s\n", isp_reg_usage_str);
}

static ssize_t reg_write(
    struct device *dev, struct device_attribute *attr,
    char const *buf, size_t size)
{
    char *buf_orig, *parm[8] = {NULL};
    long val = 0;
    unsigned int reg_addr, reg_val, i;
    ssize_t ret = size;
    uintptr_t isp_sw_base = acamera_get_isp_sw_setting_base();

    if (!buf)
        return ret;

    buf_orig = kstrdup(buf, GFP_KERNEL);
    if (!buf_orig)
        return ret;

    parse_param(buf_orig, (char **)&parm);

    if (!parm[0]) {
        ret = -EINVAL;
        goto Err;
    }

    if (!strcmp(parm[0], "r")) {
        if (!parm[1] || (kstrtoul(parm[1], 16, &val) < 0)) {
            ret = -EINVAL;
            goto Err;
        }
        reg_addr = val;
        reg_val = system_hw_read_32(reg_addr);
        pr_info("ISP READ[0x%05x]=0x%08x\n", reg_addr, reg_val);
    } else if (!strcmp(parm[0], "w")) {
        if (!parm[1] || (kstrtoul(parm[1], 16, &val) < 0)) {
            ret = -EINVAL;
            goto Err;
        }
        reg_addr = val;
        if (!parm[2] || (kstrtoul(parm[2], 16, &val) < 0)) {
            ret = -EINVAL;
            goto Err;
        }
        reg_val = val;
        if (isp_sw_base)
            system_sw_write_32((isp_sw_base + reg_addr), reg_val);
        system_hw_write_32(reg_addr, reg_val);
        pr_info("ISP WRITE[0x%05x]=0x%08x\n", reg_addr, reg_val);
    } else if (!strcmp(parm[0], "d")) {
        if (!parm[1] || (kstrtoul(parm[1], 16, &val) < 0)) {
            ret = -EINVAL;
            goto Err;
        }
        reg_addr = val;
        if (!parm[2] || (kstrtoul(parm[2], 10, &val) < 0))
            val = 1;

        for (i = 0; i < val; i++) {
            reg_val = system_hw_read_32(reg_addr);
            pr_info("ISP DUMP[0x%05x]=0x%08x\n", reg_addr, reg_val);
            reg_addr += 4;
        }
    } else
        pr_info("unsupprt cmd!\n");
Err:
    kfree(buf_orig);
    return ret;
}
static DEVICE_ATTR(reg, S_IRUGO | S_IWUSR, reg_read, reg_write);

static const char *isp_dump_usage_str = {
    "Usage:\n"
    "echo <port:fr/ds1> <dst_path> > /sys/devices/platform/ff140000.isp/dump_frame; dump first buffer\n"
    "echo <port:fr/ds1> <dst_path> buff_size(H)  offset(H) > /sys/devices/platform/ff140000.isp/dump_frame; dump specific buffers\n"
};

static ssize_t dump_frame_read(
    struct device *dev,
    struct device_attribute *attr,
    char *buf)
{
    return sprintf(buf, "%s\n", isp_dump_usage_str);
}

static ssize_t dump_frame_write(
    struct device *dev, struct device_attribute *attr,
    char const *buf, size_t size)
{
    char *buf_orig, *parm[8] = {NULL};
    long val = 0;
    ssize_t ret = size;
    u32 buff_offset = 0, buff_size = 0x7e9000;

    if (!buf)
        return ret;

    buf_orig = kstrdup(buf, GFP_KERNEL);
    if (!buf_orig)
        return ret;

    parse_param(buf_orig, (char **)&parm);

    if (!strcmp(parm[0], "ds1")
        || !strcmp(parm[0], "fr")) {
        if (parm[3] != NULL) {
            if (kstrtol(parm[2], 16, &val) == 0)
                buff_size = val;
            if (kstrtol(parm[3], 16, &val) == 0)
                buff_offset = val;
        } else if (parm[2] != NULL) {
            if (kstrtol(parm[2], 16, &val) == 0)
                buff_size = val;
            buff_offset = 0;
        } else {
            buff_offset = 0;
            buff_size = 0x7e9000;
        }
    }

    if (!strcmp(parm[0], "ds1")) {
        if (parm[1] != NULL) {
            pr_info("use v4l2 test app dump DS1\n");
        }
    } else if (!strcmp(parm[0], "fr")) {
        if (parm[1] != NULL)
            write_to_file(parm[1], isp_kaddr + buff_offset, buff_size);
    } else
        pr_info("unsupprt cmd!\n");

    kfree(buf_orig);
    return ret;
}

static DEVICE_ATTR(dump_frame, S_IRUGO | S_IWUSR, dump_frame_read, dump_frame_write);

uint32_t write_reg(uint32_t val, unsigned long addr)
{
    void __iomem *io_addr;
    io_addr = ioremap_nocache(addr, 8);
    if (io_addr == NULL) {
        LOG(LOG_ERR, "%s: Failed to ioremap addr\n", __func__);
        return -1;
    }
    __raw_writel(val, io_addr);
    iounmap(io_addr);
    return 0;
}

uint32_t read_reg(unsigned long addr)
{
    void __iomem *io_addr;
    uint32_t ret;

    io_addr = ioremap_nocache(addr, 8);
    if (io_addr == NULL) {
        LOG(LOG_ERR, "%s: Failed to ioremap addr\n", __func__);
        return -1;
    }
    ret = (uint32_t)__raw_readl(io_addr);
    iounmap(io_addr);
    return ret;
}

uint32_t isp_power_on(void)
{
    uint32_t orig, tmp;

    orig = read_reg(AO_RTI_GEN_PWR_SLEEP0);			//AO_PWR_SLEEP0
    tmp = orig & 0xfff3ffff;						//set bit[18-19]=0
    write_reg(tmp, AO_RTI_GEN_PWR_SLEEP0);
    mdelay(5);
    orig = read_reg(AO_RTI_GEN_PWR_ISO0);			//AO_PWR_ISO0
    tmp = orig & 0xfff3ffff;						//set bit[18-19]=0
    write_reg(tmp, AO_RTI_GEN_PWR_ISO0);

    write_reg(0x0, HHI_ISP_MEM_PD_REG0);			//MEM_PD_REG0 set 0
    write_reg(0x0, HHI_ISP_MEM_PD_REG1);			//MEM_PD_REG1 set 0
    write_reg(0x5b446585, HHI_CSI_PHY_CNTL0);		//HHI_CSI_PHY_CNTL0
    write_reg(0x803f4321, HHI_CSI_PHY_CNTL1);		//HHI_CSI_PHY_CNTL1
    return 0;
}

void isp_power_down(void)
{
    return;
}

static void hw_reset(bool reset)
{
    void __iomem *reset_addr;
    uint32_t val;

    reset_addr = ioremap_nocache(0xffd01090, 8);;//ioremap_nocache(0xffd01014, 8);
    if (reset_addr == NULL) {
        LOG(LOG_ERR, "%s: Failed to ioremap\n", __func__);
        return;
    }

    val = __raw_readl(reset_addr);
    if (reset)
        val &= ~(1 << 1);
    else
        val |= (1 <<1);
    __raw_writel(val, reset_addr);

    if (!reset && reset_addr) {
        iounmap(reset_addr);
        reset_addr = NULL;
    }

    mdelay(5);

    iounmap(reset_addr);
    if (reset)
        LOG(LOG_INFO, "%s:reset isp\n", __func__);
    else
        LOG(LOG_INFO, "%s:release reset isp\n", __func__);
}

static uint32_t isp_module_check(struct platform_device *pdev)
{
    void __iomem *efuse_addr;
    uint32_t val;
    int ret;
    struct device_node *np = pdev->dev.of_node;
    unsigned int length = 0;
    u32 efuse_val[10] = {0};

    if (of_find_property(np, "isp-efuse", &length) == NULL) {
        LOG( LOG_CRIT, "warning, no efuse register mapping. Enabled as default\n");
        return 0;
    }
    if ((length / sizeof(u32)) > (sizeof(efuse_val) / sizeof(efuse_val[0]))) {
        LOG( LOG_CRIT, "exceed the isp array efuse_val size\n");
        return 1;
    }
    ret = of_property_read_u32_array(pdev->dev.of_node, "isp-efuse",
        efuse_val, length / sizeof(u32));
    if (ret) {
        LOG( LOG_CRIT, "warning, no efuse register mapping. Enabled as default\n");
        return 0;
    } else {
        LOG( LOG_ERR, "isp efuse value: %x %x\n", efuse_val[0], efuse_val[1]);
        efuse_addr = ioremap_nocache(efuse_val[0], 8);
        if (efuse_addr == NULL) {
            LOG( LOG_CRIT, "Failed to ioremap isp efuse register\n");
            return 1;
        } else {
            val = __raw_readl(efuse_addr);
            val = (val & efuse_val[1]);
            if (val == 0) {
                iounmap(efuse_addr);
                return 0;
            } else {
                iounmap(efuse_addr);
                return 1;
            }
        }
    }

    return 0;
}

static int32_t isp_platform_probe( struct platform_device *pdev )
{
    int32_t rc = 0;
    struct resource *isp_res;
    u32 isp_clk_rate = 666666667;
    u32 isp_mipi_rate = 200000000;

    if (isp_module_check(pdev)) {
        LOG( LOG_CRIT, "This chip don't have isp module.\n" );
        return rc;
    }

    // Initialize irq
    isp_res = platform_get_resource_byname( pdev,
        IORESOURCE_IRQ, "ISP" );

    if ( isp_res ) {
        LOG( LOG_ERR, "Juno isp irq = %d, flags = 0x%x !\n", (int)isp_res->start, (int)isp_res->flags );
        system_interrupts_set_irq( isp_res->start, isp_res->flags );
    } else {
        LOG( LOG_ERR, "Error, no isp_irq found from DT\n" );
        return -1;
    }

    isp_res = platform_get_resource( pdev,
        IORESOURCE_MEM, 0 );
    if ( isp_res ) {
        LOG( LOG_ERR, "Juno isp address = 0x%lx, end = 0x%lx !\n", isp_res->start, isp_res->end );
        if ( init_hw_io( isp_res->start, ( isp_res->end - isp_res->start ) + 1 ) != 0 ) {
            LOG( LOG_ERR, "Error on mapping gdc memory! \n" );
        }
    } else {
        LOG( LOG_ERR, "Error, no IORESOURCE_MEM DT!\n" );
    }

    isp_power_on();

    of_reserved_mem_device_init(&(pdev->dev));

    memset(&dev_info, 0, sizeof(dev_info));

    dev_info.am_sc = of_parse_phandle(pdev->dev.of_node, "link-device", 0);

    if (dev_info.am_sc == NULL) {
        LOG( LOG_ERR,"Failed to get link device\n");
    } else {
        LOG( LOG_ERR,"Success to get link device: %s\n", dev_info.am_sc->name);
        am_sc_parse_dt(dev_info.am_sc);
    }

    dev_info.clk_isp_0 = devm_clk_get(&pdev->dev, "cts_mipi_isp_clk_composite");
    if (IS_ERR(dev_info.clk_isp_0)) {
        LOG(LOG_ERR, "cannot get clock\n");
        dev_info.clk_isp_0 = NULL;
        return -1;
    }

    clk_set_rate(dev_info.clk_isp_0, isp_clk_rate);
    clk_prepare_enable(dev_info.clk_isp_0);

    isp_clk_rate = clk_get_rate(dev_info.clk_isp_0);
    LOG(LOG_ERR, "isp init clock is %d MHZ\n", isp_clk_rate / 1000000);

    dev_info.clk_mipi_0 = devm_clk_get(&pdev->dev, "cts_mipi_csi_phy_clk0_composite");
    if (IS_ERR(dev_info.clk_mipi_0)) {
        LOG(LOG_ERR, "cannot get clock\n");
        dev_info.clk_mipi_0 = NULL;
        return -1;
    }

    clk_set_rate(dev_info.clk_mipi_0, isp_mipi_rate);
    clk_prepare_enable(dev_info.clk_mipi_0);

    isp_mipi_rate = clk_get_rate(dev_info.clk_mipi_0);
    LOG(LOG_ERR, "mipi init clock is %d MHZ\n",isp_mipi_rate/1000000);

    hw_reset(true);

    system_interrupts_init();

    hw_reset(false);

    isp_pdev = pdev;
    static atomic_t drv_instance = ATOMIC_INIT( 0 );
    v4l2_device_set_name( &v4l2_dev, ISP_V4L2_MODULE_NAME, &drv_instance );
    rc = v4l2_device_register( NULL, &v4l2_dev );
    if ( rc == 0 ) {
        LOG( LOG_ERR, "register v4l2 driver. result %d.", rc );
    } else {
        LOG( LOG_ERR, "failed to register v4l2 device. rc = %d", rc );
        goto free_res;
    }

#if V4L2_SOC_SUBDEV_ENABLE
    int idx;

    LOG( LOG_ERR, "--------------------------------" );
    LOG( LOG_ERR, "Register %d subdevices", V4L2_SOC_SUBDEV_NUMBER );
    LOG( LOG_ERR, "--------------------------------" );

    g_subdevs.subdev_counter = 0;

    for ( idx = 0; idx < V4L2_SOC_SUBDEV_NUMBER; idx++ ) {
        g_subdevs.soc_async_sd[idx].match_type = V4L2_ASYNC_MATCH_CUSTOM;
        g_subdevs.soc_async_sd[idx].match.custom.match = NULL;

        g_subdevs.soc_async_sd_ptr[idx] = &g_subdevs.soc_async_sd[idx];
        g_subdevs.soc_subdevs[idx] = 0;
    }

    g_subdevs.notifier.bound = acamera_camera_async_bound;
    g_subdevs.notifier.complete = acamera_camera_async_complete;
    g_subdevs.notifier.unbind = acamera_camera_async_unbind;

    g_subdevs.notifier.subdevs = (struct v4l2_async_subdev **)&g_subdevs.soc_async_sd_ptr;
    g_subdevs.notifier.num_subdevs = V4L2_SOC_SUBDEV_NUMBER;

    rc = v4l2_async_notifier_register( &v4l2_dev, &g_subdevs.notifier );

    device_create_file(&pdev->dev, &dev_attr_reg);
    device_create_file(&pdev->dev, &dev_attr_dump_frame);

    LOG( LOG_ERR, "Init finished. async register notifier result %d. Waiting for subdevices", rc );
#else
    // no subdevice is used
    rc = isp_v4l2_create_instance( &v4l2_dev, isp_pdev);
    if ( rc < 0 )
        goto free_res;

    if ( rc == 0 ) {
        initialized = 1;
    } else {
        LOG( LOG_ERR, "Failed to register ISP v4l2 driver." );
        return -1;
    }

#endif

free_res:

    return rc;
}


static int isp_platform_remove(struct platform_device *pdev)
{
    device_remove_file(&pdev->dev, &dev_attr_reg);
    device_remove_file(&pdev->dev, &dev_attr_dump_frame);

    if ( initialized == 1 ) {
        isp_v4l2_destroy_instance(isp_pdev);
        initialized = 0;
    }

#if V4L2_SOC_SUBDEV_ENABLE
    v4l2_async_notifier_unregister( &g_subdevs.notifier );
#endif

    v4l2_device_unregister( &v4l2_dev );

    hw_reset(true);

    system_interrupts_deinit();

    if (dev_info.clk_mipi_0 != NULL) {
        clk_disable_unprepare(dev_info.clk_mipi_0);
        devm_clk_put(&pdev->dev, dev_info.clk_mipi_0);
        dev_info.clk_mipi_0 = NULL;
    }

    if (dev_info.clk_isp_0!= NULL) {
        clk_disable_unprepare(dev_info.clk_isp_0);
        devm_clk_put(&pdev->dev, dev_info.clk_isp_0);
        dev_info.clk_isp_0 = NULL;
    }

    if (dev_info.am_sc != NULL) {
        am_sc_deinit_parse_dt();
        dev_info.am_sc = NULL;
    }

    isp_power_down();

    close_hw_io();

    LOG(LOG_ERR, "Isp remove\n");
    return 0;
}

static const struct of_device_id isp_dt_match[] = {
    {.compatible = "arm, isp"},
    {}};

MODULE_DEVICE_TABLE( of, isp_dt_match );

static struct platform_driver isp_platform_driver = {
    .probe = isp_platform_probe,
    .remove	= isp_platform_remove,
    .driver = {
        .name = "arm_isp",
        .owner = THIS_MODULE,
        .of_match_table = isp_dt_match,
    },
};

static int __init fw_module_init( void )
{
    int32_t rc = 0;

    LOG( LOG_ERR, "Juno isp fw_module_init\n" );

	rc = platform_driver_register(&isp_platform_driver);

    return rc;
}

static void __exit fw_module_exit( void )
{
    LOG( LOG_ERR, "Juno isp fw_module_exit\n" );

    platform_driver_unregister( &isp_platform_driver );
}

module_init( fw_module_init );
module_exit( fw_module_exit );
MODULE_LICENSE( "GPL v2" );
MODULE_AUTHOR( "ARM IVG SW" );
