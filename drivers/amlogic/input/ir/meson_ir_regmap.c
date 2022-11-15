// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include "meson_ir_main.h"

static struct meson_ir_reg_map regs_legacy_nec[] = {
	{REG_LDR_ACTIVE,    (500 << 16) | (400 << 0)},
	{REG_LDR_IDLE,      (300 << 16) | (200 << 0)},
	{REG_LDR_REPEAT,    (150 << 16) | (80 << 0)},
	{REG_BIT_0,         (72 << 16) | (40 << 0) },
	{REG_REG0,          (7 << 28) | (0xFA0 << 12) | 0x13},
	{REG_STATUS,        (134 << 20) | (90 << 10)},
	{REG_REG1,          0xbe00},
};

static struct meson_ir_reg_map regs_default_nec[] = {
	{ REG_LDR_ACTIVE,   (500 << 16) | (400 << 0)},
	{ REG_LDR_IDLE,     (300 << 16) | (200 << 0)},
	{ REG_LDR_REPEAT,   (150 << 16) | (80 << 0)},
	{ REG_BIT_0,        (72 << 16) | (40 << 0)},
	{ REG_REG0,         (7 << 28) | (0xFA0 << 12) | 0x13},
	{ REG_STATUS,       (134 << 20) | (90 << 10) | (1 << 30)},
	{ REG_REG1,         0x9f00},
	{ REG_REG2,         0x00},
	{ REG_DURATN2,      0x00},
	{ REG_DURATN3,      0x00},
	{ REG_IRQ_CTL,      0xCFA10BB8}
};

static struct meson_ir_reg_map regs_default_duokan[] = {
	{ REG_LDR_ACTIVE,   ((70 << 16) | (30 << 0))},
	{ REG_LDR_IDLE,     ((50 << 16) | (15 << 0))},
	{ REG_LDR_REPEAT,   ((30 << 16) | (26 << 0))},
	{ REG_BIT_0,        ((66 << 16) | (40 << 0))},
	{ REG_REG0,         ((3 << 28) | (0x4e2 << 12) | (0x13))},
	{ REG_STATUS,       ((80 << 20) | (66 << 10))},
	{ REG_REG1,         0x9300},
	{ REG_REG2,         0xb90b},
	{ REG_DURATN2,      ((97 << 16) | (80 << 0))},
	{ REG_DURATN3,      ((120 << 16) | (97 << 0))},
	{ REG_REG3,         5000 << 0}
};

static struct meson_ir_reg_map regs_default_xmp_1_sw[] = {
	{ REG_LDR_ACTIVE,   0},
	{ REG_LDR_IDLE,     0},
	{ REG_LDR_REPEAT,   0},
	{ REG_BIT_0,        0},
	{ REG_REG0,        ((7 << 28) | (0xFA0 << 12) | (9))},
	{ REG_STATUS,       0},
	{ REG_REG1,         0x8574},
	{ REG_REG2,         0x02},
	{ REG_DURATN2,      0},
	{ REG_DURATN3,      0},
	{ REG_REG3,         0}
};

static struct meson_ir_reg_map regs_default_xmp_1[] = {
	{ REG_LDR_ACTIVE,   0},
	{ REG_LDR_IDLE,     0},
	{ REG_LDR_REPEAT,   0},
	{ REG_BIT_0,        (52 << 16) | (45 << 0)},
	{ REG_REG0,         ((7 << 28) | (0x5DC << 12) | (0x13))},
	{ REG_STATUS,       (86 << 20) | (80 << 10)},
	{ REG_REG1,         0x9f00},
	{ REG_REG2,         0xa90e},
	/*n=10,758+137*10=2128us,2128/20= 106*/
	{ REG_DURATN2,      (121 << 16) | (114 << 0)},
	{ REG_DURATN3,      (7 << 16) | (7 << 0)},
	{ REG_REG3,         0}
};

static struct meson_ir_reg_map regs_default_nec_sw[] = {
	{ REG_LDR_ACTIVE,   0},
	{ REG_LDR_IDLE,     0},
	{ REG_LDR_REPEAT,   0},
	{ REG_BIT_0,        0},
	{ REG_REG0,         ((7 << 28) | (0xFA0 << 12) | (9))},
	{ REG_STATUS,       0},
	{ REG_REG1,         0x8574},
	{ REG_REG2,         0x02},
	{ REG_DURATN2,      0},
	{ REG_DURATN3,      0},
	{ REG_REG3,         0}
};

