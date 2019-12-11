/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef GP_PLL_H
#define GP_PLL_H

#include <linux/kernel.h>
#include <linux/list.h>

#define GP_PLL_USER_FLAG_REQUEST 0x01
#define GP_PLL_USER_FLAG_GRANTED 0x02

#define GP_PLL_USER_EVENT_YIELD  0x01
#define GP_PLL_USER_EVENT_GRANT  0x02

struct gp_pll_user_handle_s {
	struct list_head list;
	const char *name;
	u32 priority;
	int (*callback)(struct gp_pll_user_handle_s *user, int event);
	u32 flag;
};

struct gp_pll_user_handle_s *gp_pll_user_register
	(const char *name,
	u32 priority,
	int (*callback)(struct gp_pll_user_handle_s *, int));

void gp_pll_user_unregister(struct gp_pll_user_handle_s *user);

void gp_pll_request(struct gp_pll_user_handle_s *user);

void gp_pll_release(struct gp_pll_user_handle_s *user);

#endif
