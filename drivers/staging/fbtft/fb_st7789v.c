// SPDX-License-Identifier: GPL-2.0+
/*
 * FB driver for the ST7789V LCD Controller
 *
 * Copyright (C) 2015 Dennis Menschel
 */

#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <video/mipi_display.h>

#include "fbtft.h"

#define DRVNAME "fb_st7789v"

#define DEFAULT_GAMMA \
	"70 2C 2E 15 10 09 48 33 53 0B 19 18 20 25\n" \
	"70 2C 2E 15 10 09 48 33 53 0B 19 18 20 25"

/**
 * enum st7789v_command - ST7789V display controller commands
 *
 * @PORCTRL: porch setting
 * @GCTRL: gate control
 * @VCOMS: VCOM setting
 * @VDVVRHEN: VDV and VRH command enable
 * @VRHS: VRH set
 * @VDVS: VDV set
 * @VCMOFSET: VCOM offset set
 * @PWCTRL1: power control 1
 * @PVGAMCTRL: positive voltage gamma control
 * @NVGAMCTRL: negative voltage gamma control
 *
 * The command names are the same as those found in the datasheet to ease
 * looking up their semantics and usage.
 *
 * Note that the ST7789V display controller offers quite a few more commands
 * which have been omitted from this list as they are not used at the moment.
 * Furthermore, commands that are compliant with the MIPI DCS have been left
 * out as well to avoid duplicate entries.
 */
enum st7789v_command {
	PORCTRL = 0xB2,
	GCTRL = 0xB7,
	VCOMS = 0xBB,
	VDVVRHEN = 0xC2,
	VRHS = 0xC3,
	VDVS = 0xC4,
	VCMOFSET = 0xC5,
	PWCTRL1 = 0xD0,
	PVGAMCTRL = 0xE0,
	NVGAMCTRL = 0xE1,
};

#define MADCTL_BGR BIT(3) /* bitmask for RGB/BGR order */
#define MADCTL_MV BIT(5) /* bitmask for page/column order */
#define MADCTL_MX BIT(6) /* bitmask for column address order */
#define MADCTL_MY BIT(7) /* bitmask for page address order */

/**
 * init_display() - initialize the display controller
 *
 * @par: FBTFT parameter object
 *
 * Most of the commands in this init function set their parameters to the
 * same default values which are already in place after the display has been
 * powered up. (The main exception to this rule is the pixel format which
 * would default to 18 instead of 16 bit per pixel.)
 * Nonetheless, this sequence can be used as a template for concrete
 * displays which usually need some adjustments.
 *
 * Return: 0 on success, < 0 if error occurred.
 */
static int init_display(struct fbtft_par *par)
{
	write_reg(par, 0x11);
	usleep_range(100, 200);
	write_reg(par, 0x36, 0x00);
	write_reg(par, 0x3a, 0x55);
	write_reg(par, 0xb2, 0x0c, 0x0c, 0x00, 0x33, 0x33);
	write_reg(par, 0xb7, 0x35);
	write_reg(par, 0xbb, 0x19);
	write_reg(par, 0xc0, 0x2c);
	write_reg(par, 0xc2, 0x01);
	write_reg(par, 0xc3, 0x12);
	write_reg(par, 0xc4, 0x20);
	write_reg(par, 0xc6, 0x0f);
	write_reg(par, 0xd0, 0xa4, 0xa1);

       /* gamma setting */
	write_reg(par, 0xe0, 0xd0, 0x04, 0x0d, 0x11, 0x13,
			0x2b, 0x3f, 0x54, 0x4c, 0x18, 0x0d, 0x0b, 0x1f, 0x23);
	write_reg(par, 0xe1, 0xd0, 0x04, 0x0c, 0x11, 0x13,
			0x2c, 0x3f, 0x44, 0x51, 0x2f, 0x1f, 0x1f, 0x20, 0x23);

	write_reg(par, 0x29);
	return 0;
}

/**
 * set_var() - apply LCD properties like rotation and BGR mode
 *
 * @par: FBTFT parameter object
 *
 * Return: 0 on success, < 0 if error occurred.
 */
static int set_var(struct fbtft_par *par)
{
	u8 madctl_par = 0;

	if (par->bgr)
		madctl_par |= MADCTL_BGR;
	switch (par->info->var.rotate) {
	case 0:
		break;
	case 90:
		madctl_par |= (MADCTL_MV | MADCTL_MY);
		break;
	case 180:
		madctl_par |= (MADCTL_MX | MADCTL_MY);
		break;
	case 270:
		madctl_par |= (MADCTL_MV | MADCTL_MX);
		break;
	default:
		return -EINVAL;
	}
	write_reg(par, MIPI_DCS_SET_ADDRESS_MODE, madctl_par);
	return 0;
}

