/**************************************************************************//**
 * @file    cimax+usb_config.c
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

#include "cimax+usb-driver.h"
#include "cimax+usb_config.h"
#include "bodydef.h"

#include <linux/ctype.h>

#define DEBUG
#include <linux/printk.h>

/******************************************************************************
 * Structures
 ******************************************************************************/
/******************************************************************************
 * Globals
 ******************************************************************************/
char *cimax_config_file[4] = {
	"cimax+usb.cfg",
	"cimax+usb_vcc0.cfg",
	"cimax+usb_vcc3.cfg",
	"cimax+usb_vcc5.cfg"
};

/******************************************************************************
 * Functions
 ******************************************************************************/
/******************************************************************************
 * @brief
 *   set CIMaX+ register value.
 *
 * @param   bus_adap
 *   Pointer to CIMaX+ usb adapter.
 *
 * @param   addr
 *   Addr of CIMaX+ register.
 *
 * @param   val
 *   CIMaX+ register value to set.
 *
 * @return
 *   0 if OK otherwise -1.
 ******************************************************************************/
int usb_setbyte(struct device_s *bus_adap, unsigned int addr, unsigned char val)
{
	unsigned char cmd[5];
	unsigned char *buf;
	int len;
	struct usb_device *dev = bus_adap->usbdev;

	cmd[0] = 0x7F;
	cmd[1] = (unsigned char)((addr>>8)&0xff);
	cmd[2] = (unsigned char)(addr&0xff);
	cmd[3] = 0x01;
	cmd[4] = val;
	if (bus_adap->ops->write_ep6_message(dev, cmd, sizeof(cmd))
		== sizeof(cmd)) {
		/*pr_debug("%s-%s: cmd=0x%x\n",
			DRIVER_NAME, __func__,cmd[0]);*/
		buf = kcalloc(256, sizeof(unsigned char), GFP_KERNEL);
		if (!buf) {
			pr_err("%s-%s: out of memory.\n",
				DRIVER_NAME, __func__);
			return -ENOMEM;
		}

		while ((len = bus_adap->ops->read_ep5_message(dev, buf, 256))
				>= 0) {
			if (len == 0)
				continue;
			if ((len == 5) && ((buf[0] & 0x7f) == 0x45))
				continue;
			if (len < 4) {
				pr_err("%s-%s: failed to read addr 0x%x\n",
					DRIVER_NAME, __func__, addr);
				kfree(buf);
				return -1;
			} else {
				break;
			}
		}
	} else {
		pr_err("%s-%s: failed to write addr 0x%x\n",
			DRIVER_NAME, __func__, addr);
		return -1;
	}
	kfree(buf);
	return 0;
}

/******************************************************************************
 * @brief
 *   get CIMaX+ register value.
 *
 * @param   bus_adap
 *   Pointer to CIMaX+ usb adapter.
 *
 * @param   addr
 *   Addr of CIMaX+ register.
 *
 * @param   val
 *   pointer to buffer to store CIMaX+ register value.
 *
 * @return
 *   0 if OK otherwise -1.
 ******************************************************************************/
int usb_getbyte(struct device_s *bus_adap,
	unsigned int addr, unsigned char *val)
{
	unsigned char cmd[4];
	unsigned char *buf;
	int len;
	struct usb_device *dev = bus_adap->usbdev;

	cmd[0] = 0xFF;
	cmd[1] = (unsigned char)((addr>>8)&0xff);
	cmd[2] = (unsigned char)(addr&0xff);
	cmd[3] = 0x01;
	if (bus_adap->ops->write_ep6_message(dev, cmd, sizeof(cmd))
			== sizeof(cmd)) {
		/*pr_debug("%s-%s: cmd=0x%x\n",
			DRIVER_NAME, __func__,cmd[0]);*/
		buf = kcalloc(256, sizeof(unsigned char), GFP_KERNEL);
		if (!buf) {
			pr_err("%s-%s: out of memory.\n",
				DRIVER_NAME, __func__);
			return -ENOMEM;
		}

		while ((len = bus_adap->ops->read_ep5_message(dev, buf, 256))
				>= 0) {
			if (len == 0)
				continue;
			/*pr_debug("%s-%s: 0x%x\n",
				DRIVER_NAME, __func__,buf[0]);*/
			if ((len == 5) && ((buf[0] & 0x7f) == 0x45))
				continue;
			if (len >= 5) {
				*val = buf[4];
				break;
			} else {
				pr_err("%s-%s: failed to read addr 0x%x\n",
					DRIVER_NAME, __func__, addr);
				kfree(buf);
				return -1;
			}
		}
	} else {
		pr_err("%s-%s: failed to read addr 0x%x\n",
			DRIVER_NAME, __func__, addr);
		return -1;
	}
	kfree(buf);
	return 0;
}

