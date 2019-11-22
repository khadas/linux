/*
 * drivers/amlogic/media/di_multi/di_vframe.c
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
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

#include <linux/semaphore.h>
#include <linux/kfifo.h>
#include <linux/spinlock.h>

#include "deinterlace.h"
#include "di_data_l.h"
#include "di_pre.h"
#include "di_prc.h"
#include "di_dbg.h"

#include "di_vframe.h"

struct dev_vfram_t *get_dev_vframe(unsigned int ch)
{
	if (ch < DI_CHANNEL_NUB)
		return &get_datal()->ch_data[ch].vfm;

	pr_info("err:%s ch overflow %d\n", __func__, ch);
	return &get_datal()->ch_data[0].vfm;
}

const char * const di_rev_name[4] = {
	"deinterlace",
	"dimulti.1",
	"dimulti.2",
	"dimulti.3",
};

void dev_vframe_reg(struct dev_vfram_t *pvfm)
{
	if (pvfm->reg) {
		PR_WARN("duplicate reg\n");
		return;
	}
	vf_reg_provider(&pvfm->di_vf_prov);
	vf_notify_receiver(pvfm->name, VFRAME_EVENT_PROVIDER_START, NULL);
	pvfm->reg = 1;
}

void dev_vframe_unreg(struct dev_vfram_t *pvfm)
{
	if (pvfm->reg) {
		vf_unreg_provider(&pvfm->di_vf_prov);
		pvfm->reg = 0;
	} else {
		PR_WARN("duplicate ureg\n");
	}
}

void di_vframe_reg(unsigned int ch)
{
	struct dev_vfram_t *pvfm;

	pvfm = get_dev_vframe(ch);

	dev_vframe_reg(pvfm);
}

void di_vframe_unreg(unsigned int ch)
{
	struct dev_vfram_t *pvfm;

	pvfm = get_dev_vframe(ch);
	dev_vframe_unreg(pvfm);
}

/*--------------------------*/

const char * const di_receiver_event_cmd[] = {
	"",
	"_UNREG",
	"_LIGHT_UNREG",
	"_START",
	NULL,	/* "_VFRAME_READY", */
	NULL,	/* "_QUREY_STATE", */
	"_RESET",
	NULL,	/* "_FORCE_BLACKOUT", */
	"_REG",
	"_LIGHT_UNREG_RETURN_VFRAME",
	NULL,	/* "_DPBUF_CONFIG", */
	NULL,	/* "_QUREY_VDIN2NR", */
	NULL,	/* "_SET_3D_VFRAME_INTERLEAVE", */
	NULL,	/* "_FR_HINT", */
	NULL,	/* "_FR_END_HINT", */
	NULL,	/* "_QUREY_DISPLAY_INFO", */
	NULL,	/* "_PROPERTY_CHANGED", */
};

#define VFRAME_EVENT_PROVIDER_CMD_MAX	16

static int di_receiver_event_fun(int type, void *data, void *arg)
{
	struct dev_vfram_t *pvfm;
	unsigned int ch;
	int ret = 0;

	ch = *(int *)arg;

	pvfm = get_dev_vframe(ch);

	if (type <= VFRAME_EVENT_PROVIDER_CMD_MAX	&&
	    di_receiver_event_cmd[type]) {
		dbg_ev("ch[%d]:%s,%d:%s\n", ch, __func__,
		       type,
		       di_receiver_event_cmd[type]);
	}

	switch (type) {
	case VFRAME_EVENT_PROVIDER_UNREG:
		ret = di_ori_event_unreg(ch);
/*		task_send_cmd(LCMD1(eCMD_UNREG, 0));*/
		break;
	case VFRAME_EVENT_PROVIDER_REG:
		/*dev_vframe_reg(pvfm);*/
		ret = di_ori_event_reg(data, ch);
/*		task_send_cmd(LCMD1(eCMD_REG, 0));*/
		break;
	case VFRAME_EVENT_PROVIDER_START:
		break;

	case VFRAME_EVENT_PROVIDER_LIGHT_UNREG:
		ret = di_ori_event_light_unreg(ch);
		break;
	case VFRAME_EVENT_PROVIDER_VFRAME_READY:
		ret = di_ori_event_ready(ch);
		break;
	case VFRAME_EVENT_PROVIDER_QUREY_STATE:
		ret = di_ori_event_qurey_state(ch);
		break;
	case VFRAME_EVENT_PROVIDER_RESET:
		ret = di_ori_event_reset(ch);
		break;
	case VFRAME_EVENT_PROVIDER_LIGHT_UNREG_RETURN_VFRAME:
		ret = di_ori_event_light_unreg_revframe(ch);
		break;
	case VFRAME_EVENT_PROVIDER_QUREY_VDIN2NR:
		ret = di_ori_event_qurey_vdin2nr(ch);
		break;
	case VFRAME_EVENT_PROVIDER_SET_3D_VFRAME_INTERLEAVE:
		di_ori_event_set_3D(type, data, ch);
		break;
	case VFRAME_EVENT_PROVIDER_FR_HINT:
	case VFRAME_EVENT_PROVIDER_FR_END_HINT:
		vf_notify_receiver(pvfm->name, type, data);
		break;

	default:
		break;
	}

	return ret;
}

