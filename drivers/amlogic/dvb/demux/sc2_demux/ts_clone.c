// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/dvb/dmx.h>
#include <linux/version.h>
#include <linux/module.h>
#include "../aml_dvb.h"
#include "ts_input.h"
#include "../dmx_log.h"
#include <linux/amlogic/media/utils/am_com.h>

#include "ts_clone.h"

#define STATE_WRITE_IDLE			0
#define STATE_WRITING				1
#define STATE_WRITE_DONE			2

struct demod_ts_clone_child {
	int state;
	int dmx_id;
	unsigned int rp;
	struct in_elem *input;
	struct list_head node;
};

struct demod_ts_clone_parent {
	int source;
	int sid;
	int child_num;
	unsigned long mem;
	unsigned long mem_phy;
	unsigned int mem_size;
	/*protect child_head*/
	struct mutex mutex;
	struct out_elem *ts_output;
	struct list_head child_head;
};

struct dvr_ts_clone_child {
	int state;
	int dmx_id;
	struct in_elem *input;
	struct list_head node;
};

struct dvr_ts_clone_parent {
	int source;
	int child_num;
	struct list_head child_head;
	struct list_head node;
};

struct ts_clone_task {
	int running;
	wait_queue_head_t wait_queue;
	struct task_struct *clone_task;
	u16 flush_time_ms;
	struct timer_list clone_timer;
};

/*demod info*/
static int demod_connect_num;
static u8 demod_ts_num;
static struct demod_ts_clone_parent *demod_ts_info;

/*dvr info*/
struct mutex dvr_mutex;
struct list_head dvr_source_head;

static struct ts_clone_task ts_clone_task_tmp;
static int timer_wake_up;
static int clone_flush_time = 10;
static int ts_clone_init_flag;

