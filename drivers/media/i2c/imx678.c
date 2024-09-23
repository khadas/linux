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
#include <linux/pinctrl/consumer.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_graph.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/mfd/syscon.h>
#include <linux/rk-preisp.h>

/************************************config******************************************start*/
#define IMX678_NORMAL_3840_2160_4LANE_25FPS_30FPS_30CP		1     //0:25fps    1:30fps   2:30fps crop
#define IMX678_NORMAL_2688_1512_4LANE_30FPS_60FPS		1     //0:30fps    1:60fps
#define IMX678_DOL_HDR_3840_2160_4LANE_15FPS_16_7FPS    	0     //15fps      1:16.7fps
#define USED_SYS_DEBUG
/**************config************error eport start**/
#if (!((IMX678_NORMAL_3840_2160_4LANE_25FPS_30FPS_30CP >= 0) && (IMX678_NORMAL_3840_2160_4LANE_25FPS_30FPS_30CP <= 2)    \
	&& (IMX678_NORMAL_2688_1512_4LANE_30FPS_60FPS >= 0) && (IMX678_NORMAL_2688_1512_4LANE_30FPS_60FPS <= 1)              \
	&& (IMX678_DOL_HDR_3840_2160_4LANE_15FPS_16_7FPS >= 0) && (IMX678_DOL_HDR_3840_2160_4LANE_15FPS_16_7FPS <= 1))) 
#---------------config error---------------#
#endif
/**************config************error eport end**/
/************************************config********************************************end*/

#define DRIVER_VERSION					KERNEL_VERSION(0, 0x01, 0x01)

#ifndef V4L2_CID_DIGITAL_GAIN
#define V4L2_CID_DIGITAL_GAIN				V4L2_CID_GAIN
#endif

#define IMX678_XVCLK_FREQ		 		24000000
#define MIPI_FREQ_720M					720000000 //1440000000
#define IMX678_MAX_FREQ					MIPI_FREQ_720M

/* TODO: Get the real chip id from reg */
#define CHIP_ID						0x0884
#define IMX678_REG_CHIP_ID				0x3046

#define IMX678_REG_CTRL_MODE			0x3000
#define IMX678_MODE_SW_STANDBY			0x01
#define IMX678_MODE_STREAMING			0x00

#define IMX678_GROUP_HOLD_REG			0x3001
#define IMX678_GROUP_HOLD_START			0x01
#define IMX678_GROUP_HOLD_END			0x00

#define IMX678_GAIN_SWITCH_REG			0x3030
#define IMX678_LF_GAIN_REG_H			0x3071
#define IMX678_LF_GAIN_REG_L			0x3070

#define IMX678_SF1_GAIN_REG_H			0x3073
#define IMX678_SF1_GAIN_REG_L			0x3072

#define IMX678_SF2_GAIN_REG_H			0x3075
#define IMX678_SF2_GAIN_REG_L			0x3074

#define IMX678_LF_EXPO_REG_H			0x3052
#define IMX678_LF_EXPO_REG_M			0x3051
#define IMX678_LF_EXPO_REG_L			0x3050

#define IMX678_SF1_EXPO_REG_H			0x3056
#define IMX678_SF1_EXPO_REG_M			0x3055
#define IMX678_SF1_EXPO_REG_L			0x3054

#define IMX678_SF2_EXPO_REG_H			0x305A
#define IMX678_SF2_EXPO_REG_M			0x3059
#define IMX678_SF2_EXPO_REG_L			0x3058

#define IMX678_RHS1_REG_H			0x3062
#define IMX678_RHS1_REG_M			0x3061
#define IMX678_RHS1_REG_L			0x3060
#define IMX678_RHS1_DEFAULT			0x01D7
#define IMX678_RHS1_X3_DEFAULT			0x0079

#define IMX678_RHS2_REG_H			0x3066
#define IMX678_RHS2_REG_M			0x3065
#define IMX678_RHS2_REG_L			0x3064
#define IMX678_RHS2_X3_DEFAULT			0x0086

#define IMX678_MAXFAC_NUM			1
#define IMX678_BLANK_NUM			70

#define	IMX678_EXPOSURE_MIN			3
#define	IMX678_EXPOSURE_STEP			1
#define IMX678_VTS_MAX				0x7fff

#define IMX678_GAIN_MIN				0x00
#define IMX678_GAIN_MAX				0xf0
#define IMX678_GAIN_STEP			1
#define IMX678_GAIN_DEFAULT			0x00

#define IMX678_FETCH_GAIN_H(VAL)		(((VAL) >> 8) & 0x07)
#define IMX678_FETCH_GAIN_L(VAL)		((VAL) & 0xFF)

#define IMX678_FETCH_EXP_H(VAL)			(((VAL) >> 16) & 0x0F)
#define IMX678_FETCH_EXP_M(VAL)			(((VAL) >> 8) & 0xFF)
#define IMX678_FETCH_EXP_L(VAL)			((VAL) & 0xFF)

#define IMX678_FETCH_RHS1_H(VAL)		(((VAL) >> 16) & 0x0F)
#define IMX678_FETCH_RHS1_M(VAL)		(((VAL) >> 8) & 0xFF)
#define IMX678_FETCH_RHS1_L(VAL)		((VAL) & 0xFF)

#define IMX678_FETCH_VTS_H(VAL)			(((VAL) >> 16) & 0x0F)
#define IMX678_FETCH_VTS_M(VAL)			(((VAL) >> 8) & 0xFF)
#define IMX678_FETCH_VTS_L(VAL)			((VAL) & 0xFF)

#define IMX678_VTS_REG_L			0x3028
#define IMX678_VTS_REG_M			0x3029
#define IMX678_VTS_REG_H			0x302A

#define IMX678_MIRROR_BIT_MASK			BIT(0)
#define IMX678_FLIP_BIT_MASK			BIT(0)
#define IMX678_FLIP_REG				0x3020

#define REG_NULL				0xFFFF

#define IMX678_REG_VALUE_08BIT			1
#define IMX678_REG_VALUE_16BIT			2
#define IMX678_REG_VALUE_24BIT			3

/* Basic Readout Lines. Number of necessary readout lines in sensor */
#define BRL					2200u
#define BRL_BINNING				1115u
/* Readout timing setting of SEF1(DOL2): RHS1 < 2 * BRL and should be 2n + 1 */
#define RHS1_MAX_X2				((BRL * 2 - 1) / 2 * 2 + 1)
#define SHR1_MIN_X2				5u

/* Readout timing setting of SEF1(DOL3): RHS1 < 3 * BRL and should be 3n + 2 */
#define RHS1_MAX_X3				((BRL * 3 - 1) / 3 * 3 + 2)
#define SHR1_MIN_X3				7u

static bool g_isHCG;

#define OF_CAMERA_PINCTRL_STATE_DEFAULT	"rockchip,camera_default"
#define OF_CAMERA_PINCTRL_STATE_SLEEP	"rockchip,camera_sleep"
#define OF_CAMERA_HDR_MODE	"rockchip,camera-hdr-mode"

#define IMX678_NAME				"imx678"
#define NO_CLEAR_HDR				0
#define CLEAR_HDR				1


static const char * const imx678_supply_names[] = {
	"dvdd",		/* Digital core power */
	"dovdd",	/* Digital I/O power */
	"avdd",		/* Analog power */
};

#define IMX678_NUM_SUPPLIES ARRAY_SIZE(imx678_supply_names)

struct regval {
	u16 addr;
	u8 val;
};

struct imx678_mode {
	u32 bus_fmt;
	u32 width;
	u32 height;
	u32 real_width;
	u32 real_height;	
	struct v4l2_fract max_fps;
	u32 hts_def;
	u32 vts_def;
	u32 exp_def;
	u32 mipi_freq_idx;
	u32 link_freq_def;
	u32 bpp;
	u32 clear_hdr_mode;
	const struct regval *global_reg_list;
	const struct regval *reg_list;
	u32 hdr_mode;
	u32 vc[PAD_MAX];
};

struct imx678 {
	struct i2c_client	*client;
	struct clk		*xvclk;
	struct gpio_desc	*reset_gpio;
	struct gpio_desc	*pwdn_gpio;
	struct regulator_bulk_data supplies[IMX678_NUM_SUPPLIES];

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
	const struct imx678_mode *cur_mode;
	u32			module_index;
	u32			lane_num;
	u32			cfg_num;
	const char		*module_facing;
	const char		*module_name;
	const char		*len_name;
	u32			cur_vts;
	bool			has_init_exp;
	struct preisp_hdrae_exp_s init_hdrae_exp;
};

#define to_imx678(sd) container_of(sd, struct imx678, subdev)




/************************************************************************分割线_4lane**************************************************************************/
static const struct regval imx678_global_4lane_setting_regs[] = {
	{0x3000, 0x01},  //STANDBY
	{0x3001, 0x01},  //REGHOLD
	{0x3002, 0x00},  //XMSTA

	{0x3014, 0x04},    //INCK_SEL[3:0]		
	{0x3015, 0x03},    //DATARATE_SEL[3:0]		

	{0x3050, 0x03},  //SHR0[19:0]
	{0x3051, 0x00},  //SHR0[19:0]
	{0x3052, 0x00},  //SHR0[19:0]
	{0x3054, 0x0E},  //SHR1[19:0]
	{0x3055, 0x00},  //SHR1[19:0]
	{0x3056, 0x00},  //SHR1[19:0]
	{0x3058, 0x8A},  //SHR2[19:0]
	{0x3059, 0x01},  //SHR2[19:0]
	{0x305A, 0x00},  //SHR2[19:0]
	{0x3060, 0x16},  //RHS1[19:0]
	{0x3061, 0x01},  //RHS1[19:0]
	{0x3062, 0x00},  //RHS1[19:0]
	{0x3064, 0xC4},  //RHS2[19:0]
	{0x3065, 0x0C},  //RHS2[19:0]
	{0x3066, 0x00},  //RHS2[19:0]
	{0x3069, 0x00},  //CHDR_GAIN_EN
	{0x306B, 0x00},  //Sensor_register
	{0x3070, 0x00},  //GAIN[10:0]
	{0x3071, 0x00},  //GAIN[10:0]
	{0x3072, 0x00},  //GAIN_1[10:0]
	{0x3073, 0x00},  //GAIN_1[10:0]
	{0x3074, 0x00},  //GAIN_2[10:0]
	{0x3075, 0x00},  //GAIN_2[10:0]
	{0x3081, 0x00},  //EXP_GAIN
	{0x308C, 0x00},  //CHDR_DGAIN0_HG[15:0]
	{0x308D, 0x01},  //CHDR_DGAIN0_HG[15:0]
	{0x3094, 0x00},  //CHDR_AGAIN0_LG[10:0]
	{0x3095, 0x00},  //CHDR_AGAIN0_LG[10:0]
	{0x309C, 0x00},  //CHDR_AGAIN0_HG[10:0]
	{0x309D, 0x00},  //CHDR_AGAIN0_HG[10:0]
	{0x30A4, 0xAA},  //XVSOUTSEL[1:0]
	{0x30A6, 0x00},  //XVS_DRV[1:0]
	{0x30CC, 0x00},  //Sensor_register
	{0x30CD, 0x00},  //Sensor_register
	{0x30DC, 0x32},  //BLKLEVEL[11:0]
	{0x30DD, 0x40},  //BLKLEVEL[11:0]
	{0x3400, 0x01},  //GAIN_PGC_FIDMD
	{0x3460, 0x22},  //Sensor_register
	{0x355A, 0x64},  //Sensor_register
	{0x3A02, 0x7A},  //Sensor_register
	{0x3A10, 0xEC},  //Sensor_register
	{0x3A12, 0x71},  //Sensor_register
	{0x3A14, 0xDE},  //Sensor_register
	{0x3A20, 0x2B},  //Sensor_register
	{0x3A24, 0x22},  //Sensor_register
	{0x3A25, 0x25},  //Sensor_register
	{0x3A26, 0x2A},  //Sensor_register
	{0x3A27, 0x2C},  //Sensor_register
	{0x3A28, 0x39},  //Sensor_register
	{0x3A29, 0x38},  //Sensor_register
	{0x3A30, 0x04},  //Sensor_register
	{0x3A31, 0x04},  //Sensor_register
	{0x3A32, 0x03},  //Sensor_register
	{0x3A33, 0x03},  //Sensor_register
	{0x3A34, 0x09},  //Sensor_register
	{0x3A35, 0x06},  //Sensor_register
	{0x3A38, 0xCD},  //Sensor_register
	{0x3A3A, 0x4C},  //Sensor_register
	{0x3A3C, 0xB9},  //Sensor_register
	{0x3A3E, 0x30},  //Sensor_register
	{0x3A40, 0x2C},  //Sensor_register
	{0x3A42, 0x39},  //Sensor_register
	{0x3A4E, 0x00},  //Sensor_register
	{0x3A52, 0x00},  //Sensor_register
	{0x3A56, 0x00},  //Sensor_register
	{0x3A5A, 0x00},  //Sensor_register
	{0x3A5E, 0x00},  //Sensor_register
	{0x3A62, 0x00},  //Sensor_register
	{0x3A64, 0x00},  //Sensor_register
	{0x3A6E, 0xA0},  //Sensor_register
	{0x3A70, 0x50},  //Sensor_register
	{0x3A8C, 0x04},  //Sensor_register
	{0x3A8D, 0x03},  //Sensor_register
	{0x3A8E, 0x09},  //Sensor_register
	{0x3A90, 0x38},  //Sensor_register
	{0x3A91, 0x42},  //Sensor_register
	{0x3A92, 0x3C},  //Sensor_register
	{0x3B0E, 0xF3},  //Sensor_register
	{0x3B12, 0xE5},  //Sensor_register
	{0x3B27, 0xC0},  //Sensor_register
	{0x3B2E, 0xEF},  //Sensor_register
	{0x3B30, 0x6A},  //Sensor_register
	{0x3B32, 0xF6},  //Sensor_register
	{0x3B36, 0xE1},  //Sensor_register
	{0x3B3A, 0xE8},  //Sensor_register
	{0x3B5A, 0x17},  //Sensor_register
	{0x3B5E, 0xEF},  //Sensor_register
	{0x3B60, 0x6A},  //Sensor_register
	{0x3B62, 0xF6},  //Sensor_register
	{0x3B66, 0xE1},  //Sensor_register
	{0x3B6A, 0xE8},  //Sensor_register
	{0x3B88, 0xEC},  //Sensor_register
	{0x3B8A, 0xED},  //Sensor_register
	{0x3B94, 0x71},  //Sensor_register
	{0x3B96, 0x72},  //Sensor_register
	{0x3B98, 0xDE},  //Sensor_register
	{0x3B9A, 0xDF},  //Sensor_register
	{0x3C0F, 0x06},  //Sensor_register
	{0x3C10, 0x06},  //Sensor_register
	{0x3C11, 0x06},  //Sensor_register
	{0x3C12, 0x06},  //Sensor_register
	{0x3C13, 0x06},  //Sensor_register
	{0x3C18, 0x20},  //Sensor_register
	{0x3C37, 0x10},  //Sensor_register
	{0x3C3A, 0x7A},  //Sensor_register
	{0x3C40, 0xF4},  //Sensor_register
	{0x3C48, 0xE6},  //Sensor_register
	{0x3C54, 0xCE},  //Sensor_register
	{0x3C56, 0xD0},  //Sensor_register
	{0x3C6C, 0x53},  //Sensor_register
	{0x3C6E, 0x55},  //Sensor_register
	{0x3C70, 0xC0},  //Sensor_register
	{0x3C72, 0xC2},  //Sensor_register
	{0x3C7E, 0xCE},  //Sensor_register
	{0x3C8C, 0xCF},  //Sensor_register
	{0x3C8E, 0xEB},  //Sensor_register
	{0x3C98, 0x54},  //Sensor_register
	{0x3C9A, 0x70},  //Sensor_register
	{0x3C9C, 0xC1},  //Sensor_register
	{0x3C9E, 0xDD},  //Sensor_register
	{0x3CB0, 0x7A},  //Sensor_register
	{0x3CB2, 0xBA},  //Sensor_register
	{0x3CC8, 0xBC},  //Sensor_register
	{0x3CCA, 0x7C},  //Sensor_register
	{0x3CD4, 0xEA},  //Sensor_register
	{0x3CD5, 0x01},  //Sensor_register
	{0x3CD6, 0x4A},  //Sensor_register
	{0x3CD8, 0x00},  //Sensor_register
	{0x3CD9, 0x00},  //Sensor_register
	{0x3CDA, 0xFF},  //Sensor_register
	{0x3CDB, 0x03},  //Sensor_register
	{0x3CDC, 0x00},  //Sensor_register
	{0x3CDD, 0x00},  //Sensor_register
	{0x3CDE, 0xFF},  //Sensor_register
	{0x3CDF, 0x03},  //Sensor_register
	{0x3CE4, 0x4C},  //Sensor_register
	{0x3CE6, 0xEC},  //Sensor_register
	{0x3CE7, 0x01},  //Sensor_register
	{0x3CE8, 0xFF},  //Sensor_register
	{0x3CE9, 0x03},  //Sensor_register
	{0x3CEA, 0x00},  //Sensor_register
	{0x3CEB, 0x00},  //Sensor_register
	{0x3CEC, 0xFF},  //Sensor_register
	{0x3CED, 0x03},  //Sensor_register
	{0x3CEE, 0x00},  //Sensor_register
	{0x3CEF, 0x00},  //Sensor_register
	{0x3CF2, 0xFF},  //Sensor_register
	{0x3CF3, 0x03},  //Sensor_register
	{0x3CF4, 0x00},  //Sensor_register
	{0x3E28, 0x82},  //Sensor_register
	{0x3E2A, 0x80},  //Sensor_register
	{0x3E30, 0x85},  //Sensor_register
	{0x3E32, 0x7D},  //Sensor_register
	{0x3E5C, 0xCE},  //Sensor_register
	{0x3E5E, 0xD3},  //Sensor_register
	{0x3E70, 0x53},  //Sensor_register
	{0x3E72, 0x58},  //Sensor_register
	{0x3E74, 0xC0},  //Sensor_register
	{0x3E76, 0xC5},  //Sensor_register
	{0x3E78, 0xC0},  //Sensor_register
	{0x3E79, 0x01},  //Sensor_register
	{0x3E7A, 0xD4},  //Sensor_register
	{0x3E7B, 0x01},  //Sensor_register
	{0x3EB4, 0x0B},  //Sensor_register
	{0x3EB5, 0x02},  //Sensor_register
	{0x3EB6, 0x4D},  //Sensor_register
	{0x3EB7, 0x42},  //Sensor_register
	{0x3EEC, 0xF3},  //Sensor_register
	{0x3EEE, 0xE7},  //Sensor_register
	{0x3F01, 0x01},  //Sensor_register
	{0x3F24, 0x10},  //Sensor_register
	{0x3F28, 0x2D},  //Sensor_register
	{0x3F2A, 0x2D},  //Sensor_register
	{0x3F2C, 0x2D},  //Sensor_register
	{0x3F2E, 0x2D},  //Sensor_register
	{0x3F30, 0x23},  //Sensor_register
	{0x3F38, 0x2D},  //Sensor_register
	{0x3F3A, 0x2D},  //Sensor_register
	{0x3F3C, 0x2D},  //Sensor_register
	{0x3F3E, 0x28},  //Sensor_register
	{0x3F40, 0x1E},  //Sensor_register
	{0x3F48, 0x2D},  //Sensor_register
	{0x3F4A, 0x2D},  //Sensor_register
	{0x3F4C, 0x00},  //Sensor_register
	{0x4004, 0xE4},  //Sensor_register
	{0x4006, 0xFF},  //Sensor_register
	{0x4018, 0x69},  //Sensor_register
	{0x401A, 0x84},  //Sensor_register
	{0x401C, 0xD6},  //Sensor_register
	{0x401E, 0xF1},  //Sensor_register
	{0x4038, 0xDE},  //Sensor_register
	{0x403A, 0x00},  //Sensor_register
	{0x403B, 0x01},  //Sensor_register
	{0x404C, 0x63},  //Sensor_register
	{0x404E, 0x85},  //Sensor_register
	{0x4050, 0xD0},  //Sensor_register
	{0x4052, 0xF2},  //Sensor_register
	{0x4108, 0xDD},  //Sensor_register
	{0x410A, 0xF7},  //Sensor_register
	{0x411C, 0x62},  //Sensor_register
	{0x411E, 0x7C},  //Sensor_register
	{0x4120, 0xCF},  //Sensor_register
	{0x4122, 0xE9},  //Sensor_register
	{0x4138, 0xE6},  //Sensor_register
	{0x413A, 0xF1},  //Sensor_register
	{0x414C, 0x6B},  //Sensor_register
	{0x414E, 0x76},  //Sensor_register
	{0x4150, 0xD8},  //Sensor_register
	{0x4152, 0xE3},  //Sensor_register
	{0x417E, 0x03},  //Sensor_register
	{0x417F, 0x01},  //Sensor_register
	{0x4186, 0xE0},  //Sensor_register
	{0x4190, 0xF3},  //Sensor_register
	{0x4192, 0xF7},  //Sensor_register
	{0x419C, 0x78},  //Sensor_register
	{0x419E, 0x7C},  //Sensor_register
	{0x41A0, 0xE5},  //Sensor_register
	{0x41A2, 0xE9},  //Sensor_register
	{0x41C8, 0xE2},  //Sensor_register
	{0x41CA, 0xFD},  //Sensor_register
	{0x41DC, 0x67},  //Sensor_register
	{0x41DE, 0x82},  //Sensor_register
	{0x41E0, 0xD4},  //Sensor_register
	{0x41E2, 0xEF},  //Sensor_register
	{0x4200, 0xDE},  //Sensor_register
	{0x4202, 0xDA},  //Sensor_register
	{0x4218, 0x63},  //Sensor_register
	{0x421A, 0x5F},  //Sensor_register
	{0x421C, 0xD0},  //Sensor_register
	{0x421E, 0xCC},  //Sensor_register
	{0x425A, 0x82},  //Sensor_register
	{0x425C, 0xEF},  //Sensor_register
	{0x4348, 0xFE},  //Sensor_register
	{0x4349, 0x06},  //Sensor_register
	{0x4352, 0xCE},  //Sensor_register
	{0x4420, 0x0B},  //Sensor_register
	{0x4421, 0x02},  //Sensor_register
	{0x4422, 0x4D},  //Sensor_register
	{0x4423, 0x0A},  //Sensor_register
	{0x4426, 0xF5},  //Sensor_register
	{0x442A, 0xE7},  //Sensor_register
	{0x4432, 0xF5},  //Sensor_register
	{0x4436, 0xE7},  //Sensor_register
	{0x4466, 0xB4},  //Sensor_register
	{0x446E, 0x32},  //Sensor_register
	{0x449F, 0x1C},  //Sensor_register
	{0x44A4, 0x2C},  //Sensor_register
	{0x44A6, 0x2C},  //Sensor_register
	{0x44A8, 0x2C},  //Sensor_register
	{0x44AA, 0x2C},  //Sensor_register
	{0x44B4, 0x2C},  //Sensor_register
	{0x44B6, 0x2C},  //Sensor_register
	{0x44B8, 0x2C},  //Sensor_register
	{0x44BA, 0x2C},  //Sensor_register
	{0x44C4, 0x2C},  //Sensor_register
	{0x44C6, 0x2C},  //Sensor_register
	{0x44C8, 0x2C},  //Sensor_register
	{0x4506, 0xF3},  //Sensor_register
	{0x450E, 0xE5},  //Sensor_register
	{0x4516, 0xF3},  //Sensor_register
	{0x4522, 0xE5},  //Sensor_register
	{0x4524, 0xF3},  //Sensor_register
	{0x452C, 0xE5},  //Sensor_register
	{0x453C, 0x22},  //Sensor_register
	{0x453D, 0x1B},  //Sensor_register
	{0x453E, 0x1B},  //Sensor_register
	{0x453F, 0x15},  //Sensor_register
	{0x4540, 0x15},  //Sensor_register
	{0x4541, 0x15},  //Sensor_register
	{0x4542, 0x15},  //Sensor_register
	{0x4543, 0x15},  //Sensor_register
	{0x4544, 0x15},  //Sensor_register
	{0x4548, 0x00},  //Sensor_register
	{0x4549, 0x01},  //Sensor_register
	{0x454A, 0x01},  //Sensor_register
	{0x454B, 0x06},  //Sensor_register
	{0x454C, 0x06},  //Sensor_register
	{0x454D, 0x06},  //Sensor_register
	{0x454E, 0x06},  //Sensor_register
	{0x454F, 0x06},  //Sensor_register
	{0x4550, 0x06},  //Sensor_register
	{0x4554, 0x55},  //Sensor_register
	{0x4555, 0x02},  //Sensor_register
	{0x4556, 0x42},  //Sensor_register
	{0x4557, 0x05},  //Sensor_register
	{0x4558, 0xFD},  //Sensor_register
	{0x4559, 0x05},  //Sensor_register
	{0x455A, 0x94},  //Sensor_register
	{0x455B, 0x06},  //Sensor_register
	{0x455D, 0x06},  //Sensor_register
	{0x455E, 0x49},  //Sensor_register
	{0x455F, 0x07},  //Sensor_register
	{0x4560, 0x7F},  //Sensor_register
	{0x4561, 0x07},  //Sensor_register
	{0x4562, 0xA5},  //Sensor_register
	{0x4564, 0x55},  //Sensor_register
	{0x4565, 0x02},  //Sensor_register
	{0x4566, 0x42},  //Sensor_register
	{0x4567, 0x05},  //Sensor_register
	{0x4568, 0xFD},  //Sensor_register
	{0x4569, 0x05},  //Sensor_register
	{0x456A, 0x94},  //Sensor_register
	{0x456B, 0x06},  //Sensor_register
	{0x456D, 0x06},  //Sensor_register
	{0x456E, 0x49},  //Sensor_register
	{0x456F, 0x07},  //Sensor_register
	{0x4572, 0xA5},  //Sensor_register
	{0x460C, 0x7D},  //Sensor_register
	{0x460E, 0xB1},  //Sensor_register
	{0x4614, 0xA8},  //Sensor_register
	{0x4616, 0xB2},  //Sensor_register
	{0x461C, 0x7E},  //Sensor_register
	{0x461E, 0xA7},  //Sensor_register
	{0x4624, 0xA8},  //Sensor_register
	{0x4626, 0xB2},  //Sensor_register
	{0x462C, 0x7E},  //Sensor_register
	{0x462E, 0x8A},  //Sensor_register
	{0x4630, 0x94},  //Sensor_register
	{0x4632, 0xA7},  //Sensor_register
	{0x4634, 0xFB},  //Sensor_register
	{0x4636, 0x2F},  //Sensor_register
	{0x4638, 0x81},  //Sensor_register
	{0x4639, 0x01},  //Sensor_register
	{0x463A, 0xB5},  //Sensor_register
	{0x463B, 0x01},  //Sensor_register
	{0x463C, 0x26},  //Sensor_register
	{0x463E, 0x30},  //Sensor_register
	{0x4640, 0xAC},  //Sensor_register
	{0x4641, 0x01},  //Sensor_register
	{0x4642, 0xB6},  //Sensor_register
	{0x4643, 0x01},  //Sensor_register
	{0x4644, 0xFC},  //Sensor_register
	{0x4646, 0x25},  //Sensor_register
	{0x4648, 0x82},  //Sensor_register
	{0x4649, 0x01},  //Sensor_register
	{0x464A, 0xAB},  //Sensor_register
	{0x464B, 0x01},  //Sensor_register
	{0x464C, 0x26},  //Sensor_register
	{0x464E, 0x30},  //Sensor_register
	{0x4654, 0xFC},  //Sensor_register
	{0x4656, 0x08},  //Sensor_register
	{0x4658, 0x12},  //Sensor_register
	{0x465A, 0x25},  //Sensor_register
	{0x4662, 0xFC},  //Sensor_register
	{0x46A2, 0xFB},  //Sensor_register
	{0x46D6, 0xF3},  //Sensor_register
	{0x46E6, 0x00},  //Sensor_register
	{0x46E8, 0xFF},  //Sensor_register
	{0x46E9, 0x03},  //Sensor_register
	{0x46EC, 0x7A},  //Sensor_register
	{0x46EE, 0xE5},  //Sensor_register
	{0x46F4, 0xEE},  //Sensor_register
	{0x46F6, 0xF2},  //Sensor_register
	{0x470C, 0xFF},  //Sensor_register
	{0x470D, 0x03},  //Sensor_register
	{0x470E, 0x00},  //Sensor_register
	{0x4714, 0xE0},  //Sensor_register
	{0x4716, 0xE4},  //Sensor_register
	{0x471E, 0xED},  //Sensor_register
	{0x472E, 0x00},  //Sensor_register
	{0x4730, 0xFF},  //Sensor_register
	{0x4731, 0x03},  //Sensor_register
	{0x4734, 0x7B},  //Sensor_register
	{0x4736, 0xDF},  //Sensor_register
	{0x4754, 0x7D},  //Sensor_register
	{0x4756, 0x8B},  //Sensor_register
	{0x4758, 0x93},  //Sensor_register
	{0x475A, 0xB1},  //Sensor_register
	{0x475C, 0xFB},  //Sensor_register
	{0x475E, 0x09},  //Sensor_register
	{0x4760, 0x11},  //Sensor_register
	{0x4762, 0x2F},  //Sensor_register
	{0x4766, 0xCC},  //Sensor_register
	{0x4776, 0xCB},  //Sensor_register
	{0x477E, 0x4A},  //Sensor_register
	{0x478E, 0x49},  //Sensor_register
	{0x4794, 0x7C},  //Sensor_register
	{0x4796, 0x8F},  //Sensor_register
	{0x4798, 0xB3},  //Sensor_register
	{0x4799, 0x00},  //Sensor_register
	{0x479A, 0xCC},  //Sensor_register
	{0x479C, 0xC1},  //Sensor_register
	{0x479E, 0xCB},  //Sensor_register
	{0x47A4, 0x7D},  //Sensor_register
	{0x47A6, 0x8E},  //Sensor_register
	{0x47A8, 0xB4},  //Sensor_register
	{0x47A9, 0x00},  //Sensor_register
	{0x47AA, 0xC0},  //Sensor_register
	{0x47AC, 0xFA},  //Sensor_register
	{0x47AE, 0x0D},  //Sensor_register
	{0x47B0, 0x31},  //Sensor_register
	{0x47B1, 0x01},  //Sensor_register
	{0x47B2, 0x4A},  //Sensor_register
	{0x47B3, 0x01},  //Sensor_register
	{0x47B4, 0x3F},  //Sensor_register
	{0x47B6, 0x49},  //Sensor_register
	{0x47BC, 0xFB},  //Sensor_register
	{0x47BE, 0x0C},  //Sensor_register
	{0x47C0, 0x32},  //Sensor_register
	{0x47C1, 0x01},  //Sensor_register
	{0x47C2, 0x3E},  //Sensor_register
	{0x47C3, 0x01},  //Sensor_register

	{REG_NULL, 0x00},
};

