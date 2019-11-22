/*
 * drivers/amlogic/media/vout/vdac/vdac_dev.c
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

/* Standard Linux headers */
#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/clk.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/media/vout/vdac_dev.h>
#include <linux/amlogic/iomap.h>
#include <linux/io.h>
#include <linux/mutex.h>
#include "vdac_dev.h"

#define AMVDAC_NAME               "amvdac"
#define AMVDAC_DRIVER_NAME        "amvdac"
#define AMVDAC_MODULE_NAME        "amvdac"
#define AMVDAC_DEVICE_NAME        "amvdac"
#define AMVDAC_CLASS_NAME         "amvdac"

struct amvdac_dev_s {
	dev_t                       devt;
	struct cdev                 cdev;
	dev_t                       devno;
	struct device               *dev;
	struct class                *clsp;
};
static struct amvdac_dev_s amvdac_dev;
static struct meson_vdac_data *s_vdac_data;
static struct mutex vdac_mutex;

/* priority for module,
 * #define VDAC_MODULE_ATV_DEMOD (1 << 0)
 * #define VDAC_MODULE_DTV_DEMOD (1 << 1)
 * #define VDAC_MODULE_TVAFE     (1 << 2)
 * #define VDAC_MODULE_CVBS_OUT  (1 << 3)
 * #define VDAC_MODULE_AUDIO_OUT (1 << 4)
 * #define VDAC_MODULE_AVOUT_ATV (1 << 5)
 * #define VDAC_MODULE_AVOUT_AV  (1 << 6)
 */
static unsigned int pri_flag;

static unsigned int vdac_debug_print;

static inline unsigned int vdac_hiu_reg_read(unsigned int reg)
{
	return aml_read_hiubus(reg);
}

static inline void vdac_hiu_reg_write(unsigned int reg, unsigned int val)
{
	aml_write_hiubus(reg, val);
}

static inline void vdac_hiu_reg_setb(unsigned int reg, unsigned int value,
		unsigned int start, unsigned int len)
{
	vdac_hiu_reg_write(reg, ((vdac_hiu_reg_read(reg) &
			~(((1L << (len)) - 1) << (start))) |
			(((value) & ((1L << (len)) - 1)) << (start))));
}

static inline unsigned int vdac_hiu_reg_getb(unsigned int reg,
		unsigned int start, unsigned int len)
{
	unsigned int val;

	val = ((vdac_hiu_reg_read(reg) >> (start)) & ((1L << (len)) - 1));

	return val;
}

static inline unsigned int vdac_vcbus_reg_read(unsigned int reg)
{
	return aml_read_vcbus(reg);
}

static inline void vdac_vcbus_reg_write(unsigned int reg, unsigned int val)
{
	aml_write_vcbus(reg, val);
}

static inline void vdac_vcbus_reg_setb(unsigned int reg, unsigned int value,
		unsigned int start, unsigned int len)
{
	vdac_vcbus_reg_write(reg, ((vdac_vcbus_reg_read(reg) &
			~(((1L << (len)) - 1) << (start))) |
			(((value) & ((1L << (len)) - 1)) << (start))));
}

static inline unsigned int vdac_vcbus_reg_getb(unsigned int reg,
		unsigned int start, unsigned int len)
{
	unsigned int val;

	val = ((vdac_vcbus_reg_read(reg) >> (start)) & ((1L << (len)) - 1));

	return val;
}

static int vdac_ctrl_config(bool on, unsigned int reg, unsigned int bit)
{
	struct meson_vdac_ctrl_s *table = s_vdac_data->ctrl_table;
	unsigned int val;
	int i = 0;
	int ret = -1;

	while (i < VDAC_CTRL_MAX) {
		if (table[i].reg == VDAC_REG_MAX)
			break;
		if ((table[i].reg == reg) && (table[i].bit == bit)) {
			if (on)
				val = table[i].val;
			else
				val = table[i].val ? 0 : 1;
			vdac_hiu_reg_setb(reg, val, bit, table[i].len);
			if (vdac_debug_print) {
				pr_info("vdac: reg=0x%02x set bit%d=%d, readback=0x%08x\n",
					reg, bit, val, vdac_hiu_reg_read(reg));
			}
			ret = 0;
			break;
		}
		i++;
	}

	return ret;
}

