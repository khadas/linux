// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

 #define pr_fmt(fmt) "vout: " fmt

/* Linux Headers */
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/err.h>

/* Amlogic Headers */
#include <linux/amlogic/media/vpu/vpu.h>
#include <linux/amlogic/media/vout/vout_notify.h>

/* Local Headers */
#include "vout_func.h"

static DEFINE_MUTEX(vout_mutex);

static struct vout_module_s vout_module = {
	.vout_server_list = {
		&vout_module.vout_server_list,
		&vout_module.vout_server_list
	},
	.curr_vout_server = NULL,
	.next_vout_server = NULL,
	.init_flag = 0,
	/* vout_fr_policy:
	 *    0: disable
	 *    1: nearby (only for 60->59.94 and 30->29.97)
	 *    2: force (60/50/30/24/59.94/23.97)
	 */
	.fr_policy = 0,
};

#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
static struct vout_module_s vout2_module = {
	.vout_server_list = {
		&vout2_module.vout_server_list,
		&vout2_module.vout_server_list
	},
	.curr_vout_server = NULL,
	.next_vout_server = NULL,
	.init_flag = 0,
	/* vout_fr_policy:
	 *    0: disable
	 *    1: nearby (only for 60->59.94 and 30->29.97)
	 *    2: force (60/50/30/24/59.94/23.97)
	 */
	.fr_policy = 0,
};
#endif

#ifdef CONFIG_AMLOGIC_VOUT3_SERVE
static struct vout_module_s vout3_module = {
	.vout_server_list = {
		&vout3_module.vout_server_list,
		&vout3_module.vout_server_list
	},
	.curr_vout_server = NULL,
	.next_vout_server = NULL,
	.init_flag = 0,
	/* vout_fr_policy:
	 *    0: disable
	 *    1: nearby (only for 60->59.94 and 30->29.97)
	 *    2: force (60/50/30/24/59.94/23.97)
	 */
	.fr_policy = 0,
};
#endif

static struct vinfo_s invalid_vinfo = {
	.name              = "invalid",
	.mode              = VMODE_INVALID,
	.width             = 1920,
	.height            = 1080,
	.field_height      = 1080,
	.aspect_ratio_num  = 16,
	.aspect_ratio_den  = 9,
	.sync_duration_num = 60,
	.sync_duration_den = 1,
	.video_clk         = 148500000,
	.htotal            = 2200,
	.vtotal            = 1125,
	.viu_color_fmt     = COLOR_FMT_RGB444,
	.viu_mux           = ((3 << 4) | VIU_MUX_MAX),
	.vout_device       = NULL,
};

struct vinfo_s *get_invalid_vinfo(int index, unsigned int flag)
{
	if (flag) {
		VOUTERR("invalid vinfo%d. current vmode is not supported\n",
			index);
	}
	return &invalid_vinfo;
}
EXPORT_SYMBOL(get_invalid_vinfo);

void vout_trim_string(char *str)
{
	char *start, *end;
	int len;

	if (!str)
		return;

	len = strlen(str);
	if ((str[len - 1] == '\n') || (str[len - 1] == '\r')) {
		len--;
		str[len] = '\0';
	}

	start = str;
	end = str + len - 1;
	while (*start && (*start == ' '))
		start++;
	while (*end && (*end == ' '))
		*end-- = '\0';
	strcpy(str, start);
}
EXPORT_SYMBOL(vout_trim_string);

struct vout_module_s *vout_func_get_vout_module(void)
{
	return &vout_module;
}
EXPORT_SYMBOL(vout_func_get_vout_module);

#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
struct vout_module_s *vout_func_get_vout2_module(void)
{
	return &vout2_module;
}
EXPORT_SYMBOL(vout_func_get_vout2_module);
#endif

#ifdef CONFIG_AMLOGIC_VOUT3_SERVE
struct vout_module_s *vout_func_get_vout3_module(void)
{
	return &vout3_module;
}
EXPORT_SYMBOL(vout_func_get_vout3_module);
#endif

