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
#include <linux/delay.h>
#include <linux/fs.h>
#include "bridge_common.h"
#include "ringbuffer.h"
#include "bridge_pcm_hal.h"

#define DRIVER_NAME   "audio-bridge"
#define PREFIX	"aml-audio-line,"

static struct list_head bridge_devs = LIST_HEAD_INIT(bridge_devs);

static ssize_t bridge_enable_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	struct audio_pcm_bridge_t *bridge =
		container_of(attr, struct audio_pcm_bridge_t, enable_attr);
	if (!bridge)
		return sprintf(buf, "%s err\n", attr->attr.name);
	return sprintf(buf, "%s\n", bridge->enable ? "enable" : "disable");
}

static ssize_t bridge_enable_store(struct kobject *kobj,
		 struct kobj_attribute *attr,
		 const char *buf, size_t len)
{
	int ret;
	int check;
	struct audio_pcm_bridge_t *bridge =
		container_of(attr, struct audio_pcm_bridge_t, enable_attr);
	if (!bridge)
		return len;

	ret = kstrtouint(buf, 10, &check);
	if (ret < 0) {
		pr_err("%s err!", __func__);
		return len;
	}
	if (check) {
		bridge->enable = true;
		bridge->audio_cap->start(bridge->audio_cap);
		bridge->audio_play->start(bridge->audio_play);
	} else {
		bridge->enable = false;
		bridge->audio_cap->stop(bridge->audio_cap);
		bridge->audio_play->stop(bridge->audio_play);
	}
	return len;
}

static ssize_t bridge_line_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	struct audio_pcm_bridge_t *bridge =
		container_of(attr, struct audio_pcm_bridge_t, line_attr);
	if (!bridge)
		return sprintf(buf, "%s err\n", attr->attr.name);
	return sprintf(buf, "%s-%s-%s<--->%s-%s-%s\n",
				find_core_desc(bridge->audio_cap->coreid),
				find_card_desc(bridge->audio_cap->cardid),
				find_mode_desc(bridge->audio_cap->modeid),
				find_core_desc(bridge->audio_play->coreid),
				find_card_desc(bridge->audio_play->cardid),
				find_mode_desc(bridge->audio_play->modeid));
}

static ssize_t bridge_ringbuf_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	struct audio_pcm_bridge_t *bridge =
		container_of(attr, struct audio_pcm_bridge_t, ringbuf_attr);
	if (!bridge)
		return sprintf(buf, "%s err\n", attr->attr.name);
	return sprintf(buf, "size:%d, use:%d free:%d\n", ring_buffer_len(bridge->rb),
		ring_buffer_used_len(bridge->rb), ring_buffer_free_len(bridge->rb));
}

static ssize_t bridge_isolated_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	struct audio_pcm_bridge_t *bridge =
		container_of(attr, struct audio_pcm_bridge_t, isolated_attr);
	if (!bridge)
		return sprintf(buf, "%s err\n", attr->attr.name);
	return sprintf(buf, "%s\n", bridge->isolated_enable ? "enable" : "disable");
}

static ssize_t bridge_isolated_store(struct kobject *kobj,
		 struct kobj_attribute *attr,
		 const char *buf, size_t len)
{
	int ret;
	int check;
	struct audio_pcm_bridge_t *bridge =
		container_of(attr, struct audio_pcm_bridge_t, isolated_attr);
	if (!bridge)
		return len;

	ret = kstrtouint(buf, 10, &check);
	if (ret < 0) {
		pr_err("%s err!", __func__);
		return len;
	}
	if (check)
		bridge->isolated_enable = true;
	else
		bridge->isolated_enable = false;

	return len;
}

static int aml_bridge_pcm_format2bytes(enum PCM_FORMAT format)
{
	int format_bytes = 0;

	switch (format) {
	case PCM_FORMAT_S8:
		format_bytes = 1;
		break;
	case PCM_FORMAT_S16_LE:
	case PCM_FORMAT_S16_BE:
		format_bytes = 2;
		break;
	case PCM_FORMAT_S24_3LE:
	case PCM_FORMAT_S24_3BE:
		format_bytes = 3;
		break;
	case PCM_FORMAT_S24_LE:
	case PCM_FORMAT_S24_BE:
	case PCM_FORMAT_S32_LE:
	case PCM_FORMAT_S32_BE:
		format_bytes = 4;
		break;
	default:
		format_bytes = 2;
	}

	return format_bytes;
}

static int aml_bridge_calculate_ring_buffer_size(struct audio_pcm_function_t *pcm_function)
{
	int format_bytes = 0;

	if (!pcm_function)
		return -1;

	format_bytes = aml_bridge_pcm_format2bytes(pcm_function->format);

	return pcm_function->channels * format_bytes * PERIOD_SIZE_MAX;
}

