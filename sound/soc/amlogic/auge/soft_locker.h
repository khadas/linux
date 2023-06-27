/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef SOFT_LOCKER_H
#define SOFT_LOCKER_H

#include "ddr_mngr.h"

struct buf_config {
	int buf_frames;
	int frame_size;
	int sample_rate;
};

struct ddr_status {
	unsigned int buf_frames;
	unsigned int frame_size;
	unsigned int sample_rate;

	unsigned int wrap_cnt;
	int start_pos;
	int last_pos;
};

#define LOCKER_DEVICE_MAX 8
struct soft_locker {
	int devin_id;
	struct toddr *to;
	struct ddr_status to_status;
	int devout_id;
	struct frddr *fr;
	struct ddr_status fr_status;
	struct timer_list timer;
	int diff_ms;
	bool en;
	bool reset;
	bool update_ddr;
	const char *dais_name[LOCKER_DEVICE_MAX];
	struct toddr *src_tddrs[LOCKER_DEVICE_MAX];
	struct frddr *sink_fddrs[LOCKER_DEVICE_MAX];
};

const char *locker_id_to_dai_name(struct soft_locker *locker, int dev_num);

void locker_register_frddr(struct soft_locker *locker,
		struct frddr *fr, const char *name);
void locker_register_toddr(struct soft_locker *locker,
		struct toddr *to, const char *name);
void locker_release_frddr(struct soft_locker *locker, const char *name);
void locker_release_toddr(struct soft_locker *locker, const char *name);

int card_add_locker_kcontrols(struct snd_soc_card *card);

int locker_reset(struct soft_locker *locker);
int locker_update_ddr_en(struct soft_locker *locker);
int locker_add_dai_name(struct soft_locker *locker, int dev_num, const char *name);
int locker_en_ddr_by_dai_name(struct soft_locker *locker,
		const char *dai_name, int input);

#endif /* SOFT_LOCKER_H */