static inline int vout_func_check_state(int index, unsigned int state,
					struct vout_server_s *p_server)
{
	if (state & ~(1 << index)) {
		VOUTERR("vout%d: server %s is activated by another vout\n",
			index, p_server->name);
		return -1;
	}

	return 0;
}

static inline void vout_func_pre_server_disable(int index, struct vout_server_s *p_server)
{
	struct vinfo_s *vinfo = NULL;
	enum vmode_e mode = VMODE_MAX;

	if (p_server->op.get_vinfo) {
		vinfo = p_server->op.get_vinfo(p_server->data);
		if (vinfo) {
			mode = vinfo->mode;
			vout_viu_mux_clear(index, vinfo->viu_mux);
		}
	}
	if (p_server->op.disable)
		p_server->op.disable(mode, p_server->data);
}

void vout_func_set_state(int index, enum vmode_e mode)
{
	struct vout_server_s *p_server;
	struct vout_module_s *p_module = NULL;
	void *data;
	int state;

	mutex_lock(&vout_mutex);

	if (index == 1)
		p_module = &vout_module;
#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
	else if (index == 2)
		p_module = &vout2_module;
#endif
#ifdef CONFIG_AMLOGIC_VOUT3_SERVE
	else if (index == 3)
		p_module = &vout3_module;
#endif

	if (!p_module) {
		VOUTERR("vout[%d]: %s: vout_module is NULL\n", index, __func__);
		mutex_unlock(&vout_mutex);
		return;
	}

	list_for_each_entry(p_server, &p_module->vout_server_list, list) {
		data = p_server->data;
		if (p_module->next_vout_server && p_server->name &&
		    p_module->next_vout_server->name &&
		    (strcmp(p_server->name, p_module->next_vout_server->name) == 0)) {
			if (vout_debug_print) {
				VOUTPR("vout[%d]: %s: valid: server_name=%s, data=%px\n",
					index, __func__, p_server->name, data);
			}
			if (p_server->op.vmode_is_supported(mode, data)) {
				p_module->curr_vout_server = p_server;
				p_module->next_vout_server = NULL;
				if (p_server->op.set_state)
					p_server->op.set_state(index, data);
			}
		} else {
			if (p_server->op.get_state) {
				state = p_server->op.get_state(data);
				if (state & (1 << index))
					vout_func_pre_server_disable(index, p_server);
			}
			if (p_server->op.clr_state)
				p_server->op.clr_state(index, data);
		}
	}

	mutex_unlock(&vout_mutex);
}
EXPORT_SYMBOL(vout_func_set_state);

void vout_func_update_viu(int index)
{
	struct vinfo_s *vinfo = NULL;
	struct vout_server_s *p_server;
	struct vout_module_s *p_module = NULL;
	unsigned int mux_sel;
	unsigned int flag = 0;

	mutex_lock(&vout_mutex);

	if (index == 1)
		p_module = &vout_module;
#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
	else if (index == 2)
		p_module = &vout2_module;
#endif
#ifdef CONFIG_AMLOGIC_VOUT3_SERVE
	else if (index == 3)
		p_module = &vout3_module;
#endif

	if (!p_module) {
		VOUTERR("vout%d: %s: vout_module is NULL\n", index, __func__);
		mutex_unlock(&vout_mutex);
		return;
	}
	p_server = p_module->curr_vout_server;
	flag = p_module->init_flag;

	/*
	 *VOUTPR("%s: before: 0x%04x=0x%08x, 0x%04x=0x%08x\n",
	 *	__func__, VPU_VIU_VENC_MUX_CTRL,
	 *	vout_vcbus_read(VPU_VIU_VENC_MUX_CTRL),
	 *	VPU_VENCX_CLK_CTRL,
	 *	vout_vcbus_read(VPU_VENCX_CLK_CTRL));
	 */

	if (p_server && p_server->op.get_vinfo)
		vinfo = p_server->op.get_vinfo(p_server->data);
	if (!vinfo)
		vinfo = get_invalid_vinfo(index, flag);

	mux_sel = vinfo->viu_mux;

	vout_viu_mux_update(index, mux_sel);

	mutex_unlock(&vout_mutex);
}
EXPORT_SYMBOL(vout_func_update_viu);

