// SPDX-License-Identifier: GPL-2.0
/*
 * Sony IMX290 CMOS Image Sensor Driver
 *
 * Copyright (C) 2019 FRAMOS GmbH.
 *
 * Copyright (C) 2019 Linaro Ltd.
 * Author: Manivannan Sadhasivam <manivannan.sadhasivam@linaro.org>
 */
#include <linux/version.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <media/media-entity.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-fwnode.h>
#include <media/v4l2-subdev.h>
#include <linux/of_platform.h>
#include <linux/of_graph.h>

#include "../include/mclk_api.h"

#define IMX290_STANDBY    0x3000
#define IMX290_REGHOLD    0x3001
#define IMX290_XMSTA      0x3002
#define IMX290_GAIN       0x3014
#define IMX290_EXPOSURE   0x3020
#define IMX290_ID         0xb201

#define IMX290_BLKLEVEL_LOW  0x300a
#define IMX290_BLKLEVEL_HIGH 0x300b

#define IMX290_HMAX_LOW  0x301c
#define IMX290_HMAX_HIGH 0x301d

#define IMX290_PGCTRL     0x308c

#define IMX290_FR_FDG_SEL    0x3009
#define IMX290_PHY_LANE_NUM  0x3407
#define IMX290_CSI_LANE_MODE 0x3443

#define AML_SENSOR_NAME  "imx290-%u"

#define IMX290_60HZ

struct imx290_regval {
	u16 reg;
	u8 val;
};

struct imx290_mode {
	u32 width;
	u32 height;
	u32 hmax;
	u32 link_freq_index;

	const struct imx290_regval *data;
	u32 data_size;
};

struct imx290 {
	int index;
	struct device *dev;
	struct clk *xclk;
	struct regmap *regmap;
	u8 nlanes;
	u8 bpp;
	u32 enWDRMode;

	struct i2c_client *client;
	struct v4l2_subdev sd;
	struct v4l2_fwnode_endpoint ep;
	struct media_pad pad;
	struct v4l2_mbus_framefmt current_format;
	const struct imx290_mode *current_mode;

	struct gpio_desc *rst_gpio;
	struct gpio_desc *pwdn_gpio;
	struct gpio_desc *power_gpio;

	struct v4l2_ctrl_handler ctrls;
	struct v4l2_ctrl *link_freq;
	struct v4l2_ctrl *pixel_rate;
	struct v4l2_ctrl *wdr;
	struct v4l2_ctrl *fps;

	int status;
	struct mutex lock;

	int flag_60hz;
};

struct imx290_pixfmt {
	u32 code;
	u32 min_width;
	u32 max_width;
	u32 min_height;
	u32 max_height;
	u8 bpp;
};

static const struct imx290_pixfmt imx290_formats[] = {
	//30hz
	{ MEDIA_BUS_FMT_SRGGB10_1X10, 1280, 1920, 720, 1080, 10 },
	{ MEDIA_BUS_FMT_SRGGB12_1X12, 1280, 1920, 720, 1080, 12 },
	//60hz sdr
	{ MEDIA_BUS_FMT_SGBRG10_1X10, 1280, 1920, 720, 1080, 10 },
};

static const struct regmap_config imx290_regmap_config = {
	.reg_bits = 16,
	.val_bits = 8,
	.cache_type = REGCACHE_RBTREE,
};

//T7 default
static const struct imx290_regval imx290_global_init_settings_60hz[] = {
	{0x3000, 0x01},
	{0x3002, 0x00},
	{0x3005, 0x00},
	{0x3007, 0x00},
	{0x3009, 0x01},
	{0x300a, 0x3c},
	{0x300f, 0x00},
	{0x3010, 0x21},
	{0x3012, 0x64},
	{0x3014, 0x00},
	{0x3016, 0x09},
	{0x3018, 0xDF},
	{0x3019, 0x04},
	{0x301c, 0xEC},
	{0x301d, 0x07},
	{0x3020, 0x02},
	{0x3021, 0x01},
	{0x3022, 0x00},
	{0x3046, 0x00},
	{0x304b, 0x0a},
	{0x3418, 0x49},
	{0x3419, 0x04},
	{0x305c, 0x18},
	{0x305d, 0x03},
	{0x305e, 0x20},
	{0x305f, 0x01},
	{0x3070, 0x02},
	{0x3071, 0x11},
	{0x309b, 0x10},
	{0x309c, 0x22},
	{0x30a2, 0x02},
	{0x30a6, 0x20},
	{0x30a8, 0x20},
	{0x30aa, 0x20},
	{0x30ac, 0x20},
	{0x30b0, 0x43},
	{0x3106, 0x00},
	{0x3119, 0x9e},
	{0x311c, 0x1e},
	{0x311e, 0x08},
	{0x3128, 0x05},
	{0x3129, 0x1d},
	{0x313d, 0x83},
	{0x3150, 0x03},
	{0x315e, 0x1a},
	{0x3164, 0x1a},
	{0x317c, 0x12},
	{0x317e, 0x00},
	{0x31ec, 0x37},
	{0x32b8, 0x50},
	{0x32b9, 0x10},
	{0x32ba, 0x00},
	{0x32bb, 0x04},
	{0x32c8, 0x50},
	{0x32c9, 0x10},
	{0x32ca, 0x00},
	{0x32cb, 0x04},
	{0x332c, 0xd3},
	{0x332d, 0x10},
	{0x332e, 0x0d},
	{0x3358, 0x06},
	{0x3359, 0xe1},
	{0x335a, 0x11},
	{0x3360, 0x1e},
	{0x3361, 0x61},
	{0x3362, 0x10},
	{0x33b0, 0x50},
	{0x33b2, 0x1a},
	{0x33b3, 0x04},
	{0x3405, 0x10},
	{0x3407, 0x03},
	{0x3414, 0x0a},
	{0x3415, 0x00},
	{0x3441, 0x0a},
	{0x3442, 0x0a},
	{0x3443, 0x03},
	{0x3444, 0x20},
	{0x3445, 0x25},
	{0x3446, 0x57},
	{0x3447, 0x00},
	{0x3448, 0x37},
	{0x3449, 0x00},
	{0x344a, 0x1f},
	{0x344b, 0x00},
	{0x344c, 0x1f},
	{0x344d, 0x00},
	{0x344e, 0x1f},
	{0x344f, 0x00},
	{0x3450, 0x77},
	{0x3451, 0x00},
	{0x3452, 0x1f},
	{0x3453, 0x00},
	{0x3454, 0x17},
	{0x3455, 0x00},
	{0x3472, 0x9c},
	{0x3473, 0x07},
	{0x3480, 0x49},
	{0x3002, 0x00},

};

