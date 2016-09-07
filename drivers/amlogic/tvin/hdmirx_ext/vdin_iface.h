/*
 * drivers/amlogic/tvin/hdmirx_ext/vdin_iface.h
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

#ifndef __VDIN_INTERFACE_H__
#define __VDIN_INTERFACE_H__

#include "../tvin_frontend.h"
#include "hdmiin_drv.h"

extern int start_tvin_service(int no, struct vdin_parm_s *para);
extern int stop_tvin_service(int no);
extern void set_invert_top_bot(bool invert_flag);

extern int hdmiin_register_tvin_frontend(struct tvin_frontend_s *frontend);
extern void hdmiin_stop_vdin(struct hdmiin_vdin_s *info);
extern void hdmiin_start_vdin(struct hdmiin_vdin_s *info, int width, int height,
		int frame_rate, int field_flag);
extern void hdmiin_start_vdin_mode(unsigned int mode);

#endif
