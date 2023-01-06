/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __ISDBT_FUNC_H__
#define __ISDBT_FUNC_H__

enum isdbt_system {
	ISDBT_SYSTEM_ISDB_T,     /* ISDB for Terrestrial Television Broadcasting */
	ISDBT_SYSTEM_ISDB_TSB,   /* ISDB for Terrestrial Sound Broadcasting */
	ISDBT_SYSTEM_RESERVED_2, /* reserved by specification (10) */
	ISDBT_SYSTEM_RESERVED_3  /* reserved by specification (11) */
};

enum isdbt_modulation {
	ISDBT_MODULATION_DQPSK,      /* DQPSK */
	ISDBT_MODULATION_QPSK,       /* QPSK  */
	ISDBT_MODULATION_16QAM,      /* 16QAM */
	ISDBT_MODULATION_64QAM,      /* 64QAM */
	ISDBT_MODULATION_RESERVED_4, /* reserved by specification (100) */
	ISDBT_MODULATION_RESERVED_5, /* reserved by specification (101) */
	ISDBT_MODULATION_RESERVED_6, /* reserved by specification (110) */
	ISDBT_MODULATION_UNUSED_7    /* unused (111) */
};

enum isdbt_coding_rate {
	ISDBT_CODING_RATE_1_2,        /* Code Rate : 1/2 */
	ISDBT_CODING_RATE_2_3,        /* Code Rate : 2/3 */
	ISDBT_CODING_RATE_3_4,        /* Code Rate : 3/4 */
	ISDBT_CODING_RATE_5_6,        /* Code Rate : 5/6 */
	ISDBT_CODING_RATE_7_8,        /* Code Rate : 7/8 */
	ISDBT_CODING_RATE_RESERVED_5, /* reserved by specification (101) */
	ISDBT_CODING_RATE_RESERVED_6, /* reserved by specification (110) */
	ISDBT_CODING_RATE_UNUSED_7    /* unused (111) */
};

enum isdbt_il_length {
	ISDBT_IL_LENGTH_0_0_0,      /* Mode1: 0, Mode2: 0, Mode3: 0 */
	ISDBT_IL_LENGTH_4_2_1,      /* Mode1: 4, Mode2: 2, Mode3: 1 */
	ISDBT_IL_LENGTH_8_4_2,      /* Mode1: 8, Mode2: 4, Mode3: 2 */
	ISDBT_IL_LENGTH_16_8_4,     /* Mode1:16, Mode2: 8, Mode3: 4 */
	ISDBT_IL_LENGTH_RESERVED_4, /* reserved by specification (100) */
	ISDBT_IL_LENGTH_RESERVED_5, /* reserved by specification (101) */
	ISDBT_IL_LENGTH_RESERVED_6, /* reserved by specification (110) */
	ISDBT_IL_LENGTH_UNUSED_7    /* unused (111) */
};

struct isdbt_tmcc_layer_info {
	enum isdbt_modulation modulation;   /* Modulation */
	enum isdbt_coding_rate coding_rate; /* Code rate */
	enum isdbt_il_length il_length;     /* Interleave length */
	unsigned char segments_num;         /* 1-13:Seguments number, 0,14:Reserved, 15:Unused */
};

struct isdbt_tmcc_group_param {
	unsigned char is_partial; /* 0:No partial reception, 1:Partial reception available */
	struct isdbt_tmcc_layer_info layer_a; /* Information of layer A */
	struct isdbt_tmcc_layer_info layer_b; /* Information of layer B */
	struct isdbt_tmcc_layer_info layer_c; /* Information of layer C */
};

struct isdbt_tmcc_info {
	enum isdbt_system system_id; /* ISDB-T system identification */
	unsigned char tp_switch;     /* Indicator of transmission-parameter switching */
	unsigned char ews_flag;      /* Start flag for emergency-alarm broadcasting */
	struct isdbt_tmcc_group_param current_info; /* Current information */
	struct isdbt_tmcc_group_param next_info;    /* Next information */

	/* Used for digital terrestrial sound broadcasting or
	 * terrestrial multimedia broadcasting
	 */
	unsigned char phase_shift;
	unsigned short reserved;
};

void isdbt_get_tmcc_info(struct isdbt_tmcc_info *tmcc_info);

#endif /* __ISDBT_FUNC_H__ */
