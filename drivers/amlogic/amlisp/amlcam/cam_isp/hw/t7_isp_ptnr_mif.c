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

void isp_ptnr_mif_cfg_size(struct isp_dev_t *isp_dev, struct aml_format *fmt)
{
	u32 val = 0;
	u32 xsize, ysize;

	xsize = fmt->width;
	ysize = fmt->height;
/* ptnr wmif */
	val = (0 << 0) | ((xsize - 1) << 16);
	isp_reg_write(isp_dev, ISP_WMIF_PNR0_SCOPE_X, val);

	val = (0 << 0) | ((ysize - 1) << 16);
	isp_reg_write(isp_dev, ISP_WMIF_PNR0_SCOPE_Y, val);

	val = (xsize * 10 + 127) >> 7;
	isp_reg_update_bits(isp_dev, ISP_WMIF_PNR0_CTRL3, val, 0, 13);

	isp_reg_write(isp_dev, ISP_WMIF_PNR0_CTRL6, 0x0000);

/* ptnr rmif */
	val = (0 << 0) | ((xsize - 1) << 16);
	isp_reg_write(isp_dev, ISP_RMIF_PNR0_SCOPE_X, val);

	val = (0 << 0) | ((ysize - 1) << 16);
	isp_reg_write(isp_dev, ISP_RMIF_PNR0_SCOPE_Y, val);

	val = (xsize * 10 + 127) >> 7;
	isp_reg_update_bits(isp_dev, ISP_RMIF_PNR0_CTRL3, val, 0, 13);
}

void isp_ptnr_mif_cfg_buf(struct isp_dev_t *isp_dev, struct aml_buffer *buff)
{
	u32 paddr = 0;

	paddr = buff->addr[AML_PLANE_A] >> 4;

/* ptnr wmif */
	isp_reg_write(isp_dev, ISP_WMIF_PNR0_CTRL4, paddr);

/* ptnr rmif */
	isp_reg_write(isp_dev, ISP_RMIF_PNR0_CTRL4, paddr);
}

void isp_ptnr_mif_enable(struct isp_dev_t *isp_dev, u32 enable)
{
	if (enable) {
		isp_reg_update_bits(isp_dev, ISP_TOP_BED_CTRL, 1, 21,1);
		isp_reg_update_bits(isp_dev, ISP_TOP_PATH_EN, 5, 28, 3);
	} else {
		isp_reg_update_bits(isp_dev, ISP_TOP_BED_CTRL, 0, 21,1);
		isp_reg_update_bits(isp_dev, ISP_TOP_PATH_EN, 0, 28, 3);
	}
}

void isp_ptnr_mif_init(struct isp_dev_t *isp_dev)
{
/* ptnr wmif */
	isp_reg_update_bits(isp_dev, ISP_WMIF_PNR0_CTRL1, 0, 0, 3);

	isp_reg_update_bits(isp_dev, ISP_WMIF_PNR0_CTRL5, 0, 0, 3);
	isp_reg_update_bits(isp_dev, ISP_WMIF_PNR0_CTRL5, 7, 3, 4);
	isp_reg_update_bits(isp_dev, ISP_WMIF_PNR0_CTRL5, 0, 7, 1);
	isp_reg_update_bits(isp_dev, ISP_WMIF_PNR0_CTRL5, 0, 24, 1);

/* ptnr rmif */
	isp_reg_update_bits(isp_dev, ISP_RMIF_PNR0_CTRL1, 0, 0, 3);

	isp_reg_update_bits(isp_dev, ISP_RMIF_PNR0_CTRL5, 0, 0, 3);
	isp_reg_update_bits(isp_dev, ISP_RMIF_PNR0_CTRL5, 7, 3, 4);
	isp_reg_update_bits(isp_dev, ISP_RMIF_PNR0_CTRL5, 0, 16, 1);
}
