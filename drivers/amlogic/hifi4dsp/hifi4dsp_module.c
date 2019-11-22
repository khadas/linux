/*
 * drivers/amlogic/hifi4dsp/hifi4dsp_module.c
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
#define DEBUG

#define pr_fmt(fmt) "hifi4dsp: " fmt

#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/io.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/device.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/reset.h>
#include <linux/dma-mapping.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/of_reserved_mem.h>
#include <linux/cma.h>
#include <linux/dma-contiguous.h>
#include <linux/vmalloc.h>
#include <linux/clk.h>
#include <asm/cacheflush.h>

#include <linux/amlogic/major.h>
#include <linux/amlogic/media/utils/amstream.h>
#include <linux/amlogic/media/sound/aiu_regs.h>
#include <linux/amlogic/media/frame_sync/ptsserv.h>
#include <linux/amlogic/media/frame_sync/timestamp.h>
#include <linux/amlogic/media/frame_sync/tsync.h>

#include "hifi4dsp_api.h"
#include "hifi4dsp_priv.h"
#include "hifi4dsp_firmware.h"
#include "hifi4dsp_dsp.h"
#include "hifi4dsp_ipc.h"

#include "tm2_dsp_top.h"

#define		HIFI4DSP_NUM		2
#define		HIFI4DSP_MAX_CNT	2
#define		DSP_PHY_ADDR_0		0x30000000
#define		DSP_PHY_ADDR_1		0x30800000

struct hifi4dsp_priv *hifi4dsp_p[HIFI4DSP_MAX_CNT];
unsigned int hifi4dsp_debug_flag = 1;

#define DEF_RESERVE_MEM_SIZE	0x800000

#define HIFI4DSP_TM2_ALL_SMEM_SIZE		(0x00300000) /* 3 MBytes */
#define HIFI4DSP_TM2_BASE_PADDR			DSP_PHY_ADDR_0
#define HIFI4DSP_TM2_IRAM_OFFSET		(0x00000000)
#define HIFI4DSP_TM2_DRAM_OFFSET		(0x00100000 + \
				HIFI4DSP_TM2_IRAM_OFFSET)
#define HIFI4DSP_TM2_SMEM_OFFSET		(0x00040000 + \
				HIFI4DSP_TM2_DRAM_OFFSET)
#define HIFI4DSP_TM2_MAILBOX_OFFSET		(0x00004000 + \
				HIFI4DSP_TM2_SMEM_OFFSET)
#define HIFI4DSP_TM2_TIMESTAMP_OFFSET	(0x00000800 +\
				HIFI4DSP_TM2_MAILBOX_OFFSET)
#define HIFI4DSP_TM2_IPC_MAX_SIZE		256

#define IPC_MSG_HEADER		0x10

#define MASK_BF(x, mask, shift)			(((x&mask)<<shift))
	/* msg header bf */
#define IPC_MSG_HEADER_ID_SHIFT			0
#define IPC_MSG_HEADER_ID_MASK			(0xFF)
#define IPC_MSG_HEADER_ID(x)			MASK_BF(x, 0xFF, 0)

#define IPC_MSG_HEADER_SENDERID_SHIFT		8
#define IPC_MSG_HEADER_SENDERID_MASK		(0xFF)
#define IPC_MSG_HEADER_SENDERID(x)		MASK_BF(x, 0xFF, 8)

#define IPC_MSG_HEADER_NOTIFY_SHIFT		16
#define IPC_MSG_HEADER_NOTIFY_MASK		(0x1)
#define IPC_MSG_HEADER_NOTIFY(x)		MASK_BF(x, 0x1, 16)

#define IPC_MSG_HEADER_WITH_DATA_SHIFT		17
#define IPC_MSG_HEADER_WITH_DATA_MASK		(0x1)
#define IPC_MSG_HEADER_WITH_DATA(x)		MASK_BF(x, 0x1, 17)

#define IPC_MSG_HEADER_DATA_BYTES_SHIFT		18
#define IPC_MSG_HEADER_DATA_BYTES_MASK		(0xFFFC)
#define IPC_MSG_HEADER_DATA_BYTES(x)		MASK_BF(x, 0xFFFC, 18)

/* MSG_ID/CMD_ID,  header's HEADER_ID */
#define IPC_MSG_AP_2_DSP_0	0x01
#define IPC_MSG_AP_2_DSP_1	0x02

#define IPC_MSG_DSP_2_AP_0	0x20
#define IPC_MSG_DSP_2_AP_1	0x21

/* share info, if not have registers, can store info in iram or dram */
#define IPC_STS_DSP		0x00 /* IPC DSP -> AP */
#define IPC_STS_AP		0x08 /* IPC AP  -> DSP */
/*IPC_STS_DSP bf*/
#define IPC_STS_DSP_DONE	(0x1 << 30)
#define IPC_STS_DSP_BUSY	(0x1 << 31)
#define IPC_STS_DSP_DONE_MASK	(0x1 << 30)
#define IPC_STS_DSP_BUSY_MASK	(0x1 << 31)

/*IPC_STS_AP bf*/
#define IPC_STS_AP_DONE		(0x1 << 30)
#define IPC_STS_AP_BUSY		(0x1 << 31)
#define IPC_STS_AP_DONE_MASK	(0x1 << 30)
#define IPC_STS_AP_BUSY_MASK	(0x1 << 31)

