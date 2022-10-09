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
#include "system_am_sc1.h"
#include "system_am_sc2.h"
#include "system_am_sc3.h"
#include "system_am_flicker.h"
#include "system_am_decmpr.h"
#include "system_am_md.h"
#include "isp_config_seq.h"

#include <linux/amlogic/power_domain.h>

#include <linux/fs.h>
#include <asm/uaccess.h>
#include <asm/unaligned.h>
#include <linux/delay.h>
#include "v4l2_interface/isp-v4l2.h"
#include <linux/pm_runtime.h>

#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
#include <linux/amlogic/pm.h>
static struct early_suspend early_suspend;
#endif

static int ds_port;
module_param(ds_port, int, 0664);

extern void system_isp_proc_create( struct device *dev );
extern void system_isp_proc_remove( struct device *dev );
uint32_t global_isp_clk_rate = 500000000;

#define LOG_CONTEXT "[ ACamera ]"
#define ISP_V4L2_MODULE_NAME "isp-v4l2"

unsigned int g_firmware_context_number = 1;
unsigned int dcam = 1;
module_param(dcam, uint, 0664);
MODULE_PARM_DESC(dcam, "\n camera number\n");
#define P_PWRCTRL_FOCRST0       (0xfe00c030)

#define P_PWRCTRL_PWR_OFF0      (0xfe00c010)
#define P_PWRCTRL_MEM_PD13      (0xfe00c074)
#define P_PWRCTRL_MEM_PD14      (0xfe00c078)
#define P_PWRCTRL_MEM_PD15      (0xfe00c07c)
#define P_PWRCTRL_MEM_PD16      (0xfe00c080)
#define P_PWRCTRL_MEM_PD17      (0xfe00c084)

#define P_PWRCTRL_ISO_EN0       (0xfe00c020)

#define CLKCTRL_MIPI_ISP_CLK_CTRL (0xfe000110)
#define CLKCTRL_MIPI_CSI_PHY_CLK_CTRL (0xfe00010c)

//Wr(CLKCTRL_MIPI_ISP_CLK_CTRL,(2<<9)|(1<<8)|(0<<0)); //666Mhz
//Wr(CLKCTRL_MIPI_CSI_PHY_CLK_CTRL,0xb02);//enable DPHY clk


#define ANACTRL_CSI_PHY_CNTL0   (0xfe007c00 + (0x0090 << 2))
#define ANACTRL_CSI_PHY_CTRL1   (0xfe007c00 + (0x0091 << 2))
#define ANACTRL_CSI_PHY_CTRL2   (0xfe007c00 + (0x0092 << 2))
#define ANACTRL_CSI_PHY_CTRL3   (0xfe007c00 + (0x0093 << 2))

#define ISP_MIPI_CLK            0xfe000910

#define RESETCTRL_RESET3_LEVEL  (0xfe00204c)


struct device_info {
    struct clk* clk_isp_0;
    struct clk* clk_mipi_0;
    struct device_node *am_sc;
    struct device_node *am_md;
	struct device_node *am_flicker;
	struct device_node *am_cmpr;
    int clk_level;
};

extern uint32_t seamless;
extern temper_addr isp_temper_paddr[FIRMWARE_CONTEXT_NUMBER];

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
    uint32_t hw_isp_addr;
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
            rc = isp_v4l2_create_instance( &v4l2_dev, isp_pdev, g_subdevs.hw_isp_addr );

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

void cache_flush_for_device(uint32_t buf_start, uint32_t buf_size)
{
    if ((buf_start == 0) || (buf_size == 0))
        return;
    if ( isp_pdev != NULL)
        dma_sync_single_for_device(&isp_pdev->dev, buf_start, buf_size, DMA_TO_DEVICE);
    else {
        pr_err("%s: isp_pdev is null, cache_flush failed!\n", __func__);
        return;
    }
}

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
    "echo <port:fr/ds1> <dst_path> > /sys/devices/platform/ff000000.isp/dump_frame; dump first buffer\n"
    "echo <port:fr/ds1> <dst_path> buff_size(H)  offset(H) > /sys/devices/platform/ff000000.isp/dump_frame; dump specific buffers\n"
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
            write_to_file(parm[1], phys_to_virt(isp_temper_paddr[0].isp_paddr) + buff_offset, buff_size);
    } else
        pr_info("unsupprt cmd!\n");

    kfree(buf_orig);
    return ret;
}