static void vdac_enable_avout_atv(bool on)
{
	unsigned int reg_cntl0 = s_vdac_data->reg_cntl0;
	unsigned int reg_cntl1 = s_vdac_data->reg_cntl1;

	if (on) {
		/* clock delay control */
		vdac_hiu_reg_setb(HHI_VIID_CLK_DIV, 1, 19, 1);
		/* vdac_clock_mux form atv demod */
		vdac_hiu_reg_setb(HHI_VID_CLK_CNTL2, 1, 8, 1);
		vdac_hiu_reg_setb(HHI_VID_CLK_CNTL2, 1, 4, 1);
		/* vdac_clk gated clock control */
		vdac_vcbus_reg_setb(VENC_VDAC_DACSEL0, 1, 5, 1);

		vdac_ctrl_config(1, reg_cntl0, 9);
		/*after txlx need reset bandgap after bit9 enabled*/
		/*bit10 reset bandgap in g12a*/
		if (s_vdac_data->cpu_id == VDAC_CPU_TXLX) {
			vdac_ctrl_config(0, reg_cntl0, 13);
			udelay(5);
			vdac_ctrl_config(1, reg_cntl0, 13);
		}

		/*Cdac pwd*/
		vdac_ctrl_config(1, reg_cntl1, 3);
		/* enable AFE output buffer */
		if (s_vdac_data->cpu_id < VDAC_CPU_G12AB)
			vdac_ctrl_config(0, reg_cntl0, 10);
		vdac_ctrl_config(1, reg_cntl0, 0);


	} else {
		vdac_ctrl_config(0, reg_cntl0, 9);

		vdac_ctrl_config(0, reg_cntl0, 0);
		/* Disable AFE output buffer */
		if (s_vdac_data->cpu_id < VDAC_CPU_G12AB)
			vdac_ctrl_config(0, reg_cntl0, 10);
		/* disable dac output */
		vdac_ctrl_config(0, reg_cntl1, 3);

		/* vdac_clk gated clock control */
		vdac_vcbus_reg_setb(VENC_VDAC_DACSEL0, 0, 5, 1);
		/* vdac_clock_mux form atv demod */
		vdac_hiu_reg_setb(HHI_VID_CLK_CNTL2, 0, 4, 1);
		vdac_hiu_reg_setb(HHI_VID_CLK_CNTL2, 0, 8, 1);
		/* clock delay control */
		vdac_hiu_reg_setb(HHI_VIID_CLK_DIV, 0, 19, 1);
	}
}

static void vdac_enable_dtv_demod(bool on)
{
	unsigned int reg_cntl0 = s_vdac_data->reg_cntl0;
	unsigned int reg_cntl1 = s_vdac_data->reg_cntl1;

	if (on) {
		vdac_ctrl_config(1, reg_cntl0, 9);
		if (s_vdac_data->cpu_id == VDAC_CPU_TXLX) {
			vdac_ctrl_config(0, reg_cntl0, 13);
			udelay(5);
			vdac_ctrl_config(1, reg_cntl0, 13);
		}
		vdac_ctrl_config(1, reg_cntl1, 3);
		if (s_vdac_data->cpu_id < VDAC_CPU_G12AB)
			vdac_ctrl_config(0, reg_cntl0, 10);
		vdac_ctrl_config(1, reg_cntl0, 0);
	} else {
		vdac_ctrl_config(0, reg_cntl0, 9);
		vdac_ctrl_config(0, reg_cntl1, 3);
	}
}

static void vdac_enable_avout_av(bool on)
{
	unsigned int reg_cntl0 = s_vdac_data->reg_cntl0;
	unsigned int reg_cntl1 = s_vdac_data->reg_cntl1;

	if (on) {
		vdac_ctrl_config(1, reg_cntl0, 9);
		/*txlx need reset bandgap after bit9 enabled*/
		if (s_vdac_data->cpu_id == VDAC_CPU_TXLX) {
			vdac_ctrl_config(0, reg_cntl0, 13);
			udelay(5);
			vdac_ctrl_config(1, reg_cntl0, 13);
		}

		vdac_ctrl_config(1, reg_cntl1, 3);
		if (s_vdac_data->cpu_id < VDAC_CPU_G12AB)
			vdac_ctrl_config(1, reg_cntl0, 10);
		if (s_vdac_data->cpu_id == VDAC_CPU_TL1 ||
			s_vdac_data->cpu_id == VDAC_CPU_TM2) {
			/*[6][8]bypass buffer enable*/
			vdac_ctrl_config(1, reg_cntl1, 6);
			vdac_ctrl_config(1, reg_cntl1, 8);
		}
	} else {
		vdac_ctrl_config(0, reg_cntl0, 9);
		if (s_vdac_data->cpu_id == VDAC_CPU_TL1 ||
			s_vdac_data->cpu_id == VDAC_CPU_TM2) {
			/*[6][8]bypass buffer disable*/
			vdac_ctrl_config(0, reg_cntl1, 6);
			vdac_ctrl_config(0, reg_cntl1, 8);
		}

		/* Disable AFE output buffer */
		if (s_vdac_data->cpu_id < VDAC_CPU_G12AB)
			vdac_ctrl_config(0, reg_cntl0, 10);
		/* disable dac output */
		vdac_ctrl_config(0, reg_cntl1, 3);
	}
}

