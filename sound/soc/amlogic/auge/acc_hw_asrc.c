// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/kernel.h>

#include "regs.h"
#include "iomap.h"
#include "acc_hw_asrc.h"
#include "acc_asrc_coeff.h"

void acc_asrc_set_ram_coeff_aa(struct aml_audio_controller *actrl, int len,
				unsigned int *params)
{
	int i;
	unsigned int *p = params;

	aml_acc_write(actrl, AUDIO_RSAMP_ACC_AA_COEF_ADDR, AA_FILTER_COEF_ADDR);
	for (i = 0; i < len; i++, p++)
		aml_acc_write(actrl, AUDIO_RSAMP_ACC_AA_COEF_DATA, *p);
}

void acc_asrc_set_ram_coeff_sinc(struct aml_audio_controller *actrl, int len,
				unsigned int *params)
{
	int i;
	unsigned int *p = params;

	aml_acc_write(actrl, AUDIO_RSAMP_ACC_SINC_COEF_ADDR, SINC8_FILTER_COEF_ADDR);
	for (i = 0; i < len; i++, p++)
		aml_acc_write(actrl, AUDIO_RSAMP_ACC_SINC_COEF_DATA, *p);
}

void acc_asrc_clk_enable(struct aml_audio_controller *actrl, bool enable)
{
	/* fixed sys clk and core clk enable */
	aml_acc_update_bits(actrl, AUDIO_ACC_CLK_GATE_EN, 0x3, enable << 1 | enable);
}

void acc_asrc_set_wrapper(struct aml_audio_controller *actrl)
{
	/* config acc wrapper */
	aml_acc_update_bits(actrl, AUDIO_ACC_ASRC_WRAPPER_TOP_CTL0,
			0x1 << 31 | 0x7 << 11 | 0x1f << 6 | 0x7 << 3 | 0x7,
			0 << 31 | //reg_start_en       [31]
			4 << 11 | //reg_data_type      [13:11]
			31 << 6 | //reg_data_msb       [10:6]
			1 << 3  | //reg_frddr_source   [5:3]
			1 << 2 | //reg_afifo_in_rst   [2]
			1 << 1 | //reg_afifo_out_rst  [1]
			0 << 0   //reg_eqdrc_en       [0]
			);
	aml_acc_update_bits(actrl, AUDIO_ACC_ASRC_WRAPPER_TOP_CTL1, 0xf << 16 | 0xffff,
			1 << 16 | //reg_channel_valid the valid channel num [19:16]
			0 << 7  | // reg_eq_ch7_en [7]
			0 << 6  | // reg_eq_ch6_en [6]
			0 << 5  | // reg_eq_ch5_en [5]
			0 << 4  | // reg_eq_ch4_en [4]
			0 << 3  | // reg_eq_ch3_en [3]
			0 << 2  | // reg_eq_ch2_en [2]
			1 << 1  | // reg_eq_ch1_en [1]
			1 << 0    // reg_eq_ch0_en [0]
			);
	aml_acc_update_bits(actrl, AUDIO_ACC_ASRC_WRAPPER_TOP_CTL2,
			0x7 | 0x1 << 3 | 0x7 << 4 | 0x1 << 7 | 0x7 << 8 | 0x1 << 11 | 0xff << 12,
			/* reg_req_sel0       <= pwdata_aed[2:0]
			 * 0: toddr, 1: asrc, 2: tdmout_a, 3: tdmout_b, 4: spdifout
			 */
			3 << 0  |
			1 << 3  | //  reg_req_sel0_en    <= pwdata_aed[3]
			0 << 4  | //  reg_req_sel1       <= pwdata_aed[6:4]
			0 << 7  | //  reg_req_sel1_en    <= pwdata_aed[7]
			0 << 8  | //  reg_req_sel2       <= pwdata_aed[10:8]
			0 << 11 | //  reg_req_sel2_en    <= pwdata_aed[11]
			0 << 12   //  reg_ack_num        <= pwdata_aed[19:12]
			);
	aml_acc_write(actrl, AUDIO_ACC_ASRC_WRAPPER_TOP_CTL3,
			7 << 28 | 6 << 24 | 5 << 20 | 4 << 16 |
			3 << 12 | 2 << 8  | 1 << 4  | 0 << 0);
	aml_acc_write(actrl, AUDIO_ACC_ASRC_WRAPPER_TOP_CTL4,
			15 << 28 | 14 << 24 | 13 << 20 | 12 << 16 |
			11 << 12 | 10 << 8  | 9 << 4 | 8 << 0);
	aml_acc_update_bits(actrl, AUDIO_ACC_ASRC_WRAPPER_TOP_CTL0,
			1 << 31 | 1 << 0, 1 << 31 | 1 << 0);
}

