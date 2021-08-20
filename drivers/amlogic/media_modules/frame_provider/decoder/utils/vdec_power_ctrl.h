/*
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Description:
 */
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_device.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/pm_domain.h>
#include <linux/pm_runtime.h>
#include "vdec.h"

/* Directly controlled by reading and writing registers. */
#define PM_POWER_CTRL_RW_REG		(0)

/* Use power_ctrl_xxx family of interface controls. */
#define PM_POWER_CTRL_API		(1)

/*
 * Power Domain interface control, currently supported
 * in versions 4.19 and above.
 */
#define PM_POWER_DOMAIN			(2)

/*
 * Controlled by the secure API provided by power domain,
 * version 4.9 supports currently supported platforms (SC2).
 */
#define PM_POWER_DOMAIN_SEC_API		(3)

/*
 * Use non-secure API control through power domain, version 4.9 support,
 * currently supported platforms (SM1, TM2, TM2-revB).
 */
#define PM_POWER_DOMAIN_NONSEC_API	(4)

enum pm_pd_e {
	PD_VDEC,
	PD_HCODEC,
	PD_VDEC2,
	PD_HEVC,
	PD_HEVCB,
	PD_WAVE,
	PD_MAX
};

struct pm_pd_s {
	u8 *name;
	struct device *dev;
	struct device_link *link;
};

struct power_manager_s {
	int pm_type;
	struct pm_pd_s *pd_data;
	int (*init) (struct device *dev);
	void (*release) (struct device *dev);
	void (*power_on) (struct device *dev, int id);
	void (*power_off) (struct device *dev, int id);
	bool (*power_state) (struct device *dev, int id);
};

const char *get_pm_name(int type);

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 19, 0)
#define DL_FLAG_STATELESS		BIT(0)
#define DL_FLAG_AUTOREMOVE_CONSUMER	BIT(1)
#define DL_FLAG_PM_RUNTIME		BIT(2)
#define DL_FLAG_RPM_ACTIVE		BIT(3)
#define DL_FLAG_AUTOREMOVE_SUPPLIER	BIT(4)

struct device_link {
	u32 flags;
	/* ... */
};

static inline struct device *dev_pm_domain_attach_by_name(struct device *dev,
							  const char *name)
							  { return NULL; }
static inline struct device_link *device_link_add(struct device *consumer,
				    struct device *supplier, u32 flags)
				    { return NULL; }
static inline void device_link_del(struct device_link *link) { return; }
static inline void device_link_remove(void *consumer, struct device *supplier) { return; }
#endif

