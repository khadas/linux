/*
 * drivers/amlogic/amports/vdec_trace.h
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

#undef TRACE_SYSTEM
#define TRACE_SYSTEM vdec

#if !defined(_VDEC_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _VDEC_TRACE_H

#include <linux/tracepoint.h>

struct vdec_s;

/* single lifecycle events */
DECLARE_EVENT_CLASS(vdec_event_class,
	TP_PROTO(struct vdec_s *vdec),
	TP_ARGS(vdec),
	TP_STRUCT__entry(
		__field(struct vdec_s *, vdec)
	),
	TP_fast_assign(
		__entry->vdec = vdec;
	),
	TP_printk("[%p]",  __entry->vdec)
);

#define DEFINE_VDEC_EVENT(name) \
DEFINE_EVENT(vdec_event_class, name, \
	TP_PROTO(struct vdec_s *vdec), \
	TP_ARGS(vdec))

DEFINE_VDEC_EVENT(vdec_create);
DEFINE_VDEC_EVENT(vdec_connect);
DEFINE_VDEC_EVENT(vdec_disconnect);
DEFINE_VDEC_EVENT(vdec_destroy);
DEFINE_VDEC_EVENT(vdec_reset);
DEFINE_VDEC_EVENT(vdec_release);

/* set format event */
#define format_name(format) \
	__print_symbolic(format, \
		{0, "MPEG"}, \
		{1, "MPEG4"}, \
		{2, "H264"}, \
		{3, "MJPEG"}, \
		{4, "REAL"}, \
		{5, "JPEG"}, \
		{6, "VC1"}, \
		{7, "AVS"}, \
		{8, "YUV"}, \
		{9, "H264MVC"}, \
		{10, "H264_4K2K"}, \
		{11, "H265"}, \
		{12, "ENC_AVC"}, \
		{13, "ENC_JPEG"}, \
		{14, "VP9"})

TRACE_EVENT(vdec_set_format,
	TP_PROTO(struct vdec_s *vdec, int format),
	TP_ARGS(vdec, format),
	TP_STRUCT__entry(
		__field(struct vdec_s *, vdec)
		__field(int, format)
	),
	TP_fast_assign(
		 __entry->vdec = vdec;
		 __entry->format = format;
	),
	TP_printk("[%p]:%s", __entry->vdec,
		format_name(__entry->format))
);

/* status events */
#define status_name(status) \
	__print_symbolic(status, \
		{0, "UNINITIALIZED"}, \
		{1, "DISCONNECTED"}, \
		{2, "CONNECTED"}, \
		{3, "ACTIVE"})

DECLARE_EVENT_CLASS(vdec_status_class,
	TP_PROTO(struct vdec_s *vdec, int state),
	TP_ARGS(vdec, state),
	TP_STRUCT__entry(
		__field(struct vdec_s *, vdec)
		__field(int, state)
	),
	TP_fast_assign(
		__entry->vdec = vdec;
		__entry->state = state;
	),
	TP_printk("[%p]:%s", __entry->vdec, status_name(__entry->state))
);

#define DEFINE_STATUS_EVENT(name) \
DEFINE_EVENT(vdec_status_class, name, \
	TP_PROTO(struct vdec_s *vdec, int status), \
	TP_ARGS(vdec, status))

DEFINE_STATUS_EVENT(vdec_set_status);
DEFINE_STATUS_EVENT(vdec_set_next_status);

/* set pts events */
DECLARE_EVENT_CLASS(vdec_pts_class,
	TP_PROTO(struct vdec_s *vdec, u64 pts),
	TP_ARGS(vdec, pts),
	TP_STRUCT__entry(
		__field(struct vdec_s *, vdec)
		__field(u64, pts)
	),
	TP_fast_assign(
		__entry->vdec = vdec;
		__entry->pts = pts;
	),
	TP_printk("[%p]%llu", __entry->vdec, __entry->pts)
);

#define DEFINE_PTS_EVENT(name) \
DEFINE_EVENT(vdec_pts_class, name, \
	TP_PROTO(struct vdec_s *vdec, u64 pts), \
	TP_ARGS(vdec, pts))

DEFINE_PTS_EVENT(vdec_set_pts);
DEFINE_PTS_EVENT(vdec_set_pts64);

#endif /* _VDEC_TRACE_H */

/*
#undef TRACE_INCLUDE_PATH
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_PATH .
#define TRACE_INCLUDE_FILE vdec_trace
#include <trace/define_trace.h>
*/
/**/ //DEBUG_TMP
