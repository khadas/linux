// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/major.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/reset.h>
#include <linux/compiler.h>
#include <linux/arm-smccc.h>

#include <linux/amlogic/media/vout/vinfo.h>
#include <linux/amlogic/media/vout/hdmi_tx21/enc_clk_config.h>
#include <linux/amlogic/media/vout/hdmi_tx21/hdmi_info_global.h>
#include <linux/amlogic/media/vout/hdmi_tx21/hdmi_tx_module.h>
#include <linux/amlogic/media/vout/hdmi_tx21/hdmi_tx_ddc.h>

#include "common.h"

static void tpi_info_send(u8 sel, u8 *data)
{
	int i;

	hdmitx21_wr_reg(TPI_INFO_FSEL_IVCTX, sel); // buf selection
	if (!data) {
		hdmitx21_wr_reg(TPI_INFO_EN_IVCTX, 0);
		return;
	}
	for (i = 0; i < 31; i++)
		hdmitx21_wr_reg(TPI_INFO_B0_IVCTX + i, data[i]);
	hdmitx21_wr_reg(TPI_INFO_EN_IVCTX, 0xe0); //TPI Info enable
}

void hdmitx_infoframe_send(u8 info_type, u8 *hb, u8 *db)
{
	int i;
	u8 sel;
	u8 data[31];
	u8 checksum;

	memset(data, 0, sizeof(data));
	switch (info_type) {
	case IF_AVI:
		sel = 0;
		break;
	case IF_VSIF:
		sel = 1;
		break;
	case IF_DRM:
		sel = 2;
		break;
	case IF_AUD:
		sel = 3;
		break;
	case IF_SPD:
		sel = 16;
		break;
	default:
		pr_info("%s[%d] wrong info_type %d\n", __func__, __LINE__, info_type);
		return;
	}

	if (!hb || !db) {
		tpi_info_send(sel, NULL);
		return;
	}

	memcpy(&data[0], hb, 3);
	// data[3] is checksum, and need to calculate in the end
	memcpy(&data[4], db, 27);
	checksum = 0;
	for (i = 0; i < 31; i++)
		checksum += data[i];
	checksum = 0 - checksum;
	data[3] = checksum;
	tpi_info_send(sel, data);
}
