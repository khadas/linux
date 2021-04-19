/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __AML_HDMIRX_ARC_IOMAP_H__
#define __AML_HDMIRX_ARC_IOMAP_H__

void hdmirx_arc_write_reg(int reg, int value);
void hdmirx_arc_update_reg(int reg, int mask, int value);

#endif
