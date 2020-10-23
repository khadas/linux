/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 *
 * Copyright (c) 2018 Amlogic, inc.
 *
 */

#ifndef __CLK_SECURE_H
#define __CLK_SECURE_H

/*for sc2 secure*/
#define CLK_SECURE_RW		0x8200009A
#define CPUCLK_SECURE_RW	0x8200009B

#define CLK_SMC_READ 0
#define CLK_SMC_WRITE 1

#define SYS_PLL_STEP0 0
#define SYS_PLL_STEP1 1
#define SYS_PLL_DISABLE 2
#define GP1_PLL_STEP0 3
#define GP1_PLL_STEP1 4
#define GP1_PLL_DISABLE 5
#define CLK_REG_RW 6

#define CLK_CPU_REG_RW 0
#define CLK_DSU_REG_RW 1
#define CPU_CLK_SET_PARENT 2
#define DSU_CLK_SET_PARENT 3
#define CPU_CLK_SET_RATE 4
#define DSU_CLK_SET_RATE 5
#define CLK_CPU_DSU_REG_RW 6
#define SET_CPU0_MUX_PARENT 7
#define SET_CPU123_DSU_MUX_PARENT 8
#define SET_DSU_PRE_MUX_PARENT 9
#define DSU_DIVIDER_SET_RATE 10
#define CPU_DIVIDER_SET_RATE 11

extern const struct clk_ops meson_secure_clk_pll_ops; /* for sc2 secure pll*/
extern const struct clk_ops clk_regmap_secure_divider_ops;
extern const struct clk_ops clk_regmap_secure_divider_ro_ops;
extern const struct clk_ops clk_regmap_secure_mux_ops;
extern const struct clk_ops clk_regmap_secure_mux_ro_ops;
extern const struct clk_ops meson_secure_clk_cpu_dyndiv_ops;

#endif /* __CLK_SECURE_H */
