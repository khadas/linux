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

#include "aml_cam.h"

#define AML_ADAPTER_NAME "isp-adapter"
//#define ADAPTER_WDR_DUMP_LONG_FRAME

static struct adapter_dev_t *g_adap_dev[4];

static const struct aml_format adap_support_formats[] = {
	{0, 0, 0, 0, MEDIA_BUS_FMT_SBGGR8_1X8, 0, 1, 8},
	{0, 0, 0, 0, MEDIA_BUS_FMT_SGBRG8_1X8, 0, 1, 8},
	{0, 0, 0, 0, MEDIA_BUS_FMT_SGRBG8_1X8, 0, 1, 8},
	{0, 0, 0, 0, MEDIA_BUS_FMT_SRGGB8_1X8, 0, 1, 8},
	{0, 0, 0, 0, MEDIA_BUS_FMT_SBGGR10_1X10, 0, 1, 10},
	{0, 0, 0, 0, MEDIA_BUS_FMT_SGBRG10_1X10, 0, 1, 10},
	{0, 0, 0, 0, MEDIA_BUS_FMT_SGRBG10_1X10, 0, 1, 10},
	{0, 0, 0, 0, MEDIA_BUS_FMT_SRGGB10_1X10, 0, 1, 10},
	{0, 0, 0, 0, MEDIA_BUS_FMT_SBGGR12_1X12, 0, 1, 12},
	{0, 0, 0, 0, MEDIA_BUS_FMT_SGBRG12_1X12, 0, 1, 12},
	{0, 0, 0, 0, MEDIA_BUS_FMT_SGRBG12_1X12, 0, 1, 12},
	{0, 0, 0, 0, MEDIA_BUS_FMT_SRGGB12_1X12, 0, 1, 12},
	{0, 0, 0, 0, MEDIA_BUS_FMT_SBGGR14_1X14, 0, 1, 14},
	{0, 0, 0, 0, MEDIA_BUS_FMT_SGBRG14_1X14, 0, 1, 14},
	{0, 0, 0, 0, MEDIA_BUS_FMT_SGRBG14_1X14, 0, 1, 14},
	{0, 0, 0, 0, MEDIA_BUS_FMT_SRGGB14_1X14, 0, 1, 14},
	{0, 0, 0, 0, MEDIA_BUS_FMT_YUYV8_2X8, 0, 1, 8},
	{0, 0, 0, 0, MEDIA_BUS_FMT_YVYU8_2X8, 0, 1, 8},
};

int write_data_to_buf(char *buf, int size)
{
	char path[60] = {'\0'};
	int fd = -1;
	int ret = 0;
	struct file *fp = NULL;
	mm_segment_t old_fs;
	loff_t pos = 0;
	unsigned int r_size = 0;
	static int num = 0;
	num ++;

	if (buf == NULL || size == 0) {
		pr_info("%s:Error input param\n", __func__);
		return -1;
	}

	sprintf(path, "/sdcard/DCIM/adapter_long_dump-%d.raw",num);

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	fp = filp_open(path, O_RDWR|O_CREAT, 0);
	if (IS_ERR(fp)) {
		pr_info("read error.\n");
		return -1;
	}

	r_size = vfs_write(fp, buf, size, &pos);

	vfs_fsync(fp, 0);
	filp_close(fp, NULL);
	set_fs(old_fs);

	return ret;
}

struct adapter_dev_t *adap_get_dev(int index)
{
	if (index >= 4) {
		pr_err("err adap index num\n");
	}

	return g_adap_dev[index];
}

static void __iomem *adap_ioremap_resource(void *a_dev, char *name)
{
	void __iomem *reg;
	struct resource *res;
	resource_size_t size;
	struct adapter_dev_t *adap_dev;
	struct device *dev;
	struct platform_device *pdev;

	if (!a_dev || !name) {
		pr_err("Error input param\n");
		return NULL;
	}

	adap_dev = a_dev;
	dev = adap_dev->dev;
	pdev = adap_dev->pdev;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, name);
	if (!res) {
		dev_err(dev, "Error %s res\n", name);
		return NULL;
	}

	size = resource_size(res);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
	reg = devm_ioremap(dev, res->start, size);
#else
	reg = devm_ioremap_nocache(dev, res->start, size);
