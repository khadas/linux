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
#include <linux/delay.h>

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

MODULE_PARM_DESC(dmc_keep_alive, "\n\t\t Enable keep dmc alive");
static int dmc_keep_alive;
module_param(dmc_keep_alive, int, 0644);

MODULE_PARM_DESC(write_timeout_ms, "\n\t\t write timeout default 1s");
static int write_timeout_ms = 1000;
module_param(write_timeout_ms, int, 0644);

#define INPUT_DUMP_FILE   "/data/input_dump"

struct mem_cache {
	unsigned long start_virt;
	unsigned long start_phys;
	unsigned char elem_count;
	unsigned int elem_size;
	unsigned int used_count;
	char flag_arry[64];
};

#define FIRST_CACHE_ELEM_COUNT    64

#define SECOND_CACHE_ELEM_COUNT    48
#define SECOND_CACHE_ELEM_SIZE     (128 * 1024)

static int cache0_count_max = FIRST_CACHE_ELEM_COUNT;
static int cache1_count_max = SECOND_CACHE_ELEM_COUNT;

static struct mem_cache *first_cache;
static struct mem_cache *second_cache;

struct mem_region {
	u8 status;
	unsigned int start_phy;
	unsigned long start_virt;
	unsigned int len;
	struct mem_region *pnext;
};

struct dmc_range {
	u8 used;
	u8 level;
	unsigned int handle;
	unsigned int size;
	unsigned int buf_start_phy;
	unsigned int ref;
	unsigned int free_start_phy;
	unsigned int free_len;
	struct mem_region *region_list;
};

struct dmc_mem {
	int init;
	u8 level;
	unsigned int total_size;
	u8 range_num;
	struct dmc_range *range;
};

#define DMC_MEM_DEFAULT_SIZE (20 * 1024 * 1024)
static struct dmc_mem dmc_mem_level[7];

static int cache_init(int cache_level)
{
	int total_size = 0;
	int flags = 0;
	int buf_page_num = 0;

	if (cache_level == 0 && !first_cache) {
		first_cache = vmalloc(sizeof(*first_cache));
		if (!first_cache)
			return -1;

		memset(first_cache, 0, sizeof(struct mem_cache));
		first_cache->elem_size = sizeof(union mem_desc);
		first_cache->elem_count = cache0_count_max;
		total_size = sizeof(union mem_desc) * cache0_count_max;
		first_cache->start_virt = (unsigned long)dma_alloc_coherent(aml_get_device(),
			total_size, (dma_addr_t *)&first_cache->start_phys,
			GFP_KERNEL | GFP_DMA32);
		if (!first_cache->start_virt) {
			vfree(first_cache);
			first_cache = NULL;
			dprint("%s first cache fail\n", __func__);
			return -1;
		}
	} else if (cache_level == 1 && !second_cache) {
		second_cache = vmalloc(sizeof(*second_cache));
		if (!second_cache)
			return -1;

		memset(second_cache, 0, sizeof(struct mem_cache));
		second_cache->elem_size = SECOND_CACHE_ELEM_SIZE;
		second_cache->elem_count = cache1_count_max;
		total_size = cache1_count_max * SECOND_CACHE_ELEM_SIZE;

		flags = CODEC_MM_FLAGS_DMA_CPU;
		buf_page_num = PAGE_ALIGN(total_size) / PAGE_SIZE;

		second_cache->start_phys =
		    codec_mm_alloc_for_dma("dmx_cache", buf_page_num,
					4 + PAGE_SHIFT, flags);
		if (!second_cache->start_phys) {
			vfree(second_cache);
			second_cache = NULL;
			dprint("%s second cache fail\n", __func__);
			return -1;
		}
		second_cache->start_virt =
		(unsigned long)codec_mm_phys_to_virt(second_cache->start_phys);
		if (IS_ERR_OR_NULL((const void *)second_cache->start_phys)) {
			codec_mm_free_for_dma("dmx", second_cache->start_phys);
			vfree(second_cache);
			dprint("phys to virt addr failed\n");
			return -1;
		}
	}
	return 0;
}

static void cache_destroy(int cache_level)
{
	if (cache_level == 0 && first_cache) {
		dma_free_coherent(aml_get_device(),
			first_cache->elem_size * first_cache->elem_count,
			(void *)first_cache->start_virt,
			first_cache->start_phys);
		vfree(first_cache);
		first_cache = NULL;
		dprint_i("clear first cache done\n");
	} else if (cache_level == 1 && second_cache) {
		codec_mm_free_for_dma("dmx_cache", second_cache->start_phys);
		vfree(second_cache);
		second_cache = NULL;
		dprint_i("clear second cache done\n");
	}
}

static int cache_get_block(struct mem_cache *cache,
	unsigned long *p_virt, unsigned long *p_phys)
{
	int i = 0;

	for (i = 0; i < cache->elem_count; i++) {
		if (cache->flag_arry[i] == 0)
			break;
	}

	if (i == cache->elem_count) {
		dprint_i("dmx cache full\n");
		return -1;
	}
	if (p_virt)
		*p_virt = cache->start_virt + i * cache->elem_size;
	if (p_phys)
		*p_phys = cache->start_phys + i * cache->elem_size;

	cache->flag_arry[i] = 1;
	cache->used_count++;
	return 0;
}

