// SPDX-License-Identifier: (GPL-2.0+ OR MIT/
/*
 * imx585 driver
 *
 * Copyright (c) 2023 Shenzhen Wesion Technology Co.,Ltd. All rights reserved.
 */


#define DEBUG
#include <linux/clk.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>
#include <linux/sysfs.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/rk-camera-module.h>
#include <media/media-entity.h>
#include <media/v4l2-async.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-fwnode.h>
#include <media/v4l2-mediabus.h>
#include <linux/of_graph.h>
#include <linux/pinctrl/consumer.h>
#include <linux/rk-preisp.h>
#include <media/v4l2-fwnode.h>
#include <linux/of_graph.h>
#include "../platform/rockchip/isp/rkisp_tb_helper.h"

#define DRIVER_VERSION			KERNEL_VERSION(0, 0x01, 0x08)

#ifndef V4L2_CID_DIGITAL_GAIN
#define V4L2_CID_DIGITAL_GAIN		V4L2_CID_GAIN
#endif

#define MIPI_FREQ_1188M			1188000000
#define MIPI_FREQ_891M			891000000
#define MIPI_FREQ_446M			446000000
#define MIPI_FREQ_743M			743000000
#define MIPI_FREQ_297M			297000000
#define MIPI_FREQ_800M			800000000

#define IMX585_4LANES			4
#define IMX585_2LANES			2

#define IMX585_MAX_PIXEL_RATE		(MIPI_FREQ_1188M / 12 * 2 * IMX585_4LANES)
#define OF_CAMERA_HDR_MODE		"rockchip,camera-hdr-mode"

#define IMX585_XVCLK_FREQ_37M		37125000
#define IMX585_XVCLK_FREQ_27M		27000000
#define IMX585_XVCLK_FREQ_24M		24000000

/* TODO: Get the real chip id from reg */
#define CHIP_ID				0x32
#define IMX585_REG_CHIP_ID		0x30DC

#define IMX585_REG_CTRL_MODE		0x3000
#define IMX585_MODE_SW_STANDBY		BIT(0)
#define IMX585_MODE_STREAMING		0x0

#define IMX585_LF_GAIN_REG_H		0x306D
#define IMX585_LF_GAIN_REG_L		0x306C

#define IMX585_SF1_GAIN_REG_H		0x306F
#define IMX585_SF1_GAIN_REG_L		0x306E

#define IMX585_SF2_GAIN_REG_H		0x3071
#define IMX585_SF2_GAIN_REG_L		0x3070

#define IMX585_LF_EXPO_REG_H		0x3052
#define IMX585_LF_EXPO_REG_M		0x3051
#define IMX585_LF_EXPO_REG_L		0x3050

#define IMX585_SF1_EXPO_REG_H		0x3056
#define IMX585_SF1_EXPO_REG_M		0x3055
#define IMX585_SF1_EXPO_REG_L		0x3054

#define IMX585_SF2_EXPO_REG_H		0x305A
#define IMX585_SF2_EXPO_REG_M		0x3059
#define IMX585_SF2_EXPO_REG_L		0x3058

#define IMX585_RHS1_REG_H		0x3062
#define IMX585_RHS1_REG_M		0x3061
#define IMX585_RHS1_REG_L		0x3060
#define IMX585_RHS1_DEFAULT		0x004D

#define IMX585_RHS2_REG_H		0x3066
#define IMX585_RHS2_REG_M		0x3065
#define IMX585_RHS2_REG_L		0x3064
#define IMX585_RHS2_DEFAULT		0x004D

#define	IMX585_EXPOSURE_MIN		4
#define	IMX585_EXPOSURE_STEP		1
#define IMX585_VTS_MAX			0x7fff

#define IMX585_GAIN_MIN			0x00
#define IMX585_GAIN_MAX			0xf0
#define IMX585_GAIN_STEP		1
#define IMX585_GAIN_DEFAULT		0x00

#define IMX585_FETCH_GAIN_H(VAL)	(((VAL) >> 8) & 0x07)
#define IMX585_FETCH_GAIN_L(VAL)	((VAL) & 0xFF)

#define IMX585_FETCH_EXP_H(VAL)		(((VAL) >> 16) & 0x0F)
#define IMX585_FETCH_EXP_M(VAL)		(((VAL) >> 8) & 0xFF)
#define IMX585_FETCH_EXP_L(VAL)		((VAL) & 0xFF)

#define IMX585_FETCH_RHS1_H(VAL)	(((VAL) >> 16) & 0x0F)
#define IMX585_FETCH_RHS1_M(VAL)	(((VAL) >> 8) & 0xFF)
#define IMX585_FETCH_RHS1_L(VAL)	((VAL) & 0xFF)

#define IMX585_FETCH_VTS_H(VAL)		(((VAL) >> 16) & 0x0F)
#define IMX585_FETCH_VTS_M(VAL)		(((VAL) >> 8) & 0xFF)
#define IMX585_FETCH_VTS_L(VAL)		((VAL) & 0xFF)

#define IMX585_VTS_REG_L		0x3028
#define IMX585_VTS_REG_M		0x3029
#define IMX585_VTS_REG_H		0x302A

#define IMX585_MIRROR_BIT_MASK		BIT(0)
#define IMX585_FLIP_BIT_MASK		BIT(1)
#define IMX585_FLIP_REG			0x3030

#define REG_NULL			0xFFFF
#define REG_DELAY			0xFFFE

#define IMX585_REG_VALUE_08BIT		1
#define IMX585_REG_VALUE_16BIT		2
#define IMX585_REG_VALUE_24BIT		3

#define IMX585_GROUP_HOLD_REG		0x3001
#define IMX585_GROUP_HOLD_START		0x01
#define IMX585_GROUP_HOLD_END		0x00

/* Basic Readout Lines. Number of necessary readout lines in sensor */
#define BRL_ALL				2228u
#define BRL_BINNING			1115u
/* Readout timing setting of SEF1(DOL2): RHS1 < 2 * BRL and should be 4n + 1 */
#define RHS1_MAX_X2(VAL)		(((VAL) * 2 - 1) / 4 * 4 + 1)
#define SHR1_MIN_X2			9u

/* Readout timing setting of SEF1(DOL3): RHS1 < 3 * BRL and should be 6n + 1 */
#define RHS1_MAX_X3(VAL)		(((VAL) * 3 - 1) / 6 * 6 + 1)
#define SHR1_MIN_X3			13u

#define OF_CAMERA_PINCTRL_STATE_DEFAULT	"rockchip,camera_default"
#define OF_CAMERA_PINCTRL_STATE_SLEEP	"rockchip,camera_sleep"
#define RKMODULE_CAMERA_FASTBOOT_ENABLE "rockchip,camera_fastboot"

#define IMX585_NAME			"imx585"

static const char * const imx585_supply_names[] = {
	"dvdd",		/* Digital core power */
	"dovdd",	/* Digital I/O power */
	"avdd",		/* Analog power */
};

#define IMX585_NUM_SUPPLIES ARRAY_SIZE(imx585_supply_names)

struct regval {
	u16 addr;
	u8 val;
};

struct imx585_mode {
	u32 bus_fmt;
	u32 width;
	u32 height;
	struct v4l2_fract max_fps;
	u32 hts_def;
	u32 vts_def;
	u32 exp_def;
	u32 mipi_freq_idx;
	u32 bpp;
	const struct regval *global_reg_list;
	const struct regval *reg_list;
	u32 hdr_mode;
	u32 vc[PAD_MAX];
	u32 xvclk;
};

struct imx585 {
	struct i2c_client	*client;
	struct clk		*xvclk;
	struct gpio_desc	*reset_gpio;
	struct gpio_desc	*power_gpio;
	struct regulator_bulk_data supplies[IMX585_NUM_SUPPLIES];

	struct pinctrl		*pinctrl;
	struct pinctrl_state	*pins_default;
	struct pinctrl_state	*pins_sleep;

	struct v4l2_subdev	subdev;
	struct media_pad	pad;
	struct v4l2_ctrl_handler ctrl_handler;
	struct v4l2_ctrl	*exposure;
	struct v4l2_ctrl	*anal_a_gain;
	struct v4l2_ctrl	*digi_gain;
	struct v4l2_ctrl	*hblank;
	struct v4l2_ctrl	*vblank;
	struct v4l2_ctrl	*pixel_rate;
	struct v4l2_ctrl	*link_freq;
	struct mutex		mutex;
	bool			streaming;
	bool			power_on;
	u32			is_thunderboot;
	bool			is_thunderboot_ng;
	bool			is_first_streamoff;
	const struct imx585_mode *supported_modes;
	const struct imx585_mode *cur_mode;
	u32			module_index;
	u32			cfg_num;
	const char		*module_facing;
	const char		*module_name;
	const char		*len_name;
	u32			cur_vts;
	bool			has_init_exp;
	struct preisp_hdrae_exp_s init_hdrae_exp;
	struct v4l2_fwnode_endpoint bus_cfg;
};

static struct rkmodule_csi_dphy_param dcphy_param = {
	.vendor = PHY_VENDOR_SAMSUNG,
	.lp_vol_ref = 6,
	.lp_hys_sw = {3, 0, 0, 0},
	.lp_escclk_pol_sel = {1, 1, 1, 1},
	.skew_data_cal_clk = {0, 3, 3, 3},
	.clk_hs_term_sel = 2,
	.data_hs_term_sel = {2, 2, 2, 2},
	.reserved = {0},
};

#define to_imx585(sd) container_of(sd, struct imx585, subdev)

/*
 * Xclk 37.125Mhz
 */
static __maybe_unused const struct regval imx585_global_12bit_3864x2192_regs[] = {
	{0x3002, 0x00},
	{0x3008, 0x7F},
	{0x300A, 0x5B},
	{0x30C1, 0x00},
	{0x3031, 0x01},
	{0x3032, 0x01},
	{0x30D9, 0x06},
	{0x3116, 0x24},
	{0x3118, 0xC0},
	{0x311E, 0x24},
	{0x32D4, 0x21},
	{0x32EC, 0xA1},
	{0x3452, 0x7F},
	{0x3453, 0x03},
	{0x358A, 0x04},
	{0x35A1, 0x02},
	{0x36BC, 0x0C},
	{0x36CC, 0x53},
	{0x36CD, 0x00},
	{0x36CE, 0x3C},
	{0x36D0, 0x8C},
	{0x36D1, 0x00},
	{0x36D2, 0x71},
	{0x36D4, 0x3C},
	{0x36D6, 0x53},
	{0x36D7, 0x00},
	{0x36D8, 0x71},
	{0x36DA, 0x8C},
	{0x36DB, 0x00},
	{0x3701, 0x03},
	{0x3724, 0x02},
	{0x3726, 0x02},
	{0x3732, 0x02},
	{0x3734, 0x03},
	{0x3736, 0x03},
	{0x3742, 0x03},
	{0x3862, 0xE0},
	{0x38CC, 0x30},
	{0x38CD, 0x2F},
	{0x395C, 0x0C},
	{0x3A42, 0xD1},
	{0x3A4C, 0x77},
	{0x3AE0, 0x02},
	{0x3AEC, 0x0C},
	{0x3B00, 0x2E},
	{0x3B06, 0x29},
	{0x3B98, 0x25},
	{0x3B99, 0x21},
	{0x3B9B, 0x13},
	{0x3B9C, 0x13},
	{0x3B9D, 0x13},
	{0x3B9E, 0x13},
	{0x3BA1, 0x00},
	{0x3BA2, 0x06},
	{0x3BA3, 0x0B},
	{0x3BA4, 0x10},
	{0x3BA5, 0x14},
	{0x3BA6, 0x18},
	{0x3BA7, 0x1A},
	{0x3BA8, 0x1A},
	{0x3BA9, 0x1A},
	{0x3BAC, 0xED},
	{0x3BAD, 0x01},
	{0x3BAE, 0xF6},
	{0x3BAF, 0x02},
	{0x3BB0, 0xA2},
	{0x3BB1, 0x03},
	{0x3BB2, 0xE0},
	{0x3BB3, 0x03},
	{0x3BB4, 0xE0},
	{0x3BB5, 0x03},
	{0x3BB6, 0xE0},
	{0x3BB7, 0x03},
	{0x3BB8, 0xE0},
	{0x3BBA, 0xE0},
	{0x3BBC, 0xDA},
	{0x3BBE, 0x88},
	{0x3BC0, 0x44},
	{0x3BC2, 0x7B},
	{0x3BC4, 0xA2},
	{0x3BC8, 0xBD},
	{0x3BCA, 0xBD},
	{0x4004, 0x48},
	{0x4005, 0x09},
	{REG_NULL, 0x00},
};

static __maybe_unused const struct regval imx585_linear_12bit_3864x2192_891M_regs[] = {
	{0x3020, 0x00},
	{0x3021, 0x00},
	{0x3022, 0x00},
	{0x3024, 0xCA},
	{0x3025, 0x08},
	{0x3028, 0x4C},
	{0x3029, 0x04},
	{0x302C, 0x00},
	{0x302D, 0x00},
	{0x3033, 0x05},
	{0x3050, 0x08},
	{0x3051, 0x00},
	{0x3054, 0x19},
	{0x3058, 0x3E},
	{0x3060, 0x25},
	{0x3064, 0x4A},
	{0x30CF, 0x00},
	{0x3260, 0x01},
	{0x400C, 0x00},
	{0x4018, 0x7F},
	{0x401A, 0x37},
	{0x401C, 0x37},
	{0x401E, 0xF7},
	{0x401F, 0x00},
	{0x4020, 0x3F},
	{0x4022, 0x6F},
	{0x4024, 0x3F},
	{0x4026, 0x5F},
	{0x4028, 0x2F},
	{0x4074, 0x01},
	{REG_NULL, 0x00},
};

static __maybe_unused const struct regval imx585_hdr2_12bit_3864x2192_1782M_regs[] = {
	{0x3020, 0x00},
	{0x3021, 0x00},
	{0x3022, 0x00},
	{0x3024, 0xCA},
	{0x3025, 0x08},
	{0x3028, 0x26},
	{0x3029, 0x02},
	{0x302C, 0x01},
	{0x302D, 0x01},
	{0x3033, 0x04},
	{0x3050, 0x90},
	{0x3051, 0x0D},
	{0x3054, 0x09},
	{0x3058, 0x3E},
	{0x3060, 0x4D},
	{0x3064, 0x4A},
	{0x30CF, 0x01},
	{0x3260, 0x00},
	{0x400C, 0x01},
	{0x4018, 0xB7},
	{0x401A, 0x67},
	{0x401C, 0x6F},
	{0x401E, 0xDF},
	{0x401F, 0x01},
	{0x4020, 0x6F},
	{0x4022, 0xCF},
	{0x4024, 0x6F},
	{0x4026, 0xB7},
	{0x4028, 0x5F},
	{0x4074, 0x00},
	{REG_NULL, 0x00},
};

static __maybe_unused const struct regval imx585_hdr3_12bit_3864x2192_1782M_regs[] = {
	{0x3020, 0x00},
	{0x3021, 0x00},
	{0x3022, 0x00},
	{0x3024, 0x96},
	{0x3025, 0x06},
	{0x3028, 0x26},
	{0x3029, 0x02},
	{0x302C, 0x01},
	{0x302D, 0x02},
	{0x3033, 0x04},
	{0x3050, 0x14},
	{0x3051, 0x01},
	{0x3054, 0x0D},
	{0x3058, 0x26},
	{0x3060, 0x19},
	{0x3064, 0x32},
	{0x30CF, 0x03},
	{0x3260, 0x00},
	{0x400C, 0x01},
	{0x4018, 0xB7},
	{0x401A, 0x67},
	{0x401C, 0x6F},
	{0x401E, 0xDF},
	{0x401F, 0x01},
	{0x4020, 0x6F},
	{0x4022, 0xCF},
	{0x4024, 0x6F},
	{0x4026, 0xB7},
	{0x4028, 0x5F},
	{0x4074, 0x00},
	{REG_NULL, 0x00},
};

static __maybe_unused const struct regval imx585_global_10bit_3864x2192_regs[] = {
	{0x3002, 0x00},
	{0x3008, 0x7F},
	{0x300A, 0x5B},
	{0x3031, 0x00},
	{0x3032, 0x00},
	{0x30C1, 0x00},
	{0x30D9, 0x06},
	{0x3116, 0x24},
	{0x311E, 0x24},
	{0x32D4, 0x21},
	{0x32EC, 0xA1},
	{0x3452, 0x7F},
	{0x3453, 0x03},
	{0x358A, 0x04},
	{0x35A1, 0x02},
	{0x36BC, 0x0C},
	{0x36CC, 0x53},
	{0x36CD, 0x00},
	{0x36CE, 0x3C},
	{0x36D0, 0x8C},
	{0x36D1, 0x00},
	{0x36D2, 0x71},
	{0x36D4, 0x3C},
	{0x36D6, 0x53},
	{0x36D7, 0x00},
	{0x36D8, 0x71},
	{0x36DA, 0x8C},
	{0x36DB, 0x00},
	{0x3701, 0x00},
	{0x3724, 0x02},
	{0x3726, 0x02},
	{0x3732, 0x02},
	{0x3734, 0x03},
	{0x3736, 0x03},
	{0x3742, 0x03},
	{0x3862, 0xE0},
	{0x38CC, 0x30},
	{0x38CD, 0x2F},
	{0x395C, 0x0C},
	{0x3A42, 0xD1},
	{0x3A4C, 0x77},
	{0x3AE0, 0x02},
	{0x3AEC, 0x0C},
	{0x3B00, 0x2E},
	{0x3B06, 0x29},
	{0x3B98, 0x25},
	{0x3B99, 0x21},
	{0x3B9B, 0x13},
	{0x3B9C, 0x13},
	{0x3B9D, 0x13},
	{0x3B9E, 0x13},
	{0x3BA1, 0x00},
	{0x3BA2, 0x06},
	{0x3BA3, 0x0B},
	{0x3BA4, 0x10},
	{0x3BA5, 0x14},
	{0x3BA6, 0x18},
	{0x3BA7, 0x1A},
	{0x3BA8, 0x1A},
	{0x3BA9, 0x1A},
	{0x3BAC, 0xED},
	{0x3BAD, 0x01},
	{0x3BAE, 0xF6},
	{0x3BAF, 0x02},
	{0x3BB0, 0xA2},
	{0x3BB1, 0x03},
	{0x3BB2, 0xE0},
	{0x3BB3, 0x03},
	{0x3BB4, 0xE0},
	{0x3BB5, 0x03},
	{0x3BB6, 0xE0},
	{0x3BB7, 0x03},
	{0x3BB8, 0xE0},
	{0x3BBA, 0xE0},
	{0x3BBC, 0xDA},
	{0x3BBE, 0x88},
	{0x3BC0, 0x44},
	{0x3BC2, 0x7B},
	{0x3BC4, 0xA2},
	{0x3BC8, 0xBD},
	{0x3BCA, 0xBD},
	{0x4004, 0x48},
	{0x4005, 0x09},
	{REG_NULL, 0x00},
};

