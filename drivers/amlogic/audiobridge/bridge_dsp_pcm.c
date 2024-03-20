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
#include <linux/amlogic/meson_mhu_common.h>
#include <linux/amlogic/scpi_protocol.h>
#include "../hifi4dsp/hifi4dsp_priv.h"
#include "bridge_common.h"

#include <linux/kthread.h>

#include <linux/delay.h>
#include <linux/fs.h>
#include <sound/pcm.h>
#include <linux/semaphore.h>
#include <uapi/linux/sched/types.h>

#include "ringbuffer.h"
#include "dsp_client_api.h"
#include "bridge_pcm_hal.h"
#include "bridge_dsp_card.h"

#define COREID SCPI_DSPA

#define LOOPBACK_CHANNELS        (2)
#define DSP_PARAM_CARD           2

#define GAIN_MAX 0
#define GAIN_MIN -40

struct dsp_pcm_param {
	u8  card;
	u32 device;
	u8  channels;
	u32 rate;
	u32 format;
	u32 period_size;
};

struct dsp_pcm_t {
	void *pcm_handle;
	struct device *dev;
	struct task_struct *thread_handle;
	struct task_struct *aux_thread_handle;
	struct dsp_pcm_param pcm_param;
	struct mutex lock;	/* lock to protect dsp bridge data*/
	struct semaphore sem;
	struct aml_aprocess *aprocess;
	u8 speaker_process_flag;
	u8 run_flag;
	int volume;
	int mute;
	int db_gain;
};

static struct audio_pcm_function_t *cap_pcm;
static struct audio_pcm_function_t *play_pcm;

const struct aml_dsp_id aml_dsp_adev[] = {
	{((PCM_DEV_TDMA << 1) | PCM_PLAYBACK), DEVICE_TDMOUT_A},
	{((PCM_DEV_TDMB << 1) | PCM_PLAYBACK), DEVICE_TDMOUT_B},
/*
 *	{((PCM_DEV_TDMB << 1) | PCM_CAPTURE), DEVICE_TDMIN_B},
 *	{((PCM_DEV_TDMA << 1) | PCM_CAPTURE), DEVICE_TDMIN_A},
 *	{((PCM_DEV_SPDIFIN << 1) | PCM_CAPTURE), DEVICE_SPDIFIN},
 *	{((PCM_DEV_LOOPBACK << 1) | PCM_CAPTURE), DEVICE_LOOPBACK},
 *	{((PCM_DEV_PDMIN << 1) | PCM_CAPTURE), DEVICE_PDMIN},
 */
	{((PCM_DEV_PROCESS << 1) | PCM_CAPTURE), DEVICE_LOOPBACK},
};

const struct aml_dsp_id aml_dsp_fmt[] = {
	{PCM_FORMAT_S8, DSP_PCM_FORMAT_S8},
	{PCM_FORMAT_S16_LE, DSP_PCM_FORMAT_S16_LE},
	{PCM_FORMAT_S16_BE, DSP_PCM_FORMAT_S16_BE},
	{PCM_FORMAT_S24_3LE, DSP_PCM_FORMAT_S24_3LE},
	{PCM_FORMAT_S24_3BE, DSP_PCM_FORMAT_S24_3BE},
	{PCM_FORMAT_S24_LE, DSP_PCM_FORMAT_S24_LE},
	{PCM_FORMAT_S24_BE, DSP_PCM_FORMAT_S24_BE},
	{PCM_FORMAT_S32_LE, DSP_PCM_FORMAT_S32_LE},
	{PCM_FORMAT_S32_BE, DSP_PCM_FORMAT_S32_BE},
};

int find_dsp_device_node(enum PCM_CARD card, enum PCM_MODE pcm_mode)
{
	int i;

	for (i = 0; i < sizeof(aml_dsp_adev) / sizeof(struct aml_dsp_id); i++) {
		if (aml_dsp_adev[i].value == ((card << 1) | pcm_mode))
			return aml_dsp_adev[i].id;
	}
	return -1;
}

int find_dsp_fmt_node(enum PCM_FORMAT fmt)
{
	int i;

	for (i = 0; i < sizeof(aml_dsp_fmt) / sizeof(struct aml_dsp_id); i++) {
		if (aml_dsp_fmt[i].value == fmt)
			return aml_dsp_fmt[i].id;
	}
	return -1;
}

