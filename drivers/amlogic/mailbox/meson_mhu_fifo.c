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

#include "meson_mhu_fifo.h"

#define DRIVER_NAME		"meson_mhu_fifo"
#define IRQ_MAX_DEFAULT		64

struct mbox_data {
	u32 status;
	u64 task;
	u64 complete;
	u64 ullclt;
	char data[MBOX_DATA_SIZE];
} __packed;
int mbox_nums;

static struct list_head mbox_devs = LIST_HEAD_INIT(mbox_devs);
static struct class *mbox_class;

static struct list_head mbox_list[CHANNEL_FIFO_MAX];

static void mbox_fifo_write(void __iomem *to, void *from, long count)
{
	int i = 0;
	long len = count / 4;
	long rlen = count % 4;
	u32 rdata = 0;
	u32 *p = (u32 *)from;

	while (len > 0) {
		writel(*(const u32 *)p, (to + 4 * i));
		len--;
		i++;
		p++;
	}

	/*remainder data need use copy for no over mem size*/
	if (rlen != 0) {
		memcpy(&rdata, p, rlen);
		writel(rdata, (to + 4 * i));
	}
}

static void mbox_fifo_read(void *to, void __iomem *from, long count)
{
	int i = 0;
	long len = count / 4;
	long rlen = count % 4;
	u32 rdata = 0;
	u32 *p = to;

	while (len > 0) {
		*p = readl(from + (4 * i));
		len--;
		i++;
		p++;
	}

	/*remainder data need use copy for no over mem size*/
	if (rlen != 0) {
		rdata = readl(from + (4 * i));
		memcpy(p, &rdata, rlen);
	}
}

static void mbox_fifo_clr(void __iomem *to, long count)
{
	int i = 0;
	int len = (count + 3) / 4;

	while (len > 0) {
		writel(0, (to + 4 * i));
		len--;
		i++;
	}
}

static int to_send_idx(int idx, bool send)
{
	if (send)
		return (idx % 2) ? idx : idx + 1;
	return (idx % 2) ? idx - 1 : idx;
}

static void mbox_chan_report(u32 status, void *msg, int idx)
{
	struct mhu_data_buf *data_buf = (struct mhu_data_buf *)msg;
	struct mbox_message *message;
	struct mbox_message *listen_msg = NULL;
	struct list_head *list;
	unsigned long flags;
	struct mbox_data *mbox_data =
		(struct mbox_data *)(data_buf->rx_buf);
	struct mhu_mbox *mbox_dev, *n;

	list_for_each_entry_safe(mbox_dev, n, &mbox_devs, char_list) {
		if (mbox_dev->channel_id == idx)
			break;
	}

	spin_lock_irqsave(&mbox_dev->mhu_lock, flags);
	if (list_empty(&mbox_list[idx])) {
		spin_unlock_irqrestore(&mbox_dev->mhu_lock, flags);
		return;
	}

	list_for_each(list, &mbox_list[idx]) {
		message = list_entry(list, struct mbox_message, list);
		if ((unsigned long)mbox_data->complete ==
					(unsigned long)&message->complete) {
			memcpy(message->data, mbox_data->data,
			       SIZE_LEN(status));
			complete(&message->complete);
			spin_unlock_irqrestore(&mbox_dev->mhu_lock, flags);
			return;
		} else if (!listen_msg && (status & CMD_MASK) == message->cmd) {
			listen_msg = message;
		}
	}

	spin_unlock_irqrestore(&mbox_dev->mhu_lock, flags);
	if (listen_msg) {
		memcpy(listen_msg->data, mbox_data->data, SIZE_LEN(status));
		complete(&listen_msg->complete);
		return;
	}
}