#define dprint(fmt, args...)   \
	dprintk(LOG_ERROR, debug_ts_clone, "clone:" fmt, ## args)
#define pr_dbg(fmt, args...)   \
	dprintk(LOG_DBG, debug_ts_clone, "clone:" fmt, ## args)

MODULE_PARM_DESC(debug_ts_clone, "\n\t\t Enable ts clone debug information");
static int debug_ts_clone;
module_param(debug_ts_clone, int, 0644);

MODULE_PARM_DESC(clone_buf_size, "\n\t\t Configure ts clone buffer size");
//about 2.5MB
static int clone_buf_size = 14000 * 188;
module_param(clone_buf_size, int, 0644);

static int _ts_clone_open_output(struct demod_ts_clone_parent *node)
{
	struct out_elem *output;
	int ret;

	if (node->ts_output)
		return 0;
	output = ts_output_open(node->sid, 0xff, CLONE_FORMAT, 0, 0, 0);
	if (!output)
		return -1;

	clone_buf_size = clone_buf_size / 188 * 188;
	ret = ts_output_set_mem(output,	clone_buf_size, 0, 0,	0);
	if (ret != 0)
		goto fail;

	ret = ts_output_add_pid(output, 0x1fff, 0x1fff, 0xff, NULL);
	if (ret != 0)
		goto fail;

	ts_output_get_meminfo(output, &node->mem_size, &node->mem, &node->mem_phy);
	node->ts_output = output;
	return 0;

fail:
	dprint("%s fail\n", __func__);
	ts_output_close(output);
	return -1;
}

static int _ts_clone_close_output(struct demod_ts_clone_parent *node)
{
	if (node->ts_output) {
		ts_output_remove_pid(node->ts_output, 0x1fff);
		ts_output_close(node->ts_output);
		node->ts_output = NULL;
		node->mem = 0;
		node->mem_size = 0;
		node->mem_phy = 0;
	}
	return 0;
}

/*found ret 1, or ret 0*/
static int _ts_clone_find_input(struct list_head  *head, struct in_elem *input, int demod_source)
{
	struct demod_ts_clone_child *demod_input;
	struct dvr_ts_clone_child *dvr_input;

	if (demod_source) {
		list_for_each_entry(demod_input, head, node) {
			if (demod_input->input == input)
				return 1;
		}
	} else {
		list_for_each_entry(dvr_input, head, node) {
			if (dvr_input->input == input)
				return 1;
		}
	}
	return 0;
}

static int _ts_clone_add_input(struct list_head  *head,
	struct in_elem *input, int demod_source, int dmx_id)
{
	struct demod_ts_clone_child *demod_input;
	struct dvr_ts_clone_child *dvr_input;

	if (demod_source) {
		demod_input = kmalloc(sizeof(*demod_input), GFP_KERNEL);
		if (!demod_input) {
			dprint("%s kmalloc fail\n", __func__);
			return -1;
		}
		memset(demod_input, 0, sizeof(*demod_input));
		demod_input->dmx_id = dmx_id;
		demod_input->input = input;
		INIT_LIST_HEAD(&demod_input->node);

		list_add_tail(&demod_input->node, head);
	} else {
		dvr_input = kmalloc(sizeof(*dvr_input), GFP_KERNEL);
		if (!dvr_input) {
			dprint("%s kmalloc fail\n", __func__);
			return -1;
		}
		memset(dvr_input, 0, sizeof(*dvr_input));
		dvr_input->dmx_id = dmx_id;
		dvr_input->input = input;
		INIT_LIST_HEAD(&dvr_input->node);

		list_add_tail(&dvr_input->node, head);
	}
	return 0;
}

static int _ts_clone_demod_wait_writing_done(struct demod_ts_clone_child *demod_input)
{
	if (demod_input->state == STATE_WRITING) {
		while (1) {
			if (ts_input_non_block_write_status(demod_input->input) == 1) {
				ts_input_non_block_write_free(demod_input->input);
				demod_input->state = STATE_WRITE_IDLE;
				break;
			}
		}
	}
	return 0;
}

static int _ts_clone_remove_input(struct list_head  *head, struct in_elem *input, int demod_source)
{
	struct demod_ts_clone_child *demod_input;
	struct demod_ts_clone_child *demod_tmp;
	struct dvr_ts_clone_child *dvr_input;
	struct dvr_ts_clone_child *dvr_tmp;

	if (demod_source) {
		list_for_each_entry_safe(demod_input, demod_tmp, head, node) {
			if (demod_input->input == input) {
				dprint("%s remove input\n", __func__);
				_ts_clone_demod_wait_writing_done(demod_input);
				dprint("%s remove input--- %d\n", __func__, input->id);
				list_del(&demod_input->node);
				kfree(demod_input);
				return 0;
			}
		}
	} else {
		list_for_each_entry_safe(dvr_input, dvr_tmp, head, node) {
			if (dvr_input->input == input) {
				list_del(&dvr_input->node);
				kfree(dvr_input);
				return 0;
			}
		}
	}
	return -1;
}

/*handle dvr parent node*/
static void *_ts_clone_find_dvr_source(int source)
{
	struct dvr_ts_clone_parent *dvr_parent_node;

	list_for_each_entry(dvr_parent_node, &dvr_source_head, node) {
		if (dvr_parent_node->source == source)
			return (void *)dvr_parent_node;
	}
	return NULL;
}

static void *_ts_clone_add_dvr_source(int source)
{
	struct dvr_ts_clone_parent *dvr_parent_node;

	dvr_parent_node = kmalloc(sizeof(*dvr_parent_node), GFP_KERNEL);
	if (!dvr_parent_node) {
		dprint("%s kmalloc fail\n", __func__);
		return NULL;
	}

	memset(dvr_parent_node, 0, sizeof(struct dvr_ts_clone_parent));
	INIT_LIST_HEAD(&dvr_parent_node->node);
	INIT_LIST_HEAD(&dvr_parent_node->child_head);
	dvr_parent_node->source = source;

	list_add_tail(&dvr_parent_node->node, &dvr_source_head);
	return dvr_parent_node;
}

static int _ts_clone_remove_dvr_source(void *dvr_node)
{
	struct dvr_ts_clone_parent *dvr_parent_node;
	struct dvr_ts_clone_parent *tmp;
	struct dvr_ts_clone_parent *dvr_node_tmp = (struct dvr_ts_clone_parent *)dvr_node;

	if (!dvr_node)
		return -1;

	list_for_each_entry_safe(dvr_parent_node, tmp, &dvr_source_head, node) {
		if (dvr_parent_node->child_num == 0 &&
			dvr_parent_node->source == dvr_node_tmp->source) {
			list_del(&dvr_parent_node->node);
			kfree(dvr_parent_node);
			return 0;
		}
	}
	return -1;
}

static int _ts_clone_demod_connect(int dmx_id, int source, struct in_elem *input)
{
	int i = 0;

	pr_dbg("%s dmx_id:%d connect num:%d source:%d input:%d start\n",
		__func__, dmx_id, demod_connect_num, source, input->id);

	//check input
	for (i = 0; i < demod_ts_num; i++) {
		if (demod_ts_info[i].source == source) {
			if (demod_ts_info[i].child_num == 0) {
				_ts_clone_open_output(&demod_ts_info[i]);
			} else {
				if (_ts_clone_find_input(&demod_ts_info[i].child_head,
					input, 1) == 1) {
					return 0;
				}
			}
			if (_ts_clone_add_input(&demod_ts_info[i].child_head,
						input, 1, dmx_id) == 0) {
				demod_connect_num++;
				demod_ts_info[i].child_num++;
				mod_timer(&ts_clone_task_tmp.clone_timer,
				 jiffies + msecs_to_jiffies(clone_flush_time));
			}
			return 0;
		}
	}
	if (i == demod_ts_num) {
		pr_dbg("%s can't find ts source:%d\n", __func__, source);
		return -1;
	}
	return 0;
}

static int _ts_clone_demod_disconnect(int source, struct in_elem *input)
{
	int i = 0;

	pr_dbg("%s connect num:%d source:%d, input:%d start\n",
		__func__, demod_connect_num, source, input->id);
	//check input
	for (i = 0; i < demod_ts_num; i++) {
		if (demod_ts_info[i].source == source) {
			mutex_lock(&demod_ts_info[i].mutex);
			dprint("%s child num:%d\n", __func__, demod_ts_info[i].child_num);
			if (_ts_clone_find_input(&demod_ts_info[i].child_head, input, 1) == 1) {
				if (_ts_clone_remove_input(&demod_ts_info[i].child_head,
					input, 1) == 0) {
					demod_connect_num--;
					demod_ts_info[i].child_num--;
				}
			}
			if (demod_ts_info[i].child_num == 0)
				_ts_clone_close_output(&demod_ts_info[i]);
			mutex_unlock(&demod_ts_info[i].mutex);
		}
	}
	return 0;
}

static int _ts_clone_dvr_connect(int dmx_id, int source, struct in_elem *input)
{
	struct dvr_ts_clone_parent *node;

	mutex_lock(&dvr_mutex);
	node = _ts_clone_find_dvr_source(source);
	if (node) {
		if (_ts_clone_find_input(&node->child_head, input, 0) == 1) {
			mutex_unlock(&dvr_mutex);
			return 0;
		}
		if (_ts_clone_add_input(&node->child_head, input, 0, dmx_id) == 0)
			node->child_num++;
		mutex_unlock(&dvr_mutex);
		return 0;
	}
	node = _ts_clone_add_dvr_source(source);
	if (node) {
		if (_ts_clone_add_input(&node->child_head, input, 0, dmx_id) == 0)
			node->child_num++;
		mutex_unlock(&dvr_mutex);
		return 0;
	}
	pr_dbg("%s add dvr source:%d node fail\n", __func__, source);
	mutex_unlock(&dvr_mutex);
	return -1;
}

static int _ts_clone_dvr_disconnect(int source, struct in_elem *input)
{
	struct dvr_ts_clone_parent *node;

	mutex_lock(&dvr_mutex);
	node = _ts_clone_find_dvr_source(source);
	if (node) {
		if (_ts_clone_find_input(&node->child_head, input, 0) == 1)
			if (_ts_clone_remove_input(&node->child_head, input, 0) == 0)
				node->child_num--;
		if (node->child_num == 0)
			_ts_clone_remove_dvr_source(node);
	}
	mutex_unlock(&dvr_mutex);
	return 0;
}

static int _ts_clone_dvr_write(int source, char *pdata,
	char *buf_phys, int count, int mode, int packet_len)
{
	struct dvr_ts_clone_parent *node;
	struct dvr_ts_clone_child *child_node;
	int w_len;
	int not_write_all_done = 0;
	struct timeval time_begin;
	struct timeval time_end;
	int use_time;

	pr_dbg("%s source:%d, count:%d\n", __func__, source, count);
	node = _ts_clone_find_dvr_source(source);
	if (node && node->child_num) {
		/*writing*/
		list_for_each_entry(child_node, &node->child_head, node) {
			w_len = ts_input_non_block_write(child_node->input, pdata,
				buf_phys, count, mode, packet_len);
			if (w_len == count)
				child_node->state = STATE_WRITING;
		}

		do_gettimeofday(&time_begin);
		/*check writing status*/
		do {
			not_write_all_done = 0;
			list_for_each_entry(child_node, &node->child_head, node) {
				if (child_node->state == STATE_WRITING) {
					if (ts_input_non_block_write_status(child_node->input)
						== 1) {
						ts_input_non_block_write_free(child_node->input);
						child_node->state = STATE_WRITE_IDLE;
					} else {
						not_write_all_done = 1;
					}
				}
			}
			if (not_write_all_done)
				usleep_range(20, 30);
			do_gettimeofday(&time_end);
			use_time = (time_end.tv_sec * 1000 * 1000 + time_end.tv_usec -
				time_begin.tv_sec * 1000 * 1000 - time_begin.tv_usec) / 1000;
		} while (not_write_all_done && use_time <= 500);

		/*handle timeout*/
		if (use_time > 500) {
			pr_dbg("%s timeout:%d\n", __func__, use_time);
			list_for_each_entry(child_node, &node->child_head, node) {
				if (child_node->state == STATE_WRITING) {
					ts_input_non_block_write_free(child_node->input);
					child_node->state = STATE_WRITE_IDLE;
				}
			}
		} else {
			pr_dbg("%s done\n", __func__);
		}
	}
	return count;
}

static int _ts_clone_demod_write(struct demod_ts_clone_parent *ts)
{
	struct demod_ts_clone_child *demod_input;
	unsigned int wp = 0;
	int ret = 0;
	int len = 0;

	if (ts->child_num == 0 || !ts->ts_output)
		return -1;

	list_for_each_entry(demod_input, &ts->child_head, node) {
		pr_dbg("demod_input stat:%d input:%d, rp:0x%0x\n",
				demod_input->state, demod_input->input->id, demod_input->rp);
		if (demod_input->state == STATE_WRITING) {
			if (ts_input_non_block_write_status(demod_input->input) == 1) {
				ts_input_non_block_write_free(demod_input->input);
				demod_input->state = STATE_WRITE_IDLE;
			}
		}

		if (demod_input->state == STATE_WRITE_IDLE) {
			ret = ts_output_get_wp(ts->ts_output, &wp);
			if (wp != demod_input->rp) {
				if (wp < demod_input->rp)
					wp = ts->mem_size;
				len = wp - demod_input->rp;
				len = len / 188 * 188;
				if (!len)
					continue;
				ret = ts_input_non_block_write(demod_input->input,
					(char *)(demod_input->rp + ts->mem),
					(char *)(demod_input->rp + ts->mem_phy), len, 0, 188);
				if (ret == 0 || ret == -1) {
					dprint("%s fail ret:%d\n", __func__, ret);
					continue;
				}
				if (ret == len) {
					demod_input->rp = (demod_input->rp + len) % (ts->mem_size);
					demod_input->state = STATE_WRITING;
					pr_dbg("write len:0x%0x, ret:0x%0x, mem size:0x%0x rp:0x%0x\n",
							len, ret, ts->mem_size, demod_input->rp);
				} else {
					dprint("%s ret:%d, len:%d error\n", __func__, ret, len);
				}
			}
			pr_dbg("%s line:%d\n", __func__, __LINE__);
		}
	}
	return 0;
}

static int _ts_clone_dump_demod_info(struct demod_ts_clone_parent *ts, char *buf, int count)
{
	char tmp_buf[255];
	int r, total = 0;
	struct demod_ts_clone_child *demod_input;
	int child_num = 0;
	unsigned int wp = 0;

	memset(tmp_buf, 0, sizeof(tmp_buf));
	if (ts->source >= FRONTEND_TS0 && ts->source < DMA_0_1)
		sprintf(tmp_buf, "source: frontend_ts%d", (ts->source - FRONTEND_TS0));
	else
		sprintf(tmp_buf, "source: none-%d", ts->source);

	r = sprintf(buf, "%d %s sid:0x%0x ", count, tmp_buf, ts->sid);
	buf += r;
	total += r;

	if (ts->child_num == 0) {
		r = sprintf(buf, "child num:%d\n", ts->child_num);
		buf += r;
		total += r;
		return total;
	}
	r = sprintf(buf, "child num:%d mem size:0x%0x\n", ts->child_num, ts->mem_size);
	buf += r;
	total += r;

	ts_output_get_wp(ts->ts_output, &wp);

	list_for_each_entry(demod_input, &ts->child_head, node) {
		r = sprintf(buf, " child:%d dmx id:%d input sid:0x%0x ",
			child_num, demod_input->dmx_id, demod_input->input->id);
		buf += r;
		total += r;

		if (demod_input->state == STATE_WRITING)
			r = sprintf(buf, "state:writing rp:0x%0x, wp:0x%0x\n", demod_input->rp, wp);
		else
			r = sprintf(buf, "state:idle rp:0x%0x, wp:0x%0x\n", demod_input->rp, wp);
		buf += r;
		total += r;

		child_num++;
	}

	return total;
}

static int _ts_clone_dump_dvr_info(char *buf)
{
	struct dvr_ts_clone_parent *dvr_parent_node;
	int r, total = 0;
	struct dvr_ts_clone_child *dvr_input;
	int child_num = 0;
	int count = demod_ts_num;

	mutex_lock(&dvr_mutex);
	list_for_each_entry(dvr_parent_node, &dvr_source_head, node) {
		if (dvr_parent_node->source >= DMA_0 && dvr_parent_node->source < FRONTEND_TS0)
			r = sprintf(buf, "%d source: dma%d ",
					count, (dvr_parent_node->source - DMA_0));
		else
			r = sprintf(buf, "%d source: none-%d ",
					count, dvr_parent_node->source);
		buf += r;
		total += r;

		r = sprintf(buf, "child num:%d\n", dvr_parent_node->child_num);
		buf += r;
		total += r;

		list_for_each_entry(dvr_input, &dvr_parent_node->child_head, node) {
			r = sprintf(buf, " child:%d dmx id:%d input sid:0x%0x ",
				child_num, dvr_input->dmx_id, dvr_input->input->id);
			buf += r;
			total += r;

			if (dvr_input->state == STATE_WRITING)
				r = sprintf(buf, "state:writing\n");
			else
				r = sprintf(buf, "state:idle\n");
			buf += r;
			total += r;

			child_num++;
		}
		count++;
	}
	mutex_unlock(&dvr_mutex);
	return total;
}

static int _transfer_source(int org_source, int *source, int *demod_source)
{
	if (org_source >= DMA_0 && org_source < FRONTEND_TS0) {
		*source = org_source;
		*demod_source = 0;
	} else if (org_source >= DMA_0_1 && org_source < FRONTEND_TS0_1) {
		*source = org_source - DMA_0_1 + DMA_0;
		*demod_source = 0;
	} else if (org_source >= FRONTEND_TS0 && org_source < DMA_0_1) {
		*source = org_source;
		*demod_source = 1;
	} else if (org_source >= FRONTEND_TS0_1) {
		*source = org_source - FRONTEND_TS0_1 + FRONTEND_TS0;
		*demod_source = 1;
	} else {
		pr_dbg("%s source:%d invalid\n", __func__, org_source);
		return -1;
	}
	return 0;
}

static int _check_timer_clone_wakeup(void)
{
	if (timer_wake_up) {
		timer_wake_up = 0;
		return 1;
	}
	return 0;
}

static void _timer_ts_clone_func(struct timer_list *timer)
{
//    dprint("wakeup ts_clone_timer\n");
	if (demod_connect_num) {
		timer_wake_up = 1;
		wake_up_interruptible(&ts_clone_task_tmp.wait_queue);
		mod_timer(&ts_clone_task_tmp.clone_timer,
		  jiffies + msecs_to_jiffies(clone_flush_time));
	}
}

static int _task_clone_func(void *data)
{
	int timeout = 0;
	int i = 0;

	while (ts_clone_task_tmp.running == TASK_RUNNING) {
		timeout =
		    wait_event_interruptible(ts_clone_task_tmp.wait_queue,
						     _check_timer_clone_wakeup());

		if (ts_clone_task_tmp.running != TASK_RUNNING)
			break;

		for (i = 0; i < demod_ts_num; i++) {
			mutex_lock(&demod_ts_info[i].mutex);
			_ts_clone_demod_write(&demod_ts_info[i]);
			mutex_unlock(&demod_ts_info[i].mutex);
		}
	}
	return 0;
}

/**
 * ts_clone_init
 * \retval 0:success.
 * \retval -1:fail.
 */
int ts_clone_init(void)
{
	struct aml_dvb *advb = aml_get_dvb_device();
	int i = 0;
	int count = 0;

	if (ts_clone_init_flag != 0)
		return 0;

	INIT_LIST_HEAD(&dvr_source_head);

	for (i = 0; i < FE_DEV_COUNT; i++) {
		if (advb->ts[i].ts_sid != -1)
			demod_ts_num++;
	}
	pr_dbg("%s demod_ts_num:%d\n", __func__, demod_ts_num);
	if (demod_ts_num > 0) {
		demod_ts_info = kmalloc_array(demod_ts_num,
			sizeof(*demod_ts_info), GFP_KERNEL);
		if (!demod_ts_info) {
			dprint("%s kmalloc fail\n", __func__);
			demod_ts_num = 0;
			return -1;
		}
		memset(demod_ts_info, 0, sizeof(*demod_ts_info) * demod_ts_num);
		for (i = 0; i < FE_DEV_COUNT; i++) {
			if (advb->ts[i].ts_sid != -1) {
				if (count >= demod_ts_num) {
					dprint("%s fail count:%d, input ts:%d\n",
						__func__, count, demod_ts_num);
					kfree(demod_ts_info);
					demod_ts_info = NULL;
					demod_ts_num = 0;
					return -1;
				}
				demod_ts_info[count].source = FRONTEND_TS0 + i;
				demod_ts_info[count].sid = advb->ts[i].ts_sid;
				mutex_init(&demod_ts_info[count].mutex);
				INIT_LIST_HEAD(&demod_ts_info[count].child_head);
				count++;
			}
		}

		ts_clone_task_tmp.running = TASK_RUNNING;
		ts_clone_task_tmp.flush_time_ms = clone_flush_time;

		init_waitqueue_head(&ts_clone_task_tmp.wait_queue);
		timer_setup(&ts_clone_task_tmp.clone_timer, _timer_ts_clone_func, 0);
		add_timer(&ts_clone_task_tmp.clone_timer);
		mod_timer(&ts_clone_task_tmp.clone_timer,
		  jiffies + msecs_to_jiffies(clone_flush_time));

		ts_clone_task_tmp.clone_task =
		    kthread_run(_task_clone_func, (void *)NULL, "ts_clone_task");
		if (!ts_clone_task_tmp.clone_task)
			dprint("create ts_out_task fail\n");
	}

	mutex_init(&dvr_mutex);
	ts_clone_init_flag = 1;
	return 0;
}

/**
 * ts_clone_destroy
 * \retval 0:success.
 * \retval -1:fail.
 */
int ts_clone_destroy(void)
{
	return 0;
}

/**
 * ts_clone_write
 * \param source:dvr or frontend num source
 * \param pdata:data pointer
 * \param buf_phys: data phy addr
 * \param count:data len
 * \param mode:1:security; 0:normal
 * \param packet_len:188 or 192
 * \retval 0:success.
 * \retval -1:fail.
 */
int ts_clone_write(int source, char *pdata, char *buf_phys, int count, int mode, int packet_len)
{
	int source_tmp = 0;
	int demod_source = 0;

	if (!ts_clone_init_flag)
		return -1;

	pr_dbg("%s source:%d, buf_phys:0x%0x, count:0x%0x, mode:%d, packet:%d\n",
		__func__, source, (u32)(long)buf_phys, count, mode, packet_len);
	if (_transfer_source(source, &source_tmp, &demod_source) != 0)
		return -1;

	if (demod_source) {
		pr_dbg("%s invalid source:%d\n", __func__, source);
		return -1;
	}
	return _ts_clone_dvr_write(source_tmp, pdata, buf_phys, count, mode, packet_len);
}

/**
 * ts_clone_connect
 * \param dmx_id:
 * \param source:
 * \param input:
 * \retval 0:success.
 * \retval -1:fail.
 */
int ts_clone_connect(int dmx_id, int source, struct in_elem *input)
{
	int source_tmp = 0;
	int demod_source = 0;

	if (!ts_clone_init_flag)
		return -1;

	pr_dbg("%s dmx:%d, source:%d input id:%d\n", __func__, dmx_id, source, input->id);
	if (_transfer_source(source, &source_tmp, &demod_source) != 0)
		return -1;

	if (demod_source)
		return _ts_clone_demod_connect(dmx_id, source_tmp, input);
	else
		return _ts_clone_dvr_connect(dmx_id, source_tmp, input);
	return 0;
}

/**
 * ts_clone_disconnect
 * \param dmx_id:
 * \param source:
 * \param input:
 * \retval 0:success.
 * \retval -1:fail.
 */
int ts_clone_disconnect(int dmx_id, int source, struct in_elem *input)
{
	int source_tmp = 0;
	int demod_source = 0;

	if (!ts_clone_init_flag)
		return -1;

	pr_dbg("%s dmx:%d, source:%d input id:%d\n", __func__, dmx_id, source, input->id);

	if (_transfer_source(source, &source_tmp, &demod_source) != 0)
		return -1;

	if (demod_source)
		return _ts_clone_demod_disconnect(source_tmp, input);
	else
		return _ts_clone_dvr_disconnect(source_tmp, input);
	return 0;
}

/**
 * ts_clone_dump_info
 * \param buf:
 * \retval :len
 */
int ts_clone_dump_info(char *buf)
{
	int i = 0;
	int r, total = 0;

	if (!ts_clone_init_flag)
		return 0;

	r = sprintf(buf, "********ts clone source********\n");
	buf += r;
	total += r;

	for (i = 0; i < demod_ts_num; i++) {
		mutex_lock(&demod_ts_info[i].mutex);
		r = _ts_clone_dump_demod_info(&demod_ts_info[i], buf, i);
		mutex_unlock(&demod_ts_info[i].mutex);
		buf += r;
		total += r;
	}
	r = _ts_clone_dump_dvr_info(buf);
	buf += r;
	total += r;
	return total;
}
