/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _AML_SECURITY_KEY_H_
#define _AML_SECURITY_KEY_H_

/* internal return value*/
#define RET_OK		0
#define RET_EFAIL	1	/*not found*/
#define RET_EINVAL	2	/*name length*/
#define RET_EMEM	3	/*no enough memory*/
#define RET_EUND	0xff

#define SMC_UNK		-1

#define	BL31_STORAGE_QUERY		0x82000060
#define	BL31_STORAGE_READ		0x82000061
#define	BL31_STORAGE_WRITE		0x82000062
#define	BL31_STORAGE_TELL		0x82000063
#define	BL31_STORAGE_VERIFY		0x82000064
#define	BL31_STORAGE_STATUS		0x82000065
#define	BL31_STORAGE_LIST		0x82000067
#define	BL31_STORAGE_REMOVE		0x82000068
#define	BL31_STORAGE_IN			0x82000023
#define	BL31_STORAGE_OUT		0x82000024
#define	BL31_STORAGE_BLOCK		0x82000025
#define	BL31_STORAGE_SIZE		0x82000027
#define	BL31_STORAGE_SET_ENCTYPE	0x8200006A
#define	BL31_STORAGE_GET_ENCTYPE	0x8200006B
#define	BL31_STORAGE_VER		0x8200006C

struct bl31_storage_share_mem {
	void __iomem  *in;
	void __iomem  *out;
	void __iomem  *block;
	unsigned long size;
};

/* keyattr: 0: normal, 1: secure*/
unsigned long secure_storage_write(u8 *name, u8 *buf, u32 len, u32 attr);
unsigned long secure_storage_read(u8 *name, u8 *buf, u32 len, u32 *readlen);
unsigned long secure_storage_verify(u8 *name, u8 *hashbuf);
unsigned long secure_storage_query(u8 *name, u32 *retval);
unsigned long secure_storage_tell(u8 *name, u32 *retval);
unsigned long secure_storage_status(u8 *name, u32 *retval);
void *secure_storage_getbuf(u32 *size);
int __init security_key_init(struct platform_device *pdev);
#endif
