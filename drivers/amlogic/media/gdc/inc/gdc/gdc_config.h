/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __GDC_CONFIG_H__
#define __GDC_CONFIG_H__

#include <linux/interrupt.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/miscdevice.h>
#include <linux/highmem.h>
#ifdef CONFIG_AMLOGIC_TEE
#include <linux/amlogic/tee.h>
#endif
#include "system_gdc_io.h"
#include "gdc_api.h"
#include "gdc_dmabuf.h"
#include "system_log.h"

struct gdc_context_s;

enum {
	CORE_AXI,
	MUXGATE_MUXSEL_GATE,
	GATE
};

enum {
	CORE,
	AXI,
	MUX_GATE,
	MUX_SEL,
	CLK_GATE,
	CLK_NAME_NUM
};

enum {
	CORE_0,
	CORE_1,
	CORE_2,
	CORE_NUM
};

struct gdc_device_data_s {
	int dev_type;
	int clk_type;
	/* use independ reg to set 8g addr MSB */
	int ext_msb_8g;
	int bit_width_ext; /* 8/10/12/16bit support */
	int gamma_support; /* gamma support */
	int core_cnt;      /* total core count */
	int smmu_support;  /* smmu support */
};

struct meson_gdc_dev_t {
	struct miscdevice misc_dev;
	int irq;
	int probed;
	struct platform_device *pdev;
	char *config_out_file;
	int config_out_path_defined;
	int trace_mode_enable;
	int reg_store_mode_enable;
	int clk_type;
	union {
		struct clk *clk_core[CORE_NUM];
		struct clk *clk_gate[CORE_NUM];
	};
	struct clk *clk_axi[CORE_NUM]; /* not used for clk_gate only cases */
	int ext_msb_8g;
	int bit_width_ext;
	int gamma_support;
	int core_cnt;
	struct gdc_pd pd[CORE_NUM];
	u32 is_idle[CORE_NUM]; /* indicate the core status, 0:busy 1:idle */
	ktime_t time_stamp[CORE_NUM]; /* start time stamp */
	struct gdc_queue_item_s *current_item[CORE_NUM];
};

struct gdc_event_s {
	struct completion d_com;
	struct completion process_complete[CORE_NUM];
	/* for queue switch and create destroy queue. */
	spinlock_t sem_lock;
	struct semaphore cmd_in_sem;
};

struct gdc_manager_s {
	struct list_head process_queue;
	struct gdc_context_s *current_wq;
	struct gdc_context_s *last_wq;
	struct task_struct *gdc_thread;
	struct gdc_event_s event;
	struct aml_dma_buffer *buffer;
	int gdc_state;
	int process_queue_state; //thread running flag
	struct meson_gdc_dev_t *gdc_dev;
	struct meson_gdc_dev_t *aml_gdc_dev;
};

struct gdc_irq_data_s {
	u32 dev_type;
	u32 core_id;
};

extern struct gdc_manager_s gdc_manager;
extern int gdc_smmu_enable;

#define GDC_DEVICE(dev_type) ((dev_type) == ARM_GDC ?             \
			      &gdc_manager.gdc_dev->pdev->dev :   \
			      &gdc_manager.aml_gdc_dev->pdev->dev)

#define GDC_DEV_T(dev_type) ((dev_type) == ARM_GDC ?   \
			     gdc_manager.gdc_dev :     \
			     gdc_manager.aml_gdc_dev)

/* AML GDC registers */
#define ISP_DWAP_REG_MARK                  BIT(16)
#define ISP_DWAP_TOP_SRC_FSIZE             ((0x00 << 2) | ISP_DWAP_REG_MARK)
#define ISP_DWAP_TOP_CTRL0                 ((0x02 << 2) | ISP_DWAP_REG_MARK)
#define ISP_DWAP_TOP_COEF_CTRL0            ((0x03 << 2) | ISP_DWAP_REG_MARK)
#define ISP_DWAP_TOP_COEF_CTRL1            ((0x04 << 2) | ISP_DWAP_REG_MARK)
#define ISP_DWAP_TOP_CMD_CTRL0             ((0x05 << 2) | ISP_DWAP_REG_MARK)
#define ISP_DWAP_TOP_CMD_CTRL1             ((0x06 << 2) | ISP_DWAP_REG_MARK)
#define ISP_DWAP_TOP_SRC_Y_CTRL0           ((0x07 << 2) | ISP_DWAP_REG_MARK)
#define ISP_DWAP_TOP_SRC_Y_CTRL1           ((0x08 << 2) | ISP_DWAP_REG_MARK)
#define ISP_DWAP_TOP_SRC_U_CTRL0           ((0x09 << 2) | ISP_DWAP_REG_MARK)
#define ISP_DWAP_TOP_SRC_U_CTRL1           ((0x0a << 2) | ISP_DWAP_REG_MARK)
#define ISP_DWAP_TOP_SRC_V_CTRL0           ((0x0b << 2) | ISP_DWAP_REG_MARK)
#define ISP_DWAP_TOP_SRC_V_CTRL1           ((0x0c << 2) | ISP_DWAP_REG_MARK)
#define ISP_DWAP_TOP_MESH_CTRL0            ((0x0d << 2) | ISP_DWAP_REG_MARK)
#define ISP_DWAP_TOP_DST_FSIZE             ((0x10 << 2) | ISP_DWAP_REG_MARK)
#define ISP_DWAP_TOP_DST_Y_CTRL0           ((0x13 << 2) | ISP_DWAP_REG_MARK)
#define ISP_DWAP_TOP_DST_Y_CTRL1           ((0x14 << 2) | ISP_DWAP_REG_MARK)
#define ISP_DWAP_TOP_DST_U_CTRL0           ((0x15 << 2) | ISP_DWAP_REG_MARK)
#define ISP_DWAP_TOP_DST_U_CTRL1           ((0x16 << 2) | ISP_DWAP_REG_MARK)
#define ISP_DWAP_TOP_DST_V_CTRL0           ((0x17 << 2) | ISP_DWAP_REG_MARK)
#define ISP_DWAP_TOP_DST_V_CTRL1           ((0x18 << 2) | ISP_DWAP_REG_MARK)

#define ISP_DWAP_GAMMA_CTRL                ((0x60 << 2) | ISP_DWAP_REG_MARK)
#define ISP_DWAP_GAMMA_OFST                ((0x61 << 2) | ISP_DWAP_REG_MARK)
#define ISP_DWAP_GAMMA_NUM                 ((0x62 << 2) | ISP_DWAP_REG_MARK)
#define ISP_DWAP_GAMMA_STP                 ((0x63 << 2) | ISP_DWAP_REG_MARK)
#define ISP_DWAP_GAMMA_LUT_ADDR            ((0x64 << 2) | ISP_DWAP_REG_MARK)
#define ISP_DWAP_GAMMA_LUT_DATA            ((0x65 << 2) | ISP_DWAP_REG_MARK)

#define DEWARP_STRIDE_ALIGN(x) (((x) + 15) / 16)
#define AML_GDC_COEF_SIZE  256
#define AML_GDC_CFG_STRIDE 4096

// ----------------------------------- //
// Instance 'gdc' of module 'gdc_ip_config'
// ----------------------------------- //

#define GDC_BASE_ADDR (0x00L)
#define GDC_SIZE (0x100)

// ----------------------------------- //
// Group: ID
// ----------------------------------- //

// ----------------------------------- //
// Register: API
// ----------------------------------- //

#define GDC_ID_API_DEFAULT (0x0)
#define GDC_ID_API_DATASIZE (32)
#define GDC_ID_API_OFFSET (0x0)
#define GDC_ID_API_MASK (0xffffffff)

// args: data (32-bit)
static inline u32 gdc_id_api_read(void)
{
	return system_gdc_read_32(0x00L, 0);
}

// ----------------------------------- //
// Register: Product
// ----------------------------------- //

#define GDC_ID_PRODUCT_DEFAULT (0x0)
#define GDC_ID_PRODUCT_DATASIZE (32)
#define GDC_ID_PRODUCT_OFFSET (0x4)
#define GDC_ID_PRODUCT_MASK (0xffffffff)

// args: data (32-bit)
static inline u32 gdc_id_product_read(void)
{
	return system_gdc_read_32(0x04L, 0);
}

// ----------------------------------- //
// Register: Version
// ----------------------------------- //

#define GDC_ID_VERSION_DEFAULT (0x0)
#define GDC_ID_VERSION_DATASIZE (32)
#define GDC_ID_VERSION_OFFSET (0x8)
#define GDC_ID_VERSION_MASK (0xffffffff)

// args: data (32-bit)
static inline u32 gdc_id_version_read(void)
{
	return system_gdc_read_32(0x08L, 0);
}

// ----------------------------------- //
// Register: Revision
// ----------------------------------- //

#define GDC_ID_REVISION_DEFAULT (0x0)
#define GDC_ID_REVISION_DATASIZE (32)
#define GDC_ID_REVISION_OFFSET (0xc)
#define GDC_ID_REVISION_MASK (0xffffffff)

// args: data (32-bit)
static inline u32 gdc_id_revision_read(void)
{
	return system_gdc_read_32(0x0cL, 0);
}

// ----------------------------------- //
// Group: GDC
// ----------------------------------- //
// ----------------------------------- //
// GDC controls
// ----------------------------------- //
// ----------------------------------- //
// Register: config addr
// ----------------------------------- //
// ----------------------------------- //
// Base address of configuration stream
// (in bytes, AXI word aligned)
// ----------------------------------- //

#define GDC_CONFIG_ADDR_DEFAULT (0x0)
#define GDC_CONFIG_ADDR_DATASIZE (32)
#define GDC_CONFIG_ADDR_OFFSET (0x10)
#define GDC_CONFIG_ADDR_MASK (0xffffffff)

static inline void gdc_coef_addr_write(ulong data, u32 core_id)
{
	gdc_log(LOG_DEBUG, "coef paddr: 0x%lx\n", data);
	system_gdc_write_32(ISP_DWAP_TOP_COEF_CTRL0, data >> 4, core_id);
	system_gdc_write_32(ISP_DWAP_TOP_COEF_CTRL1, AML_GDC_COEF_SIZE,
			    core_id);
}

static inline void gdc_aml_cfg_addr_write(ulong data, u32 core_id)
{
	u32 curr = system_gdc_read_32(ISP_DWAP_TOP_CMD_CTRL1, core_id);

	gdc_log(LOG_DEBUG, " cfg paddr: 0x%lx\n", data);
	system_gdc_write_32(ISP_DWAP_TOP_CMD_CTRL0, data >> 4, core_id);
	system_gdc_write_32(ISP_DWAP_TOP_CMD_CTRL1, AML_GDC_CFG_STRIDE | curr,
			    core_id);
}

static inline void gdc_mesh_addr_write(ulong data, u32 core_id)
{
	gdc_log(LOG_DEBUG, "mesh paddr: 0x%lx\n", data);
	system_gdc_write_32(ISP_DWAP_TOP_MESH_CTRL0, data >> 4, core_id);
}

