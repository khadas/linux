/*
 * drivers/amlogic/display/vout/vout_notify.c
 *
 * Copyright (C) 2015 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
*/


/* Linux Headers */
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/sched.h>

/* Amlogic Headers */
#include <linux/amlogic/vout/vout_notify.h>

/* Local Headers */
#include "vout_log.h"

static BLOCKING_NOTIFIER_HEAD(vout_notifier_list);
static DEFINE_MUTEX(vout_mutex);

static struct vout_module_s vout_module = {
	.vout_server_list = {
		&vout_module.vout_server_list,
		&vout_module.vout_server_list
	},
	.curr_vout_server = NULL,
};

/**
 *	vout_register_client - register a client notifier
 *	@nb: notifier block to callback on events
 */
int vout_register_client(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&vout_notifier_list, nb);
}
EXPORT_SYMBOL(vout_register_client);

/**
 *	vout_unregister_client - unregister a client notifier
 *	@nb: notifier block to callback on events
 */
int vout_unregister_client(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&vout_notifier_list, nb);
}
EXPORT_SYMBOL(vout_unregister_client);

/**
 * vout_notifier_call_chain - notify clients of fb_events
 *
 */
int vout_notifier_call_chain(unsigned long val, void *v)
{
	return blocking_notifier_call_chain(&vout_notifier_list, val, v);
}
EXPORT_SYMBOL_GPL(vout_notifier_call_chain);

/*
*interface export to client who want to get current vinfo.
*/
struct vinfo_s *get_current_vinfo(void)
{
	struct vinfo_s *info = NULL;
	unsigned int atomic_flag = in_interrupt();

	if (atomic_flag == 0)
		mutex_lock(&vout_mutex);

	if (vout_module.curr_vout_server) {
		BUG_ON(vout_module.curr_vout_server->op.get_vinfo == NULL);
		info = vout_module.curr_vout_server->op.get_vinfo();
	}
	if (info == NULL) /* avoid crash mistake */
		info = get_invalid_vinfo();

	if (atomic_flag == 0)
		mutex_unlock(&vout_mutex);

	return info;
}
EXPORT_SYMBOL(get_current_vinfo);

/*
*interface export to client who want to get current vmode.
*/
enum vmode_e get_current_vmode(void)
{
	const struct vinfo_s *info;
	enum vmode_e mode = VMODE_MAX;

	mutex_lock(&vout_mutex);

	if (vout_module.curr_vout_server) {
		BUG_ON(vout_module.curr_vout_server->op.get_vinfo == NULL);
		info = vout_module.curr_vout_server->op.get_vinfo();
		mode = info->mode;
	}

	mutex_unlock(&vout_mutex);

	return mode;
}
EXPORT_SYMBOL(get_current_vmode);

/* fps = 9600/duration/100 hz */
int get_vsource_fps(int duration)
{
	int fps = 6000;

	switch (duration) {
	case 1600:
		fps = 6000;
		break;
	case 1601:
	case 1602:
		fps = 5994;
		break;
	case 1920:
		fps = 5000;
		break;
	case 3200:
		fps = 3000;
		break;
	case 3203:
		fps = 2997;
		break;
	case 3840:
		fps = 2500;
		break;
	case 4000:
		fps = 2400;
		break;
	case 4004:
		fps = 2397;
		break;
	default:
		break;
	}
	return fps;
}
EXPORT_SYMBOL(get_vsource_fps);

/*
*interface export to client who want to notify about source frame rate.
*/
int set_vframe_rate_hint(int duration)
{
	int r = -1;
	struct vout_server_s  *p_server;

	/* mutex_lock(&vout_mutex); */
	list_for_each_entry(p_server, &vout_module.vout_server_list, list) {
		if ((p_server->op.set_vframe_rate_hint != NULL) &&
		    (p_server->op.set_vframe_rate_hint(duration) == 0)) {
			/* mutex_unlock(&vout_mutex); */
			return 0;
		}
	}

	/* mutex_unlock(&vout_mutex); */

	return r;
}
EXPORT_SYMBOL(set_vframe_rate_hint);

/*
*interface export to client who want to notify about source frame rate end.
*/
int set_vframe_rate_end_hint(void)
{
	int r = -1;
	struct vout_server_s  *p_server;

	/* mutex_lock(&vout_mutex); */
	list_for_each_entry(p_server, &vout_module.vout_server_list, list) {
		if ((p_server->op.set_vframe_rate_end_hint != NULL) &&
		    (p_server->op.set_vframe_rate_end_hint() == 0)) {
			/* mutex_unlock(&vout_mutex); */
			return 0;
		}
	}

	/* mutex_unlock(&vout_mutex); */

	return r;
}
EXPORT_SYMBOL(set_vframe_rate_end_hint);

/*
*interface export to client who want to notify about source fr_policy.
*/
int set_vframe_rate_policy(int policy)
{
	int r = -1;
	struct vout_server_s  *p_server;

	/* mutex_lock(&vout_mutex); */
	list_for_each_entry(p_server, &vout_module.vout_server_list, list) {
		if ((p_server->op.set_vframe_rate_policy != NULL) &&
		    (p_server->op.set_vframe_rate_policy(policy) == 0)) {
			/* mutex_unlock(&vout_mutex); */
			return 0;
		}
	}

	/* mutex_unlock(&vout_mutex); */

	return r;
}
EXPORT_SYMBOL(set_vframe_rate_policy);

