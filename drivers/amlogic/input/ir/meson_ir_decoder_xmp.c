// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/bitrev.h>
#include <linux/module.h>
#include "meson_ir_main.h"

#define	XMP_UNIT		136000 /* ns */
#define	XMP_LEADER		210000 /* ns */
#define	XMP_NIBBLE_PREFIX	760000 /* ns */
#define	XMP_HALFFRAME_SPACE	13800000 /* ns */

/* should be 80ms but not all duration suppliers can go that high */
#define	XMP_TRAILER_SPACE	20000000

enum xmp_state {
	STATE_INACTIVE,
	STATE_LEADER_PULSE,
	STATE_NIBBLE_SPACE,
};

struct nibble_win {
	u8 value;
	int min;
	int max;
};

struct xmp_dec {
	int state;
	unsigned int count;
	u32 durations[16];
};

static const struct nibble_win nibble_windows[] = {
	{0, 690000, 826000},   /*758000*/
	{1, 827000, 963000},   /*895000*/
	{2, 964000, 1100000},   /*1032000*/
	{3, 1110000, 1237000},   /*1169000*/
	{4, 1238000, 1374000},   /*1306000*/
	{5, 1375000, 1511000},   /*1443000*/
	{6, 1512000, 1648000},   /*1580000*/
	{7, 1649000, 1785000},   /*1717000*/
	{8, 1786000, 1922000},   /*1854000*/
	{9, 1923000, 2059000},   /*1991000*/
	{0xa, 2060000, 2196000}, /*2128000*/
	{0xb, 2197000, 2333000}, /*2265000*/
	{0xc, 2334000, 2470000}, /*2402000*/
	{0xd, 2471000, 2607000}, /*2539000*/
	{0xe, 2608000, 2744000}, /*2676000*/
	{0x0f, 2745000, 2881000}, /*2813000*/
	{0xff, 11800000, 16800000} /*13800000 ,half frame space*/
};

int meson_ir_decode_xmp(struct meson_ir_dev *dev,
			struct xmp_dec *data, struct meson_ir_raw_event *ev)
{
	int  i;
	u8 addr, subaddr, subaddr2, toggle, oem, obc1, obc2, sum1, sum2;
	u32 *n;
	u32 scancode;
	int custom_code;
	int nb = 0;

	if (data->count != 16) {
		meson_ir_dbg(dev, "rx TRAILER c=%d, d=%d\n", data->count,
			     ev->duration);
		data->state = STATE_INACTIVE;
		return -EINVAL;
	}

	n = data->durations;
	for (i = 0; i < 16; i++) {
		for (nb = 0; nb < 16; nb++) {
			if (n[i] >= nibble_windows[nb].min &&
			    n[i] <= nibble_windows[nb].max) {
				n[i] = nibble_windows[nb].value;
			}
		}
	}
	sum1 = (15 + n[0] + n[1] + n[2] + n[3] +
		n[4] + n[5] + n[6] + n[7]) % 16;
	sum2 = (15 + n[8] + n[9] + n[10] + n[11] +
		n[12] + n[13] + n[14] + n[15]) % 16;

	if (sum1 != 15 || sum2 != 15) {
		meson_ir_dbg(dev, "checksum err\n");
		data->state = STATE_INACTIVE;
		return -EINVAL;
	}

	subaddr	= n[0] << 4 | n[2];
	subaddr2 = n[8] << 4 | n[11];
	oem = n[4] << 4 | n[5];
	addr = n[6] << 4 | n[7];
	toggle = n[10];
	obc1 = n[12] << 4 | n[13];
	obc2 = n[14] << 4 | n[15];

	if (subaddr != subaddr2) {
		meson_ir_dbg(dev, "s1 != s2\n");
		data->state = STATE_INACTIVE;
		return -EINVAL;
	}
	scancode = obc1;
	custom_code =  oem << 8 | addr;
	meson_ir_dbg(dev, "custom_code=%d\n", custom_code);
	meson_ir_dbg(dev, "scancode=0x%x,t=%d\n", scancode, toggle);
	dev->set_custom_code(dev, custom_code);
	if (toggle == 0)
		meson_ir_keydown(dev, scancode, IR_STATUS_NORMAL);
	else
		meson_ir_keydown(dev, scancode, IR_STATUS_REPEAT);

