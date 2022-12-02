// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/module.h>
#include <linux/dma-mapping.h>
#include <linux/wait.h>
#include <linux/list.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/reset.h>
#include <linux/clk.h>
#include <linux/compat.h>
#include <linux/of_reserved_mem.h>
#include <linux/of_address.h>
#include <linux/kfifo.h>
#include <linux/kthread.h>
#include <linux/signal.h>
#include <linux/sysfs.h>

#include <linux/amlogic/secmon.h>

#include "aml_aucpu.h"

#define AUCPU_PLATFORM_DEVICE_NAME "aml_aucpu"
#define AUCPU_DEV_NAME "aml_aucpu"
#define AUCPU_CLASS_NAME "aml_aucpu"

#ifndef VM_RESERVED	/*for kernel up to 3.7.0 version*/
#define VM_RESERVED (VM_DONTEXPAND | VM_DONTDUMP)
#endif

#define MHz (1000000)

#define AUCPU_MEMORY_SIZE_IN_BYTE	(64 * SZ_1K)
#define AUCPU_UPD_SZ			(32)
#define AUCPU_DBG_BUF_SZ		(32 * SZ_1K)
#define AUCPU_DBG_PAD_SZ		(256)

#define LOG_ALL			0
#define LOG_INFO		1
#define LOG_DEBUG		2
#define LOG_ERROR		3

#define AUCPU_LOG_EMERG		0 /* ERROR */
#define AUCPU_LOG_ALERT		1 /* ERROR */
#define AUCPU_LOG_CRIT		2 /* ERROR */
#define AUCPU_LOG_ERR		3 /* ERROR */
#define AUCPU_LOG_WARNING	4 /* WARN */
#define AUCPU_LOG_NOTICE	5 /* INFO */
#define AUCPU_LOG_INFO		6 /* INFO */
#define AUCPU_LOG_DEBUG		7 /* DEBUG */

#define DBG_POLL_TIME		(HZ / 100)

#define aucpu_pr(level, x...) \
	do { \
		if ((level) >= print_level) \
			printk(x); \
	} while (0)

static s32 print_level = LOG_ERROR; //LOG_ALL;
static s32 dbg_level = AUCPU_LOG_ERR;

/* back up value for dts config failure */
#define AUCPU_REG_BASE_ADDR	(0xFE09E080)
#define AUCPU_REG_SIZE		(0x100)

#define CMD_SEQ_ADDR		(0x0)
#define CMD_TYPE_ADDR		(0x4)
#define CMD_OPT_ADDR		(0x8)
#define CMD_SRC_ADDR		(0xC)
#define CMD_SRC_SZ		(0x10)
#define CMD_DST_ADDR		(0x14)
#define CMD_DST_SZ		(0x18)
#define CMD_RPT_ADDR		(0x1C)
#define CMD_RPT_SZ		(0x20)
#define CMD_UPD_ADDR		(0x24)

#define RESP_SEQ_ADDR		(0x80)
#define RESP_TYPE_ADDR		(0x84)
#define RESP_RST_ADDR		(0x88)

#define VERSION_ADDR		(0xA8)

#define DBG_REG_WRPTR		(0xC0)
#define DBG_REG_STAGE		(0xE8)
#define DBG_REG_ERROR		(0xEC)

#define ReadAucpuRegister(addr) \
	readl((void __iomem *)(s_aucpu_register_base + (addr)))

#define WriteAucpuRegister(addr, val) \
	writel((u32)val, (void __iomem *)(s_aucpu_register_base + (addr)))

enum aml_aucpu_cmd_type {
	CMD_NONE = 0,
	CMD_SET_DBG_BUF_LEVEL,
	CMD_RESET_DBG_BUF,
	CMD_SET_DBG_LEVEL,
	CMD_INST_ADD,
	CMD_INST_START,
	CMD_INST_STOP,
	CMD_INST_FLUSH,
	CMD_INST_REMOVE,
	CMD_RESET_ALL,
	CMD_MAX,
};

static bool use_reserve;
static s32 s_aucpu_irq;
static bool s_aucpu_irq_requested;
static bool s_aucpu_delaywork_requested;

static s32 s_aucpu_major;
static s32 s_register_flag;
static struct device *aucpu_dev;
static ulong s_aucpu_register_base;
static s32 load_firmware_status = AUCPU_DRVERR_NO_INIT;

struct aucpu_buffer_t {
	u32 size;
	ulong phys_addr; /* physical address */
	ulong base; /* kernel logical address */
};

struct aml_cmd_set {
	u32 seq; /* command sequence number */
	u32 cmd_type; //command type + instance index
	u32 media_options; // media type + options
	u32 src_buf_start; // source es/desc buffer address
	u32 src_buf_sz; //source es buffer size or desc buffer size
	u32 dst_buf_start; // destination es/desc buffer address
	u32 dst_buf_sz; //destination es buffer size or desc buffer size
	u32 dst_report_start; //report structure start address
	u32 dst_report_sz; //report structure max sz
	u32 src_update_start; //input source update structure start address
};

struct aml_cmd_resp {
	u32 resp_seq; /* resp sequence number */
	u32 cmd_type; /* original cmd + instance index */
	u32 result_status; /* 0 OK, others error code */
};

struct aucpu_report_t {
	u32 r_status;          /* execut status */
	u32 r_dst_byte_cnt;    /* destination total bytes */
	u32 r_dst_wrptr;       /* destination write position phy address*/
	u32 r_src_wrptr;       /* origin src write position phy address*/
	u32 r_src_rdptr;       /* origin src write position phy address*/
	u32 r_src_byte_cnt;    /* origin src total bytes */

	u32 total_bytes;     /* total bytes on current round */
	u32 buffed_bytes;    /* buffered bytes on current round */
	u32 processed_bytes; /* processed bytes on current round */
	u32 dbg_buf_wrptr;   /* print log position in ddr buffer */
	u32 run_cnt;
};

struct aucpu_inst_t {
	u32 inst_status; // current instant status
	u32 media_type;
	u32 config_options;// bit 0: src update by DDR (1) or dmx (0)
				// bit 1: source is linklist 1, or ringbuffer 0
				// bit 2: output is linklist 1, or ringbuffer 0
	struct aucpu_buffer_t src_buf;  // source es buffer
	struct aucpu_buffer_t dst_buf;  // destination es buffer
	struct aucpu_buffer_t reprt_buf; //report result buffer
	struct aucpu_buffer_t in_upd_buf; //report result buffer
	u32 src_update_index; //source pointer update demux channel index
	s32 inst_handle; //handle from aucpu returned.
	// below are runtime  status
	u32 work_state; // the working  status of the instance
	u32 src_wrptr; //source buffer wrptr
	u32 src_rdptr; //source rdprt
	u32 src_byte_cnt; // source total processed bytes shadow
	u32 dst_wrptr;  // dst wrptr shadow
	u32 dst_byte_cnt; // dst total processed bytes shadow

	u32 total_bytes;     /* total bytes on current round */
	u32 buffed_bytes;    /* buffered bytes on current round */
	u32 processed_bytes; /* processed bytes on current round */
	u32 run_cnt; //running count of rounds
};

struct aucpu_ctx_t {
	u32 activated_inst_num; /* active instances number */
	struct aucpu_inst_t strm_inst[AUCPU_MAX_INTST_NUM];
	struct aml_cmd_set last_cmd;
	struct aucpu_buffer_t aucpu_reg; /* register assignment */
	struct aucpu_buffer_t aucpu_mem; /* local total buffer pool */

