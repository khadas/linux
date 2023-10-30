// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/io.h>
#include <linux/amlogic/media/vpq/vpq_cmd.h>
#include "vpq_ioctl.h"
#include "vpq_printk.h"
#include "vpq_table_type.h"
#include "vpq_table_logic.h"
#include "vpq_processor.h"

typedef int (*pvpq_ioctl_func)(struct file *file, unsigned long arg);
struct vpq_ioctl_func_s {
	int cmd;
	pvpq_ioctl_func pioctlfunc;
};

int vpq_ioctl_set_pqtable_param(struct file *file, unsigned long arg)
{
	int ret = 0;
	void __user *argp;
	unsigned char *buf;
	struct vpq_pqtable_bin_param_s pq_table_param;

	VPQ_PR_DRV("%s start\n", __func__);

	memset(&pq_table_param, 0, sizeof(struct vpq_pqtable_bin_param_s));

	if (copy_from_user(&pq_table_param,
			(void __user *)arg, sizeof(struct vpq_pqtable_bin_param_s))) {
		VPQ_PR_ERR("%s copy_from_user fail\n", __func__);
		return -EFAULT;
	}

	if (pq_table_param.len == 0) {
		VPQ_PR_ERR("%s pq_table_param.len is 0\n", __func__);
		return -EFAULT;
	}

	argp = (void __user *)pq_table_param.ptr;
	VPQ_PR_DRV("%s index:0x%x, len:0x%x\n",
		__func__, pq_table_param.index, pq_table_param.len);
	buf = vmalloc(pq_table_param.len);
	if (!buf) {
		VPQ_PR_ERR("%s vmalloc buf for receive PQ_TABLE_PARAM failed\n", __func__);
		vfree(buf);
		return -EFAULT;
	}
	if (copy_from_user((void *)buf, argp, pq_table_param.len)) {
		VPQ_PR_ERR("%s cp PQ_TABLE_PARAM to buf fail\n", __func__);
		vfree(buf);
		return -EFAULT;
	}

	pq_table_param.ptr = buf;
	//ret = vpq_init_default_pqtable(&pq_table_param);
	VPQ_PR_DRV("%s ret:%d\n", __func__, ret);

	vfree(buf);

	VPQ_PR_DRV("%s end\n", __func__);
	return ret;
}

int vpq_ioctl_set_pq_module_cfg(struct file *file, unsigned long arg)
{
	int ret = -1;

	ret = vpq_set_pq_module_cfg();

	VPQ_PR_DRV("%s ret:%d\n", __func__, ret);
	return ret;
}

int vpq_ioctl_set_brightness(struct file *file, unsigned long arg)
{
	int ret = -1;
	int value = 0;

	if (!pq_module_cfg.vadj1_en)
		return -EINVAL;

	if (copy_from_user(&value, (void __user *)arg, sizeof(int))) {
		VPQ_PR_ERR("%s copy_from_user fail\n", __func__);
		ret = -EFAULT;
	} else {
		VPQ_PR_INFO(PR_IOCTL, "%s value:%d\n", __func__, value);
		ret = vpq_set_brightness(value);
	}

	VPQ_PR_INFO(PR_IOCTL, "%s ret:%d\n", __func__, ret);
	return ret;
}

int vpq_ioctl_set_contrast(struct file *file, unsigned long arg)
{
	int ret = -1;
	int value = 0;

	if (!pq_module_cfg.vadj1_en)
		return -EINVAL;

	if (copy_from_user(&value, (void __user *)arg, sizeof(int))) {
		VPQ_PR_ERR("%s copy_from_user fail\n", __func__);
		ret = -EFAULT;
	} else {
		VPQ_PR_INFO(PR_IOCTL, "%s value:%d\n", __func__, value);
		ret = vpq_set_contrast(value);
	}

	VPQ_PR_INFO(PR_IOCTL, "%s ret:%d\n", __func__, ret);
	return ret;
}

int vpq_ioctl_set_saturation(struct file *file, unsigned long arg)
{
	int ret = -1;
	int value = 0;

	if (!pq_module_cfg.vadj1_en)
		return -EINVAL;

	if (copy_from_user(&value, (void __user *)arg, sizeof(int))) {
		VPQ_PR_ERR("%s copy_from_user fail\n", __func__);
		ret = -EFAULT;
	} else {
		VPQ_PR_INFO(PR_IOCTL, "%s value:%d\n", __func__, value);
		ret = vpq_set_saturation(value);
	}

	VPQ_PR_INFO(PR_IOCTL, "%s ret:%d\n", __func__, ret);
	return ret;
}

int vpq_ioctl_set_hue(struct file *file, unsigned long arg)
{
	int ret = -1;
	int value = 0;

	if (!pq_module_cfg.vadj1_en)
		return -EINVAL;

	if (copy_from_user(&value, (void __user *)arg, sizeof(int))) {
		VPQ_PR_ERR("%s copy_from_user fail\n", __func__);
		ret = -EFAULT;
	} else {
		VPQ_PR_INFO(PR_IOCTL, "%s value:%d\n", __func__, value);
		ret = vpq_set_hue(value);
	}

	VPQ_PR_INFO(PR_IOCTL, "%s ret:%d\n", __func__, ret);
	return ret;
}

int vpq_ioctl_set_sharpness(struct file *file, unsigned long arg)
{
	int ret = -1;
	int value = 0;

	if (!pq_module_cfg.sharpness0_en && !pq_module_cfg.sharpness1_en)
		return -EINVAL;

	if (copy_from_user(&value, (void __user *)arg, sizeof(int))) {
		VPQ_PR_ERR("%s copy_from_user fail\n", __func__);
		ret = -EFAULT;
	} else {
		VPQ_PR_INFO(PR_IOCTL, "%s value:%d\n", __func__, value);
		ret = vpq_set_sharpness(value);
	}

	VPQ_PR_INFO(PR_IOCTL, "%s ret:%d\n", __func__, ret);
	return ret;
}

