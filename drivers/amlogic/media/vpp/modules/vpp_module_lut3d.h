/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __VPP_MODULE_LUT3D_H__
#define __VPP_MODULE_LUT3D_H__

#define LUT3D_UNIT_DATA_LEN (17)
#define LUT3D_DATA_SIZE (LUT3D_UNIT_DATA_LEN * LUT3D_UNIT_DATA_LEN * LUT3D_UNIT_DATA_LEN * 3)

int vpp_module_lut3d_init(struct vpp_dev_s *pdev);
void vpp_module_lut3d_en(bool enable);
void vpp_module_lut3d_set_data(int *pdata, int by_vsync);

void vpp_module_lut3d_dump_info(enum vpp_dump_module_info_e info_type);

#endif