void mbox_irq_clean(u64 mask, struct mhu_ctlr *ctlr)
{
	void __iomem *mbox_irq_base = ctlr->mbox_irq_base;
	int irqclr = ctlr->mhu_irqclr;
	int irqmax = ctlr->mhu_irqmax;
	u64 hstatus, lstatus;

	if (irqmax / MHUIRQ_MAXNUM_DEF == 2) {
		hstatus = (mask >> MBOX_IRQSHIFT) & MBOX_IRQMASK;
		lstatus = mask & MBOX_IRQMASK;
		writel(lstatus, mbox_irq_base + IRQ_CLR_OFFSETL(irqclr));
		writel(hstatus, mbox_irq_base + IRQ_CLR_OFFSETH(irqclr));
	} else {
		writel((mask & MBOX_IRQMASK), mbox_irq_base + IRQ_CLR_OFFSET(irqclr));
	}
}

static void mbox_isr_handler(int mhu_id, void *p)
{
	struct mhu_ctlr *ctlr = (struct mhu_ctlr *)p;
	void __iomem *mbox_rd_base = ctlr->mbox_rd_base;
	void __iomem *mbox_fclr_base = ctlr->mbox_fclr_base;
	void __iomem *mbox_fsts_base = ctlr->mbox_fsts_base;
	struct mhu_chan *chan = NULL;
	struct mhu_data_buf *data = NULL;
	int channel = 0;
	u32 status;

	for (channel = 0; channel < mbox_nums; channel++) {
		if (ctlr->mhu_id[channel] == mhu_id)
			break;
	}

	if (channel >= mbox_nums)
		return;

	status = readl(mbox_fsts_base + CTL_OFFSET(mhu_id));
	if (status) {
		chan = &ctlr->channels[channel];
		data = chan->data;
		if (!data)
			return;
		if (data->rx_buf) {
			mbox_fifo_read(data->rx_buf,
				       mbox_rd_base + PAYLOAD_OFFSET(mhu_id),
				       data->rx_size);
			mbox_chan_report(status, data, channel);
			memset(data->rx_buf, 0, data->rx_size);
		}
	}
	mbox_irq_clean(IRQ_REV_BIT(mhu_id), ctlr);
	writel(~0, mbox_fclr_base + CTL_OFFSET(mhu_id));
}

void mbox_ack_isr_handler(int mhu_id, void *p)
{
	struct mhu_ctlr *ctlr = p;
	void __iomem *mbox_rd_base = ctlr->mbox_rd_base;
	void __iomem *mbox_wr_base = ctlr->mbox_wr_base;
	struct mbox_controller *mbox_con = &ctlr->mbox_con;
	struct mhu_chan *chan = NULL;
	struct mbox_chan *mbox_chan = NULL;
	struct mhu_data_buf *data = NULL;
	int channel;

	for (channel = 0; channel < mbox_nums; channel++) {
		if (ctlr->mhu_id[channel] == mhu_id)
			break;
	}

	if (channel >= mbox_nums)
		return;

	chan = &ctlr->channels[channel];
	mbox_chan = &mbox_con->chans[channel];
	data = chan->data;

	if (data) {
		if (data->rx_buf) {
			mbox_fifo_read(data->rx_buf,
				       mbox_rd_base + PAYLOAD_OFFSET(mhu_id),
				       data->rx_size);
		}
		chan->data = NULL;
		mbox_chan_received_data(mbox_chan, data);
	}
	mbox_fifo_clr(mbox_wr_base + PAYLOAD_OFFSET(mhu_id),
		      MBOX_FIFO_SIZE);
	mbox_irq_clean(IRQ_SENDACK_BIT(mhu_id), ctlr);
	complete(&mbox_chan->tx_complete);
}

static u64 mbox_irqstatus(struct mhu_ctlr *ctlr)
{
	void __iomem *mbox_irq_base = ctlr->mbox_irq_base;
	int irqctlr = ctlr->mhu_irqctlr;
	int irqmax = ctlr->mhu_irqmax;
	u64 status, hstatus, lstatus;

	if (irqmax / MHUIRQ_MAXNUM_DEF == 2) {
		lstatus = readl(mbox_irq_base + IRQ_STS_OFFSETL(irqctlr));
		hstatus = readl(mbox_irq_base + IRQ_STS_OFFSETH(irqctlr));
		status = (hstatus << MBOX_IRQSHIFT) | (lstatus & MBOX_IRQMASK);
	} else {
		status = readl(mbox_irq_base + IRQ_STS_OFFSET(irqctlr));
	}
	pr_debug("irq status = %llu\n", status);
	return status;
}

