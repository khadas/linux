/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __MESON_MHU_PL_H__
#define __MESON_MHU_PL_H__

#include <linux/cdev.h>
#include <linux/mailbox_controller.h>
#include <linux/amlogic/meson_mhu_common.h>

#define CHANNEL_PL_MAX		6
#define MHUDEV_MAX		(CHANNEL_PL_MAX / 2)

#define MBOX_PL_SIZE		512
#define RX_OFFSET_STAT          (0x20 << 2)
#define RX_OFFSET_CLR           (0x10 << 2)
#define RX_OFFSET_SET           (0x0 << 2)
#define TX_OFFSET_STAT          (0x21 << 2)
#define TX_OFFSET_CLR           (0x11 << 2)
#define TX_OFFSET_SET           (0x1 << 2)

#define PAYLOAD_MAX_SIZE        0x200
#define RX_PAYLOAD		0x0
#define TX_PAYLOAD		0x200
#define DEBUG_MAILBOX
/*Async 1, sync 2*/
#define MBOX_TX_SIZE            244
#define MBOX_SYNC_TX_SIZE       (MBOX_TX_SIZE + MBOX_COMPLETE_LEN)
#define MBOX_ALLOWED_SIZE       (MBOX_TX_SIZE + MBOX_USER_CMD_LEN)
#define NR_DSP			2

/*user space mailbox cmd len define type int*/
#define MBOX_USER_CMD_LEN	4
/*u64 reserve*/
#define MBOX_RESERVE_LEN	8
/* u64 complete*/
#define MBOX_COMPLETE_LEN	8

#define MBOX_PL_HEAD_SIZE	0x1c
#define MBOX_PL_SCPITODSP_SIZE	MBOX_SYNC_TX_SIZE
#define MBOX_PL_SCPITOSE_SIZE	(0x80 - 0x1c)

int __init aml_mhu_pl_init(void);
void aml_mhu_pl_exit(void);
#endif
