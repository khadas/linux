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

#include "meson_mhu.h"
#include "../firmware/bl40_module.h"

struct device *the_scpi_device;
u32 num_scp_chans;
u32 send_listen_chans;
u32 isr_send;
u32 isr_m4;

#define DRIVER_NAME		"meson_mhu"
#define MHU_BUFFER_SIZE		0x100
/*
 * +--------------------+-------+---------------+
 * |  Hardware Register | Offset|  Driver View  |
 * +--------------------+-------+---------------+
 * |  SCP_INTR_L_STAT   | 0x014 |  RX_STATUS(L) |
 * |  SCP_INTR_L_SET    | 0x010 |  RX_SET(L)    |
 * |  SCP_INTR_L_CLEAR  | 0x018 |  RX_CLEAR(L)  |
 * +--------------------+-------+---------------+
 * |  SCP_INTR_H_STAT   | 0x020 |  RX_STATUS(H) |
 * |  SCP_INTR_H_SET    | 0x01c |  RX_SET(H)    |
 * |  SCP_INTR_H_CLEAR  | 0x024 |  RX_CLEAR(H)  |
 * +--------------------+-------+---------------+
 * |  CPU_INTR_L_STAT   | 0x038 |  TX_STATUS(L) |
 * |  CPU_INTR_L_SET    | 0x034 |  TX_SET(L)    |
 * |  CPU_INTR_L_CLEAR  | 0x03c |  TX_CLEAR(L)  |
 * +--------------------+-------+---------------+
 * |  CPU_INTR_H_STAT   | 0x044 |  TX_STATUS(H) |
 * |  CPU_INTR_H_SET    | 0x040 |  TX_SET(H)    |
 * |  CPU_INTR_H_CLEAR  | 0x048 |  TX_CLEAR(H)  |
 * +--------------------+-------+---------------+
 */
#define RX_OFFSET(chan)     (0x10 + (chan) * 0xc)
#define RX_STATUS(chan)     (RX_OFFSET(chan) + 0x4)
#define RX_SET(chan)        RX_OFFSET(chan)
#define RX_CLEAR(chan)      (RX_OFFSET(chan) + 0x8)

#define TX_OFFSET(chan)     (0x34 + (chan) * 0xc)
#define TX_STATUS(chan)     (TX_OFFSET(chan) + 0x4)
#define TX_SET(chan)        TX_OFFSET(chan)
#define TX_CLEAR(chan)      (TX_OFFSET(chan) + 0x8)

/*
 * +---------------+-------+----------------+
 * |    Payload    | Offset|  Driver View   |
 * +---------------+-------+----------------+
 * |  SCP->AP Low  | 0x000 |  RX_PAYLOAD(L) |
 * |  SCP->AP High | 0x400 |  RX_PAYLOAD(H) |
 * +---------------+-------+----------------+
 * |  AP->SCP Low  | 0x200 |  TX_PAYLOAD(H) |
 * |  AP->SCP High | 0x600 |  TX_PAYLOAD(H) |
 * +---------------+-------+----------------+
 */
#define PAYLOAD_MAX_SIZE	0x200
#define PAYLOAD_OFFSET		0x400
#define RX_PAYLOAD(chan)	((chan) * PAYLOAD_OFFSET)
#define TX_PAYLOAD(chan)	((chan) * PAYLOAD_OFFSET + PAYLOAD_MAX_SIZE)

struct mhu_chan {
	int index;
	int rx_irq;
	struct mhu_ctlr *ctlr;
	struct mhu_data_buf *data;
};

struct mhu_ctlr {
	struct device *dev;
	void __iomem *mbox_base;
	void __iomem *payload_base;
	struct mbox_controller mbox_con;
	struct mhu_chan *channels;
};

void bl40_rx_callback(struct mbox_client *cl, void *msg)
{
	struct mhu_data_buf *data = (struct mhu_data_buf *)msg;

	pr_debug("call %s\n", __func__);
	bl40_rx_msg(data->rx_buf, data->rx_size);
}

