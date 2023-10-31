/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/kprobes.h>

#define DBUF_TRACE_FUNC_0	(0)
#define DBUF_TRACE_FUNC_1	(1)
#define DBUF_TRACE_FUNC_2	(2)
#define DBUF_TRACE_FUNC_3	(3)
	//...
#define DBUF_TRACE_FUNC_MAX	(128)

#define KPS_DEF()				\
	int kps_n = ARRAY_SIZE(g_kps);		\
	struct kprobe *kps_p[DBUF_TRACE_FUNC_MAX]

#define KPS_INIT(t)				\
	({typeof(t) _t = t; int i;		\
	for (i = 0; i < kps_n; i++)		\
		kps_p[i] = &(_t)->kps[i];	\
	})

#define KPS_GET()	(kps_p)
#define KPS_SIZE()	(kps_n)

typedef void (*kps_cb)(void*, const char*, int, int, void *, void *, void *);

int codec_mm_reg_kprobes(void **kps_h, void *priv, kps_cb cb);
void codec_mm_unreg_kprobes(void *kps_h);

