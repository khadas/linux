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
#ifndef ADEC_H
#define ADEC_H

#include "../amports/streambuf.h"
#include <linux/amlogic/media/utils/aformat.h>

extern s32 adec_init(struct stream_port_s *port);

extern s32 adec_release(enum aformat_e af);

extern s32 astream_dev_register(void);

extern s32 astream_dev_unregister(void);

#endif /* ADEC_H */