static int cache_free_block(struct mem_cache *cache, unsigned long phys_mem)
{
	int i = 0;

	if (phys_mem >= cache->start_phys &&
		phys_mem <= cache->start_phys +
			(cache->elem_count - 1) * cache->elem_size) {
		for (i = 0; i < cache->elem_count; i++) {
			if (phys_mem ==
					cache->start_phys +
					i * cache->elem_size) {
				cache->flag_arry[i] = 0;
				cache->used_count--;
				return 0;
			}
		}
	}
	return -1;
}

static int cache_malloc(int len, unsigned long *p_virt, unsigned long *p_phys)
{
//	dprint("%s, len:%d\n", __func__, len);
	if (!first_cache) {
		if (cache_init(0) != 0)
			return -1;
	}
	if (!second_cache) {
		if (cache_init(1) != 0)
			return -1;
	}

	if (len <= first_cache->elem_size)
		return cache_get_block(first_cache, p_virt, p_phys);
	else if (len <= second_cache->elem_size)
		return cache_get_block(second_cache, p_virt, p_phys);

	return -1;
}

static int cache_free(int len, unsigned long phys_mem)
{
	int iret  = -1;

	if (first_cache && len <= first_cache->elem_size)
		iret = cache_free_block(first_cache, phys_mem);
	else if (second_cache && len <= second_cache->elem_size)
		iret = cache_free_block(second_cache, phys_mem);

	return iret;
}

int cache_clear(void)
{
	if (first_cache && first_cache->used_count == 0)
		cache_destroy(0);
	if (second_cache && second_cache->used_count == 0)
		cache_destroy(1);
	return 0;
}

int cache_adjust(int cache0_count, int cache1_count)
{
	if (cache0_count > 64 || cache1_count > 64) {
		dprint_i("cache count can't bigger than 64\n");
		return -1;
	}
	dprint_i("cache0 count:%d, cache1 count:%d\n",
		cache0_count, cache1_count);
	cache0_count_max = cache0_count;
	cache1_count_max = cache1_count;

	if (first_cache && first_cache->used_count == 0)
		cache_destroy(0);
	if (second_cache && second_cache->used_count == 0)
		cache_destroy(1);

	cache_init(0);
	cache_init(1);
	return 0;
}

static void dump_file_open(char *path, struct dump_input_file *dump_file_fp,
	int sid)
{
	int i = 0;
	char whole_path[255];
	struct file *file_fp;

	if (dump_file_fp->file_fp)
		return;

	//find new file name
	while (i < 999) {
		snprintf((char *)&whole_path, sizeof(whole_path),
		"%s_0x%0x_%03d.ts", path, sid, i);

		file_fp = filp_open(whole_path, O_RDONLY, 0666);
		if (IS_ERR(file_fp))
			break;
		filp_close(file_fp, current->files);
		i++;
	}
	dump_file_fp->file_fp = filp_open(whole_path,
		O_CREAT | O_RDWR | O_APPEND, 0666);
	if (IS_ERR(dump_file_fp->file_fp)) {
		pr_err("create video dump [%s] file failed [%d]\n",
			whole_path, (int)PTR_ERR(dump_file_fp->file_fp));
		dump_file_fp->file_fp = NULL;
	} else {
		dprint("create dump [%s] success\n", whole_path);
	}
}

static void dump_file_write(char *buf,
		size_t count, struct dump_input_file *dump_file_fp)
{
	mm_segment_t old_fs;

	if (!dump_file_fp->file_fp) {
		pr_err("Failed to write video dump file fp is null\n");
		return;
	}
	if (count == 0)
		return;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	if (count != vfs_write(dump_file_fp->file_fp, buf, count,
			&dump_file_fp->file_pos))
		pr_err("Failed to write dump file\n");

	set_fs(old_fs);
}

static void dump_file_close(struct dump_input_file *dump_file_fp)
{
	if (dump_file_fp->file_fp) {
		vfs_fsync(dump_file_fp->file_fp, 0);
		filp_close(dump_file_fp->file_fp, current->files);
		dump_file_fp->file_fp = NULL;
	}
}

int cache_status_info(char *buf)
{
	int r, total = 0;

	if (first_cache) {
		r = sprintf(buf, "first cache:\n");
		buf += r;
		total += r;

		r = sprintf(buf, "total size:%d, block count:%d, ",
		    first_cache->elem_count * first_cache->elem_size,
			first_cache->elem_count);
		buf += r;
		total += r;

		r = sprintf(buf, "block size:%d, used count:%d\n",
			first_cache->elem_size,
			first_cache->used_count);
		buf += r;
		total += r;
	} else {
		r = sprintf(buf, "first cache:no\n");
		buf += r;
		total += r;
	}

	if (second_cache) {
		r = sprintf(buf, "second cache:\n");
		buf += r;
		total += r;

		r = sprintf(buf, "total size:%d, block count:%d, ",
		    second_cache->elem_count * second_cache->elem_size,
			second_cache->elem_count);
		buf += r;
		total += r;

		r = sprintf(buf, "block size:%d, used count:%d\n",
			second_cache->elem_size,
			second_cache->used_count);
		buf += r;
		total += r;
	} else {
		r = sprintf(buf, "second cache:no\n");
		buf += r;
		total += r;
	}
	return total;
}

