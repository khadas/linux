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

static struct dentry *hifi4rtos_debug_dir;
static struct dentry *hifi4alogbuf_file;
static struct dentry *hifi4blogbuf_file;

#define SENDCMDTODSPA 1
#define SENDCMDTODSPB 0

void dumpsyslogbinary(void *base, u32 bytes)
{
	int i;
	u32 *buf;

	buf = (u32 *)base;
	for (i = 0; i < bytes / 4; i += 2) {
		pr_info("%u:   %08x     %08x\n",
			i, buf[i], buf[i + 1]);
	}
}

static struct page *hifi4mappage(phys_addr_t addr)
{
	struct page *page = pfn_to_page(addr >> PAGE_SHIFT);

	page = pfn_to_page(addr >> PAGE_SHIFT);

	return page;
}

static int dumphifi4syslog(u32 dspid, struct seq_file *s)
{
	int cnt = 0;
	char *hifi4logdata;
	char *tmpbuffer;
	struct page *page = NULL;
	u32 hifi4rtosinfo_addr;
	u32 hifi4rtosinfo_size;
	u32 hifi4rtosinfo_head;
	u32 hifi4rtosinfo_tail;

	tmpbuffer = NULL;
	hifi4logdata = NULL;

	if (strncmp(hifi4logbuffer[dspid].syslogstate, "ON", 2) == 0) {
		hifi4rtosinfo_addr = hifi4logbuffer[dspid].logbaseaddr;
		pr_info("hifi4rtosinfo_addr=0x%08x\n", hifi4rtosinfo_addr);
		hifi4rtosinfo_size = hifi4logbuffer[dspid].syslogsize;
		pr_info("hifi4rtosinfo_size=0x%08x\n", hifi4rtosinfo_size);
		hifi4rtosinfo_head = hifi4logbuffer[dspid].loghead;
		hifi4rtosinfo_tail = hifi4logbuffer[dspid].logtail;
		pr_info("hifi4rtosinfo_head=0x%08x, hifi4rtosinfo_tail:0x%08x\n",
			hifi4rtosinfo_head, hifi4rtosinfo_tail);

		dma_sync_single_for_device(hifi4dsp_p[0]->dsp->dev,
					   (phys_addr_t)hifi4rtosinfo_addr,
					   hifi4rtosinfo_size,
					   DMA_TO_DEVICE);

		/*Alloc log buffer memory*/
		hifi4logdata = kmalloc(hifi4rtosinfo_size, GFP_KERNEL);
		if (!hifi4logdata)
			return -ENOMEM;

		/*map hifi4 logbuffer to kernel space*/
		page = hifi4mappage((phys_addr_t)hifi4rtosinfo_addr);
		tmpbuffer = kmap(page) + (hifi4rtosinfo_addr & ~PAGE_MASK);
		tmpbuffer[hifi4rtosinfo_addr] = '\0';

		memset(hifi4logdata, 0, hifi4rtosinfo_size);

		if (hifi4rtosinfo_head < hifi4rtosinfo_tail) {
			pr_info("copy from head - tail to user space\n");
			cnt = hifi4rtosinfo_tail - hifi4rtosinfo_head;
			memcpy(hifi4logdata, tmpbuffer, cnt);
		} else if (hifi4rtosinfo_head > hifi4rtosinfo_tail) {
			cnt = hifi4rtosinfo_size;
			pr_debug("\ncopy from head to end\n");
			memcpy(hifi4logdata, &tmpbuffer[hifi4rtosinfo_head],
			       cnt - hifi4rtosinfo_head);
			pr_debug("\ncopy from 0 to tail\n");
			memcpy(&hifi4logdata[cnt - hifi4rtosinfo_head],
			       tmpbuffer, hifi4rtosinfo_tail + 1);
		}
		seq_printf(s, "%s\n", hifi4logdata);
		kunmap(page);
		page = NULL;
		kfree(hifi4logdata);
		hifi4logdata = NULL;
		strcpy(hifi4logbuffer[dspid].syslogstate, "OFF");
	} else {
		seq_printf(s, "hifi4[%u], syslogstate: %s, did not get reply ringbuffer information\n",
			   dspid, hifi4logbuffer[dspid].syslogstate);
	}
	return 0;
}

static int sendrequest(u32 dspid)
{
	char requestinfo[] = "AP request";

	strcpy(hifi4logbuffer[dspid].syslogstate, "OFF");
	/*send request cmd to dsp to get syslog args*/
	if (dspid == 0)
		scpi_send_dsp_cmd(requestinfo, sizeof(requestinfo),
				  SENDCMDTODSPA, SCPI_CMD_HIFI4SYSTLOG, 0);
	else
		scpi_send_dsp_cmd(requestinfo, sizeof(requestinfo),
				  SENDCMDTODSPB, SCPI_CMD_HIFI4SYSTLOG, 0);
	return 0;
}

static int gethifi4alogbufer(struct seq_file *s, void *what)
{
	int ret;

	ret = sendrequest(0);
	if (!ret)
		ret = dumphifi4syslog(0, s);
	return ret;
}

static int gethifi4blogbufer(struct seq_file *s, void *what)
{
	int ret;

	ret = sendrequest(1);
	if (!ret)
		ret = dumphifi4syslog(1, s);
	return ret;
}

static ssize_t logbuf_write(struct file *file, const char __user *userbuf,
			    size_t count, loff_t *ppos)
{
	return count;
}

static int hifi4alogbuf_open(struct inode *inode, struct file *file)
{
	return single_open(file, gethifi4alogbufer, inode->i_private);
}

static int hifi4blogbuf_open(struct inode *inode, struct file *file)
{
	return single_open(file, gethifi4blogbufer, inode->i_private);
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

void create_hifi4_syslog(void)
{
	hifi4rtos_debug_dir = debugfs_create_dir("hifi4frtos", NULL);
	if (!hifi4rtos_debug_dir) {
		pr_err("[%s]debugfs_create_dir failed..\n", __func__);
		return;
	}

	hifi4alogbuf_file = debugfs_create_file("hifi4alogbuf", S_IFREG | 0440,
						hifi4rtos_debug_dir, NULL,
						&logbuf_fops[0]);
	if (!hifi4alogbuf_file)
		pr_err("[%s]debugfs_create_file failed..\n", __func__);
	hifi4blogbuf_file = debugfs_create_file("hifi4blogbuf", S_IFREG | 0440,
						hifi4rtos_debug_dir, NULL,
						&logbuf_fops[1]);
	if (!hifi4alogbuf_file)
		pr_err("[%s]debugfs_create_file failed..\n", __func__);
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