static __maybe_unused const struct regval imx585_hdr3_10bit_3864x2192_1585M_regs[] = {
	{0x3020, 0x00},
	{0x3021, 0x00},
	{0x3022, 0x00},
	{0x3024, 0xBD},
	{0x3025, 0x06},
	{0x3028, 0x1A},
	{0x3029, 0x02},
	{0x302C, 0x01},
	{0x302D, 0x02},
	{0x3033, 0x08},
	{0x3050, 0x90},
	{0x3051, 0x15},
	{0x3054, 0x0D},
	{0x3058, 0xA4},
	{0x3060, 0x97},
	{0x3064, 0xB6},
	{0x30CF, 0x03},
	{0x3118, 0xA0},
	{0x3260, 0x00},
	{0x400C, 0x01},
	{0x4018, 0xA7},
	{0x401A, 0x57},
	{0x401C, 0x5F},
	{0x401E, 0x97},
	{0x401F, 0x01},
	{0x4020, 0x5F},
	{0x4022, 0xAF},
	{0x4024, 0x5F},
	{0x4026, 0x9F},
	{0x4028, 0x4F},
	{0x4074, 0x00},
	{REG_NULL, 0x00},
};

static __maybe_unused const struct regval imx585_hdr3_10bit_3864x2192_1782M_regs[] = {
	{0x3020, 0x00},
	{0x3021, 0x00},
	{0x3022, 0x00},
	{0x3024, 0xEA},
	{0x3025, 0x07},
	{0x3028, 0xCA},
	{0x3029, 0x01},
	{0x302C, 0x01},
	{0x302D, 0x02},
	{0x3033, 0x04},
	{0x3050, 0x3E},
	{0x3051, 0x01},
	{0x3054, 0x0D},
	{0x3058, 0x9E},
	{0x3060, 0x91},
	{0x3064, 0xC2},
	{0x30CF, 0x03},
	{0x3118, 0xC0},
	{0x3260, 0x00},
	{0x400C, 0x01},
	{0x4018, 0xB7},
	{0x401A, 0x67},
	{0x401C, 0x6F},
	{0x401E, 0xDF},
	{0x401F, 0x01},
	{0x4020, 0x6F},
	{0x4022, 0xCF},
	{0x4024, 0x6F},
	{0x4026, 0xB7},
	{0x4028, 0x5F},
	{0x4074, 0x00},
	{REG_NULL, 0x00},
};

static __maybe_unused const struct regval imx585_hdr2_10bit_3864x2192_1585M_regs[] = {
	{0x3020, 0x00},
	{0x3021, 0x00},
	{0x3022, 0x00},
	{0x3024, 0xFC},
	{0x3025, 0x08},
	{0x3028, 0x1A},
	{0x3029, 0x02},
	{0x302C, 0x01},
	{0x302D, 0x01},
	{0x3033, 0x08},
	{0x3050, 0xA8},
	{0x3051, 0x0D},
	{0x3054, 0x09},
	{0x3058, 0x3E},
	{0x3060, 0x4D},
	{0x3064, 0x4a},
	{0x30CF, 0x01},
	{0x3118, 0xA0},
	{0x3260, 0x00},
	{0x400C, 0x01},
	{0x4018, 0xA7},
	{0x401A, 0x57},
	{0x401C, 0x5F},
	{0x401E, 0x97},
	{0x401F, 0x01},
	{0x4020, 0x5F},
	{0x4022, 0xAF},
	{0x4024, 0x5F},
	{0x4026, 0x9F},
	{0x4028, 0x4F},
	{0x4074, 0x00},
	{REG_NULL, 0x00},
};

static __maybe_unused const struct regval imx585_linear_10bit_3864x2192_891M_regs[] = {
	{0x3020, 0x00},
	{0x3021, 0x00},
	{0x3022, 0x00},
	{0x3024, 0xCA},
	{0x3025, 0x08},
	{0x3028, 0x4C},
	{0x3029, 0x04},
	{0x302C, 0x00},
	{0x302D, 0x00},
	{0x3033, 0x05},
	{0x3050, 0x08},
	{0x3051, 0x00},
	{0x3054, 0x19},
	{0x3058, 0x3E},
	{0x3060, 0x25},
	{0x3064, 0x4a},
	{0x30CF, 0x00},
	{0x3118, 0xC0},
	{0x3260, 0x01},
	{0x400C, 0x00},
	{0x4018, 0x7F},
	{0x401A, 0x37},
	{0x401C, 0x37},
	{0x401E, 0xF7},
	{0x401F, 0x00},
	{0x4020, 0x3F},
	{0x4022, 0x6F},
	{0x4024, 0x3F},
	{0x4026, 0x5F},
	{0x4028, 0x2F},
	{0x4074, 0x01},
	{REG_NULL, 0x00},
};

static __maybe_unused const struct regval imx585_linear_12bit_1932x1096_594M_regs[] = {
	{0x3020, 0x01},
	{0x3021, 0x01},
	{0x3022, 0x01},
	{0x3024, 0x5D},
	{0x3025, 0x0C},
	{0x3028, 0x0E},
	{0x3029, 0x03},
	{0x302C, 0x00},
	{0x302D, 0x00},
	{0x3031, 0x00},
	{0x3033, 0x07},
	{0x3050, 0x08},
	{0x3051, 0x00},
	{0x3054, 0x19},
	{0x3058, 0x3E},
	{0x3060, 0x25},
	{0x3064, 0x4A},
	{0x30CF, 0x00},
	{0x30D9, 0x02},
	{0x30DA, 0x01},
	{0x3118, 0x80},
	{0x3260, 0x01},
	{0x3701, 0x00},
	{0x400C, 0x00},
	{0x4018, 0x67},
	{0x401A, 0x27},
	{0x401C, 0x27},
	{0x401E, 0xB7},
	{0x401F, 0x00},
	{0x4020, 0x2F},
	{0x4022, 0x4F},
	{0x4024, 0x2F},
	{0x4026, 0x47},
	{0x4028, 0x27},
	{0x4074, 0x01},
	{REG_NULL, 0x00},
};

static __maybe_unused const struct regval imx585_hdr2_12bit_1932x1096_891M_regs[] = {
	{0x3020, 0x01},
	{0x3021, 0x01},
	{0x3022, 0x01},
	{0x3024, 0xFC},
	{0x3025, 0x08},
	{0x3028, 0x1A},
	{0x3029, 0x02},
	{0x302C, 0x01},
	{0x302D, 0x01},
	{0x3031, 0x00},
	{0x3033, 0x05},
	{0x3050, 0xB8},
	{0x3051, 0x00},
	{0x3054, 0x09},
	{0x3058, 0x3E},
	{0x3060, 0x25},
	{0x3064, 0x4A},
	{0x30CF, 0x01},
	{0x30D9, 0x02},
	{0x30DA, 0x01},
	{0x3118, 0xC0},
	{0x3260, 0x00},
	{0x3701, 0x00},
	{0x400C, 0x00},
	{0x4018, 0xA7},
	{0x401A, 0x57},
	{0x401C, 0x5F},
	{0x401E, 0x97},
	{0x401F, 0x01},
	{0x4020, 0x5F},
	{0x4022, 0xAF},
	{0x4024, 0x5F},
	{0x4026, 0x9F},
	{0x4028, 0x4F},
	{0x4074, 0x01},
	{REG_NULL, 0x00},
};

static __maybe_unused const struct regval imx585_global_10bit_3864x2192_regs_2lane[] = {
	{REG_NULL, 0x00},
};

static __maybe_unused const struct regval imx585_linear_10bit_3864x2192_regs_2lane[] = {
	{0x3002, 0x00},
	{0x3008, 0x54},
	{0x300A, 0x3B},
	{0x3024, 0xCB},
	{0x3028, 0x2A},
	{0x3029, 0x04},
	{0x3031, 0x00},
	{0x3032, 0x00},
	{0x3033, 0x08},
	{0x3050, 0x08},
	{0x30C1, 0x00},
	{0x3116, 0x23},
	{0x3118, 0xB4},
	{0x311A, 0xFC},
	{0x311E, 0x23},
	{0x32D4, 0x21},
	{0x32EC, 0xA1},
	{0x344C, 0x2B},
	{0x344D, 0x01},
	{0x344E, 0xED},
	{0x344F, 0x01},
	{0x3450, 0xF6},
	{0x3451, 0x02},
	{0x3452, 0x7F},
	{0x3453, 0x03},
	{0x358A, 0x04},
	{0x35A1, 0x02},
	{0x35EC, 0x27},
	{0x35EE, 0x8D},
	{0x35F0, 0x8D},
	{0x35F2, 0x29},
	{0x36BC, 0x0C},
	{0x36CC, 0x53},
	{0x36CD, 0x00},
	{0x36CE, 0x3C},
	{0x36D0, 0x8C},
	{0x36D1, 0x00},
	{0x36D2, 0x71},
	{0x36D4, 0x3C},
	{0x36D6, 0x53},
	{0x36D7, 0x00},
	{0x36D8, 0x71},
	{0x36DA, 0x8C},
	{0x36DB, 0x00},
	{0x3701, 0x00},
	{0x3720, 0x00},
	{0x3724, 0x02},
	{0x3726, 0x02},
	{0x3732, 0x02},
	{0x3734, 0x03},
	{0x3736, 0x03},
	{0x3742, 0x03},
	{0x3862, 0xE0},
	{0x38CC, 0x30},
	{0x38CD, 0x2F},
	{0x395C, 0x0C},
	{0x39A4, 0x07},
	{0x39A8, 0x32},
	{0x39AA, 0x32},
	{0x39AC, 0x32},
	{0x39AE, 0x32},
	{0x39B0, 0x32},
	{0x39B2, 0x2F},
	{0x39B4, 0x2D},
	{0x39B6, 0x28},
	{0x39B8, 0x30},
	{0x39BA, 0x30},
	{0x39BC, 0x30},
	{0x39BE, 0x30},
	{0x39C0, 0x30},
	{0x39C2, 0x2E},
	{0x39C4, 0x2B},
	{0x39C6, 0x25},
	{0x3A42, 0xD1},
	{0x3A4C, 0x77},
	{0x3AE0, 0x02},
	{0x3AEC, 0x0C},
	{0x3B00, 0x2E},
	{0x3B06, 0x29},
	{0x3B98, 0x25},
	{0x3B99, 0x21},
	{0x3B9B, 0x13},
	{0x3B9C, 0x13},
	{0x3B9D, 0x13},
	{0x3B9E, 0x13},
	{0x3BA1, 0x00},
	{0x3BA2, 0x06},
	{0x3BA3, 0x0B},
	{0x3BA4, 0x10},
	{0x3BA5, 0x14},
	{0x3BA6, 0x18},
	{0x3BA7, 0x1A},
	{0x3BA8, 0x1A},
	{0x3BA9, 0x1A},
	{0x3BAC, 0xED},
	{0x3BAD, 0x01},
	{0x3BAE, 0xF6},
	{0x3BAF, 0x02},
	{0x3BB0, 0xA2},
	{0x3BB1, 0x03},
	{0x3BB2, 0xE0},
	{0x3BB3, 0x03},
	{0x3BB4, 0xE0},
	{0x3BB5, 0x03},
	{0x3BB6, 0xE0},
	{0x3BB7, 0x03},
	{0x3BB8, 0xE0},
	{0x3BBA, 0xE0},
	{0x3BBC, 0xDA},
	{0x3BBE, 0x88},
	{0x3BC0, 0x44},
	{0x3BC2, 0x7B},
	{0x3BC4, 0xA2},
	{0x3BC8, 0xBD},
	{0x3BCA, 0xBD},
	{0x4001, 0x01},
	{0x4004, 0x00},
	{0x4005, 0x06},
	{0x4018, 0x9F},
	{0x401A, 0x57},
	{0x401C, 0x57},
	{0x401E, 0x87},
	{0x4020, 0x5F},
	{0x4022, 0xA7},
	{0x4024, 0x5F},
	{0x4026, 0x97},
	{0x4028, 0x4F},
	{REG_NULL, 0x00},
};

/*
 * Xclk 27Mhz
 * 15fps
 * CSI-2_2lane
 * AD:12bit Output:12bit
 * 891Mbps
 * Master Mode
 * Time 9.988ms Gain:6dB
 * All-pixel
 */
static __maybe_unused const struct regval imx585_linear_12bit_3864x2192_891M_regs_2lane[] = {
	{0x3008, 0x5D},
	{0x300A, 0x42},
	{0x3028, 0x98},
	{0x3029, 0x08},
	{0x3033, 0x05},
	{0x3050, 0x79},
	{0x3051, 0x07},
	{0x3090, 0x14},
	{0x30C1, 0x00},
	{0x3116, 0x23},
	{0x3118, 0xC6},
	{0x311A, 0xE7},
	{0x311E, 0x23},
	{0x32D4, 0x21},
	{0x32EC, 0xA1},
	{0x344C, 0x2B},
	{0x344D, 0x01},
	{0x344E, 0xED},
	{0x344F, 0x01},
	{0x3450, 0xF6},
	{0x3451, 0x02},
	{0x3452, 0x7F},
	{0x3453, 0x03},
	{0x358A, 0x04},
	{0x35A1, 0x02},
	{0x35EC, 0x27},
	{0x35EE, 0x8D},
	{0x35F0, 0x8D},
	{0x35F2, 0x29},
	{0x36BC, 0x0C},
	{0x36CC, 0x53},
	{0x36CD, 0x00},
	{0x36CE, 0x3C},
	{0x36D0, 0x8C},
	{0x36D1, 0x00},
	{0x36D2, 0x71},
	{0x36D4, 0x3C},
	{0x36D6, 0x53},
	{0x36D7, 0x00},
	{0x36D8, 0x71},
	{0x36DA, 0x8C},
	{0x36DB, 0x00},
	{0x3720, 0x00},
	{0x3724, 0x02},
	{0x3726, 0x02},
	{0x3732, 0x02},
	{0x3734, 0x03},
	{0x3736, 0x03},
	{0x3742, 0x03},
	{0x3862, 0xE0},
	{0x38CC, 0x30},
	{0x38CD, 0x2F},
	{0x395C, 0x0C},
	{0x39A4, 0x07},
	{0x39A8, 0x32},
	{0x39AA, 0x32},
	{0x39AC, 0x32},
	{0x39AE, 0x32},
	{0x39B0, 0x32},
	{0x39B2, 0x2F},
	{0x39B4, 0x2D},
	{0x39B6, 0x28},
	{0x39B8, 0x30},
	{0x39BA, 0x30},
	{0x39BC, 0x30},
	{0x39BE, 0x30},
	{0x39C0, 0x30},
	{0x39C2, 0x2E},
	{0x39C4, 0x2B},
	{0x39C6, 0x25},
	{0x3A42, 0xD1},
	{0x3A4C, 0x77},
	{0x3AE0, 0x02},
	{0x3AEC, 0x0C},
	{0x3B00, 0x2E},
	{0x3B06, 0x29},
	{0x3B98, 0x25},
	{0x3B99, 0x21},
	{0x3B9B, 0x13},
	{0x3B9C, 0x13},
	{0x3B9D, 0x13},
	{0x3B9E, 0x13},
	{0x3BA1, 0x00},
	{0x3BA2, 0x06},
	{0x3BA3, 0x0B},
	{0x3BA4, 0x10},
	{0x3BA5, 0x14},
	{0x3BA6, 0x18},
	{0x3BA7, 0x1A},
	{0x3BA8, 0x1A},
	{0x3BA9, 0x1A},
	{0x3BAC, 0xED},
	{0x3BAD, 0x01},
	{0x3BAE, 0xF6},
	{0x3BAF, 0x02},
	{0x3BB0, 0xA2},
	{0x3BB1, 0x03},
	{0x3BB2, 0xE0},
	{0x3BB3, 0x03},
	{0x3BB4, 0xE0},
	{0x3BB5, 0x03},
	{0x3BB6, 0xE0},
	{0x3BB7, 0x03},
	{0x3BB8, 0xE0},
	{0x3BBA, 0xE0},
	{0x3BBC, 0xDA},
	{0x3BBE, 0x88},
	{0x3BC0, 0x44},
	{0x3BC2, 0x7B},
	{0x3BC4, 0xA2},
	{0x3BC8, 0xBD},
	{0x3BCA, 0xBD},
	{0x4001, 0x01},
	{0x4004, 0xC0},
	{0x4005, 0x06},
	{0x400C, 0x00},
	{0x4018, 0x7F},
	{0x401A, 0x37},
	{0x401C, 0x37},
	{0x401E, 0xF7},
	{0x401F, 0x00},
	{0x4020, 0x3F},
	{0x4022, 0x6F},
	{0x4024, 0x3F},
	{0x4026, 0x5F},
	{0x4028, 0x2F},
	{0x4074, 0x01},
	{0x3002, 0x00},
	//{0x3000, 0x00},
	{REG_DELAY, 0x1E},//wait_ms(30)
	{REG_NULL, 0x00},
};

