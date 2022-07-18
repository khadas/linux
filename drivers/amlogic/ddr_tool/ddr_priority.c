// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/arm-smccc.h>
#include <linux/slab.h>
#include <linux/io.h>
#include "ddr_port.h"
#include "ddr_bandwidth.h"

static struct ddr_priority ddr_priority_s4[] __initdata = {
	{ .port_id = 0, .reg_base = 0xfe036000,
		.reg_mode = 0, .reg_config = 0,
		.w_offset = (0x80 << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x80 << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 1, .reg_base = 0xfe036000,
		.reg_mode = 0, .reg_config = 0,
		.w_offset = (0x84 << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x84 << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 3, .reg_base = 0xfe036000,
		.reg_mode = 0, .reg_config = 0,
		.w_offset = (0x8c << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x8c << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 4, .reg_base = 0xfe036000,
		.reg_mode = 0, .reg_config = 0,
		.w_offset = (0x90 << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x90 << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 5, .reg_base = 0,
		.reg_mode = 0, .reg_config = 0,
		.w_offset = 0, .w_bit_s = 0, .w_width = 0,
		.r_offset = 0, .r_bit_s = 0, .r_width = 0		},

	{ .port_id = 6, .reg_base = 0xfe036000,
		.reg_mode = 0, .reg_config = 0,
		.w_offset = (0x98 << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x98 << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 7, .reg_base = 0xfe036000,
		.reg_mode = 0, .reg_config = 0,
		.w_offset = (0x9c << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x9c << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 11, .reg_base = 0xfe036000,
		.reg_mode = 0, .reg_config = 0,
		.w_offset = (0xac << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0xac << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 16, .reg_base = 0xfe036000,
		.reg_mode = 0, .reg_config = 0,
		.w_offset = (0x60 << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x60 << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 17, .reg_base = 0xfe036000,
		.reg_mode = 0, .reg_config = 0,
		.w_offset = (0x64 << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x64 << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 18, .reg_base = 0xfe036000,
		.reg_mode = 0, .reg_config = 0,
		.w_offset = (0x68 << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x68 << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 19, .reg_base = 0xfe036000,
		.reg_mode = 0, .reg_config = 0,
		.w_offset = (0x6c << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x6c << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 20, .reg_base = 0xfe036000,
		.reg_mode = 0, .reg_config = 0,
		.w_offset = (0x70 << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x70 << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 21, .reg_base = 0xfe036000,
		.reg_mode = 0, .reg_config = 0,
		.w_offset = (0x74 << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x74 << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 23, .reg_base = 0xfe036000,
		.reg_mode = 0, .reg_config = 0,
		.w_offset = (0x7c << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x7c << 2), .r_bit_s = 16, .r_width = 0x7	}
};

static struct ddr_priority ddr_priority_t7[] __initdata = {
	{ .port_id = 1, .reg_base = 0xfe704100,
		.reg_mode = 1, .reg_config = 0,
		.w_offset = 0, .w_bit_s = 0, .w_width = 0xf,
		.r_offset = 0, .r_bit_s = 0, .r_width = 0xf	},

	{ .port_id = 2, .reg_base = 0xfe703100,
		.reg_mode = 1, .reg_config = 0,
		.w_offset = 0, .w_bit_s = 0, .w_width = 0xf,
		.r_offset = 0, .r_bit_s = 0, .r_width = 0xf	},

	{ .port_id = 3, .reg_base = 0xfe701100,
		.reg_mode = 1, .reg_config = 0,
		.w_offset = 0, .w_bit_s = 0, .w_width = 0xf,
		.r_offset = 0, .r_bit_s = 0, .r_width = 0xf	},

	{ .port_id = 4, .reg_base = 0xfe702100,
		.reg_mode = 1, .reg_config = 0,
		.w_offset = 0, .w_bit_s = 0, .w_width = 0xf,
		.r_offset = 0, .r_bit_s = 0, .r_width = 0xf	},

	//{ .port_id = 8, .reg_base = 0xfe374014,
	//	.reg_mode = 1, .reg_config = 0x1000000,
	//	.w_offset = 0, .w_bit_s = 28, .w_width = 0xf,
	//	.r_offset = 0, .r_bit_s = 28, .r_width = 0xf	},

	//{ .port_id = 9, .reg_base = 0xfe374014,
	//	.reg_mode = 1, .reg_config = 0x10000,
	//	.w_offset = 0, .w_bit_s = 20, .w_width = 0xf,
	//	.r_offset = 0, .r_bit_s = 20, .r_width = 0xf	},

	//{ .port_id = 10, .reg_base = 0xfe374014,
	//	.reg_mode = 1, .reg_config = 0x100,
	//	.w_offset = 0, .w_bit_s = 12, .w_width = 0xf,
	//	.r_offset = 0, .r_bit_s = 12, .r_width = 0xf	},

	//{ .port_id = 11, .reg_base = 0xfe374014,
	//	.reg_mode = 1, .reg_config = 0x1,
	//	.w_offset = 0, .w_bit_s = 4, .w_width = 0xf,
	//	.r_offset = 0, .r_bit_s = 4, .r_width = 0xf	},

	{ .port_id = 16, .reg_base = 0xfe0104d4,
		.reg_mode = 0, .reg_config = 0,
		.w_offset = 0, .w_bit_s = 0, .w_width = 0xf,
		.r_offset = 0, .r_bit_s = 4, .r_width = 0xf	},

	{ .port_id = 17, .reg_base = 0xfe0104d8,
		.reg_mode = 0, .reg_config = 0,
		.w_offset = 0, .w_bit_s = 0, .w_width = 0xf,
		.r_offset = 0, .r_bit_s = 4, .r_width = 0xf	},

	{ .port_id = 18, .reg_base = 0xfe0104d8,
		.reg_mode = 0, .reg_config = 0,
		.w_offset = 0, .w_bit_s = 8, .w_width = 0xf,
		.r_offset = 0, .r_bit_s = 12, .r_width = 0xf	},

	{ .port_id = 19, .reg_base = 0xfe0104d4,
		.reg_mode = 0, .reg_config = 0,
		.w_offset = 0, .w_bit_s = 24, .w_width = 0xf,
		.r_offset = 0, .r_bit_s = 28, .r_width = 0xf	},

	{ .port_id = 24, .reg_base = 0xfe0104d4,
		.reg_mode = 0, .reg_config = 0,
		.w_offset = 0, .w_bit_s = 8, .w_width = 0x3,
		.r_offset = 0, .r_bit_s = 12, .r_width = 0x3	},

	{ .port_id = 25, .reg_base = 0xfe0104d4,
		.reg_mode = 0, .reg_config = 0,
		.w_offset = 0, .w_bit_s = 8, .w_width = 0x3,
		.r_offset = 0, .r_bit_s = 12, .r_width = 0x3	},

	{ .port_id = 26, .reg_base = 0xfe0104d4,
		.reg_mode = 0, .reg_config = 0,
		.w_offset = 0, .w_bit_s = 8, .w_width = 0x3,
		.r_offset = 0, .r_bit_s = 12, .r_width = 0x3	},

	{ .port_id = 27, .reg_base = 0xfe0104d4,
		.reg_mode = 0, .reg_config = 0,
		.w_offset = 0, .w_bit_s = 8, .w_width = 0x3,
		.r_offset = 0, .r_bit_s = 12, .r_width = 0x3	},

	{ .port_id = 28, .reg_base = 0xfe0104d4,
		.reg_mode = 0, .reg_config = 0,
		.w_offset = 0, .w_bit_s = 8, .w_width = 0x3,
		.r_offset = 0, .r_bit_s = 12, .r_width = 0x3	},

	{ .port_id = 29, .reg_base = 0xfe0104d4,
		.reg_mode = 0, .reg_config = 0,
		.w_offset = 0, .w_bit_s = 8, .w_width = 0x3,
		.r_offset = 0, .r_bit_s = 12, .r_width = 0x3	},

	{ .port_id = 34, .reg_base = 0xfe0104d4,
		.reg_mode = 0, .reg_config = 0,
		.w_offset = 0, .w_bit_s = 16, .w_width = 0x3,
		.r_offset = 0, .r_bit_s = 20, .r_width = 0x3	},

	{ .port_id = 35, .reg_base = 0xfe0104d4,
		.reg_mode = 0, .reg_config = 0,
		.w_offset = 0, .w_bit_s = 16, .w_width = 0x3,
		.r_offset = 0, .r_bit_s = 20, .r_width = 0x3	},

	{ .port_id = 36, .reg_base = 0xfe0104d4,
		.reg_mode = 0, .reg_config = 0,
		.w_offset = 0, .w_bit_s = 16, .w_width = 0x3,
		.r_offset = 0, .r_bit_s = 20, .r_width = 0x3	},

	{ .port_id = 37, .reg_base = 0xfe0104d4,
		.reg_mode = 0, .reg_config = 0,
		.w_offset = 0, .w_bit_s = 16, .w_width = 0x3,
		.r_offset = 0, .r_bit_s = 20, .r_width = 0x3	},

	{ .port_id = 38, .reg_base = 0xfe0104d4,
		.reg_mode = 0, .reg_config = 0,
		.w_offset = 0, .w_bit_s = 16, .w_width = 0x3,
		.r_offset = 0, .r_bit_s = 20, .r_width = 0x3	},

	{ .port_id = 39, .reg_base = 0xfe0104d4,
		.reg_mode = 0, .reg_config = 0,
		.w_offset = 0, .w_bit_s = 16, .w_width = 0x3,
		.r_offset = 0, .r_bit_s = 20, .r_width = 0x3	},

	{ .port_id = 48, .reg_base = 0xfe0104dc,
		.reg_mode = 0, .reg_config = 0,
		.w_offset = 0, .w_bit_s = 0, .w_width = 0xf,
		.r_offset = 0, .r_bit_s = 4, .r_width = 0xf	},

	{ .port_id = 49, .reg_base = 0xfe0104d8,
		.reg_mode = 0, .reg_config = 0,
		.w_offset = 0, .w_bit_s = 24, .w_width = 0xf,
		.r_offset = 0, .r_bit_s = 28, .r_width = 0xf	},

	{ .port_id = 50, .reg_base = 0xfe0104d8,
		.reg_mode = 0, .reg_config = 0,
		.w_offset = 0, .w_bit_s = 16, .w_width = 0x7,
		.r_offset = 0, .r_bit_s = 20, .r_width = 0x7	},

	{ .port_id = 51, .reg_base = 0xfe0104e0,
		.reg_mode = 0, .reg_config = 0,
		.w_offset = 0, .w_bit_s = 0, .w_width = 0x7,
		.r_offset = 0, .r_bit_s = 4, .r_width = 0x7	},

	{ .port_id = 52, .reg_base = 0xfe0104dc,
		.reg_mode = 0, .reg_config = 0,
		.w_offset = 0, .w_bit_s = 24, .w_width = 0x3,
		.r_offset = 0, .r_bit_s = 28, .r_width = 0x3	},

	{ .port_id = 53, .reg_base = 0xfe0104dc,
		.reg_mode = 0, .reg_config = 0,
		.w_offset = 0, .w_bit_s = 16, .w_width = 0x3,
		.r_offset = 0, .r_bit_s = 20, .r_width = 0x3	},

	{ .port_id = 54, .reg_base = 0xfe0104dc,
		.reg_mode = 0, .reg_config = 0,
		.w_offset = 0, .w_bit_s = 8, .w_width = 0x3,
		.r_offset = 0, .r_bit_s = 12, .r_width = 0x3	},

	{ .port_id = 55, .reg_base = 0xfe0104e0,
		.reg_mode = 0, .reg_config = 0,
		.w_offset = 0, .w_bit_s = 8, .w_width = 0xf,
		.r_offset = 0, .r_bit_s = 12, .r_width = 0xf	},

	{ .port_id = 63, .reg_base = 0xfe0104e4,
		.reg_mode = 0, .reg_config = 0,
		.w_offset = 0, .w_bit_s = 0, .w_width = 0x3,
		.r_offset = 0, .r_bit_s = 4, .r_width = 0x3	},

	{ .port_id = 64, .reg_base = 0xfe3b3000,
		.reg_mode = 0, .reg_config = 0x10000,
		.w_offset = 0xac4, .w_bit_s = 0, .w_width = 0xf,
		.r_offset = 0xac0, .r_bit_s = 0, .r_width = 0xf},

	{ .port_id = 65, .reg_base = 0,
		.reg_mode = 0, .reg_config = 0x0,
		.w_offset = 0, .w_bit_s = 0, .w_width = 0,
		.r_offset = 0, .r_bit_s = 0, .r_width = 0	},

	{ .port_id = 66, .reg_base = 0xfe3b5000,
		.reg_mode = 0, .reg_config = 0x10000,
		.w_offset = 0xac4, .w_bit_s = 0, .w_width = 0xf,
		.r_offset = 0xac0, .r_bit_s = 0, .r_width = 0xf},

	{ .port_id = 67, .reg_base = 0xfe3b3000,
		.reg_mode = 0, .reg_config = 0x10000,
		.w_offset = 0xec4, .w_bit_s = 0, .w_width = 0xf,
		.r_offset = 0xec0, .r_bit_s = 0, .r_width = 0xf},

	{ .port_id = 68, .reg_base = 0xfe0104e0,
		.reg_mode = 0, .reg_config = 0,
		.w_offset = 0, .w_bit_s = 16, .w_width = 0xf,
		.r_offset = 0, .r_bit_s = 20, .r_width = 0xf	},

	{ .port_id = 69, .reg_base = 0xfe0104e0,
		.reg_mode = 0, .reg_config = 0,
		.w_offset = 0, .w_bit_s = 24, .w_width = 0xf,
		.r_offset = 0, .r_bit_s = 28, .r_width = 0xf	},

	{ .port_id = 72, .reg_base = 0xfe0104e4,
		.reg_mode = 0, .reg_config = 0,
		.w_offset = 0, .w_bit_s = 8, .w_width = 0x3,
		.r_offset = 0, .r_bit_s = 12, .r_width = 0x3	},

	{ .port_id = 73, .reg_base = 0xfe0104e4,
		.reg_mode = 0, .reg_config = 0,
		.w_offset = 0, .w_bit_s = 8, .w_width = 0x3,
		.r_offset = 0, .r_bit_s = 12, .r_width = 0x3	},

	{ .port_id = 80, .reg_base = 0xff009000,
		.reg_mode = 1, .reg_config = 0,
		.w_offset = 0xcc8, .w_bit_s = 0, .w_width = 0xffff,
		.r_offset = 0xcc0, .r_bit_s = 0, .r_width = 0xffff},

	{ .port_id = 81, .reg_base = 0xff009000,
		.reg_mode = 1, .reg_config = 0,
		.w_offset = 0xcc8, .w_bit_s = 16, .w_width = 0xffff,
		.r_offset = 0xcc0, .r_bit_s = 16, .r_width = 0xffff},

	{ .port_id = 82, .reg_base = 0xff009000,
		.reg_mode = 1, .reg_config = 0,
		.w_offset = 0, .w_bit_s = 0, .w_width = 0,
		.r_offset = 0xcc4, .r_bit_s = 0, .r_width = 0xffff}
};

static struct ddr_priority ddr_priority_s5[] __initdata = {
	{ .port_id = 2, .reg_base = 0xfe010000,
		.reg_mode = 0, .reg_config = 0,
		.w_offset = (0x130 << 2), .w_bit_s = 20, .w_width = 0xf0f,
		.r_offset = (0x130 << 2), .r_bit_s = 16, .r_width = 0xf0f	},

	{ .port_id = 4, .reg_base = 0xfe010000,
		.reg_mode = 0, .reg_config = 0,
		.w_offset = (0x130 << 2), .w_bit_s = 12, .w_width = 0xf,
		.r_offset = (0x130 << 2), .r_bit_s = 8, .r_width = 0xf	},

	{ .port_id = 8, .reg_base = 0xfe010000,
		.reg_mode = 0, .reg_config = 0,
		.w_offset = (0x15b << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0x15b << 2), .r_bit_s = 20, .r_width = 0xf	},

	{ .port_id = 9, .reg_base = 0xfe010000,
		.reg_mode = 0, .reg_config = 0,
		.w_offset = (0x15b << 2), .w_bit_s = 0, .w_width = 0xf,
		.r_offset = (0x15b << 2), .r_bit_s = 4, .r_width = 0xf	},

	{ .port_id = 16, .reg_base = 0,
		.reg_mode = 0, .reg_config = 0,
		.w_offset = 0, .w_bit_s = 0, .w_width = 0,
		.r_offset = 0, .r_bit_s = 0, .r_width = 0		},

	{ .port_id = 24, .reg_base = 0xfe010000,
		.reg_mode = 0, .reg_config = 0,
		.w_offset = (0x138 << 2), .w_bit_s = 20, .w_width = 0x3,
		.r_offset = (0x138 << 2), .r_bit_s = 20, .r_width = 0x3	},

	{ .port_id = 25, .reg_base = 0xfe010000,
		.reg_mode = 0, .reg_config = 0,
		.w_offset = (0x138 << 2), .w_bit_s = 20, .w_width = 0x3,
		.r_offset = (0x138 << 2), .r_bit_s = 20, .r_width = 0x3	},

	{ .port_id = 26, .reg_base = 0xfe010000,
		.reg_mode = 0, .reg_config = 0,
		.w_offset = (0x138 << 2), .w_bit_s = 20, .w_width = 0x3,
		.r_offset = (0x138 << 2), .r_bit_s = 20, .r_width = 0x3	},

	{ .port_id = 28, .reg_base = 0xfe010000,
		.reg_mode = 0, .reg_config = 0,
		.w_offset = (0x138 << 2), .w_bit_s = 20, .w_width = 0x3,
		.r_offset = (0x138 << 2), .r_bit_s = 20, .r_width = 0x3	},

	{ .port_id = 29, .reg_base = 0xfe010000,
		.reg_mode = 0, .reg_config = 0,
		.w_offset = (0x138 << 2), .w_bit_s = 20, .w_width = 0x3,
		.r_offset = (0x138 << 2), .r_bit_s = 20, .r_width = 0x3	},

	{ .port_id = 33, .reg_base = 0xfe010000,
		.reg_mode = 0, .reg_config = 0,
		.w_offset = (0x139 << 2), .w_bit_s = 0, .w_width = 0xffff,
		.r_offset = (0x139 << 2), .r_bit_s = 16, .r_width = 0xffff},

	{ .port_id = 35, .reg_base = 0xfe010000,
		.reg_mode = 0, .reg_config = 0,
		.w_offset = (0x138 << 2), .w_bit_s = 22, .w_width = 0xf,
		.r_offset = (0x138 << 2), .r_bit_s = 22, .r_width = 0xf	},

	{ .port_id = 36, .reg_base = 0xfe010000,
		.reg_mode = 0, .reg_config = 0,
		.w_offset = (0x139 << 2), .w_bit_s = 0, .w_width = 0xf,
		.r_offset = (0x139 << 2), .r_bit_s = 0, .r_width = 0xf	},

	{ .port_id = 37, .reg_base = 0xfe010000,
		.reg_mode = 0, .reg_config = 0,
		.w_offset = (0x139 << 2), .w_bit_s = 4, .w_width = 0xf,
		.r_offset = (0x139 << 2), .r_bit_s = 4, .r_width = 0xf	},

	{ .port_id = 38, .reg_base = 0xfe010000,
		.reg_mode = 0, .reg_config = 0,
		.w_offset = (0x139 << 2), .w_bit_s = 8, .w_width = 0xf,
		.r_offset = (0x139 << 2), .r_bit_s = 8, .r_width = 0xf	},

	{ .port_id = 54, .reg_base = 0xfe010000,
		.reg_mode = 0, .reg_config = 0,
		.w_offset = (0x139 << 2), .w_bit_s = 12, .w_width = 0xf,
		.r_offset = (0x139 << 2), .r_bit_s = 12, .r_width = 0xf	},

	{ .port_id = 56, .reg_base = 0xfe010000,
		.reg_mode = 0, .reg_config = 0,
		.w_offset = (0x138 << 2), .w_bit_s = 0, .w_width = 0xf,
		.r_offset = (0x138 << 2), .r_bit_s = 0, .r_width = 0xf	},

	{ .port_id = 57, .reg_base = 0xfe010000,
		.reg_mode = 0, .reg_config = 0,
		.w_offset = (0x138 << 2), .w_bit_s = 4, .w_width = 0xf,
		.r_offset = (0x138 << 2), .r_bit_s = 4, .r_width = 0xf	},

	{ .port_id = 58, .reg_base = 0xfe010000,
		.reg_mode = 0, .reg_config = 0,
		.w_offset = (0x138 << 2), .w_bit_s = 8, .w_width = 0xf,
		.r_offset = (0x138 << 2), .r_bit_s = 8, .r_width = 0xf	},

	{ .port_id = 59, .reg_base = 0xfe010000,
		.reg_mode = 0, .reg_config = 0,
		.w_offset = (0x138 << 2), .w_bit_s = 12, .w_width = 0xf,
		.r_offset = (0x138 << 2), .r_bit_s = 12, .r_width = 0xf	},

	{ .port_id = 60, .reg_base = 0xfe010000,
		.reg_mode = 0, .reg_config = 0,
		.w_offset = (0x138 << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0x138 << 2), .r_bit_s = 16, .r_width = 0xf	},

	{ .port_id = 64, .reg_base = 0xff809000,
		.reg_mode = 0, .reg_config = 0,
		.w_offset = 0xcc8, .w_bit_s = 0, .w_width = 0xffff,
		.r_offset = 0xcc0, .r_bit_s = 0, .r_width = 0xffff},

	{ .port_id = 66, .reg_base = 0xff809000,
		.reg_mode = 0, .reg_config = 0,
		.w_offset = 0xcc8, .w_bit_s = 16, .w_width = 0xffff,
		.r_offset = 0xcc0, .r_bit_s = 16, .r_width = 0xffff},

	{ .port_id = 68, .reg_base = 0xff809000,
		.reg_mode = 0, .reg_config = 0,
		.w_offset = 0, .w_bit_s = 0, .w_width = 0,
		.r_offset = 0xcc4, .r_bit_s = 0, .r_width = 0xffff},

	{ .port_id = 70, .reg_base = 0xff809000,
		.reg_mode = 0, .reg_config = 0,
		.w_offset = 0, .w_bit_s = 0, .w_width = 0,
		.r_offset = 0xcc4, .r_bit_s = 16, .r_width = 0xffff},

	{ .port_id = 72, .reg_base = 0xfe010000,
		.reg_mode = 0, .reg_config = 0,
		.w_offset = (0x158 << 2), .w_bit_s = 9, .w_width = 0x7,
		.r_offset = (0x158 << 2), .r_bit_s = 6, .r_width = 0x7	},

	{ .port_id = 80, .reg_base = 0xfe010000,
		.reg_mode = 0, .reg_config = 0,
		.w_offset = (0x158 << 2), .w_bit_s = 15, .w_width = 0x7,
		.r_offset = (0x158 << 2), .r_bit_s = 12, .r_width = 0x7	},

	{ .port_id = 81, .reg_base = 0xfe010000,
		.reg_mode = 0, .reg_config = 0,
		.w_offset = (0x158 << 2), .w_bit_s = 21, .w_width = 0x7,
		.r_offset = (0x158 << 2), .r_bit_s = 18, .r_width = 0x7	},

	{ .port_id = 88, .reg_base = 0xfe010000,
		.reg_mode = 0, .reg_config = 0,
		.w_offset = (0x158 << 2), .w_bit_s = 27, .w_width = 0x7,
		.r_offset = (0x158 << 2), .r_bit_s = 24, .r_width = 0x7	},

	{ .port_id = 96, .reg_base = 0xfe010000,
		.reg_mode = 0, .reg_config = 0,
		.w_offset = (0x158 << 2), .w_bit_s = 3, .w_width = 0x7,
		.r_offset = (0x158 << 2), .r_bit_s = 0, .r_width = 0x7	},

	{ .port_id = 104, .reg_base = 0xfe010000,
		.reg_mode = 0, .reg_config = 0,
		.w_offset = (0x133 << 2), .w_bit_s = 0, .w_width = 0xf,
		.r_offset = (0x133 << 2), .r_bit_s = 4, .r_width = 0xf	},

	{ .port_id = 106, .reg_base = 0xfe03e000,
		.reg_mode = 0, .reg_config = 0,
		.w_offset = 0x4c4, .w_bit_s = 16, .w_width = 0xffff,
		.r_offset = 0x4c8, .r_bit_s = 0, .r_width = 0xfffff	},

	{ .port_id = 108, .reg_base = 0xfe010000,
		.reg_mode = 0, .reg_config = 0,
		.w_offset = (0x133 << 2), .w_bit_s = 24, .w_width = 0xf,
		.r_offset = (0x133 << 2), .r_bit_s = 28, .r_width = 0xf	},
};

static struct ddr_priority ddr_priority_c3[] __initdata = {
	{ .port_id = 2, .reg_base = 0xffe42000,
		.reg_mode = 1, .reg_config = 0,
		.w_offset = 0x104, .w_bit_s = 0, .w_width = 0xf,
		.r_offset = 0x100, .r_bit_s = 0, .r_width = 0xf		},

	{ .port_id = 8, .reg_base = 0xfe036000,
		.reg_mode = 0, .reg_config = 0,
		.w_offset = (0x8c << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0x8c << 2), .r_bit_s = 16, .r_width = 0xf	},

	{ .port_id = 9, .reg_base = 0xfe036000,
		.reg_mode = 0, .reg_config = 0,
		.w_offset = (0x8c << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0x8c << 2), .r_bit_s = 16, .r_width = 0xf	},

	{ .port_id = 10, .reg_base = 0xfe036000,
		.reg_mode = 0, .reg_config = 0,
		.w_offset = (0x9c << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0x9c << 2), .r_bit_s = 16, .r_width = 0xf	},

	{ .port_id = 11, .reg_base = 0xfe036000,
		.reg_mode = 0, .reg_config = 0,
		.w_offset = (0x90 << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0x90 << 2), .r_bit_s = 16, .r_width = 0xf	},

	{ .port_id = 16, .reg_base = 0xff010800,
		.reg_mode = 1, .reg_config = 0x10000,
		.w_offset = (0xb1 << 2), .w_bit_s = 0, .w_width = 0xffff,
		.r_offset = (0xb0 << 2), .r_bit_s = 0, .r_width = 0xffff},

	{ .port_id = 17, .reg_base = 0xff010000,
		.reg_mode = 1, .reg_config = 0,
		.w_offset = (0x05 << 2), .w_bit_s = 8, .w_width = 0xff,
		.r_offset = (0x05 << 2), .r_bit_s = 0, .r_width = 0xff	},

	{ .port_id = 18, .reg_base = 0xff010000,
		.reg_mode = 1, .reg_config = 0,
		.w_offset = (0x05 << 2), .w_bit_s = 16, .w_width = 0xff,
		.r_offset = 0, .r_bit_s = 0, .r_width = 0		},

	{ .port_id = 19, .reg_base = 0xfe350010,
		.reg_mode = 0, .reg_config = 0,
		.w_offset = 0, .w_bit_s = 0, .w_width = 0,
		.r_offset = 0, .r_bit_s = 4, .r_width = 0xff	},

	{ .port_id = 40, .reg_base = 0xfd342000,
		.reg_mode = 0, .reg_config = 0,
		.w_offset = 0x104, .w_bit_s = 0, .w_width = 0xf,
		.r_offset = 0x100, .r_bit_s = 0, .r_width = 0xf	},

	{ .port_id = 41, .reg_base = 0xfd343000,
		.reg_mode = 0, .reg_config = 0,
		.w_offset = 0x104, .w_bit_s = 0, .w_width = 0xf,
		.r_offset = 0x100, .r_bit_s = 0, .r_width = 0xf	},

	{ .port_id = 42, .reg_base = 0xfd344000,
		.reg_mode = 0, .reg_config = 0,
		.w_offset = 0x104, .w_bit_s = 0, .w_width = 0xf,
		.r_offset = 0x100, .r_bit_s = 0, .r_width = 0xf	},

	{ .port_id = 43, .reg_base = 0xfd346000,
		.reg_mode = 0, .reg_config = 0,
		.w_offset = 0x104, .w_bit_s = 0, .w_width = 0xf,
		.r_offset = 0x100, .r_bit_s = 0, .r_width = 0xf	},

	{ .port_id = 44, .reg_base = 0xfd345000,
		.reg_mode = 0, .reg_config = 0,
		.w_offset = 0x104, .w_bit_s = 0, .w_width = 0xf,
		.r_offset = 0x100, .r_bit_s = 0, .r_width = 0xf	},

	{ .port_id = 48, .reg_base = 0xffe48000,
		.reg_mode = 1, .reg_config = 0,
		.w_offset = 0x104, .w_bit_s = 0, .w_width = 0xf,
		.r_offset = 0x100, .r_bit_s = 0, .r_width = 0xf	},

	{ .port_id = 72, .reg_base = 0xffe44000,
		.reg_mode = 1, .reg_config = 0,
		.w_offset = 0x104, .w_bit_s = 0, .w_width = 0xf,
		.r_offset = 0x100, .r_bit_s = 0, .r_width = 0xf	},

	{ .port_id = 73, .reg_base = 0xffe44000,
		.reg_mode = 1, .reg_config = 0,
		.w_offset = 0x104, .w_bit_s = 0, .w_width = 0xf,
		.r_offset = 0x100, .r_bit_s = 0, .r_width = 0xf	},

	{ .port_id = 74, .reg_base = 0xffe44000,
		.reg_mode = 1, .reg_config = 0,
		.w_offset = 0x104, .w_bit_s = 0, .w_width = 0xf,
		.r_offset = 0x100, .r_bit_s = 0, .r_width = 0xf	},

	{ .port_id = 80, .reg_base = 0xffe47000,
		.reg_mode = 1, .reg_config = 0,
		.w_offset = 0x104, .w_bit_s = 0, .w_width = 0xf,
		.r_offset = 0x100, .r_bit_s = 0, .r_width = 0xf	},

	{ .port_id = 81, .reg_base = 0xffe45000,
		.reg_mode = 1, .reg_config = 0,
		.w_offset = 0x104, .w_bit_s = 0, .w_width = 0xf,
		.r_offset = 0x100, .r_bit_s = 0, .r_width = 0xf	},

	{ .port_id = 82, .reg_base = 0xffe4a000,
		.reg_mode = 1, .reg_config = 0,
		.w_offset = 0x104, .w_bit_s = 0, .w_width = 0xf,
		.r_offset = 0x100, .r_bit_s = 0, .r_width = 0xf	},

	{ .port_id = 83, .reg_base = 0xffe46000,
		.reg_mode = 1, .reg_config = 0,
		.w_offset = 0x104, .w_bit_s = 0, .w_width = 0xf,
		.r_offset = 0x100, .r_bit_s = 0, .r_width = 0xf	},

	{ .port_id = 85, .reg_base = 0xffe49000,
		.reg_mode = 1, .reg_config = 0,
		.w_offset = 0x104, .w_bit_s = 0, .w_width = 0xf,
		.r_offset = 0x100, .r_bit_s = 0, .r_width = 0xf	},
};

static struct ddr_priority *ddr_priority_list;
static unsigned int ddr_priority_list_num __initdata;

int __init ddr_find_port_priority(int cpu_type, struct ddr_priority **desc)
{
	int desc_size = -EINVAL;

	switch (cpu_type) {
	case DMC_TYPE_T7:
		*desc = ddr_priority_t7;
		desc_size = ARRAY_SIZE(ddr_priority_t7);
		break;
	case DMC_TYPE_S4:
		*desc = ddr_priority_s4;
		desc_size = ARRAY_SIZE(ddr_priority_s4);
		break;
	case DMC_TYPE_S5:
		*desc = ddr_priority_s5;
		desc_size = ARRAY_SIZE(ddr_priority_s5);
		break;
	case DMC_TYPE_C3:
		*desc = ddr_priority_c3;
		desc_size = ARRAY_SIZE(ddr_priority_c3);
		break;
	default:
		return -EINVAL;
	}
	/* using once */
	ddr_priority_list = kcalloc(desc_size, sizeof(*ddr_priority_list), GFP_KERNEL);
	if (!ddr_priority_list)
		return -ENOMEM;
	memcpy(ddr_priority_list, *desc, sizeof(struct ddr_priority) * desc_size);
	ddr_priority_list_num = desc_size;
	*desc = ddr_priority_list;

	return desc_size;
}

static int ddr_priority_get_info(unsigned char port_id)
{
	unsigned int i;

	if (!aml_db->ddr_priority_desc) {
		pr_info("ddr priority null\n");
		return -EINVAL;
	}

	for (i = 0; i < aml_db->ddr_priority_num; i++) {
		if (aml_db->ddr_priority_desc[i].port_id == port_id)
			return i;
	}

	return -EINVAL;
}

static int ddr_priority_config(struct ddr_priority info,
				int *priority, unsigned char type)
{
	void __iomem *vaddr = 0;
	unsigned int reg_addr, reg_value;
	unsigned char bit_s;
	unsigned int width;
	int set_priority = 0;

	/* type define
	 * 0x00: read r_priority  0x10: read w_priority
	 * 0x01: write r_priority  0x11: write w_priority
	 */
	if (type & 0x10) {
		if (info.w_width) {
			reg_addr = info.reg_base + info.w_offset;
			bit_s = info.w_bit_s;
			width = info.w_width;
		} else {
			pr_err("port id : %d w_priority not exit\n", info.port_id);
			*priority = -1;
			return -EINVAL;
		}

	} else {
		if (info.r_width) {
			reg_addr = info.reg_base + info.r_offset;
			bit_s = info.r_bit_s;
			width = info.r_width;
		} else {
			pr_err("port id : %d r_priority not exit\n", info.port_id);
			*priority = -1;
			return -EINVAL;
		}
	}

	if (strstr(priority_find_port_name(info.port_id), "ISP")) {
		if (!(aml_db->ddr_priority_num & DDR_PRIORITY_POWER)) {
			*priority = -1;
			return -EINVAL;
		}
	}

	if (aml_db->ddr_priority_num & DDR_PRIORITY_DEBUG)
		pr_info("port %d, name %s, addr %x mode:%d\n",
			info.port_id,
			priority_find_port_name(info.port_id),
			reg_addr,
			info.reg_mode);

	set_priority = width & ((*priority) | (*priority) << 8 |
			(*priority) << 16 | (*priority) << 24);

	if (info.reg_mode) {/* secure mode */
		reg_value = dmc_rw(reg_addr, 0, DMC_READ);
		if (type & 0x01) {
			reg_value &= ~(width << bit_s);
			reg_value |= (set_priority << bit_s);
			reg_value |= info.reg_config;
			dmc_rw(reg_addr, reg_value, DMC_WRITE);
		} else {
			*priority = (reg_value >> bit_s) & 0xf;
		}
	} else {
		vaddr = ioremap(reg_addr, 0x4);
		if (!vaddr) {
			pr_err("reg_addr %x ioremap failed\n", reg_addr);
			return -EINVAL;
		}
		reg_value = readl_relaxed(vaddr);
		if (type & 0x01) {
			reg_value &= ~(width << bit_s);
			reg_value |= (set_priority << bit_s);
			reg_value |= info.reg_config;
			writel_relaxed(reg_value, vaddr);
		} else {
			*priority = (reg_value >> bit_s) & 0xf;
		}
	}

	if (vaddr)
		iounmap(vaddr);

	return 0;
}

int ddr_priority_rw(unsigned char port_id, int *priority_r,
			int *priority_w, unsigned char control)
{
	int index;
	struct ddr_priority info;

	index = ddr_priority_get_info(port_id);
	if (index < 0) {
		pr_err("port_id: %d not exit\n", port_id);
		return -EINVAL;
	}
	info = aml_db->ddr_priority_desc[index];

	switch (control) {
	case 0:
		ddr_priority_config(info, priority_r, 0x00);
		ddr_priority_config(info, priority_w, 0x10);
		break;
	case 1:
		ddr_priority_config(info, priority_r, 0x01);
		ddr_priority_config(info, priority_w, 0x11);
		break;
	case 2:
		ddr_priority_config(info, priority_r, 0x01);
		break;
	case 3:
		ddr_priority_config(info, priority_w, 0x11);
		break;
	default:
		break;
	}

	return 0;
}

struct ddr_port_desc *priority_port_list;
static unsigned int priority_port_list_num;
void __init ddr_priority_port_list(void)
{
	struct ddr_port_desc *desc = NULL;
	int pcnt = 0;

	if (aml_db->cpu_type != DMC_TYPE_C3) {
		priority_port_list = aml_db->port_desc;
		priority_port_list_num = aml_db->real_ports;
	} else {
		pcnt = ddr_find_port_desc(aml_db->cpu_type, &desc);
		if (pcnt < 0) {
			pr_err("can't find priority_port_list, cpu:%d\n", aml_db->cpu_type);
		} else {
			priority_port_list = desc;
			priority_port_list_num = pcnt;
		}
	}
}

char *priority_find_port_name(int id)
{
	int i;

	if (!priority_port_list || !priority_port_list_num)
		return NULL;

	for (i = 0; i < priority_port_list_num; i++) {
		if (priority_port_list[i].port_id == id)
			return priority_port_list[i].port_name;
	}
	return NULL;
}

int priority_display(char *buf)
{
	int i, s = 0, ret;
	int priority_w, priority_r;
	struct ddr_priority info;

	s += sprintf(buf + s, "\nusage Example:\n"
			"\techo 2 0xf (r) > priority\n"
			"\tparm0: port id\n"
			"\tparm1: priority value\n"
			"\tparm2: 'r' or 'w' priority (default set all)\n");

	s += sprintf(buf + s, "\tid\t name \t\tw_current \tw_max \t\tr_current \tr_max\n");
	for (i = 0; i < aml_db->ddr_priority_num; i++) {
		info =  aml_db->ddr_priority_desc[i];
		ret = ddr_priority_rw(info.port_id, &priority_r, &priority_w, DMC_READ);

		s += sprintf(buf + s, "%10d\t %*s ",
				info.port_id,
				-12, priority_find_port_name(info.port_id));
		if (priority_w < 0) {
			s += sprintf(buf + s, "\t%*s \t%*s ",
					-8, "N/A", -8, "N/A");
		} else {
			s += sprintf(buf + s, "\t%-8x \t%-8x",
					priority_w, info.w_width & 0xf);
		}

		if (priority_r < 0) {
			s += sprintf(buf + s, "\t%*s \t%*s\n",
					-8, "N/A", -8, "N/A");
		} else {
			s += sprintf(buf + s, "\t%-8x \t%-8x\n",
					priority_r, info.r_width  & 0xf);
		}
	}
	return s;
}
