/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 *
 */

#ifndef __RKX11X_DEBUGFS_H__
#define __RKX11X_DEBUGFS_H__

#include <linux/version.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/i2c.h>

#include "rkx11x_drv.h"

/* rkx11x debugfs api */
int rkx11x_debugfs_init(struct rkx11x *rkx11x);
void rkx11x_debugfs_deinit(struct rkx11x *rkx11x);

#endif /* __RKX11X_DEBUGFS_H__ */
