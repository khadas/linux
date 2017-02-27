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
// business relationship or transaction between the  parties. Each
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
// ESM Host Library.
//
// Description:
//
// ESM Host Library Driver - Linux driver (kernel module)
// host_lib_driver_linux_kernel
//
//-----------------------------------------------------------------------*/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/netlink.h>
#include <linux/proc_fs.h>
#include <linux/version.h>
#include <linux/delay.h>
#include "hdmi_rx_reg.h"
#include "hdmirx_drv.h"

#include "host_lib_driver_linux_if.h"

/* #include "../hw/mach_reg.h" */
#ifdef HDCP22_ENABLE
#define ESM_DEVICE_MAJOR   60
#define MAX_ESM_DEVICES    6
#define ESM_RX_HPI_BASE    0xd0076000
/* ESM Device */
struct esm_device {
	int allocated;
	int code_loaded;
	int code_is_phys_mem;
	ulong code_base;
	ulong code_size;
	uint8_t *code;
	int data_is_phys_mem;
	ulong data_base;
	ulong data_size;
	uint8_t *data;
	ulong hpi_base;
	ulong hpi_size;
	uint8_t *hpi;
	int hpi_mem_region_requested;
	bool code_alloc_err;
	bool data_alloc_err;
};

/* Configuration parameters */
static int verbose;

/* Constant strings */
static const char *MY_TAG = "ESM HLD: ";
static const char *ESM_DEVICE_NAME = "esm_rx";
static const char *ESM_DEVICE_CLASS = "elliptic_rx";

/* Linux device, class and range */
static int device_created;
static struct device *device;
static int device_range_registered;
static int device_class_created;
static struct class *device_class;
static int is_esmmem_created;

/* ESM devices */
static struct esm_device esm_devices[MAX_ESM_DEVICES];
static void dump_device_info(struct esm_device *dev);

/*---------------------------------------------------------------------------
//              Processing of the requests from the userspace
---------------------------------------------------------------------------*/

/* Loads the firmware */
static long cmd_load_code(struct esm_device *esm,
	struct esm_hld_ioctl_load_code *request)
{
	unsigned long ret = 0;
	uint8_t *kernel_code;
	struct esm_hld_ioctl_load_code krequest;

	ret = copy_from_user(&krequest, request,
		sizeof(struct esm_hld_ioctl_load_code));
	if (ret)
		pr_info("copy left %ld Bytes\n", ret);

	if (verbose) {
		pr_info("%scmd_load_code: code=%p code_size=0x%x\n",
		MY_TAG, krequest.code, krequest.code_size);
	}

	if (do_esm_rst_flag == 1)
			esm->code_loaded = 0;

	if (esm->code_loaded == 1) {
		pr_info("%scmd_load_code: Code already loaded.\n", MY_TAG);
		krequest.returned_status = ESM_HL_DRIVER_SUCCESS;
	} else {
	if (krequest.code_size > esm->code_size) {
		pr_info("%sCode larger than memory (0x%x > 0x%lx).\n",
			MY_TAG, krequest.code_size, esm->code_size);
		krequest.returned_status = ESM_HL_DRIVER_NO_MEMORY;
	} else {
		kernel_code = kmalloc(krequest.code_size, GFP_KERNEL);

	if (kernel_code) {
		/* No Endian shift */
		ret = copy_from_user(esm->code, krequest.code,
			krequest.code_size);
		if (ret)
			pr_info("copy left %ld Bytes\n", ret);

	/*
	// Endian shift
	uint offset;

	copy_from_user(kernel_code, krequest.code, krequest.code_size);

	// Endian shift of firmware image
	for (offset = 0; offset < esm->code_size - 4; offset += 4)
	{
	esm->code[offset + 3] = kernel_code[offset];
	esm->code[offset + 2] = kernel_code[offset + 1];
	esm->code[offset + 1] = kernel_code[offset + 2];
	esm->code[offset]     = kernel_code[offset + 3];
	}
	*/

	kfree(kernel_code);
	krequest.returned_status = ESM_HL_DRIVER_SUCCESS;

		if (verbose)
			pr_info("%scopying firmware to code memory region.\n",
				MY_TAG);
		} else {
			pr_info("%sFailed to allocate (code_size=0x%x)\n",
				MY_TAG, krequest.code_size);
			krequest.returned_status = ESM_HL_DRIVER_NO_MEMORY;
		}
	}
	}

	ret = copy_to_user(request, &krequest,
		sizeof(struct esm_hld_ioctl_load_code));
	if (ret)
		pr_info("copy left %ld Bytes\n", ret);
	#if 0
	if (do_esm_rst_flag == 1)
		esm->code_loaded = 0;
	else
	#endif
		esm->code_loaded = (krequest.returned_status ==
		ESM_HL_DRIVER_SUCCESS);
	return 0;
}

static long cmd_load_code32(struct esm_device *esm,
	struct compact_esm_hld_ioctl_load_code *request)
{
	unsigned long ret = 0;
	ulong r;
	struct  compact_esm_hld_ioctl_load_code __user *uf = request;
	uint8_t *kernel_code;
	struct  compact_esm_hld_ioctl_load_code krequest;

	memset(&krequest, 0,
		sizeof(struct  compact_esm_hld_ioctl_load_code));
	r  = get_user(krequest.code, &uf->code);
	r |= get_user(krequest.code_size, &uf->code_size);
	if (r)
		return -EFAULT;