static ssize_t bridge_capture_volume_ctr_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	struct dsp_pcm_t *dsp_pcm;

	if (!hifi4dsp_p[COREID] ||
		!hifi4dsp_p[COREID]->dsp || !hifi4dsp_p[COREID]->dsp->dspstarted)
		return 0;

	if (!cap_pcm || !cap_pcm->private_data) {
		pr_err("can't start up dsp bridge!\n");
		return 0;
	}
	dsp_pcm = cap_pcm->private_data;
	if (!dsp_pcm) {
		pr_err("the %s private is NULL!\n", find_mode_desc(cap_pcm->modeid));
		return 0;
	}

	mutex_lock(&dsp_pcm->lock);
	if (dsp_pcm->run_flag && dsp_pcm->pcm_handle)
		pcm_process_client_get_volume_gain(dsp_pcm->pcm_handle, &dsp_pcm->db_gain,
				PCM_CAPTURE, cap_pcm->dev, COREID);
	mutex_unlock(&dsp_pcm->lock);
	return sprintf(buf,
		"Volume: %d\nMuteState: %d\n(Volume-Down:%ld, Volume-Up:%ld, Volume-Mute:%ld)\n",
		dsp_pcm->volume, dsp_pcm->mute, VOLUME_DECRE, VOLUME_INCRE, VOLUME_MUTE);
}

static ssize_t bridge_capture_volume_ctr_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		 const char *buf, size_t len)
{
	char bufer;
	struct dsp_pcm_t *dsp_pcm;
	int ret = kstrtou8(buf, 10, &bufer);

	if (ret < 0)
		pr_err("%s err!", __func__);
	if (!hifi4dsp_p[COREID] ||
		!hifi4dsp_p[COREID]->dsp || !hifi4dsp_p[COREID]->dsp->dspstarted)
		return len;

	if (!cap_pcm || !cap_pcm->private_data) {
		pr_err("can't start up dsp bridge!\n");
		return len;
	}
	dsp_pcm = cap_pcm->private_data;

	if (bufer == VOLUME_DECRE) {
		if (dsp_pcm->volume > 0)
			dsp_pcm->volume--;
		if (!dsp_pcm->mute) {
			if (dsp_pcm->volume) {
				dsp_pcm->db_gain = dsp_pcm->volume * (GAIN_MAX - GAIN_MIN) /
					(VOLUME_MAX - VOLUME_MIN) + GAIN_MIN;
			} else {
				dsp_pcm->db_gain = -48;
			}
		}
	} else if (bufer == VOLUME_INCRE) {
		if (dsp_pcm->volume < 100)
			dsp_pcm->volume++;
		if (!dsp_pcm->mute)
			dsp_pcm->db_gain = dsp_pcm->volume * (GAIN_MAX - GAIN_MIN) /
					(VOLUME_MAX - VOLUME_MIN) + GAIN_MIN;
	} else if (bufer == VOLUME_MUTE) {
		if (dsp_pcm->mute) {
			dsp_pcm->mute = 0;
			if (dsp_pcm->volume)
				dsp_pcm->db_gain = dsp_pcm->volume * (GAIN_MAX - GAIN_MIN) /
					(VOLUME_MAX - VOLUME_MIN) + GAIN_MIN;
			else
				dsp_pcm->db_gain = -48;
		} else {
			dsp_pcm->mute = 1;
			dsp_pcm->db_gain = -48;
		}
	} else {
		pr_err("%s err unsupport %d!", __func__, bufer);
		return len;
	}

	mutex_lock(&dsp_pcm->lock);
	if (dsp_pcm->run_flag && dsp_pcm->pcm_handle)
		pcm_process_client_set_volume_gain(dsp_pcm->pcm_handle, dsp_pcm->db_gain,
				PCM_CAPTURE, cap_pcm->dev, COREID);

	mutex_unlock(&dsp_pcm->lock);
	return len;
}

static struct kobj_attribute attr_bridge_capture_volume_ctr = __ATTR_RW(bridge_capture_volume_ctr);

