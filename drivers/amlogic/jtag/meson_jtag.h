/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __AML_JTAG_H
#define __AML_JTAG_H

#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/notifier.h>

#define CLUSTER_BIT		2
#define	CLUSTER_DISABLE		(-1)
#define MAX_VALID_PARAM_NUM	3
#define MAX_PARAM_NUM		(MAX_VALID_PARAM_NUM + 1)

#define MAX_PARAM_LENGTH	32

struct aml_jtag_dev {
	struct platform_device *pdev;
	struct pinctrl *jtag_pinctrl;
	struct class cls;
	struct device dev;

#ifdef CONFIG_MACH_MESON8B
	void __iomem *base;
#endif

	struct notifier_block notifier;

	unsigned int old_select;
	unsigned int select;
	unsigned int cluster;
	unsigned int old_cluster;

	const unsigned int *ao_gpios;
	const unsigned int *ee_gpios;

	int ao_ngpios;
	int ee_ngpios;

};

struct jtag_id_desc {
	unsigned short id;
	const char *core_type;
	const char *jtag_type;
	const char *alias;
};

/* jtag type */
#define JTAG_A		0
#define JTAG_B		1
#define SWD_A		2

/* core type */
#define	AP		1 /* a35 a53 a72 ... */
#define SCP		2 /* m3 */
#define SP		3 /* m4 */
#define DSPA		4 /* dsp-a */
#define	DSPB		5 /* dsp-b */

/*
 * bitmap for jtag select ID:
 *
 *	|---|---|---|---|---|---|---|------|------|
 *	| 9 | 8 | 6 | 5 | 4 | 3 | 2 |   1  |   0  |
 *	|---|---|---|---|---|---|---|------|------|
 *	|   core type   | jtag type |cluster type |
 *	|---------------|-----------|-------------|
 */
#define CORE_TYPE_SHIFT		5
#define JTAG_TYPE_SHIFT		2
#define CLUSTER_TYPE_SHIFT	0

#define CORE_TYPE_MASK		0xf
#define JTAG_TYPE_MASK		0x7
#define CLUSTER_TYPE_MASK	0x3

#define JTAG_SELECT_ID(core_type, jtag_type, cluster_type) \
	((((core_type) & CORE_TYPE_MASK) << CORE_TYPE_SHIFT) |\
	(((jtag_type) & JTAG_TYPE_MASK) << JTAG_TYPE_SHIFT) |\
	(((cluster_type) & CLUSTER_TYPE_MASK) << CLUSTER_TYPE_SHIFT))

#define CLUSTER_TYPE_UPDATE(id, cluster_type) \
	(((id) & (~(CLUSTER_TYPE_MASK))) | (cluster_type))

#define CORE_TYPE_GET(id) \
	(((id) >> CORE_TYPE_SHIFT) & CORE_TYPE_MASK)

#define JTAG_TYPE_GET(id) \
	(((id) >> JTAG_TYPE_SHIFT) & JTAG_TYPE_MASK)

#define CLUSTER_TYPE_GET(id) \
	(((id) >> CLUSTER_TYPE_SHIFT) & CLUSTER_TYPE_MASK)

#endif