static struct audio_pcm_function_t *aml_bridge_parse_of(struct device *dev,
		struct device_node *node, char *name)
{
	int ret;
	u32 core, card;
	enum PCM_MODE mode;
	struct audio_pcm_function_t *audio_p;
	struct device_node *nb = of_get_child_by_name(node, name);
	u32 enable = 0;

	if (!nb)
		goto err_parse0;

	of_property_read_u32(nb, "core", &core);
	of_property_read_u32(nb, "sound-card", &card);
	if (!strcmp(name, "src"))
		mode = PCM_CAPTURE;
	else
		mode = PCM_PLAYBACK;
	audio_p = audio_pcm_alloc(dev, core, card, mode);
	if (!audio_p)
		goto err_parse0;

	of_property_read_u32(nb, "channels", &audio_p->channels);
	of_property_read_u32(nb, "sample-rate", &audio_p->rate);
	of_property_read_u32(nb, "format", &audio_p->format);

	of_property_read_u32(nb, "create-vir-card", &enable);
	if (enable && audio_p->create_virtual_sound_card)
		audio_p->create_virtual_sound_card(audio_p);
	audio_p->enable_create_card = enable;
	audio_p->ring_buffer_size = aml_bridge_calculate_ring_buffer_size(audio_p);

	if (!audio_p->set_hw)
		goto err_parse1;
	ret = audio_p->set_hw(audio_p, audio_p->channels, audio_p->rate, audio_p->format);
	if (ret < 0)
		goto err_parse1;

	pr_info("%s [core:%d card:%d] channels:%d rate:%d format:%d create-vcard:%d rb size:%d\n",
				name, core, card, audio_p->channels,
				audio_p->rate, audio_p->format, enable, audio_p->ring_buffer_size);
	return audio_p;
err_parse1:
	if (enable && audio_p->destroy_virtual_sound_card)
		audio_p->destroy_virtual_sound_card(audio_p);
	audio_pcm_free(audio_p);
err_parse0:
	return NULL;
}

static int aml_bridge_create_attribute(struct device *dev, struct audio_pcm_bridge_t *bridge)
{
	int ret = 0;
	char bridge_name[128];

	snprintf(bridge_name, sizeof(bridge_name), "bridge%d", bridge->id);
	bridge->kobj = kobject_create_and_add(bridge_name, &dev->kobj);
	if (!bridge->kobj) {
		ret = -ENOMEM;
		goto err_attr0;
	}
	bridge->enable_attr.attr.name = __stringify(enable);
	bridge->enable_attr.attr.mode = VERIFY_OCTAL_PERMISSIONS(0644);
	bridge->enable_attr.show = bridge_enable_show;
	bridge->enable_attr.store = bridge_enable_store;
	ret = sysfs_create_file(bridge->kobj, &bridge->enable_attr.attr);
	if (ret < 0) {
		ret = -ENOMEM;
		goto err_attr1;
	}
	bridge->line_attr.attr.name = __stringify(bridge_line);
	bridge->line_attr.attr.mode = VERIFY_OCTAL_PERMISSIONS(0444);
	bridge->line_attr.show  = bridge_line_show;
	ret = sysfs_create_file(bridge->kobj, &bridge->line_attr.attr);
	if (ret < 0) {
		ret = -ENOMEM;
		goto err_attr2;
	}
	bridge->ringbuf_attr.attr.name = __stringify(bridge_ringbuf);
	bridge->ringbuf_attr.attr.mode = VERIFY_OCTAL_PERMISSIONS(0444);
	bridge->ringbuf_attr.show  = bridge_ringbuf_show;
	ret = sysfs_create_file(bridge->kobj, &bridge->ringbuf_attr.attr);
	if (ret < 0) {
		ret = -ENOMEM;
		goto err_attr3;
	}
	bridge->isolated_attr.attr.name = __stringify(bridge_isolated);
	bridge->isolated_attr.attr.mode = VERIFY_OCTAL_PERMISSIONS(0644);
	bridge->isolated_attr.show  = bridge_isolated_show;
	bridge->isolated_attr.store = bridge_isolated_store;
	ret = sysfs_create_file(bridge->kobj, &bridge->isolated_attr.attr);
	if (ret < 0) {
		ret = -ENOMEM;
		goto err_attr4;
	}
	if (bridge->audio_cap->create_attr) {
		ret = bridge->audio_cap->create_attr(bridge->audio_cap, bridge->kobj);
		if (ret < 0) {
			ret = -EINVAL;
			goto err_attr5;
		}
	}
	if (bridge->audio_play->create_attr) {
		ret = bridge->audio_play->create_attr(bridge->audio_play, bridge->kobj);
		if (ret < 0) {
			ret = -EINVAL;
			goto err_attr6;
		}
	}
	return 0;
err_attr6:
	if (bridge->audio_cap->destroy_attr)
		bridge->audio_cap->destroy_attr(bridge->audio_cap, bridge->kobj);
err_attr5:
	sysfs_remove_file(bridge->kobj, &bridge->isolated_attr.attr);
err_attr4:
	sysfs_remove_file(bridge->kobj, &bridge->ringbuf_attr.attr);
err_attr3:
	sysfs_remove_file(bridge->kobj, &bridge->line_attr.attr);
err_attr2:
	sysfs_remove_file(bridge->kobj, &bridge->enable_attr.attr);
err_attr1:
	kobject_put(bridge->kobj);
err_attr0:
	return ret;
}

