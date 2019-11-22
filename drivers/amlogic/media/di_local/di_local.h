/*
 * drivers/amlogic/media/di_local/di_local.h
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
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

#ifndef __DI_LOCAL_H__
#define __DI_LOCAL_H__

struct di_ext_ops {
	unsigned int (*di_post_reg_rd)(unsigned int addr);
	int (*di_post_wr_reg_bits)(u32 adr, u32 val, u32 start, u32 len);
	void (*post_update_mc)(void);
};

#endif	/*__DI_LOCAL_H__*/
