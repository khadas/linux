/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __AML_AUDIO_EFFECTS_HW_H__
#define __AML_AUDIO_EFFECTS_HW_H__
#include <linux/types.h>
#include <linux/errno.h>

#include "regs.h"
#include "iomap.h"

int DRC0_enable(int enable, int thd0, int k0);
int init_EQ_DRC_module(void);
int set_internal_EQ_volume(unsigned int master_volume,
			   unsigned int channel_1_volume,
			   unsigned int channel_2_volume);
void aed_req_sel(bool enable, int sel, int req_module);
int aed_get_req_sel(int sel);
void aed_set_eq(int enable, int params_len, unsigned int *params);
void aed_set_drc(int enable, int drc_len, unsigned int *drc_params,
		 int drc_tko_len, unsigned int *drc_tko_params);
int aml_aed_format_set(int frddr_dst);
void aed_src_select(bool enable, int frddr_dst, int fifo_id);
void aed_set_lane(int lane_mask);
void aed_set_channel(int channel_mask);
#endif