int vpq_ioctl_set_brightness_post(struct file *file, unsigned long arg)
{
	int ret = -1;
	int value = 0;

	if (!pq_module_cfg.vadj2_en)
		return -EINVAL;

	if (copy_from_user(&value, (void __user *)arg, sizeof(int))) {
		VPQ_PR_ERR("%s copy_from_user fail\n", __func__);
		ret = -EFAULT;
	} else {
		VPQ_PR_INFO(PR_IOCTL, "%s value:%d\n", __func__, value);
		ret = vpq_set_brightness_post(value);
	}

	VPQ_PR_INFO(PR_IOCTL, "%s ret:%d\n", __func__, ret);
	return ret;
}

int vpq_ioctl_set_contrast_post(struct file *file, unsigned long arg)
{
	int ret = -1;
	int value = 0;

	if (!pq_module_cfg.vadj2_en)
		return -EINVAL;

	if (copy_from_user(&value, (void __user *)arg, sizeof(int))) {
		VPQ_PR_ERR("%s copy_from_user fail\n", __func__);
		ret = -EFAULT;
	} else {
		VPQ_PR_INFO(PR_IOCTL, "%s value:%d\n", __func__, value);
		ret = vpq_set_contrast_post(value);
	}

	VPQ_PR_INFO(PR_IOCTL, "%s ret:%d\n", __func__, ret);
	return ret;
}

int vpq_ioctl_set_saturation_post(struct file *file, unsigned long arg)
{
	int ret = -1;
	int value = 0;

	if (!pq_module_cfg.vadj2_en)
		return -EINVAL;

	if (copy_from_user(&value, (void __user *)arg, sizeof(int))) {
		VPQ_PR_ERR("%s copy_from_user fail\n", __func__);
		ret = -EFAULT;
	} else {
		VPQ_PR_INFO(PR_IOCTL, "%s value:%d\n", __func__, value);
		ret = vpq_set_saturation_post(value);
	}

	VPQ_PR_INFO(PR_IOCTL, "%s ret:%d\n", __func__, ret);
	return ret;
}

int vpq_ioctl_set_hue_post(struct file *file, unsigned long arg)
{
	int ret = -1;
	int value = 0;

	if (!pq_module_cfg.vadj2_en)
		return -EINVAL;

	if (copy_from_user(&value, (void __user *)arg, sizeof(int))) {
		VPQ_PR_ERR("%s copy_from_user fail\n", __func__);
		ret = -EFAULT;
	} else {
		VPQ_PR_INFO(PR_IOCTL, "%s value:%d\n", __func__, value);
		ret = vpq_set_hue_post(value);
	}

	VPQ_PR_INFO(PR_IOCTL, "%s ret:%d\n", __func__, ret);
	return ret;
}

int vpq_ioctl_set_overscan_table(struct file *file, unsigned long arg)
{
	int ret = -1;
	unsigned int buf_size = 0;
	struct vpq_overscan_table_s os_table;
	struct vpq_overscan_data_s *pos_data = NULL;

	if (copy_from_user(&os_table, (void __user *)arg,
		sizeof(struct vpq_overscan_table_s))) {
		VPQ_PR_ERR("%s copy_from_user fail\n", __func__);
		return -EFAULT;
	}

	if (os_table.length > VPQ_TIMING_MAX ||
		os_table.length <= 0) {
		VPQ_PR_ERR("%s length check fail\n", __func__);
		return -EFAULT;
	}

	buf_size = os_table.length * sizeof(struct vpq_overscan_data_s);
	pos_data = kmalloc(buf_size, GFP_KERNEL);
	if (!pos_data) {
		VPQ_PR_ERR("%s vmalloc buf failed\n", __func__);
		return -EFAULT;
	}

	if (copy_from_user(pos_data, (void __user *)os_table.param_ptr, buf_size)) {
		VPQ_PR_ERR("%s copy_from_user fail 2\n", __func__);
		ret = -EFAULT;
	} else {
		ret = vpq_set_overscan_data(os_table.length, pos_data);
	}

	kfree(pos_data);

	VPQ_PR_INFO(PR_IOCTL, "%s ret:%d\n", __func__, ret);
	return ret;
}

int vpq_ioctl_set_gamma_on_off(struct file *file, unsigned long arg)
{
	int ret = -1;
	int value = 0;

	if (!pq_module_cfg.gamma_en)
		return -EINVAL;

	if (copy_from_user(&value, (void __user *)arg, sizeof(int))) {
		VPQ_PR_ERR("%s copy_from_user fail\n", __func__);
		ret = -EFAULT;
	} else {
		VPQ_PR_INFO(PR_IOCTL, "%s value:%d\n", __func__, value);
		ret = vpq_set_pq_module_status(VPQ_MODULE_GAMMA, (value == 1) ? true : false);
	}

	VPQ_PR_INFO(PR_IOCTL, "%s ret:%d\n", __func__, ret);
	return ret;
}