static struct meson_ir_reg_map regs_default_rc5[] = {
	{ REG_LDR_ACTIVE,   0},
	{ REG_LDR_IDLE,     0},
	{ REG_LDR_REPEAT,   0},
	{ REG_BIT_0,        0},
	{ REG_REG0,         ((3 << 28) | (0x1644 << 12) | 0x13)},
	{ REG_STATUS,       (1 << 30)},
	{ REG_REG1,         ((1 << 15) | (13 << 8))},
	/*bit[0-3]: RC5; bit[8]: MSB first mode; bit[11]: compare frame method*/
	{ REG_REG2,         ((1 << 13) | (1 << 11) | (1 << 8) | 0x7)},
	/*Half bit for RC5 format: 888.89us*/
	{ REG_DURATN2,      ((49 << 16) | (40 << 0))},
	/*RC5 typically 1777.78us for whole bit*/
	{ REG_DURATN3,      ((94 << 16) | (83 << 0))},
	{ REG_REG3,         0}
};

static struct meson_ir_reg_map regs_default_rc6[] = {
	{REG_LDR_ACTIVE,    (210 << 16) | (125 << 0)},
	/*rc6 leader 4000us,200* timebase*/
	{REG_LDR_IDLE,      (50 << 16) | (38 << 0)}, /* leader idle 400*/
	{REG_LDR_REPEAT,    (145 << 16) | (125 << 0)}, /* leader repeat*/
	/* logic '0' or '00' 1500us*/
	{REG_BIT_0,         (51 << 16) | (38 << 0) },
	{REG_REG0,          (7 << 28) | (0xFA0 << 12) | 0x13},
	/* sys clock boby time.base time = 20 body frame*/
	{REG_STATUS,        (94 << 20) | (82 << 10)},
	/*20bit:9440 32bit:9f40 36bit:a340 37bit:a440*/
	{REG_REG1,          0xa440},
	/*it may get the wrong customer value and key value from register if
	 *the value is set to 0x4,so the register value must set to 0x104
	 */
	{REG_REG2,          0x3909},
	{REG_DURATN2,       ((28 << 16) | (16 << 0))},
	{REG_DURATN3,       ((51 << 16) | (38 << 0))},
	{REG_REG3,          2000}
};

static struct meson_ir_reg_map regs_default_toshiba[] = {
	{ REG_LDR_ACTIVE,   (280 << 16) | (180 << 0)},
	{ REG_LDR_IDLE,     (280 << 16) | (180 << 0)},
	{ REG_LDR_REPEAT,   (150 << 16) | (60 << 0)},
	{ REG_BIT_0,        (72 << 16) | (40 << 0)},
	{ REG_REG0,         (7 << 28) | (0xFA0 << 12) | 0x13},
	{ REG_STATUS,       (134 << 20) | (90 << 10)},
	{ REG_REG1,         0x9f00},
	{ REG_REG2,         0x05 | (1 << 24) | (23 << 11)},
	{ REG_DURATN2,      0x00},
	{ REG_DURATN3,      0x00,},
	{ REG_REPEAT_DET,   (1 << 31) | (0xFA0 << 16) | (10 << 0)},
	{ REG_REG3,         0x2AF8}
};

static struct meson_ir_reg_map regs_default_rca[] = {
	{ REG_LDR_ACTIVE,   (250 << 16) | (160 << 0)},
	{ REG_LDR_IDLE,     (250 << 16) | (160 << 0)},
	{ REG_LDR_REPEAT,   (250 << 16) | (160 << 0)},
	{ REG_BIT_0,        (100 << 16) | (48 << 0)},
	{ REG_REG0,         (7 << 28) | (0xFA0 << 12) | 0x13},
	{ REG_STATUS,       (150 << 20) | (110 << 10)},
	{ REG_REG1,         0x9700},
	{ REG_REG2,         0x104 | (1 << 24) | (23 << 11)},
	{ REG_DURATN2,      0x00},
	{ REG_REPEAT_DET,   (1 << 31) | (0xFA0 << 16) | (10 << 0)},
	{ REG_REG3,         0x1A00},
	{ REG_DURATN3,      0x00}
};

#define FRAME_MSB_FIRST BIT(8)
static struct meson_ir_reg_map regs_default_sharp[] = {
	{ REG_LDR_ACTIVE,   0},
	{ REG_LDR_IDLE,     0},
	{ REG_LDR_REPEAT,   0},
	{ REG_BIT_0,	    (70 << 16) | (25 << 0)},
	{ REG_REG0,	    (7 << 28) | (3350 << 12) | 0x13},
	{ REG_STATUS,	    (125 << 20) | (75 << 10)},
	{ REG_REG1,	    0x8e00},
	{ REG_REG2,	    FRAME_MSB_FIRST | 0x01},
	{ REG_DURATN2,	    0x00},
	{ REG_DURATN3,	    0x00}
};

