// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2024 Rockchip Electronics Co. Ltd.
 *
 * Author: Cai Wenzhong <cwz@rock-chips.com>
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>

#include "rkx11x_reg.h"
#include "rkx11x_csi2host.h"

/* csi2host base */
#define CSI2_HOST0_BASE		RKX11X_CSI2HOST0_BASE
#define CSI2_HOST1_BASE		RKX11X_CSI2HOST1_BASE

#define CSI2HOST_DEV(id)	((id) == 0 ? (CSI2_HOST0_BASE) : (CSI2_HOST1_BASE))

/* csi2host register */
#define CSI2HOST_VERSION	0x0000
#define CSI2HOST_N_LANES	0x0004
#define N_LANES(x)		UPDATE(x, 1, 0)

#define CSI2HOST_RESETN		0x0010
#define RESETN(x)		UPDATE(x, 0, 0)

#define CSI2HOST_PHY_STATE	0x0014
#define CSI2HOST_ERR1		0x0020
#define CSI2HOST_ERR2		0x0024
#define CSI2HOST_MSK1		0x0028
#define CSI2HOST_MSK2		0x002c
#define CSI2HOST_CONTROL	0x0040
#define SW_DATETYPE_LE_MASK	GENMASK(31, 26)
#define SW_DATATYPE_LE(x)	UPDATE(x, 31, 26)
#define SW_DATETYPE_LS_MASK	GENMASK(25, 20)
#define SW_DATETYPE_LS(x)	UPDATE(x, 25, 20)
#define SW_DATETYPE_FE_MASK	GENMASK(19, 14)
#define SW_DATETYPE_FE(x)	UPDATE(x, 19, 14)
#define SW_DATETYPE_FS_MASK	GENMASK(13, 8)
#define SW_DATETYPE_FS(x)	UPDATE(x, 13, 8)
#define SW_DPHY_ADAPTER_MASK	GENMASK(5, 5)
#define SW_DPHY_ADAPTER_MUX(x)	UPDATE(x, 5, 5)
#define SW_DSI_CSI2_MASK	GENMASK(1, 1)
#define SW_SEL_DSI		UPDATE(1, 1, 1)
#define SW_SEL_CSI2		UPDATE(0, 1, 1)
#define SW_CDPHY_MASK		GENMASK(0, 0)
#define SW_SEL_DPHY		UPDATE(0, 0, 0)
#define SW_SEL_CPHY		UPDATE(1, 0, 0)

int rkx11x_csi2host_init(struct rkx11x_csi2host *csi2host)
{
	struct i2c_client *client = NULL;
	u32 csi2host_base = 0;
	u32 reg = 0, mask = 0, val = 0;
	int ret = 0;

	if ((csi2host == NULL) || (csi2host->client == NULL)
			|| (csi2host->i2c_reg_read == NULL)
			|| (csi2host->i2c_reg_write == NULL)
			|| (csi2host->i2c_reg_update == NULL))
		return -EINVAL;

	client = csi2host->client;
	csi2host_base = CSI2HOST_DEV(csi2host->id);

	/* csi2host version */
	reg = csi2host_base + CSI2HOST_VERSION;
	ret |= csi2host->i2c_reg_read(client, reg, &val);
	dev_info(&client->dev, "CSI2HOST: Version = %x\n", val);

	/* csi2host phy status */
	reg = csi2host_base + CSI2HOST_PHY_STATE;
	ret |= csi2host->i2c_reg_read(client, reg, &val);
	dev_info(&client->dev, "CSI2HOST: PHY State = 0x%08x\n", val);

	/* csi2host reset active */
	reg = csi2host_base + CSI2HOST_RESETN;
	val = RESETN(0);
	ret |= csi2host->i2c_reg_write(client, reg, val);

	/* number of active data lanes */
	reg = csi2host_base + CSI2HOST_N_LANES;
	val = N_LANES(csi2host->data_lanes - 1);
	ret |= csi2host->i2c_reg_write(client, reg, val);

	/* interface and phy mode select */
	reg = csi2host_base + CSI2HOST_CONTROL;
	mask = SW_DSI_CSI2_MASK | SW_CDPHY_MASK;
	val = 0;
	if (csi2host->interface == RKX11X_CSI2HOST_DSI)
		val |= SW_SEL_DSI;
	else
		val |= SW_SEL_CSI2;
	if (csi2host->phy_mode == RKX11X_CSI2HOST_CPHY)
		val |= SW_SEL_CPHY;
	else
		val |= SW_SEL_DPHY;
	ret |= csi2host->i2c_reg_update(client, reg, mask, val);

	/* csi2host reset release */
	reg = csi2host_base + CSI2HOST_RESETN;
	val = RESETN(1);
	ret |= csi2host->i2c_reg_write(client, reg, val);

	return ret;
}
