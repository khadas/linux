// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/module.h>

#include "ts_input.h"
#include "mem_desc.h"
#include "../dmx_log.h"
#define KERNEL_ATRACE_TAG KERNEL_ATRACE_TAG_DEMUX
#include <trace/events/meson_atrace.h>

#define MAX_INPUT_NUM			32
static struct in_elem ts_input_table[MAX_INPUT_NUM];

#define TS_INPUT_DESC_MAX_SIZE	(400 * 188)
#define TS_INPUT_BUFF_SIZE		(400 * 188)

#define dprint(fmt, args...)   \
	dprintk(LOG_ERROR, debug_input, "ts_input:" fmt, ## args)
#define pr_dbg(fmt, args...)   \
	dprintk(LOG_DBG, debug_input, "ts_input:" fmt, ## args)

MODULE_PARM_DESC(debug_input, "\n\t\t Enable demux input information");
static int debug_input;
module_param(debug_input, int, 0644);

/**
 * ts_input_init
 * \retval 0:success.
 * \retval -1:fail.
 */
int ts_input_init(void)
{
	memset(&ts_input_table, 0, sizeof(ts_input_table));
	return 0;
}

/**
 * ts_input_destroy
 * \retval 0:success.
 * \retval -1:fail.
 */
int ts_input_destroy(void)
{
	int i = 0;

	for (i = 0; i < MAX_INPUT_NUM; i++) {
		struct in_elem *pelem = &ts_input_table[i];

		if (pelem->used && pelem->pchan) {
			SC2_bufferid_dealloc(pelem->pchan);
			pelem->pchan = NULL;
		}
		pelem->used = 0;
	}
	return 0;
}

/**
 * ts_input_open
 * \param id:
 * \retval in_elem:success.
 * \retval NULL:fail.
 */
struct in_elem *ts_input_open(int id)
{
	int ret = 0;
	struct bufferid_attr attr;
	struct in_elem *elem;

	pr_dbg("%s line:%d id:%d\n", __func__, __LINE__, id);

	if (id >= MAX_INPUT_NUM) {
		dprint("%s id:%d invalid\n", __func__, id);
		return NULL;
	}
	attr.mode = INPUT_MODE;
	attr.req_id = id;
	ret = SC2_bufferid_alloc(&attr, &ts_input_table[id].pchan, NULL);
	if (ret != 0)
		return NULL;

	ts_input_table[id].id = id;
	ts_input_table[id].used = 1;
	elem = &ts_input_table[id];

	if (SC2_bufferid_set_mem(elem->pchan,
				 TS_INPUT_BUFF_SIZE, 0) != 0) {
		SC2_bufferid_dealloc(ts_input_table[id].pchan);
		ts_input_table[id].used = 0;
		dprint("input id:%d, malloc fail\n", id);
		return NULL;
	}
	pr_dbg("%s line:%d\n", __func__, __LINE__);
	return elem;
}

/**
 * ts_input_close
 * \param elem:
 * \retval 0:success.
 * \retval -1:fail.
 */
int ts_input_close(struct in_elem *elem)
{
	pr_dbg("%s id:%d\n", __func__, elem->id);
	if (elem && elem->pchan)
		SC2_bufferid_dealloc(elem->pchan);
	if (elem)
		elem->used = 0;
	return 0;
}

/**
 * ts_input_write
 * \param elem:
 * \param buf:
 * \param buf_phys:
 * \param count:
 * \param mode: 1:security; 0:normal
 * \param packet_len:188 or 192
 * \retval size:written count
 * \retval -1:fail.
 */
int ts_input_write(struct in_elem *elem, const char *buf, char *buf_phys, int count,
		int mode, int packet_len)
{
	int ret = 0;

	if (!elem)
		return -1;

	if (!elem->pchan || !buf) {
		pr_dbg("%s invalid parameter line:%d\n", __func__, __LINE__);
		return 0;
	}
	pr_dbg("%s id:%d count:%d\n", __func__, elem->id, count);

	ret = SC2_bufferid_write(elem->pchan,
				buf, buf_phys, count, mode, packet_len);
	ATRACE_COUNTER("demux_ts_input", ret);
	return ret;
}

/**
 * ts_input_non_block_write
 * \param elem:
 * \param buf:
 * \param buf_phys:
 * \param count:
 * \param mode: 1:security; 0:normal
 * \param packet_len:188 or 192
 * \retval size:written count
 * \retval -1:fail.
 */
int ts_input_non_block_write(struct in_elem *elem, const char *buf, char *buf_phys, int count,
		int mode, int packet_len)
{
	int ret = 0;

	if (!elem)
		return -1;

	if (!elem->pchan || !buf) {
		pr_dbg("%s invalid parameter line:%d\n", __func__, __LINE__);
		return 0;
	}
	pr_dbg("%s id:%d count:%d\n", __func__, elem->id, count);

	ret = SC2_bufferid_non_block_write(elem->pchan,
				buf, buf_phys, count, mode, packet_len);
	ATRACE_COUNTER("demux_ts_input", ret);
	return ret;
}

/**
 * ts_input_non_block_write_status
 * \param elem:
 * \retval 1:done, 0:not done
 */
int ts_input_non_block_write_status(struct in_elem *elem)
{
	int ret = 0;

	if (!elem)
		return -1;

	if (!elem->pchan) {
		pr_dbg("%s invalid parameter line:%d\n", __func__, __LINE__);
		return 0;
	}
	ret = SC2_bufferid_non_block_write_status(elem->pchan);
	pr_dbg("%s id:%d, ret:%d\n", __func__, elem->id, ret);
	return ret;
}

/**
 * ts_input_non_block_write_free
 * \param elem:
 * \retval 0:success
 */
int ts_input_non_block_write_free(struct in_elem *elem)
{
	int ret = 0;

	if (!elem)
		return -1;

	if (!elem->pchan) {
		pr_dbg("%s invalid parameter line:%d\n", __func__, __LINE__);
		return 0;
	}
	ret = SC2_bufferid_non_block_write_free(elem->pchan);
	pr_dbg("%s id:%d, ret:%d\n", __func__, elem->id, ret);
	return ret;
}

/**
 * ts_input_write_empty
 * \param elem:
 * \param pid
 * \retval -1:fail.
 */
int ts_input_write_empty(struct in_elem *elem, int pid)
{
	int ret = 0;

	ret = SC2_bufferid_write_empty(elem->pchan, pid);
	return ret;
}

