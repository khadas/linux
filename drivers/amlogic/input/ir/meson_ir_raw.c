// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/export.h>
#include <linux/kthread.h>
#include <linux/mutex.h>
#include <linux/kmod.h>
#include <linux/sched.h>
#include <linux/freezer.h>
#include <linux/jiffies.h>
#include "meson_ir_main.h"

static DEFINE_MUTEX(meson_ir_raw_handler_lock);
static LIST_HEAD(meson_ir_raw_handler_list);
static LIST_HEAD(meson_ir_raw_client_list);

#define MAX_REMOTE_EVENT_SIZE      512

static int meson_ir_raw_event_thread(void *data)
{
	DEFINE_REMOTE_RAW_EVENT(ev);
	struct meson_ir_raw_handler *handler;
	struct meson_ir_raw_handle *raw = (struct meson_ir_raw_handle *)data;
	int retval;

	while (!kthread_should_stop()) {
		/*spin_lock_irq(&raw->lock); */
		retval = kfifo_len(&raw->kfifo);

		if (retval < sizeof(ev)) {
			set_current_state(TASK_INTERRUPTIBLE);

			if (kthread_should_stop())
				set_current_state(TASK_RUNNING);

			/* spin_unlock_irq(&raw->lock); */
			schedule();
			continue;
		}

		retval = kfifo_out(&raw->kfifo, &ev, sizeof(ev));
		/*spin_unlock_irq(&raw->lock); */

		mutex_lock(&meson_ir_raw_handler_lock);
		list_for_each_entry(handler, &meson_ir_raw_handler_list, list)
			if (raw->dev->rc_type == handler->protocols)
				handler->decode(raw->dev, ev, handler->data);
		mutex_unlock(&meson_ir_raw_handler_lock);
	}

	return 0;
}

int meson_ir_raw_event_store(struct meson_ir_dev *dev,
			     struct meson_ir_raw_event *ev)
{
	if (!dev->raw)
		return -EINVAL;

	if (kfifo_in(&dev->raw->kfifo, ev, sizeof(*ev)) != sizeof(*ev))
		return -ENOMEM;

	return 0;
}

int meson_ir_raw_event_store_edge(struct meson_ir_dev *dev,
				  enum raw_event_type type, u32 duration)
{
	DEFINE_REMOTE_RAW_EVENT(ev);
	int rc = 0;
	unsigned long timeout;

	if (!dev->raw)
		return -EINVAL;

	timeout = dev->raw->jiffies_old +
		msecs_to_jiffies(dev->raw->max_frame_time);

	if (time_after(jiffies, timeout) || !dev->raw->last_type)
		type |= RAW_START_EVENT;
	else
		ev.duration = duration;

	if (type & RAW_START_EVENT)
		ev.reset = true;
	else if (dev->raw->last_type & RAW_SPACE)
		ev.pulse = false;
	else if (dev->raw->last_type & RAW_PULSE)
		ev.pulse = true;
	else
		return 0;

	rc = meson_ir_raw_event_store(dev, &ev);

	dev->raw->last_type = type;
	dev->raw->jiffies_old = jiffies;
	return rc;
}

void meson_ir_raw_event_handle(struct meson_ir_dev *dev)
{
	/*unsigned long flags;*/

	if (!dev || !dev->raw)
		return;

	/*spin_lock_irqsave(&dev->raw->lock, flags);*/
	wake_up_process(dev->raw->thread);
	/*spin_unlock_irqrestore(&dev->raw->lock, flags);*/
}

int meson_ir_raw_event_register(struct meson_ir_dev *dev)
{
	int ret;

	dev_info(dev->dev, "meson ir raw event register\n");
	dev->raw = kzalloc(sizeof(*dev->raw), GFP_KERNEL);
	if (!dev->raw)
		return -ENOMEM;

	dev->raw->dev = dev;
	dev->raw->max_frame_time = dev->max_frame_time;

	dev->raw->jiffies_old = jiffies;

	ret = kfifo_alloc(&dev->raw->kfifo,
			  sizeof(struct meson_ir_raw_event) *
			  MAX_REMOTE_EVENT_SIZE, GFP_KERNEL);
	if (ret < 0)
		goto out;

	dev->raw->thread = kthread_run(meson_ir_raw_event_thread, dev->raw,
				       "ir-thread");

	if (IS_ERR(dev->raw->thread)) {
		ret = PTR_ERR(dev->raw->thread);
		goto err_alloc_thread;
	}
	mutex_lock(&meson_ir_raw_handler_lock);
	list_add_tail(&dev->raw->list, &meson_ir_raw_client_list);
	mutex_unlock(&meson_ir_raw_handler_lock);
	return 0;
err_alloc_thread:
	kfifo_free(&dev->raw->kfifo);
out:
	kfree(dev->raw);
	return ret;
}

void meson_ir_raw_event_unregister(struct meson_ir_dev *dev)
{
	if (!dev || !dev->raw)
		return;

	kthread_stop(dev->raw->thread);
	mutex_lock(&meson_ir_raw_handler_lock);
	list_del(&dev->raw->list);
	mutex_unlock(&meson_ir_raw_handler_lock);
}

int meson_ir_raw_handler_register(struct meson_ir_raw_handler *handler)
{
	mutex_lock(&meson_ir_raw_handler_lock);
	list_add_tail(&handler->list, &meson_ir_raw_handler_list);
	mutex_unlock(&meson_ir_raw_handler_lock);
	return 0;
}

void meson_ir_raw_handler_unregister(struct meson_ir_raw_handler *handler)
{
	mutex_lock(&meson_ir_raw_handler_lock);
	list_del(&handler->list);
	mutex_unlock(&meson_ir_raw_handler_lock);
}
