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

#include "meson_mhu.h"
#include "meson_mhu_dsp.h"

struct device *dsp_scpi_device;

#define DRIVER_NAME		"meson_mhu_dsp"

static char *dsp_name[] = {
	"dsp_dev",
	"dspb_dev",
};

/* for list */
spinlock_t lock;
static LIST_HEAD(mbox_list);
static DEFINE_MUTEX(chan_mutex);

enum USR_CMD {
	MBOX_USER_CMD = 0x1001,
};

/*
 * mbox_chan_report
 * Report receive data
 */
static void mbox_chan_report(u32 status, void *msg)
{
	struct mhu_data_buf *data_buf = (struct mhu_data_buf *)msg;
	struct mbox_message *message;
	struct mbox_message *listen_msg = NULL;
	struct list_head *list;
	unsigned long flags;
	struct mbox_data *mbox_data =
		(struct mbox_data *)(data_buf->rx_buf);
	struct device *dev = dsp_scpi_device;

	spin_lock_irqsave(&lock, flags);
	if (list_empty(&mbox_list)) {
		spin_unlock_irqrestore(&lock, flags);
		return;
	}

	list_for_each(list, &mbox_list) {
		message = list_entry(list, struct mbox_message, list);
		dev_dbg(dev, "complete %lx %x %x\n",
			(unsigned long)(mbox_data->complete),
			status & CMD_MASK, message->cmd);
		if ((unsigned long)mbox_data->complete ==
					(unsigned long)&message->complete) {
			memcpy(message->data,
			       mbox_data->data,
			       (status >> SIZE_SHIFT) & SIZE_MASK);
			complete(&message->complete);
			spin_unlock_irqrestore(&lock, flags);
			return;
		} else if (!listen_msg &&
				(status & CMD_MASK) == message->cmd) {
			listen_msg = message;
		}
	}

	spin_unlock_irqrestore(&lock, flags);
	if (listen_msg) {
		dev_dbg(dev, "listen cmd\n");
		memcpy(listen_msg->data,
		       mbox_data->data,
		       (status >> SIZE_SHIFT) & SIZE_MASK);
		complete(&listen_msg->complete);
		return;
	}

	/* Use for cmd judge */
	switch (status & CMD_MASK) {
	case MBOX_USER_CMD:
		break;
	default:
		break;
	};
}

static irqreturn_t mbox_dsp_handler(int irq, void *p)
{
	struct mbox_chan *link = (struct mbox_chan *)p;
	struct mhu_chan *chan = link->con_priv;
	struct mhu_ctlr *ctlr = chan->ctlr;
	void __iomem *mbox_dspa_base = ctlr->mbox_dspa_base;
	void __iomem *mbox_dspb_base = ctlr->mbox_dspb_base;
	void __iomem *mbox_base;
	void __iomem *payload = ctlr->payload_base;
	int idx = chan->index;
	struct mhu_data_buf *data;
	u32 status;

	if (BIT(idx) & 0xC)
		mbox_base = mbox_dspb_base;
	else if (BIT(idx) & 0x3)
		mbox_base = mbox_dspa_base;
	else
		mbox_base = mbox_dspa_base;

	status = readl(mbox_base + RX_OFFSET_STAT);

	if (status && irq == chan->rx_irq) {
		data = chan->data;
		if (!data)
			return IRQ_NONE;
		if (data->rx_buf) {
			memcpy_fromio(data->rx_buf,
				      payload + RX_PAYLOAD(idx),
				      data->rx_size);
			mbox_chan_report(status, data);
			memset(data->rx_buf, 0, data->rx_size);
		}
		writel(~0, mbox_base + RX_OFFSET_CLR);
	}

	return IRQ_HANDLED;
}

static int mhu_transfer_data(struct mbox_chan *link, void *msg)
{
	struct mhu_chan *chan = link->con_priv;
	struct mhu_ctlr *ctlr = chan->ctlr;
	void __iomem *mbox_dspa_base = ctlr->mbox_dspa_base;
	void __iomem *mbox_dspb_base = ctlr->mbox_dspb_base;
	void __iomem *payload = ctlr->payload_base;
	struct mhu_data_buf *data = (struct mhu_data_buf *)msg;
	int idx = chan->index;
	void __iomem *mbox_base;

	if (!data)
		return -EINVAL;

	if (BIT(idx) & 0xC)
		mbox_base = mbox_dspb_base;
	else if (BIT(idx) & 0x3)
		mbox_base = mbox_dspa_base;
	else
		mbox_base = mbox_dspa_base;

	chan->data = data;
	if (data->tx_buf) {
		memset_io(payload + TX_PAYLOAD(idx),
			  0, MHU_BUFFER_SIZE);
		memcpy_toio(payload + TX_PAYLOAD(idx),
			    data->tx_buf, data->tx_size);
	}
	writel(data->cmd, mbox_base + TX_OFFSET_SET);

	return 0;
}

