/*
 * drivers/amlogic/media/gdc/app/gdc_wq.c
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

#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/miscdevice.h>
#include <linux/types.h>
#include <linux/clk.h>
#include <linux/uaccess.h>
#include <meson_ion.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/dma-contiguous.h>
#include <linux/of_reserved_mem.h>
#include <linux/dma-buf.h>

#include <linux/of_address.h>
#include <api/gdc_api.h>
#include "system_log.h"

#include <linux/types.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/semaphore.h>

#include "gdc_config.h"
#include "gdc_dmabuf.h"
#include "gdc_wq.h"

#define WAIT_THRESHOLD 1000

u8 __iomem *map_virt_from_phys(phys_addr_t phys, unsigned long total_size)
{
	u32 offset, npages;
	struct page **pages = NULL;
	pgprot_t pgprot;
	u8 __iomem *vaddr;
	int i;

	npages = PAGE_ALIGN(total_size) / PAGE_SIZE;
	offset = phys & (~PAGE_MASK);
	if (offset)
		npages++;
	pages = vmalloc(sizeof(struct page *) * npages);
	if (!pages)
		return NULL;
	for (i = 0; i < npages; i++) {
		pages[i] = phys_to_page(phys);
		phys += PAGE_SIZE;
	}
	/*nocache*/
	pgprot = pgprot_noncached(PAGE_KERNEL);

	vaddr = vmap(pages, npages, VM_MAP, pgprot);
	if (!vaddr) {
		pr_err("vmaped fail, size: %d\n",
			npages << PAGE_SHIFT);
		vfree(pages);
		return NULL;
	}
	vfree(pages);

	return vaddr;
}

void unmap_virt_from_phys(u8 __iomem *vaddr)
{
	if (vaddr) {
		/* unmap prevois vaddr */
		vunmap(vaddr);
		vaddr = NULL;
	}
}

static int write_buf_to_file(char *path, char *buf, int size)
{
	int ret = 0;
	struct file *fp = NULL;
	mm_segment_t old_fs;
	loff_t pos = 0;
	int w_size = 0;

	if (!path || !config_out_path_defined) {
		gdc_log(LOG_ERR, "please define path first\n");
		return -1;
	}

	/* change to KERNEL_DS address limit */
	old_fs = get_fs();
	set_fs(KERNEL_DS);

	/* open file to write */
	fp = filp_open(path, O_WRONLY|O_CREAT, 0640);
	if (IS_ERR(fp)) {
		gdc_log(LOG_ERR, "open file error\n");
		ret = -1;
	}

	/* Write buf to file */
	w_size = vfs_write(fp, buf, size, &pos);
	gdc_log(LOG_INFO, "write w_size = %u, size = %u\n", w_size, size);

	vfs_fsync(fp, 0);
	filp_close(fp, NULL);
	set_fs(old_fs);

	return w_size;
}

static void dump_config_file(struct gdc_config_s *gc)
{
	void __iomem *config_virt_addr;
	int ret;

	/* dump config buffer */
	config_virt_addr =
		map_virt_from_phys(gc->config_addr,
		PAGE_ALIGN(gc->config_size * 4));
	ret = write_buf_to_file(config_out_file,
				config_virt_addr,
				(gc->config_size * 4));
	if (ret <= 0)
		gdc_log(LOG_ERR,
			"Failed to read_file_to_buf\n");
	unmap_virt_from_phys(config_virt_addr);
}

static void dump_gdc_regs(void)
{
	int i;

	for (i = 0; i <= 0xFF; i += 4)
		gdc_log(LOG_ERR, "reg[0x%x] = 0x%x\n",
			i, system_gdc_read_32(i));

}

