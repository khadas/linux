/*
*
* SPDX-License-Identifier: GPL-2.0
*
* Copyright (C) 2011-2018 ARM or its affiliates
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; version 2.
* This program is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
* or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
* for more details.
* You should have received a copy of the GNU General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*
*/

#if !defined( __ACAMERA_CONTROL_CONFIG_H__ )
#define __ACAMERA_CONTROL_CONFIG_H__

#include "acamera_firmware_config.h"

//#if ISP_HAS_CONNECTION_SOCKET
/* please call these routines from the platform specific code to setup socket routines */
//struct acamera_socket_f *acamera_socket_f_impl(void);
//void acamera_socket_set_f_impl(struct acamera_socket_f *f);
//#endif // ISP_HAS_CONNECTION_SOCKET
#define ISP_HAS_STREAM_CONNECTION ( ISP_HAS_CONNECTION_SOCKET || ISP_HAS_CONNECTION_BUFFER || ISP_HAS_CONNECTION_UART || ISP_HAS_CONNECTION_CHARDEV )

#endif
