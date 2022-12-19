/* SPDX-License-Identifier: GPL-2.0
*
* Copyright (C) 2020 Amlogic or its affiliates
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; version 2.
* This program is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
* or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
* for more details.
* You should have received a copy of the GNU General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*
*/

#include "aml_isp_reg.h"
#include "aml_isp_hw.h"

void isp_rdmif0_mirror_enable(struct isp_dev_t *isp_dev, u32 enable)
{
	isp_reg_update_bits(isp_dev, ISP_RMIF0_CTRL1, enable & 0x1, 4, 1);
}

void isp_rdmif0_flip_enable(struct isp_dev_t *isp_dev, u32 enable)
{
	isp_reg_update_bits(isp_dev, ISP_RMIF0_CTRL1, enable & 0x1, 5, 1);
}

void isp_rdmif0_cfg_frm_size(struct isp_dev_t *isp_dev, struct aml_format *fmt)
{
	u32 val = 0;
	u32 width, height;

	width = fmt->width;
	height = fmt->height;

	val = (width * fmt->bpp + 127) >> 7;
	isp_reg_update_bits(isp_dev, ISP_RMIF0_CTRL3, val, 0, 13);

	val = ((width - 1) << 16) | (0 << 0);
	isp_reg_write(isp_dev, ISP_RMIF0_SCOPE_X, val);

	val = ((height - 1) << 16) | (0 << 0);
	isp_reg_write(isp_dev, ISP_RMIF0_SCOPE_Y, val);

	val = fmt->bpp == 8 ? 1 :
		fmt->bpp == 16 ? 2 :
		fmt->bpp == 10 ? 7 :
		fmt->bpp == 12 ? 6 :
		fmt->bpp == 14 ? 8 : 1;
	isp_reg_update_bits(isp_dev, ISP_RMIF0_CTRL5, val, 3, 4);

	isp_reg_update_bits(isp_dev, ISP_RMIF0_CTRL5, fmt->bpp, 7, 9);

	isp_reg_update_bits(isp_dev, ISP_RMIF0_CTRL5, 0, 16, 1);
}

void isp_rdmif0_cfg_frm_buff(struct isp_dev_t *isp_dev, struct aml_buffer *buff)
{
	u32 paddr = buff->addr[AML_PLANE_A];

	isp_reg_write(isp_dev, ISP_RMIF0_CTRL4, paddr >> 4);
}

void isp_rdmif0_module_enable(struct isp_dev_t *isp_dev, u32 enable)
{
	if (enable) {
		isp_reg_update_bits(isp_dev, ISP_TOP_PATH_SEL, 2, 16, 3);
		isp_reg_update_bits(isp_dev, ISP_TOP_PATH_EN, 1, 16, 2);
	} else {
		isp_reg_update_bits(isp_dev, ISP_TOP_PATH_SEL, 1, 16, 3);
		isp_reg_update_bits(isp_dev, ISP_TOP_PATH_EN, 0, 16, 2);
	}
}

void isp_rdmif0_init(struct isp_dev_t *isp_dev)
{
	return;
}
