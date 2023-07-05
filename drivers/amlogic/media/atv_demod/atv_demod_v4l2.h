/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __ATV_DEMOD_V4L2_H__
#define __ATV_DEMOD_V4L2_H__

#include <linux/amlogic/aml_atvdemod.h>
#include <media/v4l2-device.h>
#include <dvb_frontend.h>

/** generic attach function. */
#if (defined CONFIG_AMLOGIC_ATV_DEMOD ||\
		defined CONFIG_AMLOGIC_ATV_DEMOD_MODULE)
#define v4l2_attach(FUNCTION, ARGS...) ({ \
	void *__r = NULL; \
	typeof(&FUNCTION) __a = symbol_request(FUNCTION); \
	if (__a) { \
		__r = (void *) __a(ARGS); \
		if (__r == NULL) \
			symbol_put(FUNCTION); \
	} else { \
		pr_err("V4L2_FE: Unable to find " \
				"symbol "#FUNCTION"()\n"); \
	} \
	__r; \
})

#define v4l2_detach(FUNCTION)    symbol_put_addr(FUNCTION)

#else

#define v4l2_attach(FUNCTION, ARGS...) ({ \
	FUNCTION(ARGS); \
})

#define v4l2_detach(FUNCTION)    {}

#endif /* CONFIG_MEDIA_ATTACH */


#define V4L2_FE_NO_EXIT 0
#define V4L2_FE_NORMAL_EXIT 1
#define V4L2_FE_DEVICE_REMOVED 2
#define V4L2_FE_DEVICE_RESUME 3

#define V4L2FE_STATE_IDLE 1
#define V4L2FE_STATE_RETUNE 2
#define V4L2FE_STATE_TUNING_FAST 4
#define V4L2FE_STATE_TUNING_SLOW 8
#define V4L2FE_STATE_TUNED 16
#define V4L2FE_STATE_ZIGZAG_FAST 32
#define V4L2FE_STATE_ZIGZAG_SLOW 64
#define V4L2FE_STATE_DISEQC 128
#define V4L2FE_STATE_ERROR 256
#define V4L2FE_STATE_WAITFORLOCK (V4L2FE_STATE_TUNING_FAST |\
		V4L2FE_STATE_TUNING_SLOW | V4L2FE_STATE_ZIGZAG_FAST |\
		V4L2FE_STATE_ZIGZAG_SLOW | V4L2FE_STATE_DISEQC)
#define V4L2FE_STATE_SEARCHING_FAST (V4L2FE_STATE_TUNING_FAST |\
		V4L2FE_STATE_ZIGZAG_FAST)
#define V4L2FE_STATE_SEARCHING_SLOW (V4L2FE_STATE_TUNING_SLOW |\
		V4L2FE_STATE_ZIGZAG_SLOW)
#define V4L2FE_STATE_LOSTLOCK (V4L2FE_STATE_ZIGZAG_FAST |\
		V4L2FE_STATE_ZIGZAG_SLOW)

#define V4L2_IOCTL_MAX_MSGS 64

#define V4L2_SET_FRONTEND    _IOW('V', 105, struct v4l2_analog_parameters)
#define V4L2_GET_FRONTEND    _IOR('V', 106, struct v4l2_analog_parameters)
#define V4L2_GET_EVENT       _IOR('V', 107, struct v4l2_frontend_event)
#define V4L2_SET_MODE        _IOW('V', 108, int)
#define V4L2_READ_STATUS     _IOR('V', 109, enum v4l2_status)
#define V4L2_SET_PROPERTY    _IOWR('V', 110, struct v4l2_properties)
#define V4L2_GET_PROPERTY    _IOWR('V', 111, struct v4l2_properties)
#define V4L2_DETECT_TUNE     _IOR('V', 112, struct v4l2_tune_status)
#define V4L2_DETECT_STANDARD _IO('V', 113)

#define ANALOG_FLAG_ENABLE_AFC          0x00000001
#define ANALOG_FLAG_MANUL_SCAN          0x00000011

#define V4L2_UNDEFINED           0
#define V4L2_TUNE                1
#define V4L2_SOUND_SYS           2
#define V4L2_SLOW_SEARCH_MODE    3
#define V4L2_FREQUENCY           4
#define V4L2_STD                 5
#define V4L2_FINE_TUNE           6
#define V4L2_SIF_OVER_MODULATION 7
#define V4L2_TUNER_TYPE          8
#define V4L2_TUNER_IF_FREQ       9
#define V4L2_AFC                 10
#define V4L2_ENABLE_AFC          11
#define V4L2_AFC_STATE           12

#define V4L2_MAX_COMMAND         V4L2_AFC

struct v4l2_frontend;

struct v4l2_analog_parameters {
	unsigned int frequency;
	unsigned int audmode;

	/* soundsys & 0xff0000: A2,BTSC,EIAJ,NICAM.
	 * soundsys & 0xff00: signal input mode.
	 * soundsys & 0xff: output mode.
	 */
	unsigned int soundsys;

