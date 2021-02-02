// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

/* Linux Headers */
#include <linux/module.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/fb.h>
#include <linux/list.h>
#include <linux/uaccess.h>
#include <linux/sysfs.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_address.h>
#include <linux/of_reserved_mem.h>
#include <linux/reset.h>
#include <linux/clk.h>
#include <linux/pm_runtime.h>
#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif
/* Amlogic Headers */
#include <linux/amlogic/media/ge2d/ge2d.h>
#include <linux/amlogic/media/ge2d/ge2d_cmd.h>
#ifdef CONFIG_AMLOGIC_VPU
#include <linux/amlogic/media/vpu/vpu.h>
#endif
#include <linux/amlogic/media/registers/regs/ao_regs.h>

/* Local Headers */
#include "ge2dgen.h"
#include "ge2d_log.h"
#include "ge2d_wq.h"
#include "ge2d_dmabuf.h"
#include "ge2d_reg.h"

#define GE2D_CLASS_NAME "ge2d"
#define MAX_GE2D_CLK 500000000
#define HHI_MEM_PD_REG0 0x40
#define RESET2_LEVEL    0x422

struct ge2d_device_s {
	char name[20];
	atomic_t open_count;
	int major;
	unsigned int dbg_enable;
	struct class *cla;
	struct device *dev;
};

void __iomem *ge2d_reg_map;
static struct ge2d_device_s ge2d_device;
static DEFINE_MUTEX(ge2d_mutex);
unsigned int ge2d_log_level;
unsigned int ge2d_dump_reg_enable;
unsigned int ge2d_dump_reg_cnt;

struct ge2d_device_data_s ge2d_meson_dev;

static int init_ge2d_device(void);
static int remove_ge2d_device(void);
static int ge2d_open(struct inode *inode, struct file *file);
static long ge2d_ioctl(struct file *filp, unsigned int cmd,
		       unsigned long args);
#ifdef CONFIG_COMPAT
static long ge2d_compat_ioctl(struct file *filp, unsigned int cmd,
			      unsigned long args);
#endif
static int ge2d_release(struct inode *inode, struct file *file);
static ssize_t log_level_show(struct class *cla,
			      struct class_attribute *attr,
			      char *buf);
static ssize_t log_level_store(struct class *cla,
			       struct class_attribute *attr,
			       const char *buf, size_t count);
static ssize_t dump_reg_enable_show(struct class *cla,
				    struct class_attribute *attr,
				    char *buf);
static ssize_t dump_reg_enable_store(struct class *cla,
				     struct class_attribute *attr,
				     const char *buf, size_t count);
static ssize_t dump_reg_cnt_show(struct class *cla,
				 struct class_attribute *attr,
				 char *buf);
static ssize_t dump_reg_cnt_store(struct class *cla,
				  struct class_attribute *attr,
				  const char *buf, size_t count);

static const struct file_operations ge2d_fops = {
	.owner = THIS_MODULE,
	.open = ge2d_open,
	.unlocked_ioctl = ge2d_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = ge2d_compat_ioctl,
#endif
	.release = ge2d_release,
};

static CLASS_ATTR_RO(work_queue_status);
static CLASS_ATTR_RO(free_queue_status);
static CLASS_ATTR_RW(log_level);
static CLASS_ATTR_RW(dump_reg_enable);
static CLASS_ATTR_RW(dump_reg_cnt);

static struct attribute *ge2d_class_attrs[] = {
	&class_attr_work_queue_status.attr,
	&class_attr_free_queue_status.attr,
	&class_attr_log_level.attr,
	&class_attr_dump_reg_enable.attr,
	&class_attr_dump_reg_cnt.attr,
	NULL
};
ATTRIBUTE_GROUPS(ge2d_class);

static struct class ge2d_class = {
	.name = GE2D_CLASS_NAME,
	.class_groups = ge2d_class_groups,
};

static ssize_t dump_reg_enable_show(struct class *cla,
				    struct class_attribute *attr,
				    char *buf)
{
	return snprintf(buf, 40, "%d\n", ge2d_dump_reg_enable);
}

static ssize_t dump_reg_enable_store(struct class *cla,
				     struct class_attribute *attr,
				     const char *buf, size_t count)
{
	int res = 0;
	int ret = 0;

	ret = kstrtoint(buf, 0, &res);
	ge2d_log_info("ge2d dump_reg_enbale: %d->%d\n",
		      ge2d_dump_reg_enable, res);
	ge2d_dump_reg_enable = res;

	return count;
}

static ssize_t dump_reg_cnt_show(struct class *cla,
				 struct class_attribute *attr,
				 char *buf)
{
	return snprintf(buf, 40, "%d\n", ge2d_dump_reg_cnt);
}

static ssize_t dump_reg_cnt_store(struct class *cla,
				  struct class_attribute *attr,
				  const char *buf, size_t count)
{
	int res = 0;
	int ret = 0;

	ret = kstrtoint(buf, 0, &res);
	ge2d_log_info("ge2d dump_reg: %d->%d\n", ge2d_dump_reg_cnt, res);
	ge2d_dump_reg_cnt = res;
	return count;
}

static ssize_t log_level_show(struct class *cla,
			      struct class_attribute *attr,
			      char *buf)
{
	return snprintf(buf, 40, "%d\n", ge2d_log_level);
}

static ssize_t log_level_store(struct class *cla,
			       struct class_attribute *attr,
			       const char *buf, size_t count)
{
	int res = 0;
	int ret = 0;

	ret = kstrtoint(buf, 0, &res);
	ge2d_log_info("ge2d log_level: %d->%d\n", ge2d_log_level, res);
	ge2d_log_level = res;

	return count;
}

static int ge2d_open(struct inode *inode, struct file *file)
{
	struct ge2d_context_s *context = NULL;
	int i;

	/* we create one ge2d workqueue for this file handler. */
	context = create_ge2d_work_queue();
	if (!context) {
		ge2d_log_err("can't create work queue\n");
		return -1;
	}

	/* reset dms_used flag */
	for (i = 0; i < MAX_PLANE; i++) {
		context->config.src_dma_cfg[i].dma_used[i] = 0;
		context->config.src2_dma_cfg[i].dma_used[i] = 0;
		context->config.dst_dma_cfg[i].dma_used[i] = 0;
	}

	file->private_data = context;
	atomic_inc(&ge2d_device.open_count);

	return 0;
}

