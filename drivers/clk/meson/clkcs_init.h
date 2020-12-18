/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _CLK_MODULE_H__
#define _CLK_MODULE_H__

#ifdef MODULE

#ifdef CONFIG_COMMON_CLK_G12A
int meson_g12a_clkc_init(void);
int meson_g12a_aoclkc_init(void);
#else
static inline int meson_g12a_clkc_init(void)
{
	return 0;
}

static inline int meson_g12a_aoclkc_init(void)
{
	return 0;
}
#endif

#ifdef CONFIG_COMMON_CLK_TM2
int meson_tm2_clkc_init(void);
int meson_tm2_aoclkc_init(void);
#else
static inline int meson_tm2_clkc_init(void)
{
	return 0;
}

static inline int meson_tm2_aoclkc_init(void)
{
	return 0;
}
#endif

#ifdef CONFIG_COMMON_CLK_SC2
int meson_sc2_clkc_init(void);
#else
static inline int meson_sc2_clkc_init(void)
{
	return 0;
}
#endif

#ifdef CONFIG_COMMON_CLK_T5D
int meson_t5d_clkc_init(void);
int meson_t5d_periph_clkc_init(void);
int meson_t5d_aoclkc_init(void);
#else
static inline int meson_t5d_clkc_init(void)
{
	return 0;
}

static inline int meson_t5d_aoclkc_init(void)
{
	return 0;
}

static inline int meson_t5d_periph_clkc_init(void)
{
	return 0;
}
#endif

#endif /* end of ifdef MODULE */
#endif /* end of _CLK_MODULE_H__ */