int vout_func_set_vmode(int index, enum vmode_e mode)
{
	int ret = -1;
	struct vout_module_s *p_module = NULL;
	void *data;

	mutex_lock(&vout_mutex);

	if (index == 1)
		p_module = &vout_module;
#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
	else if (index == 2)
		p_module = &vout2_module;
#endif
#ifdef CONFIG_AMLOGIC_VOUT3_SERVE
	else if (index == 3)
		p_module = &vout3_module;
#endif

	if (!p_module) {
		VOUTERR("vout%d: %s: vout_module is NULL\n", index, __func__);
		mutex_unlock(&vout_mutex);
		return -1;
	}
	if (!p_module->curr_vout_server) {
		VOUTERR("vout%d: %s: current_server is NULL\n", index, __func__);
		mutex_unlock(&vout_mutex);
		return -1;
	}
	data = p_module->curr_vout_server->data;
	if (p_module->curr_vout_server->op.set_vmode)
		ret = p_module->curr_vout_server->op.set_vmode(mode, data);
	p_module->init_flag = 1;

	mutex_unlock(&vout_mutex);

	return ret;
}
EXPORT_SYMBOL(vout_func_set_vmode);

/*
 * interface export to client who want to set current vmode.
 */
int vout_func_set_current_vmode(int index, enum vmode_e mode)
{
	vout_func_set_state(index, mode);
	vout_func_update_viu(index);

	return vout_func_set_vmode(index, mode);
}
EXPORT_SYMBOL(vout_func_set_current_vmode);

int vout_func_check_same_vmodeattr(int index, char *name)
{
	struct vout_server_s *p_server = NULL;
	void *data;
	int ret = 1;

	mutex_lock(&vout_mutex);

	if (index == 1)
		p_server = vout_module.curr_vout_server;
#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
	else if (index == 2)
		p_server = vout2_module.curr_vout_server;
#endif
#ifdef CONFIG_AMLOGIC_VOUT3_SERVE
	else if (index == 3)
		p_server = vout3_module.curr_vout_server;
#endif

	if (p_server) {
		data = p_server->data;
		if (p_server->op.check_same_vmodeattr)
			ret = p_server->op.check_same_vmodeattr(name, data);
	}

	mutex_unlock(&vout_mutex);
	return ret;
}

/*
 *interface export to client who want to set current vmode.
 */
enum vmode_e vout_func_validate_vmode(int index, char *name, unsigned int frac)
{
	enum vmode_e ret = VMODE_MAX;
	struct vout_server_s  *p_server;
	struct vout_module_s *p_module = NULL;
	void *data;
	int state;

	mutex_lock(&vout_mutex);

	if (index == 1)
		p_module = &vout_module;
#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
	else if (index == 2)
		p_module = &vout2_module;
#endif
#ifdef CONFIG_AMLOGIC_VOUT3_SERVE
	else if (index == 3)
		p_module = &vout3_module;
#endif

	if (!p_module) {
		VOUTERR("vout%d: %s: vout_module is NULL\n", index, __func__);
		mutex_unlock(&vout_mutex);
		return VMODE_MAX;
	}
	p_module->next_vout_server = NULL;
	list_for_each_entry(p_server, &p_module->vout_server_list, list) {
		data = p_server->data;
		if (vout_debug_print) {
			VOUTPR("vout[%d]: %s: list: server_name=%s, data=%px\n",
				index, __func__, p_server->name, data);
		}
		/* check state for another vout */
		if (p_server->op.get_state) {
			state = p_server->op.get_state(data);
			if (vout_func_check_state(index, state, p_server)) {
				if (vout_debug_print) {
					VOUTPR("vout[%d]: %s: check state: %d\n",
						index, __func__, state);
				}
				continue;
			}
		}
		if (p_server->op.validate_vmode) {
			ret = p_server->op.validate_vmode(name, frac, data);
			if (ret != VMODE_MAX) { /* valid vmode find. */
				p_module->next_vout_server = p_server;
				if (vout_debug_print) {
					VOUTPR("vout[%d]: %s: valid server: name=%s, data=%px\n",
						index, __func__,
						p_module->next_vout_server->name,
						p_module->next_vout_server->data);
				}
				break;
			}
		}
	}
	mutex_unlock(&vout_mutex);

