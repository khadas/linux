/*-----------------------------------------------------------------------
//
// Proprietary Information of Elliptic Technologies
// Copyright (C) 2001-2015, all rights reserved
// Elliptic Technologies, Inc.
//
// As part of our confidentiality agreement, Elliptic Technologies and
// the Company, as a Receiving Party, of this information agrees to
// keep strictly confidential all Proprietary Information so received
// from Elliptic Technologies. Such Proprietary Information can be used
// solely for the purpose of evaluating and/or conducting a proposed
// business relationship or transaction between the parties. Each
// Party agrees that any and all Proprietary Information is and
// shall remain confidential and the property of Elliptic Technologies.
// The Company may not use any of the Proprietary Information of
// Elliptic Technologies for any purpose other than the above-stated
// purpose without the prior written consent of Elliptic Technologies.
//
//-----------------------------------------------------------------------
//
// Project:
//
// ESM Host Library
//
// Description:
//
// ESM Host Library Driver: Interface to the Linux kernel driver.
//
//-----------------------------------------------------------------------*/

#ifndef _HOST_LIB_DRIVER_LINUX_IF_H_
#define _HOST_LIB_DRIVER_LINUX_IF_H_

#ifdef __KERNEL__
#include <linux/ioctl.h>
#else
#include <sys/ioctl.h>
#endif

#include "ESMHostTypes.h"
#include "ESMHostLibDriverErrors.h"

/* ESM_HLD_IOCTL_LOAD_CODE */
struct esm_hld_ioctl_load_code {
	uint8_t *code;
	uint32_t code_size;
	ESM_STATUS returned_status;
};

struct compact_esm_hld_ioctl_load_code {
	compat_uptr_t code;
	uint32_t code_size;
	ESM_STATUS returned_status;
};

/* ESM_HLD_IOCTL_GET_CODE_PHYS_ADDR */
struct esm_hld_ioctl_get_code_phys_addr {
	uint32_t returned_phys_addr;
	ESM_STATUS returned_status;
};

/* ESM_HLD_IOCTL_GET_DATA_PHYS_ADDR */
struct esm_hld_ioctl_get_data_phys_addr {
	uint32_t returned_phys_addr;
	ESM_STATUS returned_status;
};

/* ESM_HLD_IOCTL_GET_DATA_SIZE */
struct esm_hld_ioctl_get_data_size {
	uint32_t returned_data_size;
	ESM_STATUS returned_status;
};

/* ESM_HLD_IOCTL_HPI_READ */
struct esm_hld_ioctl_hpi_read {
	uint32_t offset;
	uint32_t returned_data;
	ESM_STATUS returned_status;
};

/* ESM_HLD_IOCTL_HPI_WRITE */
struct esm_hld_ioctl_hpi_write {
	uint32_t offset;
	uint32_t data;
	ESM_STATUS returned_status;
};

/* ESM_HLD_IOCTL_DATA_READ */
struct esm_hld_ioctl_data_read {
	uint32_t offset;
	uint32_t nbytes;
	uint8_t *dest_buf;
	ESM_STATUS returned_status;
};

struct compact_esm_hld_ioctl_data_read {
	uint32_t offset;
	uint32_t nbytes;
	compat_uptr_t dest_buf;
	ESM_STATUS returned_status;
};

/* ESM_HLD_IOCTL_DATA_WRITE */
struct esm_hld_ioctl_data_write {
	uint32_t offset;
	uint32_t nbytes;
	uint8_t *src_buf;
	ESM_STATUS returned_status;
};

struct compact_esm_hld_ioctl_data_write {
	uint32_t offset;
	uint32_t nbytes;
	compat_uptr_t src_buf;
	ESM_STATUS returned_status;
};

/* ESM_HLD_IOCTL_DATA_SET */
struct esm_hld_ioctl_data_set {
	uint32_t offset;
	uint32_t nbytes;
	uint8_t data;
	ESM_STATUS returned_status;
};

/* ESM_HLD_IOCTL_ESM_OPEN */
struct esm_hld_ioctl_esm_open {
	uint32_t hpi_base;
	uint32_t code_base;
	uint32_t code_size;
	uint32_t data_base;
	uint32_t data_size;
	ESM_STATUS returned_status;
};

/* IOCTL commands */
#define ESM_HLD_IOC_MAGIC  'E'
#define ESM_HLD_IOCTL_LOAD_CODE \
	_IOWR(ESM_HLD_IOC_MAGIC, 1000, struct esm_hld_ioctl_load_code)
#define ESM_HLD_IOCTL_GET_CODE_PHYS_ADDR \
	_IOR(ESM_HLD_IOC_MAGIC, 1001, struct esm_hld_ioctl_get_code_phys_addr)
#define ESM_HLD_IOCTL_GET_DATA_PHYS_ADDR \
	_IOR(ESM_HLD_IOC_MAGIC, 1002, struct esm_hld_ioctl_get_data_phys_addr)
#define ESM_HLD_IOCTL_GET_DATA_SIZE \
	_IOR(ESM_HLD_IOC_MAGIC, 1003, struct esm_hld_ioctl_get_data_size)
#define ESM_HLD_IOCTL_HPI_READ \
	_IOWR(ESM_HLD_IOC_MAGIC, 1004, struct esm_hld_ioctl_hpi_read)
#define ESM_HLD_IOCTL_HPI_WRITE \
	_IOWR(ESM_HLD_IOC_MAGIC, 1005, struct esm_hld_ioctl_hpi_write)
#define ESM_HLD_IOCTL_DATA_READ \
	_IOWR(ESM_HLD_IOC_MAGIC, 1006, struct esm_hld_ioctl_data_read)
#define ESM_HLD_IOCTL_DATA_WRITE \
	_IOWR(ESM_HLD_IOC_MAGIC, 1007, struct esm_hld_ioctl_data_write)
#define ESM_HLD_IOCTL_DATA_SET \
	_IOWR(ESM_HLD_IOC_MAGIC, 1008, struct esm_hld_ioctl_data_set)
#define ESM_HLD_IOCTL_ESM_OPEN \
	_IOWR(ESM_HLD_IOC_MAGIC, 1009, struct esm_hld_ioctl_esm_open)
#define ESM_HLD_IOCTL_LOAD_CODE32 \
	_IOWR(ESM_HLD_IOC_MAGIC, 1000, struct compact_esm_hld_ioctl_load_code)
#define ESM_HLD_IOCTL_DATA_READ32 \
	_IOWR(ESM_HLD_IOC_MAGIC, 1006, struct compact_esm_hld_ioctl_data_read)
#define ESM_HLD_IOCTL_DATA_WRITE32 \
	_IOWR(ESM_HLD_IOC_MAGIC, 1007, struct compact_esm_hld_ioctl_data_write)

#endif /* _HOST_LIB_DRIVER_LINUX_IF_H_ */
