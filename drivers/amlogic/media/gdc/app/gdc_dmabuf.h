/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _GDC_DMABUF_H_
#define _GDC_DMABUF_H_

#include <linux/mm_types.h>
#include <linux/mutex.h>
#include <linux/dma-buf.h>
#include <linux/amlogic/media/gdc/gdc.h>

#define AML_MAX_DMABUF 32

struct aml_dma_buf {
	struct device		*dev;
	void				*cookie;
	void				*vaddr;
	unsigned int		size;
	enum dma_data_direction		dma_dir;
	unsigned long		attrs;
	unsigned int		index;
	dma_addr_t			dma_addr;
	atomic_t			refcount;
	/* DMABUF related */
	struct dma_buf_attachment	*db_attach;
	void                *priv;
	struct sg_table			*sgt_base;
};

struct aml_dma_buf_priv {
	void *mem_priv;
	int index;
	int fd;
	unsigned int alloc;
	struct dma_buf *dbuf;
};

struct aml_dma_buffer {
	struct mutex lock; /* */
	struct aml_dma_buf_priv gd_buffer[AML_MAX_DMABUF];
};

void *gdc_dma_buffer_create(void);
void gdc_dma_buffer_destroy(struct aml_dma_buffer *buffer);
int gdc_dma_buffer_alloc(struct aml_dma_buffer *buffer,
			 struct device *dev,
			 struct gdc_dmabuf_req_s *gdc_req_buf);
int gdc_dma_buffer_free(struct aml_dma_buffer *buffer, int index);
int gdc_dma_buffer_export(struct aml_dma_buffer *buffer,
			  struct gdc_dmabuf_exp_s *gdc_exp_buf);
int gdc_dma_buffer_map(struct aml_dma_cfg *cfg);
void gdc_dma_buffer_unmap(struct aml_dma_cfg *cfg);
int gdc_dma_buffer_get_phys(struct aml_dma_buffer *buffer,
			    struct aml_dma_cfg *cfg, unsigned long *addr);
void gdc_dma_buffer_dma_flush(struct device *dev, int fd);
void gdc_dma_buffer_cache_flush(struct device *dev, int fd);
void gdc_recycle_linear_config(struct aml_dma_cfg *cfg);
#endif
