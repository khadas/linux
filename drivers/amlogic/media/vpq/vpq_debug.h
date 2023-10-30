/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __VPQ_DEBUG_H__
#define __VPQ_DEBUG_H__

#include <linux/types.h>
#include "vpq_drv.h"

ssize_t vpq_debug_cmd_show(struct class *cla,
			struct class_attribute *attr,
			char *buf);
ssize_t vpq_debug_cmd_store(struct class *class,
			struct class_attribute *attr,
			const char *buf,
			size_t count);

ssize_t vpq_log_lev_show(struct class *cla,
			struct class_attribute *attr,
			char *buf);
ssize_t vpq_log_lev_store(struct class *class,
			struct class_attribute *attr,
			const char *buf,
			size_t count);

ssize_t vpq_picture_mode_item_show(struct class *cla,
			struct class_attribute *attr,
			char *buf);
ssize_t vpq_picture_mode_item_store(struct class *class,
			struct class_attribute *attr,
			const char *buf,
			size_t count);

ssize_t vpq_pq_module_status_show(struct class *cla,
			struct class_attribute *attr,
			char *buf);
ssize_t vpq_pq_module_status_store(struct class *class,
			struct class_attribute *attr,
			const char *buf,
			size_t count);

ssize_t vpq_src_infor_show(struct class *cla,
			struct class_attribute *attr,
			char *buf);
ssize_t vpq_src_infor_store(struct class *class,
			struct class_attribute *attr,
			const char *buf,
			size_t count);

ssize_t vpq_src_hist_avg_show(struct class *cla,
			struct class_attribute *attr,
			char *buf);
ssize_t vpq_src_hist_avg_store(struct class *class,
			struct class_attribute *attr,
			const char *buf,
			size_t count);

ssize_t vpq_debug_other_show(struct class *cla,
			struct class_attribute *attr,
			char *buf);
ssize_t vpq_debug_other_store(struct class *class,
			struct class_attribute *attr,
			const char *buf,
			size_t count);

ssize_t vpq_dump_show(struct class *cla,
			struct class_attribute *attr,
			char *buf);
ssize_t vpq_dump_store(struct class *class,
			struct class_attribute *attr,
			const char *buf,
			size_t count);

#endif //__VPQ_DEBUG_H__
