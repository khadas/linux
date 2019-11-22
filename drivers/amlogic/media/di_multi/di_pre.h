/*
 * drivers/amlogic/media/di_multi/di_pre.h
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

#ifndef __DI_PRE_H__
#define __DI_PRE_H__

void dpre_process(void);

void dpre_init(void);

const char *dpre_state_name_get(enum eDI_PRE_ST state);
void dpre_dbg_f_trig(unsigned int cmd);
void pre_vinfo_set(unsigned int ch,
		   struct vframe_s *ori_vframe);
unsigned int is_vinfo_change(unsigned int ch);
bool dpre_can_exit(unsigned int ch);
bool is_bypass_i_p(void);
bool dim_bypass_detect(unsigned int ch, struct vframe_s *vfm);

void pre_mode_setting(void);
bool dpre_process_step4(void);
const char *dpre_state4_name_get(enum eDI_PRE_ST4 state);

#endif	/*__DI_PRE_H__*/