#define INT_VEC_DSPB_MBOX7	(249) // sec tx
#define INT_VEC_DSPB_MBOX5	(248) // tx
#define INT_VEC_DSPB_MBOX6	(247) // sec rx
#define INT_VEC_DSPB_MBOX4	(246) // rx

#define INT_VEC_DSPA_MBOX7	(245) // sec tx
#define INT_VEC_DSPA_MBOX5	(244) // tx
#define INT_VEC_DSPA_MBOX6	(243) // sec rx
#define INT_VEC_DSPA_MBOX4	(242) // rx

static int hifi4dsp_tm2_dsp_load_fw(struct hifi4dsp_dsp *dsp);
static int hifi4dsp_tm2_dsp_reset(struct hifi4dsp_dsp *dsp);
static int hifi4dsp_tm2_dsp_start(struct hifi4dsp_dsp *dsp);
static int hifi4dsp_tm2_dsp_stop(struct hifi4dsp_dsp *dsp);
static int hifi4dsp_tm2_dsp_sleep(struct hifi4dsp_dsp *dsp);
static int hifi4dsp_tm2_dsp_wake(struct hifi4dsp_dsp *dsp);

static int hifi4dsp_miscdev_open(struct inode *inode, struct file *fp)
{
	/*
	 *struct hifi4dsp_priv *priv;
	 *dev = conainer_of(inode->i_cdev,struct globalmem_dev,cdev);
	 *flip->private_data = dev;
	 */
	int minor = iminor(inode);
	int major = imajor(inode);
	struct hifi4dsp_priv *priv;
	struct miscdevice *c;
	struct hifi4dsp_miscdev_t *pmscdev_t;

	c = fp->private_data;
	priv = hifi4dsp_p[0];
	pmscdev_t = container_of(c, struct hifi4dsp_miscdev_t, dsp_miscdev);
	if (pmscdev_t == NULL) {
		pr_err("hifi4dsp_miscdev_t == NULL\n");
		return -1;
	}
	if (pmscdev_t->priv == NULL) {
		pr_err("hifi4dsp_miscdev_t -> priv==NULL\n");
		return -1;
	}
	priv = pmscdev_t->priv;
	if (priv == NULL) {
		pr_err("hifi4dsp_miscdev_t pointer *pmscdev_t==NULL\n");
		return -1;
	}
	fp->private_data = priv;

	pr_debug("%s,%s,major=%d,minor=%d\n", __func__,
			priv->dev->kobj.name,
			major,
			minor);
	return 0;
}

static int hifi4dsp_miscdev_release(struct inode *inode, struct file *fp)
{
	return 0;
}

static long hifi4dsp_miscdev_ioctl(struct file *fp, unsigned int cmd,
	  unsigned long arg)
{
	int ret = 0;
	struct hifi4dsp_priv *priv;
	struct hifi4dsp_dsp *dsp;
	struct hifi4dsp_info_t *info;
	void __user *argp = (void __user *)arg;

	pr_debug("%s\n", __func__);
	if (fp->private_data == NULL) {
		pr_err("%s error: fp->private_data is null", __func__);
		return -1;
	}
	priv = (struct hifi4dsp_priv *)fp->private_data;
	if (priv == NULL) {
		pr_err("%s error: hifi4dsp_priv is null", __func__);
		return -1;
	}
	dsp = priv->dsp;
	if (dsp  == NULL) {
		pr_err("%s hifi4dsp_dsp is null:\n", __func__);
		return -1;
	}
	if (priv->dsp->dsp_fw == NULL) {
		pr_err("%s hifi4dsp_firmware is null:\n", __func__);
		return -1;
	}
	if (priv->dsp->fw == NULL) {
		pr_err("%s firmware is null:\n", __func__);
		return -1;
	}
	pr_debug("%s %s\n", __func__, priv->dev->kobj.name);
	info = kmalloc(sizeof(struct hifi4dsp_info_t), GFP_KERNEL);
	if (!info)
		return -1;

	switch (cmd) {
	case HIFI4DSP_TEST:
		pr_debug("%s HIFI4DSP_TEST\n", __func__);
	break;
	case HIFI4DSP_LOAD:
		pr_debug("%s HIFI4DSP_LOAD\n", __func__);
		ret = copy_from_user(info, argp,
				sizeof(struct hifi4dsp_info_t));
		priv->dsp->info = info;
		hifi4dsp_tm2_dsp_load_fw(priv->dsp);
	break;
	case HIFI4DSP_RESET:
		pr_debug("%s HIFI4DSP_RESET\n", __func__);
		ret = copy_from_user(info, argp,
				sizeof(struct hifi4dsp_info_t));
		priv->dsp->info = info;
		hifi4dsp_tm2_dsp_reset(priv->dsp);
	break;
	case HIFI4DSP_START:
		pr_debug("%s HIFI4DSP_START\n", __func__);
		ret = copy_from_user(info, argp,
				sizeof(struct hifi4dsp_info_t));
		priv->dsp->info = info;
		hifi4dsp_tm2_dsp_start(priv->dsp);
	break;
	case HIFI4DSP_STOP:
		pr_debug("%s HIFI4DSP_STOP\n", __func__);
		ret = copy_from_user(info, argp,
				sizeof(struct hifi4dsp_info_t));
		priv->dsp->info = info;
		hifi4dsp_tm2_dsp_stop(priv->dsp);
	break;
	case HIFI4DSP_SLEEP:
		pr_debug("%s HIFI4DSP_SLEEP\n", __func__);
		ret = copy_from_user(info, argp,
				sizeof(struct hifi4dsp_info_t));
		priv->dsp->info = info;
		hifi4dsp_tm2_dsp_sleep(priv->dsp);
	break;
	case HIFI4DSP_WAKE:
		pr_debug("%s HIFI4DSP_WAKE\n", __func__);
		ret = copy_from_user(info, argp,
				sizeof(struct hifi4dsp_info_t));
		priv->dsp->info = info;
		hifi4dsp_tm2_dsp_wake(priv->dsp);
	break;
	case HIFI4DSP_GET_INFO:
		pr_debug("%s HIFI4DSP_GET_INFO\n", __func__);
		ret = copy_from_user(info, argp,
				sizeof(struct hifi4dsp_info_t));
		pr_debug("%s HIFI4DSP_GET_INFO %s\n", __func__,
					info->fw_name);
		strcpy(info->fw_name, "1234abcdef");
		ret = copy_to_user(argp, info,
				sizeof(struct hifi4dsp_info_t));
		pr_debug("%s HIFI4DSP_GET_INFO %s\n", __func__,
					info->fw_name);
	break;
	default:
		pr_err("%s ioctl CMD error\n", __func__);
	break;
	}
	kfree(info);
	priv->dsp->info = NULL;

	return ret;
}

