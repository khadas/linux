/*
 * drivers/amlogic/media/stream_input/amports/adec.c
 *
 * Copyright (C) 2016 Amlogic, Inc. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/uio_driver.h>
#include <linux/amlogic/media/utils/aformat.h>
#include <linux/amlogic/media/frame_sync/ptsserv.h>
#include <linux/amlogic/media/registers/register.h>
#include <linux/amlogic/media/codec_mm/configs.h>
#include "../parser/streambuf.h"
#include <linux/module.h>
#include <linux/of.h>
#include "amports_priv.h"
#include "../../common/chips/decoder_cpu_ver_info.h"
#define INFO_VALID ((astream_dev) && (astream_dev->format))

struct astream_device_s {
	char *name;
	char *format;
	s32 channum;
	s32 samplerate;
	s32 datawidth;
	int offset;

	struct device dev;
};

static char *astream_format[] = {
	"amadec_mpeg",
	"amadec_pcm_s16le",
	"amadec_aac",
	"amadec_ac3",
	"amadec_alaw",
	"amadec_mulaw",
	"amadec_dts",
	"amadec_pcm_s16be",
	"amadec_flac",
	"amadec_cook",
	"amadec_pcm_u8",
	"amadec_adpcm",
	"amadec_amr",
	"amadec_raac",
	"amadec_wma",
	"amadec_wmapro",
	"amadec_pcm_bluray",
	"amadec_alac",
	"amadec_vorbis",
	"amadec_aac_latm",
	"amadec_ape",
	"amadec_eac3",
	"amadec_pcm_widi",
	"amadec_dra",
	"amadec_sipr",
	"amadec_truehd",
	"amadec_mpeg1",
	"amadec_mpeg2",
	"amadec_wmavoi",
	"amadec_wmalossless",
	"amadec_pcm_s24le",
	"adec_max"
};

static const char *na_string = "NA";
static struct astream_device_s *astream_dev;

static ssize_t format_show(struct class *class, struct class_attribute *attr,
						   char *buf)
{
	if (INFO_VALID && astream_dev->format)
		return sprintf(buf, "%s\n", astream_dev->format);
	else
		return sprintf(buf, "%s\n", na_string);
}

static ssize_t channum_show(struct class *class, struct class_attribute *attr,
							char *buf)
{
	if (INFO_VALID)
		return sprintf(buf, "%u\n", astream_dev->channum);
	else
		return sprintf(buf, "%s\n", na_string);
}

static ssize_t samplerate_show(struct class *class,
				struct class_attribute *attr, char *buf)
{
	if (INFO_VALID)
		return sprintf(buf, "%u\n", astream_dev->samplerate);
	else
		return sprintf(buf, "%s\n", na_string);
}

static ssize_t datawidth_show(struct class *class,
				struct class_attribute *attr,
					char *buf)
{
	if (INFO_VALID)
		return sprintf(buf, "%u\n", astream_dev->datawidth);
	else
		return sprintf(buf, "%s\n", na_string);
}

static ssize_t pts_show(struct class *class, struct class_attribute *attr,
						char *buf)
{
	u32 pts;
	u32 pts_margin = 0;

	if (astream_dev->samplerate <= 12000)
		pts_margin = 512;

	if (INFO_VALID && (pts_lookup(PTS_TYPE_AUDIO, &pts, pts_margin) >= 0))
		return sprintf(buf, "0x%x\n", pts);
	else
		return sprintf(buf, "%s\n", na_string);
}

static ssize_t addr_offset_show(struct class *class,
				struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", astream_dev->offset);
}

static struct class_attribute astream_class_attrs[] = {
	__ATTR_RO(format),
	__ATTR_RO(samplerate),
	__ATTR_RO(channum),
	__ATTR_RO(datawidth),
	__ATTR_RO(pts),
	__ATTR_RO(addr_offset),
	__ATTR_NULL
};

static struct class astream_class = {
		.name = "astream",
		.class_attrs = astream_class_attrs,
	};

#if 1
#define IO_CBUS_PHY_BASE 0xc1100000ULL
#define IO_AOBUS_PHY_BASE 0xc8100000ULL
#define CBUS_REG_OFFSET(reg) ((reg) << 2)
#define IO_SECBUS_PHY_BASE 0xda000000ULL


#define IO_AOBUS_PHY_BASE_AFTER_G12A 0xff800000ULL

static struct uio_info astream_uio_info = {
	.name = "astream_uio",
	.version = "0.1",
	.irq = UIO_IRQ_NONE,

	.mem = {
		[0] = {
			.name = "AIFIFO",
			.memtype = UIO_MEM_PHYS,
			.addr =
			(IO_CBUS_PHY_BASE + CBUS_REG_OFFSET(AIU_AIFIFO_CTRL))
			&(PAGE_MASK),
			.size = PAGE_SIZE,
		},
		[1] = {
			.memtype = UIO_MEM_PHYS,
			.addr =
			(IO_CBUS_PHY_BASE + CBUS_REG_OFFSET(VCOP_CTRL_REG)),
			.size = PAGE_SIZE,
		},
/*
		[2] = {
			.name = "SECBUS",
			.memtype = UIO_MEM_PHYS,
			.addr = (IO_SECBUS_PHY_BASE),
			.size = PAGE_SIZE,
		},
*/
		[2] = {
			.name = "CBUS",
			.memtype = UIO_MEM_PHYS,
			.addr =
			(IO_CBUS_PHY_BASE + CBUS_REG_OFFSET(ASSIST_HW_REV))
			&(PAGE_MASK),
			.size = PAGE_SIZE,
		},
		[3] = {
			.name = "CBUS-START",
			.memtype = UIO_MEM_PHYS,
			.addr = (IO_CBUS_PHY_BASE + CBUS_REG_OFFSET(0x1000)),
			.size = PAGE_SIZE,
		},
		[4] = {
			.name = "AOBUS-START",
			.memtype = UIO_MEM_PHYS,
			.addr = (IO_AOBUS_PHY_BASE),
			.size = PAGE_SIZE,
		},
	},
};
#endif

