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

#include "acamera_firmware_config.h"
#include "system_log.h"
#include "system_hw_io.h"

//debug log names for level
const char *const log_level_name[SYSTEM_LOG_LEVEL_MAX] = {"DEBUG", "INFO", "NOTICE", "WARNING", "ERR", "CRIT"};
//debug log names for modules
const char *const log_module_name[SYSTEM_LOG_MODULE_MAX] = FSM_NAMES;

static uint32_t irq_dbg = 0;

void system_irq_status( uint32_t irq_mask )
{
	irq_dbg = irq_mask;
}

static ssize_t isp_dbg_read(
    struct device *dev,
    struct device_attribute *attr,
    char *buf)
{
	ssize_t ret = 0;

	uint32_t active_size = system_hw_read_32(0x98);
	uint32_t isp_start = system_hw_read_32(0xa0);
	uint32_t frame_count = system_hw_read_32(0x70);
	uint32_t bypass_stitch = system_hw_read_32(0x18EAC) & (1 << 5);
	uint32_t ping_pong = system_hw_read_32(0x24) & 7;

	ret = sprintf(buf, "\t\tactive size: %x\t\tisp start:%x\t\tframe count:%x\t\tbypass stitch:%x\n \
		irq status:%x\t\t\tframe start:%x\t\tmulti ctx err:%x\t\t\tbroken frame:%x\n \
		isp watchdog:%x\t\t\tframe collision:%x\tdma err:%x\t\t\tpingpong:%x\n",\
		active_size, isp_start, frame_count, bypass_stitch, \
		irq_dbg, irq_dbg & (0x01),irq_dbg & (1 << 2), irq_dbg & (1 << 3),\
		irq_dbg & (1 << 19),irq_dbg & (1 << 20),irq_dbg & (1 << 22),ping_pong);

	irq_dbg = 0;

    return ret;
}

static ssize_t isp_dbg_write(
    struct device *dev, struct device_attribute *attr,
    char const *buf, size_t size)
{
    ssize_t ret = size;

    return ret;
}

static DEVICE_ATTR(isp_dbg, S_IRUGO | S_IWUSR, isp_dbg_read, isp_dbg_write);

void system_dbg_create( struct device *dev )
{
	device_create_file(dev, &dev_attr_isp_dbg);
}

void system_dbg_remove( struct device *dev )
{
	device_remove_file(dev, &dev_attr_isp_dbg);
}

