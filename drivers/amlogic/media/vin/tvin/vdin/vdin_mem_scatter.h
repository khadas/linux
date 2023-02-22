/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __VDIN_MEM_SCATTER_H
#define __VDIN_MEM_SCATTER_H

/* Standard Linux headers */
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/cma.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/dma-contiguous.h>
#include <linux/delay.h>
#include <linux/sched/clock.h>

#include <linux/sizes.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include "vdin_mmu_box.h"
#include "vdin_drv.h"

#define DRIVER_NAME	"vdin"

int vdin_sct_init(struct vdin_dev_s *devp);
void vdin_sct_worker(struct work_struct *work);
void vdin_sct_alloc(struct vdin_dev_s *devp, int vf_idx);
int vdin_sct_start(struct vdin_dev_s *devp);
void vdin_sct_queue_work(struct vdin_dev_s *devp);
void vdin_sct_free(struct vf_pool *p, int index);
int vdin_mem_init(struct vdin_dev_s *devp);
int vdin_mem_exit(struct vdin_dev_s *devp);
void vdin_sct_read_mmu_num(struct vdin_dev_s *devp, struct vf_entry *vfe);
void vdin_sct_free_wr_list_idx(struct vf_pool *p, struct vframe_s *vf);
phy_addr_type vdin_sct_get_page_addr(struct codec_mm_scatter *mms, int id);

#endif /* __VDIN_MEM_SCATTER_H */