	if (do_esm_rst_flag == 1)
			esm->code_loaded = 0;

	if (esm->code_loaded == 1) {
		pr_info("%scmd_load_code: Code already loaded.\n", MY_TAG);
		krequest.returned_status = ESM_HL_DRIVER_SUCCESS;
	} else {
	if (krequest.code_size > esm->code_size) {
		pr_info("%sCode size larger than code memory (0x%x > 0x%lx).\n",
		MY_TAG, krequest.code_size, esm->code_size);
		krequest.returned_status = ESM_HL_DRIVER_NO_MEMORY;
	} else {
		kernel_code = kmalloc(krequest.code_size, GFP_KERNEL);

	if (kernel_code) {
		pr_info("%s[%d]\n", __func__, __LINE__);
		/* No Endian shift */
		ret = copy_from_user(esm->code, compat_ptr(krequest.code),
			krequest.code_size);
		if (ret)
			pr_info("copy %ld Bytes\n", ret);
	/*
	// Endian shift
	uint offset;

	copy_from_user(kernel_code, krequest.code, krequest.code_size);

	// Endian shift of firmware image
	for (offset = 0; offset < esm->code_size - 4; offset += 4)
	{
	esm->code[offset + 3] = kernel_code[offset];
	esm->code[offset + 2] = kernel_code[offset + 1];
	esm->code[offset + 1] = kernel_code[offset + 2];
	esm->code[offset]     = kernel_code[offset + 3];
	}
	*/

		kfree(kernel_code);
		krequest.returned_status = ESM_HL_DRIVER_SUCCESS;

		if (verbose)
			pr_info("%scopying firmware to code memory region.\n",
				MY_TAG);
		} else {
			pr_info("%sFailed to allocat memory (code_size=0x%x)\n",
				MY_TAG, krequest.code_size);
			krequest.returned_status = ESM_HL_DRIVER_NO_MEMORY;
		}
	}
	}

	ret = copy_to_user(request, &krequest,
		sizeof(struct compact_esm_hld_ioctl_load_code));
	if (ret)
		pr_info("copy left %ld Bytes\n", ret);

	#if 0
		if (do_esm_rst_flag == 1)
			esm->code_loaded = 0;
		else
	#endif
		esm->code_loaded = (krequest.returned_status ==
		ESM_HL_DRIVER_SUCCESS);

	return 0;
}
/* Returns the physical address of the code */
static long cmd_get_code_phys_addr(struct esm_device *esm,
	struct esm_hld_ioctl_get_code_phys_addr *request)
{
	unsigned long ret = 0;
	struct esm_hld_ioctl_get_code_phys_addr krequest;

	krequest.returned_phys_addr = esm->code_base;
	krequest.returned_status = ESM_HL_DRIVER_SUCCESS;

	if (verbose) {
		pr_info("%scmd_get_code_phys_addr: returning code_base=0x%x\n",
			MY_TAG, krequest.returned_phys_addr);
	}

	ret = copy_to_user(request, &krequest,
		sizeof(struct esm_hld_ioctl_get_code_phys_addr));
	if (ret)
		pr_info("copy left %ld Bytes\n", ret);
	return 0;
}

/* Returns the physical address of the data */
static long cmd_get_data_phys_addr(struct esm_device *esm,
	struct esm_hld_ioctl_get_data_phys_addr *request)
{
	unsigned long ret = 0;
	struct esm_hld_ioctl_get_data_phys_addr krequest;

	krequest.returned_phys_addr = esm->data_base;
	krequest.returned_status = ESM_HL_DRIVER_SUCCESS;

	esm_data_base_addr = (int)krequest.returned_phys_addr;
	rx_pr("%scmd_get_data_phys_addr: returning data_base=0x%x\n",
		MY_TAG, krequest.returned_phys_addr);

	ret = copy_to_user(request, &krequest,
		sizeof(struct esm_hld_ioctl_get_data_phys_addr));
	if (ret)
		pr_info("copy left %ld Bytes\n", ret);
	return 0;
}

/* Returns the size of the data memory region */
static long cmd_get_data_size(struct esm_device *esm,
	struct esm_hld_ioctl_get_data_size *request)
{
	unsigned long ret = 0;
	struct esm_hld_ioctl_get_data_size krequest;

	krequest.returned_data_size = esm->data_size;
	krequest.returned_status = ESM_HL_DRIVER_SUCCESS;

	rx_pr("%scmd_get_data_size: returning data_size=0x%x\n",
		MY_TAG, krequest.returned_data_size);

	ret = copy_to_user(request, &krequest,
		sizeof(struct esm_hld_ioctl_get_data_size));
	if (ret)
		pr_info("copy left %ld Bytes\n", ret);
	return 0;
}

/* Reads a single 32-bit HPI register */
static long cmd_hpi_read(struct esm_device *esm,
	struct esm_hld_ioctl_hpi_read *request)
{
	unsigned long ret = 0;
	struct esm_hld_ioctl_hpi_read krequest;

	ret = copy_from_user(&krequest, request,
		sizeof(struct esm_hld_ioctl_hpi_read));
	if (ret)
		pr_info("copy left %ld Bytes\n", ret);
	if (verbose) {
		pr_info("%scmd_hpi_read: Reading register at offset 0x%x\n",
			MY_TAG, krequest.offset);
	}

