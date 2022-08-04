/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __VPP_VF_PROC_H__
#define __VPP_VF_PROC_H__

struct vpp_vf_signal_info_s {
	unsigned int format;
	unsigned int range;
	unsigned int color_primaries;
	unsigned int transfer_characteristic;
};

struct vpp_vf_param_s {
	unsigned int sps_h_en;
	unsigned int sps_v_en;
	unsigned int sps_w_in;
	unsigned int sps_h_in;
	unsigned int cm_in_w;
	unsigned int cm_in_h;
};

void vpp_vf_proc(struct vframe_s *pvf,
	struct vframe_s *ptoggle_vf,
	struct vpp_vf_param_s *pvf_param,
	int flags,
	enum vpp_vd_path_e vd_path,
	enum vpp_vf_top_e vpp_top);

#endif

