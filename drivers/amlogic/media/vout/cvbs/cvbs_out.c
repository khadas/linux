// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * drivers/amlogic/media/vout/cvbs/cvbs_out.c
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
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/ctype.h>
/*#include <linux/major.h>*/
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/workqueue.h>
#include <linux/uaccess.h>
#include <linux/clk.h>

/* Amlogic Headers */
#include <linux/amlogic/media/registers/cpu_version.h>
#include <linux/amlogic/iomap.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/media/vout/vinfo.h>
#include <linux/amlogic/media/vout/vdac_dev.h>
#ifdef CONFIG_AMLOGIC_VPU
#include <linux/amlogic/media/vpu/vpu.h>
#endif

/* Local Headers */
#include "cvbsregs.h"
#include "cvbs_out.h"
#include "enc_clk_config.h"
#include "cvbs_out_reg.h"
#include "cvbs_log.h"
#ifdef CONFIG_AMLOGIC_WSS
#include "wss.h"
#endif
#include <linux/amlogic/gki_module.h>

#ifdef CONFIG_AML_VOUT_CC_BYPASS
/* interrupt source */
#define INT_VIU_VSYNC    35
#endif

const char *cvbs_mode_t[] = {
	"480cvbs",
	"576cvbs",
	"pal_m",
	"pal_n",
	"ntsc_m",
	NULL
};

static struct vinfo_s cvbs_info[] = {
	{ /* MODE_480CVBS*/
		.name              = "480cvbs",
		.mode              = VMODE_CVBS,
		.frac              = 0,
		.width             = 720,
		.height            = 480,
		.field_height      = 240,
		.aspect_ratio_num  = 4,
		.aspect_ratio_den  = 3,
		.sync_duration_num = 60,
		.sync_duration_den = 1,
		.video_clk         = 27000000,
		.htotal            = 1716,
		.vtotal            = 525,
		.fr_adj_type       = VOUT_FR_ADJ_NONE,
		.viu_color_fmt     = COLOR_FMT_YUV444,
		.viu_mux           = VIU_MUX_ENCI,
		.vout_device       = NULL,
	},
	{ /* MODE_576CVBS */
		.name              = "576cvbs",
		.mode              = VMODE_CVBS,
		.frac              = 0,
		.width             = 720,
		.height            = 576,
		.field_height      = 288,
		.aspect_ratio_num  = 4,
		.aspect_ratio_den  = 3,
		.sync_duration_num = 50,
		.sync_duration_den = 1,
		.video_clk         = 27000000,
		.htotal            = 1728,
		.vtotal            = 625,
		.fr_adj_type       = VOUT_FR_ADJ_NONE,
		.viu_color_fmt     = COLOR_FMT_YUV444,
		.viu_mux           = VIU_MUX_ENCI,
		.vout_device       = NULL,
	},
	{ /* MODE_PAL_M */
		.name              = "pal_m",
		.mode              = VMODE_CVBS,
		.frac              = 0,
		.width             = 720,
		.height            = 480,
		.field_height      = 240,
		.aspect_ratio_num  = 4,
		.aspect_ratio_den  = 3,
		.sync_duration_num = 60,
		.sync_duration_den = 1,
		.video_clk         = 27000000,
		.htotal            = 1716,
		.vtotal            = 525,
		.viu_color_fmt     = COLOR_FMT_YUV444,
		.viu_mux           = VIU_MUX_ENCI,
		.vout_device       = NULL,
	},
	{ /* MODE_PAL_N */
		.name              = "pal_n",
		.mode              = VMODE_CVBS,
		.frac              = 0,
		.width             = 720,
		.height            = 576,
		.field_height      = 288,
		.aspect_ratio_num  = 4,
		.aspect_ratio_den  = 3,
		.sync_duration_num = 50,
		.sync_duration_den = 1,
		.video_clk         = 27000000,
		.htotal            = 1728,
		.vtotal            = 625,
		.viu_color_fmt     = COLOR_FMT_YUV444,
		.viu_mux           = VIU_MUX_ENCI,
		.vout_device       = NULL,
	},
	{ /* MODE_NTSC_M */
		.name              = "ntsc_m",
		.mode              = VMODE_CVBS,
		.frac              = 0,
		.width             = 720,
		.height            = 480,
		.field_height      = 240,
		.aspect_ratio_num  = 4,
		.aspect_ratio_den  = 3,
		.sync_duration_num = 60,
		.sync_duration_den = 1,
		.video_clk         = 27000000,
		.htotal            = 1716,
		.vtotal            = 525,
		.fr_adj_type       = VOUT_FR_ADJ_NONE,
		.viu_color_fmt     = COLOR_FMT_YUV444,
		.viu_mux           = VIU_MUX_ENCI,
		.vout_device       = NULL,
	},
};

#ifdef CONFIG_AML_VOUT_CC_BYPASS
static struct cc_ring_mgr_s cc_ringbuf;
static spinlock_t tvout_clk_lock;
static unsigned int vsync_empty_flag;
static unsigned int vsync_empty_flag_evn;
static unsigned int vsync_empty_flag_odd;
#endif

/*bit[0]: 0=vid_pll, 1=gp0_pll*/
/*bit[1]: 0=vid2_clk, 1=vid1_clk*/
unsigned int cvbs_clk_path;

static struct cvbs_drv_s *cvbs_drv;
static enum cvbs_mode_e local_cvbs_mode;
static DEFINE_MUTEX(setmode_mutex);

static int cvbs_vdac_power_level;
static ssize_t aml_CVBS_attr_vdac_power_show(struct class *class,
					     struct class_attribute *attr,
					     char *buf);
static ssize_t aml_CVBS_attr_vdac_power_store(struct class *class,
					      struct class_attribute *attr,
					      const char *buf, size_t count);
struct class_attribute class_CVBS_attr_vdac_power_level =
	__ATTR(vdac_power_level, 0644, aml_CVBS_attr_vdac_power_show,
	       aml_CVBS_attr_vdac_power_store);

static ssize_t aml_CVBS_attr_debug_show(struct class *class,
					struct class_attribute *attr,
					char *buf);
static ssize_t aml_CVBS_attr_debug_store(struct class *class,
					 struct class_attribute *attr,
					 const char *buf, size_t count);
struct class_attribute class_CVBS_attr_debug =
	__ATTR(debug, 0644, aml_CVBS_attr_debug_show,
	       aml_CVBS_attr_debug_store);

#ifdef CONFIG_AMLOGIC_WSS
struct class_attribute class_CVBS_attr_wss = __ATTR(wss, 0644,
			aml_CVBS_attr_wss_show, aml_CVBS_attr_wss_store);
#endif /*CONFIG_AMLOGIC_WSS*/

static void cvbs_bist_test(unsigned int bist);

/* static int get_cpu_minor(void)
 * {
 *	return get_meson_cpu_version(MESON_CPU_VERSION_LVL_MINOR);
 * }
 */

int cvbs_cpu_type(void)
{
	if (!cvbs_drv) {
		cvbs_log_err("error: %s: no cvbs drv\n", __func__);
		return -1;
	}
	if (!cvbs_drv->cvbs_data) {
		cvbs_log_err("error: %s: no cvbs data\n", __func__);
		return -1;
	}
	return cvbs_drv->cvbs_data->cpu_id;
}
EXPORT_SYMBOL(cvbs_cpu_type);

struct meson_cvbsout_data *get_cvbs_data(void)
{
	if (!cvbs_drv) {
		cvbs_log_err("error: %s: no cvbs drv\n", __func__);
		return NULL;
	}
	if (!cvbs_drv->cvbs_data) {
		cvbs_log_err("error: %s: no cvbs data\n", __func__);
		return NULL;
	}
	return cvbs_drv->cvbs_data;
}

static void cvbs_vdac_output(unsigned int open)
{
	if (open == 0) { /* close */
		cvbs_drv->flag &= ~CVBS_FLAG_EN_VDAC;
		vdac_enable(0, VDAC_MODULE_CVBS_OUT);
	} else if (open == 1) { /* open */
		vdac_vref_adj(cvbs_drv->cvbs_data->vdac_vref_adj);
		vdac_enable(1, VDAC_MODULE_CVBS_OUT);
		cvbs_drv->flag |= CVBS_FLAG_EN_VDAC;
	}
	cvbs_log_info("%s: %d\n", __func__, open);
}

#ifdef CONFIG_CVBS_PERFORMANCE_COMPATIBILITY_SUPPORT
static void cvbs_performance_enhancement(enum cvbs_mode_e mode)
{
	struct performance_config_s *perfconf = NULL;
	const struct reg_s *s = NULL;
	int i = 0;

	switch (mode) {
	case MODE_576CVBS:
		perfconf = &cvbs_drv->perf_conf_pal;
		break;
	case MODE_480CVBS:
	case MODE_NTSC_M:
		perfconf = &cvbs_drv->perf_conf_ntsc;
		break;
	default:
		break;
	}
	if (!perfconf)
		return;

	if (!perfconf->reg_table) {
		cvbs_log_info("no performance table\n");
		return;
	}

	i = 0;
	s = perfconf->reg_table;
	while (i < perfconf->reg_cnt) {
		cvbs_out_reg_write(s->reg, s->val);
		cvbs_log_info("%s: vcbus reg 0x%04x = 0x%08x\n",
			      __func__, s->reg, s->val);
		s++;
		i++;
	}
	cvbs_log_info("%s\n", __func__);
}
#endif/* end of CVBS_PERFORMANCE_COMPATIBILITY_SUPPORT */

