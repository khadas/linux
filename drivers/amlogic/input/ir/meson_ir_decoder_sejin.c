// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/bitrev.h>
#include <linux/module.h>
#include "meson_ir_main.h"

#define BITS_PER_INT			32

#define SEJIN_BIT_LEADER		8
#define SEJIN_BIT_UNIT			4
#define SEJIN_BIT_MAP_MAX		(SEJIN_BIT_LEADER + SEJIN_BIT_UNIT * 32)

/* unit 10us */
#define SEJIN_TIME_UNIT			30
#define SEJIN_TIME_LEADER_1		100
#define SEJIN_TIME_FRAME_MAX		400

#define SEJIN_FRAME_HIGH_BIT		(4 + 16)

#define SEJIN_BIT_SPEC_LEADER		(0x47 << 1)
#define SEJIN_BIT_SPEC_00		0x1
#define SEJIN_BIT_SPEC_01		0x4
#define SEJIN_BIT_SPEC_10		0x2
#define SEJIN_BIT_SPEC_11		0x8

enum sejin_state {
	SEJIN_STATE_STOP = 0,
	SEJIN_STATE_RESET,
};

struct sejin_dec {
	u32 raw_bitmap[3];
	u8  raw_bitmap_cnt;
	u8  bit_1_cnt;
	u8  decode_state;
};

static u8 meson_ir_sejin_get_pulse_cnt(u16 duration)
{
	u8 pulse_cnt = duration / SEJIN_TIME_UNIT;

	if ((duration % SEJIN_TIME_UNIT) > (SEJIN_TIME_UNIT / 2))
		pulse_cnt++;

	return pulse_cnt;
}

static inline u32 reverse_int(u32 x)
{
	x = (((x & 0xaaaaaaaa) >> 1) | ((x & 0x55555555) << 1));
	x = (((x & 0xcccccccc) >> 2) | ((x & 0x33333333) << 2));
	x = (((x & 0xf0f0f0f0) >> 4) | ((x & 0x0f0f0f0f) << 4));
	x = (((x & 0xff00ff00) >> 8) | ((x & 0x00ff00ff) << 8));

	return ((x >> 16) | (x << 16));
}

static u32 meson_ir_sejin_handle_bitmap(struct sejin_dec *data)
{
	int i;
	u8 bit_spec, bit_code;
	u8 bitmap_pos, bitmap_offset;
	u32 framecode = 0;

	if ((data->raw_bitmap[0] & 0xFF) != SEJIN_BIT_SPEC_LEADER)
		return -EINVAL;

	for (i = SEJIN_BIT_LEADER; i <= (SEJIN_BIT_MAP_MAX / 2);
	     i += SEJIN_BIT_UNIT) {
		bitmap_pos = i / BITS_PER_INT;
		bitmap_offset = i % BITS_PER_INT;
		bit_spec = (data->raw_bitmap[bitmap_pos] >>
			    bitmap_offset) & 0xF;

		switch (bit_spec) {
		case SEJIN_BIT_SPEC_00:
			bit_code = 0x0;
			break;
		case SEJIN_BIT_SPEC_01:
			bit_code = 0x1;
			break;
		case SEJIN_BIT_SPEC_10:
			bit_code = 0x2;
			break;
		case SEJIN_BIT_SPEC_11:
			bit_code = 0x3;
			break;
		default:
			return -EINVAL;
		}
		framecode |= (bit_code << ((i - 8) / 2));
	}
	return reverse_int(framecode);
}

static void meson_ir_sejin_store_bitmap(struct sejin_dec *data, u8 bit_cnt)
{
	u8 bit_mask = (0x1 << bit_cnt) - 1;
	u8 bitmap_pos = data->raw_bitmap_cnt / BITS_PER_INT;
	u8 bitmap_offset = data->raw_bitmap_cnt % BITS_PER_INT;

	data->raw_bitmap[bitmap_pos] |= (bit_mask << bitmap_offset);

	if ((bitmap_offset + bit_cnt) > BITS_PER_INT)
		data->raw_bitmap[bitmap_pos + 1] |=
				(bit_mask >> (BITS_PER_INT - bitmap_offset));
}

static void meson_ir_sejin_decode_reset(struct sejin_dec *data)
{
	memset(data, 0, sizeof(struct sejin_dec));
	//4-bit align, reserve bit 0
	data->raw_bitmap_cnt = 1;
	data->decode_state = SEJIN_STATE_RESET;
}

/**
 * meson_ir_sejin_decode() - Decode one SEJIN pulse or space
 * @dev:	the struct rc_dev descriptor of the device
 * @duration:	the struct ir_raw_event descriptor of the pulse/space
 *
 * This function returns -EINVAL if the pulse violates the state machine
 */
static int meson_ir_sejin_decode(struct meson_ir_dev *dev,
				 struct meson_ir_raw_event ev, void *data_dec)
{
	struct meson_ir_chip *chip = (struct meson_ir_chip *)dev->platform_data;
	struct sejin_dec *data = data_dec;
	u8 bit_cnt;
	u32 frame_code;
	u8 pulse = ev.duration >> 31;
	u32 duration = (ev.duration & GENMASK(30, 0)) / 1000 / 10;

	if (duration > SEJIN_TIME_FRAME_MAX) {
		meson_ir_sejin_decode_reset(data);
		return 0;
	}

	if (data->decode_state == SEJIN_STATE_STOP)
		if (pulse &&
		    (duration % SEJIN_TIME_UNIT) < (SEJIN_TIME_UNIT / 2))
			meson_ir_sejin_decode_reset(data);

	if (data->decode_state == SEJIN_STATE_STOP)
		return -EINVAL;

	bit_cnt = meson_ir_sejin_get_pulse_cnt(duration);

	if (pulse) {
		data->bit_1_cnt += bit_cnt;
		meson_ir_sejin_store_bitmap(data, bit_cnt);
	}
	data->raw_bitmap_cnt += bit_cnt;

	if (data->bit_1_cnt >= SEJIN_FRAME_HIGH_BIT) {
		data->decode_state = SEJIN_STATE_STOP;
		frame_code = meson_ir_sejin_handle_bitmap(data);
		dev->set_custom_code(dev, frame_code);

		meson_ir_keydown(dev, (frame_code >> 8) & 0xff,
				 chip->decode_status);

		return 0;
	}

	if (data->raw_bitmap_cnt >= SEJIN_BIT_MAP_MAX)
		data->decode_state = SEJIN_STATE_STOP;

	return 0;
}

static struct meson_ir_raw_handler sejin_handler = {
	.protocols	= REMOTE_TYPE_RAW_SEJIN,
	.decode		= meson_ir_sejin_decode,
};

int meson_ir_sejin_decode_init(void)
{
	struct sejin_dec *xdec = kzalloc(sizeof(*xdec), GFP_KERNEL);

	sejin_handler.data =  xdec;
	if (!sejin_handler.data)
		return -ENOMEM;

	meson_ir_raw_handler_register(&sejin_handler);

	return 0;
}

void meson_ir_sejin_decode_exit(void)
{
	meson_ir_raw_handler_unregister(&sejin_handler);
	if (!sejin_handler.data)
		kfree(sejin_handler.data);
}
