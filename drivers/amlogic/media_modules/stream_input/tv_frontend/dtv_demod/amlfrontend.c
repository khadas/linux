/*****************************************************************
 **
 **  Copyright (C) 2009 Amlogic,Inc.
 **  All rights reserved
 **        Filename : amlfrontend.c
 **
 **  comment:
 **        Driver for m6_demod demodulator
 **  author :
 **	    Shijie.Rong@amlogic
 **  version :
 **	    v1.0	 12/3/13
 **          v2.0     15/10/12
 ****************************************************************
 */

/*
 *  Driver for gxtv_demod demodulator
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#ifdef ARC_700
#include <asm/arch/am_regs.h>
#else
/* #include <mach/am_regs.h> */
#endif
#include <linux/i2c.h>
#include <linux/gpio.h>
#include "../aml_fe.h"

#include <linux/dma-contiguous.h>
#include <linux/dvb/aml_demod.h>
#include "demod_func.h"
#include "../aml_dvb.h"
#include "amlfrontend.h"

MODULE_PARM_DESC(debug_aml, "\n\t\t Enable frontend debug information");
static int debug_aml;
module_param(debug_aml, int, 0644);

#define pr_dbg(a ...) \
	do { \
		if (debug_aml) { \
			printk(a); \
		} \
	} while (0)
#define pr_error(fmt, args ...) pr_err("GXTV_DEMOD: "fmt, ## args)
#define pr_inf(fmt, args...)  pr_err("GXTV_DEMOD: " fmt, ## args)

static int last_lock = -1;
#define DEMOD_DEVICE_NAME  "gxtv_demod"
static int cci_thread;
static int freq_dvbc;
static struct aml_demod_sta demod_status;
static fe_modulation_t atsc_mode = VSB_8;

long *mem_buf;

MODULE_PARM_DESC(frontend_mode, "\n\t\t Frontend mode 0-DVBC, 1-DVBT");
static int frontend_mode = -1;
module_param(frontend_mode, int, 0444);

MODULE_PARM_DESC(frontend_i2c, "\n\t\t IIc adapter id of frontend");
static int frontend_i2c = -1;
module_param(frontend_i2c, int, 0444);

MODULE_PARM_DESC(frontend_tuner,
		 "\n\t\t Frontend tuner type 0-NULL, 1-DCT7070, 2-Maxliner, 3-FJ2207, 4-TD1316");
static int frontend_tuner = -1;
module_param(frontend_tuner, int, 0444);

MODULE_PARM_DESC(frontend_tuner_addr, "\n\t\t Tuner IIC address of frontend");
static int frontend_tuner_addr = -1;
module_param(frontend_tuner_addr, int, 0444);
static int autoflags, autoFlagsTrig;
static struct mutex aml_lock;

static int Gxtv_Demod_Dvbc_Init(struct aml_fe_dev *dev, int mode);

static ssize_t dvbc_auto_sym_show(struct class *cls,
				  struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "dvbc_autoflags: %s\n", autoflags ? "on" : "off");
}

static ssize_t dvbc_auto_sym_store(struct class *cls,
				   struct class_attribute *attr,
				   const char *buf, size_t count)
{

	return 0;
}

static unsigned int dtmb_mode;

enum {
	DTMB_READ_STRENGTH = 0,
	DTMB_READ_SNR = 1,
	DTMB_READ_LOCK = 2,
	DTMB_READ_BCH = 3,
};



int convert_snr(int in_snr)
{
	int out_snr;
	static int calce_snr[40] = {
		5, 6, 8, 10, 13,
		16, 20, 25, 32, 40,
		50, 63, 80, 100, 126,
		159, 200, 252, 318, 400,
		504, 634, 798, 1005, 1265,
		1592, 2005, 2524, 3177, 4000,
		5036, 6340, 7981, 10048, 12649,
		15924, 20047, 25238, 31773, 40000};
	for (out_snr = 1 ; out_snr <= 40; out_snr++)
		if (in_snr <= calce_snr[out_snr])
			break;

	return out_snr;
}


static ssize_t dtmb_para_show(struct class *cls,
				  struct class_attribute *attr, char *buf)
{
	int snr, lock_status, bch, agc_if_gain;
	struct dvb_frontend *dvbfe;
	int strength = 0;

	if (dtmb_mode == DTMB_READ_STRENGTH) {
		dvbfe = get_si2177_tuner();
		if (dvbfe != NULL)
			if (dvbfe->ops.tuner_ops.get_strength) {
				strength =
				dvbfe->ops.tuner_ops.get_strength(dvbfe);
			}
		if (strength <= -56) {
			agc_if_gain =
				((dtmb_read_reg(DTMB_TOP_FRONT_AGC))&0x3ff);
			strength = dtmb_get_power_strength(agc_if_gain);
		}
		return sprintf(buf, "strength is %d\n", strength);
	} else if (dtmb_mode == DTMB_READ_SNR) {
		snr = dtmb_read_reg(DTMB_TOP_FEC_LOCK_SNR) & 0x3fff;
		snr = convert_snr(snr);
		return sprintf(buf, "snr is %d\n", snr);
	} else if (dtmb_mode == DTMB_READ_LOCK) {
		lock_status =
			(dtmb_read_reg(DTMB_TOP_FEC_LOCK_SNR) >> 14) & 0x1;
		return sprintf(buf, "lock_status is %d\n", lock_status);
	} else if (dtmb_mode == DTMB_READ_BCH) {
		bch = dtmb_read_reg(DTMB_TOP_FEC_BCH_ACC);
		return sprintf(buf, "bch is %d\n", bch);
	} else {
		return sprintf(buf, "dtmb_para_show can't match mode\n");
	}
}



static ssize_t dtmb_para_store(struct class *cls,
				   struct class_attribute *attr,
				   const char *buf, size_t count)
{
	if (buf[0] == '0')
		dtmb_mode = DTMB_READ_STRENGTH;
	else if (buf[0] == '1')
		dtmb_mode = DTMB_READ_SNR;
	else if (buf[0] == '2')
		dtmb_mode = DTMB_READ_LOCK;
	else if (buf[0] == '3')
		dtmb_mode = DTMB_READ_BCH;

	return count;
}

static int readregdata;

static ssize_t dvbc_reg_show(struct class *cls, struct class_attribute *attr,
			     char *buf)
{
/*      int readregaddr=0;*/
	char *pbuf = buf;

	pbuf += sprintf(pbuf, "%x", readregdata);

	pr_dbg("read dvbc_reg\n");
	return pbuf - buf;
}

static ssize_t dvbc_reg_store(struct class *cls, struct class_attribute *attr,
			      const char *buf, size_t count)
{
	return 0;
}

