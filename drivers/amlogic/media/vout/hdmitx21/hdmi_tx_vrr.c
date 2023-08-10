// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>

#include <linux/amlogic/media/vout/vinfo.h>
#include <linux/amlogic/media/vout/hdmi_tx21/enc_clk_config.h>
#include <linux/amlogic/media/vout/hdmi_tx21/hdmi_info_global.h>
#include <linux/amlogic/media/vout/hdmi_tx21/hdmi_tx_module.h>

#include "hdmi_tx.h"

/* BRR 720p60hz */
static const struct mvrr_const_val const_hdmi720p60_6000 = {
	.duration = 6000,
	.vtotal_fixed = 750,
};

static const struct mvrr_const_val const_hdmi720p60_5994 = {
	.duration = 5994,
	.vtotal_fixed = 750, /* 0.75 */
	.bit_len = 24,
	.frac_array = {0x3f, 0xe3, 0xfe},
};

static const struct mvrr_const_val const_hdmi720p60_5000 = {
	.duration = 5000,
	.vtotal_fixed = 900,
};

static const struct mvrr_const_val const_hdmi720p60_4800 = {
	.duration = 4800,
	.vtotal_fixed = 937, /* 0.5 */
	.bit_len = 8,
	.frac_array = {0x3c},
};

static const struct mvrr_const_val const_hdmi720p60_4795 = {
	.duration = 4795,
	.vtotal_fixed = 938, /* 0.4375 */
	.bit_len = 16,
	.frac_array = {0x1e, 0x0e},
};

static const struct mvrr_const_val const_hdmi720p60_3000 = {
	.duration = 3000,
	.vtotal_fixed = 1500,
};

static const struct mvrr_const_val const_hdmi720p60_2997 = {
	.duration = 2997,
	.vtotal_fixed = 1501, /* 0.5 */
	.bit_len = 56,
	.frac_array = {0x1f, 0xe0, 0x3f, 0x80, 0xfe, 0x03, 0xf8},
};

static const struct mvrr_const_val const_hdmi720p60_2500 = {
	.duration = 2500,
	.vtotal_fixed = 1800,
};

static const struct mvrr_const_val const_hdmi720p60_2400 = {
	.duration = 2400,
	.vtotal_fixed = 1875,
};

static const struct mvrr_const_val const_hdmi720p60_2397 = {
	.duration = 2397,
	.vtotal_fixed = 1876, /* 0.875 */
	.bit_len = 40,
	.frac_array = {0x1f, 0xff, 0xff, 0xff, 0xfc},
};

/* BRR 720p120hz */
static const struct mvrr_const_val const_hdmi720p120_12000 = {
	.duration = 12000,
	.vtotal_fixed = 750,
};

static const struct mvrr_const_val const_hdmi720p120_11988 = {
	.duration = 11988,
	.vtotal_fixed = 750, /* 0.75 */
	.bit_len = 24,
	.frac_array = {0x3f, 0xe3, 0xfe},
};

static const struct mvrr_const_val const_hdmi720p120_10000 = {
	.duration = 10000,
	.vtotal_fixed = 900,
};

static const struct mvrr_const_val const_hdmi720p120_6000 = {
	.duration = 6000,
	.vtotal_fixed = 1500,
};

static const struct mvrr_const_val const_hdmi720p120_5994 = {
	.duration = 5994,
	.vtotal_fixed = 1501, /* 0.5 */
	.bit_len = 56,
	.frac_array = {0x0f, 0xe0, 0x3f, 0x80, 0xfe, 0x03, 0xf8},
};

static const struct mvrr_const_val const_hdmi720p120_5000 = {
	.duration = 5000,
	.vtotal_fixed = 1800,
};

static const struct mvrr_const_val const_hdmi720p120_4800 = {
	.duration = 4800,
	.vtotal_fixed = 1875,
};

static const struct mvrr_const_val const_hdmi720p120_4795 = {
	.duration = 4795,
	.vtotal_fixed = 1876, /* 0.875 */
	.bit_len = 40,
	.frac_array = {0x1f, 0xff, 0xff, 0xff, 0xfc},
};

static const struct mvrr_const_val const_hdmi720p120_3000 = {
	.duration = 3000,
	.vtotal_fixed = 3000,
};

static const struct mvrr_const_val const_hdmi720p120_2997 = {
	.duration = 2997,
	.vtotal_fixed = 3003,
};

static const struct mvrr_const_val const_hdmi720p120_2500 = {
	.duration = 2500,
	.vtotal_fixed = 3600,
};

static const struct mvrr_const_val const_hdmi720p120_2400 = {
	.duration = 2400,
	.vtotal_fixed = 3750,
};

static const struct mvrr_const_val const_hdmi720p120_2397 = {
	.duration = 2397,
	.vtotal_fixed = 3753, /* 0.75 */
	.bit_len = 88,
	.frac_array = {
		0x03, 0xff, 0xff, 0xff, 0xfe, 0x00, 0x3f, 0xff, 0xff, 0xff, 0xe0
	},
};

/* BRR 1080p60hz */
static const struct mvrr_const_val const_hdmi1080p60_6000 = {
	.duration = 6000,
	.vtotal_fixed = 1125,
};

static const struct mvrr_const_val const_hdmi1080p60_5994 = {
	.duration = 5994,
	.vtotal_fixed = 1126, /* 0.125 */
	.bit_len = 24,
	.frac_array = {0x00, 0x38, 0x00},
};

static const struct mvrr_const_val const_hdmi1080p60_5000 = {
	.duration = 5000,
	.vtotal_fixed = 1350,
};

static const struct mvrr_const_val const_hdmi1080p60_4800 = {
	.duration = 4800,
	.vtotal_fixed = 1406, /* 0.25 */
	.bit_len = 16,
	.frac_array = {0x03, 0xc0},
};

static const struct mvrr_const_val const_hdmi1080p60_4795 = {
	.duration = 4795,
	.vtotal_fixed = 1407, /* 0.65625 */
	.bit_len = 32,
	.frac_array = {0x1f, 0xf1, 0x7f, 0xf0},
};

static const struct mvrr_const_val const_hdmi1080p60_3000 = {
	.duration = 3000,
	.vtotal_fixed = 2250,
};