	if (esm->hpi) {
		krequest.returned_data = rx_hdcp22_rd(krequest.offset);
		krequest.returned_status = ESM_HL_DRIVER_SUCCESS;

		if (verbose) {
			pr_info("%scmd_hpi_read: Returning data=0x%x\n",
			MY_TAG, krequest.returned_data);
		}
	} else {
		krequest.returned_data = 0;
		krequest.returned_status = ESM_HL_DRIVER_NO_MEMORY;
		pr_info("%scmd_hpi_read: No memory.\n", MY_TAG);
	}

	ret = copy_to_user(request, &krequest,
		sizeof(struct esm_hld_ioctl_hpi_read));
	if (ret)
		pr_info("copy left %ld Bytes\n", ret);
	return 0;
}

/* Writes a single 32-bit HPI register */
static long cmd_hpi_write(struct esm_device *esm,
	struct esm_hld_ioctl_hpi_write *request)
{
	unsigned long ret = 0;
	struct esm_hld_ioctl_hpi_write krequest;

	ret = copy_from_user(&krequest, request,
		sizeof(struct esm_hld_ioctl_hpi_write));
	if (ret)
		pr_info("copy left %ld Bytes\n", ret);

	if (verbose) {
		pr_info("%sWriting 0x%X to register at offset 0x%X\n",
			MY_TAG, krequest.data, krequest.offset);
	}

	if (esm->hpi) {
		rx_hdcp22_wr_reg(krequest.offset, (uint32_t)krequest.data);
		krequest.returned_status = ESM_HL_DRIVER_SUCCESS;

		if (verbose) {
			pr_info("%sWrote 0x%X to register at offset 0x%X\n",
				MY_TAG, krequest.data, krequest.offset);
		}
	} else {
		krequest.returned_status = ESM_HL_DRIVER_NO_MEMORY;
		pr_info("%scmd_hpi_write: No memory.\n", MY_TAG);
	}

	ret = copy_to_user(request, &krequest,
		sizeof(struct esm_hld_ioctl_hpi_write));
	if (ret)
		pr_info("copy left %ld Bytes\n", ret);
	return 0;
}

/* Reads from a region of the data memory */
static long cmd_data_read(struct esm_device *esm,
	struct esm_hld_ioctl_data_read *request)
{
	unsigned long ret = 0;
	struct esm_hld_ioctl_data_read krequest;

	ret = copy_from_user(&krequest, request,
		sizeof(struct esm_hld_ioctl_data_read));
	if (ret)
		pr_info("copy left %ld Bytes\n", ret);
	if (verbose) {
		pr_info("%sReading %u bytes at offset offset 0x%x\n",
			MY_TAG, krequest.nbytes, krequest.offset);
	}

	if (esm->data) {
		if (krequest.offset + krequest.nbytes > esm->data_size) {
			krequest.returned_status = ESM_HL_DRIVER_INVALID_PARAM;
			pr_info("%scmd_data_read: Invalid offset and size.\n",
				MY_TAG);
		} else {
			ret = copy_to_user(krequest.dest_buf,
				esm->data + krequest.offset, krequest.nbytes);
			if (ret)
				pr_info("copy left %ld Bytes\n", ret);
			krequest.returned_status = ESM_HL_DRIVER_SUCCESS;

			if (verbose) {
				pr_info("%sreading %u at offset 0x%x\n",
					MY_TAG, krequest.nbytes,
					krequest.offset);
			}
		}
	} else {
		krequest.returned_status = ESM_HL_DRIVER_NO_MEMORY;
		pr_info("%scmd_data_read: No memory.\n", MY_TAG);
	}

	ret = copy_to_user(request, &krequest,
		sizeof(struct esm_hld_ioctl_data_read));
	if (ret)
		pr_info("copy left %ld Bytes\n", ret);
	return 0;
}
#if 1
/* Reads from a region of the data memory */
static long cmd_data_read32(struct esm_device *esm,
	struct compact_esm_hld_ioctl_data_read *request)
{
	unsigned long ret = 0;
	struct compact_esm_hld_ioctl_data_read krequest;

	ret = copy_from_user(&krequest, request,
		sizeof(struct compact_esm_hld_ioctl_data_read));
	if (ret)
		pr_info("copy left %ld Bytes\n", ret);
	if (verbose) {
		pr_info("%sReading %u offset offset 0x%x\n",
			MY_TAG, krequest.nbytes, krequest.offset);
	}

	if (esm->data) {
		if (krequest.offset + krequest.nbytes > esm->data_size) {
			krequest.returned_status = ESM_HL_DRIVER_INVALID_PARAM;
			pr_info("%scmd_data_read: Invalid offset and size.\n",
				MY_TAG);
		} else {
			ret = copy_to_user(compat_ptr(krequest.dest_buf),
				esm->data + krequest.offset, krequest.nbytes);
		if (ret)
			pr_info("copy left %ld Bytes\n", ret);
		krequest.returned_status = ESM_HL_DRIVER_SUCCESS;

		if (verbose) {
			pr_info("%sreading %u bytes at offset 0x%x\n",
				MY_TAG, krequest.nbytes, krequest.offset);
		}
	}
	} else {
		krequest.returned_status = ESM_HL_DRIVER_NO_MEMORY;
		pr_info("%scmd_data_read: No memory.\n", MY_TAG);
	}

