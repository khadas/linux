/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __EARC_H__
#define __EARC_H__
/* earc probe is at arch_initcall stage which is earlier to normal driver */
bool is_earc_spdif(void);
bool get_earcrx_chnum_mult_mode(void);

#endif