static const struct regval imx678_dol_hdr_3frame_3840_2160_4lane_setting_regs[] = {
	{0x3018, 0x00},    //WINMODE[4:0]
	{0x301A, 0x02},    //WDMODE
	{0x301B, 0x00},    //ADDMODE[1:0]
	{0x301C, 0x01},    //THIN_V_EN
	{0x301E, 0x01},    //VCMODE
	{0x3020, 0x00},    //HREVERSE
	{0x3021, 0x00},    //VREVERSE
	{0x3022, 0x00},    //ADBIT[1:0]
	{0x3023, 0x00},    //MDBIT
#if (IMX678_DOL_HDR_3840_2160_4LANE_15FPS_16_7FPS == 0)     //15fps
	{0x3028, 0xC3},    //VMAX[19:0]
	{0x3029, 0x09},    //VMAX[19:0]
#elif (IMX678_DOL_HDR_3840_2160_4LANE_15FPS_16_7FPS == 1)   //16.7fps
	{0x3028, 0xCA},    //VMAX[19:0]
	{0x3029, 0x08},    //VMAX[19:0]
#endif
	{0x302A, 0x00},    //VMAX[19:0]
	{0x302C, 0x94},    //HMAX[15:0]
	{0x302D, 0x02},    //HMAX[15:0]
	{0x3030, 0x00},    //FDG_SEL0[1:0]
	{0x3031, 0x00},    //FDG_SEL1[1:0]
	{0x3032, 0x00},    //FDG_SEL2[1:0]
	{0x303C, 0x00},    //PIX_HST[12:0]
	{0x303D, 0x00},    //PIX_HST[12:0]
	{0x303E, 0x10},    //PIX_HWIDTH[12:0]
	{0x303F, 0x0F},    //PIX_HWIDTH[12:0]
	{0x3040, 0x03},    //LANEMODE[2:0]
	{0x3042, 0x00},    //XSIZE_OVERLAP[10:0]
	{0x3043, 0x00},    //XSIZE_OVERLAP[10:0]
	{0x3044, 0x00},    //PIX_VST[11:0]
	{0x3045, 0x00},    //PIX_VST[11:0]
	{0x3046, 0x84},    //PIX_VWIDTH[11:0]
	{0x3047, 0x08},    //PIX_VWIDTH[11:0]

#if (IMX678_DOL_HDR_3840_2160_4LANE_15FPS_16_7FPS == 0)     //15fps
	{0x3050, 0x41},  //SHR0[19:0]
	{0x3051, 0x16},  //SHR0[19:0]
#elif (IMX678_DOL_HDR_3840_2160_4LANE_15FPS_16_7FPS == 1)   //16.7fps
	{0x3050, 0x56},  //SHR0[19:0]
	{0x3051, 0x13},  //SHR0[19:0]
#endif	
	{0x3052, 0x00},  //SHR0[19:0]
	{0x3054, 0x07},  //SHR1[19:0]
	{0x3055, 0x00},  //SHR1[19:0]
	{0x3056, 0x00},  //SHR1[19:0]
	{0x3058, 0x83},  //SHR2[19:0]
	{0x3059, 0x00},  //SHR2[19:0]
	{0x305A, 0x00},  //SHR2[19:0]
	{0x3060, 0x79},  //RHS1[19:0]
	{0x3061, 0x00},  //RHS1[19:0]
	{0x3062, 0x00},  //RHS1[19:0]
	{0x3064, 0x86},  //RHS2[19:0]
	{0x3065, 0x00},  //RHS2[19:0]
	{0x3066, 0x00},  //RHS2[19:0]

	{0x3001, 0x00},    //REGHOLD

	{REG_NULL, 0x00},
};


static const struct regval imx678_dol_hdr_2frame_3840_2160_4lane_setting_regs[] = {
	{0x3018, 0x00},    //WINMODE[4:0]
	{0x301A, 0x01},    //WDMODE
	{0x301B, 0x00},    //ADDMODE[1:0]
	{0x301C, 0x01},    //THIN_V_EN
	{0x301E, 0x01},    //VCMODE
	{0x3020, 0x00},    //HREVERSE
	{0x3021, 0x00},    //VREVERSE
	{0x3022, 0x00},    //ADBIT[1:0]
	{0x3023, 0x00},    //MDBIT
	{0x3028, 0xCA},    //VMAX[19:0]
	{0x3029, 0x08},    //VMAX[19:0]
	{0x302A, 0x00},    //VMAX[19:0]
	{0x302C, 0x94},    //HMAX[15:0]
	{0x302D, 0x02},    //HMAX[15:0]
	{0x3030, 0x00},    //FDG_SEL0[1:0]
	{0x3031, 0x00},    //FDG_SEL1[1:0]
	{0x3032, 0x00},    //FDG_SEL2[1:0]
	{0x303C, 0x00},    //PIX_HST[12:0]
	{0x303D, 0x00},    //PIX_HST[12:0]
	{0x303E, 0x10},    //PIX_HWIDTH[12:0]
	{0x303F, 0x0F},    //PIX_HWIDTH[12:0]
	{0x3040, 0x03},    //LANEMODE[2:0]
	{0x3042, 0x00},    //XSIZE_OVERLAP[10:0]
	{0x3043, 0x00},    //XSIZE_OVERLAP[10:0]
	{0x3044, 0x00},    //PIX_VST[11:0]
	{0x3045, 0x00},    //PIX_VST[11:0]
	{0x3046, 0x84},    //PIX_VWIDTH[11:0]
	{0x3047, 0x08},    //PIX_VWIDTH[11:0]

	{0x3050, 0x14},  //SHR0[19:0]
	{0x3051, 0x03},  //SHR0[19:0]
	{0x3052, 0x00},  //SHR0[19:0]
	{0x3054, 0x05},  //SHR1[19:0]
	{0x3055, 0x00},  //SHR1[19:0]
	{0x3056, 0x00},  //SHR1[19:0]
	{0x3058, 0x8A},  //SHR2[19:0]
	{0x3059, 0x01},  //SHR2[19:0]
	{0x305A, 0x00},  //SHR2[19:0]
	
	{0x3060, 0xD7},  //RHS1[19:0]
	{0x3061, 0x01},  //RHS1[19:0]
	{0x3062, 0x00},  //RHS1[19:0]
	{0x3064, 0xC4},  //RHS2[19:0]
	{0x3065, 0x0C},  //RHS2[19:0]
	{0x3066, 0x00},  //RHS2[19:0]

	{0x3001, 0x00},    //REGHOLD

	{REG_NULL, 0x00},
};

static const struct regval imx678_clear_hdr_3840_2160_4lane_setting_regs[] = {
	{0x3018, 0x00},    //WINMODE[4:0]
	{0x301A, 0x08},    //WDMODE
	{0x301B, 0x00},    //ADDMODE[1:0]
	{0x301C, 0x00},    //THIN_V_EN
	{0x301E, 0x01},    //VCMODE
	{0x3020, 0x00},    //HREVERSE
	{0x3021, 0x00},    //VREVERSE
	{0x3022, 0x00},    //ADBIT[1:0]
	{0x3023, 0x00},    //MDBIT
	{0x3028, 0x94},    //VMAX[19:0]
	{0x3029, 0x11},    //VMAX[19:0]
	{0x302A, 0x00},    //VMAX[19:0]
	{0x302C, 0x94},    //HMAX[15:0]
	{0x302D, 0x02},    //HMAX[15:0]
	{0x3030, 0x00},    //FDG_SEL0[1:0]
	{0x3031, 0x00},    //FDG_SEL1[1:0]
	{0x3032, 0x00},    //FDG_SEL2[1:0]
	{0x303C, 0x00},    //PIX_HST[12:0]
	{0x303D, 0x00},    //PIX_HST[12:0]
	{0x303E, 0x10},    //PIX_HWIDTH[12:0]
	{0x303F, 0x0F},    //PIX_HWIDTH[12:0]
	{0x3040, 0x03},    //LANEMODE[2:0]
	{0x3042, 0x00},    //XSIZE_OVERLAP[10:0]
	{0x3043, 0x00},    //XSIZE_OVERLAP[10:0]
	{0x3044, 0x00},    //PIX_VST[11:0]
	{0x3045, 0x00},    //PIX_VST[11:0]
	{0x3046, 0x84},    //PIX_VWIDTH[11:0]
	{0x3047, 0x08},    //PIX_VWIDTH[11:0]

	{0x3050, 0x06},  //SHR0[19:0]
	{0x3051, 0x00},  //SHR0[19:0]
	{0x3052, 0x00},  //SHR0[19:0]
	{0x3054, 0x0E},  //SHR1[19:0]
	{0x3055, 0x00},  //SHR1[19:0]
	{0x3056, 0x00},  //SHR1[19:0]
	{0x3058, 0x8A},  //SHR2[19:0]
	{0x3059, 0x01},  //SHR2[19:0]
	{0x305A, 0x00},  //SHR2[19:0]
	{0x3060, 0x16},  //RHS1[19:0]
	{0x3061, 0x01},  //RHS1[19:0]
	{0x3062, 0x00},  //RHS1[19:0]
	{0x3064, 0xC4},  //RHS2[19:0]
	{0x3065, 0x0C},  //RHS2[19:0]
	{0x3066, 0x00},  //RHS2[19:0]

	{0x306B, 0x04},  //Sensor_register
	{0x3081, 0x02},  //EXP_GAIN 
	{0x355A, 0x00},  //Sensor_register
	{0x3A64, 0x01},  //Sensor_register
	{0x3C37, 0x30},  //Sensor_register
	{0x3CF2, 0x78},  //Sensor_register
	{0x3CF3, 0x00},  //Sensor_register
	{0x3CF4, 0xA5},  //Sensor_register
	{0x3EB4, 0x7B},  //Sensor_register
	{0x3EB5, 0x00},  //Sensor_register
	{0x3EB6, 0xA5},  //Sensor_register
	{0x3EB7, 0x40},  //Sensor_register
	{0x3F24, 0x17},  //Sensor_register
	{0x3F4C, 0x2D},  //Sensor_register
	{0x4420, 0xFF},  //Sensor_register
	{0x4421, 0x03},  //Sensor_register
	{0x4422, 0x00},  //Sensor_register
	{0x4423, 0x08},  //Sensor_register
	{0x44A4, 0x37},  //Sensor_register
	{0x44A6, 0x37},  //Sensor_register
	{0x44A8, 0x37},  //Sensor_register
	{0x44AA, 0x37},  //Sensor_register
	{0x44B4, 0x37},  //Sensor_register
	{0x44B6, 0x37},  //Sensor_register
	{0x44B8, 0x37},  //Sensor_register
	{0x44BA, 0x37},  //Sensor_register
	{0x44C4, 0x37},  //Sensor_register
	{0x44C6, 0x37},  //Sensor_register
	{0x44C8, 0x37},  //Sensor_register
	{0x453D, 0x18},  //Sensor_register
	{0x453E, 0x18},  //Sensor_register
	{0x453F, 0x11},  //Sensor_register
	{0x4540, 0x11},  //Sensor_register
	{0x4541, 0x11},  //Sensor_register
	{0x4542, 0x11},  //Sensor_register
	{0x4543, 0x11},  //Sensor_register
	{0x4544, 0x11},  //Sensor_register
	{0x4549, 0x00},  //Sensor_register
	{0x454A, 0x00},  //Sensor_register
	{0x454B, 0x04},  //Sensor_register
	{0x454C, 0x04},  //Sensor_register
	{0x454D, 0x04},  //Sensor_register
	{0x454E, 0x04},  //Sensor_register
	{0x454F, 0x04},  //Sensor_register
	{0x4550, 0x04},  //Sensor_register
	
	{0x3001, 0x00},    //REGHOLD

	{REG_NULL, 0x00},
};

