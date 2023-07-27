// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/etherdevice.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/atomic.h>
#include <linux/module.h>

#include <linux/amlogic/media/utils/amstream.h>
#include <linux/amlogic/media/frame_sync/ptsserv.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/media/frame_provider/tvin/tvin_v4l2.h>

#include "../tvin_global.h"
#include "../tvin_frontend.h"
#include "../tvin_format_table.h"
#include "vdin_mem_scatter.h"

/* Amlogic headers */
#include <linux/amlogic/media/vfm/vframe.h>
#ifdef CONFIG_AMLOGIC_TEE
#include <linux/amlogic/tee.h>
#endif

/* Local headers */
#include "../tvin_format_table.h"
#include "vdin_afbce.h"
#include "vdin_drv.h"

#define SCT_PRINT_CTL_INIT	BIT(0) /* print sct memory init info */
#define SCT_PRINT_CTL_EXIT	BIT(1) /* print sct memory exit info */
#define SCT_PRINT_CTL_ALLOC_IDX	BIT(2)
#define SCT_PRINT_CTL_FREE_TAIL	BIT(3)
#define SCT_PRINT_CTL_FREE_IDX	BIT(4)
#define SCT_PRINT_CTL_QUE_WORK	BIT(5) /* print queue work */
#define SCT_PRINT_CTL_WORKER	BIT(6) /* print worker processing */
#define SCT_PRINT_CTL_MMU_NUM	BIT(7) /* print worker processing */
#define SCT_PRINT_CTL_GAME	BIT(8) /* print game processing */
#define SCT_PRINT_CTL_WARN	BIT(9) /* buffer warning info */
#define SCT_PRINT_CTL_MEM_HLD	BIT(10) /* for keeper,scatter mem handle */

#define MAX_ADDR_SHIFT PAGE_SHIFT
#define PAGE_ADDR(mm_page) (ulong)(((mm_page) >> MAX_ADDR_SHIFT)\
			<< MAX_ADDR_SHIFT)

int sct_print_ctl;
module_param(sct_print_ctl, int, 0664);
MODULE_PARM_DESC(sct_print_ctl, "sct_print_ctl");

static u64 cur_to_usecs(void)/*2019*/
{
	u64 cur = sched_clock();

	do_div(cur, NSEC_PER_USEC);
	return cur;
}

phy_addr_type vdin_sct_get_page_addr(struct codec_mm_scatter *mms, int id)
{
	phy_addr_type page;

	if (!(mms) || !(mms)->pages_list ||
	     (mms)->page_cnt <= (id) || (id) < 0)
		return 0;
	page = mms->pages_list[id];
	return PAGE_ADDR(page);
}

/* Free buffers of these vf which will put into write list */
void vdin_sct_free_wr_list_idx(struct vf_pool *p, struct vframe_s *vf)
{
	struct vf_entry *vfe = NULL;
	struct vdin_dev_s *devp = NULL;

	vfe = container_of(vf, struct vf_entry, vf);
	if (!vfe) {
		pr_info("%s vfe == NULL\n", __func__);
		return;
	}

	if (vfe->sct_stat != VFRAME_SCT_STATE_INIT) {
		devp = vdin_get_dev(0);
		if (!devp->afbce_info) {
			pr_err("%s vdin%d,afbce_info == NULL!!!\n", __func__, devp->index);
			return;
		}
		//we are not free tail in game mode, so we need to free idx here
		//otherwise it'll hold too much memory
		if (devp->debug.dbg_sct_ctl & DBG_SCT_CTL_NO_FREE_WR_LIST) {
			vdin_sct_free(p, vf->index);
			vfe->sct_stat = VFRAME_SCT_STATE_INIT;
		}
	}
}

void vdin_sct_read_mmu_num(struct vdin_dev_s *devp, struct vf_entry *vfe)
{
	if (devp->mem_type == VDIN_MEM_TYPE_SCT && vfe) {
		vfe->vf.afbce_num = rd(devp->addr_offset, AFBCE_MMU_NUM);
		devp->msct_top.vfe = vfe;
		if (sct_print_ctl & SCT_PRINT_CTL_MMU_NUM)
			pr_info("vdin%d vf:%d afbce_num:%d\n",
				devp->index, vfe->vf.index, vfe->vf.afbce_num);
	}
}