static const struct mvrr_const_val const_hdmi1080p60_2997 = {
	.duration = 2997,
	.vtotal_fixed = 2252, /* 0.25 */
	.bit_len = 56,
	.frac_array = {0x00, 0x3f, 0x80, 0x00, 0x03, 0xf8, 0x00},
};

static const struct mvrr_const_val const_hdmi1080p60_2500 = {
	.duration = 2500,
	.vtotal_fixed = 2700,
};

static const struct mvrr_const_val const_hdmi1080p60_2400 = {
	.duration = 2400,
	.vtotal_fixed = 2812, /* 0.5 */
	.bit_len = 24,
	.frac_array = {0x03, 0xff, 0xc0},
};

static const struct mvrr_const_val const_hdmi1080p60_2397 = {
	.duration = 2397,
	.vtotal_fixed = 2815, /* 0.3125 */
	.bit_len = 128,
	.frac_array = {
		0x00, 0x7f, 0x80, 0x00, 0x3f, 0xc0, 0x00, 0x0f,
		0xf0, 0x00, 0x07, 0xf8, 0x00, 0x01, 0xfe, 0x00,
	},
};

/* BRR 1080p120hz */
static const struct mvrr_const_val const_hdmi1080p120_12000 = {
	.duration = 12000,
	.vtotal_fixed = 1125,
};

static const struct mvrr_const_val const_hdmi1080p120_11988 = {
	.duration = 11988,
	.vtotal_fixed = 1126, /* 0.125 */
	.bit_len = 24,
	.frac_array = {0x00, 0x38, 0x00},
};

static const struct mvrr_const_val const_hdmi1080p120_10000 = {
	.duration = 10000,
	.vtotal_fixed = 1350,
};

static const struct mvrr_const_val const_hdmi1080p120_6000 = {
	.duration = 6000,
	.vtotal_fixed = 2250,
};

static const struct mvrr_const_val const_hdmi1080p120_5994 = {
	.duration = 5994,
	.vtotal_fixed = 2252, /* 0.25 */
	.bit_len = 56,
	.frac_array = {0x00, 0x3f, 0x80, 0x00, 0x03, 0xf8, 0x00},
};

static const struct mvrr_const_val const_hdmi1080p120_5000 = {
	.duration = 5000,
	.vtotal_fixed = 2700,
};

static const struct mvrr_const_val const_hdmi1080p120_4800 = {
	.duration = 4800,
	.vtotal_fixed = 2812, /* 0.5 */
	.bit_len = 24,
	.frac_array = {0x03, 0xff, 0xc0},
};

static const struct mvrr_const_val const_hdmi1080p120_4795 = {
	.duration = 4795,
	.vtotal_fixed = 2812, /* 0.3125 */
	.bit_len = 48,
	.frac_array = {0x00, 0x3f, 0x80, 0x00, 0x3f, 0xc0},
};

static const struct mvrr_const_val const_hdmi1080p120_3000 = {
	.duration = 3000,
	.vtotal_fixed = 4500,
};

static const struct mvrr_const_val const_hdmi1080p120_2997 = {
	.duration = 2997,
	.vtotal_fixed = 4504, /* 0.5 */
	.bit_len = 32,
	.frac_array = {0x00, 0xff, 0xff, 0x00},
};

static const struct mvrr_const_val const_hdmi1080p120_2500 = {
	.duration = 2500,
	.vtotal_fixed = 5400,
};

static const struct mvrr_const_val const_hdmi1080p120_2400 = {
	.duration = 2400,
	.vtotal_fixed = 5625,
};

static const struct mvrr_const_val const_hdmi1080p120_2397 = {
	.duration = 2397,
	.vtotal_fixed = 5630, /* 0.625 */
	.bit_len = 48,
	.frac_array = {0x00, 0x7f, 0xff, 0xff, 0xfe, 0x00},
};

/* BRR 2160p60hz */
static const struct mvrr_const_val const_hdmi2160p60_6000 = {
	.duration = 6000,
	.vtotal_fixed = 2250,
};

static const struct mvrr_const_val const_hdmi2160p60_5994 = {
	.duration = 5994,
	.vtotal_fixed = 2252, /* 0.25 */
	.bit_len = 56,
	.frac_array = {0x00, 0x3f, 0x80, 0x00, 0x03, 0xf8, 0x00},
};

static const struct mvrr_const_val const_hdmi2160p60_5000 = {
	.duration = 5000,
	.vtotal_fixed = 2700,
};

static const struct mvrr_const_val const_hdmi2160p60_4800 = {
	.duration = 4800,
	.vtotal_fixed = 2812, /* 0.5 */
	.bit_len = 24,
	.frac_array = {0x03, 0xff, 0xc0},
};

static const struct mvrr_const_val const_hdmi2160p60_4795 = {
	.duration = 4795,
	.vtotal_fixed = 2815, /* 0.3125 */
	.bit_len = 48,
	.frac_array = {0x00, 0x3f, 0x80, 0x00, 0x3f, 0xc0},
};

static const struct mvrr_const_val const_hdmi2160p60_3000 = {
	.duration = 3000,
	.vtotal_fixed = 4500,
};

static const struct mvrr_const_val const_hdmi2160p60_2997 = {
	.duration = 2997,
	.vtotal_fixed = 4504, /* 0.5 */
	.bit_len = 32,
	.frac_array = {0x00, 0xff, 0xff, 0x00},
};

static const struct mvrr_const_val const_hdmi2160p60_2500 = {
	.duration = 2500,
	.vtotal_fixed = 5400,
};

static const struct mvrr_const_val const_hdmi2160p60_2400 = {
	.duration = 2400,
	.vtotal_fixed = 5625,
};

static const struct mvrr_const_val const_hdmi2160p60_2397 = {
	.duration = 2397,
	.vtotal_fixed = 5630, /* 0.625 */
	.bit_len = 48,
	.frac_array = {0x00, 0x3f, 0xff, 0xff, 0xff, 0x00},
};

/* BRR 2160p120hz */
static const struct mvrr_const_val const_hdmi2160p120_12000 = {
	.duration = 12000,
	.vtotal_fixed = 2250,
};

static const struct mvrr_const_val const_hdmi2160p120_11988 = {
	.duration = 11988,
	.vtotal_fixed = 2252, /* 0.25 */
	.bit_len = 32,
	.frac_array = {0x01, 0xe0, 0x03, 0xc0},
};

