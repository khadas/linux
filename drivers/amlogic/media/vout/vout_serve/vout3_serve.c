// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

 #define pr_fmt(fmt) "vout: " fmt

/* Linux Headers */
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/ctype.h>
#include <linux/of.h>
#include <linux/major.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>

/* Amlogic Headers */
#include <linux/amlogic/media/vout/vout_notify.h>

/* Local Headers */
#include "vout_func.h"

#include <linux/amlogic/gki_module.h>

#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
#include <linux/amlogic/pm.h>
static struct early_suspend early_suspend;
static int early_suspend_flag;
#endif

#define VOUT_CDEV_NAME  "display3"
#define VOUT_CLASS_NAME "display3"
#define MAX_NUMBER_PARA 10

#define VMODE_NAME_LEN_MAX    64
static struct class *vout3_class;
static DEFINE_MUTEX(vout3_serve_mutex);
static char vout3_mode[VMODE_NAME_LEN_MAX];
static char local_name[VMODE_NAME_LEN_MAX] = {0};
static char vout3_mode_uboot[VMODE_NAME_LEN_MAX] = "null";
static unsigned int vout3_init_vmode = VMODE_INIT_NULL;
static int uboot_display;
static unsigned int bist_mode3;

static struct vout_cdev_s *vout3_cdev;
static struct device *vout3_dev;

/* **********************************************************
 * null display support
 * **********************************************************
 */
static int nulldisp_index = VMODE_NULL_DISP_MAX;
static struct vinfo_s nulldisp_vinfo[] = {
	{
		.name              = "null",
		.mode              = VMODE_NULL,
		.frac              = 0,
		.width             = 1920,
		.height            = 1080,
		.field_height      = 1080,
		.aspect_ratio_num  = 16,
		.aspect_ratio_den  = 9,
		.sync_duration_num = 60,
		.sync_duration_den = 1,
		.video_clk         = 148500000,
		.htotal            = 2200,
		.vtotal            = 1125,
		.fr_adj_type       = VOUT_FR_ADJ_NONE,
		.viu_color_fmt     = COLOR_FMT_RGB444,
		.viu_mux           = ((3 << 4) | VIU_MUX_MAX),
		.vout_device       = NULL,
	},
	{
		.name              = "invalid",
		.mode              = VMODE_INVALID,
		.frac              = 0,
		.width             = 1920,
		.height            = 1080,
		.field_height      = 1080,
		.aspect_ratio_num  = 16,
		.aspect_ratio_den  = 9,
		.sync_duration_num = 60,
		.sync_duration_den = 1,
		.video_clk         = 148500000,
		.htotal            = 2200,
		.vtotal            = 1125,
		.fr_adj_type       = VOUT_FR_ADJ_NONE,
		.viu_color_fmt     = COLOR_FMT_RGB444,
		.viu_mux           = ((3 << 4) | VIU_MUX_MAX),
		.vout_device       = NULL,
	}
};

static struct vinfo_s *nulldisp_get_current_info(void *data)
{
	if (nulldisp_index >= ARRAY_SIZE(nulldisp_vinfo))
		return NULL;

	return &nulldisp_vinfo[nulldisp_index];
}

static int nulldisp_set_current_vmode(enum vmode_e mode, void *data)
{
	return 0;
}

static enum vmode_e nulldisp_validate_vmode(char *name, unsigned int frac,
					    void *data)
{
	enum vmode_e vmode = VMODE_MAX;
	int i;

	if (frac)
		return VMODE_MAX;

	for (i = 0; i < ARRAY_SIZE(nulldisp_vinfo); i++) {
		if (strcmp(nulldisp_vinfo[i].name, name) == 0) {
			vmode = nulldisp_vinfo[i].mode;
			nulldisp_index = i;
			break;
		}
	}

	return vmode;
}

static int nulldisp_vmode_is_supported(enum vmode_e mode, void *data)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(nulldisp_vinfo); i++) {
		if (nulldisp_vinfo[i].mode == (mode & VMODE_MODE_BIT_MASK))
			return true;
	}
	return false;
}

static int nulldisp_disable(enum vmode_e cur_vmod, void *data)
{
	return 0;
}

static int nulldisp_vout_state;
static int nulldisp_vout_set_state(int bit, void *data)
{
	nulldisp_vout_state |= (1 << bit);
	return 0;
}

