/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
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
	struct gdc_context_s *context;
	u32 core_id; /* core index for processing */
};

struct gdc_irq_handle_wq {
	struct gdc_queue_item_s *current_item;
	struct work_struct work;
};

u8 __iomem *gdc_map_virt_from_phys(phys_addr_t phys, unsigned long total_size);
void gdc_unmap_virt_from_phys(u8 __iomem *vaddr);

int gdc_wq_init(void);
int gdc_wq_deinit(void);
void *gdc_prepare_item(struct gdc_context_s *wq);
int gdc_wq_add_work(struct gdc_context_s *wq,
		    struct gdc_queue_item_s *pitem);
inline void recycle_resource(struct gdc_queue_item_s *item, u32 core_id);
void dump_config_file(struct gdc_config_s *gc, u32 dev_type);
void dump_gdc_regs(u32 dev_type, u32 core_id);

#endif