static int hifi4dsp_tm2_dsp_load_fw(struct hifi4dsp_dsp *dsp)
{
	int err = 0;
	struct hifi4dsp_firmware *new_dsp_fw;
	struct hifi4dsp_info_t *info = dsp->info;

	if (strlen(info->fw_name) == 0)
		return -1;
	pr_debug("info->fw_name:%s\n", info->fw_name);
	new_dsp_fw = hifi4dsp_fw_register(dsp, info->fw_name);
	if (new_dsp_fw == NULL) {
		pr_err("register firmware:%s error\n", info->fw_name);
		return -1;
	}
	dsp->dsp_fw = new_dsp_fw;  /*set newest fw as def fw of dsp*/
	strcpy(new_dsp_fw->name, info->fw_name);
	if (info->phy_addr != 0) { /*to be improved*/
		//info->phy_addr may !=0, but illegal
		new_dsp_fw->paddr = info->phy_addr;
		/*TODO*/
		/*new_dsp_fw->buf = phys_to_virt(new_dsp_fw->paddr);*/
		new_dsp_fw->buf = dsp->pdata->fw_buf;
	} else {
		new_dsp_fw->paddr = dsp->pdata->fw_paddr;
		new_dsp_fw->buf = dsp->pdata->fw_buf;
	}
	pr_debug("new hifi4dsp_firmware, name=%s, paddr=0x%llx, buf=0x%p\n",
		new_dsp_fw->name,
		(unsigned long long)new_dsp_fw->paddr,
		new_dsp_fw->buf);
	hifi4dsp_fw_load(new_dsp_fw);
	return err;
}

static int hifi4dsp_tm2_dsp_init(struct hifi4dsp_priv *priv,
	struct device *dev, struct hifi4dsp_pdata *pdata,
	struct hifi4dsp_dsp_device *dsp_dev)
{
	int err = 0;
	struct hifi4dsp_dsp *new_dsp;
	struct hifi4dsp_firmware *new_dsp_fw;
	struct firmware *fw;

	pr_debug("%s\n", __func__);
	/* init dsp */
	new_dsp = hifi4dsp_dsp_new(priv, pdata, dsp_dev);
	if (new_dsp == NULL) {
		pr_err("%s get new hifi4dsp_dsp error\n", __func__);
		err = -ENODEV;
		goto dsp_new_err;
	}

	/*malloc firmware*/
	fw = kzalloc(sizeof(struct firmware), GFP_KERNEL);
	if (new_dsp == NULL) {
		pr_err("kzalloc new firmware error\n");
		return -ENOMEM;
	}
	new_dsp->fw = fw;
	/*init id, irq, clk*/
	new_dsp->id = pdata->id;
	new_dsp->irq = pdata->irq;
	new_dsp->freq = pdata->clk_freq;

	/*init addr*/
	new_dsp->addr.fw_paddr = pdata->fw_paddr;
	new_dsp->addr.fw = pdata->fw_buf;
	new_dsp->addr.reg = pdata->reg;
	new_dsp->addr.reg_size = pdata->reg_size;
	/*init mailbox*/

	/* reset */
	hifi4dsp_dsp_reset(new_dsp);
	/* malloc hifi4dsp_firmware & init */
	new_dsp_fw = hifi4dsp_fw_new(new_dsp, new_dsp->fw, priv);
	new_dsp->dsp_fw = new_dsp_fw;

	/* add to priv*/
	priv->dsp_fw = new_dsp_fw;
	priv->dsp = new_dsp;

	hifi4dsp_dsp_boot(priv->dsp);

	pr_info("%s done\n", __func__);
dsp_new_err:
	return err;
}

