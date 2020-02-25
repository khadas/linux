/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __DI_REG_TABL_H__
#define __DI_REG_TABL_H__

int reg_con_show(struct seq_file *seq, void *v);

bool dim_wr_cue_int(void);
int dim_reg_cue_int_show(struct seq_file *seq, void *v);

#endif	/*__DI_REG_TABL_H__*/
