/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef _LINUX_SECMEM_H_
#define _LINUX_SECMEM_H_

#include <linux/ioctl.h>
#include <linux/types.h>
#include <linux/dma-buf.h>

#define AML_DMA_BUF_MANAGER_VERSION  1
#define VIDEODEC_DATA_MAX_LEN 256
#define DMA_BUF_MANAGER_MAX_BUFFER_LEN 320

enum dmabuf_manage_type {
	DMA_BUF_TYPE_INVALID,
	DMA_BUF_TYPE_SECMEM,
	DMA_BUF_TYPE_DMX_ES,
	DMA_BUF_TYPE_DMABUF,
	DMA_BUF_TYPE_VIDEODEC_ES,
	DMA_BUF_TYPE_MAX
};

struct filter_info {
	__u32 token;
	__u32 filter_fd;
	__u32 release;
};

struct dmabuf_dmx_sec_es_data {
	__u8 pts_dts_flag;
	__u64 video_pts;
	__u64 video_dts;
	__u32 buf_start;
	__u32 buf_end;
	__u32 data_start;
	__u32 data_end;
	__u32 buf_rp;
	__u64 av_handle;
	__u32 token;
	__u32 extend0;
	__u32 extend1;
	__u32 extend2;
	__u32 extend3;
};

struct secmem_block {
	__u32 paddr;
	__u32 size;
	__u32 handle;
};

enum dmabuf_manage_videodec_type {
	DMA_BUF_VIDEODEC_HDR10PLUS = 1
};

struct dmabuf_videodec_es_data {
	__u32 data_type;
	__u8  data[VIDEODEC_DATA_MAX_LEN];
	__u32 data_len;
};

struct dmabuf_manage_buffer {
	__u32 type;
	__u32 fd;
	__u32 paddr;
	__u32 size;
	__u32 handle;
	__u32 flags;
	__u32 extend;
	union {
		struct dmabuf_dmx_sec_es_data dmxes;
		struct dmabuf_videodec_es_data vdecdata;
		__u8 data[DMA_BUF_MANAGER_MAX_BUFFER_LEN];
	} buffer;
};

#define DMABUF_ALLOC_FROM_CMA   1

struct dmabuf_manage_block {
	__u32 paddr;
	__u32 size;
	__u32 handle;
	__u32 type;
	__u32 flags;
	void *priv;
};

#define SECURE_HEAP_DEFAULT_VERSION			1
#define SECURE_HEAP_SUPPORT_DELAY_ALLOC_VERSION		2
#define SECURE_HEAP_SUPPORT_MULTI_POOL_VERSION		3
#define SECURE_HEAP_MAX_VERSION				4

phys_addr_t secure_block_alloc(unsigned long size, unsigned long maxsize, unsigned long id);
unsigned long secure_block_free(phys_addr_t addr, unsigned long size);
unsigned int dmabuf_manage_get_can_alloc_blocknum(unsigned long id, unsigned long maxsize,
	unsigned long predictedsize, unsigned long paddr);
unsigned int dmabuf_manage_get_allocated_blocknum(unsigned long id);
unsigned int dmabuf_manage_get_secure_heap_version(void);
unsigned int dmabuf_manage_get_type(struct dma_buf *dbuf);
void *dmabuf_manage_get_info(struct dma_buf *dbuf, unsigned int type);

bool dmabuf_is_esbuf(struct dma_buf *dmabuf);

#define DMABUF_MANAGE_IOC_MAGIC			'S'

#define DMABUF_MANAGE_EXPORT_DMA		_IOWR(DMABUF_MANAGE_IOC_MAGIC, 1, int)
#define DMABUF_MANAGE_GET_HANDLE		_IOWR(DMABUF_MANAGE_IOC_MAGIC, 2, int)
#define DMABUF_MANAGE_GET_PHYADDR		_IOWR(DMABUF_MANAGE_IOC_MAGIC, 3, int)
#define DMABUF_MANAGE_IMPORT_DMA		_IOWR(DMABUF_MANAGE_IOC_MAGIC, 4, int)
#define DMABUF_MANAGE_SET_VDEC_INFO		_IOWR(DMABUF_MANAGE_IOC_MAGIC, 5, int)
#define DMABUF_MANAGE_CLEAR_VDEC_INFO	_IOWR(DMABUF_MANAGE_IOC_MAGIC, 6, int)

#define DMABUF_MANAGE_VERSION			_IOWR(DMABUF_MANAGE_IOC_MAGIC, 1000, int)
#define DMABUF_MANAGE_EXPORT_DMABUF		_IOWR(DMABUF_MANAGE_IOC_MAGIC, 1001, int)
#define DMABUF_MANAGE_GET_DMABUFINFO	_IOWR(DMABUF_MANAGE_IOC_MAGIC, 1002, int)
#define DMABUF_MANAGE_SET_FILTERFD		_IOWR(DMABUF_MANAGE_IOC_MAGIC, 1003, int)
#define DMABUF_MANAGE_ALLOCDMABUF		_IOWR(DMABUF_MANAGE_IOC_MAGIC, 1004, int)
#define DMABUF_MANAGE_FREEDMABUF		_IOWR(DMABUF_MANAGE_IOC_MAGIC, 1005, int)
#endif /* _LINUX_DMABUF_MANAGE_H_ */
