// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * drivers/amlogic/media/di_multi/di_sys.c
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

#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/semaphore.h>
#include <linux/workqueue.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/major.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/proc_fs.h>
#include <linux/list.h>
#include <linux/of_reserved_mem.h>
#include <linux/of_irq.h>
#include <linux/uaccess.h>
#include <linux/of_fdt.h>
#include <linux/cma.h>
#include <linux/dma-contiguous.h>
#include <linux/ctype.h>
#include <linux/string.h>
#include <linux/of_device.h>

#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vpu/vpu.h>
/*dma_get_cma_size_int_byte*/
#include <linux/amlogic/media/codec_mm/codec_mm.h>

#include "deinterlace_dbg.h"
#include "deinterlace.h"
#include "di_data_l.h"
#include "di_data.h"
#include "di_dbg.h"
#include "di_vframe.h"
#include "di_task.h"
#include "di_prc.h"
#include "di_sys.h"
#include "di_api.h"
#include "di_que.h"

#include "register.h"
#include "nr_downscale.h"

static di_dev_t *di_pdev;

struct di_dev_s *get_dim_de_devp(void)
{
	return di_pdev;
}

unsigned int di_get_dts_nrds_en(void)
{
	return get_dim_de_devp()->nrds_enable;
}

u8 *dim_vmap(ulong addr, u32 size, bool *bflg)
{
	u8 *vaddr = NULL;
	ulong phys = addr;
	u32 offset = phys & ~PAGE_MASK;
	u32 npages = PAGE_ALIGN(size) / PAGE_SIZE;
	struct page **pages = NULL;
	pgprot_t pgprot;
	int i;

	if (!PageHighMem(phys_to_page(phys))) {
		*bflg = false;
		return phys_to_virt(phys);
	}
	if (offset)
		npages++;
	pages = vmalloc(sizeof(struct page *) * npages);
	if (!pages)
		return NULL;
	for (i = 0; i < npages; i++) {
		pages[i] = phys_to_page(phys);
		phys += PAGE_SIZE;
	}
	pgprot = pgprot_writecombine(PAGE_KERNEL);
	vaddr = vmap(pages, npages, VM_MAP, pgprot);
	if (!vaddr) {
		PR_ERR("the phy(%lx) vmaped fail, size: %d\n",
		       addr - offset, npages << PAGE_SHIFT);
		vfree(pages);
		return NULL;
	}
	vfree(pages);
	*bflg = true;
	return vaddr + offset;
}

void dim_unmap_phyaddr(u8 *vaddr)
{
	void *addr = (void *)(PAGE_MASK & (ulong)vaddr);

	vunmap(addr);
}

void dim_mcinfo_v_alloc(struct di_buf_s *pbuf, unsigned int bsize)
{
	if (!dimp_get(edi_mp_lmv_lock_win_en) ||
	    pbuf->mcinfo_alloc_flg)
		return;
	pbuf->mcinfo_adr_v = (unsigned short *)dim_vmap(pbuf->mcinfo_adr,
				      bsize,
				      &pbuf->mcinfo_alloc_flg);
	if (!pbuf->mcinfo_adr_v)
		PR_ERR("%s:%d\n", __func__, pbuf->index);
	else
		PR_INF("mcinfo v [%d], ok\n", pbuf->index);
}

void dim_mcinfo_v_release(struct di_buf_s *pbuf)
{
	if (pbuf->mcinfo_alloc_flg) {
		dim_unmap_phyaddr((u8 *)pbuf->mcinfo_adr_v);
		pbuf->mcinfo_alloc_flg = false;
		PR_INF("%s [%d], ok\n", __func__, pbuf->index);
	}
}

#ifdef MARK_HIS
void dim_mcinfo_v_alloc_idat(struct dim_iat_s *idat, unsigned int bsize)
{
	if (/*!dimp_get(edi_mp_lmv_lock_win_en) ||*/
	    !idat || idat->mcinfo_alloc_flg)
		return;
	idat->mcinfo_adr_v = (unsigned short *)dim_vmap(idat->start_mc,
				      bsize,
				      &idat->mcinfo_alloc_flg);
	if (!idat->mcinfo_adr_v)
		PR_ERR("%s:%d\n", __func__, idat->header.index);
	else
		dbg_mem2("mcinfo v [%d], ok 0x%px\n",
			 idat->header.index,
			 idat->mcinfo_adr_v);
}

void dim_mcinfo_v_release_idat(struct dim_iat_s *idat)
{
	if (idat->mcinfo_alloc_flg) {
		dim_unmap_phyaddr((u8 *)idat->mcinfo_adr_v);
		idat->mcinfo_alloc_flg = false;
		PR_INF("%s [%d], ok\n", __func__, idat->header.index);
	}
}
#endif

/********************************************
 * mem
 *******************************************/
#ifdef CONFIG_CMA
#define TVP_MEM_PAGES	0xffff
static bool mm_codec_alloc(const char *owner, size_t count,
			   int cma_mode,
			   struct dim_mm_s *o,
			   bool tvp_flg)
{
	int flags = 0;
	bool istvp = false;

	#ifdef HIS_CODE
	if (codec_mm_video_tvp_enabled()) {
		istvp = true;
		flags |= CODEC_MM_FLAGS_TVP;
		o->flg |= DI_BIT0;
	} else {
		flags |= CODEC_MM_FLAGS_RESERVED | CODEC_MM_FLAGS_CPU;
		o->flg &= (~DI_BIT0);
	}
	#else
	if (tvp_flg) {
		istvp = true;
		flags |= CODEC_MM_FLAGS_TVP;
	} else {
		flags |= CODEC_MM_FLAGS_RESERVED | CODEC_MM_FLAGS_CPU;
	}
	#endif
	if (cma_mode == 4 && !istvp)
		flags = CODEC_MM_FLAGS_CMA_FIRST |
			CODEC_MM_FLAGS_CPU;
	o->addr = codec_mm_alloc_for_dma(owner,
					 count,
					 0,
					 flags);
	if (o->addr == 0) {
		PR_ERR("%s: failed\n", __func__);
		return false;
	}
	if (istvp)
		o->ppage = (struct page *)TVP_MEM_PAGES;
	else
		o->ppage = codec_mm_phys_to_virt(o->addr);
	return true;
}

static bool mm_cma_alloc(struct device *dev, size_t count,
			 struct dim_mm_s *o)
{
	o->ppage = dma_alloc_from_contiguous(dev, count, 0, 0);
	if (o->ppage) {
		o->addr = page_to_phys(o->ppage);
		return true;
	}
	PR_ERR("%s: failed\n", __func__);
	return false;
}

bool dim_mm_alloc(int cma_mode, size_t count, struct dim_mm_s *o,
		  bool tvp_flg)
{
	struct di_dev_s *de_devp = get_dim_de_devp();
	bool ret;

	if (cma_mode == 3 || cma_mode == 4)
		ret = mm_codec_alloc(DEVICE_NAME,
				     count,
				     cma_mode,
				     o,
				     tvp_flg);
	else
		ret = mm_cma_alloc(&de_devp->pdev->dev, count, o);
	return ret;
}

bool dim_mm_release(int cma_mode,
		    struct page *pages,
		    int count,
		    unsigned long addr)
{
	struct di_dev_s *de_devp = get_dim_de_devp();
	bool ret = true;

	if (cma_mode == 3 || cma_mode == 4)
		codec_mm_free_for_dma(DEVICE_NAME, addr);
	else
		ret = dma_release_from_contiguous(&de_devp->pdev->dev,
						  pages,
						  count);
	return ret;
}

unsigned int dim_cma_alloc_total(struct di_dev_s *de_devp)
{
	struct dim_mm_t_s *mmt = dim_mmt_get();
	struct dim_mm_s omm;
	bool ret;

	ret = dim_mm_alloc(cfgg(MEM_FLAG),
			   mmt->mem_size >> PAGE_SHIFT,
			   &omm,
			   0);
	if (!ret) /*failed*/
		return 0;
	mmt->mem_start = omm.addr;
	mmt->total_pages = omm.ppage;
	if (cfgnq(MEM_FLAG, EDI_MEM_M_REV) && de_devp->nrds_enable)
		dim_nr_ds_buf_init(cfgg(MEM_FLAG), 0, &de_devp->pdev->dev, 0);
	return 1;
}

static bool dim_cma_release_total(void)
{
	struct dim_mm_t_s *mmt = dim_mmt_get();
	bool ret = false;
	bool lret = false;

	if (!mmt) {
		PR_ERR("%s:mmt is null\n", __func__);
		return lret;
	}
	ret = dim_mm_release(cfgg(MEM_FLAG), mmt->total_pages,
			     mmt->mem_size >> PAGE_SHIFT,
			     mmt->mem_start);
	if (ret) {
		mmt->total_pages = NULL;
		mmt->mem_start = 0;
		mmt->mem_size = 0;
		lret = true;
	} else {
		PR_ERR("%s:fail.\n", __func__);
	}
	return lret;
}

#endif

bool dim_mm_alloc_api(int cma_mode, size_t count, struct dim_mm_s *o,
		      bool tvp_flg)
{
	bool ret = false;
#ifdef CONFIG_CMA
	ret = dim_mm_alloc(cma_mode, count, o, tvp_flg);
#endif
	return ret;
}

bool dim_mm_release_api(int cma_mode,
			struct page *pages,
			int count,
			unsigned long addr)
{
	bool ret = false;
#ifdef CONFIG_CMA
	ret = dim_mm_release(cma_mode, pages, count, addr);
#endif
	return ret;
}

bool dim_rev_mem_check(void)
{
	struct di_dev_s *di_devp = get_dim_de_devp();
	struct dim_mm_t_s *mmt = dim_mmt_get();
	unsigned int ch;
	unsigned int o_size;
	unsigned long rmstart;
	unsigned int rmsize;
	unsigned int flg_map;

	if (!di_devp) {
		PR_ERR("%s:no dev\n", __func__);
		return false;
	}
	if (!mmt) {
		PR_ERR("%s:mmt\n", __func__);
		return false;
	}
	if (cfgeq(MEM_FLAG, EDI_MEM_M_REV) && di_devp->mem_flg)
		return true;
	PR_INF("%s\n", __func__);
	dil_get_rev_mem(&rmstart, &rmsize);
	dil_get_flg(&flg_map);
	if (!rmstart) {
		PR_ERR("%s:reserved mem start add is 0\n", __func__);
		return false;
	}
	mmt->mem_start = rmstart;
	mmt->mem_size = rmsize;
	if (!flg_map)
		di_devp->flags |= DI_MAP_FLAG;
	o_size = rmsize / DI_CHANNEL_NUB;
	for (ch = 0; ch < DI_CHANNEL_NUB; ch++) {
		di_set_mem_info(ch,
				mmt->mem_start + (o_size * ch), o_size);
		PR_INF("rmem:ch[%d]:start:0x%lx, size:%uB\n",
		       ch,
		       (mmt->mem_start + (o_size * ch)),
		       o_size);
	}
	PR_INF("rmem:0x%lx, size %uMB.\n",
	       mmt->mem_start, (mmt->mem_size >> 20));
	di_devp->mem_flg = true;
	return true;
}

static void dim_mem_remove(void)
{
#ifdef CONFIG_CMA
	dim_cma_release_total();
#endif
}

static void dim_mem_prob(void)
{
	unsigned int mem_flg = cfgg(MEM_FLAG);
	struct di_dev_s *di_devp = get_dim_de_devp();
	struct dim_mm_t_s *mmt = dim_mmt_get();

	if (mem_flg >= EDI_MEM_M_MAX) {
		cfgs(MEM_FLAG, EDI_MEM_M_CMA);
		PR_ERR("%s:mem_flg overflow[%d], set to def\n",
		       __func__, mem_flg);
		mem_flg = cfgg(MEM_FLAG);
	}
	switch (mem_flg) {
	case EDI_MEM_M_REV:
		dim_rev_mem_check();
		dip_cma_st_set_ready_all();
		break;
#ifdef CONFIG_CMA
	case EDI_MEM_M_CMA:
		di_devp->flags |= DI_MAP_FLAG;
		mmt->mem_size =
			dma_get_cma_size_int_byte(&di_devp->pdev->dev);
			PR_INF("mem size from dts:0x%x\n", mmt->mem_size);
		break;
	case EDI_MEM_M_CMA_ALL:
		di_devp->flags |= DI_MAP_FLAG;
		mmt->mem_size =
			dma_get_cma_size_int_byte(&di_devp->pdev->dev);
			PR_INF("mem size from dts:0x%x\n", mmt->mem_size);
		if (dim_cma_alloc_total(di_devp))
			dip_cma_st_set_ready_all();
		break;
	case EDI_MEM_M_CODEC_A:
	case EDI_MEM_M_CODEC_B:
		di_devp->flags |= DI_MAP_FLAG;
		if (mmt->mem_size <= 0x800000) {/*need check??*/
			mmt->mem_size = 0x2800000;
			if (mem_flg != EDI_MEM_M_CODEC_A)
				cfgs(MEM_FLAG, EDI_MEM_M_CODEC_B);
		}
		break;
#endif
	case EDI_MEM_M_MAX:
	default:
		break;
	}
}

static const struct que_creat_s qbf_blk_cfg_q[] = {//TST_Q_IN_NUB
	[QBF_BLK_Q_IDLE] = {
		.name	= "QBF_BLK_IDLE",
		.type	= Q_T_FIFO,
		.lock	= 0,
	},
	[QBF_BLK_Q_READY] = {
		.name	= "QBF_BLK_READY",
		.type	= Q_T_FIFO,
		.lock	= DIM_QUE_LOCK_RD,
	},
	[QBF_BLK_Q_RCYCLE] = {
		.name	= "QBF_BLK_RCY",
		.type	= Q_T_FIFO,
		.lock	= 0,
	},
	[QBF_BLK_Q_RCYCLE2] = {
		.name	= "QBF_BLK_RCY2",
		.type	= Q_T_FIFO,
		.lock	= 0,
	}
};

static const struct qbuf_creat_s qbf_blk_cfg_qbuf = {
	.name	= "qbuf_blk",
	.nub_que	= QBF_BLK_Q_NUB,
	.nub_buf	= DIM_BLK_NUB,
	.code		= CODE_BLK,
};

void bufq_blk_int(struct di_ch_s *pch)
{
	struct dim_mm_blk_s *blk_buf;
	struct buf_que_s *pbufq;
	int i;

	if (!pch) {
		//PR_ERR("%s:\n", __func__);
		return;
	}

	/* clear buf*/
	blk_buf = &pch->blk_bf[0];
	memset(blk_buf, 0, sizeof(*blk_buf) * DIM_BLK_NUB);

	/* set buf's header */
	for (i = 0; i < DIM_BLK_NUB; i++) {
		blk_buf = &pch->blk_bf[i];

		blk_buf->header.code	= CODE_BLK;
		blk_buf->header.index	= i;
		blk_buf->header.ch	= pch->ch_id;
	}

	pbufq = &pch->blk_qb;
	/*clear bq */
	memset(pbufq, 0, sizeof(*pbufq));

	/* point to resource */
	for (i = 0; i < QBF_BLK_Q_NUB; i++)
		pbufq->pque[i] = &pch->blk_q[i];

	for (i = 0; i < DIM_BLK_NUB; i++)
		pbufq->pbuf[i].qbc = &pch->blk_bf[i].header;

	qbuf_int(pbufq, &qbf_blk_cfg_q[0], &qbf_blk_cfg_qbuf);

	/* all to idle */
	qbuf_in_all(pbufq, QBF_BLK_Q_IDLE);
}

void bufq_blk_exit(struct di_ch_s *pch)
{
	struct buf_que_s *pbufq;

	PR_INF("%s:\n", __func__);
	if (!pch) {
		//PR_ERR("%s:\n", __func__);
		return;
	}
	pbufq = &pch->blk_qb;

	qbuf_release_que(pbufq);
}