static const struct imx290_regval imx290_global_init_settings[] = {
	{0x3000, 0x01}, /* standby */

	{0x3005, 0x01}, //0:10bit 1:12bit
	{0x3007, 0x00}, //full hd 1080p
	{0x3009, 0x12},
	{0x300a, 0xF0}, //black level
	{0x300B, 0x00},
	{0x300c, 0x00},
	{0x300F, 0x00},
	{0x3010, 0x21},
	{0x3012, 0x64},
	{0x3013, 0x00},
	{0x3014, 0x02},//Gain
	{0x3016, 0x09},
	{0x3018, 0x85}, /* VMAX[7:0] */
	{0x3019, 0x04}, /* VMAX[15:8] */
	{0x301a, 0x00}, /* VMAX[16] */
	{0x301b, 0x00},
	{0x301c, 0x30}, /* HMAX[7:0] */
	{0x301d, 0x11}, /* HMAX[15:8] */
	{0x3020, 0x81},//SHS1
	{0x3021, 0x01},
	{0x3022, 0x00},//SHS1
	{0x3024, 0x00},//SHS2
	{0x3025, 0x00},//SHS2
	{0x3026, 0x00},//SHS2
	{0x3030, 0x00},//RHS1
	{0x3031, 0x00},//RHS1
	{0x3032, 0x00},//RHS1
	{0x3045, 0x01},//DOL
	{0x3046, 0xe1},//LANE CHN
	{0x304b, 0x00},
	//{0x3418, 0x 1},//Y_out size, tools should modify from B2 to 9C
	//{0x3419, 0x 1},//Y_out size

	{0x305C, 0x18},
	{0x305D, 0x03},
	{0x305E, 0x20},
	{0x305F, 0x01},

	{0x3070, 0x02}, //must set
	{0x3071, 0x11},
	{0x309B, 0x10},
	{0x309C, 0x22},
	{0x30A2, 0x02},
	{0x30A6, 0x20},
	{0x30A8, 0x20},
	{0x30AA, 0x20},
	{0x30AC, 0x20},
	{0x30B0, 0x43},

	{0x3106, 0x00}, //Need double confirm, H company 11h, 8/3th version
	{0x3119, 0x9e},
	{0x311c, 0x1e},
	{0x311e, 0x08},

	{0x3128, 0x05},

	{0x3129, 0x00},

	{0x313D, 0x83},
	{0x3150, 0x03},
	{0x315E, 0x1A},// 1A:37.125MHz 1B:74.25MHz
	{0x3164, 0x1A},// 1A:37.125MHz 1B:74.25MHz
	{0x317C, 0x00},
	{0x317E, 0x00},

	{0x31EC, 0x0E},

	{0x32B8, 0x50},
	{0x32B9, 0x10},
	{0x32BA, 0x00},
	{0x32BB, 0x04},
	{0x32C8, 0x50},
	{0x32C9, 0x10},
	{0x32CA, 0x00},
	{0x32CB, 0x04},

	{0x332C, 0xD3},
	{0x332D, 0x10},
	{0x332E, 0x0D},
	{0x3358, 0x06},
	{0x3359, 0xE1},
	{0x335A, 0x11},
	{0x3360, 0x1E},
	{0x3361, 0x61},
	{0x3362, 0x10},
	{0x33B0, 0x50},
	{0x33B2, 0x1A},
	{0x33B3, 0x04},

	{0x3405, 0x20},
	{0x3407, 0x03},
	{0x3414, 0x0A},
	{0x3415, 0x01},
	{0x3418, 0x49},
	{0x3419, 0x04},
	{0x3441, 0x0C},
	{0x3442, 0x0C},
	{0x3443, 0x03},
	{0x3444, 0x20},//mclk :37.125M
	{0x3445, 0x25},

	{0x3446, 0x47},//global timming
	{0x3447, 0x00},
	{0x3448, 0x1F},
	{0x3449, 0x00},
	{0x344A, 0x17},
	{0x344B, 0x00},
	{0x344C, 0x0F},
	{0x344D, 0x00},
	{0x344E, 0x17},
	{0x344F, 0x00},
	{0x3450, 0x47},
	{0x3451, 0x00},
	{0x3452, 0x0F},
	{0x3453, 0x00},
	{0x3454, 0x0F},
	{0x3455, 0x00},

	{0x3472, 0x9C},
	{0x3473, 0x07},
	{0x347B, 0x24},//add
	{0x3480, 0x49},

	{0x3002, 0x00}, /* master mode start */

	{0x3049, 0x0A}, /* XVSOUTSEL XHSOUTSEL */

};

static struct imx290_regval dol_1080p_30fps_4lane_10bits[] = {
	{0x3000, 0x01 }, /* standby */

	{0x3002, 0x00 }, /* XTMSTA */

	{0x3005, 0x00 },
	{0x3007, 0x00 },
	{0x3009, 0x01 },
	{0x300a, 0x3c },
	{0x300c, 0x11 },
	{0x300f, 0x00 },
	{0x3010, 0x21 },
	{0x3012, 0x64 },
	{0x3014, 0x02 },
	{0x3016, 0x09 },
	{0x3018, 0xC4 },//VMAX change from 0465 to 04C4
	{0x3019, 0x04 },//VMAX

	{0x301c, 0xEC },//* HMAX */ change from 0898 to 07EC
	{0x301d, 0x07 },//* HMAX */
	{0x3020, 0x3c },//SHS1
	{0x3021, 0x01 },//SHS1
	{0x3022, 0x00 },//SHS1
	{0x3024, 0xcb },//SHS2
	{0x3025, 0x00 },//SHS2
	{0x3026, 0x00 },//SHS2
	{0x3030, 0xc9 },//RHS1
	{0x3031, 0x00 },//RHS1
	{0x3032, 0x00 },//RHS1
	{0x3045, 0x05 },//DOL
	{0x3046, 0x00 },//Datasheet should modify, Tools should modify
	{0x304b, 0x0a },
	{0x3418, 0x5e },//Y_out size, tools should modify from B2 to 9C
	{0x3419, 0x09 },//Y_out size

	{0x305c, 0x18 },
	{0x305d, 0x03 },
	{0x305e, 0x20 },
	{0x305f, 0x01 },

	{0x3070, 0x02 },
	{0x3071, 0x11 },