/******************************************************************************
 * @brief
 *   compute a logical Or between CIMaX+ register value and a param value.
 *
 * @param   bus_adap
 *   Pointer to CIMaX+ usb adapter.
 *
 * @param   addr
 *   Addr of CIMaX+ register.
 *
 * @param   val
 *   value to compute.
 *
 * @param   pval
 *   pointer to buffer to store CIMaX+ register value.
 *
 * @return
 *   0 if OK otherwise -1.
 ******************************************************************************/
int setLogicalOr(struct device_s *bus_adap,
		unsigned int addr, unsigned char val, unsigned char *pval)
{
	*pval |= val;

	if (usb_setbyte(bus_adap, addr, *pval) < 0)
		return -1;

	if (usb_getbyte(bus_adap, addr, pval) < 0)
		return -1;

	dbg("=> Logical OR [%02X] => %02X\n", val, *pval);

	return 0;
}

/******************************************************************************
 * @brief
 *   compute a logical And between CIMaX+ register value and a param value.
 *
 * @param   bus_adap
 *   Pointer to CIMaX+ usb adapter.
 *
 * @param   addr
 *   Addr of CIMaX+ register.
 *
 * @param   val
 *   value to compute.
 *
 * @param   pval
 *   pointer to buffer to store CIMaX+ register value.
 *
 * @return
 *   0 if OK otherwise -1.
 ******************************************************************************/
int setLogicalAnd(struct device_s *bus_adap,
		unsigned int addr, unsigned char val, unsigned char *pval)
{
	*pval &= val;

	if (usb_setbyte(bus_adap, addr, *pval) < 0)
		return -1;

	if (usb_getbyte(bus_adap, addr, pval) < 0)
		return -1;

	dbg("=> Logical AND [%02X] => %02X\n", val, *pval);

	return 0;
}

/******************************************************************************
 * @brief
 *   wait CIMaX+ register value match a param value.
 *
 * @param   bus_adap
 *   Pointer to CIMaX+ usb adapter.
 *
 * @param   addr
 *   Addr of CIMaX+ register.
 *
 * @param   val
 *   value to match.
 *
 * @param   pval
 *   pointer to buffer to store CIMaX+ register value.
 *
 * @return
 *   0 if OK otherwise -1.
 ******************************************************************************/
int waitForValue(struct device_s *bus_adap,
		unsigned int addr, unsigned char val, unsigned char *pval)
{
	dbg("=> Wait for Value [%02X]\n", val);
	if (*pval == val)
		return 0;

	while (1) {
		if (usb_getbyte(bus_adap, addr, pval) < 0)
			return -1;

		dbg("\r => %02X", *pval);

		if (*pval == val)
			break;
	}

	dbg("\n");

	return 0;
}

/******************************************************************************
 * @brief
 *   wait CIMaX+ register bits match a param value.
 *
 * @param   bus_adap
 *   Pointer to CIMaX+ usb adapter.
 *
 * @param   addr
 *   Addr of CIMaX+ register.
 *
 * @param   val
 *   value to match.
 *
 * @param   pval
 *   pointer to buffer to store CIMaX+ register value.
 *
 * @return
 *   0 if OK otherwise -1.
 ******************************************************************************/
int waitForBitsSet(struct device_s *bus_adap,
		unsigned int addr, unsigned char val, unsigned char *pval)
{
	dbg("=> Wait for Bits set [%02X]\n", val);
	if ((*pval & val) == val)
		return 0;

	while (1) {
		if (usb_getbyte(bus_adap, addr, pval) < 0)
			return -1;

		dbg("\r => %02X", *pval);

		if ((*pval & val) == val)
			break;
	}

	dbg("\n");

	return 0;
}

/******************************************************************************
 * @brief
 *   wait CIMaX+ register bits cleared.
 *
 * @param   bus_adap
 *   Pointer to CIMaX+ usb adapter.
 *
 * @param   addr
 *   Addr of CIMaX+ register.
 *
 * @param   val
 *   bits to check.
 *
 * @param   pval
 *   pointer to buffer to store CIMaX+ register value.
 *
 * @return
 *   0 if OK otherwise -1.
 ******************************************************************************/
int waitForBitsCleared(struct device_s *bus_adap,
		unsigned int addr, unsigned char val, unsigned char *pval)
{
	dbg("=> Wait for Bits cleared [%02X]\n", val);
	if ((*pval & val) == 0x00)
		return 0;

	while (1) {
		if (usb_getbyte(bus_adap, addr, pval) < 0)
			return -1;

		dbg("\r => %02X", *pval);

		if ((*pval & val) == 0x00)
			break;
	}

	dbg("\n");

	return 0;
}

