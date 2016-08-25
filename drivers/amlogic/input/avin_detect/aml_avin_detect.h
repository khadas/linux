#ifndef __AML_AVIN_DETECT_H
#define __AML_AVIN_DETECT_H

#include <linux/cdev.h>
#include <linux/workqueue.h>
#include <linux/timer.h>
#include <linux/mutex.h>
#include <linux/gpio/consumer.h>


#ifndef bool
#define bool unsigned char
#endif

#define INT_GPIO_0	96

enum avin_status_e {
	AVIN_STATUS_IN = 0,
	AVIN_STATUS_OUT = 1,
	AVIN_STATUS_UNKNOW = 2,
};
enum avin_channel_e {
	AVIN_CHANNEL1 = 0,
	AVIN_CHANNEL2 = 1,
	AVIN_CHANNEL3 = 2,
};

struct report_data_s {
	enum avin_channel_e channel;
	enum avin_status_e status;
};

struct dts_const_param_s {
	unsigned char dts_device_num;
	unsigned char dts_detect_times;
	unsigned char dts_fault_tolerance;
	unsigned char dts_interval_length;
};

/*
irq_falling_times[i][j]
i: number of avin device
j: the times of set_detect_times  --detect_channel_times
*/
struct code_variable_s {
	bool report_data_flag;
	bool *pin_mask_irq_flag;
	unsigned char first_time_into_loop;
	unsigned char *loop_detect_times;
	unsigned char *detect_channel_times;
	unsigned char *actual_into_irq_times;
	unsigned char *irq_falling_times;
	struct report_data_s *report_data_s;
	enum avin_status_e *ch_current_status;
};

struct hw_resource_s {
	int *irq_num;
	struct gpio_desc **pin;
};

struct avin_det_s {
	char config_name[20];
	dev_t  avin_devno;
	struct device *config_dev;
	struct class *config_class;
	struct cdev avin_cdev;
	struct dts_const_param_s dts_param;
	struct code_variable_s code_variable;
	struct hw_resource_s hw_res;
	struct input_dev *input_dev;
	struct timer_list timer;
	struct mutex lock;
	struct work_struct work_struct_update;
	struct work_struct work_struct_maskirq;
};

#endif

