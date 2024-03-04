/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2024 Rockchip Electronics Co. Ltd.
 *
 * Author: Steven Liu <steven.liu@rock-chips.com>
 */

#ifndef __RKX12X_PINCTRL_CORE_H__
#define __RKX12X_PINCTRL_CORE_H__

#include "hal_def.h"
#include "hal_os_def.h"

typedef enum {
	PIN_UNDEF,
	PIN_RKX120,
	PIN_MAX
} HAL_PinType;

struct xferpin {
	HAL_PinType type;
	char name[16]; /* slave addr is expected */
	void *client;
	HAL_RegRead_t *read;
	HAL_RegWrite_t *write;
};

struct hwpin {
	char name[32];
	HAL_PinType type;
	struct xferpin xfer;
};

enum {
	RKX12X_GPIO_PA0 = 0,
	RKX12X_GPIO_PA1,
	RKX12X_GPIO_PA2,
	RKX12X_GPIO_PA3,
	RKX12X_GPIO_PA4,
	RKX12X_GPIO_PA5,
	RKX12X_GPIO_PA6,
	RKX12X_GPIO_PA7,
	RKX12X_GPIO_PB0 = 8,
	RKX12X_GPIO_PB1,
	RKX12X_GPIO_PB2,
	RKX12X_GPIO_PB3,
	RKX12X_GPIO_PB4,
	RKX12X_GPIO_PB5,
	RKX12X_GPIO_PB6,
	RKX12X_GPIO_PB7,
	RKX12X_GPIO_PC0 = 16,
	RKX12X_GPIO_PC1,
	RKX12X_GPIO_PC2,
	RKX12X_GPIO_PC3,
	RKX12X_GPIO_PC4,
	RKX12X_GPIO_PC5,
	RKX12X_GPIO_PC6,
	RKX12X_GPIO_PC7,
	RKX12X_GPIO_PD0 = 24,
	RKX12X_GPIO_PD1,
	RKX12X_GPIO_PD2,
	RKX12X_GPIO_PD3,
	RKX12X_GPIO_PD4,
	RKX12X_GPIO_PD5,
	RKX12X_GPIO_PD6,
	RKX12X_GPIO_PD7,
	RKX12X_GPIO_PIN_MAX
};

HAL_Status RKX12x_HAL_PINCTRL_SetParam(struct hwpin *hw,
				uint32_t bank, uint32_t mPins, uint32_t param);
HAL_Status RKX12x_HAL_PINCTRL_Register(struct hwpin *hw, struct xferpin *xfer);

#endif /* __RKX12X_PINCTRL_CORE_H__ */