	ret = copy_to_user(request, &krequest,
		sizeof(struct compact_esm_hld_ioctl_data_read));
	if (ret)
		pr_info("copy left %ld Bytes\n", ret);
	return 0;
}
#endif
/* Writes to a region of the data memory */
static long cmd_data_write(struct esm_device *esm,
	struct esm_hld_ioctl_data_write *request)
{
	unsigned long ret = 0;
	struct esm_hld_ioctl_data_write krequest;

	ret = copy_from_user(&krequest, request,
		sizeof(struct esm_hld_ioctl_data_write));
	if (ret)
		pr_info("copy left %ld Bytes\n", ret);
	if (verbose) {
		pr_info("%sWriting %u bytes to data memory at offset 0x%x\n",
			MY_TAG, krequest.nbytes, krequest.offset);
	}

	if (esm->data) {
		if (krequest.offset + krequest.nbytes > esm->data_size) {
			krequest.returned_status = ESM_HL_DRIVER_INVALID_PARAM;
			pr_info("%scmd_data_write: Invalid offset and size.\n",
				MY_TAG);
		} else {
			ret = copy_from_user(esm->data + krequest.offset,
				krequest.src_buf, krequest.nbytes);
			if (ret)
				pr_info("copy left %ld Bytes\n", ret);
			krequest.returned_status = ESM_HL_DRIVER_SUCCESS;

			if (verbose) {
				pr_info("%swriting %u to 0x%x\n",
					MY_TAG, krequest.nbytes,
					krequest.offset);
			}
		}
	} else {
		krequest.returned_status = ESM_HL_DRIVER_NO_MEMORY;
		pr_info("%scmd_data_write: No memory.\n", MY_TAG);
	}

	ret = copy_to_user(request, &krequest,
		sizeof(struct esm_hld_ioctl_data_write));
	if (ret)
		pr_info("copy left %ld Bytes\n", ret);
	return 0;
}
#if 1
/* Writes to a region of the data memory */
static long cmd_data_write32(struct esm_device *esm,
	struct compact_esm_hld_ioctl_data_write *request)
{
	unsigned long ret = 0;
	struct compact_esm_hld_ioctl_data_write krequest;

	ret = copy_from_user(&krequest, request,
		sizeof(struct compact_esm_hld_ioctl_data_write));
	if (ret)
		pr_info("copy left %ld Bytes\n", ret);
	if (verbose) {
		pr_info("%sWriting %u bytes to memory at offset 0x%x\n",
			MY_TAG, krequest.nbytes, krequest.offset);
	}

	if (esm->data) {
		if (krequest.offset + krequest.nbytes > esm->data_size) {
			krequest.returned_status = ESM_HL_DRIVER_INVALID_PARAM;
			pr_info("%scmd_data_write: Invalid offset and size.\n",
				MY_TAG);
		} else {
		ret = copy_from_user(esm->data + krequest.offset,
			compat_ptr(krequest.src_buf), krequest.nbytes);
			if (ret)
				pr_info("copy %ld Bytes\n", ret);
			krequest.returned_status = ESM_HL_DRIVER_SUCCESS;

			if (verbose) {
				pr_info("%swriting %u at offset 0x%x\n",
					MY_TAG, krequest.nbytes,
					krequest.offset);
			}
		}
	} else {
		krequest.returned_status = ESM_HL_DRIVER_NO_MEMORY;
		pr_info("%scmd_data_write: No memory.\n", MY_TAG);
	}

	ret = copy_to_user(request, &krequest,
		sizeof(struct compact_esm_hld_ioctl_data_write));
	if (ret)
		pr_info("copy %ld Bytes\n", ret);
	return 0;
}
#endif
/* Sets a region of the data memory to a given 8-bit value */
static long cmd_data_set(struct esm_device *esm,
	struct esm_hld_ioctl_data_set *request)
{
	unsigned long ret = 0;
	struct esm_hld_ioctl_data_set krequest;

	ret = copy_from_user(&krequest, request,
		sizeof(struct esm_hld_ioctl_data_set));
	if (ret)
		pr_info("copy left %ld Bytes\n", ret);
	if (verbose) {
		pr_info("%sSetting %u bytes (data=0x%x) from offset 0x%x\n",
			MY_TAG, krequest.nbytes, krequest.data,
			krequest.offset);
	}

	if (esm->data) {
		if (krequest.offset + krequest.nbytes > esm->data_size) {
			krequest.returned_status = ESM_HL_DRIVER_INVALID_PARAM;
			pr_info("%scmd_data_set: Invalid offset and size.\n",
				MY_TAG);
		} else {
			memset(esm->data + krequest.offset, krequest.data,
				krequest.nbytes);
			krequest.returned_status = ESM_HL_DRIVER_SUCCESS;

			if (verbose) {
				pr_info("%ssetting %u data=0x%x offset 0x%x\n",
					MY_TAG, krequest.nbytes, krequest.data,
					krequest.offset);
			}
		}
	} else {
		krequest.returned_status = ESM_HL_DRIVER_NO_MEMORY;
		pr_info("%scmd_data_set: No memory.\n", MY_TAG);
	}

	ret = copy_to_user(request, &krequest,
		sizeof(struct esm_hld_ioctl_data_set));
	if (ret)
		pr_info("copy left %ld Bytes\n", ret);
	return 0;
}

