/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2023 Amlogic, Inc. All rights reserved.
 */

#ifndef _BRIDGE_COMMON_H
#define _BRIDGE_COMMON_H

#include <linux/kernel.h>
#include <linux/types.h>

enum PCM_CORE {
	PCM_ARM = 0,
	PCM_DSP,
	PCM_UAC,
};

enum PCM_CARD {
	PCM_DEV_NONE = 0,
	PCM_DEV_TDMA,
	PCM_DEV_TDMB,
	PCM_DEV_SPDIFIN,
	PCM_DEV_LOOPBACK,
	PCM_DEV_PDMIN,
	PCM_DEV_PROCESS,
};

enum PCM_MODE {
	 PCM_CAPTURE = 0,
	 PCM_PLAYBACK,
};

enum PCM_FORMAT {
	PCM_FORMAT_S8 = 0,
	PCM_FORMAT_S16_LE,
	PCM_FORMAT_S16_BE,
	PCM_FORMAT_S24_LE,
	PCM_FORMAT_S24_BE,
	PCM_FORMAT_S24_3LE,
	PCM_FORMAT_S24_3BE,
	PCM_FORMAT_S32_LE,
	PCM_FORMAT_S32_BE,
	PCM_FORMAT_MAX
};

enum PCM_CONTROL {
	PCM_VOLUME = 0,
	PCM_MUTE
};

#define VOLUME_DECRE BIT(0)
#define VOLUME_INCRE BIT(1)
#define VOLUME_MUTE  BIT(2)
#define VOLUME_PLAY  BIT(3)

#define VOLUME_MAX 100
#define VOLUME_MIN 0

/* dsp use smaller period size for low latency but will cause mailbox will
 * communicating too frequently so mailbox should use bigger period size
 * arm period size = dsp period size * DSP_PERIOD_SIZE_TO_ARM_PERIOD_SIZE
 */
#define DSP_PERIOD_SIZE_TO_ARM_PERIOD_SIZE (1)
#define PERIOD_SIZE_MAX (2048)
#define PERIOD_COUNT    (4)
#define DSP_PERIOD_SIZE (1024)

struct audio_pcm_function_t {
	struct ring_buffer *rb;
	u32 ring_buffer_size;
	enum PCM_CORE coreid;
	enum PCM_CARD cardid;
	enum PCM_MODE modeid;
	u32 channels;
	u32 rate;
	u32 format;
	u8 enable_create_card;
	int (*set_hw)(struct audio_pcm_function_t *audio_pcm,
							u8 channels, u32 rate, u32 format);
	int (*start)(struct audio_pcm_function_t *audio_pcm);
	int (*stop)(struct audio_pcm_function_t *audio_pcm);
	int (*get_status)(struct audio_pcm_function_t *audio_pcm);
	int (*create_attr)(struct audio_pcm_function_t *audio_pcm,
							struct kobject *kobj);
	void (*destroy_attr)(struct audio_pcm_function_t *audio_pcm,
							struct kobject *kobj);
	int (*create_virtual_sound_card)(struct audio_pcm_function_t *audio_pcm);
	void (*destroy_virtual_sound_card)(struct audio_pcm_function_t *audio_pcm);
	void (*control)(struct audio_pcm_function_t *audio_pcm, int cmd, int value);
	void *private_data;
	struct device *dev;
	struct kobject *kobj;
	struct audio_pcm_function_t *opposite;
	struct audio_pcm_bridge_t *audio_bridge;
};

struct audio_pcm_bridge_t {
	unsigned int id;
	struct kobject *kobj;
	u8 enable;
	u8 isolated_enable;
	int def_volume;
	int bridge_ctr_cmd;
	int bridge_ctr_value;
	struct work_struct ctr_work;
	struct kobj_attribute enable_attr;
	struct kobj_attribute line_attr;
	struct kobj_attribute ringbuf_attr;
	struct kobj_attribute isolated_attr;
	struct ring_buffer *rb;
	struct list_head pcm_list;
	struct audio_pcm_function_t *audio_cap;
	struct audio_pcm_function_t *audio_play;
};

#endif         /*_BRIDGE_COMMON_H*/