/*
 * Xclk 27Mhz
 * 90.059fps
 * CSI-2_2lane
 * AD:10bit Output:12bit
 * 2376Mbps
 * Master Mode
 * Time 9.999ms Gain:6dB
 * 2568x1440 2/2-line binning & Window cropping
 */
static __maybe_unused const struct regval imx585_linear_12bit_1284x720_2376M_regs_2lane[] = {
	{0x3008, 0x5D},
	{0x300A, 0x42},
	{0x301C, 0x04},
	{0x3020, 0x01},
	{0x3021, 0x01},
	{0x3022, 0x01},
	{0x3024, 0xAB},
	{0x3025, 0x07},
	{0x3028, 0xA4},
	{0x3029, 0x01},
	{0x3031, 0x00},
	{0x3033, 0x00},
	{0x3040, 0x88},
	{0x3041, 0x02},
	{0x3042, 0x08},
	{0x3043, 0x0A},
	{0x3044, 0xF0},
	{0x3045, 0x02},
	{0x3046, 0x40},
	{0x3047, 0x0B},
	{0x3050, 0xC4},
	{0x3090, 0x14},
	{0x30C1, 0x00},
	{0x30D9, 0x02},
	{0x30DA, 0x01},
	{0x3116, 0x23},
	{0x3118, 0x08},
	{0x3119, 0x01},
	{0x311A, 0xE7},
	{0x311E, 0x23},
	{0x32D4, 0x21},
	{0x32EC, 0xA1},
	{0x344C, 0x2B},
	{0x344D, 0x01},
	{0x344E, 0xED},
	{0x344F, 0x01},
	{0x3450, 0xF6},
	{0x3451, 0x02},
	{0x3452, 0x7F},
	{0x3453, 0x03},
	{0x358A, 0x04},
	{0x35A1, 0x02},
	{0x35EC, 0x27},
	{0x35EE, 0x8D},
	{0x35F0, 0x8D},
	{0x35F2, 0x29},
	{0x36BC, 0x0C},
	{0x36CC, 0x53},
	{0x36CD, 0x00},
	{0x36CE, 0x3C},
	{0x36D0, 0x8C},
	{0x36D1, 0x00},
	{0x36D2, 0x71},
	{0x36D4, 0x3C},
	{0x36D6, 0x53},
	{0x36D7, 0x00},
	{0x36D8, 0x71},
	{0x36DA, 0x8C},
	{0x36DB, 0x00},
	{0x3701, 0x00},
	{0x3720, 0x00},
	{0x3724, 0x02},
	{0x3726, 0x02},
	{0x3732, 0x02},
	{0x3734, 0x03},
	{0x3736, 0x03},
	{0x3742, 0x03},
	{0x3862, 0xE0},
	{0x38CC, 0x30},
	{0x38CD, 0x2F},
	{0x395C, 0x0C},
	{0x39A4, 0x07},
	{0x39A8, 0x32},
	{0x39AA, 0x32},
	{0x39AC, 0x32},
	{0x39AE, 0x32},
	{0x39B0, 0x32},
	{0x39B2, 0x2F},
	{0x39B4, 0x2D},
	{0x39B6, 0x28},
	{0x39B8, 0x30},
	{0x39BA, 0x30},
	{0x39BC, 0x30},
	{0x39BE, 0x30},
	{0x39C0, 0x30},
	{0x39C2, 0x2E},
	{0x39C4, 0x2B},
	{0x39C6, 0x25},
	{0x3A42, 0xD1},
	{0x3A4C, 0x77},
	{0x3AE0, 0x02},
	{0x3AEC, 0x0C},
	{0x3B00, 0x2E},
	{0x3B06, 0x29},
	{0x3B98, 0x25},
	{0x3B99, 0x21},
	{0x3B9B, 0x13},
	{0x3B9C, 0x13},
	{0x3B9D, 0x13},
	{0x3B9E, 0x13},
	{0x3BA1, 0x00},
	{0x3BA2, 0x06},
	{0x3BA3, 0x0B},
	{0x3BA4, 0x10},
	{0x3BA5, 0x14},
	{0x3BA6, 0x18},
	{0x3BA7, 0x1A},
	{0x3BA8, 0x1A},
	{0x3BA9, 0x1A},
	{0x3BAC, 0xED},
	{0x3BAD, 0x01},
	{0x3BAE, 0xF6},
	{0x3BAF, 0x02},
	{0x3BB0, 0xA2},
	{0x3BB1, 0x03},
	{0x3BB2, 0xE0},
	{0x3BB3, 0x03},
	{0x3BB4, 0xE0},
	{0x3BB5, 0x03},
	{0x3BB6, 0xE0},
	{0x3BB7, 0x03},
	{0x3BB8, 0xE0},
	{0x3BBA, 0xE0},
	{0x3BBC, 0xDA},
	{0x3BBE, 0x88},
	{0x3BC0, 0x44},
	{0x3BC2, 0x7B},
	{0x3BC4, 0xA2},
	{0x3BC8, 0xBD},
	{0x3BCA, 0xBD},
	{0x4001, 0x01},
	{0x4004, 0xC0},
	{0x4005, 0x06},
	{0x4018, 0xE7},
	{0x401A, 0x8F},
	{0x401C, 0x8F},
	{0x401E, 0x7F},
	{0x401F, 0x02},
	{0x4020, 0x97},
	{0x4022, 0x0F},
	{0x4023, 0x01},
	{0x4024, 0x97},
	{0x4026, 0xF7},
	{0x4028, 0x7F},
	{0x3002, 0x00},
	//{0x3000, 0x00},
	{REG_DELAY, 0x1E},//wait_ms(30)
	{REG_NULL, 0x00},
};

/*
 * Xclk 24Mhz
 * 90.059fps
 * CSI-2_4lane
 * AD:10bit Output:12bit
 * 800Mbps
 * Master Mode
 * Time 9.999ms Gain:6dB
 * 3840x2160 2/2-line binning & Window cropping
 */
static __maybe_unused const struct regval imx585_linear_12bit_3840x2160_1600M_regs_4lane[] = {
	{0x3000,0x01},
	{0x3002,0x01},
	{0x3008,0x7F},
	{0x3009,0x00},
	{0x300A,0x5B},
	{0x300B,0x50},
//	{0x3024,0x65},
//	{0x3025,0x04},
//	{0x3026,0x00},
	{0x3028,0x4C},
	{0x3029,0x04},
	{0x30A5,0x00},
	{0x3114,0x02},
	{0x311C,0x84},
	{0x3260,0x22},
	{0x3262,0x02},
	{0x3278,0xA2},
	{0x3324,0x00},
	{0x3366,0x31},
	{0x340C,0x4D},
	{0x3416,0x10},
	{0x3417,0x13},
	{0x3432,0x93},
	{0x34CE,0x1E},
	{0x34CF,0x1E},
	{0x34DC,0x80},
	{0x351C,0x03},
	{0x359E,0x70},
	{0x35A2,0x9C},
	{0x35AC,0x08},
	{0x35C0,0xFA},
	{0x35C2,0x4E},
	{0x3608,0x41},
	{0x360A,0x47},
	{0x361E,0x4A},
	{0x3630,0x43},
	{0x3632,0x47},
	{0x363C,0x41},
	{0x363E,0x4A},
	{0x3648,0x41},
	{0x364A,0x47},
	{0x3660,0x04},
	{0x3676,0x3F},
	{0x367A,0x3F},
	{0x36A4,0x41},
	{0x3798,0x82},
	{0x379A,0x82},
	{0x379C,0x82},
	{0x379E,0x82},
	{0x3804,0x22},
	{0x3807,0x60},
	{0x3888,0xA8},
	{0x388C,0xA6},
	{0x3914,0x15},
	{0x3915,0x15},
	{0x3916,0x15},
	{0x3917,0x14},
	{0x3918,0x14},
	{0x3919,0x14},
	{0x391A,0x13},
	{0x391B,0x13},
	{0x391C,0x13},
	{0x391E,0x00},
	{0x391F,0xA5},
	{0x3920,0xED},
	{0x3921,0x0E},
	{0x39A2,0x0C},
	{0x39A4,0x16},
	{0x39A6,0x2B},
	{0x39A7,0x01},
	{0x39D2,0x2D},
	{0x39D3,0x00},
	{0x39D8,0x37},
	{0x39D9,0x00},
	{0x39DA,0x9B},
	{0x39DB,0x01},
	{0x39E0,0x28},
	{0x39E1,0x00},
	{0x39E2,0x2C},
	{0x39E3,0x00},
	{0x39E8,0x96},
	{0x39EA,0x9A},
	{0x39EB,0x01},
	{0x39F2,0x27},
	{0x39F3,0x00},
	{0x3A00,0x38},
	{0x3A01,0x00},
	{0x3A02,0x95},
	{0x3A03,0x01},
	{0x3A18,0x9B},
	{0x3A2A,0x0C},
	{0x3A30,0x15},
	{0x3A32,0x31},
	{0x3A33,0x01},
	{0x3A36,0x4D},
	{0x3A3E,0x11},
	{0x3A40,0x31},
	{0x3A42,0x4C},
	{0x3A43,0x01},
	{0x3A44,0x47},
	{0x3A46,0x4B},
	{0x3A4E,0x11},
	{0x3A50,0x32},
	{0x3A52,0x46},
	{0x3A53,0x01},
	{0x3D01,0x03},
	{0x3D04,0x48},
	{0x3D05,0x09},
	{0x3D18,0x9F},
	{0x3D1A,0x57},
	{0x3D1C,0x57},
	{0x3D1E,0x87},
	{0x3D20,0x5F},
	{0x3D22,0xA7},
	{0x3D24,0x5F},
	{0x3D26,0x97},
	{0x3D28,0x4F},
	{0x3002,0x00},
	{0xffff,0x10},
	{0x3000,0x00},
	{0x0100,0x01},
	{REG_NULL, 0x00},
};

/*
 * Xclk 37.125Mhz
 * 60fps
 * CSI-2_4lane
 * AD:12bit Output:12bit
 * 1782Mbps
 * Master Mode
 * Time 16.607ms Gain:6dB
 * 3840x2160 All-pixel scan & Window cropping
 */
static __maybe_unused const struct regval imx585_linear_12bit_3840x2160_1782M_regs_4lane[] = {
	{0x3000,0x01},  // STANDBY
	{0x3001,0x00},  // REGHOLD
	{0x3002,0x01},  // XMSTA
	{0x3014,0x01},  // INCK_SEL[3:0]
	{0x3015,0x02},  // DATARATE_SEL[3:0]
	{0x3018,0x10},  // WINMODE[4:0]
	{0x3019,0x00},  // CFMODE[2:0]
	{0x301A,0x00},  // WDMODE
	{0x301B,0x00},  // ADDMODE[1:0]
	{0x301C,0x00},  // THIN_V_EN
	{0x301E,0x01},  // VCMODE
	{0x3020,0x01},  // HREVERSE
	{0x3021,0x01},  // VREVERSE
	{0x3022,0x02},  // ADBIT[1:0]
	{0x3023,0x01},  // MDBIT[1:0]
	{0x3024,0x00},  // COMBI_EN
	{0x3028,0xCA},  // VMAX[19:0]
	{0x3029,0x08},  // VMAX[19:0]
	{0x302A,0x00},  // VMAX[19:0]
	{0x302C,0x26},  // HMAX[15:0]
	{0x302D,0x02},  // HMAX[15:0]
	{0x3030,0x00},  // FDG_SEL0[1:0]
	{0x3031,0x00},  // FDG_SEL1[1:0]
	{0x3032,0x00},  // FDG_SEL2[1:0]
	{0x303C,0x00},  // PIX_HST[12:0]
	{0x303D,0x00},  // PIX_HST[12:0]
	{0x303E,0x10},  // PIX_HWIDTH[12:0]
	{0x303F,0x0F},  // PIX_HWIDTH[12:0]
	{0x3040,0x03},  // LANEMODE[2:0]
	{0x3042,0x00},  // XSIZE_OVERLAP[10:0]
	{0x3043,0x00},  // XSIZE_OVERLAP[10:0]
	{0x3044,0x00},  // PIX_VST[11:0]
	{0x3045,0x00},  // PIX_VST[11:0]
	{0x3046,0x84},  // PIX_VWIDTH[11:0]
	{0x3047,0x08},  // PIX_VWIDTH[11:0]
	{0x3050,0x08},  // SHR0[19:0]
	{0x3051,0x00},  // SHR0[19:0]
	{0x3052,0x00},  // SHR0[19:0]
	{0x3054,0x0E},  // SHR1[19:0]
	{0x3055,0x00},  // SHR1[19:0]
	{0x3056,0x00},  // SHR1[19:0]
	{0x3058,0x8A},  // SHR2[19:0]
	{0x3059,0x01},  // SHR2[19:0]
	{0x305A,0x00},  // SHR2[19:0]
	{0x3060,0x16},  // RHS1[19:0]
	{0x3061,0x01},  // RHS1[19:0]
	{0x3062,0x00},  // RHS1[19:0]
	{0x3064,0xC4},  // RHS2[19:0]
	{0x3065,0x0C},  // RHS2[19:0]
	{0x3066,0x00},  // RHS2[19:0]
	{0x3069,0x00},  // -
	{0x306A,0x00},  // CHDR_GAIN_EN
	{0x306C,0x00},  // GAIN[10:0]
	{0x306D,0x00},  // GAIN[10:0]
	{0x306E,0x00},  // GAIN_1[10:0]
	{0x306F,0x00},  // GAIN_1[10:0]
	{0x3070,0x00},  // GAIN_2[10:0]
	{0x3071,0x00},  // GAIN_2[10:0]
	{0x3074,0x64},  // -
	{0x3081,0x00},  // EXP_GAIN
	{0x308C,0x00},  // CHDR_DGAIN0_HG[15:0]
	{0x308D,0x01},  // CHDR_DGAIN0_HG[15:0]
	{0x3094,0x00},  // CHDR_AGAIN0_LG[10:0]
	{0x3095,0x00},  // CHDR_AGAIN0_LG[10:0]
	{0x3096,0x00},  // CHDR_AGAIN1[10:0]
	{0x3097,0x00},  // CHDR_AGAIN1[10:0]
	{0x309C,0x00},  // CHDR_AGAIN0_HG[10:0]
	{0x309D,0x00},  // CHDR_AGAIN0_HG[10:0]
	{0x30A4,0xAA},  // XVSOUTSEL[1:0]
	{0x30A6,0x00},  // XVS_DRV[1:0]
	{0x30CC,0x00},  // -
	{0x30CD,0x00},  // -
	{0x30D5,0x04},  // DIG_CLP_VSTART[4:0]
	{0x30DC,0x32},  // BLKLEVEL[15:0]
	{0x30DD,0x00},  // BLKLEVEL[15:0]
	{0x3400,0x01},  // GAIN_PGC_FIDMD
	{0x3460,0x21},  // -
	{0x3478,0xA1},  // -
	{0x347C,0x01},  // -
	{0x3480,0x01},  // -
	{0x36D0,0x00},  // EXP_TH_H
	{0x36D1,0x10},  // EXP_TH_H
	{0x36D4,0x00},  // EXP_TH_L
	{0x36D5,0x10},  // EXP_TH_L
	{0x36E2,0x00},  // EXP_BK
	{0x36E4,0x00},  // CCMP2_EXP
	{0x36E5,0x00},  // CCMP2_EXP
	{0x36E6,0x00},  // CCMP2_EXP
	{0x36E8,0x00},  // CCMP1_EXP
	{0x36E9,0x00},  // CCMP1_EXP
	{0x36EA,0x00},  // CCMP1_EXP
	{0x36EC,0x00},  // ACMP2_EXP
	{0x36EE,0x00},  // ACMP1_EXP
	{0x36EF,0x00},  // CCMP_EN
	{0x3930,0x0C},  // DUR
	{0x3931,0x01},  // DUR
	{0x3A4C,0x39},  // WAIT_ST0
	{0x3A4D,0x01},  // WAIT_ST0
	{0x3A4E,0x14},  // -
	{0x3A50,0x48},  // WAIT_ST1
	{0x3A51,0x01},  // WAIT_ST1
	{0x3A52,0x14},  // -
	{0x3A56,0x00},  // -
	{0x3A5A,0x00},  // -
	{0x3A5E,0x00},  // -
	{0x3A62,0x00},  // -
	{0x3A6A,0x20},  // -
	{0x3A6C,0x42},  // -
	{0x3A6E,0xA0},  // -
	{0x3B2C,0x0C},  // -
	{0x3B30,0x1C},  // -
	{0x3B34,0x0C},  // -
	{0x3B38,0x1C},  // -
	{0x3BA0,0x0C},  // -
	{0x3BA4,0x1C},  // -
	{0x3BA8,0x0C},  // -
	{0x3BAC,0x1C},  // -
	{0x3D3C,0x11},  // -
	{0x3D46,0x0B},  // -
	{0x3DE0,0x3F},  // -
	{0x3DE1,0x08},  // -
	{0x3E10,0x10},  // ADTHEN[2:0]
	{0x3E14,0x87},  // -
	{0x3E16,0x91},  // -
	{0x3E18,0x91},  // -
	{0x3E1A,0x87},  // -
	{0x3E1C,0x78},  // -
	{0x3E1E,0x50},  // -
	{0x3E20,0x50},  // -
	{0x3E22,0x50},  // -
	{0x3E24,0x87},  // -
	{0x3E26,0x91},  // -
	{0x3E28,0x91},  // -
	{0x3E2A,0x87},  // -
	{0x3E2C,0x78},  // -
	{0x3E2E,0x50},  // -
	{0x3E30,0x50},  // -
	{0x3E32,0x50},  // -
	{0x3E34,0x87},  // -
	{0x3E36,0x91},  // -
	{0x3E38,0x91},  // -
	{0x3E3A,0x87},  // -
	{0x3E3C,0x78},  // -
	{0x3E3E,0x50},  // -
	{0x3E40,0x50},  // -
	{0x3E42,0x50},  // -
	{0x4054,0x64},  // -
	{0x4148,0xFE},  // -
	{0x4149,0x05},  // -
	{0x414A,0xFF},  // -
	{0x414B,0x05},  // -
	{0x420A,0x03},  // -
	{0x4231,0x08},  // -
	{0x423D,0x9C},  // -
	{0x4242,0xB4},  // -
	{0x4246,0xB4},  // -
	{0x424E,0xB4},  // -
	{0x425C,0xB4},  // -
	{0x425E,0xB6},  // -
	{0x426C,0xB4},  // -
	{0x426E,0xB6},  // -
	{0x428C,0xB4},  // -
	{0x428E,0xB6},  // -
	{0x4708,0x00},  // -
	{0x4709,0x00},  // -
	{0x470A,0xFF},  // -
	{0x470B,0x03},  // -
	{0x470C,0x00},  // -
	{0x470D,0x00},  // -
	{0x470E,0xFF},  // -
	{0x470F,0x03},  // -
	{0x47EB,0x1C},  // -
	{0x47F0,0xA6},  // -
	{0x47F2,0xA6},  // -
	{0x47F4,0xA0},  // -
	{0x47F6,0x96},  // -
	{0x4808,0xA6},  // -
	{0x480A,0xA6},  // -
	{0x480C,0xA0},  // -
	{0x480E,0x96},  // -
	{0x492C,0xB2},  // -
	{0x4930,0x03},  // -
	{0x4932,0x03},  // -
	{0x4936,0x5B},  // -
	{0x4938,0x82},  // -
	{0x493C,0x23},  // WAIT_10_SHF
	{0x493E,0x23},  // -
	{0x4940,0x23},  // WAIT_12_SHF
	{0x4BA8,0x1C},  // -
	{0x4BA9,0x03},  // -
	{0x4BAC,0x1C},  // -
	{0x4BAD,0x1C},  // -
	{0x4BAE,0x1C},  // -
	{0x4BAF,0x1C},  // -
	{0x4BB0,0x1C},  // -
	{0x4BB1,0x1C},  // -
	{0x4BB2,0x1C},  // -
	{0x4BB3,0x1C},  // -
	{0x4BB4,0x1C},  // -
	{0x4BB8,0x03},  // -
	{0x4BB9,0x03},  // -
	{0x4BBA,0x03},  // -
	{0x4BBB,0x03},  // -
	{0x4BBC,0x03},  // -
	{0x4BBD,0x03},  // -
	{0x4BBE,0x03},  // -
	{0x4BBF,0x03},  // -
	{0x4BC0,0x03},  // -
	{0x4C14,0x87},  // -
	{0x4C16,0x91},  // -
	{0x4C18,0x91},  // -
	{0x4C1A,0x87},  // -
	{0x4C1C,0x78},  // -
	{0x4C1E,0x50},  // -
	{0x4C20,0x50},  // -
	{0x4C22,0x50},  // -
	{0x4C24,0x87},  // -
	{0x4C26,0x91},  // -
	{0x4C28,0x91},  // -
	{0x4C2A,0x87},  // -
	{0x4C2C,0x78},  // -
	{0x4C2E,0x50},  // -
	{0x4C30,0x50},  // -
	{0x4C32,0x50},  // -
	{0x4C34,0x87},  // -
	{0x4C36,0x91},  // -
	{0x4C38,0x91},  // -
	{0x4C3A,0x87},  // -
	{0x4C3C,0x78},  // -
	{0x4C3E,0x50},  // -
	{0x4C40,0x50},  // -
	{0x4C42,0x50},  // -
	{0x4D12,0x1F},  // -
	{0x4D13,0x1E},  // -
	{0x4D26,0x33},  // -
	{0x4E0E,0x59},  // -
	{0x4E14,0x55},  // -
	{0x4E16,0x59},  // -
	{0x4E1E,0x3B},  // -
	{0x4E20,0x47},  // -
	{0x4E22,0x54},  // -
	{0x4E26,0x81},  // -
	{0x4E2C,0x7D},  // -
	{0x4E2E,0x81},  // -
	{0x4E36,0x63},  // -
	{0x4E38,0x6F},  // -
	{0x4E3A,0x7C},  // -
	{0x4F3A,0x3C},  // -
	{0x4F3C,0x46},  // -
	{0x4F3E,0x59},  // -
	{0x4F42,0x64},  // -
	{0x4F44,0x6E},  // -
	{0x4F46,0x81},  // -
	{0x4F4A,0x82},  // -
	{0x4F5A,0x81},  // -
	{0x4F62,0xAA},  // -
	{0x4F72,0xA9},  // -
	{0x4F78,0x36},  // -
	{0x4F7A,0x41},  // -
	{0x4F7C,0x61},  // -
	{0x4F7D,0x01},  // -
	{0x4F7E,0x7C},  // -
	{0x4F7F,0x01},  // -
	{0x4F80,0x77},  // -
	{0x4F82,0x7B},  // -
	{0x4F88,0x37},  // -
	{0x4F8A,0x40},  // -
	{0x4F8C,0x62},  // -
	{0x4F8D,0x01},  // -
	{0x4F8E,0x76},  // -
	{0x4F8F,0x01},  // -
	{0x4F90,0x5E},  // -
	{0x4F91,0x02},  // -
	{0x4F92,0x69},  // -
	{0x4F93,0x02},  // -
	{0x4F94,0x89},  // -
	{0x4F95,0x02},  // -
	{0x4F96,0xA4},  // -
	{0x4F97,0x02},  // -
	{0x4F98,0x9F},  // -
	{0x4F99,0x02},  // -
	{0x4F9A,0xA3},  // -
	{0x4F9B,0x02},  // -
	{0x4FA0,0x5F},  // -
	{0x4FA1,0x02},  // -
	{0x4FA2,0x68},  // -
	{0x4FA3,0x02},  // -
	{0x4FA4,0x8A},  // -
	{0x4FA5,0x02},  // -
	{0x4FA6,0x9E},  // -
	{0x4FA7,0x02},  // -
	{0x519E,0x79},  // -
	{0x51A6,0xA1},  // -
	{0x51F0,0xAC},  // -
	{0x51F2,0xAA},  // -
	{0x51F4,0xA5},  // -
	{0x51F6,0xA0},  // -
	{0x5200,0x9B},  // -
	{0x5202,0x91},  // -
	{0x5204,0x87},  // -
	{0x5206,0x82},  // -
	{0x5208,0xAC},  // -
	{0x520A,0xAA},  // -
	{0x520C,0xA5},  // -
	{0x520E,0xA0},  // -
	{0x5210,0x9B},  // -
	{0x5212,0x91},  // -
	{0x5214,0x87},  // -
	{0x5216,0x82},  // -
	{0x5218,0xAC},  // -
	{0x521A,0xAA},  // -
	{0x521C,0xA5},  // -
	{0x521E,0xA0},  // -
	{0x5220,0x9B},  // -
	{0x5222,0x91},  // -
	{0x5224,0x87},  // -
	{0x5226,0x82},  // -
	{0x5B3C,0x7F},  // -
	{0x3002,0x00},
	{0xffff,0x10},
	{0x3000,0x00},
	{0x0100,0x01},
	{REG_NULL, 0x00},
};
	

