/*
 * drivers/amlogic/media/stream_input/amports/adec.h
 *
 * Copyright (C) 2016 Amlogic, Inc. All rights reserved.
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

#ifndef ADEC_H
#define ADEC_H

#include "../parser/streambuf.h"
#include <linux/amlogic/media/utils/aformat.h>

extern s32 adec_init(struct stream_port_s *port);

extern s32 adec_release(enum aformat_e af);

extern s32 astream_dev_register(void);

extern s32 astream_dev_unregister(void);

#endif /* ADEC_H */