static inline void gdc_bit_width_write(u32 format, u32 core_id)
{
	u32 curr = 0;
	u32 in_bitw = 0;
	u32 out_bitw = 0;

	switch (format & FORMAT_IN_BITW_MASK) {
	case IN_BITW_8:
		in_bitw  = 0;
		break;
	case IN_BITW_10:
		in_bitw  = 1;
		break;
	case IN_BITW_12:
		in_bitw  = 2;
		break;
	case IN_BITW_16:
		in_bitw  = 3;
		break;
	default:
		gdc_log(LOG_ERR, "%s, format (0x%x) in_bitw is wrong\n",
			__func__, format);
	}

	switch (format & FORMAT_OUT_BITW_MASK) {
	case OUT_BITW_8:
		out_bitw  = 0;
		break;
	case OUT_BITW_10:
		out_bitw  = 1;
		break;
	case OUT_BITW_12:
		out_bitw  = 2;
		break;
	case OUT_BITW_16:
		out_bitw  = 3;
		break;
	default:
		gdc_log(LOG_ERR, "%s, format (0x%x) out_bitw is wrong\n",
			__func__, format);
	}

	gdc_log(LOG_DEBUG, "in bit width:%d\n", in_bitw);
	gdc_log(LOG_DEBUG, "out bit width:%d\n", out_bitw);

	curr = system_gdc_read_32(ISP_DWAP_TOP_CMD_CTRL1, core_id);
	system_gdc_write_32(ISP_DWAP_TOP_CMD_CTRL1,
			    (curr & 0x3fffffff) | (in_bitw << 30), core_id);

	curr = system_gdc_read_32(ISP_DWAP_GAMMA_CTRL, core_id);
	system_gdc_write_32(ISP_DWAP_GAMMA_CTRL,
			    (curr & 0xfffffff3) | (out_bitw << 2), core_id);
}

// args: data (32-bit)
static inline void gdc_config_addr_write(struct gdc_dmabuf_cfg_s *cfg, u32 data, u32 dev_type,
					 u32 core_id)
{
	u32 msb = cfg->paddr_8g_msb;

	if (dev_type == ARM_GDC) {
		system_gdc_write_32(0x10L, data, core_id);
	} else {
		if (gdc_smmu_enable) {
			ulong fw_addr = ((u64)msb << 32) + data;
			void *vaddr = cfg->dma_cfg.linear_config.buf_vaddr;
			u32 coef_size = *(u32 *)vaddr;
			u32 mesh_size = *((u32 *)vaddr + 1);
			u32 fw_offset = *((u32 *)vaddr + 2);

			fw_addr += fw_offset;

			gdc_coef_addr_write(fw_addr, core_id);
			gdc_mesh_addr_write(fw_addr + coef_size, core_id);
			gdc_aml_cfg_addr_write(fw_addr + coef_size + mesh_size,
					       core_id);
			gdc_log(LOG_DEBUG, "   coef_size: 0x%x %u\n",
				coef_size, coef_size);
			gdc_log(LOG_DEBUG, "   mesh_size: 0x%x %u\n",
				mesh_size, mesh_size);
			gdc_log(LOG_DEBUG, "   fw offset: 0x%x %u\n",
				fw_offset, fw_offset);
		} else {
			ulong fw_addr = ((u64)msb << 32) + data;
			struct page *page = phys_to_page(fw_addr);
			void *vaddr = kmap(page);
			u32 coef_size = *(u32 *)vaddr;
			u32 mesh_size = *((u32 *)vaddr + 1);
			u32 fw_offset = *((u32 *)vaddr + 2);

			fw_addr += fw_offset;

			gdc_coef_addr_write(fw_addr, core_id);
			gdc_mesh_addr_write(fw_addr + coef_size, core_id);
			gdc_aml_cfg_addr_write(fw_addr + coef_size + mesh_size,
					       core_id);

			kunmap(page);
			gdc_log(LOG_DEBUG, "   coef_size: 0x%x %u\n",
				coef_size, coef_size);
			gdc_log(LOG_DEBUG, "   mesh_size: 0x%x %u\n",
				mesh_size, mesh_size);
			gdc_log(LOG_DEBUG, "   fw offset: 0x%x %u\n",
				fw_offset, fw_offset);
		}
	}
}

static inline u32 gdc_config_addr_read(void)
{
	return system_gdc_read_32(0x10L, 0);
}

// ----------------------------------- //
// Register: config size
// ----------------------------------- //
// ----------------------------------- //
// Size of the configuration stream	//
// (in bytes, 32 bit word granularity) //
// ----------------------------------- //

#define GDC_CONFIG_SIZE_DEFAULT (0x0)
#define GDC_CONFIG_SIZE_DATASIZE (32)
#define GDC_CONFIG_SIZE_OFFSET (0x14)
#define GDC_CONFIG_SIZE_MASK (0xffffffff)

// args: data (32-bit)
static inline void gdc_config_size_write(u32 data, u32 dev_type, u32 core_id)
{
	if (dev_type == ARM_GDC)
		system_gdc_write_32(0x14L, data, core_id);
}

static inline u32 gdc_config_size_read(void)
{
	return system_gdc_read_32(0x14L, 0);
}

// ----------------------------------- //
// Register: datain width
// ----------------------------------- //

// ----------------------------------- //
// Width of the input image (in pixels)
// ----------------------------------- //

#define GDC_DATAIN_WIDTH_DEFAULT (0x0)
#define GDC_DATAIN_WIDTH_DATASIZE (16)
#define GDC_DATAIN_WIDTH_OFFSET (0x20)
#define GDC_DATAIN_WIDTH_MASK (0xffff)

// args: data (16-bit)
static inline void gdc_datain_width_write(u16 data, u32 dev_type, u32 core_id)
{
	u32 curr;

	if (dev_type == ARM_GDC) {
		curr = system_gdc_read_32(0x20L, core_id);
		system_gdc_write_32(0x20L, ((curr & 0xffff0000) | data),
				    core_id);
	} else {
		curr = system_gdc_read_32(ISP_DWAP_TOP_SRC_FSIZE, core_id);
		system_gdc_write_32(ISP_DWAP_TOP_SRC_FSIZE,
				    ((curr & 0x0000ffff) | (data << 16)),
				    core_id);
	}
}

static inline uint16_t gdc_datain_width_read(void)
{
	return (uint16_t)((system_gdc_read_32(0x20L, 0) & 0xffff) >> 0);
}

// ----------------------------------- //
// Register: datain_height
// ----------------------------------- //

// ----------------------------------- //
// Height of the input image (in pixels)
// ----------------------------------- //

#define GDC_DATAIN_HEIGHT_DEFAULT (0x0)
#define GDC_DATAIN_HEIGHT_DATASIZE (16)
#define GDC_DATAIN_HEIGHT_OFFSET (0x24)
#define GDC_DATAIN_HEIGHT_MASK (0xffff)

// args: data (16-bit)
static inline void gdc_datain_height_write(u16 data, u32 dev_type, u32 core_id)
{
	u32 curr = 0;

	if (dev_type == ARM_GDC) {
		curr = system_gdc_read_32(0x24L, core_id);
		system_gdc_write_32(0x24L, ((curr & 0xffff0000) | data),
				    core_id);
	} else {
		curr = system_gdc_read_32(ISP_DWAP_TOP_SRC_FSIZE, core_id);
		system_gdc_write_32(ISP_DWAP_TOP_SRC_FSIZE,
				    ((curr & 0xffff0000) | data),
				    core_id);
	}
}

static inline uint16_t gdc_datain_height_read(void)
{
	return (uint16_t)((system_gdc_read_32(0x24L, 0) & 0xffff) >> 0);
}

// ----------------------------------- //
// Register: data1in addr
// ----------------------------------- //

// ----------------------------------- //
// Base address of the 1st plane in the
// input frame buffer
// (in bytes, AXI word aligned)
// ----------------------------------- //

#define GDC_DATA1IN_ADDR_DEFAULT (0x0)
#define GDC_DATA1IN_ADDR_DATASIZE (32)
#define GDC_DATA1IN_ADDR_OFFSET (0x28)
#define GDC_DATA1IN_ADDR_MASK (0xffffffff)

// args: data (32-bit)
static inline void gdc_data1in_addr_write(u32 msb, u32 data, u32 dev_type,
					  u32 core_id)
{
	if (dev_type == ARM_GDC)
		system_gdc_write_32(0x28L, data, core_id);
	else
		system_gdc_write_32(ISP_DWAP_TOP_SRC_Y_CTRL0,
				    (((u64)msb << 32) + data) >> 4, core_id);
}

static inline u32 gdc_data1in_addr_read(void)
{
	return system_gdc_read_32(0x28L, 0);
}

// ----------------------------------- //
// Register: data1in line offset
// ----------------------------------- //

// ----------------------------------- //
// Address difference between adjacent
// lines for the 1st plane in the input
// frame buffer (in bytes, AXI word aligned)
// ----------------------------------- //

#define GDC_DATA1IN_LINE_OFFSET_DEFAULT (0x0)
#define GDC_DATA1IN_LINE_OFFSET_DATASIZE (32)
#define GDC_DATA1IN_LINE_OFFSET_OFFSET (0x2c)
#define GDC_DATA1IN_LINE_OFFSET_MASK (0xffffffff)

// args: data (32-bit)
static inline void gdc_data1in_line_offset_write(u32 data, u32 dev_type,
						 u32 core_id)
{
	if (dev_type == ARM_GDC) {
		system_gdc_write_32(0x2cL, data, core_id);
	} else {
		data = DEWARP_STRIDE_ALIGN(data);
		system_gdc_write_32(ISP_DWAP_TOP_SRC_Y_CTRL1, data, core_id);
	}
}

static inline u32 gdc_data1in_line_offset_read(void)
{
	return system_gdc_read_32(0x2cL, 0);
}

// ----------------------------------- //
// Register: data2in addr
// ----------------------------------- //

// ----------------------------------- //
// Address of the 2nd plane in the
// input frame buffer
// (in bytes, AXI word aligned)
// ----------------------------------- //

#define GDC_DATA2IN_ADDR_DEFAULT (0x0)
#define GDC_DATA2IN_ADDR_DATASIZE (32)
#define GDC_DATA2IN_ADDR_OFFSET (0x30)
#define GDC_DATA2IN_ADDR_MASK (0xffffffff)

// args: data (32-bit)
static inline void gdc_data2in_addr_write(u32 msb, u32 data, u32 dev_type,
					  u32 core_id)
{
	if (dev_type == ARM_GDC)
		system_gdc_write_32(0x30L, data, core_id);
	else
		system_gdc_write_32(ISP_DWAP_TOP_SRC_U_CTRL0,
				    (((u64)msb << 32) + data) >> 4, core_id);
}

static inline u32 gdc_data2in_addr_read(void)
{
	return system_gdc_read_32(0x30L, 0);
}

// ----------------------------------- //
// Register: data2in line offset
// ----------------------------------- //

// ----------------------------------- //
// Address difference between adjacent
// lines for the 2nd plane in the input
// frame buffer (in bytes, AXI word aligned)
// ----------------------------------- //

#define GDC_DATA2IN_LINE_OFFSET_DEFAULT (0x0)
#define GDC_DATA2IN_LINE_OFFSET_DATASIZE (32)
#define GDC_DATA2IN_LINE_OFFSET_OFFSET (0x34)
#define GDC_DATA2IN_LINE_OFFSET_MASK (0xffffffff)

// args: data (32-bit)
static inline void gdc_data2in_line_offset_write(u32 data, u32 dev_type,
						 u32 core_id)
{
	if (dev_type == ARM_GDC) {
		system_gdc_write_32(0x34L, data, core_id);
	} else {
		data = DEWARP_STRIDE_ALIGN(data);
		system_gdc_write_32(ISP_DWAP_TOP_SRC_U_CTRL1, data, core_id);
	}
}

static inline u32 gdc_data2in_line_offset_read(void)
{
	return system_gdc_read_32(0x34L, 0);
}