#endif
	if (!reg) {
		dev_err(dev, "Failed to ioremap %s %pR\n", name, res);
		reg = IOMEM_ERR_PTR(-ENOMEM);
	}

	return reg;
}

static void adap_iounmap_resource(void *a_dev)
{
	struct device *dev;
	struct adapter_dev_t *adap_dev;

	adap_dev = a_dev;
	dev = adap_dev->dev;

	if (adap_dev->adap) {
		devm_iounmap(dev, adap_dev->adap);
		adap_dev->adap = NULL;
	}

	if ((adap_dev->index == AML_CAM_4) && adap_dev->wrmif) {
		devm_iounmap(dev, adap_dev->wrmif);
		adap_dev->wrmif = NULL;
	}
}

static int adap_of_parse_version(struct adapter_dev_t *adap_dev)
{
	int rtn = 0;
	struct device *dev = adap_dev->dev;

	switch (adap_dev->version) {
	case 0:
		adap_dev->ops = &adap_dev_hw_ops;
	break;
	default:
		rtn = -EINVAL;
		dev_err(dev, "Error invalid version num: %u\n", adap_dev->version);
	break;
	}

	return rtn;
}

static int adap_alloc_raw_buffs(struct adapter_dev_t *a_dev)
{
	int i = 0;
	int rtn = 0;
	unsigned long flags;
	u32 paddr = 0x0000;
	void *virtaddr = NULL;
	unsigned int bsize = 0;
	unsigned int fsize = 0;
	unsigned int fcnt = 0;
	struct adapter_dev_param *param = &a_dev->param;
	struct adapter_global_info *g_info = aml_adap_global_get_info();

	if ((param->mode == MODE_MIPI_RAW_SDR_DDR) ||
		(param->mode == MODE_MIPI_RAW_HDR_DDR_DIRCT) ||
		(param->mode == MODE_MIPI_YUV_SDR_DDR))
		pr_info("ddr mode, alloc buffer.\n");
	else
		return rtn;

	switch (param->format) {
	case ADAP_RAW10:
		fsize = param->width * param->height * 10 / 8;
	break;
	case ADAP_RAW12:
		fsize = param->width * param->height * 12 / 8;
	break;
	case ADAP_YUV422_8BIT:
		fsize = param->width * param->height * 16 / 8;
	break;
	default:
		fsize = param->width * param->height * 10 / 8;
	break;
	}

	if (HDR_LOOPBACK_MODE)
		fsize = ISP_SIZE_ALIGN(fsize, 1 << 12) / param->height * 60;
	else
		fsize = ISP_SIZE_ALIGN(fsize, 1 << 12);

	fcnt = sizeof(param->ddr_buf) /sizeof(param->ddr_buf[0]);

	bsize = fcnt * fsize + fsize;

	rtn = aml_subdev_cma_alloc(a_dev->pdev, &paddr, virtaddr, bsize);
	if (rtn != 0) {
		pr_err("Failed to alloc raw buff\n");
		return -1;
	}

	virtaddr = aml_subdev_map_vaddr(paddr, bsize);

	INIT_LIST_HEAD(&param->free_list);

	spin_lock_init(&param->ddr_lock);

	spin_lock_irqsave(&param->ddr_lock, flags);

	for (i = 0; i < fcnt; i++) {
		param->ddr_buf[i].bsize = fsize;
		param->ddr_buf[i].devno = a_dev->index;
		param->ddr_buf[i].addr[AML_PLANE_A] = paddr + i * fsize;
		param->ddr_buf[i].vaddr[AML_PLANE_A] = virtaddr + i * fsize;
		param->ddr_buf[i].nplanes = 1;

		list_add_tail(&param->ddr_buf[i].list, &param->free_list);
	}

	param->rsvd_buf.bsize = fsize;
	param->rsvd_buf.devno = a_dev->index;
	param->rsvd_buf.addr[AML_PLANE_A] = paddr + fcnt * fsize;
	param->rsvd_buf.vaddr[AML_PLANE_A] = virtaddr + fcnt * fsize;
	param->rsvd_buf.nplanes = 1;

	spin_unlock_irqrestore(&param->ddr_lock, flags);

	return rtn;
}

