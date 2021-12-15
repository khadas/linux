// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/export.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/mailbox_controller.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/notifier.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/mailbox_client.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/list.h>
#include "meson_mhu_sec.h"

#define REV_MBOX_MASK		0xAAAA
#define DRIVER_NAME		"secmbox"
struct sp_mb_csr {
	union {
		u32 d32;
		struct {
			/* words lengthm1   [7:0]   */
			unsigned lengthm1:8;
			/* Full             [8]     */
			unsigned full:1;
			/* Empty            [9]     */
			unsigned empty:1;
			/* Reserved         [15:10] */
			unsigned reserved_1:6;
			/* A2B disable      [16]    */
			unsigned a2b_dis:1;
			/* A2B state        [19:17] */
			unsigned a2b_st:3;
			/* A2B error        [20]    */
			unsigned a2b_err:1;
			/* Reserved         [31:21] */
			unsigned reserved_2:11;
		} b;
	} reg;
};

/*
 * This function writes size of data into buffer of mailbox
 * for sending.
 *
 * Note:
 * Due to hardware design, data size for mailbox send/recv
 * should be a **MULTIPLE** of 4 bytes.
 */
static void mb_write(void *to, void *from, u32 size)
{
	u32 *rd_ptr = (u32 *)from;
	u32 *wr_ptr = (u32 *)to;
	u32 i = 0;

	if (!rd_ptr || !wr_ptr) {
		pr_debug("Invalid inputs for mb_write_buf!\n");
		return;
	}

	for (i = 0; i < size; i += sizeof(u32))
		writel(*rd_ptr++, wr_ptr++);
}

/*
 * This function read size of data from buffer of mailbox
 * for receiving.
 *
 * Note:
 * Due to hardware design, data size for mailbox send/recv
 * should be a **MULTIPLE** of 4 bytes.
 */
static void mb_read(void *to, void *from, u32 size)
{
	u32 *rd_ptr = (u32 *)from;
	u32 *wr_ptr = (u32 *)to;
	u32 i = 0;

	if (!rd_ptr || !wr_ptr) {
		pr_debug("Invalid inputs for mb_read_buf!\n");
		return;
	}

	for (i = 0; i < size; i += sizeof(u32))
		*wr_ptr++ = readl(rd_ptr++);
}

static irqreturn_t mbox_handler(int irq, void *p)
{
	struct mbox_chan *mbox_chan = (struct mbox_chan *)p;
	struct mhu_chan *mhu_chan = mbox_chan->con_priv;
	struct mhu_ctlr *ctlr = mhu_chan->ctlr;
	void __iomem *mbox_scpu2nee_csr = ctlr->mbox_rd_base;
	void __iomem *mbox_scpu2nee_data_st = ctlr->mbox_fsts_base;
	struct mhu_data_buf *data;
	struct sp_mb_csr vcsr;
	u32 rx_size;

	vcsr.reg.d32 = readl(mbox_scpu2nee_csr);
	rx_size = (vcsr.reg.b.lengthm1 + 1) * sizeof(u32);
	if (vcsr.reg.b.full) {
		data = mhu_chan->data;
		if (!data)
			return IRQ_NONE;

		if (data->rx_buf) {
			mb_read(data->rx_buf, mbox_scpu2nee_data_st, rx_size);
			data->rx_size = rx_size;
			mbox_chan_received_data(mbox_chan, data);
		}
	}

	return IRQ_HANDLED;
}