static int cvbs_out_set_venc(enum cvbs_mode_e mode)
{
	const struct reg_s *s;
	int ret;

	/* setup encoding regs */
	s = cvbs_regs_tab[mode].enc_reg_setting;
	if (!s) {
		cvbs_log_info("display mode %d get enc set NULL\n", mode);
		ret = -1;
	} else {
		while (s->reg != MREG_END_MARKER) {
			cvbs_out_reg_write(s->reg, s->val);
			s++;
		}
		ret = 0;
	}
	/* cvbs_log_info("%s[%d]\n", __func__, __LINE__); */
	return ret;
}

static void cvbs_out_disable_clk(void)
{
	disable_vmode_clk();
}

static void cvbs_out_vpu_power_ctrl(int status)
{
	if (!cvbs_drv->vinfo)
		return;
	if (status) {
#ifdef CONFIG_AMLOGIC_VPU
		vpu_dev_clk_request(cvbs_drv->cvbs_vpu_dev, cvbs_drv->vinfo->video_clk);
		vpu_dev_mem_power_on(cvbs_drv->cvbs_vpu_dev);
#endif
	} else {
#ifdef CONFIG_AMLOGIC_VPU
		vpu_dev_mem_power_down(cvbs_drv->cvbs_vpu_dev);
		vpu_dev_clk_release(cvbs_drv->cvbs_vpu_dev);
#endif
	}
}

static void cvbs_out_clk_gate_ctrl(int status)
{
	if (!cvbs_drv->vinfo)
		return;

	if (status) {
		if (cvbs_drv->clk_gate_state) {
			cvbs_log_info("clk_gate is already on\n");
			return;
		}

		if (IS_ERR(cvbs_drv->venci_top_gate))
			cvbs_log_err("error: %s: venci_top_gate\n", __func__);
		else
			clk_prepare_enable(cvbs_drv->venci_top_gate);

		if (IS_ERR(cvbs_drv->venci_0_gate))
			cvbs_log_err("error: %s: venci_0_gate\n", __func__);
		else
			clk_prepare_enable(cvbs_drv->venci_0_gate);

		if (IS_ERR(cvbs_drv->venci_1_gate))
			cvbs_log_err("error: %s: venci_1_gate\n", __func__);
		else
			clk_prepare_enable(cvbs_drv->venci_1_gate);

		if (IS_ERR(cvbs_drv->vdac_clk_gate))
			cvbs_log_err("error: %s: vdac_clk_gate\n", __func__);
		else
			clk_prepare_enable(cvbs_drv->vdac_clk_gate);

#ifdef CONFIG_AMLOGIC_VPU
		vpu_dev_clk_gate_on(cvbs_drv->cvbs_vpu_dev);
#endif

		cvbs_drv->clk_gate_state = 1;
	} else {
		if (cvbs_drv->clk_gate_state == 0) {
			cvbs_log_info("clk_gate is already off\n");
			return;
		}

#ifdef CONFIG_AMLOGIC_VPU
		vpu_dev_clk_gate_off(cvbs_drv->cvbs_vpu_dev);
#endif
		if (IS_ERR(cvbs_drv->vdac_clk_gate))
			cvbs_log_err("error: %s: vdac_clk_gate\n", __func__);
		else
			clk_disable_unprepare(cvbs_drv->vdac_clk_gate);

		if (IS_ERR(cvbs_drv->venci_0_gate))
			cvbs_log_err("error: %s: venci_0_gate\n", __func__);
		else
			clk_disable_unprepare(cvbs_drv->venci_0_gate);

		if (IS_ERR(cvbs_drv->venci_1_gate))
			cvbs_log_err("error: %s: venci_1_gate\n", __func__);
		else
			clk_disable_unprepare(cvbs_drv->venci_1_gate);

		if (IS_ERR(cvbs_drv->venci_top_gate))
			cvbs_log_err("error: %s: venci_top_gate\n", __func__);
		else
			clk_disable_unprepare(cvbs_drv->venci_top_gate);

		cvbs_drv->clk_gate_state = 0;
	}
}

static void cvbs_vdac_dwork(struct work_struct *work)
{
	if ((cvbs_drv->flag & CVBS_FLAG_EN_ENCI) == 0)
		return;
	cvbs_vdac_output(1);
}

static int cvbs_out_setmode(void)
{
	int ret;

	switch (local_cvbs_mode) {
	case MODE_480CVBS:
		cvbs_log_info("SET cvbs mode: 480cvbs\n");
		break;
	case MODE_576CVBS:
		cvbs_log_info("SET cvbs mode: 576cvbs\n");
		break;
	case MODE_PAL_M:
		cvbs_log_info("SET cvbs mode: pal_m\n");
		break;
	case MODE_PAL_N:
		cvbs_log_info("SET cvbs mode: pal_n\n");
		break;
	case MODE_NTSC_M:
		cvbs_log_info("SET cvbs mode: ntsc_m\n");
		break;
	default:
		cvbs_log_err("%s:invalid cvbs mode", __func__);
		break;
	}

	if (local_cvbs_mode >= MODE_MAX) {
		cvbs_log_err("%s:mode error.return", __func__);
		return -1;
	}
	mutex_lock(&setmode_mutex);

	cvbs_out_vpu_power_ctrl(1);
	cvbs_out_clk_gate_ctrl(1);

	cvbs_vdac_output(0);
	cvbs_out_reg_write(VENC_VDAC_SETTING, 0xff);
	/* Before setting clk for CVBS, disable ENCI to avoid hungup */
	cvbs_out_reg_write(ENCI_VIDEO_EN, 0);
	set_vmode_clk();
	ret = cvbs_out_set_venc(local_cvbs_mode);
	if (ret) {
		mutex_lock(&setmode_mutex);
		return -1;
	}
#ifdef CONFIG_CVBS_PERFORMANCE_COMPATIBILITY_SUPPORT
	cvbs_performance_enhancement(local_cvbs_mode);
#endif
	cvbs_drv->flag |= CVBS_FLAG_EN_ENCI;
	schedule_delayed_work(&cvbs_drv->vdac_dwork, msecs_to_jiffies(1000));
	mutex_unlock(&setmode_mutex);
	return 0;
}

static int cvbs_open(struct inode *inode, struct file *file)
{
	struct cvbs_drv_s *cdrv;
	/* Get the per-device structure that contains this cdev */
	cdrv = container_of(&inode->i_cdev, struct cvbs_drv_s, cdev);
	file->private_data = cdrv;
	return 0;
}

static int cvbs_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

static long cvbs_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long ret = 0;
#ifdef CONFIG_AML_VOUT_CC_BYPASS
	unsigned int CC_2byte_data = 0;
	unsigned long flags = 0;
	void __user *argp = (void __user *)arg;
#endif
	cvbs_log_info("[cvbs..] %s: cmd_nr = 0x%x\n",
			__func__, _IOC_NR(cmd));
	if (_IOC_TYPE(cmd) != _TM_V) {
		cvbs_log_err("%s invalid command: %u\n", __func__, cmd);
		return -ENOTTY;
	}
	switch (cmd) {
#ifdef CONFIG_AML_VOUT_CC_BYPASS
	case VOUT_IOC_CC_OPEN:
		spin_lock_irqsave(&tvout_clk_lock, flags);
		memset(&cc_ringbuf, 0, sizeof(struct cc_ring_mgr_s));
		spin_unlock_irqrestore(&tvout_clk_lock, flags);
		cvbs_out_reg_setb(ENCI_VBI_SETTING, 0x3, 0, 2);
		break;
	case VOUT_IOC_CC_CLOSE:
		spin_lock_irqsave(&tvout_clk_lock, flags);
		memset(&cc_ringbuf, 0, sizeof(struct cc_ring_mgr_s));
		spin_unlock_irqrestore(&tvout_clk_lock, flags);
		cvbs_out_reg_setb(ENCI_VBI_SETTING, 0x0, 0, 2);
		break;
	case VOUT_IOC_CC_DATA: {
		struct vout_cc_parm_s parm = {0};

		spin_lock_irqsave(&tvout_clk_lock, flags);
		if (copy_from_user(&parm, argp,
				sizeof(struct vout_cc_parm_s))) {
			cvbs_log_err("VOUT_IOC_CC_DATAinvalid parameter\n");
			ret = -EFAULT;
			spin_unlock_irqrestore(&tvout_clk_lock, flags);
			break;
		}

		if (parm.type != 0) {
			spin_unlock_irqrestore(&tvout_clk_lock, flags);
			break;
		}
		/* cc standerd:nondisplay control byte + display control byte */
		/* our chip high-low 16bits is opposite */
		CC_2byte_data = parm.data2 << 8 | parm.data1;
		if ((cc_ringbuf.wp + 1) % MAX_RING_BUFF_LEN != cc_ringbuf.rp) {
			cc_ringbuf.cc_data[cc_ringbuf.wp].type = parm.type;
			cc_ringbuf.cc_data[cc_ringbuf.wp].data = CC_2byte_data;
			cc_ringbuf.wp = (cc_ringbuf.wp + 1) %
					MAX_RING_BUFF_LEN;
			/* vout_log_info("CCringbuf Write :0x%x wp:%d\n", */
			/* CC_2byte_data, cc_ringbuf.wp); */
		} else {
			cvbs_log_err("CCringbuf is FULL!! can't write.\n");
		}

		spin_unlock_irqrestore(&tvout_clk_lock, flags);
		break;
	}
#endif
	default:
		ret = -ENOIOCTLCMD;
		cvbs_log_err("%s %d is not supported command\n",
			     __func__, cmd);
		break;
	}
	cvbs_log_info("%s..out.ret=0x%lx\n", __func__, ret);
	return ret;
}

