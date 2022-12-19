/*
*
* SPDX-License-Identifier: GPL-2.0
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
#include <linux/version.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/of_reserved_mem.h>
#include <linux/pm_runtime.h>
#include <linux/pm_domain.h>
#include <linux/interrupt.h>
#include <linux/irqreturn.h>
#include <linux/delay.h>

#include "aml_isp.h"
#include "aml_cam.h"

#include "aml_adapter.h"

static struct isp_global_info g_isp_info;

struct isp_global_info *isp_global_get_info(void)
{
	return &g_isp_info;
}

static int isp_global_reg_buf_alloc(struct isp_dev_t *isp_dev)
{
	u32 rsize;
	dma_addr_t paddr = 0x0000;
	void *virtaddr = NULL;
	struct isp_global_info *g_info = isp_global_get_info();

	rsize = 128 * 1024;

	virtaddr = dma_alloc_coherent(isp_dev->dev, rsize, &paddr, GFP_KERNEL);

	g_info->rreg_buff.nplanes = 1;
	g_info->rreg_buff.addr[AML_PLANE_A] = paddr;
	g_info->rreg_buff.vaddr[AML_PLANE_A] = virtaddr;

	pr_debug("isp global reg alloc\n");

	return 0;
}

static int isp_global_reg_buf_free(struct isp_dev_t *isp_dev)
{
	u32 paddr = 0x0000;
	void *vaddr = NULL;
	u32 rsize = 0;
	struct isp_global_info *g_info = isp_global_get_info();

	paddr = g_info->rreg_buff.addr[AML_PLANE_A];
	vaddr = g_info->rreg_buff.vaddr[AML_PLANE_A];
	rsize = 128 * 1024;

	if (vaddr)
		dma_free_coherent(isp_dev->dev, rsize, vaddr, (dma_addr_t)paddr);

	g_info->rreg_buff.addr[AML_PLANE_A] = 0x0000;
	g_info->rreg_buff.vaddr[AML_PLANE_A] = NULL;

	pr_debug("isp global reg free\n");

	return 0;
}

void isp_global_mode(int mode)
{
	struct isp_global_info *g_info = isp_global_get_info();

	g_info->mode = mode;
}

int isp_global_manual_apb_dma(int vdev)
{
	struct isp_dev_t *isp_dev = isp_subdrv_get_dev(vdev);
	struct isp_global_info *g_info = isp_global_get_info();
	struct aml_reg *wregs = isp_dev->wreg_buff.vaddr[AML_PLANE_A];
	int i = 0;

	if ((g_info->mode == AML_ISP_SCAM) ||
		(isp_dev->apb_dma == 0) ||
		(isp_dev->isp_status == STATUS_STOP)) {
		pr_err("donot set ISP%d\n", vdev);
		return -1;
	}

	dma_sync_single_for_device(isp_dev->dev,isp_dev->wreg_buff.addr[AML_PLANE_A], isp_dev->wreg_buff.bsize, DMA_TO_DEVICE);

	isp_dev->ops->hw_start_apb_dma(isp_dev);
	isp_dev->ops->hw_manual_trigger_apb_dma(isp_dev);
	isp_dev->ops->hw_check_done_apb_dma(isp_dev);
	isp_dev->ops->hw_stop_apb_dma(isp_dev);
	/*for (i = isp_dev->fwreg_cnt; i < isp_dev->twreg_cnt; i ++)
		isp_hwreg_write(isp_dev, wregs[i].addr - isp_dev->phy_base, wregs[i].val);*/

	pr_debug("global dma %d-%d count:%d-%d\n", isp_dev->index, vdev, isp_dev->twreg_cnt, isp_dev->twreg_cnt - isp_dev->fwreg_cnt);

	return 0;
}

int isp_global_reset(struct isp_dev_t *isp_dev)
{
	struct isp_global_info *g_info = isp_global_get_info();

	isp_dev->frm_cnt = 0;
	isp_dev->wreg_cnt = 0;
	isp_dev->fwreg_cnt = 0;
	isp_dev->twreg_cnt = 0;
	memset(isp_dev->lutWr.lutRegW, 0, sizeof(isp_dev->lutWr.lutRegW));
	memset(isp_dev->lutWr.lutRegAddr, 0, sizeof(isp_dev->lutWr.lutRegAddr));

	if (g_info->user)
		return 0;

	isp_dev->ops->hw_reset(isp_dev);
	isp_dev->ops->hw_fill_gisp_rreg_buff(g_info);

	return 0;
}

void isp_global_stream_on(void)
{
	struct isp_global_info *g_info = isp_global_get_info();

	g_info->user ++;
}

void isp_global_stream_off(void)
{
	struct isp_global_info *g_info = isp_global_get_info();

	g_info->user --;
}

int isp_global_init(struct isp_dev_t *isp_dev)
{
	struct isp_global_info *g_info = isp_global_get_info();

	if (g_info->status == STATUS_START)
		return 0;

	isp_global_reg_buf_alloc(isp_dev);

	g_info->isp_dev = isp_dev;
	g_info->mode = AML_ISP_SCAM;

	isp_dev->ops->hw_reset(isp_dev);
	isp_dev->ops->hw_fill_gisp_rreg_buff(g_info);

	g_info->status = STATUS_START;

	return 0;
}

int isp_global_deinit(struct isp_dev_t *isp_dev)
{
	struct isp_global_info *g_info = isp_global_get_info();

	if (g_info->isp_dev == isp_dev)
		isp_global_reg_buf_free(isp_dev);

	g_info->status = STATUS_STOP;
	g_info->isp_dev = NULL;

	return 0;
}