/**
 * mbox_ack_isr_handler
 * AP ack interrupt handler
 */
static irqreturn_t mbox_ack_isr_handler(int irq, void *p)
{
	struct mbox_chan *mbox_chan = (struct mbox_chan *)p;
	struct mhu_chan *mhu_chan = (struct mhu_chan *)mbox_chan->con_priv;
	struct mhu_ctlr *ctlr = mhu_chan->ctlr;
	void __iomem *payload = ctlr->payload_base;
	struct mhu_data_buf *data;
	int mem_idx = mhu_chan->index;

	if (irq == mhu_chan->rx_irq) {
		data = mhu_chan->data;
		if (!data)
			return IRQ_NONE; /* spurious */
		if (data->rx_buf)
			memcpy_fromio(data->rx_buf,
				      payload + TX_PAYLOAD(mem_idx),
				      data->rx_size);
		mhu_chan->data = NULL;
		mbox_chan_received_data(mbox_chan, data);
		complete(&mbox_chan->tx_complete);
	}
	return IRQ_HANDLED;
}

static int mhu_startup(struct mbox_chan *link)
{
	struct mhu_chan *chan = link->con_priv;
	int err, mbox_irq = chan->rx_irq;

	err = request_threaded_irq(mbox_irq, mbox_ack_isr_handler,
				   NULL, IRQF_ONESHOT,
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
	void __iomem *mbox_dspa_base = ctlr->mbox_dspa_base;
	void __iomem *mbox_dspb_base = ctlr->mbox_dspb_base;
	void __iomem *mbox_base;
	int idx = chan->index;

	if (idx / 2)
		mbox_base =  mbox_dspb_base;
	else
		mbox_base =  mbox_dspa_base;
	return !readl(mbox_base + TX_OFFSET_STAT);
}

static struct mbox_chan_ops mhu_ops = {
	.send_data = mhu_transfer_data,
	.startup = mhu_startup,
	.shutdown = mhu_shutdown,
	.last_tx_done = mhu_last_tx_done,
};

static ssize_t mbox_message_write(struct file *filp,
				  const char __user *userbuf,
				  size_t count, loff_t *ppos)
{
	int ret;
	struct mbox_message *mbox_msg;
	struct mbox_client cl = {0};
	struct mbox_chan *chan;
	struct mbox_data mbox_data;
	struct mhu_data_buf data_buf;
	unsigned long flags;
	int cmd;
	struct device *dev = dsp_scpi_device;
	struct inode *inode = filp->f_inode;
	int chan_index = 1;
	struct device *dsp_dev = &mbox_cdev.dsp_dev[0];

	if (inode->i_rdev == mbox_cdev.dsp_dev[1].devt) {
		chan_index = 3;
		dsp_dev = &mbox_cdev.dsp_dev[1];
	}

	if (count > MBOX_ALLOWED_SIZE) {
		dev_err(dev,
			"Message length %zd greater than max allowed\n",
			count);
		return -EINVAL;
	}
	mbox_msg = kzalloc(sizeof(*mbox_msg), GFP_KERNEL);
	if (!mbox_msg) {
		ret = -ENOMEM;
		goto err_probe0;
	}

	mbox_msg->data = kzalloc(sizeof(mbox_data), GFP_KERNEL);
	if (!mbox_msg->data) {
		ret = -ENOMEM;
		goto err_probe1;
	}

	ret = copy_from_user(mbox_data.data, userbuf + 4, count - 4);
	if (ret) {
		ret = -EFAULT;
		goto err_probe2;
	}

	ret = copy_from_user((char *)&cmd, userbuf, 4);
	if (ret) {
		ret = -EFAULT;
		goto err_probe2;
	}

	mbox_msg->cmd = cmd & CMD_MASK;
	init_completion(&mbox_msg->complete);
	mbox_msg->task = current;
	spin_lock_irqsave(&lock, flags);
	list_add_tail(&mbox_msg->list, &mbox_list);
	spin_unlock_irqrestore(&lock, flags);

	/*Listen data not send data to hifi*/
	if (cmd & LISTEN_DATA) {
		ret = count;
		goto err_probe0;
	}

	mbox_data.complete = (unsigned long)(&mbox_msg->complete);
	dev_dbg(dev, "%s %lx\n", __func__, (unsigned long)mbox_data.complete);
	data_buf.tx_buf = (void *)&mbox_data;
	data_buf.tx_size = count - 4 + sizeof(mbox_data) - MBOX_TX_SIZE;
	data_buf.cmd = (mbox_msg->cmd)
		       | ((data_buf.tx_size & SIZE_MASK) << SIZE_SHIFT)
		       | ASYNC_OR_SYNC(1);
	data_buf.rx_buf = NULL;
	cl.dev = dev;
	cl.tx_block = true;
	cl.tx_tout = 2000;
	mutex_lock(&dsp_dev->mutex);
	chan = mbox_request_channel(&cl, chan_index);
	if (IS_ERR(chan)) {
		mutex_unlock(&dsp_dev->mutex);
		dev_err(dev, "Failed Req Chan\n");
		ret = PTR_ERR(chan);
		goto err_probe3;
	}
	ret = mbox_send_message(chan, (void *)(&data_buf));
	mbox_free_channel(chan);
	mutex_unlock(&dsp_dev->mutex);
	if (ret < 0) {
		dev_err(dev, "Failed to send message via mailbox %d\n", ret);
	} else {
		dev_dbg(dev, "Ack OK\n");
		return count;
	}
err_probe3:
	spin_lock_irqsave(&lock, flags);
	list_del(&mbox_msg->list);
	spin_unlock_irqrestore(&lock, flags);
err_probe2:
	kfree(mbox_msg->data);
err_probe1:
	kfree(mbox_msg);
err_probe0:
	return ret;
}

static ssize_t mbox_message_read(struct file *filp, char __user *userbuf,
				 size_t count, loff_t *ppos)
{
	int ret;
	struct mbox_message *msg;
	struct list_head *list;
	unsigned long flags;
	struct device *dev = dsp_scpi_device;

	spin_lock_irqsave(&lock, flags);
	if (list_empty(&mbox_list)) {
		spin_unlock_irqrestore(&lock, flags);
		return -ENXIO;
	}
	list_for_each(list, &mbox_list) {
		msg = list_entry(list, struct mbox_message, list);
		if (msg->task == current) {
			spin_unlock_irqrestore(&lock, flags);
			wait_for_completion(&msg->complete);
			dev_dbg(dev, "Wait end %s\n", msg->data);
			break;
		}
	}
	if (list == &mbox_list) {
		dev_err(dev, "List is null or not find data\n");
		spin_unlock_irqrestore(&lock, flags);
		return -ENXIO;
	}
	*ppos = 0;
	ret = simple_read_from_buffer(userbuf, count, ppos,
				      msg->data, MBOX_TX_SIZE);

	spin_lock_irqsave(&lock, flags);
	list_del(list);
	spin_unlock_irqrestore(&lock, flags);
	kfree(msg->data);
	kfree(msg);
	return ret;
}

static const struct file_operations mbox_message_ops = {
	.write	= mbox_message_write,
	.read	= mbox_message_read,
	.open	= simple_open,
};

static int init_char_cdev(struct device *dev)
{
	int err;
	int nr_minor = 0;
	int i = 0;

	err = of_property_read_u32(dev->of_node,
				   "nr-dsp", &nr_minor);
	if (err) {
		dev_err(dev, "failed to nr-dsp\n");
		return -EINVAL;
	}
	if (nr_minor == 0 || nr_minor > NR_DSP)
		nr_minor = NR_DSP;

	err = alloc_chrdev_region(&mbox_cdev.dsp_no, 0, nr_minor, DRIVER_NAME);
	if (err < 0) {
		dev_err(dev, "%s dsp alloc dev_t number failed\n", __func__);
		err = -1;
		goto err2;
	}

	for (i = 0; i < nr_minor; i++) {
		mbox_cdev.dsp_dev[i].init_name = dsp_name[i];
		cdev_init(&mbox_cdev.dsp_cdev[i], &mbox_message_ops);
		mbox_cdev.dsp_cdev[i].owner = THIS_MODULE;
		mbox_cdev.dsp_dev[i].devt = MKDEV(MAJOR(mbox_cdev.dsp_no), i);
		device_initialize(&mbox_cdev.dsp_dev[i]);
		err = cdev_device_add(&mbox_cdev.dsp_cdev[i],
				      &mbox_cdev.dsp_dev[i]);
		if (err < 0) {
			dev_err(dev, "%s: could not add character device\n",
				mbox_cdev.dsp_dev[i].init_name);
			goto err1;
		}
	}
	return 0;
err1:
	put_device(&mbox_cdev.dsp_dev[0]);
	put_device(&mbox_cdev.dsp_dev[1]);
err2:
	return err;
}

static int mhu_dsp_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mhu_ctlr *mhu_ctrl;
	struct mhu_chan *mhu_chan;
	struct mbox_chan *mbox_chans;
	struct resource *res;
	int idx, num_chans;
	int err = 0;

	mhu_ctrl = devm_kzalloc(dev, sizeof(*mhu_ctrl), GFP_KERNEL);
	if (!mhu_ctrl)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(dev, "failed to get mailbox memory resource\n");
		return -ENXIO;
	}
	mhu_ctrl->mbox_dspa_base = devm_ioremap_resource(dev, res);
	if (IS_ERR(mhu_ctrl->mbox_dspa_base))
		return PTR_ERR(mhu_ctrl->mbox_dspa_base);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!res) {
		dev_err(dev, "failed to get mailbox memory resource\n");
		return -ENXIO;
	}
	mhu_ctrl->mbox_dspb_base = devm_ioremap_resource(dev, res);
	if (IS_ERR(mhu_ctrl->mbox_dspb_base))
		return PTR_ERR(mhu_ctrl->mbox_dspb_base);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	if (!res) {
		dev_err(dev, "failed to get payload memory resource\n");
		return -ENXIO;
	}
	mhu_ctrl->payload_base = devm_ioremap_resource(dev, res);
	if (IS_ERR(mhu_ctrl->payload_base))
		return PTR_ERR(mhu_ctrl->payload_base);

	mhu_ctrl->dev = dev;
	platform_set_drvdata(pdev, mhu_ctrl);

	num_chans = 0;
	err = of_property_read_u32(dev->of_node,
				   "mbox-nums", &num_chans);
	if (err) {
		dev_err(dev, "failed to mbox-nums\n");
		return -EINVAL;
	}
	if (!num_chans)
		num_chans = CHANNEL_MAX;
	mbox_chans = devm_kzalloc(dev,
				  sizeof(*mbox_chans) * num_chans,
				  GFP_KERNEL);
	if (!mbox_chans)
		return -ENOMEM;

	mhu_ctrl->mbox_con.chans = mbox_chans;
	mhu_ctrl->mbox_con.num_chans = num_chans;
	mhu_ctrl->mbox_con.txdone_irq = true;
	mhu_ctrl->mbox_con.ops = &mhu_ops;
	mhu_ctrl->mbox_con.dev = dev;

	mhu_ctrl->channels = devm_kzalloc(dev,
					  sizeof(struct mhu_chan) * num_chans,
					  GFP_KERNEL);
	for (idx = 0; idx < num_chans; idx++) {
		mhu_chan = &mhu_ctrl->channels[idx];
		mhu_chan->index = idx;
		mhu_chan->ctlr = mhu_ctrl;
		mhu_chan->rx_irq = platform_get_irq(pdev, idx);

		if (mhu_chan->rx_irq < 0) {
			dev_err(dev, "failed to get interrupt %d\n", idx);
			return -ENXIO;
		}
		mbox_chans[idx].con_priv = mhu_chan;
	}

	if (mbox_controller_register(&mhu_ctrl->mbox_con)) {
		dev_err(dev, "failed to register mailbox controller\n");
		return -ENOMEM;
	}

	dsp_scpi_device = dev;

	for (idx = 0; idx < num_chans; idx++) {
		if (BIT(idx) & 0xA)
			continue;
		mhu_chan =  &mhu_ctrl->channels[idx];
		mhu_chan->index = idx;
		mhu_chan->data = devm_kzalloc(dev,
					      sizeof(struct mhu_data_buf),
					      GFP_KERNEL);
		mhu_chan->data->rx_buf = devm_kzalloc(dev,
						      MHU_BUFFER_SIZE,
						      GFP_KERNEL);
		if (!mhu_chan->data->rx_buf)
			return -ENOMEM;
		mhu_chan->data->rx_size = MHU_BUFFER_SIZE;
		err = request_threaded_irq(mhu_chan->rx_irq, mbox_dsp_handler,
					   NULL, IRQF_ONESHOT,
					   DRIVER_NAME, &mbox_chans[idx]);
		if (err) {
			dev_err(dev, "request irq error\n");
			return err;
		}
	}

	err = init_char_cdev(dev);
	if (err < 0) {
		pr_info("init cdev fail\n");
		return err;
	}
	pr_info("dsp mailbox init done\n");
	return 0;
}

static int mhu_remove(struct platform_device *pdev)
{
	struct mhu_ctlr *ctlr = platform_get_drvdata(pdev);

	mbox_controller_unregister(&ctlr->mbox_con);

	platform_set_drvdata(pdev, NULL);
	return 0;
}

static const struct of_device_id mhu_of_match[] = {
	{ .compatible = "amlogic, meson_mhu_dsp" },
	{},
};

static struct platform_driver mhu_driver = {
	.probe = mhu_dsp_probe,
	.remove = mhu_remove,
	.driver = {
		.owner		= THIS_MODULE,
		.name = DRIVER_NAME,
		.of_match_table = mhu_of_match,
	},
};

int mhu_dsp_init(void)
{
	return platform_driver_register(&mhu_driver);
}

void mhu_dsp_exit(void)
{
	platform_driver_unregister(&mhu_driver);
}
