/* SPDX-License-Identifier: GPL-2.0 */
/*
 * RKx12x Deserializer driver API function declaration
 *
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 *
 * Author: Cai Wenzhong <cwz@rock-chips.com>
 */

#ifndef __RKX12X_API_H__
#define __RKX12X_API_H__

#include <linux/version.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/i2c-mux.h>

#include "rkx12x_i2c.h"
#include "rkx12x_drv.h"
#include "rkx12x_compact.h"
#include "rkx12x_debugfs.h"
#include "rkx12x_remote.h"
#include "rkx12x_linkrx.h"
#include "rkx12x_txphy.h"
#include "rkx12x_csi2tx.h"
#include "rkx12x_dvptx.h"

#include "hal/cru_api.h"
#include "hal/pinctrl_api.h"

#endif /* __RKX12X_API_H__ */