// ----------------------------------- //
// Register: data3in addr
// ----------------------------------- //

// ----------------------------------- //
// Base address of the 3rd plane in the
// input frame buffer
// (in bytes, AXI word aligned)
// ----------------------------------- //

#define GDC_DATA3IN_ADDR_DEFAULT (0x0)
#define GDC_DATA3IN_ADDR_DATASIZE (32)
#define GDC_DATA3IN_ADDR_OFFSET (0x38)
#define GDC_DATA3IN_ADDR_MASK (0xffffffff)

// args: data (32-bit)
static inline void gdc_data3in_addr_write(u32 msb, u32 data, u32 dev_type,
					  u32 core_id)
{
	if (dev_type == ARM_GDC)
		system_gdc_write_32(0x38L, data, core_id);
	else
		system_gdc_write_32(ISP_DWAP_TOP_SRC_V_CTRL0,
				    (((u64)msb << 32) + data) >> 4, core_id);
}

static inline u32 gdc_data3in_addr_read(void)
{
	return system_gdc_read_32(0x38L, 0);
}

// ----------------------------------- //
// Register: data3in line offset
// ----------------------------------- //

// ----------------------------------- //
// Address difference between adjacent
// lines for the 3rd plane in the
// input frame buffer (in bytes,
// AXI word aligned)
// ----------------------------------- //

#define GDC_DATA3IN_LINE_OFFSET_DEFAULT (0x0)
#define GDC_DATA3IN_LINE_OFFSET_DATASIZE (32)
#define GDC_DATA3IN_LINE_OFFSET_OFFSET (0x3c)
#define GDC_DATA3IN_LINE_OFFSET_MASK (0xffffffff)

// args: data (32-bit)
static inline void gdc_data3in_line_offset_write(u32 data, u32 dev_type,
						 u32 core_id)
{
	if (dev_type == ARM_GDC) {
		system_gdc_write_32(0x3cL, data, core_id);
	} else {
		data = DEWARP_STRIDE_ALIGN(data);
		system_gdc_write_32(ISP_DWAP_TOP_SRC_V_CTRL1, data, core_id);
	}
}

static inline u32 gdc_data3in_line_offset_read(void)
{
	return system_gdc_read_32(0x3cL, 0);
}

// ----------------------------------- //
// Register: dataout width
// ----------------------------------- //

// ----------------------------------- //
// Width of the output image (in pixels)
// ----------------------------------- //

#define GDC_DATAOUT_WIDTH_DEFAULT (0x0)
#define GDC_DATAOUT_WIDTH_DATASIZE (16)
#define GDC_DATAOUT_WIDTH_OFFSET (0x40)
#define GDC_DATAOUT_WIDTH_MASK (0xffff)

// args: data (16-bit)
static inline void gdc_dataout_width_write(u16 data, u32 dev_type, u32 core_id)
{
	u32 curr;

	if (dev_type == ARM_GDC) {
		curr = system_gdc_read_32(0x40L, core_id);
		system_gdc_write_32(0x40L, ((curr & 0xffff0000) | data),
				    core_id);
	} else {
		curr = system_gdc_read_32(ISP_DWAP_TOP_DST_FSIZE, core_id);
		system_gdc_write_32(ISP_DWAP_TOP_DST_FSIZE,
				    ((curr & 0x0000ffff) | (data << 16)),
				    core_id);
	}
}

static inline uint16_t gdc_dataout_width_read(void)
{
	return (uint16_t)((system_gdc_read_32(0x40L, 0) & 0xffff) >> 0);
}

// ----------------------------------- //
// Register: dataout height
// ----------------------------------- //

// ----------------------------------- //
// Height of the output image (in pixels)
// ----------------------------------- //

#define GDC_DATAOUT_HEIGHT_DEFAULT (0x0)
#define GDC_DATAOUT_HEIGHT_DATASIZE (16)
#define GDC_DATAOUT_HEIGHT_OFFSET (0x44)
#define GDC_DATAOUT_HEIGHT_MASK (0xffff)

// args: data (16-bit)
static inline void gdc_dataout_height_write(u16 data, u32 dev_type, u32 core_id)
{
	u32 curr;

	if (dev_type == ARM_GDC) {
		curr = system_gdc_read_32(0x44L, core_id);
		system_gdc_write_32(0x44L, ((curr & 0xffff0000) | data),
				    core_id);
	} else {
		curr = system_gdc_read_32(ISP_DWAP_TOP_DST_FSIZE, core_id);
		system_gdc_write_32(ISP_DWAP_TOP_DST_FSIZE,
				    ((curr & 0xffff0000) | data), core_id);
	}
}

static inline uint16_t gdc_dataout_height_read(void)
{
	return (uint16_t)((system_gdc_read_32(0x44L, 0) & 0xffff) >> 0);
}

// ----------------------------------- //
// Register: data1out addr
// ----------------------------------- //

// ----------------------------------- //
// Base address of the 1st plane in the
// output frame buffer
// (in bytes, AXI word aligned)
// ----------------------------------- //

#define GDC_DATA1OUT_ADDR_DEFAULT (0x0)
#define GDC_DATA1OUT_ADDR_DATASIZE (32)
#define GDC_DATA1OUT_ADDR_OFFSET (0x48)
#define GDC_DATA1OUT_ADDR_MASK (0xffffffff)

// args: data (32-bit)
static inline void gdc_data1out_addr_write(u32 msb, u32 data, u32 dev_type,
					   u32 core_id)
{
	if (dev_type == ARM_GDC)
		system_gdc_write_32(0x48L, data, core_id);
	else
		system_gdc_write_32(ISP_DWAP_TOP_DST_Y_CTRL0,
				    (((u64)msb << 32) + data) >> 4, core_id);
}

static inline u32 gdc_data1out_addr_read(void)
{
	return system_gdc_read_32(0x48L, 0);
}

// ----------------------------------- //
// Register: data1out line offset
// ----------------------------------- //

// ----------------------------------- //
// Address difference between adjacent
// lines for the 1st plane in the
// output frame buffer (in bytes, AXI word aligned)
// ----------------------------------- //

#define GDC_DATA1OUT_LINE_OFFSET_DEFAULT (0x0)
#define GDC_DATA1OUT_LINE_OFFSET_DATASIZE (32)
#define GDC_DATA1OUT_LINE_OFFSET_OFFSET (0x4c)
#define GDC_DATA1OUT_LINE_OFFSET_MASK (0xffffffff)

// args: data (32-bit)
static inline void gdc_data1out_line_offset_write(u32 data, u32 dev_type,
						  u32 core_id)
{
	if (dev_type == ARM_GDC) {
		system_gdc_write_32(0x4cL, data, core_id);
	} else {
		data = DEWARP_STRIDE_ALIGN(data);
		system_gdc_write_32(ISP_DWAP_TOP_DST_Y_CTRL1, data, core_id);
	}
}

static inline u32 gdc_data1out_line_offset_read(void)
{
	return system_gdc_read_32(0x4cL, 0);
}

// ----------------------------------- //
// Register: data2out addr
// ----------------------------------- //

// ----------------------------------- //
// Base address of the 2nd plane in the
// output frame buffer  (in bytes, AXI word aligned)
// ----------------------------------- //

#define GDC_DATA2OUT_ADDR_DEFAULT (0x0)
#define GDC_DATA2OUT_ADDR_DATASIZE (32)
#define GDC_DATA2OUT_ADDR_OFFSET (0x50)
#define GDC_DATA2OUT_ADDR_MASK (0xffffffff)

// args: data (32-bit)
static inline void gdc_data2out_addr_write(u32 msb, u32 data, u32 dev_type,
					   u32 core_id)
{
	if (dev_type == ARM_GDC)
		system_gdc_write_32(0x50L, data, core_id);
	else
		system_gdc_write_32(ISP_DWAP_TOP_DST_U_CTRL0,
				    (((u64)msb << 32) + data) >> 4, core_id);
}

static inline u32 gdc_data2out_addr_read(void)
{
	return system_gdc_read_32(0x50L, 0);
}

// ----------------------------------- //
// Register: data2out line offset
// ----------------------------------- //

// ----------------------------------- //
// Address difference between adjacent lines
// for the 2ndt plane in the
// output frame buffer (in bytes, AXI word aligned)
// ----------------------------------- //

#define GDC_DATA2OUT_LINE_OFFSET_DEFAULT (0x0)
#define GDC_DATA2OUT_LINE_OFFSET_DATASIZE (32)
#define GDC_DATA2OUT_LINE_OFFSET_OFFSET (0x54)
#define GDC_DATA2OUT_LINE_OFFSET_MASK (0xffffffff)

// args: data (32-bit)
static inline void gdc_data2out_line_offset_write(u32 data, u32 dev_type,
						  u32 core_id)
{
	if (dev_type == ARM_GDC) {
		system_gdc_write_32(0x54L, data, core_id);
	} else {
		data = DEWARP_STRIDE_ALIGN(data);
		system_gdc_write_32(ISP_DWAP_TOP_DST_U_CTRL1, data, core_id);
	}
}

static inline u32 gdc_data2out_line_offset_read(void)
{
	return system_gdc_read_32(0x54L, 0);
}

// ----------------------------------- //
// Register: data3out addr
// ----------------------------------- //

// ----------------------------------- //
// Base address of the 3rd plane in the
// output frame buffer  (in bytes, AXI word aligned)
// ----------------------------------- //

#define GDC_DATA3OUT_ADDR_DEFAULT (0x0)
#define GDC_DATA3OUT_ADDR_DATASIZE (32)
#define GDC_DATA3OUT_ADDR_OFFSET (0x58)
#define GDC_DATA3OUT_ADDR_MASK (0xffffffff)

// args: data (32-bit)
static inline void gdc_data3out_addr_write(u32 msb, u32 data, u32 dev_type,
					   u32 core_id)
{
	if (dev_type == ARM_GDC)
		system_gdc_write_32(0x58L, data, core_id);
	else
		system_gdc_write_32(ISP_DWAP_TOP_DST_V_CTRL0,
				    (((u64)msb << 32) + data) >> 4, core_id);
}

static inline u32 gdc_data3out_addr_read(void)
{
	return system_gdc_read_32(0x58L, 0);
}

// ----------------------------------- //
// Register: data3out line offset
// ----------------------------------- //

// ----------------------------------- //
// Address difference between adjacent
// lines for the 3rd plane in the
// output frame buffer (in bytes, AXI word aligned)
// ----------------------------------- //

#define GDC_DATA3OUT_LINE_OFFSET_DEFAULT (0x0)
#define GDC_DATA3OUT_LINE_OFFSET_DATASIZE (32)
#define GDC_DATA3OUT_LINE_OFFSET_OFFSET (0x5c)
#define GDC_DATA3OUT_LINE_OFFSET_MASK (0xffffffff)

// args: data (32-bit)
static inline void gdc_data3out_line_offset_write(u32 data, u32 dev_type,
						  u32 core_id)
{
	if (dev_type == ARM_GDC) {
		system_gdc_write_32(0x5cL, data, core_id);
	} else {
		data = DEWARP_STRIDE_ALIGN(data);
		system_gdc_write_32(ISP_DWAP_TOP_DST_V_CTRL1, data, core_id);
	}
}

static inline u32 gdc_data3out_line_offset_read(void)
{
	return system_gdc_read_32(0x5cL, 0);
}

// ----------------------------------- //
// Register: status
// ----------------------------------- //

// ----------------------------------- //
// word with status fields:
// ----------------------------------- //