static const struct regval imx678_normal_3840_2160_60fps_4lane_global_setting_regs[] = {
	{0x3000, 0x01},		//standby
	{0x3001, 0x00},		//hold on 
	{0x3002, 0x00},		//master start
	{REG_NULL, 0x00},
};

/*
"All-pixel scan
CSI-2_4lane
24MHz
AD:10bit Output:10bit
1440Mbps
Master Mode
LCG Mode
60fps
Integration Time
16.644ms"
*/
static const struct regval imx678_normal_3840_2160_60fps_4lane_setting_regs[] = {
	{0x3014, 0x04},		//INCK_SEL	24M
	{0x3015, 0x03},		//DATARATE_SEL
	{0x3022, 0x00},		//ADBIT
	{0x3023, 0x00},		//MDBIT
	{0x302C, 0x26},		//HMAX
	{0x302D, 0x02},		//HMAX
	{0x3050, 0x03},		//SHR0
	{0x30A6, 0x00},		//xhs_drv
	{0x3460, 0x22},		//-
	{0x355A, 0x64},		//Other than Clear HDR mode
	{0x3A02, 0x7A},		//-
	{0x3A10, 0xEC},		//-
	{0x3A12, 0x71},		//-
	{0x3A14, 0xDE},		//-
	{0x3A20, 0x2B},		//Other than Clear HDR mode
	{0x3A24, 0x22},		//Other than Clear HDR mode
	{0x3A25, 0x25},		//-
	{0x3A26, 0x2A},		//Other than Clear HDR mode
	{0x3A27, 0x2C},		//-
	{0x3A28, 0x39},		//Other than Clear HDR mode
	{0x3A29, 0x38},		//-
	{0x3A30, 0x04},		//-
	{0x3A31, 0x04},		//-
	{0x3A32, 0x03},		//-
	{0x3A33, 0x03},		//-
	{0x3A34, 0x09},		//-
	{0x3A35, 0x06},		//-
	{0x3A38, 0xCD},		//-
	{0x3A3A, 0x4C},		//-
	{0x3A3C, 0xB9},		//-
	{0x3A3E, 0x30},		//-
	{0x3A40, 0x2C},		//-
	{0x3A42, 0x39},		//-
	{0x3A4E, 0x00},		//-
	{0x3A52, 0x00},		//-
	{0x3A56, 0x00},		//-
	{0x3A5A, 0x00},		//-
	{0x3A5E, 0x00},		//-
	{0x3A62, 0x00},		//-
	{0x3A6E, 0xA0},		//-
	{0x3A70, 0x50},		//-
	{0x3A8C, 0x04},		//-
	{0x3A8D, 0x03},		//-
	{0x3A8E, 0x09},		//-
	{0x3A90, 0x38},		//-
	{0x3A91, 0x42},		//-
	{0x3A92, 0x3C},		//-
	{0x3B0E, 0xF3},		//-
	{0x3B12, 0xE5},		//-
	{0x3B27, 0xC0},		//-
	{0x3B2E, 0xEF},		//-
	{0x3B30, 0x6A},		//-
	{0x3B32, 0xF6},		//-
	{0x3B36, 0xE1},		//-
	{0x3B3A, 0xE8},		//-
	{0x3B5A, 0x17},		//-
	{0x3B5E, 0xEF},		//-
	{0x3B60, 0x6A},		//-
	{0x3B62, 0xF6},		//-
	{0x3B66, 0xE1},		//-
	{0x3B6A, 0xE8},		//-
	{0x3B88, 0xEC},		//-
	{0x3B8A, 0xED},		//-
	{0x3B94, 0x71},		//-
	{0x3B96, 0x72},		//-
	{0x3B98, 0xDE},		//-
	{0x3B9A, 0xDF},		//-
	{0x3C0F, 0x06},		//-
	{0x3C10, 0x06},		//-
	{0x3C11, 0x06},		//-
	{0x3C12, 0x06},		//-
	{0x3C13, 0x06},		//-
	{0x3C18, 0x20},		//-
	{0x3C37, 0x10},		//-
	{0x3C3A, 0x7A},		//-
	{0x3C40, 0xF4},		//-
	{0x3C48, 0xE6},		//-
	{0x3C54, 0xCE},		//-
	{0x3C56, 0xD0},		//-
	{0x3C6C, 0x53},		//-
	{0x3C6E, 0x55},		//-
	{0x3C70, 0xC0},		//-
	{0x3C72, 0xC2},		//-
	{0x3C7E, 0xCE},		//-
	{0x3C8C, 0xCF},		//-
	{0x3C8E, 0xEB},		//-
	{0x3C98, 0x54},		//-
	{0x3C9A, 0x70},		//-
	{0x3C9C, 0xC1},		//-
	{0x3C9E, 0xDD},		//-
	{0x3CB0, 0x7A},		//-
	{0x3CB2, 0xBA},		//-
	{0x3CC8, 0xBC},		//-
	{0x3CCA, 0x7C},		//-
	{0x3CD4, 0xEA},		//-
	{0x3CD5, 0x01},		//-
	{0x3CD6, 0x4A},		//-
	{0x3CD8, 0x00},		//-
	{0x3CD9, 0x00},		//-
	{0x3CDA, 0xFF},		//-
	{0x3CDB, 0x03},		//-
	{0x3CDC, 0x00},		//-
	{0x3CDD, 0x00},		//-
	{0x3CDE, 0xFF},		//-
	{0x3CDF, 0x03},		//-
	{0x3CE4, 0x4C},		//-
	{0x3CE6, 0xEC},		//-
	{0x3CE7, 0x01},		//-
	{0x3CE8, 0xFF},		//-
	{0x3CE9, 0x03},		//-
	{0x3CEA, 0x00},		//-
	{0x3CEB, 0x00},		//-
	{0x3CEC, 0xFF},		//-
	{0x3CED, 0x03},		//-
	{0x3CEE, 0x00},		//-
	{0x3CEF, 0x00},		//-
	{0x3CF2, 0xFF},		//-
	{0x3CF3, 0x03},		//-
	{0x3CF4, 0x00},		//-
	{0x3E28, 0x82},		//-
	{0x3E2A, 0x80},		//-
	{0x3E30, 0x85},		//-
	{0x3E32, 0x7D},		//-
	{0x3E5C, 0xCE},		//-
	{0x3E5E, 0xD3},		//-
	{0x3E70, 0x53},		//-
	{0x3E72, 0x58},		//-
	{0x3E74, 0xC0},		//-
	{0x3E76, 0xC5},		//-
	{0x3E78, 0xC0},		//-
	{0x3E79, 0x01},		//-
	{0x3E7A, 0xD4},		//-
	{0x3E7B, 0x01},		//-
	{0x3EB4, 0x0B},		//Other than Clear HDR mode
	{0x3EB5, 0x02},		//Other than Clear HDR mode
	{0x3EB6, 0x4D},		//Other than Clear HDR mode
	{0x3EEC, 0xF3},		//-
	{0x3EEE, 0xE7},		//-
	{0x3F01, 0x01},		//-
	{0x3F24, 0x10},		//Other than Clear HDR mode
	{0x3F28, 0x2D},		//-
	{0x3F2A, 0x2D},		//-
	{0x3F2C, 0x2D},		//-
	{0x3F2E, 0x2D},		//-
	{0x3F30, 0x23},		//-
	{0x3F38, 0x2D},		//-
	{0x3F3A, 0x2D},		//-
	{0x3F3C, 0x2D},		//-
	{0x3F3E, 0x28},		//-
	{0x3F40, 0x1E},		//-
	{0x3F48, 0x2D},		//-
	{0x3F4A, 0x2D},		//-
	{0x3F4C, 0x00},		//-
	{0x4004, 0xE4},		//-
	{0x4006, 0xFF},		//-
	{0x4018, 0x69},		//-
	{0x401A, 0x84},		//-
	{0x401C, 0xD6},		//-
	{0x401E, 0xF1},		//-
	{0x4038, 0xDE},		//-
	{0x403A, 0x00},		//-
	{0x403B, 0x01},		//-
	{0x404C, 0x63},		//-
	{0x404E, 0x85},		//-
	{0x4050, 0xD0},		//-
	{0x4052, 0xF2},		//-
	{0x4108, 0xDD},		//-
	{0x410A, 0xF7},		//-
	{0x411C, 0x62},		//-
	{0x411E, 0x7C},		//-
	{0x4120, 0xCF},		//-
	{0x4122, 0xE9},		//-
	{0x4138, 0xE6},		//-
	{0x413A, 0xF1},		//-
	{0x414C, 0x6B},		//-
	{0x414E, 0x76},		//-
	{0x4150, 0xD8},		//-
	{0x4152, 0xE3},		//-
	{0x417E, 0x03},		//-
	{0x417F, 0x01},		//-
	{0x4186, 0xE0},		//-
	{0x4190, 0xF3},		//-
	{0x4192, 0xF7},		//-
	{0x419C, 0x78},		//-
	{0x419E, 0x7C},		//-
	{0x41A0, 0xE5},		//-
	{0x41A2, 0xE9},		//-
	{0x41C8, 0xE2},		//-
	{0x41CA, 0xFD},		//-
	{0x41DC, 0x67},		//-
	{0x41DE, 0x82},		//-
	{0x41E0, 0xD4},		//-
	{0x41E2, 0xEF},		//-
	{0x4200, 0xDE},		//-
	{0x4202, 0xDA},		//-
	{0x4218, 0x63},		//-
	{0x421A, 0x5F},		//-
	{0x421C, 0xD0},		//-
	{0x421E, 0xCC},		//-
	{0x425A, 0x82},		//-
	{0x425C, 0xEF},		//-
	{0x4348, 0xFE},		//-
	{0x4349, 0x06},		//-
	{0x4352, 0xCE},		//-
	{0x4420, 0x0B},		//Other than Clear HDR mode
	{0x4421, 0x02},		//Other than Clear HDR mode
	{0x4422, 0x4D},		//Other than Clear HDR mode
	{0x4426, 0xF5},		//-
	{0x442A, 0xE7},		//-
	{0x4432, 0xF5},		//-
	{0x4436, 0xE7},		//-
	{0x4466, 0xB4},		//-
	{0x446E, 0x32},		//-
	{0x449F, 0x1C},		//-
	{0x449F, 0x1C},		//-
	{0x44A4, 0x2C},		//Other than Clear HDR mode
	{0x44A6, 0x2C},		//Other than Clear HDR mode
	{0x44A8, 0x2C},		//Other than Clear HDR mode
	{0x44AA, 0x2C},		//Other than Clear HDR mode
	{0x44B4, 0x2C},		//Other than Clear HDR mode
	{0x44B6, 0x2C},		//Other than Clear HDR mode
	{0x44B8, 0x2C},		//Other than Clear HDR mode
	{0x44BA, 0x2C},		//Other than Clear HDR mode
	{0x44C4, 0x2C},		//Other than Clear HDR mode
	{0x44C6, 0x2C},		//Other than Clear HDR mode
	{0x44C8, 0x2C},		//Other than Clear HDR mode
	{0x4506, 0xF3},		//-
	{0x450E, 0xE5},		//-
	{0x4516, 0xF3},		//-
	{0x4522, 0xE5},		//-
	{0x4524, 0xF3},		//-
	{0x452C, 0xE5},		//-
	{0x453C, 0x22},		//-
	{0x453D, 0x1B},		//Other than Clear HDR mode
	{0x453E, 0x1B},		//Other than Clear HDR mode
	{0x453F, 0x15},		//Other than Clear HDR mode
	{0x4540, 0x15},		//Other than Clear HDR mode
	{0x4541, 0x15},		//Other than Clear HDR mode
	{0x4542, 0x15},		//Other than Clear HDR mode
	{0x4543, 0x15},		//Other than Clear HDR mode
	{0x4544, 0x15},		//Other than Clear HDR mode
	{0x4548, 0x00},		//-
	{0x4549, 0x01},		//Other than Clear HDR mode
	{0x454A, 0x01},		//Other than Clear HDR mode
	{0x454B, 0x06},		//Other than Clear HDR mode
	{0x454C, 0x06},		//Other than Clear HDR mode
	{0x454D, 0x06},		//Other than Clear HDR mode
	{0x454E, 0x06},		//Other than Clear HDR mode
	{0x454F, 0x06},		//Other than Clear HDR mode
	{0x4550, 0x06},		//Other than Clear HDR mode
	{0x4554, 0x55},		//-
	{0x4555, 0x02},		//-
	{0x4556, 0x42},		//-
	{0x4557, 0x05},		//-
	{0x4558, 0xFD},		//-
	{0x4559, 0x05},		//-
	{0x455A, 0x94},		//-
	{0x455B, 0x06},		//-
	{0x455D, 0x06},		//-
	{0x455E, 0x49},		//-
	{0x455F, 0x07},		//-
	{0x4560, 0x7F},		//-
	{0x4561, 0x07},		//-
	{0x4562, 0xA5},		//-
	{0x4564, 0x55},		//-
	{0x4565, 0x02},		//-
	{0x4566, 0x42},		//-
	{0x4567, 0x05},		//-
	{0x4568, 0xFD},		//-
	{0x4569, 0x05},		//-
	{0x456A, 0x94},		//-
	{0x456B, 0x06},		//-
	{0x456D, 0x06},		//-
	{0x456E, 0x49},		//-
	{0x456F, 0x07},		//-
	{0x4572, 0xA5},		//-
	{0x460C, 0x7D},		//-
	{0x460E, 0xB1},		//-
	{0x4614, 0xA8},		//-
	{0x4616, 0xB2},		//-
	{0x461C, 0x7E},		//-
	{0x461E, 0xA7},		//-
	{0x4624, 0xA8},		//-
	{0x4626, 0xB2},		//-
	{0x462C, 0x7E},		//-
	{0x462E, 0x8A},		//-
	{0x4630, 0x94},		//-
	{0x4632, 0xA7},		//-
	{0x4634, 0xFB},		//-
	{0x4636, 0x2F},		//-
	{0x4638, 0x81},		//-
	{0x4639, 0x01},		//-
	{0x463A, 0xB5},		//-
	{0x463B, 0x01},		//-
	{0x463C, 0x26},		//-
	{0x463E, 0x30},		//-
	{0x4640, 0xAC},		//-
	{0x4641, 0x01},		//-
	{0x4642, 0xB6},		//-
	{0x4643, 0x01},		//-
	{0x4644, 0xFC},		//-
	{0x4646, 0x25},		//-
	{0x4648, 0x82},		//-
	{0x4649, 0x01},		//-
	{0x464A, 0xAB},		//-
	{0x464B, 0x01},		//-
	{0x464C, 0x26},		//-
	{0x464E, 0x30},		//-
	{0x4654, 0xFC},		//-
	{0x4656, 0x08},		//-
	{0x4658, 0x12},		//-
	{0x465A, 0x25},		//-
	{0x4662, 0xFC},		//-
	{0x46A2, 0xFB},		//-
	{0x46D6, 0xF3},		//-
	{0x46E6, 0x00},		//-
	{0x46E8, 0xFF},		//-
	{0x46E9, 0x03},		//-
	{0x46EC, 0x7A},		//-
	{0x46EE, 0xE5},		//-
	{0x46F4, 0xEE},		//-
	{0x46F6, 0xF2},		//-
	{0x470C, 0xFF},		//-
	{0x470D, 0x03},		//-
	{0x470E, 0x00},		//-
	{0x4714, 0xE0},		//-
	{0x4716, 0xE4},		//-
	{0x471E, 0xED},		//-
	{0x472E, 0x00},		//-
	{0x4730, 0xFF},		//-
	{0x4731, 0x03},		//-
	{0x4734, 0x7B},		//-
	{0x4736, 0xDF},		//-
	{0x4754, 0x7D},		//-
	{0x4756, 0x8B},		//-
	{0x4758, 0x93},		//-
	{0x475A, 0xB1},		//-
	{0x475C, 0xFB},		//-
	{0x475E, 0x09},		//-
	{0x4760, 0x11},		//-
	{0x4762, 0x2F},		//-
	{0x4766, 0xCC},		//-
	{0x4776, 0xCB},		//-
	{0x477E, 0x4A},		//-
	{0x478E, 0x49},		//-
	{0x4794, 0x7C},		//-
	{0x4796, 0x8F},		//-
	{0x4798, 0xB3},		//-
	{0x4799, 0x00},		//-
	{0x479A, 0xCC},		//-
	{0x479C, 0xC1},		//-
	{0x479E, 0xCB},		//-
	{0x47A4, 0x7D},		//-
	{0x47A6, 0x8E},		//-
	{0x47A8, 0xB4},		//-
	{0x47A9, 0x00},		//-
	{0x47AA, 0xC0},		//-
	{0x47AC, 0xFA},		//-
	{0x47AE, 0x0D},		//-
	{0x47B0, 0x31},		//-
	{0x47B1, 0x01},		//-
	{0x47B2, 0x4A},		//-
	{0x47B3, 0x01},		//-
	{0x47B4, 0x3F},		//-
	{0x47B6, 0x49},		//-
	{0x47BC, 0xFB},		//-
	{0x47BE, 0x0C},		//-
	{0x47C0, 0x32},		//-
	{0x47C1, 0x01},		//-
	{0x47C2, 0x3E},		//-
	{0x47C3, 0x01},		//-
	{0x4E3C, 0x07},		//2Lnae, 4Lane
	{0x3001, 0x00},		//REGHOLD
	{REG_NULL, 0x00},
};

static const struct regval imx678_normal_3840_2160_4lane_setting_regs[] = {		
	{0x301A, 0x00},    //WDMODE
	{0x301B, 0x00},    //ADDMODE[1:0]
	{0x301C, 0x00},    //THIN_V_EN
	{0x301E, 0x01},    //VCMODE
	{0x3020, 0x00},    //HREVERSE
	{0x3021, 0x00},    //VREVERSE
	{0x3022, 0x00},    //ADBIT[1:0]
	{0x3023, 0x00},    //MDBIT
#if (IMX678_NORMAL_3840_2160_4LANE_25FPS_30FPS_30CP == 0)        //25fps
	{0x3018, 0x00},    //WINMODE[4:0]
	{0x3028, 0x94},    //VMAX[19:0]
	{0x3029, 0x11},    //VMAX[19:0]
	{0x303C, 0x00},    //PIX_HST[12:0]
	{0x303D, 0x00},    //PIX_HST[12:0]
	{0x303E, 0x10},    //PIX_HWIDTH[12:0]
	{0x303F, 0x0F},    //PIX_HWIDTH[12:0]
	{0x3044, 0x00},    //PIX_VST[11:0]
	{0x3045, 0x00},    //PIX_VST[11:0]
	{0x3046, 0x84},    //PIX_VWIDTH[11:0]
	{0x3047, 0x08},    //PIX_VWIDTH[11:0]
#elif (IMX678_NORMAL_3840_2160_4LANE_25FPS_30FPS_30CP == 1)      //30fps
	{0x3018, 0x00},    //WINMODE[4:0]
	{0x3028, 0xA6},    //VMAX[19:0]
	{0x3029, 0x0E},    //VMAX[19:0]
	{0x303C, 0x00},    //PIX_HST[12:0]
	{0x303D, 0x00},    //PIX_HST[12:0]
	{0x303E, 0x10},    //PIX_HWIDTH[12:0]
	{0x303F, 0x0F},    //PIX_HWIDTH[12:0]
	{0x3044, 0x00},    //PIX_VST[11:0]
	{0x3045, 0x00},    //PIX_VST[11:0]
	{0x3046, 0x84},    //PIX_VWIDTH[11:0]
	{0x3047, 0x08},    //PIX_VWIDTH[11:0]
#elif (IMX678_NORMAL_3840_2160_4LANE_25FPS_30FPS_30CP == 2)      //30fps crop
	{0x3018, 0x04},    //WINMODE[4:0]
	{0x3028, 0xA6},    //VMAX[19:0]
	{0x3029, 0x0E},    //VMAX[19:0]
	{0x303C, 0x08},    //PIX_HST[12:0]
	{0x303D, 0x00},    //PIX_HST[12:0]
	{0x303E, 0x00},    //PIX_HWIDTH[12:0]
	{0x303F, 0x0F},    //PIX_HWIDTH[12:0]
	{0x3044, 0x08},    //PIX_VST[11:0]
	{0x3045, 0x00},    //PIX_VST[11:0]
	{0x3046, 0x74},    //PIX_VWIDTH[11:0]
	{0x3047, 0x08},    //PIX_VWIDTH[11:0]
#endif	
	{0x302A, 0x00},    //VMAX[19:0]
	{0x302C, 0x94},    //HMAX[15:0]
	{0x302D, 0x02},    //HMAX[15:0]
	{0x3030, 0x00},    //FDG_SEL0[1:0]
	{0x3031, 0x00},    //FDG_SEL1[1:0]
	{0x3032, 0x00},    //FDG_SEL2[1:0]
	
	{0x3040, 0x03},    //LANEMODE[2:0]
	{0x3042, 0x00},    //XSIZE_OVERLAP[10:0]
	{0x3043, 0x00},    //XSIZE_OVERLAP[10:0]


	{0x3001, 0x00},    //REGHOLD

	{REG_NULL, 0x00},
};


