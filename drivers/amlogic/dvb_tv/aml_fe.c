/*
 * AMLOGIC DVB frontend driver.
 */

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/string.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/spinlock.h>
#include <linux/fcntl.h>
#include <asm/irq.h>
#include <linux/uaccess.h>
#include <linux/poll.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/gpio/consumer.h>
#include <linux/gpio.h>
#include <linux/dma-contiguous.h>
#include "aml_fe.h"
#include "amlatvdemod/atvdemod_func.h"


#ifdef pr_dbg
#undef pr_dbg
#define pr_dbg(fmt, args ...) \
	do { \
		if (debug_fe) \
			pr_info("FE: " fmt, ## args); \
	} while (0)
#endif
#define pr_error(fmt, args ...) pr_err("FE: " fmt, ## args)
#define pr_inf(fmt, args ...) pr_info("FE: " fmt, ## args)

#define AFC_BEST_LOCK      50
#define ATV_AFC_500KHZ   500000
#define ATV_AFC_1_0MHZ   1000000
#define ATV_AFC_2_0MHZ   2000000

#define AML_FE_MAX_RES          50

MODULE_PARM_DESC(debug_fe, "\n\t\t Enable frontend debug information");
static int debug_fe;
module_param(debug_fe, int, 0644);

static int slow_mode;
module_param(slow_mode, int, 0644);
MODULE_DESCRIPTION("search the channel by slow_mode,by add +1MHz\n");

static int video_mode_manul;
module_param(video_mode_manul, int, 0644);
MODULE_DESCRIPTION("search the video manully by get_frontend api\n");

static int audio_mode_manul;
module_param(audio_mode_manul, int, 0644);
MODULE_DESCRIPTION("search the audio manully by get_froutend api\n");

static int tuner_status_cnt = 16;	/*4-->16 test on sky mxl661 */
module_param(tuner_status_cnt, int, 0644);
MODULE_DESCRIPTION("after write a freq, max cnt value of read tuner status\n");

static int delay_cnt = 10;	/*10-->20ms test on sky mxl661 */
module_param(delay_cnt, int, 0644);
MODULE_DESCRIPTION("delay_cnt value of read cvd format\n");

static int delay_afc = 40;	/*ms new add on sky mxl661 */
module_param(delay_afc, int, 0644);
MODULE_DESCRIPTION("search the channel delay_afc,by add +1ms\n");

static struct aml_fe_drv *tuner_drv_list;
static struct aml_fe_drv *atv_demod_drv_list;
static struct aml_fe_drv *dtv_demod_drv_list;
static struct aml_fe_man fe_man;
static long aml_fe_suspended;
static int memstart = 0x1ef00000;

static int afc_offset;
module_param(afc_offset, uint, 0644);
MODULE_PARM_DESC(afc_offset, "\n afc_offset\n");
static int no_sig_cnt;
struct timer_list aml_timer;
#define AML_INTERVAL		(HZ/100)   /* 10ms, #define HZ 100 */
static unsigned int timer_init_state;
static unsigned int aft_thread_enable;
static unsigned int aft_thread_delaycnt;
static unsigned int aft_thread_enable_cache;
static unsigned int aml_timer_en = 1;
module_param(aml_timer_en, uint, 0644);
MODULE_PARM_DESC(aml_timer_en, "\n aml_timer_en\n");

static DEFINE_SPINLOCK(lock);
static int aml_fe_afc_closer(struct dvb_frontend *fe, int minafcfreq,
			     int maxafcfreq, int isAutoSearch);

typedef int (*hook_func_t) (void);
hook_func_t aml_fe_hook_atv_status = NULL;
hook_func_t aml_fe_hook_hv_lock = NULL;
hook_func_t aml_fe_hook_get_fmt = NULL;
void aml_fe_hook_cvd(hook_func_t atv_mode, hook_func_t cvd_hv_lock,
	hook_func_t get_fmt)
{
	aml_fe_hook_atv_status = atv_mode;
	aml_fe_hook_hv_lock = cvd_hv_lock;
	aml_fe_hook_get_fmt = get_fmt;
	pr_dbg("[aml_fe]%s\n", __func__);
}
EXPORT_SYMBOL(aml_fe_hook_cvd);

int amlogic_gpio_direction_output(unsigned int pin, int value,
				  const char *owner)
{
	gpio_direction_output(pin, value);
	return 0;
}

int amlogic_gpio_request(unsigned int pin, const char *label)
{
	return 0;
}

static v4l2_std_id trans_tvin_fmt_to_v4l2_std(int fmt)
{
	v4l2_std_id std = 0;
	switch (fmt) {
	case  TVIN_SIG_FMT_CVBS_NTSC_M:
	case  TVIN_SIG_FMT_CVBS_NTSC_443:
		std = V4L2_COLOR_STD_NTSC;
		break;
	case  TVIN_SIG_FMT_CVBS_PAL_I:
	case  TVIN_SIG_FMT_CVBS_PAL_M:
	case  TVIN_SIG_FMT_CVBS_PAL_60:
	case  TVIN_SIG_FMT_CVBS_PAL_CN:
		std = V4L2_COLOR_STD_PAL;
		break;

	case  TVIN_SIG_FMT_CVBS_SECAM:
		std = V4L2_COLOR_STD_SECAM;
		break;
	default:
		pr_err("%s err fmt: 0x%x\n", __func__, fmt);
		break;
	}
	return std;
}

static v4l2_std_id demod_fmt_2_v4l2_std(int fmt)
{
	v4l2_std_id std = 0;
	switch (fmt) {
	case AML_ATV_DEMOD_VIDEO_MODE_PROP_PAL_DK:
		std = V4L2_STD_PAL_DK;
		break;
	case AML_ATV_DEMOD_VIDEO_MODE_PROP_PAL_I:
		std = V4L2_STD_PAL_I;
		break;
	case AML_ATV_DEMOD_VIDEO_MODE_PROP_PAL_BG:
		std = V4L2_STD_PAL_BG;
		break;
	case AML_ATV_DEMOD_VIDEO_MODE_PROP_PAL_M:
		std = V4L2_STD_PAL_M;
		break;
	case AML_ATV_DEMOD_VIDEO_MODE_PROP_NTSC_DK:
	case AML_ATV_DEMOD_VIDEO_MODE_PROP_NTSC_I:
	case AML_ATV_DEMOD_VIDEO_MODE_PROP_NTSC_BG:
	case AML_ATV_DEMOD_VIDEO_MODE_PROP_NTSC:
	case AML_ATV_DEMOD_VIDEO_MODE_PROP_NTSC_M:
		std = V4L2_STD_NTSC_M;
		break;
	case AML_ATV_DEMOD_VIDEO_MODE_PROP_SECAM_L:
		std = V4L2_STD_SECAM_L;
		break;
	case AML_ATV_DEMOD_VIDEO_MODE_PROP_SECAM_DK2:
	case AML_ATV_DEMOD_VIDEO_MODE_PROP_SECAM_DK3:
		std = V4L2_STD_SECAM_DK;
		break;
	default:
		pr_err("%s unsupport fmt:0x%0x !!!\n", __func__, fmt);
	}
	return std;
}

static struct aml_fe_drv **aml_get_fe_drv_list(enum aml_fe_dev_type_t type)
{
	switch (type) {
	case AM_DEV_TUNER:
		return &tuner_drv_list;
	case AM_DEV_ATV_DEMOD:
		return &atv_demod_drv_list;
	case AM_DEV_DTV_DEMOD:
		return &dtv_demod_drv_list;
	default:
		return NULL;
	}
}

int aml_register_fe_drv(enum aml_fe_dev_type_t type, struct aml_fe_drv *drv)
{
	if (drv) {
		struct aml_fe_drv **list = aml_get_fe_drv_list(type);
		unsigned long flags;

		spin_lock_irqsave(&lock, flags);

		drv->next = *list;
		*list = drv;

		drv->ref = 0;

		spin_unlock_irqrestore(&lock, flags);
	}

	return 0;
}
EXPORT_SYMBOL(aml_register_fe_drv);

int aml_unregister_fe_drv(enum aml_fe_dev_type_t type, struct aml_fe_drv *drv)
{
	int ret = 0;

	if (drv) {
		struct aml_fe_drv *pdrv, *pprev;
		struct aml_fe_drv **list = aml_get_fe_drv_list(type);
		unsigned long flags;

		spin_lock_irqsave(&lock, flags);

		if (!drv->ref) {
			for (pprev = NULL, pdrv = *list;
			     pdrv; pprev = pdrv, pdrv = pdrv->next) {
				if (pdrv == drv) {
					if (pprev)
						pprev->next = pdrv->next;
					else
						*list = pdrv->next;
					break;
				}
			}
		} else {
			pr_error("fe driver %d is inused\n", drv->id);
			ret = -1;
		}

		spin_unlock_irqrestore(&lock, flags);
	}

	return ret;
}
EXPORT_SYMBOL(aml_unregister_fe_drv);

struct dvb_frontend *get_si2177_tuner(void)
{
	int i;
	struct aml_fe_dev *dev;

	for (i = 0; i < FE_DEV_COUNT; i++) {
		dev = &fe_man.tuner[i];
		if (dev == NULL || dev->drv == NULL || dev->fe == NULL)
			continue;
#if (defined CONFIG_AM_SI2177)
		if (!strcmp(dev->drv->name, "si2177_tuner"))
			return dev->fe->fe;
#elif (defined CONFIG_AM_MXL661)
		if (!strcmp(dev->drv->name, "mxl661_tuner"))
			return dev->fe->fe;
#elif (defined CONFIG_AM_R840)
		if (!strcmp(dev->drv->name, "r840_tuner"))
			return dev->fe->fe;
#else
#endif
		if (!strcmp(dev->drv->name, "r842_tuner"))
			return dev->fe->fe;
		return dev->fe->fe;
	}
	pr_error("can not find out tuner drv\n");
	return NULL;
}
EXPORT_SYMBOL(get_si2177_tuner);

void set_aft_thread_enable(int enable, u32_t delay)
{
	aft_thread_enable = enable;
	aft_thread_delaycnt = delay;
}

static void aml_fe_do_work(struct work_struct *work)
{
	struct aml_dvb *dvb = aml_get_dvb_device();
	struct dvb_frontend *fe = dvb->fe;
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;
	int afc = 100;
	int lock = 0;
	int tmp = 0;
	static int audio_overmodul;
	static int afc_wave_cnt;
	static int afc_wrong_lock;
	struct aml_fe *fee;
	fee = fe->demodulator_priv;

	retrieve_vpll_carrier_lock(&tmp);/* 0 means lock, 1 means unlock */
	lock = !tmp;
	if (lock) {
		if (0 == ((audio_overmodul++)%10))
			aml_audio_overmodulation(1);
	}

	retrieve_frequency_offset(&afc);
	if (debug_fe & 0x4)
		pr_err("%s,afc raw val:%d,afc:%d lock:%d\n", __func__,
				afc, afc*488/1000, lock);
	afc = afc*488/1000;
	if (lock && (abs(afc) < AFC_BEST_LOCK)) {
		if (debug_fe & 0x4)
			pr_err("%s,afc lock, set wave_cnt 0\n", __func__);
		/* afc_wave_cnt = 0; */
		afc_wrong_lock = 0;
		return;
	} else {
		afc_wave_cnt++;
		if (!lock && (abs(afc) < AFC_BEST_LOCK))
			afc_wrong_lock++;
		else
			afc_wrong_lock = 0;
		if (debug_fe & 0x4)
			pr_err("%s afc_wrong_lock:%d\n",
				__func__, afc_wrong_lock);
	}
	if (afc_wave_cnt < 15) {
		if (debug_fe & 0x1)
			pr_err("%s,afc:%d is wave,lock:%d ignore\n",
				__func__, afc, lock);
		return;
	}
	if (14 == (afc_wrong_lock % 15)) {
		c->frequency -= afc_offset*1000;
		c->frequency += 1;
		if (debug_fe & 0x4)
			pr_err("%s,afc is ok,but unlock, set freq:%d\n",
					__func__, c->frequency);
		if (fe->ops.tuner_ops.set_params)
			fe->ops.tuner_ops.set_params(fe);

		afc_offset = 0;
		return;

	}
	if (afc_wrong_lock > 0)
		return;
	if (abs(afc_offset) >= 2000) {
		no_sig_cnt++;
		if (no_sig_cnt == 20) {
			c->frequency -= afc_offset*1000;
			if (debug_fe & 0x4)
				pr_err("%s,afc no_sig trig, set freq:%d\n",
						__func__, c->frequency);
			if (fe->ops.tuner_ops.set_params)
				fe->ops.tuner_ops.set_params(fe);
			afc_offset = 0;
		}
		return;
	}
	no_sig_cnt = 0;
	c->frequency += afc*1000;
	afc_offset += afc;
	if (debug_fe & 0x4)
		pr_err("%s,afc:%d , set freq:%d\n",
				__func__, afc, c->frequency);
	if (fe->ops.tuner_ops.set_params)
		fe->ops.tuner_ops.set_params(fe);
}

void aml_timer_hander(unsigned long arg)
{
	struct dvb_frontend *fe = (struct dvb_frontend *)arg;
	struct aml_dvb *dvb = aml_get_dvb_device();
	aml_timer.expires = jiffies + AML_INTERVAL*20;/* 100ms timer */
	add_timer(&aml_timer);
	if (!aft_thread_enable) {
		pr_info("%s, stop aft thread\n", __func__);
		return;
	}
	if (aft_thread_delaycnt > 0) {
		aft_thread_delaycnt--;
		return;
	}
	if ((aml_timer_en == 0) || (FE_ANALOG != fe->ops.info.type))
		return;

	dvb->fe = (struct dvb_frontend *)arg;
	schedule_work(&dvb->aml_fe_wq);
}

int aml_fe_analog_set_frontend(struct dvb_frontend *fe)
{
	struct aml_fe *afe = fe->demodulator_priv;
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;
	struct analog_parameters p;
	fe_status_t tuner_state = FE_TIMEDOUT;
	int ret = -1;
	struct aml_fe *fee;
	fee = fe->demodulator_priv;

	if (fe->ops.tuner_ops.set_params)
		ret = fe->ops.tuner_ops.set_params(fe);
	if (fe->ops.tuner_ops.get_status)
		fe->ops.tuner_ops.get_status(fe, &tuner_state);

	if (fee->tuner->drv->id == AM_TUNER_R840)
		p.tuner_id = AM_TUNER_R840;
	else if (fee->tuner->drv->id == AM_TUNER_SI2151)
		p.tuner_id = AM_TUNER_SI2151;
	else if (fee->tuner->drv->id == AM_TUNER_MXL661)
		p.tuner_id = AM_TUNER_MXL661;
	p.if_freq = fee->demod_param.if_freq;
	p.if_inv = fee->demod_param.if_inv;

	p.frequency  = c->frequency;
	p.soundsys   = c->analog.soundsys;
	p.audmode    = c->analog.audmode;
	p.std        = c->analog.std;
	p.reserved   = c->analog.reserved;

	/*set tuner&ademod such as philipse tuner */
	if (fe->ops.analog_ops.set_params) {
		fe->ops.analog_ops.set_params(fe, &p);
		ret = 0;
	}

	if (ret == 0) {
		afe->params.frequency = c->frequency;
		afe->params.inversion = c->inversion;
		afe->params.analog = c->analog;
	}
	/* afc tune */
	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_MG9TV) {
		if (aml_timer_en == 1) {
			if (timer_init_state == 1) {
				del_timer_sync(&aml_timer);
				timer_init_state = 0;
			}
		}

		if (aml_timer_en == 1 && aft_thread_enable) {
			init_timer(&aml_timer);
			aml_timer.function = aml_timer_hander;
			aml_timer.data = (ulong) fe;
			/* after 5s enable demod auto detect */
			aml_timer.expires = jiffies + AML_INTERVAL*500;
			afc_offset = 0;
			no_sig_cnt = 0;
			add_timer(&aml_timer);
			timer_init_state = 1;
		}
	}

	return ret;
}
EXPORT_SYMBOL(aml_fe_analog_set_frontend);

