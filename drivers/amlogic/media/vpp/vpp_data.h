/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __VPP_DATA_H__
#define __VPP_DATA_H__

struct data_vs_param_s {
	enum vpp_data_src_e src_type;
	unsigned int vf_signal_change;
	unsigned int vf_width;
	unsigned int vf_height;
};

int vpp_data_init(struct vpp_dev_s *pdev);
void vpp_data_updata_reg_lc(struct vpp_lc_param_s *pdata);
void vpp_data_updata_reg_sr0(struct vpp_sr0_param_s *pdata);
void vpp_data_updata_reg_sr1(struct vpp_sr1_param_s *pdata);
void vpp_data_on_vs(struct data_vs_param_s *pvs_param);

#endif

