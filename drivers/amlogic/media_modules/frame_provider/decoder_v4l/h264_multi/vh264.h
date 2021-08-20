/*
 * drivers/amlogic/media/frame_provider/decoder/h264/vh264.h
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

#ifndef VH264_H
#define VH264_H

extern int query_video_status(int type, int *value);

/* extern s32 vh264_init(void); */

extern s32 vh264_release(void);

#endif /* VMPEG4_H */
