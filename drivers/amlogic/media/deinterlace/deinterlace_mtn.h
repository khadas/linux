/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _DI_MTN_H
#define _DI_MTN_H
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/frame_provider/tvin/tvin.h>
struct combing_param_s {
	unsigned int width;
	unsigned int height;
	unsigned int field_idx;
	enum vframe_source_type_e src_type;
	enum tvin_sig_fmt_e fmt;
	bool prog_flag;
};

struct combing_status_s {
	unsigned int frame_diff_avg;
	unsigned int cmb_row_num;
	unsigned int field_diff_rate;
	int like_pulldown22_flag;
	unsigned int cur_level;
};

struct combing_status_s *adpative_combing_config(unsigned int width,
	unsigned int height,
	enum vframe_source_type_e src_type, bool prog,
	enum tvin_sig_fmt_e fmt);
void fix_tl1_1080i_patch_sel(unsigned int mode);
int adaptive_combing_fixing(
	struct combing_status_s *cmb_status,
	unsigned int field_diff, unsigned int frame_diff,
	int bit_mode);
void adpative_combing_exit(void);
extern void mtn_int_combing_glbmot(void);
void com_patch_pre_sw_set(unsigned int mode);

#endif
