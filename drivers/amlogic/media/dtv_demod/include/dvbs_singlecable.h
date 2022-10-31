/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __DVBS_SINGLECABLE_H__
#define __DVBS_SINGLECABLE_H__

enum SINGLECABLE_ADDRESS {
	/**< Any device (Master to all devices) */
	SINGLECABLE_ADDRESS_ALL_DEVICES            = 0x00,

	/**< Any LNB, Switcher or SMATV (Master to all...) */
	SINGLECABLE_ADDRESS_ALL_LNB_SMATV_SWITCHER = 0x10,

	/**< LNB devices. */
	SINGLECABLE_ADDRESS_LNB_DEVICE             = 0x11
};

enum SINGLECABLE_VERSION {
	SINGLECABLE_VERSION_1_EN50494 = 1,     /**< Single cable 1 (EN50494) */
	SINGLECABLE_VERSION_2_EN50607 = 2,     /**< Single cable 2 (EN50607) */
	SINGLECABLE_VERSION_UNKNOWN = 3
};

/**
 * bit[0] ~ bit[2].
 * bit[0] 0 - low band,   1 - high band.
 * bit[1] 0 - Vertical,   1 - Horizontal.
 * bit[2] 0 - position A, 1 - position B.
 */
enum SINGLECABLE_BANK {
	SINGLECABLE_BANK_0 = 0,    /**< Position A, Vertical,   Low  band. */
	SINGLECABLE_BANK_1 = 1,    /**< Position A, Vertical,   High band. */
	SINGLECABLE_BANK_2 = 2,    /**< Position A, Horizontal, Low  band. */
	SINGLECABLE_BANK_3 = 3,    /**< Position A, Horizontal, High band. */
	SINGLECABLE_BANK_4 = 4,    /**< Position B, Vertical,   Low  band. */
	SINGLECABLE_BANK_5 = 5,    /**< Position B, Vertical,   High band. */
	SINGLECABLE_BANK_6 = 6,    /**< Position B, Horizontal, Low  band. */
	SINGLECABLE_BANK_7 = 7     /**< Position B, Horizontal, High band. */
};

enum SINGLECABLE_SAT_POS {
	SINGLECABLE_SAT_POS_1,   /**< 1 Satellite Position */
	SINGLECABLE_SAT_POS_2    /**< 2 Satellite Positions */
};

enum SINGLECABLE_LNB_BAND {
	SINGLECABLE_LNB_BAND_L,   /**< Low  band */
	SINGLECABLE_LNB_BAND_H    /**< High band */
};

enum SINGLECABLE_POLAR {
	SINGLECABLE_POLAR_V,   /**< Vertical */
	SINGLECABLE_POLAR_H    /**< Horizontal */
};

enum SINGLECABLE_RF_BAND {
	SINGLECABLE_RF_BAND_STANDARD,  /**< Standard RF */
	SINGLECABLE_RF_BAND_WIDE       /**< Wide band RF */
};

enum SINGLECABLE_UB_SLOTS {
	SINGLECABLE_UB_SLOTS_2,   /**< 2 UB slots */
	SINGLECABLE_UB_SLOTS_4,   /**< 4 UB slots */
	SINGLECABLE_UB_SLOTS_6,   /**< 6 UB slots */
	SINGLECABLE_UB_SLOTS_8    /**< 8 UB slots */
};

enum SINGLECABLE_LOFREQ {
	SINGLECABLE_LOFREQ_NONE         = 0x00, /**< None (switcher) */
	SINGLECABLE_LOFREQ_UNKNOWN      = 0x01, /**< Unknown */
	SINGLECABLE_LOFREQ_9750_MHZ     = 0x02, /**< 9750MHz */
	SINGLECABLE_LOFREQ_10000_MHZ    = 0x03, /**< 10000MHz */
	SINGLECABLE_LOFREQ_10600_MHZ    = 0x04, /**< 10600MHz */
	SINGLECABLE_LOFREQ_10750_MHZ    = 0x05, /**< 10750MHz */
	SINGLECABLE_LOFREQ_11000_MHZ    = 0x06, /**< 11000MHz */
	SINGLECABLE_LOFREQ_11250_MHZ    = 0x07, /**< 11250MHz */
	SINGLECABLE_LOFREQ_11475_MHZ    = 0x08, /**< 11475MHz */
	SINGLECABLE_LOFREQ_20250_MHZ    = 0x09, /**< 20250MHz */
	SINGLECABLE_LOFREQ_5150_MHZ     = 0x0A, /**< 5150MHz */
	SINGLECABLE_LOFREQ_1585_MHZ     = 0x0B, /**< 1585MHz */
	SINGLECABLE_LOFREQ_13850_MHZ    = 0x0C, /**< 13850MHz */
	SINGLECABLE_LOFREQ_WB_NONE      = 0x10, /**< Wide band RF, None(switcher) */
	SINGLECABLE_LOFREQ_WB_10000_MHZ = 0x11, /**< Wide band RF, 10000MHz */
	SINGLECABLE_LOFREQ_WB_10200_MHZ = 0x12, /**< Wide band RF, 10200MHz */
	SINGLECABLE_LOFREQ_WB_13250_MHZ = 0x13, /**< Wide band RF, 13250MHz */
	SINGLECABLE_LOFREQ_WB_13450_MHZ = 0x14  /**< Wide band RF, 13450MHz */
};

