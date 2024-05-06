// SPDX-License-Identifier: GPL-2.0
/*
 * Rockchip SerDes RKx11x Serializer driver
 *
 * Copyright (C) 2024 Rockchip Electronics Co., Ltd.
 *
 * Author: Cai Wenzhong <cwz@rock-chips.com>
 *
 * V1.00.00 rockchip serdes rkx11x driver framework.
 *
 * V1.01.00 support pinctrl and passthrough
 *
 */
#include <linux/version.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/compat.h>
#include <linux/of_device.h>

#include "rkx11x_i2c.h"
#include "rkx11x_drv.h"
#include "rkx11x_reg.h"
#include "rkx11x_api.h"

#define DRIVER_VERSION		KERNEL_VERSION(1, 0x01, 0x00)

#define IOMUX_PINSET_API_EN	1 /* 1: PINCTRL API enable */

int rkx11x_linktx_pma_enable(struct i2c_client *ser_client, bool enable, u32 pma_id)
{
	struct rkser_dev *ser_dev = NULL;
	struct rkx11x *rkx11x = NULL;
	struct rkx11x_linktx *linktx = NULL;
	int ret = 0;

	if (ser_client == NULL)
		return -EINVAL;

	ser_dev = i2c_get_clientdata(ser_client);
	if (ser_dev == NULL)
		return -EINVAL;

	rkx11x = ser_dev->ser_data;
	if (rkx11x == NULL)
		return -EINVAL;

	linktx = &rkx11x->linktx;
	if ((linktx == NULL)
			|| (linktx->i2c_reg_read == NULL)
			|| (linktx->i2c_reg_write == NULL)
			|| (linktx->i2c_reg_update == NULL))
		return -EINVAL;

	dev_info(&ser_client->dev, "%s\n", __func__);

	ret = rkx11x_ser_pma_enable(linktx, enable, pma_id);

	return ret;
}
EXPORT_SYMBOL(rkx11x_linktx_pma_enable);

int rkx11x_linktx_set_rate(struct i2c_client *ser_client,
				uint32_t rate_mode,
				uint32_t pll_refclk_div,
				uint32_t pll_div,
				uint32_t clk_div,
				bool pll_div4,
				bool pll_fck_vco_div2,
				bool force_init_en)
{
	struct rkser_dev *ser_dev = NULL;
	struct rkx11x *rkx11x = NULL;
	struct rkx11x_linktx *linktx = NULL;
	struct rkx11x_pma_pll pma_pll;
	int ret = 0;

	if (ser_client == NULL)
		return -EINVAL;

	ser_dev = i2c_get_clientdata(ser_client);
	if (ser_dev == NULL)
		return -EINVAL;

	rkx11x = ser_dev->ser_data;
	if (rkx11x == NULL)
		return -EINVAL;

	linktx = &rkx11x->linktx;
	if ((linktx == NULL)
			|| (linktx->i2c_reg_read == NULL)
			|| (linktx->i2c_reg_write == NULL)
			|| (linktx->i2c_reg_update == NULL))
		return -EINVAL;

	dev_info(&ser_client->dev, "%s\n", __func__);

	pma_pll.rate_mode = rate_mode;
	pma_pll.pll_refclk_div = pll_refclk_div;
	pma_pll.pll_div = pll_div;
	pma_pll.clk_div = clk_div;
	pma_pll.pll_div4 = pll_div4;
	pma_pll.pll_fck_vco_div2 = pll_fck_vco_div2;
	pma_pll.force_init_en = force_init_en;

	ret = rkx11x_ser_pma_set_rate(linktx, &pma_pll);

	return ret;
}
EXPORT_SYMBOL(rkx11x_linktx_set_rate);

int rkx11x_linktx_set_line(struct i2c_client *ser_client, u32 link_line)
{
	struct rkser_dev *ser_dev = NULL;
	struct rkx11x *rkx11x = NULL;
	struct rkx11x_linktx *linktx = NULL;
	int ret = 0;

	if (ser_client == NULL)
		return -EINVAL;

	ser_dev = i2c_get_clientdata(ser_client);
	if (ser_dev == NULL)
		return -EINVAL;

	rkx11x = ser_dev->ser_data;
	if (rkx11x == NULL)
		return -EINVAL;

	linktx = &rkx11x->linktx;
	if ((linktx == NULL)
			|| (linktx->i2c_reg_read == NULL)
			|| (linktx->i2c_reg_write == NULL)
			|| (linktx->i2c_reg_update == NULL))
		return -EINVAL;

	dev_info(&ser_client->dev, "%s\n", __func__);

	ret = rkx11x_ser_pma_set_line(linktx, link_line);

	return ret;
}
EXPORT_SYMBOL(rkx11x_linktx_set_line);

static void rkx11x_callback_init(struct rkx11x *rkx11x)
{
	rkx11x->i2c_reg_read = rkx11x_i2c_reg_read;
	rkx11x->i2c_reg_write = rkx11x_i2c_reg_write;
	rkx11x->i2c_reg_update = rkx11x_i2c_reg_update;
}

static int rkx11x_check_chip_id(struct rkx11x *rkx11x)
{
	struct i2c_client *client = rkx11x->client;
	struct device *dev = &client->dev;
	u32 chip_id;
	int ret = 0, loop = 0;

	for (loop = 0; loop < 5; loop++) {
		if (loop != 0) {
			dev_info(dev, "check rkx11x chipid retry (%d)", loop);
			msleep(10);
		}
		ret = rkx11x->i2c_reg_read(client, RKX11X_GRF_REG_CHIP_ID, &chip_id);
		if (ret == 0) {
			if (chip_id == rkx11x->chip_id) {
				if (chip_id == RKX11X_SER_CHIP_ID_V0) {
					dev_info(dev, "rkx11x v0, chip_id: 0x%08x\n", chip_id);
					rkx11x->version = RKX11X_SER_CHIP_V0;
					return 0;
				}

				if (chip_id == RKX11X_SER_CHIP_ID_V1) {
					dev_info(dev, "rkx11x v1, chip_id: 0x%08x\n", chip_id);
					rkx11x->version = RKX11X_SER_CHIP_V1;
					return 0;
				}
			} else {
				// if chip id is unexpected, retry
				dev_err(dev, "Unexpected rkx11x chipid = 0x%08x\n", chip_id);
			}
		}
	}

	dev_err(dev, "rkx11x check chipid error, ret(%d)\n", ret);

	return -ENODEV;
}