static int nulldisp_vout_clr_state(int bit, void *data)
{
	nulldisp_vout_state &= ~(1 << bit);
	return 0;
}

static int nulldisp_vout_get_state(void *data)
{
	return nulldisp_vout_state;
}

static struct vout_server_s nulldisp_vout3_server = {
	.name = "nulldisp_vout3_server",
	.op = {
		.get_vinfo          = nulldisp_get_current_info,
		.set_vmode          = nulldisp_set_current_vmode,
		.validate_vmode     = nulldisp_validate_vmode,
		.vmode_is_supported = nulldisp_vmode_is_supported,
		.disable            = nulldisp_disable,
		.set_state          = nulldisp_vout_set_state,
		.clr_state          = nulldisp_vout_clr_state,
		.get_state          = nulldisp_vout_get_state,
		.set_bist           = NULL,
	},
	.data = NULL,
};

/* ********************************************************** */

char *get_vout3_mode_internal(void)
{
	return vout3_mode;
}
EXPORT_SYMBOL(get_vout3_mode_internal);

#define MAX_UEVENT_LEN 64
static int vout3_set_uevent(unsigned int vout_event, int val)
{
	char env[MAX_UEVENT_LEN];
	char *envp[2];
	int ret;

	if (vout_event != VOUT_EVENT_MODE_CHANGE)
		return 0;

	if (!vout3_dev) {
		VOUTERR("no vout3_dev\n");
		return -1;
	}

	memset(env, 0, sizeof(env));
	envp[0] = env;
	envp[1] = NULL;
	snprintf(env, MAX_UEVENT_LEN, "vout3_setmode=%d", val);

	ret = kobject_uevent_env(&vout3_dev->kobj, KOBJ_CHANGE, envp);

	return ret;
}

static int set_vout3_mode(char *name)
{
	enum vmode_e mode;
	unsigned int frac;
	int ret = 0;

	vout_trim_string(name);
	VOUTPR("vout3: vmode set to %s\n", name);

	if ((vout3_check_same_vmodeattr(name) &&
	    (strcmp(name, vout3_mode) == 0))) {
		VOUTPR("vout3: don't set the same mode as current\n");
		return -1;
	}
	memset(local_name, 0, sizeof(local_name));
	snprintf(local_name, VMODE_NAME_LEN_MAX, "%s", name);
	frac = vout_parse_vout_name(local_name);
	if (vout_debug_print) {
		VOUTPR("vout3: %s: local_name=%s, frac=%d\n",
			__func__, local_name, frac);
	}

	mode = validate_vmode3(local_name, frac);
	if (mode == VMODE_MAX) {
		VOUTERR("vout3: no matched vout3 mode\n");
		return -1;
	}

	vout3_set_uevent(VOUT_EVENT_MODE_CHANGE, 1);

	vout3_notifier_call_chain(VOUT_EVENT_MODE_CHANGE_PRE, &mode);
	ret = set_current_vmode3(mode);
	if (ret) {
		VOUTERR("vout3: new mode %s set error\n", name);
	} else {
		memset(vout3_mode, 0, sizeof(vout3_mode));
		snprintf(vout3_mode, VMODE_NAME_LEN_MAX, "%s", name);
		VOUTPR("vout3: new mode %s set ok\n", name);
	}
	vout3_notifier_call_chain(VOUT_EVENT_MODE_CHANGE, &mode);

	vout3_set_uevent(VOUT_EVENT_MODE_CHANGE, 0);

	return ret;
}

static int set_vout3_init_mode(void)
{
	enum vmode_e vmode;
	unsigned int frac;
	int ret = 0;

	memset(local_name, 0, sizeof(local_name));
	snprintf(local_name, VMODE_NAME_LEN_MAX, "%s", vout3_mode_uboot);
	frac = vout_parse_vout_name(local_name);

	vout3_init_vmode = validate_vmode3(local_name, frac);
	if (vout3_init_vmode >= VMODE_MAX) {
		VOUTERR("vout3: no matched vout3_init mode %s, force to invalid\n",
			vout3_mode_uboot);
		nulldisp_index = 1;
		vout3_init_vmode = nulldisp_vinfo[nulldisp_index].mode;
		snprintf(local_name, VMODE_NAME_LEN_MAX, "%s",
			 nulldisp_vinfo[nulldisp_index].name);
	} else { /* recover vout_mode_uboot */
		snprintf(local_name, VMODE_NAME_LEN_MAX, "%s",
			 vout3_mode_uboot);
	}
	if (uboot_display)
		vmode = vout3_init_vmode | VMODE_INIT_BIT_MASK;
	else
		vmode = vout3_init_vmode;

	ret = set_current_vmode3(vmode);
	if (ret) {
		VOUTERR("vout3: init mode %s set error\n", local_name);
	} else {
		memset(vout3_mode, 0, sizeof(vout3_mode));
		snprintf(vout3_mode, VMODE_NAME_LEN_MAX, local_name);
		VOUTPR("vout3: init mode %s set ok\n", local_name);
	}

	return ret;
}

