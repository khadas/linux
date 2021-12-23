/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef _MESON_VPU_GLOBAL_MIF_H_
#define _MESON_VPU_GLOBAL_MIF_H_

#define VPP_OSD1_SCALE_CTRL                        0x1a73
#define VPP_OSD2_SCALE_CTRL                        0x1a74
#define VPP_OSD3_SCALE_CTRL                        0x1a75
#define VPP_OSD4_SCALE_CTRL                        0x1a76

#define VPP_VD1_DSC_CTRL                           0x1a83
#define VPP_VD2_DSC_CTRL                           0x1a84
#define VPP_VD3_DSC_CTRL                           0x1a85

#define MALI_AFBCD1_TOP_CTRL                       0x1a55
#define MALI_AFBCD2_TOP_CTRL                       0x1a56
#define PATH_START_SEL                             0x1a8a

#define VIU_VD1_PATH_CTRL                          0x1a73
#define VIU_VD2_PATH_CTRL                          0x1a74

#define VIU_OSD1_PATH_CTRL                         0x1a76
#define VIU_OSD2_PATH_CTRL                         0x1a77
#define VIU_OSD3_PATH_CTRL                         0x1a78

#define OSD1_HDR_IN_SIZE                           0x1a5a
#define OSD2_HDR_IN_SIZE                           0x1a5b
#define OSD3_HDR_IN_SIZE                           0x1a5c
#define OSD4_HDR_IN_SIZE                           0x1a5d
#define OSD1_HDR2_CTRL                             0x38a0
#define OSD2_HDR2_CTRL                             0x5b00
#define OSD3_HDR2_CTRL                             0x5b50
#define OSD4_HDR2_CTRL                             0x5ba0
#define OSD2_HDR2_MATRIXI_EN_CTRL                  0x5b3b
#define OSD2_HDR2_MATRIXO_EN_CTRL                  0x5b3c
#define OSD3_HDR2_MATRIXI_EN_CTRL                  0x5b8b
#define OSD3_HDR2_MATRIXO_EN_CTRL                  0x5b8c
#define OSD4_HDR2_MATRIXI_EN_CTRL                  0x5bdb
#define OSD4_HDR2_MATRIXO_EN_CTRL                  0x5bdc

void fix_vpu_clk2_default_regs(void);

void independ_path_default_regs(void);

#endif