	return ret;
}
EXPORT_SYMBOL(vout_func_validate_vmode);

int vout_func_get_disp_cap(int index, char *buf)
{
	struct vout_server_s *p_server;
	struct vout_module_s *p_module = NULL;
	void *data;
	int state, len;

	mutex_lock(&vout_mutex);

	if (index == 1)
		p_module = &vout_module;
#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
	else if (index == 2)
		p_module = &vout2_module;
#endif
#ifdef CONFIG_AMLOGIC_VOUT3_SERVE
	else if (index == 3)
		p_module = &vout3_module;
#endif

	if (!p_module) {
		len = sprintf(buf, "vout%d: %s: vout_module is NULL\n",
			      index, __func__);
		mutex_unlock(&vout_mutex);
		return len;
	}

	len = 0;
	list_for_each_entry(p_server, &p_module->vout_server_list, list) {
		data = p_server->data;
		if (p_server->op.get_state) {
			state = p_server->op.get_state(data);
			if (vout_func_check_state(index, state, p_server))
				continue;
		}
		if (!p_server->op.get_disp_cap)
			continue;
		len += p_server->op.get_disp_cap(buf + len, data);
	}

	mutex_unlock(&vout_mutex);
	return len;
}

int vout_func_set_vframe_rate_hint(int index, int duration)
{
	int ret = -1;
	struct vout_server_s *p_server = NULL;
	void *data;

	mutex_lock(&vout_mutex);

	if (index == 1)
		p_server = vout_module.curr_vout_server;
#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
	else if (index == 2)
		p_server = vout2_module.curr_vout_server;
#endif
#ifdef CONFIG_AMLOGIC_VOUT3_SERVE
	else if (index == 3)
		p_server = vout3_module.curr_vout_server;
#endif

	if (p_server) {
		data = p_server->data;
		if (p_server->op.set_vframe_rate_hint)
			ret = p_server->op.set_vframe_rate_hint(duration, data);
	}

	mutex_unlock(&vout_mutex);
	return ret;
}
EXPORT_SYMBOL(vout_func_set_vframe_rate_hint);

/*
 *interface export to client who want to notify about source frame rate end.
 */
int vout_func_get_vframe_rate_hint(int index)
{
	int ret = 0;
	struct vout_server_s *p_server = NULL;
	void *data;

	mutex_lock(&vout_mutex);

	if (index == 1)
		p_server = vout_module.curr_vout_server;
#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
	else if (index == 2)
		p_server = vout2_module.curr_vout_server;
#endif
#ifdef CONFIG_AMLOGIC_VOUT3_SERVE
	else if (index == 3)
		p_server = vout3_module.curr_vout_server;
#endif

	if (p_server) {
		data = p_server->data;
		if (p_server->op.get_vframe_rate_hint)
			ret = p_server->op.get_vframe_rate_hint(data);
	}

	mutex_unlock(&vout_mutex);
	return ret;
}
EXPORT_SYMBOL(vout_func_get_vframe_rate_hint);

/*
 * interface export to client who want to set test bist.
 */
void vout_func_set_test_bist(int index, unsigned int bist)
{
	struct vout_server_s *p_server = NULL;

	mutex_lock(&vout_mutex);

	if (index == 1)
		p_server = vout_module.curr_vout_server;
#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
	else if (index == 2)
		p_server = vout2_module.curr_vout_server;
#endif
#ifdef CONFIG_AMLOGIC_VOUT3_SERVE
	else if (index == 3)
		p_server = vout3_module.curr_vout_server;
#endif

	if (p_server) {
		if (p_server->op.set_bist)
			p_server->op.set_bist(bist, p_server->data);
	}

	mutex_unlock(&vout_mutex);
}
EXPORT_SYMBOL(vout_func_set_test_bist);

