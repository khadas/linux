/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef VPU_SECURITY_H_
#define VPU_SECURITY_H_

#define MODULE_NUM  6
struct vpu_secure_ins {
	struct mutex secure_lock;/*vpu secure mutex*/
	unsigned char registered;
	unsigned char secure_enable;
	unsigned char secure_status;
	unsigned char config_delay;
	int (*reg_wr_op)(u32 addr, u32 val);
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

enum cpu_type_e {
	MESON_CPU_MAJOR_ID_SC2_ = 0x1,
	MESON_CPU_MAJOR_ID_UNKNOWN,
};

struct sec_dev_data_s {
	enum cpu_type_e cpu_type;
};

#endif