static int aml_fe_analog_get_frontend(struct dvb_frontend *fe)
{
	struct dtv_frontend_properties *p = &fe->dtv_property_cache;
	struct aml_fe *afe = fe->demodulator_priv;
	int audio = 0;
	v4l2_std_id std_bk = 0;
	int varify_cnt = 0, i = 0;
	fe_status_t tuner_state = FE_TIMEDOUT;
	fe_status_t ade_state = FE_TIMEDOUT;

	p->frequency = afe->params.frequency;


	if (video_mode_manul) {
		fe->ops.tuner_ops.get_pll_status(fe, &tuner_state);
		fe->ops.analog_ops.get_pll_status(fe, &ade_state);
		if ((FE_HAS_LOCK == ade_state) ||
				(FE_HAS_LOCK == tuner_state)) {
			for (i = 0; i < 100; i++) {
				if (aml_fe_hook_get_fmt == NULL)
					break;
				std_bk = aml_fe_hook_get_fmt();
				if (std_bk)
					varify_cnt++;
				if (varify_cnt > 3)
					break;
				msleep(20);
			}
			if (std_bk == 0) {
				pr_err("%s, failed to get v fmt\n",
						__func__);
				p->analog.std &= 0x00ffffff;
				p->analog.std |= V4L2_COLOR_STD_PAL;
			} else {
				p->analog.std &= 0x00ffffff;
				p->analog.std |=
					trans_tvin_fmt_to_v4l2_std(std_bk);
				pr_err("%s,frequency:%d,std_bk:0x%x,std:0x%x\n",
					__func__, p->frequency,
					(unsigned int)std_bk,
					(unsigned int)p->analog.std);
			}
		}
	}
	if (audio_mode_manul) {
		std_bk = p->analog.std & 0xff000000;
		if (std_bk == V4L2_COLOR_STD_NTSC) {
			audio = V4L2_STD_NTSC_M;
		} else if (std_bk == V4L2_COLOR_STD_SECAM) {
			audio = V4L2_STD_SECAM_L;
		} else {
			amlatvdemod_set_std(
				AML_ATV_DEMOD_VIDEO_MODE_PROP_PAL_DK);
			audio = aml_audiomode_autodet(fe);
			amlatvdemod_set_std(audio);
			audio = demod_fmt_2_v4l2_std(audio);
		}
		p->analog.audmode = audio;
		p->analog.std &= 0xff000000;
		p->analog.std |= audio;
		pr_err("[%s] params.frequency:%d, audio:0x%0x, vfmt:0x%x\n",
			__func__, p->frequency, (unsigned int)p->analog.audmode,
			(unsigned int)p->analog.std);
	}
	return 0;
}

static int aml_fe_analog_sync_frontend(struct dvb_frontend *fe)
{
	struct dtv_frontend_properties *p = &fe->dtv_property_cache;
	struct aml_fe *afe = fe->demodulator_priv;

	afe->params.frequency = p->frequency;
	afe->params.inversion = p->inversion;
	afe->params.analog = p->analog;

	return 0;
}

static int aml_fe_analog_read_status(struct dvb_frontend *fe,
				     fe_status_t *status)
{
	int ret = 0;

	if (!status)
		return -1;
	/*atv only demod locked is vaild */
	if (fe->ops.analog_ops.get_status)
		fe->ops.analog_ops.get_status(fe, status);
	else if (fe->ops.tuner_ops.get_status)
		ret = fe->ops.tuner_ops.get_status(fe, status);

	return ret;
}

static int aml_fe_analog_read_signal_strength(struct dvb_frontend *fe,
					      u16 *strength)
{
	int ret = -1;
	u16 s;

	s = 0;
	if (fe->ops.analog_ops.has_signal) {
		fe->ops.analog_ops.has_signal(fe, &s);
		*strength = s;
		ret = 0;
	} else if (fe->ops.tuner_ops.get_rf_strength) {
		ret = fe->ops.tuner_ops.get_rf_strength(fe, strength);
	}

	return ret;
}

static int aml_fe_analog_read_signal_snr(struct dvb_frontend *fe, u16 *snr)
{
	if (!snr) {
		pr_error("[aml_fe..]%s null pointer error.\n", __func__);
		return -1;
	}
	if (fe->ops.analog_ops.get_snr)
		*snr = (unsigned short)fe->ops.analog_ops.get_snr(fe);
	return 0;
}

static enum dvbfe_algo aml_fe_get_analog_algo(struct dvb_frontend *dev)
{
	return DVBFE_ALGO_CUSTOM;
}



/*this func set two ways to search the channel*/
/*if the afc_range>1Mhz,set the freq  more than once*/
/*if the afc_range<=1MHz,set the freq only once ,on the mid freq*/
static enum dvbfe_search aml_fe_analog_search(struct dvb_frontend *fe)
{
	struct dtv_frontend_properties *p = &fe->dtv_property_cache;
	fe_status_t tuner_state = FE_TIMEDOUT;
	fe_status_t ade_state = FE_TIMEDOUT;
	struct atv_status_s atv_status;
	__u32 set_freq = 0;
	__u32 minafcfreq, maxafcfreq;
	__u32 frist_step;
	__u32 afc_step;
	int tuner_status_cnt_local = tuner_status_cnt;
	int atv_cvd_format, hv_lock_status, snr_vale;
	v4l2_std_id std_bk = 0;
	struct aml_fe *fee;
	int audio = 0;
	int try_ntsc = 0, get_vfmt_maxcnt = 50;
	int varify_cnt = 0, i = 0;
	int double_check_cnt = 1;

#ifdef DEBUG_TIME_CUS
	unsigned int time_start, time_end, time_delta;
	time_start = jiffies_to_msecs(jiffies);
#endif
	fee = fe->demodulator_priv;
	if ((fe == NULL) || (p == NULL) || (fee == NULL))
		return DVBFE_ALGO_SEARCH_FAILED;
	atv_cvd_format = 0;
	hv_lock_status = 0;
	snr_vale = 0;
	pr_dbg("[%s],afc_range=%d,flag=0x%x[1->auto,11->mannul], freq=[%d]\n",
	     __func__, p->analog.afc_range, p->analog.flag, p->frequency);
	pr_dbg("the tuner type is [%d]\n", fee->tuner->drv->id);
	/* backup the freq by api */
	set_freq = p->frequency;

	if (p->analog.std == 0) {
		p->analog.std = (V4L2_COLOR_STD_NTSC | V4L2_STD_NTSC_M);
		pr_dbg("%s, user analog.std is 0, so set it to NTSC | M\n",
			__func__);
	}
	if (p->analog.afc_range == 0) {
		pr_dbg("[%s]:afc_range==0,skip the search\n", __func__);
		return DVBFE_ALGO_SEARCH_FAILED;
	}
/*set the frist_step*/
	if (p->analog.afc_range > ATV_AFC_1_0MHZ)
		frist_step = ATV_AFC_1_0MHZ;
	else
		frist_step = p->analog.afc_range;
/*set the afc_range and start freq*/
	minafcfreq = p->frequency - p->analog.afc_range;
	maxafcfreq = p->frequency + p->analog.afc_range;
/*from the min freq start,and set the afc_step*/
	/*if step is 2Mhz,r840 will miss program*/
	if (slow_mode || (fee->tuner->drv->id == AM_TUNER_R840)) {
		pr_dbg("[%s]this is slow mode to search the channel\n",
		       __func__);
		p->frequency = minafcfreq;
		afc_step = ATV_AFC_1_0MHZ;
	} else if (!slow_mode) {
		p->frequency = minafcfreq;
		afc_step = ATV_AFC_2_0MHZ;
	} else {
		pr_dbg("[%s]unknown tuner type, slow_mode search the channel\n",
			__func__);
		p->frequency = minafcfreq;
		afc_step = ATV_AFC_1_0MHZ;
	}

