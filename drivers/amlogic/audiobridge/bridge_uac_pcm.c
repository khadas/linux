// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2023 Amlogic, Inc. All rights reserved.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sysfs.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/kasan.h>
#include <linux/highmem.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/printk.h>
#include <linux/string.h>
#include <linux/arm-smccc.h>
#include <linux/psci.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <uapi/linux/psci.h>
#include <linux/debugfs.h>
#include <linux/sched/signal.h>
#include <linux/dma-mapping.h>
#include <linux/of_reserved_mem.h>
#include <linux/vmalloc.h>
#include <linux/clk.h>
#include <asm/cacheflush.h>
#include <linux/delay.h>
#include <linux/fs.h>
#ifdef CONFIG_USB_CONFIGFS_F_HID
#include <linux/usb/g_hid.h>
#endif

#include "bridge_common.h"
#include "ringbuffer.h"
#include "bridge_pcm_hal.h"

struct uac_pcm_t {
	int chmask;
	int srate;
	int ssize;
	u8 uac_status;
	u8 run_flag;
};

static struct audio_pcm_function_t *cap_pcm;
static struct audio_pcm_function_t *play_pcm;

static ssize_t bridge_uac_status_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	struct audio_pcm_function_t *apf;
	struct uac_pcm_t *uac_pcm;

	if (cap_pcm && cap_pcm->kobj == kobj)
		apf = cap_pcm;
	else if (play_pcm && play_pcm->kobj == kobj)
		apf = play_pcm;
	else
		return sprintf(buf, "0\n");

	uac_pcm = apf->private_data;
	if (!uac_pcm)
		return sprintf(buf, "0\n");

	return sprintf(buf, "%d\n", uac_pcm->uac_status);
}

static struct kobj_attribute attr_bridge_uac_status = __ATTR_RO(bridge_uac_status);

static ssize_t bridge_capture_volume_ctr_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "unsupport!");
}

static ssize_t bridge_capture_volume_ctr_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		 const char *buf, size_t len)
{
	char bufer;
	int ret = kstrtou8(buf, 10, &bufer);

	if (ret < 0) {
		pr_err("%s err!", __func__);
		return len;
	}
	pr_err("%s unsupport!", __func__);
	return len;
}

static struct kobj_attribute attr_bridge_capture_volume_ctr = __ATTR_RW(bridge_capture_volume_ctr);

int uac_pcm_get_capture_status(void)
{
	struct audio_pcm_function_t *audio_pcm = cap_pcm;

	if (!audio_pcm || !audio_pcm->get_status)
		return 0;
	return audio_pcm->get_status(audio_pcm);
}

int uac_pcm_get_playback_status(void)
{
	struct audio_pcm_function_t *audio_pcm = play_pcm;

	if (!audio_pcm || !audio_pcm->get_status)
		return 0;
	return audio_pcm->get_status(audio_pcm);
}

int uac_pcm_get_capture_hw(int *chmask, int *srate, int *ssize)
{
	struct audio_pcm_function_t *audio_pcm = cap_pcm;
	struct uac_pcm_t *uac_pcm;

	if (!audio_pcm || !audio_pcm->private_data)
		return -EINVAL;
	uac_pcm = audio_pcm->private_data;
	*chmask = uac_pcm->chmask;
	*srate = uac_pcm->srate;
	*ssize = uac_pcm->ssize;
	return 0;
}

int uac_pcm_get_playback_hw(int *chmask, int *srate, int *ssize)
{
	struct audio_pcm_function_t *audio_pcm = play_pcm;
	struct uac_pcm_t *uac_pcm;

	if (!audio_pcm || !audio_pcm->private_data)
		return -EINVAL;
	uac_pcm = audio_pcm->private_data;
	*chmask = uac_pcm->chmask;
	*srate = uac_pcm->srate;
	*ssize = uac_pcm->ssize;
	return 0;
}

int uac_pcm_start_capture(void)
{
	struct audio_pcm_function_t *audio_pcm = cap_pcm;
	struct uac_pcm_t *uac_pcm;

	if (!audio_pcm || !audio_pcm->rb)
		return 0;
	uac_pcm = audio_pcm->private_data;
	if (!uac_pcm)
		return 0;
	uac_pcm->uac_status = 1;
	return ring_buffer_go_empty(audio_pcm->rb);
}

