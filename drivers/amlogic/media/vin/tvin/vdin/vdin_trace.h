/*
 * drivers/amlogic/media/vin/tvin/vdin/vdin_trace.h
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

#undef TRACE_SYSTEM
#define TRACE_SYSTEM vdin

#if !defined(_VDIN_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _VDIN_TRACE_H

#include <linux/tracepoint.h>

/* single lifecycle events */
DECLARE_EVENT_CLASS(vdin_event_class,
	TP_PROTO(const char *name, int field_cnt, unsigned long time),
	TP_ARGS(name, field_cnt, time),
	TP_STRUCT__entry(
		__string(name, name)
		__field(int, field_cnt)
		__field(unsigned long, time)
	),
	TP_fast_assign(
		__assign_str(name, name);
		__entry->field_cnt = field_cnt;
		__entry->time = time;
	),
	TP_printk("[%s-%dth-%lums]",  __get_str(name),
			__entry->field_cnt, __entry->time)
);

#define DEFINE_VDIN_EVENT(name) \
DEFINE_EVENT(vdin_event_class, name, \
	TP_PROTO(const char *name, int field_cnt, unsigned long time), \
	TP_ARGS(name, field_cnt, time))

DEFINE_VDIN_EVENT(vdin_afbc);
#endif /* _VDEC_TRACE_H */

#if 0
#define CREATE_TRACE_POINTS
#include "vdin_trace.h"
#undef TRACE_INCLUDE_PATH
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_PATH .
#define TRACE_INCLUDE_FILE vdin_trace
#include <trace/define_trace.h>

#define VDIN_TRACE
#ifdef VDIN_TRACE
trace_vdin_afbc("vdin-afbc", devp->afbce_mode, devp->frame_cnt);
#endif

#endif

/*
 * a. clear pipe
 * cat /sys/kernel/debug/tracing/trace_pipe
 * b. enable trace di:
 * echo 1 >/sys/kernel/debug/tracing/events/vdin/vdin_afbc/enable
 * echo 1 >/sys/kernel/debug/tracing/tracing_on
 * c. disable trace
 * echo 0 >/sys/kernel/debug/tracing/tracing_on
 * cat /sys/kernel/debug/tracing/trace_pipe
 */

