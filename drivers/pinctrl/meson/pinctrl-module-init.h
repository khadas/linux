/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _PINCTRL_MODULE_H__
#define _PINCTRL_MODULE_H__

#ifdef MODULE

#ifdef CONFIG_PINCTRL_MESON_G12A
int meson_g12a_pinctrl_init(void);
#else
static inline int meson_g12a_pinctrl_init(void)
{
	return 0;
}
#endif

#ifdef CONFIG_PINCTRL_MESON_TM2
int meson_tm2_pinctrl_init(void);
#else
static inline int meson_tm2_pinctrl_init(void)
{
	return 0;
}
#endif

#ifdef CONFIG_PINCTRL_MESON_SC2
int meson_sc2_pinctrl_init(void);
#else
static inline int meson_sc2_pinctrl_init(void)
{
	return 0;
}
#endif

#ifdef CONFIG_PINCTRL_MESON_T5D
int meson_t5d_pinctrl_init(void);
#else
static inline int meson_t5d_pinctrl_init(void)
{
	return 0;
}
#endif

#ifdef CONFIG_PINCTRL_MESON_T7
int meson_t7_pinctrl_init(void);
#else
static inline int meson_t7_pinctrl_init(void)
{
	return 0;
}
#endif

#ifdef CONFIG_PINCTRL_MESON_S4
int meson_s4_pinctrl_init(void);
#else
static inline int meson_s4_pinctrl_init(void)
{
	return 0;
}
#endif

#endif /* end of ifdef MODULE */
#endif /* end of _PINCTRL_MODULE_H__ */

