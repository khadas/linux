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
#include <linux/io.h>

/* Amlogic Headers */

/* Local Headers */
#include "atvdemod_func.h"
#include "../aml_fe.h"
#include <uapi/linux/dvb/frontend.h>
#include <linux/amlogic/tvin/tvin.h>

#define ATVDEMOD_DEVICE_NAME                "amlatvdemod"
#define ATVDEMOD_DRIVER_NAME	"amlatvdemod"
#define ATVDEMOD_MODULE_NAME	"amlatvdemod"
#define ATVDEMOD_CLASS_NAME	"amlatvdemod"

struct amlatvdemod_device_s *amlatvdemod_devp;
#define AMLATVDEMOD_VER "Ref.2015/09/01a"

static int afc_wave_cnt;
static int last_frq, last_std;

unsigned int reg_23cf = 0x88188832; /*IIR filter*/
module_param(reg_23cf, uint, 0664);
MODULE_PARM_DESC(reg_23cf, "\n reg_23cf\n");

unsigned int atvdemod_scan_mode = 0; /*IIR filter*/
module_param(atvdemod_scan_mode, uint, 0664);
MODULE_PARM_DESC(atvdemod_scan_mode, "\n atvdemod_scan_mode\n");

/* used for resume */
#define ATVDEMOD_STATE_IDEL 0
#define ATVDEMOD_STATE_WORK 1
#define ATVDEMOD_STATE_SLEEP 2
static int atvdemod_state = ATVDEMOD_STATE_IDEL;

