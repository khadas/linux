// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/kernel.h>

#include "regs.h"
#include "iomap.h"
#include "acc_hw_eq.h"

void acc_eq_init_ram_coeff(struct aml_audio_controller *actrl,
			int ram_count, int addr, int len, unsigned int *params)
{
	int i, ctrl_v;
	unsigned int *p = params;

	for (i = 0; i < len; i++, p++) {
		ctrl_v = ((addr + i) << 2) | (0x1 << 1) | (0x1 << 0);
		aml_acc_write(actrl, AEQ_COEF_DATA_A, *p);
		aml_acc_write(actrl, AEQ_COEF_CNTL_A, ctrl_v);
	}

	/* init RAMB */
	if (ram_count == 2) {
		p = params;
		for (i = 0; i < len; i++, p++) {
			ctrl_v = ((addr + i) << 2) | (0x1 << 1) | (0x1 << 0);
			aml_acc_write(actrl, AEQ_COEF_DATA_B, *p);
			aml_acc_write(actrl, AEQ_COEF_CNTL_B, ctrl_v);
		}
	}
}

void acc_eq_set_ram_coeff(struct aml_audio_controller *actrl,
			int ram_count, int addr, int len, unsigned int *params)
{
	int i, ctrl_v;
	unsigned int *p = params, value = 0;

	if (ram_count == 2) {
		/*
		 * dynamic control
		 * step 1. write params to unselected ram
		 * step 2. switch  to unselected ram
		 * step 3. write params to the other ram
		 */

		/*
		 * AEQ_TOP_CTRL bit 0
		 * coef ram sel: 0 select rama coef; 1 select ramb coef
		 */
		value = aml_acc_read(actrl, AEQ_TOP_CTRL);
		value &= 0x1;

		if (value) {
			for (i = 0; i < len; i++, p++) {
				ctrl_v = ((addr + i) << 2)
					| (0x1 << 1)
					| (0x1 << 0);
				aml_acc_write(actrl, AEQ_COEF_DATA_A, *p);
				aml_acc_write(actrl, AEQ_COEF_CNTL_A, ctrl_v);
			}
		} else {
			for (i = 0; i < len; i++, p++) {
				ctrl_v = ((addr + i) << 2)
					| (0x1 << 1)
					| (0x1 << 0);
				aml_acc_write(actrl, AEQ_COEF_DATA_B, *p);
				aml_acc_write(actrl, AEQ_COEF_CNTL_B, ctrl_v);
			}
		}

		aml_acc_update_bits(actrl, AEQ_TOP_CTRL,
				  0x1 << 0,
				  !value << 0);

		p = params;
		if (value) {
			for (i = 0; i < len; i++, p++) {
				ctrl_v = ((addr + i) << 2)
					| (0x1 << 1)
					| (0x1 << 0);
				aml_acc_write(actrl, AEQ_COEF_DATA_B, *p);
				aml_acc_write(actrl, AEQ_COEF_CNTL_B, ctrl_v);
			}
		} else {
			for (i = 0; i < len; i++, p++) {
				ctrl_v = ((addr + i) << 2)
					| (0x1 << 1)
					| (0x1 << 0);
				aml_acc_write(actrl, AEQ_COEF_DATA_A, *p);
				aml_acc_write(actrl, AEQ_COEF_CNTL_A, ctrl_v);
			}
		}

	} else {
		for (i = 0; i < len; i++, p++) {
			ctrl_v = ((addr + i) << 2) | (0x1 << 1) | (0x1 << 0);
			aml_acc_write(actrl, AEQ_COEF_DATA_A, *p);
			aml_acc_write(actrl, AEQ_COEF_CNTL_A, ctrl_v);
		}
	}
}

