// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include "vpp_common.h"
#include "vpp_drv.h"
#include "vpp_pq_mgr.h"
#include "vpp_debug.h"

unsigned int pr_lvl;
struct vpp_dev_s *vpp_dev;

struct vpp_dev_s *get_vpp_dev(void)
{
	return vpp_dev;
}

const struct class_attribute vpp_class_attr[] = {
	__ATTR(vpp_debug, 0644,
		vpp_debug_show,
		vpp_debug_store),
	__ATTR(vpq_reg_rw, 0644,
		vpp_debug_reg_show,
		vpp_debug_reg_store),
	__ATTR(vpq_cm_reg_rw, 0644,
		vpp_debug_cm_reg_show,
		vpp_debug_cm_reg_store),
	__ATTR(vpp_brightness, 0644,
		vpp_debug_brightness_show,
		vpp_debug_brightness_store),
	__ATTR(vpp_brightness_post, 0644,
		vpp_debug_brightness_post_show,
		vpp_debug_brightness_post_store),
	__ATTR(vpp_contrast, 0644,
		vpp_debug_contrast_show,
		vpp_debug_contrast_store),
	__ATTR(vpp_contrast_post, 0644,
		vpp_debug_contrast_post_show,
		vpp_debug_contrast_post_store),
	__ATTR(vpp_sat, 0644,
		vpp_debug_saturation_show,
		vpp_debug_saturation_store),
	__ATTR(vpp_sat_post, 0644,
		vpp_debug_saturation_post_show,
		vpp_debug_saturation_post_store),
	__ATTR(vpp_hue, 0644,
		vpp_debug_hue_show,
		vpp_debug_hue_store),
	__ATTR(vpp_hue_post, 0644,
		vpp_debug_hue_post_show,
		vpp_debug_hue_post_store),
	__ATTR(vpp_sharpness, 0644,
		vpp_debug_sharpness_show,
		vpp_debug_sharpness_store),
	__ATTR(vpp_pre_gamma, 0644,
		vpp_debug_pre_gamma_show,
		vpp_debug_pre_gamma_store),
	__ATTR(vpp_gamma, 0644,
		vpp_debug_gamma_show,
		vpp_debug_gamma_store),
	__ATTR(vpp_pre_gamma_pattern, 0644,
		vpp_debug_pre_gamma_pattern_show,
		vpp_debug_pre_gamma_pattern_store),
	__ATTR(vpp_gamma_pattern, 0644,
		vpp_debug_gamma_pattern_show,
		vpp_debug_gamma_pattern_store),
	__ATTR(vpp_white_balance, 0644,
		vpp_debug_whitebalance_show,
		vpp_debug_whitebalance_store),
	__ATTR(vpp_module_ctrl, 0644,
		vpp_debug_module_ctrl_show,
		vpp_debug_module_ctrl_store),
	__ATTR(vpp_dnlp, 0644,
		vpp_debug_dnlp_show,
		vpp_debug_dnlp_store),
	__ATTR(vpp_lc, 0644,
		vpp_debug_lc_show,
		vpp_debug_lc_store),
	__ATTR(vpp_hdr_type, 0644,
		vpp_debug_hdr_type_show,
		vpp_debug_hdr_type_store),
	__ATTR(vpp_pc_mode, 0644,
		vpp_debug_pc_mode_show,
		vpp_debug_pc_mode_store),
	__ATTR(vpp_csc_type, 0644,
		vpp_debug_csc_type_show,
		vpp_debug_csc_type_store),
	__ATTR(vpp_color_primary, 0644,
		vpp_debug_color_primary_show,
		vpp_debug_color_primary_store),
	__ATTR(vpp_histogram, 0644,
		vpp_debug_histogram_show,
		vpp_debug_histogram_store),
	__ATTR(vpp_histogram_ave, 0644,
		vpp_debug_histogram_ave_show,
		vpp_debug_histogram_ave_store),
	__ATTR(vpp_histogram_hdr, 0644,
		vpp_debug_histogram_hdr_show,
		vpp_debug_histogram_hdr_store),
	__ATTR(vpp_hdr_metadata, 0644,
		vpp_debug_hdr_metadata_show,
		vpp_debug_hdr_metadata_store),
	__ATTR(vpp_matrix, 0644,
		vpp_debug_matrix_show,
		vpp_debug_matrix_store),
	__ATTR(vpp_eye_protect, 0644,
		vpp_debug_eye_protect_show,
		vpp_debug_eye_protect_store),
	__ATTR(vpp_color_curve, 0644,
		vpp_debug_color_curve_show,
		vpp_debug_color_curve_store),
	__ATTR(vpp_overscan, 0644,
		vpp_debug_overscan_show,
		vpp_debug_overscan_store),
	__ATTR_NULL,
};

const struct match_data_s vpp_t3_match = {
	.chip_id = CHIP_T3,
};

const struct match_data_s vpp_match = {
	.chip_id = CHIP_MAX,
};

const struct of_device_id vpp_dts_match[] = {
	{
		.compatible = "amlogic, vpp-t3",
		.data = &vpp_t3_match,
	},
	{
		.compatible = "amlogic, vpp",
		.data = &vpp_match,
	},
	{},
};

static int vpp_open(struct inode *inode, struct file *filp)
{
	struct vpp_dev_s *vpp_devp;

	vpp_devp = container_of(inode->i_cdev, struct vpp_dev_s, vpp_cdev);
	filp->private_data = vpp_devp;
	return 0;
}

static int vpp_release(struct inode *inode, struct file *filp)
{
	filp->private_data = NULL;
	return 0;
}

