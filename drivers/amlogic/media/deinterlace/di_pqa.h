/*
 * drivers/amlogic/media/deinterlace/di_pqa.h
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#ifndef __DI_PQA_H__
#define __DI_PQA_H__

#include <linux/amlogic/media/frame_provider/tvin/tvin.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/device.h>
#include "../deinterlace/pulldown_drv.h"
#include "../deinterlace/deinterlace_mtn.h"
#if 0 /*move from pulldown_drv.h to di_pq.h*/
enum pulldown_mode_e {
	PULL_DOWN_BLEND_0	= 0,/* buf1=dup[0] */
	PULL_DOWN_BLEND_2	= 1,/* buf1=dup[2] */
	PULL_DOWN_MTN		= 2,/* mtn only */
	PULL_DOWN_BUF1		= 3,/* do wave with dup[0] */
	PULL_DOWN_EI		= 4,/* ei only */
	PULL_DOWN_NORMAL	= 5,/* normal di */
	PULL_DOWN_NORMAL_2	= 6,/* di */
};

struct pulldown_vof_win_s {
	unsigned short win_vs;
	unsigned short win_ve;
	enum pulldown_mode_e blend_mode;
};

struct pulldown_detected_s {
	enum pulldown_mode_e global_mode;
	struct pulldown_vof_win_s regs[4];
};
#endif

#if 0	/*move from deinterlace_mtn.h*/
struct combing_status_s {
	unsigned int frame_diff_avg;
	unsigned int cmb_row_num;
	unsigned int field_diff_rate;
	int like_pulldown22_flag;
	unsigned int cur_level;
};
#endif
/**************************
 * pulldown
 *************************/
struct pulldown_op_s {
	unsigned char (*init)(unsigned short width, unsigned short height);
	unsigned int (*detection)(struct pulldown_detected_s *res,
				  struct combing_status_s *cmb_sts,
				  bool reverse,
				  struct vframe_s *vf);
	void (*vof_win_vshift)(struct pulldown_detected_s *wins,
			       unsigned short v_offset);
	int (*module_para)(struct seq_file *seq);
	void (*prob)(struct device *dev);
	void (*remove)(struct device *dev);
};

bool di_attach_ops_pulldown(const struct pulldown_op_s **ops);

/**************************
 * detect3d
 *************************/
struct detect3d_op_s {
	void (*det3d_config)(bool flag);
	enum tvin_trans_fmt (*det3d_fmt_detect)(void);
	int (*module_para)(struct seq_file *seq);
};

bool di_attach_ops_3d(const struct detect3d_op_s **ops);

/**************************
 * nr_drv
 *************************/
struct nr_op_s {
	void (*nr_hw_init)(void);
	void (*nr_gate_control)(bool gate);
	void (*nr_drv_init)(struct device *dev);
	void (*nr_drv_uninit)(struct device *dev);
	void (*nr_process_in_irq)(void);
	void (*nr_all_config)(unsigned short nCol, unsigned short nRow,
			      unsigned short type);
	bool (*set_nr_ctrl_reg_table)(unsigned int addr, unsigned int value);
	void (*cue_int)(struct vframe_s *vf);
	void (*adaptive_cue_adjust)(unsigned int frame_diff,
				    unsigned int field_diff);
	int (*module_para)(struct seq_file *seq);

};

bool di_attach_ops_nr(const struct nr_op_s **ops);

/**************************
 * deinterlace_mtn
 *************************/
struct mtn_op_s {
	void (*mtn_int_combing_glbmot)(void);
	void (*adpative_combing_exit)(void);
	void (*fix_tl1_1080i_patch_sel)(unsigned int mode);
	int (*adaptive_combing_fixing)(
		struct combing_status_s *cmb_status,
		unsigned int field_diff, unsigned int frame_diff,
		int bit_mode);
	/*adpative_combing_config*/
	struct combing_status_s *
		(*adpative_combing_config)(unsigned int width,
					   unsigned int height,
					   enum vframe_source_type_e src_type,
					   bool prog,
					   enum tvin_sig_fmt_e fmt);
	void (*com_patch_pre_sw_set)(unsigned int mode);
	int (*module_para)(struct seq_file *seq);
};

bool di_attach_ops_mtn(const struct mtn_op_s **ops);

#endif	/*__DI_PQA_H__*/