static int ge2d_ioctl_config_ex_mem(struct ge2d_context_s *context,
				    unsigned int cmd, unsigned long args)
{
	struct config_para_ex_memtype_s *ge2d_config_ex_mem;
	struct config_ge2d_para_ex_s *ge2d_para_config;
	int ret = 0;
#ifdef CONFIG_COMPAT
	struct compat_config_para_ex_memtype_s __user *uf_ex_mem;
	struct compat_config_ge2d_para_ex_s __user *uf_ge2d_para;
	int r = 0;
	int i, j;
#endif
	void __user *argp = (void __user *)args;

	ge2d_para_config = kzalloc(sizeof(*ge2d_para_config), GFP_KERNEL);
	if (!ge2d_para_config)
		return -ENOMEM;

	switch (cmd) {
	case GE2D_CONFIG_EX_MEM:
		ret = copy_from_user(ge2d_para_config, argp,
				     sizeof(struct config_ge2d_para_ex_s));
		ge2d_config_ex_mem = &ge2d_para_config->para_config_memtype;
		ret = ge2d_context_config_ex_mem(context, ge2d_config_ex_mem);
		break;
#ifdef CONFIG_COMPAT
	case GE2D_CONFIG_EX32_MEM:
		uf_ge2d_para = (struct compat_config_ge2d_para_ex_s *)argp;
		r |= get_user(ge2d_para_config->para_config_memtype.ge2d_magic,
			&uf_ge2d_para->para_config_memtype.ge2d_magic);
		ge2d_config_ex_mem = &ge2d_para_config->para_config_memtype;

		if (ge2d_para_config->para_config_memtype.ge2d_magic
			== sizeof(struct compat_config_para_ex_memtype_s)) {
			struct config_para_ex_ion_s *pge2d_config_ex;

			uf_ex_mem =
				(struct compat_config_para_ex_memtype_s *)argp;
			pge2d_config_ex =
				&ge2d_config_ex_mem->_ge2d_config_ex;
			r = copy_from_user(&pge2d_config_ex->src_para,
					   &uf_ex_mem->_ge2d_config_ex.src_para,
					   sizeof(struct src_dst_para_ex_s));
			r |= copy_from_user
					(&pge2d_config_ex->src2_para,
					 &uf_ex_mem->_ge2d_config_ex.src2_para,
					 sizeof(struct src_dst_para_ex_s));
			r |= copy_from_user
					(&pge2d_config_ex->dst_para,
					 &uf_ex_mem->_ge2d_config_ex.dst_para,
					 sizeof(struct src_dst_para_ex_s));
			r |= copy_from_user(&pge2d_config_ex->src_key,
				&uf_ex_mem->_ge2d_config_ex.src_key,
				sizeof(struct src_key_ctrl_s));
			r |= copy_from_user(&pge2d_config_ex->src2_key,
				&uf_ex_mem->_ge2d_config_ex.src2_key,
				sizeof(struct src_key_ctrl_s));

			r |= get_user(pge2d_config_ex->src1_cmult_asel,
				&uf_ex_mem->_ge2d_config_ex.src1_cmult_asel);
			r |= get_user(pge2d_config_ex->src2_cmult_asel,
				&uf_ex_mem->_ge2d_config_ex.src2_cmult_asel);
			r |= get_user(pge2d_config_ex->alu_const_color,
				&uf_ex_mem->_ge2d_config_ex.alu_const_color);
			r |= get_user(pge2d_config_ex->src1_gb_alpha_en,
				&uf_ex_mem->_ge2d_config_ex.src1_gb_alpha_en);
			r |= get_user(pge2d_config_ex->src1_gb_alpha,
				&uf_ex_mem->_ge2d_config_ex.src1_gb_alpha);
#ifdef CONFIG_GE2D_SRC2
			r |= get_user(pge2d_config_ex->src2_gb_alpha_en,
				&uf_ex_mem->_ge2d_config_ex.src2_gb_alpha_en);
			r |= get_user(pge2d_config_ex->src2_gb_alpha,
				&uf_ex_mem->_ge2d_config_ex.src2_gb_alpha);
#endif
			r |= get_user(pge2d_config_ex->op_mode,
				&uf_ex_mem->_ge2d_config_ex.op_mode);
			r |= get_user(pge2d_config_ex->bitmask_en,
				&uf_ex_mem->_ge2d_config_ex.bitmask_en);
			r |= get_user(pge2d_config_ex->bytemask_only,
				&uf_ex_mem->_ge2d_config_ex.bytemask_only);
			r |= get_user(pge2d_config_ex->bitmask,
				&uf_ex_mem->_ge2d_config_ex.bitmask);
			r |= get_user(pge2d_config_ex->dst_xy_swap,
				&uf_ex_mem->_ge2d_config_ex.dst_xy_swap);
			r |= get_user(pge2d_config_ex->hf_init_phase,
				&uf_ex_mem->_ge2d_config_ex.hf_init_phase);
			r |= get_user(pge2d_config_ex->hf_rpt_num,
				&uf_ex_mem->_ge2d_config_ex.hf_rpt_num);
			r |= get_user(pge2d_config_ex->hsc_start_phase_step,
				&uf_ex_mem->_ge2d_config_ex
				.hsc_start_phase_step);
			r |= get_user(pge2d_config_ex->hsc_phase_slope,
				&uf_ex_mem->_ge2d_config_ex.hsc_phase_slope);
			r |= get_user(pge2d_config_ex->vf_init_phase,
				&uf_ex_mem->_ge2d_config_ex.vf_init_phase);
			r |= get_user(pge2d_config_ex->vf_rpt_num,
				&uf_ex_mem->_ge2d_config_ex.vf_rpt_num);
			r |= get_user(pge2d_config_ex->vsc_start_phase_step,
				&uf_ex_mem->_ge2d_config_ex
				.vsc_start_phase_step);
			r |= get_user(pge2d_config_ex->vsc_phase_slope,
				&uf_ex_mem->_ge2d_config_ex.vsc_phase_slope);
			r |=
			get_user(pge2d_config_ex->src1_vsc_phase0_always_en,
				 &uf_ex_mem->_ge2d_config_ex
				.src1_vsc_phase0_always_en);
			r |=
			get_user(pge2d_config_ex->src1_hsc_phase0_always_en,
				 &uf_ex_mem->_ge2d_config_ex
				.src1_hsc_phase0_always_en);
			r |=
			get_user(pge2d_config_ex->src1_hsc_rpt_ctrl,
				 &uf_ex_mem->_ge2d_config_ex.src1_hsc_rpt_ctrl);
			r |=
			get_user(pge2d_config_ex->src1_vsc_rpt_ctrl,
				 &uf_ex_mem->_ge2d_config_ex.src1_vsc_rpt_ctrl);

			for (i = 0; i < 4; i++) {
				struct config_planes_ion_s *psrc_planes;

				psrc_planes =
					&pge2d_config_ex->src_planes[i];
				r |= get_user(psrc_planes->addr,
					&uf_ex_mem->_ge2d_config_ex
					.src_planes[i].addr);
				r |= get_user(psrc_planes->w,
					&uf_ex_mem->_ge2d_config_ex
					.src_planes[i].w);
				r |= get_user(psrc_planes->h,
					&uf_ex_mem->_ge2d_config_ex
					.src_planes[i].h);
				r |= get_user(psrc_planes->shared_fd,
					&uf_ex_mem->_ge2d_config_ex
					.src_planes[i].shared_fd);
			}

			for (i = 0; i < 4; i++) {
				struct config_planes_ion_s *psrc2_planes;

				psrc2_planes =
					&pge2d_config_ex->src2_planes[i];
				r |= get_user(psrc2_planes->addr,
					&uf_ex_mem->_ge2d_config_ex
					.src2_planes[i].addr);
				r |= get_user(psrc2_planes->w,
					&uf_ex_mem->_ge2d_config_ex
					.src2_planes[i].w);
				r |= get_user(psrc2_planes->h,
					&uf_ex_mem->_ge2d_config_ex
					.src2_planes[i].h);
				r |= get_user(psrc2_planes->shared_fd,
					&uf_ex_mem->_ge2d_config_ex
					.src2_planes[i].shared_fd);
			}

			for (j = 0; j < 4; j++) {
				struct config_planes_ion_s *pdst_planes;

				pdst_planes =
					&pge2d_config_ex->dst_planes[j];
				r |= get_user(pdst_planes->addr,
					&uf_ex_mem->_ge2d_config_ex
					.dst_planes[j].addr);
				r |= get_user(pdst_planes->w,
					&uf_ex_mem->_ge2d_config_ex
					.dst_planes[j].w);
				r |= get_user(pdst_planes->h,
					&uf_ex_mem->_ge2d_config_ex
					.dst_planes[j].h);
				r |= get_user(pdst_planes->shared_fd,
					&uf_ex_mem->_ge2d_config_ex
					.dst_planes[j].shared_fd);
			}

			r |= get_user(ge2d_config_ex_mem->src1_mem_alloc_type,
				&uf_ex_mem->src1_mem_alloc_type);
			r |= get_user(ge2d_config_ex_mem->src2_mem_alloc_type,
				&uf_ex_mem->src2_mem_alloc_type);
			r |= get_user(ge2d_config_ex_mem->dst_mem_alloc_type,
				&uf_ex_mem->dst_mem_alloc_type);
			r |= copy_from_user(&ge2d_config_ex_mem->matrix_custom,
					    &uf_ex_mem->matrix_custom,
					    sizeof(struct ge2d_matrix_s));
			r |= copy_from_user(&ge2d_config_ex_mem->stride_custom,
					    &uf_ex_mem->stride_custom,
					    sizeof(struct ge2d_stride_s));
		}
		if (r) {
			pr_err("GE2D_CONFIG_EX32 get parameter failed .\n");
			ret = -EFAULT;
			goto release;
		}
		ret = ge2d_context_config_ex_mem(context, ge2d_config_ex_mem);
		break;
#endif
	}
#ifdef CONFIG_COMPAT
release:
#endif
	kfree(ge2d_para_config);

	return ret;
}