static CLASS_ATTR(auto_sym, 0644, dvbc_auto_sym_show, dvbc_auto_sym_store);
static CLASS_ATTR(dtmb_para, 0644, dtmb_para_show, dtmb_para_store);
static CLASS_ATTR(dvbc_reg, 0644, dvbc_reg_show, dvbc_reg_store);

#if 0
static irqreturn_t amdemod_isr(int irq, void *data)
{
/*	struct aml_fe_dev *state = data;
 *
 * #define dvb_isr_islock() (((frontend_mode==0)&&dvbc_isr_islock()) \
 * ||((frontend_mode==1)&&dvbt_isr_islock()))
 * #define dvb_isr_monitor() do {\
 *              if(frontend_mode==1) dvbt_isr_monitor(); }while(0)
 * #define dvb_isr_cancel() do { if(frontend_mode==1) dvbt_isr_cancel(); \
 *              else if(frontend_mode==0) dvbc_isr_cancel();}while(0)
 *
 *      dvb_isr_islock();
 *      {
 *              if(waitqueue_active(&state->lock_wq))
 *                      wake_up_interruptible(&state->lock_wq);
 *      }
 *
 *      dvb_isr_monitor();
 *
 *      dvb_isr_cancel();
 */

	return IRQ_HANDLED;
}
#endif

static int install_isr(struct aml_fe_dev *state)
{
	int r = 0;

	/* hook demod isr */
/*	pr_dbg("amdemod irq register[IRQ(%d)].\n", INT_DEMOD);
 *      r = request_irq(INT_DEMOD, &amdemod_isr,
 *                              IRQF_SHARED, "amldemod",
 *                              (void *)state);
 *      if (r) {
 *              pr_error("amdemod irq register error.\n");
 *      }
 */
	return r;
}


static int amdemod_qam(fe_modulation_t qam)
{
	switch (qam) {
	case QAM_16:
		return 0;
	case QAM_32:
		return 1;
	case QAM_64:
		return 2;
	case QAM_128:
		return 3;
	case QAM_256:
		return 4;
	case VSB_8:
		return 5;
	case QAM_AUTO:
		return 6;
	default:
		return 2;
	}
	return 2;
}

static int amdemod_stat_islock(struct aml_fe_dev *dev, int mode)
{
	struct aml_demod_sts demod_sts;
	int lock_status;
	int dvbt_status1;

	if (mode == 0) {
		/*DVBC*/
		/*dvbc_status(state->sta, state->i2c, &demod_sts);*/
		demod_sts.ch_sts = apb_read_reg(QAM_BASE + 0x18);
		return demod_sts.ch_sts & 0x1;
	} else if (mode == 1) {
		/*DVBT*/
		dvbt_status1 =
			((apb_read_reg(DVBT_BASE + (0x0a << 2)) >> 20) & 0x3ff);
		lock_status = (apb_read_reg(DVBT_BASE + (0x2a << 2))) & 0xf;
		if ((((lock_status) == 9) || ((lock_status) == 10))
		    && ((dvbt_status1) != 0))
			return 1;
		else
			return 0;
		/*((apb_read_reg(DVBT_BASE+0x0)>>12)&0x1);//
		 * dvbt_get_status_ops()->get_status(&demod_sts, &demod_sta);
		 */
	} else if (mode == 2) {
		/*ISDBT*/
		/*return dvbt_get_status_ops()->get_status
		 * (demod_sts, demod_sta);
		 */
	} else if (mode == 3) {
		/*ATSC*/
		if ((atsc_mode == QAM_64) || (atsc_mode == QAM_256))
			return (atsc_read_iqr_reg() >> 16) == 0x1f;
		else if (atsc_mode == VSB_8)
			return atsc_read_reg(0x0980) == 0x79;
		else
			return (atsc_read_iqr_reg() >> 16) == 0x1f;
	} else if (mode == 4) {
		/*DTMB*/
	/*	pr_dbg("DTMB lock status is %u\n",
	 *	       ((dtmb_read_reg(DTMB_BASE + (0x0e3 << 2)) >> 14) &
	 *		0x1));
	 */
		return (dtmb_read_reg(DTMB_BASE + (0x0e3 << 2)) >> 14) & 0x1;
	}
	return 0;
}

#define amdemod_dvbc_stat_islock(dev)  amdemod_stat_islock((dev), 0)
#define amdemod_dvbt_stat_islock(dev)  amdemod_stat_islock((dev), 1)
#define amdemod_isdbt_stat_islock(dev)  amdemod_stat_islock((dev), 2)
#define amdemod_atsc_stat_islock(dev)  amdemod_stat_islock((dev), 3)
#define amdemod_dtmb_stat_islock(dev)  amdemod_stat_islock((dev), 4)

static int gxtv_demod_dvbc_set_qam_mode(struct dvb_frontend *fe)
{
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;
	struct aml_demod_dvbc param;    /*mode 0:16, 1:32, 2:64, 3:128, 4:256*/

	memset(&param, 0, sizeof(param));
	param.mode = amdemod_qam(c->modulation);
	dvbc_set_qam_mode(param.mode);
	return 0;
}

static void gxtv_demod_dvbc_release(struct dvb_frontend *fe)
{
/*
 *	struct aml_fe_dev *state = fe->demodulator_priv;
 *
 *	uninstall_isr(state);
 *
 *	kfree(state);
 */
}

static int gxtv_demod_dvbc_read_status
	(struct dvb_frontend *fe, fe_status_t *status)
{
/*      struct aml_fe_dev *dev = afe->dtv_demod;*/
	struct aml_demod_sts demod_sts;
/*      struct aml_demod_sta demod_sta;*/
/*      struct aml_demod_i2c demod_i2c;*/
	int ilock;

	demod_sts.ch_sts = apb_read_reg(QAM_BASE + 0x18);
/*      dvbc_status(&demod_sta, &demod_i2c, &demod_sts);*/
	if (demod_sts.ch_sts & 0x1) {
		ilock = 1;
		*status =
			FE_HAS_LOCK | FE_HAS_SIGNAL | FE_HAS_CARRIER |
			FE_HAS_VITERBI | FE_HAS_SYNC;
	} else {
		ilock = 0;
		*status = FE_TIMEDOUT;
	}
	if (last_lock != ilock) {
		pr_error("%s.\n",
			 ilock ? "!!  >> LOCK << !!" : "!! >> UNLOCK << !!");
		last_lock = ilock;
	}

	return 0;
}

static int gxtv_demod_dvbc_read_ber(struct dvb_frontend *fe, u32 *ber)
{
	/*struct aml_fe_dev *dev = afe->dtv_demod;*/
	struct aml_demod_sts demod_sts;
	struct aml_demod_i2c demod_i2c;
	struct aml_demod_sta demod_sta;

	dvbc_status(&demod_sta, &demod_i2c, &demod_sts);
	*ber = demod_sts.ch_ber;
	return 0;
}

