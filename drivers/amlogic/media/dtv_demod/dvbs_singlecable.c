// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <uapi/linux/dvb/frontend.h>
#include "dvbs_singlecable.h"

static int add_pin_code(struct dvb_diseqc_master_cmd *cmd, unsigned char pin);

enum SINGLECABLE_BANK aml_singlecable_get_bank(enum SINGLECABLE_SAT_POS sat_pos,
		unsigned char polarity, unsigned char lnb_band)
{
	enum SINGLECABLE_BANK bank = 0;

	if (sat_pos == SINGLECABLE_SAT_POS_1) {
		if (polarity == SINGLECABLE_POLAR_V) {
			if (lnb_band == SINGLECABLE_LNB_BAND_L)
				bank = SINGLECABLE_BANK_0;
			else
				bank = SINGLECABLE_BANK_1;
		} else {
			if (lnb_band == SINGLECABLE_LNB_BAND_L)
				bank = SINGLECABLE_BANK_2;
			else
				bank = SINGLECABLE_BANK_3;
		}
	} else {
		if (polarity == SINGLECABLE_POLAR_V) {
			if (lnb_band == SINGLECABLE_LNB_BAND_L)
				bank = SINGLECABLE_BANK_4;
			else
				bank = SINGLECABLE_BANK_5;
		} else {
			if (lnb_band == SINGLECABLE_LNB_BAND_L)
				bank = SINGLECABLE_BANK_6;
			else
				bank = SINGLECABLE_BANK_7;
		}
	}

	return bank;
}

int aml_singlecable_command_ODU_channel_change(struct dvb_diseqc_master_cmd *cmd,
		unsigned char address, unsigned char userband,
		unsigned int ub_slot_freq_khz, unsigned char bank,
		unsigned int center_freq_khz)
{
	unsigned short T = 0;
	unsigned char data = 0;

	if (!cmd)
		return -ENXIO;

	if (userband <= 0 || 9 <= userband || bank >= 8)
		return -EINVAL;

	/* Calculation T */
	T = (unsigned short)(((center_freq_khz + ub_slot_freq_khz + 2000) / 4000) - 350);

	if (T > 0x3FFF)
		return -EINVAL;

	cmd->msg[0] = 0xE0;
	cmd->msg[1] = (unsigned char)address;
	cmd->msg[2] = 0x5A;

	data = (unsigned char)(((unsigned int)(userband - 1) & 0x07) << 5);
	data |= (unsigned char)(((unsigned int)bank & 0x07) << 2);
	data |= (unsigned char)((T >> 8) & 0x03);
	cmd->msg[3] = data;

	cmd->msg[4] = (unsigned char)(T & 0xFF);
	cmd->msg_len = 5;

	return 0;
}

int aml_singlecable_command_ODU_poweroff(struct dvb_diseqc_master_cmd *cmd,
		unsigned char address, unsigned char userband)
{
	if (!cmd)
		return -ENXIO;

	if (userband <= 0 || 9 <= userband)
		return -EINVAL;

	cmd->msg[0] = 0xE0;
	cmd->msg[1] = (unsigned char)address;
	cmd->msg[2] = 0x5A;
	cmd->msg[3] = (unsigned char)((((unsigned int)(userband - 1)) & 0x07) << 5);
	cmd->msg[4] = 0x00;
	cmd->msg_len = 5;

	return 0;
}

int aml_singlecable_command_ODU_ubx_signal_on(struct dvb_diseqc_master_cmd *cmd,
		unsigned char address)
{
	if (!cmd)
		return -ENXIO;

	cmd->msg[0] = 0xE0;
	cmd->msg[1] = (unsigned char)address;
	cmd->msg[2] = 0x5B;
	cmd->msg[3] = 0x00;
	cmd->msg[4] = 0x00;
	cmd->msg_len = 5;

	return 0;
}