static DEVICE_ATTR(dump_frame, S_IRUGO | S_IWUSR, dump_frame_read, dump_frame_write);

static const char *isp_clk_level_usage_str = {
    "Usage:\n"
    "echo clk_lvl > /sys/devices/platform/ff000000.isp/isp_clk;\n"
    "------clk_lvl------\n"
    "0: default;\n"
    "1: 100M;\n"
    "2: 200M;\n"
    "3: 250M;\n"
    "4: 286M;\n"
    "5: 333M;\n"
    "6: 400M;\n"
    "7: 500M;\n"
    "8: 667M;\n"
};

static ssize_t isp_clk_read(
    struct device *dev,
    struct device_attribute *attr,
    char *buf)
{
    return sprintf(buf, "%s\n", isp_clk_level_usage_str);
}

static ssize_t isp_clk_write(
    struct device *dev, struct device_attribute *attr,
    char const *buf, size_t size)
{
    char *buf_orig, *parm[5] = {NULL};
    long val = 0;
    ssize_t ret = size;
    ssize_t rt = size;
    u32 clk_level = 0;
    u32 clk_rate = 0;
    if (!buf)
        return ret;

    buf_orig = kstrdup(buf, GFP_KERNEL);
    if (!buf_orig)
        return ret;

    parse_param(buf_orig, (char **)&parm);

    if (parm[0] != NULL) {
        if (kstrtol(parm[0], 16, &val) == 0)
            clk_level = val;
    } else {
        kfree(buf_orig);
        return -EINVAL;
    }

    switch (clk_level) {
        case 0:
            clk_rate = 666666667;
            break;
        case 1:
            clk_rate = 100000000;
            break;
        case 2:
            clk_rate = 200000000;
            break;
        case 3:
            clk_rate = 250000000;
            break;
        case 4:
            clk_rate = 286000000;
            break;
        case 5:
            clk_rate = 333333333;
            break;
        case 6:
            clk_rate = 400000000;
            break;
        case 7:
            clk_rate = 500000000;
            break;
        case 8:
            clk_rate = 666666667;
            break;
        default:
            pr_err("Invalid clk level %d !\n", clk_level);
            break;
    }
    if (clk_rate == 0) {
         kfree(buf_orig);
         return -EINVAL;
    }
    pr_err("clk_level: %d, clk %d\n",clk_level, clk_rate);
    clk_set_rate(dev_info.clk_isp_0, clk_rate);
    rt = clk_prepare_enable(dev_info.clk_isp_0);
    if (rt != 0) {
        LOG(LOG_CRIT, "Failed to enable isp clk");
        return -EINVAL;
    }
    kfree(buf_orig);
    return ret;
}

static DEVICE_ATTR(isp_clk, S_IRUGO | S_IWUSR, isp_clk_read, isp_clk_write);

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

int aml_cmpr_bypass(void) {
    uint32_t data = 0;
	LOG(LOG_CRIT, "Frank Debug ... %ss, %d\n", __func__, __LINE__);
    data = read_reg(MIPI_TOP_ADAPT_DE_CTRL0);
	LOG(LOG_CRIT, "Frank Debug ... %ss, %d\n", __func__, __LINE__);
    write_reg(data | 1<<7 | 1<<3, MIPI_TOP_ADAPT_DE_CTRL0);  //adapt_de_bypass
	LOG(LOG_CRIT, "Frank Debug ... %ss, %d\n", __func__, __LINE__);

#if ISP_HAS_CMPR == 0
	LOG(LOG_CRIT, "Frank Debug ... %ss, %d\n", __func__, __LINE__);
    data = read_reg(MIPI_TOP_TNR_DE_CTRL0);
	LOG(LOG_CRIT, "Frank Debug ... %ss, %d\n", __func__, __LINE__);
    write_reg(data | 1 << 0, MIPI_TOP_TNR_DE_CTRL0);       //tnr_de_bypass
	LOG(LOG_CRIT, "Frank Debug ... %ss, %d\n", __func__, __LINE__);
#endif
    return 0;
}