int is_atvdemod_scan_mode(void)
{
	return atvdemod_scan_mode;
}
EXPORT_SYMBOL(is_atvdemod_scan_mode);

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
	int data_afc, block_addr, block_reg, block_val = 0;
	int i, val = 0;
	unsigned long tmp = 0;
	struct aml_fe *atvdemod_fe = NULL;
	buf_orig = kstrdup(buf, GFP_KERNEL);
	ps = buf_orig;
	block_addr = 0;
	block_reg = 0;
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
		/* val  = simple_strtol(parm[1], NULL, 10); */
	} else if (!strcmp(parm[0], "set")) {
		if (!strncmp(parm[1], "avout_gain", strlen("avout_gain"))) {
			if (kstrtoul(buf+strlen("avout_offset")+1, 10,
				&tmp) == 0)
				val  = tmp;
			atv_dmd_wr_byte(0x0c, 0x01, val&0xff);
		} else if (!strncmp(parm[1], "avout_offset",
			strlen("avout_offset"))) {
			if (kstrtoul(buf+strlen("avout_offset")+1, 10,
				&tmp) == 0)
				val  = tmp;
			atv_dmd_wr_byte(0x0c, 0x04, val&0xff);
		} else if (!strncmp(parm[1], "atv_gain", strlen("atv_gain"))) {
			if (kstrtoul(buf+strlen("atv_gain")+1, 10, &tmp) == 0)
				val  = tmp;
			atv_dmd_wr_byte(0x19, 0x01, val&0xff);
		} else if (!strncmp(parm[1], "atv_offset",
			strlen("atv_offset"))) {
			if (kstrtoul(buf+strlen("atv_offset")+1, 10,
				&tmp) == 0)
				val  = tmp;
			atv_dmd_wr_byte(0x19, 0x04, val&0xff);
		}
	} else if (!strcmp(parm[0], "get")) {
		if (!strncmp(parm[1], "avout_gain", strlen("avout_gain"))) {
			val = atv_dmd_rd_byte(0x0c, 0x01);
			pr_dbg("avout_gain:0x%x\n", val);
		} else if (!strncmp(parm[1], "avout_offset",
			strlen("avout_offset"))) {
			val = atv_dmd_rd_byte(0x0c, 0x04);
			pr_dbg("avout_offset:0x%x\n", val);
		} else if (!strncmp(parm[1], "atv_gain", strlen("atv_gain"))) {
			val = atv_dmd_rd_byte(0x19, 0x01);
			pr_dbg("atv_gain:0x%x\n", val);
		} else if (!strncmp(parm[1], "atv_offset",
			strlen("atv_offset"))) {
			val = atv_dmd_rd_byte(0x19, 0x04);
			pr_dbg("atv_offset:0x%x\n", val);
		}
	} else if (!strncmp(parm[0], "snr_hist", strlen("snr_hist"))) {
		data_snr_avg = 0;
		for (i = 0; i < 128; i++) {
			data_snr[i] =
			    (atv_dmd_rd_long(APB_BLOCK_ADDR_VDAGC, 0x50) >> 8);
			usleep_range(50*1000, 50*1000+100);
			data_snr_avg += data_snr[i];
		}
		data_snr_avg = data_snr_avg / 128;
		pr_dbg("**********snr_hist_128avg:0x%x(%d)*********\n",
		       data_snr_avg, data_snr_avg);
	} else if (!strncmp(parm[0], "afc_info", strlen("afc_info"))) {
		data_afc = retrieve_vpll_carrier_afc();
		pr_dbg("[amlatvdemod..]afc %d Khz.\n", data_afc);
	} else if (!strncmp(parm[0], "ver_info", strlen("ver_info"))) {
		pr_dbg("[amlatvdemod..]aml_atvdemod_ver %s.\n",
			AMLATVDEMOD_VER);
	} else if (!strncmp(parm[0], "audio_autodet",
		strlen("audio_autodet"))) {
		aml_audiomode_autodet(NULL);
	} else if (!strncmp(parm[0], "overmodule_det",
			strlen("overmodule_det"))) {
		/* unsigned long over_threshold, */
		/* int det_mode = auto_det_mode; */
		aml_atvdemod_overmodule_det();
	} else if (!strncmp(parm[0], "audio_gain_set",
			strlen("audio_gain_set"))) {
		if (kstrtoul(buf+strlen("audio_gain_set")+1, 16, &tmp) == 0)
			val = tmp;
		aml_audio_valume_gain_set(val);
		pr_dbg("audio_gain_set : %d\n", val);
	} else if (!strncmp(parm[0], "audio_gain_get",
			strlen("audio_gain_get"))) {
		val = aml_audio_valume_gain_get();
		pr_dbg("audio_gain_get : %d\n", val);
	} else if (!strncmp(parm[0], "fix_pwm_adj", strlen("fix_pwm_adj"))) {
		if (kstrtoul(parm[1], 10, &tmp) == 0) {
			val = tmp;
			aml_fix_PWM_adjust(val);
		}
	} else if (!strncmp(parm[0], "rs", strlen("rs"))) {
		if (kstrtoul(parm[1], 16, &tmp) == 0)
				block_addr  = tmp;
		if (kstrtoul(parm[2], 16, &tmp) == 0)
				block_reg  = tmp;
		if (block_addr < APB_BLOCK_ADDR_TOP)
			block_val = atv_dmd_rd_long(block_addr, block_reg);
		pr_info("rs block_addr:0x%x,block_reg:0x%x,block_val:0x%x\n",
			block_addr, block_reg, block_val);
	} else if (!strncmp(parm[0], "ws", strlen("ws"))) {
		if (kstrtoul(parm[1], 16, &tmp) == 0)
			block_addr  = tmp;
		if (kstrtoul(parm[2], 16, &tmp) == 0)
			block_reg  = tmp;
		if (kstrtoul(parm[3], 16, &tmp) == 0)
			block_val  = tmp;
		if (block_addr < APB_BLOCK_ADDR_TOP)
			atv_dmd_wr_long(block_addr, block_reg, block_val);
		pr_info("ws block_addr:0x%x,block_reg:0x%x,block_val:0x%x\n",
			block_addr, block_reg, block_val);
		block_val = atv_dmd_rd_long(block_addr, block_reg);
		pr_info("readback_val:0x%x\n", block_val);
	} else if (!strncmp(parm[0], "pin_mux", strlen("pin_mux"))) {
		amlatvdemod_devp->pin =
			devm_pinctrl_get_select(amlatvdemod_devp->dev,
				amlatvdemod_devp->pin_name);
		pr_dbg("atvdemod agc pinmux name:%s\n",
				amlatvdemod_devp->pin_name);
	} else if (!strncmp(parm[0], "snr_cur", strlen("snr_cur"))) {
		data_snr_avg = aml_atvdemod_get_snr_ex();
		pr_dbg("**********snr_cur:%d*********\n", data_snr_avg);
	} else
		pr_dbg("invalid command\n");
	kfree(buf_orig);
	return count;
}

