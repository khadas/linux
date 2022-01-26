// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * drivers/amlogic/media/frc/frc_drv.c
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
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_fdt.h>
#include <linux/of_reserved_mem.h>
#include <linux/of_irq.h>
#include <linux/of_clk.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/errno.h>
#include <linux/uaccess.h>
#include <linux/ioport.h>
#include <linux/ctype.h>
#include <linux/vmalloc.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/clk.h>
#include <linux/sched/clock.h>
#include <linux/pm_runtime.h>
#include <linux/pm_domain.h>

#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/amlogic/media/frc/frc_reg.h>
#include <linux/amlogic/media/frc/frc_common.h>
#include <linux/amlogic/power_domain.h>
#include <dt-bindings/power/t3-pd.h>
#include <linux/amlogic/media/video_sink/video_signal_notify.h>

#include "frc_drv.h"
#include "frc_proc.h"
#include "frc_dbg.h"
#include "frc_buf.h"
#include "frc_hw.h"
#ifdef CONFIG_AMLOGIC_MEDIA_FRC_RDMA
#include "frc_rdma.h"
#endif

// static struct frc_dev_s *frc_dev; // for SWPL-53056:KASAN: use-after-free
static struct frc_dev_s frc_dev;

int frc_dbg_en;
EXPORT_SYMBOL(frc_dbg_en);
module_param(frc_dbg_en, int, 0664);
MODULE_PARM_DESC(frc_dbg_en, "frc debug level");

struct frc_dev_s *get_frc_devp(void)
{
	// return frc_dev;
	return &frc_dev;
}

int  frc_kerdrv_ver = FRC_KERDRV_VER;
EXPORT_SYMBOL(frc_kerdrv_ver);

// static struct frc_fw_data_s *fw_data;
struct frc_fw_data_s fw_data;  // important 2021_0510
static const char frc_alg_defver[] = "alg_ver:default";

struct frc_fw_data_s *frc_get_fw_data(void)
{
	return &fw_data;
}
EXPORT_SYMBOL(frc_get_fw_data);

u32 sizeof_frc_fw_data_struct(void)
{
	return sizeof(struct frc_fw_data_s);
}
EXPORT_SYMBOL(sizeof_frc_fw_data_struct);

ssize_t frc_reg_show(struct class *class,
	struct class_attribute *attr,
	char *buf)
{
	pr_frc(0, "read:  echo r reg > /sys/class/frc/reg\n");
	pr_frc(0, "write: echo w reg value > /sys/class/frc/reg\n");
	pr_frc(0, "dump:  echo d reg length > /sys/class/frc/reg\n");
	return 0;
}

ssize_t frc_reg_store(struct class *class,
	struct class_attribute *attr,
	const char *buf,
	size_t count)
{
	frc_reg_io(buf);
	return count;
}

ssize_t frc_tool_debug_show(struct class *class,
	struct class_attribute *attr,
	char *buf)
{
	struct tool_debug_s *read_parm = NULL;
	struct frc_dev_s *devp = get_frc_devp();

	read_parm = &devp->tool_dbg;
	return sprintf(buf, "[0x%x] = 0x%x\n",
		read_parm->reg_read, read_parm->reg_read_val);
}

ssize_t frc_tool_debug_store(struct class *class,
	struct class_attribute *attr,
	const char *buf,
	size_t count)
{
	struct frc_dev_s *devp = get_frc_devp();

	frc_tool_dbg_store(devp, buf);
	return count;
}

ssize_t frc_debug_show(struct class *class,
	struct class_attribute *attr,
	char *buf)
{
	struct frc_dev_s *devp = get_frc_devp();

	return frc_debug_if_help(devp, buf);
}

ssize_t frc_debug_store(struct class *class,
	struct class_attribute *attr,
	const char *buf,
	size_t count)
{
	struct frc_dev_s *devp = get_frc_devp();

	frc_debug_if(devp, buf, count);

	return count;
}

static struct class_attribute frc_class_attrs[] = {
	__ATTR(debug, 0644, frc_debug_show, frc_debug_store),
	__ATTR(reg, 0644, frc_reg_show, frc_reg_store),
	__ATTR(tool_debug, 0644, frc_tool_debug_show, frc_tool_debug_store),
	__ATTR(final_line_param, 0644, frc_bbd_final_line_param_show,
		frc_bbd_final_line_param_store),
	__ATTR(vp_ctrl_param, 0644, frc_vp_ctrl_param_show, frc_vp_ctrl_param_store),
	__ATTR(logo_ctrl_param, 0644, frc_logo_ctrl_param_show, frc_logo_ctrl_param_store),
	__ATTR(iplogo_ctrl_param, 0644, frc_iplogo_ctrl_param_show, frc_iplogo_ctrl_param_store),
	__ATTR(melogo_ctrl_param, 0644, frc_melogo_ctrl_param_show, frc_melogo_ctrl_param_store),
	__ATTR(sence_chg_detect_param, 0644, frc_sence_chg_detect_param_show,
		frc_sence_chg_detect_param_store),
	__ATTR(fb_ctrl_param, 0644, frc_fb_ctrl_param_show, frc_fb_ctrl_param_store),
	__ATTR(me_ctrl_param, 0644, frc_me_ctrl_param_show, frc_me_ctrl_param_store),
	__ATTR(search_rang_param, 0644, frc_search_rang_param_show, frc_search_rang_param_store),
	__ATTR(pixel_lpf_param, 0644, frc_pixel_lpf_param_show, frc_pixel_lpf_param_store),
	__ATTR(me_rule_param, 0644, frc_me_rule_param_show, frc_me_rule_param_store),
	__ATTR(film_ctrl_param, 0644, frc_film_ctrl_param_show, frc_film_ctrl_param_store),
	__ATTR(glb_ctrl_param, 0644, frc_glb_ctrl_param_show, frc_glb_ctrl_param_store),