	/**enter manual search mode**/
	if (p->analog.flag == ANALOG_FLAG_MANUL_SCAN) {
		/*manul search force to ntsc_m */
		std_bk = p->analog.std;
		pr_dbg("%s Manully user analog.std:0x%08x\n",
			__func__, (uint32_t)std_bk);
		if (get_cpu_type() < MESON_CPU_MAJOR_ID_MG9TV)
			p->analog.std = (V4L2_COLOR_STD_NTSC | V4L2_STD_NTSC_M);

		if (fe->ops.set_frontend(fe)) {
			pr_error("[%s]the func of set_param err.\n", __func__);
			p->analog.std = std_bk;
			fe->ops.set_frontend(fe);
			std_bk = 0;
			return DVBFE_ALGO_SEARCH_FAILED;
		}

		/*delete it will be not get program*/
		if (fee->tuner->drv->id == AM_TUNER_MXL661)
			usleep_range((delay_cnt+20)*1000,
				(delay_cnt+20)*1000+100);
		else
			usleep_range(delay_cnt*1000, delay_cnt*1000+100);

		if (get_cpu_type() >= MESON_CPU_MAJOR_ID_MG9TV) {
			if ((fe->ops.tuner_ops.get_pll_status == NULL) ||
			    (fe->ops.analog_ops.get_pll_status == NULL)) {
				pr_info("[%s]error:the func of get_pll_status is NULL.\n",
					__func__);
				return DVBFE_ALGO_SEARCH_FAILED;
			}
			fe->ops.tuner_ops.get_pll_status(fe, &tuner_state);
			fe->ops.analog_ops.get_pll_status(fe, &ade_state);
		} else {
			if ((fe->ops.tuner_ops.get_status == NULL) ||
			    (fe->ops.analog_ops.get_status == NULL)) {
				pr_info("[%s]error:the func of get_status is NULL.\n",
					__func__);
				return DVBFE_ALGO_SEARCH_FAILED;
			}
			fe->ops.tuner_ops.get_status(fe, &tuner_state);
			fe->ops.analog_ops.get_status(fe, &ade_state);
		}
		if (((FE_HAS_LOCK == ade_state ||
		      FE_HAS_LOCK == tuner_state) &&
		     (fee->tuner->drv->id != AM_TUNER_R840)) ||
		    ((FE_HAS_LOCK == ade_state &&
		      FE_HAS_LOCK == tuner_state) &&
		     (fee->tuner->drv->id == AM_TUNER_R840))) {
			if (debug_fe & 0x1)
				pr_err("[%s][%d]freq:%d pll lock success\n",
					__func__, __LINE__, p->frequency);
			if (fee->tuner->drv->id == AM_TUNER_MXL661) {
				fe->ops.analog_ops.get_atv_status(fe,
					&atv_status);
				if (atv_status.atv_lock)
					usleep_range(30*1000, 30*1000+100);
			}
			if (fee->tuner->drv->id == AM_TUNER_MXL661)
				usleep_range(40*1000, 40*1000+100);

			if (aml_fe_afc_closer(fe, p->frequency,
					      p->frequency + ATV_AFC_500KHZ, 1)
				== 0) {
				try_ntsc = 0;
				get_vfmt_maxcnt = 200;
				p->analog.std =
					(V4L2_COLOR_STD_PAL | V4L2_STD_PAL_I);
				p->frequency += 1;
				fe->ops.set_frontend(fe);
				usleep_range(20*1000, 20*1000+100);

			while (1) {
				for (i = 0; i < get_vfmt_maxcnt; i++) {
					if (aml_fe_hook_get_fmt == NULL)
						break;
					std_bk = aml_fe_hook_get_fmt();
					if (std_bk)
						varify_cnt++;
					if (varify_cnt > 3)
						break;
					if (i == (get_vfmt_maxcnt/3) ||
						(i == (get_vfmt_maxcnt/3)*2)) {
						p->analog.std =
							(V4L2_COLOR_STD_NTSC
							| V4L2_STD_NTSC_M);
						p->frequency += 1;
						fe->ops.set_frontend(fe);
					}
					usleep_range(30*1000, 30*1000+100);
				}
				if (std_bk == 0) {
					pr_err("%s, failed to get v fmt !!\n",
						__func__);
					if (try_ntsc > 0) {
						pr_err("%s,vfmt assume PAL!!\n",
							__func__);
						std_bk =
							TVIN_SIG_FMT_CVBS_PAL_I;
						break;
					} else {
						p->analog.std =
							(V4L2_COLOR_STD_NTSC
							| V4L2_STD_NTSC_M);
						p->frequency += 1;
						fe->ops.set_frontend(fe);
						usleep_range(20*1000,
							20*1000+100);
						try_ntsc++;
						continue;
					}
				}
				break;
			}
			if (try_ntsc) {
				p->analog.std =
					(V4L2_COLOR_STD_PAL | V4L2_STD_PAL_DK);
				p->frequency += 1;
				fe->ops.set_frontend(fe);
				usleep_range(20*1000, 20*1000+100);
			}
			std_bk = trans_tvin_fmt_to_v4l2_std(std_bk);
			if (std_bk == V4L2_COLOR_STD_NTSC) {
				amlatvdemod_set_std(
					AML_ATV_DEMOD_VIDEO_MODE_PROP_PAL_DK);
				audio = aml_audiomode_autodet(fe);
				audio = demod_fmt_2_v4l2_std(audio);
				if (audio == V4L2_STD_PAL_M)
					audio = V4L2_STD_NTSC_M;
				else
					std_bk = V4L2_COLOR_STD_PAL;
			} else if (std_bk == V4L2_COLOR_STD_SECAM) {
				audio = V4L2_STD_SECAM_L;
			} else {
				amlatvdemod_set_std(
					AML_ATV_DEMOD_VIDEO_MODE_PROP_PAL_DK);
				audio = aml_audiomode_autodet(fe);
				audio = demod_fmt_2_v4l2_std(audio);
				if (audio == V4L2_STD_PAL_M) {
					audio = demod_fmt_2_v4l2_std(
						broad_std_except_pal_m);
					pr_err("select best audio mode 0x%x\n",
						audio);
				}
			}
			pr_err("%s,Manual freq:%d: std_bk:0x%x ,audmode:0x%x\n",
				__func__, p->frequency,
				(unsigned int)std_bk, audio);
			if (std_bk != 0) {
				p->analog.audmode = audio;
				p->analog.std = std_bk | audio;
				/*avoid std unenable */
				p->frequency -= 1;
				std_bk = 0;
			}
#ifdef DEBUG_TIME_CUS
			time_end = jiffies_to_msecs(jiffies);
			time_delta = time_end - time_start;
			pr_dbg("[ATV_SEARCH_SUCCESS]%s: time_delta:%d ms\n",
				__func__, time_delta);
#endif
			/*sync param */
			aml_fe_analog_sync_frontend(fe);
			return DVBFE_ALGO_SEARCH_SUCCESS;

			}
		}
		usleep_range(20*1000, 20*1000+100);
		p->frequency += afc_step;
		fe->ops.set_frontend(fe);
		return DVBFE_ALGO_SEARCH_FAILED;
	}
	/**enter auto search mode**/
	pr_dbg("%s Autosearch user analog.std:0x%08x\n",
		__func__, (uint32_t)p->analog.std);
	if (fe->ops.set_frontend(fe)) {
		pr_error("[%s]the func of set_param err.\n", __func__);
		return DVBFE_ALGO_SEARCH_FAILED;
	}
#ifdef DEBUG_TIME_CUS
	time_end = jiffies_to_msecs(jiffies);
	time_delta = time_end - time_start;
	pr_dbg
	    ("[ATV_SEARCH_SET_FRONTEND]%s: time_delta_001:%d ms,afc_step:%d\n",
	     __func__, time_delta, afc_step);
#endif
/* atuo bettween afc range */
	if (unlikely(!fe->ops.tuner_ops.get_status ||
		     !fe->ops.analog_ops.get_status || !fe->ops.set_frontend)) {
		pr_error("[%s]error: NULL func.\n", __func__);
		return DVBFE_ALGO_SEARCH_FAILED;
	}
	while (p->frequency <= maxafcfreq) {
		if (debug_fe & 0x3)
			pr_err("[%s] p->frequency=[%d] is processing,maxafcfreq:[%d]\n",
				__func__, p->frequency, maxafcfreq);
		if (fee->tuner->drv->id != AM_TUNER_R840 &&
		fee->tuner->drv->id != AM_TUNER_MXL661 &&
		fee->tuner->drv->id != AM_TUNER_SI2151) {
			do {
				if (get_cpu_type() != MESON_CPU_MAJOR_ID_GXTVBB)
					usleep_range(delay_cnt*1000,
						delay_cnt*1000+100);
				if ((fe->ops.tuner_ops.get_pll_status == NULL)
				    ||
				    (fe->ops.analog_ops.get_pll_status ==
				     NULL)) {
					pr_info("[%s]error:the func of get_pll_status is NULL.\n",
						__func__);
					return DVBFE_ALGO_SEARCH_FAILED;
				}
				fe->ops.tuner_ops.get_pll_status(fe,
								 &tuner_state);
				fe->ops.analog_ops.get_pll_status(fe,
								  &ade_state);
				tuner_status_cnt_local--;
				if (FE_HAS_LOCK == ade_state ||
				    FE_HAS_LOCK == tuner_state ||
				    tuner_status_cnt_local == 0)
					break;
			} while (1);
			tuner_status_cnt_local = tuner_status_cnt;
			if (FE_HAS_LOCK == ade_state ||
			    FE_HAS_LOCK == tuner_state) {
				pr_dbg("[%s] pll lock success\n", __func__);
				do {
					tuner_status_cnt_local--;
					/*tvafe_cvd2_get_atv_format(); */
					if (aml_fe_hook_atv_status != NULL)
						atv_cvd_format =
					    aml_fe_hook_atv_status();
					/*tvafe_cvd2_get_hv_lock(); */
					if (aml_fe_hook_hv_lock != NULL)
						hv_lock_status =
						aml_fe_hook_hv_lock();
					if (fe->ops.analog_ops.get_snr != NULL)
						snr_vale =
					    fe->ops.analog_ops.get_snr(fe);

					pr_dbg("[%s] atv_cvd_format:0x%x;"
						"hv_lock_status:0x%x;"
						"snr_vale:%d, v fmt:0x%x\n",
						__func__, atv_cvd_format,
						hv_lock_status, snr_vale,
						(unsigned int)std_bk);
					if (((atv_cvd_format & 0x4) == 0)
					    || ((hv_lock_status == 0x4)
						&& (snr_vale < 10))) {
						std_bk = p->analog.std
							 & 0xff000000;
						audio =
						aml_audiomode_autodet(fe);
						p->analog.std = std_bk |
						demod_fmt_2_v4l2_std(audio);
						p->analog.audmode =
						demod_fmt_2_v4l2_std(audio);
						/*avoid std unenable */
						p->frequency += 1;
						pr_dbg("[%s] maybe ntsc m\n",
						       __func__);
						break;
					}
					if (tuner_status_cnt_local == 0)
						break;
				if (get_cpu_type() != MESON_CPU_MAJOR_ID_GXTVBB)
					usleep_range(delay_cnt*1000,
						delay_cnt*1000+100);
				} while (1);
			}
			if (tuner_status_cnt_local != 0) {
				if (fe->ops.set_frontend(fe)) {
					pr_info("[%s] the func of set_frontend err.\n",
						__func__);
					return DVBFE_ALGO_SEARCH_FAILED;
				}
			}
		}
		tuner_status_cnt_local = tuner_status_cnt;
		do {
			if (fee->tuner->drv->id == AM_TUNER_MXL661)
				usleep_range((delay_cnt+20)*1000,
					(delay_cnt+20)*1000+100);
/*			if (fee->tuner->drv->id == AM_TUNER_R840)
				usleep_range(delay_cnt*1000,
					 delay_cnt*1000+100);
*/
			if (get_cpu_type() >= MESON_CPU_MAJOR_ID_MG9TV) {
				if ((fe->ops.tuner_ops.get_pll_status == NULL)
				    ||
				    (fe->ops.analog_ops.get_pll_status ==
				     NULL)) {
					pr_info("[%s]error:the func of get_pll_status is NULL.\n",
						__func__);
					return DVBFE_ALGO_SEARCH_FAILED;
				}
				fe->ops.tuner_ops.get_pll_status(fe,
								 &tuner_state);
				fe->ops.analog_ops.get_pll_status(fe,
								  &ade_state);
			} else {
				if ((fe->ops.tuner_ops.get_status == NULL)
				    ||
				    (fe->ops.analog_ops.get_status ==
				     NULL)) {
					pr_info("[%s]error:the func of get_status is NULL.\n",
						__func__);
					return DVBFE_ALGO_SEARCH_FAILED;
				}
				fe->ops.tuner_ops.get_status(fe, &tuner_state);
				fe->ops.analog_ops.get_status(fe, &ade_state);
			}
			tuner_status_cnt_local--;
			if (((FE_HAS_LOCK == ade_state ||
			      FE_HAS_LOCK == tuner_state) &&
			     (fee->tuner->drv->id != AM_TUNER_R840)) ||
			    ((FE_HAS_LOCK == ade_state &&
			      FE_HAS_LOCK == tuner_state) &&
			     (fee->tuner->drv->id == AM_TUNER_R840)) ||
			    (tuner_status_cnt_local == 0))
				break;
		} while (1);
		tuner_status_cnt_local = tuner_status_cnt;
		if (((FE_HAS_LOCK == ade_state ||
		      FE_HAS_LOCK == tuner_state) &&
		     (fee->tuner->drv->id != AM_TUNER_R840)) ||
		    ((FE_HAS_LOCK == ade_state &&
		      FE_HAS_LOCK == tuner_state) &&
		     (fee->tuner->drv->id == AM_TUNER_R840))) {
			if (debug_fe & 0x1)
				pr_err("[%s][%d]freq:%d pll lock success\n",
			       __func__, __LINE__, p->frequency);
			if (fee->tuner->drv->id == AM_TUNER_MXL661) {
				fe->ops.analog_ops.get_atv_status(fe,
					&atv_status);
				if (atv_status.atv_lock)
					usleep_range(30*1000, 30*1000+100);
			}
			if (aml_fe_afc_closer(fe, minafcfreq,
				maxafcfreq + ATV_AFC_500KHZ, 1) == 0) {
				try_ntsc = 0;
				get_vfmt_maxcnt = 200;
			while (1) {
				for (i = 0; i < get_vfmt_maxcnt; i++) {
					if (aml_fe_hook_get_fmt == NULL)
						break;
					std_bk = aml_fe_hook_get_fmt();
					if (std_bk)
						varify_cnt++;
				if (fee->tuner->drv->id == AM_TUNER_R840) {
					if (varify_cnt > 0)
						break;
				}
					if (varify_cnt > 3)
						break;
					if (i == (get_vfmt_maxcnt/3) ||
						(i == (get_vfmt_maxcnt/3)*2)) {
						p->analog.std =
							(V4L2_COLOR_STD_NTSC
							| V4L2_STD_NTSC_M);
						p->frequency += 1;
						fe->ops.set_frontend(fe);
					}
					usleep_range(30*1000, 30*1000+100);
				}
				if (debug_fe & 0x2)
					pr_err("get std_bk cnt:%d\n", i);

				if (std_bk == 0) {
					pr_err("%s, failed to get v fmt !!\n",
						__func__);
					if (try_ntsc > 0) {
						pr_err("%s,vfmt assume PAL!!\n",
							__func__);
						std_bk =
							TVIN_SIG_FMT_CVBS_PAL_I;
						break;
					} else {
						p->analog.std =
							(V4L2_COLOR_STD_NTSC
							| V4L2_STD_NTSC_M);
						p->frequency += 1;
						fe->ops.set_frontend(fe);
						usleep_range(20*1000,
							20*1000+100);
						try_ntsc++;
						continue;
					}
				}
				break;
			}
			if (try_ntsc) {
				p->analog.std = (V4L2_COLOR_STD_PAL
					| V4L2_STD_PAL_DK);
				p->frequency += 1;
				fe->ops.set_frontend(fe);
				usleep_range(20*1000, 20*1000+100);
			}
			std_bk = trans_tvin_fmt_to_v4l2_std(std_bk);

			if (std_bk == V4L2_COLOR_STD_NTSC) {
				amlatvdemod_set_std(
					AML_ATV_DEMOD_VIDEO_MODE_PROP_PAL_DK);
				audio = aml_audiomode_autodet(fe);
				audio = demod_fmt_2_v4l2_std(audio);
				if (audio == V4L2_STD_PAL_M)
					audio = V4L2_STD_NTSC_M;
				else
					std_bk = V4L2_COLOR_STD_PAL;
			} else if (std_bk == V4L2_COLOR_STD_SECAM) {
				audio = V4L2_STD_SECAM_L;
			} else {
				amlatvdemod_set_std(
					AML_ATV_DEMOD_VIDEO_MODE_PROP_PAL_DK);
				audio = aml_audiomode_autodet(fe);
				audio = demod_fmt_2_v4l2_std(audio);
				if (audio == V4L2_STD_PAL_M) {
					audio = demod_fmt_2_v4l2_std(
						broad_std_except_pal_m);
					pr_err("select the audio mode 0x%x\n",
						audio);
				}
			}
			pr_err("%s,Auto search freq:%d: std_bk:0x%x ,audmode:0x%x\n",
					__func__, p->frequency,
					(unsigned int)std_bk, audio);
				if (std_bk != 0) {
					p->analog.audmode = audio;
					p->analog.std = std_bk | audio;
					/*avoid std unenable */
					p->frequency -= 1;
					std_bk = 0;
				}
#ifdef DEBUG_TIME_CUS
				time_end = jiffies_to_msecs(jiffies);
				time_delta = time_end - time_start;
				pr_dbg("[ATV_SEARCH_SUCCESS]%s: time_delta:%d ms\n",
					__func__, time_delta);
#endif
				/*sync param */
				aml_fe_analog_sync_frontend(fe);
				return DVBFE_ALGO_SEARCH_SUCCESS;
			}
		}
		/*avoid sound format is not match after search over */
		if (std_bk != 0) {
			p->analog.std = std_bk;
			fe->ops.set_frontend(fe);
			std_bk = 0;
		}
		pr_dbg("[%s] freq[analog.std:0x%08x] is[%d] unlock\n",
			__func__, (uint32_t)p->analog.std, p->frequency);
		if (p->frequency >= 44200000 && p->frequency <= 44300000
				&& double_check_cnt) {
			double_check_cnt--;
			p->frequency -= afc_step;
			pr_err("%s 44.25Mhz double check\n", __func__);
		} else
			p->frequency += afc_step;
		if (p->frequency > maxafcfreq) {
			pr_dbg("[%s] p->frequency=[%d] over maxafcfreq=[%d].search failed.\n",
				__func__, p->frequency, maxafcfreq);
			/*back  original freq  to api */
			p->frequency = set_freq;
			fe->ops.set_frontend(fe);
#ifdef DEBUG_TIME_CUS
			time_end = jiffies_to_msecs(jiffies);
			time_delta = time_end - time_start;
			pr_dbg("[ATV_SEARCH_FAILED]%s: time_delta:%d ms\n",
			       __func__, time_delta);
#endif
			return DVBFE_ALGO_SEARCH_FAILED;
		}
		fe->ops.set_frontend(fe);
	}
#ifdef DEBUG_TIME_CUS
	time_end = jiffies_to_msecs(jiffies);
	time_delta = time_end - time_start;
	pr_dbg("[ATV_SEARCH_FAILED]%s: time_delta:%d ms\n",
	       __func__, time_delta);
#endif
	return DVBFE_ALGO_SEARCH_FAILED;
}

