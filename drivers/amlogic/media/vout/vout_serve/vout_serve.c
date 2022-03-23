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

/* Amlogic Headers */
#include <linux/amlogic/media/vout/vout_notify.h>

/* Local Headers */
#include "vout_func.h"
#include "vout_reg.h"

#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
#include <linux/amlogic/pm.h>
static struct early_suspend early_suspend;
static int early_suspend_flag;
#endif

/* must be last includ file */
#include <linux/amlogic/gki_module.h>

#define VOUT_CDEV_NAME  "display"
#define VOUT_CLASS_NAME "display"
#define MAX_NUMBER_PARA 10

#define VMODE_NAME_LEN_MAX    64
static struct class *vout_class;
static DEFINE_MUTEX(vout_serve_mutex);
static char vout_mode_uboot[VMODE_NAME_LEN_MAX] = "null";
static char vout_mode[VMODE_NAME_LEN_MAX] __nosavedata;
static char local_name[VMODE_NAME_LEN_MAX] = {0};
static u32 vout_init_vmode = VMODE_INIT_NULL;
static int uboot_display;
static unsigned int bist_mode;

static struct vout_cdev_s *vout_cdev;
static struct device *vout_dev;
static unsigned int vsync_irq;
static unsigned int vs_meas_en;

static bool disable_modesysfs;
static bool enable_debugmode;

int vout_debug_print;

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
	},
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

static struct vout_server_s nulldisp_vout_server = {
	.name = "nulldisp_vout_server",
	.op = {
		.get_vinfo          = nulldisp_get_current_info,
		.set_vmode          = nulldisp_set_current_vmode,
		.validate_vmode     = nulldisp_validate_vmode,
		.vmode_is_supported = nulldisp_vmode_is_supported,
		.disable            = nulldisp_disable,
		.set_state          = nulldisp_vout_set_state,
		.clr_state          = nulldisp_vout_clr_state,
		.get_state          = nulldisp_vout_get_state,
		.get_disp_cap       = NULL,
		.set_bist           = NULL,
	},
	.data = NULL,
};

/* ********************************************************** */
static irqreturn_t vout_vsync_irq_handler(int irq, void *data)
{
	struct vinfo_s *vinfo = NULL;
	unsigned int fr;

	if (vs_meas_en == 0)
		return IRQ_HANDLED;

	vinfo = get_current_vinfo();
	if (!vinfo)
		return IRQ_HANDLED;
	switch (vinfo->mode) {
	case VMODE_HDMI:
	case VMODE_CVBS:
	case VMODE_LCD:
	case VMODE_DUMMY_ENCP:
	case VMODE_DUMMY_ENCI:
	case VMODE_DUMMY_ENCL:
		fr = vout_frame_rate_measure();
		vinfo->sync_duration_num = fr;
		vinfo->sync_duration_den = 1000;
		break;
	default:
		break;
	}

	return IRQ_HANDLED;
}

/* ********************************************************** */

char *get_vout_mode_internal(void)
{
	return vout_mode;
}
EXPORT_SYMBOL(get_vout_mode_internal);

char *get_vout_mode_uboot(void)
{
	return vout_mode_uboot;
}
EXPORT_SYMBOL(get_vout_mode_uboot);

int get_vout_mode_uboot_state(void)
{
	return uboot_display;
}
EXPORT_SYMBOL(get_vout_mode_uboot_state);

#define MAX_UEVENT_LEN 64

static int vout_set_uevent(unsigned int vout_event, int val)
{
	char env[MAX_UEVENT_LEN];
	char *envp[2];
	int ret;

	if (vout_event != VOUT_EVENT_MODE_CHANGE)
		return 0;

	if (!vout_dev) {
		VOUTERR("no vout_dev\n");
		return -1;
	}

	memset(env, 0, sizeof(env));
	envp[0] = env;
	envp[1] = NULL;
	snprintf(env, MAX_UEVENT_LEN, "vout_setmode=%d", val);

	ret = kobject_uevent_env(&vout_dev->kobj, KOBJ_CHANGE, envp);

	return ret;
}

int set_vout_mode_pre_process(enum vmode_e mode)
{
	vout_set_uevent(VOUT_EVENT_MODE_CHANGE, 1);
	vout_notifier_call_chain(VOUT_EVENT_MODE_CHANGE_PRE, &mode);
	return 0;
}
EXPORT_SYMBOL(set_vout_mode_pre_process);

