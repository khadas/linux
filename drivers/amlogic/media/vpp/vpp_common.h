/* SPDX-License-Identifier: (GPL-2.0+ OR MIT)*/
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __VPP_COMMON_H__
#define __VPP_COMMON_H__

#define PR_SYS  BIT(0)
#define PR_IOC  BIT(1)
#define PR_DEBUG  BIT(2)

extern unsigned int pr_lvl;
#define pr_vpp(level, fmt, args ...)\
	do {\
		if ((level) & pr_lvl)\
			pr_info("vpp: " fmt, ##args);\
	} while (0)

#define PR_ERR(fmt, args...)  pr_info("vpp_err: " fmt, ##args)
#define PR_DRV(fmt, args...)  pr_info("pr_drv: " fmt, ##args)

#define _VPP_TYPE  'V'

struct vpp_pq_state_s {
	int pq_en;
};

#define VPP_IOC_GET_PQ_STATE    _IOR(_VPP_TYPE, 0x1, struct vpp_pq_state_s)

#endif

