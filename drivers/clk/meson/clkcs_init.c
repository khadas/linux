// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifdef MODULE

#include <linux/platform_device.h>
#include <linux/module.h>
#include "clkcs_init.h"

static int __init clkcs_module_init(void)
{
	/* add more here */
	meson_g12a_clkc_init();
	meson_g12a_aoclkc_init();

	meson_tm2_clkc_init();
	meson_tm2_aoclkc_init();

	meson_sc2_clkc_init();

	meson_t5d_clkc_init();
	meson_t5d_periph_clkc_init();
	meson_t5d_aoclkc_init();

	return 0;
}

static void __exit clkcs_module_exit(void)
{
}

module_init(clkcs_module_init);
module_exit(clkcs_module_exit);

MODULE_LICENSE("GPL v2");
#endif
