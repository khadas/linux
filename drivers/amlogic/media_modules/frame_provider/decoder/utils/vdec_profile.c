/*
 * drivers/amlogic/amports/vdec_profile.c
 *
 * Copyright (C) 2016 Amlogic, Inc. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/debugfs.h>
#include <linux/moduleparam.h>


#include <linux/amlogic/media/utils/vdec_reg.h>
#include "vdec_profile.h"
#include "vdec.h"

#define ISA_TIMERE 0x2662
#define ISA_TIMERE_HI 0x2663

#define PROFILE_REC_SIZE 40

static DEFINE_MUTEX(vdec_profile_mutex);
static int rec_wp;
static bool rec_wrapped;
static uint dec_time_stat_flag;
static uint dec_time_stat_reset;


struct dentry *root, *event;

#define MAX_INSTANCE_MUN  9

struct vdec_profile_time_stat_s {
	int time_6ms_less_cnt;
	int time_6_9ms_cnt;
	int time_9_12ms_cnt;
	int time_12_15ms_cnt;
	int time_15_18ms_cnt;
	int time_18_21ms_cnt;
	int time_21ms_up_cnt;
	u64 time_max_us;
	u64 time_total_us;
};

struct vdec_profile_statistics_s {
	bool status;
	u64 run_lasttimestamp;
	int run_cnt;
	u64 cb_lasttimestamp;
	int cb_cnt;
	u64 decode_first_us;
	struct vdec_profile_time_stat_s run2cb_time_stat;
	struct vdec_profile_time_stat_s decode_time_stat;
};

static struct vdec_profile_statistics_s statistics_s[MAX_INSTANCE_MUN];


struct vdec_profile_rec_s {
	struct vdec_s *vdec;
	u64 timestamp;
	int event;
	int para1;
	int para2;
};

static struct vdec_profile_rec_s recs[PROFILE_REC_SIZE];
static const char *event_name[VDEC_PROFILE_MAX_EVENT] = {
	"run",
	"cb",
	"save_input",
	"check run ready",
	"run ready",
	"disconnect",
	"dec_work",
	"info"
};

#if 0 /* get time from hardware. */
static u64 get_us_time_hw(void)
{
	u32 lo, hi1, hi2;
	int offset = 0;

	/* txlx, g12a isa register base is 0x3c00 */
	if (get_cpu_major_id() >= MESON_CPU_MAJOR_ID_TXLX)
		offset = 0x1600;

	do {
		hi1 = READ_MPEG_REG(ISA_TIMERE_HI + offset);
		lo = READ_MPEG_REG(ISA_TIMERE + offset);
		hi2 = READ_MPEG_REG(ISA_TIMERE_HI + offset);
	} while (hi1 != hi2);

	return (((u64)hi1) << 32) | lo;
}
#endif

static u64 get_us_time_system(void)
{
	struct timeval tv;

	do_gettimeofday(&tv);

	return div64_u64(timeval_to_ns(&tv), 1000);
}

static void vdec_profile_update_alloc_time(
	struct vdec_profile_time_stat_s *time_stat, u64 startus, u64 endus)
{
	u64 spend_time_us = endus - startus;

	if (spend_time_us > 0 && spend_time_us < 100000000) {
		if (spend_time_us < 6000)
			time_stat->time_6ms_less_cnt++;
		else if (spend_time_us < 9000)
			time_stat->time_6_9ms_cnt++;
		else if (spend_time_us < 12000)
			time_stat->time_9_12ms_cnt++;
		else if (spend_time_us < 15000)
			time_stat->time_12_15ms_cnt++;
		else if (spend_time_us < 18000)
			time_stat->time_15_18ms_cnt++;
		else if (spend_time_us < 21000)
			time_stat->time_18_21ms_cnt++;
		else
			time_stat->time_21ms_up_cnt++;
	}

	if (spend_time_us > time_stat->time_max_us)
		time_stat->time_max_us = spend_time_us;

	time_stat->time_total_us += spend_time_us;
}