void blk_polling(unsigned int ch, struct mtsk_cmd_s *cmd)
{
	struct di_mtask *tsk = get_mtask();
	struct dim_fcmd_s *fcmd;
	struct di_ch_s *pch;
	struct buf_que_s *pbufq;
	struct dim_mm_blk_s *blk_buf;
	unsigned int cnt = 0;
	unsigned int i;
	unsigned int index;
	bool ret;
	/*alloc*/
	bool aret;
	struct dim_mm_s omm;
	bool flg_release, flg_alloc = false;
	unsigned int length;
	struct div2_mm_s *mm;
	unsigned int size_p;
	enum EDI_TOP_STATE chst;
	u64 timer_st, timer_end, diff; //debug only

	//struct di_dev_s *de_devp = get_dim_de_devp();

	pch = get_chdata(ch);
	fcmd = &tsk->fcmd[ch];

	pbufq = &pch->blk_qb;
	mm = dim_mm_get(ch);
	size_p = cmd->flg.b.page;
	switch (cmd->cmd) {
	case ECMD_BLK_RELEASE_ALL:
		dbg_mem2("%s:ch[%d] release all\n", __func__, ch);
		/* READY -> BLOCK_RCYCLE*/
		length = qbufp_count(pbufq, QBF_BLK_Q_READY);
		if (length)
			flg_release = true;
		else
			flg_release = false;

		while (flg_release) {
			ret = qbuf_out(pbufq, QBF_BLK_Q_READY, &index);
			if (!ret)
				break;
			qbuf_in(pbufq, QBF_BLK_Q_RCYCLE, index);

			if (qbuf_is_empty(pbufq, QBF_BLK_Q_READY))
				flg_release = false;
			else
				flg_release = true;
		}
		/* QBF_BLK_Q_RCYCLE2 -> BLOCK_RCYCLE*/
		length = qbufp_count(pbufq, QBF_BLK_Q_RCYCLE2);

		if (length)
			flg_release = true;
		else
			flg_release = false;

		while (flg_release) {
			ret = qbuf_out(pbufq, QBF_BLK_Q_RCYCLE2, &index);
			if (!ret)
				break;
			qbuf_in(pbufq, QBF_BLK_Q_RCYCLE, index);

			if (qbuf_is_empty(pbufq, QBF_BLK_Q_RCYCLE2))
				flg_release = false;
			else
				flg_release = true;
		}

		/* BLOCK_RCYCLE -> release */
		length = qbufp_count(pbufq, QBF_BLK_Q_RCYCLE);
		if (length)
			flg_release = true;
		else
			flg_release = false;

		cnt = 0;

		while (flg_release) {
			ret = qbuf_out(pbufq, QBF_BLK_Q_RCYCLE, &index);
			if (!ret)
				break;
			blk_buf = (struct dim_mm_blk_s *)pbufq->pbuf[index].qbc;
			/* hf */
			if (blk_buf->flg_hf) {
				//ret = dim_mm_release(EDI_MEM_M_CMA,
				ret = dim_mm_release(cfgg(MEM_FLAG),
					blk_buf->hf_buff.pages,
					blk_buf->hf_buff.cnt,
					blk_buf->hf_buff.mem_start);
				if (ret) {
					fcmd->sum_hf_psize -= blk_buf->hf_buff.cnt;
					memset(&blk_buf->hf_buff, 0, sizeof(blk_buf->hf_buff));
					blk_buf->flg_hf = 0;
					fcmd->sum_hf_alloc--;
					dbg_mem2("release:hf:%d\n", blk_buf->header.index);
				} else {
					PR_ERR("%s:fail.release hf [%d] 0x%x:\n", __func__,
					       index, fcmd->sum_hf_psize);
				}
			}

			/* hf end */
			ret = dim_mm_release(cfgg(MEM_FLAG),
					     blk_buf->pages,
					     //blk_buf->size_page,
					     blk_buf->flg.b.page,
					     blk_buf->mem_start);
			if (ret) {
				dbg_mem2("blk r:%d:st[0x%lx] size_p[0x%x]\n",
					 blk_buf->header.index,
					 blk_buf->mem_start,
					 blk_buf->flg.b.page);
				if (blk_buf->flg.b.is_i)
					mm->sts.num_local--;
				else
					mm->sts.num_post--;
				blk_buf->pages		= NULL;
				blk_buf->mem_start	= 0;
				//blk_buf->size_page	= 0;
				blk_buf->flg_alloc	= false;
				blk_buf->flg.d32	= 0;
				blk_buf->sct		= NULL;
				blk_buf->sct_keep	= 0xff;
				qbuf_in(pbufq, QBF_BLK_Q_IDLE, index);
				cnt++;
				fcmd->sum_alloc--;
			} else {
				qbuf_in(pbufq, QBF_BLK_Q_RCYCLE, index);
				PR_ERR("%s:fail.release [%d]\n", __func__,
				       index);
			}

			if (qbuf_is_empty(pbufq, QBF_BLK_Q_RCYCLE))
				flg_release = false;
			else
				flg_release = true;
		}

		dbg_mem2("%s:release:[%d]\n", __func__, cnt);

		fcmd->release_cmd = 0;

		break;
		/* note: not break; add ECMD_BLK_RELEASE:*/
	case ECMD_BLK_RELEASE:
		/* QBF_BLK_Q_RCYCLE2 -> BLOCK_RCYCLE*/
		length = qbufp_count(pbufq, QBF_BLK_Q_RCYCLE2);

		if (length)
			flg_release = true;
		else
			flg_release = false;

		while (flg_release) {
			ret = qbuf_out(pbufq, QBF_BLK_Q_RCYCLE2, &index);
			if (!ret)
				break;
			qbuf_in(pbufq, QBF_BLK_Q_RCYCLE, index);

			if (qbuf_is_empty(pbufq, QBF_BLK_Q_RCYCLE2))
				flg_release = false;
			else
				flg_release = true;
		}

		/* BLOCK_RCYCLE -> release */
		length = qbufp_count(pbufq, QBF_BLK_Q_RCYCLE);
		if (length)
			flg_release = true;
		else
			flg_release = false;

		cnt = 0;

		while (flg_release) {
			ret = qbuf_out(pbufq, QBF_BLK_Q_RCYCLE, &index);
			if (!ret)
				break;
			blk_buf = (struct dim_mm_blk_s *)pbufq->pbuf[index].qbc;
			/* hf */
			if (blk_buf->flg_hf) {
				//ret = dim_mm_release(EDI_MEM_M_CMA,
				ret = dim_mm_release(cfgg(MEM_FLAG),
					blk_buf->hf_buff.pages,
					blk_buf->hf_buff.cnt,
					blk_buf->hf_buff.mem_start);
				if (ret) {
					fcmd->sum_hf_psize -= blk_buf->hf_buff.cnt;
					memset(&blk_buf->hf_buff, 0, sizeof(blk_buf->hf_buff));
					blk_buf->flg_hf = 0;
					dbg_mem2("release:hf:%d\n", blk_buf->header.index);
					fcmd->sum_hf_alloc--;
				} else {
					PR_ERR("%s:fail.release hf [%d]\n", __func__,
					       index);
				}
			}
			/* hf end */

			ret = dim_mm_release(cfgg(MEM_FLAG),
					     blk_buf->pages,
					     //blk_buf->size_page,
					     blk_buf->flg.b.page,
					     blk_buf->mem_start);
			if (ret) {
				dbg_mem2("blk r:%d:st[0x%lx] size_p[0x%x]\n",
					 blk_buf->header.index,
					 blk_buf->mem_start,
					 blk_buf->flg.b.page);
				if (blk_buf->flg.b.is_i)
					mm->sts.num_local--;
				else
					mm->sts.num_post--;
				blk_buf->pages		= NULL;
				blk_buf->mem_start	= 0;
				//blk_buf->size_page	= 0;
				blk_buf->flg_alloc	= false;
				blk_buf->flg.d32	= 0;
				blk_buf->sct		= NULL;
				blk_buf->sct_keep	= 0xff;
				atomic_set(&blk_buf->p_ref_mem, 0);
				qbuf_in(pbufq, QBF_BLK_Q_IDLE, index);
				cnt++;
				fcmd->sum_alloc--;
			} else {
				qbuf_in(pbufq, QBF_BLK_Q_RCYCLE, index);
				PR_ERR("%s:fail.release [%d]\n", __func__,
				       index);
			}

			if (qbuf_is_empty(pbufq, QBF_BLK_Q_RCYCLE))
				flg_release = false;
			else
				flg_release = true;
		}

		dbg_mem2("%s:release:[%d]\n", __func__, cnt);

		fcmd->release_cmd = 0;

		break;
	case ECMD_BLK_ALLOC:
		/* alloc */
		chst = dip_chst_get(ch);
		dbg_timer(ch, EDBG_TIMER_MEM_1);
		dbg_mem2("%s:ch[%d] alloc:nub[%d],size[0x%x],top_sts[%d],block[%d]\n",
			 __func__, ch, cmd->nub, size_p, chst, cmd->block_mode);
		cnt = 0;
		for (i = 0; i < cmd->nub; i++) {
			if (qbuf_is_empty(pbufq, QBF_BLK_Q_IDLE))
				break;

			/* blk_buf */
			ret = qbuf_out(pbufq, QBF_BLK_Q_IDLE, &index);
			if (!ret)
				break;
			blk_buf = (struct dim_mm_blk_s *)pbufq->pbuf[index].qbc;

			/*alloc*/
			omm.flg = 0;
			timer_st = cur_to_msecs();//dbg
			aret = dim_mm_alloc(cfgg(MEM_FLAG),
					    size_p,
					    &omm,
					    cmd->flg.b.tvp);
			timer_end = cur_to_msecs();//dbg
			diff = timer_end - timer_st;
			dbg_mem2("a:a%d:%ums\n", i, (unsigned int)diff);
			if (!aret) {
				PR_ERR("2:%s: alloc failed %d fail.\n",
				       __func__,
					blk_buf->header.index);
				/* back blk_buf */
				qbuf_in(pbufq, QBF_BLK_Q_IDLE,
					blk_buf->header.index);
				break;
			}

			blk_buf->mem_start	= omm.addr;
			blk_buf->pages		= omm.ppage;
			blk_buf->flg.d32	= cmd->flg.d32;
			//blk_buf->size_page	= cmdbyte->b.page;

			blk_buf->flg.b.tvp	= cmd->flg.b.tvp;//omm.flg;
			blk_buf->flg_alloc	= true;
			blk_buf->reg_cnt	= pch->sum_reg_cnt;
			blk_buf->sct		= NULL;
			blk_buf->sct_keep	= 0xff;
			blk_buf->pat_buf	= NULL;
			if (blk_buf->flg.b.is_i)
				mm->sts.num_local++;
			else
				mm->sts.num_post++;
			dbg_mem2("blk a:%d:st[0x%lx] size_p[0x%x]\n",
				 blk_buf->header.index,
				 blk_buf->mem_start, size_p);
			/*alloc hf */
			if (cmd->hf_need && !blk_buf->flg_hf) {
				memset(&omm, 0, sizeof(omm));
				//aret = dim_mm_alloc(EDI_MEM_M_CMA,
				timer_st = cur_to_msecs();//dbg
				aret = dim_mm_alloc(cfgg(MEM_FLAG),
						    mm->cfg.size_buf_hf >> PAGE_SHIFT,
						    &omm,
						    0);
				timer_end = cur_to_msecs();//dbg
				diff = timer_end - timer_st;
				dbg_mem2("a:b%d:%ums\n", i, (unsigned int)diff);
				if (!aret) {
					PR_ERR("2:%s: alloc hf failed %d fail.0x%x;%d\n",
					       __func__,
						blk_buf->header.index,
						fcmd->sum_hf_psize,
						fcmd->sum_hf_alloc);
					blk_buf->flg_hf = 0;
				} else {
					blk_buf->flg_hf = 1;
					blk_buf->hf_buff.mem_start = omm.addr;
					blk_buf->hf_buff.cnt = mm->cfg.size_buf_hf >> PAGE_SHIFT;
					blk_buf->hf_buff.pages = omm.ppage;
					fcmd->sum_hf_psize += blk_buf->hf_buff.cnt;
					fcmd->sum_hf_alloc++;
					dbg_mem2("alloc:hf:%d:0x%x\n",
						 blk_buf->header.index, fcmd->sum_hf_psize);
				}

			} else if (cmd->hf_need && blk_buf->flg_hf) {
				PR_ERR("%s:have hf?%d\n", __func__, blk_buf->header.index);
			} else {
				blk_buf->flg_hf = 0;
			}
			/* hf en */
			qbuf_in(pbufq, QBF_BLK_Q_READY, blk_buf->header.index);
			fcmd->sum_alloc++;
			cnt++;
		}
		flg_alloc = true;
		dbg_timer(ch, EDBG_TIMER_MEM_1);
		dbg_mem2("%s:alloc:[%d]\n", __func__, cnt);
		fcmd->alloc_cmd = 0;

		break;
	}
	atomic_dec(&fcmd->doing);//fcmd->doing--;
	if (flg_alloc)
		task_send_ready(25);
}

/* mem blk rebuilt end*/
/********************************************/
/* mem config */
static const struct que_creat_s qbf_mem_cfg_q[] = {//TST_Q_IN_NUB
	[QBF_MEM_Q_GET_PRE] = {
		.name	= "QBF_MEM_GET_PRE",
		.type	= Q_T_FIFO_2,
		.lock	= 0,
	},
	[QBF_MEM_Q_GET_PST] = {
		.name	= "QBF_MEM_GET_PST",
		.type	= Q_T_FIFO_2,
		.lock	= 0,
	},
	[QBF_MEM_Q_IN_USED] = {
		.name	= "QBF_MEM_USED",
		.type	= Q_T_FIFO_2,
		.lock	= 0,
	},
	[QBF_MEM_Q_RECYCLE] = {
		.name	= "QBF_MEM_RECYCLE",
		.type	= Q_T_FIFO_2,
		.lock	= 0,
	}
};

static const struct qbuf_creat_s qbf_mem_cfg_qbuf = {
	.name	= "qbuf_mem",
	.nub_que	= QBF_MEM_Q_NUB,
	.nub_buf	= 0,
	.code		= 0,
};

bool di_pst_afbct_check(struct di_ch_s *pch)
{
	//bool ret;
	struct div2_mm_s *mm;
	unsigned int ch;
	struct di_dat_s *pdat;

	ch = pch->ch_id;

	mm = dim_mm_get(ch);
	if (!mm->cfg.dat_pafbct_flg.b.page)
		return true;
	pdat = get_pst_afbct(pch);

	if (pdat->flg_alloc && pdat->virt)
		return true;
	return false;
}

bool di_i_dat_check(struct di_ch_s *pch)
{
	struct div2_mm_s *mm;
	unsigned int ch;
	struct di_dat_s *idat = get_idat(pch);

	ch = pch->ch_id;

	mm = dim_mm_get(ch);

	if (!mm->cfg.num_local)
		return true;
	if (!mm->cfg.size_idat_one)
		return true;
	if (idat->virt && idat->flg_alloc)
		return true;

	return false;
}

bool mem_alloc_check(struct di_ch_s *pch)
{
	struct di_mtask *tsk = get_mtask();
	struct dim_fcmd_s *fcmd;
	unsigned int ch;

	ch = pch->ch_id;

	fcmd = &tsk->fcmd[ch];

	if (atomic_read(&fcmd->doing) > 0)
		return false;
	return true;
}

static unsigned long di_cnt_pst_afbct_addr(unsigned int ch,
					   unsigned int nub)
{
	struct di_ch_s *pch;
	struct div2_mm_s *mm;
	unsigned long addr;
	struct di_dat_s *pdat;

	mm = dim_mm_get(ch);

	if (!mm->cfg.dat_pafbct_flg.d32)
		return 0;

	pch = get_chdata(ch);
	pdat = get_pst_afbct(pch);
	addr = pdat->addr_st +
		(nub * mm->cfg.size_pafbct_one);
	if ((addr + mm->cfg.size_pafbct_one) >
	    pdat->addr_end) {
		PR_ERR("%s:addr is overflow 0x%lx, 0x%lx\n",
		       __func__, (addr + mm->cfg.size_pafbct_one),
		       pdat->addr_end);
		       addr = pdat->addr_st;
	}
	dbg_mem2("%s:nub[%d], addr[0x%lx]\n", __func__, nub, addr);
	return addr;
}

static void pat_set_addr(struct di_ch_s *pch)
{
	int i;
	struct div2_mm_s *mm;
	unsigned int ch;
	unsigned long addr;
	bool ret;

	ch = pch->ch_id;

	mm = dim_mm_get(ch);

	for (i = 0; i < mm->cfg.nub_pafbct; i++) {
		addr = di_cnt_pst_afbct_addr(ch, i);
		ret = qpat_idle_to_ready(pch, addr);
		if (!ret) {
			PR_ERR("%s:%d\n", __func__, i);
			break;
		}
	}
}

void pat_clear_mem(struct device *dev,
		struct di_ch_s *pch,
		struct dim_pat_s *pat_buf)
{
	unsigned int pat_size;
	struct div2_mm_s *mm;

	if (!dev	||
	    !pch	||
	    !pat_buf	||
	    !pat_buf->vaddr) {
		PR_ERR("%s\n", __func__);
		return;
	}
	mm = dim_mm_get(pch->ch_id);
	pat_size =	mm->cfg.size_pafbct_one;
	dma_sync_single_for_device(dev,
				   pat_buf->mem_start,//src_phys,//
				   pat_size,
				   DMA_TO_DEVICE);
	//dbg_sct("%s:%d:0x%x\n", __func__, pat_buf->header.index, pat_size);
	memset(pat_buf->vaddr, 0, pat_size);

	dma_sync_single_for_device(dev,
				   pat_buf->mem_start,//src_phys,//
				   pat_size,
				   DMA_TO_DEVICE);
}

void pat_frash(struct device *dev,
		struct di_ch_s *pch,
		struct dim_pat_s *pat_buf)
{
	unsigned int pat_size;
	struct div2_mm_s *mm;

	if (!dev	||
	    !pch	||
	    !pat_buf	||
	    !pat_buf->vaddr) {
		PR_ERR("%s\n", __func__);
		return;
	}

	mm = dim_mm_get(pch->ch_id);
	pat_size =	mm->cfg.size_pafbct_one;
	dma_sync_single_for_device(dev,
				   pat_buf->mem_start,//src_phys,//
				   pat_size,
				   DMA_TO_DEVICE);
}

/************************************************
 * afbc_table
 * afbc_info
 ********************
 * cnt
 * xx
 ********************
 * mc info
 */
static void iat_set_addr(struct di_ch_s *pch)
{
	int i;
	struct div2_mm_s *mm;
	unsigned int ch;
	unsigned long addr_st_afbct/*, addr_st_mcinfo*/;
//	unsigned long addr_afbct, addr_mc;
	unsigned int size_tafbct;
	unsigned int size_afbct;
	struct dim_iat_s *idat = NULL;
	bool ret;
	struct di_dat_s *pdat = NULL;

	ch = pch->ch_id;

	mm = dim_mm_get(ch);
	pdat = get_idat(pch);

	size_afbct = mm->cfg.afbct_local_max_size;

	size_tafbct	= size_afbct * mm->cfg.nub_idat;

	addr_st_afbct	= pdat->addr_st;
	//addr_st_mcinfo	= pdat->addr_st + size_tafbct;

	dbg_mem2("%s:ch[%d]\n", __func__, ch);
	for (i = 0; i < mm->cfg.nub_idat; i++) {
		ret = qiat_idle_to_ready(pch, &idat);
		idat->start_afbct	= addr_st_afbct + size_afbct * i;

		//idat->start_mc = addr_st_mcinfo + mm->cfg.mcinfo_size * i;

		//dim_mcinfo_v_alloc_idat(idat, mm->cfg.mcinfo_size);
		if (!ret) {
			PR_ERR("%s:%d\n", __func__, i);
			break;
		}

		dbg_mem2("%d\t:addr_afbct\t0x%lx\n", i, idat->start_afbct);
		//dbg_mem2("\t:addr_mc\t0x%lx\n", idat->start_mc);
	}
}

