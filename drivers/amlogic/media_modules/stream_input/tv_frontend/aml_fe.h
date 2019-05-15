/*
* Copyright (C) 2017 Amlogic, Inc. All rights reserved.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful, but WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
* FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
* more details.
*
* You should have received a copy of the GNU General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*
* Description:
*/
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

#include <dvbdev.h>
#include <demux.h>
#include <dvb_demux.h>
#include <dmxdev.h>
#include <dvb_filter.h>
#include <dvb_net.h>
#include <dvb_ringbuffer.h>
#include <dvb_frontend.h>

#include <linux/videodev2.h>
#include <linux/amlogic/aml_gpio_consumer.h>

#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/pinctrl/consumer.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/of_reserved_mem.h>

#include "../parser/hw_demux/aml_dvb.h"

enum aml_tuner_type_t {
	AM_TUNER_SI2176 = 1,
	AM_TUNER_SI2196 = 2,
	AM_TUNER_FQ1216 = 3,
	AM_TUNER_HTM    = 4,
	AM_TUNER_CTC703 = 5,
	AM_TUNER_SI2177 = 6,
	AM_TUNER_R840   = 7,
	AM_TUNER_SI2157 = 8,
	AM_TUNER_SI2151 = 9,
	AM_TUNER_MXL661 = 10,
	AM_TUNER_MXL608 = 11,
	AM_TUNER_SI2159 = 12,
	AM_TUNER_R842 = 13,
};

enum aml_atv_demod_type_t {
	AM_ATV_DEMOD_SI2176 = 1,
	AM_ATV_DEMOD_SI2196 = 2,
	AM_ATV_DEMOD_FQ1216 = 3,
	AM_ATV_DEMOD_HTM    = 4,
	AM_ATV_DEMOD_CTC703 = 5,
	AM_ATV_DEMOD_SI2177 = 6,
	AM_ATV_DEMOD_AML    = 7,
	AM_ATV_DEMOD_R840   = 8
};

enum aml_dtv_demod_type_t {
	AM_DTV_DEMOD_M1       = 0,
	AM_DTV_DEMOD_SI2176   = 1,
	AM_DTV_DEMOD_MXL101   = 2,
	AM_DTV_DEMOD_SI2196   = 3,
	AM_DTV_DEMOD_AVL6211  = 4,
	AM_DTV_DEMOD_SI2168   = 5,
	AM_DTV_DEMOD_ITE9133  = 6,
	AM_DTV_DEMOD_ITE9173  = 7,
	AM_DTV_DEMOD_DIB8096  = 8,
	AM_DTV_DEMOD_ATBM8869 = 9,
	AM_DTV_DEMOD_MXL241   = 10,
	AM_DTV_DEMOD_AVL68xx  = 11,
	AM_DTV_DEMOD_MXL683   = 12,
	AM_DTV_DEMOD_ATBM8881 = 13
};

enum aml_fe_dev_type_t {
	AM_DEV_TUNER,
	AM_DEV_ATV_DEMOD,
	AM_DEV_DTV_DEMOD
};

struct aml_fe_dev;
struct aml_fe_man;
struct aml_fe;

struct aml_fe_drv {
	struct module          *owner;
	struct aml_fe_drv      *next;
	enum aml_tuner_type_t	id;
	char                   *name;
	int (*init)(struct aml_fe_dev *dev);
	int (*release)(struct aml_fe_dev *dev);
	int (*resume)(struct aml_fe_dev *dev);
	int (*suspend)(struct aml_fe_dev *dev);
	int (*support)(struct aml_fe_dev *dev, enum fe_delivery_system sys);
	int (*get_ops)(struct aml_fe_dev *dev, enum fe_delivery_system sys,
				struct dvb_frontend_ops  *ops);
	int (*init_sys)(struct aml_fe_dev *dev, enum fe_delivery_system sys);
	int (*release_sys)(struct aml_fe_dev *dev, enum fe_delivery_system sys);
	int (*start_blind_scan)(struct aml_fe_dev *dev);
	int (*stop_blind_scan)(struct aml_fe_dev *dev);
	int			ref;
};

struct aml_fe_dev {
	/*point to parent aml_fe */
	enum aml_fe_dev_type_t  type;
	int                     dev_id;
	struct aml_fe           *fe;
	struct aml_fe_man       *man;
	struct aml_fe_drv       *drv;
	void                    *priv_data;
	/*i2c and reset gpio for all demod and tune*/
	int						i2c_adap_id;
	int						i2c_addr;
	struct i2c_adapter		*i2c_adap;
	int						reset_gpio;
	int						reset_value;
};

struct aml_fe {
	struct dvb_frontend            *fe;
	struct aml_fe_man              *man;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend		es;
#endif                          /*CONFIG_HAS_EARLYSUSPEND */
	spinlock_t			slock;
	int				init;
	int				dev_id;
	enum fe_delivery_system         sys;
	int	sub_sys;
/*used to identify T T2 OR C-A C-B C-C,S S2,ISDBT ISDBS ISDBC*/
	enum aml_ts_source_t		ts;
	struct aml_fe_dev              *tuner;
	struct aml_fe_dev              *atv_demod;
	struct aml_fe_dev              *dtv_demod;
	struct workqueue_struct        *work_q;
	wait_queue_head_t               wait_q;
	struct work_struct              work;
	int                             work_running;
	struct dvbsx_blindscanpara		blind_scan_para;

	/*Driver's work function.*/
	void (*do_work)(struct aml_fe *fe);
	/*Driver's property function.*/
	int (*get_property)(struct dvb_frontend *fe, struct dtv_property *tvp);
	int (*set_property)(struct dvb_frontend *fe, struct dtv_property *tvp);
};

struct aml_fe_man {
	struct aml_dvb         *dvb;
	struct aml_fe		fe[FE_DEV_COUNT];
	struct aml_fe_dev	tuner[FE_DEV_COUNT];
	struct aml_fe_dev	atv_demod[FE_DEV_COUNT];
	struct aml_fe_dev	dtv_demod[FE_DEV_COUNT];
	struct dvb_frontend	dev[FE_DEV_COUNT];
	struct pinctrl         *pinctrl;
	struct platform_device *pdev;
};

extern int aml_register_fe_drv(enum aml_fe_dev_type_t type,
			struct aml_fe_drv *drv);
extern int aml_unregister_fe_drv(enum aml_fe_dev_type_t type,
			struct aml_fe_drv *drv);

extern int aml_fe_of_property_string(struct aml_fe_dev *dev,
			const char *name, const char **str);
extern int aml_fe_of_property_u32(struct aml_fe_dev *dev,
			const char *name, u32 *v);

extern void  aml_fe_set_pdata(struct aml_fe_dev *dev, void *pdata);
extern void *aml_fe_get_pdata(struct aml_fe_dev *dev);

extern void aml_fe_schedule_work(struct aml_fe *fe,
			void (*func)(struct aml_fe *fe));
extern void aml_fe_cancel_work(struct aml_fe *fe);
extern int  aml_fe_work_cancelled(struct aml_fe *fe);
extern int  aml_fe_work_sleep(struct aml_fe *fe, unsigned long delay);

#endif /*_AML_FE_H_*/
