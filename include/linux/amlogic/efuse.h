/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __EFUSE_AMLOGIC_H
#define __EFUSE_AMLOGIC_H

#define EFUSE_KEY_NAME_LEN	32

#define EFUSE_READ_CALI_ITEM		0x8200003E
#define EFUSE_CALI_SUBITEM_WHOBURN	0x100
#define EFUSE_CALI_SUBITEM_SENSOR0	0x101
#define EFUSE_CALI_SUBITEM_SARADC	0x102
#define EFUSE_CALI_SUBITEM_USBPHY	0x103
#define EFUSE_CALI_SUBITEM_MIPICSI	0x104
#define EFUSE_CALI_SUBITEM_HDMIRX	0x105
#define EFUSE_CALI_SUBITEM_ETHERNET	0x106
#define EFUSE_CALI_SUBITEM_CVBS		0x107
#define EFUSE_CALI_SUBITEM_EARCRX	0x108
#define EFUSE_CALI_SUBITEM_EARCTX	0x109
#define EFUSE_CALI_SUBITEM_USBCCLOGIC	0x10A
#define EFUSE_CALI_SUBITEM_BC		0x10B

#define EFUSE_LOCK_SUBITEM_BASE          0x1000
#define EFUSE_LOCK_SUBITEM_DGPK1_KEY     0x1000
#define EFUSE_LOCK_SUBITEM_DGPK2_KEY     0x1001
#define EFUSE_LOCK_SUBITEM_AUDIO_V_ID    0x1002
#define EFUSE_LOCK_SUBITEM_MAX           0X1FFF

struct efusekey_info {
	char keyname[EFUSE_KEY_NAME_LEN];
	unsigned int offset;
	unsigned int size;
};

int efuse_getinfo(char *item, struct efusekey_info *info);
ssize_t efuse_user_attr_show(char *name, char *buf);
ssize_t efuse_user_attr_store(char *name, const char *buf, size_t count);
ssize_t efuse_user_attr_read(char *name, char *buf);

int efuse_amlogic_cali_item_read(unsigned int item);
int efuse_amlogic_check_lockable_item(unsigned int item);

#endif
