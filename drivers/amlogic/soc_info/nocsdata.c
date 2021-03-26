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

void __iomem *socinfo_sharemem_base;
static long meson64_efuse_fn_smc(struct efuse_hal_api_arg *arg)
{
	long ret;
	unsigned int cmd, offset, size;
	unsigned long *retcnt = (unsigned long *)(arg->retcnt);
	struct arm_smccc_res res;

	switch (arg->cmd) {
	case EFUSE_HAL_NOCS_API_READ:
		cmd = read_nocsdata_cmd;
		socinfo_sharemem_base = get_meson_sm_output_base();
		if (!socinfo_sharemem_base)
			return -1;
		break;
	case EFUSE_HAL_NOCS_API_WRITE:
		cmd = write_nocsdata_cmd;
		socinfo_sharemem_base = get_meson_sm_input_base();
		if (!socinfo_sharemem_base)
			return -1;
		break;
	default:
		return -1;
		}
	offset = arg->offset;
	size = arg->size;
	meson_sm_mutex_lock();
	if (arg->cmd == EFUSE_HAL_NOCS_API_WRITE)
		memcpy((void *)socinfo_sharemem_base,
			(const void *)arg->buffer, size);

	asm __volatile__("" : : : "memory");

	arm_smccc_smc(cmd, offset, size, 0, 0, 0, 0, 0, &res);
	ret = res.a0;
	*retcnt = res.a0;
	if (arg->cmd == EFUSE_HAL_NOCS_API_READ && ret != 0)
		memcpy((void *)arg->buffer,
			(const void *)socinfo_sharemem_base, ret);
	meson_sm_mutex_unlock();

	if (!ret)
		return -1;
	else
		return 0;
}

static int meson64_trustzone_efuse(struct efuse_hal_api_arg *arg)
{
	int ret;
//	struct cpumask org_cpumask;

	if (!arg)
		return -1;

//	cpumask_copy(&org_cpumask, &current->cpus_allowed);
//	set_cpus_allowed_ptr(current, cpumask_of(0));
	ret = meson64_efuse_fn_smc(arg);
//	set_cpus_allowed_ptr(current, &org_cpumask);
	return ret;
}

ssize_t nocsdata_read(char *buf, size_t count, loff_t *ppos)
{
	unsigned int pos = *ppos;

	struct efuse_hal_api_arg arg;
	unsigned long retcnt;
	int ret;

	arg.cmd = EFUSE_HAL_NOCS_API_READ;
	arg.offset = pos;
	arg.size = count;
	arg.buffer = (unsigned long)buf;
	arg.retcnt = (unsigned long)&retcnt;
	ret = meson64_trustzone_efuse(&arg);
	if (ret == 0) {
		*ppos += retcnt;
		return 0;
	}
	pr_debug("%s:%s:%d: read error!!!\n",
			   __FILE__, __func__, __LINE__);
	return ret;
}

ssize_t nocsdata_write(const char *buf, size_t count, loff_t *ppos)
{
	unsigned int pos = *ppos;

	struct efuse_hal_api_arg arg;
	unsigned long retcnt;
	int ret;

	arg.cmd = EFUSE_HAL_NOCS_API_WRITE;
	arg.offset = pos;
	arg.size = count;
	arg.buffer = (unsigned long)buf;
	arg.retcnt = (unsigned long)&retcnt;

	ret = meson64_trustzone_efuse(&arg);
	if (ret == 0) {
		*ppos = retcnt;
		return 0;
	}
	pr_debug("%s:%s:%d: write error!!!\n",
			   __FILE__, __func__, __LINE__);
	return ret;
}