void pre_sec_alloc(struct di_ch_s *pch, unsigned int flg)
{
	struct di_dat_s *idat;
	struct blk_flg_s flgs;
	unsigned int idat_size;
	//for cma:
	struct dim_mm_s omm;
	bool ret;
	struct di_dev_s *de_devp = get_dim_de_devp();

	idat = get_idat(pch);

	if (idat->flg_alloc)
		return;
	flgs.d32 = flg;
#ifdef USE_KALLOC //use kzalloc:
	idat_size = flgs.b.page << PAGE_SHIFT;

	if (!idat_size) {
		dbg_reg("%s:no size\n", __func__);
		return;
	}
	idat->virt = kzalloc(idat_size, GFP_KERNEL);
	if (!idat->virt) {
		PR_ERR("%s:\n", __func__);
		return;
	}

	idat->addr_st = virt_to_phys(idat->virt);
	idat->addr_end = idat->addr_st + idat_size;
#else	//usd cma:
	idat_size = flgs.b.page;

	if (!idat_size) {
		dbg_reg("%s:no size\n", __func__);
		return;
	}
	dbg_timer(pch->ch_id, EDBG_TIMER_SEC_PRE_B);
	ret = mm_cma_alloc(NULL, idat_size, &omm);
	dbg_timer(pch->ch_id, EDBG_TIMER_SEC_PRE_E);
	idat->flg_from = 1;
	if (!ret) {
		PR_WARN("%s:try:cma di:\n", __func__);
		//use cam: buffer:
		idat->flg_from = 2;
		ret = mm_cma_alloc(&de_devp->pdev->dev, idat_size, &omm);
		if (!ret) {
			PR_ERR("%s:cma\n", __func__);
			idat->flg_from = 0;
			return;
		}
	}
	idat->addr_st	= omm.addr;
	//idat->ppage	= omm.ppage;
	idat->virt	= omm.ppage;
	idat->cnt	= idat_size;
	idat->addr_end = idat->addr_st + (idat_size << PAGE_SHIFT);
#endif
	idat->flg.d32 = flg;
	idat->flg_alloc = 1;
	PR_INF("%s:cma size:%d,0x%px,0x%lx\n",
	       __func__,
	       idat_size, idat->virt, idat->addr_st);
	iat_set_addr(pch);
}

void pst_sec_alloc(struct di_ch_s *pch, unsigned int flg)
{
	struct di_dat_s *pdat;
	struct blk_flg_s flgs;
	unsigned int dat_size;
	//for cma:
	struct dim_mm_s omm;
	bool ret;
	struct di_dev_s *de_devp = get_dim_de_devp();

	pdat = get_pst_afbct(pch);

	flgs.d32 = flg;
#ifdef USE_KALLOC
	dat_size = flgs.b.page << PAGE_SHIFT;

	if (pdat->flg_alloc || !dat_size) {
		dbg_reg("%s:not alloc:%d,0x%x\n",
		       __func__,
		       pdat->flg_alloc, flg);
		return;
	}

	pdat->virt = kzalloc(dat_size, GFP_KERNEL);
	if (!pdat->virt) {
		PR_ERR("%s:\n", __func__);
		return;
	}

	pdat->addr_st = virt_to_phys(pdat->virt);
	pdat->addr_end = pdat->addr_st + dat_size;
#else
	dat_size = flgs.b.page;

	if (pdat->flg_alloc || !dat_size) {
		dbg_reg("%s:not alloc:%d,0x%x\n",
		       __func__,
		       pdat->flg_alloc, flg);
		return;
	}
	dbg_timer(pch->ch_id, EDBG_TIMER_SEC_PST_B);
	ret = mm_cma_alloc(NULL, dat_size, &omm);
	dbg_timer(pch->ch_id, EDBG_TIMER_SEC_PST_E);
	pdat->flg_from = 1;
	if (!ret) {
		PR_WARN("%s:try:cma di:\n", __func__);
		//use cam: buffer:
		pdat->flg_from = 2;
		ret = mm_cma_alloc(&de_devp->pdev->dev, dat_size, &omm);
		if (!ret) {
			PR_ERR("%s:cma\n", __func__);
			pdat->flg_from = 0;
			return;
		}
	}
	pdat->addr_st	= omm.addr;
	//pdat->ppage	= omm.ppage;
	pdat->virt	= omm.ppage;
	pdat->cnt	= dat_size;
	pdat->addr_end = pdat->addr_st + (dat_size << PAGE_SHIFT);
#endif
	pdat->flg.d32 = flg;
	pdat->flg_alloc = 1;
	PR_INF("%s:cma:size:%d,0x%px,0x%lx\n",
	       __func__,
	       dat_size, pdat->virt, pdat->addr_st);

	pat_set_addr(pch);
}

void dim_sec_release(struct di_ch_s *pch)
{
	struct di_dat_s *dat;
	struct di_dev_s *de_devp = get_dim_de_devp();

	dat = get_idat(pch);
	if (dat->flg_alloc) {
#ifdef USE_KALLOC
		kfree(dat->virt);
#else
		if (dat->flg_from == 2)
			dma_release_from_contiguous(&de_devp->pdev->dev,
						    (struct page *)dat->virt,
						    dat->cnt);
		else
			dma_release_from_contiguous(NULL,
						    (struct page *)dat->virt,
						    dat->cnt);
#endif

		dat->virt = NULL;
		memset(dat, 0, sizeof(*dat));
	}

	dat = get_pst_afbct(pch);
	if (dat->flg_alloc) {
#ifdef USE_KALLOC
		kfree(dat->virt);
#else
		if (dat->flg_from == 2)
			dma_release_from_contiguous(&de_devp->pdev->dev,
						    (struct page *)dat->virt,
						    dat->cnt);
		else
			dma_release_from_contiguous(NULL,
					    (struct page *)dat->virt,
					    dat->cnt);
#endif
		dat->virt = NULL;
		memset(dat, 0, sizeof(*dat));
	}
}

static void dim_buf_set_addr(unsigned int ch, struct di_buf_s *buf_p)
{
	/*need have set nr_addr*/
	struct div2_mm_s *mm = dim_mm_get(ch);
	struct afbce_map_s afbce_map;
	struct di_dev_s *devp = get_dim_de_devp();
	//struct di_pre_stru_s *ppre = get_pre_stru(channel);
	int i;
	//bool flg_afbct = false;
	unsigned long addr_afbct = 0;
	struct dim_pat_s *pat_buf;
	struct dim_iat_s *iat_buf;
	struct di_ch_s *pch;

	pch = get_chdata(ch);
	dbg_mem2("%s:ch[%d],buf t[%d] id[%d]\n",
		 __func__,
		 ch,
		 buf_p->type,
		 buf_p->index);
	if (buf_p->buf_is_i) {
		//buf_p->afbct_adr = buf_p->afbc_adr + mm->cfg.afbci_size;
		//buf_p->dw_adr = buf_p->afbct_adr + mm->cfg.afbct_size;

		if (mm->cfg.size_idat_one) {
			iat_buf = qiat_out_ready(pch);
			if (iat_buf) {
				buf_p->iat_buf		= iat_buf;

				buf_p->afbct_adr	= iat_buf->start_afbct;
				//buf_p->mcinfo_adr	= iat_buf->start_mc;
			} else {
				buf_p->iat_buf = NULL;
				PR_ERR("%s:iat is null\n", __func__);
				return;
			}
		} else {
			buf_p->iat_buf = NULL;
			buf_p->afbct_adr = 0;
		}
		buf_p->afbc_adr	= buf_p->adr_start;
		buf_p->dw_adr = buf_p->afbc_adr + mm->cfg.afbci_size;
		buf_p->nr_adr = buf_p->dw_adr;// + mm->cfg.dw_size;
		buf_p->buf_hsize	= mm->cfg.ibuf_hsize;
		/* count afbct setting and crc */
		if (dim_afds() && mm->cfg.afbci_size) {
			afbce_map.bodyadd	= buf_p->nr_adr;
			afbce_map.tabadd	= buf_p->afbct_adr;
			afbce_map.size_buf	= mm->cfg.nr_size;
			afbce_map.size_tab	= mm->cfg.afbct_size;

			buf_p->afbc_crc =
				dim_afds()->int_tab(&devp->pdev->dev,
						    &afbce_map);
		} else {
			buf_p->afbc_crc = 0;
		}

		buf_p->mtn_adr = buf_p->nr_adr +
			mm->cfg.nr_size;
		buf_p->cnt_adr = buf_p->mtn_adr +
			mm->cfg.mtn_size;
		/*if (dim_get_mcmem_alloc()) {*/
		buf_p->mcvec_adr = buf_p->cnt_adr +
			mm->cfg.count_size;
		buf_p->mcinfo_adr = buf_p->mcvec_adr +
			mm->cfg.mv_size;
		if (dim_is_slt_mode()) {//test crc
		//----------------------------
			afbce_map.tabadd	= buf_p->mtn_adr;
			afbce_map.size_tab	= mm->cfg.mtn_size +
						mm->cfg.count_size +
						mm->cfg.mv_size +
						mm->cfg.mcinfo_size;
			dim_int_tab(&devp->pdev->dev,
							    &afbce_map);
			//dbg:
			afbce_map.tabadd	= buf_p->nr_adr;
			afbce_map.size_tab	= mm->cfg.nr_size;
			dim_int_tab(&devp->pdev->dev,
							    &afbce_map);
		}
		//----------------------------

		dim_mcinfo_v_alloc(buf_p, mm->cfg.mcinfo_size);
		//dim_mcinfo_v_alloc(buf_p, mm->cfg.mcinfo_size);

		//buf_p->insert_adr = buf_p->mcinfo_adr + mm->cfg.mcinfo_size;
		/*}*/

		buf_p->nr_size = mm->cfg.nr_size;
		buf_p->tab_size = mm->cfg.afbct_size;
		buf_p->hf_adr	= 0;
		if (buf_p->blk_buf && buf_p->blk_buf->flg_hf)
			PR_ERR("%s:local buffer have hf?%d\n", __func__,
			       buf_p->blk_buf->header.index);
		//
		for (i = 0; i < 3; i++)
			buf_p->canvas_width[i] = mm->cfg.canvas_width[i];
		buf_p->canvas_config_flag = 2;
		buf_p->canvas_height_mc = mm->cfg.canvas_height_mc;
		buf_p->canvas_height	= mm->cfg.canvas_height;
		dbg_mem2("\t:nr_adr\t[0x%lx]\n", buf_p->nr_adr);

		dbg_mem2("\t:afbct_adr\t[0x%lx]\n", buf_p->afbct_adr);
		dbg_mem2("\t:afbc_adr\t[0x%lx]\n", buf_p->afbc_adr);

		//dbg_mem2("\t:dw_adr\t[0x%lx]\n", buf_p->dw_adr);

		dbg_mem2("\t:mtn_adr\t[0x%lx]\n", buf_p->mtn_adr);
		dbg_mem2("\t:cnt_adr\t[0x%lx]\n", buf_p->cnt_adr);
		dbg_mem2("\t:mcvec_adr\t[0x%lx]\n", buf_p->mcvec_adr);

		dbg_mem2("\t:mcinfo_adr\t[0x%lx]\n", buf_p->mcinfo_adr);

	} else if (buf_p->blk_buf) {
		//buf_p->afbct_adr = buf_p->afbc_adr + mm->cfg.pst_afbci_size;

		if (buf_p->blk_buf &&
		    buf_p->blk_buf->flg.b.typ == EDIM_BLK_TYP_PSCT) {
			buf_p->afbc_adr	= buf_p->adr_start;
			buf_p->afbct_adr = 0;
			buf_p->blk_buf->pat_buf = NULL;
		} else if (mm->cfg.dat_pafbct_flg.b.page) {
			pat_buf = qpat_out_ready(pch);
			if (pat_buf) {
				addr_afbct = pat_buf->mem_start;
				//buf_p->pat_buf = pat_buf;
				buf_p->blk_buf->pat_buf = pat_buf;
				pat_buf->flg_mode = 0;
			} else {
				buf_p->blk_buf->pat_buf = NULL;
			}
			buf_p->afbct_adr = addr_afbct;
			buf_p->afbc_adr	= buf_p->adr_start;
		} else {
			buf_p->afbct_adr = buf_p->adr_start;
			buf_p->afbc_adr	= buf_p->adr_start;
			buf_p->blk_buf->pat_buf	= NULL;
		}

		if (mm->cfg.size_buf_hf) {
			if (buf_p->blk_buf->flg_hf) {
				buf_p->hf_adr = buf_p->blk_buf->hf_buff.mem_start;
				di_hf_set_buffer(buf_p, mm);
			} else {
				PR_ERR("%s:size:[%d]:flg[%d]\n",
				       __func__,
				       mm->cfg.size_buf_hf, buf_p->blk_buf->flg_hf);
				buf_p->hf_adr = 0;
			}
		} else {
			buf_p->hf_adr = 0;
		}

		dbg_mem2("%s:pst:flg_afbct[%d], addr[0x%lx]\n",
			__func__,
			mm->cfg.dat_pafbct_flg.d32, addr_afbct);

		//buf_p->dw_adr = buf_p->afbc_adr + mm->cfg.pst_afbci_size;
		buf_p->dw_adr = buf_p->afbc_adr + mm->cfg.pst_afbci_size;
		buf_p->nr_adr = buf_p->dw_adr + mm->cfg.dw_size;
		buf_p->tab_size = mm->cfg.pst_afbct_size;
		buf_p->nr_size = mm->cfg.pst_buf_size;
		buf_p->buf_hsize = mm->cfg.pbuf_hsize;

		if (buf_p->blk_buf &&
		    buf_p->blk_buf->flg.b.typ == EDIM_BLK_TYP_PSCT) {
		    /*sct*/
			//pat_set_vaddr(buf_p->pat_buf,
			//	      mm->cfg.pst_afbct_size);
			buf_p->afbc_crc	= 0;
			buf_p->nr_adr	= 0;
		} else if (dim_afds() && mm->cfg.pst_afbct_size) {
			afbce_map.bodyadd	= buf_p->nr_adr;
			afbce_map.tabadd	= buf_p->afbct_adr;
			afbce_map.size_buf	= mm->cfg.pst_buf_size;
			afbce_map.size_tab	= mm->cfg.pst_afbct_size;

			buf_p->afbc_crc =
				dim_afds()->int_tab(&devp->pdev->dev,
						    &afbce_map);
		} else {
			buf_p->afbc_crc = 0;
		}
		buf_p->nr_size = mm->cfg.pst_buf_size;
		buf_p->canvas_config_flag = 1;
		buf_p->canvas_width[NR_CANVAS]	= mm->cfg.pst_cvs_w;
		buf_p->canvas_height	= mm->cfg.pst_cvs_h;
		buf_p->insert_adr	= 0;
	}

	dbg_mem2("%s:%px,btype[%d]:index[%d]:i[%d]:nr[0x%lx]:hf:[0x%lx]\n", __func__,
		 buf_p,
		 buf_p->type,
		 buf_p->index,
		 buf_p->buf_is_i,
		 buf_p->nr_adr,
		 buf_p->hf_adr);
	dbg_mem2("\t:i_ad[0x%lx]:t_add[0x%lx]:dw[0x%lx]:crc:0x%x\n",
		 buf_p->afbc_adr,
		 buf_p->afbct_adr,
		 buf_p->dw_adr,
		 buf_p->afbc_crc);
	dbg_mem2("\t:mc_info_add[0x%lx]:insert_addr[0x%lx]\n",
		 buf_p->mcinfo_adr,
		 buf_p->insert_adr);
	#ifdef MARK_HIS
	dbg_mem2("\t:%s: adr:0x%lx,size:%d\n", "afbc",
		 buf_p->afbc_adr,
		 mm->cfg.afbci_size);
	dbg_mem2("\t:%s: adr:0x%lx,size:%d\n", "afbc_tab",
		 buf_p->afbct_adr,
		 mm->cfg.afbct_size);
	dbg_mem2("\t:%s: adr:0x%lx,size:%d\n", "dw",
		 buf_p->dw_adr,
		 mm->cfg.dw_size);
	dbg_mem2("\t:%s: adr:0x%lx,size:%d\n", "nr",
		 buf_p->nr_adr,
		 mm->cfg.nr_size);
	#endif
	//msleep(100);
	#ifdef DBG_TEST_CRC_P
	dbg_checkcrc(buf_p, 0);
	#endif
}

bool dim_blk_tvp_is_sct(struct dim_mm_blk_s *blk_buf)
{
	if ((blk_buf->flg.b.typ & 0x7) == EDIM_BLK_TYP_PSCT)
		return true;
	return false;
}

bool dim_blk_tvp_is_out(struct dim_mm_blk_s *blk_buf)
{
	if ((blk_buf->flg.b.typ & 0x8) == EDIM_BLK_TYP_POUT)
		return true;
	return false;
}

void bufq_mem_clear(struct di_ch_s *pch)
{
	struct buf_que_s *pbf_mem;

	/*clear */
	pbf_mem = &pch->mem_qb;
	qbufp_restq(pbf_mem, QBF_MEM_Q_GET_PRE);
	qbufp_restq(pbf_mem, QBF_MEM_Q_GET_PST);
}

void bufq_mem_int(struct di_ch_s *pch)
{
	struct buf_que_s *pbufq;
	int i;

	if (!pch) {
		//PR_ERR("%s:\n", __func__);
		return;
	}

	pbufq = &pch->mem_qb;

	/*clear bq */
	memset(pbufq, 0, sizeof(*pbufq));
	/* point to resource */
	for (i = 0; i < QBF_MEM_Q_NUB; i++)
		pbufq->pque[i] = &pch->mem_q[i];

	qbuf_int(pbufq, &qbf_mem_cfg_q[0], &qbf_mem_cfg_qbuf);
}

void bufq_mem_exit(struct di_ch_s *pch)
{
	struct buf_que_s *pbufq;

	PR_INF("%s:\n", __func__);
	if (!pch) {
		//PR_ERR("%s:\n", __func__);
		return;
	}
	pbufq = &pch->mem_qb;

	qbuf_release_que(pbufq);
}

/************************************************
 * mem_cfg for old flow
 *	QBF_BLK_Q_READY -> QBF_MEM_Q_GET_PRE
 *			-> QBF_MEM_Q_GET_PST
 *	QBF_MEM_Q_GET_PRE -> QBF_MEM_Q_IN_USED
 *	QBF_MEM_Q_GET_PST ->
 ************************************************/
void memq_2_recycle(struct di_ch_s *pch, struct dim_mm_blk_s *blk_buf)
{
	struct buf_que_s *pbf_mem;
	union q_buf_u q_buf;

	pbf_mem = &pch->mem_qb;
	q_buf.qbc = (struct qs_buf_s *)blk_buf;

	qbufp_in(pbf_mem, QBF_MEM_Q_RECYCLE, q_buf);
}