static const struct regval imx678_normal_2688_1512_4lane_setting_regs[] = {
	{0x3018, 0x04},    //WINMODE[4:0]
	{0x301A, 0x00},    //WDMODE
	{0x301B, 0x00},    //ADDMODE[1:0]
	{0x301C, 0x00},    //THIN_V_EN
	{0x301E, 0x01},    //VCMODE
	{0x3020, 0x00},    //HREVERSE
	{0x3021, 0x00},    //VREVERSE
	{0x3022, 0x00},    //ADBIT[1:0]
	{0x3023, 0x00},    //MDBIT
	
#if (IMX678_NORMAL_2688_1512_4LANE_30FPS_60FPS == 0)   //30fps
	{0x3028, 0xA6},    //VMAX[19:0]
	{0x3029, 0x0E},    //VMAX[19:0]
	{0x302A, 0x00},    //VMAX[19:0]
	{0x302C, 0x94},    //HMAX[15:0]
	{0x302D, 0x02},    //HMAX[15:0]
#elif (IMX678_NORMAL_2688_1512_4LANE_30FPS_60FPS == 1)  //60fps
	{0x3028, 0xCA},    //VMAX[19:0]
	{0x3029, 0x08},    //VMAX[19:0]
	{0x302A, 0x00},    //VMAX[19:0]
	{0x302C, 0x26},    //HMAX[15:0]
	{0x302D, 0x02},    //HMAX[15:0]
#endif
		
	{0x3030, 0x00},    //FDG_SEL0[1:0]
	{0x3031, 0x00},    //FDG_SEL1[1:0]
	{0x3032, 0x00},    //FDG_SEL2[1:0]
	{0x303C, 0x48},    //PIX_HST[12:0]
	{0x303D, 0x02},    //PIX_HST[12:0]
	{0x303E, 0x80},    //PIX_HWIDTH[12:0]
	{0x303F, 0x0A},    //PIX_HWIDTH[12:0]
	{0x3040, 0x03},    //LANEMODE[2:0]
	{0x3042, 0x00},    //XSIZE_OVERLAP[10:0]
	{0x3043, 0x00},    //XSIZE_OVERLAP[10:0]
	{0x3044, 0x48},    //PIX_VST[11:0]
	{0x3045, 0x01},    //PIX_VST[11:0]
	{0x3046, 0xF4},    //PIX_VWIDTH[11:0]
	{0x3047, 0x05},    //PIX_VWIDTH[11:0]

	{0x3001, 0x00},    //REGHOLD

	{REG_NULL, 0x00},
};


static const struct regval imx678_normal_1920_1080_4lane_setting_regs[] = {
	{0x3000, 0x01},  //STANDBY
	{0x3001, 0x00},  //REGHOLD
	{0x3002, 0x00},  //XMSTA
	{0x3014, 0x04},  //INCK_SEL[3:0]
	{0x3015, 0x03},  //DATARATE_SEL[3:0]
	{0x3018, 0x04},  //WINMODE[4:0]
	{0x301A, 0x00},  //WDMODE
	{0x301B, 0x00},  //ADDMODE[1:0]
	{0x301C, 0x00},  //THIN_V_EN
	{0x301E, 0x01},  //VCMODE
	{0x3020, 0x00},  //HREVERSE
	{0x3021, 0x00},  //VREVERSE
	{0x3022, 0x00},  //ADBIT[1:0]
	{0x3023, 0x00},  //MDBIT
	{0x3028, 0x94},  //VMAX[19:0]
	{0x3029, 0x11},  //VMAX[19:0]
	{0x302A, 0x00},  //VMAX[19:0]
	{0x302C, 0x94},  //HMAX[15:0]
	{0x302D, 0x02},  //HMAX[15:0]
	{0x3030, 0x00},  //FDG_SEL0[1:0]
	{0x3031, 0x00},  //FDG_SEL1[1:0]
	{0x3032, 0x00},  //FDG_SEL2[1:0]
	{0x303C, 0xC8},  //PIX_HST[12:0]
	{0x303D, 0x03},  //PIX_HST[12:0]
	{0x303E, 0x80},  //PIX_HWIDTH[12:0]
	{0x303F, 0x07},  //PIX_HWIDTH[12:0]
	{0x3040, 0x03},  //LANEMODE[2:0]
	{0x3042, 0x00},  //XSIZE_OVERLAP[10:0]
	{0x3043, 0x00},  //XSIZE_OVERLAP[10:0] 
	{0x3044, 0x24},  //PIX_VST[11:0]
	{0x3045, 0x02},  //PIX_VST[11:0]
	{0x3046, 0x3C},  //PIX_VWIDTH[11:0]
	{0x3047, 0x04},  //PIX_VWIDTH[11:0]

	{0x3001, 0x00},    //REGHOLD

	{REG_NULL, 0x00},
};

static const struct regval imx678_normal_1216_2160_4lane_setting_regs[] = {
	{0x3018, 0x04},    //WINMODE[4:0]
	{0x301A, 0x00},    //WDMODE
	{0x301B, 0x00},    //ADDMODE[1:0]
	{0x301C, 0x00},    //THIN_V_EN
	{0x301E, 0x01},    //VCMODE
	{0x3020, 0x00},    //HREVERSE
	{0x3021, 0x00},    //VREVERSE
	{0x3022, 0x00},    //ADBIT[1:0]
	{0x3023, 0x00},    //MDBIT
	
	{0x3028, 0xA6},    //VMAX[19:0]
	{0x3029, 0x0E},    //VMAX[19:0]
	{0x302A, 0x00},    //VMAX[19:0]
	{0x302C, 0x94},    //HMAX[15:0]
	{0x302D, 0x02},    //HMAX[15:0]
		
	{0x3030, 0x00},    //FDG_SEL0[1:0]
	{0x3031, 0x00},    //FDG_SEL1[1:0]
	{0x3032, 0x00},    //FDG_SEL2[1:0]
	
	{0x303C, 0x28},    //PIX_HST[12:0]
	{0x303D, 0x05},    //PIX_HST[12:0]
	{0x303E, 0xC0},    //PIX_HWIDTH[12:0]
	{0x303F, 0x04},    //PIX_HWIDTH[12:0]
	
	{0x3040, 0x03},    //LANEMODE[2:0]
	{0x3042, 0x00},    //XSIZE_OVERLAP[10:0]
	{0x3043, 0x00},    //XSIZE_OVERLAP[10:0]
	
	{0x3044, 0x00},    //PIX_VST[11:0]
	{0x3045, 0x00},    //PIX_VST[11:0]
	{0x3046, 0x84},    //PIX_VWIDTH[11:0]
	{0x3047, 0x08},    //PIX_VWIDTH[11:0]

	{0x3001, 0x00},    //REGHOLD

	{REG_NULL, 0x00},
};



/************************************************************************分割线_2lane**************************************************************************/
static const struct regval imx678_global_2lane_setting_regs[] = {
	{0x3000, 0x01},    //standby
	{0x3001, 0x00},    //hold on 
	{0x3002, 0x00},		//master start

	{0x3018, 0x04},     //WINMODE:crop mode
	
	{0x3460, 0x22},
	{0x355A, 0x64},
	{0x3A02, 0x7A},
	{0x3A10, 0xEC},
	{0x3A12, 0x71},
	
	{0x3A14, 0xDE},
	{0x3A20, 0x2B},
	{0x3A24, 0x22},
	{0x3A25, 0x25},
	{0x3A26, 0x2A},
	{0x3A27, 0x2C},
	{0x3A28, 0x39},
	{0x3A29, 0x38},
	{0x3A30, 0x04},
	{0x3A31, 0x04},
	{0x3A32, 0x03},
	{0x3A33, 0x03},
	{0x3A34, 0x09},
	{0x3A35, 0x06},
	{0x3A38, 0xCD},
	{0x3A3A, 0x4C},
	{0x3A3C, 0xB9},
	{0x3A3E, 0x30},
	{0x3A40, 0x2C},
	{0x3A42, 0x39},
	{0x3A4E, 0x00},
	{0x3A52, 0x00},
	{0x3A56, 0x00},
	{0x3A5A, 0x00},
	{0x3A5E, 0x00},
	{0x3A62, 0x00},
	{0x3A64, 0x00},
	{0x3A6E, 0xA0},
	{0x3A70, 0x50},
	{0x3A8C, 0x04},
	{0x3A8D, 0x03},
	{0x3A8E, 0x09},
	{0x3A90, 0x38},
	{0x3A91, 0x42},
	{0x3A92, 0x3C},
	{0x3B0E, 0xF3},
	{0x3B12, 0xE5},
	{0x3B27, 0xC0},
	{0x3B2E, 0xEF},
	{0x3B30, 0x6A},
	{0x3B32, 0xF6},
	{0x3B36, 0xE1},
	{0x3B3A, 0xE8},
	{0x3B5A, 0x17},
	{0x3B5E, 0xEF},
	{0x3B60, 0x6A},
	{0x3B62, 0xF6},
	{0x3B66, 0xE1},
	{0x3B6A, 0xE8},
	{0x3B88, 0xEC},
	{0x3B8A, 0xED},
	{0x3B94, 0x71},
	{0x3B96, 0x72},
	{0x3B98, 0xDE},
	{0x3B9A, 0xDF},
	{0x3C0F, 0x06},
	{0x3C10, 0x06},
	{0x3C11, 0x06},
	{0x3C12, 0x06},
	{0x3C13, 0x06},
	{0x3C18, 0x20},
	{0x3C37, 0x10},
	{0x3C3A, 0x7A},
	{0x3C40, 0xF4},
	{0x3C48, 0xE6},
	{0x3C54, 0xCE},
	{0x3C56, 0xD0},
	{0x3C6C, 0x53},
	{0x3C6E, 0x55},
	{0x3C70, 0xC0},
	{0x3C72, 0xC2},
	{0x3C7E, 0xCE},
	{0x3C8C, 0xCF},
	{0x3C8E, 0xEB},
	{0x3C98, 0x54},
	{0x3C9A, 0x70},
	{0x3C9C, 0xC1},
	{0x3C9E, 0xDD},
	{0x3CB0, 0x7A},
	{0x3CB2, 0xBA},
	{0x3CC8, 0xBC},
	{0x3CCA, 0x7C},
	{0x3CD4, 0xEA},
	{0x3CD5, 0x01},
	{0x3CD6, 0x4A},
	{0x3CD8, 0x00},
	{0x3CD9, 0x00},
	{0x3CDA, 0xFF},
	{0x3CDB, 0x03},
	{0x3CDC, 0x00},
	{0x3CDD, 0x00},
	{0x3CDE, 0xFF},
	{0x3CDF, 0x03},
	{0x3CE4, 0x4C},
	{0x3CE6, 0xEC},
	{0x3CE7, 0x01},
	{0x3CE8, 0xFF},
	{0x3CE9, 0x03},
	{0x3CEA, 0x00},
	{0x3CEB, 0x00},
	{0x3CEC, 0xFF},
	{0x3CED, 0x03},
	{0x3CEE, 0x00},
	{0x3CEF, 0x00},
	{0x3CF2, 0xFF},
	{0x3CF3, 0x03},
	{0x3CF4, 0x00},
	{0x3E28, 0x82},
	{0x3E2A, 0x80},
	{0x3E30, 0x85},
	{0x3E32, 0x7D},
	{0x3E5C, 0xCE},
	{0x3E5E, 0xD3},
	{0x3E70, 0x53},
	{0x3E72, 0x58},
	{0x3E74, 0xC0},
	{0x3E76, 0xC5},
	{0x3E78, 0xC0},
	{0x3E79, 0x01},
	{0x3E7A, 0xD4},
	{0x3E7B, 0x01},
	{0x3EB4, 0x0B},
	{0x3EB5, 0x02},
	{0x3EB6, 0x4D},
	{0x3EB7, 0x42},
	{0x3EEC, 0xF3},
	{0x3EEE, 0xE7},
	{0x3F01, 0x01},
	{0x3F24, 0x10},
	{0x3F28, 0x2D},
	{0x3F2A, 0x2D},
	{0x3F2C, 0x2D},
	{0x3F2E, 0x2D},
	{0x3F30, 0x23},
	{0x3F38, 0x2D},
	{0x3F3A, 0x2D},
	{0x3F3C, 0x2D},
	{0x3F3E, 0x28},
	{0x3F40, 0x1E},
	{0x3F48, 0x2D},
	{0x3F4A, 0x2D},
	{0x3F4C, 0x00},
	{0x4004, 0xE4},
	{0x4006, 0xFF},
	{0x4018, 0x69},
	{0x401A, 0x84},
	{0x401C, 0xD6},
	{0x401E, 0xF1},
	{0x4038, 0xDE},
	{0x403A, 0x00},
	{0x403B, 0x01},
	{0x404C, 0x63},
	{0x404E, 0x85},
	{0x4050, 0xD0},
	{0x4052, 0xF2},
	{0x4108, 0xDD},
	{0x410A, 0xF7},
	{0x411C, 0x62},
	{0x411E, 0x7C},
	{0x4120, 0xCF},
	{0x4122, 0xE9},
	{0x4138, 0xE6},
	{0x413A, 0xF1},
	{0x414C, 0x6B},
	{0x414E, 0x76},
	{0x4150, 0xD8},
	{0x4152, 0xE3},
	{0x417E, 0x03},
	{0x417F, 0x01},
	{0x4186, 0xE0},
	{0x4190, 0xF3},
	{0x4192, 0xF7},
	{0x419C, 0x78},
	{0x419E, 0x7C},
	{0x41A0, 0xE5},
	{0x41A2, 0xE9},
	{0x41C8, 0xE2},
	{0x41CA, 0xFD},
	{0x41DC, 0x67},
	{0x41DE, 0x82},
	{0x41E0, 0xD4},
	{0x41E2, 0xEF},
	{0x4200, 0xDE},
	{0x4202, 0xDA},
	{0x4218, 0x63},
	{0x421A, 0x5F},
	{0x421C, 0xD0},
	{0x421E, 0xCC},
	{0x425A, 0x82},
	{0x425C, 0xEF},
	{0x4348, 0xFE},
	{0x4349, 0x06},
	{0x4352, 0xCE},
	{0x4420, 0x0B},
	{0x4421, 0x02},
	{0x4422, 0x4D},
	{0x4423, 0x0A},
	{0x4426, 0xF5},
	{0x442A, 0xE7},
	{0x4432, 0xF5},
	{0x4436, 0xE7},
	{0x4466, 0xB4},
	{0x446E, 0x32},
	{0x449F, 0x1C},
	{0x44A4, 0x2C},
	{0x44A6, 0x2C},
	{0x44A8, 0x2C},
	{0x44AA, 0x2C},
	{0x44B4, 0x2C},
	{0x44B6, 0x2C},
	{0x44B8, 0x2C},
	{0x44BA, 0x2C},
	{0x44C4, 0x2C},
	{0x44C6, 0x2C},
	{0x44C8, 0x2C},
	{0x4506, 0xF3},
	{0x450E, 0xE5},
	{0x4516, 0xF3},
	{0x4522, 0xE5},
	{0x4524, 0xF3},
	{0x452C, 0xE5},
	{0x453C, 0x22},
	{0x453D, 0x1B},
	{0x453E, 0x1B},
	{0x453F, 0x15},
	{0x4540, 0x15},
	{0x4541, 0x15},
	{0x4542, 0x15},
	{0x4543, 0x15},
	{0x4544, 0x15},
	{0x4548, 0x00},
	{0x4549, 0x01},
	{0x454A, 0x01},
	{0x454B, 0x06},
	{0x454C, 0x06},
	{0x454D, 0x06},
	{0x454E, 0x06},
	{0x454F, 0x06},
	{0x4550, 0x06},
	{0x4554, 0x55},
	{0x4555, 0x02},
	{0x4556, 0x42},
	{0x4557, 0x05},
	{0x4558, 0xFD},
	{0x4559, 0x05},
	{0x455A, 0x94},
	{0x455B, 0x06},
	{0x455D, 0x06},
	{0x455E, 0x49},
	{0x455F, 0x07},
	{0x4560, 0x7F},
	{0x4561, 0x07},
	{0x4562, 0xA5},
	{0x4564, 0x55},
	{0x4565, 0x02},
	{0x4566, 0x42},
	{0x4567, 0x05},
	{0x4568, 0xFD},
	{0x4569, 0x05},
	{0x456A, 0x94},
	{0x456B, 0x06},
	{0x456D, 0x06},
	{0x456E, 0x49},
	{0x456F, 0x07},
	{0x4572, 0xA5},
	{0x460C, 0x7D},
	{0x460E, 0xB1},
	{0x4614, 0xA8},
	{0x4616, 0xB2},
	{0x461C, 0x7E},
	{0x461E, 0xA7},
	{0x4624, 0xA8},
	{0x4626, 0xB2},
	{0x462C, 0x7E},
	{0x462E, 0x8A},
	{0x4630, 0x94},
	{0x4632, 0xA7},
	{0x4634, 0xFB},
	{0x4636, 0x2F},
	{0x4638, 0x81},
	{0x4639, 0x01},
	{0x463A, 0xB5},
	{0x463B, 0x01},
	{0x463C, 0x26},
	{0x463E, 0x30},
	{0x4640, 0xAC},
	{0x4641, 0x01},
	{0x4642, 0xB6},
	{0x4643, 0x01},
	{0x4644, 0xFC},
	{0x4646, 0x25},
	{0x4648, 0x82},
	{0x4649, 0x01},
	{0x464A, 0xAB},
	{0x464B, 0x01},
	{0x464C, 0x26},
	{0x464E, 0x30},
	{0x4654, 0xFC},
	{0x4656, 0x08},
	{0x4658, 0x12},
	{0x465A, 0x25},
	{0x4662, 0xFC},
	{0x46A2, 0xFB},
	{0x46D6, 0xF3},
	{0x46E6, 0x00},
	{0x46E8, 0xFF},
	{0x46E9, 0x03},
	{0x46EC, 0x7A},
	{0x46EE, 0xE5},
	{0x46F4, 0xEE},
	{0x46F6, 0xF2},
	{0x470C, 0xFF},
	{0x470D, 0x03},
	{0x470E, 0x00},
	{0x4714, 0xE0},
	{0x4716, 0xE4},
	{0x471E, 0xED},
	{0x472E, 0x00},
	{0x4730, 0xFF},
	{0x4731, 0x03},
	{0x4734, 0x7B},
	{0x4736, 0xDF},
	{0x4754, 0x7D},
	{0x4756, 0x8B},
	{0x4758, 0x93},
	{0x475A, 0xB1},
	{0x475C, 0xFB},
	{0x475E, 0x09},
	{0x4760, 0x11},
	{0x4762, 0x2F},
	{0x4766, 0xCC},
	{0x4776, 0xCB},
	{0x477E, 0x4A},
	{0x478E, 0x49},
	{0x4794, 0x7C},
	{0x4796, 0x8F},
	{0x4798, 0xB3},
	{0x4799, 0x00},
	{0x479A, 0xCC},
	{0x479C, 0xC1},
	{0x479E, 0xCB},
	{0x47A4, 0x7D},
	{0x47A6, 0x8E},
	{0x47A8, 0xB4},
	{0x47A9, 0x00},
	{0x47AA, 0xC0},
	{0x47AC, 0xFA},
	{0x47AE, 0x0D},
	{0x47B0, 0x31},
	{0x47B1, 0x01},
	{0x47B2, 0x4A},
	{0x47B3, 0x01},
	{0x47B4, 0x3F},
	{0x47B6, 0x49},
	{0x47BC, 0xFB},
	{0x47BE, 0x0C},
	{0x47C0, 0x32},
	{0x47C1, 0x01},
	{0x47C2, 0x3E},
	{0x47C3, 0x01},

	{REG_NULL, 0x00},
};

