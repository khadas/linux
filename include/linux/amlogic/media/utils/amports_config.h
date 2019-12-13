/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef AMPORTS_CONFIG_HHH
#define AMPORTS_CONFIG_HHH
#include <linux/kconfig.h>
#include <linux/amlogic/cpu_version.h>

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
	return 1;
}

/*
 *only mtvd,m8 has vdec2;
 *others all don't have it.
 */
static inline bool has_vdec2(void)
{
	if (is_meson_mtvd_cpu() || is_meson_m8_cpu())
		return 1;
	return 0;
}

static inline bool has_hevc_vdec(void)
{
	/*only tvd not have hevc,when later than m8 */
	return 1;
}

static inline bool has_hdec(void)
{
	return 1;
}

#endif /* AMPORTS_CONFIG_HHH */