static ssize_t vout3_mode_show(struct class *class,
			       struct class_attribute *attr, char *buf)
{
	int ret = 0;

	ret = snprintf(buf, VMODE_NAME_LEN_MAX, "%s\n", vout3_mode);

	return ret;
}

static ssize_t vout3_mode_store(struct class *class,
				struct class_attribute *attr,
				const char *buf, size_t count)
{
	char mode[64];

	mutex_lock(&vout3_serve_mutex);
	snprintf(mode, VMODE_NAME_LEN_MAX, "%s", buf);
	set_vout3_mode(mode);
	mutex_unlock(&vout3_serve_mutex);
	return count;
}

static ssize_t vout3_fr_policy_show(struct class *class,
				    struct class_attribute *attr, char *buf)
{
	int policy;
	int ret = 0;

	policy = get_vframe3_rate_policy();
	ret = sprintf(buf, "%d\n", policy);

	return ret;
}

static ssize_t vout3_fr_policy_store(struct class *class,
				     struct class_attribute *attr,
				     const char *buf, size_t count)
{
	int policy;
	int ret = 0;

	mutex_lock(&vout3_serve_mutex);
	ret = kstrtoint(buf, 10, &policy);
	if (ret) {
		pr_info("%s: invalid data\n", __func__);
		mutex_unlock(&vout3_serve_mutex);
		return -EINVAL;
	}
	ret = set_vframe3_rate_policy(policy);
	if (ret)
		pr_info("%s: %d failed\n", __func__, policy);
	mutex_unlock(&vout3_serve_mutex);

	return count;
}

static ssize_t vout3_fr_hint_show(struct class *class,
				  struct class_attribute *attr, char *buf)
{
	int fr_hint;
	int ret = 0;

	fr_hint = get_vframe3_rate_hint();
	ret = sprintf(buf, "%d\n", fr_hint);

	return ret;
}

static ssize_t vout3_fr_hint_store(struct class *class,
				   struct class_attribute *attr,
				   const char *buf, size_t count)
{
	int fr_hint;
	int ret = 0;

	mutex_lock(&vout3_serve_mutex);
	ret = kstrtoint(buf, 10, &fr_hint);
	if (ret) {
		mutex_unlock(&vout3_serve_mutex);
		return -EINVAL;
	}
	set_vframe3_rate_hint(fr_hint);
	mutex_unlock(&vout3_serve_mutex);

	return count;
}

static ssize_t vout3_fr_range_show(struct class *class,
				   struct class_attribute *attr, char *buf)
{
	const struct vinfo_s *info = NULL;

	info = get_current_vinfo3();
	if (!info)
		return sprintf(buf, "current vinfo3 is null\n");

	return sprintf(buf, "%d %d\n", info->vfreq_min, info->vfreq_max);
}

static ssize_t vout3_bist_show(struct class *class,
			       struct class_attribute *attr, char *buf)
{
	int ret = 0;

	ret = sprintf(buf, "%d\n", bist_mode3);

	return ret;
}

static ssize_t vout3_bist_store(struct class *class,
				struct class_attribute *attr,
				const char *buf, size_t count)
{
	int ret = 0;

	mutex_lock(&vout3_serve_mutex);

	ret = kstrtouint(buf, 10, &bist_mode3);
	if (ret) {
		pr_info("%s: invalid data\n", __func__);
		mutex_unlock(&vout3_serve_mutex);
		return -EINVAL;
	}
	set_vout3_bist(bist_mode3);

	mutex_unlock(&vout3_serve_mutex);

	return count;
}