static const struct mvrr_const_val const_hdmi2160p120_10000 = {
	.duration = 10000,
	.vtotal_fixed = 2700,
};

static const struct mvrr_const_val const_hdmi2160p120_6000 = {
	.duration = 6000,
	.vtotal_fixed = 4500,
};

static const struct mvrr_const_val const_hdmi2160p120_5994 = {
	.duration = 5994,
	.vtotal_fixed = 4504, /* 0.5 */
	.bit_len = 24,
	.frac_array = {0x03, 0xff, 0xc0},
};

static const struct mvrr_const_val const_hdmi2160p120_5000 = {
	.duration = 5000,
	.vtotal_fixed = 5400,
};

static const struct mvrr_const_val const_hdmi2160p120_4800 = {
	.duration = 4800,
	.vtotal_fixed = 5625,
};

static const struct mvrr_const_val const_hdmi2160p120_4795 = {
	.duration = 4795,
	.vtotal_fixed = 5630, /* 0.625 */
	.bit_len = 24,
	.frac_array = {0x00, 0x7f, 0xff},
};

static const struct mvrr_const_val const_hdmi2160p120_3000 = {
	.duration = 3000,
	.vtotal_fixed = 9000,
};

static const struct mvrr_const_val const_hdmi2160p120_2997 = {
	.duration = 2997,
	.vtotal_fixed = 9009,
};

static const struct mvrr_const_val const_hdmi2160p120_2500 = {
	.duration = 2500,
	.vtotal_fixed = 10800,
};

static const struct mvrr_const_val const_hdmi2160p120_2400 = {
	.duration = 2400,
	.vtotal_fixed = 11250,
};

static const struct mvrr_const_val const_hdmi2160p120_2397 = {
	.duration = 2397,
	.vtotal_fixed = 11261, /* 0.25 */
	.bit_len = 32,
	.frac_array = {0x00, 0x00, 0x00, 0xff},
};

/* BRR 4320p60hz */
static const struct mvrr_const_val const_hdmi4320p60_6000 = {
	.duration = 6000,
	.vtotal_fixed = 4400,
};

static const struct mvrr_const_val const_hdmi4320p60_5994 = {
	.duration = 5994,
	.vtotal_fixed = 4404, /* 0.4 */
	.bit_len = 40,
	.frac_array = {0x03, 0xc0, 0xf0, 0x3c, 0x0f},
};

static const struct mvrr_const_val const_hdmi4320p60_5000 = {
	.duration = 5000,
	.vtotal_fixed = 5280,
};

static const struct mvrr_const_val const_hdmi4320p60_4800 = {
	.duration = 4800,
	.vtotal_fixed = 5500,
};

static const struct mvrr_const_val const_hdmi4320p60_4795 = {
	.duration = 4795,
	.vtotal_fixed = 5505, /* 0.5 */
	.bit_len = 40,
	.frac_array = {0x00, 0x3f, 0xff, 0xfc, 0x00},
};

static const struct mvrr_const_val const_hdmi4320p60_3000 = {
	.duration = 3000,
	.vtotal_fixed = 8800,
};

static const struct mvrr_const_val const_hdmi4320p60_2997 = {
	.duration = 2997,
	.vtotal_fixed = 8808, /* 0.8 */
	.bit_len = 40,
	.frac_array = {0x00, 0xff, 0xff, 0xff, 0xff},
};

static const struct mvrr_const_val const_hdmi4320p60_2500 = {
	.duration = 2500,
	.vtotal_fixed = 10506,
};

static const struct mvrr_const_val const_hdmi4320p60_2400 = {
	.duration = 2400,
	.vtotal_fixed = 11000,
};

static const struct mvrr_const_val const_hdmi4320p60_2397 = {
	.duration = 2397,
	.vtotal_fixed = 11011,
};

const struct mvrr_const_st const_hdmi1080p60_val = {
	.brr_vic = HDMI_16_1920x1080p60_16x9,
	.val = {
		&const_hdmi1080p60_6000,
		&const_hdmi1080p60_5994,
		&const_hdmi1080p60_5000,
		&const_hdmi1080p60_4800,
		&const_hdmi1080p60_4795,
		&const_hdmi1080p60_3000,
		&const_hdmi1080p60_2997,
		&const_hdmi1080p60_2500,
		&const_hdmi1080p60_2400,
		&const_hdmi1080p60_2397,
		NULL,
	},
};

const struct mvrr_const_st const_hdmi1080p120_val = {
	.brr_vic = HDMI_63_1920x1080p120_16x9,
	.val = {
		&const_hdmi1080p120_12000,
		&const_hdmi1080p120_11988,
		&const_hdmi1080p120_10000,
		&const_hdmi1080p120_6000,
		&const_hdmi1080p120_5994,
		&const_hdmi1080p120_5000,
		&const_hdmi1080p120_4800,
		&const_hdmi1080p120_4795,
		&const_hdmi1080p120_3000,
		&const_hdmi1080p120_2997,
		&const_hdmi1080p120_2500,
		&const_hdmi1080p120_2400,
		&const_hdmi1080p120_2397,
		NULL,
	},
};

const struct mvrr_const_st const_hdmi720p60_val = {
	.brr_vic = HDMI_4_1280x720p60_16x9,
	.val = {
		&const_hdmi720p60_6000,
		&const_hdmi720p60_5994,
		&const_hdmi720p60_5000,
		&const_hdmi720p60_4800,
		&const_hdmi720p60_4795,
		&const_hdmi720p60_3000,
		&const_hdmi720p60_2997,
		&const_hdmi720p60_2500,
		&const_hdmi720p60_2400,
		&const_hdmi720p60_2397,
		NULL,
	},
};

const struct mvrr_const_st const_hdmi720p120_val = {
	.brr_vic = HDMI_47_1280x720p120_16x9,
	.val = {
		&const_hdmi720p120_12000,
		&const_hdmi720p120_11988,
		&const_hdmi720p120_10000,
		&const_hdmi720p120_6000,
		&const_hdmi720p120_5994,
		&const_hdmi720p120_5000,
		&const_hdmi720p120_4800,
		&const_hdmi720p120_4795,
		&const_hdmi720p120_3000,
		&const_hdmi720p120_2997,
		&const_hdmi720p120_2500,
		&const_hdmi720p120_2400,
		&const_hdmi720p120_2397,
		NULL,
	},
};