static void vdec_profile_statistics(struct vdec_s *vdec, int event)
{
	struct vdec_profile_statistics_s *time_stat = NULL;
	u64 timestamp;
	int i;

	if (vdec->id >= MAX_INSTANCE_MUN)
		return;

	if (event != VDEC_PROFILE_EVENT_RUN &&
			event != VDEC_PROFILE_EVENT_CB)
		return;

	mutex_lock(&vdec_profile_mutex);

	if (dec_time_stat_reset == 1) {
		if (event != VDEC_PROFILE_EVENT_RUN) {
			mutex_unlock(&vdec_profile_mutex);
			return;
		}
		for (i = 0; i < MAX_INSTANCE_MUN; i++)
			memset(&statistics_s[i], 0,
				sizeof(struct vdec_profile_statistics_s));
		dec_time_stat_reset = 0;
	}

	time_stat = &statistics_s[vdec->id];
	timestamp = get_us_time_system();

	if (time_stat->status == false) {
		time_stat->decode_first_us = timestamp;
		time_stat->status = true;
	}

	if (event == VDEC_PROFILE_EVENT_RUN) {
		time_stat->run_lasttimestamp = timestamp;
		time_stat->run_cnt++;
	} else if (event == VDEC_PROFILE_EVENT_CB) {
		/*run2cb statistics*/
		vdec_profile_update_alloc_time(&time_stat->run2cb_time_stat, time_stat->run_lasttimestamp, timestamp);

		/*decode statistics*/
		if (time_stat->cb_cnt == 0)
			vdec_profile_update_alloc_time(&time_stat->decode_time_stat, time_stat->decode_first_us, timestamp);
		else
			vdec_profile_update_alloc_time(&time_stat->decode_time_stat, time_stat->cb_lasttimestamp, timestamp);

		time_stat->cb_lasttimestamp = timestamp;
		time_stat->cb_cnt++;
	}

	mutex_unlock(&vdec_profile_mutex);
}


void vdec_profile_more(struct vdec_s *vdec, int event, int para1, int para2)
{
	mutex_lock(&vdec_profile_mutex);

	recs[rec_wp].vdec = vdec;
	recs[rec_wp].timestamp = get_us_time_system();
	recs[rec_wp].event = event;
	recs[rec_wp].para1 = para1;
	recs[rec_wp].para2 = para2;

	rec_wp++;
	if (rec_wp == PROFILE_REC_SIZE) {
		rec_wrapped = true;
		rec_wp = 0;
	}

	mutex_unlock(&vdec_profile_mutex);
}
EXPORT_SYMBOL(vdec_profile_more);

void vdec_profile(struct vdec_s *vdec, int event)
{
	vdec_profile_more(vdec, event, 0 , 0);
	if (dec_time_stat_flag == 1)
		vdec_profile_statistics(vdec, event);
}
EXPORT_SYMBOL(vdec_profile);

void vdec_profile_flush(struct vdec_s *vdec)
{
	int i;

	if (vdec->id >= MAX_INSTANCE_MUN)
			return;

	mutex_lock(&vdec_profile_mutex);

	for (i = 0; i < PROFILE_REC_SIZE; i++) {
		if (recs[i].vdec == vdec)
			recs[i].vdec = NULL;
	}

	memset(&statistics_s[vdec->id], 0, sizeof(struct vdec_profile_statistics_s));

	mutex_unlock(&vdec_profile_mutex);
}

static const char *event_str(int event)
{
	if (event < VDEC_PROFILE_MAX_EVENT)
		return event_name[event];

	return "INVALID";
}

static int vdec_profile_dbg_show(struct seq_file *m, void *v)
{
	int i, end;
	u64 base_timestamp;

	mutex_lock(&vdec_profile_mutex);

	if (rec_wrapped) {
		i = rec_wp;
		end = rec_wp;
	} else {
		i = 0;
		end = rec_wp;
	}

	base_timestamp = recs[i].timestamp;
	while (1) {
		if ((!rec_wrapped) && (i == end))
			break;

		if (recs[i].vdec) {
			seq_printf(m, "[%s:%d] \t%016llu us : %s (%d,%d)\n",
				vdec_device_name_str(recs[i].vdec),
				recs[i].vdec->id,
				recs[i].timestamp - base_timestamp,
				event_str(recs[i].event),
				recs[i].para1,
				recs[i].para2
				);
		} else {
			seq_printf(m, "[%s:%d] \t%016llu us : %s (%d,%d)\n",
				"N/A",
				0,
				recs[i].timestamp - base_timestamp,
				event_str(recs[i].event),
				recs[i].para1,
				recs[i].para2
				);
		}
		if (++i == PROFILE_REC_SIZE)
			i = 0;

		if (rec_wrapped && (i == end))
			break;
	}

	mutex_unlock(&vdec_profile_mutex);

	return 0;
}

