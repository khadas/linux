// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#define pr_fmt(fmt) "amaudio: " fmt

#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/io.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/device.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/platform_device.h>
#include <linux/of_address.h>
#include <linux/uaccess.h>
#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif

/* Amlogic headers */
#include <linux/amlogic/major.h>
#include <linux/amlogic/iomap.h>

#define AMAUDIO_MODULE_NAME "amaudio"
#define AMAUDIO_DRIVER_NAME "amaudio"
#define AMAUDIO_DEVICE_NAME "amaudio-dev"
#define AMAUDIO_CLASS_NAME "amaudio"
#define AMAUDIO_DEVICE_COUNT    ARRAY_SIZE(amaudio_ports)

static int amaudio_open(struct inode *inode, struct file *file);
static int amaudio_release(struct inode *inode, struct file *file);
static int amaudio_utils_open(struct inode *inode, struct file *file);
static int amaudio_utils_release(struct inode *inode, struct file *file);

static u32 decoding_errors, decoded_frames;
static u32 samplerate, ch_num, ch_configuration;
static u32 bitrate, decoded_drop;
static const char audio_standards[32] = {"AAC,AML_DDP,DD,MPEG"};
static const char * const audio_format[] = {"PCM", "CUSTOM_1", "CUSTOM_2",
	"CUSTOM_3", "DD", "AUTO"};
static int hdmi_format;

struct amaudio_port_t {
	const char *name;
	struct device *dev;
	const struct file_operations *fops;
};

static struct class *amaudio_classp;
static struct device *amaudio_dev;

static const struct file_operations amaudio_ctl_fops = {
	.owner = THIS_MODULE,
	.open = amaudio_open,
	.release = amaudio_release,
};

static const struct file_operations amaudio_utils_fops = {
	.owner = THIS_MODULE,
	.open = amaudio_utils_open,
	.release = amaudio_utils_release,
};

static struct amaudio_port_t amaudio_ports[] = {
	{
	 .name = "amaudio_ctl",
	 .fops = &amaudio_ctl_fops,
	 },
	{
	 .name = "amaudio_utils",
	 .fops = &amaudio_utils_fops,
	 },
};

static const struct file_operations amaudio_fops = {
	.owner = THIS_MODULE,
	.open = amaudio_open,
	.release = amaudio_release,
};

static int amaudio_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int amaudio_release(struct inode *inode, struct file *file)
{
	return 0;
}

static int amaudio_utils_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int amaudio_utils_release(struct inode *inode, struct file *file)
{
	return 0;
}

enum {
	I2SIN_MASTER_MODE = 0,
	I2SIN_SLAVE_MODE = 1 << 0,
	SPDIFIN_MODE = 1 << 1,
};

static int dtsm6_stream_type = -1;
static unsigned int dtsm6_apre_cnt;
static unsigned int dtsm6_apre_sel;
static unsigned int dtsm6_apre_assets_sel;
static unsigned int dtsm6_mulasset_hint;
static char dtsm6_apres_assets_Array[32] = { 0 };

static void __iomem *base;
static unsigned int dtsm6_HPS_hint;
static ssize_t store_debug(struct class *class, struct class_attribute *attr,
			   const char *buf, size_t count)
{
	if (strncmp(buf, "dtsm6_stream_type_set", 21) == 0) {
		if (kstrtoint(buf + 21, 10, &dtsm6_stream_type))
			return -EINVAL;
	} else if (strncmp(buf, "dtsm6_apre_cnt_set", 18) == 0) {
		if (kstrtoint(buf + 18, 10, &dtsm6_apre_cnt))
			return -EINVAL;
	} else if (strncmp(buf, "dtsm6_apre_sel_set", 18) == 0) {
		if (kstrtoint(buf + 18, 10, &dtsm6_apre_sel))
			return -EINVAL;
	} else if (strncmp(buf, "dtsm6_apres_assets_set", 22) == 0) {
		if (dtsm6_apre_cnt > 32) {
			pr_info("[%s %d]invalid dtsm6_apre_cnt/%d\n", __func__,
				__LINE__, dtsm6_apre_cnt);
		} else {
			memcpy(dtsm6_apres_assets_Array, buf + 22,
			       dtsm6_apre_cnt);
		}
	} else if (strncmp(buf, "dtsm6_apre_assets_sel_set", 25) == 0) {
		if (kstrtoint(buf + 25, 10, &dtsm6_apre_assets_sel))
			return -EINVAL;
	} else if (strncmp(buf, "dtsm6_mulasset_hint", 19) == 0) {
		if (kstrtoint(buf + 19, 10, &dtsm6_mulasset_hint))
			return -EINVAL;
	} else if (strncmp(buf, "dtsm6_clear_info", 16) == 0) {
		dtsm6_stream_type = -1;
		dtsm6_apre_cnt = 0;
		dtsm6_apre_sel = 0;
		dtsm6_apre_assets_sel = 0;
		dtsm6_mulasset_hint = 0;
		dtsm6_HPS_hint = 0;
		memset(dtsm6_apres_assets_Array, 0,
		       sizeof(dtsm6_apres_assets_Array));
	} else if (strncmp(buf, "dtsm6_hps_hint", 14) == 0) {
		if (kstrtoint(buf + 14, 10, &dtsm6_HPS_hint))
			return -EINVAL;
	}
	return count;
}