static irqreturn_t mbox_handler(int irq, void *p)
{
	struct mhu_ctlr *ctlr = (struct mhu_ctlr *)p;
	int irqmax = ctlr->mhu_irqmax;
	u64 status, prestatus = 0;
	u64 outcnt;
	u64 i, bit = 1;

	outcnt = irqmax;
	status = mbox_irqstatus(ctlr);
	while (status && (outcnt != 0)) {
		for (i = 0; i < irqmax; i++) {
			if (status & (bit << i)) {
				if (i % 2)
					mbox_ack_isr_handler(i / 2, p);
				else
					mbox_isr_handler(i / 2, p);
			}
		}
		prestatus = status;
		status = mbox_irqstatus(ctlr);
		status = (status | prestatus) ^ prestatus;
		outcnt--;
		WARN_ON(!outcnt);
	}
	pr_debug("in mbox handler\n");
	return IRQ_HANDLED;
}

static int mhu_send_data(struct mbox_chan *link, void *msg)
{
	struct mhu_chan *chan = link->con_priv;
	struct mhu_ctlr *ctlr = chan->ctlr;
	void __iomem *mbox_wr_base = ctlr->mbox_wr_base;
	void __iomem *mbox_fset_base = ctlr->mbox_fset_base;
	struct mhu_data_buf *data = (struct mhu_data_buf *)msg;
	int mhu_id = chan->mhu_id;

	if (!data)
		return -EINVAL;

	if (data->tx_size > MBOX_DATA_SIZE + MBOX_HEAD_SIZE) {
		pr_err("data size overflow %d\n", data->cmd);
		data->tx_size = MBOX_DATA_SIZE + MBOX_HEAD_SIZE;
		return -ENOMEM;
	}

	chan->data = data;
	if (data->tx_buf) {
		mbox_fifo_write(mbox_wr_base + PAYLOAD_OFFSET(mhu_id),
				data->tx_buf, data->tx_size);
	}
	writel(data->cmd, mbox_fset_base + CTL_OFFSET(mhu_id));

	return 0;
}

static int mhu_startup(struct mbox_chan *link)
{
	return 0;
}

static void mhu_shutdown(struct mbox_chan *link)
{
}

static bool mhu_last_tx_done(struct mbox_chan *link)
{
	struct mhu_chan *chan = link->con_priv;
	struct mhu_ctlr *ctlr = chan->ctlr;
	void __iomem *mbox_fsts_base = ctlr->mbox_fsts_base;
	int mhu_id = chan->mhu_id;

	return !readl(mbox_fsts_base + CTL_OFFSET(mhu_id));
}

static struct mbox_chan_ops mhu_ops = {
	.send_data = mhu_send_data,
	.startup = mhu_startup,
	.shutdown = mhu_shutdown,
	.last_tx_done = mhu_last_tx_done,
};