static long vpp_ioctl(struct file *filp,
	unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	int val = 0;
	void __user *argp;
	char *ptmp_data_char = NULL;
	int *ptmp_data = NULL;
	unsigned int buffer_size;
	unsigned int data_len;
	struct vpp_pq_state_s pq_state;
	struct vpp_white_balance_s wb_data;
	struct vpp_gamma_table_s gamma_data;
	struct vpp_gamma_table_s gamma_data_tmp;
	struct vpp_mtrx_info_s *pmtrx_data  = NULL;
	struct vpp_module_ctrl_s module_status;
	struct vpp_dnlp_curve_param_s *pdnlp_data = NULL;
	struct vpp_lc_curve_s *plc_curve_data = NULL;
	struct vpp_lc_param_s *plc_param_data = NULL;
	struct vpp_lut3d_table_s lut3d_data;
	struct vpp_hdr_lut_s hdr_lut_data;
	struct vpp_cm_curve_s cm_data;
	struct vpp_eye_protect_s eye_protect_data;
	struct vpp_aipq_table_s aipq_load_table;
	struct vpp_hdr_metadata_s hdr_metadata;
	struct vpp_histgm_ave_s hist_ave;
	struct vpp_table_s overscan_table;
	struct vpp_histgm_param_s *phist_data = NULL;
	struct vpp_hdr_histgm_param_s *phist_data_hdr = NULL;
	struct vpp_color_primary_s *pcolor_pri_data = NULL;
	struct vpp_overscan_table_s *overscan_data_ptr = NULL;

	memset(&pq_state, 0, sizeof(struct vpp_pq_state_s));
	memset(&wb_data, 0, sizeof(struct vpp_white_balance_s));
	memset(&module_status, 0, sizeof(struct vpp_module_ctrl_s));
	memset(&lut3d_data, 0, sizeof(struct vpp_lut3d_table_s));
	memset(&hdr_lut_data, 0, sizeof(struct vpp_hdr_lut_s));
	memset(&cm_data, 0, sizeof(struct vpp_cm_curve_s));
	memset(&eye_protect_data, 0, sizeof(struct vpp_eye_protect_s));
	memset(&aipq_load_table, 0, sizeof(struct vpp_aipq_table_s));
	memset(&hdr_metadata, 0, sizeof(struct vpp_hdr_metadata_s));
	memset(&hist_ave, 0, sizeof(struct vpp_histgm_ave_s));

	pr_vpp(PR_IOC, "%s cmd = %d\n", __func__, cmd);

	argp = (void __user *)arg;

	switch (cmd) {
	case VPP_IOC_SET_BRIGHTNESS:
		if (copy_from_user(&val, argp, sizeof(int))) {
			pr_vpp(PR_IOC, "VPP_IOC_SET_BRIGHTNESS failed\n");
			ret = -EFAULT;
		} else {
			pr_vpp(PR_IOC, "VPP_IOC_SET_BRIGHTNESS success\n");
			ret = vpp_pq_mgr_set_brightness(val);
		}
		break;
	case VPP_IOC_SET_CONTRAST:
		if (copy_from_user(&val, argp, sizeof(int))) {
			pr_vpp(PR_IOC, "VPP_IOC_SET_CONTRAST failed\n");
			ret = -EFAULT;
		} else {
			pr_vpp(PR_IOC, "VPP_IOC_SET_CONTRAST success\n");
			ret = vpp_pq_mgr_set_contrast(val);
		}
		break;
	case VPP_IOC_SET_SATURATION:
		if (copy_from_user(&val, argp, sizeof(int))) {
			pr_vpp(PR_IOC, "VPP_IOC_SET_SATURATION failed\n");
			ret = -EFAULT;
		} else {
			pr_vpp(PR_IOC, "VPP_IOC_SET_SATURATION success\n");
			ret = vpp_pq_mgr_set_saturation(val);
		}
		break;
	case VPP_IOC_SET_HUE:
		if (copy_from_user(&val, argp, sizeof(int))) {
			pr_vpp(PR_IOC, "VPP_IOC_SET_HUE failed\n");
			ret = -EFAULT;
		} else {
			pr_vpp(PR_IOC, "VPP_IOC_SET_HUE success\n");
			ret = vpp_pq_mgr_set_hue(val);
		}
		break;
	case VPP_IOC_SET_SHARPNESS:
		if (copy_from_user(&val, argp, sizeof(int))) {
			pr_vpp(PR_IOC, "VPP_IOC_SET_SHARPNESS failed\n");
			ret = -EFAULT;
		} else {
			pr_vpp(PR_IOC, "VPP_IOC_SET_SHARPNESS success\n");
			ret = vpp_pq_mgr_set_sharpness(val);
		}
		break;
	case VPP_IOC_SET_BRIGHTNESS_POST:
		if (copy_from_user(&val, argp, sizeof(int))) {
			pr_vpp(PR_IOC, "VPP_IOC_SET_BRIGHTNESS_POST failed\n");
			ret = -EFAULT;
		} else {
			pr_vpp(PR_IOC, "VPP_IOC_SET_BRIGHTNESS_POST success\n");
			ret = vpp_pq_mgr_set_brightness_post(val);
		}
		break;
	case VPP_IOC_SET_CONTRAST_POST:
		if (copy_from_user(&val, argp, sizeof(int))) {
			pr_vpp(PR_IOC, "VPP_IOC_SET_CONTRAST_POST failed\n");
			ret = -EFAULT;
		} else {
			pr_vpp(PR_IOC, "VPP_IOC_SET_CONTRAST_POST success\n");
			ret = vpp_pq_mgr_set_contrast_post(val);
		}
		break;
	case VPP_IOC_SET_SATURATION_POST:
		if (copy_from_user(&val, argp, sizeof(int))) {
			pr_vpp(PR_IOC, "VPP_IOC_SET_SATURATION_POST failed\n");
			ret = -EFAULT;
		} else {
			pr_vpp(PR_IOC, "VPP_IOC_SET_SATURATION_POST success\n");
			ret = vpp_pq_mgr_set_saturation_post(val);
		}
		break;
	case VPP_IOC_SET_HUE_POST:
		if (copy_from_user(&val, argp, sizeof(int))) {
			pr_vpp(PR_IOC, "VPP_IOC_SET_HUE_POST failed\n");
			ret = -EFAULT;
		} else {
			pr_vpp(PR_IOC, "VPP_IOC_SET_HUE_POST success\n");
			ret = vpp_pq_mgr_set_hue_post(val);
		}
		break;
	case VPP_IOC_SET_WB:
		if (copy_from_user(&wb_data, argp,
			sizeof(struct vpp_white_balance_s))) {
			pr_vpp(PR_IOC, "VPP_IOC_SET_WB failed\n");
			ret = -EFAULT;
		} else {
			pr_vpp(PR_IOC, "VPP_IOC_SET_WB success\n");
			ret = vpp_pq_mgr_set_whitebalance(&wb_data);
		}
		break;
	case VPP_IOC_SET_PRE_GAMMA_DATA:
		data_len = vpp_pq_mgr_get_pre_gamma_table_len();
		if (data_len != 0) {
			buffer_size = sizeof(struct vpp_gamma_table_s);
			if (copy_from_user(&gamma_data_tmp, argp, buffer_size)) {
				pr_vpp(PR_IOC, "VPP_IOC_SET_PRE_GAMMA_DATA failed\n");
				ret = -EFAULT;
			} else {
				buffer_size = data_len * sizeof(unsigned int);
				gamma_data.r_data = kmalloc(buffer_size, GFP_KERNEL);
				if (copy_from_user(gamma_data.r_data, argp, buffer_size)) {
					pr_vpp(PR_IOC, "VPP_IOC_SET_PRE_GAMMA_DATA r failed\n");
					ret = -EFAULT;
				}

				gamma_data.g_data = kmalloc(buffer_size, GFP_KERNEL);
				if (copy_from_user(gamma_data.g_data, argp, buffer_size)) {
					pr_vpp(PR_IOC, "VPP_IOC_SET_PRE_GAMMA_DATA g failed\n");
					ret = -EFAULT;
				}

				gamma_data.b_data = kmalloc(buffer_size, GFP_KERNEL);
				if (copy_from_user(gamma_data.b_data, argp, buffer_size)) {
					pr_vpp(PR_IOC, "VPP_IOC_SET_PRE_GAMMA_DATA b failed\n");
					ret = -EFAULT;
				}

				if (ret != -EFAULT) {
					pr_vpp(PR_IOC, "VPP_IOC_SET_PRE_GAMMA_DATA success\n");
					ret = vpp_pq_mgr_set_pre_gamma_table(&gamma_data);
				}

				kfree(gamma_data.r_data);
				kfree(gamma_data.g_data);
				kfree(gamma_data.b_data);
			}
		}
		break;
	case VPP_IOC_SET_GAMMA_DATA:
		data_len = vpp_pq_mgr_get_gamma_table_len();
		if (data_len != 0) {
			buffer_size = sizeof(struct vpp_gamma_table_s);
			if (copy_from_user(&gamma_data_tmp, argp, buffer_size)) {
				pr_vpp(PR_IOC, "VPP_IOC_SET_GAMMA_DATA failed\n");
				ret = -EFAULT;
			} else {
				buffer_size = data_len * sizeof(unsigned int);
				gamma_data.r_data = kmalloc(buffer_size, GFP_KERNEL);
				if (copy_from_user(gamma_data.r_data, argp, buffer_size)) {
					pr_vpp(PR_IOC, "VPP_IOC_SET_GAMMA_DATA r failed\n");
					ret = -EFAULT;
				}

				gamma_data.g_data = kmalloc(buffer_size, GFP_KERNEL);
				if (copy_from_user(gamma_data.g_data, argp, buffer_size)) {
					pr_vpp(PR_IOC, "VPP_IOC_SET_GAMMA_DATA g failed\n");
					ret = -EFAULT;
				}

				gamma_data.b_data = kmalloc(buffer_size, GFP_KERNEL);
				if (copy_from_user(gamma_data.b_data, argp, buffer_size)) {
					pr_vpp(PR_IOC, "VPP_IOC_SET_GAMMA_DATA b failed\n");
					ret = -EFAULT;
				}

				if (ret != -EFAULT) {
					pr_vpp(PR_IOC, "VPP_IOC_SET_GAMMA_DATA success\n");
					ret = vpp_pq_mgr_set_gamma_table(&gamma_data);
				}

				kfree(gamma_data.r_data);
				kfree(gamma_data.g_data);
				kfree(gamma_data.b_data);
			}
		}
		break;
	case VPP_IOC_SET_MATRIX_PARAM:
		buffer_size = sizeof(struct vpp_mtrx_info_s);
		pmtrx_data = kmalloc(buffer_size, GFP_KERNEL);
		if (!pmtrx_data) {
			pr_vpp(PR_IOC, "VPP_IOC_SET_MATRIX_PARAM malloc failed\n");
			ret = -ENOMEM;
		} else {
			if (copy_from_user(pmtrx_data, argp, buffer_size)) {
				pr_vpp(PR_IOC, "VPP_IOC_SET_MATRIX_PARAM failed\n");
				ret = -EFAULT;
			} else {
				pr_vpp(PR_IOC, "VPP_IOC_SET_MATRIX_PARAM success\n");
				ret = vpp_pq_mgr_set_matrix_param(pmtrx_data);
			}
			kfree(pmtrx_data);
		}
		break;
	case VPP_IOC_SET_MODULE_STATUS:
		if (copy_from_user(&module_status, argp,
			sizeof(struct vpp_module_ctrl_s))) {
			pr_vpp(PR_IOC, "VPP_IOC_SET_MODULE_STATUS failed\n");
			ret = -EFAULT;
		} else {
			pr_vpp(PR_IOC, "VPP_IOC_SET_MODULE_STATUS success\n");
			ret = vpp_pq_mgr_set_module_status(module_status.module_type,
				module_status.status);
		}
		break;
	case VPP_IOC_SET_PQ_STATE:
		if (copy_from_user(&pq_state, argp,
			sizeof(struct vpp_pq_state_s))) {
			pr_vpp(PR_IOC, "VPP_IOC_SET_PQ_STATE failed\n");
			ret = -EFAULT;
		} else {
			pr_vpp(PR_IOC, "VPP_IOC_SET_PQ_STATE success\n");
			ret = vpp_pq_mgr_set_status(&pq_state);
		}
		break;
	case VPP_IOC_SET_PC_MODE:
		if (copy_from_user(&val, argp, sizeof(enum vpp_pc_mode_e))) {
			pr_vpp(PR_IOC, "VPP_IOC_SET_PC_MODE failed\n");
			ret = -EFAULT;
		} else {
			pr_vpp(PR_IOC, "VPP_IOC_SET_PC_MODE success\n");
			ret = vpp_pq_mgr_set_pc_mode(val);
		}
		break;
	case VPP_IOC_SET_DNLP_PARAM:
		buffer_size = sizeof(struct vpp_dnlp_curve_param_s);
		pdnlp_data = kmalloc(buffer_size, GFP_KERNEL);
		if (!pdnlp_data) {
			pr_vpp(PR_IOC, "VPP_IOC_SET_DNLP_PARAM malloc failed\n");
			ret = -ENOMEM;
		} else {
			if (copy_from_user(pdnlp_data, argp, buffer_size)) {
				pr_vpp(PR_IOC, "VPP_IOC_SET_DNLP_PARAM failed\n");
				ret = -EFAULT;
			} else {
				pr_vpp(PR_IOC, "VPP_IOC_SET_DNLP_PARAM success\n");
				ret = vpp_pq_mgr_set_dnlp_param(pdnlp_data);
			}
			kfree(pdnlp_data);
		}
		break;
	case VPP_IOC_SET_LC_CURVE:
		buffer_size = sizeof(struct vpp_lc_curve_s);
		plc_curve_data = kmalloc(buffer_size, GFP_KERNEL);
		if (!plc_curve_data) {
			pr_vpp(PR_IOC, "VPP_IOC_SET_LC_CURVE malloc failed\n");
			ret = -ENOMEM;
		} else {
			if (copy_from_user(plc_curve_data, argp, buffer_size)) {
				pr_vpp(PR_IOC, "VPP_IOC_SET_LC_CURVE failed\n");
				ret = -EFAULT;
			} else {
				pr_vpp(PR_IOC, "VPP_IOC_SET_LC_CURVE success\n");
				ret = vpp_pq_mgr_set_lc_curve(plc_curve_data);
			}
			kfree(plc_curve_data);
		}
		break;
	case VPP_IOC_SET_LC_PARAM:
		buffer_size = sizeof(struct vpp_lc_param_s);
		plc_param_data = kmalloc(buffer_size, GFP_KERNEL);
		if (!plc_param_data) {
			pr_vpp(PR_IOC, "VPP_IOC_SET_LC_PARAM malloc failed\n");
			ret = -ENOMEM;
		} else {
			if (copy_from_user(plc_param_data, argp, buffer_size)) {
				pr_vpp(PR_IOC, "VPP_IOC_SET_LC_PARAM failed\n");
				ret = -EFAULT;
			} else {
				pr_vpp(PR_IOC, "VPP_IOC_SET_LC_PARAM success\n");
				ret = vpp_pq_mgr_set_lc_param(plc_param_data);
			}
			kfree(plc_param_data);
		}
		break;
	case VPP_IOC_SET_COLOR_PRIMARY_STATUS:
		if (copy_from_user(&val, argp, sizeof(int))) {
			pr_vpp(PR_IOC, "VPP_IOC_SET_COLOR_PRIMARY_STATUS failed\n");
			ret = -EFAULT;
		} else {
			pr_vpp(PR_IOC, "VPP_IOC_SET_COLOR_PRIMARY_STATUS success\n");
			ret = vpp_pq_mgr_set_color_primary_status(val);
		}
		break;
	case VPP_IOC_SET_COLOR_PRIMARY:
		buffer_size = sizeof(struct vpp_color_primary_s);
		pcolor_pri_data = kmalloc(buffer_size, GFP_KERNEL);
		if (!pcolor_pri_data) {
			pr_vpp(PR_IOC, "VPP_IOC_SET_COLOR_PRIMARY malloc failed\n");
			ret = -ENOMEM;
		} else {
			if (copy_from_user(pcolor_pri_data, argp, buffer_size)) {
				pr_vpp(PR_IOC, "VPP_IOC_SET_COLOR_PRIMARY failed\n");
				ret = -EFAULT;
			} else {
				pr_vpp(PR_IOC, "VPP_IOC_SET_COLOR_PRIMARY success\n");
				ret = vpp_pq_mgr_set_color_primary(pcolor_pri_data);
			}
			kfree(pcolor_pri_data);
		}
		break;
	case VPP_IOC_SET_CSC_TYPE:
		if (copy_from_user(&val, argp, sizeof(enum vpp_csc_type_e))) {
			pr_vpp(PR_IOC, "VPP_IOC_SET_CSC_TYPE failed\n");
			ret = -EFAULT;
		} else {
			pr_vpp(PR_IOC, "VPP_IOC_SET_CSC_TYPE success\n");
			ret = vpp_pq_mgr_set_csc_type(val);
		}
		break;
	case VPP_IOC_SET_3DLUT_DATA:
		if (copy_from_user(&lut3d_data, argp,
			sizeof(struct vpp_lut3d_table_s))) {
			pr_vpp(PR_IOC, "VPP_IOC_SET_3DLUT_DATA failed\n");
			ret = -EFAULT;
		} else {
			if (lut3d_data.data_size == 0) {
				ret = -EFAULT;
				break;
			}

			buffer_size = 17 * 17 * 17 * 3 * sizeof(int);
			ptmp_data = kmalloc(buffer_size, GFP_KERNEL);
			if (!ptmp_data) {
				pr_vpp(PR_IOC, "VPP_IOC_SET_3DLUT_DATA malloc failed\n");
				ret = -ENOMEM;
			} else {
				if (copy_from_user(ptmp_data, argp, buffer_size)) {
					pr_vpp(PR_IOC, "VPP_IOC_SET_3DLUT_DATA failed\n");
					ret = -EFAULT;
				} else {
					pr_vpp(PR_IOC, "VPP_IOC_SET_3DLUT_DATA success\n");
					lut3d_data.pdata = ptmp_data;
					ret = vpp_pq_mgr_set_3dlut_data(&lut3d_data);
				}
				kfree(ptmp_data);
			}
		}
		break;
	case VPP_IOC_SET_HDR_TMO:
		if (copy_from_user(&hdr_lut_data, argp,
			sizeof(struct vpp_hdr_lut_s))) {
			pr_vpp(PR_IOC, "VPP_IOC_SET_HDR_TMO failed\n");
			ret = -EFAULT;
		} else {
			if (hdr_lut_data.lut_size == 0) {
				ret = -EFAULT;
				break;
			}

			buffer_size = 149 * sizeof(int);
			ptmp_data = kmalloc(buffer_size, GFP_KERNEL);
			if (!ptmp_data) {
				pr_vpp(PR_IOC, "VPP_IOC_SET_HDR_TMO malloc failed\n");
				ret = -ENOMEM;
			} else {
				if (copy_from_user(ptmp_data, argp, buffer_size)) {
					pr_vpp(PR_IOC, "VPP_IOC_SET_HDR_TMO failed\n");
					ret = -EFAULT;
				} else {
					pr_vpp(PR_IOC, "VPP_IOC_SET_HDR_TMO success\n");
					hdr_lut_data.lut_data = ptmp_data;
					ret = vpp_pq_mgr_set_hdr_tmo_curve(&hdr_lut_data);
				}
				kfree(ptmp_data);
			}
		}
		break;
	case VPP_IOC_SET_HDR_OETF:
		if (copy_from_user(&hdr_lut_data, argp,
			sizeof(struct vpp_hdr_lut_s))) {
			pr_vpp(PR_IOC, "VPP_IOC_SET_HDR_OETF failed\n");
			ret = -EFAULT;
		} else {
			if (hdr_lut_data.lut_size == 0) {
				ret = -EFAULT;
				break;
			}

			buffer_size = 149 * sizeof(int);
			ptmp_data = kmalloc(buffer_size, GFP_KERNEL);
			if (!ptmp_data) {
				pr_vpp(PR_IOC, "VPP_IOC_SET_HDR_OETF malloc failed\n");
				ret = -ENOMEM;
			} else {
				if (copy_from_user(ptmp_data, argp, buffer_size)) {
					pr_vpp(PR_IOC, "VPP_IOC_SET_HDR_OETF failed\n");
					ret = -EFAULT;
				} else {
					pr_vpp(PR_IOC, "VPP_IOC_SET_HDR_OETF success\n");
					hdr_lut_data.lut_data = ptmp_data;
					ret = vpp_pq_mgr_set_hdr_oetf_curve(&hdr_lut_data);
				}
				kfree(ptmp_data);
			}
		}
		break;
	case VPP_IOC_SET_HDR_EOTF:
		if (copy_from_user(&hdr_lut_data, argp,
			sizeof(struct vpp_hdr_lut_s))) {
			pr_vpp(PR_IOC, "VPP_IOC_SET_HDR_EOTF failed\n");
			ret = -EFAULT;
		} else {
			if (hdr_lut_data.lut_size == 0) {
				ret = -EFAULT;
				break;
			}

			buffer_size = 143 * sizeof(int);
			ptmp_data = kmalloc(buffer_size, GFP_KERNEL);
			if (!ptmp_data) {
				pr_vpp(PR_IOC, "VPP_IOC_SET_HDR_EOTF malloc failed\n");
				ret = -ENOMEM;
			} else {
				if (copy_from_user(ptmp_data, argp, buffer_size)) {
					pr_vpp(PR_IOC, "VPP_IOC_SET_HDR_EOTF failed\n");
					ret = -EFAULT;
				} else {
					pr_vpp(PR_IOC, "VPP_IOC_SET_HDR_EOTF success\n");
					hdr_lut_data.lut_data = ptmp_data;
					ret = vpp_pq_mgr_set_hdr_eotf_curve(&hdr_lut_data);
				}
				kfree(ptmp_data);
			}
		}
		break;
	case VPP_IOC_SET_HDR_CGAIN:
		if (copy_from_user(&hdr_lut_data, argp,
			sizeof(struct vpp_hdr_lut_s))) {
			pr_vpp(PR_IOC, "VPP_IOC_SET_HDR_CGAIN failed\n");
			ret = -EFAULT;
		} else {
			if (hdr_lut_data.lut_size == 0) {
				ret = -EFAULT;
				break;
			}

			buffer_size = 65 * sizeof(int);
			ptmp_data = kmalloc(buffer_size, GFP_KERNEL);
			if (!ptmp_data) {
				pr_vpp(PR_IOC, "VPP_IOC_SET_HDR_CGAIN malloc failed\n");
				ret = -ENOMEM;
			} else {
				if (copy_from_user(ptmp_data, argp, buffer_size)) {
					pr_vpp(PR_IOC, "VPP_IOC_SET_HDR_CGAIN failed\n");
					ret = -EFAULT;
				} else {
					pr_vpp(PR_IOC, "VPP_IOC_SET_HDR_CGAIN success\n");
					hdr_lut_data.lut_data = ptmp_data;
					ret = vpp_pq_mgr_set_hdr_cgain_curve(&hdr_lut_data);
				}
				kfree(ptmp_data);
			}
		}
		break;
	case VPP_IOC_SET_CM_CURVE:
		if (copy_from_user(&cm_data, argp,
			sizeof(struct vpp_cm_curve_s))) {
			pr_vpp(PR_IOC, "VPP_IOC_SET_CM_CURVE failed\n");
			ret = -EFAULT;
		} else {
			if (cm_data.data_size == 0) {
				ret = -EFAULT;
				break;
			}

			buffer_size = 32 * 5 * sizeof(int);
			ptmp_data = kmalloc(buffer_size, GFP_KERNEL);
			if (!ptmp_data) {
				pr_vpp(PR_IOC, "VPP_IOC_SET_CM_CURVE malloc failed\n");
				ret = -ENOMEM;
			} else {
				if (copy_from_user(ptmp_data, argp, buffer_size)) {
					pr_vpp(PR_IOC, "VPP_IOC_SET_CM_CURVE failed\n");
					ret = -EFAULT;
				} else {
					pr_vpp(PR_IOC, "VPP_IOC_SET_CM_CURVE success\n");
					ret = vpp_pq_mgr_set_cm_curve(cm_data.curve_type,
						ptmp_data);
				}
				kfree(ptmp_data);
			}
		}
		break;
	case VPP_IOC_SET_CM_OFFSET_CURVE:
		if (copy_from_user(&cm_data, argp,
			sizeof(struct vpp_cm_curve_s))) {
			pr_vpp(PR_IOC, "VPP_IOC_SET_CM_OFFSET_CURVE failed\n");
			ret = -EFAULT;
		} else {
			if (cm_data.data_size == 0) {
				ret = -EFAULT;
				break;
			}

			buffer_size = 32 * 5 * sizeof(char);
			ptmp_data_char = kmalloc(buffer_size, GFP_KERNEL);
			if (!ptmp_data_char) {
				pr_vpp(PR_IOC, "VPP_IOC_SET_CM_OFFSET_CURVE malloc failed\n");
				ret = -ENOMEM;
			} else {
				if (copy_from_user(ptmp_data_char, argp, buffer_size)) {
					pr_vpp(PR_IOC, "VPP_IOC_SET_CM_OFFSET_CURVE failed\n");
					ret = -EFAULT;
				} else {
					pr_vpp(PR_IOC, "VPP_IOC_SET_CM_OFFSET_CURVE success\n");
					ret = vpp_pq_mgr_set_cm_offset_curve(cm_data.curve_type,
						ptmp_data_char);
				}
				kfree(ptmp_data_char);
			}
		}
		break;
	case VPP_IOC_SET_EYE_PROTECT:
		if (copy_from_user(&eye_protect_data, argp,
			sizeof(struct vpp_eye_protect_s))) {
			pr_vpp(PR_IOC, "VPP_IOC_SET_EYE_PROTECT failed\n");
			ret = -EFAULT;
		} else {
			pr_vpp(PR_IOC, "VPP_IOC_SET_EYE_PROTECT success\n");
			ret = vpp_pq_mgr_set_eye_protect(&eye_protect_data);
		}
		break;
	case VPP_IOC_SET_AIPQ_TABLE:
		if (copy_from_user(&aipq_load_table, argp,
			sizeof(struct vpp_aipq_table_s))) {
			pr_vpp(PR_IOC, "VPP_IOC_SET_AIPQ_TABLE failed\n");
			ret = -EFAULT;
			break;
		}

		if (aipq_load_table.height > 25)
			aipq_load_table.height = 25;

		if (aipq_load_table.width > 10)
			aipq_load_table.width = 10;

		data_len = 3 * aipq_load_table.height * aipq_load_table.width;
		buffer_size = (data_len + 1) * sizeof(char);

		ptmp_data_char = kmalloc(buffer_size, GFP_KERNEL);
		if (!ptmp_data_char) {
			pr_vpp(PR_IOC, "VPP_IOC_SET_AIPQ_TABLE kmalloc fail!!!\n");
			ret = -EFAULT;
			break;
		}

		argp = (void __user *)aipq_load_table.table_ptr;
		if (copy_from_user(ptmp_data_char, argp, buffer_size)) {
			pr_vpp(PR_IOC, "VPP_IOC_SET_AIPQ_TABLE copy from user fail!!!\n");
			kfree(ptmp_data_char);
			ret = -EFAULT;
			break;
		}

		ptmp_data_char[data_len] = '\0';
		if (strlen(ptmp_data_char) != data_len) {
			pr_vpp(PR_IOC, "VPP_IOC_SET_AIPQ_TABLE data_length != 3*height*width!!!\n");
			kfree(ptmp_data_char);
			break;
		}

		ret = vpp_pq_mgr_set_aipq_offset_table(ptmp_data_char,
			aipq_load_table.height, aipq_load_table.width);
		kfree(ptmp_data_char);
		break;
	case VPP_IOC_SET_OVERSCAN_TABLE:
		if (copy_from_user(&overscan_table, argp,
			sizeof(struct vpp_table_s))) {
			pr_vpp(PR_IOC, "VPP_IOC_SET_OVERSCAN_TABLE failed\n");
			ret = -EFAULT;
			break;
		}

		if (overscan_table.length > EN_TIMING_MAX ||
			overscan_table.length <= 0) {
			pr_vpp(PR_IOC, "VPP_IOC_SET_OVERSCAN_TABLE length check failed\n");
			ret = -EFAULT;
			break;
		}

		buffer_size = overscan_table.length *
			sizeof(struct vpp_overscan_table_s);
		overscan_data_ptr = kmalloc(buffer_size, GFP_KERNEL);
		if (!overscan_data_ptr) {
			pr_vpp(PR_IOC, "VPP_IOC_SET_OVERSCAN_TABLE kmalloc failed\n");
			ret = -EFAULT;
			break;
		}

		argp = (void __user *)overscan_table.param_ptr;
		if (copy_from_user(overscan_data_ptr, argp, buffer_size)) {
			pr_vpp(PR_IOC, "VPP_IOC_SET_OVERSCAN_TABLE copy from user fail!!!\n");
			kfree(overscan_data_ptr);
			ret = -EFAULT;
			break;
		}

		ret = vpp_pq_mgr_set_overscan_table(overscan_table.length,
			overscan_data_ptr);
		kfree(overscan_data_ptr);
		break;
	case VPP_IOC_GET_PC_MODE:
		val = vpp_pq_mgr_get_pc_mode();
		if (copy_to_user(argp, &val,
			sizeof(enum vpp_pc_mode_e))) {
			pr_vpp(PR_IOC, "VPP_IOC_GET_PC_MODE failed\n");
			ret = -EFAULT;
		} else {
			pr_vpp(PR_IOC, "VPP_IOC_GET_PC_MODE success\n");
		}
		break;
	case VPP_IOC_GET_CSC_TYPE:
		val = vpp_pq_mgr_get_csc_type();
		if (copy_to_user(argp, &val,
			sizeof(enum vpp_csc_type_e))) {
			pr_vpp(PR_IOC, "VPP_IOC_GET_CSC_TYPE failed\n");
			ret = -EFAULT;
		} else {
			pr_vpp(PR_IOC, "VPP_IOC_GET_CSC_TYPE success\n");
		}
		break;
	case VPP_IOC_GET_HDR_TYPE:
		val = vpp_pq_mgr_get_hdr_type();
		if (copy_to_user(argp, &val,
			sizeof(enum vpp_hdr_type_e))) {
			pr_vpp(PR_IOC, "VPP_IOC_GET_HDR_TYPE failed\n");
			ret = -EFAULT;
		} else {
			pr_vpp(PR_IOC, "VPP_IOC_GET_HDR_TYPE success\n");
		}
		break;
	case VPP_IOC_GET_COLOR_PRIM:
		val = vpp_pq_mgr_get_color_primary();
		if (copy_to_user(argp, &val,
			sizeof(enum vpp_color_primary_e))) {
			pr_vpp(PR_IOC, "VPP_IOC_GET_COLOR_PRIM failed\n");
			ret = -EFAULT;
		} else {
			pr_vpp(PR_IOC, "VPP_IOC_GET_COLOR_PRIM success\n");
		}
		break;
	case VPP_IOC_GET_HDR_METADATA:
		vpp_pq_mgr_get_hdr_metadata(&hdr_metadata);
		if (copy_to_user(argp, &hdr_metadata,
			sizeof(struct vpp_hdr_metadata_s))) {
			pr_vpp(PR_IOC, "VPP_IOC_GET_HDR_METADATA failed\n");
			ret = -EFAULT;
		} else {
			pr_vpp(PR_IOC, "VPP_IOC_GET_HDR_METADATA success\n");
		}
		break;
	case VPP_IOC_GET_HIST_AVG:
		vpp_pq_mgr_get_histogram_ave(&hist_ave);
		if (copy_to_user(argp, &hist_ave,
			sizeof(struct vpp_histgm_ave_s))) {
			pr_vpp(PR_IOC, "VPP_IOC_GET_HIST_AVG failed\n");
			ret = -EFAULT;
		} else {
			pr_vpp(PR_IOC, "VPP_IOC_GET_HIST_AVG success\n");
		}
		break;
	case VPP_IOC_GET_HIST_BIN:
		buffer_size = sizeof(struct vpp_histgm_param_s);
		phist_data = kmalloc(buffer_size, GFP_KERNEL);
		if (!phist_data) {
			pr_vpp(PR_IOC, "VPP_IOC_GET_HIST_BIN malloc failed\n");
			ret = -ENOMEM;
		} else {
			vpp_pq_mgr_get_histogram(phist_data);
			if (copy_to_user(argp, phist_data, buffer_size)) {
				pr_vpp(PR_IOC, "VPP_IOC_GET_HIST_BIN failed\n");
				ret = -EFAULT;
			} else {
				pr_vpp(PR_IOC, "VPP_IOC_GET_HIST_BIN success\n");
			}
			kfree(phist_data);
		}
		break;
	case VPP_IOC_GET_PQ_STATE:
		vpp_pq_mgr_get_status(&pq_state);
		if (copy_to_user(argp, &pq_state,
			sizeof(struct vpp_pq_state_s))) {
			pr_vpp(PR_IOC, "VPP_IOC_GET_PQ_STATE failed\n");
			ret = -EFAULT;
		} else {
			pr_vpp(PR_IOC, "VPP_IOC_GET_PQ_STATE success\n");
		}
		break;
	case VPP_IOC_GET_HDR_HIST:
		buffer_size = sizeof(struct vpp_hdr_histgm_param_s);
		phist_data_hdr = kmalloc(buffer_size, GFP_KERNEL);
		if (!phist_data_hdr) {
			pr_vpp(PR_IOC, "VPP_IOC_GET_HDR_HIST malloc failed\n");
			ret = -ENOMEM;
		} else {
			vpp_pq_mgr_get_hdr_histogram(phist_data_hdr);
			if (copy_to_user(argp, phist_data_hdr, buffer_size)) {
				pr_vpp(PR_IOC, "VPP_IOC_GET_HDR_HIST failed\n");
				ret = -EFAULT;
			} else {
				pr_vpp(PR_IOC, "VPP_IOC_GET_HDR_HIST success\n");
			}
			kfree(phist_data_hdr);
		}
		break;
	case VPP_IOC_GET_PRE_GAMMA_LEN:
		val = vpp_pq_mgr_get_pre_gamma_table_len();
		if (copy_to_user(argp, &val, sizeof(int))) {
			pr_vpp(PR_IOC, "VPP_IOC_GET_PRE_GAMMA_LEN failed\n");
			ret = -EFAULT;
		} else {
			pr_vpp(PR_IOC, "VPP_IOC_GET_PRE_GAMMA_LEN success\n");
		}
		break;
	case VPP_IOC_GET_GAMMA_LEN:
		val = vpp_pq_mgr_get_gamma_table_len();
		if (copy_to_user(argp, &val, sizeof(int))) {
			pr_vpp(PR_IOC, "VPP_IOC_GET_GAMMA_LEN failed\n");
			ret = -EFAULT;
		} else {
			pr_vpp(PR_IOC, "VPP_IOC_GET_GAMMA_LEN success\n");
		}
		break;
	default:
		ret = 0;
		break;
	}

	return ret;
}

