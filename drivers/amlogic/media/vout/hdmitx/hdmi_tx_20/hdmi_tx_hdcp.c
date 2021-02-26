// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/types.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/debugfs.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/major.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/spinlock_types.h>
#include <linux/extcon-provider.h>
#include <linux/amlogic/media/vout/hdmi_tx/hdmi_info_global.h>
#include <linux/amlogic/media/vout/hdmi_tx/hdmi_tx_module.h>
#include <linux/uaccess.h>
#include "hw/common.h"
#include "hdmi_tx_hdcp.h"
/*
 * hdmi_tx_hdcp.c
 * version 1.1
 */

static int hdmi_authenticated;

static DEFINE_MUTEX(mutex);
/* obs_cur, record current obs */
static struct hdcp_obs_val obs_cur;
/* if obs_cur is differ than last time, then save to obs_last */
static struct hdcp_obs_val obs_last;
static int write_buff(const char *fmt, ...);

static struct hdcplog_buf hdcplog_buf;

unsigned int hdcp_get_downstream_ver(void)
{
	unsigned int ret = 14;

	struct hdmitx_dev *hdev = get_hdmitx_device();

	/* if TX don't have HDCP22 key, skip RX hdcp22 ver */
	if (hdev->hwop.cntlddc(hdev, DDC_HDCP_22_LSTORE, 0) == 0)
		if (hdcp_rd_hdcp22_ver())
			ret = 22;
		else
			ret = 14;
	else
		ret = 14;
	return ret;
}

/* Notic: the HDCP key setting has been moved to uboot
 * On MBX project, it is too late for HDCP get from
 * other devices
 */

/* verify ksv, 20 ones and 20 zeroes */
int hdcp_ksv_valid(unsigned char *dat)
{
	int i, j, one_num = 0;

	for (i = 0; i < 5; i++) {
		for (j = 0; j < 8; j++) {
			if ((dat[i] >> j) & 0x1)
				one_num++;
		}
	}
	return one_num == 20;
}

static int save_obs_val(struct hdmitx_dev *hdev, struct hdcp_obs_val *obs)
{
	return hdev->hwop.cntlddc((struct hdmitx_dev *)obs,
		DDC_HDCP14_SAVE_OBS, 0);
}

static void pr_obs(struct hdcp_obs_val *obst0, struct hdcp_obs_val *obst1,
	unsigned int mask)
{
	/* if idx > HDCP_LOG_SIZE, then set idx as 0 for wrapping log */
#define GETBITS(val, start, len) (((val) >> (start)) & ((1 << (len)) - 1))
#define DIFF_ST(_v1, v0, _s, _l, str) \
	do { \
		typeof(_v1) v1 = (_v1); \
		typeof(_s) s = (_s); \
		typeof(_l) l = (_l); \
		if (GETBITS(v1, s, l) != (GETBITS(v0, s, l))) { \
			write_buff("%s: %x\n", str, GETBITS(v1, s, l)); \
		} \
	} while (0)

	if (mask & (1 << 0)) {
		if (GETBITS(obst1->obs0, 1, 7) != GETBITS(obst0->obs0, 1, 7)) {
			write_buff("StateA: %x  SubStateA: %x\n",
				GETBITS(obst1->obs0, 4, 4),
				GETBITS(obst1->obs0, 1, 3));
		}
		DIFF_ST(obst1->obs0, obst0->obs0, 0, 1, "hdcpengaged");
	}
	if (mask & (1 << 1)) {
		DIFF_ST(obst1->obs1, obst0->obs1, 4, 3, "StateOEG");
		DIFF_ST(obst1->obs1, obst0->obs1, 0, 4, "StateR");
	}
	if (mask & (1 << 2)) {
		DIFF_ST(obst1->obs2, obst0->obs2, 3, 3, "StateE");
		DIFF_ST(obst1->obs2, obst0->obs2, 0, 3, "StateEEG");
	}
	if (mask & (1 << 3)) {
		DIFF_ST(obst1->obs3, obst0->obs3, 7, 1, "RSVD");
		DIFF_ST(obst1->obs3, obst0->obs3, 6, 1, "Repeater");
		DIFF_ST(obst1->obs3, obst0->obs3, 5, 1, "KSV_FIFO_Ready");
		DIFF_ST(obst1->obs3, obst0->obs3, 4, 1, "Fast_i2c");
		DIFF_ST(obst1->obs3, obst0->obs3, 3, 1, "RSVD2");
		DIFF_ST(obst1->obs3, obst0->obs3, 2, 1, "Hdmi_mode");
		DIFF_ST(obst1->obs3, obst0->obs3, 1, 1, "Features1.1");
		DIFF_ST(obst1->obs3, obst0->obs3, 0, 1, "Fast_Reauth");
	}
	if (mask & (1 << 4)) {
		DIFF_ST(obst1->intstat, obst0->intstat, 7, 1, "hdcp_engaged");
		DIFF_ST(obst1->intstat, obst0->intstat, 6, 1, "hdcp_failed");
		DIFF_ST(obst1->intstat, obst0->intstat, 4, 1, "i2cnack");
		DIFF_ST(obst1->intstat, obst0->intstat, 3, 1,
			"lostarbitration");
		DIFF_ST(obst1->intstat, obst0->intstat, 2, 1,
			"keepouterrorint");
		DIFF_ST(obst1->intstat, obst0->intstat, 1, 1, "KSVsha1calcint");
	}
}

