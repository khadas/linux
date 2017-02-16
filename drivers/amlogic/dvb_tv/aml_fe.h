#ifndef _AML_FE_H_
#define _AML_FE_H_


#include <linux/interrupt.h>
#include <linux/socket.h>
#include <linux/netdevice.h>
#include <linux/i2c.h>

#include <linux/dvb/video.h>
#include <linux/dvb/audio.h>
#include <linux/dvb/dmx.h>
#include <linux/dvb/ca.h>
#include <linux/dvb/osd.h>
#include <linux/dvb/net.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#include "drivers/media/dvb-core/dvbdev.h"
#include "drivers/media/dvb-core/demux.h"
#include "drivers/media/dvb-core/dvb_demux.h"
#include "drivers/media/dvb-core/dmxdev.h"
#include "drivers/media/dvb-core/dvb_filter.h"
#include "drivers/media/dvb-core/dvb_net.h"
#include "drivers/media/dvb-core/dvb_ringbuffer.h"
#include "drivers/media/dvb-core/dvb_frontend.h"
#include "aml_dvb.h"
#include "linux/videodev2.h"

#include <linux/amlogic/aml_gpio_consumer.h>

#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/pinctrl/consumer.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/of_reserved_mem.h>

enum aml_fe_mode_t {
	AM_FE_UNKNOWN = 0,
	AM_FE_QPSK = 1,
	AM_FE_QAM = 2,
	AM_FE_OFDM = 4,
	AM_FE_ATSC = 8,
	AM_FE_ANALOG = 16,
	AM_FE_DTMB = 32,
	AM_FE_ISDBT = 64
};

#define AM_FE_DTV_MASK (AM_FE_QPSK | AM_FE_QAM | AM_FE_OFDM | \
			AM_FE_ATSC | AM_FE_DTMB | AM_FE_ISDBT)

enum aml_tuner_type_t {
	AM_TUNER_SI2176 = 1,
	AM_TUNER_SI2196 = 2,
	AM_TUNER_FQ1216 = 3,
	AM_TUNER_HTM = 4,
	AM_TUNER_CTC703 = 5,
	AM_TUNER_SI2177 = 6,
	AM_TUNER_R840 = 7,
	AM_TUNER_SI2157 = 8,
	AM_TUNER_SI2151 = 9,
	AM_TUNER_MXL661 = 10
};

enum aml_atv_demod_type_t {
	AM_ATV_DEMOD_SI2176 = 1,
	AM_ATV_DEMOD_SI2196 = 2,
	AM_ATV_DEMOD_FQ1216 = 3,
	AM_ATV_DEMOD_HTM = 4,
	AM_ATV_DEMOD_CTC703 = 5,
	AM_ATV_DEMOD_SI2177 = 6,
	AM_ATV_DEMOD_AML = 7,
	AM_ATV_DEMOD_R840 = 8
};

enum aml_dtv_demod_type_t {
	AM_DTV_DEMOD_M1 = 0,
	AM_DTV_DEMOD_SI2176 = 1,
	AM_DTV_DEMOD_MXL101 = 2,
	AM_DTV_DEMOD_SI2196 = 3,
	AM_DTV_DEMOD_AVL6211 = 4,
	AM_DTV_DEMOD_SI2168 = 5,
	AM_DTV_DEMOD_ITE9133 = 6,
	AM_DTV_DEMOD_ITE9173 = 7,
	AM_DTV_DEMOD_DIB8096 = 8,
	AM_DTV_DEMOD_ATBM8869 = 9,
	AM_DTV_DEMOD_MXL241 = 10,
	AM_DTV_DEMOD_AVL68xx = 11,
	AM_DTV_DEMOD_MXL683 = 12
};

enum aml_fe_dev_type_t {
	AM_DEV_TUNER,
	AM_DEV_ATV_DEMOD,
	AM_DEV_DTV_DEMOD
};