const struct mvrr_const_st const_hdmi2160p60_val = {
	.brr_vic = HDMI_97_3840x2160p60_16x9,
	.val = {
		&const_hdmi2160p60_6000,
		&const_hdmi2160p60_5994,
		&const_hdmi2160p60_5000,
		&const_hdmi2160p60_4800,
		&const_hdmi2160p60_4795,
		&const_hdmi2160p60_3000,
		&const_hdmi2160p60_2997,
		&const_hdmi2160p60_2500,
		&const_hdmi2160p60_2400,
		&const_hdmi2160p60_2397,
		NULL,
	},
};

/* The vtotal parameters of 4096x2160p60hz are the same as 3840x2160p60hz */
const struct mvrr_const_st const_hdmismpte60_val = {
	.brr_vic = HDMI_102_4096x2160p60_256x135,
	.val = {
		&const_hdmi2160p60_6000,
		&const_hdmi2160p60_5994,
		&const_hdmi2160p60_5000,
		&const_hdmi2160p60_4800,
		&const_hdmi2160p60_4795,
		&const_hdmi2160p60_3000,
		&const_hdmi2160p60_2997,
		&const_hdmi2160p60_2500,
		&const_hdmi2160p60_2400,
		&const_hdmi2160p60_2397,
		NULL,
	},
};

const struct mvrr_const_st const_hdmi2160p120_val = {
	.brr_vic = HDMI_118_3840x2160p120_16x9,  //HDMI_120_3840x2160p120_64x27
	.val = {
		&const_hdmi2160p120_12000,
		&const_hdmi2160p120_11988,
		&const_hdmi2160p120_10000,
		&const_hdmi2160p120_6000,
		&const_hdmi2160p120_5994,
		&const_hdmi2160p120_5000,
		&const_hdmi2160p120_4800,
		&const_hdmi2160p120_4795,
		&const_hdmi2160p120_3000,
		&const_hdmi2160p120_2997,
		&const_hdmi2160p120_2500,
		&const_hdmi2160p120_2400,
		&const_hdmi2160p120_2397,
		NULL,
	},
};

const struct mvrr_const_st const_hdmismpte120_val = {
	.brr_vic = HDMI_219_4096x2160p120_256x135,
	.val = {
		&const_hdmi2160p120_12000,
		&const_hdmi2160p120_11988,
		&const_hdmi2160p120_10000,
		&const_hdmi2160p120_6000,
		&const_hdmi2160p120_5994,
		&const_hdmi2160p120_5000,
		&const_hdmi2160p120_4800,
		&const_hdmi2160p120_4795,
		&const_hdmi2160p120_3000,
		&const_hdmi2160p120_2997,
		&const_hdmi2160p120_2500,
		&const_hdmi2160p120_2400,
		&const_hdmi2160p120_2397,
		NULL,
	},
};

const struct mvrr_const_st const_hdmi4320p60_val = {
	.brr_vic = HDMI_199_7680x4320p60_16x9,
	.val = {
		&const_hdmi4320p60_6000,
		&const_hdmi4320p60_5994,
		&const_hdmi4320p60_5000,
		&const_hdmi4320p60_4800,
		&const_hdmi4320p60_4795,
		&const_hdmi4320p60_3000,
		&const_hdmi4320p60_2997,
		&const_hdmi4320p60_2500,
		&const_hdmi4320p60_2400,
		&const_hdmi4320p60_2397,
		NULL,
	},
};

const struct mvrr_const_st *qms_const[] = {
	&const_hdmi1080p60_val,
	&const_hdmi1080p120_val,
	&const_hdmi2160p60_val,
	&const_hdmi720p60_val,
	&const_hdmi720p120_val,
	&const_hdmismpte60_val,
	&const_hdmi2160p120_val,
	&const_hdmismpte120_val,
	&const_hdmi4320p60_val,
	NULL,
};

/* debug only, should be positive value. if it is N, then vysnc_handler
 * will handle N frames, then it will be 0, and vysnc_handler is pending
 * value 1 is only for single steps, and -1 will work as normally.
 */
static int vrr_dbg_vframe = -1;
MODULE_PARM_DESC(vrr_dbg_vframe, "\n vrr_dbg_vframe\n");
module_param(vrr_dbg_vframe, int, 0644);

/* verbose log mode */
static int vrr_verbose;
MODULE_PARM_DESC(vrr_verbose, "\n vrr_verbose\n");
module_param(vrr_verbose, int, 0644);

static void vrr_debug_info(const char *fmt, ...)
{
	int len;
	char temp[128] = {0};
	va_list args;

	if (!vrr_verbose)
		return;

	va_start(args, fmt);
	len = vsnprintf(temp, sizeof(temp), fmt, args);
	va_end(args);

	if (len)
		pr_info("%s", temp);
}

const static struct mvrr_const_val *search_vrrconf_mconst(enum hdmi_vic brr_vic,
	int duration)
{
	const struct mvrr_const_st **table_vic = NULL;
	const struct mvrr_const_val *const *table_val = NULL;

	vrr_debug_info("%s[%d] brr_vic: %d duration: %d\n", __func__, __LINE__,
		brr_vic, duration);
	for (table_vic = qms_const; table_vic; table_vic++) {
		if ((*table_vic)->brr_vic == brr_vic) {
			table_val = (*table_vic)->val;
			for (; table_val; table_val++) {
				if ((*table_val)->duration == duration)
					break;
			}
			break;
		}
	}

	if (!table_val)
		vrr_debug_info("%s[%d] not find brr_vic: %d duration: %d\n",
			__func__, __LINE__, brr_vic, duration);

	return *table_val;
}

/* vrr_para will be used in this file, and the member vrr_params may
 * be changed at any time. So it should be set by hdmitx_set_vrr_para()
 * from set_vframe_rate_hint() call and use hdmitx_get_vrr_params() to
 * get current configure
 */
static struct tx_vrr_params vrr_para;

void tx_vrr_params_init(void)
{
	spin_lock_init(&vrr_para.lock);
}

void hdmitx_set_vrr_para(const struct vrr_conf_para *para)
{
	spin_lock(&vrr_para.lock);
	vrr_para.conf_params = *para;
	spin_unlock(&vrr_para.lock);
}

static void hdmitx_get_vrr_params(struct vrr_conf_para *para)
{
	spin_lock(&vrr_para.lock);
	*para = vrr_para.conf_params;
	spin_unlock(&vrr_para.lock);
}