	data->state = STATE_INACTIVE;
	return 0;
}

/**
 * meson_ir_xmp_decode() - Decode one XMP pulse or space
 * @dev:	the struct rc_dev descriptor of the device
 * @duration:	the struct ir_raw_event descriptor of the pulse/space
 *
 * This function returns -EINVAL if the pulse violates the state machine
 */
static int meson_ir_xmp_decode(struct meson_ir_dev *dev,
			       struct meson_ir_raw_event ev, void *data_dec)
{
	struct xmp_dec *data = data_dec;

	if (ev.reset) {
		data->state = STATE_INACTIVE;
		return 0;
	}

	meson_ir_dbg(dev, "dr:%d,s=%d, c=%d\n",
		     ev.duration, data->state, data->count);

	switch (data->state) {
	case STATE_INACTIVE:
		if (eq_margin(ev.duration, XMP_LEADER, XMP_UNIT / 2)) {
			data->count = 0;
			data->state = STATE_NIBBLE_SPACE;
		}

		return 0;

	case STATE_LEADER_PULSE:
		meson_ir_dbg(dev, "STATE_LEADER_PULSE\n");

		if (eq_margin(ev.duration, XMP_LEADER, XMP_UNIT / 2))
			data->state = STATE_NIBBLE_SPACE;

		if (data->count == 16)
			return meson_ir_decode_xmp(dev, data, &ev);
		return 0;

	case STATE_NIBBLE_SPACE:
		if (geq_margin(ev.duration,
			       XMP_TRAILER_SPACE, XMP_NIBBLE_PREFIX)) {
			return meson_ir_decode_xmp(dev, data, &ev);
		} else if (geq_margin(ev.duration, XMP_HALFFRAME_SPACE,
			  XMP_NIBBLE_PREFIX)) {
			/* Expect 8 or 16 nibble pulses. 16
			 * in case of 'final' frame
			 */
			if (data->count == 16) {
				/*
				 * TODO: for now go back to half frame position
				 *	 so trailer can be found and key press
				 *	 can be handled.
				 */
				meson_ir_dbg(dev, "over pulses\n");
				data->count = 8;
			} else if (data->count != 8) {
				meson_ir_dbg(dev, "half frame\n");
			}
			data->state = STATE_LEADER_PULSE;
			return 0;

		} else if (geq_margin(ev.duration,
			  XMP_NIBBLE_PREFIX, XMP_UNIT)) {
			/* store nibble raw data, decode after trailer */
			if (data->count == 16) {
				meson_ir_dbg(dev, "over pulses\n");
				data->state = STATE_INACTIVE;
				return -EINVAL;
			}
			data->durations[data->count] = ev.duration;
			data->count++;
			data->state = STATE_LEADER_PULSE;
			return 0;
		}

		break;
	}
	meson_ir_dbg(dev, "dec failed\n");

	data->state = STATE_INACTIVE;
	return -EINVAL;
}

static struct meson_ir_raw_handler xmp_handler = {
	.protocols	= REMOTE_TYPE_RAW_XMP_1,
	.decode		= meson_ir_xmp_decode,
};

int meson_ir_xmp_decode_init(void)
{
	struct xmp_dec *xdec = kzalloc(sizeof(*xdec), GFP_KERNEL);

	xmp_handler.data =  xdec;
	if (!xmp_handler.data)
		return -ENOMEM;

	meson_ir_raw_handler_register(&xmp_handler);

	return 0;
}

void meson_ir_xmp_decode_exit(void)
{
	meson_ir_raw_handler_unregister(&xmp_handler);
	if (!xmp_handler.data)
		kfree(xmp_handler.data);
}