#define GDC_STATUS_DEFAULT (0x0)
#define GDC_STATUS_DATASIZE (32)
#define GDC_STATUS_OFFSET (0x60)
#define GDC_STATUS_MASK (0xffffffff)

// args: data (32-bit)
static inline u32 gdc_status_read(void)
{
	return system_gdc_read_32(0x60L, 0);
}

// ----------------------------------- //
// Register: busy
// ----------------------------------- //

// ----------------------------------- //
// Busy 1 = processing in progress,
//	  0 = ready for next image
// ----------------------------------- //

#define GDC_BUSY_DEFAULT (0x0)
#define GDC_BUSY_DATASIZE (1)
#define GDC_BUSY_OFFSET (0x60)
#define GDC_BUSY_MASK (0x1)

// args: data (1-bit)
static inline void gdc_busy_write(uint8_t data)
{
	u32 curr = system_gdc_read_32(0x60L, 0);

	system_gdc_write_32(0x60L, ((data & 0x1) << 0) | (curr & 0xfffffffe),
			    0);
}

static inline uint8_t gdc_busy_read(void)
{
	return (uint8_t)((system_gdc_read_32(0x60L, 0) & 0x1) >> 0);
}

// ----------------------------------- //
// Register: error
// ----------------------------------- //

// ----------------------------------- //
// Error flag: last operation was finished with error (see bits 15:8)
// ----------------------------------- //

#define GDC_ERROR_DEFAULT (0x0)
#define GDC_ERROR_DATASIZE (1)
#define GDC_ERROR_OFFSET (0x60)
#define GDC_ERROR_MASK (0x2)

// args: data (1-bit)
static inline void gdc_error_write(uint8_t data)
{
	u32 curr = system_gdc_read_32(0x60L, 0);
	u32 val = ((data & 0x1) << 1) | (curr & 0xfffffffd);

	system_gdc_write_32(0x60L, val, 0);
}

static inline uint8_t gdc_error_read(void)
{
	return (uint8_t)((system_gdc_read_32(0x60L, 0) & 0x2) >> 1);
}

// ----------------------------------- //
// Register: Reserved for future use 1
// ----------------------------------- //

#define GDC_RESERVED_FOR_FUTURE_USE_1_DEFAULT (0x0)
#define GDC_RESERVED_FOR_FUTURE_USE_1_DATASIZE (6)
#define GDC_RESERVED_FOR_FUTURE_USE_1_OFFSET (0x60)
#define GDC_RESERVED_FOR_FUTURE_USE_1_MASK (0xfc)

// args: data (6-bit)
static inline void gdc_reserved_for_future_use_1_write(uint8_t data)
{
	u32 curr = system_gdc_read_32(0x60L, 0);
	u32 val = ((data & 0x3f) << 2) | (curr & 0xffffff03);

	system_gdc_write_32(0x60L, val, 0);
}

static inline uint8_t gdc_reserved_for_future_use_1_read(void)
{
	return (uint8_t)((system_gdc_read_32(0x60L, 0) & 0xfc) >> 2);
}

// ----------------------------------- //
// Register: configuration error
// ----------------------------------- //

// ----------------------------------- //
// Configuration error (wrong configuration stream)
// ----------------------------------- //

#define GDC_CONFIGURATION_ERROR_DEFAULT (0x0)
#define GDC_CONFIGURATION_ERROR_DATASIZE (1)
#define GDC_CONFIGURATION_ERROR_OFFSET (0x60)
#define GDC_CONFIGURATION_ERROR_MASK (0x100)

// args: data (1-bit)
static inline void gdc_configuration_error_write(uint8_t data)
{
	u32 curr = system_gdc_read_32(0x60L, 0);
	u32 val =  ((data & 0x1) << 8) | (curr & 0xfffffeff);

	system_gdc_write_32(0x60L, val, 0);
}

static inline uint8_t gdc_configuration_error_read(void)
{
	return (uint8_t)((system_gdc_read_32(0x60L, 0) & 0x100) >> 8);
}

// ----------------------------------- //
// Register: user abort
// ----------------------------------- //

// ----------------------------------- //
// User abort (stop/reset command)
// ----------------------------------- //

#define GDC_USER_ABORT_DEFAULT (0x0)
#define GDC_USER_ABORT_DATASIZE (1)
#define GDC_USER_ABORT_OFFSET (0x60)
#define GDC_USER_ABORT_MASK (0x200)

// args: data (1-bit)
static inline void gdc_user_abort_write(uint8_t data)
{
	u32 curr = system_gdc_read_32(0x60L, 0);
	u32 val = ((data & 0x1) << 9) | (curr & 0xfffffdff);

	system_gdc_write_32(0x60L, val, 0);
}

static inline uint8_t gdc_user_abort_read(void)
{
	return (uint8_t)((system_gdc_read_32(0x60L, 0) & 0x200) >> 9);
}

// ----------------------------------- //
// Register: AXI reader error
// ----------------------------------- //

// ----------------------------------- //
// AXI reader error (e.g. error code returned by fabric)
// ----------------------------------- //

#define GDC_AXI_READER_ERROR_DEFAULT (0x0)
#define GDC_AXI_READER_ERROR_DATASIZE (1)
#define GDC_AXI_READER_ERROR_OFFSET (0x60)
#define GDC_AXI_READER_ERROR_MASK (0x400)

// args: data (1-bit)
static inline void gdc_axi_reader_error_write(uint8_t data)
{
	u32 curr = system_gdc_read_32(0x60L, 0);
	u32 val = ((data & 0x1) << 10) | (curr & 0xfffffbff);

	system_gdc_write_32(0x60L, val, 0);
}

static inline uint8_t gdc_axi_reader_error_read(void)
{
	return (uint8_t)((system_gdc_read_32(0x60L, 0) & 0x400) >> 10);
}

// ----------------------------------- //
// Register: AXI writer error
// ----------------------------------- //

// ----------------------------------- //
// AXI writer error
// ----------------------------------- //

#define GDC_AXI_WRITER_ERROR_DEFAULT (0x0)
#define GDC_AXI_WRITER_ERROR_DATASIZE (1)
#define GDC_AXI_WRITER_ERROR_OFFSET (0x60)
#define GDC_AXI_WRITER_ERROR_MASK (0x800)

// args: data (1-bit)
static inline void gdc_axi_writer_error_write(uint8_t data)
{
	u32 curr = system_gdc_read_32(0x60L, 0);
	u32 val = ((data & 0x1) << 11) | (curr & 0xfffff7ff);

	system_gdc_write_32(0x60L, val, 0);
}

static inline uint8_t gdc_axi_writer_error_read(void)
{
	return (uint8_t)((system_gdc_read_32(0x60L, 0) & 0x800) >> 11);
}

// ----------------------------------- //
// Register: Unaligned access
// ----------------------------------- //

// ----------------------------------- //
// Unaligned access (address pointer is not aligned)
// ----------------------------------- //

#define GDC_UNALIGNED_ACCESS_DEFAULT (0x0)
#define GDC_UNALIGNED_ACCESS_DATASIZE (1)
#define GDC_UNALIGNED_ACCESS_OFFSET (0x60)
#define GDC_UNALIGNED_ACCESS_MASK (0x1000)

// args: data (1-bit)
static inline void gdc_unaligned_access_write(uint8_t data)
{
	u32 curr = system_gdc_read_32(0x60L, 0);
	u32 val = ((data & 0x1) << 12) | (curr & 0xffffefff);

	system_gdc_write_32(0x60L, val, 0);
}

static inline uint8_t gdc_unaligned_access_read(void)
{
	return (uint8_t)((system_gdc_read_32(0x60L, 0) & 0x1000) >> 12);
}

// ----------------------------------- //
// Register: Incompatible configuration
// ----------------------------------- //

// ----------------------------------- //
// Incompatible configuration (request
// of unimplemented mode of operation,
// e.g. unsupported image format,
// unsupported module mode in the configuration stream)
// ----------------------------------- //

#define GDC_INCOMPATIBLE_CONFIGURATION_DEFAULT (0x0)
#define GDC_INCOMPATIBLE_CONFIGURATION_DATASIZE (1)
#define GDC_INCOMPATIBLE_CONFIGURATION_OFFSET (0x60)
#define GDC_INCOMPATIBLE_CONFIGURATION_MASK (0x2000)

// args: data (1-bit)
static inline void gdc_incompatible_configuration_write(uint8_t data)
{
	u32 curr = system_gdc_read_32(0x60L, 0);
	u32 val = ((data & 0x1) << 13) | (curr & 0xffffdfff);

	system_gdc_write_32(0x60L, val, 0);
}

static inline uint8_t gdc_incompatible_configuration_read(void)
{
	return (uint8_t)((system_gdc_read_32(0x60L, 0) & 0x2000) >> 13);
}

// ----------------------------------- //
// Register: Reserved for future use 2
// ----------------------------------- //

#define GDC_RESERVED_FOR_FUTURE_USE_2_DEFAULT (0x0)
#define GDC_RESERVED_FOR_FUTURE_USE_2_DATASIZE (18)
#define GDC_RESERVED_FOR_FUTURE_USE_2_OFFSET (0x60)
#define GDC_RESERVED_FOR_FUTURE_USE_2_MASK (0xffffc000)

// args: data (18-bit)
static inline void gdc_reserved_for_future_use_2_write(u32 data)
{
	u32 curr = system_gdc_read_32(0x60L, 0);
	u32 val =  ((data & 0x3ffff) << 14) | (curr & 0x3fff);

	system_gdc_write_32(0x60L, val, 0);
}

static inline u32 gdc_reserved_for_future_use_2_read(void)
{
	return (u32)((system_gdc_read_32(0x60L, 0) & 0xffffc000) >> 14);
}

// ----------------------------------- //
// Register: config
// ----------------------------------- //

#define GDC_CONFIG_DEFAULT (0x0)
#define GDC_CONFIG_DATASIZE (32)
#define GDC_CONFIG_OFFSET (0x64)
#define GDC_CONFIG_MASK (0xffffffff)

// args: data (32-bit)
static inline void gdc_config_write(u32 data)
{
	system_gdc_write_32(0x64L, data, 0);
}

static inline u32 gdc_config_read(void)
{
	return system_gdc_read_32(0x64L, 0);
}

// ----------------------------------- //
// Register: start flag
// ----------------------------------- //

// ----------------------------------- //
// Start flag: transition from 0 to 1
// latches the data on the configuration ports
// and starts the processing
// ----------------------------------- //

#define GDC_START_FLAG_DEFAULT (0x0)
#define GDC_START_FLAG_DATASIZE (1)
#define GDC_START_FLAG_OFFSET (0x64)
#define GDC_START_FLAG_MASK (0x1)

// args: data (1-bit)
static inline void gdc_start_flag_write(u8 data, u32 dev_type, u32 core_id)
{
	if (dev_type == ARM_GDC) {
		u32 curr = system_gdc_read_32(0x64L, core_id);
		u32 val = ((data & 0x1) << 0) | (curr & 0xfffffffe);

		system_gdc_write_32(0x64L, val, core_id);
	} else {
		if (data) {
			/* secure mem access */
			u32 sec_bit = system_gdc_read_32(ISP_DWAP_TOP_CTRL0,
							 core_id) & (1 << 5);
			u32 val = 0;

			val = sec_bit |
			      1 << 30 | /* reg_sw_rst */
			      0 << 16 | /* reg_stdly_num */
			      1 << 2  ; /* reg_hs_sel */
			system_gdc_write_32(ISP_DWAP_TOP_CTRL0, val, core_id);

			val = sec_bit | 1 << 31 | 1 << 2;  /* reg_frm_rst */
			system_gdc_write_32(ISP_DWAP_TOP_CTRL0, val, core_id);
		}
	}
}

