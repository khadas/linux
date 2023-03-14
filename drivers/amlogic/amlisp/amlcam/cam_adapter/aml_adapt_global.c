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

#define pr_fmt(fmt)  "aml-adap:%s:%d: " fmt, __func__, __LINE__
#include <linux/version.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/irqreturn.h>
#include <linux/of_platform.h>
#include <linux/of_reserved_mem.h>
#include <linux/of_irq.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/freezer.h>

#include "aml_cam.h"
#include "aml_adapter.h"

static struct adapter_global_info  global_info;

static int aml_adap_global_get_done_buf(void)
{
	unsigned long flags;
	struct list_head *dlist = NULL;
	struct adapter_global_info *g_info = aml_adap_global_get_info();

	if (g_info->mode != MODE_MIPI_RAW_SDR_DDR)
		return -1;

	spin_lock_irqsave(&g_info->list_lock, flags);

	g_info->done_buf = list_first_entry_or_null(&g_info->done_list, struct aml_buffer, list);
	if (g_info->done_buf) {
		list_del(&g_info->done_buf->list);
	} else {
		spin_unlock_irqrestore(&g_info->list_lock, flags);

		pr_debug("Error global done list empty\n");

		return -1;
	}

	spin_unlock_irqrestore(&g_info->list_lock, flags);

	return 0;
}

static int aml_adap_global_put_done_buf(void)
{
	int i = 0;
	unsigned long flags;
	unsigned int fcnt = 0;
	struct adapter_dev_t *a_dev = NULL;
	struct adapter_dev_param *param = NULL;
	struct adapter_global_info *g_info = aml_adap_global_get_info();

	if (g_info->mode != MODE_MIPI_RAW_SDR_DDR)
		return -1;

	a_dev = adap_get_dev(g_info->done_buf->devno);
	param = &a_dev->param;

	spin_lock_irqsave(&param->ddr_lock, flags);

	fcnt = sizeof(param->ddr_buf) / sizeof(param->ddr_buf[0]);
	for (i = 0; i < fcnt; i++) {
		if (g_info->done_buf == (&param->ddr_buf[i]))
			break;
	}

	if (i == fcnt) {
		spin_unlock_irqrestore(&param->ddr_lock, flags);

		pr_err("Error to find this buff\n");
		return -1;
	}

	list_add_tail(&g_info->done_buf->list, &param->free_list);

	g_info->done_buf = NULL;

	spin_unlock_irqrestore(&param->ddr_lock, flags);

	return 0;
}

static int aml_adap_global_cfg_rd_buf(int vdev)
{
	int offset = 0;
	unsigned long flags;
	struct aml_video video;
	struct aml_buffer *buff;
	struct adapter_dev_t *a_dev = adap_get_dev(vdev);
	struct adapter_dev_param *param = &a_dev->param;

	struct adapter_global_info *g_info = aml_adap_global_get_info();

	if (g_info->mode != MODE_MIPI_RAW_SDR_DDR)
		return -1;

	spin_lock_irqsave(&g_info->list_lock, flags);

	video.id = MODE_MIPI_RAW_SDR_DDR;
	video.priv = a_dev;
	buff = g_info->done_buf;

	buff->addr[AML_PLANE_B] = buff->addr[AML_PLANE_A];
	a_dev->ops->hw_rd_cfg_buf(&video, buff);

	spin_unlock_irqrestore(&g_info->list_lock, flags);

	return 0;
}

int aml_adap_rd_enable(int vdev)
{
	struct aml_video video;
	struct adapter_dev_t *a_dev = adap_get_dev(vdev);
	struct adapter_dev_param *param = &a_dev->param;

	if (param->mode != MODE_MIPI_RAW_SDR_DDR)
		return -1;

	video.id = MODE_MIPI_RAW_SDR_DDR;
	video.priv = a_dev;

	a_dev->ops->hw_rd_enable(&video);

	return 0;
}

int aml_adap_offline_mode(int vdev)
{
	struct aml_video video;
	struct adapter_dev_t *a_dev = adap_get_dev(vdev);
	struct adapter_dev_param *param = &a_dev->param;

	if (param->mode != MODE_MIPI_RAW_SDR_DDR)
		return -1;

	a_dev->ops->hw_offline_mode(a_dev);

	return 0;
}

