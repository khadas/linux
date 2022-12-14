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

static int gethifilogbufer(struct seq_file *s, void *what)
{
	int len, count = 0;
	struct hifi4dsp_dsp *dsp = s->private;
	char *hifi4logdata;
	unsigned int head, headr, tail, size, magic, basepaddr;

	if (!dsp || !dsp->logbuff)
		goto out;

	head = dsp->logbuff->head;
	tail = dsp->logbuff->tail;
	size = dsp->logbuff->size;
	headr = dsp->logbuff->headr;
	magic = dsp->logbuff->magic;
	basepaddr = dsp->logbuff->basepaddr;

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
		hifi4logdata[count++] = dsp->logbuff->buffer[head++];
		head %= size;
	}
	hifi4logdata[count] = '\0';
	seq_printf(s, "%s", hifi4logdata);
	kfree(hifi4logdata);
out:
	return 0;
}

void dsp_logbuff_show(int dspid)
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

static int gethifi_clkrate(struct seq_file *s, void *what)
{
	struct hifi4dsp_dsp *dsp = s->private;

	seq_printf(s, "%u", dsp->freq);
	return 0;
}

static ssize_t hifilogbuf_write(struct file *file, const char __user *userbuf,
			    size_t count, loff_t *ppos)
{
	return count;
}

static int hifilogbuf_open(struct inode *inode, struct file *file)
{
	return single_open(file, gethifilogbufer, inode->i_private);
}

static ssize_t hificlkrate_write(struct file *file, const char __user *userbuf,
			    size_t count, loff_t *ppos)
{
	struct seq_file *sf = file->private_data;
	struct hifi4dsp_dsp *dsp = sf->private;
	char buffer[15] = {0};
	u32 freq = 0;

	count = min_t(size_t, count, sizeof(buffer) - 1);

	if (copy_from_user(buffer, userbuf, count))
		return -EFAULT;
	if (kstrtouint(buffer, 0, &dsp->freq))
		return -EINVAL;

	if (hifi4dsp_p[dsp->id]->dsp->status_reg && dsp->dspstarted && !dsp->dsphang) {
		freq = dsp->freq;
		clk_set_rate(dsp->dsp_clk, dsp->freq);
		clk_prepare_enable(dsp->dsp_clk);
		scpi_send_data(&freq, sizeof(freq), dsp->id ? SCPI_DSPB : SCPI_DSPA,
			SCPI_CMD_HIFI5_FREQ_SET_END, NULL, 0);
	}

	return count;
}

static int hificlkrate_open(struct inode *inode, struct file *file)
{
	return single_open(file, gethifi_clkrate, inode->i_private);
}

static const struct file_operations logbuf_fops = {
	.open		= hifilogbuf_open,
	.read		= seq_read,
	.write		= hifilogbuf_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static const struct file_operations clkrate_fops = {
	.open		= hificlkrate_open,
	.read		= seq_read,
	.write		= hificlkrate_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

void create_hifi_debugfs_files(struct hifi4dsp_dsp *dsp)
{
	struct dentry *hifilogbuf_file;
	struct dentry *hififreq_file;

	dsp->debug_dir = debugfs_create_dir(dsp->pdata->name, NULL);
	if (!dsp->debug_dir) {
		pr_err("[%s]debugfs_create_dir %s failed..\n", __func__, dsp->pdata->name);
		return;
	}

	hifilogbuf_file = debugfs_create_file("logbuff", S_IFREG | 0440,
						dsp->debug_dir, dsp, &logbuf_fops);
	if (!hifilogbuf_file)
		pr_err("[%s]debugfs_create_file failed..\n", __func__);

	hififreq_file = debugfs_create_file("clk_rate", S_IFREG | 0440,
						dsp->debug_dir, dsp, &clkrate_fops);
	if (!hififreq_file)
		pr_err("[%s]debugfs_create_file failed..\n", __func__);
}

void hifi_syslog_remove(void)
{
	if (hifi4dsp_p[DSPA] && hifi4dsp_p[DSPA]->dsp)
		debugfs_remove_recursive(hifi4dsp_p[DSPA]->dsp->debug_dir);
	if (hifi4dsp_p[DSPB] && hifi4dsp_p[DSPB]->dsp)
		debugfs_remove_recursive(hifi4dsp_p[DSPB]->dsp->debug_dir);
}