int vdin_sct_start(struct vdin_dev_s *devp)
{
	struct vf_entry *vfe = NULL;

	if (devp->mem_type != VDIN_MEM_TYPE_SCT)
		return 0;

	if (!devp->afbce_info) {
		pr_err("%s vdin%d,afbce_info == NULL!!!\n", __func__, devp->index);
		return 0;
	}

	devp->msct_top.mmu_4k_number =
		((devp->vf_mem_size + (1 << 12) - 1) >> 12);

	//alloc one vf scatter memory with full size.
	vfe = provider_vf_peek(devp->vfp);
	if (!vfe) {
		pr_info("%s vdin%d:peek vframe failed\n", __func__, devp->index);
		return -1;
	}
	vdin_sct_alloc(devp, vfe->vf.index);
	vfe->sct_stat = VFRAME_SCT_STATE_FULL;

	return 0;
}

void vdin_sct_alloc(struct vdin_dev_s *devp, int vf_idx)
{
	int ret;
	unsigned int page_idx;
	struct vdin_msct_top_s *psct;
	int cur_mmu_4k_number;
	u64 timer_st, timer_end, diff;

	psct = &devp->msct_top;
	timer_st = cur_to_usecs();

	cur_mmu_4k_number = devp->msct_top.mmu_4k_number;
	if (sct_print_ctl & SCT_PRINT_CTL_ALLOC_IDX)
		pr_err("%s:vf_mem_size=%#x,number=%d,vf_idx:%d,%p\n", __func__,
			devp->vf_mem_size, cur_mmu_4k_number, vf_idx,
			devp->afbce_info->fm_table_vir_paddr[vf_idx]);

	ret = vdin_mmu_box_alloc_idx(psct->box, vf_idx, cur_mmu_4k_number,
			 devp->afbce_info->fm_table_vir_paddr[vf_idx]);
	if (ret == 0) {
		page_idx = *(unsigned long *)devp->afbce_info->fm_table_vir_paddr[vf_idx];
		devp->afbce_info->fm_body_paddr[vf_idx] = (page_idx << PAGE_SHIFT);
		devp->msct_top.sct_stat[vf_idx].cur_page_cnt = cur_mmu_4k_number;
	} else {
		pr_err("%s:alloc_idx failed with %d\n", __func__, ret);
	}

	codec_mm_dma_flush(devp->afbce_info->fm_table_vir_paddr[vf_idx],
			devp->afbce_info->frame_table_size, DMA_TO_DEVICE);

	timer_end = cur_to_usecs();
	diff = timer_end - timer_st;
	if (sct_print_ctl & SCT_PRINT_CTL_ALLOC_IDX)
		pr_info("%s:use %u us\n", __func__, (unsigned int)diff);
	if ((unsigned int)diff > 10000)
		pr_info("%s:use %uus too long\n", __func__, (unsigned int)diff);
}

void vdin_sct_free_tail(struct vdin_dev_s *devp, int vf_idx, int buffer_used)

{
	struct vdin_msct_top_s	*psct;
	int ret;
	u64 timer_st, timer_end, diff;

	timer_st = cur_to_usecs();
	psct = &devp->msct_top;

	if (!buffer_used) {
		if (sct_print_ctl & SCT_PRINT_CTL_WARN)
			pr_warn("%s:vf_idx:%d,buffer_used == 0\n", __func__, vf_idx);
		return;
	}
	if (sct_print_ctl & SCT_PRINT_CTL_FREE_TAIL)
		pr_info("%s:enter,index:%d,buffer_used:%d\n",
			__func__, vf_idx, buffer_used);
	if (buffer_used > devp->msct_top.mmu_4k_number) {
		pr_err("%s:try to free %d > total %d\n", __func__,
			buffer_used, devp->msct_top.mmu_4k_number);
		return;
	}
	ret = vdin_mmu_box_free_idx_tail(psct->box, vf_idx, buffer_used);
	if (ret == 0)
		psct->sct_stat[vf_idx].cur_page_cnt = buffer_used;
	else
		pr_err("%s:free_idx_tail failed with %d\n", __func__, ret);

	timer_end = cur_to_usecs();
	diff = timer_end - timer_st;
	if (sct_print_ctl & SCT_PRINT_CTL_FREE_TAIL)
		pr_info("%s:use %uus 0x%x\n", __func__, (unsigned int)diff, buffer_used);
}

