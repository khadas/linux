/**************************************************************************//**
 * @file    cimax+usb_fw.c
 *
 * @brief   CIMaX+ USB Driver for linux based operating systems.
 *
 * Copyright (C) 2009-2011    Bruno Tonelli   <bruno.tonelli@smardtv.com>
 *                          & Franck Descours <franck.descours@smardtv.com>
 *                            for SmarDTV France, La Ciotat
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 ******************************************************************************/
/******************************************************************************
 * Include
 ******************************************************************************/
#include <linux/slab.h>
#include <linux/init.h>

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/usb.h>

#include <linux/errno.h>
#include <linux/firmware.h>

#include <linux/uaccess.h>
#include <linux/mutex.h>

#include <linux/printk.h>

#include "cimax+usb-driver.h"
#include "cimax+usb_fw.h"

/******************************************************************************
 * Structures
 ******************************************************************************/
/******************************************************************************
 * Globals
 ******************************************************************************/
char cimax_fw[] = CIMAX_FIRMWARE_NAME;

/******************************************************************************
 * Functions
 ******************************************************************************/
/******************************************************************************
 * @brief
 *   Start & check Bistrom.
 *
 * @param   dev
 *   Pointer to usb device.
 *
 * @param   startAddr
 *   start address of firmware to compute
 *
 * @param   endAddr
 *   stop address of firmware to compute
 *
 * @param   FWSign
 *   Current signature to check
 *
 *
 * @return
 *   None.
 ******************************************************************************/
int checkBistRom(struct device_s *bus_adap,
	int startAddr, int endAddr, int signature)
{
	unsigned char *Val;
	int rv;

	struct usb_device *dev = bus_adap->usbdev;

	Val = kcalloc(2, sizeof(unsigned char), GFP_KERNEL);
	if (!Val) {
		pr_err("%s-%s: out of memory.\n", DRIVER_NAME, __func__);
		return -ENOMEM;
	}

	/* Write "Flash" Size
	   (!) BistRom is computed since D000-flashSize to D0000-6
	*/
	Val[0] = (0xD000-startAddr)&0x00ff;
	Val[1] = (0xD000-startAddr)>>8;
	if (bus_adap->ops->write_ctrl_message(dev, 0x008D, Val, 2) != 2) {
		kfree(Val);
		return -1;
	}

	/* Write Signature*/
	Val[0] = signature&0x00ff;
	Val[1] = signature>>8;
	if (bus_adap->ops->write_ctrl_message(dev, 0x0080, Val, 2) != 2) {
		kfree(Val);
		return -1;
	}

	/* Launch BistRom [(D000-flashSize)..CFF9]+[FFFA..FFFF] computation*/
	Val[0] = 0x0F;
	if (bus_adap->ops->write_ctrl_message(dev, 0x0082, Val, 1) != 1) {
		kfree(Val);
		return -1;
	}

	pr_info("\n>>Read Signature\n");

	/* Read Signature*/
	if (bus_adap->ops->read_ctrl_message(dev, 0x0041, Val, 2) != 2) {
		kfree(Val);
		return -1;
	}

	pr_info("\n>> Bistrom computed = %04X\n",  Val[0]|Val[1]<<8);

	/* Read Boot status*/
	if (bus_adap->ops->read_ctrl_message(dev, 0x0009, Val, 1) != 1) {
		kfree(Val);
		return -1;
	}

	pr_info("\n>> BootRom Status = %02X\n",  Val[0]);
	rv = Val[0];
	kfree(Val);

	return rv;
}

/******************************************************************************
 * @brief
 *   Compute BistRom.
 *
 * @param   ptr
 *   Pointer to buffer with register data
 *
 * @param   size
 *   Number of register to process
 *
 * @param   FWSign
 *   Current signature
 *
 * @return
 *   None.
 ******************************************************************************/
unsigned int MISR(const unsigned char *ptr, int size, unsigned int FWSign)
{
	int k, i;

	unsigned short mySign;

	for (k = 0; k < size; k++) {
		mySign = ptr[k]&0x01;

		for (i = 0; i < 16; i++) {
			if (0x88B7 & (1<<i))
				mySign ^= (FWSign>>i) & 0x01;
		}

		mySign |= ((FWSign<<1)^(ptr[k])) & 0x00FE;
		mySign |= (FWSign<<1) & 0x00FF00;

		FWSign = mySign;
	}

	return FWSign;
}