	struct aucpu_buffer_t t_upd_buf; /* total update input buf */
	struct aucpu_buffer_t t_rpt_buf; /* total report out buf */
	struct aucpu_buffer_t dbg_buf;   /* debug buf */
	u32 dbg_buf_size;
	u32 dbg_pad_size;
	struct mutex mutex;  /* command muxtex */
	struct delayed_work dbg_work; /* aucpu debug log */
	struct device *aucpu_device;
	u32 debug_buf_rdptr;
	u32 debug_buf_wrptr;
	u32 aucpu_print_level;
	u32 aucpu_trace_debug; /* error trace line */
	u32 aucpu_trace_error; /* debug trace error line */
};

static struct platform_device *aucpu_pdev;
static struct aucpu_ctx_t *s_aucpu_ctx;

static void dma_flush(u32 buf_start, u32 buf_size)
{
	if (aucpu_pdev)
		dma_sync_single_for_device(&aucpu_pdev->dev, buf_start,
			buf_size, DMA_TO_DEVICE);
}

static void cache_flush(u32 buf_start, u32 buf_size)
{
	if (aucpu_pdev)
		dma_sync_single_for_cpu(&aucpu_pdev->dev, buf_start,
			buf_size, DMA_FROM_DEVICE);
}

s32 aucpu_hw_reset(void)
{
	aucpu_pr(LOG_DEBUG, "request aucpu reset from application\n");
	return 0;
}

static irqreturn_t aucpu_irq_handler(s32 irq, void *dev_id)
{
	struct aucpu_ctx_t *dev = (struct aucpu_ctx_t *)dev_id;

	aucpu_pr(LOG_ALL, "Receive an AUCPU interrupt\n");

	schedule_delayed_work(&dev->dbg_work, 0);

	return IRQ_HANDLED;
}

static s32 alloc_local_dma_buf(struct device *dev,
			       struct aucpu_buffer_t *buf,
			       u32 size, u32 dir)
{
	u32 required;
	dma_addr_t dma_addr = 0;

	required = roundup(size, PAGE_SIZE);
	buf->base = (ulong)kzalloc(required, GFP_KERNEL | GFP_DMA);
	if (buf->base == 0) {
		aucpu_pr(LOG_ERROR,
			 "failed to allocate aucpu local memory\n");
		return -ENOMEM;
	}
	buf->size = required;
	dma_addr = dma_map_single(dev, (void *)buf->base, buf->size, dir);
	if (dma_mapping_error(dev, dma_addr)) {
		aucpu_pr(LOG_ERROR, "error dma mapping addr 0x%lx\n",
			 (ulong)dma_addr);
		kfree((void *)buf->base);
		buf->size = 0;
		return -EINVAL;
	}
	buf->phys_addr = (ulong)dma_addr;
	return 0;
}

static s32 init_Aucpu_local_buf(struct device *dev)
{
	u32 index, update_size, err;
	ulong phy, base_addr;
	struct aucpu_buffer_t *pbuf;
	struct aucpu_ctx_t  *pctx = s_aucpu_ctx;
	/* update buffers */
	update_size = roundup(sizeof(struct aml_aucpu_buf_upd), AUCPU_UPD_SZ);
	if (use_reserve) {
		phy = pctx->aucpu_mem.phys_addr;
		base_addr = pctx->aucpu_mem.base;
	} else {
		err = alloc_local_dma_buf(dev, &pctx->t_upd_buf,
					  update_size * AUCPU_MAX_INTST_NUM,
					  DMA_TO_DEVICE);
		if (err) {
			aucpu_pr(LOG_ERROR, "error alloc update buffer\n");
			return err;
		}
		phy = pctx->t_upd_buf.phys_addr;
		base_addr = pctx->t_upd_buf.base;
	}

	aucpu_pr(LOG_DEBUG, "local upd buf start from 0x%lx item size 0x%x\n",
		 phy, update_size);
	for (index = 0; index < AUCPU_MAX_INTST_NUM; index++) {
		pbuf = &pctx->strm_inst[index].in_upd_buf;
		pbuf->phys_addr = phy;
		pbuf->base = base_addr;
		pbuf->size = update_size;
		phy += update_size;
		base_addr += update_size;
		aucpu_pr(LOG_INFO, "upd buf %d @0x%lx size 0x%x, phy 0x%lx\n",
			 index, pbuf->base, pbuf->size, pbuf->phys_addr);
	}

	/* report buffers */
	update_size = roundup(sizeof(struct aucpu_report_t), AUCPU_UPD_SZ);
	if (!use_reserve) {
		err = alloc_local_dma_buf(dev, &pctx->t_rpt_buf,
					  update_size * AUCPU_MAX_INTST_NUM,
					  DMA_FROM_DEVICE);
		if (err) {
			aucpu_pr(LOG_ERROR, "error alloc report buffer\n");
			return err;
		}
		phy = pctx->t_rpt_buf.phys_addr;
		base_addr = pctx->t_rpt_buf.base;
	}
	aucpu_pr(LOG_DEBUG, "local rpt buf start from 0x%lx item size 0x%x\n",
		 phy, update_size);
	for (index = 0; index < AUCPU_MAX_INTST_NUM; index++) {
		pbuf = &pctx->strm_inst[index].reprt_buf;
		pbuf->phys_addr = phy;
		pbuf->base = base_addr;
		pbuf->size = update_size;
		phy += update_size;
		base_addr += update_size;
		aucpu_pr(LOG_INFO, "rpt buf %d @0x%lx size 0x%x, phy 0x%lx\n",
			 index, pbuf->base, pbuf->size, pbuf->phys_addr);
	}
	/* debug buffer */
	update_size = AUCPU_DBG_BUF_SZ + AUCPU_DBG_PAD_SZ;
	pbuf = &pctx->dbg_buf;
	if (!use_reserve) {
		err = alloc_local_dma_buf(dev, pbuf, update_size,
					  DMA_FROM_DEVICE);
		if (err) {
			aucpu_pr(LOG_ERROR, "error alloc debug buffer\n");
			return err;
		}
	} else {
		pbuf->phys_addr = phy;
		pbuf->base = base_addr;
		pbuf->size = AUCPU_DBG_BUF_SZ + AUCPU_DBG_PAD_SZ;
	}

	pctx->dbg_buf_size = AUCPU_DBG_BUF_SZ;
	pctx->dbg_pad_size = AUCPU_DBG_PAD_SZ;
	aucpu_pr(LOG_DEBUG, "dbg buf @0x%lx size 0x%x pad 0x%x phy 0x%lx\n",
		 pbuf->base, pctx->dbg_buf_size, pctx->dbg_pad_size,
		 pbuf->phys_addr);
	phy += pbuf->size;

	if (use_reserve &&
	    (phy - pctx->aucpu_mem.phys_addr) > pctx->aucpu_mem.size) {
		aucpu_pr(LOG_ERROR, "Allocated too less mem size 0x%x\n",
			 pctx->aucpu_mem.size);
		return -ENOMEM;
	}
	return 0;
}