static ssize_t aml_atvdemod_show(struct class *cls,
	struct class_attribute *attr, char *buff)
{
	pr_dbg("\n usage:\n");
	pr_dbg("[get soft version] echo ver_info > /sys/class/amlatvdemod/atvdemod_debug\n");
	pr_dbg("[get afc value] echo afc_info > /sys/class/amlatvdemod/atvdemod_debug\n");
	pr_dbg("[reinit atvdemod] echo init > /sys/class/amlatvdemod/atvdemod_debug\n");
	pr_dbg("[get av-out-gain/av-out-offset/atv-gain/atv-offset]:\n"
		"echo get av_gain/av_offset/atv_gain/atv_offset > /sys/class/amlatvdemod/atvdemod_debug\n");
	pr_dbg("[set av-out-gain/av-out-offset/atv-gain/atv-offset]:\n"
		"echo set av_gain/av_offset/atv_gain/atv_offset val(0~255) > /sys/class/amlatvdemod/atvdemod_debug\n");
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
	if (amlatvdemod_devp->pin_name != NULL)
		amlatvdemod_devp->pin =
			devm_pinctrl_get_select(amlatvdemod_devp->dev,
				amlatvdemod_devp->pin_name);
	/* printk("\n%s: set atvdemod pll...\n",__func__); */
	adc_set_pll_cntl(1, 0x1);
	atvdemod_clk_init();
	err_code = atvdemod_init();
	if (err_code) {
		pr_dbg("[amlatvdemod..]%s init atvdemod error.\n", __func__);
		return err_code;
	}

	set_aft_thread_enable(1);
	atvdemod_state = ATVDEMOD_STATE_WORK;
	return 0;
}

static int aml_atvdemod_leave_mode(struct aml_fe *fe, int mode)
{
	set_aft_thread_enable(0);
	atvdemod_uninit();
	if (amlatvdemod_devp->pin != NULL) {
		devm_pinctrl_put(amlatvdemod_devp->pin);
		amlatvdemod_devp->pin = NULL;
	}
	/* reset adc pll flag */
	/* printk("\n%s: init atvdemod flag...\n",__func__); */
	adc_set_pll_cntl(0, 0x1);
	atvdemod_state = ATVDEMOD_STATE_IDEL;
	return 0;
}

static int aml_atvdemod_suspend(struct aml_fe_dev *dev)
{
	pr_info("%s\n", __func__);
	if (ATVDEMOD_STATE_IDEL != atvdemod_state) {
		aml_atvdemod_leave_mode(NULL, 0);
		atvdemod_state = ATVDEMOD_STATE_SLEEP;
	}
	return 0;
}

static int aml_atvdemod_resume(struct aml_fe_dev *dev)
{
	pr_info("%s\n", __func__);
	if (atvdemod_state == ATVDEMOD_STATE_SLEEP)
		aml_atvdemod_enter_mode(NULL, 0);
	return 0;
}

/*
static int aml_atvdemod_get_afc(struct dvb_frontend *fe,int *afc)
{
	return 0;
}*/

