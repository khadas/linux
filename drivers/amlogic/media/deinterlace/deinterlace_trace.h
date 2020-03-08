/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM di

#if !defined(_DI_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _DI_TRACE_H

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
	TP_printk("[%s-%dth-%lums]",  __get_str(name),
			__entry->field_cnt, __entry->time)
);

#define DEFINE_DI_EVENT(name) \
DEFINE_EVENT(di_event_class, name, \
	TP_PROTO(const char *name, int field_cnt, unsigned long time), \
	TP_ARGS(name, field_cnt, time))

DEFINE_DI_EVENT(di_pre);
DEFINE_DI_EVENT(di_post);
#endif /* _VDEC_TRACE_H */

#undef TRACE_INCLUDE_PATH
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_PATH  ../../drivers/amlogic/media/deinterlace
#define TRACE_INCLUDE_FILE deinterlace_trace
#include <trace/define_trace.h>
