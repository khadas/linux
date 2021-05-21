/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __IR_CORE_MESON_H__
#define __IR_CORE_MESON_H__

#undef pr_fmt
#define pr_fmt(fmt) "meson-ir:" fmt

#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/kfifo.h>
#include <linux/device.h>
#include <dt-bindings/input/meson_ir.h>
#include <linux/leds.h>

#define MULTI_IR_TYPE_MASK(type) (type & 0xff)  /*8bit*/
#define LEGACY_IR_TYPE_MASK(type) ((type >> 8) & 0xff) /*8bit*/
/*bit[7] identify whether software decode or not*/
#define MULTI_IR_SOFTWARE_DECODE(type) ((MULTI_IR_TYPE_MASK(type) >> 7) == 0x1)
#define ENABLE_LEGACY_IR(type) (LEGACY_IR_TYPE_MASK(type) == 0xff)
/*default led blink frequency*/
#define DEFAULT_LED_BLINK_FRQ	100
#define MAX_LEARNED_PULSE	256
#define meson_ir_dbg(dev, format, arg...)           \
do {                                                \
	if ((dev)->debug_enable)                    \
		pr_info(format, ##arg);             \
} while (0)

enum ir_status {
	IR_STATUS_NORMAL		= 0 << 0,
	IR_STATUS_REPEAT		= 1 << 0,
	IR_STATUS_CUSTOM_ERROR		= 1 << 1,
	IR_STATUS_DATA_ERROR		= 1 << 2,
	IR_STATUS_FRAME_ERROR		= 1 << 3,
	IR_STATUS_CHECKSUM_ERROR	= 1 << 4,
	IR_STATUS_CUSTOM_DATA		= 1 << 5
};

enum raw_event_type {
	RAW_SPACE        = (1 << 0),
	RAW_PULSE        = (1 << 1),
	RAW_START_EVENT  = (1 << 2),
	RAW_STOP_EVENT   = (1 << 3),
};

struct pulse_group {
	int len;
	/*bit 30-0 durations, bit31: level*/
	u32 pulse[0];
};

struct meson_ir_raw_handle;
struct meson_ir_dev {
	struct device *dev;
	struct input_dev *input_device;
	struct input_dev *input_device_ots[3];
	struct list_head reg_list;
	struct list_head aml_list;
	struct meson_ir_raw_handle *raw;
	struct led_trigger *led_feedback;
	struct timer_list timer_keyup;
	struct pulse_group *pulses;

	spinlock_t keylock; /*protect runtime data*/

	unsigned long delay_on;
	unsigned long delay_off;
	unsigned long keyup_jiffies;
	unsigned long keyup_delay;

	u32 keypressed;
	u32 last_scancode;
	u32 last_keycode;
	u32 cur_hardcode;
	u32 cur_customcode;
	u32 repeat_time;
	u32 max_frame_time;

	int wait_next_repeat;
	int rc_type;
	int led_blink;
	int max_learned_pulse;
	int protocol;

	void *platform_data;

	u32 (*getkeycode)(struct meson_ir_dev *ir_dev, u32 scancode);
	int (*report_rel)(struct meson_ir_dev *ir_dev, u32 scancode,
			  int status);
	bool (*set_custom_code)(struct meson_ir_dev *ir_dev, u32 scancode);
	bool (*is_valid_custom)(struct meson_ir_dev *ir_dev);
	bool (*is_next_repeat)(struct meson_ir_dev *ir_dev);

	u8 debug_enable;
	u8 enable;
	u8 ir_learning_on;
};

struct meson_ir_raw_handle {
	struct list_head list;
	struct meson_ir_dev *dev;
	struct task_struct *thread;
	struct kfifo_rec_ptr_1 kfifo;/* fifo for the pulse/space durations */

	spinlock_t lock; /*lock used to protect data in soft decode*/

	enum raw_event_type last_type;
	unsigned long jiffies_old;
	unsigned long repeat_time;
	unsigned long max_frame_time;
};

struct meson_ir_map_table {
	u32 scancode;
	u32 keycode;
};

struct meson_ir_map {
	struct meson_ir_map_table *scan;
	int rc_type;
	const char *name;
	u32 size;
};

struct meson_ir_map_list {
	struct list_head list;
	struct meson_ir_map map;
};

struct meson_ir_raw_event {
	u32 duration;
	u32 pulse:1;
	u32 reset:1;
	u32 timeout:1;
};

#define DEFINE_REMOTE_RAW_EVENT(event) \
	struct meson_ir_raw_event event = { \
		.duration = 0, \
		.pulse = 0, \
		.reset = 0, \
		.timeout = 0}

struct meson_ir_raw_handler {
	struct list_head list;

	int protocols;
	void *data;
	int (*decode)(struct meson_ir_dev *dev, struct meson_ir_raw_event event,
		      void *data_dec);
};

/* macros for IR decoders */
static inline bool geq_margin(unsigned int d1, unsigned int d2,
			      unsigned int margin)
{
	return d1 > (d2 - margin);
}

static inline bool eq_margin(unsigned int d1, unsigned int d2,
			     unsigned int margin)
{
	return (d1 > (d2 - margin)) && (d1 < (d2 + margin));
}

static inline bool is_transition(struct meson_ir_raw_event *x,
				 struct meson_ir_raw_event *y)
{
	return x->pulse != y->pulse;
}

static inline void decrease_duration(struct meson_ir_raw_event *ev,
				     unsigned int duration)
{
	if (duration > ev->duration)
		ev->duration = 0;
	else
		ev->duration -= duration;
}

void meson_ir_input_configure(struct meson_ir_dev *dev);
void meson_ir_keydown(struct meson_ir_dev *dev, int scancode, int status);
int meson_ir_raw_event_store(struct meson_ir_dev *dev,
			     struct meson_ir_raw_event *ev);
int meson_ir_raw_event_register(struct meson_ir_dev *dev);
void meson_ir_raw_event_unregister(struct meson_ir_dev *dev);
int meson_ir_raw_handler_register(struct meson_ir_raw_handler *handler);
void meson_ir_raw_handler_unregister(struct meson_ir_raw_handler *handler);
void meson_ir_raw_event_handle(struct meson_ir_dev *dev);
int meson_ir_raw_event_store_edge(struct meson_ir_dev *dev,
				  enum raw_event_type type, u32 duration);
#endif
