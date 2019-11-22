/*
 * drivers/amlogic/media/vin/tvin/tvafe/tvafe_vbi.c
 *
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
 */

/* Standard Linux headers */
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_reserved_mem.h>
#include <linux/dma-mapping.h>
#include <linux/of_irq.h>

/* #include <linux/mutex.h> */
#include <linux/mm.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/poll.h>
#include <linux/vmalloc.h>
#include <linux/io.h> /* for virt_to_phys */


/* Local include */
#include "tvafe_regs.h"
#include "tvafe_cvd.h"
#include "tvafe_vbi.h"
#include "tvafe_debug.h"
#include "../tvin_global.h"

#define VBI_NAME               "vbi"
#define VBI_DRIVER_NAME        "vbi"
#define VBI_MODULE_NAME        "vbi"
#define VBI_DEVICE_NAME        "vbi"
#define VBI_CLASS_NAME         "vbi"

#define VBI_VS_DELAY 4
#define VBI_START_DELAY 1000

static dev_t vbi_id;
static struct class *vbi_clsp;
static struct vbi_dev_s *vbi_dev_local;

static struct resource vbi_memobj;
#define VBI_MEM_NONE        0
#define VBI_MEM_RESERVED    1
#define VBI_MEM_MALLOC      2
static unsigned int vbi_mem_flag;
static char *vbi_pr_isr_buf, *vbi_pr_read_buf;
#define VBI_PR_MAX    500

/******debug********/
#define VBI_DBG_INFO       (1 << 0)  /*important info*/
#define VBI_DBG_INFO2      (1 << 1)
#define VBI_DBG_ISR        (1 << 4)
#define VBI_DBG_ISR2       (1 << 5)
#define VBI_DBG_ISR3       (1 << 6)
#define VBI_DBG_ISR4       (1 << 7)
#define VBI_DBG_READ       (1 << 8)
#define VBI_DBG_READ2      (1 << 9)
#define VBI_DBG_READ3      (1 << 10)
#define VBI_DBG_READ4      (1 << 11)
#define VBI_DBG_TEST       (1 << 12)  /* teletext page201 error report */
#define VBI_DBG_TEST2      (1 << 13)  /* teletext page201 data print */
static unsigned int vbi_dbg_en;

#define VBI_DBG_CNT        100
static unsigned int vbi_dbg_isr_cnt;
static unsigned int vbi_dbg_read_cnt;
static unsigned int vbi_dbg_print_cnt = VBI_DBG_CNT;

static bool vbi_nonblock_en;
MODULE_PARM_DESC(vbi_nonblock_en, "\n vbi_nonblock_en\n");
module_param(vbi_nonblock_en, bool, 0664);

/*vbi will be delayed when interval larger than 2*/
static unsigned int vbi_read_wakeup_interval = 2;
MODULE_PARM_DESC(vbi_read_wakeup_interval, "\n vbi_read_wakeup_interval\n");
module_param(vbi_read_wakeup_interval, uint, 0664);

static unsigned int vcnt = 1;
static unsigned int vbi_read_wakeup_cnt;
static unsigned int vbi_data_stable_flag;
static unsigned int init_vbi_flag;
static unsigned char field_data_flag;
static unsigned int vbi_search_table_err_num;
static unsigned int vbi_vcnt_search_retry;
static unsigned int vbi_slicer_bypass;
static unsigned int vbi_data_tt_check;

#define VBI_VCNT_SEARCH_CNT_MAX     5

static void vbi_hw_reset(struct vbi_dev_s *devp)
{
	/*W_VBI_APB_REG(ACD_REG_22, 0x82080000);*/
	/*W_VBI_APB_REG(ACD_REG_22, 0x04080000);*/
	/* after hw reset,bellow parameters must be reset!!! */
	vcnt = 1;
	devp->pac_addr = devp->pac_addr_start;
	devp->current_pac_wptr = 0;
}

static void vbi_state_reset(struct vbi_dev_s *devp)
{
	devp->vs_delay = VBI_VS_DELAY;
	devp->isr_cnt = 0;
	devp->slicer->slicer_cnt = 0;
	vcnt = 1;
	vbi_read_wakeup_cnt = 0;
	init_vbi_flag = 0;
	vbi_data_stable_flag = 0;
	field_data_flag = 0;
}

static void vbi_tt_start_code_set(struct vbi_dev_s *devp)
{
	unsigned int vbi_start_code = devp->vbi_start_code;

	/* start code */
	W_VBI_APB_REG(CVD2_VBI_TT_FRAME_CODE_CTL, vbi_start_code);
}

static void vbi_data_type_set(struct vbi_dev_s *devp)
{
	unsigned int vbi_data_type = devp->vbi_data_type;

	/* data type */
	W_VBI_APB_REG(CVD2_VBI_DATA_TYPE_LINE6,  vbi_data_type);
	W_VBI_APB_REG(CVD2_VBI_DATA_TYPE_LINE7, vbi_data_type);
	W_VBI_APB_REG(CVD2_VBI_DATA_TYPE_LINE8, vbi_data_type);
	W_VBI_APB_REG(CVD2_VBI_DATA_TYPE_LINE9, vbi_data_type);
	W_VBI_APB_REG(CVD2_VBI_DATA_TYPE_LINE10, vbi_data_type);
	W_VBI_APB_REG(CVD2_VBI_DATA_TYPE_LINE11, vbi_data_type);
	W_VBI_APB_REG(CVD2_VBI_DATA_TYPE_LINE12, vbi_data_type);
	W_VBI_APB_REG(CVD2_VBI_DATA_TYPE_LINE13, vbi_data_type);
	W_VBI_APB_REG(CVD2_VBI_DATA_TYPE_LINE14, vbi_data_type);
	W_VBI_APB_REG(CVD2_VBI_DATA_TYPE_LINE15, vbi_data_type);
	W_VBI_APB_REG(CVD2_VBI_DATA_TYPE_LINE16, vbi_data_type);
	W_VBI_APB_REG(CVD2_VBI_DATA_TYPE_LINE17, vbi_data_type);
	W_VBI_APB_REG(CVD2_VBI_DATA_TYPE_LINE18, vbi_data_type);
	W_VBI_APB_REG(CVD2_VBI_DATA_TYPE_LINE19, vbi_data_type);
	W_VBI_APB_REG(CVD2_VBI_DATA_TYPE_LINE20, vbi_data_type);
	W_VBI_APB_REG(CVD2_VBI_DATA_TYPE_LINE21, vbi_data_type);
	W_VBI_APB_REG(CVD2_VBI_DATA_TYPE_LINE22, vbi_data_type);
	W_VBI_APB_REG(CVD2_VBI_DATA_TYPE_LINE23, vbi_data_type);
	W_VBI_APB_REG(CVD2_VBI_DATA_TYPE_LINE24, vbi_data_type);
	W_VBI_APB_REG(CVD2_VBI_DATA_TYPE_LINE25, vbi_data_type);
	/*W_VBI_APB_REG(CVD2_VBI_DATA_TYPE_LINE26, vbi_data_type);*/
}

static void vbi_dto_set(struct vbi_dev_s *devp)
{
	unsigned int dto_cc, dto_tt, dto_wss, dto_vps;

	dto_cc = devp->vbi_dto_cc;
	dto_tt = devp->vbi_dto_tt;
	dto_wss = devp->vbi_dto_wss;
	dto_vps = devp->vbi_dto_vps;
	W_VBI_APB_REG(CVD2_VBI_CC_DTO_MSB,	     ((dto_cc>>8) & 0xff));
	W_VBI_APB_REG(CVD2_VBI_CC_DTO_LSB,	     (dto_cc & 0xff));
	W_VBI_APB_REG(CVD2_VBI_TT_DTO_MSB,	     ((dto_tt>>8) & 0xff));
	W_VBI_APB_REG(CVD2_VBI_TT_DTO_LSB,	     (dto_tt & 0xff));
	W_VBI_APB_REG(CVD2_VBI_WSS_DTO_MSB,	     ((dto_wss>>8) & 0xff));
	W_VBI_APB_REG(CVD2_VBI_WSS_DTO_LSB,	     (dto_wss & 0xff));
	W_VBI_APB_REG(CVD2_VBI_VPS_DTO_MSB,	     ((dto_vps>>8) & 0xff));
	W_VBI_APB_REG(CVD2_VBI_VPS_DTO_LSB,	     (dto_vps & 0xff));

}

/* hw reset,config vbi line data type,start code,dto */
static void vbi_slicer_type_set(struct vbi_dev_s *devp)
{
	enum vbi_slicer_e slicer_type = devp->slicer->type;

	vbi_hw_reset(devp);
	switch (slicer_type) {
	case VBI_TYPE_USCC:
		devp->vbi_data_type = VBI_DATA_TYPE_USCC;
		devp->vbi_start_code = VBI_START_CODE_USCC;
		devp->vbi_dto_cc = VBI_DTO_USCC;
		W_VBI_APB_REG(CVD2_VBI_DATA_TYPE_LINE21,
			VBI_DATA_TYPE_USCC);
		break;
	case VBI_TYPE_EUROCC:
		devp->vbi_data_type = VBI_DATA_TYPE_EUROCC;
		devp->vbi_start_code = VBI_START_CODE_EUROCC;
		devp->vbi_dto_cc = VBI_DTO_EURCC;
		/*line18 for PAL M*/
		W_VBI_APB_REG(CVD2_VBI_DATA_TYPE_LINE18,
			VBI_DATA_TYPE_EUROCC);
		/*line22 for PAL B,D,G,H,I,N,CN*/
		W_VBI_APB_REG(CVD2_VBI_DATA_TYPE_LINE22,
			VBI_DATA_TYPE_EUROCC);
		break;
	case VBI_TYPE_VPS:
		devp->vbi_data_type = VBI_DATA_TYPE_VPS;
		devp->vbi_start_code = VBI_START_CODE_VPS;
		devp->vbi_dto_vps = VBI_DTO_VPS;
		W_VBI_APB_REG(CVD2_VBI_DATA_TYPE_LINE16, VBI_DATA_TYPE_VPS);
		break;
	case VBI_TYPE_TT_625A:
		devp->vbi_data_type = VBI_DATA_TYPE_TT_625A;
		devp->vbi_start_code = VBI_START_CODE_TT_625A_REVERSE;
		devp->vbi_dto_tt = VBI_DTO_TT625A;
		break;
	case VBI_TYPE_TT_625B:
		devp->vbi_data_type = VBI_DATA_TYPE_TT_625B;
		devp->vbi_start_code = VBI_START_CODE_TT_625B_REVERSE;
		devp->vbi_dto_tt = VBI_DTO_TT625B;
		break;
	case VBI_TYPE_TT_625C:
		devp->vbi_data_type = VBI_DATA_TYPE_TT_625B;
		devp->vbi_start_code = VBI_START_CODE_TT_625C_REVERSE;
		devp->vbi_dto_tt = VBI_DTO_TT625C;
		break;
	case VBI_TYPE_TT_625D:
		devp->vbi_data_type = VBI_DATA_TYPE_TT_625D;
		devp->vbi_start_code = VBI_START_CODE_TT_625D_REVERSE;
		devp->vbi_dto_tt = VBI_DTO_TT625D;
		break;
	case VBI_TYPE_TT_525B:
		devp->vbi_data_type = VBI_DATA_TYPE_TT_525B;
		devp->vbi_start_code = VBI_START_CODE_TT_525B_REVERSE;
		devp->vbi_dto_tt = VBI_DTO_TT525B;
		break;
	case VBI_TYPE_TT_525C:
		devp->vbi_data_type = VBI_DATA_TYPE_TT_525C;
		devp->vbi_start_code = VBI_START_CODE_TT_525C_REVERSE;
		devp->vbi_dto_tt = VBI_DTO_TT525C;
		break;
	case VBI_TYPE_TT_525D:
		devp->vbi_data_type = VBI_DATA_TYPE_TT_525D;
		devp->vbi_start_code = VBI_START_CODE_TT_525D_REVERSE;
		devp->vbi_dto_tt = VBI_DTO_TT525D;
		break;
	case VBI_TYPE_WSS625:
		devp->vbi_data_type = VBI_DATA_TYPE_WSS625;
		devp->vbi_start_code = VBI_START_CODE_WSS625;
		devp->vbi_dto_wss = VBI_DTO_WSS625;
		/*line17 for PAL M*/
		W_VBI_APB_REG(CVD2_VBI_DATA_TYPE_LINE17,
			VBI_DATA_TYPE_WSS625);
		/*line23 for PAL B,D,G,H,I,N,CN*/
		W_VBI_APB_REG(CVD2_VBI_DATA_TYPE_LINE23,
			VBI_DATA_TYPE_WSS625);
		break;
	case VBI_TYPE_WSSJ:
		devp->vbi_data_type = VBI_DATA_TYPE_WSSJ;
		devp->vbi_start_code = VBI_START_CODE_WSSJ;
		devp->vbi_dto_wss = VBI_DTO_WSSJ;
		W_VBI_APB_REG(CVD2_VBI_DATA_TYPE_LINE20,
			VBI_DATA_TYPE_WSSJ);
		break;
	case VBI_TYPE_NULL:
		devp->vbi_dto_cc = VBI_DTO_USCC;
		devp->vbi_dto_wss = VBI_DTO_WSS625;
		devp->vbi_dto_tt = VBI_DTO_TT625B;
		devp->vbi_dto_vps = VBI_DTO_VPS;
		devp->vbi_data_type = VBI_DATA_TYPE_NULL;
		vbi_data_type_set(devp);
		break;
	default:/*for tt625b+wss625+vps*/
		devp->vbi_dto_cc = VBI_DTO_USCC;
		devp->vbi_dto_wss = VBI_DTO_WSS625;
		devp->vbi_dto_tt = VBI_DTO_TT625B;
		devp->vbi_dto_vps = VBI_DTO_VPS;
		W_VBI_APB_REG(CVD2_VBI_TT_FRAME_CODE_CTL,
			VBI_START_CODE_TT_625B_REVERSE);
		W_VBI_APB_REG(CVD2_VBI_DATA_TYPE_LINE16, VBI_DATA_TYPE_VPS);
		W_VBI_APB_REG(CVD2_VBI_DATA_TYPE_LINE20,
			VBI_DATA_TYPE_TT_625B);
		W_VBI_APB_REG(CVD2_VBI_DATA_TYPE_LINE21,
			VBI_DATA_TYPE_TT_625B);
		W_VBI_APB_REG(CVD2_VBI_DATA_TYPE_LINE23,
			VBI_DATA_TYPE_WSS625);
		break;
	}
	if ((slicer_type >= VBI_TYPE_TT_625A) &&
		(slicer_type <= VBI_TYPE_TT_525D)) {
		vbi_data_type_set(devp);
		vbi_tt_start_code_set(devp);
	}
	vbi_dto_set(devp);
}