//void vdin_sct_free(struct vdin_dev_s *devp, int index)
void vdin_sct_free(struct vf_pool *p, int index)
{
	int ret;
	u64 timer_st, timer_end, diff;
	struct vdin_msct_top_s *psct;
	struct vdin_dev_s *devp = NULL;

	devp = vdin_get_dev(0);

	if (devp->mem_type != VDIN_MEM_TYPE_SCT)
		return;

	psct = &devp->msct_top;

	timer_st = cur_to_usecs();
	if (index > psct->max_buf_num) {
		pr_err("%s:idx:%d,num:%d\n", __func__, index, psct->max_buf_num);
		return;
	}
	ret = vdin_mmu_box_free_idx(psct->box, index);
	if (ret == 0)
		psct->sct_stat[index].cur_page_cnt = 0;
	else
		pr_err("%s:box_free_idx failed with %d\n", __func__, ret);

	timer_end = cur_to_usecs();
	diff = timer_end - timer_st;
	if (sct_print_ctl & SCT_PRINT_CTL_FREE_IDX)
		pr_info("%s:index:%d,use %uus\n", __func__, index, (unsigned int)diff);
	if ((unsigned int)diff > 10000)
		pr_info("%s:use %uus too long\n", __func__, (unsigned int)diff);
}

int vdin_sct_init(struct vdin_dev_s *devp)
{
	int tvp_flag = 0;
	int buf_size = 64;
	unsigned int need_cache_size;
	struct vdin_msct_top_s *psct;

	vdin_mmu_box_init();

	psct = &devp->msct_top;
	if (psct->box) {
		pr_err("%s:box is exist\n", __func__);
		return -1;
	}
	need_cache_size = buf_size * SZ_1M;
	psct->sc_start_time = get_jiffies_64();
	psct->max_buf_num = VDIN_CANVAS_MAX_CNT;

	if (sct_print_ctl & SCT_PRINT_CTL_INIT)
		pr_info("%s tvp = 0x%x,max_buf_num:%d,buf_size:%dMB\n", __func__,
			psct->max_buf_num, tvp_flag, buf_size);

	psct->box = vdin_mmu_box_alloc_box(DRIVER_NAME,
			devp->index, psct->max_buf_num,
			need_cache_size, tvp_flag);
	if (!psct->box) {
		pr_err("vdin alloc mmu box failed!!\n");
		return -1;
	}

	if (sct_print_ctl & SCT_PRINT_CTL_INIT)
		pr_info("%s done\n", __func__);

	return 0;
}

int vdin_sct_release(struct vdin_dev_s *devp)
{
	struct vdin_msct_top_s *psct;

	psct = &devp->msct_top;

	if (psct->box) {
		vdin_mmu_box_free(psct->box);
		psct->box = NULL;
	}

	return 0;
}

void vdin_sct_queue_work(struct vdin_dev_s *devp)
{
	if (devp->mem_type == VDIN_MEM_TYPE_SCT) {
		devp->msct_top.que_work_cnt++;
		if (sct_print_ctl & SCT_PRINT_CTL_QUE_WORK)
			pr_info("%s,irq:%d,frame_cnt:%d;que_work_cnt:%d,worker run:%d\n",
				__func__, devp->irq_cnt, devp->frame_cnt,
				devp->msct_top.que_work_cnt, devp->msct_top.worker_run_cnt);
		queue_work(devp->wq, &devp->sct_work);
	}
}