static void astream_release(struct device *dev)
{
	kfree(astream_dev);

	astream_dev = NULL;
}

s32 adec_init(struct stream_port_s *port)
{
	enum aformat_e af;

	if (!astream_dev)
		return -ENODEV;

	af = port->aformat;

	astream_dev->channum = port->achanl;
	astream_dev->samplerate = port->asamprate;
	astream_dev->datawidth = port->adatawidth;

	/*wmb();don't need it...*/
	if (af < ARRAY_SIZE(astream_format))
		astream_dev->format = astream_format[af];
	else
		astream_dev->format = NULL;
	return 0;
}
EXPORT_SYMBOL(adec_init);

s32 adec_release(enum aformat_e vf)
{
	pr_info("adec_release\n");

	if (!astream_dev)
		return -ENODEV;

	astream_dev->format = NULL;

	return 0;
}
EXPORT_SYMBOL(adec_release);

int amstream_adec_show_fun(const char *trigger, int id, char *sbuf, int size)
{
	int ret = -1;
	void *buf, *getbuf = NULL;
	if (size < PAGE_SIZE) {
		getbuf = (void *)__get_free_page(GFP_KERNEL);
		if (!getbuf)
			return -ENOMEM;
		buf = getbuf;
	} else {
		buf = sbuf;
	}
	switch (trigger[0]) {
	case 'f':
		ret =  format_show(NULL, NULL, buf);
		break;
	case 's':
		ret =  samplerate_show(NULL, NULL, buf);
		break;
	case 'c':
		ret =  channum_show(NULL, NULL, buf);
		break;
	case 'd':
		ret =  datawidth_show(NULL, NULL, buf);
		break;
	case 'p':
		ret =  pts_show(NULL, NULL, buf);
		break;
	default:
		ret = -1;
	}
	if (ret > 0 && getbuf != NULL) {
		ret = min_t(int, ret, size);
		strncpy(sbuf, buf, ret);
	}
	if (getbuf != NULL)
		free_page((unsigned long)getbuf);
	return ret;
}