#ifdef CONFIG_COMPAT
static long cvbs_compat_ioctl(struct file *file, unsigned int cmd,
			      unsigned long arg)
{
	unsigned long ret;

	arg = (unsigned long)compat_ptr(arg);
	ret = cvbs_ioctl(file, cmd, arg);
	return ret;
}
#endif

static const struct file_operations am_cvbs_fops = {
	.open	= cvbs_open,
	.read	= NULL,/* am_cvbs_read, */
	.write	= NULL,
	.unlocked_ioctl	= cvbs_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = cvbs_compat_ioctl,
#endif
	.release	= cvbs_release,
	.poll		= NULL,
};

const struct vinfo_s *get_valid_vinfo(char  *mode)
{
	struct vinfo_s *vinfo = NULL;
	int  i, count = ARRAY_SIZE(cvbs_info);
	int mode_name_len = 0;

	/*cvbs_log_info("get_valid_vinfo..out.mode:%s\n", mode);*/
	for (i = 0; i < count; i++) {
		if (strncmp(cvbs_info[i].name, mode,
			    strlen(cvbs_info[i].name)) == 0) {
			if (strlen(cvbs_info[i].name) > mode_name_len) {
				vinfo = &cvbs_info[i];
				mode_name_len = strlen(cvbs_info[i].name);
				local_cvbs_mode = i;
				break;
			}
		}
	}
	if (vinfo) {
		strncpy(vinfo->ext_name, mode,
			(strlen(mode) < 32) ? strlen(mode) : 31);
		cvbs_drv->vinfo = vinfo;
	} else {
		local_cvbs_mode = MODE_MAX;
	}
	return vinfo;
}
EXPORT_SYMBOL(get_valid_vinfo);

static struct vinfo_s *cvbs_get_current_info(void)
{
	return cvbs_drv->vinfo;
}

enum cvbs_mode_e get_local_cvbs_mode(void)
{
	return local_cvbs_mode;
}
EXPORT_SYMBOL(get_local_cvbs_mode);

int cvbs_set_current_vmode(enum vmode_e mode)
{
	enum vmode_e tvmode;

	tvmode = mode & VMODE_MODE_BIT_MASK;
	if (tvmode != VMODE_CVBS)
		return -EINVAL;
	if (local_cvbs_mode == MODE_MAX) {
		cvbs_log_info("local_cvbs_mode err:%d!\n", local_cvbs_mode);
		return 1;
	}

	cvbs_log_info("mode is %d,sync_duration_den=%d,sync_duration_num=%d\n",
		      tvmode, cvbs_drv->vinfo->sync_duration_den,
		      cvbs_drv->vinfo->sync_duration_num);

	/*set limit range for enci*/
	amvecm_clip_range_limit(1);
	if (mode & VMODE_INIT_BIT_MASK) {
		cvbs_out_vpu_power_ctrl(1);
		cvbs_out_clk_gate_ctrl(1);
		cvbs_vdac_output(1);
		cvbs_drv->flag = (CVBS_FLAG_EN_ENCI | CVBS_FLAG_EN_VDAC);
		cvbs_log_info("already display in uboot\n");
		return 0;
	}
	cvbs_out_setmode();

	return 0;
}
EXPORT_SYMBOL(cvbs_set_current_vmode);

static enum vmode_e cvbs_validate_vmode(char *mode, unsigned int frac)
{
	const struct vinfo_s *info = get_valid_vinfo(mode);

	if (frac)
		return VMODE_MAX;

	if (info)
		return VMODE_CVBS;
	return VMODE_MAX;
}

static int cvbs_check_same_vmodeattr(char *mode)
{
	return 1;
}

static int cvbs_vmode_is_supported(enum vmode_e mode)
{
	if ((mode & VMODE_MODE_BIT_MASK) == VMODE_CVBS)
		return true;
	return false;
}

static int cvbs_module_disable(enum vmode_e cur_vmod)
{
	cvbs_drv->flag &= ~CVBS_FLAG_EN_ENCI;

	cvbs_vdac_output(0);

	/*restore full range for encp/encl*/
	amvecm_clip_range_limit(0);

	cvbs_out_vpu_power_ctrl(0);
	cvbs_out_clk_gate_ctrl(0);

	return 0;
}

static int cvbs_vout_state;
static int cvbs_vout_set_state(int index)
{
	cvbs_vout_state |= (1 << index);
	return 0;
}

static int cvbs_vout_clr_state(int index)
{
	cvbs_vout_state &= ~(1 << index);
	return 0;
}

static int cvbs_vout_get_state(void)
{
	return cvbs_vout_state;
}

static int cvbs_vout_get_disp_cap(char *buf)
{
	int ret = 0, i;

	for (i = 0; cvbs_mode_t[i]; i++)
		ret += snprintf(buf + ret, PAGE_SIZE, "%s\n", cvbs_mode_t[i]);
	return ret;
}

#ifdef CONFIG_PM
static int cvbs_suspend(void)
{
	/* TODO */
	/* video_dac_disable(); */
	cvbs_drv->flag &= ~CVBS_FLAG_EN_ENCI;
	cvbs_vdac_output(0);
	return 0;
}

static int cvbs_resume(void)
{
	/* TODO */
	/* video_dac_enable(0xff); */
	cvbs_set_current_vmode(cvbs_drv->vinfo->mode);
	return 0;
}
#endif

static struct vout_server_s cvbs_vout_server = {
	.name = "cvbs_vout_server",
	.op = {
		.get_vinfo = cvbs_get_current_info,
		.set_vmode = cvbs_set_current_vmode,
		.validate_vmode = cvbs_validate_vmode,
		.check_same_vmodeattr = cvbs_check_same_vmodeattr,
		.vmode_is_supported = cvbs_vmode_is_supported,
		.disable = cvbs_module_disable,
		.set_state = cvbs_vout_set_state,
		.clr_state = cvbs_vout_clr_state,
		.get_state = cvbs_vout_get_state,
		.get_disp_cap = cvbs_vout_get_disp_cap,
		.set_vframe_rate_hint = NULL,
		.get_vframe_rate_hint = NULL,
		.set_bist = cvbs_bist_test,
#ifdef CONFIG_PM
		.vout_suspend = cvbs_suspend,
		.vout_resume = cvbs_resume,
#endif
	},
};

#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
static struct vout_server_s cvbs_vout2_server = {
	.name = "cvbs_vout2_server",
	.op = {
		.get_vinfo = cvbs_get_current_info,
		.set_vmode = cvbs_set_current_vmode,
		.validate_vmode = cvbs_validate_vmode,
		.vmode_is_supported = cvbs_vmode_is_supported,
		.disable = cvbs_module_disable,
		.set_state = cvbs_vout_set_state,
		.clr_state = cvbs_vout_clr_state,
		.get_state = cvbs_vout_get_state,
		.get_disp_cap = cvbs_vout_get_disp_cap,
		.set_vframe_rate_hint = NULL,
		.get_vframe_rate_hint = NULL,
		.set_bist = cvbs_bist_test,
#ifdef CONFIG_PM
		.vout_suspend = cvbs_suspend,
		.vout_resume = cvbs_resume,
#endif
	},
};
#endif

static void cvbs_init_vout(void)
{
	if (!cvbs_drv->vinfo)
		cvbs_drv->vinfo = &cvbs_info[MODE_480CVBS];

	if (vout_register_server(&cvbs_vout_server))
		cvbs_log_err("register cvbs module server fail\n");
	else
		cvbs_log_info("register cvbs module server ok\n");
#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
	if (vout2_register_server(&cvbs_vout2_server))
		cvbs_log_err("register cvbs module vout2 server fail\n");
	else
		cvbs_log_info("register cvbs module vout2 server ok\n");
#endif
}

static char *cvbs_out_bist_str[] = {
	"OFF",   /* 0 */
	"Color Bar",   /* 1 */
	"Thin Line",   /* 2 */
	"Dot Grid",    /* 3 */
	"White",
	"Red",
	"Green",
	"Blue",
	"Black",
};

