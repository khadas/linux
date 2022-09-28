/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef VPU_SECURITY_H_
#define VPU_SECURITY_H_

#include <linux/amlogic/media/vpu_secure/vpu_secure.h>

struct vpu_secure_ins {
	struct mutex secure_lock;/*vpu secure mutex*/
	unsigned char registered;
	unsigned char secure_enable;
	unsigned char secure_status;
	unsigned char config_delay;
	int (*reg_wr_op[VPP_TOP_MAX])(u32 addr, u32 val, u32 start, u32 len);
	void (*secure_cb)(u32 arg);
};

struct vpu_security_device_info {
	const char *device_name;
	struct platform_device *pdev;
	struct class *clsp;
	int mismatch_cnt;
	int probed;
	struct vpu_secure_ins ins[MODULE_NUM];
};

enum vpu_security_version_e {
	VPU_SEC_V1 = 1,
	VPU_SEC_V2,
	VPU_SEC_V3,
	VPU_SEC_V4,
	VPU_SEC_MAX
};

struct vpu_sec_reg_s {
	u32 reg;
	u32 en;
	u32 start;
	u32 len;
};

struct vpu_sec_bit_s {
	u32 bit_changed; /* the changed src bit */
	u32 current_val; /* reg val after being changed */
};

struct sec_dev_data_s {
	enum vpu_security_version_e version;
};

#endif
