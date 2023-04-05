/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _AML_KT_H_
#define _AML_KT_H_

#include <linux/types.h>

/* key user */
#define AML_KT_USER_M2M_0        (0)
#define AML_KT_USER_M2M_1        (1)
#define AML_KT_USER_M2M_2        (2)
#define AML_KT_USER_M2M_3        (3)
#define AML_KT_USER_M2M_4        (4)
#define AML_KT_USER_M2M_5        (5)
#define AML_KT_USER_M2M_ANY      (7)
#define AML_KT_USER_TSD          (8)
#define AML_KT_USER_TSN          (9)
#define AML_KT_USER_TSE          (10)

/* key algorithm */
#define AML_KT_ALGO_AES          (0)
#define AML_KT_ALGO_TDES         (1)
#define AML_KT_ALGO_DES          (2)
#define AML_KT_ALGO_S17          (3)
#define AML_KT_ALGO_SM4          (4)
#define AML_KT_ALGO_NDL          (7)
#define AML_KT_ALGO_ND           (8)
#define AML_KT_ALGO_CSA3         (9)
#define AML_KT_ALGO_CSA2         (10)
#define AML_KT_ALGO_HMAC         (13)

/* key flag */
#define AML_KT_FLAG_NONE         (0)
#define AML_KT_FLAG_ENC_ONLY     (1)
#define AML_KT_FLAG_DEC_ONLY     (2)
#define AML_KT_FLAG_ENC_DEC      (3)

/* key source */
#define AML_KT_SRC_REE_CERT     (15)
#define AML_KT_SRC_NSK          (14)
#define AML_KT_SRC_MSR3         (13)
#define AML_KT_SRC_VO           (12)
#define AML_KT_SRC_SPKL         (11)
#define AML_KT_SRC_SPHOST       (10)
#define AML_KT_SRC_REEKL_MSR2   (4)
#define AML_KT_SRC_REEKL_ETSI   (3)
#define AML_KT_SRC_REEKL_NAGRA  (2)
#define AML_KT_SRC_REEKL_AML    (1)
#define AML_KT_SRC_REE_HOST     (0)

#define AML_KT_ALLOC_FLAG_IV         (1)
#define AML_KT_ALLOC_FLAG_HOST       (2)
#define AML_KT_ALLOC_FLAG_NSK_M2M    (3)

struct amlkt_alloc_param {
	__u32 flag;
	__u32 handle;
};

struct amlkt_cfg_param {
	__u32 handle;
	__u32 key_userid;
	__u32 key_algo;
	__u32 key_flag;
	__u32 key_source;
	__u32 ext_value;
};

struct amlkt_set_key_param {
	__u32 handle;
	__u8 key[32];
	__u32 key_len;
};

#define IOCTL_MAGIC 'a'
#define AML_KT_ALLOC _IOWR(IOCTL_MAGIC, 0, struct amlkt_alloc_param)
#define AML_KT_CONFIG _IOW(IOCTL_MAGIC, 1, struct amlkt_cfg_param)
#define AML_KT_SET _IOWR(IOCTL_MAGIC, 2, struct amlkt_set_key_param)
#define AML_KT_HW_SET _IOWR(IOCTL_MAGIC, 3, __u32)
#define AML_KT_FREE _IOW(IOCTL_MAGIC, 4, __u32)
#define AML_KT_INVALIDATE _IOW(IOCTL_MAGIC, 5, __u32)

#endif