/*
 * The width and height must be configured to be
 * the same as the current output resolution of the sensor.
 * The input width of the isp needs to be 16 aligned.
 * The input height of the isp needs to be 8 aligned.
 * If the width or height does not meet the alignment rules,
 * you can configure the cropping parameters with the following function to
 * crop out the appropriate resolution.
 * struct v4l2_subdev_pad_ops {
 *	.get_selection
 * }
 */
static const struct imx585_mode supported_modes[] = {
	/*
	 * frame rate = 1 / (Vtt * 1H) = 1 / (VMAX * 1H)
	 * VMAX >= (PIX_VWIDTH / 2) + 46 = height + 46
	 * 2250
	 */
	{
		.bus_fmt = MEDIA_BUS_FMT_SRGGB12_1X12,
		.width = 3856,
		.height = 2180,
		.max_fps = {
			.numerator = 10000,
			.denominator = 600000,
		},
		.exp_def = 0x08ca - 0x6,//0x08ca -0x6,
		.hts_def = 0x0226 * IMX585_4LANES * 2,
		.vts_def = 0x08ca,//0x08ca,
		.global_reg_list = NULL,
		.reg_list = imx585_linear_12bit_3840x2160_1782M_regs_4lane,
		.hdr_mode = NO_HDR,
		.mipi_freq_idx = 3,
		.bpp = 12,
		.vc[PAD0] = V4L2_MBUS_CSI2_CHANNEL_0,
		.xvclk = IMX585_XVCLK_FREQ_37M,
	},
	{
		.bus_fmt = MEDIA_BUS_FMT_SRGGB10_1X10,
		.width = 3864,
		.height = 2192,
		.max_fps = {
			.numerator = 10000,
			.denominator = 300000,
		},
		.exp_def = 0x08ca - 0x08,
		.hts_def = 0x044c * IMX585_4LANES * 2,
		.vts_def = 0x08ca,
		.global_reg_list = imx585_global_10bit_3864x2192_regs,
		.reg_list = imx585_linear_10bit_3864x2192_891M_regs,
		.hdr_mode = NO_HDR,
		.mipi_freq_idx = 1,
		.bpp = 10,
		.vc[PAD0] = V4L2_MBUS_CSI2_CHANNEL_0,
		.xvclk = IMX585_XVCLK_FREQ_37M,
	},
	{
		.bus_fmt = MEDIA_BUS_FMT_SRGGB10_1X10,
		.width = 3864,
		.height = 2192,
		.max_fps = {
			.numerator = 10000,
			.denominator = 300000,
		},
		.exp_def = 0x08fc * 2 - 0x0da8,
		.hts_def = 0x0226 * IMX585_4LANES * 2,
		/*
		 * IMX585 HDR mode T-line is half of Linear mode,
		 * make vts double to workaround.
		 */
		.vts_def = 0x08fc * 2,
		.global_reg_list = imx585_global_10bit_3864x2192_regs,
		.reg_list = imx585_hdr2_10bit_3864x2192_1585M_regs,
		.hdr_mode = HDR_X2,
		.mipi_freq_idx = 2,
		.bpp = 10,
		.vc[PAD0] = V4L2_MBUS_CSI2_CHANNEL_1,
		.vc[PAD1] = V4L2_MBUS_CSI2_CHANNEL_0,//L->csi wr0
		.vc[PAD2] = V4L2_MBUS_CSI2_CHANNEL_1,
		.vc[PAD3] = V4L2_MBUS_CSI2_CHANNEL_1,//M->csi wr2
		.xvclk = IMX585_XVCLK_FREQ_37M,
	},
	{
		.bus_fmt = MEDIA_BUS_FMT_SRGGB10_1X10,
		.width = 3864,
		.height = 2192,
		.max_fps = {
			.numerator = 10000,
			.denominator = 200000,
		},
		.exp_def = 0x13e,
		.hts_def = 0x021A * IMX585_4LANES * 2,
		/*
		 * IMX585 HDR mode T-line is half of Linear mode,
		 * make vts double to workaround.
		 */
		.vts_def = 0x06BD * 4,
		.global_reg_list = imx585_global_10bit_3864x2192_regs,
		.reg_list = imx585_hdr3_10bit_3864x2192_1585M_regs,
		.hdr_mode = HDR_X3,
		.mipi_freq_idx = 2,
		.bpp = 10,
		.vc[PAD0] = V4L2_MBUS_CSI2_CHANNEL_2,
		.vc[PAD1] = V4L2_MBUS_CSI2_CHANNEL_1,//M->csi wr0
		.vc[PAD2] = V4L2_MBUS_CSI2_CHANNEL_0,//L->csi wr0
		.vc[PAD3] = V4L2_MBUS_CSI2_CHANNEL_2,//S->csi wr2
		.xvclk = IMX585_XVCLK_FREQ_37M,
	},
	{
		.bus_fmt = MEDIA_BUS_FMT_SRGGB10_1X10,
		.width = 3864,
		.height = 2192,
		.max_fps = {
			.numerator = 10000,
			.denominator = 200000,
		},
		.exp_def = 0x13e,
		.hts_def = 0x01ca * IMX585_4LANES * 2,
		/*
		 * IMX585 HDR mode T-line is half of Linear mode,
		 * make vts double to workaround.
		 */
		.vts_def = 0x07ea * 4,
		.global_reg_list = imx585_global_10bit_3864x2192_regs,
		.reg_list = imx585_hdr3_10bit_3864x2192_1782M_regs,
		.hdr_mode = HDR_X3,
		.mipi_freq_idx = 3,
		.bpp = 10,
		.vc[PAD0] = V4L2_MBUS_CSI2_CHANNEL_2,
		.vc[PAD1] = V4L2_MBUS_CSI2_CHANNEL_1,//M->csi wr0
		.vc[PAD2] = V4L2_MBUS_CSI2_CHANNEL_0,//L->csi wr0
		.vc[PAD3] = V4L2_MBUS_CSI2_CHANNEL_2,//S->csi wr2
		.xvclk = IMX585_XVCLK_FREQ_37M,
	},
	{
		/* 1H period = (1100 clock) = (1100 * 1 / 74.25MHz) */
		.bus_fmt = MEDIA_BUS_FMT_SRGGB12_1X12,
		.width = 3864,
		.height = 2192,
		.max_fps = {
			.numerator = 10000,
			.denominator = 300000,
		},
		.exp_def = 0x08ca - 0x08,
		.hts_def = 0x044c * IMX585_4LANES * 2,
		.vts_def = 0x08ca,
		.global_reg_list = imx585_global_12bit_3864x2192_regs,
		.reg_list = imx585_linear_12bit_3864x2192_891M_regs,
		.hdr_mode = NO_HDR,
		.mipi_freq_idx = 1,
		.bpp = 12,
		.vc[PAD0] = V4L2_MBUS_CSI2_CHANNEL_0,
		.xvclk = IMX585_XVCLK_FREQ_37M,
	},
	{
		.bus_fmt = MEDIA_BUS_FMT_SRGGB12_1X12,
		.width = 3864,
		.height = 2192,
		.max_fps = {
			.numerator = 10000,
			.denominator = 300000,
		},
		.exp_def = 0x08CA * 2 - 0x0d90,
		.hts_def = 0x0226 * IMX585_4LANES * 2,
		/*
		 * IMX585 HDR mode T-line is half of Linear mode,
		 * make vts double(that is FSC) to workaround.
		 */
		.vts_def = 0x08CA * 2,
		.global_reg_list = imx585_global_12bit_3864x2192_regs,
		.reg_list = imx585_hdr2_12bit_3864x2192_1782M_regs,
		.hdr_mode = HDR_X2,
		.mipi_freq_idx = 3,
		.bpp = 12,
		.vc[PAD0] = V4L2_MBUS_CSI2_CHANNEL_1,
		.vc[PAD1] = V4L2_MBUS_CSI2_CHANNEL_0,//L->csi wr0
		.vc[PAD2] = V4L2_MBUS_CSI2_CHANNEL_1,
		.vc[PAD3] = V4L2_MBUS_CSI2_CHANNEL_1,//M->csi wr2
		.xvclk = IMX585_XVCLK_FREQ_37M,
	},
	{
		.bus_fmt = MEDIA_BUS_FMT_SRGGB12_1X12,
		.width = 3864,
		.height = 2192,
		.max_fps = {
			.numerator = 10000,
			.denominator = 200000,
		},
		.exp_def = 0x114,
		.hts_def = 0x0226 * IMX585_4LANES * 2,
		/*
		 * IMX585 HDR mode T-line is half of Linear mode,
		 * make vts double(that is FSC) to workaround.
		 */
		.vts_def = 0x0696 * 4,
		.global_reg_list = imx585_global_12bit_3864x2192_regs,
		.reg_list = imx585_hdr3_12bit_3864x2192_1782M_regs,
		.hdr_mode = HDR_X3,
		.mipi_freq_idx = 3,
		.bpp = 12,
		.vc[PAD0] = V4L2_MBUS_CSI2_CHANNEL_2,
		.vc[PAD1] = V4L2_MBUS_CSI2_CHANNEL_1,//M->csi wr0
		.vc[PAD2] = V4L2_MBUS_CSI2_CHANNEL_0,//L->csi wr0
		.vc[PAD3] = V4L2_MBUS_CSI2_CHANNEL_2,//S->csi wr2
		.xvclk = IMX585_XVCLK_FREQ_37M,
	},
	{
		.bus_fmt = MEDIA_BUS_FMT_SRGGB12_1X12,
		.width = 1944,
		.height = 1097,
		.max_fps = {
			.numerator = 10000,
			.denominator = 300000,
		},
		.exp_def = 0x05dc - 0x08,
		.hts_def = 0x030e * 3,
		.vts_def = 0x0c5d,
		.global_reg_list = imx585_global_12bit_3864x2192_regs,
		.reg_list = imx585_linear_12bit_1932x1096_594M_regs,
		.hdr_mode = NO_HDR,
		.mipi_freq_idx = 0,
		.bpp = 12,
		.vc[PAD0] = V4L2_MBUS_CSI2_CHANNEL_0,
		.xvclk = IMX585_XVCLK_FREQ_37M,
	},
	{
		.bus_fmt = MEDIA_BUS_FMT_SRGGB12_1X12,
		.width = 1944,
		.height = 1097,
		.max_fps = {
			.numerator = 10000,
			.denominator = 300000,
		},
		.exp_def = 0x08FC / 4,
		.hts_def = 0x021A * 4,
		/*
		 * IMX585 HDR mode T-line is half of Linear mode,
		 * make vts double(that is FSC) to workaround.
		 */
		.vts_def = 0x08FC * 2,
		.global_reg_list = imx585_global_12bit_3864x2192_regs,
		.reg_list = imx585_hdr2_12bit_1932x1096_891M_regs,
		.hdr_mode = HDR_X2,
		.mipi_freq_idx = 1,
		.bpp = 12,
		.vc[PAD0] = V4L2_MBUS_CSI2_CHANNEL_1,
		.vc[PAD1] = V4L2_MBUS_CSI2_CHANNEL_0,//L->csi wr0
		.vc[PAD2] = V4L2_MBUS_CSI2_CHANNEL_1,
		.vc[PAD3] = V4L2_MBUS_CSI2_CHANNEL_1,//M->csi wr2
		.xvclk = IMX585_XVCLK_FREQ_37M,
	},
};

