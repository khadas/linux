/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef AML_MM_HEADER
#define AML_MM_HEADER
#include <linux/atomic.h>
#include <linux/spinlock.h>
#include <linux/list.h>
#include<linux/printk.h>

#ifndef INFO_PREFIX
#define INFO_PREFIX "codec_mm"
#endif

/*#define codec_print(KERN_LEVEL, args...) \*/
		/*printk(KERN_LEVEL INFO_PREFIX ":" args)*/

#define codec_info(args...) pr_info(args)
#define codec_err(args...)  pr_err(args)
#define codec_warning(args...)  pr_warn(args)

/*
 *#ifdef pr_info
 *#undef pr_info
 *#undef pr_err
 *#undef pr_warn
 *#undef pr_warning
 *
 *#define pr_info(args...) codec_info(args)
 *#define pr_err(args...) codec_err(args)
 *#define pr_warn(args...) codec_warning(args)
 *#define pr_warning pr_warn
 *
 *#endif
 */

void dma_clear_buffer(struct page *page, size_t size);

u32 codec_mm_get_sc_debug_mode(void);
u32 codec_mm_get_keep_debug_mode(void);
bool codec_mm_scatter_available_check(int size);
void codec_mm_scatter_level_decrease(int size);
void codec_mm_scatter_level_increase(int size);
void codec_mm_set_min_linear_size(int min_mem_val);
int codec_mm_get_min_linear_size(void);
int codec_mm_get_scatter_watermark(void);


#endif /**/