	__ATTR_NULL
};

static int frc_open(struct inode *inode, struct file *file)
{
	struct frc_dev_s *frc_devp;

	frc_devp = container_of(inode->i_cdev, struct frc_dev_s, cdev);
	file->private_data = frc_devp;

	return 0;
}

static int frc_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

static long frc_ioctl(struct file *file,
	unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	struct frc_dev_s *devp;
	void __user *argp = (void __user *)arg;
	u32 data;
	u8  tmpver[32];
	enum frc_fpp_state_e fpp_state;

	devp = file->private_data;
	if (!devp)
		return -EFAULT;

	if (!devp->probe_ok)
		return -EFAULT;

	switch (cmd) {
	case FRC_IOC_GET_FRC_EN:
		data = devp->frc_en;
		if (copy_to_user(argp, &data, sizeof(u32)))
			ret = -EFAULT;
		break;

	case FRC_IOC_GET_FRC_STS:
		data = (u32)devp->frc_sts.state;
		if (copy_to_user(argp, &data, sizeof(u32)))
			ret = -EFAULT;
		break;

	case FRC_IOC_SET_FRC_CANDENCE:
		if (copy_from_user(&data, argp, sizeof(u32))) {
			ret = -EFAULT;
			break;
		}
		pr_frc(1, "SET_FRC_CANDENCE:%d\n", data);
		break;

	case FRC_IOC_GET_VIDEO_LATENCY:
		data = (u32)frc_get_video_latency();
		if (copy_to_user(argp, &data, sizeof(u32)))
			ret = -EFAULT;
		break;

	case FRC_IOC_GET_IS_ON:
		data = (u32)frc_is_on();
		if (copy_to_user(argp, &data, sizeof(u32)))
			ret = -EFAULT;
		break;

	case FRC_IOC_SET_INPUT_VS_RATE:
		if (copy_from_user(&data, argp, sizeof(u32))) {
			ret = -EFAULT;
			break;
		}
		pr_frc(1, "SET_INPUT_VS_RATE:%d\n", data);
		break;

	case FRC_IOC_SET_MEMC_ON_OFF:
		if (copy_from_user(&data, argp, sizeof(u32))) {
			ret = -EFAULT;
			break;
		}
		pr_frc(1, "set memc_autoctrl:%d\n", data);
		if (data) {
			if (!devp->frc_sts.auto_ctrl) {
				devp->frc_sts.auto_ctrl = true;
				devp->frc_sts.re_config = true;
			}
		} else {
			devp->frc_sts.auto_ctrl = false;
			if (devp->frc_sts.state == FRC_STATE_ENABLE)
				frc_change_to_state(FRC_STATE_DISABLE);
		}
		break;

	case FRC_IOC_SET_MEMC_LEVEL:
		if (copy_from_user(&data, argp, sizeof(u32))) {
			ret = -EFAULT;
			break;
		}
		frc_memc_set_level(data);
		// pr_frc(1, "SET_MEMC_LEVEL:%d\n", data);
		break;

	case FRC_IOC_SET_MEMC_DMEO_MODE:
		if (copy_from_user(&data, argp, sizeof(u32))) {
			ret = -EFAULT;
			break;
		}
		frc_memc_set_demo(data);
		// pr_frc(1, "SET_MEMC_DEMO:%d\n", data);
		break;

	case FRC_IOC_SET_FPP_MEMC_LEVEL:
		if (copy_from_user(&fpp_state, argp,
			sizeof(enum frc_fpp_state_e))) {
			pr_frc(1, "fpp copy from user error!/n");
			ret = -EFAULT;
			break;
		}
		if (fpp_state == FPP_MEMC_OFF)
			frc_fpp_memc_set_level(0, 0);
		else if (fpp_state == FPP_MEMC_LOW)
			frc_fpp_memc_set_level(9, 0);
		else if (fpp_state == FPP_MEMC_MID)
			frc_fpp_memc_set_level(10, 0);
		else if (fpp_state == FPP_MEMC_HIGH)
			frc_fpp_memc_set_level(10, 1);
		else
			frc_fpp_memc_set_level((u8)fpp_state, 0);

		pr_frc(1, "SET_FPP_MEMC_LEVEL:%d\n", fpp_state);
		break;
	case FRC_IOC_SET_MEMC_VENDOR:
		if (copy_from_user(&data, argp, sizeof(u32))) {
			ret = -EFAULT;
			break;
		}
		frc_tell_alg_vendor(data);
		break;
	case FRC_IOC_SET_MEMC_FB:
		if (copy_from_user(&data, argp, sizeof(u32))) {
			ret = -EFAULT;
			break;
		}
		frc_set_memc_fallback(data);
		break;
	case FRC_IOC_SET_MEMC_FILM:
		if (copy_from_user(&data, argp, sizeof(u32))) {
			ret = -EFAULT;
			break;
		}
		frc_set_film_support(data);
		break;
	case FRC_IOC_GET_MEMC_VERSION:
		strncpy(&tmpver[0], &fw_data.frc_alg_ver[0], sizeof(u8) * 32);
		if (copy_to_user(argp, tmpver, sizeof(u8) * 32))
			ret = -EFAULT;
		break;
	}

	return ret;
}