/*ret:5~100;the val is bigger,the signal is better*/
int aml_atvdemod_get_snr(struct dvb_frontend *fe)
{
#if 1
	return get_atvdemod_snr_val();
#else
	unsigned int snr_val;
	int ret;
	snr_val = atv_dmd_rd_long(APB_BLOCK_ADDR_VDAGC, 0x50) >> 8;
	/* snr_val:900000~0xffffff,ret:5~15 */
	if (snr_val > 900000)
		ret = 15 - (snr_val - 900000)*10/(0xffffff - 900000);
	/* snr_val:158000~900000,ret:15~30 */
	else if (snr_val > 158000)
		ret = 30 - (snr_val - 158000)*15/(900000 - 158000);
	/* snr_val:31600~158000,ret:30~50 */
	else if (snr_val > 31600)
		ret = 50 - (snr_val - 31600)*20/(158000 - 31600);
	/* snr_val:316~31600,ret:50~80 */
	else if (snr_val > 316)
		ret = 80 - (snr_val - 316)*30/(31600 - 316);
	/* snr_val:0~316,ret:80~100 */
	else
		ret = 100 - (316 - snr_val)*20/316;
	return ret;
#endif
}
EXPORT_SYMBOL(aml_atvdemod_get_snr);

int aml_atvdemod_get_snr_ex(void)
{
#if 1
	return get_atvdemod_snr_val();
#else
	unsigned int snr_val;
	int ret;
	snr_val = atv_dmd_rd_long(APB_BLOCK_ADDR_VDAGC, 0x50) >> 8;
	/* snr_val:900000~0xffffff,ret:5~15 */
	if (snr_val > 900000)
		ret = 15 - (snr_val - 900000)*10/(0xffffff - 900000);
	/* snr_val:158000~900000,ret:15~30 */
	else if (snr_val > 158000)
		ret = 30 - (snr_val - 158000)*15/(900000 - 158000);
	/* snr_val:31600~158000,ret:30~50 */
	else if (snr_val > 31600)
		ret = 50 - (snr_val - 31600)*20/(158000 - 31600);
	/* snr_val:316~31600,ret:50~80 */
	else if (snr_val > 316)
		ret = 80 - (snr_val - 316)*30/(31600 - 316);
	/* snr_val:0~316,ret:80~100 */
	else
		ret = 100 - (316 - snr_val)*20/316;
	return ret;
#endif
}
EXPORT_SYMBOL(aml_atvdemod_get_snr_ex);

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
	if ((vpll_lock&0x1) == 0) {
		*status = FE_HAS_LOCK;
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
	int vpll_lock = 0, line_lock = 0;
	int try_std = 1;
	int loop_cnt = 5;
	int cnt = 10;
	int try_std_cnt = 0;
	static int last_report_freq;
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;

	while (fe && atv_status && loop_cnt--) {
		atv_status->afc = retrieve_vpll_carrier_afc();
		retrieve_vpll_carrier_lock(&vpll_lock);
		line_lock = atv_dmd_rd_byte(APB_BLOCK_ADDR_VDAGC, 0x4f)&0x10;
		if ((vpll_lock&0x1) == 0 || line_lock == 0) {
			atv_status->atv_lock = 1;
			try_std_cnt = 2;
			while (try_std_cnt--) {
				atv_status->afc = retrieve_vpll_carrier_afc();
				if (atv_status->afc > 1500
					&& atvdemod_scan_mode) {
					if ((c->analog.std & 0xff000000)
						!= V4L2_COLOR_STD_PAL) {
						c->analog.std =
							V4L2_COLOR_STD_PAL
							| V4L2_STD_PAL_BG;
						c->frequency += 1;
						fe->ops.set_frontend(fe);
						msleep(20);
					} else {
						c->analog.std =
							V4L2_COLOR_STD_NTSC
							| V4L2_STD_NTSC_M;
						c->frequency += 1;
						fe->ops.set_frontend(fe);
						usleep_range(20*1000,
							20*1000+100);
					}
					atv_status->afc =
						retrieve_vpll_carrier_afc();

					cnt = 4;
				while (cnt--) {
					if (atv_status->afc < 1500)
						break;
					atv_status->afc =
						retrieve_vpll_carrier_afc();
					usleep_range(5*1000, 5*1000+100);
				}
					if (atv_status->afc < 1500)
						break;
				}
			}

			if (atv_status->afc > 4000 && !atvdemod_scan_mode)
				atv_status->atv_lock = 0;

			if (last_report_freq != c->frequency)
				last_report_freq = c->frequency;

			if (atvdemod_scan_mode)
				pr_err("%s,lock freq:%d, afc:%d\n", __func__,
					c->frequency, atv_status->afc);
			break;

		} else if (try_std%3 == 0 && atvdemod_scan_mode) {
			if ((c->analog.std & 0xff000000)
				!= V4L2_COLOR_STD_PAL) {
				c->analog.std =
					V4L2_COLOR_STD_PAL | V4L2_STD_PAL_DK;
			}
			if (1000000 < abs(c->frequency - last_report_freq)) {
				c->frequency -= 500000;
				pr_err("@@@ %s freq:%d unlock,try back 0.5M\n",
					__func__, c->frequency);
			} else
				c->frequency += 1;
			fe->ops.set_frontend(fe);
			usleep_range(10*1000, 10*1000+100);
		}
		if (atvdemod_scan_mode)
			pr_err("@@@ %s freq:%d unlock, read lock again\n",
				__func__, c->frequency);
		if (atvdemod_scan_mode == 0)
			usleep_range(10*1000, 10*1000+100);
		else
			usleep_range(1000, 1200);

		atv_status->atv_lock = 0;
		try_std++;
	}
	if (0 == atvdemod_scan_mode) {
		if (20 > abs(atv_status->afc))
			afc_wave_cnt = 0;
		if (500*1000 > abs(last_frq - c->frequency)
				&& 20 < abs(atv_status->afc)
				&& 200 > abs(atv_status->afc)) {
			afc_wave_cnt++;
			pr_err("%s play mode,afc_wave_cnt:%d\n",
				__func__, afc_wave_cnt);
			if (afc_wave_cnt < 20) {
				atv_status->afc = 0;
				pr_err("%s, afc is wave,ignore\n", __func__);
			}
		}
	}
	return 0;
}