static void vbi_hw_init(struct vbi_dev_s *devp)
{
	/* vbi memory setting */
	memset(devp->pac_addr_start, 0, devp->mem_size);
	cvd_vbi_mem_set(devp->mem_start >> 4, devp->mem_size >> 4);
	/*disable vbi*/
	W_VBI_APB_REG(CVD2_VBI_FRAME_CODE_CTL,   0x10);
	cvd_vbi_config();
	/*enable vbi*/
	W_VBI_APB_REG(CVD2_VBI_FRAME_CODE_CTL,   0x11);
	tvafe_pr_info("%s: vbi hw init done.\n", __func__);
}

#define vbi_get_byte(rdptr, retbyte) \
	do {\
		retbyte = *(rdptr); \
		rdptr += 1; \
	} while (0)
#define vbi_skip_bytes(rdptr, total_buffer, devp, nbytes) \
	do {\
		rdptr += nbytes;\
		if (rdptr > devp->pac_addr_end)\
			rdptr = devp->pac_addr_start;\
		total_buffer -= nbytes;\
	} while (0)

static ssize_t vbi_ringbuffer_free(struct vbi_ringbuffer_s *rbuf)
{
	ssize_t free;

	free = rbuf->pread - rbuf->pwrite;
	if (free <= 0) {
		free += rbuf->size;
		if (vbi_dbg_en & VBI_DBG_ISR3)
			tvafe_pr_info("%s: pread: %6d pwrite: %6d\n",
			__func__, rbuf->pread, rbuf->pwrite);
	}
	return free-1;
}


static ssize_t vbi_ringbuffer_write(struct vbi_ringbuffer_s *rbuf,
		const struct vbi_data_s *buf, size_t len)
{
	size_t todo = len;
	size_t split;

	if (rbuf->data_wmode == 1)
		len = rbuf->size;
	split =
	(rbuf->pwrite + len > rbuf->size) ? (rbuf->size - rbuf->pwrite) : 0;
	if (split > 0) {
		memcpy((char *)rbuf->data+rbuf->pwrite, (char *)buf, split);
		todo -= split;
		rbuf->pwrite = 0;
		if (vbi_dbg_en & VBI_DBG_ISR2) {
			tvafe_pr_info("%s: ringbuffer reach max, split size:%d, todo size:%d\n",
				__func__, (int)split, (int)todo);
		}
	}
	memcpy((char *)rbuf->data+rbuf->pwrite, (char *)buf+split, todo);
	rbuf->pwrite = (rbuf->pwrite + todo) % rbuf->size;
	if (vbi_dbg_en & VBI_DBG_ISR3)
		tvafe_pr_info("%s: write finish return: %6Zd\n",
			__func__, len);

	return len;
}


static int vbi_buffer_write(struct vbi_ringbuffer_s *buf,
		   const struct vbi_data_s *src, size_t len)
{
	ssize_t free;

	if (!len) {
		if (vbi_dbg_en & VBI_DBG_INFO)
			tvafe_pr_info("%s: buffer len is zero\n", __func__);
		return 0;
	}

	if ((buf == NULL) || (buf->data == NULL)) {
		if (vbi_dbg_en & VBI_DBG_INFO)
			tvafe_pr_info("%s: buffer data pointer is zero\n",
			__func__);
		return 0;
	}

	free = vbi_ringbuffer_free(buf);
	if (len > free) {
		if (vbi_dbg_en & VBI_DBG_INFO) {
			tvafe_pr_info("%s: buffer overflow, len: %6d, free: %6d\n",
				__func__, (int)len, (int)free);
			tvafe_pr_info("%s: pread: %6d, pwrite: %6d, size: %6d\n",
				__func__, buf->pread, buf->pwrite, buf->size);
		}
		return -EOVERFLOW;
	}

	return vbi_ringbuffer_write(buf, src, len);
}

static irqreturn_t vbi_isr(int irq, void *dev_id)
{
	ulong flags;
	struct vbi_dev_s *devp = (struct vbi_dev_s *)dev_id;

	spin_lock_irqsave(&devp->vbi_isr_lock, flags);
	if (devp->vbi_start == false) {
		spin_unlock_irqrestore(&devp->vbi_isr_lock, flags);
		return IRQ_HANDLED;
	}

	if (devp->vs_delay > 0) {
		devp->vs_delay--;
		spin_unlock_irqrestore(&devp->vbi_isr_lock, flags);
		return IRQ_HANDLED;
	}

	if (devp->isr_cnt++ >= 0xffffffff)
		devp->isr_cnt = 0;

	if (devp->slicer_enable)
		schedule_work(&devp->slicer_work);
	spin_unlock_irqrestore(&devp->vbi_isr_lock, flags);
	return IRQ_HANDLED;
}

static void vbi_ringbuffer_flush(struct vbi_ringbuffer_s *rbuf)
{
	memset(rbuf->data, 0, rbuf->size);
	rbuf->pread = rbuf->pwrite = 0;
	rbuf->error = 0;
	tvafe_pr_info("%s\n", __func__);
}

/*--------------------------mem data----------------------------
 * all vbi memory size:0x100000(get from dts)
 * 0       ~ 0x3ffff: original vbi data          --- VBI_BUFF1_EA
 * 0x40000 ~ 0x40fff: for original data compare  --- VBI_BUFF2_EA
 * 0x41000 ~ 0x41fff: vbi data which will be integration
 *                    to vbi_data_s              --- VBI_BUFF3_EA
 * 0x42000 ~ 0x42fff: for vbi_data_s data compare--- VBI_BUFF4_EA
 *--------------------------------------------------------------
 */
#define  VBI_BUFF1_EA    0x0
#define  VBI_BUFF2_EA    0x40000
#define  VBI_BUFF2_SIZE  0x1000
/*limit data size which be copied to VBI_BUFF3_EA */
#define  VBI_BUFF3_EA    0x41000
#define  VBI_BUFF3_SIZE  0x1000

static int copy_vbi_to(unsigned char *table_start_addr,
	unsigned char *table_end_addr, unsigned char *desc,
	unsigned char *source, int size)
{
	if ((!table_start_addr) || (!table_end_addr) ||
		(!desc) || (!source)) {
		tvafe_pr_info("copy_vbi_to addr NULL.\n");
		return -1;
	} else if ((table_end_addr <= table_start_addr) ||
			(desc <= table_end_addr) || (source > table_end_addr) ||
			(source < table_start_addr)) {
		tvafe_pr_info("copy_vbi_to addr error.\n");
		return -1;
	} else if ((size <= 0) || (size > VBI_BUFF3_SIZE)) {
		tvafe_pr_info("copy_vbi_to size error.\n");
		return -1;
	}

	if (source + size > table_end_addr)
		memcpy((table_end_addr+1), table_start_addr, size);

	memcpy(desc, source, size);
	return 0;
}

/*--------------------search_table()-------------------------
 *  len = (the found addr) - (search_point addr) + 1
 *  return the addr where find the search_content
 *
 *  Example:
 *  table: 0x00 0x11 0x22 0x33 0x44 0x55 0x66 0x77 0x88 0x99
 *  search_point = 0x22's addr
 *  search contet = 0x44 0x55 0x66 0x77
 *  so, len = 6
 *  return 0x77's addr
 *-----------------------------------------------------------
 */
static unsigned char *search_table(unsigned char *table_start_addr,
	unsigned char *search_point, unsigned char *table_end_addr,
	unsigned char *search_content, unsigned char search_size,
	unsigned int *len, unsigned int flag)
{
	unsigned char cflag = 0;
	unsigned int count = 0;
	unsigned int table_size = table_end_addr - table_start_addr + 1;
	unsigned char *p = search_point;

	vbi_search_table_err_num = 0;

	if (!table_start_addr || !search_point || !table_end_addr ||
		!search_content || (search_size <= 0)) {
		tvafe_pr_info("%s: point error, flag=%d\n", __func__, flag);
		vbi_search_table_err_num = 1;
		return NULL;
	}
	if ((table_end_addr <= table_start_addr) ||
		(search_point > table_end_addr)) {
		if (vbi_dbg_en & VBI_DBG_INFO) {
			tvafe_pr_info("%s: addr err: vcnt=%d, start=%p,search=%p,end=%p,table_size=%d, flag=%d\n",
				__func__, vcnt, table_start_addr,
				search_point, table_end_addr,
				table_size, flag);
		}
		vbi_search_table_err_num = 2;
		return NULL;
	}

	while (count++ < table_size) {
		if (((table_end_addr-p+1) < search_size) && (!cflag)) {
			cflag = 1;
			memcpy((table_end_addr+1),
				table_start_addr, search_size);
			if (vbi_dbg_en & VBI_DBG_ISR3) {
				tvafe_pr_info("%s: buffer reach max, vcnt=%d, start=%p,search=%p,end=%p,table_size=%d, copy_size=%d, flag=%d\n",
					__func__, vcnt, table_start_addr,
					search_point, table_end_addr,
					table_size, search_size, flag);
			}
		} else if (p > table_end_addr) {
			p = table_start_addr;
		}

		if (!memcmp(p, search_content, search_size))
			break;
		p++;
	}

	*len = count - 1 + search_size;
	if (count < table_size) {
		if (p + search_size - 1 > table_end_addr)
			return (p+search_size-1-table_end_addr-1+
					table_start_addr);
		else
			return p+search_size-1;
	} else {
		if (vbi_dbg_en & VBI_DBG_ISR2) {
			tvafe_pr_info("%s: cannot find: vcnt=%d, start=%p,search=%p,end=%p, flag=%d, count=%d, table_size=%d\n",
				__func__, vcnt, table_start_addr,
				search_point, table_end_addr, flag,
				count, table_size);
		}
		vbi_search_table_err_num = 3;
		return NULL;
	}
}