static void uninit_Aucpu_local_buf(struct device *dev)
{
	struct aucpu_ctx_t  *pctx = s_aucpu_ctx;

	if (use_reserve)
		return;
	if (pctx->dbg_buf.base) {
		dma_unmap_single(dev, pctx->dbg_buf.phys_addr,
				 pctx->dbg_buf.size, DMA_FROM_DEVICE);
		kfree((void *)pctx->dbg_buf.base);
		pctx->dbg_buf.base = 0;
	}
	if (pctx->t_rpt_buf.base) {
		dma_unmap_single(dev, pctx->t_rpt_buf.phys_addr,
				 pctx->t_rpt_buf.size, DMA_FROM_DEVICE);
		kfree((void *)pctx->t_rpt_buf.base);
		pctx->t_rpt_buf.base = 0;
	}
	if (pctx->t_upd_buf.base) {
		dma_unmap_single(dev, pctx->t_upd_buf.phys_addr,
				 pctx->t_upd_buf.size, DMA_TO_DEVICE);
		kfree((void *)pctx->t_upd_buf.base);
		pctx->t_upd_buf.base = 0;
	}
}

static s32 send_aucpu_cmd(struct aml_cmd_set *cmd)
{
	u32 seq; /* command sequence number */
	u32 resp_seq;
	u32 cmd_type; //command type + instance index
	s32 cmd_code;
	s32 inst_idx;
	struct aucpu_ctx_t *pctx;

	ulong expires;
	s32 result = AUCPU_NO_ERROR;

	pctx = s_aucpu_ctx;
	WriteAucpuRegister(CMD_TYPE_ADDR, cmd->cmd_type);
	WriteAucpuRegister(CMD_OPT_ADDR, cmd->media_options);
	WriteAucpuRegister(CMD_SRC_ADDR, cmd->src_buf_start);
	WriteAucpuRegister(CMD_SRC_SZ, cmd->src_buf_sz);
	WriteAucpuRegister(CMD_DST_ADDR, cmd->dst_buf_start);
	WriteAucpuRegister(CMD_DST_SZ, cmd->dst_buf_sz);
	WriteAucpuRegister(CMD_RPT_ADDR, cmd->dst_report_start);
	WriteAucpuRegister(CMD_RPT_SZ, cmd->dst_report_sz);
	WriteAucpuRegister(CMD_UPD_ADDR, cmd->src_update_start);

	seq = ReadAucpuRegister(RESP_SEQ_ADDR);
	if (seq == cmd->seq) {
		aucpu_pr(LOG_ERROR, "AUCPU sequence no updated\n");
		seq++;
	} else {
		seq = cmd->seq;
	}
	WriteAucpuRegister(CMD_SEQ_ADDR, seq); // trigger the seq
	// wait for response
	expires = jiffies + HZ / 10;
	do {
		resp_seq = ReadAucpuRegister(RESP_SEQ_ADDR);
		if (resp_seq == seq)
			break;
		if (time_after(jiffies, expires)) {
			aucpu_pr(LOG_ERROR,
				 "AUCPU Command %x timeout seq %d tobe %d\n",
				 cmd->cmd_type, resp_seq, seq);
			result = AUCPU_DRVERR_CMD_TIMEOUT;
			break;
		}
		usleep_range(20, 2000);
	} while (1);
	if (resp_seq == seq) {
		cmd_type = ReadAucpuRegister(RESP_TYPE_ADDR);
		result = ReadAucpuRegister(RESP_RST_ADDR);
		if (result == AUCPU_NO_ERROR) {
			cmd_code = cmd_type & 0xff;
			inst_idx = (cmd_type >> 8) & 0xff;
			if (cmd_code != (cmd->cmd_type & 0xff)) {
				aucpu_pr(LOG_ERROR,
					 "Command no match exp %d now %d\n",
					 cmd->cmd_type & 0xff, cmd_code);
				result = AUCPU_DRVERR_CMD_NOMATCH;
			}
			if (cmd_code == CMD_INST_ADD)
				result = inst_idx;
		}
		cmd->seq = resp_seq + 1;
	}
	return result;
}

void change_debug_level(void)
{
	struct aucpu_ctx_t *pctx;
	struct aml_cmd_set *cmd;
	s32 res = AUCPU_NO_ERROR;

	pctx = s_aucpu_ctx;
	mutex_lock(&pctx->mutex);
	cmd = &pctx->last_cmd;

	cmd->cmd_type = CMD_SET_DBG_LEVEL;
	cmd->media_options = dbg_level;
	pctx->aucpu_print_level = dbg_level;
	res = send_aucpu_cmd(cmd);
	if (res < 0) { // failed
		aucpu_pr(LOG_ERROR, "aucpu set dbg buf failed\n");
	}
	mutex_unlock(&pctx->mutex);
}

void setup_debug_polling(void)
{
	struct aucpu_ctx_t *pctx;
	struct aml_cmd_set *cmd;
	s32 res = AUCPU_NO_ERROR;

	pctx = s_aucpu_ctx;
	mutex_lock(&pctx->mutex);
	cmd = &pctx->last_cmd;
	cmd->cmd_type = CMD_SET_DBG_BUF_LEVEL;
	cmd->dst_buf_start = pctx->dbg_buf.phys_addr;
	cmd->dst_buf_sz = pctx->dbg_buf_size;  //exclude the padding
	cmd->media_options = dbg_level;
	pctx->aucpu_print_level = dbg_level;
	res = send_aucpu_cmd(cmd);
	if (res < 0) // failed
		aucpu_pr(LOG_ERROR, "aucpu set dbg buf failed\n");
	mutex_unlock(&pctx->mutex);
	pctx->debug_buf_rdptr = pctx->dbg_buf.phys_addr;
	pctx->debug_buf_wrptr = pctx->dbg_buf.phys_addr;
}

