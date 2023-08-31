// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
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
#include <dev_ion.h>
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

u8 __iomem *gdc_map_virt_from_phys(phys_addr_t phys, unsigned long total_size)
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

void gdc_unmap_virt_from_phys(u8 __iomem *vaddr)
{
	if (vaddr) {
		/* unmap previous vaddr */
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

	if (!path) {
		gdc_log(LOG_ERR, "please define path first\n");
		return -1;
	}

	/* change to KERNEL_DS address limit */
	old_fs = get_fs();
	set_fs(KERNEL_DS);

	/* open file to write */
	fp = filp_open(path, O_WRONLY | O_CREAT, 0640);
	if (IS_ERR(fp)) {
		gdc_log(LOG_ERR, "open file error\n");
		ret = -1;
	}

	/* Write buf to file */
	w_size = vfs_write(fp, buf, size, &pos);
	gdc_log(LOG_DEBUG, "write w_size = %u, size = %u\n", w_size, size);

	vfs_fsync(fp, 0);
	filp_close(fp, NULL);
	set_fs(old_fs);

	return w_size;
}

static void dump_config_file(struct gdc_config_s *gc, u32 dev_type)
{
	void __iomem *config_virt_addr;
	struct meson_gdc_dev_t *gdc_dev = GDC_DEV_T(dev_type);
	int ret;

	if (gdc_dev->config_out_path_defined) {
		/* dump config buffer */
		config_virt_addr =
			gdc_map_virt_from_phys(gc->config_addr,
					       PAGE_ALIGN(gc->config_size * 4));
		ret = write_buf_to_file(gdc_dev->config_out_file,
					config_virt_addr,
					(gc->config_size * 4));
		if (ret <= 0)
			gdc_log(LOG_ERR,
				"Failed to read_file_to_buf\n");
		gdc_unmap_virt_from_phys(config_virt_addr);
	} else {
		gdc_log(LOG_ERR, "config_out_path_defined is not set\n");
	}
}

void dump_gdc_regs(u32 dev_type, u32 core_id)
{
	int i;

	if (dev_type == ARM_GDC) {
		gdc_log(LOG_INFO, "### ARM GDC core%d ###\n", core_id);
		for (i = 0; i <= 0xFF; i += 4)
			gdc_log(LOG_INFO, "reg[0x%02x] = 0x%x\n", i,
				system_gdc_read_32(i, core_id));
	} else {
		gdc_log(LOG_INFO, "### AML GDC core%d ###\n", core_id);
		for (i = 0; i <= (0x65 << 2); i += 4)
			gdc_log(LOG_INFO, "reg[0x%03x] = 0x%x\n", i,
				system_gdc_read_32(i | ISP_DWAP_REG_MARK,
						   core_id));
	}
}

static inline int find_an_idle_core(u32 dev_type)
{
	int i;
	struct meson_gdc_dev_t *gdc_dev = GDC_DEV_T(dev_type);

	for (i = 0; i < gdc_dev->core_cnt; i++) {
		if (gdc_dev->is_idle[i])
			break;
	}

	return (i >= gdc_dev->core_cnt ? -1 : i);
}

static inline struct gdc_queue_item_s *find_an_item_from_pool(void)
{
	struct gdc_context_s *pcontext = NULL;
	struct list_head *item = NULL;
	u32 dev_type = 0;
	int core_id;

	spin_lock(&gdc_manager.event.sem_lock);
	if (list_empty(&gdc_manager.process_queue))
		goto unlock;
	list_for_each_entry(pcontext, &gdc_manager.process_queue, list) {
		/* hold an item from work_queue */
		if (!list_empty(&pcontext->work_queue)) {
			item = pcontext->work_queue.next;
			break;
		}
	}

	if (item) {
		dev_type = ((struct gdc_queue_item_s *)item)->cmd.dev_type;
		core_id = find_an_idle_core(dev_type);
		if (core_id < 0) {
			item = NULL;
			goto unlock;
		}

		/* get this item and delete it from work_queue */
		list_del(item);

		/* if work_queue is empty, move process_queue head */
		if (list_empty(&pcontext->work_queue))
			list_move(&gdc_manager.process_queue,
				  &pcontext->list);

		GDC_DEV_T(dev_type)->current_item[core_id] =
					(struct gdc_queue_item_s *)item;
		((struct gdc_queue_item_s *)item)->core_id = core_id;
		gdc_log(LOG_DEBUG, "using core:%d dev_type:%d\n",
			core_id, dev_type);
	}
unlock:
	spin_unlock(&gdc_manager.event.sem_lock);

	return (struct gdc_queue_item_s *)item;
}

static inline void start_process(struct gdc_queue_item_s *pitem)
{
	int ret = -1;
	u32 core_id = pitem->core_id;
	u32 dev_type = pitem->cmd.dev_type;
	u32 *core_idle = &GDC_DEV_T(pitem->cmd.dev_type)->is_idle[core_id];

	*core_idle = 0;
	gdc_pwr_config(true, dev_type, core_id);

	GDC_DEV_T(dev_type)->time_stamp[core_id] = ktime_get();

	pitem->start_process = 1;
	ret = gdc_run(&pitem->cmd, &pitem->dma_cfg, core_id);
	if (ret < 0) {
		gdc_log(LOG_ERR, "gdc process failed ret = %d\n", ret);
		gdc_finish_item(pitem);
		*core_idle = 1;

		return;
	}
}

static void gdc_detach_dmabuf(struct gdc_queue_item_s *item)
{
	int i;

	if (!item) {
		pr_err("%s: error input param\n", __func__);
		return;
	}

	for (i = 0; i < GDC_MAX_PLANE; i++) {
		if (item->dma_cfg.input_cfg[i].dma_used) {
			gdc_dma_buffer_unmap_info
				(gdc_manager.buffer,
				 &item->dma_cfg.input_cfg[i].dma_cfg);
			item->dma_cfg.input_cfg[i].dma_used = 0;
		}
		if (item->dma_cfg.output_cfg[i].dma_used) {
			gdc_dma_buffer_unmap_info
				(gdc_manager.buffer,
				 &item->dma_cfg.output_cfg[i].dma_cfg);
			item->dma_cfg.output_cfg[i].dma_used = 0;
		}
	}
	if (item->dma_cfg.config_cfg.dma_used) {
		gdc_dma_buffer_unmap_info(gdc_manager.buffer,
					  &item->dma_cfg.config_cfg.dma_cfg);
		item->dma_cfg.config_cfg.dma_used = 0;
	}
}

inline void recycle_resource(struct gdc_queue_item_s *item, u32 core_id)
{
	struct gdc_context_s *context = item->context;
	u32 dev_type = item->cmd.dev_type;

	if (!context) {
		gdc_log(LOG_ERR, "%s, current_wq is NULL\n", __func__);
		return;
	}

	gdc_stop(&item->cmd, core_id);

	if (!GDC_DEV_T(dev_type)->reg_store_mode_enable)
		gdc_pwr_config(false, dev_type, core_id);

	if (gdc_smmu_enable)
		gdc_recycle_linear_config(&item->dma_cfg.config_cfg.dma_cfg);

	/* if dma buf detach it */
	gdc_detach_dmabuf(item);

	spin_lock(&context->lock);
	list_add_tail(&item->list, &context->free_queue);
	item->context = NULL;
	spin_unlock(&context->lock);
}

static int gdc_monitor_thread(void *data)
{
	int ret;
	struct gdc_manager_s *manager = (struct gdc_manager_s *)data;
	struct gdc_queue_item_s *item = NULL;

	gdc_log(LOG_INFO, "gdc workqueue monitor start\n");

	while (manager->process_queue_state != GDC_PROCESS_QUEUE_STOP) {
down_wait:
		ret = down_interruptible(&manager->event.cmd_in_sem);

		item = find_an_item_from_pool();
		if (!item)
			goto down_wait;

		start_process(item);
	}
	gdc_log(LOG_INFO, "exit %s\n", __func__);
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

bool is_gdc_supported(void)
{
	if (gdc_manager.gdc_dev && gdc_manager.gdc_dev->probed)
		return true;

	return false;
}
EXPORT_SYMBOL(is_gdc_supported);

bool is_aml_gdc_supported(void)
{
	if (gdc_manager.aml_gdc_dev && gdc_manager.aml_gdc_dev->probed)
		return true;

	return false;
}
EXPORT_SYMBOL(is_aml_gdc_supported);

int get_gdc_fw_version(void)
{
	int version = ARMGDC_FW_V1;

	if (is_aml_gdc_supported())
		version = GDC_DEV_T(AML_GDC)->fw_version;

	return version;
}
EXPORT_SYMBOL(get_gdc_fw_version);

struct gdc_context_s *create_gdc_work_queue(u32 dev_type)
{
	int  i;
	struct gdc_queue_item_s *p_item;
	struct gdc_context_s *gdc_work_queue;
	int  empty;

	if ((dev_type == ARM_GDC && gdc_manager.gdc_dev->probed) ||
	    (dev_type == AML_GDC && gdc_manager.aml_gdc_dev->probed)) {
		gdc_log(LOG_DEBUG, "Create a work queue, dev_type %d\n",
			dev_type);
	} else {
		gdc_log(LOG_ERR, "GDC is not supported for this chip, dev_type %d\n",
			dev_type);
		return NULL;
	}

	gdc_work_queue = kzalloc(sizeof(*gdc_work_queue), GFP_KERNEL);
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

	gdc_work_queue->cmd.dev_type = dev_type;

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

static int queue_busy_on_core_id(struct gdc_context_s *gdc_work_queue)
{
	struct gdc_context_s *busy_wq = NULL;
	struct gdc_queue_item_s *busy_item = NULL;
	u32 dev_type = gdc_work_queue->cmd.dev_type;
	int i;

	for (i = 0; i < CORE_NUM; i++) {
		busy_item = GDC_DEV_T(dev_type)->current_item[i];
		if (busy_item)
			busy_wq = busy_item->context;
		if (busy_wq == gdc_work_queue)
			break;
	}

	return (i >= CORE_NUM ? -1 : i);
}

int destroy_gdc_work_queue(struct gdc_context_s *gdc_work_queue)
{
	struct gdc_queue_item_s *pitem, *tmp;
	struct list_head		*head;
	int empty, timeout = 0;
	struct completion *process_com = NULL;
	int core_index;

	if (gdc_work_queue) {
		/* first detach it from the process queue,then delete it . */
		/* maybe process queue is changing .so we lock it. */
		spin_lock(&gdc_manager.event.sem_lock);
		list_del(&gdc_work_queue->list);
		empty = list_empty(&gdc_manager.process_queue);
		spin_unlock(&gdc_manager.event.sem_lock);

		core_index = queue_busy_on_core_id(gdc_work_queue);
		if (core_index >= 0) {
			process_com =
				&gdc_manager.event.process_complete[core_index];
			gdc_work_queue->gdc_request_exit = 1;
			timeout = wait_for_completion_timeout
					(process_com, msecs_to_jiffies(500));
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
	pitem->context = wq;
	pitem->start_process = 0;

	return pitem;
}

void gdc_finish_item(struct gdc_queue_item_s *pitem)
{
	u32 dev_type = pitem->cmd.dev_type;
	u32 core_id = pitem->core_id;
	u32 block_mode = pitem->cmd.wait_done_flag;
	struct meson_gdc_dev_t *gdc_dev = GDC_DEV_T(dev_type);
	struct gdc_context_s *current_wq = pitem->context;

	pitem->start_process = 0;
	if (!current_wq) {
		gdc_log(LOG_ERR, "%s, current_wq is NULL\n", __func__);
		return;
	}
	recycle_resource(pitem, core_id);

	/* for block mode, notify item cmd done */
	if (block_mode) {
		pitem->cmd.wait_done_flag = 0;
		wake_up_interruptible(&current_wq->cmd_complete);
	}

	gdc_dev->is_idle[core_id] = 1;

	/* notify thread for next process */
	if (gdc_manager.event.cmd_in_sem.count == 0)
		up(&gdc_manager.event.cmd_in_sem);

	/* if context is tring to exit */
	if (current_wq->gdc_request_exit)
		complete(&gdc_manager.event.process_complete[core_id]);
}

u32 gdc_time_cost(struct gdc_queue_item_s *pitem)
{
	u32 time_cost, core_id;
	ktime_t start_time = 0, diff_time = 0;
	u32 dev_type = pitem->cmd.dev_type;
	struct meson_gdc_dev_t *gdc_dev = GDC_DEV_T(dev_type);

	core_id = pitem->core_id;
	start_time = gdc_dev->time_stamp[core_id];
	diff_time = ktime_sub(ktime_get(), start_time);
	time_cost = ktime_to_ms(diff_time);

	return time_cost;
}

void gdc_timeout_dump(struct gdc_queue_item_s *pitem)
{
	u32 dev_type = pitem->cmd.dev_type;
	u32 core_id = pitem->core_id;
	struct meson_gdc_dev_t *gdc_dev = GDC_DEV_T(dev_type);
	u32 trace_mode_enable = gdc_dev->trace_mode_enable;

	dump_stack();
	if (dev_type == ARM_GDC)
		gdc_log(LOG_ERR, "gdc timeout, status = 0x%x\n",
			gdc_status_read());
	else
		gdc_log(LOG_ERR, "aml gdc timeout, core%d\n", core_id);

	if (trace_mode_enable >= 2) {
		/* dump regs */
		dump_gdc_regs(dev_type, core_id);
		/* dump config buffer */
		dump_config_file(&pitem->cmd.gdc_config,
				 dev_type);
	}
}

int gdc_wq_add_work(struct gdc_context_s *wq,
		    struct gdc_queue_item_s *pitem)
{
	int polling_ms = 100;
	int ret = -1;
	u32 time_cost;

	gdc_log(LOG_DEBUG, "gdc add work\n");
	spin_lock(&wq->lock);
	list_move_tail(&pitem->list, &wq->work_queue);
	spin_unlock(&wq->lock);
	gdc_log(LOG_DEBUG, "gdc add work ok\n");
	/* only read not need lock */
	if (gdc_manager.event.cmd_in_sem.count == 0)
		up(&gdc_manager.event.cmd_in_sem);/* new cmd come in */

	if (pitem->cmd.wait_done_flag) {
		while (1) {
			ret = wait_event_interruptible_timeout
					(wq->cmd_complete,
					 pitem->cmd.wait_done_flag == 0,
					 msecs_to_jiffies(polling_ms));
			/* condition is false, check processing time */
			if (!ret) {
				if (pitem->start_process) {
					time_cost = gdc_time_cost(pitem);
					if (time_cost > GDC_WAIT_THRESHOLD) {
						gdc_timeout_dump(pitem);
						gdc_finish_item(pitem);
						break;
					}
				}
				continue;
			}
			break;
		}
	}

	return 0;
}

int gdc_wq_init(void)
{
	int i;

	gdc_log(LOG_INFO, "init gdc device\n");

	/* prepare bottom half */
	spin_lock_init(&gdc_manager.event.sem_lock);
	sema_init(&gdc_manager.event.cmd_in_sem, 1);
	init_completion(&gdc_manager.event.d_com);
	for (i = 0; i < CORE_NUM; i++)
		init_completion(&gdc_manager.event.process_complete[i]);
	/* coverity[Data race condition] init not need lock */
	INIT_LIST_HEAD(&gdc_manager.process_queue);
	gdc_manager.last_wq = NULL;
	gdc_manager.gdc_thread = NULL;
	gdc_manager.buffer = gdc_dma_buffer_create();
	if (!gdc_manager.buffer)
		return -1;

	if (gdc_start_monitor()) {
		gdc_log(LOG_ERR, "gdc create thread error\n");
		return -1;
	}
	return 0;
}

int gdc_wq_deinit(void)
{
	gdc_stop_monitor();
	gdc_log(LOG_INFO, "deinit gdc device\n");

	gdc_dma_buffer_destroy(gdc_manager.buffer);
	gdc_manager.buffer = NULL;
	return  0;
}
