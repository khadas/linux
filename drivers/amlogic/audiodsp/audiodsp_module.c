// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#define pr_fmt(fmt) "audio_dsp: " fmt

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
#include <asm/cacheflush.h>
#include <linux/amlogic/major.h>
#include <linux/slab.h>
#include <linux/reset.h>
#include <linux/uaccess.h>
#include <linux/amlogic/media/utils/amstream.h>
#include "audiodsp_module.h"
#include <linux/dma-mapping.h>
#include <linux/amlogic/media/frame_sync/ptsserv.h>
#include <linux/amlogic/media/frame_sync/timestamp.h>
#include <linux/amlogic/media/frame_sync/tsync.h>

unsigned int dsp_debug_flag = 1;

MODULE_DESCRIPTION("AMLOGIC APOLLO Audio dsp driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Zhou Zhi <zhi.zhou@amlogic.com>");
MODULE_VERSION("1.0.0");
/* static int IEC958_mode_raw_last; */
/* static int IEC958_mode_codec_last; */
static int decopt  = 1;
/* code for DD/DD+ DRC control  */
/* Dynamic range compression mode */
enum DDP_DEC_DRC_MODE {
	GBL_COMP_CUSTOM_0 = 0,	/* custom mode, analog  dialnorm */
	GBL_COMP_CUSTOM_1,	/* custom mode, digital dialnorm */
	GBL_COMP_LINE,		/* line out mode (default)       */
	GBL_COMP_RF		/* RF mode                       */
};

#define DRC_MODE_BIT  0
#define DRC_HIGH_CUT_BIT 3
#define DRC_LOW_BST_BIT 16
static unsigned int ac3_drc_control =
(GBL_COMP_LINE << DRC_MODE_BIT) |
(100 << DRC_HIGH_CUT_BIT) | (100 << DRC_LOW_BST_BIT);
/* code for DTS dial norm/downmix mode control */
enum DTS_DMX_MODE {
	DTS_DMX_LORO = 0,
	DTS_DMX_LTRT,
};

#define DTS_DMX_MODE_BIT  0
#define DTS_DIAL_NORM_BIT  1
#define DTS_DRC_SCALE_BIT  2
static unsigned int dts_dec_control =
(DTS_DMX_LORO << DTS_DMX_MODE_BIT) |
(1 << DTS_DIAL_NORM_BIT) | (0 << DTS_DRC_SCALE_BIT);

static struct audiodsp_priv *audiodsp_p;
#define  DSP_DRIVER_NAME	"audiodsp"
#define  DSP_NAME	"dsp"

#ifdef CONFIG_PM
struct audiodsp_pm_state_t {
	int event;
	/*  */
};

/* static struct audiodsp_pm_state_t pm_state; */

#endif
static ssize_t codec_fmt_show(struct class *cla, struct class_attribute *attr,
			      char *buf)
{
	size_t ret = 0;
	struct audiodsp_priv *priv = audiodsp_privdata();

	ret = sprintf(buf, "The codec Format %d\n", priv->stream_fmt);
	return ret;
}

static const struct file_operations audiodsp_fops = {
.owner = THIS_MODULE,
.open = NULL,
.read = NULL,
.write = NULL,
.release = NULL,
.unlocked_ioctl = NULL,
};

static ssize_t codec_fatal_err_show(struct class *cla,
				    struct class_attribute *attr, char *buf)
{
	struct audiodsp_priv *priv = audiodsp_privdata();

	return sprintf(buf, "%d\n", priv->decode_fatal_err);
}

static ssize_t codec_fatal_err_store(struct class *cla,
				     struct class_attribute *attr,
				     const char *buf, size_t count)
{
	struct audiodsp_priv *priv = audiodsp_privdata();

	if (buf[0] == '0')
		priv->decode_fatal_err = 0;
	else if (buf[0] == '1')
		priv->decode_fatal_err = 1;
	else if (buf[0] == '2')
		priv->decode_fatal_err = 2;

	pr_info("codec_fatal_err value:%d\n ", priv->decode_fatal_err);
	return count;
}

