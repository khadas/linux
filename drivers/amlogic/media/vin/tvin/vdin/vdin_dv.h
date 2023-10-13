/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __TVIN_VDIN_DV_H
#define __TVIN_VDIN_DV_H
#include <linux/highmem.h>
#include <linux/page-flags.h>
#include <linux/vmalloc.h>
/*#include <linux/dma-mapping.h>*/
/*#include <linux/dma-contiguous.h>*/
#include "vdin_drv.h"

#define K_FORCE_DV_ON			0

#define K_DV_META_BUFF_SIZE		PAGE_SIZE
#define K_DV_META_TEMP_BUFF_SIZE	2048
#define K_DV_META_RAW_BUFF0		(PAGE_SIZE * 5)
#define K_DV_META_RAW_BUFF1		PAGE_SIZE

extern unsigned int dv_dbg_log;
extern unsigned int dv_dbg_log_du;

void vdin_wrmif2_enable(struct vdin_dev_s *devp, u32 en, unsigned int rdma_enable);
void vdin_wrmif2_initial(struct vdin_dev_s *devp);
void vdin_wrmif2_addr_update(struct vdin_dev_s *devp);
irqreturn_t vdin_wrmif2_dv_meta_wr_done_isr(int irq, void *dev_id);
bool vdin_dv_is_need_tunnel(struct vdin_dev_s *devp);
bool vdin_dv_is_visf_data(struct vdin_dev_s *devp);
bool vdin_dv_not_manual_game(struct vdin_dev_s *devp);
bool vdin_dv_is_not_std_source_led(struct vdin_dev_s *devp);
#endif