static const struct imx585_mode supported_modes_2lane[] = {
	{
		/* 1H period = (1100 clock) = (1100 * 1 / 74.25MHz) */
		.bus_fmt = MEDIA_BUS_FMT_SRGGB12_1X12,
		.width = 3864,
		.height = 2192,
		.max_fps = {
			.numerator = 10000,
			.denominator = 150000,
		},
		.exp_def = 0x08ca - 0x08,
		.hts_def = 0x0898 * IMX585_2LANES * 2,
		.vts_def = 0x08ca,
		.global_reg_list = NULL,
		.reg_list = imx585_linear_12bit_3864x2192_891M_regs_2lane,
		.hdr_mode = NO_HDR,
		.mipi_freq_idx = 1,
		.bpp = 12,
		.vc[PAD0] = V4L2_MBUS_CSI2_CHANNEL_0,
		.xvclk = IMX585_XVCLK_FREQ_27M,
	},
	{
		/* 1H period = (1100 clock) = (1100 * 1 / 74.25MHz) */
		.bus_fmt = MEDIA_BUS_FMT_SRGGB12_1X12,
		.width = 1284,
		.height = 720,
		.max_fps = {
			.numerator = 10000,
			.denominator = 900000,
		},
		.exp_def = 0x07AB-8,
		.hts_def = 0x01A4 * IMX585_2LANES * 2,
		.vts_def = 0x07AB,
		.global_reg_list = NULL,
		.reg_list = imx585_linear_12bit_1284x720_2376M_regs_2lane,
		.hdr_mode = NO_HDR,
		.mipi_freq_idx = 4,
		.bpp = 12,
		.vc[PAD0] = V4L2_MBUS_CSI2_CHANNEL_0,
		.xvclk = IMX585_XVCLK_FREQ_27M,
	},
};

static const s64 link_freq_items[] = {
	MIPI_FREQ_297M,
	MIPI_FREQ_446M,
	MIPI_FREQ_743M,
	MIPI_FREQ_891M,
	MIPI_FREQ_1188M,
	MIPI_FREQ_800M,
};

/* Write registers up to 4 at a time */
static int imx585_write_reg(struct i2c_client *client, u16 reg,
			    u32 len, u32 val)
{
	u32 buf_i, val_i;
	u8 buf[6];
	u8 *val_p;
	__be32 val_be;

	if (len > 4)
		return -EINVAL;

	buf[0] = reg >> 8;
	buf[1] = reg & 0xff;

	val_be = cpu_to_be32(val);
	val_p = (u8 *)&val_be;
	buf_i = 2;
	val_i = 4 - len;

	while (val_i < 4)
		buf[buf_i++] = val_p[val_i++];

	if (i2c_master_send(client, buf, len + 2) != len + 2)
		return -EIO;

	return 0;
}

static int imx585_write_array(struct i2c_client *client,
			      const struct regval *regs)
{
	u32 i;
	int ret = 0;
	if (!regs) {
		dev_err(&client->dev, "write reg array error\n");
		return ret;
	}
	for (i = 0; ret == 0 && regs[i].addr != REG_NULL; i++) {
		if (regs[i].addr == REG_DELAY) {
			usleep_range(regs[i].val * 1000, regs[i].val * 1000 + 500);
			dev_info(&client->dev, "write reg array, sleep %dms\n", regs[i].val);
		} else {
			ret = imx585_write_reg(client, regs[i].addr,
				IMX585_REG_VALUE_08BIT, regs[i].val);
		}
	}
	return ret;
}

/* Read registers up to 4 at a time */
static int imx585_read_reg(struct i2c_client *client, u16 reg, unsigned int len,
			   u32 *val)
{
	struct i2c_msg msgs[2];
	u8 *data_be_p;
	__be32 data_be = 0;
	__be16 reg_addr_be = cpu_to_be16(reg);
	int ret;

	if (len > 4 || !len)
		return -EINVAL;

	data_be_p = (u8 *)&data_be;
	/* Write register address */
	msgs[0].addr = client->addr;
	msgs[0].flags = 0;
	msgs[0].len = 2;
	msgs[0].buf = (u8 *)&reg_addr_be;

	/* Read data from register */
	msgs[1].addr = client->addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = len;
	msgs[1].buf = &data_be_p[4 - len];

	ret = i2c_transfer(client->adapter, msgs, ARRAY_SIZE(msgs));
	if (ret != ARRAY_SIZE(msgs))
		return -EIO;

	*val = be32_to_cpu(data_be);

	return 0;
}

static int imx585_get_reso_dist(const struct imx585_mode *mode,
				struct v4l2_mbus_framefmt *framefmt)
{
	return abs(mode->width - framefmt->width) +
	       abs(mode->height - framefmt->height);
}

static const struct imx585_mode *
imx585_find_best_fit(struct imx585 *imx585, struct v4l2_subdev_format *fmt)
{
	struct v4l2_mbus_framefmt *framefmt = &fmt->format;
	int dist;
	int cur_best_fit = 0;
	int cur_best_fit_dist = -1;
	unsigned int i;

	for (i = 0; i < imx585->cfg_num; i++) {
		dist = imx585_get_reso_dist(&imx585->supported_modes[i], framefmt);
		if ((cur_best_fit_dist == -1 || dist < cur_best_fit_dist) &&
			imx585->supported_modes[i].bus_fmt == framefmt->code) {
			cur_best_fit_dist = dist;
			cur_best_fit = i;
		}
	}
	dev_info(&imx585->client->dev, "%s: cur_best_fit(%d)",
		 __func__, cur_best_fit);

	return &imx585->supported_modes[cur_best_fit];
}

static int __imx585_power_on(struct imx585 *imx585);

static void imx585_change_mode(struct imx585 *imx585, const struct imx585_mode *mode)
{
	if (imx585->is_thunderboot && rkisp_tb_get_state() == RKISP_TB_NG) {
		imx585->is_thunderboot = false;
		imx585->is_thunderboot_ng = true;
		__imx585_power_on(imx585);
	}
	imx585->cur_mode = mode;
	imx585->cur_vts = imx585->cur_mode->vts_def;
	dev_info(&imx585->client->dev, "set fmt: cur_mode: %dx%d, hdr: %d, bpp: %d\n",
		mode->width, mode->height, mode->hdr_mode, mode->bpp);
}

static int imx585_set_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_pad_config *cfg,
			  struct v4l2_subdev_format *fmt)
{
	struct imx585 *imx585 = to_imx585(sd);
	const struct imx585_mode *mode;
	s64 h_blank, vblank_def, vblank_min;
	u64 pixel_rate = 0;
	u8 lanes = imx585->bus_cfg.bus.mipi_csi2.num_data_lanes;

	mutex_lock(&imx585->mutex);

	mode = imx585_find_best_fit(imx585, fmt);
	fmt->format.code = mode->bus_fmt;
	fmt->format.width = mode->width;
	fmt->format.height = mode->height;
	fmt->format.field = V4L2_FIELD_NONE;
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
		*v4l2_subdev_get_try_format(sd, cfg, fmt->pad) = fmt->format;
#else
		mutex_unlock(&imx585->mutex);
		return -ENOTTY;
#endif
	} else {
		imx585_change_mode(imx585, mode);
		h_blank = mode->hts_def - mode->width;
		__v4l2_ctrl_modify_range(imx585->hblank, h_blank,
					 h_blank, 1, h_blank);
		vblank_def = mode->vts_def - mode->height;
		/* VMAX >= (PIX_VWIDTH / 2) + 46 = height + 46 */
		vblank_min = (mode->height + 46) - mode->height;
		__v4l2_ctrl_modify_range(imx585->vblank, vblank_min,
					 IMX585_VTS_MAX - mode->height,
					 1, vblank_def);
		__v4l2_ctrl_s_ctrl(imx585->vblank, vblank_def);
		__v4l2_ctrl_s_ctrl(imx585->link_freq, mode->mipi_freq_idx);
		pixel_rate = (u32)link_freq_items[mode->mipi_freq_idx] /
			mode->bpp * 2 * lanes;
		__v4l2_ctrl_s_ctrl_int64(imx585->pixel_rate,
					 pixel_rate);
	}
	dev_info(&imx585->client->dev, "%s: mode->mipi_freq_idx(%d)",
		 __func__, mode->mipi_freq_idx);

	mutex_unlock(&imx585->mutex);

	return 0;
}

static int imx585_get_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_pad_config *cfg,
			  struct v4l2_subdev_format *fmt)
{
	struct imx585 *imx585 = to_imx585(sd);
	const struct imx585_mode *mode = imx585->cur_mode;

	mutex_lock(&imx585->mutex);
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
		fmt->format = *v4l2_subdev_get_try_format(sd, cfg, fmt->pad);
#else
		mutex_unlock(&imx585->mutex);
		return -ENOTTY;
#endif
	} else {
		fmt->format.width = mode->width;
		fmt->format.height = mode->height;
		fmt->format.code = mode->bus_fmt;
		fmt->format.field = V4L2_FIELD_NONE;
		if (fmt->pad < PAD_MAX && mode->hdr_mode != NO_HDR)
			fmt->reserved[0] = mode->vc[fmt->pad];
		else
			fmt->reserved[0] = mode->vc[PAD0];
	}
	mutex_unlock(&imx585->mutex);

	return 0;
}

static int imx585_enum_mbus_code(struct v4l2_subdev *sd,
				 struct v4l2_subdev_pad_config *cfg,
				 struct v4l2_subdev_mbus_code_enum *code)
{
	struct imx585 *imx585 = to_imx585(sd);

	if (code->index >= imx585->cfg_num)
		return -EINVAL;

	code->code = imx585->supported_modes[code->index].bus_fmt;

	return 0;
}

static int imx585_enum_frame_sizes(struct v4l2_subdev *sd,
				   struct v4l2_subdev_pad_config *cfg,
				   struct v4l2_subdev_frame_size_enum *fse)
{
	struct imx585 *imx585 = to_imx585(sd);

	if (fse->index >= imx585->cfg_num)
		return -EINVAL;

	if (fse->code != imx585->supported_modes[fse->index].bus_fmt)
		return -EINVAL;

	fse->min_width  = imx585->supported_modes[fse->index].width;
	fse->max_width  = imx585->supported_modes[fse->index].width;
	fse->max_height = imx585->supported_modes[fse->index].height;
	fse->min_height = imx585->supported_modes[fse->index].height;

	return 0;
}

static int imx585_g_frame_interval(struct v4l2_subdev *sd,
				   struct v4l2_subdev_frame_interval *fi)
{
	struct imx585 *imx585 = to_imx585(sd);
	const struct imx585_mode *mode = imx585->cur_mode;

	fi->interval = mode->max_fps;

	return 0;
}

static int imx585_g_mbus_config(struct v4l2_subdev *sd, unsigned int pad_id,
				struct v4l2_mbus_config *config)
{
	struct imx585 *imx585 = to_imx585(sd);
	const struct imx585_mode *mode = imx585->cur_mode;
	u32 val = 0;
	u8 lanes = imx585->bus_cfg.bus.mipi_csi2.num_data_lanes;

	val = 1 << (lanes - 1) |
	      V4L2_MBUS_CSI2_CHANNEL_0 |
	      V4L2_MBUS_CSI2_CONTINUOUS_CLOCK;
	if (mode->hdr_mode != NO_HDR)
		val |= V4L2_MBUS_CSI2_CHANNEL_1;
	if (mode->hdr_mode == HDR_X3)
		val |= V4L2_MBUS_CSI2_CHANNEL_2;
	config->type = V4L2_MBUS_CSI2_DPHY;
	config->flags = val;

	return 0;
}

static void imx585_get_module_inf(struct imx585 *imx585,
				  struct rkmodule_inf *inf)
{
	memset(inf, 0, sizeof(*inf));
	strlcpy(inf->base.sensor, IMX585_NAME, sizeof(inf->base.sensor));
	strlcpy(inf->base.module, imx585->module_name,
		sizeof(inf->base.module));
	strlcpy(inf->base.lens, imx585->len_name, sizeof(inf->base.lens));
}

static int imx585_set_hdrae_3frame(struct imx585 *imx585,
				   struct preisp_hdrae_exp_s *ae)
{
	struct i2c_client *client = imx585->client;
	u32 l_exp_time, m_exp_time, s_exp_time;
	u32 l_a_gain, m_a_gain, s_a_gain;
	int shr2, shr1, shr0, rhs2, rhs1 = 0;
	int rhs1_change_limit, rhs2_change_limit = 0;
	static int rhs1_old = IMX585_RHS1_DEFAULT;
	static int rhs2_old = IMX585_RHS2_DEFAULT;
	int ret = 0;
	u32 fsc;
	int rhs1_max = 0;
	int shr2_min = 0;

	if (!imx585->has_init_exp && !imx585->streaming) {
		imx585->init_hdrae_exp = *ae;
		imx585->has_init_exp = true;
		dev_dbg(&imx585->client->dev, "imx585 is not streaming, save hdr ae!\n");
		return ret;
	}
	l_exp_time = ae->long_exp_reg;
	m_exp_time = ae->middle_exp_reg;
	s_exp_time = ae->short_exp_reg;
	l_a_gain = ae->long_gain_reg;
	m_a_gain = ae->middle_gain_reg;
	s_a_gain = ae->short_gain_reg;
	dev_dbg(&client->dev,
		"rev exp req: L_exp: 0x%x, 0x%x, M_exp: 0x%x, 0x%x S_exp: 0x%x, 0x%x\n",
		l_exp_time, m_exp_time, s_exp_time,
		l_a_gain, m_a_gain, s_a_gain);

	ret = imx585_write_reg(client, IMX585_GROUP_HOLD_REG,
		IMX585_REG_VALUE_08BIT, IMX585_GROUP_HOLD_START);
	/* gain effect n+1 */
	ret |= imx585_write_reg(client, IMX585_LF_GAIN_REG_H,
		IMX585_REG_VALUE_08BIT, IMX585_FETCH_GAIN_H(l_a_gain));
	ret |= imx585_write_reg(client, IMX585_LF_GAIN_REG_L,
		IMX585_REG_VALUE_08BIT, IMX585_FETCH_GAIN_L(l_a_gain));
	ret |= imx585_write_reg(client, IMX585_SF1_GAIN_REG_H,
		IMX585_REG_VALUE_08BIT, IMX585_FETCH_GAIN_H(m_a_gain));
	ret |= imx585_write_reg(client, IMX585_SF1_GAIN_REG_L,
		IMX585_REG_VALUE_08BIT, IMX585_FETCH_GAIN_L(m_a_gain));
	ret |= imx585_write_reg(client, IMX585_SF2_GAIN_REG_H,
		IMX585_REG_VALUE_08BIT, IMX585_FETCH_GAIN_H(s_a_gain));
	ret |= imx585_write_reg(client, IMX585_SF2_GAIN_REG_L,
		IMX585_REG_VALUE_08BIT, IMX585_FETCH_GAIN_L(s_a_gain));

	/* Restrictions
	 *   FSC = 4 * VMAX and FSC should be 6n;
	 *   exp_l = FSC - SHR0 + Toffset;
	 *
	 *   SHR0 = FSC - exp_l + Toffset;
	 *   SHR0 <= (FSC -12);
	 *   SHR0 >= RHS2 + 13;
	 *   SHR0 should be 3n;
	 *
	 *   exp_m = RHS1 - SHR1 + Toffset;
	 *
	 *   RHS1 < BRL * 3;
	 *   RHS1 <= SHR2 - 13;
	 *   RHS1 >= SHR1 + 12;
	 *   SHR1 >= 13;
	 *   SHR1 <= RHS1 - 12;
	 *   RHS1(n+1) >= RHS1(n) + BRL * 3 -FSC + 3;
	 *
	 *   SHR1 should be 3n+1 and RHS1 should be 6n+1;
	 *
	 *   exp_s = RHS2 - SHR2 + Toffset;
	 *
	 *   RHS2 < BRL * 3 + RHS1;
	 *   RHS2 <= SHR0 - 13;
	 *   RHS2 >= SHR2 + 12;
	 *   SHR2 >= RHS1 + 13;
	 *   SHR2 <= RHS2 - 12;
	 *   RHS1(n+1) >= RHS1(n) + BRL * 3 -FSC + 3;
	 *
	 *   SHR2 should be 3n+2 and RHS2 should be 6n+2;
	 */

	/* The HDR mode vts is double by default to workaround T-line */
	fsc = imx585->cur_vts;
	fsc = fsc / 6 * 6;
	shr0 = fsc - l_exp_time;
	dev_dbg(&client->dev,
		"line(%d) shr0 %d, l_exp_time %d, fsc %d\n",
		__LINE__, shr0, l_exp_time, fsc);

	rhs1 = (SHR1_MIN_X3 + m_exp_time + 5) / 6 * 6 + 1;
	if (imx585->cur_mode->height == 2192)
		rhs1_max = RHS1_MAX_X3(BRL_ALL);
	else
		rhs1_max = RHS1_MAX_X3(BRL_BINNING);
	if (rhs1 < 25)
		rhs1 = 25;
	else if (rhs1 > rhs1_max)
		rhs1 = rhs1_max;
	dev_dbg(&client->dev,
		"line(%d) rhs1 %d, m_exp_time %d rhs1_old %d\n",
		__LINE__, rhs1, m_exp_time, rhs1_old);