static int mhu_send_data(struct mbox_chan *mbox_chan, void *msg)
{
	struct mhu_chan *chan = mbox_chan->con_priv;
	struct mhu_ctlr *ctlr = chan->ctlr;
	void __iomem *mbox_nee2scpu_csr = ctlr->mbox_wr_base;
	void __iomem *mbox_nee2scpu_data_st = ctlr->mbox_fset_base;
	struct mhu_data_buf *data = (struct mhu_data_buf *)msg;
	u32 tx_size = 0;
	struct sp_mb_csr vcsr;

	if (!data)
		return -EINVAL;

	tx_size = data->tx_size & 0xff;
	chan->data = data;

	vcsr.reg.d32 = readl(mbox_nee2scpu_csr);
	vcsr.reg.b.lengthm1 = (tx_size / sizeof(u32)) - 1;
	writel(vcsr.reg.d32, mbox_nee2scpu_csr);

	if (data->tx_buf) {
		mb_write(mbox_nee2scpu_data_st,
			 data->tx_buf, tx_size);
	}
	return 0;
}

static int mhu_startup(struct mbox_chan *mbox_chan)
{
	struct mhu_chan *mhu_chan = mbox_chan->con_priv;
	struct mbox_client *cl = mbox_chan->cl;
	int err = 0;

	err = request_threaded_irq(mhu_chan->rx_irq, mbox_handler,
			NULL, IRQF_ONESHOT | IRQF_NO_SUSPEND,
			DRIVER_NAME, mbox_chan);
	if (err)
		dev_err(cl->dev, "request irq error\n");
	return err;
}

static void mhu_shutdown(struct mbox_chan *mbox_chan)
{
	struct mhu_chan *mhu_chan = mbox_chan->con_priv;

	mhu_chan->data = NULL;
	free_irq(mhu_chan->rx_irq, mbox_chan);
}

static bool mhu_last_tx_done(struct mbox_chan *link)
{
	struct mhu_chan *chan = link->con_priv;
	struct mhu_ctlr *ctlr = chan->ctlr;
	void __iomem *mbox_nee2scpu_csr = ctlr->mbox_wr_base;
	struct sp_mb_csr vcsr;

	vcsr.reg.d32 = readl(mbox_nee2scpu_csr);
	return !(vcsr.reg.b.empty);
}

static struct mbox_chan_ops mhu_ops = {
	.send_data = mhu_send_data,
	.startup = mhu_startup,
	.shutdown = mhu_shutdown,
	.last_tx_done = mhu_last_tx_done,
};