static ssize_t digital_raw_show(struct class *cla, struct class_attribute *attr,
				char *buf)
{
	char *pbuf = buf;

	pbuf += sprintf(pbuf, "%d\n", IEC958_mode_raw);
	return pbuf - buf;
}

static ssize_t digital_raw_store(struct class *class,
				 struct class_attribute *attr, const char *buf,
				 size_t count)
{
	pr_info("buf=%s\n", buf);
	if (buf[0] == '0')
		IEC958_mode_raw = 0;	/* PCM */
	else if (buf[0] == '1')
		IEC958_mode_raw = 1;	/* RAW without over clock */
	else if (buf[0] == '2')
		IEC958_mode_raw = 2;	/* RAW with over clock */

	pr_info("IEC958_mode_raw=%d\n", IEC958_mode_raw);
	return count;
}

#define SUPPORT_TYPE_NUM  10
static unsigned char *codec_str[SUPPORT_TYPE_NUM] = {
	"2 CH PCM", "DTS RAW Mode", "Dolby Digital", "DTS",
	"DD+", "DTS-HD", "8 CH PCM", "TrueHD", "DTS-HD MA",
	"HIGH_SR_Stereo_PCM"
};

static ssize_t digital_codec_show(struct class *cla,
				  struct class_attribute *attr, char *buf)
{
	char *pbuf = buf;

	pbuf += sprintf(pbuf, "%d\n", IEC958_mode_codec);
	return pbuf - buf;
}

static ssize_t digital_codec_store(struct class *class,
				   struct class_attribute *attr,
				   const char *buf, size_t count)
{
	unsigned int digital_codec = 0;
	unsigned int mode_codec = IEC958_mode_codec;

	if (buf) {
		if (kstrtoint(buf, 10, &digital_codec))
			pr_info("kstrtoint err %s\n", __func__);
		if (digital_codec < SUPPORT_TYPE_NUM) {
			IEC958_mode_codec = digital_codec;
			pr_info("IEC958_mode_codec= %d, IEC958 type %s\n",
				digital_codec, codec_str[digital_codec]);
		} else {
			pr_info("IEC958 type set exceed supported range\n");
		}
	}
	/*
	 * when raw output switch to pcm output,
	 * need trigger pcm hw prepare to re-init hw configuration
	 */
	pr_info("last mode %d,now %d\n", mode_codec, IEC958_mode_codec);

	return count;
}

static ssize_t print_flag_show(struct class *cla,
			       struct class_attribute *attr, char *buf)
{
	static const char * const dec_format[] = {
		"0 - disable arc dsp print",
		"1 - enable arc dsp print",
	};
	char *pbuf = buf;

	pbuf +=
	    sprintf(pbuf, "audiodsp decode option: %s\n",
		    dec_format[(decopt & 0x5) >> 2]);
	return pbuf - buf;
}

static ssize_t print_flag_store(struct class *class,
				struct class_attribute *attr, const char *buf,
				size_t count)
{
	unsigned int dec_opt = 0x1;

	pr_info("buf=%s\n", buf);
	if (buf[0] == '0')
		dec_opt = 0;	/* disable print flag */
	else if (buf[0] == '1')
		dec_opt = 1;	/* enable print flag */

	decopt = (decopt & (~4)) | (dec_opt << 2);
	pr_info("dec option=%d, decopt = %x\n", dec_opt, decopt);
	return count;
}

static ssize_t dec_option_show(struct class *cla, struct class_attribute *attr,
			       char *buf)
{
	static const char * const dec_format[] = {
		"0 - mute dts and ac3 ",
		"1 - mute dts.ac3 with noise ",
		"2 - mute ac3.dts with noise",
		"3 - both ac3 and dts with noise",
	};
	char *pbuf = buf;

	pbuf +=
	    sprintf(pbuf, "audiodsp decode option: %s\n",
		    dec_format[decopt & 0x3]);
	return pbuf - buf;
}