	//Dynamic adjustment rhs2 must meet the following conditions
	if (imx585->cur_mode->height == 2192)
		rhs1_change_limit = rhs1_old + 3 * BRL_ALL - fsc + 3;
	else
		rhs1_change_limit = rhs1_old + 3 * BRL_BINNING - fsc + 3;
	rhs1_change_limit = (rhs1_change_limit < 25) ? 25 : rhs1_change_limit;
	rhs1_change_limit = (rhs1_change_limit + 5) / 6 * 6 + 1;
	if (rhs1_max < rhs1_change_limit) {
		dev_err(&client->dev,
			"The total exposure limit makes rhs1 max is %d,but old rhs1 limit makes rhs1 min is %d\n",
			rhs1_max, rhs1_change_limit);
		return -EINVAL;
	}
	if (rhs1 < rhs1_change_limit)
		rhs1 = rhs1_change_limit;

	dev_dbg(&client->dev,
		"line(%d) m_exp_time %d rhs1_old %d, rhs1_new %d\n",
		__LINE__, m_exp_time, rhs1_old, rhs1);

	rhs1_old = rhs1;

	/* shr1 = rhs1 - s_exp_time */
	if (rhs1 - m_exp_time <= SHR1_MIN_X3) {
		shr1 = SHR1_MIN_X3;
		m_exp_time = rhs1 - shr1;
	} else {
		shr1 = rhs1 - m_exp_time;
	}

	shr2_min = rhs1 + 13;
	rhs2 = (shr2_min + s_exp_time + 5) / 6 * 6 + 2;
	if (rhs2 > (shr0 - 13))
		rhs2 = shr0 - 13;
	else if (rhs2 < 50)
		rhs2 = 50;
	dev_dbg(&client->dev,
		"line(%d) rhs2 %d, s_exp_time %d, rhs2_old %d\n",
		__LINE__, rhs2, s_exp_time, rhs2_old);

	//Dynamic adjustment rhs2 must meet the following conditions
	if (imx585->cur_mode->height == 2192)
		rhs2_change_limit = rhs2_old + 3 * BRL_ALL - fsc + 3;
	else
		rhs2_change_limit = rhs2_old + 3 * BRL_BINNING - fsc + 3;
	rhs2_change_limit = (rhs2_change_limit < 50) ?  50 : rhs2_change_limit;
	rhs2_change_limit = (rhs2_change_limit + 5) / 6 * 6 + 2;
	if ((shr0 - 13) < rhs2_change_limit) {
		dev_err(&client->dev,
			"The total exposure limit makes rhs2 max is %d,but old rhs1 limit makes rhs2 min is %d\n",
			shr0 - 13, rhs2_change_limit);
		return -EINVAL;
	}
	if (rhs2 < rhs2_change_limit)
		rhs2 = rhs2_change_limit;

	rhs2_old = rhs2;

	/* shr2 = rhs2 - s_exp_time */
	if (rhs2 - s_exp_time <= shr2_min) {
		shr2 = shr2_min;
		s_exp_time = rhs2 - shr2;
	} else {
		shr2 = rhs2 - s_exp_time;
	}
	dev_dbg(&client->dev,
		"line(%d) rhs2_new %d, s_exp_time %d shr2 %d, rhs2_change_limit %d\n",
		__LINE__, rhs2, s_exp_time, shr2, rhs2_change_limit);

	if (shr0 < rhs2 + 13)
		shr0 = rhs2 + 13;
	else if (shr0 > fsc - 12)
		shr0 = fsc - 12;

	dev_dbg(&client->dev,
		"long exposure: l_exp_time=%d, fsc=%d, shr0=%d, l_a_gain=%d\n",
		l_exp_time, fsc, shr0, l_a_gain);
	dev_dbg(&client->dev,
		"middle exposure(SEF1): m_exp_time=%d, rhs1=%d, shr1=%d, m_a_gain=%d\n",
		m_exp_time, rhs1, shr1, m_a_gain);
	dev_dbg(&client->dev,
		"short exposure(SEF2): s_exp_time=%d, rhs2=%d, shr2=%d, s_a_gain=%d\n",
		s_exp_time, rhs2, shr2, s_a_gain);
	/* time effect n+1 */
	/* write SEF2 exposure RHS2 regs*/
	ret |= imx585_write_reg(client,
		IMX585_RHS2_REG_L,
		IMX585_REG_VALUE_08BIT,
		IMX585_FETCH_RHS1_L(rhs2));
	ret |= imx585_write_reg(client,
		IMX585_RHS2_REG_M,
		IMX585_REG_VALUE_08BIT,
		IMX585_FETCH_RHS1_M(rhs2));
	ret |= imx585_write_reg(client,
		IMX585_RHS2_REG_H,
		IMX585_REG_VALUE_08BIT,
		IMX585_FETCH_RHS1_H(rhs2));
	/* write SEF2 exposure SHR2 regs*/
	ret |= imx585_write_reg(client,
		IMX585_SF2_EXPO_REG_L,
		IMX585_REG_VALUE_08BIT,
		IMX585_FETCH_EXP_L(shr2));
	ret |= imx585_write_reg(client,
		IMX585_SF2_EXPO_REG_M,
		IMX585_REG_VALUE_08BIT,
		IMX585_FETCH_EXP_M(shr2));
	ret |= imx585_write_reg(client,
		IMX585_SF2_EXPO_REG_H,
		IMX585_REG_VALUE_08BIT,
		IMX585_FETCH_EXP_H(shr2));
	/* write SEF1 exposure RHS1 regs*/
	ret |= imx585_write_reg(client,
		IMX585_RHS1_REG_L,
		IMX585_REG_VALUE_08BIT,
		IMX585_FETCH_RHS1_L(rhs1));
	ret |= imx585_write_reg(client,
		IMX585_RHS1_REG_M,
		IMX585_REG_VALUE_08BIT,
		IMX585_FETCH_RHS1_M(rhs1));
	ret |= imx585_write_reg(client,
		IMX585_RHS1_REG_H,
		IMX585_REG_VALUE_08BIT,
		IMX585_FETCH_RHS1_H(rhs1));
	/* write SEF1 exposure SHR1 regs*/
	ret |= imx585_write_reg(client,
		IMX585_SF1_EXPO_REG_L,
		IMX585_REG_VALUE_08BIT,
		IMX585_FETCH_EXP_L(shr1));
	ret |= imx585_write_reg(client,
		IMX585_SF1_EXPO_REG_M,
		IMX585_REG_VALUE_08BIT,
		IMX585_FETCH_EXP_M(shr1));
	ret |= imx585_write_reg(client,
		IMX585_SF1_EXPO_REG_H,
		IMX585_REG_VALUE_08BIT,
		IMX585_FETCH_EXP_H(shr1));
	/* write LF exposure SHR0 regs*/
	ret |= imx585_write_reg(client,
		IMX585_LF_EXPO_REG_L,
		IMX585_REG_VALUE_08BIT,
		IMX585_FETCH_EXP_L(shr0));
	ret |= imx585_write_reg(client,
		IMX585_LF_EXPO_REG_M,
		IMX585_REG_VALUE_08BIT,
		IMX585_FETCH_EXP_M(shr0));
	ret |= imx585_write_reg(client,
		IMX585_LF_EXPO_REG_H,
		IMX585_REG_VALUE_08BIT,
		IMX585_FETCH_EXP_H(shr0));

	ret |= imx585_write_reg(client, IMX585_GROUP_HOLD_REG,
		IMX585_REG_VALUE_08BIT, IMX585_GROUP_HOLD_END);
	return ret;
}

static int imx585_set_hdrae(struct imx585 *imx585,
			    struct preisp_hdrae_exp_s *ae)
{
	struct i2c_client *client = imx585->client;
	u32 l_exp_time, m_exp_time, s_exp_time;
	u32 l_a_gain, m_a_gain, s_a_gain;
	int shr1, shr0, rhs1, rhs1_max, rhs1_min;
	static int rhs1_old = IMX585_RHS1_DEFAULT;
	int ret = 0;
	u32 fsc;

	if (!imx585->has_init_exp && !imx585->streaming) {
		imx585->init_hdrae_exp = *ae;
		imx585->has_init_exp = true;
		dev_dbg(&imx585->client->dev, "imx585 is not streaming, save hdr ae!\n");
		return ret;
	}
	l_exp_time = ae->long_exp_reg;
	m_exp_time = ae->middle_exp_reg;
	s_exp_time = ae->short_exp_reg;
	l_a_gain = ae->long_gain_reg;
	m_a_gain = ae->middle_gain_reg;
	s_a_gain = ae->short_gain_reg;
	dev_dbg(&client->dev,
		"rev exp req: L_exp: 0x%x, 0x%x, M_exp: 0x%x, 0x%x S_exp: 0x%x, 0x%x\n",
		l_exp_time, m_exp_time, s_exp_time,
		l_a_gain, m_a_gain, s_a_gain);

	if (imx585->cur_mode->hdr_mode == HDR_X2) {
		l_a_gain = m_a_gain;
		l_exp_time = m_exp_time;
	}

	ret = imx585_write_reg(client, IMX585_GROUP_HOLD_REG,
		IMX585_REG_VALUE_08BIT, IMX585_GROUP_HOLD_START);
	/* gain effect n+1 */
	ret |= imx585_write_reg(client, IMX585_LF_GAIN_REG_H,
		IMX585_REG_VALUE_08BIT, IMX585_FETCH_GAIN_H(l_a_gain));
	ret |= imx585_write_reg(client, IMX585_LF_GAIN_REG_L,
		IMX585_REG_VALUE_08BIT, IMX585_FETCH_GAIN_L(l_a_gain));
	ret |= imx585_write_reg(client, IMX585_SF1_GAIN_REG_H,
		IMX585_REG_VALUE_08BIT, IMX585_FETCH_GAIN_H(s_a_gain));
	ret |= imx585_write_reg(client, IMX585_SF1_GAIN_REG_L,
		IMX585_REG_VALUE_08BIT, IMX585_FETCH_GAIN_L(s_a_gain));

	/* Restrictions
	 *   FSC = 2 * VMAX and FSC should be 4n;
	 *   exp_l = FSC - SHR0 + Toffset;
	 *   exp_l should be even value;
	 *
	 *   SHR0 = FSC - exp_l + Toffset;
	 *   SHR0 <= (FSC -8);
	 *   SHR0 >= RHS1 + 9;
	 *   SHR0 should be 2n;
	 *
	 *   exp_s = RHS1 - SHR1 + Toffset;
	 *   exp_s should be even value;
	 *
	 *   RHS1 < BRL * 2;
	 *   RHS1 <= SHR0 - 9;
	 *   RHS1 >= SHR1 + 8;
	 *   SHR1 >= 9;
	 *   RHS1(n+1) >= RHS1(n) + BRL * 2 -FSC + 2;
	 *
	 *   SHR1 should be 2n+1 and RHS1 should be 4n+1;
	 */

	/* The HDR mode vts is double by default to workaround T-line */
	fsc = imx585->cur_vts;
	shr0 = fsc - l_exp_time;

	if (imx585->cur_mode->height == 2192) {
		rhs1_max = min(RHS1_MAX_X2(BRL_ALL), ((shr0 - 9u) / 4 * 4 + 1));
		rhs1_min = max(SHR1_MIN_X2 + 8u, rhs1_old + 2 * BRL_ALL - fsc + 2);
	} else {
		rhs1_max = min(RHS1_MAX_X2(BRL_BINNING), ((shr0 - 9u) / 4 * 4 + 1));
		rhs1_min = max(SHR1_MIN_X2 + 8u, rhs1_old + 2 * BRL_BINNING - fsc + 2);
	}
	rhs1_min = (rhs1_min + 3) / 4 * 4 + 1;
	rhs1 = (SHR1_MIN_X2 + s_exp_time + 3) / 4 * 4 + 1;/* shall be 4n + 1 */
	dev_dbg(&client->dev,
		"line(%d) rhs1 %d, rhs1 min %d rhs1 max %d\n",
		__LINE__, rhs1, rhs1_min, rhs1_max);
	if (rhs1_max < rhs1_min) {
		dev_err(&client->dev,
			"The total exposure limit makes rhs1 max is %d,but old rhs1 limit makes rhs1 min is %d\n",
			rhs1_max, rhs1_min);
		return -EINVAL;
	}
	rhs1 = clamp(rhs1, rhs1_min, rhs1_max);
	dev_dbg(&client->dev,
		"line(%d) rhs1 %d, short time %d rhs1_old %d, rhs1_new %d\n",
		__LINE__, rhs1, s_exp_time, rhs1_old, rhs1);

	rhs1_old = rhs1;

	/* shr1 = rhs1 - s_exp_time */
	if (rhs1 - s_exp_time <= SHR1_MIN_X2) {
		shr1 = SHR1_MIN_X2;
		s_exp_time = rhs1 - shr1;
	} else {
		shr1 = rhs1 - s_exp_time;
	}

	if (shr0 < rhs1 + 9)
		shr0 = rhs1 + 9;
	else if (shr0 > fsc - 8)
		shr0 = fsc - 8;

	dev_dbg(&client->dev,
		"fsc=%d,RHS1_MAX=%d,SHR1_MIN=%d,rhs1_max=%d\n",
		fsc, RHS1_MAX_X2(BRL_ALL), SHR1_MIN_X2, rhs1_max);
	dev_dbg(&client->dev,
		"l_exp_time=%d,s_exp_time=%d,shr0=%d,shr1=%d,rhs1=%d,l_a_gain=%d,s_a_gain=%d\n",
		l_exp_time, s_exp_time, shr0, shr1, rhs1, l_a_gain, s_a_gain);
	/* time effect n+2 */
	ret |= imx585_write_reg(client,
		IMX585_RHS1_REG_L,
		IMX585_REG_VALUE_08BIT,
		IMX585_FETCH_RHS1_L(rhs1));
	ret |= imx585_write_reg(client,
		IMX585_RHS1_REG_M,
		IMX585_REG_VALUE_08BIT,
		IMX585_FETCH_RHS1_M(rhs1));
	ret |= imx585_write_reg(client,
		IMX585_RHS1_REG_H,
		IMX585_REG_VALUE_08BIT,
		IMX585_FETCH_RHS1_H(rhs1));

	ret |= imx585_write_reg(client,
		IMX585_SF1_EXPO_REG_L,
		IMX585_REG_VALUE_08BIT,
		IMX585_FETCH_EXP_L(shr1));
	ret |= imx585_write_reg(client,
		IMX585_SF1_EXPO_REG_M,
		IMX585_REG_VALUE_08BIT,
		IMX585_FETCH_EXP_M(shr1));
	ret |= imx585_write_reg(client,
		IMX585_SF1_EXPO_REG_H,
		IMX585_REG_VALUE_08BIT,
		IMX585_FETCH_EXP_H(shr1));
	ret |= imx585_write_reg(client,
		IMX585_LF_EXPO_REG_L,
		IMX585_REG_VALUE_08BIT,
		IMX585_FETCH_EXP_L(shr0));
	ret |= imx585_write_reg(client,
		IMX585_LF_EXPO_REG_M,
		IMX585_REG_VALUE_08BIT,
		IMX585_FETCH_EXP_M(shr0));
	ret |= imx585_write_reg(client,
		IMX585_LF_EXPO_REG_H,
		IMX585_REG_VALUE_08BIT,
		IMX585_FETCH_EXP_H(shr0));

	ret |= imx585_write_reg(client, IMX585_GROUP_HOLD_REG,
		IMX585_REG_VALUE_08BIT, IMX585_GROUP_HOLD_END);
	return ret;
}

static int imx585_get_channel_info(struct imx585 *imx585, struct rkmodule_channel_info *ch_info)
{
	if (ch_info->index < PAD0 || ch_info->index >= PAD_MAX)
		return -EINVAL;
	ch_info->vc = imx585->cur_mode->vc[ch_info->index];
	ch_info->width = imx585->cur_mode->width;
	ch_info->height = imx585->cur_mode->height;
	ch_info->bus_fmt = imx585->cur_mode->bus_fmt;
	return 0;
}