static void vdac_enable_cvbs_out(bool on)
{
	unsigned int reg_cntl0 = s_vdac_data->reg_cntl0;
	unsigned int reg_cntl1 = s_vdac_data->reg_cntl1;

	if (on) {
		vdac_ctrl_config(1, reg_cntl1, 3);
		vdac_ctrl_config(1, reg_cntl0, 0);
		vdac_ctrl_config(1, reg_cntl0, 9);
		if (s_vdac_data->cpu_id == VDAC_CPU_TXLX) {
			vdac_ctrl_config(0, reg_cntl0, 13);
			udelay(5);
			vdac_ctrl_config(1, reg_cntl0, 13);
		}
		if (s_vdac_data->cpu_id < VDAC_CPU_G12AB)
			vdac_ctrl_config(0, reg_cntl0, 10);
	} else {
		vdac_ctrl_config(0, reg_cntl0, 9);
		vdac_ctrl_config(0, reg_cntl0, 0);
		vdac_ctrl_config(0, reg_cntl1, 3);
	}
}

static void vdac_enable_audio_out(bool on)
{
	unsigned int reg_cntl0 = s_vdac_data->reg_cntl0;

	/*Bandgap optimization*/
	if (s_vdac_data->cpu_id == VDAC_CPU_TXLX)
		vdac_hiu_reg_setb(reg_cntl0, 0xe, 3, 5);

	if (s_vdac_data->cpu_id == VDAC_CPU_TXL ||
		s_vdac_data->cpu_id == VDAC_CPU_TXLX ||
		s_vdac_data->cpu_id == VDAC_CPU_TL1 ||
		s_vdac_data->cpu_id == VDAC_CPU_TM2) {
		if (on)
			vdac_ctrl_config(1, reg_cntl0, 9);
		else
			vdac_ctrl_config(0, reg_cntl0, 9);
	}
}