static void adap_free_raw_buffs(struct adapter_dev_t *a_dev)
{
	int i = 0;
	unsigned int fcnt = 0;
	struct adapter_dev_param *param = &a_dev->param;
	unsigned long flags;
	u32 paddr = 0x0000;
	void *page = NULL;

	if ((param->mode == MODE_MIPI_RAW_SDR_DDR) ||
		(param->mode == MODE_MIPI_RAW_HDR_DDR_DIRCT) ||
		(param->mode == MODE_MIPI_YUV_SDR_DDR))
		pr_info("ddr mode, free buffer\n");
	else
		return;

	spin_lock_irqsave(&param->ddr_lock, flags);
	INIT_LIST_HEAD(&param->free_list);
	spin_unlock_irqrestore(&param->ddr_lock, flags);

	paddr = a_dev->param.ddr_buf[0].addr[AML_PLANE_A];
	page = phys_to_page(paddr);

	fcnt = sizeof(param->ddr_buf) /sizeof(param->ddr_buf[0]);

	if (paddr)
		aml_subdev_cma_free(a_dev->pdev, page, a_dev->param.ddr_buf[0].bsize * (fcnt + 1));

	aml_subdev_unmap_vaddr(a_dev->param.ddr_buf[0].vaddr[AML_PLANE_A]);

	if ((param->ddr_buf[0].addr != 0) && (param->ddr_buf[0].vaddr != NULL)) {
		for (i = 0; i < fcnt; i++) {
			param->ddr_buf[i].addr[AML_PLANE_A] = 0;
			param->ddr_buf[i].vaddr[AML_PLANE_A] = NULL;
		}
		param->rsvd_buf.addr[AML_PLANE_A] = 0;
		param->rsvd_buf.vaddr[AML_PLANE_A] = NULL;
	}
}

int adap_wdr_cfg_buf(struct adapter_dev_t *a_dev)
{
	unsigned long flags;
	struct aml_video video;
	struct adapter_dev_param *param = &a_dev->param;

	if ((param->mode == MODE_MIPI_RAW_SDR_DDR) ||
			(param->mode == MODE_MIPI_RAW_HDR_DDR_DIRCT) ||
			(param->mode == MODE_MIPI_YUV_SDR_DDR)) {
		a_dev->ops->hw_wdr_cfg_buf(a_dev);
	}

	return 0;
}

int adap_fe_cfg_buf(struct adapter_dev_t *a_dev)
{
	unsigned long flags;
	struct aml_video video;
	struct adapter_dev_param *param = &a_dev->param;

	if (a_dev->wstatus != STATUS_START)
		return -1;

	if ((param->mode == MODE_MIPI_RAW_SDR_DIRCT) ||
		(param->mode == MODE_MIPI_RAW_HDR_DDR_DIRCT) ||
		(param->mode == MODE_MIPI_YUV_SDR_DIRCT))
		return -1;

	spin_lock_irqsave(&param->ddr_lock, flags);

	param->cur_buf = list_first_entry_or_null(&param->free_list, struct aml_buffer, list);
	if (param->cur_buf) {
		list_del(&param->cur_buf->list);
	} else {
		video.id = MODE_MIPI_RAW_SDR_DDR;
		video.priv = a_dev;
		a_dev->ops->hw_fe_cfg_buf(&video, &param->rsvd_buf);

		spin_unlock_irqrestore(&param->ddr_lock, flags);

		pr_err("ADAP%d:Error free list empty\n", a_dev->index);

		return -1;
	}

	video.id = MODE_MIPI_RAW_SDR_DDR;
	video.priv = a_dev;

	a_dev->ops->hw_fe_cfg_buf(&video, param->cur_buf);

	spin_unlock_irqrestore(&param->ddr_lock, flags);

	return 0;
}

int adap_fe_done_buf(struct adapter_dev_t *a_dev)
{
	unsigned long flags;
	struct adapter_dev_param *param = &a_dev->param;
	struct adapter_global_info *g_info = aml_adap_global_get_info();

	if (a_dev->wstatus != STATUS_START)
		return -1;

	if ((param->mode == MODE_MIPI_RAW_SDR_DIRCT) ||
		(param->mode == MODE_MIPI_RAW_HDR_DDR_DIRCT) ||
		(param->mode == MODE_MIPI_YUV_SDR_DIRCT))
		return -1;

	spin_lock_irqsave(&g_info->list_lock, flags);

	if (param->cur_buf == NULL) {
		spin_unlock_irqrestore(&g_info->list_lock, flags);

		pr_debug("Error current buffer is NULL\n");

		return -1;
	}

	list_add_tail(&param->cur_buf->list, &g_info->done_list);

	spin_unlock_irqrestore(&g_info->list_lock, flags);

	return 0;
}

