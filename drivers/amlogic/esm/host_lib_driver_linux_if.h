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

#ifndef ESM_HLD_LINUX_IOCTL_H_
#define ESM_HLD_LINUX_IOCTL_H_

#include <linux/ioctl.h>
#include <linux/types.h>

/* ioctl numbers */
enum {
	ESM_NR_MIN = 0x10,
	ESM_NR_INIT,
	ESM_NR_MEMINFO,
	ESM_NR_LOAD_CODE,
	ESM_NR_READ_DATA,
	ESM_NR_WRITE_DATA,
	ESM_NR_MEMSET_DATA,
	ESM_NR_READ_HPI,
	ESM_NR_WRITE_HPI,
	ESM_NR_MAX
};

/*
 * ESM_IOC_INIT: associate file descriptor with the indicated memory.  This
 * must be called before any other ioctl on the file descriptor.
 *
 *   - hpi_base = base address of ESM HPI registers.
 *   - code_base = base address of firmware memory (0 to allocate internally)
 *   - data_base = base address of data memory (0 to allocate internally)
 *   - code_len, data_len = length of firmware and data memory, respectively.
 */
#define ESM_IOC_INIT    _IOW('H', ESM_NR_INIT, struct esm_ioc_meminfo)

/*
 * ESM_IOC_MEMINFO: retrieve memory information from file descriptor.
 *
 * Fills out the meminfo struct, returning the values passed to ESM_IOC_INIT
 * except that the actual base addresses of internal allocations (if any) are
 * reported.
 */
#define ESM_IOC_MEMINFO _IOR('H', ESM_NR_MEMINFO, struct esm_ioc_meminfo)

struct esm_ioc_meminfo {
	__u32 hpi_base;
	__u32 code_base;
	__u32 code_size;
	__u32 data_base;
	__u32 data_size;
};

/*
 * ESM_IOC_LOAD_CODE: write the provided buffer to the firmware memory.
 *
 *   - len = number of bytes in data buffer
 *   - data = data to write to firmware memory.
 *
 * This can only be done once (successfully).  Subsequent attempts will
 * return -EBUSY.
 */
#define ESM_IOC_LOAD_CODE _IOW('H', ESM_NR_LOAD_CODE, struct esm_ioc_code)

struct esm_ioc_code {
	__u32 len;
	__u8 data[];
};

/*
 * ESM_IOC_READ_DATA: copy from data memory.
 * ESM_IOC_WRITE_DATA: copy to data memory.
 *
 *   - offset = start copying at this byte offset into the data memory.
 *   - len    = number of bytes to copy.
 *   - data   = for write, buffer containing data to copy.
 *              for read, buffer to which read data will be written.
 *
 */
#define ESM_IOC_READ_DATA  _IOWR('H', ESM_NR_READ_DATA, struct esm_ioc_data)
#define ESM_IOC_WRITE_DATA  _IOW('H', ESM_NR_WRITE_DATA, struct esm_ioc_data)

/*
 * ESM_IOC_MEMSET_DATA: initialize data memory.
 *
 *   - offset  = start initializatoin at this byte offset into the data memory.
 *   - len     = number of bytes to set.
 *   - data[0] = byte value to write to all indicated memory locations.
 */
#define ESM_IOC_MEMSET_DATA _IOW('H', ESM_NR_MEMSET_DATA, struct esm_ioc_data)

struct esm_ioc_data {
	__u32 offset;
	__u32 len;
	__u8 data[];
};

/*
 * ESM_IOC_READ_HPI: read HPI register.
 * ESM_IOC_WRITE_HPI: write HPI register.
 *
 *   - offset = byte offset of HPI register to access.
 *   - value  = for write, value to write.
 *              for read, location to which result is stored.
 */
#define ESM_IOC_READ_HPI _IOWR('H', ESM_NR_READ_HPI, struct esm_ioc_hpi_reg)
#define ESM_IOC_WRITE_HPI _IOW('H', ESM_NR_WRITE_HPI, struct esm_ioc_hpi_reg)

struct esm_ioc_hpi_reg {
	__u32 offset;
	__u32 value;
};

#endif