struct aml_fe_dev;
struct aml_fe;
struct aml_fe_drv {
	struct module *owner;
	struct aml_fe_drv *next;
	enum aml_tuner_type_t	id;
	char *name;
	int			capability;
	int (*init)(struct aml_fe_dev *dev);
	int (*release)(struct aml_fe_dev *dev);
	int (*resume)(struct aml_fe_dev *dev);
	int (*suspend)(struct aml_fe_dev *dev);
	int (*get_ops)(struct aml_fe_dev *dev, int mode,
					   void *ops);
	int (*enter_mode)(struct aml_fe *fe, int mode);
	int (*leave_mode)(struct aml_fe *fe, int mode);
	int			ref;
};

struct aml_fe_dev {
	/*point to parent aml_fe */
	struct aml_fe *fe;
	int			i2c_adap_id;
	int			i2c_addr;
	struct i2c_adapter *i2c_adap;
	int			reset_gpio;
	int			reset_value;
	struct aml_fe_drv *drv;
	wait_queue_head_t	lock_wq;
	void *priv_data;

	/*for tuner power control */
	int			tuner_power_gpio;
	/*for dtv dvbsx lnb power control */
	int			lnb_power_gpio;
	/*for ant overload control, */
	/*it possible in dtv dvbsx and depond on fe hw */
	int			antoverload_gpio;

	/*for mem reserved*/
	int			mem_start;
	int			mem_end;
	int			mem_size;
	int			cma_flag;
#ifdef CONFIG_CMA
	struct platform_device	*this_pdev;
	struct page			*venc_pages;
	unsigned int			cma_mem_size;/* BYTE */
	unsigned int			cma_mem_alloc;
#endif

	/*for dtv spectrum*/
	int			spectrum;
};
struct aml_demod_param {
	/*for tuner video if to amlatvdemod*/
	unsigned int	if_freq;  /*HZ*/
	/*for tuner output*/
	unsigned int	if_inv;
};

struct aml_fe {
	struct dvb_frontend *fe;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend		es;
#endif                          /*CONFIG_HAS_EARLYSUSPEND */
	spinlock_t			slock;
	int				init;
	int				mode;
	int				dev_id;
	int				capability;
	enum aml_ts_source_t		ts;
	struct aml_demod_param		demod_param;
	struct aml_fe_dev *tuner;
	struct aml_fe_dev *atv_demod;
	struct aml_fe_dev *dtv_demod;
	/*struct dvb_frontend_parameters params;*/
	struct dtv_frontend_properties	params;
};

struct aml_fe_man {
	struct aml_fe		fe[FE_DEV_COUNT];
	struct aml_fe_dev	tuner[FE_DEV_COUNT];
	struct aml_fe_dev	atv_demod[FE_DEV_COUNT];
	struct aml_fe_dev	dtv_demod[FE_DEV_COUNT];
	struct dvb_frontend	dev[FE_DEV_COUNT];
	struct pinctrl *pinctrl;
	struct platform_device *pdev;
};

extern int aml_register_fe_drv(enum aml_fe_dev_type_t	type,
			       struct aml_fe_drv *drv);
extern int aml_unregister_fe_drv(enum aml_fe_dev_type_t type,
				 struct aml_fe_drv *drv);
extern int aml_fe_suspend(struct platform_device *dev, pm_message_t state);
extern int aml_fe_resume(struct platform_device *dev);

extern struct dvb_frontend *get_si2177_tuner(void);
extern const char *soundsys_to_str(unsigned short soundsys);
extern const char *audmode_to_str(unsigned short soundsys);
extern const char *v4l2_std_to_str(v4l2_std_id std);
extern const char *fe_type_to_str(enum fe_type type);

extern int amlogic_gpio_name_map_num(const char *name);
extern int amlogic_gpio_direction_output(unsigned int pin, int value,
					 const char *owner);
extern int amlogic_gpio_request(unsigned int pin, const char *label);

/* vdac ctrl,adc/dac ref signal,cvbs out signal
 * module index: atv demod:0x01; dtv demod:0x02; tvafe:0x4; dac:0x8
*/
extern void vdac_enable(bool on, unsigned int module_sel);
extern void set_aft_thread_enable(int enable, u32_t delay);
#endif /*_AML_FE_H_*/