static ssize_t bridge_playback_process_ctr_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	struct dsp_pcm_t *dsp_pcm;

	if (!hifi4dsp_p[COREID] ||
		!hifi4dsp_p[COREID]->dsp || !hifi4dsp_p[COREID]->dsp->dspstarted)
		return 0;

	if (!play_pcm || !play_pcm->private_data) {
		pr_err("can't start up dsp bridge!\n");
		return 0;
	}
	dsp_pcm = play_pcm->private_data;
	if (!dsp_pcm) {
		pr_err("the %s private is NULL!\n", find_mode_desc(play_pcm->modeid));
		return 0;
	}

	return sprintf(buf, "%s\n", dsp_pcm->speaker_process_flag ? "enable" : "disable");
}

static ssize_t bridge_playback_process_ctr_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		 const char *buf, size_t len)
{
	int check;
	struct dsp_pcm_t *dsp_pcm;
	int ret = kstrtouint(buf, 10, &check);

	if (ret < 0)
		pr_err("%s err!", __func__);
	if (!hifi4dsp_p[COREID] ||
		!hifi4dsp_p[COREID]->dsp || !hifi4dsp_p[COREID]->dsp->dspstarted)
		return len;

	if (!play_pcm || !play_pcm->private_data) {
		pr_err("can't start up dsp bridge!\n");
		return len;
	}
	dsp_pcm = play_pcm->private_data;
	if (check)
		dsp_pcm->speaker_process_flag = true;
	else
		dsp_pcm->speaker_process_flag = false;

	return len;
}

static struct kobj_attribute attr_bridge_playback_process_ctr =
		__ATTR_RW(bridge_playback_process_ctr);

static int thread_capture_complete(void *data)
{
	struct audio_pcm_function_t *info = data;
	struct dsp_pcm_t *dsp_pcm = (struct dsp_pcm_t *)info->private_data;
	struct rpc_pcm_config pconfig;
	u32 read_size = 0, expected_sleep_us = 0, real_sleep_us = 0;
	u32 sema_wait_timeout = 0;
	char *buf = NULL;

	pconfig.channels = dsp_pcm->pcm_param.channels;
	pconfig.rate = dsp_pcm->pcm_param.rate;
	pconfig.format = dsp_pcm->pcm_param.format;
	pconfig.period_size = DSP_PERIOD_SIZE;

	expected_sleep_us = 1000 * 1000 * pconfig.period_size *
		DSP_PERIOD_SIZE_TO_ARM_PERIOD_SIZE / pconfig.rate;
	sema_wait_timeout = expected_sleep_us * 5;

	/*
	 * Ensure that the ringbuffer consumption
	 * speed is greater than the production speed
	 **/
	if (pconfig.period_size < 1024)
		real_sleep_us = expected_sleep_us - 300;
	else
		real_sleep_us = expected_sleep_us * 63 / 64;

	read_size = pconfig.period_size * (pconfig.channels - LOOPBACK_CHANNELS) *
			pcm_client_format_to_bytes(pconfig.format) *
			DSP_PERIOD_SIZE_TO_ARM_PERIOD_SIZE;

	buf = vmalloc(read_size);
	if (!buf) {
		vfree(buf);
		pr_err("vmalloc failed\n");
		return -1;
	}
	memset(buf, 0, read_size);

	while (dsp_pcm->run_flag && !kthread_should_stop()) {
		if (down_timeout(&dsp_pcm->sem, usecs_to_jiffies(sema_wait_timeout)))
			continue;

		if (!no_thread_safe_ring_buffer_get(info->rb, buf, read_size)) {
			usleep_range(expected_sleep_us / 2, expected_sleep_us);
		} else {
			aml_aprocess_complete(dsp_pcm->aprocess, buf, read_size);
			usleep_range(real_sleep_us - 200, real_sleep_us);
		}
	}
	vfree(buf);

	return 0;
}