static struct meson_ir_reg_map regs_default_rcmm[] = {
	{REG_LDR_ACTIVE,    (35 << 16) | (17 << 0)},
	{REG_LDR_IDLE,      (17 << 16) | (8 << 0)},
	{REG_LDR_REPEAT,    (31 << 16) | (11 << 0)},
	{REG_BIT_0,         (25 << 16) | (21 << 0)},
	{REG_REG0,          (7 << 28)  | (0x590 << 12) | 0x13},
	{REG_STATUS,        (36 << 20) | (29 << 10)},
	{REG_REG1,         0x9f00},
	{REG_REG2,         0x3D0A},
	{REG_DURATN2,      ((43 << 16) | (37 << 0))},
	{REG_DURATN3,      ((50 << 16) | (44 << 0))},
	{REG_REG3,         1200 << 0},
};

static struct meson_ir_reg_map regs_default_mitsubishi[] = {
	{ REG_LDR_ACTIVE,   (500 << 16) | (300 << 0)},
	{ REG_LDR_IDLE,     (250 << 16) | (150 << 0)},
	{ REG_LDR_REPEAT,   (250 << 16) | (150 << 0)},
	{ REG_BIT_0,        (60 << 16)  | (40 << 0)},
	{ REG_REG0,         (7 << 28) | (0xBBB << 12) | 0x13},
	{ REG_STATUS,       (120 << 20) | (80 << 10)},
	{ REG_REG1,         0x8f00},
	{ REG_REG2,         (0x3) | (1 << 24) | (23 << 11)},
	{ REG_DURATN2,      0x00},
	{ REG_REG3,         0x1950},
	{ REG_DURATN3,      0x00}
};

void meson_ir_set_hardcode(struct meson_ir_chip *chip, int code)
{
	meson_ir_dbg(chip->r_dev, "framecode=0x%x\n", code);
	chip->r_dev->cur_hardcode = code;
}

/**
 * legacy nec hardware interface
 * other interface share with the multi-format NEC
 */
static int meson_ir_legacy_nec_get_scancode(struct meson_ir_chip *chip)
{
	int  code = 0;
	int decode_status = 0;
	int status = 0;

	regmap_read(chip->ir_contr[LEGACY_IR_ID].base, REG_STATUS,
		    &decode_status);
	if (decode_status & 0x01)
		status |= IR_STATUS_REPEAT;
	chip->decode_status = status; /*set decode status*/
	regmap_read(chip->ir_contr[LEGACY_IR_ID].base, REG_FRAME, &code);
	meson_ir_dbg(chip->r_dev, "framecode=0x%x\n", code);
	chip->r_dev->cur_hardcode = code;
	code = (code >> 16) & 0xff;
	return code;
}

/*
 *nec hardware interface
 */
static int meson_ir_nec_get_scancode(struct meson_ir_chip *chip)
{
	int  code = 0;
	int decode_status = 0;
	int status = 0;

	regmap_read(chip->ir_contr[MULTI_IR_ID].base, REG_STATUS,
		    &decode_status);
	if (decode_status & 0x01)
		status |= IR_STATUS_REPEAT;
	chip->decode_status = status; /*set decode status*/
	regmap_read(chip->ir_contr[MULTI_IR_ID].base, REG_FRAME, &code);
	meson_ir_dbg(chip->r_dev, "framecode=0x%x\n", code);
	chip->r_dev->cur_hardcode = code;
	code = (code >> 16) & 0xff;
	return code;
}

static int meson_ir_nec_get_decode_status(struct meson_ir_chip *chip)
{
	int status = chip->decode_status;
	return status;
}

static u32 meson_ir_nec_get_custom_code(struct meson_ir_chip *chip)
{
	u32 custom_code;

	custom_code = chip->r_dev->cur_hardcode & 0xffff;
	return custom_code;
}

/*
 *	xmp-1 decode hardware interface
 */
static int xmp_decode_second;
static int meson_ir_xmp_get_scancode(struct meson_ir_chip *chip)
{
	int  code = 0;

	regmap_read(chip->ir_contr[MULTI_IR_ID].base, REG_FRAME, &code);
	meson_ir_dbg(chip->r_dev, "framecode=0x%x\n", code);
	if (!xmp_decode_second) {
		if (meson_ir_seek_map_tab(chip, code & 0xffff)) {
			chip->r_dev->cur_hardcode = 0;
			chip->r_dev->cur_customcode = code;
			xmp_decode_second = 1;
		}
		return -1;
	}
	xmp_decode_second = 2;
	chip->r_dev->cur_hardcode = code;
	code = (code >> 8) & 0xff;
	return code;
}

