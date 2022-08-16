/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM video

#if !defined(_VIDEO_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _VIDEO_TRACE_H

#include <linux/tracepoint.h>

/* single lifecycle events */
DECLARE_EVENT_CLASS
	(video_event_class,
	TP_PROTO(const char *name, int left, int top, int right, int bottom),
	TP_ARGS(name, left, top, bottom, right),
	TP_STRUCT__entry
		(__string(name, name)
		 __field(int, left)
		 __field(int, top)
		 __field(int, right)
		 __field(int, bottom)
	),
	TP_fast_assign
		(__assign_str(name, name);
		 __entry->left = left;
		 __entry->top = top;
		 __entry->right = right;
		 __entry->bottom = bottom;
	),
	TP_printk("[%s:%d %d %d %d]", __get_str(name),
		  __entry->left, __entry->top,
		  __entry->right, __entry->bottom)
);

#define DEFINE_VIDEO_EVENT(name) \
DEFINE_EVENT(video_event_class, name, \
	TP_PROTO(const char *name, int left, int top, int right, int bottom), \
	TP_ARGS(name, left, top, right, bottom))

DEFINE_VIDEO_EVENT(axis);

DECLARE_EVENT_CLASS
	(vsync_log_time,
	TP_PROTO(const char *name,
		unsigned long time1, unsigned long time2,
		unsigned long time3, unsigned long time4,
		unsigned long time5, int duration),
	TP_ARGS(name, time1, time2, time3, time4, time5, duration),
	TP_STRUCT__entry
		(__string(name, name)
		 __field(unsigned long, time1)
		 __field(unsigned long, time2)
		 __field(unsigned long, time3)
		 __field(unsigned long, time4)
		 __field(unsigned long, time5)
		 __field(int, duration)
	),
	TP_fast_assign
		(__assign_str(name, name);
		 __entry->time1 = time1;
		 __entry->time2 = time2;
		 __entry->time3 = time3;
		 __entry->time4 = time4;
		 __entry->time5 = time5;
		 __entry->duration = duration;
	),
	TP_printk("[%s:%ld %ld %ld %ld %ld %d]", __get_str(name),
		  __entry->time1, __entry->time2,
		  __entry->time3, __entry->time4,
		  __entry->time5, __entry->duration)
);

#define DEFINE_VSYNC_TIME(trace_name) \
DEFINE_EVENT(vsync_log_time, trace_name, \
	TP_PROTO(const char *name, \
		unsigned long time1, unsigned long time2, \
		unsigned long time3, unsigned long time4, \
		unsigned long time5, int duration), \
	TP_ARGS(name, time1, time2, time3, time4, time5, duration))

DEFINE_VSYNC_TIME(timeinfo);

DECLARE_EVENT_CLASS
	(vsync_log_encline,
	TP_PROTO(const char *name, const char *sub_name, int start_line, int cur_line),
	TP_ARGS(name, sub_name, start_line, cur_line),
	TP_STRUCT__entry
		(__string(name, name)
		 __string(sub_name, sub_name)
		 __field(int, start_line)
		 __field(int, cur_line)
	),
	TP_fast_assign
		(__assign_str(name, name);
		 __assign_str(sub_name, sub_name);
		 __entry->start_line = start_line;
		 __entry->cur_line = cur_line;
	),
	TP_printk("[%s:[%s]:%d %d]",
		  __get_str(name), __get_str(sub_name),
		  __entry->start_line, __entry->cur_line)
);

#define DEFINE_VSYNC_ENCLINE(trace_name) \
DEFINE_EVENT(vsync_log_encline, trace_name, \
	TP_PROTO(const char *name, const char *sub_name, int start_line, int cur_line), \
	TP_ARGS(name, sub_name, start_line, cur_line))

DEFINE_VSYNC_ENCLINE(encline);

DECLARE_EVENT_CLASS
	(vsync_log_field_state,
	TP_PROTO(const char *name, const char *sub_name,
				int cur_state, int new_state,
				int flag, int cnt1, int cnt2),
	TP_ARGS(name, sub_name, cur_state, new_state, flag, cnt1, cnt2),
	TP_STRUCT__entry
		(__string(name, name)
		 __string(sub_name, sub_name)
		 __field(int, cur_state)
		 __field(int, new_state)
		 __field(int, flag)
		 __field(int, cnt1)
		 __field(int, cnt2)
	),
	TP_fast_assign
		(__assign_str(name, name);
		 __assign_str(sub_name, sub_name);
		 __entry->cur_state = cur_state;
		 __entry->new_state = new_state;
		 __entry->flag = flag;
		 __entry->cnt1 = cnt1;
		 __entry->cnt2 = cnt2;
	),
	TP_printk("[%s:[%s]:%d->%d; %d-%d-%d]",
		  __get_str(name), __get_str(sub_name),
		  __entry->cur_state, __entry->new_state,
		  __entry->flag, __entry->cnt1, __entry->cnt2)
);

#define DEFINE_VSYNC_FSTATE(trace_name) \
DEFINE_EVENT(vsync_log_field_state, trace_name, \
	TP_PROTO(const char *name, const char *sub_name, \
		int cur_state, int new_state, \
		int flag, int cnt1, int cnt2), \
	TP_ARGS(name, sub_name, cur_state, new_state, flag, cnt1, cnt2))

DEFINE_VSYNC_FSTATE(field_state);

DECLARE_EVENT_CLASS
	(vsync_log_vframe,
	TP_PROTO(const char *name, void *vf, int arg1, int arg2, int id, int cnt),
	TP_ARGS(name, vf, arg1, arg2, id, cnt),
	TP_STRUCT__entry
		(__string(name, name)
		 __field(void *, vf)
		 __field(int, arg1)
		 __field(int, arg2)
		 __field(int, id)
		 __field(int, cnt)
	),
	TP_fast_assign
		(__assign_str(name, name);
		 __entry->vf = vf;
		 __entry->arg1 = arg1;
		 __entry->arg2 = arg2;
		 __entry->id = id;
		 __entry->cnt = cnt;
	),
	TP_printk("[%s:%px %x %x; id:%d:%d]",  __get_str(name),
		  __entry->vf, __entry->arg1,
		  __entry->arg2, __entry->id,
		  __entry->cnt)
);

#define DEFINE_VSYNC_VFRAME(trace_name) \
DEFINE_EVENT(vsync_log_vframe, trace_name, \
	TP_PROTO(const char *name, void *vf, int arg1, int arg2, int id, int cnt), \
	TP_ARGS(name, vf, arg1, arg2, id, cnt))

DEFINE_VSYNC_VFRAME(vframe);
#endif /* _VIDEO_TRACE_H */