static ssize_t show_debug(struct class *class, struct class_attribute *attr,
			  char *buf)
{
	int pos = 0;

	pos += sprintf(buf + pos, "dtsM6:StreamType%d\n", dtsm6_stream_type);
	pos += sprintf(buf + pos, "ApreCnt%d\n", dtsm6_apre_cnt);
	pos += sprintf(buf + pos, "ApreSel%d\n", dtsm6_apre_sel);
	pos += sprintf(buf + pos, "ApreAssetSel%d\n", dtsm6_apre_assets_sel);
	pos += sprintf(buf + pos, "MulAssetHint%d\n", dtsm6_mulasset_hint);
	pos += sprintf(buf + pos, "HPSHint%d\n", dtsm6_HPS_hint);
	pos += sprintf(buf + pos, "ApresAssetsArray");
	memcpy(buf + pos, dtsm6_apres_assets_Array,
	       sizeof(dtsm6_apres_assets_Array));
	pos += sizeof(dtsm6_apres_assets_Array);
	return pos;
}

static ssize_t dts_enable_show(struct class *class,
				struct class_attribute *attr, char *buf)
{
	unsigned int val;

	if (!base) {
		val = aml_read_aobus(0x228);
		val = (val >> 14) & 1;
	} else {
		val = readl(base + 0x48);
		val = (val >> 16) & 1;
		val = !val;
	}
	return sprintf(buf, "0x%x\n", val);
}

static ssize_t dolby_enable_show(struct class *class,
				struct class_attribute *attr, char *buf)
{
	unsigned int val;

	if (!base) {
		val = aml_read_aobus(0x228);
		val = (val >> 16) & 1;
	} else {
		val = readl(base + 0x48);
		val = (val >> 17) & 1;
		val = !val;
	}
	return sprintf(buf, "0x%x\n", val);
}

static ssize_t codec_report_info_store(struct class *class,
				       struct class_attribute *attr,
				       const char *buf, size_t count)
{
	if (strncmp(buf, "decoded_frames", 14) == 0) {
		if (kstrtoint(buf + 15, 10, (uint32_t *)&decoded_frames)) {
			pr_debug("codec_decoder_frames_store failed\n");
			return -EINVAL;
		}
	} else if (strncmp(buf, "decoded_err", 11) == 0) {
		if (kstrtoint(buf + 12, 10, (uint32_t *)&decoding_errors)) {
			pr_debug("codec_decoder_errors_store failed\n");
			return -EINVAL;
		}
	} else if (strncmp(buf, "decoded_drop", 12) == 0) {
		if (kstrtoint(buf + 13, 10, (uint32_t *)&decoded_drop))
			return -EINVAL;
	} else if (strncmp(buf, "hdmi_format", 11) == 0) {
		if (kstrtoint(buf + 12, 10, &hdmi_format))
			return -EINVAL;
	} else if (strncmp(buf, "samplerate", 10) == 0) {
		if (kstrtoint(buf + 11, 10, (uint32_t *)&samplerate))
			return -EINVAL;
	} else if (strncmp(buf, "ch_num", 6) == 0) {
		if (kstrtoint(buf + 7, 10, (uint32_t *)&ch_num))
			return -EINVAL;
	} else if (strncmp(buf, "ch_configuration", 16) == 0) {
		if (kstrtoint(buf + 17, 10, (uint32_t *)&ch_configuration))
			return -EINVAL;
	} else if (strncmp(buf, "bitrate", 7) == 0) {
		if (kstrtoint(buf + 8, 10, (uint32_t *)&bitrate))
			return -EINVAL;
	}
	return count;
}

static ssize_t codec_report_info_show(struct class *cla,
				      struct class_attribute *attr, char *buf)
{
	int pos = 0;

	pos += sprintf(buf + pos, "decoded_frames:%d ", decoded_frames);
	pos += sprintf(buf + pos, "decoded_err:%d ", decoding_errors);
	pos += sprintf(buf + pos, "decoded_drop:%d ", decoded_drop);
	pos += sprintf(buf + pos, "audio_standards:%s ", audio_standards);
	pos += sprintf(buf + pos, "audio_format:%s ",
		audio_format[hdmi_format]);
	pos += sprintf(buf + pos, "samplerate:%d ", samplerate);
	pos += sprintf(buf + pos, "ch_num:%d ", ch_num);
	pos += sprintf(buf + pos, "ch_configuration:%d ", ch_configuration);
	pos += sprintf(buf + pos, "bitrate:%d ", bitrate);

	return pos;
}