	{0x309b, 0x10 },
	{0x309c, 0x22 },

	{0x30a2, 0x02 },
	{0x30a6, 0x20 },
	{0x30a8, 0x20 },
	{0x30aa, 0x20 },
	{0x30ac, 0x20 },
	{0x30b0, 0x43 },

	{0x3106, 0x11 }, //Need double confirm, H company 11h, 8/3th version
	{0x3119, 0x9e },
	{0x311c, 0x1e },
	{0x311e, 0x08 },

	{0x3128, 0x05 },
	{0x3129, 0x1d },
	{0x313d, 0x83 },
	{0x3150, 0x03 },
	{0x315e, 0x1a },
	{0x3164, 0x1a },
	{0x317c, 0x12 },
	{0x317e, 0x00 },
	{0x31ec, 0x37 },

	{0x32b8, 0x50 },
	{0x32b9, 0x10 },
	{0x32ba, 0x00 },
	{0x32bb, 0x04 },

	{0x32c8, 0x50 },
	{0x32c9, 0x10 },
	{0x32ca, 0x00 },
	{0x32cb, 0x04 },

	{0x332c, 0xd3 },
	{0x332d, 0x10 },
	{0x332e, 0x0d },

	{0x3358, 0x06 },
	{0x3359, 0xe1 },
	{0x335a, 0x11 },

	{0x3360, 0x1e },
	{0x3361, 0x61 },
	{0x3362, 0x10 },

	{0x33b0, 0x50 },
	{0x33b2, 0x1a },
	{0x33b3, 0x04 },

	{0x3405, 0x10 },
	{0x3407, 0x03 },
	{0x3414, 0x0a },
	{0x3415, 0x00 },

	{0x3441, 0x0a },
	{0x3442, 0x0a },
	{0x3443, 0x03 },
	{0x3444, 0x20 },
	{0x3445, 0x25 },
	{0x3446, 0x57 },
	{0x3447, 0x00 },
	{0x3448, 0x37 },
	{0x3449, 0x00 },
	{0x344a, 0x1f },

	{0x344b, 0x00 },
	{0x344c, 0x1f },
	{0x344d, 0x00 },

	{0x344e, 0x1f },
	{0x344f, 0x00 },
	{0x3450, 0x77 },
	{0x3451, 0x00 },

	{0x3452, 0x1f },
	{0x3453, 0x00 },
	{0x3454, 0x17 },
	{0x3455, 0x00 },

	{0x3472, 0xA0 },//Xout size from 079c to 07A0,8/3th's info
	{0x3473, 0x07 },
	{0x347B, 0x23 },//add
	{0x3480, 0x49 },

	{0x3002, 0x00 }, /* master mode start */
};

static const struct imx290_regval imx290_1080p_settings[] = {
	/* mode settings */
	{ 0x3007, 0x00 },
	{ 0x303a, 0x0c },
	{ 0x3414, 0x0a },
	{ 0x3472, 0x80 },
	{ 0x3473, 0x07 },
	{ 0x3418, 0x38 },// vmax
	{ 0x3419, 0x04 },// vmax
	{ 0x3012, 0x64 },
	{ 0x3013, 0x00 },
	{ 0x305c, 0x18 },
	{ 0x305d, 0x03 },
	{ 0x305e, 0x20 },
	{ 0x305f, 0x01 },
	{ 0x315e, 0x1a },
	{ 0x3164, 0x1a },
	{ 0x3480, 0x49 },
	/* data rate settings */
	//{ 0x3009, 0x01 },// fr fdg sel lane related 60/50 fps
	{ 0x3405, 0x10 },
	{ 0x3446, 0x57 },
	{ 0x3447, 0x00 },
	{ 0x3448, 0x37 },
	{ 0x3449, 0x00 },
	{ 0x344a, 0x1f },
	{ 0x344b, 0x00 },
	{ 0x344c, 0x1f },
	{ 0x344d, 0x00 },
	{ 0x344e, 0x1f },
	{ 0x344f, 0x00 },
	{ 0x3450, 0x77 },
	{ 0x3451, 0x00 },
	{ 0x3452, 0x1f },
	{ 0x3453, 0x00 },
	{ 0x3454, 0x17 },
	{ 0x3455, 0x00 },
	//{ 0x301c, 0x98 },// hmax low
	//{ 0x301d, 0x08 },// hmax high
};

static const struct imx290_regval imx290_720p_settings[] = {
	/* mode settings */
	{ 0x3007, 0x10 },
	{ 0x303a, 0x06 },
	{ 0x3414, 0x04 },
	{ 0x3472, 0x00 },
	{ 0x3473, 0x05 },
	{ 0x3418, 0xd0 },
	{ 0x3419, 0x02 },
	{ 0x3012, 0x64 },
	{ 0x3013, 0x00 },
	{ 0x305c, 0x20 },
	{ 0x305d, 0x00 },
	{ 0x305e, 0x20 },
	{ 0x305f, 0x01 },
	{ 0x315e, 0x1a },
	{ 0x3164, 0x1a },
	{ 0x3480, 0x49 },
	/* data rate settings */
	{ 0x3009, 0x01 },
	{ 0x3405, 0x10 },
	{ 0x3446, 0x4f },
	{ 0x3447, 0x00 },
	{ 0x3448, 0x2f },
	{ 0x3449, 0x00 },
	{ 0x344a, 0x17 },
	{ 0x344b, 0x00 },
	{ 0x344c, 0x17 },
	{ 0x344d, 0x00 },
	{ 0x344e, 0x17 },
	{ 0x344f, 0x00 },
	{ 0x3450, 0x57 },
	{ 0x3451, 0x00 },
	{ 0x3452, 0x17 },
	{ 0x3453, 0x00 },
	{ 0x3454, 0x17 },
	{ 0x3455, 0x00 },
	{ 0x301c, 0xe4 },
	{ 0x301d, 0x0c },
};

static const struct imx290_regval imx290_10bit_settings[] = {
	{ 0x3005, 0x00},
	{ 0x3046, 0x00},
	{ 0x3129, 0x1d},
	{ 0x317c, 0x12},
	{ 0x31ec, 0x37},
	{ 0x3441, 0x0a},
	{ 0x3442, 0x0a},
	{ 0x300a, 0x3c},
	{ 0x300b, 0x00},
};


static const struct imx290_regval imx290_12bit_settings[] = {
	{ 0x3005, 0x01 },
	{ 0x3046, 0x01 },
	{ 0x3129, 0x00 },
	{ 0x317c, 0x00 },
	{ 0x31ec, 0x0e },
	{ 0x3441, 0x0c },
	{ 0x3442, 0x0c },
	{ 0x300a, 0xf0 },
	{ 0x300b, 0x00 },
};

