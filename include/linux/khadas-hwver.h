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
#define HW_VERSION_VIM1S_V10         0x10


int get_hwver(void);
#endif