int uac_pcm_stop_capture(void)
{
	struct audio_pcm_function_t *audio_pcm = cap_pcm;
	struct uac_pcm_t *uac_pcm;

	if (!audio_pcm || !audio_pcm->rb)
		return 0;
	uac_pcm = audio_pcm->private_data;
	if (!uac_pcm)
		return 0;
	uac_pcm->uac_status = 0;
	return 0;
}

int uac_pcm_start_playback(void)
{
	struct audio_pcm_function_t *audio_pcm = play_pcm;
	struct uac_pcm_t *uac_pcm;

	if (!audio_pcm || !audio_pcm->rb)
		return 0;
	uac_pcm = audio_pcm->private_data;
	if (!uac_pcm)
		return 0;
	uac_pcm->uac_status = 1;

	return ring_buffer_go_empty(audio_pcm->rb);
}

int uac_pcm_stop_playback(void)
{
	struct audio_pcm_function_t *audio_pcm = play_pcm;
	struct uac_pcm_t *uac_pcm;

	if (!audio_pcm || !audio_pcm->rb)
		return 0;
	uac_pcm = audio_pcm->private_data;
	if (!uac_pcm)
		return 0;
	uac_pcm->uac_status = 0;

	return ring_buffer_go_empty(audio_pcm->rb);
}

int uac_pcm_write_data(char *buf, unsigned int size)
{
	struct audio_pcm_function_t *audio_pcm = cap_pcm;
	struct audio_pcm_bridge_t *bridge;
	struct uac_pcm_t *uac_pcm;

	if (!audio_pcm || !audio_pcm->rb ||
		!audio_pcm->audio_bridge || !audio_pcm->private_data)
		return 0;
	bridge = audio_pcm->audio_bridge;
	uac_pcm = audio_pcm->private_data;
	uac_pcm->uac_status = 1;
	if (bridge->isolated_enable)
		return 0;
	else
		return no_thread_safe_ring_buffer_put(audio_pcm->rb, buf, size);
}

int uac_pcm_read_data(char *buf, unsigned int size)
{
	struct audio_pcm_function_t *audio_pcm = play_pcm;
	struct audio_pcm_bridge_t *bridge;
	struct uac_pcm_t *uac_pcm;

	if (!audio_pcm || !audio_pcm->rb ||
		!audio_pcm->audio_bridge || !audio_pcm->private_data)
		return 0;
	bridge = audio_pcm->audio_bridge;
	uac_pcm = audio_pcm->private_data;
	uac_pcm->uac_status = 1;
	if (bridge->isolated_enable)
		return 0;
	else
		return no_thread_safe_ring_buffer_get(audio_pcm->rb, buf, size);
}

int uac_pcm_ctl_capture(int cmd, int value)
{
	struct audio_pcm_function_t *audio_pcm = cap_pcm;
	struct audio_pcm_bridge_t *bridge;

	if (!audio_pcm || !audio_pcm->audio_bridge)
		return 0;
	bridge = audio_pcm->audio_bridge;

	bridge->bridge_ctr_cmd = cmd;
	bridge->bridge_ctr_value = value;
	schedule_work(&bridge->ctr_work);
	return 0;
}

int uac_pcm_ctl_playback(int cmd, int value)
{
	struct audio_pcm_function_t *audio_pcm = play_pcm;
	struct audio_pcm_bridge_t *bridge;

	if (!audio_pcm || !audio_pcm->audio_bridge)
		return 0;
	bridge = audio_pcm->audio_bridge;

	bridge->bridge_ctr_cmd = cmd;
	bridge->bridge_ctr_value = value;
	schedule_work(&bridge->ctr_work);
	return 0;
}

int uac_pcm_get_default_volume_capture(void)
{
	struct audio_pcm_function_t *audio_pcm = cap_pcm;
	struct audio_pcm_bridge_t *bridge;

	if (!audio_pcm || !audio_pcm->audio_bridge)
		return 0;
	bridge = audio_pcm->audio_bridge;

	return bridge->def_volume;
}

int uac_pcm_get_default_volume_playback(void)
{
	struct audio_pcm_function_t *audio_pcm = play_pcm;
	struct audio_pcm_bridge_t *bridge;

	if (!audio_pcm || !audio_pcm->audio_bridge)
		return 0;
	bridge = audio_pcm->audio_bridge;

	return bridge->def_volume;
}

