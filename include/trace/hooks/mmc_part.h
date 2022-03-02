/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM mmc_part

#define TRACE_INCLUDE_PATH trace/hooks

#if !defined(_TRACE_HOOK_MMC_PART_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_HOOK_HOOK_MMC_PART_H

#include <linux/tracepoint.h>
#include <trace/hooks/vendor_hooks.h>
#include <linux/mmc/card.h>

DECLARE_HOOK(android_vh_amlmmc_dtb_key_init,
	TP_PROTO(struct mmc_card *card, int *retp),
	TP_ARGS(card, retp));

#endif /* _TRACE_HOOK_MMC_PART_H */
/* This part must be outside protection */
#include <trace/define_trace.h>