static const struct dts_match_data dts_match_t3 = {
	.chip = ID_T3,
};

static const struct of_device_id frc_dts_match[] = {
	{
		.compatible = "amlogic, t3_frc",
		.data = &dts_match_t3,
	},
};

static int frc_attach_pd(struct frc_dev_s *devp)
{
	struct platform_device *pdev = devp->pdev;
	struct device_link *link;
	char *pd_name[3] = {"frc-top", "frc-me", "frc-mc"};
	int i;
	u32 pd_cnt = 3;

	if (pdev->dev.pm_domain) {
		pr_frc(0, "%s err pm domain\n", __func__);
		return -1;
	}

	for (i = 0; i < pd_cnt; i++) {
		devp->pd_dev = dev_pm_domain_attach_by_name(&pdev->dev, pd_name[i]);
		if (IS_ERR(devp->pd_dev))
			return PTR_ERR(devp->pd_dev);
		if (!devp->pd_dev)
			return -1;

		link = device_link_add(&pdev->dev, devp->pd_dev,
				       DL_FLAG_STATELESS | DL_FLAG_PM_RUNTIME | DL_FLAG_RPM_ACTIVE);
		if (!link) {
			pr_frc(0, "%s fail to add device_link idx (%d) pd\n", __func__, i);
			return -EINVAL;
		}
		//pr_frc(1, "pw domain %s attach\n", pd_name[i]);
	}
	return 0;
}

void frc_power_domain_ctrl(struct frc_dev_s *devp, u32 onoff)
{
	struct frc_data_s *frc_data;
	struct frc_fw_data_s *pfw_data;
	enum chip_id chip;

	frc_data = (struct frc_data_s *)devp->data;
	pfw_data = (struct frc_fw_data_s *)devp->fw_data;
	chip = frc_data->match_data->chip;

#define K_MEMC_CLK_DIS

	if (devp->power_on_flag == onoff) {
		pr_frc(0, "warning: same pw state\n");
		return;
	}
	if (!onoff) {
		// devp->power_on_flag = false;
		frc_change_to_state(FRC_STATE_BYPASS);
		set_frc_enable(false);
		set_frc_bypass(true);
		frc_state_change_finish(devp);
		devp->power_on_flag = false;
		// devp->power_off_flag = 0;
	}

	if (chip == ID_T3) {
		if (onoff) {
#ifdef K_MEMC_CLK_DIS
			pwr_ctrl_psci_smc(PDID_T3_FRCTOP, PWR_ON);
			pwr_ctrl_psci_smc(PDID_T3_FRCME, PWR_ON);
			pwr_ctrl_psci_smc(PDID_T3_FRCMC, PWR_ON);
#endif
			devp->power_on_flag = true;
			frc_init_config(devp);
			frc_buf_config(devp);
			frc_internal_initial(devp);
			frc_hw_initial(devp);
			if (pfw_data->frc_fw_reinit)
				pfw_data->frc_fw_reinit();
		} else {
#ifdef K_MEMC_CLK_DIS
			pwr_ctrl_psci_smc(PDID_T3_FRCTOP, PWR_OFF);
			pwr_ctrl_psci_smc(PDID_T3_FRCME, PWR_OFF);
			pwr_ctrl_psci_smc(PDID_T3_FRCMC, PWR_OFF);
#endif
		}
		pr_frc(0, "t3 power domain power %d\n", onoff);
	}
	// if (onoff)
	//	devp->power_on_flag = true;
}

