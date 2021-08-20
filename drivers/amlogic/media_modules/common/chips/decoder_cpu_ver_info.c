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
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/amlogic/media/registers/cpu_version.h>
#include "decoder_cpu_ver_info.h"

#define DECODE_CPU_VER_ID_NODE_NAME "cpu_ver_name"
#define AM_SUCESS 0
#define MAJOR_ID_START AM_MESON_CPU_MAJOR_ID_M6

static enum AM_MESON_CPU_MAJOR_ID cpu_ver_id = AM_MESON_CPU_MAJOR_ID_MAX;
static int cpu_sub_id = 0;

static enum AM_MESON_CPU_MAJOR_ID cpu_ver_info[AM_MESON_CPU_MAJOR_ID_MAX - MAJOR_ID_START]=
{
	AM_MESON_CPU_MAJOR_ID_M6,
	AM_MESON_CPU_MAJOR_ID_M6TV,
	AM_MESON_CPU_MAJOR_ID_M6TVL,
	AM_MESON_CPU_MAJOR_ID_M8,
	AM_MESON_CPU_MAJOR_ID_MTVD,
	AM_MESON_CPU_MAJOR_ID_M8B,
	AM_MESON_CPU_MAJOR_ID_MG9TV,
	AM_MESON_CPU_MAJOR_ID_M8M2,
	AM_MESON_CPU_MAJOR_ID_UNUSE,
	AM_MESON_CPU_MAJOR_ID_GXBB,
	AM_MESON_CPU_MAJOR_ID_GXTVBB,
	AM_MESON_CPU_MAJOR_ID_GXL,
	AM_MESON_CPU_MAJOR_ID_GXM,
	AM_MESON_CPU_MAJOR_ID_TXL,
	AM_MESON_CPU_MAJOR_ID_TXLX,
	AM_MESON_CPU_MAJOR_ID_AXG,
	AM_MESON_CPU_MAJOR_ID_GXLX,
	AM_MESON_CPU_MAJOR_ID_TXHD,
	AM_MESON_CPU_MAJOR_ID_G12A,
	AM_MESON_CPU_MAJOR_ID_G12B,
	AM_MESON_CPU_MAJOR_ID_GXLX2,
	AM_MESON_CPU_MAJOR_ID_SM1,
	AM_MESON_CPU_MAJOR_ID_A1,
	AM_MESON_CPU_MAJOR_ID_RES_0x2d,
	AM_MESON_CPU_MAJOR_ID_TL1,
	AM_MESON_CPU_MAJOR_ID_TM2,
	AM_MESON_CPU_MAJOR_ID_C1,
	AM_MESON_CPU_MAJOR_ID_RES_0x31,
	AM_MESON_CPU_MAJOR_ID_SC2,
	AM_MESON_CPU_MAJOR_ID_C2,
	AM_MESON_CPU_MAJOR_ID_T5,
	AM_MESON_CPU_MAJOR_ID_T5D,
	AM_MESON_CPU_MAJOR_ID_T7,
	AM_MESON_CPU_MAJOR_ID_S4,
	AM_MESON_CPU_MAJOR_ID_T3,
	AM_MESON_CPU_MAJOR_ID_P1,
	AM_MESON_CPU_MAJOR_ID_S4D,
	AM_MESON_CPU_MAJOR_ID_T5W,
};

static const struct of_device_id cpu_ver_of_match[] = {
	{
		.compatible = "amlogic, cpu-major-id-axg",
		.data = &cpu_ver_info[AM_MESON_CPU_MAJOR_ID_AXG - MAJOR_ID_START],
	},

	{
		.compatible = "amlogic, cpu-major-id-g12a",
		.data = &cpu_ver_info[AM_MESON_CPU_MAJOR_ID_G12A - MAJOR_ID_START],
	},

	{
		.compatible = "amlogic, cpu-major-id-gxl",
		.data = &cpu_ver_info[AM_MESON_CPU_MAJOR_ID_GXL - MAJOR_ID_START],
	},

	{
		.compatible = "amlogic, cpu-major-id-gxm",
		.data = &cpu_ver_info[AM_MESON_CPU_MAJOR_ID_GXM - MAJOR_ID_START],
	},

	{
		.compatible = "amlogic, cpu-major-id-txl",
		.data = &cpu_ver_info[AM_MESON_CPU_MAJOR_ID_TXL - MAJOR_ID_START],
	},

	{
		.compatible = "amlogic, cpu-major-id-txlx",
		.data = &cpu_ver_info[AM_MESON_CPU_MAJOR_ID_TXLX - MAJOR_ID_START],
	},

	{
		.compatible = "amlogic, cpu-major-id-sm1",
		.data = &cpu_ver_info[AM_MESON_CPU_MAJOR_ID_SM1 - MAJOR_ID_START],
	},

	{
		.compatible = "amlogic, cpu-major-id-tl1",
		.data = &cpu_ver_info[AM_MESON_CPU_MAJOR_ID_TL1 - MAJOR_ID_START],
	},
	{
		.compatible = "amlogic, cpu-major-id-tm2",
		.data = &cpu_ver_info[AM_MESON_CPU_MAJOR_ID_TM2 - MAJOR_ID_START],
	},
	{
		.compatible = "amlogic, cpu-major-id-sc2",
		.data = &cpu_ver_info[AM_MESON_CPU_MAJOR_ID_SC2 - MAJOR_ID_START],
	},
	{
		.compatible = "amlogic, cpu-major-id-t5",
		.data = &cpu_ver_info[AM_MESON_CPU_MAJOR_ID_T5 - MAJOR_ID_START],
	},
	{
		.compatible = "amlogic, cpu-major-id-t5d",
		.data = &cpu_ver_info[AM_MESON_CPU_MAJOR_ID_T5D - MAJOR_ID_START],
	},
	{
		.compatible = "amlogic, cpu-major-id-t7",
		.data = &cpu_ver_info[AM_MESON_CPU_MAJOR_ID_T7 - MAJOR_ID_START],
	},
	{
		.compatible = "amlogic, cpu-major-id-s4",
		.data = &cpu_ver_info[AM_MESON_CPU_MAJOR_ID_S4 - MAJOR_ID_START],
	},
	{
		.compatible = "amlogic, cpu-major-id-t3",
		.data = &cpu_ver_info[AM_MESON_CPU_MAJOR_ID_T3 - MAJOR_ID_START],
	},
	{
		.compatible = "amlogic, cpu-major-id-p1",
		.data = &cpu_ver_info[AM_MESON_CPU_MAJOR_ID_P1 - MAJOR_ID_START],
	},
	{
		.compatible = "amlogic, cpu-major-id-s4d",
		.data = &cpu_ver_info[AM_MESON_CPU_MAJOR_ID_S4D - MAJOR_ID_START],
	},
	{
		.compatible = "amlogic, cpu-major-id-t5w",
		.data = &cpu_ver_info[AM_MESON_CPU_MAJOR_ID_T5W - MAJOR_ID_START],
	},
	{},
};

