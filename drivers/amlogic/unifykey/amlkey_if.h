/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __AMLKEY_IF_H__
#define __AMLKEY_IF_H__

s32 amlkey_init(u8 *seed, u32 len, int encrypt_type);
u32 amlkey_exsit(const u8 *name);
unsigned int amlkey_size(const u8 *name);
u32 amlkey_get_attr(const u8 *name);
unsigned int amlkey_read(const u8 *name, u8 *buffer, u32 len);
ssize_t amlkey_write(const u8 *name, u8 *buffer, u32 len,
		     u32 attr);
s32 amlkey_hash(const u8 *name, u8 *hash);

#endif