static int frc_dts_parse(struct frc_dev_s *frc_devp)
{
	struct device_node *of_node;
	unsigned int val;
	int ret = 0;
	const struct of_device_id *of_id;
	struct platform_device *pdev = frc_devp->pdev;
	struct resource *res;
	resource_size_t *base;
	struct frc_data_s *frc_data;

	of_node = pdev->dev.of_node;
	of_id = of_match_device(frc_dts_match, &pdev->dev);
	if (of_id) {
		PR_FRC("%s\n", of_id->compatible);
		frc_data = frc_devp->data;
		frc_data->match_data = of_id->data;
		PR_FRC("chip id:%d\n", frc_data->match_data->chip);
	}

	if (of_node) {
		ret = of_property_read_u32(of_node, "frc_en", &val);
		if (ret) {
			PR_FRC("Can't find frc_en.\n");
			frc_devp->frc_en = 0;
		} else {
			frc_devp->frc_en = val;
		}
		ret = of_property_read_u32(of_node, "frc_hw_pos", &val);
		if (ret)
			PR_FRC("Can't find frc_hw_pos.\n");
		else
			frc_devp->frc_hw_pos = val;
	}
	pr_frc(0, "frc_en:%d, frc_hw_pos:%d\n", frc_devp->frc_en, frc_devp->frc_hw_pos);
	/*get irq number from dts*/
	frc_devp->in_irq = of_irq_get_byname(of_node, "irq_frc_in");
	snprintf(frc_devp->in_irq_name, sizeof(frc_devp->in_irq_name), "frc_input_irq");
	PR_FRC("%s=%d\n", frc_devp->in_irq_name, frc_devp->in_irq);
	if (frc_devp->in_irq > 0) {
		ret = request_irq(frc_devp->in_irq, frc_input_isr, IRQF_SHARED,
				  frc_devp->in_irq_name, (void *)frc_devp);
		if (ret)
			PR_ERR("request in irq fail\n");
		else
			disable_irq(frc_devp->in_irq);
	}

	frc_devp->out_irq = of_irq_get_byname(of_node, "irq_frc_out");
	snprintf(frc_devp->out_irq_name, sizeof(frc_devp->out_irq_name), "frc_out_irq");
	PR_FRC("%s=%d\n", frc_devp->out_irq_name, frc_devp->out_irq);
	if (frc_devp->out_irq > 0) {
		ret = request_irq(frc_devp->out_irq, frc_output_isr, IRQF_SHARED,
				  frc_devp->out_irq_name, (void *)frc_devp);
		if (ret)
			PR_ERR("request out irq fail\n");
		else
			disable_irq(frc_devp->out_irq);
	}

	frc_devp->rdma_irq = of_irq_get_byname(of_node, "irq_frc_rdma");
	snprintf(frc_devp->rdma_irq_name, sizeof(frc_devp->rdma_irq_name), "frc_rdma_irq");
	PR_FRC("%s=%d\n", frc_devp->rdma_irq_name, frc_devp->rdma_irq);
#ifdef CONFIG_AMLOGIC_MEDIA_FRC_RDMA
	if (frc_devp->rdma_irq > 0) {
		ret = request_irq(frc_devp->rdma_irq, frc_rdma_isr, IRQF_SHARED,
				  frc_devp->rdma_irq_name, (void *)frc_devp);
		if (ret)
			PR_ERR("request rdma irq fail\n");
		else
			disable_irq(frc_devp->rdma_irq);
	}
#endif
	/*register map*/
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "frc_reg");
	if (res) {
		base = devm_ioremap(&pdev->dev, res->start, res->end - res->start);
		if (!base) {
			PR_ERR("Unable to map reg base\n");
			frc_devp->reg = NULL;
			frc_base = NULL;
		} else {
			frc_devp->reg = (void *)base;
			frc_base = frc_devp->reg;
			pr_frc(0, "frc reg base 0x%lx -> 0x%lx, map size:0x%lx\n",
			       (ulong)res->start, (ulong)frc_base, (ulong)(res->end - res->start));
		}
	} else {
		frc_devp->reg = NULL;
		frc_base = NULL;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "frc_clk_reg");
	if (res) {
		base = devm_ioremap(&pdev->dev, res->start, res->end - res->start);
		if (!base) {
			PR_ERR("Unable to map frc clk reg base\n");
			frc_devp->clk_reg = NULL;
			frc_clk_base = NULL;
		} else {
			frc_devp->clk_reg = (void *)base;
			frc_clk_base = frc_devp->clk_reg;
			pr_frc(0, "clk reg base 0x%lx -> 0x%lx, map size:0x%lx\n",
			       (ulong)res->start, (ulong)frc_clk_base,
			       (ulong)(res->end - res->start));
		}
	} else {
		frc_devp->clk_reg = NULL;
		frc_clk_base = NULL;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "vpu_reg");
	if (res) {
		base = devm_ioremap(&pdev->dev, res->start, res->end - res->start);
		if (!base) {
			PR_ERR("Unable to map vpu reg base\n");
			frc_devp->vpu_reg = NULL;
			vpu_base = NULL;
		} else {
			frc_devp->vpu_reg = (void *)base;
			vpu_base = frc_devp->vpu_reg;
			pr_frc(0, "vpu reg base 0x%lx -> 0x%lx, map size:0x%lx\n",
			       (ulong)res->start, (ulong)vpu_base, (ulong)(res->end - res->start));
		}
	} else {
		frc_devp->vpu_reg = NULL;
		vpu_base = NULL;
	}

	ret = of_reserved_mem_device_init(&pdev->dev);
	if (ret) {
		pr_frc(0, "resource undefined !\n");
		frc_devp->buf.cma_mem_size = 0;
	}
	frc_devp->buf.cma_mem_size = dma_get_cma_size_int_byte(&pdev->dev);
	pr_frc(0, "cma_mem_size=0x%x\n", frc_devp->buf.cma_mem_size);

	frc_devp->clk_frc = clk_get(&pdev->dev, "clk_frc");
	frc_devp->clk_me = clk_get(&pdev->dev, "clk_me");
	if (IS_ERR(frc_devp->clk_me) || IS_ERR(frc_devp->clk_frc)) {
		pr_frc(0, "can't get frc clk !\n");
		frc_devp->clk_frc = NULL;
		frc_devp->clk_me = NULL;
	}
	frc_attach_pd(frc_devp);
	return ret;
}