static long imx585_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	struct imx585 *imx585 = to_imx585(sd);
	struct rkmodule_hdr_cfg *hdr;
	struct rkmodule_channel_info *ch_info;
	u32 i, h, w, stream;
	long ret = 0;
	const struct imx585_mode *mode;
	u64 pixel_rate = 0;
	struct rkmodule_csi_dphy_param *dphy_param;
	u8 lanes = imx585->bus_cfg.bus.mipi_csi2.num_data_lanes;

	switch (cmd) {
	case PREISP_CMD_SET_HDRAE_EXP:
		if (imx585->cur_mode->hdr_mode == HDR_X2)
			ret = imx585_set_hdrae(imx585, arg);
		else if (imx585->cur_mode->hdr_mode == HDR_X3)
			ret = imx585_set_hdrae_3frame(imx585, arg);
		break;
	case RKMODULE_GET_MODULE_INFO:
		imx585_get_module_inf(imx585, (struct rkmodule_inf *)arg);
		break;
	case RKMODULE_GET_HDR_CFG:
		hdr = (struct rkmodule_hdr_cfg *)arg;
		hdr->esp.mode = HDR_NORMAL_VC;
		hdr->hdr_mode = imx585->cur_mode->hdr_mode;
		break;
	case RKMODULE_SET_HDR_CFG:
		hdr = (struct rkmodule_hdr_cfg *)arg;
		w = imx585->cur_mode->width;
		h = imx585->cur_mode->height;
		for (i = 0; i < imx585->cfg_num; i++) {
			if (w == imx585->supported_modes[i].width &&
			    h == imx585->supported_modes[i].height &&
			    imx585->supported_modes[i].hdr_mode == hdr->hdr_mode) {
				dev_info(&imx585->client->dev, "set hdr cfg, set mode to %d\n", i);
				imx585_change_mode(imx585, &imx585->supported_modes[i]);
				break;
			}
		}
		if (i == imx585->cfg_num) {
			dev_err(&imx585->client->dev,
				"not find hdr mode:%d %dx%d config\n",
				hdr->hdr_mode, w, h);
			ret = -EINVAL;
		} else {
			mode = imx585->cur_mode;
			if (imx585->streaming) {
				ret = imx585_write_reg(imx585->client, IMX585_GROUP_HOLD_REG,
					IMX585_REG_VALUE_08BIT, IMX585_GROUP_HOLD_START);

				ret |= imx585_write_array(imx585->client, imx585->cur_mode->reg_list);

				ret |= imx585_write_reg(imx585->client, IMX585_GROUP_HOLD_REG,
					IMX585_REG_VALUE_08BIT, IMX585_GROUP_HOLD_END);
				if (ret)
					return ret;
			}
			w = mode->hts_def - imx585->cur_mode->width;
			h = mode->vts_def - mode->height;
			mutex_lock(&imx585->mutex);
			__v4l2_ctrl_modify_range(imx585->hblank, w, w, 1, w);
			__v4l2_ctrl_modify_range(imx585->vblank, h,
				IMX585_VTS_MAX - mode->height,
				1, h);
			__v4l2_ctrl_s_ctrl(imx585->link_freq, mode->mipi_freq_idx);
			pixel_rate = (u32)link_freq_items[mode->mipi_freq_idx] /
				mode->bpp * 2 * lanes;
			__v4l2_ctrl_s_ctrl_int64(imx585->pixel_rate,
						 pixel_rate);
			mutex_unlock(&imx585->mutex);
		}
		break;
	case RKMODULE_SET_QUICK_STREAM:

		stream = *((u32 *)arg);

		if (stream)
			ret = imx585_write_reg(imx585->client, IMX585_REG_CTRL_MODE,
				IMX585_REG_VALUE_08BIT, IMX585_MODE_STREAMING);
		else
			ret = imx585_write_reg(imx585->client, IMX585_REG_CTRL_MODE,
				IMX585_REG_VALUE_08BIT, IMX585_MODE_SW_STANDBY);
		break;
	case RKMODULE_GET_SONY_BRL:
		if (imx585->cur_mode->width == 3864 && imx585->cur_mode->height == 2192)
			*((u32 *)arg) = BRL_ALL;
		else
			*((u32 *)arg) = BRL_BINNING;
		break;
	case RKMODULE_GET_CHANNEL_INFO:
		ch_info = (struct rkmodule_channel_info *)arg;
		ret = imx585_get_channel_info(imx585, ch_info);
		break;
	case RKMODULE_GET_CSI_DPHY_PARAM:
		if (imx585->cur_mode->hdr_mode == HDR_X2) {
			dphy_param = (struct rkmodule_csi_dphy_param *)arg;
			*dphy_param = dcphy_param;
			dev_info(&imx585->client->dev,
				 "get sensor dphy param\n");
		} else
			ret = -EINVAL;
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static long imx585_compat_ioctl32(struct v4l2_subdev *sd,
				  unsigned int cmd, unsigned long arg)
{
	void __user *up = compat_ptr(arg);
	struct rkmodule_inf *inf;
	struct rkmodule_awb_cfg *cfg;
	struct rkmodule_hdr_cfg *hdr;
	struct preisp_hdrae_exp_s *hdrae;
	struct rkmodule_channel_info *ch_info;
	long ret;
	u32  stream;
	u32 brl = 0;
	struct rkmodule_csi_dphy_param *dphy_param;

	switch (cmd) {
	case RKMODULE_GET_MODULE_INFO:
		inf = kzalloc(sizeof(*inf), GFP_KERNEL);
		if (!inf) {
			ret = -ENOMEM;
			return ret;
		}

		ret = imx585_ioctl(sd, cmd, inf);
		if (!ret) {
			if (copy_to_user(up, inf, sizeof(*inf))) {
				kfree(inf);
				return -EFAULT;
			}
		}
		kfree(inf);
		break;
	case RKMODULE_AWB_CFG:
		cfg = kzalloc(sizeof(*cfg), GFP_KERNEL);
		if (!cfg) {
			ret = -ENOMEM;
			return ret;
		}

		if (copy_from_user(cfg, up, sizeof(*cfg))) {
			kfree(cfg);
			return -EFAULT;
		}
		ret = imx585_ioctl(sd, cmd, cfg);
		kfree(cfg);
		break;
	case RKMODULE_GET_HDR_CFG:
		hdr = kzalloc(sizeof(*hdr), GFP_KERNEL);
		if (!hdr) {
			ret = -ENOMEM;
			return ret;
		}

		ret = imx585_ioctl(sd, cmd, hdr);
		if (!ret) {
			if (copy_to_user(up, hdr, sizeof(*hdr))) {
				kfree(hdr);
				return -EFAULT;
			}
		}
		kfree(hdr);
		break;
	case RKMODULE_SET_HDR_CFG:
		hdr = kzalloc(sizeof(*hdr), GFP_KERNEL);
		if (!hdr) {
			ret = -ENOMEM;
			return ret;
		}

		if (copy_from_user(hdr, up, sizeof(*hdr))) {
			kfree(hdr);
			return -EFAULT;
		}
		ret = imx585_ioctl(sd, cmd, hdr);
		kfree(hdr);
		break;
	case PREISP_CMD_SET_HDRAE_EXP:
		hdrae = kzalloc(sizeof(*hdrae), GFP_KERNEL);
		if (!hdrae) {
			ret = -ENOMEM;
			return ret;
		}

		if (copy_from_user(hdrae, up, sizeof(*hdrae))) {
			kfree(hdrae);
			return -EFAULT;
		}
		ret = imx585_ioctl(sd, cmd, hdrae);
		kfree(hdrae);
		break;
	case RKMODULE_SET_QUICK_STREAM:
		if (copy_from_user(&stream, up, sizeof(u32)))
			return -EFAULT;
		ret = imx585_ioctl(sd, cmd, &stream);
		break;
	case RKMODULE_GET_SONY_BRL:
		ret = imx585_ioctl(sd, cmd, &brl);
		if (!ret) {
			if (copy_to_user(up, &brl, sizeof(u32)))
				return -EFAULT;
		}
		break;
	case RKMODULE_GET_CHANNEL_INFO:
		ch_info = kzalloc(sizeof(*ch_info), GFP_KERNEL);
		if (!ch_info) {
			ret = -ENOMEM;
			return ret;
		}

		ret = imx585_ioctl(sd, cmd, ch_info);
		if (!ret) {
			ret = copy_to_user(up, ch_info, sizeof(*ch_info));
			if (ret)
				ret = -EFAULT;
		}
		kfree(ch_info);
		break;
	case RKMODULE_GET_CSI_DPHY_PARAM:
		dphy_param = kzalloc(sizeof(*dphy_param), GFP_KERNEL);
		if (!dphy_param) {
			ret = -ENOMEM;
			return ret;
		}

		ret = imx585_ioctl(sd, cmd, dphy_param);
		if (!ret) {
			ret = copy_to_user(up, dphy_param, sizeof(*dphy_param));
			if (ret)
				ret = -EFAULT;
		}
		kfree(dphy_param);
		break;

	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}
#endif


static int __imx585_start_stream(struct imx585 *imx585)
{
	int ret;

	if (!imx585->is_thunderboot) {
		if (imx585->cur_mode->global_reg_list) {
			ret = imx585_write_array(imx585->client, imx585->cur_mode->global_reg_list);
			if (ret)
				return ret;
		}
		ret = imx585_write_array(imx585->client, imx585->cur_mode->reg_list);
		if (ret)
			return ret;
	}

	/* In case these controls are set before streaming */
	ret = __v4l2_ctrl_handler_setup(&imx585->ctrl_handler);
	if (ret)
		return ret;
	if (imx585->has_init_exp && imx585->cur_mode->hdr_mode != NO_HDR) {
		ret = imx585_ioctl(&imx585->subdev, PREISP_CMD_SET_HDRAE_EXP,
			&imx585->init_hdrae_exp);
		if (ret) {
			dev_err(&imx585->client->dev,
				"init exp fail in hdr mode\n");
			return ret;
		}
	}
	return imx585_write_reg(imx585->client, IMX585_REG_CTRL_MODE,
				IMX585_REG_VALUE_08BIT, 0);
}

static int __imx585_stop_stream(struct imx585 *imx585)
{
	imx585->has_init_exp = false;
	if (imx585->is_thunderboot)
		imx585->is_first_streamoff = true;
	return imx585_write_reg(imx585->client, IMX585_REG_CTRL_MODE,
				IMX585_REG_VALUE_08BIT, 1);
}

static int imx585_s_stream(struct v4l2_subdev *sd, int on)
{
	struct imx585 *imx585 = to_imx585(sd);
	struct i2c_client *client = imx585->client;
	int ret = 0;

	dev_info(&imx585->client->dev, "s_stream: %d. %dx%d, hdr: %d, bpp: %d\n",
	       on, imx585->cur_mode->width, imx585->cur_mode->height,
	       imx585->cur_mode->hdr_mode, imx585->cur_mode->bpp);

	mutex_lock(&imx585->mutex);
	on = !!on;
	if (on == imx585->streaming)
		goto unlock_and_return;

	if (on) {
		if (imx585->is_thunderboot && rkisp_tb_get_state() == RKISP_TB_NG) {
			imx585->is_thunderboot = false;
			__imx585_power_on(imx585);
		}
		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}

		ret = __imx585_start_stream(imx585);
		if (ret) {
			v4l2_err(sd, "start stream failed while write regs\n");
			pm_runtime_put(&client->dev);
			goto unlock_and_return;
		}
	} else {
		__imx585_stop_stream(imx585);
		pm_runtime_put(&client->dev);
	}

	imx585->streaming = on;

unlock_and_return:
	mutex_unlock(&imx585->mutex);

	return ret;
}

static int imx585_s_power(struct v4l2_subdev *sd, int on)
{
	struct imx585 *imx585 = to_imx585(sd);
	struct i2c_client *client = imx585->client;
	int ret = 0;

	mutex_lock(&imx585->mutex);

	if (imx585->power_on == !!on)
		goto unlock_and_return;

	if (on) {
		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}
		imx585->power_on = true;
	} else {
		pm_runtime_put(&client->dev);
		imx585->power_on = false;
	}

unlock_and_return:
	mutex_unlock(&imx585->mutex);

	return ret;
}

int __imx585_power_on(struct imx585 *imx585)
{
	int ret;
	struct device *dev = &imx585->client->dev;
	if (!IS_ERR_OR_NULL(imx585->pins_default)) {
		ret = pinctrl_select_state(imx585->pinctrl,
					   imx585->pins_default);
		if (ret < 0)
			dev_err(dev, "could not set pins\n");
	}

	if (!imx585->is_thunderboot) {
		ret = regulator_bulk_enable(IMX585_NUM_SUPPLIES, imx585->supplies);
		if (ret < 0) {
			dev_err(dev, "Failed to enable regulators\n");
			goto err_pinctrl;
		}
		if (!IS_ERR(imx585->power_gpio))
			gpiod_direction_output(imx585->power_gpio, 0);
		/* At least 500ns between power raising and XCLR */
		/* fix power on timing if insmod this ko */
		usleep_range(10 * 1000, 20 * 1000);
		if (!IS_ERR(imx585->reset_gpio))
			gpiod_direction_output(imx585->reset_gpio, 0);

		/* At least 1us between XCLR and clk */
		/* fix power on timing if insmod this ko */
		usleep_range(10 * 1000, 20 * 1000);
	}
	ret = clk_set_rate(imx585->xvclk, imx585->cur_mode->xvclk);
	if (ret < 0)
		dev_warn(dev, "Failed to set xvclk rate\n");
	if (clk_get_rate(imx585->xvclk) != imx585->cur_mode->xvclk)
		dev_warn(dev, "xvclk mismatched\n");

	ret = clk_prepare_enable(imx585->xvclk);
	dev_info(dev, "clk_prepare_enable:%d\n",ret);
	if (ret < 0) {
		dev_err(dev, "Failed to enable xvclk\n");
		goto err_clk;
	}

	/* At least 20us between XCLR and I2C communication */
	if (!imx585->is_thunderboot)
		usleep_range(20*1000, 30*1000);

	return 0;

err_clk:
	if (!IS_ERR(imx585->reset_gpio))
		gpiod_direction_output(imx585->reset_gpio, 1);
	regulator_bulk_disable(IMX585_NUM_SUPPLIES, imx585->supplies);

err_pinctrl:
	if (!IS_ERR_OR_NULL(imx585->pins_sleep))
		pinctrl_select_state(imx585->pinctrl, imx585->pins_sleep);

	return ret;
}

static void __imx585_power_off(struct imx585 *imx585)
{
	int ret;
	struct device *dev = &imx585->client->dev;

	if (imx585->is_thunderboot) {
		if (imx585->is_first_streamoff) {
			imx585->is_thunderboot = false;
			imx585->is_first_streamoff = false;
		} else {
			return;
		}
	}

	if (!IS_ERR(imx585->reset_gpio))
		gpiod_direction_output(imx585->reset_gpio, 1);
	clk_disable_unprepare(imx585->xvclk);
	if (!IS_ERR_OR_NULL(imx585->pins_sleep)) {
		ret = pinctrl_select_state(imx585->pinctrl,
					   imx585->pins_sleep);
		if (ret < 0)
			dev_dbg(dev, "could not set pins\n");
	}
	if (!IS_ERR(imx585->power_gpio))
		gpiod_direction_output(imx585->power_gpio, 1);
	regulator_bulk_disable(IMX585_NUM_SUPPLIES, imx585->supplies);
}

static int __maybe_unused imx585_runtime_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct imx585 *imx585 = to_imx585(sd);

	return __imx585_power_on(imx585);
}

static int __maybe_unused imx585_runtime_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct imx585 *imx585 = to_imx585(sd);

	__imx585_power_off(imx585);

	return 0;
}

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static int imx585_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct imx585 *imx585 = to_imx585(sd);
	struct v4l2_mbus_framefmt *try_fmt =
				v4l2_subdev_get_try_format(sd, fh->pad, 0);
	const struct imx585_mode *def_mode = &imx585->supported_modes[0];

	mutex_lock(&imx585->mutex);
	/* Initialize try_fmt */
	try_fmt->width = def_mode->width;
	try_fmt->height = def_mode->height;
	try_fmt->code = def_mode->bus_fmt;
	try_fmt->field = V4L2_FIELD_NONE;

	mutex_unlock(&imx585->mutex);
	/* No crop or compose */

	return 0;
}
#endif

static int imx585_enum_frame_interval(struct v4l2_subdev *sd,
	struct v4l2_subdev_pad_config *cfg,
	struct v4l2_subdev_frame_interval_enum *fie)
{
	struct imx585 *imx585 = to_imx585(sd);

	if (fie->index >= imx585->cfg_num)
		return -EINVAL;

	fie->code = imx585->supported_modes[fie->index].bus_fmt;
	fie->width = imx585->supported_modes[fie->index].width;
	fie->height = imx585->supported_modes[fie->index].height;
	fie->interval = imx585->supported_modes[fie->index].max_fps;
	fie->reserved[0] = imx585->supported_modes[fie->index].hdr_mode;
	return 0;
}

#define CROP_START(SRC, DST) (((SRC) - (DST)) / 2 / 4 * 4)
#define DST_WIDTH_3840 3840
#define DST_HEIGHT_2160 2160
#define DST_WIDTH_1920 1920
#define DST_HEIGHT_1080 1080

/*
 * The resolution of the driver configuration needs to be exactly
 * the same as the current output resolution of the sensor,
 * the input width of the isp needs to be 16 aligned,
 * the input height of the isp needs to be 8 aligned.
 * Can be cropped to standard resolution by this function,
 * otherwise it will crop out strange resolution according
 * to the alignment rules.
 */
static int imx585_get_selection(struct v4l2_subdev *sd,
				struct v4l2_subdev_pad_config *cfg,
				struct v4l2_subdev_selection *sel)
{
	struct imx585 *imx585 = to_imx585(sd);

	if (sel->target == V4L2_SEL_TGT_CROP_BOUNDS) {
		if (imx585->cur_mode->width == 3864) {
			sel->r.left = CROP_START(imx585->cur_mode->width, DST_WIDTH_3840);
			sel->r.width = DST_WIDTH_3840;
			sel->r.top = CROP_START(imx585->cur_mode->height, DST_HEIGHT_2160);
			sel->r.height = DST_HEIGHT_2160;
		} else if (imx585->cur_mode->width == 1944) {
			sel->r.left = CROP_START(imx585->cur_mode->width, DST_WIDTH_1920);
			sel->r.width = DST_WIDTH_1920;
			sel->r.top = CROP_START(imx585->cur_mode->height, DST_HEIGHT_1080);
			sel->r.height = DST_HEIGHT_1080;
		} else {
			sel->r.left = CROP_START(imx585->cur_mode->width, imx585->cur_mode->width);
			sel->r.width = imx585->cur_mode->width;
			sel->r.top = CROP_START(imx585->cur_mode->height, imx585->cur_mode->height);
			sel->r.height = imx585->cur_mode->height;
		}
		return 0;
	}
	return -EINVAL;
}

static const struct dev_pm_ops imx585_pm_ops = {
	SET_RUNTIME_PM_OPS(imx585_runtime_suspend,
			   imx585_runtime_resume, NULL)
};

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static const struct v4l2_subdev_internal_ops imx585_internal_ops = {
	.open = imx585_open,
};
#endif

static const struct v4l2_subdev_core_ops imx585_core_ops = {
	.s_power = imx585_s_power,
	.ioctl = imx585_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = imx585_compat_ioctl32,
#endif
};

static const struct v4l2_subdev_video_ops imx585_video_ops = {
	.s_stream = imx585_s_stream,
	.g_frame_interval = imx585_g_frame_interval,
};

static const struct v4l2_subdev_pad_ops imx585_pad_ops = {
	.enum_mbus_code = imx585_enum_mbus_code,
	.enum_frame_size = imx585_enum_frame_sizes,
	.enum_frame_interval = imx585_enum_frame_interval,
	.get_fmt = imx585_get_fmt,
	.set_fmt = imx585_set_fmt,
	.get_selection = imx585_get_selection,
	.get_mbus_config = imx585_g_mbus_config,
};

static const struct v4l2_subdev_ops imx585_subdev_ops = {
	.core	= &imx585_core_ops,
	.video	= &imx585_video_ops,
	.pad	= &imx585_pad_ops,
};

