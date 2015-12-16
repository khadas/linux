#ifndef __AML_AVIN_DETECT_H
#define __AML_AVIN_DETECT_H

#include <linux/cdev.h>
#include <linux/workqueue.h>
#include <linux/timer.h>

enum avin_status_e {
	AVIN_STATUS_IN = 0,
	AVIN_STATUS_OUT = 1,
	AVIN_STATUS_UNKNOW = 2,
};
enum avin_channel_e {
	AVIN_CHANNEL1 = 0,
	AVIN_CHANNEL2 = 1,
};

#define INT_GPIO_0	96

struct report_data_s {
	enum avin_channel_e channel;
	enum avin_status_e status;
};

struct avin_det_s {
	char config_name[20];
	dev_t  avin_devno;
	struct device *config_dev;
	struct class *config_class;
	struct cdev avin_cdev;
	struct report_data_s report_data_s[2];
	unsigned int report_data_flag;
	enum avin_status_e ch1_current_status;
	enum avin_status_e ch2_current_status;
	unsigned int irq_num1;
	unsigned int irq_num2;
	struct gpio_desc *pin1;
	struct gpio_desc *pin2;
	unsigned int set_detect_times;
	unsigned int set_fault_tolerance;
	unsigned int detect_channel1_times;
	unsigned int detect_channel2_times;
	unsigned int *irq1_falling_times;
	unsigned int *irq2_falling_times;
	unsigned int first_time_into_loop;
	unsigned int detect_interval_length;
	struct input_dev *input_dev;
	struct timer_list timer;
	struct work_struct work_update1;
	struct work_struct work_update2;
};

#endif