int vpq_ioctl_set_gamma_index(struct file *file, unsigned long arg)
{
	int ret = -1;
	int value = 0;

	if (!pq_module_cfg.gamma_en)
		return -EINVAL;

	if (copy_from_user(&value, (void __user *)arg, sizeof(int))) {
		VPQ_PR_ERR("%s copy_from_user fail\n", __func__);
		ret = -EFAULT;
	} else {
		VPQ_PR_INFO(PR_IOCTL, "%s value:%d\n", __func__, value);
		ret = vpq_set_gamma_index(value);
	}

	VPQ_PR_INFO(PR_IOCTL, "%s ret:%d\n", __func__, ret);
	return ret;
}

int vpq_ioctl_set_gamma_table(struct file *file, unsigned long arg)
{
	int ret = -1;
	int i = 0;
	unsigned int buf_size = 0;
	struct vpq_gamma_table_s *pgam_tb = NULL;

	buf_size = sizeof(struct vpq_gamma_table_s);
	pgam_tb = kmalloc(buf_size, GFP_KERNEL);

	if (!pgam_tb) {
		VPQ_PR_ERR("%s vmalloc buf failed\n", __func__);
		kfree(pgam_tb);
		return -EFAULT;
	}

	if (copy_from_user(pgam_tb, (void __user *)arg, buf_size)) {
		VPQ_PR_ERR("%s copy_from_user fail\n", __func__);
		ret = -EFAULT;
	} else {
		for (i = 0; i < VPQ_GAMMA_TABLE_LEN; i++) {
			VPQ_PR_INFO(PR_IOCTL, "%s [%d]:%d %d %d\n",
				__func__, i,
				pgam_tb->r_data[i], pgam_tb->g_data[i], pgam_tb->b_data[i]);
		}

		ret = vpq_set_gamma_table(pgam_tb);
	}

	kfree(pgam_tb);

	VPQ_PR_INFO(PR_IOCTL, "%s ret:%d\n", __func__, ret);
	return ret;
}

int vpq_ioctl_set_blend_gamma(struct file *file, unsigned long arg)
{
	int ret = -1;
	struct vpq_blend_gamma_s blend_gamma;

	memset(&blend_gamma, 0, sizeof(struct vpq_blend_gamma_s));

	if (copy_from_user(&blend_gamma, (void __user *)arg, sizeof(struct vpq_blend_gamma_s))) {
		VPQ_PR_ERR("%s copy_from_user fail\n", __func__);
		ret = -EFAULT;
	} else {
		VPQ_PR_INFO(PR_IOCTL, "%s blend_gamma:%d,%d\n",
			__func__,
			blend_gamma.gamma_curve, blend_gamma.ctemp_mode);

		ret = vpq_set_blend_gamma(&blend_gamma);
	}

	VPQ_PR_INFO(PR_IOCTL, "%s ret:%d\n", __func__, ret);
	return ret;
}

int vpq_ioctl_set_pre_gamma_table(struct file *file, unsigned long arg)
{
	int ret = -1;
	int i = 0;
	unsigned int buf_size = 0;
	struct vpq_pre_gamma_table_s *pre_gam_tb = NULL;

	if (!pq_module_cfg.pregamma_en)
		return -EINVAL;

	buf_size = sizeof(struct vpq_pre_gamma_table_s);
	pre_gam_tb = kmalloc(buf_size, GFP_KERNEL);

	if (!pre_gam_tb) {
		VPQ_PR_ERR("%s vmalloc buf failed\n", __func__);
		kfree(pre_gam_tb);
		return -EFAULT;
	}

	if (copy_from_user(pre_gam_tb, (void __user *)arg, buf_size)) {
		VPQ_PR_ERR("%s copy_from_user fail\n", __func__);
		ret = -EFAULT;
	} else {
		for (i = 0; i < VPQ_PRE_GAMMA_TABLE_LEN; i++) {
			VPQ_PR_INFO(PR_IOCTL, "%s [%d]:%d %d %d\n",
				__func__, i,
				pre_gam_tb->r_data[i], pre_gam_tb->g_data[i],
				pre_gam_tb->b_data[i]);
		}

		ret = vpq_set_pre_gamma_table(pre_gam_tb);
	}

	kfree(pre_gam_tb);

	VPQ_PR_INFO(PR_IOCTL, "%s ret:%d\n", __func__, ret);
	return ret;
}

int vpq_ioctl_set_rgb_ogo(struct file *file, unsigned long arg)
{
	int ret = -1;
	struct vpq_rgb_ogo_s rgb_ogo;

	if (!pq_module_cfg.wb_en)
		return -EINVAL;

	memset(&rgb_ogo, 0, sizeof(struct vpq_rgb_ogo_s));

	if (copy_from_user(&rgb_ogo, (void __user *)arg, sizeof(struct vpq_rgb_ogo_s))) {
		VPQ_PR_ERR("%s copy_from_user fail\n", __func__);
		ret = -EFAULT;
	} else {
		VPQ_PR_INFO(PR_IOCTL, "%s rgbogo:%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
			__func__,
			rgb_ogo.en,
			rgb_ogo.r_pre_offset, rgb_ogo.g_pre_offset, rgb_ogo.b_pre_offset,
			rgb_ogo.r_gain, rgb_ogo.g_gain, rgb_ogo.b_gain,
			rgb_ogo.r_post_offset, rgb_ogo.g_post_offset, rgb_ogo.b_post_offset);

		ret = vpq_set_rgb_ogo(&rgb_ogo);
	}

	VPQ_PR_INFO(PR_IOCTL, "%s ret:%d\n", __func__, ret);
	return ret;
}