int aml_singlecable_command_ODU_config(struct dvb_diseqc_master_cmd *cmd,
		unsigned char address, unsigned char userband,
		enum SINGLECABLE_SAT_POS sat_pos,
		enum SINGLECABLE_RF_BAND rfband,
		enum SINGLECABLE_UB_SLOTS ub_slots)
{
	unsigned int index = 0;
	static const struct
	{
		enum SINGLECABLE_SAT_POS sat_pos;
		enum SINGLECABLE_RF_BAND rfband;
		enum SINGLECABLE_UB_SLOTS ub_slots;
		unsigned char Config_Nb;
	} ODU_config_table[] = {
	{ SINGLECABLE_SAT_POS_1, SINGLECABLE_RF_BAND_STANDARD,
			SINGLECABLE_UB_SLOTS_2, 0x10 },
	{ SINGLECABLE_SAT_POS_1, SINGLECABLE_RF_BAND_STANDARD,
			SINGLECABLE_UB_SLOTS_4, 0x11 },
	{ SINGLECABLE_SAT_POS_1, SINGLECABLE_RF_BAND_STANDARD,
			SINGLECABLE_UB_SLOTS_6, 0x12 },
	{ SINGLECABLE_SAT_POS_1, SINGLECABLE_RF_BAND_STANDARD,
			SINGLECABLE_UB_SLOTS_8, 0x13 },
	{ SINGLECABLE_SAT_POS_1, SINGLECABLE_RF_BAND_WIDE,
			SINGLECABLE_UB_SLOTS_2, 0x14 },
	{ SINGLECABLE_SAT_POS_1, SINGLECABLE_RF_BAND_WIDE,
			SINGLECABLE_UB_SLOTS_4, 0x15 },
	{ SINGLECABLE_SAT_POS_1, SINGLECABLE_RF_BAND_WIDE,
			SINGLECABLE_UB_SLOTS_6, 0x16 },
	{ SINGLECABLE_SAT_POS_1, SINGLECABLE_RF_BAND_WIDE,
			SINGLECABLE_UB_SLOTS_8, 0x17 },
	{ SINGLECABLE_SAT_POS_2, SINGLECABLE_RF_BAND_STANDARD,
			SINGLECABLE_UB_SLOTS_2, 0x18 },
	{ SINGLECABLE_SAT_POS_2, SINGLECABLE_RF_BAND_STANDARD,
			SINGLECABLE_UB_SLOTS_4, 0x19 },
	{ SINGLECABLE_SAT_POS_2, SINGLECABLE_RF_BAND_STANDARD,
			SINGLECABLE_UB_SLOTS_6, 0x1A },
	{ SINGLECABLE_SAT_POS_2, SINGLECABLE_RF_BAND_STANDARD,
			SINGLECABLE_UB_SLOTS_8, 0x1B },
	{ SINGLECABLE_SAT_POS_2, SINGLECABLE_RF_BAND_WIDE,
			SINGLECABLE_UB_SLOTS_2, 0x1C },
	{ SINGLECABLE_SAT_POS_2, SINGLECABLE_RF_BAND_WIDE,
			SINGLECABLE_UB_SLOTS_4, 0x1D },
	{ SINGLECABLE_SAT_POS_2, SINGLECABLE_RF_BAND_WIDE,
			SINGLECABLE_UB_SLOTS_6, 0x1E },
	{ SINGLECABLE_SAT_POS_2, SINGLECABLE_RF_BAND_WIDE,
			SINGLECABLE_UB_SLOTS_8, 0x1F } };

	if (!cmd)
		return -ENXIO;

	if (userband <= 0 || 9 <= userband)
		return -EINVAL;

	cmd->msg[0] = 0xE0;
	cmd->msg[1] = (unsigned char)address;
	cmd->msg[2] = 0x5B;
	cmd->msg[3] = (unsigned char)(((((unsigned int)(userband - 1)) & 0x07) << 5) | 0x01);

	for (index = 0; index < (unsigned int)(sizeof(ODU_config_table)
							/ sizeof(ODU_config_table[0])); index++) {
		if (sat_pos == ODU_config_table[index].sat_pos &&
			rfband == ODU_config_table[index].rfband &&
			ub_slots == ODU_config_table[index].ub_slots) {
			cmd->msg[4] = ODU_config_table[index].Config_Nb;
			cmd->msg_len = 5;
			/* Success */
			return 0;
		}
	}

	/* Error */
	return -ENXIO;
}

int aml_singlecable_command_ODU_lofreq(struct dvb_diseqc_master_cmd *cmd,
		unsigned char address, unsigned char userband,
		enum SINGLECABLE_LOFREQ lofreq)
{
	if (!cmd)
		return -ENXIO;