static ssize_t vout3_vinfo_show(struct class *class,
				struct class_attribute *attr, char *buf)
{
	const struct vinfo_s *info = NULL;
	ssize_t len = 0;
	unsigned int i, j;
	unsigned char val;

	info = get_current_vinfo3();
	if (!info)
		return sprintf(buf, "current vinfo3 is null\n");

	len = sprintf(buf, "current vinfo3:\n"
		"    name:                  %s\n"
		"    mode:                  %d\n"
		"    frac:                  %d\n"
		"    width:                 %d\n"
		"    height:                %d\n"
		"    field_height:          %d\n"
		"    aspect_ratio_num:      %d\n"
		"    aspect_ratio_den:      %d\n"
		"    screen_real_width:     %d\n"
		"    screen_real_height:    %d\n"
		"    sync_duration_num:     %d\n"
		"    sync_duration_den:     %d\n"
		"    std_duration:          %d\n"
		"    vfreq_max:             %d\n"
		"    vfreq_min:             %d\n"
		"    htotal:                %d\n"
		"    vtotal:                %d\n"
		"    video_clk:             %d\n"
		"    fr_adj_type:           %d\n"
		"    viu_color_fmt:         %d\n"
		"    viu_mux:               0x%x\n\n",
		info->name, info->mode, info->frac,
		info->width, info->height, info->field_height,
		info->aspect_ratio_num, info->aspect_ratio_den,
		info->screen_real_width, info->screen_real_height,
		info->sync_duration_num, info->sync_duration_den,
		info->std_duration, info->vfreq_max, info->vfreq_min,
		info->htotal, info->vtotal, info->video_clk,
		info->fr_adj_type, info->viu_color_fmt, info->viu_mux);
	len += sprintf(buf + len, "master_display_info:\n"
		"    present_flag          %d\n"
		"    features              0x%x\n"
		"    primaries             0x%x, 0x%x\n"
		"                          0x%x, 0x%x\n"
		"                          0x%x, 0x%x\n"
		"    white_point           0x%x, 0x%x\n"
		"    luminance             %d, %d\n\n",
		info->master_display_info.present_flag,
		info->master_display_info.features,
		info->master_display_info.primaries[0][0],
		info->master_display_info.primaries[0][1],
		info->master_display_info.primaries[1][0],
		info->master_display_info.primaries[1][1],
		info->master_display_info.primaries[2][0],
		info->master_display_info.primaries[2][1],
		info->master_display_info.white_point[0],
		info->master_display_info.white_point[1],
		info->master_display_info.luminance[0],
		info->master_display_info.luminance[1]);
	len += sprintf(buf + len, "hdr_static_info:\n"
		"    hdr_support           %d\n"
		"    lumi_max              %d\n"
		"    lumi_avg              %d\n"
		"    lumi_min              %d\n",
		info->hdr_info.hdr_support,
		info->hdr_info.lumi_max,
		info->hdr_info.lumi_avg,
		info->hdr_info.lumi_min);
	len += sprintf(buf + len, "hdr_dynamic_info:");
	for (i = 0; i < 4; i++) {
		len += sprintf(buf + len,
			"\n    metadata_version:  %x\n"
			"    support_flags:     %x\n",
			info->hdr_info.dynamic_info[i].type,
			info->hdr_info.dynamic_info[i].support_flags);
		len += sprintf(buf + len, "    optional_fields:  ");
		for (j = 0; j < info->hdr_info.dynamic_info[i].of_len; j++) {
			val = info->hdr_info.dynamic_info[i].optional_fields[j];
			len += sprintf(buf + len, " %x", val);
		}
	}
	len += sprintf(buf + len, "\n");
	len += sprintf(buf + len, "hdr10+:\n");
	len += sprintf(buf + len, "    ieeeoui: %x\n",
		       info->hdr_info.hdr10plus_info.ieeeoui);
	len += sprintf(buf + len, "    application_version: %x\n",
		       info->hdr_info.hdr10plus_info.application_version);
	return len;
}

static ssize_t vout3_cap_show(struct class *class,
			      struct class_attribute *attr, char *buf)
{
	int ret;

	ret = get_vout3_disp_cap(buf);
	if (!ret)
		return sprintf(buf, "null\n");

	return ret;
}