/* rkx11x cru module */
static int rkx11x_add_hwclk(struct rkx11x *rkx11x)
{
	struct i2c_client *client = rkx11x->client;
	struct device *dev = &client->dev;
	struct hwclk *hwclk = NULL;
	struct xferclk xfer;
	int ret = 0;

	dev_info(dev, "rkx11x add hwclk\n");

	memset(&xfer, 0, sizeof(struct xferclk));

	snprintf(xfer.name, sizeof(xfer.name), "0x%x", client->addr);
	xfer.type = CLK_RKX110;
	xfer.client = client;
	if (rkx11x->version == RKX11X_SER_CHIP_V0) {
		xfer.version = RKX110_SER_CLK;
	} else if (rkx11x->version == RKX11X_SER_CHIP_V1) {
		xfer.version = RKX111_SER_CLK;
	} else {
		xfer.version = -1;
		dev_err(dev, "%s chip version error\n", __func__);
		return -1;
	}
	xfer.read = rkx11x->i2c_reg_read;
	xfer.write = rkx11x->i2c_reg_write;

	hwclk = &rkx11x->hwclk;
	ret = rkx11x_hwclk_register(hwclk, &xfer);
	if (ret)
		dev_err(dev, "%s error, ret(%d)\n", __func__, ret);

	return ret;
}

static int rkx11x_clk_hw_init(struct rkx11x *rkx11x)
{
	struct i2c_client *client = rkx11x->client;
	struct device *dev = &client->dev;
	struct hwclk *hwclk = &rkx11x->hwclk;
	u32 cru_dclk_id;
	int ret = 0;

	dev_info(dev, "rkx11x clock init\n");

	ret = rkx11x_hwclk_init(hwclk);
	if (ret) {
		dev_err(dev, "hwclk init error, ret(%d)\n", ret);
		return ret;
	}

	dev_info(dev, "serdes dclk set rate = %d", rkx11x->serdes_dclk_hz);

	cru_dclk_id = RKX110_CPS_DCLK_RX_PRE;
	if ((rkx11x->stream_interface == RKX11X_STREAM_INTERFACE_DVP)
			&& (rkx11x->version == RKX11X_SER_CHIP_V1)) {
		/* chip v1: dvp dclk change to RKX111_CPS_DCLK_RX_PRE_200M */
		cru_dclk_id = RKX111_CPS_DCLK_RX_PRE_200M;
	}

	ret = rkx11x_hwclk_set_rate(hwclk, cru_dclk_id, rkx11x->serdes_dclk_hz);
	if (ret)
		dev_err(dev, "hwclk serdes dclk set rate error, ret(%d)\n", ret);

	return ret;
}

/* rkx11x pin module */
static int rkx11x_add_hwpin(struct rkx11x *rkx11x)
{
	struct i2c_client *client = rkx11x->client;
	struct device *dev = &client->dev;
	struct hwpin *hwpin = NULL;
	struct xferpin xfer;
	int ret = 0;

	dev_info(dev, "rkx11x add hwpin\n");

	memset(&xfer, 0, sizeof(struct xferpin));

	snprintf(xfer.name, sizeof(xfer.name), "0x%x", client->addr);
	xfer.type = PIN_RKX110;
	xfer.client = client;
	xfer.read = rkx11x->i2c_reg_read;
	xfer.write = rkx11x->i2c_reg_write;

	hwpin = &rkx11x->hwpin;
	ret = rkx11x_hwpin_register(hwpin, &xfer);
	if (ret)
		dev_err(dev, "%s error, ret(%d)\n", __func__, ret);

	return ret;
}

/* rkx11x rxphy module */
static int rkx11x_add_rxphy(struct rkx11x *rkx11x)
{
	struct device *dev = &rkx11x->client->dev;

	dev_info(dev, "rkx11x add rxphy\n");

	switch (rkx11x->stream_interface) {
	case RKX11X_STREAM_INTERFACE_MIPI:
		rkx11x->rxphy.phy_id = RKX11X_RXPHY_0;
		rkx11x->rxphy.phy_mode = RKX11X_RXPHY_MODE_MIPI;
		break;
	case RKX11X_STREAM_INTERFACE_DVP:
		rkx11x->rxphy.phy_id = RKX11X_RXPHY_0;
		rkx11x->rxphy.phy_mode = RKX11X_RXPHY_MODE_GPIO;
		break;
	default:
		dev_info(dev, "stream interface = %d error for rxphy\n",
			rkx11x->stream_interface);
		return -1;
	}

	rkx11x->rxphy.client = rkx11x->client;
	rkx11x->rxphy.i2c_reg_read = rkx11x->i2c_reg_read;
	rkx11x->rxphy.i2c_reg_write = rkx11x->i2c_reg_write;
	rkx11x->rxphy.i2c_reg_update = rkx11x->i2c_reg_update;

	return 0;
}

static int rkx11x_rxphy_parse_dt(struct device *dev,
				struct rkx11x_rxphy *rxphy)
{
	struct property *prop = NULL;
	u32 lane_map[4] = { 0, 0, 0, 0 }, num_lanes = 0, array_len = 0;
	int ret = 0, i = 0;

	if (dev == NULL || rxphy == NULL)
		return -EINVAL;

	prop = of_find_property(dev->of_node, "rxphy-data-lanes", &array_len);
	if (!prop)
		return -EINVAL;

	num_lanes = array_len / sizeof(u32);
	if (num_lanes > 4)
		num_lanes = 4;
	if (num_lanes == 0)
		return -EINVAL;

	dev_info(dev, "rxphy-data-lanes property: lane_num = %d\n", num_lanes);

	ret = of_property_read_u32_array(dev->of_node, "rxphy-data-lanes",
					lane_map, num_lanes);
	if (ret)
		return ret;

	for (i = 0; i < num_lanes; i++) {
		if (lane_map[i] != 0)
			dev_info(dev, "rxphy-data-lanes property: lane_map[%d] = %d\n",
				i, lane_map[i]);
	}

	rxphy->data_lanes = num_lanes;

	return 0;
}

static int rkx11x_rxphy_hw_init(struct rkx11x *rkx11x)
{
	dev_info(&rkx11x->client->dev, "rkx11x rxphy hw init\n");

	return rkx11x_rxphy_init(&rkx11x->rxphy);
}

