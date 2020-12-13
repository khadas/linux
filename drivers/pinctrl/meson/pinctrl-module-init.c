// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifdef MODULE

#include <linux/platform_device.h>
#include <linux/module.h>
#include "pinctrl-module-init.h"

#define int_func(chip) meson_##chip##_pinctrl_init()
#define load_pinctrl_module(chip)				\
{								\
	int ret = 0;						\
	ret = int_func(chip);					\
	pr_info("call %s pinctrl init ret=%d\n", #chip, ret);	\
}

static int __init pinctrl_module_init(void)
{
	pr_info("### %s() start\n", __func__);

	/* add more here */
	load_pinctrl_module(g12a);
	load_pinctrl_module(tm2);
	load_pinctrl_module(sc2);
	load_pinctrl_module(t5d);
	load_pinctrl_module(t7);
	load_pinctrl_module(s4);

	pr_info("### %s() end\n", __func__);

	return 0;
}

static void __exit pinctrl_module_exit(void)
{
	pr_info("%s()\n", __func__);
}

module_init(pinctrl_module_init);
module_exit(pinctrl_module_exit);

MODULE_LICENSE("GPL v2");
#endif
