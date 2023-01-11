/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _GE2D_WQ_H_
#define _GE2D_WQ_H_
#ifdef CONFIG_AMLOGIC_ION
#include <linux/ion.h>

extern struct ion_client *ge2d_ion_client;
#endif

ssize_t work_queue_status_show(struct class *cla,
			       struct class_attribute *attr, char *buf);

ssize_t free_queue_status_show(struct class *cla,
			       struct class_attribute *attr, char *buf);

int ge2d_setup(int irq, struct reset_control *rstc);
int ge2d_wq_init(struct platform_device *pdev,
		 int irq, struct clk *clk);
int ge2d_wq_deinit(void);

int ge2d_buffer_alloc(struct ge2d_dmabuf_req_s *ge2d_req_buf);
int ge2d_buffer_free(int index);
int ge2d_buffer_export(struct ge2d_dmabuf_exp_s *ge2d_exp_buf);
int ge2d_set_clut_table(struct ge2d_context_s *context, unsigned long args);
void ge2d_buffer_dma_flush(int dma_fd);
void ge2d_buffer_cache_flush(int dma_fd);
void ge2d_runtime_pwr(int enable);
int ge2d_irq_init(int irq);

#if IS_ENABLED(CONFIG_AMLOGIC_DMC_DEV_ACCESS)
void dmc_ge2d_test_notifier(void);
#endif
#endif
