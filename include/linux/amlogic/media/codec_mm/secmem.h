/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _LINUX_SECMEM_H_
#define _LINUX_SECMEM_H_

#include <linux/ioctl.h>
#include <linux/types.h>

struct secmem_block {
	__u32 paddr;
	__u32 size;
	__u32 handle;
};

#define SECMEM_IOC_MAGIC		'S'

#define SECMEM_EXPORT_DMA		_IOWR(SECMEM_IOC_MAGIC, 0x01, int)
#define SECMEM_GET_HANDLE		_IOWR(SECMEM_IOC_MAGIC, 0x02, int)
#define SECMEM_GET_PHYADDR		_IOWR(SECMEM_IOC_MAGIC, 0x03, int)
#define SECMEM_IMPORT_DMA		_IOWR(SECMEM_IOC_MAGIC, 0x04, int)

#endif /* _LINUX_SECMEM_H_ */