static ssize_t mbox_message_send(struct device *dev,
				 void *data, int count,
				 int idx, struct mbox_message *mbox_msg)
{
	int ret;
	struct mbox_client cl = {0};
	struct mbox_chan *chan, *mbox_chan;
	struct mhu_chan *mhu_chan;
	struct mhu_data_buf data_buf;
	struct mbox_data mbox_data;
	unsigned long flags;
	struct mhu_mbox *mbox_dev, *n;

	mbox_data.task = 0;
	mbox_data.status = 0;
	mbox_data.ullclt = 0;
	mbox_data.complete = (unsigned long)(&mbox_msg->complete);
	dev_dbg(dev, "%s %lx\n", __func__, (unsigned long)mbox_data.complete);
	memcpy(mbox_data.data, data, count);
	data_buf.tx_buf = (void *)&mbox_data;
	data_buf.tx_size = count + MBOX_HEAD_SIZE;
	data_buf.cmd = (mbox_msg->cmd)
			| SIZE_SHIFT(data_buf.tx_size)
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
		ret = PTR_ERR(mbox_chan);
		goto err_send3;
	}
	mhu_chan = (struct mhu_chan *)chan->con_priv;
	mbox_msg->chan_idx = to_send_idx(mhu_chan->index, false);
	list_for_each_entry_safe(mbox_dev, n, &mbox_devs, char_list) {
		if (mbox_dev->channel_id == mbox_msg->chan_idx)
			break;
	}

	spin_lock_irqsave(&mbox_dev->mhu_lock, flags);
	list_add_tail(&mbox_msg->list, &mbox_list[mbox_msg->chan_idx]);
	spin_unlock_irqrestore(&mbox_dev->mhu_lock, flags);
	ret = mbox_send_message(chan, (void *)(&data_buf));
	mbox_free_channel(chan);
	if (ret < 0) {
		dev_err(dev, "Failed transfer message via mailbox %d\n", ret);
		mutex_unlock(&mbox_chan->mutex);
		spin_lock_irqsave(&mbox_dev->mhu_lock, flags);
		list_del(&mbox_msg->list);
		spin_unlock_irqrestore(&mbox_dev->mhu_lock, flags);
		goto err_send3;
	}
	mutex_unlock(&mbox_chan->mutex);
	dev_dbg(dev, "Ack OK\n");
	ret = count;
err_send3:
	return ret;
}