static long vpp_compat_ioctl(struct file *filp,
	unsigned int cmd, unsigned long arg)
{
	unsigned long ret;

	arg = (unsigned long)compat_ptr(arg);
	ret = vpp_ioctl(filp, cmd, arg);
	return ret;
}

const struct file_operations vpp_fops = {
	.owner = THIS_MODULE,
	.open = vpp_open,
	.release = vpp_release,
	.unlocked_ioctl = vpp_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = vpp_compat_ioctl,
#endif
};

static void fw_align_hw_config(struct vpp_dev_s *devp)
{
	enum vpp_chip_type_e chip_id;

	chip_id = devp->pm_data->chip_id;

	switch (chip_id) {
	case CHIP_T3:
		devp->pq_en = 1;
		devp->vpp_cfg_data.dnlp_hw = SR1_DNLP;
		devp->vpp_cfg_data.sr_hw = SR0_SR1;
		devp->vpp_cfg_data.bit_depth = BDP_10;
		devp->vpp_cfg_data.gamma_hw = GM_V2;
		devp->vpp_cfg_data.vpp_out.vpp0_out = OUT_PANEL;
		devp->vpp_cfg_data.vpp_out.vpp1_out = OUT_NULL;
		devp->vpp_cfg_data.vpp_out.vpp2_out = OUT_NULL;
		/*vlock data*/
		//devp->vpp_cfg_data.vlk_data.vlk_support = true;
		//devp->vpp_cfg_data.vlk_data.vlk_new_fsm = 1;
		//devp->vpp_cfg_data.vlk_data.vlk_hwver = vlock_hw_tm2verb;
		//devp->vpp_cfg_data.vlk_data.vlk_phlock_en = true;
		//devp->vpp_cfg_data.vlk_data.vlk_pll_sel = vlock_pll_sel_tcon;
		break;
	case CHIP_MAX:
	default:
		devp->vpp_cfg_data.dnlp_hw = SR1_DNLP;
		devp->vpp_cfg_data.sr_hw = SR0_SR1;
		devp->vpp_cfg_data.bit_depth = BDP_10;
		devp->vpp_cfg_data.gamma_hw = GM_V2;
		devp->vpp_cfg_data.vpp_out.vpp0_out = OUT_PANEL;
		devp->vpp_cfg_data.vpp_out.vpp1_out = OUT_NULL;
		devp->vpp_cfg_data.vpp_out.vpp2_out = OUT_NULL;
		/*vlock data*/
		//devp->vpp_cfg_data.vlk_data.vlk_support = false;
		//devp->vpp_cfg_data.vlk_data.vlk_new_fsm = 0;
		//devp->vpp_cfg_data.vlk_data.vlk_hwver = vlock_hw_tm2verb;
		//devp->vpp_cfg_data.vlk_data.vlk_phlock_en = false;
		//devp->vpp_cfg_data.vlk_data.vlk_pll_sel = vlock_pll_sel_tcon;
		devp->pq_en = 0;
		break;
	}
}

