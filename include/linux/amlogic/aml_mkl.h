/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _AML_MKL_H_
#define _AML_MKL_H_

#include <linux/types.h>

#ifndef __KERNEL__
#define __user
#endif

/**
 * Key-Ladder parameters
 */
enum amlkl_algo {
	KL_ALGO_TDES = 0,
	KL_ALGO_AES = 1,
	KL_ALGO_MAX,
};

struct amlkl_usage {
	__u32 crypto;
	__u32 algo;
	__u32 uid;
};

struct amlkl_params {
	__u32 kt_handle;
	__u8 levels;
	__u8 secure_level;
	struct amlkl_usage usage;
	__u8 module_id;
	__u8 kl_algo;
	__u8 kl_mode;
	__u8 mrk_cfg_index;
	__u8 kl_num;
	__u8 func_id;
	__u8 eks[6][16];
	__u8 reserved[16];
};

#define AML_MKL_IOCTL_RUN _IOW('L', 1, struct amlkl_params)

#endif
