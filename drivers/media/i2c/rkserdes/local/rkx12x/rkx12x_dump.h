/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 Rockchip Electronics Co. Ltd.
 *
 */

#ifndef __RKX12X_DUMP_H__
#define __RKX12X_DUMP_H__

enum {
	RKX12X_DUMP_GRF = 0,
	RKX12X_DUMP_LINKRX = 1,
	RKX12X_DUMP_GRF_MIPI0 = 10,
	RKX12X_DUMP_GRF_MIPI1 = 11,
	RKX12X_DUMP_TXPHY0 = 20,
	RKX12X_DUMP_TXPHY1 = 21,
	RKX12X_DUMP_CSITX0 = 30,
	RKX12X_DUMP_CSITX1 = 31,
	RKX12X_DUMP_DVPTX = 40,
};

int rkx12x_module_dump(struct rkx12x *rkx12x, u32 module_id);

#endif /* __RKX12X_DUMP_H__ */