static int gdc_process_work_queue(struct gdc_context_s *wq)
{
	struct gdc_queue_item_s *pitem;
	struct list_head  *head = &wq->work_queue, *pos;
	int ret = 0, i = 0;
	unsigned int block_mode;
	int timeout = 0;
	ktime_t start_time, stop_time, diff_time;
	int process_time = 0;

	if (wq->gdc_request_exit)
		goto exit;
	gdc_manager.gdc_state = GDC_STATE_RUNNING;
	pos = head->next;
	if (pos != head) { /* current work queue not empty. */
		if (wq != gdc_manager.last_wq) { /* maybe */
			/* modify the first item . */
			pitem = (struct gdc_queue_item_s *)pos;
			if (!pitem) {
				gdc_log(LOG_ERR, "can't get pitem\n");
				ret = -1;
				goto  exit;
			}
		} else
			/* modify the first item . */
			pitem = (struct gdc_queue_item_s *)pos;

	} else {
		ret = -1;
		goto  exit;
	}
	do {
		block_mode = pitem->cmd.wait_done_flag;
		if (trace_mode_enable >= 1)
			start_time = ktime_get();
		ret = gdc_run(&pitem->cmd);
		if (ret < 0)
			gdc_log(LOG_ERR, "gdc process failed ret = %d\n", ret);
		timeout = wait_for_completion_timeout(&gdc_manager.event.d_com,
			msecs_to_jiffies(WAIT_THRESHOLD));
		if (timeout == 0) {
			gdc_log(LOG_ERR, "gdc timeout, status = 0x%x\n",
				gdc_status_read());
			/*soft_rst(); */
			if (trace_mode_enable >= 2) {
				/* dump regs */
				dump_gdc_regs();
				/* dump config buffer */
				dump_config_file(&pitem->cmd.gdc_config);
			}
		}
		gdc_stop(&pitem->cmd);
		if (trace_mode_enable >= 1) {
			stop_time = ktime_get();
			diff_time = ktime_sub(stop_time, start_time);
			process_time = ktime_to_ms(diff_time);
			if (process_time > 50)
				gdc_log(LOG_ERR, "gdc process time = %d\n",
					process_time);
		}

		/* if block mode (cmd) */
		if (block_mode) {
			pitem->cmd.wait_done_flag = 0;
			wake_up_interruptible(&wq->cmd_complete);
		}
		spin_lock(&wq->lock);
		pos = pos->next;
		list_move_tail(&pitem->list, &wq->free_queue);
		spin_unlock(&wq->lock);
		/* if dma buf detach it */
		for (i = 0; i < GDC_MAX_PLANE; i++) {
			if (pitem->dma_cfg.input_cfg[i].dma_used) {
				gdc_dma_buffer_unmap_info(gdc_manager.buffer,
					&pitem->dma_cfg.input_cfg[i].dma_cfg);
				pitem->dma_cfg.input_cfg[i].dma_used = 0;
			}
			if (pitem->dma_cfg.output_cfg[i].dma_used) {
				gdc_dma_buffer_unmap_info(gdc_manager.buffer,
					&pitem->dma_cfg.output_cfg[i].dma_cfg);
				pitem->dma_cfg.output_cfg[i].dma_used = 0;
			}
		}
		if (pitem->dma_cfg.config_cfg.dma_used) {
			gdc_dma_buffer_unmap_info(gdc_manager.buffer,
				&pitem->dma_cfg.config_cfg.dma_cfg);
			pitem->dma_cfg.config_cfg.dma_used = 0;
		}
		pitem = (struct gdc_queue_item_s *)pos;
	} while (pos != head);
	gdc_manager.last_wq = wq;
exit:
	if (wq->gdc_request_exit)
		complete(&gdc_manager.event.process_complete);
	gdc_manager.gdc_state = GDC_STATE_IDLE;
	return ret;
}

static inline struct gdc_context_s *get_next_gdc_work_queue(
		struct gdc_manager_s *manager)
{
	struct gdc_context_s *pcontext;

	spin_lock(&manager->event.sem_lock);
	list_for_each_entry(pcontext, &manager->process_queue, list) {
		/* not lock maybe delay to next time. */
		if (!list_empty(&pcontext->work_queue)) {
			/* move head . */
			list_move(&manager->process_queue, &pcontext->list);
			spin_unlock(&manager->event.sem_lock);
			return pcontext;
		}
	}
	spin_unlock(&manager->event.sem_lock);
	return NULL;
}