static int hifi4dsp_tm2_dsp_reset(struct hifi4dsp_dsp *dsp)
{
	struct	hifi4dsp_info_t *info;

	pr_debug("%s\n", __func__);
	if (dsp->info != NULL)
		info = (struct	hifi4dsp_info_t *)dsp->info;

	dsp->info = NULL;
	return 0;
}

static int hifi4dsp_tm2_dsp_boot(struct hifi4dsp_dsp *dsp)
{
	struct  hifi4dsp_info_t *info;

	info = (struct  hifi4dsp_info_t *)dsp->info;
	pr_debug("%s\n", __func__);


	dsp->info = NULL;
	return 0;
}

static int hifi4dsp_tm2_dsp_start(struct hifi4dsp_dsp *dsp)
{
	struct  hifi4dsp_info_t *info;

	pr_debug("%s\n", __func__);
	info = (struct  hifi4dsp_info_t *)dsp->info;
	pr_debug("dsp_id: %d\n", dsp->id);
	pr_debug("dsp_freqence: %d Hz\n", dsp->freq);
	pr_debug("dsp_start_addr: 0x%llx\n",
		(unsigned long long)dsp->dsp_fw->paddr);

	if (dsp->dsp_clk == NULL)
		pr_err("dsp_clk=NULL\n");
	if (dsp->dsp_gate == NULL)
		pr_err("dsp_gate=NULL\n");

	//clk_set_rate(dsp->dsp_clk, dsp->freq);
	//clk_prepare_enable(dsp->dsp_clk);
	//clk_prepare_enable(dsp->dsp_gate);

	tm2_dsp_bootup(dsp->id, dsp->dsp_fw->paddr, dsp->freq);
	dsp->info = NULL;

	return 0;
}

static int hifi4dsp_tm2_dsp_stop(struct hifi4dsp_dsp *dsp)
{
	struct  hifi4dsp_info_t *info;

	info = (struct  hifi4dsp_info_t *)dsp->info;
	pr_debug("%s\n", __func__);


	dsp->info = NULL;
	return 0;
}

static int hifi4dsp_tm2_dsp_sleep(struct hifi4dsp_dsp *dsp)
{
	struct  hifi4dsp_info_t *info;

	info = (struct  hifi4dsp_info_t *)dsp->info;
	pr_debug("%s\n", __func__);


	dsp->info = NULL;
	return 0;
}

static int hifi4dsp_tm2_dsp_wake(struct hifi4dsp_dsp *dsp)
{
	struct  hifi4dsp_info_t *info;

	info = (struct  hifi4dsp_info_t *)dsp->info;
	pr_debug("%s\n", __func__);


	dsp->info = NULL;
	return 0;
}

static void hifi4dsp_tm2_dsp_free(struct hifi4dsp_dsp *dsp)
{
	pr_debug("%s\n", __func__);

	hifi4dsp_fw_free_all(dsp);
	//kfree(NULL) is safe and check is probably not required

	kfree(dsp->dsp_fw);
	dsp->dsp_fw = NULL;

	kfree(dsp->fw);
	dsp->fw = NULL;

	kfree(dsp->ops);
	dsp->ops = NULL;

	kfree(dsp->pdata);
	dsp->pdata = NULL;

	kfree(dsp);
}

static irqreturn_t hifi4dsp_tm2_dsp_irq(int irq, void *p)
{
	irqreturn_t ret = IRQ_NONE;
	u32 isrx;
	struct hifi4dsp_dsp *dsp = (struct hifi4dsp_dsp *) p;

	spin_lock(&dsp->spinlock);
	isrx = hifi4dsp_dsp_smem_read64_unlocked(dsp, IPC_STS_AP);
	//need improvement, process for rx_request  rx_done from dsp
	spin_unlock(&dsp->spinlock);
	ret = IRQ_WAKE_THREAD;
	return ret;
}

static inline u32 hifi4dsp_tm2_ipc_header_msg_id(u64 header)
{
	header &= IPC_MSG_HEADER_DATA_BYTES(IPC_MSG_HEADER_ID_MASK);
	return header >> IPC_MSG_HEADER_ID_SHIFT;
}

static inline u32 hifi4dsp_tm2_ipc_header_msg_data_bytes(u64 header)
{
	header &= IPC_MSG_HEADER_DATA_BYTES(IPC_MSG_HEADER_DATA_BYTES_MASK);
	return header >> IPC_MSG_HEADER_DATA_BYTES_SHIFT;
}

static int hifi4dsp_tm2_process_notification(struct hifi4dsp_priv *priv,
					unsigned long *flags)
{
	struct hifi4dsp_dsp *dsp = priv->dsp;
	u32 header;
	u32 msg_id;
	int handled = 1;

	/*todo, read msg header, get MSG_ID*/
	header = hifi4dsp_dsp_smem_read_unlocked(dsp, 0);
	msg_id = hifi4dsp_tm2_ipc_header_msg_id(header);

	switch (msg_id) {
	case IPC_MSG_DSP_2_AP_0:
		break;
	case IPC_MSG_DSP_2_AP_1:
		break;
	}

	return handled;
}

