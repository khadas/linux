/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __MESON_MHU_DSP_H__
#define __MESON_MHU_DSP_H__

#include <linux/cdev.h>
#include <linux/mailbox_controller.h>

#define CHANNEL_MAX		2

#define RX_OFFSET_STAT          (0x20 << 2)
#define RX_OFFSET_CLR           (0x10 << 2)
#define RX_OFFSET_SET           (0x0 << 2)
#define TX_OFFSET_STAT          (0x21 << 2)
#define TX_OFFSET_CLR           (0x11 << 2)
#define TX_OFFSET_SET           (0x1 << 2)

#define PAYLOAD_MAX_SIZE        0x200
#define RX_PAYLOAD(chan)        ((chan) * PAYLOAD_MAX_SIZE)
#define TX_PAYLOAD(chan)        ((chan) * PAYLOAD_MAX_SIZE)
#define MHU_BUFFER_SIZE         512
#define DEBUG_MAILBOX
#define ASYNC_CMD		1
#define SYNC_CMD		2
#define SYNC_MASK		0x7
#define SYNC_SHIFT		25
/*Async 1, sync 2*/
#define ASYNC_OR_SYNC(val)      ((val) << 25)
#define SIZE_MASK               0x1FF
#define CMD_MASK                0xFFFF
#define SIZE_SHIFT              16
#define MBOX_TX_SIZE            244
#define MBOX_SYNC_TX_SIZE       (MBOX_TX_SIZE + MBOX_COMPLETE_LEN)
#define MBOX_ALLOWED_SIZE       (MBOX_TX_SIZE + MBOX_USER_CMD_LEN)
#define TYPE_SHIFT              16
#define TYPE_VALUE(val)         ((val) << TYPE_SHIFT)
#define NR_DSP			2

/*user space mailbox cmd len define type int*/
#define MBOX_USER_CMD_LEN	4
/*u64 reserve*/
#define MBOX_RESERVE_LEN		8
/* u64 complete*/
#define MBOX_COMPLETE_LEN		8

struct mhu_data_buf {
	u32 cmd;
	int tx_size;
	void *tx_buf;
	int rx_size;
	void *rx_buf;
	void *cl_data;
};

enum call_type {
	LISTEN_CALLBACK = TYPE_VALUE(0),
	LISTEN_DATA = TYPE_VALUE(1),
};

struct mhu_chan {
	int index;
	int rx_irq;
	struct mhu_ctlr *ctlr;
	struct mhu_data_buf *data;
};

struct mbox_message {
	struct list_head list;
	struct task_struct *task;
	struct completion complete;
	int cmd;
	char *data;
};

struct mbox_data {
	u64 reserve;
	u64 complete;
	char data[MBOX_TX_SIZE];
} __packed;

struct mbox_data_sync {
	u64 reserve;
	char data[MBOX_SYNC_TX_SIZE];
} __packed;

struct mhu_ctlr {
	struct device *dev;
	void __iomem *mbox_dspa_base;
	void __iomem *mbox_dspb_base;
	void __iomem *payload_base;
	struct mbox_controller mbox_con;
	struct mhu_chan *channels;
};

extern struct device *dsp_scpi_device;
#endif
