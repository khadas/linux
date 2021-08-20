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
#ifndef PTS_SERVER_DEV_HEAD_HH
#define PTS_SERVER_DEV_HEAD_HH

#define PTSSERVER_IOC_MAGIC 'P'
#define PTSSERVER_IOC_INSTANCE_ALLOC _IOW(PTSSERVER_IOC_MAGIC, 0x01, int)
#define PTSSERVER_IOC_INSTANCE_GET   _IOW(PTSSERVER_IOC_MAGIC, 0x02, int)
#define PTSSERVER_IOC_CHECKIN_PTS   _IOW(PTSSERVER_IOC_MAGIC, 0x03, int)
#define PTSSERVER_IOC_CHECKOUT_PTS   _IOW(PTSSERVER_IOC_MAGIC, 0x04, int)
#define PTSSERVER_IOC_PEEK_PTS   _IOW(PTSSERVER_IOC_MAGIC, 0x05, int)
#define PTSSERVER_IOC_SET_ALIGNMENT_OFFSET   _IOW(PTSSERVER_IOC_MAGIC, 0x06, int)
#define PTSSERVER_IOC_GET_LAST_CHECKIN_PTS   _IOW(PTSSERVER_IOC_MAGIC, 0x07, int)
#define PTSSERVER_IOC_GET_LAST_CHECKOUT_PTS   _IOW(PTSSERVER_IOC_MAGIC, 0x08, int)
#define PTSSERVER_IOC_RELEASE   _IOW(PTSSERVER_IOC_MAGIC, 0x09, int)

#endif