#if 0
static struct isp_ctrl_s default_poweron_ctrl[] = {
	/* power up ge2d */
	{GEN_PWR_SLEEP0, AO_RTI_GEN_PWR_SLEEP0, 0, 19, 1, 0},
	/* Power up memory */
	{MEM_PD_REG0, HHI_MEM_PD_REG0, 0, 18, 8, 100},
	/* reset */
	{CBUS_BASE, RESET2_LEVEL, 0, 6, 1, 0},
	/* remove isolation */
	{GEN_PWR_ISO0, AO_RTI_GEN_PWR_ISO0, 0, 19, 1, 0},
	/* pull up reset */
	{CBUS_BASE, RESET2_LEVEL, 1, 6, 1, 0}
};

static struct isp_ctrl_s default_poweroff_ctrl[] = {
	/* power up ge2d */
	{GEN_PWR_SLEEP0, AO_RTI_GEN_PWR_SLEEP0, 0, 19, 1, 0},
	/* Power up memory */
	{MEM_PD_REG0, HHI_MEM_PD_REG0, 0, 18, 8, 100},
	/* reset */
	{CBUS_BASE, RESET2_LEVEL, 0, 6, 1, 0},
	/* remove isolation */
	{GEN_PWR_ISO0, AO_RTI_GEN_PWR_ISO0, 0, 19, 1, 0},
	/* pull up reset */
	{CBUS_BASE, RESET2_LEVEL, 1, 6, 1, 0}
};

static inline void isp_set_pwr_tbl_bits(unsigned int table_type,
					 unsigned int reg, unsigned int val,
					 unsigned int start, unsigned int len)
{
	switch (table_type) {
	case GEN_PWR_SLEEP0:
		power_ctrl_sleep(val ? 0 : 1, start);
		break;
	case GEN_PWR_ISO0:
		power_ctrl_iso(val ? 0 : 1, start);
		break;
	case MEM_PD_REG0:
		power_ctrl_mempd0(val ? 0 : 1, 0xFF, start);
		break;
	default:
		pr_err("unsupported bus type\n");
		break;
	}
}

static void isp_pwr_config(bool enable)
{
	int i, table_size;
	struct isp_ctrl_s tmp;
	struct isp_ctrl_s *power_table;

	if (default_poweron_ctrl) {
		if (enable) {
			power_table = default_poweron_ctrl;
			table_size = sizeof(default_poweron_ctrl);
		} else {
			power_table = default_poweroff_ctrl;
			table_size = sizeof(default_poweroff_ctrl);
		}

		for (i = 0; i < table_size; i++) {
			tmp = power_table[i];
			isp_set_pwr_tbl_bits(tmp.table_type, tmp.reg, tmp.val,
					      tmp.start, tmp.len);

			if (tmp.udelay > 0)
				udelay(tmp.udelay);
		}
	}

	//isp_clk_config(enable);
}
#endif
#define PDID_T7_ISP			16
#define PDID_T7_MIPI_ISP	17

int32_t isp_power_on(struct device *dev)
{
    int ret = -1;

    ret = pm_runtime_get_sync(dev);
    if (ret < 0)
        LOG(LOG_CRIT,"runtime get power error\n");
    return ret;
}

int32_t isp_power_down(struct device *dev)
{
    int ret = -1;
    ret = pm_runtime_put_sync(dev);
    if (ret < 0)
        LOG(LOG_CRIT,"runtime put power error\n");

    return ret;
}