static int gxtv_demod_dvbc_read_signal_strength
	(struct dvb_frontend *fe, u16 *strength)
{
	struct aml_fe *afe = fe->demodulator_priv;
	struct aml_fe_dev *dev = afe->dtv_demod;

	*strength = 256 - tuner_get_ch_power(dev);

	return 0;
}

static int gxtv_demod_dvbc_read_snr(struct dvb_frontend *fe, u16 *snr)
{
	struct aml_demod_sts demod_sts;
	struct aml_demod_i2c demod_i2c;
	struct aml_demod_sta demod_sta;

	dvbc_status(&demod_sta, &demod_i2c, &demod_sts);
	*snr = demod_sts.ch_snr / 100;
	return 0;
}

static int gxtv_demod_dvbc_read_ucblocks(struct dvb_frontend *fe, u32 *ucblocks)
{
	*ucblocks = 0;
	return 0;
}

/*extern int aml_fe_analog_set_frontend(struct dvb_frontend *fe);*/

static int gxtv_demod_dvbc_set_frontend(struct dvb_frontend *fe)
{
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;
	struct aml_demod_dvbc param;    /*mode 0:16, 1:32, 2:64, 3:128, 4:256*/
	struct aml_demod_sts demod_sts;
	struct aml_demod_i2c demod_i2c;
	struct aml_fe *afe = fe->demodulator_priv;
	struct aml_fe_dev *dev = afe->dtv_demod;
	int error, times;

	demod_i2c.tuner = dev->drv->id;
	demod_i2c.addr = dev->i2c_addr;
	times = 2;
	memset(&param, 0, sizeof(param));
	param.ch_freq = c->frequency / 1000;
	param.mode = amdemod_qam(c->modulation);
	param.symb_rate = c->symbol_rate / 1000;
	if ((param.mode == 3) && (demod_status.tmp != Adc_mode)) {
		Gxtv_Demod_Dvbc_Init(dev, Adc_mode);
		pr_dbg("Gxtv_Demod_Dvbc_Init,Adc_mode\n");
	} else {
		/*Gxtv_Demod_Dvbc_Init(dev,Cry_mode);*/
	}
	if (autoflags == 0) {
		/*pr_dbg("QAM_TUNING mode\n");*/
		/*flag=0;*/
	}
	if ((autoflags == 1) && (autoFlagsTrig == 0)
	    && (freq_dvbc == param.ch_freq)) {
		pr_dbg("now is auto symbrating\n");
		return 0;
	}
	autoFlagsTrig = 0;
	last_lock = -1;
	pr_dbg("[gxtv_demod_dvbc_set_frontend]PARA\t"
	       "demod_i2c.tuner is %d||||demod_i2c.addr is %d||||\t"
	       "param.ch_freq is %d||||param.symb_rate is %d,\t"
	       "param.mode is %d\n",
	       demod_i2c.tuner, demod_i2c.addr, param.ch_freq,
	       param.symb_rate, param.mode);
retry:
	aml_dmx_before_retune(afe->ts, fe);
	aml_fe_analog_set_frontend(fe);
	dvbc_set_ch(&demod_status, &demod_i2c, &param);
	if (autoflags == 1) {
		pr_dbg("QAM_PLAYING mode,start auto sym\n");
		dvbc_set_auto_symtrack();
		/*      flag=1;*/
	}
/*rsj_debug*/

	dvbc_status(&demod_status, &demod_i2c, &demod_sts);
	freq_dvbc = param.ch_freq;

	times--;
	if (amdemod_dvbc_stat_islock(dev) && times) {
		int lock;

		aml_dmx_start_error_check(afe->ts, fe);
		msleep(20);
		error = aml_dmx_stop_error_check(afe->ts, fe);
		lock = amdemod_dvbc_stat_islock(dev);
		if ((error > 200) || !lock) {
			pr_error
				("amlfe too many error, error count:%d\t"
				"lock statuc:%d, retry\n",
				error, lock);
			goto retry;
		}
	}

	aml_dmx_after_retune(afe->ts, fe);

	afe->params = *c;
/*	afe->params.frequency = c->frequency;
 *      afe->params.u.qam.symbol_rate = c->symbol_rate;
 *      afe->params.u.qam.modulation = c->modulation;
 */

	pr_dbg("AML amldemod => frequency=%d,symbol_rate=%d\r\n", c->frequency,
	       c->symbol_rate);
	return 0;
}

static int gxtv_demod_dvbc_get_frontend(struct dvb_frontend *fe)
{                               /*these content will be writed into eeprom .*/
	struct aml_fe *afe = fe->demodulator_priv;
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;
	int qam_mode;

	qam_mode = apb_read_reg(QAM_BASE + 0x008);
	afe->params.modulation = (qam_mode & 7) + 1;
	pr_dbg("[mode] is %d\n", afe->params.modulation);

	*c = afe->params;
/*	c->modulation= afe->params.u.qam.modulation;
 *      c->frequency= afe->params.frequency;
 *      c->symbol_rate= afe->params.u.qam.symbol_rate;
 */
	return 0;
}

static int Gxtv_Demod_Dvbc_Init(struct aml_fe_dev *dev, int mode)
{
	struct aml_demod_sys sys;
	struct aml_demod_i2c i2c;

	pr_dbg("AML Demod DVB-C init\r\n");
	memset(&sys, 0, sizeof(sys));
	memset(&i2c, 0, sizeof(i2c));
	i2c.tuner = dev->drv->id;
	i2c.addr = dev->i2c_addr;
	/* 0 -DVBC, 1-DVBT, ISDBT, 2-ATSC*/
	demod_status.dvb_mode = Gxtv_Dvbc;

	if (mode == Adc_mode) {
		sys.adc_clk = Adc_Clk_25M;
		sys.demod_clk = Demod_Clk_200M;
		demod_status.tmp = Adc_mode;
	} else {
		sys.adc_clk = Adc_Clk_24M;
		sys.demod_clk = Demod_Clk_72M;
		demod_status.tmp = Cry_mode;
	}
	demod_status.ch_if = Si2176_5M_If * 1000;
	pr_dbg("[%s]adc_clk is %d,demod_clk is %d\n", __func__, sys.adc_clk,
	       sys.demod_clk);
	autoFlagsTrig = 0;
	demod_set_sys(&demod_status, &i2c, &sys);
	return 0;
}

static void gxtv_demod_dvbt_release(struct dvb_frontend *fe)
{
/*
 *	struct aml_fe_dev *state = fe->demodulator_priv;
 *
 *	uninstall_isr(state);
 *
 *	kfree(state);
 */
}