static void cvbs_bist_test(unsigned int bist)
{
	switch (bist) {
	case 1:
	case 2:
	case 3:
		cvbs_out_reg_write(ENCI_TST_CLRBAR_STRT, 0x112);
		cvbs_out_reg_write(ENCI_TST_CLRBAR_WIDTH, 0xb4);
		cvbs_out_reg_write(ENCI_TST_MDSEL, bist);
		cvbs_out_reg_write(ENCI_TST_Y, 0x200);
		cvbs_out_reg_write(ENCI_TST_CB, 0x200);
		cvbs_out_reg_write(ENCI_TST_CR, 0x200);
		cvbs_out_reg_write(ENCI_TST_EN, 1);
		pr_info("show bist pattern %d: %s\n",
			bist, cvbs_out_bist_str[bist]);
		break;
	case 4:
		cvbs_out_reg_write(ENCI_TST_CLRBAR_STRT, 0x112);
		cvbs_out_reg_write(ENCI_TST_CLRBAR_WIDTH, 0xb4);
		cvbs_out_reg_write(ENCI_TST_MDSEL, 0);
		cvbs_out_reg_write(ENCI_TST_Y, 0x3ff);
		cvbs_out_reg_write(ENCI_TST_CB, 0x200);
		cvbs_out_reg_write(ENCI_TST_CR, 0x200);
		cvbs_out_reg_write(ENCI_TST_EN, 1);
		pr_info("show bist pattern %d: %s\n",
			bist, cvbs_out_bist_str[bist]);
		break;
	case 5:
		cvbs_out_reg_write(ENCI_TST_CLRBAR_STRT, 0x112);
		cvbs_out_reg_write(ENCI_TST_CLRBAR_WIDTH, 0xb4);
		cvbs_out_reg_write(ENCI_TST_MDSEL, 0);
		cvbs_out_reg_write(ENCI_TST_Y, 0x200);
		cvbs_out_reg_write(ENCI_TST_CB, 0x0);
		cvbs_out_reg_write(ENCI_TST_CR, 0x3ff);
		cvbs_out_reg_write(ENCI_TST_EN, 1);
		pr_info("show bist pattern %d: %s\n",
			bist, cvbs_out_bist_str[bist]);
		break;
	case 6:
		cvbs_out_reg_write(ENCI_TST_CLRBAR_STRT, 0x112);
		cvbs_out_reg_write(ENCI_TST_CLRBAR_WIDTH, 0xb4);
		cvbs_out_reg_write(ENCI_TST_MDSEL, 0);
		cvbs_out_reg_write(ENCI_TST_Y, 0x200);
		cvbs_out_reg_write(ENCI_TST_CB, 0x0);
		cvbs_out_reg_write(ENCI_TST_CR, 0x0);
		cvbs_out_reg_write(ENCI_TST_EN, 1);
		pr_info("show bist pattern %d: %s\n",
			bist, cvbs_out_bist_str[bist]);
		break;
	case 7:
		cvbs_out_reg_write(ENCI_TST_CLRBAR_STRT, 0x112);
		cvbs_out_reg_write(ENCI_TST_CLRBAR_WIDTH, 0xb4);
		cvbs_out_reg_write(ENCI_TST_MDSEL, 0);
		cvbs_out_reg_write(ENCI_TST_Y, 0x200);
		cvbs_out_reg_write(ENCI_TST_CB, 0x3ff);
		cvbs_out_reg_write(ENCI_TST_CR, 0x0);
		cvbs_out_reg_write(ENCI_TST_EN, 1);
		pr_info("show bist pattern %d: %s\n",
			bist, cvbs_out_bist_str[bist]);
		break;
	case 8:
		cvbs_out_reg_write(ENCI_TST_CLRBAR_STRT, 0x112);
		cvbs_out_reg_write(ENCI_TST_CLRBAR_WIDTH, 0xb4);
		cvbs_out_reg_write(ENCI_TST_MDSEL, 0);
		cvbs_out_reg_write(ENCI_TST_Y, 0x0);
		cvbs_out_reg_write(ENCI_TST_CB, 0x200);
		cvbs_out_reg_write(ENCI_TST_CR, 0x200);
		cvbs_out_reg_write(ENCI_TST_EN, 1);
		pr_info("show bist pattern %d: %s\n",
			bist, cvbs_out_bist_str[bist]);
		break;
	case 0:
	default:
		cvbs_out_reg_write(ENCI_TST_MDSEL, 1);
		cvbs_out_reg_write(ENCI_TST_Y, 0x200);
		cvbs_out_reg_write(ENCI_TST_CB, 0x200);
		cvbs_out_reg_write(ENCI_TST_CR, 0x200);
		cvbs_out_reg_write(ENCI_TST_EN, 0);
		pr_info("show bist pattern %d: %s\n",
			bist, cvbs_out_bist_str[0]);
		break;
	}
}

static ssize_t aml_CVBS_attr_vdac_power_show(struct class *class,
					     struct class_attribute *attr,
					     char *buf)
{
	return snprintf(buf, 40, "%s\n", "vdac_power_level");
}

static void vdac_power_level_store(const char *para)
{
	unsigned long level = 0;
	int ret = 0;

	ret = kstrtoul(para, 10, (unsigned long *)&level);
	cvbs_vdac_power_level = level;
}

static ssize_t aml_CVBS_attr_vdac_power_store(struct class *class,
					      struct class_attribute *attr,
					      const char *buf, size_t count)
{
	vdac_power_level_store(buf);

	return strnlen(buf, count);
}

static void dump_clk_registers(void)
{
	struct meson_cvbsout_data *cvbs_data;
	unsigned int vdac_reg0, vdac_reg1;
		/* hiu 10c8 ~ 10cd */

		cvbs_data = get_cvbs_data();
		if (!cvbs_data)
			return;
		/* hiu 104a, 104b*/

	pr_info("----hiu----\n");
	pr_info("0x%x", cvbs_out_vid_pll_read(cvbs_data->reg_vid_pll_clk_div));
	pr_info("0x%x", cvbs_out_hiu_read(cvbs_data->reg_vid_clk_div));
	pr_info("0x%x", cvbs_out_hiu_read(cvbs_data->reg_vid_clk_ctrl));
	pr_info("0x%x", cvbs_out_hiu_read(cvbs_data->reg_vid2_clk_div));
	pr_info("0x%x", cvbs_out_hiu_read(cvbs_data->reg_vid2_clk_ctrl));
	pr_info("0x%x", cvbs_out_hiu_read(cvbs_data->reg_vid_clk_ctrl2));
	pr_info("----hiu----\n");

	pr_info("------------------------\n");
	vdac_reg0 = vdac_get_reg_addr(0);
	vdac_reg1 = vdac_get_reg_addr(1);
	if (vdac_reg0 < 0x1000 && vdac_reg1 < 0x1000) {
		pr_info("hiu [0x%x] = 0x%x\n"
			"hiu [0x%x] = 0x%x\n",
			vdac_reg0, cvbs_out_ana_read(vdac_reg0),
			vdac_reg1, cvbs_out_ana_read(vdac_reg1));
	}
	pr_info("------------------------\n");
}

static void cvbs_performance_regs_dump(void)
{
	unsigned int performance_regs_enci[] = {
			VENC_VDAC_DAC0_GAINCTRL,
			ENCI_SYNC_ADJ,
			ENCI_VIDEO_BRIGHT,
			ENCI_VIDEO_CONT,
			ENCI_VIDEO_SAT,
			ENCI_VIDEO_HUE,
			ENCI_YC_DELAY,
			VENC_VDAC_DAC0_FILT_CTRL0,
			VENC_VDAC_DAC0_FILT_CTRL1
		};
	unsigned int vdac_reg0, vdac_reg1;
	int i, size;

	size = sizeof(performance_regs_enci) / sizeof(unsigned int);
	pr_info("------------------------\n");
	for (i = 0; i < size; i++) {
		pr_info("vcbus [0x%x] = 0x%x\n", performance_regs_enci[i],
			cvbs_out_reg_read(performance_regs_enci[i]));
	}
	pr_info("------------------------\n");
	vdac_reg0 = vdac_get_reg_addr(0);
	vdac_reg1 = vdac_get_reg_addr(1);
	if (vdac_reg0 < 0x1000 && vdac_reg1 < 0x1000) {
		pr_info("vdac cntl0 [0x%x] = 0x%x\n"
			"vdac cntl1 [0x%x] = 0x%x\n",
			vdac_reg0, cvbs_out_ana_read(vdac_reg0),
			vdac_reg1, cvbs_out_ana_read(vdac_reg1));
	}
	pr_info("------------------------\n");
}

static void cvbs_performance_config_dump(void)
{
	struct performance_config_s *perfconf = NULL;
	const struct reg_s *s = NULL;
	int i = 0;

	perfconf = &cvbs_drv->perf_conf_pal;
	if (!perfconf->reg_table) {
		pr_info("no performance_pal table!\n");
	} else {
		pr_info("------------------------\n");
		pr_info("performance_pal config:\n");
		s = perfconf->reg_table;
		while (i < perfconf->reg_cnt) {
			if (s->reg > 0x1000)
				pr_info("0x%04x = 0x%x\n", s->reg, s->val);
			else
				pr_info("0x%02x = 0x%x\n", s->reg, s->val);
			s++;
			i++;
		}
		pr_info("------------------------\n");
	}

	i = 0;
	perfconf = &cvbs_drv->perf_conf_ntsc;
	if (!perfconf->reg_table) {
		pr_info("no performance_ntsc table!\n");
	} else {
		pr_info("------------------------\n");
		pr_info("performance_ntsc config:\n");
		s = perfconf->reg_table;
		while (i < perfconf->reg_cnt) {
			if (s->reg > 0x1000)
				pr_info("0x%04x = 0x%x\n", s->reg, s->val);
			else
				pr_info("0x%02x = 0x%x\n", s->reg, s->val);
			s++;
			i++;
		}
		pr_info("------------------------\n");
	}
}