int vpq_ioctl_set_color_customize(struct file *file, unsigned long arg)
{
	int ret = -1;
	struct vpq_cms_s cms_param;

	memset(&cms_param, 0, sizeof(struct vpq_cms_s));

	if (copy_from_user(&cms_param, (void __user *)arg, sizeof(struct vpq_cms_s))) {
		VPQ_PR_ERR("%s copy_from_user fail\n", __func__);
		ret = -EFAULT;
	} else {
		VPQ_PR_INFO(PR_IOCTL, "%s cms_param:%d,%d,%d,%d,%d\n",
			__func__,
			cms_param.color_type, cms_param.color_9, cms_param.color_14,
			cms_param.cms_type, cms_param.value);

		ret = vpq_set_color_customize(&cms_param);
	}

	VPQ_PR_INFO(PR_IOCTL, "%s ret:%d\n", __func__, ret);
	return ret;
}

int vpq_ioctl_set_black_stretch(struct file *file, unsigned long arg)
{
	int ret = -1;
	int value = 0;

	if (!pq_module_cfg.black_ext_en)
		return -EINVAL;

	if (copy_from_user(&value, (void __user *)arg, sizeof(int))) {
		VPQ_PR_ERR("%s copy_from_user fail\n", __func__);
		ret = -EFAULT;
	} else {
		VPQ_PR_INFO(PR_IOCTL, "%s value:%d\n", __func__, value);
		ret = vpq_set_black_stretch(value);
	}

	VPQ_PR_INFO(PR_IOCTL, "%s ret:%d\n", __func__, ret);
	return ret;
}

int vpq_ioctl_set_dnlp_mode(struct file *file, unsigned long arg)
{
	int ret = -1;
	int value = 0;

	if (!pq_module_cfg.dnlp_en)
		return -EINVAL;

	if (copy_from_user(&value, (void __user *)arg, sizeof(int))) {
		VPQ_PR_ERR("%s copy_from_user fail\n", __func__);
		ret = -EFAULT;
	} else {
		VPQ_PR_INFO(PR_IOCTL, "%s value:%d\n", __func__, value);
		ret = vpq_set_dnlp_mode(value);
	}

	VPQ_PR_INFO(PR_IOCTL, "%s ret:%d\n", __func__, ret);
	return ret;
}

int vpq_ioctl_set_lc_mode(struct file *file, unsigned long arg)
{
	int ret = -1;
	int value = 0;

	if (!pq_module_cfg.lc_en)
		return -EINVAL;

	if (copy_from_user(&value, (void __user *)arg, sizeof(int))) {
		VPQ_PR_ERR("%s copy_from_user fail\n", __func__);
		ret = -EFAULT;
	} else {
		VPQ_PR_INFO(PR_IOCTL, "%s value:%d\n", __func__, value);
		ret = vpq_set_lc_mode(value);
	}

	VPQ_PR_INFO(PR_IOCTL, "%s ret:%d\n", __func__, ret);
	return ret;
}

int vpq_ioctl_set_nr(struct file *file, unsigned long arg)
{
	int ret = -1;
	int value = 0;

	if (copy_from_user(&value, (void __user *)arg, sizeof(int))) {
		VPQ_PR_ERR("%s copy_from_user fail\n", __func__);
		ret = -EFAULT;
	} else {
		VPQ_PR_INFO(PR_IOCTL, "%s value:%d\n", __func__, value);
		ret = vpq_set_nr(value);
	}

	VPQ_PR_INFO(PR_IOCTL, "%s ret:%d\n", __func__, ret);
	return ret;
}

int vpq_ioctl_set_deblock(struct file *file, unsigned long arg)
{
	int ret = -1;
	int value = 0;

	if (copy_from_user(&value, (void __user *)arg, sizeof(int))) {
		VPQ_PR_ERR("%s copy_from_user fail\n", __func__);
		ret = -EFAULT;
	} else {
		VPQ_PR_INFO(PR_IOCTL, "%s value:%d\n", __func__, value);
		ret = vpq_set_deblock(value);
	}

	VPQ_PR_INFO(PR_IOCTL, "%s ret:%d\n", __func__, ret);
	return ret;
}

int vpq_ioctl_set_demosquito(struct file *file, unsigned long arg)
{
	int ret = -1;
	int value = 0;

	if (copy_from_user(&value, (void __user *)arg, sizeof(int))) {
		VPQ_PR_ERR("%s copy_from_user fail\n", __func__);
		ret = -EFAULT;
	} else {
		VPQ_PR_INFO(PR_IOCTL, "%s value:%d\n", __func__, value);
		ret = vpq_set_demosquito(value);
	}

	VPQ_PR_INFO(PR_IOCTL, "%s ret:%d\n", __func__, ret);
	return ret;
}

int vpq_ioctl_set_smoothplus_mode(struct file *file, unsigned long arg)
{
	int ret = -1;
	int value = 0;

	if (copy_from_user(&value, (void __user *)arg, sizeof(int))) {
		VPQ_PR_ERR("%s copy_from_user fail\n", __func__);
		ret = -EFAULT;
	} else {
		VPQ_PR_INFO(PR_IOCTL, "%s value:%d\n", __func__, value);
		ret = vpq_set_smoothplus_mode(value);
	}

	VPQ_PR_INFO(PR_IOCTL, "%s ret:%d\n", __func__, ret);
	return ret;
}

int vpq_ioctl_get_hist_avg(struct file *file, unsigned long arg)
{
	int ret = -1;
	struct vpq_histgm_ave_s hist_ave;

	memset(&hist_ave, 0, sizeof(struct vpq_histgm_ave_s));

	ret = vpq_get_hist_avg(&hist_ave);
	VPQ_PR_INFO(PR_IOCTL, "%s hist_ave:%d,%d,%d,%d\n",
		__func__,
		hist_ave.sum, hist_ave.width, hist_ave.height, hist_ave.ave);

	if (copy_to_user((void __user *)arg, &hist_ave, sizeof(struct vpq_histgm_ave_s))) {
		VPQ_PR_INFO(PR_IOCTL, "%s copy_from_user fail\n", __func__);
		ret = -EFAULT;
	} else {
		;
	}

	VPQ_PR_INFO(PR_IOCTL, "%s ret:%d\n", __func__, ret);
	return ret;
}