static struct mconfig adec_configs[] = {
	MC_FUN("format", &amstream_adec_show_fun, NULL),
	MC_FUN("samplerate", &amstream_adec_show_fun, NULL),
	MC_FUN("channum", &amstream_adec_show_fun, NULL),
	MC_FUN("datawidth", &amstream_adec_show_fun, NULL),
	MC_FUN("pts", &amstream_adec_show_fun, NULL),
};
static struct mconfig_node adec_node;


s32 astream_dev_register(void)
{
	s32 r;
	struct device_node *node;
	unsigned int cbus_base = 0xffd00000;

	r = class_register(&astream_class);
	if (r) {
		pr_info("astream class create fail.\n");
		return r;
	}

	astream_dev = kzalloc(sizeof(struct astream_device_s), GFP_KERNEL);

	if (!astream_dev) {
		pr_info("astream device create fail.\n");
		r = -ENOMEM;
		goto err_3;
	}

	astream_dev->dev.class = &astream_class;
	astream_dev->dev.release = astream_release;
	astream_dev->offset = 0;
	dev_set_name(&astream_dev->dev, "astream-dev");

	dev_set_drvdata(&astream_dev->dev, astream_dev);

	r = device_register(&astream_dev->dev);
	if (r) {
		pr_info("astream device register fail.\n");
		goto err_2;
	}

	if (AM_MESON_CPU_MAJOR_ID_TXL < get_cpu_major_id()) {
		node = of_find_node_by_path("/codec_io/io_cbus_base");
		if (!node) {
			pr_info("No io_cbus_base node found.");
			goto err_1;
		}

#ifdef CONFIG_ARM64_A32
		r = of_property_read_u32_index(node, "reg", 0, &cbus_base);
#else
		r = of_property_read_u32_index(node, "reg", 1, &cbus_base);
#endif
		if (r) {
			pr_info("No find node.\n");
			goto err_1;
		}

		/*need to offset -0x100 in txlx.*/
		astream_dev->offset = -0x100;

		/*need to offset -0x180 in g12a.*/
		if (AM_MESON_CPU_MAJOR_ID_G12A <= get_cpu_major_id()) {
			astream_dev->offset = -0x180;
			/* after G12A chip, the aobus base addr changed */
			astream_uio_info.mem[4].addr = IO_AOBUS_PHY_BASE_AFTER_G12A;
		}
		astream_uio_info.mem[0].addr =
			(cbus_base + CBUS_REG_OFFSET(AIU_AIFIFO_CTRL +
			astream_dev->offset)) & (PAGE_MASK);

		astream_uio_info.mem[3].addr =
			(cbus_base + CBUS_REG_OFFSET(ASSIST_HW_REV +
			0x100)) & (PAGE_MASK);
	}

#if 1
	if (uio_register_device(&astream_dev->dev, &astream_uio_info)) {
		pr_info("astream UIO device register fail.\n");
		r = -ENODEV;
		goto err_1;
	}
#endif
	INIT_REG_NODE_CONFIGS("media", &adec_node,
		"adec", adec_configs, CONFIG_FOR_R);
	return 0;

err_1:
	device_unregister(&astream_dev->dev);

err_2:
	kfree(astream_dev);
	astream_dev = NULL;

err_3:
	class_unregister(&astream_class);

	return r;
}

void astream_dev_unregister(void)
{
	if (astream_dev) {
#if 1
		uio_unregister_device(&astream_uio_info);
#endif

		device_unregister(&astream_dev->dev);

		class_unregister(&astream_class);
	}
}