static const struct regval imx678_normal_3840_2160_2lane_setting_regs[] = {
	{0x3014, 0x04},     //INCK_SEL:24MHz
	{0x3015, 0x03},     //DATARATE_SEL:1440Mbps
	{0x301A, 0x00},     //HDRMODE
	{0x301B, 0x00},     //BINING
	{0x301C, 0x00},     //DOL MODE 
	{0x301E, 0x01},
	{0x3020, 0x00},     //H DIRE
	{0x3021, 0x00},     //V DIRE
	{0x3022, 0x00},     //AD BIT
	{0x3023, 0x00},     //OUTPUT BIT
	{0x3028, 0xCA},     //VMAX
	{0x3029, 0x08},
	{0x302A, 0x00},
	{0x302C, 0x28},     //HMAX 
	{0x302D, 0x05},
	{0x3030, 0x00},    //GAIN MODE
	{0x3031, 0x00},
	{0x3032, 0x00},
	{0x303C, 0x00},   //CROP Hori Start
	{0x303D, 0x00},
	{0x303E, 0x00},   //PIX_HWIDTH
	{0x303F, 0x0F},
	{0x3040, 0x01},   //lane mode
	{0x3042, 0x00},   //xsize_overlap 
	{0x3043, 0x00},
	{0x3044, 0x00},   //CROP Vert Start 
	{0x3045, 0x00},
	{0x3046, 0x70},   //PIX_VWIDTH
	{0x3047, 0x08},
	{0x3050, 0x03},   //shr0
	{0x3051, 0x00},
	{0x3052, 0x00},
	{0x3054, 0x0E},   //shr1
	{0x3055, 0x00},
	{0x3056, 0x00},
	{0x3058, 0x8A},   //shr2
	{0x3059, 0x01},
	{0x305A, 0x00},
	{0x3060, 0x16},   //rhs1
	{0x3061, 0x01},
	{0x3062, 0x00},
	{0x3064, 0xC4},   //rhs2
	{0x3065, 0x0C},
	{0x3066, 0x00},
	{0x3069, 0x00},   //HDR_GAIN_EN
	{0x306B, 0x00},   //CLEAR_HDR_MODE
	{0x3070, 0x00},  //gain  
	{0x3071, 0x00},
	{0x3072, 0x00},  //sef1 gain
	{0x3073, 0x00},
	{0x3074, 0x00},  //sef2 gain
	{0x3075, 0x00},
	{0x3081, 0x00},  //add HG when clear HDR MODER 
	{0x308C, 0x00},  //add digital HG when clear HDR MODER    
	{0x308D, 0x01},
	{0x3094, 0x00},   //add analog LG when clear HDR MODER
	{0x3095, 0x00},
	{0x309C, 0x00},  //add analog HG when clear HDR MODER 
	{0x309D, 0x00},
	{0x30A4, 0xAA},   //xvs_drv
	{0x30A6, 0x00},   //xhs_drv
	{0x30CC, 0x00},   //xvslng
	{0x30CD, 0x00},   //xhslng
	{0x30DC, 0x32},  //blklevel
	{0x30DD, 0x40},
	{0x3400, 0x01},   //each frame gain enable
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
static const struct imx678_mode imx678_supported_modes_4lane[] = {

/******************************************normal**********************************************************/
	/*normal 3840*2160 @ 60fps */
	{
		.bus_fmt = MEDIA_BUS_FMT_SRGGB10_1X10,
		.width = 3856,
		.height = 2180,
		.real_width = 3840,
		.real_height = 2160,

		.max_fps = {
			.numerator = 10000,
			.denominator = 600000,
		},

		.exp_def = 0x08CA - 0x40,
		.hts_def = 0x0226 * 8,
		.vts_def = 0x08CA,
		.link_freq_def = (1440*1000*1000/2),
		.global_reg_list = imx678_global_4lane_setting_regs,
		.reg_list = imx678_normal_3840_2160_60fps_4lane_setting_regs,
		.hdr_mode = NO_HDR,
		.vc[PAD0] = V4L2_MBUS_CSI2_CHANNEL_0,
		.clear_hdr_mode = NO_CLEAR_HDR,
		.bpp = 10,
	},
	/*normal 3840*2160*/
	{
		.bus_fmt = MEDIA_BUS_FMT_SRGGB10_1X10,
#if (IMX678_NORMAL_3840_2160_4LANE_25FPS_30FPS_30CP == 0)        //25fps
	.width = 0xf10,
	.height = 0x884,
	.real_width = 3840,
	.real_height = 2160,

	.max_fps = {
		.numerator = 10000,
		.denominator = 250000,
	},
	.exp_def = 0x1194 - 0x40,
	.hts_def = 0x0294 * 8,
	.vts_def = 0x1194,
#elif (IMX678_NORMAL_3840_2160_4LANE_25FPS_30FPS_30CP == 1)      //30fps
		.width = 0xf10,
		.height = 0x884,
		.real_width = 3840,
		.real_height = 2160,

		.max_fps = {
			.numerator = 10000,
			.denominator = 300000,
		},
		.exp_def = 0x0EA6 - 0x40,
		.hts_def = 0x0294 * 8,
		.vts_def = 0x0EA6,
#elif (IMX678_NORMAL_3840_2160_4LANE_25FPS_30FPS_30CP == 2)      //30fps  crop
		.width = 3840,
		.height = 2164,
		.real_width = 3840,
		.real_height = 2160,

		.max_fps = {
			.numerator = 10000,
			.denominator = 300000,
		},
		.exp_def = 0x0EA6 - 0x40,
		.hts_def = 0x0294 * 8,
		.vts_def = 0x0EA6,
#endif		
		.link_freq_def = (1440*1000*1000/2),
		.global_reg_list = imx678_global_4lane_setting_regs,
		.reg_list = imx678_normal_3840_2160_4lane_setting_regs,
		.hdr_mode = NO_HDR,
		.vc[PAD0] = V4L2_MBUS_CSI2_CHANNEL_0,
		.clear_hdr_mode = NO_CLEAR_HDR,
		.bpp = 10,
	},	

	/*2688*1512*/
	{
		.bus_fmt = MEDIA_BUS_FMT_SRGGB10_1X10, 
		.width = 0xa80,
		.height = 0x5f4,
		.real_width = 2688,
		.real_height = 1512,
#if (IMX678_NORMAL_2688_1512_4LANE_30FPS_60FPS == 0)   //30fps
		.max_fps = {
			.numerator = 10000,
			.denominator = 300000,
		},
		.exp_def = 0x0EA6 - 0x40,
		.hts_def = 0x0294 * 8,
		.vts_def = 0x0EA6,
#elif (IMX678_NORMAL_2688_1512_4LANE_30FPS_60FPS == 1)  //60fps	
		.max_fps = {
			.numerator = 10000,
			.denominator = 600000,
		},
		.exp_def = 0x08CA - 0x40,
		.hts_def = 0x0226 * 8,
		.vts_def = 0x08CA,
#endif		
		.link_freq_def = (1440*1000*1000/2),
		.global_reg_list = imx678_global_4lane_setting_regs,
		.reg_list = imx678_normal_2688_1512_4lane_setting_regs,
		.hdr_mode = NO_HDR,
		.vc[PAD0] = V4L2_MBUS_CSI2_CHANNEL_0,
		.clear_hdr_mode = NO_CLEAR_HDR,
		.bpp = 10,
	},

	/*1920*1080*/
	{
		.bus_fmt = MEDIA_BUS_FMT_SRGGB10_1X10, 
		.width = 0x0780,
		.height = 0x043c,
		.real_width = 1920,
		.real_height = 1080,
		.max_fps = {
			.numerator = 10000,
			.denominator = 250000,
		},
		.exp_def = 0x1194 - 0x40,
		.hts_def = 0x0294 * 8,
		.vts_def = 0x1194,
		.link_freq_def = (1440*1000*1000/2),
		.global_reg_list = imx678_global_4lane_setting_regs,
		.reg_list = imx678_normal_1920_1080_4lane_setting_regs,
		.hdr_mode = NO_HDR,
		.vc[PAD0] = V4L2_MBUS_CSI2_CHANNEL_0,
		.clear_hdr_mode = NO_CLEAR_HDR,
		.bpp = 10,
	},

	/*1216*2160*/
	{
		.bus_fmt = MEDIA_BUS_FMT_SRGGB10_1X10, 
		.width = 0x4C0,
		.height = 0x0884,
		.real_width = 1216,
		.real_height = 2160,

		.max_fps = {
			.numerator = 10000,
			.denominator = 300000,
		},
		.exp_def = 0x0EA6 - 0x40,
		.hts_def = 0x0294 * 8, 
		.vts_def = 0x0EA6,
		
		.link_freq_def = (1440*1000*1000/2),
		.global_reg_list = imx678_global_4lane_setting_regs,
		.reg_list = imx678_normal_1216_2160_4lane_setting_regs,
		.hdr_mode = NO_HDR,
		.vc[PAD0] = V4L2_MBUS_CSI2_CHANNEL_0,
		.clear_hdr_mode = NO_CLEAR_HDR,
		.bpp = 10,
	},
	
/******************************************dol_hdr*************************************************************/
	/*dol_hdr_2frame 3840*2160*/
	{
		.bus_fmt = MEDIA_BUS_FMT_SRGGB10_1X10, 
		.width = 0xf10,
		.height = 0x884,
		.real_width = 3840,
		.real_height = 2160,
		.max_fps = {
			.numerator = 10000,
			.denominator = 250000,
		},
		.exp_def = 0x08CA * 2 - 0x0314,   //exp_def = vts_def - shr0 = FSC - SHR0
		.hts_def = 0x0294 * 8,			//hts_def = hts_def_reg * 2 * lane_num
		.vts_def = 0x08CA * 2,			//vts_def = vts_def_reg * 2 ^ (dol_hdr_n - 1)
		.link_freq_def = (1440*1000*1000/2),
		.global_reg_list = imx678_global_4lane_setting_regs,
		.reg_list = imx678_dol_hdr_2frame_3840_2160_4lane_setting_regs,
		.hdr_mode = HDR_X2,
		.vc[PAD0] = V4L2_MBUS_CSI2_CHANNEL_1,
		.vc[PAD1] = V4L2_MBUS_CSI2_CHANNEL_0,//L->csi wr0
		.vc[PAD2] = V4L2_MBUS_CSI2_CHANNEL_1,
		.vc[PAD3] = V4L2_MBUS_CSI2_CHANNEL_1,//M->csi wr2
		.clear_hdr_mode = NO_CLEAR_HDR,
		.bpp = 10,
	},

	/*dol_hdr_3frame 3840*2160*/
	{
		.bus_fmt = MEDIA_BUS_FMT_SRGGB10_1X10,
		.width = 0xf10,
		.height = 0x884,
		.real_width = 3840,
		.real_height = 2160,
#if (IMX678_DOL_HDR_3840_2160_4LANE_15FPS_16_7FPS == 0)     //15fps
		.max_fps = {
			.numerator = 10000,
			.denominator = 150000,
		},
		.exp_def = 0x09C3 * 4 - 0x1641,
		.hts_def = 0x0294 * 8,
		.vts_def = 0x09C3 * 4,
#elif (IMX678_DOL_HDR_3840_2160_4LANE_15FPS_16_7FPS == 1)   //16.7fps
		.max_fps = {
			.numerator = 10000,
			.denominator = 167000,
		},
		.exp_def = 0x08CA * 4 - 0x1356,
		.hts_def = 0x0294 * 8,
		.vts_def = 0x08CA * 4,
#endif
		.link_freq_def = (1440*1000*1000/2),
		.global_reg_list = imx678_global_4lane_setting_regs,
		.reg_list = imx678_dol_hdr_3frame_3840_2160_4lane_setting_regs,
		.hdr_mode = HDR_X3,
		.vc[PAD0] = V4L2_MBUS_CSI2_CHANNEL_2,
		.vc[PAD1] = V4L2_MBUS_CSI2_CHANNEL_1,//M->csi wr0
		.vc[PAD2] = V4L2_MBUS_CSI2_CHANNEL_0,//L->csi wr1
		.vc[PAD3] = V4L2_MBUS_CSI2_CHANNEL_2,//S->csi wr2
		.clear_hdr_mode = NO_CLEAR_HDR,
		.bpp = 10,
	},
/******************************************clear_hdr**********************************************************/
	/*clear_hdr 3840*2160*/
	{
		.bus_fmt = MEDIA_BUS_FMT_SRGGB10_1X10, 
		.width = 0xf10,
		.height = 0x884,
		.real_width = 3840,
		.real_height = 2160,
		.max_fps = {
			.numerator = 10000,
			.denominator = 250000,
		},
		.exp_def = 0x1194 * 2 - 0x06,
		.hts_def = 0x0294 * 8,
		.vts_def = 0x1194 * 2,
		.link_freq_def = (1440*1000*1000/2),
		.global_reg_list = imx678_clear_hdr_3840_2160_4lane_setting_regs,
		.reg_list = imx678_normal_3840_2160_4lane_setting_regs,

		.hdr_mode = HDR_X2,
		.vc[PAD0] = V4L2_MBUS_CSI2_CHANNEL_1,
		.vc[PAD1] = V4L2_MBUS_CSI2_CHANNEL_0,//L->csi wr0
		.vc[PAD2] = V4L2_MBUS_CSI2_CHANNEL_1,
		.vc[PAD3] = V4L2_MBUS_CSI2_CHANNEL_1,//M->csi wr2
		.clear_hdr_mode = CLEAR_HDR,
		.bpp = 10,
	},
};

static const struct imx678_mode imx678_supported_modes_2lane[] = {
	/*3840*2160*/
	{
		.bus_fmt = MEDIA_BUS_FMT_SRGGB10_1X10,
		.width = 3840,
		.height = 2160,
		.real_width = 3840,
		.real_height = 2160,
		.max_fps = {
			.numerator = 10000,
			.denominator = 150000,
		},
		.exp_def = 0x08CA - 0x10,
		.hts_def = 0x0528 * 4,
		.vts_def = 0x08CA,
		.link_freq_def = (1440*1000*1000/2),
		.global_reg_list = imx678_global_2lane_setting_regs,
		.reg_list = imx678_normal_3840_2160_2lane_setting_regs,
		.hdr_mode = NO_HDR,
		.vc[PAD0] = V4L2_MBUS_CSI2_CHANNEL_0,
		.clear_hdr_mode = NO_CLEAR_HDR,
		.bpp = 10,
	},
};


static const s64 link_freq_items[] = {
	MIPI_FREQ_720M,
};

/* Write registers up to 4 at a time */
static int imx678_write_reg(struct i2c_client *client, u16 reg,
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

static int imx678_write_array(struct i2c_client *client,
			      const struct regval *regs)
{
	u32 i;
	int ret = 0;

	for (i = 0; ret == 0 && regs[i].addr != REG_NULL; i++) {
		ret = imx678_write_reg(client, regs[i].addr,
				       IMX678_REG_VALUE_08BIT, regs[i].val);
	}
	return ret;
}

/* Read registers up to 4 at a time */
static int imx678_read_reg(struct i2c_client *client, u16 reg, unsigned int len,
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

static int imx678_get_reso_dist(const struct imx678_mode *mode,
				struct v4l2_mbus_framefmt *framefmt)
{
	return abs(mode->real_width - framefmt->width) +
	       abs(mode->real_height - framefmt->height);
}

static const struct imx678_mode *
imx678_find_best_fit(struct imx678 *imx678, struct v4l2_subdev_format *fmt)
{
	struct v4l2_mbus_framefmt *framefmt = &fmt->format;
	int dist;
	int cur_best_fit = 0;
	int cur_best_fit_dist = -1;
	unsigned int i;

	if(imx678->lane_num == 4) {
		for (i = 0; i < imx678->cfg_num; i++) {
			dist = imx678_get_reso_dist(&imx678_supported_modes_4lane[i], framefmt);
			if (cur_best_fit_dist == -1 || dist <= cur_best_fit_dist) {
				cur_best_fit_dist = dist;
				cur_best_fit = i;
			}
		}
		
		return &imx678_supported_modes_4lane[cur_best_fit];
	}
	else {
		for (i = 0; i < imx678->cfg_num; i++) {
			dist = imx678_get_reso_dist(&imx678_supported_modes_2lane[i], framefmt);
			if (cur_best_fit_dist == -1 || dist <= cur_best_fit_dist) {
				cur_best_fit_dist = dist;
				cur_best_fit = i;
			}
		}
		
		return &imx678_supported_modes_2lane[cur_best_fit];
	}
}


static void imx678_change_mode(struct imx678 *imx678, const struct imx678_mode *mode)
{
	imx678->cur_mode = mode;
	imx678->cur_vts = imx678->cur_mode->vts_def;
	dev_dbg(&imx678->client->dev, "set fmt: cur_mode: %dx%d, hdr: %d\n",
		mode->width, mode->height, mode->hdr_mode);
}

static int imx678_set_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_pad_config *cfg,
			  struct v4l2_subdev_format *fmt)
{
	struct imx678 *imx678 = to_imx678(sd);
	const struct imx678_mode *mode;
	s64 h_blank, vblank_def, vblank_min;
	u64 pixel_rate = 0;

	mutex_lock(&imx678->mutex);

	mode = imx678_find_best_fit(imx678, fmt);
	fmt->format.code = mode->bus_fmt;
	fmt->format.width = mode->width;
	fmt->format.height = mode->height;
	fmt->format.field = V4L2_FIELD_NONE;
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
		*v4l2_subdev_get_try_format(sd, cfg, fmt->pad) = fmt->format;
#else
		mutex_unlock(&imx678->mutex);
		return -ENOTTY;
#endif
	} else {
		imx678_change_mode(imx678, mode);
		h_blank = mode->hts_def - mode->width;
		__v4l2_ctrl_modify_range(imx678->hblank, h_blank,
					 h_blank, 1, h_blank);
		vblank_def = mode->vts_def - mode->height;
		/* VMAX >= (PIX_VWIDTH / 2) + 70 = height + 70 && VMAX >= 1024*/
		vblank_min = (((mode->height + IMX678_BLANK_NUM) - mode->height) > 1024) ?
			((mode->height + IMX678_BLANK_NUM) - mode->height) : 1024;
		__v4l2_ctrl_modify_range(imx678->vblank, vblank_min,
					 IMX678_VTS_MAX - mode->height,
					 1, vblank_def);
		__v4l2_ctrl_s_ctrl(imx678->link_freq, mode->mipi_freq_idx);
		pixel_rate = (u32)link_freq_items[mode->mipi_freq_idx] / mode->bpp * 2 * imx678->lane_num;
		__v4l2_ctrl_s_ctrl_int64(imx678->pixel_rate,
					 pixel_rate);
	}

	mutex_unlock(&imx678->mutex);

	return 0;
}

static int imx678_get_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_pad_config *cfg,
			  struct v4l2_subdev_format *fmt)
{
	struct imx678 *imx678 = to_imx678(sd);
	const struct imx678_mode *mode = imx678->cur_mode;

	mutex_lock(&imx678->mutex);
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
		fmt->format = *v4l2_subdev_get_try_format(sd, cfg, fmt->pad);
#else
		mutex_unlock(&imx678->mutex);
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
	mutex_unlock(&imx678->mutex);

	return 0;
}

static int imx678_enum_mbus_code(struct v4l2_subdev *sd,
				 struct v4l2_subdev_pad_config *cfg,
				 struct v4l2_subdev_mbus_code_enum *code)
{
	struct imx678 *imx678 = to_imx678(sd);

	if (code->index != 0)
		return -EINVAL;
	code->code = imx678->cur_mode->bus_fmt;

	return 0;
}

static int imx678_enum_frame_sizes(struct v4l2_subdev *sd,
				   struct v4l2_subdev_pad_config *cfg,
				   struct v4l2_subdev_frame_size_enum *fse)
{
	struct imx678 *imx678 = to_imx678(sd);

	if (fse->index >= imx678->cfg_num)
		return -EINVAL;

	if(imx678->lane_num == 4) {
		if (fse->code != imx678_supported_modes_4lane[0].bus_fmt)
			return -EINVAL;
		fse->min_width = imx678_supported_modes_4lane[fse->index].width;
		fse->max_width = imx678_supported_modes_4lane[fse->index].width;
		fse->max_height = imx678_supported_modes_4lane[fse->index].height;
		fse->min_height = imx678_supported_modes_4lane[fse->index].height;
	}
	else {
		if (fse->code != imx678_supported_modes_2lane[0].bus_fmt)
			return -EINVAL;
		fse->min_width = imx678_supported_modes_2lane[fse->index].width;
		fse->max_width = imx678_supported_modes_2lane[fse->index].width;
		fse->max_height = imx678_supported_modes_2lane[fse->index].height;
		fse->min_height = imx678_supported_modes_2lane[fse->index].height;
	}

