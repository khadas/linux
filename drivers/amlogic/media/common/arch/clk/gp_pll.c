// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/atomic.h>
#include <linux/slab.h>
#include <linux/export.h>
#include <linux/kthread.h>
#include <linux/semaphore.h>

#include <linux/amlogic/media/clk/gp_pll.h>

static DEFINE_MUTEX(gp_mutex);

static struct list_head gp_pll_user_list = LIST_HEAD_INIT(gp_pll_user_list);
static DEFINE_SEMAPHORE(sem);

static void gp_pll_user_consolidate(void)
{
	struct gp_pll_user_handle_s *pos, *grant, *request;

	mutex_lock(&gp_mutex);

	grant = NULL;
	request = NULL;

	list_for_each_entry(pos, &gp_pll_user_list, list) {
		if (!request &&
		    (pos->flag & GP_PLL_USER_FLAG_REQUEST) &&
		    ((pos->flag & GP_PLL_USER_FLAG_GRANTED) == 0)) {
			request = pos;
		}

		if (!grant &&
		    (pos->flag & GP_PLL_USER_FLAG_GRANTED)) {
			grant = pos;
		}
	}

	if (request) {
		if (!grant) {
			request->flag |= GP_PLL_USER_FLAG_GRANTED;

			if (request->callback)
				request->callback(request,
					GP_PLL_USER_EVENT_GRANT);
		} else if (grant->priority > request->priority) {
			if (grant->callback)
				grant->callback(grant,
					GP_PLL_USER_EVENT_YIELD);
		}
	}

	mutex_unlock(&gp_mutex);
}

struct gp_pll_user_handle_s *gp_pll_user_register
	(const char *name,
	u32 priority,
	int (*callback)(struct gp_pll_user_handle_s *, int))
{
	struct gp_pll_user_handle_s *user, *pos;

	user = (struct gp_pll_user_handle_s *)
		kmalloc(sizeof(struct gp_pll_user_handle_s), GFP_KERNEL);

	if (!user)
		return NULL;

	user->name = name;
	user->priority = priority;
	user->callback = callback;
	user->flag = 0;

	mutex_lock(&gp_mutex);

	list_for_each_entry(pos, &gp_pll_user_list, list) {
		if (pos->priority > priority)
			break;
	}

	list_add_tail(&user->list, &pos->list);

	mutex_unlock(&gp_mutex);

	return user;
}
EXPORT_SYMBOL(gp_pll_user_register);

void gp_pll_user_unregister(struct gp_pll_user_handle_s *user)
{
	mutex_lock(&gp_mutex);

	list_del(&user->list);

	mutex_unlock(&gp_mutex);

	if (user->flag & GP_PLL_USER_FLAG_GRANTED)
		up(&sem);
	kfree(user);
}
EXPORT_SYMBOL(gp_pll_user_unregister);

void gp_pll_request(struct gp_pll_user_handle_s *user)
{
	if (user->flag & GP_PLL_USER_FLAG_GRANTED)
		return;

	user->flag = GP_PLL_USER_FLAG_REQUEST;

	up(&sem);
}
EXPORT_SYMBOL(gp_pll_request);

void gp_pll_release(struct gp_pll_user_handle_s *user)
{
	if (user->flag & GP_PLL_USER_FLAG_GRANTED) {
		user->flag = 0;

		up(&sem);
	}

	user->flag = 0;
}
EXPORT_SYMBOL(gp_pll_release);

static int gp_pll_thread(void *data)
{
	while (down_interruptible(&sem) == 0)
		gp_pll_user_consolidate();

	return 0;
}

int __init gp_pll_init(void)
{
	kthread_run(gp_pll_thread, NULL, "gp_pll");

	return 0;
}

#ifndef MODULE
fs_initcall(gp_pll_init);
#endif
