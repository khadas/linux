/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __VPP_DEBUG_H__
#define __VPP_DEBUG_H__

int vpp_debug_init(struct vpp_dev_s *pdev);
ssize_t vpp_debug_show(struct class *class,
	struct class_attribute *attr, char *buf);
ssize_t vpp_debug_store(struct class *class,
	struct class_attribute *attr, const char *buf, size_t count);
ssize_t vpp_debug_reg_show(struct class *class,
	struct class_attribute *attr, char *buf);
ssize_t vpp_debug_reg_store(struct class *class,
	struct class_attribute *attr, const char *buf, size_t count);
ssize_t vpp_debug_cm_reg_show(struct class *class,
	struct class_attribute *attr, char *buf);
ssize_t vpp_debug_cm_reg_store(struct class *class,
	struct class_attribute *attr, const char *buf, size_t count);
ssize_t vpp_debug_brightness_show(struct class *class,
	struct class_attribute *attr, char *buf);
ssize_t vpp_debug_brightness_store(struct class *class,
	struct class_attribute *attr, const char *buf, size_t count);
ssize_t vpp_debug_brightness_post_show(struct class *class,
	struct class_attribute *attr, char *buf);
ssize_t vpp_debug_brightness_post_store(struct class *class,
	struct class_attribute *attr, const char *buf, size_t count);
ssize_t vpp_debug_contrast_show(struct class *class,
	struct class_attribute *attr, char *buf);
ssize_t vpp_debug_contrast_store(struct class *class,
	struct class_attribute *attr, const char *buf, size_t count);
ssize_t vpp_debug_contrast_post_show(struct class *class,
	struct class_attribute *attr, char *buf);
ssize_t vpp_debug_contrast_post_store(struct class *class,
	struct class_attribute *attr, const char *buf, size_t count);
ssize_t vpp_debug_saturation_show(struct class *class,
	struct class_attribute *attr, char *buf);
ssize_t vpp_debug_saturation_store(struct class *class,
	struct class_attribute *attr, const char *buf, size_t count);
ssize_t vpp_debug_saturation_post_show(struct class *class,
	struct class_attribute *attr, char *buf);
ssize_t vpp_debug_saturation_post_store(struct class *class,
	struct class_attribute *attr, const char *buf, size_t count);
ssize_t vpp_debug_hue_show(struct class *class,
	struct class_attribute *attr, char *buf);
ssize_t vpp_debug_hue_store(struct class *class,
	struct class_attribute *attr, const char *buf, size_t count);
ssize_t vpp_debug_hue_post_show(struct class *class,
	struct class_attribute *attr, char *buf);
ssize_t vpp_debug_hue_post_store(struct class *class,
	struct class_attribute *attr, const char *buf, size_t count);
ssize_t vpp_debug_sharpness_show(struct class *class,
	struct class_attribute *attr, char *buf);
ssize_t vpp_debug_sharpness_store(struct class *class,
	struct class_attribute *attr, const char *buf, size_t count);
ssize_t vpp_debug_pre_gamma_show(struct class *class,
	struct class_attribute *attr, char *buf);
ssize_t vpp_debug_pre_gamma_store(struct class *class,
	struct class_attribute *attr, const char *buf, size_t count);
ssize_t vpp_debug_pre_gamma_pattern_show(struct class *class,
	struct class_attribute *attr, char *buf);
ssize_t vpp_debug_pre_gamma_pattern_store(struct class *class,
	struct class_attribute *attr, const char *buf, size_t count);
ssize_t vpp_debug_gamma_show(struct class *class,
	struct class_attribute *attr, char *buf);
ssize_t vpp_debug_gamma_store(struct class *class,
	struct class_attribute *attr, const char *buf, size_t count);
ssize_t vpp_debug_gamma_pattern_show(struct class *class,
	struct class_attribute *attr, char *buf);
ssize_t vpp_debug_gamma_pattern_store(struct class *class,
	struct class_attribute *attr, const char *buf, size_t count);
