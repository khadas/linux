/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __MESON_MHU_USER_H__
#define __MESON_MHU_USER_H__

#include <linux/cdev.h>
#include <linux/mailbox_controller.h>
#include <linux/amlogic/meson_mhu_common.h>

#define MBOX_USER_SIZE		0x80

int __init aml_mhu_user_init(void);
void __exit aml_mhu_user_exit(void);
#endif
