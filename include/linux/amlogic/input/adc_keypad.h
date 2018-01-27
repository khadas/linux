#ifndef __LINUX_ADC_KEYPAD_H
#define __LINUX_ADC_KEYPAD_H
#include <linux/list.h>
#include <linux/input.h>
#include <linux/kobject.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/spinlock.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#define DRIVE_NAME "adckey"
#define MAX_NAME_LEN 20

enum TOLERANCE_RANGE {
	TOL_MIN = 0,
	TOL_MAX = 255
};

enum SAMPLE_VALUE_RANGE {
	SAM_MIN = 0,
	SAM_MAX = 4095 /*12bit adc*/
};

struct adc_key {
	char name[MAX_NAME_LEN];
	unsigned int chan;
	unsigned int code;  /* input key code */
	int value; /* voltage/3.3v * 1023 */
	int tolerance;
	struct list_head list;
};

struct kp {
	unsigned char chan[SARADC_CHAN_NUM];
	unsigned char chan_num;   /*number of channel exclude duplicate*/
	unsigned char count;
	unsigned int report_code;
	unsigned int code;
	unsigned int poll_period; /*key scan period*/
	spinlock_t kp_lock;
	struct list_head adckey_head;
	struct kobject *adckey_kobj;
	struct input_dev *input;
	struct timer_list timer;
	struct work_struct work_update;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
};

#endif
