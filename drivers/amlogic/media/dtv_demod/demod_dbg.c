/*
 * drivers/amlogic/media/dtv_demod/demod_dbg.c
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

#include <linux/debugfs.h>
#include "demod_func.h"
#include "amlfrontend.h"
#include "demod_dbg.h"
#include <linux/string.h>

static void demod_dump_atsc_reg(struct seq_file *seq)
{
	unsigned int reg_start, reg_end;

	if (is_ic_ver(IC_VER_TXLX)) {
		reg_start = 0x0;
		reg_end = 0xfff;

		for (; reg_start <= reg_end; reg_start++) {
			if (reg_start % 8 == 0)
				seq_printf(seq, "\n[addr 0x%03x] ", reg_start);
			seq_printf(seq, "0x%02x\t", atsc_read_reg(reg_start));
		}

		seq_puts(seq, "\n");
	} else if (is_ic_ver(IC_VER_TL1)) {
	}
}

static int demod_dump_reg_show(struct seq_file *seq, void *v)
{
	if (demod_get_current_mode() == AML_ATSC)
		demod_dump_atsc_reg(seq);
	else if (demod_get_current_mode() == UNKNOWN)
		seq_puts(seq, "current mode is unknown\n");

	return 0;
}

#define DEFINE_SHOW_DEMOD(__name) \
static int __name ## _open(struct inode *inode, struct file *file)	\
{ \
	return single_open(file, __name ## _show, inode->i_private);	\
} \
									\
static const struct file_operations __name ## _fops = {			\
	.owner = THIS_MODULE,		\
	.open = __name ## _open,	\
	.read = seq_read,		\
	.llseek = seq_lseek,		\
	.release = single_release,	\
}

/*cat /sys/kernel/debug/demod/dump_reg*/
DEFINE_SHOW_DEMOD(demod_dump_reg);

static int dvbc_fast_search_open(struct inode *inode, struct file *file)
{
	PR_DVBC("dvbc fast channel search Open\n");
	return 0;
}

static int dvbc_fast_search_release(struct inode *inode,
	struct file *file)
{
	PR_DVBC("dvbc fast channel search Release\n");
	return 0;
}

#define BUFFER_SIZE 100
static ssize_t dvbc_fast_search_show(struct file *file,
	char __user *userbuf, size_t count, loff_t *ppos)
{
	char buf[BUFFER_SIZE];
	unsigned int len;

	len = snprintf(buf, BUFFER_SIZE, "channel fast search en : %d\n",
		demod_dvbc_get_fast_search());
	/*len += snprintf(buf + len, BUFFER_SIZE - len, "");*/

	return simple_read_from_buffer(userbuf, count, ppos, buf, len);
}

static ssize_t dvbc_fast_search_store(struct file *file,
		const char __user *userbuf, size_t count, loff_t *ppos)
{
	char buf[80];
	char cmd[80], para[80];
	int ret;

	count = min_t(size_t, count, (sizeof(buf)-1));
	if (copy_from_user(buf, userbuf, count))
		return -EFAULT;

	buf[count] = 0;

	ret = sscanf(buf, "%s %s", cmd, para);

	if (!strcmp(cmd, "fast_search")) {
		PR_INFO("channel fast search: ");

		if (!strcmp(para, "on")) {
			PR_INFO("on\n");
			demod_dvbc_set_fast_search(1);
		} else if (!strcmp(para, "off")) {
			PR_INFO("off\n");
			demod_dvbc_set_fast_search(0);
		}
	}

	return count;
}

static int adc_clk_open(struct inode *inode, struct file *file)
{
	PR_INFO("adc clk Open\n");
	return 0;
}

static int adc_clk_release(struct inode *inode,
	struct file *file)
{
	PR_INFO("adc clk Release\n");
	return 0;
}

#define BUFFER_SIZE 100
static unsigned int adc_clk;
static ssize_t adc_clk_show(struct file *file,
	char __user *userbuf, size_t count, loff_t *ppos)
{
	char buf[BUFFER_SIZE];
	unsigned int len;

	len = snprintf(buf, BUFFER_SIZE, "adc clk  sys setting %dM, dbg %dM\n",
		demod_get_adc_clk() / 1000, adc_clk);
	/*len += snprintf(buf + len, BUFFER_SIZE - len, "");*/

	return simple_read_from_buffer(userbuf, count, ppos, buf, len);
}