static unsigned char burst_data_vcnt[VBI_WRITE_BURST_BYTE] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};
static unsigned char temp_data_vcnt[VBI_WRITE_BURST_BYTE] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};
static unsigned char *search_table_for_vcnt(unsigned char *table_start_addr,
	unsigned char *search_point, unsigned char *table_end_addr,
	unsigned int vcnt_data, unsigned int *len)
{
	unsigned char vbi_head[3] = {0x00, 0xFF, 0xFF};
	unsigned char cflag = 0;
	unsigned int count = 0;
	unsigned int table_size = table_end_addr - table_start_addr + 1;
	unsigned char *p = search_point, *rp;
	unsigned int done = 0;
	unsigned int paddr;

	vbi_search_table_err_num = 0;

	if (!table_start_addr || !search_point || !table_end_addr) {
		tvafe_pr_info("%s: point error\n", __func__);
		vbi_search_table_err_num = 1;
		return NULL;
	}
	if ((table_end_addr <= table_start_addr) ||
		(search_point > table_end_addr)) {
		if (vbi_dbg_en & VBI_DBG_INFO) {
			tvafe_pr_info("%s: addr err: vcnt=%d, start=%p,search=%p,end=%p,table_size=%d\n",
				__func__, vcnt_data, table_start_addr,
				search_point, table_end_addr,
				table_size);
		}
		vbi_search_table_err_num = 2;
		return NULL;
	}

	burst_data_vcnt[0] = vcnt_data & 0xff;
	burst_data_vcnt[1] = (vcnt_data >> 8) & 0xff;
	temp_data_vcnt[0] = (vcnt_data + 1) & 0xff;
	temp_data_vcnt[1] = ((vcnt_data + 1) >> 8) & 0xff;
	paddr = R_VBI_APB_REG(ACD_REG_43);

	while (count++ < table_size) {
		if (((p + VBI_WRITE_BURST_BYTE - 1) > table_end_addr) &&
			(!cflag)) {
			cflag = 1;
			memcpy((table_end_addr+1),
				table_start_addr, (VBI_WRITE_BURST_BYTE * 2));
			if (vbi_dbg_en & VBI_DBG_INFO2) {
				tvafe_pr_info("%s: mem reach max, vcnt=%d, start=%p,search=%p,end=%p,table_size=%d, copy_size=%d\n",
					__func__, vcnt_data, table_start_addr,
					search_point, table_end_addr,
					table_size, VBI_WRITE_BURST_BYTE);
			}
		} else if (p > table_end_addr) {
			p = table_start_addr;
		}

		if (!memcmp(p, burst_data_vcnt, VBI_WRITE_BURST_BYTE)) {
			/* check vcnt is valid or not */
			rp = p + VBI_WRITE_BURST_BYTE;
			if (memcmp(rp, vbi_head, 3)) {
				if (memcmp(rp, temp_data_vcnt,
					VBI_WRITE_BURST_BYTE)) {
					if (vbi_dbg_en & VBI_DBG_INFO2) {
						tvafe_pr_info("%s: invalid vcnt=%d, start=%p,search=%p,end=%p, hw_paddr:0x%x, continue search\n",
							__func__, vcnt_data,
							table_start_addr,
							p, table_end_addr,
							paddr);
					}
					p++;
					continue;
				}
			}
			done = 1;
			break;
		}
		p++;
	}

	*len = count + VBI_WRITE_BURST_BYTE - 1;
	if (done == 0) {
		if (vbi_dbg_en & VBI_DBG_INFO) {
			tvafe_pr_info("%s: cannot find: vcnt=%d, start=%p,search=%p,end=%p, count=%d, table_size=%d\n",
				__func__, vcnt_data, table_start_addr,
				search_point, table_end_addr,
				count, table_size);
		}
		vbi_search_table_err_num = 3;
		return NULL;
	}

	rp = p + VBI_WRITE_BURST_BYTE - 1;
	if (rp > table_end_addr)
		return (rp - table_end_addr + table_start_addr - 1);
	return rp;
}

static void force_set_vcnt(unsigned char *rptr)
{
	unsigned char h_val, l_val;

	l_val = *rptr & 0xff;
	h_val = *(rptr+1) & 0xff;
	vcnt = (h_val << 8) | l_val;
	tvafe_pr_info("force set vcnt:0x%x\n", vcnt);
}

#define VBI_NEW_VCNT_SEARCH_MAX    8
static unsigned char temp_new_search[VBI_WRITE_BURST_BYTE - 2] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};
static void set_vbi_new_vcnt(struct vbi_dev_s *devp, unsigned char *rptr)
{
	unsigned char *ret_addr, *p;
	unsigned int len, temp;
	unsigned char vbi_head[3] = {0x0, 0xff, 0xff};
	unsigned int tval, search_max = 0;
	unsigned char h_val, l_val;

	temp = vcnt;

	if ((rptr < devp->pac_addr_start) ||
		(rptr > devp->pac_addr_end)) {
		tvafe_pr_info("addr error, vbi cannot search vcnt\n");
		return;
	}

	p = rptr;
vbi_new_vcnt_search:
	ret_addr = search_table(devp->pac_addr_start, p,
		devp->pac_addr_end, vbi_head, 3, &len, 0);
	if (!ret_addr) {
		force_set_vcnt(p);
		return;
	}

	if (ret_addr - devp->pac_addr_start >= 18) {
		if (memcmp((ret_addr - 16), temp_new_search,
			(VBI_WRITE_BURST_BYTE - 2))) {
			if (search_max++ > VBI_NEW_VCNT_SEARCH_MAX) {
				tvafe_pr_info("error: vbi cannot find vcnt, offset_addr:%d\n",
					(int)(ret_addr - devp->pac_addr_start));
				return;
			}
			p = ret_addr + 1;
			goto vbi_new_vcnt_search;
		}
		l_val = *(ret_addr - 18) & 0xff;
		h_val = *(ret_addr - 17) & 0xff;
	} else {
		tval = ret_addr - devp->pac_addr_start;
		if (memcmp(&devp->pac_addr_start[devp->mem_size - 16 + tval],
			temp_new_search, (VBI_WRITE_BURST_BYTE - 2))) {
			if (search_max++ > VBI_NEW_VCNT_SEARCH_MAX) {
				tvafe_pr_info("error: vbi cannot find vcnt, offset_addr:%d\n",
					(int)(ret_addr - devp->pac_addr_start));
				return;
			}
			p = ret_addr + 1;
			goto vbi_new_vcnt_search;
		}
		l_val = devp->pac_addr_start[devp->mem_size-18+tval] & 0xff;
		h_val = devp->pac_addr_start[devp->mem_size-17+tval] & 0xff;
	}

	vcnt = (h_val << 8) | l_val;
	if (vcnt > 0xffff)
		vcnt = temp;
	tvafe_pr_info("pre vcnt=0x%x, current vcnt=0x%x\n", temp, vcnt);
}

static unsigned char *search_vbi_new_addr(struct vbi_dev_s *devp)
{
	uint32_t paddr;
	unsigned char *vaddr;
	unsigned long temp;

	/*take out the real-time addr*/
	paddr = R_VBI_APB_REG(ACD_REG_43);
	/*this addr is 128bit format,need <<4*/
	paddr <<= 4;
	if (devp->mem_start > paddr)
		return NULL;
	vaddr = devp->pac_addr_start + paddr - devp->mem_start;
	temp = (unsigned long)vaddr;
	vaddr = (unsigned char *)((temp >> 4) << 4);
	if (vbi_dbg_en & VBI_DBG_INFO) {
		tvafe_pr_info("vbi search new addr:%p, addr_start:%p\n",
			vaddr, devp->pac_addr_start);
	}
	return vaddr;
}

static int init_vbi_data_sync(struct vbi_dev_s *devp)
{
	static unsigned char *addr;

	if (vbi_data_stable_flag)
		return 1;

	if (init_vbi_flag == 0)
		addr = search_vbi_new_addr(devp);

	if (init_vbi_flag == 4) {
		set_vbi_new_vcnt(devp, addr);
		vbi_data_stable_flag = 1;
		vbi_vcnt_search_retry = 0;
		tvafe_pr_info("init vbi data ok\n");
		return 1;
	}

	init_vbi_flag++;
	return 0;
}

static int check_if_sync_vbi_data(struct vbi_dev_s *devp, unsigned char *addr)
{
	static unsigned char *new_addr;

	if (!addr) {
		field_data_flag++;
		if (field_data_flag == 3) {
			new_addr = search_vbi_new_addr(devp);
		} else if (field_data_flag >= 6) {
			field_data_flag = 0;
			set_vbi_new_vcnt(devp, new_addr);
		}
		if (vbi_dbg_en & VBI_DBG_INFO)
			tvafe_pr_info("not find vbi data.\n");
		return -1;
	}
	if (addr > devp->pac_addr_end) {
		if (vbi_dbg_en & VBI_DBG_INFO)
			tvafe_pr_info("vbi ret_addr error.\n");
		return -1;
	}

	field_data_flag = 0;
	return 0;
}

static int check_vbi_data_valid(struct vbi_data_s *vbi_data)
{
	unsigned int data_byte = 0;

	if (vbi_data_tt_check == 0)
		return 0;
	if (vbi_data->vbi_type != 3) /* only check tt */
		return 0;

	switch (vbi_data->tt_sys) {
	case VBI_SYS_TT_625A:
		data_byte = VBI_PCNT_TT_625A;
		break;
	case VBI_SYS_TT_625B:
		data_byte = VBI_PCNT_TT_625B;
		break;
	case VBI_SYS_TT_625C:
		data_byte = VBI_PCNT_TT_625C;
		break;
	case VBI_SYS_TT_625D:
		data_byte = VBI_PCNT_TT_625D;
		break;
	case VBI_SYS_TT_525B:
		data_byte = VBI_PCNT_TT_525B;
		break;
	case VBI_SYS_TT_525C:
		data_byte = VBI_PCNT_TT_525C;
		break;
	case VBI_SYS_TT_525D:
		data_byte = VBI_PCNT_TT_525D;
		break;
	default:
		break;
	}

	if (vbi_data->nbytes != data_byte) {
		if (vbi_dbg_en & VBI_DBG_INFO2) {
			tvafe_pr_info("%s: data check error: tt_sys=%d, data_byte=%d\n",
				__func__, vbi_data->tt_sys, vbi_data->nbytes);
		}
		return -1;
	}

	return 0;
}

