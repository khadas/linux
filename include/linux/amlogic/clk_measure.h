/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __CLK_MEASURE_H__
#define __CLK_MEASURE_H__

int meson_clk_measure(unsigned int id);
int meson_clk_measure_with_precision(unsigned int id, unsigned int precision);

#endif