/* rkx11x csi2host module */
static int rkx11x_add_csi2host(struct rkx11x *rkx11x)
{
	dev_info(&rkx11x->client->dev, "rkx11x add csi2host\n");

	if (rkx11x->rxphy.phy_id == RKX11X_RXPHY_0)
		rkx11x->csi2host.id = RKX11X_CSI2HOST_0;
	else
		rkx11x->csi2host.id = RKX11X_CSI2HOST_1;

	rkx11x->csi2host.interface = RKX11X_CSI2HOST_CSI;
	rkx11x->csi2host.phy_mode = RKX11X_CSI2HOST_DPHY;
	rkx11x->csi2host.data_lanes = rkx11x->rxphy.data_lanes;

	rkx11x->csi2host.client = rkx11x->client;
	rkx11x->csi2host.i2c_reg_read = rkx11x->i2c_reg_read;
	rkx11x->csi2host.i2c_reg_write = rkx11x->i2c_reg_write;
	rkx11x->csi2host.i2c_reg_update = rkx11x->i2c_reg_update;

	return 0;
}

static int rkx11x_csi2host_hw_init(struct rkx11x *rkx11x)
{
	if (rkx11x->stream_interface != RKX11X_STREAM_INTERFACE_MIPI)
		return 0;

	dev_info(&rkx11x->client->dev, "rkx11x csi2host hw init\n");

	return rkx11x_csi2host_init(&rkx11x->csi2host);
}

/* rkx11x vicap module */
static int rkx11x_vicap_csi_parse_dt(struct device *dev,
				struct rkx11x_vicap *vicap)
{
	struct device_node *node = NULL, *vicap_csi_node = NULL;
	const char *csi_vc_id_name = "csi-vc-id";
	struct rkx11x_vicap_csi *vicap_csi = NULL;
	u32 sub_idx = 0, vc_id = 0, value = 0;
	int ret = 0;

	if (dev == NULL || vicap == NULL)
		return -EINVAL;

	vicap_csi = &vicap->vicap_csi;

	vicap_csi_node = of_get_child_by_name(dev->of_node, "vicap-csi");
	if (IS_ERR_OR_NULL(vicap_csi_node)) {
		dev_err(dev, "%pOF has no child node: vicap-csi\n",
				dev->of_node);
		return -ENODEV;
	}

	if (!of_device_is_available(vicap_csi_node)) {
		dev_info(dev, "%pOF is disabled\n", vicap_csi_node);
		of_node_put(vicap_csi_node);
		return -ENODEV;
	}

	while ((node = of_get_next_child(vicap_csi_node, node))) {
		if (!strncasecmp(node->name,
				 csi_vc_id_name,
				 strlen(csi_vc_id_name))) {
			if (sub_idx >= RKX11X_VICAP_VC_ID_MAX) {
				dev_err(dev, "%pOF: Too many matching %s node\n",
						vicap_csi_node, csi_vc_id_name);

				of_node_put(node);
				break;
			}

			if (!of_device_is_available(node)) {
				dev_info(dev, "%pOF is disabled\n", node);
				sub_idx++;
				continue;
			}

			/* vc id */
			ret = of_property_read_u32(node, "vc-id", &vc_id);
			if (ret) {
				// if vc_id is error, parse next node
				dev_err(dev, "Can not get vc-id property!");

				sub_idx++;
				continue;
			}
			if (vc_id >= RKX11X_VICAP_VC_ID_MAX) {
				// if vc_id is error, parse next node
				dev_err(dev, "Error vc-id = %d!", vc_id);

				sub_idx++;
				continue;
			}

			dev_info(dev, "vicap csi vc id = %d\n", vc_id);

			vicap_csi->vc_flag |= BIT(vc_id);

			ret = of_property_read_u32(node, "virtual-channel", &value);
			if (ret == 0) {
				dev_info(dev, "virtual-channel property: %d", value);
				vicap_csi->vc_id[vc_id].vc = value;
			}

			ret = of_property_read_u32(node, "width", &value);
			if (ret == 0) {
				dev_info(dev, "width property: %d", value);
				vicap_csi->vc_id[vc_id].width = value;
			}

			ret = of_property_read_u32(node, "height", &value);
			if (ret == 0) {
				dev_info(dev, "height property: %d", value);
				vicap_csi->vc_id[vc_id].height = value;
			}

			ret = of_property_read_u32(node, "data-type", &value);
			if (ret == 0) {
				dev_info(dev, "data-type property: 0x%x", value);
				vicap_csi->vc_id[vc_id].data_type = value;
			}

			ret = of_property_read_u32(node, "parse-type", &value);
			if (ret == 0) {
				dev_info(dev, "parse-type property: %d", value);
				vicap_csi->vc_id[vc_id].parse_type = value;
			}

			sub_idx++;
		}
	}

	of_node_put(vicap_csi_node);

	return 0;
}

static int rkx11x_vicap_dvp_parse_dt(struct device *dev,
				struct rkx11x_vicap *vicap)
{
	struct device_node *vicap_dvp_node = NULL;
	struct rkx11x_vicap_dvp *vicap_dvp = NULL;
	u32 value = 0;
	int ret = 0;

	if (dev == NULL || vicap == NULL)
		return -EINVAL;

	vicap_dvp = &vicap->vicap_dvp;

	vicap_dvp_node = of_get_child_by_name(dev->of_node, "vicap-dvp");
	if (IS_ERR_OR_NULL(vicap_dvp_node)) {
		dev_err(dev, "%pOF has no child node: vicap-dvp\n",
				dev->of_node);
		return -ENODEV;
	}

	if (!of_device_is_available(vicap_dvp_node)) {
		dev_info(dev, "%pOF is disabled\n", vicap_dvp_node);
		of_node_put(vicap_dvp_node);
		return -ENODEV;
	}

	ret = of_property_read_u32(vicap_dvp_node, "input-mode", &value);
	if (ret == 0) {
		dev_info(dev, "input-mode property: %d", value);
		vicap_dvp->input_mode = value;
	}

	ret = of_property_read_u32(vicap_dvp_node, "raw-width", &value);
	if (ret == 0) {
		dev_info(dev, "raw-width property: %d", value);
		vicap_dvp->raw_width = value;
	}

	ret = of_property_read_u32(vicap_dvp_node, "yuv-order", &value);
	if (ret == 0) {
		dev_info(dev, "yuv-order: %d", value);
		vicap_dvp->yuv_order = value;
	}

	ret = of_property_read_u32(vicap_dvp_node, "clock-invert", &value);
	if (ret == 0) {
		dev_info(dev, "clock-invert: %d", value);
		vicap_dvp->clock_invert = value;
	}

