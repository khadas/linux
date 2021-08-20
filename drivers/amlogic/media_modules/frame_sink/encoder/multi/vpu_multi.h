/*
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Description:
 */
#ifndef __VPU_MULTI_DRV_H__
#define __VPU_MULTI_DRV_H__

#include <linux/fs.h>
#include <linux/types.h>
#include <linux/compat.h>
#include <linux/dma-buf.h>

#define SUPPORT_SOURCE_RELEASE_INTERRUPT

/* DO NOT CHANGE THIS VALUE */
#define MAX_INST_HANDLE_SIZE	48
#define MAX_NUM_INSTANCE	6
#define MAX_NUM_VPU_CORE	1

#ifdef CONFIG_COMPAT
struct compat_vpudrv_buffer_t {
	u32 size;
	u32 cached;
	compat_ulong_t phys_addr;
	compat_ulong_t base; /* kernel logical address in use kernel */
	compat_ulong_t virt_addr; /* virtual user space address */
};
#endif

struct vpudrv_buffer_t {
	u32 size;
	u32 cached;
	ulong phys_addr;
	ulong base; /* kernel logical address in use kernel */
	ulong virt_addr; /* virtual user space address */
};

struct vpu_bit_firmware_info_t {
	u32 size; /* size of this structure*/
	u32 core_idx;
	u32 reg_base_offset;
	u16 bit_code[512];
};

struct vpudrv_inst_info_t {
	u32 core_idx;
	u32 inst_idx;
	s32 inst_open_count; /* for output only*/
};

struct vpudrv_intr_info_t {
	u32 timeout;
	s32 intr_reason;
	s32	intr_inst_index;
};

struct vpu_drv_context_t {
	struct fasync_struct *async_queue;
	ulong interrupt_reason[MAX_NUM_INSTANCE];
	u32 interrupt_flag[MAX_NUM_INSTANCE];
	u32 open_count; /*!<< device reference count. Not instance count */
};

/* To track the allocated memory buffer */
struct vpudrv_buffer_pool_t {
	struct list_head list;
	struct vpudrv_buffer_t vb;
	struct file *filp;
};

/* To track the instance index and buffer in instance pool */
struct vpudrv_instanace_list_t {
	struct list_head list;
	ulong inst_idx;
	ulong core_idx;
	struct file *filp;
};

struct vpudrv_instance_pool_t {
	u8 codecInstPool[MAX_NUM_INSTANCE][MAX_INST_HANDLE_SIZE];
};

struct vpudrv_dma_buf_info_t {
	u32 num_planes;
	int fd[3];
	ulong phys_addr[3]; /* phys address for DMA buffer */
};

//hoan add for canvas
struct vpudrv_dma_buf_canvas_info_t {
	u32 num_planes;
	u32 canvas_index;
	int fd[3];
	ulong phys_addr[3]; /* phys address for DMA buffer */
};



//end



#ifdef CONFIG_COMPAT
struct compat_vpudrv_dma_buf_info_t {
	u32 num_planes;
	compat_int_t fd[3];
	compat_ulong_t phys_addr[3]; /* phys address for DMA buffer */
};


struct compat_vpudrv_dma_buf_canvas_info_t {
	u32 num_planes;
	u32 canvas_index;
	compat_int_t fd[3];
	compat_ulong_t phys_addr[3]; /* phys address for DMA buffer */
};


#endif

struct vpu_dma_cfg {
	int fd;
	void *dev;
	void *vaddr;
	unsigned long paddr;
	struct dma_buf *dbuf;
	struct dma_buf_attachment *attach;
	struct sg_table *sg;
	enum dma_data_direction dir;
};

/* To track the occupied dma_buf  */
struct vpudrv_dma_buf_pool_t {
	struct list_head list;
	struct vpu_dma_cfg dma_cfg;
	struct file *filp;
};

#define VPUDRV_BUF_LEN struct vpudrv_buffer_t
#define VPUDRV_INST_LEN struct vpudrv_inst_info_t
#define VPUDRV_DMABUF_LEN struct vpudrv_dma_buf_info_t
	//hoan add for canvas
#define VPUDRV_DMABUF_CANVAS_LEN struct vpudrv_dma_buf_canvas_info_t
	//end

#ifdef CONFIG_COMPAT
#define VPUDRV_BUF_LEN32 struct compat_vpudrv_buffer_t
#define VPUDRV_DMABUF_LEN32 struct compat_vpudrv_dma_buf_info_t
		//hoan add for canvas
#define VPUDRV_DMABUF_CANVAS_LEN32 struct compat_vpudrv_dma_buf_canvas_info_t
		//end

#endif

#define VDI_MAGIC  'V'
#define VDI_IOCTL_ALLOCATE_PHYSICAL_MEMORY \
	_IOW(VDI_MAGIC, 0, VPUDRV_BUF_LEN)

