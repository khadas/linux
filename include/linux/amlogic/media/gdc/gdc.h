/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _GDC_H_
#define _GDC_H_

#include <linux/types.h>
#include <linux/wait.h>
#include <linux/dma-direction.h>
#include <linux/dma-buf.h>

#define CONFIG_PATH_LENG 128
#define GDC_MAX_PLANE         3
#define WORD_SIZE 16
#define WORD_MASK (~(WORD_SIZE - 1))
#define AXI_WORD_ALIGN(size) (((size) + WORD_SIZE - 1) & WORD_MASK)

enum {
	ARM_GDC,
	AML_GDC,
	HW_TYPE
};

struct gdc_linear_config_s {
	void *buf_vaddr;
	int buf_size;
	dma_addr_t dma_addr;
};

struct aml_dma_cfg {
	int fd;
	void *dev;
	void *vaddr;
	struct dma_buf *dbuf;
	struct dma_buf_attachment *attach;
	struct sg_table *sg;
	enum dma_data_direction dir;
	int is_config_buf;
	struct gdc_linear_config_s linear_config;
};

struct gdc_dmabuf_cfg_s {
	int dma_used;
	u32 paddr_8g_msb;
	struct aml_dma_cfg dma_cfg;
};

struct gdc_dma_cfg_t {
	struct gdc_dmabuf_cfg_s config_cfg;
	struct gdc_dmabuf_cfg_s input_cfg[GDC_MAX_PLANE];
	struct gdc_dmabuf_cfg_s output_cfg[GDC_MAX_PLANE];
};

struct gdc_dma_cfg {
	int fd;
	void *dev;
	void *vaddr;
	struct dma_buf *dbuf;
	struct dma_buf_attachment *attach;
	struct sg_table *sg;
	enum dma_data_direction dir;
};

/* each configuration addresses and size */
struct gdc_config_s {
	u32 format;
	u32 config_addr;     /* gdc config address */
	u32 config_size;     /* gdc config size in 32bit */
	u32 input_width;     /* gdc input width resolution */
	u32 input_height;    /* gdc input height resolution */
	u32 input_y_stride;  /* gdc input y stride resolution */
	u32 input_c_stride;  /* gdc input uv stride */
	u32 output_width;    /* gdc output width resolution */
	u32 output_height;   /* gdc output height resolution */
	u32 output_y_stride; /* gdc output y stride */
	u32 output_c_stride; /* gdc output uv stride */
};

struct gdc_cmd_s {
	u32 outplane;
	/* writing/reading to gdc base address, currently not read by api */
	u32 base_gdc;
	/* array of gdc configuration and sizes */
	struct gdc_config_s gdc_config;
	/* update this index for new config */
	/* int gdc_config_total; */
	/* start memory to write gdc output framse */
	u32 buffer_addr;
	/* size of memory output frames to determine */
	/* if it is enough and can do multiple write points */
	u32 buffer_size;
	/* current output address of gdc */
	u32 current_addr;
	/* output address for  u, v planes */
	union {
		u32 uv_out_base_addr;
		u32 u_out_base_addr;
	};
	u32 v_out_base_addr;
	/* set when expecting an interrupt from gdc */
	s32 is_waiting_gdc;
	/* input address for y and u, v planes */
	u32 y_base_addr;
	union {
		u32 uv_base_addr;
		u32 u_base_addr;
	};
	u32 v_base_addr;
	unsigned char wait_done_flag;
	/* ARM_GDC or AML_GDC */
	u32 dev_type;
	/* secure mem access */
	u32 use_sec_mem;
};

struct gdc_context_s {
	/* connect all process in one queue for RR process. */
	struct list_head   list;
	/* current wq configuration */
	u32 mmap_type;
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
	struct mutex d_mutext; /* for config context */

	struct gdc_cmd_s cmd;
	struct list_head work_queue;
	struct list_head free_queue;
	wait_queue_head_t cmd_complete;
	int gdc_request_exit;
	spinlock_t lock; /* for get and release item. */
};

/*
 * two ways to load config bin.
 * 1. loaded already, set use_builtin_fw 0, set config_paddr and config_size.
 * 2. load through gdc driver, set use_builtin_fw 1, set config_name.
 */
struct gdc_phy_setting {
	u32 format;
	u32 in_width;
	u32 in_height;
	u32 out_width;
	u32 out_height;
	u32 in_plane_num;
	u32 out_plane_num;
	ulong in_paddr[GDC_MAX_PLANE];
	ulong out_paddr[GDC_MAX_PLANE];
	ulong config_paddr;
	u32 config_size; /* in 32bit */
	u32 use_builtin_fw;
	u32 use_sec_mem; /* secure mem access */
	char config_name[CONFIG_PATH_LENG];
};

bool is_gdc_supported(void);
bool is_aml_gdc_supported(void);
struct gdc_context_s *create_gdc_work_queue(u32 dev_type);
int gdc_process_phys(struct gdc_context_s *context,
		     struct gdc_phy_setting *gs);
int destroy_gdc_work_queue(struct gdc_context_s *gdc_work_queue);

#endif
