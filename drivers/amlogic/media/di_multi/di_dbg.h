/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * drivers/amlogic/media/di_multi/di_dbg.h
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

#ifndef __DI_DBG_H__
#define __DI_DBG_H__

#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>

void didbg_fs_init(void);
void didbg_fs_exit(void);

void di_cfgx_init_val(void);

void didbg_vframe_in_copy(unsigned int ch, struct vframe_s *pvfm);
void didbg_vframe_out_save(unsigned int ch, struct vframe_s *pvfm, unsigned int id);

/********************************
 *debug register:
 *******************************/
void ddbg_reg_save(unsigned int addr, unsigned int val,
		   unsigned int st, unsigned int bw);
void dim_ddbg_mod_save(unsigned int mod,
		       unsigned int ch,
		       unsigned int cnt);
void ddbg_sw(unsigned int mode, bool on);

/********************************
 *time:
 *******************************/
u64 cur_to_msecs(void);
u64 cur_to_usecs(void);	/*2019*/

/********************************
 *trace:
 *******************************/
struct dim_tr_ops_s {
	void (*pre)(unsigned int index, unsigned long ctime);
	void (*post)(unsigned int index, unsigned long ctime);
	void (*pre_get)(unsigned int index);
	void (*pre_set)(unsigned int index);
	void (*pre_ready)(unsigned int index);
	void (*post_ready)(unsigned int index);
	void (*post_get)(unsigned int index);
	void (*post_get2)(unsigned int index);
	void (*post_set)(unsigned int index);
	void (*post_ir)(unsigned int index);
	void (*post_do)(unsigned int index);
	void (*post_peek)(unsigned int index);
	void (*sct_alloc)(unsigned int index, u64 timer_begin);
	void (*sct_tail)(unsigned int index, unsigned int used_cnt);
	void (*self_trig)(unsigned int index);
};

extern const struct dim_tr_ops_s dim_tr_ops;
void dbg_timer(unsigned int ch, enum EDBG_TIMER item);
void dim_dump_mif_state(struct DI_MIF_S *mif, char *name);

#endif	/*__DI_DBG_H__*/
