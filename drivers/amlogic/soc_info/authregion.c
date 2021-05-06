// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/scatterlist.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/ioctl.h>
#include <linux/of.h>
#include <linux/of_reserved_mem.h>
#include <linux/cma.h>
#include <linux/sysfs.h>
#include <linux/random.h>
#include <linux/amlogic/secmon.h>
#include <linux/arm-smccc.h>
#include "soc_info.h"

extern void __iomem *socinfo_sharemem_base;
int auth_region_set(void *auth_input)
{
	long ret;
	struct arm_smccc_res res;

	socinfo_sharemem_base = get_meson_sm_input_base();
	if (!socinfo_sharemem_base)
		return -1;
	meson_sm_mutex_lock();
	memcpy((void *)socinfo_sharemem_base,
			auth_input, sizeof(struct authnt_region) + sizeof(uint32_t));
	asm __volatile__("" : : : "memory");
	arm_smccc_smc(auth_reg_ops_cmd, AUTH_REG_SET_IDX, 0,
		 0, 0, 0, 0, 0, &res);
	meson_sm_mutex_unlock();
	ret = res.a0;
	if (!ret)
		return -1;
	else
		return 0;
}

int auth_region_get_all(void *auth_input)
{
	long ret;
	struct arm_smccc_res res;
	u32 data;

	socinfo_sharemem_base = get_meson_sm_output_base();
	if (!socinfo_sharemem_base)
		return -1;

	meson_sm_mutex_lock();
	asm __volatile__("" : : : "memory");
	arm_smccc_smc(auth_reg_ops_cmd, AUTH_REG_GET_ALL, 0,
		0, 0, 0, 0, 0, &res);
	ret = res.a0;
	if (ret) {
		data = *(uint32_t *)socinfo_sharemem_base;
		if (((data & 0xffff) <= AUTH_REG_NUM) &&
			(data >> 16 == AUTH_REG_MAGIC))
			memcpy((void *)auth_input, (void *)socinfo_sharemem_base, ret);
	}
	meson_sm_mutex_unlock();
	if (!ret)
		return -1;
	else
		return 0;
}

int auth_region_rst(void)
{
	long ret;
	struct arm_smccc_res res;

	asm __volatile__("" : : : "memory");
	arm_smccc_smc(auth_reg_ops_cmd, AUTH_REG_SET_RST, 0,
		 0, 0, 0, 0, 0, &res);
	ret = res.a0;
	if (!ret)
		return -1;
	else
		return 0;
}