static void adc_clk_set(unsigned int clk)
{
	int nco_rate = 0;

	if (is_ic_ver(IC_VER_TL1)) {
		if (clk == 24) {
			dd_tvafe_hiu_reg_write(ADC_PLL_CNTL0_TL1, 0x012004e0);
			dd_tvafe_hiu_reg_write(ADC_PLL_CNTL0_TL1, 0x312004e0);
			dd_tvafe_hiu_reg_write(ADC_PLL_CNTL1_TL1, 0x05400000);
			dd_tvafe_hiu_reg_write(ADC_PLL_CNTL2_TL1, 0xe0800000);
			dd_tvafe_hiu_reg_write(ADC_PLL_CNTL0_TL1, 0x111104e0);
			dtmb_write_reg(DTMB_FRONT_DDC_BYPASS, 0x6aaaaa);
			dtmb_write_reg(DTMB_FRONT_SRC_CONFIG1, 0x13196596);
			dtmb_write_reg(0x5b << 2, 0x50a30a25);
			nco_rate = (24000 * 256) / demod_get_sys_clk() + 2;
			adc_clk = 24;
		} else if (clk == 25) {
			dd_tvafe_hiu_reg_write(ADC_PLL_CNTL0_TL1, 0x001104c8);
			dd_tvafe_hiu_reg_write(ADC_PLL_CNTL0_TL1, 0x301104c8);
			dd_tvafe_hiu_reg_write(ADC_PLL_CNTL1_TL1, 0x03000000);
			dd_tvafe_hiu_reg_write(ADC_PLL_CNTL2_TL1, 0xe1800000);
			dd_tvafe_hiu_reg_write(ADC_PLL_CNTL0_TL1, 0x101104c8);
			dtmb_write_reg(DTMB_FRONT_DDC_BYPASS, 0x62c1a5);
			dtmb_write_reg(DTMB_FRONT_SRC_CONFIG1, 0x131a747d);
			dtmb_write_reg(0x5b << 2, 0x4d6a0a25);
			nco_rate = (25000 * 256) / demod_get_sys_clk() + 2;
			adc_clk = 25;
		} else {
			PR_ERR("wrong setting : adc clk\n");
		}

		if (nco_rate != 0)
			front_write_reg_v4(0x20,
				((front_read_reg_v4(0x20) & ~0xff)
				| (nco_rate & 0xff)));
	} else {
		PR_ERR("only TL1 has this functionality\n");
	}
}

static ssize_t adc_clk_store(struct file *file,
		const char __user *userbuf, size_t count, loff_t *ppos)
{
	char buf[80];
	char cmd[80], para[80];
	int ret;

	count = min_t(size_t, count, (sizeof(buf)-1));
	if (copy_from_user(buf, userbuf, count))
		return -EFAULT;

	buf[count] = 0;

	ret = sscanf(buf, "%s %s", cmd, para);

	if (!strcmp(cmd, "adc_clk")) {
		PR_INFO("set adc clk = ");

		if (!strcmp(para, "24")) {
			PR_INFO("24M\n");
			adc_clk_set(24);
		} else if (!strcmp(para, "25")) {
			PR_INFO("25M\n");
			adc_clk_set(25);
		}
	}

	return count;
}

#define DEFINE_SHOW_STORE_DEMOD(__name) \
static const struct file_operations __name ## _fops = {	\
	.owner = THIS_MODULE,		\
	.open = __name ## _open,	\
	.release = __name ## _release,	\
	.read = __name ## _show,		\
	.write = __name ## _store,	\
}

/*echo fast_search on > /sys/kernel/debug/demod/dvbc_channel_fast*/
DEFINE_SHOW_STORE_DEMOD(dvbc_fast_search);

/*echo adc_clk 24 > /sys/kernel/debug/demod/adc_clk*/
DEFINE_SHOW_STORE_DEMOD(adc_clk);

static struct demod_debugfs_files_t demod_debug_files[] = {
	{"dump_reg", S_IFREG | 0644, &demod_dump_reg_fops},
	{"dvbc_channel_fast", S_IFREG | 0644, &dvbc_fast_search_fops},
	{"adc_clk", S_IFREG | 0644, &adc_clk_fops},
};

void aml_demod_dbg_init(void)
{
	struct dentry *root_entry = dtvdd_devp->demod_root;
	struct dentry *entry;
	unsigned int i;

	PR_INFO("%s\n", __func__);

	root_entry = debugfs_create_dir("demod", NULL);
	if (!root_entry) {
		PR_INFO("Can't create debugfs dir frontend.\n");
		return;
	}

	for (i = 0; i < ARRAY_SIZE(demod_debug_files); i++) {
		entry = debugfs_create_file(demod_debug_files[i].name,
			demod_debug_files[i].mode,
			root_entry, NULL,
			demod_debug_files[i].fops);
		if (!entry)
			PR_INFO("Can't create debugfs seq file.\n");
	}
}

void aml_demod_dbg_exit(void)
{
	struct dentry *root_entry = dtvdd_devp->demod_root;

	if (dtvdd_devp && root_entry)
		debugfs_remove_recursive(root_entry);
}


