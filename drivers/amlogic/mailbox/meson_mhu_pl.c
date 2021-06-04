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
#include <linux/amlogic/scpi_protocol.h>

#include "meson_mhu_pl.h"

#define DRIVER_NAME		"meson_mhu_pl"

/* for list */
static spinlock_t mhu_list_lock;

static struct list_head mbox_devs = LIST_HEAD_INIT(mbox_devs);
static struct class *mbox_class;

static struct list_head mbox_list[CHANNEL_PL_MAX];

static int mbflag;

enum USR_CMD {
	MBOX_USER_CMD = 0x1001,
};

/*for pl dsp*/
struct mbdata_async {
	u64 reserve;
	u64 complete;
	char data[244];
} __packed;

struct mbdata_sync {
	u64 reserve;
	char data[252];
} __packed;

/*
 * mbox_chan_report
 * Report receive data
 */
static void mbox_chan_report(u32 status, void *msg, int idx)
{
	struct mhu_data_buf *data_buf = (struct mhu_data_buf *)msg;
	struct mbox_message *message;
	struct mbox_message *listen_msg = NULL;
	struct mbdata_async *mbdata_async =
		(struct mbdata_async *)(data_buf->rx_buf);
	struct list_head *list;
	unsigned long flags;

	spin_lock_irqsave(&mhu_list_lock, flags);
	if (list_empty(&mbox_list[idx])) {
		spin_unlock_irqrestore(&mhu_list_lock, flags);
		return;
	}

	list_for_each(list, &mbox_list[idx]) {
		message = list_entry(list, struct mbox_message, list);
		pr_debug("complete %lx %x %x\n",
			 (unsigned long)(mbdata_async->complete),
			 status & CMD_MASK, message->cmd);
		if ((unsigned long)mbdata_async->complete ==
					(unsigned long)&message->complete) {
			memcpy(message->data,
			       mbdata_async->data,
			       SIZE_LEN(status));
			complete(&message->complete);
			spin_unlock_irqrestore(&mhu_list_lock, flags);
			return;
		} else if (!listen_msg &&
				(status & CMD_MASK) == message->cmd) {
			listen_msg = message;
		}
	}

	spin_unlock_irqrestore(&mhu_list_lock, flags);
	if (listen_msg) {
		pr_debug("listen cmd\n");
		memcpy(listen_msg->data,
		       mbdata_async->data,
		       SIZE_LEN(status));
		complete(&listen_msg->complete);
		return;
	}
}

static irqreturn_t mbox_dsp_handler(int irq, void *p)
{
	struct mbox_chan *link = (struct mbox_chan *)p;
	struct mhu_chan *mhu_chan = link->con_priv;
	struct mhu_ctlr *ctlr = mhu_chan->ctlr;
	int idx = mhu_chan->index;
	int mhudev = mhu_chan->index / 2;
	void __iomem *mbox_clr_base = ctlr->mbox_clr_base[mhudev];
	void __iomem *mbox_sts_base = ctlr->mbox_sts_base[mhudev];
	void __iomem *payload = ctlr->mbox_pl_base[mhudev];
	struct mhu_data_buf *data;
	u32 status  = readl(mbox_sts_base);

	if (status && irq == mhu_chan->rx_irq) {
		data = mhu_chan->data;
		if (!data)
			return IRQ_NONE;
		if (data->rx_buf) {
			memcpy_fromio(data->rx_buf,
				      payload + RX_PAYLOAD,
				      data->rx_size);
			mbox_chan_report(status, data, idx);
			memset(data->rx_buf, 0, data->rx_size);
		}
		writel(~0, mbox_clr_base);
	}

	return IRQ_HANDLED;
}

void memcpy_tomb(void __iomem *to, const void *from, long count)
{
	while (count > 0) {
		__raw_writeb(*(const u8 *)from, to);
		count--;
		to++;
		from++;
	}
	/*for sram issue*/
	mb();
}

