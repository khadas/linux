// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/dma-mapping.h>
#include <linux/vmalloc.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/amlogic/tee.h>
#include <linux/fs.h>
#include <linux/syscalls.h>

//#define CHECK_PACKET_ALIGNM
#ifdef CHECK_PACKET_ALIGNM
#include <linux/highmem.h>
#endif

#include "mem_desc.h"
#include "sc2_control.h"
#include "../aml_dvb.h"
#include "../dmx_log.h"

#define MEM_DESC_SLOT_USED		1
#define MEM_DESC_SLOT_FREE		0

#define MEM_DESC_EOC_USED		1
#define MEM_DESC_EOC_FREE		0

#define R_MAX_MEM_CHAN_NUM			32
#define W_MAX_MEM_CHAN_NUM			128

static struct chan_id chan_id_table_w[W_MAX_MEM_CHAN_NUM];
static struct chan_id chan_id_table_r[R_MAX_MEM_CHAN_NUM];

#define dprint_i(fmt, args...)   \
	dprintk(LOG_ERROR, debug_mem_desc, fmt, ## args)
#define dprint(fmt, args...)     \
	dprintk(LOG_ERROR, debug_mem_desc, "mem_desc:" fmt, ## args)
#define pr_dbg(fmt, args...)     \
	dprintk(LOG_DBG, debug_mem_desc, "mem_desc:" fmt, ## args)

MODULE_PARM_DESC(debug_mem_desc, "\n\t\t Enable debug mem desc information");
static int debug_mem_desc;
module_param(debug_mem_desc, int, 0644);

MODULE_PARM_DESC(loop_desc, "\n\t\t Enable loop mem desc information");
static int loop_desc = 0x1;
module_param(loop_desc, int, 0644);

#define DEFAULT_RCH_SYNC_NUM	8
static int rch_sync_num_last = -1;

MODULE_PARM_DESC(rch_sync_num, "\n\t\t Enable loop mem desc information");
static int rch_sync_num = DEFAULT_RCH_SYNC_NUM;
module_param(rch_sync_num, int, 0644);

#define BEN_LEVEL_SIZE			(512 * 1024)

MODULE_PARM_DESC(dump_input_ts, "\n\t\t dump input ts packet");
static int dump_input_ts;
module_param(dump_input_ts, int, 0644);

static loff_t input_file_pos;
static struct file *input_dump_fp;

#define INPUT_DUMP_FILE   "/data/input_dump.ts"

static void dump_file_open(char *path)
{
	if (input_dump_fp)
		return;

	input_dump_fp = filp_open(path, O_CREAT | O_RDWR, 0666);
	if (IS_ERR(input_dump_fp)) {
		pr_err("create input dump [%s] file failed [%d]\n",
			path, (int)PTR_ERR(input_dump_fp));
		input_dump_fp = NULL;
	} else {
		dprint("create dump ts:%s success\n", path);
	}
}

static void dump_file_write(char *buf, size_t count)
{
	mm_segment_t old_fs;

	if (!input_dump_fp) {
		pr_err("Failed to write ts dump file fp is null\n");
		return;
	}

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	if (count != vfs_write(input_dump_fp, buf, count,
			&input_file_pos))
		pr_err("Failed to write video dump file\n");

	set_fs(old_fs);
}

static void dump_file_close(void)
{
	if (input_dump_fp) {
		vfs_fsync(input_dump_fp, 0);
		filp_close(input_dump_fp, current->files);
		input_dump_fp = NULL;
	}
}

int _alloc_buff(unsigned int len, int sec_level,
		unsigned long *vir_mem, unsigned long *phy_mem,
		unsigned int *handle)
{
	int flags = 0;
	int buf_page_num = 0;
	unsigned long buf_start;
	unsigned long buf_start_virt;
	u32 ret;

	if (len < BEN_LEVEL_SIZE)
		flags = CODEC_MM_FLAGS_DMA_CPU;
	else
		flags = CODEC_MM_FLAGS_DMA_CPU | CODEC_MM_FLAGS_FOR_VDECODER;

	buf_page_num = PAGE_ALIGN(len) / PAGE_SIZE;

	buf_start =
	    codec_mm_alloc_for_dma("dmx", buf_page_num, 4 + PAGE_SHIFT, flags);
	if (!buf_start) {
		dprint("%s fail\n", __func__);
		return -1;
	}
	if (sec_level) {
		//ret = tee_protect_tvp_mem(buf_start, len, handle);
		ret = tee_protect_mem_by_type(TEE_MEM_TYPE_DEMUX,
				buf_start, len, handle);
		pr_dbg("%s, protect 0x%lx, len:%d, ret:0x%x\n",
				__func__, buf_start, len, ret);
	}
	buf_start_virt = (unsigned long)codec_mm_phys_to_virt(buf_start);

	*vir_mem = buf_start_virt;
	*phy_mem = buf_start;

	return 0;
}

void _free_buff(unsigned long buf, unsigned int len, int sec_level,
		unsigned int handle)
{
	if (sec_level) {
		tee_unprotect_mem(handle);
		pr_dbg("%s, unprotect handle:%d\n", __func__, handle);
	}

	codec_mm_free_for_dma("dmx", buf);
}

static int _bufferid_malloc_desc_mem(struct chan_id *pchan,
				     unsigned int mem_size, int sec_level)
{
	unsigned long mem;
	unsigned long mem_phy;
	unsigned long memdescs;
	unsigned long memdescs_phy;
	int ret = 0;

	if ((mem_size >> 27) > 0) {
		dprint("%s fail, need support >=128M bytes\n", __func__);
		return -1;
	}
	if (pchan->mode == INPUT_MODE && sec_level != 0) {
		mem = 0;
		mem_size = 0;
		mem_phy = 0;
	} else {
		if (pchan->sec_size) {
			mem_size = pchan->sec_size;
			mem_phy = pchan->sec_mem;
			mem = mem_phy;
			pr_dbg("use sec mem:0x%lx, size:0x%x\n",
					mem_phy, mem_size);
		} else {
			ret = _alloc_buff(mem_size, sec_level, &mem,
				&mem_phy, &pchan->tee_handle);
			if (ret != 0) {
				dprint("%s malloc fail\n", __func__);
				return -1;
			}
			pr_dbg("%s malloc mem:0x%lx, mem_phy:0x%lx, sec:%d\n",
					__func__, mem, mem_phy, sec_level);
		}
	}
	ret =
	    _alloc_buff(sizeof(union mem_desc), 0, &memdescs, &memdescs_phy, 0);
	if (ret != 0) {
		_free_buff(mem_phy, mem_size, sec_level, pchan->tee_handle);
		dprint("%s malloc 2 fail\n", __func__);
		return -1;
	}
	pchan->mem = mem;
	pchan->mem_phy = mem_phy;
	pchan->mem_size = mem_size;
	pchan->sec_level = sec_level;
	pchan->memdescs = (union mem_desc *)memdescs;
	pchan->memdescs_phy = memdescs_phy;

	pchan->memdescs->bits.address = mem_phy;
	pchan->memdescs->bits.byte_length = mem_size;
	pchan->memdescs->bits.loop = loop_desc;
	pchan->memdescs->bits.eoc = 1;

	pchan->memdescs_map = dma_map_single(aml_get_device(),
					     (void *)pchan->memdescs,
					     sizeof(union mem_desc),
					     DMA_TO_DEVICE);
	pr_dbg("flush mem descs to ddr\n");
	pr_dbg("%s mem_desc phy addr 0x%x, memdsc:0x%lx\n", __func__,
	       pchan->memdescs_phy, (unsigned long)pchan->memdescs);

	pchan->r_offset = 0;
	pchan->last_w_addr = 0;
	return 0;
}

static void _bufferid_free_desc_mem(struct chan_id *pchan)
{
	if (pchan->mem && pchan->sec_size == 0)
		_free_buff((unsigned long)pchan->mem_phy,
			   pchan->mem_size, pchan->sec_level,
			   pchan->tee_handle);
	if (pchan->memdescs)
		_free_buff((unsigned long)pchan->memdescs_phy,
			   sizeof(union mem_desc), 0, 0);
	if (pchan->memdescs_map)
		dma_unmap_single(aml_get_device(), pchan->memdescs_map,
				 pchan->mem_size, DMA_TO_DEVICE);
	pchan->mem = 0;
	pchan->mem_phy = 0;
	pchan->mem_size = 0;
	pchan->sec_level = 0;
	pchan->memdescs = NULL;
	pchan->memdescs_phy = 0;
	pchan->memdescs_map = 0;
	pchan->sec_size = 0;
}

static int _bufferid_alloc_chan_w_for_es(struct chan_id **pchan,
					 struct chan_id **pchan1)
{
	int i = 0;

#define ES_BUFF_ID_END  63
	for (i = ES_BUFF_ID_END; i >= 0; i--) {
		struct chan_id *pchan_tmp = &chan_id_table_w[i];
		struct chan_id *pchan1_tmp = &chan_id_table_w[i + 64];

		if (pchan_tmp->used == 0 && pchan1_tmp->used == 0) {
			pchan_tmp->used = 1;
			pchan1_tmp->used = 1;
			pchan_tmp->is_es = 1;
			pchan1_tmp->is_es = 1;
			*pchan = pchan_tmp;
			*pchan1 = pchan1_tmp;
			break;
		}
	}
	if (i >= 0) {
		dprint("find bufferid %d & %d for es\n", i, i + 64);
		return 0;
	}
	dprint("can't find bufferid for es\n");
	return -1;
}

static int _bufferid_alloc_chan_w_for_ts(struct chan_id **pchan)
{
	int i = 0;
	struct chan_id *pchan_tmp;

	for (i = 0; i < W_MAX_MEM_CHAN_NUM; i++) {
		pchan_tmp = &chan_id_table_w[i];
		if (pchan_tmp->used == 0) {
			pchan_tmp->used = 1;
			break;
		}
	}
	if (i == W_MAX_MEM_CHAN_NUM)
		return -1;
	*pchan = pchan_tmp;
	return 0;
}

static int _bufferid_alloc_chan_r_for_ts(struct chan_id **pchan, u8 req_id)
{
	struct chan_id *pchan_tmp;

	if (req_id >= R_MAX_MEM_CHAN_NUM)
		return -1;
	pchan_tmp = &chan_id_table_r[req_id];
	if (pchan_tmp->used == 1)
		return -1;

	pchan_tmp->used = 1;
	*pchan = pchan_tmp;
	return 0;
}

#ifdef CHECK_PACKET_ALIGNM
static void check_packet_alignm(unsigned int start, unsigned int end)
{
	int n = 0;
	char *p = NULL;
	unsigned long reg;
	unsigned int detect_len = end - start;

	if (detect_len % 188 != 0) {
		dprint_i("len:%d not alignm\n", detect_len);
		return;
	}
	reg = round_down(start, 0x3);
	if (reg != start)
		dprint_i("mem addr not alignm 4\n");

	if (!pfn_valid(__phys_to_pfn(reg)))
		p = (void *)ioremap(reg, 0x4);
	else
		p = (void *)kmap(pfn_to_page(__phys_to_pfn(reg)));
	if (!p) {
		dprint_i("phy transfer to virt fail\n");
		return;
	}
	//detect packet alignm
	for (n = 0; n < detect_len / 188; n++) {
		if (p[n * 188] != 0x47) {
			dprint_i("packet not alignm at %d,header:0x%0x\n",
				n * 188, p[n * 188]);
			break;
		}
	}
	if (!pfn_valid(__phys_to_pfn(reg)))
		kunmap(pfn_to_page(__phys_to_pfn(reg)));
}
#endif

/**
 * chan init
 * \retval 0: success
 * \retval -1:fail.
 */
int SC2_bufferid_init(void)
{
	int i = 0;

	pr_dbg("%s enter\n", __func__);

	for (i = 0; i < W_MAX_MEM_CHAN_NUM; i++) {
		struct chan_id *pchan = &chan_id_table_w[i];

		memset(pchan, 0, sizeof(struct chan_id));
		pchan->id = i;
		pchan->mode = OUTPUT_MODE;
	}

	for (i = 0; i < R_MAX_MEM_CHAN_NUM; i++) {
		struct chan_id *pchan = &chan_id_table_r[i];

		memset(pchan, 0, sizeof(struct chan_id));
		pchan->id = i;
		pchan->mode = INPUT_MODE;
	}

	return 0;
}

/**
 * alloc chan
 * \param attr
 * \param pchan,if ts/es, return this channel
 * \param pchan1, if es, return this channel for pcr
 * \retval success: 0
 * \retval -1:fail.
 */
int SC2_bufferid_alloc(struct bufferid_attr *attr,
		       struct chan_id **pchan, struct chan_id **pchan1)
{
	pr_dbg("%s enter\n", __func__);

	if (attr->mode == INPUT_MODE)
		return _bufferid_alloc_chan_r_for_ts(pchan, attr->req_id);

	if (attr->is_es)
		return _bufferid_alloc_chan_w_for_es(pchan, pchan1);
	else
		return _bufferid_alloc_chan_w_for_ts(pchan);
}

/**
 * dealloc chan & free mem
 * \param pchan:struct chan_id handle
 * \retval success: 0
 * \retval -1:fail.
 */
int SC2_bufferid_dealloc(struct chan_id *pchan)
{
	pr_dbg("%s enter\n", __func__);
	if (pchan->mode == INPUT_MODE) {
		_bufferid_free_desc_mem(pchan);
		pchan->is_es = 0;
		pchan->used = 0;
		dump_file_close();
	} else {
		_bufferid_free_desc_mem(pchan);
		pchan->is_es = 0;
		pchan->used = 0;
	}
	return 0;
}

/**
 * chan mem
 * \param pchan:struct chan_id handle
 * \param mem_size:used memory
 * \param sec_level: memory security level
 * \retval success: 0
 * \retval -1:fail.
 */
int SC2_bufferid_set_mem(struct chan_id *pchan,
			 unsigned int mem_size, int sec_level)
{
	pr_dbg("%s mem_size:%d,sec_level:%d\n", __func__, mem_size, sec_level);
	_bufferid_malloc_desc_mem(pchan, mem_size, sec_level);
	return 0;
}

/**
 * chan mem
 * \param pchan:struct chan_id handle
 * \param sec_mem:direct memory
 * \param sec_size: memory size
 * \retval success: 0
 * \retval -1:fail.
 */
int SC2_bufferid_set_sec_mem(struct chan_id *pchan,
			 unsigned int sec_mem, unsigned int sec_size)
{
	pr_dbg("%s sec_mem:0x%x,sec_size:0x%0x\n", __func__, sec_mem, sec_size);
	pchan->sec_mem = sec_mem;
	pchan->sec_size = sec_size;
	return 0;
}

/**
 * set enable
 * \param pchan:struct chan_id handle
 * \param enable: 1/0
 * \retval success: 0
 * \retval -1:fail.
 */
int SC2_bufferid_set_enable(struct chan_id *pchan, int enable)
{
	unsigned int tmp;

	if (pchan->enable == enable)
		return 0;

	pr_dbg("%s id:%d enable:%d\n", __func__, pchan->id, enable);

	pchan->enable = enable;

	tmp = pchan->memdescs_phy & 0xFFFFFFFF;
	wdma_config_enable(pchan->id, enable, tmp, pchan->mem_size);

	pr_dbg("######wdma start###########\n");
	pr_dbg("err:0x%0x, active:%d\n", wdma_get_err(pchan->id),
	       wdma_get_active(pchan->id));
	pr_dbg("wptr:0x%0x\n", wdma_get_wptr(pchan->id));
	pr_dbg("wr_len:0x%0x\n", wdma_get_wr_len(pchan->id, NULL));
	pr_dbg("######wdma end###########\n");

	return 0;
}

/**
 * recv data
 * \param pchan:struct chan_id handle
 * \retval 0: no data
 * \retval 1: recv data, it can call SC2_bufferid_read
 */
int SC2_bufferid_recv_data(struct chan_id *pchan)
{
	unsigned int wr_offset = 0;

	if (!pchan)
		return 0;

	wr_offset = wdma_get_wr_len(pchan->id, NULL);
	if (wr_offset != pchan->r_offset)
		return 1;
	return 0;
}

unsigned int SC2_bufferid_get_free_size(struct chan_id *pchan)
{
	unsigned int now_w = 0;
	unsigned int r = pchan->r_offset;
	unsigned int mem_size = pchan->mem_size;
	unsigned int free_space = 0;

	now_w = wdma_get_wr_len(pchan->id, NULL) % pchan->mem_size;

	if (now_w >= r)
		free_space = mem_size - (now_w - r);
	else
		free_space = r - now_w;

	return free_space;
}

unsigned int SC2_bufferid_get_wp_offset(struct chan_id *pchan)
{
	return (wdma_get_wr_len(pchan->id, NULL) % pchan->mem_size);
}

/**
 * chan read
 * \param pchan:struct chan_id handle
 * \param pread:data addr
 * \param plen:data size addr
 * \param plen:is secure or not
 * \retval >=0:read cnt.
 * \retval -1:fail.
 */
int SC2_bufferid_read(struct chan_id *pchan, char **pread, unsigned int len,
		int is_secure)
{
	unsigned int w_offset = 0;
	unsigned int w_offset_org = 0;
	unsigned int buf_len = len;
	unsigned int data_len = 0;
	int overflow = 0;

	w_offset_org = wdma_get_wr_len(pchan->id, &overflow);
	w_offset = w_offset_org % pchan->mem_size;
//      if (is_buffer_overflow(pchan, w_offset))
//              dprint("chan buffer loop back\n");

	pchan->last_w_addr = w_offset;
	if (w_offset != pchan->r_offset) {
		pr_dbg("%s w:0x%0x, r:0x%0x, wr_len:0x%0x\n", __func__,
		       (u32)w_offset, (u32)(pchan->r_offset),
		       (u32)w_offset_org);
		if (w_offset > pchan->r_offset) {
			data_len = min((w_offset - pchan->r_offset), buf_len);
			if (!is_secure)
				dma_sync_single_for_cpu(aml_get_device(),
						(dma_addr_t)(pchan->mem_phy +
							      pchan->r_offset),
						data_len, DMA_FROM_DEVICE);
			*pread = (char *)(is_secure ?
					pchan->mem_phy + pchan->r_offset :
					pchan->mem + pchan->r_offset);
			pchan->r_offset += data_len;
		} else {
			unsigned int part1_len = 0;

			part1_len = pchan->mem_size - pchan->r_offset;
			if (part1_len == 0) {
				data_len = min(w_offset, buf_len);
				pchan->r_offset = 0;
				if (!is_secure)
					dma_sync_single_for_cpu(aml_get_device(),
							(dma_addr_t)
							(pchan->mem_phy +
							 pchan->r_offset),
							data_len,
							DMA_FROM_DEVICE);
				pchan->r_offset += data_len;
				return data_len;
			}
			data_len = min(part1_len, buf_len);
			*pread = (char *)(is_secure ?
					pchan->mem_phy + pchan->r_offset :
					pchan->mem + pchan->r_offset);
			if (data_len < part1_len) {
				if (!is_secure)
					dma_sync_single_for_cpu(aml_get_device(),
							(dma_addr_t)
							(pchan->mem_phy +
							 pchan->r_offset),
							data_len,
							DMA_FROM_DEVICE);
				pchan->r_offset += data_len;
			} else {
				data_len = part1_len;
				if (!is_secure)
					dma_sync_single_for_cpu(aml_get_device(),
							(dma_addr_t)
							(pchan->mem_phy +
							 pchan->r_offset),
							data_len,
							DMA_FROM_DEVICE);
				pchan->r_offset = 0;
			}
		}
		pr_dbg("%s request:%d, ret:%d\n", __func__, len, data_len);
		return data_len;
	}
	return 0;
}

/**
 * write to channel
 * \param pchan:struct chan_id handle
 * \param buf: data addr
 * \param  count: write size
 * \param  isphybuf: isphybuf
 * \retval -1:fail
 * \retval written size
 */
int SC2_bufferid_write(struct chan_id *pchan, const char __user *buf,
		       unsigned int count, int isphybuf)
{
	unsigned int r = count;
	unsigned int len;
	unsigned int ret;
	const char __user *p = buf;
	unsigned int tmp;
	unsigned int times = 0;
	dma_addr_t dma_addr = 0;
	dma_addr_t dma_desc_addr = 0;
	struct dmx_sec_ts_data ts_data;
//      dma_addr_t dma_desc_addr = 0;

	pr_dbg("%s start w:%d\n", __func__, r);
	do {
	} while (!rdma_get_ready(pchan->id) && times++ < 20);

	if (rch_sync_num != rch_sync_num_last) {
		rdma_config_sync_num(rch_sync_num);
		rch_sync_num_last = rch_sync_num;
	}

	times = 0;
	while (r) {
		if (isphybuf) {
			pchan->enable = 1;
			if (copy_from_user((char *)&ts_data,
				p, sizeof(struct dmx_sec_ts_data))) {
				dprint("copy_from user error\n");
				return -EFAULT;
			}
#ifdef CHECK_PACKET_ALIGNM
			check_packet_alignm(ts_data.buf_start, ts_data.buf_end);
#endif
			tmp = (unsigned long)ts_data.buf_start & 0xFFFFFFFF;
			pchan->memdescs->bits.address = tmp;
			pchan->memdescs->bits.byte_length =
				ts_data.buf_end - ts_data.buf_start;
			dma_desc_addr = dma_map_single(aml_get_device(),
						       (void *)pchan->memdescs,
						       sizeof(union mem_desc),
						       DMA_TO_DEVICE);

			tmp = (unsigned long)(pchan->memdescs) & 0xFFFFFFFF;
			len = pchan->memdescs->bits.byte_length;
			rdma_config_enable(pchan->id, 1, tmp, count, len);
			pr_dbg("%s isphybuf\n", __func__);
			/*it will exit write loop*/
			r = len;
		} else {
			if (r > pchan->mem_size)
				len = pchan->mem_size;
			else
				len = r;
			if (copy_from_user((char *)pchan->mem, p, len)) {
				dprint("copy_from user error\n");
				return -EFAULT;
			}
			if (dump_input_ts) {
				dump_file_open(INPUT_DUMP_FILE);
				dump_file_write((char *)pchan->mem, len);
			}
			dma_addr = dma_map_single(aml_get_device(),
						  (void *)pchan->mem,
						  pchan->mem_size,
						  DMA_TO_DEVICE);

			//set desc mem ==len for trigger data transfer.
			pchan->memdescs->bits.byte_length = len;
			dma_desc_addr = dma_map_single(aml_get_device(),
						       (void *)pchan->memdescs,
						       sizeof(union mem_desc),
						       DMA_TO_DEVICE);

			if (dma_mapping_error(aml_get_device(), dma_addr)) {
				dprint("mem dma_mapping error\n");
				dma_unmap_single(aml_get_device(),
						 dma_desc_addr,
						 sizeof(union mem_desc),
						 DMA_TO_DEVICE);
				return -EFAULT;
			}

			wmb();	/*Ensure pchan->mem contents visible */

			pr_dbg("%s, input data:0x%0x, des len:%d\n", __func__,
			       (*(char *)(pchan->mem)), len);
			pr_dbg("%s, desc data:0x%0x 0x%0x\n", __func__,
			       (*(unsigned int *)(pchan->memdescs)),
			       (*((unsigned int *)(pchan->memdescs) + 1)));

			pchan->enable = 1;
			tmp = pchan->memdescs_phy & 0xFFFFFFFF;
			rdma_config_enable(pchan->id, 1, tmp,
					   pchan->mem_size, len);
			dma_unmap_single(aml_get_device(),
					 dma_addr, pchan->mem_size,
					 DMA_TO_DEVICE);
		}

		do {
		} while (!rdma_get_done(pchan->id));

		dma_unmap_single(aml_get_device(),
				 dma_desc_addr, sizeof(union mem_desc),
				 DMA_TO_DEVICE);

		ret = rdma_get_rd_len(pchan->id);
		if (ret != len)
			dprint("%s, len not equal,ret:%d,w:%d\n",
			       __func__, ret, len);

		pr_dbg("#######rdma##########\n");
		pr_dbg("status:0x%0x\n", rdma_get_status(pchan->id));
		pr_dbg("err:0x%0x, len err:0x%0x, active:%d\n",
		       rdma_get_err(), rdma_get_len_err(), rdma_get_active());
		pr_dbg("pkt_sync:0x%0x\n", rdma_get_pkt_sync_status(pchan->id));
		pr_dbg("ptr:0x%0x\n", rdma_get_ptr(pchan->id));
		pr_dbg("cfg fifo:0x%0x\n", rdma_get_cfg_fifo());
		pr_dbg("#######rdma##########\n");

		/*disable */
		rdma_config_enable(pchan->id, 0, 0, 0, 0);
		rdma_clean(pchan->id);

		p += len;
		r -= len;
	}
	pr_dbg("%s end\n", __func__);
	rdma_config_ready(pchan->id);
	return count - r;
}
