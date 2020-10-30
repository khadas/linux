/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __LINUX_IRBLASTER_H
#define __LINUX_IRBLASTER_H

#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/of.h>

#define MAX_PLUSE 256
#define PS_SIZE 10

extern int irblaster_debug;
struct irblaster_chip;

static DEFINE_MUTEX(irblaster_lock);
static LIST_HEAD(irblaster_chips);

/**
 * enum irblaster_idle - Whether the controller is occupied
 * @IRBLASTER_EXPORTED: Controlled by the sysfs sys/class/irblaster
 * @IRBLASTER_REQUESTED: Controlled by consumer driver
 */
enum irblaster_idle {
	IRBLASTER_EXPORTED,
	IRBLASTER_REQUESTED
};

/**
 * struct irblaster_ops - irblaster controller operations
 * @send: send raw level data
 * @send_key: send key according to the protocol
 * @set_freq: set irblaster freq
 * @get_freq: get irblaster freq
 * @set_duty: set irblaster duty
 * @get_duty: get irblaster duty
 */
struct irblaster_ops {
	int (*send)(struct irblaster_chip *chip,
		    unsigned int *data, unsigned int len);
	int (*set_freq)(struct irblaster_chip *chip, unsigned int freq);
	unsigned int (*get_freq)(struct irblaster_chip *chip);
	int (*set_duty)(struct irblaster_chip *chip, unsigned int duty);
	unsigned int (*get_duty)(struct irblaster_chip *chip);
};

struct irblaster_state {
	unsigned int freq;
	unsigned int duty;
	int enabled;
	int idle;
};

/**
 * struct irblaster_chip - abstract a irblaster controller
 * @dev: device providing the irblaster
 * @list: list node for internal use
 * @ops: callbacks for this irblaster controller
 * @base: number of first irblaster controlled by this chip
 * @of_irblaster_n_cells: number of cells expected in the device tree
 * irblaster specifier
 * @state: irblaster controller status
 * @buffer: data
 * @buffer_len: data len
 * @sum_time: total time
 * @request: whether the controller is occupied
 */
struct irblaster_chip {
	struct device *dev;
	struct list_head list;
	struct irblaster_ops *ops;
	struct irblaster_state state;
	struct mutex sys_lock; /* use to sysfs */
	unsigned int base;
	unsigned int of_irblaster_n_cells;
	/* unsigned int buffer[MAX_PLUSE]; */
	unsigned int *buffer;
	unsigned int buffer_len;
	unsigned int sum_time;
	atomic_t request;
};

/* irblaster sysfs APIs */
void irblasterchip_sysfs_export(struct irblaster_chip *chip);
void irblasterchip_sysfs_unexport(struct irblaster_chip *chip);
int irblaster_sysfs_init(void);

/* irblaster provider APIs */
int irblasterchip_add(struct irblaster_chip *chip);
int irblasterchip_remove(struct irblaster_chip *chip);
void irblaster_chip_data_clear(struct irblaster_chip *chip);

#endif /* __LINUX_IRBLASTER_H */
