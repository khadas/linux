/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Global Timer driver for Amlogic Meson SoCs
 *
 */

#define DRIVER_NAME	"meson_glb_timer"
#define SRC_OFFSET(REG, SRCN)	((REG) + (((SRCN) * 0x08) << 2))

struct meson_glb_timer;
enum meson_glb_reg_base {
	REG_TOPCTL			= 0,
	REG_SRCSEL			= 1,
	REG_OUTCTL			= 2,
	REG_OUTSEL			= 3,
	REG_NUM				= 4,
};

enum meson_glb_topctl_reg {
	TOP_CTRL0			= 0x00 << 2,
	TOP_CTRL1			= 0x01 << 2,
	TOP_CTRL2			= 0x02 << 2,
	TOP_CTRL3			= 0x03 << 2,
	TOP_CTRL4			= 0x04 << 2,
	TOP_TS0				= 0x08 << 2,
	TOP_TS1				= 0x09 << 2,
};

enum meson_glb_srcsel_reg {
	TRIG_SRC0_CTRL0			= 0x00 << 2,
	TRIG_SRC0_TS_L			= 0x01 << 2,
	TRIG_SRC0_TS_H			= 0x02 << 2,
};

enum meson_glb_outctl_reg {
	OUTPUT_CTRL0			= 0x00 << 2,
	OUTPUT_PULSE_WDITH		= 0x01 << 2,
	OUTPUT_INTERVAL			= 0x02 << 2,
	OUTPUT_STOP_NUM			= 0x03 << 2,
	OUTPUT_EXPIRES_TS_L		= 0x04 << 2,
	OUTPUT_EXPIRES_TS_H		= 0x05 << 2,
	OUTPUT_ST0			= 0x06 << 2,
	OUTPUT_ST1			= 0x07 << 2,
};

enum meson_glb_topctl_ops {
	RESET_TIMER			= 0x1,
	HOLD_COUNTER			= 0x2,
	CLEAR_TIMER			= 0x3,
	SET_FORCE_COUNT			= 0x4,
};

enum output_level {
	LOW				= 0,
	HIGH				= 1,
};

enum output_mode {
	EXPIRE_MODE			= 0,
	SOFTWARE_MODE			= 1,
	ONESHOT_MODE			= 2,
};

struct meson_glb_timer {
	void __iomem *reg[REG_NUM];
	struct regmap *regmap[REG_NUM];
	struct clk *glb_clk;
	struct platform_device *pdev;
	struct hrtimer tst_hrtimer;
	struct gpio_descs *trig_gpios;
};
