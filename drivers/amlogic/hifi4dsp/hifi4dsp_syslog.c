// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sysfs.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/kasan.h>
#include <linux/highmem.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/printk.h>
#include <linux/string.h>
#include <linux/arm-smccc.h>
#include <linux/psci.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <uapi/linux/psci.h>
#include <linux/debugfs.h>
#include <linux/kdebug.h>
#include <linux/sched/signal.h>
#include <linux/dma-mapping.h>
#include <linux/of_reserved_mem.h>
#include <linux/vmalloc.h>
#include <linux/clk.h>
#include <asm/cacheflush.h>
#include <linux/amlogic/scpi_protocol.h>
#include "hifi4dsp_priv.h"
#include "hifi4dsp_dsp.h"
#include "hifi4dsp_firmware.h"

static struct dentry *hifi4rtos_debug_dir;
static struct dentry *hifi4alogbuf_file;
static struct dentry *hifi4blogbuf_file;

unsigned int show_logbuff_log(struct dsp_ring_buffer *rb, int dspid,
		unsigned int len)
{
	char buffer[DSP_LOGBUFF_PRINT_LEN + 1] = {0};
	unsigned int count = 0;

	if (!rb || !len)
		return 0;
	else if (len > DSP_LOGBUFF_PRINT_LEN)
		len = DSP_LOGBUFF_PRINT_LEN;

	while (len--) {
		buffer[count++] = rb->buffer[rb->headr++];
		rb->headr %= rb->size;
	}
	buffer[count] = '\0';
	pr_info("[%s-%u]%s", dspid ? "DSPBB" : "DSPAA", count, buffer);

	return count;
}

unsigned int get_logbuff_loglen(struct dsp_ring_buffer *rb)
{
	if (!rb)
		return 0;
	if (rb->tail < rb->headr)
		return rb->tail + rb->size - rb->headr;
	else
		return rb->tail - rb->headr;
}

static int gethifi4logbufer(struct seq_file *s, void *what)
{
	int len, count = 0;
	struct hifi4dsp_priv *dsp_p = s->private;
	char *hifi4logdata;
	unsigned int head, headr, tail, size, magic, basepaddr;

	if (!dsp_p || !dsp_p->dsp || !dsp_p->dsp->logbuff)
		goto out;

	head = dsp_p->dsp->logbuff->head;
	tail = dsp_p->dsp->logbuff->tail;
	size = dsp_p->dsp->logbuff->size;
	headr = dsp_p->dsp->logbuff->headr;
	magic = dsp_p->dsp->logbuff->magic;
	basepaddr = dsp_p->dsp->logbuff->basepaddr;

	pr_debug("head:%u tail:%u size:%u headr:%u magic:0x%x basepaddr:0x%x\n",
		head, tail, size, headr, magic, basepaddr);
	if (tail == head)
		len = 0;
	else if (tail > head)
		len = tail - head;
	else
		len = size - 1;
	if (len == 0) {
		seq_printf(s, "%s", "buffer length is 0\n");
		goto out;
	}
	hifi4logdata = kzalloc(len + 1, GFP_KERNEL);
	if (!hifi4logdata)
		return -ENOMEM;

	while (len--) {
		hifi4logdata[count++] = dsp_p->dsp->logbuff->buffer[head++];
		head %= size;
	}
	hifi4logdata[count] = '\0';
	seq_printf(s, "%s", hifi4logdata);
	kfree(hifi4logdata);
out:
	return 0;
}

static void dsp_logbuff_show(int dspid)
{
	struct hifi4dsp_priv *dsp_p = hifi4dsp_p[dspid];
	int len;

	if (!dsp_p || !dsp_p->dsp || !dsp_p->dsp->logbuff)
		return;

	len = get_logbuff_loglen(dsp_p->dsp->logbuff);
	if (len > 0) {
		pr_info("=========dsp%d log dump begin=========\n", dspid);
		while (len > DSP_LOGBUFF_PRINT_LEN) {
			show_logbuff_log(dsp_p->dsp->logbuff, dspid, DSP_LOGBUFF_PRINT_LEN);
			len -= DSP_LOGBUFF_PRINT_LEN;
		}
		show_logbuff_log(dsp_p->dsp->logbuff, dspid, len);
		pr_info("=========dsp%d log dump end=========\n", dspid);
	}
}

static ssize_t logbuf_write(struct file *file, const char __user *userbuf,
			    size_t count, loff_t *ppos)
{
	return count;
}

static int hifi4alogbuf_open(struct inode *inode, struct file *file)
{
	return single_open(file, gethifi4logbufer, inode->i_private);
}

static int hifi4blogbuf_open(struct inode *inode, struct file *file)
{
	return single_open(file, gethifi4logbufer, inode->i_private);
}

static const struct file_operations logbuf_fops[2] = {
{
	.open		= hifi4alogbuf_open,
	.read		= seq_read,
	.write		= logbuf_write,
	.llseek		= seq_lseek,
	.release	= single_release,
},
{
	.open		= hifi4blogbuf_open,
	.read		= seq_read,
	.write		= logbuf_write,
	.llseek		= seq_lseek,
	.release	= single_release,
}

};

static int die_notify(struct notifier_block *self,
			unsigned long cmd, void *ptr)
{
	dsp_logbuff_show(DSPA);
	dsp_logbuff_show(DSPB);
	return NOTIFY_DONE;
}

static struct notifier_block die_notifier = {
	.notifier_call	= die_notify,
};

void create_hifi4_syslog(void)
{
	hifi4rtos_debug_dir = debugfs_create_dir("hifi4frtos", NULL);
	if (!hifi4rtos_debug_dir) {
		pr_err("[%s]debugfs_create_dir failed..\n", __func__);
		return;
	}

	hifi4alogbuf_file = debugfs_create_file("hifi4alogbuf", S_IFREG | 0440,
						hifi4rtos_debug_dir, hifi4dsp_p[0],
						&logbuf_fops[0]);
	if (!hifi4alogbuf_file)
		pr_err("[%s]debugfs_create_file failed..\n", __func__);
	hifi4blogbuf_file = debugfs_create_file("hifi4blogbuf", S_IFREG | 0440,
						hifi4rtos_debug_dir, hifi4dsp_p[1],
						&logbuf_fops[1]);
	if (!hifi4blogbuf_file)
		pr_err("[%s]debugfs_create_file failed..\n", __func__);
	if (register_die_notifier(&die_notifier))
		pr_err("%s,register die notifier failed!\n", __func__);
}

void hifi4_syslog_reomve(void)
{
	debugfs_remove(hifi4alogbuf_file);
	hifi4alogbuf_file = NULL;
	debugfs_remove(hifi4blogbuf_file);
	hifi4blogbuf_file = NULL;

	debugfs_remove(hifi4rtos_debug_dir);
	hifi4rtos_debug_dir = NULL;
}