int hdmitx_dump_vrr_status(struct seq_file *s, void *p)
{
	struct tx_vrr_params *vrr = &vrr_para;
	struct vrr_conf_para *conf = &vrr_para.conf_params;

	seq_puts(s, "\n--------HDMITX VRR--------\n");

	switch (conf->type) {
	case T_VRR_GAME:
		seq_puts(s, "type: GAME-VRR\n");
		break;
	case T_VRR_QMS:
		seq_puts(s, "type: QMS-VRR\n");
		break;
	case T_VRR_NONE:
	default:
		return 0;
	}

	seq_printf(s, "vrr_enabled %d\n", conf->vrr_enabled);
	seq_printf(s, "fva_factor %d\n", conf->fva_factor);
	seq_printf(s, "brr_vic %d\n", conf->brr_vic);
	seq_printf(s, "duration %d\n", conf->duration);
	seq_printf(s, "fapa_end_extended %d\n", conf->fapa_end_extended);
	seq_printf(s, "qms support %d\n", conf->qms_sup);
	seq_printf(s, "mdelta %d\n", conf->mdelta_bit);
	seq_printf(s, "cinemavrr %d\n", conf->cinemavrr_bit);
	seq_printf(s, "neg_mvrr %d\n", conf->neg_mvrr_bit);
	seq_printf(s, "fva support %d\n", conf->fva_sup);
	seq_printf(s, "fapa_start_location %d\n", conf->fapa_start_loc);
	seq_printf(s, "qms_tfrmin %d\n", conf->qms_tfrmin);
	seq_printf(s, "qms_tfrmax %d\n", conf->qms_tfrmax);
	seq_printf(s, "vrr min %d\n", conf->vrrmin);
	seq_printf(s, "vrr max %d\n", conf->vrrmax);
	if (vrr->mconst_val) {
		int i;
		int pos;
		char tmp[33] = {0};
		const struct mvrr_const_val *m;

		m = search_vrrconf_mconst(conf->brr_vic, conf->duration);

		seq_printf(s, "mconst vic %d\n", conf->brr_vic);
		seq_printf(s, "mconst duration %d\n", m->duration);
		seq_printf(s, "mconst vtotal_fixed %d\n", m->vtotal_fixed);
		if (m->bit_len) {
			for (pos = 0, i = 0; i < m->bit_len / 8; i++)
				pos += snprintf(tmp + pos, sizeof(tmp) - 1, "%02x",
					m->frac_array[i]);
			seq_printf(s, "%s\n", tmp);
		}
	}
	seq_printf(s, "frame count %d\n", vrr->frame_cnt);
	seq_printf(s, "mdelta_limit %d\n", vrr->mdelta_limit);

	return 0;
}

static bool is_vrrconf_changed(struct vrr_conf_para *cur)
{
	static struct vrr_conf_para tmp;

	/* compare the local variable tmp, update if different */
	if (memcmp(&tmp, cur, sizeof(tmp))) {
		hdmitx_get_vrr_params(&tmp);
		return 1;
	}
	return 0;
}

const static u16 vsync_tfr_table[TFR_MAX] = {
	[TFR_QMSVRR_INACTIVE] = 0,
	[TFR_23P97] = 2397,
	[TFR_24] = 2400,
	[TFR_25] = 2500,
	[TFR_29P97] = 2997,
	[TFR_30] = 3000,
	[TFR_47P95] = 4795,
	[TFR_48] = 4800,
	[TFR_50] = 5000,
	[TFR_59P94] = 5994,
	[TFR_60] = 6000,
	[TFR_100] = 10000,
	[TFR_119P88] = 11988,
	[TFR_120] = 12000,
};

static enum TARGET_FRAME_RATE vsync_match_to_tfr(const u16 duration)
{
	int i;

	for (i = 0; i < TFR_MAX; i++) {
		if (duration == vsync_tfr_table[i])
			break;
	}

	if (i == TFR_MAX)
		return TFR_QMSVRR_INACTIVE;
	return (enum TARGET_FRAME_RATE)i;
}

static void vrr_init_game_para(struct tx_vrr_params *para)
{
	struct vrr_conf_para *conf = &para->conf_params;
	struct emp_packet_st *vrr_pkt = &para->emp_vrr_pkt;
	u8 fva_factor = conf->fva_factor;
	u16 brr_rate = 60;
	enum hdmi_vic brr_vic = conf->brr_vic;
	u16 brr_vfront;
	const struct hdmi_timing *timing;

	if (fva_factor == 0)
		fva_factor = 1; /* at least be 1 */
	para->frame_cnt = 0;
	memset(&para->emp_vrr_pkt, 0, sizeof(para->emp_vrr_pkt));

	memset(vrr_pkt, 0, sizeof(*vrr_pkt));
	vrr_pkt->type = (enum emp_type)conf->type;
	hdmi_emp_frame_set_member(vrr_pkt, CONF_HEADER_INIT,
		HDMI_INFOFRAME_TYPE_EMP);
	hdmi_emp_frame_set_member(vrr_pkt, CONF_HEADER_FIRST, 1);
	hdmi_emp_frame_set_member(vrr_pkt, CONF_HEADER_LAST, 1);
	hdmi_emp_frame_set_member(vrr_pkt, CONF_HEADER_SEQ_INDEX, 0);
	hdmi_emp_frame_set_member(vrr_pkt, CONF_DS_TYPE, 0);
	hdmi_emp_frame_set_member(vrr_pkt, CONF_SYNC, 0);
	hdmi_emp_frame_set_member(vrr_pkt, CONF_VFR, 1);
	hdmi_emp_frame_set_member(vrr_pkt, CONF_AFR, 0);
	hdmi_emp_frame_set_member(vrr_pkt, CONF_NEW, 0);
	hdmi_emp_frame_set_member(vrr_pkt, CONF_END, 0);
	hdmi_emp_frame_set_member(vrr_pkt, CONF_ORG_ID, 0);
	hdmi_emp_frame_set_member(vrr_pkt, CONF_DATA_SET_TAG, 1);
	hdmi_emp_frame_set_member(vrr_pkt, CONF_DATA_SET_LENGTH, 4);

	timing = hdmitx21_gettiming_from_vic(brr_vic);
	brr_vfront = timing ? timing->v_front : 1920;
	hdmi_emp_frame_set_member(vrr_pkt, CONF_VRR_EN, 1);
	hdmi_emp_frame_set_member(vrr_pkt, CONF_FACTOR_M1, fva_factor - 1);
	hdmi_emp_frame_set_member(vrr_pkt, CONF_BASE_VFRONT, brr_vfront);
	hdmi_emp_frame_set_member(vrr_pkt, CONF_BASE_REFRESH_RATE, brr_rate);
}

