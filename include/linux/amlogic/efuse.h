/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __EFUSE_AMLOGIC_H
#define __EFUSE_AMLOGIC_H

#define EFUSE_KEY_NAME_LEN	32

struct efusekey_info {
	char keyname[EFUSE_KEY_NAME_LEN];
	unsigned int offset;
	unsigned int size;
};

int efuse_getinfo(char *item, struct efusekey_info *info);
ssize_t efuse_user_attr_show(char *name, char *buf);
ssize_t efuse_user_attr_store(char *name, const char *buf, size_t count);
ssize_t efuse_user_attr_read(char *name, char *buf);

#endif