int set_vout_mode_post_process(enum vmode_e mode)
{
	vout_notifier_call_chain(VOUT_EVENT_MODE_CHANGE, &mode);
	vout_set_uevent(VOUT_EVENT_MODE_CHANGE, 0);
	return 0;
}
EXPORT_SYMBOL(set_vout_mode_post_process);

int set_vout_mode_name(char *name)
{
	if (!name)
		return -EINVAL;

	memset(vout_mode, 0, sizeof(vout_mode));
	snprintf(vout_mode, VMODE_NAME_LEN_MAX, "%s", name);
	return 0;
}
EXPORT_SYMBOL(set_vout_mode_name);

int set_vout_mode(char *name)
{
	enum vmode_e mode;
	unsigned int frac;
	int ret = 0;

	vout_trim_string(name);
	VOUTPR("vmode set to %s\n", name);

	if ((vout_check_same_vmodeattr(name) &&
	    (strcmp(name, vout_mode) == 0))) {
		VOUTPR("don't set the same mode as current, exit\n");
		return -1;
	}

	memset(local_name, 0, sizeof(local_name));
	snprintf(local_name, VMODE_NAME_LEN_MAX, "%s", name);
	frac = vout_parse_vout_name(local_name);
	if (vout_debug_print) {
		VOUTPR("%s: local_name=%s, frac=%d\n",
			__func__, local_name, frac);
	}

	mode = validate_vmode(local_name, frac);
	if (mode == VMODE_MAX) {
		VOUTERR("no matched vout mode, exit\n");
		return -1;
	}

	vout_set_uevent(VOUT_EVENT_MODE_CHANGE, 1);

	vout_notifier_call_chain(VOUT_EVENT_MODE_CHANGE_PRE, &mode);
	ret = set_current_vmode(mode);
	if (ret) {
		VOUTERR("new mode %s set error\n", name);
	} else {
		memset(vout_mode, 0, sizeof(vout_mode));
		snprintf(vout_mode, VMODE_NAME_LEN_MAX, "%s", name);
		VOUTPR("new mode %s set ok\n", name);
	}
	vout_notifier_call_chain(VOUT_EVENT_MODE_CHANGE, &mode);

	vout_set_uevent(VOUT_EVENT_MODE_CHANGE, 0);

	return ret;
}

static int set_vout_init_mode(void)
{
	enum vmode_e vmode;
	unsigned int frac;
	int ret = 0;

	memset(local_name, 0, sizeof(local_name));
	snprintf(local_name, VMODE_NAME_LEN_MAX, "%s", vout_mode_uboot);
	frac = vout_parse_vout_name(local_name);

	vout_init_vmode = validate_vmode(local_name, frac);
	if (vout_init_vmode >= VMODE_MAX) {
		VOUTERR("no matched vout_init mode %s, force to invalid\n",
			vout_mode_uboot);
		nulldisp_index = 1;
		vout_init_vmode = nulldisp_vinfo[nulldisp_index].mode;
		snprintf(local_name, VMODE_NAME_LEN_MAX, "%s",
			 nulldisp_vinfo[nulldisp_index].name);
	} else { /* recover vout_mode_uboot */
		snprintf(local_name, VMODE_NAME_LEN_MAX, "%s", vout_mode_uboot);
	}

	if (uboot_display)
		vmode = vout_init_vmode | VMODE_INIT_BIT_MASK;
	else
		vmode = vout_init_vmode;

	ret = set_current_vmode(vmode);
	if (ret) {
		VOUTERR("init mode %s set error\n", local_name);
	} else {
		memset(vout_mode, 0, sizeof(vout_mode));
		snprintf(vout_mode, VMODE_NAME_LEN_MAX, "%s", local_name);
		VOUTPR("init mode %s set ok\n", local_name);
	}
	vout_notifier_call_chain(VOUT_EVENT_SYS_INIT, &vmode);

	return ret;
}

static ssize_t vout_mode_show(struct class *class,
			      struct class_attribute *attr, char *buf)
{
	int ret = 0;

	ret = snprintf(buf, VMODE_NAME_LEN_MAX, "%s\n", vout_mode);

