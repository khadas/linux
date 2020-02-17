/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef AMPORTS_CONFIG_HHH
#define AMPORTS_CONFIG_HHH
#include <linux/kconfig.h>
#include <linux/amlogic/media/old_cpu_version.h>

/*
 *value seem:
 *arch\arm\plat-meson\include\plat\cpu.h
 */
#define HAS_VPU_PROT 0

/*
 *has vpu prot Later than m8;
 *except g9tv,mtvd,m8b.
 */
static inline bool has_vpu_prot(void)
{
	if (is_meson_g9tv_cpu() || is_meson_mtvd_cpu() || is_meson_m8b_cpu())
		return 0;
	else if (get_cpu_type() >= MESON_CPU_MAJOR_ID_M8)
		return 1;
	return 0;
}

/*
 *only mtvd,m8 has vdec2;
 *others all don't have it.
 */
static inline bool has_vdec2(void)
{
	/*if (is_meson_mtvd_cpu() || is_meson_m8_cpu())*/
	if (is_meson_tl1_cpu() || is_meson_m8b_cpu())
		return 1;
	return 0;
}

static inline bool has_hevc_vdec(void)
{
/*#ifndef CONFIG_AM_VDEC_H265
 *	return 0;
 *#endif
 */
	/*only tvd not have hevc,when later than m8 */
	if (is_meson_mtvd_cpu())
		return 0;
	else if (get_cpu_type() > MESON_CPU_MAJOR_ID_M8)
		return 1;
	return 0;
}

static inline bool has_hdec(void)
{
	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_M8)
		return 1;
	return 0;
}

#endif /* AMPORTS_CONFIG_HHH */