static int time_stat_profile_dbg_show(struct seq_file *m, void *v)
{
	int i;

	mutex_lock(&vdec_profile_mutex);

	for (i = 0; i < MAX_INSTANCE_MUN; i++)
	{
		if (statistics_s[i].status == false)
			continue;

		seq_printf(m, "[%d]run_cnt:%d, cb_cnt:%d\n\
			\t\t\ttime_total_us:%llu\n\
			\t\t\trun2cb time:\n\
			\t\t\ttime_max_us:%llu\n\
			\t\t\t[%d]run2cb ave_us:%llu\n\
			\t\t\ttime_6ms_less_cnt:%d\n\
			\t\t\ttime_6_9ms_cnt:%d\n\
			\t\t\ttime_9_12ms_cnt:%d\n\
			\t\t\ttime_12_15ms_cnt:%d\n\
			\t\t\ttime_15_18ms_cnt:%d\n\
			\t\t\ttime_18_21ms_cnt:%d\n\
			\t\t\ttime_21ms_up_cnt:%d\n\
			\t\t\tdecode time:\n\
			\t\t\ttime_total_us:%llu\n\
			\t\t\ttime_max_us:%llu\n\
			\t\t\t[%d]cb2cb ave_us:%llu\n\
			\t\t\ttime_6ms_less_cnt:%d\n\
			\t\t\ttime_6_9ms_cnt:%d\n\
			\t\t\ttime_9_12ms_cnt:%d\n\
			\t\t\ttime_12_15ms_cnt:%d\n\
			\t\t\ttime_15_18ms_cnt:%d\n\
			\t\t\ttime_18_21ms_cnt:%d\n\
			\t\t\ttime_21ms_up_cnt:%d\n",
			i,
			statistics_s[i].run_cnt,
			statistics_s[i].cb_cnt,
			statistics_s[i].run2cb_time_stat.time_total_us,
			statistics_s[i].run2cb_time_stat.time_max_us,
			i,
			div_u64(statistics_s[i].run2cb_time_stat.time_total_us , statistics_s[i].cb_cnt),
			statistics_s[i].run2cb_time_stat.time_6ms_less_cnt,
			statistics_s[i].run2cb_time_stat.time_6_9ms_cnt,
			statistics_s[i].run2cb_time_stat.time_9_12ms_cnt,
			statistics_s[i].run2cb_time_stat.time_12_15ms_cnt,
			statistics_s[i].run2cb_time_stat.time_15_18ms_cnt,
			statistics_s[i].run2cb_time_stat.time_18_21ms_cnt,
			statistics_s[i].run2cb_time_stat.time_21ms_up_cnt,
			statistics_s[i].decode_time_stat.time_total_us,
			statistics_s[i].decode_time_stat.time_max_us,
			i,
			div_u64(statistics_s[i].decode_time_stat.time_total_us , statistics_s[i].cb_cnt),
			statistics_s[i].decode_time_stat.time_6ms_less_cnt,
			statistics_s[i].decode_time_stat.time_6_9ms_cnt,
			statistics_s[i].decode_time_stat.time_9_12ms_cnt,
			statistics_s[i].decode_time_stat.time_12_15ms_cnt,
			statistics_s[i].decode_time_stat.time_15_18ms_cnt,
			statistics_s[i].decode_time_stat.time_18_21ms_cnt,
			statistics_s[i].decode_time_stat.time_21ms_up_cnt);
	}

	mutex_unlock(&vdec_profile_mutex);

	return 0;
}


static int vdec_profile_dbg_open(struct inode *inode, struct file *file)
{
	return single_open(file, vdec_profile_dbg_show, NULL);
}

static int time_stat_profile_dbg_open(struct inode *inode, struct file *file)
{
	return single_open(file, time_stat_profile_dbg_show, NULL);
}


static const struct file_operations event_dbg_fops = {
	.open    = vdec_profile_dbg_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release,
};

static const struct file_operations time_stat_dbg_fops = {
	.open    = time_stat_profile_dbg_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release,
};


#if 0 /*DEBUG_TMP*/
static int __init vdec_profile_init_debugfs(void)
{
	struct dentry *root, *event;

	root = debugfs_create_dir("vdec_profile", NULL);
	if (IS_ERR(root) || !root)
		goto err;

	event = debugfs_create_file("event", 0400, root, NULL,
			&event_dbg_fops);
	if (!event)
		goto err_1;

	mutex_init(&vdec_profile_mutex);

	return 0;

err_1:
	debugfs_remove(root);
err:
	pr_err("Can not create debugfs for vdec_profile\n");
	return 0;
}

#endif

int vdec_profile_init_debugfs(void)
{
	struct dentry *root, *event, *time_stat;

	root = debugfs_create_dir("vdec_profile", NULL);
	if (IS_ERR(root) || !root)
		goto err;

	event = debugfs_create_file("event", 0400, root, NULL,
			&event_dbg_fops);
	if (!event)
		goto err_1;

	time_stat = debugfs_create_file("time_stat", 0400, root, NULL,
			&time_stat_dbg_fops);
	if (!time_stat)
		goto err_2;

	mutex_init(&vdec_profile_mutex);

	return 0;

err_2:
	debugfs_remove(event);
err_1:
	debugfs_remove(root);
err:
	pr_err("Can not create debugfs for vdec_profile\n");
	return 0;
}
EXPORT_SYMBOL(vdec_profile_init_debugfs);

void vdec_profile_exit_debugfs(void)
{
	debugfs_remove(event);
	debugfs_remove(root);
}
EXPORT_SYMBOL(vdec_profile_exit_debugfs);

module_param(dec_time_stat_flag, uint, 0664);

module_param(dec_time_stat_reset, uint, 0664);


/*module_init(vdec_profile_init_debugfs);*/