	ret = of_property_read_u32(vicap_dvp_node, "href-polarity", &value);
	if (ret == 0) {
		dev_info(dev, "href-polarity: %d", value);
		vicap_dvp->href_pol = value;
	}

	ret = of_property_read_u32(vicap_dvp_node, "vsync-polarity", &value);
	if (ret == 0) {
		dev_info(dev, "vsync-polarity: %d", value);
		vicap_dvp->vsync_pol = value;
	}

	of_node_put(vicap_dvp_node);

	return 0;
}

static int rkx11x_add_vicap(struct rkx11x *rkx11x)
{
	struct device *dev = &rkx11x->client->dev;

	dev_info(dev, "rkx11x add vicap\n");

	rkx11x->vicap.id = 0; /* fixed 0 */

	switch (rkx11x->stream_interface) {
	case RKX11X_STREAM_INTERFACE_MIPI:
		rkx11x->vicap.interface = RKX11X_VICAP_CSI;
		break;
	case RKX11X_STREAM_INTERFACE_DVP:
		rkx11x->vicap.interface = RKX11X_VICAP_DVP;
		break;
	default:
		dev_info(dev, "stream interface = %d error for vicap\n",
			rkx11x->stream_interface);
		return -1;
	}

	rkx11x->vicap.client = rkx11x->client;
	rkx11x->vicap.i2c_reg_read = rkx11x->i2c_reg_read;
	rkx11x->vicap.i2c_reg_write = rkx11x->i2c_reg_write;
	rkx11x->vicap.i2c_reg_update = rkx11x->i2c_reg_update;

	return 0;
}

static int rkx11x_vicap_hw_init(struct rkx11x *rkx11x)
{
	struct device *dev = &rkx11x->client->dev;
	struct hwclk *hwclk = &rkx11x->hwclk;
	int ret = 0;

	dev_info(dev, "rkx11x vicap hw init\n");

	switch (rkx11x->stream_interface) {
	case RKX11X_STREAM_INTERFACE_MIPI:
		dev_info(dev, "VICAP: CSI: cru reset\n");
		ret |= rkx11x_hwclk_reset(hwclk, RKX110_SRST_DRESETN_VICAP_CSI);
		usleep_range(10, 20);
		ret |= rkx11x_hwclk_reset_deassert(hwclk, RKX110_SRST_DRESETN_VICAP_CSI);
		usleep_range(10, 20);
		break;
	case RKX11X_STREAM_INTERFACE_DVP:
		dev_info(dev, "VICAP: DVP: cru reset\n");
		ret |= rkx11x_hwclk_reset(hwclk, RKX110_SRST_PRESETN_VICAP_DVP_RX);
		usleep_range(10, 20);
		ret |= rkx11x_hwclk_reset_deassert(hwclk, RKX110_SRST_PRESETN_VICAP_DVP_RX);
		usleep_range(10, 20);
		break;
	default:
		dev_info(dev, "stream interface = %d error for vicap\n",
			rkx11x->stream_interface);
		ret = -1;
		break;
	}
	if (ret)
		return ret;

	return rkx11x_vicap_init(&rkx11x->vicap);
}

/* rkx11x linktx module */
static int rkx11x_linktx_parse_dt(struct device *dev,
				struct rkx11x_linktx *linktx)
{
	struct device_node *node = NULL, *linktx_node = NULL;
	const char *lane_cfg_name = "lane-config";
	struct rkx11x_lane_cfg *lane_cfg = NULL;
	u32 sub_idx = 0, lane_id = 0, value = 0;
	int ret = 0;

	if (dev == NULL || linktx == NULL)
		return -EINVAL;

	linktx_node = of_get_child_by_name(dev->of_node, "linktx");
	if (IS_ERR_OR_NULL(linktx_node)) {
		dev_err(dev, "%pOF has no child node: linktx\n",
				dev->of_node);
		return -ENODEV;
	}

	if (!of_device_is_available(linktx_node)) {
		dev_info(dev, "%pOF is disabled\n", linktx_node);
		of_node_put(linktx_node);
		return -ENODEV;
	}

	ret = of_property_read_u32(linktx_node, "video-packet-size", &value);
	if (ret == 0) {
		dev_info(dev, "video-packet-size property: %d", value);
		linktx->video_packet_size = value;
	}

	while ((node = of_get_next_child(linktx_node, node))) {
		if (!strncasecmp(node->name,
				 lane_cfg_name,
				 strlen(lane_cfg_name))) {
			if (sub_idx >= RKX11X_LINK_LANE_MAX) {
				dev_err(dev, "%pOF: Too many matching %s node\n",
						linktx_node, lane_cfg_name);

				of_node_put(node);
				break;
			}

			if (!of_device_is_available(node)) {
				dev_info(dev, "%pOF is disabled\n", node);
				sub_idx++;
				continue;
			}

			/* lane id */
			ret = of_property_read_u32(node, "lane-id", &lane_id);
			if (ret) {
				// if lane_id is error, parse next node
				dev_err(dev, "Can not get lane-id property!");

				sub_idx++;
				continue;
			}
			if (lane_id >= RKX11X_LINK_LANE_MAX) {
				// if lane id is error, parse next node
				dev_err(dev, "Error lane-id = %d!", lane_id);

				sub_idx++;
				continue;
			}

			dev_info(dev, "linktx lane id = %d\n", lane_id);

			lane_cfg = &linktx->lane_cfg[lane_id];

			/* lane enable */
			lane_cfg->lane_enable = 1;

			dev_info(dev, "linktx lane id = %d: lane_enable = %d\n",
					lane_id, lane_cfg->lane_enable);

			lane_cfg->dsource_id = lane_id;
			ret = of_property_read_u32(node, "dsource-id", &value);
			if (ret == 0) {
				dev_info(dev, "dsource-id property: %d", value);
				lane_cfg->dsource_id = value;
			}

			sub_idx++;
		}
	}

	of_node_put(linktx_node);

	return 0;
}

