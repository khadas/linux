/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __ADDR_ATSC_CNTR_BIT_H__
#define __ADDR_ATSC_CNTR_BIT_H__

union atsc_cntl_reg_0x20 {
	unsigned int bits;
	struct	 {
		unsigned int	cpu_rst                        :1,
		reserved0                      :3,
		cr_pilot_only                  :1,
		reserved1                      :27;
	} b;
};

struct	ATSC_CNTR_REG_0X21_BITS {
	unsigned int	sys_sm_ow_ctrl                 :10,
	reserved2                      :6,
	nco_loop_close_dly             :10,
	reserved3                      :6;
};
struct	ATSC_CNTR_REG_0X22_BITS {
	unsigned int	flock_to_cnt                   :8,
	no_crlock_to                   :1,
	reserved4                      :3,
	no_snr_to                      :1,
	reserved5                      :3,
	no_ddm_to                      :1,
	reserved6                      :15;
};
struct	ATSC_CNTR_REG_0X23_BITS {
	unsigned int	flock_cnt_limit                :6,
	reserved7                      :2,
	flock_th                       :6,
	reserved8                      :2,
	pn511_acum_th                  :16;
};
struct	ATSC_CNTR_REG_0X24_BITS {
	unsigned int	crdet_mode_sel_div             :8,
	crdet_mode_sel_ndiv            :8,
	ffe_tap_pwr_th                 :10,
	reserved9                      :6;
};
struct	ATSC_CNTR_REG_0X29_BITS {
	unsigned int	cw_rg0                         :25,
	reserved10                     :7;
};
struct	ATSC_CNTR_REG_0X2A_BITS {
	unsigned int	cw_rg1                         :25,
	reserved11                     :7;
};
struct	ATSC_CNTR_REG_0X2B_BITS {
	unsigned int	cw_rg2                         :25,
	reserved12                     :7;
};
struct	ATSC_CNTR_REG_0X2C_BITS {
	unsigned int	cw_rg3                         :25,
	reserved13                     :3,
	cw_exists                      :1,
	reserved14                     :3;
};
struct	ATSC_CNTR_REG_0X2D_BITS {
	unsigned int	inband_pwr_avg_out             :16,
	inband_pwr_out                 :14,
	reserved15                     :1,
	inband_pwr_low_flag            :1;
};
struct	ATSC_CNTR_REG_0X2E_BITS {
	unsigned int	sys_states                     :4,
	eq_states                      :4,
	reserved16                     :24;
};

#endif