int vpq_ioctl_get_histogram(struct file *file, unsigned long arg)
{
	int ret = -1;
	int buf_size = 0;
	int i = 0;
	struct vpq_histgm_param_s *phist_param = NULL;

	buf_size = sizeof(struct vpq_histgm_param_s);
	phist_param = kmalloc(buf_size, GFP_KERNEL);
	if (!phist_param) {
		VPQ_PR_ERR("%s vmalloc buf failed\n", __func__);
		vfree(phist_param);
		return -EFAULT;
	}

	ret = vpq_get_histogram(phist_param);
	VPQ_PR_INFO(PR_IOCTL, "%s hist_param:%d,%d,%d\n",
		__func__,
		phist_param->hist_pow, phist_param->luma_sum, phist_param->pixel_sum);
	for (i = 0; i < VPQ_HIST_BIN_COUNT; i++) {
		VPQ_PR_INFO(PR_IOCTL, "%s histgm[%d]:%d\n",
			__func__, i, phist_param->histgm[i]);
		VPQ_PR_INFO(PR_IOCTL, "%s dark_histgm[%d]:%d\n",
			__func__, i, phist_param->dark_histgm[i]);
	}
	i = 0;
	for (i = 0; i < VPQ_COLOR_HIST_BIN_COUNT; i++) {
		VPQ_PR_INFO(PR_IOCTL, "%s hue_histgm[%d]:%d\n",
			__func__, i, phist_param->hue_histgm[i]);
		VPQ_PR_INFO(PR_IOCTL, "%s sat_histgm[%d]:%d\n",
			__func__, i, phist_param->sat_histgm[i]);
	}

	if (copy_to_user((void __user *)arg, phist_param, buf_size)) {
		VPQ_PR_ERR("%s copy_from_user fail\n", __func__);
		ret = -EFAULT;
	} else {
		;
	}

	VPQ_PR_INFO(PR_IOCTL, "%s ret:%d\n", __func__, ret);
	return ret;
}

int vpq_ioctl_set_csc_type(struct file *file, unsigned long arg)
{
	int ret = -1;
	int value = 0;

	if (copy_from_user(&value, (void __user *)arg, sizeof(enum vpq_csc_type_e))) {
		VPQ_PR_ERR("%s copy_from_user fail\n", __func__);
		ret = -EFAULT;
	} else {
		VPQ_PR_INFO(PR_IOCTL, "%s value:%d\n", __func__, value);
		ret = vpq_set_csc_type(value);
	}

	VPQ_PR_INFO(PR_IOCTL, "%s ret:%d\n", __func__, ret);
	return ret;
}

int vpq_ioctl_get_csc_type(struct file *file, unsigned long arg)
{
	int ret = 0;
	int value = 0;

	value = vpq_get_csc_type();
	VPQ_PR_INFO(PR_IOCTL, "%s value:%d\n", __func__, value);

	if (copy_to_user((void __user *)arg, &value, sizeof(enum vpq_csc_type_e))) {
		VPQ_PR_ERR("%s copy_from_user fail\n", __func__);
		ret = -EFAULT;
	} else {
		;
	}

	VPQ_PR_INFO(PR_IOCTL, "%s ret:%d\n", __func__, ret);
	return ret;
}

int vpq_ioctl_set_hdr_tmo_mode(struct file *file, unsigned long arg)
{
	int ret = -1;
	int value = 0;

	if (copy_from_user(&value, (void __user *)arg, sizeof(int))) {
		VPQ_PR_ERR("%s copy_from_user fail\n", __func__);
		ret = -EFAULT;
	} else {
		VPQ_PR_INFO(PR_IOCTL, "%s value:%d\n", __func__, value);
		ret = vpq_set_hdr_tmo_mode(value);
	}

	VPQ_PR_INFO(PR_IOCTL, "%s ret:%d\n", __func__, ret);
	return ret;
}

int vpq_ioctl_set_hdr_tmo_data(struct file *file, unsigned long arg)
{
	int ret = -1;
	int i = 0;
	unsigned int buf_size = 0;
	int *ptmp_data = NULL;
	struct vpq_hdr_lut_s hdr_lut_data;

	memset(&hdr_lut_data, 0, sizeof(struct vpq_hdr_lut_s));

	if (copy_from_user(&hdr_lut_data, (void __user *)arg,
		sizeof(struct vpq_hdr_lut_s))) {
		VPQ_PR_ERR("%s copy_from_user fail\n", __func__);
		ret = -EFAULT;
	} else {
		if (hdr_lut_data.lut_size == 0)
			return -EFAULT;

		buf_size = hdr_lut_data.lut_size * sizeof(int);
		ptmp_data = kmalloc(buf_size, GFP_KERNEL);
		if (!ptmp_data) {
			VPQ_PR_ERR("%s vmalloc buf failed\n", __func__);
			return -EFAULT;
		}

		if (copy_from_user(ptmp_data, (void __user *)arg, buf_size)) {
			VPQ_PR_ERR("%s copy_from_user fail 2\n", __func__);
			ret = -EFAULT;
		} else {
			hdr_lut_data.lut_data = ptmp_data;

			for (i = 0; i < hdr_lut_data.lut_size; i++)
				VPQ_PR_INFO(PR_IOCTL, "%s lut_data[%d]:%d\n",
					__func__, i, hdr_lut_data.lut_data[i]);

			ret = vpq_set_hdr_tmo_data(&hdr_lut_data);
		}
	}

	kfree(ptmp_data);

	VPQ_PR_INFO(PR_IOCTL, "%s ret:%d\n", __func__, ret);
	return ret;
}