/* supported link frequencies */
#define FREQ_INDEX_1080P	0
#define FREQ_INDEX_720P		1
#define FREQ_INDEX_1080P_60HZ	2

/* supported link frequencies */
static const s64 imx290_link_freq_2lanes[] = {
	[FREQ_INDEX_1080P] = 445500000,
	[FREQ_INDEX_720P] = 297000000,
};

static const s64 imx290_link_freq_4lanes[] = {
	[FREQ_INDEX_1080P] = 222750000,
	[FREQ_INDEX_720P] = 148500000,
	[FREQ_INDEX_1080P_60HZ] = 445500000,
};

//static const s64 *imx290_link_freq_4lanes = imx290_link_freq_4lanes_30hz;

static inline const s64 *imx290_link_freqs_ptr(const struct imx290 *imx290)
{
	if (imx290->nlanes == 2)
		return imx290_link_freq_2lanes;
	else {
		return imx290_link_freq_4lanes;
	}
}

static inline int imx290_link_freqs_num(const struct imx290 *imx290)
{
	if (imx290->nlanes == 2)
		return ARRAY_SIZE(imx290_link_freq_2lanes);
	else
		return ARRAY_SIZE(imx290_link_freq_4lanes);
}

/* Mode configs */
static const struct imx290_mode imx290_modes_2lanes[] = {
	{
		.width = 1920,
		.height = 1080,
		.hmax  = 0x1130,
		.data = imx290_1080p_settings,
		.data_size = ARRAY_SIZE(imx290_1080p_settings),

		.link_freq_index = FREQ_INDEX_1080P,
	},
	{
		.width = 1280,
		.height = 720,
		.hmax = 0x19c8,
		.data = imx290_720p_settings,
		.data_size = ARRAY_SIZE(imx290_720p_settings),

		.link_freq_index = FREQ_INDEX_720P,
	},
};


static const struct imx290_mode imx290_modes_4lanes[] = {
	{
		.width = 1920,
		.height = 1080,
		.hmax = 0x0898,
		.link_freq_index = FREQ_INDEX_1080P,
		.data = imx290_1080p_settings,
		.data_size = ARRAY_SIZE(imx290_1080p_settings),
	},
	{
		.width = 1280,
		.height = 720,
		.hmax = 0x0ce4,
		.link_freq_index = FREQ_INDEX_720P,
		.data = imx290_720p_settings,
		.data_size = ARRAY_SIZE(imx290_720p_settings),
	},
	{
		.width = 1920,
		.height = 1080,
		.hmax  = 0x1130,
		.link_freq_index = FREQ_INDEX_1080P_60HZ,
		.data = imx290_1080p_settings,
		.data_size = ARRAY_SIZE(imx290_1080p_settings),
	},
};

static inline const struct imx290_mode *imx290_modes_ptr(const struct imx290 *imx290)
{
	if (imx290->nlanes == 2)
		return imx290_modes_2lanes;
	else
		return imx290_modes_4lanes;
}

static inline int imx290_modes_num(const struct imx290 *imx290)
{
	if (imx290->nlanes == 2)
		return ARRAY_SIZE(imx290_modes_2lanes);
	else
		return ARRAY_SIZE(imx290_modes_4lanes);
}

static inline struct imx290 *to_imx290(struct v4l2_subdev *_sd)
{
	return container_of(_sd, struct imx290, sd);
}

static inline int imx290_read_reg(struct imx290 *imx290, u16 addr, u8 *value)
{
	unsigned int regval;

	int i, ret;

	for(i = 0; i < 3; ++i){
		ret = regmap_read(imx290->regmap, addr, &regval);
		if(0 == ret ){
			break;
		}
	}

	if (ret)
		dev_err(imx290->dev, "I2C read with i2c transfer failed for addr: %x, ret %d\n", addr, ret);

	*value = regval & 0xff;
	return 0;
}

static int imx290_write_reg(struct imx290 *imx290, u16 addr, u8 value)
{
	int i, ret;

	for (i = 0; i < 3; i++) {
		ret = regmap_write(imx290->regmap, addr, value);
		if (0 == ret) {
			break;
		}
	}

	if (ret)
		dev_err(imx290->dev, "I2C write failed for addr: %x, ret %d\n", addr, ret);

	return ret;
}

static int imx290_get_id(struct imx290 *imx290)
{
	int rtn = -EINVAL;
	u32 id = 0;
	u8 val = 0;

	imx290_read_reg(imx290, 0x301e, &val);
	id |= (val << 8);

	imx290_read_reg(imx290, 0x301f, &val);
	id |= val;

	if (id != IMX290_ID) {
		dev_err(imx290->dev, "Failed to get imx290 id: 0x%x\n", id);
		return rtn;
	} else {
		dev_err(imx290->dev, "success get imx290 id 0x%x", id);
	}

	return 0;
}

