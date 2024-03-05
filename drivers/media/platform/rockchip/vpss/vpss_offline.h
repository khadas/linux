/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) 2023 Rockchip Electronics Co., Ltd. */

#ifndef _RKVPSS_OFFLINE_H
#define _RKVPSS_OFFLINE_H

#include "hw.h"

struct rkvpss_offline_dev {
	struct rkvpss_hw_dev *hw;
	struct v4l2_device v4l2_dev;
	struct video_device vfd;
	struct mutex apilock;
	struct completion cmpl;
	struct list_head list;
};

int rkvpss_register_offline(struct rkvpss_hw_dev *hw);
void rkvpss_unregister_offline(struct rkvpss_hw_dev *hw);
void rkvpss_offline_irq(struct rkvpss_hw_dev *hw, u32 irq);

#endif