int vpq_ioctl_set_aipq_mode(struct file *file, unsigned long arg)
{
	int ret = -1;
	int value = 0;

	if (copy_from_user(&value, (void __user *)arg, sizeof(int))) {
		VPQ_PR_ERR("%s copy_from_user fail\n", __func__);
		ret = -EFAULT;
	} else {
		VPQ_PR_INFO(PR_IOCTL, "%s value:%d\n", __func__, value);
		ret = vpq_set_aipq_mode(value);
	}

	VPQ_PR_INFO(PR_IOCTL, "%s ret:%d\n", __func__, ret);
	return ret;
}

int vpq_ioctl_set_aisr_mode(struct file *file, unsigned long arg)
{
	int ret = -1;
	int value = 0;

	if (copy_from_user(&value, (void __user *)arg, sizeof(int))) {
		VPQ_PR_ERR("%s copy_from_user fail\n", __func__);
		ret = -EFAULT;
	} else {
		VPQ_PR_INFO(PR_IOCTL, "%s value:%d\n", __func__, value);
		ret = vpq_set_aisr_mode(value);
	}

	VPQ_PR_INFO(PR_IOCTL, "%s ret:%d\n", __func__, ret);
	return ret;
}

int vpq_ioctl_get_color_primary(struct file *file, unsigned long arg)
{
	int value = 0;

	value = (int)vpq_get_color_primary();
	VPQ_PR_INFO(PR_IOCTL, "%s value:%d\n", __func__, value);

	if (copy_to_user((void __user *)arg, &value, sizeof(int))) {
		VPQ_PR_ERR("%s copy_from_user fail\n", __func__);
		return -EFAULT;
	}

	return 0;
}

int vpq_ioctl_get_hdr_metadata(struct file *file, unsigned long arg)
{
	int ret = -1;
	int i = 0;
	int j = 0;
	struct vpq_hdr_metadata_s hdr_metadata;

	memset(&hdr_metadata, 0, sizeof(struct vpq_hdr_metadata_s));

	ret = vpq_get_hdr_metadata(&hdr_metadata);
	for (i = 0; i < 3; i++) {
		for (j = 0; j < 2; j++)
			VPQ_PR_INFO(PR_IOCTL, "%s primaries[%d][%d]:%d\n",
				__func__, i, j, hdr_metadata.primaries[i][j]);
	}
	i = 0;
	for (i = 0; i < 2; i++) {
		VPQ_PR_INFO(PR_IOCTL, "%s white_point[%d]:%d\n",
			__func__, i, hdr_metadata.white_point[i]);
		VPQ_PR_INFO(PR_IOCTL, "%s luminance[%d]:%d\n",
			__func__, i, hdr_metadata.luminance[i]);
	}

	if (copy_to_user((void __user *)arg, &hdr_metadata, sizeof(struct vpq_hdr_metadata_s))) {
		VPQ_PR_ERR("%s copy_from_user fail\n", __func__);
		ret = -EFAULT;
	} else {
		;
	}

	VPQ_PR_INFO(PR_IOCTL, "%s ret:%d\n", __func__, ret);
	return ret;
}

int vpq_ioctl_set_blue_stretch(struct file *file, unsigned long arg)
{
	int ret = -1;
	int value = 0;

	if (!pq_module_cfg.blue_stretch_en)
		return -EINVAL;

	if (copy_from_user(&value, (void __user *)arg, sizeof(int))) {
		VPQ_PR_ERR("%s copy_from_user fail\n", __func__);
		ret = -EFAULT;
	} else {
		VPQ_PR_INFO(PR_IOCTL, "%s value:%d\n", __func__, value);
		ret = vpq_set_blue_stretch(value);
	}

	VPQ_PR_INFO(PR_IOCTL, "%s ret:%d\n", __func__, ret);
	return ret;
}

int vpq_ioctl_set_chroma_coring(struct file *file, unsigned long arg)
{
	int ret = -1;
	int value = 0;

	if (!pq_module_cfg.chroma_cor_en)
		return -EINVAL;

	if (copy_from_user(&value, (void __user *)arg, sizeof(int))) {
		VPQ_PR_ERR("%s copy_from_user fail\n", __func__);
		ret = -EFAULT;
	} else {
		VPQ_PR_INFO(PR_IOCTL, "%s value:%d\n", __func__, value);
		ret = vpq_set_chroma_coring(value);
	}

	VPQ_PR_INFO(PR_IOCTL, "%s ret:%d\n", __func__, ret);
	return ret;
}

int vpq_ioctl_set_xvycc_mode(struct file *file, unsigned long arg)
{
	int ret = -1;
	int value = 0;

	if (copy_from_user(&value, (void __user *)arg, sizeof(int))) {
		VPQ_PR_ERR("%s copy_from_user fail\n", __func__);
		ret = -EFAULT;
	} else {
		VPQ_PR_INFO(PR_IOCTL, "%s value:%d\n", __func__, value);
		ret = vpq_set_xvycc_mode(value);
	}

	VPQ_PR_INFO(PR_IOCTL, "%s ret:%d\n", __func__, ret);
	return ret;
}