static int rkx11x_add_linktx(struct rkx11x *rkx11x)
{
	struct device *dev = &rkx11x->client->dev;
	struct rkx11x_linktx *linktx = &rkx11x->linktx;

	dev_info(dev, "rkx11x add linktx\n");

	// version
	if (rkx11x->version == RKX11X_SER_CHIP_V0) {
		linktx->version = RKX11X_LINKTX_V0;
	} else if (rkx11x->version == RKX11X_SER_CHIP_V1) {
		linktx->version = RKX11X_LINKTX_V1;
	} else {
		dev_err(dev, "linktx version error!\n");
		return -EINVAL;
	}

	switch (rkx11x->stream_interface) {
	case RKX11X_STREAM_INTERFACE_MIPI:
		linktx->stream_source = RKX11X_LINK_LANE_STREAM_MIPI;
		break;
	case RKX11X_STREAM_INTERFACE_DVP:
		linktx->stream_source = RKX11X_LINK_LANE_STREAM_DVP;

		if (linktx->lane_cfg[RKX11X_LINK_LANE0].lane_enable == 0) {
			dev_err(dev, "dvp mode lane0 should be enable\n");
			return -EINVAL;
		}

		if (linktx->lane_cfg[RKX11X_LINK_LANE1].lane_enable) {
			dev_err(dev, "dvp mode should only enable only lane\n");
			return -EINVAL;
		}
		break;
	default:
		dev_err(dev, "stream interface = %d error for linktx\n",
			rkx11x->stream_interface);
		return -1;
	}

	rkx11x->linktx.client = rkx11x->client;
	rkx11x->linktx.i2c_reg_read = rkx11x->i2c_reg_read;
	rkx11x->linktx.i2c_reg_write = rkx11x->i2c_reg_write;
	rkx11x->linktx.i2c_reg_update = rkx11x->i2c_reg_update;

	return 0;
}

static int rkx11x_linktx_hw_init(struct rkx11x *rkx11x)
{
	struct device *dev = &rkx11x->client->dev;
	struct rkx11x_linktx *linktx = &rkx11x->linktx;
	int ret = 0;

	dev_info(dev, "rkx11x linktx hw init\n");

	ret = rkx11x_linktx_lane_init(linktx);
	if (ret) {
		dev_err(dev, "%s: linktx lane init error\n", __func__);
		return ret;
	}

	ret = rkx11x_linktx_video_enable(linktx, true);
	if (ret) {
		dev_err(dev, "%s: linktx video enable error\n", __func__);
		return ret;
	}

	return 0;
}

/* rkx11x pinctrl init */
static int rkx11x_pinctrl_init(struct rkx11x *rkx11x)
{
	struct device *dev = &rkx11x->client->dev;
	const char *pinctrl_name = "rkx11x-pins";
	struct device_node *np = NULL;
	u32 *pinctrl_configs = NULL;
	u32 bank, pins, pin_configs;
	int length, i, ret = 0;

	dev_info(dev, "rkx11x pinctrl init\n");

	/* rkx11x-pins = <bank pins pin_configs>; */
	for_each_child_of_node(dev->of_node, np) {
		if (!of_device_is_available(np))
			continue;

		length = of_property_count_u32_elems(np, pinctrl_name);
		if (length < 0)
			continue;
		if (length % 3) {
			dev_err(dev, "%s: invalid count for pinctrl\n", np->name);
			continue;
		}

		pinctrl_configs = kmalloc_array(length, sizeof(u32), GFP_KERNEL);
		if (!pinctrl_configs)
			continue;

		ret = of_property_read_u32_array(np, pinctrl_name, pinctrl_configs, length);
		if (ret) {
			dev_err(dev, "%s: pinctrl configs data error\n", np->name);
			kfree(pinctrl_configs);
			continue;
		}

		for (i = 0; i < length; i += 3) {
			bank = pinctrl_configs[i + 0];
			pins = pinctrl_configs[i + 1];
			pin_configs = pinctrl_configs[i + 2];

			dev_info(dev, "%s: bank = %d, pins = 0x%08x, pin_configs = 0x%08x\n",
					np->name, bank, pins, pin_configs);

			ret = rkx11x_hwpin_set(&rkx11x->hwpin, bank, pins, pin_configs);
			if (ret)
				dev_err(dev, "%s: pinctrl configs error\n", np->name);
		}

		kfree(pinctrl_configs);
	}

	return 0;
}

/* rkx11x passthrough init */
static int rkx11x_passthrough_init(struct rkx11x *rkx11x)
{
	struct device *dev = &rkx11x->client->dev;
	const char *pt_name = "rkx11x-pt";
	struct device_node *np = NULL;
	u32 *pt_configs = NULL;
	u32 func_id = 0, pt_rx = 0;
	int length, i, ret = 0;

	dev_info(dev, "rkx11x passthrough init\n");

	/* rkx11x-pt = <passthrough_func_id passthrough_rx>; */
	for_each_child_of_node(dev->of_node, np) {
		if (!of_device_is_available(np))
			continue;

		length = of_property_count_u32_elems(np, pt_name);
		if (length < 0)
			continue;
		if (length % 2) {
			dev_err(dev, "%s: invalid count for passthrough\n", np->name);
			continue;
		}

		pt_configs = kmalloc_array(length, sizeof(u32), GFP_KERNEL);
		if (!pt_configs)
			continue;

		ret = of_property_read_u32_array(np, pt_name, pt_configs, length);
		if (ret) {
			dev_err(dev, "%s: passthrough configs data error\n", np->name);
			kfree(pt_configs);
			continue;
		}

		for (i = 0; i < length; i += 2) {
			func_id = pt_configs[i + 0];
			pt_rx = pt_configs[i + 1];

			dev_info(dev, "%s: passthrough func_id = %d, pt_rx = %d\n",
					np->name, func_id, pt_rx);

			ret = rkx11x_linktx_passthrough_cfg(&rkx11x->linktx, func_id, pt_rx);
			if (ret)
				dev_err(dev, "%s: passthrough func_id = %d config error\n",
						np->name, func_id);
		}

		kfree(pt_configs);
	}

	return 0;
}