static int gxtv_demod_dvbt_read_status
	(struct dvb_frontend *fe, fe_status_t *status)
{
/*      struct aml_fe *afe = fe->demodulator_priv;*/
	struct aml_demod_i2c demod_i2c;
	struct aml_demod_sta demod_sta;
	int ilock;
	unsigned char s = 0;

	s = dvbt_get_status_ops()->get_status(&demod_sta, &demod_i2c);
	if (s == 1) {
		ilock = 1;
		*status =
			FE_HAS_LOCK | FE_HAS_SIGNAL | FE_HAS_CARRIER |
			FE_HAS_VITERBI | FE_HAS_SYNC;
	} else {
		ilock = 0;
		*status = FE_TIMEDOUT;
	}
	if (last_lock != ilock) {
		pr_error("%s.\n",
			 ilock ? "!!  >> LOCK << !!" : "!! >> UNLOCK << !!");
		last_lock = ilock;
	}

	return 0;
}

static int gxtv_demod_dvbt_read_ber(struct dvb_frontend *fe, u32 *ber)
{
/*      struct aml_fe *afe = fe->demodulator_priv;*/
	struct aml_demod_i2c demod_i2c;
	struct aml_demod_sta demod_sta;

	*ber = dvbt_get_status_ops()->get_ber(&demod_sta, &demod_i2c) & 0xffff;
	return 0;
}

static int gxtv_demod_dvbt_read_signal_strength
	(struct dvb_frontend *fe, u16 *strength)
{
	struct aml_fe *afe = fe->demodulator_priv;
	struct aml_fe_dev *dev = afe->dtv_demod;

	*strength = 256 - tuner_get_ch_power(dev);
	pr_dbg("[RSJ]tuner strength is %d dbm\n", *strength);
	return 0;
}

static int gxtv_demod_dvbt_read_snr(struct dvb_frontend *fe, u16 *snr)
{
/*      struct aml_fe *afe = fe->demodulator_priv;*/
/*      struct aml_demod_sts demod_sts;*/
	struct aml_demod_i2c demod_i2c;
	struct aml_demod_sta demod_sta;

	*snr = dvbt_get_status_ops()->get_snr(&demod_sta, &demod_i2c);
	*snr /= 8;
	pr_dbg("[RSJ]snr is %d dbm\n", *snr);
	return 0;
}

static int gxtv_demod_dvbt_read_ucblocks(struct dvb_frontend *fe, u32 *ucblocks)
{
	*ucblocks = 0;
	return 0;
}

static int gxtv_demod_dvbt_set_frontend(struct dvb_frontend *fe)
{
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;
	/*struct aml_demod_sts demod_sts;*/
	struct aml_demod_i2c demod_i2c;
	int error, times;
	struct aml_demod_dvbt param;
	struct aml_fe *afe = fe->demodulator_priv;
	struct aml_fe_dev *dev = afe->dtv_demod;

	demod_i2c.tuner = dev->drv->id;
	demod_i2c.addr = dev->i2c_addr;

	times = 2;

	/*////////////////////////////////////*/
	/* bw == 0 : 8M*/
	/*       1 : 7M*/
	/*       2 : 6M*/
	/*       3 : 5M*/
	/* agc_mode == 0: single AGC*/
	/*             1: dual AGC*/
	/*////////////////////////////////////*/
	memset(&param, 0, sizeof(param));
	param.ch_freq = c->frequency / 1000;
	param.bw = c->bandwidth_hz;
	param.agc_mode = 1;
	/*ISDBT or DVBT : 0 is QAM, 1 is DVBT, 2 is ISDBT,
	 * 3 is DTMB, 4 is ATSC
	 */
	param.dat0 = 1;
	last_lock = -1;

retry:
	aml_dmx_before_retune(AM_TS_SRC_TS2, fe);
	aml_fe_analog_set_frontend(fe);
	dvbt_set_ch(&demod_status, &demod_i2c, &param);

	/*      for(count=0;count<10;count++){
	 * if(amdemod_dvbt_stat_islock(dev)){
	 * pr_dbg("first lock success\n");
	 * break;
	 * }
	 *
	 * msleep(200);
	 * }
	 */
/*rsj_debug*/

/**/

	times--;
	if (amdemod_dvbt_stat_islock(dev) && times) {
		int lock;

		aml_dmx_start_error_check(AM_TS_SRC_TS2, fe);
		msleep(20);
		error = aml_dmx_stop_error_check(AM_TS_SRC_TS2, fe);
		lock = amdemod_dvbt_stat_islock(dev);
		if ((error > 200) || !lock) {
			pr_error
				("amlfe too many error,\t"
				"error count:%d lock statuc:%d, retry\n",
				error, lock);
			goto retry;
		}
	}

	aml_dmx_after_retune(AM_TS_SRC_TS2, fe);

	afe->params = *c;

	/*pr_dbg("AML amldemod => frequency=%d,symbol_rate=%d\r\n",
	 * p->frequency,p->u.qam.symbol_rate);
	 */
	return 0;
}

static int gxtv_demod_dvbt_get_frontend(struct dvb_frontend *fe)
{                               /*these content will be writed into eeprom .*/
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;
	struct aml_fe *afe = fe->demodulator_priv;

	*c = afe->params;
	return 0;
}

int Gxtv_Demod_Dvbt_Init(struct aml_fe_dev *dev)
{
	struct aml_demod_sys sys;
	struct aml_demod_i2c i2c;

	pr_dbg("AML Demod DVB-T init\r\n");

	memset(&sys, 0, sizeof(sys));
	memset(&i2c, 0, sizeof(i2c));
	memset(&demod_status, 0, sizeof(demod_status));
	i2c.tuner = dev->drv->id;
	i2c.addr = dev->i2c_addr;
	/* 0 -DVBC, 1-DVBT, ISDBT, 2-ATSC*/
	demod_status.dvb_mode = Gxtv_Dvbt_Isdbt;
	sys.adc_clk = Adc_Clk_24M;
	sys.demod_clk = Demod_Clk_60M;
	demod_status.ch_if = Si2176_5M_If * 1000;
	demod_set_sys(&demod_status, &i2c, &sys);
	return 0;
}

static void gxtv_demod_atsc_release(struct dvb_frontend *fe)
{
/*
 *	struct aml_fe_dev *state = fe->demodulator_priv;
 *
 *	uninstall_isr(state);
 *
 *	kfree(state);
 */
}

static int gxtv_demod_atsc_set_qam_mode(struct dvb_frontend *fe)
{
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;
	struct aml_demod_atsc param;    /*mode  3:64,  5:256, 7:vsb*/
	fe_modulation_t mode;

	memset(&param, 0, sizeof(param));
	mode = c->modulation;
	pr_dbg("mode is %d\n", mode);
	atsc_qam_set(mode);
	return 0;
}

