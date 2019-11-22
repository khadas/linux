/*
 * drivers/amlogic/drm/vpu-hw/meson_vpu_dev.c
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
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