#ifdef CONFIG_COMPAT
static long frc_campat_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	arg = (unsigned long)compat_ptr(arg);
	return frc_ioctl(file, cmd, arg);
}
#endif

int frc_notify_callback(struct notifier_block *block, unsigned long cmd, void *para)
{
	struct frc_dev_s *devp = get_frc_devp();

	if (!devp)
		return -1;

	pr_frc(1, "%s cmd: 0x%lx\n", __func__, cmd);
	switch (cmd) {
	case VOUT_EVENT_MODE_CHANGE_PRE:
		/*if frc on, need disable frc, and enable frc*/
		devp->frc_sts.out_put_mode_changed = FRC_EVENT_VOUT_CHG;
		//frc_change_to_state(FRC_STATE_DISABLE);
		break;

	case VOUT_EVENT_MODE_CHANGE:
		devp->frc_sts.out_put_mode_changed = FRC_EVENT_VOUT_CHG;
		//frc_change_to_state(FRC_STATE_DISABLE);
		break;

	default:
		break;
	}
	return 0;
}

int frc_vd_notify_callback(struct notifier_block *block, unsigned long cmd, void *para)
{
	struct frc_dev_s *devp = get_frc_devp();
	struct vd_info_s *info;
	u32 flags;

	if (!devp)
		return -1;

	info = (struct vd_info_s *)para;
	flags = info->flags;

	pr_frc(3, "%s cmd: 0x%lx flags:0x%x\n", __func__, cmd, flags);
	switch (cmd) {
	case VIDEO_INFO_CHANGED:
		/*if frc on, need disable frc, and enable frc*/
		if (((flags & VIDEO_SIZE_CHANGE_EVENT)
			== VIDEO_SIZE_CHANGE_EVENT) && devp->probe_ok) {
			set_frc_enable(false);
			set_frc_bypass(true);
			frc_change_to_state(FRC_STATE_DISABLE);
			frc_state_change_finish(devp);
			pr_frc(1, "VIDEO_SIZE_CHANGE_EVENT\n");
			devp->frc_sts.out_put_mode_changed = FRC_EVENT_VF_CHG_IN_SIZE;
		}
		break;

	default:
		break;
	}

	return 0;
}

static struct notifier_block frc_notifier_nb = {
	.notifier_call	= frc_notify_callback,
};

static struct notifier_block frc_notifier_vb = {
	.notifier_call	= frc_vd_notify_callback,
};

static const struct file_operations frc_fops = {
	.owner = THIS_MODULE,
	.open = frc_open,
	.release = frc_release,
	.unlocked_ioctl = frc_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = frc_campat_ioctl,
#endif
};

static void frc_clock_workaround(struct work_struct *work)
{
	struct frc_dev_s *devp = container_of(work,
		struct frc_dev_s, frc_clk_work);

	if (unlikely(!devp)) {
		PR_ERR("%s err, devp is NULL\n", __func__);
		return;
	}
	if (!devp->probe_ok)
		return;
	if (!devp->power_on_flag)
		return;
	// pr_frc(1, "%s, clk_state:%d\n", __func__, devp->clk_state);
	if (devp->clk_state == FRC_CLOCK_2MIN) {
		clk_set_rate(devp->clk_frc, 333333333);
		devp->clk_state = FRC_CLOCK_MIN;
	} else if (devp->clk_state == FRC_CLOCK_2NOR) {
		clk_set_rate(devp->clk_frc, 667000000);
		devp->clk_state = FRC_CLOCK_NOR;
	}
	pr_frc(1, "%s, clk_new state:%d\n", __func__, devp->clk_state);
}

