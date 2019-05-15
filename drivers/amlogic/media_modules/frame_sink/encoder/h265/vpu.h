/*
 * vpu.h
 *
 * linux device driver for VPU.
 *
 * Copyright (C) 2006 - 2013  CHIPS&MEDIA INC.
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
 */

#ifndef __VPU_DRV_H__
#define __VPU_DRV_H__

#include <linux/fs.h>
#include <linux/types.h>
#include <linux/compat.h>

#define MAX_INST_HANDLE_SIZE	        (32*1024)
#define MAX_NUM_INSTANCE                4
#define MAX_NUM_VPU_CORE                1

#define W4_CMD_INIT_VPU				(0x0001)
#define W4_CMD_SLEEP_VPU				(0x0400)
#define W4_CMD_WAKEUP_VPU			(0x0800)

/* GXM: 2000/10 = 200M */
#define HevcEnc_L0()   WRITE_HHI_REG(HHI_WAVE420L_CLK_CNTL, \
			 (3 << 25) | (1 << 16) | (3 << 9) | (1 << 0))
/* GXM: 2000/8 = 250M */
#define HevcEnc_L1()   WRITE_HHI_REG(HHI_WAVE420L_CLK_CNTL, \
			 (1 << 25) | (1 << 16) | (1 << 9) | (1 << 0))
/* GXM: 2000/7 = 285M */
#define HevcEnc_L2()   WRITE_HHI_REG(HHI_WAVE420L_CLK_CNTL, \
			 (4 << 25) | (0 << 16) | (4 << 9) | (0 << 0))
/*GXM: 2000/6 = 333M */
#define HevcEnc_L3()   WRITE_HHI_REG(HHI_WAVE420L_CLK_CNTL, \
			 (2 << 25) | (1 << 16) | (2 << 9) | (1 << 0))
/* GXM: 2000/5 = 400M */
#define HevcEnc_L4()   WRITE_HHI_REG(HHI_WAVE420L_CLK_CNTL, \
			 (3 << 25) | (0 << 16) | (3 << 9) | (0 << 0))
/* GXM: 2000/4 = 500M */
#define HevcEnc_L5()   WRITE_HHI_REG(HHI_WAVE420L_CLK_CNTL, \
			 (1 << 25) | (0 << 16) | (1 << 9) | (0 << 0))
/* GXM: 2000/3 = 667M */
#define HevcEnc_L6()   WRITE_HHI_REG(HHI_WAVE420L_CLK_CNTL, \
			 (2 << 25) | (0 << 16) | (2 << 9) | (0 << 0))

#define HevcEnc_clock_enable(level) \
	do { \
		WRITE_HHI_REG(HHI_WAVE420L_CLK_CNTL, \
			READ_HHI_REG(HHI_WAVE420L_CLK_CNTL) \
			& (~(1 << 8)) & (~(1 << 24))); \
		if (level == 0)  \
			HevcEnc_L0(); \
		else if (level == 1)  \
			HevcEnc_L1(); \
		else if (level == 2)  \
			HevcEnc_L2(); \
		else if (level == 3)  \
			HevcEnc_L3(); \
		else if (level == 4)  \
			HevcEnc_L4(); \
		else if (level == 5)  \
			HevcEnc_L5(); \
		else if (level == 6)  \
			HevcEnc_L6(); \
		WRITE_HHI_REG(HHI_WAVE420L_CLK_CNTL, \
			READ_HHI_REG(HHI_WAVE420L_CLK_CNTL) \
			| (1 << 8) | (1 << 24)); \
	} while (0)

#define HevcEnc_clock_disable() \
	WRITE_HHI_REG(HHI_WAVE420L_CLK_CNTL, \
	READ_HHI_REG(HHI_WAVE420L_CLK_CNTL) \
		& (~(1 << 8)) & (~(1 << 24)))

/* ACLK 667MHZ */
#define HevcEnc_MoreClock_enable() \
	do { \
		WRITE_HHI_REG(HHI_WAVE420L_CLK_CNTL2, \
			READ_HHI_REG(HHI_WAVE420L_CLK_CNTL2) \
			& (~(1 << 8))); \
		WRITE_HHI_REG(HHI_WAVE420L_CLK_CNTL2, \
			(2 << 9) | (0 << 0)); \
		WRITE_HHI_REG(HHI_WAVE420L_CLK_CNTL2, \
			READ_HHI_REG(HHI_WAVE420L_CLK_CNTL2) \
			| (1 << 8)); \
	} while (0)

#define HevcEnc_MoreClock_disable() \
	WRITE_HHI_REG(HHI_WAVE420L_CLK_CNTL2, \
	READ_HHI_REG(HHI_WAVE420L_CLK_CNTL2) \
		& (~(1 << 8)))

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
	s32 inst_open_count;	/* for output only*/
};

struct vpudrv_intr_info_t {
	u32 timeout;
	s32 intr_reason;
};