ssize_t vpp_debug_whitebalance_show(struct class *class,
	struct class_attribute *attr, char *buf);
ssize_t vpp_debug_whitebalance_store(struct class *class,
	struct class_attribute *attr, const char *buf, size_t count);
ssize_t vpp_debug_module_ctrl_show(struct class *class,
	struct class_attribute *attr, char *buf);
ssize_t vpp_debug_module_ctrl_store(struct class *class,
	struct class_attribute *attr, const char *buf, size_t count);
ssize_t vpp_debug_dnlp_show(struct class *class,
	struct class_attribute *attr, char *buf);
ssize_t vpp_debug_dnlp_store(struct class *class,
	struct class_attribute *attr, const char *buf, size_t count);
ssize_t vpp_debug_lc_show(struct class *class,
	struct class_attribute *attr, char *buf);
ssize_t vpp_debug_lc_store(struct class *class,
	struct class_attribute *attr, const char *buf, size_t count);
ssize_t vpp_debug_hdr_type_show(struct class *class,
	struct class_attribute *attr, char *buf);
ssize_t vpp_debug_hdr_type_store(struct class *class,
	struct class_attribute *attr, const char *buf, size_t count);
ssize_t vpp_debug_pc_mode_show(struct class *class,
	struct class_attribute *attr, char *buf);
ssize_t vpp_debug_pc_mode_store(struct class *class,
	struct class_attribute *attr, const char *buf, size_t count);
ssize_t vpp_debug_csc_type_show(struct class *class,
	struct class_attribute *attr, char *buf);
ssize_t vpp_debug_csc_type_store(struct class *class,
	struct class_attribute *attr, const char *buf, size_t count);
ssize_t vpp_debug_color_primary_show(struct class *class,
	struct class_attribute *attr, char *buf);
ssize_t vpp_debug_color_primary_store(struct class *class,
	struct class_attribute *attr, const char *buf, size_t count);
ssize_t vpp_debug_histogram_show(struct class *class,
	struct class_attribute *attr, char *buf);
ssize_t vpp_debug_histogram_store(struct class *class,
	struct class_attribute *attr, const char *buf, size_t count);
ssize_t vpp_debug_histogram_ave_show(struct class *class,
	struct class_attribute *attr, char *buf);
ssize_t vpp_debug_histogram_ave_store(struct class *class,
	struct class_attribute *attr, const char *buf, size_t count);
ssize_t vpp_debug_histogram_hdr_show(struct class *class,
	struct class_attribute *attr, char *buf);
ssize_t vpp_debug_histogram_hdr_store(struct class *class,
	struct class_attribute *attr, const char *buf, size_t count);
ssize_t vpp_debug_hdr_metadata_show(struct class *class,
	struct class_attribute *attr, char *buf);
ssize_t vpp_debug_hdr_metadata_store(struct class *class,
	struct class_attribute *attr, const char *buf, size_t count);
ssize_t vpp_debug_matrix_show(struct class *class,
	struct class_attribute *attr, char *buf);
ssize_t vpp_debug_matrix_store(struct class *class,
	struct class_attribute *attr, const char *buf, size_t count);
ssize_t vpp_debug_eye_protect_show(struct class *class,
	struct class_attribute *attr, char *buf);
ssize_t vpp_debug_eye_protect_store(struct class *class,
	struct class_attribute *attr, const char *buf, size_t count);
ssize_t vpp_debug_color_curve_show(struct class *class,
	struct class_attribute *attr, char *buf);
ssize_t vpp_debug_color_curve_store(struct class *class,
	struct class_attribute *attr, const char *buf, size_t count);
ssize_t vpp_debug_overscan_show(struct class *class,
	struct class_attribute *attr, char *buf);
ssize_t vpp_debug_overscan_store(struct class *class,
	struct class_attribute *attr, const char *buf, size_t count);

#endif

