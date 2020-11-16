/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
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