static struct class_attribute vout3_class_attrs[] = {
	__ATTR(mode,      0644, vout3_mode_show, vout3_mode_store),
	__ATTR(fr_policy, 0644,
	       vout3_fr_policy_show, vout3_fr_policy_store),
	__ATTR(fr_hint,   0644, vout3_fr_hint_show, vout3_fr_hint_store),
	__ATTR(fr_range,  0644, vout3_fr_range_show, NULL),
	__ATTR(bist,      0644, vout3_bist_show, vout3_bist_store),
	__ATTR(vinfo,     0444, vout3_vinfo_show, NULL),
	__ATTR(cap,       0644, vout3_cap_show, NULL)
};

static int vout3_attr_create(void)
{
	int i;
	int ret = 0;

	/* create vout class */
	vout3_class = class_create(THIS_MODULE, VOUT_CLASS_NAME);
	if (IS_ERR(vout3_class)) {
		VOUTERR("vout3: create vout3 class fail\n");
		return -1;
	}

	/* create vout class attr files */
	for (i = 0; i < ARRAY_SIZE(vout3_class_attrs); i++) {
		if (class_create_file(vout3_class, &vout3_class_attrs[i])) {
			VOUTERR("vout3: create vout3 attribute %s fail\n",
				vout3_class_attrs[i].attr.name);
		}
	}

	/*VOUTPR("vout3: create vout3 attribute OK\n");*/

	return ret;
}

static int vout3_attr_remove(void)
{
	int i;

	if (!vout3_class)
		return 0;

	for (i = 0; i < ARRAY_SIZE(vout3_class_attrs); i++)
		class_remove_file(vout3_class, &vout3_class_attrs[i]);

	class_destroy(vout3_class);
	vout3_class = NULL;

	return 0;
}

/* ************************************************************* */
/* vout3 ioctl                                                    */
/* ************************************************************* */
static int vout3_io_open(struct inode *inode, struct file *file)
{
	struct vout_cdev_s *vcdev;

	/*VOUTPR("%s\n", __func__);*/
	vcdev = container_of(inode->i_cdev, struct vout_cdev_s, cdev);
	file->private_data = vcdev;
	return 0;
}

static int vout3_io_release(struct inode *inode, struct file *file)
{
	/*VOUTPR("%s\n", __func__);*/
	file->private_data = NULL;
	return 0;
}