static irqreturn_t mbox_handler(int irq, void *p)
{
	struct mbox_chan *link = (struct mbox_chan *)p;
	struct mhu_chan *chan = link->con_priv;
	struct mhu_ctlr *ctlr = chan->ctlr;
	void __iomem *mbox_base = ctlr->mbox_base;
	void __iomem *payload = ctlr->payload_base;
	int idx = chan->index;
	struct mhu_data_buf *data;
	u32 status = 0;
	u32 is_send_isr = BIT(idx) & isr_send;
	u32 is_send_chan = BIT(idx) & send_listen_chans;

	if (isr_m4) {
		if (BIT(idx) & isr_m4)
			idx = 1;
		else
			idx = 0;
	}

	if (is_send_isr)
		status = 1;
	else
		status = readl(mbox_base + RX_STATUS(idx));

	pr_debug("isr %d idx %x sts %x\n", irq, idx, status);
	if (status && irq == chan->rx_irq) {
		data = chan->data;
		if (!data) {
			pr_err("data is null\n");
			return IRQ_NONE; /* spurious */
		}
		if (data->rx_buf) {
			if (is_send_isr) {
				memcpy(data->rx_buf, payload + TX_PAYLOAD(idx),
				       data->rx_size);
			} else {
				/*
				 * idx = 1 & to scp chans = 1
				 * is mailbox to m4, need to get size
				 * to m3 only low no need to get size
				 * idx = 1 & to scp chans = 2
				 * is mailbox no to m4,that mailbox chan
				 * low high all to m3, no need get size
				 */
				if (idx && num_scp_chans != CHANNEL_MAX)
					data->rx_size =
					readl(mbox_base + RX_STATUS(idx));

				memcpy(data->rx_buf, payload + RX_PAYLOAD(idx),
				       data->rx_size);
			}
		}
		mbox_chan_received_data(link, data);
		if (!is_send_isr)
			writel(~0, mbox_base + RX_CLEAR(idx));
		if (is_send_chan)
			chan->data = NULL;
	}

	return IRQ_HANDLED;
}

static int mhu_send_data(struct mbox_chan *link, void *msg)
{
	struct mhu_chan *chan = link->con_priv;
	struct mhu_ctlr *ctlr = chan->ctlr;
	void __iomem *mbox_base = ctlr->mbox_base;
	void __iomem *payload = ctlr->payload_base;
	struct mhu_data_buf *data = (struct mhu_data_buf *)msg;
	int idx = chan->index;
	u8 datatmp[512] = {0};

	if (!data)
		return -EINVAL;

	chan->data = data;
	if (isr_m4) {
		if (BIT(idx) & isr_m4)
			idx = 1;
		else
			idx = 0;
	}
	memcpy(datatmp, data->tx_buf, data->tx_size);
	if (data->tx_buf)
		memcpy(payload + TX_PAYLOAD(idx), datatmp, data->tx_size);

	writel(data->cmd, mbox_base + TX_SET(idx));

	return 0;
}

static int mhu_startup(struct mbox_chan *link)
{
	struct mhu_chan *chan = link->con_priv;
	int err, mbox_irq = chan->rx_irq;

	err = request_threaded_irq(mbox_irq, mbox_handler, NULL, IRQF_ONESHOT,
				   DRIVER_NAME, link);
	return err;
}

static void mhu_shutdown(struct mbox_chan *link)
{
	struct mhu_chan *chan = link->con_priv;

	chan->data = NULL;
	free_irq(chan->rx_irq, link);
}

static bool mhu_last_tx_done(struct mbox_chan *link)
{
	struct mhu_chan *chan = link->con_priv;
	struct mhu_ctlr *ctlr = chan->ctlr;
	void __iomem *mbox_base = ctlr->mbox_base;
	int idx = chan->index;

	return !readl(mbox_base + TX_STATUS(idx));
}

static struct mbox_chan_ops mhu_ops = {
	.send_data = mhu_send_data,
	.startup = mhu_startup,
	.shutdown = mhu_shutdown,
	.last_tx_done = mhu_last_tx_done,
};