/* Opens an ESM device. Associates a device file to an ESM device. */
static long cmd_esm_open(struct file *f,
	struct esm_hld_ioctl_esm_open *request)
{
	unsigned long r = 0;
	int i;
	struct esm_device *esm = esm_devices;
	int ret = ESM_HL_DRIVER_SUCCESS;
	struct esm_hld_ioctl_esm_open krequest;

	r = copy_from_user(&krequest, request,
		sizeof(struct esm_hld_ioctl_esm_open));
	if (r)
		pr_info("copy left %ld Bytes\n", r);
	f->private_data = NULL;
	/* Look for a matching ESM device (based on HPI address) */
	for (i = MAX_ESM_DEVICES; i--; esm++) {
		if (esm->allocated && (krequest.hpi_base == esm->hpi_base)) {
			/* Found it */
			f->private_data = esm;
			break;
		}
	}

	if (!f->private_data) {
		/* Not found. Allocate a new ESM device. */
		esm = esm_devices;

		for (i = MAX_ESM_DEVICES; i--; esm++) {
			if (!esm->allocated) {
				dma_addr_t dh;
				char region_name[20];

				esm->allocated = 1;
				esm->hpi_base  = krequest.hpi_base;
				/* this can be static since the HPI
				 * interface will not change */
				esm->hpi_size  = 0x100;
				esm->code_base = krequest.code_base;
				esm->code_size = krequest.code_size;
				esm->data_base = krequest.data_base;
				esm->data_size = krequest.data_size;

				pr_info("%sNew ESM device:\n\n", MY_TAG);
				pr_info("   hpi_base: 0x%lx\n", esm->hpi_base);
				pr_info("   hpi_size: 0x%lx\n", esm->hpi_size);
				pr_info("  code_base: 0x%lx\n", esm->code_base);
				pr_info("  code_size: 0x%lx\n", esm->code_size);
				pr_info("  data_base: 0x%lx\n", esm->data_base);
				pr_info("  data_size: 0x%lx\n\n",
					esm->data_size);

				/* Initialize the code memory */
				if (esm->code_base) {
					esm->code_is_phys_mem = 1;
					esm->code =
						phys_to_virt(esm->code_base);
					pr_info("Code is at PhyAddr 0x%lx\n",
						esm->code_base);
				} else {
					esm->code = dma_alloc_coherent(
						(struct device *)esm,
						esm->code_size, &dh,
						GFP_KERNEL);

				if (!esm->code) {
					pr_info("%sFailed alloca (%ld bytes)\n",
						MY_TAG, esm->code_size);
					esm->code_alloc_err = TRUE;
					ret = ESM_HL_DRIVER_NO_MEMORY;
					break;
				}
			esm->code_alloc_err = FALSE;
			esm->code_base = dh;
			pr_info("%sBase allocated: phys=0x%lx virt=%p\n",
				MY_TAG, esm->code_base, esm->code);
		}

		/* Initialize the data memory */
		if (esm->data_base) {
			esm->data_is_phys_mem = 1;
			esm->data = phys_to_virt(esm->data_base);
			pr_info("Data is at physical address 0x%lx\n",
				esm->code_base);
		} else {
			esm->data = dma_alloc_coherent((struct device *)esm,
				esm->data_size, &dh, GFP_KERNEL);

			if (!esm->data) {
				pr_info("%sFailed to allocate (%ld bytes)\n",
					MY_TAG, esm->data_size);
				esm->data_alloc_err = TRUE;
				ret = ESM_HL_DRIVER_NO_MEMORY;
				break;
			}
			esm->data_alloc_err = FALSE;
			esm->data_base = dh;
			pr_info("%sBaseAddr of allocated: phys=0x%lx virt=%p\n",
				MY_TAG, esm->data_base, esm->data);
		}

		/* Init HPI access */
		sprintf(region_name, "ESM-%lX", esm->hpi_base);
		request_mem_region(esm->hpi_base, esm->hpi_size, region_name);
		esm->hpi_mem_region_requested = 1;
		esm->hpi = ioremap_nocache(esm->hpi_base, esm->hpi_size);

		/* Associate the Linux file to the ESM device */
		f->private_data = esm;
		break;
		}
	}
	}

	if (!f->private_data) {
		pr_info("%scmd_esm_open: Too many ESM devices.\n", MY_TAG);
		ret = ESM_HL_DRIVER_TOO_MANY_ESM_DEVICES;
	}


	/* create the /dev/esmmem */
	if (!is_esmmem_created) {
		if ((esm->data_base) && (esm->code_base)) {
			hdmirx_dev_init();
			is_esmmem_created = 1;
			pr_info("create /dev/esmmem\n");
		}
	}
	krequest.returned_status = ret;
	r = copy_to_user(request, &krequest,
		sizeof(struct esm_hld_ioctl_esm_open));
	if (r)
		pr_info("copy left %ld Bytes\n", r);
	return 0;
}

bool esm_print_device_info(void)
{
	int i;
	for (i = 0; i < MAX_ESM_DEVICES; i++) {
		if (esm_devices[i].allocated &&
			esm_devices[i].hpi_base == ESM_RX_HPI_BASE)
			dump_device_info(&esm_devices[i]);
			return TRUE;
	}
	return FALSE;
}

/*---------------------------------------------------------------------------
//                                Linux Device
//-------------------------------------------------------------------------*/