static int dmc_range_init(struct dmc_range *range, int sec_level)
{
	int flags = 0;
	int buf_page_num = 0;
	unsigned long buf_start = 0;
	unsigned long buf_start_virt = 0;
	u32 ret = -1;
	u32 len = range->size;

	flags = CODEC_MM_FLAGS_DMA_CPU | CODEC_MM_FLAGS_FOR_VDECODER;

	buf_page_num = PAGE_ALIGN(len) / PAGE_SIZE;

	buf_start =
	    codec_mm_alloc_for_dma("dmx", buf_page_num, 4 + PAGE_SHIFT, flags);
	if (!buf_start) {
		dprint("%s fail\n", __func__);
		return -1;
	}
	buf_start_virt = (unsigned long)codec_mm_phys_to_virt(buf_start);
	if (IS_ERR_OR_NULL((const void *)buf_start_virt)) {
		codec_mm_free_for_dma("dmx", buf_start);
		dprint("phys to virt addr failed\n");
		return -1;
	}
	pr_dbg("dmc mem init phy:0x%lx, virt:0x%lx, len:%d\n",
		buf_start, buf_start_virt, len);
	memset((char *)buf_start_virt, 0, len);
	codec_mm_dma_flush((void *)buf_start_virt, len, DMA_TO_DEVICE);
	range->level = sec_level;
	if (sec_level) {
		sec_level = sec_level == 1 ? 0 : sec_level;
		ret = tee_protect_mem(TEE_MEM_TYPE_DEMUX,
			sec_level, buf_start, len, &range->handle);
		dprint("%s, protect 0x%lx, len:%d, ret:0x%x\n",
				__func__, buf_start, len, ret);
	}
	range->buf_start_phy = buf_start;
	range->free_start_phy = buf_start;
	range->free_len = len;
	range->ref = 0;
	range->region_list = NULL;
	range->used = 1;
	return 0;
}

static int dmc_range_destroy(struct dmc_range *range)
{
	struct mem_region *header = NULL;
	struct mem_region *tmp = NULL;

	if (!range->used)
		return 0;
	tee_unprotect_mem(range->handle);
	codec_mm_free_for_dma("dmx", range->buf_start_phy);
	range->handle = 0;
	range->buf_start_phy = 0;

	header = range->region_list;
	while (header) {
		tmp = header->pnext;
		if (header->status == 1)
			dprint("%s used mem have free error\n", __func__);
		vfree(header);
		header = tmp;
	}

	range->region_list = NULL;
//	mem->size = 0;
	range->free_len = 0;
	range->free_start_phy = 0;
	range->used = 0;
	return 0;
}

static int dmc_range_get_block(struct dmc_range *range, unsigned int len,
	unsigned long *p_virt, unsigned long *p_phys)
{
	struct mem_region *header = range->region_list;
	struct mem_region *temp = NULL;

	while (header) {
		if (header->len == len &&
			header->status == 0) {
			header->status = 1;
			*p_virt = header->start_virt;
			*p_phys = header->start_phy;
			range->ref++;
			return 0;
		}
		header = header->pnext;
	}

	if (range->free_len < len)
		return -1;

	temp = vmalloc(sizeof(*temp));
	if (!temp) {
		dprint("%s err vmalloc\n", __func__);
		return -1;
	}
	temp->len = len;
	temp->start_phy = range->free_start_phy;
	temp->start_virt = (unsigned long)codec_mm_phys_to_virt(temp->start_phy);
	if (!temp->start_virt) {
		vfree(temp);
		dprint("%s codec_mm_phys_to_virt fail\n", __func__);
		return -1;
	}
	temp->status = 1;
	temp->pnext = range->region_list;

	range->region_list = temp;
	range->free_len -= len;
	range->free_start_phy += len;
	range->ref++;

	*p_virt = temp->start_virt;
	*p_phys = temp->start_phy;
	return 0;
}

static int dmc_range_free_block(struct dmc_range *range, unsigned long phys_mem)
{
	struct mem_region *header = range->region_list;

	while (header) {
		if (header->start_phy == phys_mem &&
			header->status == 1) {
			header->status = 0;
			range->ref--;
			return 0;
		}
		header = header->pnext;
	}
	dprint("%s no find block\n", __func__);
	return -1;
}

static int dmc_mem_init(struct dmc_mem *mem, int sec_level)
{
	struct dmc_range *temp = NULL;
	int i = 0;
	int num = 0;

	mem->range_num = 2;
	temp = vmalloc(sizeof(*temp) * mem->range_num);
	if (!temp) {
		dprint("%s err vmalloc\n", __func__);
		return -1;
	}
	memset(temp, 0, sizeof(*temp) * mem->range_num);

	num = mem->total_size / DMC_MEM_DEFAULT_SIZE;
	if (num >= mem->range_num || num >= 3) {
		for (i = 0; i < mem->range_num; i++)
			temp[i].size = mem->total_size / mem->range_num;
	} else {
		if (num == 1) {
			temp[0].size = mem->total_size;
		} else {
			temp[0].size = mem->total_size - DMC_MEM_DEFAULT_SIZE;
			temp[1].size = DMC_MEM_DEFAULT_SIZE;
		}
	}
	mem->range = temp;
	mem->level = sec_level;
	mem->init = 1;
	return 0;
}