static bool do_debug_work_in(struct aucpu_ctx_t *pctx)
{
	u32 now_wrptr, len, trace, count = 0;
	int fullness;
	unsigned char *cur_ptr;

	trace = ReadAucpuRegister(DBG_REG_STAGE);
	if (trace != pctx->aucpu_trace_debug) {
		aucpu_pr(LOG_ERROR, "aucpu trace orig 0x%x now 0x%x",
			 pctx->aucpu_trace_debug, trace);
		aucpu_pr(LOG_ERROR, "--stage value %d line %d\n",
			 trace >> 16, trace & 0xffff);
		pctx->aucpu_trace_debug = trace;
	}

	trace = ReadAucpuRegister(DBG_REG_ERROR);
	if (trace != pctx->aucpu_trace_error) {
		aucpu_pr(LOG_ERROR, "aucpu trace error orig 0x%x now 0x%x",
			 pctx->aucpu_trace_error, trace);
		aucpu_pr(LOG_ERROR, "--Err value %d line %d\n",
			 trace >> 16, trace & 0xffff);
		pctx->aucpu_trace_error = trace;
	}
	// get latest  debuf buffer wrptr
	now_wrptr = ReadAucpuRegister(DBG_REG_WRPTR);
	if (now_wrptr == pctx->debug_buf_wrptr) // no new update return
		return 0;
	if (now_wrptr < pctx->dbg_buf.phys_addr ||
	    now_wrptr >= (pctx->dbg_buf.phys_addr + pctx->dbg_buf.size)) {
		now_wrptr = pctx->debug_buf_wrptr;
		return 0;
	}
	pctx->debug_buf_wrptr = now_wrptr;
	if (pctx->debug_buf_wrptr >=
	    (pctx->dbg_buf.phys_addr + pctx->dbg_buf_size))
		pctx->debug_buf_wrptr -= pctx->dbg_buf_size;

	fullness = now_wrptr - pctx->debug_buf_rdptr;
	if (fullness < 0)
		fullness += pctx->dbg_buf_size;
	//invalidate the whole DBG _buffer as it is small
	cache_flush(pctx->dbg_buf.phys_addr, pctx->dbg_buf.size);
	cur_ptr = (unsigned char *)(pctx->dbg_buf.base +
			(pctx->debug_buf_rdptr - pctx->dbg_buf.phys_addr));
	if (fullness > 1024)
		aucpu_pr(LOG_ERROR, "Aucpu:Too many logs, fullness %d\n",
			 fullness);
	while (fullness > 0) {
		len = strlen(cur_ptr); // count the null
		if (print_level <= LOG_DEBUG && len &&
		    (fullness <= 1024 || count < 20))
			aucpu_pr(LOG_ERROR, "Aucpu:%s", cur_ptr);
		len += 1; // add the null at the end
		cur_ptr += len;
		fullness -= len;
		count++;
		if (cur_ptr > (unsigned char *)(pctx->dbg_buf.base
				+ pctx->dbg_buf.size)) {
			aucpu_pr(LOG_ERROR, "AUCPU log over boundary\n");
			break;
		}
		if (cur_ptr > (unsigned char *)(pctx->dbg_buf.base
				+ pctx->dbg_buf_size))
			cur_ptr -= pctx->dbg_buf_size; // wrap around
		if (fullness < 0) {
			aucpu_pr(LOG_ERROR, "AUCPU log no align\n");
			break;
		}
	}
	if (fullness) // something wrong
		aucpu_pr(LOG_ERROR, "AUCPU wrong log full%d wrpt %x rdptr %x\n",
			 fullness, now_wrptr, pctx->debug_buf_rdptr);
	// check again for new update`
	pctx->debug_buf_rdptr = now_wrptr;
	now_wrptr = ReadAucpuRegister(DBG_REG_WRPTR);
	return (now_wrptr == pctx->debug_buf_wrptr) ? 0 : 1;
}

static void do_polling_work(struct work_struct *work)
{
	bool ret;

	struct aucpu_ctx_t *pctx = s_aucpu_ctx;
	int need_retry = 1;
	unsigned long delay = DBG_POLL_TIME;

	while (need_retry)
		need_retry = do_debug_work_in(pctx);

	if (pctx->aucpu_print_level != dbg_level)
		change_debug_level();

	ret = schedule_delayed_work(&pctx->dbg_work, delay);
	if (!ret) {
		aucpu_pr(LOG_ERROR, "debug task delay failed!\n");
		cancel_delayed_work(&pctx->dbg_work);
	}
}

#define AUCPU_FW_PATH "/lib/firmware/aucpu_fw.bin"
#define AUCPU_MAX_FW_SZ		(48 * SZ_1K)
#include <linux/fs.h>
#include <linux/firmware.h>
#include <linux/arm-smccc.h>

#define FID_FW_LOAD (0x82000077)
#define   FW_TYPE_AUCPU (0)
#define   FW_TYPE_AUCPU_STOP (1)

unsigned long sec_fw_cmd(u32 type, void *fw, size_t size)
{
	struct arm_smccc_res smccc;
	void __iomem *sharemem_input_base;
	long sharemem_phy_input_base;

	sharemem_input_base = get_meson_sm_input_base();
	sharemem_phy_input_base = get_secmon_phy_input_base();
	if (!sharemem_input_base || !sharemem_phy_input_base)
		return -1;

	meson_sm_mutex_lock();
	if (size)
		memcpy(sharemem_input_base,
			(const void *)fw, size);

	//asm __volatile__("" : : : "memory");

	arm_smccc_smc(FID_FW_LOAD, type, size, 0, 0, 0, 0, 0, &smccc);
	meson_sm_mutex_unlock();

	return smccc.a0;
}

static int load_start_aucpu_fw(struct device *device)
{
	const struct firmware *my_fw = NULL;
	char *fw;

	int result = 0;
	unsigned int length = 0;

	aucpu_pr(LOG_DEBUG, "FW\n");

	result = request_firmware(&my_fw, "aucpu_fw.bin", device);
	if (!my_fw || result < 0) {
		aucpu_pr(LOG_ERROR, "load aucpu_fw.bin fail\n");
		result = AUCPU_ERROR_NOT_IMPLEMENTED;
		return result;
	}

	fw = (char *)my_fw->data;
	length = my_fw->size;

	aucpu_pr(LOG_INFO, "FW read size %d data 0x%x-%x-%x-%x\n", length,
		 fw[0], fw[1], fw[2], fw[3]);

	result = sec_fw_cmd(FW_TYPE_AUCPU, fw, length);

	release_firmware(my_fw);

	aucpu_pr(LOG_INFO, "download fw success reset start AUCPU\n");
	aucpu_pr(LOG_INFO, "[%s] done  result %d\n", __func__, result);
	return result;
}

static void stop_aucpu_fw(struct device *device)
{
	int result = 0;

	result = sec_fw_cmd(FW_TYPE_AUCPU_STOP, NULL, 0);
	aucpu_pr(LOG_INFO, "[%s] done  result %d\n", __func__, result);
}

s32 aml_aucpu_strm_create(struct aml_aucpu_strm_buf *src,
			  struct aml_aucpu_strm_buf *dst,
			  struct aml_aucpu_inst_config *cfg)
{
	struct aucpu_ctx_t *pctx;
	struct aucpu_inst_t *pinst;
	struct aml_cmd_set *cmd;
	s32 handle_idx; // handle index AUCPU_STA_NONE
	s32 res = AUCPU_NO_ERROR;

	pctx = s_aucpu_ctx;
	if (!aucpu_dev) {
		aucpu_pr(LOG_ERROR, "AUCPU driver did not init yet\n");
		return AUCPU_DRVERR_NO_INIT;
	}
	if (pctx->activated_inst_num >= AUCPU_MAX_INTST_NUM) {
		aucpu_pr(LOG_ERROR, "AUCPU too many stream instances\n");
		return AUCPU_DRVERR_TOO_MANY_STRM;
	}
	/* check parmeters */
	aucpu_pr(LOG_INFO, "%s src 0x%lx size 0x%x dst 0x%lx size 0x%x\n",
		 __func__, src->phy_start, src->buf_size,
		 dst->phy_start, dst->buf_size);
	aucpu_pr(LOG_INFO, "config media_type %d chn %d flags 0x%x-0x%x-0x%x\n",
		 cfg->media_type, cfg->dma_chn_id, cfg->config_flags,
		 src->buf_flags, dst->buf_flags);

	if (cfg->media_type <= MEDIA_UNKNOWN || cfg->media_type >= MEDIA_MAX) {
		aucpu_pr(LOG_ERROR, "invalid media format %d\n",
			 cfg->media_type);
		return AUCPU_DRVERR_INVALID_TYPE;
	}

	mutex_lock(&pctx->mutex);
	for (handle_idx = 0; handle_idx < AUCPU_MAX_INTST_NUM; handle_idx++) {
		pinst = &pctx->strm_inst[handle_idx];
		if (pinst->inst_status == AUCPU_STA_NONE)
			break; // found one empty slot
	}
	if (handle_idx >= AUCPU_MAX_INTST_NUM) {
		aucpu_pr(LOG_ERROR, "AUCPU can not find empty slot\n");
		mutex_unlock(&pctx->mutex);
		return AUCPU_DRVERR_NO_SLOT;
	}

