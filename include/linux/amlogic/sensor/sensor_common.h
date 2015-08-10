#ifndef SENSOR_COMMON_H
#define SENSOR_COMMON_H

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/earlysuspend.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/kthread.h>
#include <linux/input/mt.h>
#include <linux/vmalloc.h>
#include <linux/hrtimer.h>
#include <linux/syscalls.h>
#include <linux/reboot.h>
#include <linux/proc_fs.h>
#ifdef CONFIG_OF
#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/of.h>
#else
#include <mach/gpio.h>
#include <mach/gpio_data.h>
#endif

int sensor_setup_i2c_dev(struct i2c_board_info *i2c_info,
	int *i2c_bus_nr, int *gpio);

struct sensor_pdata_t {
	struct i2c_client *client;
	int acc_negate_x;
	int acc_negate_y;
	int acc_negate_z;
	int acc_swap_xy;

	int mag_negate_x;
	int mag_negate_y;
	int mag_negate_z;
	int mag_swap_xy;

	int gyr_negate_x;
	int gyr_negate_y;
	int gyr_negate_z;
	int gyr_swap_xy;
};

void aml_sensor_report_acc(struct i2c_client *client, struct input_dev *dev,
	int x, int y, int z);
void aml_sensor_report_mag(struct i2c_client *client, struct input_dev *dev,
	int x, int y, int z);
void aml_sensor_report_gyr(struct i2c_client *client, struct input_dev *dev,
	int x, int y, int z);

#endif