	return ret;
}

static ssize_t vout_mode_store(struct class *class,
			       struct class_attribute *attr,
			       const char *buf, size_t count)
{
	char mode[VMODE_NAME_LEN_MAX];

	mutex_lock(&vout_serve_mutex);
	if (!disable_modesysfs || enable_debugmode) {
		snprintf(mode, VMODE_NAME_LEN_MAX, "%s", buf);
		set_vout_mode(mode);
	} else {
		VOUTPR("enable display/debug to set voutmode.\n");
	}
	mutex_unlock(&vout_serve_mutex);
	return count;
}

static ssize_t vout_fr_policy_show(struct class *class,
				   struct class_attribute *attr, char *buf)
{
	int policy;
	int ret = 0;

	policy = get_vframe_rate_policy();
	ret = sprintf(buf, "%d\n", policy);

	return ret;
}

static ssize_t vout_fr_policy_store(struct class *class,
				    struct class_attribute *attr,
				    const char *buf, size_t count)
{
	int policy;
	int ret = 0;

	mutex_lock(&vout_serve_mutex);
	ret = kstrtoint(buf, 10, &policy);
	if (ret) {
		pr_info("%s: invalid data\n", __func__);
		mutex_unlock(&vout_serve_mutex);
		return -EINVAL;
	}
	ret = set_vframe_rate_policy(policy);
	if (ret)
		pr_info("%s: %d failed\n", __func__, policy);
	mutex_unlock(&vout_serve_mutex);

	return count;
}

static ssize_t vout_fr_hint_show(struct class *class,
				 struct class_attribute *attr, char *buf)
{
	int fr_hint;
	int ret = 0;

	fr_hint = get_vframe_rate_hint();
	ret = sprintf(buf, "%d\n", fr_hint);

	return ret;
}

static ssize_t vout_fr_hint_store(struct class *class,
				  struct class_attribute *attr,
				  const char *buf, size_t count)
{
	int fr_hint;
	int ret = 0;

	mutex_lock(&vout_serve_mutex);
	ret = kstrtoint(buf, 10, &fr_hint);
	if (ret) {
		mutex_unlock(&vout_serve_mutex);
		return -EINVAL;
	}
	set_vframe_rate_hint(fr_hint);
	mutex_unlock(&vout_serve_mutex);

	return count;
}

static ssize_t vout_fr_range_show(struct class *class,
				  struct class_attribute *attr, char *buf)
{
	const struct vinfo_s *info = NULL;

	info = get_current_vinfo();
	if (!info)
		return sprintf(buf, "current vinfo is null\n");

	return sprintf(buf, "%d %d\n", info->vfreq_min, info->vfreq_max);
}

static ssize_t vout_frame_rate_show(struct class *class,
				    struct class_attribute *attr, char *buf)
{
	unsigned int fr;
	int ret = 0;

	fr = vout_frame_rate_measure();
	ret = sprintf(buf, "%d.%3d\n", (fr / 1000), (fr % 1000));

	return ret;
}

static ssize_t vout_bist_show(struct class *class,
			      struct class_attribute *attr, char *buf)
{
	int ret = 0;

	ret = sprintf(buf, "%d\n", bist_mode);

	return ret;
}

static ssize_t vout_bist_store(struct class *class,
			       struct class_attribute *attr,
			       const char *buf, size_t count)
{
	int ret = 0;

	mutex_lock(&vout_serve_mutex);

	ret = kstrtouint(buf, 10, &bist_mode);
	if (ret) {
		pr_info("%s: invalid data\n", __func__);
		mutex_unlock(&vout_serve_mutex);
		return -EINVAL;
	}
	set_vout_bist(bist_mode);

	mutex_unlock(&vout_serve_mutex);

	return count;
}

static ssize_t vout_vinfo_show(struct class *class,
			       struct class_attribute *attr, char *buf)
{
	const struct vinfo_s *info = NULL;
	ssize_t len = 0;
	unsigned int i, j, fr;
	unsigned char val;

	info = get_current_vinfo();
	if (!info)
		return sprintf(buf, "current vinfo is null\n");

