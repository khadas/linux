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

#ifndef SYSTEM_LOG_H_INCLUDED
#define SYSTEM_LOG_H_INCLUDED

#include <linux/sched.h>
#include <linux/kernel.h>
#include "acamera_types.h"
#include "acamera_firmware_config.h"
#include "system_timer.h"  //system_timer_timestamp
#include "system_stdlib.h" //system_memcpy
//#include "asm/string.h"
//#include <stdio.h>
#include <stdarg.h>
#include <linux/string.h>

//int printk(const char*, ...);
//defines for log level and system maximum value number for levels
#define LOG_DEBUG 0
#define LOG_INFO 1
#define LOG_NOTICE 2
#define LOG_WARNING 3
#define LOG_ERR 4
#define LOG_CRIT 5
#define LOG_NOTHING 6
#define SYSTEM_LOG_LEVEL_MAX LOG_NOTHING

//system maximum value number of modules
#define SYSTEM_LOG_MODULE_MAX LOG_MODULE_MAX

//initial values for logger log level and log module masks on real time logging, otherwise fixed
#define SYSTEM_LOG_LEVEL LOG_INFO
#define SYSTEM_LOG_MASK FW_LOG_MASK


/*define for the logger for real time changing of level and module support
value of 1 means real time logging is enabled otherwise disabled on a compile level*/
#define SYSTEM_LOG_REAL_TIME FW_LOG_REAL_TIME

/*define for the logger to include src information like filenames and line number
value of 1 means filename, function and line number is logged otherwise disabled on a compile level*/
#define SYSTEM_LOG_HAS_SRC FW_LOG_HAS_SRC

/*define for the logger to enable logging from ISR
value of 1 means ISR logging is enabled otherwise disabled on the compile level*/
#define SYSTEM_LOG_FROM_ISR FW_LOG_FROM_ISR

//printf like functions used by the logger to log output
#define SYSTEM_VPRINTF vprintk
#define SYSTEM_PRINTF printk
#define SYSTEM_SNPRINTF snprintf
#define SYSTEM_VSPRINTF vsprintf

/*define for the logger to include time on the output log
value of 1 means timestamp is logged otherwise disabled on the compile level*/
#define SYSTEM_LOG_HAS_TIME FW_LOG_HAS_TIME

//debug log names for level and modules
extern const char *const log_level_name[SYSTEM_LOG_LEVEL_MAX];
extern const char *const log_module_name[SYSTEM_LOG_MODULE_MAX];


#endif // LOG_H_INCLUDED