enum {
	CMD_REG_READ,
	CMD_REG_READ_BITS,
	CMD_REG_DUMP,
	CMD_REG_WRITE,
	CMD_REG_WRITE_BITS,

	CMD_CLK_DUMP,
	CMD_CLK_MSR,

	CMD_BIST,

	/* config a set of performance parameters:
	 *0 for sarft,
	 *1 for telecom,
	 *2 for chinamobile
	 */
	CMD_VP_SET,

	/* get the current perfomance config */
	CMD_VP_GET,

	/* dump the perfomance config in dts */
	CMD_VP_CONFIG_DUMP,
	/*set pll path: 3:vid pll  2:gp0 pll path2*/
			/*1:gp0 pll path1*/
	CMD_VP_SET_PLLPATH,

	CMD_VDAC,

	CMD_HELP,

	CMD_MAX
} debug_cmd_t;

#define func_type_map(a) {\
	if (!strcmp(a, "h")) {\
		str_type = "hiu";\
		func_read = cvbs_out_hiu_read;\
		func_write = cvbs_out_hiu_write;\
		func_getb = cvbs_out_hiu_getb;\
		func_setb = cvbs_out_hiu_setb;\
	} \
	else if (!strcmp(a, "v")) {\
		str_type = "vcbus";\
		func_read = cvbs_out_reg_read;\
		func_write = cvbs_out_reg_write;\
		func_getb = cvbs_out_reg_getb;\
		func_setb = cvbs_out_reg_setb;\
	} \
	else if (!strcmp(a, "a")) {\
		str_type = "ana";\
		func_read = cvbs_out_ana_read;\
		func_write = cvbs_out_ana_write;\
		func_getb = cvbs_out_ana_getb;\
		func_setb = cvbs_out_ana_setb;\
	} \
}

static ssize_t aml_CVBS_attr_debug_show(struct class *class,
					struct class_attribute *attr,
					char *buf)
{
	return snprintf(buf, 40, "%s\n", "debug");
}

static void cvbs_debug_store(const char *buf)
{
	unsigned int ret = 0;
	unsigned long addr, start, end, value, length, old;
	unsigned int argc, bist;
	char *p = NULL, *para = NULL,
		*argv[6] = {NULL, NULL, NULL, NULL, NULL, NULL};
	char *str_type = NULL;
	unsigned int i, cmd;
	unsigned int (*func_read)(unsigned int)  = NULL;
	void (*func_write)(unsigned int, unsigned int) = NULL;
	unsigned int (*func_getb)(unsigned int,
				  unsigned int, unsigned int) = NULL;
	void (*func_setb)(unsigned int, unsigned int,
			  unsigned int, unsigned int) = NULL;

	p = kstrdup(buf, GFP_KERNEL);
	for (argc = 0; argc < 6; argc++) {
		para = strsep(&p, " ");
		if (!para)
			break;
		argv[argc] = para;
	}

	if (!strcmp(argv[0], "r")) {
		cmd = CMD_REG_READ;
	} else if (!strcmp(argv[0], "rb")) {
		cmd = CMD_REG_READ_BITS;
	} else if (!strcmp(argv[0], "dump")) {
		cmd = CMD_REG_DUMP;
	} else if (!strcmp(argv[0], "w")) {
		cmd = CMD_REG_WRITE;
	} else if (!strcmp(argv[0], "wb")) {
		cmd = CMD_REG_WRITE_BITS;
	} else if (!strncmp(argv[0], "clkdump", strlen("clkdump"))) {
		cmd = CMD_CLK_DUMP;
	} else if (!strncmp(argv[0], "clkmsr", strlen("clkmsr"))) {
		cmd = CMD_CLK_MSR;
	} else if (!strncmp(argv[0], "bist", strlen("bist"))) {
		cmd = CMD_BIST;
	} else if (!strncmp(argv[0], "vpset", strlen("vpset"))) {
		cmd = CMD_VP_SET;
	} else if (!strncmp(argv[0], "vpget", strlen("vpget"))) {
		cmd = CMD_VP_GET;
	} else if (!strncmp(argv[0], "vpconf", strlen("vpconf"))) {
		cmd = CMD_VP_CONFIG_DUMP;
	} else if ((!strncmp(argv[0], "set_clkpath", strlen("set_clkpath"))) ||
		   (!strncmp(argv[0], "clkpath", strlen("clkpath")))) {
		cmd = CMD_VP_SET_PLLPATH;
	} else if (!strncmp(argv[0], "vdac", strlen("vdac"))) {
		cmd = CMD_VDAC;
	} else if (!strncmp(argv[0], "help", strlen("help"))) {
		cmd = CMD_HELP;
	} else if (!strncmp(argv[0], "cvbs_ver", strlen("cvbs_ver"))) {
		pr_info("cvbsout version : %s\n", CVBSOUT_VER);
		goto DEBUG_END;
	} else {
		pr_info("[%s] invalid cmd = %s!\n", __func__, argv[0]);
		goto DEBUG_END;
	}

	switch (cmd) {
	case CMD_REG_READ:
		if (argc != 3) {
			pr_info("[%s] cmd_reg_read format: r c/h/v address_hex\n",
				__func__);
			goto DEBUG_END;
		}

		func_type_map(argv[1]);
		if (!func_read)
			goto DEBUG_END;
		ret = kstrtoul(argv[2], 16, &addr);

		pr_info("read %s[0x%x] = 0x%x\n",
			str_type, (unsigned int)addr, func_read(addr));

		break;

	case CMD_REG_READ_BITS:
		if (argc != 5) {
			pr_info("[%s] cmd_reg_read_bits format:\n"
			"\trb c/h/v address_hex start_dec length_dec\n",
				__func__);
			goto DEBUG_END;
		}

		func_type_map(argv[1]);
		if (!func_read)
			goto DEBUG_END;
		ret = kstrtoul(argv[2], 16, &addr);
		ret = kstrtoul(argv[3], 10, &start);
		ret = kstrtoul(argv[4], 10, &length);

		if (length == 1)
			pr_info("read_bits %s[0x%x] = 0x%x, bit[%d] = 0x%x\n",
				str_type, (unsigned int)addr,
				func_read(addr), (unsigned int)start,
				func_getb(addr, start, length));
		else
			pr_info("read_bits %s[0x%x] = 0x%x, bit[%d-%d] = 0x%x\n",
				str_type, (unsigned int)addr,
				func_read(addr),
				(unsigned int)start + (unsigned int)length - 1,
				(unsigned int)start,
				func_getb(addr, start, length));
		break;

	case CMD_REG_DUMP:
		if (argc != 4) {
			pr_info("[%s] cmd_reg_dump format: dump a/h/v start_dec end_dec\n",
				__func__);
			goto DEBUG_END;
		}

		func_type_map(argv[1]);
		if (!func_read)
			goto DEBUG_END;
		ret = kstrtoul(argv[2], 16, &start);
		ret = kstrtoul(argv[3], 16, &end);

		for (i = start; i <= end; i++)
			pr_info("%s[0x%x] = 0x%x\n",
				str_type, i, func_read(i));

		break;

	case CMD_REG_WRITE:
		if (argc != 4) {
			pr_info("[%s] cmd_reg_write format: w value_hex c/h/v address_hex\n",
				__func__);
			goto DEBUG_END;
		}

		func_type_map(argv[2]);
		if (!func_write)
			goto DEBUG_END;
		ret = kstrtoul(argv[1], 16, &value);
		ret = kstrtoul(argv[3], 16, &addr);

		func_write(addr, value);
		pr_info("write %s[0x%x] = 0x%x\n", str_type,
			(unsigned int)addr, (unsigned int)value);
		break;

	case CMD_REG_WRITE_BITS:
		if (argc != 6) {
			pr_info("[%s] cmd_reg_wrute_bits format:\n"
			"\twb value_hex c/h/v address_hex start_dec length_dec\n",
				__func__);
			goto DEBUG_END;
		}

		func_type_map(argv[2]);
		if (!func_read)
			goto DEBUG_END;
		ret = kstrtoul(argv[1], 16, &value);
		ret = kstrtoul(argv[3], 16, &addr);
		ret = kstrtoul(argv[4], 10, &start);
		ret = kstrtoul(argv[5], 10, &length);

		old = func_read(addr);
		func_setb(addr, value, start, length);
		pr_info("write_bits %s[0x%x] old = 0x%x, new = 0x%x\n",
			str_type, (unsigned int)addr,
			(unsigned int)old, func_read(addr));
		break;

	case CMD_CLK_DUMP:
		dump_clk_registers();
		break;

	case CMD_CLK_MSR:
		/* todo */
		pr_info("cvbs: debug_store: clk_msr todo!\n");
		break;

	case CMD_BIST:
		if (argc != 2) {
			pr_info("[%s] cmd_bist format:\n"
			"\tbist 1/2/3/4/5/6/7/8/0\n", __func__);
			goto DEBUG_END;
		}
		ret = kstrtouint(argv[1], 10, &bist);
		if (ret) {
			pr_info("cvbs: invalid bist\n");
			goto DEBUG_END;
		}
		cvbs_bist_test(bist);

		break;

	case CMD_VP_SET:
		if (cvbs_drv->vinfo->mode != VMODE_CVBS) {
			pr_info("NOT VMODE_CVBS,Return\n");
			return;
		}
		cvbs_performance_enhancement(local_cvbs_mode);
		break;

	case CMD_VP_GET:
		cvbs_performance_regs_dump();
		break;

	case CMD_VP_CONFIG_DUMP:
		pr_info("performance config in dts:\n");
		cvbs_performance_config_dump();
		break;
	case CMD_VP_SET_PLLPATH:
		if (cvbs_cpu_type() < CVBS_CPU_TYPE_G12A) {
			pr_info("ERR:Only after g12a/b chip supported\n");
			break;
		}
		if (argc != 2) {
			pr_info("[%s] set clkpath 0x0~0x3\n", __func__);
			goto DEBUG_END;
		}
		ret = kstrtoul(argv[1], 10, &value);
		if (value > 0x3) {
			pr_info("invalid clkpath, only support 0x0~0x3\n");
		} else {
			cvbs_clk_path = value;
			pr_info("set clkpath 0x%x\n", cvbs_clk_path);
		}
		pr_info("bit[0]: 0=vid_pll, 1=gp0_pll\n");
		pr_info("bit[1]: 0=vid2_clk, 1=vid1_clk\n");

		break;
	case CMD_VDAC:
		if (argc != 2) {
			pr_info("[%s] vdac state: %d\n",
				__func__,
				(cvbs_drv->flag & CVBS_FLAG_EN_VDAC) ? 1 : 0);
			goto DEBUG_END;
		}
		ret = kstrtoul(argv[1], 10, &value);
		if (value)
			cvbs_vdac_output(1);
		else
			cvbs_vdac_output(0);

		break;
	case CMD_HELP:
		pr_info("command format:\n"
		"\tr c/h/v address_hex\n"
		"\trb c/h/v address_hex start_dec length_dec\n"
		"\tdump c/h/v start_dec end_dec\n"
		"\tw value_hex c/h/v address_hex\n"
		"\twb value_hex c/h/v address_hex start_dec length_dec\n"
		"\tbist 0/1/2/3/off\n"
		"\tclkdump\n"
		"\tclkpath 0/1/2/3\n"
		"\tvdac 0/1\n"
		"\tcvbs_ver\n");
		break;
	}

DEBUG_END:
	kfree(p);
}

