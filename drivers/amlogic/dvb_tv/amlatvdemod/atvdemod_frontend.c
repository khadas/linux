/*
 * Silicon labs atvdemod Device Driver
 *
 * Author: dezhi.kong <dezhi.kong@amlogic.com>
 *
 *
 * Copyright (C) 2014 Amlogic Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/* Standard Liniux Headers */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/platform_device.h>

/*#include <linux/dvb/frontend.h>*/
/*#include <mach/gpio_data.h>*/
#include <mach/gpio.h>
/*#include <uapi/linux/dvb/frontend.h>*/

/* Amlogic Headers */

/* Local Headers */
/*#include "atvdemod_frontend.h"*/
#include "atvdemod_func.h"
#include "../aml_fe.h"
#include <uapi/linux/dvb/frontend.h>

#define ATVDEMOD_DEVICE_NAME                "amlatvdemod"

#define pr_dbg(a...) \
		do {\
			if (0)\
				printk(a);\
		} while (0)

struct amlatvdemod_device_s *amlatvdemod_devp;

static int aml_atvdemod_enter_mode(struct aml_fe *fe, int mode);
/*static void sound_store(const char *buff, v4l2_std_id *std);*/
static ssize_t aml_atvdemod_store(struct class *cls,
				  struct class_attribute *attr, const char *buf,
				  size_t count)
{
	int n = 0;
	unsigned int ret = 0;
	char *buf_orig, *ps, *token;
	char *parm[4];
	unsigned int data_snr[128];
	unsigned int data_snr_avg;
	int i;
	struct aml_fe *atvdemod_fe = NULL;
	buf_orig = kstrdup(buf, GFP_KERNEL);
	ps = buf_orig;
	while (1) {
		token = strsep(&ps, "\n");
		if (token == NULL)
			break;
		if (*token == '\0')
			continue;
		parm[n++] = token;
	}
	if (!strncmp(parm[0], "init", strlen("init"))) {
		ret = aml_atvdemod_enter_mode(atvdemod_fe, 0);
		if (ret)
			pr_info("[tuner..] atv_restart error.\n");
	} else if (!strcmp(parm[0], "tune")) {
		/* val  = simple_strtol(parm[1], NULL, 10);*/
	} else if (!strncmp(parm[0], "std", strlen("std"))) {
		/*
		   if(!strncmp(parm[1],"pal",3))
		   {
		   std= V4L2_COLOR_STD_PAL;
		   sound_store(parm[2],&std);
		   }
		   else if(!strncmp(parm[1],"ntsc",4))
		   {
		   std= V4L2_COLOR_STD_NTSC;
		   sound_store(parm[2],&std);
		   }
		   else if(!strncmp(parm[1],"secam",5))
		   {
		   std= V4L2_COLOR_STD_SECAM;
		   sound_store(parm[2],&std);
		   }
		   si2177_devp->parm.std  =std;
		   si2177_set_std();
		   siprintk("[si2177..]%s set std color %s,
			audio type %s.\n",__func__,
			v4l2_std_to_str(0xff000000&si2177_devp->parm.std),
			v4l2_std_to_str(0xffffff&si2177_devp->parm.std));
		 */
	} else if (!strncmp(parm[0], "snr_hist", strlen("snr_hist"))) {
		data_snr_avg = 0;
		for (i = 0; i < 128; i++) {
			data_snr[i] =
			    (atv_dmd_rd_long(APB_BLOCK_ADDR_VDAGC, 0x50) >> 8);
			mdelay(50);
			data_snr_avg += data_snr[i];
		}
		data_snr_avg = data_snr_avg / 128;
		pr_dbg("**********snr_hist_128avg:0x%x(%d)*********\n",
		       data_snr_avg, data_snr_avg);
	} else
		pr_dbg("invalid command\n");
	kfree(buf_orig);
	return count;
}

static ssize_t aml_atvdemod_show(struct class *cls,
				 struct class_attribute *attr, char *buff)
{
	return 0;
}

static CLASS_ATTR(atvdemod_debug, 0644, aml_atvdemod_show, aml_atvdemod_store);

void aml_atvdemod_set_frequency(unsigned int freq)
{
}