static ssize_t dec_option_store(struct class *class,
				struct class_attribute *attr, const char *buf,
				size_t count)
{
	unsigned int dec_opt = 0x3;

	pr_info("buf=%s\n", buf);
	if (buf[0] == '0') {
		dec_opt = 0;	/* mute ac3/dts */
	} else if (buf[0] == '1') {
		dec_opt = 1;	/* mute dts */
	} else if (buf[0] == '2') {
		dec_opt = 2;	/* mute ac3 */
	} else if (buf[0] == '3') {
		dec_opt = 3;	/* with noise */
	} else if (buf[0] == '4') {
		pr_info("digital mode :PCM-raw\n");
		decopt |= (1 << 5);
	} else if (buf[0] == '5') {
		pr_info("digital mode :raw\n");
		decopt &= ~(1 << 5);
	}
	decopt = (decopt & (~3)) | dec_opt;
	pr_info("dec option=%d\n", dec_opt);
	return count;
}

static ssize_t ac3_drc_control_show(struct class *cla,
				    struct class_attribute *attr, char *buf)
{
	static const char * const drcmode[] = {
		"CUSTOM_0", "CUSTOM_1", "LINE", "RF" };
	char *pbuf = buf;

	pr_info("\tdd+ drc mode : %s\n", drcmode[ac3_drc_control & 0x3]);
	pr_info("\tdd+ drc high cut scale : %d%%\n",
		(ac3_drc_control >> DRC_HIGH_CUT_BIT) & 0xff);
	pr_info("\tdd+ drc low boost scale : %d%%\n",
		(ac3_drc_control >> DRC_LOW_BST_BIT) & 0xff);
	pbuf += sprintf(pbuf, "%d\n", ac3_drc_control);
	return pbuf - buf;
}

static ssize_t ac3_drc_control_store(struct class *class,
				     struct class_attribute *attr,
				     const char *buf, size_t count)
{
	char tmpbuf[128];
	static const char * const drcmode[] = {
		"CUSTOM_0", "CUSTOM_1", "LINE", "RF" };
	int i = 0;
	unsigned int val;

	while ((buf[i]) && (buf[i] != ',') && (buf[i] != ' ')) {
		tmpbuf[i] = buf[i];
		i++;
	}
	tmpbuf[i] = 0;
	if (strncmp(tmpbuf, "drcmode", 7) == 0) {
		if (kstrtoint(buf + i + 1, 16, &val))
			pr_info("kstrtoint err %s\n", __func__);
		val = val & 0x3;
		pr_info("drc mode set to %s\n", drcmode[val]);
		ac3_drc_control = (ac3_drc_control & (~3)) | val;
	} else if (strncmp(tmpbuf, "drchighcutscale", 15) == 0) {
		if (kstrtoint(buf + i + 1, 16, &val))
			pr_info("kstrtoint err %s\n", __func__);
		val = val & 0xff;
		pr_info("drc high cut scale set to %d%%\n", val);
		ac3_drc_control =
		    (ac3_drc_control & (~(0xff << DRC_HIGH_CUT_BIT))) |
		    (val << DRC_HIGH_CUT_BIT);
	} else if (strncmp(tmpbuf, "drclowboostscale", 16) == 0) {
		if (kstrtoint(buf + i + 1, 16, &val))
			pr_info("kstrtoint err %s\n", __func__);
		val = val & 0xff;
		pr_info("drc low boost scale set to %d%%\n", val);
		ac3_drc_control =
		    (ac3_drc_control & (~(0xff << DRC_LOW_BST_BIT))) |
		    (val << DRC_LOW_BST_BIT);
	} else {
		pr_info("invalid args\n");
	}
	return count;
}

static ssize_t dts_dec_control_show(struct class *cla,
				    struct class_attribute *attr, char *buf)
{
	/* char *dmxmode[] = {"Lo/Ro","Lt/Rt"}; */
	/* char *dialnorm[] = {"disable","enable"}; */
	char *pbuf = buf;

	pbuf += sprintf(pbuf, "%d\n", dts_dec_control);
	return pbuf - buf;
}

