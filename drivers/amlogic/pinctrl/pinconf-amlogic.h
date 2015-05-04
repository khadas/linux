/*
 * drivers/amlogic/pinctrl/pinconf-amlogic.h
 *
 * Copyright (C) 2015 Amlogic, Inc. All rights reserved.
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

#ifndef __LINUX_PINCTRL_AMLOGIC_H
#define __LINUX_PINCTRL_AMLOGIC_H
enum amlogic_pinconf_param {
	AML_PCON_ENOUT = 1,
	AML_PCON_PULLUP = 2,
	AML_PCON_PULLUP_SHEFFT = 4,
	AML_ENOUT_VALUE_SHIFFT = 8,
	AML_PCON_ENOUT_SHIFFT = 12,
	AML_PCON_ENPULLUP_SHIFFT = 16,
};

struct cfg_param {
	const char *property;
	enum amlogic_pinconf_param param;
};
#define AML_PINCONF_PACK_PULL(_param_, _arg_) (((_param_) << AML_PCON_PULLUP_SHEFFT) | (_arg_))
#define AML_PINCONF_PACK_ENOUT(_param_, _arg_) (((_param_) << AML_PCON_ENOUT_SHIFFT) | ((_arg_)<<AML_ENOUT_VALUE_SHIFFT))
#define AML_PINCONF_PACK_PULLEN(_param_, _arg_) (((_param_) << AML_PCON_PULLUP_SHEFFT)|((_arg_) << AML_PCON_ENPULLUP_SHIFFT))

#define AML_PINCONF_UNPACK_PULL_PARA(_conf_) (((_conf_) >> AML_PCON_PULLUP_SHEFFT)&0xf)
#define AML_PINCONF_UNPACK_PULL_ARG(_conf_) ((_conf_) & 0xf)
#define AML_PINCONF_UNPACK_PULL_EN(_conf_) (((_conf_) >> AML_PCON_ENPULLUP_SHIFFT) & 0xf)

#define AML_PINCONF_UNPACK_ENOUT_PARA(_conf_) (((_conf_) >> AML_PCON_ENOUT_SHIFFT)&0xf)
#define AML_PINCONF_UNPACK_ENOUT_ARG(_conf_) (((_conf_)>>AML_ENOUT_VALUE_SHIFFT)&0xf)
#endif