static int mhu_probe(struct platform_device *pdev)
{
	struct mhu_ctlr *ctlr;
	struct mhu_chan *chan;
	struct device *dev = &pdev->dev;
	struct mbox_chan *l;
	struct resource *res;
	struct mbox_client *cl;
	int idx, err;
	u32 mbox_chans = 0;
	int bit_chans = 0;
	int ret;
	static const char * const channel_names[] = {
		CHANNEL_LOW_PRIORITY,
		CHANNEL_HIGH_PRIORITY
	};

	ctlr = devm_kzalloc(dev, sizeof(*ctlr), GFP_KERNEL);
	if (!ctlr)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(dev, "failed to get mailbox memory resource\n");
		return -ENXIO;
	}

	ctlr->mbox_base = devm_ioremap_resource(dev, res);
	if (IS_ERR(ctlr->mbox_base))
		return PTR_ERR(ctlr->mbox_base);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!res) {
		dev_err(dev, "failed to get payload memory resource\n");
		return -ENXIO;
	}

	ctlr->payload_base = devm_ioremap_resource(dev, res);
	if (IS_ERR(ctlr->payload_base))
		return PTR_ERR(ctlr->payload_base);

	ctlr->dev = dev;
	platform_set_drvdata(pdev, ctlr);

	num_scp_chans = 0;
	ret = of_property_read_u32(dev->of_node, "num-chans-to-scp",
				   &num_scp_chans);
	if (ret) {
		dev_err(dev, "failed to num-chans-to-scp\n");
		return -EINVAL;
	}
	if (num_scp_chans == 0 || num_scp_chans > 2)
		num_scp_chans = CHANNEL_MAX;
	send_listen_chans = 0;
	ret = of_property_read_u32(dev->of_node, "send-isr-bits",
				   &send_listen_chans);
	if (ret) {
		dev_err(dev, "failed to send-isr-bits\n");
		return -EINVAL;
	}
	ret = of_property_read_u32(dev->of_node, "ack-isr-bits", &isr_send);
	if (ret) {
		dev_err(dev, "failed to ack-isr-bits\n");
		return -EINVAL;
	}
	ret = of_property_read_u32(dev->of_node, "m4-isr-bits", &isr_m4);
	if (ret) {
		dev_err(dev, "failed to m4-isr-bits\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(dev->of_node, "mbox-chans", &mbox_chans);
	if (ret) {
		dev_err(dev, "failed to mbox-chans\n");
		return -EINVAL;
	}
	if (!mbox_chans)
		mbox_chans = CHANNEL_MAX;
	l = devm_kzalloc(dev, sizeof(*l) * mbox_chans, GFP_KERNEL);
	if (!l)
		return -ENOMEM;

	ctlr->channels = devm_kzalloc(dev,
				      sizeof(struct mhu_chan) * mbox_chans,
				      GFP_KERNEL);
	if (!ctlr->channels)
		return -ENOMEM;
	ctlr->mbox_con.chans = l;
	ctlr->mbox_con.num_chans = mbox_chans;
	ctlr->mbox_con.txdone_irq = true;
	ctlr->mbox_con.ops = &mhu_ops;
	ctlr->mbox_con.dev = dev;

	for (idx = 0; idx < mbox_chans; idx++) {
		chan = &ctlr->channels[idx];
		chan->index = idx;
		chan->ctlr = ctlr;
		chan->rx_irq = platform_get_irq(pdev, idx);

		if (chan->rx_irq < 0) {
			dev_err(dev, "failed to get interrupt for %s\n",
				channel_names[idx]);
			return -ENXIO;
		}
		l[idx].con_priv = chan;
		bit_chans |= BIT(idx);
	}

	if (mbox_controller_register(&ctlr->mbox_con)) {
		dev_err(dev, "failed to register mailbox controller\n");
		return -ENOMEM;
	}

	the_scpi_device = dev;

	if (!send_listen_chans)
		goto probe_done;

	for (idx = 0; idx < mbox_chans; idx++) {
		if (BIT(idx) & send_listen_chans)
			continue;
		cl = devm_kzalloc(dev, sizeof(struct mbox_client),
				  GFP_KERNEL);
		cl->dev = dev;
		cl->rx_callback = bl40_rx_callback;
		l[idx].cl = cl;
		chan =  &ctlr->channels[idx];
		chan->data = devm_kzalloc(dev,
					  sizeof(struct mhu_data_buf),
					  GFP_KERNEL);
		chan->data->rx_buf = devm_kzalloc(dev,
						  MHU_BUFFER_SIZE,
						  GFP_KERNEL);
		if (!chan->data->rx_buf)
			return -ENOMEM;
		chan->data->rx_size = MHU_BUFFER_SIZE;
		err = request_threaded_irq(chan->rx_irq, mbox_handler,
					   NULL, IRQF_ONESHOT, DRIVER_NAME,
					   &l[idx]);
		if (err) {
			dev_err(dev, "request irq error\n");
			return err;
		}
	}
probe_done:
	return 0;
}

static int mhu_remove(struct platform_device *pdev)
{
	struct mhu_ctlr *ctlr = platform_get_drvdata(pdev);
	struct device *dev = &pdev->dev;

	mbox_controller_unregister(&ctlr->mbox_con);
	devm_kfree(dev, ctlr->mbox_con.chans);

	devm_iounmap(dev, ctlr->payload_base);
	devm_iounmap(dev, ctlr->mbox_base);

	platform_set_drvdata(pdev, NULL);
	devm_kfree(dev, ctlr);
	return 0;
}

static const struct of_device_id mhu_of_match[] = {
	{ .compatible = "amlogic, meson_mhu" },
	{},
};

static struct platform_driver mhu_driver = {
	.probe = mhu_probe,
	.remove = mhu_remove,
	.driver = {
		.owner		= THIS_MODULE,
		.name = DRIVER_NAME,
		.of_match_table = mhu_of_match,
	},
};

static int __init mhu_init(void)
{
	int ret;

	ret = mhu_dsp_init();
	if (ret)
		return ret;

	ret = platform_driver_register(&mhu_driver);
	if (ret) {
		mhu_dsp_exit();
		return ret;
	}

	return ret;
}
core_initcall(mhu_init);

static void __exit mhu_exit(void)
{
	mhu_dsp_exit();
	platform_driver_unregister(&mhu_driver);
}
module_exit(mhu_exit);

MODULE_AUTHOR("yan wang <yan.wang@amlogic.com>");
MODULE_DESCRIPTION("MESON MHU mailbox driver");
MODULE_LICENSE("GPL");