static int aml_adap_global_thread(void *data)
{
	long rtn = 0;
	int ret = 0;
	struct adapter_global_info *g_info = data;

	g_info->task_status = STATUS_START;
	set_freezable();

	while (1) {
		try_to_freeze();

		if ( kthread_should_stop() )
			break;

		if (g_info->task_status == STATUS_STOP) {
			pr_err("aml_adap_global_thread exit\n");
			break;
		}

		rtn = aml_adap_global_get_done_buf();
		if (rtn != 0) {
			msleep(1);
			continue;
		}

		ret = isp_global_manual_apb_dma(g_info->done_buf->devno);
		if (ret < 0) {
			pr_err("isp %d close, user %d\n", g_info->done_buf->devno, g_info->user);
			msleep(200);
			continue;
		}

		aml_adap_offline_mode(g_info->done_buf->devno);
		aml_adap_global_cfg_rd_buf(g_info->done_buf->devno);
		aml_adap_rd_enable(g_info->done_buf->devno);

		rtn = wait_for_completion_timeout(&g_info->complete, 200);
		if (!rtn) {
			aml_adap_global_put_done_buf();

			pr_err("Timeout to wait done complete\n");

			continue;
		}

		aml_adap_global_put_done_buf();
	}

	complete(&g_info->g_cmpt);

	return 0;
}

struct adapter_global_info *aml_adap_global_get_info(void)
{
	return &global_info;
}

void aml_adap_global_mode(int mode)
{
	struct adapter_global_info *g_info = aml_adap_global_get_info();

	g_info->mode = mode;
}

void aml_adap_global_devno(int devno)
{
	struct adapter_global_info *g_info = aml_adap_global_get_info();

	g_info->devno = devno;
}

int aml_adap_global_init(void)
{
	struct adapter_global_info *g_info = aml_adap_global_get_info();

	g_info->task_status = STATUS_STOP;

	g_info->mode = MODE_MIPI_RAW_SDR_DIRCT;

	g_info->user = 0;

	spin_lock_init(&g_info->list_lock);

	INIT_LIST_HEAD(&g_info->done_list);

	return 0;
}

int aml_adap_global_create_thread(void)
{
	struct adapter_global_info *g_info = aml_adap_global_get_info();

	if (g_info->mode != MODE_MIPI_RAW_SDR_DDR)
		return -1;

	g_info->user ++;
	if (g_info->user > 1)
		return 0;

	init_completion(&g_info->g_cmpt);
	init_completion(&g_info->complete);

	g_info->task = kthread_run(aml_adap_global_thread, g_info, "adap-global");
	if (g_info->task == NULL) {
		pr_err("Error to create adap global thread\n");

		return -1;
	}
	return 0;
}

int aml_adap_global_done_completion(void)
{
	struct adapter_global_info *g_info = aml_adap_global_get_info();

	if (g_info->mode != MODE_MIPI_RAW_SDR_DDR)
		return -1;

	complete(&g_info->complete);

	return 0;
}

int aml_adap_global_get_vdev(void)
{
	struct adapter_global_info *g_info = aml_adap_global_get_info();

	if (g_info->mode != MODE_MIPI_RAW_SDR_DDR)
		return g_info->devno;

	if (g_info->done_buf == NULL) {
		pr_err("get vdev error\n");
		return g_info->devno;
	}

	return g_info->done_buf->devno;
}

void aml_adap_global_destroy_thread(void)
{
	unsigned long flags;
	struct adapter_global_info *g_info = aml_adap_global_get_info();

	if (g_info->mode != MODE_MIPI_RAW_SDR_DDR)
		return;

	g_info->user --;
	if (g_info->user)
		return;

	g_info->task_status = STATUS_STOP;
	wait_for_completion(&g_info->g_cmpt);
	kthread_stop(g_info->task);

	spin_lock_irqsave(&g_info->list_lock, flags);
	INIT_LIST_HEAD(&g_info->done_list);
	spin_unlock_irqrestore(&g_info->list_lock, flags);
}