/* The device has been opened */
static int device_open(struct inode *inode, struct file *filp)
{
	if (verbose)
		pr_info("%sDevice opened.\n", MY_TAG);

	/* No associated ESM device yet.
	* Use IOCTL ESM_HLD_IOCTL_ESM_OPEN to associate an ESM
	* to the opened device file.
	*/
	filp->private_data = NULL;

	return 0;
}

/* The device has been closed */
static int device_release(struct inode *inode, struct file *filp)
{
	if (verbose)
		pr_info("%sDevice released.\n", MY_TAG);


	return 0;
}

/* IOCTL operation on the device */
static long device_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
	arg = (unsigned long)compat_ptr(arg);

	switch (cmd) {
	case ESM_HLD_IOCTL_LOAD_CODE:
		return cmd_load_code(
			(struct esm_device *)f->private_data,
			(struct esm_hld_ioctl_load_code *)arg);

	case ESM_HLD_IOCTL_LOAD_CODE32:
		return cmd_load_code32(
			(struct esm_device *)f->private_data,
			(struct compact_esm_hld_ioctl_load_code *)arg);

	case ESM_HLD_IOCTL_GET_CODE_PHYS_ADDR:
		return cmd_get_code_phys_addr(
			(struct esm_device *)f->private_data,
			(struct esm_hld_ioctl_get_code_phys_addr *)arg);

	case ESM_HLD_IOCTL_GET_DATA_PHYS_ADDR:
		return cmd_get_data_phys_addr(
			(struct esm_device *)f->private_data,
			(struct esm_hld_ioctl_get_data_phys_addr *)arg);

	case ESM_HLD_IOCTL_GET_DATA_SIZE:
		return cmd_get_data_size(
			(struct esm_device *)f->private_data,
			(struct esm_hld_ioctl_get_data_size *)arg);

	case ESM_HLD_IOCTL_HPI_READ:
		return cmd_hpi_read(
		(struct esm_device *)f->private_data,
			(struct esm_hld_ioctl_hpi_read *)arg);

	case ESM_HLD_IOCTL_HPI_WRITE:
		return cmd_hpi_write(
		(struct esm_device *)f->private_data,
			(struct esm_hld_ioctl_hpi_write *)arg);

	case ESM_HLD_IOCTL_DATA_READ:
		return cmd_data_read(
		(struct esm_device *)f->private_data,
			(struct esm_hld_ioctl_data_read *)arg);

	case ESM_HLD_IOCTL_DATA_READ32:
		return cmd_data_read32(
		(struct esm_device *)f->private_data,
			(struct compact_esm_hld_ioctl_data_read *)arg);

	case ESM_HLD_IOCTL_DATA_WRITE:
		return cmd_data_write(
		(struct esm_device *)f->private_data,
			(struct esm_hld_ioctl_data_write *)arg);

	case ESM_HLD_IOCTL_DATA_WRITE32:
		return cmd_data_write32(
		(struct esm_device *)f->private_data,
			(struct compact_esm_hld_ioctl_data_write *)arg);

	case ESM_HLD_IOCTL_DATA_SET:
		return cmd_data_set(
		(struct esm_device *)f->private_data,
			(struct esm_hld_ioctl_data_set *)arg);

	case ESM_HLD_IOCTL_ESM_OPEN:
		return cmd_esm_open(f, (struct esm_hld_ioctl_esm_open *)arg);
	}


	return -1;
}

static void dump_device_info(struct esm_device *dev)
{
	pr_info("  --------------esm info-------------------\n");
	pr_info("  allocated = %d\n", dev->allocated);
	pr_info("  code_loaded = %d\n", dev->code_loaded);
	pr_info("  code_is_phys_mem = %d\n", dev->code_is_phys_mem);
	pr_info("  code_base = 0x%lx\n", dev->code_base);
	pr_info("  code_size = %lu\n", dev->code_size);
	pr_info("  code = 0x%p\n", dev->code);
	pr_info("  data_is_phys_mem = %d\n", dev->data_is_phys_mem);
	pr_info("  data_base = 0x%lx\n", dev->data_base);
	pr_info("  data_size = %lu\n", dev->data_size);
	pr_info("  data = 0x%p\n", dev->data);
	pr_info("  code alloc error = %d\n", dev->code_alloc_err);
	pr_info("  data alloc error = %d\n", dev->data_alloc_err);
	pr_info("  hpi_base = 0x%lx\n", dev->hpi_base);
	pr_info("  hpi_size = %lu\n", dev->hpi_size);
	pr_info("  hpi = 0x%p\n", dev->hpi);
	pr_info("  hpi_mem_region_requested = %d\n",
		dev->hpi_mem_region_requested);
}

static void dump_device_raw(const uint8_t *buf, uint32_t size)
{
	uint32_t i;
	pr_info("Dump Addr: 0x%p   Size: %d\n", buf, size);
	pr_info("==============DUMP START==============\n");
	for (i = 0; i < size; i += 8)
		pr_info("%02x %02x %02x %02x %02x %02x %02x %02x\n",
			buf[i], buf[i+1], buf[i+2], buf[i+3], buf[i+4],
			buf[i+5], buf[i+6], buf[i+7]);
	pr_info("==============DUMP END  ==============\n");
}

#define DUMP_SECTION(prefix, start, end) \
do { \
	if (start > end) { \
		pr_info("Error start = 0x%x > end = 0x%x\n", (uint32_t)start, \
			(uint32_t)end); \
		break; \
	} \
	pr_info("Dump %s\n", prefix); \
	for (addr = start; addr < end + 1; addr++) {	\
		val = hdmitx_rd_reg(addr); \
		if (val) \
			pr_info("[0x%08x]: 0x%08x\n", addr, val); \
	} \
} while (0)
#if 0