static int adap_of_parse_dev(struct adapter_dev_t *adap_dev)
{
	int rtn = 0;

	of_reserved_mem_device_init(adap_dev->dev);

	adap_dev->adap = adap_ioremap_resource(adap_dev, "adapter");
	if (!adap_dev->adap) {
		dev_err(adap_dev->dev, "Failed to get adapter reg\n");
		rtn = -EINVAL;
		goto error_rtn;
	}

	adap_dev->adap_clk = devm_clk_get(adap_dev->dev, "mipi_isp_clk");
#ifndef T7C_CHIP
	adap_dev->vapb_clk = devm_clk_get(adap_dev->dev, "vapb_clk");
#endif

error_rtn:
	return rtn;
}

static irqreturn_t adap_irq_handler(int irq, void *dev)
{
	int status = 0;
	unsigned long flags;
	struct adapter_dev_t *adap_dev = dev;

	spin_lock_irqsave(&adap_dev->irq_lock, flags);

	status = adap_dev->ops->hw_interrupt_status(adap_dev);
	if (status & 0x1) {
		tasklet_schedule(&adap_dev->irq_tasklet);
	}

	spin_unlock_irqrestore(&adap_dev->irq_lock, flags);

	return IRQ_HANDLED;
}

static void adap_irq_tasklet(unsigned long data)
{
	int id = 0;
	int status = 0xff001234;
	struct aml_video *video;
	struct adapter_dev_t *adap_dev = (struct adapter_dev_t *)data;

	for (id = 0; id < AML_ADAP_STREAM_MAX; id++) {
		video = &adap_dev->video[id];
		if (video->ops->cap_irq_handler)
			video->ops->cap_irq_handler(video, status);
	}
}

static int adap_request_irq(struct adapter_dev_t *adap_dev)
{
	int rtn = 0;

	if (adap_dev->index != AML_CAM_4)
		return rtn;

	adap_dev->irq = irq_of_parse_and_map(adap_dev->dev->of_node, 0);
	if (!adap_dev->irq) {
		dev_err(adap_dev->dev, "Error to parse irq\n");
		return -EINVAL;
	}

	rtn = devm_request_irq(adap_dev->dev, adap_dev->irq,
				adap_irq_handler, IRQF_SHARED,
				dev_driver_string(adap_dev->dev), adap_dev);
	if (rtn) {
		dev_err(adap_dev->dev, "Error to request irq: rtn %d\n", rtn);
		return rtn;
	}

	spin_lock_init(&adap_dev->irq_lock);
	tasklet_init(&adap_dev->irq_tasklet, adap_irq_tasklet, (unsigned long)adap_dev);

	return rtn;
}

static irqreturn_t adap_irq_handler_offline(int irq, void *dev)
{
	int status = 0;
	unsigned long flags;
	struct adapter_dev_t *adap_dev = dev;

	if (adap_dev->wstatus != STATUS_START)
		return IRQ_HANDLED;

	spin_lock_irqsave(&adap_dev->irq_lock, flags);

	status = adap_dev->ops->hw_interrupt_status(adap_dev);
	if (status & (1 << 18)) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
		adap_fe_done_buf(adap_dev);
		adap_fe_cfg_buf(adap_dev);
#else
		tasklet_schedule(&adap_dev->irq_tasklet);
#endif
	}

	spin_unlock_irqrestore(&adap_dev->irq_lock, flags);

	return IRQ_HANDLED;
}

static void adap_irq_tasklet_offline(unsigned long data)
{
	int id = 0;
	int status = 0xff001234;
	struct aml_video *video;
	struct adapter_dev_t *adap_dev = (struct adapter_dev_t *)data;

	adap_fe_done_buf(adap_dev);
	adap_fe_cfg_buf(adap_dev);
}