static unsigned char vbi_tt_test_p201_head[23][2] = {
	{0x8c, 0x15}, /* line 1 */
	{0x49, 0x02}, /* line 2 */
	{0x8c, 0x02}, /* line 3 */
	{0x49, 0x49}, /* line 4 */
	{0x8c, 0x49}, /* line 5 */
	{0x49, 0x5e}, /* line 6 */
	{0x8c, 0x5e}, /* line 7 */
	{0x49, 0x64}, /* line 8 */
	{0x8c, 0x64}, /* line 9 */
	{0x49, 0x73}, /* line 10 */
	{0x8c, 0x73}, /* line 11 */
	{0x49, 0x38}, /* line 12 */
	{0x8c, 0x38}, /* line 13 */
	{0x49, 0x2f}, /* line 14 */
	{0x8c, 0x2f}, /* line 15 */
	{0x49, 0xd0}, /* line 16 */
	{0x8c, 0xd0}, /* line 17 */
	{0x49, 0xc7}, /* line 18 */
	{0x8c, 0xc7}, /* line 19 */
	{0x49, 0x8c}, /* line 20 */
	{0x8c, 0x8c}, /* line 21 */
	{0x49, 0x9b}, /* line 22 */
	{0x8c, 0x9b}, /* line 23 */
};
static void vbi_tt_raw_data_test(struct vbi_data_s *vbi_data)
{
	int i, j, n;

	if (vbi_data->nbytes != 42)
		return;
	for (n = 0; n < 23; n++) {
		if ((vbi_data->b[0] == vbi_tt_test_p201_head[n][0]) &&
		(vbi_data->b[1] == vbi_tt_test_p201_head[n][1]) &&
		(vbi_data->b[2] == 0x7f))
			break;
	}
	if (n >= 23)
		return;
	j = 0;
	for (i = 0; i < 40; i++) {
		if (vbi_data->b[i+2] != 0x7f)
			j++;
	}
	if (j > 0) {
		pr_info("[vbi..]: vcnt:%d, page201 line:%d, error cnt:%d\n",
			vcnt, (n + 1), j);
	}
	if (vbi_pr_isr_buf) {
		if (vbi_dbg_en & VBI_DBG_TEST2) {
			j = sprintf(vbi_pr_isr_buf,
				"[vbi..]: vcnt:%d, page201 line:%d, data:",
				vcnt, (n + 1));
			for (i = 0; i < 42 ; i++) {
				j += sprintf(vbi_pr_isr_buf + j,
					"%02x ", vbi_data->b[i]);
			}
			pr_info("%s\n", vbi_pr_isr_buf);
		}
	}
}

static void vbi_slicer_work(struct work_struct *p_work)
{
	struct vbi_dev_s *devp = vbi_dev_local;
	struct vbi_data_s vbi_data;
	unsigned char rbyte = 0;
	unsigned int i = 0, j;
	unsigned short pre_val = 0;
	unsigned char *local_rptr = NULL;
	unsigned char vbi_head[3] = {0x00, 0xFF, 0xFF};
	unsigned char *rptr, *ret_addr, *vbi_addr;
	unsigned int len = 0, chlen = 0, vbihlen, datalen;
	int ret, data_save_flag = 0;
	unsigned char *tmp;
	unsigned char *vbi_addr_bak = NULL;
	unsigned int vbihlen_bak = 0, chlen_bak = 0;

	if (!devp)
		return;
	if (devp->vbi_start == false)
		return;
	if (tvafe_clk_status == false) {
		vbi_ringbuffer_flush(&devp->slicer->buffer);
		return;
	}

	if (devp->slicer->busy) {
		if (vbi_dbg_en & VBI_DBG_INFO)
			tvafe_pr_info("%s busy, exit\n", __func__);
		return;
	}
	if (vbi_slicer_bypass) {
		if (vbi_dbg_en & VBI_DBG_ISR2)
			tvafe_pr_info("%s bypass, exit\n", __func__);
		return;
	}

	mutex_lock(&devp->slicer->task_mutex);
	devp->slicer->busy = 1;
	if (devp->slicer->slicer_cnt++ >= 0xffffffff)
		devp->slicer->slicer_cnt = 0;

	tmp = (unsigned char *)&vbi_data;

	ret = init_vbi_data_sync(devp);
	if (!ret) {
		devp->slicer->busy = 0;
		mutex_unlock(&devp->slicer->task_mutex);
		return;
	}

	if (++vbi_read_wakeup_cnt % vbi_read_wakeup_interval == 0)
		vbi_read_wakeup_cnt = 0;

	if (vbi_dbg_en & VBI_DBG_ISR3)
		tvafe_pr_info("pac_addr:%p\n", devp->pac_addr);
	if (devp->pac_addr > devp->pac_addr_end)
		devp->pac_addr = devp->pac_addr_start;

	rptr = devp->pac_addr;  /* backup package data pointer */

	ret_addr = search_table_for_vcnt(devp->pac_addr_start, rptr,
		devp->pac_addr_end, vcnt, &len);
	if ((vbi_dbg_en & VBI_DBG_INFO) && (vbi_search_table_err_num == 2)) {
		tvafe_pr_info("pac_start:%p, pac_addr:%p, offset:%d, ret_addr:%p, vcnt:%d, len:%d\n",
			devp->pac_addr_start, devp->pac_addr,
			(int)(devp->pac_addr - devp->pac_addr_start),
			ret_addr, vcnt, len);
	}
	if (vbi_search_table_err_num == 3) {
		if (vbi_vcnt_search_retry++ < VBI_VCNT_SEARCH_CNT_MAX)
			goto vbi_slicer_work_exit;
	}
	if ((vbi_vcnt_search_retry > 0) &&
		(vbi_vcnt_search_retry < VBI_VCNT_SEARCH_CNT_MAX))
		tvafe_pr_info("%s: find vcnt:%d, retry_cnt:%d\n",
			__func__, vcnt, vbi_vcnt_search_retry);
	vbi_vcnt_search_retry = 0;

	ret = check_if_sync_vbi_data(devp, ret_addr);
	if (ret) {
		if (vbi_dbg_en & VBI_DBG_INFO) {
			tvafe_pr_info("%s: check_if_sync_vbi_data failed, vcnt:%d\n",
				__func__, vcnt);
		}
		goto vbi_slicer_work_next;
	}

	if ((len <= VBI_WRITE_BURST_BYTE) ||
		(len > VBI_BUFF3_EA - VBI_BUFF2_EA)) {
		if (vbi_dbg_en & VBI_DBG_INFO2) {
			tvafe_pr_info("%s: no vbi data, len %d, vcnt:%d\n",
				__func__, len, vcnt);
		}
		goto vbi_slicer_work_next;
	}

	/* temp_addr_start = pac_addr_start + VBI_BUFF3_EA */
	ret = copy_vbi_to(devp->pac_addr_start, devp->pac_addr_end,
		devp->temp_addr_start, rptr,
		(len - VBI_WRITE_BURST_BYTE));
	if (ret) {
		if (vbi_dbg_en & VBI_DBG_INFO) {
			tvafe_pr_info("%s: copy_vbi_to failed, vcnt:%d\n",
				__func__, vcnt);
		}
		goto vbi_slicer_work_next;
	}

	local_rptr = devp->temp_addr_start;
	datalen = sizeof(struct vbi_data_s);
	while (chlen < len) {
		if (!devp->vbi_start || !tvafe_clk_status) {
			if (vbi_dbg_en & VBI_DBG_INFO)
				tvafe_pr_info("%s: vbi stopped\n", __func__);
			goto vbi_slicer_work_exit;
		}
		if ((local_rptr > devp->temp_addr_end)
			|| (local_rptr < devp->temp_addr_start)) {
			if (vbi_dbg_en & VBI_DBG_INFO) {
				tvafe_pr_info("%s: local_rptr:%p invalid, vcnt:%d\n",
					__func__, local_rptr, vcnt);
			}
			goto vbi_slicer_work_next;
		}

		vbi_addr = search_table(local_rptr, local_rptr,
		(devp->temp_addr_start + len - VBI_WRITE_BURST_BYTE - 1),
			vbi_head, 3, &vbihlen, 2);
		if (!vbi_addr) {
			if ((vbi_search_table_err_num == 2) &&
				(vbi_dbg_en & VBI_DBG_INFO)) {
				tvafe_pr_info("%s: vbi data search table invalid, vcnt:%d, chlen:%d, len:%d, data_save_flag:%d, vbi_search_table_err_num=%d\n",
					__func__, vcnt, chlen, len,
					data_save_flag,
					vbi_search_table_err_num);
				tvafe_pr_info("%s: chlen_bak:%d, vbihlen_bak:%d, vbi_addr_bak:%p, mem_start=%p, mem_end=%p\n",
					__func__, chlen_bak, vbihlen_bak,
					vbi_addr_bak,
					devp->pac_addr_start,
					devp->pac_addr_end);
				for (i = 0; i < (len-VBI_WRITE_BURST_BYTE);
					i++) {
					pr_info("***%d: 0x%p=0x%02x\n",
						i, (devp->temp_addr_start + i),
						*(devp->temp_addr_start + i));
				}
				for (i = 0; i < (len+3); i++) {
					pr_info("**%d: 0x%p=0x%02x\n",
						i, (devp->pac_addr + i),
						*(devp->pac_addr + i));
				}
			}
			goto vbi_slicer_work_next;
		}
		chlen += (vbihlen + 3);
		local_rptr = vbi_addr + 1;
		vbi_addr_bak = vbi_addr;
		vbihlen_bak = vbihlen;

		if ((local_rptr > devp->temp_addr_end)
			|| (local_rptr < devp->temp_addr_start)) {
			if (vbi_dbg_en & VBI_DBG_INFO) {
				tvafe_pr_info("%s: local_rptr:%p invalid, vcnt:%d\n",
					__func__, local_rptr, vcnt);
			}
			goto vbi_slicer_work_next;
		}

		/* vbi_type & field_id */
		vbi_get_byte(local_rptr, rbyte);
		chlen++;
		vbi_data.vbi_type = (rbyte>>1) & 0x7;
		vbi_data.field_id = rbyte & 1;
		#if defined(VBI_TT_SUPPORT)
		vbi_data.tt_sys = rbyte >> 5;
		#endif
		if (vbi_data.vbi_type > MAX_PACKET_TYPE) {
			if (vbi_dbg_en & VBI_DBG_INFO) {
				tvafe_pr_info("[vbi..]: vcnt:%d, chlen:%d, unsupport vbi_type_id:%d\n",
					vcnt, chlen, vbi_data.vbi_type);
			}
			continue;
		}
		/* byte counter */
		vbi_get_byte(local_rptr, rbyte);
		chlen++;
		vbi_data.nbytes = rbyte;
		/* line number */
		vbi_get_byte(local_rptr, rbyte);
		chlen++;
		pre_val = (u16)rbyte;
		vbi_get_byte(local_rptr, rbyte);
		chlen++;
		pre_val |= ((u16)rbyte & 0x3) << 8;
		vbi_data.line_num = pre_val;
		if (vbi_dbg_en & VBI_DBG_ISR4)
			vbi_data.line_num = vcnt;
		/* data */
		chlen += vbi_data.nbytes;
		if (vbi_data.nbytes > VBI_DATA_BYTE_MAX) {
			if (vbi_dbg_en & VBI_DBG_INFO) {
				tvafe_pr_info("[vbi..]: vcnt:%d, chlen:%d, unsupport vbi_data_byte:%d\n",
					vcnt, chlen, vbi_data.nbytes);
			}
			continue;
		}
		if (check_vbi_data_valid(&vbi_data)) {
			local_rptr += vbi_data.nbytes;
			if (vbi_dbg_en & VBI_DBG_INFO) {
				tvafe_pr_info("[vbi..]: vcnt:%d, chlen:%d, invalid vbi_data type:%d, tt_sys:%d, byte:%d\n",
					vcnt, chlen, vbi_data.vbi_type,
					vbi_data.tt_sys, vbi_data.nbytes);
			}
			continue;
		}
		memcpy(&(vbi_data.b[0]), local_rptr, vbi_data.nbytes);
		local_rptr += vbi_data.nbytes;
		/* capture data to vbi buffer */
		vbi_buffer_write(&devp->slicer->buffer, &vbi_data, datalen);
		data_save_flag = 1;
		/* go to search next package */
		if (vbi_dbg_en & VBI_DBG_TEST)
			vbi_tt_raw_data_test(&vbi_data);
		if (vbi_dbg_en & VBI_DBG_ISR4) {
			if (vbi_pr_isr_buf) {
				j = sprintf(vbi_pr_isr_buf,
					"[vbi..]: chlen:%d, vcnt:%d, field_id:%d, line_num:%4d, data_cnt:%d:",
					chlen, vcnt,
					vbi_data.field_id,
					vbi_data.line_num,
					vbi_data.nbytes);
				for (i = 0; i < vbi_data.nbytes ; i++) {
					if (i%16 == 0) {
						j += sprintf(vbi_pr_isr_buf + j,
							"\n    ");
					}
					j += sprintf(vbi_pr_isr_buf + j,
						"0x%02x ", vbi_data.b[i]);
				}
				pr_info("%s\n", vbi_pr_isr_buf);

				tvafe_pr_info("%s: vbi_dbg_isr_cnt %d\n",
					__func__, vbi_dbg_isr_cnt);
				if (vbi_dbg_isr_cnt++ >= vbi_dbg_print_cnt) {
					vbi_dbg_en &= ~(VBI_DBG_ISR4);
					vbi_dbg_isr_cnt = 0;
					kfree(vbi_pr_isr_buf);
					vbi_pr_isr_buf = NULL;
				}
			}
		}

		chlen_bak = chlen;
	}

vbi_slicer_work_next:
	if (data_save_flag) {
		if (vbi_dbg_en & VBI_DBG_ISR2)
			tvafe_pr_info("[vbi..]: buffer pread:%d, pwrite=%d, vbi_read_wakeup_cnt:%d\n",
				devp->slicer->buffer.pread,
				devp->slicer->buffer.pwrite,
				vbi_read_wakeup_cnt);
		if ((devp->slicer->buffer.pread !=
			devp->slicer->buffer.pwrite) &&
			(vbi_read_wakeup_cnt == 0)) {
			wake_up(&devp->slicer->buffer.queue);
			if (vbi_dbg_en & VBI_DBG_ISR2)
				tvafe_pr_info("[vbi..]: wake_up buffer polling\n");
		}
	}

	if (vcnt >= 0xffff)
		vcnt = 1;
	else
		vcnt++;

	if (ret_addr) {
		if (ret_addr + 1 > devp->pac_addr_end)
			devp->pac_addr = devp->pac_addr_start;
		else
			devp->pac_addr = ret_addr + 1;
	}

vbi_slicer_work_exit:
	devp->slicer->busy = 0;
	mutex_unlock(&devp->slicer->task_mutex);

	return;
}

