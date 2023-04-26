/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __EFFECTS_HW_ACC_H__
#define __EFFECTS_HW_ACC_H__

#include <linux/types.h>
#include "audio_io.h"
#include "ddr_mngr.h"

enum {
	VERSION1 = 0,
	VERSION2,
	VERSION3,
	VERSION4
};

#define ACC_EQ_BAND                   (20)
#define ACC_EQ_FILTER_PARAM_SIZE      (5)
#define ACC_EQ_FILTER_SIZE_CH         (100)
#define ACC_EQ_FILTER_SIZE            (200)
#define ACC_EQ_INDEX                  (8)

#define ACC_EQ_DC_RAM_ADDR            (0)
#define ACC_EQ_DC_RAM_SIZE            (5)
#define ACC_EQ_MIXER_RAM_ADDR         (5)
#define ACC_EQ_MIXER_RAM_SIZE         (4)
#define ACC_EQ_FILTER_RAM_ADDR        (9)
#define ACC_EQ_FILTER_RAM_SIZE        (100)
#define ACC_EQ_MIXER_FILTER_OFFSET_SIZE (ACC_EQ_MIXER_RAM_SIZE + ACC_EQ_FILTER_RAM_SIZE)

#define ACC_FILTER_PARAM_BYTE             (66) /*"0x%8.8x "*/

void acc_eq_init_ram_coeff(struct aml_audio_controller *actrl,
			int ram_count, int addr, int len, unsigned int *params);
void acc_eq_set_ram_coeff(struct aml_audio_controller *actrl,
			int ram_count, int addr, int len, unsigned int *params);
void acc_eq_get_ram_coeff(struct aml_audio_controller *actrl,
			int ram_count, int addr, int len, unsigned int *params);
void acc_eq_clk_enable(struct aml_audio_controller *actrl, bool enable);
void acc_eq_set_wrapper(struct aml_audio_controller *actrl);

#endif
