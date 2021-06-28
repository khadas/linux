/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _MESON_MHU_COMMON_H_
#define _MESON_MHU_COMMON_H_
#include <linux/cdev.h>
#include <linux/mailbox_controller.h>

#define ASYNC_CMD		1
#define SYNC_CMD		2
#define SYNC_SHIFT(val)		((val) << 25)
#define SYNC_MASK		0x7

#define SIZE_MASK		0x1FF
#define SIZE_SHIFT(val)		(((val) & SIZE_MASK) << 16)
#define SIZE_LEN(val)		(((val) >> 16) & SIZE_MASK)

#define CMD_MASK                0xFFFF
#define CMD_SHIFT(val)		(((val) & CMD_MASK) << 0)

#define TYPE_SHIFT              16
#define TYPE_VALUE(val)         ((val) << TYPE_SHIFT)

#define MBOX_TIME_OUT		10000

#define MASK_MHU                (BIT(0))
#define MASK_MHU_FIFO           (BIT(1))
#define MASK_MHU_PL             (BIT(2))
#define MASK_MHU_SEC            (BIT(3))
#define MASK_MHU_ALL            (MASK_MHU | MASK_MHU_FIFO | MASK_MHU_PL | MASK_MHU_SEC)

#define MBOX_MAX		6
#define MHUDEV_MAX		(MBOX_MAX / 2)
#define CDEV_NAME_SIZE		32

extern struct device *mhu_device;
extern struct device *mhu_fifo_device;
extern struct device *mhu_pl_device;
extern struct device *mhu_sec_device;

extern u32 mhu_f;

struct mhu_data_buf {
	u32 cmd;
	int tx_size;
	void *tx_buf;
	int rx_size;
	void *rx_buf;
	void *cl_data;
	int head_off;
};

enum call_type {
	LISTEN_CALLBACK = TYPE_VALUE(0),
	LISTEN_DATA = TYPE_VALUE(1),
};

struct mbox_message {
	struct list_head list;
	struct task_struct *task;
	struct completion complete;
	int cmd;
	char *data;
};

struct mhu_chan {
	int index;
	int rx_irq;
	int mhu_id;
	char mhu_name[CDEV_NAME_SIZE];
	struct mhu_ctlr *ctlr;
	struct mhu_data_buf *data;
};

struct mhu_mbox {
	int channel_id;
	/*for mhu channel*/
	struct mutex mutex;
	struct list_head char_list;
	dev_t char_no;
	struct cdev char_cdev;
	struct device *char_dev;
	char char_name[32];
	int mhu_id;
	struct device *mhu_dev;
	/*mhu lock for mhu hw reg*/
	spinlock_t mhu_lock;
};

struct mhu_ctlr {
	struct device *dev;
	/*for old mhu*/
	void __iomem *mbox_base;
	void __iomem *payload_base;
	/*for fifo mhu*/
	void __iomem *mbox_wr_base;
	void __iomem *mbox_rd_base;
	void __iomem *mbox_fset_base;
	void __iomem *mbox_fclr_base;
	void __iomem *mbox_fsts_base;
	void __iomem *mbox_irq_base;
	void __iomem *mbox_payload_base;
	int mhu_id[MBOX_MAX];
	int mhu_irq;
	int mhu_irqctlr;
	int mhu_irqmax;
	/*for pl mhu*/
	void __iomem *mbox_sts_base[MHUDEV_MAX];
	void __iomem *mbox_set_base[MHUDEV_MAX];
	void __iomem *mbox_clr_base[MHUDEV_MAX];
	void __iomem *mbox_pl_base[MHUDEV_MAX];
	/*for common*/
	struct mutex mutex;
	struct mbox_controller mbox_con;
	struct mhu_chan *channels;
};

struct mhu_dev {
	int chan_idx;
	u32 dest;
	u32 r_size;
	struct list_head list;
	dev_t char_no;
	struct cdev char_cdev;
	struct device *dev;
	struct device *p_dev;
	const char *name;
	char *data;
	bool busy;
	struct completion complete;

};
#endif