static inline void gdc_secure_set(u8 data, u32 dev_type, u32 core_id)
{
	if (dev_type == ARM_GDC) {
		#ifdef CONFIG_AMLOGIC_TEE
		tee_config_device_state(DMC_DEV_ID_GDC, data);
		#endif
	} else {
		u32 val = system_gdc_read_32(ISP_DWAP_TOP_CTRL0, core_id);

		if (data)
			val |= 1 << 5;
		else
			val &= ~(1 << 5);
		system_gdc_write_32(ISP_DWAP_TOP_CTRL0, val, core_id);
	}
	gdc_log(LOG_DEBUG, "secure mode:%d, dev_type:%d\n", data, dev_type);
}

static inline uint8_t gdc_start_flag_read(void)
{
	return (uint8_t)((system_gdc_read_32(0x64L, 0) & 0x1) >> 0);
}

// ----------------------------------- //
// Register: stop flag
// ----------------------------------- //

// ----------------------------------- //
// Stop/reset flag: 0 - normal operation,
// 1 means to initiate internal cleanup procedure
// to abandon the current frame
// and prepare for processing of the next frame.
// The busy flag in status word should be
// cleared at the end of this process
// ----------------------------------- //

#define GDC_STOP_FLAG_DEFAULT (0x0)
#define GDC_STOP_FLAG_DATASIZE (1)
#define GDC_STOP_FLAG_OFFSET (0x64)
#define GDC_STOP_FLAG_MASK (0x2)

// args: data (1-bit)
static inline void gdc_stop_flag_write(u8 data, u32 dev_type, u32 core_id)
{
	if (dev_type == ARM_GDC) {
		u32 curr = system_gdc_read_32(0x64L, core_id);
		u32 val = ((data & 0x1) << 1) | (curr & 0xfffffffd);

		system_gdc_write_32(0x64L, val, core_id);
	}
}

static inline uint8_t gdc_stop_flag_read(void)
{
	return (uint8_t)((system_gdc_read_32(0x64L, 0) & 0x2) >> 1);
}

// ----------------------------------- //
// Register: Reserved for future use 3
// ----------------------------------- //

#define GDC_RESERVED_FOR_FUTURE_USE_3_DEFAULT (0x0)
#define GDC_RESERVED_FOR_FUTURE_USE_3_DATASIZE (30)
#define GDC_RESERVED_FOR_FUTURE_USE_3_OFFSET (0x64)
#define GDC_RESERVED_FOR_FUTURE_USE_3_MASK (0xfffffffc)

// args: data (30-bit)
static inline void gdc_reserved_for_future_use_3_write(u32 data)
{
	u32 curr = system_gdc_read_32(0x64L, 0);
	u32 val =  ((data & 0x3fffffff) << 2) | (curr & 0x3);

	system_gdc_write_32(0x64L, val, 0);
}

static inline u32 gdc_reserved_for_future_use_3_read(void)
{
	return (u32)((system_gdc_read_32(0x64L, 0) & 0xfffffffc) >> 2);
}

// ----------------------------------- //
// Register: Capability mask
// ----------------------------------- //

#define GDC_CAPABILITY_MASK_DEFAULT (0x0)
#define GDC_CAPABILITY_MASK_DATASIZE (32)
#define GDC_CAPABILITY_MASK_OFFSET (0x68)
#define GDC_CAPABILITY_MASK_MASK (0xffffffff)

// args: data (32-bit)
static inline u32 gdc_capability_mask_read(void)
{
	return system_gdc_read_32(0x68L, 0);
}

// ----------------------------------- //
// Register: Eight bit data suppoirted
// ----------------------------------- //

// ----------------------------------- //
// 8 bit data supported
// ----------------------------------- //

#define GDC_EIGHT_BIT_DATA_SUPPOIRTED_DEFAULT (0x0)
#define GDC_EIGHT_BIT_DATA_SUPPOIRTED_DATASIZE (1)
#define GDC_EIGHT_BIT_DATA_SUPPOIRTED_OFFSET (0x68)
#define GDC_EIGHT_BIT_DATA_SUPPOIRTED_MASK (0x1)

// args: data (1-bit)
static inline void gdc_eight_bit_data_suppoirted_write(uint8_t data)
{
	u32 curr = system_gdc_read_32(0x68L, 0);
	u32 val = ((data & 0x1) << 0) | (curr & 0xfffffffe);

	system_gdc_write_32(0x68L, val, 0);
}

static inline uint8_t gdc_eight_bit_data_suppoirted_read(void)
{
	return (uint8_t)((system_gdc_read_32(0x68L, 0) & 0x1) >> 0);
}

// ----------------------------------- //
// Register: Ten bit data supported
// ----------------------------------- //

// ----------------------------------- //
// 10 bit data supported
// ----------------------------------- //

#define GDC_TEN_BIT_DATA_SUPPORTED_DEFAULT (0x0)
#define GDC_TEN_BIT_DATA_SUPPORTED_DATASIZE (1)
#define GDC_TEN_BIT_DATA_SUPPORTED_OFFSET (0x68)
#define GDC_TEN_BIT_DATA_SUPPORTED_MASK (0x2)

// args: data (1-bit)
static inline void gdc_ten_bit_data_supported_write(uint8_t data)
{
	u32 curr = system_gdc_read_32(0x68L, 0);
	u32 val = ((data & 0x1) << 1) | (curr & 0xfffffffd);

	system_gdc_write_32(0x68L, val, 0);
}

static inline uint8_t gdc_ten_bit_data_supported_read(void)
{
	return (uint8_t)((system_gdc_read_32(0x68L, 0) & 0x2) >> 1);
}

// ----------------------------------- //
// Register: Grayscale supported
// ----------------------------------- //

// ----------------------------------- //
// grayscale supported
// ----------------------------------- //

#define GDC_GRAYSCALE_SUPPORTED_DEFAULT (0x0)
#define GDC_GRAYSCALE_SUPPORTED_DATASIZE (1)
#define GDC_GRAYSCALE_SUPPORTED_OFFSET (0x68)
#define GDC_GRAYSCALE_SUPPORTED_MASK (0x4)

// args: data (1-bit)
static inline void gdc_grayscale_supported_write(uint8_t data)
{
	u32 curr = system_gdc_read_32(0x68L, 0);
	u32 val = ((data & 0x1) << 2) | (curr & 0xfffffffb);

	system_gdc_write_32(0x68L, val, 0);
}

static inline uint8_t gdc_grayscale_supported_read(void)
{
	return (uint8_t)((system_gdc_read_32(0x68L, 0) & 0x4) >> 2);
}

// ----------------------------------- //
// Register: RGBA888 supported
// ----------------------------------- //

// ----------------------------------- //
// RGBA8:8:8/YUV4:4:4 mode supported
// ----------------------------------- //

#define GDC_RGBA888_SUPPORTED_DEFAULT (0x0)
#define GDC_RGBA888_SUPPORTED_DATASIZE (1)
#define GDC_RGBA888_SUPPORTED_OFFSET (0x68)
#define GDC_RGBA888_SUPPORTED_MASK (0x8)

// args: data (1-bit)
static inline void gdc_rgba888_supported_write(uint8_t data)
{
	u32 curr = system_gdc_read_32(0x68L, 0);
	u32 val =  ((data & 0x1) << 3) | (curr & 0xfffffff7);

	system_gdc_write_32(0x68L, val, 0);
}

static inline uint8_t gdc_rgba888_supported_read(void)
{
	return (uint8_t)((system_gdc_read_32(0x68L, 0) & 0x8) >> 3);
}

// ----------------------------------- //
// Register: RGB YUV444 planar supported
// ----------------------------------- //

// ----------------------------------- //
// RGB/YUV444 planar modes supported
// ----------------------------------- //

#define GDC_RGB_YUV444_PLANAR_SUPPORTED_DEFAULT (0x0)
#define GDC_RGB_YUV444_PLANAR_SUPPORTED_DATASIZE (1)
#define GDC_RGB_YUV444_PLANAR_SUPPORTED_OFFSET (0x68)
#define GDC_RGB_YUV444_PLANAR_SUPPORTED_MASK (0x10)

// args: data (1-bit)
static inline void gdc_rgb_yuv444_planar_supported_write(uint8_t data)
{
	u32 curr = system_gdc_read_32(0x68L, 0);
	u32 val = ((data & 0x1) << 4) | (curr & 0xffffffef);

	system_gdc_write_32(0x68L, val, 0);
}

static inline uint8_t gdc_rgb_yuv444_planar_supported_read(void)
{
	return (uint8_t)((system_gdc_read_32(0x68L, 0) & 0x10) >> 4);
}

// ----------------------------------- //
// Register: YUV semiplanar supported
// ----------------------------------- //

// ----------------------------------- //
// YUV semiplanar modes supported
// ----------------------------------- //

#define GDC_YUV_SEMIPLANAR_SUPPORTED_DEFAULT (0x0)
#define GDC_YUV_SEMIPLANAR_SUPPORTED_DATASIZE (1)
#define GDC_YUV_SEMIPLANAR_SUPPORTED_OFFSET (0x68)
#define GDC_YUV_SEMIPLANAR_SUPPORTED_MASK (0x20)

// args: data (1-bit)
static inline void gdc_yuv_semiplanar_supported_write(uint8_t data)
{
	u32 curr = system_gdc_read_32(0x68L, 0);
	u32 val = ((data & 0x1) << 5) | (curr & 0xffffffdf);

	system_gdc_write_32(0x68L, val, 0);
}

static inline uint8_t gdc_yuv_semiplanar_supported_read(void)
{
	return (uint8_t)((system_gdc_read_32(0x68L, 0) & 0x20) >> 5);
}

// ----------------------------------- //
// Register: YUV422 linear mode supported
// ----------------------------------- //

// ----------------------------------- //
// YUV4:2:2 linear mode supported (16 bit/pixel)
// ----------------------------------- //

#define GDC_YUV422_LINEAR_MODE_SUPPORTED_DEFAULT (0x0)
#define GDC_YUV422_LINEAR_MODE_SUPPORTED_DATASIZE (1)
#define GDC_YUV422_LINEAR_MODE_SUPPORTED_OFFSET (0x68)
#define GDC_YUV422_LINEAR_MODE_SUPPORTED_MASK (0x40)

// args: data (1-bit)
static inline void gdc_yuv422_linear_mode_supported_write(uint8_t data)
{
	u32 curr = system_gdc_read_32(0x68L, 0);
	u32 val =  ((data & 0x1) << 6) | (curr & 0xffffffbf);

	system_gdc_write_32(0x68L, val, 0);
}

static inline uint8_t gdc_yuv422_linear_mode_supported_read(void)
{
	return (uint8_t)((system_gdc_read_32(0x68L, 0) & 0x40) >> 6);
}

// ----------------------------------- //
// Register: RGB10_10_10 supported
// ----------------------------------- //

// ----------------------------------- //
// RGB10:10:10 mode supported
// ----------------------------------- //

#define GDC_RGB10_10_10_SUPPORTED_DEFAULT (0x0)
#define GDC_RGB10_10_10_SUPPORTED_DATASIZE (1)
#define GDC_RGB10_10_10_SUPPORTED_OFFSET (0x68)
#define GDC_RGB10_10_10_SUPPORTED_MASK (0x80)