/* rkx11x camera mclk */
static int rkx11x_camera_mclk_enable(struct rkx11x *rkx11x)
{
	struct i2c_client *client = rkx11x->client;
	struct device *dev = &client->dev;
	struct hwclk *hw = &rkx11x->hwclk;
	u32 mclk_hz = rkx11x->camera_mclk_hz;
	u32 reg = 0, val = 0;
	int ret = 0;

	if (rkx11x->camera_mclk_hz == 0)
		return 0;

	dev_info(dev, "%s: camera mclk = %d HZ\n", __func__, mclk_hz);

	/* mclk iomux */
#if (IOMUX_PINSET_API_EN == 0)
	if (rkx11x->camera_mclk_id == 0) {
		val = HIWORD_UPDATE(3, GENMASK(8, 6), 6); /* MIPI_MCLK0 */
		reg = SER_GRF_GPIO0B_IOMUX_H;
	} else {
		val = HIWORD_UPDATE(3, GENMASK(11, 9), 9); /* MIPI_MCLK1 */
		reg = SER_GRF_GPIO0C_IOMUX_L;
	}
	ret = rkx11x->i2c_reg_write(client, reg, val);
#else
	reg = 0; val = 0;
	if (rkx11x->camera_mclk_id == 0)
		ret = rkx11x_hwpin_set(&rkx11x->hwpin, RKX11X_SER_GPIO_BANK0,
					RKX11X_GPIO_PIN_B7, RKX11X_PIN_CONFIG_MUX_FUNC3);
	else
		ret = rkx11x_hwpin_set(&rkx11x->hwpin, RKX11X_SER_GPIO_BANK0,
					RKX11X_GPIO_PIN_C3, RKX11X_PIN_CONFIG_MUX_FUNC3);
#endif
	if (ret) {
		dev_err(dev, "%s: mclk iomux error\n", __func__);
		return ret;
	}

	/* mclk set rate */
	if (rkx11x->camera_mclk_id == 0) {
		ret |= rkx11x_hwclk_set_rate(hw, RKX110_CPS_CLK_CAM0_OUT2IO, mclk_hz);
		ret |= rkx11x_hwclk_enable(hw, RKX110_CLK_CAM0_OUT2IO_GATE);
	} else {
		ret |= rkx11x_hwclk_set_rate(hw, RKX110_CPS_CLK_CAM1_OUT2IO, mclk_hz);
		ret |= rkx11x_hwclk_enable(hw, RKX110_CLK_CAM1_OUT2IO_GATE);
	}

	return ret;
}

static int rkx11x_camera_mclk_disable(struct rkx11x *rkx11x)
{
	struct i2c_client *client = rkx11x->client;
	struct device *dev = &client->dev;
	struct hwclk *hw = &rkx11x->hwclk;
	int ret = 0;

	if (rkx11x->camera_mclk_hz == 0)
		return 0;

	dev_info(dev, "%s\n", __func__);

	if (rkx11x->camera_mclk_id == 0)
		ret = rkx11x_hwclk_disable(hw, RKX110_CLK_CAM0_OUT2IO_GATE);
	else
		ret = rkx11x_hwclk_disable(hw, RKX110_CLK_CAM1_OUT2IO_GATE);

	return ret;
}

/* rkx11x dvp iomux */
static int rkx11x_dvp_iomux_cfg(struct rkx11x *rkx11x)
{
	struct i2c_client *client = rkx11x->client;
	struct device *dev = &client->dev;
#if (IOMUX_PINSET_API_EN == 0)
	u32 reg = 0, val = 0;
	int ret = 0;

	dev_info(dev, "rkx11x dvp iomux\n");

	reg = SER_GRF_GPIO1B_IOMUX;
	val = 0;
	val |= HIWORD_UPDATE(GPIO1B3_CIF_D15, GPIO1B3_MASK, GPIO1B3_SHIFT);
	val |= HIWORD_UPDATE(GPIO1B2_CIF_D14, GPIO1B2_MASK, GPIO1B2_SHIFT);
	val |= HIWORD_UPDATE(GPIO1B1_CIF_D13, GPIO1B1_MASK, GPIO1B1_SHIFT);
	val |= HIWORD_UPDATE(GPIO1B0_CIF_D12, GPIO1B0_MASK, GPIO1B0_SHIFT);
	ret |= rkx11x->i2c_reg_write(client, reg, val);

	reg = SER_GRF_GPIO1A_IOMUX;
	val = 0;
	val |= HIWORD_UPDATE(GPIO1A7_CIF_D11, GPIO1A7_MASK, GPIO1A7_SHIFT);
	val |= HIWORD_UPDATE(GPIO1A6_CIF_D10, GPIO1A6_MASK, GPIO1A6_SHIFT);
	val |= HIWORD_UPDATE(GPIO1A5_CIF_D9, GPIO1A5_MASK, GPIO1A5_SHIFT);
	val |= HIWORD_UPDATE(GPIO1A4_CIF_D8, GPIO1A4_MASK, GPIO1A4_SHIFT);
	val |= HIWORD_UPDATE(GPIO1A3_CIF_D7, GPIO1A3_MASK, GPIO1A3_SHIFT);
	val |= HIWORD_UPDATE(GPIO1A2_CIF_D6, GPIO1A2_MASK, GPIO1A2_SHIFT);
	val |= HIWORD_UPDATE(GPIO1A1_CIF_D5, GPIO1A1_MASK, GPIO1A1_SHIFT);
	val |= HIWORD_UPDATE(GPIO1A0_CIF_D4, GPIO1A0_MASK, GPIO1A0_SHIFT);
	ret |= rkx11x->i2c_reg_write(client, reg, val);

	reg = SER_GRF_GPIO0C_IOMUX_H;
	val = 0;
	val |= HIWORD_UPDATE(GPIO0C7_CIF_D3, GPIO0C7_MASK, GPIO0C7_SHIFT);
	val |= HIWORD_UPDATE(GPIO0C6_CIF_D2, GPIO0C6_MASK, GPIO0C6_SHIFT);
	val |= HIWORD_UPDATE(GPIO0C5_CIF_D1, GPIO0C5_MASK, GPIO0C5_SHIFT);
	ret |= rkx11x->i2c_reg_write(client, reg, val);

	reg = SER_GRF_GPIO0C_IOMUX_L;
	val = 0;
	val |= HIWORD_UPDATE(GPIO0C4_CIF_D0, GPIO0C4_MASK, GPIO0C4_SHIFT);
	val |= HIWORD_UPDATE(GPIO0C2_CIF_HSYNC, GPIO0C2_MASK, GPIO0C2_SHIFT);
	val |= HIWORD_UPDATE(GPIO0C1_CIF_VSYNC, GPIO0C1_MASK, GPIO0C1_SHIFT);
	val |= HIWORD_UPDATE(GPIO0C0_CIF_CLK, GPIO0C0_MASK, GPIO0C0_SHIFT);
	ret |= rkx11x->i2c_reg_write(client, reg, val);

	if (ret) {
		dev_err(dev, "rkx11x dvp iomux error\n");
	}

	return 0;
#else /* IOMUX_PINSET_API_EN */
	uint32_t pins;
	int ret = 0;

	dev_info(dev, "rkx110 dvp pin iomux.\n");

	pins = RKX11X_GPIO_PIN_C0 | RKX11X_GPIO_PIN_C1 | RKX11X_GPIO_PIN_C2 |
	       RKX11X_GPIO_PIN_C4 | RKX11X_GPIO_PIN_C5 |
	       RKX11X_GPIO_PIN_C6 | RKX11X_GPIO_PIN_C7;
	ret |= rkx11x_hwpin_set(&rkx11x->hwpin, RKX11X_SER_GPIO_BANK0,
				pins, RKX11X_PIN_CONFIG_MUX_FUNC2);

	pins = RKX11X_GPIO_PIN_A0 | RKX11X_GPIO_PIN_A1 | RKX11X_GPIO_PIN_A2 |
	       RKX11X_GPIO_PIN_A3 | RKX11X_GPIO_PIN_A4 | RKX11X_GPIO_PIN_A5 |
	       RKX11X_GPIO_PIN_A6 | RKX11X_GPIO_PIN_A7 | RKX11X_GPIO_PIN_B0 |
	       RKX11X_GPIO_PIN_B1 | RKX11X_GPIO_PIN_B2 | RKX11X_GPIO_PIN_B3;
	ret |= rkx11x_hwpin_set(&rkx11x->hwpin, RKX11X_SER_GPIO_BANK1,
				pins, RKX11X_PIN_CONFIG_MUX_FUNC2);

	if (ret) {
		dev_err(dev, "rkx11x dvp iomux error\n");
	}

	return 0;
#endif /* IOMUX_PINSET_API_EN */
}

