/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __VPP_MODULE_DNLP_H__
#define __VPP_MODULE_DNLP_H__

int vpp_module_dnlp_init(struct vpp_dev_s *pdev);
void vpp_module_dnlp_en(bool enable);
void vpp_module_dnlp_update_data(int hist_luma_sum, int *phist_data);
void vpp_module_dnlp_get_sat_compensation(bool *psat_comp, int *psat_val);
void vpp_module_dnlp_set_param(struct vpp_dnlp_curve_param_s *pdata);

#endif

