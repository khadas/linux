/*
 * drivers/amlogic/tee/tee.c
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/arm-smccc.h>
#include <linux/platform_device.h>

#include <linux/amlogic/tee.h>
#include <asm/cputype.h>

#define DRIVER_NAME "tee_info"
#define DRIVER_DESC "Amlogic tee driver"

#define TEE_MSG_UID_0         0x384fb3e0
#define TEE_MSG_UID_1         0xe7f811e3
#define TEE_MSG_UID_2         0xaf630002
#define TEE_MSG_UID_3         0xa5d5c51b
static int disable_flag;
#define TEE_SMC_FUNCID_CALLS_REVISION 0xFF03
#define TEE_SMC_CALLS_REVISION \
	ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL, ARM_SMCCC_SMC_32, \
			ARM_SMCCC_OWNER_TRUSTED_OS_END, \
			TEE_SMC_FUNCID_CALLS_REVISION)

#define TEE_SMC_FUNCID_CALLS_UID 0xFF01
#define TEE_SMC_CALLS_UID \
	ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL, ARM_SMCCC_SMC_32, \
			   ARM_SMCCC_OWNER_TRUSTED_OS_END, \
			   TEE_SMC_FUNCID_CALLS_UID)


#define TEE_SMC_FAST_CALL_VAL(func_num) \
	ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL, ARM_SMCCC_SMC_32, \
			ARM_SMCCC_OWNER_TRUSTED_OS, (func_num))

#define TEE_SMC_FUNCID_GET_OS_REVISION 0x0001
#define TEE_SMC_CALL_GET_OS_REVISION \
	TEE_SMC_FAST_CALL_VAL(TEE_SMC_FUNCID_GET_OS_REVISION)

#define TEE_SMC_FUNCID_CONFIG_DEVICE_SECURE        14
#define TEE_SMC_CONFIG_DEVICE_SECURE \
	TEE_SMC_FAST_CALL_VAL(TEE_SMC_FUNCID_CONFIG_DEVICE_SECURE)

#define TEE_SMC_FUNCID_LOAD_VIDEO_FW               15
#define TEE_SMC_LOAD_VIDEO_FW \
	TEE_SMC_FAST_CALL_VAL(TEE_SMC_FUNCID_LOAD_VIDEO_FW)

#define TEE_SMC_FUNCID_PROTECT_TVP_MEM             0xE020
#define TEE_SMC_PROTECT_TVP_MEM \
	TEE_SMC_FAST_CALL_VAL(TEE_SMC_FUNCID_PROTECT_TVP_MEM)

#define TEE_SMC_FUNCID_UNPROTECT_TVP_MEM           0xE021
#define TEE_SMC_UNPROTECT_TVP_MEM \
	TEE_SMC_FAST_CALL_VAL(TEE_SMC_FUNCID_UNPROTECT_TVP_MEM)

#define TEE_SMC_FUNCID_PROTECT_MEM_BY_TYPE         0xE023
#define TEE_SMC_PROTECT_MEM_BY_TYPE \
	TEE_SMC_FAST_CALL_VAL(TEE_SMC_FUNCID_PROTECT_MEM_BY_TYPE)

#define TEE_SMC_FUNCID_UNPROTECT_MEM               0xE024
#define TEE_SMC_UNPROTECT_MEM \
	TEE_SMC_FAST_CALL_VAL(TEE_SMC_FUNCID_UNPROTECT_MEM)

static struct class *tee_sys_class;

struct tee_smc_calls_revision_result {
	unsigned long major;
	unsigned long minor;
	unsigned long reserved0;
	unsigned long reserved1;
};

static int tee_msg_os_revision(uint32_t *major, uint32_t *minor)
{
	union {
		struct arm_smccc_res smccc;
		struct tee_smc_calls_revision_result result;
	} res;

	arm_smccc_smc(TEE_SMC_CALL_GET_OS_REVISION,
			0, 0, 0, 0, 0, 0, 0, &res.smccc);
	*major = res.result.major;
	*minor = res.result.minor;

	return 0;
}

static int tee_msg_api_revision(uint32_t *major, uint32_t *minor)
{
	union {
		struct arm_smccc_res smccc;
		struct tee_smc_calls_revision_result result;
	} res;

	arm_smccc_smc(TEE_SMC_CALLS_REVISION,
			0, 0, 0, 0, 0, 0, 0, &res.smccc);
	*major = res.result.major;
	*minor = res.result.minor;

	return 0;
}

static ssize_t tee_os_version_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	int ret;
	uint32_t major, minor;

	ret = tee_msg_os_revision(&major, &minor);
	if (ret)
		return 0;

	ret = sprintf(buf, "os version: V%d.%d\n", major, minor);

	return ret;
}

static ssize_t tee_api_version_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	int ret;
	uint32_t major, minor;

	ret = tee_msg_api_revision(&major, &minor);
	if (ret)
		return 0;

	ret = sprintf(buf, "api version: V%d.%d\n", major, minor);

	return ret;
}

static CLASS_ATTR(os_version, 0644, tee_os_version_show,
		NULL);
static CLASS_ATTR(api_version, 0644, tee_api_version_show,
		NULL);

/*
 * index: firmware index
 * vdec:  vdec type(0: compatible, 1: legency vdec, 2: HEVC vdec)
 */
