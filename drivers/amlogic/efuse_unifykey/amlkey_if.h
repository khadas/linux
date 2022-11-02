/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __AMLKEY_IF_H__
#define __AMLKEY_IF_H__

struct amlkey_if {
	s32 (*init)(u8 *seed, u32 len, int encrypt_type);
	u32 (*exist)(const u8 *name);
	unsigned int (*size)(const u8 *name);
	u32 (*get_attr)(const u8 *name);
	unsigned int (*read)(const u8 *name, u8 *buffer, u32 len);
	ssize_t (*write)(const u8 *name, u8 *buffer, u32 len, u32 attr);
	s32 (*hash)(const u8 *name, u8 *hash);
};

extern struct amlkey_if *amlkey_if;

static inline s32 amlkey_init(u8 *seed, u32 len, int encrypt_type)
{
	return amlkey_if->init(seed, len, encrypt_type);
}

static inline u32 amlkey_exist(const u8 *name)
{
	return amlkey_if->exist(name);
}

static inline unsigned int amlkey_size(const u8 *name)
{
	return amlkey_if->size(name);
}

static inline u32 amlkey_get_attr(const u8 *name)
{
	return amlkey_if->get_attr(name);
}

static inline unsigned int amlkey_read(const u8 *name, u8 *buffer, u32 len)
{
	return amlkey_if->read(name, buffer, len);
}

static inline ssize_t amlkey_write(const u8 *name, u8 *buffer, u32 len,
				   u32 attr)
{
	return amlkey_if->write(name, buffer, len, attr);
}

static inline s32 amlkey_hash(const u8 *name, u8 *hash)
{
	return amlkey_if->hash(name, hash);
}

int amlkey_if_init(struct platform_device *pdev);
void amlkey_if_deinit(void);
#endif