	fr = vout_frame_rate_measure();
	len = sprintf(buf, "current vinfo:\n"
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
		       "    (meas_frame_rate:      %d.%03d)\n"
		       "    std_duration:          %d\n"
		       "    vfreq_max:             %d\n"
		       "    vfreq_min:             %d\n"
		       "    htotal:                %d\n"
		       "    vtotal:                %d\n"
		       "    video_clk:             %d\n"
		       "    fr_adj_type:           %d\n"
		       "    viu_color_fmt:         %d\n"
		       "    viu_mux:               0x%x\n"
		       "    3d_info:               %d\n\n",
		       info->name, info->mode, info->frac,
		       info->width, info->height, info->field_height,
		       info->aspect_ratio_num, info->aspect_ratio_den,
		       info->screen_real_width, info->screen_real_height,
		       info->sync_duration_num, info->sync_duration_den,
		       (fr / 1000), (fr % 1000), info->std_duration,
		       info->vfreq_max, info->vfreq_min,
		       info->htotal, info->vtotal, info->video_clk,
		       info->fr_adj_type, info->viu_color_fmt,
		       info->viu_mux, info->info_3d);
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
		       "    lumi_min              %d\n"
		       "    ldim_support          %d\n"
		       "    lumi_peak             %d\n",
		       info->hdr_info.hdr_support,
		       info->hdr_info.lumi_max,
		       info->hdr_info.lumi_avg,
		       info->hdr_info.lumi_min,
		       info->hdr_info.ldim_support,
		       info->hdr_info.lumi_peak);
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

static ssize_t vout_cap_show(struct class *class,
			     struct class_attribute *attr, char *buf)
{
	int ret;

	ret = get_vout_disp_cap(buf);
	if (!ret)
		return sprintf(buf, "null\n");

	return ret;
}

static ssize_t vout_debug_mode_show(struct class *class,
			     struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", enable_debugmode);
}

static ssize_t vout_debug_mode_store(struct class *class,
			       struct class_attribute *attr,
			       const char *buf, size_t count)
{
	int ret;
	int debug;

	ret = kstrtoint(buf, 10, &debug);
	if (ret)
		return -EINVAL;

	if (debug > 0)
		enable_debugmode = 1;
	else
		enable_debugmode = 0;

	return count;
}

static ssize_t vout_debug_vs_meas_show(struct class *class,
				       struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", vs_meas_en);
}

static ssize_t vout_debug_vs_meas_store(struct class *class,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	int ret;
	int temp;

	ret = kstrtoint(buf, 10, &temp);
	if (ret)
		return -EINVAL;

	if (temp > 0)
		vs_meas_en = 1;
	else
		vs_meas_en = 0;

	return count;
}

static ssize_t vout_debug_print_show(struct class *class,
				       struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", vout_debug_print);
}

static ssize_t vout_debug_print_store(struct class *class,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	int ret;
	int temp;

	ret = kstrtoint(buf, 10, &temp);
	if (ret)
		return -EINVAL;

	if (temp > 0)
		vout_debug_print = 1;
	else
		vout_debug_print = 0;

	return count;
}

static struct class_attribute vout_class_attrs[] = {
	__ATTR(mode,       0644, vout_mode_show, vout_mode_store),
	__ATTR(fr_policy,  0644,
	       vout_fr_policy_show, vout_fr_policy_store),
	__ATTR(fr_hint,    0644, vout_fr_hint_show, vout_fr_hint_store),
	__ATTR(fr_range,   0644, vout_fr_range_show, NULL),
	__ATTR(frame_rate, 0644, vout_frame_rate_show, NULL),
	__ATTR(bist,       0644, vout_bist_show, vout_bist_store),
	__ATTR(vinfo,      0444, vout_vinfo_show, NULL),
	__ATTR(cap,        0644, vout_cap_show, NULL),
	__ATTR(debug,      0644, vout_debug_mode_show, vout_debug_mode_store),
	__ATTR(vs_meas,    0644, vout_debug_vs_meas_show, vout_debug_vs_meas_store),
	__ATTR(print,      0644, vout_debug_print_show, vout_debug_print_store),
};