static int aml_fe_afc_closer(struct dvb_frontend *fe, int minafcfreq,
			     int maxafcfreq, int isAutoSearch)
{
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;
	int afc = 100;
	__u32 set_freq;
	int count = 25;
	int lock_cnt = 0;
	struct aml_fe *fee;
	static int freq_success;
	static int temp_freq, temp_afc;
	struct timespec time_now;
	static struct timespec success_time;
	fee = fe->demodulator_priv;
	if (debug_fe & 0x2)
		pr_err("%s: freq_success:%d,freq:%d,minfreq:%d,maxfreq:%d\n",
			__func__, freq_success, c->frequency,
			minafcfreq, maxafcfreq);

	/* avoid more search the same program, except < 45.00Mhz */
	if (abs(c->frequency - freq_success) < 3000000
			&& c->frequency > 45000000) {
		ktime_get_ts(&time_now);
		if (debug_fe & 0x2)
			pr_err("%s: tv_sec now:%ld,tv_sec success:%ld\n",
				__func__, time_now.tv_sec, success_time.tv_sec);
		/* beyond 10s search same frequency is ok */
		if ((time_now.tv_sec - success_time.tv_sec) < 10)
			return -1;
	}
	/*do the auto afc make sure the afc<50k or the range from api */
	if ((fe->ops.analog_ops.get_afc || fe->ops.tuner_ops.get_afc) &&
	    fe->ops.set_frontend) {
		/*
			delete it will miss program
			when c->frequency equal program frequency
		*/
		c->frequency++;
		if (fe->ops.tuner_ops.set_params)
			fe->ops.tuner_ops.set_params(fe);
		if (fee->tuner->drv->id == AM_TUNER_SI2151
				|| fee->tuner->drv->id == AM_TUNER_R840)
			usleep_range(10*1000, 10*1000+100);
		else if (fee->tuner->drv->id == AM_TUNER_MXL661)
			usleep_range(30*1000, 30*1000+100);
		/*****************************/
		set_freq = c->frequency;
		while (abs(afc) > AFC_BEST_LOCK) {
			if ((fe->ops.analog_ops.get_afc) &&
			    ((fee->tuner->drv->id == AM_TUNER_R840) ||
			     (fee->tuner->drv->id == AM_TUNER_SI2151) ||
			     (fee->tuner->drv->id == AM_TUNER_MXL661)))
				fe->ops.analog_ops.get_afc(fe, &afc);
			else if (fe->ops.tuner_ops.get_afc)
				fe->ops.tuner_ops.get_afc(fe, &afc);

			if (afc == 0xffff) {
				/*last lock, but this unlock,so try get afc*/
				if (lock_cnt > 0) {
					c->frequency =
						temp_freq + temp_afc*1000;
					if (debug_fe & 0x2)
						pr_err("%s,force lock,f:%d\n",
							__func__, c->frequency);
					freq_success = c->frequency;
					ktime_get_ts(&success_time);
					return 0;
				} else
					afc = 500;
			} else {
				lock_cnt++;
				temp_freq = c->frequency;
				if (afc > 50)
					temp_afc = 500;
				else if (afc < -50)
					temp_afc = -500;
				else
					temp_afc = afc;
			}

			if (((abs(afc) > (500 - AFC_BEST_LOCK))
				&& (abs(afc) < (500 + AFC_BEST_LOCK))
				&& (abs(afc) != 500))
				|| ((abs(afc) == 500) && (lock_cnt > 0))) {
				c->frequency += afc * 1000;
				break;
			}

			if (afc >= (500 + AFC_BEST_LOCK))
				afc = 500;

			c->frequency += afc * 1000;

			if (unlikely(c->frequency > maxafcfreq)) {
				if (debug_fe & 0x2)
					pr_err("[%s]:[%d] is exceed maxafcfreq[%d]\n",
						__func__, c->frequency,
						maxafcfreq);
				c->frequency = set_freq;
				return -1;
			}
			#if 0 /*if enable ,it would miss program*/
			if (unlikely(c->frequency < minafcfreq)) {
				pr_dbg("[%s]:[%d ] is exceed minafcfreq[%d]\n",
				       __func__, c->frequency, minafcfreq);
				c->frequency = set_freq;
				return -1;
			}
			#endif
			if (likely(!(count--))) {
				if (debug_fe & 0x2)
					pr_err("[%s]:exceed the afc count\n",
						__func__);
				c->frequency = set_freq;
				return -1;
			}

			/* afc tune */
			if (get_cpu_type() >= MESON_CPU_MAJOR_ID_MG9TV) {
				if (aml_timer_en == 1) {
					if (timer_init_state == 1) {
						del_timer_sync(&aml_timer);
						timer_init_state = 0;
					}
				}

				if (aml_timer_en == 1 && aft_thread_enable) {
					init_timer(&aml_timer);
					aml_timer.function = aml_timer_hander;
					aml_timer.data = (ulong) fe;
					/* after 5s enable demod auto detect*/
					aml_timer.expires =
						jiffies + AML_INTERVAL*500;
					afc_offset = 0;
					no_sig_cnt = 0;
					add_timer(&aml_timer);
					timer_init_state = 1;
				}
			}
			c->frequency++;
			if (fe->ops.tuner_ops.set_params)
				fe->ops.tuner_ops.set_params(fe);

			/*delete it will miss program*/
			if (fee->tuner->drv->id == AM_TUNER_MXL661)
				usleep_range(30*1000, 30*1000+100);
			else
				usleep_range(10*1000, 10*1000+100);

			if (debug_fe & 0x2)
				pr_err("[aml_fe..]%s get afc %d khz, freq %u.\n",
					__func__, afc, c->frequency);
		}
		freq_success = c->frequency;
		ktime_get_ts(&success_time);
		if (debug_fe & 0x2)
			pr_err("[aml_fe..]%s get afc %d khz done, freq %u.\n",
				__func__, afc, c->frequency);
	}
	return 0;
}

static int aml_fe_set_mode(struct dvb_frontend *dev, fe_type_t type)
{
	struct aml_fe *fe;
	enum aml_fe_mode_t mode;
	unsigned long flags;
	int ret = 0;

	fe = dev->demodulator_priv;
	/*type=FE_ATSC; */
	switch (type) {
	case FE_QPSK:
		mode = AM_FE_QPSK;
		pr_dbg("set mode -> QPSK\n");
		break;
	case FE_QAM:
		pr_dbg("set mode -> QAM\n");
		mode = AM_FE_QAM;
		break;
	case FE_OFDM:
		pr_dbg("set mode -> OFDM\n");
		mode = AM_FE_OFDM;
		break;
	case FE_ATSC:
		pr_dbg("set mode -> ATSC\n");
		mode = AM_FE_ATSC;
		break;
	case FE_ISDBT:
		pr_dbg("set mode -> ISDBT\n");
		mode = AM_FE_ISDBT;
		break;
	case FE_DTMB:
		pr_dbg("set mode -> DTMB\n");
		mode = AM_FE_DTMB;
		break;
	case FE_ANALOG:
		pr_dbg("set mode -> ANALOG\n");
		mode = AM_FE_ANALOG;
		break;
	default:
		pr_error("illegal fe type %d\n", type);
		return -1;
	}

	if (fe->mode == mode) {
		pr_dbg("[%s]:the mode is not change!!!!\n", __func__);
		return 0;
	}

	if (fe->mode != AM_FE_UNKNOWN) {
		pr_dbg("leave mode %d\n", fe->mode);

		if (fe->dtv_demod && (fe->dtv_demod->drv->capability & fe->mode)
		    && fe->dtv_demod->drv->leave_mode)
			fe->dtv_demod->drv->leave_mode(fe, fe->mode);
		if (fe->atv_demod && (fe->atv_demod->drv->capability & fe->mode)
		    && fe->atv_demod->drv->leave_mode)
			fe->atv_demod->drv->leave_mode(fe, fe->mode);
		if (fe->tuner && (fe->tuner->drv->capability & fe->mode)
		    && fe->tuner->drv->leave_mode)
			fe->tuner->drv->leave_mode(fe, fe->mode);

		if (fe->mode & AM_FE_DTV_MASK)
			aml_dmx_register_frontend(fe->ts, NULL);

		fe->mode = AM_FE_UNKNOWN;
	}

	if (!(mode & fe->capability)) {
		int i;

		spin_lock_irqsave(&lock, flags);
		for (i = 0; i < FE_DEV_COUNT; i++) {
			if ((mode & fe_man.fe[i].capability)
			    && (fe_man.fe[i].dev_id == fe->dev_id))
				break;
		}
		spin_unlock_irqrestore(&lock, flags);

		if (i >= FE_DEV_COUNT) {
			pr_error
			    ("frend %p don't support mode %x, capability %x\n",
			     fe, mode, fe->capability);
			return -1;
		}

		fe = &fe_man.fe[i];
		dev->demodulator_priv = fe;
	}

	if (fe->mode & AM_FE_DTV_MASK) {
		aml_dmx_register_frontend(fe->ts, NULL);
		fe->mode = 0;
	}

	spin_lock_irqsave(&fe->slock, flags);

	memset(&fe->fe->ops.tuner_ops, 0, sizeof(fe->fe->ops.tuner_ops));
	memset(&fe->fe->ops.analog_ops, 0, sizeof(fe->fe->ops.analog_ops));
	memset(&fe->fe->ops.info, 0, sizeof(fe->fe->ops.info));
	fe->fe->ops.release = NULL;
	fe->fe->ops.release_sec = NULL;
	fe->fe->ops.init = NULL;
	fe->fe->ops.sleep = NULL;
	fe->fe->ops.write = NULL;
	fe->fe->ops.tune = NULL;
	fe->fe->ops.get_frontend_algo = NULL;
	fe->fe->ops.set_frontend = NULL;
	fe->fe->ops.get_tune_settings = NULL;
	fe->fe->ops.get_frontend = NULL;
	fe->fe->ops.read_status = NULL;
	fe->fe->ops.read_ber = NULL;
	fe->fe->ops.read_signal_strength = NULL;
	fe->fe->ops.read_snr = NULL;
	fe->fe->ops.read_ucblocks = NULL;
	fe->fe->ops.set_qam_mode = NULL;
	fe->fe->ops.diseqc_reset_overload = NULL;
	fe->fe->ops.diseqc_send_master_cmd = NULL;
	fe->fe->ops.diseqc_recv_slave_reply = NULL;
	fe->fe->ops.diseqc_send_burst = NULL;
	fe->fe->ops.set_tone = NULL;
	fe->fe->ops.set_voltage = NULL;
	fe->fe->ops.enable_high_lnb_voltage = NULL;
	fe->fe->ops.dishnetwork_send_legacy_command = NULL;
	fe->fe->ops.i2c_gate_ctrl = NULL;
	fe->fe->ops.ts_bus_ctrl = NULL;
	fe->fe->ops.search = NULL;
	fe->fe->ops.track = NULL;
	fe->fe->ops.set_property = NULL;
	fe->fe->ops.get_property = NULL;
	memset(&fe->fe->ops.blindscan_ops, 0,
	       sizeof(fe->fe->ops.blindscan_ops));
	fe->fe->ops.asyncinfo.set_frontend_asyncenable = 0;
	if (fe->tuner && fe->tuner->drv && (mode & fe->tuner->drv->capability)
	    && fe->tuner->drv->get_ops)
		fe->tuner->drv->get_ops(fe->tuner, mode,
					&fe->fe->ops.tuner_ops);

	if (fe->atv_demod && fe->atv_demod->drv
	    && (mode & fe->atv_demod->drv->capability)
	    && fe->atv_demod->drv->get_ops) {
		fe->atv_demod->drv->get_ops(fe->atv_demod, mode,
					    &fe->fe->ops.analog_ops);
		fe->fe->ops.set_frontend = aml_fe_analog_set_frontend;
		fe->fe->ops.get_frontend = aml_fe_analog_get_frontend;
		fe->fe->ops.read_status = aml_fe_analog_read_status;
		fe->fe->ops.read_signal_strength =
		    aml_fe_analog_read_signal_strength;
		fe->fe->ops.read_snr = aml_fe_analog_read_signal_snr;
		fe->fe->ops.get_frontend_algo = aml_fe_get_analog_algo;
		fe->fe->ops.search = aml_fe_analog_search;
	}

	if (fe->dtv_demod && fe->dtv_demod->drv
	    && (mode & fe->dtv_demod->drv->capability)
	    && fe->dtv_demod->drv->get_ops)
		fe->dtv_demod->drv->get_ops(fe->dtv_demod, mode, &fe->fe->ops);

	spin_unlock_irqrestore(&fe->slock, flags);

	pr_dbg("enter mode %d\n", mode);

	if (fe->dtv_demod && (fe->dtv_demod->drv->capability & mode)
	    && fe->dtv_demod->drv->enter_mode)
		ret = fe->dtv_demod->drv->enter_mode(fe, mode);
	if (fe->atv_demod && (fe->atv_demod->drv->capability & mode)
	    && fe->atv_demod->drv->enter_mode)
		ret = fe->atv_demod->drv->enter_mode(fe, mode);
	if (fe->tuner && (fe->tuner->drv->capability & mode)
	    && fe->tuner->drv->enter_mode)
		ret = fe->tuner->drv->enter_mode(fe, mode);
	if (ret != 0) {
		pr_error("enter mode %d fail, ret %d\n", mode, ret);
		return ret;
	}

	pr_dbg("register demux frontend\n");
	if (mode & AM_FE_DTV_MASK)
		aml_dmx_register_frontend(fe->ts, fe->fe);
	strcpy(fe->fe->ops.info.name, "amlogic dvb frontend");

	fe->fe->ops.info.type = type;
	fe->mode = mode;

	pr_dbg("set mode ok\n");

	return 0;
}