struct vpu_drv_context_t {
	struct fasync_struct *async_queue;
	ulong interrupt_reason;
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

#define VPUDRV_BUF_LEN struct vpudrv_buffer_t
#define VPUDRV_BUF_LEN32 struct compat_vpudrv_buffer_t
#define VPUDRV_INST_LEN struct vpudrv_inst_info_t

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

#define VDI_IOCTL_FLUSH_BUFFER \
	_IOW(VDI_MAGIC, 13, VPUDRV_BUF_LEN)

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
	_IOW(VDI_MAGIC, 13, VPUDRV_BUF_LEN32)
#endif

enum {
	W4_INT_INIT_VPU = 0,
	W4_INT_DEC_PIC_HDR = 1,
	W4_INT_SET_PARAM = 1,
	W4_INT_ENC_INIT_SEQ = 1,
	W4_INT_FINI_SEQ = 2,
	W4_INT_DEC_PIC = 3,
	W4_INT_ENC_PIC = 3,
	W4_INT_SET_FRAMEBUF = 4,
	W4_INT_FLUSH_DEC = 5,
	W4_INT_ENC_SLICE_INT = 7,
	W4_INT_GET_FW_VERSION = 8,
	W4_INT_QUERY_DEC = 9,
	W4_INT_SLEEP_VPU = 10,
	W4_INT_WAKEUP_VPU = 11,
	W4_INT_CHANGE_INT = 12,
	W4_INT_CREATE_INSTANCE = 14,
	W4_INT_BSBUF_EMPTY = 15,
    /*!<< Bitstream buffer empty[dec]/full[enc] */
};

/* WAVE4 registers */
#define VPU_REG_BASE_ADDR	0xc8810000
#define VPU_REG_SIZE	(0x4000 * MAX_NUM_VPU_CORE)

#define W4_REG_BASE					0x0000
#define W4_VPU_BUSY_STATUS			(W4_REG_BASE + 0x0070)
#define W4_VPU_INT_REASON_CLEAR			(W4_REG_BASE + 0x0034)
#define W4_VPU_VINT_CLEAR				(W4_REG_BASE + 0x003C)
#define W4_VPU_VPU_INT_STS			(W4_REG_BASE + 0x0044)
#define W4_VPU_INT_REASON				(W4_REG_BASE + 0x004c)

#define W4_RET_SUCCESS					(W4_REG_BASE + 0x0110)
#define W4_RET_FAIL_REASON			(W4_REG_BASE + 0x0114)

/* WAVE4 INIT, WAKEUP */
#define W4_PO_CONF					(W4_REG_BASE + 0x0000)
#define W4_VCPU_CUR_PC					(W4_REG_BASE + 0x0004)

#define W4_VPU_VINT_ENABLE			(W4_REG_BASE + 0x0048)

#define W4_VPU_RESET_REQ				(W4_REG_BASE + 0x0050)
#define W4_VPU_RESET_STATUS			(W4_REG_BASE + 0x0054)

#define W4_VPU_REMAP_CTRL				(W4_REG_BASE + 0x0060)
#define W4_VPU_REMAP_VADDR			(W4_REG_BASE + 0x0064)
#define W4_VPU_REMAP_PADDR			(W4_REG_BASE + 0x0068)
#define W4_VPU_REMAP_CORE_START			(W4_REG_BASE + 0x006C)
#define W4_VPU_BUSY_STATUS			(W4_REG_BASE + 0x0070)

#define W4_HW_OPTION					(W4_REG_BASE + 0x0124)
#define W4_CODE_SIZE					(W4_REG_BASE + 0x011C)
/* Note: W4_INIT_CODE_BASE_ADDR should be aligned to 4KB */
#define W4_ADDR_CODE_BASE			(W4_REG_BASE + 0x0118)
#define W4_CODE_PARAM					(W4_REG_BASE + 0x0120)
#define W4_INIT_VPU_TIME_OUT_CNT		(W4_REG_BASE + 0x0134)

/* WAVE4 Wave4BitIssueCommand */
#define W4_CORE_INDEX					(W4_REG_BASE + 0x0104)
#define W4_INST_INDEX					(W4_REG_BASE + 0x0108)
#define W4_COMMAND					(W4_REG_BASE + 0x0100)
#define W4_VPU_HOST_INT_REQ			(W4_REG_BASE + 0x0038)

#define W4_BS_RD_PTR					(W4_REG_BASE + 0x0130)
#define W4_BS_WR_PTR					(W4_REG_BASE + 0x0134)
#define W4_RET_ENC_PIC_BYTE			(W4_REG_BASE + 0x01C8)

#define W4_REMAP_CODE_INDEX			 0

#define ReadVpuRegister(addr) \
	readl((void __iomem *)(s_vpu_register.virt_addr \
	+ s_bit_firmware_info[core].reg_base_offset + addr))

#define WriteVpuRegister(addr, val) \
	writel((u32)val, (void __iomem *)(s_vpu_register.virt_addr \
	+ s_bit_firmware_info[core].reg_base_offset + addr))

#define WriteVpu(addr, val) writel((u32)val, (void __iomem *)addr)
#endif
