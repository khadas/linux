/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __VPP_MODULE_VE_H__
#define __VPP_MODULE_VE_H__

int vpp_module_ve_init(struct vpp_dev_s *pdev);
int vpp_module_ve_blkext_en(bool enable);
int vpp_module_ve_set_blkext_slope(int val);
int vpp_module_ve_set_blkext_start(int val);
int vpp_module_ve_ccoring_en(bool enable);
int vpp_module_ve_set_ccoring_slope(int val);
int vpp_module_ve_set_ccoring_th(int val);

#endif

