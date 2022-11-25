/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * drivers/amlogic/media/di_multi/di_data.h
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

#ifndef __DI_DATA_H__
#define __DI_DATA_H__

/**********************************************************
 * fifo for uchar
 *
 **********************************************************/
#define UFI256_ALLOC(fifo)	\
kfifo_alloc(fifo,		\
	    256,		\
	    GFP_KERNEL)

#define UFI128_ALLOC(fifo)	\
kfifo_alloc(fifo,		\
	    128,		\
	    GFP_KERNEL)

#define UFI64_ALLOC(fifo)	\
kfifo_alloc(fifo,		\
	    64,			\
	    GFP_KERNEL)

#define UFI32_ALLOC(fifo)	\
kfifo_alloc(fifo,		\
	    32,			\
	    GFP_KERNEL)

#define UFI16_ALLOC(fifo)	\
kfifo_alloc(fifo,		\
	    16,			\
	    GFP_KERNEL)

#define UFI8_ALLOC(fifo)	\
kfifo_alloc(fifo,		\
	    8,			\
	    GFP_KERNEL)

#endif	/*__DI_DATA_H__*/