/******************************************************************************
 * @brief
 *   retreive addr of CIMaX+ register.
 *
 * @param   dev
 *   Pointer to CIMaX+ register Name.
 *
 * @return
 *   Address of CIMaX+ register.
 *****************************************************************************/
int cimaxusb_rtr_reg_addr(char *str_addr)
{
	int i32NbItem = sizeof(cimax_reg_map) / sizeof(struct reg_s);
	int i32index;
	int i32ValAddr = -1;

	for (i32index = 0; i32index < i32NbItem; i32index++) {
		if (strcmp(str_addr, cimax_reg_map[i32index].RegisterName)
				== 0) {
			i32ValAddr = cimax_reg_map[i32index].RegAddr;
			break;
		}
	}
	return i32ValAddr;
}

static int cimaxusb_parse_cfg_ops(struct device_s *bus_adap,
		unsigned char **pptr, size_t *plen)
{
	unsigned char op;
	int ret;
	char param1[256], param2[256];
	char line[256], *ptr_line;
	int val;
	unsigned char val2;
	unsigned int  addr;
	unsigned char *ptr = *pptr;
	size_t len = *plen;

	ptr_line = line;
	op = *ptr;
	ret = sscanf(ptr, "%s %s", param1, param2);
	ptr++;
	len++;
	if (sscanf(ptr, "%s %X", param1, &val) != 2)
		return -EFAULT;
	/*pr_debug("%s: param1=%s,param2=%s, val=%x\n",
		DRIVER_NAME, param1, param2, val);*/
	strcpy(ptr_line, param1);
	len += strlen(param1);
	ptr += strlen(param1);
	strncat(ptr_line, ptr, 1);
	len++;
	ptr++;
	strcat(ptr_line, param2);
	len += strlen(param2);
	ptr += strlen(param2);
	/*pr_debug("%s: len = %08d line: %s\n",
		DRIVER_NAME, len, line);*/
	addr = cimaxusb_rtr_reg_addr(param1);
	if (((signed)addr) < 0) {
		pr_err("%s: unknown register name: %s\n",
			DRIVER_NAME, param1);
		return -ENODEV;
	}
	if (usb_getbyte(bus_adap, addr, &val2) < 0) {
		pr_err("%s: CIMaX+ register reading problem: %s\n",
		DRIVER_NAME, param1);
		return -ENODEV;
	}

	switch (op) {
	case '=':
		if (waitForValue(bus_adap, addr, val, &val2) == -1) {
			pr_err("%s : waitForValue failed on : %s\n",
				DRIVER_NAME, param1);
			return -ENODEV;
		}
		pr_info("%s : wait for value ([%x] = %x = %x)\n",
			line, addr, val, val2);
	break;
	case '?':
		if (waitForBitsSet(bus_adap, addr, val, &val2) == -1) {
			pr_err("%s : waitForBitsSet failed on : %s\n",
				DRIVER_NAME, param1);
			return -ENODEV;
		}
		pr_info("%s : wait for bitsset ([%x] = %x = %x)\n",
			line, addr, val, val2);
	break;
	case '!':
		if (waitForBitsCleared(bus_adap, addr, val, &val2)
				== -1) {
			pr_err("%s : waitForBitsCleared failed on : %s\n",
				DRIVER_NAME, param1);
			return -ENODEV;
		}
		pr_info("%s : wait for bits cleared ([%x] = %x = %x)\n",
			line, addr, val, val2);
	break;
	case '|':
		if (setLogicalOr(bus_adap, addr, val, &val2) == -1) {
			pr_err("%s : setLogicalOr failed on : %s\n",
				DRIVER_NAME, param1);
			return -ENODEV;
		}
		pr_info("%s : setOr ([%x] = %x = %x)\n",
			line, addr, val, val2);
	break;
	case '&':
		if (setLogicalAnd(bus_adap, addr, val, &val2) == -1) {
			pr_err("%s : setLogicalAnd failed on : %s\n",
				DRIVER_NAME, param1);
			return -ENODEV;
		}
		pr_info("%s : setAnd ([%x] = %x = %x)\n",
			line, addr, val, val2);
	break;
	default:
		pr_err("Error: Marker Unknown <%c> !!!\n", op);
		return -ENODEV;
	}
	*pptr = ptr;
	*plen = len;
	return 0;
}

