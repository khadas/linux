/*
 * drivers/input/touchscreen/fake_ts.c
 *
 * Fake TouchScreen driver.
 *
 * Copyright (c) 2018  Wesion.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 *
 */
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/hrtimer.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/async.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/input/mt.h>

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#define SCREEN_MAX_X    480
#define SCREEN_MAX_Y    800
#define PRESS_MAX       255

#define FAKE_TS_NAME	"fake_ts"
#define MAX_CONTACTS 5

#define KEY_MIN_X	800
#define KEY_NUM 	4


#ifndef ABS_MT_TOUCH_MAJOR
#define ABS_MT_TOUCH_MAJOR	0x30	/* touching ellipse */
#define ABS_MT_TOUCH_MINOR	0x31	/* (omit if circular) */
#define ABS_MT_WIDTH_MAJOR	0x32	/* approaching ellipse */
#define ABS_MT_WIDTH_MINOR	0x33	/* (omit if circular) */
#define ABS_MT_ORIENTATION	0x34	/* Ellipse orientation */
#define ABS_MT_POSITION_X	  0x35	/* Center X ellipse position */
#define ABS_MT_POSITION_Y	  0x36	/* Center Y ellipse position */
#define ABS_MT_TOOL_TYPE	  0x37	/* Type of touching device */
#define ABS_MT_BLOB_ID		  0x38	/* Group set of pkts as blob */
#endif /* ABS_MT_TOUCH_MAJOR */

struct point_data {
	u8 status;
	u8 id;
	u16 x;
	u16 y;
};

struct ts_event {
  u16  touch_point;
  struct point_data point[5];
};

struct fake_ts_dev {
	struct input_dev	*input_dev;
	struct ts_event		event;
};

static struct fake_ts_dev *g_dev;

static int fake_ts_probe(void)
{
	struct fake_ts_dev *fake_ts;
	struct input_dev *input_dev;
	int err = 0;

	fake_ts = (struct fake_ts_dev *)kzalloc(sizeof(*fake_ts), GFP_KERNEL);
	if (!fake_ts)	{
		err = -ENOMEM;
		goto exit_alloc_data_failed;
	}

	input_dev = input_allocate_device();
	if (!input_dev) {
		err = -ENOMEM;
		printk("failed to allocate input device\n");
		goto exit_input_dev_alloc_failed;
	}

	fake_ts->input_dev = input_dev;

	__set_bit(EV_ABS, input_dev->evbit);
	__set_bit(EV_KEY, input_dev->evbit);
	__set_bit(EV_REP,  input_dev->evbit);
	__set_bit(INPUT_PROP_DIRECT, input_dev->propbit);
	set_bit(ABS_MT_POSITION_X, input_dev->absbit);
	set_bit(ABS_MT_POSITION_Y, input_dev->absbit);
	set_bit(ABS_MT_TOUCH_MAJOR, input_dev->absbit);
	set_bit(ABS_MT_WIDTH_MAJOR, input_dev->absbit);

//	input_mt_init_slots(input_dev, MAX_CONTACTS);


	input_set_abs_params(input_dev,ABS_MT_POSITION_X, 0, SCREEN_MAX_X, 0, 0);
	input_set_abs_params(input_dev,ABS_MT_POSITION_Y, 0, SCREEN_MAX_Y, 0, 0);
	input_set_abs_params(input_dev,ABS_MT_TOUCH_MAJOR, 0, PRESS_MAX, 0, 0);
	input_set_abs_params(input_dev,ABS_MT_WIDTH_MAJOR, 0, 200, 0, 0);

	input_dev->name		= FAKE_TS_NAME;
	err = input_register_device(input_dev);
	if (err) {
		printk("fake_ts_probe: failed to register input device\n");
		goto exit_input_register_device_failed;
	}

	g_dev = fake_ts;

	printk("=======fake_ts_probe=======\n");

    return 0;

exit_input_register_device_failed:
	input_free_device(input_dev);
exit_input_dev_alloc_failed:
	kfree(fake_ts);
exit_alloc_data_failed:
	return err;
}

static int  fake_ts_remove(void)
{
	input_unregister_device(g_dev->input_dev);
	kfree(g_dev);

	return 0;
}


static int __init fake_ts_init(void)
{
	return fake_ts_probe();
}

static void __exit fake_ts_exit(void)
{
	fake_ts_remove();

	return;
}

module_init(fake_ts_init);
module_exit(fake_ts_exit);

MODULE_AUTHOR("Nick <nick@khadas.com>");
MODULE_DESCRIPTION("Fake TouchScreen driver");
MODULE_LICENSE("GPL");