/*static void aml_atvdemod_set_std(void);*/

/*try audmode B,CH,I,DK,return the sound level*/
/*static unsigned char set_video_audio_mode(unsigned char color,
unsigned char audmode);*/
/*static void aml_atvdemod_get_status(struct dvb_frontend *fe,
void *stat);*/
/*static void aaaml_atvdemod_get_pll_status(struct dvb_frontend *fe,
void *stat);*/

static int aml_atvdemod_fe_init(struct aml_fe_dev *dev)
{

	int error_code = 0;
	if (!dev) {
		pr_dbg("[amlatvdemod..]%s: null pointer error.\n", __func__);
		return -1;
	}
	return error_code;
}

static int aml_atvdemod_enter_mode(struct aml_fe *fe, int mode)
{
	int err_code;
	err_code = atvdemod_init();
	if (err_code) {
		pr_dbg("[amlatvdemod..]%s init si2177 error.\n", __func__);
		return err_code;
	}
	return 0;
}

static int aml_atvdemod_leave_mode(struct aml_fe *fe, int mode)
{
	return 0;
}

static int aml_atvdemod_suspend(struct aml_fe_dev *dev)
{
	return 0;
}

static int aml_atvdemod_resume(struct aml_fe_dev *dev)
{
	return 0;
}

/*
static int aml_atvdemod_get_afc(struct dvb_frontend *fe,int *afc)
{
	return 0;
}*/
static int aml_atvdemod_get_snr(struct dvb_frontend *fe)
{
	unsigned int snr_val;
	int ret;
	snr_val = atv_dmd_rd_long(APB_BLOCK_ADDR_VDAGC, 0x50) >> 8;
	if (snr_val > 900000)
		ret = 5;
	else if (snr_val > 158000)
		ret = 15;
	else if (snr_val > 31600)
		ret = 30;
	else if (snr_val > 316)
		ret = 50;
	else
		ret = 80;
	return ret;
}

/*tuner lock status & demod lock status should be same in silicon tuner*/
static int aml_atvdemod_get_status(struct dvb_frontend *fe, void *stat)
{
	int video_lock;
	fe_status_t *status = (fe_status_t *) stat;
	retrieve_video_lock(&video_lock);
	if ((video_lock & 0x1) == 0) {
		/*  *status = FE_HAS_LOCK;*/
		*status = FE_TIMEDOUT;
		pr_info("video lock:locked\n");
	} else {
		pr_info("video lock:unlocked\n");
		*status = FE_TIMEDOUT;
		/*  *status = FE_HAS_LOCK;*/
	}
	return 0;
}

/*tuner lock status & demod lock status should be same in silicon tuner*/
/* force return lock, for atvdemo status not sure */
static void aml_atvdemod_get_pll_status(struct dvb_frontend *fe, void *stat)
{
	int vpll_lock;
	fe_status_t *status = (fe_status_t *) stat;
	retrieve_vpll_carrier_lock(&vpll_lock);
	if ((vpll_lock & 0x1) == 0) {
		/*  *status = FE_HAS_LOCK;*/
		*status = FE_TIMEDOUT;
		pr_info("visual carrier lock:locked\n");
	} else {
		pr_info("visual carrier lock:unlocked\n");
		*status = FE_TIMEDOUT;
		/*  *status = FE_HAS_LOCK;*/
	}
	return;
}

static int aml_atvdemod_get_atv_status(struct dvb_frontend *fe,
				       struct atv_status_s *atv_status)
{
	int afc = 0;
	fe_status_t tuner_state = FE_TIMEDOUT;

	if (fe && atv_status) {
		if (fe->ops.tuner_ops.get_afc) {
			fe->ops.tuner_ops.get_afc(fe, &afc);
			atv_status->afc = afc;
		}

		if (fe->ops.tuner_ops.get_status) {
			fe->ops.tuner_ops.get_status(fe, &tuner_state);
			if (tuner_state == FE_HAS_LOCK)
				atv_status->atv_lock = 1;
			else
				atv_status->atv_lock = 0;
		} else if (fe->ops.tuner_ops.get_pll_status) {
			fe->ops.tuner_ops.get_pll_status(fe, &tuner_state);
			if (tuner_state == FE_HAS_LOCK)
				atv_status->atv_lock = 1;
			else
				atv_status->atv_lock = 0;
		}
	}
	return 0;
}