static void frc_drv_initial(struct frc_dev_s *devp)
{
	struct vinfo_s *vinfo = get_current_vinfo();
	struct frc_fw_data_s *fw_data;
	u32 i;

	pr_frc(0, "%s\n", __func__);
	if (!devp)
		return;

	devp->frc_sts.state = FRC_STATE_BYPASS;
	devp->frc_sts.new_state = FRC_STATE_BYPASS;

	/*0:before postblend; 1:after postblend*/
	//devp->frc_hw_pos = FRC_POS_AFTER_POSTBLEND;/*for test*/
	devp->frc_sts.auto_ctrl = 0;
	devp->frc_fw_pause = false;
	// devp->frc_fw_pause = true;
	devp->frc_sts.frame_cnt = 0;
	devp->frc_sts.state_transing = false;
	devp->frc_sts.re_cfg_cnt = 0;
	devp->frc_sts.out_put_mode_changed = false;
	devp->frc_sts.re_config = false;
	devp->in_sts.vf_sts = 0;/*initial to no*/
	devp->dbg_force_en = 0;

	/*input sts initial*/
	devp->in_sts.have_vf_cnt = 0;
	devp->in_sts.no_vf_cnt = 0;

	devp->dbg_in_out_ratio = FRC_RATIO_1_1;/*enum frc_ratio_mode_type frc_ratio_mode*/
	// devp->dbg_in_out_ratio = FRC_RATIO_2_5;/*enum frc_ratio_mode_type frc_ratio_mode*/
	devp->dbg_input_hsize = vinfo->width;
	devp->dbg_input_vsize = vinfo->height;
	devp->dbg_reg_monitor_i = 0;
	devp->dbg_reg_monitor_o = 0;
	devp->dbg_vf_monitor = 0;
	for (i = 0; i < MONITOR_REG_MAX; i++) {
		devp->dbg_in_reg[i] = 0;
		devp->dbg_out_reg[i] = 0;
	}
	devp->dbg_buf_len = 0;

	devp->loss_ratio = 0;
	devp->prot_mode = false;

	devp->in_out_ratio = FRC_RATIO_1_1;
	// devp->in_out_ratio = FRC_RATIO_2_5;
	// devp->film_mode = EN_FILM32;
	devp->film_mode = EN_VIDEO;
	devp->film_mode_det = 0;

	fw_data = (struct frc_fw_data_s *)devp->fw_data;
	fw_data->holdline_parm.me_hold_line = 4;
	fw_data->holdline_parm.mc_hold_line = 4;
	fw_data->holdline_parm.inp_hold_line = 4;
	fw_data->holdline_parm.reg_post_dly_vofst = 0;/*fixed*/
	fw_data->holdline_parm.reg_mc_dly_vofst0 = 1;/*fixed*/

	memset(&devp->frc_crc_data, 0, sizeof(struct frc_crc_data_s));
	memset(&devp->ud_dbg, 0, sizeof(struct frc_ud_s));
	/*used for force in/out size for frc process*/
	memset(&devp->force_size, 0, sizeof(struct frc_force_size_s));
}

void get_vout_info(struct frc_dev_s *frc_devp)
{
	struct vinfo_s *vinfo = get_current_vinfo();
	struct frc_fw_data_s *pfw_data;
	u32  tmpframterate = 0;

	if (!frc_devp) {
		PR_ERR("%s: frc_devp is null\n", __func__);
		return;
	}

	pfw_data = (struct frc_fw_data_s *)frc_devp->fw_data;
	frc_devp->out_sts.vout_height = vinfo->height;
	frc_devp->out_sts.vout_width = vinfo->width;
	tmpframterate =
	(vinfo->sync_duration_num * 100 / vinfo->sync_duration_den) / 100;
	if (frc_devp->out_sts.out_framerate != tmpframterate) {
		frc_devp->out_sts.out_framerate = tmpframterate;
		pfw_data->frc_top_type.frc_other_reserved =
			frc_devp->out_sts.out_framerate;
		pr_frc(0, "vout:w-%d,h-%d,rate-%d\n",
				frc_devp->out_sts.vout_width,
				frc_devp->out_sts.vout_height,
				frc_devp->out_sts.out_framerate);
	}
}

