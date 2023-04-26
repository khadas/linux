/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __ASRC_HW_ACC_H__
#define __ASRC_HW_ACC_H__

#include <linux/types.h>
#include "audio_io.h"
#include "ddr_mngr.h"

void acc_asrc_set_ram_coeff_aa(struct aml_audio_controller *actrl, int len,
				unsigned int *params);
void acc_asrc_set_ram_coeff_sinc(struct aml_audio_controller *actrl, int len,
				unsigned int *params);
void acc_asrc_clk_enable(struct aml_audio_controller *actrl, bool enable);
void acc_asrc_set_wrapper(struct aml_audio_controller *actrl);
void acc_asrc_arm_init(struct aml_audio_controller *actrl);
void acc_asrc_set_ratio(struct aml_audio_controller *actrl, int input_sr,
				int output_sr, int channel);
void acc_asrc_enable(struct aml_audio_controller *actrl, bool enable);

#endif