/*
*interface export to client who want to notify about source fr_policy.
*/
int get_vframe_rate_policy(void)
{
	int r = -1;
	struct vout_server_s  *p_server;

	/* mutex_lock(&vout_mutex); */
	list_for_each_entry(p_server, &vout_module.vout_server_list, list) {
		if (p_server->op.get_vframe_rate_policy != NULL) {
			r = p_server->op.get_vframe_rate_policy();
			/* mutex_unlock(&vout_mutex); */
			return r;
		}
	}

	/* mutex_unlock(&vout_mutex); */

	return r;
}
EXPORT_SYMBOL(get_vframe_rate_policy);

#ifdef CONFIG_SCREEN_ON_EARLY
static int wake_up_flag;
void wakeup_early_suspend_proc(void)
{
	wake_up_flag = 1;
}
#endif
int vout_suspend(void)
{
	int ret = 0;
	struct vout_server_s  *p_server = vout_module.curr_vout_server;
#ifdef CONFIG_SCREEN_ON_EARLY
	wake_up_flag = 0;
	int i = 0;

	for (; i < 20; i++)
		if (wake_up_flag)
			break;
		else
			msleep(100);

	wake_up_flag = 0;
#endif
	mutex_lock(&vout_mutex);

	if (p_server) {
		if (p_server->op.vout_suspend)
			ret = p_server->op.vout_suspend();
	}

	mutex_unlock(&vout_mutex);
	return ret;
}
EXPORT_SYMBOL(vout_suspend);
int vout_resume(void)
{
	struct vout_server_s  *p_server = vout_module.curr_vout_server;

	mutex_lock(&vout_mutex);

	if (p_server) {
		if (p_server->op.vout_resume) {
			/* ignore error when resume. */
			p_server->op.vout_resume();
		}
	}

	mutex_unlock(&vout_mutex);
	return 0;
}
EXPORT_SYMBOL(vout_resume);
/*
 * interface export to client who want to set current vmode.
 */
int set_current_vmode(enum vmode_e mode)
{
	int r = -1;
	struct vout_server_s  *p_server;

	mutex_lock(&vout_mutex);
	list_for_each_entry(p_server, &vout_module.vout_server_list, list) {
		BUG_ON(p_server->op.vmode_is_supported == NULL);

		if (true == p_server->op.vmode_is_supported(mode)) {
			vout_module.curr_vout_server = p_server;
			r = p_server->op.set_vmode(mode);
			if (vout_module.curr_vout_server)
				update_vout_mode_attr
				(vout_module.curr_vout_server->op.get_vinfo());
			/* break;  do not exit , should disable other modules */
		} else
			p_server->op.disable(mode);
	}

	mutex_unlock(&vout_mutex);

	return r;
}
EXPORT_SYMBOL(set_current_vmode);

/*
*interface export to client who want to set current vmode.
*/
enum vmode_e validate_vmode(char *name)
{
	enum vmode_e r = VMODE_MAX;
	struct vout_server_s  *p_server;

	mutex_lock(&vout_mutex);
	list_for_each_entry(p_server, &vout_module.vout_server_list, list) {
		BUG_ON(p_server->op.validate_vmode == NULL);
		r = p_server->op.validate_vmode(name);

		if (VMODE_MAX != r) /* valid vmode find. */
			break;
	}
	mutex_unlock(&vout_mutex);

	return r;
}
EXPORT_SYMBOL(validate_vmode);

/*
*here we offer two functions to get and register vout module server
*vout module server will set and store tvmode attributes for vout encoder
*we can ensure TVMOD SET MODULE independent with these two function.
*/


int vout_register_server(struct vout_server_s  *mem_server)
{
	struct list_head *p_iter;
	struct vout_server_s  *p_server;

	BUG_ON(mem_server == NULL);
	vout_log_info("%s\n", __func__);

	mutex_lock(&vout_mutex);
	list_for_each(p_iter, &vout_module.vout_server_list) {
		p_server = list_entry(p_iter, struct vout_server_s, list);

		if (p_server->name && mem_server->name &&
		    strcmp(p_server->name, mem_server->name) == 0) {
			/* vout server already registered. */
			mutex_unlock(&vout_mutex);
			return -1;
		}
	}
	list_add(&mem_server->list, &vout_module.vout_server_list);
	mutex_unlock(&vout_mutex);
	return  0;
}
EXPORT_SYMBOL(vout_register_server);

int vout_unregister_server(struct vout_server_s  *mem_server)
{
	struct vout_server_s  *p_server;

	BUG_ON(mem_server == NULL);
	mutex_lock(&vout_mutex);
	list_for_each_entry(p_server, &vout_module.vout_server_list, list) {
		if (p_server->name && mem_server->name &&
		    strcmp(p_server->name, mem_server->name) == 0) {
			/*
			 * We will not move current vout server pointer
			 * automatically if current vout server pointer
			 * is the one which will be deleted next.
			 * So you should change current vout server
			 * first then remove it
			 */
			if (vout_module.curr_vout_server == p_server)
				vout_module.curr_vout_server = NULL;

			list_del(&mem_server->list);
			mutex_unlock(&vout_mutex);
			return 0;
		}
	}
	mutex_unlock(&vout_mutex);
	return 0;
}
EXPORT_SYMBOL(vout_unregister_server);
