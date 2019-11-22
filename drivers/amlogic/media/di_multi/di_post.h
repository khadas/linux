/*
 * drivers/amlogic/media/di_multi/di_post.h
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

#ifndef __DI_POST_H__
#define __DI_POST_H__

void dpost_init(void);
void dpst_process(void);
const char *dpst_state_name_get(enum eDI_PST_ST state);
void dpst_dbg_f_trig(unsigned int cmd);
bool dpst_can_exit(unsigned int ch);

#endif	/*__DI_POST_H__*/