static int imx585_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct imx585 *imx585 = container_of(ctrl->handler,
					     struct imx585, ctrl_handler);
	struct i2c_client *client = imx585->client;
	s64 max;
	u32 vts = 0, val;
	int ret = 0;
	u32 shr0 = 0;

	/* Propagate change of current control to all related controls */
	switch (ctrl->id) {
	case V4L2_CID_VBLANK:
		if (imx585->cur_mode->hdr_mode == NO_HDR) {
			/* Update max exposure while meeting expected vblanking */
			max = imx585->cur_mode->height + ctrl->val - 8;
			__v4l2_ctrl_modify_range(imx585->exposure,
					 imx585->exposure->minimum, max,
					 imx585->exposure->step,
					 imx585->exposure->default_value);
		}
		break;
	}

	if (!pm_runtime_get_if_in_use(&client->dev))
		return 0;

	switch (ctrl->id) {
	case V4L2_CID_EXPOSURE:
		if (imx585->cur_mode->hdr_mode != NO_HDR)
			goto ctrl_end;
		shr0 = imx585->cur_vts - ctrl->val;
		ret = imx585_write_reg(imx585->client, IMX585_LF_EXPO_REG_L,
				       IMX585_REG_VALUE_08BIT,
				       IMX585_FETCH_EXP_L(shr0));
		ret |= imx585_write_reg(imx585->client, IMX585_LF_EXPO_REG_M,
				       IMX585_REG_VALUE_08BIT,
				       IMX585_FETCH_EXP_M(shr0));
		ret |= imx585_write_reg(imx585->client, IMX585_LF_EXPO_REG_H,
				       IMX585_REG_VALUE_08BIT,
				       IMX585_FETCH_EXP_H(shr0));
		dev_dbg(&client->dev, "set exposure(shr0) %d = cur_vts(%d) - val(%d)\n",
			shr0, imx585->cur_vts, ctrl->val);
		break;
	case V4L2_CID_ANALOGUE_GAIN:
		if (imx585->cur_mode->hdr_mode != NO_HDR)
			goto ctrl_end;
		ret = imx585_write_reg(imx585->client, IMX585_LF_GAIN_REG_H,
				       IMX585_REG_VALUE_08BIT,
				       IMX585_FETCH_GAIN_H(ctrl->val));
		ret |= imx585_write_reg(imx585->client, IMX585_LF_GAIN_REG_L,
				       IMX585_REG_VALUE_08BIT,
				       IMX585_FETCH_GAIN_L(ctrl->val));
		dev_dbg(&client->dev, "set analog gain 0x%x\n",
			ctrl->val);
		break;
	case V4L2_CID_VBLANK:
		vts = ctrl->val + imx585->cur_mode->height;
		/*
		 * vts of hdr mode is double to correct T-line calculation.
		 * Restore before write to reg.
		 */
		if (imx585->cur_mode->hdr_mode == HDR_X2) {
			vts = (vts + 3) / 4 * 4;
			imx585->cur_vts = vts;
			vts /= 2;
		} else if (imx585->cur_mode->hdr_mode == HDR_X3) {
			vts = (vts + 11) / 12 * 12;
			imx585->cur_vts = vts;
			vts /= 4;
		} else {
			imx585->cur_vts = vts;
		}
		ret = imx585_write_reg(imx585->client, IMX585_VTS_REG_L,
				       IMX585_REG_VALUE_08BIT,
				       IMX585_FETCH_VTS_L(vts));
		ret |= imx585_write_reg(imx585->client, IMX585_VTS_REG_M,
				       IMX585_REG_VALUE_08BIT,
				       IMX585_FETCH_VTS_M(vts));
		ret |= imx585_write_reg(imx585->client, IMX585_VTS_REG_H,
				       IMX585_REG_VALUE_08BIT,
				       IMX585_FETCH_VTS_H(vts));
		dev_dbg(&client->dev, "set vblank 0x%x vts %d\n",
			ctrl->val, vts);
		break;
	case V4L2_CID_HFLIP:
		ret = imx585_read_reg(imx585->client, IMX585_FLIP_REG,
				      IMX585_REG_VALUE_08BIT, &val);
		if (ret)
			break;
		if (ctrl->val)
			val |= IMX585_MIRROR_BIT_MASK;
		else
			val &= ~IMX585_MIRROR_BIT_MASK;
		ret = imx585_write_reg(imx585->client, IMX585_FLIP_REG,
				       IMX585_REG_VALUE_08BIT, val);
		break;
	case V4L2_CID_VFLIP:
		ret = imx585_read_reg(imx585->client, IMX585_FLIP_REG,
				      IMX585_REG_VALUE_08BIT, &val);
		if (ret)
			break;
		if (ctrl->val)
			val |= IMX585_FLIP_BIT_MASK;
		else
			val &= ~IMX585_FLIP_BIT_MASK;
		ret = imx585_write_reg(imx585->client, IMX585_FLIP_REG,
				       IMX585_REG_VALUE_08BIT, val);
		break;
	default:
		dev_warn(&client->dev, "%s Unhandled id:0x%x, val:0x%x\n",
			 __func__, ctrl->id, ctrl->val);
		break;
	}

ctrl_end:
	pm_runtime_put(&client->dev);

	return ret;
}

static int imx585_v4l2_ctrl_g_ctrl_standard( struct v4l2_ctrl *ctrl )
{
    int ret = 0;

    switch ( ctrl->id ) {
        case V4L2_CID_MIN_BUFFERS_FOR_OUTPUT:
            ctrl->val = 32;
        break;
        case V4L2_CID_MIN_BUFFERS_FOR_CAPTURE:
            ctrl->val = 32;
        break;

        default:
            ret = -EINVAL;
    }
    return ret;
}

static const struct v4l2_ctrl_ops imx585_ctrl_ops = {
	.s_ctrl = imx585_set_ctrl,
	.g_volatile_ctrl = imx585_v4l2_ctrl_g_ctrl_standard,
};

static int imx585_initialize_controls(struct imx585 *imx585)
{
	const struct imx585_mode *mode;
	struct v4l2_ctrl_handler *handler;
	s64 exposure_max, vblank_def;
	u64 pixel_rate;
	u64 max_pixel_rate;
	u32 h_blank;
	int ret;
	u8 lanes = imx585->bus_cfg.bus.mipi_csi2.num_data_lanes;

	handler = &imx585->ctrl_handler;
	mode = imx585->cur_mode;
	ret = v4l2_ctrl_handler_init(handler, 8);
	if (ret)
		return ret;
	handler->lock = &imx585->mutex;

	imx585->link_freq = v4l2_ctrl_new_int_menu(handler, NULL,
				V4L2_CID_LINK_FREQ,
				ARRAY_SIZE(link_freq_items) - 1, 0,
				link_freq_items);
	v4l2_ctrl_s_ctrl(imx585->link_freq, mode->mipi_freq_idx);

	/* pixel rate = link frequency * 2 * lanes / BITS_PER_SAMPLE */
	pixel_rate = (u32)link_freq_items[mode->mipi_freq_idx] / mode->bpp * 2 * lanes;
	max_pixel_rate = MIPI_FREQ_1188M / mode->bpp * 2 * lanes;
	imx585->pixel_rate = v4l2_ctrl_new_std(handler, NULL,
		V4L2_CID_PIXEL_RATE, 0, max_pixel_rate,
		1, pixel_rate);

	h_blank = mode->hts_def - mode->width;
	imx585->hblank = v4l2_ctrl_new_std(handler, NULL, V4L2_CID_HBLANK,
				h_blank, h_blank, 1, h_blank);
	if (imx585->hblank)
		imx585->hblank->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	vblank_def = mode->vts_def - mode->height;
	imx585->vblank = v4l2_ctrl_new_std(handler, &imx585_ctrl_ops,
				V4L2_CID_VBLANK, vblank_def,
				IMX585_VTS_MAX - mode->height,
				1, vblank_def);
	imx585->cur_vts = mode->vts_def;

	exposure_max = mode->vts_def - 6;
	imx585->exposure = v4l2_ctrl_new_std(handler, &imx585_ctrl_ops,
				V4L2_CID_EXPOSURE, IMX585_EXPOSURE_MIN,
				exposure_max, IMX585_EXPOSURE_STEP,
				mode->exp_def);

	imx585->anal_a_gain = v4l2_ctrl_new_std(handler, &imx585_ctrl_ops,
				V4L2_CID_ANALOGUE_GAIN, IMX585_GAIN_MIN,
				IMX585_GAIN_MAX, IMX585_GAIN_STEP,
				IMX585_GAIN_DEFAULT);

	v4l2_ctrl_new_std(handler, &imx585_ctrl_ops, V4L2_CID_HFLIP, 0, 1, 1, 0);
	v4l2_ctrl_new_std(handler, &imx585_ctrl_ops, V4L2_CID_VFLIP, 0, 1, 1, 0);


	v4l2_ctrl_new_std(handler, &imx585_ctrl_ops, V4L2_CID_MIN_BUFFERS_FOR_OUTPUT, 2, 32, 1, 32);
	v4l2_ctrl_new_std(handler, &imx585_ctrl_ops, V4L2_CID_MIN_BUFFERS_FOR_CAPTURE, 2, 32, 1, 32);

	if (handler->error) {
		ret = handler->error;
		dev_err(&imx585->client->dev,
			"Failed to init controls(%d)\n", ret);
		goto err_free_handler;
	}

	imx585->subdev.ctrl_handler = handler;
	imx585->has_init_exp = false;

	return 0;

err_free_handler:
	v4l2_ctrl_handler_free(handler);

	return ret;
}

static int imx585_check_sensor_id(struct imx585 *imx585,
				  struct i2c_client *client)
{
	struct device *dev = &imx585->client->dev;
	u32 id = 0;
	int ret;

	if (imx585->is_thunderboot) {
		dev_info(dev, "Enable thunderboot mode, skip sensor id check\n");
		return 0;
	}

	ret = imx585_read_reg(client, IMX585_REG_CHIP_ID,
			      IMX585_REG_VALUE_08BIT, &id);
	if (id != CHIP_ID) {
		dev_err(dev, "Unexpected sensor id(%06x), ret(%d)\n", id, ret);
		return -ENODEV;
	}

	dev_info(dev, "Detected imx585 id %06x\n", CHIP_ID);

	return 0;
}

static int imx585_configure_regulators(struct imx585 *imx585)
{
	unsigned int i;

	for (i = 0; i < IMX585_NUM_SUPPLIES; i++)
		imx585->supplies[i].supply = imx585_supply_names[i];

	return devm_regulator_bulk_get(&imx585->client->dev,
				       IMX585_NUM_SUPPLIES,
				       imx585->supplies);
}

static int imx585_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct device_node *node = dev->of_node;
	struct imx585 *imx585;
	struct v4l2_subdev *sd;
	struct device_node *endpoint;
	char facing[2];
	int ret;
	u32 i, hdr_mode = 0;

	dev_info(dev, "driver version: %02x.%02x.%02x",
		DRIVER_VERSION >> 16,
		(DRIVER_VERSION & 0xff00) >> 8,
		DRIVER_VERSION & 0x00ff);

	imx585 = devm_kzalloc(dev, sizeof(*imx585), GFP_KERNEL);
	if (!imx585)
		return -ENOMEM;

	ret = of_property_read_u32(node, RKMODULE_CAMERA_MODULE_INDEX,
				   &imx585->module_index);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_FACING,
				       &imx585->module_facing);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_NAME,
				       &imx585->module_name);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_LENS_NAME,
				       &imx585->len_name);
	if (ret) {
		dev_err(dev, "could not get module information!\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(node, OF_CAMERA_HDR_MODE, &hdr_mode);
	if (ret) {
		hdr_mode = NO_HDR;
		dev_warn(dev, " Get hdr mode failed! no hdr default\n");
	}

	endpoint = of_graph_get_next_endpoint(dev->of_node, NULL);
	if (!endpoint) {
		dev_err(dev, "Failed to get endpoint\n");
		return -EINVAL;
	}

	ret = v4l2_fwnode_endpoint_parse(of_fwnode_handle(endpoint),
		&imx585->bus_cfg);
	of_node_put(endpoint);
	if (ret) {
		dev_err(dev, "Failed to get bus config\n");
		return -EINVAL;
	}

	imx585->client = client;
	if (imx585->bus_cfg.bus.mipi_csi2.num_data_lanes == IMX585_4LANES) {
		imx585->supported_modes = supported_modes;
		imx585->cfg_num = ARRAY_SIZE(supported_modes);
	} else {
		imx585->supported_modes = supported_modes_2lane;
		imx585->cfg_num = ARRAY_SIZE(supported_modes_2lane);
	}
	dev_info(dev, "detect imx585 lane %d\n",
		imx585->bus_cfg.bus.mipi_csi2.num_data_lanes);

	for (i = 0; i < imx585->cfg_num; i++) {
		if (hdr_mode == imx585->supported_modes[i].hdr_mode) {
			imx585->cur_mode = &imx585->supported_modes[i];
			break;
		}
	}

	of_property_read_u32(node, RKMODULE_CAMERA_FASTBOOT_ENABLE,
		&imx585->is_thunderboot);

	imx585->xvclk = devm_clk_get(dev, "xvclk");
	if (IS_ERR(imx585->xvclk)) {
		dev_err(dev, "Failed to get xvclk\n");
		return -EINVAL;
	}

	imx585->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_ASIS);
	if (IS_ERR(imx585->reset_gpio))
		dev_warn(dev, "Failed to get reset-gpios\n");
	imx585->power_gpio = devm_gpiod_get(dev, "pwdn", GPIOD_ASIS);
	if (IS_ERR(imx585->power_gpio))
		dev_warn(dev, "Failed to get power-gpios\n");
	imx585->pinctrl = devm_pinctrl_get(dev);
	if (!IS_ERR(imx585->pinctrl)) {
		imx585->pins_default =
			pinctrl_lookup_state(imx585->pinctrl,
					     OF_CAMERA_PINCTRL_STATE_DEFAULT);
		if (IS_ERR(imx585->pins_default))
			dev_info(dev, "could not get default pinstate\n");

		imx585->pins_sleep =
			pinctrl_lookup_state(imx585->pinctrl,
					     OF_CAMERA_PINCTRL_STATE_SLEEP);
		if (IS_ERR(imx585->pins_sleep))
			dev_info(dev, "could not get sleep pinstate\n");
	} else {
		dev_info(dev, "no pinctrl\n");
	}

	ret = imx585_configure_regulators(imx585);
	if (ret) {
		dev_err(dev, "Failed to get power regulators\n");
		return ret;
	}

	mutex_init(&imx585->mutex);

	sd = &imx585->subdev;
	v4l2_i2c_subdev_init(sd, client, &imx585_subdev_ops);
	ret = imx585_initialize_controls(imx585);
	if (ret)
		goto err_destroy_mutex;

	ret = __imx585_power_on(imx585);
	if (ret)
		goto err_free_handler;

	ret = imx585_check_sensor_id(imx585, client);
	if (ret)
		goto err_power_off;

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
	sd->internal_ops = &imx585_internal_ops;
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE |
		     V4L2_SUBDEV_FL_HAS_EVENTS;
#endif
#if defined(CONFIG_MEDIA_CONTROLLER)
	imx585->pad.flags = MEDIA_PAD_FL_SOURCE;
	sd->entity.function = MEDIA_ENT_F_CAM_SENSOR;
	ret = media_entity_pads_init(&sd->entity, 1, &imx585->pad);
	if (ret < 0)
		goto err_power_off;
#endif

	memset(facing, 0, sizeof(facing));
	if (strcmp(imx585->module_facing, "back") == 0)
		facing[0] = 'b';
	else
		facing[0] = 'f';

	snprintf(sd->name, sizeof(sd->name), "m%02d_%s_%s %s",
		 imx585->module_index, facing,
		 IMX585_NAME, dev_name(sd->dev));
	ret = v4l2_async_register_subdev_sensor_common(sd);
	if (ret) {
		dev_err(dev, "v4l2 async register subdev failed\n");
		goto err_clean_entity;
	}

	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);
	pm_runtime_idle(dev);

	return 0;

err_clean_entity:
#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&sd->entity);
#endif
err_power_off:
	__imx585_power_off(imx585);
err_free_handler:
	v4l2_ctrl_handler_free(&imx585->ctrl_handler);
err_destroy_mutex:
	mutex_destroy(&imx585->mutex);

	return ret;
}

static int imx585_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct imx585 *imx585 = to_imx585(sd);

	v4l2_async_unregister_subdev(sd);
#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&sd->entity);
#endif
	v4l2_ctrl_handler_free(&imx585->ctrl_handler);
	mutex_destroy(&imx585->mutex);

	pm_runtime_disable(&client->dev);
	if (!pm_runtime_status_suspended(&client->dev))
		__imx585_power_off(imx585);
	pm_runtime_set_suspended(&client->dev);

	return 0;
}

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id imx585_of_match[] = {
	{ .compatible = "sony,imx585" },
	{},
};
MODULE_DEVICE_TABLE(of, imx585_of_match);
#endif

static const struct i2c_device_id imx585_match_id[] = {
	{ "sony,imx585", 0 },
	{ },
};

static struct i2c_driver imx585_i2c_driver = {
	.driver = {
		.name = IMX585_NAME,
		.pm = &imx585_pm_ops,
		.of_match_table = of_match_ptr(imx585_of_match),
	},
	.probe		= &imx585_probe,
	.remove		= &imx585_remove,
	.id_table	= imx585_match_id,
};

static int __init sensor_mod_init(void)
{
	return i2c_add_driver(&imx585_i2c_driver);
}

static void __exit sensor_mod_exit(void)
{
	i2c_del_driver(&imx585_i2c_driver);
}

device_initcall_sync(sensor_mod_init);
module_exit(sensor_mod_exit);

MODULE_DESCRIPTION("Sony imx585 sensor driver");
MODULE_LICENSE("GPL v2");