static int thread_capture(void *data)
{
	struct audio_pcm_function_t *info = (struct audio_pcm_function_t *)data;
	struct audio_pcm_bridge_t *bridge = info->audio_bridge;
	struct dsp_pcm_t *dsp_pcm;
	unsigned int size;
	struct rpc_pcm_config pconfig;
	struct buf_info buf;
	u32 read_size = 0, sleep_us = 0;

	if (!info || !info->private_data)
		return -EINVAL;
	dsp_pcm = (struct dsp_pcm_t *)info->private_data;

	while (!hifi4dsp_p[COREID] || !hifi4dsp_p[COREID]->dsp ||
			!hifi4dsp_p[COREID]->dsp->dspstarted) {
		if (!dsp_pcm->run_flag || kthread_should_stop())
			return -EINVAL;
		msleep(100);
	}
	msleep(100);
	memset(&buf, 0, sizeof(buf));
	pconfig.channels = dsp_pcm->pcm_param.channels;
	pconfig.rate = dsp_pcm->pcm_param.rate;
	pconfig.format = dsp_pcm->pcm_param.format;
	pconfig.period_size = DSP_PERIOD_SIZE;
	pconfig.period_count = PERIOD_COUNT;
	sleep_us = 1000 * 1000 * pconfig.period_size *
		DSP_PERIOD_SIZE_TO_ARM_PERIOD_SIZE / pconfig.rate / 3;

	read_size = pconfig.period_size * (pconfig.channels - LOOPBACK_CHANNELS) *
			pcm_client_format_to_bytes(pconfig.format) *
			DSP_PERIOD_SIZE_TO_ARM_PERIOD_SIZE;
	dsp_pcm->pcm_handle = audio_device_open(dsp_pcm->pcm_param.card,
					dsp_pcm->pcm_param.device,
					PCM_IN, &pconfig, info->dev, COREID);
	if (!dsp_pcm->pcm_handle) {
		pr_err("can't open libpcm device!\n");
		return -ENXIO;
	}

	pcm_process_client_set_volume_gain(dsp_pcm->pcm_handle, dsp_pcm->db_gain,
					PCM_CAPTURE, info->dev, COREID);
	while (dsp_pcm->run_flag && !kthread_should_stop()) {
		buf.size = read_size;
		size = pcm_process_client_dqbuf(dsp_pcm->pcm_handle, &buf, &buf,
				PROCESSBUF, info->dev, COREID);
		if (buf.size) {
			dma_sync_single_for_device
					(hifi4dsp_p[COREID]->dsp->dev,
					 (phys_addr_t)buf.phyaddr,
					 buf.size,
					 DMA_FROM_DEVICE);
			if (dsp_pcm->aprocess->status) {
				no_thread_safe_ring_buffer_put(info->rb, buf.viraddr, buf.size);
				up(&dsp_pcm->sem);
				//aml_aprocess_complete(dsp_pcm->aprocess, buf.viraddr, buf.size);
			}

			if (!bridge->isolated_enable)
				no_thread_safe_ring_buffer_put(info->rb, buf.viraddr, buf.size);
		} else {
			usleep_range(sleep_us, sleep_us * 2);
		}
	}
	pcm_process_client_close(dsp_pcm->pcm_handle, info->dev, COREID);
	dsp_pcm->run_flag = 0;
	return 0;
}