static int hifi4dsp_tm2_process_reply(struct hifi4dsp_priv *priv, u64 header)
{
	struct hifi4dsp_ipc_message *msg;

	msg = hifi4dsp_ipc_reply_find_msg(&priv->ipc, header);
	if (msg == NULL)
		return 1;

	if (header & IPC_MSG_HEADER_WITH_DATA(header)) {
		msg->rx_size = hifi4dsp_tm2_ipc_header_msg_data_bytes(header);
		hifi4dsp_dsp_mailbox_inbox_read(priv->dsp,
				msg->rx_data, msg->rx_size);
	}

	list_del(&msg->list);
	/* wake up */
	hifi4dsp_ipc_tx_msg_reply_complete(&priv->ipc, msg);

	return 1;
}

static irqreturn_t hifi4dsp_tm2_dsp_irq_thread(int irq, void *context)
{
	struct hifi4dsp_dsp  *dsp = (struct hifi4dsp_dsp *) context;
	struct hifi4dsp_priv *priv = dsp->priv;
	struct hifi4dsp_ipc  *ipc = &priv->ipc;
	u64 header;
	u32 ap_sts;
	unsigned long flags;

	spin_lock_irqsave(&dsp->spinlock, flags);

	ap_sts = hifi4dsp_dsp_smem_read_unlocked(dsp, IPC_STS_AP);
	header = hifi4dsp_dsp_smem_read64_unlocked(dsp, IPC_MSG_HEADER);
	if (header & (IPC_STS_AP_BUSY&ap_sts)) {
		if (header&IPC_MSG_HEADER_NOTIFY_MASK) {
			/* msg */
			hifi4dsp_tm2_process_notification(priv, &flags);
		} else {
			/* reply msg */
			hifi4dsp_tm2_process_reply(priv, header);
		}
		/*
		 * clear DSP BUSY bit and set DONE bit. Tell DSP we have
		 * processed the message and can accept new. Clear data part
		 * of the header
		 */
		hifi4dsp_dsp_smem_update_bits64_unlocked(dsp, IPC_STS_AP,
			IPC_STS_AP_BUSY|IPC_STS_AP_DONE, IPC_STS_AP_DONE);
		/* unmask msg request interrupts */
		//hifi4dsp_dsp_smem_update_bits_unlocked(dsp, 0);
	}

	spin_unlock_irqrestore(&dsp->spinlock, flags);

	/* continue to send any remaining messages... */
	kthread_queue_work(&ipc->kworker, &ipc->kwork);

	return IRQ_HANDLED;
}

/*transfer param from pdata to dsp*/
static int hifi4dsp_tm2_resource_map(struct hifi4dsp_dsp *dsp,
				struct hifi4dsp_pdata *pdata)
{
	int ret = 0;

	if (!pdata) {
		pr_err("%s error\n", __func__);
		ret = -1;
	}
	return ret;
}

static int hifi4dsp_tm2_init(struct hifi4dsp_dsp *dsp,
				struct hifi4dsp_pdata *pdata)
{
	struct device *dev;
	int ret = -ENODEV;

	dev = dsp->dev;
	pr_debug("%s\n", __func__);
	ret = hifi4dsp_tm2_resource_map(dsp, pdata);
	if (ret < 0) {
		dev_err(dev, "failed to map resources\n");
		return ret;
	}

	//ret = dma_coerce_mask_and_coherent(dsp->dma_dev, DMA_BIT_MASK(32));
	if (ret)
		return ret;

	/* enable interrupt from both arm sides and dsp side */
	/* todo */
	return 0;
}

static void hifi4dsp_tm2_free(struct hifi4dsp_priv *priv)
{
	pr_debug("%s\n", __func__);
	if (priv->dsp) {
		hifi4dsp_tm2_dsp_free(priv->dsp);
		priv->dsp = NULL;
	}
	if (priv->dsp_fw) {
		priv->dsp_fw = NULL;
		return;
	}
}

static int hifi4dsp_tm2_load_and_parse_fw(struct hifi4dsp_firmware *dsp_fw,
				void *pinfo)
{
	struct  hifi4dsp_info_t *info;

	info = (struct hifi4dsp_info_t *)pinfo;
	pr_debug("%s\n", __func__);
	hifi4dsp_tm2_dsp_load_fw(dsp_fw->dsp);
	return 0;
}

struct hifi4dsp_ops;
struct hifi4dsp_ops hifi4dsp_tm2_dsp_ops = {
	.boot = hifi4dsp_tm2_dsp_boot,
	.reset = hifi4dsp_tm2_dsp_reset,
	.sleep = hifi4dsp_tm2_dsp_sleep,
	.wake = hifi4dsp_tm2_dsp_wake,

	.write = hifi4dsp_smem_write,
	.read = hifi4dsp_smem_read,
	.write64 = hifi4dsp_smem_write64,
	.read64 = hifi4dsp_smem_read64,
	.ram_read = hifi4dsp_memcpy_fromio_32,
	.ram_write = hifi4dsp_memcpy_toio_32,

	.irq_handler = hifi4dsp_tm2_dsp_irq,
	.init = hifi4dsp_tm2_init,
	.free = hifi4dsp_tm2_dsp_free,
	.parse_fw = hifi4dsp_tm2_load_and_parse_fw,
};

