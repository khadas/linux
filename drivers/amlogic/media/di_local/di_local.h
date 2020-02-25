/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __DI_LOCAL_H__
#define __DI_LOCAL_H__

struct di_ext_ops {
	unsigned int (*di_post_reg_rd)(unsigned int addr);
	int (*di_post_wr_reg_bits)(u32 adr, u32 val, u32 start, u32 len);
	void (*post_update_mc)(void);
};

#endif	/*__DI_LOCAL_H__*/
