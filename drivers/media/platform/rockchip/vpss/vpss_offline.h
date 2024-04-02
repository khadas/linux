/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) 2023 Rockchip Electronics Co., Ltd. */

#ifndef _RKVPSS_OFFLINE_H
#define _RKVPSS_OFFLINE_H
#define DEV_NUM_MAX 10

#include "hw.h"

struct rkvpss_ofl_incfginfo {
	int width;
	int height;
	int format;
	int buf_fd;
};

struct rkvpss_ofl_outcfginfo {
	int enable;
	int format;
	int buf_fd;

	int crop_h_offs;
	int crop_v_offs;
	int crop_width;
	int crop_height;

	int scl_width;
	int scl_height;
};

struct rkvpss_ofl_cfginfo {
	int dev_id;
	int sequence;
	struct rkvpss_ofl_incfginfo input;
	struct rkvpss_ofl_outcfginfo output[RKVPSS_OUTPUT_MAX];
	struct list_head list;
};

struct rkvpss_dev_rate {
	u32 sequence;
	u32 in_rate;
	u64 in_timestamp;
	u64 out_timestamp;
	u32 out_rate;
	u32 delay;
};

struct rkvpss_offline_dev {
	struct rkvpss_hw_dev *hw;
	struct v4l2_device v4l2_dev;
	struct video_device vfd;
	struct mutex apilock;
	struct completion cmpl;
	struct list_head list;
	struct proc_dir_entry *procfs;
	struct list_head cfginfo_list;
	struct mutex ofl_lock;
	struct rkvpss_dev_rate dev_rate[DEV_NUM_MAX];
	bool mode_sel_en;
};

int rkvpss_register_offline(struct rkvpss_hw_dev *hw);
void rkvpss_unregister_offline(struct rkvpss_hw_dev *hw);
void rkvpss_offline_irq(struct rkvpss_hw_dev *hw, u32 irq);

#endif
