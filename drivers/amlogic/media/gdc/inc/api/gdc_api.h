/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __GDC_API_H__
#define __GDC_API_H__

#include <linux/of_address.h>
#include <linux/amlogic/media/gdc/gdc.h>

enum gdc_memtype_s {
	AML_GDC_MEM_ION,
	AML_GDC_MEM_DMABUF,
	AML_GDC_MEM_INVALID,
};

struct gdc_buf_cfg {
	u32 type;
	unsigned long len;
};

struct gdc_buffer_info {
	unsigned int mem_alloc_type;
	unsigned int plane_number;
	union {
		unsigned int y_base_fd;
		unsigned int shared_fd;
	};
	union {
		unsigned int uv_base_fd;
		unsigned int u_base_fd;
	};
	unsigned int v_base_fd;
};

// overall gdc settings and state
struct gdc_settings {
	u32 magic;
	//writing/reading to gdc base address, currently not read by api
	u32 base_gdc;
	 //array of gdc configuration and sizes
	struct gdc_config_s gdc_config;
	//update this index for new config
	//int gdc_config_total;
	//start memory to write gdc output frames
	u32 buffer_addr;
	//size of memory output frames to determine
	//if it is enough and can do multiple write points
	u32 buffer_size;
	//current output address of gdc
	u32 current_addr;
	//set when expecting an interrupt from gdc
	s32 is_waiting_gdc;

	s32 in_fd;  //input buffer's share fd
	s32 out_fd; //output buffer's share fd

	//input address for y and u, v planes
	u32 y_base_addr;
	union {
		u32 uv_base_addr;
		u32 u_base_addr;
	};
	u32 v_base_addr;
	//opaque address in ddr added with offset to
	//write the gdc config sequence
	void *ddr_mem;
	//when initialised this callback will be called
	//to update frame buffer addresses and offsets
	void (*get_frame_buffer)(u32 y_base_addr,
				 u32 uv_base_addr,
				 u32 y_line_offset,
				 u32 uv_line_offset);
	void *fh;
	s32 y_base_fd;
	union {
		s32 uv_base_fd;
		s32 u_base_fd;
	};
	s32 v_base_fd;
};

struct gdc_settings_ex {
	u32 magic;
	struct gdc_config_s gdc_config;
	struct gdc_buffer_info input_buffer;
	struct gdc_buffer_info config_buffer;
	struct gdc_buffer_info output_buffer;
};

/* for gdc dma buf define */
struct gdc_dmabuf_req_s {
	int index;
	unsigned int len;
	unsigned int dma_dir;
};

struct gdc_dmabuf_exp_s {
	int index;
	unsigned int flags;
	int fd;
};

/* end of gdc dma buffer define */

#define GDC_IOC_MAGIC  'G'
#define GDC_PROCESS	 _IOW(GDC_IOC_MAGIC, 0x00, struct gdc_settings)
#define GDC_PROCESS_NO_BLOCK	_IOW(GDC_IOC_MAGIC, 0x01, struct gdc_settings)
#define GDC_RUN	_IOW(GDC_IOC_MAGIC, 0x02, struct gdc_settings)
#define GDC_REQUEST_BUFF _IOW(GDC_IOC_MAGIC, 0x03, struct gdc_settings)
#define GDC_HANDLE _IOW(GDC_IOC_MAGIC, 0x04, struct gdc_settings)

#define GDC_PROCESS_EX _IOW(GDC_IOC_MAGIC, 0x05, struct gdc_settings_ex)
#define GDC_REQUEST_DMA_BUFF _IOW(GDC_IOC_MAGIC, 0x06, struct gdc_dmabuf_req_s)
#define GDC_EXP_DMA_BUFF _IOW(GDC_IOC_MAGIC, 0x07, struct gdc_dmabuf_exp_s)
#define GDC_FREE_DMA_BUFF _IOW(GDC_IOC_MAGIC, 0x08, int)
#define GDC_SYNC_DEVICE _IOW(GDC_IOC_MAGIC, 0x09, int)
#define GDC_SYNC_CPU _IOW(GDC_IOC_MAGIC, 0x0a, int)
#define GDC_PROCESS_WITH_FW _IOW(GDC_IOC_MAGIC, 0x0b, \
					struct gdc_settings_with_fw)
#define GDC_GET_VERSION _IOR(GDC_IOC_MAGIC, 0x0c, int)

enum {
	INPUT_BUFF_TYPE = 0x1000,
	OUTPUT_BUFF_TYPE,
	CONFIG_BUFF_TYPE,
	GDC_BUFF_TYPE_MAX
};

enum {
	EQUISOLID = 1,
	CYLINDER,
	EQUIDISTANT,
	CUSTOM,
	AFFINE,
	FW_TYPE_MAX
};

/* path: "/vendor/lib/firmware/gdc/" */
#define FIRMWARE_DIR "gdc"

struct fw_equisolid_s {
	/* float */
	char strength_x[8];
	/* float */
	char strength_y[8];
	int rotation;
};

struct fw_cylinder_s {
	/* float */
	char strength[8];
	int rotation;
};

struct fw_equidistant_s {
	/* float */
	char azimuth[8];
	int elevation;
	int rotation;
	int fov_width;
	int fov_height;
	bool keep_ratio;
	int cylindricity_x;
	int cylindricity_y;
};

struct fw_custom_s {
	char *fw_name;
};

struct fw_affine_s {
	int rotation;
};

struct fw_input_info_s {
	int with;
	int height;
	int fov;
	int diameter;
	int offset_x;
	int offset_y;
};