	pinst->media_type = cfg->media_type;
	pinst->config_options = cfg->config_flags & 0x1;
	pinst->src_update_index = cfg->dma_chn_id;
	if (src->buf_flags & 0x1)
		pinst->config_options |= 0x2;
	if (dst->buf_flags & 0x1)
		pinst->config_options |= 0x4;

	pinst->src_buf.phys_addr = src->phy_start;
	pinst->src_buf.size = src->buf_size;
	pinst->dst_buf.phys_addr = dst->phy_start;
	pinst->dst_buf.size = dst->buf_size;
	pinst->src_wrptr = pinst->src_buf.phys_addr;
	pinst->src_rdptr = pinst->src_buf.phys_addr;
	pinst->src_byte_cnt = 0;
	pinst->dst_wrptr = pinst->dst_buf.phys_addr;
	pinst->dst_byte_cnt = 0;
	cmd = &pctx->last_cmd;
	cmd->cmd_type = CMD_INST_ADD | ((pinst->inst_handle & 0xff) << 8);
	cmd->media_options = (pinst->config_options << 8) | pinst->media_type;
	cmd->src_buf_start = pinst->src_buf.phys_addr;
	cmd->src_buf_sz = pinst->src_buf.size;
	cmd->dst_buf_start = pinst->dst_buf.phys_addr;
	cmd->dst_buf_sz = pinst->dst_buf.size;
	cmd->dst_report_start = pinst->reprt_buf.phys_addr;
	cmd->dst_report_sz = pinst->reprt_buf.size;
	if (pinst->config_options & 0x1)
		cmd->src_update_start = pinst->in_upd_buf.phys_addr;
	else
		cmd->src_update_start = pinst->src_update_index;
	res = send_aucpu_cmd(cmd);
	if (res >= 0) { // success
		pinst->inst_handle = res;
		pinst->inst_status = AUCPU_STA_IDLE; // change status
		pctx->activated_inst_num++;
		res = handle_idx;
		aucpu_pr(LOG_INFO, "New strm %d  %d created\n",
			 pinst->inst_handle, res);
	}
	mutex_unlock(&pctx->mutex);
	return res;
}
EXPORT_SYMBOL(aml_aucpu_strm_create);

static s32 valid_handle2Inst(s32 handle, struct aucpu_ctx_t *pctx,
			     struct aucpu_inst_t **inst)
{
	struct aucpu_inst_t *pinst;

	if (!aucpu_dev) {
		aucpu_pr(LOG_ERROR, "AUCPU driver did not init yet\n");
		return AUCPU_DRVERR_NO_INIT;
	}
	if (pctx->activated_inst_num == 0) {
		aucpu_pr(LOG_ERROR, "AUCPU No strm active\n");
		return AUCPU_DRVERR_NO_ACTIVE;
	}
	if (handle < 0 || handle >= AUCPU_MAX_INTST_NUM) {
		aucpu_pr(LOG_ERROR, "AUCPU invalid strm handle\n");
		return AUCPU_DRVERR_INVALID_HANDLE;
	}
	pinst = &pctx->strm_inst[handle];
	if (pinst->inst_handle < 0 ||
	    pinst->inst_handle >= AUCPU_MAX_INTST_NUM) {
		aucpu_pr(LOG_ERROR, "AUCPU handle is invalid\n");
		return AUCPU_DRVERR_INVALID_HANDLE;
	}
	if (pinst->inst_status == AUCPU_STA_NONE) {
		aucpu_pr(LOG_ERROR, "AUCPU strm handle is empty\n");
		return AUCPU_DRVERR_INVALID_HANDLE;
	}
	*inst = pinst;
	return AUCPU_NO_ERROR;
}

s32 aml_aucpu_strm_start(s32 handle)
{
	struct aucpu_ctx_t *pctx;
	struct aucpu_inst_t *pinst;
	struct aml_cmd_set *cmd;
	s32 res = AUCPU_NO_ERROR;

	pctx = s_aucpu_ctx;
	res = valid_handle2Inst(handle, pctx, &pinst);
	if (res < 0) {
		aucpu_pr(LOG_ERROR, "%s invalid_handle %d\n", __func__, handle);
		return res;
	}
	if (pinst->inst_status != AUCPU_STA_IDLE) {
		aucpu_pr(LOG_ERROR, "AUCPU strm %d is not idle\n", handle);
		return AUCPU_DRVERR_WRONG_STATE;
	}
	mutex_lock(&pctx->mutex);
	cmd = &pctx->last_cmd;
	cmd->cmd_type = CMD_INST_START | ((pinst->inst_handle & 0xff) << 8);
	res = send_aucpu_cmd(cmd);
	if (res >= 0) { // success
		pinst->inst_status = AUCPU_STA_RUNNING; // change status
		aucpu_pr(LOG_INFO, "Strm %d Started\n", pinst->inst_handle);
	}
	mutex_unlock(&pctx->mutex);
	return res;
}
EXPORT_SYMBOL(aml_aucpu_strm_start);

s32 aml_aucpu_strm_stop(s32 handle)
{
	struct aucpu_ctx_t *pctx;
	struct aucpu_inst_t *pinst;
	struct aml_cmd_set *cmd;
	s32 res = AUCPU_NO_ERROR;

	pctx = s_aucpu_ctx;
	res = valid_handle2Inst(handle, pctx, &pinst);
	if (res < 0) {
		aucpu_pr(LOG_ERROR, "%s invalid_handle %d\n", __func__, handle);
		return res;
	}
	if (pinst->inst_status != AUCPU_STA_RUNNING &&
	    pinst->inst_status != AUCPU_STA_FLUSHING) {
		aucpu_pr(LOG_ERROR, "AUCPU strm %d is not running\n", handle);
		return AUCPU_DRVERR_WRONG_STATE;
	}

	mutex_lock(&pctx->mutex);
	cmd = &pctx->last_cmd;
	cmd->cmd_type = CMD_INST_STOP | ((pinst->inst_handle & 0xff) << 8);
	res = send_aucpu_cmd(cmd);
	if (res >= 0) { // success
		pinst->inst_status = AUCPU_STA_IDLE; // change status
		aucpu_pr(LOG_INFO, "Strm %d Stopped\n", pinst->inst_handle);
	}
	mutex_unlock(&pctx->mutex);
	return res;
}
EXPORT_SYMBOL(aml_aucpu_strm_stop);

s32 aml_aucpu_strm_flush(s32 handle)
{
	struct aucpu_ctx_t *pctx;
	struct aucpu_inst_t *pinst;
	struct aml_cmd_set *cmd;
	s32 res = AUCPU_NO_ERROR;

	pctx = s_aucpu_ctx;
	res = valid_handle2Inst(handle, pctx, &pinst);
	if (res < 0) {
		aucpu_pr(LOG_ERROR, "%s invalid_handle %d\n", __func__, handle);
		return res;
	}

	if (pinst->inst_status != AUCPU_STA_RUNNING) {
		aucpu_pr(LOG_ERROR, "AUCPU strm %d is not running\n", handle);
		return AUCPU_DRVERR_WRONG_STATE;
	}
	mutex_lock(&pctx->mutex);
	cmd = &pctx->last_cmd;
	cmd->cmd_type = CMD_INST_FLUSH | ((pinst->inst_handle & 0xff) << 8);
	res = send_aucpu_cmd(cmd);
	if (res >= 0) { // success
		pinst->inst_status = AUCPU_STA_FLUSHING; // change status
		aucpu_pr(LOG_INFO, "Strm %d flushing\n", pinst->inst_handle);
	}
	mutex_unlock(&pctx->mutex);
	return res;
}
EXPORT_SYMBOL(aml_aucpu_strm_flush);