static int aml_fe_read_ts(struct dvb_frontend *dev, int *ts)
{
	struct aml_fe *fe;

	fe = dev->demodulator_priv;

	*ts = fe->ts;
	return 0;
}

#ifndef CONFIG_OF
struct resource *aml_fe_platform_get_resource_byname(const char *name)
{
	int i;

	for (i = 0; i < aml_fe_num_resources; i++) {
		struct resource *r = &aml_fe_resource[i];

		if (!strcmp(r->name, name))
			return r;
	}
	return NULL;
}
#endif				/*CONFIG_OF */
#if (defined CONFIG_AM_DTVDEMOD)
static int rmem_demod_device_init(struct reserved_mem *rmem, struct device *dev)
{
	unsigned int demod_mem_start;
	unsigned int demod_mem_size;

	demod_mem_start = rmem->base;
	demod_mem_size = rmem->size;
	memstart = demod_mem_start;
	pr_info("demod reveser memory 0x%x, size %dMB.\n",
		demod_mem_start, (demod_mem_size >> 20));
	return 1;
}

static void rmem_demod_device_release(struct reserved_mem *rmem,
				      struct device *dev)
{
}

static const struct reserved_mem_ops rmem_demod_ops = {
	.device_init = rmem_demod_device_init,
	.device_release = rmem_demod_device_release,
};

static int __init rmem_demod_setup(struct reserved_mem *rmem)
{
	/*
	 * struct cma *cma;
	 * int err;
	 * pr_info("%s setup.\n",__func__);
	 * err = cma_init_reserved_mem(rmem->base, rmem->size, 0, &cma);
	 * if (err) {
	 *      pr_err("Reserved memory: unable to setup CMA region\n");
	 *      return err;
	 * }
	 */
	rmem->ops = &rmem_demod_ops;
	/* rmem->priv = cma; */

	pr_info
	    ("DTV demod reserved memory: %pa, size %ld MiB\n",
	     &rmem->base, (unsigned long)rmem->size / SZ_1M);

	return 0;
}

RESERVEDMEM_OF_DECLARE(demod, "amlogic, demod-mem", rmem_demod_setup);
#endif
static int aml_fe_dev_init(struct aml_dvb *dvb, struct platform_device *pdev,
			   enum aml_fe_dev_type_t type, struct aml_fe_dev *dev,
			   int id)
{
#ifndef CONFIG_OF
	struct resource *res;
#endif
	char *name = NULL;
	char buf[32];
	int ret;
	u32 value;
	const char *str;

	switch (type) {
	case AM_DEV_TUNER:
		name = "tuner";
		break;
	case AM_DEV_ATV_DEMOD:
		name = "atv_demod";
		break;
	case AM_DEV_DTV_DEMOD:
		name = "dtv_demod";
		break;
	default:
		break;
	}

	pr_dbg("init %s %d pdev: %p\n", name, id, pdev);

	snprintf(buf, sizeof(buf), "%s%d", name, id);
#ifdef CONFIG_OF
	ret = of_property_read_string(pdev->dev.of_node, buf, &str);
	if (ret) {
		pr_error("cannot find resource \"%s\"\n", buf);
		return 0;
	} else {
		struct aml_fe_drv **list = aml_get_fe_drv_list(type);
		struct aml_fe_drv *drv;
		unsigned long flags;

		spin_lock_irqsave(&lock, flags);

		for (drv = *list; drv; drv = drv->next)
			if (!strcmp(drv->name, str))
				break;

		if (dev->drv != drv) {
			if (dev->drv) {
				dev->drv->ref--;
				if (dev->drv->owner)
					module_put(dev->drv->owner);
			}
			if (drv) {
				drv->ref++;
				if (drv->owner)
					try_module_get(drv->owner);
			}
			dev->drv = drv;
		}

		spin_unlock_irqrestore(&lock, flags);

		if (drv) {
			pr_inf("found %s%d driver: %s\n", name, id, str);
		} else {
			pr_err("cannot find %s%d driver: %s\n", name, id, str);
			return -1;
		}
	}

#else				/*CONFIG_OF */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, buf);
	if (res) {
		struct aml_fe_drv **list = aml_get_fe_drv_list(type);
		struct aml_fe_drv *drv;
		int type = res->start;
		unsigned long flags;

		spin_lock_irqsave(&lock, flags);

		for (drv = *list; drv; drv = drv->next) {
			if (drv->id == type) {
				drv->ref++;
				if (drv->owner)
					try_module_get(drv->owner);
				break;
			}
		}

		spin_unlock_irqrestore(&lock, flags);

		if (drv) {
			dev->drv = drv;
		} else {
			pr_error("cannot find %s%d driver: %d\n", name, id,
				 type);
			return -1;
		}
	} else {
		pr_dbg("cannot find resource \"%s\"\n", buf);
		return 0;
	}
#endif				/*CONFIG_OF */

	snprintf(buf, sizeof(buf), "%s%d_i2c_adap_id", name, id);
#ifdef CONFIG_OF
	ret = of_property_read_u32(pdev->dev.of_node, buf, &value);
	if (!ret) {
		dev->i2c_adap_id = value;
		dev->i2c_adap = i2c_get_adapter(value);
		pr_inf("%s: %d[%p]\n", buf, dev->i2c_adap_id, dev->i2c_adap);
	} else {
		dev->i2c_adap_id = -1;
		pr_error("cannot find resource \"%s\"\n", buf);
	}
#else				/*CONFIG_OF */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, buf);
	if (res) {
		int adap = res->start;

		dev->i2c_adap_id = adap;
		dev->i2c_adap = i2c_get_adapter(adap);
	} else {
		dev->i2c_adap_id = -1;
		pr_error("cannot find resource \"%s\"\n", buf);
	}
#endif				/*CONFIG_OF */

	snprintf(buf, sizeof(buf), "%s%d_i2c_addr", name, id);
#ifdef CONFIG_OF
	ret = of_property_read_u32(pdev->dev.of_node, buf, &value);
	if (!ret) {
		dev->i2c_addr = value;
		pr_inf("%s: %d\n", buf, dev->i2c_addr);
	} else {
		dev->i2c_addr = -1;
		pr_error("cannot find resource \"%s\"\n", buf);
	}
#else				/*CONFIG_OF */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, buf);
	if (res) {
		int addr = res->start;

		dev->i2c_addr = addr;
		pr_inf("%s: %d\n", buf, dev->i2c_addr);
	} else {
		dev->i2c_addr = -1;
		pr_error("cannot find resource \"%s\"\n", buf);
	}
#endif

	snprintf(buf, sizeof(buf), "%s%d_reset_gpio", name, id);
#ifdef CONFIG_OF
	ret = of_property_read_string(pdev->dev.of_node, buf, &str);
	if (!ret) {
		dev->reset_gpio =
		    desc_to_gpio(of_get_named_gpiod_flags(pdev->dev.of_node,
							  buf, 0, NULL));
		pr_inf("%s: %s\n", buf, str);
	} else {
		dev->reset_gpio = -1;
		pr_error("cannot find resource \"%s\"\n", buf);
	}
#else				/*CONFIG_OF */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, buf);
	if (res) {
		int gpio = res->start;

		dev->reset_gpio = gpio;
		pr_inf("%s: %x\n", buf, gpio);
	} else {
		dev->reset_gpio = -1;
		pr_error("cannot find resource \"%s\"\n", buf);
	}
#endif				/*CONFIG_OF */

	snprintf(buf, sizeof(buf), "%s%d_reset_value", name, id);
#ifdef CONFIG_OF
	ret = of_property_read_u32(pdev->dev.of_node, buf, &value);
	if (!ret) {
		dev->reset_value = value;
		pr_inf("%s: %d\n", buf, dev->reset_value);
	} else {
		dev->reset_value = -1;
	}
#else				/*CONFIG_OF */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, buf);
	if (res) {
		int v = res->start;

		dev->reset_value = v;
		pr_inf("%s: %d\n", buf, dev->reset_value);
	} else {
		dev->reset_value = 0;
		pr_error("cannot find resource \"%s\"\n", buf);
	}
#endif				/*CONFIG_OF */

	snprintf(buf, sizeof(buf), "%s%d_tunerpower", name, id);
#ifdef CONFIG_OF
	ret = of_property_read_string(pdev->dev.of_node, buf, &str);
	if (!ret) {
		dev->tuner_power_gpio =
		    desc_to_gpio(of_get_named_gpiod_flags(pdev->dev.of_node,
							  buf, 0, NULL));
		pr_inf("%s: %s\n", buf, str);
	} else {
		dev->tuner_power_gpio = -1;
	}
#else				/*CONFIG_OF */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, buf);
	if (res) {
		int gpio = res->start;

		dev->tuner_power_gpio = gpio;
	} else {
		dev->tuner_power_gpio = -1;
	}
#endif				/*CONFIG_OF */

	snprintf(buf, sizeof(buf), "%s%d_lnbpower", name, id);
#ifdef CONFIG_OF
	ret = of_property_read_string(pdev->dev.of_node, buf, &str);
	if (!ret) {
		dev->lnb_power_gpio =
		    desc_to_gpio(of_get_named_gpiod_flags(pdev->dev.of_node,
							  buf, 0, NULL));
		pr_inf("%s: %s\n", buf, str);
	} else {
		dev->lnb_power_gpio = -1;
	}
#else				/*CONFIG_OF */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, buf);
	if (res) {
		int gpio = res->start;

		dev->lnb_power_gpio = gpio;
	} else {
		dev->lnb_power_gpio = -1;
	}
#endif

	snprintf(buf, sizeof(buf), "%s%d_antoverload", name, id);
#ifdef CONFIG_OF
	ret = of_property_read_string(pdev->dev.of_node, buf, &str);
	if (!ret) {
		dev->antoverload_gpio =
		    desc_to_gpio(of_get_named_gpiod_flags(pdev->dev.of_node,
							  buf, 0, NULL));
		pr_inf("%s: %s\n", buf, str);
	} else {
		dev->antoverload_gpio = -1;
	}
#else				/*CONFIG_OF */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, buf);
	if (res) {
		int gpio = res->start;

		dev->antoverload_gpio = gpio;
	} else {
		dev->antoverload_gpio = -1;
	}
#endif				/*CONFIG_OF */

	snprintf(buf, sizeof(buf), "%s%d_spectrum", name, id);
#ifdef CONFIG_OF
	ret = of_property_read_u32(pdev->dev.of_node, buf, &value);
	if (!ret) {
		dev->spectrum = value;
		pr_inf("%s: %d\n", buf, value);
	} else {
		dev->spectrum = 2;
	}
#else				/*CONFIG_OF */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, buf);
	if (res) {
		int spectrum = res->start;

		dev->spectrum = spectrum;
	} else {
		dev->spectrum = 0;
	}
#endif
	snprintf(buf, sizeof(buf), "%s%d_cma_flag", name, id);
#ifdef CONFIG_OF
		ret = of_property_read_u32(pdev->dev.of_node, buf, &value);
		if (!ret) {
			dev->cma_flag = value;
			pr_inf("%s: %d\n", buf, value);
		} else {
			dev->cma_flag = 0;
		}
#else				/*CONFIG_OF */
		res = platform_get_resource_byname(pdev, IORESOURCE_MEM, buf);
		if (res) {
			int cma_flag = res->start;
			dev->cma_flag = cma_flag;
		} else {
			dev->cma_flag = 0;
		}
#endif
	if (dev->cma_flag == 1) {
		snprintf(buf, sizeof(buf), "%s%d_cma_mem_size", name, id);
#ifdef CONFIG_CMA
#ifdef CONFIG_OF
	ret = of_property_read_u32(pdev->dev.of_node, buf, &value);
	if (!ret) {
		dev->cma_mem_size = value;
		pr_inf("%s: %d\n", buf, value);
	} else {
		dev->cma_mem_size = 0;
	}
#else				/*CONFIG_OF */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, buf);
	if (res) {
		int cma_mem_size = res->start;

		dev->cma_mem_size = cma_mem_size;
	} else {
		dev->cma_mem_size = 0;
	}
#endif
	dev->cma_mem_size =
				dma_get_cma_size_int_byte(&pdev->dev);
		dev->this_pdev = pdev;
		dev->cma_mem_alloc = 0;
		pr_inf("[cma]demod cma_mem_size = %d MB\n",
				(u32)dev->cma_mem_size/SZ_1M);
#endif
	} else {
#ifdef CONFIG_OF
	dev->mem_start = memstart;
#endif

		if (dev->drv->init) {
			ret = dev->drv->init(dev);
			if (ret != 0) {
				dev->drv = NULL;
				pr_error("[aml_fe..]%s error.\n", __func__);
				return ret;
			}
		}
	}

	return 0;
}