bool mem_cfg_pre(struct di_ch_s *pch)
{
	struct di_mtask *tsk = get_mtask();
	struct dim_fcmd_s *fcmd;
	struct buf_que_s *pbf_blk, *pbf_mem;
	unsigned int index;
//	int i;
	bool ret = true;
	union q_buf_u q_buf;
	bool flg_q;
	struct div2_mm_s *mm;
	struct dim_mm_blk_s *blk_buf;
//	struct di_buf_s *di_buf = NULL;
	unsigned int ch;
//	unsigned int err_cnt = 0;
	unsigned int cnt, cnt_pre, cnt_pst;
	unsigned int length;
//	struct di_dat_s *pdat;

	ch = pch->ch_id;
	mm = dim_mm_get(ch);
	fcmd = &tsk->fcmd[ch];

//	if (fcmd->doing > 0)
//		return false;

	pbf_blk = &pch->blk_qb;
	pbf_mem = &pch->mem_qb;

	/* QBF_BLK_Q_READY -> 'QBF_MEM_Q_GET' */
	cnt = 0;
	cnt_pre	= 0;
	cnt_pst	= 0;
	length = qbufp_count(pbf_blk, QBF_BLK_Q_READY);
	if (length)
		flg_q = true;
	else
		flg_q = false;
	while (flg_q) {
		ret = qbuf_out(pbf_blk, QBF_BLK_Q_READY, &index);
		if (!ret)
			break;
		q_buf = pbf_blk->pbuf[index];
		blk_buf = (struct dim_mm_blk_s *)q_buf.qbc;
		//if (blk_buf->flg.b.page == mm->cfg.ibuf_flg.b.page) {
		if (blk_buf->flg.b.typ == mm->cfg.ibuf_flg.b.typ) {
			ret = qbufp_in(pbf_mem, QBF_MEM_Q_GET_PRE, q_buf);
			cnt_pre++;
		} else if (blk_buf->flg.b.typ == mm->cfg.pbuf_flg.b.typ) {
			ret = qbufp_in(pbf_mem, QBF_MEM_Q_GET_PST, q_buf);
			cnt_pst++;
		}
		if (!ret) {
			PR_ERR("%s:get is overflow\n", __func__);
			break;
		}
		cnt++;
		if (qbuf_is_empty(pbf_blk, QBF_BLK_Q_READY))
			flg_q = false;
		else
			flg_q = true;
	}
	//PR_INF("%s:%d\n", __func__, cnt);
	pch->sts_mem_pre_cfg = cnt;
	return ret;
}

bool mem_cfg_2local(struct di_ch_s *pch)
{
	struct buf_que_s *pbf_mem;
	union q_buf_u q_buf;

	//struct div2_mm_s *mm;
	struct dim_mm_blk_s *blk_buf;
	struct di_buf_s *di_buf = NULL;
	unsigned int ch;
	unsigned int err_cnt = 0;
	unsigned int cnt;
	unsigned int length;

	ch = pch->ch_id;
	//mm = dim_mm_get(ch);

	pbf_mem = &pch->mem_qb;

	/* QBF_MEM_Q_GET -> QBF_MEM_Q_IN_USED*/
	/* local */
	cnt = 0;
	length = qbufp_count(pbf_mem, QBF_MEM_Q_GET_PRE);

	while (length) {
		if (!qbufp_out(pbf_mem, QBF_MEM_Q_GET_PRE, &q_buf)) {
			PR_ERR("%s:local:%d\n", __func__, cnt);
			err_cnt++;
			break;
		}

		/* cfg mem */
		di_buf = di_que_out_to_di_buf(ch, QUE_PRE_NO_BUF);
		if (!di_buf) {
			qbufp_in(pbf_mem, QBF_MEM_Q_GET_PRE, q_buf);
			PR_ERR("%s:local no pre_no_buf[%d]\n", __func__, cnt);
			err_cnt++;
			break;
		}
		blk_buf = (struct dim_mm_blk_s *)q_buf.qbc;
		di_buf->blk_buf = blk_buf;
		//di_buf->nr_adr = blk_buf->mem_start;
		//di_buf->afbc_adr = blk_buf->mem_start;
		//di_buf->afbct_adr = blk_buf->mem_start;
		di_buf->adr_start = blk_buf->mem_start;
		di_buf->buf_is_i = 1;
		di_buf->flg_null = 0;
		dim_buf_set_addr(ch, di_buf);
		di_buf->jiff = jiffies;
		/*  to in used */
		qbufp_in(pbf_mem, QBF_MEM_Q_IN_USED, q_buf);
		queue_in(ch, di_buf, QUEUE_LOCAL_FREE);

		//di_que_in(ch, QUE_PRE_NO_BUF_WAIT, di_buf);
		cnt++;
		length--;
	}
	//PR_INF("%s: [%d]\n", __func__, cnt);
	pch->sts_mem_2_local = cnt;

	if (err_cnt)
		return false;
	return true;
}

bool mem_cfg_2pst(struct di_ch_s *pch)
{
	struct buf_que_s *pbf_mem;
	union q_buf_u q_buf;

	//struct div2_mm_s *mm;
	struct dim_mm_blk_s *blk_buf;
	struct di_buf_s *di_buf = NULL;
	unsigned int ch;
	unsigned int err_cnt = 0;
	unsigned int cnt;
	unsigned int length_pst;

	ch = pch->ch_id;
	pbf_mem = &pch->mem_qb;

	length_pst = qbufp_count(pbf_mem, QBF_MEM_Q_GET_PST);

	/* post */
	cnt = 0;
	while (length_pst) {
		if (!qbufp_out(pbf_mem, QBF_MEM_Q_GET_PST, &q_buf)) {
			PR_ERR("%s:pst:%d\n", __func__, cnt);
			err_cnt++;
			break;
		}

		/* cfg mem */
		di_buf = di_que_out_to_di_buf(ch, QUE_PST_NO_BUF);
		if (!di_buf) {
			qbufp_in(pbf_mem, QBF_MEM_Q_GET_PST, q_buf);
			PR_ERR("%s:no pst_no_buf[%d]\n", __func__, cnt);
			err_cnt++;
			break;
		}
		blk_buf = (struct dim_mm_blk_s *)q_buf.qbc;
		di_buf->blk_buf = blk_buf;
		//di_buf->nr_adr = blk_buf->mem_start;
		//di_buf->afbc_adr = blk_buf->mem_start;
		//di_buf->afbct_adr = blk_buf->mem_start;
		di_buf->adr_start = blk_buf->mem_start;
		di_buf->buf_is_i = 0;
		di_buf->flg_null = 0;
		dim_buf_set_addr(ch, di_buf);
		di_buf->flg_nr = 0;
		di_buf->flg_nv21 = 0;
		di_buf->jiff = jiffies;
		dim_print("nv21 clear %s:%px:\n", __func__, di_buf);

		/*  to in used */
		qbufp_in(pbf_mem, QBF_MEM_Q_IN_USED, q_buf);
		//di_que_in(ch, QUE_POST_FREE, di_buf);
		if (dim_blk_tvp_is_out(blk_buf)) /* new interface */
			di_que_in(ch, QUE_PST_NO_BUF_WAIT, di_buf);
		else if (dim_blk_tvp_is_sct(blk_buf))
			di_que_in(ch, QUE_PST_NO_BUF_WAIT, di_buf);
		else
			di_que_in(ch, QUE_POST_FREE, di_buf);
		cnt++;
		length_pst--;
	}

	//PR_INF("%s: pst[%d]\n", __func__, cnt);
	pch->sts_mem_2_pst = cnt;
	if (err_cnt)
		return false;
	return true;
}

void di_buf_no2wait(struct di_ch_s *pch)
{
	unsigned int len, ch;
	int i;
	struct di_buf_s *di_buf;

	ch = pch->ch_id;
	len = di_que_list_count(ch, QUE_PST_NO_BUF);
	if (!len)
		return;
	if (len > 11)
		len = 11;
	for (i = 0; i < len; i++) {
		di_buf = di_que_out_to_di_buf(ch, QUE_PST_NO_BUF);
		di_que_in(ch, QUE_PST_NO_BUF_WAIT, di_buf);
	}

	PR_INF("%s:%d\n", __func__, len);
}

static unsigned int cnt_out_buffer_h_size(struct di_ch_s *pch, unsigned int cvs_h)
{
	unsigned int out_format;
	unsigned int buf_hsize = 0;

	if (!cfgg(LINEAR) || !dip_itf_is_ins_exbuf(pch))
		return 0;
	out_format = pch->itf.u.dinst.parm.output_format & 0xffff;
	if (out_format == DI_OUTPUT_NV12 ||
	    out_format == DI_OUTPUT_NV21) {
		buf_hsize = cvs_h;
	} else if (out_format == DI_OUTPUT_422) {
		buf_hsize = cvs_h * 2 / 5;
	} else {
		PR_ERR("%s:format not support!%d\n", __func__, out_format);
	}
	dbg_ic("%s:buf_hsize=%d\n", __func__, buf_hsize);

	return buf_hsize;
}

/*ary note: for new interface out buffer */
bool mem_cfg_pst(struct di_ch_s *pch)
{
	unsigned int len, ch, len_pst;
	struct di_buf_s *di_buf;
	struct di_buffer *buffer;
	struct div2_mm_s *mm;

//	dbg_dt("%s:1\n", __func__);
	if (!dip_itf_is_ins_exbuf(pch)) {
//		dbg_dt("%s:5\n", __func__);
		return false;
	}
	ch = pch->ch_id;

	len = di_que_list_count(ch, QUE_PST_NO_BUF_WAIT);
	dbg_dt("%s:2[%d]\n", __func__, len);
	if (!len)
		return false;

	len_pst = npst_cnt(pch);
	dbg_dt("%s:3[%d]\n", __func__, len_pst);
	if (!len_pst)
		return false;
	dbg_dt("%s:4\n", __func__);

	di_buf = di_que_out_to_di_buf(ch, QUE_PST_NO_BUF_WAIT);
	buffer = npst_qout(pch);
	if (di_buf->blk_buf) {
		//@ary_note todo
		PR_ERR("%s:have blk?", __func__);
	} else {
		/* @ary_note: */
		di_buf->c.buffer = buffer;
		di_buf->adr_start = buffer->vf->canvas0_config[0].phy_addr;
		di_buf->nr_adr	= di_buf->adr_start;
		/* h_size */
		di_buf->buf_hsize =
		cnt_out_buffer_h_size(pch,
				      buffer->vf->canvas0_config[0].width);
	}
	di_buf->canvas_config_flag = 1;
	mm = dim_mm_get(ch);
	di_buf->canvas_width[NR_CANVAS]	= mm->cfg.pst_cvs_w;
	di_buf->flg_null = 0;
	pch->sum_ext_buf_in++;
	di_que_in(ch, QUE_POST_FREE, di_buf);
	//dbg_log_pst_buffer(di_buf, 1);
	return true;
}

void mem_resize_buf(struct di_ch_s *pch, struct di_buf_s *di_buf)
{
	struct div2_mm_s *mm;
	struct di_buffer *buf;

	//struct vframe_s *vfm;

	if (dip_itf_is_ins_exbuf(pch)) {
		buf = (struct di_buffer *)di_buf->c.buffer;
		if (buf && buf->vf) {
			di_buf->canvas_width[NR_CANVAS] = buf->vf->canvas0_config[0].width;
			return;
		}
		PR_ERR("%s:\n", __func__);
	}

	mm = dim_mm_get(pch->ch_id);
	di_buf->canvas_width[NR_CANVAS]	= mm->cfg.pst_cvs_w;
}

bool mem_cfg(struct di_ch_s *pch)
{
	struct di_mtask *tsk = get_mtask();
	struct dim_fcmd_s *fcmd;
	struct buf_que_s *pbf_blk, *pbf_mem;
//	unsigned int index;
//	int i;
//	bool ret;
	union q_buf_u q_buf;
//	bool flg_q;
	struct div2_mm_s *mm;
	struct dim_mm_blk_s *blk_buf;
	struct di_buf_s *di_buf = NULL;
	unsigned int ch;
	unsigned int err_cnt = 0;
	unsigned int cnt;
	unsigned int length, length_pst;

	ch = pch->ch_id;
	mm = dim_mm_get(ch);
	fcmd = &tsk->fcmd[ch];

	if (atomic_read(&fcmd->doing) > 0)
		return false;

	pbf_blk = &pch->blk_qb;
	pbf_mem = &pch->mem_qb;

	/* QBF_BLK_Q_READY -> 'QBF_MEM_Q_GET' */
	cnt = 0;
	#ifdef HIS_CODE /*move to pre*/
	length = qbufp_count(pbf_blk, QBF_BLK_Q_READY);
	if (length)
		flg_q = true;
	else
		flg_q = false;
	while (flg_q) {
		ret = qbuf_out(pbf_blk, QBF_BLK_Q_READY, &index);
		if (!ret)
			break;
		q_buf = pbf_blk->pbuf[index];
		blk_buf = (struct dim_mm_blk_s *)q_buf.qbc;
		if (blk_buf->flg.b.page == mm->cfg.ibuf_flg.b.page)
			ret = qbufp_in(pbf_mem, QBF_MEM_Q_GET_PRE, q_buf);
		else
			ret = qbufp_in(pbf_mem, QBF_MEM_Q_GET_PST, q_buf);

		if (!ret) {
			PR_ERR("%s:get is overflow\n", __func__);
			break;
		}
		cnt++;
		if (qbuf_is_empty(pbf_blk, QBF_BLK_Q_READY))
			flg_q = false;
		else
			flg_q = true;
	}
	#endif
	length_pst = qbufp_count(pbf_mem, QBF_MEM_Q_GET_PST);
	dbg_mem2("%s:ch[%d] ready[%d]\n", __func__, ch, cnt);
	dbg_mem2("\t:pre:[%d],pst[%d]\n",
		 qbufp_count(pbf_mem, QBF_MEM_Q_GET_PRE),
		 length_pst);
	//qbufp_list(pbf_mem, QBF_MEM_Q_GET_PRE);
	//qbufp_list(pbf_mem, QBF_MEM_Q_GET_PST);

	/* QBF_MEM_Q_GET -> QBF_MEM_Q_IN_USED*/
	/* local */
	cnt = 0;
	length = qbufp_count(pbf_mem, QBF_MEM_Q_GET_PRE);

	while (length) {
		if (!qbufp_out(pbf_mem, QBF_MEM_Q_GET_PRE, &q_buf)) {
			PR_ERR("%s:local:%d\n", __func__, cnt);
			err_cnt++;
			break;
		}

		/* cfg mem */
		di_buf = di_que_out_to_di_buf(ch, QUE_PRE_NO_BUF);
		if (!di_buf) {
			qbufp_in(pbf_mem, QBF_MEM_Q_GET_PRE, q_buf);
			PR_ERR("%s:local no pre_no_buf[%d]\n", __func__, cnt);
			err_cnt++;
			break;
		}
		blk_buf = (struct dim_mm_blk_s *)q_buf.qbc;
		di_buf->blk_buf = blk_buf;
		//di_buf->nr_adr = blk_buf->mem_start;
		//di_buf->afbc_adr = blk_buf->mem_start;
		//di_buf->afbct_adr = blk_buf->mem_start;
		di_buf->adr_start = blk_buf->mem_start;
		di_buf->buf_is_i = 1;
		di_buf->flg_null = 0;
		dim_buf_set_addr(ch, di_buf);
		di_buf->jiff = jiffies;
		/*  to in used */
		qbufp_in(pbf_mem, QBF_MEM_Q_IN_USED, q_buf);
		queue_in(ch, di_buf, QUEUE_LOCAL_FREE);

		//di_que_in(ch, QUE_PRE_NO_BUF_WAIT, di_buf);
		cnt++;
		length--;
	}
	dbg_mem2("%s: pre[%d]\n", __func__, cnt);

	/* post */
	cnt = 0;
	while (length_pst) {
		if (!qbufp_out(pbf_mem, QBF_MEM_Q_GET_PST, &q_buf)) {
			PR_ERR("%s:pst:%d\n", __func__, cnt);
			err_cnt++;
			break;
		}

		/* cfg mem */
		di_buf = di_que_out_to_di_buf(ch, QUE_PST_NO_BUF);
		if (!di_buf) {
			qbufp_in(pbf_mem, QBF_MEM_Q_GET_PST, q_buf);
			PR_ERR("%s:no pst_no_buf[%d]\n", __func__, cnt);
			err_cnt++;
			break;
		}
		blk_buf = (struct dim_mm_blk_s *)q_buf.qbc;
		di_buf->blk_buf = blk_buf;
		//di_buf->nr_adr = blk_buf->mem_start;
		//di_buf->afbc_adr = blk_buf->mem_start;
		//di_buf->afbct_adr = blk_buf->mem_start;
		di_buf->adr_start = blk_buf->mem_start;
		di_buf->buf_is_i = 0;
		di_buf->flg_null = 0;
		dim_buf_set_addr(ch, di_buf);
		di_buf->flg_nr = 0;
		di_buf->flg_nv21 = 0;
		di_buf->jiff = jiffies;
		dim_print("nv21 clear %s:%px:\n", __func__, di_buf);

		/*  to in used */
		qbufp_in(pbf_mem, QBF_MEM_Q_IN_USED, q_buf);
		//di_que_in(ch, QUE_POST_FREE, di_buf);
		if (dim_blk_tvp_is_out(blk_buf)) /* new interface */
			di_que_in(ch, QUE_PST_NO_BUF_WAIT, di_buf);
		else if (dim_blk_tvp_is_sct(blk_buf))
			di_que_in(ch, QUE_PST_NO_BUF_WAIT, di_buf);
		else
			di_que_in(ch, QUE_POST_FREE, di_buf);
		cnt++;
		length_pst--;
	}
	dbg_mem2("%s: pst[%d]\n", __func__, cnt);

	if (err_cnt)
		return false;
	return true;
}

#ifdef AFBC_DBG
static unsigned int sleep_cnt;
module_param(sleep_cnt, uint, 0664);
MODULE_PARM_DESC(sleep_cnt, "debug sleep_cnt");
#endif

/************************************************
 * mem_cfg_realloc for old flow
 *	QBF_BLK_Q_READY -> QBF_MEM_Q_GET_PST
 *	QBF_MEM_Q_GET_PST -> QBF_MEM_Q_IN_USED
 *	QBF_MEM_Q_GET_PST
 *	QUE_PST_NO_BUF -> QUE_POST_FREE
 ************************************************/