void vpp_attach_init(struct vpp_dev_s *devp)
{
	PR_DRV("%s attach_init in\n", __func__);

	vpp_pq_mgr_init(devp);

	return;
	//hw_ops_attach(devp->vpp_ops.hw_ops);
}

static irqreturn_t vpp_lc_curve_isr(int irq, void *dev_id)
{
	vpp_pq_mgr_set_lc_isr();
	return IRQ_HANDLED;
}

static int vpp_dts_parse(struct vpp_dev_s *vpp_devp)
{
	int ret = 0;
	int val;
	struct device_node *of_node;
	const struct of_device_id *of_id;
	struct platform_device *pdev = vpp_devp->pdev;

	of_id = of_match_device(vpp_dts_match, &pdev->dev);
	if (of_id) {
		PR_DRV("%s compatible\n", of_id->compatible);
		vpp_devp->pm_data = (struct match_data_s *)of_id->data;
	}

	of_node = pdev->dev.of_node;
	if (of_node) {
		ret = of_property_read_u32(of_node, "pq_en", &val);
		if (ret)
			PR_DRV("of get node pq_en failed\n");
		else
			vpp_devp->pq_en = val;
	}

	vpp_devp->res_lc_irq = platform_get_resource_byname(pdev,
		IORESOURCE_IRQ, "lc_curve");

	return ret;
}