static int gxtv_demod_atsc_read_status
	(struct dvb_frontend *fe, fe_status_t *status)
{
	struct aml_fe *afe = fe->demodulator_priv;
	struct aml_fe_dev *dev = afe->dtv_demod;
/*      struct aml_demod_i2c demod_i2c;*/
/*      struct aml_demod_sta demod_sta;*/
	int ilock;
	unsigned char s = 0;

	s = amdemod_atsc_stat_islock(dev);
	if (s == 1) {
		ilock = 1;
		*status =
			FE_HAS_LOCK | FE_HAS_SIGNAL | FE_HAS_CARRIER |
			FE_HAS_VITERBI | FE_HAS_SYNC;
	} else {
		ilock = 0;
		*status = FE_TIMEDOUT;
	}
	if (last_lock != ilock) {
		pr_error("%s.\n",
			 ilock ? "!!  >> LOCK << !!" : "!! >> UNLOCK << !!");
		last_lock = ilock;
	}

	return 0;
}

static int gxtv_demod_atsc_read_ber(struct dvb_frontend *fe, u32 *ber)
{
/*      struct aml_fe *afe = fe->demodulator_priv;*/
/*      struct aml_fe_dev *dev = afe->dtv_demod;*/
/*      struct aml_demod_sts demod_sts;*/
/*      struct aml_demod_i2c demod_i2c;*/
/*      struct aml_demod_sta demod_sta;*/

/* check_atsc_fsm_status();*/
	return 0;
}

static int gxtv_demod_atsc_read_signal_strength
	(struct dvb_frontend *fe, u16 *strength)
{
	struct aml_fe *afe = fe->demodulator_priv;
	struct aml_fe_dev *dev = afe->dtv_demod;

	*strength = tuner_get_ch_power(dev);
	return 0;
}

static int gxtv_demod_atsc_read_snr(struct dvb_frontend *fe, u16 *snr)
{
/*      struct aml_fe *afe = fe->demodulator_priv;*/
/*      struct aml_fe_dev *dev = afe->dtv_demod;*/

/*      struct aml_demod_sts demod_sts;*/
/*      struct aml_demod_i2c demod_i2c;*/
/*      struct aml_demod_sta demod_sta;*/

/*      * snr=check_atsc_fsm_status();*/
	return 0;
}

static int gxtv_demod_atsc_read_ucblocks(struct dvb_frontend *fe, u32 *ucblocks)
{
	*ucblocks = 0;
	return 0;
}

static int gxtv_demod_atsc_set_frontend(struct dvb_frontend *fe)
{
/*      struct amlfe_state *state = fe->demodulator_priv;*/
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;
	struct aml_demod_atsc param;
/*      struct aml_demod_sta demod_sta;*/
/*      struct aml_demod_sts demod_sts;*/
	struct aml_demod_i2c demod_i2c;
	int error, times;
	struct aml_fe *afe = fe->demodulator_priv;
	struct aml_fe_dev *dev = afe->dtv_demod;

	demod_i2c.tuner = dev->drv->id;
	demod_i2c.addr = dev->i2c_addr;
	times = 2;

	memset(&param, 0, sizeof(param));
	param.ch_freq = c->frequency / 1000;

	last_lock = -1;
	/*p->u.vsb.modulation=QAM_64;*/
	atsc_mode = c->modulation;
	/* param.mode = amdemod_qam(p->u.vsb.modulation);*/
	param.mode = c->modulation;

retry:
	aml_dmx_before_retune(AM_TS_SRC_TS2, fe);
	aml_fe_analog_set_frontend(fe);
	atsc_set_ch(&demod_status, &demod_i2c, &param);

	/*{
	 * int ret;
	 * ret = wait_event_interruptible_timeout(
	 *              dev->lock_wq, amdemod_atsc_stat_islock(dev), 4*HZ);
	 * if(!ret)     pr_error("amlfe wait lock timeout.\n");
	 * }
	 */
/*rsj_debug*/
	/*      int count;
	 * for(count=0;count<10;count++){
	 * if(amdemod_atsc_stat_islock(dev)){
	 * pr_dbg("first lock success\n");
	 * break;
	 * }
	 *
	 * msleep(200);
	 * }
	 */

	times--;
	if (amdemod_atsc_stat_islock(dev) && times) {
		int lock;

		aml_dmx_start_error_check(AM_TS_SRC_TS2, fe);
		msleep(20);
		error = aml_dmx_stop_error_check(AM_TS_SRC_TS2, fe);
		lock = amdemod_atsc_stat_islock(dev);
		if ((error > 200) || !lock) {
			pr_error
				("amlfe too many error,\t"
				"error count:%d lock statuc:%d, retry\n",
				error, lock);
			goto retry;
		}
	}

	aml_dmx_after_retune(AM_TS_SRC_TS2, fe);

	afe->params = *c;
	/*pr_dbg("AML amldemod => frequency=%d,symbol_rate=%d\r\n",
	 * p->frequency,p->u.qam.symbol_rate);
	 */
	return 0;
}

static int gxtv_demod_atsc_get_frontend(struct dvb_frontend *fe)
{                               /*these content will be writed into eeprom .*/
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;
	struct aml_fe *afe = fe->demodulator_priv;

	pr_dbg("c->frequency is %d\n", c->frequency);
	*c = afe->params;
	return 0;
}

int Gxtv_Demod_Atsc_Init(struct aml_fe_dev *dev)
{
	struct aml_demod_sys sys;
	struct aml_demod_i2c i2c;

	pr_dbg("AML Demod ATSC init\r\n");

	memset(&sys, 0, sizeof(sys));
	memset(&i2c, 0, sizeof(i2c));
	memset(&demod_status, 0, sizeof(demod_status));
	/* 0 -DVBC, 1-DVBT, ISDBT, 2-ATSC*/
	demod_status.dvb_mode = Gxtv_Atsc;
	sys.adc_clk = Adc_Clk_25_2M;    /*Adc_Clk_26M;*/
	sys.demod_clk = Demod_Clk_75M;  /*Demod_Clk_71M;//Demod_Clk_78M;*/
	demod_status.ch_if = 6350;
	demod_status.tmp = Adc_mode;
	demod_set_sys(&demod_status, &i2c, &sys);
	return 0;
}

static void gxtv_demod_dtmb_release(struct dvb_frontend *fe)
{
/*
 *	struct aml_fe_dev *state = fe->demodulator_priv;
 *
 *	uninstall_isr(state);
 *
 *	kfree(state);
 */
}

