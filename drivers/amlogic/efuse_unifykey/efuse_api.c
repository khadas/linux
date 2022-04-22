// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/arm-smccc.h>
#include <linux/highmem.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/amlogic/efuse.h>
#include <linux/kallsyms.h>
#include "efuse.h"
#include <linux/amlogic/secmon.h>

static DEFINE_MUTEX(efuse_lock);

static unsigned long get_sharemem_info(unsigned long function_id)
{
	struct arm_smccc_res res;

	arm_smccc_smc((unsigned long)function_id, 0, 0, 0, 0, 0, 0, 0, &res);

	return res.a0;
}

static ssize_t meson_efuse_fn_smc(struct efuse_hal_api_arg *arg)
{
	long ret;
	unsigned int cmd, offset, size;
	unsigned long *retcnt = (unsigned long *)(arg->retcnt);
	struct arm_smccc_res res;
	void *sharemem_in_base = NULL;
	void *sharemem_out_base;
	long phy_in_base = 0;
	long phy_out_base = 0;

	if (arg->cmd == EFUSE_HAL_API_READ)
		cmd = efuse_cmd.read_cmd;
	else if (arg->cmd == EFUSE_HAL_API_WRITE)
		cmd = efuse_cmd.write_cmd;
	else
		return -1;

	offset = arg->offset;
	size = arg->size;

	mutex_lock(&efuse_lock);

	if (arg->cmd == EFUSE_HAL_API_WRITE) {
		phy_in_base = get_sharemem_info(efuse_cmd.mem_in_base_cmd);

		if (!pfn_valid(__phys_to_pfn(phy_in_base)))
			sharemem_in_base = ioremap_nocache(phy_in_base, size);
		else
			sharemem_in_base = phys_to_virt(phy_in_base);

		memcpy((void *)sharemem_in_base,
		       (const void *)arg->buffer, size);
	}

	asm __volatile__("" : : : "memory");

	arm_smccc_smc(cmd, offset, size, 0, 0, 0, 0, 0, &res);
	ret = res.a0;
	*retcnt = res.a0;

	if (arg->cmd == EFUSE_HAL_API_WRITE) {
		if (!pfn_valid(__phys_to_pfn(phy_in_base)))
			iounmap(sharemem_in_base);
	} else if ((arg->cmd == EFUSE_HAL_API_READ) && (ret != 0)) {
		phy_out_base = get_sharemem_info(efuse_cmd.mem_out_base_cmd);

		if (!pfn_valid(__phys_to_pfn(phy_out_base)))
			sharemem_out_base = ioremap_nocache(phy_out_base, ret);
		else
			sharemem_out_base = phys_to_virt(phy_out_base);

		memcpy((void *)arg->buffer,
		       (const void *)sharemem_out_base, ret);

		if (!pfn_valid(__phys_to_pfn(phy_out_base)))
			iounmap(sharemem_out_base);
	}

	mutex_unlock(&efuse_lock);

	if (!ret)
		return -1;

	return 0;
}

static ssize_t meson_trustzone_efuse(struct efuse_hal_api_arg *arg)
{
	ssize_t ret;
	struct cpumask task_cpumask;

	if (!arg)
		return -1;

	cpumask_copy(&task_cpumask, current->cpus_ptr);
	set_cpus_allowed_ptr(current, cpumask_of(0));

	ret = meson_efuse_fn_smc(arg);
	set_cpus_allowed_ptr(current, &task_cpumask);

	return ret;
}

static unsigned long efuse_data_process(unsigned long type,
					unsigned long buffer,
					unsigned long length,
					unsigned long option)
{
	struct arm_smccc_res res;
	void *sharemem_in_base;
	long phy_in_base;
	struct page *page;

	mutex_lock(&efuse_lock);

	phy_in_base = get_sharemem_info(efuse_cmd.mem_in_base_cmd);
	page = pfn_to_page(phy_in_base >> PAGE_SHIFT);

	if (!pfn_valid(__phys_to_pfn(phy_in_base)))
		sharemem_in_base = ioremap_nocache(phy_in_base, length);
	else
		sharemem_in_base = phys_to_virt(phy_in_base);

	memcpy((void *)sharemem_in_base,
	       (const void *)buffer, length);

	asm __volatile__("" : : : "memory");

	do {
		arm_smccc_smc((unsigned long)AML_DATA_PROCESS,
			      (unsigned long)type,
			      (unsigned long)phy_in_base,
			      (unsigned long)length,
			      (unsigned long)option,
			      0, 0, 0, &res);
	} while (0);

	if (!pfn_valid(__phys_to_pfn(phy_in_base)))
		iounmap(sharemem_in_base);

	mutex_unlock(&efuse_lock);

	return res.a0;
}