static int mhu_sec_probe(struct platform_device *pdev)
{
	struct mhu_ctlr *mhu_ctlr;
	struct mhu_chan *mhu_chan;
	struct mbox_chan *mbox_chans;
	struct device *dev = &pdev->dev;
	struct resource *res;
	int idx, err;
	u32 num_chans = 0;

	pr_debug("mhu sec start\n");
	mhu_ctlr = devm_kzalloc(dev, sizeof(*mhu_ctlr), GFP_KERNEL);
	if (!mhu_ctlr)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(dev, "failed to get mailbox memory resource 0\n");
		return -ENXIO;
	}
	mhu_ctlr->mbox_wr_base = devm_ioremap_resource(dev, res);
	if (IS_ERR_OR_NULL(mhu_ctlr->mbox_wr_base))
		return PTR_ERR(mhu_ctlr->mbox_wr_base);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!res) {
		dev_err(dev, "failed to get mailbox memory resource 1\n");
		return -ENXIO;
	}
	mhu_ctlr->mbox_rd_base = devm_ioremap_resource(dev, res);
	if (IS_ERR_OR_NULL(mhu_ctlr->mbox_rd_base))
		return PTR_ERR(mhu_ctlr->mbox_rd_base);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	if (!res) {
		dev_err(dev, "failed to get mailbox memory resource 2\n");
		return -ENXIO;
	}
	mhu_ctlr->mbox_fset_base = devm_ioremap_resource(dev, res);
	if (IS_ERR(mhu_ctlr->mbox_fset_base))
		return PTR_ERR(mhu_ctlr->mbox_fset_base);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 3);
	if (!res) {
		dev_err(dev, "failed to get mailbox memory resource 3\n");
		return -ENXIO;
	}
	mhu_ctlr->mbox_fsts_base = devm_ioremap_resource(dev, res);
	if (IS_ERR(mhu_ctlr->mbox_fsts_base))
		return PTR_ERR(mhu_ctlr->mbox_fsts_base);

	mhu_ctlr->dev = dev;
	platform_set_drvdata(pdev, mhu_ctlr);

	err = of_property_read_u32(dev->of_node,
				   "mbox-nums", &num_chans);
	if (err) {
		dev_err(dev, "failed to get mbox num %d\n", err);
		return -ENXIO;
	}

	mbox_chans = devm_kzalloc(dev,
				  sizeof(*mbox_chans) * num_chans,
				  GFP_KERNEL);
	if (!mbox_chans)
		return -ENOMEM;

	mhu_ctlr->mbox_con.chans = mbox_chans;
	mhu_ctlr->mbox_con.num_chans = num_chans;
	mhu_ctlr->mbox_con.txdone_irq = true;
	mhu_ctlr->mbox_con.ops = &mhu_ops;
	mhu_ctlr->mbox_con.dev = dev;

	mhu_ctlr->channels = devm_kzalloc(dev,
					  sizeof(struct mhu_chan) * num_chans,
					  GFP_KERNEL);

	for (idx = 0; idx < num_chans; idx++) {
		mhu_chan = &mhu_ctlr->channels[idx];
		mhu_chan->index = idx;
		mhu_chan->mhu_id = idx;//mhu_sec_ctlr->mhu_id[idx];
		mhu_chan->ctlr = mhu_ctlr;
		pr_debug("%s, sec chan index: %d, idx: %d, sec_mbu_id:%d\n",
			 __func__, mhu_chan->index, idx, mhu_chan->mhu_id);

		mbox_chans[idx].con_priv = mhu_chan;
	}
	if (mbox_controller_register(&mhu_ctlr->mbox_con)) {
		dev_err(dev, "failed to register mailbox controller\n");
		return -ENOMEM;
	}
	for (idx = 0; idx < num_chans; idx++) {
		if (BIT(idx) & REV_MBOX_MASK)
			continue;
		mhu_chan =  &mhu_ctlr->channels[idx];
		mhu_chan->index = idx;
		mhu_chan->rx_irq = platform_get_irq(pdev, idx / 2);
		pr_debug("rx_irq: %X\n", mhu_chan->rx_irq);
		if (mhu_chan->rx_irq < 0) {
			dev_err(dev, "failed to get interrupt %d\n", idx / 2);
			return -ENXIO;
		}

		mhu_chan->data = devm_kzalloc(dev,
					      sizeof(struct mhu_data_buf),
					      GFP_KERNEL);
		if (!mhu_chan->data)
			return -ENOMEM;

		mhu_chan->data->rx_buf = devm_kzalloc(dev,
						      MBOX_SEC_SIZE,
						      GFP_KERNEL);
		if (!mhu_chan->data->rx_buf)
			return -ENOMEM;

		mhu_chan->data->rx_size = MBOX_SEC_SIZE;
	}

	/*set mhu type*/
	mhu_f |= MASK_MHU_SEC;
	pr_info("mbox sec init done\n");
	return 0;
}

static int mhu_sec_remove(struct platform_device *pdev)
{
	struct mhu_ctlr *ctlr = platform_get_drvdata(pdev);

	mbox_controller_unregister(&ctlr->mbox_con);

	platform_set_drvdata(pdev, NULL);

	return 0;
}

static const struct of_device_id mhu_of_match[] = {
	{ .compatible = "amlogic, meson_mhu_sec" },
	{},
};

static struct platform_driver mhu_sec_driver = {
	.probe = mhu_sec_probe,
	.remove = mhu_sec_remove,
	.driver = {
		.owner		= THIS_MODULE,
		.name = DRIVER_NAME,
		.of_match_table = mhu_of_match,
	},
};

int __init aml_mhu_sec_init(void)
{
	return platform_driver_register(&mhu_sec_driver);
}

void __exit aml_mhu_sec_exit(void)
{
	platform_driver_unregister(&mhu_sec_driver);
}
