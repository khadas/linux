/*
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Description:
 */
#ifndef VAV1_H
#define VAV1_H

#define DEBUG_FGS_REGS            (1 << 0)   //0x00000001
#define DEBUG_FGS_REGS_PARSE      (1 << 1)   //0x00000002
#define DEBUG_FGS_DETAIL          (1 << 2)   //0x00000004
#define DEBUG_FGS_TABLE_DUMP      (1 << 3)   //0x00000008
#define DEBUG_FGS_CONSUME_TIME    (1 << 4)   //0x00000010
#define DEBUG_FGS_BYPASS          (1 << 5)   //0x00000020

int get_debug_fgs(void);
int pic_film_grain_run(u32 frame_count, char *fg_table_addr, u32 fgs_ctrl, u32 *fgs_data);

void adapt_coef_probs(int pic_count, int prev_kf, int cur_kf, int pre_fc,
unsigned int *prev_prob, unsigned int *cur_prob, unsigned int *count);
#endif