static ssize_t dts_dec_control_store(struct class *class,
				     struct class_attribute *attr,
				     const char *buf, size_t count)
{
	char tmpbuf[128];
	static const char * const dmxmode[] = { "Lo/Ro", "Lt/Rt" };
	static const char * const dialnorm[] = { "disable", "enable" };
	int i = 0;
	unsigned int val;

	while ((buf[i]) && (buf[i] != ',') && (buf[i] != ' ')) {
		tmpbuf[i] = buf[i];
		i++;
	}
	tmpbuf[i] = 0;
	if (strncmp(tmpbuf, "dtsdmxmode", 10) == 0) {
		if (kstrtoint(buf + i + 1, 16, &val))
			pr_info("kstrtoint err %s\n", __func__);
		val = val & 0x1;
		pr_info("dts dmx mode set to %s\n", dmxmode[val]);
		dts_dec_control = (dts_dec_control & (~1)) | val;
	} else if (strncmp(tmpbuf, "dtsdrcscale", 11) == 0) {
		if (kstrtoint(buf + i + 1, 16, &val))
			pr_info("kstrtoint err %s\n", __func__);
		val = val & 0xff;
		pr_info("dts drc  scale set to %d\n", val);
		dts_dec_control =
		    (dts_dec_control & (~(0xff << DTS_DRC_SCALE_BIT))) |
		    (val << DTS_DRC_SCALE_BIT);
	} else if (strncmp(tmpbuf, "dtsdialnorm", 11) == 0) {
		if (kstrtoint(buf + i + 1, 16, &val))
			pr_info("kstrtoint err %s\n", __func__);
		val = val & 0x1;
		pr_info("dts  dial norm : set to %s\n", dialnorm[val]);
		dts_dec_control =
		    (dts_dec_control & (~(0x1 << DTS_DIAL_NORM_BIT))) |
		    (val << DTS_DIAL_NORM_BIT);
	} else {
		if (kstrtoint(tmpbuf, 16, &dts_dec_control))
			pr_info("kstrtoint err %s\n", __func__);
		pr_info("dts_dec_control/0x%x\n", dts_dec_control);
	}
	return count;
}

static ssize_t dsp_debug_show(struct class *cla, struct class_attribute *attr,
			      char *buf)
{
	char *pbuf = buf;

	pbuf +=
	    sprintf(pbuf, "DSP Debug Flag: %s\n",
		    (dsp_debug_flag == 0) ? "0 - OFF" : "1 - ON");
	return pbuf - buf;
}

static ssize_t dsp_debug_store(struct class *class,
			       struct class_attribute *attr, const char *buf,
			       size_t count)
{
	if (buf[0] == '0')
		dsp_debug_flag = 0;
	else if (buf[0] == '1')
		dsp_debug_flag = 1;

	pr_info("DSP Debug Flag: %d\n", dsp_debug_flag);
	return count;
}

static struct class_attribute audiodsp_attrs[] = {
	__ATTR_RO(codec_fmt),
	__ATTR(codec_fatal_err, 0664,
	       codec_fatal_err_show, codec_fatal_err_store),
	/* __ATTR_RO(swap_buf_ptr), */
	/* __ATTR_RO(dsp_working_status), */
	__ATTR(digital_raw, 0664, digital_raw_show,
	       digital_raw_store),
	__ATTR(digital_codec, 0664, digital_codec_show,
	       digital_codec_store),
	__ATTR(dec_option, 0644, dec_option_show,
	       dec_option_store),
	__ATTR(print_flag, 0644, print_flag_show,
	       print_flag_store),
	__ATTR(ac3_drc_control, 0664,
	       ac3_drc_control_show, ac3_drc_control_store),
	__ATTR(dsp_debug, 0644, dsp_debug_show, dsp_debug_store),
	__ATTR(dts_dec_control, 0644, dts_dec_control_show,
	       dts_dec_control_store),
	/* __ATTR(skip_rawbytes, 0644, skip_rawbytes_show, */
	/*       skip_rawbytes_store), */
	/* __ATTR_RO(pcm_left_len),   */
	__ATTR_NULL
};

