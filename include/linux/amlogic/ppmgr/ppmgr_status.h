/*
 * include/linux/amlogic/ppmgr/ppmgr_status.h
 *
 * Copyright (C) 2015 Amlogic, Inc. All rights reserved.
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

extern int get_ppmgr_status(void);
extern void set_ppmgr_status(int flag);
#ifdef CONFIG_POST_PROCESS_MANAGER_3D_PROCESS
extern unsigned get_ppmgr_3dmode(void);
extern void set_ppmgr_3dmode(unsigned mode);
extern unsigned get_ppmgr_viewmode(void);
extern void set_ppmgr_viewmode(unsigned mode);
extern unsigned get_ppmgr_scaledown(void);
extern void set_ppmgr_scaledown(unsigned scale_down);
extern unsigned get_ppmgr_direction3d(void);
extern void set_ppmgr_direction3d(unsigned angle);
#endif
int get_use_prot(void);