static int vout_attr_create(void)
{
	int i;
	int ret = 0;

	/* create vout class */
	vout_class = class_create(THIS_MODULE, VOUT_CLASS_NAME);
	if (IS_ERR(vout_class)) {
		VOUTERR("create vout class fail\n");
		return -1;
	}

	/* create vout class attr files */
	for (i = 0; i < ARRAY_SIZE(vout_class_attrs); i++) {
		if (class_create_file(vout_class, &vout_class_attrs[i])) {
			VOUTERR("create vout attribute %s fail\n",
				vout_class_attrs[i].attr.name);
		}
	}

	/*VOUTPR("create vout attribute OK\n");*/

	return ret;
}

static int vout_attr_remove(void)
{
	int i;

	if (!vout_class)
		return 0;

	for (i = 0; i < ARRAY_SIZE(vout_class_attrs); i++)
		class_remove_file(vout_class, &vout_class_attrs[i]);

	class_destroy(vout_class);
	vout_class = NULL;

	return 0;
}

/* ************************************************************* */
/* vout ioctl                                                    */
/* ************************************************************* */
static int vout_io_open(struct inode *inode, struct file *file)
{
	struct vout_cdev_s *vcdev;

	/*VOUTPR("%s\n", __func__);*/
	vcdev = container_of(inode->i_cdev, struct vout_cdev_s, cdev);
	file->private_data = vcdev;
	return 0;
}

static int vout_io_release(struct inode *inode, struct file *file)
{
	/*VOUTPR("%s\n", __func__);*/
	file->private_data = NULL;
	return 0;
}

static long vout_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	void __user *argp;
	int mcd_nr;
	struct vinfo_s *info = NULL;
	struct vinfo_base_s baseinfo;
	struct optical_base_s optical_info;
	int i, j;

	mcd_nr = _IOC_NR(cmd);
	/*VOUTPR("%s: cmd_dir = 0x%x, cmd_nr = 0x%x\n", __func__, _IOC_DIR(cmd), mcd_nr);*/

	argp = (void __user *)arg;
	switch (mcd_nr) {
	case VOUT_IOC_NR_GET_VINFO:
		info = get_current_vinfo();
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
			if (copy_to_user(argp, &baseinfo, sizeof(struct vinfo_base_s)))
				ret = -EFAULT;
		}
		break;
	case VOUT_IOC_NR_GET_OPTICAL_INFO:
		info = get_current_vinfo();
		if (!info) {
			ret = -EFAULT;
			break;
		}
		if (info->mode == VMODE_INIT_NULL) {
			ret = -EFAULT;
			break;
		}

		memset(&optical_info, 0, sizeof(struct optical_base_s));
		for (i = 0; i < 3; i++) {
			for (j = 0; j < 2; j++) {
				optical_info.primaries[i][j] =
					info->master_display_info.primaries[i][j];
			}
		}
		for (i = 0; i < 2; i++) {
			optical_info.white_point[i] =
				info->master_display_info.white_point[i];
		}
		optical_info.lumi_max = info->hdr_info.lumi_max;
		optical_info.lumi_min = info->hdr_info.lumi_min;
		optical_info.lumi_avg = info->hdr_info.lumi_avg;
		optical_info.lumi_peak = info->hdr_info.lumi_peak;
		optical_info.ldim_support = info->hdr_info.ldim_support;
		if (copy_to_user(argp, &optical_info, sizeof(struct optical_base_s)))
			ret = -EFAULT;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static long vout_compat_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	unsigned long ret;

	arg = (unsigned long)compat_ptr(arg);
	ret = vout_ioctl(file, cmd, arg);
	return ret;
}
#endif

static const struct file_operations vout_fops = {
	.owner          = THIS_MODULE,
	.open           = vout_io_open,
	.release        = vout_io_release,
	.unlocked_ioctl = vout_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl   = vout_compat_ioctl,
#endif
};