static int dmc_mem_destroy(struct dmc_mem *mem)
{
	int i = 0;
	int not_free = 0;

	for (i = 0; i < mem->range_num; i++) {
		if (mem->range[i].used) {
			if (mem->range[i].ref == 0)
				dmc_range_destroy(&mem->range[i]);
			else
				not_free = 1;
		}
	}
	if (not_free)
		return 0;
	vfree(mem->range);
	mem->range = NULL;
	mem->init = 0;
	return 0;
}

static int dmc_mem_get_block(struct dmc_mem *mem, unsigned int len,
	unsigned long *p_virt, unsigned long *p_phys)
{
	int i = 0;

	for (i = 0; i < mem->range_num; i++) {
		if (!mem->range[i].used && mem->range[i].size != 0)
			if (dmc_range_init(&mem->range[i], mem->level) != 0)
				return -1;
		if (dmc_range_get_block(&mem->range[i], len, p_virt, p_phys) == 0)
			return 0;
	}
	return -1;
}

static int dmc_mem_free_block(struct dmc_mem *mem, unsigned long phys_mem)
{
	int i = 0;

	for (i = 0; i < mem->range_num; i++) {
		if (mem->range[i].used && mem->range[i].size != 0) {
			if (dmc_range_free_block(&mem->range[i], phys_mem) == 0) {
				if (!mem->range[i].ref && !dmc_keep_alive)
					dmc_mem_destroy(mem);
				return 0;
			}
		}
	}
	return -1;
}

static int dmc_mem_malloc(int sec_level, int len,
	unsigned long *p_virt, unsigned long *p_phys)
{
	int dmc_index = sec_level - 1;
	int ret = 0;

	if (dmc_index < 0 || dmc_index >= 7) {
		dprint("%s err level:%d\n", __func__, sec_level);
		return -1;
	}

	if (!dmc_mem_level[dmc_index].init) {
		ret = dmc_mem_init(&dmc_mem_level[dmc_index], sec_level);
		if (ret != 0)
			return -1;
	}
	return dmc_mem_get_block(&dmc_mem_level[dmc_index], len, p_virt, p_phys);
}

static int dmc_mem_free(unsigned long buf, unsigned int len, int sec_level)
{
	int dmc_index = sec_level - 1;
	int ret = 0;

	if (dmc_index < 0 || dmc_index >= 7) {
		dprint("%s err level:%d\n", __func__, sec_level);
		return -1;
	}
	if (!dmc_mem_level[dmc_index].init) {
		dprint("%s err no init\n", __func__);
		return -1;
	}
	ret = dmc_mem_free_block(&dmc_mem_level[dmc_index], buf);
	if (ret != 0) {
		dprint("err: can't free dmc mem buf:0x%lx", buf);
		return -1;
	}
	return ret;
}

int dmc_mem_set_size(int sec_level, unsigned int mem_size)
{
	int dmc_index = sec_level - 1;

	if (dmc_index < 0 || dmc_index >= 7) {
		dprint("%s err level:%d\n", __func__, sec_level);
		return -1;
	}
	if (!dmc_mem_level[dmc_index].init)
		dmc_mem_level[dmc_index].total_size = mem_size / (64 * 1024) * (64 * 1024);
	else
		dprint("%s should set size before app\n", __func__);
	return 0;
}

static int dmc_mem_dump(char *buf, struct dmc_mem *mem)
{
	int i = 0;
	int r, total = 0;
	int n = 0;

	if (mem->init) {
		r = sprintf(buf, "%d status: using level:%d total size:%d\n",
			i,
		mem->level,
		mem->total_size);
		buf += r;
		total += r;
		if (!mem->range)
			return total;

		for (n = 0; n < mem->range_num; n++) {
			r = sprintf(buf, " %d status:%d size 0x%0x ref:%d free:0x%0x\n",
				n + 1,
			mem->range[n].used,
			mem->range[n].size,
			mem->range[n].ref,
			mem->range[n].free_len);
			buf += r;
			total += r;
		}
	} else {
		r = sprintf(buf, "%d status: no use level:%d total size:%d\n",
			i,
		mem->level,
		mem->total_size);
		buf += r;
		total += r;
	}
	return total;
}

int dmc_mem_dump_info(char *buf)
{
	int i = 0;
	int r, total = 0;
	struct dmc_mem *mem;

	r = sprintf(buf, "********dmc mem********\n");
	buf += r;
	total += r;

	for (i = 0; i < 7; i++) {
		mem = &dmc_mem_level[i];
		r = dmc_mem_dump(buf, mem);
		buf += r;
		total += r;
	}
	return total;
}

