/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __VPP_MODULE_GO_H__
#define __VPP_MODULE_GO_H__

int vpp_module_go_init(struct vpp_dev_s *pdev);
void vpp_module_go_en(bool enable);
void vpp_module_go_set_gain(unsigned char idx, int val);
void vpp_module_go_set_offset(unsigned char idx, int val);
void vpp_module_go_set_pre_offset(unsigned char idx, int val);

void vpp_module_go_dump_info(enum vpp_dump_module_info_e info_type);

#endif