static int adap_request_irq_offline(struct adapter_dev_t *adap_dev)
{
	int rtn = 0;
	struct adapter_dev_param *param = &adap_dev->param;

	adap_dev->irq = irq_of_parse_and_map(adap_dev->dev->of_node, adap_dev->index);
	if (!adap_dev->irq) {
		dev_err(adap_dev->dev, "Error to parse irq\n");
		return -EINVAL;
	}

	rtn = devm_request_irq(adap_dev->dev, adap_dev->irq,
				adap_irq_handler_offline, IRQF_SHARED,
				dev_driver_string(adap_dev->dev), adap_dev);
	if (rtn) {
		dev_err(adap_dev->dev, "Error to request irq: rtn %d\n", rtn);
		return rtn;
	}

	spin_lock_init(&adap_dev->irq_lock);
	tasklet_init(&adap_dev->irq_tasklet, adap_irq_tasklet_offline, (unsigned long)adap_dev);

	return rtn;
}

static int adap_subdev_stream_on(void *priv)
{
	struct adapter_dev_t *adap_dev = priv;

	adap_dev->wstatus = STATUS_START;

	adap_alloc_raw_buffs(adap_dev);

	adap_wdr_cfg_buf(adap_dev);

	adap_fe_cfg_buf(adap_dev);

	aml_adap_global_create_thread();

	if (adap_dev->ops->hw_start)
		adap_dev->ops->hw_start(adap_dev);

	if (adap_dev->ops->hw_irq_en)
		adap_dev->ops->hw_irq_en(adap_dev);

	return 0;
}

static void adap_subdev_stream_off(void *priv)
{
	struct adapter_dev_t *adap_dev = priv;

	adap_dev->enWDRMode = WDR_MODE_NONE;
	adap_dev->wstatus = STATUS_STOP;

	aml_adap_global_destroy_thread();

	if (adap_dev->ops->hw_stop)
		adap_dev->ops->hw_stop(adap_dev);

	if (adap_dev->ops->hw_irq_dis)
		adap_dev->ops->hw_irq_dis(adap_dev);

	adap_free_raw_buffs(adap_dev);
}

static int adap_subdev_convert_fmt(struct adapter_dev_t *adap_dev,
				struct v4l2_mbus_framefmt *format)
{
	int i = 0;
	int fmt = -1;
	const struct aml_format *am_fmt;

	for (i = 0; i < adap_dev->fmt_cnt; i++) {
		if (adap_dev->formats[i].code == format->code) {
			am_fmt = &adap_dev->formats[i];
			break;
		}
	}

	if (i == adap_dev->fmt_cnt)
		return fmt;

	switch (am_fmt->bpp) {
	case 8:
		fmt = ADAP_RAW8;
	break;
	case 10:
		fmt = ADAP_RAW10;
	break;
	case 12:
		fmt = ADAP_RAW12;
	break;
	case 14:
		fmt = ADAP_RAW14;
	break;
	default:
		dev_err(adap_dev->dev, "Error support format\n");
	break;
	}

	if ((format->code == MEDIA_BUS_FMT_YVYU8_2X8) || (format->code == MEDIA_BUS_FMT_YUYV8_2X8))
		fmt = ADAP_YUV422_8BIT;

	return fmt;
}

static int adap_subdev_hw_init(struct adapter_dev_t *adap_dev,
				struct v4l2_mbus_framefmt *format)
{
	int rtn = 0;
	struct adapter_dev_param *param = &adap_dev->param;

	rtn = adap_subdev_convert_fmt(adap_dev, format);
	if (rtn < 0) {
		dev_err(adap_dev->dev, "Error to convert fmt\n");
		return rtn;
	}
	param->format = rtn;

	param->width = format->width;
	param->height = format->height;
	if (adap_dev->enWDRMode == WDR_MODE_2To1_LINE) {
		param->mode = MODE_MIPI_RAW_HDR_DDR_DIRCT;
		param->dol_type = ADAP_DOL_LINEINFO;
	} else if (adap_dev->enWDRMode == WDR_MODE_2To1_FRAME) {
		param->mode = MODE_MIPI_RAW_HDR_DDR_DIRCT;
		param->dol_type = ADAP_DOL_VC;
	} else if (adap_dev->enWDRMode == SDR_DDR_MODE) {
		param->mode = MODE_MIPI_RAW_SDR_DDR;
		param->dol_type = ADAP_DOL_NONE;
		aml_adap_global_mode(MODE_MIPI_RAW_SDR_DDR);
	} else if (adap_dev->enWDRMode == ISP_SDR_DCAM_MODE) {
		param->mode = MODE_MIPI_RAW_SDR_DDR;
		param->dol_type = ADAP_DOL_NONE;
		aml_adap_global_mode(MODE_MIPI_RAW_SDR_DDR);
		pr_err("dcam mode\n");
	} else {
		param->mode = MODE_MIPI_RAW_SDR_DIRCT;
		if (param->format == ADAP_YUV422_8BIT)
			param->mode = MODE_MIPI_YUV_SDR_DIRCT;
		param->dol_type = ADAP_DOL_NONE;
	}
	param->offset.offset_x = 0;
	param->offset.offset_y = 0;
	param->offset.long_offset = 0x8;
	param->offset.short_offset = 0x8;