static long ge2d_ioctl(struct file *filp, unsigned int cmd, unsigned long args)
{
	struct ge2d_context_s *context = NULL;
	struct config_para_s ge2d_config;
	struct ge2d_para_s para;
	struct config_para_ex_s *ge2d_config_ex;
	struct config_para_ex_ion_s *ge2d_config_ex_ion;
	struct ge2d_dmabuf_req_s ge2d_req_buf;
	struct ge2d_dmabuf_exp_s ge2d_exp_buf;
	int ret = 0;
#ifdef CONFIG_COMPAT
	struct compat_config_para_s __user *uf;
	struct compat_config_para_ex_s __user *uf_ex;
	struct compat_config_para_ex_ion_s __user *uf_ex_ion;
	int r = 0;
	int i, j;
#endif
	int cap_mask = 0;
	int index = 0, dma_fd;
	enum ge2d_data_type_e data_type;
	struct ge2d_dmabuf_attach_s attatch;
	void __user *argp = (void __user *)args;

	ge2d_config_ex = kzalloc(sizeof(*ge2d_config_ex), GFP_KERNEL);
	if (!ge2d_config_ex)
		return -ENOMEM;

	ge2d_config_ex_ion = kzalloc(sizeof(*ge2d_config_ex_ion), GFP_KERNEL);
	if (!ge2d_config_ex_ion) {
		ret = -ENOMEM;
		goto release2;
	}

	context = (struct ge2d_context_s *)filp->private_data;
	memset(&ge2d_config, 0, sizeof(struct config_para_s));
	memset(&ge2d_req_buf, 0, sizeof(struct ge2d_dmabuf_req_s));
	memset(&ge2d_exp_buf, 0, sizeof(struct ge2d_dmabuf_exp_s));
#ifdef CONFIG_AMLOGIC_MEDIA_GE2D_MORE_SECURITY
	switch (cmd) {
	case GE2D_CONFIG:
	case GE2D_CONFIG32:
	case GE2D_CONFIG_EX:
	case GE2D_CONFIG_EX32:
		pr_err("ioctl not support.\n");
		ret = -EINVAL;
		goto release1;
	}
#endif
	switch (cmd) {
	case GE2D_GET_CAP:
		/* DST_SIGN_MODE   |
		 * DST_REPEAT      |
		 * CANVAS_STATUS   |
		 * HAS_SELF_POWER  |
		 * DEEP_COLOR      |
		 * ADVANCED_MATRIX |
		 * SRC2_REPEAT     |
		 * SRC2_ALPHA
		 */
		cap_mask = ge2d_meson_dev.dst_sign_mode << 8 |
			   ge2d_meson_dev.dst_repeat << 7    |
			   ge2d_meson_dev.canvas_status << 5 |
			   ge2d_meson_dev.has_self_pwr  << 4 |
			   ge2d_meson_dev.deep_color    << 3 |
			   ge2d_meson_dev.adv_matrix    << 2 |
			   ge2d_meson_dev.src2_repeat   << 1 |
			   ge2d_meson_dev.src2_alp      << 0;
		put_user(cap_mask, (int __user *)argp);
		break;
	case GE2D_CONFIG:
	case GE2D_SRCCOLORKEY:
		ret = copy_from_user(&ge2d_config,
				     argp, sizeof(struct config_para_s));
		break;
#ifdef CONFIG_COMPAT
	case GE2D_SRCCOLORKEY32:
		uf = (struct compat_config_para_s *)argp;
		r = get_user(ge2d_config.src_dst_type, &uf->src_dst_type);
		r |= get_user(ge2d_config.alu_const_color,
			&uf->alu_const_color);
		r |= get_user(ge2d_config.src_format, &uf->src_format);
		r |= get_user(ge2d_config.dst_format, &uf->dst_format);
		for (i = 0; i < 4; i++) {
			r |= get_user(ge2d_config.src_planes[i].addr,
				&uf->src_planes[i].addr);
			r |= get_user(ge2d_config.src_planes[i].w,
				&uf->src_planes[i].w);
			r |= get_user(ge2d_config.src_planes[i].h,
				&uf->src_planes[i].h);
		}
		for (j = 0; j < 4; j++) {
			r |= get_user(ge2d_config.dst_planes[j].addr,
				&uf->dst_planes[j].addr);
			r |= get_user(ge2d_config.dst_planes[j].w,
				&uf->dst_planes[j].w);
			r |= get_user(ge2d_config.dst_planes[j].h,
				&uf->dst_planes[j].h);
		}
		r |= copy_from_user(&ge2d_config.src_key, &uf->src_key,
			sizeof(struct src_key_ctrl_s));
		if (r) {
			pr_err("GE2D_SRCCOLORKEY32 get parameter failed .\n");
			ret = -EFAULT;
			goto release1;
		}
		break;
#endif
#ifdef CONFIG_COMPAT
	case GE2D_CONFIG32:
		uf = (struct compat_config_para_s *)argp;
		r = get_user(ge2d_config.src_dst_type, &uf->src_dst_type);
		r |= get_user(ge2d_config.alu_const_color,
			&uf->alu_const_color);
		r |= get_user(ge2d_config.src_format, &uf->src_format);
		r |= get_user(ge2d_config.dst_format, &uf->dst_format);
		for (i = 0; i < 4; i++) {
			r |= get_user(ge2d_config.src_planes[i].addr,
				&uf->src_planes[i].addr);
			r |= get_user(ge2d_config.src_planes[i].w,
				&uf->src_planes[i].w);
			r |= get_user(ge2d_config.src_planes[i].h,
				&uf->src_planes[i].h);
		}
		for (j = 0; j < 4; j++) {
			r |= get_user(ge2d_config.dst_planes[j].addr,
				&uf->dst_planes[j].addr);
			r |= get_user(ge2d_config.dst_planes[j].w,
				&uf->dst_planes[j].w);
			r |= get_user(ge2d_config.dst_planes[j].h,
				&uf->dst_planes[j].h);
		}
		r |= copy_from_user(&ge2d_config.src_key, &uf->src_key,
			sizeof(struct src_key_ctrl_s));
		if (r) {
			pr_err("GE2D_CONFIG32 get parameter failed .\n");
			ret = -EFAULT;
			goto release1;
		}

		break;
#endif
	case GE2D_CONFIG_EX:
		ret = copy_from_user(ge2d_config_ex, argp,
				     sizeof(struct config_para_ex_s));
		break;
#ifdef CONFIG_COMPAT
	case GE2D_CONFIG_EX32:
		uf_ex = (struct compat_config_para_ex_s *)argp;
		r = copy_from_user(&ge2d_config_ex->src_para, &uf_ex->src_para,
				   sizeof(struct src_dst_para_ex_s));
		r |= copy_from_user(&ge2d_config_ex->src2_para,
			&uf_ex->src2_para,
			sizeof(struct src_dst_para_ex_s));
		r |= copy_from_user(&ge2d_config_ex->dst_para, &uf_ex->dst_para,
			sizeof(struct src_dst_para_ex_s));

		r |= copy_from_user(&ge2d_config_ex->src_key, &uf_ex->src_key,
			sizeof(struct src_key_ctrl_s));
		r |= copy_from_user(&ge2d_config_ex->src2_key, &uf_ex->src2_key,
			sizeof(struct src_key_ctrl_s));

		r |= get_user(ge2d_config_ex->alu_const_color,
			&uf_ex->alu_const_color);
		r |= get_user(ge2d_config_ex->src1_gb_alpha,
			&uf_ex->src1_gb_alpha);
		r |= get_user(ge2d_config_ex->op_mode, &uf_ex->op_mode);
		r |= get_user(ge2d_config_ex->bitmask_en, &uf_ex->bitmask_en);
		r |= get_user(ge2d_config_ex->bytemask_only,
			&uf_ex->bytemask_only);
		r |= get_user(ge2d_config_ex->bitmask, &uf_ex->bitmask);
		r |= get_user(ge2d_config_ex->dst_xy_swap, &uf_ex->dst_xy_swap);
		r |= get_user(ge2d_config_ex->hf_init_phase,
			&uf_ex->hf_init_phase);
		r |= get_user(ge2d_config_ex->hf_rpt_num, &uf_ex->hf_rpt_num);
		r |= get_user(ge2d_config_ex->hsc_start_phase_step,
			&uf_ex->hsc_start_phase_step);
		r |= get_user(ge2d_config_ex->hsc_phase_slope,
			&uf_ex->hsc_phase_slope);
		r |= get_user(ge2d_config_ex->vf_init_phase,
			&uf_ex->vf_init_phase);
		r |= get_user(ge2d_config_ex->vf_rpt_num, &uf_ex->vf_rpt_num);
		r |= get_user(ge2d_config_ex->vsc_start_phase_step,
			&uf_ex->vsc_start_phase_step);
		r |= get_user(ge2d_config_ex->vsc_phase_slope,
			&uf_ex->vsc_phase_slope);
		r |= get_user(ge2d_config_ex->src1_vsc_phase0_always_en,
			&uf_ex->src1_vsc_phase0_always_en);
		r |= get_user(ge2d_config_ex->src1_hsc_phase0_always_en,
			&uf_ex->src1_hsc_phase0_always_en);
		r |= get_user(ge2d_config_ex->src1_hsc_rpt_ctrl,
			&uf_ex->src1_hsc_rpt_ctrl);
		r |= get_user(ge2d_config_ex->src1_vsc_rpt_ctrl,
			&uf_ex->src1_vsc_rpt_ctrl);

		for (i = 0; i < 4; i++) {
			r |= get_user(ge2d_config_ex->src_planes[i].addr,
				&uf_ex->src_planes[i].addr);
			r |= get_user(ge2d_config_ex->src_planes[i].w,
				&uf_ex->src_planes[i].w);
			r |= get_user(ge2d_config_ex->src_planes[i].h,
				&uf_ex->src_planes[i].h);
		}

		for (i = 0; i < 4; i++) {
			r |= get_user(ge2d_config_ex->src2_planes[i].addr,
				&uf_ex->src2_planes[i].addr);
			r |= get_user(ge2d_config_ex->src2_planes[i].w,
				&uf_ex->src2_planes[i].w);
			r |= get_user(ge2d_config_ex->src2_planes[i].h,
				&uf_ex->src2_planes[i].h);
		}

		for (j = 0; j < 4; j++) {
			r |= get_user(ge2d_config_ex->dst_planes[j].addr,
				&uf_ex->dst_planes[j].addr);
			r |= get_user(ge2d_config_ex->dst_planes[j].w,
				&uf_ex->dst_planes[j].w);
			r |= get_user(ge2d_config_ex->dst_planes[j].h,
				&uf_ex->dst_planes[j].h);
		}

		if (r) {
			pr_err("GE2D_CONFIG_EX32 get parameter failed .\n");
			ret = -EFAULT;
			goto release1;
		}
		break;
#endif
	case GE2D_CONFIG_EX_ION:
			ret = copy_from_user(ge2d_config_ex_ion, argp,
					     sizeof(struct config_para_ex_ion_s)
					     );
			break;
#ifdef CONFIG_COMPAT
	case GE2D_CONFIG_EX32_ION:
			uf_ex_ion = (struct compat_config_para_ex_ion_s *)argp;
			r = copy_from_user(&ge2d_config_ex_ion->src_para,
					   &uf_ex_ion->src_para,
					   sizeof(struct src_dst_para_ex_s));
			r |= copy_from_user(&ge2d_config_ex_ion->src2_para,
					    &uf_ex_ion->src2_para,
					    sizeof(struct src_dst_para_ex_s));
			r |= copy_from_user(&ge2d_config_ex_ion->dst_para,
					    &uf_ex_ion->dst_para,
					    sizeof(struct src_dst_para_ex_s));

			r |= copy_from_user(&ge2d_config_ex_ion->src_key,
				&uf_ex_ion->src_key,
				sizeof(struct src_key_ctrl_s));
			r |= copy_from_user(&ge2d_config_ex_ion->src2_key,
				&uf_ex_ion->src2_key,
				sizeof(struct src_key_ctrl_s));

			r |= get_user(ge2d_config_ex_ion->src1_cmult_asel,
				&uf_ex_ion->src1_cmult_asel);
			r |= get_user(ge2d_config_ex_ion->src2_cmult_asel,
				&uf_ex_ion->src2_cmult_asel);
			r |= get_user(ge2d_config_ex_ion->alu_const_color,
				&uf_ex_ion->alu_const_color);
			r |= get_user(ge2d_config_ex_ion->src1_gb_alpha_en,
				&uf_ex_ion->src1_gb_alpha_en);
			r |= get_user(ge2d_config_ex_ion->src1_gb_alpha,
				&uf_ex_ion->src1_gb_alpha);
#ifdef CONFIG_GE2D_SRC2
			r |= get_user(ge2d_config_ex_ion->src2_gb_alpha_en,
				&uf_ex_ion->src2_gb_alpha_en);
			r |= get_user(ge2d_config_ex_ion->src2_gb_alpha,
				&uf_ex_ion->src2_gb_alpha);
#endif
			r |= get_user(ge2d_config_ex_ion->op_mode,
				&uf_ex_ion->op_mode);
			r |= get_user(ge2d_config_ex_ion->bitmask_en,
				&uf_ex_ion->bitmask_en);
			r |= get_user(ge2d_config_ex_ion->bytemask_only,
				&uf_ex_ion->bytemask_only);
			r |= get_user(ge2d_config_ex_ion->bitmask,
				&uf_ex_ion->bitmask);
			r |= get_user(ge2d_config_ex_ion->dst_xy_swap,
				&uf_ex_ion->dst_xy_swap);
			r |= get_user(ge2d_config_ex_ion->hf_init_phase,
				&uf_ex_ion->hf_init_phase);
			r |= get_user(ge2d_config_ex_ion->hf_rpt_num,
				&uf_ex_ion->hf_rpt_num);
			r |= get_user(ge2d_config_ex_ion->hsc_start_phase_step,
				&uf_ex_ion->hsc_start_phase_step);
			r |= get_user(ge2d_config_ex_ion->hsc_phase_slope,
				&uf_ex_ion->hsc_phase_slope);
			r |= get_user(ge2d_config_ex_ion->vf_init_phase,
				&uf_ex_ion->vf_init_phase);
			r |= get_user(ge2d_config_ex_ion->vf_rpt_num,
				&uf_ex_ion->vf_rpt_num);
			r |= get_user(ge2d_config_ex_ion->vsc_start_phase_step,
				&uf_ex_ion->vsc_start_phase_step);
			r |= get_user(ge2d_config_ex_ion->vsc_phase_slope,
				&uf_ex_ion->vsc_phase_slope);
			r |= get_user
				(ge2d_config_ex_ion->src1_vsc_phase0_always_en,
				 &uf_ex_ion->src1_vsc_phase0_always_en);
			r |= get_user
				(ge2d_config_ex_ion->src1_hsc_phase0_always_en,
				 &uf_ex_ion->src1_hsc_phase0_always_en);
			r |= get_user(ge2d_config_ex_ion->src1_hsc_rpt_ctrl,
				      &uf_ex_ion->src1_hsc_rpt_ctrl);
			r |= get_user(ge2d_config_ex_ion->src1_vsc_rpt_ctrl,
				      &uf_ex_ion->src1_vsc_rpt_ctrl);

			for (i = 0; i < 4; i++) {
				struct config_planes_ion_s *psrc_planes;

				psrc_planes =
					&ge2d_config_ex_ion->src_planes[i];
				r |= get_user(psrc_planes->addr,
					&uf_ex_ion->src_planes[i].addr);
				r |= get_user(psrc_planes->w,
					&uf_ex_ion->src_planes[i].w);
				r |= get_user(psrc_planes->h,
					&uf_ex_ion->src_planes[i].h);
				r |= get_user(psrc_planes->shared_fd,
					&uf_ex_ion->src_planes[i].shared_fd);
			}

			for (i = 0; i < 4; i++) {
				struct config_planes_ion_s *psrc2_planes;

				psrc2_planes =
					&ge2d_config_ex_ion->src2_planes[i];
				r |= get_user(psrc2_planes->addr,
					&uf_ex_ion->src2_planes[i].addr);
				r |= get_user(psrc2_planes->w,
					&uf_ex_ion->src2_planes[i].w);
				r |= get_user(psrc2_planes->h,
					&uf_ex_ion->src2_planes[i].h);
				r |= get_user(psrc2_planes->shared_fd,
					&uf_ex_ion->src2_planes[i].shared_fd);
			}

			for (j = 0; j < 4; j++) {
				struct config_planes_ion_s *pdst_planes;

				pdst_planes =
					&ge2d_config_ex_ion->dst_planes[j];
				r |= get_user(pdst_planes->addr,
					&uf_ex_ion->dst_planes[j].addr);
				r |= get_user(pdst_planes->w,
					&uf_ex_ion->dst_planes[j].w);
				r |= get_user(pdst_planes->h,
					&uf_ex_ion->dst_planes[j].h);
				r |= get_user(pdst_planes->shared_fd,
					&uf_ex_ion->dst_planes[j].shared_fd);
			}
			if (r) {
				pr_err("GE2D_CONFIG_EX32 get parameter failed .\n");
				ret = -EFAULT;
				goto release1;
			}
			break;
#endif
	case GE2D_SET_COEF:
	case GE2D_ANTIFLICKER_ENABLE:
		break;
	case GE2D_REQUEST_BUFF:
		ret = copy_from_user(&ge2d_req_buf, argp,
				     sizeof(struct ge2d_dmabuf_req_s));
		if (ret < 0) {
			pr_err("Error user param\n");
			ret = -EINVAL;
			goto release1;
		}
		break;
	case GE2D_EXP_BUFF:
		ret = copy_from_user(&ge2d_exp_buf, argp,
				     sizeof(struct ge2d_dmabuf_exp_s));
		if (ret < 0) {
			pr_err("Error user param\n");
			ret = -EINVAL;
			goto release1;
		}
		break;
	case GE2D_FREE_BUFF:
		ret = copy_from_user(&index, argp,
				     sizeof(int));
		if (ret < 0) {
			pr_err("Error user param\n");
			ret = -EINVAL;
			goto release1;
		}
		break;
	case GE2D_SYNC_DEVICE:
	case GE2D_SYNC_CPU:
		ret = copy_from_user(&dma_fd, argp,
				     sizeof(int));
		if (ret < 0) {
			pr_err("Error user param\n");
			ret = -EINVAL;
			goto release1;
		}
		break;
	case GE2D_ATTACH_DMA_FD:
		ret = copy_from_user(&attatch, argp,
				     sizeof(struct ge2d_dmabuf_attach_s));
		if (ret < 0) {
			pr_err("Error user param\n");
			ret = -EINVAL;
			goto release1;
		}
		break;
	case GE2D_DETACH_DMA_FD:
		ret = copy_from_user(&data_type, argp,
				     sizeof(enum ge2d_data_type_e));
		if (ret < 0) {
			pr_err("Error user param\n");
			ret = -EINVAL;
			goto release1;
		}
		break;
	case GE2D_CONFIG_OLD:
	case GE2D_CONFIG_EX_OLD:
	case GE2D_SRCCOLORKEY_OLD:
		pr_err("ioctl not support yed.\n");
		ret = -EINVAL;
		goto release1;
	default:
		ret = copy_from_user(&para, argp, sizeof(struct ge2d_para_s));
		break;
	}

	switch (cmd) {
	case GE2D_CONFIG:
#ifdef CONFIG_COMPAT
	case GE2D_CONFIG32:
#endif
		ret = ge2d_context_config(context, &ge2d_config);
		break;
	case GE2D_CONFIG_EX:
#ifdef CONFIG_COMPAT
	case GE2D_CONFIG_EX32:
#endif
		ret = ge2d_context_config_ex(context, ge2d_config_ex);
		break;
	case GE2D_CONFIG_EX_ION:
#ifdef CONFIG_COMPAT
	case GE2D_CONFIG_EX32_ION:
#endif
		ret = ge2d_context_config_ex_ion(context, ge2d_config_ex_ion);
		break;
	case GE2D_CONFIG_EX_MEM:
#ifdef CONFIG_COMPAT
	case GE2D_CONFIG_EX32_MEM:
#endif
		ret = ge2d_ioctl_config_ex_mem(context, cmd, args);
		break;
	case GE2D_SET_COEF:
		ge2d_wq_set_scale_coef(context, args & 0xff, args >> 16);
		break;
	case GE2D_ANTIFLICKER_ENABLE:
		ge2d_antiflicker_enable(context, args);
		break;
	case GE2D_SRCCOLORKEY:
#ifdef CONFIG_COMPAT
	case GE2D_SRCCOLORKEY32:
#endif
		ge2d_log_dbg("src colorkey...,key_enable=0x%x,key_color=0x%x,key_mask=0x%x,key_mode=0x%x\n",
			     ge2d_config.src_key.key_enable,
			     ge2d_config.src_key.key_color,
			     ge2d_config.src_key.key_mask,
			     ge2d_config.src_key.key_mode);

		ge2dgen_src_key(context, ge2d_config.src_key.key_enable,
				ge2d_config.src_key.key_color,
				ge2d_config.src_key.key_mask,
				ge2d_config.src_key.key_mode); /* RGBA MODE */
		break;
	case GE2D_FILLRECTANGLE:
		ge2d_log_dbg("fill rect...,x=%d,y=%d,w=%d,h=%d,color=0x%x\n",
			     para.src1_rect.x, para.src1_rect.y,
			     para.src1_rect.w, para.src1_rect.h,
			     para.color);

		fillrect(context,
			 para.src1_rect.x, para.src1_rect.y,
			 para.src1_rect.w, para.src1_rect.h,
			 para.color);
		break;
	case GE2D_FILLRECTANGLE_NOBLOCK:
		ge2d_log_dbg("fill rect noblk\t");
		ge2d_log_dbg("x=%d,y=%d,w=%d,h=%d,color=0x%x\n",
			     para.src1_rect.x, para.src1_rect.y,
			     para.src1_rect.w, para.src1_rect.h,
			     para.color);

		fillrect_noblk(context,
			       para.src1_rect.x, para.src1_rect.y,
			       para.src1_rect.w, para.src1_rect.h,
			       para.color);
		break;
	case GE2D_STRETCHBLIT:
		/* stretch blit */
		ge2d_log_dbg("stretchblt\t");
		ge2d_log_dbg("x=%d,y=%d,w=%d,h=%d,dst.w=%d,dst.h=%d\n",
			     para.src1_rect.x, para.src1_rect.y,
			     para.src1_rect.w, para.src1_rect.h,
			     para.dst_rect.w, para.dst_rect.h);

		stretchblt(context,
			   para.src1_rect.x, para.src1_rect.y,
			   para.src1_rect.w, para.src1_rect.h,
			   para.dst_rect.x,  para.dst_rect.y,
			   para.dst_rect.w,  para.dst_rect.h);
		break;
	case GE2D_STRETCHBLIT_NOBLOCK:
		/* stretch blit */
		ge2d_log_dbg("stretchblt noblk\t");
		ge2d_log_dbg("x=%d,y=%d,w=%d,h=%d,dst.w=%d,dst.h=%d\n",
			     para.src1_rect.x, para.src1_rect.y,
			     para.src1_rect.w, para.src1_rect.h,
			     para.dst_rect.w, para.dst_rect.h);

		stretchblt_noblk(context,
				 para.src1_rect.x, para.src1_rect.y,
				 para.src1_rect.w, para.src1_rect.h,
				 para.dst_rect.x,  para.dst_rect.y,
				 para.dst_rect.w,  para.dst_rect.h);
		break;
	case GE2D_BLIT:
		/* bitblt */
		ge2d_log_dbg("blit...\n");
		bitblt(context,
		       para.src1_rect.x, para.src1_rect.y,
		       para.src1_rect.w, para.src1_rect.h,
		       para.dst_rect.x, para.dst_rect.y);
		break;
	case GE2D_BLIT_NOBLOCK:
		/* bitblt */
		ge2d_log_dbg("blit...,noblk\n");
		bitblt_noblk(context,
			     para.src1_rect.x, para.src1_rect.y,
			     para.src1_rect.w, para.src1_rect.h,
			     para.dst_rect.x, para.dst_rect.y);
		break;
	case GE2D_BLEND:
		ge2d_log_dbg("blend ...\n");
		blend(context,
		      para.src1_rect.x, para.src1_rect.y,
		      para.src1_rect.w, para.src1_rect.h,
		      para.src2_rect.x, para.src2_rect.y,
		      para.src2_rect.w, para.src2_rect.h,
		      para.dst_rect.x, para.dst_rect.y,
		      para.dst_rect.w, para.dst_rect.h,
		      para.op);
		break;
	case GE2D_BLEND_NOBLOCK:
		ge2d_log_dbg("blend ...,noblk\n");
		blend_noblk(context,
			    para.src1_rect.x, para.src1_rect.y,
			    para.src1_rect.w, para.src1_rect.h,
			    para.src2_rect.x, para.src2_rect.y,
			    para.src2_rect.w, para.src2_rect.h,
			    para.dst_rect.x, para.dst_rect.y,
			    para.dst_rect.w, para.dst_rect.h,
			    para.op);
		break;
	case GE2D_BLEND_NOALPHA:
		ge2d_log_dbg("blend_noalpha ...\n");
		blend_noalpha(context,
			      para.src1_rect.x, para.src1_rect.y,
			      para.src1_rect.w, para.src1_rect.h,
			      para.src2_rect.x, para.src2_rect.y,
			      para.src2_rect.w, para.src2_rect.h,
			      para.dst_rect.x, para.dst_rect.y,
			      para.dst_rect.w, para.dst_rect.h,
			      para.op);
			break;
	case GE2D_BLEND_NOALPHA_NOBLOCK:
		ge2d_log_dbg("blend_noalpha ...,noblk\n");
		blend_noalpha_noblk(context,
				    para.src1_rect.x, para.src1_rect.y,
				    para.src1_rect.w, para.src1_rect.h,
				    para.src2_rect.x, para.src2_rect.y,
				    para.src2_rect.w, para.src2_rect.h,
				    para.dst_rect.x, para.dst_rect.y,
				    para.dst_rect.w, para.dst_rect.h,
				    para.op);
		break;
	case GE2D_BLIT_NOALPHA:
		/* bitblt_noalpha */
		ge2d_log_dbg("blit_noalpha...\n");
		bitblt_noalpha(context,
			       para.src1_rect.x, para.src1_rect.y,
			       para.src1_rect.w, para.src1_rect.h,
			       para.dst_rect.x, para.dst_rect.y);
		break;
	case GE2D_BLIT_NOALPHA_NOBLOCK:
		/* bitblt_noalpha */
		ge2d_log_dbg("blit_noalpha...,noblk\n");
		bitblt_noalpha_noblk(context,
				     para.src1_rect.x, para.src1_rect.y,
				     para.src1_rect.w, para.src1_rect.h,
				     para.dst_rect.x, para.dst_rect.y);
		break;
	case GE2D_STRETCHBLIT_NOALPHA:
		/* stretch blit */
		ge2d_log_dbg("stretchblt_noalpha\t");
		ge2d_log_dbg("x=%d,y=%d,w=%d,h=%d,dst.w=%d,dst.h=%d\n",
			     para.src1_rect.x, para.src1_rect.y,
			     para.src1_rect.w, para.src1_rect.h,
			     para.dst_rect.w, para.dst_rect.h);

		stretchblt_noalpha(context,
				   para.src1_rect.x, para.src1_rect.y,
				   para.src1_rect.w, para.src1_rect.h,
				   para.dst_rect.x,  para.dst_rect.y,
				   para.dst_rect.w,  para.dst_rect.h);
		break;
	case GE2D_STRETCHBLIT_NOALPHA_NOBLOCK:
		/* stretch blit */
		ge2d_log_dbg("stretchblt_noalpha_noblk\t");
		ge2d_log_dbg("x=%d,y=%d,w=%d,h=%d,dst.w=%d,dst.h=%d\n",
			     para.src1_rect.x, para.src1_rect.y,
			     para.src1_rect.w, para.src1_rect.h,
			     para.dst_rect.w, para.dst_rect.h);

		stretchblt_noalpha_noblk(context,
					 para.src1_rect.x, para.src1_rect.y,
					 para.src1_rect.w, para.src1_rect.h,
					 para.dst_rect.x,  para.dst_rect.y,
					 para.dst_rect.w,  para.dst_rect.h);
		break;
	case GE2D_REQUEST_BUFF:
		ret = ge2d_buffer_alloc(&ge2d_req_buf);
		if (ret == 0)
			ret = copy_to_user(argp, &ge2d_req_buf,
					   sizeof(struct ge2d_dmabuf_req_s));
		break;
	case GE2D_EXP_BUFF:
		ret = ge2d_buffer_export(&ge2d_exp_buf);
		if (ret == 0)
			ret = copy_to_user(argp, &ge2d_exp_buf,
					   sizeof(struct ge2d_dmabuf_exp_s));
		break;
	case GE2D_FREE_BUFF:
		ret = ge2d_buffer_free(index);
		break;
	case GE2D_SYNC_DEVICE:
		ge2d_buffer_dma_flush(dma_fd);
		break;
	case GE2D_SYNC_CPU:
		ge2d_buffer_cache_flush(dma_fd);
		break;
	case GE2D_ATTACH_DMA_FD:
		ge2d_ioctl_attach_dma_fd(context, &attatch);
		break;
	case GE2D_DETACH_DMA_FD:
		ge2d_ioctl_detach_dma_fd(context, data_type);
		break;
	}
release1:
	kfree(ge2d_config_ex_ion);
release2:
	kfree(ge2d_config_ex);
	return ret;
}