bool mem_cfg_realloc(struct di_ch_s *pch) /*temp for re-alloc mem*/
{
	struct di_mtask *tsk = get_mtask();
	struct dim_fcmd_s *fcmd;
	struct buf_que_s *pbf_blk, *pbf_mem;
	unsigned int index;
//	int i;
	bool ret;
	union q_buf_u q_buf;
	bool flg_q;
	struct div2_mm_s *mm = dim_mm_get(pch->ch_id);
	struct dim_mm_blk_s *blk_buf;
	struct di_buf_s *di_buf = NULL;
	unsigned int ch;
	unsigned int length;
	unsigned int cnt;
	unsigned int err_cnt = 0;

	ch = pch->ch_id;
	fcmd = &tsk->fcmd[pch->ch_id];

	if (atomic_read(&fcmd->doing) > 0)
		return false;

	pbf_blk = &pch->blk_qb;
	pbf_mem = &pch->mem_qb;

	//length = di_que_list_count(ch, QUE_PST_NO_BUF);
	length = qbufp_count(pbf_blk, QBF_BLK_Q_READY);

	if (!length)
		return false;

	/* QBF_BLK_Q_READY -> 'QBF_MEM_Q_GET' */
	cnt = 0;
	do {
		ret = qbuf_out(pbf_blk, QBF_BLK_Q_READY, &index);
		if (!ret)
			break;
		q_buf = pbf_blk->pbuf[index];
		blk_buf = (struct dim_mm_blk_s *)q_buf.qbc;
		if (blk_buf->flg.b.typ == mm->cfg.ibuf_flg.b.typ)
			ret = qbufp_in(pbf_mem, QBF_MEM_Q_GET_PRE, q_buf);
		else if (blk_buf->flg.b.typ == mm->cfg.pbuf_flg.b.typ)
			ret = qbufp_in(pbf_mem, QBF_MEM_Q_GET_PST, q_buf);
		else if (blk_buf->flg.b.typ == mm->cfg.dat_pafbct_flg.b.typ)
			PR_ERR("%s:pafbct?\n", __func__);
		else
		/*if (blk_buf->flg.b.typ == mm->cfg.dat_idat_flg.b.typ)*/
			PR_ERR("%s:%d?\n", __func__, blk_buf->flg.b.typ);

		if (!ret) {
			PR_ERR("%s:get is overflow\n", __func__);
			break;
		}
		cnt++;
		if (qbuf_is_empty(pbf_blk, QBF_BLK_Q_READY))
			flg_q = false;
		else
			flg_q = true;

	} while (flg_q);
	dbg_mem2("%s:ready [%d]\n", __func__, cnt);
	/* QBF_MEM_Q_GET -> QBF_MEM_Q_IN_USED*/
	/* local */
	length = qbufp_count(pbf_mem, QBF_MEM_Q_GET_PRE);
	cnt = 0;
	while (length) {
		if (!qbufp_out(pbf_mem, QBF_MEM_Q_GET_PRE, &q_buf)) {
			PR_ERR("%s:local:%d\n", __func__, cnt);
			err_cnt++;
			break;
		}

		/* cfg mem */
		di_buf = di_que_out_to_di_buf(ch, QUE_PRE_NO_BUF);
		if (!di_buf) {
			qbufp_in(pbf_mem, QBF_MEM_Q_GET_PRE, q_buf);
			PR_ERR("%s:local no pre_no_buf[%d]\n", __func__, cnt);
			err_cnt++;
			break;
		}
		blk_buf = (struct dim_mm_blk_s *)q_buf.qbc;
		di_buf->blk_buf = blk_buf;
		//di_buf->nr_adr = blk_buf->mem_start;
		//di_buf->afbc_adr = blk_buf->mem_start;
		//di_buf->afbct_adr = blk_buf->mem_start;
		di_buf->adr_start = blk_buf->mem_start;
		di_buf->buf_is_i = 1;
		di_buf->flg_null = 0;
		dbg_mem2("cfg:buf t[%d]ind[%d]:blk[%d][0x%lx]\n",
			 di_buf->type,
			 di_buf->index,
			 blk_buf->header.index,
			 blk_buf->mem_start);
		dim_buf_set_addr(ch, di_buf);
		di_buf->jiff = jiffies;

		/*  to in used */
		qbufp_in(pbf_mem, QBF_MEM_Q_IN_USED, q_buf);

		queue_in(ch, di_buf, QUEUE_LOCAL_FREE);
		//di_que_in(ch, QUE_PRE_NO_BUF_WAIT, di_buf);
		cnt++;
		length--;
	}
	dbg_mem2("%s: pre[%d]\n", __func__, cnt);

	/* post */
	cnt = 0;
	length = qbufp_count(pbf_mem, QBF_MEM_Q_GET_PST);
	while (length) {
		if (!qbufp_out(pbf_mem, QBF_MEM_Q_GET_PST, &q_buf)) {
			PR_ERR("%s:local:%d\n", __func__, cnt);
			break;
		}

		/* cfg mem */
		di_buf = di_que_out_to_di_buf(ch, QUE_PST_NO_BUF);
		if (!di_buf) {
			qbufp_in(pbf_mem, QBF_MEM_Q_GET_PST, q_buf);
			PR_ERR("%s:local no pst_no_buf[%d]\n", __func__, cnt);
			err_cnt++;
			break;
		}
		blk_buf = (struct dim_mm_blk_s *)q_buf.qbc;
		di_buf->blk_buf = blk_buf;
		//di_buf->nr_adr = blk_buf->mem_start;
		//di_buf->afbc_adr = blk_buf->mem_start;
		//di_buf->afbct_adr = blk_buf->mem_start;
		di_buf->adr_start	= blk_buf->mem_start;
		di_buf->buf_is_i = 0;
		di_buf->flg_null = 0;
		//crash: msleep(200);
		dbg_mem2("cfg:buf t[%d]ind[%d]:blk[%d][0x%lx]\n",
			 di_buf->type,
			 di_buf->index,
			 blk_buf->header.index,
			 blk_buf->mem_start);
		dim_buf_set_addr(ch, di_buf);
		di_buf->jiff = jiffies;
		#ifdef AFBC_DBG
		if (sleep_cnt)
			msleep(sleep_cnt);// a ok :
		#endif
		di_buf->flg_nr = 0;
		di_buf->flg_nv21 = 0;
		dim_print("nv21 clear %s:%px:\n", __func__, di_buf);
		//msleep(200);// a ok :
		//PR_INF("sleep100ms");
		/*  to in used */
		qbufp_in(pbf_mem, QBF_MEM_Q_IN_USED, q_buf);
		//di_que_in(ch, QUE_POST_FREE, di_buf);
		if (blk_buf->flg.b.typ == EDIM_BLK_TYP_PSCT)
			di_que_in(ch, QUE_PST_NO_BUF_WAIT, di_buf);
		else
			di_que_in(ch, QUE_POST_FREE, di_buf);
		cnt++;
		length--;
	}
	dbg_mem2("%s: cfg ok [%d]\n", __func__, cnt);

	if (err_cnt)
		return false;
	return true;
}

void mem_release_one_inused(struct di_ch_s *pch, struct dim_mm_blk_s *blk_buf)
{
	struct buf_que_s *pbf_blk, *pbf_mem;
	bool ret;
	union q_buf_u ubuf;
	struct dim_sct_s *sct;
	bool f_sctkeep = false;
	struct dim_pat_s *pat_buf;

	if (!blk_buf) {
		PR_WARN("%s:none\n", __func__);
		return;
	}

	if (blk_buf->sct_keep != 0xff) {
		codec_mm_keeper_unmask_keeper(blk_buf->sct_keep, 1);
		PR_INF("%s:sct_keep[0x%x]\n", "release", blk_buf->sct_keep);
		blk_buf->sct_keep = 0xff;
		f_sctkeep = true;
	}

	if (blk_buf->sct) {
		sct  = (struct dim_sct_s *)blk_buf->sct;
		sct->pat_buf->flg_mode = 0;
		qsct_used_some_to_recycle(pch, sct);
		blk_buf->sct	= NULL;
		blk_buf->pat_buf = NULL;
	} else if (blk_buf->pat_buf) {
		/* recycl pat*/
		pat_buf = (struct dim_pat_s *)blk_buf->pat_buf;
		if (blk_buf->sct_keep) {
			pat_buf->flg_mode = 0;
			pat_release_vaddr(pat_buf);
		}
		qpat_in_ready(pch, pat_buf);
		blk_buf->pat_buf = NULL;
	}
	pbf_blk = &pch->blk_qb;
	pbf_mem = &pch->mem_qb;

	ubuf.qbc = &blk_buf->header;

	ret = qbufp_out_some(pbf_mem, QBF_MEM_Q_IN_USED, ubuf);
	if (!ret) {
		PR_WARN("%s:no [%d] in in_used\n",
			__func__, blk_buf->header.index);
		dump_stack();
//		dbg_q_listid_print(pbf_blk);
		return;
	}

	//qbuf_in(pbf_blk, QBF_BLK_Q_RCYCLE2, blk_buf->header.index);
	qbufp_in(pbf_mem, QBF_MEM_Q_RECYCLE, ubuf);
}

/* no keep buf*/
void mem_release_all_inused(struct di_ch_s *pch)
{
	struct buf_que_s *pbf_blk, *pbf_mem;
	unsigned int cnt;
	bool ret;
	union q_buf_u bufq;
	unsigned int length;

	pbf_blk = &pch->blk_qb;
	pbf_mem = &pch->mem_qb;

	/*QBF_MEM_Q_IN_USED -> out:QBF_BLK_Q_RCYCLE2*/
	length = qbufp_count(pbf_mem, QBF_MEM_Q_IN_USED);
	if (length) {
		cnt	= 0;
		do {
			ret = qbufp_out(pbf_mem, QBF_MEM_Q_IN_USED, &bufq);
			if (!ret)
				break;

			//qbufp_in(pbf_blk, QBF_BLK_Q_RCYCLE2, bufq);
			qbufp_in(pbf_mem, QBF_MEM_Q_RECYCLE, bufq);
			cnt++;
			if (qbuf_is_empty(pbf_mem, QBF_MEM_Q_IN_USED))
				break;
		} while (cnt < length);
		dbg_mem2("%s:in_used[%d] release[%d]\n", __func__, length, cnt);
	}
}

/* @ary_note: only one caller, release all keep back buffer */
/* return: release nub */
unsigned int mem_release_keep_back(struct di_ch_s *pch)
{
//	struct buf_que_s *pbf_blk, *pbf_mem;

//	unsigned int tmpa[MAX_FIFO_SIZE];
//	unsigned int psize, itmp;
//	struct di_buf_s *p;
	//struct di_dev_s *de_devp = get_dim_de_devp();
//	int retr;
//	ulong flags = 0;
	unsigned int cnt = 0;
//	unsigned int ch;
	unsigned int len_keep;
	int i;
	struct qs_cls_s	*pq;
	bool ret;
	union q_buf_u pbuf;
	struct dim_ndis_s *ndis;

	pq = &pch->ndis_que_kback;

	len_keep = pq->ops.count(NULL, pq);
	if (!len_keep)
		return 0;
	for (i = 0; i < len_keep; i++) {
		ret = pq->ops.out(NULL, pq, &pbuf);
		if (!ret) {
			PR_ERR("%s:o err:%d\n", __func__, i);
			continue;
		}
		ndis = (struct dim_ndis_s *)pbuf.qbc;
		if (!ndis) {
			PR_ERR("%s:no ndis ?:%d\n", __func__, i);
			continue;
		}
		if (!ndis->c.blk) {
			PR_ERR("%s:no ndis blk?:%d\n", __func__, i);
			continue;
		}
		dbg_keep("%s:keep back out %d\n", __func__, ndis->header.index);

		mem_release_one_inused(pch, ndis->c.blk);
		ndis_move_keep2idle(pch, ndis);
		cnt++;
	}
	dbg_mem2("%s:[%d]\n", __func__, cnt);
	return cnt;
}

/* @ary_note: process keep back buffer when ready */
/* @ary_note: only one caller */
/* @ary_note: */
void dim_post_keep_back_recycle(struct di_ch_s *pch)
{
	unsigned int cnt;
	struct div2_mm_s *mm = dim_mm_get(pch->ch_id);

	cnt = mem_release_keep_back(pch);
	mm->sts.flg_realloc += cnt;
}

/* @ary_note: unreg buf only */
void mem_release(struct di_ch_s *pch, struct dim_mm_blk_s **blks, unsigned int blk_nub)
{
	//unsigned int tmpa[MAX_FIFO_SIZE];
	//unsigned int psize, itmp;
	//struct di_buf_s *p;
	unsigned int blk_mst;
	unsigned int pat_mst;
	unsigned int ch = pch->ch_id;

	struct buf_que_s *pbf_blk, *pbf_mem;
	unsigned int cnt, cntr;
	bool ret;
	union q_buf_u bufq;
	unsigned int length;
//	unsigned int ch = pch->ch_id;
	struct di_buf_s *pbuf_local;
	struct div2_mm_s *mm = dim_mm_get(ch);
	int i;
	struct di_buf_s *di_buf;
	struct dim_pat_s *pat_buf;
	struct dim_mm_blk_s *blk_buf;
	struct dim_sct_s *sct;

	/* clear local */
	pbuf_local = get_buf_local(ch);
	for (i = 0; i < mm->cfg.num_local; i++) {
		di_buf = &pbuf_local[i];
		if (di_buf->mcinfo_alloc_flg)
			dim_mcinfo_v_release(di_buf);
	}
//	#endif
	/***********/
	/* mask blk */
	blk_mst = 0;
	pat_mst = 0;
	//di_que_list(ch, QUE_POST_KEEP, &tmpa[0], &psize);
	//dbg_keep("post_keep: curr(%d)\n", psize);

	for (i = 0; i < blk_nub; i++) {
		blk_buf = blks[i];
		if (!blk_buf) {
			PR_ERR("%s:null %d\n", __func__, i);
			break;
		}
		bset(&blk_mst, blk_buf->header.index);
		/* 2010-10-05 */
		if (blk_buf->pat_buf) {
			pat_buf = (struct dim_pat_s *)blk_buf->pat_buf;
			bset(&pat_mst, pat_buf->header.index);
		}
		/* sct */
		if (blk_buf->sct) {
			sct = (struct dim_sct_s *)blk_buf->sct;
			blk_buf->sct_keep = sct_keep(pch, sct);
			sct->flg_act.b.tab_keep = 1;
			qsct_used_some_to_recycle(pch, sct);
			blk_buf->sct = NULL;
		}
	}

	qpat_in_ready_msk(pch, pat_mst);
	//dbg_mem2("%s:blk_mst[0x%x]\n", __func__, blk_mst);
	//PR_INF("%s:blk_mst[0x%x]\n", __func__, blk_mst);
	//PR_INF("%s:pat_mst[0x%x]\n", __func__, pat_mst);
	pch->sts_unreg_blk_msk = blk_mst;
	pch->sts_unreg_pat_mst = pat_mst;
	pbf_blk = &pch->blk_qb;
	pbf_mem = &pch->mem_qb;
	/*QBF_MEM_Q_GET_PRE -> out:QBF_BLK_Q_RCYCLE2*/

	if (!qbuf_is_empty(pbf_mem, QBF_MEM_Q_GET_PRE)) {
		cnt = 0;
		do {
			ret = qbufp_out(pbf_mem, QBF_MEM_Q_GET_PRE, &bufq);
			if (!ret)
				break;
			//qbufp_in(pbf_blk, QBF_BLK_Q_RCYCLE2, bufq);
			qbufp_in(pbf_mem, QBF_MEM_Q_RECYCLE, bufq);
			cnt++;

			if (qbuf_is_empty(pbf_mem, QBF_MEM_Q_GET_PRE))
				break;
		} while (cnt < MAX_FIFO_SIZE);
		dbg_mem2("%s:cnt get_pre %d\n", __func__, cnt);
	}
	/*QBF_MEM_Q_GET_PST -> out:QBF_BLK_Q_RCYCLE2*/
	if (!qbuf_is_empty(pbf_mem, QBF_MEM_Q_GET_PST)) {
		cnt = 0;
		do {
			ret = qbufp_out(pbf_mem, QBF_MEM_Q_GET_PST, &bufq);
			if (!ret)
				break;
			//qbufp_in(pbf_blk, QBF_BLK_Q_RCYCLE2, bufq);
			qbufp_in(pbf_mem, QBF_MEM_Q_RECYCLE, bufq);
			cnt++;

			if (qbuf_is_empty(pbf_mem, QBF_MEM_Q_GET_PST))
				break;
		} while (cnt < MAX_FIFO_SIZE);
		dbg_mem2("%s:cnt get_pre %d\n", __func__, cnt);
	}
	/*QBF_MEM_Q_IN_USED -> out:QBF_BLK_Q_RCYCLE2*/
	length = qbufp_count(pbf_mem, QBF_MEM_Q_IN_USED);
	if (length) {
		cnt	= 0;
		cntr	= 0;
		do {
			ret = qbufp_out(pbf_mem, QBF_MEM_Q_IN_USED, &bufq);
			if (!ret)
				break;
			if (bget(&blk_mst, bufq.qbc->index)) {
				/* keep */
				qbufp_in(pbf_mem, QBF_MEM_Q_IN_USED, bufq);
				cnt++;
				continue;
			}
			//qbufp_in(pbf_blk, QBF_BLK_Q_RCYCLE2, bufq);
			blk_buf = (struct dim_mm_blk_s *)bufq.qbc;
			if (blk_buf->sct) {
				sct  = (struct dim_sct_s *)blk_buf->sct;
				qsct_used_some_to_recycle(pch, sct);
				blk_buf->sct	= NULL;
				blk_buf->pat_buf = NULL;
			}

			qbufp_in(pbf_mem, QBF_MEM_Q_RECYCLE, bufq);
			cnt++;
			cntr++; /*release*/
			if (qbuf_is_empty(pbf_mem, QBF_MEM_Q_IN_USED))
				break;
		} while (cnt < length);
		dbg_mem2("%s:in_used[%d]release[%d]\n", __func__, length, cntr);
	}
}