/******************************************************************************
 * @brief
 *   upload firmware in CIMaX+ chip.
 *
 * @param   dev
 *   Pointer to usb device.
 *
 * @param   fw_data
 *   Pointer to buffer with firmware data.
 *
 * @param   FWSign
 *   Pointer to store computed signature.
 *
 * @return
 *   None.
 ******************************************************************************/
int cimaxusb_firmware_upload(struct device_s *bus_adap,
	const unsigned char *fw_data, unsigned int *FWSign)
{
	int errno = -EFAULT;
	const unsigned char *ptr;
	int startAddr = START_FW_ADDR;
	unsigned char *recv;
	int size;

	struct usb_device *dev = bus_adap->usbdev;

	/* data firmware */
	ptr = fw_data;
	ptr += START_FW_ADDR;

	recv = kcalloc(300, sizeof(unsigned char), GFP_KERNEL);
	if (!recv) {
		pr_err("%s-%s: out of memory.\n", DRIVER_NAME, __func__);
		return -ENOMEM;
	}

	do {
		/* compute the size to send to CIMaX+ */
		size = (startAddr <= (STOP_FW_ADDR + 1 - MAX_FW_PKT_SIZE)) ?
			(MAX_FW_PKT_SIZE) : (STOP_FW_ADDR + 1 - startAddr);
		/* compute Signature */
		*FWSign = MISR(ptr, size, *FWSign);

		pr_info("%s: firmware start address %08x size %d\n",
			DRIVER_NAME, startAddr, size);

		/* upload data firmware */
		if (bus_adap->ops->write_ctrl_message(
				dev, startAddr, (void *)ptr, size) != size) {
			pr_err("Failed to load CIMaX+ firmware\n");
			errno = -ENODEV;
			break;
		}
		if (bus_adap->ops->read_ctrl_message(
				dev, startAddr, (void *)recv, size) != size) {
			pr_err("Failed to load CIMaX+ firmware\n");
			errno = -ENODEV;
			break;
		}
		if (memcmp(ptr, recv, size)) {
			pr_err("Failed compare at Address 0x%04x\n",
				startAddr);
			errno = -ENODEV;
			break;
		}
		/* update size sent to CIMaX+ */
		startAddr += size;
		ptr += size;
		if (startAddr >= STOP_FW_ADDR)
			break;
	} while (1);

	kfree(recv);

	if (errno == -EFAULT) {
		/* upload interrupt vector*/
		ptr = fw_data;
		ptr += START_INTVECT_ADDR;
		startAddr =  START_INTVECT_ADDR;

		/* continue to compute Signature */
		*FWSign = MISR(ptr, 6, *FWSign);

		/* upload interrupt vector data*/
		if (bus_adap->ops->write_ctrl_message(
				dev, startAddr, (void *)ptr, 6) != 6) {
			pr_err("Failed to load CIMaX firmware(Int vector)\n");
			errno = -ENODEV;
		}
	}
	return (errno == -EFAULT) ? 0 : errno;
}

/******************************************************************************
 * @brief
 *   upload and start firmware in CIMaX+ chip.
 *
 * @param   bus_adap
 *   Pointer to usb device.
 *
 * @return
 *   None.
 ******************************************************************************/
int cimaxusb_fw_upload(struct device_s *bus_adap)
{
	int errno = -EFAULT;
	const struct firmware *firmware;
	char *fw = cimax_fw;
	unsigned int FWSign = 0;
	int ret;

	struct usb_device *dev = bus_adap->usbdev;

	/* request kernel to locate firmware file */
	errno = request_firmware(&firmware, fw, &dev->dev);
	if (errno < 0) {
		pr_err("%s: unable to locate firmware file: %s\n",
				DRIVER_NAME, fw);
		goto error;
	}

	errno = cimaxusb_firmware_upload(bus_adap, firmware->data, &FWSign);
	if (errno < 0) {
		pr_err("%s: unable to upload firmware file: %s\n",
				DRIVER_NAME, fw);
		goto error;
	}

	pr_info("%s: firmware: %s loaded with success. Current Bistrom %04X\n",
			DRIVER_NAME, fw, FWSign);

	ret = checkBistRom(bus_adap, START_FW_ADDR, STOP_FW_ADDR, FWSign);
	if (ret != 0x02) {
		pr_err("\nError: Fail on compare BistRom (%02X) !\n", ret);
		errno = -ENODEV;
		goto error;
	}

	if (bus_adap->ops->init_fw(dev) != 0) {
		pr_err("\nError: Fail on INIT command !\n");
		errno = -ENODEV;
		goto error;
	}

error:
	/* release firmware if needed */
	if (firmware != NULL)
		release_firmware(firmware);
	return errno;
}