// args: data (1-bit)
static inline void gdc_rgb10_10_10_supported_write(uint8_t data)
{
	u32 curr = system_gdc_read_32(0x68L, 0);
	u32 val =  ((data & 0x1) << 7) | (curr & 0xffffff7f);

	system_gdc_write_32(0x68L, val, 0);
}

static inline uint8_t gdc_rgb10_10_10_supported_read(void)
{
	return (uint8_t)((system_gdc_read_32(0x68L, 0) & 0x80) >> 7);
}

// ----------------------------------- //
// Register: Bicubic interpolation supported
// ----------------------------------- //

// ----------------------------------- //
// 4 tap bicubic interpolation supported
// ----------------------------------- //

#define GDC_BICUBIC_INTERPOLATION_SUPPORTED_DEFAULT (0x0)
#define GDC_BICUBIC_INTERPOLATION_SUPPORTED_DATASIZE (1)
#define GDC_BICUBIC_INTERPOLATION_SUPPORTED_OFFSET (0x68)
#define GDC_BICUBIC_INTERPOLATION_SUPPORTED_MASK (0x100)

// args: data (1-bit)
static inline void gdc_bicubic_interpolation_supported_write(uint8_t data)
{
	u32 curr = system_gdc_read_32(0x68L, 0);
	u32 val =  ((data & 0x1) << 8) | (curr & 0xfffffeff);

	system_gdc_write_32(0x68L, val, 0);
}

static inline uint8_t gdc_bicubic_interpolation_supported_read(void)
{
	return (uint8_t)((system_gdc_read_32(0x68L, 0) & 0x100) >> 8);
}

// ----------------------------------- //
// Register: Bilinear interpolation mode 1 supported
// ----------------------------------- //

// ----------------------------------- //
// bilinear interpolation mode 1 supported {for U,V components}
// ----------------------------------- //

#define GDC_BILINEAR_INTERPOLATION_MODE_1_SUPPORTED_DEFAULT (0x0)
#define GDC_BILINEAR_INTERPOLATION_MODE_1_SUPPORTED_DATASIZE (1)
#define GDC_BILINEAR_INTERPOLATION_MODE_1_SUPPORTED_OFFSET (0x68)
#define GDC_BILINEAR_INTERPOLATION_MODE_1_SUPPORTED_MASK (0x200)

// args: data (1-bit)
static inline void
gdc_bilinear_interpolation_mode_1_supported_write(uint8_t data)
{
	u32 curr = system_gdc_read_32(0x68L, 0);
	u32 val =  ((data & 0x1) << 9) | (curr & 0xfffffdff);

	system_gdc_write_32(0x68L, val, 0);
}

static inline uint8_t gdc_bilinear_interpolation_mode_1_supported_read(void)
{
	return (uint8_t)((system_gdc_read_32(0x68L, 0) & 0x200) >> 9);
}

// ----------------------------------- //
// Register: Bilinear interpolation mode 2 supported
// ----------------------------------- //

// ----------------------------------- //
// bilinear interpolation mode 2 supported {for U,V components}
// ----------------------------------- //

#define GDC_BILINEAR_INTERPOLATION_MODE_2_SUPPORTED_DEFAULT (0x0)
#define GDC_BILINEAR_INTERPOLATION_MODE_2_SUPPORTED_DATASIZE (1)
#define GDC_BILINEAR_INTERPOLATION_MODE_2_SUPPORTED_OFFSET (0x68)
#define GDC_BILINEAR_INTERPOLATION_MODE_2_SUPPORTED_MASK (0x400)

// args: data (1-bit)
static inline void
gdc_bilinear_interpolation_mode_2_supported_write(uint8_t data)
{
	u32 curr = system_gdc_read_32(0x68L, 0);
	u32 val =  ((data & 0x1) << 10) | (curr & 0xfffffbff);

	system_gdc_write_32(0x68L, val, 0);
}

static inline uint8_t gdc_bilinear_interpolation_mode_2_supported_read(void)
{
	return (uint8_t)((system_gdc_read_32(0x68L, 0) & 0x400) >> 10);
}

// ----------------------------------- //
// Register: Output of interpolation coordinates supported
// ----------------------------------- //

// ----------------------------------- //
// output of interpolation coordinates is supported
// ----------------------------------- //

#define GDC_OUTPUT_OF_INTERPOLATION_COORDINATES_SUPPORTED_DEFAULT (0x0)
#define GDC_OUTPUT_OF_INTERPOLATION_COORDINATES_SUPPORTED_DATASIZE (1)
#define GDC_OUTPUT_OF_INTERPOLATION_COORDINATES_SUPPORTED_OFFSET (0x68)
#define GDC_OUTPUT_OF_INTERPOLATION_COORDINATES_SUPPORTED_MASK (0x800)

// args: data (1-bit)
static inline void
gdc_output_of_interpolation_coordinates_supported_write(uint8_t data)
{
	u32 curr = system_gdc_read_32(0x68L, 0);
	u32 val = ((data & 0x1) << 11) | (curr & 0xfffff7ff);

	system_gdc_write_32(0x68L, val, 0);
}

static inline uint8_t
gdc_output_of_interpolation_coordinates_supported_read(void)
{
	return (uint8_t)((system_gdc_read_32(0x68L, 0) & 0x800) >> 11);
}

// ----------------------------------- //
// Register: Reserved for future use 4
// ----------------------------------- //

#define GDC_RESERVED_FOR_FUTURE_USE_4_DEFAULT (0x0)
#define GDC_RESERVED_FOR_FUTURE_USE_4_DATASIZE (4)
#define GDC_RESERVED_FOR_FUTURE_USE_4_OFFSET (0x68)
#define GDC_RESERVED_FOR_FUTURE_USE_4_MASK (0xf000)

// args: data (4-bit)
static inline void gdc_reserved_for_future_use_4_write(uint8_t data)
{
	u32 curr = system_gdc_read_32(0x68L, 0);
	u32 val = ((data & 0xf) << 12) | (curr & 0xffff0fff);

	system_gdc_write_32(0x68L, val, 0);
}

static inline uint8_t gdc_reserved_for_future_use_4_read(void)
{
	return (uint8_t)((system_gdc_read_32(0x68L, 0) & 0xf000) >> 12);
}

// ----------------------------------- //
// Register: Size of output cache
// ----------------------------------- //

// ----------------------------------- //
// log2(size of output cache in lines)-5 (0 - 32lines, 1 - 64 lines etc)
// ----------------------------------- //

#define GDC_SIZE_OF_OUTPUT_CACHE_DEFAULT (0x0)
#define GDC_SIZE_OF_OUTPUT_CACHE_DATASIZE (3)
#define GDC_SIZE_OF_OUTPUT_CACHE_OFFSET (0x68)
#define GDC_SIZE_OF_OUTPUT_CACHE_MASK (0x70000)

// args: data (3-bit)
static inline void gdc_size_of_output_cache_write(uint8_t data)
{
	u32 curr = system_gdc_read_32(0x68L, 0);
	u32 val =  ((data & 0x7) << 16) | (curr & 0xfff8ffff);

	system_gdc_write_32(0x68L, val, 0);
}

static inline uint8_t gdc_size_of_output_cache_read(void)
{
	return (uint8_t)((system_gdc_read_32(0x68L, 0) & 0x70000) >> 16);
}

// ----------------------------------- //
// Register: Size of tile cache
// ----------------------------------- //

// ----------------------------------- //
// log2(size of tile cache in 16x16 clusters)
// ----------------------------------- //

#define GDC_SIZE_OF_TILE_CACHE_DEFAULT (0x0)
#define GDC_SIZE_OF_TILE_CACHE_DATASIZE (5)
#define GDC_SIZE_OF_TILE_CACHE_OFFSET (0x68)
#define GDC_SIZE_OF_TILE_CACHE_MASK (0xf80000)

// args: data (5-bit)
static inline void gdc_size_of_tile_cache_write(uint8_t data)
{
	u32 curr = system_gdc_read_32(0x68L, 0);
	u32 val =  ((data & 0x1f) << 19) | (curr & 0xff07ffff);

	system_gdc_write_32(0x68L, val, 0);
}

static inline uint8_t gdc_size_of_tile_cache_read(void)
{
	return (uint8_t)((system_gdc_read_32(0x68L, 0) & 0xf80000) >> 19);
}

// ----------------------------------- //
// Register: Nuimber of polyphase filter banks
// ----------------------------------- //

// ----------------------------------- //
// log2(number of polyphase filter banks)
// ----------------------------------- //

#define GDC_NUIMBER_OF_POLYPHASE_FILTER_BANKS_DEFAULT (0x0)
#define GDC_NUIMBER_OF_POLYPHASE_FILTER_BANKS_DATASIZE (3)
#define GDC_NUIMBER_OF_POLYPHASE_FILTER_BANKS_OFFSET (0x68)
#define GDC_NUIMBER_OF_POLYPHASE_FILTER_BANKS_MASK (0x7000000)

// args: data (3-bit)
static inline void gdc_nuimber_of_polyphase_filter_banks_write(uint8_t data)
{
	u32 curr = system_gdc_read_32(0x68L, 0);
	u32 val = ((data & 0x7) << 24) | (curr & 0xf8ffffff);

	system_gdc_write_32(0x68L, val, 0);
}

static inline uint8_t gdc_nuimber_of_polyphase_filter_banks_read(void)
{
	return (uint8_t)((system_gdc_read_32(0x68L, 0) & 0x7000000) >> 24);
}

// ----------------------------------- //
// Register: AXI data width
// ----------------------------------- //

// ----------------------------------- //
// log2(AXI_DATA_WIDTH)-5
// ----------------------------------- //

#define GDC_AXI_DATA_WIDTH_DEFAULT (0x0)
#define GDC_AXI_DATA_WIDTH_DATASIZE (3)
#define GDC_AXI_DATA_WIDTH_OFFSET (0x68)
#define GDC_AXI_DATA_WIDTH_MASK (0x38000000)

// args: data (3-bit)
static inline void gdc_axi_data_width_write(uint8_t data)
{
	u32 curr = system_gdc_read_32(0x68L, 0);
	u32 val = ((data & 0x7) << 27) | (curr & 0xc7ffffff);

	system_gdc_write_32(0x68L, val, 0);
}

static inline uint8_t gdc_axi_data_width_read(void)
{
	return (uint8_t)((system_gdc_read_32(0x68L, 0) & 0x38000000) >> 27);
}

// ----------------------------------- //
// Register: Reserved for future use 5
// ----------------------------------- //

#define GDC_RESERVED_FOR_FUTURE_USE_5_DEFAULT (0x0)
#define GDC_RESERVED_FOR_FUTURE_USE_5_DATASIZE (2)
#define GDC_RESERVED_FOR_FUTURE_USE_5_OFFSET (0x68)
#define GDC_RESERVED_FOR_FUTURE_USE_5_MASK (0xc0000000)

// args: data (2-bit)
static inline void gdc_reserved_for_future_use_5_write(uint8_t data)
{
	u32 curr = system_gdc_read_32(0x68L, 0);
	u32 val = ((data & 0x3) << 30) | (curr & 0x3fffffff);

	system_gdc_write_32(0x68L, val, 0);
}

static inline uint8_t gdc_reserved_for_future_use_5_read(void)
{
	return (uint8_t)((system_gdc_read_32(0x68L, 0) & 0xc0000000) >> 30);
}

// ----------------------------------- //
// Register: default ch1
// ----------------------------------- //

// ----------------------------------- //
// Default value for 1st data channel (Y/R color)
// to fill missing pixels (when coordinated
// are out of bound). LSB aligned
// ----------------------------------- //