static int imx290_set_register_array(struct imx290 *imx290,
				     const struct imx290_regval *settings,
				     unsigned int num_settings)
{
	unsigned int i;
	int ret = 0;

	for (i = 0; i < num_settings; ++i, ++settings) {
		ret = imx290_write_reg(imx290, settings->reg, settings->val);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static int imx290_write_buffered_reg(struct imx290 *imx290, u16 address_low,
				     u8 nr_regs, u32 value)
{
	unsigned int i;
	int ret;

	ret = imx290_write_reg(imx290, IMX290_REGHOLD, 0x01);
	if (ret) {
		dev_err(imx290->dev, "Error setting hold register\n");
		return ret;
	}

	for (i = 0; i < nr_regs; i++) {
		ret = imx290_write_reg(imx290, address_low + i,
				       (u8)(value >> (i * 8)));
		if (ret) {
			dev_err(imx290->dev, "Error writing buffered registers\n");
			return ret;
		}
	}

	ret = imx290_write_reg(imx290, IMX290_REGHOLD, 0x00);
	if (ret) {
		dev_err(imx290->dev, "Error setting hold register\n");
		return ret;
	}

	return ret;
}

static int imx290_set_gain(struct imx290 *imx290, u32 value)
{
	int ret;

	ret = imx290_write_buffered_reg(imx290, IMX290_GAIN, 1, value);
	if (ret)
		dev_err(imx290->dev, "Unable to write gain\n");

	return ret;
}

static int imx290_set_exposure(struct imx290 *imx290, u32 value)
{
	int ret;

	ret = imx290_write_buffered_reg(imx290, IMX290_EXPOSURE, 2, value & 0xFFFF);
	if (ret)
		dev_err(imx290->dev, "Unable to write gain\n");

	if (imx290->enWDRMode)
		ret = imx290_write_buffered_reg(imx290, 0x3024, 2, (value >> 16) & 0xFFFF);

	return ret;
}


/* Stop streaming */
static int imx290_stop_streaming(struct imx290 *imx290)
{
	int ret;
	imx290->enWDRMode = WDR_MODE_NONE;

	ret = imx290_write_reg(imx290, IMX290_STANDBY, 0x01);
	if (ret < 0)
		return ret;

	msleep(30);

	return imx290_write_reg(imx290, IMX290_XMSTA, 0x01);
}

static int imx290_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct imx290 *imx290 = container_of(ctrl->handler,
					     struct imx290, ctrls);
	int ret = 0;

	/* V4L2 controls values will be applied only when power is already up */
	if (!pm_runtime_get_if_in_use(imx290->dev))
		return 0;

	switch (ctrl->id) {
	case V4L2_CID_GAIN:
		ret = imx290_set_gain(imx290, ctrl->val);
		break;
	case V4L2_CID_EXPOSURE:
		ret = imx290_set_exposure(imx290, ctrl->val);
		break;
	case V4L2_CID_HBLANK:
		break;
	case V4L2_CID_AML_MODE:
		imx290->enWDRMode = ctrl->val;
		break;
	case V4L2_CID_AML_USER_FPS:
		dev_err(imx290->dev,"set user fps\n");
		if (ctrl->val == 60) {
			imx290->flag_60hz = 1;
		} else {
			imx290->flag_60hz = 0;
		}
		break;
	default:
		dev_err(imx290->dev, "Error ctrl->id %u, flag 0x%lx\n",
			ctrl->id, ctrl->flags);
		ret = -EINVAL;
		break;
	}

	pm_runtime_put(imx290->dev);

	return ret;
}

static const struct v4l2_ctrl_ops imx290_ctrl_ops = {
	.s_ctrl = imx290_set_ctrl,
};
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
static int imx290_enum_mbus_code(struct v4l2_subdev *sd,
				  struct v4l2_subdev_state *cfg,
				 struct v4l2_subdev_mbus_code_enum *code)
#else
static int imx290_enum_mbus_code(struct v4l2_subdev *sd,
				 struct v4l2_subdev_pad_config *cfg,
				 struct v4l2_subdev_mbus_code_enum *code)
#endif
{
	if (code->index >= ARRAY_SIZE(imx290_formats))
		return -EINVAL;

	code->code = imx290_formats[code->index].code;

	return 0;
}
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
static int imx290_enum_frame_size(struct v4l2_subdev *sd,
			        struct v4l2_subdev_state *cfg,
			       struct v4l2_subdev_frame_size_enum *fse)
#else
static int imx290_enum_frame_size(struct v4l2_subdev *sd,
			       struct v4l2_subdev_pad_config *cfg,
			       struct v4l2_subdev_frame_size_enum *fse)
#endif
{
	if (fse->index >= ARRAY_SIZE(imx290_formats))
		return -EINVAL;

	fse->min_width = imx290_formats[fse->index].min_width;
	fse->min_height = imx290_formats[fse->index].min_height;;
	fse->max_width = imx290_formats[fse->index].max_width;
	fse->max_height = imx290_formats[fse->index].max_height;

	return 0;
}
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
static int imx290_get_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_state *cfg,
			  struct v4l2_subdev_format *fmt)
#else
static int imx290_get_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_pad_config *cfg,
			  struct v4l2_subdev_format *fmt)
#endif
{
	struct imx290 *imx290 = to_imx290(sd);
	struct v4l2_mbus_framefmt *framefmt;

	mutex_lock(&imx290->lock);

	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY)
		framefmt = v4l2_subdev_get_try_format(&imx290->sd, cfg,
						      fmt->pad);
	else
		framefmt = &imx290->current_format;

	fmt->format = *framefmt;

	mutex_unlock(&imx290->lock);

	return 0;
}


static inline u8 imx290_get_link_freq_index(struct imx290 *imx290)
{
	return imx290->current_mode->link_freq_index;
}

static s64 imx290_get_link_freq(struct imx290 *imx290)
{
	u8 index = imx290_get_link_freq_index(imx290);

	return *(imx290_link_freqs_ptr(imx290) + index);
}

