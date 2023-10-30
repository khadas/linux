/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __VPQ_IOCTL_H__
#define __VPQ_IOCTL_H__

#include <linux/fs.h>

int vpq_ioctl_process(struct file *file, unsigned int cmd, unsigned long arg);

#endif //__VPQ_IOCTL_H__