	return 0;
}


static int imx678_g_frame_interval(struct v4l2_subdev *sd,
				   struct v4l2_subdev_frame_interval *fi)
{
	struct imx678 *imx678 = to_imx678(sd);
	const struct imx678_mode *mode = imx678->cur_mode;

	mutex_lock(&imx678->mutex);
	fi->interval = mode->max_fps;
	mutex_unlock(&imx678->mutex);

	return 0;
}



static int imx678_g_mbus_config(struct v4l2_subdev *sd, unsigned int pad_id,
				struct v4l2_mbus_config *config)
{
	struct imx678 *imx678 = to_imx678(sd);
	const struct imx678_mode *mode = imx678->cur_mode;
	u32 val = 0;

	if (mode->hdr_mode == NO_HDR) {
		val = 1 << (imx678->lane_num  - 1) |
		V4L2_MBUS_CSI2_CHANNEL_0 |
		V4L2_MBUS_CSI2_CONTINUOUS_CLOCK;
	}
	if (mode->hdr_mode == HDR_X2)
		val = 1 << (imx678->lane_num  - 1) |
		V4L2_MBUS_CSI2_CHANNEL_0 |
		V4L2_MBUS_CSI2_CONTINUOUS_CLOCK |
		V4L2_MBUS_CSI2_CHANNEL_1;
	if (mode->hdr_mode == HDR_X3)
		val = 1 << (imx678->lane_num  - 1) |
		V4L2_MBUS_CSI2_CHANNEL_0 |
		V4L2_MBUS_CSI2_CONTINUOUS_CLOCK |
		V4L2_MBUS_CSI2_CHANNEL_1 |
		V4L2_MBUS_CSI2_CHANNEL_2;

	config->type = V4L2_MBUS_CSI2_DPHY;
	config->flags = val;

	return 0;
}

static void imx678_get_module_inf(struct imx678 *imx678,
				  struct rkmodule_inf *inf)
{
	memset(inf, 0, sizeof(*inf));
	strlcpy(inf->base.sensor, IMX678_NAME, sizeof(inf->base.sensor));
	strlcpy(inf->base.module, imx678->module_name,
		sizeof(inf->base.module));
	strlcpy(inf->base.lens, imx678->len_name, sizeof(inf->base.lens));
}

static int imx678_set_hdrae(struct imx678 *imx678,
			    struct preisp_hdrae_exp_s *ae)
{
	struct i2c_client *client = imx678->client;
	u32 l_exp_time, m_exp_time, s_exp_time;
	u32 l_a_gain, m_a_gain, s_a_gain;
	u32 gain_switch = 0;
	u32 shr1 = 0;
	u32 shr0 = 0;
	u32 rhs1 = 0;
	u32 rhs1_max = 0;
	static int rhs1_old = IMX678_RHS1_DEFAULT;
	int rhs1_change_limit;
	int ret = 0;
	u32 fsc = imx678->cur_vts;
	u8 cg_mode = 0;

	if (!imx678->has_init_exp && !imx678->streaming) {
		imx678->init_hdrae_exp = *ae;
		imx678->has_init_exp = true;
		dev_dbg(&imx678->client->dev, "imx678 don't stream, record exp for hdr!\n");
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

	if (imx678->cur_mode->hdr_mode == HDR_X2) {
		if(imx678->cur_mode->clear_hdr_mode == CLEAR_HDR) {    /*clear hdr*/
		} else {                                               /*dol hdr*/
			//2 stagger
			l_a_gain = m_a_gain;
			l_exp_time = m_exp_time;
			cg_mode = ae->middle_cg_mode;
		}
	}

	if (!g_isHCG && cg_mode == GAIN_MODE_HCG) {
		gain_switch = 0x01 | 0x100;
		g_isHCG = true;
	} else if (g_isHCG && cg_mode == GAIN_MODE_LCG) {
		gain_switch = 0x00 | 0x100;
		g_isHCG = false;
	}

	if(imx678->cur_mode->clear_hdr_mode == CLEAR_HDR) {    /*clear hdr*/

		return 0;    /*************测试************/
		
		ret = imx678_write_reg(client,
			IMX678_GROUP_HOLD_REG,
			IMX678_REG_VALUE_08BIT,
			IMX678_GROUP_HOLD_START);
		//gain effect n+1
		ret |= imx678_write_reg(client,
			IMX678_LF_GAIN_REG_H,
			IMX678_REG_VALUE_08BIT,
			IMX678_FETCH_GAIN_H(l_a_gain));
		ret |= imx678_write_reg(client,
			IMX678_LF_GAIN_REG_L,
			IMX678_REG_VALUE_08BIT,
			IMX678_FETCH_GAIN_L(l_a_gain));
		ret |= imx678_write_reg(client,
			IMX678_SF1_GAIN_REG_H,
			IMX678_REG_VALUE_08BIT,
			IMX678_FETCH_GAIN_H(s_a_gain));
		ret |= imx678_write_reg(client,
			IMX678_SF1_GAIN_REG_L,
			IMX678_REG_VALUE_08BIT,
			IMX678_FETCH_GAIN_L(s_a_gain));
		if (gain_switch & 0x100)
			ret |= imx678_write_reg(client,
				IMX678_GAIN_SWITCH_REG,
				IMX678_REG_VALUE_08BIT,
				gain_switch & 0xff);

		ret |= imx678_write_reg(client,
			IMX678_GROUP_HOLD_REG,
			IMX678_REG_VALUE_08BIT,
			IMX678_GROUP_HOLD_END);

	} else {                                               /*dol hdr*/
		ret = imx678_write_reg(client,
			IMX678_GROUP_HOLD_REG,
			IMX678_REG_VALUE_08BIT,
			IMX678_GROUP_HOLD_START);
		//gain effect n+1
		ret |= imx678_write_reg(client,
			IMX678_LF_GAIN_REG_H,
			IMX678_REG_VALUE_08BIT,
			IMX678_FETCH_GAIN_H(l_a_gain));
		ret |= imx678_write_reg(client,
			IMX678_LF_GAIN_REG_L,
			IMX678_REG_VALUE_08BIT,
			IMX678_FETCH_GAIN_L(l_a_gain));
		ret |= imx678_write_reg(client,
			IMX678_SF1_GAIN_REG_H,
			IMX678_REG_VALUE_08BIT,
			IMX678_FETCH_GAIN_H(s_a_gain));
		ret |= imx678_write_reg(client,
			IMX678_SF1_GAIN_REG_L,
			IMX678_REG_VALUE_08BIT,
			IMX678_FETCH_GAIN_L(s_a_gain));
		if (gain_switch & 0x100)
			ret |= imx678_write_reg(client,
				IMX678_GAIN_SWITCH_REG,
				IMX678_REG_VALUE_08BIT,
				gain_switch & 0xff);
		/* Restrictions
		 *   FSC = 2 * VMAX = 2n                   (2n, align with 2)
		 *   SHR1 + 5 <= SHR0 <= (FSC - 2)
		 *
		 *   exp_l = FSC - SHR0
		 *   SHR0 = FSC - exp_l                     (2n, align with 2)
		 *
		 *   exp_s = RHS1 - SHR1
		 *   SHR1 + 2 <= RHS1 < BRL * 2             (2n + 1)
		 *   SHR1 + 2 <= RHS1 <= SHR0 - 5
		 *   5 <= SHR1 <= RHS1 - 2                   (2n + 1)
		 *
		 *   RHS1(n+1) >= (RHS1(n) + BRL * 2) - FSC + 2
		 *
		 *   RHS1 and SHR1 shall be even value.
		 *
		 *   T(l_exp) = FSC - SHR0,  unit: H
		 *   T(s_exp) = RHS1 - SHR1, unit: H
		 *   Exposure ratio: T(l_exp) / T(s_exp) >= 1
		 */

		/* The HDR mode vts is already double by default to workaround T-line */

		//long exposure and short exposure
		shr0 = fsc - l_exp_time;
		rhs1_max = (RHS1_MAX_X2 > (shr0 - 5)) ? (shr0 - 5) : RHS1_MAX_X2;
		rhs1 = SHR1_MIN_X2 + s_exp_time;
		dev_dbg(&client->dev, "line(%d) rhs1 %d\n", __LINE__, rhs1);
		if (rhs1 < 7)
			rhs1 = 7;
		else if (rhs1 > rhs1_max)
			rhs1 = rhs1_max;
		dev_dbg(&client->dev, "line(%d) rhs1 %d\n", __LINE__, rhs1);
		//printk("line(%d), ret %d\n", __LINE__,ret);

		//Dynamic adjustment rhs1 must meet the following conditions
		rhs1_change_limit = rhs1_old + 2 * BRL - fsc + 4;
		rhs1_change_limit = (rhs1_change_limit < 7) ?  7 : rhs1_change_limit;
		if (rhs1_max < rhs1_change_limit)
			dev_err(&client->dev,
				"The total exposure limit makes rhs1 max is %d,but old rhs1 limit makes rhs1 min is %d\n",
				rhs1_max, rhs1_change_limit);
		if (rhs1 < rhs1_change_limit)
			rhs1 = rhs1_change_limit;

		dev_dbg(&client->dev,
			"line(%d) rhs1 %d,short time %d rhs1_old %d test %d\n",
			__LINE__, rhs1, s_exp_time, rhs1_old,
			(rhs1_old + 2 * BRL - fsc + 4));

		rhs1 = (rhs1 - 1) / 2 * 2 + 1;
		rhs1_old = rhs1;

		if (rhs1 - s_exp_time <= SHR1_MIN_X2) {
			shr1 = SHR1_MIN_X2;
			s_exp_time = rhs1 - shr1;
		} else {
			shr1 = rhs1 - s_exp_time;
		}

		if (shr1 < 5)
			shr1 = 5;
		else if (shr1 > (rhs1 - 2))
			shr1 = rhs1 - 2;

		if (shr0 < (rhs1 + 5))
			shr0 = rhs1 + 5;
		else if (shr0 > (fsc - 2))
			shr0 = fsc - 2;

		dev_dbg(&client->dev,
			"fsc=%d,RHS1_MAX=%d,SHR1_MIN=%d,rhs1_max=%d\n",
			fsc, RHS1_MAX_X2, SHR1_MIN_X2, rhs1_max);
		dev_dbg(&client->dev,
			"l_exp_time=%d,s_exp_time=%d,shr0=%d,shr1=%d,rhs1=%d,l_a_gain=%d,s_a_gain=%d\n",
			l_exp_time, s_exp_time, shr0, shr1, rhs1, l_a_gain, s_a_gain);
		//time effect n+2
		ret |= imx678_write_reg(client,
			IMX678_RHS1_REG_L,
			IMX678_REG_VALUE_08BIT,
			IMX678_FETCH_RHS1_L(rhs1));
		ret |= imx678_write_reg(client,
			IMX678_RHS1_REG_M,
			IMX678_REG_VALUE_08BIT,
			IMX678_FETCH_RHS1_M(rhs1));
		ret |= imx678_write_reg(client,
			IMX678_RHS1_REG_H,
			IMX678_REG_VALUE_08BIT,
			IMX678_FETCH_RHS1_H(rhs1));

		ret |= imx678_write_reg(client,
			IMX678_SF1_EXPO_REG_L,
			IMX678_REG_VALUE_08BIT,
			IMX678_FETCH_EXP_L(shr1));
		ret |= imx678_write_reg(client,
			IMX678_SF1_EXPO_REG_M,
			IMX678_REG_VALUE_08BIT,
			IMX678_FETCH_EXP_M(shr1));
		ret |= imx678_write_reg(client,
			IMX678_SF1_EXPO_REG_H,
			IMX678_REG_VALUE_08BIT,
			IMX678_FETCH_EXP_H(shr1));
		ret |= imx678_write_reg(client,
			IMX678_LF_EXPO_REG_L,
			IMX678_REG_VALUE_08BIT,
			IMX678_FETCH_EXP_L(shr0));
		ret |= imx678_write_reg(client,
			IMX678_LF_EXPO_REG_M,
			IMX678_REG_VALUE_08BIT,
			IMX678_FETCH_EXP_M(shr0));
		ret |= imx678_write_reg(client,
			IMX678_LF_EXPO_REG_H,
			IMX678_REG_VALUE_08BIT,
			IMX678_FETCH_EXP_H(shr0));
		ret |= imx678_write_reg(client,
			IMX678_GROUP_HOLD_REG,
			IMX678_REG_VALUE_08BIT,
			IMX678_GROUP_HOLD_END);
	}

	return ret;
}

static int imx678_set_hdrae_3frame(struct imx678 *imx678,
				   struct preisp_hdrae_exp_s *ae)
{
	struct i2c_client *client = imx678->client;
	u32 l_exp_time, m_exp_time, s_exp_time;
	u32 l_a_gain, m_a_gain, s_a_gain;
	int shr2, shr1, shr0, rhs2, rhs1 = 0;
	int rhs1_change_limit, rhs2_change_limit = 0;
	static int rhs1_old = IMX678_RHS1_X3_DEFAULT;
	static int rhs2_old = IMX678_RHS2_X3_DEFAULT;
	int ret = 0;
	u32 gain_switch = 0;
	u8 cg_mode = 0;
	u32 fsc;
	int rhs1_max = 0;
	int shr2_min = 0;

	if (!imx678->has_init_exp && !imx678->streaming) {
		imx678->init_hdrae_exp = *ae;
		imx678->has_init_exp = true;
		dev_dbg(&imx678->client->dev, "IMX678 is not streaming, save hdr ae!\n");
		return ret;
	}
	l_exp_time = ae->long_exp_reg;
	m_exp_time = ae->middle_exp_reg;
	s_exp_time = ae->short_exp_reg;
	l_a_gain = ae->long_gain_reg;
	m_a_gain = ae->middle_gain_reg;
	s_a_gain = ae->short_gain_reg;

	if (imx678->cur_mode->hdr_mode == HDR_X3) {
		//3 stagger
		cg_mode = ae->long_cg_mode;
	}
	if (!g_isHCG && cg_mode == GAIN_MODE_HCG) {
		gain_switch = 0x01 | 0x100;
		g_isHCG = true;
	} else if (g_isHCG && cg_mode == GAIN_MODE_LCG) {
		gain_switch = 0x00 | 0x100;
		g_isHCG = false;
	}

	dev_dbg(&client->dev,
		"rev exp req: L_exp: 0x%x, 0x%x, M_exp: 0x%x, 0x%x S_exp: 0x%x, 0x%x\n",
		l_exp_time, l_a_gain, m_exp_time, m_a_gain, s_exp_time, s_a_gain);

	ret = imx678_write_reg(client, IMX678_GROUP_HOLD_REG,
		IMX678_REG_VALUE_08BIT, IMX678_GROUP_HOLD_START);
	/* gain effect n+1 */
	ret |= imx678_write_reg(client, IMX678_LF_GAIN_REG_H,
		IMX678_REG_VALUE_08BIT, IMX678_FETCH_GAIN_H(l_a_gain));
	ret |= imx678_write_reg(client, IMX678_LF_GAIN_REG_L,
		IMX678_REG_VALUE_08BIT, IMX678_FETCH_GAIN_L(l_a_gain));
	ret |= imx678_write_reg(client, IMX678_SF1_GAIN_REG_H,
		IMX678_REG_VALUE_08BIT, IMX678_FETCH_GAIN_H(m_a_gain));
	ret |= imx678_write_reg(client, IMX678_SF1_GAIN_REG_L,
		IMX678_REG_VALUE_08BIT, IMX678_FETCH_GAIN_L(m_a_gain));
	ret |= imx678_write_reg(client, IMX678_SF2_GAIN_REG_H,
		IMX678_REG_VALUE_08BIT, IMX678_FETCH_GAIN_H(s_a_gain));
	ret |= imx678_write_reg(client, IMX678_SF2_GAIN_REG_L,
		IMX678_REG_VALUE_08BIT, IMX678_FETCH_GAIN_L(s_a_gain));
	if (gain_switch & 0x100)
		ret |= imx678_write_reg(client,
			IMX678_GAIN_SWITCH_REG,
			IMX678_REG_VALUE_08BIT,
			gain_switch & 0xff);

	/* Restrictions
	 *   FSC = 2 * VMAX and FSC should be 3n;
	 *   exp_l = FSC - SHR0;
	 *
	 *   SHR0 = FSC - exp_l;
	 *   RHS2 + 7 <= SHR0 <= (FSC -3);
	 *   SHR0 should be 3n;
	 *
	 *   exp_m = RHS1 - SHR1;
	 *   7 <= SHR1 <= RHS1 - 3;	 
	 *   RHS1 < BRL * 3;
	 *   SHR1 + 3 <= RHS1 <= SHR2 - 7;
	 *   RHS1(n+1) >= RHS1(n) + BRL * 3 -FSC + 3;
	 *
	 *   SHR1 should be 3n+1 and RHS1 should be 3n+1;
	 *
	 *   exp_s = RHS2 - SHR2;
	 *
	 *   RHS2 < RHS1 + BRL * 3;
	 *   SHR2 + 3 <= RHS2 <= SHR0 - 7;
	 *   RHS1 + 7 <= SHR2 <= RHS2 - 3;
	 *   RHS1(n+1) >= RHS1(n) + BRL * 3 -FSC + 3;
	 *
	 *   SHR2 should be 3n+2 and RHS2 should be 3n+2;
	 */

	/* The HDR mode vts is double by default to workaround T-line */
	fsc = (((imx678->cur_vts) * 3) / 4) / 6 * 6;
	shr0 = fsc - l_exp_time;
	dev_dbg(&client->dev,
		"line(%d) shr0 %d, l_exp_time %d, fsc %d\n",
		__LINE__, shr0, l_exp_time, fsc);

	rhs1 = (SHR1_MIN_X3 + m_exp_time - 1) / 3 * 3 + 1;
	rhs1_max = RHS1_MAX_X3;
	if (rhs1 < SHR1_MIN_X3 + 3)
		rhs1 = SHR1_MIN_X3 + 3;
	else if (rhs1 > rhs1_max)
		rhs1 = rhs1_max;

	dev_dbg(&client->dev,
		"line(%d) rhs1 %d, m_exp_time %d rhs1_old %d\n",
		__LINE__, rhs1, m_exp_time, rhs1_old);

	//Dynamic adjustment rhs2 must meet the following conditions
	rhs1_change_limit = rhs1_old + 3 * BRL - fsc + 6;
	rhs1_change_limit = (rhs1_change_limit < 10) ? 10 : rhs1_change_limit;
	rhs1_change_limit = (rhs1_change_limit - 1) / 3 * 3 + 1;

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

	shr2_min = rhs1 + 7;
	rhs2 =  (shr2_min + s_exp_time - 1) / 3 * 3 + 2;
	if (rhs2 > (shr0 - 7))
		rhs2 = shr0 - 7;
	else if (rhs2 < 20) //10 + 7  + 3
		rhs2 = 20;

	dev_dbg(&client->dev,
		"line(%d) rhs2 %d, s_exp_time %d, rhs2_old %d\n",
		__LINE__, rhs2, s_exp_time, rhs2_old);

	//Dynamic adjustment rhs2 must meet the following conditions
	rhs2_change_limit = rhs2_old + 3 * BRL - fsc + 6;
	rhs2_change_limit = (rhs2_change_limit < 20) ?  20 : rhs2_change_limit;
	rhs2_change_limit = (rhs2_change_limit - 1) / 3 * 3 + 2;