static int mhu_transfer_data(struct mbox_chan *link, void *msg)
{
	struct mhu_chan *mhu_chan = link->con_priv;
	struct mhu_ctlr *ctlr = mhu_chan->ctlr;
	struct mhu_data_buf *data = (struct mhu_data_buf *)msg;
	int mhudev = mhu_chan->index / 2;
	void __iomem *mbox_set_base = ctlr->mbox_set_base[mhudev];
	void __iomem *payload = ctlr->mbox_pl_base[mhudev];

	if (!data)
		return -EINVAL;

	mhu_chan->data = data;
	if (data->tx_buf) {
		if (mbflag == 1) {
			memcpy_tomb(payload + TX_PAYLOAD,
				    data->tx_buf, data->tx_size);
		} else {
			memset_io(payload + TX_PAYLOAD,
				  0, MBOX_PL_SIZE);
			memcpy_toio(payload + TX_PAYLOAD,
				    data->tx_buf, data->tx_size);
		}
	}
	writel(data->cmd, mbox_set_base);

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
	int mhudev = mhu_chan->index / 2;
	void __iomem *payload = ctlr->mbox_pl_base[mhudev];
	struct mhu_data_buf *data;

	if (irq == mhu_chan->rx_irq) {
		data = mhu_chan->data;
		if (!data)
			return IRQ_NONE; /* spurious */
		if (data->rx_buf)
			memcpy_fromio(data->rx_buf,
				      payload + TX_PAYLOAD,
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
	int mhudev = chan->index / 2;
	void __iomem *mbox_sts_base = ctlr->mbox_sts_base[mhudev];

	return !readl(mbox_sts_base);
}

static struct mbox_chan_ops mhu_ops = {
	.send_data = mhu_transfer_data,
	.startup = mhu_startup,
	.shutdown = mhu_shutdown,
	.last_tx_done = mhu_last_tx_done,
};

static int to_send_idx(int idx, bool send)
{
	if (send)
		return (idx % 2) ? idx : idx + 1;
	return (idx % 2) ? idx - 1 : idx;
}

static ssize_t mbox_message_send(struct device *dev, void *data,
				 int count, int idx,
				 struct mbox_message *mbox_msg)
{
	int ret;
	struct mbox_client cl = {0};
	struct mbox_chan *chan, *mbox_chan;
	struct mhu_chan *mhu_chan;
	struct mhu_data_buf data_buf;
	struct mbdata_async mbdata_async;
	unsigned long flags;
	int data_size = 0;

	mbdata_async.complete = (unsigned long)(&mbox_msg->complete);
	dev_dbg(dev, "%s %lx\n", __func__,
		(unsigned long)mbdata_async.complete);
	memcpy(mbdata_async.data, data, count);
	data_buf.tx_buf = (void *)&mbdata_async;
	data_buf.tx_size = count + MBOX_COMPLETE_LEN + MBOX_RESERVE_LEN;
	data_size = count + MBOX_COMPLETE_LEN;
	data_buf.cmd = (mbox_msg->cmd)
			| SIZE_SHIFT(data_size)
			| SYNC_SHIFT(ASYNC_CMD);
	data_buf.rx_buf = NULL;
	cl.dev = dev;
	cl.tx_block = true;
	cl.tx_tout = MBOX_TIME_OUT;
	mbox_chan = mbox_get_channel(&cl, idx);
	if (IS_ERR_OR_NULL(mbox_chan)) {
		dev_err(dev, "Not have this chan\n");
		ret = PTR_ERR(mbox_chan);
		goto err_send3;
	}
	mutex_lock(&mbox_chan->mutex);
	chan = mbox_request_channel(&cl, idx);
	if (IS_ERR_OR_NULL(chan)) {
		mutex_unlock(&mbox_chan->mutex);
		dev_err(dev, "Failed Req Chan\n");
		ret = PTR_ERR(chan);
		goto err_send3;
	}
	mhu_chan = (struct mhu_chan *)chan->con_priv;
	mbox_msg->chan_idx = to_send_idx(mhu_chan->index, false);
	spin_lock_irqsave(&mhu_list_lock, flags);
	list_add_tail(&mbox_msg->list, &mbox_list[mbox_msg->chan_idx]);
	spin_unlock_irqrestore(&mhu_list_lock, flags);
	ret = mbox_send_message(chan, (void *)(&data_buf));
	mbox_free_channel(chan);
	if (ret < 0) {
		dev_err(dev, "Failed transfer message via mailbox %d\n", ret);
		mutex_unlock(&mbox_chan->mutex);
		spin_lock_irqsave(&mhu_list_lock, flags);
		list_del(&mbox_msg->list);
		spin_unlock_irqrestore(&mhu_list_lock, flags);
		goto err_send3;
	}
	mutex_unlock(&mbox_chan->mutex);
	dev_dbg(dev, "Ack OK\n");
	ret = count;
err_send3:
	return ret;
}

ssize_t mbox_message_send_pl(struct device *dev, int cmd,
				 void *data, int count, int idx)
{
	struct mbox_message *mbox_msg;
	struct mbdata_async *mb_data;
	int ret = 0;
	unsigned long flags;

	mbox_msg = kzalloc(sizeof(*mbox_msg), GFP_KERNEL);
	if (!mbox_msg) {
		ret = -ENOMEM;
		goto err_send0;
	}

	mb_data = kzalloc(sizeof(*mb_data), GFP_KERNEL);
	if (!mb_data) {
		ret = -ENOMEM;
		goto err_send1;
	}

	mbox_msg->data = (void *)mb_data;
	mbox_msg->cmd = cmd & CMD_MASK;
	mbox_msg->task = current;
	init_completion(&mbox_msg->complete);
	ret = mbox_message_send(dev, data, count, idx, mbox_msg);
	if (ret >= 0) {
		ret = wait_for_completion_killable(&mbox_msg->complete);
		if (!ret)
			memcpy(data, mbox_msg->data, count);
		spin_lock_irqsave(&mhu_list_lock, flags);
		list_del(&mbox_msg->list);
		spin_unlock_irqrestore(&mhu_list_lock, flags);
	}
	kfree(mbox_msg->data);
err_send1:
	kfree(mbox_msg);
err_send0:
	return ret;
}

static ssize_t mbox_message_write(struct file *filp,
				  const char __user *userbuf,
				  size_t count, loff_t *ppos)
{
	int ret;
	struct mbox_message *mbox_msg;
	struct mbox_client cl = {0};
	struct mbox_chan *chan, *mbox_chan;
	struct mbdata_async mbdata_async;
	struct mhu_data_buf data_buf;
	unsigned long flags;
	int cmd;
	struct mhu_mbox *mbox_dev = filp->private_data;
	struct device *dev = mbox_dev->mhu_dev;
	int channel =  mbox_dev->channel_id;
	int send_channel;

	send_channel = to_send_idx(channel, true);

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

	mbox_msg->data = kzalloc(sizeof(mbdata_async), GFP_KERNEL);
	if (!mbox_msg->data) {
		ret = -ENOMEM;
		goto err_probe1;
	}

	ret = copy_from_user(mbdata_async.data, userbuf
			     + MBOX_USER_CMD_LEN,
			     count - MBOX_USER_CMD_LEN);
	if (ret) {
		ret = -EFAULT;
		goto err_probe2;
	}

	ret = copy_from_user((char *)&cmd, userbuf, MBOX_USER_CMD_LEN);
	if (ret) {
		ret = -EFAULT;
		goto err_probe2;
	}

	mbox_msg->cmd = cmd & CMD_MASK;
	init_completion(&mbox_msg->complete);
	mbox_msg->task = current;
	spin_lock_irqsave(&mhu_list_lock, flags);
	list_add_tail(&mbox_msg->list, &mbox_list[channel]);
	spin_unlock_irqrestore(&mhu_list_lock, flags);

	/*Listen data not send data to hifi*/
	if (cmd & LISTEN_DATA) {
		ret = count;
		goto err_probe0;
	}

	mbdata_async.complete = (unsigned long)(&mbox_msg->complete);
	dev_dbg(dev, "%s %lx\n", __func__,
		(unsigned long)mbdata_async.complete);
	data_buf.tx_buf = (void *)&mbdata_async;
	data_buf.tx_size = count - MBOX_USER_CMD_LEN + MBOX_COMPLETE_LEN;
	data_buf.cmd = (mbox_msg->cmd)
		       | SIZE_SHIFT(data_buf.tx_size)
		       | SYNC_SHIFT(ASYNC_CMD);
	data_buf.rx_buf = NULL;
	cl.dev = dev;
	cl.tx_block = true;
	cl.tx_tout = MBOX_TIME_OUT;
	mbox_chan = mbox_get_channel(&cl, send_channel);
	if (IS_ERR_OR_NULL(mbox_chan)) {
		dev_err(dev, "Not have this chan\n");
		ret = PTR_ERR(mbox_chan);
		goto err_probe3;
	}
	mutex_lock(&mbox_chan->mutex);
	chan = mbox_request_channel(&cl, send_channel);
	if (IS_ERR_OR_NULL(chan)) {
		mutex_unlock(&mbox_chan->mutex);
		dev_err(dev, "Failed Req Chan\n");
		ret = PTR_ERR(chan);
		goto err_probe3;
	}
	ret = mbox_send_message(chan, (void *)(&data_buf));
	mbox_free_channel(chan);
	mutex_unlock(&mbox_chan->mutex);
	if (ret < 0) {
		dev_err(dev, "Failed to send message via mailbox %d\n", ret);
	} else {
		dev_dbg(dev, "Ack OK\n");
		return count;
	}
err_probe3:
	spin_lock_irqsave(&mhu_list_lock, flags);
	list_del(&mbox_msg->list);
	spin_unlock_irqrestore(&mhu_list_lock, flags);
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
	struct mhu_mbox *mbox_dev = filp->private_data;
	struct device *dev = mbox_dev->mhu_dev;
	int channel =  mbox_dev->channel_id;

	spin_lock_irqsave(&mhu_list_lock, flags);
	if (list_empty(&mbox_list[channel])) {
		spin_unlock_irqrestore(&mhu_list_lock, flags);
		return -ENXIO;
	}
	list_for_each(list, &mbox_list[channel]) {
		msg = list_entry(list, struct mbox_message, list);
		if (msg->task == current) {
			spin_unlock_irqrestore(&mhu_list_lock, flags);
			ret = wait_for_completion_killable(&msg->complete);
			if (ret < 0) {
				dev_err(dev, "Read msg wait killed %d\n",
					ret);
				return -ENXIO;
			}
			dev_dbg(dev, "Wait end %s\n", msg->data);
			break;
		}
	}

	if (list == &mbox_list[channel]) {
		dev_err(dev, "List is null or not find data\n");
		spin_unlock_irqrestore(&mhu_list_lock, flags);
		return -ENXIO;
	}
	*ppos = 0;
	ret = simple_read_from_buffer(userbuf, count, ppos,
				      msg->data, MBOX_TX_SIZE);

	spin_lock_irqsave(&mhu_list_lock, flags);
	list_del(list);
	spin_unlock_irqrestore(&mhu_list_lock, flags);
	kfree(msg->data);
	kfree(msg);
	return ret;
}

static int mbox_message_open(struct inode *inode, struct file *filp)
{
	struct cdev *cdev = inode->i_cdev;
	struct mhu_mbox *dev = container_of(cdev, struct mhu_mbox, char_cdev);

	filp->private_data = dev;
	return 0;
}

static const struct file_operations mbox_message_ops = {
	.write	= mbox_message_write,
	.read	= mbox_message_read,
	.open	= mbox_message_open,
};

static void mhu_cleanup_devs(void)
{
	struct mhu_mbox *cur, *n;

	list_for_each_entry_safe(cur, n, &mbox_devs, char_list) {
		if (cur->char_dev) {
			cdev_del(&cur->char_cdev);
			device_del(cur->char_dev);
		}
		list_del(&cur->char_list);
		kfree(cur);
	}
}

static int mhu_cdev_init(struct device *dev, struct mhu_ctlr *mhu_ctlr)
{
	struct mhu_chan *mhu_chan;
	dev_t char_dev;
	int char_major;
	const char *name = NULL;
	int mbdevs, mbox_nums = 0;
	int index, i;
	int err = 0;

	of_property_read_u32(dev->of_node,
			     "mbox-nums", &mbox_nums);
	if (mbox_nums == 0 || mbox_nums > CHANNEL_PL_MAX)
		mbox_nums = CHANNEL_PL_MAX;

	mbdevs = mbox_nums / 2;

	mbox_class = class_create(THIS_MODULE, "mbox_dev");
	if (IS_ERR_OR_NULL(mbox_class))
		goto err;

	err = alloc_chrdev_region(&char_dev, 0, mbdevs, DRIVER_NAME);
	if (err < 0) {
		dev_err(dev, "%s mhu alloc dev_t number failed\n", __func__);
		err = -1;
		goto class_err;
	}

	char_major = MAJOR(char_dev);
	for (i = 0; i < mbdevs; i++) {
		struct mhu_mbox *cur =
			kzalloc(sizeof(struct mhu_mbox), GFP_KERNEL);
		if (!cur) {
			dev_err(dev, "mbox unable to alloc dev\n");
			goto out_err;
		}
		list_add_tail(&cur->char_list, &mbox_devs);

		mhu_chan = &mhu_ctlr->channels[i * 2];
		cur->channel_id = i * 2;
		cur->char_no = MKDEV(char_major, i);

		if (!of_get_property(dev->of_node, "mbox-names", NULL)) {
			dev_err(dev, "%s() get mbox name fail\n", __func__);
			goto out_err;
		}

		index = i * 2;
		if (of_property_read_string_index(dev->of_node,
						  "mbox-names", index, &name)) {
			dev_err(dev, "%s get mbox[%d] name fail\n",
				__func__, index);
			goto out_err;
		}

		strncpy(cur->char_name, name, CDEV_NAME_SIZE - 1);
		cur->char_name[CDEV_NAME_SIZE - 1] = '\0';
		pr_debug("dts char name[%d]: %s\n", index, cur->char_name);

		cur->mhu_dev = dev;

		INIT_LIST_HEAD(&mbox_list[i * 2]);

		cdev_init(&cur->char_cdev, &mbox_message_ops);
		err = cdev_add(&cur->char_cdev, cur->char_no, 1);
		if (err) {
			dev_err(dev, "mbox fail to add cdev\n");
			goto out_err;
		}

		cur->char_dev =
			device_create(mbox_class, NULL, cur->char_no,
				      cur, "%s", cur->char_name);
		if (IS_ERR_OR_NULL(cur->char_dev)) {
			dev_err(dev, "mbox fail to create device\n");
			goto out_err;
		}
	}
	return 0;
out_err:
	mhu_cleanup_devs();
	unregister_chrdev_region(char_dev, mbdevs);
class_err:
	class_destroy(mbox_class);
err:
	return err;
}

static int mhu_pl_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mhu_ctlr *mhu_ctlr;
	struct mhu_chan *mhu_chan;
	struct mbox_chan *mbox_chans;
	struct resource *res;
	int idx, num_chans, memid, i;
	int err = 0;

	pr_info("pl mailbox init start\n");
	mhu_ctlr = devm_kzalloc(dev, sizeof(*mhu_ctlr), GFP_KERNEL);
	if (!mhu_ctlr)
		return -ENOMEM;

	num_chans = 0;
	of_property_read_u32(dev->of_node,
			     "mbox-nums", &num_chans);
	if (!num_chans) {
		dev_err(dev, "failed to get mailbox num\n");
		return -ENXIO;
	}

	mbflag = 0;
	of_property_read_u32(dev->of_node,
			     "mbox-mb", &mbflag);
	if (!mbflag)
		dev_err(dev, "no mailbox mbox-mb\n");

	memid = num_chans / 2;

	for (i = 0; i < memid; i++) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, (i * 4 + 0));
		if (!res) {
			dev_err(dev, "failed to get mailbox sts reg\n");
			return -ENXIO;
		}
		mhu_ctlr->mbox_sts_base[i] = devm_ioremap_resource(dev, res);
		if (IS_ERR_OR_NULL(mhu_ctlr->mbox_sts_base[i]))
			return PTR_ERR(mhu_ctlr->mbox_sts_base[i]);

		res = platform_get_resource(pdev, IORESOURCE_MEM, (i * 4 + 1));
		if (!res) {
			dev_err(dev, "failed to get mailbox set reg\n");
			return -ENXIO;
		}
		mhu_ctlr->mbox_set_base[i] = devm_ioremap_resource(dev, res);
		if (IS_ERR_OR_NULL(mhu_ctlr->mbox_set_base[i]))
			return PTR_ERR(mhu_ctlr->mbox_set_base[i]);

		res = platform_get_resource(pdev, IORESOURCE_MEM, (i * 4 + 2));
		if (!res) {
			dev_err(dev, "failed to get mailbox clr reg\n");
			return -ENXIO;
		}
		mhu_ctlr->mbox_clr_base[i] = devm_ioremap_resource(dev, res);
		if (IS_ERR_OR_NULL(mhu_ctlr->mbox_clr_base[i]))
			return PTR_ERR(mhu_ctlr->mbox_clr_base[i]);

		res = platform_get_resource(pdev, IORESOURCE_MEM, (i * 4 + 3));
		if (!res) {
			dev_err(dev, "failed to get mailbox payload\n");
			return -ENXIO;
		}
		mhu_ctlr->mbox_pl_base[i] = devm_ioremap_resource(dev, res);
		if (IS_ERR_OR_NULL(mhu_ctlr->mbox_pl_base[i]))
			return PTR_ERR(mhu_ctlr->mbox_pl_base[i]);
	}

	mutex_init(&mhu_ctlr->mutex);
	mhu_ctlr->dev = dev;
	platform_set_drvdata(pdev, mhu_ctlr);

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
		mhu_chan->ctlr = mhu_ctlr;
		mhu_chan->rx_irq = platform_get_irq(pdev, idx);

		if (mhu_chan->rx_irq < 0) {
			dev_err(dev, "failed to get interrupt %d\n", idx);
			return -ENXIO;
		}
		mutex_init(&mbox_chans[idx].mutex);
		mbox_chans[idx].con_priv = mhu_chan;
	}

	if (mbox_controller_register(&mhu_ctlr->mbox_con)) {
		dev_err(dev, "failed to register mailbox controller\n");
		return -ENOMEM;
	}

	for (idx = 0; idx < num_chans; idx++) {
		if (BIT(idx) & 0xA)
			continue;
		mhu_chan =  &mhu_ctlr->channels[idx];
		mhu_chan->index = idx;
		mhu_chan->data = devm_kzalloc(dev,
					      sizeof(struct mhu_data_buf),
					      GFP_KERNEL);
		if (!mhu_chan->data)
			return -ENOMEM;

		mhu_chan->data->rx_buf = devm_kzalloc(dev,
						      MBOX_PL_SIZE,
						      GFP_KERNEL);
		if (!mhu_chan->data->rx_buf)
			return -ENOMEM;

		mhu_chan->data->rx_size = MBOX_PL_SIZE;
		err = request_threaded_irq(mhu_chan->rx_irq, mbox_dsp_handler,
					   NULL, IRQF_ONESHOT,
					   DRIVER_NAME, &mbox_chans[idx]);
		if (err) {
			dev_err(dev, "request irq error\n");
			return err;
		}
	}

	err = mhu_cdev_init(dev, mhu_ctlr);
	if (err < 0) {
		pr_info("init cdev fail\n");
		return err;
	}

	mhu_pl_device = dev;
	mhu_f |= MASK_MHU_PL;
	pr_info("pl mailbox init done %pK, 0x%x\n", mhu_pl_device, mhu_f);
	return 0;
}

static int mhu_pl_remove(struct platform_device *pdev)
{
	struct mhu_ctlr *ctlr = platform_get_drvdata(pdev);

	mbox_controller_unregister(&ctlr->mbox_con);

	platform_set_drvdata(pdev, NULL);
	return 0;
}

static const struct of_device_id mhu_of_match[] = {
	{ .compatible = "amlogic, meson_mhu_pl" },
	{},
};

static struct platform_driver mhu_pl_driver = {
	.probe = mhu_pl_probe,
	.remove = mhu_pl_remove,
	.driver = {
		.owner		= THIS_MODULE,
		.name = DRIVER_NAME,
		.of_match_table = mhu_of_match,
	},
};

int __init aml_mhu_pl_init(void)
{
	return platform_driver_register(&mhu_pl_driver);
}

void __exit aml_mhu_pl_exit(void)
{
	platform_driver_unregister(&mhu_pl_driver);
}
