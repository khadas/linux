/*
 * include/linux/amlogic/aml_serial.h
 *
 * Copyright (C) 2015 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
*/

#ifndef _AML_SERIAL_H
#define _AML_SERIAL_H

#include <linux/bitops.h>

/* Based on AO_RTI_STATUS0 , 0xC8100000 */
#define MESON_AO_UART0_WFIFO			(0x4C0)
#define MESON_AO_UART0_STATUS			(0x4CC)


#define MESON_UART_TX_FULL			BIT(21)

#endif