static int frc_probe(struct platform_device *pdev)
{
	int ret = 0, i;
	struct frc_data_s *frc_data;
	struct frc_dev_s *frc_devp = &frc_dev;
	// frc_dev = kzalloc(sizeof(*frc_dev), GFP_KERNEL);
	// if (!frc_dev) {
	//	PR_ERR("%s: frc_dev kzalloc memory failed\n", __func__);
	//	goto fail_alloc_dev;
	// }
	memset(frc_devp, 0, (sizeof(struct frc_dev_s)));

	frc_devp->data = NULL;
	frc_devp->data = kzalloc(sizeof(*frc_devp->data), GFP_KERNEL);
	if (!frc_devp->data) {
		PR_ERR("%s: frc_dev->data fail\n", __func__);
		goto fail_alloc_data_fail;
	}

	// frc_devp->fw_data = NULL;
	// frc_devp->fw_data = kzalloc(sizeof(struct frc_fw_data_s), GFP_KERNEL);
	frc_devp->fw_data = &fw_data;
	memset(frc_devp->fw_data, 0, (sizeof(struct frc_fw_data_s)));
	strncpy(&fw_data.frc_alg_ver[0], &frc_alg_defver[0],
			strlen(frc_alg_defver));
	if (!frc_devp->fw_data) {
		PR_ERR("%s: frc_dev->fw_data fail\n", __func__);
		goto fail_alloc_fw_data_fail;
	}
	PR_FRC("%s fw_data st size:%d", __func__, sizeof_frc_fw_data_struct());

	frc_devp->out_line = frc_init_out_line();
	ret = alloc_chrdev_region(&frc_devp->devno, 0, FRC_DEVNO, FRC_NAME);
	if (ret < 0) {
		PR_ERR("%s: alloc region fail\n", __func__);
		goto fail_alloc_region;
	}
	frc_devp->clsp = class_create(THIS_MODULE, FRC_CLASS_NAME);
	if (IS_ERR(frc_devp->clsp)) {
		ret = PTR_ERR(frc_devp->clsp);
		PR_ERR("%s: create class fail\n", __func__);
		goto fail_create_cls;
	}

	for (i = 0; frc_class_attrs[i].attr.name; i++) {
		if (class_create_file(frc_devp->clsp, &frc_class_attrs[i]) < 0)
			goto fail_class_create_file;
	}
	get_vout_info(frc_devp);

	cdev_init(&frc_devp->cdev, &frc_fops);
	frc_devp->cdev.owner = THIS_MODULE;
	ret = cdev_add(&frc_devp->cdev, frc_devp->devno, FRC_DEVNO);
	if (ret)
		goto fail_add_cdev;

	frc_devp->dev = device_create(frc_devp->clsp, NULL,
		frc_devp->devno, frc_devp, FRC_NAME);
	if (IS_ERR(frc_devp->dev)) {
		PR_ERR("%s: device create fail\n", __func__);
		goto fail_dev_create;
	}

	dev_set_drvdata(frc_devp->dev, frc_devp);
	platform_set_drvdata(pdev, frc_devp);
	frc_devp->pdev = pdev;

	frc_data = (struct frc_data_s *)frc_devp->data;
	// fw_data = (struct frc_fw_data_s *)frc_devp->fw_data;
	frc_dts_parse(frc_devp);
	// if (ret < 0)  // fixed CID 139501
	//	goto fail_dev_create;

	tasklet_init(&frc_devp->input_tasklet, frc_input_tasklet_pro, (unsigned long)frc_devp);
	tasklet_init(&frc_devp->output_tasklet, frc_output_tasklet_pro, (unsigned long)frc_devp);
	/*register a notify*/
	vout_register_client(&frc_notifier_nb);
	vd_signal_register_client(&frc_notifier_vb);

	/*driver internal data initial*/
	frc_drv_initial(frc_devp);
	frc_clk_init(frc_devp);
	frc_devp->power_on_flag = true;
	frc_init_config(frc_devp);

	/*buffer config*/
	frc_buf_calculate(frc_devp);
	frc_buf_alloc(frc_devp);
	frc_buf_distribute(frc_devp);
	frc_buf_config(frc_devp);

	frc_hw_initial(frc_devp);
	frc_internal_initial(frc_devp); /*need after frc_top_init*/
	/*enable irq*/
	if (frc_devp->in_irq > 0)
		enable_irq(frc_devp->in_irq);
	if (frc_devp->out_irq > 0)
		enable_irq(frc_devp->out_irq);
#ifdef CONFIG_AMLOGIC_MEDIA_FRC_RDMA
	if (frc_devp->rdma_irq > 0)
		enable_irq(frc_devp->rdma_irq);
#endif
	INIT_WORK(&frc_devp->frc_clk_work, frc_clock_workaround);

	frc_devp->probe_ok = true;
	frc_devp->power_off_flag = false;

	PR_FRC("%s probe st:%d", __func__, frc_devp->probe_ok);
	return ret;
fail_dev_create:
	cdev_del(&frc_devp->cdev);
fail_add_cdev:
	PR_ERR("%s: cdev add fail\n", __func__);
fail_class_create_file:
	for (i = 0; frc_class_attrs[i].attr.name; i++)
		class_remove_file(frc_devp->clsp, &frc_class_attrs[i]);
	class_destroy(frc_devp->clsp);
fail_create_cls:
	unregister_chrdev_region(frc_devp->devno, FRC_DEVNO);
fail_alloc_region:
	// kfree(frc_dev->fw_data);
	frc_devp->fw_data = NULL;
fail_alloc_fw_data_fail:
	kfree(frc_devp->data);
	frc_devp->data = NULL;
fail_alloc_data_fail:
	// kfree(frc_dev);
	// frc_dev = NULL;
// fail_alloc_dev:
	return ret;
}

