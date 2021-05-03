/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef VP_CTRL_H
#define VP_CTRL_H

#include "frc_drv.h"

void frc_vp_param_init(struct frc_dev_s *frc_devp);
void frc_vp_ctrl(struct frc_dev_s *frc_devp);
void frc_vp_add_7_seg(void);
void frc_set_global_dehalo_en(u32 set_value);

#endif