	if (userband <= 0 || 9 <= userband)
		return -EINVAL;

	cmd->msg[0] = 0xE0;
	cmd->msg[1] = (unsigned char)address;
	cmd->msg[2] = 0x5B;
	cmd->msg[3] = (unsigned char)(((((unsigned int)(userband - 1)) & 0x07) << 5) | 0x02);
	cmd->msg[4] = (unsigned char)lofreq;
	cmd->msg_len = 5;

	return 0;
}

int aml_singlecable_command_ODU_channel_change_MDU(struct dvb_diseqc_master_cmd *cmd,
		unsigned char address, unsigned char userband,
		unsigned int ub_slot_freq_khz, unsigned char bank,
		unsigned int center_freq_khz, unsigned char pin)
{
	int ret = 0;

	if (!cmd)
		return -ENXIO;

	if (userband <= 0 || 9 <= userband)
		return -EINVAL;

	ret = aml_singlecable_command_ODU_channel_change(cmd, address,
			userband, ub_slot_freq_khz, bank, center_freq_khz);
	if (!ret)
		return ret;

	cmd->msg[2] = 0x5C;

	return add_pin_code(cmd, pin);
}

int aml_singlecable_command_ODU_poweroff_MDU(struct dvb_diseqc_master_cmd *cmd,
		unsigned char address, unsigned char userband, unsigned char pin)
{
	int ret = 0;

	if (!cmd)
		return -ENXIO;

	if (userband <= 0 || 9 <= userband)
		return -EINVAL;

	ret = aml_singlecable_command_ODU_poweroff(cmd, address, userband);
	if (!ret)
		return ret;

	cmd->msg[2] = 0x5C;

	return add_pin_code(cmd, pin);
}

int aml_singlecable_command_ODU_ubx_signal_on_MDU(struct dvb_diseqc_master_cmd *cmd,
		unsigned char address, unsigned char pin)
{
	int ret = 0;

	if (!cmd)
		return -ENXIO;

	ret = aml_singlecable_command_ODU_ubx_signal_on(cmd, address);
	if (!ret)
		return ret;

	cmd->msg[2] = 0x5D;

	return add_pin_code(cmd, pin);
}

int aml_singlecable_command_ODU_config_MDU(struct dvb_diseqc_master_cmd *cmd,
		unsigned char address, unsigned char userband,
		enum SINGLECABLE_SAT_POS sat_pos,
		enum SINGLECABLE_RF_BAND rfband,
		enum SINGLECABLE_UB_SLOTS ub_slots, unsigned char pin)
{
	int ret = 0;

	if (!cmd)
		return -ENXIO;

	if (userband <= 0 || 9 <= userband)
		return -EINVAL;

	ret = aml_singlecable_command_ODU_config(cmd, address, userband,
			sat_pos, rfband, ub_slots);
	if (!ret)
		return ret;

	cmd->msg[2] = 0x5D;

	return add_pin_code(cmd, pin);
}

int aml_singlecable_command_ODU_lofreq_MDU(struct dvb_diseqc_master_cmd *cmd,
		unsigned char address, unsigned char userband,
		enum SINGLECABLE_LOFREQ lofreq, unsigned char pin)
{
	int ret = 0;

	if (!cmd)
		return -ENXIO;

	if (userband <= 0 || 9 <= userband)
		return -EINVAL;

	ret = aml_singlecable_command_ODU_lofreq(cmd, address, userband, lofreq);
	if (!ret)
		return ret;

	cmd->msg[2] = 0x5D;

	return add_pin_code(cmd, pin);
}

int aml_singlecable2_command_ODU_channel_change(struct dvb_diseqc_master_cmd *cmd,
		unsigned char userband, unsigned int if_freq_mhz,
		unsigned char uncommitted_switches,
		unsigned char committed_switches)
{
	unsigned short T = 0;

	if (!cmd)
		return -ENXIO;

	if (userband < 1 || userband > 32 ||
		uncommitted_switches > 0x0F || committed_switches > 0x0F)
		return -EINVAL;

	if (if_freq_mhz <= 9) {
		/* Special value */
		T = (unsigned short)if_freq_mhz;
	} else {
		/* Calculation T */
		T = (unsigned short)(if_freq_mhz - 100);
	}

	if (T > 0x7FF)
		return -EINVAL;

	cmd->msg[0] = 0x70;
	cmd->msg[1] = (unsigned char)(((userband - 1) << 3) | ((T >> 8) & 0x07));
	cmd->msg[2] = (unsigned char)(T & 0xFF);
	cmd->msg[3] = (unsigned char)((uncommitted_switches << 4)
			| (committed_switches));
	cmd->msg_len = 4;

	return 0;
}