void aml_atvdemod_set_params(struct dvb_frontend *fe,
				struct analog_parameters *p)
{
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;
	if (FE_ANALOG == fe->ops.info.type) {
		if ((p->std != amlatvdemod_devp->parm.std) ||
			(p->tuner_id == AM_TUNER_R840) ||
			(p->tuner_id == AM_TUNER_SI2151) ||
			(p->tuner_id == AM_TUNER_MXL661)) {
			amlatvdemod_devp->parm.std  = p->std;
			amlatvdemod_devp->parm.if_freq = p->if_freq;
			amlatvdemod_devp->parm.if_inv = p->if_inv;
			amlatvdemod_devp->parm.tuner_id = p->tuner_id;
			/* open AGC if needed */
			if (amlatvdemod_devp->pin != NULL)
				devm_pinctrl_put(amlatvdemod_devp->pin);
			if (amlatvdemod_devp->pin_name)
				amlatvdemod_devp->pin =
				devm_pinctrl_get_select(amlatvdemod_devp->dev,
						amlatvdemod_devp->pin_name);
			atv_dmd_set_std();
			last_frq = c->frequency;
			last_std = c->analog.std;
			pr_info("[amlatvdemod..]%s set std color %s, audio type %s.\n",
				__func__,
			v4l2_std_to_str(0xff000000&amlatvdemod_devp->parm.std),
			v4l2_std_to_str(0xffffff&amlatvdemod_devp->parm.std));
			pr_info("[amlatvdemod..]%s set if_freq 0x%x, if_inv 0x%x.\n",
				__func__, amlatvdemod_devp->parm.if_freq,
				amlatvdemod_devp->parm.if_inv);
		}
	}
	return;
}
static int aml_atvdemod_get_afc(struct dvb_frontend *fe, s32 *afc)
{
	*afc = retrieve_vpll_carrier_afc();
	pr_info("[amlatvdemod..]%s afc %d.\n", __func__, *afc);
	return 0;
}