int vpq_ioctl_set_mcdi_mode(struct file *file, unsigned long arg)
{
	int ret = -1;
	int value = 0;

	if (copy_from_user(&value, (void __user *)arg, sizeof(int))) {
		VPQ_PR_ERR("%s copy_from_user fail\n", __func__);
		ret = -EFAULT;
	} else {
		VPQ_PR_INFO(PR_IOCTL, "%s value:%d\n", __func__, value);
		ret = vpq_set_mcdi_mode(value);
	}

	VPQ_PR_INFO(PR_IOCTL, "%s ret:%d\n", __func__, ret);
	return ret;
}

int vpq_ioctl_set_color_base(struct file *file, unsigned long arg)
{
	int ret = -1;
	int value = 0;

	if (copy_from_user(&value, (void __user *)arg, sizeof(int))) {
		VPQ_PR_ERR("%s copy_from_user fail\n", __func__);
		ret = -EFAULT;
	} else {
		VPQ_PR_INFO(PR_IOCTL, "%s value:%d\n", __func__, value);
		ret = vpq_set_color_base(value);
	}

	VPQ_PR_INFO(PR_IOCTL, "%s ret:%d\n", __func__, ret);
	return ret;
}

int vpq_ioctl_set_cabc(struct file *file, unsigned long arg)
{
	int ret = -1;

	ret = vpq_set_cabc();

	VPQ_PR_INFO(PR_IOCTL, "%s ret:%d\n", __func__, ret);
	return ret;
}

int vpq_ioctl_set_add(struct file *file, unsigned long arg)
{
	int ret = -1;

	ret = vpq_set_add();

	VPQ_PR_INFO(PR_IOCTL, "%s ret:%d\n", __func__, ret);
	return ret;
}

int vpq_ioctl_set_pll_value(struct file *file, unsigned long arg)
{
	int ret = -1;

	ret = vpq_set_pll_value();

	VPQ_PR_INFO(PR_IOCTL, "%s ret:%d\n", __func__, ret);
	return ret;
}

int vpq_ioctl_set_pc_mode(struct file *file, unsigned long arg)
{
	int ret = -1;
	int value = 0;

	if (copy_from_user(&value, (void __user *)arg, sizeof(enum vpq_pc_mode_e))) {
		VPQ_PR_ERR("%s copy_from_user fail\n", __func__);
		ret = -EFAULT;
	} else {
		VPQ_PR_INFO(PR_IOCTL, "%s value:%d\n", __func__, value);
		ret = vpq_set_pc_mode(value);
	}

	VPQ_PR_INFO(PR_IOCTL, "%s ret:%d\n", __func__, ret);
	return ret;
}

int vpq_ioctl_get_event_info(struct file *file, unsigned long arg)
{
	enum vpq_event_info_e event_info = VPQ_EVENT_NONE;

	event_info = (enum vpq_event_info_e)vpq_get_event_info();
	VPQ_PR_INFO(PR_IOCTL, "%s event_info:%d\n", __func__, event_info);

	if (copy_to_user((void __user *)arg, &event_info, sizeof(enum vpq_event_info_e))) {
		VPQ_PR_ERR("%s copy_from_user fail\n", __func__);
		return -EFAULT;
	}

	return 0;
}

int vpq_ioctl_get_signal_info(struct file *file, unsigned long arg)
{
	struct vpq_signal_info_s sig_info;

	memset(&sig_info, 0, sizeof(struct vpq_signal_info_s));

	vpq_get_signal_info(&sig_info);

	VPQ_PR_INFO(PR_IOCTL, "%s sig_info:%d %d %d %d %d %d\n",
		__func__,
		sig_info.src_type, sig_info.hdmi_port, sig_info.sig_mode,
		sig_info.hdr_type, sig_info.height, sig_info.width);

	if (copy_to_user((void __user *)arg, &sig_info, sizeof(struct vpq_signal_info_s))) {
		VPQ_PR_ERR("%s copy_from_user fail\n", __func__);
		return -EFAULT;
	}

	return 0;
}

int vpq_ioctl_set_frame_status(struct file *file, unsigned long arg)
{
	int ret = -1;
	int value = 0;

	if (copy_from_user(&value, (void __user *)arg, sizeof(enum vpq_frame_status_e))) {
		VPQ_PR_ERR("%s copy_from_user fail\n", __func__);
		ret = -EFAULT;
	} else {
		VPQ_PR_INFO(PR_IOCTL, "%s value:%d\n", __func__, value);
		ret = vpq_set_frame_status(value);
		ret |= vpq_processor_set_frame_status(value);
	}

	VPQ_PR_INFO(PR_IOCTL, "%s ret:%d\n", __func__, ret);
	return ret;
}

int vpq_ioctl_set_frame(struct file *file, unsigned long arg)
{
	int ret = -1;
	struct vpq_frame_info_s frame_info;

	memset(&frame_info, 0, sizeof(struct vpq_frame_info_s));

	if (copy_from_user(&frame_info, (void __user *)arg, sizeof(struct vpq_frame_info_s))) {
		VPQ_PR_ERR("%s copy_from_user fail\n", __func__);
		ret = -EFAULT;
	} else {
		VPQ_PR_INFO(PR_IOCTL, "%s shared_fd:%d\n", __func__, frame_info.shared_fd);
		ret = vpq_process_set_frame(file->private_data, &frame_info);
	}

	VPQ_PR_INFO(PR_IOCTL, "%s ret:%d\n", __func__, ret);
	return ret;
}