static void dump_snps_regs(void)
{


	unsigned int addr;
	unsigned int val;

	DUMP_SECTION("TOP", HDMITX_TOP_SW_RESET, HDMITX_TOP_STAT0);
	DUMP_SECTION("TOPSec", HDMITX_TOP_SKP_CNTL_STAT, HDMITX_TOP_DUK_3);
	DUMP_SECTION("TOP", HDMITX_TOP_INFILTER, HDMITX_TOP_NSEC_SCRATCH);
	DUMP_SECTION("TOPSec", HDMITX_TOP_SEC_SCRATCH, HDMITX_TOP_SEC_SCRATCH);
	DUMP_SECTION("TOP", HDMITX_TOP_DONT_TOUCH0, HDMITX_TOP_DONT_TOUCH1);
	DUMP_SECTION("DWC", HDMITX_DWC_DESIGN_ID, HDMITX_DWC_CSC_LIMIT_DN_LSB);
	DUMP_SECTION("DWCSec", HDMITX_DWC_A_HDCPCFG0, HDMITX_DWC_A_HDCPCFG1);
	DUMP_SECTION("DWC", HDMITX_DWC_A_HDCPOBS0, HDMITX_DWC_A_KSVMEMCTRL);
	/* Exclude HDMITX_DWC_HDCP_BSTATUS_0 to HDMITX_DWC_HDCPREG_RMLCTL */
	DUMP_SECTION("DWC", HDMITX_DWC_HDCPREG_RMLSTS,
		HDMITX_DWC_HDCPREG_RMLSTS);
	DUMP_SECTION("DWCSec", HDMITX_DWC_HDCPREG_SEED0,
		HDMITX_DWC_HDCPREG_DPK6);
	DUMP_SECTION("DWC", HDMITX_DWC_HDCP22REG_ID,
		HDMITX_DWC_HDCP22REG_ID);
	DUMP_SECTION("DWCSec", HDMITX_DWC_HDCP22REG_CTRL,
		HDMITX_DWC_HDCP22REG_CTRL);
	DUMP_SECTION("DWC", HDMITX_DWC_HDCP22REG_CTRL1,
		HDMITX_DWC_I2CM_SCDC_UPDATE1);

}
#endif

#define DBG_HELP_STR    \
	"dev_infoN: show the Nth esm devices info\n"    \
	"dumpesm: deump ESM registers\n" \
	"dumpsnps: dump SNPS registers\n" \
	"reset: reset ESM cpu\n"

static ssize_t show_debug(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int pos = 0;
	static const char *dbg_help = DBG_HELP_STR;

	pos += snprintf(buf+pos, PAGE_SIZE, "%s", dbg_help);
	return pos;
}

static ssize_t store_debug(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;
	unsigned long idx;
	if (strncmp(buf, "dev_info", 8) == 0) {
		ret = kstrtoul(buf+8, 10, &idx);
		if (idx >= MAX_ESM_DEVICES)
			pr_info("MAX_ESM_DEVICES is %d\n", MAX_ESM_DEVICES);
		else {
			pr_info("esm_devices NO %lu\n", idx);
			dump_device_info(&esm_devices[idx]);
		}
		return 16;
	}
	if (strncmp(buf, "hwinit", 6) == 0) {
		/* hdcp22_hw_init(); */
		return 16;
	}
	if (strncmp(buf, "dumpesm", 7) == 0) {
		uint32_t i;
		uint32_t val;
		for (i = 0; i < 0x100; i += 4) {
			val = rx_hdcp22_rd_reg(i);
			if (val)
				pr_info("hdcp22 0x%02x: 0x%08x\n", i, val);
		}
		return 16;
	}
	if (strncmp(buf, "dumpcode", 8) == 0) {
		ret = kstrtoul(buf+8, 10, &idx);
		if (idx >= MAX_ESM_DEVICES)
			pr_info("MAX_ESM_DEVICES is %d\n", MAX_ESM_DEVICES);
		else {
			pr_info("esm_devices NO %lu\n", idx);
			dump_device_raw((uint8_t *)esm_devices[idx].code,
				esm_devices[idx].code_size);
		}
		return 16;
	}
	if (strncmp(buf, "dumpdata", 8) == 0) {
		ret = kstrtoul(buf+8, 10, &idx);
		if (idx >= MAX_ESM_DEVICES)
			pr_info("MAX_ESM_DEVICES is %d\n", MAX_ESM_DEVICES);
		else {
			pr_info("esm_devices NO %lu\n", idx);
			dump_device_raw((uint8_t *)esm_devices[idx].data,
				esm_devices[idx].data_size);
		}
		return 16;
	}
	if (strncmp(buf, "dumphpi", 7) == 0) {
		/* sec_reg_test(); */
		return 16;
	}
	if (strncmp(buf, "fixrom", 6) == 0)
		return 16;

	if (strncmp(buf, "reset", 5) == 0)
		;

	return 16;
}

static DEVICE_ATTR(debug, S_IWUSR | S_IRUGO, show_debug, store_debug);