ssize_t mbox_message_send_fifo(struct device *dev, int cmd,
			       void *data, int count, int idx)
{
	struct mbox_message *mbox_msg;
	struct mbox_data *mb_data;
	int ret = 0;
	unsigned long flags;
	struct mhu_mbox *mbox_dev, *n;

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
		list_for_each_entry_safe(mbox_dev, n, &mbox_devs, char_list) {
			if (mbox_dev->channel_id == mbox_msg->chan_idx)
				break;
		}

		spin_lock_irqsave(&mbox_dev->mhu_lock, flags);
		list_del(&mbox_msg->list);
		spin_unlock_irqrestore(&mbox_dev->mhu_lock, flags);
	}
	kfree(mb_data);
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
	struct mbox_data mbox_data;
	struct mhu_data_buf data_buf;
	unsigned long flags;
	int cmd;
	struct mhu_mbox *mbox_dev = filp->private_data;
	struct device *dev = mbox_dev->mhu_dev;
	int channel =  mbox_dev->channel_id;
	int send_channel;

	send_channel = to_send_idx(channel, true);

	if (count > MBOX_USER_MAX_LEN || count < MBOX_USER_CMD_LEN) {
		dev_err(dev,
			"Message length %zd over range\n",
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

	ret = copy_from_user(mbox_data.data, userbuf + MBOX_USER_CMD_LEN,
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
	spin_lock_irqsave(&mbox_dev->mhu_lock, flags);
	list_add_tail(&mbox_msg->list, &mbox_list[channel]);
	spin_unlock_irqrestore(&mbox_dev->mhu_lock, flags);

	/*Listen data not send data to hifi*/
	if (cmd & LISTEN_DATA) {
		ret = count;
		goto err_probe0;
	}

	mbox_data.task = 0;
	mbox_data.status = 0;
	mbox_data.ullclt = 0;
	mbox_data.complete = (unsigned long)(&mbox_msg->complete);
	dev_dbg(dev, "%s %lx\n", __func__, (unsigned long)mbox_data.complete);
	data_buf.tx_buf = (void *)&mbox_data;
	data_buf.tx_size = count -  MBOX_USER_CMD_LEN + MBOX_HEAD_SIZE;
	pr_debug("txsize %d, %d, %d\n",
		 data_buf.tx_size, MBOX_USER_CMD_LEN, MBOX_HEAD_SIZE);
	data_buf.cmd = CMD_SHIFT(mbox_msg->cmd)
		       | SIZE_SHIFT(data_buf.tx_size)
		       | SYNC_SHIFT(ASYNC_CMD);
	data_buf.rx_buf = NULL;
	data_buf.rx_size = 0;
	cl.dev = dev;
	cl.tx_block = true;
	cl.tx_tout = MBOX_TIME_OUT;
	cl.rx_callback = NULL;
	mbox_chan = mbox_get_channel(&cl, send_channel);
	if (IS_ERR_OR_NULL(mbox_chan)) {
		dev_err(dev, "Not have this chan\n");
		ret = PTR_ERR(mbox_chan);
		goto err_probe3;
	}
	mutex_lock(&mbox_chan->mutex);
	chan = mbox_request_channel(&cl, send_channel);
	if (IS_ERR(chan)) {
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
	spin_lock_irqsave(&mbox_dev->mhu_lock, flags);
	list_del(&mbox_msg->list);
	spin_unlock_irqrestore(&mbox_dev->mhu_lock, flags);
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

	spin_lock_irqsave(&mbox_dev->mhu_lock, flags);
	if (list_empty(&mbox_list[channel])) {
		spin_unlock_irqrestore(&mbox_dev->mhu_lock, flags);
		return -ENXIO;
	}

	list_for_each(list, &mbox_list[channel]) {
		msg = list_entry(list, struct mbox_message, list);
		if (msg->task == current) {
			spin_unlock_irqrestore(&mbox_dev->mhu_lock, flags);
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
		dev_err(dev, "List is not find data\n");
		spin_unlock_irqrestore(&mbox_dev->mhu_lock, flags);
		return -ENXIO;
	}
	*ppos = 0;
	ret = simple_read_from_buffer(userbuf, count, ppos,
				      msg->data, MBOX_DATA_SIZE);

	spin_lock_irqsave(&mbox_dev->mhu_lock, flags);
	list_del(&msg->list);
	spin_unlock_irqrestore(&mbox_dev->mhu_lock, flags);
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
	.write		= mbox_message_write,
	.read		= mbox_message_read,
	.open		= mbox_message_open,
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
	int mbdevs;
	int index, i;
	int err = 0;

	mbox_nums = 0;
	of_property_read_u32(dev->of_node,
			     "mbox-nums", &mbox_nums);
	if (mbox_nums == 0 || mbox_nums > CHANNEL_FIFO_MAX)
		mbox_nums = CHANNEL_FIFO_MAX;

	mbdevs = mbox_nums / 2;

	mbox_class = class_create(THIS_MODULE, "mbox_dev");
	if (IS_ERR(mbox_class))
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

		cur->mhu_id = mhu_chan->mhu_id;
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
		if (IS_ERR(cur->char_dev)) {
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

static int mhu_fifo_probe(struct platform_device *pdev)
{
	struct mhu_ctlr *mhu_ctlr;
	struct mhu_chan *mhu_chan;
	struct mbox_chan *mbox_chans;
	struct device *dev = &pdev->dev;
	struct resource *res;
	int idx, err;
	u32 num_chans = 0;
	u32 irqctlr = 0;
	u32 irqclr = 0;
	u32 irqmax = 0;
	int wrrd = 0;
	int memid = 0;

	pr_debug("mhu fifo probe\n");
	mhu_ctlr = devm_kzalloc(dev, sizeof(*mhu_ctlr), GFP_KERNEL);
	if (!mhu_ctlr)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, memid++);
	if (!res) {
		dev_err(dev, "failed to get mailbox memory resource\n");
		return -ENXIO;
	}
	mhu_ctlr->mbox_wr_base = devm_ioremap_resource(dev, res);
	if (IS_ERR_OR_NULL(mhu_ctlr->mbox_wr_base))
		return PTR_ERR(mhu_ctlr->mbox_wr_base);

	err = of_property_read_u32(dev->of_node,
				   "mbox-wr-rd", &wrrd);
	if (err)
		dev_err(dev, "no get mbox wrrd %d\n", err);

	pr_debug("mbox-wr-rd %d\n", wrrd);

	if (wrrd == 0) {
		pr_debug("mbox-wr-rd 0\n");
		res = platform_get_resource(pdev, IORESOURCE_MEM, memid++);
		if (!res) {
			dev_err(dev, "failed to get mailbox memory resource\n");
			return -ENXIO;
		}
		mhu_ctlr->mbox_rd_base = devm_ioremap_resource(dev, res);
		if (IS_ERR_OR_NULL(mhu_ctlr->mbox_rd_base))
			return PTR_ERR(mhu_ctlr->mbox_rd_base);
	} else {
		/*wr rd use the same fifo buf*/
		pr_debug("mbox-wr-rd 11\n");
		mhu_ctlr->mbox_rd_base = mhu_ctlr->mbox_wr_base;
	}
	res = platform_get_resource(pdev, IORESOURCE_MEM, memid++);
	if (!res) {
		dev_err(dev, "failed to get mailbox memory resource\n");
		return -ENXIO;
	}
	mhu_ctlr->mbox_fset_base = devm_ioremap_resource(dev, res);
	if (IS_ERR(mhu_ctlr->mbox_fset_base))
		return PTR_ERR(mhu_ctlr->mbox_fset_base);

	res = platform_get_resource(pdev, IORESOURCE_MEM, memid++);
	if (!res) {
		dev_err(dev, "failed to get mailbox memory resource\n");
		return -ENXIO;
	}
	mhu_ctlr->mbox_fclr_base = devm_ioremap_resource(dev, res);
	if (IS_ERR(mhu_ctlr->mbox_fclr_base))
		return PTR_ERR(mhu_ctlr->mbox_fclr_base);

	res = platform_get_resource(pdev, IORESOURCE_MEM, memid++);
	if (!res) {
		dev_err(dev, "failed to get mailbox memory resource\n");
		return -ENXIO;
	}
	mhu_ctlr->mbox_fsts_base = devm_ioremap_resource(dev, res);
	if (IS_ERR(mhu_ctlr->mbox_fsts_base))
		return PTR_ERR(mhu_ctlr->mbox_fsts_base);

	res = platform_get_resource(pdev, IORESOURCE_MEM, memid++);
	if (!res) {
		dev_err(dev, "failed to get mailbox memory resource\n");
		return -ENXIO;
	}

	mhu_ctlr->mbox_irq_base = devm_ioremap_resource(dev, res);
	if (IS_ERR(mhu_ctlr->mbox_irq_base))
		return PTR_ERR(mhu_ctlr->mbox_irq_base);

	res = platform_get_resource(pdev, IORESOURCE_MEM, memid++);
	if (!res) {
		dev_err(dev, "no payload, only use fifo\n");
	} else {
		mhu_ctlr->mbox_payload_base = devm_ioremap_resource(dev, res);
		if (IS_ERR(mhu_ctlr->mbox_payload_base))
			return PTR_ERR(mhu_ctlr->mbox_payload_base);
		dev_err(dev, "payload or fifo canbe use\n");
	}

	mhu_ctlr->mhu_irq = platform_get_irq(pdev, 0);
	if (mhu_ctlr->mhu_irq < 0) {
		dev_err(dev, "failed to get interrupt %d\n", 0);
		return -ENXIO;
	}

	err = of_property_read_u32(dev->of_node,
				   "mbox-irqctlr", &irqctlr);
	if (err) {
		dev_err(dev, "failed to get mbox irq ctlr %d\n", err);
		return -ENXIO;
	}
	mhu_ctlr->mhu_irqctlr = irqctlr;

	err = of_property_read_u32(dev->of_node,
				   "mbox-irqclr", &irqclr);
	if (err)
		irqclr = irqctlr;
	mhu_ctlr->mhu_irqclr = irqclr;

	err = of_property_read_u32(dev->of_node,
				   "mbox-irqmax", &irqmax);
	if (err) {
		dev_err(dev, "set mbox irqmax default value %d\n", err);
		irqmax = MHUIRQ_MAXNUM_DEF;
	}
	if (irqmax > IRQ_MAX_DEFAULT)
		irqmax = IRQ_MAX_DEFAULT;
	mhu_ctlr->mhu_irqmax = irqmax;

	mutex_init(&mhu_ctlr->mutex);
	mhu_ctlr->dev = dev;
	platform_set_drvdata(pdev, mhu_ctlr);

	num_chans = 0;
	err = of_property_read_u32(dev->of_node,
				   "mbox-nums", &num_chans);
	if (err) {
		dev_err(dev, "failed to get mbox num %d\n", err);
		return -ENXIO;
	}
	if (of_property_read_u32_array(pdev->dev.of_node, "mbox-id",
				       &mhu_ctlr->mhu_id[0], num_chans)) {
		dev_warn(&pdev->dev, "Missing mbox-id using default %d\n",
			 0x0);
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
					  sizeof(*mhu_chan) * num_chans,
					  GFP_KERNEL);

	for (idx = 0; idx < num_chans; idx++) {
		mhu_chan = &mhu_ctlr->channels[idx];
		mhu_chan->index = idx;
		mhu_chan->mhu_id = mhu_ctlr->mhu_id[idx];
		mhu_chan->ctlr = mhu_ctlr;
		mbox_chans[idx].con_priv = mhu_chan;
		mutex_init(&mbox_chans[idx].mutex);
		pr_debug("%s, chan index: %d, idx: %d, mbu_id:%d\n",
			 __func__, mhu_chan->index, idx, mhu_chan->mhu_id);
		if (BIT(idx) & REV_MBOX_MASK)
			continue;
		mhu_chan->data = devm_kzalloc(dev,
					      sizeof(struct mhu_data_buf),
					      GFP_KERNEL);
		if (!mhu_chan->data)
			return -ENOMEM;

		mhu_chan->data->rx_buf = devm_kzalloc(dev,
						      MBOX_FIFO_SIZE,
						      GFP_KERNEL);
		if (!mhu_chan->data->rx_buf)
			return -ENOMEM;

		mhu_chan->data->rx_size = MBOX_FIFO_SIZE;
	}

	if (mbox_controller_register(&mhu_ctlr->mbox_con)) {
		dev_err(dev, "failed to register mailbox controller\n");
		return -ENOMEM;
	}

	err = request_threaded_irq(mhu_ctlr->mhu_irq, mbox_handler,
				   NULL, IRQF_ONESHOT | IRQF_NO_SUSPEND,
				   DRIVER_NAME, mhu_ctlr);
	if (err) {
		dev_err(dev, "request irq error\n");
		return err;
	}

	err = mhu_cdev_init(dev, mhu_ctlr);
	if (err < 0) {
		dev_err(dev, "init cdev fail\n");
		return err;
	}

	mhu_fifo_device = dev;
	/*set mhu type*/
	mhu_f |= MASK_MHU_FIFO;
	pr_debug("mbox fifo probe done node:%pK, mhuf:0x%x\n",
		mhu_fifo_device, mhu_f);
	return 0;
}

static int mhu_fifo_remove(struct platform_device *pdev)
{
	struct mhu_ctlr *ctlr = platform_get_drvdata(pdev);

	mbox_controller_unregister(&ctlr->mbox_con);

	platform_set_drvdata(pdev, NULL);

	return 0;
}

static const struct of_device_id mhu_of_match[] = {
	{ .compatible = "amlogic, meson_mhu_fifo" },
	{},
};

static struct platform_driver mhu_fifo_driver = {
	.probe = mhu_fifo_probe,
	.remove = mhu_fifo_remove,
	.driver = {
		.owner		= THIS_MODULE,
		.name = DRIVER_NAME,
		.of_match_table = mhu_of_match,
	},
};

int __init aml_mhu_fifo_init(void)
{
	return platform_driver_register(&mhu_fifo_driver);
}

void __exit aml_mhu_fifo_exit(void)
{
	platform_driver_unregister(&mhu_fifo_driver);
}