#define VDI_IOCTL_FREE_PHYSICALMEMORY \
	_IOW(VDI_MAGIC, 1, VPUDRV_BUF_LEN)

#define VDI_IOCTL_WAIT_INTERRUPT \
	_IOW(VDI_MAGIC, 2, struct vpudrv_intr_info_t)

#define VDI_IOCTL_SET_CLOCK_GATE \
	_IOW(VDI_MAGIC, 3, u32)

#define VDI_IOCTL_RESET \
	_IOW(VDI_MAGIC, 4, u32)

#define VDI_IOCTL_GET_INSTANCE_POOL \
	_IOW(VDI_MAGIC, 5, VPUDRV_BUF_LEN)

#define VDI_IOCTL_GET_COMMON_MEMORY \
	_IOW(VDI_MAGIC, 6, VPUDRV_BUF_LEN)

#define VDI_IOCTL_GET_RESERVED_VIDEO_MEMORY_INFO \
	_IOW(VDI_MAGIC, 8, VPUDRV_BUF_LEN)

#define VDI_IOCTL_OPEN_INSTANCE \
	_IOW(VDI_MAGIC, 9, VPUDRV_INST_LEN)

#define VDI_IOCTL_CLOSE_INSTANCE \
	_IOW(VDI_MAGIC, 10, VPUDRV_INST_LEN)

#define VDI_IOCTL_GET_INSTANCE_NUM \
	_IOW(VDI_MAGIC, 11, VPUDRV_INST_LEN)

#define VDI_IOCTL_GET_REGISTER_INFO \
	_IOW(VDI_MAGIC, 12, VPUDRV_BUF_LEN)

#define VDI_IOCTL_GET_FREE_MEM_SIZE	\
	_IOW(VDI_MAGIC, 13, u32)

#define VDI_IOCTL_FLUSH_BUFFER \
	_IOW(VDI_MAGIC, 14, VPUDRV_BUF_LEN)

#define VDI_IOCTL_CACHE_INV_BUFFER \
	_IOW(VDI_MAGIC, 15, VPUDRV_BUF_LEN)

#define VDI_IOCTL_CONFIG_DMA \
	_IOW(VDI_MAGIC, 16, VPUDRV_DMABUF_LEN)

#define VDI_IOCTL_UNMAP_DMA \
	_IOW(VDI_MAGIC, 17, VPUDRV_DMABUF_LEN)


//hoan add for canvas
#define VDI_IOCTL_READ_CANVAS \
	_IOW(VDI_MAGIC, 20, VPUDRV_DMABUF_CANVAS_LEN)
//end

#ifdef CONFIG_COMPAT
#define VDI_IOCTL_ALLOCATE_PHYSICAL_MEMORY32 \
	_IOW(VDI_MAGIC, 0, VPUDRV_BUF_LEN32)

#define VDI_IOCTL_FREE_PHYSICALMEMORY32 \
	_IOW(VDI_MAGIC, 1, VPUDRV_BUF_LEN32)

#define VDI_IOCTL_GET_INSTANCE_POOL32 \
	_IOW(VDI_MAGIC, 5, VPUDRV_BUF_LEN32)

#define VDI_IOCTL_GET_COMMON_MEMORY32 \
	_IOW(VDI_MAGIC, 6, VPUDRV_BUF_LEN32)

#define VDI_IOCTL_GET_RESERVED_VIDEO_MEMORY_INFO32 \
	_IOW(VDI_MAGIC, 8, VPUDRV_BUF_LEN32)

#define VDI_IOCTL_GET_REGISTER_INFO32 \
	_IOW(VDI_MAGIC, 12, VPUDRV_BUF_LEN32)

#define VDI_IOCTL_FLUSH_BUFFER32 \
	_IOW(VDI_MAGIC, 14, VPUDRV_BUF_LEN32)

#define VDI_IOCTL_CACHE_INV_BUFFER32 \
	_IOW(VDI_MAGIC, 15, VPUDRV_BUF_LEN32)

#define VDI_IOCTL_CONFIG_DMA32 \
	_IOW(VDI_MAGIC, 16, VPUDRV_DMABUF_LEN32)

#define VDI_IOCTL_UNMAP_DMA32 \
	_IOW(VDI_MAGIC, 17, VPUDRV_DMABUF_LEN32)

//hoan add for canvas
#define VDI_IOCTL_READ_CANVAS32 \
	_IOW(VDI_MAGIC, 20, VPUDRV_DMABUF_CANVAS_LEN32)
//end