static const struct vframe_receiver_op_s di_vf_receiver = {
	.event_cb	= di_receiver_event_fun
};

bool vf_type_is_prog(unsigned int type)
{
	bool ret = (type & VIDTYPE_TYPEMASK) == 0 ? true : false;

	return ret;
}

bool vf_type_is_interlace(unsigned int type)
{
	bool ret = (type & VIDTYPE_INTERLACE) ? true : false;

	return ret;
}

bool vf_type_is_top(unsigned int type)
{
	bool ret = ((type & VIDTYPE_TYPEMASK) == VIDTYPE_INTERLACE_TOP)
		? true : false;
	return ret;
}

bool vf_type_is_bottom(unsigned int type)
{
	bool ret = ((type & VIDTYPE_INTERLACE_BOTTOM)
		== VIDTYPE_INTERLACE_BOTTOM)
		? true : false;

	return ret;
}

bool vf_type_is_inter_first(unsigned int type)
{
	bool ret = (type & VIDTYPE_INTERLACE_TOP) ? true : false;

	return ret;
}

bool vf_type_is_mvc(unsigned int type)
{
	bool ret = (type & VIDTYPE_MVC) ? true : false;

	return ret;
}

bool vf_type_is_no_video_en(unsigned int type)
{
	bool ret = (type & VIDTYPE_NO_VIDEO_ENABLE) ? true : false;

	return ret;
}

bool vf_type_is_VIU422(unsigned int type)
{
	bool ret = (type & VIDTYPE_VIU_422) ? true : false;

	return ret;
}

bool vf_type_is_VIU_FIELD(unsigned int type)
{
	bool ret = (type & VIDTYPE_VIU_FIELD) ? true : false;

	return ret;
}

bool vf_type_is_VIU_SINGLE(unsigned int type)
{
	bool ret = (type & VIDTYPE_VIU_SINGLE_PLANE) ? true : false;

	return ret;
}

bool vf_type_is_VIU444(unsigned int type)
{
	bool ret = (type & VIDTYPE_VIU_444) ? true : false;

	return ret;
}

bool vf_type_is_VIUNV21(unsigned int type)
{
	bool ret = (type & VIDTYPE_VIU_NV21) ? true : false;

	return ret;
}

bool vf_type_is_vscale_dis(unsigned int type)
{
	bool ret = (type & VIDTYPE_VSCALE_DISABLE) ? true : false;

	return ret;
}

bool vf_type_is_canvas_toggle(unsigned int type)
{
	bool ret = (type & VIDTYPE_CANVAS_TOGGLE) ? true : false;

	return ret;
}

bool vf_type_is_pre_interlace(unsigned int type)
{
	bool ret = (type & VIDTYPE_PRE_INTERLACE) ? true : false;

	return ret;
}

bool vf_type_is_highrun(unsigned int type)
{
	bool ret = (type & VIDTYPE_HIGHRUN) ? true : false;

	return ret;
}

bool vf_type_is_compress(unsigned int type)
{
	bool ret = (type & VIDTYPE_COMPRESS) ? true : false;

	return ret;
}

bool vf_type_is_pic(unsigned int type)
{
	bool ret = (type & VIDTYPE_PIC) ? true : false;

	return ret;
}

bool vf_type_is_scatter(unsigned int type)
{
	bool ret = (type & VIDTYPE_SCATTER) ? true : false;

	return ret;
}

bool vf_type_is_vd2(unsigned int type)
{
	bool ret = (type & VIDTYPE_VD2) ? true : false;

	return ret;
}

bool is_bypss_complete(struct dev_vfram_t *pvfm)
{
	return pvfm->bypass_complete;
}

#if 0
bool is_reg(unsigned int ch)
{
	struct dev_vfram_t *pvfm;

	pvfm = get_dev_vframe(ch);

	return pvfm->reg;
}
#endif

void set_bypass_complete(struct dev_vfram_t *pvfm, bool on)
{
	if (on)
		pvfm->bypass_complete = true;
	else
		pvfm->bypass_complete = false;
}

void set_bypass2_complete(unsigned int ch, bool on)
{
	struct dev_vfram_t *pvfm;

	pvfm = get_dev_vframe(ch);
	set_bypass_complete(pvfm, on);
}

bool is_bypss2_complete(unsigned int ch)
{
	struct dev_vfram_t *pvfm;

	pvfm = get_dev_vframe(ch);

	return is_bypss_complete(pvfm);
}

#if 0
static void set_reg(unsigned int ch, int on)
{
	struct dev_vfram_t *pvfm;

	pvfm = get_dev_vframe(ch);

	if (on)
		pvfm->reg = true;
	else
		pvfm->reg = false;
}
#endif
static struct vframe_s *di_vf_peek(void *arg)
{
	unsigned int ch = *(int *)arg;