static inline void vbi_slicer_state_set(struct vbi_dev_s *dev,
						int state)
{
	spin_lock_irq(&dev->lock);
	dev->slicer->state = state;
	spin_unlock_irq(&dev->lock);
}

static inline int vbi_slicer_reset(struct vbi_dev_s *dev)
{
	if (dev->slicer->state < VBI_STATE_SET)
		return 0;

	dev->slicer->type = VBI_TYPE_NULL;
	vbi_slicer_state_set(dev, VBI_STATE_ALLOCATED);
	return 0;
}

static int vbi_slicer_stop(struct vbi_slicer_s *vbi_slicer)
{
	if (vbi_slicer->state < VBI_STATE_GO)
		return 0;
	vbi_slicer->state = VBI_STATE_DONE;

	vbi_ringbuffer_flush(&vbi_slicer->buffer);

	return 0;
}

static int vbi_slicer_free(struct vbi_dev_s *vbi_dev,
		  struct vbi_slicer_s *vbi_slicer)
{
	mutex_lock(&vbi_dev->mutex);
	mutex_lock(&vbi_slicer->mutex);

	vbi_slicer_stop(vbi_slicer);
	vbi_slicer_reset(vbi_dev);

	if (vbi_slicer->buffer.data) {
		void *mem = vbi_slicer->buffer.data;

		spin_lock_irq(&vbi_dev->lock);
		vbi_slicer->buffer.data = NULL;
		spin_unlock_irq(&vbi_dev->lock);
		vfree(mem);
	}

	vbi_slicer_state_set(vbi_dev, VBI_STATE_FREE);
	wake_up(&vbi_slicer->buffer.queue);
	mutex_unlock(&vbi_slicer->mutex);
	mutex_unlock(&vbi_dev->mutex);

	return 0;
}

static void vbi_ringbuffer_init(struct vbi_ringbuffer_s *rbuf,
			void *data, size_t len)
{
	if (data == NULL)
		rbuf->data = vmalloc(len*sizeof(struct vbi_data_s));
	else
		rbuf->data = data;
	rbuf->size = len*sizeof(struct vbi_data_s);
	rbuf->data_num = len;

	vbi_ringbuffer_flush(rbuf);
}

static int vbi_set_buffer_size(struct vbi_dev_s *dev,
		      unsigned int size)
{
	struct vbi_slicer_s *vbi_slicer = dev->slicer;
	struct vbi_ringbuffer_s *buf = &vbi_slicer->buffer;
	void *newmem;
	void *oldmem;

	if (buf->size == size) {
		tvafe_pr_info("%s: buf->size == size\n", __func__);
		return 0;
	}
	if (!size) {
		tvafe_pr_info("%s: buffer size is 0!!!\n", __func__);
		return -EINVAL;
	}
	if (size > VBI_DEFAULT_BUFFER_SIZE) {
		tvafe_pr_info("%s: buffer size %d can't over %d\n",
			      __func__, size, VBI_DEFAULT_BUFFER_SIZE);
		return -EINVAL;
	}
	if (vbi_slicer->state >= VBI_STATE_GO) {
		tvafe_pr_info("%s: vbi_slicer busy!!!\n", __func__);
		return -EBUSY;
	}

	newmem = vmalloc(size);
	if (!newmem)
		return -ENOMEM;

	oldmem = buf->data;

	spin_lock_irq(&dev->lock);
	buf->data = newmem;
	buf->size = size;

	/* reset and not flush in case the buffer shrinks */
	vbi_ringbuffer_flush(buf);
	spin_unlock_irq(&dev->lock);

	vfree(oldmem);

	return 0;
}

static int vbi_slicer_start(struct vbi_dev_s *dev)
{
	struct vbi_slicer_s *vbi_slicer = dev->slicer;
	void *mem;

	if (vbi_slicer->state < VBI_STATE_SET)
		return -EINVAL;

	if (vbi_slicer->state >= VBI_STATE_GO)
		vbi_slicer_stop(vbi_slicer);

	if (!vbi_slicer->buffer.data) {
		mem = vmalloc(vbi_slicer->buffer.size);
		if (!mem) {
			tvafe_pr_info("[vbi..] %s: get memory error!!!\n",
				__func__);
			return -ENOMEM;
		}
		spin_lock_irq(&dev->lock);
		vbi_slicer->buffer.data = mem;
		spin_unlock_irq(&dev->lock);
	}

	vbi_ringbuffer_flush(&vbi_slicer->buffer);

	vbi_slicer_state_set(dev, VBI_STATE_GO);

	tvafe_pr_info("[vbi..] %s: vbi data size: %d\n",
		__func__, (int)sizeof(struct vbi_data_s));

	return 0;
}

static int vbi_slicer_set(struct vbi_dev_s *vbi_dev,
		 struct vbi_slicer_s *vbi_slicer)
{
	vbi_slicer_stop(vbi_slicer);

	vbi_slicer_state_set(vbi_dev, VBI_STATE_SET);

	vbi_slicer_type_set(vbi_dev);

	return 0;/* vbi_slicer_start(vbi_dev); */
}

static ssize_t vbi_ringbuffer_avail(struct vbi_ringbuffer_s *rbuf)
{
	ssize_t avail;

	avail = (rbuf->pwrite >= rbuf->pread) ?
		(rbuf->pwrite - rbuf->pread) :
		(rbuf->size - (rbuf->pread - rbuf->pwrite));

	return avail;
}

static int vbi_ringbuffer_empty(struct vbi_ringbuffer_s *rbuf)
{
	return (int)(rbuf->pread == rbuf->pwrite);
}

static void vbi_user_buffer_dump(u8 __user *buf, size_t len)
{
	unsigned char *data;
	unsigned int i, j, chlen, datalen;
	struct vbi_data_s vbi_data;

	if (vbi_pr_read_buf) {
		data = kcalloc(len, sizeof(char), GFP_KERNEL);
		if (data == NULL) {
			tvafe_pr_info("%s: data error\n", __func__);
			return;
		}
		if (copy_from_user(data, buf, len)) {
			tvafe_pr_info("%s: copy_from_user error\n", __func__);
			kfree(data);
			return;
		}

		datalen = sizeof(struct vbi_data_s);
		chlen = 0;
		while (chlen < len) {
			if ((chlen + datalen) > len) {
				tvafe_pr_info("%s: chlen:%d,datalen:%d,len:%d invalid\n",
					__func__, chlen, datalen, (int)len);
				break;
			}
			memcpy(&vbi_data, (data + chlen), datalen);
			chlen += datalen;
			if (vbi_data.vbi_type > MAX_PACKET_TYPE) {
				tvafe_pr_info("[user..]: chlen:%d, line_num:%4d, unsupport vbi_type_id:%d\n",
					chlen, vbi_data.line_num,
					vbi_data.vbi_type);
				continue;
			}
			if (vbi_data.nbytes > VBI_DATA_BYTE_MAX) {
				tvafe_pr_info("[user..]: chlen:%d, line_num:%4d, unsupport vbi_data_byte:%d\n",
					chlen, vbi_data.line_num,
					vbi_data.nbytes);
				continue;
			}

			j = sprintf(vbi_pr_read_buf,
				"[user..]: chlen:%d, field_id:%d, line_num:%4d, data_cnt:%d:",
				chlen,
				vbi_data.field_id,
				vbi_data.line_num,
				vbi_data.nbytes);
			for (i = 0; i < vbi_data.nbytes ; i++) {
				if (i%16 == 0) {
					j += sprintf(vbi_pr_read_buf + j,
						"\n    ");
				}
				j += sprintf(vbi_pr_read_buf + j, "0x%02x ",
					vbi_data.b[i]);
			}
			pr_info("%s\n", vbi_pr_read_buf);
		}
		kfree(data);
	}
}

static ssize_t vbi_ringbuffer_read_user(struct vbi_ringbuffer_s *rbuf,
			u8 __user *buf, size_t len)
{
	size_t todo = len;
	size_t split;

	split = (rbuf->pread + len > rbuf->size) ? rbuf->size - rbuf->pread : 0;
	if (split > 0) {
		if (vbi_dbg_en & VBI_DBG_READ2) {
			tvafe_pr_info("%s: user buffer0: %p\n",
				__func__, buf);
		}
		if (copy_to_user(buf, (char *)rbuf->data+rbuf->pread, split))
			return -EFAULT;
		buf += split;
		todo -= split;
		rbuf->pread = 0;
		if (vbi_dbg_en & VBI_DBG_READ2) {
			tvafe_pr_info("%s: ringbuffer read max, split size=%d, todo size=%d\n",
				__func__, (int)split, (int)todo);
		}
		if (vbi_dbg_en & VBI_DBG_READ2) {
			tvafe_pr_info("%s: user buffer: %p\n",
				__func__, buf);
		}
	}
	if (copy_to_user(buf, (char *)rbuf->data+rbuf->pread, todo))
		return -EFAULT;
	rbuf->pread = (rbuf->pread + todo) % rbuf->size;
	if (vbi_dbg_en & VBI_DBG_READ4) {
		vbi_user_buffer_dump(buf, len);
		tvafe_pr_info("%s: vbi_dbg_read_cnt %d, len %d\n",
			__func__, vbi_dbg_read_cnt, (int)len);
		if (vbi_dbg_read_cnt++ >= vbi_dbg_print_cnt) {
			vbi_dbg_en &= ~(VBI_DBG_READ4);
			vbi_dbg_read_cnt = 0;
			kfree(vbi_pr_read_buf);
			vbi_pr_read_buf = NULL;
		}
	}

	return len;
}