struct SINGLECABLE_CONFIG_NB {
	enum SINGLECABLE_SAT_POS sat_pos;   /**< Number of Satellite Positions. */
	enum SINGLECABLE_RF_BAND rf_band;   /**< RF Band(Standard or Wide). */
	enum SINGLECABLE_UB_SLOTS ub_slots; /**< Number of User band (UB) slots. */
};

enum SINGLECABLE_BANK aml_singlecable_get_bank(enum SINGLECABLE_SAT_POS sat_pos,
		unsigned char polarity, unsigned char lnb_band);
int aml_singlecable_command_ODU_channel_change(struct dvb_diseqc_master_cmd *cmd,
		unsigned char address, unsigned char userband,
		unsigned int ub_slot_freq_khz, unsigned char bank,
		unsigned int center_freq_khz);
int aml_singlecable_command_ODU_poweroff(struct dvb_diseqc_master_cmd *cmd,
		unsigned char address, unsigned char userband);
int aml_singlecable_command_ODU_ubx_signal_on(struct dvb_diseqc_master_cmd *cmd,
		unsigned char address);
int aml_singlecable_command_ODU_config(struct dvb_diseqc_master_cmd *cmd,
		unsigned char address, unsigned char userband,
		enum SINGLECABLE_SAT_POS sat_pos, enum SINGLECABLE_RF_BAND rfband,
		enum SINGLECABLE_UB_SLOTS ub_slots);
int aml_singlecable_command_ODU_lofreq(struct dvb_diseqc_master_cmd *cmd,
		unsigned char address, unsigned char userband,
		enum SINGLECABLE_LOFREQ lofreq);
int aml_singlecable_command_ODU_channel_change_MDU(struct dvb_diseqc_master_cmd *cmd,
		unsigned char address, unsigned char userband,
		unsigned int ub_slot_freq_khz, unsigned char bank,
		unsigned int center_freq_khz, unsigned char pin);
int aml_singlecable_command_ODU_poweroff_MDU(struct dvb_diseqc_master_cmd *cmd,
		unsigned char address, unsigned char userband, unsigned char pin);
int aml_singlecable_command_ODU_ubx_signal_on_MDU(struct dvb_diseqc_master_cmd *cmd,
		unsigned char address, unsigned char pin);
int aml_singlecable_command_ODU_config_MDU(struct dvb_diseqc_master_cmd *cmd,
		unsigned char address, unsigned char userband,
		enum SINGLECABLE_SAT_POS sat_pos, enum SINGLECABLE_RF_BAND rfband,
		enum SINGLECABLE_UB_SLOTS ub_slots, unsigned char pin);
int aml_singlecable_command_ODU_lofreq_MDU(struct dvb_diseqc_master_cmd *cmd,
		unsigned char address, unsigned char userband,
		enum SINGLECABLE_LOFREQ lofreq, unsigned char pin);
int aml_singlecable2_command_ODU_channel_change(struct dvb_diseqc_master_cmd *cmd,
		unsigned char userband, unsigned int if_freq_mhz,
		unsigned char uncommitted_switches,
		unsigned char committed_switches);
int aml_singlecable2_command_ODU_channel_change_pin(struct dvb_diseqc_master_cmd *cmd,
		unsigned char userband, unsigned int if_freq_mhz,
		unsigned char uncommitted_switches,
		unsigned char committed_switches, unsigned char pin);
int aml_singlecable2_command_ODU_poweroff(struct dvb_diseqc_master_cmd *cmd,
		unsigned char userband);
int aml_singlecable2_command_ODU_poweroff_pin(struct dvb_diseqc_master_cmd *cmd,
		unsigned char userband, unsigned char pin);
int aml_singlecable2_command_ODU_UB_avail(struct dvb_diseqc_master_cmd *cmd);
int aml_singlecable2_command_ODU_UB_freq(struct dvb_diseqc_master_cmd *cmd,
		unsigned char userband);
int aml_singlecable2_command_ODU_UB_inuse(struct dvb_diseqc_master_cmd *cmd);
int aml_singlecable2_command_ODU_UB_pin(struct dvb_diseqc_master_cmd *cmd);
int aml_singlecable2_command_ODU_UB_switches(struct dvb_diseqc_master_cmd *cmd,
		unsigned char userband);

#endif /* __DVBS_SINGLECABLE_H__ */