static int tee_load_firmware(uint32_t index, uint32_t vdec, bool is_swap)
{
	struct arm_smccc_res res;

	arm_smccc_smc(TEE_SMC_LOAD_VIDEO_FW,
			index, vdec, is_swap, 0, 0, 0, 0, &res);

	return res.a0;
}

int tee_load_video_fw(uint32_t index, uint32_t vdec)
{
	return tee_load_firmware(index, vdec, false);
}
EXPORT_SYMBOL(tee_load_video_fw);

int tee_load_video_fw_swap(uint32_t index, uint32_t vdec, bool is_swap)
{
	return tee_load_firmware(index, vdec, is_swap);
}
EXPORT_SYMBOL(tee_load_video_fw_swap);

bool tee_enabled(void)
{
	struct arm_smccc_res res;
	if (disable_flag == 1)
		return false;
	/*return false;*/ /*disable tee load temporary*/

	arm_smccc_smc(TEE_SMC_CALLS_UID, 0, 0, 0, 0, 0, 0, 0, &res);

	if (res.a0 == TEE_MSG_UID_0 && res.a1 == TEE_MSG_UID_1 &&
	    res.a2 == TEE_MSG_UID_2 && res.a3 == TEE_MSG_UID_3)
		return true;
	return false;
}
EXPORT_SYMBOL(tee_enabled);

uint32_t tee_protect_tvp_mem(uint32_t start, uint32_t size, uint32_t *handle)
{
	struct arm_smccc_res res;

	if (handle == NULL)
		return 0xFFFF0006;

	arm_smccc_smc(TEE_SMC_PROTECT_TVP_MEM,
			start, size, 0, 0, 0, 0, 0, &res);

	*handle = res.a1;

	return res.a0;
}
EXPORT_SYMBOL(tee_protect_tvp_mem);

void tee_unprotect_tvp_mem(uint32_t handle)
{
	struct arm_smccc_res res;

	arm_smccc_smc(TEE_SMC_UNPROTECT_TVP_MEM,
			handle, 0, 0, 0, 0, 0, 0, &res);
}
EXPORT_SYMBOL(tee_unprotect_tvp_mem);

uint32_t tee_protect_mem_by_type(uint32_t type,
		uint32_t start, uint32_t size,
		uint32_t *handle)
{
	struct arm_smccc_res res;

	if (!handle)
		return 0xFFFF0006;

	arm_smccc_smc(TEE_SMC_PROTECT_MEM_BY_TYPE,
			type, start, size, 0, 0, 0, 0, &res);

	*handle = res.a1;

	return res.a0;
}
EXPORT_SYMBOL(tee_protect_mem_by_type);

void tee_unprotect_mem(uint32_t handle)
{
	struct arm_smccc_res res;

	arm_smccc_smc(TEE_SMC_UNPROTECT_MEM,
			handle, 0, 0, 0, 0, 0, 0, &res);
}
EXPORT_SYMBOL(tee_unprotect_mem);

int tee_config_device_state(int dev_id, int secure)
{
	struct arm_smccc_res res;

	arm_smccc_smc(TEE_SMC_CONFIG_DEVICE_SECURE,
			dev_id, secure, 0, 0, 0, 0, 0, &res);

	return res.a0;
}
EXPORT_SYMBOL(tee_config_device_state);

int tee_create_sysfs(void)
{
	int ret;

	tee_sys_class = class_create(THIS_MODULE, DRIVER_NAME);
	ret = class_create_file(tee_sys_class, &class_attr_os_version);
	if (ret != 0) {
		pr_err("create class file os_version fail\n");
		return ret;
	}
	ret = class_create_file(tee_sys_class, &class_attr_api_version);
	if (ret != 0) {
		pr_err("create class file os_version fail\n");
		return ret;
	}

	return ret;
}

static int __init aml_tee_modinit(void)
{
	return tee_create_sysfs();
}

arch_initcall(aml_tee_modinit);

static void __exit aml_tee_modexit(void)
{
	class_destroy(tee_sys_class);
}
module_param(disable_flag, uint, 0664);
MODULE_PARM_DESC(disable_flag, "\n tee firmload disable_flag flag\n");

module_exit(aml_tee_modexit);

MODULE_AUTHOR("pengguang.zhu<pengguang.zhu@amlogic.com>");
MODULE_DESCRIPTION(DRIVER_TEE);
MODULE_LICENSE("GPL");