static int vout_fops_create(void)
{
	int ret = 0;

	vout_cdev = kmalloc(sizeof(*vout_cdev), GFP_KERNEL);
	if (!vout_cdev) {
		VOUTERR("failed to allocate vout_cdev\n");
		return -1;
	}

	ret = alloc_chrdev_region(&vout_cdev->devno, 0, 1, VOUT_CDEV_NAME);
	if (ret < 0) {
		VOUTERR("failed to alloc vout devno\n");
		goto vout_fops_err1;
	}

	cdev_init(&vout_cdev->cdev, &vout_fops);
	vout_cdev->cdev.owner = THIS_MODULE;
	ret = cdev_add(&vout_cdev->cdev, vout_cdev->devno, 1);
	if (ret) {
		VOUTERR("failed to add vout cdev\n");
		goto vout_fops_err2;
	}

	vout_cdev->dev = device_create(vout_class, NULL, vout_cdev->devno,
				       NULL, VOUT_CDEV_NAME);
	if (IS_ERR(vout_cdev->dev)) {
		ret = PTR_ERR(vout_cdev->dev);
		VOUTERR("failed to create vout device: %d\n", ret);
		goto vout_fops_err3;
	}

	/*VOUTPR("%s OK\n", __func__);*/
	return 0;

vout_fops_err3:
	cdev_del(&vout_cdev->cdev);
vout_fops_err2:
	unregister_chrdev_region(vout_cdev->devno, 1);
vout_fops_err1:
	kfree(vout_cdev);
	vout_cdev = NULL;
	return -1;
}

static void vout_fops_remove(void)
{
	cdev_del(&vout_cdev->cdev);
	unregister_chrdev_region(vout_cdev->devno, 1);
	kfree(vout_cdev);
	vout_cdev = NULL;
}

/* ************************************************************* */

#ifdef CONFIG_PM
static int aml_vout_suspend(struct platform_device *pdev, pm_message_t state)
{
#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND

	if (early_suspend_flag)
		return 0;

#endif
	vout_suspend();
	return 0;
}

static int aml_vout_resume(struct platform_device *pdev)
{
#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND

	if (early_suspend_flag)
		return 0;

#endif
	vout_resume();
	return 0;
}
#endif

#ifdef CONFIG_HIBERNATION
static int aml_vout_freeze(struct device *dev)
{
	return 0;
}

static int aml_vout_thaw(struct device *dev)
{
	return 0;
}

static int aml_vout_restore(struct device *dev)
{
	enum vmode_e mode;

	mode = get_current_vmode();
	vout_notifier_call_chain(VOUT_EVENT_MODE_CHANGE, &mode);

	return 0;
}

static int aml_vout_pm_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);

	return aml_vout_suspend(pdev, PMSG_SUSPEND);
}

static int aml_vout_pm_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);

	return aml_vout_resume(pdev);
}
#endif

#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
static void aml_vout_early_suspend(struct early_suspend *h)
{
	if (early_suspend_flag)
		return;

	vout_suspend();
	early_suspend_flag = 1;
}

static void aml_vout_late_resume(struct early_suspend *h)
{
	if (!early_suspend_flag)
		return;

	early_suspend_flag = 0;
	vout_resume();
}
#endif

static void aml_vout_get_dt_info(struct platform_device *pdev)
{
	struct resource *res;
	unsigned int para[2];
	int ret;

	ret = of_property_read_u32(pdev->dev.of_node, "fr_policy", &para[0]);
	if (!ret) {
		ret = set_vframe_rate_policy(para[0]);
		if (ret)
			VOUTERR("init fr_policy %d failed\n", para[0]);
		else
			VOUTPR("fr_policy:%d\n", para[0]);
	}

	ret = of_property_read_u32(pdev->dev.of_node, "vs_meas", &para[0]);
	if (!ret) {
		vs_meas_en = para[0];
		VOUTPR("get vs_meas:%d\n", vs_meas_en);
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_IRQ, "vsync");
	if (res) {
		vsync_irq = res->start;
		if (request_irq(vsync_irq, vout_vsync_irq_handler, IRQF_SHARED,
				"vout_vsync", (void *)&vsync_irq)) {
			VOUTERR("%s: can't request vout_vsync\n", __func__);
		}
	} else {
		VOUTERR("%s: can't get vsync irq\n", __func__);
	}
}

/*****************************************************************
 **
 **	vout driver interface
 **
 ******************************************************************/
static int aml_vout_probe(struct platform_device *pdev)
{
	int ret = -1;

	vout_dev = &pdev->dev;

	aml_vout_get_dt_info(pdev);
	vout_vdo_meas_ctrl_init();

#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
	early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
	early_suspend.suspend = aml_vout_early_suspend;
	early_suspend.resume = aml_vout_late_resume;
	register_early_suspend(&early_suspend);
#endif

	vout_class = NULL;
	ret = vout_attr_create();
	ret = vout_fops_create();

	vout_register_server(&nulldisp_vout_server);
	set_vout_init_mode();

	VOUTPR("%s OK\n", __func__);
	return ret;
}