static int meson_ir_xmp_get_decode_status(struct meson_ir_chip *chip)
{
	int status = 0;

	switch (xmp_decode_second) {
	case 0:
	case 1:
		status = IR_STATUS_CUSTOM_DATA;
		break;
	case 2:
		if (chip->r_dev->cur_hardcode & (8 << 20))
			status = IR_STATUS_REPEAT;
		else
			status = IR_STATUS_NORMAL;
		xmp_decode_second = 0;
		break;
	default:
		break;
	}
	return status;
}

static u32 meson_ir_xmp_get_custom_code(struct meson_ir_chip *chip)
{
	u32 custom_code;

	custom_code = chip->r_dev->cur_customcode & 0xffff;
	meson_ir_dbg(chip->r_dev, "custom_code=0x%x\n", custom_code);
	return custom_code;
}

/*
 * duokan hardware interface
 */
static int meson_ir_duokan_parity_check(int code)
{
	unsigned int data;
	unsigned int c74, c30, d74, d30, p30;

	c74 = (code >> 16) & 0xF;
	c30 = (code >> 12) & 0xF;
	d74 = (code >> 8)  & 0xF;
	d30 = (code >> 4)  & 0xF;
	p30 = (code >> 0)  & 0xF;

	data = c74 ^ c30 ^ d74 ^ d30;

	if (p30 == data)
		return 0;

	pr_err("%s: parity check error code=0x%x\n", DRIVER_NAME, code);
	return -1;
}

static int meson_ir_duokan_get_scancode(struct meson_ir_chip *chip)
{
	int  code = 0;

	regmap_read(chip->ir_contr[MULTI_IR_ID].base, REG_FRAME, &code);
	if (meson_ir_duokan_parity_check(code) < 0) {
		meson_ir_set_hardcode(chip, 0);
		return 0;
	}
	meson_ir_set_hardcode(chip, code);
	code = (code >> 4) & 0xff;
	return code;
}

static int meson_ir_duokan_get_decode_status(struct meson_ir_chip *chip)
{
	int decode_status = 0;
	int status = 0;

	regmap_read(chip->ir_contr[MULTI_IR_ID].base, REG_STATUS,
		    &decode_status);
	decode_status &= 0xf;
	if (decode_status & 0x01)
		status |= IR_STATUS_REPEAT;
	/*
	 *it is error,if the custom_code is not mask.
	 *if (decode_status & 0x02)
	 *	status |= IR_STATUS_CUSTOM_ERROR;
	 */
	if (decode_status & 0x04)
		status |= IR_STATUS_DATA_ERROR;
	return status;
}

static u32 meson_ir_duokan_get_custom_code(struct meson_ir_chip *chip)
{
	u32 custom_code;

	custom_code = (chip->r_dev->cur_hardcode >> 12) & 0xffff;
	chip->r_dev->cur_customcode = custom_code;
	meson_ir_dbg(chip->r_dev, "custom_code=0x%x\n", custom_code);
	return custom_code;
}

/*
 *xmp-1 raw decoder interface
 */
static u32 meson_ir_raw_xmp_get_custom_code(struct meson_ir_chip *chip)
{
	u32 custom_code;

	custom_code = chip->r_dev->cur_customcode;
	meson_ir_dbg(chip->r_dev, "custom_code=0x%x\n", custom_code);
	return custom_code;
}

static bool meson_ir_raw_xmp_set_custom_code(struct meson_ir_chip *chip,
					     u32 code)
{
	chip->r_dev->cur_customcode = code;
	return 0;
}

/*
 * RC5 decoder interface
 * 14bit of one frame is stored in [13:0]:
 *      bit[13:11] is S1, S2, Toggle
 *      bit[10:6] is system_code/custom_code
 *      bit [5:0] is data_code/scan_code
 */

static int meson_ir_rc5_get_scancode(struct meson_ir_chip *chip)
{
	int code = 0;
	int status = 0;
	int decode_status = 0;

	regmap_read(chip->ir_contr[MULTI_IR_ID].base, REG_STATUS,
		    &decode_status);
	decode_status &= 0xf;
	if (decode_status & 0x01)
		status |= IR_STATUS_REPEAT;
	chip->decode_status = status;
	regmap_read(chip->ir_contr[MULTI_IR_ID].base, REG_FRAME, &code);
	meson_ir_dbg(chip->r_dev, "framecode=0x%x\n", code);
	chip->r_dev->cur_hardcode = code;
	code = code & 0x3f;
	return code;
}

static int meson_ir_rc5_get_decode_status(struct meson_ir_chip *chip)
{
	int status = chip->decode_status;

	return status;
}

static u32 meson_ir_rc5_get_custom_code(struct meson_ir_chip *chip)
{
	u32 custom_code;

	custom_code = (chip->r_dev->cur_hardcode >> 6) & 0x1f;
	return custom_code;
}