int efuse_amlogic_cali_item_read(unsigned int item)
{
	struct arm_smccc_res res;

	/* range check */
	if (item < EFUSE_CALI_SUBITEM_WHOBURN ||
		item > EFUSE_CALI_SUBITEM_BC)
		return -EINVAL;

	mutex_lock(&efuse_lock);

	do {
		arm_smccc_smc((unsigned long)EFUSE_READ_CALI_ITEM,
			(unsigned long)item,
			0, 0, 0, 0, 0, 0, &res);
	} while (0);

	mutex_unlock(&efuse_lock);
	return res.a0;
}
EXPORT_SYMBOL_GPL(efuse_amlogic_cali_item_read);

/*
 *retrun: 1: wrote, 0: not write, -1: fail or not support
 */
int efuse_amlogic_check_lockable_item(unsigned int item)
{
	struct arm_smccc_res res;

	/* range check */
	if (item < EFUSE_LOCK_SUBITEM_BASE ||
		item > EFUSE_LOCK_SUBITEM_MAX)
		return -EINVAL;

	mutex_lock(&efuse_lock);

	do {
		arm_smccc_smc((unsigned long)EFUSE_READ_CALI_ITEM,
			(unsigned long)item,
			0, 0, 0, 0, 0, 0, &res);
	} while (0);

	mutex_unlock(&efuse_lock);
	return res.a0;
}
EXPORT_SYMBOL_GPL(efuse_amlogic_check_lockable_item);

unsigned long efuse_amlogic_set(char *buf, size_t count)
{
	unsigned long ret;
	struct cpumask task_cpumask;

	cpumask_copy(&task_cpumask, current->cpus_ptr);
	set_cpus_allowed_ptr(current, cpumask_of(0));

	ret = efuse_data_process(AML_D_P_W_EFUSE_AMLOGIC,
				 (unsigned long)buf, (unsigned long)count, 0);
	set_cpus_allowed_ptr(current, &task_cpumask);

	return ret;
}

static u32 meson_efuse_obj_read(u32 obj_id, u8 *buff, u32 *size)
{
	u32 rc = EFUSE_OBJ_ERR_UNKNOWN;
	u32 len = 0;
	struct arm_smccc_res res;

	mutex_lock(&efuse_lock);
	meson_sm_mutex_lock();

	memcpy((void *)sharemem_input_base, buff, *size);

	do {
		arm_smccc_smc((unsigned long)EFUSE_OBJ_READ,
			      (unsigned long)obj_id,
			      (unsigned long)*size,
			      0, 0, 0, 0, 0, &res);

	} while (0);

	rc = res.a0;
	len = res.a1;

	if (rc == EFUSE_OBJ_SUCCESS) {
		if (*size >= len) {
			memcpy(buff, (const void *)sharemem_output_base, len);
			*size = len;
		} else {
			rc = EFUSE_OBJ_ERR_SIZE;
		}
	}

	meson_sm_mutex_unlock();
	mutex_unlock(&efuse_lock);

	return rc;
}

static u32 meson_efuse_obj_write(u32 obj_id, u8 *buff, u32 size)
{
	struct arm_smccc_res res;

	mutex_lock(&efuse_lock);
	meson_sm_mutex_lock();

	memcpy((void *)sharemem_input_base, buff, size);

	do {
		arm_smccc_smc((unsigned long)EFUSE_OBJ_WRITE,
			      (unsigned long)obj_id,
			      (unsigned long)size,
			      0, 0, 0, 0, 0, &res);

	} while (0);

	meson_sm_mutex_unlock();
	mutex_unlock(&efuse_lock);

	return res.a0;
}

u32 efuse_obj_write(u32 obj_id, char *name, u8 *buff, u32 size)
{
	u32 ret;
	struct efuse_obj_field_t efuseinfo;
	struct cpumask task_cpumask;

	if (efuse_obj_cmd_status != 1)
		return EFUSE_OBJ_ERR_NOT_FOUND;

	cpumask_copy(&task_cpumask, current->cpus_ptr);
	set_cpus_allowed_ptr(current, cpumask_of(0));

	memset(&efuseinfo, 0, sizeof(efuseinfo));
	strncpy(efuseinfo.name, name, sizeof(efuseinfo.name) - 1);
	if (size > sizeof(efuseinfo.data))
		return EFUSE_OBJ_ERR_SIZE;
	efuseinfo.size = size;
	memcpy(efuseinfo.data, buff, efuseinfo.size);
	ret = meson_efuse_obj_write(obj_id, (uint8_t *)&efuseinfo, sizeof(efuseinfo));
	set_cpus_allowed_ptr(current, &task_cpumask);

	return ret;
}