static struct hifi4dsp_pdata dsp_pdatas[] = {/*ARRAY_SIZE(dsp_pdatas)*/
	{
		.id = 0,
		.name = "hifi4dsp0",
		.reg_paddr = 0xFF680000,
		.reg_size = 0x00100000,
		.clk_freq = 400*1000*1000,
		.smem_paddr = DSP_PHY_ADDR_0,
		.smem_size = HIFI4DSP_TM2_ALL_SMEM_SIZE,
		.irq = 242,
		.fw_paddr = DSP_PHY_ADDR_0,
	},
	{
		.id = 1,
		.name = "hifi4dsp1",
		.reg_paddr = 0xFF690000,
		.reg_size = 0x00100000,
		.clk_freq = 400*1000*1000,
		.irq = 246,
		.fw_paddr = DSP_PHY_ADDR_1,
	},
};

static struct hifi4dsp_dsp_device hifi4dsp_dev = {
		.thread = hifi4dsp_tm2_dsp_irq_thread,
		.ops = &hifi4dsp_tm2_dsp_ops,
};

static void hifi4dsp_tm2_ipc_dbg(struct hifi4dsp_ipc *ipc,
					const char *dbg_info)
{
	struct hifi4dsp_dsp *dsp = ipc->dsp;

	if (dsp == NULL) {
		pr_err("%s ipc->dsp is null\n", __func__);
		return;
	}
	/*dump mailbox register info*/
	pr_debug("%s %s\n", __func__, dbg_info);
}

/*
 *	call mailbox function to send the msg or
 *   directlly to write the msg to share register/memory
 *   then send notification msg though mailbox to DSP
 */
static void hifi4dsp_tm2_ipc_tx_msg(struct hifi4dsp_ipc *ipc,
				struct hifi4dsp_ipc_message *msg)
{
	if (msg->header)
		hifi4dsp_dsp_mailbox_outbox_write(ipc->dsp,
			msg->tx_data,
			msg->tx_size);
	/*hifi4dsp_dsp_smem_write_unlocked(ipc->dsp,
	 *			MBOX_TX_SET,
	 *			(u32)(msg->header);
	 */
}

/*
 * msg content = lower 32-bit of the header + data
 *
 */
static void hifi4dsp_tm2_ipc_data_copy(struct hifi4dsp_ipc_message *msg,
				char *tx_data, size_t tx_bytes)
{
	*(u32 *)msg->tx_data = (u32)(msg->header & (u32)-1);
	memcpy(msg->tx_data + sizeof(u32), tx_data, tx_bytes);
	msg->tx_size += sizeof(u32);
}

/*
 * read from mailbox register or
 *			share register between dsp and AP or
 *			share memory eara
 * to judge if dsp is busy
 */
static u64 hifi4dsp_tm2_ipc_reply_msg_match(u64 header, u64 *mask)
{
	/* match reply to message sent based on msg and stream IDs */
	*mask = IPC_MSG_HEADER_ID_MASK;
	header &= *mask;
	return header;
}

/*
 * read from mailbox register or
 *           share register between dsp and AP or
 *           share memory eara
 * to judge if dsp is busy
 */
static bool hifi4dsp_tm2_is_dsp_busy(struct hifi4dsp_dsp *dsp)
{
	u32 is_busy = 0;

	pr_debug("%s %s busy\n", __func__, is_busy?"":"not");
	return 0;
}

struct hifi4dsp_ipc_plat_ops hifi4dsp_tm2_ipc_ops = {
	.tx_msg = hifi4dsp_tm2_ipc_tx_msg,
	.tx_data_copy = hifi4dsp_tm2_ipc_data_copy,
	.reply_msg_match = hifi4dsp_tm2_ipc_reply_msg_match,
	.is_dsp_busy = hifi4dsp_tm2_is_dsp_busy,
	.debug_info = hifi4dsp_tm2_ipc_dbg,
};

struct hifi4dsp_priv *hifi4dsp_privdata()
{
	return hifi4dsp_p[0];
}

static int hifi4dsp_platform_remove(struct platform_device *pdev)
{
	struct hifi4dsp_priv *priv;
	int id = 0, dsp_cnt = 0;

	dsp_cnt = ARRAY_SIZE(dsp_pdatas);
	priv = hifi4dsp_privdata();
	for (id = 0; id < dsp_cnt; id++) {
		if (!priv)
			continue;
		hifi4dsp_tm2_free(priv);
		if (priv->dev)
			device_destroy(priv->class, priv->dev->devt);
		priv += 1;
	}
	kfree(priv);
	priv = NULL;
	for (id = 0; id < dsp_cnt; id++)
		hifi4dsp_p[id] = NULL;

	return 0;
}

static const struct file_operations hifi4dsp_miscdev_fops = {
	.owner = THIS_MODULE,
	.open = hifi4dsp_miscdev_open,
	.read = NULL,
	.write = NULL,
	.release = hifi4dsp_miscdev_release,
	.unlocked_ioctl = hifi4dsp_miscdev_ioctl,
	.compat_ioctl = hifi4dsp_miscdev_ioctl,
};

static struct miscdevice hifi4dsp_miscdev[] = {
	{
		.minor	= MISC_DYNAMIC_MINOR,
		.name	= "hifi4dsp0",
		.fops	= &hifi4dsp_miscdev_fops,
	},
	{
		.minor	= MISC_DYNAMIC_MINOR,
		.name	= "hifi4dsp1",
		.fops	= &hifi4dsp_miscdev_fops,
	}
};

