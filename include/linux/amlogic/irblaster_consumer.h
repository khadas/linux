/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __LINUX_IRBLASTER_CONSUMER_H
#define __LINUX_IRBLASTER_CONSUMER_H

#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/of.h>

struct irblaster_chip;

/* irblaster user APIs */
int irblaster_send(struct irblaster_chip *chip,
		   unsigned int *data,
		   unsigned int len);
int irblaster_set_freq(struct irblaster_chip *chip,
		       unsigned int freq);
unsigned int irblaster_get_freq(struct irblaster_chip *chip);
int irblaster_set_duty(struct irblaster_chip *chip, unsigned int duty);
unsigned int irblaster_get_duty(struct irblaster_chip *chip);
struct irblaster_chip *of_irblaster_get(struct device_node *np,
					const char *con_id);
struct irblaster_chip *devm_of_irblaster_get(struct device *dev,
					     struct device_node *np,
					     const char *con_id);
void irblaster_put(struct irblaster_chip *chip);

#endif /* __LINUX_IRBLASTER_CONSUMER_H */
