/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __EFFECTS_HW_V2_H__
#define __EFFECTS_HW_V2_H__
#include <linux/types.h>
#include "ddr_mngr.h"

enum {
	VERSION1 = 0,
	VERSION2,
	VERSION3,
	VERSION4
};

void aed_init_ram_coeff(int version, int add, int len, unsigned int *params);
void aed_set_ram_coeff(int version, int add, int len, unsigned int *params);
void aed_get_ram_coeff(int add, int len, unsigned int *params);
void aed_set_multiband_drc_coeff(int band, unsigned int *params);
void aed_get_multiband_drc_coeff(int band, unsigned int *params);
void aed_set_fullband_drc_coeff(int group, unsigned int *params);
void aed_get_fullband_drc_coeff(int len, unsigned int *params);
void aed_dc_enable(bool enable);
void aed_nd_enable(bool enable);
void aed_eq_enable(int idx, bool enable);
void aed_set_fullband_drc_enable(bool enable);
void aed_set_multiband_drc_enable(bool enable);
void aed_set_volume(unsigned int master_volume,
		    unsigned int lch_vol, unsigned int rch_vol);
void aed_set_lane_and_channels(int lane_mask, int ch_mask);
void aed_set_lane_and_channels_v3(int lane_mask, int ch_mask);
void aed_set_ctrl(bool enable, int sel,
		  enum frddr_dest module, int offset);
void aed_set_format(int msb, enum ddr_types frddr_type,
		    enum ddr_num source, int offset);
void aed_reload_config(void);
void aed_enable(bool enable);
void aed_set_mixer_params(void);
void aed_eq_taps(unsigned int eq1_taps);
void aed_set_multiband_drc_param(void);
void aed_set_fullband_drc_param(int tap);
void aed_module_reset(int offset);

#endif