/*RC6 decode interface*/
static int meson_ir_rc6_get_scancode(struct meson_ir_chip *chip)
{
	int code = 0;
	int code1 = 0;
	int status = 0;
	int decode_status = 0;

	regmap_read(chip->ir_contr[MULTI_IR_ID].base, REG_STATUS,
		    &decode_status);
	decode_status &= 0xf;
	if (decode_status & 0x01)
		status |= IR_STATUS_REPEAT;
	chip->decode_status = status;
	regmap_read(chip->ir_contr[MULTI_IR_ID].base, REG_FRAME, &code);
	meson_ir_dbg(chip->r_dev, "framecode=0x%x\n", code);
	/**
	 *if the frame length larger than 32Bit, we must read the REG_FRAME1.
	 *Otherwise it will affect the update of the 'frame1' and repeat frame
	 *detect
	 */
	regmap_read(chip->ir_contr[MULTI_IR_ID].base, REG_FRAME1, &code1);
	chip->r_dev->cur_hardcode = code;
	code = code & 0xff;
	return code;
}

static int meson_ir_rc6_get_decode_status(struct meson_ir_chip *chip)
{
	int status = chip->decode_status;

	return status;
}

static u32 meson_ir_rc6_get_custom_code(struct meson_ir_chip *chip)
{
	u32 custom_code;

	custom_code = (chip->r_dev->cur_hardcode >> 16) & 0xffff;
	return custom_code;
}

static int meson_ir_toshiba_get_scancode(struct meson_ir_chip *chip)
{
	int  code = 0;
	int decode_status = 0;
	int status = 0;

	regmap_read(chip->ir_contr[MULTI_IR_ID].base, REG_STATUS,
		    &decode_status);
	if (decode_status & 0x01)
		status |= IR_STATUS_REPEAT;
	chip->decode_status = status; /*set decode status*/
	regmap_read(chip->ir_contr[MULTI_IR_ID].base, REG_FRAME, &code);
	meson_ir_dbg(chip->r_dev, "framecode=0x%x\n", code);
	chip->r_dev->cur_hardcode = code;
	code = (code >> 16) & 0xff;
	return code;
}

static int meson_ir_toshiba_get_decode_status(struct meson_ir_chip *chip)
{
	int status = chip->decode_status;

	return status;
}

static u32 meson_ir_toshiba_get_custom_code(struct meson_ir_chip *chip)
{
	u32 custom_code;

	custom_code = (chip->r_dev->cur_hardcode) & 0xffff;
	return custom_code;
}

static int meson_ir_rca_get_scancode(struct meson_ir_chip *chip)
{
	int  code = 0;
	int decode_status = 0;
	int status = 0;

	regmap_read(chip->ir_contr[MULTI_IR_ID].base, REG_STATUS,
		    &decode_status);
	decode_status &= 0xf;
	if (decode_status & 0x01)
		status |= IR_STATUS_REPEAT;

	chip->decode_status = status;
	regmap_read(chip->ir_contr[MULTI_IR_ID].base, REG_FRAME, &code);
	meson_ir_dbg(chip->r_dev, "framecode=0x%x\n", code);
	chip->r_dev->cur_hardcode = code;

	if ((~(code >> 12) & 0xfff) != (code & 0xfff)) {
		pr_err("%s: rca verification check error code=0x%x\n",
		       DRIVER_NAME, code);
		return 0;
	}

	code = (code >> 12) & 0xff;
	return code;
}

static int meson_ir_rca_get_decode_status(struct meson_ir_chip *chip)
{
	int status = chip->decode_status;

	return status;
}

static u32 meson_ir_rca_get_custom_code(struct meson_ir_chip *chip)
{
	u32 custom_code;

	custom_code = (chip->r_dev->cur_hardcode >> 20) & 0x0f;
	return custom_code;
}