static int gxtv_demod_dtmb_read_status
	(struct dvb_frontend *fe, fe_status_t *status)
{
	struct aml_fe *afe = fe->demodulator_priv;
	struct aml_fe_dev *dev = afe->dtv_demod;
/*      struct aml_demod_i2c demod_i2c;*/
/*      struct aml_demod_sta demod_sta;*/
	int ilock;
	unsigned char s = 0;

/*      s = amdemod_dtmb_stat_islock(dev);*/
/*      if(s==1)*/
	if (is_meson_txl_cpu())
		s = dtmb_check_status_txl(fe);
	else
		s = dtmb_check_status_gxtv(fe);
	s = amdemod_dtmb_stat_islock(dev);
/*      s=1;*/
	if (s == 1) {
		ilock = 1;
		*status =
			FE_HAS_LOCK | FE_HAS_SIGNAL | FE_HAS_CARRIER |
			FE_HAS_VITERBI | FE_HAS_SYNC;
	} else {
		ilock = 0;
		*status = FE_TIMEDOUT;
	}
	if (last_lock != ilock) {
		pr_error("%s.\n",
			 ilock ? "!!  >> LOCK << !!" : "!! >> UNLOCK << !!");
		last_lock = ilock;
	}

	return 0;
}

static int gxtv_demod_dtmb_read_ber(struct dvb_frontend *fe, u32 *ber)
{
/*      struct aml_fe *afe = fe->demodulator_priv;*/
/*      struct aml_fe_dev *dev = afe->dtv_demod;*/
/*      struct aml_demod_sts demod_sts;*/
/*      struct aml_demod_i2c demod_i2c;*/
/*      struct aml_demod_sta demod_sta;*/

/* check_atsc_fsm_status();*/
/* int fec_bch_add; */
/* fec_bch_add = dtmb_read_reg(0xdf); */
/* *ber = fec_bch_add; */
	return 0;
}

static int gxtv_demod_dtmb_read_signal_strength
		(struct dvb_frontend *fe, u16 *strength)
{
	struct aml_fe *afe = fe->demodulator_priv;
	struct aml_fe_dev *dev = afe->dtv_demod;

	*strength = tuner_get_ch_power(dev);
	return 0;
}

static int gxtv_demod_dtmb_read_snr(struct dvb_frontend *fe, u16 *snr)
{
/*      struct aml_fe *afe = fe->demodulator_priv;*/
/*      struct aml_fe_dev *dev = afe->dtv_demod;*/
#if 1
	int tmp, snr_avg;

	tmp = snr_avg = 0;
	tmp = dtmb_read_reg(DTMB_TOP_FEC_LOCK_SNR);
/*	snr_avg = (tmp >> 16) & 0x3fff;
 *	if (snr_avg >= 2048)
 *		snr_avg = snr_avg - 4096;
 *	snr_avg = snr_avg / 32;
 */
	*snr = tmp&0xff;
#endif
	return 0;
}

static int gxtv_demod_dtmb_read_ucblocks(struct dvb_frontend *fe, u32 *ucblocks)
{
	*ucblocks = 0;
	return 0;
}

static int gxtv_demod_dtmb_read_fsm(struct dvb_frontend *fe, u32 *fsm_status)
{
	int tmp;

	tmp = dtmb_read_reg(DTMB_TOP_CTRL_FSM_STATE0);
	*fsm_status =  tmp&0xffffffff;
	pr_dbg("[rsj] fsm_status is %x\n", *fsm_status);
	return 0;
}


static int gxtv_demod_dtmb_set_frontend(struct dvb_frontend *fe)
{
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;
	struct aml_demod_dtmb param;
/*      struct aml_demod_sta demod_sta;*/
/*      struct aml_demod_sts demod_sts;*/
	struct aml_demod_i2c demod_i2c;
	int times;
	struct aml_fe *afe = fe->demodulator_priv;
	struct aml_fe_dev *dev = afe->dtv_demod;

	demod_i2c.tuner = dev->drv->id;
	demod_i2c.addr = dev->i2c_addr;
	times = 2;
	pr_dbg("gxtv_demod_dtmb_set_frontend,freq is %d\n", c->frequency);
	memset(&param, 0, sizeof(param));
	param.ch_freq = c->frequency / 1000;

	last_lock = -1;
/* demod_power_switch(PWR_OFF); */
	aml_fe_analog_set_frontend(fe);
	msleep(100);
/* demod_power_switch(PWR_ON); */
	dtmb_set_ch(&demod_status, &demod_i2c, &param);
	afe->params = *c;
	/*      pr_dbg("AML amldemod => frequency=%d,symbol_rate=%d\r\n",
	 * p->frequency,p->u.qam.symbol_rate);
	 */
	return 0;
}

static int gxtv_demod_dtmb_get_frontend(struct dvb_frontend *fe)
{                               /*these content will be writed into eeprom .*/
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;
	struct aml_fe *afe = fe->demodulator_priv;

	*c = afe->params;
/*      pr_dbg("[get frontend]c->frequency is %d\n",c->frequency);*/
	return 0;
}

int Gxtv_Demod_Dtmb_Init(struct aml_fe_dev *dev)
{
	struct aml_demod_sys sys;
	struct aml_demod_i2c i2c;

	pr_dbg("AML Demod DTMB init\r\n");

	memset(&sys, 0, sizeof(sys));
	memset(&i2c, 0, sizeof(i2c));
	memset(&demod_status, 0, sizeof(demod_status));
	/* 0 -DVBC, 1-DVBT, ISDBT, 2-ATSC*/
	demod_status.dvb_mode = Gxtv_Dtmb;
	if (is_meson_txl_cpu()) {
		sys.adc_clk = Adc_Clk_25M;      /*Adc_Clk_26M;*/
		sys.demod_clk = Demod_Clk_225M;
	} else {
		sys.adc_clk = Adc_Clk_25M;      /*Adc_Clk_26M;*/
		sys.demod_clk = Demod_Clk_200M;
	}
	demod_status.ch_if = Si2176_5M_If;
	demod_status.tmp = Adc_mode;
	demod_status.spectrum = dev->spectrum;
	demod_set_sys(&demod_status, &i2c, &sys);
	return 0;
}