s32 aml_aucpu_strm_remove(s32 handle)
{
	struct aucpu_ctx_t *pctx;
	struct aucpu_inst_t *pinst;
	struct aml_cmd_set *cmd;
	s32 res = AUCPU_NO_ERROR;

	pctx = s_aucpu_ctx;
	res = valid_handle2Inst(handle, pctx, &pinst);
	if (res < 0) {
		aucpu_pr(LOG_ERROR, "%s invalid_handle %d\n", __func__, handle);
		return res;
	}

	if (pinst->inst_status != AUCPU_STA_IDLE) {
		aucpu_pr(LOG_ERROR, "AUCPU strm %d is not running\n", handle);
		return AUCPU_DRVERR_WRONG_STATE;
	}
	mutex_lock(&pctx->mutex);
	cmd = &pctx->last_cmd;
	cmd->cmd_type = CMD_INST_REMOVE | ((pinst->inst_handle & 0xff) << 8);
	res = send_aucpu_cmd(cmd);
	if (res >= 0) { // success
		pinst->inst_status = AUCPU_STA_NONE; // change status
		pctx->activated_inst_num--;
		aucpu_pr(LOG_INFO, "Strm %d removed\n", pinst->inst_handle);
	}
	mutex_unlock(&pctx->mutex);
	return res;
}
EXPORT_SYMBOL(aml_aucpu_strm_remove);

s32 aml_aucpu_strm_update_src(s32 handle, struct aml_aucpu_buf_upd *upd)
{
	struct aucpu_ctx_t *pctx;
	struct aucpu_inst_t *pinst;
	struct aml_aucpu_buf_upd *pdest;
	s32 res = AUCPU_NO_ERROR;

	pctx = s_aucpu_ctx;
	res = valid_handle2Inst(handle, pctx, &pinst);
	if (res < 0) {
		aucpu_pr(LOG_ERROR, "%s invalid_handle %d\n", __func__, handle);
		return res;
	}
	if (pinst->inst_status != AUCPU_STA_RUNNING) {
		aucpu_pr(LOG_ERROR, "AUCPU strm %d is not running\n", handle);
		return AUCPU_DRVERR_WRONG_STATE;
	}
	if ((pinst->config_options & 0x1) == 0) {
		aucpu_pr(LOG_ERROR, "AUCPU strm %d wrong config\n", handle);
		return AUCPU_DRVERR_VALID_CONTEXT;
	}
	pdest = (struct aml_aucpu_buf_upd *)pinst->in_upd_buf.base;
	pdest->phy_cur_ptr = upd->phy_cur_ptr;
	pdest->byte_cnt = upd->byte_cnt;
	aucpu_pr(LOG_INFO, "AUCPU strm %d update ptr 0x%lx byte_cnt %d\n",
		 handle, upd->phy_cur_ptr, upd->byte_cnt);
	/* need clean the  cache here to make sure data in DDR */
	dma_flush(pinst->in_upd_buf.phys_addr, pinst->in_upd_buf.size);
	return AUCPU_NO_ERROR;
}
EXPORT_SYMBOL(aml_aucpu_strm_update_src);

s32 aml_aucpu_strm_get_dst(s32 handle, struct aml_aucpu_buf_upd *upd)
{
	struct aucpu_ctx_t *pctx;
	struct aucpu_inst_t *pinst;
	struct aucpu_report_t *rpt;
	s32 res = AUCPU_NO_ERROR;

	pctx = s_aucpu_ctx;
	res = valid_handle2Inst(handle, pctx, &pinst);
	if (res < 0) {
		aucpu_pr(LOG_ERROR, "%s invalid_handle %d\n", __func__, handle);
		return res;
	}
	if (pinst->inst_status == AUCPU_STS_INST_IDLE) {
		upd->phy_cur_ptr = pinst->dst_wrptr;
		upd->byte_cnt = pinst->dst_byte_cnt;
		aucpu_pr(LOG_INFO, "AUCPU strm %d is not running\n", handle);
		return res;
	} else if (pinst->inst_status != AUCPU_STA_RUNNING &&
		   pinst->inst_status != AUCPU_STA_FLUSHING) {
		aucpu_pr(LOG_ERROR, "AUCPU strm %d is not running\n", handle);
		return AUCPU_DRVERR_WRONG_STATE;
	}

	cache_flush(pinst->reprt_buf.phys_addr, pinst->reprt_buf.size);
	rpt = (struct aucpu_report_t *)pinst->reprt_buf.base;
	pinst->work_state =  rpt->r_status;
	pinst->src_wrptr = rpt->r_src_wrptr;
	pinst->src_rdptr = rpt->r_src_rdptr;
	pinst->src_byte_cnt = rpt->r_src_byte_cnt;
	pinst->dst_wrptr = rpt->r_dst_wrptr;
	pinst->dst_byte_cnt = rpt->r_dst_byte_cnt;
	upd->phy_cur_ptr = pinst->dst_wrptr;
	upd->byte_cnt = pinst->dst_byte_cnt;
	aucpu_pr(LOG_INFO, "AUCPU strm %d cur in @0x%x %d, out @0x%x %d\n",
		 handle, pinst->src_rdptr, pinst->src_byte_cnt,
		 rpt->r_dst_wrptr, rpt->r_dst_byte_cnt);
	if (pinst->inst_status == AUCPU_STA_FLUSHING) {
		if (pinst->work_state == AUCPU_STS_INST_IDLE) {
			aucpu_pr(LOG_DEBUG, "AUCPU strm %d flush done\n",
				 handle);
			pinst->inst_status = AUCPU_STA_IDLE;
		}
	}
	return res;
}
EXPORT_SYMBOL(aml_aucpu_strm_get_dst);

s32 aml_aucpu_strm_get_status(s32 handle, s32 *state, s32 *report)
{
	struct aucpu_ctx_t *pctx;
	struct aucpu_inst_t *pinst;
	s32 res = AUCPU_NO_ERROR;

	pctx = s_aucpu_ctx;
	res = valid_handle2Inst(handle, pctx, &pinst);
	if (res < 0) {
		aucpu_pr(LOG_ERROR, "%s invalid_handle %d\n", __func__, handle);
		return AUCPU_DRVERR_VALID_CONTEXT;
	}
	*state = pinst->inst_status;
	*report = pinst->work_state;
	return res;
}
EXPORT_SYMBOL(aml_aucpu_strm_get_status);

s32 aml_aucpu_strm_get_load_firmware_status(void)
{
	return load_firmware_status;
}
EXPORT_SYMBOL(aml_aucpu_strm_get_load_firmware_status);