/**
 * set_gamma() - set gamma curves
 *
 * @par: FBTFT parameter object
 * @curves: gamma curves
 *
 * Before the gamma curves are applied, they are preprocessed with a bitmask
 * to ensure syntactically correct input for the display controller.
 * This implies that the curves input parameter might be changed by this
 * function and that illegal gamma values are auto-corrected and not
 * reported as errors.
 *
 * Return: 0 on success, < 0 if error occurred.
 */
static int set_gamma(struct fbtft_par *par, u32 *curves)
{
	int i;
	int j;
	int c; /* curve index offset */

	/*
	 * Bitmasks for gamma curve command parameters.
	 * The masks are the same for both positive and negative voltage
	 * gamma curves.
	 */
	static const u8 gamma_par_mask[] = {
		0xFF, /* V63[3:0], V0[3:0]*/
		0x3F, /* V1[5:0] */
		0x3F, /* V2[5:0] */
		0x1F, /* V4[4:0] */
		0x1F, /* V6[4:0] */
		0x3F, /* J0[1:0], V13[3:0] */
		0x7F, /* V20[6:0] */
		0x77, /* V36[2:0], V27[2:0] */
		0x7F, /* V43[6:0] */
		0x3F, /* J1[1:0], V50[3:0] */
		0x1F, /* V57[4:0] */
		0x1F, /* V59[4:0] */
		0x3F, /* V61[5:0] */
		0x3F, /* V62[5:0] */
	};

	for (i = 0; i < par->gamma.num_curves; i++) {
		c = i * par->gamma.num_values;
		for (j = 0; j < par->gamma.num_values; j++)
			curves[c + j] &= gamma_par_mask[j];
		write_reg(par, PVGAMCTRL + i,
			  curves[c + 0],  curves[c + 1],  curves[c + 2],
			  curves[c + 3],  curves[c + 4],  curves[c + 5],
			  curves[c + 6],  curves[c + 7],  curves[c + 8],
			  curves[c + 9],  curves[c + 10], curves[c + 11],
			  curves[c + 12], curves[c + 13]);
	}
	return 0;
}

/**
 * blank() - blank the display
 *
 * @par: FBTFT parameter object
 * @on: whether to enable or disable blanking the display
 *
 * Return: 0 on success, < 0 if error occurred.
 */
static int blank(struct fbtft_par *par, bool on)
{
	if (on)
		write_reg(par, MIPI_DCS_SET_DISPLAY_OFF);
	else
		write_reg(par, MIPI_DCS_SET_DISPLAY_ON);
	return 0;
}

static void write_register(struct fbtft_par *par, int len, ...)
{
	va_list args;
	int i;

	va_start(args, len);
	if (len == 1) {
		/*only write command*/
		for (i = 0; i < len; i++)
			par->buf[i] = va_arg(args, unsigned int);

		/* keep DC low for all command bytes to transfer */
		fbtft_write_buf_dc(par, par->buf, len, 0);
	} else if (len > 1) {
		/*write command and data*/
		for (i = 0; i < len; i++)
			par->buf[i] = va_arg(args, unsigned int);

		/* keep DC low for all command bytes to transfer */
		fbtft_write_buf_dc(par, par->buf, 1, 0);

		/* keep DC high for all data bytes to transfer */
		fbtft_write_buf_dc(par, par->buf + 1, len - 1, 1);
	}
	va_end(args);
}

static struct fbtft_display display = {
	.regwidth = 8,
	.width = 240,
	.height = 320,
	.gamma_num = 2,
	.gamma_len = 14,
	.gamma = DEFAULT_GAMMA,
	.fbtftops = {
		.init_display = init_display,
		.set_var = set_var,
		.set_gamma = set_gamma,
		.blank = blank,
		.write_register = write_register,
	},
};

FBTFT_REGISTER_DRIVER(DRVNAME, "sitronix,st7789v", &display);

MODULE_ALIAS("spi:" DRVNAME);
MODULE_ALIAS("platform:" DRVNAME);
MODULE_ALIAS("spi:st7789v");
MODULE_ALIAS("platform:st7789v");

MODULE_DESCRIPTION("FB driver for the ST7789V LCD Controller");
MODULE_AUTHOR("Dennis Menschel");
MODULE_LICENSE("GPL");
