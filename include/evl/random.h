/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2023 Lionel Minne  <lionel.minne@exail.com>
 * Copyright (C) 2023 Philippe Gerum  <rpm@xenomai.org>
 *
 * A fast and efficient random number generator usable from the
 * out-of-band stage.
 */

#ifndef _EVL_RANDOM_H
#define _EVL_RANDOM_H

#include <linux/types.h>

void evl_init_rng(void);

u8 evl_read_rng_u8(void);

u16 evl_read_rng_u16(void);

u32 evl_read_rng_u32(void);

#endif /* !_EVL_RANDOM_H */