bool mem_2_blk(struct di_ch_s *pch)
{
	struct buf_que_s *pbf_blk, *pbf_mem;
	bool ret;
	union q_buf_u bufq;
	unsigned int length, cnt;

	/* QBF_MEM_Q_RECYCLE -> QBF_BLK_Q_RCYCLE2*/
	pbf_mem = &pch->mem_qb;
	pbf_blk = &pch->blk_qb;

	length = qbufp_count(pbf_mem, QBF_MEM_Q_RECYCLE);
	if (!length)
		return true;

	cnt = 0;
	do {
		ret = qbufp_out(pbf_mem, QBF_MEM_Q_RECYCLE, &bufq);
		if (!ret)
			break;
		qbufp_in(pbf_blk, QBF_BLK_Q_RCYCLE2, bufq);
		//qbufp_in(pbf_mem, QBF_MEM_Q_RECYCLE, bufq);
		cnt++;

		if (qbuf_is_empty(pbf_mem, QBF_MEM_Q_RECYCLE))
			break;
	} while (cnt < length);
	dbg_mem2("%s:cnt %d\n", __func__, cnt);

	return true;
}

unsigned int mem_release_sct_wait(struct di_ch_s *pch)
{
	struct di_buf_s *p = NULL;
	unsigned int ch;
	unsigned int length, rls_pst;

	ch = pch->ch_id;
	/* post */
	length = di_que_list_count(ch, QUE_PST_NO_BUF_WAIT);
	rls_pst = length;
	dbg_mem2("%s: pst free[%d]\n", __func__, rls_pst);
	while (length) {
		p = di_que_out_to_di_buf(ch, QUE_PST_NO_BUF_WAIT);
		if (!p) {
			PR_ERR("%s:post out:%d\n", __func__, length);
			break;
		}

		mem_release_one_inused(pch, p->blk_buf);
		dbg_mem2("%s:pst:[%d]\n", __func__, p->index);
		p->blk_buf = NULL;
		di_que_in(ch, QUE_PST_NO_BUF, p);
		length--;
	}
	return rls_pst;
}

unsigned int mem_release_free(struct di_ch_s *pch)
{
	struct di_buf_s *p = NULL;
	//int itmp, i;
	unsigned int ch;

	unsigned int length, rls_pst;

	ch = pch->ch_id;

	/* pre */
	length = list_count(ch, QUEUE_LOCAL_FREE);
	while (length) {
		p = get_di_buf_head(ch, QUEUE_LOCAL_FREE);
		if (!p) {
			PR_ERR("%s:length[%d]\n", __func__, length);
			break;
		}
		dbg_mem2("index %2d, 0x%p, type %d\n", p->index, p, p->type);
		queue_out(ch, p);
		if (p->iat_buf) {
			qiat_in_ready(pch, (struct dim_iat_s *)p->iat_buf);
			p->iat_buf = NULL;
		}
		mem_release_one_inused(pch, p->blk_buf);
		p->blk_buf = NULL;
		di_que_in(ch, QUE_PRE_NO_BUF, p);
		length--;
	}
	/* post */
	length = di_que_list_count(ch, QUE_POST_FREE);
	rls_pst = length;
	dbg_mem2("%s: pst free[%d]\n", __func__, rls_pst);
	while (length) {
		p = di_que_out_to_di_buf(ch, QUE_POST_FREE);
		if (!p) {
			PR_ERR("%s:post out:%d\n", __func__, length);
			break;
		}
		mem_release_one_inused(pch, p->blk_buf);
		dbg_mem2("%s:pst:[%d]\n", __func__, p->index);
		p->blk_buf = NULL;
		di_que_in(ch, QUE_PST_NO_BUF, p);
		length--;
	}
	return rls_pst;
}

/********************************************/
/* post afbc table */

static const struct que_creat_s qbf_pat_cfg_q[] = {//TST_Q_IN_NUB
	[QBF_PAT_Q_IDLE] = {
		.name	= "QBF_PAT_IDLE",
		.type	= Q_T_FIFO,
		.lock	= 0,
	},
	[QBF_PAT_Q_READY] = {
		.name	= "QBF_PAT_READY",
		.type	= Q_T_FIFO,
		.lock	= 0,
	},
	[QBF_PAT_Q_READY_SCT] = {
		.name	= "QBF_PAT_READY_SCT",
		.type	= Q_T_FIFO,
		.lock	= 0,
	},
	[QBF_PAT_Q_IN_USED] = {
		.name	= "QBF_PAT_Q_IN_USED",
		.type	= Q_T_FIFO,
		.lock	= 0,
	}
};

static const struct qbuf_creat_s qbf_pat_cfg_qbuf = {
	.name	= "qbuf_pat",
	.nub_que	= QBF_PAT_Q_NUB,
	.nub_buf	= DIM_PAT_NUB,
	.code		= CODE_PAT,
};

void bufq_pat_int(struct di_ch_s *pch)
{
	struct dim_pat_s *pat_buf;
	struct buf_que_s *pbufq;
	int i;

	if (!pch) {
		PR_ERR("%s:\n", __func__);
		return;
	}

	/* clear buf*/
	pat_buf = &pch->pat_bf[0];
	memset(pat_buf, 0, sizeof(*pat_buf) * DIM_PAT_NUB);

	/* set buf's header */
	for (i = 0; i < DIM_PAT_NUB; i++) {
		pat_buf = &pch->pat_bf[i];

		pat_buf->header.code	= CODE_PAT;
		pat_buf->header.index	= i;
		pat_buf->header.ch	= pch->ch_id;
	}

	pbufq = &pch->pat_qb;
	/*clear bq */
	memset(pbufq, 0, sizeof(*pbufq));

	/* point to resource */
	for (i = 0; i < QBF_PAT_Q_NUB; i++)
		pbufq->pque[i] = &pch->pat_q[i];

	for (i = 0; i < DIM_PAT_NUB; i++)
		pbufq->pbuf[i].qbc = &pch->pat_bf[i].header;

	qbuf_int(pbufq, &qbf_pat_cfg_q[0], &qbf_pat_cfg_qbuf);

	/* all to idle */
	qbuf_in_all(pbufq, QBF_PAT_Q_IDLE);
}

void bufq_pat_exit(struct di_ch_s *pch)
{
	struct buf_que_s *pbufq;

	PR_INF("%s:\n", __func__);
	if (!pch) {
		PR_ERR("%s:\n", __func__);
		return;
	}
	pbufq = &pch->pat_qb;

	qbuf_release_que(pbufq);
}

bool qpat_idle_to_ready(struct di_ch_s *pch, unsigned long addr)
{
	unsigned int index;
	bool ret;
	struct buf_que_s *pbufq;
	union q_buf_u q_buf;
	struct dim_pat_s *pat_buf;

	pbufq = &pch->pat_qb;

	ret = qbuf_out(pbufq, QBF_PAT_Q_IDLE, &index);
	if (!ret)
		return false;

	q_buf = pbufq->pbuf[index];
	pat_buf = (struct dim_pat_s *)q_buf.qbc;

	pat_buf->mem_start = addr;

	qbuf_in(pbufq, QBF_PAT_Q_READY, index);

	return true;
}

/* ready to in used */
struct dim_pat_s *qpat_out_ready(struct di_ch_s *pch)
{
	unsigned int index;
	bool ret;
	struct buf_que_s *pbufq;
	union q_buf_u q_buf;
	struct dim_pat_s *pat_buf;

	pbufq = &pch->pat_qb;

	ret = qbuf_out(pbufq, QBF_PAT_Q_READY, &index);
	if (!ret)
		return NULL;
	q_buf = pbufq->pbuf[index];
	pat_buf = (struct dim_pat_s *)q_buf.qbc;
	qbuf_in(pbufq, QBF_PAT_Q_IN_USED, index);

	return pat_buf;
}

/* ready to in ready_sct ???*/
struct dim_pat_s *qpat_out_ready_sct(struct di_ch_s *pch)
{
	unsigned int index;
	bool ret;
	struct buf_que_s *pbufq;
	union q_buf_u q_buf;
	struct dim_pat_s *pat_buf;

	pbufq = &pch->pat_qb;

	ret = qbuf_out(pbufq, QBF_PAT_Q_READY, &index);
	if (!ret)
		return NULL;
	q_buf = pbufq->pbuf[index];
	pat_buf = (struct dim_pat_s *)q_buf.qbc;
	qbuf_in(pbufq, QBF_PAT_Q_IN_USED, index);

	return pat_buf;
}

/* in_used to ready*/
bool qpat_in_ready(struct di_ch_s *pch, struct dim_pat_s *pat_buf)
{
//	unsigned int index;
	bool ret;
	struct buf_que_s *pbufq;
	union q_buf_u q_buf;
	//struct dim_pat_s *pat_buf;

	pbufq = &pch->pat_qb;
	q_buf.qbc = &pat_buf->header;

	ret = qbuf_out_some(pbufq, QBF_PAT_Q_IN_USED, q_buf);
	if (!ret) {
		PR_ERR("%s:[%d]\n", __func__, pat_buf->header.index);
		dbg_q_listid_print(pbufq);
		dump_stack();
		return false;
	}
	ret = qbuf_in(pbufq, QBF_PAT_Q_READY, pat_buf->header.index);
	if (!ret)
		return false;

	return true;
}

bool qpat_in_ready_msk(struct di_ch_s *pch, unsigned int msk)
{
	unsigned int index;
	bool ret;
	struct buf_que_s *pbufq;
//	union q_buf_u q_buf;
	struct dim_pat_s *pat_buf;
	unsigned int len;
	int i;
	bool err = false;

	pbufq = &pch->pat_qb;

	len = qbufp_count(pbufq, QBF_PAT_Q_IN_USED);
	for (i = 0; i < len; i++) {
		ret = qbuf_out(pbufq, QBF_PAT_Q_IN_USED, &index);
		if (!ret) {
			PR_ERR("%s:%d\n", __func__, i);
			err = true;
			break;
		}
		pat_buf = (struct dim_pat_s *)pbufq->pbuf[index].qbc;
		if (bget(&msk, index)) {
			qbuf_in(pbufq, QBF_PAT_Q_IN_USED,
				pat_buf->header.index);
		} else {
			if (pat_buf->vaddr)
				pat_release_vaddr(pat_buf);
			qbuf_in(pbufq, QBF_PAT_Q_READY,
				pat_buf->header.index);
		}
	}
	dbg_keep("%s:0x%x\n", __func__, msk);
	if (err)
		return false;
	return true;
}

void dbg_pat(struct seq_file *s, struct di_ch_s *pch)
{
	struct buf_que_s *pbufq;
	struct dim_pat_s *pat_buf;
	int i;
	char *splt = "---------------------------";

	pbufq = &pch->pat_qb;

	seq_printf(s, "%s\n", splt);
	for (i = 0; i < DIM_PAT_NUB; i++) {
		pat_buf = (struct dim_pat_s *)pbufq->pbuf[i].qbc;
		seq_printf(s, "%s:%d\n", "idx", i);
		seq_printf(s, "\t%s:0x%lx\n", "mem_start", pat_buf->mem_start);
		if (pat_buf->vaddr)
			seq_printf(s, "\t%s:0x%px\n", "vaddr", pat_buf->vaddr);
		else
			seq_printf(s, "\t%s\n", "vaddr_none");
		seq_printf(s, "\t%s:%d\n", "flg_vmap", pat_buf->flg_vmap);
		seq_printf(s, "\t%s:0x%x\n", "crc", pat_buf->crc);
		seq_printf(s, "%s\n", splt);
	}
}

/********************************************/
/* local buffer exit table */
static const struct que_creat_s qbf_iat_cfg_q[] = {//TST_Q_IN_NUB
	[QBF_IAT_Q_IDLE] = {
		.name	= "QBF_IAT_IDLE",
		.type	= Q_T_FIFO,
		.lock	= 0,
	},
	[QBF_IAT_Q_READY] = {
		.name	= "QBF_IAT_READY",
		.type	= Q_T_FIFO,
		.lock	= 0,
	},
	[QBF_IAT_Q_IN_USED] = {
		.name	= "QBF_IAT_Q_IN_USED",
		.type	= Q_T_FIFO,
		.lock	= 0,
	}
};

static const struct qbuf_creat_s qbf_iat_cfg_qbuf = {
	.name	= "qbuf_iat",
	.nub_que	= QBF_IAT_Q_NUB,
	.nub_buf	= DIM_IAT_NUB,
	.code		= CODE_IAT,
};

void bufq_iat_int(struct di_ch_s *pch)
{
	struct dim_iat_s *iat_buf;
	struct buf_que_s *pbufq;
	int i;

	if (!pch) {
		PR_ERR("%s:\n", __func__);
		return;
	}

	/* clear buf*/
	iat_buf = &pch->iat_bf[0];
	memset(iat_buf, 0, sizeof(*iat_buf) * DIM_IAT_NUB);

	/* set buf's header */
	for (i = 0; i < DIM_IAT_NUB; i++) {
		iat_buf = &pch->iat_bf[i];

		iat_buf->header.code	= CODE_IAT;
		iat_buf->header.index	= i;
		iat_buf->header.ch	= pch->ch_id;
	}

	pbufq = &pch->iat_qb;
	/*clear bq */
	memset(pbufq, 0, sizeof(*pbufq));

	/* point to resource */
	for (i = 0; i < QBF_IAT_Q_NUB; i++)
		pbufq->pque[i] = &pch->iat_q[i];

	for (i = 0; i < DIM_IAT_NUB; i++)
		pbufq->pbuf[i].qbc = &pch->iat_bf[i].header;

	qbuf_int(pbufq, &qbf_iat_cfg_q[0], &qbf_iat_cfg_qbuf);

	/* all to idle */
	qbuf_in_all(pbufq, QBF_IAT_Q_IDLE);
}

void bufq_iat_exit(struct di_ch_s *pch)
{
	struct buf_que_s *pbufq;

	PR_INF("%s:\n", __func__);
	if (!pch) {
		PR_ERR("%s:\n", __func__);
		return;
	}
	pbufq = &pch->iat_qb;

	qbuf_release_que(pbufq);
}

void bufq_iat_rest(struct di_ch_s *pch)
{
//	struct dim_iat_s *iat_buf;
	struct buf_que_s *pbufq;
	int i;

	if (!pch) {
		PR_ERR("%s:\n", __func__);
		return;
	}

	pbufq = &pch->iat_qb;

	for (i = 0; i < QBF_IAT_Q_NUB; i++)
		qbufp_restq(pbufq, i);

	/* all to idle */
	qbuf_in_all(pbufq, QBF_IAT_Q_IDLE);
}

/* idle to ready*/
bool qiat_idle_to_ready(struct di_ch_s *pch,
			struct dim_iat_s **idat)
{
	unsigned int index;
	bool ret;
	struct buf_que_s *pbufq;
	union q_buf_u q_buf;
	struct dim_iat_s *iat_buf;

	pbufq = &pch->iat_qb;

	ret = qbuf_out(pbufq, QBF_IAT_Q_IDLE, &index);
	if (!ret)
		return false;

	q_buf = pbufq->pbuf[index];
	iat_buf = (struct dim_iat_s *)q_buf.qbc;

	//iat_buf->start_idat	= addr;
	//iat_buf->start_afbct	= addr_afbct;
	//iat_buf->start_mc	= addr_mc;
	*idat = iat_buf;
	qbuf_in(pbufq, QBF_IAT_Q_READY, index);

	return true;
}

/* ready to in used */
struct dim_iat_s *qiat_out_ready(struct di_ch_s *pch)
{
	unsigned int index;
	bool ret;
	struct buf_que_s *pbufq;
	union q_buf_u q_buf;
	struct dim_iat_s *iat_buf;

	pbufq = &pch->iat_qb;

	ret = qbuf_out(pbufq, QBF_IAT_Q_READY, &index);
	if (!ret)
		return NULL;
	q_buf = pbufq->pbuf[index];
	iat_buf = (struct dim_iat_s *)q_buf.qbc;
	qbuf_in(pbufq, QBF_IAT_Q_IN_USED, index);

	return iat_buf;
}

/* in_used to ready*/
bool qiat_in_ready(struct di_ch_s *pch, struct dim_iat_s *iat_buf)
{
//	unsigned int index;
	bool ret;
	struct buf_que_s *pbufq;
	union q_buf_u q_buf;
	//struct dim_pat_s *pat_buf;

	pbufq = &pch->iat_qb;
	q_buf.qbc = &iat_buf->header;

	ret = qbuf_out_some(pbufq, QBF_IAT_Q_IN_USED, q_buf);
	if (!ret) {
		PR_ERR("%s:\n", __func__);
		//dbg_hd_print(&iat_buf->header);
		//dbg_q_listid_print(pbufq);
	}
	ret = qbuf_in(pbufq, QBF_IAT_Q_READY, iat_buf->header.index);
	if (!ret)
		return false;

	return true;
}

//add for unreg
bool qiat_all_back2_ready(struct di_ch_s *pch)
{
	unsigned int len;
	struct buf_que_s *pbufq;
	int i;
	unsigned int index;
	union q_buf_u q_buf;

	pbufq = &pch->iat_qb;

	len = qbufp_count(pbufq, QBF_IAT_Q_IN_USED);
	for (i = 0; i < len; i++) {
		qbuf_out(pbufq, QBF_IAT_Q_IN_USED, &index);
		q_buf = pbufq->pbuf[index];
		qbuf_in(pbufq, QBF_IAT_Q_READY, index);
	}
	len = qbufp_count(pbufq, QBF_IAT_Q_READY);
	dbg_reg("%s:len[%d]\n", __func__, len);

	return true;
}

#ifdef MARK_HIS
bool qiat_all_release_mc_v(struct di_ch_s *pch)
{
	unsigned int len;
	struct buf_que_s *pbufq;
	int i;
	unsigned int index;
	union q_buf_u q_buf;
	struct dim_iat_s *iat_buf;

	pbufq = &pch->iat_qb;

	len = qbufp_count(pbufq, QBF_IAT_Q_READY);
	for (i = 0; i < len; i++) {
		qbuf_out(pbufq, QBF_IAT_Q_READY, &index);
		q_buf = pbufq->pbuf[index];
		iat_buf = (struct dim_iat_s *)q_buf.qbc;

		dim_mcinfo_v_release_idat(iat_buf);
		qbuf_in(pbufq, QBF_IAT_Q_IDLE, index);
	}

	return true;
}
#endif