static int cimaxusb_parse_cfg_default(struct device_s *bus_adap,
		unsigned char **pptr, size_t *plen)
{
	unsigned char *ptr = *pptr;
	size_t len = *plen;
	int ret;
	char param1[256], param2[256];
	char line[256], *ptr_line;
	int val;
	unsigned char val2;
	unsigned int  addr;

	ptr_line = line;

	ret = sscanf(ptr, "%s %s", param1, param2);
	ret = sscanf(param2, "%X", &val);
	/*pr_debug("%s: param1=%s,param2=%s,val=%x\n",
		DRIVER_NAME,param1,param2,val);*/
	strcpy(ptr_line, param1);
	len += strlen(param1);
	ptr += strlen(param1);
	strncat(ptr_line, ptr, 1);
	len++;
	ptr++;
	strcat(ptr_line, param2);
	len += strlen(param2);
	ptr += strlen(param2);
	/*pr_debug("%s: len = %08d line: %s\n",
		DRIVER_NAME, len, line);*/
	addr = cimaxusb_rtr_reg_addr(param1);
	if (((signed)addr) < 0) {
		pr_err("%s: unknown register name: %s\n",
			DRIVER_NAME, param1);
		return -ENODEV;
	}
	if (usb_getbyte(bus_adap, addr, &val2) < 0) {
		pr_err("%s: CIMaX+ register reading problem: %s\n",
			DRIVER_NAME, param1);
		return -ENODEV;
	}
	if (usb_setbyte(bus_adap, addr, val) < 0) {
		pr_err("%s: CIMaX+ register writing problem: %s\n",
			DRIVER_NAME, param1);
		return -ENODEV;
	}
	if (usb_getbyte(bus_adap, addr, &val2) < 0) {
		pr_err("%s: CIMaX+ register checking problem: %s\n",
			DRIVER_NAME, param1);
		return -ENODEV;
	}
	pr_info("set: %s(0x%04x) : 0x%02x\n", param1, addr, val);

	*pptr = ptr;
	*plen = len;
	return 0;
}

/******************************************************************************
 * @brief
 *   parse CIMaX+ config file.
 *
 * @param   dev
 *   Pointer to usb device.
 *
 * @param   fw_data
 *   Pointer to buffer with firmware data.
 *
 * @return
 *   O if no error otherwise errno.
 *****************************************************************************/
int cimaxusb_parse_cfg(struct device_s *bus_adap,
		const unsigned char *cfg_data, size_t size)
{
	char line[256], *ptr_line;
	size_t len = 0;
	unsigned char *ptr = (unsigned char *)cfg_data;
	int ret = -EFAULT;

	dbg("%s: %s size = %zd\n", DRIVER_NAME, __func__, size);
	do {
		ptr_line = line;
		switch (*ptr) {
		case ';':
			/* continue up to find \r character */
			while (*ptr != 0x0A) {
				*ptr_line++ = *ptr;
				ptr++;
				len++;
			}
			*ptr_line = 0;
			/*pr_debug("%s: len = %08d line: %s\n",
				DRIVER_NAME, len, line);*/
		break;
		case 0x0A:
		case 0x0D:
			ptr++;
			len++;
		break;
		case '=':
		case '?':
		case '!':
		case '|':
		case '&':
			ret = cimaxusb_parse_cfg_ops(bus_adap, &ptr, &len);
			if (ret < 0)
				return ret;
		break;
		default:
			ret = cimaxusb_parse_cfg_default(bus_adap, &ptr, &len);
			if (ret < 0)
				return ret;
		break;
		}
	} while (len < size);

	return (ret == -EFAULT) ? 0 : ret;
}

/******************************************************************************
 * @brief
 *   read configuration file ( CIMAX_CONFIG_NAME) and set in CIMaX+ chip.
 *
 * @param   bus_adap
 *   Pointer to usb device.
 *
 * @return
 *   None.
 ******************************************************************************/
/*
opt: 0: init, 1: off, 2: Vcc3.3 3: Vcc5
*/
int cimaxusb_configuration_setting_opt(struct device_s *bus_adap, int opt)
{
	int errno = -EFAULT;
	const struct firmware *config;
	char *cfg = cimax_config_file[opt];

	struct usb_device *dev = bus_adap->usbdev;

	dbg("request configuration file");
	/* request kernel to locate firmware file */
	errno = request_firmware(&config, cfg, &dev->dev);
	if (errno < 0) {
		pr_err("%s: unable to locate configuration file: %s\n",
			 DRIVER_NAME, cfg);
		goto error;
	}

	dbg("parse configuration file");
	errno = cimaxusb_parse_cfg(bus_adap, config->data, config->size);
	if (errno < 0) {
		pr_err("%s: unable to parse config file: %s\n",
			 DRIVER_NAME, cfg);
		goto error;
	}

error:
	/* release firmware if needed */
	if (config != NULL)
		release_firmware(config);
	return errno;
}

int cimaxusb_configuration_setting(struct device_s *bus_adap)
{
	return cimaxusb_configuration_setting_opt(bus_adap, 0);
}

int cimaxusb_configuration_setting_vcc(struct device_s *bus_adap, int vcc)
{
	return cimaxusb_configuration_setting_opt(bus_adap, vcc);
}
