/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM printk

#define TRACE_INCLUDE_PATH trace/hooks

#if !defined(_TRACE_HOOK_PRINTK_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_HOOK_PRINTK_H

#include <linux/tracepoint.h>
#include <trace/hooks/vendor_hooks.h>

#if IS_ENABLED(CONFIG_AMLOGIC_PRINTK)
DECLARE_HOOK(android_vh_printk_modify_len,
	TP_PROTO(u16 *reserve_size, unsigned long irqflags),
	TP_ARGS(reserve_size, irqflags));

DECLARE_HOOK(android_vh_printk_insert_info,
	TP_PROTO(char *text_buf, u16 *text_len),
	TP_ARGS(text_buf, text_len));
#endif

DECLARE_HOOK(android_vh_printk_hotplug,
	TP_PROTO(int *flag),
	TP_ARGS(flag));

#endif /* _TRACE_HOOK_PRINTK_H */
/* This part must be outside protection */
#include <trace/define_trace.h>
