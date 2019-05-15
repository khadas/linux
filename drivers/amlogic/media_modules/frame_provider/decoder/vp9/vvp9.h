/*
 * drivers/amlogic/amports/vvp9.h
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

#ifndef VVP9_H
#define VVP9_H

void adapt_coef_probs(int pic_count, int prev_kf, int cur_kf, int pre_fc,
unsigned int *prev_prob, unsigned int *cur_prob, unsigned int *count);
#endif