static int vpp_drv_probe(struct platform_device *pdev)
{
	int ret = 0;
	int i;
	struct vpp_dev_s *vpp_devp = NULL;

	PR_DRV("%s:In\n", __func__);

	vpp_dev = kzalloc(sizeof(*vpp_dev), GFP_KERNEL);
	if (!vpp_dev) {
		PR_ERR("vpp dev kzalloc error\n");
		ret = -1;
		goto fail_alloc_dev;
	}
	memset(vpp_dev, 0, sizeof(struct vpp_dev_s));
	vpp_devp = vpp_dev;

	ret = alloc_chrdev_region(&vpp_devp->devno, 0, VPP_DEVNO, VPP_NAME);
	if (ret < 0) {
		PR_ERR("vpp devno alloc failed\n");
		goto fail_alloc_region;
	}

	vpp_devp->clsp = class_create(THIS_MODULE, VPP_CLS_NAME);
	if (IS_ERR(vpp_devp->clsp)) {
		PR_ERR("vpp class create failed\n");
		ret = -1;
		goto fail_create_class;
	}

	for (i = 0; vpp_class_attr[i].attr.name; i++) {
		ret = class_create_file(vpp_devp->clsp, &vpp_class_attr[i]);
		if (ret < 0) {
			PR_ERR("vpp class create file failed\n");
			goto fail_create_class_file;
		}
	}

	cdev_init(&vpp_devp->vpp_cdev, &vpp_fops);
	vpp_devp->vpp_cdev.owner = THIS_MODULE;
	ret = cdev_add(&vpp_devp->vpp_cdev, vpp_devp->devno, VPP_DEVNO);
	if (ret < 0) {
		PR_ERR("vpp add cdev failed\n");
		goto fail_add_cdev;
	}

	vpp_devp->dev = device_create(vpp_devp->clsp, NULL,
		vpp_devp->devno, vpp_devp, VPP_NAME);
	if (!vpp_devp->dev) {
		PR_ERR("vpp device_create failed\n");
		ret = -1;
		goto fail_create_dev;
	}

	dev_set_drvdata(vpp_devp->dev, vpp_devp);
	platform_set_drvdata(pdev, vpp_devp);
	vpp_devp->pdev = pdev;

	vpp_dts_parse(vpp_devp);

	/*need get vout first, need confirm which vpp out, panel or tx*/

	/*get vout end*/
	fw_align_hw_config(vpp_devp);
	vpp_attach_init(vpp_devp);

	vpp_devp->lcisr_defined = 0;
	if (vpp_devp->res_lc_irq) {
		if (request_irq(vpp_devp->res_lc_irq->start,
				vpp_lc_curve_isr, IRQF_SHARED,
				"lc_curve_isr", (void *)"lc_curve_isr")) {
			PR_ERR("can't request res_lc_curve_irq\n");
		} else {
			vpp_devp->lcisr_defined = 1;
			PR_DRV("request res_lc_curve_irq successful\n");
		}
	}

	return ret;

fail_create_dev:
	cdev_del(&vpp_devp->vpp_cdev);
fail_add_cdev:
	for (i = 0; vpp_class_attr[i].attr.name; i++)
		class_remove_file(vpp_devp->clsp, &vpp_class_attr[i]);
fail_create_class_file:
	class_destroy(vpp_devp->clsp);
fail_create_class:
	unregister_chrdev_region(vpp_devp->devno, VPP_DEVNO);
fail_alloc_region:
	kfree(vpp_dev);
	vpp_dev = NULL;
fail_alloc_dev:
	return ret;
}

