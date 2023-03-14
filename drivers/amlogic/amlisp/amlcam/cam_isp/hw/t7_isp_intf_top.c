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

void isp_intf_top_cfg_size(struct isp_dev_t *isp_dev, struct aml_format *fmt)
{
	u32 val = 0;
	u32 xsize, ysize;

	xsize = fmt->width;
	ysize = fmt->height;

	val = (1 << 16) | (1 << 0);
	isp_reg_write(isp_dev, ISP_INTF_LOSSE_CTRL, val);

	val = (xsize << 16) | (ysize << 0);
	isp_reg_write(isp_dev, ISP_INTF_LOSSE0_SIZE, val);

	val = (((xsize + 1) >> 1) << 16) | (((ysize + 1) >> 1) << 0);
	isp_reg_write(isp_dev, ISP_INTF_LOSSE1_SIZE, val);


	val = (xsize << 16) | (ysize << 0);
	isp_reg_write(isp_dev, ISP_INTF_LOSSD0_SIZE, val);

	val = (((xsize + 1) >> 1) << 16) | (((ysize + 1) >> 1) << 0);
	isp_reg_write(isp_dev, ISP_INTF_LOSSD1_SIZE, val);
}

void isp_intf_top_cfg_buf(struct isp_dev_t *isp_dev, struct aml_format *fmt, struct aml_buffer *buff)
{
	u32 i = 0;
	u32 iir_link_addr = 0;
	u32 iir_slice_addr = 0;
	u32 iir_link_size = 0;
	u32 iir_slice_size = 0;
	u32 mix_link_addr = 0;
	u32 mix_slice_addr = 0;
	u32 mix_link_size = 0;
	u32 mix_slice_size = 0;
	u32 iir_body_size = 0;
	u32 iir_body_page = 0;
	u32 mix_body_size = 0;
	u32 mix_body_page = 0;
	u32 iir_body_addr = 0;
	u32 mix_body_addr = 0;
	void *iir_link_vaddr = NULL;
	void * mix_link_vaddr = NULL;
	u32 *alink_vaddr = NULL;
	u32 *blink_vaddr = NULL;
	struct isp_global_info *g_info = isp_global_get_info();

	iir_body_size = (fmt->width * isp_dev->tnr_bits + fmt->width / 10) * fmt->height / 8;
	iir_body_size = ISP_ALIGN(iir_body_size, 1 << 12);
	iir_body_page = iir_body_size >> 12;

	mix_body_size = (fmt->width * isp_dev->tnr_bits * 12 / 16 + fmt->width / 10) * fmt->height / 4 / 8;
	mix_body_size = ISP_ALIGN(mix_body_size, 1 << 12);
	mix_body_page = mix_body_size >> 12;

	iir_slice_addr = buff->addr[AML_PLANE_B];
	iir_slice_size = 16 * 1024;
	iir_slice_size = ISP_ALIGN(iir_slice_size, 1 << 12);

	mix_slice_addr = iir_slice_addr + iir_slice_size * 2;
	mix_slice_size = 16 * 1024;
	mix_slice_size = ISP_ALIGN(mix_slice_size, 1 << 12);

	iir_link_addr = mix_slice_addr + mix_slice_size * 2;
	iir_link_size = iir_body_page * sizeof(u32);
	iir_link_size = ISP_ALIGN(iir_link_size, 1 << 12);
	iir_link_vaddr = buff->vaddr[AML_PLANE_B] + iir_slice_size * 2 + mix_slice_size * 2;

	mix_link_addr = iir_link_addr + iir_link_size * 2;
	mix_link_size = mix_body_page * sizeof(u32);
	mix_link_size = ISP_ALIGN(mix_link_size, 1 << 12);
	mix_link_vaddr = iir_link_vaddr + iir_link_size * 2;

	iir_body_addr = mix_link_addr + mix_link_size * 2;

	mix_body_addr = iir_body_addr + iir_body_size * 2;

	isp_reg_write(isp_dev, ISP_INTF_LOSS_LOOP0_BADDR00, iir_link_addr);
	isp_reg_write(isp_dev, ISP_INTF_LOSS_LOOP0_BADDR01, iir_link_addr + iir_link_size);
	isp_reg_write(isp_dev, ISP_INTF_LOSS_LOOP0_BADDR02, iir_link_addr);
	isp_reg_write(isp_dev, ISP_INTF_LOSS_LOOP0_BADDR03, iir_link_addr);

	isp_reg_write(isp_dev, ISP_INTF_LOSS_LOOP0_BADDR10, iir_slice_addr);
	isp_reg_write(isp_dev, ISP_INTF_LOSS_LOOP0_BADDR11, iir_slice_addr + iir_slice_size);
	isp_reg_write(isp_dev, ISP_INTF_LOSS_LOOP0_BADDR12, iir_slice_addr);
	isp_reg_write(isp_dev, ISP_INTF_LOSS_LOOP0_BADDR13, iir_slice_addr);

	isp_reg_write(isp_dev, ISP_INTF_LOSS_LOOP1_BADDR00, mix_link_addr);
	isp_reg_write(isp_dev, ISP_INTF_LOSS_LOOP1_BADDR01, mix_link_addr + mix_link_size);
	isp_reg_write(isp_dev, ISP_INTF_LOSS_LOOP1_BADDR02, mix_link_addr);
	isp_reg_write(isp_dev, ISP_INTF_LOSS_LOOP1_BADDR03, mix_link_addr);

	isp_reg_write(isp_dev, ISP_INTF_LOSS_LOOP1_BADDR10, mix_slice_addr);
	isp_reg_write(isp_dev, ISP_INTF_LOSS_LOOP1_BADDR11, mix_slice_addr + mix_slice_size);
	isp_reg_write(isp_dev, ISP_INTF_LOSS_LOOP1_BADDR12, mix_slice_addr);
	isp_reg_write(isp_dev, ISP_INTF_LOSS_LOOP1_BADDR13, mix_slice_addr);

	alink_vaddr = iir_link_vaddr;
	blink_vaddr = iir_link_vaddr + iir_link_size;
	for (i = 0; i < iir_body_page; i++) {
		*(alink_vaddr + i) = iir_body_addr + (i << 12);
		*(blink_vaddr + i) = iir_body_addr + iir_body_size + (i << 12);
	}

	alink_vaddr = mix_link_vaddr;
	blink_vaddr = mix_link_vaddr + mix_link_size;
	for (i = 0; i < mix_body_page; i++) {
		*(alink_vaddr + i) = mix_body_addr + (i << 12);
		*(blink_vaddr + i) = mix_body_addr + mix_body_size + (i << 12);
	}

	if (g_info->mode == AML_ISP_SCAM) {
		isp_hwreg_update_bits(isp_dev, ISP_INTF_TOP_CTRL, 1, 0, 1);
		isp_hwreg_update_bits(isp_dev, ISP_MCNR_HW_CTRL0, 1, 31, 1);
		isp_hwreg_update_bits(isp_dev, ISP_INTF_TOP_CTRL, 0, 0, 1);
		isp_hwreg_update_bits(isp_dev, ISP_MCNR_HW_CTRL0, 0, 31, 1);
	}
}