static u64 imx290_calc_pixel_rate(struct imx290 *imx290)
{
	s64 link_freq = imx290_get_link_freq(imx290);
	u8 nlanes = imx290->nlanes;
	u64 pixel_rate;

	/* pixel rate = link_freq * 2 * nr_of_lanes / bits_per_sample */
	pixel_rate = link_freq * 2 * nlanes;
	do_div(pixel_rate, imx290->bpp);
	return pixel_rate;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
static int imx290_set_fmt(struct v4l2_subdev *sd,
			struct v4l2_subdev_state *cfg,
			struct v4l2_subdev_format *fmt)
#else
static int imx290_set_fmt(struct v4l2_subdev *sd,
			struct v4l2_subdev_pad_config *cfg,
			struct v4l2_subdev_format *fmt)
#endif
{
	struct imx290 *imx290 = to_imx290(sd);
	const struct imx290_mode *mode;
	struct v4l2_mbus_framefmt *format;
	unsigned int i,ret;

	mutex_lock(&imx290->lock);
	if (imx290->flag_60hz == 1) {
		mode = &imx290_modes_4lanes[2];
	} else {
		mode = v4l2_find_nearest_size(imx290_modes_ptr(imx290),
					 imx290_modes_num(imx290),
					width, height,
					fmt->format.width, fmt->format.height);
	}
	fmt->format.width = mode->width;
	fmt->format.height = mode->height;

	for (i = 0; i < ARRAY_SIZE(imx290_formats); i++) {
		if (imx290_formats[i].code == fmt->format.code) {
			dev_err(imx290->dev, " zzw find proper format %d \n",i);
			break;
		}
	}

	if (i >= ARRAY_SIZE(imx290_formats)) {
		i = 0;
		dev_err(imx290->dev, " zzw No format. reset i = 0 \n");
	}

	fmt->format.code = imx290_formats[i].code;
	fmt->format.field = V4L2_FIELD_NONE;

	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
		dev_err(imx290->dev, " zzw try format \n");
		format = v4l2_subdev_get_try_format(sd, cfg, fmt->pad);
		mutex_unlock(&imx290->lock);
		return 0;
	} else {
		dev_err(imx290->dev, " zzw set format, w %d, h %d, code 0x%x \n",
            fmt->format.width, fmt->format.height,
            fmt->format.code);
		format = &imx290->current_format;
		imx290->current_mode = mode;
		imx290->bpp = imx290_formats[i].bpp;

		if (imx290->link_freq)
			__v4l2_ctrl_s_ctrl(imx290->link_freq, imx290_get_link_freq_index(imx290) );
		if (imx290->pixel_rate)
			__v4l2_ctrl_s_ctrl_int64(imx290->pixel_rate, imx290_calc_pixel_rate(imx290) );
	}

	*format = fmt->format;
	imx290->status = 0;

	mutex_unlock(&imx290->lock);
	if (imx290->enWDRMode) {
		/* Set init register settings */
		ret = imx290_set_register_array(imx290, dol_1080p_30fps_4lane_10bits,
				ARRAY_SIZE(dol_1080p_30fps_4lane_10bits));
		if (ret < 0) {
			dev_err(imx290->dev, "Could not set init registers\n");
			return ret;
		} else
			dev_err(imx290->dev, "imx290 wdr mode init...\n");
	} else {
		/* Set init register settings */
		if (imx290->flag_60hz) {
			ret = imx290_set_register_array(imx290, imx290_global_init_settings_60hz,
			ARRAY_SIZE(imx290_global_init_settings_60hz));
		} else {
			ret = imx290_set_register_array(imx290, imx290_global_init_settings,
				ARRAY_SIZE(imx290_global_init_settings));
		}
		if (ret < 0) {
			dev_err(imx290->dev, "Could not set init registers\n");
			return ret;
		} else
			dev_err(imx290->dev, "imx290 linear mode init...\n");
	}
	return 0;
}
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
int imx290_get_selection(struct v4l2_subdev *sd,
			     struct v4l2_subdev_state *cfg,
			     struct v4l2_subdev_selection *sel)
#else
int imx290_get_selection(struct v4l2_subdev *sd,
			     struct v4l2_subdev_pad_config *cfg,
			     struct v4l2_subdev_selection *sel)
#endif
{
	int rtn = 0;
	struct imx290 *imx290 = to_imx290(sd);
	const struct imx290_mode *mode = imx290->current_mode;

	switch (sel->target) {
	case V4L2_SEL_TGT_CROP_DEFAULT:
		sel->r.left = 0;
		sel->r.top = 0;
		sel->r.width = mode->width;
		sel->r.height = mode->height;
	break;
	case V4L2_SEL_TGT_CROP:
		sel->r.left = 0;
		sel->r.top = 0;
		sel->r.width = mode->width;
		sel->r.height = mode->height;
	break;
	default:
		rtn = -EINVAL;
		dev_err(imx290->dev, "Error support target: 0x%x\n", sel->target);
	break;
	}

	return rtn;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
static int imx290_entity_init_cfg(struct v4l2_subdev *subdev,
				  struct v4l2_subdev_state *cfg)
#else
static int imx290_entity_init_cfg(struct v4l2_subdev *subdev,
				  struct v4l2_subdev_pad_config *cfg)
#endif
{
	struct v4l2_subdev_format fmt = { 0 };

	fmt.which = cfg ? V4L2_SUBDEV_FORMAT_TRY : V4L2_SUBDEV_FORMAT_ACTIVE;
	fmt.format.width = 1920;
	fmt.format.height = 1080;

	imx290_set_fmt(subdev, cfg, &fmt);

	return 0;
}

/* Start streaming */
static int imx290_start_streaming(struct imx290 *imx290)
{
	int ret = 0;

	ret = imx290_write_reg(imx290, IMX290_STANDBY, 0x00);
	if (ret < 0)
		return ret;

	msleep(30);

	/* Start streaming */
	return imx290_write_reg(imx290, IMX290_XMSTA, 0x00);
}

static int imx290_set_stream(struct v4l2_subdev *sd, int enable)
{
	struct imx290 *imx290 = to_imx290(sd);
	int ret = 0;

	if (imx290->status == enable)
		return ret;
	else
		imx290->status = enable;

	if (enable) {
		ret = imx290_start_streaming(imx290);
		if (ret) {
			dev_err(imx290->dev, "Start stream failed\n");
			goto unlock_and_return;
		}

		dev_info(imx290->dev, "stream on\n");
	} else {
		imx290_stop_streaming(imx290);

		dev_info(imx290->dev, "stream off\n");
	}

unlock_and_return:

	return ret;
}


static int imx290_set_data_lanes(struct imx290 *imx290)
{
	int ret = 0, laneval, frsel;

	switch (imx290->nlanes) {
	case 2:
		laneval = 0x01;
		frsel = 0x02;
		break;
	case 4:
		laneval = 0x03;
		frsel = 0x01;
		break;
	default:
		/*
		 * We should never hit this since the data lane count is
		 * validated in probe itself
		 */
		dev_err(imx290->dev, "Lane configuration not supported\n");
		ret = -EINVAL;
		goto exit;
	}

	ret = imx290_write_reg(imx290, IMX290_PHY_LANE_NUM, laneval);
	if (ret) {
		dev_err(imx290->dev, "Error setting Physical Lane number register\n");
		goto exit;
	}

	ret = imx290_write_reg(imx290, IMX290_CSI_LANE_MODE, laneval);
	if (ret) {
		dev_err(imx290->dev, "Error setting CSI Lane mode register\n");
		goto exit;
	}

	ret = imx290_write_reg(imx290, IMX290_FR_FDG_SEL, frsel);
	if (ret)
		dev_err(imx290->dev, "Error setting FR/FDG SEL register\n");

exit:
	return ret;
}

static int imx290_power_on(struct imx290 *imx290)
{
	int ret;

	reset_am_enable(imx290->dev,"reset", 1);

	ret = mclk_enable(imx290->dev,37125000);
	if (ret < 0 )
		dev_err(imx290->dev, "set mclk fail\n");
	udelay(30);

	// 30ms
	usleep_range(30000, 31000);

	/* Set data lane count */
	imx290_set_data_lanes(imx290);

	return 0;
}

static int imx290_power_off(struct imx290 *imx290)
{
	mclk_disable(imx290->dev);

	reset_am_enable(imx290->dev,"reset", 0);

	return 0;
}

static int imx290_power_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct imx290 *imx290 = to_imx290(sd);

	reset_am_enable(imx290->dev,"reset", 0);

	return 0;
}