static ssize_t vbi_buffer_read(struct vbi_ringbuffer_s *src,
		      int non_blocking, char __user *buf,
		      size_t count, loff_t *ppos)
{
	ssize_t avail;
	ssize_t ret = 0;
	ssize_t timeout = 0;

	if ((src == NULL) || (src->data == NULL)) {
		tvafe_pr_info("%s: data null\n", __func__);
		return 0;
	}

	if (src->error) {
		ret = src->error;
		vbi_ringbuffer_flush(src);
		tvafe_pr_info("%s: data error\n", __func__);
		return ret;
	}

	while (vbi_ringbuffer_empty(src)) {
		if ((non_blocking || vbi_nonblock_en) ||
			(tvafe_clk_status == false) || (timeout++ > 50)) {
			ret = -EWOULDBLOCK;
			tvafe_pr_info("%s:nonblock|closed|timeout return.\n",
				__func__);
			return ret;
		}
		msleep(20);
	}

	if (tvafe_clk_status == false) {
		ret = -EWOULDBLOCK;
		vbi_ringbuffer_flush(src);
		if (vbi_dbg_en & VBI_DBG_READ)
			tvafe_pr_info("[vbi..] %s: tvafe is closed.pread = %d;pwrite = %d\n",
				__func__, src->pread, src->pwrite);
		return ret;
	}
	if (src->error) {
		ret = src->error;
		vbi_ringbuffer_flush(src);
		if (vbi_dbg_en & VBI_DBG_READ)
			tvafe_pr_info("%s: read buffer error\n", __func__);
		return ret;
	}
	if (vbi_dbg_en & VBI_DBG_READ3)
		tvafe_pr_info("%s:src->pread = %d;src->pwrite = %d\n",
		__func__, src->pread, src->pwrite);

	avail = vbi_ringbuffer_avail(src);
	if (avail > count)
		avail = count;

	ret = vbi_ringbuffer_read_user(src, buf, avail);
	if (ret < 0) {
		tvafe_pr_info("%s: vbi_ringbuffer_read_user error\n",
			__func__);
	}
	if (vbi_dbg_en & VBI_DBG_READ3)
		tvafe_pr_info("%s: counter:%Zx, return:%Zx\n",
		__func__, count, ret);
	return ret;
}

static ssize_t vbi_read(struct file *file, char __user *buf, size_t count,
	loff_t *ppos)
{
	struct vbi_dev_s *vbi_dev = file->private_data;
	struct vbi_slicer_s *vbi_slicer = vbi_dev->slicer;
	int ret = 0;

	if (mutex_lock_interruptible(&vbi_slicer->mutex)) {
		tvafe_pr_info("%s: slicer mutex error\n", __func__);
		return -ERESTARTSYS;
	}

	ret = vbi_buffer_read(&vbi_slicer->buffer,
		file->f_flags & O_NONBLOCK, buf, count, ppos);

	mutex_unlock(&vbi_slicer->mutex);

	return ret;
}

static int vbi_open(struct inode *inode, struct file *file)
{
	struct vbi_dev_s *vbi_dev;
	int ret = 0;

	tvafe_pr_info("%s: open start.\n", __func__);
	vbi_dev = container_of(inode->i_cdev, struct vbi_dev_s, cdev);
	file->private_data = vbi_dev;
	if (mutex_lock_interruptible(&vbi_dev->mutex)) {
		tvafe_pr_info("%s: dev mutex_lock_interruptible error\n",
			__func__);
		return -ERESTARTSYS;
	}
	vbi_ringbuffer_init(&vbi_dev->slicer->buffer, NULL,
		VBI_DEFAULT_BUFFER_PACKAGE_NUM);
	vbi_dev->slicer->type = VBI_TYPE_NULL;
	vbi_slicer_state_set(vbi_dev, VBI_STATE_ALLOCATED);
	/*disable data capture function*/
	vbi_dev->slicer_enable = false;
	vbi_dev->vbi_start = false;
	/* request irq */
	ret = request_irq(vbi_dev->vs_irq, vbi_isr, IRQF_SHARED,
		vbi_dev->irq_name, (void *)vbi_dev);
	vbi_dev->irq_free_status = 1;
	if (ret < 0)
		tvafe_pr_err("%s: request_irq fail\n", __func__);

	mutex_unlock(&vbi_dev->mutex);
	tvafe_pr_info("%s: open device ok.\n", __func__);
	return 0;
}


static int vbi_release(struct inode *inode, struct file *file)
{
	struct vbi_dev_s *vbi_dev = file->private_data;
	struct vbi_slicer_s *vbi_slicer = vbi_dev->slicer;
	int ret = 0;

	vbi_dev->slicer_enable = false;
	vbi_dev->vbi_start = false;  /*disable data capture function*/
	ret = vbi_slicer_free(vbi_dev, vbi_slicer);
	/* free irq */
	if (vbi_dev->irq_free_status == 1)
		free_irq(vbi_dev->vs_irq, (void *)vbi_dev);
	vbi_dev->irq_free_status = 0;
	vcnt = 1;
	if (tvafe_clk_status) {
		/* vbi reset release, vbi agent enable */
		/*W_VBI_APB_REG(ACD_REG_22, 0x06080000);*/
		W_VBI_APB_REG(CVD2_VBI_FRAME_CODE_CTL, 0x10);
	}
	tvafe_pr_info("[vbi..]%s: device release OK.\n", __func__);
	return ret;
}


static long vbi_ioctl(struct file *file,
		unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	void __user *argp = (void __user *)arg;
	unsigned int buffer_size_t;

	struct vbi_dev_s *vbi_dev = file->private_data;
	struct vbi_slicer_s *vbi_slicer = vbi_dev->slicer;

	if (mutex_lock_interruptible(&vbi_dev->mutex))
		return -ERESTARTSYS;

	switch (cmd) {
	case VBI_IOC_START:
		if (mutex_lock_interruptible(&vbi_slicer->mutex)) {
			mutex_unlock(&vbi_dev->mutex);
			tvafe_pr_err("%s: slicer mutex error\n", __func__);
			return -ERESTARTSYS;
		}
		if (tvafe_clk_status)
			vbi_hw_init(vbi_dev);
		else {
			tvafe_pr_err("tvafe not opend.ioctl start err\n");
			ret = -EINVAL;
			mutex_unlock(&vbi_slicer->mutex);
			break;
		}
		if (vbi_slicer->state < VBI_STATE_SET)
			ret = -EINVAL;
		else
			ret = vbi_slicer_start(vbi_dev);
		if (ret) {
			pr_err("[vbi..] tvafe not opend.ioctl start err\n");
			mutex_unlock(&vbi_slicer->mutex);
			break;
		}
		/* enable data capture function */
		vbi_state_reset(vbi_dev);

		vbi_dev->vbi_start = true;
		vbi_dev->slicer_enable = true;

		mutex_unlock(&vbi_slicer->mutex);
		mdelay(VBI_START_DELAY);
		tvafe_pr_info("[vbi..] %s: start slicer state:%d\n",
			__func__, vbi_slicer->state);
		break;

	case VBI_IOC_STOP:
		tvafe_pr_info("[vbi..] %s: stop vbi function\n", __func__);
		if (mutex_lock_interruptible(&vbi_slicer->mutex)) {
			mutex_unlock(&vbi_dev->mutex);
			tvafe_pr_err("%s: slicer mutex error\n", __func__);
			return -ERESTARTSYS;
		}
		/* disable data capture function */
		vbi_dev->slicer_enable = false;
		vbi_dev->vbi_start = false;
		vbi_data_stable_flag = 0;
		ret = vbi_slicer_stop(vbi_slicer);
		if (tvafe_clk_status) {
			/* manuel reset vbi */
			/*W_VBI_APB_REG(ACD_REG_22, 0x82080000);*/
			/* vbi reset release, vbi agent enable*/
				/*W_VBI_APB_REG(ACD_REG_22, 0x06080000);*/
			/*WAPB_REG(CVD2_VBI_CC_START, 0x00000054);*/
			W_VBI_APB_REG(CVD2_VBI_FRAME_CODE_CTL, 0x10);
		}
		mutex_unlock(&vbi_slicer->mutex);
		tvafe_pr_info("%s: stop slicer state:%d\n",
			__func__, vbi_slicer->state);
		break;

	case VBI_IOC_SET_TYPE:
		if (!tvafe_clk_status) {
			mutex_unlock(&vbi_dev->mutex);
			tvafe_pr_info("[vbi..] %s: afe not open,SET_TYPE return\n",
				__func__);
			return -ERESTARTSYS;
		}
		tvafe_pr_info("[vbi..] %s: VBI_IOC_SET_TYPE\n", __func__);
		if (mutex_lock_interruptible(&vbi_slicer->mutex)) {
			mutex_unlock(&vbi_dev->mutex);
			tvafe_pr_info("%s: slicer mutex error\n", __func__);
			return -ERESTARTSYS;
		}
		if (copy_from_user(&vbi_slicer->type, argp, sizeof(int))) {
			tvafe_pr_info("[vbi..] %s: copy err\n", __func__);
			ret = -EFAULT;
			mutex_unlock(&vbi_slicer->mutex);
			break;
		}
		if (tvafe_clk_status) {
			ret = vbi_slicer_set(vbi_dev, vbi_slicer);
		} else {
			ret = -EFAULT;
			tvafe_pr_err("[vbi..] %s: tvafeclose, no vbi_slicer_set\n",
				__func__);
		}
		mutex_unlock(&vbi_slicer->mutex);
		tvafe_pr_info("%s: set slicer type to 0x%x, state:%d\n",
			__func__, vbi_slicer->type, vbi_slicer->state);
		break;

	case VBI_IOC_S_BUF_SIZE:
		if (mutex_lock_interruptible(&vbi_slicer->mutex)) {
			mutex_unlock(&vbi_dev->mutex);
			tvafe_pr_info("%s: slicer mutex error\n", __func__);
			return -ERESTARTSYS;
		}
		if (copy_from_user(&buffer_size_t, argp, sizeof(int))) {
			ret = -EFAULT;
			mutex_unlock(&vbi_slicer->mutex);
			break;
		}
		if ((!buffer_size_t) ||
		    (buffer_size_t > VBI_DEFAULT_BUFFER_SIZE)) {
			mutex_unlock(&vbi_slicer->mutex);
			tvafe_pr_info("%s: invalid buf size %d\n",
				      __func__, buffer_size_t);
			break;
		}
		ret = vbi_set_buffer_size(vbi_dev, buffer_size_t);
		mutex_unlock(&vbi_slicer->mutex);
		tvafe_pr_info("%s: set buf size to %d, state:%d\n",
			__func__, vbi_slicer->buffer.size, vbi_slicer->state);
		break;

	default:
		ret = -EINVAL;
		break;
	}
	mutex_unlock(&vbi_dev->mutex);

	return ret;
}

#ifdef CONFIG_COMPAT
static long vbi_compat_ioctl(struct file *file, unsigned int cmd,
	unsigned long arg)
{
	unsigned long ret;

	arg = (unsigned long)compat_ptr(arg);
	ret = vbi_ioctl(file, cmd, arg);
	return ret;
}
#endif

static unsigned int vbi_poll(struct file *file, poll_table *wait)
{
	struct vbi_dev_s *devp = file->private_data;
	struct vbi_slicer_s *vbi_slicer = devp->slicer;
	unsigned int mask = 0;

	if (!vbi_slicer) {
		tvafe_pr_info("%s: slicer null\n", __func__);
		return -EINVAL;
	}
	poll_wait(file, &vbi_slicer->buffer.queue, wait);

	if (vbi_slicer->state != VBI_STATE_GO &&
		vbi_slicer->state != VBI_STATE_DONE &&
		vbi_slicer->state != VBI_STATE_TIMEDOUT)
		return 0;

	if (vbi_slicer->buffer.error)
		mask |= (POLLIN | POLLRDNORM | POLLPRI | POLLERR);

	if (!vbi_ringbuffer_empty(&vbi_slicer->buffer))
		mask |= (POLLIN | POLLRDNORM | POLLPRI);

	return mask;
}

/* File operations structure. Defined in linux/fs.h */
static const struct file_operations vbi_fops = {
	.owner   = THIS_MODULE,         /* Owner */
	.open    = vbi_open,            /* Open method */
	.release = vbi_release,         /* Release method */
	.unlocked_ioctl   = vbi_ioctl,           /* Ioctl method */
#ifdef CONFIG_COMPAT
	.compat_ioctl = vbi_compat_ioctl,
#endif
	.read    = vbi_read,
	.poll    = vbi_poll,
	/* ... */
};