	/* std & 0xff000000: PAL/NTSC/SECAM.
	 * std & 0x00ffffff: CVBS format.
	 */
	v4l2_std_id std;
	unsigned int flag; /* for search or play */
	unsigned int afc_range;
	unsigned int reserved;
};

struct v4l2_tune_status {
	unsigned char lock; /* unlocked: 0, locked: 1 */
	v4l2_std_id std;
	unsigned int audmode;
	int snr;
	int afc; /* KHz */
	union {
		void *resrvred;
		__u64 reserved1;
	};
};

enum v4l2_status {
	V4L2_HAS_SIGNAL  = 0x01, /* found something above the noise level */
	V4L2_HAS_CARRIER = 0x02, /* found a DVB signal */
	V4L2_HAS_VITERBI = 0x04, /* FEC is stable  */
	V4L2_HAS_SYNC    = 0x08, /* found sync bytes  */
	V4L2_HAS_LOCK    = 0x10, /* everything's working... */
	V4L2_TIMEDOUT    = 0x20, /* no lock within the last ~2 seconds */
	V4L2_REINIT      = 0x40, /* frontend was reinitialized, */
};				/* application is recommended to reset */
				/* DiSEqC, tone and parameters */

struct v4l2_property {
	unsigned int cmd;
	unsigned int data;
	int result;
};

struct v4l2_properties {
	unsigned int num;
	union {
		struct v4l2_property *props;
		__u64 reserved;
	};
};

enum v4l2_search {
	V4L2_SEARCH_SUCCESS = (1 << 0),
	V4L2_SEARCH_ASLEEP  = (1 << 1),
	V4L2_SEARCH_FAILED  = (1 << 2),
	V4L2_SEARCH_INVALID = (1 << 3),
	V4L2_SEARCH_AGAIN   = (1 << 4),
	V4L2_SEARCH_ERROR   = (1 << 31),
};

struct v4l2_frontend_event {
	enum v4l2_status status;
	struct v4l2_analog_parameters parameters;
};

#define MAX_EVENT 8

struct v4l2_fe_events {
	struct v4l2_frontend_event events[MAX_EVENT];
	int eventw;
	int eventr;
	int overflow;
	wait_queue_head_t wait_queue;
	struct mutex mtx;
};

struct v4l2_frontend_private {
	struct v4l2_atvdemod_device *v4l2dev;

	struct v4l2_fe_events events;
	struct semaphore sem;
	struct list_head list_head;
	wait_queue_head_t wait_queue;
	struct task_struct *thread;
	unsigned long release_jiffies;
	unsigned int exit;
	unsigned int wakeup;
	enum v4l2_status status;
	unsigned int delay;

	unsigned int state;
	enum v4l2_search algo_status;

	unsigned int tune_needexit;
};

struct v4l2_adapter {
	struct device *dev;

	struct dvb_frontend fe;
};

struct v4l2_frontend_ops {
	int (*set_property)(struct v4l2_frontend *v4l2_fe,
			struct v4l2_property *tvp);
	int (*get_property)(struct v4l2_frontend *v4l2_fe,
			struct v4l2_property *tvp);

	/* for signal one shot search, return lock status and afc value */
	int (*tune)(struct v4l2_frontend *v4l2_fe,
			struct v4l2_tune_status *status);

	/* for auto standard detection */
	int (*detect)(struct v4l2_frontend *v4l2_fe);

	/*
	 * These callbacks are for devices that implement their own
	 * tuning algorithms, rather than a simple tune.
	 */
	enum v4l2_search (*search)(struct v4l2_frontend *v4l2_fe);
};

struct v4l2_frontend {
	struct device *dev;

	struct dvb_frontend fe;

	enum v4l2_tuner_type mode;

	void *frontend_priv;
	void *tuner_priv;
	void *analog_demod_priv;

	struct v4l2_analog_parameters params;

	struct v4l2_frontend_ops ops;

	/* Used for asynchronous exit of tune threads */
	int (*async_tune_needexit)(struct v4l2_frontend *v4l2_fe);
};

struct v4l2_atvdemod_device {
	char *name;
	struct class *clsp;
	struct device *dev;

	struct v4l2_device v4l2_dev;
	struct video_device *video_dev;

	struct mutex lock;
};

int v4l2_resister_frontend(struct v4l2_frontend *v4l2_fe);
int v4l2_unregister_frontend(struct v4l2_frontend *v4l2_fe);

void v4l2_frontend_detach(struct v4l2_frontend *v4l2_fe);
int v4l2_frontend_shutdown(struct v4l2_frontend *v4l2_fe);
int v4l2_frontend_suspend(struct v4l2_frontend *v4l2_fe);
int v4l2_frontend_resume(struct v4l2_frontend *v4l2_fe);

#endif // __ATV_DEMOD_V4L2_H__
