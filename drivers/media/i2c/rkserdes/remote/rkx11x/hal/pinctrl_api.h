/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2024 Rockchip Electronics Co. Ltd.
 *
 * Author: Steven Liu <steven.liu@rock-chips.com>
 */

#ifndef __RKX11X_PINCTRL_API_H__
#define __RKX11X_PINCTRL_API_H__

#include "hal_def.h"
#include "hal_os_def.h"
#include "pinctrl_def.h"
#include "pinctrl_rkx110.h"

static inline int rkx11x_hwpin_register(struct hwpin *hw, struct xferpin *xfer)
{
	return RKX11x_HAL_PINCTRL_Register(hw, xfer);
}

static inline int rkx11x_hwpin_set(struct hwpin *hw,
				uint32_t bank, uint32_t mpins, uint32_t param)
{
	return RKX11x_HAL_PINCTRL_SetParam(hw, bank, mpins, param);
}

#endif /* __RKX11X_PINCTRL_API_H__ */