void vout_func_set_bl_brightness(int index, unsigned int brightness)
{
	struct vout_server_s *p_server = NULL;
	void *data;

	mutex_lock(&vout_mutex);

	if (index == 1)
		p_server = vout_module.curr_vout_server;
#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
	else if (index == 2)
		p_server = vout2_module.curr_vout_server;
#endif
#ifdef CONFIG_AMLOGIC_VOUT3_SERVE
	else if (index == 3)
		p_server = vout3_module.curr_vout_server;
#endif

	if (p_server) {
		data = p_server->data;
		if (p_server->op.set_bl_brightness)
			p_server->op.set_bl_brightness(brightness, data);
	}

	mutex_unlock(&vout_mutex);
}
EXPORT_SYMBOL(vout_func_set_bl_brightness);

/*
 *interface export to client who want to update backlight brightness.
 */
unsigned int vout_func_get_bl_brightness(int index)
{
	struct vout_server_s *p_server = NULL;
	void *data;
	unsigned int brightness = 0;

	mutex_lock(&vout_mutex);

	if (index == 1)
		p_server = vout_module.curr_vout_server;
#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
	else if (index == 2)
		p_server = vout2_module.curr_vout_server;
#endif
#ifdef CONFIG_AMLOGIC_VOUT3_SERVE
	else if (index == 3)
		p_server = vout3_module.curr_vout_server;
#endif

	if (p_server) {
		data = p_server->data;
		if (p_server->op.get_bl_brightness)
			brightness = p_server->op.get_bl_brightness(data);
	}

	mutex_unlock(&vout_mutex);
	return brightness;
}
EXPORT_SYMBOL(vout_func_get_bl_brightness);

int vout_func_vout_suspend(int index)
{
	int ret = 0;
	struct vout_server_s *p_server = NULL;

	mutex_lock(&vout_mutex);

	if (index == 1)
		p_server = vout_module.curr_vout_server;
#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
	else if (index == 2)
		p_server = vout2_module.curr_vout_server;
#endif

	if (p_server) {
		if (p_server->op.vout_suspend)
			ret = p_server->op.vout_suspend(p_server->data);
	}

	mutex_unlock(&vout_mutex);
	return ret;
}
EXPORT_SYMBOL(vout_func_vout_suspend);

int vout_func_vout_resume(int index)
{
	struct vout_server_s *p_server = NULL;

	mutex_lock(&vout_mutex);

	if (index == 1)
		p_server = vout_module.curr_vout_server;
#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
	else if (index == 2)
		p_server = vout2_module.curr_vout_server;
#endif
#ifdef CONFIG_AMLOGIC_VOUT3_SERVE
	else if (index == 3)
		p_server = vout3_module.curr_vout_server;
#endif

	if (p_server) {
		if (p_server->op.vout_resume) {
			/* ignore error when resume. */
			p_server->op.vout_resume(p_server->data);
		}
	}

	mutex_unlock(&vout_mutex);
	return 0;
}
EXPORT_SYMBOL(vout_func_vout_resume);

/*
 *interface export to client who want to shutdown.
 */
int vout_func_vout_shutdown(int index)
{
	int ret = -1;
	struct vout_server_s *p_server;
	struct vout_module_s *p_module = NULL;

	if (index == 1)
		p_module = &vout_module;
#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
	else if (index == 2)
		p_module = &vout2_module;
#endif
#ifdef CONFIG_AMLOGIC_VOUT3_SERVE
	else if (index == 3)
		p_module = &vout3_module;
#endif

	if (!p_module) {
		VOUTERR("vout%d: %s: vout_module is NULL\n", index, __func__);
		return -1;
	}
	list_for_each_entry(p_server, &p_module->vout_server_list, list) {
		if (p_server->op.vout_shutdown)
			ret = p_server->op.vout_shutdown(p_server->data);
	}

	return ret;
}
EXPORT_SYMBOL(vout_func_vout_shutdown);

