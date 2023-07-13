/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __VDIN_HW_H
#define __VDIN_HW_H

void vdin_wr(struct vdin_dev_s *devp, u32 reg, const u32 val);
void vdin_wr_bits(struct vdin_dev_s *devp, u32 reg, const u32 val, const u32 start, const u32 len);
unsigned int vdin_get_field_type(unsigned int offset);
#endif
