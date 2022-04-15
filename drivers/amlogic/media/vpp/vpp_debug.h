/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __VPP_DEBUG_H__
#define __VPP_DEBUG_H__

int vpp_debug_init(struct vpp_dev_s *pdev);
ssize_t vpp_debug_show(struct class *class,
	struct class_attribute *attr,
	char *buf);
ssize_t vpp_debug_store(struct class *class,
	struct class_attribute *attr,
	const char *buf, size_t count);
ssize_t vpp_debug_brightness_show(struct class *class,
	struct class_attribute *attr,
	char *buf);
ssize_t vpp_debug_brightness_store(struct class *class,
	struct class_attribute *attr,
	const char *buf, size_t count);
ssize_t vpp_debug_brightness_post_show(struct class *class,
	struct class_attribute *attr,
	char *buf);
ssize_t vpp_debug_brightness_post_store(struct class *class,
	struct class_attribute *attr,
	const char *buf, size_t count);
ssize_t vpp_debug_contrast_show(struct class *class,
	struct class_attribute *attr,
	char *buf);
ssize_t vpp_debug_contrast_store(struct class *class,
	struct class_attribute *attr,
	const char *buf, size_t count);
ssize_t vpp_debug_contrast_post_show(struct class *class,
	struct class_attribute *attr,
	char *buf);
ssize_t vpp_debug_contrast_post_store(struct class *class,
	struct class_attribute *attr,
	const char *buf, size_t count);
ssize_t vpp_debug_saturation_show(struct class *class,
	struct class_attribute *attr,
	char *buf);
ssize_t vpp_debug_saturation_store(struct class *class,
	struct class_attribute *attr,
	const char *buf, size_t count);
ssize_t vpp_debug_saturation_post_show(struct class *class,
	struct class_attribute *attr,
	char *buf);
ssize_t vpp_debug_saturation_post_store(struct class *class,
	struct class_attribute *attr,
	const char *buf, size_t count);
ssize_t vpp_debug_hue_show(struct class *class,
	struct class_attribute *attr,
	char *buf);
ssize_t vpp_debug_hue_store(struct class *class,
	struct class_attribute *attr,
	const char *buf, size_t count);
ssize_t vpp_debug_hue_post_show(struct class *class,
	struct class_attribute *attr,
	char *buf);
ssize_t vpp_debug_hue_post_store(struct class *class,
	struct class_attribute *attr,
	const char *buf, size_t count);
ssize_t vpp_debug_pre_gamma_show(struct class *class,
	struct class_attribute *attr,
	char *buf);
ssize_t vpp_debug_pre_gamma_store(struct class *class,
	struct class_attribute *attr,
	const char *buf, size_t count);
ssize_t vpp_debug_pre_gamma_pattern_show(struct class *class,
	struct class_attribute *attr,
	char *buf);
ssize_t vpp_debug_pre_gamma_pattern_store(struct class *class,
	struct class_attribute *attr,
	const char *buf, size_t count);
ssize_t vpp_debug_gamma_show(struct class *class,
	struct class_attribute *attr,
	char *buf);
ssize_t vpp_debug_gamma_store(struct class *class,
	struct class_attribute *attr,
	const char *buf, size_t count);
ssize_t vpp_debug_gamma_pattern_show(struct class *class,
	struct class_attribute *attr,
	char *buf);
ssize_t vpp_debug_gamma_pattern_store(struct class *class,
	struct class_attribute *attr,
	const char *buf, size_t count);
ssize_t vpp_debug_whitebalance_show(struct class *class,
	struct class_attribute *attr,
	char *buf);
ssize_t vpp_debug_whitebalance_store(struct class *class,
	struct class_attribute *attr,
	const char *buf, size_t count);
ssize_t vpp_debug_module_ctrl_show(struct class *class,
	struct class_attribute *attr,
	char *buf);
ssize_t vpp_debug_module_ctrl_store(struct class *class,
	struct class_attribute *attr,
	const char *buf, size_t count);

#endif