static int vpp_drv_remove(struct platform_device *pdev)
{
	int i;
	struct vpp_dev_s *vpp_devp = get_vpp_dev();

	device_destroy(vpp_devp->clsp, vpp_devp->devno);
	cdev_del(&vpp_devp->vpp_cdev);

	for (i = 0; vpp_class_attr[i].attr.name; i++)
		class_remove_file(vpp_devp->clsp, &vpp_class_attr[i]);
	class_destroy(vpp_devp->clsp);

	unregister_chrdev_region(vpp_devp->devno, VPP_DEVNO);

	vpp_pq_mgr_deinit();

	kfree(vpp_dev);
	vpp_dev = NULL;

	return 0;
}

static void vpp_drv_shutdown(struct platform_device *pdev)
{
	int i;
	struct vpp_dev_s *vpp_devp = get_vpp_dev();

	device_destroy(vpp_devp->clsp, vpp_devp->devno);
	cdev_del(&vpp_devp->vpp_cdev);
	for (i = 0; vpp_class_attr[i].attr.name; i++)
		class_remove_file(vpp_devp->clsp, &vpp_class_attr[i]);
	class_destroy(vpp_devp->clsp);
	unregister_chrdev_region(vpp_devp->devno, VPP_DEVNO);
	kfree(vpp_dev);
	vpp_dev = NULL;
}

static int vpp_drv_suspend(struct platform_device *pdev, pm_message_t state)
{
	return 0;
}

static int vpp_drv_resume(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver vpp_driver = {
	.probe = vpp_drv_probe,
	.remove = vpp_drv_remove,
	.shutdown = vpp_drv_shutdown,
	.suspend = vpp_drv_suspend,
	.resume = vpp_drv_resume,
	.driver = {
		.name = "aml_vpp",
		.owner = THIS_MODULE,
		.of_match_table = vpp_dts_match,
	},
};

int __init vpp_drv_init(void)
{
	PR_DRV("%s module init\n", __func__);

	if (platform_driver_register(&vpp_driver)) {
		PR_ERR("%s module init failed\n", __func__);
		return -ENODEV;
	}

	return 0;
}

void __exit vpp_drv_exit(void)
{
	PR_DRV("%s:module exit\n", __func__);
	platform_driver_unregister(&vpp_driver);
}