int _alloc_buff(unsigned int len, int sec_level,
		unsigned long *vir_mem, unsigned long *phy_mem)
{
	int flags = 0;
	int buf_page_num = 0;
	unsigned long buf_start = 0;
	unsigned long buf_start_virt = 0;
	int iret = 0;

	if (sec_level) {
		iret = dmc_mem_malloc(sec_level, len, &buf_start_virt, &buf_start);
		if (iret == 0) {
			*vir_mem = buf_start_virt;
			*phy_mem = buf_start;
			return 0;
		}
		dprint("err:can't get mem len 0x%0x\n", len);
		return -1;
	}

	iret = cache_malloc(len, &buf_start_virt, &buf_start);
	if (iret == 0) {
		if (buf_start >= 0xffffffff)
			dprint_i("error cache phy:0x%lx, virt:0x%lx, len:%d\n",
				buf_start, buf_start_virt, len);
		else
			pr_dbg("init cache phy 11:0x%lx, virt:0x%lx, len:%d\n",
				buf_start, buf_start_virt, len);
		memset((char *)buf_start_virt, 0xa5, len);
		*vir_mem = buf_start_virt;
		*phy_mem = buf_start;
		return 0;
	}

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
	buf_start_virt = (unsigned long)codec_mm_phys_to_virt(buf_start);
	if (IS_ERR_OR_NULL((const void *)buf_start_virt)) {
		codec_mm_free_for_dma("dmx", buf_start);
		dprint("phys to virt addr failed\n");
		return -1;
	}
	pr_dbg("init phy:0x%lx, virt:0x%lx, len:%d\n",
			buf_start, buf_start_virt, len);
	memset((char *)buf_start_virt, 0xa5, len);

	*vir_mem = buf_start_virt;
	*phy_mem = buf_start;

	return 0;
}

void _free_buff(unsigned long buf, unsigned int len, int sec_level)
{
	int iret = 0;

	if (sec_level) {
		dmc_mem_free(buf, len, sec_level);
		return;
	}

	iret = cache_free(len, buf);
	if (iret == 0)
		return;

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
			mem = (unsigned long)phys_to_virt(mem_phy);
			pr_dbg("use sec phy mem:0x%lx, virt:0x%lx size:0x%x\n",
					mem_phy, mem, mem_size);
		} else {
			ret = _alloc_buff(mem_size, sec_level, &mem,
				&mem_phy);
			if (ret != 0) {
				dprint("%s malloc fail\n", __func__);
				return -1;
			}
			pr_dbg("%s malloc mem:0x%lx, mem_phy:0x%lx, sec:%d\n",
					__func__, mem, mem_phy, sec_level);
		}
	}
	ret =
	    _alloc_buff(sizeof(union mem_desc), 0, &memdescs, &memdescs_phy);
	if (ret != 0) {
		_free_buff(mem_phy, mem_size, sec_level);
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

	dma_sync_single_for_device(aml_get_device(),
		pchan->memdescs_phy, sizeof(union mem_desc), DMA_TO_DEVICE);
	pr_dbg("flush mem descs to ddr\n");
	pr_dbg("%s mem_desc phy addr 0x%x, memdescs:0x%lx\n", __func__,
	       pchan->memdescs_phy, (unsigned long)pchan->memdescs);

	/*mem from secure os, don't operate this memory*/
	if (pchan->sec_size == 0)
		dma_sync_single_for_device(aml_get_device(),
			mem_phy, mem_size, DMA_TO_DEVICE);

	pchan->r_offset = 0;
	pchan->last_w_addr = 0;
	return 0;
}