static int aml_fe_dev_release(struct aml_dvb *dvb, enum aml_fe_dev_type_t type,
			      struct aml_fe_dev *dev)
{
	if (dev->drv) {
		if (dev->drv->owner)
			module_put(dev->drv->owner);
		dev->drv->ref--;
		if (dev->drv->release)
			dev->drv->release(dev);
	}

	dev->drv = NULL;
	return 0;
}

static void aml_fe_man_run(struct aml_dvb *dvb, struct aml_fe *fe)
{
	int tuner_cap = 0xFFFFFFFF;
	int demod_cap = 0;

	if (fe->init)
		return;

	if (fe->tuner && fe->tuner->drv) {
		tuner_cap = fe->tuner->drv->capability;
		fe->init = 1;
	}

	if (fe->atv_demod && fe->atv_demod->drv) {
		demod_cap |= fe->atv_demod->drv->capability;
		fe->init = 1;
	}

	if (fe->dtv_demod && fe->dtv_demod->drv) {
		demod_cap |= fe->dtv_demod->drv->capability;
		fe->init = 1;
	}

	if (fe->init) {
		int reg = 1;
		int ret;
		int id;

		spin_lock_init(&fe->slock);
		fe->mode = AM_FE_UNKNOWN;
		fe->capability = (tuner_cap & demod_cap);
		pr_dbg("fe: %p cap: %x tuner: %x demod: %x\n", fe,
		       fe->capability, tuner_cap, demod_cap);

		for (id = 0; id < FE_DEV_COUNT; id++) {
			struct aml_fe *prev_fe = &fe_man.fe[id];

			if (prev_fe == fe)
				continue;
			if (prev_fe->init && (prev_fe->dev_id == fe->dev_id)) {
				reg = 0;
				break;
			}
		}
		fe->fe = &fe_man.dev[fe->dev_id];
		if (reg) {
			fe->fe->demodulator_priv = fe;
			fe->fe->ops.set_mode = aml_fe_set_mode;
			fe->fe->ops.read_ts = aml_fe_read_ts;

			ret = dvb_register_frontend(&dvb->dvb_adapter, fe->fe);
			if (ret) {
				pr_error("register fe%d failed\n", fe->dev_id);
				return;
			}
		}

		if (fe->tuner)
			fe->tuner->fe = fe;
		if (fe->atv_demod)
			fe->atv_demod->fe = fe;
		if (fe->dtv_demod)
			fe->dtv_demod->fe = fe;
	}
}

static int aml_fe_man_init(struct aml_dvb *dvb, struct platform_device *pdev,
			   struct aml_fe *fe, int id)
{
#ifndef CONFIG_OF
	struct resource *res;
#endif
	char buf[32];
	u32 value;
	int ret;

	snprintf(buf, sizeof(buf), "fe%d_tuner", id);

#ifdef CONFIG_OF
	ret = of_property_read_u32(pdev->dev.of_node, buf, &value);
	if (!ret) {
		int id = value;
		if ((id < 0) || (id >= FE_DEV_COUNT) || !fe_man.tuner[id].drv) {
			pr_error("invalid tuner device id %d\n", id);
			return -1;
		}

		fe->tuner = &fe_man.tuner[id];

		pr_dbg("%s: %d\n", buf, id);
	}
#else				/*CONFIG_OF */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, buf);
	if (res) {
		int id = res->start;
		if ((id < 0) || (id >= FE_DEV_COUNT) || !fe_man.tuner[id].drv) {
			pr_error("invalid tuner device id %d\n", id);
			return -1;
		}

		fe->tuner = &fe_man.tuner[id];
	}
#endif				/*CONFIG_OF */

	snprintf(buf, sizeof(buf), "fe%d_atv_demod", id);
#ifdef CONFIG_OF
	ret = of_property_read_u32(pdev->dev.of_node, buf, &value);
	if (!ret) {
		int id = value;
		if ((id < 0) || (id >= FE_DEV_COUNT)
		    || !fe_man.atv_demod[id].drv) {
			pr_error("invalid ATV demod device id %d\n", id);
			return -1;
		}

		fe->atv_demod = &fe_man.atv_demod[id];
		pr_dbg("%s: %d\n", buf, id);
	}
#else				/*CONFIG_OF */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, buf);
	if (res) {
		int id = res->start;
		if ((id < 0) || (id >= FE_DEV_COUNT)
		    || !fe_man.atv_demod[id].drv) {
			pr_error("invalid ATV demod device id %d\n", id);
			return -1;
		}

		fe->atv_demod = &fe_man.atv_demod[id];
	}
#endif				/*CONFIG_OF */

	snprintf(buf, sizeof(buf), "fe%d_dtv_demod", id);
#ifdef CONFIG_OF
	ret = of_property_read_u32(pdev->dev.of_node, buf, &value);
	if (!ret) {
		int id = value;
		if ((id < 0) || (id >= FE_DEV_COUNT)
		    || !fe_man.dtv_demod[id].drv) {
			pr_error("invalid DTV demod device id %d\n", id);
			return -1;
		}

		fe->dtv_demod = &fe_man.dtv_demod[id];
		pr_dbg("%s: %d\n", buf, id);
	}
#else				/*CONFIG_OF */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, buf);
	if (res) {
		int id = res->start;

		pr_dbg("[dvb] res->start is %d\n", res->start);
		if ((id < 0) || (id >= FE_DEV_COUNT)
		    || !fe_man.dtv_demod[id].drv) {
			pr_error("invalid DTV demod device id %d\n", id);
			return -1;
		}

		fe->dtv_demod = &fe_man.dtv_demod[id];
	}
#endif				/*CONFIG_OF */

	snprintf(buf, sizeof(buf), "fe%d_ts", id);
#ifdef CONFIG_OF
	ret = of_property_read_u32(pdev->dev.of_node, buf, &value);
	if (!ret) {
		int id = value;
		enum aml_ts_source_t ts = AM_TS_SRC_TS0;

		switch (id) {
		case 0:
			ts = AM_TS_SRC_TS0;
			break;
		case 1:
			ts = AM_TS_SRC_TS1;
			break;
		case 2:
			ts = AM_TS_SRC_TS2;
			break;
		default:
			break;
		}

		fe->ts = ts;
		pr_dbg("%s: %d\n", buf, id);
	}
#else				/*CONFIG_OF */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, buf);
	if (res) {
		int id = res->start;
		enum aml_ts_source_t ts = AM_TS_SRC_TS0;

		switch (id) {
		case 0:
			ts = AM_TS_SRC_TS0;
			break;
		case 1:
			ts = AM_TS_SRC_TS1;
			break;
		case 2:
			ts = AM_TS_SRC_TS2;
			break;
		default:
			break;
		}

		fe->ts = ts;
	}
#endif				/*CONFIG_OF */

	snprintf(buf, sizeof(buf), "fe%d_dev", id);
#ifdef CONFIG_OF
	ret = of_property_read_u32(pdev->dev.of_node, buf, &value);
	if (!ret) {
		int id = value;

		if ((id >= 0) && (id < FE_DEV_COUNT))
			fe->dev_id = id;
		else
			fe->dev_id = 0;
		pr_dbg("%s: %d\n", buf, fe->dev_id);
	} else {
		fe->dev_id = 0;
		pr_dbg("cannot get resource \"%s\"\n", buf);
	}
#else				/*CONFIG_OF */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, buf);
	if (res) {
		int id = res->start;

		if ((id >= 0) && (id < FE_DEV_COUNT))
			fe->dev_id = id;
		else
			fe->dev_id = 0;
		pr_dbg("%s: %d\n", buf, fe->dev_id);
	} else {
		fe->dev_id = 0;
		pr_dbg("cannot get resource \"%s\"\n", buf);
	}
#endif				/*CONFIG_OF */

	aml_fe_man_run(dvb, fe);

	return 0;
}

static int aml_fe_man_release(struct aml_dvb *dvb, struct aml_fe *fe)
{
	if (fe->init) {
		aml_dmx_register_frontend(fe->ts, NULL);
		dvb_unregister_frontend(fe->fe);
		dvb_frontend_detach(fe->fe);

		fe->tuner = NULL;
		fe->atv_demod = NULL;
		fe->dtv_demod = NULL;
		fe->init = 0;
	}

	return 0;
}

static ssize_t tuner_name_show(struct class *cls, struct class_attribute *attr,
			       char *buf)
{
	size_t len = 0;
	struct aml_fe_drv *drv;
	unsigned long flags;

	struct aml_fe_drv **list = aml_get_fe_drv_list(AM_DEV_TUNER);

	spin_lock_irqsave(&lock, flags);
	for (drv = *list; drv; drv = drv->next)
		len += sprintf(buf + len, "%s\n", drv->name);
	spin_unlock_irqrestore(&lock, flags);
	return len;
}

static ssize_t atv_demod_name_show(struct class *cls,
				   struct class_attribute *attr, char *buf)
{
	size_t len = 0;
	struct aml_fe_drv *drv;
	unsigned long flags;

	struct aml_fe_drv **list = aml_get_fe_drv_list(AM_DEV_ATV_DEMOD);

	spin_lock_irqsave(&lock, flags);
	for (drv = *list; drv; drv = drv->next)
		len += sprintf(buf + len, "%s\n", drv->name);
	spin_unlock_irqrestore(&lock, flags);
	return len;
}

static ssize_t dtv_demod_name_show(struct class *cls,
				   struct class_attribute *attr, char *buf)
{
	size_t len = 0;
	struct aml_fe_drv *drv;
	unsigned long flags;

	struct aml_fe_drv **list = aml_get_fe_drv_list(AM_DEV_DTV_DEMOD);

	spin_lock_irqsave(&lock, flags);
	for (drv = *list; drv; drv = drv->next)
		len += sprintf(buf + len, "%s\n", drv->name);
	spin_unlock_irqrestore(&lock, flags);
	return len;
}

