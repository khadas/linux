/*
 * khadas hardware version detete support
 *
 * Copyright (C) 2019 Khadas
 *
 * Author: Terry terry@khadas.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __KHADAS_HWVER_H__
#define __KHADAS_HWVER_H__

#define HW_VERSION_UNKNOW            0x00
#define HW_VERSION_VIM1_V12          0x12
#define HW_VERSION_VIM1_V13          0x13
#define HW_VERSION_VIM2_V12          0x22
#define HW_VERSION_VIM2_V14          0x24
#define HW_VERSION_VIM3_V11          0x31
#define HW_VERSION_VIM3_V12          0x32


int get_hwver(void);
#endif