static int thread_playback(void *data)
{
	struct audio_pcm_function_t *info = (struct audio_pcm_function_t *)data;
	struct audio_pcm_bridge_t *bridge = info->audio_bridge;
	struct dsp_pcm_t *dsp_pcm;
	struct rpc_pcm_config pconfig;
	struct buf_info buf;
	u32 send_size = 0, sleep_us = 0;

	if (!info || !info->private_data)
		return -EINVAL;
	dsp_pcm = (struct dsp_pcm_t *)info->private_data;

	while (!hifi4dsp_p[COREID] || !hifi4dsp_p[COREID]->dsp ||
			!hifi4dsp_p[COREID]->dsp->dspstarted) {
		if (!dsp_pcm->run_flag || kthread_should_stop())
			return -EINVAL;
		msleep(100);
	}
	msleep(100);
	memset(&buf, 0, sizeof(buf));
	pconfig.channels = dsp_pcm->pcm_param.channels;
	pconfig.rate = dsp_pcm->pcm_param.rate;
	pconfig.format = dsp_pcm->pcm_param.format;
	pconfig.period_size = DSP_PERIOD_SIZE;
	pconfig.period_count = PERIOD_COUNT;
	send_size = pconfig.period_size * pconfig.channels *
		pcm_client_format_to_bytes(pconfig.format) * DSP_PERIOD_SIZE_TO_ARM_PERIOD_SIZE;
	buf.size = send_size;
	sleep_us = 1000 * 1000 * pconfig.period_size *
		DSP_PERIOD_SIZE_TO_ARM_PERIOD_SIZE / pconfig.rate / 3;

	pr_info("open playback device!\n");
	dsp_pcm->pcm_handle = audio_device_open(dsp_pcm->pcm_param.card,
					dsp_pcm->pcm_param.device,
					PCM_OUT, &pconfig, info->dev, COREID);
	if (!dsp_pcm->pcm_handle) {
		pr_err("can't open playback device!\n");
		return -ENXIO;
	}

	pr_info("get share mm!\n");
	/* first call qbuf for get ready buffer address */
	pcm_process_client_qbuf(dsp_pcm->pcm_handle, &buf,
						RAWBUF, info->dev, COREID);

	ring_buffer_go_empty(info->rb);
	pcm_process_client_set_volume_gain(dsp_pcm->pcm_handle, dsp_pcm->db_gain,
					PCM_PLAYBACK, dsp_pcm->dev, COREID);
	while (dsp_pcm->run_flag && !kthread_should_stop()) {
		if (bridge->isolated_enable) {
			if (!dsp_pcm->aprocess->status) {
				usleep_range(sleep_us, sleep_us * 2);
				continue;
			}
			if (!aml_aprocess_complete(dsp_pcm->aprocess, buf.viraddr, buf.size)) {
				usleep_range(sleep_us, sleep_us * 2);
				continue;
			}
		} else {
			if (!no_thread_safe_ring_buffer_get(info->rb, buf.viraddr, buf.size))
				memset(buf.viraddr, 0, buf.size);
		}
		dma_sync_single_for_device
					(hifi4dsp_p[COREID]->dsp->dev,
					 (phys_addr_t)buf.phyaddr,
					 buf.size,
					 DMA_TO_DEVICE);
		buf.size = send_size;
		pcm_process_client_qbuf(dsp_pcm->pcm_handle, &buf,
						RAWBUF, info->dev, COREID);
	}
	pcm_process_client_close(dsp_pcm->pcm_handle, info->dev, COREID);
	dsp_pcm->run_flag = 0;
	return 0;
}

static int dsp_pcm_start(struct audio_pcm_function_t *audio_pcm)
{
	int rc = 0;
	struct dsp_pcm_t *dsp_pcm;
	struct sched_param param = { .sched_priority = 99 };

	if (!audio_pcm || !audio_pcm->private_data) {
		pr_err("the bridge info is NULL!\n");
		return -EINVAL;
	}
	dsp_pcm = audio_pcm->private_data;

	mutex_lock(&dsp_pcm->lock);
	if (dsp_pcm->run_flag) {
		pr_err("the %s bridge is running!\n", find_mode_desc(audio_pcm->modeid));
		mutex_unlock(&dsp_pcm->lock);
		return 0;
	}

	if (dsp_pcm->thread_handle) {
		kthread_stop(dsp_pcm->thread_handle);
		dsp_pcm->thread_handle = NULL;
	}

	dsp_pcm->run_flag = 1;
	if (audio_pcm->modeid == PCM_CAPTURE) {
		dsp_pcm->thread_handle = kthread_run(thread_capture, audio_pcm, "dsp_cap");
		dsp_pcm->aux_thread_handle = kthread_run(thread_capture_complete,
							audio_pcm, "dsp_cap_complete");
		sched_setscheduler(dsp_pcm->thread_handle, SCHED_FIFO, &param);
		sched_setscheduler(dsp_pcm->aux_thread_handle, SCHED_FIFO, &param);
	} else {
		dsp_pcm->thread_handle = kthread_run(thread_playback, audio_pcm, "dsp_play");
		dsp_pcm->aux_thread_handle = NULL;
		sched_setscheduler(dsp_pcm->thread_handle, SCHED_FIFO, &param);
	}

	if (IS_ERR(dsp_pcm->thread_handle)) {
		dsp_pcm->thread_handle = NULL;
		dsp_pcm->run_flag = 0;
		rc = PTR_ERR(dsp_pcm->thread_handle);
		pr_err(" error %d create %s thread", rc, find_mode_desc(audio_pcm->modeid));
		mutex_unlock(&dsp_pcm->lock);
		return -EINVAL;
	}
	pr_info("dsp %s start up!\n", find_mode_desc(audio_pcm->modeid));
	mutex_unlock(&dsp_pcm->lock);
	return 0;
}

