/*
 * include/linux/amlogic/vout/aml_bl.h
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

#ifndef __INCLUDE_AML_BL_H
#define __INCLUDE_AML_BL_H



struct bl_platform_data {
	void (*bl_init)(void);
	void (*power_on_bl)(void);
	void (*power_off_bl)(void);
	unsigned (*get_bl_level)(void);
	void (*set_bl_level)(unsigned);
	int max_brightness;
	int dft_brightness;
};


#endif