u32 efuse_obj_read(u32 obj_id, char *name, u8 *buff, u32 *size)
{
	u32 ret;
	struct efuse_obj_field_t efuseinfo;
	struct cpumask task_cpumask;

	if (efuse_obj_cmd_status != 1)
		return EFUSE_OBJ_ERR_NOT_FOUND;

	cpumask_copy(&task_cpumask, current->cpus_ptr);
	set_cpus_allowed_ptr(current, cpumask_of(0));

	memset(&efuseinfo, 0, sizeof(efuseinfo));
	strncpy(efuseinfo.name, name, sizeof(efuseinfo.name) - 1);
	*size = sizeof(efuseinfo);
	ret = meson_efuse_obj_read(obj_id, (uint8_t *)&efuseinfo, size);
	memcpy(buff, efuseinfo.data, efuseinfo.size);
	*size = efuseinfo.size;
	set_cpus_allowed_ptr(current, &task_cpumask);

	return ret;
}

static ssize_t meson_trustzone_efuse_get_max(struct efuse_hal_api_arg *arg)
{
	ssize_t ret;
	unsigned int cmd;
	struct arm_smccc_res res;

	if (arg->cmd != EFUSE_HAL_API_USER_MAX)
		return -1;

	cmd = efuse_cmd.get_max_cmd;

	asm __volatile__("" : : : "memory");
	arm_smccc_smc(cmd, 0, 0, 0, 0, 0, 0, 0, &res);
	ret = res.a0;

	if (!ret)
		return -1;

	return ret;
}

ssize_t efuse_get_max(void)
{
	struct efuse_hal_api_arg arg;
	ssize_t ret;
	struct cpumask task_cpumask;

	arg.cmd = EFUSE_HAL_API_USER_MAX;

	cpumask_copy(&task_cpumask, current->cpus_ptr);
	set_cpus_allowed_ptr(current, cpumask_of(0));

	ret = meson_trustzone_efuse_get_max(&arg);
	set_cpus_allowed_ptr(current, &task_cpumask);

	return ret;
}

static ssize_t _efuse_read(char *buf, size_t count, loff_t *ppos)
{
	unsigned int pos = *ppos;

	struct efuse_hal_api_arg arg;
	unsigned long retcnt;
	ssize_t ret;

	arg.cmd = EFUSE_HAL_API_READ;
	arg.offset = pos;
	arg.size = count;
	arg.buffer = (unsigned long)buf;
	arg.retcnt = (unsigned long)&retcnt;
	ret = meson_trustzone_efuse(&arg);
	if (ret == 0) {
		*ppos += retcnt;
		return retcnt;
	}

	return ret;
}

static ssize_t _efuse_write(const char *buf, size_t count, loff_t *ppos)
{
	unsigned int pos = *ppos;

	struct efuse_hal_api_arg arg;
	unsigned long retcnt;
	ssize_t ret;

	arg.cmd = EFUSE_HAL_API_WRITE;
	arg.offset = pos;
	arg.size = count;
	arg.buffer = (unsigned long)buf;
	arg.retcnt = (unsigned long)&retcnt;

	ret = meson_trustzone_efuse(&arg);
	if (ret == 0) {
		*ppos = retcnt;
		return retcnt;
	}

	return ret;
}

ssize_t efuse_read_usr(char *buf, size_t count, loff_t *ppos)
{
	char *pdata = NULL;
	ssize_t ret;
	loff_t pos;

	pdata = kmalloc(count, GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	pos = *ppos;

	ret = _efuse_read(pdata, count, (loff_t *)&pos);

	memcpy(buf, pdata, count);
	kfree(pdata);

	return ret;
}

ssize_t efuse_write_usr(char *buf, size_t count, loff_t *ppos)
{
	char *pdata = NULL;
	ssize_t ret;
	loff_t pos;

	pdata = kmalloc(count, GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	memcpy(pdata, buf, count);
	pos = *ppos;

	ret = _efuse_write(pdata, count, (loff_t *)&pos);
	kfree(pdata);

	return ret;
}
