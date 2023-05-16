/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _GE2D_DMABUF_H_
#define _GE2D_DMABUF_H_

#include <linux/mm_types.h>
#include <linux/mutex.h>
#include <linux/dma-buf.h>

/* Amlogic Headers */
#include <linux/amlogic/media/ge2d/ge2d.h>

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
};

struct aml_dma_buf_priv {
	void *aml_buf;
	int index;
	int fd;
	unsigned int alloc;
	struct dma_buf *dbuf;
};

struct aml_dma_buffer {
	struct mutex lock; /* */
	struct aml_dma_buf_priv gd_buffer[AML_MAX_DMABUF];
};

struct aml_dma_cfg {
	int fd;
	void *dev;
	void *vaddr;
	struct dma_buf *dbuf;
	struct dma_buf_attachment *attach;
	struct sg_table *sg;
	enum dma_data_direction dir;
};

void *ge2d_dma_buffer_create(void);
void ge2d_dma_buffer_destroy(struct aml_dma_buffer *buffer);
int ge2d_dma_buffer_alloc(struct aml_dma_buffer *buffer,
			  struct device *dev,
			  struct ge2d_dmabuf_req_s *ge2d_req_buf);
int ge2d_dma_buffer_free(struct aml_dma_buffer *buffer, int index);
int ge2d_dma_buffer_export(struct aml_dma_buffer *buffer,
			   struct ge2d_dmabuf_exp_s *ge2d_exp_buf);
int ge2d_dma_buffer_map(struct aml_dma_cfg *cfg);
void ge2d_dma_buffer_unmap(struct aml_dma_cfg *cfg);
int ge2d_dma_buffer_get_phys(struct aml_dma_buffer *buffer,
			     struct aml_dma_cfg *cfg, unsigned long *addr);
void ge2d_dma_buffer_dma_flush(struct device *dev, int fd);
void ge2d_dma_buffer_cache_flush(struct device *dev, int fd);
int ge2d_ioctl_attach_dma_fd(struct ge2d_context_s *wq,
			     struct ge2d_dmabuf_attach_s *dma_attach);
void ge2d_ioctl_detach_dma_fd(struct ge2d_context_s *wq,
			      enum ge2d_data_type_e data_type);
#endif