static struct attribute *audiodsp_class_attrs[] = {
	&audiodsp_attrs[0].attr,
	&audiodsp_attrs[1].attr,
	&audiodsp_attrs[2].attr,
	&audiodsp_attrs[3].attr,
	&audiodsp_attrs[4].attr,
	&audiodsp_attrs[5].attr,
	&audiodsp_attrs[6].attr,
	&audiodsp_attrs[7].attr,
	&audiodsp_attrs[8].attr,
	NULL
};
ATTRIBUTE_GROUPS(audiodsp_class);

static struct class audiodsp_class = {
	.name = DSP_DRIVER_NAME,
	.owner = THIS_MODULE,
	.class_groups = audiodsp_class_groups,
//	.class_attrs = audiodsp_attrs,
};

int audiodsp_probe(void)
{
	int res = 0;
	struct audiodsp_priv *priv;

	dsp_debug_flag = 1;
	priv = kmalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		DSP_PRNT("Out of memory for audiodsp register\n");
		return -1;
	}
	memset(priv, 0, sizeof(struct audiodsp_priv));
	priv->dsp_is_started = 0;
	/*
	 *  priv->p = ioremap_nocache(AUDIO_DSP_START_PHY_ADDR, S_1M);
	 *  if(priv->p)
	 *  DSP_PRNT("DSP IOREMAP to addr 0x%x\n",(unsigned)priv->p);
	 *  else{
	 *  DSP_PRNT("DSP IOREMAP error\n");
	 *  goto error1;
	 *  }
	 */
	audiodsp_p = priv;
	if (priv->dsp_heap_size) {
		if (priv->dsp_heap_start == 0)
			priv->dsp_heap_start =
			    (unsigned long)kmalloc(priv->dsp_heap_size,
						   GFP_KERNEL);
		if (priv->dsp_heap_start == 0) {
			DSP_PRNT("no memory for audio dsp dsp_set_heap\n");
			kfree(priv);
			return -ENOMEM;
		}
	}
	res = register_chrdev(AUDIODSP_MAJOR, DSP_NAME, &audiodsp_fops);
	if (res < 0) {
		DSP_PRNT("Can't register  char device for " DSP_NAME "\n");
		goto error1;
	} else {
		DSP_PRNT("register " DSP_NAME " to char device(%d)\n",
			 AUDIODSP_MAJOR);
	}

	res = class_register(&audiodsp_class);
	if (res < 0) {
		DSP_PRNT("Create audiodsp class failed\n");
		res = -EEXIST;
		goto error2;
	}

	priv->class = &audiodsp_class;
	priv->dev = device_create(priv->class,
				  NULL, MKDEV(AUDIODSP_MAJOR, 0),
				  NULL, "audiodsp0");
	if (!priv->dev) {
		res = -EEXIST;
		goto error3;
	}
#ifdef CONFIG_AM_STREAMING
	/* set_adec_func(audiodsp_get_status); */
#endif
	return res;

	device_destroy(priv->class, MKDEV(AUDIODSP_MAJOR, 0));
 error3:
	class_destroy(priv->class);
 error2:
	unregister_chrdev(AUDIODSP_MAJOR, DSP_NAME);
 error1:
	kfree(priv);
	return res;
}

struct audiodsp_priv *audiodsp_privdata(void)
{
	return audiodsp_p;
}

static int __init audiodsp_init_module(void)
{
	return audiodsp_probe();
}

static void __exit audiodsp_exit_module(void)
{
	struct audiodsp_priv *priv;

	priv = audiodsp_privdata();
#ifdef CONFIG_AM_STREAMING
	set_adec_func(NULL);
#endif
	/*
	 *  iounmap(priv->p);
	 */
	device_destroy(priv->class, MKDEV(AUDIODSP_MAJOR, 0));
	class_destroy(priv->class);
	unregister_chrdev(AUDIODSP_MAJOR, DSP_NAME);
	kfree(priv);
	priv = NULL;
}

module_init(audiodsp_init_module);
module_exit(audiodsp_exit_module);