static long vout3_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	void __user *argp;
	int mcd_nr;
	struct vinfo_s *info = NULL;
	struct vinfo_base_s baseinfo;

	mcd_nr = _IOC_NR(cmd);
	/*VOUTPR("%s: cmd_dir = 0x%x, cmd_nr = 0x%x\n", __func__, _IOC_DIR(cmd), mcd_nr);*/

	argp = (void __user *)arg;
	switch (mcd_nr) {
	case VOUT_IOC_NR_GET_VINFO:
		info = get_current_vinfo3();
		if (!info) {
			ret = -EFAULT;
		} else if (info->mode == VMODE_INIT_NULL) {
			ret = -EFAULT;
		} else {
			baseinfo.mode = info->mode;
			baseinfo.width = info->width;
			baseinfo.height = info->height;
			baseinfo.field_height = info->field_height;
			baseinfo.aspect_ratio_num = info->aspect_ratio_num;
			baseinfo.aspect_ratio_den = info->aspect_ratio_den;
			baseinfo.sync_duration_num = info->sync_duration_num;
			baseinfo.sync_duration_den = info->sync_duration_den;
			baseinfo.screen_real_width = info->screen_real_width;
			baseinfo.screen_real_height = info->screen_real_height;
			baseinfo.video_clk = info->video_clk;
			baseinfo.viu_color_fmt = info->viu_color_fmt;
			if (copy_to_user(argp, &baseinfo,
					 sizeof(struct vinfo_base_s)))
				ret = -EFAULT;
		}
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static long vout3_compat_ioctl(struct file *file, unsigned int cmd,
			       unsigned long arg)
{
	unsigned long ret;

	arg = (unsigned long)compat_ptr(arg);
	ret = vout3_ioctl(file, cmd, arg);
	return ret;
}
#endif

static const struct file_operations vout3_fops = {
	.owner          = THIS_MODULE,
	.open           = vout3_io_open,
	.release        = vout3_io_release,
	.unlocked_ioctl = vout3_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl   = vout3_compat_ioctl,
#endif
};

static int vout3_fops_create(void)
{
	int ret = 0;

	vout3_cdev = kmalloc(sizeof(*vout3_cdev), GFP_KERNEL);
	if (!vout3_cdev) {
		VOUTERR("vout3: failed to allocate vout3_cdev\n");
		return -1;
	}

	ret = alloc_chrdev_region(&vout3_cdev->devno, 0, 1, VOUT_CDEV_NAME);
	if (ret < 0) {
		VOUTERR("vout3: failed to alloc vout3 devno\n");
		goto vout3_fops_err1;
	}

	cdev_init(&vout3_cdev->cdev, &vout3_fops);
	vout3_cdev->cdev.owner = THIS_MODULE;
	ret = cdev_add(&vout3_cdev->cdev, vout3_cdev->devno, 1);
	if (ret) {
		VOUTERR("vout3: failed to add vout3 cdev\n");
		goto vout3_fops_err2;
	}

	vout3_cdev->dev = device_create(vout3_class, NULL, vout3_cdev->devno,
					NULL, VOUT_CDEV_NAME);
	if (IS_ERR(vout3_cdev->dev)) {
		ret = PTR_ERR(vout3_cdev->dev);
		VOUTERR("vout3: failed to create vout3 device: %d\n", ret);
		goto vout3_fops_err3;
	}

	/*VOUTPR("vout3: %s OK\n", __func__);*/
	return 0;

vout3_fops_err3:
	cdev_del(&vout3_cdev->cdev);
vout3_fops_err2:
	unregister_chrdev_region(vout3_cdev->devno, 1);
vout3_fops_err1:
	kfree(vout3_cdev);
	vout3_cdev = NULL;
	return -1;
}

static void vout3_fops_remove(void)
{
	cdev_del(&vout3_cdev->cdev);
	unregister_chrdev_region(vout3_cdev->devno, 1);
	kfree(vout3_cdev);
	vout3_cdev = NULL;
}

/* ************************************************************* */

#ifdef CONFIG_PM
static int aml_vout3_suspend(struct platform_device *pdev, pm_message_t state)
{
#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND

	if (early_suspend_flag)
		return 0;

#endif
	vout3_suspend();
	return 0;
}

static int aml_vout3_resume(struct platform_device *pdev)
{
#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND

	if (early_suspend_flag)
		return 0;

#endif
	vout3_resume();
	return 0;
}
#endif

#ifdef CONFIG_HIBERNATION
static int aml_vout3_freeze(struct device *dev)
{
	return 0;
}

static int aml_vout3_thaw(struct device *dev)
{
	return 0;
}

static int aml_vout3_restore(struct device *dev)
{
	enum vmode_e mode;

	mode = get_current_vmode3();
	vout3_notifier_call_chain(VOUT_EVENT_MODE_CHANGE, &mode);

	return 0;
}

static int aml_vout3_pm_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);

	return aml_vout3_suspend(pdev, PMSG_SUSPEND);
}

static int aml_vout3_pm_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);

	return aml_vout3_resume(pdev);
}
#endif

#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
static void aml_vout3_early_suspend(struct early_suspend *h)
{
	if (early_suspend_flag)
		return;

	vout3_suspend();
	early_suspend_flag = 1;
}

static void aml_vout3_late_resume(struct early_suspend *h)
{
	if (!early_suspend_flag)
		return;

	early_suspend_flag = 0;
	vout3_resume();
}
#endif

/*****************************************************************
 **
 **	vout driver interface
 **
 ******************************************************************/
static void aml_vout3_get_dt_info(struct platform_device *pdev)
{
	int ret;
	unsigned int para[2];

	ret = of_property_read_u32(pdev->dev.of_node, "fr_policy", &para[0]);
	if (!ret) {
		ret = set_vframe3_rate_policy(para[0]);
		if (ret)
			VOUTERR("vout3: init fr_policy %d failed\n", para[0]);
		else
			VOUTPR("vout3: fr_policy:%d\n", para[0]);
	}
}

static int aml_vout3_probe(struct platform_device *pdev)
{
	int ret = -1;

	vout3_dev = &pdev->dev;

	aml_vout3_get_dt_info(pdev);

#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
	early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
	early_suspend.suspend = aml_vout3_early_suspend;
	early_suspend.resume = aml_vout3_late_resume;
	register_early_suspend(&early_suspend);
#endif

	vout3_class = NULL;
	ret = vout3_attr_create();
	ret = vout3_fops_create();

	vout3_register_server(&nulldisp_vout3_server);

	set_vout3_init_mode();

	VOUTPR("vout3: %s OK\n", __func__);
	return ret;
}

