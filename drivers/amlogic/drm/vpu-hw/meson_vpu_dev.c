// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

struct vpu_dev_func {
	void (*hw_init)();
	void (*hw_exit)();
};

static struct vpu_dev_func g12a = {
	.hw_init = g12a_hw_init,
	.hw_exit = g12a_hw_exit,
};

static struct vpu_dev_func g12b = {
	.hw_init = g12b_hw_init,
	.hw_exit = g12b_hw_exit,
};

static struct vpu_dev_func tl1 = {
	.hw_init = tl1_hw_init,
	.hw_exit = tl1_hw_exit,
};