static void hw_reset(bool reset)
{
    void __iomem *reset_addr;

    uint32_t val;
    reset_addr = ioremap_nocache(RESETCTRL_RESET3_LEVEL, 8);
    if (reset_addr == NULL) {
        LOG(LOG_ERR, "%s: Failed to ioremap\n", __func__);
        return;
    }

    val = __raw_readl(reset_addr);
    if (reset)
        val &= ~(1 << 26);
    else
        val |= (1 << 26);
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

int32_t isp_clk_enable(void)
{

    int32_t rc = 0;

    uint32_t isp_clk_rate = 666666667;

    switch (dev_info.clk_level) {
    case 0:
        isp_clk_rate = 666666667;
        break;
    case 1:
        isp_clk_rate = 100000000;
        break;
    case 2:
        isp_clk_rate = 200000000;
        break;
    case 3:
        isp_clk_rate = 250000000;
        break;
    case 4:
        isp_clk_rate = 286000000;
        break;
    case 5:
        isp_clk_rate = 333333333;
        break;
    case 6:
        isp_clk_rate = 400000000;
        break;
    case 7:
        isp_clk_rate = 500000000;
        break;
    case 8:
        isp_clk_rate = 666666667;
        break;
    default:
        pr_err("Invalid clk level %d !\n", dev_info.clk_level);
        break;
    }
    //isp_clk_rate = 666666667;
    global_isp_clk_rate = 500000000;
    uint32_t isp_mipi_rate = 200000000;
    clk_set_rate(dev_info.clk_isp_0, isp_clk_rate);
    clk_set_rate(dev_info.clk_mipi_0, isp_mipi_rate);
    LOG(LOG_CRIT, "get clk isp:%ld, mipi:%ld\n", clk_get_rate(dev_info.clk_isp_0), clk_get_rate(dev_info.clk_mipi_0));

    rc = clk_prepare_enable(dev_info.clk_isp_0);
    if (rc != 0) {
        LOG(LOG_CRIT, "Failed to enable isp clk");
        return rc;
    }

    rc = clk_prepare_enable(dev_info.clk_mipi_0);
    if (rc != 0) {
        LOG(LOG_CRIT, "Failed to enable mipi clk");
        return rc;
    }

    return rc;
}

void isp_clk_disable(void)
{
    clk_disable_unprepare(dev_info.clk_mipi_0);
    clk_disable_unprepare(dev_info.clk_isp_0);
}

#if 0
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
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 10, 0))
static const struct v4l2_async_notifier_operations acamera_camera_async_ops = {
    .bound = acamera_camera_async_bound,
    .unbind = acamera_camera_async_unbind,
    .complete = acamera_camera_async_complete,
};
#endif

#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
extern void interrupt_handler( void *data, uint32_t mask );
extern int fw_intf_is_isp_started( void );
static void isp_early_suspend(struct early_suspend *h)
{
    if (fw_intf_is_isp_started())
        system_interrupts_deinit();
}

static void isp_late_resume(struct early_suspend *h)
{
    if (fw_intf_is_isp_started()) {
        acamera_load_isp_sequence( 0, seq_table, 1 );
        system_interrupt_set_handler( interrupt_handler, NULL );
        system_interrupts_init();
    }
}
#endif