static int dsp_pcm_stop(struct audio_pcm_function_t *audio_pcm)
{
	struct dsp_pcm_t *dsp_pcm;

	if (!audio_pcm || !audio_pcm->private_data) {
		pr_err("the bridge info is NULL!\n");
		return -EINVAL;
	}

	dsp_pcm = audio_pcm->private_data;

	mutex_lock(&dsp_pcm->lock);
	dsp_pcm->run_flag = 0;
	if (dsp_pcm->aux_thread_handle) {
		kthread_stop(dsp_pcm->aux_thread_handle);
		dsp_pcm->aux_thread_handle = NULL;
	}
	if (dsp_pcm->thread_handle) {
		kthread_stop(dsp_pcm->thread_handle);
		dsp_pcm->thread_handle = NULL;
	}
	pr_info("dsp %s stop!\n", find_mode_desc(audio_pcm->modeid));
	mutex_unlock(&dsp_pcm->lock);
	return 0;
}

static int dsp_pcm_set_hw(struct audio_pcm_function_t *audio_pcm, u8 channels, u32 rate, u32 format)
{
	struct dsp_pcm_t *dsp_pcm;
	int alsa_format;

	if (!audio_pcm || !audio_pcm->private_data) {
		pr_err("the bridge info is NULL!\n");
		return -EINVAL;
	}
	dsp_pcm = audio_pcm->private_data;

	dsp_pcm->pcm_param.channels = channels;
	dsp_pcm->pcm_param.rate = rate;
	dsp_pcm->pcm_param.format = find_dsp_fmt_node(format);
	switch (format) {
	case PCM_FORMAT_S8:
		alsa_format = SNDRV_PCM_FMTBIT_S8;
		break;
	case PCM_FORMAT_S16_LE:
		alsa_format = SNDRV_PCM_FMTBIT_S16;
		break;
	case PCM_FORMAT_S32_LE:
		alsa_format = SNDRV_PCM_FMTBIT_S32;
		break;
	default:
		alsa_format = SNDRV_PCM_FMTBIT_S16;
		break;
	}

	if (audio_pcm->modeid == PCM_CAPTURE)
		aml_aprocess_set_hw(dsp_pcm->aprocess, (channels - LOOPBACK_CHANNELS),
			alsa_format, rate, DSP_PERIOD_SIZE * DSP_PERIOD_SIZE_TO_ARM_PERIOD_SIZE);
	else
		aml_aprocess_set_hw(dsp_pcm->aprocess, channels,
			alsa_format, rate, DSP_PERIOD_SIZE * DSP_PERIOD_SIZE_TO_ARM_PERIOD_SIZE);
	return 0;
}

static int dsp_pcm_create_sound_card(struct audio_pcm_function_t *audio_pcm)
{
	struct dsp_pcm_t *dsp_pcm;

	if (!audio_pcm || !audio_pcm->private_data) {
		pr_err("the bridge info is NULL!\n");
		return -EINVAL;
	}
	dsp_pcm = audio_pcm->private_data;

	dsp_pcm->aprocess = aml_aprocess_init(audio_pcm->dev,
			"aprocess", audio_pcm->dev->kobj.name, audio_pcm->modeid);
	return 0;
}

static void dsp_pcm_destroy_sound_card(struct audio_pcm_function_t *audio_pcm)
{
	struct dsp_pcm_t *dsp_pcm;

	if (!audio_pcm || !audio_pcm->private_data) {
		pr_err("the bridge info is NULL!\n");
		return;
	}
	dsp_pcm = audio_pcm->private_data;

	aml_aprocess_destroy(dsp_pcm->aprocess);
}

static int dsp_pcm_get_status(struct audio_pcm_function_t *audio_pcm)
{
	struct dsp_pcm_t *dsp_pcm;

	if (!audio_pcm || !audio_pcm->private_data)
		return 0;
	dsp_pcm = audio_pcm->private_data;

	return dsp_pcm->run_flag;
}

static int dsp_pcm_create_attribute(struct audio_pcm_function_t *audio_pcm, struct kobject *kobj)
{
	if (!kobj)
		return 0;
	if (audio_pcm->modeid == PCM_CAPTURE)
		return sysfs_create_file(kobj, &attr_bridge_capture_volume_ctr.attr);
	else
		return sysfs_create_file(kobj, &attr_bridge_playback_process_ctr.attr);
}