void vdac_enable(bool on, unsigned int module_sel)
{
	if (!s_vdac_data) {
		pr_err("\n%s: s_vdac_data NULL\n", __func__);
		return;
	}

	pr_info("\n%s: %d, module_sel:0x%x, private_flag:0x%x\n",
		__func__, on, module_sel, pri_flag);

	mutex_lock(&vdac_mutex);
	switch (module_sel) {
	case VDAC_MODULE_AVOUT_ATV: /* atv avout */
		if (on) {
			if (pri_flag & VDAC_MODULE_AVOUT_ATV) {
				pr_info("%s: avout_atv is already on\n",
					__func__);
				break;
			}
			pri_flag |= VDAC_MODULE_AVOUT_ATV;
			if (pri_flag & VDAC_MODULE_CVBS_OUT) {
				pr_info("%s: %d, bypass for cvbsout\n",
					__func__, on);
				break;
			}
			vdac_enable_avout_atv(1);
		} else {
			if (!(pri_flag & VDAC_MODULE_AVOUT_ATV)) {
				pr_info("%s: avout_atv is already off\n",
					__func__);
				break;
			}
			pri_flag &= ~VDAC_MODULE_AVOUT_ATV;
			if (pri_flag & VDAC_MODULE_CVBS_OUT) {
				if (vdac_debug_print) {
					pr_info("%s: %d, bypass for cvbsout\n",
						__func__, on);
				}
				break;
			}
			vdac_enable_avout_atv(0);
		}
		break;
	case VDAC_MODULE_DTV_DEMOD: /* dtv demod */
		if (on) {
			pri_flag |= VDAC_MODULE_DTV_DEMOD;
			vdac_enable_dtv_demod(1);
		} else {
			pri_flag &= ~VDAC_MODULE_DTV_DEMOD;
			if (pri_flag & VDAC_MODULE_MASK)
				break;
			vdac_enable_dtv_demod(0);
		}
		break;
	case VDAC_MODULE_AVOUT_AV: /* avin avout */
		if (on) {
			pri_flag |= VDAC_MODULE_AVOUT_AV;
			if (pri_flag & VDAC_MODULE_CVBS_OUT) {
				pr_info("%s: %d, bypass for cvbsout\n",
					__func__, on);
				break;
			}
			vdac_enable_avout_av(1);
		} else {
			pri_flag &= ~VDAC_MODULE_AVOUT_AV;
			if (pri_flag & VDAC_MODULE_CVBS_OUT) {
				if (vdac_debug_print) {
					pr_info("%s: %d, bypass for cvbsout\n",
						__func__, on);
				}
				break;
			}
			if (pri_flag & VDAC_MODULE_MASK)
				break;
			vdac_enable_avout_av(0);
		}
		break;
	case VDAC_MODULE_CVBS_OUT:
		if (on) {
			pri_flag |= VDAC_MODULE_CVBS_OUT;
			vdac_enable_cvbs_out(1);
		} else {
			pri_flag &= ~VDAC_MODULE_CVBS_OUT;
			if ((pri_flag & VDAC_MODULE_MASK) == 0) {
				vdac_enable_cvbs_out(0);
				break;
			}

			if (pri_flag & VDAC_MODULE_AVOUT_ATV)
				vdac_enable_avout_atv(1);
			else if (pri_flag & VDAC_MODULE_AVOUT_AV)
				vdac_enable_avout_av(1);
			else if (pri_flag & VDAC_MODULE_DTV_DEMOD)
				vdac_enable_dtv_demod(1);
		}
		break;
	case VDAC_MODULE_AUDIO_OUT: /* audio demod */
		if (on) {
			pri_flag |= VDAC_MODULE_AUDIO_OUT;
			vdac_enable_audio_out(1);
		} else {
			pri_flag &= ~VDAC_MODULE_AUDIO_OUT;
			if (pri_flag & VDAC_MODULE_MASK)
				break;
			vdac_enable_audio_out(0);
		}
		break;
	default:
		pr_err("%s:module_sel: 0x%x wrong module index !! "
					, __func__, module_sel);
		break;
	}

	if (vdac_debug_print) {
		pr_info("private_flag:             0x%02x\n"
			"reg_cntl0:                0x%02x=0x%08x\n"
			"reg_cntl1:                0x%02x=0x%08x\n",
			pri_flag,
			s_vdac_data->reg_cntl0,
			vdac_hiu_reg_read(s_vdac_data->reg_cntl0),
			s_vdac_data->reg_cntl1,
			vdac_hiu_reg_read(s_vdac_data->reg_cntl1));
	}

	mutex_unlock(&vdac_mutex);
}
EXPORT_SYMBOL(vdac_enable);

void vdac_set_ctrl0_ctrl1(unsigned int ctrl0, unsigned int ctrl1)
{
	unsigned int reg_cntl0;
	unsigned int reg_cntl1;

	if (!s_vdac_data) {
		pr_err("\n%s: s_vdac_data NULL\n", __func__);
		return;
	}

	reg_cntl0 = s_vdac_data->reg_cntl0;
	reg_cntl1 = s_vdac_data->reg_cntl1;

	vdac_hiu_reg_write(reg_cntl0, ctrl0);
	vdac_hiu_reg_write(reg_cntl1, ctrl1);
	if (vdac_debug_print) {
		pr_info("vdac: set reg 0x%02x=0x%08x, readback=0x%08x\n",
			reg_cntl0, ctrl0, vdac_hiu_reg_read(reg_cntl0));
		pr_info("vdac: set reg 0x%02x=0x%08x, readback=0x%08x\n",
			reg_cntl1, ctrl1, vdac_hiu_reg_read(reg_cntl1));
	}
}

int vdac_enable_check_dtv(void)
{
	return (pri_flag & VDAC_MODULE_DTV_DEMOD) ? 1 : 0;
}

int vdac_enable_check_cvbs(void)
{
	return (pri_flag & VDAC_MODULE_CVBS_OUT) ? 1 : 0;
}

