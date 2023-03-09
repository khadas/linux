/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * Copyright (C) 2022 Amlogic, Inc. All rights reserved.
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
#ifndef AML_MEDIA_INFO_H
#define AML_MEDIA_INFO_H

#ifndef __KERNEL__
#include <stdint.h>
#else
#include <linux/notifier.h>
#endif

/* max k/v len */
#define AMEDIA_INFO_KV_MAX_LEN 1023
#define AMEDIA_INFO_KEY_MAX_LEN 127

/* id should be the first field, dont add before it */
struct info_record {
	uint32_t id; /* -1 for a new record, otherwise update existed one */
	char kv[AMEDIA_INFO_KV_MAX_LEN + 1];
};

struct info_key {
	char k[AMEDIA_INFO_KEY_MAX_LEN + 1];
};

/* media_info ioctl */
#define _A_MI_S  'I'
/* set/update k/v pair */
#define AMI_IOC_SET			_IOWR((_A_MI_S), 0x00, struct info_record)
/* remove key */
#define AMI_IOC_DEL			_IOR((_A_MI_S), 0x01, struct info_record)
/* set poll for type=key */
#define AMI_IOC_POLL_SET_TYPE		_IOR((_A_MI_S), 0x02, struct info_key)

#ifdef __KERNEL__
int media_info_register_event(const char *key, struct notifier_block *nb);
int media_info_unregister_event(struct notifier_block *nb);
int media_info_post_kv(const char *kv, uint32_t *id);
#endif

#endif