static void _bufferid_free_desc_mem(struct chan_id *pchan)
{
	if (pchan->mem && pchan->sec_size == 0)
		_free_buff((unsigned long)pchan->mem_phy,
			   pchan->mem_size, pchan->sec_level);
	if (pchan->memdescs)
		_free_buff((unsigned long)pchan->memdescs_phy,
			   sizeof(union mem_desc), 0);
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
static void check_packet_alignm(unsigned int start, unsigned int end, int pack_len)
{
	int n = 0;
	char *p = NULL;
	unsigned long reg;
	unsigned int detect_len = end - start;

	if (detect_len % pack_len != 0) {
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
	for (n = 0; n < detect_len / pack_len; n++) {
		if (p[n * pack_len] != 0x47) {
			dprint_i("packet not alignm at %d,header:0x%0x\n",
				n * pack_len, p[n * pack_len]);
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

	for (i = 1; i <= 7; i++)
		dmc_mem_set_size(i, DMC_MEM_DEFAULT_SIZE);

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
	_bufferid_free_desc_mem(pchan);
	pchan->is_es = 0;
	pchan->used = 0;
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
	return _bufferid_malloc_desc_mem(pchan, mem_size, sec_level);
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
int SC2_bufferid_set_enable(struct chan_id *pchan, int enable, int sid, int pid)
{
	unsigned int tmp;

	if (pchan->enable == enable)
		return 0;

	pr_dbg("%s id:%d enable:%d\n", __func__, pchan->id, enable);

	pchan->enable = enable;

	tmp = pchan->memdescs_phy & 0xFFFFFFFF;
	pr_dbg("WCH_ADDR, buffer id:%d, desc_phy:0x%x, addr:0x%x, length:%d\n",
			pchan->id, tmp,
			pchan->memdescs->bits.address,
			pchan->memdescs->bits.byte_length);
	//wdma_config_enable(pchan->id, enable, tmp, pchan->mem_size);
	wdma_config_enable(pchan, enable, tmp, pchan->mem_size,
		sid, pid, pchan->sec_level);

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

unsigned int SC2_bufferid_get_data_len(struct chan_id *pchan)
{
	unsigned int w_offset = 0;

	w_offset = wdma_get_wr_len(pchan->id, NULL) % pchan->mem_size;
	if (w_offset >= pchan->r_offset)
		return w_offset - pchan->r_offset;
	else
		return pchan->mem_size - pchan->r_offset + w_offset;
}

int SC2_bufferid_read_header_again(struct chan_id *pchan, char **pread)
{
	int offset = pchan->r_offset;

	usleep_range(40, 50);
	offset = pchan->r_offset ? (pchan->r_offset - 0x10) : (pchan->mem_size - 0x10);
	dma_sync_single_for_cpu(aml_get_device(),
		(dma_addr_t)((pchan->mem_phy + offset) / 64 * 64), 64, DMA_FROM_DEVICE);

	*pread = (char *)(pchan->mem + offset);
	return 0;
}

int SC2_bufferid_read_newest_pts(struct chan_id *pchan, char **pread)
{
	unsigned int w_offset = 0;
	unsigned int w_offset_org = 0;
	unsigned int buf_len = 16;
	int overflow = 0;
	int pts_mem_offset = 0;

	if (!pchan || !pchan->mem_phy) {
		dprint("pchan is null or pchan->mem_phy is 0\n");
		return 0;
	}

	w_offset_org = wdma_get_wr_len(pchan->id, &overflow);
	w_offset = w_offset_org % pchan->mem_size;
	w_offset = w_offset / buf_len * buf_len;

	if (w_offset != pchan->pts_newest_r_offset) {
//		dprint("%s w:0x%0x, r:0x%0x, wr_len:0x%0x\n", __func__,
//		       (u32)w_offset, (u32)(pchan->pts_newest_r_offset),
//		       (u32)w_offset_org);
		if (w_offset == 0)
			pts_mem_offset = pchan->mem_size - buf_len;
		else
			pts_mem_offset = w_offset - buf_len;
		/*get the second to last es header, not the last es header*/
		if (pts_mem_offset == 0)
			pts_mem_offset = pchan->mem_size - buf_len;
		else
			pts_mem_offset = pts_mem_offset - buf_len;

		dma_sync_single_for_cpu(aml_get_device(),
				(dma_addr_t)(pchan->mem_phy + pts_mem_offset),
				buf_len, DMA_FROM_DEVICE);
		*pread = (char *)pchan->mem + pts_mem_offset;
		pchan->pts_newest_r_offset = w_offset;
		return buf_len;
	}
	return 0;
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
		usleep_range(20, 30);
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
 * SC2_bufferid_move_read_rp
 * \param pchan:struct chan_id handle
 * \param len:length
 * \param flag: 0:rewind; 1:forward
 * \retval 0:success.
 * \retval -1:fail.
 */
int SC2_bufferid_move_read_rp(struct chan_id *pchan, unsigned int len, int flag)
{
	if (flag == 0)
		pchan->r_offset = (pchan->r_offset + pchan->mem_size - len) % pchan->mem_size;
	else
		pchan->r_offset = (pchan->r_offset + len) % pchan->mem_size;
	return 0;
}

/**
 * write to channel
 * \param pchan:struct chan_id handle
 * \param buf: data addr
 * \param buf_phys: data phys addr
 * \param  count: write size
 * \param  isphybuf: isphybuf
 * \param  pack_len: 188 or 192
 * \retval -1:fail
 * \retval written size
 */
int SC2_bufferid_write(struct chan_id *pchan, const char *buf, char *buf_phys,
		       unsigned int count, int isphybuf, int pack_len)
{
	unsigned int len;
	unsigned int ret;
	unsigned int tmp;
	unsigned int times = 0;
	struct dmx_sec_ts_data *ts_data;
	unsigned long mem;
	int total = count;
	s64 prev_time_nsec;
	s64 max_timeout_nsec;

	pr_dbg("%s start w:%d id:%d, addr:0x%0x\n", __func__,
		count, pchan->id, (u32)(long)buf_phys);
	do {
	} while (!rdma_get_ready(pchan->id) && times++ < 20);

	if (rch_sync_num != rch_sync_num_last) {
		rdma_config_sync_num(rch_sync_num);
		rch_sync_num_last = rch_sync_num;
	}

	times = 0;
	if (isphybuf) {
		pchan->enable = 1;
		ts_data = (struct dmx_sec_ts_data *)buf;
#ifdef CHECK_PACKET_ALIGNM
		check_packet_alignm(ts_data->buf_start, ts_data->buf_end, pack_len);
#endif
		tmp = (unsigned long)ts_data->buf_start & 0xFFFFFFFF;
		pchan->memdescs->bits.address = tmp;
		pchan->memdescs->bits.byte_length =
			ts_data->buf_end - ts_data->buf_start;

		if (dump_input_ts) {
			dump_file_open(INPUT_DUMP_FILE, &pchan->dump_file, pchan->id);
			mem = (unsigned long)
				phys_to_virt(ts_data->buf_start);
			dump_file_write((char *)mem,
				pchan->memdescs->bits.byte_length, &pchan->dump_file);
		} else {
			dump_file_close(&pchan->dump_file);
		}

		dma_sync_single_for_device(aml_get_device(),
			pchan->memdescs_phy, sizeof(union mem_desc),
			DMA_TO_DEVICE);

//			tmp = (unsigned long)(pchan->memdescs) & 0xFFFFFFFF;
		tmp = pchan->memdescs_phy & 0xFFFFFFFF;
		len = pchan->memdescs->bits.byte_length;
		//rdma_config_enable(pchan->id, 1, tmp, count, len);

		rdma_config_enable(pchan, 1, tmp, count, len, pack_len);
		pr_dbg("%s isphybuf\n", __func__);
		/*it will exit write loop*/
		total = len;
	} else {
		if (dump_input_ts) {
			dump_file_open(INPUT_DUMP_FILE, &pchan->dump_file, pchan->id);
			dump_file_write((char *)buf, total, &pchan->dump_file);
		} else {
			dump_file_close(&pchan->dump_file);
		}

		dma_sync_single_for_device(aml_get_device(),
			(dma_addr_t)buf_phys, total, DMA_TO_DEVICE);

		pchan->memdescs->bits.address = (u32)(long)buf_phys;
		//set desc mem ==len for trigger data transfer.
		pchan->memdescs->bits.byte_length = total;
		dma_sync_single_for_device(aml_get_device(),
			pchan->memdescs_phy, sizeof(union mem_desc),
			DMA_TO_DEVICE);

		wmb();	/*Ensure pchan->mem contents visible */

		pr_dbg("%s, input data:0x%0x, des len:%d\n", __func__,
		       (*(char *)(pchan->mem)), total);
		pr_dbg("%s, desc data:0x%0x 0x%0x\n", __func__,
		       (*(unsigned int *)(pchan->memdescs)),
		       (*((unsigned int *)(pchan->memdescs) + 1)));

		pchan->enable = 1;
		tmp = pchan->memdescs_phy & 0xFFFFFFFF;
		//rdma_config_enable(pchan->id, 1, tmp,
		rdma_config_enable(pchan, 1, tmp,
				   pchan->mem_size, total, pack_len);
	}
	prev_time_nsec = ktime_to_ns(ktime_get());
	max_timeout_nsec = (s64)write_timeout_ms * 1000 * 1000;
	do {
		usleep_range(100, 200);
	} while (!rdma_get_done(pchan->id) &&
		(ktime_to_ns(ktime_get()) - prev_time_nsec) < max_timeout_nsec);

	ret = rdma_get_rd_len(pchan->id);
	if (ret != total)
		dprint("%s, len not equal,ret:%d,w:%d\n",
		       __func__, ret, total);

	pr_dbg("#######rdma##########\n");
	pr_dbg("status:0x%0x\n", rdma_get_status(pchan->id));
	pr_dbg("err:0x%0x, len err:0x%0x, active:%d\n",
	       rdma_get_err(), rdma_get_len_err(), rdma_get_active());
	pr_dbg("pkt_sync:0x%0x\n", rdma_get_pkt_sync_status(pchan->id));
	pr_dbg("ptr:0x%0x\n", rdma_get_ptr(pchan->id));
	pr_dbg("cfg fifo:0x%0x\n", rdma_get_cfg_fifo());
	pr_dbg("#######rdma##########\n");

	/*disable */
	//rdma_config_enable(pchan->id, 0, 0, 0, 0);
	rdma_config_enable(pchan, 0, 0, 0, 0, 0);
	rdma_clean(pchan->id);

	pr_dbg("%s end\n", __func__);
	rdma_config_ready(pchan->id);
	return count;
}

/**
 * write to channel
 * \param pchan:struct chan_id handle
 * \param buf: data addr
 * \param buf_phys: data phys addr
 * \param  count: write size
 * \param  isphybuf: isphybuf
 * \param  pack_len: 188 or 192
 * \retval -1:fail
 * \retval written size
 */
int SC2_bufferid_non_block_write(struct chan_id *pchan, const char *buf, char *buf_phys,
		       unsigned int count, int isphybuf, int pack_len)
{
	unsigned int len;
	unsigned int tmp;
	unsigned int times = 0;
	struct dmx_sec_ts_data *ts_data;
	int total = count;
	unsigned long mem;

	pr_dbg("%s id:%d start w:%d\n", __func__, pchan->id, count);
	do {
	} while (!rdma_get_ready(pchan->id) && times++ < 20);

	if (rch_sync_num != rch_sync_num_last) {
		rdma_config_sync_num(rch_sync_num);
		rch_sync_num_last = rch_sync_num;
	}

	times = 0;
	if (isphybuf) {
		pchan->enable = 1;
		ts_data = (struct dmx_sec_ts_data *)buf;
#ifdef CHECK_PACKET_ALIGNM
		check_packet_alignm(ts_data->buf_start, ts_data->buf_end, pack_len);
#endif
		tmp = (unsigned long)ts_data->buf_start & 0xFFFFFFFF;
		pchan->memdescs->bits.address = tmp;
		pchan->memdescs->bits.byte_length =
			ts_data->buf_end - ts_data->buf_start;

		if (dump_input_ts) {
			dump_file_open(INPUT_DUMP_FILE, &pchan->dump_file, pchan->id);
			mem = (unsigned long)
				phys_to_virt(ts_data->buf_start);
			dump_file_write((char *)mem,
				pchan->memdescs->bits.byte_length, &pchan->dump_file);
		} else {
			dump_file_close(&pchan->dump_file);
		}

		dma_sync_single_for_device(aml_get_device(),
			pchan->memdescs_phy, sizeof(union mem_desc),
			DMA_TO_DEVICE);

//			tmp = (unsigned long)(pchan->memdescs) & 0xFFFFFFFF;
		tmp = pchan->memdescs_phy & 0xFFFFFFFF;
		len = pchan->memdescs->bits.byte_length;
		//rdma_config_enable(pchan->id, 1, tmp, count, len);

		rdma_config_enable(pchan, 1, tmp, count, len, pack_len);
		pr_dbg("%s isphybuf\n", __func__);
		/*it will exit write loop*/
		total = len;
	} else {
		if (dump_input_ts) {
			dump_file_open(INPUT_DUMP_FILE, &pchan->dump_file, pchan->id);
			dump_file_write((char *)buf, total, &pchan->dump_file);
		} else {
			dump_file_close(&pchan->dump_file);
		}

		pchan->memdescs->bits.address = (u32)(long)buf_phys;
		//set desc mem ==len for trigger data transfer.
		pchan->memdescs->bits.byte_length = total;
		dma_sync_single_for_device(aml_get_device(),
			pchan->memdescs_phy, sizeof(union mem_desc),
			DMA_TO_DEVICE);

		wmb();	/*Ensure pchan->mem contents visible */

		pr_dbg("%s, input data:0x%0x, des len:%d\n", __func__,
			   (*(char *)(pchan->mem)), total);
		pr_dbg("%s, desc data:0x%0x 0x%0x\n", __func__,
			   (*(unsigned int *)(pchan->memdescs)),
			   (*((unsigned int *)(pchan->memdescs) + 1)));

		pchan->enable = 1;
		tmp = pchan->memdescs_phy & 0xFFFFFFFF;
		//rdma_config_enable(pchan->id, 1, tmp,
		rdma_config_enable(pchan, 1, tmp,
				   pchan->mem_size, total, pack_len);
	}
	return count;
}

/**
 * check wrint done
 * \param pchan:struct chan_id handle
 * \retval 1:done, 0:not done
 */
int SC2_bufferid_non_block_write_status(struct chan_id *pchan)
{
	return rdma_get_done(pchan->id);
}

/**
 * free channel
 * \param pchan:struct chan_id handle
 * \retval 0:success
 */
int SC2_bufferid_non_block_write_free(struct chan_id *pchan)
{
	/*disable */
	//rdma_config_enable(pchan->id, 0, 0, 0, 0);
	rdma_config_enable(pchan, 0, 0, 0, 0, 0);
	rdma_clean(pchan->id);
	pr_dbg("%s end\n", __func__);
	rdma_config_ready(pchan->id);
	return 0;
}

int SC2_bufferid_write_empty(struct chan_id *pchan, int pid)
{
	unsigned int len;
	unsigned int ret;
	char *p;
	unsigned int tmp;
	int i = 0;
	unsigned int times = 0;
	unsigned long virt = 0;
	unsigned long phys = 0;

#define MAX_EMPTY_PACKET_NUM     4

	ret = cache_malloc(MAX_EMPTY_PACKET_NUM * 188, &virt, &phys);
	if (ret == -1) {
		dprint("%s, fail\n", __func__);
		return -1;
	}

	do {
	} while (!rdma_get_ready(pchan->id) && times++ < 20);

	if (rch_sync_num != rch_sync_num_last) {
		rdma_config_sync_num(rch_sync_num);
		rch_sync_num_last = rch_sync_num;
	}

	p = (char *)virt;

	memset(p, 0, 188);
	p[0] = 0x47;
	p[1] = (pid >> 8) & 0x1f;
	p[2] = pid & 0xff;
	p[3] = 0x20;

	for (i = 1; i < MAX_EMPTY_PACKET_NUM; i++)
		memcpy(p + 188 * i, p, 188);

	len = 188 * MAX_EMPTY_PACKET_NUM;

	dma_sync_single_for_device(aml_get_device(),
		phys, len, DMA_TO_DEVICE);

	pchan->memdescs->bits.address = phys;
	//set desc mem ==len for trigger data transfer.
	pchan->memdescs->bits.byte_length = len;
	dma_sync_single_for_device(aml_get_device(),
		pchan->memdescs_phy, sizeof(union mem_desc),
		DMA_TO_DEVICE);

	wmb();	/*Ensure pchan->mem contents visible */

	pr_dbg("%s, desc data:0x%0x 0x%0x\n", __func__,
	       (*(unsigned int *)(pchan->memdescs)),
	       (*((unsigned int *)(pchan->memdescs) + 1)));

	pchan->enable = 1;
	tmp = pchan->memdescs_phy & 0xFFFFFFFF;
	//rdma_config_enable(pchan->id, 1, tmp,
	rdma_config_enable(pchan, 1, tmp, len, len, 188);

	do {
	} while (!rdma_get_done(pchan->id));

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
	//rdma_config_enable(pchan->id, 0, 0, 0, 0);
	rdma_config_enable(pchan, 0, 0, 0, 0, 0);
	rdma_clean(pchan->id);

	rdma_config_ready(pchan->id);
	cache_free(len, phys);
	dprint("%s pid:%d end\n", __func__, pid);
	return len;
}