static int meson_ir_sharp_get_scancode(struct meson_ir_chip *chip)
{
	int code = 0;
	int status = 0;
	int decode_status = 0;
	int key0 = 0, key0_mask;
	int temp_h = 0, temp_l = 0;
	int switch_code = 0;
	int i = 0;
	#define NUM_BIT 15

	regmap_read(chip->ir_contr[MULTI_IR_ID].base, REG_STATUS,
		    &decode_status);

	decode_status &= 0xf;
	if (decode_status & 0x01)
		status |= IR_STATUS_REPEAT;
	regmap_read(chip->ir_contr[MULTI_IR_ID].base, REG_FRAME, &code);
	meson_ir_dbg(chip->r_dev, "rx-data=0x%x, rx_count=%d\n",
		     code, chip->rx_count);

	switch (chip->rx_count) {
	case 0:
		chip->rx_buffer[0] = code;
		chip->rx_count = 1;
		chip->decode_status = IR_STATUS_CUSTOM_DATA;
		return -1;
	case 1:
		key0_mask = (~(code >> 2)) & 0xff;
		key0 = (chip->rx_buffer[0] >> 2) & 0xff;
		if (key0 != key0_mask) {
			chip->rx_buffer[0] = code;
			chip->rx_count = 1;
			chip->decode_status = IR_STATUS_CHECKSUM_ERROR;
			return -1;
		}
		chip->rx_count = 0; /*enter new code receive*/
		break;
	default:
		break;
	}
	code = chip->rx_buffer[0];
	meson_ir_dbg(chip->r_dev, "framecode=0x%x\n", code);
	for (i = 0; i <= NUM_BIT; i++)
		switch_code |= ((code >> i) & 0x01) << (NUM_BIT - i);
	chip->r_dev->cur_hardcode = switch_code;
	temp_h = (switch_code >> 10) & 0xf;
	temp_l = (switch_code >> 6) & 0xf;
	code = (temp_h << 4) | temp_l;
	meson_ir_dbg(chip->r_dev, "switchcode=0x%x, code=0x%x\n",
		     switch_code, code);
	chip->decode_status = IR_STATUS_NORMAL;
	return code;
}

static int meson_ir_sharp_get_decode_status(struct meson_ir_chip *chip)
{
	int status = chip->decode_status;

	return status;
}

static u32 meson_ir_sharp_get_custom_code(struct meson_ir_chip *chip)
{
	u32 custom_code;
	u32 temp_h = 0, temp_l = 0;

	temp_h = (chip->r_dev->cur_hardcode >> 5) & 0x1;
	temp_l = (chip->r_dev->cur_hardcode >> 1) & 0xf;
	custom_code = (temp_h << 4) | temp_l;
	meson_ir_dbg(chip->r_dev, "customcode=0x%x\n", custom_code);
	return custom_code;
}

/* RCMM decode interface */
static int ir_rcmm_get_scancode(struct meson_ir_chip *chip)
{
	int code = 0;
	int status = 0;
	int decode_status = 0;

	/*get status*/
	regmap_read(chip->ir_contr[MULTI_IR_ID].base, REG_STATUS,
		    &decode_status);
	decode_status &= 0xf;
	if (decode_status & 0x01)
		status |= IR_STATUS_REPEAT;
	chip->decode_status = status;

	/*get frame*/
	regmap_read(chip->ir_contr[MULTI_IR_ID].base, REG_FRAME, &code);
	meson_ir_dbg(chip->r_dev, "framecode=0x%x\n", code);
	chip->r_dev->cur_hardcode = code;
	code = code & 0xff;
	return code;
}

static int ir_rcmm_get_decode_status(struct meson_ir_chip *chip)
{
	int status = chip->decode_status;

	return status;
}

static u32 ir_rcmm_get_custom_code(struct meson_ir_chip  *chip)
{
	u32 custom_code;

	custom_code = (chip->r_dev->cur_hardcode >> 16) & GENMASK(15, 0);
	return custom_code;
}

static int meson_ir_mitsubishi_get_scancode(struct meson_ir_chip *chip)
{
	int  code = 0;
	int decode_status = 0;
	int status = 0;

	regmap_read(chip->ir_contr[MULTI_IR_ID].base, REG_STATUS, &decode_status);
	decode_status &= 0xf;
	if (decode_status & 0x01)
		status |= IR_STATUS_REPEAT;

	chip->decode_status = status;
	regmap_read(chip->ir_contr[MULTI_IR_ID].base, REG_FRAME, &code);
	meson_ir_dbg(chip->r_dev, "framecode=0x%x\n", code);
	chip->r_dev->cur_hardcode = code;
	code = (code >> 8) & 0xff;
	return code;
}

static int meson_ir_mitsubishi_get_decode_status(struct meson_ir_chip *chip)
{
	int status = chip->decode_status;

	return status;
}

static u32 meson_ir_mitsubishi_get_custom_code(struct meson_ir_chip *chip)
{
	u32 custom_code;

	custom_code = chip->r_dev->cur_hardcode & 0xff;
	return custom_code;
}

/*legacy IR controller support protocols*/
static struct meson_ir_reg_proto reg_rcmm = {
	.protocol = REMOTE_TYPE_RCMM,
	.name     = "RCMM",
	.reg_map      = regs_default_rcmm,
	.reg_map_size = ARRAY_SIZE(regs_default_rcmm),
	.get_scancode      = ir_rcmm_get_scancode,
	.get_decode_status = ir_rcmm_get_decode_status,
	.get_custom_code   = ir_rcmm_get_custom_code,
};