void isp_intf_top_loss_index(struct isp_dev_t *isp_dev)
{
	struct isp_global_info *g_info = isp_global_get_info();

	if (g_info->mode == AML_ISP_SCAM)
		return;

	isp_reg_update_bits(isp_dev, ISP_INTF_LOSS_LOOP0_SW, isp_dev->frm_cnt % 2, 8, 4);
	isp_reg_update_bits(isp_dev, ISP_INTF_LOSS_LOOP0_SW, (isp_dev->frm_cnt + 1) % 2, 12, 4);
	isp_reg_update_bits(isp_dev, ISP_INTF_LOSS_LOOP1_SW, isp_dev->frm_cnt % 2, 8, 4);
	isp_reg_update_bits(isp_dev, ISP_INTF_LOSS_LOOP1_SW, (isp_dev->frm_cnt + 1)% 2, 12, 4);
}

void isp_intf_top_init(struct isp_dev_t *isp_dev)
{
	struct isp_global_info *g_info = isp_global_get_info();
	u32 val = 0;

	isp_reg_update_bits(isp_dev, ISP_INTF_CHECKSUM_CTRL, 1, 0, 1);
	isp_reg_update_bits(isp_dev, ISP_INTF_CHECKSUM_CTRL, 2, 4, 4);

	val = (2 << 4) | (1 << 1) | (1 << 0);
	isp_reg_write(isp_dev, ISP_INTF_LOSS_LOOP0_CTRL, val);

	val = (2 << 4) | (1 << 1) | (1 << 0);
	isp_reg_write(isp_dev, ISP_INTF_LOSS_LOOP1_CTRL, val);

	isp_reg_update_bits(isp_dev, ISP_INTF_LOSSD_CTRL, 1, 9, 1);
	isp_reg_update_bits(isp_dev, ISP_INTF_LOSSD_CTRL, 1, 0, 9);

	isp_reg_update_bits(isp_dev, ISP_INTF_LOSSD_CTRL, 1, 25, 1);
	isp_reg_update_bits(isp_dev, ISP_INTF_LOSSD_CTRL, 1, 16, 9);

	if (g_info->mode != AML_ISP_SCAM) {
		isp_reg_update_bits(isp_dev, ISP_INTF_LOSS_LOOP0_SW, 3, 0, 2);
		isp_reg_update_bits(isp_dev, ISP_INTF_LOSS_LOOP1_SW, 3, 0, 2);
	}
}
