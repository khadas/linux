/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 Rockchip Electronics Co. Ltd.
 *
 */

#ifndef __RKX11X_DUMP_H__
#define __RKX11X_DUMP_H__

enum {
	RKX11X_DUMP_GRF = 0,
	RKX11X_DUMP_LINKTX = 1,
	RKX11X_DUMP_GRF_MIPI0 = 10,
	RKX11X_DUMP_GRF_MIPI1 = 11,
	RKX11X_DUMP_RXPHY0 = 20,
	RKX11X_DUMP_RXPHY1 = 21,
	RKX11X_DUMP_CSI2HOST0 = 30,
	RKX11X_DUMP_CSI2HOST1 = 31,
	RKX11X_DUMP_VICAP_CSI = 40,
	RKX11X_DUMP_VICAP_DVP = 50,
};

int rkx11x_module_dump(struct rkx11x *rkx11x, u32 module_id);

#endif /* __RKX11X_DUMP_H__ */