static int hifi4dsp_priv_data_init(struct hifi4dsp_priv *priv,
		struct hifi4dsp_pdata *pdata)
{
	//priv->fw_id = 0;
	priv->dsp_is_started = false;
	priv->dsp_dev = &hifi4dsp_dev;
	priv->pdata = pdata;
	HIFI4DSP_PRNT("%s done\n", __func__);
	return 0;
}

static void *hifi4dsp_mm_vmap(phys_addr_t phys, unsigned long size)
{
	u32 offset, npages;
	struct page **pages = NULL;
	pgprot_t pgprot = PAGE_KERNEL;
	void *vaddr;
	int i;

	offset = offset_in_page(phys);
	npages = DIV_ROUND_UP(size + offset, PAGE_SIZE);

	pages = vmalloc(sizeof(struct page *) * npages);
	if (!pages)
		return NULL;
	for (i = 0; i < npages; i++) {
		pages[i] = phys_to_page(phys);
		phys += PAGE_SIZE;
	}
	/* pgprot = pgprot_writecombine(PAGE_KERNEL); */

	vaddr = vmap(pages, npages, VM_MAP, pgprot);
	if (!vaddr) {
		pr_err("vmaped fail, size: %d\n",
			npages << PAGE_SHIFT);
		vfree(pages);
		return NULL;
	}
	vfree(pages);
	pr_debug("[HIGH-MEM-MAP] pa(%lx) to va(%p), size: %d\n",
		(unsigned long)phys, vaddr, npages << PAGE_SHIFT);

	return vaddr;
}

/*of read clk_gate, clk*/
static inline int of_read_dsp_irq(
				struct platform_device *pdev, int dsp_id)
{
	int irq = -1;

	if (dsp_id == 0)
		irq = of_irq_get_byname(pdev->dev.of_node, "irq_frm_dspa");
	else if (dsp_id == 1)
		irq = of_irq_get_byname(pdev->dev.of_node, "irq_frm_dspb");

	pr_debug("%s %s irq=%d\n", __func__,
			(irq < 0)?"error":"successful", irq);

	return irq;
}

/*of read clk_gate, clk*/
static inline struct clk *of_read_dsp_clk(
				struct platform_device *pdev, int dsp_id)
{
	struct clk *p_clk = NULL;
	char clk_name[20];

	if (dsp_id == 0) {
		strcpy(clk_name, "dspa_clk");
		p_clk = devm_clk_get(&pdev->dev, clk_name);
	} else if (dsp_id == 1) {
		strcpy(clk_name, "dspb_clk");
		p_clk = devm_clk_get(&pdev->dev, clk_name);
	}
	if (!p_clk)
		pr_err("%s %s error\n", __func__, clk_name);

	return p_clk;
}

/*of read clk_gate, clk*/
static inline struct clk *of_read_dsp_clk_gate(
				struct platform_device *pdev, int dsp_id)
{
	struct clk *p_clk_gate = NULL;
	char clk_name[20];

	if (dsp_id == 0) {
		strcpy(clk_name, "dspa_gate");
		p_clk_gate = devm_clk_get(&pdev->dev, clk_name);
	} else if (dsp_id == 1) {
		strcpy(clk_name, "dspb_gate");
		p_clk_gate = devm_clk_get(&pdev->dev, clk_name);
	}
	if (!p_clk_gate)
		pr_err("%s %s error\n", __func__, clk_name);

	return p_clk_gate;
}