static struct meson_ir_reg_proto reg_legacy_nec = {
	.protocol = REMOTE_TYPE_LEGACY_NEC,
	.name     = "LEGACY_NEC",
	.reg_map      = regs_legacy_nec,
	.reg_map_size = ARRAY_SIZE(regs_legacy_nec),
	.get_scancode      = meson_ir_legacy_nec_get_scancode,
	.get_decode_status = meson_ir_nec_get_decode_status,
	.get_custom_code   = meson_ir_nec_get_custom_code
};

static struct meson_ir_reg_proto reg_nec = {
	.protocol = REMOTE_TYPE_NEC,
	.name     = "NEC",
	.reg_map      = regs_default_nec,
	.reg_map_size = ARRAY_SIZE(regs_default_nec),
	.get_scancode      = meson_ir_nec_get_scancode,
	.get_decode_status = meson_ir_nec_get_decode_status,
	.get_custom_code   = meson_ir_nec_get_custom_code
};

static struct meson_ir_reg_proto reg_duokan = {
	.protocol = REMOTE_TYPE_DUOKAN,
	.name	  = "DUOKAN",
	.reg_map      = regs_default_duokan,
	.reg_map_size = ARRAY_SIZE(regs_default_duokan),
	.get_scancode      = meson_ir_duokan_get_scancode,
	.get_decode_status = meson_ir_duokan_get_decode_status,
	.get_custom_code   = meson_ir_duokan_get_custom_code
};

static struct meson_ir_reg_proto reg_xmp_1_sw = {
	.protocol = REMOTE_TYPE_RAW_XMP_1,
	.name	  = "XMP-1-RAW",
	.reg_map      = regs_default_xmp_1_sw,
	.reg_map_size = ARRAY_SIZE(regs_default_xmp_1_sw),
	.get_scancode      = NULL,
	.get_decode_status = NULL,
	.get_custom_code   = meson_ir_raw_xmp_get_custom_code,
	.set_custom_code   = meson_ir_raw_xmp_set_custom_code
};

static struct meson_ir_reg_proto reg_xmp_1 = {
	.protocol = REMOTE_TYPE_XMP_1,
	.name	  = "XMP-1",
	.reg_map      = regs_default_xmp_1,
	.reg_map_size = ARRAY_SIZE(regs_default_xmp_1),
	.get_scancode      = meson_ir_xmp_get_scancode,
	.get_decode_status = meson_ir_xmp_get_decode_status,
	.get_custom_code   = meson_ir_xmp_get_custom_code,
};

static struct meson_ir_reg_proto reg_nec_sw = {
	.protocol = REMOTE_TYPE_RAW_NEC,
	.name	  = "NEC-SW",
	.reg_map      = regs_default_nec_sw,
	.reg_map_size = ARRAY_SIZE(regs_default_nec_sw),
	.get_scancode      = NULL,
	.get_decode_status = NULL,
	.get_custom_code   = NULL,
};

static struct meson_ir_reg_proto reg_rc5 = {
	.protocol = REMOTE_TYPE_RC5,
	.name	  = "RC5",
	.reg_map      = regs_default_rc5,
	.reg_map_size = ARRAY_SIZE(regs_default_rc5),
	.get_scancode      = meson_ir_rc5_get_scancode,
	.get_decode_status = meson_ir_rc5_get_decode_status,
	.get_custom_code   = meson_ir_rc5_get_custom_code,
};

static struct meson_ir_reg_proto reg_rc6 = {
	.protocol = REMOTE_TYPE_RC6,
	.name	  = "RC6",
	.reg_map      = regs_default_rc6,
	.reg_map_size = ARRAY_SIZE(regs_default_rc6),
	.get_scancode      = meson_ir_rc6_get_scancode,
	.get_decode_status = meson_ir_rc6_get_decode_status,
	.get_custom_code   = meson_ir_rc6_get_custom_code,
};

static struct meson_ir_reg_proto reg_toshiba = {
	.protocol = REMOTE_TYPE_TOSHIBA,
	.name	  = "TOSHIBA",
	.reg_map      = regs_default_toshiba,
	.reg_map_size = ARRAY_SIZE(regs_default_toshiba),
	.get_scancode      = meson_ir_toshiba_get_scancode,
	.get_decode_status = meson_ir_toshiba_get_decode_status,
	.get_custom_code   = meson_ir_toshiba_get_custom_code,
};