/* rkx11x module init */
static int rkx11x_module_init(struct rkser_dev *rkser_dev)
{
	struct rkx11x *rkx11x = rkser_dev->ser_data;
	struct device *dev = &rkx11x->client->dev;
	int ret = 0;

	dev_info(dev, "%s\n", __func__);

	ret = rkx11x_check_chip_id(rkx11x);
	if (ret)
		return ret;

	ret = rkx11x_clk_hw_init(rkx11x);
	if (ret) {
		dev_err(dev, "%s: clock init error\n", __func__);
		return ret;
	}

	ret = rkx11x_linktx_hw_init(rkx11x);
	if (ret) {
		dev_err(dev, "%s: linktx hw init error\n", __func__);
		return ret;
	}

	ret = rkx11x_vicap_hw_init(rkx11x);
	if (ret) {
		dev_err(dev, "%s: vicap hw init error\n", __func__);
		return ret;
	}

	ret = rkx11x_csi2host_hw_init(rkx11x);
	if (ret) {
		dev_err(dev, "%s: csi2host init error\n", __func__);
		return ret;
	}

	ret = rkx11x_rxphy_hw_init(rkx11x);
	if (ret) {
		dev_err(dev, "%s: rxphy init error\n", __func__);
		return ret;
	}

	if (rkx11x->stream_interface == RKX11X_STREAM_INTERFACE_DVP) {
		ret = rkx11x_dvp_iomux_cfg(rkx11x);
		if (ret) {
			dev_err(dev, "%s: dvp iomux config error\n", __func__);
			return ret;
		}
	}

	ret = rkx11x_pinctrl_init(rkx11x);
	if (ret) {
		dev_err(dev, "%s: pinctrl init error\n", __func__);
		return ret;
	}

	ret = rkx11x_passthrough_init(rkx11x);
	if (ret) {
		dev_err(dev, "%s: passthrough init error\n", __func__);
		return ret;
	}

	rkx11x_camera_mclk_enable(rkx11x);

	rkx11x_i2c_run_init_seq(rkx11x->client, &rkx11x->ser_init_seq);

	rkser_dev->ser_state = RKSER_STATE_INIT;

	return 0;
}

static int rkx11x_module_deinit(struct rkser_dev *rkser_dev)
{
	struct rkx11x *rkx11x = rkser_dev->ser_data;
	struct device *dev = &rkx11x->client->dev;

	dev_info(dev, "%s\n", __func__);

	rkx11x_camera_mclk_disable(rkx11x);

	rkser_dev->ser_state = RKSER_STATE_DEINIT;

	return 0;
}

static struct rkser_ops rkx11x_ops = {
	.ser_module_init = rkx11x_module_init,
	.ser_module_deinit = rkx11x_module_deinit,
};


static int rkx11x_parse_dt(struct rkx11x *rkx11x)
{
	struct device *dev = &rkx11x->client->dev;
	struct device_node *node = dev->of_node;
	struct device_node *init_seq_node = NULL;
	struct rkx11x_i2c_init_seq *init_seq = NULL;
	u32 value = 0;
	int ret = 0;

	dev_info(dev, "=== rkx11x parse dt ===\n");

	ret = of_property_read_u32(node, "stream-interface", &value);
	if (ret == 0) {
		dev_info(dev, "stream-interface property: %d", value);
		rkx11x->stream_interface = value;
	}

	ret = of_property_read_u32(node, "serdes-dclk-hz", &value);
	if (ret == 0) {
		dev_info(dev, "serdes-dclk-hz property: %d", value);
		rkx11x->serdes_dclk_hz = value;
	}

	ret = of_property_read_u32(node, "camera-mclk-hz", &value);
	if (ret == 0) {
		dev_info(dev, "camera-mclk-hz property: %d", value);
		rkx11x->camera_mclk_hz = value;
	}

	ret = of_property_read_u32(node, "camera-mclk-id", &value);
	if (ret == 0) {
		dev_info(dev, "camera-mclk-id property: %d", value);
		rkx11x->camera_mclk_id = value;
	}

	rkx11x_rxphy_parse_dt(dev, &rkx11x->rxphy);

	if (rkx11x->stream_interface == RKX11X_STREAM_INTERFACE_MIPI)
		rkx11x_vicap_csi_parse_dt(dev, &rkx11x->vicap);

	if (rkx11x->stream_interface == RKX11X_STREAM_INTERFACE_DVP)
		rkx11x_vicap_dvp_parse_dt(dev, &rkx11x->vicap);

	rkx11x_linktx_parse_dt(dev, &rkx11x->linktx);

	/* ser init sequence */
	init_seq_node = of_get_child_by_name(node, "ser-init-sequence");
	if (!IS_ERR_OR_NULL(init_seq_node)) {
		dev_info(dev, "load ser-init-sequence\n");

		init_seq = &rkx11x->ser_init_seq;
		rkx11x_i2c_load_init_seq(dev,
				init_seq_node, init_seq);

		of_node_put(init_seq_node);
	}

	return 0;
}

