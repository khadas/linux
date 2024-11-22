/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) 2023 Fuzhou Rockchip Electronics Co., Ltd. */

#ifndef _RKVPSS_DEV_H
#define _RKVPSS_DEV_H

#include <linux/rk-vpss-config.h>

#include "hw.h"
#include "procfs.h"
#include "stream.h"
#include "vpss.h"

#define DRIVER_NAME			"rkvpss"
#define S0_VDEV_NAME DRIVER_NAME	"_scale0"
#define S1_VDEV_NAME DRIVER_NAME	"_scale1"
#define S2_VDEV_NAME DRIVER_NAME	"_scale2"
#define S3_VDEV_NAME DRIVER_NAME	"_scale3"

enum rkvpss_input {
	INP_INVAL = 0,
	INP_ISP,
};

enum {
	T_CMD_QUEUE,
	T_CMD_DEQUEUE,
	T_CMD_LEN,
	T_CMD_END,
};

enum {
	VPSS_UNITE_LEFT = 0,
	VPSS_UNITE_RIGHT,
	VPSS_UNITE_MAX,
};

struct rkvpss_rdbk_info {
	u64 timestamp;
	u64 seq;
};

struct rkvpss_device {
	char name[128];
	struct device *dev;
	void *sw_base_addr;
	struct v4l2_device v4l2_dev;
	struct media_device media_dev;
	struct v4l2_async_notifier notifier;
	struct v4l2_ctrl_handler ctrl_handler;
	struct v4l2_subdev *remote_sd;

	struct rkvpss_hw_dev *hw_dev;
	struct rkvpss_subdev vpss_sdev;
	struct rkvpss_stream_vdev stream_vdev;
	struct proc_dir_entry *procfs;

	atomic_t pipe_power_cnt;
	atomic_t pipe_stream_cnt;

	spinlock_t cmsc_lock;
	struct rkvpss_cmsc_cfg cmsc_cfg;

	enum rkvpss_ver	vpss_ver;
	/* mutex to serialize the calls from user */
	struct mutex apilock;
	enum rkvpss_input inp;
	u32 dev_id;
	u32 isr_cnt;
	u32 isr_err_cnt;

	bool mir_en;
	bool cmsc_upd;
	u32 unite_mode;
	u8 unite_index;
	bool stopping;
	wait_queue_head_t stop_done;
	unsigned int irq_ends;
	unsigned int irq_ends_mask;

	bool is_probe_end;
	bool is_suspend;
	bool is_idle;
	struct completion pm_suspend_wait_fe;
};

void rkvpss_pipeline_default_fmt(struct rkvpss_device *dev);
int rkvpss_pipeline_open(struct rkvpss_device *dev);
int rkvpss_pipeline_close(struct rkvpss_device *dev);
int rkvpss_pipeline_stream(struct rkvpss_device *dev, bool on);
#endif