static struct meson_ir_reg_proto reg_rca = {
	.protocol = REMOTE_TYPE_RCA,
	.name	  = "rca",
	.reg_map      = regs_default_rca,
	.reg_map_size = ARRAY_SIZE(regs_default_rca),
	.get_scancode	   = meson_ir_rca_get_scancode,
	.get_decode_status = meson_ir_rca_get_decode_status,
	.get_custom_code   = meson_ir_rca_get_custom_code,
};

static struct meson_ir_reg_proto reg_sharp = {
	.protocol = REMOTE_TYPE_SHARP,
	.name	  = "SHARP",
	.reg_map      = regs_default_sharp,
	.reg_map_size = ARRAY_SIZE(regs_default_sharp),
	.get_scancode	   = meson_ir_sharp_get_scancode,
	.get_decode_status = meson_ir_sharp_get_decode_status,
	.get_custom_code   = meson_ir_sharp_get_custom_code,
};

static struct meson_ir_reg_proto reg_mitsubishi = {
	.protocol = REMOTE_TYPE_MITSUBISHI,
	.name	  = "mitsubishi",
	.reg_map      = regs_default_mitsubishi,
	.reg_map_size = ARRAY_SIZE(regs_default_mitsubishi),
	.get_scancode	   = meson_ir_mitsubishi_get_scancode,
	.get_decode_status = meson_ir_mitsubishi_get_decode_status,
	.get_custom_code   = meson_ir_mitsubishi_get_custom_code,
};

const struct meson_ir_reg_proto *meson_ir_regs_proto[] = {
	&reg_nec,
	&reg_duokan,
	&reg_xmp_1,
	&reg_xmp_1_sw,
	&reg_nec_sw,
	&reg_rc5,
	&reg_rc6,
	&reg_legacy_nec,
	&reg_toshiba,
	&reg_rca,
	&reg_sharp,
	&reg_rcmm,
	&reg_mitsubishi,
	NULL
};

static int meson_ir_contr_init(struct meson_ir_chip *chip, int type,
			       unsigned char id)
{
	const struct meson_ir_reg_proto **reg_proto = meson_ir_regs_proto;
	struct meson_ir_reg_map *reg_map;
	int size;
	int status;

	for ( ; (*reg_proto) != NULL ; ) {
		if ((*reg_proto)->protocol == type)
			break;
		reg_proto++;
	}
	if (!*reg_proto) {
		dev_err(chip->dev, "%s, protocol set err %d\n", __func__, type);
		return -EINVAL;
	}

	regmap_read(chip->ir_contr[id].base, REG_STATUS, &status);
	regmap_read(chip->ir_contr[id].base, REG_FRAME,  &status);
	/*
	 * reset ir decoder and disable the state machine
	 * of IR decoder.
	 * [15] = 0 ,disable the machine of IR decoder
	 * [0] = 0x01,set to 1 to reset the IR decoder
	 */
	regmap_write(chip->ir_contr[id].base, REG_REG1, 0x01);
	reg_map = (*reg_proto)->reg_map;
	size    = (*reg_proto)->reg_map_size;

	for (  ; size > 0;  ) {
		regmap_write(chip->ir_contr[id].base, reg_map->reg,
			     reg_map->val);
		reg_map++;
		size--;
	}
	/*
	 * when we reinstall remote controller register,
	 * we need reset IR decoder, set 1 to REG_REG1 bit0,
	 * after IR decoder reset, we need to clear the bit0
	 */
	regmap_update_bits(chip->ir_contr[id].base, REG_REG1, BIT(1), 1);
	regmap_update_bits(chip->ir_contr[id].base, REG_REG1, BIT(1), 0);

	chip->ir_contr[id].get_scancode      = (*reg_proto)->get_scancode;
	chip->ir_contr[id].get_decode_status = (*reg_proto)->get_decode_status;
	chip->ir_contr[id].proto_name        = (*reg_proto)->name;
	chip->ir_contr[id].get_custom_code   = (*reg_proto)->get_custom_code;
	chip->ir_contr[id].set_custom_code   = (*reg_proto)->set_custom_code;

	return 0;
}

int meson_ir_register_default_config(struct meson_ir_chip *chip, int type)
{
	if (ENABLE_LEGACY_IR(type)) {
		/*initialize registers for legacy IR controller*/
		meson_ir_contr_init(chip, LEGACY_IR_TYPE_MASK(type),
				    LEGACY_IR_ID);
	} else {
		/*disable legacy IR controller: REG_REG1[15]*/
		regmap_write(chip->ir_contr[LEGACY_IR_ID].base, REG_REG1, 0x0);
	}
	/*initialize registers for Multi-format IR controller*/
	meson_ir_contr_init(chip, MULTI_IR_TYPE_MASK(type), MULTI_IR_ID);

	return 0;
}
EXPORT_SYMBOL(meson_ir_register_default_config);