void acc_eq_get_ram_coeff(struct aml_audio_controller *actrl,
			int ram_count, int addr, int len, unsigned int *params)
{
	int i, ctrl_v;
	unsigned int *p = params, value = 0;

	if (ram_count == 2) {
		value = aml_acc_read(actrl, AEQ_TOP_CTRL);
		value &= 0x1;

		if (value) {
			for (i = 0; i < len; i++, p++) {
				ctrl_v = ((addr + i) << 2) | (0x0 << 1) | (0x1 << 0);
				aml_acc_write(actrl, AEQ_COEF_CNTL_A, ctrl_v);
				*p = aml_acc_read(actrl, AEQ_COEF_DATA_A);
			}
		} else {
			for (i = 0; i < len; i++, p++) {
				ctrl_v = ((addr + i) << 2) | (0x0 << 1) | (0x1 << 0);
				aml_acc_write(actrl, AEQ_COEF_CNTL_B, ctrl_v);
				*p = aml_acc_read(actrl, AEQ_COEF_DATA_B);
			}
		}
	} else {
		for (i = 0; i < len; i++, p++) {
			ctrl_v = ((addr + i) << 2) | (0x0 << 1) | (0x1 << 0);
			aml_acc_write(actrl, AEQ_COEF_CNTL_A, ctrl_v);
			*p = aml_acc_read(actrl, AEQ_COEF_DATA_A);
		}
	}
	/* pr_info("%s, params[%d] = %8.8x\n", __func__, i, *p); */
}

void acc_eq_clk_enable(struct aml_audio_controller *actrl, bool enable)
{
	/* fixed sys clk and core clk enable */
	aml_acc_update_bits(actrl, AUDIO_ACC_CLK_GATE_EN, 0x3 << 2, enable << 3 | enable << 2);
}

void acc_eq_set_wrapper(struct aml_audio_controller *actrl)
{
	/* config eq wrapper */
	aml_acc_update_bits(actrl, AUDIO_ACC_EQDRC_WRAPPER_TOP_CTL0,
			0x1 << 31 | 0x7 << 11 | 0x1f << 6 | 0x7 << 3 | 0x7,
			0 << 31 | //reg_start_en       [31]
			4 << 11 | //reg_data_type      [13:11]
			31 << 6 | //reg_data_msb       [10:6]
			1 << 3  | //reg_frddr_source   [5:3]
			1 << 2 | //reg_afifo_in_rst   [2]
			1 << 1 | //reg_afifo_out_rst  [1]
			0 << 0   //reg_eqdrc_en       [0]
			);
	aml_acc_update_bits(actrl, AUDIO_ACC_EQDRC_WRAPPER_TOP_CTL1, 0xf << 16 | 0xffff,
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
	aml_acc_update_bits(actrl, AUDIO_ACC_EQDRC_WRAPPER_TOP_CTL2,
			0x7 | 0x1 << 3 | 0x7 << 4 | 0x1 << 7 | 0x7 << 8 | 0x1 << 11 | 0xff << 12,
			/* reg_req_sel0       <= pwdata_aed[2:0]
			 * 0: toddr, 1: asrc, 2: tdmout_a, 3: tdmout_b, 4: spdifout
			 */
			1 << 0  |
			1 << 3  | //  reg_req_sel0_en    <= pwdata_aed[3]
			0 << 4  | //  reg_req_sel1       <= pwdata_aed[6:4]
			0 << 7  | //  reg_req_sel1_en    <= pwdata_aed[7]
			0 << 8  | //  reg_req_sel2       <= pwdata_aed[10:8]
			0 << 11 | //  reg_req_sel2_en    <= pwdata_aed[11]
			0 << 12   //  reg_ack_num        <= pwdata_aed[19:12]
			);
	aml_acc_write(actrl, AUDIO_ACC_EQDRC_WRAPPER_TOP_CTL3,
			7 << 28 | 6 << 24 | 5 << 20 | 4 << 16 |
			3 << 12 | 2 << 8  | 1 << 4  | 0 << 0);
	aml_acc_write(actrl, AUDIO_ACC_EQDRC_WRAPPER_TOP_CTL4,
			15 << 28 | 14 << 24 | 13 << 20 | 12 << 16 |
			11 << 12 | 10 << 8  | 9 << 4 | 8 << 0);
	aml_acc_update_bits(actrl, AUDIO_ACC_EQDRC_WRAPPER_TOP_CTL0,
			1 << 31 | 1 << 0, 1 << 31 | 1 << 0);

	/* set EQ tap to max 20 bands*/
	aml_acc_update_bits(actrl, AEQ_STATUS0_CTRL, 0x1f << 2, 0x14 << 2);
}

void acc_eq_enable(struct aml_audio_controller *actrl, bool enable)
{
	aml_acc_update_bits(actrl, AEQ_STATUS0_CTRL, 0x1 << 1, enable << 1);
}

