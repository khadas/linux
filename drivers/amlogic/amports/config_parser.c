/*
 * drivers/amlogic/amports/config_parser.c
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
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/errno.h>

#include "config_parser.h"
/*
sample config:
configs: width:1920;height:1080;
need:width
ok: return 0;
*val = value;
*/
int get_config_int(const char *configs, const char *need, int *val)
{
	const char *str;
	int ret;
	int lval = 0;
	*val = 0;

	if (!configs || !need)
		return -1;
	str = strstr(configs, need);
	if (str != NULL) {
		if (str > configs && str[-1] != ';') {
			/*
			   if not the first config val.
			   make sure before is ';'
			   to recognize:
			   ;crop_width:100
			   ;width:100
			 */
			return -2;
		}
		str += strlen(need);
		if (str[0] != ':' || str[1] == '\0')
			return -3;
		ret = sscanf(str, ":%d", &lval);
		if (ret == 1) {
			*val = lval;
			return 0;
		}
	}

	return -4;
}
