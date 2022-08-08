/* SPDX-License-Identifier: (GPL-2.0+ OR MIT)*/
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __VPP_HW_H__
#define __VPP_HW_H__

enum reg_cfg_mode {
	CFG_DIR = 0,
	CFG_RDMA
};

enum mod_ctl_e {
	DISABLE = 0,
	ENABLE
};

struct reg_data1_s {
	unsigned int reg_addr;
	unsigned int msk;
	unsigned int value;
};

struct reg_data2_s {
	unsigned int reg_addr;
	unsigned int value;
	unsigned int st;
	unsigned int len;
};

struct cfg_data_s {
	enum reg_cfg_mode reg_mod;
	struct reg_data1_s *data1;
	struct reg_data2_s *data2;
	void *data_p;
};

struct hw_ops_s {
	void (*hist_cfg)(struct cfg_data_s *cfg_data);
	int (*sr0_cfg)(struct cfg_data_s *cfg_data);
	int (*sr0_dnlp_cfg)(struct cfg_data_s *cfg_data);
	int (*sr0_dnlp_curve)(struct cfg_data_s *cfg_data);
	int (*sr1_cfg)(struct cfg_data_s *cfg_data);
	int (*sr1_dnlp_cfg)(struct cfg_data_s *cfg_data);
	int (*sr1_dnlp_curve)(struct cfg_data_s *cfg_data);
	int (*sr1_lc_cfg)(struct cfg_data_s *cfg_data);
	int (*cc_cfg)(struct cfg_data_s *cfg_data);
	int (*ble_cfg)(struct cfg_data_s *cfg_data);
	int (*cm_cfg)(struct cfg_data_s *cfg_data);
	int (*blue_str_cfg)(struct cfg_data_s *cfg_data);
	int (*vd_adj_cfg)(struct cfg_data_s *cfg_data);
	int (*lut3d_cfg)(struct cfg_data_s *cfg_data);
	int (*pregm_set)(struct cfg_data_s *cfg_data);
	int (*wb_set)(struct cfg_data_s *cfg_data);
	int (*outclip_set)(struct cfg_data_s *cfg_data);
	int (*gm_set)(struct cfg_data_s *cfg_data);
	int (*mtx_cfg)(struct cfg_data_s *cfg_data);
};

void hw_ops_attach(const struct hw_ops_s *ops);
#endif