static void aml_bridge_destroy_attribute(struct audio_pcm_bridge_t *bridge)
{
	if (bridge->audio_play->destroy_attr)
		bridge->audio_play->destroy_attr(bridge->audio_play, bridge->kobj);
	if (bridge->audio_cap->destroy_attr)
		bridge->audio_cap->destroy_attr(bridge->audio_cap, bridge->kobj);
	sysfs_remove_file(bridge->kobj, &bridge->isolated_attr.attr);
	sysfs_remove_file(bridge->kobj, &bridge->ringbuf_attr.attr);
	sysfs_remove_file(bridge->kobj, &bridge->line_attr.attr);
	sysfs_remove_file(bridge->kobj, &bridge->enable_attr.attr);
	kobject_put(bridge->kobj);
}

static void bridge_control_work(struct work_struct *data)
{
	struct audio_pcm_bridge_t *bridge = container_of(data, struct audio_pcm_bridge_t,
					ctr_work);
	if (bridge->audio_cap && bridge->audio_cap->control)
		bridge->audio_cap->control(bridge->audio_cap,
			bridge->bridge_ctr_cmd, bridge->bridge_ctr_value);

	if (bridge->audio_play && bridge->audio_play->control)
		bridge->audio_play->control(bridge->audio_play,
			bridge->bridge_ctr_cmd, bridge->bridge_ctr_value);
}

static int aml_bridge_init(struct device *dev, struct device_node *node, int idx)
{
	int ret = 0;
	u32 value;
	struct audio_pcm_bridge_t *bridge;

	bridge = vzalloc(sizeof(*bridge));
	if (!bridge) {
		ret = -ENOMEM;
		goto err_init0;
	}
	bridge->id = idx;

	bridge->audio_cap = aml_bridge_parse_of(dev, node, "src");
	if (!bridge->audio_cap) {
		ret = -ENOMEM;
		goto err_init1;
	}

	bridge->audio_play = aml_bridge_parse_of(dev, node, "dst");
	if (!bridge->audio_play) {
		ret = -ENOMEM;
		goto err_init2;
	}
	if (!bridge->audio_cap->start || !bridge->audio_play->start) {
		ret = -EINVAL;
		goto err_init3;
	}

	bridge->rb = ring_buffer_init(bridge->audio_cap->ring_buffer_size);
	if (!bridge->rb) {
		ret = -ENOMEM;
		goto err_init3;
	}

	bridge->audio_cap->rb = bridge->rb;
	bridge->audio_play->rb = bridge->rb;

	bridge->audio_cap->opposite = bridge->audio_play;
	bridge->audio_play->opposite = bridge->audio_cap;

	bridge->audio_cap->audio_bridge = bridge;
	bridge->audio_play->audio_bridge = bridge;
	ret = aml_bridge_create_attribute(dev, bridge);
	if (ret < 0) {
		ret = -EINVAL;
		goto err_init4;
	}
	ret = of_property_read_u32(node, "isolated", &value);
	if (ret >= 0 && value)
		bridge->isolated_enable = true;

	ret = of_property_read_u32(node, "def-volume", &value);
	if (ret >= 0 && value >= VOLUME_MIN && value <= VOLUME_MAX)
		bridge->def_volume = value;
	else
		bridge->def_volume = VOLUME_MAX;

	if (bridge->audio_cap->control)
		bridge->audio_cap->control(bridge->audio_cap, PCM_VOLUME, bridge->def_volume);

	if (bridge->audio_play->control)
		bridge->audio_play->control(bridge->audio_play, PCM_VOLUME, bridge->def_volume);

	ret = of_property_read_u32(node, "auto-running", &value);
	if (ret >= 0 && value) {
		bridge->enable = true;
		ret = bridge->audio_cap->start(bridge->audio_cap);
		if (ret < 0) {
			ret = -EINVAL;
			goto err_init5;
		}
		ret = bridge->audio_play->start(bridge->audio_play);
		if (ret < 0) {
			ret = -EINVAL;
			goto err_init6;
		}
	}
	INIT_WORK(&bridge->ctr_work, bridge_control_work);
	list_add_tail(&bridge->pcm_list, &bridge_devs);
	return ret;
err_init6:
	bridge->audio_cap->stop(bridge->audio_cap);
err_init5:
	aml_bridge_destroy_attribute(bridge);
err_init4:
	ring_buffer_release(bridge->rb);
err_init3:
	audio_pcm_free(bridge->audio_play);
err_init2:
	audio_pcm_free(bridge->audio_cap);
err_init1:
	vfree(bridge);
err_init0:
	return ret;
}

