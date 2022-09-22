// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include "vpp_common.h"

int vpp_check_range(int val, int down, int up)
{
	int ret = 0;

	if (val < down)
		ret = down;
	else if (val > up)
		ret = up;
	else
		ret = val;

	return ret;
}

int vpp_mask_int(int val, int start, int len)
{
	int ret = 0xffffffff;
	int tmp = sizeof(int) - len;

	if (tmp < 0)
		tmp = 0;

	ret = ((val >> start) << tmp) >> tmp;

	return ret;
}

int vpp_insert_int(int src_val, int insert_val,
	int start, int len)
{
	int ret = 0xffffffff;
	int tmp = sizeof(int) - len;
	int mask;

	if (tmp < 0)
		tmp = 0;

	mask = ~(((ret >> start) << tmp) >> (tmp - start));
	tmp = insert_val << start;
	ret = (src_val & mask) | tmp;

	return ret;
}

