/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM aml_v4l2

#define TRACE_INCLUDE_PATH trace/hooks

#if !defined(_TRACE_HOOK_AML_V4L2_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_HOOK_AML_V4L2_H

#include <trace/hooks/vendor_hooks.h>

struct vb2_v4l2_buffer;
struct v4l2_buffer;
struct v4l2_fmtdesc;
struct v4l2_streamparm;

/*
 * Following tracepoints are not exported in tracefs and provide a
 * mechanism for vendor modules to hook and extend functionality
 */
DECLARE_HOOK(android_vh_meta_ptr_update,
	TP_PROTO(struct vb2_v4l2_buffer *vbuf, struct v4l2_buffer *b),
	TP_ARGS(vbuf, b));

DECLARE_HOOK(android_vh_fill_fmtdesc,
	TP_PROTO(struct v4l2_fmtdesc *format),
	TP_ARGS(format));

DECLARE_HOOK(android_vh_strparm_save,
	TP_PROTO(struct v4l2_streamparm *p, unsigned int mod),
	TP_ARGS(p, mod));

#endif /* _TRACE_HOOK_AML_V4L2_H */
/* This part must be outside protection */
#include <trace/define_trace.h>
