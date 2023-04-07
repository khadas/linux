/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __EFFECTS_ACC_H__
#define __EFFECTS_ACC_H__

enum acc_dest {
	TO_DDR,
	TO_TDMOUT_A,
	TO_TDMOUT_B,
	TO_SPDIFOUT_A,
};

int card_add_acc_kcontrols(struct snd_soc_card *card);

#endif