static int gxtv_demod_fe_get_ops(struct aml_fe_dev *dev, int mode, void *ops)
{
	struct dvb_frontend_ops *fe_ops = (struct dvb_frontend_ops *)ops;

	if (mode == AM_FE_OFDM) {
		fe_ops->info.frequency_min = 51000000;
		fe_ops->info.frequency_max = 858000000;
		fe_ops->info.frequency_stepsize = 0;
		fe_ops->info.frequency_tolerance = 0;
		fe_ops->info.caps =
			FE_CAN_FEC_1_2 | FE_CAN_FEC_2_3 | FE_CAN_FEC_3_4 |
			FE_CAN_FEC_5_6 | FE_CAN_FEC_7_8 | FE_CAN_FEC_AUTO |
			FE_CAN_QPSK | FE_CAN_QAM_16 | FE_CAN_QAM_64 |
			FE_CAN_QAM_AUTO | FE_CAN_TRANSMISSION_MODE_AUTO |
			FE_CAN_GUARD_INTERVAL_AUTO | FE_CAN_HIERARCHY_AUTO |
			FE_CAN_RECOVER | FE_CAN_MUTE_TS;
		fe_ops->release = gxtv_demod_dvbt_release;
		fe_ops->set_frontend = gxtv_demod_dvbt_set_frontend;
		fe_ops->get_frontend = gxtv_demod_dvbt_get_frontend;
		fe_ops->read_status = gxtv_demod_dvbt_read_status;
		fe_ops->read_ber = gxtv_demod_dvbt_read_ber;
		fe_ops->read_signal_strength =
			gxtv_demod_dvbt_read_signal_strength;
		fe_ops->read_snr = gxtv_demod_dvbt_read_snr;
		fe_ops->read_ucblocks = gxtv_demod_dvbt_read_ucblocks;
		fe_ops->read_dtmb_fsm = NULL;

		pr_dbg("=========================dvbt demod init\r\n");
		Gxtv_Demod_Dvbt_Init(dev);
	} else if (mode == AM_FE_QAM) {
		fe_ops->info.frequency_min = 51000000;
		fe_ops->info.frequency_max = 858000000;
		fe_ops->info.frequency_stepsize = 0;
		fe_ops->info.frequency_tolerance = 0;
		fe_ops->info.caps =
			FE_CAN_FEC_1_2 | FE_CAN_FEC_2_3 | FE_CAN_FEC_3_4 |
			FE_CAN_FEC_5_6 | FE_CAN_FEC_7_8 | FE_CAN_FEC_AUTO |
			FE_CAN_QPSK | FE_CAN_QAM_16 | FE_CAN_QAM_32 |
			FE_CAN_QAM_128 | FE_CAN_QAM_256 | FE_CAN_QAM_64 |
			FE_CAN_QAM_AUTO | FE_CAN_TRANSMISSION_MODE_AUTO |
			FE_CAN_GUARD_INTERVAL_AUTO | FE_CAN_HIERARCHY_AUTO |
			FE_CAN_RECOVER | FE_CAN_MUTE_TS;

		fe_ops->release = gxtv_demod_dvbc_release;
		fe_ops->set_frontend = gxtv_demod_dvbc_set_frontend;
		fe_ops->get_frontend = gxtv_demod_dvbc_get_frontend;
		fe_ops->read_status = gxtv_demod_dvbc_read_status;
		fe_ops->read_ber = gxtv_demod_dvbc_read_ber;
		fe_ops->read_signal_strength =
			gxtv_demod_dvbc_read_signal_strength;
		fe_ops->read_snr = gxtv_demod_dvbc_read_snr;
		fe_ops->read_ucblocks = gxtv_demod_dvbc_read_ucblocks;
		fe_ops->set_qam_mode = gxtv_demod_dvbc_set_qam_mode;
		fe_ops->read_dtmb_fsm = NULL;
		install_isr(dev);
		pr_dbg("=========================dvbc demod init\r\n");
		Gxtv_Demod_Dvbc_Init(dev, Adc_mode);
	} else if (mode == AM_FE_ATSC) {
		fe_ops->info.frequency_min = 51000000;
		fe_ops->info.frequency_max = 858000000;
		fe_ops->info.frequency_stepsize = 0;
		fe_ops->info.frequency_tolerance = 0;
		fe_ops->info.caps =
			FE_CAN_FEC_1_2 | FE_CAN_FEC_2_3 | FE_CAN_FEC_3_4 |
			FE_CAN_FEC_5_6 | FE_CAN_FEC_7_8 | FE_CAN_FEC_AUTO |
			FE_CAN_QPSK | FE_CAN_QAM_16 | FE_CAN_QAM_64 |
			FE_CAN_QAM_AUTO | FE_CAN_TRANSMISSION_MODE_AUTO |
			FE_CAN_GUARD_INTERVAL_AUTO | FE_CAN_HIERARCHY_AUTO |
			FE_CAN_RECOVER | FE_CAN_MUTE_TS;

		fe_ops->release = gxtv_demod_atsc_release;
		fe_ops->set_frontend = gxtv_demod_atsc_set_frontend;
		fe_ops->get_frontend = gxtv_demod_atsc_get_frontend;
		fe_ops->read_status = gxtv_demod_atsc_read_status;
		fe_ops->read_ber = gxtv_demod_atsc_read_ber;
		fe_ops->read_signal_strength =
			gxtv_demod_atsc_read_signal_strength;
		fe_ops->read_snr = gxtv_demod_atsc_read_snr;
		fe_ops->read_ucblocks = gxtv_demod_atsc_read_ucblocks;
		fe_ops->set_qam_mode = gxtv_demod_atsc_set_qam_mode;
		fe_ops->read_dtmb_fsm = NULL;
		Gxtv_Demod_Atsc_Init(dev);
	} else if (mode == AM_FE_DTMB) {
		fe_ops->info.frequency_min = 51000000;
		fe_ops->info.frequency_max = 900000000;
		fe_ops->info.frequency_stepsize = 0;
		fe_ops->info.frequency_tolerance = 0;
		fe_ops->info.caps =
			FE_CAN_FEC_1_2 | FE_CAN_FEC_2_3 | FE_CAN_FEC_3_4 |
			FE_CAN_FEC_5_6 | FE_CAN_FEC_7_8 | FE_CAN_FEC_AUTO |
			FE_CAN_QPSK | FE_CAN_QAM_16 | FE_CAN_QAM_64 |
			FE_CAN_QAM_AUTO | FE_CAN_TRANSMISSION_MODE_AUTO |
			FE_CAN_GUARD_INTERVAL_AUTO | FE_CAN_HIERARCHY_AUTO |
			FE_CAN_RECOVER | FE_CAN_MUTE_TS;

		fe_ops->release = gxtv_demod_dtmb_release;
		fe_ops->set_frontend = gxtv_demod_dtmb_set_frontend;
		fe_ops->get_frontend = gxtv_demod_dtmb_get_frontend;
		fe_ops->read_status = gxtv_demod_dtmb_read_status;
		fe_ops->read_ber = gxtv_demod_dtmb_read_ber;
		fe_ops->read_signal_strength =
			gxtv_demod_dtmb_read_signal_strength;
		fe_ops->read_snr = gxtv_demod_dtmb_read_snr;
		fe_ops->read_ucblocks = gxtv_demod_dtmb_read_ucblocks;
		fe_ops->read_dtmb_fsm = gxtv_demod_dtmb_read_fsm;
		Gxtv_Demod_Dtmb_Init(dev);
	}
	return 0;
}