static ssize_t aml_CVBS_attr_debug_store(struct class *class,
					 struct class_attribute *attr,
					 const char *buf, size_t count)
{
	cvbs_debug_store(buf);

	return strnlen(buf, count);
}

static  struct  class_attribute   *cvbs_attr[] = {
	&class_CVBS_attr_vdac_power_level,
	&class_CVBS_attr_debug,
#ifdef CONFIG_AMLOGIC_WSS
	&class_CVBS_attr_wss,
#endif
};

static int create_cvbs_attr(struct cvbs_drv_s *cdrv)
{
	/* create base class for display */
	int i;
	int ret = 0;

	cdrv->base_class = class_create(THIS_MODULE, CVBS_CLASS_NAME);
	if (IS_ERR(cdrv->base_class)) {
		ret = PTR_ERR(cdrv->base_class);
		goto fail_create_class;
	}
	/* create class attr */
	for (i = 0; i < ARRAY_SIZE(cvbs_attr); i++) {
		if (class_create_file(cdrv->base_class, cvbs_attr[i]) < 0)
			goto fail_class_create_file;
	}
	/*cdev_init(cdrv->cdev, &am_cvbs_fops);*/
	cdrv->cdev = cdev_alloc();
	cdrv->cdev->ops = &am_cvbs_fops;
	cdrv->cdev->owner = THIS_MODULE;
	ret = cdev_add(cdrv->cdev, cdrv->devno, 1);
	if (ret)
		goto fail_add_cdev;
	cdrv->dev = device_create(cdrv->base_class, NULL, cdrv->devno,
				  NULL, CVBS_NAME);
	if (IS_ERR(cdrv->dev)) {
		ret = PTR_ERR(cdrv->dev);
		goto fail_create_device;
	} else {
		cvbs_log_info("create cdev %s\n", CVBS_NAME);
	}
	return 0;

fail_create_device:
	cvbs_log_info("[cvbs.] : cvbs device create error.\n");
	cdev_del(cdrv->cdev);
fail_add_cdev:
	cvbs_log_info("[cvbs.] : cvbs add device error.\n");
fail_class_create_file:
	cvbs_log_info("[cvbs.] : cvbs class create file error.\n");
	for (i = 0; i < ARRAY_SIZE(cvbs_attr); i++)
		class_remove_file(cdrv->base_class, cvbs_attr[i]);
	class_destroy(cdrv->base_class);
fail_create_class:
	cvbs_log_info("[cvbs.] : cvbs class create error.\n");
	unregister_chrdev_region(cdrv->devno, 1);
	return ret;
}

/* **************************************************** */
static char *cvbsout_performance_str[] = {
	"performance", /* default for pal */
	"performance_pal",
	"performance_ntsc",
};

static void cvbsout_get_config(struct device *dev)
{
	int ret = 0;
	unsigned int val, cnt, i, j;
	struct reg_s *s = NULL;
	const char *str;

	/*clk path*/
	/*bit[0]: 0=vid_pll, 1=gp0_pll*/
	/*bit[1]: 0=vid2_clk, 1=vid_clk*/
	ret = of_property_read_u32(dev->of_node, "clk_path", &val);
	if (!ret) {
		if (val > 0x3) {
			cvbs_log_err("error: invalid clk_path\n");
		} else {
			cvbs_clk_path = val;
			cvbs_log_info("clk path:0x%x\n", cvbs_clk_path);
		}
	}

	/* performance: PAL */
	cvbs_drv->perf_conf_pal.reg_cnt = 0;
	cvbs_drv->perf_conf_pal.reg_table = NULL;
	str = cvbsout_performance_str[1];
	ret = of_property_read_u32_index(dev->of_node, str, 0, &val);
	if (ret)
		str = cvbsout_performance_str[0];
	cnt = 0;
	while (cnt < CVBS_PERFORMANCE_CNT_MAX) {
		j = 2 * cnt;
		ret = of_property_read_u32_index(dev->of_node, str, j, &val);
		if (ret) {
			cnt = 0;
			break;
		}
		if (val == MREG_END_MARKER) /* ending */
			break;
		cnt++;
	}
	if (cnt >= CVBS_PERFORMANCE_CNT_MAX)
		cnt = 0;
	if (cnt > 0) {
		cvbs_log_info("find performance_pal config\n");
		cvbs_drv->perf_conf_pal.reg_table =
			kcalloc(cnt, sizeof(struct reg_s), GFP_KERNEL);
		if (!cvbs_drv->perf_conf_pal.reg_table) {
			cvbs_log_err("error: failed to alloc %s table\n", str);
			cnt = 0;
		}
		cvbs_drv->perf_conf_pal.reg_cnt = cnt;

		i = 0;
		s = cvbs_drv->perf_conf_pal.reg_table;
		while (i < cvbs_drv->perf_conf_pal.reg_cnt) {
			j = 2 * i;
			ret = of_property_read_u32_index(dev->of_node,
				str, j, &val);
			s->reg = val;
			j = 2 * i + 1;
			ret = of_property_read_u32_index(dev->of_node,
				str, j, &val);
			s->val = val;
			/*pr_info("%p: 0x%04x = 0x%x\n", s, s->reg, s->val);*/

			s++;
			i++;
		}
	}

	/* performance: NTSC */
	cvbs_drv->perf_conf_ntsc.reg_cnt = 0;
	cvbs_drv->perf_conf_ntsc.reg_table = NULL;
	cnt = 0;
	str = cvbsout_performance_str[2];
	while (cnt < CVBS_PERFORMANCE_CNT_MAX) {
		j = 2 * cnt;
		ret = of_property_read_u32_index(dev->of_node, str, j, &val);
		if (ret) {
			cnt = 0;
			break;
		}
		if (val == MREG_END_MARKER) /* ending */
			break;
		cnt++;
	}
	if (cnt >= CVBS_PERFORMANCE_CNT_MAX)
		cnt = 0;
	if (cnt > 0) {
		cvbs_log_info("find performance_ntsc config\n");
		cvbs_drv->perf_conf_ntsc.reg_table =
			kcalloc(cnt, sizeof(struct reg_s), GFP_KERNEL);
		if (!cvbs_drv->perf_conf_ntsc.reg_table) {
			cvbs_log_err("error: failed to alloc %s table\n", str);
			cnt = 0;
		}
		cvbs_drv->perf_conf_ntsc.reg_cnt = cnt;

		i = 0;
		s = cvbs_drv->perf_conf_ntsc.reg_table;
		while (i < cvbs_drv->perf_conf_ntsc.reg_cnt) {
			j = 2 * i;
			ret = of_property_read_u32_index(dev->of_node,
							 str, j, &val);
			s->reg = val;
			j = 2 * i + 1;
			ret = of_property_read_u32_index(dev->of_node,
							 str, j, &val);
			s->val = val;
			/*pr_info("%p: 0x%04x = 0x%x\n", s, s->reg, s->val);*/

			s++;
			i++;
		}
	}
}