static int imx290_power_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct imx290 *imx290 = to_imx290(sd);

	reset_am_enable(imx290->dev,"reset", 1);

	return 0;
}

static int imx290_log_status(struct v4l2_subdev *sd)
{
	struct imx290 *imx290 = to_imx290(sd);

	dev_info(imx290->dev, "log status done\n");

	return 0;
}

int imx290_sbdev_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh) {
	struct imx290 *imx290 = to_imx290(sd);
	imx290_power_on(imx290);
	return 0;
}
int imx290_sbdev_close(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh) {
	struct imx290 *imx290 = to_imx290(sd);
	imx290_set_stream(sd, 0);
	imx290_power_off(imx290);
	return 0;
}

static const struct dev_pm_ops imx290_pm_ops = {
	SET_RUNTIME_PM_OPS(imx290_power_suspend, imx290_power_resume, NULL)
};

const struct v4l2_subdev_core_ops imx290_core_ops = {
	.log_status = imx290_log_status,
};

static const struct v4l2_subdev_video_ops imx290_video_ops = {
	.s_stream = imx290_set_stream,
};

static const struct v4l2_subdev_pad_ops imx290_pad_ops = {
	.init_cfg = imx290_entity_init_cfg,
	.enum_mbus_code = imx290_enum_mbus_code,
	.enum_frame_size = imx290_enum_frame_size,
	.get_selection = imx290_get_selection,
	.get_fmt = imx290_get_fmt,
	.set_fmt = imx290_set_fmt,
};

static struct v4l2_subdev_internal_ops imx290_internal_ops = {
	.open = imx290_sbdev_open,
	.close = imx290_sbdev_close,
};

static const struct v4l2_subdev_ops imx290_subdev_ops = {
	.core = &imx290_core_ops,
	.video = &imx290_video_ops,
	.pad = &imx290_pad_ops,
};

static const struct media_entity_operations imx290_subdev_entity_ops = {
	.link_validate = v4l2_subdev_link_validate,
};

static struct v4l2_ctrl_config wdr_cfg = {
	.ops = &imx290_ctrl_ops,
	.id = V4L2_CID_AML_MODE,
	.name = "wdr mode",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	.min = 0,
	.max = 2,
	.step = 1,
	.def = 0,
};

static struct v4l2_ctrl_config v4l2_ctrl_output_fps = {
	.ops = &imx290_ctrl_ops,
	.id = V4L2_CID_AML_USER_FPS,
	.name = "Sensor output fps",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.min = 0,
	.max = 120,
	.step = 1,
	.def = 30,
};

static int imx290_ctrls_init(struct imx290 *imx290)
{
	int rtn = 0;

	v4l2_ctrl_handler_init(&imx290->ctrls, 4);

	v4l2_ctrl_new_std(&imx290->ctrls, &imx290_ctrl_ops,
				V4L2_CID_GAIN, 0, 0xF0, 1, 0);

	v4l2_ctrl_new_std(&imx290->ctrls, &imx290_ctrl_ops,
				V4L2_CID_EXPOSURE, 0, 0x7fffffff, 1, 0);

	imx290->link_freq = v4l2_ctrl_new_int_menu(&imx290->ctrls,
					       &imx290_ctrl_ops,
					       V4L2_CID_LINK_FREQ,
					       imx290_link_freqs_num(imx290) - 1,
					       0, imx290_link_freqs_ptr(imx290) );

	if (imx290->link_freq)
		imx290->link_freq->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	imx290->pixel_rate = v4l2_ctrl_new_std(&imx290->ctrls,
					       &imx290_ctrl_ops,
					       V4L2_CID_PIXEL_RATE,
					       1, INT_MAX, 1,
					       imx290_calc_pixel_rate(imx290));

	imx290->wdr = v4l2_ctrl_new_custom(&imx290->ctrls, &wdr_cfg, NULL);
	imx290->fps = v4l2_ctrl_new_custom(&imx290->ctrls, &v4l2_ctrl_output_fps, NULL);

	imx290->sd.ctrl_handler = &imx290->ctrls;

	if (imx290->ctrls.error) {
		dev_err(imx290->dev, "Control initialization a error  %d\n",
			imx290->ctrls.error);
		rtn = imx290->ctrls.error;
	}

	return rtn;
}

static int imx290_parse_power(struct imx290 *imx290)
{
	int rtn = 0;

	imx290->rst_gpio = devm_gpiod_get_optional(imx290->dev,
						"reset",
						GPIOD_OUT_LOW);
	if (IS_ERR(imx290->rst_gpio)) {
		dev_err(imx290->dev, "Cannot get reset gpio\n");
		rtn = PTR_ERR(imx290->rst_gpio);
		goto err_return;
	}

	/*imx290->pwdn_gpio = devm_gpiod_get_optional(imx290->dev,
						"pwdn",
						GPIOD_OUT_LOW);
	if (IS_ERR(imx290->pwdn_gpio)) {
		dev_err(imx290->dev, "Cannot get pwdn gpio\n");
		rtn = PTR_ERR(imx290->pwdn_gpio);
		goto err_return;
	}*/

err_return:

	return rtn;
}

/*
 * Returns 0 if all link frequencies used by the driver for the given number
 * of MIPI data lanes are mentioned in the device tree, or the value of the
 * first missing frequency otherwise.
 */
static s64 imx290_check_link_freqs(const struct imx290 *imx290,
				   const struct v4l2_fwnode_endpoint *ep)
{
	int i, j;
	const s64 *freqs = imx290_link_freqs_ptr(imx290);
	int freqs_count = imx290_link_freqs_num(imx290);

	for (j = 0; j < ep->nr_of_link_frequencies; j++) {
		for (i = 0; i < freqs_count; i++) {
			if (freqs[i] == ep->link_frequencies[j]) {
				return 0;
			}
		}
		if (i == freqs_count)
			return ep->link_frequencies[j];
	}

	return 0;
}