static void clear_aucpu_all_instances(void)
{
	struct aucpu_ctx_t *pctx;
	struct aucpu_inst_t *pinst;
	s32 res = AUCPU_NO_ERROR;
	struct aml_cmd_set *cmd;
	u32 handle_idx;

	pctx = s_aucpu_ctx;
	if (!pctx) {
		aucpu_pr(LOG_ERROR, "AUCPU driver did not init yet\n");
		return;
	}
	if (pctx->activated_inst_num == 0)
		aucpu_pr(LOG_ERROR, "AUCPU no active stream instances\n");

	mutex_lock(&pctx->mutex);
	cmd = &pctx->last_cmd;
	cmd->cmd_type = CMD_RESET_ALL;
	res = send_aucpu_cmd(cmd);
	if (res < 0) // failed
		aucpu_pr(LOG_ERROR, "aucpu reset all scmd failed\n");

	// clear the instance now
	for (handle_idx = 0; handle_idx < AUCPU_MAX_INTST_NUM; handle_idx++) {
		pinst = &pctx->strm_inst[handle_idx];
		if (pinst->inst_status != AUCPU_STA_NONE) {
			pinst->inst_status = AUCPU_STA_NONE;
			pctx->activated_inst_num--;
		}
		if (pctx->activated_inst_num == 0)
			break;
	}
	mutex_unlock(&pctx->mutex);
}

static s32 aucpu_open(struct inode *inode, struct file *filp)
{
	s32 r = 0;

	aucpu_pr(LOG_DEBUG, "[+] %s\n", __func__);
	filp->private_data = (void *)(s_aucpu_ctx);
	aucpu_pr(LOG_ERROR, "Do nothing for AUCPU driver open\n");
	aucpu_pr(LOG_DEBUG, "[-] %s, ret: %d\n", __func__, r);
	return r;
}

static long aucpu_ioctl(struct file *filp, u32 cmd, ulong arg)
{
	s32 ret = 0;
	struct aucpu_ctx_t *dev = (struct aucpu_ctx_t *)filp->private_data;

	aucpu_pr(LOG_ERROR, "IOCTL NO supported, dev %p ret: %d\n", dev, ret);
	return ret;
}

#ifdef CONFIG_COMPAT
static long aucpu_compat_ioctl(struct file *filp, u32 cmd, ulong arg)
{
	long ret;

	arg = (ulong)compat_ptr(arg);
	ret = aucpu_ioctl(filp, cmd, arg);
	return ret;
}
#endif

static s32 aucpu_release(struct inode *inode, struct file *filp)
{
	s32 ret = 0;

	aucpu_pr(LOG_DEBUG, "%s\n", __func__);
	aucpu_pr(LOG_ERROR, "NO thing to do now, ret: %d\n", ret);
	return ret;
}

static const struct file_operations aucpu_fops = {
	.owner = THIS_MODULE,
	.open = aucpu_open,
	.release = aucpu_release,
	.unlocked_ioctl = aucpu_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = aucpu_compat_ioctl,
#endif
};

static ssize_t aucpu_status_show(struct class *cla,
				 struct class_attribute *attr, char *buf)
{
	char *pbuf = buf;

	pbuf += snprintf(buf, 40, "\n Opened Aucpu instances:\n");
	pbuf += snprintf(buf, 80, "Total active %d\n",
			 s_aucpu_ctx->activated_inst_num);
	return pbuf - buf;
}

static CLASS_ATTR_RO(aucpu_status);

static struct attribute *aucpu_class_attrs[] = {
	&class_attr_aucpu_status.attr,
	NULL
};

ATTRIBUTE_GROUPS(aucpu_class);

static struct class aucpu_class = {
	.name = AUCPU_CLASS_NAME,
	.class_groups = aucpu_class_groups,
};

s32 init_Aucpu_device(void)
{
	s32  r = 0;

	r = register_chrdev(0, AUCPU_DEV_NAME, &aucpu_fops);
	if (r <= 0) {
		aucpu_pr(LOG_ERROR, "register aucpu device error.\n");
		return  -1;
	}
	s_aucpu_major = r;

	r = class_register(&aucpu_class);
	s_register_flag = 1;
	aucpu_dev = device_create(&aucpu_class, NULL, MKDEV(s_aucpu_major, 0),
				  NULL, AUCPU_DEV_NAME);

	if (IS_ERR(aucpu_dev)) {
		aucpu_pr(LOG_ERROR, "create aucpu device error.\n");
		class_unregister(&aucpu_class);
		return -1;
	}
	return 0;
}

s32 uninit_Aucpu_device(void)
{
	if (aucpu_dev)
		device_destroy(&aucpu_class, MKDEV(s_aucpu_major, 0));

	if (s_register_flag)
		class_destroy(&aucpu_class);
	s_register_flag = 0;

	if (s_aucpu_major)
		unregister_chrdev(s_aucpu_major, AUCPU_DEV_NAME);
	s_aucpu_major = 0;
	return 0;
}

static s32 aucpu_mem_device_init(struct reserved_mem *rmem,
	struct device *dev)
{
	s32 r;
	struct aucpu_ctx_t *pctx;

	if (!rmem) {
		aucpu_pr(LOG_ERROR, "Can not obtain I/O memory, ");
		aucpu_pr(LOG_ERROR, "will allocate multienc buffer!\n");
		r = -EFAULT;
		return r;
	}

	if (!rmem->base) {
		aucpu_pr(LOG_ERROR, " Aucpu NO reserved mem assgned\n");
		r = -EFAULT;
		return r;
	}
	if (!s_aucpu_ctx) {
		aucpu_pr(LOG_ERROR, "Aucpu did not allocate yet\n");
		r = -EFAULT;
		return r;
	}
	r = 0;
	pctx = s_aucpu_ctx;
	pctx->aucpu_mem.size = rmem->size;
	pctx->aucpu_mem.phys_addr = (ulong)rmem->base;
	pctx->aucpu_mem.base =
		(ulong)phys_to_virt(pctx->aucpu_mem.phys_addr);
	aucpu_pr(LOG_DEBUG, "aml_mem_device_init %d, 0x%lx\n",
		 pctx->aucpu_mem.size, pctx->aucpu_mem.phys_addr);

	return r;
}