/* ********************************************************* */
static ssize_t vdac_info_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	ssize_t len = 0;

	if (!s_vdac_data)
		return sprintf(buf, "vdac data is NULL\n");

	len = sprintf(buf, "vdac driver info:\n");
	len += sprintf(buf + len,
		"chip_type:                %s(%d)\n"
		"private_flag:             0x%02x\n"
		"reg_cntl0:                0x%02x=0x%08x\n"
		"reg_cntl1:                0x%02x=0x%08x\n"
		"debug_print:              %d\n",
		s_vdac_data->name, s_vdac_data->cpu_id, pri_flag,
		s_vdac_data->reg_cntl0,
		vdac_hiu_reg_read(s_vdac_data->reg_cntl0),
		s_vdac_data->reg_cntl1,
		vdac_hiu_reg_read(s_vdac_data->reg_cntl1),
		vdac_debug_print);

	return len;
}

static const char *vdac_debug_usage_str = {
"Usage:\n"
"    cat /sys/class/amvdac/info ; print vdac information\n"
"    echo flag > /sys/class/amvdac/debug ; vdac_private_flag config\n"
"    echo print <val_dec> > /sys/class/amvdac/debug ; vdac_debug_print config\n"
};

static ssize_t vdac_debug_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", vdac_debug_usage_str);
}

static void vdac_parse_param(char *buf_orig, char **parm)
{
	char *ps, *token;
	char delim1[3] = " ";
	char delim2[2] = "\n";
	unsigned int n = 0;

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

static ssize_t vdac_debug_store(struct class *class,
		struct class_attribute *attr, const char *buff, size_t count)
{
	char *buf_orig;
	char *parm[3] = {NULL};

	if (!s_vdac_data) {
		pr_info("vdac data is null\n");
		return count;
	}

	buf_orig = kstrdup(buff, GFP_KERNEL);
	vdac_parse_param(buf_orig, (char **)&parm);
	if (parm[0] == NULL)
		return count;

	if (!strcmp(parm[0], "flag")) {
		pr_info("vdac_private_flag: 0x%x\n", pri_flag);
	} else if (!strcmp(parm[0], "print")) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 10, &vdac_debug_print) < 0)
				goto vdac_store_err;
		}
		pr_info("vdac_debug_print:%d\n", vdac_debug_print);
	} else {
		pr_info("invalid cmd\n");
	}

	kfree(buf_orig);
	return count;

vdac_store_err:
	kfree(buf_orig);
	return -EINVAL;
}

static struct class_attribute vdac_class_attrs[] = {
	__ATTR(info, 0644, vdac_info_show, NULL),
	__ATTR(debug, 0644, vdac_debug_show, vdac_debug_store),
};

static int vdac_create_class(struct amvdac_dev_s *devp)
{
	int i;

	if (IS_ERR_OR_NULL(devp->clsp)) {
		pr_err("vdac: %s debug class is null\n", __func__);
		return -1;
	}

	for (i = 0; i < ARRAY_SIZE(vdac_class_attrs); i++) {
		if (class_create_file(devp->clsp, &vdac_class_attrs[i])) {
			pr_err("vdac: create debug attribute %s failed\n",
				vdac_class_attrs[i].attr.name);
		}
	}

	return 0;
}

static int vdac_remove_class(struct amvdac_dev_s *devp)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(vdac_class_attrs); i++)
		class_remove_file(devp->clsp, &vdac_class_attrs[i]);

	class_destroy(devp->clsp);
	devp->clsp = NULL;

	return 0;
}
/* ********************************************************* */

static int amvdac_open(struct inode *inode, struct file *file)
{
	struct amvdac_dev_s *devp;
	/* Get the per-device structure that contains this cdev */
	devp = container_of(inode->i_cdev, struct amvdac_dev_s, cdev);
	file->private_data = devp;
	return 0;
}

static int amvdac_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

static const struct file_operations amvdac_fops = {
	.owner   = THIS_MODULE,
	.open    = amvdac_open,
	.release = amvdac_release,
};
/* ********************************************************* */

static int aml_vdac_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct amvdac_dev_s *devp = &amvdac_dev;

	memset(devp, 0, (sizeof(struct amvdac_dev_s)));

	s_vdac_data = aml_vdac_config_probe(pdev);
	if (!s_vdac_data) {
		pr_err("%s: config probe failed\n", __func__);
		return ret;
	}

	mutex_init(&vdac_mutex);

	ret = alloc_chrdev_region(&devp->devno, 0, 1, AMVDAC_NAME);
	if (ret < 0)
		goto fail_alloc_region;

	devp->clsp = class_create(THIS_MODULE, AMVDAC_CLASS_NAME);
	if (IS_ERR(devp->clsp)) {
		ret = PTR_ERR(devp->clsp);
		goto fail_create_class;
	}

	cdev_init(&devp->cdev, &amvdac_fops);
	devp->cdev.owner = THIS_MODULE;
	ret = cdev_add(&devp->cdev, devp->devno, 1);
	if (ret)
		goto fail_add_cdev;

	devp->dev = device_create(devp->clsp, NULL, devp->devno,
			NULL, AMVDAC_NAME);
	if (IS_ERR(devp->dev)) {
		ret = PTR_ERR(devp->dev);
		goto fail_create_device;
	}

	vdac_create_class(devp);

	pr_info("%s: ok\n", __func__);

	return ret;