static void cvbsout_clktree_probe(struct device *dev)
{
	cvbs_drv->clk_gate_state = 0;
	cvbs_drv->venci_top_gate = devm_clk_get(dev, "venci_top_gate");
	if (IS_ERR(cvbs_drv->venci_top_gate))
		cvbs_log_err("error: %s: clk venci_top_gate\n", __func__);

	cvbs_drv->venci_0_gate = devm_clk_get(dev, "venci_0_gate");
	if (IS_ERR(cvbs_drv->venci_0_gate))
		cvbs_log_err("error: %s: clk venci_0_gate\n", __func__);

	cvbs_drv->venci_1_gate = devm_clk_get(dev, "venci_1_gate");
	if (IS_ERR(cvbs_drv->venci_1_gate))
		cvbs_log_err("error: %s: clk venci_1_gate\n", __func__);

	cvbs_drv->vdac_clk_gate = devm_clk_get(dev, "vdac_clk_gate");
	if (IS_ERR(cvbs_drv->vdac_clk_gate))
		cvbs_log_err("error: %s: clk vdac_clk_gate\n", __func__);
}

static void cvbsout_clktree_remove(struct device *dev)
{
	if (!IS_ERR(cvbs_drv->venci_top_gate))
		devm_clk_put(dev, cvbs_drv->venci_top_gate);
	if (!IS_ERR(cvbs_drv->venci_0_gate))
		devm_clk_put(dev, cvbs_drv->venci_0_gate);
	if (!IS_ERR(cvbs_drv->venci_1_gate))
		devm_clk_put(dev, cvbs_drv->venci_1_gate);
	if (!IS_ERR(cvbs_drv->vdac_clk_gate))
		devm_clk_put(dev, cvbs_drv->vdac_clk_gate);
}

#ifdef CONFIG_OF
struct meson_cvbsout_data meson_g12a_cvbsout_data = {
	.cpu_id = CVBS_CPU_TYPE_G12A,
	.name = "meson-g12a-cvbsout",

	.vdac_vref_adj = 0x10,
	.vdac_gsw = 0x0,

	.reg_vid_pll_clk_div = HHI_VID_PLL_CLK_DIV,
	.reg_vid_clk_div = HHI_VID_CLK_DIV,
	.reg_vid_clk_ctrl = HHI_VID_CLK_CNTL,
	.reg_vid2_clk_div = HHI_VIID_CLK_DIV,
	.reg_vid2_clk_ctrl = HHI_VIID_CLK_CNTL,
	.reg_vid_clk_ctrl2 = HHI_VID_CLK_CNTL2,
};

struct meson_cvbsout_data meson_g12b_cvbsout_data = {
	.cpu_id = CVBS_CPU_TYPE_G12B,
	.name = "meson-g12b-cvbsout",

	.vdac_vref_adj = 0xf,
	.vdac_gsw = 0x0,

	.reg_vid_pll_clk_div = HHI_VID_PLL_CLK_DIV,
	.reg_vid_clk_div = HHI_VID_CLK_DIV,
	.reg_vid_clk_ctrl = HHI_VID_CLK_CNTL,
	.reg_vid2_clk_div = HHI_VIID_CLK_DIV,
	.reg_vid2_clk_ctrl = HHI_VIID_CLK_CNTL,
	.reg_vid_clk_ctrl2 = HHI_VID_CLK_CNTL2,
};

struct meson_cvbsout_data meson_tl1_cvbsout_data = {
	.cpu_id = CVBS_CPU_TYPE_TL1,
	.name = "meson-tl1-cvbsout",

	.vdac_vref_adj = 0x10,
	.vdac_gsw = 0x0,

	.reg_vid_pll_clk_div = HHI_VID_PLL_CLK_DIV,
	.reg_vid_clk_div = HHI_VID_CLK_DIV,
	.reg_vid_clk_ctrl = HHI_VID_CLK_CNTL,
	.reg_vid2_clk_div = HHI_VIID_CLK_DIV,
	.reg_vid2_clk_ctrl = HHI_VIID_CLK_CNTL,
	.reg_vid_clk_ctrl2 = HHI_VID_CLK_CNTL2,
};

struct meson_cvbsout_data meson_sm1_cvbsout_data = {
	.cpu_id = CVBS_CPU_TYPE_SM1,
	.name = "meson-sm1-cvbsout",

	.vdac_vref_adj = 0xf,
	.vdac_gsw = 0x0,

	.reg_vid_pll_clk_div = HHI_VID_PLL_CLK_DIV,
	.reg_vid_clk_div = HHI_VID_CLK_DIV,
	.reg_vid_clk_ctrl = HHI_VID_CLK_CNTL,
	.reg_vid2_clk_div = HHI_VIID_CLK_DIV,
	.reg_vid2_clk_ctrl = HHI_VIID_CLK_CNTL,
	.reg_vid_clk_ctrl2 = HHI_VID_CLK_CNTL2,
};

struct meson_cvbsout_data meson_tm2_cvbsout_data = {
	.cpu_id = CVBS_CPU_TYPE_TM2,
	.name = "meson-tm2-cvbsout",

	.vdac_vref_adj = 0x10,
	.vdac_gsw = 0x0,

	.reg_vid_pll_clk_div = HHI_VID_PLL_CLK_DIV,
	.reg_vid_clk_div = HHI_VID_CLK_DIV,
	.reg_vid_clk_ctrl = HHI_VID_CLK_CNTL,
	.reg_vid2_clk_div = HHI_VIID_CLK_DIV,
	.reg_vid2_clk_ctrl = HHI_VIID_CLK_CNTL,
	.reg_vid_clk_ctrl2 = HHI_VID_CLK_CNTL2,
};

struct meson_cvbsout_data meson_sc2_cvbsout_data = {
	.cpu_id = CVBS_CPU_TYPE_SC2,
	.name = "meson-sc2-cvbsout",

	.vdac_vref_adj = 0x10,
	.vdac_gsw = 0x0,

	.reg_vid_pll_clk_div = CLKCTRL_VID_PLL_CLK_DIV,
	.reg_vid_clk_div = CLKCTRL_VID_CLK_DIV,
	.reg_vid_clk_ctrl = CLKCTRL_VID_CLK_CTRL,
	.reg_vid2_clk_div = CLKCTRL_VIID_CLK_DIV,
	.reg_vid2_clk_ctrl = CLKCTRL_VIID_CLK_CTRL,
	.reg_vid_clk_ctrl2 = CLKCTRL_VID_CLK_CTRL2,
};

struct meson_cvbsout_data meson_t5_cvbsout_data = {
	.cpu_id = CVBS_CPU_TYPE_T5,
	.name = "meson-t5-cvbsout",

	.vdac_vref_adj = 0x10,
	.vdac_gsw = 0x5c,

	.reg_vid_pll_clk_div = HHI_VID_PLL_CLK_DIV,
	.reg_vid_clk_div = HHI_VID_CLK_DIV,
	.reg_vid_clk_ctrl = HHI_VID_CLK_CNTL,
	.reg_vid2_clk_div = HHI_VIID_CLK_DIV,
	.reg_vid2_clk_ctrl = HHI_VIID_CLK_CNTL,
	.reg_vid_clk_ctrl2 = HHI_VID_CLK_CNTL2,
};

struct meson_cvbsout_data meson_t5d_cvbsout_data = {
	.cpu_id = CVBS_CPU_TYPE_T5D,
	.name = "meson-t5d-cvbsout",

	.vdac_vref_adj = 0x10,
	.vdac_gsw = 0x5c,

	.reg_vid_pll_clk_div = HHI_VID_PLL_CLK_DIV,
	.reg_vid_clk_div = HHI_VID_CLK_DIV,
	.reg_vid_clk_ctrl = HHI_VID_CLK_CNTL,
	.reg_vid2_clk_div = HHI_VIID_CLK_DIV,
	.reg_vid2_clk_ctrl = HHI_VIID_CLK_CNTL,
	.reg_vid_clk_ctrl2 = HHI_VID_CLK_CNTL2,
};

struct meson_cvbsout_data meson_s4_cvbsout_data = {
	.cpu_id = CVBS_CPU_TYPE_S4,
	.name = "meson-s4-cvbsout",

	.vdac_vref_adj = 0x10,
	.vdac_gsw = 0x5c,

	.reg_vid_pll_clk_div = CLKCTRL_VID_PLL_CLK_DIV,
	.reg_vid_clk_div = CLKCTRL_VID_CLK_DIV,
	.reg_vid_clk_ctrl = CLKCTRL_VID_CLK_CTRL,
	.reg_vid2_clk_div = CLKCTRL_VIID_CLK_DIV,
	.reg_vid2_clk_ctrl = CLKCTRL_VIID_CLK_CTRL,
	.reg_vid_clk_ctrl2 = CLKCTRL_VID_CLK_CTRL2,
};

static const struct of_device_id meson_cvbsout_dt_match[] = {
	{
		.compatible = "amlogic, cvbsout-g12a",
		.data		= &meson_g12a_cvbsout_data,
	}, {
		.compatible = "amlogic, cvbsout-g12b",
		.data		= &meson_g12b_cvbsout_data,
	}, {
		.compatible = "amlogic, cvbsout-tl1",
		.data		= &meson_tl1_cvbsout_data,
	}, {
		.compatible = "amlogic, cvbsout-sm1",
		.data		= &meson_sm1_cvbsout_data,
	}, {
		.compatible = "amlogic, cvbsout-tm2",
		.data		= &meson_tm2_cvbsout_data,
	}, {
		.compatible = "amlogic, cvbsout-sc2",
		.data		= &meson_sc2_cvbsout_data,
	}, {
		.compatible = "amlogic, cvbsout-t5",
		.data		= &meson_t5_cvbsout_data,
	}, {
		.compatible = "amlogic, cvbsout-t5d",
		.data		= &meson_t5d_cvbsout_data,
	}, {
		.compatible = "amlogic, cvbsout-s4",
		.data		= &meson_s4_cvbsout_data,
	},
	{}
};
#endif