/*
 *here we offer two functions to get and register vout module server
 *vout module server will set and store tvmode attributes for vout encoder
 *we can ensure TVMOD SET MODULE independent with these two function.
 */

int vout_func_vout_register_server(int index,
				   struct vout_server_s *mem_server)
{
	struct list_head *p_iter;
	struct vout_server_s *p_server;
	struct vout_module_s *p_module = NULL;

	if (!mem_server) {
		VOUTERR("vout%d: server is NULL\n", index);
		return -1;
	}
	if (!mem_server->name) {
		VOUTERR("vout%d: server name is NULL\n", index);
		return -1;
	}
	VOUTDBG("vout%d: register server: %s\n", index, mem_server->name);

	mutex_lock(&vout_mutex);

	if (index == 1)
		p_module = &vout_module;
#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
	else if (index == 2)
		p_module = &vout2_module;
#endif
#ifdef CONFIG_AMLOGIC_VOUT3_SERVE
	else if (index == 3)
		p_module = &vout3_module;
#endif

	if (!p_module) {
		VOUTERR("vout%d: %s: vout_module is NULL\n", index, __func__);
		mutex_unlock(&vout_mutex);
		return -1;
	}
	list_for_each(p_iter, &p_module->vout_server_list) {
		p_server = list_entry(p_iter, struct vout_server_s, list);

		if (p_server->name &&
		    (strcmp(p_server->name, mem_server->name) == 0)) {
			VOUTPR("vout%d: server %s is already registered\n",
			       index, mem_server->name);
			/* vout server already registered. */
			mutex_unlock(&vout_mutex);
			return -1;
		}
	}
	list_add(&mem_server->list, &p_module->vout_server_list);
	mutex_unlock(&vout_mutex);
	return 0;
}
EXPORT_SYMBOL(vout_func_vout_register_server);

int vout_func_vout_unregister_server(int index,
				     struct vout_server_s *mem_server)
{
	struct vout_server_s  *p_server;
	struct vout_module_s *p_module = NULL;

	if (!mem_server) {
		VOUTERR("vout%d: server is NULL\n", index);
		return -1;
	}
	if (!mem_server->name) {
		VOUTERR("vout%d: server name is NULL\n", index);
		return -1;
	}
	/*VOUTPR("vout%d: unregister server: %s\n", index, mem_server->name);*/

	mutex_lock(&vout_mutex);

	if (index == 1)
		p_module = &vout_module;
#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
	else if (index == 2)
		p_module = &vout2_module;
#endif
#ifdef CONFIG_AMLOGIC_VOUT3_SERVE
	else if (index == 3)
		p_module = &vout3_module;
#endif

	if (!p_module) {
		VOUTERR("vout%d: %s: vout_module is NULL\n", index, __func__);
		mutex_unlock(&vout_mutex);
		return -1;
	}
	list_for_each_entry(p_server, &p_module->vout_server_list, list) {
		if (p_server->name && mem_server->name &&
		    (strcmp(p_server->name, mem_server->name) == 0)) {
			/*
			 * We will not move current vout server pointer
			 * automatically if current vout server pointer
			 * is the one which will be deleted next.
			 * So you should change current vout server
			 * first then remove it
			 */
			if (p_module->curr_vout_server == p_server)
				p_module->curr_vout_server = NULL;

			list_del(&mem_server->list);
			mutex_unlock(&vout_mutex);
			return 0;
		}
	}
	mutex_unlock(&vout_mutex);
	return 0;
}
EXPORT_SYMBOL(vout_func_vout_unregister_server);

/* return vout frac */
unsigned int vout_parse_vout_name(char *name)
{
	char *p, *frac_str;
	unsigned int frac = 0;

	mutex_lock(&vout_mutex);

	p = strchr(name, ',');
	if (!p) {
		frac = 0;
	} else {
		frac_str = p + 1;
		*p = '\0';
		if (strcmp(frac_str, "frac") == 0)
			frac = 1;
	}

	mutex_unlock(&vout_mutex);

	return frac;
}