static int aml_atvdemod_get_ops(struct aml_fe_dev *dev, int mode, void *ops)
{
	struct analog_demod_ops *aml_analog_ops =
	    (struct analog_demod_ops *)ops;
	if (!ops) {
		pr_dbg("[amlatvdemod..]%s null pointer error.\n", __func__);
		return -1;
	}
	aml_analog_ops->get_afc = aml_atvdemod_get_afc;
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

static void aml_atvdemod_dt_parse(struct platform_device *pdev)
{
	struct device_node *node;
	unsigned int val;
	int ret;
	node = pdev->dev.of_node;
	/* get interger value */
	if (node) {
		ret = of_property_read_u32(node, "reg_23cf", &val);
		if (ret)
			pr_dbg("Can't find  reg_23cf.\n");
		else
			reg_23cf = val;
		ret = of_property_read_u32(node, "audio_gain_val", &val);
		if (ret)
			pr_dbg("Can't find  audio_gain_val.\n");
		else
			set_audio_gain_val(val);
		ret = of_property_read_u32(node, "video_gain_val", &val);
		if (ret)
			pr_dbg("Can't find  video_gain_val.\n");
		else
			set_video_gain_val(val);
		/* agc pin mux */
		ret = of_property_read_string(node, "pinctrl-names",
			&amlatvdemod_devp->pin_name);
		if (!ret) {
			/* amlatvdemod_devp->pin = */
			/* devm_pinctrl_get_select(&pdev->dev, */
			/* amlatvdemod_devp->pin_name); */
			pr_dbg("atvdemod agc pinmux name:%s\n",
				amlatvdemod_devp->pin_name);
		}
	}
}
static struct resource amlatvdemod_memobj;
void __iomem *amlatvdemod_reg_base;
void __iomem *amlatvdemod_hiu_reg_base;
void __iomem *amlatvdemod_periphs_reg_base;
int amlatvdemod_reg_read(unsigned int reg, unsigned int *val)
{
	*val = readl(amlatvdemod_reg_base + reg);
	return 0;
}

int amlatvdemod_reg_write(unsigned int reg, unsigned int val)
{
	writel(val, (amlatvdemod_reg_base + reg));
	return 0;
}

int amlatvdemod_hiu_reg_read(unsigned int reg, unsigned int *val)
{
	*val = readl(amlatvdemod_hiu_reg_base + ((reg - 0x1000)<<2));
	return 0;
}

int amlatvdemod_hiu_reg_write(unsigned int reg, unsigned int val)
{
	writel(val, (amlatvdemod_hiu_reg_base + ((reg - 0x1000)<<2)));
	return 0;
}
int amlatvdemod_periphs_reg_read(unsigned int reg, unsigned int *val)
{
	*val = readl(amlatvdemod_periphs_reg_base + ((reg - 0x1000)<<2));
	return 0;
}

int amlatvdemod_periphs_reg_write(unsigned int reg, unsigned int val)
{
	writel(val, (amlatvdemod_periphs_reg_base + ((reg - 0x1000)<<2)));
	return 0;
}

static int aml_atvdemod_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct resource *res;
	int size_io_reg;
	res = &amlatvdemod_memobj;
	amlatvdemod_devp = kmalloc(sizeof(struct amlatvdemod_device_s),
		GFP_KERNEL);
	if (!amlatvdemod_devp)
		goto fail_alloc_region;
	memset(amlatvdemod_devp, 0, sizeof(struct amlatvdemod_device_s));
	amlatvdemod_devp->clsp = class_create(THIS_MODULE,
		ATVDEMOD_DEVICE_NAME);
	if (!amlatvdemod_devp->clsp)
		goto fail_create_class;
	ret = class_create_file(amlatvdemod_devp->clsp,
		&class_attr_atvdemod_debug);
	if (ret)
		goto fail_class_create_file;
	amlatvdemod_devp->dev = &pdev->dev;

	/*reg mem*/
	pr_info("%s:amlatvdemod start get  ioremap .\n", __func__);
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "missing memory resource\n");
		return -ENODEV;
	}
	size_io_reg = resource_size(res);
	pr_info("amlatvdemod_probe reg=%p,size=%x\n",
			(void *)res->start, size_io_reg);
	amlatvdemod_reg_base =
		devm_ioremap_nocache(&pdev->dev, res->start, size_io_reg);
	if (!amlatvdemod_reg_base) {
		dev_err(&pdev->dev, "amlatvdemod ioremap failed\n");
		return -ENOMEM;
	}
	pr_info("%s: amlatvdemod maped reg_base =%p, size=%x\n",
			__func__, amlatvdemod_reg_base, size_io_reg);
	/*remap hiu mem*/
	amlatvdemod_hiu_reg_base = ioremap(0xc883c000, 0x2000);
	/*remap periphs mem*/
	amlatvdemod_periphs_reg_base = ioremap(0xc8834000, 0x2000);

	/*initialize the tuner common struct and register*/
	aml_register_fe_drv(AM_DEV_ATV_DEMOD, &aml_atvdemod_drv);

	aml_atvdemod_dt_parse(pdev);
	pr_dbg("[amlatvdemod.] : probe ok.\n");
	return 0;