union transform_u {
	struct fw_equisolid_s fw_equisolid;
	struct fw_cylinder_s fw_cylinder;
	struct fw_equidistant_s fw_equidistant;
	struct fw_custom_s fw_custom;
	struct fw_affine_s fw_affine;
};

struct fw_output_info_s {
	int offset_x;
	int offset_y;
	int width;
	int height;
	union transform_u trans;
	int pan;
	int tilt;
	/* float*/
	char zoom[8];
};

struct firmware_info {
	unsigned int format;
	unsigned int trans_size_type;
	char *file_name;
	phys_addr_t phys_addr;
	void __iomem *virt_addr;
	unsigned int size;
	struct page *cma_pages;
	unsigned int loaded;
};

struct fw_info_s {
	char *fw_name;
	int fw_type;
	struct page *cma_pages;
	phys_addr_t phys_addr;
	void __iomem *virt_addr;
	struct fw_input_info_s fw_input_info;
	struct fw_output_info_s fw_output_info;
};

struct gdc_settings_with_fw {
	u32 magic;
	struct gdc_config_s gdc_config;
	struct gdc_buffer_info input_buffer;
	struct gdc_buffer_info reserved;
	struct gdc_buffer_info output_buffer;
	struct fw_info_s fw_info;
};

struct gdc_pd {
	/* powerdomain virtual device */
	struct device *dev;
	/* on:1 off:0 */
	u32 status;
};

/**
 *   Configure the output gdc configuration
 *
 *   address/size and buffer address/size; and resolution.
 *
 *   More than one gdc settings can be accessed by index to a gdc_config_t.
 *
 *   @param  gdc_cmd_s - overall gdc settings and state
 *   @param  gdc_config_num - selects the current gdc config to be applied
 *
 *   @return 0 - success
 *	 -1 - fail.
 */
int gdc_init(struct gdc_cmd_s *gdc_cmd, struct gdc_dma_cfg_t *dma_cfg,
	     u32 core_id);
/**
 *   This function stops the gdc block
 *
 *   @param  gdc_cmd_s - overall gdc settings and state
 *
 */
void gdc_stop(struct gdc_cmd_s *gdc_cmd, u32 core_id);

/**
 *   This function starts the gdc block
 *
 *   Writing 0->1 transition is necessary for trigger
 *
 *   @param  gdc_cmd_s - overall gdc settings and state
 *
 */
void gdc_start(struct gdc_cmd_s *gdc_cmd, u32 core_id);

/**
 *   This function points gdc to
 *
 *   its input resolution and yuv address and offsets
 *
 *   Shown inputs to GDC are Y and UV plane address and offsets
 *
 *   @param  gdc_cmd_s - overall gdc settings and state
 *   @param  active_width -  input width resolution
 *   @param  active_height - input height resolution
 *   @param  y_base_addr -  input Y base address
 *   @param  uv_base_addr - input UV base address
 *   @param  y_line_offset - input Y line buffer offset
 *   @param  uv_line_offset-  input UV line buffer offer
 *
 *   @return 0 - success
 *	 -1 - no interrupt from GDC.
 */
int gdc_process(struct gdc_cmd_s *gdc_cmd,
		u32 y_base_addr,
		u32 uv_base_addr,
		struct gdc_dma_cfg_t *dma_cfg,
		u32 core_id);
int gdc_process_yuv420p(struct gdc_cmd_s *gdc_cmd,
			u32 y_base_addr,
			u32 u_base_addr,
			u32 v_base_addr,
			struct gdc_dma_cfg_t *dma_cfg,
			u32 core_id);
int gdc_process_y_grey(struct gdc_cmd_s *gdc_cmd,
		       u32 y_base_addr,
		       struct gdc_dma_cfg_t *dma_cfg,
		       u32 core_id);
int gdc_process_yuv444p(struct gdc_cmd_s *gdc_cmd,
			u32 y_base_addr,
			u32 u_base_addr,
			u32 v_base_addr,
			struct gdc_dma_cfg_t *dma_cfg,
			u32 core_id);
int gdc_process_rgb444p(struct gdc_cmd_s *gdc_cmd,
			u32 y_base_addr,
			u32 u_base_addr,
			u32 v_base_addr,
			struct gdc_dma_cfg_t *dma_cfg,
			u32 core_id);

/**
 *   This function gets the GDC output frame addresses
 *
 *   and offsets and updates the frame buffer via callback
 *
 *   if it is available Shown ouputs to GDC are
 *
 *   Y and UV plane address and offsets
 *
 *   @param  gdc_cmd_s - overall gdc settings and state
 *
 *   @return 0 - success
 *	 -1 - unexpected interrupt from GDC.
 */
int gdc_get_frame(struct gdc_cmd_s *gdc_cmd);

/**
 *   This function points gdc to its input resolution
 *
 *   and yuv address and offsets
 *
 *   Shown inputs to GDC are Y and UV plane address and offsets
 *
 *   @param  gdc_cmd_s - overall gdc settings and state
 *
 *   @return 0 - success
 *	 -1 - no interrupt from GDC.
 */
int gdc_run(struct gdc_cmd_s *g, struct gdc_dma_cfg_t *dma_cfg, u32 core_id);

s32 init_gdc_io(struct device_node *dn, u32 dev_type);

int gdc_pwr_init(struct device *dev, struct gdc_pd *pd, u32 dev_type);

int gdc_pwr_config(bool enable, u32 dev_type, u32 core_id);

void gdc_pwr_remove(struct gdc_pd *pd);

void gdc_runtime_pwr_all(u32 dev_type, bool enable);

void gdc_clk_config_all(u32 dev_type, bool enable);
#endif