static ssize_t setting_show(struct class *cls, struct class_attribute *attr,
			    char *buf)
{
	int r, total = 0;
	int i;
	struct aml_fe_man *fm = &fe_man;

	r = sprintf(buf, "tuner:\n");
	buf += r;
	total += r;
	for (i = 0; i < FE_DEV_COUNT; i++) {
		struct aml_fe_dev *dev = &fm->tuner[i];
		if (dev->drv) {
			r = sprintf(buf,
				    "\t%d: %s i2s_id: %d i2c_addr: 0x%x reset_gpio: 0x%x reset_level: %d\n",
				    i, dev->drv->name, dev->i2c_adap_id,
				    dev->i2c_addr, dev->reset_gpio,
				    dev->reset_value);
			buf += r;
			total += r;
		}
	}

	r = sprintf(buf, "atv_demod:\n");
	buf += r;
	total += r;
	for (i = 0; i < FE_DEV_COUNT; i++) {
		struct aml_fe_dev *dev = &fm->atv_demod[i];
		if (dev->drv) {
			r = sprintf(buf,
				    "\t%d: %s i2s_id: %d i2c_addr: 0x%x reset_gpio: 0x%x reset_level: %d\n",
				    i, dev->drv->name, dev->i2c_adap_id,
				    dev->i2c_addr, dev->reset_gpio,
				    dev->reset_value);
			buf += r;
			total += r;
		}
	}

	r = sprintf(buf, "dtv_demod:\n");
	buf += r;
	total += r;
	for (i = 0; i < FE_DEV_COUNT; i++) {
		struct aml_fe_dev *dev = &fm->dtv_demod[i];
		if (dev->drv) {
			r = sprintf(buf,
				    "\t%d: %s i2s_id: %d i2c_addr: 0x%x reset_gpio: 0x%x reset_level: %d\n",
				    i, dev->drv->name, dev->i2c_adap_id,
				    dev->i2c_addr, dev->reset_gpio,
				    dev->reset_value);
			buf += r;
			total += r;
		}
	}

	r = sprintf(buf, "frontend:\n");
	buf += r;
	total += r;
	for (i = 0; i < FE_DEV_COUNT; i++) {
		struct aml_fe *fe = &fm->fe[i];

		r = sprintf(buf,
			    "\t%d: %s device: %d ts: %d tuner: %s atv_demod: %s dtv_demod: %s\n",
			    i, fe->init ? "enabled" : "disabled", fe->dev_id,
			    fe->ts, fe->tuner ? fe->tuner->drv->name : "none",
			    fe->atv_demod ? fe->atv_demod->drv->name : "none",
			    fe->dtv_demod ? fe->dtv_demod->drv->name : "none");
		buf += r;
		total += r;
	}

	return total;
}

static void reset_drv(int id, enum aml_fe_dev_type_t type, const char *name)
{
	struct aml_fe_man *fm = &fe_man;
	struct aml_fe_drv **list;
	struct aml_fe_drv **pdrv;
	struct aml_fe_drv *drv;
	struct aml_fe_drv *old;

	if ((id < 0) || (id >= FE_DEV_COUNT))
		return;

	if (fm->fe[id].init) {
		pr_error("cannot reset driver when the device is inused\n");
		return;
	}

	list = aml_get_fe_drv_list(type);
	for (drv = *list; drv; drv = drv->next)
		if (!strcmp(drv->name, name))
			break;

	switch (type) {
	case AM_DEV_TUNER:
		pdrv = &fm->tuner[id].drv;
		break;
	case AM_DEV_ATV_DEMOD:
		pdrv = &fm->atv_demod[id].drv;
		break;
	case AM_DEV_DTV_DEMOD:
		pdrv = &fm->dtv_demod[id].drv;
		break;
	default:
		return;
	}

	old = *pdrv;
	if (old == drv)
		return;

	if (old) {
		old->ref--;
		if (old->owner)
			module_put(old->owner);
	}

	if (drv) {
		drv->ref++;
		if (drv->owner)
			try_module_get(drv->owner);
	}

	*pdrv = drv;
}

static ssize_t setting_store(struct class *class, struct class_attribute *attr,
			     const char *buf, size_t size)
{
	struct aml_dvb *dvb = aml_get_dvb_device();
	struct aml_fe_man *fm = &fe_man;
	int id, val;
	char dev_name[32];
	char gpio_name[32];
	unsigned long flags;

	spin_lock_irqsave(&lock, flags);

	if (sscanf(buf, "tuner %i driver %s", &id, dev_name) == 2) {
		reset_drv(id, AM_DEV_TUNER, dev_name);
	} else if (sscanf(buf, "tuner %i i2c_id %i", &id, &val) == 2) {
		if ((id >= 0) && (id < FE_DEV_COUNT)) {
			fm->tuner[id].i2c_adap_id = val;
			fm->tuner[id].i2c_adap = i2c_get_adapter(val);
		}
	} else if (sscanf(buf, "tuner %i i2c_addr %i", &id, &val) == 2) {
		if ((id >= 0) && (id < FE_DEV_COUNT))
			fm->tuner[id].i2c_addr = val;
#ifdef CONFIG_OF
	} else if (sscanf(buf, "tuner %i reset_gpio %s", &id, gpio_name) == 2) {
		val =
		    desc_to_gpio(of_get_named_gpiod_flags
				 (dvb->pdev->dev.of_node, gpio_name, 0, NULL));
#else
	} else if (sscanf(buf, "tuner %i reset_gpio %i", &id, &val) == 2) {
#endif
		if ((id >= 0) && (id < FE_DEV_COUNT))
			fm->tuner[id].reset_gpio = val;
	} else if (sscanf(buf, "tuner %i reset_level %i", &id, &val) == 2) {
		if ((id >= 0) && (id < FE_DEV_COUNT))
			fm->tuner[id].reset_value = val;
	} else if (sscanf(buf, "atv_demod %i driver %s", &id, dev_name) == 2) {
		reset_drv(id, AM_DEV_ATV_DEMOD, dev_name);
	} else if (sscanf(buf, "atv_demod %i i2c_id %i", &id, &val) == 2) {
		if ((id >= 0) && (id < FE_DEV_COUNT)) {
			fm->atv_demod[id].i2c_adap_id = val;
			fm->dtv_demod[id].i2c_adap = i2c_get_adapter(val);
		}
	} else if (sscanf(buf, "atv_demod %i i2c_addr %i", &id, &val) == 2) {
		if ((id >= 0) && (id < FE_DEV_COUNT))
			fm->atv_demod[id].i2c_addr = val;
#ifdef CONFIG_OF
	} else if (sscanf(buf, "atv_demod %i reset_gpio %s", &id, gpio_name) ==
		   2) {
		val =
		    desc_to_gpio(of_get_named_gpiod_flags
				 (dvb->pdev->dev.of_node, gpio_name, 0, NULL));
#else
	} else if (sscanf(buf, "atv_demod %i reset_gpio %i", &id, &val) == 2) {
#endif
		if ((id >= 0) && (id < FE_DEV_COUNT))
			fm->atv_demod[id].reset_gpio = val;
	} else if (sscanf(buf, "atv_demod %i reset_level %i", &id, &val) == 2) {
		if ((id >= 0) && (id < FE_DEV_COUNT))
			fm->atv_demod[id].reset_value = val;
	} else if (sscanf(buf, "dtv_demod %i driver %s", &id, dev_name) == 2) {
		reset_drv(id, AM_DEV_DTV_DEMOD, dev_name);
	} else if (sscanf(buf, "dtv_demod %i i2c_id %i", &id, &val) == 2) {
		if ((id >= 0) && (id < FE_DEV_COUNT)) {
			fm->dtv_demod[id].i2c_adap_id = val;
			fm->dtv_demod[id].i2c_adap = i2c_get_adapter(val);
		}
	} else if (sscanf(buf, "dtv_demod %i i2c_addr %i", &id, &val) == 2) {
		if ((id >= 0) && (id < FE_DEV_COUNT))
			fm->dtv_demod[id].i2c_addr = val;
#ifdef CONFIG_OF
	} else if (sscanf(buf, "dtv_demod %i reset_gpio %s", &id, gpio_name) ==
		   2) {
		val =
		    desc_to_gpio(of_get_named_gpiod_flags
				 (dvb->pdev->dev.of_node, gpio_name, 0, NULL));
#else
	} else if (sscanf(buf, "dtv_demod %i reset_gpio %i", &id, &val) == 2) {
#endif
		if ((id >= 0) && (id < FE_DEV_COUNT))
			fm->dtv_demod[id].reset_gpio = val;
	} else if (sscanf(buf, "dtv_demod %i reset_level %i", &id, &val) == 2) {
		if ((id >= 0) && (id < FE_DEV_COUNT))
			fm->dtv_demod[id].reset_value = val;
	} else if (sscanf(buf, "frontend %i device %i", &id, &val) == 2) {
		if ((id >= 0) && (id < FE_DEV_COUNT))
			fm->fe[id].dev_id = val;
	} else if (sscanf(buf, "frontend %i ts %i", &id, &val) == 2) {
		if ((id >= 0) && (id < FE_DEV_COUNT))
			fm->fe[id].ts = val;
	} else if (sscanf(buf, "frontend %i tuner %i", &id, &val) == 2) {
		if ((id >= 0) && (id < FE_DEV_COUNT) && (val >= 0)
		    && (val < FE_DEV_COUNT) && fm->tuner[val].drv)
			fm->fe[id].tuner = &fm->tuner[val];
	} else if (sscanf(buf, "frontend %i atv_demod %i", &id, &val) == 2) {
		if ((id >= 0) && (id < FE_DEV_COUNT) && (val >= 0)
		    && (val < FE_DEV_COUNT) && fm->atv_demod[val].drv)
			fm->fe[id].atv_demod = &fm->atv_demod[val];
	} else if (sscanf(buf, "frontend %i dtv_demod %i", &id, &val) == 2) {
		if ((id >= 0) && (id < FE_DEV_COUNT) && (val >= 0)
		    && (val < FE_DEV_COUNT) && fm->dtv_demod[val].drv)
			fm->fe[id].dtv_demod = &fm->dtv_demod[val];
	}

	spin_unlock_irqrestore(&lock, flags);

	if (sscanf(buf, "enable %i", &id) == 1) {
		if ((id >= 0) && (id < FE_DEV_COUNT))
			aml_fe_man_run(dvb, &fm->fe[id]);
	} else if (sscanf(buf, "disable %i", &id) == 1) {
		if ((id >= 0) && (id < FE_DEV_COUNT))
			aml_fe_man_release(dvb, &fm->fe[id]);
	} else if (strstr(buf, "autoload")) {
		for (id = 0; id < FE_DEV_COUNT; id++) {
			aml_fe_dev_init(dvb, fm->pdev, AM_DEV_TUNER,
					&fm->tuner[id], id);
			aml_fe_dev_init(dvb, fm->pdev, AM_DEV_ATV_DEMOD,
					&fm->atv_demod[id], id);
			aml_fe_dev_init(dvb, fm->pdev, AM_DEV_DTV_DEMOD,
					&fm->dtv_demod[id], id);
		}

		for (id = 0; id < FE_DEV_COUNT; id++)
			aml_fe_man_init(dvb, fm->pdev, &fm->fe[id], id);
	} else if (strstr(buf, "disableall")) {
		for (id = 0; id < FE_DEV_COUNT; id++)
			aml_fe_man_release(dvb, &fm->fe[id]);

		for (id = 0; id < FE_DEV_COUNT; id++) {
			aml_fe_dev_release(dvb, AM_DEV_DTV_DEMOD,
					   &fm->dtv_demod[id]);
			aml_fe_dev_release(dvb, AM_DEV_ATV_DEMOD,
					   &fm->atv_demod[id]);
			aml_fe_dev_release(dvb, AM_DEV_TUNER, &fm->tuner[id]);
		}
	}

	return size;
}

static ssize_t aml_fe_show_suspended_flag(struct class *class,
					  struct class_attribute *attr,
					  char *buf)
{
	ssize_t ret = 0;

	ret = sprintf(buf, "%ld\n", aml_fe_suspended);

	return ret;
}

static ssize_t aml_fe_store_suspended_flag(struct class *class,
					   struct class_attribute *attr,
					   const char *buf, size_t size)
{
	/*aml_fe_suspended = simple_strtol(buf, 0, 0); */
	int ret = kstrtol(buf, 0, &aml_fe_suspended);

	if (ret)
		return ret;
	return size;
}

static struct class_attribute aml_fe_cls_attrs[] = {
	__ATTR(tuner_name,
	       S_IRUGO | S_IWUSR,
	       tuner_name_show, NULL),
	__ATTR(atv_demod_name,
	       S_IRUGO | S_IWUSR,
	       atv_demod_name_show, NULL),
	__ATTR(dtv_demod_name,
	       S_IRUGO | S_IWUSR,
	       dtv_demod_name_show, NULL),
	__ATTR(setting,
	       S_IRUGO | S_IWUSR,
	       setting_show, setting_store),
	__ATTR(aml_fe_suspended_flag,
	       S_IRUGO | S_IWUSR,
	       aml_fe_show_suspended_flag,
	       aml_fe_store_suspended_flag),
	__ATTR_NULL
};

static struct class aml_fe_class = {
	.name = "amlfe",
	.class_attrs = aml_fe_cls_attrs,
};

