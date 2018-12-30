/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __IR_SYSFS_MESON_H__
#define __IR_SYSFS_MESON_H__
int meson_ir_sysfs_init(struct meson_ir_chip *rc);
void meson_ir_sysfs_exit(struct meson_ir_chip *chip);
#endif