/* when in game mode,try to free vf memory which in wr_list,except the first one */
void vdin_sct_free_idx_in_game(struct vdin_dev_s *devp)
{
	int i;
	struct vf_entry *vfe = NULL, *next_wr_vfe = NULL;

	for (i = 0; i < devp->vfp->size; i++) {
		vfe = vf_get_master(devp->vfp, i);
		if (!vfe) {
			pr_err("%s null vfe\n", __func__);
			break;
		}
		if (sct_print_ctl & SCT_PRINT_CTL_GAME)
			pr_info("[%d],index:%d,sct_stat:%d,status:%d,cur_page_cnt:%d\n",
				i, vfe->vf.index, vfe->sct_stat, vfe->status,
				devp->msct_top.sct_stat[i].cur_page_cnt);
		//this vfe alloced memory for the next vsync,do not free it.
		next_wr_vfe = provider_vf_peek(devp->vfp);
		if (next_wr_vfe && vfe->vf.index == next_wr_vfe->vf.index)
			continue;
		/* donot free it, other vf still use it's memory in one buffer mode */
		if (vfe->vf.index == devp->af_num)
			continue;
		if (vfe->status == VF_STATUS_WL &&
		    vfe->sct_stat == VFRAME_SCT_STATE_FULL) {
			vdin_sct_free(devp->vfp, vfe->vf.index);
			vfe->sct_stat = VFRAME_SCT_STATE_INIT;
		}
	}
}

void vdin_sct_worker(struct work_struct *work)
{
	struct vf_entry *vfe = NULL, *next_wr_vfe = NULL;

	struct vdin_dev_s *devp =
		container_of(work, struct vdin_dev_s, sct_work);

	devp->msct_top.worker_run_cnt++;
	if (sct_print_ctl & SCT_PRINT_CTL_WORKER)
		pr_info("%s,enter;que_work_cnt:%d,worker run:%d\n", __func__,
			devp->msct_top.que_work_cnt, devp->msct_top.worker_run_cnt);
	if (!devp->afbce_info) {
		pr_err("%s vdin%d,afbce_info == NULL!!!\n", __func__, devp->index);
		return;
	}
	/* alloc memory may take same time,save the captured vfe first */
	vfe = devp->msct_top.vfe;
	devp->msct_top.vfe = NULL;

	//alloc memory for the next vfe.
	next_wr_vfe = provider_vf_peek(devp->vfp);
	if (next_wr_vfe) {
		if ((sct_print_ctl & SCT_PRINT_CTL_WARN) &&
		    !devp->game_mode && next_wr_vfe->sct_stat == VFRAME_SCT_STATE_FULL &&
		    !(devp->debug.dbg_sct_ctl & DBG_SCT_CTL_NO_FREE_TAIL) &&
		    !(devp->debug.dbg_sct_ctl & DBG_SCT_CTL_NO_FREE_WR_LIST))
			pr_warn("%s,full mem size!!!vf_index:%d,sct_stat:%d,status:%d\n",
				__func__, next_wr_vfe->vf.index,
				next_wr_vfe->sct_stat, next_wr_vfe->status);
		if (sct_print_ctl & SCT_PRINT_CTL_WORKER)
			pr_info("%s,vf_idx:%d,cur_page_cnt:%d\n",
				__func__, next_wr_vfe->vf.index,
				devp->msct_top.sct_stat[next_wr_vfe->vf.index].cur_page_cnt);
		next_wr_vfe->vf.afbce_num = 0;
		next_wr_vfe->vf.mem_handle =
			vdin_mmu_box_get_mem_handle(devp->msct_top.box, next_wr_vfe->vf.index);
		if (sct_print_ctl & SCT_PRINT_CTL_MEM_HLD)
			pr_info("vdin%d,mem handle[%d]:%p\n", devp->index, next_wr_vfe->vf.index,
			next_wr_vfe->vf.mem_handle);
		//this memory is used for every vf in one buffer mode,donot alloc again or free it
		if (next_wr_vfe->vf.index != devp->af_num) {
			vdin_sct_alloc(devp, next_wr_vfe->vf.index);
			next_wr_vfe->sct_stat = VFRAME_SCT_STATE_FULL;
		}
	} else {
		if (sct_print_ctl & SCT_PRINT_CTL_WARN)
			pr_info("vdin%d:peek vframe failed,irq:%d,frame:%d;[%d %d %d %d]%d %d\n",
				devp->index, devp->irq_cnt, devp->frame_cnt,
				devp->vfp->wr_list_size, devp->vfp->wr_mode_size,
				devp->vfp->rd_list_size, devp->vfp->rd_mode_size,
				devp->msct_top.que_work_cnt, devp->msct_top.worker_run_cnt);
	}
	//alloc the next vfe memory end

	//free redundancy memory of captured vf.
	//do not free redundancy pages in game mode,free all pages of these in wr_list.
	if (vfe) {
		if (!devp->game_mode && !(devp->debug.dbg_sct_ctl & DBG_SCT_CTL_NO_FREE_TAIL)) {
			if (sct_print_ctl & SCT_PRINT_CTL_WORKER)
				pr_info("%s,vdin%d index:%d,num:%d,stat:%d\n",
					__func__, devp->index, vfe->vf.index,
					vfe->vf.afbce_num, vfe->sct_stat);
			if (vfe->sct_stat != VFRAME_SCT_STATE_FULL)
				pr_err("%s,sct_stat= %d\n", __func__, vfe->sct_stat);
			//free tail
			vdin_sct_free_tail(devp, vfe->vf.index, vfe->vf.afbce_num);
			vfe->sct_stat = VFRAME_SCT_STATE_FREE_TAIL;
		} else if (devp->game_mode) {
			vdin_sct_free_idx_in_game(devp);
		}
		if (sct_print_ctl & SCT_PRINT_CTL_WORKER)
			pr_info("%s,exit! vf_idx:%d,stat:%d %d\n", __func__,
				vfe->vf.index, vfe->sct_stat, vfe->status);
	} else {
		if (sct_print_ctl & SCT_PRINT_CTL_WARN)
			pr_warn("%s,no msct_top.vfe!!!\n", __func__);
	}
	//free redundancy memory of captured vf end
}