static int aml_bridge_free(void)
{
	struct audio_pcm_bridge_t *cur, *n;

	list_for_each_entry_safe(cur, n, &bridge_devs, pcm_list) {
		cur->enable = false;
		list_del(&cur->pcm_list);
		if (cur->kobj)
			aml_bridge_destroy_attribute(cur);
		if (cur->audio_cap) {
			if (cur->audio_cap->enable_create_card &&
				cur->audio_cap->destroy_virtual_sound_card) {
				cur->audio_cap->destroy_virtual_sound_card(cur->audio_cap);
			}
			cur->audio_cap->stop(cur->audio_cap);
			audio_pcm_free(cur->audio_cap);
			cur->audio_cap = NULL;
		}
		if (cur->audio_play) {
			if (cur->audio_play->enable_create_card &&
				cur->audio_play->destroy_virtual_sound_card) {
				cur->audio_play->destroy_virtual_sound_card(cur->audio_play);
			}
			cur->audio_play->stop(cur->audio_play);
			audio_pcm_free(cur->audio_play);
			cur->audio_play = NULL;
		}
		if (cur->rb) {
			ring_buffer_release(cur->rb);
			cur->rb = NULL;
		}
		vfree(cur);
	}
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int audio_bridge_prepare(struct device *dev)
{
	return 0;
}

static void audio_bridge_complete(struct device *dev)
{
}

static int audio_bridge_suspend(struct device *dev)
{
	struct audio_pcm_bridge_t *cur, *n;

	list_for_each_entry_safe(cur, n, &bridge_devs, pcm_list) {
		if (cur->enable) {
			if (cur->audio_cap)
				cur->audio_cap->stop(cur->audio_cap);
			if (cur->audio_play)
				cur->audio_play->stop(cur->audio_play);
		}
	}

	return 0;
}

static int audio_bridge_resume(struct device *dev)
{
	struct audio_pcm_bridge_t *cur, *n;

	list_for_each_entry_safe(cur, n, &bridge_devs, pcm_list) {
		if (cur->enable) {
			if (cur->audio_cap)
				cur->audio_cap->start(cur->audio_cap);
			if (cur->audio_play)
				cur->audio_play->start(cur->audio_play);
		}
	}

	return 0;
}

static const struct dev_pm_ops audio_bridge_dev_pm_ops = {
	.prepare	= audio_bridge_prepare,
	.complete	= audio_bridge_complete,

	SET_SYSTEM_SLEEP_PM_OPS(audio_bridge_suspend, audio_bridge_resume)
};
#endif

static int audio_bridge_probe(struct platform_device *pdev)
{
	int ret;
	int i = 0;
	struct device_node *node = pdev->dev.of_node;
	struct device *dev = &pdev->dev;
	struct device_node *dai_link;

	dai_link = of_get_child_by_name(node, PREFIX "link");
	if (dai_link) {
		struct device_node *np = NULL;

		for_each_child_of_node(node, np) {
			dev_info(dev, "\tlink %d:\n", i);
			ret = aml_bridge_init(dev, np, i);
			if (ret < 0) {
				of_node_put(np);
				goto err_probe1;
			}
			i++;
		}
	}

	dev_info(dev, "%s init done\n", __func__);
	return 0;
err_probe1:
	aml_bridge_free();
	return ret;
}

static int audio_bridge_remove(struct platform_device *pdev)
{
	aml_bridge_free();
	return 0;
}

static const struct of_device_id audio_bridge_of_match[] = {
	{ .compatible = "amlogic, audio-bridge" },
	{},
};

static struct platform_driver audio_bridge_driver = {
	.probe = audio_bridge_probe,
	.remove = audio_bridge_remove,
	.driver = {
		.owner          = THIS_MODULE,
		.name = DRIVER_NAME,
		.of_match_table = audio_bridge_of_match,
#ifdef CONFIG_PM_SLEEP
		.pm = &audio_bridge_dev_pm_ops,
#endif
	},
};

static int __init audio_bridge_init(void)
{
	return platform_driver_register(&audio_bridge_driver);
}

static void __exit audio_bridge_exit(void)
{
	platform_driver_unregister(&audio_bridge_driver);
}

module_init(audio_bridge_init);
module_exit(audio_bridge_exit);

MODULE_AUTHOR("ziheng li <ziheng.li@amlogic.com>");
MODULE_LICENSE("GPL");
