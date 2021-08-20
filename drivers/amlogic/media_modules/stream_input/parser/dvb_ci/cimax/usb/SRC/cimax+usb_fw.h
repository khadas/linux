/**************************************************************************//**
 * @file    cimax+usb_fw.h
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
 *
 ******************************************************************************/

#ifndef __CIMAXPLUS_USB_FW_H
#define __CIMAXPLUS_USB_FW_H

/******************************************************************************
 * Include
 ******************************************************************************/
#include "cimax+usb_handle.h"

/******************************************************************************
 * Defines
 ******************************************************************************/
#define CIMAX_FIRMWARE_NAME   "cimax+_usbdvb.bin"
#define START_FW_ADDR	      0x8000
#define STOP_FW_ADDR	         0xCFF9
#define START_INTVECT_ADDR	   0xFFFA
#define MAX_FW_PKT_SIZE	      256

/******************************************************************************
 * Enums
 ******************************************************************************/
/******************************************************************************
 * Structures
 ******************************************************************************/

/******************************************************************************
 * Functions
 ******************************************************************************/
/******************************************************************************
 * @brief
 *   upload firmware in CIMaX+ chip.
 *
 * @param   bus_adap
 *   Pointer to usb device.
 *
 * @return
 *   None.
 ******************************************************************************/
int cimaxusb_fw_upload(struct device_s *bus_adap);

#endif