static void vrr_init_qms_para(struct tx_vrr_params *para)
{
	const struct mvrr_const_val *mval;
	struct vrr_conf_para *conf = &para->conf_params;
	struct emp_packet_st *vrr_pkt = &para->emp_vrr_pkt;
	u8 fva_factor = conf->fva_factor;
	u16 brr_rate = 60;
	enum hdmi_vic brr_vic = conf->brr_vic;
	u16 brr_vfront;
	const struct hdmi_timing *timing;

	hdmitx_get_vrr_params(conf);
	if (conf->type == T_VRR_GAME) {
		if (fva_factor == 0)
			fva_factor = 1; /* at least be 1 */
	}
	para->mconst_val = search_vrrconf_mconst(brr_vic, conf->duration);
	if (!para->mconst_val) {
		pr_info("use the default const_hdmi1080p60_6000\n");
		para->mconst_val = &const_hdmi1080p60_6000;
	}
	para->frame_cnt = 0;
	para->fapa_early_cnt = 4;
	para->mdelta_limit = 0;
	memset(&para->emp_vrr_pkt, 0, sizeof(para->emp_vrr_pkt));
	if (conf->mdelta_bit && para->mconst_val) {
		mval = search_vrrconf_mconst(brr_vic, 6000);
		if (mval)
			para->mdelta_limit = mval->vtotal_fixed / 2;
	}

	if (para->mconst_val && (brr_vic == HDMI_63_1920x1080p120_16x9 ||
		brr_vic == HDMI_118_3840x2160p120_16x9))
		brr_rate = 120;

	memset(vrr_pkt, 0, sizeof(*vrr_pkt));
	vrr_pkt->type = (enum emp_type)conf->type;
	hdmi_emp_frame_set_member(vrr_pkt, CONF_HEADER_INIT,
		HDMI_INFOFRAME_TYPE_EMP);
	hdmi_emp_frame_set_member(vrr_pkt, CONF_HEADER_FIRST, 1);
	hdmi_emp_frame_set_member(vrr_pkt, CONF_HEADER_LAST, 1);
	hdmi_emp_frame_set_member(vrr_pkt, CONF_HEADER_SEQ_INDEX, 0);
	hdmi_emp_frame_set_member(vrr_pkt, CONF_DS_TYPE, 0);
	hdmi_emp_frame_set_member(vrr_pkt, CONF_SYNC, 1);
	hdmi_emp_frame_set_member(vrr_pkt, CONF_VFR, 1);
	hdmi_emp_frame_set_member(vrr_pkt, CONF_AFR, 0);
	hdmi_emp_frame_set_member(vrr_pkt, CONF_NEW, 0);
	hdmi_emp_frame_set_member(vrr_pkt, CONF_END, 0);
	hdmi_emp_frame_set_member(vrr_pkt, CONF_ORG_ID, 0);
	hdmi_emp_frame_set_member(vrr_pkt, CONF_DATA_SET_TAG, 1);
	hdmi_emp_frame_set_member(vrr_pkt, CONF_DATA_SET_LENGTH, 4);

	timing = hdmitx21_gettiming_from_vic(brr_vic);
	brr_vfront = timing ? timing->v_front : 1920;
	hdmi_emp_frame_set_member(vrr_pkt, CONF_M_CONST, 0);
	hdmi_emp_frame_set_member(vrr_pkt, CONF_QMS_EN, 1);
	hdmi_emp_frame_set_member(vrr_pkt, CONF_NEXT_TFR,
		vsync_match_to_tfr(para->mconst_val->duration));
	hdmi_emp_frame_set_member(vrr_pkt, CONF_BASE_VFRONT, brr_vfront);
	hdmi_emp_frame_set_member(vrr_pkt, CONF_BASE_REFRESH_RATE, brr_rate);
}

static void vrr_init_para(struct tx_vrr_params *para)
{
	struct vrr_conf_para *conf = &para->conf_params;

	hdmitx_get_vrr_params(conf);
	if (conf->type == T_VRR_GAME)
		vrr_init_game_para(para);
	else
		vrr_init_qms_para(para);
}

static u16 calc_cur_vtotal(const struct mvrr_const_val *mvrr, u32 frame_cnt)
{
	u32 pos;
	u16 vtotal = 1125;

	if (!mvrr)
		return vtotal;

	vtotal = mvrr->vtotal_fixed;
	/* if bit_len is 0, then means there is no fraction */
	if (!mvrr->bit_len)
		return vtotal;

	/* calculate the fraction number */
	pos = frame_cnt % mvrr->bit_len;
	if (mvrr->frac_array[pos / 8] & (1 << (7 - (pos % 8))))
		vtotal++;

	return vtotal;
}

static u16 calc_vtotal_with_mdelta(struct tx_vrr_params *vrr)
{
	u32 vtotal_target;
	u32 vtotal_cur;
	u32 delta;
	u32 val;

	if (!vrr || !vrr->mconst_val)
		return 0;

	vtotal_cur = hdmitx_vrr_get_maxlncnt();
	delta = vrr->mdelta_limit;
	vtotal_target = vrr->mconst_val->vtotal_fixed;
	if (vtotal_cur < vtotal_target)
		val = min(vtotal_cur + delta, vtotal_target);
	else
		val = max(vtotal_cur - delta, vtotal_target);
	vrr_debug_info("%s[%d] vtotal_cur=%d vtotal_target=%d val=%d\n",
		__func__, __LINE__, vtotal_cur, vtotal_target, val);
	vrr_debug_info("mdelta_limit = %d\n", delta);
	return val;
}