void acc_asrc_arm_init(struct aml_audio_controller *actrl)
{
	/* set aa filter & sinc filter for init */
	acc_asrc_set_ram_coeff_sinc(actrl, SINC8_FILTER_COEF_SIZE, &sinc8_coef[0]);
	acc_asrc_set_ram_coeff_aa(actrl, AA_FILTER_COEF_SIZE, &aa_coef_a_half[0]);

	aml_acc_update_bits(actrl, AUDIO_RSAMP_ACC_CTRL2, 0xff << 8, 0x3f << 8);
	aml_acc_update_bits(actrl, AUDIO_RSAMP_ACC_CTRL2, 0xff, 0x3f);
	aml_acc_update_bits(actrl, AUDIO_RSAMP_ACC_CTRL1, 0x1 << 12, 0x1 << 12);
}

void acc_asrc_set_ratio(struct aml_audio_controller *actrl, int input_sr,
				int output_sr, int channel)
{
	u32 input_sample_rate = (u32)input_sr;
	u32 output_sample_rate = (u32)output_sr;
	u64 phase_step = (u64)input_sample_rate;

	aml_acc_update_bits(actrl, AUDIO_RSAMP_ACC_CTRL1, 0x1 << 11, 0x0 << 11);

	aml_acc_write(actrl, AUDIO_RSAMP_PHSINIT, 0x0);

	/* max downsample ratio is 8, if more than 8, enable AA filter */
	if (div_u64(input_sample_rate, output_sample_rate) > 8) {
		aml_acc_update_bits(actrl, AUDIO_RSAMP_ACC_CTRL2, 0x3 << 16, 0x1 << 16);
		aml_acc_update_bits(actrl, AUDIO_RSAMP_ACC_CTRL1, 0x1 << 10, 0x1 << 10);
		phase_step *= (1 << 27);
	} else {
		aml_acc_update_bits(actrl, AUDIO_RSAMP_ACC_CTRL2, 0x3 << 16, 0x0 << 16);
		aml_acc_update_bits(actrl, AUDIO_RSAMP_ACC_CTRL1, 0x1 << 10, 0x0 << 10);
		phase_step *= (1 << 28);
	}

	phase_step = div_u64(phase_step, output_sample_rate);

	aml_acc_write(actrl, AUDIO_RSAMP_ACC_PHSSTEP, (u32)phase_step);

	aml_acc_update_bits(actrl, AUDIO_RSAMP_ACC_CTRL2, 0x3f << 24, channel << 24);

	aml_acc_update_bits(actrl, AUDIO_RSAMP_ACC_CTRL1, 0x1 << 11, 0x1 << 11);

	pr_info("%s(), phase_step = 0x%x, input_sr = %d, output_sr = %d",
		__func__, (u32)phase_step, input_sr, output_sr);
}

void acc_asrc_enable(struct aml_audio_controller *actrl, bool enable)
{
	aml_acc_update_bits(actrl, AUDIO_RSAMP_ACC_CTRL1, 0x1 << 24, (!enable) << 24);
}