#define GDC_DEFAULT_CH1_DEFAULT (0x0)
#define GDC_DEFAULT_CH1_DATASIZE (12)
#define GDC_DEFAULT_CH1_OFFSET (0x70)
#define GDC_DEFAULT_CH1_MASK (0xfff)

// args: data (12-bit)
static inline void gdc_default_ch1_write(uint16_t data)
{
	u32 curr = system_gdc_read_32(0x70L, 0);
	u32  val = ((data & 0xfff) << 0) | (curr & 0xfffff000);

	system_gdc_write_32(0x70L, val, 0);
}

static inline uint16_t gdc_default_ch1_read(void)
{
	return (uint16_t)((system_gdc_read_32(0x70L, 0) & 0xfff) >> 0);
}

// ----------------------------------- //
// Register: default ch2
// ----------------------------------- //

// ----------------------------------- //
// Default value for 2nd data channel
// (U/G color) to fill missing pixels
// (when coordinated are out of bound) LSB aligned
// ----------------------------------- //

#define GDC_DEFAULT_CH2_DEFAULT (0x0)
#define GDC_DEFAULT_CH2_DATASIZE (12)
#define GDC_DEFAULT_CH2_OFFSET (0x74)
#define GDC_DEFAULT_CH2_MASK (0xfff)

// args: data (12-bit)
static inline void gdc_default_ch2_write(uint16_t data)
{
	u32 curr = system_gdc_read_32(0x74L, 0);
	u32 val = ((data & 0xfff) << 0) | (curr & 0xfffff000);

	system_gdc_write_32(0x74L, val, 0);
}

static inline uint16_t gdc_default_ch2_read(void)
{
	return (uint16_t)((system_gdc_read_32(0x74L, 0) & 0xfff) >> 0);
}

// ----------------------------------- //
// Register: default ch3
// ----------------------------------- //

// ----------------------------------- //
// Default value for 3rd data channel
// (V/B color) to fill missing pixels
// (when coordinated are out of bound) LSB aligned
// ----------------------------------- //

#define GDC_DEFAULT_CH3_DEFAULT (0x0)
#define GDC_DEFAULT_CH3_DATASIZE (12)
#define GDC_DEFAULT_CH3_OFFSET (0x78)
#define GDC_DEFAULT_CH3_MASK (0xfff)

// args: data (12-bit)
static inline void gdc_default_ch3_write(uint16_t data)
{
	u32 curr = system_gdc_read_32(0x78L, 0);
	u32 val = ((data & 0xfff) << 0) | (curr & 0xfffff000);

	system_gdc_write_32(0x78L, val, 0);
}

static inline uint16_t gdc_default_ch3_read(void)
{
	return (uint16_t)((system_gdc_read_32(0x78L, 0) & 0xfff) >> 0);
}

// ----------------------------------- //
// Group: GDC diagnostics
// ----------------------------------- //

// ----------------------------------- //
// Register: cfg_stall_count0
// ----------------------------------- //

// ----------------------------------- //
// Cycles spent on stalls on configuration FIFO to tile reader
// ----------------------------------- //

#define GDC_DIAGNOSTICS_CFG_STALL_COUNT0_DEFAULT (0x0)
#define GDC_DIAGNOSTICS_CFG_STALL_COUNT0_DATASIZE (32)
#define GDC_DIAGNOSTICS_CFG_STALL_COUNT0_OFFSET (0x80)
#define GDC_DIAGNOSTICS_CFG_STALL_COUNT0_MASK (0xffffffff)

// args: data (32-bit)
static inline u32 gdc_diagnostics_cfg_stall_count0_read(void)
{
	return system_gdc_read_32(0x80L, 0);
}

// ----------------------------------- //
// Register: cfg_stall_count1
// ----------------------------------- //

// ----------------------------------- //
// Cycles spent on stalls on configuration FIFO to CIM
// ----------------------------------- //

#define GDC_DIAGNOSTICS_CFG_STALL_COUNT1_DEFAULT (0x0)
#define GDC_DIAGNOSTICS_CFG_STALL_COUNT1_DATASIZE (32)
#define GDC_DIAGNOSTICS_CFG_STALL_COUNT1_OFFSET (0x84)
#define GDC_DIAGNOSTICS_CFG_STALL_COUNT1_MASK (0xffffffff)

// args: data (32-bit)
static inline u32 gdc_diagnostics_cfg_stall_count1_read(void)
{
	return system_gdc_read_32(0x84L, 0);
}

// ----------------------------------- //
// Register: cfg_stall_count2
// ----------------------------------- //

// ----------------------------------- //
// Cycles spent on stalls on configuration FIFO to PIM
// ----------------------------------- //

#define GDC_DIAGNOSTICS_CFG_STALL_COUNT2_DEFAULT (0x0)
#define GDC_DIAGNOSTICS_CFG_STALL_COUNT2_DATASIZE (32)
#define GDC_DIAGNOSTICS_CFG_STALL_COUNT2_OFFSET (0x88)
#define GDC_DIAGNOSTICS_CFG_STALL_COUNT2_MASK (0xffffffff)

// args: data (32-bit)
static inline u32 gdc_diagnostics_cfg_stall_count2_read(void)
{
	return system_gdc_read_32(0x88L, 0);
}

// ----------------------------------- //
// Register: cfg_stall_count3
// ----------------------------------- //

// ----------------------------------- //
// Cycles spent on stalls on configuration FIFO to write cache
// ----------------------------------- //

#define GDC_DIAGNOSTICS_CFG_STALL_COUNT3_DEFAULT (0x0)
#define GDC_DIAGNOSTICS_CFG_STALL_COUNT3_DATASIZE (32)
#define GDC_DIAGNOSTICS_CFG_STALL_COUNT3_OFFSET (0x8c)
#define GDC_DIAGNOSTICS_CFG_STALL_COUNT3_MASK (0xffffffff)

// args: data (32-bit)
static inline u32 gdc_diagnostics_cfg_stall_count3_read(void)
{
	return system_gdc_read_32(0x8cL, 0);
}

// ----------------------------------- //
// Register: cfg_stall_count4
// ----------------------------------- //

// ----------------------------------- //
// Cycles spent on stalls on configuration FIFO to tile writer
// ----------------------------------- //

#define GDC_DIAGNOSTICS_CFG_STALL_COUNT4_DEFAULT (0x0)
#define GDC_DIAGNOSTICS_CFG_STALL_COUNT4_DATASIZE (32)
#define GDC_DIAGNOSTICS_CFG_STALL_COUNT4_OFFSET (0x90)
#define GDC_DIAGNOSTICS_CFG_STALL_COUNT4_MASK (0xffffffff)

// args: data (32-bit)
static inline u32 gdc_diagnostics_cfg_stall_count4_read(void)
{
	return system_gdc_read_32(0x90L, 0);
}

// ----------------------------------- //
// Register: int_read_stall_count
// ----------------------------------- //

// ----------------------------------- //
// Cycles spent on waiting on pixel interpolator read pixel stream
// ----------------------------------- //

#define GDC_DIAGNOSTICS_INT_READ_STALL_COUNT_DEFAULT (0x0)
#define GDC_DIAGNOSTICS_INT_READ_STALL_COUNT_DATASIZE (32)
#define GDC_DIAGNOSTICS_INT_READ_STALL_COUNT_OFFSET (0x94)
#define GDC_DIAGNOSTICS_INT_READ_STALL_COUNT_MASK (0xffffffff)

// args: data (32-bit)
static inline u32 gdc_diagnostics_int_read_stall_count_read(void)
{
	return system_gdc_read_32(0x94L, 0);
}

// ----------------------------------- //
// Register: int_coord_stall_count
// ----------------------------------- //

// ----------------------------------- //
// Cycles spent on waiting on coordinate stream of pixel interpolator
// ----------------------------------- //

#define GDC_DIAGNOSTICS_INT_COORD_STALL_COUNT_DEFAULT (0x0)
#define GDC_DIAGNOSTICS_INT_COORD_STALL_COUNT_DATASIZE (32)
#define GDC_DIAGNOSTICS_INT_COORD_STALL_COUNT_OFFSET (0x98)
#define GDC_DIAGNOSTICS_INT_COORD_STALL_COUNT_MASK (0xffffffff)

// args: data (32-bit)
static inline u32 gdc_diagnostics_int_coord_stall_count_read(void)
{
	return system_gdc_read_32(0x98L, 0);
}

// ----------------------------------- //
// Register: int_write_wait_count
// ----------------------------------- //

// ----------------------------------- //
// Cycles spent on waiting on pixel interpolator output pixel stream
// ----------------------------------- //

#define GDC_DIAGNOSTICS_INT_WRITE_WAIT_COUNT_DEFAULT (0x0)
#define GDC_DIAGNOSTICS_INT_WRITE_WAIT_COUNT_DATASIZE (32)
#define GDC_DIAGNOSTICS_INT_WRITE_WAIT_COUNT_OFFSET (0x9c)
#define GDC_DIAGNOSTICS_INT_WRITE_WAIT_COUNT_MASK (0xffffffff)

// args: data (32-bit)
static inline u32 gdc_diagnostics_int_write_wait_count_read(void)
{
	return system_gdc_read_32(0x9cL, 0);
}

// ----------------------------------- //
// Register: wrt_write_wait_count
// ----------------------------------- //

// ----------------------------------- //
// Cycles spent on waiting on sending word from write cache to tile writer
// ----------------------------------- //

#define GDC_DIAGNOSTICS_WRT_WRITE_WAIT_COUNT_DEFAULT (0x0)
#define GDC_DIAGNOSTICS_WRT_WRITE_WAIT_COUNT_DATASIZE (32)
#define GDC_DIAGNOSTICS_WRT_WRITE_WAIT_COUNT_OFFSET (0xa0)
#define GDC_DIAGNOSTICS_WRT_WRITE_WAIT_COUNT_MASK (0xffffffff)

// args: data (32-bit)
static inline u32 gdc_diagnostics_wrt_write_wait_count_read(void)
{
	return system_gdc_read_32(0xa0L, 0);
}

// ----------------------------------- //
// Register: int_dual_count
// ----------------------------------- //

// ----------------------------------- //
// Number of beats on output of tile writer
// interface where 2 pixels were interpolated.
// ----------------------------------- //

#define GDC_DIAGNOSTICS_INT_DUAL_COUNT_DEFAULT (0x0)
#define GDC_DIAGNOSTICS_INT_DUAL_COUNT_DATASIZE (32)
#define GDC_DIAGNOSTICS_INT_DUAL_COUNT_OFFSET (0xa4)
#define GDC_DIAGNOSTICS_INT_DUAL_COUNT_MASK (0xffffffff)

// args: data (32-bit)
static inline u32 gdc_diagnostics_int_dual_count_read(void)
{
	return system_gdc_read_32(0xa4L, 0);
}

// ----------------------------------------------------//
// Group: AXI Settings
// ----------------------------------------------------//
// ----------------------------------------------------//
// Register: config reader max arlen
// ----------------------------------------------------//
// ----------------------------------------------------//
// Maximum value to use for arlen (axi burst length).
// "0000"= max 1 transfer/burst
// upto "1111"= max 16 transfers/burst
// ----------------------------------------------------//

#define GDC_AXI_SETTINGS_CONFIG_READER_MAX_ARLEN_DEFAULT (0xF)
#define GDC_AXI_SETTINGS_CONFIG_READER_MAX_ARLEN_DATASIZE (4)
#define GDC_AXI_SETTINGS_CONFIG_READER_MAX_ARLEN_OFFSET (0xa8)
#define GDC_AXI_SETTINGS_CONFIG_READER_MAX_ARLEN_MASK (0xf)