static int gdc_monitor_thread(void *data)
{
	int ret;
	struct gdc_manager_s *manager = (struct gdc_manager_s *)data;

	gdc_log(LOG_INFO, "gdc workqueue monitor start\n");
	/* setup current_wq here. */
	while (manager->process_queue_state != GDC_PROCESS_QUEUE_STOP) {
		ret = down_interruptible(&manager->event.cmd_in_sem);
		gdc_pwr_config(true);
		while ((manager->current_wq =
				get_next_gdc_work_queue(manager)) != NULL)
			gdc_process_work_queue(manager->current_wq);
		if (!gdc_reg_store_mode)
			gdc_pwr_config(false);
	}
	gdc_log(LOG_INFO, "exit gdc_monitor_thread\n");
	return 0;
}

static int gdc_start_monitor(void)
{
	int ret = 0;

	gdc_log(LOG_INFO, "gdc start monitor\n");
	gdc_manager.process_queue_state = GDC_PROCESS_QUEUE_START;
	gdc_manager.gdc_thread = kthread_run(gdc_monitor_thread,
					       &gdc_manager,
					       "gdc_monitor");
	if (IS_ERR(gdc_manager.gdc_thread)) {
		ret = PTR_ERR(gdc_manager.gdc_thread);
		gdc_log(LOG_ERR, "failed to start kthread (%d)\n", ret);
	}
	return ret;
}

static int gdc_stop_monitor(void)
{
	gdc_log(LOG_INFO, "gdc stop monitor\n");
	if (gdc_manager.gdc_thread) {
		gdc_manager.process_queue_state = GDC_PROCESS_QUEUE_STOP;
		up(&gdc_manager.event.cmd_in_sem);
		kthread_stop(gdc_manager.gdc_thread);
		gdc_manager.gdc_thread = NULL;
	}
	return  0;
}

static inline int work_queue_no_space(struct gdc_context_s *queue)
{
	return  list_empty(&queue->free_queue);
}

struct gdc_context_s *create_gdc_work_queue(void)
{
	int  i;
	struct gdc_queue_item_s *p_item;
	struct gdc_context_s *gdc_work_queue;
	int  empty;

	gdc_work_queue = kzalloc(sizeof(struct gdc_context_s), GFP_KERNEL);
	if (IS_ERR(gdc_work_queue)) {
		gdc_log(LOG_ERR, "can't create work queue\n");
		return NULL;
	}
	gdc_work_queue->gdc_request_exit = 0;
	INIT_LIST_HEAD(&gdc_work_queue->work_queue);
	INIT_LIST_HEAD(&gdc_work_queue->free_queue);
	init_waitqueue_head(&gdc_work_queue->cmd_complete);
	mutex_init(&gdc_work_queue->d_mutext);
	spin_lock_init(&gdc_work_queue->lock);  /* for process lock. */
	for (i = 0; i < MAX_GDC_CMD; i++) {
		p_item = kcalloc(1,
				sizeof(struct gdc_queue_item_s),
				GFP_KERNEL);
		if (IS_ERR(p_item)) {
			gdc_log(LOG_ERR, "can't request queue item memory\n");
			goto fail;
		}
		list_add_tail(&p_item->list, &gdc_work_queue->free_queue);
	}

	/* put this process queue  into manager queue list. */
	/* maybe process queue is changing . */
	spin_lock(&gdc_manager.event.sem_lock);
	empty = list_empty(&gdc_manager.process_queue);
	list_add_tail(&gdc_work_queue->list, &gdc_manager.process_queue);
	spin_unlock(&gdc_manager.event.sem_lock);
	return gdc_work_queue; /* find it */
fail:
	{
		struct list_head *head;
		struct gdc_queue_item_s *tmp;

		head = &gdc_work_queue->free_queue;
		list_for_each_entry_safe(p_item, tmp, head, list) {
			if (p_item) {
				list_del(&p_item->list);
				kfree(p_item);
			}
		}
		return NULL;
	}
}
EXPORT_SYMBOL(create_gdc_work_queue);