void aml_atvdemod_set_params(struct dvb_frontend *fe,
			     struct analog_parameters *p)
{
	if (FE_ANALOG == fe->ops.info.type) {
		if (p->std != amlatvdemod_devp->parm.std) {
			amlatvdemod_devp->parm.std = p->std;
			atv_dmd_set_std();
			pr_info
			("[amlatvdemod..]%s set std color %s, audio type %s.\n",
			__func__,
			v4l2_std_to_str(0xff000000 & amlatvdemod_devp->
					     parm.std),
			v4l2_std_to_str(0xffffff & amlatvdemod_devp->parm.
					     std));
		}
	}
	return;
}

static int aml_atvdemod_get_ops(struct aml_fe_dev *dev, int mode, void *ops)
{

	struct analog_demod_ops *aml_analog_ops =
	    (struct analog_demod_ops *)ops;
	if (!ops) {
		pr_dbg("[amlatvdemod..]%s null pointer error.\n", __func__);
		return -1;
	}
	/*aml_analog_ops->get_afc = aml_atvdemod_get_afc;*/
	aml_analog_ops->get_snr = aml_atvdemod_get_snr;
	aml_analog_ops->get_status = aml_atvdemod_get_status;
	aml_analog_ops->set_params = aml_atvdemod_set_params;
	aml_analog_ops->get_pll_status = aml_atvdemod_get_pll_status;
	aml_analog_ops->get_atv_status = aml_atvdemod_get_atv_status;
	return 0;
}

static struct aml_fe_drv aml_atvdemod_drv = {
	.name = "aml_atv_demod",
	.capability = AM_FE_ANALOG,
	.id = AM_ATV_DEMOD_AML,
	.get_ops = aml_atvdemod_get_ops,
	.init = aml_atvdemod_fe_init,
	.enter_mode = aml_atvdemod_enter_mode,
	.leave_mode = aml_atvdemod_leave_mode,
	.suspend = aml_atvdemod_suspend,
	.resume = aml_atvdemod_resume,
};

struct class *aml_atvdemod_clsp;

static int __init aml_atvdemod_init(void)
{
	int ret = 0;
	amlatvdemod_devp =
	    kmalloc(sizeof(struct amlatvdemod_device_s), GFP_KERNEL);
	if (!amlatvdemod_devp) {
		pr_info
		("[atvdemod..] %s:allocate memory error,no enough memory.\n",
		__func__);
		return -ENOMEM;
	}
	memset(amlatvdemod_devp, 0, sizeof(struct amlatvdemod_device_s));
	amlatvdemod_devp->clsp =
	    class_create(THIS_MODULE, ATVDEMOD_DEVICE_NAME);
	if (!amlatvdemod_devp->clsp) {
		pr_info("[amlatvdemod..]%s:create class error.\n", __func__);
		return PTR_ERR(amlatvdemod_devp->clsp);
	}
	ret =
	    class_create_file(amlatvdemod_devp->clsp,
			      &class_attr_atvdemod_debug);
	if (ret)
		pr_err("[amlatvdemod]%s create atvdemod class file error.\n",
		       __func__);
	/*initialize the tuner common struct and register */
	aml_register_fe_drv(AM_DEV_ATV_DEMOD, &aml_atvdemod_drv);
	pr_dbg("[amlatvdemod..]%s.\n", __func__);
	return 0;
}

static void __exit aml_atvdemod_exit(void)
{
	class_destroy(amlatvdemod_devp->clsp);
	aml_unregister_fe_drv(AM_DEV_ATV_DEMOD, &aml_atvdemod_drv);
	pr_info("[amlatvdemod..]%s: driver removed ok.\n", __func__);
}

MODULE_AUTHOR("dezhi.kong <dezhi.kong@amlogic.com>");
MODULE_DESCRIPTION("aml atv demod device driver");
MODULE_LICENSE("GPL");

fs_initcall(aml_atvdemod_init);
module_exit(aml_atvdemod_exit);
