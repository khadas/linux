// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) Rockchip Electronics Co., Ltd. */
#include <linux/clk.h>
#include <linux/proc_fs.h>
#include <linux/sem.h>
#include <linux/seq_file.h>
#include <media/v4l2-common.h>

#include "dev.h"
#include "procfs.h"
#include "regs.h"
#include "version.h"

#ifdef CONFIG_PROC_FS
static int vpss_show(struct seq_file *p, void *v)
{
	struct rkvpss_device *dev = p->private;
	enum rkvpss_state state = dev->vpss_sdev.state;
	u32 val;

	seq_printf(p, "%-10s Version:v%02x.%02x.%02x\n",
		   dev->name,
		   RKVPSS_DRIVER_VERSION >> 16,
		   (RKVPSS_DRIVER_VERSION & 0xff00) >> 8,
		   RKVPSS_DRIVER_VERSION & 0x00ff);
	for (val = 0; val < dev->hw_dev->clks_num; val++) {
		seq_printf(p, "%-10s %ld\n",
			   dev->hw_dev->match_data->clks[val],
			   clk_get_rate(dev->hw_dev->clks[val]));
	}
	if (state != VPSS_START)
		return 0;

	seq_printf(p, "%-10s Cnt:%d ErrCnt:%d\n",
		   "Interrupt",
		   dev->isr_cnt,
		   dev->isr_err_cnt);
	return 0;
}

static int vpss_open(struct inode *inode, struct file *file)
{
	struct rkvpss_device *data = pde_data(inode);

	return single_open(file, vpss_show, data);
}

static const struct proc_ops ops = {
	.proc_open		= vpss_open,
	.proc_read		= seq_read,
	.proc_lseek		= seq_lseek,
	.proc_release		= single_release,
};

int rkvpss_proc_init(struct rkvpss_device *dev)
{
	dev->procfs = proc_create_data(dev->name, 0, NULL, &ops, dev);
	if (!dev->procfs)
		return -EINVAL;
	return 0;
}

void rkvpss_proc_cleanup(struct rkvpss_device *dev)
{
	if (dev->procfs)
		remove_proc_entry(dev->name, NULL);
	dev->procfs = NULL;
}
#endif /* CONFIG_PROC_FS */