	if ((shr0 - 7) < rhs2_change_limit) {
		dev_err(&client->dev,
			"The total exposure limit makes rhs2 max is %d,but old rhs1 limit makes rhs2 min is %d\n",
			shr0 - 7, rhs2_change_limit);
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

	if (shr0 < rhs2 + 7)
		shr0 = rhs2 + 7;
	else if (shr0 > fsc - 3)
		shr0 = fsc - 3;

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
	ret |= imx678_write_reg(client,
		IMX678_RHS2_REG_L,
		IMX678_REG_VALUE_08BIT,
		IMX678_FETCH_RHS1_L(rhs2));
	ret |= imx678_write_reg(client,
		IMX678_RHS2_REG_M,
		IMX678_REG_VALUE_08BIT,
		IMX678_FETCH_RHS1_M(rhs2));
	ret |= imx678_write_reg(client,
		IMX678_RHS2_REG_H,
		IMX678_REG_VALUE_08BIT,
		IMX678_FETCH_RHS1_H(rhs2));
	/* write SEF2 exposure SHR2 regs*/
	ret |= imx678_write_reg(client,
		IMX678_SF2_EXPO_REG_L,
		IMX678_REG_VALUE_08BIT,
		IMX678_FETCH_EXP_L(shr2));
	ret |= imx678_write_reg(client,
		IMX678_SF2_EXPO_REG_M,
		IMX678_REG_VALUE_08BIT,
		IMX678_FETCH_EXP_M(shr2));
	ret |= imx678_write_reg(client,
		IMX678_SF2_EXPO_REG_H,
		IMX678_REG_VALUE_08BIT,
		IMX678_FETCH_EXP_H(shr2));
	/* write SEF1 exposure RHS1 regs*/
	ret |= imx678_write_reg(client,
		IMX678_RHS1_REG_L,
		IMX678_REG_VALUE_08BIT,
		IMX678_FETCH_RHS1_L(rhs1));
	ret |= imx678_write_reg(client,
		IMX678_RHS1_REG_M,
		IMX678_REG_VALUE_08BIT,
		IMX678_FETCH_RHS1_M(rhs1));
	ret |= imx678_write_reg(client,
		IMX678_RHS1_REG_H,
		IMX678_REG_VALUE_08BIT,
		IMX678_FETCH_RHS1_H(rhs1));
	/* write SEF1 exposure SHR1 regs*/
	ret |= imx678_write_reg(client,
		IMX678_SF1_EXPO_REG_L,
		IMX678_REG_VALUE_08BIT,
		IMX678_FETCH_EXP_L(shr1));
	ret |= imx678_write_reg(client,
		IMX678_SF1_EXPO_REG_M,
		IMX678_REG_VALUE_08BIT,
		IMX678_FETCH_EXP_M(shr1));
	ret |= imx678_write_reg(client,
		IMX678_SF1_EXPO_REG_H,
		IMX678_REG_VALUE_08BIT,
		IMX678_FETCH_EXP_H(shr1));
	/* write LF exposure SHR0 regs*/
	ret |= imx678_write_reg(client,
		IMX678_LF_EXPO_REG_L,
		IMX678_REG_VALUE_08BIT,
		IMX678_FETCH_EXP_L(shr0));
	ret |= imx678_write_reg(client,
		IMX678_LF_EXPO_REG_M,
		IMX678_REG_VALUE_08BIT,
		IMX678_FETCH_EXP_M(shr0));
	ret |= imx678_write_reg(client,
		IMX678_LF_EXPO_REG_H,
		IMX678_REG_VALUE_08BIT,
		IMX678_FETCH_EXP_H(shr0));

	ret |= imx678_write_reg(client, IMX678_GROUP_HOLD_REG,
		IMX678_REG_VALUE_08BIT, IMX678_GROUP_HOLD_END);
	return ret;
}

static int imx678_set_conversion_gain(struct imx678 *imx678, u32 *cg)
{
	int ret = 0;
	struct i2c_client *client = imx678->client;
	int cur_cg = *cg;
	u32 gain_switch = 0;

	if (g_isHCG && cur_cg == GAIN_MODE_LCG) {
		gain_switch = 0x00 | 0x100;
		g_isHCG = false;
	} else if (!g_isHCG && cur_cg == GAIN_MODE_HCG) {
		gain_switch = 0x01 | 0x100;
		g_isHCG = true;
	}
	ret = imx678_write_reg(client,
			IMX678_GROUP_HOLD_REG,
			IMX678_REG_VALUE_08BIT,
			IMX678_GROUP_HOLD_START);
	if (gain_switch & 0x100)
		ret |= imx678_write_reg(client,
			IMX678_GAIN_SWITCH_REG,
			IMX678_REG_VALUE_08BIT,
			gain_switch & 0xff);
	ret |= imx678_write_reg(client,
			IMX678_GROUP_HOLD_REG,
			IMX678_REG_VALUE_08BIT,
			IMX678_GROUP_HOLD_END);
	return ret;
}

#ifdef USED_SYS_DEBUG
//ag: echo 0 >  /sys/devices/platform/ff510000.i2c/i2c-1/1-0037/cam_s_cg
static ssize_t set_conversion_gain_status(struct device *dev,
	struct device_attribute *attr,
	const char *buf,
	size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct imx678 *imx678 = to_imx678(sd);
	int status = 0;
	int ret = 0;

	ret = kstrtoint(buf, 0, &status);
	if (!ret && status >= 0 && status < 2)
		imx678_set_conversion_gain(imx678, &status);
	else
		dev_err(dev, "input 0 for LCG, 1 for HCG, cur %d\n", status);
	return count;
}

static struct device_attribute attributes[] = {
	__ATTR(cam_s_cg, S_IWUSR, NULL, set_conversion_gain_status),
};

static int add_sysfs_interfaces(struct device *dev)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(attributes); i++)
		if (device_create_file(dev, attributes + i))
			goto undo;
	return 0;
undo:
	for (i--; i >= 0 ; i--)
		device_remove_file(dev, attributes + i);
	dev_err(dev, "%s: failed to create sysfs interface\n", __func__);
	return -ENODEV;
}

static int remove_sysfs_interfaces(struct device *dev)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(attributes); i++)
		device_remove_file(dev, attributes + i);
	return 0;
}
#endif


static long imx678_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	struct imx678 *imx678 = to_imx678(sd);
	struct rkmodule_hdr_cfg *hdr;
	u32 i, h, w, stream;
	long ret = 0;
	const struct imx678_mode *mode;
	u64 pixel_rate = 0;

	switch (cmd) {
	case PREISP_CMD_SET_HDRAE_EXP:
		if (imx678->cur_mode->hdr_mode == HDR_X2)
			ret = imx678_set_hdrae(imx678, arg);
		else if (imx678->cur_mode->hdr_mode == HDR_X3)
			ret = imx678_set_hdrae_3frame(imx678, arg);
		break;
	case RKMODULE_GET_MODULE_INFO:
		imx678_get_module_inf(imx678, (struct rkmodule_inf *)arg);
		break;
	case RKMODULE_GET_HDR_CFG:
		hdr = (struct rkmodule_hdr_cfg *)arg;
		hdr->esp.mode = HDR_NORMAL_VC;
		hdr->hdr_mode = imx678->cur_mode->hdr_mode;
		break;
	case RKMODULE_SET_HDR_CFG:
		hdr = (struct rkmodule_hdr_cfg *)arg;
		if(imx678->lane_num == 4) {
			w = imx678->cur_mode->width;
			h = imx678->cur_mode->height;
			for (i = 0; i < imx678->cfg_num; i++) {
				if (w == imx678_supported_modes_4lane[i].width &&
				    h == imx678_supported_modes_4lane[i].height &&
				    imx678_supported_modes_4lane[i].hdr_mode == hdr->hdr_mode) {
					imx678_change_mode(imx678, &imx678_supported_modes_4lane[i]);
					break;
				}
			}
		}
		else {
			w = imx678->cur_mode->width;
			h = imx678->cur_mode->height;
			for (i = 0; i < imx678->cfg_num; i++) {
				if (w == imx678_supported_modes_2lane[i].width &&
					h == imx678_supported_modes_2lane[i].height &&
					imx678_supported_modes_2lane[i].hdr_mode == hdr->hdr_mode) {
					imx678_change_mode(imx678, &imx678_supported_modes_2lane[i]);
					break;
				}
			}
		}

		if (i == imx678->cfg_num) {
			dev_err(&imx678->client->dev,
				"not find hdr mode:%d %dx%d config\n",
				hdr->hdr_mode, w, h);
			ret = -EINVAL;
		} else {
			mode = imx678->cur_mode;
			if (imx678->streaming) {
				ret = imx678_write_reg(imx678->client, IMX678_GROUP_HOLD_REG,
					IMX678_REG_VALUE_08BIT, IMX678_GROUP_HOLD_START);

				ret |= imx678_write_array(imx678->client, mode->global_reg_list);
				ret |= imx678_write_array(imx678->client, mode->reg_list);

				ret |= imx678_write_reg(imx678->client, IMX678_GROUP_HOLD_REG,
					IMX678_REG_VALUE_08BIT, IMX678_GROUP_HOLD_END);
				if (ret)
					return ret;
			}
			w = mode->hts_def - mode->width;
			h = mode->vts_def - mode->height;
			__v4l2_ctrl_modify_range(imx678->hblank, w, w, 1, w);
			__v4l2_ctrl_modify_range(imx678->vblank, h,
				IMX678_VTS_MAX - mode->height,
				1, h);
			__v4l2_ctrl_s_ctrl(imx678->link_freq, mode->mipi_freq_idx);
			pixel_rate = (u32)link_freq_items[mode->mipi_freq_idx] / mode->bpp * 2 * imx678->lane_num;
			__v4l2_ctrl_s_ctrl_int64(imx678->pixel_rate,
						 pixel_rate);
		}
		break;
	case RKMODULE_SET_CONVERSION_GAIN:
		ret = imx678_set_conversion_gain(imx678, (u32 *)arg);
		break;
	case RKMODULE_SET_QUICK_STREAM:

		stream = *((u32 *)arg);

		if (stream)
			ret = imx678_write_reg(imx678->client, IMX678_REG_CTRL_MODE,
				IMX678_REG_VALUE_08BIT, IMX678_MODE_STREAMING);
		else
			ret = imx678_write_reg(imx678->client, IMX678_REG_CTRL_MODE,
				IMX678_REG_VALUE_08BIT, IMX678_MODE_SW_STANDBY);
		break;
	case RKMODULE_GET_SONY_BRL:
		if (imx678->cur_mode->width == 3856 && imx678->cur_mode->height == 2180)
			*((u32 *)arg) = BRL;
		else
			*((u32 *)arg) = BRL_BINNING;
		break;
	case RKMODULE_SET_DPCC_CFG:
	case RKMODULE_GET_NR_SWITCH_THRESHOLD:
		break;

	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static long imx678_compat_ioctl32(struct v4l2_subdev *sd,
				  unsigned int cmd, unsigned long arg)
{
	void __user *up = compat_ptr(arg);
	struct rkmodule_inf *inf;
	struct rkmodule_awb_cfg *cfg;
	struct rkmodule_hdr_cfg *hdr;
	struct preisp_hdrae_exp_s *hdrae;
	long ret;
	u32 cg = 0,brl = 0;
	u32  stream;

	switch (cmd) {
	case RKMODULE_GET_MODULE_INFO:
		inf = kzalloc(sizeof(*inf), GFP_KERNEL);
		if (!inf) {
			ret = -ENOMEM;
			return ret;
		}

		ret = imx678_ioctl(sd, cmd, inf);
		if (!ret)
			ret = copy_to_user(up, inf, sizeof(*inf));
		kfree(inf);
		break;
	case RKMODULE_AWB_CFG:
		cfg = kzalloc(sizeof(*cfg), GFP_KERNEL);
		if (!cfg) {
			ret = -ENOMEM;
			return ret;
		}

		ret = copy_from_user(cfg, up, sizeof(*cfg));
		if (!ret)
			ret = imx678_ioctl(sd, cmd, cfg);
		kfree(cfg);
		break;
	case RKMODULE_GET_HDR_CFG:
		hdr = kzalloc(sizeof(*hdr), GFP_KERNEL);
		if (!hdr) {
			ret = -ENOMEM;
			return ret;
		}

		ret = imx678_ioctl(sd, cmd, hdr);
		if (!ret)
			ret = copy_to_user(up, hdr, sizeof(*hdr));
		kfree(hdr);
		break;
	case RKMODULE_SET_HDR_CFG:
		hdr = kzalloc(sizeof(*hdr), GFP_KERNEL);
		if (!hdr) {
			ret = -ENOMEM;
			return ret;
		}

		ret = copy_from_user(hdr, up, sizeof(*hdr));
		if (!ret)
			ret = imx678_ioctl(sd, cmd, hdr);
		kfree(hdr);
		break;
	case PREISP_CMD_SET_HDRAE_EXP:
		hdrae = kzalloc(sizeof(*hdrae), GFP_KERNEL);
		if (!hdrae) {
			ret = -ENOMEM;
			return ret;
		}

		ret = copy_from_user(hdrae, up, sizeof(*hdrae));
		if (!ret)
			ret = imx678_ioctl(sd, cmd, hdrae);
		kfree(hdrae);
		break;
	case RKMODULE_SET_CONVERSION_GAIN:
		ret = copy_from_user(&cg, up, sizeof(cg));
		if (!ret)
			ret = imx678_ioctl(sd, cmd, &cg);
		break;
	case RKMODULE_SET_QUICK_STREAM:

		ret = copy_from_user(&stream, up, sizeof(u32));
		if (!ret)
			ret = imx678_ioctl(sd, cmd, &stream);
		break;
	case RKMODULE_GET_SONY_BRL:
		ret = imx678_ioctl(sd, cmd, &brl);
		if (!ret) {
			if (copy_to_user(up, &brl, sizeof(u32)))
				return -EFAULT;
		}
		break;

	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}
#endif

static int imx678_init_conversion_gain(struct imx678 *imx678)
{
	int ret = 0;
	struct i2c_client *client = imx678->client;

	ret = imx678_write_reg(client,
		IMX678_GAIN_SWITCH_REG,
		IMX678_REG_VALUE_08BIT,
		0X00);
	if (!ret)
		g_isHCG = false;
	return ret;
}


static int __imx678_start_stream(struct imx678 *imx678)
{
	int ret;

	ret = imx678_write_array(imx678->client, imx678->cur_mode->global_reg_list);
	if (ret)
		return ret;

	ret = imx678_write_array(imx678->client, imx678->cur_mode->reg_list);
	if (ret)
		return ret;

	ret = imx678_init_conversion_gain(imx678);
	if (ret)
		return ret;

	/* In case these controls are set before streaming */
	ret = __v4l2_ctrl_handler_setup(&imx678->ctrl_handler);
	if (ret)
		return ret;
	if (imx678->has_init_exp && imx678->cur_mode->hdr_mode != NO_HDR) {
		ret = imx678_ioctl(&imx678->subdev, PREISP_CMD_SET_HDRAE_EXP,
			&imx678->init_hdrae_exp);
		if (ret) {
			dev_err(&imx678->client->dev,
				"init exp fail in hdr mode\n");
			return ret;
		}
	}
	return imx678_write_reg(imx678->client, IMX678_REG_CTRL_MODE,
				IMX678_REG_VALUE_08BIT, IMX678_MODE_STREAMING);
}

static int __imx678_stop_stream(struct imx678 *imx678)
{
	imx678->has_init_exp = false;
	return imx678_write_reg(imx678->client, IMX678_REG_CTRL_MODE,
				IMX678_REG_VALUE_08BIT, IMX678_MODE_SW_STANDBY);
}

static int imx678_s_stream(struct v4l2_subdev *sd, int on)
{
	struct imx678 *imx678 = to_imx678(sd);
	struct i2c_client *client = imx678->client;
	int ret = 0;

	dev_dbg(&imx678->client->dev, "s_stream: %d. %dx%d, hdr: %d, bpp: %d\n",
		on, imx678->cur_mode->width, imx678->cur_mode->height,
		imx678->cur_mode->hdr_mode, imx678->cur_mode->bpp);

	mutex_lock(&imx678->mutex);
	on = !!on;
	if (on == imx678->streaming)
		goto unlock_and_return;

	if (on) {
		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}

		ret = __imx678_start_stream(imx678);
		if (ret) {
			v4l2_err(sd, "start stream failed while write regs\n");
			pm_runtime_put(&client->dev);
			goto unlock_and_return;
		}
	} else {
		__imx678_stop_stream(imx678);
		pm_runtime_put(&client->dev);
	}

	imx678->streaming = on;

unlock_and_return:
	mutex_unlock(&imx678->mutex);

	return ret;
}

static int imx678_s_power(struct v4l2_subdev *sd, int on)
{
	struct imx678 *imx678 = to_imx678(sd);
	struct i2c_client *client = imx678->client;
	int ret = 0;

	mutex_lock(&imx678->mutex);

	if (imx678->power_on == !!on)
		goto unlock_and_return;

	if (on) {
		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}
		imx678->power_on = true;
	} else {
		pm_runtime_put(&client->dev);
		imx678->power_on = false;
	}

unlock_and_return:
	mutex_unlock(&imx678->mutex);

	return ret;
}

static int __imx678_power_on(struct imx678 *imx678)
{
	int ret;
	struct device *dev = &imx678->client->dev;

	if (!IS_ERR_OR_NULL(imx678->pins_default)) {
		ret = pinctrl_select_state(imx678->pinctrl,
					   imx678->pins_default);
		if (ret < 0)
			dev_err(dev, "could not set pins\n");
	}

	ret = regulator_bulk_enable(IMX678_NUM_SUPPLIES, imx678->supplies);
	if (ret < 0) {
		dev_err(dev, "Failed to enable regulators\n");
		goto err_pinctrl;
	}
	if (!IS_ERR(imx678->pwdn_gpio))
		gpiod_direction_output(imx678->pwdn_gpio, 0);
	/* At least 500ns between power raising and XCLR */
	/* fix power on timing if insmod this ko */
	usleep_range(10 * 1000, 20 * 1000);
	if (!IS_ERR(imx678->reset_gpio))
		gpiod_direction_output(imx678->reset_gpio, 0);

	/* At least 1us between XCLR and clk */
	/* fix power on timing if insmod this ko */
	usleep_range(10 * 1000, 20 * 1000);

	ret = clk_set_rate(imx678->xvclk, IMX678_XVCLK_FREQ);
	if (ret < 0)
		dev_warn(dev, "Failed to set xvclk rate\n");
	if (clk_get_rate(imx678->xvclk) != IMX678_XVCLK_FREQ)
		dev_warn(dev, "xvclk mismatched\n");

	ret = clk_prepare_enable(imx678->xvclk);
	dev_info(dev, "clk_prepare_enable:%d\n",ret);
	if (ret < 0) {
		dev_err(dev, "Failed to enable xvclk\n");
		goto err_clk;
	}

	/* At least 20us between XCLR and I2C communication */
	usleep_range(20*1000, 30*1000);

	return 0;

err_clk:
	if (!IS_ERR(imx678->reset_gpio))
		gpiod_direction_output(imx678->reset_gpio, 1);
	regulator_bulk_disable(IMX678_NUM_SUPPLIES, imx678->supplies);

err_pinctrl:
	if (!IS_ERR_OR_NULL(imx678->pins_sleep))
		pinctrl_select_state(imx678->pinctrl, imx678->pins_sleep);

	return ret;
}


static void __imx678_power_off(struct imx678 *imx678)
{
	int ret;
	struct device *dev = &imx678->client->dev;

	if (!IS_ERR(imx678->reset_gpio))
		gpiod_direction_output(imx678->reset_gpio, 1);
	clk_disable_unprepare(imx678->xvclk);
	if (!IS_ERR_OR_NULL(imx678->pins_sleep)) {
		ret = pinctrl_select_state(imx678->pinctrl,
					   imx678->pins_sleep);
		if (ret < 0)
			dev_dbg(dev, "could not set pins\n");
	}
	if (!IS_ERR(imx678->pwdn_gpio))
		gpiod_direction_output(imx678->pwdn_gpio, 1);
	regulator_bulk_disable(IMX678_NUM_SUPPLIES, imx678->supplies);
}

static int imx678_runtime_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct imx678 *imx678 = to_imx678(sd);

	return __imx678_power_on(imx678);
}

static int imx678_runtime_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct imx678 *imx678 = to_imx678(sd);

	__imx678_power_off(imx678);

	return 0;
}

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static int imx678_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct imx678 *imx678 = to_imx678(sd);
	struct v4l2_mbus_framefmt *try_fmt =
				v4l2_subdev_get_try_format(sd, fh->pad, 0);
	const struct imx678_mode *def_mode;

	mutex_lock(&imx678->mutex);
	/* Initialize try_fmt */
	if(imx678->lane_num == 4)
		def_mode = &imx678_supported_modes_4lane[0];
	else
		def_mode = &imx678_supported_modes_2lane[0];

	try_fmt->width = def_mode->width;
	try_fmt->height = def_mode->height;
	try_fmt->code = def_mode->bus_fmt;
	try_fmt->field = V4L2_FIELD_NONE;

	mutex_unlock(&imx678->mutex);
	/* No crop or compose */

	return 0;
}

#endif

static int imx678_enum_frame_interval(struct v4l2_subdev *sd,
	struct v4l2_subdev_pad_config *cfg,
	struct v4l2_subdev_frame_interval_enum *fie)
{
	struct imx678 *imx678 = to_imx678(sd);

	if (fie->index >= imx678->cfg_num)
		return -EINVAL;

	if(imx678->lane_num == 4) {
		fie->code = imx678_supported_modes_4lane[fie->index].bus_fmt;
		fie->width = imx678_supported_modes_4lane[fie->index].width;
		fie->height = imx678_supported_modes_4lane[fie->index].height;
		fie->interval = imx678_supported_modes_4lane[fie->index].max_fps;
		fie->reserved[0] = imx678_supported_modes_4lane[fie->index].hdr_mode;
	}
	else {
		fie->code = imx678_supported_modes_2lane[fie->index].bus_fmt;
		fie->width = imx678_supported_modes_2lane[fie->index].width;
		fie->height = imx678_supported_modes_2lane[fie->index].height;
		fie->interval = imx678_supported_modes_2lane[fie->index].max_fps;
		fie->reserved[0] = imx678_supported_modes_2lane[fie->index].hdr_mode;
	}
	