static int uac_pcm_start(struct audio_pcm_function_t *audio_pcm)
{
	struct uac_pcm_t *uac_pcm;

	if (!audio_pcm || !audio_pcm->private_data)
		return -1;
	uac_pcm = audio_pcm->private_data;
	uac_pcm->run_flag = 1;
	return 0;
}

static int uac_pcm_stop(struct audio_pcm_function_t *audio_pcm)
{
	struct uac_pcm_t *uac_pcm;

	if (!audio_pcm || !audio_pcm->private_data)
		return -1;
	uac_pcm = audio_pcm->private_data;
	uac_pcm->run_flag = 0;
	return 0;
}

static int uac_pcm_set_hw(struct audio_pcm_function_t *audio_pcm,
					u8 channels, u32 rate, u32 format)
{
	int i;
	struct uac_pcm_t *uac_pcm;

	if (!audio_pcm || !audio_pcm->private_data)
		return -EINVAL;
	uac_pcm = audio_pcm->private_data;
	uac_pcm->srate = rate;
	for (i = 0; i < channels; i++)
		uac_pcm->chmask |= (1 << i);

	switch (format) {
	case PCM_FORMAT_S16_LE:
		uac_pcm->ssize = 2;
		break;
	case PCM_FORMAT_S24_3LE:
		uac_pcm->ssize = 3;
		break;
	case PCM_FORMAT_S32_LE:
		uac_pcm->ssize = 4;
		break;
	default:
		uac_pcm->ssize = 2;
		break;
	}
	return 0;
}

static int uac_pcm_get_status(struct audio_pcm_function_t *audio_pcm)
{
	struct uac_pcm_t *uac_pcm;
	struct audio_pcm_bridge_t *bridge;

	bridge = audio_pcm->audio_bridge;
	if (!audio_pcm || !audio_pcm->private_data || bridge->isolated_enable)
		return 0;

	uac_pcm = audio_pcm->private_data;

	return uac_pcm->run_flag;
}

static int uac_pcm_create_attribute
			(struct audio_pcm_function_t *audio_pcm, struct kobject *kobj)
{
	int ret = 0;

	if (!kobj)
		return 0;
	if (audio_pcm->modeid == PCM_CAPTURE)
		ret = sysfs_create_file(kobj, &attr_bridge_capture_volume_ctr.attr);
	ret = sysfs_create_file(kobj, &attr_bridge_uac_status.attr);
	audio_pcm->kobj = kobj;
	return ret;
}

static void uac_pcm_destroy_attribute
			(struct audio_pcm_function_t *audio_pcm, struct kobject *kobj)
{
	if (!kobj)
		return;
	if (audio_pcm->modeid == PCM_CAPTURE)
		sysfs_remove_file(kobj, &attr_bridge_capture_volume_ctr.attr);
	sysfs_remove_file(kobj, &attr_bridge_uac_status.attr);
	audio_pcm->kobj = NULL;
}

static void uac_pcm_control(struct audio_pcm_function_t *audio_pcm, int cmd, int value)
{
	if (!audio_pcm)
		return;
}

int uac_pcm_init(struct audio_pcm_function_t *audio_pcm, enum PCM_MODE mode)
{
	struct uac_pcm_t *uac_pcm;

	if (!audio_pcm)
		return -EINVAL;
	uac_pcm = vzalloc(sizeof(*uac_pcm));
	if (!uac_pcm)
		return -ENOMEM;

	audio_pcm->set_hw = uac_pcm_set_hw;
	audio_pcm->start = uac_pcm_start;
	audio_pcm->stop = uac_pcm_stop;
	audio_pcm->get_status = uac_pcm_get_status;
	audio_pcm->create_attr = uac_pcm_create_attribute;
	audio_pcm->destroy_attr = uac_pcm_destroy_attribute;
	audio_pcm->control = uac_pcm_control;
	audio_pcm->private_data = uac_pcm;
	if (mode == PCM_CAPTURE)
		cap_pcm = audio_pcm;
	else
		play_pcm = audio_pcm;
	return 0;
}

void uac_pcm_deinit(struct audio_pcm_function_t *audio_pcm)
{
	struct uac_pcm_t *uac_pcm;

	if (!audio_pcm || !audio_pcm->private_data)
		return;
	uac_pcm = audio_pcm->private_data;
	if (audio_pcm->modeid == PCM_CAPTURE)
		cap_pcm = NULL;
	else
		play_pcm = NULL;
	uac_pcm_stop(audio_pcm);
	vfree(uac_pcm);
}