static void vbi_parse_param(char *buf_orig, char **parm)
{
	char *ps, *token;
	char delim1[3] = " ";
	char delim2[2] = "\n";
	unsigned int n = 0;

	ps = buf_orig;
	strcat(delim1, delim2);
	while (1) {
		token = strsep(&ps, delim1);
		if (token == NULL)
			break;
		if (*token == '\0')
			continue;
		parm[n++] = token;
	}
}
static void vbi_dump_mem(char *path, struct vbi_dev_s *devp)
{
	struct file *filp = NULL;
	loff_t pos = 0;
	mm_segment_t old_fs = get_fs();

	if (!path) {
		tvafe_pr_info("%s file path is null\n", __func__);
		return;
	}

	set_fs(KERNEL_DS);
	filp = filp_open(path, O_RDWR|O_CREAT, 0666);

	if (IS_ERR(filp)) {
		tvafe_pr_info("create %s error.\n", path);
		return;
	}

	vfs_write(filp, devp->pac_addr_start, devp->mem_size, &pos);
	tvafe_pr_info("write buffer addr:0x%p size: %2u  to %s.\n",
			devp->pac_addr_start, devp->mem_size, path);
	vfs_fsync(filp, 0);
	filp_close(filp, NULL);
	set_fs(old_fs);
}
static void vbi_dump_buf(char *path, struct vbi_dev_s *devp)
{
	struct file *filp = NULL;
	loff_t pos = 0;
	void *buf = NULL;
	mm_segment_t old_fs = get_fs();

	if (!path) {
		tvafe_pr_info("%s file path is null\n", __func__);
		return;
	}

	set_fs(KERNEL_DS);
	filp = filp_open(path, O_RDWR|O_CREAT, 0666);

	if (IS_ERR(filp)) {
		tvafe_pr_info("create %s error.\n", path);
		return;
	}
	buf = (void *)devp->slicer->buffer.data;
	if (buf == NULL) {
		tvafe_pr_info("buf is null!!!.\n");
		return;
	}
	vfs_write(filp, buf, devp->slicer->buffer.size, &pos);
	tvafe_pr_info("write buffer addr:0x%p size: %2u  to %s.\n",
			buf, devp->slicer->buffer.size, path);
	vfs_fsync(filp, 0);
	filp_close(filp, NULL);
	set_fs(old_fs);
}

static const char *vbi_debug_usage_str = {
	"Usage:\n"
	"echo dumpmem /udisk-path/***/xxx.txt > /sys/class/vbi/vbi/debug; dump vbi mem\n"
	"echo dumpbuf /udisk-path/***/xxx.txt > /sys/class/vbi/vbi/debug; dump vbi mem\n"
	"echo status > /sys/class/vbi/vbi/debug; dump vbi status\n"
	"echo enable_tasklet > /sys/class/vbi/vbi/debug; enable vbi tasklet\n"
	"echo data_wmode val(d) > /sys/class/vbi/vbi/debug; set vbi data write mode\n"
	"echo start > /sys/class/vbi/vbi/debug; start vbi\n"
	"echo stop > /sys/class/vbi/vbi/debug; stop vbi\n"
	"echo set_size val(d) > /sys/class/vbi/vbi/debug; set vbi buf size\n"
	"echo set_type val(x) > /sys/class/vbi/vbi/debug; set vbi type\n"
	"echo open > /sys/class/vbi/vbi/debug; open vbi device\n"
	"echo release > /sys/class/vbi/vbi/debug; release vbi device\n"
	"echo dbg_en val(x) > /sys/class/vbi/vbi/debug; enable debug print\n"
};

static ssize_t vbi_show(struct device *dev,
		struct device_attribute *attr, char *buff)
{
	return sprintf(buff, "%s\n", vbi_debug_usage_str);
}

static ssize_t vbi_store(struct device *dev,
		struct device_attribute *attr, const char *buff, size_t len)
{
	char *buf_orig;
	char *parm[6] = {NULL};
	struct vbi_dev_s *devp = dev_get_drvdata(dev);
	struct vbi_slicer_s *vbi_slicer;
	struct vbi_ringbuffer_s *vbi_buffer;
	unsigned int val;
	int ret = 0;

	if (!buff || !devp)
		return len;

	vbi_slicer = devp->slicer;
	vbi_buffer = &(vbi_slicer->buffer);

	buf_orig = kstrdup(buff, GFP_KERNEL);
	vbi_parse_param(buf_orig, (char **)&parm);
	if (!strncmp(parm[0], "dumpmem", strlen("dumpmem"))) {
		if (!parm[1]) {
			tvafe_pr_info("error: miss dump file name!\n");
			goto vbi_store_exit;
		}
		vbi_dump_mem(parm[1], devp);
		tvafe_pr_info("dump mem done!!\n");
	} else if (!strncmp(parm[0], "dumpbuf", strlen("dumpbuf"))) {
		if (!parm[1]) {
			tvafe_pr_info("error: miss dump file name!\n");
			goto vbi_store_exit;
		}
		vbi_dump_buf(parm[1], devp);
		tvafe_pr_info("dump buf done!!\n");
	} else if (!strncmp(parm[0], "status", strlen("status"))) {
		tvafe_pr_info("vcnt:0x%x\n", vcnt);
		tvafe_pr_info("mem_start:0x%x,mem_size:0x%x\n",
			devp->mem_start, devp->mem_size);
		tvafe_pr_info("vbi_start:%d,vbi_data_type:0x%x,vbi_start_code:0x%x,slicer_enable:%d\n",
			devp->vbi_start, devp->vbi_data_type,
			devp->vbi_start_code, devp->slicer_enable);
		tvafe_pr_info("vbi_dto_cc:0x%x,vbi_dto_tt:0x%x\n",
			devp->vbi_dto_cc, devp->vbi_dto_tt);
		tvafe_pr_info("vbi_dto_wss:0x%x,vbi_dto_vps:0x%x\n",
			devp->vbi_dto_wss, devp->vbi_dto_vps);
		tvafe_pr_info("mem_start:0x%p,pac_addr_start:0x%p,pac_addr_end:0x%p\n",
			devp->pac_addr, devp->pac_addr_start,
			devp->pac_addr_end);
		if (vbi_slicer)
			tvafe_pr_info("vbi_slicer:type:%d,state:%d\n",
				vbi_slicer->type, vbi_slicer->state);
		if (vbi_buffer)
			tvafe_pr_info("vbi_buffer:size:%d,data_num:%d,pread:%d,pwrite:%d,data_wmode:%d\n",
				vbi_buffer->size, vbi_buffer->data_num,
				vbi_buffer->pread, vbi_buffer->pwrite,
				vbi_buffer->data_wmode);
		if (vbi_slicer)
			tvafe_pr_info("isr_cnt:%d,slicer_cnt:%d\n",
				      devp->isr_cnt, vbi_slicer->slicer_cnt);
		tvafe_pr_info("drv_version:%s\n", VBI_DRV_VER);
		tvafe_pr_info("dump satus done!!\n");
	} else if (!strncmp(parm[0], "dbg_en", strlen("dbg_en"))) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 16, &val) < 0)
				goto vbi_store_err;
			if (val & VBI_DBG_ISR4) {
				vbi_dbg_isr_cnt = 0;
				vbi_pr_isr_buf = kcalloc(VBI_PR_MAX,
					sizeof(char), GFP_KERNEL);
			} else if ((val & VBI_DBG_ISR4) == 0) {
				if (val & VBI_DBG_TEST2) {
					if (!vbi_pr_isr_buf) {
						vbi_pr_isr_buf =
						kcalloc(VBI_PR_MAX,
							sizeof(char),
							GFP_KERNEL);
					}
				} else if ((val & VBI_DBG_TEST2) == 0) {
					kfree(vbi_pr_isr_buf);
					vbi_pr_isr_buf = NULL;
				}
			}
			if (val & VBI_DBG_READ4) {
				vbi_dbg_read_cnt = 0;
				vbi_pr_read_buf = kcalloc(VBI_PR_MAX,
					sizeof(char), GFP_KERNEL);
			}
			vbi_dbg_en = val;
		}
		tvafe_pr_info("vbi_dbg_en:0x%x\n", vbi_dbg_en);
	} else if (!strncmp(parm[0], "dbg_print_cnt",
		strlen("dbg_print_cnt"))) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 10, &vbi_dbg_print_cnt) < 0)
				goto vbi_store_err;
		}
		tvafe_pr_info("vbi_dbg_print_cnt:%d\n", vbi_dbg_print_cnt);
	} else if (!strncmp(parm[0], "tt_check", strlen("tt_check"))) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 10, &val) < 0)
				goto vbi_store_err;
			vbi_data_tt_check = val;
		}
		tvafe_pr_info("vbi_data_tt_check:%d\n", vbi_data_tt_check);
	} else if (!strncmp(parm[0], "slicer_bypass",
		strlen("slicer_bypass"))) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 10, &vbi_slicer_bypass) < 0)
				goto vbi_store_err;
		}
		tvafe_pr_info("vbi_slicer_bypass:%d\n", vbi_slicer_bypass);
	} else if (!strncmp(parm[0], "slicer_enable",
		strlen("slicer_enable"))) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 10, &val) < 0)
				goto vbi_store_err;
			devp->slicer_enable = val;
		}
		tvafe_pr_info("slicer_enable:%d\n", devp->slicer_enable);
	} else if (!strncmp(parm[0], "data_wmode",
		strlen("data_wmode"))) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 10, &val) < 0)
				goto vbi_store_err;
			vbi_buffer->data_wmode = val;
		}
		tvafe_pr_info("data_wmode:%d\n", vbi_buffer->data_wmode);
	} else if (!strncmp(parm[0], "start", strlen("start"))) {
		W_APB_REG(ACD_REG_22, 0x07080000);
		/* manuel reset vbi */
		W_APB_REG(ACD_REG_22, 0x87080000);
		W_APB_REG(ACD_REG_22, 0x04080000);
		vbi_hw_init(devp);
		vbi_slicer_start(devp);

		vbi_state_reset(devp);
		/* enable data capture function */
		devp->slicer_enable = true;
		devp->vbi_start = true;
		tvafe_pr_info("start done!!!\n");
	} else if (!strncmp(parm[0], "stop", strlen("stop"))) {
		/* disable data capture function */
		devp->slicer_enable = false;
		devp->vbi_start = false;
		vbi_data_stable_flag = 0;
		vbi_slicer_stop(vbi_slicer);
		/* manuel reset vbi */
		/* vbi reset release, vbi agent enable*/
		W_VBI_APB_REG(ACD_REG_22, 0x06080000);
		W_VBI_APB_REG(CVD2_VBI_FRAME_CODE_CTL, 0x10);
		tvafe_pr_info(" disable vbi function\n");
		tvafe_pr_info("stop done!!!\n");
	} else if (!strncmp(parm[0], "set_size", strlen("set_size"))) {
		if (kstrtouint(parm[1], 10, &val) < 0)
			goto vbi_store_err;
		vbi_set_buffer_size(devp, val);
		tvafe_pr_info(" set buf size to %d\n",
			vbi_slicer->buffer.size);
	} else if (!strncmp(parm[0], "set_type", strlen("set_type"))) {
		if (kstrtouint(parm[1], 16, &val) < 0)
			goto vbi_store_err;
		vbi_slicer->type = val;
		vbi_slicer_set(devp, vbi_slicer);
		tvafe_pr_info(" set slicer type to %d\n",
			vbi_slicer->type);
	} else if (!strncmp(parm[0], "open", strlen("open"))) {
		vbi_ringbuffer_init(vbi_buffer, NULL,
			VBI_DEFAULT_BUFFER_PACKAGE_NUM);
		devp->slicer->type = VBI_TYPE_NULL;
		vbi_slicer_state_set(devp, VBI_STATE_ALLOCATED);
		/*disable data capture function*/
		devp->slicer_enable = false;
		devp->vbi_start = false;
		/* request irq */
		ret = request_irq(devp->vs_irq, vbi_isr, IRQF_SHARED,
			devp->irq_name, (void *)devp);
		devp->irq_free_status = 1;
		if (ret < 0)
			tvafe_pr_err("request_irq fail\n");
		tvafe_pr_info(" open ok.\n");
	} else if (!strncmp(parm[0], "release", strlen("release"))) {
		ret = vbi_slicer_free(devp, vbi_slicer);
		devp->slicer_enable = false;
		devp->vbi_start = false;  /*disable data capture function*/
		/* free irq */
		if (devp->irq_free_status == 1)
			free_irq(devp->vs_irq, (void *)devp);
		devp->irq_free_status = 0;
		/* vbi reset release, vbi agent enable */
		W_VBI_APB_REG(ACD_REG_22, 0x06080000);
		W_VBI_APB_REG(CVD2_VBI_FRAME_CODE_CTL, 0x10);
		tvafe_pr_info("[vbi..]device release OK.\n");
	} else {
		tvafe_pr_info("[vbi..]unsupport cmd!!!\n");
	}