	return 0;
}

#define CROP_START(SRC, DST) (((SRC) - (DST)) / 2 / 4 * 4)
/*
 * The resolution of the driver configuration needs to be exactly
 * the same as the current output resolution of the sensor,
 * the input width of the isp needs to be 16 aligned,
 * the input height of the isp needs to be 8 aligned.
 * Can be cropped to standard resolution by this function,
 * otherwise it will crop out strange resolution according
 * to the alignment rules.
 */
static int imx678_get_selection(struct v4l2_subdev *sd,
				struct v4l2_subdev_pad_config *cfg,
				struct v4l2_subdev_selection *sel)
{
	struct imx678 *imx678 = to_imx678(sd);

	if (sel->target == V4L2_SEL_TGT_CROP_BOUNDS) {
		sel->r.left = CROP_START(imx678->cur_mode->width, imx678->cur_mode->real_width);
		sel->r.width = imx678->cur_mode->real_width;
		sel->r.top = CROP_START(imx678->cur_mode->height, imx678->cur_mode->real_height);
		sel->r.height = imx678->cur_mode->real_height;
		
		return 0;
	}
	return -EINVAL;
}

static const struct dev_pm_ops imx678_pm_ops = {
	SET_RUNTIME_PM_OPS(imx678_runtime_suspend,
			   imx678_runtime_resume, NULL)
};

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static const struct v4l2_subdev_internal_ops imx678_internal_ops = {
	.open = imx678_open,
};
#endif

static const struct v4l2_subdev_core_ops imx678_core_ops = {
	.s_power = imx678_s_power,
	.ioctl = imx678_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = imx678_compat_ioctl32,
#endif
};

static const struct v4l2_subdev_video_ops imx678_video_ops = {
	.s_stream = imx678_s_stream,
	.g_frame_interval = imx678_g_frame_interval,
	
};

static const struct v4l2_subdev_pad_ops imx678_pad_ops = {
	.enum_mbus_code = imx678_enum_mbus_code,
	.enum_frame_size = imx678_enum_frame_sizes,
	.enum_frame_interval = imx678_enum_frame_interval,
	.get_fmt = imx678_get_fmt,
	.set_fmt = imx678_set_fmt,
	.get_selection = imx678_get_selection,
	.get_mbus_config = imx678_g_mbus_config,
};

static const struct v4l2_subdev_ops imx678_subdev_ops = {
	.core	= &imx678_core_ops,
	.video	= &imx678_video_ops,
	.pad	= &imx678_pad_ops,
};

static int imx678_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct imx678 *imx678 = container_of(ctrl->handler,
					     struct imx678, ctrl_handler);
	struct i2c_client *client = imx678->client;
	s64 max;
	u32 vts = 0,val;
	int ret = 0;
	u32 shr0 = 0;

	/* Propagate change of current control to all related controls */
	switch (ctrl->id) {
	case V4L2_CID_VBLANK:
		if (imx678->cur_mode->hdr_mode == NO_HDR) {
			/* Update max exposure while meeting expected vblanking */
			max = imx678->cur_mode->height + ctrl->val - IMX678_MAXFAC_NUM;
			__v4l2_ctrl_modify_range(imx678->exposure,
					 imx678->exposure->minimum, max,
					 imx678->exposure->step,
					 imx678->exposure->default_value);
		}
		break;
	}

	if (!pm_runtime_get_if_in_use(&client->dev))
		return 0;

	switch (ctrl->id) {
	case V4L2_CID_EXPOSURE:
		if (imx678->cur_mode->hdr_mode == NO_HDR) {
			shr0 = imx678->cur_vts - ctrl->val;
			ret = imx678_write_reg(imx678->client, IMX678_LF_EXPO_REG_L,
					       IMX678_REG_VALUE_08BIT,
					       IMX678_FETCH_EXP_L(shr0));
			ret |= imx678_write_reg(imx678->client, IMX678_LF_EXPO_REG_M,
					       IMX678_REG_VALUE_08BIT,
					       IMX678_FETCH_EXP_M(shr0));
			ret |= imx678_write_reg(imx678->client, IMX678_LF_EXPO_REG_H,
					       IMX678_REG_VALUE_08BIT,
					       IMX678_FETCH_EXP_H(shr0));
			dev_dbg(&client->dev, "set exposure(shr0) %d = cur_vts(%d) - val(%d)\n",
				shr0, imx678->cur_vts, ctrl->val);
		}
		break;
	case V4L2_CID_ANALOGUE_GAIN:
		if (imx678->cur_mode->hdr_mode == NO_HDR) {
			ret = imx678_write_reg(imx678->client, IMX678_LF_GAIN_REG_H,
					       IMX678_REG_VALUE_08BIT,
					       IMX678_FETCH_GAIN_H(ctrl->val));
			ret |= imx678_write_reg(imx678->client, IMX678_LF_GAIN_REG_L,
					       IMX678_REG_VALUE_08BIT,
					       IMX678_FETCH_GAIN_L(ctrl->val));
			dev_dbg(&client->dev, "set analog gain 0x%x\n",
				ctrl->val);

		}
		break;
	case V4L2_CID_VBLANK:
		vts = ctrl->val + imx678->cur_mode->height;
		/*
		 * vts of hdr mode is double to correct T-line calculation.
		 * Restore before write to reg.
		 */
		if (imx678->cur_mode->hdr_mode == HDR_X2) {
			if(imx678->cur_mode->clear_hdr_mode == CLEAR_HDR) {    /*clear hdr*/
				//imx678->cur_vts = vts;
				vts = (vts + 1) / 2 * 2;
				imx678->cur_vts = vts;
				vts /= 2;
			} else {                                               /*dol hdr*/
				vts = (vts + 1) / 2 * 2;
				imx678->cur_vts = vts;
				vts /= 2;
			}
		} else if (imx678->cur_mode->hdr_mode == HDR_X3) {
			vts = (vts + 2) / 3 * 3;
			imx678->cur_vts = vts;
			vts /= 4;
		} else {
			imx678->cur_vts = vts;
		}
		ret = imx678_write_reg(imx678->client, IMX678_VTS_REG_L,
				       IMX678_REG_VALUE_08BIT,
				       IMX678_FETCH_VTS_L(vts));
		ret |= imx678_write_reg(imx678->client, IMX678_VTS_REG_M,
				       IMX678_REG_VALUE_08BIT,
				       IMX678_FETCH_VTS_M(vts));
		ret |= imx678_write_reg(imx678->client, IMX678_VTS_REG_H,
				       IMX678_REG_VALUE_08BIT,
				       IMX678_FETCH_VTS_H(vts));
		dev_dbg(&client->dev, "set vblank 0x%x vts %d\n",
			ctrl->val, vts);
		break;
	case V4L2_CID_HFLIP:
		ret = imx678_read_reg(imx678->client, IMX678_FLIP_REG,
				      IMX678_REG_VALUE_08BIT, &val);
		if (ret)
			break;
		if (ctrl->val)
			val |= IMX678_MIRROR_BIT_MASK;
		else
			val &= ~IMX678_MIRROR_BIT_MASK;
		ret = imx678_write_reg(imx678->client, IMX678_FLIP_REG,
				       IMX678_REG_VALUE_08BIT, val);
		break;
	case V4L2_CID_VFLIP:
		ret = imx678_read_reg(imx678->client, IMX678_FLIP_REG + 1,
				      IMX678_REG_VALUE_08BIT, &val);
		if (ret)
			break;
		if (ctrl->val)
			val |= IMX678_FLIP_BIT_MASK;
		else
			val &= ~IMX678_FLIP_BIT_MASK;
		ret = imx678_write_reg(imx678->client, IMX678_FLIP_REG + 1,
				       IMX678_REG_VALUE_08BIT, val);
		break;
	default:
		dev_warn(&client->dev, "%s Unhandled id:0x%x, val:0x%x\n",
			 __func__, ctrl->id, ctrl->val);
		break;
	}

	pm_runtime_put(&client->dev);

	return ret;
}

static const struct v4l2_ctrl_ops imx678_ctrl_ops = {
	.s_ctrl = imx678_set_ctrl,
};

static int imx678_initialize_controls(struct imx678 *imx678)
{
	const struct imx678_mode *mode;
	struct v4l2_ctrl_handler *handler;
	s64 exposure_max, vblank_def;
	u64 pixel_rate,pixel_rate_max;
	u32 h_blank;
	int ret;

	handler = &imx678->ctrl_handler;
	mode = imx678->cur_mode;
	ret = v4l2_ctrl_handler_init(handler, 8);
	if (ret)
		return ret;
	handler->lock = &imx678->mutex;

	imx678->link_freq = v4l2_ctrl_new_int_menu(handler, NULL,
				V4L2_CID_LINK_FREQ,
				ARRAY_SIZE(link_freq_items) - 1, 0,
				link_freq_items);
	__v4l2_ctrl_s_ctrl(imx678->link_freq, mode->mipi_freq_idx); 

	/* pixel rate = link frequency * 2 * lanes / BITS_PER_SAMPLE */
	pixel_rate = (u32)link_freq_items[mode->mipi_freq_idx] / mode->bpp * 2 * imx678->lane_num;
	pixel_rate_max = (u32)IMX678_MAX_FREQ / mode->bpp * 2 * imx678->lane_num;
	imx678->pixel_rate = v4l2_ctrl_new_std(handler, NULL,
		V4L2_CID_PIXEL_RATE, 0, pixel_rate_max,
		1, pixel_rate);

	h_blank = mode->hts_def - mode->width;
	imx678->hblank = v4l2_ctrl_new_std(handler, NULL, V4L2_CID_HBLANK,
				h_blank, h_blank, 1, h_blank);
	if (imx678->hblank)
		imx678->hblank->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	vblank_def = mode->vts_def - mode->height;
	imx678->vblank = v4l2_ctrl_new_std(handler, &imx678_ctrl_ops,
				V4L2_CID_VBLANK, vblank_def,
				IMX678_VTS_MAX - mode->height,
				1, vblank_def);
	imx678->cur_vts = mode->vts_def;

	exposure_max = mode->vts_def - IMX678_MAXFAC_NUM;
	imx678->exposure = v4l2_ctrl_new_std(handler, &imx678_ctrl_ops,
				V4L2_CID_EXPOSURE, IMX678_EXPOSURE_MIN,
				exposure_max, IMX678_EXPOSURE_STEP,
				mode->exp_def);

	imx678->anal_a_gain = v4l2_ctrl_new_std(handler, &imx678_ctrl_ops,
				V4L2_CID_ANALOGUE_GAIN, IMX678_GAIN_MIN,
				IMX678_GAIN_MAX, IMX678_GAIN_STEP,
				IMX678_GAIN_DEFAULT);

	v4l2_ctrl_new_std(handler, &imx678_ctrl_ops, V4L2_CID_HFLIP, 0, 1, 1, 0);
	v4l2_ctrl_new_std(handler, &imx678_ctrl_ops, V4L2_CID_VFLIP, 0, 1, 1, 0);

	if (handler->error) {
		ret = handler->error;
		dev_err(&imx678->client->dev,
			"Failed to init controls(%d)\n", ret);
		goto err_free_handler;
	}

	imx678->subdev.ctrl_handler = handler;
	imx678->has_init_exp = false;

	return 0;

err_free_handler:
	v4l2_ctrl_handler_free(handler);

	return ret;
}


static int imx678_check_sensor_id(struct imx678 *imx678,
				  struct i2c_client *client)
{
	struct device *dev = &imx678->client->dev;
	u32 reg = 0, pid =0, vid =0;
	int ret;

	ret = imx678_read_reg(client,  IMX678_REG_CHIP_ID, IMX678_REG_VALUE_08BIT, &pid);
	ret |= imx678_read_reg(client, IMX678_REG_CHIP_ID + 1, IMX678_REG_VALUE_08BIT, &vid);
	reg = (vid << 8) | pid;
	
	if (reg != CHIP_ID) {
		dev_err(dev, "Unexpected sensor id(%06x), ret(%d)\n", reg, ret);
		return -ENODEV;
	}
	dev_info(dev, "detected imx678 %04x sensor\n", reg);

	return 0;
}

static int imx678_configure_regulators(struct imx678 *imx678)
{
	unsigned int i;

	for (i = 0; i < IMX678_NUM_SUPPLIES; i++)
		imx678->supplies[i].supply = imx678_supply_names[i];

	return devm_regulator_bulk_get(&imx678->client->dev,
				       IMX678_NUM_SUPPLIES,
				       imx678->supplies);
}

static int imx678_parse_of(struct imx678 *imx678,u32 hdr_mode)
{
	struct device *dev = &imx678->client->dev;
	struct device_node *endpoint;
	struct fwnode_handle *fwnode;
	int rval, i = 0;

	endpoint = of_graph_get_next_endpoint(dev->of_node, NULL);
	if (!endpoint) {
		dev_err(dev, "Failed to get endpoint\n");
		return -EINVAL;
	}
	fwnode = of_fwnode_handle(endpoint);
	rval = fwnode_property_read_u32_array(fwnode, "data-lanes", NULL, 0);
	if (rval <= 0) {
		dev_warn(dev, " Get mipi lane num failed!\n");
		return -1;
	}

	imx678->lane_num = rval;
	if (4 == imx678->lane_num) {
		imx678->cfg_num = ARRAY_SIZE(imx678_supported_modes_4lane);
		for (i = 0; i < imx678->cfg_num; i++) {
			if (hdr_mode == imx678_supported_modes_4lane[i].hdr_mode) {
				imx678->cur_mode = &imx678_supported_modes_4lane[i];
				break;
			}
		}
		
		if (i == imx678->cfg_num)
			imx678->cur_mode = &imx678_supported_modes_4lane[0];

	} else {
		imx678->cfg_num = ARRAY_SIZE(imx678_supported_modes_2lane);
		for (i = 0; i < imx678->cfg_num; i++) {
			if (hdr_mode == imx678_supported_modes_4lane[i].hdr_mode) {
				imx678->cur_mode = &imx678_supported_modes_2lane[i];
				break;
			}
		}
		
		if (i == imx678->cfg_num)
			imx678->cur_mode = &imx678_supported_modes_2lane[0];
	}

	dev_info(dev, "imx678 current lane_num is(%d)\n",imx678->lane_num);	
	dev_err(dev, "====imx678 current lane_num is(%d)\n",imx678->lane_num);	
	
	return 0;
}


static int imx678_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct device_node *node = dev->of_node;
	struct imx678 *imx678;
	struct v4l2_subdev *sd;
	char facing[2];
	int ret;
	u32 hdr_mode = 0;

	dev_info(dev, "driver version: %02x.%02x.%02x",
		DRIVER_VERSION >> 16,
		(DRIVER_VERSION & 0xff00) >> 8,
		DRIVER_VERSION & 0x00ff);

	imx678 = devm_kzalloc(dev, sizeof(*imx678), GFP_KERNEL);
	if (!imx678)
		return -ENOMEM;

	ret = of_property_read_u32(node, RKMODULE_CAMERA_MODULE_INDEX,
				   &imx678->module_index);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_FACING,
				       &imx678->module_facing);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_NAME,
				       &imx678->module_name);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_LENS_NAME,
				       &imx678->len_name);
	if (ret) {
		dev_err(dev, "could not get module information!\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(node, OF_CAMERA_HDR_MODE, &hdr_mode);
	if (ret) {
		hdr_mode = NO_HDR;
		dev_warn(dev, " Get hdr mode failed! no hdr default\n");
	}

	imx678->client = client;

	imx678->xvclk = devm_clk_get(dev, "xvclk");
	if (IS_ERR(imx678->xvclk)) {
		dev_err(dev, "Failed to get xvclk\n");
		return -EINVAL;
	}

	imx678->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(imx678->reset_gpio))
		dev_warn(dev, "Failed to get reset-gpios\n");
	imx678->pwdn_gpio = devm_gpiod_get(dev, "pwdn", GPIOD_OUT_LOW);
	if (IS_ERR(imx678->pwdn_gpio))
		dev_warn(dev, "Failed to get pwdn-gpios\n");
	imx678->pinctrl = devm_pinctrl_get(dev);
	if (!IS_ERR(imx678->pinctrl)) {
		imx678->pins_default =
			pinctrl_lookup_state(imx678->pinctrl,
					     OF_CAMERA_PINCTRL_STATE_DEFAULT);
		if (IS_ERR(imx678->pins_default))
			dev_info(dev, "could not get default pinstate\n");

		imx678->pins_sleep =
			pinctrl_lookup_state(imx678->pinctrl,
					     OF_CAMERA_PINCTRL_STATE_SLEEP);
		if (IS_ERR(imx678->pins_sleep))
			dev_info(dev, "could not get sleep pinstate\n");
	} else {
		dev_info(dev, "no pinctrl\n");
	}

	ret = imx678_configure_regulators(imx678);
	if (ret) {
		dev_err(dev, "Failed to get power regulators\n");
		return ret;
	}

	ret = imx678_parse_of(imx678,hdr_mode);
	if (ret != 0)
		return -EINVAL;

	mutex_init(&imx678->mutex);

	sd = &imx678->subdev;
	v4l2_i2c_subdev_init(sd, client, &imx678_subdev_ops);
	ret = imx678_initialize_controls(imx678);
	if (ret)
		goto err_destroy_mutex;

	ret = __imx678_power_on(imx678);
	if (ret)
		goto err_free_handler;

	ret = imx678_check_sensor_id(imx678, client);
//	if (ret)
//		goto err_power_off;

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
	sd->internal_ops = &imx678_internal_ops;
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE |
		     V4L2_SUBDEV_FL_HAS_EVENTS;
#endif
#if defined(CONFIG_MEDIA_CONTROLLER)
	imx678->pad.flags = MEDIA_PAD_FL_SOURCE;
	sd->entity.function = MEDIA_ENT_F_CAM_SENSOR;
	ret = media_entity_pads_init(&sd->entity, 1, &imx678->pad);
	if (ret < 0)
		goto err_power_off;
#endif

	memset(facing, 0, sizeof(facing));
	if (strcmp(imx678->module_facing, "back") == 0)
		facing[0] = 'b';
	else
		facing[0] = 'f';

	snprintf(sd->name, sizeof(sd->name), "m%02d_%s_%s %s",
		 imx678->module_index, facing,
		 IMX678_NAME, dev_name(sd->dev));
	ret = v4l2_async_register_subdev_sensor_common(sd);
	if (ret) {
		dev_err(dev, "v4l2 async register subdev failed\n");
		goto err_clean_entity;
	}

	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);
	pm_runtime_idle(dev);

	g_isHCG = false;
#ifdef USED_SYS_DEBUG
	add_sysfs_interfaces(dev);
#endif

	pr_err("====finished %s probe====\n", __FUNCTION__);

	return 0;

err_clean_entity:
#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&sd->entity);
#endif
err_power_off:
	__imx678_power_off(imx678);
err_free_handler:
	v4l2_ctrl_handler_free(&imx678->ctrl_handler);
err_destroy_mutex:
	mutex_destroy(&imx678->mutex);

	return ret;
}

static int imx678_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct imx678 *imx678 = to_imx678(sd);

	v4l2_async_unregister_subdev(sd);
#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&sd->entity);
#endif
	v4l2_ctrl_handler_free(&imx678->ctrl_handler);
	mutex_destroy(&imx678->mutex);

	pm_runtime_disable(&client->dev);
	if (!pm_runtime_status_suspended(&client->dev))
		__imx678_power_off(imx678);
	pm_runtime_set_suspended(&client->dev);

#ifdef USED_SYS_DEBUG
	remove_sysfs_interfaces(&client->dev);
#endif

	return 0;
}

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id imx678_of_match[] = {
	{ .compatible = "sony,imx678" },
	{},
};
MODULE_DEVICE_TABLE(of, imx678_of_match);
#endif

static const struct i2c_device_id imx678_match_id[] = {
	{ "sony,imx678", 0 },
	{ },
};

static struct i2c_driver imx678_i2c_driver = {
	.driver = {
		.name = IMX678_NAME,
		.pm = &imx678_pm_ops,
		.of_match_table = of_match_ptr(imx678_of_match),
	},
	.probe		= &imx678_probe,
	.remove		= &imx678_remove,
	.id_table	= imx678_match_id,
};

static int __init sensor_mod_init(void)
{
	return i2c_add_driver(&imx678_i2c_driver);
}

static void __exit sensor_mod_exit(void)
{
	i2c_del_driver(&imx678_i2c_driver);
}

device_initcall_sync(sensor_mod_init);
module_exit(sensor_mod_exit);

MODULE_DESCRIPTION("Sony imx678 sensor driver");
MODULE_LICENSE("GPL v2");
