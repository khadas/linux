/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * File    : aml_usbcam.h
 * Purpose : USB CAM driver
 *
 */
#ifndef _AML_USBCAM_H
#define _AML_USBCAM_H

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kref.h>
#include <linux/usb.h>
#include <linux/mutex.h>
#include <asm/uaccess.h>
#include <linux/mm.h>
#include <linux/moduleparam.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/kthread.h>
#include <linux/gfp.h>
#include <linux/compat.h>
#include <asm/current.h>
#include <linux/poll.h>
#include <linux/fs.h>

#define AML_USBCAM_IOC_MAGIC					'c'
#define AML_USBCAM_IOC_GET_DRIVER_VERSION	_IOR(AML_USBCAM_IOC_MAGIC, 0, uint32_t)
#define AML_USBCAM_IOC_GET_INFO			_IOR(AML_USBCAM_IOC_MAGIC, 1, \
							struct aml_usbcam_info)
#define AML_USBCAM_IOC_RESET			_IO(AML_USBCAM_IOC_MAGIC, 2)
#define AML_USBCAM_IOC_CANCEL_TRANSFER		_IO(AML_USBCAM_IOC_MAGIC, 3)
#define AML_USBCAM_IOC_MODULE_CAPABILITIES	_IOR(AML_USBCAM_IOC_MAGIC, 4, \
							struct aml_usbcam_module_capabilities)

#define USBCAM_MAX_NAME_LEN 30

struct aml_usbcam_module_capabilities {
	char ci_manufacturer_name[USBCAM_MAX_NAME_LEN];
	char ci_product_name[USBCAM_MAX_NAME_LEN];
	bool ci_plus_supported;
	bool op_profile_supported;
};

struct aml_usbcam_info {
	unsigned short vendor_id;//vendor ID
	unsigned short product_id;//product id
	u32 ci_compatibility;//compatibility
	unsigned char is_ci20_detected;
	unsigned char arreserve[31];
};

#endif