static void vrr_cur_vtotal_debug(u32 frame_cnt, bool m_const, u32 vt_target)
{
	u32 dist;
	u32 vt_cur = hdmitx_vrr_get_maxlncnt();
	static bool m_const_pre;
	static u32 vtotal_pre;
	bool trigger = 0;

	if (vt_cur > vt_target)
		dist = vt_cur - vt_target;
	else
		dist = vt_target - vt_cur;

	if (vtotal_pre != vt_cur || m_const_pre != m_const) {
		trigger = 1;
		vtotal_pre = vt_cur;
		m_const_pre = m_const;
	}
	if (trigger)
		vrr_debug_info("[%s]FrameNo%d mcon%d vt_target/%d %s vt_cur/%d\n",
			__func__, frame_cnt, m_const, vt_target,
			dist == 0 ? "==" : (dist == 1 ? "?=" : "!="), vt_cur);
}

ssize_t _vrr_cap_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int pos = 0;
	struct hdmitx_dev *hdev = get_hdmitx21_device();
	struct rx_cap *prxcap = &hdev->rxcap;
	struct vrr_device_s *vrr = &hdev->hdmitx_vrr_dev;

	pos += snprintf(buf + pos, PAGE_SIZE,
			"neg_mvrr: %d\n", prxcap->neg_mvrr);
	pos += snprintf(buf + pos, PAGE_SIZE,
			"fva: %d\n", prxcap->fva);
	pos += snprintf(buf + pos, PAGE_SIZE,
			"allm: %d\n", prxcap->allm);
	pos += snprintf(buf + pos, PAGE_SIZE,
			"fapa_start_location: %d\n", prxcap->fapa_start_loc);
	pos += snprintf(buf + pos, PAGE_SIZE,
			"qms: %d\n", prxcap->qms);
	pos += snprintf(buf + pos, PAGE_SIZE,
			"qms_tfr_max: %d\n", prxcap->qms_tfr_max);
	pos += snprintf(buf + pos, PAGE_SIZE,
			"qms_tfr_min: %d\n", prxcap->qms_tfr_min);
	pos += snprintf(buf + pos, PAGE_SIZE,
			"mdelta: %d\n", prxcap->mdelta);
	pos += snprintf(buf + pos, PAGE_SIZE,
			"RX_CAP vrr_max: %d\n", prxcap->vrr_max);
	pos += snprintf(buf + pos, PAGE_SIZE,
			"RX_CAP vrr_min: %d\n", prxcap->vrr_min);
	pos += snprintf(buf + pos, PAGE_SIZE,
			"vrr.vfreq_max: %d\n", vrr->vfreq_max);
	pos += snprintf(buf + pos, PAGE_SIZE,
			"vrr.vfreq_min: %d\n", vrr->vfreq_min);
	pos += snprintf(buf + pos, PAGE_SIZE,
			"vrr.vline_max: %d\n", vrr->vline_max);
	pos += snprintf(buf + pos, PAGE_SIZE,
			"vrr.vline_min: %d\n", vrr->vline_min);
	return pos;
}

static const enum hdmi_vic brr_list[] = {
	HDMI_16_1920x1080p60_16x9,
	HDMI_63_1920x1080p120_16x9,
	HDMI_4_1280x720p60_16x9,
	HDMI_47_1280x720p120_16x9,
	HDMI_97_3840x2160p60_16x9,
	HDMI_102_4096x2160p60_256x135,
	HDMI_118_3840x2160p120_16x9,
	HDMI_199_7680x4320p60_16x9,
};

static bool check_qms_brr_format(const enum hdmi_vic vic)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(brr_list); i++) {
		if (vic == brr_list[i])
			return 1;
	}

	return 0;
}

int hdmitx_set_fr_hint(int rate, void *data)
{
	struct hdmitx_dev *hdev = get_hdmitx21_device();
	struct rx_cap *prxcap = &hdev->rxcap;
	struct vrr_conf_para para;
	enum TARGET_FRAME_RATE tfr = TFR_QMSVRR_INACTIVE;
	int tmp_rate;

	hdmitx_vrr_disable();
	if (!hdev->para) {
		para.vrr_enabled = 0;
		return 0;
	}

	/* check current rate, should less or equal than current rate of BRR */
	tmp_rate = hdev->para->timing.v_freq / 10;
	if (rate < 2397 || rate > 120000 || rate > tmp_rate) {
		pr_info("vrr rate over range %d [2397~%d]\n", rate, tmp_rate);
		return 0;
	}

	if (hdev->vrr_type == 1) {
		para.type = T_VRR_GAME;
	} else if (hdev->vrr_type == 2) {
		para.type = T_VRR_QMS;
	} else {
		para.vrr_enabled = 0;
		return 0;
	}

	para.duration = rate;
	para.brr_vic = hdev->para->timing.vic;
	if (para.type == T_VRR_QMS) {
		tfr = vsync_match_to_tfr(rate);
		if (tfr == TFR_QMSVRR_INACTIVE || tfr == TFR_MAX) {
			pr_info("unsupport current rate %d\n", rate);
			return 0;
		}
		if (!check_qms_brr_format(para.brr_vic)) {
			pr_info("hdmitx: QMS only support BRR %s\n",
				"720p60/120hz,1080p60/120hz,2160p60hz\n");
			pr_info("hdmitx: current vic %d\n", para.brr_vic);
			para.vrr_enabled = 0;
			return 0;
		}
		if (!prxcap->qms)
			pr_info("TODO TV not support QMS\n");
		/* for application, here will set the mode like below
		 * /sys/class/amhdmitx/amhdmitx0/frac_rate_policy
		 * /sys/class/display/fr_hint
		 * QMS: 1080p24   set frac_rate_policy as 0 and fr_hint as 24
		 * QMS: 1080p223.976   set frac_rate_policy as 1 and fr_hint as 24
		 */
		if (hdev->frac_rate_policy) {
			switch (rate) {
			case 2400:
			case 3000:
			case 4800:
			case 6000:
			case 12000:
				para.duration = rate * 1000 / 1001;
				break;
			default:
				break;
			}
		}
	} else { /* for vrr-game */
		para.duration = rate;
	}

	para.vrr_enabled = 1;
	para.fapa_end_extended = prxcap->fapa_end_extended;
	para.qms_sup = prxcap->qms;
	para.mdelta_bit = prxcap->mdelta;
	para.cinemavrr_bit = prxcap->cinemavrr;
	para.neg_mvrr_bit = prxcap->neg_mvrr;
	para.fva_sup = prxcap->fva;
	para.fva_factor = 1; /* todo */
	para.fapa_start_loc = prxcap->fapa_start_loc;
	para.qms_tfrmin = prxcap->qms_tfr_min;
	para.qms_tfrmax = prxcap->qms_tfr_max;
	para.vrrmin = prxcap->vrr_min;
	para.vrrmax = prxcap->vrr_max;

	hdmitx_set_vrr_para(&para);
	hdmitx_vrr_enable();

	return 1;
}

