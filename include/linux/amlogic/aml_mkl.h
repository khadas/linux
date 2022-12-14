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

/* key ladder level */
#define AML_KL_LEVEL_3 (3)
#define AML_KL_LEVEL_4 (4)
#define AML_KL_LEVEL_5 (5)
#define AML_KL_LEVEL_6 (6)

/* key flag */
#define AML_KT_FLAG_NONE (0)
#define AML_KT_FLAG_ENC_ONLY (1)
#define AML_KT_FLAG_DEC_ONLY (2)
#define AML_KT_FLAG_ENC_DEC (3)

/* key algorithm */
#define AML_KT_ALGO_AES (0)
#define AML_KT_ALGO_TDES (1)
#define AML_KT_ALGO_DES (2)
#define AML_KT_ALGO_NDL (7)
#define AML_KT_ALGO_ND (8)
#define AML_KT_ALGO_CSA3 (9)
#define AML_KT_ALGO_CSA2 (10)
#define AML_KT_ALGO_HMAC (13)

/* key user */
#define AML_KT_USER_M2M_0 (0)
#define AML_KT_USER_M2M_1 (1)
#define AML_KT_USER_M2M_2 (2)
#define AML_KT_USER_M2M_3 (3)
#define AML_KT_USER_M2M_4 (4)
#define AML_KT_USER_M2M_5 (5)
#define AML_KT_USER_M2M_ANY (7)
#define AML_KT_USER_TSD (8)
#define AML_KT_USER_TSN (9)
#define AML_KT_USER_TSE (10)

/* key ladder algo */
#define AML_KL_ALGO_TDES (0)
#define AML_KL_ALGO_AES (1)

/* key ladder mode */
#define AML_KL_MODE_AML (0)
#define AML_KL_MODE_ETSI (2)

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

/* key ladder kl_num */
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