int aml_singlecable2_command_ODU_channel_change_pin(struct dvb_diseqc_master_cmd *cmd,
		unsigned char userband, unsigned int if_freq_mhz,
		unsigned char uncommitted_switches,
		unsigned char committed_switches, unsigned char pin)
{
	int ret = 0;

	if (!cmd)
		return -ENXIO;

	if (userband < 1 || userband > 32)
		return -EINVAL;

	ret = aml_singlecable2_command_ODU_channel_change(cmd, userband,
			if_freq_mhz, uncommitted_switches, committed_switches);
	if (!ret)
		return ret;

	/* Overwrite command ID */
	cmd->msg[0] = 0x71;

	return add_pin_code(cmd, pin);
}

int aml_singlecable2_command_ODU_poweroff(struct dvb_diseqc_master_cmd *cmd,
		unsigned char userband)
{
	if (!cmd)
		return -ENXIO;

	if (userband < 1 || userband > 32)
		return -EINVAL;

	return aml_singlecable2_command_ODU_channel_change(cmd, userband,
			0, 0, 0);
}

int aml_singlecable2_command_ODU_poweroff_pin(struct dvb_diseqc_master_cmd *cmd,
		unsigned char userband, unsigned char pin)
{
	int ret = 0;

	if (!cmd)
		return -ENXIO;

	if (userband < 1 || userband > 32)
		return -EINVAL;

	ret = aml_singlecable2_command_ODU_channel_change_pin(cmd,
			userband, 0, 0, 0, pin);
	if (!ret)
		return ret;

	return add_pin_code(cmd, pin);
}

int aml_singlecable2_command_ODU_UB_avail(struct dvb_diseqc_master_cmd *cmd)
{
	if (!cmd)
		return -ENXIO;

	cmd->msg[0] = 0x7A;
	cmd->msg_len = 1;

	return 0;
}

int aml_singlecable2_command_ODU_UB_freq(struct dvb_diseqc_master_cmd *cmd,
		unsigned char userband)
{
	if (!cmd)
		return -ENXIO;

	if (userband < 1 || userband > 32)
		return -EINVAL;

	cmd->msg[0] = 0x7D;
	cmd->msg[1] = (unsigned char)(((userband - 1) & 0x1F) << 3);
	cmd->msg_len = 2;

	return 0;
}

int aml_singlecable2_command_ODU_UB_inuse(struct dvb_diseqc_master_cmd *cmd)
{
	if (!cmd)
		return -ENXIO;

	cmd->msg[0] = 0x7C;
	cmd->msg_len = 1;

	return 0;
}

int aml_singlecable2_command_ODU_UB_pin(struct dvb_diseqc_master_cmd *cmd)
{
	if (!cmd)
		return -ENXIO;

	cmd->msg[0] = 0x7B;
	cmd->msg_len = 1;

	return 0;
}

int aml_singlecable2_command_ODU_UB_switches(struct dvb_diseqc_master_cmd *cmd,
		unsigned char userband)
{
	if (!cmd)
		return -ENXIO;

	if (userband < 1 || userband > 32)
		return -EINVAL;

	cmd->msg[0] = 0x7E;
	cmd->msg[1] = (unsigned char)(((userband - 1) & 0x1F) << 3);
	cmd->msg_len = 2;

	return 0;
}

static int add_pin_code(struct dvb_diseqc_master_cmd *cmd, unsigned char pin)
{
	int ret = 0;

	if (!cmd)
		return -ENXIO;

	if (cmd->msg_len < ARRAY_SIZE(cmd->msg)) {
		cmd->msg[cmd->msg_len] = pin;
		cmd->msg_len++;
	} else {
		return -ENXIO;
	}

	return ret;
}