static const int cpu_sub_info[] = {
		AM_MESON_CPU_MINOR_ID_REVB_G12B,
		AM_MESON_CPU_MINOR_ID_REVB_TM2,
		AM_MESON_CPU_MINOR_ID_S4_S805X2,
};

static const struct of_device_id cpu_sub_id_of_match[] = {
	{
		.compatible = "amlogic, cpu-major-id-g12b-b",
		.data = &cpu_sub_info[0],
	},
	{
		.compatible = "amlogic, cpu-major-id-tm2-b",
		.data = &cpu_sub_info[1],
	},
	{
		.compatible = "amlogic, cpu-major-id-s4-805x2",
		.data = &cpu_sub_info[2],
	},
};

static bool get_cpu_id_from_dtb(enum AM_MESON_CPU_MAJOR_ID *pid_type, int *sub_id)
{
	struct device_node *pnode = NULL;
	struct platform_device *pdev = NULL;
	const struct of_device_id *pmatch = NULL;

	pnode = of_find_node_by_name(NULL, DECODE_CPU_VER_ID_NODE_NAME);
	if (NULL == pnode) {
		pr_err("No find node.\n");
		return -EINVAL;
	}

	pdev =  of_find_device_by_node(pnode);
	if (NULL == pdev)
		return -EINVAL;

	pmatch = of_match_device(cpu_ver_of_match, &pdev->dev);
	if (NULL == pmatch) {
		pmatch = of_match_device(cpu_sub_id_of_match, &pdev->dev);
		if (NULL == pmatch) {
			pr_err("No find of_match_device\n");
			return -EINVAL;
		}
	}

	*pid_type = (enum AM_MESON_CPU_MAJOR_ID)(*(int *)pmatch->data) & (MAJOY_ID_MASK);

	*sub_id = ((*(int *)pmatch->data) & (SUB_ID_MASK)) >> 8;

	return AM_SUCESS;
}

static void initial_cpu_id(void)
{
	enum AM_MESON_CPU_MAJOR_ID id_type = AM_MESON_CPU_MAJOR_ID_MAX;
	int sub_id = 0;

	if (AM_SUCESS == get_cpu_id_from_dtb(&id_type, &sub_id)) {
		cpu_ver_id = id_type;
		cpu_sub_id = sub_id;
	} else {
		cpu_ver_id = (enum AM_MESON_CPU_MAJOR_ID)get_cpu_type();
		cpu_sub_id = (is_meson_rev_b()) ? CHIP_REVB : CHIP_REVA;
	}

	if ((AM_MESON_CPU_MAJOR_ID_G12B == cpu_ver_id) && (CHIP_REVB == cpu_sub_id))
		cpu_ver_id = AM_MESON_CPU_MAJOR_ID_TL1;

	pr_info("vdec init cpu id: 0x%x(%d)", cpu_ver_id, cpu_sub_id);
}

enum AM_MESON_CPU_MAJOR_ID get_cpu_major_id(void)
{
	if (AM_MESON_CPU_MAJOR_ID_MAX == cpu_ver_id)
		initial_cpu_id();

	return cpu_ver_id;
}
EXPORT_SYMBOL(get_cpu_major_id);

int get_cpu_sub_id(void)
{
	return cpu_sub_id;
}
EXPORT_SYMBOL(get_cpu_sub_id);

bool is_cpu_meson_revb(void)
{
	if (AM_MESON_CPU_MAJOR_ID_MAX == cpu_ver_id)
		initial_cpu_id();

	return (cpu_sub_id == CHIP_REVB);
}
EXPORT_SYMBOL(is_cpu_meson_revb);

bool is_cpu_tm2_revb(void)
{
	return ((get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_TM2)
		&& (is_cpu_meson_revb()));
}
EXPORT_SYMBOL(is_cpu_tm2_revb);

bool is_cpu_s4_s805x2(void)
{
	return ((get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_S4)
		&& (get_cpu_sub_id() == CHIP_REVX));
}
EXPORT_SYMBOL(is_cpu_s4_s805x2);

