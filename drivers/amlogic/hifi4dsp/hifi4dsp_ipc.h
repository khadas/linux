/*
 * drivers/amlogic/hifi4dsp/hifi4dsp_ipc.h
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

#ifndef _HIFI4DSP_IPC_H
#define _HIFI4DSP_IPC_H

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/wait.h>
#include <linux/list.h>
#include <linux/workqueue.h>
#include <linux/sched.h>
#include <linux/kthread.h>

#include "hifi4dsp_dsp.h"

struct hifi4dsp_ipc;

#define IPC_MAILBOX_MAX_BYTES	256
#define IPC_EMPTY_LIST_SIZE	8
#define IPC_MSG_TIMEOUT_MSECS	300 /* IPC message timeout (msecs) */

struct hifi4dsp_ipc_message {
	struct list_head list;
	u64 header;

	char	*tx_data;
	size_t	tx_size;
	char	*rx_data;
	size_t	rx_size;

	wait_queue_head_t waitq;
	bool pending;
	bool complete;
	bool wait;
	int	 errno;
};

struct hifi4dsp_ipc_plat_ops {
	void (*tx_msg)(struct hifi4dsp_ipc *, struct hifi4dsp_ipc_message *);
	void (*tx_data_copy)(struct hifi4dsp_ipc_message *, char *, size_t);
	bool (*is_dsp_busy)(struct hifi4dsp_dsp *dsp);
	void (*debug_info)(struct hifi4dsp_ipc *, const char *);
	u64  (*reply_msg_match)(u64 header, u64 *mask);
};

struct hifi4dsp_ipc {
	struct device *dev;
	struct hifi4dsp_dsp *dsp;

	/* IPC messaging */
	struct list_head tx_list;
	struct list_head rx_list;
	struct list_head empty_list;
	wait_queue_head_t wait_txq;
	struct task_struct *tx_thread;
	struct kthread_worker kworker;
	struct kthread_work kwork;
	bool pending;
	struct hifi4dsp_ipc_message *msg;
	int tx_data_max_size;
	int rx_data_max_size;

	struct hifi4dsp_ipc_plat_ops ops;
};

int hifi4dsp_ipc_tx_message_wait(struct hifi4dsp_ipc *ipc, u64 header,
	void *tx_data, size_t tx_bytes, void *rx_data, size_t rx_bytes);

int hifi4dsp_ipc_tx_message_nowait(struct hifi4dsp_ipc *ipc, u64 header,
	void *tx_data, size_t tx_bytes);

struct hifi4dsp_ipc_message *hifi4dsp_ipc_reply_find_msg(
	struct hifi4dsp_ipc *ipc, u64 header);

void hifi4dsp_ipc_tx_msg_reply_complete(struct hifi4dsp_ipc *ipc,
	struct hifi4dsp_ipc_message *msg);

void hifi4dsp_ipc_drop_all(struct hifi4dsp_ipc *ipc);
int hifi4dsp_ipc_init(struct hifi4dsp_ipc *ipc);
void hifi4dsp_ipc_fini(struct hifi4dsp_ipc *ipc);

#endif /*_HIFI4DSP_IPC_H*/
