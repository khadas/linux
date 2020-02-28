// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/delay.h>
#include <linux/amlogic/media/vout/hdmi_tx/hdmi_tx_module.h>
#include <linux/amlogic/media/vout/hdmi_tx/hdmi_tx_ddc.h>
#include "hw/common.h"

void scdc_config(struct hdmitx_dev *hdev)
{
	/* TMDS 1/40 & Scramble */
	scdc_wr_sink(TMDS_CFG, hdev->para->tmds_clk_div40 ? 0x3 : 0);
}