/* afbce use scatter or continuous memory */
int vdin_mem_init(struct vdin_dev_s *devp)
{
	bool is_afbce;

	is_afbce = (devp->afbce_mode == 1 || devp->double_wr);

	if ((devp->debug.dbg_sct_ctl & DBG_SCT_CTL_DIS) ||
	      devp->index || !is_afbce ||
	    !(devp->cma_config_flag & MEM_ALLOC_CODEC_SCT)) {
		devp->mem_type = VDIN_MEM_TYPE_CONTINUOUS;
		devp->msct_top.sct_pause_dec = false;
		pr_info("%s,vdin%d do not use scatter memory;is_afbce:%d,cma_flag:%#x\n",
			__func__, devp->index, is_afbce, devp->cma_config_flag);
		return 0;
	}
	pr_info("%s vdin%d use scatter memory\n", __func__, devp->index);

	memset(&devp->msct_top, 0, sizeof(devp->msct_top));
	/* scatter memory flag */
	devp->mem_type = VDIN_MEM_TYPE_SCT;
	/* need extra one buffer for sct mem */
	if (devp->frame_buff_num_bak < VDIN_CANVAS_MAX_CNT)
		devp->frame_buff_num = devp->frame_buff_num_bak + 1;

	/* alloc box */
	vdin_sct_init(devp);

	devp->wq = alloc_ordered_workqueue("vdin0-worker",
		__WQ_LEGACY | WQ_MEM_RECLAIM | WQ_HIGHPRI);
	if (!devp->wq) {
		pr_err("%s Failed to create workqueue\n", __func__);
		return -EINVAL;
	}
	INIT_WORK(&devp->sct_work, vdin_sct_worker);
	//queue_work(devp->wq, &devp->sct_work);
	if (sct_print_ctl & SCT_PRINT_CTL_INIT)
		pr_info("%s done\n", __func__);

	return 0;
}

int vdin_mem_exit(struct vdin_dev_s *devp)
{
	int i;

	if (sct_print_ctl & SCT_PRINT_CTL_EXIT)
		pr_info("%s mem_type:%d\n", __func__, devp->mem_type);

	if (devp->mem_type != VDIN_MEM_TYPE_SCT)
		return 0;

	for (i = 0; i < devp->vfp->size; i++)
		vdin_sct_free(devp->vfp, i);

	vdin_sct_release(devp);

	flush_workqueue(devp->wq);
	destroy_workqueue(devp->wq);

	devp->frame_buff_num = devp->frame_buff_num_bak;

	if (sct_print_ctl & SCT_PRINT_CTL_EXIT)
		pr_info("%s done\n", __func__);

	return 0;
}