static int aml_vout3_remove(struct platform_device *pdev)
{
#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
	unregister_early_suspend(&early_suspend);
#endif
	vout3_attr_remove();
	vout3_fops_remove();
	vout3_unregister_server(&nulldisp_vout3_server);

	return 0;
}

static void aml_vout3_shutdown(struct platform_device *pdev)
{
	VOUTPR("vout3: %s\n", __func__);
	vout3_shutdown();
}

static const struct of_device_id aml_vout3_dt_match[] = {
	{ .compatible = "amlogic, vout3",},
	{ },
};

#ifdef CONFIG_HIBERNATION
const struct dev_pm_ops vout3_pm = {
	.freeze		= aml_vout3_freeze,
	.thaw		= aml_vout3_thaw,
	.restore	= aml_vout3_restore,
	.suspend	= aml_vout3_pm_suspend,
	.resume		= aml_vout3_pm_resume,
};
#endif

static struct platform_driver vout3_driver = {
	.probe     = aml_vout3_probe,
	.remove    = aml_vout3_remove,
	.shutdown  = aml_vout3_shutdown,
#ifdef CONFIG_PM
	.suspend   = aml_vout3_suspend,
	.resume    = aml_vout3_resume,
#endif
	.driver = {
		.name = "vout3",
		.of_match_table = aml_vout3_dt_match,
#ifdef CONFIG_HIBERNATION
		.pm = &vout3_pm,
#endif
	},
};

int __init vout3_init_module(void)
{
	return platform_driver_register(&vout3_driver);
}

__exit void vout3_exit_module(void)
{
	platform_driver_unregister(&vout3_driver);
}

static int str2lower(char *str)
{
	while (*str != '\0') {
		*str = tolower(*str);
		str++;
	}
	return 0;
}

static void vout3_init_mode_parse(char *str)
{
	/* detect vout mode */
	if (strlen(str) <= 1) {
		VOUTERR("%s: %s\n", __func__, str);
		return;
	}

	/* detect uboot display */
	if (strncmp(str, "en", 2) == 0) { /* enable */
		uboot_display = 1;
		VOUTPR("vout3: %s: %d\n", str, uboot_display);
		return;
	}
	if (strncmp(str, "dis", 3) == 0) { /* disable */
		uboot_display = 0;
		VOUTPR("vout3: %s: %d\n", str, uboot_display);
		return;
	}
	if (strncmp(str, "frac", 4) == 0) { /* frac */
		if ((strlen(vout3_mode_uboot) + strlen(str))
		    < VMODE_NAME_LEN_MAX)
			strcat(vout3_mode_uboot, str);
		else
			VOUTERR("%s: str len out of support\n", __func__);
		VOUTPR("%s\n", str);
		return;
	}

	/*
	 * just save the vmode_name,
	 * convert to vmode when vout sever registered
	 */
	snprintf(vout3_mode_uboot, VMODE_NAME_LEN_MAX, "%s", str);
	vout_trim_string(vout3_mode_uboot);
	VOUTPR("vout3: %s\n", vout3_mode_uboot);
}

static int get_vout3_init_mode(char *str)
{
	char *ptr = str;
	char sep[2];
	char *option;
	int count = 3;
	char find = 0;

	if (!str)
		return -EINVAL;

	do {
		if (!isalpha(*ptr) && !isdigit(*ptr) &&
		    (*ptr != '_') && (*ptr != '-')) {
			find = 1;
			break;
		}
	} while (*++ptr != '\0');
	if (!find)
		return -EINVAL;

	sep[0] = *ptr;
	sep[1] = '\0';
	while ((count--) && (option = strsep(&str, sep))) {
		/* VOUTPR("vout3:%s\n", option); */
		str2lower(option);
		vout3_init_mode_parse(option);
	}

	return 0;
}
__setup("vout3=", get_vout3_init_mode);

//MODULE_AUTHOR("Platform-BJ <platform.bj@amlogic.com>");
//MODULE_DESCRIPTION("vout3 Server Module");
//MODULE_LICENSE("GPL");