static int __exit frc_remove(struct platform_device *pdev)
{
	struct frc_dev_s *frc_devp = &frc_dev;

	PR_FRC("%s:module remove\n", __func__);
	// frc_devp = platform_get_drvdata(pdev);
	cancel_work_sync(&frc_devp->frc_clk_work);
	tasklet_kill(&frc_devp->input_tasklet);
	tasklet_kill(&frc_devp->output_tasklet);
	tasklet_disable(&frc_devp->input_tasklet);
	tasklet_disable(&frc_devp->output_tasklet);
	if (frc_devp->in_irq > 0)
		free_irq(frc_devp->in_irq, (void *)frc_devp);
	if (frc_devp->out_irq > 0)
		free_irq(frc_devp->out_irq, (void *)frc_devp);

	device_destroy(frc_devp->clsp, frc_devp->devno);
	cdev_del(&frc_devp->cdev);
	class_destroy(frc_devp->clsp);
	unregister_chrdev_region(frc_devp->devno, FRC_DEVNO);
	//destroy_workqueue(frc_devp->frc_wq);
	set_frc_clk_disable();
	//if (frc_devp->dbg_buf)
	//	kfree(frc_devp->dbg_buf);
	frc_buf_release(frc_devp);
	kfree(frc_devp->data);
	frc_devp->data = NULL;
	// kfree(frc_dev);
	// frc_dev = NULL;
	PR_FRC("%s:module remove done\n", __func__);
	return 0;
}

static void frc_shutdown(struct platform_device *pdev)
{
	struct frc_dev_s *frc_devp = &frc_dev;

	PR_FRC("%s:module shutdown\n", __func__);
	// frc_devp = platform_get_drvdata(pdev);
	frc_devp->power_on_flag = false;
	tasklet_kill(&frc_devp->input_tasklet);
	tasklet_kill(&frc_devp->output_tasklet);
	tasklet_disable(&frc_devp->input_tasklet);
	tasklet_disable(&frc_devp->output_tasklet);
	if (frc_devp->in_irq > 0)
		free_irq(frc_devp->in_irq, (void *)frc_devp);
	if (frc_devp->out_irq > 0)
		free_irq(frc_devp->out_irq, (void *)frc_devp);
	device_destroy(frc_devp->clsp, frc_devp->devno);
	cdev_del(&frc_devp->cdev);
	class_destroy(frc_devp->clsp);
	unregister_chrdev_region(frc_devp->devno, FRC_DEVNO);
	//destroy_workqueue(frc_devp->frc_wq);
	set_frc_clk_disable();
	//if (frc_devp->dbg_buf)
	//	kfree(frc_devp->dbg_buf);
	frc_buf_release(frc_devp);
	kfree(frc_devp->data);
	frc_devp->data = NULL;
	// kfree(frc_dev);
	// frc_dev = NULL;
	pwr_ctrl_psci_smc(PDID_T3_FRCTOP, PWR_OFF);
	pwr_ctrl_psci_smc(PDID_T3_FRCME, PWR_OFF);
	pwr_ctrl_psci_smc(PDID_T3_FRCMC, PWR_OFF);
	PR_FRC("%s:module shutdown done with powerdomain\n", __func__);

}

#if CONFIG_PM
static int frc_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct frc_dev_s *devp = NULL;

	devp = get_frc_devp();
	if (!devp)
		return -1;
	PR_FRC("%s ...\n", __func__);
	frc_power_domain_ctrl(devp, 0);
	if (devp->power_on_flag)
		devp->power_on_flag = false;

	return 0;
}

static int frc_resume(struct platform_device *pdev)
{
	struct frc_dev_s *devp = NULL;

	devp = get_frc_devp();
	if (!devp)
		return -1;
	PR_FRC("%s ...\n", __func__);
	frc_power_domain_ctrl(devp, 1);
	if (!devp->power_on_flag)
		devp->power_on_flag = true;

	return 0;
}
#endif

static struct platform_driver frc_driver = {
	.probe = frc_probe,
	.remove = frc_remove,
	.shutdown = frc_shutdown,
#ifdef CONFIG_PM
	.suspend = frc_suspend,
	.resume = frc_resume,
#endif
	.driver = {
		.name = "aml_frc",
		.owner = THIS_MODULE,
		.of_match_table = frc_dts_match,
	},
};

int __init frc_init(void)
{
	PR_FRC("%s:module init\n", __func__);
	if (platform_driver_register(&frc_driver)) {
		PR_ERR("failed to register frc driver module\n");
		return -ENODEV;
	}
	return 0;
}

void __exit frc_exit(void)
{
	platform_driver_unregister(&frc_driver);
	PR_FRC("%s:module exit\n", __func__);
}
