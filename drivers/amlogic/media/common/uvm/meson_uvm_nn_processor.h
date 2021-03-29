/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/amlogic/meson_uvm_core.h>

struct meson_nn_mod_ops {
	const struct nn_private_data *(*nn_mod_setinfo)(void);
	const struct nn_private_data *(*nn_mod_getinfo)(void);
	/* TODO */
};

struct nn_private_data {
	/* TODO */
};

struct uvm_hook_mod_info *get_nn_hook_mod_info(void);
struct nn_private_data *nn_mod_getinfo(void);
struct nn_private_data *nn_mod_setinfo(void);