#ifdef MARK_HIS
bool qiat_clear_buf(struct di_ch_s *pch)
{
	//struct buf_que_s *pbf_mem;
	//union q_buf_u bufq;

	/*all in ready*/
	qiat_all_back2_ready(pch);
	/* release mc infor addr v*/
	qiat_all_release_mc_v(pch);

#ifdef MARK_HIS
	/* clear idat*/
	if (pch->rse_ori.dat_i.blk_buf) {
		pbf_mem = &pch->mem_qb;
		bufq.qbc = (struct qs_buf_s *)pch->rse_ori.dat_i.blk_buf;
		qbufp_in(pbf_mem, QBF_MEM_Q_RECYCLE, bufq);
		memset(&pch->rse_ori.dat_i, 0, sizeof(pch->rse_ori.dat_i));
		pch->rse_ori.dat_i.blk_buf = NULL;
	}
#endif
	return true;
}
#endif

/********************************************/
/* sct table */

static const struct que_creat_s qbf_sct_cfg_q[] = {//TST_Q_IN_NUB
	[QBF_SCT_Q_IDLE] = {
		.name	= "QBF_SCT_IDLE",
		.type	= Q_T_FIFO,
		.lock	= 0,
	},
	[QBF_SCT_Q_READY] = {
		.name	= "QBF_SCT_READY",
		.type	= Q_T_FIFO,
		.lock	= 0,
	},
	[QBF_SCT_Q_REQ] = {
		.name	= "QBF_SCT_REQ",
		.type	= Q_T_FIFO,
		.lock	= 0,
	},
	[QBF_SCT_Q_USED] = {
		.name	= "QBF_SCT_Q_USED",
		.type	= Q_T_FIFO,
		.lock	= 0,
	},
	[QBF_SCT_Q_RECYCLE] = {
		.name	= "QBF_SCT_Q_RECYCLE",
		.type	= Q_T_FIFO,
		.lock	= 0,
	},
	[QBF_SCT_Q_KEEP] = {
		.name	= "QBF_SCT_Q_KEEP",
		.type	= Q_T_FIFO,
		.lock	= 0,
	}
};

static const struct qbuf_creat_s qbf_sct_cfg_qbuf = {
	.name	= "qbuf_sct",
	.nub_que	= QBF_SCT_Q_NUB,
	.nub_buf	= DIM_SCT_NUB,
	.code		= CODE_SCT
};

void bufq_sct_int(struct di_ch_s *pch)
{
	struct dim_sct_s *sct_buf;
	struct buf_que_s *pbufq;
	int i;
	unsigned int post_nub, sct_nub;

	if (!pch) {
		PR_ERR("%s:\n", __func__);
		return;
	}

	/* clear buf*/
	sct_buf = &pch->sct_bf[0];
	memset(sct_buf, 0, sizeof(*sct_buf) * DIM_SCT_NUB);

	/* set buf's header */
	for (i = 0; i < DIM_SCT_NUB; i++) {
		sct_buf = &pch->sct_bf[i];

		sct_buf->header.code	= CODE_SCT;
		sct_buf->header.index	= i;
		sct_buf->header.ch	= pch->ch_id;
	}

	pbufq = &pch->sct_qb;
	/*clear bq */
	memset(pbufq, 0, sizeof(*pbufq));

	/* point to resource */
	for (i = 0; i < QBF_SCT_Q_NUB; i++)
		pbufq->pque[i] = &pch->sct_q[i];

	for (i = 0; i < DIM_SCT_NUB; i++)
		pbufq->pbuf[i].qbc = &pch->sct_bf[i].header;

	qbuf_int(pbufq, &qbf_sct_cfg_q[0], &qbf_sct_cfg_qbuf);

	post_nub = cfgg(POST_NUB);
	if ((post_nub) && post_nub < POST_BUF_NUM)
		sct_nub = post_nub;
	else
		sct_nub = DIM_SCT_NUB;

	pbufq->nub_buf = sct_nub;

	/* all to idle */
	qbuf_in_all(pbufq, QBF_SCT_Q_IDLE);/*ary:this number must be total?*/
}

void bufq_sct_exit(struct di_ch_s *pch)
{
	struct buf_que_s *pbufq;

	PR_INF("%s:\n", __func__);
	if (!pch) {
		PR_ERR("%s:\n", __func__);
		return;
	}
	pbufq = &pch->sct_qb;

	qbuf_release_que(pbufq);
}

void bufq_sct_rest(struct di_ch_s *pch)
{
	struct buf_que_s *pbufq;
	int i;

	if (!pch) {
		PR_ERR("%s:\n", __func__);
		return;
	}

	pbufq = &pch->sct_qb;

	for (i = 0; i < QBF_SCT_Q_NUB; i++)
		qbufp_restq(pbufq, i);

	/* all to idle */
	qbuf_in_all(pbufq, QBF_SCT_Q_IDLE);
}

/* idle to req*/
bool qsct_idle_to_req(struct di_ch_s *pch,
			struct dim_sct_s **sct)
{
	unsigned int index;
	bool ret;
	struct buf_que_s *pbufq;
	union q_buf_u q_buf;
	struct dim_sct_s *sct_buf;

	pbufq = &pch->sct_qb;

	ret = qbuf_out(pbufq, QBF_SCT_Q_IDLE, &index);
	if (!ret)
		return false;

	q_buf = pbufq->pbuf[index];
	sct_buf = (struct dim_sct_s *)q_buf.qbc;

	*sct = sct_buf;
	qbuf_in(pbufq, QBF_SCT_Q_REQ, index);

	return true;
}

/* req to ready */
bool qsct_req_to_ready(struct di_ch_s *pch,
			struct dim_sct_s **sct)
{
	unsigned int index;
	bool ret;
	struct buf_que_s *pbufq;
	union q_buf_u q_buf;
	struct dim_sct_s *sct_buf;

	pbufq = &pch->sct_qb;

	ret = qbuf_out(pbufq, QBF_SCT_Q_REQ, &index);
	if (!ret)
		return false;

	q_buf = pbufq->pbuf[index];
	sct_buf = (struct dim_sct_s *)q_buf.qbc;

	*sct = sct_buf;
	qbuf_in(pbufq, QBF_SCT_Q_READY, index);

	return true;
}

/* ready to used */
bool qsct_ready_to_used(struct di_ch_s *pch,
			struct dim_sct_s **sct)
{
	unsigned int index;
	bool ret;
	struct buf_que_s *pbufq;
	union q_buf_u q_buf;
	struct dim_sct_s *sct_buf;

	pbufq = &pch->sct_qb;

	ret = qbuf_out(pbufq, QBF_SCT_Q_READY, &index);
	if (!ret)
		return false;

	q_buf = pbufq->pbuf[index];
	sct_buf = (struct dim_sct_s *)q_buf.qbc;

	*sct = sct_buf;
	qbuf_in(pbufq, QBF_SCT_Q_USED, index);

	return true;
}

bool qsct_recycle_to_idle(struct di_ch_s *pch,
			struct dim_sct_s **sct)
{
	unsigned int index;
	bool ret;
	struct buf_que_s *pbufq;
	union q_buf_u q_buf;
	struct dim_sct_s *sct_buf;

	pbufq = &pch->sct_qb;

	ret = qbuf_out(pbufq, QBF_SCT_Q_RECYCLE, &index);
	if (!ret)
		return false;

	q_buf = pbufq->pbuf[index];
	sct_buf = (struct dim_sct_s *)q_buf.qbc;

	*sct = sct_buf;
	qbuf_in(pbufq, QBF_SCT_Q_IDLE, index);

	return true;
}

bool qsct_req_to_idle(struct di_ch_s *pch,
			struct dim_sct_s **sct)
{
	unsigned int index;
	bool ret;
	struct buf_que_s *pbufq;
	union q_buf_u q_buf;
	struct dim_sct_s *sct_buf;

	pbufq = &pch->sct_qb;

	ret = qbuf_out(pbufq, QBF_SCT_Q_REQ, &index);
	if (!ret)
		return false;

	q_buf = pbufq->pbuf[index];
	sct_buf = (struct dim_sct_s *)q_buf.qbc;

	*sct = sct_buf;
	qbuf_in(pbufq, QBF_SCT_Q_IDLE, index);

	return true;
}

/* in_used to recycle*/
bool qsct_used_some_to_recycle(struct di_ch_s *pch, struct dim_sct_s *sct_buf)
{
//	unsigned int index;
	bool ret;
	struct buf_que_s *pbufq;
	union q_buf_u q_buf;

	pbufq = &pch->sct_qb;
	q_buf.qbc = &sct_buf->header;

	ret = qbuf_out_some(pbufq, QBF_SCT_Q_USED, q_buf);
	if (!ret) {
		PR_ERR("%s:\n", __func__);
		//dbg_hd_print(&sct_buf->header);
		//dbg_q_listid_print(pbufq);
	}
	ret = qbuf_in(pbufq, QBF_SCT_Q_RECYCLE, sct_buf->header.index);
	if (!ret)
		return false;

	return true;
}

bool qsct_any_to_recycle(struct di_ch_s *pch,
			enum QBF_SCT_Q_TYPE qtype,
			struct dim_sct_s **sct)
{
	unsigned int index;
	bool ret;
	struct buf_que_s *pbufq;
	union q_buf_u q_buf;
	struct dim_sct_s *sct_buf;

	pbufq = &pch->sct_qb;

	ret = qbuf_out(pbufq, qtype, &index);
	if (!ret)
		return false;

	q_buf = pbufq->pbuf[index];
	sct_buf = (struct dim_sct_s *)q_buf.qbc;

	*sct = sct_buf;
	qbuf_in(pbufq, QBF_SCT_Q_RECYCLE, index);

	return true;
}

/* in_used to keep */
bool qsct_used_some_to_keep(struct di_ch_s *pch, struct dim_sct_s *sct_buf)
{
//	unsigned int index;
	bool ret;
	struct buf_que_s *pbufq;
	union q_buf_u q_buf;

	pbufq = &pch->sct_qb;
	q_buf.qbc = &sct_buf->header;

	ret = qbuf_out_some(pbufq, QBF_SCT_Q_USED, q_buf);
	if (!ret) {
		PR_ERR("%s:\n", __func__);
		//dbg_hd_print(&sct_buf->header);
		//dbg_q_listid_print(pbufq);
	}
	ret = qbuf_in(pbufq, QBF_SCT_Q_KEEP, sct_buf->header.index);
	if (!ret)
		return false;

	return true;
}

struct dim_sct_s *qsct_req_peek(struct di_ch_s *pch)
{
	struct buf_que_s *pbufq;
	union q_buf_u q_buf;// = NULL;
	struct dim_sct_s *sct;
	bool ret;

	pbufq = &pch->sct_qb;
	//ret = qbuf_peek_s(pbufq, QBF_SCT_Q_REQ, q_buf);
	ret = qbufp_peek(pbufq, QBF_SCT_Q_REQ, &q_buf);
	if (!ret || !q_buf.qbc)
		return NULL;
	sct = (struct dim_sct_s *)q_buf.qbc;

	return sct;
}

struct dim_sct_s *qsct_peek(struct di_ch_s *pch, unsigned int q)
{
	struct buf_que_s *pbufq;
	union q_buf_u q_buf;// = NULL;
	struct dim_sct_s *sct;
	bool ret;

	pbufq = &pch->sct_qb;
	//ret = qbuf_peek_s(pbufq, q, &q_buf);
	ret = qbufp_peek(pbufq, q, &q_buf);
	if (!ret)
		return NULL;
	sct = (struct dim_sct_s *)q_buf.qbc;

	return sct;
}

void dbg_sct_used(struct seq_file *s, struct di_ch_s *pch)
{
	unsigned int i;
	struct qs_cls_s *p;
	unsigned int psize;
	char *splt = "---------------------------";
//	union q_buf_u q_buf;

	struct buf_que_s *pbufq;
	struct dim_sct_s *sct;

	pbufq = &pch->sct_qb;
	p	= pbufq->pque[QBF_SCT_Q_USED];

	p->ops.list(pbufq, p, &psize);
	//seq_printf(s, "%s:\n", pqbuf->name);
	if (psize == 0) {
		seq_printf(s, "%s", "no list\n");
		return;
	}
	seq_printf(s, "%s\n", __func__);
	for (i = 0; i < psize; i++) {
		sct = (struct dim_sct_s *)pbufq->pbuf[pbufq->list_id[i]].qbc;
		seq_printf(s, "[%d]\n", i);
		seq_printf(s, "\t:tail:%d:0x%x\n",
			   sct->flg_act.b.tab_resize, sct->tail_cnt);
		if (sct->pat_buf) {
			seq_printf(s, "\t\tpat:[%d],0x%lx, 0x%px\n",
				sct->pat_buf->header.index,
				sct->pat_buf->mem_start,
				sct->pat_buf->vaddr);
		}
		seq_printf(s, "%s\n", splt);
	}
}

static ssize_t
show_config(struct device *dev,
	    struct device_attribute *attr, char *buf)
{
	int pos = 0;

	return pos;
}

static ssize_t show_tvp_region(struct device *dev,
			       struct device_attribute *attr, char *buff)
{
	ssize_t len = 0;
	/*struct di_dev_s *de_devp = get_dim_de_devp();*/
	struct dim_mm_t_s *mmt = dim_mmt_get();

	if (!mmt)
		return 0;
	len = sprintf(buff, "segment DI:%lx - %lx (size:0x%x)\n",
		      mmt->mem_start,
		      mmt->mem_start + mmt->mem_size - 1,
		      mmt->mem_size);
	return len;
}

static
ssize_t
show_log(struct device *dev, struct device_attribute *attr, char *buf)
{
	return dim_read_log(buf);
}

static ssize_t
show_frame_format(struct device *dev,
		  struct device_attribute *attr, char *buf)
{
	int ret = 0;
	unsigned int channel = get_current_channel();	/*debug only*/
	struct di_pre_stru_s *ppre = get_pre_stru(channel);

	if (get_init_flag(channel))
		ret += sprintf(buf + ret, "%s\n",
			ppre->cur_prog_flag
			? "progressive" : "interlace");

	else
		ret += sprintf(buf + ret, "%s\n", "null");

	return ret;
}

static DEVICE_ATTR(frame_format, 0444, show_frame_format, NULL);
static DEVICE_ATTR(config, 0640, show_config, store_config);
static DEVICE_ATTR(debug, 0200, NULL, store_dbg);
static DEVICE_ATTR(dump_pic, 0200, NULL, store_dump_mem);
static DEVICE_ATTR(log, 0640, show_log, store_log);
static DEVICE_ATTR(provider_vframe_status, 0444, show_vframe_status, NULL);
static DEVICE_ATTR(tvp_region, 0444, show_tvp_region, NULL);
static DEVICE_ATTR(kpi_frame_num, 0640, show_kpi_frame_num,
	store_kpi_frame_num);

/********************************************/
static int di_open(struct inode *node, struct file *file)
{
	di_dev_t *di_in_devp;

/* Get the per-device structure that contains this cdev */
	di_in_devp = container_of(node->i_cdev, di_dev_t, cdev);
	file->private_data = di_in_devp;

	return 0;
}

static int di_release(struct inode *node, struct file *file)
{
/* di_dev_t *di_in_devp = file->private_data; */

/* Reset file pointer */

/* Release some other fields */
	file->private_data = NULL;
	return 0;
}

static long di_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long ret = 0;
	struct di_dev_s *di_devp = NULL;

	if (_IOC_TYPE(cmd) != _DI_) {
		PR_ERR("%s invalid command: %u\n", __func__, cmd);
		return -EFAULT;
	}
	di_devp = di_pdev;
	if (!di_devp || (di_devp->flags & DI_SUSPEND_FLAG)) {
		PR_ERR("%s:suspend, do nothing\n", __func__);
		return  -EFAULT;
	}
#ifdef MARK_HIS
	dbg_reg("no pq\n");
	return 0;
#endif
	switch (cmd) {
	case AMDI_IOC_SET_PQ_PARM:
		ret = dim_pq_load_io(arg);

		break;
	default:
		break;
	}
	return ret;
}

#ifdef CONFIG_COMPAT
static long di_compat_ioctl(struct file *file, unsigned int cmd,
			    unsigned long arg)
{
	unsigned long ret;

	arg = (unsigned long)compat_ptr(arg);
	ret = di_ioctl(file, cmd, arg);
	return ret;
}
#endif

static const struct file_operations di_fops = {
	.owner		= THIS_MODULE,
	.open		= di_open,
	.release	= di_release,
	.unlocked_ioctl	= di_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= di_compat_ioctl,
#endif
};

//#define ARY_MATCH (1)

static const struct di_meson_data  data_g12a = {
	.name = "dim_g12a",
	.ic_id	= DI_IC_ID_G12A,
};

static const struct di_meson_data  data_g12b = {
	.name = "dim_g12b",
	.ic_id	= DI_IC_ID_G12B,
};

static const struct di_meson_data  data_sm1 = {
	.name = "dim_sm1",
	.ic_id	= DI_IC_ID_SM1,
};

static const struct di_meson_data  data_tm2_vb = {
	.name = "dim_tm2_vb",
	.ic_id	= DI_IC_ID_TM2B,
};

static const struct di_meson_data  data_sc2 = {
	.name = "dim_sc2",//sc2c ic_sub_ver=1,DI_IC_REV_SUB
	.ic_id	= DI_IC_ID_SC2,
	.support = IC_SUPPORT_DW
};

static const struct di_meson_data  data_t5 = {
	.name = "dim_t5",
	.ic_id	= DI_IC_ID_T5,
};

static const struct di_meson_data  data_t7 = {
	.name = "dim_t7",
	.ic_id	= DI_IC_ID_T7,
	.support = IC_SUPPORT_DW
};

static const struct di_meson_data  data_t5d_va = {
	.name = "dim_t5d_va",
	.ic_id	= DI_IC_ID_T5D,
};

static const struct di_meson_data  data_t5d_vb = {
	.name = "dim_t5d_vb", //note: this is vb
	.ic_id	= DI_IC_ID_T5DB,
};

static const struct di_meson_data  data_s4 = {
	.name = "dim_s4",
	.ic_id	= DI_IC_ID_S4,
};

static const struct di_meson_data  data_t3 = {
	.name = "dim_t3",//t5w sub_v=1,t3 costdown
	.ic_id	= DI_IC_ID_T3,
	.support = IC_SUPPORT_DECONTOUR	|
		   IC_SUPPORT_HDR	|
		   IC_SUPPORT_DW
};

