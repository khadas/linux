/*
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Description:
 */
#ifndef _AMCI_H
#define _AMCI_H

/* #include <asm/types.h> */
#include <linux/types.h>


enum AM_CI_IO_MODE {
	AM_CI_IOR = 0,
	AM_CI_IOW,
	AM_CI_MEMR,
	AM_CI_MEMW
};

struct ci_rw_param {
	enum AM_CI_IO_MODE mode;
	int addr;
	u_int8_t value;
};


#define AMCI_IOC_MAGIC  'D'

#define AMCI_IOC_RESET       _IO(AMCI_IOC_MAGIC, 0x00)
#define AMCI_IOC_IO          _IOWR(AMCI_IOC_MAGIC, 0x01, struct ci_rw_param)
#define AMCI_IOC_GET_DETECT  _IOWR(AMCI_IOC_MAGIC, 0x02, int)
#define AMCI_IOC_SET_POWER   _IOW(AMCI_IOC_MAGIC, 0x03, int)


#endif