static void dsp_pcm_destroy_attribute(struct audio_pcm_function_t *audio_pcm, struct kobject *kobj)
{
	if (!kobj)
		return;
	if (audio_pcm->modeid == PCM_CAPTURE)
		sysfs_remove_file(kobj, &attr_bridge_capture_volume_ctr.attr);
	else
		sysfs_remove_file(kobj, &attr_bridge_playback_process_ctr.attr);
}

static void dsp_pcm_control(struct audio_pcm_function_t *audio_pcm, int cmd, int value)
{
	struct dsp_pcm_t *dsp_pcm;

	if (!audio_pcm || !audio_pcm->private_data)
		return;

	dsp_pcm = audio_pcm->private_data;

	if (cmd == PCM_VOLUME) {
		dsp_pcm->volume = value;
		if (!dsp_pcm->mute) {
			if (dsp_pcm->volume)
				dsp_pcm->db_gain = dsp_pcm->volume * (GAIN_MAX - GAIN_MIN) /
					(VOLUME_MAX - VOLUME_MIN) + GAIN_MIN;
			else
				dsp_pcm->db_gain = -48;
		}

		if (!hifi4dsp_p[COREID] ||
			!hifi4dsp_p[COREID]->dsp || !hifi4dsp_p[COREID]->dsp->dspstarted)
			return;

		mutex_lock(&dsp_pcm->lock);
		if (dsp_pcm->run_flag && dsp_pcm->pcm_handle)
			pcm_process_client_set_volume_gain(dsp_pcm->pcm_handle, dsp_pcm->db_gain,
					audio_pcm->modeid, dsp_pcm->dev, COREID);

		mutex_unlock(&dsp_pcm->lock);
	}
}

int dsp_pcm_init(struct audio_pcm_function_t *audio_pcm,
			enum PCM_CARD pcm_card, enum PCM_MODE mode)
{
	struct dsp_pcm_t *dsp_pcm;

	if (!audio_pcm)
		return -EINVAL;
	dsp_pcm = vzalloc(sizeof(*dsp_pcm));
	if (!dsp_pcm)
		return -ENOMEM;
	dsp_pcm->pcm_param.device = find_dsp_device_node(pcm_card, mode);
	if (dsp_pcm->pcm_param.device < 0) {
		vfree(dsp_pcm);
		pr_err("not support this device!\n");
		return -EINVAL;
	}
	dsp_pcm->dev = audio_pcm->dev;
	dsp_pcm->pcm_param.card = pcm_card;
	dsp_pcm->volume = VOLUME_MAX;
	dsp_pcm->mute = 0;
	dsp_pcm->db_gain = dsp_pcm->volume * (GAIN_MAX - GAIN_MIN) /
				(VOLUME_MAX - VOLUME_MIN) + GAIN_MIN;
	audio_pcm->set_hw = dsp_pcm_set_hw;
	audio_pcm->start = dsp_pcm_start;
	audio_pcm->stop = dsp_pcm_stop;
	audio_pcm->get_status = dsp_pcm_get_status;
	audio_pcm->create_attr = dsp_pcm_create_attribute;
	audio_pcm->destroy_attr = dsp_pcm_destroy_attribute;
	audio_pcm->create_virtual_sound_card = dsp_pcm_create_sound_card;
	audio_pcm->destroy_virtual_sound_card = dsp_pcm_destroy_sound_card;
	audio_pcm->control = dsp_pcm_control;
	audio_pcm->private_data = dsp_pcm;
	if (mode == PCM_CAPTURE)
		cap_pcm = audio_pcm;
	else
		play_pcm = audio_pcm;
	mutex_init(&dsp_pcm->lock);
	sema_init(&dsp_pcm->sem, 0);
	return 0;
}

void dsp_pcm_deinit(struct audio_pcm_function_t *audio_pcm)
{
	struct dsp_pcm_t *dsp_pcm;

	if (!audio_pcm || !audio_pcm->private_data)
		return;
	dsp_pcm = audio_pcm->private_data;
	if (audio_pcm->modeid == PCM_CAPTURE)
		cap_pcm = NULL;
	else
		play_pcm = NULL;
	dsp_pcm_stop(audio_pcm);
	mutex_destroy(&dsp_pcm->lock);
	vfree(dsp_pcm);
}