static int gxtv_demod_fe_resume(struct aml_fe_dev *dev)
{
	int memstart_dtmb;

	pr_inf("gxtv_demod_fe_resume\n");
/*	demod_power_switch(PWR_ON);*/
	Gxtv_Demod_Dtmb_Init(dev);
	memstart_dtmb = dev->fe->dtv_demod->mem_start;
	pr_dbg("[im]memstart is %x\n", memstart_dtmb);
	dtmb_write_reg(DTMB_FRONT_MEM_ADDR, memstart_dtmb);
	pr_dbg("[dtmb]mem_buf is 0x%x\n",
	dtmb_read_reg(DTMB_FRONT_MEM_ADDR));
	return 0;
}

static int gxtv_demod_fe_suspend(struct aml_fe_dev *dev)
{
	pr_inf("gxtv_demod_fe_suspend\n");
/*	demod_power_switch(PWR_OFF);*/
	return 0;
}

#ifdef CONFIG_CMA
void dtmb_cma_alloc(struct aml_fe_dev *devp)
{
	unsigned int mem_size = devp->cma_mem_size;

	devp->venc_pages =
			dma_alloc_from_contiguous(&(devp->this_pdev->dev),
			mem_size >> PAGE_SHIFT, 0);
		pr_dbg("[cma]mem_size is %d,%d\n",
			mem_size, mem_size >> PAGE_SHIFT);
		if (devp->venc_pages) {
			devp->mem_start = page_to_phys(devp->venc_pages);
			devp->mem_size  = mem_size;
			pr_dbg("demod mem_start = 0x%x, mem_size = 0x%x\n",
				devp->mem_start, devp->mem_size);
			pr_dbg("demod cma alloc ok!\n");
		} else {
			pr_dbg("demod cma mem undefined2.\n");
		}
}

void dtmb_cma_release(struct aml_fe_dev *devp)
{
	dma_release_from_contiguous(&(devp->this_pdev->dev),
			devp->venc_pages,
			devp->cma_mem_size>>PAGE_SHIFT);
		pr_dbg("demod cma release ok!\n");
	devp->mem_start = 0;
	devp->mem_size = 0;
}
#endif


static int gxtv_demod_fe_enter_mode(struct aml_fe *fe, int mode)
{
	struct aml_fe_dev *dev = fe->dtv_demod;
	int memstart_dtmb;

	/* must enable the adc ref signal for demod, */
	vdac_enable(1, 0x2);

	autoFlagsTrig = 1;
	if (cci_thread)
		if (dvbc_get_cci_task() == 1)
			dvbc_create_cci_task();
	/*mem_buf = (long *)phys_to_virt(memstart);*/
	if (mode == AM_FE_DTMB) {
		Gxtv_Demod_Dtmb_Init(dev);
	if (fe->dtv_demod->cma_flag == 1) {
		pr_dbg("CMA MODE, cma flag is %d,mem size is %d",
			fe->dtv_demod->cma_flag, fe->dtv_demod->cma_mem_size);
		dtmb_cma_alloc(dev);
		memstart_dtmb = dev->mem_start;
	} else {
		memstart_dtmb = fe->dtv_demod->mem_start;
	}
		pr_dbg("[im]memstart is %x\n", memstart_dtmb);
		dtmb_write_reg(DTMB_FRONT_MEM_ADDR, memstart_dtmb);
		pr_dbg("[dtmb]mem_buf is 0x%x\n",
			dtmb_read_reg(DTMB_FRONT_MEM_ADDR));
		/* open arbit */
		demod_set_demod_reg(0x8, DEMOD_REG4);
	} else if (mode == AM_FE_QAM) {
		Gxtv_Demod_Dvbc_Init(dev, Adc_mode);
	}

	return 0;
}

static int gxtv_demod_fe_leave_mode(struct aml_fe *fe, int mode)
{
	struct aml_fe_dev *dev = fe->dtv_demod;

	dtvpll_init_flag(0);
	/*dvbc_timer_exit();*/
	if (cci_thread)
		dvbc_kill_cci_task();
	if (mode == AM_FE_DTMB) {
		/* close arbit */
		demod_set_demod_reg(0x0, DEMOD_REG4);
		if (fe->dtv_demod->cma_flag == 1)
			dtmb_cma_release(dev);
	}

	/* should disable the adc ref signal for demod */
	vdac_enable(0, 0x2);

	return 0;
}

static struct aml_fe_drv gxtv_demod_dtv_demod_drv = {
	.id		= AM_DTV_DEMOD_M1,
	.name		= "AMLDEMOD",
	.capability	=
		AM_FE_QPSK | AM_FE_QAM | AM_FE_ATSC | AM_FE_OFDM | AM_FE_DTMB,
	.get_ops	= gxtv_demod_fe_get_ops,
	.suspend	= gxtv_demod_fe_suspend,
	.resume		= gxtv_demod_fe_resume,
	.enter_mode	= gxtv_demod_fe_enter_mode,
	.leave_mode	= gxtv_demod_fe_leave_mode
};

struct class *gxtv_clsp;
struct class *gxtv_para_clsp;

static int __init gxtvdemodfrontend_init(void)
{
	int ret;

	pr_dbg("register gxtv_demod demod driver\n");
	ret = 0;

	dtvpll_lock_init();
	mutex_init(&aml_lock);

	gxtv_clsp = class_create(THIS_MODULE, DEMOD_DEVICE_NAME);
	if (!gxtv_clsp) {
		pr_error("[gxtv demod]%s:create class error.\n", __func__);
		return PTR_ERR(gxtv_clsp);
	}
	ret = class_create_file(gxtv_clsp, &class_attr_auto_sym);
	if (ret)
		pr_error("[gxtv demod]%s create class error.\n", __func__);

	ret = class_create_file(gxtv_clsp, &class_attr_dtmb_para);
	if (ret)
		pr_error("[gxtv demod]%s create class error.\n", __func__);

	ret = class_create_file(gxtv_clsp, &class_attr_dvbc_reg);
	if (ret)
		pr_error("[gxtv demod]%s create class error.\n", __func__);

	return aml_register_fe_drv(AM_DEV_DTV_DEMOD, &gxtv_demod_dtv_demod_drv);
}

static void __exit gxtvdemodfrontend_exit(void)
{
	pr_dbg("unregister gxtv_demod demod driver\n");

	mutex_destroy(&aml_lock);

	class_remove_file(gxtv_clsp, &class_attr_auto_sym);
	class_remove_file(gxtv_clsp, &class_attr_dtmb_para);
	class_remove_file(gxtv_clsp, &class_attr_dvbc_reg);
	class_destroy(gxtv_clsp);
	aml_unregister_fe_drv(AM_DEV_DTV_DEMOD, &gxtv_demod_dtv_demod_drv);
}

fs_initcall(gxtvdemodfrontend_init);
module_exit(gxtvdemodfrontend_exit);

MODULE_DESCRIPTION("gxtv_demod DVB-T/DVB-C/DTMB Demodulator driver");
MODULE_AUTHOR("RSJ");
MODULE_LICENSE("GPL");