static int32_t isp_platform_probe( struct platform_device *pdev )
{
    int32_t rc = 0;
    struct resource *isp_res;

    // Initialize irq
    isp_res = platform_get_resource_byname( pdev,
        IORESOURCE_IRQ, "ISP" );

    if ( isp_res ) {
        LOG( LOG_ERR, "Juno isp irq = %d, flags = 0x%x !\n", (int)isp_res->start, (int)isp_res->flags );
        system_interrupts_set_irq( isp_res->start, 4);
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

    pm_runtime_enable(&pdev->dev);
    isp_power_on(&pdev->dev);

    of_reserved_mem_device_init(&(pdev->dev));

    memset(&dev_info, 0, sizeof(dev_info));

    dev_info.am_sc = of_parse_phandle(pdev->dev.of_node, "link-device", 0);
#if ISP_HAS_MD
	dev_info.am_md = of_parse_phandle(pdev->dev.of_node, "md-device", 0);
	if (dev_info.am_md== NULL) {
		LOG( LOG_ERR,"Failed to get md-device\n");
	} else {
		LOG( LOG_ERR,"Success to get md device: %s\n", dev_info.am_md->name);
		am_md_parse_dt(dev_info.am_md);
	}
#endif
    if (dev_info.am_sc == NULL) {
        LOG( LOG_ERR,"Failed to get link device\n");
    } else {
        LOG( LOG_ERR,"Success to get link device: %s\n", dev_info.am_sc->name);
        am_sc_parse_dt(dev_info.am_sc, 0);
		am_sc1_parse_dt(dev_info.am_sc, 1);
		am_sc2_parse_dt(dev_info.am_sc, 2);
		am_sc3_parse_dt(dev_info.am_sc, 3);
    }
#if ISP_HAS_FLICKER
    dev_info.am_flicker = of_parse_phandle(pdev->dev.of_node, "att-device", 0);

    if (dev_info.am_flicker == NULL) {
        LOG( LOG_CRIT,"Failed to get att device\n");
    } else {
        LOG( LOG_CRIT,"Success to get att device: %s\n", dev_info.am_flicker->name);
        am_flicker_parse_dt(dev_info.am_flicker);
    }
#endif

#if ISP_HAS_CMPR
    dev_info.am_cmpr = of_parse_phandle(pdev->dev.of_node, "cmpr-device", 0);

    if (dev_info.am_cmpr == NULL) {
        LOG( LOG_CRIT,"Failed to get cmpr device\n");
    } else {
        LOG( LOG_CRIT,"Success to get cmpr device: %s\n", dev_info.am_cmpr->name);
        am_cmpr_parse_dt(dev_info.am_cmpr);
    }
#endif

    dev_info.clk_isp_0 = devm_clk_get(&pdev->dev, "cts_mipi_isp_clk");
    dev_info.clk_mipi_0 = devm_clk_get(&pdev->dev, "cts_mipi_csi_phy_clk0");
    rc = of_property_read_u32(pdev->dev.of_node, "clk-level", &(dev_info.clk_level));

    if (IS_ERR(dev_info.clk_isp_0)) {
        LOG(LOG_CRIT, "cannot get isp clock\n");
        dev_info.clk_isp_0 = NULL;
        return -1;
    }

    if (IS_ERR(dev_info.clk_mipi_0)) {
        LOG(LOG_CRIT, "cannot get phy clock\n");
        dev_info.clk_mipi_0 = NULL;
        return -1;
    }
    if (rc != 0) {
        pr_err("%s: failed to get isp clk level\n", __func__);
        dev_info.clk_level = 0;
    }

    isp_clk_enable();

    if (seamless)
    {
        if (acamera_isp_input_port_mode_status_read( 0 ) != ACAMERA_ISP_INPUT_PORT_MODE_REQUEST_SAFE_START)
        {
            hw_reset(true);
            system_interrupts_init();
            hw_reset(false);
        }
        else
            system_interrupts_init();
    }
    else
    {
        hw_reset(true);
        system_interrupts_init();
        hw_reset(false);
    }

    isp_pdev = pdev;
    static atomic_t drv_instance = ATOMIC_INIT( 0 );
    v4l2_device_set_name( &v4l2_dev, ISP_V4L2_MODULE_NAME, &drv_instance );
    rc = v4l2_device_register( &pdev->dev, &v4l2_dev );
    if ( rc == 0 ) {
        LOG( LOG_ERR, "register v4l2 driver. result %d.", rc );
    } else {
        LOG( LOG_ERR, "failed to register v4l2 device. rc = %d", rc );
        goto free_res;
    }

    g_firmware_context_number = dcam;
    LOG(LOG_CRIT, "cam num:%d", g_firmware_context_number);
    int idx;

    LOG( LOG_ERR, "--------------------------------" );
    LOG( LOG_ERR, "Register %d subdevices", V4L2_SOC_SUBDEV_NUMBER );
    LOG( LOG_ERR, "--------------------------------" );

	LOG(LOG_CRIT, "Frank Debug ... %ss, %d\n", __func__, __LINE__);

    g_subdevs.subdev_counter = 0;

	v4l2_async_notifier_init(&g_subdevs.notifier);

    for ( idx = 0; idx < V4L2_SOC_SUBDEV_NUMBER; idx++ ) {
        g_subdevs.soc_async_sd[idx].match_type = V4L2_ASYNC_MATCH_CUSTOM;
        g_subdevs.soc_async_sd[idx].match.custom.match = NULL;
        g_subdevs.soc_async_sd_ptr[idx] = &g_subdevs.soc_async_sd[idx];
        g_subdevs.soc_subdevs[idx] = 0;
		v4l2_async_notifier_add_subdev(&g_subdevs.notifier, &g_subdevs.soc_async_sd[idx]);
    }

	LOG(LOG_CRIT, "Frank Debug ... %ss, %d\n", __func__, __LINE__);
    g_subdevs.hw_isp_addr = (uint32_t)isp_res->start; //ISP_SOC_START_ADDR;
    g_subdevs.notifier.ops = &acamera_camera_async_ops;
    rc = v4l2_async_notifier_register( &v4l2_dev, &g_subdevs.notifier );

	LOG(LOG_CRIT, "Frank Debug ... %ss, %d\n", __func__, __LINE__);
    device_create_file(&pdev->dev, &dev_attr_reg);
	LOG(LOG_CRIT, "Frank Debug ... %ss, %d\n", __func__, __LINE__);
    device_create_file(&pdev->dev, &dev_attr_dump_frame);
	LOG(LOG_CRIT, "Frank Debug ... %ss, %d\n", __func__, __LINE__);
    device_create_file(&pdev->dev, &dev_attr_isp_clk);
	LOG(LOG_CRIT, "Frank Debug ... %ss, %d\n", __func__, __LINE__);
    system_dbg_create(&pdev->dev);
	LOG(LOG_CRIT, "Frank Debug ... %ss, %d\n", __func__, __LINE__);
    system_isp_proc_create(&pdev->dev);
	LOG(LOG_CRIT, "Frank Debug ... %ss, %d\n", __func__, __LINE__);
    aml_cmpr_bypass();
	LOG(LOG_CRIT, "Frank Debug ... %ss, %d\n", __func__, __LINE__);
    LOG( LOG_ERR, "Init finished. async register notifier result %d. Waiting for subdevices", rc );

	LOG(LOG_CRIT, "Frank Debug ... %ss, %d\n", __func__, __LINE__);
#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
    early_suspend.level = EARLY_SUSPEND_LEVEL_STOP_DRAWING;
    early_suspend.suspend = isp_early_suspend;
    early_suspend.resume = isp_late_resume;
    early_suspend.param = pdev;
    register_early_suspend(&early_suspend);
	LOG(LOG_CRIT, "Frank Debug ... %ss, %d\n", __func__, __LINE__);
#endif

free_res:

    return rc;
}


static int isp_platform_remove(struct platform_device *pdev)
{
#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
    unregister_early_suspend(&early_suspend);
#endif

    device_remove_file(&pdev->dev, &dev_attr_reg);
    device_remove_file(&pdev->dev, &dev_attr_dump_frame);
    device_remove_file(&pdev->dev, &dev_attr_isp_clk);
    system_dbg_remove(&pdev->dev);
    system_isp_proc_remove(&pdev->dev);
    if ( initialized == 1 ) {
        isp_v4l2_destroy_instance(isp_pdev);
        initialized = 0;
    }

#if ISP_HAS_FLICKER
    aml_flicker_deinit();
#endif

#if ISP_HAS_CMPR
    aml_cmpr_deinit();
#endif

#if ISP_HAS_MD
    am_md_destroy();
#endif

#if V4L2_SOC_SUBDEV_ENABLE
    v4l2_async_notifier_unregister( &g_subdevs.notifier );
	//v4l2_async_notifier_cleanup( &g_subdevs.notifier );

#endif

    v4l2_device_unregister( &v4l2_dev );

    hw_reset(true);

    system_interrupts_deinit();

    //isp_clk_disable();

    if (dev_info.clk_mipi_0 != NULL) {
        devm_clk_put(&pdev->dev, dev_info.clk_mipi_0);
        dev_info.clk_mipi_0 = NULL;
    }

    if (dev_info.clk_isp_0!= NULL) {
        devm_clk_put(&pdev->dev, dev_info.clk_isp_0);
        dev_info.clk_isp_0 = NULL;
    }

    if (dev_info.am_sc != NULL) {
        am_sc_deinit_parse_dt();
        am_sc1_deinit_parse_dt();
        am_sc2_deinit_parse_dt();
        am_sc3_deinit_parse_dt();
        dev_info.am_sc = NULL;
    }

    isp_power_down(&pdev->dev);

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
