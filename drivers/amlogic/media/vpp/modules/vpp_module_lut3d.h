/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __VPP_MODULE_LUT3D_H__
#define __VPP_MODULE_LUT3D_H__

#define LUT3D_UNIT_DATA_LEN (17)

int vpp_module_lut3d_init(struct vpp_dev_s *pdev);
void vpp_module_lut3d_en(bool enable);
void vpp_module_lut3d_set_data(int *pdata);

#endif