int destroy_gdc_work_queue(struct gdc_context_s *gdc_work_queue)
{
	struct gdc_queue_item_s *pitem, *tmp;
	struct list_head		*head;
	int empty, timeout = 0;

	if (gdc_work_queue) {
		/* first detatch it from the process queue,then delete it . */
		/* maybe process queue is changing .so we lock it. */
		spin_lock(&gdc_manager.event.sem_lock);
		list_del(&gdc_work_queue->list);
		empty = list_empty(&gdc_manager.process_queue);
		spin_unlock(&gdc_manager.event.sem_lock);
		if ((gdc_manager.current_wq == gdc_work_queue) &&
		    (gdc_manager.gdc_state == GDC_STATE_RUNNING)) {
			gdc_work_queue->gdc_request_exit = 1;
			timeout = wait_for_completion_timeout(
					&gdc_manager.event.process_complete,
					msecs_to_jiffies(500));
			if (!timeout)
				gdc_log(LOG_ERR, "wait timeout\n");
			/* condition so complex ,simplify it . */
			gdc_manager.last_wq = NULL;
		} /* else we can delete it safely. */

		head = &gdc_work_queue->work_queue;
		list_for_each_entry_safe(pitem, tmp, head, list) {
			if (pitem) {
				list_del(&pitem->list);
				kfree(pitem);
			}
		}
		head = &gdc_work_queue->free_queue;
		list_for_each_entry_safe(pitem, tmp, head, list) {
			if (pitem) {
				list_del(&pitem->list);
				kfree(pitem);
			}
		}

		kfree(gdc_work_queue);
		gdc_work_queue = NULL;
		return 0;
	}

	return  -1;
}
EXPORT_SYMBOL(destroy_gdc_work_queue);

void *gdc_prepare_item(struct gdc_context_s *wq)
{
	struct gdc_queue_item_s *pitem;

	if (work_queue_no_space(wq)) {
		gdc_log(LOG_ERR, "work queue no space\n");
		return NULL;
	}

	pitem = list_entry(wq->free_queue.next, struct gdc_queue_item_s, list);
	if (IS_ERR(pitem))
		return NULL;

	memcpy(&pitem->cmd, &wq->cmd, sizeof(struct gdc_cmd_s));
	memcpy(&pitem->dma_cfg, &wq->dma_cfg, sizeof(struct gdc_dma_cfg_t));
	return pitem;
}

int gdc_wq_add_work(struct gdc_context_s *wq,
	struct gdc_queue_item_s *pitem)
{
	gdc_log(LOG_DEBUG, "gdc add work\n");
	spin_lock(&wq->lock);
	list_move_tail(&pitem->list, &wq->work_queue);
	spin_unlock(&wq->lock);
	gdc_log(LOG_DEBUG, "gdc add work ok\n");
	/* only read not need lock */
	if (gdc_manager.event.cmd_in_sem.count == 0)
		up(&gdc_manager.event.cmd_in_sem);/* new cmd come in */
	/* add block mode   if() */
	if (pitem->cmd.wait_done_flag) {
		wait_event_interruptible(wq->cmd_complete,
				pitem->cmd.wait_done_flag == 0);
		/* interruptible_sleep_on(&wq->cmd_complete); */
	}
	return 0;
}

int gdc_wq_init(struct meson_gdc_dev_t *gdc_dev)
{
	gdc_log(LOG_INFO, "init gdc device\n");
	gdc_manager.gdc_dev = gdc_dev;

	/* prepare bottom half */
	spin_lock_init(&gdc_manager.event.sem_lock);
	sema_init(&gdc_manager.event.cmd_in_sem, 1);
	init_completion(&gdc_manager.event.d_com);
	init_completion(&gdc_manager.event.process_complete);
	INIT_LIST_HEAD(&gdc_manager.process_queue);
	gdc_manager.last_wq = NULL;
	gdc_manager.gdc_thread = NULL;
	gdc_manager.buffer = gdc_dma_buffer_create();
	if (!gdc_manager.buffer)
		return -1;
	if (!gdc_manager.ion_client)
		gdc_manager.ion_client =
		meson_ion_client_create(-1, "meson-gdc");

	if (gdc_start_monitor()) {
		gdc_log(LOG_ERR, "gdc create thread error\n");
		return -1;
	}
	return 0;
}

int gdc_wq_deinit(void)
{
	if (gdc_manager.ion_client) {
		ion_client_destroy(gdc_manager.ion_client);
		gdc_manager.ion_client = NULL;
	}

	gdc_stop_monitor();
	gdc_log(LOG_INFO, "deinit gdc device\n");

	gdc_dma_buffer_destroy(gdc_manager.buffer);
	gdc_manager.buffer = NULL;
	gdc_manager.gdc_dev = NULL;
	return  0;
}