static int aml_vout_remove(struct platform_device *pdev)
{
	if (vsync_irq)
		free_irq(vsync_irq, (void *)&vsync_irq);

#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
	unregister_early_suspend(&early_suspend);
#endif

	vout_attr_remove();
	vout_fops_remove();
	vout_unregister_server(&nulldisp_vout_server);

	return 0;
}

static void aml_vout_shutdown(struct platform_device *pdev)
{
	VOUTPR("%s\n", __func__);
	vout_shutdown();
}

static const struct of_device_id aml_vout_dt_match[] = {
	{ .compatible = "amlogic, vout",},
	{ },
};

#ifdef CONFIG_HIBERNATION
const struct dev_pm_ops vout_pm = {
	.freeze		= aml_vout_freeze,
	.thaw		= aml_vout_thaw,
	.restore	= aml_vout_restore,
	.suspend	= aml_vout_pm_suspend,
	.resume		= aml_vout_pm_resume,
};
#endif

static struct platform_driver vout_driver = {
	.probe     = aml_vout_probe,
	.remove    = aml_vout_remove,
	.shutdown   = aml_vout_shutdown,
#ifdef CONFIG_PM
	.suspend   = aml_vout_suspend,
	.resume    = aml_vout_resume,
#endif
	.driver = {
		.name = "vout",
		.of_match_table = aml_vout_dt_match,
#ifdef CONFIG_HIBERNATION
		.pm = &vout_pm,
#endif
	},
};

int __init vout_init_module(void)
{
	return platform_driver_register(&vout_driver);
}

__exit void vout_exit_module(void)
{
	platform_driver_unregister(&vout_driver);
}

static int str2lower(char *str)
{
	while (*str != '\0') {
		*str = tolower(*str);
		str++;
	}
	return 0;
}

static void vout_init_mode_parse(char *str)
{
	/* detect vout mode */
	if (strlen(str) <= 1) {
		VOUTERR("%s: %s\n", __func__, str);
		return;
	}

	/* detect uboot display */
	if (strncmp(str, "en", 2) == 0) { /* enable */
		uboot_display = 1;
		VOUTPR("%s: %d\n", str, uboot_display);
		return;
	}
	if (strncmp(str, "dis", 3) == 0) { /* disable */
		uboot_display = 0;
		VOUTPR("%s: %d\n", str, uboot_display);
		return;
	}
	if (strncmp(str, "frac", 4) == 0) { /* frac */
		if ((strlen(vout_mode_uboot) + strlen(str))
		    < VMODE_NAME_LEN_MAX)
			strcat(vout_mode_uboot, str);
		else
			VOUTERR("%s: str len out of support\n", __func__);
		VOUTPR("%s\n", str);
		return;
	}
	if (strncmp(str, "frac", 4) == 0) { /* frac */
		if ((strlen(vout_mode_uboot) + strlen(str))
		    < VMODE_NAME_LEN_MAX)
			strcat(vout_mode_uboot, str);
		else
			VOUTERR("%s: str len out of support\n", __func__);
		VOUTPR("%s\n", str);
		return;
	}

	/*
	 * just save the vmode_name,
	 * convert to vmode when vout sever registered
	 */
	snprintf(vout_mode_uboot, VMODE_NAME_LEN_MAX, "%s", str);
	vout_trim_string(vout_mode_uboot);
	VOUTPR("%s\n", vout_mode_uboot);
}

int get_vout_init_mode(char *str)
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
		/* VOUTPR("%s\n", option); */
		str2lower(option);
		vout_init_mode_parse(option);
	}

	return 0;
}
__setup("vout=", get_vout_init_mode);

/*TODO: drm to disable display/mode sysfs set.*/
void disable_vout_mode_set_sysfs(void)
{
	disable_modesysfs = true;
}
EXPORT_SYMBOL(disable_vout_mode_set_sysfs);

//MODULE_AUTHOR("Platform-BJ <platform.bj@amlogic.com>");
//MODULE_DESCRIPTION("VOUT Server Module");
//MODULE_LICENSE("GPL");
