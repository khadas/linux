/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _AML_MKL_H_
#define _AML_MKL_H_

#include <linux/types.h>

/**
 * Key-Ladder parameters
 */

/* key ladder level */
#define AML_KL_LEVEL_3 (3)
#define AML_KL_LEVEL_4 (4)
#define AML_KL_LEVEL_5 (5)
#define AML_KL_LEVEL_6 (6)

#define MSR_KL_LEVEL_1 (1)
#define MSR_KL_LEVEL_2 (2)
#define MSR_KL_LEVEL_3 (3)

/* key ladder algo */
#define AML_KL_ALGO_TDES (0)
#define AML_KL_ALGO_AES (1)

/* key ladder mode */
#define AML_KL_MODE_AML (0)
#define AML_KL_MODE_ETSI (2)
#define AML_KL_MODE_MSR (3)

/* ETSI key ladder mrk_cfg_index */
#define AML_KL_MRK_ETSI_0 (0)
#define AML_KL_MRK_ETSI_1 (1)
#define AML_KL_MRK_ETSI_2 (2)
#define AML_KL_MRK_ETSI_3 (3)

/* AML key ladder mrk_cfg_index */
#define AML_KL_MRK_ACGK (0)
#define AML_KL_MRK_ACUK (1)
#define AML_KL_MRK_DVGK (2)
#define AML_KL_MRK_DVUK (3)
#define AML_KL_MRK_DGPK1 (4)
#define AML_KL_MRK_DGPK2 (5)
#define AML_KL_MRK_ACRK (6)

/* T5W ETSI key ladder kl_num */
#define AML_KL_NUM_0 (0)
#define AML_KL_NUM_1 (1)
#define AML_KL_NUM_2 (2)
#define AML_KL_NUM_3 (3)
#define AML_KL_NUM_4 (4)
#define AML_KL_NUM_5 (5)
#define AML_KL_NUM_6 (6)
#define AML_KL_NUM_7 (7)
#define AML_KL_NUM_8 (8)
#define AML_KL_NUM_9 (9)
#define AML_KL_NUM_10 (10)

/* ETSI key ladder func_id */
#define AML_KL_FUNC_ID_0 (0)
#define AML_KL_FUNC_ID_6 (6)
#define AML_KL_FUNC_ID_7 (7)
#define AML_KL_FUNC_ID_8 (8)
#define AML_KL_FUNC_ID_9 (9)

/* MSR key ladder func_id */
#define MSR_KL_FUNC_ID_CWUK (1)
#define MSR_KL_FUNC_ID_CPUK (2)
#define MSR_KL_FUNC_ID_SSUK (3)
#define MSR_KL_FUNC_ID_CAUK (4)
#define MSR_KL_FUNC_ID_CCCK (6)
#define MSR_KL_FUNC_ID_TAUK (7)

/* AML key ladder func_id */
#define AML_KL_FUNC_AES_0 (0)
#define AML_KL_FUNC_AES_1 (1)
#define AML_KL_FUNC_AES_2 (2)
#define AML_KL_FUNC_AES_3 (3)
#define AML_KL_FUNC_TDES2_0 (4)
#define AML_KL_FUNC_TDES2_1 (5)
#define AML_KL_FUNC_HMAC_L (6)
#define AML_KL_FUNC_HMAC_H (7)
#define AML_KL_FUNC_AES256_L (8)
#define AML_KL_FUNC_AES256_H (9)

struct amlkl_usage {
	__u32 crypto;
	__u32 algo;
	__u32 uid;
};

struct amlkl_params {
	__u32 kt_handle;
	__u8 levels;
	__u8 module_id;
	__u8 kl_algo;
	__u8 kl_mode;
	struct amlkl_usage usage;
	__u8 mrk_cfg_index;
	__u8 kl_num;
	__u8 func_id;
	__u8 reserved1;
	__u8 eks[6][16];
	__u16 vid;
	__u8 reserved[14];
};

#define AML_MKL_IOCTL_RUN _IOW('L', 1, struct amlkl_params)

#endif
