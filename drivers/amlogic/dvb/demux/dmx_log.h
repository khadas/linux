/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _DMX_LOG_H_
#define _DMX_LOG_H_

#define LOG_ERROR      0
#define LOG_DBG        1
#define LOG_VER        2

#define dprintk(level, debug, x...)\
	do {\
		if ((level) <= (debug)) \
			printk(x);\
	} while (0)
#endif