static void _hdcp_do_work(struct work_struct *work)
{
	int ret = 0;
	struct hdmitx_dev *hdev =
		container_of(work, struct hdmitx_dev, work_do_hdcp.work);

	switch (hdev->hdcp_mode) {
	case 2:
		/* hdev->HWOp.CntlMisc(hdev, MISC_HDCP_CLKDIS, 1); */
		/* schedule_delayed_work(&hdev->work_do_hdcp, HZ / 50); */
		break;
	case 1:
		mutex_lock(&mutex);
		ret = save_obs_val(hdev, &obs_cur);
		/* ret is NZ, then update obs_last */
		if (ret) {
			pr_obs(&obs_last, &obs_cur, ret);
			obs_last = obs_cur;
		}
		mutex_unlock(&mutex);
		/* log time frequency */
		schedule_delayed_work(&hdev->work_do_hdcp, HZ / 20);
		break;
	case 0:
	default:
		mutex_lock(&mutex);
		memset(&obs_cur, 0, sizeof(obs_cur));
		memset(&obs_last, 0, sizeof(obs_last));
		mutex_unlock(&mutex);
		hdev->hwop.cntlmisc(hdev, MISC_HDCP_CLKDIS, 0);
		break;
	}
}

void hdmitx_hdcp_do_work(struct hdmitx_dev *hdev)
{
	_hdcp_do_work(&hdev->work_do_hdcp.work);
}

static int hdmitx_hdcp_task(void *data)
{
	static int auth_trigger;
	struct hdmitx_dev *hdev = (struct hdmitx_dev *)data;

	INIT_DELAYED_WORK(&hdev->work_do_hdcp, _hdcp_do_work);
	while (hdev->hpd_event != 0xff) {
		hdmi_authenticated = hdev->hwop.cntlddc(hdev,
			DDC_HDCP_GET_AUTH, 0);
		hdmitx_hdcp_status(hdmi_authenticated);
		if (auth_trigger != hdmi_authenticated) {
			auth_trigger = hdmi_authenticated;
			pr_info("hdcptx: %d  auth: %d\n", hdev->hdcp_mode,
				auth_trigger);
		}
		msleep_interruptible(200);
	}

	return 0;
}

static int read_buff(char *p)
{
	if (hdcplog_buf.rd_pos == hdcplog_buf.wr_pos)
		return 0;

	*p = hdcplog_buf.buf[hdcplog_buf.rd_pos];
	hdcplog_buf.rd_pos = (hdcplog_buf.rd_pos + 1) % HDCP_LOG_SIZE;
	return 1;
}

static void _write_buff(char c)
{
	hdcplog_buf.buf[hdcplog_buf.wr_pos] = c;
	hdcplog_buf.wr_pos = (hdcplog_buf.wr_pos + 1) % HDCP_LOG_SIZE;
	if (hdcplog_buf.wr_pos == hdcplog_buf.rd_pos)
		hdcplog_buf.rd_pos = (hdcplog_buf.rd_pos + 1) % HDCP_LOG_SIZE;
	wake_up_interruptible(&hdcplog_buf.wait);
}

static int write_buff(const char *fmt, ...)
{
	va_list args;
	int i, len;
	static char temp[64];

	va_start(args, fmt);
	len = vsnprintf(temp, sizeof(temp), fmt, args);
	va_end(args);

	for (i = 0; i < len; i++)
		_write_buff(temp[i]);

	return len;
}

static int hdcplog_open(struct inode *inode, struct file *file)
{
	return 0;
}

static ssize_t hdcplog_read(struct file *file, char __user *buf,
			size_t count, loff_t *ppos)
{
	ssize_t error = -EINVAL;
	int i = 0;
	char c;

	if ((file->f_flags & O_NONBLOCK) && (hdcplog_buf.rd_pos == hdcplog_buf.wr_pos))
		return  -EAGAIN;

	if (!buf || !count)
		goto out;

	error = wait_event_interruptible(hdcplog_buf.wait,
			(hdcplog_buf.wr_pos != hdcplog_buf.rd_pos));
	if (error)
		goto out;
	while (!error && (read_buff(&c)) && i < count) {
		error = __put_user(c, buf);
		buf++;
		i++;
	}

	if (!error)
		error = i;
out:
	return error;
}

const struct file_operations hdcplog_ops = {
	.read = hdcplog_read,
	.open = hdcplog_open,
};

static struct dentry *hdmitx_dbgfs;

int hdmitx_hdcp_init(void)
{
	struct hdmitx_dev *hdev = get_hdmitx_device();
	struct dentry *entry;

	pr_info(HDCP "%s\n", __func__);
	if (!hdev->hdtx_dev) {
		pr_info(HDCP "exit for null device of hdmitx!\n");
		return -ENODEV;
	}

	memset(&hdcplog_buf, 0, sizeof(hdcplog_buf));
	init_waitqueue_head(&hdcplog_buf.wait);

	hdev->task_hdcp = kthread_run(hdmitx_hdcp_task,	(void *)hdev,
				      "kthread_hdcp");

	hdmitx_dbgfs = hdmitx_get_dbgfsdentry();
	if (!hdmitx_dbgfs)
		hdmitx_dbgfs = debugfs_create_dir(DEVICE_NAME, NULL);
	if (!hdmitx_dbgfs) {
		pr_err("can't create %s debugfs dir\n", DEVICE_NAME);
		return 0;
	}
	entry = debugfs_create_file("hdcp_log", S_IFREG | 0444,
			hdmitx_dbgfs, NULL,
			&hdcplog_ops);
	if (!entry)
		pr_err("debugfs create file %s failed\n", "hdcp_log");
	return 0;
}

void __exit hdmitx_hdcp_exit(void)
{
	struct hdmitx_dev *hdev = get_hdmitx_device();

	if (hdev)
		cancel_delayed_work_sync(&hdev->work_do_hdcp);
}

MODULE_PARM_DESC(hdmi_authenticated, "\n hdmi_authenticated\n");
module_param(hdmi_authenticated, int, 0444);
