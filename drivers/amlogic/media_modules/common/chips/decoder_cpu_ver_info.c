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
* FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
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
#include <linux/amlogic/cpu_version.h>
#include "decoder_cpu_ver_info.h"

#define DECODE_CPU_VER_ID_NODE_NAME "cpu_ver_name"
#define AM_SUCESS 0
#define MAJOR_ID_START AM_MESON_CPU_MAJOR_ID_M6

static enum AM_MESON_CPU_MAJOR_ID cpu_ver_id = AM_MESON_CPU_MAJOR_ID_MAX;

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
	AM_MESON_CPU_MAJOR_ID_TL1,
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
	{},
};

static bool get_cpu_id_from_dtb(enum AM_MESON_CPU_MAJOR_ID *pidType)
{
	struct device_node *pNode = NULL;
	struct platform_device* pDev = NULL;
	const struct of_device_id *pMatch = NULL;

	pNode = of_find_node_by_name(NULL, DECODE_CPU_VER_ID_NODE_NAME);

	if (NULL == pNode)
	{
		pr_err("No find node.\n");
		return -EINVAL;
	}

	pDev =  of_find_device_by_node(pNode);
	if (NULL == pDev)
	{
		return -EINVAL;
	}

	pMatch = of_match_device(cpu_ver_of_match, &pDev->dev);

	if (NULL == pMatch)
	{
		pr_err("No find of_match_device\n");
		return -EINVAL;
	}

	*pidType = *(enum AM_MESON_CPU_MAJOR_ID *)pMatch->data;

	return AM_SUCESS;
}

static void initial_cpu_id(void)
{
	enum AM_MESON_CPU_MAJOR_ID id_type = AM_MESON_CPU_MAJOR_ID_MAX;

	if (AM_SUCESS == get_cpu_id_from_dtb(&id_type))
	{
		cpu_ver_id = id_type;
	}else
	{
		cpu_ver_id = (enum AM_MESON_CPU_MAJOR_ID)get_cpu_type();
	}

	if (AM_MESON_CPU_MAJOR_ID_G12B == cpu_ver_id)
	{
		if (is_meson_rev_b())
		cpu_ver_id = AM_MESON_CPU_MAJOR_ID_TL1;
	}
}

enum AM_MESON_CPU_MAJOR_ID get_cpu_major_id(void)
{
	if (AM_MESON_CPU_MAJOR_ID_MAX == cpu_ver_id)
	{
		initial_cpu_id();
	}

	return cpu_ver_id;
}

EXPORT_SYMBOL(get_cpu_major_id);
