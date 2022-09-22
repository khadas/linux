// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * drivers/amlogic/media/video_sink/video_trace.c
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

#define CREATE_TRACE_POINTS
#include "video_trace.h"
#undef TRACE_INCLUDE_PATH
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_PATH ../../drivers/amlogic/media/video_sink
#define TRACE_INCLUDE_FILE video_trace
#include <trace/define_trace.h>

void vpp_trace_axis(int left, int top, int right, int bottom)
{
	trace_axis("VIDEO-AXIS", left, top, right, bottom);
}

void vpp_trace_timeinfo(unsigned long time1,
	unsigned long time2, unsigned long time3,
	unsigned long time4, unsigned long time5,
	int duration)
{
	trace_timeinfo("VSYNC-TIME", time1, time2, time3, time4, time5, duration);
}

void vpp_trace_encline(const char *sub_name, int start_line, int cur_line)
{
	trace_encline("VSYNC-ENCLINE", sub_name, start_line, cur_line);
}

void vpp_trace_field_state(const char *sub_name,
	int cur_state, int new_state,
	int flag, int cnt1, int cnt2)
{
	trace_field_state("VSYNC-FSTATE", sub_name, cur_state, new_state, flag, cnt1, cnt2);
}

void vpp_trace_vframe(const char *name, void *vf, int arg1, int arg2, int id, int cnt)
{
	trace_vframe(name, vf, arg1, arg2, id, cnt);
}

