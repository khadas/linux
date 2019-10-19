/*
 * drivers/amlogic/media/gdc/app/gdc_wq.h
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
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

#ifndef _GDC_WQ_H_
#define _GDC_WQ_H_

#include <linux/types.h>
#include <linux/sched.h>
#include <linux/semaphore.h>
#include <linux/amlogic/media/gdc/gdc.h>
#include <gdc_config.h>
#include <gdc_api.h>

#define MAX_GDC_CMD  32   /* 64 */
#define GDC_STATE_IDLE                 0
#define GDC_STATE_RUNNING              1

#define	GDC_PROCESS_QUEUE_START        0
#define	GDC_PROCESS_QUEUE_STOP         1

struct gdc_queue_item_s {
	struct list_head list;
	struct gdc_cmd_s cmd;
	struct gdc_dma_cfg_t dma_cfg;
};

extern unsigned int gdc_reg_store_mode;
extern int trace_mode_enable;
extern char *config_out_file;
extern int config_out_path_defined;

u8 __iomem *map_virt_from_phys(phys_addr_t phys, unsigned long total_size);
void unmap_virt_from_phys(u8 __iomem *vaddr);

int gdc_wq_init(struct meson_gdc_dev_t *gdc_dev);
int gdc_wq_deinit(void);
void *gdc_prepare_item(struct gdc_context_s *wq);
int gdc_wq_add_work(struct gdc_context_s *wq,
	struct gdc_queue_item_s *pitem);
#endif
