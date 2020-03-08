// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
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

	if (!PageHighMem(phys_to_page(phys)))
		return phys_to_virt(phys);
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

/********************************************
 * mem
 *******************************************/
#ifdef CONFIG_CMA
#define TVP_MEM_PAGES	0xffff
static bool mm_codec_alloc(const char *owner, size_t count,
			   int cma_mode,
			   struct dim_mm_s *o)
{
	int flags = 0;
	bool istvp = false;

	if (codec_mm_video_tvp_enabled()) {
		istvp = true;
		flags |= CODEC_MM_FLAGS_TVP;
	} else {
		flags |= CODEC_MM_FLAGS_RESERVED | CODEC_MM_FLAGS_CPU;
	}
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

bool dim_mm_alloc(int cma_mode, size_t count, struct dim_mm_s *o)
{
	struct di_dev_s *de_devp = get_dim_de_devp();
	bool ret;

	if (cma_mode == 3 || cma_mode == 4)
		ret = mm_codec_alloc(DEVICE_NAME,
				     count,
				     cma_mode,
				     o);
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
			   &omm);
	if (!ret) /*failed*/
		return 0;
	mmt->mem_start = omm.addr;
	mmt->total_pages = omm.ppage;
	if (cfgnq(MEM_FLAG, EDI_MEM_M_REV) && de_devp->nrds_enable)
		dim_nr_ds_buf_init(cfgg(MEM_FLAG), 0, &de_devp->pdev->dev);
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

static unsigned int di_cma_alloc(struct di_dev_s *devp, unsigned int channel)
{
	unsigned int start_time, end_time, delta_time;
	struct di_buf_s *buf_p = NULL;
	int itmp, alloc_cnt = 0;
	struct di_dev_s *de_devp = get_dim_de_devp();
	struct di_mm_s *mm = dim_mm_get(channel);
	bool aret;
	struct dim_mm_s omm;

	start_time = jiffies_to_msecs(jiffies);
	queue_for_each_entry(buf_p, channel, QUEUE_LOCAL_FREE, list) {
		if (buf_p->pages) {
			PR_ERR("1:%s:buf[%d] page:0x%p alloced skip\n",
			       __func__, buf_p->index, buf_p->pages);
			continue;
		}
		aret = dim_mm_alloc(cfgg(MEM_FLAG),
				    mm->cfg.size_local >> PAGE_SHIFT,
			&omm);
		if (!aret) {
			buf_p->pages = NULL;
			PR_ERR("2:%s: alloc failed %d fail.\n",
			       __func__,
				buf_p->index);
			return 0;
		}
		buf_p->pages = omm.ppage;
		buf_p->nr_adr = omm.addr;
		alloc_cnt++;
		mm->sts.num_local++;
		dbg_mem("CMA  allocate buf[%d]page:0x%p\n",
			buf_p->index, buf_p->pages);
		dbg_mem(" addr 0x%lx ok.\n", buf_p->nr_adr);
		if (mm->cfg.buf_alloc_mode == 0) {
			buf_p->mtn_adr = buf_p->nr_adr +
				mm->cfg.nr_size;
			buf_p->cnt_adr = buf_p->nr_adr +
				mm->cfg.nr_size +
				mm->cfg.mtn_size;
			if (dim_get_mcmem_alloc()) {
				buf_p->mcvec_adr = buf_p->nr_adr +
					mm->cfg.nr_size +
					mm->cfg.mtn_size +
					mm->cfg.count_size;
				buf_p->mcinfo_adr =
					buf_p->nr_adr +
					mm->cfg.nr_size +
					mm->cfg.mtn_size +
					mm->cfg.count_size +
					mm->cfg.mv_size;
				dim_mcinfo_v_alloc(buf_p, mm->cfg.mcinfo_size);
			}
		}
	}
	PR_INF("%s:ch[%d] num_local[%d]:[%d]\n", __func__,
	       channel, mm->sts.num_local, alloc_cnt);
	if (cfgnq(MEM_FLAG, EDI_MEM_M_REV) && de_devp->nrds_enable)
		dim_nr_ds_buf_init(cfgg(MEM_FLAG), 0, &de_devp->pdev->dev);
	end_time = jiffies_to_msecs(jiffies);
	delta_time = end_time - start_time;
	PR_INF("%s:ch[%d] use %u ms(%u~%u)\n",
	       __func__,
	       channel,
	       delta_time, start_time, end_time);
	return 1;
}

static unsigned int dpst_cma_alloc(struct di_dev_s *devp, unsigned int channel)
{
	struct di_buf_s *buf_p = NULL;
	int itmp, alloc_cnt = 0;
	unsigned int tmpa[MAX_FIFO_SIZE];
	unsigned int psize;
	struct di_mm_s *mm = dim_mm_get(channel);
	bool aret;
	struct dim_mm_s omm;
	u64	time1, time2;

	time1 = cur_to_usecs();
	if (dimp_get(edi_mp_post_wr_en) && dimp_get(edi_mp_post_wr_support)) {
		di_que_list(channel, QUE_POST_FREE, &tmpa[0], &psize);
		for (itmp = 0; itmp < psize; itmp++) {
			buf_p = pw_qindex_2_buf(channel, tmpa[itmp]);
			if (buf_p->pages) {
				dbg_mem("3:%s:buf[%d] page:0x%p skip\n",
					__func__,
					buf_p->index, buf_p->pages);
				continue;
			}
			aret = dim_mm_alloc(cfgg(MEM_FLAG),
					    mm->cfg.size_post >> PAGE_SHIFT,
					&omm);
			if (!aret) {
				buf_p->pages = NULL;
				PR_ERR("4:%s: buf[%d] fail.\n", __func__,
				       buf_p->index);
				return 0;
			}
			buf_p->pages = omm.ppage;
			buf_p->nr_adr = omm.addr;
			mm->sts.num_post++;
			alloc_cnt++;
			dbg_mem("%s:pbuf[%d]page:0x%p\n",
				__func__,
				buf_p->index, buf_p->pages);
			dbg_mem(" addr 0x%lx ok.\n", buf_p->nr_adr);
		}
		PR_INF("%s:num_pst[%d]:[%d]\n", __func__, mm->sts.num_post,
		       alloc_cnt);
	}
	time2 = cur_to_usecs();
	PR_INF("%s:ch[%d] use %u us\n",
	       __func__,
	       channel,
	       (unsigned int)(time2 - time1));
	return 1;
}

static void di_cma_release(struct di_dev_s *devp, unsigned int channel)
{
	unsigned int i, ii, rels_cnt = 0, start_time, end_time, delta_time;
	struct di_buf_s *buf_p;
	struct di_buf_s *pbuf_local = get_buf_local(channel);
	struct di_dev_s *de_devp = get_dim_de_devp();
	bool ret;
	struct di_mm_s *mm = dim_mm_get(channel);

	start_time = jiffies_to_msecs(jiffies);
	for (i = 0; (i < mm->cfg.num_local); i++) {
		buf_p = &pbuf_local[i];
		ii = USED_LOCAL_BUF_MAX;
		if (ii >= USED_LOCAL_BUF_MAX &&
		    buf_p->pages) {
			dim_mcinfo_v_release(buf_p);
			ret = dim_mm_release(cfgg(MEM_FLAG),
					     buf_p->pages,
					     mm->cfg.size_local >> PAGE_SHIFT,
					     buf_p->nr_adr);
			if (ret) {
				buf_p->pages = NULL;
				mm->sts.num_local--;
				rels_cnt++;
				dbg_mem("release buf[%d] ok.\n", i);
			} else {
				PR_ERR("%s:release buf[%d] fail.\n",
				       __func__, i);
			}
		} else {
			if (!IS_ERR_OR_NULL(buf_p->pages)) {
				dbg_mem("buf[%d] page:0x%p no release.\n",
					buf_p->index, buf_p->pages);
			}
		}
	}
	if (de_devp->nrds_enable)
		dim_nr_ds_buf_uninit(cfgg(MEM_FLAG), &de_devp->pdev->dev);
	if (mm->sts.num_local < 0 || mm->sts.num_post < 0)
		PR_ERR("%s:mm:nub_local=%d,nub_post=%d\n",
		       __func__,
		       mm->sts.num_local,
		       mm->sts.num_post);
	end_time = jiffies_to_msecs(jiffies);
	delta_time = end_time - start_time;
	PR_INF("%s:ch[%d] release %u buffer use %u ms(%u~%u)\n",
	       __func__,
	       channel,
	       rels_cnt, delta_time, start_time, end_time);
}

static void dpst_cma_release(struct di_dev_s *devp, unsigned int ch)
{
	unsigned int i, rels_cnt = 0;
	struct di_buf_s *buf_p;
	struct di_buf_s *pbuf_post = get_buf_post(ch);
	bool ret;
	struct di_mm_s *mm = dim_mm_get(ch);
	u64 time1, time2;

	time1 = cur_to_usecs();
	if (dimp_get(edi_mp_post_wr_en) && dimp_get(edi_mp_post_wr_support)) {
		for (i = 0; i < mm->cfg.num_post; i++) {
			buf_p = &pbuf_post[i];
			if (di_que_is_in_que(ch, QUE_POST_KEEP, buf_p))
				continue;
			if (!buf_p->pages) {
				PR_INF("2:%s:post buf[%d] is null\n",
				       __func__, i);
				continue;
			}
			ret = dim_mm_release(cfgg(MEM_FLAG),
					     buf_p->pages,
					     mm->cfg.size_post >> PAGE_SHIFT,
					     buf_p->nr_adr);
			if (ret) {
				buf_p->pages = NULL;
				mm->sts.num_post--;
				rels_cnt++;
				dbg_mem("post buf[%d] ok.\n", i);
			} else {
				PR_ERR("%s:post buf[%d]\n", __func__, i);
			}
		}
	}
	if (mm->sts.num_post < 0)
		PR_ERR("%s:mm:nub_post=%d\n",
		       __func__,
		       mm->sts.num_post);
	time2 = cur_to_usecs();
	PR_INF("%s:ch[%d] %u buffer use %u us\n",
	       __func__,
	       ch,
	       rels_cnt, (unsigned int)(time2 - time1));
}
#endif
bool dim_cma_top_alloc(unsigned int ch)
{
	struct di_dev_s *de_devp = get_dim_de_devp();
	bool ret = false;
#ifdef CONFIG_CMA
	if (di_cma_alloc(de_devp, ch) &&
	    dpst_cma_alloc(de_devp, ch))
		ret = true;
#endif
	return ret;
}

bool dim_cma_top_release(unsigned int ch)
{
	struct di_dev_s *de_devp = get_dim_de_devp();
#ifdef CONFIG_CMA
	di_cma_release(de_devp, ch);
	dpst_cma_release(de_devp, ch);
#endif
	return true;
}

bool dim_mm_alloc_api(int cma_mode, size_t count, struct dim_mm_s *o)
{
	bool ret = false;
#ifdef CONFIG_CMA
	ret = dim_mm_alloc(cma_mode, count, o);
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

/********************************************/
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

	if (_IOC_TYPE(cmd) != _DI_) {
		PR_ERR("%s invalid command: %u\n", __func__, cmd);
		return -EFAULT;
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

#define ARY_MATCH (1)
#ifdef ARY_MATCH

static const struct di_meson_data  data_g12a = {
	.name = "dim_g12a",
};

static const struct di_meson_data  data_sm1 = {
	.name = "dim_sm1",
};

static const struct di_meson_data  data_tm2 = {
	.name = "dim_tm2",
};

/* #ifdef CONFIG_USE_OF */
static const struct of_device_id amlogic_deinterlace_dt_match[] = {
	/*{ .compatible = "amlogic, deinterlace", },*/
	{	.compatible = "amlogic, dim-g12a",
		.data = &data_g12a,
	}, {	.compatible = "amlogic, dim-g12b",
		.data = &data_sm1,
	}, {	.compatible = "amlogic, dim-sm1",
		.data = &data_sm1,
	}, {	.compatible = "amlogic, dim-tm2",
		.data = &data_tm2,
	}, {}
};
#endif

void dim_vpu_dev_register(struct di_dev_s *vdevp)
{
	vdevp->di_vpu_clk_gate_dev = vpu_dev_register(VPU_VPU_CLKB,
						      DEVICE_NAME);
	vdevp->di_vpu_pd_dec = vpu_dev_register(VPU_AFBC_DEC, DEVICE_NAME);
	vdevp->di_vpu_pd_dec1 = vpu_dev_register(VPU_AFBC_DEC1, DEVICE_NAME);
	vdevp->di_vpu_pd_vd1 = vpu_dev_register(VPU_VIU_VD1, DEVICE_NAME);
	vdevp->di_vpu_pd_post = vpu_dev_register(VPU_DI_POST, DEVICE_NAME);
}

static int dim_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct di_dev_s *di_devp = NULL;
	int i;
#ifdef ARY_MATCH
	const struct of_device_id *match;
	struct di_data_l_s *pdata;
#endif
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
	PR_INF("%s: major %d\n", __func__, MAJOR(di_pdev->devno));
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
	if (!dip_prob())
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

#ifdef ARY_MATCH
	/************************/
	match = of_match_device(amlogic_deinterlace_dt_match,
				&pdev->dev);
	if (!match) {
		PR_ERR("%s,no matched table\n", __func__);
		goto fail_cdev_add;
	}
	pdata = (struct di_data_l_s *)di_pdev->data_l;
	pdata->mdata = match->data;
	PR_INF("match name: %s\n", pdata->mdata->name);
#endif

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

	atomic_set(&di_devp->pq_flag, 0);

	di_devp->pre_irq = irq_of_parse_and_map(pdev->dev.of_node, 0);
	PR_INF("pre_irq:%d\n", di_devp->pre_irq);
	di_devp->post_irq = irq_of_parse_and_map(pdev->dev.of_node, 1);
	PR_INF("post_irq:%d\n",	di_devp->post_irq);

	di_pr_info("%s allocate rdma channel %d.\n", __func__,
		   di_devp->rdma_handle);
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXL)) {
		dim_get_vpu_clkb(&pdev->dev, di_devp);
		#ifdef CLK_TREE_SUPPORT
		clk_prepare_enable(di_devp->vpu_clkb);
		PR_INF("enable vpu clkb.\n");
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

#ifdef DI_USE_FIXED_CANVAS_IDX
	if (dim_get_canvas()) {
		pr_dbg("DI get canvas error.\n");
		ret = -EEXIST;
		return ret;
	}
#endif

	device_create_file(di_devp->dev, &dev_attr_config);
	device_create_file(di_devp->dev, &dev_attr_debug);
	device_create_file(di_devp->dev, &dev_attr_dump_pic);
	device_create_file(di_devp->dev, &dev_attr_log);
	device_create_file(di_devp->dev, &dev_attr_provider_vframe_status);
	device_create_file(di_devp->dev, &dev_attr_frame_format);
	device_create_file(di_devp->dev, &dev_attr_tvp_region);

	/*pd_device_files_add*/
	get_ops_pd()->prob(di_devp->dev);

	get_ops_nr()->nr_drv_init(di_devp->dev);

	dim_vpu_dev_register(di_devp);

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

	di_devp->sema_flg = 1;	/*di_sema_init_flag = 1;*/
	dimh_hw_init(dimp_get(edi_mp_pulldown_enable),
		     dimp_get(edi_mp_mcpre_en));

	dim_set_di_flag();

	task_start();

	post_mif_sw(false);

	dim_debugfs_init();	/*2018-07-18 add debugfs*/

	dimh_patch_post_update_mc_sw(DI_MC_SW_IC, true);

	dil_set_diffver_flag(1);

	pr_info("%s:ok\n", __func__);
	return ret;

fail_cdev_add:
	pr_info("%s:fail_cdev_add\n", __func__);
	kfree(di_devp->data_l);
	di_devp->data_l = NULL;
fail_kmalloc_datal:
	pr_info("%s:fail_kmalloc datal\n", __func__);

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

	PR_INF("%s:\n", __func__);
	di_devp = platform_get_drvdata(pdev);

	dimh_hw_uninit();

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXLX))
		clk_disable_unprepare(di_devp->vpu_clkb);

	di_devp->di_event = 0xff;

	dim_uninit_buf(1, 0);/*channel 0*/
	di_set_flg_hw_int(false);

	task_stop();

	dim_rdma_exit();

/* Remove the cdev */
	device_remove_file(di_devp->dev, &dev_attr_config);
	device_remove_file(di_devp->dev, &dev_attr_debug);
	device_remove_file(di_devp->dev, &dev_attr_log);
	device_remove_file(di_devp->dev, &dev_attr_dump_pic);
	device_remove_file(di_devp->dev, &dev_attr_provider_vframe_status);
	device_remove_file(di_devp->dev, &dev_attr_frame_format);
	device_remove_file(di_devp->dev, &dev_attr_tvp_region);
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
#ifndef ARY_MATCH
/* #ifdef CONFIG_USE_OF */
static const struct of_device_id amlogic_deinterlace_dt_match[] = {
	/*{ .compatible = "amlogic, deinterlace", },*/
	{ .compatible = "amlogic, dim-g12a", },
	{}
};
#endif
/* #else */
/* #define amlogic_deinterlace_dt_match NULL */
/* #endif */

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

#ifndef MODULE
module_init(dim_module_init);
module_exit(dim_module_exit);
#endif

//MODULE_DESCRIPTION("AMLOGIC MULTI-DI driver");
//MODULE_LICENSE("GPL");
//MODULE_VERSION("4.0.0");