static int aml_fe_probe(struct platform_device *pdev)
{
	struct aml_dvb *dvb = aml_get_dvb_device();
	int i;

	of_reserved_mem_device_init(&pdev->dev);
	for (i = 0; i < FE_DEV_COUNT; i++) {
		if (aml_fe_dev_init
		    (dvb, pdev, AM_DEV_TUNER, &fe_man.tuner[i], i) < 0)
			goto probe_end;
		if (aml_fe_dev_init
		    (dvb, pdev, AM_DEV_ATV_DEMOD, &fe_man.atv_demod[i], i) < 0)
			goto probe_end;
		if (aml_fe_dev_init
		    (dvb, pdev, AM_DEV_DTV_DEMOD, &fe_man.dtv_demod[i], i) < 0)
			goto probe_end;
	}

	for (i = 0; i < FE_DEV_COUNT; i++)
		if (aml_fe_man_init(dvb, pdev, &fe_man.fe[i], i) < 0)
			goto probe_end;

 probe_end:

	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_MG9TV)
		INIT_WORK(&dvb->aml_fe_wq,
			(void (*)(struct work_struct *))aml_fe_do_work);


#ifdef CONFIG_OF
	fe_man.pinctrl = devm_pinctrl_get_select_default(&pdev->dev);
#endif

	platform_set_drvdata(pdev, &fe_man);

	if (class_register(&aml_fe_class) < 0)
		pr_error("[aml_fe..] register class error\n");

	fe_man.pdev = pdev;

	pr_dbg("[aml_fe..] probe ok.\n");

	return 0;
}

static int aml_fe_remove(struct platform_device *pdev)
{
	struct aml_fe_man *fe_man = platform_get_drvdata(pdev);
	struct aml_dvb *dvb = aml_get_dvb_device();
	int i;

	if (fe_man) {
		platform_set_drvdata(pdev, NULL);
		for (i = 0; i < FE_DEV_COUNT; i++)
			aml_fe_man_release(dvb, &fe_man->fe[i]);
		for (i = 0; i < FE_DEV_COUNT; i++) {
			aml_fe_dev_release(dvb, AM_DEV_DTV_DEMOD,
					   &fe_man->dtv_demod[i]);
			aml_fe_dev_release(dvb, AM_DEV_ATV_DEMOD,
					   &fe_man->atv_demod[i]);
			aml_fe_dev_release(dvb, AM_DEV_TUNER,
					   &fe_man->tuner[i]);
		}

		if (fe_man->pinctrl)
			devm_pinctrl_put(fe_man->pinctrl);
	}

	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_MG9TV)
		cancel_work_sync(&dvb->aml_fe_wq);

	class_unregister(&aml_fe_class);

	return 0;
}

int aml_fe_suspend(struct platform_device *dev, pm_message_t state)
{
	int i;
	struct aml_dvb *dvb = aml_get_dvb_device();

	cancel_work_sync(&dvb->aml_fe_wq);
	aft_thread_enable_cache = aft_thread_enable;
	aft_thread_enable = 0;
	for (i = 0; i < FE_DEV_COUNT; i++) {
		struct aml_fe *fe = &fe_man.fe[i];

		if (fe->tuner && fe->tuner->drv && fe->tuner->drv->suspend)
			fe->tuner->drv->suspend(fe->tuner);

		if (fe->atv_demod && fe->atv_demod->drv
		    && fe->atv_demod->drv->suspend)
			fe->atv_demod->drv->suspend(fe->atv_demod);

		if (fe->dtv_demod && fe->dtv_demod->drv
		    && fe->dtv_demod->drv->suspend)
			fe->dtv_demod->drv->suspend(fe->dtv_demod);
	}

	aml_fe_suspended = 1;

	return 0;
}
EXPORT_SYMBOL(aml_fe_suspend);

int aml_fe_resume(struct platform_device *dev)
{
	int i;

	for (i = 0; i < FE_DEV_COUNT; i++) {
		struct aml_fe *fe = &fe_man.fe[i];

		if (fe->tuner && fe->tuner->drv && fe->tuner->drv->resume)
			fe->tuner->drv->resume(fe->tuner);
		if (fe->atv_demod && fe->atv_demod->drv
		    && fe->atv_demod->drv->resume)
			fe->atv_demod->drv->resume(fe->atv_demod);

		if (fe->dtv_demod && fe->dtv_demod->drv
		    && fe->dtv_demod->drv->resume)
			fe->dtv_demod->drv->resume(fe->dtv_demod);
	}
	aft_thread_enable = aft_thread_enable_cache;

	return 0;
}
EXPORT_SYMBOL(aml_fe_resume);

#ifdef CONFIG_OF
static const struct of_device_id aml_fe_dt_match[] = {
	{
	 .compatible = "amlogic, dvbfe",
	 },
	{},
};
#endif				/*CONFIG_OF */

static struct platform_driver aml_fe_driver = {
	.probe = aml_fe_probe,
	.remove = aml_fe_remove,
	.driver = {
		   .name = "amlogic-dvb-fe",
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = aml_fe_dt_match,
#endif
		   }
};

const char *audmode_to_str(unsigned short audmode)
{
	/*  switch(audmode)
	 * {
	 * case V4L2_TUNER_AUDMODE_NULL:
	 * return "V4L2_TUNER_AUDMODE_NULL";
	 * break;
	 * case V4L2_TUNER_MODE_MONO:
	 * return "V4L2_TUNER_MODE_MONO";
	 * break;
	 * case V4L2_TUNER_MODE_STEREO:
	 * return "V4L2_TUNER_MODE_STEREO";
	 * break;
	 * case V4L2_TUNER_MODE_LANG2:
	 * return "V4L2_TUNER_MODE_LANG2";
	 * break;
	 * case V4L2_TUNER_MODE_SAP:
	 * return "V4L2_TUNER_MODE_SAP";
	 * break;
	 * case V4L2_TUNER_SUB_LANG1:
	 * return "V4L2_TUNER_SUB_LANG1";
	 * break;
	 * case V4L2_TUNER_MODE_LANG1_LANG2:
	 * return "V4L2_TUNER_MODE_LANG1_LANG2";
	 * break;
	 * default:
	 * return "NO AUDMODE";
	 * break;
	 * } */
	return 0;
}
EXPORT_SYMBOL(audmode_to_str);

const char *soundsys_to_str(unsigned short sys)
{
	/*switch(sys){
	 * case V4L2_TUNER_SYS_NULL:
	 * return "V4L2_TUNER_SYS_NULL";
	 * break;
	 * case V4L2_TUNER_SYS_A2_BG:
	 * return "V4L2_TUNER_SYS_A2_BG";
	 * break;
	 * case V4L2_TUNER_SYS_A2_DK1:
	 * return "V4L2_TUNER_SYS_A2_DK1";
	 * break;
	 * case V4L2_TUNER_SYS_A2_DK2:
	 * return "V4L2_TUNER_SYS_A2_DK2";
	 * break;
	 * case V4L2_TUNER_SYS_A2_DK3:
	 * return "V4L2_TUNER_SYS_A2_DK3";
	 * break;
	 * case V4L2_TUNER_SYS_A2_M:
	 * return "V4L2_TUNER_SYS_A2_M";
	 * break;
	 * case V4L2_TUNER_SYS_NICAM_BG:
	 * return "V4L2_TUNER_SYS_NICAM_BG";
	 * break;
	 * case V4L2_TUNER_SYS_NICAM_I:
	 * return "V4L2_TUNER_SYS_NICAM_I";
	 * break;
	 * case V4L2_TUNER_SYS_NICAM_DK:
	 * return "V4L2_TUNER_SYS_NICAM_DK";
	 * break;
	 * case V4L2_TUNER_SYS_NICAM_L:
	 * return "V4L2_TUNER_SYS_NICAM_L";
	 * break;
	 * case V4L2_TUNER_SYS_EIAJ:
	 * return "V4L2_TUNER_SYS_EIAJ";
	 * break;
	 * case V4L2_TUNER_SYS_BTSC:
	 * return "V4L2_TUNER_SYS_BTSC";
	 * break;
	 * case V4L2_TUNER_SYS_FM_RADIO:
	 * return "V4L2_TUNER_SYS_FM_RADIO";
	 * break;
	 * default:
	 * return "NO SOUND SYS";
	 * break;
	 * } */
	return 0;
}
EXPORT_SYMBOL(soundsys_to_str);

const char *v4l2_std_to_str(v4l2_std_id std)
{
	switch (std) {
	case V4L2_STD_PAL_B:
		return "V4L2_STD_PAL_B";
		break;
	case V4L2_STD_PAL_B1:
		return "V4L2_STD_PAL_B1";
		break;
	case V4L2_STD_PAL_G:
		return "V4L2_STD_PAL_G";
		break;
	case V4L2_STD_PAL_H:
		return "V4L2_STD_PAL_H";
		break;
	case V4L2_STD_PAL_I:
		return "V4L2_STD_PAL_I";
		break;
	case V4L2_STD_PAL_D:
		return "V4L2_STD_PAL_D";
		break;
	case V4L2_STD_PAL_D1:
		return "V4L2_STD_PAL_D1";
		break;
	case V4L2_STD_PAL_K:
		return "V4L2_STD_PAL_K";
		break;
	case V4L2_STD_PAL_M:
		return "V4L2_STD_PAL_M";
		break;
	case V4L2_STD_PAL_N:
		return "V4L2_STD_PAL_N";
		break;
	case V4L2_STD_PAL_Nc:
		return "V4L2_STD_PAL_Nc";
		break;
	case V4L2_STD_PAL_60:
		return "V4L2_STD_PAL_60";
		break;
	case V4L2_STD_NTSC_M:
		return "V4L2_STD_NTSC_M";
		break;
	case V4L2_STD_NTSC_M_JP:
		return "V4L2_STD_NTSC_M_JP";
		break;
	case V4L2_STD_NTSC_443:
		return "V4L2_STD_NTSC_443";
		break;
	case V4L2_STD_NTSC_M_KR:
		return "V4L2_STD_NTSC_M_KR";
		break;
	case V4L2_STD_SECAM_B:
		return "V4L2_STD_SECAM_B";
		break;
	case V4L2_STD_SECAM_D:
		return "V4L2_STD_SECAM_D";
		break;
	case V4L2_STD_SECAM_G:
		return "V4L2_STD_SECAM_G";
		break;
	case V4L2_STD_SECAM_H:
		return "V4L2_STD_SECAM_H";
		break;
	case V4L2_STD_SECAM_K:
		return "V4L2_STD_SECAM_K";
		break;
	case V4L2_STD_SECAM_K1:
		return "V4L2_STD_SECAM_K1";
		break;
	case V4L2_STD_SECAM_L:
		return "V4L2_STD_SECAM_L";
		break;
	case V4L2_STD_SECAM_LC:
		return "V4L2_STD_SECAM_LC";
		break;
	case V4L2_STD_ATSC_8_VSB:
		return "V4L2_STD_ATSC_8_VSB";
		break;
	case V4L2_STD_ATSC_16_VSB:
		return "V4L2_STD_ATSC_16_VSB";
		break;
	case V4L2_COLOR_STD_PAL:
		return "V4L2_COLOR_STD_PAL";
		break;
	case V4L2_COLOR_STD_NTSC:
		return "V4L2_COLOR_STD_NTSC";
		break;
	case V4L2_COLOR_STD_SECAM:
		return "V4L2_COLOR_STD_SECAM";
		break;
	case V4L2_STD_MN:
		return "V4L2_STD_MN";
		break;
	case V4L2_STD_B:
		return "V4L2_STD_B";
		break;
	case V4L2_STD_GH:
		return "V4L2_STD_GH";
		break;
	case V4L2_STD_DK:
		return "V4L2_STD_DK";
		break;
	case V4L2_STD_PAL_BG:
		return "V4L2_STD_PAL_BG";
		break;
	case V4L2_STD_PAL_DK:
		return "V4L2_STD_PAL_DK";
		break;
	case V4L2_STD_PAL:
		return "V4L2_STD_PAL";
		break;
	case V4L2_STD_NTSC:
		return "V4L2_STD_NTSC";
		break;
	case V4L2_STD_SECAM_DK:
		return "V4L2_STD_SECAM_DK";
		break;
	case V4L2_STD_SECAM:
		return "V4L2_STD_SECAM";
		break;
	case V4L2_STD_525_60:
		return "V4L2_STD_525_60";
		break;
	case V4L2_STD_625_50:
		return "V4L2_STD_625_50";
		break;
	case V4L2_STD_ATSC:
		return "V4L2_STD_ATSC";
		break;
	case V4L2_STD_ALL:
		return "V4L2_STD_ALL";
		break;
	default:
		return "V4L2_STD_UNKNOWN";
		break;
	}
}
EXPORT_SYMBOL(v4l2_std_to_str);

const char *fe_type_to_str(fe_type_t type)
{
	switch (type) {
	case FE_QPSK:
		return "FE_QPSK";
		break;
	case FE_QAM:
		return "FE_QAM";
		break;
	case FE_OFDM:
		return "FE_OFDM";
		break;
	case FE_ATSC:
		return "FE_ATSC";
		break;
	case FE_ANALOG:
		return "FE_ANALOG";
		break;
	case FE_ISDBT:
		return "FE_ISDBT";
		break;
	case FE_DTMB:
		return "FE_DTMB";
		break;
	default:
		return "UNKONW TYPE";
		break;
	}
}
EXPORT_SYMBOL(fe_type_to_str);

static int __init aml_fe_init(void)
{
	return platform_driver_register(&aml_fe_driver);
}

static void __exit aml_fe_exit(void)
{
	platform_driver_unregister(&aml_fe_driver);
}

module_init(aml_fe_init);
module_exit(aml_fe_exit);

MODULE_DESCRIPTION("amlogic frontend driver");
MODULE_AUTHOR("L+#= +0=1");
