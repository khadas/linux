// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/types.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/errno.h>
#include <asm/irq.h>
#include <linux/io.h>

#include <linux/major.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/pinctrl/consumer.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include "meson_ir_core.h"
#include "meson_ir_main.h"

static void meson_ir_do_keyup(struct meson_ir_dev *dev)
{
	int cnt;
	struct meson_ir_chip *chip = (struct meson_ir_chip *)dev->platform_data;
	struct meson_ir_map_tab_list *ct = chip->cur_tab;

	if (!ct)
		return;

	if (DECIDE_VENDOR_TA_ID) {
		for (cnt = 0; cnt <= chip->input_cnt; cnt++) {
			if (chip->search_id[cnt] == ct->tab.vendor)
				break;
		}
		if (cnt > chip->input_cnt) {
			dev_err(chip->dev, "vendor ID Configuration error\n");
			dev_err(chip->dev, "vendor = %x, product = %x, version = %x\n",
					ct->tab.vendor, ct->tab.product, ct->tab.version);
			return;
		}
		input_report_key(dev->input_device_ots[cnt], dev->last_keycode, 0);
		input_sync(dev->input_device_ots[cnt]);
	} else {
		input_report_key(dev->input_device, dev->last_keycode, 0);
		input_sync(dev->input_device);
	}
	dev->keypressed = 0;
	dev->last_scancode = -1;
	meson_ir_dbg(dev, "keyup!!\n");
}

void meson_ir_timer_keyup(struct timer_list *t)
{
	struct meson_ir_dev *dev = from_timer(dev, t, timer_keyup);
	unsigned long flags;

	if (!dev->keypressed)
		return;
	spin_lock_irqsave(&dev->keylock, flags);
	if (dev->is_next_repeat(dev)) {
		dev->keyup_jiffies = jiffies +
			msecs_to_jiffies(dev->keyup_delay);
		mod_timer(&dev->timer_keyup, dev->keyup_jiffies);
		dev->wait_next_repeat = 1;
		meson_ir_dbg(dev, "wait for repeat\n");
	} else {
		if (time_is_before_eq_jiffies(dev->keyup_jiffies))
			meson_ir_do_keyup(dev);
		dev->wait_next_repeat = 0;
	}
	spin_unlock_irqrestore(&dev->keylock, flags);
}

static void meson_ir_do_keydown(struct meson_ir_dev *dev, int scancode,
				u32 keycode)
{
	int cnt;
	struct meson_ir_chip *chip = (struct meson_ir_chip *)dev->platform_data;
	struct meson_ir_map_tab_list *ct = chip->cur_tab;

	if (!ct)
		return;
	meson_ir_dbg(dev, "keypressed=0x%x\n", dev->keypressed);

	if (dev->keypressed)
		meson_ir_do_keyup(dev);

	if (keycode != KEY_RESERVED) {
		dev->keypressed = 1;
		dev->last_scancode = scancode;
		dev->last_keycode = keycode;
		if (DECIDE_VENDOR_TA_ID) {
			for (cnt = 0; cnt <= chip->input_cnt; cnt++) {
				if (chip->search_id[cnt] == ct->tab.vendor)
					break;
			}
			if (cnt > chip->input_cnt) {
				dev_err(chip->dev, "vendor ID configuration error\n");
				dev_err(chip->dev, "vendor = %x, product = %x, version = %x\n",
						ct->tab.vendor, ct->tab.product, ct->tab.version);
				return;
			}
			input_report_key(dev->input_device_ots[cnt], keycode, 1);
			input_sync(dev->input_device_ots[cnt]);
		} else {
			input_report_key(dev->input_device, keycode, 1);
			input_sync(dev->input_device);
		}
		meson_ir_dbg(dev, "report key!!\n");
	} else {
		dev_err(dev->dev, "no valid key to handle");
	}
}

void meson_ir_keydown(struct meson_ir_dev *dev, int scancode, int status)
{
	unsigned long flags;
	u32 keycode;

	if (dev->led_blink)
		led_trigger_blink_oneshot(dev->led_feedback, &dev->delay_on,
					  &dev->delay_off, 0);

	if (status != IR_STATUS_REPEAT) {
		if (dev->is_valid_custom && !dev->is_valid_custom(dev)) {
			dev_err(dev->dev, "invalid custom:0x%x\n",
				dev->cur_hardcode);
			return;
		}
	}
	spin_lock_irqsave(&dev->keylock, flags);
	/**
	 *only a few keys which be set in key map table support
	 *report relative axes event in mouse mode, other keys
	 *will continue to report key event.
	 */
	if (status == IR_STATUS_NORMAL || status == IR_STATUS_REPEAT) {
		/*to report relative axes event*/
		if (dev->report_rel(dev, scancode, status) == 0) {
			spin_unlock_irqrestore(&dev->keylock, flags);
			return;
		}
	}

	if (status == IR_STATUS_NORMAL) {
		keycode = dev->getkeycode(dev, scancode);
		if (keycode == KEY_POWER)
			pm_wakeup_hard_event(dev->dev);
		meson_ir_do_keydown(dev, scancode, keycode);
	}

	if (dev->keypressed) {
		dev->wait_next_repeat = 0;
		dev->keyup_jiffies = jiffies +
			msecs_to_jiffies(dev->keyup_delay);
		mod_timer(&dev->timer_keyup, dev->keyup_jiffies);
	}
	spin_unlock_irqrestore(&dev->keylock, flags);
}
EXPORT_SYMBOL(meson_ir_keydown);

void meson_ir_input_ots_configure(struct meson_ir_dev *dev, int cnt0)
{
	int i;

	for (i = KEY_RESERVED; i < BTN_MISC; i++)
		input_set_capability(dev->input_device_ots[cnt0], EV_KEY, i);

	for (i = KEY_OK; i < BTN_TRIGGER_HAPPY; i++)
		input_set_capability(dev->input_device_ots[cnt0], EV_KEY, i);

	for (i = BTN_MOUSE; i < BTN_SIDE; i++)
		input_set_capability(dev->input_device_ots[cnt0], EV_KEY, i);

	input_set_capability(dev->input_device_ots[cnt0], EV_REL, REL_X);
	input_set_capability(dev->input_device_ots[cnt0], EV_REL, REL_Y);
	input_set_capability(dev->input_device_ots[cnt0], EV_REL, REL_WHEEL);
}
EXPORT_SYMBOL(meson_ir_input_ots_configure);

void meson_ir_input_configure(struct meson_ir_dev *dev)
{
	int i;

	for (i = KEY_RESERVED; i < BTN_MISC; i++)
		input_set_capability(dev->input_device, EV_KEY, i);

	for (i = KEY_OK; i < BTN_TRIGGER_HAPPY; i++)
		input_set_capability(dev->input_device, EV_KEY, i);

	for (i = BTN_MOUSE; i < BTN_SIDE; i++)
		input_set_capability(dev->input_device, EV_KEY, i);

	input_set_capability(dev->input_device, EV_REL, REL_X);
	input_set_capability(dev->input_device, EV_REL, REL_Y);
	input_set_capability(dev->input_device, EV_REL, REL_WHEEL);
}
EXPORT_SYMBOL(meson_ir_input_configure);