/* #ifdef CONFIG_USE_OF */
static const struct of_device_id amlogic_deinterlace_dt_match[] = {
	/*{ .compatible = "amlogic, deinterlace", },*/
	{	.compatible = "amlogic, dim-g12a",
		.data = &data_g12a,
	}, {	.compatible = "amlogic, dim-g12b",
		.data = &data_g12b,
	}, {	.compatible = "amlogic, dim-sm1",
		.data = &data_sm1,
	}, {	.compatible = "amlogic, dim-tm2vb",
		.data = &data_tm2_vb,
	}, {	.compatible = "amlogic, dim-sc2",
		.data = &data_sc2,
	}, {	.compatible = "amlogic, dim-t5",
		.data = &data_t5,
	}, {	.compatible = "amlogic, dim-t7",
		.data = &data_t7,
	}, {	.compatible = "amlogic, dim-t5d",
		.data = &data_t5d_va,
	}, {	.compatible = "amlogic, dim-t5dvb",
		.data = &data_t5d_vb,
	}, {	.compatible = "amlogic, dim-s4",
		.data = &data_s4,
	}, {	.compatible = "amlogic, dim-t3",
		.data = &data_t3,
	}, {}
};

void dim_vpu_dev_register(struct di_dev_s *vdevp)
{
	vdevp->dim_vpu_clk_gate_dev = vpu_dev_register(VPU_VPU_CLKB,
						       DEVICE_NAME);
	vdevp->dim_vpu_pd_dec = vpu_dev_register(VPU_AFBC_DEC,
						 DEVICE_NAME);
	vdevp->dim_vpu_pd_dec1 = vpu_dev_register(VPU_AFBC_DEC1,
						  DEVICE_NAME);
	vdevp->dim_vpu_pd_vd1 = vpu_dev_register(VPU_VIU_VD1,
						 DEVICE_NAME);
	vdevp->dim_vpu_pd_post = vpu_dev_register(VPU_DI_POST,
						  DEVICE_NAME);
}

static int dim_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct di_dev_s *di_devp = NULL;
	int i;
	const struct of_device_id *match;
	struct di_data_l_s *pdata;

	PR_INF("%s:\n", __func__);

	/*move from init to here*/
	di_pdev = kzalloc(sizeof(*di_pdev), GFP_KERNEL);
	if (!di_pdev) {
		PR_ERR("%s fail to allocate memory.\n", __func__);
		goto fail_kmalloc_dev;
	}

	/******************/
	ret = alloc_chrdev_region(&di_pdev->devno, 0, DI_COUNT, DEVICE_NAME);
	if (ret < 0) {
		PR_ERR("%s: failed to allocate major number\n", __func__);
		goto fail_alloc_cdev_region;
	}
	dbg_reg("%s: major %d\n", __func__, MAJOR(di_pdev->devno));
	di_pdev->pclss = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(di_pdev->pclss)) {
		ret = PTR_ERR(di_pdev->pclss);
		PR_ERR("%s: failed to create class\n", __func__);
		goto fail_class_create;
	}

	di_devp = di_pdev;
	/* *********new********* */
	di_pdev->data_l = NULL;
	di_pdev->data_l = kzalloc(sizeof(struct di_data_l_s), GFP_KERNEL);
	if (!di_pdev->data_l) {
		PR_ERR("%s fail to allocate data l.\n", __func__);
		goto fail_kmalloc_datal;
	}
	/*memset(di_pdev->data_l, 0, sizeof(struct di_data_l_s));*/
	/*pr_info("\tdata size: %ld\n", sizeof(struct di_data_l_s));*/
	/************************/
	if (!dip_prob()) /* load function interface */
		goto fail_cdev_add;

	di_devp->flags |= DI_SUSPEND_FLAG;
	cdev_init(&di_devp->cdev, &di_fops);
	di_devp->cdev.owner = THIS_MODULE;
	ret = cdev_add(&di_devp->cdev, di_devp->devno, DI_COUNT);
	if (ret)
		goto fail_cdev_add;

	di_devp->devt = MKDEV(MAJOR(di_devp->devno), 0);
	di_devp->dev = device_create(di_devp->pclss, &pdev->dev,
				     di_devp->devt, di_devp, "di%d", 0);

	if (!di_devp->dev) {
		pr_error("device_create create error\n");
		goto fail_cdev_add;
	}
	dev_set_drvdata(di_devp->dev, di_devp);
	platform_set_drvdata(pdev, di_devp);
	di_devp->pdev = pdev;

	/************************/
	match = of_match_device(amlogic_deinterlace_dt_match,
				&pdev->dev);
	if (!match) {
		PR_ERR("%s,no matched table\n", __func__);
		goto fail_cdev_add;
	}
	pdata = (struct di_data_l_s *)di_pdev->data_l;
	pdata->mdata = match->data;
	if (DIM_IS_IC(SC2) &&
	    (get_meson_cpu_version(MESON_CPU_VERSION_LVL_MINOR) >= 0xC))
		pdata->ic_sub_ver = DI_IC_REV_SUB;
	else
		pdata->ic_sub_ver = DI_IC_REV_MAJOR;

	PR_INF("match name: %s:id[%d]:ver[%d]\n", pdata->mdata->name,
	       pdata->mdata->ic_id, pdata->ic_sub_ver);

	ret = of_reserved_mem_device_init(&pdev->dev);
	if (ret != 0)
		PR_INF("no reserved mem.\n");

	di_cfg_top_dts();

	/* move to dim_mem_prob dim_rev_mem(di_devp);*/

	ret = of_property_read_u32(pdev->dev.of_node,
				   "nrds-enable", &di_devp->nrds_enable);
	ret = of_property_read_u32(pdev->dev.of_node,
				   "pps-enable", &di_devp->pps_enable);

	/*di pre h scaling down :sm1 tm2*/
	/*pre_hsc_down_en;*/
	di_devp->h_sc_down_en = dimp_get(edi_mp_pre_hsc_down_en);

	di_devp->pps_enable = dimp_get(edi_mp_pps_en);
//	PR_INF("pps2:[%d]\n", di_devp->h_sc_down_en);

			/*(flag_cma ? 3) reserved in*/
			/*codec mm : cma in codec mm*/
	dim_mem_prob();

	/* mutex_init(&di_devp->cma_mutex); */
	INIT_LIST_HEAD(&di_devp->pq_table_list);

	atomic_set(&di_devp->pq_flag, 1); /* idle */
	atomic_set(&di_devp->pq_io, 1); /* idle */

	di_devp->pre_irq = irq_of_parse_and_map(pdev->dev.of_node, 0);
	PR_INF("pre_irq:%d\n", di_devp->pre_irq);
	di_devp->post_irq = irq_of_parse_and_map(pdev->dev.of_node, 1);
	PR_INF("post_irq:%d\n",	di_devp->post_irq);

	di_devp->aisr_irq = -ENXIO;

	di_devp->aisr_irq = platform_get_irq_byname(pdev, "aisr_irq");
	if (di_devp->aisr_irq  == -ENXIO)
		PR_INF("no aisr_irq\n");
	else
		PR_INF("aisr_irq:%d\n", di_devp->aisr_irq);

	//di_pr_info("%s allocate rdma channel %d.\n", __func__,
	//	   di_devp->rdma_handle);
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXL)) {
		dim_get_vpu_clkb(&pdev->dev, di_devp);
		#ifdef CLK_TREE_SUPPORT
		clk_prepare_enable(di_devp->vpu_clkb);
		PR_INF("vpu clkb =%ld.\n", clk_get_rate(di_devp->vpu_clkb));
		#else
		aml_write_hiubus(HHI_VPU_CLKB_CNTL, 0x1000100);
		#endif
	}
	di_devp->flags &= (~DI_SUSPEND_FLAG);

	/* set flag to indicate that post_wr is supportted */
	ret = of_property_read_u32(pdev->dev.of_node,
				   "post-wr-support",
				   &di_devp->post_wr_support);
	if (ret)
		dimp_set(edi_mp_post_wr_support, 0);/*post_wr_support = 0;*/
	else	/*post_wr_support = di_devp->post_wr_support;*/
		dimp_set(edi_mp_post_wr_support, di_devp->post_wr_support);

	ret = of_property_read_u32(pdev->dev.of_node,
				   "nr10bit-support",
				   &di_devp->nr10bit_support);
	if (ret)
		dimp_set(edi_mp_nr10bit_support, 0);/*nr10bit_support = 0;*/
	else	/*nr10bit_support = di_devp->nr10bit_support;*/
		dimp_set(edi_mp_nr10bit_support, di_devp->nr10bit_support);

	di_pdev->local_meta_size =
			LOCAL_META_BUFF_SIZE * DI_CHANNEL_NUB *
			(MAX_IN_BUF_NUM + MAX_POST_BUF_NUM +
			(MAX_LOCAL_BUF_NUM * 2)) * sizeof(u8);
	di_pdev->local_meta_addr = vmalloc(di_pdev->local_meta_size);
	if (!di_pdev->local_meta_addr) {
		PR_INF("alloc local meta buffer fail\n");
		di_pdev->local_meta_size = 0;
	}

	di_pdev->local_ud_size =
			LOCAL_UD_BUFF_SIZE * DI_CHANNEL_NUB *
			(MAX_IN_BUF_NUM + MAX_POST_BUF_NUM +
			(MAX_LOCAL_BUF_NUM * 2)) * sizeof(u8);
	di_pdev->local_ud_addr = vmalloc(di_pdev->local_ud_size);
	if (!di_pdev->local_ud_addr) {
		PR_INF("alloc local ud buffer fail\n");
		di_pdev->local_ud_addr = 0;
	}

	device_create_file(di_devp->dev, &dev_attr_config);
	device_create_file(di_devp->dev, &dev_attr_debug);
	device_create_file(di_devp->dev, &dev_attr_dump_pic);
	device_create_file(di_devp->dev, &dev_attr_log);
	device_create_file(di_devp->dev, &dev_attr_provider_vframe_status);
	device_create_file(di_devp->dev, &dev_attr_frame_format);
	device_create_file(di_devp->dev, &dev_attr_tvp_region);
	device_create_file(di_devp->dev, &dev_attr_kpi_frame_num);
	dim_vpu_dev_register(di_devp);

	//set ic version need before PQ init
	dil_set_diffver_flag(1);
	dil_set_cpuver_flag(get_datal()->mdata->ic_id);
	if (DIM_IS_IC(SC2) || DIM_IS_IC(S4))
		di_devp->is_crc_ic = true;
	dip_init_pq_ops();

	if (dim_get_canvas()) {
		pr_dbg("DI get canvas error.\n");
		ret = -EEXIST;
		return ret;
	}

	for (i = 0; i < DI_CHANNEL_NUB; i++) {
		set_init_flag(i, false);
		set_reg_flag(i, false);
	}

	set_or_act_flag(true);
	/*PR_INF("\t 11\n");*/
	ret = devm_request_irq(&pdev->dev, di_devp->pre_irq, &dim_irq,
			       IRQF_SHARED,
			       "pre_di", (void *)"pre_di");
	if (di_devp->post_wr_support) {
		ret = devm_request_irq(&pdev->dev, di_devp->post_irq,
				       &dim_post_irq,
				       IRQF_SHARED, "post_di",
				       (void *)"post_di");
	}

	if (di_devp->aisr_irq != -ENXIO) {
		ret = devm_request_irq(&pdev->dev, di_devp->aisr_irq,
				       &dim_aisr_irq,
				       IRQF_SHARED, "aisr_pre",
				       (void *)"aisr_pre");
		PR_INF("aisr _irq ok\n");
	}
	di_devp->sema_flg = 1;	/*di_sema_init_flag = 1;*/
	dimh_hw_init(dimp_get(edi_mp_pulldown_enable),
		     dimp_get(edi_mp_mcpre_en));

	dim_set_di_flag();
	dim_polic_prob();
	dct_pre_prob(pdev);
	dcntr_prob();
	dim_dw_prob();
	dip_prob_ch();
	dim_hdr_prob();

	task_start();
	mtask_start();
	if (DIM_IS_IC_EF(SC2))
		opl1()->pst_mif_sw(false, DI_MIF0_SEL_PST_ALL);
	else
		post_mif_sw(false);

	dim_debugfs_init();	/*2018-07-18 add debugfs*/

	dimh_patch_post_update_mc_sw(DI_MC_SW_IC, true);

	PR_INF("%s:ok\n", __func__);
	return ret;

fail_cdev_add:
	PR_WARN("%s:fail_cdev_add\n", __func__);
	kfree(di_devp->data_l);
	di_devp->data_l = NULL;
fail_kmalloc_datal:
	PR_WARN("%s:fail_kmalloc datal\n", __func__);

	/*move from init*/
/*fail_pdrv_register:*/
	class_destroy(di_pdev->pclss);
fail_class_create:
	unregister_chrdev_region(di_pdev->devno, DI_COUNT);
fail_alloc_cdev_region:
	kfree(di_pdev);
	di_pdev = NULL;
fail_kmalloc_dev:

	return ret;

	return ret;
}

static int dim_remove(struct platform_device *pdev)
{
	struct di_dev_s *di_devp = NULL;
	int i;
	struct di_ch_s *pch;

	PR_INF("%s:\n", __func__);
	di_devp = platform_get_drvdata(pdev);

	dimh_hw_uninit();

	dct_pre_revome(pdev);
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXLX))
		clk_disable_unprepare(di_devp->vpu_clkb);

	di_devp->di_event = 0xff;

	for (i = 0; i < DI_CHANNEL_NUB; i++) {
		dim_uninit_buf(1, i);/*channel 0*/
		pch = get_chdata(i);
		//qiat_clear_buf(pch);
		dim_sec_release(pch);
	}
	dim_hdr_remove();
	di_set_flg_hw_int(false);

	task_stop();
	mtask_stop();

	dim_rdma_exit();

/* Remove the cdev */
	device_remove_file(di_devp->dev, &dev_attr_config);
	device_remove_file(di_devp->dev, &dev_attr_debug);
	device_remove_file(di_devp->dev, &dev_attr_log);
	device_remove_file(di_devp->dev, &dev_attr_dump_pic);
	device_remove_file(di_devp->dev, &dev_attr_provider_vframe_status);
	device_remove_file(di_devp->dev, &dev_attr_frame_format);
	device_remove_file(di_devp->dev, &dev_attr_tvp_region);
	device_remove_file(di_devp->dev, &dev_attr_kpi_frame_num);

	/*pd_device_files_del*/
	get_ops_pd()->remove(di_devp->dev);
	get_ops_nr()->nr_drv_uninit(di_devp->dev);
	cdev_del(&di_devp->cdev);

	dim_mem_remove();

	device_destroy(di_devp->pclss, di_devp->devno);

/* free drvdata */

	dev_set_drvdata(&pdev->dev, NULL);
	platform_set_drvdata(pdev, NULL);

	/*move to remove*/
	class_destroy(di_pdev->pclss);

	dim_debugfs_exit();

	dip_exit();
	unregister_chrdev_region(di_pdev->devno, DI_COUNT);

	kfree(di_devp->data_l);
	di_devp->data_l = NULL;
	vfree(di_pdev->local_meta_addr);
	di_pdev->local_meta_addr = NULL;
	vfree(di_pdev->local_ud_addr);
	di_pdev->local_ud_addr = NULL;
	kfree(di_pdev);
	di_pdev = NULL;
	PR_INF("%s:finish\n", __func__);
	return 0;
}

static void dim_shutdown(struct platform_device *pdev)
{
	struct di_dev_s *di_devp = NULL;
	int i;

	di_devp = platform_get_drvdata(pdev);

	for (i = 0; i < DI_CHANNEL_NUB; i++)
		set_init_flag(i, false);

	if (is_meson_txlx_cpu())
		dim_top_gate_control(true, true);
	else
		DIM_DI_WR(DI_CLKG_CTRL, 0x2);

	if (!is_meson_txlx_cpu())
		diext_clk_b_sw(false);

	PR_INF("%s.\n", __func__);
}

#ifdef CONFIG_PM

static void di_clear_for_suspend(struct di_dev_s *di_devp)
{
	pr_info("%s\n", __func__);

	dip_cma_close();
	pr_info("%s end\n", __func__);
}

/* must called after lcd */
static int di_suspend(struct device *dev)
{
	struct di_dev_s *di_devp = NULL;

	di_devp = dev_get_drvdata(dev);
	di_devp->flags |= DI_SUSPEND_FLAG;

	di_clear_for_suspend(di_devp);

	if (!is_meson_txlx_cpu())
		diext_clk_b_sw(false);
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXHD))
		clk_disable_unprepare(di_devp->vpu_clkb);
	PR_INF("%s\n", __func__);
	return 0;
}

/* must called before lcd */
static int di_resume(struct device *dev)
{
	struct di_dev_s *di_devp = NULL;

	PR_INF("%s\n", __func__);
	di_devp = dev_get_drvdata(dev);

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXL))
		clk_prepare_enable(di_devp->vpu_clkb);

	di_devp->flags &= ~DI_SUSPEND_FLAG;

	/************/
	PR_INF("%s finish\n", __func__);
	return 0;
}

static const struct dev_pm_ops di_pm_ops = {
	.suspend_late = di_suspend,
	.resume_early = di_resume,
};
#endif

static struct platform_driver di_driver = {
	.probe			= dim_probe,
	.remove			= dim_remove,
	.shutdown		= dim_shutdown,
	.driver			= {
		.name		= DEVICE_NAME,
		.owner		= THIS_MODULE,
		.of_match_table = amlogic_deinterlace_dt_match,
#ifdef CONFIG_PM
		.pm			= &di_pm_ops,
#endif
	}
};

int __init dim_module_init(void)
{
	int ret = 0;

	PR_INF("%s\n", __func__);

	ret = platform_driver_register(&di_driver);
	if (ret != 0) {
		PR_ERR("%s: failed to register driver\n", __func__);
		/*goto fail_pdrv_register;*/
		return -ENODEV;
	}
	PR_INF("%s finish\n", __func__);
	return 0;
}

void __exit dim_module_exit(void)
{
	platform_driver_unregister(&di_driver);
	PR_INF("%s: ok.\n", __func__);
}

//MODULE_DESCRIPTION("AMLOGIC MULTI-DI driver");
//MODULE_LICENSE("GPL");
//MODULE_VERSION("4.0.0");