static s32 aucpu_probe(struct platform_device *pdev)
{
	s32 err = 0, irq, reg_count, idx;
	struct resource res;
	struct device_node *np, *child;
	struct aucpu_ctx_t *pctx;

	aucpu_pr(LOG_DEBUG, "%s\n", __func__);

	s_aucpu_major = 0;
	use_reserve = false;
	s_aucpu_irq = -1;
	s_aucpu_irq_requested = false;
	s_aucpu_delaywork_requested = false;

	aucpu_dev = NULL;
	aucpu_pdev = NULL;
	s_register_flag = 0;
	s_aucpu_register_base = 0;
	s_aucpu_ctx = kzalloc(sizeof(*s_aucpu_ctx), GFP_KERNEL);
	if (!s_aucpu_ctx) {
		aucpu_pr(LOG_ERROR,
			 "failed to allocate aucpu memory context\n");
		return -ENOMEM;
	}

	memset(&res, 0, sizeof(struct resource));
	pctx = s_aucpu_ctx;

	idx = of_reserved_mem_device_init(&pdev->dev);
	if (idx != 0) {
		aucpu_pr(LOG_DEBUG,
			 "Aucpu reserved memory config fail.\n");
	} else if (pctx->aucpu_mem.phys_addr) {
		use_reserve = true;
	}
	/* get interrupt resource */
	irq = platform_get_irq_byname(pdev, "aucpu_irq");
	if (irq < 0) {
		aucpu_pr(LOG_ERROR, "get Aucpu irq resource error\n");
		err = -EFAULT;
		goto ERROR_PROVE_DEVICE;
	}
	s_aucpu_irq = irq;
	aucpu_pr(LOG_DEBUG, "Aucpu ->irq: %d\n", s_aucpu_irq);

	reg_count = 0;
	np = pdev->dev.of_node;
	for_each_child_of_node(np, child) {
		if (of_address_to_resource(child, 0, &res) ||
		    reg_count > 1) {
			aucpu_pr(LOG_ERROR,
				 "no reg ranges or more reg ranges %d\n",
				 reg_count);
			err = -ENXIO;
			goto ERROR_PROVE_DEVICE;
		}
		/* if platform driver is implemented */
		if (res.start != 0) {
			pctx->aucpu_reg.phys_addr = res.start;
			pctx->aucpu_reg.base =
				(ulong)ioremap_nocache(res.start,
				resource_size(&res));
			pctx->aucpu_reg.size = res.end - res.start;

			aucpu_pr(LOG_DEBUG,
				 "aucpu base address get from platfor");
			aucpu_pr(LOG_DEBUG,
				 " physical addr=0x%lx, base addr=0x%lx\n",
				 pctx->aucpu_reg.phys_addr,
				 pctx->aucpu_reg.base);
		} else {
			pctx->aucpu_reg.phys_addr = AUCPU_REG_BASE_ADDR;
			pctx->aucpu_reg.base =
				(ulong)ioremap_nocache(pctx->aucpu_reg.phys_addr,
				AUCPU_REG_SIZE);

			pctx->aucpu_reg.size = AUCPU_REG_SIZE;
			aucpu_pr(LOG_DEBUG,
				 "aucpu base address get from defined value ");
			aucpu_pr(LOG_DEBUG,
				 "physical addr=0x%lx, base addr=0x%lx\n",
				 pctx->aucpu_reg.phys_addr,
				 pctx->aucpu_reg.base);
		}
		s_aucpu_register_base = pctx->aucpu_reg.base;
		reg_count++;
	}
	/* get the major number of the character device */
	if (init_Aucpu_device()) {
		err = -EBUSY;
		aucpu_pr(LOG_ERROR, "could not allocate major number\n");
		goto ERROR_PROVE_DEVICE;
	}
	aucpu_pr(LOG_INFO, "SUCCESS alloc_chrdev_region\n");
	pctx->aucpu_device = aucpu_dev;

	/* init the local buffers */
	if (init_Aucpu_local_buf(&pdev->dev /*aucpu_dev */)) {
		err = -EBUSY;
		aucpu_pr(LOG_ERROR, "did not allocate enough memory\n");
		goto ERROR_PROVE_DEVICE;
	}
	/* load FW and start AUCPU */
	load_firmware_status = load_start_aucpu_fw(&pdev->dev /*aucpu_dev */);
	if (load_firmware_status) {
		aucpu_pr(LOG_ERROR, "load start_aucpu_fw fail\n");
		goto ERROR_PROVE_DEVICE;
	}
	aucpu_pr(LOG_INFO, "aucpu fw version: 0x%x\n",
			ReadAucpuRegister(VERSION_ADDR));

	mutex_init(&pctx->mutex);
	/*setup  the DEBUG buffer */
	setup_debug_polling();

	INIT_DELAYED_WORK(&pctx->dbg_work, do_polling_work);
	s_aucpu_delaywork_requested = true;

	err = request_irq(s_aucpu_irq, aucpu_irq_handler, 0,
			  "Aucpu-irq", (void *)s_aucpu_ctx);
	if (err) {
		aucpu_pr(LOG_ERROR, "Failed to register irq handler\n");
		goto ERROR_PROVE_DEVICE;
	}
	s_aucpu_irq_requested = true;
	schedule_delayed_work(&pctx->dbg_work, DBG_POLL_TIME);
	aucpu_pdev = pdev;
	aucpu_pr(LOG_ERROR, "Amlogic Aucpu driver installed\n");
	return 0;

ERROR_PROVE_DEVICE:
	if (s_aucpu_irq_requested) {
		free_irq(s_aucpu_irq, s_aucpu_ctx);
		s_aucpu_irq = -1;
		s_aucpu_irq_requested = false;
	}
	if (s_aucpu_delaywork_requested) {
		cancel_delayed_work_sync(&pctx->dbg_work);
		flush_scheduled_work();
		s_aucpu_delaywork_requested = false;
	}
	if (s_aucpu_register_base)
		iounmap((void *)s_aucpu_register_base);

	uninit_Aucpu_device();
	aucpu_dev = NULL;
	kfree(s_aucpu_ctx);
	s_aucpu_ctx = NULL;
	return err;
}

static s32 aucpu_remove(struct platform_device *pdev)
{
	aucpu_pr(LOG_DEBUG, "aupu_remove\n");

	clear_aucpu_all_instances(); //make sure aucpu is idle
	stop_aucpu_fw(&pdev->dev /*aucpu_dev */);

	if (s_aucpu_irq_requested) {
		free_irq(s_aucpu_irq, s_aucpu_ctx);
		s_aucpu_irq = -1;
		s_aucpu_irq_requested = false;
	}
	if (s_aucpu_delaywork_requested) {
		cancel_delayed_work_sync(&s_aucpu_ctx->dbg_work);
		flush_scheduled_work();
		s_aucpu_delaywork_requested = false;
	}

	uninit_Aucpu_local_buf(&pdev->dev);

	if (s_aucpu_register_base)
		iounmap((void *)s_aucpu_register_base);

	uninit_Aucpu_device();
	kfree(s_aucpu_ctx);
	s_aucpu_ctx = NULL;
	aucpu_dev = NULL;
	aucpu_pdev = NULL;
	aucpu_pr(LOG_DEBUG, "aupu_remove done\n");
	return 0;
}

static const struct of_device_id aml_aucpu_dt_match[] = {
	{
		.compatible = "amlogic, aucpu",
	},
	{},
};

static struct platform_driver aucpu_driver = {
	.driver = {
		.name = AUCPU_PLATFORM_DEVICE_NAME,
		.of_match_table = aml_aucpu_dt_match,
	},
	.probe = aucpu_probe,
	.remove = aucpu_remove,
};

static s32 __init aucpu_init(void)
{
	s32 res;

	aucpu_pr(LOG_DEBUG, "%s\n", __func__);

	res = platform_driver_register(&aucpu_driver);
	aucpu_pr(LOG_INFO, "end %s result=0x%x\n", __func__, res);
	return res;
}

static void __exit aucpu_exit(void)
{
	aucpu_pr(LOG_DEBUG, "%s\n", __func__);
	platform_driver_unregister(&aucpu_driver);
}

static const struct reserved_mem_ops rmem_aucpu_ops = {
	.device_init = aucpu_mem_device_init,
};

static s32 __init aucpu_mem_setup(struct reserved_mem *rmem)
{
	rmem->ops = &rmem_aucpu_ops;
	aucpu_pr(LOG_DEBUG, "Aucpu reserved mem setup.\n");
	return 0;
}

module_param(print_level, uint, 0664);
MODULE_PARM_DESC(print_level, "\n print_level\n");
module_param(dbg_level, uint, 0664);
MODULE_PARM_DESC(dbg_level, "\n dbg_level\n");

MODULE_AUTHOR("Amlogic Inc.");
MODULE_DESCRIPTION("AUCPU linux driver");
MODULE_LICENSE("GPL");

module_init(aucpu_init);
module_exit(aucpu_exit);
RESERVEDMEM_OF_DECLARE(aml_aucpu, "aml, Aucpu-mem", aucpu_mem_setup);
