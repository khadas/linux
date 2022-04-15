/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __VPP_MODULE_VADJ_H__
#define __VPP_MODULE_VADJ_H__

int vpp_module_vadj_init(struct vpp_dev_s *pdev);
int vpp_module_vadj_en(bool enable);
int vpp_module_vadj_post_en(bool enable);
int vpp_module_vadj_set_brightness(int val);
int vpp_module_vadj_set_brightness_post(int val);
int vpp_module_vadj_set_contrast(int val);
int vpp_module_vadj_set_contrast_post(int val);
int vpp_module_vadj_set_sat_hue(int val);
int vpp_module_vadj_set_sat_hue_post(int val);

#endif

