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
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/semaphore.h>
#include <gdc_config.h>

struct gdc_cmd_s;
#define MAX_GDC_CMD  32   /* 64 */
#define MAX_PLANE    3
#define GDC_STATE_IDLE                 0
#define GDC_STATE_RUNNING              1

#define	GDC_PROCESS_QUEUE_START        0
#define	GDC_PROCESS_QUEUE_STOP         1

struct gdc_dmabuf_cfg_s {
	int dma_used;
	struct aml_dma_cfg dma_cfg;
};

struct gdc_dma_cfg_t {
	struct gdc_dmabuf_cfg_s config_cfg;
	struct gdc_dmabuf_cfg_s input_cfg[MAX_PLANE];
	struct gdc_dmabuf_cfg_s output_cfg[MAX_PLANE];
};

struct gdc_queue_item_s {
	struct list_head list;
	struct gdc_cmd_s cmd;
	struct gdc_dma_cfg_t dma_cfg;
};

struct gdc_context_s {
	/* connect all process in one queue for RR process. */
	struct list_head   list;
	/* current wq configuration */
	uint32_t mmap_type;
	dma_addr_t i_paddr;
	dma_addr_t o_paddr;
	dma_addr_t c_paddr;
	void *i_kaddr;
	void *o_kaddr;
	void *c_kaddr;
	unsigned long i_len;
	unsigned long o_len;
	unsigned long c_len;
	struct gdc_dma_cfg y_dma_cfg;
	struct gdc_dma_cfg uv_dma_cfg;
	struct gdc_dma_cfg_t dma_cfg;
	struct mutex d_mutext;

	struct gdc_cmd_s	cmd;
	struct list_head	work_queue;
	struct list_head	free_queue;
	wait_queue_head_t	cmd_complete;
	int	                gdc_request_exit;
	spinlock_t              lock;/* for get and release item. */
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
struct gdc_context_s *create_gdc_work_queue(void);
int destroy_gdc_work_queue(struct gdc_context_s *gdc_work_queue);
#endif
