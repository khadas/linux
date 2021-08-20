/**************************************************************************//**
 * @file    cimax+usb_config.h
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

#ifndef __CIMAXPLUS_USB_CFG_H
#define __CIMAXPLUS_USB_CFG_H

/******************************************************************************
 * Include
 ******************************************************************************/
#include "cimax+usb_handle.h"

/******************************************************************************
 * Defines
 ******************************************************************************/
#define CIMAX_CONFIG_NAME   "cimax+usb.cfg"

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
 *   read configuration file ( CIMAX_CONFIG_NAME) and set in CIMaX+ chip.
 *
 * @param   bus_adap
 *   Pointer to usb device.
 *
 * @return
 *   None.
 ******************************************************************************/
int cimaxusb_configuration_setting(struct device_s *bus_adap);

#endif

