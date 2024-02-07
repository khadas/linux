/* SPDX-License-Identifier: GPL-2.0 */
#if !defined(_TRACE_DMC_MONITOR_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_DMC_MONITOR_H

#include <linux/tracepoint.h>

#undef TRACE_SYSTEM
#define TRACE_SYSTEM dmc_monitor

#include <asm/memory.h>
#include <linux/page-flags.h>
struct page;
char *to_ports(int id);
char *to_sub_ports_name(int mid, int sid, char rw);
unsigned long read_violation_mem(unsigned long addr, char rw);

TRACE_EVENT(dmc_violation,

	TP_PROTO(char *title, unsigned long addr, unsigned long status, char *port, char *sub,
		 char rw, unsigned long pagetrace, unsigned long flags, unsigned long long time),

	TP_ARGS(title, addr, status, port, sub, rw, pagetrace, flags, time),

	TP_STRUCT__entry(
		__string(title, title)
		__field(unsigned long, addr)
		__field(unsigned long, status)
		__string(port, port)
		__string(sub, sub)
		__field(char, rw)
		__field(unsigned long, page_trace)
		__field(unsigned long, flags)
		__field(unsigned long long, time)
	),

	TP_fast_assign(
		__assign_str(title, title);
		__entry->addr = addr;
		__entry->status = status;
		__assign_str(port, port);
		__assign_str(sub, sub);
		__entry->rw = rw;
		__entry->page_trace = pagetrace;
		__entry->flags = flags;
		__entry->time = time;
	),

	TP_printk("addr=%09lx val=%016lx s=%08lx port=%s sub=%s f:%08lx bd:%d sb:%d lru:%d a:%ps t:%lld rw:%c%s",
		  __entry->addr,
		  read_violation_mem(__entry->addr, __entry->rw),
		  __entry->status,
		  __get_str(port),
		  __get_str(sub),
		  __entry->flags,
		  __entry->flags & PAGE_FLAGS_CHECK_AT_FREE ? 1 : 0,
		  test_bit(PG_slab, &__entry->flags),
		  test_bit(PG_lru, &__entry->flags),
		  (void *)__entry->page_trace,
		  __entry->time,
		  __entry->rw,
		  __get_str(title))
);

#endif /*  _TRACE_DMC_MONITOR_H */

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH ../../drivers/amlogic/ddr_tool/
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE dmc_trace

/* This part must be outside protection */
#include <trace/define_trace.h>
