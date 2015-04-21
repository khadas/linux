#ifndef __AML_SARADC_H__
#define __AML_SARADC_H__


#define SARADC_DEV_NUM 1

enum {
	CHAN_0 = 0,
	CHAN_1,
	CHAN_2,
	CHAN_3,
	CHAN_4,
	CHAN_5,
	CHAN_6,
	CHAN_7,
	SARADC_CHAN_NUM,
};

extern int get_adc_sample(int dev_id, int ch);

#endif
