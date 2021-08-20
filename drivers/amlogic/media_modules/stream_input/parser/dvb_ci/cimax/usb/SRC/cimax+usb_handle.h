/*
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
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
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Description:
 * @file    cimax+usb_handle.h
 *
 * @brief   CIMaX+ USB Driver for linux based operating systems.
 *
 * Bruno Tonelli   <bruno.tonelli@smardtv.com>
 * & Franck Descours <franck.descours@smardtv.com>
 * for SmarDTV France, La Ciotat
 */
#ifndef __CIMAXPLUS_USB_HDLE_H
#define __CIMAXPLUS_USB_HDLE_H

#ifdef __KERNEL__

struct cimaxusb_priv_ops_t {
	int (*write_ctrl_message)(
		struct usb_device *dev, int addr, void *data, int size);

	int (*read_ctrl_message)(
		struct usb_device *dev, int addr, void *data, int size);

	int (*init_fw)(
		struct usb_device *dev);

	int (*write_ep6_message)(
		struct usb_device *dev, void *data, int size);

	int (*read_ep5_message)(
		struct usb_device *dev, void *data, int size);
};

#endif

#endif
