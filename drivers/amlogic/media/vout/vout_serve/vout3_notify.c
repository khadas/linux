// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

/* Linux Headers */
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/err.h>

/* Amlogic Headers */
#include <linux/amlogic/media/vout/vout_notify.h>

/* Local Headers */
#include "vout_func.h"

static BLOCKING_NOTIFIER_HEAD(vout3_notifier_list);

/**
 *	vout_register_client - register a client notifier
 *	@nb: notifier block to callback on events
 */
int vout3_register_client(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&vout3_notifier_list, nb);
}
EXPORT_SYMBOL(vout3_register_client);

/**
 *	vout_unregister_client - unregister a client notifier
 *	@nb: notifier block to callback on events
 */
int vout3_unregister_client(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&vout3_notifier_list, nb);
}
EXPORT_SYMBOL(vout3_unregister_client);

/**
 * vout_notifier_call_chain - notify clients of fb_events
 *
 */
int vout3_notifier_call_chain(unsigned long val, void *v)
{
	return blocking_notifier_call_chain(&vout3_notifier_list, val, v);
}
EXPORT_SYMBOL_GPL(vout3_notifier_call_chain);

/*
 *interface export to client who want to get current vinfo.
 */
struct vinfo_s *get_current_vinfo3(void)
{
	struct vinfo_s *vinfo = NULL;
	struct vout_module_s *p_module = NULL;
	void *data;

	p_module = vout_func_get_vout3_module();
	if (!IS_ERR_OR_NULL(p_module->curr_vout_server)) {
		data = p_module->curr_vout_server->data;
		if (p_module->curr_vout_server->op.get_vinfo)
			vinfo = p_module->curr_vout_server->op.get_vinfo(data);
	}
	if (!vinfo) /* avoid crash mistake */
		vinfo = get_invalid_vinfo(3, p_module->init_flag);

	return vinfo;
}
EXPORT_SYMBOL(get_current_vinfo3);

/*
 *interface export to client who want to get display support list.
 */
int get_vout3_disp_cap(char *buf)
{
	return vout_func_get_disp_cap(3, buf);
}
EXPORT_SYMBOL(get_vout3_disp_cap);

/*
 *interface export to client who want to get current vmode.
 */
enum vmode_e get_current_vmode3(void)
{
	const struct vinfo_s *vinfo;
	struct vout_module_s *p_module = NULL;
	enum vmode_e mode = VMODE_MAX;
	void *data;

	p_module = vout_func_get_vout3_module();
	if (!IS_ERR_OR_NULL(p_module->curr_vout_server)) {
		data = p_module->curr_vout_server->data;
		if (p_module->curr_vout_server->op.get_vinfo) {
			vinfo = p_module->curr_vout_server->op.get_vinfo(data);
			if (vinfo)
				mode = vinfo->mode;
		}
	}

	return mode;
}
EXPORT_SYMBOL(get_current_vmode3);

const char *get_name_by_vmode3(enum vmode_e mode)
{
	const char *str = NULL;
	const struct vinfo_s *vinfo = NULL;
	struct vout_module_s *p_module = NULL;
	void *data;

	p_module = vout_func_get_vout3_module();
	if (!IS_ERR_OR_NULL(p_module->curr_vout_server)) {
		data = p_module->curr_vout_server->data;
		if (p_module->curr_vout_server->op.get_vinfo)
			vinfo = p_module->curr_vout_server->op.get_vinfo(data);
	}
	if (!vinfo)
		vinfo = get_invalid_vinfo(3, p_module->init_flag);
	str = vinfo->name;

	return str;
}
EXPORT_SYMBOL(get_name_by_vmode3);

/*
 * interface export to client who want to set current vmode.
 */
int set_current_vmode3(enum vmode_e mode)
{
	return vout_func_set_current_vmode(3, mode);
}
EXPORT_SYMBOL(set_current_vmode3);

/*
 *interface export to client who want to check same vmode attr.
 */
int vout3_check_same_vmodeattr(char *name)
{
	return vout_func_check_same_vmodeattr(3, name);
}
EXPORT_SYMBOL(vout3_check_same_vmodeattr);

/*
 *interface export to client who want to set current vmode.
 */
enum vmode_e validate_vmode3(char *name, unsigned int frac)
{
	return vout_func_validate_vmode(3, name, frac);
}
EXPORT_SYMBOL(validate_vmode3);

/*
 *interface export to client who want to notify about source frame rate.
 */
int set_vframe3_rate_hint(int duration)
{
	return vout_func_set_vframe_rate_hint(3, duration);
}
EXPORT_SYMBOL(set_vframe3_rate_hint);

int get_vframe3_rate_hint(void)
{
	return vout_func_get_vframe_rate_hint(3);
}
EXPORT_SYMBOL(get_vframe3_rate_hint);

/*
 *interface export to client who want to notify about source fr_policy.
 */
int set_vframe3_rate_policy(int policy)
{
	struct vout_module_s *p_module = NULL;

	p_module = vout_func_get_vout3_module();
	if (!p_module)
		return -1;

	p_module->fr_policy = policy;

	return 0;
}
EXPORT_SYMBOL(set_vframe3_rate_policy);

/*
 *interface export to client who want to notify about source fr_policy.
 */
int get_vframe3_rate_policy(void)
{
	struct vout_module_s *p_module = NULL;

	p_module = vout_func_get_vout3_module();
	if (!p_module)
		return 0;

	return p_module->fr_policy;
}
EXPORT_SYMBOL(get_vframe3_rate_policy);

/*
 * interface export to client who want to set test bist.
 */
void set_vout3_bist(unsigned int bist)
{
	vout_func_set_test_bist(3, bist);
}
EXPORT_SYMBOL(set_vout3_bist);

/*
 *interface export to client who want to update backlight brightness.
 */
void set_vout3_bl_brightness(unsigned int brightness)
{
	vout_func_set_bl_brightness(3, brightness);
}
EXPORT_SYMBOL(set_vout3_bl_brightness);

/*
 *interface export to client who want to update backlight brightness.
 */
unsigned int get_vout3_bl_brightness(void)
{
	return vout_func_get_bl_brightness(3);
}
EXPORT_SYMBOL(get_vout3_bl_brightness);

int vout3_suspend(void)
{
	return vout_func_vout_suspend(3);
}
EXPORT_SYMBOL(vout3_suspend);

int vout3_resume(void)
{
	return vout_func_vout_resume(3);
}
EXPORT_SYMBOL(vout3_resume);

/*
 *interface export to client who want to shutdown.
 */
int vout3_shutdown(void)
{
	return vout_func_vout_shutdown(3);
}
EXPORT_SYMBOL(vout3_shutdown);

/*
 *here we offer two functions to get and register vout module server
 *vout module server will set and store tvmode attributes for vout encoder
 *we can ensure TVMODE SET MODULE independent with these two function.
 */

int vout3_register_server(struct vout_server_s *mem_server)
{
	return vout_func_vout_register_server(3, mem_server);
}
EXPORT_SYMBOL(vout3_register_server);

int vout3_unregister_server(struct vout_server_s *mem_server)
{
	return vout_func_vout_unregister_server(3, mem_server);
}
EXPORT_SYMBOL(vout3_unregister_server);
