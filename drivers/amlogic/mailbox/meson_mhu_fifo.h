/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __MESON_MHU_FIFO_H__
#define __MESON_MHU_FIFO_H__

#include <linux/cdev.h>
#include <linux/mailbox_controller.h>
#include <linux/amlogic/meson_mhu_common.h>

#define CHANNEL_FIFO_MAX	6
#define MBOX_MAX		CHANNEL_FIFO_MAX
#define MHUIRQ_MAXNUM		32

#define PAYLOAD_OFFSET(chan)	(0x80 * (chan))
#define CTL_OFFSET(chan)	((chan) * 0x4)

#define IRQ_REV_BIT(mbox)	(1 << ((mbox) * 2))
#define IRQ_SENDACK_BIT(mbox)	(1 << ((mbox) * 2 + 1))

#define IRQ_MASK_OFFSET(x)	(0x00 + ((x) << 2))
#define IRQ_TYPE_OFFSET(x)	(0x10 + ((x) << 2))
#define IRQ_CLR_OFFSET(x)	(0x20 + ((x) << 2))
#define IRQ_STS_OFFSET(x)	(0x30 + ((x) << 2))

/*inclule status 0x4 task id 0x8, ullclt 0x8, completion 0x8*/
#define MBOX_HEAD_SIZE		0x1c
#define MBOX_RESEV_SIZE		0x4

#define MBOX_FIFO_SIZE		0x80
#define MBOX_DATA_SIZE \
	(MBOX_FIFO_SIZE - MBOX_HEAD_SIZE - MBOX_RESEV_SIZE)

/*user space mailbox cmd len define type int*/
#define MBOX_USER_CMD_LEN	4

#define MBOX_USER_MAX_LEN	(MBOX_DATA_SIZE + MBOX_USER_CMD_LEN)
/*u64 taskid*/
#define MBOX_TASKID_LEN		8
/* u64 compete*/
#define MBOX_COMPETE_LEN	8
/* u64 ullctl*/
#define MBOX_ULLCLT_LEN		8

#define REV_MBOX_MASK		0xAA

int __init aml_mhu_fifo_init(void);
void aml_mhu_fifo_exit(void);
#endif