static int imx290_parse_endpoint(struct imx290 *imx290)
{
	int rtn = 0;
	s64 fq;
	struct fwnode_handle *endpoint = NULL;
	struct device_node *node = NULL;

	//endpoint = fwnode_graph_get_next_endpoint(dev_fwnode(imx415->dev), NULL);
	for_each_endpoint_of_node(imx290->dev->of_node, node) {
		if (strstr(node->name, "imx290")) {
			endpoint = of_fwnode_handle(node);
			break;
		}
	}

	rtn = v4l2_fwnode_endpoint_alloc_parse(endpoint, &imx290->ep);
	fwnode_handle_put(endpoint);
	if (rtn) {
		dev_err(imx290->dev, "Parsing endpoint node failed\n");
		rtn = -EINVAL;
		goto err_return;
	}

	/* Only CSI2 is supported for now */
	if (imx290->ep.bus_type != V4L2_MBUS_CSI2_DPHY) {
		dev_err(imx290->dev, "Unsupported bus type, should be CSI2\n");
		rtn = -EINVAL;
		goto err_free;
	}

	imx290->nlanes = imx290->ep.bus.mipi_csi2.num_data_lanes;
	if (imx290->nlanes != 2 && imx290->nlanes != 4) {
		dev_err(imx290->dev, "Invalid data lanes: %d\n", imx290->nlanes);
		rtn = -EINVAL;
		goto err_free;
	}
	dev_info(imx290->dev, "Using %u data lanes\n", imx290->nlanes);

	if (!imx290->ep.nr_of_link_frequencies) {
		dev_err(imx290->dev, "link-frequency property not found in DT\n");
		rtn = -EINVAL;
		goto err_free;
	}

	/* Check that link frequences for all the modes are in device tree */
	fq = imx290_check_link_freqs(imx290, &imx290->ep);
	if (fq) {
		dev_err(imx290->dev, "Link frequency of %lld is not supported\n", fq);
		rtn = -EINVAL;
		goto err_free;
	}

	return rtn;

err_free:
	v4l2_fwnode_endpoint_free(&imx290->ep);
err_return:
	return rtn;
}


static int imx290_register_subdev(struct imx290 *imx290)
{
	int rtn = 0;

	v4l2_i2c_subdev_init(&imx290->sd, imx290->client, &imx290_subdev_ops);

	imx290->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	imx290->sd.dev = &imx290->client->dev;
	imx290->sd.internal_ops = &imx290_internal_ops;
	imx290->sd.entity.ops = &imx290_subdev_entity_ops;
	imx290->sd.entity.function = MEDIA_ENT_F_CAM_SENSOR;
	snprintf(imx290->sd.name, sizeof(imx290->sd.name), AML_SENSOR_NAME, imx290->index);

	imx290->pad.flags = MEDIA_PAD_FL_SOURCE;
	rtn = media_entity_pads_init(&imx290->sd.entity, 1, &imx290->pad);
	if (rtn < 0) {
		dev_err(imx290->dev, "Could not register media entity\n");
		goto err_return;
	}

	rtn = v4l2_async_register_subdev(&imx290->sd);
	if (rtn < 0) {
		dev_err(imx290->dev, "Could not register v4l2 device\n");
		goto err_return;
	}

err_return:
	return rtn;
}

static int imx290_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct imx290 *imx290;
	int ret = -EINVAL;


	imx290 = devm_kzalloc(dev, sizeof(*imx290), GFP_KERNEL);
	if (!imx290)
		return -ENOMEM;
	dev_err(dev, "i2c dev addr 0x%x, name %s \n", client->addr, client->name);

	imx290->dev = dev;
	imx290->client = client;

	imx290->regmap = devm_regmap_init_i2c(client, &imx290_regmap_config);
	if (IS_ERR(imx290->regmap)) {
		dev_err(dev, "Unable to initialize I2C\n");
		return -ENODEV;
	}

	if (of_property_read_u32(dev->of_node, "index", &imx290->index)) {
		dev_err(dev, "Failed to read sensor index. default to 0\n");
		imx290->index = 0;
	}


	ret = imx290_parse_endpoint(imx290);
	if (ret) {
		dev_err(imx290->dev, "Error parse endpoint\n");
		goto return_err;
	}

	ret = imx290_parse_power(imx290);
	if (ret) {
		dev_err(imx290->dev, "Error parse power ctrls\n");
		goto free_err;
	}

	mutex_init(&imx290->lock);

	/* Power on the device to match runtime PM state below */
	dev_err(dev, "bef get id. pwdn -0, reset - 1\n");

	ret = imx290_power_on(imx290);
	if (ret < 0) {
		dev_err(dev, "Could not power on the device\n");
		goto free_err;
	}


	ret = imx290_get_id(imx290);
	if (ret) {
		dev_err(dev, "Could not get id\n");
		imx290_power_off(imx290);
		goto free_err;
	}

	/*
	 * Initialize the frame format. In particular, imx290->current_mode
	 * and imx290->bpp are set to defaults: imx290_calc_pixel_rate() call
	 * below in imx290_ctrls_init relies on these fields.
	 */
	imx290_entity_init_cfg(&imx290->sd, NULL);

	ret = imx290_ctrls_init(imx290);
	if (ret) {
		dev_err(imx290->dev, "Error ctrls init\n");
		goto free_ctrl;
	}

	ret = imx290_register_subdev(imx290);
	if (ret) {
		dev_err(imx290->dev, "Error register subdev\n");
		goto free_entity;
	}

	v4l2_fwnode_endpoint_free(&imx290->ep);

	dev_info(imx290->dev, "probe done \n");

	return 0;

free_entity:
	media_entity_cleanup(&imx290->sd.entity);
free_ctrl:
	v4l2_ctrl_handler_free(&imx290->ctrls);
	mutex_destroy(&imx290->lock);
free_err:
	v4l2_fwnode_endpoint_free(&imx290->ep);
return_err:
	return ret;
}

static int imx290_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct imx290 *imx290 = to_imx290(sd);

	v4l2_async_unregister_subdev(sd);
	media_entity_cleanup(&sd->entity);
	v4l2_ctrl_handler_free(sd->ctrl_handler);

	mutex_destroy(&imx290->lock);

	imx290_power_off(imx290);

	return 0;
}

static const struct of_device_id imx290_of_match[] = {
	{ .compatible = "sony, imx290" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, imx290_of_match);

static struct i2c_driver imx290_i2c_driver = {
	.probe_new  = imx290_probe,
	.remove = imx290_remove,
	.driver = {
		.name  = "imx290",
		.pm = &imx290_pm_ops,
		.of_match_table = of_match_ptr(imx290_of_match),
	},
};

module_i2c_driver(imx290_i2c_driver);

MODULE_DESCRIPTION("Sony IMX290 CMOS Image Sensor Driver");
MODULE_AUTHOR("keke.li");
MODULE_AUTHOR("keke.li <keke.li@amlogic.com>");
MODULE_LICENSE("GPL v2");