static int rkx11x_module_data_init(struct rkx11x *rkx11x)
{
	struct device *dev = &rkx11x->client->dev;
	int ret = 0;

	dev_info(dev, "=== rkx11x module data init ===\n");

	rkx11x->stream_interface = RKX11X_STREAM_INTERFACE_MIPI; // MIPI Port

	rkx11x->serdes_dclk_hz = (200 * 1000 * 1000); // serdes dclk
	rkx11x->camera_mclk_hz = 0; // MCLK disable
	rkx11x->camera_mclk_id = 0; // MCLK id

	rkx11x->linktx.lane_cfg[RKX11X_LINK_LANE0].dsource_id = 0;
	rkx11x->linktx.lane_cfg[RKX11X_LINK_LANE1].dsource_id = 1;

	ret = rkx11x_parse_dt(rkx11x);
	if (ret) {
		dev_err(dev, "rkx11x parse dt error\n");
		return ret;
	}

	return 0;
}

static int rkx11x_module_sw_init(struct rkx11x *rkx11x)
{
	struct device *dev = &rkx11x->client->dev;
	int ret = 0;

	dev_info(dev, "=== rkx11x module sw init ===\n");

	ret = rkx11x_add_hwclk(rkx11x);
	if (ret) {
		dev_err(dev, "rkx11x add hwclk error\n");
		return ret;
	}

	ret = rkx11x_add_hwpin(rkx11x);
	if (ret) {
		dev_err(dev, "rkx11x add hwpin error\n");
		return ret;
	}

	ret = rkx11x_add_rxphy(rkx11x);
	if (ret) {
		dev_err(dev, "rkx11x add rxphy error\n");
		return ret;
	}

	ret = rkx11x_add_csi2host(rkx11x);
	if (ret) {
		dev_err(dev, "rkx11x add csi2host error\n");
		return ret;
	}

	ret = rkx11x_add_vicap(rkx11x);
	if (ret) {
		dev_err(dev, "rkx11x add vicap error\n");
		return ret;
	}

	ret = rkx11x_add_linktx(rkx11x);
	if (ret) {
		dev_err(dev, "rkx11x add linktx error\n");
		return ret;
	}

	return 0;
}

static int rkx11x_i2c_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct rkser_dev *rkser_dev = NULL;
	struct rkx11x *rkx11x = NULL;
	u32 chip_id;
	int ret = 0;

	dev_info(dev, "driver version: %02x.%02x.%02x", DRIVER_VERSION >> 16,
		(DRIVER_VERSION & 0xff00) >> 8, DRIVER_VERSION & 0x00ff);

	chip_id = (uintptr_t)of_device_get_match_data(dev);
	if (chip_id == RKX11X_SER_CHIP_ID_V0) {
		dev_info(dev, "rockchip serializer driver for rkx110\n");
	} else if (chip_id == RKX11X_SER_CHIP_ID_V1) {
		dev_info(dev, "rockchip serializer driver for rkx111\n");
	} else {
		dev_err(dev, "rockchip serializer driver unknown chip\n");
		return -EINVAL;
	}

	rkser_dev = devm_kzalloc(dev, sizeof(*rkser_dev), GFP_KERNEL);
	if (!rkser_dev) {
		dev_err(dev, "rkser_dev probe no memory error\n");
		return -ENOMEM;
	}

	rkx11x = devm_kzalloc(dev, sizeof(*rkx11x), GFP_KERNEL);
	if (!rkx11x) {
		dev_err(dev, "rkx11x probe no memory error\n");
		return -ENOMEM;
	}

	rkx11x->client = client;
	rkx11x->chip_id = chip_id;
	rkx11x->parent = rkser_dev;

	if (rkx11x->chip_id == RKX11X_SER_CHIP_ID_V0)
		rkx11x->version = RKX11X_SER_CHIP_V0;
	if (rkx11x->chip_id == RKX11X_SER_CHIP_ID_V1)
		rkx11x->version = RKX11X_SER_CHIP_V1;

	rkx11x_callback_init(rkx11x);

	rkser_dev->client = client;
	rkser_dev->ser_ops = &rkx11x_ops;
	rkser_dev->ser_state = RKSER_STATE_DEINIT;
	rkser_dev->ser_data = rkx11x;

	i2c_set_clientdata(client, rkser_dev);

	mutex_init(&rkx11x->mutex);

	ret = rkx11x_module_data_init(rkx11x);
	if (ret)
		goto err_destroy_mutex;

	ret = rkx11x_module_sw_init(rkx11x);
	if (ret)
		goto err_destroy_mutex;

	ret = rkx11x_debugfs_init(rkx11x);
	if (ret)
		goto err_destroy_mutex;

	return 0;

err_destroy_mutex:
	mutex_destroy(&rkx11x->mutex);

	return ret;
}

#if KERNEL_VERSION(6, 1, 0) > LINUX_VERSION_CODE
static int rkx11x_i2c_remove(struct i2c_client *client)
#else
static void rkx11x_i2c_remove(struct i2c_client *client)
#endif
{
	struct rkser_dev *rkser_dev = i2c_get_clientdata(client);
	struct rkx11x *rkx11x = rkser_dev->ser_data;

	rkx11x_debugfs_deinit(rkx11x);

	mutex_destroy(&rkx11x->mutex);

	rkser_dev->ser_state = RKSER_STATE_DEINIT;

#if KERNEL_VERSION(6, 1, 0) > LINUX_VERSION_CODE
	return 0;
#endif
}

static const struct of_device_id rkx11x_of_match[] = {
	{
		.compatible = "rockchip,ser,rkx110",
		.data = (const void *)RKX11X_SER_CHIP_ID_V0
	}, {
		.compatible = "rockchip,ser,rkx111",
		.data = (const void *)RKX11X_SER_CHIP_ID_V1
	}, {
		.compatible = "rockchip,ser,rk671",
		.data = (const void *)RKX11X_SER_CHIP_ID_V1
	},
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, rkx11x_of_match);

static struct i2c_driver rkx11x_i2c_driver = {
	.driver = {
		.name = "rkx11x_ser",
		.of_match_table = of_match_ptr(rkx11x_of_match),
	},
	.probe		= &rkx11x_i2c_probe,
	.remove		= &rkx11x_i2c_remove,
};

module_i2c_driver(rkx11x_i2c_driver);

MODULE_AUTHOR("Cai Wenzhong <cwz@rock-chips.com>");
MODULE_DESCRIPTION("Rockchip RKx11x Serializer Driver");
MODULE_LICENSE("GPL");