#ifdef CONFIG_COMPAT
static long ge2d_compat_ioctl(struct file *filp,
			      unsigned int cmd, unsigned long args)
{
	unsigned long ret;

	args = (unsigned long)compat_ptr(args);
	ret = ge2d_ioctl(filp, cmd, args);

	return ret;
}
#endif

static int ge2d_release(struct inode *inode, struct file *file)
{
	struct ge2d_context_s *context = NULL;

	context = (struct ge2d_context_s *)file->private_data;
	if (context && (destroy_ge2d_work_queue(context) == 0)) {
		atomic_dec(&ge2d_device.open_count);
		return 0;
	}
	ge2d_log_dbg("release one ge2d device\n");
	return -1;
}

static struct ge2d_ctrl_s default_poweron_ctrl[] = {
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
static struct ge2d_ctrl_s default_poweroff_ctrl[] = {
			/* reset */
			{CBUS_BASE, RESET2_LEVEL, 0, 6, 1, 0},
			/* add isolation */
			{GEN_PWR_ISO0, AO_RTI_GEN_PWR_ISO0, 1, 19, 1, 0},
			/* Power down memory */
			{MEM_PD_REG0, HHI_MEM_PD_REG0, 0xFF, 18, 8, 0},
			/* power down ge2d */
			{GEN_PWR_SLEEP0, AO_RTI_GEN_PWR_SLEEP0, 1, 19, 1, 0}
		};

struct ge2d_power_table_s default_poweron_table = {5, default_poweron_ctrl};
struct ge2d_power_table_s default_poweroff_table = {4, default_poweroff_ctrl};

static struct ge2d_ctrl_s smc_poweron_ctrl[] = {
		{PWR_SMC, 0, 1, 0, 0}
	};
static struct ge2d_ctrl_s smc_poweroff_ctrl[] = {
		{PWR_SMC, 0, 0, 0, 0}
	};

struct ge2d_power_table_s smc_poweron_table = {1, smc_poweron_ctrl};
struct ge2d_power_table_s smc_poweroff_table = {1, smc_poweroff_ctrl};

static struct ge2d_ctrl_s runtime_poweron_ctrl[] = {
		{PWR_RUNTIME, 0, 1, 0, 0}
	};
static struct ge2d_ctrl_s runtime_poweroff_ctrl[] = {
		{PWR_RUNTIME, 0, 0, 0, 0}
	};

struct ge2d_power_table_s runtime_poweron_table = {1, runtime_poweron_ctrl};
struct ge2d_power_table_s runtime_poweroff_table = {1, runtime_poweroff_ctrl};

#ifndef CONFIG_AMLOGIC_REMOVE_OLD
static struct ge2d_device_data_s ge2d_gxl = {
	.ge2d_rate = 400000000,
	.src2_alp = 0,
	.canvas_status = 0,
	.deep_color = 0,
	.hang_flag = 0,
	.fifo = 0,
	.has_self_pwr = 0,
	.chip_type = MESON_CPU_MAJOR_ID_GXL,
};

static struct ge2d_device_data_s ge2d_gxm = {
	.ge2d_rate = 400000000,
	.src2_alp = 0,
	.canvas_status = 0,
	.deep_color = 0,
	.hang_flag = 0,
	.fifo = 0,
	.has_self_pwr = 0,
	.chip_type = MESON_CPU_MAJOR_ID_GXM,
};

static struct ge2d_device_data_s ge2d_txl = {
	.ge2d_rate = 400000000,
	.src2_alp = 0,
	.canvas_status = 0,
	.deep_color = 1,
	.hang_flag = 0,
	.fifo = 0,
	.has_self_pwr = 0,
	.chip_type = MESON_CPU_MAJOR_ID_TXL,
};

static struct ge2d_device_data_s ge2d_txlx = {
	.ge2d_rate = 400000000,
	.src2_alp = 0,
	.canvas_status = 0,
	.deep_color = 1,
	.hang_flag = 1,
	.fifo = 1,
	.has_self_pwr = 0,
	.chip_type = MESON_CPU_MAJOR_ID_TXLX,
};

static struct ge2d_device_data_s ge2d_axg = {
	.ge2d_rate = 400000000,
	.src2_alp = 0,
	.canvas_status = 1,
	.deep_color = 1,
	.hang_flag = 1,
	.fifo = 1,
	.has_self_pwr = 0,
	.chip_type = MESON_CPU_MAJOR_ID_AXG,
};

static struct ge2d_device_data_s ge2d_c1 = {
	.ge2d_rate = 400000000,
	.src2_alp = 1,
	.canvas_status = 2,
	.deep_color = 1,
	.hang_flag = 1,
	.fifo = 1,
	.has_self_pwr = 1,
	.poweron_table = &smc_poweron_table,
	.poweroff_table = &smc_poweroff_table,
	.chip_type = MESON_CPU_MAJOR_ID_C1,
};

static struct ge2d_device_data_s ge2d_c2 = {
	.ge2d_rate = 400000000,
	.src2_alp = 1,
	.canvas_status = 2,
	.deep_color = 1,
	.hang_flag = 1,
	.fifo = 1,
	.has_self_pwr = 1,
	.poweron_table = &smc_poweron_table,
	.poweroff_table = &smc_poweroff_table,
	.chip_type = MESON_CPU_MAJOR_ID_C2,
	.adv_matrix = 1,
	.src2_repeat = 1,
};
#endif

static struct ge2d_device_data_s ge2d_g12a = {
	.ge2d_rate = 500000000,
	.src2_alp = 1,
	.canvas_status = 0,
	.deep_color = 1,
	.hang_flag = 1,
	.fifo = 1,
	.has_self_pwr = 0,
	.chip_type = MESON_CPU_MAJOR_ID_G12A,
};

static struct ge2d_device_data_s ge2d_sm1 = {
	.ge2d_rate = 500000000,
	.src2_alp = 1,
	.canvas_status = 0,
	.deep_color = 1,
	.hang_flag = 1,
	.fifo = 1,
	.has_self_pwr = 1,
	.poweron_table = &default_poweron_table,
	.poweroff_table = &default_poweroff_table,
	.chip_type = MESON_CPU_MAJOR_ID_SM1,
};

static struct ge2d_device_data_s ge2d_sc2 = {
	.ge2d_rate = 500000000,
	.src2_alp = 1,
	.canvas_status = 0,
	.deep_color = 1,
	.hang_flag = 1,
	.fifo = 1,
	.has_self_pwr = 1,
	.poweron_table = &runtime_poweron_table,
	.poweroff_table = &runtime_poweroff_table,
	.chip_type = MESON_CPU_MAJOR_ID_SC2,
	.adv_matrix = 1,
	.src2_repeat = 1,
};

static struct ge2d_device_data_s ge2d_t5 = {
	.ge2d_rate = 500000000,
	.src2_alp = 1,
	.canvas_status = 0,
	.deep_color = 1,
	.hang_flag = 1,
	.fifo = 1,
	.has_self_pwr = 0,
	.chip_type = MESON_CPU_MAJOR_ID_T5,
};

static struct ge2d_device_data_s ge2d_t7 = {
	.ge2d_rate = 667000000,
	.src2_alp = 1,
	.canvas_status = 2,
	.deep_color = 1,
	.hang_flag = 1,
	.fifo = 1,
	.has_self_pwr = 1,
	.poweron_table = &runtime_poweron_table,
	.poweroff_table = &runtime_poweroff_table,
	.chip_type = MESON_CPU_MAJOR_ID_T7,
	.adv_matrix = 1,
	.src2_repeat = 1,
	.dst_repeat = 1,
	.dst_sign_mode = 1,
	.blk_stride_mode = 1,
};

static struct ge2d_device_data_s ge2d_s4 = {
	.ge2d_rate = 500000000,
	.src2_alp = 1,
	.canvas_status = 0,
	.deep_color = 1,
	.hang_flag = 1,
	.fifo = 1,
	.has_self_pwr = 1,
	.poweron_table = &runtime_poweron_table,
	.poweroff_table = &runtime_poweroff_table,
	.chip_type = MESON_CPU_MAJOR_ID_S4,
	.adv_matrix = 1,
	.src2_repeat = 1,
};

static const struct of_device_id ge2d_dt_match[] = {
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
	{
		.compatible = "amlogic, ge2d-gxl",
		.data = &ge2d_gxl,
	},
	{
		.compatible = "amlogic, ge2d-gxm",
		.data = &ge2d_gxm,
	},
	{
		.compatible = "amlogic, ge2d-txl",
		.data = &ge2d_txl,
	},
	{
		.compatible = "amlogic, ge2d-txlx",
		.data = &ge2d_txlx,
	},
	{
		.compatible = "amlogic, ge2d-axg",
		.data = &ge2d_axg,
	},
	{
		.compatible = "amlogic, ge2d-c1",
		.data = &ge2d_c1,
	},
	{
		.compatible = "amlogic, ge2d-c2",
		.data = &ge2d_c2,
	},
#endif
	{
		.compatible = "amlogic, ge2d-g12a",
		.data = &ge2d_g12a,
	},
	{
		.compatible = "amlogic, ge2d-sm1",
		.data = &ge2d_sm1,
	},
	{
		.compatible = "amlogic, ge2d-sc2",
		.data = &ge2d_sc2,
	},
	{
		.compatible = "amlogic, ge2d-t5",
		.data = &ge2d_t5,
	},
	{
		.compatible = "amlogic, ge2d-t7",
		.data = &ge2d_t7,
	},
	{
		.compatible = "amlogic, ge2d-s4",
		.data = &ge2d_s4,
	},
	{},
};

static int ge2d_probe(struct platform_device *pdev)
{
	int ret = 0;
	int irq = 0;
	int clk_cnt = -1;
	struct clk *clk_gate;
	struct clk *clk_vapb0;
	struct clk *clk;
	struct resource res;

	init_ge2d_device();

	if (pdev->dev.of_node) {
		const struct of_device_id *match;
		struct ge2d_device_data_s *ge2d_meson;
		struct device_node *of_node = pdev->dev.of_node;

		match = of_match_node(ge2d_dt_match, of_node);
		if (match) {
			ge2d_meson = (struct ge2d_device_data_s *)match->data;
			if (ge2d_meson) {
				memcpy(&ge2d_meson_dev, ge2d_meson,
				       sizeof(struct ge2d_device_data_s));
			} else {
				pr_err("%s data NOT match\n", __func__);
				return -ENODEV;
			}
		} else {
			pr_err("%s NOT match\n", __func__);
			return -ENODEV;
		}
	}
	/* get interrupt resource */
	irq = platform_get_irq_byname(pdev, "ge2d");
	if (irq == -ENXIO) {
		ge2d_log_err("get ge2d irq resource error\n");
		ret = -ENXIO;
		goto failed1;
	}

	clk_cnt = of_property_count_strings(pdev->dev.of_node, "clock-names");
	if (clk_cnt < 0) {
		ge2d_log_err("count clock-names err\n");
		ret = -ENOENT;
		goto failed1;
	}

	if (clk_cnt == 3) {
		clk_gate = devm_clk_get(&pdev->dev, "clk_ge2d_gate");
		if (IS_ERR_OR_NULL(clk_gate)) {
			ge2d_log_err("cannot get clock\n");
			clk_gate = NULL;
			ret = -ENOENT;
			goto failed1;
		}
		ge2d_log_dbg("clock source clk_ge2d_gate %p\n", clk_gate);
		clk_prepare_enable(clk_gate);

		clk = devm_clk_get(&pdev->dev, "clk_ge2d");
		if (IS_ERR_OR_NULL(clk)) {
			ge2d_log_err("cannot get clock\n");
			clk = NULL;
			ret = -ENOENT;
			goto failed1;
		}
		ge2d_log_dbg("clock clk_ge2d source %p\n", clk);
		clk_prepare_enable(clk);

		clk_vapb0 = devm_clk_get(&pdev->dev, "clk_vapb_0");
		if (PTR_ERR(clk_vapb0) != -ENOENT) {
			int vapb_rate, vpu_rate;

			if (!IS_ERR_OR_NULL(clk_vapb0)) {
				ge2d_log_dbg("clock source clk_vapb_0 %p\n",
					     clk_vapb0);
				vpu_rate = ge2d_meson_dev.ge2d_rate;
				vapb_rate = ge2d_meson_dev.ge2d_rate;
			#ifdef CONFIG_AMLOGIC_VPU
				vpu_rate = vpu_clk_get();
			#endif
				ge2d_log_info("ge2d init clock is %d HZ, VPU clock is %d HZ\n",
					      vapb_rate, vpu_rate);

				if (vpu_rate >= ge2d_meson_dev.ge2d_rate)
					vapb_rate = ge2d_meson_dev.ge2d_rate;
				else if (vpu_rate == 333330000)
					vapb_rate = 333333333;
				else if (vpu_rate == 166660000)
					vapb_rate = 166666667;
				else if (vapb_rate > vpu_rate)
					vapb_rate = vpu_rate;
				clk_set_rate(clk_vapb0, vapb_rate);
				clk_prepare_enable(clk_vapb0);
				vapb_rate = clk_get_rate(clk_vapb0);
				ge2d_log_info("ge2d clock is %d MHZ\n",
					      vapb_rate / 1000000);
			}
		}
	} else if (clk_cnt == 1) {
		clk_gate = devm_clk_get(&pdev->dev, "clk_ge2d_gate");
			if (!IS_ERR_OR_NULL(clk_gate)) {
				int clk_rate = ge2d_meson_dev.ge2d_rate;

				clk_set_rate(clk_gate, clk_rate);
				clk_prepare_enable(clk_gate);
				clk_rate = clk_get_rate(clk_gate);
				ge2d_log_info("ge2d clock is %d MHZ\n",
					      clk_rate / 1000000);
			}
	} else {
		ge2d_log_err("unsupported clk cnt\n");
		ret = -EINVAL;
		goto failed1;
	}

	ret = of_address_to_resource(pdev->dev.of_node, 0, &res);
	if (ret == 0) {
		ge2d_log_dbg("find address resource\n");
		if (res.start != 0) {
			ge2d_reg_map =
				ioremap(res.start, resource_size(&res));
			if (ge2d_reg_map) {
				ge2d_log_info("map io source 0x%p,size=%d to 0x%p\n",
					      (void *)res.start,
					      (int)resource_size(&res),
					      ge2d_reg_map);
			}
		} else {
			ge2d_reg_map = 0;
			ge2d_log_info("ignore io source start %p,size=%d\n",
				      (void *)res.start,
				      (int)resource_size(&res));
		}
	}
	ret = of_reserved_mem_device_init(&pdev->dev);
	if (ret < 0)
		ge2d_log_info("reserved mem is not used\n");

	ret = ge2d_wq_init(pdev, irq, clk_gate);

	clk_disable_unprepare(clk_gate);

	if (ge2d_meson_dev.has_self_pwr)
		pm_runtime_enable(&pdev->dev);
failed1:
	return ret;
}

static int ge2d_remove(struct platform_device *pdev)
{
	ge2d_log_info("%s\n", __func__);
	ge2d_wq_deinit();
	remove_ge2d_device();
	return 0;
}

static struct platform_driver ge2d_driver = {
	.probe      = ge2d_probe,
	.remove     = ge2d_remove,
	.driver     = {
		.name = "ge2d",
		.of_match_table = ge2d_dt_match,
	}
};

static int init_ge2d_device(void)
{
	int  ret = 0;

	strcpy(ge2d_device.name, "ge2d");
	ret = register_chrdev(0, ge2d_device.name, &ge2d_fops);
	if (ret <= 0) {
		ge2d_log_err("register ge2d device error\n");
		return  ret;
	}
	ge2d_device.major = ret;
	ge2d_device.dbg_enable = 0;
	ge2d_log_dbg("ge2d_dev major:%d\n", ret);
	ret = class_register(&ge2d_class);
	if (ret < 0) {
		ge2d_log_err("error create ge2d class\n");
		return ret;
	}
	ge2d_device.cla = &ge2d_class;
	ge2d_device.dev = device_create(ge2d_device.cla,
					NULL, MKDEV(ge2d_device.major,
					0), NULL, ge2d_device.name);
	if (IS_ERR_OR_NULL(ge2d_device.dev)) {
		ge2d_log_err("create ge2d device error\n");
		class_unregister(ge2d_device.cla);
		return -1;
	}
	return ret;
}

static int remove_ge2d_device(void)
{
	if (!ge2d_device.cla)
		return 0;

	if (ge2d_device.dev)
		device_destroy(ge2d_device.cla, MKDEV(ge2d_device.major, 0));
	class_unregister(ge2d_device.cla);
	unregister_chrdev(ge2d_device.major, ge2d_device.name);

	return  0;
}

int __init ge2d_init_module(void)
{
	return platform_driver_register(&ge2d_driver);
}

void __exit ge2d_remove_module(void)
{
	platform_driver_unregister(&ge2d_driver);
	ge2d_log_info("%s\n", __func__);
}