fail_class_create_file:
	pr_dbg("[amlatvdemod.] : atvdemod class file create error.\n");
	class_destroy(amlatvdemod_devp->clsp);
fail_create_class:
	pr_dbg("[amlatvdemod.] : atvdemod class create error.\n");
	kfree(amlatvdemod_devp);
fail_alloc_region:
	pr_dbg("[amlatvdemod.] : atvdemod alloc error.\n");
	pr_dbg("[amlatvdemod.] : atvdemod_init fail.\n");
	return ret;
}

static int __exit aml_atvdemod_remove(struct platform_device *pdev)
{
	if (amlatvdemod_devp == NULL)
		return -1;
	class_destroy(amlatvdemod_devp->clsp);
	aml_unregister_fe_drv(AM_DEV_ATV_DEMOD, &aml_atvdemod_drv);
	kfree(amlatvdemod_devp);
	pr_dbg("[amlatvdemod.] : amvecm_remove.\n");
	return 0;
}


static const struct of_device_id aml_atvdemod_dt_match[] = {
	{
		.compatible = "amlogic, aml_atv_demod",
	},
	{},
};

static struct platform_driver aml_atvdemod_driver = {
	.driver = {
		.name = "aml_atv_demod",
		.owner = THIS_MODULE,
		.of_match_table = aml_atvdemod_dt_match,
	},
	.probe = aml_atvdemod_probe,
	.remove = __exit_p(aml_atvdemod_remove),
};


static int __init aml_atvdemod_init(void)
{
	if (platform_driver_register(&aml_atvdemod_driver)) {
		pr_err("failed to register amlatvdemod driver module\n");
		return -ENODEV;
	}
	pr_dbg("[amlatvdemod..]%s.\n", __func__);
	return 0;
}

static void __exit aml_atvdemod_exit(void)
{
	platform_driver_unregister(&aml_atvdemod_driver);
	pr_dbg("[amlatvdemod..]%s: driver removed ok.\n", __func__);
}

MODULE_AUTHOR("dezhi.kong <dezhi.kong@amlogic.com>");
MODULE_DESCRIPTION("aml atv demod device driver");
MODULE_LICENSE("GPL");

fs_initcall(aml_atvdemod_init);
module_exit(aml_atvdemod_exit);