static struct class_attribute amaudio_attrs[] = {
	__ATTR(debug, 0664,
	       show_debug, store_debug),
	__ATTR(codec_report_info, 0664,
	       codec_report_info_show, codec_report_info_store),
	__ATTR_RO(dts_enable),
	__ATTR_RO(dolby_enable),
	__ATTR_NULL,
};

static struct attribute *amaudio_class_attrs[] = {
	&amaudio_attrs[0].attr,
	&amaudio_attrs[1].attr,
	&amaudio_attrs[2].attr,
	&amaudio_attrs[3].attr,
	NULL
};
ATTRIBUTE_GROUPS(amaudio_class);

static struct class amaudio_class = {
	.name = AMAUDIO_CLASS_NAME,
	.owner = THIS_MODULE,
	.class_groups = amaudio_class_groups,
	//.class_attrs = amaudio_attrs,
};

static int amaudio_probe(struct platform_device *pdev)
{
		struct resource res;

		if (of_address_to_resource(pdev->dev.of_node, 0, &res)) {
			pr_err("found resource failed\n");
			return -1;
		}

		if (res.start != 0) {
			base = ioremap_nocache(res.start, resource_size(&res));
			if (!base) {
				pr_err("cannot map otp_tee registers\n");
				return -ENOMEM;
			}
		}
		return 0;
}

static const struct of_device_id amlogic_amaudio_dt_match[] = {
	{	.compatible = "amlogic, amaudio",
	},
	{},
};

static struct platform_driver amlogic_amaudio_driver = {
	.probe = amaudio_probe,
	.driver = {
		.name = "amaudio",
		.of_match_table = amlogic_amaudio_dt_match,
	.owner = THIS_MODULE,
	},
};

static int __init amaudio_init(void)
{
	int ret = 0;
	int i = 0;
	struct amaudio_port_t *ap;

	pr_info("amaudio: driver %s init!\n", AMAUDIO_DRIVER_NAME);

	ret =
	    register_chrdev(AMAUDIO_MAJOR, AMAUDIO_DRIVER_NAME, &amaudio_fops);
	if (ret < 0) {
		pr_err("Can't register %s device ", AMAUDIO_DRIVER_NAME);
		ret = -ENODEV;
		goto err;
	}

	amaudio_classp = class_create(THIS_MODULE, AMAUDIO_DEVICE_NAME);
	if (IS_ERR(amaudio_classp)) {
		ret = PTR_ERR(amaudio_classp);
		goto err1;
	}

	ap = &amaudio_ports[0];
	for (i = 0; i < AMAUDIO_DEVICE_COUNT; ap++, i++) {
		ap->dev = device_create(amaudio_classp, NULL,
					MKDEV(AMAUDIO_MAJOR, i), NULL,
					amaudio_ports[i].name);
		if (IS_ERR(ap->dev)) {
			pr_err
			    ("amaudio: failed to create amaudio device node\n");
			goto err2;
		}
	}

	ret = class_register(&amaudio_class);
	if (ret) {
		pr_err("amaudio class create fail.\n");
		goto err3;
	}

	amaudio_dev =
	    device_create(&amaudio_class, NULL, MKDEV(AMAUDIO_MAJOR, 10), NULL,
			  AMAUDIO_CLASS_NAME);
	if (IS_ERR(amaudio_dev)) {
		pr_err("amaudio creat device fail.\n");
		goto err4;
	}

	pr_info("%s - amaudio: driver %s success!\n",
		__func__, AMAUDIO_DRIVER_NAME);

	return platform_driver_register(&amlogic_amaudio_driver);

 err4:
	device_destroy(&amaudio_class, MKDEV(AMAUDIO_MAJOR, 10));
 err3:
	class_unregister(&amaudio_class);
 err2:
	for (ap = &amaudio_ports[0], i = 0; i < AMAUDIO_DEVICE_COUNT; ap++, i++)
		device_destroy(amaudio_classp, MKDEV(AMAUDIO_MAJOR, i));
 err1:
	class_destroy(amaudio_classp);
 err:
	unregister_chrdev(AMAUDIO_MAJOR, AMAUDIO_DRIVER_NAME);
	return ret;
}

static void __exit amaudio_exit(void)
{
	int i = 0;
	struct amaudio_port_t *ap;

	device_destroy(&amaudio_class, MKDEV(AMAUDIO_MAJOR, 10));
	class_unregister(&amaudio_class);
	for (ap = &amaudio_ports[0], i = 0; i < AMAUDIO_DEVICE_COUNT; ap++, i++)
		device_destroy(amaudio_classp, MKDEV(AMAUDIO_MAJOR, i));
	class_destroy(amaudio_classp);
	unregister_chrdev(AMAUDIO_MAJOR, AMAUDIO_DRIVER_NAME);

	platform_driver_unregister(&amlogic_amaudio_driver);
}

module_init(amaudio_init);
module_exit(amaudio_exit);

MODULE_DESCRIPTION("AMLOGIC Audio Control Interface driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Amlogic Inc.");
MODULE_VERSION("1.0.0");
