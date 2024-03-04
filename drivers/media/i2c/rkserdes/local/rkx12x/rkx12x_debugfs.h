/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 *
 */

#ifndef __RKX12X_DEBUGFS_H__
#define __RKX12X_DEBUGFS_H__

#include <linux/version.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/i2c.h>

#include "rkx12x_drv.h"

/* rkx12x debugfs api */
int rkx12x_debugfs_init(struct rkx12x *rkx12x);
void rkx12x_debugfs_deinit(struct rkx12x *rkx12x);

#endif /* __RKX12X_DEBUGFS_H__ */