fail_create_device:
	pr_err("%s: device create error\n", __func__);
	cdev_del(&devp->cdev);
fail_add_cdev:
	pr_err("%s: add device error\n", __func__);
fail_create_class:
	pr_err("%s: class create error\n", __func__);
	class_destroy(devp->clsp);
	unregister_chrdev_region(devp->devno, 1);
fail_alloc_region:
	pr_err("%s:  alloc error\n", __func__);

	return ret;
}

static int __exit aml_vdac_remove(struct platform_device *pdev)
{
	struct amvdac_dev_s *devp = &amvdac_dev;

	vdac_remove_class(devp);
	device_destroy(devp->clsp, devp->devno);
	cdev_del(&devp->cdev);
	class_destroy(devp->clsp);
	unregister_chrdev_region(devp->devno, 1);

	mutex_destroy(&vdac_mutex);

	pr_info("%s: amvdac_exit.\n", __func__);

	return 0;
}

#ifdef CONFIG_PM
static int amvdac_drv_suspend(struct platform_device *pdev,
		pm_message_t state)
{
	if (s_vdac_data->cpu_id == VDAC_CPU_TXL ||
		s_vdac_data->cpu_id == VDAC_CPU_TXLX)
		vdac_hiu_reg_write(s_vdac_data->reg_cntl0, 0);
	else if (s_vdac_data->cpu_id == VDAC_CPU_TL1 ||
		s_vdac_data->cpu_id == VDAC_CPU_TM2)
		vdac_ctrl_config(0, s_vdac_data->reg_cntl1, 7);
	pr_info("%s: suspend module\n", __func__);
	return 0;
}

static int amvdac_drv_resume(struct platform_device *pdev)
{
	/*0xbc[7] for bandgap enable: 0:enable,1:disable*/
	if (s_vdac_data->cpu_id == VDAC_CPU_TL1 ||
		s_vdac_data->cpu_id == VDAC_CPU_TM2) {
		vdac_ctrl_config(1, s_vdac_data->reg_cntl1, 7);
	}
	pr_info("%s: resume module\n", __func__);
	return 0;
}
#endif

static void amvdac_drv_shutdown(struct platform_device *pdev)
{
	unsigned int cntl0, cntl1;

	pr_info("%s: shutdown module, private_flag:0x%x\n",
		__func__, pri_flag);
	cntl0 = 0x0;
	if (s_vdac_data->cpu_id == VDAC_CPU_TXL ||
		s_vdac_data->cpu_id == VDAC_CPU_TXLX)
		cntl1 = 0x0;
	else if (s_vdac_data->cpu_id >= VDAC_CPU_TL1)
		cntl1 = 0x80;
	else
		cntl1 = 0x8;
	vdac_set_ctrl0_ctrl1(cntl0, cntl1);
}

static struct platform_driver aml_vdac_driver = {
	.driver = {
		.name = "aml_vdac",
		.owner = THIS_MODULE,
		.of_match_table = meson_vdac_dt_match,
	},
	.probe = aml_vdac_probe,
	.remove = __exit_p(aml_vdac_remove),
#ifdef CONFIG_PM
	.suspend    = amvdac_drv_suspend,
	.resume     = amvdac_drv_resume,
#endif
	.shutdown   = amvdac_drv_shutdown,
};

static int __init aml_vdac_init(void)
{
	s_vdac_data = NULL;

	if (platform_driver_register(&aml_vdac_driver)) {
		pr_err("%s: failed to register vdac driver module\n", __func__);
		return -ENODEV;
	}

	return 0;
}

static void __exit aml_vdac_exit(void)
{
	platform_driver_unregister(&aml_vdac_driver);
}

postcore_initcall(aml_vdac_init);
module_exit(aml_vdac_exit);

MODULE_DESCRIPTION("AMLOGIC vdac driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Frank Zhao <frank.zhao@amlogic.com>");
