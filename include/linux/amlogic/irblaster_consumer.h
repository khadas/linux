/*
 * include/linux/amlogic/irblaster_consumer.h
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#ifndef __LINUX_IRBLASTER_CONSUMER_H
#define __LINUX_IRBLASTER_CONSUMER_H

#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/of.h>
#ifdef CONFIG_AMLOGIC_IRBLASTER_PROTOCOL
#include <linux/amlogic/irblaster_encoder.h>
#endif

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

#ifdef CONFIG_AMLOGIC_IRBLASTER_PROTOCOL
int irblaster_send_key(struct irblaster_chip *chip,
		       unsigned int addr,
		       unsigned int commmand);
int irblaster_set_protocol(struct irblaster_chip *chip,
			   enum irblaster_protocol ir_protocol);
enum irblaster_protocol irblaster_get_protocol(struct irblaster_chip *chip);
#endif

#endif /* __LINUX_IRBLASTER_CONSUMER_H */