vbi_store_exit:
	kfree(buf_orig);
	return len;

vbi_store_err:
	kfree(buf_orig);
	return -EINVAL;
}

static DEVICE_ATTR(debug, 0644, vbi_show, vbi_store);

static int vbi_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct resource *res;
	struct vbi_dev_s *vbi_dev;

	ret = alloc_chrdev_region(&vbi_id, 0, 1, VBI_NAME);
	if (ret < 0) {
		tvafe_pr_err(": failed to allocate major number\n");
		goto fail_alloc_cdev_region;
	}

	vbi_clsp = class_create(THIS_MODULE, VBI_NAME);
	if (IS_ERR(vbi_clsp)) {
		tvafe_pr_err(": can't get vbi_clsp\n");
		goto fail_class_create;
	}

	/* allocate memory for the per-device structure */
	vbi_dev = kzalloc(sizeof(struct vbi_dev_s), GFP_KERNEL);
	if (!vbi_dev) {
		ret = -ENOMEM;
		goto fail_kzalloc_mem;
	}
	memset(vbi_dev, 0, sizeof(struct vbi_dev_s));
	vbi_mem_start = 0;
	vbi_dev_local = vbi_dev;

	/* connect the file operations with cdev */
	cdev_init(&vbi_dev->cdev, &vbi_fops);
	vbi_dev->cdev.owner = THIS_MODULE;
	/* connect the major/minor number to the cdev */
	ret = cdev_add(&vbi_dev->cdev, vbi_id, 1);
	if (ret) {
		tvafe_pr_err(": failed to add device\n");
		goto fail_add_cdev;
	}
	/* create /dev nodes */
	vbi_dev->dev = device_create(vbi_clsp, &pdev->dev,
		MKDEV(MAJOR(vbi_id), 0), NULL, "%s", VBI_NAME);
	if (IS_ERR(vbi_dev->dev)) {
		tvafe_pr_err(": failed to create device node\n");
		ret = PTR_ERR(vbi_dev->dev);
		goto fail_create_device;
	}
	/*create sysfs attribute files*/
	ret = device_create_file(vbi_dev->dev, &dev_attr_debug);
	if (ret < 0) {
		tvafe_pr_err("tvafe: fail to create dbg attribute file\n");
		goto fail_create_dbg_file;
	}

	/* get device memory */
	res = &vbi_memobj;
	ret = of_reserved_mem_device_init(&pdev->dev);
	if (ret == 0) {
		vbi_mem_flag = VBI_MEM_RESERVED;
		vbi_dev->mem_start = res->start;
		vbi_dev->mem_size = res->end - res->start + 1;
		if (vbi_dev->mem_size > DECODER_VBI_SIZE)
			vbi_dev->mem_size = DECODER_VBI_SIZE;
		tvafe_pr_info("vbi: reserved memory phy start_addr is:0x%x, size is:0x%x\n",
				vbi_dev->mem_start, vbi_dev->mem_size);
		/* remap the package vbi hardware address for our conversion */
		vbi_dev->pac_addr_start = phys_to_virt(vbi_dev->mem_start);
		/*ioremap_nocache(vbi_dev->mem_start, vbi_dev->mem_size);*/
		memset(vbi_dev->pac_addr_start, 0, vbi_dev->mem_size);
		if (vbi_dev->pac_addr_start == NULL) {
			tvafe_pr_err(": ioremap error!!!\n");
			goto fail_alloc_mem;
		}
	} else {
		/*vbi memory alloc*/
		tvafe_pr_info("vbi: alloc memory resource\n");
		vbi_dev->mem_size = DECODER_VBI_SIZE;
		vbi_mem_flag = VBI_MEM_MALLOC;
		vbi_dev->pac_addr_start = kzalloc(vbi_dev->mem_size,
			GFP_KERNEL);
		vbi_dev->mem_start = virt_to_phys(vbi_dev->pac_addr_start);
		if (vbi_dev->pac_addr_start == NULL) {
			tvafe_pr_err(": vbi mem malloc failed!!!\n");
			goto fail_alloc_mem;
		}
		tvafe_pr_info("vbi: dma_alloc phy start_addr is:0x%x, size is:0x%x\n",
			vbi_dev->mem_start, vbi_dev->mem_size);
	}

	vbi_dev->mem_size = vbi_dev->mem_size/2;
	vbi_dev->mem_size >>= 4;
	vbi_dev->mem_size <<= 4;
	vbi_mem_start = vbi_dev->mem_start;
	vbi_dev->pac_addr_end = vbi_dev->pac_addr_start + vbi_dev->mem_size - 1;
	tvafe_pr_info(": vbi_dev->pac_addr_start=0x%p, end:0x%p, size:0x%x\n",
		vbi_dev->pac_addr_start, vbi_dev->pac_addr_end,
		vbi_dev->mem_size);
	vbi_dev->pac_addr = vbi_dev->pac_addr_start;
	/* temp buffer for vbi data parse */
	vbi_dev->temp_addr_start = vbi_dev->pac_addr_start + VBI_BUFF3_EA;
	vbi_dev->temp_addr_end = vbi_dev->temp_addr_start + VBI_BUFF3_SIZE - 1;

	mutex_init(&vbi_dev->mutex);
	spin_lock_init(&vbi_dev->lock);
	spin_lock_init(&vbi_dev->vbi_isr_lock);

	/* init drv data */
	dev_set_drvdata(vbi_dev->dev, vbi_dev);
	platform_set_drvdata(pdev, vbi_dev);

	vbi_dev->slicer_enable = false;
	vbi_dev->vbi_start = false;
	/* Initialize tasklet */
	/*tasklet_init(&vbi_dev->tsklt_slicer, vbi_slicer_task,
	 *			(unsigned long)vbi_dev);
	 */
	INIT_WORK(&vbi_dev->slicer_work, vbi_slicer_work);

	vbi_dev->vs_delay = VBI_VS_DELAY;

	vbi_dev->slicer = vmalloc(sizeof(struct vbi_slicer_s));
	if (!vbi_dev->slicer) {
		ret = -ENOMEM;
		goto fail_alloc_mem;
	}
	mutex_init(&vbi_dev->slicer->mutex);
	mutex_init(&vbi_dev->slicer->task_mutex);
	init_waitqueue_head(&vbi_dev->slicer->buffer.queue);
	spin_lock_init(&(vbi_dev->slicer->buffer.lock));
	vbi_dev->slicer->buffer.data = NULL;
	vbi_dev->slicer->state = VBI_STATE_FREE;

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res) {
		tvafe_pr_err("%s: can't get irq resource\n", __func__);
		ret = -ENXIO;
		goto fail_get_resource_irq;
	}
	vbi_dev->vs_irq = res->start;
	snprintf(vbi_dev->irq_name, sizeof(vbi_dev->irq_name),
			"vbi-irq");
	tvafe_pr_info("vbi irq: %d\n", vbi_dev->vs_irq);

	tvafe_pr_info(": driver probe ok\n");
	return 0;

fail_get_resource_irq:
fail_alloc_mem:
	tvafe_pr_err(": vmalloc error!!!\n");
fail_create_dbg_file:
	device_destroy(vbi_clsp, MKDEV(MAJOR(vbi_id), 0));
fail_create_device:
	cdev_del(&vbi_dev->cdev);
fail_add_cdev:
	kfree(vbi_dev);
fail_kzalloc_mem:
	tvafe_pr_err(": failed to allocate memory for vbi device\n");
	class_destroy(vbi_clsp);
fail_class_create:
	unregister_chrdev_region(vbi_id, 1);
fail_alloc_cdev_region:
	return ret;
}

static int vbi_remove(struct platform_device *pdev)
{
	struct vbi_dev_s *vbi_dev;

	vbi_dev = platform_get_drvdata(pdev);

	mutex_destroy(&vbi_dev->slicer->task_mutex);
	mutex_destroy(&vbi_dev->slicer->mutex);
	mutex_destroy(&vbi_dev->mutex);
	/*tasklet_kill(&vbi_dev->tsklt_slicer);*/
	cancel_work_sync(&vbi_dev->slicer_work);
	if (vbi_dev->pac_addr_start) {
		if (vbi_mem_flag == VBI_MEM_RESERVED)
			iounmap(vbi_dev->pac_addr_start);
		else if (vbi_mem_flag == VBI_MEM_MALLOC)
			kfree(vbi_dev->pac_addr_start);
	}
	vfree(vbi_dev->slicer);
	device_destroy(vbi_clsp, MKDEV(MAJOR(vbi_id), 0));
	cdev_del(&vbi_dev->cdev);
	kfree(vbi_dev);
	class_destroy(vbi_clsp);
	unregister_chrdev_region(vbi_id, 0);

	tvafe_pr_info(": driver removed ok.\n");

	return 0;
}

#ifdef CONFIG_PM
static int vbi_drv_suspend(struct platform_device *pdev, pm_message_t state)
{
	return 0;
}

static int vbi_drv_resume(struct platform_device *pdev)
{
	return 0;
}
#endif

static void vbi_drv_shutdown(struct platform_device *pdev)
{
	struct vbi_dev_s *vbi_dev;

	vbi_dev = platform_get_drvdata(pdev);
	vbi_dev->slicer_enable = false;
	vbi_dev->vbi_start = false;
	/*tasklet_kill(&vbi_dev->tsklt_slicer);*/
	cancel_work_sync(&vbi_dev->slicer_work);
	if (vbi_dev->irq_free_status == 1)
		free_irq(vbi_dev->vs_irq, (void *)vbi_dev);
	vbi_dev->irq_free_status = 0;
	tvafe_pr_info("%s ok.\n", __func__);

}

static const struct of_device_id vbi_dt_match[] = {
	{
	.compatible     = "amlogic, vbi",
	},
	{},
};

static struct platform_driver vbi_driver = {
	.probe      = vbi_probe,
	.remove     = vbi_remove,
#ifdef CONFIG_PM
	.suspend	= vbi_drv_suspend,
	.resume		= vbi_drv_resume,
#endif
	.shutdown   = vbi_drv_shutdown,
	.driver     = {
		.name   = VBI_DRIVER_NAME,
		.of_match_table = vbi_dt_match,
	}
};

static int __init vbi_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&vbi_driver);
	if (ret != 0) {
		tvafe_pr_err("failed to register vbi module, error %d\n",
			ret);
		return -ENODEV;
	}
	/*tvafe_pr_info("vbi: vbi_init.\n");*/
	return 0;
}

static void __exit vbi_exit(void)
{
	platform_driver_unregister(&vbi_driver);
}

static int vbi_mem_device_init(struct reserved_mem *rmem,
		struct device *dev)
{
	s32 ret = 0;
	struct resource *res = NULL;

	if (!rmem) {
		tvafe_pr_info("Can't get reverse mem!\n");
		ret = -EFAULT;
		return ret;
	}
	res = &vbi_memobj;
	res->start = rmem->base;
	res->end = rmem->base + rmem->size - 1;

	tvafe_pr_info("init vbi memsource 0x%lx->0x%lx\n",
		(unsigned long)res->start, (unsigned long)res->end);

	return 0;
}

static const struct reserved_mem_ops rmem_vbi_ops = {
	.device_init = vbi_mem_device_init,
};

static int __init vbi_mem_setup(struct reserved_mem *rmem)
{
	rmem->ops = &rmem_vbi_ops;
	tvafe_pr_info("vbi share mem setup\n");
	return 0;
}

module_init(vbi_init);
module_exit(vbi_exit);
RESERVEDMEM_OF_DECLARE(vbi, "amlogic, vbi-mem",
	vbi_mem_setup);

MODULE_DESCRIPTION("AMLOGIC vbi driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("frank  <frank.zhao@amlogic.com>");

