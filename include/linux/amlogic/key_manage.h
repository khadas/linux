/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __KEYMANAGE__
#define __KEYMANAGE__

enum {
	UNIFYKEY_STORAGE_TYPE_INVALID = 0,
	UNIFYKEY_STORAGE_TYPE_EMMC,
	UNIFYKEY_STORAGE_TYPE_NAND,
	UNIFYKEY_STORAGE_TYPE_MAX
};

struct unifykey_storage_ops {
	s32 (*read)(u8 *buf, u32 len, u32 *actual_len);
	s32 (*write)(u8 *buf, u32 len, u32 *actual_len);
};

struct unifykey_type {
	u32 storage_type;
	struct unifykey_storage_ops *ops;
};

int register_unifykey_types(struct unifykey_type *uk_type);
void auto_attach(void);

#endif /*__KEYMANAGE__*/