	aml_adap_global_devno(adap_dev->index);

	if (adap_dev->ops->hw_reset)
		adap_dev->ops->hw_reset(adap_dev);

	if (adap_dev->ops->hw_init)
		adap_dev->ops->hw_init(adap_dev);

	return 0;
}

static int adap_subdev_set_format(void *priv, void *s_fmt, void *m_fmt)
{
	int rtn = 0;
	struct adapter_dev_t *adap_dev = priv;
	struct v4l2_subdev_format *fmt = s_fmt;
	struct v4l2_mbus_framefmt *format = m_fmt;

	if (fmt->pad == AML_ADAP_PAD_SINK)
		rtn = adap_subdev_hw_init(adap_dev, format);

	return rtn;
}

static void adap_subdev_log_status(void *priv)
{
	struct csiphy_dev_t *adap_dev = priv;

	dev_info(adap_dev->dev, "Log status done\n");
}

static const struct aml_sub_ops adap_subdev_ops = {
	.set_format = adap_subdev_set_format,
	.stream_on = adap_subdev_stream_on,
	.stream_off = adap_subdev_stream_off,
	.log_status = adap_subdev_log_status,
};

static int adap_subdev_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct adapter_dev_t *adap_dev = container_of(ctrl->handler,
					     struct adapter_dev_t, ctrls);
	int ret = 0;

	switch (ctrl->id) {
	case V4L2_CID_AML_MODE:
		pr_info("adap_subdev_set_ctrl:%d\n", ctrl->val);
		adap_dev->enWDRMode = ctrl->val;
		break;
	default:
		pr_err( "Error ctrl->id %u, flag 0x%lx\n", ctrl->id, ctrl->flags);
		break;
	}

	return ret;
}

static const struct v4l2_ctrl_ops adap_ctrl_ops = {
	.s_ctrl = adap_subdev_set_ctrl,
};

static struct v4l2_ctrl_config mode_cfg = {
	.ops = &adap_ctrl_ops,
	.id = V4L2_CID_AML_MODE,
	.name = "adap mode",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	.min = 0,
	.max = 5,
	.step = 1,
	.def = 0,
};

static int adap_subdev_ctrls_init(struct adapter_dev_t *adap_dev)
{
	int rtn = 0;

	v4l2_ctrl_handler_init(&adap_dev->ctrls, 1);

	adap_dev->wdr = v4l2_ctrl_new_custom(&adap_dev->ctrls, &mode_cfg, NULL);

	adap_dev->sd.ctrl_handler = &adap_dev->ctrls;

	if (adap_dev->ctrls.error) {
		rtn = adap_dev->ctrls.error;
	}

	return rtn;
}

static int adap_subdev_power_on(struct adapter_dev_t *adap_dev)
{
	int rtn = 0;
#ifndef T7C_CHIP
	rtn = clk_prepare_enable(adap_dev->vapb_clk);
	if (rtn)
		pr_err("Error to enable vapb clk\n");
#endif
	clk_set_rate(adap_dev->adap_clk, 666666666);
	rtn = clk_prepare_enable(adap_dev->adap_clk);
	if (rtn)
		dev_err(adap_dev->dev, "Error to enable adap_clk(isp) clk\n");

	return rtn;
}

static void adap_subdev_power_off(struct adapter_dev_t *adap_dev)
{
	if (adap_dev->index == AML_CAM_4) {
		clk_disable_unprepare(adap_dev->wrmif_clk);
	}
}