static struct vpq_ioctl_func_s st_ioctlinfo[] = {
	{VPQ_IOC_SET_PQTABLE_PARAM,          vpq_ioctl_set_pqtable_param},
	{VPQ_IOC_SET_PQ_MODULE_CFG,          vpq_ioctl_set_pq_module_cfg},
	{VPQ_IOC_SET_BRIGHTNESS,             vpq_ioctl_set_brightness},
	{VPQ_IOC_SET_CONTRAST,               vpq_ioctl_set_contrast},
	{VPQ_IOC_SET_SATURATION,             vpq_ioctl_set_saturation},
	{VPQ_IOC_SET_HUE,                    vpq_ioctl_set_hue},
	{VPQ_IOC_SET_SHARPNESS,              vpq_ioctl_set_sharpness},
	{VPQ_IOC_SET_BRIGHTNESS_POST,        vpq_ioctl_set_brightness_post},
	{VPQ_IOC_SET_CONTRAST_POST,          vpq_ioctl_set_contrast_post},
	{VPQ_IOC_SET_SATURATION_POST,        vpq_ioctl_set_saturation_post},
	{VPQ_IOC_SET_HUE_POST,               vpq_ioctl_set_hue_post},
	{VPQ_IOC_SET_OVERSCAN_TABLE,         vpq_ioctl_set_overscan_table},
	{VPQ_IOC_SET_GAMMA_ONOFF,            vpq_ioctl_set_gamma_on_off},
	{VPQ_IOC_SET_GAMMA_INDEX,            vpq_ioctl_set_gamma_index},
	{VPQ_IOC_SET_GAMMA_TABLE,            vpq_ioctl_set_gamma_table},
	{VPQ_IOC_SET_BLEND_GAMMA,            vpq_ioctl_set_blend_gamma},
	{VPQ_IOC_SET_PRE_GAMMA_DATA,         vpq_ioctl_set_pre_gamma_table},
	{VPQ_IOC_SET_RGB_OGO,                vpq_ioctl_set_rgb_ogo},
	{VPQ_IOC_SET_COLOR_CUSTOMIZE,        vpq_ioctl_set_color_customize},
	{VPQ_IOC_SET_BLACK_STRETCH,          vpq_ioctl_set_black_stretch},
	{VPQ_IOC_SET_DNLP_MODE,              vpq_ioctl_set_dnlp_mode},
	{VPQ_IOC_SET_LC_MODE,                vpq_ioctl_set_lc_mode},
	{VPQ_IOC_SET_NR,                     vpq_ioctl_set_nr},
	{VPQ_IOC_SET_DEBLOCK,                vpq_ioctl_set_deblock},
	{VPQ_IOC_SET_DEMOSQUITO,             vpq_ioctl_set_demosquito},
	{VPQ_IOC_SET_SMOOTHPLUS_MODE,        vpq_ioctl_set_smoothplus_mode},
	{VPQ_IOC_GET_HIST_AVG,               vpq_ioctl_get_hist_avg},
	{VPQ_IOC_GET_HIST_BIN,               vpq_ioctl_get_histogram},
	{VPQ_IOC_SET_CSC_TYPE,               vpq_ioctl_set_csc_type},
	{VPQ_IOC_GET_CSC_TYPE,               vpq_ioctl_get_csc_type},
	{VPQ_IOC_SET_HDR_TMO_MODE,           vpq_ioctl_set_hdr_tmo_mode},
	{VPQ_IOC_SET_HDR_TMO_DATA,           vpq_ioctl_set_hdr_tmo_data},
	{VPQ_IOC_SET_AIPQ_MODE,              vpq_ioctl_set_aipq_mode},
	{VPQ_IOC_SET_AISR_MODE,              vpq_ioctl_set_aisr_mode},
	{VPQ_IOC_GET_COLOR_PRIM,             vpq_ioctl_get_color_primary},
	{VPQ_IOC_GET_HDR_METADATA,           vpq_ioctl_get_hdr_metadata},
	{VPQ_IOC_SET_BLUE_STRETCH,           vpq_ioctl_set_blue_stretch},
	{VPQ_IOC_SET_CHROMA_CORING,          vpq_ioctl_set_chroma_coring},
	{VPQ_IOC_SET_XVYCC_MODE,             vpq_ioctl_set_xvycc_mode},
	{VPQ_IOC_SET_MCDI_MODE,              vpq_ioctl_set_mcdi_mode},
	{VPQ_IOC_SET_COLOR_BASE,             vpq_ioctl_set_color_base},
	{VPQ_IOC_SET_CABC,                   vpq_ioctl_set_cabc},
	{VPQ_IOC_SET_ADD,                    vpq_ioctl_set_add},
	{VPQ_IOC_SET_PLL_VALUE,              vpq_ioctl_set_pll_value},
	{VPQ_IOC_SET_PC_MODE,                vpq_ioctl_set_pc_mode},
	{VPQ_IOC_GET_EVENT_INFO,             vpq_ioctl_get_event_info},
	{VPQ_IOC_GET_SIGNAL_INFO,            vpq_ioctl_get_signal_info},
	{VPQ_IOC_SET_FRAME_STATUS,           vpq_ioctl_set_frame_status},
	{VPQ_IOC_SET_FRAME,                  vpq_ioctl_set_frame},
};

int vpq_ioctl_process(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = -1;
	int index = 0;
	int maxindex = 0;

	maxindex = sizeof(st_ioctlinfo) / sizeof(struct vpq_ioctl_func_s);
	for (index = 0; index < maxindex; index++) {
		if (cmd == st_ioctlinfo[index].cmd) {
			VPQ_PR_INFO(PR_IOCTL, "%s cmd:0x%x\n", __func__, _IOC_NR(cmd));
			ret = st_ioctlinfo[index].pioctlfunc(file, arg);
			break;
		}
	}

	return ret;
}

