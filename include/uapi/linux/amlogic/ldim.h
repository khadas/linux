/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _LDIM_H_
#define _LDIM_H_

#include <linux/types.h>

/* **********************************
 * IOCTL define
 * **********************************
 */
#define _VE_LDIM  'C'
#define AML_LDIM_IOC_NR_GET_PQ_INIT	0x04
#define AML_LDIM_IOC_NR_SET_PQ_INIT	0x05
#define AML_LDIM_IOC_NR_GET_LEVEL_IDX	0x06
#define AML_LDIM_IOC_NR_SET_LEVEL_IDX	0x07
#define AML_LDIM_IOC_NR_GET_FUNC_EN	0x08
#define AML_LDIM_IOC_NR_SET_FUNC_EN	0x09
#define AML_LDIM_IOC_NR_GET_REMAP_EN	0x0A
#define AML_LDIM_IOC_NR_SET_REMAP_EN	0x0B
#define AML_LDIM_IOC_NR_GET_BL_MATRIX	0x0C
#define AML_LDIM_IOC_NR_SET_BL_MATRIX	0x0D
#define AML_LDIM_IOC_NR_GET_DEMOMODE	0x0E
#define AML_LDIM_IOC_NR_SET_DEMOMODE	0x0F
#define AML_LDIM_IOC_NR_GET_ZONENUM	0x11
#define AML_LDIM_IOC_NR_SET_ZONENUM	0x12

#define AML_LDIM_IOC_NR_GET_BL_MAPPING_PATH	0x55
#define AML_LDIM_IOC_NR_SET_BL_MAPPING	0x56

#define AML_LDIM_IOC_NR_GET_BL_PROFILE_PATH	0x57
#define AML_LDIM_IOC_NR_SET_BL_PROFILE	0x58

struct aml_ldim_bin_s {
	unsigned int index;
	unsigned int len;
	union {
	void *ptr;
	long long ptr_length;
	};
};

#define AML_LDIM_IOC_CMD_GET_PQ_INIT   \
	_IOR(_VE_LDIM, AML_LDIM_IOC_NR_GET_PQ_INIT, struct aml_ldim_bin_s)
#define AML_LDIM_IOC_CMD_SET_PQ_INIT   \
	_IOW(_VE_LDIM, AML_LDIM_IOC_NR_SET_PQ_INIT, struct aml_ldim_bin_s)
#define AML_LDIM_IOC_CMD_GET_LEVEL_IDX   \
	_IOR(_VE_LDIM, AML_LDIM_IOC_NR_GET_LEVEL_IDX, unsigned char)
#define AML_LDIM_IOC_CMD_SET_LEVEL_IDX   \
	_IOW(_VE_LDIM, AML_LDIM_IOC_NR_SET_LEVEL_IDX, unsigned char)
#define AML_LDIM_IOC_CMD_GET_FUNC_EN   \
	_IOR(_VE_LDIM, AML_LDIM_IOC_NR_GET_FUNC_EN, unsigned char)
#define AML_LDIM_IOC_CMD_SET_FUNC_EN   \
	_IOW(_VE_LDIM, AML_LDIM_IOC_NR_SET_FUNC_EN, unsigned char)
#define AML_LDIM_IOC_CMD_GET_REMAP_EN   \
	_IOR(_VE_LDIM, AML_LDIM_IOC_NR_GET_REMAP_EN, unsigned char)
#define AML_LDIM_IOC_CMD_SET_REMAP_EN   \
	_IOW(_VE_LDIM, AML_LDIM_IOC_NR_SET_REMAP_EN, unsigned char)
#define AML_LDIM_IOC_CMD_GET_DEMOMODE   \
	_IOR(_VE_LDIM, AML_LDIM_IOC_NR_GET_DEMOMODE, unsigned char)
#define AML_LDIM_IOC_CMD_SET_DEMOMODE   \
	_IOW(_VE_LDIM, AML_LDIM_IOC_NR_SET_DEMOMODE, unsigned char)

#define AML_LDIM_IOC_CMD_GET_ZONENUM   \
	_IOR(_VE_LDIM, AML_LDIM_IOC_NR_GET_ZONENUM, unsigned int)
#define AML_LDIM_IOC_CMD_GET_BL_MATRIX   \
	_IOR(_VE_LDIM, AML_LDIM_IOC_NR_GET_BL_MATRIX, unsigned int)

#define AML_LDIM_IOC_CMD_GET_BL_MAPPING_PATH   \
	_IOR(_VE_LDIM, AML_LDIM_IOC_NR_GET_BL_MAPPING_PATH, char)
#define AML_LDIM_IOC_CMD_SET_BL_MAPPING   \
	_IOW(_VE_LDIM, AML_LDIM_IOC_NR_SET_BL_MAPPING, struct aml_ldim_bin_s)
#define AML_LDIM_IOC_CMD_GET_BL_PROFILE_PATH   \
	_IOR(_VE_LDIM, AML_LDIM_IOC_NR_GET_BL_PROFILE_PATH, char)
#define AML_LDIM_IOC_CMD_SET_BL_PROFILE   \
	_IOW(_VE_LDIM, AML_LDIM_IOC_NR_SET_BL_PROFILE, struct aml_ldim_bin_s)
#endif