int aml_adap_subdev_register(struct adapter_dev_t *adap_dev)
{
	int rtn = -1;
	struct media_pad *pads = adap_dev->pads;
	struct aml_subdev *subdev = &adap_dev->subdev;

	adap_dev->formats = adap_support_formats;
	adap_dev->fmt_cnt = ARRAY_SIZE(adap_support_formats);

	pads[AML_ADAP_PAD_SINK].flags = MEDIA_PAD_FL_SINK;
	pads[AML_ADAP_PAD_SRC].flags = MEDIA_PAD_FL_SOURCE;

	subdev->name = AML_ADAPTER_NAME;
	subdev->dev = adap_dev->dev;
	subdev->sd = &adap_dev->sd;
	subdev->function = MEDIA_ENT_F_VID_IF_BRIDGE;
	subdev->v4l2_dev = adap_dev->v4l2_dev;
	subdev->pads = pads;
	subdev->pad_max = AML_ADAP_PAD_MAX;
	subdev->formats = adap_dev->formats;
	subdev->fmt_cnt = adap_dev->fmt_cnt;
	subdev->pfmt = adap_dev->pfmt;
	subdev->ops = &adap_subdev_ops;
	subdev->priv = adap_dev;

	adap_subdev_ctrls_init(adap_dev);

	rtn = aml_subdev_register(subdev);
	if (rtn)
		goto error_rtn;

	dev_info(adap_dev->dev, "ADAP%u: register subdev\n", adap_dev->index);

error_rtn:
	return rtn;
}

void aml_adap_subdev_unregister(struct adapter_dev_t *adap_dev)
{
	aml_subdev_unregister(&adap_dev->subdev);
}

int aml_adap_subdev_init(void *c_dev)
{
	int rtn = -1;
	struct platform_device *pdev;
	struct adapter_dev_t *adap_dev;
	struct device_node *node;
	struct cam_device *cam_dev = c_dev;

	adap_dev = &cam_dev->adap_dev;

	node = of_parse_phandle(cam_dev->dev->of_node, "adapter", 0);
	if (!node) {
		pr_err("Failed to parse adapter handle\n");
		return rtn;
	}

	adap_dev->pdev = of_find_device_by_node(node);
	if (!adap_dev->pdev) {
		of_node_put(node);
		pr_err("Failed to find adapter platform device");
		return rtn;
	}
	of_node_put(node);

	pdev = adap_dev->pdev;
	adap_dev->dev = &pdev->dev;
	adap_dev->v4l2_dev = &cam_dev->v4l2_dev;
	adap_dev->index = cam_dev->index;
	adap_dev->bus_info = cam_dev->bus_info;
	adap_dev->enWDRMode = WDR_MODE_NONE;
	platform_set_drvdata(pdev, adap_dev);

	rtn = adap_of_parse_version(adap_dev);
	if (rtn) {
		dev_err(adap_dev->dev, "Failed to parse version\n");
		return rtn;
	}

	rtn = adap_of_parse_dev(adap_dev);
	if (rtn) {
		dev_err(adap_dev->dev, "Failed to parse dev\n");
		return rtn;
	}

	rtn = adap_subdev_power_on(adap_dev);
	if (rtn) {
		dev_err(adap_dev->dev, "Failed to power on\n");
		return rtn;
	}

	aml_adap_global_init();

	rtn = adap_request_irq_offline(adap_dev);
	if (rtn)
		adap_iounmap_resource(adap_dev);

	dev_info(adap_dev->dev, "ADAP%u: subdev init\n", adap_dev->index);

	g_adap_dev[cam_dev->index] = adap_dev;

	return rtn;
}

void aml_adap_subdev_deinit(void *c_dev)
{
	struct adapter_dev_t *adap_dev;
	struct cam_device *cam_dev;

	cam_dev = c_dev;
	adap_dev = &cam_dev->adap_dev;

	adap_subdev_power_off(adap_dev);

	adap_iounmap_resource(adap_dev);

	tasklet_kill(&adap_dev->irq_tasklet);
	devm_free_irq(adap_dev->dev, adap_dev->irq, adap_dev);

	if (adap_dev->index == AML_CAM_4)
		devm_clk_put(adap_dev->dev, adap_dev->wrmif_clk);

	dev_info(adap_dev->dev, "ADAP%u: subdev deinit\n", adap_dev->index);
}