#ifdef CONFIG_AML_VOUT_CC_BYPASS
static irqreturn_t tvout_vsync_isr(int irq, void *dev_id)
{
	unsigned int CC_2byte_data;
	unsigned long flags = 0;
	struct vout_cc_parm_s parm = {0};

	spin_lock_irqsave(&tvout_clk_lock, flags);
	if (cc_ringbuf.rp != cc_ringbuf.wp) {
		parm.type = cc_ringbuf.cc_data[cc_ringbuf.rp].type;
		CC_2byte_data = cc_ringbuf.cc_data[cc_ringbuf.rp].data;
		vsync_empty_flag = 0;
		vsync_empty_flag_evn = 0;
		vsync_empty_flag_odd = 0;
	} else {
		if (vsync_empty_flag == 0) {
			if ((cvbs_out_reg_read(ENCI_INFO_READ) &
							0x20000000) == 0x0) {
				cvbs_out_reg_write(ENCI_VBI_CCDT_EVN, 0x8080);
				vsync_empty_flag_evn = 1;
			} else {
				cvbs_out_reg_write(ENCI_VBI_CCDT_ODD, 0x8080);
				vsync_empty_flag_odd = 1;
			}
			vsync_empty_flag = vsync_empty_flag_evn &
					vsync_empty_flag_odd;
		}
		spin_unlock_irqrestore(&tvout_clk_lock, flags);
		return IRQ_HANDLED;
	}

	if (parm.type == 0) {
		if ((((CC_2byte_data >> 8) & 0x7f) >= 0x1) &&
				(((CC_2byte_data >> 8) & 0x7f) < 0x10)) {
			if ((cvbs_out_reg_read(ENCI_INFO_READ) &
							0x20000000) != 0x0) {
				cvbs_out_reg_write(ENCI_VBI_CCDT_ODD,
								CC_2byte_data);
				if (((cvbs_out_reg_read(ENCI_INFO_READ) >> 16) &
							0xff) <= 0x15)
					cc_ringbuf.rp = (cc_ringbuf.rp +
							1) % MAX_RING_BUFF_LEN;
			} else {
				cvbs_out_reg_write(ENCI_VBI_CCDT_ODD, 0x8080);
			}
		} else {
			if ((cvbs_out_reg_read(ENCI_INFO_READ) &
						0x20000000) == 0x0){
				cvbs_out_reg_write(ENCI_VBI_CCDT_EVN,
						CC_2byte_data);
				if (((cvbs_out_reg_read(ENCI_INFO_READ) >>
							16) & 0xff) <= 0x15)
					cc_ringbuf.rp = (cc_ringbuf.rp +
						1) % MAX_RING_BUFF_LEN;
			} else {
				cvbs_out_reg_write(ENCI_VBI_CCDT_EVN, 0x8080);
			}
		}
	} else {
		cvbs_log_err("vsync_isr.type:%d Unknown\n", parm.type);
	}
	spin_unlock_irqrestore(&tvout_clk_lock, flags);
	return IRQ_HANDLED;
}
#endif

static int cvbsout_probe(struct platform_device *pdev)
{
	int  ret;
	const struct of_device_id *match;

	cvbs_clk_path = 0;
	local_cvbs_mode = MODE_MAX;

#ifdef CONFIG_AML_VOUT_CC_BYPASS
	memset(&cc_ringbuf, 0, sizeof(struct cc_ring_mgr_s));
	cc_ringbuf.max_len = MAX_RING_BUFF_LEN;
	spin_lock_init(&tvout_clk_lock);
#endif

	cvbs_drv = kzalloc(sizeof(*cvbs_drv), GFP_KERNEL);
	if (!cvbs_drv)
		return -ENOMEM;

	match = of_match_device(meson_cvbsout_dt_match, &pdev->dev);
	if (!match) {
		cvbs_log_err("%s, no matched table\n", __func__);
		goto cvbsout_probe_err;
	}
	cvbs_drv->cvbs_data = (struct meson_cvbsout_data *)match->data;
	cvbs_log_info("%s, cpu_id:%d,name:%s\n", __func__,
		cvbs_drv->cvbs_data->cpu_id, cvbs_drv->cvbs_data->name);

	if (cvbs_drv->cvbs_data->cpu_id != CVBS_CPU_TYPE_SC2 &&
	    cvbs_drv->cvbs_data->cpu_id != CVBS_CPU_TYPE_S4)
		cvbsout_clktree_probe(&pdev->dev);

	cvbsout_get_config(&pdev->dev);
	cvbs_init_vout();

#ifdef CONFIG_AMLOGIC_VPU
	/*vpu gate register for cvbs*/
	cvbs_drv->cvbs_vpu_dev = vpu_dev_register(VPU_VENCI, CVBS_NAME);
#endif

	ret = alloc_chrdev_region(&cvbs_drv->devno, 0, 1, CVBS_NAME);
	if (ret < 0) {
		cvbs_log_err("alloc_chrdev_region error\n");
		goto cvbsout_probe_err;
	}
	cvbs_log_err("chrdev devno %d for disp\n", cvbs_drv->devno);
	ret = create_cvbs_attr(cvbs_drv);
	if (ret < 0) {
		cvbs_log_err("create_cvbs_attr error\n");
		goto cvbsout_probe_err;
	}

#ifdef CONFIG_AML_VOUT_CC_BYPASS
	if (request_irq(INT_VIU_VSYNC, &tvout_vsync_isr,
		IRQF_SHARED, "tvout_vsync", (void *)"tvout_vsync")) {
		cvbs_log_err("can't request vsync_irq for tvout\n");
	} else {
		cvbs_log_info("request tvout vsync_irq successful\n");
	}
#endif

	INIT_DELAYED_WORK(&cvbs_drv->vdac_dwork, cvbs_vdac_dwork);
	cvbs_log_info("%s OK\n", __func__);
	return 0;

cvbsout_probe_err:
	kfree(cvbs_drv);
	cvbs_drv = NULL;
	return -1;
}

static int cvbsout_remove(struct platform_device *pdev)
{
	int i;

#ifdef CONFIG_AML_VOUT_CC_BYPASS
	free_irq(INT_VIU_VSYNC, (void *)"tvout_vsync");
#endif
	cvbsout_clktree_remove(&pdev->dev);

	if (cvbs_drv->base_class) {
		for (i = 0; i < ARRAY_SIZE(cvbs_attr); i++)
			class_remove_file(cvbs_drv->base_class, cvbs_attr[i]);
		class_destroy(cvbs_drv->base_class);
	}
	if (cvbs_drv) {
		cdev_del(cvbs_drv->cdev);
#ifdef CONFIG_AMLOGIC_VPU
	/*vpu gate unregister for cvbs*/
	vpu_dev_unregister(cvbs_drv->cvbs_vpu_dev);
#endif
		kfree(cvbs_drv);
	}
	vout_unregister_server(&cvbs_vout_server);
#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
	vout2_unregister_server(&cvbs_vout2_server);
#endif
	cvbs_log_info("%s\n", __func__);
	return 0;
}

static void cvbsout_shutdown(struct platform_device *pdev)
{
	if ((cvbs_drv->flag & CVBS_FLAG_EN_ENCI) == 0)
		return;

	if (cvbs_drv->flag & CVBS_FLAG_EN_VDAC)
		cvbs_vdac_output(0);

	cvbs_drv->flag &= ~CVBS_FLAG_EN_ENCI;
	cvbs_out_reg_write(ENCI_VIDEO_EN, 0);
	cvbs_out_disable_clk();

	cvbs_out_vpu_power_ctrl(0);
	cvbs_out_clk_gate_ctrl(0);
	cvbs_log_info("%s\n", __func__);
}

static struct platform_driver cvbsout_driver = {
	.probe = cvbsout_probe,
	.remove = cvbsout_remove,
	.shutdown = cvbsout_shutdown,
	.driver = {
		.name = "cvbsout",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = meson_cvbsout_dt_match,
#endif
	},
};

int __init cvbs_init_module(void)
{
	/* cvbs_log_info("%s module init\n", __func__); */
	if (platform_driver_register(&cvbsout_driver)) {
		cvbs_log_err("%s failed to register module\n", __func__);
		return -ENODEV;
	}
#ifdef CONFIG_AML_VOUT_CC_BYPASS
	memset(&cc_ringbuf, 0, sizeof(struct cc_ring_mgr_s));
	cc_ringbuf.max_len = MAX_RING_BUFF_LEN;
#endif
	return 0;
}

__exit void cvbs_exit_module(void)
{
	/* cvbs_log_info("%s module exit\n", __func__); */
	platform_driver_unregister(&cvbsout_driver);
}

//MODULE_AUTHOR("Platform-BJ <platform.bj@amlogic.com>");
//MODULE_DESCRIPTION("TV Output Module");
//MODULE_LICENSE("GPL");