/* Creates the device required to interface with the HLD driver */
static int create_device(void)
{
	int ret = 0;
	pr_info("%sCreating device '%s'...\n",
		MY_TAG, ESM_DEVICE_NAME);

	device = device_create(device_class, NULL, MKDEV(ESM_DEVICE_MAJOR, 0),
	NULL, ESM_DEVICE_NAME);

	if (IS_ERR(device)) {
		pr_info("%sFailed to create device '%s'.\n",
			MY_TAG, ESM_DEVICE_NAME);
		return PTR_ERR(device);
	}
	ret = device_create_file(device, &dev_attr_debug);
	device_created = 1;
	pr_info("%sDevice '%s' has been created.\n",
		MY_TAG, ESM_DEVICE_NAME);

	return 0;
}

/* Destroys the interface device */
static void end_device(void)
{
	int i;
	struct esm_device *esm = esm_devices;

	if (device_created) {
		pr_info("%sDeleting device '%s'...\n",
			MY_TAG, ESM_DEVICE_NAME);
		device_remove_file(device, &dev_attr_debug);
		device_destroy(device_class, MKDEV(ESM_DEVICE_MAJOR, 0));
		device_created = 0;
	}

	for (i = MAX_ESM_DEVICES; i--; esm++) {
		if (esm->allocated) {
			if (esm->code && !esm->code_is_phys_mem) {
				dma_addr_t dh = (dma_addr_t)esm->code_base;
				dma_free_coherent(NULL, esm->code_size,
					esm->code, dh);
			}

			if (esm->data && !esm->data_is_phys_mem) {
				dma_addr_t dh = (dma_addr_t)esm->data_base;
				dma_free_coherent(NULL, esm->data_size,
					esm->data, dh);
			}

			if (esm->hpi)
				iounmap(esm->hpi);

			if (esm->hpi_mem_region_requested)
				release_mem_region(esm->hpi_base,
					esm->hpi_size);
		}
	}

	memset(esm_devices, 0, sizeof(esm_devices));
}

/*---------------------------------------------------------------------------
//                       Linux device class and range
//-------------------------------------------------------------------------*/

/* Table of the supported operations on ESM devices */
static const struct file_operations device_file_operations = {
	.open = device_open,
	.release = device_release,
/*   .unlocked_ioctl = device_ioctl, */
	.compat_ioctl = device_ioctl,
	.owner = THIS_MODULE,
};

static int register_device_range(void)
{
	int ret;

	pr_info("%sRegistering device range '%s'...\n",
		MY_TAG, ESM_DEVICE_NAME);

	ret = register_chrdev(ESM_DEVICE_MAJOR, ESM_DEVICE_NAME,
		&device_file_operations);

	if (ret < 0) {
		pr_info("%sFailed to register device range '%s'.\n",
			MY_TAG, ESM_DEVICE_NAME);
		return ret;
	}

	pr_info("%sDevice range '%s' has been registered.\n",
		MY_TAG, ESM_DEVICE_NAME);
	device_range_registered = 1;

	return 0;
}

static void unregister_device_range(void)
{
	if (device_range_registered) {
		pr_info("%sUnregistering device range '%s'...\n",
			MY_TAG, ESM_DEVICE_NAME);
		unregister_chrdev(ESM_DEVICE_MAJOR, ESM_DEVICE_NAME);
		device_range_registered = 0;
	}
}

/* Creates the interface device class.
//
// Note: Attributes could be created for that class.
//       Not required at this time.
*/
static int create_device_class(void)
{
	pr_info("%sCreating class /sys/class/%s...\n",
	MY_TAG, ESM_DEVICE_CLASS);

	device_class = class_create(THIS_MODULE, ESM_DEVICE_CLASS);

	if (IS_ERR(device_class)) {
		pr_info("%sFailed to create device class /sys/class/%s.\n",
			MY_TAG, ESM_DEVICE_CLASS);
		return PTR_ERR(device_class);
	}

	device_class_created = 1;
	pr_info("%sThe class /sys/class/%s has been created.\n",
		MY_TAG, ESM_DEVICE_CLASS);

	return 0;
}

/* Ends the device class of the ESM devices */
static void end_device_class(void)
{
	if (device_class_created) {
		pr_info("%sDeleting the device class /sys/class/%s...\n",
			MY_TAG, ESM_DEVICE_CLASS);
		class_destroy(device_class);
		device_class_created = 0;
	}
}
/* -------------------------------------------------------- */
/*              Initialization/termination of the module */
/* ---------------------------------------------------------*/

static int __init hld_init(void)
{
	pr_info("%sInitializing...\n", MY_TAG);

	memset(esm_devices, 0, sizeof(esm_devices));

	if ((register_device_range() == 0) &&
		(create_device_class() == 0) &&
		(create_device() == 0))
		pr_info("%sDone initializing the HLD driver.\n",
			MY_TAG);

	else
		pr_info("%sFailed to initialize the HLD driver.\n",
			MY_TAG);

	/* hdcp22_hw_init(); */
	return 0;
}

static void __exit hld_exit(void)
{
	pr_info("%sExiting...\n", MY_TAG);
	end_device();
	end_device_class();
	unregister_device_range();
	pr_info("%sDone.\n", MY_TAG);
}

module_init(hld_init);
module_exit(hld_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Elliptic Technologies");
MODULE_DESCRIPTION("ESM Linux Host Library Driver");

module_param(verbose, int, 0644);
MODULE_PARM_DESC(verbose, "Enable (1) or disable (0) the debug traces.");

#endif
