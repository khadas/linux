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

#include <linux/delay.h>

void isp_apb_dma_check_done(struct isp_dev_t *isp_dev)
{
	u32 val = 0;
	u32 failed = 0;

	while (isp_dev->isp_status == STATUS_START) {
		val = isp_hwreg_read(isp_dev, ISP_DMA_PENDING0);
		if (val & 0x1)
			break;

		udelay(300);
		if (failed ++ > 1000) {
			pr_err("isp apb dma timeout\n");
			break;
		}
	}

	isp_hwreg_write(isp_dev, ISP_DMA_PENDING0, val);
}

void isp_apb_dma_start(struct isp_dev_t *isp_dev)
{
	u32 val = 0;

/* just use ping to write */
	val = (0 << 3)  | (1 << 2) | (0 << 1) | (1 << 0);
	isp_hwreg_write(isp_dev, ISP_DMA_SRC0_CTL, val);

/* select task0 type and cmd length */
	val = (1 << 31) | (1 << 30) | (isp_dev->twreg_cnt * 2 -1);
	isp_hwreg_write(isp_dev, ISP_DMA_SRC0_PING_TASK0, val);

/* config ping store addr */
	val = isp_dev->wreg_buff.addr[AML_PLANE_A];
	isp_hwreg_write(isp_dev, ISP_DMA_SRC0_PING_CMD_ADDR0, val);

/* apb dma enable */
	val = (1 << 31);
	isp_hwreg_write(isp_dev, ISP_DMA_CTL0, val);
}

void isp_apb_dma_stop(struct isp_dev_t *isp_dev)
{
	isp_hwreg_write(isp_dev, ISP_DMA_CTL0, 0);
}

void isp_apb_dma_manual_trigger(struct isp_dev_t *isp_dev)
{
	isp_hwreg_update_bits(isp_dev, ISP_TOP_RDMA_CTRL, 1, 0, 1);

	isp_hwreg_update_bits(isp_dev, ISP_TOP_RDMA_CTRL, 1, 16, 1);
}

void isp_apb_dma_fill_rreg_buff(struct isp_dev_t *isp_dev)
{
	u32 i = 0;
	u32 base_reg = (ISP_TOP_INPUT_SIZE - ISP_BASE) >> 2;
	u32 max_reg = (ISP_LOSSD_MIX_RO_BIT_LEN_L_3 - ISP_BASE) >> 2;
	struct isp_global_info *g_info = isp_global_get_info();
	struct aml_reg *g_rreg = g_info->rreg_buff.vaddr[AML_PLANE_A];
	struct aml_reg *rreg = isp_dev->rreg_buff.vaddr[AML_PLANE_A];

	for (i = base_reg; i <= max_reg; i++) {
		rreg[i].addr = i;
		rreg[i].val = g_rreg[i].val;
	}
}

void isp_apb_dma_auto_trigger(struct isp_dev_t *isp_dev)
{
	u32 val =0;

	val = (1 << 0) | (1 << 16);
	isp_hwreg_write(isp_dev, ISP_TOP_RDMA_CTRL, val);

	val = (0 << 0);
	isp_hwreg_write(isp_dev, ISP_TOP_RDMA_CTRL, val);
}

void isp_apb_dma_fill_gisp_rreg_buff(struct isp_global_info *g_isp)
{
	u32 i = 0;
	u32 base_reg = (ISP_TOP_INPUT_SIZE - ISP_BASE) >> 2;
	u32 max_reg = (ISP_LOSSD_MIX_RO_BIT_LEN_L_3 - ISP_BASE) >> 2;
	struct isp_dev_t *isp_dev = g_isp->isp_dev;
	struct aml_reg *rreg = g_isp->rreg_buff.vaddr[AML_PLANE_A];

	for (i = base_reg; i <= max_reg; i++) {
		rreg[i].addr = i;
		rreg[i].val = isp_hwreg_read(isp_dev, ((i << 2) + ISP_BASE));
	}
}

void isp_apb_dma_init(struct isp_dev_t *isp_dev)
{
	u32 val = 0;
/* soft rst */
	val = isp_hwreg_read(isp_dev, ISP_DMA_CTL0);
	val = val | (1 << 27);
	isp_hwreg_write(isp_dev, ISP_DMA_CTL0, val);

	val = isp_hwreg_read(isp_dev, ISP_DMA_CTL0);
	val = val &  (~(1 << 27));
	isp_hwreg_write(isp_dev, ISP_DMA_CTL0, val);

/* cfg axi ctl */
	val = 0x100109;
	isp_hwreg_write(isp_dev, ISP_DMA_AXI_CTL, val);

/* cfg mask0 */
	val = isp_hwreg_read(isp_dev, ISP_DMA_IRQ_MASK0);
	val = val | (1 << 0);
	isp_hwreg_write(isp_dev, ISP_DMA_IRQ_MASK0, val);
}