#endif
/* implement to power management functions */
#define BIT_BASE		0x0000
#define BIT_CODE_RUN		(BIT_BASE + 0x000)
#define BIT_CODE_DOWN		(BIT_BASE + 0x004)
#define BIT_INT_CLEAR		(BIT_BASE + 0x00C)
#define BIT_INT_STS		(BIT_BASE + 0x010)
#define BIT_CODE_RESET		(BIT_BASE + 0x014)
#define BIT_INT_REASON		(BIT_BASE + 0x174)
#define BIT_BUSY_FLAG		(BIT_BASE + 0x160)
#define BIT_RUN_COMMAND		(BIT_BASE + 0x164)
#define BIT_RUN_INDEX		(BIT_BASE + 0x168)
#define BIT_RUN_COD_STD		(BIT_BASE + 0x16C)


#define VPU_REG_BASE_ADDR 0xFE070000
#define VPU_REG_SIZE (0x4000*MAX_NUM_VPU_CORE)

/* registers */
#define VP5_REG_BASE			0x0000
#define VP5_VPU_BUSY_STATUS		(VP5_REG_BASE + 0x0070)
#define VP5_VPU_INT_REASON_CLEAR	(VP5_REG_BASE + 0x0034)
#define VP5_VPU_VINT_CLEAR		(VP5_REG_BASE + 0x003C)
#define VP5_VPU_VPU_INT_STS		(VP5_REG_BASE + 0x0044)
#define VP5_VPU_INT_REASON		(VP5_REG_BASE + 0x004c)
#define VP5_RET_FAIL_REASON		(VP5_REG_BASE + 0x010C)

#define VP5_RET_BS_EMPTY_INST		(VP5_REG_BASE + 0x01E4)
#define VP5_RET_QUEUE_CMD_DONE_INST	(VP5_REG_BASE + 0x01E8)
#define VP5_RET_SEQ_DONE_INSTANCE_INFO	(VP5_REG_BASE + 0x01FC)

/* interrrupt bits */
enum {
	INT_INIT_VPU		= 0,
	INT_WAKEUP_VPU		= 1,
	INT_SLEEP_VPU		= 2,
	INT_CREATE_INSTANCE	= 3,
	INT_FLUSH_INSTANCE	= 4,
	INT_DESTORY_INSTANCE	= 5,
	INT_INIT_SEQ		= 6,
	INT_SET_FRAMEBUF	= 7,
	INT_DEC_PIC		= 8,
	INT_ENC_PIC		= 8,
	INT_ENC_SET_PARAM	= 9,
#ifdef SUPPORT_SOURCE_RELEASE_INTERRUPT
	INT_ENC_SRC_RELEASE	= 10,
#endif
	INT_ENC_LOW_LATENCY	= 13,
	INT_DEC_QUERY		= 14,
	INT_BSBUF_EMPTY		= 15,
	INT_BSBUF_FULL		= 15,
};

/* INIT, WAKEUP */
#define VP5_PO_CONF			(VP5_REG_BASE + 0x0000)
#define VP5_VPU_VINT_ENABLE		(VP5_REG_BASE + 0x0048)

#define VP5_VPU_RESET_REQ		(VP5_REG_BASE + 0x0050)
#define VP5_VPU_RESET_STATUS		(VP5_REG_BASE + 0x0054)

#define VP5_VPU_REMAP_CTRL		(VP5_REG_BASE + 0x0060)
#define VP5_VPU_REMAP_VADDR		(VP5_REG_BASE + 0x0064)
#define VP5_VPU_REMAP_PADDR		(VP5_REG_BASE + 0x0068)
#define VP5_VPU_REMAP_CORE_START	(VP5_REG_BASE + 0x006C)

#define VP5_REMAP_CODE_INDEX		0

/*VPU registers */
#define VP5_ADDR_CODE_BASE		(VP5_REG_BASE + 0x0110)
#define VP5_CODE_SIZE			(VP5_REG_BASE + 0x0114)
#define VP5_CODE_PARAM			(VP5_REG_BASE + 0x0118)
#define VP5_INIT_VPU_TIME_OUT_CNT	(VP5_REG_BASE + 0x0130)
#define VP5_HW_OPTION			(VP5_REG_BASE + 0x012C)
#define VP5_RET_SUCCESS			(VP5_REG_BASE + 0x0108)
#define VP5_COMMAND			(VP5_REG_BASE + 0x0100)
#define VP5_VPU_HOST_INT_REQ		(VP5_REG_BASE + 0x0038)
/* Product register */
#define VPU_PRODUCT_CODE_REGISTER	(BIT_BASE + 0x1044)

#define ReadVpuRegister(addr) \
	readl((void __iomem *)(s_vpu_register.virt_addr \
	+ s_bit_firmware_info[core].reg_base_offset + addr))

#define WriteVpuRegister(addr, val) \
	writel((u32)val, (void __iomem *)(s_vpu_register.virt_addr \
	+ s_bit_firmware_info[core].reg_base_offset + addr))

#define WriteVpu(addr, val) writel((u32)val, (void __iomem *)addr)
#endif
