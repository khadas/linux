/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __BL40_MODULE_H__
#define __BL40_MODULE_H__

#ifdef CONFIG_AMLOGIC_FIRMWARE
void bl40_rx_msg(void *msg, int size);
#else
static inline void bl40_rx_msg(void *msg, int size)
{
}
#endif

#endif /*__BL40_MODULE_H__*/
