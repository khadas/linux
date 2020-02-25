/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
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
void didbg_vframe_out_save(struct vframe_s *pvfm);

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
};

extern const struct dim_tr_ops_s dim_tr_ops;

#endif	/*__DI_DBG_H__*/
