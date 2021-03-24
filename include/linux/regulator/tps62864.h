/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __LINUX_REGULATOR_TPS62864_H
#define __LINUX_REGULATOR_TPS62864_H

/*
 * struct tps62864_regulator_platform_data - tps62864 regulator platform data.
 *
 * @reg_init_data: The regulator init data.
 * @vout_reg_id: The VOUT register id (0~1).
 */
struct tps62864_regulator_platform_data {
	struct regulator_init_data *reg_init_data;
	int vout_reg_id;
};

#endif /* __LINUX_REGULATOR_TPS62864_H */
