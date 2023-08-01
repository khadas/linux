/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef AML_KT_DEV
#define AML_KT_DEV

#define KT_KTE_MAX (128)
#define KT_IVE_MAX (32)
#define KT_IV_FLAG_OFFSET (31)

struct aml_kt_dev {
	struct cdev cdev;
	struct mutex lock; /*define mutex*/
	struct amlkt_cfg_param *kt_slot[KT_KTE_MAX];
	struct amlkt_cfg_param *kt_iv_slot[KT_IVE_MAX];
	void __iomem *base_addr;
	struct reg {
		u32 rdy_offset;
		u32 cfg_offset;
		u32 sts_offset;
		u32 key0_offset;
		u32 key1_offset;
		u32 key2_offset;
		u32 key3_offset;
		u32 s17_cfg_offset;
	} reg;
	u32 user_cap;
	u32 algo_cap;
	u32 kte_start;
	u32 kte_end;
	u32 ive_start;
	u32 ive_end;
	u32 kt_reserved;
};

extern struct aml_kt_dev aml_kt_dev;

int aml_kt_alloc(struct aml_kt_dev *dev, u32 flag, u32 *handle);
int aml_kt_config(struct aml_kt_dev *dev, struct amlkt_cfg_param key_cfg);
int aml_kt_set_host_key(struct aml_kt_dev *dev, struct amlkt_set_key_param *key_param);
int aml_kt_free(struct aml_kt_dev *dev, u32 handle);
int aml_kt_get_status(struct aml_kt_dev *dev, u32 handle, u32 *key_sts);
int aml_kt_handle_to_kte(struct aml_kt_dev *dev, u32 handle, u32 *kte);
int aml_kt_set_hw_key(struct aml_kt_dev *dev, u32 handle);

#endif /* AML_KT_DEV */