	/*dim_print("%s:ch[%d]\n",__func__,ch);*/
	if (di_is_pause(ch))
		return NULL;

	if (is_bypss2_complete(ch))
		return pw_vf_peek(ch);
	else
		return di_vf_l_peek(ch);
}

static struct vframe_s *di_vf_get(void *arg)
{
	unsigned int ch = *(int *)arg;
	/*struct vframe_s *vfm;*/

	dim_tr_ops.post_get2(5);
	if (di_is_pause(ch))
		return NULL;

	di_pause_step_done(ch);

	/*pvfm = get_dev_vframe(ch);*/

	if (is_bypss2_complete(ch))
	#if 0
		vfm = pw_vf_peek(ch);
		if (dim_bypass_detect(ch, vfm))
			return NULL;

	#endif
		return pw_vf_get(ch);

	return di_vf_l_get(ch);
}

static void di_vf_put(struct vframe_s *vf, void *arg)
{
	unsigned int ch = *(int *)arg;

	if (is_bypss2_complete(ch)) {
		pw_vf_put(vf, ch);
		pw_vf_notify_provider(ch,
				      VFRAME_EVENT_RECEIVER_PUT, NULL);
		return;
	}

	di_vf_l_put(vf, ch);
}

static int di_event_cb(int type, void *data, void *private_data)
{
	if (type == VFRAME_EVENT_RECEIVER_FORCE_UNREG) {
		pr_info("%s: RECEIVER_FORCE_UNREG return\n",
			__func__);
		return 0;
	}
	return 0;
}

static int di_vf_states(struct vframe_states *states, void *arg)
{
	unsigned int ch = *(int *)arg;

	if (!states)
		return -1;

	dim_print("%s:ch[%d]\n", __func__, ch);

	di_vf_l_states(states, ch);
	return 0;
}

static const struct vframe_operations_s deinterlace_vf_provider = {
	.peek		= di_vf_peek,
	.get		= di_vf_get,
	.put		= di_vf_put,
	.event_cb	= di_event_cb,
	.vf_states	= di_vf_states,
};

#if 1
struct vframe_s *pw_vf_get(unsigned int ch)
{
	sum_g_inc(ch);
	return vf_get(di_rev_name[ch]);
}

struct vframe_s *pw_vf_peek(unsigned int ch)
{
	return vf_peek(di_rev_name[ch]);
}

void pw_vf_put(struct vframe_s *vf, unsigned int ch)
{
	sum_p_inc(ch);
	vf_put(vf, di_rev_name[ch]);
}

int pw_vf_notify_provider(unsigned int channel, int event_type, void *data)
{
	return vf_notify_provider(di_rev_name[channel], event_type, data);
}

int pw_vf_notify_receiver(unsigned int channel, int event_type, void *data)
{
	return vf_notify_receiver(di_rev_name[channel], event_type, data);
}

void pw_vf_light_unreg_provider(unsigned int ch)
{
	struct dev_vfram_t *pvfm;
	struct vframe_provider_s *prov;

	pvfm = get_dev_vframe(ch);

	prov = &pvfm->di_vf_prov;
	vf_light_unreg_provider(prov);
}

#else
struct vframe_s *pw_vf_get(unsigned int channel)
{
	return vf_get(VFM_NAME);
}

struct vframe_s *pw_vf_peek(unsigned int channel)
{
	return vf_peek(VFM_NAME);
}

void pw_vf_put(struct vframe_s *vf, unsigned int channel)
{
	vf_put(vf, VFM_NAME);
}

int pw_vf_notify_provider(unsigned int channel, int event_type, void *data)
{
	return vf_notify_provider(VFM_NAME, event_type, data);
}

int pw_vf_notify_receiver(unsigned int channel, int event_type, void *data)
{
	return vf_notify_receiver(VFM_NAME, event_type, data);
}

#endif

void dev_vframe_exit(void)
{
	struct dev_vfram_t *pvfm;
	int ch;

	for (ch = 0; ch < DI_CHANNEL_NUB; ch++) {
		pvfm = get_dev_vframe(ch);
		vf_unreg_provider(&pvfm->di_vf_prov);
		vf_unreg_receiver(&pvfm->di_vf_recv);
	}
	pr_info("%s finish\n", __func__);
}

void dev_vframe_init(void)
{
	struct dev_vfram_t *pvfm;
	int ch;

	for (ch = 0; ch < DI_CHANNEL_NUB; ch++) {
		pvfm = get_dev_vframe(ch);
		pvfm->name = di_rev_name[ch];
		pvfm->indx = ch;
		/*set_bypass_complete(pvfm, true);*/ /*test only*/

		/*receiver:*/
		vf_receiver_init(&pvfm->di_vf_recv, pvfm->name,
				 &di_vf_receiver, &pvfm->indx);
		vf_reg_receiver(&pvfm->di_vf_recv);

		/*provider:*/
		vf_provider_init(&pvfm->di_vf_prov, pvfm->name,
				 &deinterlace_vf_provider, &pvfm->indx);
	}
	pr_info("%s finish\n", __func__);
}
