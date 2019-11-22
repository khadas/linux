/*
 * include/linux/amlogic/media/sound/iomapres.h
 *
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
 */

#ifndef __IOMAPRES_H__
#define __IOMAPRES_H__

#include <linux/regmap.h>

struct regmap *regmap_resource(struct device *dev, char *name);

unsigned int mmio_read(struct regmap *map, unsigned int reg_ofs);

int mmio_write(struct regmap *map, unsigned int reg_ofs, unsigned int value);

int mmio_update_bits(struct regmap *map,
		     unsigned int reg_ofs,
		     unsigned int mask,
		     unsigned int value);

#endif
