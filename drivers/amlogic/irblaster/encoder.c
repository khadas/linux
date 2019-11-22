/*
 * drivers/amlogic/irblaster/encoder.c
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

#include <linux/module.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/amlogic/irblaster.h>
#include <linux/amlogic/irblaster_encoder.h>

static int irblaster_raw_gen_pulse_space(unsigned int *data,
					 unsigned int *max,
					 unsigned int pulse_width,
					 unsigned int space_width,
					 unsigned int len)
{
	if (!*max)
		return -ENOBUFS;
	data[len - *max] = pulse_width;
	if (!--*max)
		return -ENOBUFS;
	data[len - *max] = space_width;
	--*max;

	return 0;
}

/**
 * irblaster_raw_gen() - Encode data to raw events with pulse-length modulation.
 * @data:	Pointer to data
 * @timings:	Pulse distance modulation timings.
 * @raw:	Data bits to encode.
 * @Returns:	buff len on success.
 *
 * Encodes the @n least significant bits of @data using space-distance
 * modulation with the timing characteristics described by @timings, writing up
 * to data using the *data pointer.
 */
int irblaster_raw_gen(unsigned int *data,
		      const struct irblaster_raw_timings *timings,
		      u32 raw)
{
	int i, ret, max;
	int len = timings->data_size;
	unsigned int space;

	max = len;
	if (timings->header_pulse) {
		ret = irblaster_raw_gen_pulse_space(data, &max,
						    timings->header_pulse,
						    timings->header_space,
						    len);
		if (ret)
			return ret;
	}

	if (timings->msb_first) {
		for (i = timings->raw_nbits - 1; i >= 0; --i) {
			space = timings->bit_space[(raw >> i) & 1];
			ret = irblaster_raw_gen_pulse_space(data, &max,
							    timings->bit_pulse,
							    space, len);
			if (ret)
				return ret;
		}
	} else {
		for (i = 0; i < timings->raw_nbits; ++i, raw >>= 1) {
			space = timings->bit_space[raw & 1];
			ret = irblaster_raw_gen_pulse_space(data, &max,
							    timings->bit_pulse,
							    space, len);
			if (ret)
				return ret;
		}
	}

	ret = irblaster_raw_gen_pulse_space(data, &max,
					    timings->trailer_pulse,
					    timings->trailer_space,
					    len);
	if (ret)
		return ret;

	return len;
}

unsigned int protocol_show_select(struct irblaster_chip *chip, char *buf)
{
	struct irblaster_raw_handler *protocol;
	unsigned int len = 0;

	mutex_lock(&irblaster_raw_handler_lock);
	list_for_each_entry(protocol, &irblaster_raw_handler_list, list) {
		if (chip->protocol &&
		    chip->state.protocol == protocol->protocol)
			len += scnprintf(buf + len, PAGE_SIZE - len, "[%s] ",
					 protocol->name);
		else
			len += scnprintf(buf + len, PAGE_SIZE - len, "%s ",
					 protocol->name);
	}

	len += scnprintf(len + buf, PAGE_SIZE - len, "\n");
	mutex_unlock(&irblaster_raw_handler_lock);

	return len;
}

unsigned int protocol_store_select(const char *buf)
{
	struct irblaster_raw_handler *protocol;

	mutex_lock(&irblaster_raw_handler_lock);
	list_for_each_entry(protocol, &irblaster_raw_handler_list, list) {
		if (sysfs_streq(buf, protocol->name)) {
			mutex_unlock(&irblaster_raw_handler_lock);
			return protocol->protocol;
		}
	}
	mutex_unlock(&irblaster_raw_handler_lock);

	return 0;
}

/**
 * irblaster_raw_handler_register()
 * - register a new raw_handle
 * @ir_raw_handler: the raw_handle to add
 * @Returns: 0 on success
 */
int irblaster_raw_handler_register(struct irblaster_raw_handler *ir_raw_handler)
{
	mutex_lock(&irblaster_raw_handler_lock);
	list_add_tail(&ir_raw_handler->list, &irblaster_raw_handler_list);
	mutex_unlock(&irblaster_raw_handler_lock);

	return 0;
}
EXPORT_SYMBOL(irblaster_raw_handler_register);

/**
 * irblaster_raw_handler_unregister()
 * - unregister a raw_handle
 * @ir_raw_handler: the raw_handle to remove
 */
void irblaster_raw_handler_unregister(struct irblaster_raw_handler
				      *ir_raw_handler)
{
	mutex_lock(&irblaster_raw_handler_lock);
	list_del(&ir_raw_handler->list);
	mutex_unlock(&irblaster_raw_handler_lock);
}
EXPORT_SYMBOL(irblaster_raw_handler_unregister);

/**
 * irblaster_send_key() - send key with addr and commmand
 * @chip: irblaster controller
 * @addr: remote control ID
 * @commmand: key
 */
int irblaster_send_key(struct irblaster_chip *chip, unsigned int addr,
		       unsigned int commmand)
{
	int ret;
	unsigned int *data;

	if (!chip)
		return -EINVAL;

	if (chip->ops->send_key) {
		ret = chip->ops->send_key(chip, addr, commmand);
		if (ret) {
			pr_err("%s(): irblaster_send fail\n",
			       __func__);
			return -EINVAL;
		}
	} else {
		data = kzalloc(sizeof(uint32_t) * MAX_PLUSE, GFP_KERNEL);
		if (!data)
			return -ENOMEM;

		if (chip->protocol->encode) {
			ret = chip->protocol->encode(chip->state.protocol,
						     addr, commmand, data);
			if (ret <= 0) {
				pr_err("%s(): irblaster encode fail\n",
				       __func__);
				goto err;
			}
		} else {
			pr_err("%s(): irblaster func %s not found\n",
			       __func__, __func__);
			goto err;
		}

		ret = irblaster_send(chip, data,
				     chip->protocol->timing->data_size);
		if (ret) {
			pr_err("%s(): irblaster_send fail\n", __func__);
			goto err;
		}

		kfree(data);
	}

	return 0;
err:
	kfree(data);
	return -EINVAL;
}

/**
 * irblaster_set_protocol() - set irblaster protocol
 * @chip: irblaster controller
 * @ir_protocol: irblaster protocol
 */
int irblaster_set_protocol(struct irblaster_chip *chip,
			   enum irblaster_protocol ir_protocol)
{
	struct irblaster_raw_handler *protocol;

	if (!chip || ir_protocol < 0 || ir_protocol >= IRBLASTER_PROTOCOL_MAX)
		return -EINVAL;

	mutex_lock(&irblaster_raw_handler_lock);

	list_for_each_entry(protocol, &irblaster_raw_handler_list, list)
		if (protocol->protocol == ir_protocol) {
			chip->state.protocol = ir_protocol;
			chip->protocol = protocol;
			mutex_unlock(&irblaster_raw_handler_lock);
			return 0;
		}

	mutex_unlock(&irblaster_raw_handler_lock);
	pr_err("%s(): irblaster protocol is not found\n", __func__);

	return -EINVAL;
}

/**
 * irblaster_get_protocol() - get irblaster protocol
 * @chip: irblaster controller
 */
enum irblaster_protocol irblaster_get_protocol(struct irblaster_chip *chip)
{
	if (!chip)
		return -EINVAL;

	return chip->state.protocol;
}
