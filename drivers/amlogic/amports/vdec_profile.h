/*
 * drivers/amlogic/amports/vdec_profile.h
 *
 * Copyright (C) 2016 Amlogic, Inc. All rights reserved.
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

#ifndef VDEC_PROFILE_H
#define VDEC_PROFILE_H

struct vdec_s;

#define VDEC_PROFILE_EVENT_RUN         0
#define VDEC_PROFILE_EVENT_CB          1
#define VDEC_PROFILE_EVENT_SAVE_INPUT  2
#define VDEC_PROFILE_EVENT_CHK_RUN_READY 3
#define VDEC_PROFILE_EVENT_RUN_READY   4
#define VDEC_PROFILE_EVENT_DISCONNECT  5
#define VDEC_PROFILE_EVENT_DEC_WORK    6
#define VDEC_PROFILE_MAX_EVENT         7

extern void vdec_profile(struct vdec_s *vdec, int event);

extern void vdec_profile_flush(struct vdec_s *vdec);

#endif /* VDEC_PROFILE_H */