static int hifi4dsp_platform_probe(struct platform_device *pdev)
{
	int ret = 0;
	int i = 0, id = 0;
	unsigned int dsp_cnt = 0;
	struct hifi4dsp_priv *priv;
	struct hifi4dsp_pdata *pdata;
	struct hifi4dsp_miscdev_t *p_dsp_miscdev;
	struct miscdevice *pmscdev;
	struct page *page;
	struct resource res_mem;
	int mem_bytes;
	void	*fw_addr = NULL;
	void __iomem *dsp_regbase;
	int irq;
	struct device_node *np;
	struct clk *dsp_clk = NULL;
	struct clk *dsp_gate = NULL;

	np = pdev->dev.of_node;
	dsp_cnt = ARRAY_SIZE(dsp_pdatas);
	pr_debug("%s pdatas: dsp_cnt=%d\n", __func__, dsp_cnt);
	ret = of_property_read_u32(np, "dsp-cnt", &dsp_cnt);
	if (ret < 0) {
		dev_err(&pdev->dev, "Can't retrieve dsp-cnt\n");
		ret = -EINVAL;
		goto dsp_cnt_error;
	}
	pr_debug("%s of read dsp-cnt=%d\n", __func__, dsp_cnt);

	/*init miscdev_t, miscdevice*/
	p_dsp_miscdev = NULL;
	p_dsp_miscdev = kcalloc(dsp_cnt, sizeof(struct hifi4dsp_miscdev_t),
			GFP_KERNEL);
	if (p_dsp_miscdev == NULL) {
		HIFI4DSP_PRNT("kzalloc for p_dsp_miscdev error\n");
		ret = -ENOMEM;
		goto miscdev_malloc_error;
	}

	/*init hifi4dsp_priv*/
	priv = NULL;
	priv = kcalloc(dsp_cnt, sizeof(struct hifi4dsp_priv),
			GFP_KERNEL);
	if (priv == NULL) {
		HIFI4DSP_PRNT("kzalloc for hifi4dsp_priv error\n");
		ret = -ENOMEM;
		goto priv_malloc_error;
	}
	/*of read reserved memory*/
	ret = of_reserved_mem_device_init(&pdev->dev);
	if (ret) {
		pr_err("reserved memory init fail:%d\n", ret);
		ret = -ENOMEM;
		goto reserved_mem_alloc_error;
	}

	for (i = 0; i < dsp_cnt; i++) {
		id = i;
		p_dsp_miscdev += i;
		priv += i;
		pr_info("register dsp-%d start\n", id);

		memcpy(&(p_dsp_miscdev->dsp_miscdev),
			&hifi4dsp_miscdev[id], sizeof(struct miscdevice));
		pmscdev = &p_dsp_miscdev->dsp_miscdev;

		p_dsp_miscdev->priv = priv;

		/*of read reg base, ioremap it*/
		//res_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		if (of_address_to_resource(np, id, &res_mem)) {
			ret = -EINVAL;
			pr_err("%s didn't get iomem from dts\n", __func__);
			goto error2;
		}
		dsp_regbase = ioremap_nocache(res_mem.start,
					resource_size(&res_mem));
		pr_debug("dsp_regbase ioremap, reg:%x->%p, size:%x, %s\n",
			(u32)res_mem.start,
			dsp_regbase,
			(u32)resource_size(&res_mem),
			(dsp_regbase != NULL)?"successful":"error");

		/*of read irq num*/
		irq = of_read_dsp_irq(pdev, id);
		if (irq < 0) {
			ret = -EINVAL;
			goto error2;
		}

		/*cma alloc mem for firmware*/
		if (of_property_read_u32(np, "reserved_mem_size", &mem_bytes)) {
			mem_bytes = DEF_RESERVE_MEM_SIZE;
			pr_debug("of read reserved_mem_size error, def value:0x%x\n",
					mem_bytes);
		} else
			pr_info("reserved_mem_size:0x%x\n", mem_bytes);

		page = dma_alloc_from_contiguous(&pdev->dev,
					 mem_bytes >> PAGE_SHIFT, 0);
		if (!page) {
			pr_err("alloc page failed, ret:%p\n", page);
			return -ENOMEM;
		}
		if (!PageHighMem(page)) {
			//fw_addr = page_address(page);
			fw_addr = phys_to_virt(page_to_phys(page));
		} else {
			fw_addr = hifi4dsp_mm_vmap(page_to_phys(page),
					mem_bytes);
			pr_info("kernel addr map phys:0x%llx->virt:0x%p\n",
				(unsigned long long)page_to_phys(page),
				fw_addr);
		}
		pr_debug("get page:%p, 0x%lx, phys:0x%llx, virt:0x%p\n",
			page,
			page_to_pfn(page),
			(unsigned long long)page_to_phys(page),
			page_address(page));

		/*of read clk_gate, clk*/
		dsp_gate = of_read_dsp_clk_gate(pdev, id);
		dsp_clk = of_read_dsp_clk(pdev, id);
		priv->p_clk = dsp_clk;
		priv->p_clk_gate = dsp_gate;

		/*register dsp device*/
		//pmscdev = &hifi4dsp_miscdev;
		ret = misc_register(pmscdev);
		if (ret) {
			pr_err("register vad_miscdev error\n");
			goto error2;
		}
		priv->dev = pmscdev->this_device;

		pdata = &dsp_pdatas[i];
		pdata->fw_paddr = page_to_phys(page);
		pdata->fw_buf = fw_addr;
		pdata->fw_max_size = mem_bytes;
		pdata->reg_size = (u32)resource_size(&res_mem);
		pdata->reg = dsp_regbase;
		id = pdata->id;
		pdata->irq = irq;
		hifi4dsp_priv_data_init(priv, pdata);
		hifi4dsp_tm2_dsp_init(priv, priv->dev,
				priv->pdata, priv->dsp_dev);
		priv->dsp->dsp_clk = priv->p_clk;
		priv->dsp->dsp_gate = priv->p_clk_gate;

		hifi4dsp_p[i] = priv;
		//priv += 1;
		priv->dev = pmscdev->this_device;
		dev_set_drvdata(priv->dev, priv);

		pr_info("register dsp-%d done\n", id);
	}
	ret = 0;
	//tm2_dsp_hw_init(priv->dsp->id, priv->dsp->freq);
	tm2_dsp_regs_iomem_init();

	pr_info("%s done\n", __func__);

	goto done;

error2:
reserved_mem_alloc_error:
	kfree(priv);
priv_malloc_error:
	kfree(p_dsp_miscdev);
miscdev_malloc_error:
dsp_cnt_error:
done:
	return ret;
}

static const struct of_device_id hifi4dsp_device_id[] = {
	{
		.compatible = "amlogic, hifi4dsp",
	},
	{},
};
MODULE_DEVICE_TABLE(of, hifi4dsp_device_id);

static struct platform_driver hifi4dsp_platform_driver = {
	.driver = {
		.name  = "hifi4dsp",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(hifi4dsp_device_id),
	},
	.probe  = hifi4dsp_platform_probe,
	.remove = hifi4dsp_platform_remove,
};
module_platform_driver(hifi4dsp_platform_driver);

MODULE_AUTHOR("Shuyu Li");
MODULE_DESCRIPTION("HiFi DSP Module Driver");
MODULE_LICENSE("GPL v2");