// args: data (4-bit)
static inline void gdc_axi_settings_config_reader_max_arlen_write(uint8_t data)
{
	u32 curr = system_gdc_read_32(0xa8L, 0);
	u32 val = ((data & 0xf) << 0) | (curr & 0xfffffff0);

	system_gdc_write_32(0xa8L, val, 0);
}

static inline uint8_t gdc_axi_settings_config_reader_max_arlen_read(void)
{
	return (uint8_t)((system_gdc_read_32(0xa8L, 0) & 0xf) >> 0);
}

// ----------------------------------- //
// Register: config reader fifo watermark
// ----------------------------------- //

// ----------------------------------- //
// Number of words space in fifo before AXI read burst(s) start
// (legal values = max_burst_length(max_arlen+1)
// to 2**fifo_aw, but workable value
// for your system are probably less!).
// Allowing n back to back bursts to generated
// if watermark is set to n*burst length.
// Burst(s) continue while fifo has enough space for next burst.
// ----------------------------------- //

#define GDC_AXI_SETTINGS_CONFIG_READER_FIFO_WATERMARK_DEFAULT (0x10)
#define GDC_AXI_SETTINGS_CONFIG_READER_FIFO_WATERMARK_DATASIZE (8)
#define GDC_AXI_SETTINGS_CONFIG_READER_FIFO_WATERMARK_OFFSET (0xa8)
#define GDC_AXI_SETTINGS_CONFIG_READER_FIFO_WATERMARK_MASK (0xff00)

// args: data (8-bit)
static inline void
gdc_axi_settings_config_reader_fifo_watermark_write(uint8_t data)
{
	u32 curr = system_gdc_read_32(0xa8L, 0);
	u32 val = ((data & 0xff) << 8) | (curr & 0xffff00ff);

	system_gdc_write_32(0xa8L, val, 0);
}

static inline uint8_t gdc_axi_settings_config_reader_fifo_watermark_read(void)
{
	return (uint8_t)((system_gdc_read_32(0xa8L, 0) & 0xff00) >> 8);
}

// ----------------------------------- //
// Register: config reader rxact maxostand
// ----------------------------------- //
// ----------------------------------- //
// Max outstanding read transactions (bursts) allowed.
// zero means no maximum(uses fifo size as max)
// ----------------------------------- //

#define GDC_AXI_SETTINGS_CONFIG_READER_RXACT_MAXOSTAND_DEFAULT (0x00)
#define GDC_AXI_SETTINGS_CONFIG_READER_RXACT_MAXOSTAND_DATASIZE (8)
#define GDC_AXI_SETTINGS_CONFIG_READER_RXACT_MAXOSTAND_OFFSET (0xa8)
#define GDC_AXI_SETTINGS_CONFIG_READER_RXACT_MAXOSTAND_MASK (0xff0000)

// args: data (8-bit)
static inline void
gdc_axi_settings_config_reader_rxact_maxostand_write(uint8_t data)
{
	u32 curr = system_gdc_read_32(0xa8L, 0);
	u32 val = ((data & 0xff) << 16) | (curr & 0xff00ffff);

	system_gdc_write_32(0xa8L, val, 0);
}

static inline uint8_t gdc_axi_settings_config_reader_rxact_maxostand_read(void)
{
	return (uint8_t)((system_gdc_read_32(0xa8L, 0) & 0xff0000) >> 16);
}

// ----------------------------------- //
// Register: tile reader max arlen
// ----------------------------------- //

// ----------------------------------- //
// Maximum value to use for arlen (axi burst length).
// "0000"= max 1 transfer/burst , upto "1111"= max 16 transfers/burst
// ----------------------------------- //

#define GDC_AXI_SETTINGS_TILE_READER_MAX_ARLEN_DEFAULT (0xF)
#define GDC_AXI_SETTINGS_TILE_READER_MAX_ARLEN_DATASIZE (4)
#define GDC_AXI_SETTINGS_TILE_READER_MAX_ARLEN_OFFSET (0xac)
#define GDC_AXI_SETTINGS_TILE_READER_MAX_ARLEN_MASK (0xf)

// args: data (4-bit)
static inline void gdc_axi_settings_tile_reader_max_arlen_write(uint8_t data)
{
	u32 curr = system_gdc_read_32(0xacL, 0);
	u32 val = ((data & 0xf) << 0) | (curr & 0xfffffff0);

	system_gdc_write_32(0xacL, val, 0);
}

static inline uint8_t gdc_axi_settings_tile_reader_max_arlen_read(void)
{
	return (uint8_t)((system_gdc_read_32(0xacL, 0) & 0xf) >> 0);
}

// ----------------------------------- //
// Register: tile reader fifo watermark
// ----------------------------------- //

// ----------------------------------- //
// Number of words space in fifo before AXI read burst(s) start
// (legal values = max_burst_length(max_arlen+1) to 2**fifo_aw,
// but workable value for your system are probably less!).
// Allowing n back to back bursts to generated
// if watermark is set to n*burst length.
// Burst(s) continue while fifo has enough space for next burst.
// ----------------------------------- //

#define GDC_AXI_SETTINGS_TILE_READER_FIFO_WATERMARK_DEFAULT (0x10)
#define GDC_AXI_SETTINGS_TILE_READER_FIFO_WATERMARK_DATASIZE (8)
#define GDC_AXI_SETTINGS_TILE_READER_FIFO_WATERMARK_OFFSET (0xac)
#define GDC_AXI_SETTINGS_TILE_READER_FIFO_WATERMARK_MASK (0xff00)

// args: data (8-bit)
static inline void
gdc_axi_settings_tile_reader_fifo_watermark_write(uint8_t data)
{
	u32 curr = system_gdc_read_32(0xacL, 0);
	u32 val = ((data & 0xff) << 8) | (curr & 0xffff00ff);

	system_gdc_write_32(0xacL, val, 0);
}

static inline uint8_t gdc_axi_settings_tile_reader_fifo_watermark_read(void)
{
	return (uint8_t)((system_gdc_read_32(0xacL, 0) & 0xff00) >> 8);
}

// ----------------------------------- //
// Register: tile reader rxact maxostand
// ----------------------------------- //

// ----------------------------------- //
// Max outstanding read transactions
// (bursts) allowed. zero means no maximum(uses fifo size as max).
// ----------------------------------- //

#define GDC_AXI_SETTINGS_TILE_READER_RXACT_MAXOSTAND_DEFAULT (0x00)
#define GDC_AXI_SETTINGS_TILE_READER_RXACT_MAXOSTAND_DATASIZE (8)
#define GDC_AXI_SETTINGS_TILE_READER_RXACT_MAXOSTAND_OFFSET (0xac)
#define GDC_AXI_SETTINGS_TILE_READER_RXACT_MAXOSTAND_MASK (0xff0000)

// args: data (8-bit)
static inline void
gdc_axi_settings_tile_reader_rxact_maxostand_write(uint8_t data)
{
	u32 curr = system_gdc_read_32(0xacL, 0);
	u32 val = ((data & 0xff) << 16) | (curr & 0xff00ffff);

	system_gdc_write_32(0xacL, val, 0);
}

static inline uint8_t gdc_axi_settings_tile_reader_rxact_maxostand_read(void)
{
	return (uint8_t)((system_gdc_read_32(0xacL, 0) & 0xff0000) >> 16);
}

// ----------------------------------- //
// Register: tile writer max awlen
// ----------------------------------- //

// ----------------------------------- //
// Maximum value to use for awlen (axi burst length).
// "0000"= max 1 transfer/burst , upto "1111"= max 16 transfers/burst
// ----------------------------------- //

#define GDC_AXI_SETTINGS_TILE_WRITER_MAX_AWLEN_DEFAULT (0xF)
#define GDC_AXI_SETTINGS_TILE_WRITER_MAX_AWLEN_DATASIZE (4)
#define GDC_AXI_SETTINGS_TILE_WRITER_MAX_AWLEN_OFFSET (0xb0)
#define GDC_AXI_SETTINGS_TILE_WRITER_MAX_AWLEN_MASK (0xf)

// args: data (4-bit)
static inline void gdc_axi_settings_tile_writer_max_awlen_write(uint8_t data)
{
	u32 curr = system_gdc_read_32(0xb0L, 0);
	u32 val = ((data & 0xf) << 0) | (curr & 0xfffffff0);

	system_gdc_write_32(0xb0L, val, 0);
}

static inline uint8_t gdc_axi_settings_tile_writer_max_awlen_read(void)
{
	return (uint8_t)((system_gdc_read_32(0xb0L, 0) & 0xf) >> 0);
}

// ----------------------------------- //
// Register: tile writer fifo watermark
// ----------------------------------- //

// ----------------------------------- //
// Number of words in fifo before AXI write burst(s) start
// (legal values = max_burst_length(max_awlen+1) to 2**fifo_aw,
// but workable value for your system are probably less!).
// Allowing n back to back bursts to generated
// if watermark is set to n*burst length.
// Burst(s) continue while fifo has enough for next burst.
// ----------------------------------- //

#define GDC_AXI_SETTINGS_TILE_WRITER_FIFO_WATERMARK_DEFAULT (0x10)
#define GDC_AXI_SETTINGS_TILE_WRITER_FIFO_WATERMARK_DATASIZE (8)
#define GDC_AXI_SETTINGS_TILE_WRITER_FIFO_WATERMARK_OFFSET (0xb0)
#define GDC_AXI_SETTINGS_TILE_WRITER_FIFO_WATERMARK_MASK (0xff00)

// args: data (8-bit)
static inline void
gdc_axi_settings_tile_writer_fifo_watermark_write(uint8_t data)
{
	u32 curr = system_gdc_read_32(0xb0L, 0);
	u32 val = ((data & 0xff) << 8) | (curr & 0xffff00ff);

	system_gdc_write_32(0xb0L, val, 0);
}

static inline uint8_t
gdc_axi_settings_tile_writer_fifo_watermark_read(void)
{
	return (uint8_t)((system_gdc_read_32(0xb0L, 0) & 0xff00) >> 8);
}

// ----------------------------------- //
// Register: tile writer wxact maxostand
// ----------------------------------- //

// ----------------------------------- //
// Max outstanding write transactions (bursts)
// allowed. zero means no maximum(uses internal limit of 2048)
// ----------------------------------- //

#define GDC_AXI_SETTINGS_TILE_WRITER_WXACT_MAXOSTAND_DEFAULT (0x00)
#define GDC_AXI_SETTINGS_TILE_WRITER_WXACT_MAXOSTAND_DATASIZE (8)
#define GDC_AXI_SETTINGS_TILE_WRITER_WXACT_MAXOSTAND_OFFSET (0xb0)
#define GDC_AXI_SETTINGS_TILE_WRITER_WXACT_MAXOSTAND_MASK (0xff0000)

// args: data (8-bit)
static inline void
gdc_axi_settings_tile_writer_wxact_maxostand_write(uint8_t data)
{
	u32 curr = system_gdc_read_32(0xb0L, 0);
	u32 val = ((data & 0xff) << 16) | (curr & 0xff00ffff);

	system_gdc_write_32(0xb0L, val, 0);
}

static inline uint8_t gdc_axi_settings_tile_writer_wxact_maxostand_read(void)
{
	return (uint8_t)((system_gdc_read_32(0xb0L, 0) & 0xff0000) >> 16);
}

// ----------------------------------- //
#endif //__GDC_CONFIG_H__
