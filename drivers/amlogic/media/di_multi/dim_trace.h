/*
 * drivers/amlogic/media/di_multi/dim_trace.h
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
#define TRACE_SYSTEM dim

#if !defined(_DIM_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _DIM_TRACE_H

#include <linux/tracepoint.h>

/* single lifecycle events */
DECLARE_EVENT_CLASS(di_event_class,
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
	TP_printk("[%s-%-4dth-%lums]",  __get_str(name),
		  __entry->field_cnt, __entry->time)
);

#define DEFINE_DI_EVENT(name) \
DEFINE_EVENT(di_event_class, name, \
	TP_PROTO(const char *name, int field_cnt, unsigned long time), \
	TP_ARGS(name, field_cnt, time))

DEFINE_DI_EVENT(dim_pre);
DEFINE_DI_EVENT(dim_post);
/*2019-06-18*/
DEFINE_DI_EVENT(dim_pre_getxx);
DEFINE_DI_EVENT(dim_pre_setxx);
DEFINE_DI_EVENT(dim_pre_ready);
DEFINE_DI_EVENT(dim_pst_ready);
DEFINE_DI_EVENT(dim_pst_getxx);
DEFINE_DI_EVENT(dim_pst_setxx);
DEFINE_DI_EVENT(dim_pst_irxxx);
DEFINE_DI_EVENT(dim_pst_doing);
DEFINE_DI_EVENT(dim_pst_peekx);
DEFINE_DI_EVENT(dim_pst_get2x);

#endif /* _DIM_TRACE_H */

#if 0
#undef TRACE_INCLUDE_PATH
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_PATH .
#define TRACE_INCLUDE_FILE deinterlace_trace
#include <trace/define_trace.h>
#endif
