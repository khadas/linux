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
#include <linux/spinlock.h>
#include <linux/seq_file.h>

#include <linux/amlogic/media/vout/vinfo.h>
#include <linux/amlogic/media/vout/hdmi_tx21/enc_clk_config.h>
#include <linux/amlogic/media/vout/hdmi_tx21/hdmi_info_global.h>
#include <linux/amlogic/media/vout/hdmi_tx21/hdmi_tx_module.h>
#include <linux/amlogic/media/vout/hdmi_tx21/hdmi_tx_ddc.h>

#include "common.h"
#include "../hdmi_tx.h"

static int emp_verbose;
MODULE_PARM_DESC(emp_verbose, "\n emp_verbose\n");
module_param(emp_verbose, int, 0644);

static DEFINE_SPINLOCK(tpi_lock);
static void tpi_info_send(u8 sel, u8 *data, bool no_chksum_flag)
{
	u8 checksum = 0;
	int i;
	unsigned long flags;

	spin_lock_irqsave(&tpi_lock, flags);
	hdmitx21_wr_reg(TPI_INFO_FSEL_IVCTX, sel); // buf selection
	if (!data) {
		hdmitx21_wr_reg(TPI_INFO_EN_IVCTX, 0);
		spin_unlock_irqrestore(&tpi_lock, flags);
		return;
	}

	if (!no_chksum_flag) {
		/* do checksum */
		data[3] = 0;
		for (i = 0; i < 31; i++)
			checksum += data[i];
		checksum = 0 - checksum;
		data[3] = checksum;
	}

	for (i = 0; i < 31; i++)
		hdmitx21_wr_reg(TPI_INFO_B0_IVCTX + i, data[i]);
	hdmitx21_wr_reg(TPI_INFO_EN_IVCTX, 0xe0); //TPI Info enable
	spin_unlock_irqrestore(&tpi_lock, flags);
}

/* packet no need checksum */
static void tpi_packet_send(u8 sel, u8 *data)
{
	int i;
	unsigned long flags;

	spin_lock_irqsave(&tpi_lock, flags);
	hdmitx21_wr_reg(TPI_INFO_FSEL_IVCTX, sel); // buf selection
	if (!data) {
		hdmitx21_wr_reg(TPI_INFO_EN_IVCTX, 0);
		spin_unlock_irqrestore(&tpi_lock, flags);
		return;
	}

	for (i = 0; i < 31; i++)
		hdmitx21_wr_reg(TPI_INFO_B0_IVCTX + i, data[i]);
	hdmitx21_wr_reg(TPI_INFO_EN_IVCTX, 0xe0); //TPI Info enable
	spin_unlock_irqrestore(&tpi_lock, flags);
}

static int tpi_info_get(u8 sel, u8 *data)
{
	int i;
	unsigned long flags;

	spin_lock_irqsave(&tpi_lock, flags);
	hdmitx21_wr_reg(TPI_INFO_FSEL_IVCTX, sel); // buf selection
	if (!data) {
		spin_unlock_irqrestore(&tpi_lock, flags);
		return -1;
	}
	if (hdmitx21_rd_reg(TPI_INFO_EN_IVCTX) == 0) {
		spin_unlock_irqrestore(&tpi_lock, flags);
		return 0;
	}
	for (i = 0; i < 31; i++)
		data[i] = hdmitx21_rd_reg(TPI_INFO_B0_IVCTX + i);
	spin_unlock_irqrestore(&tpi_lock, flags);
	return 31; /* fixed value */
}

void dump_infoframe_packets(struct seq_file *s)
{
	int i, j;
	u8 body[32] = {0};

	for (i = 0; i < 17; i++) {
		tpi_info_get(i, body);
		body[31] = hdmitx21_rd_reg(TPI_INFO_EN_IVCTX);
		seq_printf(s, "dump hdmi infoframe[%d]\n", i);
		for (j = 0; j < 32; j += 8)
			seq_printf(s, "%02x%02x%02x%02x%02x%02x%02x%02x\n",
				body[j + 0], body[j + 1],
				body[j + 2], body[j + 3],
				body[j + 4], body[j + 5],
				body[j + 6], body[j + 7]);
		memset(body, 0, sizeof(body));
	}
}

static int _tpi_infoframe_wrrd(u8 wr, u16 info_type, u8 *body)
{
	u8 sel;
	bool no_chksum_flag = 0;

	/* the bank index is fixed */
	switch (info_type) {
	case HDMI_INFOFRAME_TYPE_AVI:
		sel = 0;
		break;
	case HDMI_INFOFRAME_TYPE_AUDIO:
		sel = 2;
		break;
	case HDMI_INFOFRAME_TYPE_SPD:
		sel = 3;
		break;
	/* used for DV_VSIF / HDMI1.4b_VSIF */
	case HDMI_INFOFRAME_TYPE_VENDOR:
		sel = 5;
		break;
	case HDMI_INFOFRAME_TYPE_DRM:
		sel = 6;
		break;
	case HDMI_PACKET_TYPE_GCP:
		sel = 7;
		tpi_packet_send(sel, body);
		return 0;
	/* specially for HF-VSIF */
	case HDMI_INFOFRAME_TYPE_VENDOR2:
		sel = 8;
		break;
	case HDMI_INFOFRAME_TYPE_EMP:
		sel = 9;
		no_chksum_flag = 1;
		break;
	default:
		pr_info("%s[%d] wrong info_type %d\n", __func__, __LINE__, info_type);
		return -1;
	}

	if (wr) {
		if (!body) {
			tpi_info_send(sel, NULL, no_chksum_flag);
			return 0;
		}
		/* do checksum */
		tpi_info_send(sel, body, no_chksum_flag);
		return 0;
	} else {
		return tpi_info_get(sel, body);
	}
}

void hdmitx_infoframe_send(u16 info_type, u8 *body)
{
	_tpi_infoframe_wrrd(1, info_type, body);
}

int hdmitx_infoframe_rawget(u16 info_type, u8 *body)
{
	return _tpi_infoframe_wrrd(0, info_type, body);
}