void hdmitx_unregister_vrr(struct hdmitx_dev *hdev)
{
	int ret;

	ret = aml_vrr_unregister_device(hdev->enc_idx);
	pr_info("%s ret = %d\n", __func__, ret);
}

static struct vinfo_s *hdmitx_get_curvinfo(void *data)
{
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	if (!hdev->para)
		return NULL;
	return &hdev->para->hdmitx_vinfo;
}

void hdmitx_register_vrr(struct hdmitx_dev *hdev)
{
	int ret;
	char *name = "hdmitx_vrr";
	struct rx_cap *prxcap = &hdev->rxcap;
	struct vinfo_s *vinfo;
	struct vrr_device_s *vrr = &hdev->hdmitx_vrr_dev;

	vinfo = hdmitx_get_curvinfo(NULL);
	if (!vinfo || vinfo->mode != VMODE_HDMI)
		return;
	vrr->output_src = VRR_OUTPUT_ENCP;
	vrr->vfreq_max = prxcap->vrr_min;
	vrr->vfreq_min = prxcap->vrr_max;
	vrr->vline_max =
		vinfo->vtotal * (prxcap->vrr_max / prxcap->vrr_min);
	if (prxcap->vrr_max == 0)
		vrr->vline_min = 0;
	else
		vrr->vline_min = vinfo->vtotal;
	vrr->vline = vinfo->vtotal;
	if (prxcap->vrr_max < 100 || prxcap->vrr_min > 48)
		vrr->enable = 0;
	else
		vrr->enable = 1;
	strncpy(vrr->name, name, VRR_NAME_LEN_MAX);
	ret = aml_vrr_register_device(vrr, hdev->enc_idx);
	pr_info("%s ret = %d\n", __func__, ret);
}

static void hdmitx_vrr_game_handler(struct hdmitx_dev *hdev)
{
	static bool m_const;
	struct tx_vrr_params *vrr = &vrr_para;
	struct vrr_conf_para *conf = &vrr->conf_params;
	const struct hdmi_timing *timing = NULL;
	u32 vtotal_tmp = 0;

	timing = hdmitx21_gettiming_from_vic(conf->brr_vic);
	if (!timing) {
		vrr_debug_info("%s[%d] failed to get timing of brr_vic %d\n",
			__func__, __LINE__, conf->brr_vic);
		return;
	}
	vtotal_tmp = timing->v_total * timing->v_freq / 10;
	vtotal_tmp = vtotal_tmp / conf->duration;
	vrr->game_val.vtotal_fixed = vtotal_tmp;
	vrr_debug_info("game-vrr vtotal = %d\n", vrr->game_val.vtotal_fixed);

	hdmi_emp_infoframe_set(EMP_TYPE_VRR_GAME, &vrr->emp_vrr_pkt);
	hdmitx_vrr_set_maxlncnt(vrr->game_val.vtotal_fixed);
	vrr_cur_vtotal_debug(vrr->frame_cnt, m_const, vrr->game_val.vtotal_fixed);
	vrr->frame_cnt++;
}

irqreturn_t hdmitx_vrr_vsync_handler(struct hdmitx_dev *hdev)
{
	static bool m_const;
	struct tx_vrr_params *vrr = &vrr_para;
	u16 vtotal_tmp = 0;

	if (!vrr->conf_params.vrr_enabled)
		return IRQ_HANDLED;

	/* for vsync debug only */
	if (vrr_dbg_vframe == 0)
		return IRQ_HANDLED;
	if (vrr_dbg_vframe > 0)
		vrr_dbg_vframe--;

	/* if configuration changed then init local vrr variables */
	if (is_vrrconf_changed(&vrr->conf_params)) {
		m_const = 0;
		vrr_init_para(vrr);
	}

	if (vrr->conf_params.type == T_VRR_GAME) {
		hdmitx_vrr_game_handler(hdev);
		return IRQ_HANDLED;
	}

	/* For QMS-VRR */
	if (vrr->fapa_early_cnt) {
		vrr->fapa_early_cnt--;
		m_const = 0;
		hdmi_emp_frame_set_member(&vrr->emp_vrr_pkt, CONF_M_CONST,
			m_const);
		hdmi_emp_infoframe_set(EMP_TYPE_VRR_QMS, &vrr->emp_vrr_pkt);
		vrr_cur_vtotal_debug(vrr->frame_cnt, m_const, vrr->mconst_val->vtotal_fixed);
		return IRQ_HANDLED;
	}

	if (vrr->conf_params.mdelta_bit == 0)
		m_const = 1;

	if (m_const == 1) {
		vtotal_tmp = calc_cur_vtotal(vrr->mconst_val, vrr->frame_cnt);
	} else {
		if (vrr->conf_params.mdelta_bit)
			vtotal_tmp = calc_vtotal_with_mdelta(vrr);
	}

	if (vtotal_tmp == vrr->mconst_val->vtotal_fixed)
		m_const = 1;

	hdmi_emp_frame_set_member(&vrr->emp_vrr_pkt, CONF_M_CONST, m_const);
	hdmi_emp_infoframe_set(EMP_TYPE_VRR_GAME, &vrr->emp_vrr_pkt);
	hdmitx_vrr_set_maxlncnt(vtotal_tmp);
	vrr_cur_vtotal_debug(vrr->frame_cnt, m_const, vrr->mconst_val->vtotal_fixed);
	/* the frame count will add 1 at end */
	if (m_const == 1)
		vrr->frame_cnt++;
	return IRQ_HANDLED;
}

void hdmitx_vrr_disable(void)
{
	struct tx_vrr_params *vrr = &vrr_para;
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	if (hdev->irq_vrr_vsync)
		vrr->conf_params.vrr_enabled = 0;
}

void hdmitx_vrr_enable(void)
{
	struct tx_vrr_params *vrr = &vrr_para;
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	if (hdev->irq_vrr_vsync)
		vrr->conf_params.vrr_enabled = 1;
}
